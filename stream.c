/*
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
 
void _buf_init(Stream* b, char* bdata, const uint8_t s)
{
    b->buf = bdata; 
    b->size = s; 
    b->index=0; 
    b->length=0; 
}      



/****************************************************************************
 * put character chr into stream b. Blocks if buffer is full.
 ****************************************************************************/
 
void	putch(Stream *b, const char chr)
{
     sem_down(&b->block);
     _sendByte(b, chr);
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



/****************************************************************************
 * get a character from stream b. Blocks until character is available. 
 ****************************************************************************/
 
char getch(Stream *b)
{
    sem_down(&b->block);
    cli(); 
    register char x = '\0'; 
    if (!_buf_empty(b)) 
        x = _buf_get(b);
    sei(); 
    return x;
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
 * The following functions are meant to be called from driver implementations. 
 */
 
/***************************************************************************
 * Read a character from stream buffer (for driver implementations)
 ***************************************************************************/
    
char _buf_get(Stream* b)
{   
    register uint8_t i = b->index;
    b->length--; 
    if (++b->index >= b->size) 
        b->index = 0; 
    return b->buf[i];
}


/***************************************************************************
 * Write a character to stream buffer (for driver implementations)
 ***************************************************************************/
 
void _buf_put(Stream* b, const char c)
{  
    register uint8_t i = b->index + b->length; 
    if (i >= b->size)
        i -= b->size; 
    b->buf[i] = c; 
    b->length++; 
}
   

/***************************************************************************
 * Send a character (for driver implementations)
 * Puts character into buffer and kicks driver if necessary
 ***************************************************************************/
 
void _sendByte(Stream *b, const char chr)
{
    cli();
    uint8_t was_empty = _buf_empty(b);
    if (!_buf_full(b)) 
        _buf_put(b, chr);
    sei();
    if (was_empty)
        (*b->kick)();
}

