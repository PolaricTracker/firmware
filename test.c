#include "config.h"
#include <avr/io.h>
#include <inttypes.h>
#include "kernel.h"
#include <avr/interrupt.h>
#include "timer.h"
#include <stdlib.h>
#include <string.h>
#include <avr/pgmspace.h>
#include "afsk.h"
#include "hdlc.h"



/***************************************************************************
 * Main clock interrupt routine. Provides clock ticks for software timers
 * (100Hz), AFSK transmitter (1200Hz) and AFSK receiver (9600Hz).  
 ***************************************************************************/ 
 
ISR(TIMER1_COMPA_vect) 
{
     static uint8_t ticks, txticks; 
     
     /*
      * cont 8 ticks to get to a 1200Hz rate
      */
     if (++txticks == 8) {
         afsk_txBitClock();
         txticks = 0;
     }
      
     /* 
      * Count 96 ticks to get to a 100Hz rate
      */
     if (++ticks == 96) { 
        timer_tick();  
        ticks = 0; 
     }  
     
}




/*************************************************************************
 * Heartbeat LED blink. 
 *************************************************************************/
 
void led1()
{
    while (1) {
          set_bit( USBKEY_LED1 );
          sleep(5);
          clear_bit( USBKEY_LED1 );
          sleep(95);
    }
}
 




int main(void) {
      CLKPR = (1<<7);
      CLKPR = 1; 
      init_kernel(60); 
      DDRD |= (1<<DDD4) | (1<<DDD5) | (1<<DDD6)| (1<<DDD7);
    
      DDRB |= (1<<DDB0) | (1<<DDB1); // TX Test 
    
     
      TCCR1B = 0x02          /* Pre-scaler for timer0 */             
             | (1<<WGM02);   /* CTC mode */             
      TIMSK1 = 1<<OCIE1A;    /* Interrupt on compare match */
      OCR1A  = (F_CPU / 8 / 9600) - 1;
     
      sei();
      
      THREAD_START(led1, 70);  
      
      hdlc_init_encoder( afsk_init_encoder() );  
      hdlc_test_on(0x7e);

      while(1) 
          { t_yield(); }      /* FIXME: The MCU should be set in idle mode here */
  
}
