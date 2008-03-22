#include "defines.h"
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <inttypes.h>
#include "kernel.h"
#include "timer.h"
#include <stdlib.h>
#include <string.h>
#include "afsk.h"
#include "hdlc.h"
#include "usb.h"
#include "ax25.h"
#include "config.h"

extern Semaphore cdc_run;    
extern Stream cdc_instr; 
extern Stream cdc_outstr;
fbq_t* outframes;  


/***************************************************************************
 * Main clock interrupt routine. Provides clock ticks for software timers
 * (100Hz), AFSK transmitter (1200Hz) and AFSK receiver (9600Hz).  
 ***************************************************************************/ 
 
ISR(TIMER1_COMPA_vect) 
{
     static uint8_t ticks, txticks; 
     
     /*
      * count 8 ticks to get to a 1200Hz rate
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
 
void led1(void)
{
    while (1) {
          set_bit( USBKEY_LED1 );
          sleep(5);
          clear_bit( USBKEY_LED1 );
          sleep(95);
    }
}
 
 


      
/**************************************************************************
 * Read and process commands on USB interface
 **************************************************************************/
       
void serListener(void)
{   
    /* Wait until USB is plugged in */
    sem_down(&cdc_run);
    
    /* And wait until some character has been typed */
    getch(&cdc_instr);
    cmdProcessor(&cdc_instr, &cdc_outstr);
}




int main(void) {
      CLKPR = (1<<7);
      CLKPR = 1; 
      init_kernel(60); 
      DDRD |= (1<<DDD4) | (1<<DDD5) | (1<<DDD6)| (1<<DDD7);    
      DDRB |= (1<<DDB0) | (1<<DDB1) | (1<<DDB3) | (1<<DDB7); // TX Test 
    
     
      TCCR1B = 0x02          /* Pre-scaler for timer0 */             
             | (1<<WGM12);   /* CTC mode */             
      TIMSK1 = 1<<OCIE1A;    /* Interrupt on compare match */
      OCR1A  = (SCALED_F_CPU / 8 / 9600) - 1;
     
      sei();
  
      usb_init();         
      outframes = hdlc_init_encoder( afsk_init_encoder() );  

      THREAD_START(led1, 70);  
      THREAD_START(serListener, 80);
      

      while(1) 
          { USB_USBTask(); t_yield(); }     
           /* The MCU should be set in idle mode here, if possible */
  
}
