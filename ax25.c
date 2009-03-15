/*
 * $Id: ax25.c,v 1.12 2009-03-15 00:13:48 la7eca Exp $
 */
 
#include "ax25.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <avr/pgmspace.h>

#define AXLEN           7 
#define TO_POS    1
#define FROM_POS  (AXLEN + 1)
#define DIGI_POS  (AXLEN + AXLEN + 1)

static void encode_addr(FBUF *, char*, uint8_t, uint8_t);
static uint8_t decode_addr(FBUF *, addr_t* );



addr_t* addr(addr_t* x, char* c, uint8_t s) 
{ 
   strncpy(x->callsign, c, 7); 
   x->ssid=s; 
   return x;
} 

   
/*************************************************************************
 * Convert string into AX.25 address field
 * Format: <callsign>-<ssid> 
 **************************************************************************/

void str2addr(addr_t* addr, const char* string)
{
   uint8_t ssid = 0;
   int i;
   for (i=0; i<7 && string[i] != 0; i++) {
      if (string[i] == '-') {
         ssid = (uint8_t) atoi( string+i+1 );
         break;
      }
      addr->callsign[i] = (char) toupper( string[i] );  
   }
   addr->callsign[i] = 0;
   addr->ssid = ssid & 0x0f; 
}



/*************************************************************************
 * Convert AX.25 address field into string
 * Format: <callsign>-<ssid> 
 **************************************************************************/

char* addr2str(char* string, const addr_t* addr)
{
    if (addr->ssid == 0)
        sprintf_P(string, PSTR("%s\0"), addr->callsign);
    else
        sprintf_P(string, PSTR("%s-%d\0"), addr->callsign, addr->ssid);
    return string;
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



/**********************************************************************
 * Decode an AX25 frame
 **********************************************************************/ 
uint8_t ax25_decode_header(FBUF* b, addr_t* from, 
                                    addr_t* to,
                                    addr_t digis[],
                                    uint8_t* ctrl,
                                    uint8_t* pid)
{
    register uint8_t i = -1;
    decode_addr(b, to);
    if (!(decode_addr(b, from) & FLAG_LAST));
       for (i=0; i<7; i++)
           if ( decode_addr(b, &digis[i]) & FLAG_LAST)   
              break;
    *ctrl = fbuf_getChar(b);
    *pid = fbuf_getChar(b);
    return i+1;
}





/************************************************************************
 * Decode AX25 address field (callsign)
 ************************************************************************/      
static uint8_t decode_addr(FBUF *b, addr_t* a)
{
    register char* c = a->callsign;
    register uint8_t x, i;
    for (i=0; i<6; i++)
    {
        x = fbuf_getChar(b);
        x >>= 1;
        if (x != ASCII_SPC)
           *(c++) = x;
    }
    *c = '\0';
    x = fbuf_getChar(b);
    a->ssid = (x & 0x0E) >> 1; 
    a->flags = x & 0x81;
    return x & 0x81;
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
      
 
      
/**************************************************************************
 * Display AX.25 frame (on output stream)
 **************************************************************************/
void ax25_display_addr(Stream* out, addr_t* a)
{
    char buf[10];
    addr2str(buf, a);
    putstr(out, buf);
}

void ax25_display_frame(Stream* out, FBUF *b)
{
    fbuf_reset(b);
    addr_t to, from;
    addr_t digis[7];
    uint8_t ctrl;
    uint8_t pid;
    uint8_t ndigis = ax25_decode_header(b, &from, &to, digis, &ctrl, &pid);
    ax25_display_addr(out, &from); 
    putstr_P(out, PSTR(">"));
    ax25_display_addr(out, &to);
    uint8_t i;
    for (i=0; i<ndigis; i++) {
       putstr_P(out, PSTR(","));
       ax25_display_addr(out, &digis[i]);
       if (digis[i].flags & FLAG_DIGI)
           putstr_P(out, PSTR("*"));
    }
    if (ctrl == FTYPE_UI)
    {
       putstr_P(out, PSTR(":"));    
       for (i=0; i < fbuf_length(b) - (14+2+ndigis*7)-2; i++)
          putch(out, fbuf_getChar(b));
    }
    else
       putstr_P(out, PSTR(" *** NON-UI FRAME ***")); 

}

