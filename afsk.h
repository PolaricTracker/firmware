#if !defined __AFSK_H__
#define __AFSK_H__

#include <stdint.h>
#include <stdbool.h>
#include "stream.h"
#include "fbuf.h"


enum {
  UNDECIDED = -1,
  MARK,  // 1200 Hz
  SPACE  // 2200 Hz
};



stream_t init_afsk_encoder();
void afsk_ptt_on();
void afsk_ptt_off();
void afsk_txBitClock();


stream_t* afsk_init_decoder ();
void afsk_enable_decoder ();
void afsk_disable_decoder ();
bool afsk_channel_ready (uint16_t timeout); /* ms, µs, something else? */


extern bool transmit; 
extern uint8_t dcd;

extern FBQ outbuf, fbin; 


#endif /* __AFSK_H__ */
