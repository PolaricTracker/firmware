#ifndef __TRANSCEIVER_H__
#define __TRANSCEIVER_H__


#define TRANSCEIVER_N_REGISTER 0
#define TRANSCEIVER_VCO_REGISTER 1
#define TRANSCEIVER_MODULATION_REGISTER 2
#define TRANSCEIVER_CLOCK_REGISTER 3
#define TRANSCEIVER_DEMOD_REGISTER 4
#define TRANSCEIVER_IF_FILTER_REGISTER 5
#define TRANSCEIVER_IF_FILTER_CAL_REGISTER 6
#define TRANSCEIVER_READBACK_REGISTER 7
#define TRANSCEIVER_POWER_DOWN_REGISTER 8
#define TRANSCEIVER_AGC_REGISTER 9
#define TRANSCEIVER_AFC_REGISTER 10
#define TRANSCEIVER_SWD_REGISTER 11
#define TRANSCEIVER_SWD_THRESHOLD_REGISTER 12
#define TRANSCEIVER_mFSK_DEMOD_REGISTER 13
#define TRANSCEIVER_TEST_DAC_REGISTER 14
#define TRANSCEIVER_TEST_MODE_REGISTER 15

#define TRANSCEIVER_READBACK_AFC_WORD 0x107
#define TRANSCEIVER_READBACK_RSSI 0x147
#define TRANSCEIVER_READBACK_VOLTAGE 0x157
#define TRANSCEIVER_READBACK_TEMP 0x167
#define TRANSCEIVER_READBACK_EXT_ADC 0x177
#define TRANSCEIVER_READBACK_FILTER_CAL 0x187
#define TRANSCEIVER_READBACK_SILICON_REV 0x1C7


typedef struct _transceiver_setup {
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
} transceiver_setup_t;

#define transceiver_setup_init(s) memset (s, 0, sizeof (transceiver_setup_t))

// TODO: define macros to easily manipulate fields in _transceiver_setup


extern bool transceiver_enabled;
extern bool transceiver_tx_enabled;


void transceiver_init (transceiver_setup_t *s);

void transceiver_power_on ();
void transceiver_power_off ();

void transceiver_enable_tx ();
void transceiver_disable_tx ();


void transceiver_write_register (uint32_t data)
uint32_t transceiver_read_register (uint32_t readback)

#endif /* __TRANSCEIVER_H__ */
