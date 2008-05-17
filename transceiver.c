/* $Id: transceiver.c,v 1.11 2008-05-17 23:38:18 la7eca Exp $ */

#include <avr/io.h>
#include <math.h>
#include "defines.h"
#include "timer.h"
#include "transceiver.h"

#define MINIMUM_N 23

#include "stream.h"

bool adf7021_enabled = false;
bool adf7021_tx_enabled = false;

static adf7021_setup_t* setup;


void adf7021_setup_init(adf7021_setup_t *setup) {
  memset (setup, 0, sizeof (adf7021_setup_t));

  /* Setup to do coarse filter calibration on every power on */
  ADF7021_INIT_REGISTER(setup->if_filter, ADF7021_IF_FILTER_REGISTER);
  setup->if_filter.calibrate = true;
  setup->if_filter.divider = ADF7021_XTAL / 50e3;
}



void adf7021_set_frequency (adf7021_setup_t *setup, uint32_t freq)
{
  ADF7021_INIT_REGISTER(setup->vco_osc, ADF7021_VCO_OSC_REGISTER);

  /* Ref. page 24 in then datasheet for the following settings. There
     seems to be some inconsistencies between the text and table 9.
     These setting are according to the table. */
  if (freq < 431e6 || (freq > 475e6 && freq < 862e6))
    setup->vco_osc.vco_inductor = ADF7021_VCO_INDUCTOR_EXTERNAL;

  if (freq <= 200e6 /* Text says 325MHz */ ||
      (setup->vco_osc.vco_inductor = ADF7021_VCO_INDUCTOR_INTERNAL &&
       freq >= 431e6 && freq <= 475e6))
    setup->vco_osc.rf_div_2 = true;

  if (setup->vco_osc.vco_inductor == ADF7021_VCO_INDUCTOR_INTERNAL) {
    setup->vco_osc.vco_bias = ADF7021_VCO_BIAS_CURRENT_mA (2.0);
    if (freq > 900e6 || (freq > 450 && freq < 470))
      setup->vco_osc.vco_adjust = ADF7021_VCO_ADJUST_UP_3;
    else
      setup->vco_osc.vco_adjust = ADF7021_VCO_ADJUST_NORMAL;
  } else
    if (freq > 450e6)
      setup->vco_osc.vco_bias = ADF7021_VCO_BIAS_CURRENT_mA (1.0);
    else
    if (freq > 200e6)
      setup->vco_osc.vco_bias = ADF7021_VCO_BIAS_CURRENT_mA (0.75);
    else
      setup->vco_osc.vco_bias = ADF7021_VCO_BIAS_CURRENT_mA (0.5);

  setup->vco_osc.clockout_divide = 0xF; // divide by 30
  setup->vco_osc.vco_enable = true;

  // TODO: figure out when to use xtal doubler
  setup->vco_osc.xtal_doubler = false;


  /* Setup N register for RX */
  ADF7021_INIT_REGISTER(setup->rx_n, ADF7021_N_REGISTER);
  setup->rx_n.tx_disable = true;
 
  double n;
  uint32_t pfd_div;
  setup->vco_osc.r_counter = 0;
  do {
    setup->vco_osc.r_counter++;
    pfd_div = adf7021_pfd_freq (setup) >> (setup->vco_osc.rf_div_2 ? 1 : 0);    
    n = (double)(freq - 100e3) / pfd_div;
    setup->rx_n.integer = n;
    setup->rx_n.fractional = (n - floor (n)) * 32768.0 + 0.5;
  } while (setup->rx_n.integer < MINIMUM_N);

  
  /* Setup N register for TX */
  ADF7021_INIT_REGISTER(setup->tx_n, ADF7021_N_REGISTER);
  n = (double)freq / pfd_div;
  setup->tx_n.integer = n;
  setup->tx_n.fractional = (n - floor (n)) * 32768.0 + 0.5;
}



