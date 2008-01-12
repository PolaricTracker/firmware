#if !defined __HDLC_H__
#define __HDLC_H__

#include "stream.h"
#include "fbuf.h"

#define HDLC_FLAG 0x7E
#define MAX_HDLC_FRAME_SIZE 289 // including FCS field


fqb_t* hdlc_init_decoder (stream_t *s);


#endif /* __HDLC_H__ */
