/*
 * AFSK Modulator/Transmitter
 */
 
#include "config.h"
#include <avr/io.h>
#include <inttypes.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include "fbuf.h"
#include "afsk.h"
#include "kernel.h"
#include "timer.h"
#include "stream.h"

	
extern uint8_t	transmit;		          
uint8_t txbytes; 

// Buffers
FBQ outbuf; 
static FBUF buffer;                                 

#define BUFFER_EMPTY (fbuf_eof(&buffer))            

enum {TXOFF, TXPRE, TXPACKET, TXPOST} txState; 

/* FIXME: Those should be parameters stored in EEPROM ! */
#define TXDELAY 30
#define TXTAIL 4
#define PERSISTENCE 100
#define SLOTTIME    50


   
void init_hdlc_encoder()
{
   DEFINE_FBQ(outbuf, TX_Q_SIZE);
   THREAD_START( afsk_txencoder, DEFAULT_STACK_SIZE);
}		



/*******************************************************************************
 * Transmit a frame.
 *
 * This function gets a frame from buffer-queue, and starts the transmitter
 * as soon as the channel is free. It should typically be called repeatedly
 * from a loop in a thread.  
 *******************************************************************************/
 
static void afsk_txencoder()
{  
   /* Get frame from buffer-queue when available. This is a blocking call.
    * FIXME: Protect against concurrent access
    */
   buffer = fbq_get(&outbuf);     
       
   /* Wait until channel is free 
    * P-persistence algorithm 
    */
   for (;;) {
     while ( !afsk_channel_ready(0) )
        t_yield(); 
     int r  = rand(); 
     if (r > PERSISTENCE * 255)
         sleep(SLOTTIME); 
     else
         break;
   }
  
   afsk_ptt_on();   
   afsk_encode_frames();
}
   



/*******************************************************************************
 * HDLC encode and transmit one or more frames (one single transmission)
 * It is responsible for computing checksum, bit stuffing and for adding 
 * flags at start and end of frames.
 *******************************************************************************/
 
void afsk_encode_frames()
{
    uint8_t txbyte, txbits, bit_zero, flag, nflags;  
    uint8_t outbyte, outbits;
    uint8_t sequential_ones;
    
    txState = TXPRE;
    txbytes = 0;
    while (TRUE)
    {
       /* 
        * This part (if txbits == 0) determines the next byte to 
        * send or alternatively, to turn off the transmitter.
        */  
       if (txbits == 0) 
       {
           /* 
            * State machine: 
            * TXPRE    - start sending frame - send as many flags as indicated in TXDELAY
            * TXPOST   - end frame - send as many flags as indicated in TXTAIL
            * TXPACKET - send frame data until buffer is empty.         
            */
           if (txState == TXPRE && nflags >= TXDELAY)
               txState = TXPACKET;
             
           else if (txState == TXPOST && nflags >= TXTAIL) {
               nflags = 0; 
               return;
           }
           else if (txState == TXPACKET && BUFFER_EMPTY) {
               nflags = 0;                               // If end of buffer, go to TXPOST state (send end-of-frame flags)
               txState = TXPOST;                         // FIXME: Allow more than one frame per transmission?
               fbuf_release(&buffer); 
           }    

      
           /*
            * Do the work (depending on the state). 
            * Get next byte to transmit, or use FLAG
            */
           if (txState == TXPACKET) {          
               txbyte = fbuf_getChar(&buffer);            // send next byte from buffer
               txbytes++; 
               flag = 0;        
           }
           else {                                 
               flag = txbyte = FLAG;                      // Send flag. And indicate that this was done deliberately
               nflags++;                                  // And count how many flags was sent  
           }
           txbits = 8; 
       }
    
    
       /* 
        * Transmit the bits with bit stuffing.
        * AFSK:  Toggle transmitter tone if 0. Do nothing if 1. 
        */
       if (flag)
           sequential_ones = 0; 
        
       bit_zero = txbyte & 0x01;  
       if (!bit_zero || sequential_ones++ == 6) 
           sequential_ones = 0;

       if (sequential_ones < 6) 
          { txbyte >>= 1; txbits--; }    
       
       outbyte << 1;
       outbyte |= bit_zero; 
       outbits++;
       if (!sequential_ones)                       
          { outbyte << 1; outbits++; }  /* Bit stuffing */ 
       
       putch(&stream, outbyte);       
    }
}

