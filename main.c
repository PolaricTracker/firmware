/*
 * $Id: main.c,v 1.16 2008-09-15 22:04:25 la7eca Exp $
 *
 * Polaric tracker main program.
 * Copyright (C) 2008 LA3T Tromsøgruppen av NRRL
 * Copyright (C) 2008 Øyvind Hanssen la7eca@hans.priv.no 
 * Copyright (C) 2008 Espen S Johnsen esj@cs.uit.no
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 
 * or a compatible license. See <http://www.gnu.org/licenses/>.
 */
 
 
#include "defines.h"
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <inttypes.h>
#include "kernel/kernel.h"
#include "kernel/timer.h"
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


#define soft_reset()        \
do {                        \
    wdt_enable(WDTO_15MS);  \
    for(;;)  { }            \
} while(0)



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
 
#define BUTTON_TIME 200
static Timer button_timer;
static bool is_off = false;
static void onoff_handler(void);
static void sleepmode(void);

static void onoff_handler()
{
    if (is_off) {
       is_off = false;
       soft_reset(); 
    }
    else {
       /* External devices should be turned off. Todo: USB */
       adf7021_power_off();
       clear_port(LED1);
       clear_port(LED2);
       clear_port(LED3);
       gps_off();
       is_off = true; 
    } 
    sleepmode();
}  

static void sleepmode()
{
    if (is_off)
        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    else
        set_sleep_mode(SLEEP_MODE_IDLE);        
}


ISR(INT1_vect)
{ 
    nop();
    if (!pin_is_high(BUTTON)) {
       set_sleep_mode(SLEEP_MODE_IDLE);
       timer_set(&button_timer, BUTTON_TIME);
       timer_callback(&button_timer, onoff_handler);
    }   
    else {
       timer_cancel(&button_timer);
       sleepmode();
    }
}



/*************************************************************************
 * Heartbeat LED blink. 
 *************************************************************************/
uint8_t blink_length, blink_interval;
 
void led1(void)
{       
    /* Blink both LEDS when turned on */
    set_port(LED1);
    set_port(LED2);
    sleep(100);
    clear_port(LED1);
    clear_port(LED2);
    
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
    
    adf7021_set_data_rate (&trx_setup, 4400);    
    adf7021_set_modulation (&trx_setup, ADF7021_MODULATION_OVERSAMPLED_2FSK, dev);
    adf7021_set_power (&trx_setup, power, ADF7021_PA_RAMP_OFF);
    
    adf7021_set_demodulation (&trx_setup, ADF7021_DEMOD_2FSK_LINEAR);
    trx_setup.demod.if_bw = ADF7021_DEMOD_IF_BW_12_5;
    adf7021_set_post_demod_filter (&trx_setup, 3500);
    ADF7021_INIT_REGISTER(trx_setup.test_mode, ADF7021_TEST_MODE_REGISTER);
    trx_setup.test_mode.rx = ADF7021_RX_TEST_MODE_LINEAR_SLICER_ON_TxRxDATA;

    adf7021_init (&trx_setup);
}



/**************************************************************************
 * main thread (startup)
 **************************************************************************/

int main(void) 
{
      CLKPR = (1<<7);
      CLKPR = 0;
      
      /* Disable watchdog timer */
      MCUSR = 0;
      wdt_disable();
    
      /* Start the multi-threading kernel */     
      init_kernel(60); 
      
      /* DDR Registers */
      make_output(LED1);
      make_output(LED2);
      make_output(LED3);
      make_output(CPUBUSY);
      
      /* Button */
      make_input(BUTTON);
      EICRA = (1<<ISC10);
      EIMSK = (1<<INT1);
    
      /* Timer */    
      TCCR1B = 0x02                   /* Pre-scaler for timer0 */             
             | (1<<WGM12);            /* CTC mode */             
      TIMSK1 = 1<<OCIE1A;             /* Interrupt on compare match */
      OCR1A  = (SCALED_F_CPU / 8 / 9600) - 1;
   
      TRACE_INIT;
      sei();    
                      
      /* Transceiver setup */
      setup_transceiver(); 
     
      /* HDLC and AFSK setup */
      outframes =  hdlc_init_encoder( afsk_init_encoder() );            
     
      /* GPS and tracking */
      gps_init(&cdc_outstr);
      tracker_init();

      /* USB */
      usb_init();    
      THREAD_START(usbSerListener, 200);
      
      THREAD_START(led1, 70);  
      TRACE(1);
      
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
