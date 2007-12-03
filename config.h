
#define TRUE   (1)
#define FALSE  (0)

#define F_CPU 14745600UL 

#define	UART_BUF_SIZE	(16)		

#define TX_Q_SIZE   8
#define RX_Q_SIZE   8

#define FBUF_SLOTS    32
#define FBUF_SLOTSIZE 16

#define DCD_LED_ON  { PORTB |= (1 << PB0); }
#define DCD_LED_OFF { PORTB &= ~(1 << PB0); }
