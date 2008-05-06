/* 
 * $Id: nmea.h,v 1.1 2008-05-06 08:57:27 la7eca Exp $
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

extern posdata_t pos;

/* API */
void  nmeaProcessor(Stream*, Stream*);
void  nmea_mon_pos(void);
void  nmea_mon_raw(void);
void  nmea_mon_off(void);
char* time2str(char*, timestamp_t);
posdata_t getPos(void);



#endif



