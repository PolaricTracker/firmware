/*
 * $Id: ui.c,v 1.9 2009-03-26 22:11:57 la7eca Exp $
 *
 * Polaric tracker UI, using buzzer and LEDs on top of tracker unit
 * Handle on/off button and battery charging.
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
#include "usb.h"


static bool usb_on = false;
static bool buzzer = false;

static void ui_thread(void);
static void batt_check_thread(void);
static void wakeup_handler(void);

static float _batt_voltage;
static bool _batt_charged = false;



void ui_init()
{   
      /* Enable wdt */
      wdt_enable(WDTO_4S);
      
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
      THREAD_START(batt_check_thread, STACK_BATT); 
}



/*************************************************************************
 * Handler for on/off button
 *************************************************************************/
 
#define ONOFF_TIME 200
#define PUSH_TIME 80

static Timer onoff_timer, push_timer;
bool is_off = false;
static void onoff_handler(void*);
static void sleepmode(void);


void turn_off()
{
    is_off = true; 
    sleepmode();
}


/* 
 * Called by batt_thread to handle situations where device is waked up
 * (but not necessarily turned on) by external power suppply. 
 */
bool autopower  __attribute__ ((section (".noinit")));
static void wakeup_handler()
{
   if (is_off && (pin_is_high(EXT_CHARGER) || usb_on || usb_con())) {
       wdt_enable(WDTO_4S);
       if(GET_BYTE_PARAM(AUTOPOWER)) {   
          is_off = false;
          autopower = true; /* Turn off if ext pwr is removed */
          soft_reset(); 
       }
   }    
   /* if (is_off && gps_hasWaiters())
        gps_on(); */
}



static void onoff_handler(void* x)
{
    if (is_off) {
       is_off = false;
       autopower = false;
       soft_reset(); 
    }
    else 
       turn_off();
}  



/* The last thing to be done before putting CPU to sleep:
 * Power down (almost) everything */
static bool _powerdown = false;
void powerdown_handler()
{
     if (!_powerdown)
         return;
    
     _powerdown = false;
    
     EIMSK = (1<<INT1); 
    /*
     * Use pin-change interrupt to wake up the device if 
     * external charger is plugged in. We do not need an ISR for this
     * We just assume that one exists
     */
     set_bit (PCMSK0, PCINT6);
     set_bit (PCICR, PCIE0); 
    
    /* External devices should be turned off. Todo: USB. 
     * Note that after this, the device should be reset when it is re-activated */
     autopower = false;
     adf7021_power_off();
     gps_off();
     clear_port(LED1);
     clear_port(LED2);
     rgb_led_off();
     wdt_disable();
}



static void sleepmode()
{
    if (is_off && !pin_is_high(EXT_CHARGER) && !usb_on && !usb_con() && pin_is_high(BUTTON)) {
        _powerdown = true;
        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    }
    else
        set_sleep_mode(SLEEP_MODE_IDLE);        
}



static uint8_t tmp_push_count = 0, push_count = 0;
static void push_thandler(void* x)
{
   push_count = tmp_push_count;
   tmp_push_count = 0;
   sleepmode();
}


bool buttdown = false;
/* Button pin change interrupt */
ISR(INT1_vect)
{ 
    nop(); 
    if (!pin_is_high(BUTTON)) {
       if (buttdown) return;
       buttdown = true;
       set_sleep_mode(SLEEP_MODE_IDLE);
       timer_set(&onoff_timer, ONOFF_TIME);
       timer_callback(&onoff_timer, onoff_handler, NULL);
       
       if (tmp_push_count > 0)
           timer_cancel(&push_timer); 
    }   
    else {
       if (!buttdown) return;
       buttdown = false;
       timer_cancel(&onoff_timer);
       
       tmp_push_count++;
       timer_set(&push_timer, PUSH_TIME);
       timer_callback(&push_timer, push_thandler, NULL); 
    }
}

