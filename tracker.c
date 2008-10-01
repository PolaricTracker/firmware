/*
 * $Id: tracker.c,v 1.11 2008-10-01 21:38:46 la7eca Exp $
 */
 
#include "defines.h"
#include "kernel/kernel.h"
#include "gps.h"
#include "config.h"
#include "kernel/timer.h"
// #include "math.h"


Semaphore tracker_run;
posdata_t prev_pos; 
extern fbq_t* outframes;  
extern Stream cdc_outstr;

void tracker_init(void);
void tracker_on(void); 
void tracker_off(void);
static void trackerThread(void);
static bool should_update(posdata_t*, posdata_t*);
static void report_position(posdata_t*);

double fabs(double); /* INLINE FUNC IN MATH.H - CANNOT BE INCLUDED MORE THAN ONCE */


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
    while (true) 
    {
       sem_down(&tracker_run);  
       gps_on();     
       
       while (GET_BYTE_PARAM(TRACKER_ON)) 
       {
          /*
           * Wait for a fix on position. 
           */  
           TRACE(101);
           bool waited = gps_wait_fix();   
           
           /* Pause GPS serial data to avoid interference with modulation 
            * and to save CPU cycles. 
            */
           TRACE(102); 
           uart_rx_pause(); 
           
            
           /* 
            * Send report if criteria are satisfied or if we waited 
            * for GPS fix
            */

           if (waited || should_update(&prev_pos, &current_pos))
           {
              adf7021_power_on(); 
              sleep(50);
              TRACE(103); 
              report_position(&current_pos);
              prev_pos = current_pos;
             
              /* 
               * Before turning off the transceiver chip, wait 
               * a little to allow packet to be received by the 
               * encoder. Then wait until channel is ready and packet 
               * encoded and sent.
               */
              TRACE(104);
              sleep(50);
              TRACE(105);
              hdlc_wait_idle();
              TRACE(106);
              adf7021_wait_tx_off();
              adf7021_power_off(); 
           }
           TRACE(107);         
           GET_PARAM(TRACKER_SLEEP_TIME, &t);
           t = (t > GPS_FIX_TIME) ?
               t - GPS_FIX_TIME : 1;
           sleep(t * TIMER_RESOLUTION); 
           
           TRACE(108);
           uart_rx_resume();
           TRACE(109);
           sleep(GPS_FIX_TIME * TIMER_RESOLUTION);   
       }
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
    TRACE(111);
    uint16_t turn_limit; 
    GET_PARAM(TRACKER_TURN_LIMIT, &turn_limit);
    
    if ( ++pause_count >= GET_BYTE_PARAM(TRACKER_PAUSE_LIMIT) ||             /* Upper time limit */
           (  current_pos.speed > 0 && prev_pos.speed > 0 &&                 /* change in course */
              abs(current_pos.course - prev_pos.course) > turn_limit ) ) 
    {     
       pause_count = 0;
       return true;
    }
    return false;
}



/**********************************************************************
 * Report position as an APRS packet
 *  Currently: Uncompressed APRS position report without timestamp 
 *  (may add more options later)
 **********************************************************************/
extern uint16_t course_count; 
static void report_position(posdata_t* pos)
{
    TRACE(121);
    FBUF packet;    
    char latd[12], longd[12], dspeed[10], comment[COMMENT_LENGTH];
    addr_t from, to; 
    
    /* Format latitude and longitude values, etc. */
    char lat_sn = (pos->latitude < 0 ? 'S' : 'N');
    char long_we = (pos->longitude < 0 ? 'W' : 'E');
    double latf = fabs(pos->latitude);
    double longf = fabs(pos->longitude);
 
    sprintf_P(latd,  PSTR("%02d%05.2f%c\0"), (int)latf, (latf - (int)latf) * 60, lat_sn);
    sprintf_P(longd, PSTR("%03d%05.2f%c\0"), (int)longf, (longf - (int)longf) * 60, long_we);
    sprintf_P(dspeed, PSTR("%03u/%03.0f\0"), pos->course, pos->speed);
    GET_PARAM(REPORT_COMMENT, comment);
    TRACE(122);
    
    /* Create packet header */
    GET_PARAM(MYCALL, &from);
    
    GET_PARAM(DEST, &to);
    addr_t digis[7];
    uint8_t ndigis = GET_BYTE_PARAM(NDIGIS); 
    GET_PARAM(DIGIS, &digis);      
    ax25_encode_header(&packet, &from, &to, digis, ndigis, FTYPE_UI, PID_NO_L3);
    
    /* APRS Packet content */
    fbuf_putChar(&packet, '!');
    fbuf_putstr (&packet, latd);
    fbuf_putChar(&packet, GET_BYTE_PARAM(SYMBOL_TABLE));
    fbuf_putstr (&packet, longd);
    fbuf_putChar(&packet, GET_BYTE_PARAM(SYMBOL));   
    fbuf_putstr (&packet, dspeed); 
    if (*comment != '\0') {
       fbuf_putChar (&packet, '/');
       fbuf_putstr (&packet, comment);     
    } 
    /* Send packet */
    TRACE(123);
    fbq_put(outframes, packet);
}



