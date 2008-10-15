#if !defined __ADC_H__
#define __ADC_H__

#define ADC_CHANNEL_0 0x00
#define ADC_CHANNEL_1 0x01

void adc_enable(void);
void adc_disable(void);
float adc_get(uint8_t chan);

#endif /* __ADC_H__ */
