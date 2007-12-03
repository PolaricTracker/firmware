#include "config.h"
#include <avr/io.h>
#include <inttypes.h>
#include "kernel.h"
#include <avr/interrupt.h>
#include "stream.h"
#include "timer.h"
#include <stdlib.h>
#include <string.h>
#include <avr/pgmspace.h>
#include "fbuf.h"
#include "afsk.h"


/***************************************************************************
 * Main clock interrupt routine. Provides clock ticks for software timers
 * (100Hz), AFSK transmitter (1200Hz) and AFSK receiver (9600Hz).  
 ***************************************************************************/ 
 
ISR(SIG_OVERFLOW0) 
{
     static uint8_t ticks; 
     static uint8_t tticks = 8; 
     
     if (transmit) 
     {
         if (tticks++ == 8) 
         {
            afsk_txBitClock();     // 1200 Hz
            tticks = 0;
         }
     }
     else 
          afsk_rxBitClock();       // 9600 Hz
        
     /* 
      * Count 96 ticks to get to a 100Hz rate
      */
     if (ticks++ == 96) { 
           timer_tick();  
           ticks = 0; 
     }   
     /*
      * Set the counter to generate a clock rate of 9600 Hz
      * Assume that the timer prescaler is set to 8. 
      */
     TCNT0 = 255 - (F_CPU / 8 / 9600);   
}




/*************************************************************************
 * Heartbeat LED blink. 
 *************************************************************************/
 
void led2()
{
    while (1) {
          PORTB |= (1 << PB1);
          sleep(10);
          PORTB  &= ~(1 << PB1);
          sleep(90);
    }
}



extern uint8_t txbytes;
extern Stream instr, outstr;

/*************************************************************************
 * Process "commands" on serial port
 *************************************************************************/
 
void serListener()
{
    static char buf[20];
    sleep(10);
    putstr_P(&outstr, PSTR("\n\rVelkommen til LA7ECA AVR test firmware\n\r"));
    while (1) {
          putstr(&outstr, "cmd: ");     
          getstr(&instr, buf, 30, '\r');
                            
          if (strncmp("rx", buf, 2) == 0)
          {
              for (;;)
              {
                  FBUF packet = fbq_get(&fbin);
                  putstr_P(&outstr, PSTR("RECV: '"));
                  fbuf_reset(&packet);
                  while (!fbuf_eof(&packet)) 
                      putch(&outstr, fbuf_getChar(&packet));
                  putstr(&outstr, "'\n\r");   
                  fbuf_release(&packet);
               }
          }
          
          
          else if (strncmp("tx", buf, 2) == 0) 
          {
               FBUF packet; 
               fbuf_new(&packet);
               fbuf_putstr_P(&packet, PSTR("123456789012345678901234567890"));            
               fbq_put(&outbuf, packet);
               afsk_startTx(); 
          }              
         
    }
}




int main(void) {
      init_kernel(60);       
      
      CLKPR = 0x80;          /* Pre-scaler on CPU freq (remember to set FUSE bits as well) */
      CLKPR = 0x01; 
      TCCR0B = 0x02;         /* Pre-scaler for timer0 = 8 */             
      TIMSK0 = 1<<TOIE0;
      TCCR2B = 0x07;         /* Pre-scaler for timer2 */
      TIMSK2 = 1<<TOIE2;
            
      /* enable PC5 as output */
      DDRB |= (1<<DDB0);
      DDRB |= (1<<DDB1);
           
      init_afsk_TX();
      init_afsk_RX(); 
      init_UART(1); 

      sei();        
      THREAD_START(led2, 70);
      THREAD_START(serListener, 70);

      while(1) 
          { t_yield(); }      /* FIXME: The MCU should be set in idle mode here */
  
}
