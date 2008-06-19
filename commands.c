/*
 * $Id: commands.c,v 1.15 2008-06-19 18:42:31 la7eca Exp $
 */
 
#include "defines.h"
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include "timer.h"
#include "stream.h"
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
static void do_txdelay   (uint8_t, char**, Stream*);
static void do_txtail    (uint8_t, char**, Stream*);
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
static void do_deviation (uint8_t, char**, Stream*);
static void do_rssi      (uint8_t, char**, Stream*, Stream*);

static char buf[BUFSIZE]; 
extern fbq_t* outframes;  

extern void tracker_on(void);
extern void tracker_off(void);


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
    while (1) {
         putstr(out, "cmd: ");    
         readLine(in, out, buf, BUFSIZE);
         
         /* Split input line into argument tokens */
         argc = tokenize(buf, argv, MAXTOKENS, " \t\r\n", true);

         /* Select command handler */         
         if (strncmp("teston", argv[0], 6) == 0)
             do_teston(argc, argv, out);
         else if (strncmp("testoff", argv[0], 7) == 0)
             do_testoff(argc, argv, out);
         else if (strncmp("testpacket",  argv[0], 5) == 0)
             do_testpacket(argc, argv, out);
         else if (strncmp("txdelay",     argv[0], 3) == 0)
             do_txdelay(argc, argv, out);
         else if (strncmp("txtail",     argv[0], 3) == 0)
             do_txtail(argc, argv, out);
         else if (strncmp("mycall",     argv[0], 2) == 0)
             do_mycall(argc, argv, out);    
         else if (strncmp("dest",     argv[0], 3) == 0)
             do_dest(argc, argv, out);    
         else if (strncmp("symbol",   argv[0], 3) == 0)
             do_symbol(argc, argv, out);               
         else if (strncmp("freq", argv[0], 2) == 0)
             do_freq(argc, argv, out);  
         else if (strncmp("power", argv[0], 2) == 0)
             do_power(argc, argv, out);    
         else if (strncmp("squelch", argv[0], 2) == 0)
             do_squelch(argc, argv, out);       
         else if (strncmp("deviation", argv[0], 3) == 0)
             do_deviation(argc, argv, out);               
         else if (strncmp("gps",     argv[0], 3) == 0)
             do_nmea(argc, argv, out, in);     
         else if (strncmp("trx",     argv[0], 3) == 0)
             do_trx(argc, argv, out, in);        
         else if (strncmp("tracker", argv[0], 3) == 0)
             do_tracker(argc, argv, out, in);
         else if (strncmp("txon",     argv[0], 4) == 0)
             do_txon(argc, argv, out);        
         else if (strncmp("txoff",     argv[0], 4) == 0)
             do_txoff(argc, argv, out);     
         else if (strncmp("rssi", argv[0], 2) == 0)
             do_rssi(argc, argv, out, in);                 
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
    
    addr_t digis[] = {{"WIDE2", 2}}; 
            
    ax25_encode_header(&packet, &from, &to, digis, 1, FTYPE_UI, PID_NO_L3);
    fbuf_putstr_P(&packet, PSTR("The lazy brown dog jumps over the quick fox 1234567890"));                      
    putstr_P(out, PSTR("Sending (AX25 UI) test packet....\r\n"));        
    fbq_put(outframes, packet);
}

         
         
/*********************************************
 * config: txdelay 
 *********************************************/
 
static void do_txdelay(uint8_t argc, char** argv, Stream* out)
{
    if (argc > 1) {
       int x = 0;
       sscanf(argv[1], " %d", &x);   
       SET_BYTE_PARAM(TXDELAY, x);
    } 
    else {
       uint8_t x = GET_BYTE_PARAM(TXDELAY);
       sprintf_P(buf, PSTR("TXDELAY is: %d\r\n\0"), x);
       putstr(out, buf);
    }
}

         
/*********************************************
 * config: txtail 
 *********************************************/
 
static void do_txtail(uint8_t argc, char** argv, Stream* out)
{
    if (argc > 1) {
       int x = 0;
       sscanf(argv[1], " %d", &x);   
       SET_BYTE_PARAM(TXTAIL, x);
    } 
    else {
       uint8_t x = GET_BYTE_PARAM(TXTAIL);
       sprintf_P(buf, PSTR("TXTAIL is: %d\r\n\0"), x);
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
   }
   else {
      GET_PARAM(DEST, &x);
      sprintf_P(buf, PSTR("DEST is: %s\r\n\0"), addr2str(cbuf, &x));
      putstr(out, buf);
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
    } 
    else {
       GET_PARAM(TRX_SQUELCH, &x);
       sprintf_P(buf, PSTR("SQUELCH level is: %f dBm\r\n\0"), x);
       putstr(out, buf);
    }
}


/*********************************************
 * config: AFSK modulation deviation
 *********************************************/
 
static void do_deviation(uint8_t argc, char** argv, Stream* out)
{
    uint16_t x = 0;
    if (argc > 1) {
       sscanf(argv[1], " %d", &x);  
       SET_PARAM(TRX_AFSK_DEV, &x);
    } 
    else {
       GET_PARAM(TRX_AFSK_DEV, &x);
       sprintf_P(buf, PSTR("DEVIATION is: %d\r\n\0"), x);
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
