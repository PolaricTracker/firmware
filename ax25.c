#include "ax25.h"


#define AXLEN		7 
#define TO_POS    1
#define FROM_POS  (AXLEN + 1)
#define DIGI_POS  (AXLEN + AXLEN + 1)

static void encode_addr(FBUF *, char*, uint8_t, uint8_t);



addr_t addr(char* c, uint8_t s) 
   { addr_t x; x.callsign=c; x.ssid=s; return x;} 
   
   
   
/**********************************************************************
 * Encode a new AX25 frame
 **********************************************************************/
 
void ax25_encode_frame(FBUF* b, addr_t from, 
                                addr_t to,
                                addr_t digis[],
                                uint8_t ctrl,
                                uint8_t pid,
                                char* data, 
                                uint8_t data_length)
{
    register uint8_t i;
    fbuf_new(b);
    encode_addr(b, to.callsign, to.ssid, 0);
    encode_addr(b, from.callsign, from.ssid, 
                  (digis[0].callsign == NULL ? FLAG_LAST : 0));
    fbuf_putChar(b, ctrl);       
    
    for (i=0; i<7; i++)
    {
        if (digis[i].callsign == NULL)
            break;
        encode_addr(b, digis[i].callsign, digis[i].ssid, 
                      (digis[i+1].callsign == NULL ? FLAG_LAST : 0));
    }
    
    if ((ctrl & 0x01) == 0  ||       // PID and data field, only if I frame
         ctrl == FTYPE_UI)           // or UI frame
    {
        fbuf_putChar(b, pid);
        fbuf_write(b, data, data_length);
    }   
}

 
      

/************************************************************************
 * Encode AX25 address field (callsign)
 ************************************************************************/
 
static void encode_addr(FBUF *b, char* c, uint8_t ssid, uint8_t flags)
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
      
      


