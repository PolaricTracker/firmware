/*
 * $Id: defines.h,v 1.7 2008-04-12 18:18:05 la7eca Exp $ 
 */

#include <stdint.h>
#include <stdio.h>
#include <ctype.h>

#define USBKEY_TEST  /* Comment out if compiling for the real Polaric Tracker */

#define TRUE   (1)
#define FALSE  (0)


/********************************************
 * Note: The CPU frequency must be defined
 * in the makefile
 ********************************************/
 
#if defined USBKEY_TEST
#define SCALED_F_CPU F_CPU
#else
#define SCALED_F_CPU F_CPU
#endif


/******************************************* 
 * Buffers, memory management
 *******************************************/
 
#define UART_BUF_SIZE	   16		
#define FBUF_SLOTS         32
#define FBUF_SLOTSIZE      16
#define DEFAULT_STACK_SIZE 70

#define AFSK_ENCODER_BUFFER_SIZE 128
#define AFSK_DECODER_BUFFER_SIZE 128
#define HDLC_DECODER_QUEUE_SIZE  4
#define HDLC_ENCODER_QUEUE_SIZE  4


/********************************************
 * LEDS, misc. signals
 ********************************************/
 
#define TXDATA_PORT         PORTB
#define TXDATA_BIT          2 
#define TXDATA_DDR          DDRB
#define BUZZER_PORT         PORTA
#define BUZZER_BIT          6
#define BUZZER_DDR          DDRA
#define EXTERNAL_PA_ON_PORT PORTA  
#define EXTERNAL_PA_ON_BIT  2    
#define EXTERNAL_PA_ON_DDR  DDRA   
#define GPSON_PORT          PORTB
#define GPSON_BIT           6
#define GPSON_DDR           DDRB
#define GPIO_PORT           PORTE
#define GPIO_BIT            0
#define GPIO_DDR            DDRE
#define UART_IN_PORT        PORTD
#define UART_IN_BIT         2
#define UART_IN_DDR         DDRD
#define UART_OUT_PORT       PORTD
#define UART_OUT_BIT        3
#define UART_OUT_DDR        DDRD

#if defined USBKEY_TEST

#define LED1_PORT       USBKEY_LED1_PORT
#define LED1_BIT        USBKEY_LED1_BIT
#define LED1_DDR        USBKEY_LED1_DDR
#define LED2_PORT       USBKEY_LED2_PORT
#define LED2_BIT        USBKEY_LED2_BIT
#define LED2_DDR        USBKEY_LED2_DDR
#define LED3_PORT       USBKEY_LED3_PORT
#define LED3_BIT        USBKEY_LED3_BIT
#define LED3_DDR        USBKEY_LED3_DDR

#else

#define TXDATA_PORT     BUZZER_PORT 
#define TXDATA_BIT      BUZZER_BIT 
#define TXDATA_DDR      BUZZER_DDR
#define LED1_PORT       PORTA
#define LED1_BIT        7
#define LED1_DDR        DDRA
#define LED2_PORT       PORTC
#define LED2_BIT        7
#define LED2_DDR        DDRC
#define LED3_PORT       PORTA
#define LED3_BIT        0
#define LED3_DDR        DDRA

#endif




/*
 * ADF 7021 specific definitions
 */
#define ADF7021_XTAL        6400000 

#define ADF7021_SCLK_DDR      DDRC
#define ADF7021_SCLK_PORT     PORTC
#define ADF7021_SCLK_PIN      PINC
#define ADF7021_SCLK_BIT      4

#define ADF7021_SREAD_DDR     DDRC
#define ADF7021_SREAD_PORT    PORTC
#define ADF7021_SREAD_PIN     PINC
#define ADF7021_SREAD_BIT     3

#define ADF7021_SDATA_DDR     DDRC
#define ADF7021_SDATA_PORT    PORTC
#define ADF7021_SDATA_PIN     PINC
#define ADF7021_SDATA_BIT     2

#define ADF7021_SLE_DDR       DDRC
#define ADF7021_SLE_PORT      PORTC
#define ADF7021_SLE_PIN       PINC
#define ADF7021_SLE_BIT       1

#define ADF7021_ON_DDR        DDRC
#define ADF7021_ON_PORT       PORTC
#define ADF7021_ON_PIN        PINC
#define ADF7021_ON_BIT        0

