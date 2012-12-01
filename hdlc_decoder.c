#include <stdint.h>
#include <stdbool.h>
#include <util/crc16.h>
#include "kernel/kernel.h"
#include "kernel/stream.h"
#include "defines.h"
#include "hdlc.h"
#include "fbuf.h"
#include <avr/pgmspace.h>
#include "ui.h"
#include "config.h"
#include "ax25.h"


static stream_t *stream;
static fbuf_t fbuf;
static fbq_t* mqueue[3];
static bool monitor = false;

static void hdlc_decode (void);
static bool crc_match(FBUF*, uint8_t);


   
/***********************************************************
 * Subscribe or unsubscribe to packets from decoder
 * packets are put into the given buffer queue.
 ***********************************************************/
 
void hdlc_subscribe_rx(fbq_t* q, uint8_t i)
{
    if (i > 2)
        return;
    if (mqueue[i] != NULL)
        fbq_clear(mqueue[i]);
    mqueue[i] = q;
}



/***********************************************************
 * init hdlc-deoder
 ***********************************************************/
 
fbq_t* hdlc_init_decoder (stream_t *s)
{   
   stream = s;
   mqueue[0] = mqueue[1] = mqueue[2] = NULL;
   DEFINE_FBQ(fbin, HDLC_DECODER_QUEUE_SIZE);
   fbuf_new(&fbuf);
   THREAD_START (hdlc_decode, STACK_HDLCDECODER);

   return &fbin;
}



/***********************************************************
 * Get a bit from the demodulator layer.
 * .. or a FLAG
 ***********************************************************/
 
static uint8_t get_bit ()
{
   static uint16_t bits = 0xffff;
   static uint8_t bit_count = 0;
  
   if (bit_count < 8) {
      register uint8_t byte = getch(stream);
      bits |= ((uint16_t) byte << bit_count);
      bit_count += 8;
   }
   if ((uint8_t) (bits & 0x00ff) == HDLC_FLAG) {
      bits >>= 8;
      bit_count -= 8;
      return HDLC_FLAG;
   } 
   else {
      uint8_t bit = (uint8_t) bits & 0x0001;
      bits >>= 1;
      bit_count--;
      return bit;
  }
}




/***********************************************************
 * Main decoder thread.
 ***********************************************************/

static void hdlc_decode ()
{
   uint8_t bit;

   /* Sync to next flag */
   flag_sync:
   do {      
      bit = get_bit ();
   }  while (bit != HDLC_FLAG);

   
   /* Sync to next frame */
   frame_sync:
   do {      
      bit = get_bit ();
   }  while (bit == HDLC_FLAG);

   
   /* Receiving frame */
   uint8_t bit_count = 0;
   uint8_t ones_count = 0;
   uint16_t length = 0;
   uint8_t octet = 0;
  
   fbuf_release (&fbuf); // In case we had an abort or checksum
   fbuf_new(&fbuf);      // mismatch on the previous frame
   do {
      if (length > MAX_HDLC_FRAME_SIZE)
         goto flag_sync; // Lost termination flag or only receiving noise?

      for (bit_count = 0; bit_count < 8; bit_count++) 
      {
         octet = (bit ? 0x80 : 0x00) | (octet >> 1);
         if (bit) ones_count++;   
         else ones_count = 0; 
         if (bit == HDLC_FLAG)    // Not very likely, but could happen
            goto frame_sync;
         
         bit = get_bit ();

         if (ones_count == 5) {
            if (bit)              // Got more than five consecutive one bits,
               goto flag_sync;    // which is a certain error
            ones_count = 0;
            bit = get_bit();      // Bit stuffing - skip one bit
         }
      }
      fbuf_putChar(&fbuf, octet);
      length++;
   } while (bit != HDLC_FLAG);


   if (crc_match(&fbuf, length)) 
   {     
      /* Send packets to subscriber, if any 
       */
      if (mqueue[0] || mqueue[1] || mqueue[2]) { 
         if (mqueue[0]) fbq_put( mqueue[0], fbuf);               /* Monitor */
         if (mqueue[1]) fbq_put( mqueue[1], fbuf_newRef(&fbuf)); /* Digipeater */
         if (mqueue[2]) fbq_put( mqueue[2], fbuf_newRef(&fbuf));
      }
      else 
         fbuf_release(&fbuf); 
      fbuf_new(&fbuf);
   }
  
   goto frame_sync; // Two consecutive frames may share the same flag
}



/***********************************************************
 * CRC check
 ***********************************************************/

bool crc_match(FBUF* b, uint8_t length)
{
   /* Calculate FCS from frame content */
   uint16_t crc = 0xFFFF;
   uint8_t i;
   for (i=0; i<length-2; i++)
       crc = _crc_ccitt_update(crc, fbuf_getChar(b)); 
       
   /* Get FCS from 2 last bytes of frame */
   uint16_t rcrc; 
   fbuf_rseek(b, length-2);
   rcrc = (uint16_t) fbuf_getChar(b) ^ 0xFF;
   rcrc |= (uint16_t) (fbuf_getChar(b) ^ 0xFF) << 8;
 
   return (crc==rcrc);
}
