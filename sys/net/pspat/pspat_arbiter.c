#define PSPAT

#include "pspat_arbiter.h"
#include "mailbox.h"
#include "pspat_opts.h"

#include <machine/atomic.h>
#include <sys/mbuf.h>
#include <netpfil/ipfw/ip_dn_io.h>

#define	NSEC_PER_SEC	1000000000L
#define	PSPAT_ARB_STATS_LOOPS	0x1000

/*
 * Delete any mailboxes that appear in the to_delete list
 *
 * @arb the arbiter to delete from
 */
static void delete_dead_mbs(struct pspat_arbiter *arb);

/*
 * Deletes a mailbox from all client queues that contain it
 * @arb: The arbiter to delete from
 * @m: The mailbox to delete
 */
static void delete_mb(struct pspat_arbiter *arb, struct pspat_mailbox *m);

/*
 * Gets the current mailbox for a given client queue
 *
 * @pq: The Client queue to get the mailbox from
 */
static struct pspat_mailbox *get_client_mb(struct pspat_queue *pq);

/*
 * Gets the next buffer from the given queue. Once a mailbox is empty, it is
 * added to the list of mailboxes to clear. If the mailbox is dead, it is
 * deleted by the arbiter.
 *
 * @arb the arbiter that we are getting the mbf from (for deleting mailboxes)
 * @pq the client queue we are getting the mbf from
 */
static struct mbuf *get_client_mbuf(struct pspat_arbiter *arb, struct pspat_queue *pq);

/*
 * calls the mailbox prefetch on the last mailbox in the queue
 */
static inline void prefetch_mb(struct pspat_queue *pq);

/*
 * Acknowledges to the client all read mailboxes by clearing any mailboxes on
 * the clear list
 *
 * @pq the queue to acknowledge for
 */
static void send_ack(struct pspat_queue *pq);

/*
 * Attempt to send the given mbf to the given dispatcher. If insertion fails,
 * then the backpressure flag is set for the last client on the queue where the
 * mbf was sent
 *
 * @arb The arbiter we are dispatching from
 * @d the dispatcher we are sending to
 * @mbf the buffer we are dispatching
 */
static int send_to_dispatcher(struct pspat_arbiter *arb, struct pspat_dispatcher *d, struct mbuf *mbf);

/*
 * Dispatch a mbuf ourselves
 *
 * @m the mbuf to dispatch
 */
static void dispatch(struct mbuf *m);

/*
 * Drains the given client queue to relieve backpressure
 * @arb the arbiter that this queue is a part of
 * @pq the client queue
 */
static void drain_client_queue(struct pspat_arbiter *arb, struct pspat_queue *pq);


/*
 * IMPLEMENTATIONS
 */


static void
delete_dead_mbs(struct pspat_arbiter *arb) {
	struct pspat_mailbox *mb;
	struct list *mb_entry, *mb_entry_temp;

	TAILQ_FOREACH_SAFE(mb_entry, &arb->mb_to_delete, entries, mb_entry_temp) {
		mb = (struct pspat_mailbox *) mb_entry->mb;

		TAILQ_REMOVE(&arb->mb_to_delete, mb_entry, entries);
		entry_init(mb_entry);
		pspat_mb_delete(mb);
	}
}

static void
delete_mb(struct pspat_arbiter *arb, struct pspat_mailbox *m) {
	int i;
	struct pspat_queue *pq;

	/* Remove m from all of the client list's current mailbox pointers */
	for (i = 0; i < arb->n_queues; i++) {
		pq = arb->queues + i;
		if (pq->arb_last_mb == m) {
			pq->arb_last_mb = NULL;
		}
	}
	entry_init(&m->entry);
	TAILQ_INSERT_TAIL(&arb->mb_to_delete, &m->entry, entries);
}

static struct pspat_mailbox *
get_client_mb(struct pspat_queue *pq) {
    struct pspat_mailbox *m = pq->arb_last_mb;

    if (m == NULL || pspat_mb_empty(m)) {
	    /* Either we haven't gotten a mailbox or we've exhausted the
	     * mailbox. Get a new one */
	    m = pspat_mb_extract(pq->inq);

	    if (m != NULL) {
		    //TODO, does this work??
		    if (ENTRY_EMPTY(pq->inq->entry)) {
			    /* The mailbox is empty, and is not in the list of
			     * mailboxes to be cleared -
			     * Insert it into the list of mailboxes to be cleared! */
			    TAILQ_INSERT_TAIL(&pq->mb_to_clear, &pq->inq->entry, entries);
		    }

		    pq->arb_last_mb = m;
		    /* Wait for previous updates in the new mailbox. TODO What? */
		    mb();
	    }

    }
    return m;
}

