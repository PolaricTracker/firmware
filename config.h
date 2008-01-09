
#define TRUE   (1)
#define FALSE  (0)

#define F_CPU 14745600UL 

#define	UART_BUF_SIZE	(16)		

#define TX_Q_SIZE   8
#define RX_Q_SIZE   8

#define FBUF_SLOTS    32
#define FBUF_SLOTSIZE 16

#define DCD_LED_PORT PORTB
#define DCD_LED_BIT  PB0

#define ADF_SCLK_PORT  PORTB
#define ADF_SCLK_BIT   PB0
#define ADF_SDATA_PORT PORTB
#define ADF_SDATA_BIT  PB1

#define _SET_BIT(p, bit)  (p) |= (1 << (bit)) 
#define _CLR_BIT(p, bit)  (p) &= ~(1 << (bit)) 
#define SET_BIT(x)       _SET_BIT(x##_PORT, x##_BIT)
#define CLR_BIT(x)       _CLR_BIT(x##_PORT, x##_BIT)


