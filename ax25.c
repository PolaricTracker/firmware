#include "ax25.h"


#define AXLEN		7 
#define TO_POS    1
#define FROM_POS  (AXLEN + 1)
#define DIGI_POS  (AXLEN + AXLEN + 1)
#define ASCII_0    0                      // FIXME
#define ASCII_SPC  0
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



/**********************************************************************
 * Encode a new AX25 frame
 **********************************************************************/
 
void ax25_encode_frame(FBUF* b, char* from, uint8_t from_ssid, 
                                char* to, uint8_t to_ssid,
                                uint8_t ctrl,
                                uint8_t pid,
                                char* data)
{
    fbuf_new(b);
    encode_callsign(b, to, to_ssid, 0);
    encode_callsign(b, from, from_ssid, 0);
    if (pf_bit)
       ctrl |= 0x08;
    fbuf_putChar(ctrl);       
    
    if (ctrl & 0x01 == 0)            // I frame
       (ctrl & 0xEF == FTYPE_UI)     // UI frame
    {
        fbuf_putChar(b, pid);
        fbuf_putstr(b, data, data_len);
    }
 
    
}

 
      


static void encode_callsign(FBUF *b, char* c, uint8_t ssid, uint8_t flags)
{
     register uint8_t i;
     for (i=0; i<6; i++) 
     {
         if (*c != '\0' ) {
            fbuf_putChar(b, *c << 1);
            c++;
         }
         else
            fbuf_putChar(b, ASCII_SPC << 1);
     }
     fbuf_putChar(b, ((ssid & 0x0f) << 1) | (flags & 0x81) | 0x60 );
}
      
      



void ax25_addDigi ( Ax25Frame f, const char* c)
{
}


