/* $Id: */


/* Require access to radio transceiver */
void radio_require(void);

/* Release radio */
void radio_release(void);

/* Setup of radio (read parameters from EEPROM) */
void radio_setup(void);
