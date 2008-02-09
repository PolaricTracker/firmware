#include <stdint.h>

#define TRUE   (1)
#define FALSE  (0)

#define F_CPU 4000000UL 

#define	UART_BUF_SIZE	(16)		


#define FBUF_SLOTS    32
#define FBUF_SLOTSIZE 16

#define DEFAULT_STACK_SIZE 70

#define AFSK_ENCODER_BUFFER_SIZE 128
#define AFSK_DECODER_BUFFER_SIZE 128
#define HDLC_DECODER_QUEUE_SIZE  4
#define HDLC_ENCODER_QUEUE_SIZE  4

#define DCD_LED_PORT    PORTD
#define DCD_LED_BIT     PD5
#define ADF_SCLK_PORT   PORTC
#define ADF_SCLK_BIT    PC4
#define ADF_SDATA_PORT  PORTC
#define ADF_SDATA_BIT   PC2
#define ADF_SREAD_PORT  PORTC
#define ADF_SREAD_BIT   PC3

#define ADF_TXRXDATA_PORT PORTB
#define ADF_TXRXDATA_BIT  PB3


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

#define USBKEY_JS_LEFT_PORT  PORTB
#define USBKEY_JS_LEFT_BIT   PB6
#define USBKEY_JS_RIGHT_PORT PORTE
#define USBKEY_JS_RIGHT_BIT  PE4
#define USBKEY_JS_UP_PORT    PORTB
#define USBKEY_JS_UP_BIT     PB7
#define USBKEY_JS_DOWN_PORT  PORTE
#define USBKEY_JS_DOWN_BIT   PE5
#define USBKEY_JS_PUSH_PORT  PORTB
#define USBKEY_JS_PUSH_BIT   PB5


/*
 * Bit fiddling macros
 */
#define bit_is_set(x)  (x##_PORT) & (x##_BIT)
#define set_bit(x)     (x##_PORT) |= (1  << (x##_BIT))
#define clear_bit(x)   (x##_PORT)  &= ~(1 << x##_BIT)
#define toggle_bit(x)  (x##_PORT) ^= (1 << x##_BIT)

#define set_sfr_bit(sfr,bit) (sfr)|=(1<<(bit))
#define clear_sfr_bit(sfr,bit) (sfr)&=~(1<<(bit))
#define toggle_sfr_bit(sfr,bit) (sfr)^=(1<<(bit));

/*
 * other
 */
#define nop()  asm volatile("nop");
