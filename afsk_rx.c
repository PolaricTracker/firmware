/*
 * $Id: afsk_rx.c,v 1.7 2008-11-22 19:05:23 la7eca Exp $
 * AFSK receiver/demodulator
 */
 
#include <stdbool.h>
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "kernel/kernel.h"
#include "afsk.h"
#include "defines.h"
#include "config.h"
#include "kernel/stream.h"
#include "transceiver.h"


#define SYMBOL_SAMPLE_INTERVAL (SCALED_F_CPU/128)/AFSK_BAUD /*  52.083 */

#ifdef SIMPLE_DETECTOR
#define TXTONE_MID = ((AFSK_TXTONE_MARK+AFSK_TXTONE_SPACE)/2)
#define CENTER_FREQUENCY (SCALED_F_CPU/64)/(TXTONE_MID*2)           /*  36.764 */
#else
#define MARK_FREQUENCY  (SCALED_F_CPU/64)/(AFSK_TXTONE_MARK*2)      /*  52.083 */
#define SPACE_FREQUENCY (SCALED_F_CPU/64)/(AFSK_TXTONE_SPACE*2)     /*  28.41 */
#define FREQUENCY_DEVIATION 10

#endif

int8_t hard_symbol; /* Most recent detected symbol. */
int8_t soft_symbol; /* Differs from the above by also having UNDECIDED as valid state */
bool valid_symbol;  /* (DCD) Will always be true when using the simple detector */

bool decoder_running = false; 
bool decoder_enabled = false;
double sqlevel; 

stream_t afsk_rx_stream;

static void _afsk_stop_decoder (void);
static void _afsk_start_decoder (void);

static void afsk_abort ()
{
    /* A sequence of seven or more consecutive ones will always
     * reset the HDLC decoder to flag sync state. 
     * If the buffer full, an abort token has already been
     * written to the stream 
     */
    stream_put_nb (&afsk_rx_stream, 0xFF);
}


stream_t* afsk_init_decoder ()
{
    STREAM_INIT(afsk_rx_stream, AFSK_DECODER_BUFFER_SIZE);
    return &afsk_rx_stream;
}


static void _afsk_start_decoder ()
{  
  decoder_running = true;
  valid_symbol = false;
  soft_symbol = hard_symbol = UNDECIDED;
    
  /* Setup Timer0 for symbol detection and sampling clock synchronization */
  clear_bit (TCCR0A, WGM00); /* \                       */
  clear_bit (TCCR0A, WGM01); /*  } Enable normal mode   */
  clear_bit (TCCR0B, WGM02); /* /                       */ 
  set_bit (TCCR0B, CS00);    /* \                       */
  set_bit (TCCR0B, CS01);    /*  } Prescaler set to 64  */
  clear_bit (TCCR0B, CS02);  /* /                       */  
  
  /* Setup Timer2 for symbol sampling at 1200 baud*/
  clear_bit (TCCR2A, WGM20); /* \                       */
  set_bit (TCCR2A, WGM21);   /*  } Enable CTC mode      */
  clear_bit (TCCR2B, WGM22); /* /                       */ 
  clear_bit (ASSR, AS2);     /* I/O clock as source     */
  set_bit (TCCR2B, CS20);    /* \                       */
  clear_bit (TCCR2B, CS21);  /*  } Prescaler set to 128 */
  set_bit (TCCR2B, CS22);    /* /                       */

  /* Enable pin change interrupt on rx data from receiver */
  make_input(TXDATA);
  set_bit (PCMSK0, PCINT2);
  set_bit (PCICR, PCIE0);
}


static void _afsk_stop_decoder ()
{  
  decoder_running = false;
  /* Disable pin change interrupt on input pin from receiver */
  clear_bit (PCICR, PCIE0);
  clear_bit (PCMSK0, PCINT2);
  
  /* Disable Timer2 interrupt */
  clear_bit (TIMSK2, OCIE2A);
  clear_port(LED2);
  afsk_abort (); // Just in case the decoder is disabled while the HDLC
                 // decoder is still in sync
}


