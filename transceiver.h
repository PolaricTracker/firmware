/*
 * $Id: transceiver.h,v 1.12 2009-03-26 22:09:49 la7eca Exp $
 */
#ifndef __TRANSCEIVER_H__
#define __TRANSCEIVER_H__


#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* N Register */

#define ADF7021_N_REGISTER 0

enum {
  ADF7021_MUXOUT_REGULATOR_READY,
  ADF7021_MUXOUT_FILTER_CAL_COMPLETE,
  ADF7021_MUXOUT_DIGITAL_LOCK_DETECT,
  ADF7021_MUXOUT_RSSI_READY,
  ADF7021_MUXOUT_TX_ON, /* or is it off */
  ADF7021_MUXOUT_LOGIC_ZERO,
  ADF7021_MUXOUT_TRISTATE,
  ADF7021_MUXOUT_LOGIC_ONE
};

#define ADF7021_MUXOUT_WAIT()  while (!pin_is_high (ADF7021_MUXOUT)) t_yield();


typedef struct _adf7021_n_register {
  unsigned address : 4;
  unsigned fractional : 15;
  unsigned integer : 8;
  bool tx_disable : 1;
  bool uart_mode : 1;
  unsigned muxout : 3;
} __attribute__ ((packed)) adf7021_n_register_t;



/* VCO/Oscillator Register */

#define ADF7021_VCO_OSC_REGISTER 1

enum {
  ADF7021_XTAL_BIAS_20,
  ADF7021_XTAL_BIAS_25,
  ADF7021_XTAL_BIAS_30,
  ADF7021_XTAL_BIAS_35
};

enum {
  ADF7021_CP_CURRENT_0_3,
  ADF7021_CP_CURRENT_0_9,
  ADF7021_CP_CURRENT_1_5,
  ADF7021_CP_CURRENT_2_1
};

#define ADF7021_VCO_BIAS_CURRENT_mA(mA) (unsigned)((double)(mA)/0.25) // Valid 0.25--3.75mA

enum {
  ADF7021_VCO_ADJUST_NORMAL,
  ADF7021_VCO_ADJUST_UP_1,
  ADF7021_VCO_ADJUST_UP_2,
  ADF7021_VCO_ADJUST_UP_3
};

enum {
  ADF7021_VCO_INDUCTOR_INTERNAL,
  ADF7021_VCO_INDUCTOR_EXTERNAL
};

typedef struct _adf7021_vco_osc_register {
  unsigned address : 4;
  unsigned r_counter : 3;
  unsigned clockout_divide : 4;
  bool xtal_doubler : 1;
  bool xosc_enable : 1;
  unsigned xtal_bias : 2;
  unsigned cp_current : 2;
  bool vco_enable : 1;
  bool rf_div_2 : 1;
  unsigned vco_bias : 4;
  unsigned vco_adjust : 2;
  unsigned vco_inductor : 1;
} __attribute__ ((packed)) adf7021_vco_osc_register_t;



/* Transmitt Modulation Register */

#define ADF7021_MODULATION_REGISTER 2

typedef enum {
  ADF7021_MODULATION_2FSK,
  ADF7021_MODULATION_GAUSSIAN_2FSK,
  ADF7021_MODULATION_3FSK,
  ADF7021_MODULATION_4FSK,
  ADF7021_MODULATION_OVERSAMPLED_2FSK,
  ADF7021_MODULATION_RAISED_COSINE_2FSK,
  ADF7021_MODULATION_RAISED_COSINE_3FSK,
  ADF7021_MODULATION_RAISED_COSINE_4FSK
} adf7021_modulation_t;

typedef enum {
  ADF7021_PA_RAMP_OFF,
  ADF7021_PA_RAMP_256_BITS,
  ADF7021_PA_RAMP_128_BITS,
  ADF7021_PA_RAMP_64_BITS,
  ADF7021_PA_RAMP_32_BITS,
  ADF7021_PA_RAMP_16_BITS,
  ADF7021_PA_RAMP_8_BITS,
  ADF7021_PA_RAMP_4_BITS  
} adf7021_pa_ramp_t;

