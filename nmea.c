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

#define BUFSIZE   50
#define MAXTOKENS 16

/* Defined in commands.c */
extern uint8_t tokenize(char*, char*[], uint8_t, char*);

static void do_rmc  (uint8_t, char**);
static void do_gga  (uint8_t, char**);

static char buf[BUFSIZE];

/**************************************************************************
 * Read and process NMEA sentences.
 *    Not thread safe. Use in at most one thread, use lock, or allocate
 *    buf on the stack instead. 
 **************************************************************************/
       
void nmeaProcessor(Stream *in)
{
    char* argv[16];
    uint8_t argc;
    uint8_t checksum = 0; 
    int c_checksum;
    
    while (1) {
         getstr(in, buf, BUFSIZE, '\r');
         if (buf[0] != '$')
            continue;
            
         /* Checksum (optional) */
         uint8_t i;
         for (i=1; i<BUFSIZE; i++ && buf[i] !='*' && buf[i] != 0) 
            checksum ^= buf[i];
         if (buf[i] == '*') {
            buf[i] = 0;
            sscanf(buf+i+1, "%d", &c_checksum);
            if (c_checksum != checksum)
               continue;
         }
         
         /* Split input line into tokens */
         argc = tokenize(buf, argv, MAXTOKENS, " \t\r\n");   
      
         /* Select command handler */    
         if (strcmp("RMC", argv[0]+3) == 0)
             do_rmc(argc, argv);
         else if (strcmp("GGA", argv[0]+3) == 0)
             do_gga(argc, argv);

   }
}



static void do_rmc(uint8_t argc, char** argv)
{
    /* TBD */
}

static void do_gga(uint8_t argc, char** argv)
{
    /* TBD */
}





