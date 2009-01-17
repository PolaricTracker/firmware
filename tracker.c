/*
 * $Id: tracker.c,v 1.18 2009-01-17 11:39:48 la7eca Exp $
 * This is the APRS tracking code
 */
 
#include "defines.h"
#include "kernel/kernel.h"
#include "gps.h"
#include "config.h"
#include "kernel/timer.h"
#include "adc.h"
#include "transceiver.h"
#include "hdlc.h"
#include "uart.h"
#include "ui.h"


// #include "math.h"

Semaphore tracker_run;
posdata_t prev_pos; 
extern fbq_t* outframes;  
extern Stream cdc_outstr;

void tracker_init(void);
void tracker_on(void); 
void tracker_off(void);
static void trackerThread(void);
static void activate_tx(void);
static bool should_update(posdata_t*, posdata_t*);
static void report_position(posdata_t*);
static void report_status(posdata_t*);
static void send_header(FBUF*);
static void send_timestamp(FBUF* packet, posdata_t* pos);
static void send_timestamp_z(FBUF* packet, posdata_t* pos);
static void send_latlong_compressed(FBUF*, double, bool);
static void send_csT_compressed(FBUF*, posdata_t*);


double fabs(double); /* INLINE FUNC IN MATH.H - CANNOT BE INCLUDED MORE THAN ONCE */
int abs(int);  
double round(double);
double log(double);
long lround(double);


/***************************************************************
 * Init tracker. gps_init should be called first.
 ***************************************************************/
 
void tracker_init()
{
    sem_init(&tracker_run, 0);
    THREAD_START(trackerThread, STACK_TRACKER);
    if (GET_BYTE_PARAM(TRACKER_ON)) 
       sem_up(&tracker_run);  
}


void tracker_on() 
{
    SET_BYTE_PARAM(TRACKER_ON, 1);
    sem_up(&tracker_run); 
}

void tracker_off()
{ 
    SET_BYTE_PARAM(TRACKER_ON, 0);
    sem_nb_down(&tracker_run);
}



/**************************************************************
 * main thread for tracking
 **************************************************************/
 
static void trackerThread(void)
{
    uint16_t t;
    uint8_t st_count = 0;
    
    while (true) 
    {
       sem_down(&tracker_run);  
       gps_on();     
       
       while (GET_BYTE_PARAM(TRACKER_ON)) 
       {
          /*
           * Wait for a fix on position. 
           */  
           bool waited = gps_wait_fix();   
           
           /* Pause GPS serial data to avoid interference with modulation 
            * and to save CPU cycles. 
            */
           uart_rx_pause();         
            
           /*
            * Send status report
            */
           if (++st_count == GET_BYTE_PARAM( STATUS_TIME)) {
              st_count = 0;
              report_status(&current_pos);
           }
            
           /* 
            * Send report, if criteria are satisfied, or if we waited 
            * for GPS fix
            */
           if (waited || should_update(&prev_pos, &current_pos)) {
              report_position(&current_pos);
              prev_pos = current_pos;          
           }         
           activate_tx();
           
           GET_PARAM(TRACKER_SLEEP_TIME, &t);
           t = (t > GPS_FIX_TIME) ?
               t - GPS_FIX_TIME : 1;
           sleep(t * TIMER_RESOLUTION); 
           
           uart_rx_resume();
           sleep(GPS_FIX_TIME * TIMER_RESOLUTION);   
       }
    }   
}


/*********************************************************************
 * Activate transmitter - 
 *  If outgoing packets waiting, turn on transmitter, send packets 
 *  and turn off
/*********************************************************************/

static void activate_tx()
{
      if (hdlc_enc_packets_waiting()) {
         adf7021_power_on(); 

         /* 
          * Before turning off the transceiver chip, wait 
          * a little to allow packet to be received by the 
          * encoder. Then wait until channel is ready and then until 
          * packets are encoded and sent.
          */
         sleep(100);
         hdlc_wait_idle();
         adf7021_wait_tx_off();
         adf7021_power_off();
      } 
}



