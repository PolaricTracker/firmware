/* 
 * Common stuff for serial communication
 */

#if !defined __STREAM_H__
#define __STREAM_H__

/* Type name fontified as such in Emacs */
#define stream_t Stream

typedef struct _Stream
{
    Semaphore mutex, block;
    void (*kick)();
    uint8_t size, index, length; 
    char* buf; 
} Stream;



/* Init driver */

void   init_UART(unsigned char );

/* API */

void   _buf_init(Stream*, char*, const uint8_t);
void   putch(Stream*, const char );
void   putstr(Stream*, const char *);
void   putstr_P(Stream *outbuf, const char *);
char   getch(Stream* );
void   getstr(Stream*, char* addr, uint8_t, char marker);

#define DEFINE_STREAM_BUF(name,size) static char name##_charbuf[(size)];     \
                                     static Buffer (name);                   \
                                     _buf_init(&(name), (name##charbuf), (size));
#define NOTIFY_CHAR 1
#define NOTIFY_BLOCK 2
#define ENDLINE '\r'


/*
 * The remaining definitions are for driver implementations
 */

#define _buf_empty(b)      ((b)->length == 0)
#define _buf_full(b)       ((b)->length == (b)->size)

char _buf_get(Stream*);
void _buf_put(Stream*,  const char);
void _sendByte(Stream*, const char);

#endif /* __STREAM_H__ */