typedef enum {
  ADF7021_PA_BIAS_5_uA,
  ADF7021_PA_RAMP_7_uA,
  ADF7021_PA_RAMP_9_uA,
  ADF7021_PA_RAMP_11_uA
} adf7021_pa_bias_t;

enum {
  ADF7021_R_COSINE_ALPHA_0_5,
  ADF7021_R_COSINE_ALPHA_0_7
};

#define ADF7021_PA_LEVEL_STEP (63.0/29.0)

typedef struct _adf7021_modulation_register {
  unsigned address : 4;
  adf7021_modulation_t scheme : 3;
  bool pa_enable : 1;
  adf7021_pa_ramp_t pa_ramp : 3;
  adf7021_pa_bias_t pa_bias : 2;
  unsigned pa_level : 6;
  unsigned tx_frequency_deviation : 9;
  unsigned tx_data_invert : 2;
  unsigned r_cosine_alpha : 1;
} __attribute__ ((packed)) adf7021_modulation_register_t;


/* Transmit/receive Clock Register */

#define ADF7021_CLOCK_REGISTER 3

// TODO: define macros to set clocks in Hz

typedef struct _adf7021_clock_register {
  unsigned address : 4;
  unsigned bbos_clk_divide : 2;
  unsigned dem_clk_divide : 4;
  unsigned cdr_clk_divide : 8;
  unsigned seq_clk_divide : 8;
  unsigned agc_clk_divide : 6;
} __attribute__ ((packed)) adf7021_clock_register_t;


/* Demodulator Setup Register */

#define ADF7021_DEMOD_REGISTER 4

typedef enum {
  ADF7021_DEMOD_2FSK_LINEAR,
  ADF7021_DEMOD_2FSK_CORRELATOR,
  ADF7021_DEMOD_3FSK,
  ADF7021_DEMOD_4FSK  
} adf7021_demod_scheme_t;

enum {
  ADF7021_DEMOD_PRODUCT_DOT,
  ADF7021_DEMOD_PRODUCT_CROSS
};
  
enum {
  ADF7021_RX_INVERT_NORMAL,
  ADF7021_RX_INVERT_CLK,
  ADF7021_RX_INVERT_DATA,
  ADF7021_RX_INVERT_CLK_DATA
};

typedef enum {
  ADF7021_DEMOD_IF_BW_12_5,
  ADF7021_DEMOD_IF_BW_18_75,
  ADF7021_DEMOD_IF_BW_25
} adf7021_if_bw_t;
  
typedef struct _adf7021_demod_register {
  unsigned address : 4;
  adf7021_demod_scheme_t scheme : 3;
  unsigned product : 1;
  unsigned rx_invert : 2;
  unsigned discriminator_bw : 10;
  unsigned post_demod_bw : 10;
  unsigned if_bw : 2;
} __attribute__ ((packed)) adf7021_demod_register_t;


/* IF Filter Setup Register */

#define ADF7021_IF_FILTER_REGISTER 5

#define ADF7021_IF_FILTER_DIVIDER() (ADF7021_XTAL / 50e3)
#define ADF7021_IF_FILTER_ADJUST(adj) ((adj) >= 0 ? (adj) : (-1 * (adj)) & 0x20)

enum {
  ADF7021_IR_ADJUST_I,
  ADF7021_IR_ADJUST_Q,
};

typedef struct _adf7021_if_filter_register {
  unsigned address : 4;
  bool calibrate : 1;
  unsigned divider : 9;
  unsigned adjust : 6;
  unsigned ir_phase_adjust_mag : 6;
  unsigned ir_phase_adjust_direction : 1;
  unsigned ir_gain_adjust_mag : 6;
  unsigned ir_gain_adjust_direction : 1;
  bool ir_attenuate_ : 1;
} __attribute__ ((packed)) adf7021_if_filter_register_t;