/*********************************************************************
 * This function should return true if we have moved longer
 * than a certain threshold, changed direction more than a 
 * certain amount or at least a certain time has elapsed since
 * the previous update. 
 *********************************************************************/

static uint8_t pause_count = 0;
static bool should_update(posdata_t* prev, posdata_t* current)
{
    uint16_t turn_limit; 
    GET_PARAM(TRACKER_TURN_LIMIT, &turn_limit);
    
    if ( ++pause_count >= GET_BYTE_PARAM(TRACKER_PAUSE_LIMIT) ||             /* Upper time limit */    
           (  current_pos.speed > 1 && prev_pos.speed > 0 &&                 /* change in course */
              abs(current_pos.course - prev_pos.course) > turn_limit ))  
              
           /****
             OR
             pause_count >= min-distance / est_speed() / TRACKER_MIN_PAUSE + min-time ) 
           ****/
    {     
       pause_count = 0;
       return true;
    }
    return false;
}



/**********************************************************************
 * APRS status report. 
 *  What should we put into this report? Currently, I would 
 *  try the battery voltage and a static text. 
 **********************************************************************/

static void report_status(posdata_t* pos)
{
    FBUF packet;   
    
    /* Create packet header */
    send_header(&packet);  
    fbuf_putChar(&packet, '>');
    send_timestamp_z(&packet, pos); 
    
    /* 
     * Get battery voltage - This should perhaps not be here but in status message or
     * telemetry message instead. 
     */
    char vbatt[7];
    sprintf_P(vbatt, PSTR("%.1f\0"), batt_voltage());
    
    /* Send firmware version and battery voltage in status report */
    fbuf_putstr_P(&packet, PSTR("FW="));
    fbuf_putstr_P(&packet, PSTR(VERSION_STRING));
    fbuf_putstr_P(&packet, PSTR(" / VBATT="));
    fbuf_putstr(&packet, vbatt);
   
    /* Send packet */
    fbq_put(outframes, packet);
}



/**********************************************************************
 * Report position by sending an APRS packet
 *
 **********************************************************************/

#define ASCII_BASE 33
#define log108(x) (log((x))/0.076961) 
#define log1002(x) (log((x))/0.001998)

extern uint16_t course_count; 

static void report_position(posdata_t* pos)
{
    static uint8_t ccount;
    FBUF packet;    
    char pbuf[14], comment[COMMENT_LENGTH];
          
    /* Create packet header */
    send_header(&packet);    
    
    /* APRS Packet content 
     * Timestamp */
    uint8_t tstamp = GET_BYTE_PARAM(TIMESTAMP_ON);
    fbuf_putChar(&packet, (tstamp ? '/' : '!'));
    if (tstamp) 
        send_timestamp(&packet, pos);
    
    if (GET_BYTE_PARAM(COMPRESS_ON))
    {  
       fbuf_putChar(&packet, GET_BYTE_PARAM(SYMBOL_TABLE));
       send_latlong_compressed(&packet, pos->latitude, false);
       send_latlong_compressed(&packet, pos->longitude, true);
       fbuf_putChar(&packet, GET_BYTE_PARAM(SYMBOL)); 
       send_csT_compressed(&packet, pos);
    }
    else
    {
       /* Format latitude and longitude values, etc. */
       char lat_sn = (pos->latitude < 0 ? 'S' : 'N');
       char long_we = (pos->longitude < 0 ? 'W' : 'E');
       double latf = fabs(pos->latitude);
       double longf = fabs(pos->longitude);
      
       sprintf_P(pbuf,  PSTR("%02d%05.2f%c\0"), (int)latf, (latf - (int)latf) * 60, lat_sn);
       fbuf_putstr (&packet, pbuf);
       fbuf_putChar(&packet, GET_BYTE_PARAM(SYMBOL_TABLE));
       sprintf_P(pbuf, PSTR("%03d%05.2f%c\0"), (int)longf, (longf - (int)longf) * 60, long_we);
       fbuf_putstr (&packet, pbuf);
       fbuf_putChar(&packet, GET_BYTE_PARAM(SYMBOL));   
       sprintf_P(pbuf, PSTR("%03u/%03.0f\0"), pos->course, pos->speed);
       fbuf_putstr (&packet, pbuf); 

       /* Altitude */
       if (pos->altitude >= 0 && GET_BYTE_PARAM(ALTITUDE_ON)) {
           uint16_t altd = (uint16_t) round(pos->altitude / 0.3048);
           sprintf_P(pbuf,PSTR("/A=%06u\0"), altd);
           fbuf_putstr(&packet, pbuf);
       }
    }  
    
    
    /* Comment */
    if (ccount-- == 0) 
    {
       GET_PARAM(REPORT_COMMENT, comment);
       if (*comment != '\0') {
          fbuf_putChar (&packet, ' ');     /* Or should it be a slash ??*/
          fbuf_putstr (&packet, comment);     
       }
       ccount = COMMENT_PERIOD; 
    }
    
    if (GET_BYTE_PARAM(REPORT_BEEP))
      beep(4);
  
    /* Send packet */
    fbq_put(outframes, packet);
}






