/* 
 * $Id: gps.h,v 1.1 2008-05-07 17:57:18 la7eca Exp $
 * NMEA data 
 */

#if !defined __DEF_NMEA_H__
#define __DEF_NMEA_H__

#include "stream.h"

/* Timestamp: Seconds since first day of month 00:00 */
typedef uint32_t timestamp_t; 

/* Position report */
typedef struct _PosData {    
    float latitude;
    float longitude;
    timestamp_t timestamp;
} posdata_t;



/* API */

void  gps_init (Stream*);
void  gps_mon_pos (void);
void  gps_mon_raw (void);
void  gps_mon_off (void);
posdata_t* gps_getPos (void);
bool  gps_is_locked (void);
void  gps_wait_lock (void);
char* time2str (char*, timestamp_t);

#endif



