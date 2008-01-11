/*
 * Software timer
 */
 
#include "timer.h"
#include "kernel.h"
#include <avr/interrupt.h>

/* List of running timers */
static Timer * _timers;



/********************************************************************
 * Set a timer t to expire (wake up its waiters) after the specified
 * number of ticks. 
 ********************************************************************/
 
void timer_set(Timer* t, uint16_t ticks) 
{
    sem_init(&t->kick, 0);

    enter_critical();
    t->count = ticks;
    t->prev = NULL;
    t->next = _timers;
    if (_timers != NULL) 
        _timers->prev = t;  
    _timers = t;
    leave_critical();  
}


/********************************************************************
 * Block the calling thread until the timer expire. 
 ********************************************************************/
 
void timer_wait(Timer* t) 
   { sem_down(&t->kick); }



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
        if ( --(t->count) == 0) 
        {
             /* Remove t from the list of running timers and
              * kick the waiting thread
              */
             enter_critical();
             if (t->next != NULL)
                 t->next->prev = t->prev; 
             if (t->prev != NULL) 
                 t->prev->next = t->next; 
             else { if (t->next == NULL)
                    _timers = NULL;
                 else
                    _timers = t->next; 
             }          
             leave_critical();
             sem_up(&t->kick);
        }
        t = t->next; 
    }
}

