#include <stdint.h>
#include <stdbool.h>
#include <util/crc16.h>
#include "kernel/kernel.h"
#include "kernel/stream.h"
#include "defines.h"
#include "hdlc.h"
#include "fbuf.h"

static stream_t *stream;
static fbuf_t fbuf;
static fqb_t fbin;

static void hdlc_decode ();


fbq_t* hdlc_init_decoder (stream_t *s)
{
  stream = s;
  DEFINE_FBQ(fbin, HDLC_DECODER_QUEUE_SIZE);
  fbuf_new(&fbuf);

  THREAD_START (hdlc_decode, DEFAULT_STACK_SIZE);

  return &fbin;
}


static uint8_t get_bit ()
{
  static uint16_t bits;
  static uint8_t bit_count = 0;
  
  if (bit_count <= 8) {
    bits |= (uint16_t)getch (stream) << bit_count;
    bit_count += 8;
  }
  if ((uint8_t)bits == HDLC_FLAG) {
    bits >>= 8;
    bit_count -= 8;
    return HDLC_FLAG;
  } else {
    uint8_t bit = bits & 0x01;
    bits >>= 1;
    bit_count--;
    return bit;
  }
}

static void hdlc_decode ()
{
  uint8_t bit;
    
  /* Sync to next flag */
 flag_sync:
  do {      
    bit = get_bit ();
  } while (bit != HDLC_FLAG);
  
  /* Sync to next frame */
 frame_sync:
  do {      
    bit = get_bit ();
  } while (bit == HDLC_FLAG);
  
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
    
    for (bit_count = 0; bit_count < 8; bit_count++) {
      if (bit == HDLC_FLAG) // Not very likely, but could happend
	goto frame_sync; 
      
      octet = (bit ? 0x80 : 0x00) | (octet >> 1);
      if (bit) ones_count++;	
      bit = get_bit ();
      if (ones_count == 5) {
	if (bit)          // Got more than five consecutive one bits,
	  goto flag_sync; // which is a certain error
	ones_count = 0;
      }
    }

    crc = _crc_ccitt_update (crc, octet);
    fbuf_putChar(&fbuf, octet);
    length++;
  } while (bit != HDLC_FLAG);

  if (crc == 0xf0b8) {
    fbq_put(&fbin, fbuf);       
    fbuf_new(&fbuf);
  }
  
  goto frame_sync; // Two consecutive frames may share the same flag
}
