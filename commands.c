/*
 * $Id: commands.c,v 1.18 2008-09-08 22:35:01 la7eca Exp $
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
#include "gps.h"


#define MAXTOKENS 10
#define BUFSIZE   60


void setup_transceiver(void);
uint8_t tokenize(char*, char*[], uint8_t, char*, bool);

static void do_teston    (uint8_t, char**, Stream*);
static void do_testoff   (uint8_t, char**, Stream*);
static void do_testpacket(uint8_t, char**, Stream*);
static void do_mycall    (uint8_t, char**, Stream*);
static void do_dest      (uint8_t, char**, Stream*);
static void do_symbol    (uint8_t, char**, Stream*);
static void do_nmea      (uint8_t, char**, Stream*, Stream* in);
static void do_trx       (uint8_t, char**, Stream*, Stream* in);
static void do_txon      (uint8_t, char**, Stream*);            
static void do_txoff     (uint8_t, char**, Stream*);            
static void do_tracker   (uint8_t, char**, Stream*, Stream* in);
static void do_freq      (uint8_t, char**, Stream*);
static void do_power     (uint8_t, char**, Stream*);
static void do_squelch   (uint8_t, char**, Stream*);
static void do_rssi      (uint8_t, char**, Stream*, Stream*);
static void do_digipath  (uint8_t, char**, Stream*);


static char buf[BUFSIZE]; 
extern fbq_t* outframes;  

extern void tracker_on(void);
extern void tracker_off(void);


/***************************************************************************************
 * Generic getter/setter commands for simple numeric parameters. 
 *     MOVE TO config.c
 *
 * Note: the ee_addr and default_val arguments are defined by macros (see config.h) 
 * and are further passed to get_param() and set_param() 
 ***************************************************************************************/
 