void adf7021_set_data_rate (adf7021_setup_t *setup, uint16_t data_rate)
{
  ADF7021_INIT_REGISTER(setup->clock, ADF7021_CLOCK_REGISTER);

  /* Set baseband offset frequency between 1MHz and 2MHz */
  setup->clock.bbos_clk_divide = 0;
  while (ADF7021_XTAL / (1 << (setup->clock.bbos_clk_divide + 2)) < 1e6 ||
         ADF7021_XTAL / (1 << (setup->clock.bbos_clk_divide + 2)) > 2e6)
    setup->clock.bbos_clk_divide++;

  /* Set DEMOD_CLK between 2MHz and 15MHz */
  setup->clock.dem_clk_divide = 1;
  while (ADF7021_XTAL / setup->clock.dem_clk_divide > 15e6)
    setup->clock.dem_clk_divide++;

  /* Set CDR_CLK to 32 * data_rate */
  setup->clock.cdr_clk_divide = ADF7021_XTAL / setup->clock.dem_clk_divide / (data_rate * 32);

  /* Set SEQ_CLK to 100kHz */
  setup->clock.seq_clk_divide = ADF7021_XTAL / 100e3;
  
  /* Set AGC_CLK to 10kHz */
  setup->clock.agc_clk_divide = ADF7021_XTAL / (setup->clock.seq_clk_divide * 10e3);
  
  setup->data_rate = data_rate;
}




void adf7021_set_modulation (adf7021_setup_t *setup, adf7021_modulation_t scheme, uint16_t deviation)
{
  ADF7021_INIT_REGISTER(setup->modulation, ADF7021_MODULATION_REGISTER);

  setup->modulation.tx_frequency_deviation =
    ((uint32_t)deviation * 1 << 16) /
    (adf7021_pfd_freq (setup) >> (setup->vco_osc.rf_div_2 ? 1 : 0));

  setup->modulation.scheme = scheme;
  setup->deviation = deviation;
}




void adf7021_set_if_filter_fine_calibration (adf7021_setup_t *setup, double tone_cal_time)
{
  ADF7021_INIT_REGISTER (setup->if_filter_cal, ADF7021_IF_FILTER_CAL_REGISTER);

  setup->if_filter_cal.fine_calibrate = true;
  setup->if_filter_cal.lower_tone_divide = ADF7021_XTAL / 65.8e3;
  setup->if_filter_cal.upper_tone_divide = ADF7021_XTAL / 131.5e3;
  setup->if_filter_cal.dwell_time = tone_cal_time * setup->clock.seq_clk_divide;
}




void adf7021_set_post_demod_filter (adf7021_setup_t *setup, uint16_t cutoff)
{
  setup->demod.post_demod_bw = (double)(1 << 11) * M_PI * cutoff / (ADF7021_XTAL / setup->clock.dem_clk_divide);
}




void adf7021_set_demodulation (adf7021_setup_t *setup, adf7021_demod_scheme_t scheme)
{
  ADF7021_INIT_REGISTER(setup->demod, ADF7021_DEMOD_REGISTER);

  setup->demod.scheme = scheme;

  uint32_t k;
  switch (scheme) {
  case ADF7021_DEMOD_3FSK:
    k = 100e3 / (2 * setup->deviation) + 0.5; break;
  case ADF7021_DEMOD_4FSK: // TODO
  default:
    k = 100e3 / setup->deviation + 0.5;
  }
  setup->demod.discriminator_bw = ((ADF7021_XTAL / setup->clock.dem_clk_divide) * k) / 400e3;

  /* Setup post demodulator filter according to table 19 page 34 */
  uint16_t cutoff;
  switch (scheme) {
  case ADF7021_DEMOD_3FSK: cutoff = setup->data_rate; break;
  case ADF7021_DEMOD_4FSK: cutoff = 1.6 * setup->data_rate; break;
  default: cutoff = 0.75 * setup->data_rate;
  }
  adf7021_set_post_demod_filter (setup, cutoff);
}




void adf7021_set_power (adf7021_setup_t *setup, double dBm, adf7021_pa_ramp_t ramp)
{
  setup->modulation.pa_level = (uint8_t)(dBm * (1 / ADF7021_PA_LEVEL_STEP) + 0.5) + 1;
  setup->modulation.pa_ramp = ramp;
  
  if (dBm > 10)
    setup->modulation.pa_bias = ADF7021_PA_RAMP_11_uA;
  else
    setup->modulation.pa_bias = ADF7021_PA_RAMP_9_uA;

  setup->modulation.pa_enable = true;

  /* Compute PA ramp time */
  double bit_time = 1000 / setup->data_rate;
  switch (ramp) {
  case ADF7021_PA_RAMP_OFF:      setup->ramp_time = 0; break;
  case ADF7021_PA_RAMP_256_BITS: setup->ramp_time = bit_time * 256; break;
  case ADF7021_PA_RAMP_128_BITS: setup->ramp_time = bit_time * 128; break;
  case ADF7021_PA_RAMP_64_BITS:  setup->ramp_time = bit_time * 64; break;
  case ADF7021_PA_RAMP_32_BITS:  setup->ramp_time = bit_time * 32; break;
  case ADF7021_PA_RAMP_16_BITS:  setup->ramp_time = bit_time * 16; break;
  case ADF7021_PA_RAMP_8_BITS:   setup->ramp_time = bit_time * 8; break;
  case ADF7021_PA_RAMP_4_BITS:   setup->ramp_time = bit_time * 4; break;
  }
  setup->ramp_time++;
}




