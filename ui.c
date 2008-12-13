/*
 * $Id: ui.c,v 1.3 2008-12-13 11:43:12 la7eca Exp $
 *
 * Polaric tracker UI, using LEDs on top of tracker unit
 */
 
 
#include "defines.h"
#include <avr/io.h>
#include "kernel/kernel.h"
#include "kernel/timer.h"
#include <stdlib.h>
#include "config.h"
#include "ui.h"
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <inttypes.h>
#include <avr/interrupt.h>
#include "transceiver.h"
#include "gps.h"
#include "adc.h"


static bool usb_on = false;
static bool buzzer = false;

static void ui_thread(void);
static void batt_check_thread(void);


void ui_init()
{   
      /* DDR Registers */
      make_output(LED1);
      make_output(LED2);
      make_output(LED3);
      clear_port(LED1); 
      clear_port(LED2);
      make_output(LED4);
      make_output(LED5);
      make_output(BUZZER);     
      rgb_led_off();
      clear_port(BUZZER);
      usb_on = false;     
            
      /* Button */
      make_input(BUTTON);
      EICRA = (1<<ISC10);
      EIMSK = (1<<INT1);     
       
      /* Battery charging, etc. */
      make_output(HIGH_CHARGE);
      clear_port(HIGH_CHARGE);  
      make_input(EXT_CHARGER); 
      clear_port(EXT_CHARGER); /* No internal pull-up */
      
      THREAD_START(ui_thread, STACK_LED); 
      THREAD_START(batt_check_thread, 90); 
}



/*************************************************************************
 * Handler for on/off button
 *************************************************************************/

#define soft_reset()        \
do {                        \
    wdt_enable(WDTO_15MS);  \
    for(;;)  { }            \
} while(0)

 
#define BUTTON_TIME 200
static Timer button_timer;
static bool is_off = false;
static void onoff_handler(void);
static void sleepmode(void);


void turn_off()
{
    /* External devices should be turned off. Todo: USB */
    adf7021_power_off();
    gps_off();
    is_off = true; 
    sleepmode();
    EIMSK = (1<<INT1); 
    /*
     * Use pin-change interrupt to wake up the device if 
     * external charger is plugged in. We do not need an ISR for this
     * We just assume that one exists
     */
     set_bit (PCMSK0, PCINT6);
     set_bit (PCICR, PCIE0); 
}


static void onoff_handler()
{
    if (is_off) {
       is_off = false;
       soft_reset(); 
    }
    else 
       turn_off();
}  


static void sleepmode()
{
    if (is_off && !pin_is_high(EXT_CHARGER) && !usb_on && pin_is_high(BUTTON)) {
        clear_port(LED1);
        clear_port(LED2);
        rgb_led_off();
        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    }  
    else
        set_sleep_mode(SLEEP_MODE_IDLE);        
}


ISR(INT1_vect)
{ 
    nop();
    if (!pin_is_high(BUTTON)) {
       set_sleep_mode(SLEEP_MODE_IDLE);
       timer_set(&button_timer, BUTTON_TIME);
       timer_callback(&button_timer, onoff_handler);
    }   
    else {
       timer_cancel(&button_timer);
       sleepmode();
    }
}



/************************************************************************
 * To be called from system clock ISR
 * Rate: 2400 Hz
 ************************************************************************/

void ui_clock()
{
   if (buzzer)
      toggle_port(BUZZER);      
}



/************************************************************************
 * 1200 Hz BEEP
 * t - duration in 10ms units
 * s - A sequence of dashes '-' and dots '.' (morse code). 
 *     Space means pause
 ************************************************************************/
 
void beep(uint16_t t)
{
    buzzer = true;
    sleep(t);
    buzzer = false;
    clear_port(BUZZER);
}
 
void beeps(char* s)
{
    while (*s != 0) {
       if (*s == '.')
          beep(5);
       else if (*s == '-')
          beep(15);
       else
          sleep(10);
       sleep(5);  
       s++;
   }
}



