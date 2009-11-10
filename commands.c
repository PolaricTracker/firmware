/*
 * $Id: commands.c,v 1.32 2009-05-15 22:48:11 la7eca Exp $
 */
 
#include "defines.h"
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include "kernel/timer.h"
#include "kernel/stream.h"
#include "hdlc.h"
#include "fbuf.h"
#include "ax25.h"
#include "config.h"
#include "transceiver.h"
#include "radio.h"
#include "gps.h"
#include <avr/interrupt.h>
#include "adc.h"
#include "afsk.h"
#include "ui.h"
#include <avr/wdt.h>


#define MAXTOKENS 10
#define BUFSIZE   90


void setup_transceiver(void);
uint8_t tokenize(char*, char*[], uint8_t, char*, bool);

static void do_teston    (uint8_t, char**, Stream*, Stream*);
static void do_version   (uint8_t, char**, Stream*);
static void do_testpacket(uint8_t, char**, Stream*);
static void do_mycall    (uint8_t, char**, Stream*);
static void do_obj_id    (uint8_t, char**, Stream*);
static void do_dest      (uint8_t, char**, Stream*);
static void do_symbol    (uint8_t, char**, Stream*);
static void do_obj_symbol(uint8_t, char**, Stream*);
static void do_nmea      (uint8_t, char**, Stream*, Stream*);
static void do_trx       (uint8_t, char**, Stream*, Stream*);
static void do_txon      (uint8_t, char**, Stream*, Stream*);                 
static void do_tracker   (uint8_t, char**, Stream*, Stream*);
static void do_freq      (uint8_t, char**, Stream*);
static void do_fcal      (uint8_t, char**, Stream*);
static void do_power     (uint8_t, char**, Stream*);
static void do_squelch   (uint8_t, char**, Stream*);
static void do_rssi      (uint8_t, char**, Stream*, Stream*);
static void do_digipath  (uint8_t, char**, Stream*);
static void do_trace     (uint8_t, char**, Stream*);
static void do_txtone    (uint8_t, char**, Stream*, Stream*);
static void do_vbatt     (uint8_t, char**, Stream*);
static void do_listen    (uint8_t, char**, Stream*, Stream*);
static void do_converse  (uint8_t, char**, Stream*, Stream*);
static void do_btext     (uint8_t, char**, Stream*);
static void do_ps        (uint8_t, char**, Stream*);

static char buf[BUFSIZE]; 
extern fbq_t* outframes;  

/* May be moved to tracker.h ??*/
extern void tracker_on(void);
extern void tracker_off(void);
void tracker_controls_trx(bool c);


/***************************************************************************************
 * Generic getter/setter commands for simple numeric parameters. 
 *     MOVE TO config.c  ??
 *
 * Note: the ee_addr and default_val arguments are defined by macros (see config.h) 
 * and are further passed to get_param() and set_param() 
 ***************************************************************************************/
 
static void _parameter_setting_uint16(uint8_t argc, char** argv, Stream* out, 
                void* ee_addr, PGM_P default_val, uint16_t lower, uint16_t upper, PGM_P pfmt, PGM_P sfmt)
{
    int x;
    if (argc > 1) {
       if (sscanf_P(argv[1], sfmt, &x) != 1 || x<lower || x>upper) {
          sprintf_P(buf, PSTR("Sorry, parameter must be a number in range %d-%d\r\n"),lower,upper); 
          putstr(out,buf); 
       }
       else {
          set_param(ee_addr, &x, sizeof(uint16_t));
          putstr_P(out,PSTR("OK\r\n"));
       }
    } 
    else {
       get_param(ee_addr, &x, sizeof(uint16_t), default_val); 
       sprintf_P(buf, pfmt, x);
       putstr(out, buf);
    }
}


static void _parameter_setting_uint8(uint8_t argc, char** argv, Stream* out, 
                void* ee_addr, PGM_P default_val, uint8_t lower, uint8_t upper, PGM_P pfmt, PGM_P sfmt)
{
    int x;
    if (argc > 1) {
       if (sscanf_P(argv[1], sfmt, &x) != 1 || x<lower || x>upper) {
          sprintf_P(buf, PSTR("Sorry, parameter must be a number in range %d-%d\r\n"),lower,upper);  
          putstr(out,buf);
       }
       else {
          set_byte_param(ee_addr, (uint8_t) x);
          putstr_P(out, PSTR("OK\r\n"));
       }
    } 
    else {
       x = get_byte_param(ee_addr, default_val); 
       sprintf_P(buf, pfmt, x);
       putstr(out, buf);
    }
}


