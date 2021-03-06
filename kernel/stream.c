/*
 * $Id: stream.c,v 1.5 2009-05-15 22:57:57 la7eca Exp $
 * Simple stream buffer routines (to be used with serial ports)
 */
 
#include <avr/io.h>
#include <avr/signal.h>
#include <avr/pgmspace.h>

#include "kernel/kernel.h"
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
    for (i=0; i<len-1; i++) 
    {
       x = getch(b);   
       if (marker != '\0' && x == marker)
           break;
       addr[i] = x;  
    }
    addr[i] = '\0';   
}



/********************************************************************************
 *  Blocking receive of a line (from a terminal) to a string buffer. 
 *  Returns when len characters are received or when reaching end of line. 
 *  Can use backspace to correct. Echo to terminal. 
 *
 *  NOTE: This uses a static variable and there should not be more than one 
 *  thread using this at the same time...
 ********************************************************************************/
 
bool readLine(Stream *in, Stream *out, char* addr, const uint16_t len)
{
    uint16_t i = 0;
    register char x = '\0';
    static bool wait_lf = false;
    static bool wait_cr = false;
    while ( i<len ) 
    {
       x = getch(in); 
       
       if (wait_lf || wait_cr) {
          if ((wait_lf && x == '\n') || (wait_cr && x == '\r'))
              x = getch(in);
          wait_lf = wait_cr = false;   
       }
          
       if (x == 0x03)     /* CTRL-C */
           return false;
              
       if (x == '\b' && i > 0) {
           putstr(out, "\b \b");
           i--;  
           continue;       
       }
       else if (x < ' ' && x != '\n' && x != '\r') 
           continue;  
         
       if (x == '\r' || x == '\n') {
           if (x == '\r')
              wait_lf = true;
           else 
              wait_cr = true;
           putstr(out, "\r\n"); /* Should EOLN be configurable? */
           break;
       }
       stream_sendByte(out, x); 
       addr[i++] = x;
    }
    addr[i] = '\0'; 
    return true; 
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
 
static char _stream_get(Stream* b) 
{
    CONTAINS_CRITICAL;
    enter_critical(); 
    register uint16_t i = b->index;
    if (++b->index >= b->size) 
        b->index = 0;  
    leave_critical(); 
    sem_up(&b->capacity);
    return b->buf[i];
}
    
char stream_get(Stream* b)
{   
    sem_down(&b->length);
    register char c = _stream_get(b);
    return c;
}

char stream_get_nb(Stream* b)
{   
    if (b->length.cnt==0 )
       return 0;
    
    sem_nb_down(&b->length);
    register char c = _stream_get(b);
    return c;
}



/***************************************************************************
 * Write a character to stream buffer 
 ***************************************************************************/
 
static void _stream_put(Stream* b, const char c)  
{
    CONTAINS_CRITICAL;
    enter_critical(); 
    register uint16_t i = b->index + b->length.cnt; 
    if (i >= b->size)
        i -= b->size; 
    b->buf[i] = c; 
    leave_critical();
    sem_up(&b->length);
}
 
void stream_put(Stream* b, const char c)
{  
    sem_down(&b->capacity);
    _stream_put(b, c);
}
   
void stream_put_nb(Stream* b, const char c)
{  
    if (b->capacity.cnt==0)
       return;
       
    sem_nb_down(&b->capacity);
    _stream_put(b,c);
}



