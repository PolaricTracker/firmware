#include "ax25.h"
#include <string.h>

#define AXLEN		7 
#define TO_POS    1
#define FROM_POS  (AXLEN + 1)
#define DIGI_POS  (AXLEN + AXLEN + 1)

static void encode_addr(FBUF *, char*, uint8_t, uint8_t);



addr_t* addr(addr_t* x, char* c, uint8_t s) 
{ 
   strncpy(x->callsign, c, 7); 
   x->ssid=s; 
   return x;
} 
   
   
   
/**********************************************************************
 * Encode a new AX25 frame
 **********************************************************************/
 
void ax25_encode_header(FBUF* b, addr_t* from, 
                                 addr_t* to,
                                 addr_t digis[],
                                 uint8_t ndigis,
                                 uint8_t ctrl,
                                 uint8_t pid)
{
    register uint8_t i;
    if (ndigis > 7) ndigis = 0;
    fbuf_new(b);
    encode_addr(b, to->callsign, to->ssid, 0);
    encode_addr(b, from->callsign, from->ssid, (ndigis == 0 ? FLAG_LAST : 0));       
    
    /* Digipeater field */
    for (i=0; i<ndigis; i++)
        encode_addr(b, digis[i].callsign, digis[i].ssid, (i+1==ndigis ? FLAG_LAST : 0));

    
    fbuf_putChar(b, ctrl);           // CTRL field
    if ((ctrl & 0x01) == 0  ||       // PID (and data) field, only if I frame
         ctrl == FTYPE_UI)           // or UI frame
       fbuf_putChar(b, pid);        
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
      
      


