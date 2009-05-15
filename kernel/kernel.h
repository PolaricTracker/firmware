/* 
 * $Id: kernel.h,v 1.4 2009-05-15 22:56:47 la7eca Exp $
 * Non-preemptive multithreading kernel. 
 */

#if !defined __DEF_KERNEL_H__
#define __DEF_KERNEL_H__

#include <stdbool.h>
#include <inttypes.h>
#include <setjmp.h>
#include <stdbool.h>

#define NULL ((void*) 0)


/* 
 * Task control block.
 */
typedef struct _TCB {
    jmp_buf env;
    struct _TCB * next;
    uint16_t stackbase;    // For debugging
} TCB;


/*
 * Condition variable and Dijkstra counting semaphore. 
 * For synchronisation.
 */
typedef struct _cond {
    TCB *qfirst, *qlast;
} Cond;


typedef struct _bcond {
    bool val;
    Cond waiters;
} BCond;

    
typedef struct _sem {
    uint16_t cnt; 
    Cond waiters;
} Semaphore;


/* Kernel API */
void init_kernel(uint16_t);
void _t_start( void(*)(void) , TCB*, uint16_t); 
void t_yield(void);
bool t_is_idle(void);

void cond_init(Cond* c);
void wait(Cond* c);
void notify(Cond* c);
void notifyAll(Cond* c);
bool hasWaiters(Cond* c);

void bcond_init(BCond* c, bool v);
void bcond_set(BCond* c);
void bcond_clear(BCond* c);
void bcond_wait(BCond* c);

void sem_init(Semaphore*, uint16_t);
void sem_down(Semaphore*);
void sem_up(Semaphore*);
bool sem_nb_down(Semaphore*);

 
/*
 * Convenience macro for creating and starting threads. 
 * n is the function to be run as a separate thread. 
 * st is the stack size. 
 */
 
#define THREAD_START(n, st)  \
      static TCB __tcb_##n;    \
      _t_start(n, &__tcb_##n, (st));

#endif


/*
 * Macros for entering leaving critical regions 
 * (clear or set global interrupt flag). 
 */

#define CONTAINS_CRITICAL     register uint8_t __sreg
#define enter_critical()      __sreg  = SREG; cli() 
#define leave_critical()      SREG = __sreg


