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
#include "nmea.h"


#define NMEA_BUFSIZE   80
#define NMEA_MAXTOKENS 16

/* Defined in commands.c */
uint8_t tokenize(char*, char*[], uint8_t, char*, bool);

/* Local handlers */
static void do_rmc  (uint8_t, char**, Stream*);
static void do_gga  (uint8_t, char**, Stream*);

static char buf[NMEA_BUFSIZE];

static posdata_t pos;
static bool monitor_pos, monitor_raw; 


/**************************************************************************
 * Read and process NMEA sentences.
 *    Not thread safe. Use in at most one thread, use lock, or allocate
 *    buf on the stack instead. 
 **************************************************************************/

void nmeaProcessor(Stream* in, Stream* out )
{
    char* argv[16];
    uint8_t argc;
 
    monitor_pos = monitor_raw = false; 
    while (1) {
         uint8_t checksum = 0; 
         int c_checksum;
         
         getstr(in, buf, NMEA_BUFSIZE, '\n');
         if (buf[0] != '$')
            continue;

         /* Checksum (optional) */
         uint8_t i;
         for (i=1; i<NMEA_BUFSIZE && buf[i] !='*' && buf[i] != 0 ; i++) 
            checksum ^= buf[i];
         if (buf[i] == '*') {
            buf[i] = 0;
            sscanf(buf+i+1, "%X", &c_checksum);
            if (c_checksum != checksum)
               continue;
         } 
         
         /* If requested, show raw NMEA packet on screen */
         if (monitor_raw) {
             putstr(out, buf);
             putstr(out, "\n");
         }
        
         /* Split input line into tokens */
         argc = tokenize(buf, argv, NMEA_MAXTOKENS, ",", false);   
         
         /* Select command handler */    
         if (strcmp("RMC", argv[0]+3) == 0)
             do_rmc(argc, argv, out);
         else if (strcmp("GGA", argv[0]+3) == 0)
             do_gga(argc, argv, out);

   }
}

/****************************************************************
 * Monitoring control
 *   nmea_mon_pos - valid GPRMC position reports
 *   nmea_mon_raw - NMEA packets  
 *   nmea_mon_off - turn it all off
 ****************************************************************/

void nmea_mon_pos(void)
   { monitor_pos = true; }
void nmea_mon_raw(void)
   { monitor_raw = true; }
void nmea_mon_off(void)
   { monitor_pos = monitor_raw = false; }
   
   
/****************************************************************
 * Get current position info
 ****************************************************************/
posdata_t getPos() 
   { return pos; }
    
   
/****************************************************************
 * Convert position NMEA fields to float (degrees)
 ****************************************************************/

static void str2coord(const uint8_t ndeg, const char* str, float* coord)
{
    float minutes;
    char dstring[ndeg+1];

    /* Format [ddmm.mmmmm] */
    strncpy(dstring, str, ndeg);
    dstring[ndeg] = 0;
    
    sscanf(dstring, "%f", coord);      /* Degrees */
    sscanf(str+ndeg, "%f", &minutes);  /* Minutes */
    *coord += (minutes / 60);
}


/*****************************************************************
 * Convert date/time NMEA fields to 32 bit integer (timestamp)
 *****************************************************************/
 
static void nmea2time( timestamp_t* t, const char* timestr)
{
    int hour, min, sec;
    sscanf(timestr, "%2u%2u%2u", &hour, &min, &sec);
    *t = (uint32_t) hour * 3600 + min * 60 + sec;
}



char* time2str(char* buf, timestamp_t time)
{
    sprintf(buf, "%2u:%2u:%2u", 
      (uint8_t) (time / 3600 % 24), (uint8_t) (time / 60 % 60), (uint8_t) (time % 60) );
    return buf;
}
 
 
 
/****************************************************************
 * handle changes in GPS lock - mainly change LED blinking
 ****************************************************************/
 
extern uint8_t blink_length, blink_interval;
static bool is_locked;

void notify_lock(bool lock)
{
   if (!lock && is_locked) 
       BLINK_GPS_SEARCHING
   else if (lock && !is_locked) 
       BLINK_NORMAL
   is_locked = lock;
}

bool gps_is_locked()
   { return is_locked; }
   
         
       
/****************************************************************
 * Handle RMC line
 ****************************************************************/

static void do_rmc(uint8_t argc, char** argv, Stream *out)
{
    char buf[60], tbuf[9];
    if (argc != 13)            /* Ignore if wrong format */
       return;
    if (*argv[2] != 'A') { 
      notify_lock(false);      /* Ignore if receiver not in lock */
      return;
    }
    else
      notify_lock(true);

    /* get timestamp */
    timestamp_t time; 
    nmea2time(&time, argv[1]);
   
    /* get latitude [ddmm.mmmmm] */
    str2coord(2, argv[3], &pos.latitude);  
    if (*argv[4] == 'S')
        pos.latitude = -pos.latitude;
        
     /* get longitude [dddmm.mmmmm] */
    str2coord(3, argv[5], &pos.longitude);  
    if (*argv[6] == 'W')
        pos.longitude = -pos.longitude;
    
    /* If requested, show position on screen */    
    if (monitor_pos) {
        sprintf_P(buf, PSTR("TIME: %s, POS: lat=%f, long=%f\r\n"), 
          time2str(tbuf, time), pos.latitude, pos.longitude);
        putstr(out, buf);
    }
}



static void do_gga(uint8_t argc, char** argv, Stream *out)
{
    /* TBD if needed */
}





