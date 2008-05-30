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
#include "gps.h"


#define NMEA_BUFSIZE   80
#define NMEA_MAXTOKENS 16

/* Defined in commands.c */
uint8_t tokenize(char*, char*[], uint8_t, char*, bool);

/* Defined in uart.c */
Stream* uart_tx_init(void);
Stream* uart_rx_init(bool);

posdata_t current_pos; 

/* Local handlers */
static void do_rmc        (uint8_t, char**, Stream*);
static void do_gga        (uint8_t, char**, Stream*);
static void nmeaListener  (void); 

static char buf[NMEA_BUFSIZE];
static Cond wait_gps; 
static bool monitor_pos, monitor_raw; 
static Stream *in, *out; 
static bool is_locked = true;
extern uint8_t blink_length, blink_interval;


void gps_init(Stream *outstr)
{
    cond_init(&wait_gps); 
    monitor_pos = monitor_raw = false; 
    in = uart_rx_init(FALSE);
    out = outstr;
    THREAD_START(nmeaListener, 200);
    make_output(GPSON); 
    set_port(GPSON);
}

void gps_off()
{ 
   set_port(GPSON);
   is_locked = false; 
   BLINK_NORMAL; 
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
             putstr(out, "\r\n");
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

void gps_mon_pos(void)
   { monitor_pos = true; }
void gps_mon_raw(void)
   { monitor_raw = true; }
void gps_mon_off(void)
   { monitor_pos = monitor_raw = false; }
   

    
   
/****************************************************************
 * Convert position NMEA fields to float (degrees)
 ****************************************************************/

static void str2coord(const uint8_t ndeg, const char* str, double* coord)
{
    double minutes;
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

static void notify_lock(bool lock)
{
   if (!lock && is_locked) 
       BLINK_GPS_SEARCHING
   else if (lock && !is_locked) {
       notifyAll(&wait_gps);     
       BLINK_NORMAL
   }
   is_locked = lock;
}

bool gps_is_locked()
   { return is_locked; }
   
void gps_wait_lock()
   { while (!is_locked)
       wait(&wait_gps); }         
  
  
       
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
    str2coord(2, argv[3], &current_pos.latitude);  
    if (*argv[4] == 'S')
        current_pos.latitude = -current_pos.latitude;
        
     /* get longitude [dddmm.mmmmm] */
    str2coord(3, argv[5], &current_pos.longitude);  
    if (*argv[6] == 'W')
        current_pos.longitude = -current_pos.longitude;
    
    /* If requested, show position on screen */    
    if (monitor_pos) {
        sprintf_P(buf, PSTR("TIME: %s, POS: lat=%f, long=%f\r\n"), 
          time2str(tbuf, time), current_pos.latitude, current_pos.longitude);
        putstr(out, buf);
    }
}



static void do_gga(uint8_t argc, char** argv, Stream *out)
{
    /* TBD if needed */
}