/* IF Fine Cal Setup Register */

#define ADF7021_IF_FILTER_CAL_REGISTER 6

enum {
  ADF7021_IR_CAL_SOURCE_DRIVE_LEVEL_OFF,
  ADF7021_IR_CAL_SOURCE_DRIVE_LEVEL_LOW,
  ADF7021_IR_CAL_SOURCE_DRIVE_LEVEL_MED,
  ADF7021_IR_CAL_SOURCE_DRIVE_LEVEL_HIGH
};

typedef struct _adf7021_if_filter_cal_register {
  unsigned address : 4;
  bool fine_calibrate : 1;
  unsigned lower_tone_divide : 8;
  unsigned upper_tone_divide : 8;
  unsigned dwell_time : 7;
  unsigned source_drive_level : 2;
  bool ir_cal_source_div_2 : 1;
} __attribute__ ((packed)) adf7021_if_filter_cal_register_t;


/* Readback Setup Register */

#define ADF7021_READBACK_REGISTER 7

#define ADF7021_READBACK_AFC_WORD 0x107
#define ADF7021_READBACK_RSSI 0x147
#define ADF7021_READBACK_VOLTAGE 0x157
#define ADF7021_READBACK_TEMP 0x167
#define ADF7021_READBACK_EXT_ADC 0x177
#define ADF7021_READBACK_FILTER_CAL 0x187
#define ADF7021_READBACK_SILICON_REV 0x1C7


/* Power Down Test Register */

#define ADF7021_POWER_DOWN_REGISTER 8

typedef struct _adf7021_power_down_register {
  unsigned address : 4;
  bool synth_enable : 1;
  bool _reserverd : 1;
  bool lna_mixer_enable : 1;
  bool filter_enable : 1;
  bool adc_enable : 1;
  bool demod_enable : 1;
  bool log_amp_enable : 1;
  bool tx_rx_switch_disable : 1;
  bool pa_enable_rx_mode : 1;
  bool demod_reset : 1;
  bool cdr_reset : 1;
  bool counter_reset : 1;
} __attribute__ ((packed)) adf7021_power_down_register_t;


/* AGC Register */

#define ADF7021_AGC_REGISTER 9

enum {
  ADF7021_AGC_MODE_AUTO,
  ADF7021_AGC_MODE_MANUAL,
  ADF7021_AGC_MODE_FREEZE
};

enum {
  ADF7021_AGC_LNA_GAIN_3,
  ADF7021_AGC_LNA_GAIN_10,
  ADF7021_AGC_LNA_GAIN_30
};

enum {
  ADF7021_AGC_FILTER_GAIN_8,
  ADF7021_AGC_FILTER_GAIN_24,
  ADF7021_AGC_FILTER_GAIN_72
};

enum {
  ADF7021_AGC_FILTER_CURRENT_LOW,
  ADF7021_AGC_FILTER_CURRENT_HIGH
};

enum {
  ADF7021_AGC_LNA_MODE_DEFAULT,
  ADF7021_AGC_LNA_MODE_REDUCED_GAIN
};

enum {
  ADF7021_AGC_MIXER_LINEARITY_DEFAULT,
  ADF7021_AGC_MIXER_LINEARITY_HIGH
};

typedef struct _adf7021_agc_register {
  unsigned address : 4;
  unsigned low_threshold : 7;
  unsigned high_threshold : 7;
  unsigned mode : 2;
  unsigned lna_gain : 2;
  unsigned filter_gain : 2;
  unsigned filter_curent : 1;
  unsigned lna_mode : 1;
  unsigned _lna_current : 2;
  unsigned mixer_linearity : 1;
} __attribute__ ((packed)) adf7021_agc_register_t;
  

/* AFC Register */

#define ADF7021_AFC_REGISTER 10

