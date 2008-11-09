/*
 * $Id: defines.h,v 1.22 2008-11-09 23:32:18 la7eca Exp $ 
 */

#include <stdint.h>
#include <stdio.h>
#include <ctype.h>

// #define TRACKER_MK1     /* Define if compiling for the first lab model */
// #define USBKEY_TEST     /* Define if compiling for the USBKEY */

#if defined USBKEY_TEST || !defined TRACKER_MK1
#define AFSK_DAC_METHOD    /* Use 4-bit DAC for modulation */
#endif

#define TRUE   (1)
#define FALSE  (0)

/*
 *  Some assumptions about environment: 
 *   - timer resolution - 1/100 sec
 *   - Minimum time (sec) to get a position from GPS 
 *     (when tracker is already active)
 *   - Minimum time (sec) to transmit a report 
 */

#define TIMER_RESOLUTION 100 
#define GPS_FIX_TIME     4  
#define PACKET_TX_TIME   2



/********************************************
 * Note: The CPU frequency must be defined
 * in the makefile
 ********************************************/
 
#if defined USBKEY_TEST
#define SCALED_F_CPU F_CPU
#else
#define SCALED_F_CPU F_CPU
#endif



/*********************************************
 * Stack configuration (one stack per thread)
 *********************************************/

#define STACK_MAIN             90
#define STACK_LED              90
#define STACK_USBLISTENER      330
#define STACK_HDLCENCODER      150
#define STACK_HDLCENCODER_TEST 110
#define STACK_GPSLISTENER      330
#define STACK_TRACKER          300



/******************************************* 
 * Buffers, memory management
 *******************************************/
 
#define UART_BUF_SIZE	   16		
#define FBUF_SLOTS         32
#define FBUF_SLOTSIZE      16

#define AFSK_ENCODER_BUFFER_SIZE 128
#define AFSK_DECODER_BUFFER_SIZE 128
#define HDLC_DECODER_QUEUE_SIZE  4
#define HDLC_ENCODER_QUEUE_SIZE  4


/********************************************
 * LED blinking
 ********************************************/
 
extern uint8_t blink_length, blink_interval;
#define BLINK_NORMAL        { blink_length = 5; blink_interval = 95; }
#define BLINK_GPS_SEARCHING { blink_length = 45; blink_interval = 45; }


/*******************************************
 * ADC 
 *******************************************/
 
#define ADC_PRESCALER 0x06
#define ADC_REFERENCE 3.3


/*******************************************
 * 4 bit DAC
 *******************************************/
 
#if defined USBKEY_TEST 
#define DAC_PORT           PORTC
#define DAC_DDR            DDRC
#define DAC_MASK           0x0F;
#else
#define DAC_PORT           PORTD
#define DAC_DDR            DDRD
#define DAC_MASK           0xF0;
#endif


/********************************************
 * MCU port configuration 
 ********************************************/
                        
 /* Common */

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

#define PD3OUT_PORT        PORTD
#define PD3OUT_BIT         3
#define PD3OUT_DDR         DDRD
#define PD0OUT_PORT        PORTD
#define PD0OUT_BIT         0
#define PD0OUT_DDR         DDRD

#define BUTTON_PORT        PORTD
#define BUTTON_BIT         1
#define BUTTON_DDR         DDRD
#define BUTTON_PIN         PIND

#define OC3A_PORT          PORTC
#define OC3A_BIT           6
#define OC3A_DDR           DDRC


#if defined USBKEY_TEST  
 /* These are for the USBKEY configuration */     
#define BUZZER_PORT         PORTA
#define BUZZER_BIT          6
#define BUZZER_DDR          DDRA        
#define TXDATA_PORT     PORTB
#define TXDATA_BIT      2 
#define TXDATA_DDR      DDRB
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
 /* Tracker configuration (both MK1 and MK2) */
#define TXDATA_PORT     ADF7021_TXRXDATA_PORT
#define TXDATA_BIT      ADF7021_TXRXDATA_BIT
#define TXDATA_DDR      ADF7021_TXRXDATA_DDR
#define LED1_PORT       PORTA
#define LED1_BIT        7       
#define LED1_DDR        DDRA
#define LED2_PORT       PORTC
#define LED2_BIT        7
#define LED2_DDR        DDRC

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

 
#if defined TRACKER_MK1 

 /* Tracker MK1 configuration */
#define LED3_PORT       PORTA
#define LED3_BIT        0
#define LED3_DDR        DDRA

#define ADF7021_ON_DDR        DDRC
#define ADF7021_ON_PORT       PORTC
#define ADF7021_ON_PIN        PINC
#define ADF7021_ON_BIT        0
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


#else                  
 /* Tracker MK2 configuration (pilot series) */
#define BUZZER_PORT         PORTE
#define BUZZER_BIT          4
#define BUZZER_DDR          DDRE
 
#define VBATT_ADC_PORT     PORTF
#define VBATT_ADC_BIT      0
#define VBATT_ADC_DDR      DDRF
#define LED3_PORT       PORTA
#define LED3_BIT        6
#define LED3_DDR        DDRA
#define LED4_PORT 		PORTC
#define LED4_BIT			6
#define LED4_DDR			DDRC
#define LED5_PORT 		PORTC
#define LED5_BIT			5
#define LED5_DDR			DDRC

#define ADF7021_ON_DDR        DDRC
#define ADF7021_ON_PORT       PORTC
#define ADF7021_ON_PIN        PINC
#define ADF7021_ON_BIT        4
#define ADF7021_SCLK_DDR      DDRC
#define ADF7021_SCLK_PORT     PORTC
#define ADF7021_SCLK_PIN      PINC
#define ADF7021_SCLK_BIT      0
#define ADF7021_SREAD_DDR     DDRC
#define ADF7021_SREAD_PORT    PORTC
#define ADF7021_SREAD_PIN     PINC
#define ADF7021_SREAD_BIT     1
#define ADF7021_SDATA_DDR     DDRC
#define ADF7021_SDATA_PORT    PORTC
#define ADF7021_SDATA_PIN     PINC
#define ADF7021_SDATA_BIT     2
#define ADF7021_SLE_DDR       DDRC
#define ADF7021_SLE_PORT      PORTC
#define ADF7021_SLE_PIN       PINC
#define ADF7021_SLE_BIT       3

#endif
#endif



/********************************************************
 * ADF 7021 specific definitions
 ********************************************************/
 
#define ADF7021_XTAL        6400000 




/**********************************************
 * These are specific to the USB KEY.  
 **********************************************/
  
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


/************************************************
 * Bit fiddling macros
 ************************************************/

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
