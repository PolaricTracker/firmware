/*
 * $Id: tracker.c,v 1.2 2008-06-01 21:59:08 la7eca Exp $
 */
 
#include "defines.h"
#include "kernel.h"
#include "gps.h"
#include "config.h"
#include "timer.h"
#include "math.h"


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



/***************************************************************
 * Init tracker. gps_init should be called first.
 ***************************************************************/
 
void tracker_init()
{
    sem_init(&tracker_run, 0);
    THREAD_START(trackerThread, 200);
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
       sleep(100); /* Seems to stop it from crashing (like in hdlc_encoder) */
       sem_down(&tracker_run);       
       while (GET_BYTE_PARAM(TRACKER_ON)) 
       {
           gps_on();
           sleep(200);
           gps_wait_lock();   
           
           if (should_update(&prev_pos, &current_pos))
           {
             adf7021_power_on();   
             sleep(100); 
             report_position(&current_pos);
             prev_pos = current_pos;
             sleep(100);
             adf7021_power_off(); 
           }
           gps_off();
           
           GET_PARAM(TRACKER_SLEEP_TIME, &t);
           sleep(t);       
       }
    }   
}



/****************************************************************
 * This function should return true if we have moved longer
 * than a certain threshold, changed direction more than a 
 * certain amount or at least a certain time has elapesed since
 * the previous update. 
 ****************************************************************/

static bool should_update(posdata_t* prev, posdata_t* current)
{
    /* For now, we just update each time */
    return true;
}


/**********************************************************************
 * Report position as an APRS packet
 *  Currently: Uncompressed APRS position report without timestamp 
 *  (may add more options later)
 **********************************************************************/
 
static void report_position(posdata_t* pos)
{
    FBUF packet;    
    char latd[10], longd[10];
    addr_t from, to; 
    
    /* Format latitude and longitude values */
    char lat_sn = (pos->latitude < 0 ? 'S' : 'N');
    char long_we = (pos->longitude < 0 ? 'W' : 'E');
    double latf = fabs(pos->latitude);
    double longf = fabs(pos->longitude);
    sprintf(latd, "%02d%05.2f%c", (int)latf, (latf - (int)latf) * 60, lat_sn);
    sprintf(longd, "%03d%05.2f%c", (int)longf, (longf - (int)longf) * 60, long_we);about:
 
    /* Create packet header */
    GET_PARAM(MYCALL, &from);
    GET_PARAM(DEST, &to);
    addr_t digis[] = {{"WIDE3", 3}};        
    ax25_encode_header(&packet, &from, &to, digis, 1, FTYPE_UI, PID_NO_L3);
    
    /* APRS Packet content */
    fbuf_putChar(&packet, '!');
    fbuf_putstr (&packet, latd);
    fbuf_putChar(&packet, GET_BYTE_PARAM(SYMBOL));
    fbuf_putstr (&packet, longd);
    fbuf_putChar(&packet, GET_BYTE_PARAM(SYMBOL_TABLE));    
    fbuf_putstr(&packet, "Polaric Tracker");     
 
    /* Send packet */
    fbq_put(outframes, packet);
}