#define ADF7021_TXRXDATA_DDR  DDRB
#define ADF7021_TXRXDATA_PORT PORTB
#define ADF7021_TXRXDATA_PIN  PINB
#define ADF7021_TXRXDATA_BIT  2
#define ADF7021_TXRXDATA_INT  PCINT2

#define ADF7021_TXRXCLK_DDR   DDRB
#define ADF7021_TXRXCLK_PORT  PORTB
#define ADF7021_TXRXCLK_PIN   PINB
#define ADF7021_TXRXCLK_BIT   3

#define ADF7021_MUXOUT_DDR    DDRB
#define ADF7021_MUXOUT_PORT   PORTB
#define ADF7021_MUXOUT_PIN    PINB
#define ADF7021_MUXOUT_BIT    4

#define ADF7021_SWD_DDR       DDRB
#define ADF7021_SWD_PORT      PORTB
#define ADF7021_SWD_PIN       PINB
#define ADF7021_SWD_BIT       0

#define ADF7021_CLKOUT_DDR    DDRB
#define ADF7021_CLKOUT_PORT   PORTB
#define ADF7021_CLKOUT_PIN    PINB
#define ADF7021_CLKOUT_BIT    1

#define EXTERNAL_PA_ON_DDR    DDRA
#define EXTERNAL_PA_ON_PORT   PORTA
#define EXTERNAL_PA_ON_PIN    PINA
#define EXTERNAL_PA_ON_BIT    2

/*
 * These are specific to the USB KEY.  
 */
  
#define USBKEY_LED1_PORT   PORTD
#define USBKEY_LED1_BIT    4
#define USBKEY_LED1_DDR    DDRD
#define USBKEY_LED2_PORT   PORTD
#define USBKEY_LED2_BIT    5
#define USBKEY_LED2_DDR    DDRD
#define USBKEY_LED3_PORT   PORTD
#define USBKEY_LED3_BIT    6
#define USBKEY_LED3_DDR    DDRD
#define USBKEY_LED4_PORT   PORTD
#define USBKEY_LED4_BIT    7
#define USBKEY_LED4_DDR    DDRD

#define USBKEY_JS_LEFT_PORT  PORTB
#define USBKEY_JS_LEFT_BIT   6
#define USBKEY_JS_RIGHT_PORT PORTE
#define USBKEY_JS_RIGHT_BIT  4
#define USBKEY_JS_UP_PORT    PORTB
#define USBKEY_JS_UP_BIT     7
#define USBKEY_JS_DOWN_PORT  PORTE
#define USBKEY_JS_DOWN_BIT   5
#define USBKEY_JS_PUSH_PORT  PORTB
#define USBKEY_JS_PUSH_BIT   5


/*
 * Bit fiddling macros
 */

#define _set_ddr(x) (x##_DDR) |= _BV(x##_BIT)
#define _clear_ddr(x) (x##_DDR) &= ~_BV(x##_BIT)
#define set_port(x) (x##_PORT) |= _BV(x##_BIT)
#define clear_port(x) (x##_PORT) &= ~_BV(x##_BIT)
#define port_is_set(x) (x##_PORT & _BV(x##_BIT))
#define toggle_port(x) (x##_PORT) ^= _BV(x##_BIT) // Alternative: Setting PIN will toggle PORT
#define pin_is_high(x) (x##_PIN & _BV(x##_BIT))

/* Use float (or tri-state as Atmel calls it) if pin has external
   pull-up. Be carefull when switching between input and output or
   output and input. */
#define make_output(x){ clear_port(x); _set_ddr(x); } 
#define make_input(x) { _clear_ddr(x); set_port(x); }
#define make_float(x) { _clear_ddr(x); clear_port(x); } // Default on power up

#define set_bit(sfr,bit) (sfr)|=_BV(bit)
#define clear_bit(sfr,bit) (sfr)&=~_BV(bit)
#define toggle_bit(sfr,bit) (sfr)^=_BV(bit)


/*
 * other
 */
#define nop()  asm volatile("nop");
#define min(a,b) ((a)<(b) ? a : b)
#define max(a,b) ((a)>(b) ? a : b)

static inline uint8_t xtoi (char d) {
  return (isdigit (d)) ? d - 48 : toupper (d) - ('A' - 10);
}