void adf7021_init (adf7021_setup_t* s)
{
  adf7021_enabled = adf7021_tx_enabled = false;
  
  make_output (PD3OUT);
  make_output (EXTERNAL_PA_ON);
  make_output (ADF7021_ON);
  make_output (ADF7021_SCLK);
  make_output (ADF7021_SLE);
  make_output (ADF7021_SDATA);

  make_input  (ADF7021_SREAD); 


  make_input   (ADF7021_MUXOUT);  
  make_output  (ADF7021_TXRXDATA);
  make_output  (ADF7021_TXRXCLK); 

  
  /* Just to be on the safe side */
  clear_port (EXTERNAL_PA_ON);
  clear_port (ADF7021_ON);
  set_port (PD3OUT);
  
  setup = s;
}




void adf7021_power_on ()
{  
  set_port (ADF7021_ON);  
  
  /* Wait for transceiver to become ready */
  ADF7021_MUXOUT_WAIT ();
  
  /* Power down unused parts of the transceiver */
  if (ADF7021_REGISTER_IS_INITIALIZED (setup->power_down))
     adf7021_write_register (ADF7021_REGISTER_DEREF (setup->power_down));
     
  /* Turn on VCO */
  adf7021_write_register (ADF7021_REGISTER_DEREF (setup->vco_osc));
  sleep (1); // Minimum 700 µs
  
  /* Turn on TX/RX clock */
  adf7021_write_register (ADF7021_REGISTER_DEREF (setup->clock));

  /* Enable PLL and wait for lock */
  setup->rx_n.muxout = ADF7021_MUXOUT_DIGITAL_LOCK_DETECT;
  adf7021_write_register (ADF7021_REGISTER_DEREF (setup->rx_n));
  ADF7021_MUXOUT_WAIT ();

  /* Set MUXOUT to indicate when filter calibration has completed. */
  setup->rx_n.muxout = ADF7021_MUXOUT_FILTER_CAL_COMPLETE;
  adf7021_write_register (ADF7021_REGISTER_DEREF (setup->rx_n));

  /* Setup IF filter */
  if (ADF7021_REGISTER_IS_INITIALIZED (setup->if_filter_cal))
    /* Will do fine calibration */
    adf7021_write_register (ADF7021_REGISTER_DEREF (setup->if_filter_cal));
  adf7021_write_register (ADF7021_REGISTER_DEREF (setup->if_filter));

  /* Wait for filter calibration to complete */
  if (setup->if_filter.calibrate)
    ADF7021_MUXOUT_WAIT ();

  // TODO: readback and store fine calibration adjustment
  
  /* Setup frame synchronization */
  if (ADF7021_REGISTER_IS_INITIALIZED (setup->swd_word))
    adf7021_write_register (ADF7021_REGISTER_DEREF (setup->swd_word));
  if (ADF7021_REGISTER_IS_INITIALIZED (setup->swd_threshold))
    adf7021_write_register (ADF7021_REGISTER_DEREF (setup->swd_threshold));
    
  /* Setup demodulation */
  if (ADF7021_REGISTER_IS_INITIALIZED (setup->mfsk_demod))
    adf7021_write_register (ADF7021_REGISTER_DEREF (setup->mfsk_demod));
  if (ADF7021_REGISTER_IS_INITIALIZED (setup->agc))
    adf7021_write_register (ADF7021_REGISTER_DEREF (setup->agc));
  if (ADF7021_REGISTER_IS_INITIALIZED (setup->afc))
    adf7021_write_register (ADF7021_REGISTER_DEREF (setup->afc));
  adf7021_write_register (ADF7021_REGISTER_DEREF (setup->demod));

  /* Setup modulation */
  adf7021_write_register (ADF7021_REGISTER_DEREF (setup->modulation));

  /* Setup test modes */
  if (ADF7021_REGISTER_IS_INITIALIZED (setup->test_dac))
    adf7021_write_register (ADF7021_REGISTER_DEREF (setup->test_dac));
  if (ADF7021_REGISTER_IS_INITIALIZED (setup->test_mode))
    adf7021_write_register (ADF7021_REGISTER_DEREF (setup->test_mode));

}



