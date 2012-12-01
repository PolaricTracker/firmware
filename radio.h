
#if defined TARGET_USBKEY

#define radio_require() 
#define radio_release()
#define radio_setup()

#else

/* Require access to radio transceiver */
void radio_require(void);

/* Release radio */
void radio_release(void);

/* Setup of radio (read parameters from EEPROM) */
void radio_setup(void);

#endif