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
uint8_t tokenize(char*, char*[], uint8_t, char*, bool);

/* Local handlers */
static void do_rmc  (uint8_t, char**);
static void do_gga  (uint8_t, char**);

static char buf[BUFSIZE];

typedef struct _TimeStamp {
    uint32_t time;
    uint8_t  year; 
    uint8_t  month; 
    uint8_t  day;
} timestamp_t;

typedef struct _PosData {
    float latitude;
    float longitude;
    timestamp_t timestamp;
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
 
    while (1) {
         uint8_t checksum = 0; 
         int c_checksum;
         
         getstr(in, buf, BUFSIZE, '\n');
 //        putstr(&cdc_outstr, buf);         /* ECHO FOR DEBUGGING */
         if (buf[0] != '$')
            continue;
            
         /* Checksum (optional) */
         uint8_t i;
         for (i=1; i<BUFSIZE && buf[i] !='*' && buf[i] != 0 ; i++) 
            checksum ^= buf[i];
         if (buf[i] == '*') {
            buf[i] = 0;
            sscanf(buf+i+1, "%X", &c_checksum);
            if (c_checksum != checksum)
               continue;
         } 
         
         /* Split input line into tokens */
         argc = tokenize(buf, argv, MAXTOKENS, ",", false);   
         
         /* Select command handler */    
         if (strcmp("RMC", argv[0]+3) == 0)
             do_rmc(argc, argv);
         else if (strcmp("GGA", argv[0]+3) == 0)
             do_gga(argc, argv);

   }
}

void str2coord(const uint8_t ndeg, const char* str, float* coord)
{
    float minutes;
    char dstring[ndeg+1];

    /* Format [ddmm.mmmmm] */
    strncpy(dstring, str, ndeg);
    dstring[ndeg] = 0;
    
    sscanf(dstring, "%f", coord);      /* Degrees */
    sscanf(str+ndeg, "%f", &minutes);  /* Minutes */
    *coord += (minutes / 60);
    
    char buf[30];
}


void nmea2time(const char* timestr, uint32_t* t)
{
    int hour, min, sec;
    sscanf(timestr, "%2u%2u%2u", &hour, &min, &sec);
    *t = (uint32_t) hour * 3600 + min * 60 + sec;
}

char* time2str(char* buf, uint32_t time)
{
    sprintf(buf, "%2u:%2u:%2u", 
      (uint8_t) (time / 3600), (uint8_t) (time / 60 % 60), (uint8_t) (time % 60) );
    return buf;
}
 


static void do_rmc(uint8_t argc, char** argv)
{
    char buf[60], tbuf[9];
    
    if (argc != 13)            /* Ignore if wrong format */
       return;
   // if (*argv[2] != 'A')     /* Ignore if receiver not in lock */
   //   return;
   
    /* get timestamp */
    uint32_t time; 
    nmea2time(argv[1], &time);
   
    /* get latitude [ddmm.mmmmm] */
    str2coord(2, argv[3], &pos.latitude);   
    if (*argv[4] == 'S')
        pos.latitude = -pos.latitude;
        
     /* get longitude [dddmm.mmmmm] */
    str2coord(3, argv[5], &pos.longitude);  
    if (*argv[6] == 'W')
        pos.longitude = -pos.longitude;
        
    sprintf(buf, "TIME: %s, POS: lat=%f, long=%f\r\n", 
       time2str(tbuf, time), pos.latitude, pos.longitude);
    putstr(&cdc_outstr, buf);
}



static void do_gga(uint8_t argc, char** argv)
{
    /* TBD if needed */
}





