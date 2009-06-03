/* 
 * $Id: gps.c,v 1.20 2009-06-03 07:38:36 la7eca Exp $
 * NMEA data 
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
#include "gps.h"


#define NMEA_BUFSIZE   80
#define NMEA_MAXTOKENS 16


/* Defined in commands.c */
uint8_t tokenize(char*, char*[], uint8_t, char*, bool);

/* Defined in uart.c */
Stream* uart_tx_init(uint16_t);
Stream* uart_rx_init(uint16_t, bool);

/* Current position */
posdata_t current_pos; 

/* Local handlers */
static void do_rmc        (uint8_t, char**, Stream*);
static void do_gga        (uint8_t, char**, Stream*);
static void nmeaListener  (void); 
void notify_fix   (bool);

static char buf[NMEA_BUFSIZE];
static Cond wait_gps; 
static bool monitor_pos, monitor_raw; 
static Stream *in, *out; 
static bool is_fixed = true;
extern uint8_t blink_length, blink_interval;


void gps_init(Stream *outstr)
{
    cond_init(&wait_gps); 
    monitor_pos = monitor_raw = false; 
    uint16_t baud; 
    GET_PARAM(GPS_BAUD, &baud);
    in = uart_rx_init(baud, FALSE);
    out = outstr;
    THREAD_START(nmeaListener, STACK_GPSLISTENER);
    make_output(GPSON); 
    set_port(GPSON);
}

void gps_on()
{
   clear_port(GPSON);
   notify_fix(false);
}


void gps_off()
{ 
   set_port(GPSON);
   BLINK_NORMAL   
}
 
 

/**************************************************************************
 * Read and process NMEA sentences.
 *    Not thread safe. Use in at most one thread, use lock, or allocate
 *    buf on the stack instead. 
 **************************************************************************/

static void nmeaListener()
{
    char* argv[16];
    uint8_t argc;
   
    while (1) {
         uint8_t checksum = 0; 
         int c_checksum;
                 
         getstr(in, buf, NMEA_BUFSIZE, '\n');
         
         if (buf[0] != '$')
            continue;

         
         /* If requested, show raw NMEA packet on screen */
         if (monitor_raw) {
             putstr(out, buf);
             putstr(out, "\n");
         }
         
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
        
         /* Split input line into tokens */
         argc = tokenize(buf, argv, NMEA_MAXTOKENS, ",", false);           
         
         /* Select command handler */    
         if (strncmp("RMC", argv[0]+3, 3) == 0)
             do_rmc(argc, argv, out);
         else if (strncmp("GGA", argv[0]+3, 3) == 0)
             do_gga(argc, argv, out);
   }
}



/****************************************************************
 * Monitoring control
 *   nmea_mon_pos - valid GPRMC position reports
 *   nmea_mon_raw - NMEA packets  
 *   nmea_mon_off - turn it all off
 ****************************************************************/

void gps_mon_pos(void)
   { monitor_pos = true; }
void gps_mon_raw(void)
   { monitor_raw = true; }
void gps_mon_off(void)
   { monitor_pos = monitor_raw = false; }
   

    
   
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
 
static void nmea2time( timestamp_t* t, const char* timestr, const char* datestr)
{
    uint16_t hour, min, sec;
    uint16_t date, month, year;
    sscanf(timestr, "%2u%2u%2u", &hour, &min, &sec);
    sscanf(datestr, "%2u%2u%2u", &date, &month, &year);
    *t = (uint32_t) 
         ((uint32_t) date-1) * 86400 +  ((uint32_t)hour) * 3600 + ((uint32_t)min) * 60 + sec;
}



char* time2str(char* buf, timestamp_t time)
{
    sprintf(buf, "%2u:%2u:%2u", 
      (uint8_t) ((time / 3600) % 24), (uint8_t) ((time / 60) % 60), (uint8_t) (time % 60) );
    return buf;
}
 
 
 
/****************************************************************
 * handle changes in GPS lock - mainly change LED blinking
 ****************************************************************/

void notify_fix(bool lock)
{
   if (!lock) 
       BLINK_GPS_SEARCHING
   else {
       if (!is_fixed) {
          notifyAll(&wait_gps);
       }     
       BLINK_NORMAL
   }
   is_fixed = lock;
}


bool gps_is_fixed()
   { return is_fixed; }
   
   
bool gps_wait_fix()
{   
    if (is_fixed) return false;      
    while (!is_fixed) 
       wait(&wait_gps);
    return true;
}         
 
bool gps_hasWaiters()
   {return hasWaiters(&wait_gps); }
   
   
uint16_t course_count = 0;  
float altitude = -1;
       
       
       
/****************************************************************
 * Handle RMC line
 ****************************************************************/

static void do_rmc(uint8_t argc, char** argv, Stream *out)
{
    static uint8_t lock_cnt = 4;
    
    char buf[95], tbuf[9];
    if (argc != 13)                 /* Ignore if wrong format */
       return;

    if (*argv[2] != 'A') { 
       notify_fix(false);          /* Ignore if receiver not in lock */
       lock_cnt = 4;
       return;
    }
    else
      if (lock_cnt > 0) {
         lock_cnt--;
         return;
      }
      
    lock_cnt = 1;
    notify_fix(true);
       
    /* get timestamp */
    nmea2time(&current_pos.timestamp, argv[1], argv[9]);
   
    /* get latitude [ddmm.mmmmm] */
    str2coord(2, argv[3], &current_pos.latitude);  
    if (*argv[4] == 'S')
        current_pos.latitude = -current_pos.latitude;
        
     /* get longitude [dddmm.mmmmm] */
    str2coord(3, argv[5], &current_pos.longitude);  
    if (*argv[6] == 'W')
        current_pos.longitude = -current_pos.longitude;
    
    /* get speed [nnn.nn] */
    if (*argv[7] != '\0')
       sscanf(argv[7], "%f", &current_pos.speed);
    else
       current_pos.speed = 0;
       
    /* get course [nnn.nn] */
    if (*argv[8] != '\0') {
       double x;
       sscanf(argv[8], "%f", &x);
       current_pos.course = (uint16_t) x+0.5;
    }
    else
       current_pos.course = 0;
    current_pos.altitude = altitude;
           
    /* If requested, show position on screen */    
    if (monitor_pos) {
        sprintf_P(buf, PSTR("TIME: %s, POS: lat=%f, long=%f, SPEED: %f km/h, COURSE: %u deg\r\n"), 
          time2str(tbuf, current_pos.timestamp), current_pos.latitude, current_pos.longitude, 
          current_pos.speed*KNOTS2KMH, current_pos.course);
        putstr(out, buf);
    }
}


/******************************************* 
 * Get altitude from GGA line
 *******************************************/

static void do_gga(uint8_t argc, char** argv, Stream *out)
{
    if (argc == 15 && *argv[6] > '0')
       sscanf(argv[9], "%f", &altitude);
    else
       altitude = -1; 
}





