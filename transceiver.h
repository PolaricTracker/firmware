#ifndef __TRANSCEIVER_H__
#define __TRANSCEIVER_H__


#define ADF7021_N_REGISTER 0
#define ADF7021_VCO_REGISTER 1
#define ADF7021_MODULATION_REGISTER 2
#define ADF7021_CLOCK_REGISTER 3
#define ADF7021_DEMOD_REGISTER 4
#define ADF7021_IF_FILTER_REGISTER 5
#define ADF7021_IF_FILTER_CAL_REGISTER 6
#define ADF7021_READBACK_REGISTER 7
#define ADF7021_POWER_DOWN_REGISTER 8
#define ADF7021_AGC_REGISTER 9
#define ADF7021_AFC_REGISTER 10
#define ADF7021_SWD_REGISTER 11
#define ADF7021_SWD_THRESHOLD_REGISTER 12
#define ADF7021_mFSK_DEMOD_REGISTER 13
#define ADF7021_TEST_DAC_REGISTER 14
#define ADF7021_TEST_MODE_REGISTER 15

#define ADF7021_READBACK_AFC_WORD 0x107
#define ADF7021_READBACK_RSSI 0x147
#define ADF7021_READBACK_VOLTAGE 0x157
#define ADF7021_READBACK_TEMP 0x167
#define ADF7021_READBACK_EXT_ADC 0x177
#define ADF7021_READBACK_FILTER_CAL 0x187
#define ADF7021_READBACK_SILICON_REV 0x1C7

/* This struct should be set up at modulator/demodulator level during
   initialization and a call to adf7021_init made */
typedef struct _adf7021_setup {
  uint32_t n;
  uint32_t vco;
  uint32_t modulation;
  uint32_t clock;
  uint32_t demod;
  uint32_t if_filter;

  /* The remaining fields are optional and should be cleared if not used */
  uint32_t if_filter_cal; 
  uint32_t power_down;
  uint32_t agc;
  uint32_t afc;
  uint32_t swd_word;
  uint32_t swd_threshold;
  uint32_t mfsk_demod;
  uint32_t test_dac;
  uint32_t test_mode;
} adf7021_setup_t;

#define adf7021_setup_init(s) memset (s, 0, sizeof (adf7021_setup_t))

// TODO: define macros to easily manipulate fields in _adf7021_setup


extern bool adf7021_enabled;
extern bool adf7021_tx_enabled;


void adf7021_init (adf7021_setup_t *s);

void adf7021_power_on ();
void adf7021_power_off ();

void adf7021_enable_tx ();
void adf7021_disable_tx ();


void adf7021_write_register (uint32_t data)
uint32_t adf7021_read_register (uint32_t readback)

#endif /* __TRANSCEIVER_H__ */
