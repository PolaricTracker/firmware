/*
 * $Id: tracker.c,v 1.8 2008-09-08 22:37:39 la7eca Exp $
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
    THREAD_START(trackerThread, 250);
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
           * OOPS: Should there be a timeout on this? 
           */  
           TRACE(10);
           gps_wait_lock();   
           
           /* Pause GPS serial data to avoid interference with modulation 
            * and to save CPU cycles. 
            */
           uart_rx_pause(); 
           
            
           /* Send report if criteria are satisfied */
           TRACE(11);
           if (should_update(&prev_pos, &current_pos))
           {
              adf7021_power_on(); 
              sleep(100);
              TRACE(12); 
              report_position(&current_pos);
              prev_pos = current_pos;
             
              /* 
               * Before turning off the transceiver chip, wait 
               * a little to allow packet to be received by the 
               * encoder. Then wait until channel is ready and packet 
               * encoded and sent.
               */
              TRACE(13);
              sleep(100);
              hdlc_wait_idle();
              adf7021_wait_tx_off();
              adf7021_power_off(); 
           }
           TRACE(14);         
           GET_PARAM(TRACKER_SLEEP_TIME, &t);
           t = (t > GPS_FIX_TIME + PACKET_TX_TIME) ?
               t - (GPS_FIX_TIME + PACKET_TX_TIME) : (GPS_FIX_TIME + PACKET_TX_TIME);
           sleep(t * TIMER_RESOLUTION); 
           
           TRACE(15);
           uart_rx_resume();
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
    uint16_t turn_limit; 
    GET_PARAM(TRACKER_TURN_LIMIT, &turn_limit);
    
    if ( pause_count >= GET_BYTE_PARAM(TRACKER_PAUSE_LIMIT) ||             /* Upper time limit */
           (  current_pos.speed > 0 && prev_pos.speed > 0 &&               /* change in course */
              abs(current_pos.course - prev_pos.course) > turn_limit ) ) 
    {     
       pause_count = 0;
       return true;
    }
    pause_count++;
    return false;
}


/**********************************************************************
 * Report position as an APRS packet
 *  Currently: Uncompressed APRS position report without timestamp 
 *  (may add more options later)
 **********************************************************************/
 
static void report_position(posdata_t* pos)
{
    FBUF packet;    
    char latd[12], longd[12], dspeed[8], comment[COMMENT_LENGTH];
    addr_t from, to; 
    
    /* Format latitude and longitude values, etc. */
    char lat_sn = (pos->latitude < 0 ? 'S' : 'N');
    char long_we = (pos->longitude < 0 ? 'W' : 'E');
    double latf = fabs(pos->latitude);
    double longf = fabs(pos->longitude);
    sprintf_P(latd,  PSTR("%02d%05.2f%c"), (int)latf, (latf - (int)latf) * 60, lat_sn);
    sprintf_P(longd, PSTR("%03d%05.2f%c"), (int)longf, (longf - (int)longf) * 60, long_we);
    sprintf_P(dspeed, PSTR("%03u/%03u"), pos->course, pos->speed);
    GET_PARAM(REPORT_COMMENT, comment);
    
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
    fbuf_putstr (&packet, comment);     
 
    /* Send packet */
    fbq_put(outframes, packet);
}



