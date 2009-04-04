/*
 * Buffers for frames of data.
 */ 
 
#include "defines.h"
#include "fbuf.h"
#include <avr/signal.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "config.h"


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
 * We also assume that slots are not shared between FBUF objects, 
 * and that FBUF objects are not accessed from interrupt handlers. 
 * That may change later. In that case, check if we need to protect
 * parts of code by disabling interrupts.
 */


/******************************************************
    Internal: Allocate a new buffer slot 
 ******************************************************/
 
static uint8_t _fbuf_newslot ()
{
   register uint8_t i; 
   for (i=0; i<FBUF_SLOTS; i++)
       if (_fbuf_refcnt[i] == 0) 
       {
           _fbuf_refcnt[i]++;
           _fbuf_length[i] = 0;
           _fbuf_next[i] = NILPTR; 
           return i; 
       }
   return NILPTR; 
}



/*******************************************************
    initialise a buffer chain
 *******************************************************/
 
void fbuf_new (FBUF* bb)
{
    bb->head = bb->wslot = bb->rslot = NILPTR; 
    bb->rpos = 0;
    bb->length = 0;
}


/*******************************************************
    dispose the content of a buffer chain
 *******************************************************/

void fbuf_release(FBUF* bb)
{
    register uint8_t b = bb->head;
    while (b != NILPTR) 
    {
       _fbuf_refcnt[b]--; 
       b = _fbuf_next[b]; 
    } 
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
   register uint8_t i=pos;
   if (pos > b->length)
       return;
   fbuf_reset(b);
   while (i > _fbuf_length[b->rslot]) {
        i -= _fbuf_length[b->rslot];
        b->rslot = _fbuf_next[b->rslot];
   }
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
 * Merge a buffer chain into another buffer chain
 *******************************************************/
 
void fbuf_merge(FBUF* b, FBUF* x, uint8_t pos)
{
    register uint8_t islot = b->head;    
    while (pos >= FBUF_SLOTSIZE) {
        pos -= _fbuf_length[islot]; 
        if (pos > 0) 
            islot = _fbuf_next[islot];
    }
    
    /* Find last slot in x chain and increment reference count*/
    register uint8_t xlast = x->head;
    _fbuf_refcnt[xlast]++;
    while (_fbuf_next[xlast] != NILPTR) {
        xlast = _fbuf_next[xlast];
        _fbuf_refcnt[xlast]++;
    }
    
    
    /* Split slot if necessary */    
    if (pos!=0) {
         register uint8_t newslot = _fbuf_newslot();
         _fbuf_next[newslot] = _fbuf_next[islot];
         _fbuf_next[islot] = newslot;
         for (uint8_t i = 0; i<FBUF_SLOTSIZE-pos; i++)
             _fbuf_buf[newslot][i] = _fbuf_buf[islot][pos+i];   
         _fbuf_length[newslot] = FBUF_SLOTSIZE - pos;
         _fbuf_length[islot] = pos;               
    }
    
    /* Insert x chain after islot */  
    _fbuf_next[xlast] = _fbuf_next[islot];
    _fbuf_next[islot] = x->head;
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



/*******************************************************
    Write a null terminated string to a buffer chain
 *******************************************************/
 
void fbuf_putstr(FBUF* b, const char *data)
{ 
    while (*data != 0)
        fbuf_putChar(b, *(data++));
}



/*******************************************************
    Write a null terminated string in program memory
    to a buffer chain
 *******************************************************/

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
       if (r >= size || r >= b->length)
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
 
void _fbq_init(FBQ* q, FBUF* buf, const uint16_t sz)
{
    q->size = sz;
    q->buf = buf;
    sem_init(&q->length, 0);
    sem_init(&q->capacity, sz);
    fbq_clear(q);
}


void fbq_clear(FBQ* q)
{
    register uint8_t i;
    for (i=q->index; i< q->index+q->length.cnt; i++)
        fbuf_release(&q->buf[i]);
}



/********************************************************
    put a buffer chain into the queue
 ********************************************************/
 
void fbq_put(FBQ* q, FBUF b)
{
    sem_down(&q->capacity); 
    register uint8_t i = q->index + q->length.cnt; 
    if (i >= q->size)
        i -= q->size; 
    q->buf[i] = b; 
    sem_up(&q->length);
}



/*********************************************************
    get a buffer chain from the queue (block if empty)
 *********************************************************/
 
FBUF fbq_get(FBQ* q)
{
    sem_down(&q->length);
    register uint8_t i = q->index;
    if (++q->index >= q->size) 
        q->index = 0; 
    sem_up(&q->capacity); 
    return q->buf[i];
}




