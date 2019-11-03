#ifndef __PSPAT_SYSCTL_H__
#define __PSPAT_SYSCTL_H__

#include <sys/types.h>
#include <sys/sysctl.h>

/*
 * Settings for the PSPAT subsystem
 */
extern bool		      		pspat_enable;		    		/* Is PSPAT even enabled? */
extern bool		      		pspat_debug_xmit;		    	/* Print debug messages about PSPAT transmissions? */
#define PSPAT_XMIT_MODE_ARB		0 /* packets sent by the arbiter */
#define PSPAT_XMIT_MODE_DISPATCH	1 /* packets sent by dispatcher */
#define PSPAT_XMIT_MODE_MAX		2 /* packets dropped by the arbiter */
extern int					pspat_xmit_mode;		    	/* The transmission mode for the pspat subsystem */
extern unsigned long	    pspat_rate;		    			/* The rate at which TODO what? */
extern unsigned long		pspat_arb_interval_ns;	    	/* Interval at whcih the arbiter will run */
extern unsigned long	    pspat_arb_backpressure_drop;  	/* TODO what is the purpose of this */
extern unsigned long	    pspat_arb_dispatch_drop;	    /* TODO what is the purpose of this */
extern unsigned long	    pspat_dispatch_deq;	    		/* TODO what is the purpose of this */
extern unsigned long 		pspat_arb_loop_avg_ns;
extern unsigned long		pspat_arb_loop_max_ns;
extern unsigned long		pspat_arb_loop_avg_reqs;
extern unsigned long		pspat_mailbox_entries;
extern unsigned long		pspat_mailbox_line_size;
extern unsigned long	    *pspat_rounds;		    		/* TODO what is the purpose of this and why is it a pointer */
extern uint32_t		      	pspat_arb_batch;		    	/* How many packets to batch before sending to the arbiter */
extern uint32_t		      	pspat_dispatch_batch;	    	/* How many packets to batch together before dispatching */

int pspat_sysctl_init(void);
void pspat_sysctl_fini(void);

#endif /* !__PSPAT_SYSCTL_H__ */
