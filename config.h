#if !defined __CONFIG_H__
#define __CONFIG_H__

#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include "ax25.h"

#if defined __CONFIG_C__

#define DEFINE_PARAM(x, t) \
     EEMEM t PARAM_##x;       \
     EEMEM uint8_t PARAM_##x##_CRC
     
#define DEFAULT_PARAM(x) PROGMEM __typeof__(PARAM_##x) PARAM_DEFAULT_##x

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
DEFINE_PARAM( TRX_SQUELCH,        double       );
DEFINE_PARAM( TRACKER_ON,         uint8_t      );
DEFINE_PARAM( TRACKER_SLEEP_TIME, uint16_t     ); 
DEFINE_PARAM( SYMBOL,             uint8_t      );
DEFINE_PARAM( SYMBOL_TABLE,       uint8_t      );
DEFINE_PARAM( REPORT_COMMENT,     comment      );
DEFINE_PARAM( GPS_BAUD,           uint16_t     );


#if defined __CONFIG_C__
/***************************************************************
 * Default values for parameters to be stored in program
 * memory. This MUST be done for each parameter defined above
 * or the linker will complain.
 ***************************************************************/

DEFAULT_PARAM( MYCALL )             = {"NOCALL",0};
DEFAULT_PARAM( DEST )               = {"NONE", 0};
DEFAULT_PARAM( DIGIS )              = {{"WIDE3", 3}};
DEFAULT_PARAM( NDIGIS )             = 1;
DEFAULT_PARAM( TXDELAY )            = 20;
DEFAULT_PARAM( TXTAIL )             = 10;
DEFAULT_PARAM( TRX_FREQ )           = 144.800e6;
DEFAULT_PARAM( TRX_CALIBRATE )      = 0;
DEFAULT_PARAM( TRX_TXPOWER )        = -13.0;
DEFAULT_PARAM( TRX_AFSK_DEV )       = 3500;
DEFAULT_PARAM( TRX_SQUELCH )        = -80;
DEFAULT_PARAM( TRACKER_ON )         = 0;
DEFAULT_PARAM( TRACKER_SLEEP_TIME ) = 3000; // 30 sec.
DEFAULT_PARAM( SYMBOL)              = '.';
DEFAULT_PARAM( SYMBOL_TABLE)        = '/';
DEFAULT_PARAM( REPORT_COMMENT )     = "Polaric Tracker";
DEFAULT_PARAM( GPS_BAUD )           = 4800;

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
