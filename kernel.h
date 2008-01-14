/* 
 * Non-preemptive multithreading kernel. 
 */

#if !defined __DEF_KERNEL_H__
#define __DEF_KERNEL_H__

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
 * Dijkstra semaphore. For synchronisation.
 */
typedef struct _sem {
    uint16_t cnt; 
    TCB * qfirst, *qlast; 
} Semaphore;


/* Kernel API */
void init_kernel(uint8_t);
void _t_start(void(*task)() , TCB * tcb, uint8_t stsize); 
void t_yield();
void sem_init(Semaphore* s, uint8_t cnt);
void sem_down(Semaphore* s);
void sem_up(Semaphore* s);
bool sem_nb_down(Semaphore *s);

/*
 * Convenience macro for creating and starting threads. 
 * n is the function to be run as a separate thread. 
 * st is the stack size. 
 */
#define THREAD_START(n, st)  \
      static TCB __tcb_##n;    \
      _t_start(n, &__tcb_##n, (st));


#endif

#define enter_critical()   uint8_t __sreg = SREG; cli();
#define leave_critical()   SREG = __sreg; 
