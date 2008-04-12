/*
 * $Id: timer.h,v 1.4 2008-04-12 18:29:47 la7eca Exp $
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
    Semaphore kick; 
    uint16_t count; 
    struct _timer * next, * prev; 
} Timer;
 

/* Timer API */
void timer_set(Timer*, uint16_t);
void timer_wait(Timer*);
void sleep(uint16_t);


/* Must be called periodically from a timer interrupt handler */
void timer_tick(void);

#endif