static void send_latlong_compressed(FBUF* packet, double pos, bool is_longitude)
{
    uint32_t v = (is_longitude ? 190463 *(180+pos) : 380926 *(90-pos));
    fbuf_putChar(packet, (char) (lround(v / 753571) + ASCII_BASE));
    v %= 753571;
    fbuf_putChar(packet, (char) (lround(v / 8281) + ASCII_BASE));
    v %= 8281;
    fbuf_putChar(packet, (char) (lround(v / 91) + ASCII_BASE));
    v %= 91;
    fbuf_putChar(packet, (char) (lround(v) + ASCII_BASE));   
}



static void send_csT_compressed(FBUF* packet, posdata_t* pos)
/* FIXME: Special case where there is no course/speed */
{
    if (pos->altitude >= 0 && GET_BYTE_PARAM(ALTITUDE_ON)) {
       /* Send altitude */
       uint32_t alt =  (uint32_t) log1002((double)pos->altitude / 0.3048);
       fbuf_putChar(packet, (char) (lround(alt / 91) + ASCII_BASE));
       alt %= 91;
       fbuf_putChar(packet, (char) (lround(alt) + ASCII_BASE));
       fbuf_putChar(packet, 0x10 + ASCII_BASE);
    }
    else {
       /* Send course/speed (default) */
       fbuf_putChar(packet, pos->course / 4 + ASCII_BASE);
       fbuf_putChar(packet, (char) log108((double) pos->speed+1) + ASCII_BASE); 
       fbuf_putChar(packet, 0x18 + ASCII_BASE);
    }
}




static void send_header(FBUF* packet)
{
    addr_t from, to; 
    GET_PARAM(MYCALL, &from);   
    GET_PARAM(DEST, &to);
    addr_t digis[7];
    uint8_t ndigis = GET_BYTE_PARAM(NDIGIS); 
    GET_PARAM(DIGIS, &digis);      
    ax25_encode_header(packet, &from, &to, digis, ndigis, FTYPE_UI, PID_NO_L3);
}



static void send_timestamp(FBUF* packet, posdata_t* pos)
{
    char ts[9];
    sprintf_P(ts, PSTR("%02u%02u%02uh\0"), 
       (uint8_t) ((pos->timestamp / 3600) % 24), 
       (uint8_t) ((pos->timestamp / 60) % 60), 
       (uint8_t) (pos->timestamp % 60) );
    fbuf_putstr(packet, ts);   
}


static void send_timestamp_z(FBUF* packet, posdata_t* pos)
{
    char ts[9];
    sprintf_P(ts, PSTR("%02u%02u%02uz\0"), 
       (uint8_t) (pos->timestamp / 86400)+1,
       (uint8_t) ((pos->timestamp / 3600) % 24), 
       (uint8_t) ((pos->timestamp / 60) % 60) ); 
    fbuf_putstr(packet, ts);   
}

