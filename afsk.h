
#define	MARK  1  				   // 167 - works from 190 to 155 (1200 Hz.)
#define	SPACE 0 						// 213 - works from 204 to 216 (2200 Hz.)
#define  FLAG  (0x7E)

void afsk_setTxTone(unsigned char x);
void init_afsk_TX();
void init_afsk_RX();
void afsk_enable_RX();
void afsk_disable_RX();
void afsk_startTx();
void afsk_txBitClock();
void afsk_rxBitClock();


extern uint8_t transmit; 
extern uint8_t dcd;

extern FBQ outbuf, fbin; 