void afsk_enable_decoder ()
   { decoder_enabled = true; 
     GET_PARAM(TRX_SQUELCH, &sqlevel); }
     
void afsk_disable_decoder ()
   { decoder_enabled = false; 
     _afsk_stop_decoder(); }


/* To be called periodically to see if there is a signal on the channel */
void afsk_check_channel ()
{
    if (!decoder_enabled)
       return;    
    if ((!decoder_running) && (adf7021_read_rssi() > sqlevel))
       _afsk_start_decoder();
    else if (decoder_running)
       _afsk_stop_decoder();   
}


/******************************************************************************
 * Interrupt routine to be called when the input signal from the              *
 * receiver changes level. Measures frequency of signal and determines        *
 * if there has been a toggle between mark and space.                         *
 ******************************************************************************/

ISR(PCINT0_vect)
{
  /* We assume for now that PCINT2 is the only enabled pin change
     interrupt, so we just go about doing decoding without further
     check of interrupt source. */

  int8_t symbol;
  uint8_t counts = TCNT0; /* Counts on Timer0 since last change */
  TCNT0 = 0;

#ifdef SIMPLE_DETECTOR
  symbol = counts >= CENTER_FREQUENCY ? MARK : SPACE;

  if (symbol != hard_symbol) {
    hard_symbol = symbol;
    /* Reset Timer2 and set compare match register to 1/2 of a bit */
    TCNT2 = 0;
    OCR2A = HARD_SYMBOL_SAMPLE_INTERVAL / 2
    set_bit (TIMSK2, OCIE2A); /* Enable Compare Match A Interrupt */
  }

  valid_symbol = true;
  soft_symbol = hard_symbol;


#else
  if (counts >= MARK_FREQUENCY - FREQUENCY_DEVIATION &&
      counts <= MARK_FREQUENCY + FREQUENCY_DEVIATION)
    symbol = MARK;
  else
    if (counts >= SPACE_FREQUENCY - FREQUENCY_DEVIATION &&
        counts <= SPACE_FREQUENCY + FREQUENCY_DEVIATION)
      symbol = SPACE;
    else
      symbol = UNDECIDED;

  if (symbol != UNDECIDED && symbol != hard_symbol)
  {
    hard_symbol = symbol;
    
    /* Reset Timer2 and set compare match register to 2/3 of a bit */
    TCNT2 = 0;
    OCR2A = (SYMBOL_SAMPLE_INTERVAL * 2) / 3;
    set_bit (TIMSK2, OCIE2A); /* Enable Compare Match A Interrupt */
  }

  if (symbol == UNDECIDED && soft_symbol == UNDECIDED)
    valid_symbol = false;
  else
    if (symbol != UNDECIDED && soft_symbol != UNDECIDED)
      valid_symbol = true;
  soft_symbol = symbol;
#endif
}



/******************************************************************************
 * Interrupt routine to be called to sample input symbols at the baud rate    *
 ******************************************************************************/

ISR(TIMER2_COMPA_vect)
{
   CONTAINS_CRITICAL;
   static int8_t prev_symbol;
   static uint8_t bit_count = 0;
   static uint8_t octet;
   enter_critical();
   if (valid_symbol) {
      set_port (LED2);

      octet = (octet >> 1) | ((hard_symbol == prev_symbol) ? 0x80 : 0x00);
      prev_symbol = hard_symbol;
      leave_critical();   // Should be safe to enable interrupts from here
     
      if (bit_count++ == 8) 
      {
         /* Always leave room for abort token */
         if (afsk_rx_stream.length.cnt < afsk_rx_stream.size) { 
            stream_put_nb (&afsk_rx_stream, octet);
         } else
            /* This should never happend, but if if it does it can only be
             * caused by the buffer being too short or a tread running too
             * long. Having some kind of log to report such errors would be
             * a good idea. 
             */
            afsk_abort();
         bit_count = 0;
      }
   } else {
      leave_critical();
      afsk_abort();
      clear_port (LED2);
   }
  
   OCR2A = SYMBOL_SAMPLE_INTERVAL;
}
