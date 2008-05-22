/*
 * $Id: timer.h,v 1.5 2008-05-22 20:12:52 la7eca Exp $
 */
 
#if !defined __TIMER_H__
#define __TIMER_H__


/*
 * Software timer
 */
 
#include <inttypes.h>
#include "kernel.h"

#define NULL ((void*) 0)


/* Timer control block. An instance of this represents
 * an independent timer. Waiting is implemented by using a
 * binary semaphore. This means that a timer should not be shared
 * between multiple threads 
 */
typedef struct _timer
{
    Cond kick; 
    void (*callback)(void);
    uint16_t count; 
    struct _timer * next, * prev; 
} Timer;
 

/* Timer API */
void timer_set(Timer*, uint16_t);
void sleep(uint16_t);
void timer_cancel(Timer*);

#define timer_callback(t, cb) { (t)->callback = cb; }
#define timer_wait(t)         { wait( &(t)->kick ); }

/* Must be called periodically from a timer interrupt handler */
void timer_tick(void);

#endif
