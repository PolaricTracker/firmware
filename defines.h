#include <stdint.h>

#define SCALED_F_CPU   (F_CPU / 2)

#define TRUE   (1)
#define FALSE  (0)

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


/*
 * ADF 7021 specific definitions
 */
#define ADF7021_XTAL        2400000 
#define ADF7021_SCLK_PORT   PORTC
#define ADF7021_SCLK_DDR    DDRC
#define ADF7021_SCLK_BIT    PC4
#define ADF7021_SDATA_PORT  PORTC
#define ADF7021_SDATA_DDR   DDRC
#define ADF7021_SDATA_BIT   PC2
#define ADF7021_SREAD_PORT  PORTC
#define ADF7021_SREAD_PORT  DDRC
#define ADF7021_SREAD_BIT   PC3
#define ADF7021_SLE_PORT    PORTB
#define ADF7021_SLE_DDR     DDRB
#define ADF7021_SLE_BIT     PB3
#define ADF7021_ON_PORT     PORTC
#define ADF7021_ON_DDR      DDRC
#define ADF7021_ON_BIT      PC5
#define ADF7021_MUXOUT_PORT PORTC
#define ADF7021_MUXOUT_DDR  DDRC
#define ADF7021_MUXOUT_BIT  PC5


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

#define set_ddr(x)     (x##_DDR) |= _BV(x##_BIT)
#define clear_ddr(x)   (x##_DDR) &= ~_BV(x##_BIT)
#define set_port(x)    (x##_PORT) |= _BV(x##_BIT)
#define clear_port(x)  (x##_PORT) &= ~_BV(x##_BIT)
#define port_is_set(x) (x##_PORT & _BV(x##_BIT))
#define toggle_port(x) (x##_PIN) |= _BV(x##_BIT) 
#define pin_is_high(x) (x##_PIN & _BV(x##_BIT))

/* Use float (or tri-state as Atmel calls it) if pin has external
   pull-up. Be carefull when switching between input and output or
   output and input. */
#define make_output(x) set_ddr(x)
#define make_input(x) { clear_ddr(x); set_port(x); }
#define make_float(x) { clear_ddr(x); clear_port(x); }

/*
 * other
 */
#define nop()  asm volatile("nop");