static void _parameter_setting_bool(uint8_t argc, char** argv, Stream* out, 
                void* ee_addr, PGM_P default_val, PGM_P name)
{
    if (argc < 2) {
       putstr_P(out, name);
       if (get_byte_param(ee_addr, default_val)) 
          putstr_P(out, PSTR(" is ON\r\n")); 
       else
          putstr_P(out, PSTR(" is OFF\r\n"));
       return; 
    }
    putstr_P(out, PSTR("***** "));
    putstr_P(out, name);
    if (strncasecmp("on", argv[1], 2) == 0) {   
       putstr_P(out, PSTR(" ON *****\r\n"));
       set_byte_param(ee_addr, 1);
    }  
    if (strncasecmp("off", argv[1], 2) == 0) {     
       putstr_P(out, PSTR(" OFF *****\r\n"));
       set_byte_param(ee_addr, 0);
    }
}




/***************************************************************************************
 * Macro to test for command to get or set numeric parameter. 
 * Usage: 
 *
 *   IF_COMMAND_PARAM_type(  command, cmpchars, 
 *                          argc, argv, outstream, 
 *                          paramname, printformat, scanformat)
 *
 *   paramname   = name of parameter (as defined in config.h)
 *   printformat = Format string (in program memory) for showing value of parameter. 
 *                 It  must contain one "%d" or equivalent.
 *   scanformat  = Format string (in program memory) for reading value of parameter. 
 *                 It must contain one "%d" or equivalent. 
 ***************************************************************************************/

