
/*
 * Digipeater
 * 
 * Stored parameters used by digipeater (can be changed in command interface)
 *    MYCALL            - my callsign
 *    DIGIPEATER_WIDE1  - true if wide1/fill-in digipeater mode. Meaning that only WIDE1 alias will be reacted on. 
 *    DIGIPEATER_SAR    - true if SAR preemption mode. If an alias SAR is found anywhere in the path, it will 
 *                        preempt others (moved first) and digipeated upon.  
 * 
 * Macros for configuration (defined in defines.h)
 *    HDLC_DECODER_QUEUE_SIZE - size (in packets) of receiving queue. Normally 7.
 *    STACK_DIGIPEATER        - size of stack for digipeater task.
 *    STACK_HLIST_TICK        - size of stack for tick_thread (for heard list).
 *   
 */

#include "kernel/kernel.h"
#include "kernel/stream.h"
#include "defines.h"
#include "config.h"
#include "ax25.h"
#include "hdlc.h"
#include "digipeater.h"
#include <util/crc16.h>
#include <string.h>
   
static bool digi_on = false;
static FBQ rxqueue;
static void digi_thread(void);

extern fbq_t* outframes; 
extern fbq_t* mon_q;

static void tick_thread(void);
static void digipeater_thread(void);
static bool duplicate_packet(addr_t* from, addr_t* to, FBUF* f, uint8_t ndigis);
static uint16_t digi_checksum(addr_t* from, addr_t* to, FBUF* f, uint8_t ndigis);
static void check_frame(FBUF *f);



void digipeater_init()
{
    FBQ_INIT(rxqueue, HDLC_DECODER_QUEUE_SIZE);
}


/***************************************************************
 * Activate the digipeater if argument is true
 * Deactivate if false
 ***************************************************************/

void digipeater_activate(bool m)
{ 
   bool tstart = m && !digi_on;
   bool tstop = !m && digi_on;
   
   digi_on = m;
   FBQ* mq = (digi_on? &rxqueue : NULL);
 
   if (tstart) {
      /* Subscribe to RX packets and start treads */
      hdlc_subscribe_rx(mq, 1);
      THREAD_START(digipeater_thread, STACK_DIGIPEATER);  
      THREAD_START(tick_thread, STACK_HLIST_TICK);
      
      /* Turn on radio and decoder */
      radio_require();
      afsk_enable_decoder();
   } 
   if (tstop) {
     /* Turn off radio and decoder */
      afsk_disable_decoder();
      radio_release();
      
      /* Unsubscribe to RX packets and stop threads */
      hdlc_subscribe_rx(NULL, 1);
      fbq_signal(&rxqueue);
   }
}


/*******************************************************************************
 * Return true if packet is heard earlier. 
 * If not, put it into the heard list. 
 *******************************************************************************/

static bool duplicate_packet(addr_t* from, addr_t* to, FBUF* f, uint8_t ndigis)
{ 
   uint16_t cs = digi_checksum(from, to, f, ndigis);
   bool hrd = hlist_exists(cs);
   if (!hrd) hlist_add(cs);
   return hrd;
}



/*********************************************************************************
 * Compute a checksum (hash) from source-callsign + destination-callsign 
 * + message. This is used to check for duplicate packets. 
 *********************************************************************************/

static uint16_t digi_checksum(addr_t* from, addr_t* to, FBUF* f, uint8_t ndigis)
{
  uint16_t crc = 0xFFFF;
  uint8_t i = 0;
  while (from->callsign[i] != 0)
    crc = _crc_ccitt_update(crc, from->callsign[i++]); 
  crc = _crc_ccitt_update(crc, from->ssid);
  i=0;
  while (to->callsign[i] != 0)
    crc = _crc_ccitt_update(crc, to->callsign[i++]);
  crc = _crc_ccitt_update(crc, to->ssid);
  fbuf_rseek(f, 14+2+ndigis*7); 
  for (i=0; i<fbuf_length(f)-2; i++)
     crc = _crc_ccitt_update(crc, fbuf_getChar(f)); 
  return crc;
}



/*******************************************************************
 * Check a frame if it is to be digipeated
 * If yes, digipeat it :)
 *******************************************************************/

static void check_frame(FBUF *f)
{
   FBUF newHdr;
   addr_t mycall, from, to; 
   addr_t digis[7], digis2[7];
   bool widedigi = false;
   uint8_t ctrl, pid;
   uint8_t i, j; 
   int8_t  sar_pos = -1;
   uint8_t ndigis =  ax25_decode_header(f, &from, &to, digis, &ctrl, &pid);

   if (duplicate_packet(&from, &to, f, ndigis))
       return;
   GET_PARAM(MYCALL, &mycall);

   /* Copy items in digi-path that has digi flag turned on, 
    * i.e. the digis that the packet has been through already 
    */
   for (i=0; i<ndigis && (digis[i].flags & FLAG_DIGI); i++) 
       digis2[i] = digis[i];   

   /* Return if it has been through all digis in path */
   if (i==ndigis)
       return;

   if (GET_BYTE_PARAM(DIGIPEATER_WIDE1) 
           && strncasecmp("WIDE1", digis[i].callsign, 5) == 0 && digis[i].ssid == 1)
       widedigi = true; 
  
   /* Look for SAR alias in the rest of the path */    
   if (GET_BYTE_PARAM(DIGIPEATER_SAR)) 
     for (j=i; j<ndigis; j++)
       if (strncasecmp("SAR", digis[j].callsign, 3) == 0) // FIXME
          { sar_pos = j; break; } 
   
   if (sar_pos < 0 && !widedigi)
      return; 

   /* Mark as digipeated through mycall */
   j = i;
   mycall.flags = FLAG_DIGI;
   digis2[j++] = mycall; 
   
   
   /* do SAR preemption if requested  */
   if (sar_pos > -1) 
       str2addr(&digis2[j++], "SAR", true);
 
   /* Otherwise, use wide digipeat method if requested and allowed */
   else if (widedigi) {
       i++;
       str2addr(&digis2[j++], "WIDE1", true);
   }

   /* Copy rest of the path, exept the SAR alias (if used) */
   for (; i<ndigis; i++) 
       if (sar_pos < 0 || i != sar_pos)
          digis2[j++] = digis[i];
   
   /* Write a new header -> newHdr */
   fbuf_new(&newHdr);
   ax25_encode_header(&newHdr, &from, &to, digis2, j, ctrl, pid);

   /* Replace header in original packet with new header. 
    * Do this non-destructively: Just add rest of existing packet to new header 
    */
   fbuf_connect(&newHdr, f, AX25_HDR_LEN(ndigis) );

   /* Send packet */
   beeps("..");
   fbq_put(outframes, newHdr);  
   
}




static void digipeater_thread()
{
    beeps("-.. ..");
    while (digi_on)
    {
        /* Wait for frame. 
         */
        FBUF frame = fbq_get(&rxqueue);
        if (fbuf_empty(&frame)) {
            fbuf_release(&frame); 
            continue;    
        }
        
        /* Do something about it */
        check_frame(&frame);
        
        /* And dispose it */
        fbuf_release(&frame);
    }
    beeps("-.. ..-.  ");
}



static void tick_thread() 
{
    while (digi_on) {
       sleep(500);
       hlist_tick();
    }
}



