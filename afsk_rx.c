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
#include "ui.h"

 
/* The middle point between the two tones. */
#define TXTONE_MID ((AFSK_TXTONE_MARK+AFSK_TXTONE_SPACE)/2)

/* Symbol sampling interval. Used with counter TCNT1 */ 
#define SYMBOL_SAMPLE_INTERVAL ((SCALED_F_CPU/8/AFSK_BAUD))       
#define SYMBOL_SAMPLE_RESYNC   ((SCALED_F_CPU/8/AFSK_BAUD))*0.5

/* These are used with counter TCNT0 to measure frequency of input signal.
 * Note that these values corresponds to the time of a wave period.  
 */
#define CENTER_FREQUENCY ((SCALED_F_CPU/64)/TXTONE_MID-2)          
#define MARK_FREQUENCY  ((SCALED_F_CPU/64)/AFSK_TXTONE_MARK-1)    
#define SPACE_FREQUENCY ((SCALED_F_CPU/64)/AFSK_TXTONE_SPACE-1)  
#define FREQUENCY_DEVIATION 50



int8_t hard_symbol;    /* Most recent detected symbol. */
int8_t soft_symbol;    /* Differs from the above by also having UNDECIDED as valid state */
bool valid_symbol;     /* (DCD) Will always be true when using the simple detector */
extern bool transmit;  /* True when transmitter(modulator) is active (see afsk_tx.c). */
 
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
#if defined TARGET_USBKEY
    set_bit(DIDR1, AIN1D);
    make_output(TPB1);
    make_output(USBKEY_LED3);
    clear_bit(ACSR, ACD);
#endif
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
   clear_bit (TCCR1B, CS12);  /* /    
                               */
#if defined TARGET_USBKEY 
   set_port(USBKEY_LED3);
#else  
   /* Enable pin change interrupt on rx data from receiver */
   set_bit (PCMSK0, PCINT2);
   set_bit (PCICR, PCIE0);
#endif
   pri_rgb_led_on(true,true,false);
}


/****************************************************************************
 * Stop the decoding (typically used when a signal drops). 
 ****************************************************************************/
 
static void _afsk_stop_decoder ()
{  
   decoder_running = false;
#if defined TARGET_USBKEY
   clear_port(USBKEY_LED3); 
#else  
   /* Disable pin change interrupt on input pin from receiver */
   clear_bit (PCICR, PCIE0);
   clear_bit (PCMSK0, PCINT2);
#endif  
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
{ 
   make_input(TXDATA);
#if defined TARGET_USBKEY
   /* Enable analog comparator input and interrupt */
   set_bit(ACSR, ACBG);
   set_bit(ACSR, ACIE);
//   set_bit(ACSR, ACIS0); //  On rising edge only
//   set_bit(ACSR, ACIS1);
#endif
   decoder_enabled = true; 
   GET_PARAM(TRX_SQUELCH, &sqlevel); 
}
     
     
void afsk_disable_decoder ()
{ 
   decoder_enabled = false; 
   _afsk_stop_decoder(); 
#if defined TARGET_USBKEY
   /* Disable analog comparator input and interrupt */
   clear_bit(ACSR, ACIE);
   clear_bit(ACSR, ACBG);
#endif
}



static uint16_t signal_ints = 0;
#define SIG_THRESHOLD 2

/***************************************************************************
 * To be called periodically to see if there is a signal on the channel    *
 ***************************************************************************/
 
void afsk_check_channel ()
{
    if (decoder_enabled && !transmit)
    {
#if defined TARGET_USBKEY
       if ((!decoder_running) && (signal_ints >= SIG_THRESHOLD))
          _afsk_start_decoder();
       else if (decoder_running && (signal_ints < SIG_THRESHOLD))
          _afsk_stop_decoder();
#else           
       if ((!decoder_running) && (adf7021_read_rssi() > sqlevel))
          _afsk_start_decoder();
       else if (decoder_running && (adf7021_read_rssi() <= sqlevel))
          _afsk_stop_decoder();   
#endif    
    }
    signal_ints = 0;
}



/******************************************************************************
 * Interrupt routine to be called when the input signal from the              *
 * receiver changes level (this corresponds to a zero-crossing of the input   *  
 * signal). Measures frequency of signal and determines                       *
 * if there has been a toggle between mark and space.                         *
 ******************************************************************************/
   

uint16_t prev_zc_count; 


#if defined TARGET_USBKEY
ISR(ANALOG_COMP_vect)
#else
ISR(PCINT0_vect)
#endif
{
  signal_ints++; 
  if (!decoder_running)
     return;
  
  int8_t symbol;
  uint8_t count = TCNT0; /* Counts on Timer0 since last level-change */
  TCNT0 = 0;
  set_sleep_mode(SLEEP_MODE_IDLE);

  
  /* Use use count now + previous count: The sample is done every
   * half-period, but we use the time of the whole period for analysis.   
   */
  uint16_t counts = (uint16_t) prev_zc_count + count; 
  prev_zc_count = count; 
  
  
  symbol = UNDECIDED;
  if (counts > CENTER_FREQUENCY &&
       counts < MARK_FREQUENCY + FREQUENCY_DEVIATION)
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
        ((counts - (uint16_t) SYMBOL_SAMPLE_RESYNC+5) / (uint16_t) (SYMBOL_SAMPLE_INTERVAL+10));

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
         /* This should never happened, but if if it does it can only be
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
