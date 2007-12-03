/*
 * Buffers for frames of data.
 */ 
 
#include "config.h"
#include "fbuf.h"
#include <avr/signal.h>
#include <avr/pgmspace.h>
#include <string.h>

/***************************************************************
 * Storage for packet buffer chains
 * For each buffer slot we have 
 *    - reference count.
 *    - Length of content in bytes
 *    - Index of next buffer in chain (-1 if this is the last)
 *    - Storage for actual content
 *
 ***************************************************************/
      
uint8_t   _fbuf_refcnt[FBUF_SLOTS];
uint8_t   _fbuf_length[FBUF_SLOTS];
uint8_t   _fbuf_next  [FBUF_SLOTS];
char      _fbuf_buf   [FBUF_SLOTS][FBUF_SLOTSIZE]; 


/*
 * Note: We assume that FBUF objects are not shared between 
 * ISRs/threads. 
 * 
 * We also assume that slots are not shared between FBUF objects. 
 * That may change later. In that case, check if we need to protect
 * parts of code by disabling interrupts.
 */


/******************************************************
    Internal: Allocate a new buffer slot 
 ******************************************************/
 
static uint8_t _fbuf_newslot ()
{
   register uint8_t i; 
   cli();
   for (i=0; i<FBUF_SLOTS; i++)
       if (_fbuf_refcnt[i] == 0) 
       {
           _fbuf_refcnt[i]++;
           _fbuf_length[i] = 0;
           _fbuf_next[i] = NILPTR; 
           return i; 
       }
   sei(); 
   return NILPTR; 
}



/*******************************************************
    initialise a buffer chain
 *******************************************************/
 
void fbuf_new (FBUF* bb)
{
    bb->head = bb->wslot = bb->rslot = NILPTR; 
    bb->rpos = 0;
}


/*******************************************************
    dispose the content of a buffer chain
 *******************************************************/

void fbuf_release(FBUF* bb)
{
    cli();
    register uint8_t b = bb->head;
    while (b != NILPTR) 
    {
       _fbuf_refcnt[b]--; 
       b = _fbuf_next[b]; 
    } 
    sei();
    fbuf_new(bb);
}



/*******************************************************
    reset  or set read position of a buffer chain
 *******************************************************/
 
void fbuf_reset(FBUF* b)
{
    b->rslot = b->head; 
    b->rpos = 0;
}

void fbuf_rseek(FBUF* b, const uint8_t pos)
{
   register uint8_t i=0;
   if (pos > b->length)
       return;
   fbuf_reset(b);
   while (pos > _fbuf_length[b->rslot])
        i -= _fbuf_length[b->rslot++];
   b->rpos = i;
}



/*******************************************************
    Add a byte to a buffer chain. 
    Add new slots to it if necessary
 *******************************************************/
 
void fbuf_putChar (FBUF* b, const char c)
{
    register uint8_t pos = _fbuf_length[b->wslot]; 
    if (pos == FBUF_SLOTSIZE || b->head == NILPTR)
    {
        pos = 0; 
        register uint8_t newslot = _fbuf_newslot();
        if (newslot == NILPTR)
            return;
        b->wslot = _fbuf_next[b->wslot] = newslot; 
        if (b->head == NILPTR)
            b->rslot = b->head = newslot;
    }
    _fbuf_buf[b->wslot] [pos] =  c; 
    _fbuf_length[b->wslot]++; 
    b->length++;
}



/*******************************************************
    Write a string to a buffer chain
 *******************************************************/
 
void fbuf_write (FBUF* b, const char* data, const uint8_t size)
{
    register uint8_t i; 
    for (i=0; i<size; i++)
        fbuf_putChar(b, data[i]);
}


void fbuf_putstr(FBUF* b, const char *data)
{ 
    while (*data != 0)
        fbuf_putChar(b, *(data++));
}


void fbuf_putstr_P(FBUF *b, const char * data) 
{
   char c;
	while ((c = pgm_read_byte(data)) != 0) 
   { 	
		fbuf_putChar(b, pgm_read_byte(data) );
      data++;
   }	
}



/*******************************************************
    Read a byte from a buffer chain. 
    (this will increment the read-position)
 *******************************************************/
 
char fbuf_getChar(FBUF* b)
{
    register char x = _fbuf_buf[b->rslot][b->rpos]; 
    if (b->rpos == _fbuf_length[b->rslot]-1)
    {
        b->rslot = _fbuf_next[b->rslot];
        b->rpos = 0;
    }
    else
        b->rpos++;
    return x;          
}



/*******************************************************
    Read a string of bytes from buffer chain. 
    (this will add 'size' to the read-position)
 *******************************************************/
 
char* fbuf_read (FBUF* b, uint8_t size, char *buf)
{
    register uint8_t n, bb, r=0;
    bb = b->head; 
    while ( bb != NILPTR )
    {
       n = size - r; 
       if (n > _fbuf_length[bb]) 
           n = _fbuf_length[bb];
            
       strncpy(buf+r, _fbuf_buf[bb], n);
       r += n; 
       bb = _fbuf_next[bb];
       if (r >= size)
           break;
    }
    return buf; 
}



/* 
 *  FBQ: QUEUE OF BUFFER-CHAINS
 */   

/*******************************************************
    initialise a queue
 *******************************************************/
 
void _fbq_init(FBQ* q, void* buf, const uint8_t sz)
{
    register uint8_t i;
    cli();
    q->size = sz;
    q->buf = (FBUF*) buf;
    sem_init(&q->length, 0);
    for (i=q->index; i< q->index+q->length.cnt; i++)
        fbuf_release(&q->buf[i]);
    sei();
}



/********************************************************
    put a buffer chain into the queue
 ********************************************************/
 
void fbq_put(FBQ* q, FBUF b)
{
    cli();
    register uint8_t i = q->index + q->length.cnt; 
    if (i >= q->size)
        i -= q->size; 
    q->buf[i] = b; 
    sei();
    sem_up(&q->length);
}



/*********************************************************
    get a buffer chain from the queue (block if empty)
 *********************************************************/
 
FBUF fbq_get(FBQ* q)
{
    sem_down(&q->length); 
    cli();
    register uint8_t i = q->index;
    if (++q->index >= q->size) 
        q->index = 0; 
    return q->buf[i];
    sei();
}

// FIXME: Prevent unwanted sharing of FBUF/slots?
// FIXME: Need nonblocking version of get, to be used from ISR's. 




   
