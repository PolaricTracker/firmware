 
#include "kernel/kernel.h"
#include "kernel/stream.h"
#include "defines.h"
#include "config.h"
#include "digipeater.h"

 
 typedef struct _hitem {
      uint16_t val;
      uint16_t ts; 
 } HItem; 
 
 HItem hlist[HEARDLIST_SIZE];
 uint8_t hlist_next = 0;
 uint8_t hlist_length = 0;
 uint16_t time = 0; 
 
 
/***************************************************************** 
 * Must be called periodically with a fixed and known interval
 * It goes through list and removes entries that are older than
 * HEARDLIST_MAX_AGE (in ticks)
 *****************************************************************/ 

 void hlist_tick()
 {
    time++; 
    uint8_t i = hlist_next - hlist_length; 
    if (i<0) 
        i = HEARDLIST_SIZE-i; 
    
    while (i != hlist_next) {
       if (hlist[i].ts < time - HEARDLIST_MAX_AGE)
          hlist_length--;
       else
          break;
       i = (i+1) % HEARDLIST_SIZE;
    }
 }
 
 
 /**************************************************************
  * return true if x exists in list
  **************************************************************/
 
 bool hlist_exists(uint16_t x)
 {
   uint8_t i = hlist_next - hlist_length; 
   if (i<0) 
     i = HEARDLIST_SIZE-i; 
   
   while (i != hlist_next) {
     if (hlist[i].val == x)
        return true; 
     i = (i+1) % HEARDLIST_SIZE;
   } 
   return false; 
 }
 
 
 /*************************************************
  * Add an entry to the list
  *************************************************/
 void hlist_add(uint16_t x)
 {
   hlist[hlist_next].val = x; 
   hlist[hlist_next].ts = time;
   hlist_next = (hlist_next + 1) % HEARDLIST_SIZE; 
   if (hlist_length < HEARDLIST_SIZE)
     hlist_length++;
 }