/*
 * AFSK receiver/demodulator
 */
 

#include <avr/io.h>
#include <inttypes.h>
#include <avr/interrupt.h>
#include "fbuf.h"
#include "afsk.h"
#include "kernel.h"
#include "stream.h"
#include "config.h" 


static   uint8_t    rxtoggled;  // Signals frequency just toggled
         uint8_t    dcd;        // Carrier detect of sorts
volatile uint8_t    frame;      // Receiving a frame   

static   FBUF       fbuf;       // Incoming frame buffer
FBQ                 fbin;       // Incoming frame buffer queue    



void init_afsk_RX()
{
    DEFINE_FBQ(fbin, RX_Q_SIZE);
    ADCSRB = 0;                    // Select AIN1 as neg. input (SFIOR in Atmega8)
    ACSR =  (1<<ACBG);             // Select bandgap for positive input. 
    fbuf_new(&fbuf);               // Initialise packet buffer
    clear_bit( DCD_LED );
}



/*************************************************************************************
 * Enable receiver (at startup or when transmission ends)
 *************************************************************************************/
 
void afsk_enable_RX()
{
    ACSR  |= (1<<ACIE);             // Enable comparator interrupt.
    DIDR1  = 3;
    TCCR1B = 0x03;                  // 3 => Timer2 clock prescale of 8.     3=>64. 
    TCNT1  = 0;
}



/**************************************************************************************
 * Disable Receiver (to be used when keying transmitter)
 **************************************************************************************/
 
void afsk_disable_RX()
{
    ACSR &= ~(1<<ACIE);             // Disable comparator interrupt.
    dcd = 0;
}




uint16_t toggles; 

/***************************************************************************************
 * If transmit flag is not on, this function should be called periodically, 
 * at the same rate as wanted baud rate.
 *
 * It is responsible for receiving frames. As long as the dcd is on it reads
 * if input tones are changing between mark and space (see interrupt handler below), 
 * and it shifts in the bits the tones represent (toggle means 1 no-toggle means 0). 
 * Incoming complete frames are put into the packet-buffer-queue (fbin).
 ***************************************************************************************/
extern Stream outstr; 

void afsk_rxBitClock()
{
    static uint8_t last8bits;             // Last 8 bits received
    static uint8_t bit_count;             // Bits of the next incoming byte
    static uint8_t byte_count;            // Bytes received in current frame
    static uint8_t ones_count;            // Sequential ones (detect a stuff)
    static uint8_t next_sample, sample_clock;     

    if (dcd)                              // If we are actively monitoring a signal
    {   
       dcd--;                             // Decrement the dcd timer
       set_bit( DCD_LED ); 
       
       if (rxtoggled)                     // See if a tone toggle was recognized
       {    
           if(ones_count != 5)            // If receiving a frame, Only process if NOT a bit stuff toggle
           {
              bit_count++;                // Increment bit counter
              last8bits >>= 1;            // Shift in a zero from the left
           }
           rxtoggled--;                   // Decrement receiver toggle count
           ones_count = 0;                // Clear number of sequential ones
           next_sample = 12;
           sample_clock = 0;
       }
       else if (++sample_clock == next_sample)
       {  
           ones_count++;                  // Increment ones counter since no toggle
           bit_count++;                   // Increment bit counter
           last8bits >>= 1;               // Shift the bits to the right
           last8bits |= 0x80;             //  shift in a one from the left
           next_sample = 8;
           sample_clock = 0;
       } 

       
       if (last8bits == FLAG) 
       { 
           if (byte_count && frame)       // If flag, and we are receiving, and frame start was flagged earlier 
           {                              // ...this means that we reached end of frame 
              frame = FALSE;      
              fbuf_putstr(&fbuf, ">"); 
              fbq_put(&fbin, fbuf);       // Put buffer chain in queue (a copy of the head)
              fbuf_new(&fbuf);            // reset the buffer chain header (without destroying buffers) 
           }
           else {
              fbuf_putstr(&fbuf, "<");
              frame = TRUE;
           }
           bit_count = byte_count = 0;
       } 
       else if (frame && bit_count == 8)              
       {                                  // Add incoming byte to buffer chain
           byte_count++;
           if (last8bits >= '0' && last8bits <= '9')
               fbuf_putChar(&fbuf, last8bits);
           else
               fbuf_putChar(&fbuf, '.');
       }    
       if (bit_count == 8)
           bit_count = 0;
    }    
    else {
       clear_bit( DCD_LED ); 
       frame = FALSE;
       if (byte_count)                     // IF DCD drops and we are receiving...
       {                                   //  throw buffer away
            byte_count = 0;                
            fbuf_release(&fbuf);  
       }
    }
}       




/********************************************************************************
 * Interrupt routine to be called each time signal crosses zero
 * Measures frequency of signal and determines if there has been a toggle 
 * between mark and space. Based on whereavr code
 ********************************************************************************/

SIGNAL(SIG_COMPARATOR)
{
    static uint16_t tcount;
    static uint8_t last; 
       
    tcount = TCNT1;                    // Read counts since last interrupt
    dcd = 10;
    
    if (tcount > 34)                   // 34 -- Below 1700 Hz?   
    {
         TCNT1 = 0;  
         if (last == SPACE)            // If the last tone detected was a SPACE
            { rxtoggled++; }           // Toggle detected                              
         last = MARK;                  // MARK is detected
    }
    else            
    {
         TCNT1 = 0;  
         if (last == MARK)             // If the last tone detected was a MARK
            { rxtoggled++; }           // Toggle detected
         last = SPACE;                 // SPACE is detected
    } 
    
 
}  // End SIGNAL(SIG_COMPARATOR)