void tracker_addObject();
void push_handler()
{
    if (is_off)
       return;
    if (push_count == 2)
        beeps("..---");
    else if (push_count == 3) {
        beeps("..-. ..-." );
        tracker_addObject();
    }
    else if (push_count == 4)
        beeps("....-");    
    else if (push_count == 5)
        beeps(".....");
    else if (push_count == 6)
        beeps("-....");      
    else if (push_count == 7)
        beeps("--..."); 
    else if (push_count == 8)
        beeps("---..");     
    else if (push_count == 9)
        beeps("----.");             
    push_count = 0;
    
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
  
void lbeep()
{
    static bool beeped;
    if (!is_off || beeped)
        return;
    buzzer = true;
    for (int i=0;i<400; i++) t_yield();
    buzzer = false;
    clear_port(BUZZER);
    beeped = true;
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



static bool pri_rgb_on = false;
/*************************************************************************
 * Make the led blue if usb is on
 *************************************************************************/
void led_usb_restore()
{
   if (pri_rgb_on) 
       return;
    rgb_led_off();
    if (usb_on)
      rgb_led_on(false, false, true);
}


/************************************************************************
 * Turn on specified RGB led(s)
 ************************************************************************/
 
void rgb_led_on(bool red, bool green, bool blue)
{
   if (pri_rgb_on) 
       return;
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
    if (pri_rgb_on)
       return;
    set_port(LED3);
    set_port(LED4);
    set_port(LED5);
}



/************************************************************************
 * Turn on priority RGB led(s). This cannot be overridden and can only
 * be turned off by pri_rgb_led_off()
 ************************************************************************/
 
void pri_rgb_led_on(bool red, bool green, bool blue)
{
   rgb_led_on(red,green,blue);
   pri_rgb_on = true;
}


void pri_rgb_led_off()
{
   pri_rgb_on = false;
   led_usb_restore();
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


float batt_voltage()
  {return _batt_voltage;}
  
  
static void batt_check_thread()
{
    uint8_t cbeep = 1, cusb = 1;
    
    while (true) 
    {
       /* Reset WDT */
       wdt_reset(); 
       
       /* Read battery voltage */
       adc_enable();
       _batt_voltage = adc_get(ADC_CHANNEL_0) * ADC_VBATT_DIVIDE;
       sleep(10);
       _batt_voltage += adc_get(ADC_CHANNEL_0) * ADC_VBATT_DIVIDE;
       _batt_voltage /= 2;
       adc_disable();
    
       if (_batt_voltage >= BATT_HIGHCHARGE_MAX && !_batt_charged) {
          _batt_charged = true;
          clear_port(HIGH_CHARGE);
       } 
       else
          if (_batt_voltage < BATT_LOWCHARGE_MIN || !_batt_charged) {
             _batt_charged = false;
             set_port(HIGH_CHARGE);
          }
       
       if (_batt_voltage <= BATT_LOW_WARNING) {
          if (_batt_voltage <= BATT_LOW_TURNOFF)
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
 
       if (((pin_is_high(EXT_CHARGER) /* && _batt_voltage >= 6.75 */ )  || usb_con()) && !usb_on) {
          if (_batt_charged) 
             rgb_led_on(false,true,false);
          else
             rgb_led_on(true,false,false);
       }
       else {
          /* If only USB is connected, go briefly to red every 3rd round, 
           * if battery is not fully charged */
          if (usb_on && cusb-- == 0 && !_batt_charged) {
             rgb_led_on(true, false, false);
             sleep(50);
             cusb = 3;
          }
         if (autopower && !pin_is_high(EXT_CHARGER) && !usb_on && !usb_con())
            turn_off();
         sleep(10);
         led_usb_restore();   
         sleepmode();
       }   
       
       sleep(100);
       /* Things to do if waked up by external charger */
       wakeup_handler();
       
       if (push_count > 0)
          push_handler();
    }   
}



