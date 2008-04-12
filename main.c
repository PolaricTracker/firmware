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
#include "transceiver.h"

extern void cmdProcessor(Stream *, Stream *);  /* commands.c */
extern void nmeaProcessor(Stream*);            /* nmea.c */
extern Semaphore cdc_run;                      /* usb.c */
extern Stream cdc_instr; 
extern Stream cdc_outstr;
extern Stream uart_instr;                      /* uart.c */

Semaphore nmea_run; 
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
          set_bit( LED1 );
          sleep(5);
          clear_bit( LED1 );
          sleep(95);
    }
}
 
 


/**************************************************************************
 * Read and process commands on USB interface
 * Read and process nmea messages from GPS
 **************************************************************************/
       
void usbSerListener(void)
{   
    /* Wait until USB is plugged in */
    sem_down(&cdc_run);
    
    /* And wait until some character has been typed */
    getch(&cdc_instr);
    cmdProcessor(&cdc_instr, &cdc_outstr);
}


void nmeaListener(void)
{
    sem_down(&nmea_run);
    nmeaProcessor(&uart_instr);
}



/**************************************************************************
 * Setup the adf7021 tranceiver. 
 *   - We may move this to a separate source file or to config.c ?
 *   - Parts of the setup may be stored in EEPROM?
 **************************************************************************/
void setup_tranceiver(void)
{
    adf7021_setup_t setup;
    adf7021_setup_init(&setup);
    adf7021_set_data_rate(&setup, 1200);
    /* More tbd... */
}



/**************************************************************************
 * main thread (startup)
 **************************************************************************/

int main(void) 
{
      /*
       * Set prescaler to get 4 Mhz clock frequency 
       * The USBKEY has an 8Mhz crystal, so we divide by 2.
       */
      CLKPR = (1<<7);
#if defined USBKEY_TEST
      CLKPR = 1; 
#else
      CLKPR = 0;
#endif
     
      /* Start the multi-threading kernel */     
      init_kernel(60); 
      
      /* DDR Registers */
      make_output(LED1);
      make_output(LED2);
      make_output(LED3);
      make_output(TXDATA);
      make_output(GPSON); 

           
//      DDRD |= (1<<DDD4) | (1<<DDD5) | (1<<DDD6)| (1<<DDD7);    
//      DDRB |= (1<<DDB0) | (1<<DDB1) | (1<<DDB3) | (1<<DDB7); // TX Test 
  
     
      /* Timer */    
      TCCR1B = 0x02                   /* Pre-scaler for timer0 */             
             | (1<<WGM12);            /* CTC mode */             
      TIMSK1 = 1<<OCIE1A;             /* Interrupt on compare match */
      OCR1A  = (SCALED_F_CPU / 8 / 9600) - 1;
   
      sei();    
      usb_init();    
      uart_init(FALSE);
      outframes = hdlc_init_encoder( afsk_init_encoder() );  

      THREAD_START(led1, 70);  
      THREAD_START(usbSerListener, 80);
     
      /* GPS */
        set_bit(GPSON);
//      sem_init(&nmea_run, 0);
//      THREAD_START(nmeaListener, 80);

      while(1) 
          { USB_USBTask(); t_yield(); }     
           /* The MCU should be set in idle mode here, if possible */
}