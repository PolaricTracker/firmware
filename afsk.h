#if !defined __AFSK_H__
#define __AFSK_H__

#include <stdint.h>
#include <stdbool.h>
#include "kernel/stream.h"
#include "fbuf.h"


enum {
  UNDECIDED = -1,
  MARK,  // 1200 Hz
  SPACE  // 2200 Hz
};



stream_t* afsk_init_encoder(void);
void afsk_ptt_on(void);
void afsk_ptt_off(void);
void afsk_txBitClock(void);


stream_t* afsk_init_decoder (void);
void afsk_enable_decoder (void);
void afsk_disable_decoder (void);
bool afsk_channel_ready (uint16_t); /* ms, �s, something else? */


extern bool transmit; 
extern uint8_t dcd;

extern FBQ outbuf, fbin; 


#endif /* __AFSK_H__ */
