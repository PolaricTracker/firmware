#ifndef __CRC_CCITT_H__
#define __CRC_CCITT_H__

#include <stdint.h>
#include <stdbool.h>

#define CRC_CCITT_OK 0xf0b8

extern uint16_t const crc_ccitt_table[256];

uint16_t crc_ccitt (uint16_t crc, const uint8_t *buffer, uint16_t len);

static inline uint16_t crc_ccitt_byte (uint16_t crc, const uint8_t c)
{
  return (crc >> 8) ^ crc_ccitt_table[(crc ^ c) & 0xff];
}

static inline bool check_crc_ccitt (const uint8_t *buf, uint16_t cnt)
{
  return (crc_ccitt (0xffff, buf, cnt) & 0xffff) == CRC_CCITT_OK;
}

#endif /* __CRC_CCITT_H__ */