void adf7021_power_off ()
{
  adf7021_enabled = adf7021_tx_enabled = false;
  clear_port (EXTERNAL_PA_ON);
  set_port (PD3OUT);

  clear_port (ADF7021_ON);
}



void adf7021_enable_tx ()
{
  /* Turn on external PA */
//  set_port (EXTERNAL_PA_ON);  // FIXME
//  clear_port(PD3OUT);
   
  /* Enable transmit mode */
  adf7021_write_register (ADF7021_REGISTER_DEREF (setup->tx_n));  
  adf7021_tx_enabled = true;
//  sleep (setup->ramp_time);   // FIXME  
}



void adf7021_disable_tx ()
{
  adf7021_tx_enabled = false;
  adf7021_write_register (ADF7021_REGISTER_DEREF (setup->rx_n));

//  set_port (PD3OUT);  
//  clear_port (EXTERNAL_PA_ON);     // FIXME
//  sleep (setup->ramp_time);        // FIXME
}



static inline void _write_register (uint32_t data)
{
  uint32_t counter = 0xffffffff;

  asm ("L_%=: "
       "cbi %[sclk_port], %[sclk_bit]\n\t"
       "sbrs %D0, 7\n\t"
       "cbi %[sdata_port], %[sdata_bit]\n\t"
       "sbrc %D0, 7\n\t"
       "sbi %[sdata_port], %[sdata_bit]\n\t"
       "lsl %A0\n\t"
       "rol %B0\n\t"
       "rol %C0\n\t"
       "rol %D0\n\t"
       "sbi %[sclk_port], %[sclk_bit]\n\t"
       "lsl %A1\n\t"
       "rol %B1\n\t"
       "rol %C1\n\t"
       "rol %D1\n\t"
       "sbrc %D1, 7\n\t"
       "rjmp L_%=\n\t"
       :: "r" (data), "r" (counter),
          [sclk_port]  "I" (_SFR_IO_ADDR(ADF7021_SCLK_PORT)),
          [sclk_bit]   "I" (ADF7021_SCLK_BIT),
          [sdata_port] "I" (_SFR_IO_ADDR(ADF7021_SDATA_PORT)),
          [sdata_bit]  "I" (ADF7021_SDATA_BIT));
  clear_port (ADF7021_SCLK);
  clear_port (ADF7021_SDATA);
  set_port (ADF7021_SLE);
}


void adf7021_write_register (uint32_t data)
{
  _write_register (data);
  clear_port (ADF7021_SLE);
}


uint16_t adf7021_read_register (uint32_t readback)
{

  uint32_t data;
  uint32_t counter = 0xffff8000; // We need to clock in 17 bits, of which the
                                 // first will be discarded
  
  _write_register (readback);
  
  asm ("L_%=: "
       "lsl %A1\n\t"
       "rol %B1\n\t"
       "rol %C1\n\t"
       "rol %D1\n\t"
       "sbi %[sclk_port], %[sclk_bit]\n\t"
       "clc\n\t"
       "sbic %[sread_pin], %[sread_bit]\n\t"
       "sec\n\t"
       "rol %A0\n\t"
       "rol %B0\n\t"
       "rol %C0\n\t"
       "rol %D0\n\t"
       "cbi %[sclk_port], %[sclk_bit]\n\t"
       "sbrc %D1, 7\n\t"
       "rjmp L_%=\n\t"
       : "=&r" (data)
       : "r" (counter),
          [sclk_port]  "I" (_SFR_IO_ADDR(ADF7021_SCLK_PORT)),
          [sclk_bit]   "I" (ADF7021_SCLK_BIT),
          [sread_pin] "I" (_SFR_IO_ADDR(ADF7021_SREAD_PIN)),
          [sread_bit]  "I" (ADF7021_SREAD_BIT));

  /* An 18th clock cycle is needed to complete the readback */
  set_port (ADF7021_SCLK);  
  clear_port (ADF7021_SLE);
  clear_port (ADF7021_SCLK);

  return (uint16_t)data;
}


double adf7021_read_rssi ()
{
  uint32_t readback = adf7021_read_register (ADF7021_READBACK_RSSI);
  double rssi = (double)(readback & 0x7f);
  double gain;

  switch ((readback >> 7) & 0xF) {
  case 0x6: gain = 24.0; break;
  case 0x5: gain = 38.0; break;
  case 0x4: gain = 58.0; break;
  case 0x0: gain = 86.0; break;
  default:  gain = 0.0;
  }
    
  return -130.0 + (rssi + gain) * 0.5;
}
