/* 
 * $Id: gps.h,v 1.8 2008-12-13 11:36:59 la7eca Exp $
 * NMEA data 
 */

#if !defined __DEF_NMEA_H__
#define __DEF_NMEA_H__

#include "kernel/stream.h"

/* Timestamp: Seconds since first day of month 00:00 */
typedef uint32_t timestamp_t; 

/* Position report */
typedef struct _PosData {    
    float latitude;
    float longitude;
    float  speed, altitude;
    uint16_t course;
    timestamp_t timestamp;
} posdata_t;



/* API */

extern posdata_t current_pos;
void  gps_init (Stream*);
void  gps_mon_pos (void);
void  gps_mon_raw (void);
void  gps_mon_off (void);
bool  gps_is_fixed (void);
bool  gps_wait_fix (void);
char* time2str (char*, timestamp_t);
void gps_on(void);
void gps_off(void);



#endif