/*************************************************************************
 * Indicate that USB is connected
 *************************************************************************/
 
void led_usb_on()
{
   usb_on = true;
   led_usb_restore();
}

/*************************************************************************
 * Indicate that USB is disconnected
 *************************************************************************/
 
void led_usb_off()
{
   usb_on = false;
   led_usb_restore();
}


/*************************************************************************
 * Make the led blue if usb is on
 *************************************************************************/
void led_usb_restore()
{
    rgb_led_off();
    if (usb_on)
      rgb_led_on(false, false, true);
}


/************************************************************************
 * Turn on specified RGB led(s)
 ************************************************************************/
 
void rgb_led_on(bool red, bool green, bool blue)
{
   rgb_led_off();
   if (red)
       clear_port(LED5);
   if (green)
       clear_port(LED3);
   if (blue)
       clear_port(LED4);    
}


/************************************************************************
 * Turn off all RGB leds
 ************************************************************************/
 
void rgb_led_off()
{
    set_port(LED3);
    set_port(LED4);
    set_port(LED5);
}



/*************************************************************************
 * Thread for periodic LED blinking 
 * Welcome and heartbeat. 
 *************************************************************************/
 
uint8_t blink_length, blink_interval;
Cond dont_blink;
 
static void ui_thread(void)
{       
    set_port(LED1);
    set_port(LED2);
    rgb_led_on(true,true,true);
    sleep(100);
    clear_port(LED1);    
    clear_port(LED2);
    rgb_led_off();
    
    beeps("--.- .-. ...-");   
    if (usb_on)
        rgb_led_on(false,false,true);
   
    BLINK_NORMAL;
    while (1) {
        set_port( LED1 );
        sleep(blink_length);
        clear_port( LED1 );
        sleep( is_off ? 500 : blink_interval );
    }
}
 
 

/**********************************************************************
 * Battery stuff
 **********************************************************************/

static bool batt_charged = false;


static void batt_check_thread()
{
    float voltage;
    uint8_t cbeep = 1, cusb = 1;
    
    while (true) 
    {
       adc_enable();
       voltage = adc_get(ADC_CHANNEL_0) * ADC_VBATT_DIVIDE;
       sleep(10);
       voltage += adc_get(ADC_CHANNEL_0) * ADC_VBATT_DIVIDE;
       voltage /= 2;
       adc_disable();
    
       if (voltage >= BATT_HIGHCHARGE_MAX && !batt_charged) {
          batt_charged = true;
          clear_port(HIGH_CHARGE);
       } 
       else
          if (voltage < BATT_LOWCHARGE_MIN || !batt_charged) {
             batt_charged = false;
             set_port(HIGH_CHARGE);
          }
       
       if (voltage < BATT_LOW_WARNING) 
          if (voltage < BATT_LOW_TURNOFF)
             turn_off();
          else {
             /* Bættery low warning */
             if (cbeep-- == 0) {
                 beeps("-...");
                 cbeep = 15;
             }
             rgb_led_off();
             rgb_led_on(true,false,true);
             sleep(40);
             led_usb_restore();
          }   
       
       /* External charger handler. Indicate when plugged in
        * even if device is "turned off" 
        */   
       make_output(EXT_CHARGER); 
       clear_port(EXT_CHARGER); /* Need to pull down due to hw design flaw */
       sleep(50);
       make_input(EXT_CHARGER);
       clear_port(EXT_CHARGER);
       sleep(5);
       if (pin_is_high(EXT_CHARGER)) {
          if (batt_charged) 
             rgb_led_on(false,true,false);
          else
             rgb_led_on(true,false,false);
       }
       else {
          /* If only USB is connected, go briefly to green every 3rd round, 
           * if battery is fully charged */
          if (usb_on && cusb-- == 0 && batt_charged) {
             rgb_led_on(false, true, false);
             sleep(50);
             cusb = 3;
          }
          led_usb_restore();   
          sleepmode();
       }   
       sleep(100);
    }   
}



