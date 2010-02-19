/*
 * $Id: timer.h,v 1.1 2008-08-13 22:20:14 la7eca Exp $
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
 * Cond variable. 
 */
typedef struct _timer
{
    Cond kick; 
    void (*callback)(void*);
    void *cbarg; 
    uint16_t count; 
    struct _timer * next, * prev; 
} Timer;
 
typedef void (*CBfunc) (void*);


/* Timer API */
void timer_set(Timer*, uint16_t);
void sleep(uint16_t);
void timer_cancel(Timer*);

#define timer_callback(t, cb, arg) { (t)->cbarg = arg; (t)->callback = cb; }
#define timer_wait(t)              { if ((t)->count != 0) wait( &(t)->kick ); }
#define timer_count(t)             ((t)->count)

/* Must be called periodically from a timer interrupt handler */
void timer_tick(void);

#endif
