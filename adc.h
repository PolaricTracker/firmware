/*
 * $Id: adc.h,v 1.2 2008-12-31 01:17:49 la7eca Exp $
 * Reading analog/digital converter
 */
 

#if !defined __ADC_H__
#define __ADC_H__

#define ADC_CHANNEL_0 0x00
#define ADC_CHANNEL_1 0x01

void adc_enable(void);
void adc_disable(void);
float adc_get(uint8_t chan);

#endif /* __ADC_H__ */
