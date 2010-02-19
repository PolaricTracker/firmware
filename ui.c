/*
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


static bool   usb_on = false;
static bool   buzzer = false;
static Mutex  beep_mutex;
static float  _batt_voltage;
static bool   _batt_charged = false;

static bool _powerdown = false;  /* True if requested to go to power-off */
static bool asleep     = false;  /* True if device is actually put into power-off mode */
bool autopower  __attribute__ ((section (".noinit")));
                                 /* True if we want device to be "turned on" by external charger */
                                 
static void ui_thread(void);
static void report_batt(void);
static void batt_check_thread(void);
static void wakeup_handler(void);
static void enable_ports_offmode();

#define ENABLE_BUTTON_INT  EIMSK |= (1<<INT1)
#define DISABLE_BUTTON_INT EIMSK &= ~(1<<INT1)

void ui_init()
{   
      /* Enable wdt */
      wdt_enable(WDTO_4S);

      /* Disable USB charger */
      make_output(USB_CHARGER);
      clear_port(USB_CHARGER);
      
      make_output(BUZZER);     
      rgb_led_off();
      clear_port(BUZZER);
      usb_on = false;
      mutex_init(&beep_mutex);
      
      EICRA |= (1<<ISC10);
      ENABLE_BUTTON_INT;
      
      make_float(PE2_IN);
      make_float(BUTTON);
      make_float(EXT_CHARGER); 
      enable_ports_offmode();
      clear_port(EXT_CHARGER); /* No internal pull-up */
      THREAD_START(ui_thread, STACK_LED); 
      THREAD_START(batt_check_thread, STACK_BATT); 
}



/*************************************************************************
 * Port to be enabled, when MCU wakes up for charging.
 *************************************************************************/

static void enable_ports_offmode()
{
      make_output(HIGH_CHARGE);
      clear_port(HIGH_CHARGE);
      make_output(LED1);
      make_output(LED2);
      make_output(LED3);
      make_output(LED4);
      make_output(LED5);
      clear_port(LED1);
      clear_port(LED2);
      rgb_led_off();
}


/*************************************************************************
 * Handler routines for on/off pushbutton
 *************************************************************************/
 
#define ONOFF_TIME 200
#define PUSH_TIME 80

static Timer onoff_timer, push_timer;
bool is_off = false;
static void onoff_handler(void*);
static void sleepmode(void);

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
    if (!pin_is_high(BUTTON)) {
       if (!buttdown) {
          buttdown = true;
          set_sleep_mode(SLEEP_MODE_IDLE);
       
          timer_set(&onoff_timer, ONOFF_TIME);
          timer_callback(&onoff_timer, onoff_handler, NULL);
          if (tmp_push_count > 0)
             timer_cancel(&push_timer);  
       }
    }   
    else {
       if (buttdown) {
          buttdown = false;
       
          timer_cancel(&onoff_timer);
          tmp_push_count++;
          timer_set(&push_timer, PUSH_TIME);
          timer_callback(&push_timer, push_thandler, NULL); 
       }
    }     
}


/*
 * Called when pushbutton is pressed for more than 2 seconds.
 * Turn off or on
 */
 
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



/************************************************************************
 * Execute pushbutton commands
 *  3 times - send aprs report
 *  4 times - add (and send) object report
 *  5 times - delete all objects. 
 ************************************************************************/
 
void tracker_posReport(void);
void tracker_addObject(void);
void tracker_clearObjects(void);


void push_handler()
{ 
    if (is_off)
        return;
       
    else if (push_count == 2) 
        report_batt();
    else if (push_count == 3) {
        beeps(".-.");   
        sleep(100); 
        tracker_posReport();    
    }
    else if (push_count == 4) {
        beeps("..-.");  
        tracker_addObject();
    }
    else if (push_count == 5) {
        beeps("..-. -.-.");
        tracker_clearObjects();
    }
    push_count = 0;
}


void report_batt()
{
   if (batt_voltage() > 6.55)
        pri_rgb_led_on(false, true, false);
   else if (batt_voltage() > 6.10)
        pri_rgb_led_on(true, true, false);
   else
        pri_rgb_led_on(true, false, false);
   sleep(150);
   pri_rgb_led_off();
   sleep(30);
}



/***************************************************************************** 
 * Called by batt_thread to handle situations where device is waked up
 * (but not necessarily turned on) by external power suppply. 
 *****************************************************************************/
 
static void wakeup_handler()
{
   if (asleep && is_off && (pin_is_high(EXT_CHARGER) || usb_on || usb_con())) {
       wdt_enable(WDTO_4S);
       asleep = false;
       enable_ports_offmode();
       if(GET_BYTE_PARAM(AUTOPOWER)) {   
          is_off = false;
          autopower = true; /* Turn off if ext pwr is removed */
          soft_reset(); 
       }
   }    
}



/****************************************************************************
 * Routines to turn off the device.
 * 
 ****************************************************************************/

void turn_off()
{
    is_off = true; 
    sleepmode();
}


/*
 * The last thing to be done before putting CPU to sleep:
 * Power down (almost) everything. Called from root thread (see main.c)
 */
void powerdown_handler()
{
     if (!_powerdown)
         return;
    
     _powerdown = false;
     make_float(EXT_CHARGER);

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
     radio_release();
     adf7021_power_off (); /* Just in case */
     gps_off();
     clear_port(LED1);
     clear_port(LED2);
     rgb_led_off();

     asleep = true;
     wdt_disable();
     set_sleep_mode(SLEEP_MODE_PWR_DOWN);
}


/*
 * Turn off device when is_off is true and not connected to charger.
 * Called periodically from batt_check_thread (see below)
 */
