/*
 * AFSK Modulator/Transmitter
 */
 
#include "config.h"
#include <avr/io.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdbool.h>
#include <avr/interrupt.h>
#include "fbuf.h"
#include "hdlc.h"
#include "kernel.h"
#include "timer.h"
#include "stream.h"
#include <util/crc16.h>
		          
uint8_t txbytes; 

// Buffers
FBQ encoder_queue; 
static FBUF buffer;                                 
static stream_t* outstream;

static Semaphore test; 
static bool test_active;
static uint8_t testbyte;

#define BUFFER_EMPTY (fbuf_eof(&buffer))            

enum {TXOFF, TXPRE, TXPACKET, TXPOST} txState; 

/* FIXME: Those should be parameters stored in EEPROM ! */
#define TXDELAY 30
#define TXTAIL 4
#define PERSISTENCE 100
#define SLOTTIME    50

static void hdlc_txencoder();
static void hdlc_testsignal();
static void hdlc_encode_frames();


   
void init_hdlc_encoder()
{
   DEFINE_FBQ(encoder_queue, HDLC_ENCODER_QUEUE_SIZE);
   THREAD_START( hdlc_txencoder, DEFAULT_STACK_SIZE);
   
   sem_init(&test, 0);
   THREAD_START( hdlc_testsignal, DEFAULT_STACK_SIZE);
}		


/*******************************************************
 * Code for generating a test signal
 *******************************************************/

void hdlc_test_on(uint8_t b)
{ 
    testbyte = b;
    test_active = true;
    sem_up(&test);
}

void hdlc_test_off()
    { test_active=false; }

    
static void hdlc_testsignal()
{
    while (true)
    {
        sem_down(&test);
        while(test_active);
            putch(outstream, testbyte);  
    }
}





/*******************************************************************************
 * Transmit a frame.
 *
 * This function gets a frame from buffer-queue, and starts the transmitter
 * as soon as the channel is free. It should typically be called repeatedly
 * from a loop in a thread.  
 *******************************************************************************/
 
static void hdlc_txencoder()
{  
   /* Get frame from buffer-queue when available. This is a blocking call.
    * FIXME: Protect against concurrent access
    */
   buffer = fbq_get(&encoder_queue);     
       
   /* Wait until channel is free 
    * P-persistence algorithm 
    */
   for (;;) {
     while ( !afsk_channel_ready(0) )  // FIXME
        t_yield(); 
     int r  = rand(); 
     if (r > PERSISTENCE * 255)
         sleep(SLOTTIME); 
     else
         break;
   }
  
   afsk_ptt_on();                      // FIXME
   hdlc_encode_frames();
}
   



/*******************************************************************************
 * HDLC encode and transmit one or more frames (one single transmission)
 * It is responsible for computing checksum, bit stuffing and for adding 
 * flags at start and end of frames.
 *******************************************************************************/
 
static void hdlc_encode_frames()
{
    uint8_t  bit_zero, txbyte=0, txbits=0, flag=0, nflags=0;  
    uint8_t outbyte=0, outbits=0, crc=0;
    uint8_t sequential_ones=0;
    
    txState = TXPRE;
    txbytes = 0;
    while (TRUE) {
       /* 
        * This part (if txbits == 0) determines the next byte to send
        */  
       if (txbits == 0) 
       {
           /* 
            * State machine: 
            * TXPRE    - start sending frame - send as many flags as indicated in TXDELAY
            * TXPOST   - end frame - send as many flags as indicated in TXTAIL
            * TXPACKET - send frame data until buffer is empty.         
            */
           txbits = 8;  
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
               txbyte = crc;                             // Insert checksum
               break;                                   
           }    
     
           /*
            * Do the work (depending on the state). 
            * Get next byte to transmit, or use FLAG
            */
           if (txState == TXPACKET) {          
               txbyte = fbuf_getChar(&buffer);            // send next byte from buffer
               crc = _crc_ccitt_update (crc, txbyte);     // compute checksum
               txbytes++; 
               flag = 0;        
           }
           else {                                 
               flag = txbyte = HDLC_FLAG;                 // Send flag. And indicate that this was done deliberately
               nflags++;                                  // And count how many flags was sent  
           }
       }
    
    
       /* 
        * Transmit the bits with bit stuffing.
        */
       if (flag)
           sequential_ones = 0; 
        
       bit_zero = txbyte & 0x01;  
       if (!bit_zero || sequential_ones++ == 6) 
           sequential_ones = 0;

       if (sequential_ones < 6) 
          { txbyte >>= 1; txbits--; }    
       
       outbyte <<= 1;
       outbyte |= bit_zero; 
       outbits++;
       if (!sequential_ones)                       
          { outbyte <<= 1; outbits++; }  /* Bit stuffing */ 
       
       putch(outstream, outbyte);       
    }
}

