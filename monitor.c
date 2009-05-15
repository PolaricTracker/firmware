#include "kernel/kernel.h"
#include "kernel/stream.h"
#include "defines.h"
#include "config.h"
#include "ax25.h"


BCond mon_ok;
   /* External variable in afsk_decoder. 
    * May pass it as return value from mon_init instead? */
   
static bool mon_on = false;
static stream_t *out;
static FBQ mon;
static void mon_thread(void);



void mon_init(stream_t* outstr)
{
    out = outstr;
    bcond_init(&mon_ok, true);
    FBQ_INIT(mon, HDLC_DECODER_QUEUE_SIZE);
    THREAD_START(mon_thread, STACK_MONITOR);
}


void mon_activate(bool m)
{ 
   mon_on = m;
   FBQ* mq = (mon_on? &mon : NULL);
   hdlc_monitor_rx(mq);
   if (!mon_on || GET_BYTE_PARAM(TXMON_ON))
      hdlc_monitor_tx(mq);
}



static void mon_thread()
{
    while (true)
    {
        /* Wait for frame and then to AFSK decoder/encoder 
         * is not running 
         */
        FBUF frame = fbq_get(&mon);
        bcond_wait(&mon_ok);
        
        /* Display it */
        ax25_display_frame(out, &frame);
        putstr_P(out, PSTR("\r\n"));
        
        /* And dispose it */
        fbuf_release(&frame);
    }
}



