#include <stdint.h>

#define USBKEY_TEST  /* Comment out if compiling for the real Polaric Tracker */

#define TRUE   (1)
#define FALSE  (0)


/********************************************
 * Note: The CPU frequency must be defined
 * in the makefile
 ********************************************/
 
#if defined USBKEY_TEST
#define SCALED_F_CPU   (F_CPU / 2)
#else
#define SCALED_F_CPU F_CPU
#endif


/******************************************* 
 * Buffers, memory management
 *******************************************/
 
#define UART_BUF_SIZE	(16)		
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
 
#define DCD_LED_PORT    LED1_PORT
#define DCD_LED_BIT     LED2_BIT

#define TXDATA_PORT     PORTB
#define TXDATA_BIT      PB2 
#define TXDATA_DDR      DDB2
#define BUZZER_PORT     PORTA
#define BUZZER_BIT      PA6
#define BUZZER_DDR      DDRA

#if defined USBKEY_TEST

#define LED1_PORT       PORTA
#define LED1_BIT        PA7
#define LED1_DDR        DDRA
#define LED2_PORT       PORTC
#define LED2_BIT        PC7
#define LED2_DDR        DDRC
#define LED3_PORT       PORTA
#define LED3_BIT        PA0
#define LED3_DDR        DDRA

#else

#define TXDATA_PORT     BUZZER_PORT 
#define TXDATA_BIT      BUZZER_BIT 
#define TXDATA_DDR      BUZZER_DDR
#define LED1_PORT       PORTA
#define LED1_BIT        PA7
#define LED1_DDR        DDRA
#define LED2_PORT       PORTC
#define LED2_BIT        PC7
#define LED2_DDR        DDRC
#define LED3_PORT       PORTA
#define LED3_BIT        PA0
#define LED3_DDR        DDRA

#endif




/*
 * ADF 7021 specific definitions
 */
#define ADF7021_XTAL        6400000 
#define ADF7021_SCLK_PORT   PORTC
#define ADF7021_SCLK_DDR    DDRC
#define ADF7021_SCLK_BIT    PC4
#define ADF7021_SDATA_PORT  PORTC
#define ADF7021_SDATA_DDR   DDRC
#define ADF7021_SDATA_BIT   PC2
#define ADF7021_SREAD_PORT  PORTC
#define ADF7021_SREAD_DDR   DDRC
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
