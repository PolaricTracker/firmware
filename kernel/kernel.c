/* 
 * $Id: kernel.c,v 1.2 2008-09-08 22:40:27 la7eca Exp $
 * Non-preemptive multithreading kernel. 
 */
 
#include "kernel.h"
#include <avr/interrupt.h>
#include "defines.h"

static TCB root_task;         /* root task is assigned the initial thread (main) */
static TCB * q_head, * q_end; /* Ready queue */

static void *stack; 


/****************************************************************************
 * Enable multithreading. stsize is the size of the stack to be 
 * allocated to the root task, i.e. the already running thread. 
 ****************************************************************************/
 
void init_kernel(uint16_t stsize) 
{  
        enter_critical();
        q_head = q_end = &root_task;
        q_head->next = q_head;

        asm volatile(
            "in __tmp_reg__,  __SP_L__"  "\n\t"
            "mov %A0, __tmp_reg__"       "\n\t"
            "in __tmp_reg__,  __SP_H__"  "\n\t"
            "mov %B0, __tmp_reg__"       "\n\t" 
             : "=e" (stack) :
         );
        stack -= stsize; 
        leave_critical(); 
}


/****************************************************************************
 *  Create a new task and allocate a stack of the given
 *  size. Assume that a TCB exists (given
 *  as argument. 
 ****************************************************************************/

void _t_start(void (*task)(), TCB * tcb, uint16_t stsize)
{
    if (setjmp(q_head->env) == 0)
    {
        enter_critical();
        tcb->next = q_head->next; 
        q_head->next = tcb; 
        q_end = q_head; 
        q_head = tcb; 
        
        /* Set stack pointer and call thread function */
        asm volatile(
           "mov __tmp_reg__, %A0"      "\n\t"
           "out __SP_L__, __tmp_reg__" "\n\t"
           "mov __tmp_reg__, %B0"      "\n\t"
           "out __SP_H__, __tmp_reg__" "\n\t"
           ::
            "e" (stack)
         ); 
         
         leave_critical();
         stack -= stsize; 
         (*task)();
        
        /* Thread is terminated. The last thing to do is to 
         * remove task and switch to the next entry in the 
         * ready queue 
         */
        q_head = q_head->next;
        q_end->next = q_head;
        longjmp(q_head->env, 1);
    }   
}


/****************************************************************************
 * Voluntarly give up the CPU. Schedule the next thread in ready queue
 ****************************************************************************/
 
void t_yield()
{
    if (setjmp(q_head->env) == 0)
    {
        enter_critical();
        q_head = q_head->next;
        q_end = q_end->next;
        leave_critical();
        longjmp(q_head->env, 1);
    }
}
 
 
/****************************************************************************
 * Return true if there is only one thread active (system is idle)
 ****************************************************************************/
  
bool t_is_idle() 
   { return (q_head == q_end); }
 

/**********************************************************************************
 * Initialise a condition variable
 **********************************************************************************/

void cond_init(Cond* c)
   { c->qfirst = c->qlast = NULL; }
   

/******************************************************************************
 * Go to sleep (until a notification is posted on c)
 ******************************************************************************/
 
void wait(Cond* c)
{
    if (setjmp(q_head->env) == 0) 
    { 
       enter_critical();
       if (c->qfirst == NULL)
           c->qlast = c->qfirst = q_head; 
       else
           c->qlast = c->qlast->next = q_head; 

       q_end->next = q_head = q_head->next;     
       
       c->qlast->next = NULL;
       leave_critical();
       longjmp(q_head->env, 1);
    }
}


/******************************************************************************
 * notify: Wake up one of the threads waiting on c
 * notifyAll: Wake up all.
 ******************************************************************************/

void notify(Cond* c)
{
    enter_critical();
    if (c->qfirst != NULL) {
       register TCB* x = c->qfirst; 
       c->qfirst = x->next; 
       x->next = q_head;
       q_end = q_end->next = x;   
    }
    leave_critical();
}

void notifyAll(Cond* c)
{
    enter_critical();
    while (c->qfirst != NULL)
        notify(c);
    leave_critical();
}

   
/**********************************************************************************
 * Initialise a semaphore
 **********************************************************************************/

void sem_init(Semaphore* s, uint16_t cnt)
   { s->cnt = cnt; cond_init(&s->waiters); }


/*********************************************************************************
 * Set the semaphore to any (non-negative) value
 *********************************************************************************/

void sem_set(Semaphore* s, uint16_t cnt)
   { enter_critical(); s->cnt = cnt; leave_critical(); }


/********************************************************************************
 * Non-blocking down-counter
 ********************************************************************************/
 
bool sem_nb_down(Semaphore* s)
{ 
    enter_critical(); 
    if (s->cnt > 0) 
       { s->cnt--; leave_critical(); return true;}
    else
       { leave_critical(); return false; }
}


/********************************************************************************
 * Trying to decrement a semaphore which is 0, means that the
 * calling thread is put to sleep until another thread increments
 * it. This means that the thread is put into the semaphore's waiting 
 * queue.
 ********************************************************************************/
 
void sem_down(Semaphore* s)
{
   enter_critical();
   if (s->cnt == 0)
      wait(&s->waiters);
   else
      s->cnt--; 
   leave_critical();
}


/********************************************************************************
 * Counting up a semaphore which is 0 may wake up 
 * the next thread in the semaphore's waiting-queue.  
 ********************************************************************************/

void sem_up(Semaphore* s)
{
    enter_critical();
    if (s->waiters.qfirst == NULL)
       s->cnt++;
    else
       notify(&s->waiters);
    leave_critical();
}



