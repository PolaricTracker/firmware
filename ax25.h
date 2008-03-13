#if !defined __AX25_H__
#define __AX25_H__

#include <inttypes.h>
#include "fbuf.h"

#define ASCII_0    0x00                 
#define ASCII_SPC  0x20

#define FLAG_CMD   0x80
#define FLAG_LAST  0x01

#define FTYPE_I      0x00
#define FTYPE_RR     0x01
#define FTYPE_RNR    0x05
#define FTYPE_REJ    0x09
#define FTYPE_SABM   0x2F
#define FTYPE_SABME  0x6F
#define FTYPE_DISC   0x43
#define FTYPE_DM     0x0F

#define FTYPE_UA     0x63
#define FTYPE_FRMR   0x87
#define FTYPE_UI     0x03

#define PID_NO_L3     0xf0


typedef struct {
    char callsign[7];
    uint8_t ssid;
} addr_t;



addr_t* addr(addr_t*, char*, uint8_t); 

void ax25_encode_header( FBUF*, addr_t*, addr_t*, addr_t[], uint8_t, 
                        uint8_t, uint8_t );

#endif /* __AX25_H__ */