static struct mbuf *
get_client_mbuf(struct pspat_arbiter *arb, struct pspat_queue *pq) {
	struct pspat_mailbox *m;
	struct mbuf *mbf;

retry:
	/* First, get the current mailbox for this CPU */
	m = get_client_mb(pq);
	if (m == NULL) {
		return NULL;
	}

	/* Try to extract an mbf from the current mailbox */
	mbf = pspat_mb_extract(m);
	if(mbf != NULL) {
		/* Let send_ack() see this mailbox */
		entry_init(&m->entry);
		TAILQ_INSERT_TAIL(&pq->mb_to_clear, &m->entry, entries);
	} else if (m->dead) {
		/* Potentially remove this mailbox from the ack list */
		if (!ENTRY_EMPTY(m->entry)) {
			TAILQ_REMOVE(&pq->mb_to_clear, &m->entry, entries);
		}

		/* The client is gone, the arbiter takes responsibility for deleting the mailbox */
		delete_mb(arb, m);
		goto retry;
	}

	return mbf;
}

static inline void
prefetch_mb(struct pspat_queue *pq) {
	if (pq->arb_last_mb != NULL) {
		pspat_mb_prefetch(pq->arb_last_mb);
	}
}

static void
send_ack(struct pspat_queue *pq) {
	struct pspat_mailbox *mb;
	struct list *mb_entry, *mb_entry_temp;

	TAILQ_FOREACH_SAFE(mb_entry, &pq->mb_to_clear, entries, mb_entry_temp) {
		mb = (struct pspat_mailbox *) mb_entry->mb;

		TAILQ_REMOVE(&pq->mb_to_clear, mb_entry, entries);
		entry_init(&mb->entry);
		pspat_mb_clear(mb);
	}
}

static int
send_to_dispatcher(struct pspat_arbiter *arb, struct pspat_dispatcher *d, struct mbuf *mbf) {
	int err;

	err = pspat_mb_insert(d->mb, mbf);
	if (err) {
		/* Drop this mbf and possible set the backpressure flag for the last client on the queue
		 * where this mbf was transmitted */
		struct pspat_mailbox *cli_mb;
		struct pspat_queue *pq;

		pq = arb->queues + mbf->sender_cpu - 1;
		cli_mb = pq->arb_last_mb;

		if (cli_mb != NULL && cli_mb->backpressure) {
			cli_mb->backpressure = 1;
		}
		pspat_arb_dispatch_drop ++;
		m_free(mbf);
	}

	return err;
}

static void
dispatch(struct mbuf *m) {
	/* NOTE : Calling the below function is technically supposed to work
	 * properly but due to some unresolved issue (potential thread conflict)
	 * it doesn't. Hence it may be preferred to use printfs and comment
	 * out the following statement to test the rest of the code well */

	dummynet_send(m);
}

static void
drain_client_queue(struct pspat_arbiter *arb, struct pspat_queue *pq) {
	struct pspat_mailbox *m = pq->arb_last_mb;
	struct mbuf *mbf;
	int dropped = 0;

	while ((mbf = get_client_mbuf(arb, pq))) {
		m_free(mbf);
		dropped ++;
	}

	if (!m->backpressure) {
		m->backpressure = 1;
	}

	if (pspat_debug_xmit) {
		printf("PSPAT Drained mailbox %s [dropped %d mbfs]\n", m->name, dropped);
	}

	pspat_arb_backpressure_drop += dropped;
}


