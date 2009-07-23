#if !defined __UI_H__
#define __UI_H__

void ui_init(void);
void ui_clock(void);  
void powerdown_handler(void);
void beep(uint16_t);
void lbeep(void);
void beeps(char*);
void led_init(void);
void led_usb_on(void);
void led_usb_off(void);
void led_usb_restore(void);
void rgb_led_on(bool, bool, bool);
void rgb_led_off(void);
void batt_extcharger_event(void);
float batt_voltage(void);
void turn_off(void);
void led_usb_restore(void);
void pri_rgb_led_on(bool, bool, bool);
void pri_rgb_led_off(void);

#endif
