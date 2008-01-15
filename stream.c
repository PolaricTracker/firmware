/*
 * Simple stream buffer routines (to be used with serial ports)
 */
 
#include <avr/io.h>
#include <avr/signal.h>
#include <avr/pgmspace.h>

#include "kernel.h"
#include "stream.h"
#include "config.h"

     
/***************************************************************************
 * Initialize stream buffer b. 
 *   bdata - preallocated array of chars to be used as buffer area. 
 *   s - buffer size.
 ***************************************************************************/     
 
void _stream_init(Stream* b, char* bdata, const uint8_t s)
{
    b->buf = bdata; 
    b->size = s; 
    b->index=0; 
    sem_init(&b->mutex,1);
    sem_init(&b->length, 0);
    sem_init(&b->capacity, s-1); 
}      




/****************************************************************************
 * put character string s into stream b. Blocks if buffer is full.
 ****************************************************************************/
 
void putstr(Stream *b, const char *str)
{
     while (*str != 0)                       
        putch(b, *(str++));             
}       



/****************************************************************************
 * put character string s (in program memory) into stream b. 
 * Blocks if buffer is full.
 ****************************************************************************/
        
void putstr_P(Stream *b, const char * addr) 
{
   char c;
   while ((c = pgm_read_byte(addr)) != 0) 
   {    
      putch(b, pgm_read_byte(addr) );
      addr++;
   }    
}               




/********************************************************************************
 *  Blocking receive to a string buffer. 
 *  Returns when len characters are received or when a marker character 
 *  is received. The marker is not returned in the receive buffer. 
 ********************************************************************************/
 
void getstr(Stream *b, char* addr, const uint8_t len, const char marker)
{ 
    register uint8_t i;
    register char x;
    sem_down(&b->mutex); 

    for (i=0; i<len-1; i++) 
    {
       x = getch(b);   
       if (marker != '\0' && x == marker)
           break;
       addr[i] = x;  
    }
    addr[i] = '\0';
    sem_up(&b->mutex);
}



/* 
 * The following functions are meant to be called from driver implementations
 * with nonblock=true, or from API functions above with nonblock=false. 
 */
 

/***************************************************************************
 * Send a character (for driver implementations)
 * Puts character into buffer and kicks driver if necessary
 ***************************************************************************/
 
void _stream_sendByte(Stream *b, const char chr, const bool nonblock)
{ 
    bool was_empty = _stream_empty(b);
    _stream_put(b, chr, nonblock);
    if (was_empty && b->kick)
        (*b->kick)();
}

 
/***************************************************************************
 * Read a character from stream buffer (for driver implementations)
 ***************************************************************************/
    
char _stream_get(Stream* b, const bool nonblock)
{   
    if (nonblock && !sem_nb_down(&b->length))
       return 0;
    sem_down(&b->length);
    
    enter_critical();
    register uint8_t i = b->index;
    if (++b->index >= b->size) 
        b->index = 0; 
    leave_critical();    
    sem_up(&b->capacity);
    return b->buf[i];
}


/***************************************************************************
 * Write a character to stream buffer (for driver implementations)
 ***************************************************************************/
 
void _stream_put(Stream* b, const char c, bool nonblock)
{  
    if (nonblock && !sem_nb_down(&b->capacity))
       return;

    sem_down(&b->capacity);

    enter_critical();   
    register uint8_t i = b->index + b->length.cnt; 
    if (i >= b->size)
        i -= b->size; 
    b->buf[i] = c; 
    leave_critical();
    sem_up(&b->length);
}
   


