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
#define TXDELAY 60
#define TXTAIL 40
#define PERSISTENCE 100
#define SLOTTIME    50

static void hdlc_txencoder(void);
static void hdlc_testsignal(void);
static void hdlc_encode_frames(void);
static void hdlc_encode_byte(uint8_t, bool);

   
fbq_t* hdlc_init_encoder(stream_t* os)
{
   outstream = os;
   FBQ_INIT( encoder_queue, HDLC_ENCODER_QUEUE_SIZE ); 
   THREAD_START( hdlc_txencoder, DEFAULT_STACK_SIZE );
   
   sem_init(&test, 0);
   THREAD_START( hdlc_testsignal, DEFAULT_STACK_SIZE);
   return &encoder_queue;
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
        while(test_active) 
           putch(outstream, testbyte);
    }
}




#define afsk_channel_ready(t) true

/*******************************************************************************
 * Transmit a frame.
 *
 * This function gets a frame from buffer-queue, and starts the transmitter
 * as soon as the channel is free. It should typically be called repeatedly
 * from a loop in a thread.  
 *******************************************************************************/
 
static void hdlc_txencoder()
{ 
   while (true)  
   {
      /* Get frame from buffer-queue when available. 
       * This is a blocking call.
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
      hdlc_encode_frames();
   }
}
   



/*******************************************************************************
 * HDLC encode and transmit one or more frames (one single transmission)
 * It is responsible for computing checksum, bit stuffing and for adding 
 * flags at start and end of frames.
 *******************************************************************************/
 
static void hdlc_encode_frames()
{
     uint16_t crc = 0;
     uint8_t txbyte, i;
   
     for (i=0; i<TXDELAY; i++)
         hdlc_encode_byte(HDLC_FLAG, true);

     while(!BUFFER_EMPTY)
     {
         txbyte = fbuf_getChar(&buffer);
         crc = _crc_ccitt_update (crc, txbyte);
         hdlc_encode_byte(txbyte, false);
     }
     fbuf_release(&buffer);
     hdlc_encode_byte(crc, false);     // Send FCS, LSB first?
     hdlc_encode_byte(crc>>8, false);  // MSB
          
     for (i=0; i<TXTAIL; i++)
         hdlc_encode_byte(HDLC_FLAG, true);
}





static void hdlc_encode_byte(uint8_t txbyte, bool flag)
{    
     static uint8_t outbits = 0;
     static uint8_t sequential_ones = 0;
     static uint8_t outbyte;
     
     for (uint8_t i=1; i<8+1; i++)
     { 
        register uint8_t bit_zero = txbyte & 0x01;
        if (!flag && bit_zero && ++sequential_ones == 6) {
            i--;
            sequential_ones = 0;     
        }
        else {
          txbyte >>= 1;       
          outbyte |= (bit_zero << 7); 
        }
     
        if (++outbits == 8) {
            putch(outstream, outbyte);
            outbyte = 0;
            outbits = 0;
        }
        else
            outbyte >>= 1;    
     }   
}

