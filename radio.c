/* $Id: */

#include "defines.h"
#include "config.h"
#include "transceiver.h"
#include "kernel/timer.h"
#include "hdlc.h"

 
static int count = 0; 
static void _radio_setup(void);


/******************************************************
 * Need radio - turn it on if not already on
 ******************************************************/
 
void radio_require(void)
{
    if (++count == 1) {
        _radio_setup();
        adf7021_power_on();
    }
}


/*******************************************************
 * Radio not needed any more - turn it off if no others
 * need it
 *******************************************************/
 
void radio_release(void)
{
    if (--count == 0) {
       /* 
        * Before turning off transceiver, wait until
        * Packet is sent and transmitter is turned off. 
        */
       sleep(50);
       hdlc_wait_idle();
       adf7021_wait_tx_off();
       adf7021_power_off();
    }
}


/***************************************
 * Force re-initialisation of radio
 ***************************************/
 
void radio_setup(void)
{
   if (count > 0) {
      adf7021_power_off();
      _radio_setup();
      adf7021_power_on();
   } 
}

adf7021_setup_t trx_setup;


/**************************************************************************
 * Setup the adf7021 tranceiver.
 *   - We may move this to a separate source file or to config.c ?
 *   - Parts of the setup may be stored in EEPROM?
 **************************************************************************/
static void _radio_setup(void)
{
    uint32_t freq; 
    int16_t  fcal;
    double power; 
    uint16_t dev;
    uint16_t afc;
    
    GET_PARAM(TRX_FREQ, &freq);
    GET_PARAM(TRX_CALIBRATE, &fcal);
    GET_PARAM(TRX_TXPOWER, &power);
    GET_PARAM(TRX_AFSK_DEV, &dev);
    GET_PARAM(TRX_AFC, &afc);

    adf7021_setup_init(&trx_setup);
    adf7021_set_frequency (&trx_setup, freq+fcal);
    trx_setup.vco_osc.xosc_enable = true;
    trx_setup.test_mode.analog = ADF7021_ANALOG_TEST_MODE_RSSI;
    
    adf7021_set_data_rate (&trx_setup, 4400);    
    adf7021_set_modulation (&trx_setup, ADF7021_MODULATION_OVERSAMPLED_2FSK, dev);
    adf7021_set_power (&trx_setup, power, ADF7021_PA_RAMP_OFF);   
    adf7021_set_demodulation (&trx_setup, ADF7021_DEMOD_2FSK_LINEAR);
    adf7021_enable_AFC(&trx_setup, afc);
    trx_setup.demod.if_bw = ADF7021_DEMOD_IF_BW_12_5;
    adf7021_set_post_demod_filter (&trx_setup, 3500); 
    ADF7021_INIT_REGISTER(trx_setup.test_mode, ADF7021_TEST_MODE_REGISTER);
    trx_setup.test_mode.rx = ADF7021_RX_TEST_MODE_LINEAR_SLICER_ON_TxRxDATA;
    adf7021_init (&trx_setup);

}
