
#include "defines.h"
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include "kernel/timer.h"
#include "kernel/stream.h"
#include "hdlc.h"
#include "fbuf.h"
#include "ax25.h"
#include "config.h"
#include "transceiver.h"
#include "radio.h"
#include "gps.h"
#include <avr/interrupt.h>
#include "adc.h"
#include "afsk.h"
#include "ui.h"
#include "usb.h"



#define MAXTOKENS 10
#define BUFSIZE   90


void setup_transceiver(void);
uint8_t tokenize(char*, char*[], uint8_t, char*, bool);

static void do_boot      (uint8_t, char**, Stream*, Stream*);
static void do_teston    (uint8_t, char**, Stream*, Stream*);
static void do_version   (uint8_t, char**, Stream*, Stream*);
static void do_testpacket(uint8_t, char**, Stream*, Stream*);
static void do_mycall    (uint8_t, char**, Stream*, Stream*);
static void do_obj_id    (uint8_t, char**, Stream*, Stream*);
static void do_dest      (uint8_t, char**, Stream*, Stream*);
static void do_symbol    (uint8_t, char**, Stream*, Stream*);
static void do_obj_symbol(uint8_t, char**, Stream*, Stream*);
static void do_nmea      (uint8_t, char**, Stream*, Stream*);
static void do_txon      (uint8_t, char**, Stream*, Stream*);                 
static void do_tracker   (uint8_t, char**, Stream*, Stream*);
static void do_freq      (uint8_t, char**, Stream*, Stream*);
static void do_fcal      (uint8_t, char**, Stream*, Stream*);
static void do_power     (uint8_t, char**, Stream*, Stream*);
static void do_squelch   (uint8_t, char**, Stream*, Stream*);
static void do_rssi      (uint8_t, char**, Stream*, Stream*);
static void do_digipath  (uint8_t, char**, Stream*, Stream*);
static void do_trace     (uint8_t, char**, Stream*, Stream*);
static void do_txtone    (uint8_t, char**, Stream*, Stream*);
static void do_vbatt     (uint8_t, char**, Stream*, Stream*);
static void do_listen    (uint8_t, char**, Stream*, Stream*);
static void do_converse  (uint8_t, char**, Stream*, Stream*);
static void do_btext     (uint8_t, char**, Stream*, Stream*);
static void do_ps        (uint8_t, char**, Stream*, Stream*);
static void do_reset     (uint8_t, char**, Stream*, Stream*);
static void do_digipeater(uint8_t, char**, Stream*, Stream*);

static char buf[BUFSIZE]; 
extern fbq_t* outframes;  

/* May be moved to tracker.h ??*/
extern void tracker_on(void);
extern void tracker_off(void);
void tracker_controls_trx(bool c);


/***************************************************************************************
 * Generic getter/setter commands for simple numeric parameters. 
 *     MOVE TO config.c  ??
 *
 * Note: the ee_addr and default_val arguments are defined by macros (see config.h) 
 * and are further passed to get_param() and set_param() 
 ***************************************************************************************/
 
static void _parameter_setting_uint16(uint8_t argc, char** argv, Stream* out, 
                void* ee_addr, PGM_P default_val, uint16_t lower, uint16_t upper, PGM_P pfmt, PGM_P sfmt,
                bool help, PGM_P helptext)
{
    int x;
    if (help)
       putstr_P(out, helptext);
    else if (argc > 1) {
       if (sscanf_P(argv[1], sfmt, &x) != 1 || x<lower || x>upper) {
          sprintf_P(buf, PSTR("ERROR: parameter must be a number in range %d-%d\r\n"),lower,upper);
          putstr(out,buf); 
       }
       else {
          set_param(ee_addr, &x, sizeof(uint16_t));
          putstr_P(out,PSTR("Ok\r\n"));
       }
    } 
    else {
       get_param(ee_addr, &x, sizeof(uint16_t), default_val); 
       sprintf_P(buf, pfmt, x);
       putstr(out, buf);
    }
}


static void _parameter_setting_uint8(uint8_t argc, char** argv, Stream* out, 
                void* ee_addr, PGM_P default_val, uint8_t lower, uint8_t upper, PGM_P pfmt, PGM_P sfmt, 
                bool help, PGM_P helptext)
{
    int x;
    if (help)
       putstr_P(out, helptext);
    else if (argc > 1) {
       if (sscanf_P(argv[1], sfmt, &x) != 1 || x<lower || x>upper) {
          sprintf_P(buf, PSTR("ERROR: parameter must be a number in range %d-%d\r\n"),lower,upper);
          putstr(out,buf);
       }
       else {
          set_byte_param(ee_addr, (uint8_t) x);
          putstr_P(out, PSTR("Ok\r\n"));
       }
    } 
    else {
       x = get_byte_param(ee_addr, default_val); 
       sprintf_P(buf, pfmt, x);
       putstr(out, buf);
    }
}


static void _parameter_setting_bool(uint8_t argc, char** argv, Stream* out, 
                void* ee_addr, PGM_P default_val, PGM_P name,
                bool help, PGM_P helptext)
{
    if (help)
      { putstr_P(out, helptext); return; }
    if (argc < 2) {
       putstr_P(out, name);
       if (get_byte_param(ee_addr, default_val)) 
          putstr_P(out, PSTR(" ON\r\n"));
       else
          putstr_P(out, PSTR(" OFF\r\n"));
       return; 
    }
    if (strncasecmp("on", argv[1], 2) == 0 || strncasecmp("true", argv[1], 1) == 0) {
       putstr_P(out, PSTR("Ok\r\n"));
       set_byte_param(ee_addr, (uint8_t) 1);
    }  
    else if (strncasecmp("off", argv[1], 2) == 0 || strncasecmp("false", argv[1], 1) == 0) {
       putstr_P(out, PSTR("Ok\r\n"));
       set_byte_param(ee_addr, (uint8_t) 0);
    }
    else 
       putstr_P(out, PSTR("ERROR: parameter must be 'ON' or 'OFF'\r\n"));
    
    
}