typedef struct _adf7021_afc_register {
  unsigned address : 4;
  bool enable : 1;
  unsigned scaling_factor : 12;
  unsigned ki : 4;
  unsigned kp : 3;
  unsigned max_range : 8;
} __attribute__ ((packed)) adf7021_afc_register_t;


/* Sync Word Detect Register */

#define ADF7021_SWD_WORD_REGISTER 11

typedef struct _adf7021_swd_word_register {
  unsigned address : 4;
  unsigned length : 2;
  unsigned tolerance : 2;
  long unsigned sequence : 24;
} __attribute__ ((packed)) adf7021_swd_word_register_t;


/* SWD/Threshold Setup Register */

#define ADF7021_SWD_THRESHOLD_REGISTER 12

enum {
  ADF7021_LOCK_MODE_FREE_RUNNIGN,
  ADF7021_LOCK_MODE_AFTER_NEXTS_SYNCWORD,
  ADF7021_LOCK_MODE_AFTER_NEXTS_SYNCWORD_FOR_DATA_PACKET_LENGTH,
  ADF7021_LOCK_MODE_LOCK_THRESHOLD
};

enum {
  ADF7021_SWD_MODE_PIN_LOW,
  ADF7021_SWD_MODE_PIN_PIN_HIGH_AFTER_NEXT_SYNCWORD,
  ADF7021_SWD_MODE_PIN_PIN_HIGH_AFTER_NEXT_SYNCWORD_FOR_DATA_PACKET_LENGTH,
  ADF7021_SWD_MODE_INTERRUPT_PIN_HIGH
};


typedef struct _adf7021_swd_threshold_register {
  unsigned address : 4;
  unsigned lock_mode : 2;
  unsigned swd_mode : 2;
  unsigned data_packet_length : 8;
} __attribute__ ((packed)) adf7021_swd_threshold_register_t;


/* 3FSK/4FSK Demod Register */

#define ADF7021_MFSK_DEMOD_REGISTER 13

enum {
  ADF7021_VITERBI_PATH_MEMORY_4_BITS,
  ADF7021_VITERBI_PATH_MEMORY_6_BITS,
  ADF7021_VITERBI_PATH_MEMORY_8_BITS,
  ADF7021_VITERBI_PATH_MEMORY_32_BITS
};

typedef struct _adf7021_mfsk_demod_register {
  unsigned address : 4;
  unsigned slicer_threshold : 7;
  bool three_fsk_viterbi_decoder : 1;
  bool phase_correction : 1;
  unsigned viterbi_path_memory : 3;
  unsigned three_fsk_cdr_threshold : 7;
  unsigned three_fsk_preamble_time_valid : 4;
} __attribute__ ((packed)) adf7021_mfsk_demod_register_t;


/* Test DAC Register */

#define ADF7021_TEST_DAC_REGISTER 14

typedef struct _adf7021_test_dac_register {
  unsigned address : 4;
  bool enable : 1;
  unsigned offset : 16;
  unsigned gain : 4;  // computed as pow(2, gain)
  unsigned pulse_extension : 2; 
  unsigned ed_leak_factor : 3; // leakage = pow(2, -8 - ed_leak_factor)
  unsigned ed_peak_response : 2; // computed as 1 / pow(2, ed_peak_response)
} __attribute__ ((packed)) adf7021_test_dac_register_t;
/* Pulse_extension and ed_peak_respones may have been mixed in the datasheet */


/* Test Mode Register */

#define ADF7021_TEST_MODE_REGISTER 15

