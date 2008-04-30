/*
 * $Id: main.c,v 1.5 2008-04-30 08:48:43 la7eca Exp $
 */
 
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
extern void nmeaProcessor(Stream*, Stream *);  /* nmea.c */
extern Semaphore cdc_run;                      /* usb.c */
extern Stream cdc_instr; 
extern Stream cdc_outstr;
extern Stream uart_instr;                      /* uart.c */

extern Semaphore nmea_run; 
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
          set_port( LED1 );
          sleep(5);
          clear_port( LED1 );
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
    nmeaProcessor(&uart_instr, &cdc_outstr);
}




adf7021_setup_t trx_setup;

/**************************************************************************
 * Setup the adf7021 tranceiver. 

 *   - We may move this to a separate source file or to config.c ?
 *   - Parts of the setup may be stored in EEPROM?
 **************************************************************************/
void setup_transceiver(void)
{
    adf7021_setup_init(&trx_setup);
    
    adf7021_set_frequency (&trx_setup, 144.800e6);
    trx_setup.vco_osc.xosc_enable = true;
    trx_setup.test_mode.analog = ADF7021_ANALOG_TEST_MODE_RSSI;
    
    adf7021_set_data_rate (&trx_setup, 1200);    
    adf7021_set_modulation (&trx_setup, ADF7021_MODULATION_OVERSAMPLED_2FSK, 2750);
    adf7021_set_power (&trx_setup, 13.0, ADF7021_PA_RAMP_OFF);
    
    adf7021_set_demodulation (&trx_setup, ADF7021_DEMOD_2FSK_LINEAR);
    trx_setup.demod.if_bw = ADF7021_DEMOD_IF_BW_12_5;
    adf7021_set_post_demod_filter (&trx_setup, 3500);
    ADF7021_INIT_REGISTER(trx_setup.test_mode, ADF7021_TEST_MODE_REGISTER);
    trx_setup.test_mode.rx = ADF7021_RX_TEST_MODE_LINEAR_SLICER_ON_TxRxDATA;


    /* Turn it on */
    adf7021_init (&trx_setup);
    adf7021_power_on ();   
}



/**************************************************************************
 * main thread (startup)
 **************************************************************************/

int main(void) 
{
      CLKPR = (1<<7);
      CLKPR = 0;
     
      /* Start the multi-threading kernel */     
      init_kernel(60); 
      
      /* DDR Registers */
      make_output(LED1);
      make_output(LED2);
      make_output(LED3);
      make_output(TXDATA);
      make_output(GPSON); 
      set_port(GPSON);
    
      /* Timer */    
      TCCR1B = 0x02                   /* Pre-scaler for timer0 */             
             | (1<<WGM12);            /* CTC mode */             
      TIMSK1 = 1<<OCIE1A;             /* Interrupt on compare match */
      OCR1A  = (SCALED_F_CPU / 8 / 9600) - 1;
   
      sei();    
      outframes =  hdlc_init_encoder( afsk_init_encoder() );  
      THREAD_START(led1, 60);  
                  
      /* GPS */
      uart_init(FALSE);
//      clear_port(GPSON); // BUG: GPS og USB spiller ikke i lag. 
      sem_init(&nmea_run, 0);
      THREAD_START(nmeaListener, 200);
      
      usb_init();    
      THREAD_START(usbSerListener, 200);
      
      while(1) 
          { USB_USBTask(); t_yield(); }     
           /* The MCU should be set in idle mode here, if possible */
}
