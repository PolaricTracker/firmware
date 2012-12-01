
#include "kernel/kernel.h"
#include "kernel/stream.h"
#include "defines.h"
#include "config.h"
#include "ax25.h"
#include "hdlc.h"

BCond mon_ok;
   /* External variable in afsk_rx and afsk_tx. 
    * May pass it as return value from mon_init instead? */
   
static bool mon_on = false;
static stream_t *out;
static FBQ mon;
static void mon_thread(void);

FBQ* mon_q = &mon;

void mon_init(stream_t* outstr)
{
    out = outstr;
    bcond_init(&mon_ok, true);
    FBQ_INIT(mon, HDLC_DECODER_QUEUE_SIZE);
}


void mon_activate(bool m)
{ 
   bool tstart = m && !mon_on;
   bool tstop = !m && mon_on;
   
   mon_on = m;
   FBQ* mq = (mon_on? &mon : NULL);
   hdlc_subscribe_rx(mq, 0);
   if (!mon_on || GET_BYTE_PARAM(TXMON_ON))
      hdlc_monitor_tx(mq);
   if (tstart) 
      THREAD_START(mon_thread, STACK_MONITOR);  
   if (tstop) {
      hdlc_monitor_tx(NULL);
      hdlc_subscribe_rx(NULL, 0);
      fbq_signal(&mon);
   }
}


void mon_showtext(char* txt)
{
   if (mon_on)
      putstr(out, txt);
}


static void mon_thread()
{
    while (mon_on)
    {
        /* Wait for frame and then to AFSK decoder/encoder 
         * is not running. 
         */
        FBUF frame = fbq_get(&mon);
        if (fbuf_empty(&frame))
            continue;
//        bcond_wait(&mon_ok);
        
        /* Display it */
        ax25_display_frame(out, &frame);
        putstr_P(out, PSTR("\r\n"));
        
        /* And dispose it */
        fbuf_release(&frame);
    }
}



