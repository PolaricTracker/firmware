
#include "config.h"
#include <avr/io.h>
#include <avr/signal.h>



void adf_write(uint8_t addr, uint32_t data, uint8_t data_len)
{
    uint8_t i;
    data <<= 4;
    data |=  (addr & 0x0F);
    data <<= (32-data_len);
    
    for (i=0; i<data_len; i++)
    {
        if (data & 0x80000000)
           set_bit( ADF_SDATA ); 
        else 
           clear_bit( ADF_SDATA );
           
        set_bit( ADF_SCLK );
        data <<= 1;       
        clear_bit( ADF_SCLK );
    }
}


