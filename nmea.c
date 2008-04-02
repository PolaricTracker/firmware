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


#define BUFSIZE   70
#define MAXTOKENS 16

/* Defined in commands.c */
uint8_t tokenize(char*, char*[], uint8_t, char*);

/* Local handlers */
static void do_rmc  (uint8_t, char**);
static void do_gga  (uint8_t, char**);

static char buf[BUFSIZE];



typedef struct _PosData {
    float latitude;
    float longitude;
    float time;
} posdata_t;

posdata_t pos;
extern Stream cdc_outstr;  /* FOR DEBUGGING */


/**************************************************************************
 * Read and process NMEA sentences.
 *    Not thread safe. Use in at most one thread, use lock, or allocate
 *    buf on the stack instead. 
 **************************************************************************/

void nmeaProcessor(Stream* in)
{
    char* argv[16];
    uint8_t argc;
    uint8_t checksum = 0; 
    int c_checksum;
    
    while (1) {
         getstr(in, buf, BUFSIZE, '\r');
         putstr(&cdc_outstr, buf);         /* ECHO FOR DEBUGGING */
         if (buf[0] != '$')
            continue;
            
         /* Checksum (optional) */
         uint8_t i;
         for (i=1; i<BUFSIZE && buf[i] !='*' && buf[i] != 0; i++) 
            checksum ^= buf[i];
         if (buf[i] == '*') {
            buf[i] = 0;
            sscanf(buf+i+1, "%X", &c_checksum);
            if (c_checksum != checksum)
               continue;
         }
         
         /* Split input line into tokens */
         argc = tokenize(buf, argv, MAXTOKENS, ",");   
         
         /* Select command handler */    
         if (strcmp("RMC", argv[0]+3) == 0)
             do_rmc(argc, argv);
         else if (strcmp("GGA", argv[0]+3) == 0)
             do_gga(argc, argv);

   }
}

static float str2coord(const uint8_t ndeg, const char* str)
{
    float coord, minutes;
    char dstring[ndeg+1];
    
    /* Format [ddmm.mmmmm] */
    strncpy(dstring, str, ndeg);
    dstring[ndeg] = 0;

    sscanf(dstring, "%f", &coord);     /* Degrees */
    sscanf(str+ndeg, "%f", &minutes);  /* Minutes */
    coord += (minutes / 60);
    return coord;
}



static void do_rmc(uint8_t argc, char** argv)
{
    if (argc != 12)                   /* Ignore if wrong format */
       return;
    if (*argv[2] != 'A')              /* Ignore if receiver not in lock */
       return;
   
    /* get latitude [ddmm.mmmmm] */
    pos.latitude = str2coord(2, argv[3]);   
    if (*argv[4] == 'S')
        pos.latitude = -pos.latitude;
        
     /* get longitude [dddmm.mmmmm] */
    pos.longitude = str2coord(3, argv[5]);  
    if (*argv[6] == 'W')
        pos.longitude = -pos.longitude;
}



static void do_gga(uint8_t argc, char** argv)
{
    /* TBD if needed */
}





