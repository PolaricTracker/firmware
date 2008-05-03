/*
 * $Id: stream.c,v 1.11 2008-05-03 19:41:20 la7eca Exp $
 * Simple stream buffer routines (to be used with serial ports)
 */
 
#include <avr/io.h>
#include <avr/signal.h>
#include <avr/pgmspace.h>

#include "kernel.h"
#include "stream.h"

     
/***************************************************************************
 * Initialize stream buffer b. 
 *   bdata - preallocated array of chars to be used as buffer area. 
 *   s - buffer size.
 ***************************************************************************/     
 
void _stream_init(Stream* b, char* bdata, const uint16_t s)
{
    b->buf = bdata; 
    b->size = s; 
    b->index=0; 
    sem_init(&b->mutex, 1);
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
 
void getstr(Stream *b, char* addr, const uint16_t len, const char marker)
{ 
    uint16_t i;
    char x;
//    sem_down(&b->mutex); // Vranglås på grunn av dette?  
    for (i=0; i<len; i++) 
    {
       x = getch(b);   
       if (marker != '\0' && x == marker)
           break;
       addr[i] = x;  
    }
    addr[i] = '\0';   
//    sem_up(&b->mutex);
}



/* 
 * The following functions are in two versions: One blocking to implement
 * the api, and one nonblocking to be used by driver implementations.
 */
 

/***************************************************************************
 * Send a character 
 * Puts character into buffer and kicks driver if necessary
 ***************************************************************************/
 
void stream_sendByte(Stream *b, const char chr)
{ 
    bool was_empty = stream_empty(b);
    stream_put(b, chr);
    if (was_empty && b->kick)
        (*b->kick)();
}
void stream_sendByte_nb(Stream *b, const char chr)
{ 
    bool was_empty = stream_empty(b);
    stream_put_nb(b, chr);
    if (was_empty && b->kick)
        (*b->kick)();
}
 
/***************************************************************************
 * Read a character from stream buffer 
 ***************************************************************************/
 
inline char _stream_get(Stream* b)
{
    register uint16_t i = b->index;
    if (++b->index >= b->size) 
        b->index = 0;  
    sem_up(&b->capacity);
    return b->buf[i];
}
    
char stream_get(Stream* b)
{   
    enter_critical();
    sem_down(&b->length);
    register char c = _stream_get(b);
    leave_critical();     
    return c;
}

char stream_get_nb(Stream* b)
{   
    if (&b->length.cnt==0 )
       return 0;
  
    enter_critical();
    sem_nb_down(&b->length);
    register char c = _stream_get(b);
    leave_critical();     
    return c;
}


/***************************************************************************
 * Write a character to stream buffer 
 ***************************************************************************/
 
inline void _stream_put(Stream* b, const char c)
{
    register uint16_t i = b->index + b->length.cnt; 
    if (i >= b->size)
        i -= b->size; 
    b->buf[i] = c; 
    sem_up(&b->length);
}
 
void stream_put(Stream* b, const char c)
{  
    enter_critical();  
    sem_down(&b->capacity);
    _stream_put(b, c);
    leave_critical();
}
   
void stream_put_nb(Stream* b, const char c)
{  
    if (&b->capacity==0)
       return;
       
    enter_critical();  
    sem_nb_down(&b->capacity);
    _stream_put(b,c);
    leave_critical();
}





