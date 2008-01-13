#include "transceiver.h"


bool adf7021_enabled = false;
bool adf7021_tx_enabled = false;

static adf7021_setup* setup;

static void _rx_enable ();


void adf7021_init (adf7021_setup_t* s)
{
  adf7021_enabled = adf7021_tx_enabled = false;
  
  // TODO: set port directions

  clear_bit (EXTERNAL_PA_ENABLE);
  clear_bit (ADF7021_ENABLE);
  clear_bit (ADF7021_SCLK);
  clear_bit (ADF7021_SLE);

  setup = s;
}


void adf7021_power_on ()
{
  set_bit (ADF7021_ENABLE);

  /* Wait for transceiver to become ready */
  while (!bit_is_set (ADF7021_MUXOUT))
    ;

  /* Power down unused parts of the transceiver */
  if (setup->power_down)
    adf7021_write_register (setup->power_down);

  /* Turn on VCO */
  adf7021_write_register (setup->vco);
  us_delay (700);

  /* Turn on TX/RX clock */
  adf7021_write_register (setup->clock);

  /* Setup IF filter */
  if (setup->if_filter_cal) {
    /* Do fine calibration */
    adf7021_write_register (setup->if_filter_cal);
    adf7021_write_register (setup->if_filter);
    us_delay (5200);
  } else {
    adf7021_write_register (setup->if_filter);
    us_delay (200);
  }

  /* Not sure where best to put these */
  if (setup->mfsk_demod)
    adf7021_write_register (setup->mfsk_demod);
  if (setup->agc)
    adf7021_write_register (setup->agc);
  if (setup->test_dac)
    adf7021_write_register (setup->test_dac);
  if (setup->test_mode)
    adf7021_write_register (setup->test_mode);
  
  /* Setup frame synchronization */
  if (setup->swd_word)
    adf7021_write_register (setup->swd_word);
  if (setup->swd_threshold)
    adf7021_write_register (setup->swd_threshold);
  
  /* Enable PLL */
  adf7021_write_register (setup->n);
  us_delay (40);

  /* Enable rx mode */
  _rx_enable ();
  
  adf7021_enabled = true;
}


void adf7021_power_off ()
{
  adf7021_enabled = adf7021_tx_enabled =false;
  clear_bit (EXTERNAL_PA_ENABLE);
  clear_bit (ADF7021_ENABLE);
}


void adf7021_tx_enable ()
{
  set_bit (EXTERNAL_PA_ENABLE);
  adf7021_write_register (setup->modulation);

  // TODO: wait for PA ramp up
  
  adf7021_tx_enabled = true;
}


void adf7021_tx_disable ()
{
  adf7021_tx_enabled = false;
  clear_bit (EXTERNAL_PA_ENABLE);

  /* This will disable the internal PA */
  adf7021_write_register (ADF7021_MODULATION_REGISTER);

  _rx_enable ();
}


void _rx_enable ()
{
  adf7021_write_register (setup->demod);

  if (setup->afc)
    adf7021_write_register (setup->afc);
}


/* This function should be rewritten in assembly and inlined */
static void _write_register (uint32_t data)
{
  nop ();
  for (uint8_t n = 0; n < 32; n++) {
    if (data & 0x8000)
      set_bit (ADF7021_SDATA);
    else
      clear_bit (ADF7021_SDATA);
    data <<= 1;
    set_bit (ADF7021_SCLK);
    nop();
    clear_bit (ADF7021_SCLK);
    nop ();
  }
  set_bit (ADF7021_SLE);
  nop ();
}

void adf7021_write_register (uint32_t data)
{
  _write_register (data);
  clear_bit (ADF7021_SLE);
}

/* This function should be rewritten in assembly and inlined */
static void _read_register (uint32_t *data)
{
  *data = 0;
  
  for (uint8_t n = 0; n < 32; n++) {
    set_bit (ADF7021_SCLK);
    nop();
    *data = (*data << 1) | (bit_is_set (ADF7021_SREAD) & 0x0001);
    clear_bit (ADF7021_SCLK);
    nop();
  }
  clear_bit (ADF7021_SLE);
}


uint32_t adf7021_read_register (uint32_t readback)
{
  uint32_t data;
  _write_register (readback);
  _read_register (&data);
  
  return data;
}
