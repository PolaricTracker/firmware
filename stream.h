/*
 * $Id: stream.h,v 1.9 2008-05-30 22:44:24 la7eca Exp $ 
 * Common stuff for serial communication
 */

#if !defined __STREAM_H__
#define __STREAM_H__

# include "kernel.h"



/* Type name fontified as such in Emacs */
#define stream_t Stream

typedef struct _Stream
{
    Semaphore length, capacity;
    void (*kick)(void);
    uint16_t size, index; 
    char* buf; 
} Stream;




/* API */

void   _stream_init(Stream*, char*, const uint16_t);
char   stream_get(Stream*);
void   stream_put(Stream*, const char);
void   stream_sendByte(Stream *b, const char);
char   stream_get_nb(Stream*);
void   stream_put_nb(Stream*, const char);
void   stream_sendByte_nb(Stream *b, const char);

void   putstr(Stream*, const char *);
void   putstr_P(Stream *outbuf, const char *);
void   getstr(Stream*, char*, uint16_t, char);


#define getch(s)        stream_get((s))   
#define putch(s, chr)   stream_sendByte((s), (chr));   

#define stream_empty(b)   ((b)->length.cnt == 0)
#define stream_full(b)    ((b)->capacity.cnt == 0)

#define STREAM_INIT(name,size) static char name##_charbuf[(size)];     \
                               _stream_init(&(name), (name##_charbuf), (size));
#define NOTIFY_CHAR 1
#define NOTIFY_BLOCK 2
#define ENDLINE '\r'


#endif /* __STREAM_H__ */
