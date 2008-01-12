
#include "config.h"
#include <avr/io.h>
#include <avr/signal.h>

#include "kernel.h"
#include "stream.h"

#define USART_BAUD 9600
#define UART_UBRR (F_CPU/(16L*USART_BAUD)-1) 

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

	// Set baud rate 
	UBRR1 = UART_UBRR;
   
	// Enable Receiver and Transmitter Interrupt, Receiver and Transmitter
   UCSR1B = (1<<RXCIE1)|(1<<TXCIE1)| (1<<RXEN1)|(1<<TXEN1);
	// Set frame format to 8 data bits, no parity, and 1stop bit
	UCSR1C = (1<<UCSZ10) | (1<<UCSZ11);
}		



void kickout()
{
   if ((UCSR1A & (1<<UDRE1)))
   {
		 UDR1 = _buf_get(&outstr);     
       sem_up(&outstr.block);
   }
}



/*******************************************************************************
 	Called by the receive ISR (interrupt). Saves the next serial
   byte to the head of the RX buffer.
 *******************************************************************************/
 
ISR(USART_RX_vect)
{
   enter_critical(); 
   if (!_buf_full(&instr))
   { 
      register char x = UDR1;	    
      _buf_put(&instr, x);
      if ( echo ) 
      {
         _sendByte(&outstr, x);   
         sem_nb_down(&outstr.block);
      }  
      sem_up(&instr.block);
   }
   leave_critical();
} 



/*******************************************************************************
   Called by the transmit ISR (interrupt). Puts the next serial
   byte into the TX register.
 *******************************************************************************/

ISR(USART_TX_vect)
{
   enter_critical(); 
	if (! _buf_empty(&outstr) ) 
      kickout();
   leave_critical();
}
 