static void _parameter_setting_uint16(uint8_t argc, char** argv, Stream* out, 
                void* ee_addr, PGM_P default_val, uint16_t lower, uint16_t upper, PGM_P pfmt, PGM_P sfmt)
{
    uint16_t x;
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
    uint8_t x;
    if (argc > 1) {
       if (sscanf_P(argv[1], sfmt, &x) != 1 || x<lower || x>upper) {
          sprintf_P(buf, PSTR("Sorry, parameter must be a number in range %d-%d\r\n"),lower,upper);  
          putstr(out,buf);
       }
       else {
          set_byte_param(ee_addr, x);
          putstr_P(out,PSTR("OK\r\n"));
       }
    } 
    else {
       x = get_byte_param(ee_addr, default_val); 
       sprintf_P(buf, pfmt, x);
       putstr(out, buf);
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
    if (strncmp(command, argv[0], cmpchars) == 0) \
        _parameter_setting_uint16(argc, argv, out, &PARAM_##x, &PARAM_DEFAULT_##x, lower, upper, pfmt, sfmt) 

#define IF_COMMAND_PARAM_uint8(command, cmpchars, argc, argv, out, x, lower, upper, pfmt, sfmt)  \
    if (strncmp(command, argv[0], cmpchars) == 0) \
        _parameter_setting_uint8(argc, argv, out, &PARAM_##x, &PARAM_DEFAULT_##x, lower, upper, pfmt, sfmt) 




/**************************************************************************
 * Read and process commands .
 *    Not thread safe. Use in at most one thread, use lock, or allocate
 *    buf on the stack instead. 
 **************************************************************************/

void readLine(Stream*, Stream*, char*, const uint16_t); // Move to stream.h
       
void cmdProcessor(Stream *in, Stream *out)
{
    char* argv[MAXTOKENS];
    uint8_t argc;
    
    putstr_P(out, PSTR("\n\rVelkommen til LA3T 'Polaric Tracker' firmware\r\n\r\n"));
    sprintf_P(buf, PSTR("trace = %u\r\n"), GET_TRACE);
    putstr(out,buf);
    while (1) {
         putstr(out, "cmd: ");    
         readLine(in, out, buf, BUFSIZE);
         
         /* Split input line into argument tokens */
         argc = tokenize(buf, argv, MAXTOKENS, " \t\r\n,", true);

         /* Select command handler: 
          * misc commands 
          */         
         if (strncmp("teston", argv[0], 6) == 0)
             do_teston(argc, argv, out);
         else if (strncmp("testoff", argv[0], 7) == 0)
             do_testoff(argc, argv, out);
         else if (strncmp("testpacket",  argv[0], 5) == 0)
             do_testpacket(argc, argv, out);                               
         else if (strncmp("gps",     argv[0], 4) == 0)
             do_nmea(argc, argv, out, in);     
         else if (strncmp("trx",     argv[0], 3) == 0)
             do_trx(argc, argv, out, in);        
         else if (strncmp("tracker", argv[0], 6) == 0)
             do_tracker(argc, argv, out, in);
         else if (strncmp("txon",     argv[0], 4) == 0)
             do_txon(argc, argv, out);        
         else if (strncmp("txoff",     argv[0], 4) == 0)
             do_txoff(argc, argv, out);     
         else if (strncmp("rssi", argv[0], 2) == 0)
             do_rssi(argc, argv, out, in);        
     
         
         /* Commands for setting/viewing parameters */
         else if (strncmp("mycall", argv[0], 2) == 0)
             do_mycall(argc, argv, out);    
         else if (strncmp("dest", argv[0], 3) == 0)
             do_dest(argc, argv, out);  
         else if (strncmp("digipath", argv[0], 4) == 0)  
             do_digipath(argc, argv, out);
         else if (strncmp("symbol", argv[0], 3) == 0)
             do_symbol(argc, argv, out);               
         else if (strncmp("freq",argv[0], 2) == 0)
             do_freq(argc, argv, out);  
         else if (strncmp("power", argv[0], 2) == 0)
             do_power(argc, argv, out);    
         else if (strncmp("squelch", argv[0], 2) == 0)
             do_squelch(argc, argv, out); 
         
         else IF_COMMAND_PARAM_uint8
                  ( "txdelay", 3, argc, argv, out,
                    TXDELAY, 0, 200, PSTR("TXDELAY (in 1 byte units) is %d\r\n\0"), PSTR(" %d") );      
         
         else IF_COMMAND_PARAM_uint8
                  ( "txtail", 3, argc, argv, out,
                    TXDELAY, 0, 200, PSTR("TXTAIL (in 1 byte units) is %d\r\n\0"), PSTR(" %d") );
         
         else IF_COMMAND_PARAM_uint16
                 ( "tracktime", 6, argc, argv, out, 
                   TRACKER_SLEEP_TIME, 10, 3600, PSTR("Tracker sleep time is %d seconds\r\n\0"), PSTR(" %d") );  
                      
         else IF_COMMAND_PARAM_uint16
                 ( "deviation", 3, argc, argv, out,
                   TRX_AFSK_DEV, 100, 5000, PSTR("AFSK Deviation is %d Hz\r\n\0"), PSTR(" %d") );
                   
         else IF_COMMAND_PARAM_uint16 
                 ( "gpsbaud", 4, argc, argv, out, 
                    GPS_BAUD, 1200, 19200, PSTR("GPS baud rate is %d\r\n\0"), PSTR(" %d") );        
         
         else IF_COMMAND_PARAM_uint16 
                 ( "maxturn", 6, argc, argv, out, 
                    TRACKER_TURN_LIMIT, 0, 360, PSTR("Tracker turn limit is %d degrees\r\n\0"), PSTR(" %d") );    
                        
         else IF_COMMAND_PARAM_uint8 
                 ( "maxpause", 6, argc, argv, out, 
                    TRACKER_PAUSE_LIMIT, 1, 200, PSTR("Tracker pause limit is %d units (see tracktime)\r\n\0"), PSTR(" %d") );                                 
                    
         else if (strlen(argv[0]) > 0)
             putstr_P(out, PSTR("*** Unknown command\r\n"));
         else 
             continue; 
         putstr(out,"\r\n");         
   }
}   



static void do_rssi(uint8_t argc, char** argv, Stream* out, Stream* in)
{
    int i;    
    for (i=0; i<60; i++)
    {
        double x = adf7021_read_rssi();
        sprintf_P(buf, PSTR("RSSI level: %f\r\n\0"), x);
        putstr(out, buf);
        sleep(100);
    }
}



/************************************************
 * For testing of GPS .....
 ************************************************/
 
Semaphore nmea_run; 

static void do_nmea(uint8_t argc, char** argv, Stream* out, Stream* in)
{                                                                                                            
  if (argc < 2)
      putstr_P(out, PSTR("Usage: GPS on|off|nmea|pos\r\n"));
      
  if (strncmp("on", argv[1], 2) == 0) {
      putstr_P(out, PSTR("***** GPS ON *****\r\n"));
      gps_on();
      return;
  } 
  if (strncmp("off", argv[1], 2) == 0) {
      putstr_P(out, PSTR("***** GPS OFF *****\r\n"));
      gps_off();
      return;
  }  
  if (strncmp("nmea", argv[1], 1) == 0) {
      putstr_P(out, PSTR("***** NMEA PACKETS *****\r\n"));
      gps_mon_raw();
  } 
  else if (strncmp("pos", argv[1], 3) == 0){
      putstr_P(out, PSTR("***** VALID POSITION REPORTS (GPRMC) *****\r\n"));
      gps_mon_pos();
  } 
  else if (strncmp("fix", argv[1], 3) == 0)
     { notify_lock(true); return; }
      
  else if (strncmp("unfix", argv[1], 5) == 0)
     { notify_lock(false); return; }     
  else
     return;

  /* And wait until some character has been typed */
  getch(in);
  getch(in);
  gps_mon_off();
}




/************************************************
 * Turn on/off transceiver chip.....
 ************************************************/
 
static void do_trx(uint8_t argc, char** argv, Stream* out, Stream* in)
{
   if (strncmp("on", argv[1], 2) == 0) {
      putstr_P(out, PSTR("***** TRX CHIP ON *****\r\n"));
      setup_transceiver();
      adf7021_power_on ();
   }
   if (strncmp("off", argv[1], 2) == 0) {
      putstr_P(out, PSTR("***** TRX CHIP OFF *****\r\n"));
      adf7021_power_off ();
   }
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
  if (strncmp("on", argv[1], 2) == 0) {   
      putstr_P(out, PSTR("***** TRACKER ON *****\r\n"));
      tracker_on();
  }  
  if (strncmp("off", argv[1], 2) == 0) {     
      putstr_P(out, PSTR("***** TRACKER OFF *****\r\n"));
      tracker_off();
  }
}



/************************************************
 * For testing of transmitter .....
 ************************************************/

static void do_txon(uint8_t argc, char** argv, Stream* out)
{
   putstr_P(out, PSTR("***** TX ON *****\r\n"));
   adf7021_enable_tx();
}

static void do_txoff(uint8_t argc, char** argv, Stream* out)
{
   putstr_P(out, PSTR("***** TX OFF *****\r\n"));
   adf7021_disable_tx();
}


      
/*********************************************
 * teston <byte> : Turn on test signal
 *********************************************/
 
static void do_teston(uint8_t argc, char** argv, Stream* out)
{
    int ch = 0;
    putstr(out, "**** TEST SIGNAL ON ****\r\n");
    hdlc_test_off();
    sleep(10);
    sscanf(argv[1], " %i", &ch);  
    hdlc_test_on((uint8_t) ch);
    sprintf_P(buf, PSTR("Test signal on: 0x%X\r\n\0"), ch);
    putstr(out, buf );  
}


         

/*********************************************
 * testoff : Turn off test signal
 *********************************************/
 
static void do_testoff(uint8_t argc, char** argv, Stream* out)
{
    putstr_P(out, PSTR("**** TEST SIGNAL OFF ****\r\n"));  
    hdlc_test_off();  
}


         
/*********************************************
 * tx : Send AX25 test packet
 *********************************************/

static void do_testpacket(uint8_t argc, char** argv, Stream* out)
{
    FBUF packet;    
    addr_t from, to; 
    GET_PARAM(MYCALL, &from);
    GET_PARAM(DEST, &to);
    
    addr_t digis[7];
    uint8_t ndigis = GET_BYTE_PARAM(NDIGIS); 
    GET_PARAM(DIGIS, &digis);
            
    ax25_encode_header(&packet, &from, &to, digis, 1, FTYPE_UI, PID_NO_L3);
    fbuf_putstr_P(&packet, PSTR("The lazy brown dog jumps over the quick fox 1234567890"));                      
    putstr_P(out, PSTR("Sending (AX25 UI) test packet....\r\n"));        
    fbq_put(outframes, packet);
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
    
    if (argc > 2) {
       ndigis = argc - 1;
       if (ndigis > 7) 
           ndigis = 7;
       for (i=0; i<ndigis; i++)
           str2addr(&digis[i], argv[i+1]);
       SET_BYTE_PARAM(NDIGIS, ndigis);
       SET_PARAM(DIGIS, digis);
       putstr_P(out,PSTR("OK\r\n")); 
    }
    else  {
       ndigis = GET_BYTE_PARAM(NDIGIS);
       GET_PARAM(DIGIS,&digis);
       putstr_P(out, PSTR("Digipeater path is ")); 
       for (i=0; i<ndigis; i++)
       {
           putstr(out, addr2str(cbuf, &digis[i]));
           if (i < ndigis-1)
               putstr(out, ", "); 
       }
       putstr(out,"\n\r");
    }
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
 * config: transmitter power (dBm)
 *********************************************/
 
static void do_power(uint8_t argc, char** argv, Stream* out)
{
    double x = 0;
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
    double x = 0;
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
 ****************************************************************************/
 
uint8_t tokenize(char* buf, char* tokens[], uint8_t maxtokens, char *delim, bool merge)
{ 
     register uint8_t ntokens = 0;
     while (ntokens<maxtokens)
     {
        tokens[ntokens] = strsep(&buf, delim);          /* ER DENNE KORREKT? */
        if ( buf == NULL)
            break;
        if (!merge || *tokens[ntokens] != '\0') 
           ntokens++;
     }
     return (merge && *tokens[ntokens] == '\0' ? ntokens : ntokens+1);
}
