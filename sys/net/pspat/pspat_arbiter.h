#ifndef __PSPAT_ARBITER_H__
#define __PSPAT_ARBITER_H__

#include "mailbox.h"
#include "pspat_dispatcher.h"

#include <sys/types.h>
#include <sys/kthread.h>

/* Per-CPU queue */
struct pspat_queue {
	/* Input queue (a mailbox of mailboxes). Written by clients, read by
	 * arbitrer */
	struct pspat_mailbox  *inq;

	/* Client fields */
	unsigned long	      cli_last_mb __aligned(CACHE_LINE_SIZE);

	/* Arbitrer fields */
	unsigned long	      arb_next __aligned(CACHE_LINE_SIZE);
	struct pspat_mailbox *arb_last_mb;

	/* Mailboxes that need to be cleared */
	struct entry_list     mb_to_clear;
};

struct pspat_arbiter {
	/* Queue of dead mailboxes to be deleted at the first safe opportunity */
	struct entry_list   mb_to_delete;

	/* Dummynet scheduling fields */
	struct dn_fsk	    *fs;
	struct dn_sch_inst  *si;
	struct dn_queueu    *q;

	/* Statistics used to evaluate the cost of the arbiter */
	unsigned int	    num_loops;		/* Number of times the arbiter loop has run */
	unsigned int	    num_reqs;		/* Number of requests the arbiter has evaluated */
	unsigned long	    max_picos;		/* Maximum time to evaluate a request in ps */
	unsigned long	    num_picos;		/* Number of picoseconds the arbiter has run in total */
	unsigned long	    last_ts;		/* Last timespec information we received */

	/* Mailboxes between clients and the arbiter */
	unsigned int	    n_queues; /* Number of queues */
	struct pspat_queue  queues[0]; /* VLA containing the queues b/w clients and arbiter */
};


/*
 * GLOBAL VARIABLE DEFINITIONS
 */

/* Global struct containing all of the information about the data structure */
extern struct pspat_system *pspat_info;

/* Read-write lock for `pspat_info` */
extern struct rwlock pspat_rwlock;

/*
 * Data collection information
 */
extern unsigned long	      pspat_arb_loop_avg_ns;	    /* The average time it takes to run an iteration of the arbiter loop */
extern unsigned long	      pspat_arb_loop_max_ns;	    /* Maximum time it ever took to run an iteration of the arbiter loop */
extern unsigned long	      pspat_arb_loop_avg_reqs;	    /* Average number of requests we process per iteration */

/*
 * Runs the arbiter loop
 * @returns 0
 */
int pspat_arbiter_run(struct pspat_arbiter *arb, struct pspat_dispatcher *dispatcher);

/*
 * Shuts down the given arbiter
 */
void pspat_arbiter_shutdown(struct pspat_arbiter *arb);

#endif /* !__PSPAT_ARBITER_H__ */
