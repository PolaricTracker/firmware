/*
 * $Id: afsk_rx.c,v 1.12 2009-05-15 22:47:08 la7eca Exp $
 * AFSK receiver/demodulator
 */
 
#include <stdbool.h>
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include "kernel/kernel.h"
#include "afsk.h"
#include "defines.h"
#include "config.h"
#include "kernel/stream.h"
#include "transceiver.h"


#define SYMBOL_SAMPLE_INTERVAL ((SCALED_F_CPU/8/AFSK_BAUD))       
#define SYMBOL_SAMPLE_RESYNC   ((SCALED_F_CPU/8/AFSK_BAUD))/2

#define TXTONE_MID ((AFSK_TXTONE_MARK+AFSK_TXTONE_SPACE)/2)
#define CENTER_FREQUENCY ((SCALED_F_CPU/64)/(TXTONE_MID*2)-1)           /*  36.764 */
#define MARK_FREQUENCY  ((SCALED_F_CPU/64)/(AFSK_TXTONE_MARK*2)-1)      /*  52.083 */
#define SPACE_FREQUENCY ((SCALED_F_CPU/64)/(AFSK_TXTONE_SPACE*2)-1)     /*  28.41 */
#define FREQUENCY_DEVIATION 24


int8_t hard_symbol;    /* Most recent detected symbol. */
int8_t soft_symbol;    /* Differs from the above by also having UNDECIDED as valid state */
bool valid_symbol;     /* (DCD) Will always be true when using the simple detector */
extern bool transmit;  /* True when transmitter(modulator) is active (see afsk_tx.c). */
extern BCond mon_ok;   /* since activity on USB/serial port disturbs demodulation,
                          there must be synchronisation to prevent those two things
                          from running at the same time */
 
bool decoder_running = false; 
bool decoder_enabled = false;
double sqlevel; 

stream_t afsk_rx_stream;

static void _afsk_stop_decoder (void);
static void _afsk_start_decoder (void);
static void _afsk_abort (void);
static void sample_bits(void);
static void add_bit(bool);


/************************************************************
 * Init afsk decoder 
 ************************************************************/
 
stream_t* afsk_init_decoder ()
{
    STREAM_INIT(afsk_rx_stream, AFSK_DECODER_BUFFER_SIZE);
    return &afsk_rx_stream;
}



/****************************************************************************
 * Start the decoding of a signal. 
 ****************************************************************************/
 
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
  
  /* Setup Timer1 for symbol sampling at 1200 baud*/
  clear_bit (TCCR1A, WGM10); /* \                       */
  clear_bit (TCCR1A, WGM11); /*  } Enable normal mode   */
  clear_bit (TCCR1B, WGM12); /* /                       */ 
 //  clear_bit (ASSR, AS2);   /* I/O clock as source     */
  clear_bit (TCCR1B, CS10);  /* \                       */
  set_bit (TCCR1B, CS11);    /*  } Prescaler set to 8   */
  clear_bit (TCCR1B, CS12);  /* /                       */

  /* Enable pin change interrupt on rx data from receiver */
  make_input(TXDATA);
  set_bit (PCMSK0, PCINT2);
  set_bit (PCICR, PCIE0);

  pri_rgb_led_on(true,true,false);
  bcond_clear(&mon_ok);
}


/****************************************************************************
 * Stop the decoding (typically used when a signal drops). 
 ****************************************************************************/
 
static void _afsk_stop_decoder ()
{  
  bcond_set(&mon_ok);
  decoder_running = false;
  /* Disable pin change interrupt on input pin from receiver */
  clear_bit (PCICR, PCIE0);
  clear_bit (PCMSK0, PCINT2);
  
  pri_rgb_led_off();
  _afsk_abort (); // Just in case the decoder is disabled while the HDLC
                  // decoder is still in sync
}



/****************************************************************************
 * Enable/disable decoder. I.e. start/stop listening to radio receiver 
 * to see if there is a signal. If this is enabled, the AFSK decoder will
 * be activated when incoming signal is stronger than squelch level. 
 ****************************************************************************/
 
void afsk_enable_decoder ()
   { decoder_enabled = true; 
     GET_PARAM(TRX_SQUELCH, &sqlevel); }
     
void afsk_disable_decoder ()
   { decoder_enabled = false; 
     _afsk_stop_decoder(); }




/***************************************************************************
 * To be called periodically to see if there is a signal on the channel    *
 ***************************************************************************/
 
void afsk_check_channel ()
{
    if (!decoder_enabled || transmit)
       return;    
    if ((!decoder_running) && (adf7021_read_rssi() > sqlevel))
        _afsk_start_decoder();
    else if (decoder_running && (adf7021_read_rssi() <= sqlevel))
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
  set_sleep_mode(SLEEP_MODE_IDLE);

  symbol = UNDECIDED;
  if (counts > CENTER_FREQUENCY &&
       counts < MARK_FREQUENCY + FREQUENCY_DEVIATION  )
     symbol = MARK;
  else 
    if (counts > SPACE_FREQUENCY - FREQUENCY_DEVIATION && 
         counts < CENTER_FREQUENCY )
     symbol = SPACE;

  /* If toggle between mark and space */
  if (symbol != UNDECIDED && symbol != soft_symbol)
      sample_bits();
  
  if (symbol == UNDECIDED && soft_symbol == UNDECIDED) {
     if (valid_symbol) 
         _afsk_abort();
     valid_symbol = false;
  }
  else
    if (symbol != UNDECIDED && soft_symbol != UNDECIDED)
        valid_symbol = true;
  soft_symbol = symbol;
}


/*******************************************************************************
 * To be called when a toggle between mark and space is detected, to 
 * sample incoming bits. 
 *******************************************************************************/
 
static void sample_bits()
{
    /* The number of 1-bits is determined by the timer count */
     uint16_t counts = TCNT1;
     TCNT1 = 0;
     uint8_t ones = (uint8_t) 
        ((counts - (uint16_t) SYMBOL_SAMPLE_RESYNC) / (uint16_t) SYMBOL_SAMPLE_INTERVAL);

     if (ones >= 8) 
         /* Bit stuffing/FLAG will ensure that there should not be more than 
          * six 1-bits in a valid frame */ 
         _afsk_abort();
     else {
         for (uint8_t i = 0; i<ones; i++)
            add_bit(true);  
         add_bit(false);
    }
}



/********************************************************************************
 * Send a single bit to the HDLC decoder
 ********************************************************************************/
 
static uint8_t bit_count = 0;

static void add_bit(bool bit)
{ 
   static uint8_t octet;
   octet = (octet >> 1) | (bit ? 0x80 : 0x00);
   bit_count++;
   
   if (bit_count == 8) 
   {        
      /* Always leave room for abort token */
      if (afsk_rx_stream.length.cnt < afsk_rx_stream.size-1) 
         stream_put_nb (&afsk_rx_stream, octet);
      else
         /* This should never happend, but if if it does it can only be
          * caused by the buffer being too short or a thread running too
          * long. Having some kind of log to report such errors would be
          * a good idea. 
          */ 
          _afsk_abort();
      bit_count = 0;
   }
}




/******************************************************************************
 * Indicate that decoding fails (e.g. if signal is lost)
 ******************************************************************************/
 
static void _afsk_abort ()
{
    /* A sequence of seven or more consecutive ones will always
     * reset the HDLC decoder to flag sync state. 
     * If the buffer full, an abort token has already been
     * written to the stream 
     */
    bit_count = 0;
    if (afsk_rx_stream.length.cnt < afsk_rx_stream.size)
       stream_put_nb (&afsk_rx_stream, 0xFF);
    
}