static void sleepmode()
{
    if (is_off && !pin_is_high(EXT_CHARGER) && !usb_on && !usb_con() && pin_is_high(BUTTON)) {
        _powerdown = true;
        /* powerdown_handler will be called as the last thing */
    }
    else
        set_sleep_mode(SLEEP_MODE_IDLE);        
}






/************************************************************************
 * 1200 Hz BEEP (sound signal)
 * t - duration in 10ms units
 * s - A sequence of dashes '-' and dots '.' (morse code). 
 *     Space means pause
 ************************************************************************/
 
static void _beep(uint16_t t)
{
    buzzer = true;
    sleep(t);
    buzzer = false;
    clear_port(BUZZER);
}
  

/*
 * Special version, to be called from root thread before going to poweroff-mode
 * cannot block here. Use spinlock instead.
 */
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


/* Beep for 10 * t ms */
void beep(uint16_t t)
{
    mutex_lock(&beep_mutex);
    _beep(t);
    mutex_unlock(&beep_mutex);
}


/* Morse code */
void beeps(char* s)
{
    mutex_lock(&beep_mutex);
    while (*s != 0) {
       if (*s == '.')
          _beep(5);
       else if (*s == '-')
          _beep(15);
       else
          sleep(10);
       sleep(5);  
       s++;
   }
   mutex_unlock(&beep_mutex);
}


/*
 * To be called from system clock ISR to generate sound
 * Rate: 2400 Hz
 */

void ui_clock()
{
   if (buzzer)
      toggle_port(BUZZER);       
}


void beep_lock()    {mutex_lock(&beep_mutex);}
void beep_unlock()  {mutex_unlock(&beep_mutex);}



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

    /* 'QRV' in morse code */
    wdt_reset();
    if ( GET_BYTE_PARAM(BOOT_SOUND) ) {
        beeps("--.-"); 
        wdt_reset(); 
        beeps(" .-. ...-");
        wdt_reset();
    }
    report_batt(); 
    if (usb_on)
        rgb_led_on(false,false,true);
   
    BLINK_NORMAL;
    while (1) {
        set_port( LED1 );        
        sleep(blink_length);
        clear_port( LED1 );
        sleep( is_off ? 250 : blink_interval );
        wdt_reset();
        if (is_off) 
          { sleep(250); wdt_reset(); }
    }
}
 
 

/**********************************************************************
 * Thread that mainly controls charging of battery.
 * It also resets WDT and runs pushbutton commands (including turn-off) 
 **********************************************************************/


float batt_voltage()
  {return _batt_voltage;}
  

extern Timer usb_startup;

static void batt_check_thread()
{
    uint8_t cbeep = 1, cusb = 1;
    while (true) 
    {  
       /* Read battery voltage */
       adc_enable();
       _batt_voltage = adc_get(ADC_CHANNEL_0) * ADC_VBATT_DIVIDE;
       sleep(10);
       _batt_voltage += adc_get(ADC_CHANNEL_0) * ADC_VBATT_DIVIDE;
       _batt_voltage /= 2;
       adc_disable();
       
       /* If battery is fully charged, charge current should be low. If not, it should
        * be high
        */
       if (_batt_voltage >= BATT_HIGHCHARGE_MAX && !_batt_charged) {
          _batt_charged = true;
          clear_port(HIGH_CHARGE);
       } 
       else
          if (_batt_voltage < BATT_LOWCHARGE_MIN || !_batt_charged) {
             _batt_charged = false;
             set_port(HIGH_CHARGE);
          }
      
       /* Warn or turn off device if battery voltage is too low */
       if (_batt_voltage <= BATT_LOW_WARNING) {
          if (_batt_voltage <= BATT_LOW_TURNOFF)
             turn_off();
          else {
             /* Battery low warning */
             if (cbeep-- == 0) {
                 beeps("-...");
                 cbeep = 15;
             }
             rgb_led_off();
             rgb_led_on(true,false,false);
             sleep(40);
             led_usb_restore();
          }   
       }
       
        /*
        * External charger handler. Indicate when plugged in
        * even if device is "turned off" 
        */
       make_output(EXT_CHARGER); 
       clear_port(EXT_CHARGER);   /* Need to pull down due to hw design flaw */
       sleep(20);
       make_float(EXT_CHARGER);
       sleep(10);



       /* LED indication of charging status */
       if (((pin_is_high(EXT_CHARGER) )  || usb_con()) && !usb_on) {
          if (_batt_charged) 
             rgb_led_on(false,true,false); /* Green if fully charged */
          else
             rgb_led_on(true,false,false); /* Red if still charging */
       }
       else {
          /* If only USB is connected, go briefly to red every 3rd round, 
          * if battery is not fully charged */
          if (usb_on && cusb-- == 0 && !_batt_charged) {
             rgb_led_on(true, false, false);
             sleep(20);
             cusb = 3;
          }
          if (autopower && !pin_is_high(EXT_CHARGER) && !usb_on && !usb_con())
             turn_off();
      
          sleep(10);
          led_usb_restore();   
          /* Turn off device if told to */
          sleepmode();
       }   
       
       /*
        * Turn on USB charger if connected and other charger is not used. 
        * otherwise, turn it off 
        */
       if (usb_con() && !pin_is_high(EXT_CHARGER)) {
          timer_wait(&usb_startup);  /* Wait while USB is starting up */
         // set_port(USB_CHARGER);
         make_float(USB_CHARGER);
       }
       else {
          make_output(USB_CHARGER);
          clear_port(USB_CHARGER);
       }      
          
       sleep(100);
       /* Things to do if waked up by external charger */
       wakeup_handler();

       if (push_count > 0) 
          push_handler();
    }   
}