typedef void (*cmd_func)(uint8_t, char**, Stream*, Stream*);

static void _do_command(cmd_func f, bool help, PGM_P helptext, uint8_t argc, char** argv, Stream* out, Stream* in)
{
    if (help)
       putstr_P(out, helptext);
    else {
       (*f)(argc, argv, out, in);
    }
}


static bool _cmpCmd(char* command, char* c, uint8_t cmpchars) 
{
    uint8_t n = strlen(c);
    if (n<cmpchars)
       return false;
    return ((strncasecmp(command, c, n) == 0) && n <= strlen(command));
}


/***************************************************************************************
 * Macro to test for command to get or set numeric parameter. 
 * Usage: 
 *
 *   IF_COMMAND_PARAM_type(  command, cmpchars, 
 *                          argc, argv, outstream, 
 *                          paramname, printformat, scanformat)
 *
 *   paramname   = name of parameter (as defined in config.h)
 *   printformat = Format string (in program memory) for showing value of parameter. 
 *                 It  must contain one "%d" or equivalent.
 *   scanformat  = Format string (in program memory) for reading value of parameter. 
 *                 It must contain one "%d" or equivalent. 
 ***************************************************************************************/

#define IF_COMMAND_PARAM_uint16(c, command, cmpchars, argc, argv, out, x, lower, upper, pfmt, sfmt, help, helptext)  \
    if (_cmpCmd(command, c, cmpchars)) \
        _parameter_setting_uint16(argc, argv, out, &PARAM_##x, (PGM_P) &PARAM_DEFAULT_##x, lower, upper, pfmt, sfmt, help, helptext) 

#define IF_COMMAND_PARAM_uint8(c, command, cmpchars, argc, argv, out, x, lower, upper, pfmt, sfmt, help, helptext)  \
    if (_cmpCmd(command, c, cmpchars)) \
        _parameter_setting_uint8(argc, argv, out, &PARAM_##x, (PGM_P) &PARAM_DEFAULT_##x, lower, upper, pfmt, sfmt, help, helptext) 

#define IF_COMMAND_PARAM_bool(c, command, cmpchars, argc, argv, out, x, name, help, helptext)  \
    if (_cmpCmd(command, c, cmpchars)) \
        _parameter_setting_bool(argc, argv, out, &PARAM_##x, (PGM_P) &PARAM_DEFAULT_##x, name, help, helptext) 

#define IF_COMMAND(c, command, cmpchars, f, argc, argv, out, in, help, helptext) \
    if (_cmpCmd(command, c, cmpchars)) \
        _do_command(f, help, helptext, argc, argv, out, in)


/**************************************************************************
 * Read and process commands .
 *    Not thread safe. Use in at most one thread, use lock, or allocate
 *    buf on the stack instead. 
 **************************************************************************/

bool readLine(Stream*, Stream*, char*, const uint16_t); 
 /* Move to stream.h, or move the actual code here... */
       
void cmdProcessor(Stream *in, Stream *out)
{
    char* argv[MAXTOKENS];
    uint8_t argc;
    sleep (10);
    putstr_P(out, PSTR("\r\n\r\n**************************************************************"));
#if defined TARGET_USBKEY
    putstr_P(out, PSTR("\n\r Welcome to 'Polaric USBKEY' firmware "));        
#else 
    putstr_P(out, PSTR("\n\r Welcome to 'Polaric Tracker' firmware "));
#endif    
    putstr_P(out, PSTR(VERSION_STRING));
    putstr_P(out, PSTR("\r\n (C) 2010-2014 LA3T Tromsogruppen av NRRL and contributors.."));
    putstr_P(out, PSTR("\r\n This is free software (see source code on github)"));  
    putstr_P(out, PSTR("\r\n**************************************************************\r\n\r\n"));
    
    while (1) {
         memset(argv, 0, MAXTOKENS);
         putstr(out, "cmd: ");    
         readLine(in, out, buf, BUFSIZE);
         bool help = false;
         
         /* Split input line into argument tokens */
         argc = tokenize(buf, argv, MAXTOKENS, " \t\r\n,", true);
         char* arg = argv[0]; 
         if (_cmpCmd("help", arg, 3) || strcmp("?", arg) == 0)
         {
             if (argc < 2) {
                putstr_P(out, PSTR("Available commands: \r\n"));
                putstr_P(out, PSTR("  afc, altitude, autopower, beep, boot, bootsound, btext, compress, converse,\r\n")); 
                putstr_P(out, PSTR("  dest, digipeater, digi-sar, digi-wide1, fcal, extraturn, fakereports, \r\n")); 
                putstr_P(out, PSTR("  freq, gps, listen,  maxframe, maxpause, maxturn, mindist, minpause, \r\n"));
                putstr_P(out, PSTR("  mycall, oident, osymbol,  path, powersave,  repeat, reset, rssi, squelch, \r\n"));
                putstr_P(out, PSTR("  statustime, symbol, testpacket, timestamp, teston, tracker, tracktime, \r\n"));
                putstr_P(out, PSTR("  txdelay, txon, txmon, txtail, txtone, version\r\n"));
                putstr_P(out, PSTR("\r\nMore info: \r\n  help <command> or ? <command>\r\n\r\n"));
                continue;
             }
             arg = argv[1];
             help = true;
         }
         
         /* Select command handler: 
          * misc commands 
          */         
         IF_COMMAND(arg, "teston", 6, do_teston, argc, argv, out, in,
              help, PSTR("Send test signal with given pattern (type a key to turn off)\r\n"));
         else IF_COMMAND(arg, "version", 3, do_version, argc, argv, out, in, 
              help, PSTR("Show firmware version\r\n"));
         else IF_COMMAND(arg, "txtone", 5, do_txtone, argc, argv, out, in, 
              help, PSTR("TXTONE HIGH|LOW\r\nSend test signal with high or low tone (type a key to turn off)\r\n"));
         else IF_COMMAND(arg, "testpacket", 5, do_testpacket, argc, argv, out, in, 
              help, PSTR("Send test packet\r\n"));                               
         else IF_COMMAND(arg, "gps", 2, do_nmea, argc, argv, out, in, 
              help, PSTR("GPS ON|OFF|NMEA|POS:\r\n  GPS ON/OFF - turn on or off GPS (should only be used when TRACKER is OFF)\r\n  GPS NMEA - show nmea stream\r\n  GPS POS - show valid positions\r\n"));     
         else IF_COMMAND(arg, "tracker", 6, do_tracker, argc, argv, out, in,
              help, PSTR("Turn on or off automatic tracking\r\n"));
         else IF_COMMAND(arg, "digipeater", 4, do_digipeater, argc, argv, out, in,
              help, PSTR("Turn on or off digipeater\r\n"));         
         else IF_COMMAND(arg, "txon", 4, do_txon, argc, argv, out, in,
              help, PSTR("Turn on transmitter (type a key to turn off)\r\n"));
         else IF_COMMAND(arg, "rssi", 2, do_rssi, argc, argv, out, in,
              help, PSTR("Show signal strength on receiver (type a key to turn off)\r\n"));       
         else IF_COMMAND(arg, "trace", 5, do_trace, argc, argv, out, in, 
              help, PSTR("Debugging info (for developers)\r\n")); 
         else IF_COMMAND(arg, "ps", 2, do_ps, argc, argv, out, in, 
              help, PSTR("Process status. Show info about tasks (for developers)\r\n"));    
         else IF_COMMAND(arg, "vbatt", 2, do_vbatt, argc, argv, out, in,
              help, PSTR("Show battery voltage\r\n"));
         else IF_COMMAND(arg, "listen", 3, do_listen, argc, argv, out, in, 
              help, PSTR("Enter listen mode. Show incoming packets on console (CTRL-C to leave)\r\n"));            
         else if (strcasecmp("k", arg) == 0 || _cmpCmd("converse", arg, 4))
             _do_command( do_converse, help, 
                PSTR("Enter converse mode. Show incoming packets. Send typed text as packets (CTRL-C to leave\r\n"), 
                argc, argv, out, in );   
         else IF_COMMAND(arg, "reset", 5, do_reset, argc, argv, out, in,
               help, PSTR("Reset all settings to defaults\r\n"));      
	      else if (strcasecmp("protocol", arg) == 0) 
	           putstr_P(out,PSTR("protocol 1\r\n"));
	             
         /* Commands for setting/viewing parameters */
         else IF_COMMAND(arg, "mycall", 2, do_mycall, argc, argv, out, in, 
              help, PSTR("Callsign\r\n"));    
         else IF_COMMAND(arg, "oident", 3, do_obj_id, argc, argv, out, in,
              help, PSTR("Ident prefix for object reports\r\n"));           
         else IF_COMMAND(arg, "dest", 3, do_dest, argc, argv, out, in,
              help, PSTR("Destination address\r\n"));  
         else IF_COMMAND(arg, "path", 3, do_digipath, argc, argv, out, in,
              help, PSTR("Digipeater path\r\n"));
         else IF_COMMAND(arg, "symbol", 3, do_symbol, argc, argv, out, in,
              help, PSTR("Symbol for position reports. <symtable> <symbol>\r\n"));  
         else IF_COMMAND(arg, "osymbol", 4, do_obj_symbol, argc, argv, out, in,
              help, PSTR("Symbol for object reports. <symtable> <symbol>\r\n"));                       
         else IF_COMMAND(arg, "freq", 2, do_freq, argc, argv, out, in,
              help, PSTR("Frequency of transceiver (in kHz)\r\n"));  
         else IF_COMMAND(arg, "fcal", 3, do_fcal, argc, argv, out, in,
              help, PSTR("Frequency calibration (in Hz)\r\n"));
         else IF_COMMAND(arg, "txpower", 4, do_power, argc, argv, out, in, 
              help, PSTR("TX output from ADF7021 chip (in dBm)\r\n"));    
         else IF_COMMAND(arg, "squelch", 2, do_squelch, argc, argv, out, in, 
              help, PSTR("Receiver signal threshold\r\n"));
         else IF_COMMAND(arg, "btext", 2, do_btext, argc, argv, out, in,
              help, PSTR("Beacon text (comment to be sent with position reports\r\n")); 

         else IF_COMMAND_PARAM_uint8
                  ( arg, "txdelay", 3, argc, argv, out,
                    TXDELAY, 0, 200, PSTR("TXDELAY %d\r\n\0"), PSTR(" %d"), 
                    help, PSTR("Number of FLAGs to open a transmission (range 0-200)\r\n"));
         else IF_COMMAND_PARAM_uint8
                  ( arg, "txtail", 3, argc, argv, out,
                    TXTAIL, 0, 200, PSTR("TXTAIL %d\r\n\0"), PSTR(" %d"),
                    help, PSTR("Number of FLAGs to close a transmission (range 0-200)\r\n") );
         else IF_COMMAND_PARAM_uint8
                  ( arg, "maxframe", 5, argc, argv, out,
                    MAXFRAME, 1, 7, PSTR("MAXFRAME %d\r\n\0"), PSTR(" %d"),
                    help, PSTR("Max number of frames to send in the same transmission (range 1-7) \r\n") );
         else IF_COMMAND_PARAM_uint16
                 ( arg, "afc", 3, argc, argv, out, 
                    TRX_AFC, 0, 12000, PSTR("AFC %d\r\n\0"), PSTR(" %d"),
                    help, PSTR("Frequency range (range 0-12000 Hz) for RX AFC\r\n") );
         else IF_COMMAND_PARAM_uint8
                 ( arg, "tracktime", 6, argc, argv, out, 
                   TRACKER_SLEEP_TIME, GPS_FIX_TIME+1, 240, PSTR("TRACKTIME %d\r\n\0"), PSTR(" %d"),
                   help, PSTR("Smallest possible interval between position reports (in seconds, max 240)\r\n") );
         else IF_COMMAND_PARAM_uint16
                 ( arg, "deviation", 3, argc, argv, out,
                   TRX_AFSK_DEV, 0, 5000, PSTR("DEVIATION %d\r\n\0"), PSTR(" %d"),
                   help, PSTR("Frequency deviation for FSK modulation (range 0-5000 Hz)\r\n") );
         else IF_COMMAND_PARAM_uint16 
                 (  arg, "gpsbaud", 4, argc, argv, out, 
                    GPS_BAUD, 1200, 19200, PSTR("GPSBAUD %d\r\n\0"), PSTR(" %d"),
                    help, PSTR("Baud rate for serial comm with GPS unit\r\n") );
         else IF_COMMAND_PARAM_uint16 
                 ( arg, "maxturn", 5, argc, argv, out, 
                    TRACKER_TURN_LIMIT, 0, 180, PSTR("MAXTURN %d\r\n\0"), PSTR(" %d"), 
                    help, PSTR("Course change threshold (0-180 degrees)\r\n") );
         else IF_COMMAND_PARAM_uint8 
                 ( arg, "maxpause", 5, argc, argv, out, 
                    TRACKER_MAXPAUSE, 1, 200, PSTR("MAXPAUSE %d\r\n\0"), PSTR(" %d"),
                    help, PSTR("Max time between tracker position reports (in TRACKTIME units)\r\n") );
         else IF_COMMAND_PARAM_uint8 
                 ( arg, "minpause", 5, argc, argv, out, 
                    TRACKER_MINPAUSE, 1, 200, PSTR("MINPAUSE %d\r\n\0"), PSTR(" %d"),
                    help, PSTR("Min time between tracker position reports in straight forward movement (in TRACKTIME units)\r\n") );
         else IF_COMMAND_PARAM_uint8 
                 ( arg, "mindist", 5, argc, argv, out, 
                    TRACKER_MINDIST, 1, 200, PSTR("MINDIST %d\r\n\0"), PSTR(" %d"),
                    help, PSTR("Distance between position reports when moving slowly (in meters)\r\n") );
         else IF_COMMAND_PARAM_uint8 
                 ( arg, "statustime", 7, argc, argv, out, 
                    STATUS_TIME, 1, 200, PSTR("STATUSTIME %d\r\n\0"), PSTR(" %d"), 
                    help, PSTR("Time between status reports (in TRACKTIME units)\r\n") );
         else IF_COMMAND_PARAM_bool 
                 ( arg, "altitude", 3, argc, argv, out, ALTITUDE_ON, PSTR("ALTITUDE"),
                    help, PSTR("Send altitude with position reports (on/off)\r\n") );
         else IF_COMMAND_PARAM_bool           
                 ( arg, "timestamp", 4, argc, argv, out, TIMESTAMP_ON, PSTR("TIMESTAMP"),
                    help, PSTR("Send timestamp with position reports (on/off)\r\n") );
         else IF_COMMAND_PARAM_bool
                 ( arg, "compress", 4, argc, argv, out, COMPRESS_ON, PSTR("COMPRESS"),
                   help, PSTR("Compress position reports (on/off)\r\n") );
         else IF_COMMAND_PARAM_bool
                 ( arg, "powersave", 6, argc, argv, out, GPS_POWERSAVE, PSTR("POWERSAVE"),
                   help, PSTR("Try to save power by turning off GPS when not moving (on/off)\r\n") );  
         else IF_COMMAND_PARAM_bool
                 ( arg, "beep", 2, argc, argv, out, REPORT_BEEP, PSTR("BEEP"),
                   help, PSTR("Beep when sending position reports automatically (on/off)\r\n") );        
         else IF_COMMAND_PARAM_bool
                 ( arg, "txmon", 3, argc, argv, out, TXMON_ON, PSTR("TXMON"),
                   help, PSTR("Monitor transmitted packets in LISTEN or CONVERSE mode (on/off)\r\n") );
         else IF_COMMAND_PARAM_bool
                 ( arg, "autopower", 4, argc, argv, out, AUTOPOWER, PSTR("AUTOPOWER"),
                   help, PSTR("External power supply turns on/off tracker automatically (on/off)\r\n") );
         else IF_COMMAND_PARAM_bool
                 ( arg, "repeat", 3, argc, argv, out, REPEAT, PSTR("REPEAT"),
                    help, PSTR("EXPERIMENTAL: Extra transmission of position reports (on/off)\r\n") );
	 else IF_COMMAND_PARAM_bool
	         (arg, "extraturn", 4, argc, argv, out, EXTRATURN, PSTR("EXTRATURN"),
		    help, PSTR("EXPERIMENTAL: If reporting is triggered by course change, add previous position (on/off)\r\n") );
	 else IF_COMMAND_PARAM_bool
                 ( arg, "bootsound", 5, argc, argv, out, BOOT_SOUND, PSTR("BOOTSOUND"),
                    help, PSTR("Sound signal at boot (on/off)\r\n") );	 
         else IF_COMMAND_PARAM_bool
                   ( arg, "digi-sar", 6, argc, argv, out, DIGIPEATER_SAR, PSTR("DIGI-SAR"),
                   help, PSTR("Digipeater. Preemption on SAR alias\r\n") );
         else IF_COMMAND_PARAM_bool
           ( arg, "digi-wide1", 6, argc, argv, out, DIGIPEATER_WIDE1, PSTR("DIGI-WIDE1"),
                   help, PSTR("Digipeater. Preemption on SAR alias\r\n") );        
         else IF_COMMAND_PARAM_bool
                 ( arg, "fakereports", 6, argc, argv, out, FAKE_REPORTS, PSTR("FAKEREPORTS"),
                   help, PSTR("EXPERIMENTAL: In LISTEN or CONVERSE mode, display position reports every TRACKTIME (on/off)\r\n") ); 	     
         else IF_COMMAND(arg, "boot", 4, do_boot, argc, argv, out, in,
               help, PSTR("Invoke bootloader for firmware upgrade\r\n"));
         else if (strlen(arg) > 0)
             putstr_P(out, PSTR("*** Unknown command\r\n"));
         else              continue; 
         putstr(out,"\r\n");         
   }
}   


/************************************************
 * Invoke bootloader
 ************************************************/

typedef void (*AppPtr_t)(void) ATTR_NO_RETURN;

static void do_boot(uint8_t argc, char** argv, Stream* out, Stream* in)
{
  /* Turn off external devices */
  USB_ShutDown ();
  gps_off ();
  adf7021_power_off ();
  clear_port (HIGH_CHARGE);
  clear_port (USB_CHARGER);
  rgb_led_off ();
  clear_port (LED1);
  clear_port (LED2);

  /* Disable all interrupts */
  TIMSK0 = TIMSK1 = TIMSK2 = TIMSK3 = 0;
  ADCSRA = UCSR1B = PCICR = EIMSK = 0;
  cli ();

  AppPtr_t bootloader_entry_point = 0x1E000;
  bootloader_entry_point ();
}

/************************************************
 * Reset all settings
 ************************************************/
 
static void do_reset(uint8_t argc, char** argv, Stream* out, Stream* in)
{
   reset_all_param();
   putstr_P(out, PSTR("Ok (reset settings)\r\n"));
}

/************************************************
 * Report firmware version
 ************************************************/
 
static void do_version(uint8_t argc, char** argv, Stream* out, Stream* in)
{
   putstr_P(out,PSTR(VERSION_STRING));
   putstr_P(out,PSTR("\r\n"));
}



/************************************************
 * Report RSSI level 
 ************************************************/

static void do_rssi(uint8_t argc, char** argv, Stream* out, Stream* in)
{
    int i;    
    radio_require();
    stream_get_nb(in);
    for (;;)
    {
        sleep(25);
        double x = adf7021_read_rssi();
        sprintf_P(buf, PSTR("RSSI: %4.0f :\0"), x);
        putstr(out, buf);
        for (i=0; i < (125+x)/2; i++)
           putch(out,'*');
        putstr_P(out, PSTR("\r\n"));
        if (stream_get_nb(in) > 10)
           break;
    }
    radio_release();
}



/************************************************
 * Report battery voltage
 ************************************************/
 
static void do_vbatt(uint8_t argc, char** argv, Stream* out, Stream* in)
{
   sprintf_P(buf, PSTR("Battery voltage: %.2f V\r\n\0"), 
             batt_voltage());
   putstr(out, buf);
}



/************************************************
 * Decode and show incoming packets
 ************************************************/
 
static void do_listen(uint8_t argc, char** argv, Stream* out, Stream* in)
{
   putstr_P(out, PSTR("***** LISTEN ON RECEIVER *****\r\n"));
   radio_require();
   afsk_enable_decoder();
   mon_activate(true);
   getch(in);
   mon_activate(false);
   afsk_disable_decoder();
   radio_release();
}



/************************************************
 * Converse mode
 ************************************************/

static void do_converse(uint8_t argc, char** argv, Stream* out, Stream* in)
{
   FBUF packet; 
   putstr_P(out, PSTR("***** CONVERSE MODE *****\r\n"));
   radio_require();
   afsk_enable_decoder();
   mon_activate(true); 
   while ( readLine(in, out, buf, BUFSIZE)) { 
        fbuf_new(&packet);
        addr_t from, to; 
        GET_PARAM(MYCALL, &from);
        GET_PARAM(DEST, &to);       
        addr_t digis[7];
        uint8_t ndigis = GET_BYTE_PARAM(NDIGIS); 
        GET_PARAM(DIGIS, &digis);   
        ax25_encode_header(&packet, &from, &to, digis, ndigis, FTYPE_UI, PID_NO_L3);
        fbuf_putstr(&packet, buf);                        
        fbq_put(outframes, packet);
   }
   mon_activate(false);
   afsk_disable_decoder(); 
   radio_release();
}




/*********************************************
 * tx : Send AX25 test packet
 *********************************************/

static void do_testpacket(uint8_t argc, char** argv, Stream* out, Stream* in)
{ 
  FBUF packet;    
  addr_t from, to; 
  radio_require();
  GET_PARAM(MYCALL, &from);
  GET_PARAM(DEST, &to);       
  addr_t digis[7];
  uint8_t ndigis = GET_BYTE_PARAM(NDIGIS); 
  GET_PARAM(DIGIS, &digis);   
  fbuf_new(&packet);
  ax25_encode_header(&packet, &from, &to, digis, ndigis, FTYPE_UI, PID_NO_L3);
  fbuf_putstr_P(&packet, PSTR("The lazy brown dog jumps over the quick fox 1234567890"));                      
  putstr_P(out, PSTR("Sending (AX25 UI) test packet....\r\n"));       
  fbq_put(outframes, packet);
  radio_release();
}



static void do_digipeater(uint8_t argc, char** argv, Stream* out, Stream* in)
{
  if (argc < 2)
  {
    if (GET_BYTE_PARAM(DIGIPEATER_ON))
      putstr_P(out, PSTR("DIGIPEATER ON\r\n"));
    else
      putstr_P(out, PSTR("DIGIPEATER OFF\r\n"));
    return;
  }
  if (strncasecmp("on", argv[1], 2) == 0) {   
    putstr_P(out, PSTR("Ok\r\n"));
    SET_BYTE_PARAM(DIGIPEATER_ON, true);
    digipeater_activate(true);
  }  
  else if (strncasecmp("off", argv[1], 2) == 0) {     
    putstr_P(out, PSTR("Ok\r\n"));
    SET_BYTE_PARAM(DIGIPEATER_ON, false);
    digipeater_activate(false);
  }
  else {
    putstr_P(out, PSTR("ERROR: parameter must be 'ON' or 'OFF'\r\n"));
  }
}




/************************************************
 * For testing of GPS .....
 ************************************************/
 
Semaphore nmea_run; 

static void do_nmea(uint8_t argc, char** argv, Stream* out, Stream* in)
{                                                                                                            
  if (argc < 2) {
      putstr_P(out, PSTR("Usage: GPS on|off|nmea|pos\r\n"));
      return;
  }
  else if (strncasecmp("on", argv[1], 2) == 0) {
      putstr_P(out, PSTR("***** GPS ON *****\r\n"));
      gps_on();
      return;
  } 
  else if (strncasecmp("off", argv[1], 2) == 0) {
      putstr_P(out, PSTR("***** GPS OFF *****\r\n"));
      gps_off();
      return;
  }  
  else if (strncasecmp("nmea", argv[1], 1) == 0) {
      putstr_P(out, PSTR("***** NMEA PACKETS *****\r\n"));
      gps_mon_raw();
  } 
  else if (strncasecmp("pos", argv[1], 3) == 0){
      putstr_P(out, PSTR("***** VALID POSITION REPORTS (GPRMC) *****\r\n"));
      gps_mon_pos();
  }     
  else
     return;

  /* And wait until some character has been typed */
  getch(in);
  gps_mon_off();
}




/************************************************
 * For testing of tracker .....
 ************************************************/

static void do_tracker(uint8_t argc, char** argv, Stream* out, Stream* in)
{
  if (argc < 2)
  {
      if (GET_BYTE_PARAM(TRACKER_ON))
          putstr_P(out, PSTR("TRACKER ON\r\n"));
      else
          putstr_P(out, PSTR("TRACKER OFF\r\n"));
      return;
  }
  if (strncasecmp("on", argv[1], 2) == 0) {   
      putstr_P(out, PSTR("Ok\r\n"));
      tracker_on();
  }  
  else if (strncasecmp("off", argv[1], 2) == 0) {     
      putstr_P(out, PSTR("Ok\r\n"));
      tracker_off();
  }
  else {
        putstr_P(out, PSTR("ERROR: parameter must be 'ON' or 'OFF'\r\n"));
  }
}



/************************************************
 * For testing of transmitter .....
 ************************************************/

static void do_txon(uint8_t argc, char** argv, Stream* out, Stream* in)
{
   putstr_P(out, PSTR("***** TX ON *****\r\n"));
   radio_require();
   set_port(LED2);
   adf7021_enable_tx();

   getch(in);
   adf7021_disable_tx();
   clear_port(LED2);
   radio_release();
}


      
/*********************************************
 * teston <byte> : Generate test signal
 *********************************************/
 
static void do_teston(uint8_t argc, char** argv, Stream* out, Stream* in)
{
    int ch = 0;
    if (argc < 2) {
        putstr_P(out, PSTR("Usage: TESTON <byte>\r\n"));
        return;
    }
    sscanf(argv[1], " %i", &ch);
    radio_require();  
    hdlc_test_on((uint8_t) ch);
    sprintf_P(buf, PSTR("**** TEST SIGNAL: 0x%X ****\r\n"), ch);
    putstr(out, buf );  
    
     /* And wait until some character has been typed */
    getch(in);
    hdlc_test_off();
    radio_release();
}


static void do_txtone(uint8_t argc, char** argv, Stream* out, Stream* in)
{
  if (argc < 2) {
      putstr_P(out, PSTR("Usage: TXTONE high|low\r\n"));
      return;
  }
 
  radio_require();
  if (strncasecmp("hi", argv[1], 2) == 0) {
      putstr_P(out, PSTR("***** TEST TONE HIGH *****\r\n"));
      afsk_high_tone(true);
      hdlc_test_on(0xff);
  }
  else if (strncasecmp("low", argv[1], 2) == 0) {
      putstr_P(out, PSTR("***** TEST TONE LOW *****\r\n"));
      afsk_high_tone(false);
      hdlc_test_on(0xff);
  } 
  
 /* And wait until some character has been typed */
  getch(in);
  hdlc_test_off();
  radio_release();
}




/************************************************
 * Set identifier of object
 ************************************************/
 
static void do_obj_id(uint8_t argc, char** argv, Stream* out, Stream* in)
{
    if (argc > 1){
        if (strlen(argv[1]) <= 9)
        {
           SET_PARAM(OBJ_ID, argv[1]);
           putstr_P(out,PSTR("Ok\r\n"));
        }
        else
           putstr_P(out,PSTR("ERROR: Id cannot be longer than 9 characters\r\n"));
    }
    else {
        char x[10];
        GET_PARAM(OBJ_ID, x);
        sprintf_P(buf, PSTR("OIDENT \"%s\"\r\n\0"), x);
        putstr(out, buf);
    }
}
         
         
/*********************************************
 * config: mycall (sender address)
 *********************************************/
 
static void do_mycall(uint8_t argc, char** argv, Stream* out, Stream* in)
{
   addr_t x;
   char cbuf[11]; 
   if (argc > 1) {
      str2addr(&x, argv[1], false);
      SET_PARAM(MYCALL, &x);
      putstr_P(out,PSTR("Ok\r\n"));
   }
   else {
      GET_PARAM(MYCALL, &x);
      sprintf_P(buf, PSTR("MYCALL %s\r\n\0"), addr2str(cbuf, &x));
      putstr(out, buf);
   }   
}

         
/*********************************************
 * config: dest (destination address)
 *********************************************/
 
static void do_dest(uint8_t argc, char** argv, Stream* out, Stream* in)
{
   addr_t x;
   char cbuf[11]; 
   if (argc > 1) {
      str2addr(&x, argv[1], false);
      SET_PARAM(DEST, &x);
      putstr_P(out,PSTR("Ok\r\n"));
   }
   else {
      GET_PARAM(DEST, &x);
      sprintf_P(buf, PSTR("DEST %s\r\n\0"), addr2str(cbuf, &x));
      putstr(out, buf);
   }   
}



/*********************************************
 * config: digipeater path
 *********************************************/
 
static void do_digipath(uint8_t argc, char** argv, Stream* out, Stream* in)
{
    __digilist_t digis;
    uint8_t ndigis;
    uint8_t i;
    char cbuf[11]; 
    
    if (argc > 1) {
       if (argc==2 && strncasecmp("off", argv[1], 3)==0)
           ndigis = 0;
       else{
         ndigis = argc - 1;
         if (ndigis > 7) 
             ndigis = 7;
         for (i=0; i<ndigis; i++)
             str2addr(&digis[i], argv[i+1], false);
         SET_PARAM(DIGIS, digis);     
       }
       SET_BYTE_PARAM(NDIGIS, ndigis);
       putstr_P(out,PSTR("Ok\r\n"));
    }
    else  {
       ndigis = GET_BYTE_PARAM(NDIGIS);
       GET_PARAM(DIGIS, &digis);
       putstr_P(out, PSTR("PATH ")); 
       if (ndigis==0)
           putstr_P(out, PSTR("<EMPTY>\r\n"));
       for (i=0; i<ndigis; i++)
       {   
           putstr(out, addr2str(cbuf, &digis[i]));           
           if (i < ndigis-1)
               putstr_P(out, PSTR(", ")); 
       }
       putstr_P(out,PSTR("\n\r"));
    }
}


/***********************************************
 * config: Beacon text (comment in pos reports)
 ***********************************************/
 
static void do_btext(uint8_t argc, char** argv, Stream* out, Stream* in)
{
    if (argc > 1){
        SET_PARAM(REPORT_COMMENT, argv[1]);
        putstr_P(out,PSTR("Ok\r\n"));
    }
    else {
        char x[COMMENT_LENGTH];
        GET_PARAM(REPORT_COMMENT, x);
        sprintf_P(buf, PSTR("BTEXT \"%s\"\r\n\0"), x);
        putstr(out, buf);
    }
}


/************************************************
 * Show trace of current and previous run
 * (for debugging)
 ************************************************/
 
static void do_trace(uint8_t argc, char** argv, Stream* out, Stream* in)
{
    show_trace(buf, 0, PSTR("Current run  = "), PSTR("\r\n"));
    putstr(out,buf);
    show_trace(buf, 1, PSTR("Previous run = "), PSTR("\r\n"));
    putstr(out, buf);
}



/************************************************
 * Show info about tasks (ps command)
 ************************************************/
 
static void do_ps(uint8_t argc, char** argv, Stream* out, Stream* in)
{
   uint8_t running = t_nRunning();
   sprintf_P(buf, PSTR("Tasks running        : %d\r\n\0"), running);
   putstr(out, buf);   
   sprintf_P(buf, PSTR("Tasks sleeping       : %d\r\n\0"), t_nTasks()- t_nTerminated() - running);
   putstr(out, buf);
   if (t_nTerminated()) {
      sprintf_P(buf, PSTR("Tasks terminated     : %d\r\n\0"), t_nTerminated());
      putstr(out, buf);
   }
   sprintf_P(buf, PSTR("Total avail. buffers : %d bytes\r\n\0"), FBUF_SLOTS * FBUF_SLOTSIZE);
   putstr(out, buf);   
   sprintf_P(buf, PSTR("Free buffers         : %d bytes\r\n\0"), fbuf_freeSlots()*FBUF_SLOTSIZE);
   putstr(out, buf);
   sprintf_P(buf, PSTR("Stack space allocated: %d bytes\r\n\0"), t_stackUsed());
   putstr(out, buf);
   putstr_P(out, PSTR("Stack high task      : ")); 
   if (t_stackHigh() == 255)
      sprintf_P(buf, PSTR("none\r\n\0"));
   else
      sprintf_P(buf, PSTR("%d\r\n\0"), t_stackHigh());
   putstr(out, buf);
}



/*********************************************
 * config: symbol (APRS symbol/symbol table)
 *********************************************/
 
static void do_symbol(uint8_t argc, char** argv, Stream* out, Stream* in)
{
   if (argc > 2) {
      SET_BYTE_PARAM(SYMBOL_TABLE, *argv[1]);
      SET_BYTE_PARAM(SYMBOL, *argv[2]);
      putstr_P(out,PSTR("Ok\r\n"));
   }
   else {
      sprintf_P(buf, PSTR("SYMBOL %c %c\r\n\0"),
           GET_BYTE_PARAM(SYMBOL_TABLE), GET_BYTE_PARAM(SYMBOL));
      putstr(out, buf);
   }   
}



/****************************************************
 * config: object symbol (APRS symbol/symbol table)
 ****************************************************/

static void do_obj_symbol(uint8_t argc, char** argv, Stream* out, Stream* in)
{
   if (argc > 2) {
      SET_BYTE_PARAM(OBJ_SYMBOL_TABLE, *argv[1]);
      SET_BYTE_PARAM(OBJ_SYMBOL, *argv[2]);
      putstr_P(out,PSTR("Ok\r\n"));
   }
   else {
      sprintf_P(buf, PSTR("SYMBOL %c %c\r\n\0"),
           GET_BYTE_PARAM(OBJ_SYMBOL_TABLE), GET_BYTE_PARAM(OBJ_SYMBOL));
      putstr(out, buf);
   }   
}



/*********************************************
 * config: transceiver frequency
 *********************************************/
 
static void do_freq(uint8_t argc, char** argv, Stream* out, Stream* in)
{
    uint32_t x = 0;
    if (argc > 1) {
       sscanf(argv[1], " %ld", &x);
       x *= 1000;   
       SET_PARAM(TRX_FREQ, &x);
       putstr_P(out,PSTR("Ok\r\n"));
    } 
    else {
       GET_PARAM(TRX_FREQ, &x);
       sprintf_P(buf, PSTR("FREQ %ld\r\n\0"), x/1000);
       putstr(out, buf);
    }
}



/*********************************************
 * config: transceiver frequency calibration
 *********************************************/
 
static void do_fcal(uint8_t argc, char** argv, Stream* out, Stream* in)
{
    int16_t x = 0;
    if (argc > 1) {
       sscanf(argv[1], " %d", &x);
       SET_PARAM(TRX_CALIBRATE, &x);
       putstr_P(out,PSTR("Ok\r\n"));
    } 
    else {
       GET_PARAM(TRX_CALIBRATE, &x);
       sprintf_P(buf, PSTR("FCAL %d\r\n\0"), x);
       putstr(out, buf);
    }
}


/*********************************************
 * config: transmitter power (dBm)
 *********************************************/
 
static void do_power(uint8_t argc, char** argv, Stream* out, Stream* in)
{
    float x = 0;
    if (argc > 1) {
       sscanf(argv[1], " %f", &x);  
       SET_PARAM(TRX_TXPOWER, &x);
       putstr_P(out,PSTR("Ok\r\n"));
    } 
    else {
       GET_PARAM(TRX_TXPOWER, &x);
       sprintf_P(buf, PSTR("TXPOWER %f\r\n\0"), x);
       putstr(out, buf);
    }
}


/*********************************************
 * config: squelch level (dBm)
 *********************************************/
 
static void do_squelch(uint8_t argc, char** argv, Stream* out, Stream* in)
{
    float x = 0;
    if (argc > 1) {
       sscanf(argv[1], " %f", &x);  
       SET_PARAM(TRX_SQUELCH, &x);
       putstr_P(out,PSTR("Ok\r\n"));
    } 
    else {
       GET_PARAM(TRX_SQUELCH, &x);
       sprintf_P(buf, PSTR("SQUELCH %f\r\n\0"), x);
       putstr(out, buf);
    }
}


/****************************************************************************
 * split input string into tokens - returns number of tokens found
 *
 * ARGUMENTS: 
 *   buf       - text buffer to tokenize
 *   tokens    - array in which to store pointers to tokens
 *   maxtokens - maximum number of tokens to scan for
 *   delim     - characters which can be used as delimiters between tokens
 *   merge     - if true, merge empty tokens
 *
 * Text which is enclosed by " " is regarded as a single token. 
 ****************************************************************************/
 
uint8_t tokenize(char* buf, char* tokens[], uint8_t maxtokens, char *delim, bool merge)
{ 
     register uint8_t ntokens = 0;
     while (ntokens<maxtokens)
     {
        if (*buf == '\"') {
            /* Special case: token is enclosed in " */
            tokens[ntokens++] = ++buf;
            char *endt = strchrnul(buf, '\"'); 
            *endt = '\0';
            buf = endt + 1;
        }
        else {    
            tokens[ntokens] = strsep(&buf, delim);
            if ( buf == NULL)
               break;
            if (!merge || *tokens[ntokens] != '\0') 
               ntokens++;
        }
     }
     return (merge && *tokens[ntokens] == '\0' ? ntokens : ntokens+1);
}
