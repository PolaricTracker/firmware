#include "usb.h"
#include "kernel/kernel.h"
#include "kernel/stream.h"
#include "defines.h"
#include "ui.h"
#include <avr/sleep.h>

#define CDC_BUF_SIZE 64

Semaphore cdc_run;    
Stream cdc_instr; 
Stream cdc_outstr;
BCond usb_active;


/** LUFA CDC Class driver interface configuration and state information. This structure is
 *  passed to all CDC Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */

USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface =
{
    .Config = 
    {
         .ControlInterfaceNumber         = 0,

         .DataINEndpointNumber           = CDC_TX_EPNUM,
         .DataINEndpointSize             = CDC_TXRX_EPSIZE,
         .DataINEndpointDoubleBank       = false,

         .DataOUTEndpointNumber          = CDC_RX_EPNUM,
         .DataOUTEndpointSize            = CDC_TXRX_EPSIZE,
         .DataOUTEndpointDoubleBank      = false,

         .NotificationEndpointNumber     = CDC_NOTIFICATION_EPNUM,
         .NotificationEndpointSize       = CDC_NOTIFICATION_EPSIZE,
	 .NotificationEndpointDoubleBank = false,
    },
};



/* Main thread */
void usb_thread (void)
{
    for (;;)
    {
         bcond_wait(&usb_active);
         t_yield();
         int16_t c; 
         while ((c = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface)) > 0)
            stream_put(&cdc_instr, c);
 
         t_yield();
         while ( ! stream_empty(&cdc_outstr))
            CDC_Device_SendByte(&VirtualSerial_CDC_Interface, stream_get(&cdc_outstr)); 
                       
         CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
	 USB_USBTask();	
    }
}


/* Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void)
{
    set_sleep_mode(SLEEP_MODE_IDLE); 
    bcond_set(&usb_active);
}


/* Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
    led_usb_off();
    bcond_clear(&usb_active);
}


/* Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	if ((CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface)))
	{
             led_usb_on(); 
             sem_up(&cdc_run);    
	}   
}


/* Event handler for the library USB Unhandled Control Request event. */
void  EVENT_USB_Device_ControlRequest(void)
{
    CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}


    
bool usb_con()
   { return USB_VBUS_GetStatus(); }   
    
     

void usb_init()
{ 
   /* Initialize USB Subsystem */
   /* See makefile for mode constraints */
   USB_Init();
   sem_init(&cdc_run, 0);
   bcond_init(&usb_active, usb_con());

   STREAM_INIT( cdc_instr, CDC_BUF_SIZE);
   STREAM_INIT( cdc_outstr, CDC_BUF_SIZE);
   cdc_outstr.kick = NULL;
   
   THREAD_START(usb_thread, STACK_USB);
}

      