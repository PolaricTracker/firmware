#if !defined __UART_H__
#define __UART_H__


Stream* uart_tx_init(uint16_t baud);
Stream* uart_rx_init(uint16_t baud, bool e);
void uart_rx_pause();
void uart_rx_resume();

#endif