int pspat_arbiter_run(struct pspat_arbiter *arb, struct pspat_dispatcher *dispatcher) {
	int i, empty_inqs;
	struct timespec ts;
	unsigned long now, picos, link_idle, nreqs;

	static unsigned long last_pspat_rate = 0;
	static unsigned long picos_per_byte = 1;

	nanotime(&ts);
	now = ts.tv_nsec << 10;
	link_idle = 0;
	nreqs = 0;

	/* Number of empty client lists found in the last round. If we have a
	 * round with only empty CLs we can safely delete */
	empty_inqs = 0;

	if (pspat_rate != last_pspat_rate) {
		last_pspat_rate = pspat_rate;
		picos_per_byte = (8 * (NSEC_PER_SEC << 10)) / last_pspat_rate;
	}

	/* Bring in pending packets arrived between link_idle and now */
	for (i = 0; i < arb->n_queues; i++) {
		struct pspat_queue *pq = arb->queues + i;
		struct mbuf *mbf = NULL;
		bool empty = true;

		pq->arb_next = now + (pspat_arb_interval_ns << 10);

		prefetch_mb(&(arb->queues[(i + 1) % arb->n_queues]));

		while( (mbf = get_client_mbuf(arb, pq))) {
			/* Note : Comment the following line - 287 and uncomment line - 251,
			 * lines 290 -  315 and lines 334 - 339 to use a scheduler instead of
			 * sending a packet from the arbiter to the dispatcher queue
			 * directly. */

			send_to_dispatcher(arb, dispatcher, mbf);

			/* Enqueue to SA here */
			//			if (first_packet) {
			/*
			 * NOW : Using first packet's data to determine the scheduler
			 * instance to be used.
			 *
			 * TODO : Change scheduler instance to be used everytime a
			 * new scheduler instance is used with dummynet.
			 */

			//				struct ip_fw_args *fwa = mbf->fwa;

			//				int fs_id = (fwa->rule.info & IPFW_INFO_MASK) +
			//				((fwa->rule.info & IPFW_IS_PIPE) ? 2*DN_MAX_ID : 0);
			//				arb->fs = dn_ht_find(dn_cfg.fshash, fs_id, 0, NULL);
			//				arb->si = ipdn_si_find(arb->fs->sched, &(fwa->f_id));

			//				if (arb->fs->sched->fp->flags & DN_MULTIQUEUE)
			//					arb->q = ipdn_q_find(arb->fs, arb->si, &(fwa->f_id));

			//				arb->fs->sched->fp->enqueue(arb->si, arb->q, mbf);

			//				first_packet = 0;

			//			} else {
			//				arb->fs->sched->fp->enqueue(arb->si, arb->q, mbf);
			//			}

			empty = false;
			++nreqs;
		}
		if (empty) {
			++empty_inqs;
		}
	}

	if (empty_inqs == arb->n_queues) {
		delete_dead_mbs(arb);
	}

	for(i = 0; i < arb->n_queues; i++) {
		struct pspat_queue *pq = arb->queues + i;
		send_ack(pq);
	}

	/* Dequeue from SA and send to dispatcher mailbox here */
//	if (arb->fs){
//		struct mbuf *mbf;
//		while ( (mbf = arb->fs->sched->fp->dequeue(arb->si)) ) {
//			send_to_dispatcher(arb, mbf);
//		}
//	}

	if(pspat_xmit_mode == PSPAT_XMIT_MODE_ARB) {
		/* Arbiter flushes the packets */
		unsigned int ndeq = 0;

		struct pspat_mailbox *m = dispatcher->mb;
		struct mbuf *mbf;

		while (link_idle < now && ndeq < pspat_arb_batch) {
			if ((mbf = pspat_mb_extract(m)) != NULL) {
				link_idle += picos_per_byte * mbf->m_len;
				dispatch(mbf);
				ndeq ++;
			} else {
				link_idle = now;
			}
		}

		pspat_mb_clear(m);
	}

	/* Update statistics ! */
	picos = now - arb->last_ts;
	arb->last_ts = now;
	arb->num_picos += picos;
	arb->num_reqs += nreqs;
	arb->num_loops++;
	if (picos > arb->max_picos) {
		arb->max_picos = picos;
	}
	if (arb->num_loops & PSPAT_ARB_STATS_LOOPS) {
		pspat_arb_loop_avg_ns = (arb->num_picos / PSPAT_ARB_STATS_LOOPS) >> 10;
		pspat_arb_loop_max_ns = arb->max_picos >> 10;
		pspat_arb_loop_avg_reqs = arb->num_reqs / PSPAT_ARB_STATS_LOOPS;
		arb->num_loops = 0;
		arb->num_picos = 0;
		arb->max_picos = 0;
		arb->num_reqs = 0;
	}

	/*
	* NOTE : It has been been noticed that sometimes packets can disappear
	* from the PSPAT client queues if the Arbiter doesn't pick them up before
	*  the client threads return. If this happens, then commenting out the
	* following statement line - 388 can be helpful.
	*/
	pause("Thread Pause", 100);
	return 0;
}



void pspat_arbiter_shutdown(struct pspat_arbiter *arb) {
    int n;
    int i;
    /* We need to drain all client lists and client mailboxxes to discover all
     * dead mailboxes */

    for(i = 0, n = 0; i < arb->n_queues; i++) {
	    struct pspat_queue *pq = arb->queues + i;
	    struct mbuf *mbf;

	    while ( (mbf = get_client_mbuf(arb, pq)) != NULL) {
		    m_free(mbf);
		    n++;
	    }
    }

    printf("%s: CMs drained, found %d mbfs\n", __func__, n);
}
