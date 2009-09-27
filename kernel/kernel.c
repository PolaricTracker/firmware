/* 
 * $Id: kernel.c,v 1.10 2009-05-15 22:56:31 la7eca Exp $
 * Non-preemptive multithreading kernel. 
 */
 
#include "kernel.h"
#include <avr/interrupt.h>
#include "defines.h"
#include "../config.h"

static TCB root_task;          /* root task is assigned the initial thread (main) */
static TCB * q_head, * q_end;  /* Ready queue */
static TCB * fl_head;          /* Ordered list of terminated tasks which have stack space to be freed */

static uint8_t lastpid = 0, terminated = 0;
static void *stackbase;
static void *stack; 

static void _free_stack(TCB*);


uint16_t t_stackUsed()   { return (uint16_t) (stackbase - stack); }
uint8_t t_nTasks()       { return lastpid; }
uint8_t t_nTerminated()  { return terminated; }

uint8_t t_nRunning()    
{   CONTAINS_CRITICAL;
    uint8_t n = 1; 
    enter_critical();
    TCB* p = q_head; 
    if (p==NULL)
       n = 0; 
    else while (p != q_end)
       { p=p->next; n++; }
    leave_critical();
    return n; 
}


/****************************************************************************
 * Enable multithreading. stsize is the size of the stack to be 
 * allocated to the root task, i.e. the already running thread. 
 ****************************************************************************/
 
void init_kernel(uint16_t stsize) 
{       CONTAINS_CRITICAL;
        enter_critical();
        q_head = q_end = &root_task;
        q_head->next = q_head;
        fl_head = NULL;
        leave_critical(); 
        asm volatile(
            "in __tmp_reg__,  __SP_L__"  "\n\t"
            "mov %A0, __tmp_reg__"       "\n\t"
            "in __tmp_reg__,  __SP_H__"  "\n\t"
            "mov %B0, __tmp_reg__"       "\n\t" 
             : "=e" (stack) :
         );
        root_task.pid = 0;
        stackbase = stack;
        root_task.stsize = stsize;
        stack -= stsize; 
}


/****************************************************************************
 *  Create a new task and allocate a stack of the given
 *  size. Assume that a TCB exists (given
 *  as argument. 
 ****************************************************************************/

void _t_start(void (*task)(), TCB * tcb, uint16_t stsize)
{   CONTAINS_CRITICAL;       
    if (setjmp(q_head->env) == 0)
    {
        /* Put the TCB into the ready queue */
        enter_critical();
        tcb->next = q_head->next; 
        q_head->next = tcb; 
        q_end = q_head; 
        q_head = tcb; 
        tcb->stsize = stsize;
        lastpid++;
        tcb->pid = lastpid;
        leave_critical();
        
        /* Set stack pointer and call thread function */
        asm volatile(
           "mov __tmp_reg__, %A0"      "\n\t"
           "out __SP_L__, __tmp_reg__" "\n\t"
           "mov __tmp_reg__, %B0"      "\n\t"
           "out __SP_H__, __tmp_reg__" "\n\t"
           ::
            "e" (stack)
         ); 

         stack -= stsize; 
         (*task)();

        /* Thread is terminated. The last thing to do is to 
         * remove task and switch to the next entry in the 
         * ready queue 
         */
        enter_critical();
        register TCB* x = q_head;
        q_head = q_head->next;
        q_end->next = q_head;
        _free_stack(x);
        leave_critical();
        longjmp(q_head->env, 1);
    } 
}


/****************************************************************************
 * Try to free the space used on the stack by a terminated task.
 ****************************************************************************/

static void _free_stack(TCB* tcb)
{
    /* The highest task-number is on the top of the stack.
     * It can simply be freed. We also free the top of 
     * the free-list, if this is the next highest task-number. 
     */
    
    if (tcb->pid == lastpid) {
        stack += tcb->stsize;
        lastpid--;
        while (fl_head->pid == lastpid) {
            stack += fl_head->stsize;
            lastpid--;
            terminated--;
            fl_head = fl_head->next; 
        }    
    }
    else {
        /* The free-list is ordered by task-number. The highest 
         * number is first (corresponds to the top of the stack 
         */
        terminated++;
        tcb->next = NULL;
        if (fl_head == NULL) 
            fl_head = tcb;
        else {
           register TCB* p, *prev;
           p = prev = fl_head;
           while (p != NULL && p->pid > tcb->pid) {
               p = p->next; 
               prev = prev->next;
           }
           tcb->next = p;
           if (p == prev)
              fl_head = tcb;
           else
              prev->next = tcb;
        }
    }       
}



/****************************************************************************
 * Voluntarly give up the CPU. Schedule the next thread in ready queue
 ****************************************************************************/
 
void t_yield()
{   CONTAINS_CRITICAL;
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
{ 
   CONTAINS_CRITICAL;
   enter_critical();
   c->qfirst = c->qlast = NULL; 
   leave_critical();
}
   

/******************************************************************************
 * Go to sleep (until a notification is posted on c)
 ******************************************************************************/
 
void wait(Cond* c)
{   
    CONTAINS_CRITICAL;
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
{   CONTAINS_CRITICAL;
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
    while (c->qfirst != NULL)
        notify(c);
}

   
   
bool hasWaiters(Cond* c)
{
    return (c->qfirst != NULL); 
}



/**********************************************************************************
 *  Boolean condition variable
 **********************************************************************************/
 
void bcond_init(BCond* c, bool v) 
{ 
    cond_init(&(c->waiters)); 
    c->val = v;
}


void bcond_set(BCond* c)
{   CONTAINS_CRITICAL;
    enter_critical();
    if (!c->val) {
      c->val = true;
      leave_critical();
      notifyAll(&(c->waiters));
    }
    else
       leave_critical();
}


void bcond_clear(BCond* c)
   { c->val = false; }
   
   
void bcond_wait(BCond* c)
{
   if (!c->val)
       wait(&(c->waiters));
}




/**********************************************************************************
 * Initialise a semaphore
 **********************************************************************************/

void sem_init(Semaphore* s, uint16_t cnt)
   { s->cnt = cnt; cond_init(&(s->waiters)); }


/*********************************************************************************
 * Set the semaphore to any (non-negative) value
 *********************************************************************************/

void sem_set(Semaphore* s, uint16_t cnt)
   {CONTAINS_CRITICAL; enter_critical(); s->cnt = cnt; leave_critical(); }


/********************************************************************************
 * Non-blocking down-counter
 ********************************************************************************/
 
bool sem_nb_down(Semaphore* s)
{   CONTAINS_CRITICAL;
    enter_critical(); 
    if (s->cnt > 0) 
       { s->cnt--; 
        leave_critical(); 
        return true;}
    else
       { 
         leave_critical(); 
         return false; }
}


/********************************************************************************
 * Trying to decrement a semaphore which is 0, means that the
 * calling thread is put to sleep until another thread increments
 * it. This means that the thread is put into the semaphore's waiting 
 * queue.
 ********************************************************************************/
 
void sem_down(Semaphore* s)
{  CONTAINS_CRITICAL;
   enter_critical();
   while (s->cnt == 0) {
      leave_critical();
      wait(&(s->waiters));
      enter_critical();
   }
   s->cnt--; 
   leave_critical();
}


/********************************************************************************
 * Counting up a semaphore which is 0 may wake up 
 * the next thread in the semaphore's waiting-queue.  
 ********************************************************************************/

void sem_up(Semaphore* s)
{   CONTAINS_CRITICAL;
    enter_critical();
    s->cnt++;
    if (s->waiters.qfirst != NULL)
       notify(&(s->waiters));
    leave_critical();
}
