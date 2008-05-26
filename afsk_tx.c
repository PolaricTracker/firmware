/*
 * $Id: afsk_tx.c,v 1.16 2008-05-26 21:51:04 la7eca Exp $
 * AFSK Modulator/Transmitter
 */
 
#include "defines.h"
#include <avr/io.h>
#include <inttypes.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include "afsk.h"
#include "kernel.h"
#include "stream.h"
#include "transceiver.h"


/* Move to config.h or afsk.h */
#define AFSK_TXTONE_MARK  1200
#define AFSK_TXTONE_SPACE 2200

/* Internal config */
#define _PRESCALER3  8
#define _TXI_MARK   ((SCALED_F_CPU / _PRESCALER3 / AFSK_TXTONE_MARK / 2) - 1)
#define _TXI_SPACE  ((SCALED_F_CPU / _PRESCALER3 / AFSK_TXTONE_SPACE / 2) - 1)

bool     transmit; 		
stream_t afsk_tx_stream;
static uint16_t timertop;
   
   
   
stream_t* afsk_init_encoder(void) 
{
    STREAM_INIT(afsk_tx_stream, AFSK_ENCODER_BUFFER_SIZE);   

#if defined USE_TXPIN_OC
    make_output(OC3A);
#else
    make_output(TXDATA);
#endif
    return &afsk_tx_stream;
}		




/*******************************************************************************
 * Turn on transmitter (tone generator, ptt and led.)
 *******************************************************************************/
 
void afsk_ptt_on()
{        
    TCCR3B = 0x02                  /* Pre-scaler for timer3 = 8x */             
             | (1<<WGM32) ;        /* CTC mode */   
    TCCR3A |= (1<<COM3A0);         /* Toggle OC3A on compare match. */
    OCR3A  = timertop = _TXI_MARK;
    TIMSK3 = 1<<OCIE3A;            /* Interrupt on compare match */ 
    transmit = true; 
    adf7021_enable_tx();
    set_port(LED2);
}




/*******************************************************************************
 * Turn off transmitter (tone generator, ptt and led.)
 *******************************************************************************/

void afsk_ptt_off(void)
{
    TIMSK3 = 0x00;
    TCCR3A &= ~(1<<COM3A0);           /* Toggle OC3A on compare match: OFF. */
    transmit = false; 
    clear_port(LED2);                 /* LED / PTT */
#if !defined USE_TXPIN_OC    
    clear_port(TXDATA);               /* out signal */
#endif
    adf7021_disable_tx();
}



/***********************************************
 * Get next bit from stream
 * Note: see also get_bit() in hdlc_decoder.c 
 ***********************************************/
 
static uint8_t get_bit(void)
{
  static uint8_t bits;
  static uint8_t bit_count = 0;
  if (bit_count == 0) 
  {
    /* Turn off TX if buffer is empty (have reached end of frame) */
    if (stream_empty(&afsk_tx_stream)) {
        afsk_ptt_off();
        return 0; 
    }   
    bits = stream_get_nb (&afsk_tx_stream); 
    bit_count = 8;    
  } 
  uint8_t bit = bits & 0x01;
  bits >>= 1;
  bit_count--;
  return bit;
}



/*******************************************************************************
 * If transmit flag is on, this function should be called periodically, 
 * at same rate as wanted baud rate.
 *
 * It is responsible for transmitting frames by toggling the frequency of
 * the tone generated by the interrupt handler below. 
 *******************************************************************************/ 

void afsk_txBitClock(void)
{
    if (!transmit) {
        if (stream_empty(&afsk_tx_stream))
           return;
        else
           afsk_ptt_on();
    }       
    if ( ! get_bit() ) 
        /* Toggle TX frequency */ 
        timertop = ((timertop >= _TXI_MARK) ? _TXI_SPACE : _TXI_MARK); 
}



/******************************************************************************
 * Simple method of generating a clock signal at 1200 and 2200 Hz. 
 * This is output to ADF TXRXCLK pin. Note: If we can use the OC1A pin  
 * we dont need this interrupt handler at all! 
 ******************************************************************************/
 
ISR(TIMER3_COMPA_vect)
{   
#if !defined USE_TXPIN_OC
     toggle_port( TXDATA );
#endif
     OCR3A = timertop;
} 

 
