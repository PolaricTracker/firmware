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
void timer_set(Timer* t, uint16_t ticks);
void timer_wait(Timer* t);
void sleep(uint16_t ticks);


/* Must be called periodically from a timer interrupt handler */
void timer_tick();
