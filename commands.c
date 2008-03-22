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
#include "config.h"

static void do_teston(uint8_t,    char**, Stream*);
static void do_testoff(uint8_t,   char**, Stream*);
static void do_testpacket(uint8_t,char**, Stream*);
static void do_txdelay(uint8_t,   char**, Stream*);
static void do_txtail(uint8_t,    char**, Stream*);


static char buf[40];
extern fbq_t* outframes;  

/**************************************************************************
 * Read and process commands 
 **************************************************************************/
       
void cmdProcessor(Stream *in, Stream *out)
{
    char* save;
    char* argv[10];
    uint8_t argc;
    
    putstr_P(out, PSTR("\n\rVelkommen til LA3T AVR test firmware\n\r"));
    while (1) {
         putstr(out, "cmd: ");     
         getstr(in, buf, 40, '\r');
         
         /* Parse input line into argument tokens */
         argv[0] = strtok_r(buf, " \t\r\n", &save);
         for (argc=1; argc<10; argc++) 
         {
            argv[argc] = strtok_r(NULL, "\t\r\n", &save);
            if (argv[argc] == NULL) 
               break;
         }
         
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
         else if (strlen(argv[0]) > 1)
             putstr_P(out, PSTR("Unknown command\n\r"));
   }
}


      
/***************************************
 * teston <byte> : Turn on 	test signal
 ***************************************/
 
static void do_teston(uint8_t argc, char** argv, Stream* out)
{
    int ch = 0;
    hdlc_test_off();
    sleep(10);
    sscanf(argv[1], " %i", &ch);          // FIXME. Check
    hdlc_test_on((uint8_t) ch);
    sprintf_P(buf, PSTR("Test signal on: 0x%X\n\r\0"), ch);
    putstr(out, buf );  
}


         
/**********************************
 * testoff : Turn off test signal
 **********************************/
 
static void do_testoff(uint8_t argc, char** argv, Stream* out)
{
    putstr_P(out, PSTR("Test signal off\n\r"));  
    hdlc_test_off();  
}


         
/*********************************
 * tx : Send AX25 test packet
 *********************************/

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

