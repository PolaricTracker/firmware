/*
 * $Id: timer.c,v 1.1 2008-08-13 22:20:14 la7eca Exp $
 *
 * Software timer
 */
 
#include "timer.h"
#include "kernel.h"
#include <avr/interrupt.h>

/* List of running timers */
static Timer * _timers;
static void timer_remove(Timer*);


/********************************************************************
 * Set a timer t to expire (wake up its waiters) after the specified
 * number of ticks. 
 ********************************************************************/
 
void timer_set(Timer* t, uint16_t ticks) 
{
    cond_init(&t->kick);

    enter_critical();
    t->callback = NULL;
    t->count = ticks;
    t->prev = NULL;
    t->next = _timers;
    if (_timers != NULL) 
        _timers->prev = t;  
    _timers = t;
    leave_critical();  
}


/********************************************************************
 * Cancel a running timer
 *  Note that waiters (blocked threads) are notified, while callback
 *  functions are NOT called.
 ********************************************************************/
 
void timer_cancel(Timer* t)
{
    enter_critical();
    if (t->count != 0) {
       t->callback = NULL;
       t->count = 0;
       timer_remove(t);
       notifyAll(&t->kick);
    }
    leave_critical();
}


/********************************************************************
 * The calling thread will sleep for the specified number of ticks
 * (set a timer and wait for it)
 ********************************************************************/
 
void sleep(uint16_t ticks) 
{
    Timer tmr; 
    timer_set(&tmr, ticks);
    timer_wait(&tmr);
}



/********************************************************************** 
 * This function must be called periodically (e.g. at 100Hz) from a 
 * timer interrupt handler. 
 **********************************************************************/
 
void timer_tick()
{
    register Timer *t;
    for (t = _timers; t != NULL;) 
    {
        enter_critical();
        if ( --(t->count) == 0) 
        {
             /* Remove t from the list of running timers and
              * kick the waiting thread
              */
             timer_remove(t);
             notifyAll(&t->kick);
             if (t->callback != NULL)
                (*t->callback)();
        }
        leave_critical();
        t = t->next; 
    }
}


/*********************************************************
 * Remove (cancel) a running timer
 *********************************************************/
 
static void timer_remove(Timer* t)
{
     if (t->next != NULL)
         t->next->prev = t->prev; 
     if (t->prev != NULL) 
         t->prev->next = t->next; 
     else if (_timers == t) 
            _timers = t->next; 

}



