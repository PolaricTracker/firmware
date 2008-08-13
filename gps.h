/* 
 * $Id: gps.h,v 1.3 2008-08-13 22:23:20 la7eca Exp $
 * NMEA data 
 */

#if !defined __DEF_NMEA_H__
#define __DEF_NMEA_H__

#include "kernel/stream.h"

/* Timestamp: Seconds since first day of month 00:00 */
typedef uint32_t timestamp_t; 

/* Position report */
typedef struct _PosData {    
    double latitude;
    double longitude;
    timestamp_t timestamp;
} posdata_t;



/* API */

extern posdata_t current_pos;
void  gps_init (Stream*);
void  gps_mon_pos (void);
void  gps_mon_raw (void);
void  gps_mon_off (void);
bool  gps_is_locked (void);
void  gps_wait_lock (void);
char* time2str (char*, timestamp_t);
void gps_off(void);

#define gps_on() clear_port(GPSON)


#endif



