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


static stream_t *out; 
static stream_t *stream;
static fbuf_t fbuf;
static fbq_t fbin;

static void hdlc_decode (void);


fbq_t* hdlc_init_decoder (stream_t *s, stream_t *outstr)
{   
   out = outstr;
   stream = s;
   DEFINE_FBQ(fbin, HDLC_DECODER_QUEUE_SIZE);
   fbuf_new(&fbuf);
   THREAD_START (hdlc_decode, 90);

   return &fbin;
}

static uint8_t get_bit ()
{
   static uint16_t bits = 0xffff;
   static uint8_t bit_count = 0;
  
   if (bit_count <= 8) {
     bits |= (uint16_t)getch (stream) << bit_count;
     bit_count += 8;
   }
   if ((bits & 0x00ff) == HDLC_FLAG) {
     bits >>= 8;
     bit_count -= 8;
     toggle_port(LED2);
     return HDLC_FLAG;
   } 
   else {
     uint8_t bit = (uint8_t) bits & 0x0001;
     bits >>= 1;
     bit_count--;
     return bit;
  }
}



static void hdlc_decode ()
{
   uint8_t bit;
   TRACE(151); 
   /* Sync to next flag */
   flag_sync:
   TRACE(152);
//   rgb_led_on(true,false,true);
   do {      
      bit = get_bit ();
   }  while (bit != HDLC_FLAG);
  
   /* Sync to next frame */
   frame_sync:
   TRACE(153);
//   rgb_led_on(true,true,false);
   do {      
      bit = get_bit ();
   }  while (bit == HDLC_FLAG);

   TRACE(154);
   /* Receiving frame */
   uint8_t bit_count = 0;
   uint8_t ones_count = 0;
   uint16_t length = 0, crc = 0;
   uint8_t octet = 0;
  
   fbuf_release (&fbuf); // In case we had an abort or checksum
                         // mismatch on the previous frame
  
   do {
      if (length > MAX_HDLC_FRAME_SIZE)
         goto flag_sync; // Lost termination flag or only receiving noise?
//      rgb_led_on(false,true,false);
      TRACE(155);
      for (bit_count = 0; bit_count < 8; bit_count++) 
      {
         octet = (bit ? 0x80 : 0x00) | (octet >> 1);
         if (bit) ones_count++;    
         bit = get_bit ();
         if (bit == HDLC_FLAG)   // Not very likely, but could happen
            goto frame_sync;
         
         if (ones_count == 5) {
            TRACE(156);
            if (bit)             // Got more than five consecutive one bits,
               goto flag_sync;   // which is a certain error
            ones_count = 0;
            bit = get_bit();     // Bit stuffing - skip one bit
         }
      }
      crc = _crc_ccitt_update (crc, octet);
      fbuf_putChar(&fbuf, octet);
      length++;
      TRACE(157);
   } while (bit != HDLC_FLAG);
   
   TRACE(158);
   rgb_led_on(false,true,false);
   if (true || crc == 0xf0b8) {
      putstr_P(out, PSTR("*** RECEIVED PACKET\r\n"));
      // fbq_put(&fbin, fbuf);       
      fbuf_new(&fbuf);
   }
  
   goto frame_sync; // Two consecutive frames may share the same flag
}
