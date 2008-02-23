/* 
 * Common stuff for serial communication
 */

#if !defined __STREAM_H__
#define __STREAM_H__

# include "kernel.h"



/* Type name fontified as such in Emacs */
#define stream_t Stream

typedef struct _Stream
{
    Semaphore mutex, length, capacity;
    void (*kick)();
    uint16_t size, index; 
    char* buf; 
} Stream;




/* API */


  
void   _stream_init(Stream*, char*, const uint8_t);
char   _stream_get(Stream*, const bool nonblock);
void   _stream_put(Stream*, const char, const bool nonblock);
void   _stream_sendByte(Stream *b, const char chr, const bool nonblock);

void   putstr(Stream*, const char *);
void   putstr_P(Stream *outbuf, const char *);
void   getstr(Stream*, char* addr, uint8_t, char marker);


#define getch(s)        _stream_get((s), false)   
#define putch(s, chr)   _stream_sendByte((s), (chr), false);   

#define _stream_empty(b)   ((b)->length.cnt == 0)
#define _stream_full(b)    ((b)->capacity.cnt == 0)

#define STREAM_INIT(name,size) static char name##_charbuf[(size)];     \
                               _stream_init(&(name), (name##_charbuf), (size));
#define NOTIFY_CHAR 1
#define NOTIFY_BLOCK 2
#define ENDLINE '\r'


#endif /* __STREAM_H__ */
