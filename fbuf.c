/*
 * Buffers for frames of data.
 */ 

#include "defines.h"
#include "fbuf.h"
#include <avr/signal.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "config.h"
#include "kernel/kernel.h" 


/***************************************************************
 * Storage for packet buffer chains
 * For each buffer slot we have 
 *    - reference count.
 *    - Length of content in bytes
 *    - Index of next buffer in chain (NILPTR if this is the last)
 *    - Storage for actual content
 *
 ***************************************************************/
      
uint8_t   _fbuf_refcnt[FBUF_SLOTS];
uint8_t   _fbuf_length[FBUF_SLOTS];
uint8_t   _fbuf_next  [FBUF_SLOTS];
char      _fbuf_buf   [FBUF_SLOTS][FBUF_SLOTSIZE]; 

static uint8_t _free_slots = FBUF_SLOTS; 

static void(*memFullError)(void) = NULL;

static uint8_t _split(uint8_t islot, uint8_t pos);

uint8_t fbuf_freeSlots()
   { return _free_slots; }

   
   
/*
 * Note: We assume that FBUF objects are not shared between 
 * ISRs/threads. 
 * 
 * We also assume that FBUF objects are not accessed from interrupt handlers. 
 * That may change later. In that case, check if we need to protect
 * parts of code by disabling interrupts. slots may be shared between
 * FBUF objects but it is problematic and this should be tested and 
 * analysed more. We should disallow writing to a FBUF that contains
 * shared slots.
 */


void fbuf_errorHandler( void(*f)(void) ) 
  { memFullError = f; }



/******************************************************
    Internal: Allocate a new buffer slot 
 ******************************************************/
 
static uint8_t _fbuf_newslot ()
{
   register uint8_t i; 
   for (i=0; i<FBUF_SLOTS; i++)
       if (_fbuf_refcnt[i] == 0) 
       {
           _fbuf_refcnt[i] = 1;
           _fbuf_length[i] = 0;
           _fbuf_next[i] = NILPTR; 
           _free_slots--;
           return i; 
       }
   return NILPTR; 
}


/*******************************************************
    initialise a buffer chain
 *******************************************************/
 
void fbuf_new (FBUF* bb)
{
    bb->head = bb->wslot = bb->rslot = _fbuf_newslot();
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
       if (_fbuf_refcnt[b] > 0) {
          _fbuf_refcnt[b]--;
           if (_fbuf_refcnt[b] == 0)
             _free_slots++;
       }
       b = _fbuf_next[b]; 
    } 
    bb->head = bb->wslot = bb->rslot = NILPTR;
    bb->rpos = bb->length = 0;
}


/*******************************************************
 *   Create a new reference to a buffer chain
 *******************************************************/

FBUF fbuf_newRef(FBUF* bb)
{
  FBUF newb;
  register uint8_t b = bb->head;
  while (b != NILPTR) 
  {
    _fbuf_refcnt[b]++; 
    b = _fbuf_next[b]; 
  } 
  newb.head = bb->head; 
  newb.length = bb->length; 
  fbuf_reset(&newb);
  newb.wslot = bb->wslot;
  return newb;
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
    /* if wslot is NIL it means that writing is not allowed */
    if (b->wslot == NILPTR)
       return;
    
    register uint8_t pos = _fbuf_length[b->wslot]; 
    if (pos == FBUF_SLOTSIZE || b->head == NILPTR)
    {
        pos = 0; 
        register uint8_t newslot = _fbuf_newslot();
        if (newslot == NILPTR) {
            if (memFullError != NULL)
               (*memFullError)();
            else
               return;
        }  
        b->wslot = _fbuf_next[b->wslot] = newslot; 
        if (b->head == NILPTR)
            b->rslot = b->head = newslot;
    }
    _fbuf_buf[b->wslot] [pos] =  c; 
    _fbuf_length[b->wslot]++; 
    b->length++;
}


    

/*******************************************************
 * Insert a buffer chain x into another buffer chain b 
 * at position pos
 * 
 * Note: After calling this, writing into the buffers
 * are disallowed.
 *******************************************************/
 
