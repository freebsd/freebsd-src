#ifndef __PSPAT_H__
#define __PSPAT_H__

#include "mailbox.h"
#include "pspat_arbiter.h"
#include "pspat_dispatcher.h"

#include <sys/types.h>
#include <sys/kthread.h>
#include <sys/rwlock.h>

/* PSPAT overall data structure */
struct pspat_system {
	struct thread	    *arb_thread;	/* The thread that the arbiter is running on */
	struct thread	    *dispatch_thread;	/* The thread that is actually dispatching */

	/* Used with PSPAT_XMIT_MODE_DISPATCH */
	struct pspat_dispatcher dispatchers[1];

  struct pspat_arbiter arbiter;
};


struct pspat_stats {
	unsigned long	  inq_drop; /* Number of dropped things from the in-queue */
} __attribute__((aligned(32)));

/*
 * GLOBAL VARIABLE DEFINITIONS
 */

/* Global struct containing all of the information about the data structure */
extern struct pspat_system *pspat_info;

/* Read-write lock for `pspat_info` */
extern struct rwlocak pspat_rwlock;

/*
 * Data collection information
 */
extern struct pspat_stats     *pspat_stats;		    /* Statistics about the PSPAT subsystem */

#endif /* !__PSPAT_SYSTEM_H__ */
