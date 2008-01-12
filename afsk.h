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



void afsk_setTxTone(unsigned char x);
void init_afsk_TX();
void afsk_startTx();
void afsk_txBitClock();


stream_t* afsk_init_decoder ();
void afsk_enable_decoder ();
void afsk_disable_decoder ();
bool afsk_channel_ready (uint16_t timeout); /* ms, µs, something else? */


extern uint8_t transmit; 
extern uint8_t dcd;

extern FBQ outbuf, fbin; 


#endif /* __AFSK_H__ */
