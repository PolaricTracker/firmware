#if !defined __FBUF_H__
#define __FBUF_H__

#include <inttypes.h>
#include "kernel.h"

#define NILPTR        255

/* Type names fontified as such in Emacs */
#define fbuf_t FBUF
#define fbq_t FBQ


/*********************************
   Packet buffer chain
 *********************************/
typedef struct _fb
{
   uint8_t head, wslot, rslot, rpos; 
   uint8_t length;
}
FBUF; 


/****************************************
   Operations for packet buffer chain
 ****************************************/
 
void  fbuf_new      (FBUF* b);
void  fbuf_release  (FBUF* b);
void  fbuf_reset    (FBUF* b);
void  fbuf_rseek    (FBUF* b, const uint8_t pos);
void  fbuf_putChar  (FBUF* b, const char c);
void  fbuf_write    (FBUF* b, const char* data, const uint8_t size);
void  fbuf_putstr   (FBUF* b, const char *data);
void  fbuf_putstr_P (FBUF *b, const char * data);
char  fbuf_getChar  (FBUF* b);
char* fbuf_read     (FBUF* b, uint8_t size, char *buf);


#define fbuf_eof(b) ((b)->rslot == NILPTR)



/*********************************
   Queue of packet buffer chains
 *********************************/

typedef struct _fbq
{
    uint8_t size, index; 
    Semaphore length, capacity; 
    FBUF* buf; 
} FBQ;



/************************************************
   Operations for queue of packet buffer chains
 ************************************************/
 
void  _fbq_init (FBQ* q, FBUF* buf, const uint8_t size); 
void  fbq_put (FBQ* q, FBUF b); 
FBUF  fbq_get (FBQ* q);


#define fbq_length(q) (&(q)->length.cnt)

#define DEFINE_FBQ(name,size) static FBUF name##_fbqbuf[(size)];     \
                              static FBQ (name);                   \
                              _fbq_init(&(name), (name##_fbqbuf), (size));

   
#endif /* __FBUF_H__ */
