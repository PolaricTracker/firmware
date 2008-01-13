#include "transceiver.h"


bool transceiver_enabled = false;
bool transceiver_tx_enabled = false;

static transceiver_setup* setup;

static void _enable_rx ();


void transceiver_init (transceiver_setup_t* s)
{
  transceiver_enabled = transceiver_tx_enabled = false;
  
  // TODO: set port directions

  clear_bit (TRANSCEIVER_PA_ENABLE);
  clear_bit (TRANSCEIVER_ENABLE);
  clear_bit (TRANSCEIVER_SCLK);
  clear_bit (TRANSCEIVER_SLE);

  setup = s;
}


void transceiver_power_on ()
{
  set_bit (TRANSCEIVER_ENABLE);

  /* Wait for transceiver to become ready */
  while (!bit_is_set (TRANSCEIVER_MUXOUT))
    ;

  /* Power down unused parts of the transceiver */
  if (setup->power_down)
    transceiver_write_register (setup->power_down);

  /*  Turn on VCO */
  transceiver_write_register (setup->vco);
  us_delay (700);

  /* Turn on TX/RX clock */
  transceiver_write_register (setup->clock);

  /* Calibrate IF filter */
  if (setup->if_filter_cal) {
    /* Do fine calibration */
    transceiver_write_register (setup->if_filter_cal);
    transceiver_write_register (setup->if_filter);
    us_delay (5200);
  } else {
    transceiver_write_register (setup->if_filter);
    us_delay (200);
  }

  /* Not sure where to put these */
  if (setup->mfsk_demod)
    transceiver_write_register (setup->mfsk_demod);
  if (setup->agc)
    transceiver_write_register (setup->agc);
  if (setup->test_dac)
    transceiver_write_register (setup->test_dac);
  if (setup->test_mode)
    transceiver_write_register (setup->test_mode);
  
  /* Setup frame synchronization */
  if (setup->swd_word)
    transceiver_write_register (setup->swd_word);
  if (setup->swd_threshold)
    transceiver_write_register (setup->swd_threshold);
  
  /* Enable PLL */
  transceiver_write_register (setup->n);
  us_delay (40);

  /* Enable rx mode */
  _enable_rx ();
  
  transceiver_enabled = true;
}


void transceiver_power_off ()
{
  transceiver_enabled = transceiver_tx_enabled =false;
  clear_bit (TRANSCEIVER_PA_ENABLE);
  clear_bit (TRANSCEIVER_ENABLE);
}


void transceiver_tx_enable ()
{
  set_bit (TRANSCEIVER_EXTERNAL_PA_ENABLE);
  transceiver_write_register (setup->modulation);

  // TODO: wait for PA ramp up
  
  transceiver_tx_enabled = true;
}


void transceiver_tx_disable ()
{
  transceiver_tx_enabled = false;
  clear_bit (TRANSCEIVER_EXTERNAL_PA_ENABLE);

  /* This will disable the internal PA */
  transceiver_write_register (TRANSCEIVER_MODULATION_REGISTER);

  _enable_rx ();
}


void _enable_rx ()
{
  transceiver_write_register (setup->demod);

  if (setup->afc)
    transceiver_write_register (setup->afc);
}


/* This function should be rewritten in assembly and inlined */
static void _write_register (uint32_t data)
{
  nop ();
  for (uint8_t n = 0; n < 32; n++) {
    if (data & 0x8000)
      set_bit (TRANSCEIVER_SDATA);
    else
      clear_bit (TRANSCEIVER_SDATA);
    data <<= 1;
    set_bit (TRANSCEIVER_SCLK);
    nop();
    clear_bit (TRANSCEIVER_SCLK);
    nop ();
  }
  set_bit (TRANSCEIVER_SLE);
  nop ();
}

void transceiver_write_register (uint32_t data)
{
  _write_register (data);
  clear_bit (TRANSCEIVER_SLE);
}

/* This function should be rewritten in assembly and inlined */
static void _read_register (uint32_t *data)
{
  *data = 0;
  
  for (uint8_t n = 0; n < 32; n++) {
    set_bit (TRANSCEIVER_SCLK);
    nop();
    *data = (*data << 1) | (bit_is_set (TRANSCEIVER_RDATA) & 0x0001);
    clear_bit (TRANSCEIVER_SCLK);
    nop();
  }
  clear_bit (TRANSCEIVER_SLE);
}


uint32_t transceiver_read_register (uint32_t readback)
{
  uint32_t data;
  _write_register (readback);
  _read_register (&data);
  
  return data;
}
