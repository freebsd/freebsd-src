/*
** No copyright?!
**
** $Id: timer.c,v 1.3 1996/09/22 15:42:59 miff Exp $
*/
#include "doscmd.h"

static void
int08(regcontext_t *REGS)
{
    softint(0x1c);
}

static void
int1c(regcontext_t *REGS)
{
}

unsigned char timer;

static u_char
inb_timer(int port)
{
    return (--timer);
}

void
timer_init(void)
{
    u_long 		vec;
    struct itimerval	itv;

    vec = insert_hardint_trampoline();
    ivec[0x08] = vec;
    register_callback(vec, int08, "int 08");
    
    vec = insert_softint_trampoline();
    ivec[0x1c] = vec;
    register_callback(vec, int1c, "int 1c");

    define_input_port_handler(0x42, inb_timer);
    define_input_port_handler(0x40, inb_timer);
    
    itv.it_interval.tv_sec = 0;
    itv.it_interval.tv_usec = 54925; /* 1193182/65536 times per second */
    itv.it_value.tv_sec = 0;
    itv.it_value.tv_usec = 54925;	 /* 1193182/65536 times per second */
    if (! timer_disable)
        setitimer(ITIMER_REAL, &itv, 0);
}
