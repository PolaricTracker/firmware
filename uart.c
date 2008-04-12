/*
 * $Id: uart.c,v 1.9 2008-04-12 18:26:57 la7eca Exp $
 */
 
#include "defines.h"
#include <avr/io.h>
#include <avr/signal.h>

#include "kernel.h"
#include "stream.h"

#define USART_BAUD 9600
#define UART_UBRR (SCALED_F_CPU/(16L*USART_BAUD)-1) 

void uart_kickout(void); 


/************************************
        DRIVER FOR UART
 ************************************/

Stream uart_instr;
Stream uart_outstr; 

static unsigned char echo;



void uart_init(const unsigned char e)
{
   echo = e; 

   STREAM_INIT( uart_instr, UART_BUF_SIZE );
   STREAM_INIT( uart_outstr, UART_BUF_SIZE );
    
   uart_outstr.kick = uart_kickout; 
   
   // Set baud rate 
   UBRR1 = UART_UBRR;
  
   // Enable Receiver and Transmitter Interrupt, Receiver and Transmitter
   UCSR1B = (1<<RXCIE1)|(1<<TXCIE1)| (1<<RXEN1)|(1<<TXEN1);
   // Set frame format to 8 data bits, no parity, and 1stop bit
   UCSR1C = (1<<UCSZ10) | (1<<UCSZ11);
}               



void uart_kickout(void)
{
   if ((UCSR1A & (1<<UDRE1)))
       UDR1 = stream_get_nb(&uart_outstr);     
}


/*******************************************************************************
        Called by the receive ISR (interrupt). Saves the next serial
   byte to the head of the RX buffer.
 *******************************************************************************/
 
ISR(USART1_RX_vect)
{
      register char x = UDR1;   
      stream_put_nb(&uart_instr, x);
      if ( echo ) 
         stream_sendByte_nb(&uart_outstr, x);    
} 



/*******************************************************************************
   Called by the transmit ISR (interrupt). Puts the next serial
   byte into the TX register.
 *******************************************************************************/

ISR(USART1_TX_vect)
{
   if (! stream_empty(&uart_outstr) ) 
       uart_kickout();
}
 
