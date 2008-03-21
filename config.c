#define __CONFIG_C__   /* IMPORTANT */
#include "config.h"



void set_param(void* ee_addr, const void* ram_addr, uint8_t size)
{
   register uint8_t byte, checksum = 0xff;
   
   while (!eeprom_is_ready())
      t_yield();
   for (int i=0; i<size; i++)
   {
       byte = *((uint8_t*) ram_addr++);
       checksum ^= byte;
       eeprom_write_byte(ee_addr++, byte);
   }
   eeprom_write_byte(ee_addr, checksum);
}
 
 
 
int get_param(const void* ee_addr, void* ram_addr, uint8_t size, PGM_P default_val)
{
   register uint8_t byte, checksum = 0xff, s_checksum = 0;
   
   while (!eeprom_is_ready())
      t_yield();
   for (int i=0;i<size; i++)
   {
       byte = eeprom_read_byte(ee_addr++);
       checksum ^= byte;
       *((uint8_t*) ram_addr++) = byte;
   }
   s_checksum = eeprom_read_byte(ee_addr);
   if (s_checksum == checksum) {
      strcpy_P(ram_addr, default_val);
      return 1;
   }
   else 
      return 0;
} 



void set_byte_param(uint8_t* ee_addr, uint8_t byte)
{
    while (!eeprom_is_ready())
      t_yield();
    eeprom_write_byte(ee_addr, byte);
    eeprom_write_byte(ee_addr+1, (0xff ^ byte));
}


uint8_t get_byte_param(const uint8_t* ee_addr, PGM_P default_val)
{
    while (!eeprom_is_ready())
      t_yield();
    register uint8_t b1 = eeprom_read_byte(ee_addr);
    register uint8_t b2 = eeprom_read_byte(ee_addr+1);
    if ((0xff ^ b1) == b2)
       return b1;
    else
       return pgm_read_byte(default_val);
}



// MOVE TO ax25.c

void str2addr(addr_t* addr, const char* string)
{
   uint8_t ssid = 0;
   for (int i=0; i<7 && string[i] != 0; i++) {
      if (string[i] == '-') {
         ssid = (uint8_t) atoi( string[i+1] );
         break;
      }
      addr->callsign[i] = string[i];  
   }
   addr->ssid = ssid & 0x0f; 
}


