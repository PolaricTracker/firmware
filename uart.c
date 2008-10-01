/*
 * $Id: uart.c,v 1.16 2008-10-01 21:43:00 la7eca Exp $
 */
 
#include "defines.h"
#include <avr/io.h>
#include <avr/signal.h>

#include "kernel/kernel.h"
#include "kernel/stream.h"

#define UART_UBRR(x) (SCALED_F_CPU/(16L*(x))-1) 


static void uart_kickout(void); 


/************************************
        DRIVER FOR UART
 ************************************/

static Stream uart_instr;
static Stream uart_outstr; 

static bool echo;



Stream* uart_tx_init(uint16_t baud)
{
   STREAM_INIT( uart_outstr, UART_BUF_SIZE );
   uart_outstr.kick = uart_kickout; 
   
   // Set baud rate 
   UBRR1 = UART_UBRR(baud);
   // Set frame format to 8 data bits, no parity, and 1stop bit
   UCSR1C |= (1<<UCSZ10) | (1<<UCSZ11);
     
   // Enable Transmitter Interrupt
   UCSR1B = (1<<TXCIE1)| (1<<TXEN1);
   return &uart_outstr;
}


Stream* uart_rx_init(uint16_t baud, bool e)
{
   echo = e; 
   STREAM_INIT( uart_instr, UART_BUF_SIZE );    
   
   // Set baud rate 
   UBRR1 = UART_UBRR(baud);
   // Set frame format to 8 data bits, no parity, and 1stop bit
   UCSR1C = (1<<UCSZ10) | (1<<UCSZ11);
     
   // Enable Receiver Interrupt
   UCSR1B |= (1<<RXCIE1) | (1<<RXEN1);
   return &uart_instr;
}


void uart_rx_pause()
{
   // Clear Receiver Interrupt
   UCSR1B &= ~(1<<RXCIE1) & ~(1<<RXEN1);
}
void uart_rx_resume()
{
   // Enable Receiver Interrupt
   UCSR1B |= (1<<RXCIE1) | (1<<RXEN1);
}


static void uart_kickout(void)
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
 
