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

#define COMMENT_LENGTH 40
#define TRACE_LENGTH 12
typedef addr_t __digilist_t[7];  
typedef char comment[COMMENT_LENGTH];
typedef uint8_t __trace_t[TRACE_LENGTH][2];

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
DEFINE_PARAM( TIMESTAMP_ON,       uint8_t      );
DEFINE_PARAM( ALTITUDE_ON,        uint8_t      );
DEFINE_PARAM( REPORT_COMMENT,     comment      );
DEFINE_PARAM( GPS_BAUD,           uint16_t     );
DEFINE_PARAM( TRACKER_TURN_LIMIT, uint16_t     );
DEFINE_PARAM( TRACKER_PAUSE_LIMIT,uint8_t      );
DEFINE_PARAM( STATUS_TIME,        uint8_t      );

extern __trace_t trace           __attribute__ ((section (".noinit")));
extern uint8_t   trace_index[]   __attribute__ ((section (".noinit")));

#if defined __CONFIG_C__


/***************************************************************
 * Default values for parameters to be stored in program
 * memory. This MUST be done for each parameter defined above
 * or the linker will complain.
 ***************************************************************/

DEFAULT_PARAM( MYCALL )              = {"NOCALL",0};
DEFAULT_PARAM( DEST )                = {"NONE", 0};
DEFAULT_PARAM( DIGIS )               = {{"WIDE3", 3}};
DEFAULT_PARAM( NDIGIS )              = 1;
DEFAULT_PARAM( TXDELAY )             = 20;
DEFAULT_PARAM( TXTAIL )              = 10;
DEFAULT_PARAM( TRX_FREQ )            = 144.800e6;
DEFAULT_PARAM( TRX_CALIBRATE )       = 0;
DEFAULT_PARAM( TRX_TXPOWER )         = -13.0;
DEFAULT_PARAM( TRX_AFSK_DEV )        = 3500;
DEFAULT_PARAM( TRX_SQUELCH )         = -80;
DEFAULT_PARAM( TRACKER_ON )          = 0;
DEFAULT_PARAM( TRACKER_SLEEP_TIME )  = 10; 
DEFAULT_PARAM( SYMBOL)               = '.';
DEFAULT_PARAM( SYMBOL_TABLE)         = '/';
DEFAULT_PARAM( TIMESTAMP_ON)         = 1;
DEFAULT_PARAM( ALTITUDE_ON)          = 0;
DEFAULT_PARAM( REPORT_COMMENT )      = "Polaric Tracker";
DEFAULT_PARAM( GPS_BAUD )            = 4800;
DEFAULT_PARAM( TRACKER_TURN_LIMIT )  = 45;
DEFAULT_PARAM( TRACKER_PAUSE_LIMIT ) = 5;
DEFAULT_PARAM( STATUS_TIME )         = 30;

__trace_t trace            __attribute__ ((section (".noinit")));
uint8_t   trace_index[2]   __attribute__ ((section (".noinit")));

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


/* Tracing. 
 * Two vectors:  0 is current, 1 is saved from previous run 
 */
void show_trace(char*, uint8_t, PGM_P, PGM_P);

#define TRACE_INIT         for (uint8_t i=0; i<TRACE_LENGTH; i++) trace[i][1] = trace[i][0]; \
                           trace_index[1] = trace_index[0]; \
                           trace_index[0] = 0; \
                           for (uint8_t i=0; i<TRACE_LENGTH; i++) trace[i][0] = 0;
                           
#define TRACE(val)         trace[trace_index[0]++ % TRACE_LENGTH][0] = (val)    
#define GET_TRACE(s,i)     trace[(trace_index[(s)] + (i)) % TRACE_LENGTH][(s)]               
#define TRACE_LAST         TRACE_LENGTH-1
                   
#endif /* __CONFIG_H__ */