void fbuf_insert(FBUF* b, FBUF* x, uint8_t pos)
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
    
    /* Insert x chain after islot */  
    _fbuf_next[xlast] = _split(islot, pos); 
    _fbuf_next[islot] = x->head;
    
    b->wslot = x->wslot = NILPTR; // Disallow writing
    b->length += x->length;
}


/*****************************************************
 * Connect b buffer to x, at position pos. In practice
 * this mean that we get two buffers, with different
 * headers but with a shared last part. 
 * 
 * Note: After calling this, writing into the buffers
 * is not allowed. 
 *****************************************************/

void fbuf_connect(FBUF* b, FBUF* x, uint8_t pos)
{
    register uint8_t islot = x->head;  
    register uint8_t p = pos;
    while (p >= FBUF_SLOTSIZE) {
        p -= _fbuf_length[islot]; 
        if (p > 0) 
            islot = _fbuf_next[islot];
    }

    /* Find last slot of b and connect it to rest of x */
    register uint8_t xlast = b->head;
    while (_fbuf_next[xlast] != NILPTR) 
        xlast = _fbuf_next[xlast];

    _fbuf_next[xlast] = _split(islot, p);

    /* Increment reference count of rest of x */
    while (_fbuf_next[xlast] != NILPTR) {
        xlast = _fbuf_next[xlast];
        _fbuf_refcnt[xlast]++;
    }

    b->wslot = x->wslot = NILPTR; // Disallow writing
    b->length = b->length + x->length - pos;
}



static uint8_t _split(uint8_t islot, uint8_t pos)
{
      if (pos == 0)
          return _fbuf_next[islot];
      register uint8_t newslot = _fbuf_newslot();
      _fbuf_next[newslot] = _fbuf_next[islot];
      _fbuf_next[islot] = newslot;
      _fbuf_refcnt[newslot] = _fbuf_refcnt[islot]; 
      
      /* Copy last part of slot to newslot */
      for (uint8_t i = 0; i<_fbuf_length[islot]-pos; i++)
          _fbuf_buf[newslot][i] = _fbuf_buf[islot][pos+i];   

      _fbuf_length[newslot] = _fbuf_length[islot] - pos;
      _fbuf_length[islot] = pos; 
      return newslot;
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
    if (b->length < size)
       size = b->length; 
    bb = b->head; 
    while ( bb != NILPTR )
    {
       n = size - r; 
       if (n > _fbuf_length[bb]) 
           n = _fbuf_length[bb];
            
       strncpy(buf+r, _fbuf_buf[bb], n);
       r += n; 
       bb = _fbuf_next[bb];
       if (r >= size ) 
           break;
    }
    buf[r] = '\0';
    return buf; 
}


void fbuf_removeLast(FBUF* x)
{
  register uint8_t xlast = x->head;
  register uint8_t prev = xlast;
  
  while (_fbuf_next[xlast] != NILPTR) {
    xlast = _fbuf_next[xlast];
    if (_fbuf_next[xlast] != NILPTR)
      prev = xlast;
  }
  
  _fbuf_length[xlast]--;
  if (_fbuf_length[xlast] == 0 && prev != xlast) {
    _fbuf_refcnt[xlast]--;
    if (_fbuf_refcnt[xlast] == 0)
      _free_slots++;
    _fbuf_next[prev] = NILPTR;
  }
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
    q->index = 0;
    sem_init(&q->length, 0);
    sem_init(&q->capacity, sz);
}


void fbq_clear(FBQ* q)
{
    register uint16_t i;
    for (i = q->index;  i < q->index + q->length.cnt;  i++)
        fbuf_release(&q->buf[(uint8_t) (i % q->size)]);
    sem_set(&q->length, 0);
    sem_set(&q->capacity, q->size);    
    q->index = 0;
}



/********************************************************
    put a buffer chain into the queue
 ********************************************************/
 
void fbq_put(FBQ* q, FBUF b)
{
    sem_down(&q->capacity); 
    register uint16_t i = q->index + q->length.cnt;
    if (i >= q->size)
        i -= q->size; 
    q->buf[(uint8_t) i] = b; 
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



/**********************************************************
 * put an empty buffer onto the queue. 
 **********************************************************/
 
void fbq_signal(FBQ* q)
{
   FBUF b; 
   fbuf_new(&b);
   fbq_put(q, b);
}