typedef enum {
  ADF7021_RX_TEST_MODE_NORMAL,
  ADF7021_RX_TEST_MODE_SCLK_DATA_TO_IQ,
  ADF7021_RX_TEST_MODE_REVERSE_IQ,
  ADF7021_RX_TEST_MODE_IQ_TO_TxRxCLK_TxRxDATA,
  ADF7021_RX_TEST_MODE_3FSK_SLICER_ON_TxRxDATA,
  ADF7021_RX_TEST_MODE_CORRELATOR_SLICE_ON_TxRxDATA,
  ADF7021_RX_TEST_MODE_LINEAR_SLICER_ON_TxRxDATA,// Datasheet says RXDATA
  ADF7021_RX_TEST_MODE_SDATA_TO_CDR,
  ADF7021_RX_TEST_MODE_ADDITIONAL_FILTERING_ON_IQ,
  ADF7021_RX_TEST_MODE_ENABLE_REG14_DEMOD_PARAMETERS,
  ADF7021_RX_TEST_MODE_POWER_DOWN_DDT_AND_ED_IN_T_DIV_4_MODE,
  ADF7021_RX_TEST_MODE_ENVELOPE_DETECTOR_WHATCHDOG_DISABLED,
  ADF7021_RX_TEST_MODE_PROHIBIT_CALACTIVE = 13,
  ADF7021_RX_TEST_MODE_FORCE_CALACTIVE,
  ADF7021_RX_TEST_MODE_ENABLE_DEMOD_DURING_CAL
} adf7021_rx_test_mode_t;

typedef enum {
  ADF7021_TX_TEST_MODE_NORMAL,
  ADF7021_TX_TEST_MODE_CARRIER,
  ADF7021_TX_TEST_MODE_HIGH,
  ADF7021_TX_TEST_MODE_LOW,
  ADF7021_TX_TEST_MODE_1010,
  ADF7021_TX_TEST_MODE_PN9,
  ADF7021_TX_TEST_MODE_SYNC_WORD
} adf7021_tx_test_mode_t;


typedef enum {
  ADF7021_CLK_MUX_NORMAL,
  ADF7021_CLK_MUX_DEMOD_CLK,
  ADF7021_CLK_MUX_CDR_CLK,
  ADF7021_CLK_MUX_SEQ_CLK,
  ADF7021_CLK_MUX_BB_OFFSET_CLK,
  ADF7021_CLK_MUX_SIGMA_DELTA_CLK,
  ADF7021_CLK_MUX_ADC_CLK,
  ADF7021_CLK_MUX_TXRX_CLK,
} adf7021_clk_mux_t;

typedef enum {
  ADF7021_ANALOG_TEST_MODE_BAND_GAP,
  ADF7021_ANALOG_TEST_MODE_40uA_FROM_REG4,
  ADF7021_ANALOG_TEST_MODE_FILTER_I_1,
  ADF7021_ANALOG_TEST_MODE_FILTER_I_2,
  ADF7021_ANALOG_TEST_MODE_FILTER_Q_1 = 5,
  ADF7021_ANALOG_TEST_MODE_FILTER_Q_2,
  ADF7021_ANALOG_TEST_MODE_ADC_REF = 8,
  ADF7021_ANALOG_TEST_MODE_BIAS_FROM_RSSI_5uA,
  ADF7021_ANALOG_TEST_MODE_FILTER_COARSE_CAL_OSC_OP,
  ADF7021_ANALOG_TEST_MODE_RSSI,
  ADF7021_ANALOG_TEST_MODE_OSET_LOOP,
  ADF7021_ANALOG_TEST_MODE_RSSI_RECT_P,
  ADF7021_ANALOG_TEST_MODE_RSSI_RECT_M,
  ADF7021_ANALOG_TEST_MODE_BIAS_FROM_BB_FILTER
} adf7021_analog_test_mode_t;

typedef struct _adf7021_test_mode_register {
  unsigned address : 4;
  adf7021_rx_test_mode_t rx : 4;
  adf7021_tx_test_mode_t tx : 3;
  unsigned sigma_delta : 3;
  unsigned pfd_cp : 3;
  adf7021_clk_mux_t clk_mux : 3;
  unsigned pll : 3;
  adf7021_analog_test_mode_t analog : 4;
  bool force_ld_high : 1;
  bool power_down_enable : 1;
} __attribute__ ((packed)) adf7021_test_mode_register_t;


