#include <stdint.h>

#define TRUE   (1)
#define FALSE  (0)

#define F_CPU 8000000UL 

#define	UART_BUF_SIZE	(16)		

#define TX_Q_SIZE   8
#define RX_Q_SIZE   8

#define FBUF_SLOTS    32
#define FBUF_SLOTSIZE 16

#define DCD_LED_PORT    PORTD
#define DCD_LED_BIT     PD5
#define ADF_SCLK_PORT   PORTB
#define ADF_SCLK_BIT    PB0
#define ADF_SDATA_PORT  PORTB
#define ADF_SDATA_BIT   PB1


/*
 * These are specific to the USB KEY.  
 */
#define USBKEY_LED1_PORT   PORTD
#define USBKEY_LED1_BIT    PD4
#define USBKEY_LED2_PORT   PORTD
#define USBKEY_LED2_BIT    PD5
#define USBKEY_LED3_PORT   PORTD
#define USBKEY_LED3_BIT    PD6
#define USBKEY_LED4_PORT   PORTD
#define USBKEY_LED4_BIT    PD7


/*
 * Bit fiddling macros
 */
#define bit_is_set(x)  (x##_PORT) & (x##_BIT)
#define set_bit(x)     (x##_PORT) |= (1  << (x##_BIT))
#define clear_bit(x)   (x##_PORT)  &= ~(1 << x##_BIT)
#define toggle_bit(x)  (x##_PORT) ^= (x##_BIT)

/*
 * other
 */
#define nop()  asm volatile("nop");
