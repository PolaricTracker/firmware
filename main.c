/*
 * $Id: main.c,v 1.29 2009-05-15 22:55:09 la7eca Exp $
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
#include "ui.h"
#include "commands.h"
#include <avr/sleep.h>

/* usb.c */
extern Semaphore cdc_run;   
extern Stream cdc_instr; 
extern Stream cdc_outstr;

fbq_t *outframes, *inframes;  



/***************************************************************************
 * Main clock interrupt routine. Provides clock ticks for software timers
 * (100Hz), AFSK transmitter (1200Hz) and AFSK receiver (9600Hz).  
 ***************************************************************************/ 

 
ISR(TIMER2_COMPA_vect) 
{
     static uint8_t ticks, txticks, rxticks; 
     sei(); /* Enable nested interrupts. MAY BE DANGEROUS???? */
     
     /*
      * count 2 ticks to get to a 1200Hz rate
      */
     if (++txticks == 2) {
         afsk_txBitClock();
         txticks = 0;
     }
      
     /* 
      * Count 96 ticks to get to a 100Hz rate
      */
     if (++ticks == 24) { 
        timer_tick();  
        ticks = 0; 
     }  
     if (++rxticks == 240) {
        rxticks = 0; 
        afsk_check_channel ();
     }
     ui_clock();
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



void reset_params()
{
    /* If version was changed */
    if ( GET_BYTE_PARAM(VERSION_KEY) < CURRENT_VERSION_KEY )
    {
        /* Some of the EEPROM parameters may need to be reset */
        SET_BYTE_PARAM(VERSION_KEY, CURRENT_VERSION_KEY);
    }
}



/**************************************************************************
 * main thread (startup)
 **************************************************************************/
extern bool is_off;
int main(void) 
{
      CLKPR = (1<<7);
      CLKPR = 0;
      
      /* Disable watchdog timer */
      MCUSR = 0;
      wdt_disable();
    
      /* Start the multi-threading kernel */     
      init_kernel(STACK_MAIN); 
      
      /* Timer */    
      TCCR2B = 0x03;                   /* Pre-scaler for timer0 */             
      TCCR2A = 1<<WGM21;               /* CTC mode */             
      TIMSK2 = 1<<OCIE2A;              /* Interrupt on compare match */
      OCR2A  = (SCALED_F_CPU / 32 / 2400) - 1;
   
      TRACE_INIT;
      sei();
      reset_params();
                            
      /* Transceiver setup */
      setup_transceiver(); 
     
      /* HDLC and AFSK setup */
      mon_init(&cdc_outstr);
      outframes = hdlc_init_encoder( afsk_init_encoder() );            
      inframes  = hdlc_init_decoder( afsk_init_decoder() );
      
      /* GPS and tracking */
      gps_init(&cdc_outstr);
      tracker_init();

      /* USB */
      usb_init();    
      THREAD_START(usbSerListener, STACK_USBLISTENER);

      ui_init();    
      TRACE(1);
      
      while(1) 
      {  
           lbeep();
           if (t_is_idle()) 
              /* Enter idle mode or sleep mode here */
              sleep_mode();
           else 
              t_yield(); 
      }     
}
