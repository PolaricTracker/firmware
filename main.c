/*
 * $Id: main.c,v 1.10 2008-05-22 20:15:26 la7eca Exp $
 */
 
#include "defines.h"
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
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
#include "gps.h"


/* usb.c */
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
 * Handler for on/off button
 *************************************************************************/
 
Timer button_timer;
#define BUTTON_TIME 100

void onoff_handler()
{
   toggle_port(LED2);
}


ISR(INT1_vect)
{ 
    nop();
    if (!pin_is_high(BUTTON)) {
       timer_set(&button_timer, BUTTON_TIME);
       timer_callback(&button_timer, onoff_handler);
    }   
    else
       timer_cancel(&button_timer);
}



/*************************************************************************
 * Heartbeat LED blink. 
 *************************************************************************/
uint8_t blink_length, blink_interval;
 
void led1(void)
{       
    BLINK_NORMAL;
    while (1) {
        set_port( LED1 );
        sleep(blink_length);
        clear_port( LED1 );
        sleep(blink_interval);
    }
}
 
 


/**************************************************************************
 * Read and process commands on USB interface
 **************************************************************************/
       
void usbSerListener(void)
{   
    /* Wait until USB is plugged in */
    sem_down(&cdc_run);
    
    /* And wait until some character has been typed */
    getch(&cdc_instr);
    cmdProcessor(&cdc_instr, &cdc_outstr);
}



adf7021_setup_t trx_setup;

/**************************************************************************
 * Setup the adf7021 tranceiver.
 *   - We may move this to a separate source file or to config.c ?
 *   - Parts of the setup may be stored in EEPROM?
 **************************************************************************/
void setup_transceiver(void)
{
    uint32_t freq; 
    double power; 
    uint16_t dev;
    
    GET_PARAM(TRX_FREQ, &freq);
    GET_PARAM(TRX_TXPOWER, &power);
    GET_PARAM(TRX_AFSK_DEV, &dev);
    
    adf7021_setup_init(&trx_setup);
    adf7021_set_frequency (&trx_setup, freq);
    trx_setup.vco_osc.xosc_enable = true;
    trx_setup.test_mode.analog = ADF7021_ANALOG_TEST_MODE_RSSI;
    
    adf7021_set_data_rate (&trx_setup, 13200);    
    adf7021_set_modulation (&trx_setup, ADF7021_MODULATION_OVERSAMPLED_2FSK, dev);
    adf7021_set_power (&trx_setup, power, ADF7021_PA_RAMP_OFF);
    
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
    
      /* Button */
      make_input(BUTTON);
      EICRA = (1<<ISC10);
      EIMSK = (1<<INT1);
    
      /* Timer */    
      TCCR1B = 0x02                   /* Pre-scaler for timer0 */             
             | (1<<WGM12);            /* CTC mode */             
      TIMSK1 = 1<<OCIE1A;             /* Interrupt on compare match */
      OCR1A  = (SCALED_F_CPU / 8 / 9600) - 1;
   
      sei();    
      outframes =  hdlc_init_encoder( afsk_init_encoder() );  
      THREAD_START(led1, 60);  
                
      /* GPS and tracking */
      gps_init(&cdc_outstr);
      tracker_init();

      /* USB */
      usb_init();    
      THREAD_START(usbSerListener, 200);

      while(1) 
      {  
           if (t_is_idle()) {
              clear_port(CPUBUSY);
              /* Enter idle mode or sleep mode here */
              sleep_mode();
           }
           else {
              set_port(CPUBUSY);
              t_yield(); 
           }
      }     
}
