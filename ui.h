#if !defined __UI_H__
#define __UI_H__

void ui_init(void);
void ui_clock(void);  
void beep(uint16_t);
void led_init(void);
void led_usb_on(void);
void led_usb_off(void);
void rgb_led_on(bool, bool, bool);
void rgb_led_off(void);

#endif
