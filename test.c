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
#include "usb.h"
#include "ax25.h"


extern Semaphore cdc_run;    
extern Stream cdc_instr; 
extern Stream cdc_outstr;
static fbq_t* outframes;  


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
    static char buf[30];
    
    /* Wait until USB is plugged in */
    sem_down(&cdc_run);
    
    getstr(&cdc_instr, buf, 30, '\r');
    putstr_P(&cdc_outstr, PSTR("\n\rVelkommen til LA3T AVR test firmware\n\r"));
    while (1) {
         putstr(&cdc_outstr, "cmd: ");     
         getstr(&cdc_instr, buf, 30, '\r');
         
         /***************************************
          * teston <byte> : Turn on test signal
          ***************************************/
         if (strncmp("teston", buf, 6) == 0)
         {
             int ch = 0;
             hdlc_test_off();
             sleep(10);
             sscanf(buf+6, " %i", &ch);
             hdlc_test_on((uint8_t) ch);
             sprintf_P(buf, PSTR("Test signal on: 0x%X\n\r"), ch);
             putstr(&cdc_outstr, buf );  
  
         }
         
         /**********************************
          * testoff : Turn off test signal
          **********************************/
         else if (strncmp("testoff", buf, 7) == 0)
         {
             putstr_P(&cdc_outstr, PSTR("Test signal off\n\r"));  
             hdlc_test_off();  
         }
         
         /***************************
          * tx : Send test packet
          ***************************/
         else if (strncmp("tx", buf, 3) == 0)
         {
             FBUF packet; 
             fbuf_new(&packet);
             putstr_P(&cdc_outstr, PSTR("Sending 60 bytes test packet: 1234....\n\r"));    
             fbuf_putstr_P(&packet, PSTR("123456789012345678901234567890123456789012345678901234567890"));     
             fbq_put(outframes, packet);
         }
         
         /*********************************
          * tx2 : Send AX25 test packet
          *********************************/
         else if (strncmp("tx2", buf, 3) == 0)
         {
             FBUF packet;    
             addr_t digis[] = {{NULL,0}};         
             ax25_encode_frame(&packet, addr("LA7ECA",0), addr("TEST",0), digis, 
                                        FTYPE_UI, PID_NO_L3, "----------------------------------------------------------------------------------------------------------", 100);
                                        
             putstr_P(&cdc_outstr, PSTR("Sending (AX25 UI) test packet....\n\r"));        
             fbq_put(outframes, packet);
         }
    }
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
      THREAD_START(serListener, 70);
      

      while(1) 
          { USB_USBTask(); t_yield(); }     
           /* The MCU should be set in idle mode here, if possible */
  
}
