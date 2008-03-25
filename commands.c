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

#define MAXTOKENS 10
#define BUFSIZE   40

uint8_t tokenize(char*, char*[], uint8_t, char*);

static void do_teston    (uint8_t, char**, Stream*);
static void do_testoff   (uint8_t, char**, Stream*);
static void do_testpacket(uint8_t, char**, Stream*);
static void do_txdelay   (uint8_t, char**, Stream*);
static void do_txtail    (uint8_t, char**, Stream*);
static void do_mycall    (uint8_t, char**, Stream*);
static void do_dest      (uint8_t, char**, Stream*);

static char buf[BUFSIZE]; 
extern fbq_t* outframes;  

/**************************************************************************
 * Read and process commands .
 *    Not thread safe. Use in at most one thread, use lock, or allocate
 *    buf on the stack instead. 
 **************************************************************************/
       
void cmdProcessor(Stream *in, Stream *out)
{
    char* argv[MAXTOKENS];
    uint8_t argc;
    
    putstr_P(out, PSTR("\n\rVelkommen til LA3T 'Polaric Tracker' firmware\n\r"));
    while (1) {
         putstr(out, "cmd: ");     
         getstr(in, buf, BUFSIZE, '\r');
         
         /* Split input line into argument tokens */
         argc = tokenize(buf, argv, MAXTOKENS, " \t\r\n");
         
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
         else if (strncmp("dest",     argv[0], 2) == 0)
             do_dest(argc, argv, out);      
         else if (strlen(argv[0]) > 1)
             putstr_P(out, PSTR("*** Unknown command\n\r"));
   }
}


      
/*********************************************
 * teston <byte> : Turn on 	test signal
 *********************************************/
 
static void do_teston(uint8_t argc, char** argv, Stream* out)
{
    int ch = 0;
    hdlc_test_off();
    sleep(10);
    sscanf(argv[1], " %i", &ch);  
    hdlc_test_on((uint8_t) ch);
    sprintf_P(buf, PSTR("Test signal on: 0x%X\n\r\0"), ch);
    putstr(out, buf );  
}


         
/*********************************************
 * testoff : Turn off test signal
 *********************************************/
 
static void do_testoff(uint8_t argc, char** argv, Stream* out)
{
    putstr_P(out, PSTR("Test signal off\n\r"));  
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
    
    addr_t digis[] = {{"LD9TS", 0}}; 
            
    ax25_encode_header(&packet, &from, &to, digis, 1, FTYPE_UI, PID_NO_L3);
    fbuf_putstr_P(&packet, PSTR("The lazy brown dog jumps over the quick fox 1234567890"));                      
    putstr_P(out, PSTR("Sending (AX25 UI) test packet....\n\r"));        
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
       sprintf_P(buf, PSTR("TXDELAY is: %d\n\r\0"), x);
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
       sprintf_P(buf, PSTR("TXTAIL is: %d\n\r\0"), x);
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
      sprintf_P(buf, PSTR("MYCALL is: %s\n\r\0"), addr2str(cbuf, &x));
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
      sprintf_P(buf, PSTR("DEST is: %s\n\r\0"), addr2str(cbuf, &x));
      putstr(out, buf);
   }   
}

         
/*********************************************
 * split input string into tokens
 *********************************************/
 
uint8_t tokenize(char* buf, char* tokens[], uint8_t maxtokens, char *delim)
{ 
     register uint8_t ntokens;
     char* save;
     
     tokens[0] = strtok_r(buf, delim, &save);
     for (ntokens=1; ntokens<maxtokens; ntokens++) 
     {
        tokens[ntokens] = strtok_r(NULL, delim, &save);
        if (tokens[ntokens] == NULL) 
           break;
     }
     return ntokens;
}
