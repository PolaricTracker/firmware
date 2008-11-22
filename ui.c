/*
 * $Id: ui.c,v 1.2 2008-11-22 19:12:57 la7eca Exp $
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

static bool usb_on = false;
static bool buzzer = false;

static void ui_thread(void);


void ui_init()
{   
      /* DDR Registers */
      make_output(LED1);
      make_output(LED2);
      make_output(LED3);
      clear_port(LED1); 
      clear_port(LED2);
#if defined TRACKER_MK1 
      clear_port(LED3);
#else
      make_output(LED4);
      make_output(LED5);
      make_output(BUZZER);      
      set_port(LED3);
      set_port(LED4);
      set_port(LED5);
      clear_port(BUZZER);
#endif

      THREAD_START(ui_thread, STACK_LED);  
}



/************************************************************************
 * To be called from system clock ISR
 * Rate: 2400 Hz
 ************************************************************************/

void ui_clock()
{
#if !defined TRACKER_MK1
   if (buzzer)
      toggle_port(BUZZER);
#endif       
}



/************************************************************************
 * 1200 Hz BEEP
 * t - duration in 10ms units
 ************************************************************************/
 
void beep(uint16_t t)
{
#if !defined TRACKER_MK1
    buzzer = true;
    sleep(t);
    buzzer = false;
    clear_port(BUZZER);
#endif 
}
 
void beeps(char* s)
{
#if !defined TRACKER_MK1
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
#endif 
}



/*************************************************************************
 * Indicate that USB is connected
 *************************************************************************/
 
void led_usb_on()
{
#if defined TRACKER_MK1
   set_port(LED3);
#else
  rgb_led_on(false,false,true);
#endif
   usb_on = true;
}

/*************************************************************************
 * Indicate that USB is disconnected
 *************************************************************************/
 
void led_usb_off()
{
#if defined TRACKER_MK1
   clear_port(LED3);
#else
   rgb_led_off();
#endif
   usb_on = false;
}



/************************************************************************
 * Turn on specified RGB led(s)
 ************************************************************************/
 
void rgb_led_on(bool red, bool green, bool blue)
{
#if !defined TRACKER_MK1
   if (red)
       clear_port(LED5);
   if (green)
       clear_port(LED3);
   if (blue)
       clear_port(LED4);
#endif        
}


/************************************************************************
 * Turn off all RGB leds
 ************************************************************************/
 
void rgb_led_off()
{
#if !defined TRACKER_MK1
    set_port(LED3);
    set_port(LED4);
    set_port(LED5);
#endif     
}



/*************************************************************************
 * Thread for periodic LED blinking 
 * Welcome and heartbeat. 
 *************************************************************************/
 
uint8_t blink_length, blink_interval;
 
static void ui_thread(void)
{       
#if defined TRACKER_MK1
    /* Blink both LEDS when turned on */
    set_port(LED1);
    set_port(LED2);
    sleep(100);
    clear_port(LED1);
    clear_port(LED2);
#else
    beeps("--.- .-. ...-");
    set_port(LED1);
    sleep(30);
    clear_port(LED1);
    set_port(LED2);
    sleep(30);
    clear_port(LED2);
    rgb_led_off();
    rgb_led_on(true,false,false);
    sleep(30);
    rgb_led_off();
    rgb_led_on(false,true,false);
    sleep(30);
    rgb_led_off();
    rgb_led_on(false,false,true);
    sleep(30);
    rgb_led_off();
    sleep(50);
    if (usb_on)
        rgb_led_on(false,false,true);
#endif           
    BLINK_NORMAL;
    while (1) {
        set_port( LED1 );
        sleep(blink_length);
        clear_port( LED1 );
        sleep(blink_interval);
    }
}
 
 