#define ADF7021_REGISTER_DEREF(reg)  *(uint32_t*)&(reg)
#define ADF7021_INIT_REGISTER(reg,addr) ADF7021_REGISTER_DEREF (reg) = addr
#define ADF7021_REGISTER_IS_INITIALIZED(place) ((place).address)


typedef struct _adf7021_setup {
  adf7021_n_register_t tx_n;
  adf7021_n_register_t rx_n;
  adf7021_vco_osc_register_t vco_osc;
  adf7021_modulation_register_t modulation;
  adf7021_clock_register_t clock;
  adf7021_demod_register_t demod;
  adf7021_if_filter_register_t if_filter;

  /* The remaining fields are optional and has to be exxplicit
     initialize with the ADF7021_INIT_REGISTER */
  adf7021_if_filter_cal_register_t if_filter_cal; 
  adf7021_power_down_register_t power_down;
  adf7021_agc_register_t agc;
  adf7021_afc_register_t afc;
  adf7021_swd_word_register_t swd_word;
  adf7021_swd_threshold_register_t swd_threshold;
  adf7021_mfsk_demod_register_t mfsk_demod;
  adf7021_test_dac_register_t test_dac;
  adf7021_test_mode_register_t test_mode;

  /* Non register settings*/
  uint16_t deviation;
  uint16_t data_rate;
  uint16_t ramp_time;
} adf7021_setup_t;


extern bool adf7021_enabled;
extern bool adf7021_tx_enabled;


void adf7021_enable_AFC (adf7021_setup_t *setup, uint16_t range);
void adf7021_setup_init(adf7021_setup_t *setup);
void adf7021_set_power (adf7021_setup_t *setup, double dBm,  adf7021_pa_ramp_t ramp);
void adf7021_set_frequency (adf7021_setup_t *setup, uint32_t freq);
void adf7021_set_data_rate (adf7021_setup_t *setup, uint16_t data_rate);
void adf7021_set_modulation (adf7021_setup_t *setup, adf7021_modulation_t scheme, uint16_t deviation);
void adf7021_set_if_filter_fine_calibration (adf7021_setup_t *setup, double tone_cal_time);
void adf7021_set_post_demod_filter (adf7021_setup_t *setup, uint16_t cutoff);
void adf7021_set_demodulation (adf7021_setup_t *setup, adf7021_demod_scheme_t scheme);

static inline uint32_t adf7021_pfd_freq (adf7021_setup_t *setup) {
  return ((ADF7021_XTAL << (setup->vco_osc.xtal_doubler ? 1 : 0)) / setup->vco_osc.r_counter);
}


void adf7021_init (adf7021_setup_t *s);

void adf7021_power_on (void);
void adf7021_power_off (void);

void adf7021_enable_tx (void);
void adf7021_disable_tx (void);

void adf7021_write_register (uint32_t data);
uint16_t adf7021_read_register (uint32_t readback);

static inline double adf7021_read_battery_voltage (void) {
  return (double)(adf7021_read_register (ADF7021_READBACK_VOLTAGE) & 0x7f) / 21.1;
}

static inline double adf7021_read_adc_voltage (void) {
  return (double)(adf7021_read_register (ADF7021_READBACK_EXT_ADC) & 0x7f) / 42.1;
}

static inline double adf7021_read_temperature (void) {
  return -40.0 + (68.4 - (adf7021_read_register (ADF7021_READBACK_TEMP) & 0x7f)) * 9.32;
}

double adf7021_read_rssi (void);
void adf7021_wait_enabled (void);
void adf7021_wait_tx_off(void);


#if defined USBKEY_TEST
#define adf7021_wait_enabled() 
#define adf7021_read_rssi() -130
#endif

#endif /* __TRANSCEIVER_H__ */