#define IF_COMMAND_PARAM_uint16(command, cmpchars, argc, argv, out, x, lower, upper, pfmt, sfmt)  \
    if (strncasecmp(command, argv[0], cmpchars) == 0) \
        _parameter_setting_uint16(argc, argv, out, &PARAM_##x, (PGM_P) &PARAM_DEFAULT_##x, lower, upper, pfmt, sfmt) 

#define IF_COMMAND_PARAM_uint8(command, cmpchars, argc, argv, out, x, lower, upper, pfmt, sfmt)  \
    if (strncasecmp(command, argv[0], cmpchars) == 0) \
        _parameter_setting_uint8(argc, argv, out, &PARAM_##x, (PGM_P) &PARAM_DEFAULT_##x, lower, upper, pfmt, sfmt) 

#define IF_COMMAND_PARAM_bool(command, cmpchars, argc, argv, out, x, name)  \
    if (strncasecmp(command, argv[0], cmpchars) == 0) \
        _parameter_setting_bool(argc, argv, out, &PARAM_##x, (PGM_P) &PARAM_DEFAULT_##x, name) 




/**************************************************************************
 * Read and process commands .
 *    Not thread safe. Use in at most one thread, use lock, or allocate
 *    buf on the stack instead. 
 **************************************************************************/

bool readLine(Stream*, Stream*, char*, const uint16_t); 
 /* Move to stream.h, or move the actual code here... */
       
void cmdProcessor(Stream *in, Stream *out)
{
    char* argv[MAXTOKENS];
    uint8_t argc;
    sleep (10);
    putstr_P(out, PSTR("\r\n\r\n*************************************************************"));
    putstr_P(out, PSTR("\n\r Velkommen til 'Polaric Tracker' firmware "));
    putstr_P(out, PSTR(VERSION_STRING));
    putstr_P(out, PSTR("\r\n Utviklet av LA3T, Troms�gruppen av NRRL"));
    putstr_P(out, PSTR("\r\n*************************************************************\r\n\r\n"));
    
    while (1) {
         memset(argv, 0, MAXTOKENS);
         putstr(out, "cmd: ");    
         readLine(in, out, buf, BUFSIZE);
         
         /* Split input line into argument tokens */
         argc = tokenize(buf, argv, MAXTOKENS, " \t\r\n,", true);

         /* Select command handler: 
          * misc commands 
          */         
         if (strncasecmp("teston", argv[0], 6) == 0)
             do_teston(argc, argv, out, in);
         else if (strncasecmp("version", argv[0], 3) == 0)
             do_version(argc, argv, out);
         else if (strncasecmp("txtone", argv[0], 7) == 0)
             do_txtone(argc, argv, out, in);
         else if (strncasecmp("testpacket",  argv[0], 5) == 0)
             do_testpacket(argc, argv, out);                               
         else if (strncasecmp("gps",     argv[0], 4) == 0)
             do_nmea(argc, argv, out, in);     
         else if (strncasecmp("trx",     argv[0], 3) == 0)
             do_trx(argc, argv, out, in);        
         else if (strncasecmp("tracker", argv[0], 6) == 0)
             do_tracker(argc, argv, out, in);
         else if (strncasecmp("txon",     argv[0], 4) == 0)
             do_txon(argc, argv, out, in);             
         else if (strncasecmp("rssi", argv[0], 2) == 0)
             do_rssi(argc, argv, out, in);       
         else if (strncasecmp("trace", argv[0], 5) == 0)
             do_trace(argc, argv, out); 
         else if (strncasecmp("ps", argv[0], 2) == 0)
             do_ps(argc, argv, out);    
         else if (strncasecmp("vbatt", argv[0], 2) == 0)
             do_vbatt(argc, argv, out);
         else if (strncasecmp("listen", argv[0], 3) == 0)
             do_listen(argc, argv, out, in);            
         else if (strncasecmp("k", argv[0], 1) == 0 || strncasecmp("converse", argv[0], 4) == 0)
             do_converse(argc, argv, out, in);   
				 else if (strcasecmp("boot", argv[0]) == 0) {
					 /* Tell the bootloader invoke firmware upgrade */
					 /* This command does only work with the customized bootloader */
					 eeprom_write_byte ((void*)E2END, 0xff); 
					 soft_reset ();
				 }
				 
         /* Commands for setting/viewing parameters */
         else if (strncasecmp("mycall", argv[0], 2) == 0)
             do_mycall(argc, argv, out);    
         else if (strncasecmp("oident", argv[0], 3) == 0)
             do_obj_id(argc, argv, out);           
         else if (strncasecmp("dest", argv[0], 3) == 0)
             do_dest(argc, argv, out);  
         else if (strncasecmp("digipath", argv[0], 4) == 0)  
             do_digipath(argc, argv, out);
         else if (strncasecmp("symbol", argv[0], 3) == 0)
             do_symbol(argc, argv, out);  
         else if (strncasecmp("osymbol", argv[0], 4) == 0)
             do_obj_symbol(argc, argv, out);                       
         else if (strncasecmp("freq",argv[0], 2) == 0)
             do_freq(argc, argv, out);  
         else if (strncasecmp("fcal",argv[0], 3) == 0)
             do_fcal(argc, argv, out);
         else if (strncasecmp("txpower", argv[0], 4) == 0)
             do_power(argc, argv, out);    
         else if (strncasecmp("squelch", argv[0], 2) == 0)
             do_squelch(argc, argv, out); 
         else if (strncasecmp("btext", argv[0], 2) == 0)
             do_btext(argc, argv, out); 
         else IF_COMMAND_PARAM_uint8
                  ( "txdelay", 3, argc, argv, out,
                    TXDELAY, 0, 200, PSTR("TXDELAY (in 1 byte units) is %d\r\n\0"), PSTR(" %d") );      
         
         else IF_COMMAND_PARAM_uint8
                  ( "txtail", 3, argc, argv, out,
                    TXTAIL, 0, 200, PSTR("TXTAIL (in 1 byte units) is %d\r\n\0"), PSTR(" %d") );
                    
         else IF_COMMAND_PARAM_uint8
                  ( "maxframe", 5, argc, argv, out,
                    MAXFRAME, 1, 7, PSTR("MAXFRAME is %d\r\n\0"), PSTR(" %d") );
         
         else IF_COMMAND_PARAM_uint16
                 ("afc", 3, argc, argv, out, 
                    TRX_AFC, 0, 12000, PSTR("AFC range is %d Hz\r\n\0"), PSTR(" %d") );
                   
         else IF_COMMAND_PARAM_uint8
                 ( "tracktime", 6, argc, argv, out, 
                   TRACKER_SLEEP_TIME, GPS_FIX_TIME+1, 240, PSTR("Tracker sleep time is %d seconds\r\n\0"), PSTR(" %d") );  
                      
         else IF_COMMAND_PARAM_uint16
                 ( "deviation", 3, argc, argv, out,
                   TRX_AFSK_DEV, 0, 5000, PSTR("AFSK Deviation is %d Hz\r\n\0"), PSTR(" %d") );
                   
         else IF_COMMAND_PARAM_uint16 
                 ( "gpsbaud", 4, argc, argv, out, 
                    GPS_BAUD, 1200, 19200, PSTR("GPS baud rate is %d\r\n\0"), PSTR(" %d") );        
         
         else IF_COMMAND_PARAM_uint16 
                 ( "maxturn", 6, argc, argv, out, 
                    TRACKER_TURN_LIMIT, 0, 360, PSTR("Tracker turn limit is %d degrees\r\n\0"), PSTR(" %d") );    
                        
         else IF_COMMAND_PARAM_uint8 
                 ( "maxpause", 6, argc, argv, out, 
                    TRACKER_MAXPAUSE, 1, 200, PSTR("Tracker pause limit is %d units (see tracktime)\r\n\0"), PSTR(" %d") );                                 
       
         else IF_COMMAND_PARAM_uint8 
                 ( "minpause", 6, argc, argv, out, 
                    TRACKER_MINPAUSE, 1, 200, PSTR("Tracker minimum pause (straight forward movement) is %d units (see tracktime)\r\n\0"), PSTR(" %d") );                                 
    
         else IF_COMMAND_PARAM_uint8 
                 ( "mindist", 6, argc, argv, out, 
                    TRACKER_MINDIST, 1, 200, PSTR("Tracker distance (at low speed) is %d meters\r\n\0"), PSTR(" %d") );                                 
         
         else IF_COMMAND_PARAM_uint8 
                 ( "statustime", 7, argc, argv, out, 
                    STATUS_TIME, 1, 200, PSTR("Status time is %d units (see tracktime)\r\n\0"), PSTR(" %d") );
         
         else IF_COMMAND_PARAM_bool 
                 ( "altitude", 3, argc, argv, out, ALTITUDE_ON, PSTR("ALTITUDE") );
         
         else IF_COMMAND_PARAM_bool           
                 ( "timestamp", 4, argc, argv, out, TIMESTAMP_ON, PSTR("TIMESTAMP") );
                 
         else IF_COMMAND_PARAM_bool
                 ( "compress", 4, argc, argv, out, COMPRESS_ON, PSTR("COMPRESS") );
                 
         else IF_COMMAND_PARAM_bool
                 ( "powersave", 6, argc, argv, out, GPS_POWERSAVE, PSTR("POWERSAVE") );  
                          
         else IF_COMMAND_PARAM_bool
                 ( "beep", 2, argc, argv, out, REPORT_BEEP, PSTR("BEEP") );        
                          
         else IF_COMMAND_PARAM_bool
                 ( "txmon", 3, argc, argv, out, TXMON_ON, PSTR("TX MONITOR") );   
                          
         else IF_COMMAND_PARAM_bool
                 ( "autopower", 3, argc, argv, out, AUTOPOWER, PSTR("AUTO POWER") );   
                                           
         else if (strlen(argv[0]) > 0)
             putstr_P(out, PSTR("*** Unknown command\r\n"));
         else 
             continue; 
         putstr(out,"\r\n");         
   }
}   


/************************************************
 * Report firmware version
 ************************************************/
 
static void do_version(uint8_t argc, char** argv, Stream* out)
{
   putstr_P(out,PSTR(VERSION_STRING));
   putstr_P(out,PSTR("\r\n"));
}



/************************************************
 * Report RSSI level 
 ************************************************/

static void do_rssi(uint8_t argc, char** argv, Stream* out, Stream* in)
{
    int i;    
    radio_require();
    stream_get_nb(in);
    for (;;)
    {
        sleep(25);
        double x = adf7021_read_rssi();
        sprintf_P(buf, PSTR("RSSI: %4.0f :\0"), x);
        putstr(out, buf);
        for (i=0; i < (125+x)/2; i++)
           putch(out,'*');
        putstr_P(out, PSTR("\r\n"));
        if (stream_get_nb(in) > 10)
           break;
    }
    radio_release();
}



/************************************************
 * Report battery voltage
 ************************************************/
 
static void do_vbatt(uint8_t argc, char** argv, Stream* out)
{
   sprintf_P(buf, PSTR("Battery voltage: %.2f V\r\n\0"), 
             batt_voltage());
   putstr(out, buf);
}



/************************************************
 * Decode and show incoming packets
 ************************************************/
 
static void do_listen(uint8_t argc, char** argv, Stream* out, Stream* in)
{
   putstr_P(out, PSTR("***** LISTEN ON RECEIVER *****\r\n"));
   radio_require();
   afsk_enable_decoder();
   mon_activate(true);
   getch(in);
   mon_activate(false);
   afsk_disable_decoder();
   radio_release();
}



/************************************************
 * Converse mode
 ************************************************/
 
static void do_converse(uint8_t argc, char** argv, Stream* out, Stream* in)
{
   putstr_P(out, PSTR("***** CONVERSE MODE *****\r\n"));
   radio_require();
   afsk_enable_decoder();
   mon_activate(true);
   while ( readLine(in, out, buf, BUFSIZE)) {
        FBUF packet;    
        addr_t from, to; 
        GET_PARAM(MYCALL, &from);
        GET_PARAM(DEST, &to);       
        addr_t digis[7];
        uint8_t ndigis = GET_BYTE_PARAM(NDIGIS); 
        GET_PARAM(DIGIS, &digis);   
        ax25_encode_header(&packet, &from, &to, digis, ndigis, FTYPE_UI, PID_NO_L3);
        fbuf_putstr(&packet, buf);                        
        fbq_put(outframes, packet);
   }
   mon_activate(false);
   afsk_disable_decoder();
   radio_release();
}




/************************************************
 * For testing of GPS .....
 ************************************************/
 
Semaphore nmea_run; 

static void do_nmea(uint8_t argc, char** argv, Stream* out, Stream* in)
{                                                                                                            
  if (argc < 2)
      putstr_P(out, PSTR("Usage: GPS on|off|nmea|pos\r\n"));
      
  if (strncasecmp("on", argv[1], 2) == 0) {
      putstr_P(out, PSTR("***** GPS ON *****\r\n"));
      gps_on();
      return;
  } 
  if (strncasecmp("off", argv[1], 2) == 0) {
      putstr_P(out, PSTR("***** GPS OFF *****\r\n"));
      gps_off();
      return;
  }  
  if (strncasecmp("nmea", argv[1], 1) == 0) {
      putstr_P(out, PSTR("***** NMEA PACKETS *****\r\n"));
      gps_mon_raw();
  } 
  else if (strncasecmp("pos", argv[1], 3) == 0){
      putstr_P(out, PSTR("***** VALID POSITION REPORTS (GPRMC) *****\r\n"));
      gps_mon_pos();
  }     
  else
     return;

  /* And wait until some character has been typed */
  getch(in);
  gps_mon_off();
}




/************************************************
 * Turn on/off transceiver chip.....
 ************************************************/
 
static void do_trx(uint8_t argc, char** argv, Stream* out, Stream* in)
{
    putstr_P(out, PSTR("***** TRX COMMAND IS NO LONGER NEEDED *****\r\n"));
}



/************************************************
 * For testing of tracker .....
 ************************************************/

static void do_tracker(uint8_t argc, char** argv, Stream* out, Stream* in)
{
  if (argc < 2)
  {
      if (GET_BYTE_PARAM(TRACKER_ON))
          putstr_P(out, PSTR("Tracker is ON\r\n"));
      else
          putstr_P(out, PSTR("Tracker is OFF\r\n"));
      return;
  }
  if (strncasecmp("on", argv[1], 2) == 0) {   
      putstr_P(out, PSTR("***** TRACKER ON *****\r\n"));
      tracker_on();
  }  
  if (strncasecmp("off", argv[1], 2) == 0) {     
      putstr_P(out, PSTR("***** TRACKER OFF *****\r\n"));
      tracker_off();
  }
}



/************************************************
 * For testing of transmitter .....
 ************************************************/

static void do_txon(uint8_t argc, char** argv, Stream* out, Stream* in)
{
   putstr_P(out, PSTR("***** TX ON *****\r\n"));
   radio_require();
   adf7021_enable_tx();

   getch(in);
   adf7021_disable_tx();
   radio_release();
}


      
/*********************************************
 * teston <byte> : Generate test signal
 *********************************************/
 
static void do_teston(uint8_t argc, char** argv, Stream* out, Stream* in)
{
    int ch = 0;
    if (argc < 2) {
        putstr_P(out, PSTR("Usage: TESTON <byte>\r\n"));
        return;
    }
    sscanf(argv[1], " %i", &ch);
    radio_require();  
    hdlc_test_on((uint8_t) ch);
    sprintf_P(buf, PSTR("**** TEST SIGNAL: 0x%X ****\r\n"), ch);
    putstr(out, buf );  
    
     /* And wait until some character has been typed */
    getch(in);
    hdlc_test_off();
    radio_release();
}


static void do_txtone(uint8_t argc, char** argv, Stream* out, Stream* in)
{
  if (argc < 2) {
      putstr_P(out, PSTR("Usage: TXTONE high|low\r\n"));
      return;
  }
 
  radio_require();
  if (strncasecmp("hi", argv[1], 2) == 0) {
      putstr_P(out, PSTR("***** TEST TONE HIGH *****\r\n"));
      afsk_high_tone(true);
      hdlc_test_on(0xff);
  }
  else if (strncasecmp("low", argv[1], 2) == 0) {
      putstr_P(out, PSTR("***** TEST TONE LOW *****\r\n"));
      afsk_high_tone(false);
      hdlc_test_on(0xff);
  } 
  
 /* And wait until some character has been typed */
  getch(in);
  hdlc_test_off();
  radio_release();
}


         
/*********************************************
 * tx : Send AX25 test packet
 *********************************************/

static void do_testpacket(uint8_t argc, char** argv, Stream* out)
{ 
    FBUF packet;    
    addr_t from, to; 
    radio_require();
    GET_PARAM(MYCALL, &from);
    GET_PARAM(DEST, &to);       
    addr_t digis[7];
    uint8_t ndigis = GET_BYTE_PARAM(NDIGIS); 
    GET_PARAM(DIGIS, &digis);   
    ax25_encode_header(&packet, &from, &to, digis, ndigis, FTYPE_UI, PID_NO_L3);
    fbuf_putstr_P(&packet, PSTR("The lazy brown dog jumps over the quick fox 1234567890"));                      
    putstr_P(out, PSTR("Sending (AX25 UI) test packet....\r\n"));       
    fbq_put(outframes, packet);
    radio_release();
}



/************************************************
 * Set identifier of object
 ************************************************/
 
static void do_obj_id(uint8_t argc, char** argv, Stream* out)
{
    if (argc > 1){
        if (strlen(argv[1]) <= 9)
        {
           SET_PARAM(OBJ_ID, argv[1]);
           putstr_P(out,PSTR("OK\r\n"));
        }
        else
           putstr_P(out,PSTR("Id cannot be longer than 9 characters\r\n"));
    }
    else {
        char x[9];
        GET_PARAM(OBJ_ID, x);
        sprintf_P(buf, PSTR("OBJECT IDENT is: \"%s\"\r\n\0"), x);
        putstr(out, buf);
    }
}
         
         
/*********************************************
 * config: mycall (sender address)
 *********************************************/
 
static void do_mycall(uint8_t argc, char** argv, Stream* out)
{
   addr_t x;
   char cbuf[11]; 
   if (argc > 1) {
      str2addr(&x, argv[1]);
      SET_PARAM(MYCALL, &x);
      putstr_P(out,PSTR("OK\r\n"));
   }
   else {
      GET_PARAM(MYCALL, &x);
      sprintf_P(buf, PSTR("MYCALL is: %s\r\n\0"), addr2str(cbuf, &x));
      putstr(out, buf);
   }   
}

         
/*********************************************
 * config: dest (destination address)
 *********************************************/
 
static void do_dest(uint8_t argc, char** argv, Stream* out)
{
   addr_t x;
   char cbuf[11]; 
   if (argc > 1) {
      str2addr(&x, argv[1]);
      SET_PARAM(DEST, &x);
      putstr_P(out,PSTR("OK\r\n"));
   }
   else {
      GET_PARAM(DEST, &x);
      sprintf_P(buf, PSTR("DEST is: %s\r\n\0"), addr2str(cbuf, &x));
      putstr(out, buf);
   }   
}



/*********************************************
 * config: digipeater path
 *********************************************/
 
static void do_digipath(uint8_t argc, char** argv, Stream* out)
{
    __digilist_t digis;
    uint8_t ndigis;
    uint8_t i;
    char cbuf[11]; 
    
    if (argc > 1) {
       if (argc==2 && strncasecmp("off", argv[1], 3)==0)
           ndigis = 0;
       else{
         ndigis = argc - 1;
         if (ndigis > 7) 
             ndigis = 7;
         for (i=0; i<ndigis; i++)
             str2addr(&digis[i], argv[i+1]);
         SET_PARAM(DIGIS, digis);     
       }
       SET_BYTE_PARAM(NDIGIS, ndigis);
       putstr_P(out,PSTR("OK\r\n")); 
    }
    else  {
       ndigis = GET_BYTE_PARAM(NDIGIS);
       GET_PARAM(DIGIS, &digis);
       putstr_P(out, PSTR("Digipeater path is ")); 
       if (ndigis==0)
           putstr_P(out, PSTR(" <EMPTY>\r\n"));
       for (i=0; i<ndigis; i++)
       {   
           putstr(out, addr2str(cbuf, &digis[i]));           
           if (i < ndigis-1)
               putstr_P(out, PSTR(", ")); 
       }
       putstr_P(out,PSTR("\n\r"));
    }
}


/***********************************************
 * config: Beacon text (comment in pos reports)
 ***********************************************/
 
static void do_btext(uint8_t argc, char** argv, Stream* out)
{
    if (argc > 1){
        SET_PARAM(REPORT_COMMENT, argv[1]);
        putstr_P(out,PSTR("OK\r\n"));
    }
    else {
        char x[COMMENT_LENGTH];
        GET_PARAM(REPORT_COMMENT, x);
        sprintf_P(buf, PSTR("BTEXT is: \"%s\"\r\n\0"), x);
        putstr(out, buf);
    }
}


/************************************************
 * Show trace of current and previous run
 * (for debugging)
 ************************************************/
 
static void do_trace(uint8_t argc, char** argv, Stream* out)
{
    show_trace(buf, 0, PSTR("Current run  = "), PSTR("\r\n"));
    putstr(out,buf);
    show_trace(buf, 1, PSTR("Previous run = "), PSTR("\r\n"));
    putstr(out, buf);
}



/************************************************
 * Show info about tasks (ps command)
 ************************************************/
 
static void do_ps(uint8_t argc, char** argv, Stream* out)
{
   uint8_t running = t_nRunning();
   sprintf_P(buf, PSTR("Tasks running        : %d\r\n\0"), running);
   putstr(out, buf);   
   sprintf_P(buf, PSTR("Tasks sleeping       : %d\r\n\0"), t_nTasks()- t_nTerminated() - running);
   putstr(out, buf);
   if (t_nTerminated()) {
      sprintf_P(buf, PSTR("Tasks terminated     : %d\r\n\0"), t_nTerminated());
      putstr(out, buf);
   }
   sprintf_P(buf, PSTR("Stack space allocated: %d bytes\r\n\0"), t_stackUsed());
   putstr(out, buf);
}



/*********************************************
 * config: symbol (APRS symbol/symbol table)
 *********************************************/
 
static void do_symbol(uint8_t argc, char** argv, Stream* out)
{
   if (argc > 2) {
      SET_BYTE_PARAM(SYMBOL_TABLE, *argv[1]);
      SET_BYTE_PARAM(SYMBOL, *argv[2]);
      putstr_P(out,PSTR("OK\r\n"));
   }
   else {
      sprintf_P(buf, PSTR("SYMTABLE/SYMBOL is: %c %c\r\n\0"), 
           GET_BYTE_PARAM(SYMBOL_TABLE), GET_BYTE_PARAM(SYMBOL));
      putstr(out, buf);
   }   
}



/****************************************************
 * config: object symbol (APRS symbol/symbol table)
 ****************************************************/

static void do_obj_symbol(uint8_t argc, char** argv, Stream* out)
{
   if (argc > 2) {
      SET_BYTE_PARAM(OBJ_SYMBOL_TABLE, *argv[1]);
      SET_BYTE_PARAM(OBJ_SYMBOL, *argv[2]);
      putstr_P(out,PSTR("OK\r\n"));
   }
   else {
      sprintf_P(buf, PSTR("OBJECT SYMTABLE/SYMBOL is: %c %c\r\n\0"), 
           GET_BYTE_PARAM(OBJ_SYMBOL_TABLE), GET_BYTE_PARAM(OBJ_SYMBOL));
      putstr(out, buf);
   }   
}



/*********************************************
 * config: transceiver frequency
 *********************************************/
 
static void do_freq(uint8_t argc, char** argv, Stream* out)
{
    uint32_t x = 0;
    if (argc > 1) {
       sscanf(argv[1], " %ld", &x);
       x *= 1000;   
       SET_PARAM(TRX_FREQ, &x);
       putstr_P(out,PSTR("OK\r\n"));
    } 
    else {
       GET_PARAM(TRX_FREQ, &x);
       sprintf_P(buf, PSTR("FREQ is: %ld kHz\r\n\0"), x/1000);
       putstr(out, buf);
    }
}



/*********************************************
 * config: transceiver frequency calibration
 *********************************************/
 
static void do_fcal(uint8_t argc, char** argv, Stream* out)
{
    int16_t x = 0;
    if (argc > 1) {
       sscanf(argv[1], " %d", &x);
       SET_PARAM(TRX_CALIBRATE, &x);
       putstr_P(out,PSTR("OK\r\n"));
    } 
    else {
       GET_PARAM(TRX_CALIBRATE, &x);
       sprintf_P(buf, PSTR("FREQ CALIBRATION is: %d Hz\r\n\0"), x);
       putstr(out, buf);
    }
}


/*********************************************
 * config: transmitter power (dBm)
 *********************************************/
 
static void do_power(uint8_t argc, char** argv, Stream* out)
{
    float x = 0;
    if (argc > 1) {
       sscanf(argv[1], " %f", &x);  
       SET_PARAM(TRX_TXPOWER, &x);
       putstr_P(out,PSTR("OK\r\n"));
    } 
    else {
       GET_PARAM(TRX_TXPOWER, &x);
       sprintf_P(buf, PSTR("POWER is: %f dBm\r\n\0"), x);
       putstr(out, buf);
    }
}


/*********************************************
 * config: squelch level (dBm)
 *********************************************/
 
static void do_squelch(uint8_t argc, char** argv, Stream* out)
{
    float x = 0;
    if (argc > 1) {
       sscanf(argv[1], " %f", &x);  
       SET_PARAM(TRX_SQUELCH, &x);
       putstr_P(out,PSTR("OK\r\n"));
    } 
    else {
       GET_PARAM(TRX_SQUELCH, &x);
       sprintf_P(buf, PSTR("SQUELCH level is: %f dBm\r\n\0"), x);
       putstr(out, buf);
    }
}


/****************************************************************************
 * split input string into tokens - returns number of tokens found
 *
 * ARGUMENTS: 
 *   buf       - text buffer to tokenize
 *   tokens    - array in which to store pointers to tokens
 *   maxtokens - maximum number of tokens to scan for
 *   delim     - characters which can be used as delimiters between tokens
 *   merge     - if true, merge empty tokens
 *
 * Text which is enclosed by " " is regarded as a single token. 
 ****************************************************************************/
 
uint8_t tokenize(char* buf, char* tokens[], uint8_t maxtokens, char *delim, bool merge)
{ 
     register uint8_t ntokens = 0;
     while (ntokens<maxtokens)
     {
        if (*buf == '\"') {
            /* Special case: token is enclosed in " */
            tokens[ntokens++] = ++buf;
            char *endt = strchrnul(buf, '\"'); 
            *endt = '\0';
            buf = endt + 1;
        }
        else {    
            tokens[ntokens] = strsep(&buf, delim);
            if ( buf == NULL)
               break;
            if (!merge || *tokens[ntokens] != '\0') 
               ntokens++;
        }
     }
     return (merge && *tokens[ntokens] == '\0' ? ntokens : ntokens+1);
}
