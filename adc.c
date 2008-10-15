#include <avr/io.h>
#include "adc.h"
#include "defines.h"
#include "kernel/kernel.h"
#include <avr/signal.h>


static Cond adc_ready;


void adc_enable(void)
{
    /*
     *  enable ADC. Default to single sample convert mode. 
     * Set prescaler 
     * enable ADC interrupts 
     */
    ADCSRA = (1<<ADEN) | ADC_PRESCALER | (1<<ADIE); 

    /* Use AVcc as reference voltage. Right-adjusted result  */ 
    ADMUX = (1<<REFS0); 
    cond_init(&adc_ready);
}



void adc_disable(void)
{
    ADCSRA &= ~(1<<ADIE | 1<<ADEN); 
}


/* 
 * Do a conversion on given channel. Result is in volts. 
 */
 
float adc_get(uint8_t chan)
{
    
    ADMUX |= (0x1F & chan);  /* Set channel */ 
    ADCSRA &= ~(1<<ADIF);    /* Clear complete flag */
    ADCSRA |= (1<<ADSC);     /* Start conversion */
    wait(&adc_ready);        /* Wait until result is ready */
      
    /* MUST READ ADCL BEFORE ADCH */
    return (ADCL | ADCH<<8) * ADC_REFERENCE / 0x03FF;
}




//! Interrupt handler for ADC complete interrupt.
ISR(ADC_vect)
{
    notify(&adc_ready);
}

