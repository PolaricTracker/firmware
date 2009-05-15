#if !defined __HDLC_H__
#define __HDLC_H__

#include "kernel/stream.h"
#include "fbuf.h"

#define HDLC_FLAG 0x7E
#define MAX_HDLC_FRAME_SIZE 289 // including FCS field


fbq_t* hdlc_init_decoder (stream_t *);
fbq_t* hdlc_init_encoder (stream_t *);

void hdlc_monitor_rx(fbq_t*);
void hdlc_monitor_tx(fbq_t*);
void hdlc_subscribe(fbq_t*);
void hdlc_test_on(uint8_t);
void hdlc_test_off(void);
void hdlc_wait_idle(void);
bool hdlc_enc_packets_waiting(void);

/* Packet monitoring (defined in monitor.c) */
void mon_init(stream_t*);
void mon_activate(bool);

#endif /* __HDLC_H__ */
