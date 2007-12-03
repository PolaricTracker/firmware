
#include "config.h"
#include <avr/io.h>
#include <avr/signal.h>

#include "kernel.h"
#include "stream.h"


void kickout(); 


/************************************
        DRIVER FOR UART
 ************************************/

Stream instr;
static char inbd[UART_BUF_SIZE];  
Stream outstr; 
static char outbd[UART_BUF_SIZE]; 

static unsigned char echo;



void	init_UART(const unsigned char e)
{
   echo = e; 

   _buf_init(&instr, inbd, UART_BUF_SIZE);
   _buf_init(&outstr, outbd, UART_BUF_SIZE);
    
   sem_init(&instr.block, 0);
   sem_init(&instr.mutex, 1);
   sem_init(&outstr.block, UART_BUF_SIZE-1);
   sem_init(&outstr.mutex, 1);
 
   outstr.kick = kickout; 

	// Set baud rate of USART to 4800 baud at 14.7456 MHz
	UBRR0H = 0;
	UBRR0L = 96;  // 191 for 4800bd 
   
	// Enable Receiver and Transmitter Interrupt, Receiver and Transmitter
   UCSR0B = (1<<RXCIE0)|(1<<TXCIE0)| (1<<RXEN0)|(1<<TXEN0);
	// Set frame format to 8 data bits, no parity, and 1stop bit
	UCSR0C = (3<<UCSZ00);
}		



void kickout()
{
   if ((UCSR0A & (1<<UDRE0)))
   {
		 UDR0 = _buf_get(&outstr);     
       sem_up(&outstr.block);
   }
}



/*******************************************************************************
 	Called by the receive ISR (interrupt). Saves the next serial
   byte to the head of the RX buffer.
 *******************************************************************************/
 
ISR(USART_RX_vect)
{
   cli(); 
   if (!_buf_full(&instr))
   { 
      register char x = UDR0;	    
      _buf_put(&instr, x);
      if ( echo ) 
      {
         _sendByte(&outstr, x);   
         sem_nb_down(&outstr.block);
      }  
      sem_up(&instr.block);
   }
   sei();
} 



/*******************************************************************************
   Called by the transmit ISR (interrupt). Puts the next serial
   byte into the TX register.
 *******************************************************************************/

ISR(USART_TX_vect)
{
   cli(); 
	if (! _buf_empty(&outstr) ) 
      kickout();
   sei();
}
 
