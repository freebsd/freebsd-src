#ifndef __PSPAT_SYSTEM_H__
#define __PSPAT_SYSTEM_H__

#include "mailbox.h"
#include "pspat_arbiter.h"
#include "pspat_dispatcher.h"

#include <sys/types.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/rwlock.h>

/* PSPAT overall data structure */
struct pspat_system {
	struct thread	    *arb_thread;	/* The thread that the arbiter is running on */
	struct thread	    *dispatcher_thread;	/* The thread that is actually dispatching */

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
extern struct pspat_system *pspat;

/* Read-write lock for `pspat` */
extern struct rwlock pspat_rwlock;

/*
 * Data collection information
 */
extern struct pspat_stats     *pspat_stats;		    /* Statistics about the PSPAT subsystem */

#endif /* !__PSPAT_SYSTEM_H__ */
