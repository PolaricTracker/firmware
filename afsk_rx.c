/*
 * AFSK receiver/demodulator
 */
 
#include <stdbool.h>
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "kernel.h"
#include "afsk.h"
#include "config.h"
#include "stream.h"


// TODO: Compute all constant automatically from CPU freq

#define SYMBOL_SAMPLE_INTERVAL 26 // (4e6/128)/1200 = 26.042

#ifdef SIMPLE_DETECTOR
#define CENTER_FREQUENCY 18 // (4e6/64)/(1700*2) = 18.382
                            // (4e6/8)/(1700*2) = 147.059
#else
#define MARK_FREQUENCY 26  // (4e6/64)/(1200*2) = 26.042
#define SPACE_FREQUENCY 14 // (4e6/64)/(2200*2) = 14.205
#define FREQUENCY_DEVIATION 1

#endif

int8_t hard_symbol; // Most recent detected symbol.
int8_t soft_symbol; // Differs from the above by also having
                    // UNDECIDED as valid state
bool valid_symbol;  // (DCD) Will always be true when using the simple detector


static stream_t afsk_rx_stream;
static uint8_t afsk_rx_buf[AFSK_DECODER_BUFFER_SIZE];

static void afsk_abort ()
{
  /* If the buffer full, an abort token has already been
     written to the stream */
  if (!_buf_full (&afsk_rx_stream)) {
    _buf_put (&afsk_rx_stream, 0xFF);
    /* A sequence of seven or more consecutive ones will always
       reset the HDLC decoder to flag sync state */
    sem_up (&afsk_rx_stream.block);
  }
}


stream_t* afsk_init_decoder ()
{
  _buf_init (&afsk_rx_stream, (char*)afsk_rx_buf, AFSK_DECODER_BUFFER_SIZE);

  return &afsk_rx_stream;
}


void afsk_enable_decoder ()
{
  valid_symbol = false;
  soft_symbol = hard_symbol = UNDECIDED;
    
  /* Setup Timer0 for symbol detection and sampling clock synchronization */
  clear_sfr_bit (TCCR0A, WGM00); /* \                       */
  clear_sfr_bit (TCCR0A, WGM01); /*  } Enable normal mode   */
  clear_sfr_bit (TCCR0B, WGM02); /* /                       */ 
  set_sfr_bit (TCCR0B, CS00);    /* \                       */
  set_sfr_bit (TCCR0B, CS01);    /*  } Prescaler set to 64  */
  clear_sfr_bit (TCCR0B, CS02);  /* /                       */  
  
  /* Setup Timer2 for symbol sampling at 1200 baud*/
  clear_sfr_bit (TCCR2A, WGM20); /* \                       */
  set_sfr_bit (TCCR2A, WGM21);   /*  } Enable CTC mode      */
  clear_sfr_bit (TCCR2B, WGM22); /* /                       */ 
  clear_sfr_bit (ASSR, AS2);     /* I/O clock as source     */
  set_sfr_bit (TCCR2B, CS20);    /* \                       */
  clear_sfr_bit (TCCR2B, CS21);  /*  } Prescaler set to 128 */
  set_sfr_bit (TCCR2B, CS22);    /* /                       */

  /* Enable pin change interrupt on rx data from receiver */
  set_sfr_bit (PCMSK0, PCINT3);
  set_sfr_bit (PCICR, PCIE0);
}


void afsk_disable_decoder ()
{
  /* Disable pin change interrupt on input pin from receiver */
  clear_sfr_bit (PCICR, PCIE0);
  clear_sfr_bit (PCMSK0, PCINT3);
  
  /* Disable Timer2 interrupt */
  clear_sfr_bit (TIMSK2, OCIE2A);

  afsk_abort (); // Just in case the decoder is disabled while the HDLC
                 // decoder is still in sync
}


bool afsk_channel_ready (uint16_t timeout)
{
  return true;
}


/******************************************************************************
 * Interrupt routine to be called when the input signal from the              *
 * receiver changes level. Measures frequency of signal and determines        *
 * if there has been a toggle between mark and space.                         *
 ******************************************************************************/

ISR(PCINT0_vect)
{
  /* We assume for now that PCINT3 is the only enabled pin change
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
    set_sfr_bit (TIMSK2, OCIE2A); /* Enable Compare Match A Interrupt */
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

  if (symbol != UNDECIDED && symbol != hard_symbol) {
    hard_symbol = symbol;
    /* Reset Timer2 and set compare match register to 2/3 of a bit */
    TCNT2 = 0;
    OCR2A = (SYMBOL_SAMPLE_INTERVAL * 2) / 3;
    set_sfr_bit (TIMSK2, OCIE2A); /* Enable Compare Match A Interrupt */
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
  static int8_t prev_symbol;
  static uint8_t bit_count = 0;
  static uint8_t octet;

  if (valid_symbol) {
    set_bit (DCD_LED);

    octet = (octet >> 1) | ((hard_symbol == prev_symbol) ? 0x80 : 0x00);
    prev_symbol = hard_symbol;
    leave_critical (); // Should be safe to enable interrupts from here
    
    if (bit_count++ == 8) {
      // Always leave room for abort token
      if (afsk_rx_stream.length < afsk_rx_stream.size) { 
	_buf_put (&afsk_rx_stream, octet);
	sem_up (&afsk_rx_stream.block);
      } else
	/* This should never happend, but if if it does it can only be
	   caused by the buffer being too short or a tread running too
	   long. Having some kind of log to report such errors would be
	   a good idea. */
	afsk_abort ();
      bit_count = 0;
    }
  } else {
    leave_critical ();
    afsk_abort ();
    clear_bit (DCD_LED);
  }
  
  OCR2A = SYMBOL_SAMPLE_INTERVAL;
}
