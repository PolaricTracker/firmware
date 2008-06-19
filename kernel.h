/* 
 * $Id: kernel.h,v 1.10 2008-06-19 18:35:34 la7eca Exp $
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
} TCB;


/*
 * Condition variable and Dijkstra counting semaphore. 
 * For synchronisation.
 */
typedef struct _cond {
    TCB *qfirst, *qlast;
} Cond;

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

/* This will not work unless __sreg is made a local variable, but then
   enter_critical and leave_critical always have to be called from the
   same block and 'SREG = __sreg' changed to 'SREG = __sreg & 0x80'*/
/* #define enter_critical()   uint8_t __sreg = SREG; cli(); */
/* #define leave_critical()   SREG = __sreg;  */

/* IMPORTANT: enter_critical/leave_critical must be called in ISRs to
   get the count correct, if other critical regions are going to be
   called from within the ISR */
   
uint8_t __disable_count;
#define enter_critical() { cli(); __disable_count++; }
#define leave_critical() if (--__disable_count == 0) sei ();
