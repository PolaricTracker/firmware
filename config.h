#if !defined __CONFIG_H__
#define __CONFIG_H__

#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include "ax25.h"

#if defined __CONFIG_C__

#define DEFINE_PARAM(x, t) \
     EEMEM t PARAM_##x;       \
     EEMEM uint8_t PARAM_##x##_CRC
     
#define DEFAULT_PARAM(x,t) PROGMEM t PARAM_DEFAULT_##x   

#else

#define DEFINE_PARAM(x, t); \
     extern EEMEM t PARAM_##x; \
     extern PROGMEM t PARAM_DEFAULT_##x 
     
#endif


typedef addr_t __digilist_t[7];  
typedef char comment[40];


/***************************************************************
 * Define parameters:
 *            Name     Type     
 ***************************************************************/ 
           
DEFINE_PARAM( MYCALL,             addr_t       );
DEFINE_PARAM( DEST,               addr_t       );
DEFINE_PARAM( DIGIS,              __digilist_t );
DEFINE_PARAM( NDIGIS,             uint8_t      );
DEFINE_PARAM( TXDELAY,            uint8_t      );
DEFINE_PARAM( TXTAIL,             uint8_t      );
DEFINE_PARAM( TRX_FREQ,           uint32_t     );
DEFINE_PARAM( TRX_CALIBRATE,      int          );
DEFINE_PARAM( TRX_TXPOWER,        double       );
DEFINE_PARAM( TRX_AFSK_DEV,       uint16_t     );
DEFINE_PARAM( TRACKER_ON,         uint8_t      );
DEFINE_PARAM( TRACKER_SLEEP_TIME, uint16_t     ); 
DEFINE_PARAM( SYMBOL,             uint8_t      );
DEFINE_PARAM( SYMBOL_TABLE,       uint8_t      );
DEFINE_PARAM( REPORT_COMMENT,     comment      );

#if defined __CONFIG_C__
/***************************************************************
 * Default values for parameters to be stored in program
 * memory. This MUST be done for each parameter defined above
 * or the linker will complain.
 ***************************************************************/

DEFAULT_PARAM( MYCALL, addr_t)               = {"NOCALL",0};
DEFAULT_PARAM( DEST, addr_t)                 = {"NONE", 0};
DEFAULT_PARAM( DIGIS, __digilist_t)          = {};
DEFAULT_PARAM( NDIGIS,  uint8_t)             = 0;
DEFAULT_PARAM( TXDELAY, uint8_t)             = 20;
DEFAULT_PARAM( TXTAIL,  uint8_t)             = 10;
DEFAULT_PARAM( TRX_FREQ, uint32_t)           = 144.800e6;
DEFAULT_PARAM( TRX_CALIBRATE, int)           = 0;
DEFAULT_PARAM( TRX_TXPOWER, double)          = -13.0;
DEFAULT_PARAM( TRX_AFSK_DEV, uint16_t)       = 3500;
DEFAULT_PARAM( TRACKER_ON, uint8_t)          = 0;
DEFAULT_PARAM( TRACKER_SLEEP_TIME, uint16_t) = 6000; // 60 sec.
DEFAULT_PARAM( SYMBOL, uint8_t)              = '.';
DEFAULT_PARAM( SYMBOL_TABLE, uint8_t)        = '/';
DEFAULT_PARAM( REPORT_COMMENT, comment )     = "Polaric Tracker 0.2";

#endif
 

/***************************************************************
 * Functions/macros for accessing parameters
 ***************************************************************/

#define GET_PARAM(x, val)      get_param(&PARAM_##x, (val), sizeof(PARAM_##x),(PGM_P) &PARAM_DEFAULT_##x)
#define SET_PARAM(x, val)      set_param(&PARAM_##x, (val), sizeof(PARAM_##x))
#define GET_BYTE_PARAM(x)      get_byte_param(&PARAM_##x,(PGM_P) &PARAM_DEFAULT_##x)
#define SET_BYTE_PARAM(x, val) set_byte_param(&PARAM_##x, ((uint8_t) val))

void    set_param(void*, const void*, uint8_t);
int     get_param(const void*, void*, uint8_t, PGM_P);
void    set_byte_param(uint8_t*, uint8_t);
uint8_t get_byte_param(const uint8_t*, PGM_P);

#endif /* __CONFIG_H__ */
