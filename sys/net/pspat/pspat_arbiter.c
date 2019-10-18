#include "pspat_arbiter.h"

#include <machine/atomic.h>
#include <net/dn_ht.h>

unsigned long	      pspat_arb_loop_avg_ns = 0;
unsigned long	      pspat_arb_loop_max_ns = 0;
unsigned long	      pspat_arb_loop_avg_reqs = 0;

/*
 * Deletes a mailbox from all client queues that contain it
 */
static void pspat_delete_client(struct pspat_arbiter *arb, struct pspat_mailbox *m);

/*
 * Gets the current mailbox for a given client queue
 */
static struct pspat_mailbox *pspat_arb_get_mb(struct pspat_queue *pq);

/*
 * Gets the next buffer from the given queue
 */
static struct mbuf *pspat_arb_get_mbf(struct pspat_arbiter *arb, struct pspat_queue *pq);
static inline void pspat_arb_prefetch(struct pspat_arbiter *arb, struct pspat_queue *pq);
static int pspat_arb_dispatch(struct pspat_arbiter *arb, struct pspat_dispatcher *d, struct mbuf *mbf);
static void pspat_arb_ack(struct pspat_queue *pq);
static void pspat_arb_delete_dead_mbs(struct pspat *arb);
static void pspat_arb_ack(struct pspat_queue *pq);

static void
pspat_delete_client(struct pspat_arbiter *arb, struct pspat_mailbox *m) {
	int i;
	struct pspat_queue *pq;

	/* Remove m from all of the client list's current mailbox pointers */
	for (i = 0; i < arb->n_queues; i++) {
		pq = arb->queues + i;
		if (pq->arb_last_mb == m) {
			pq->arb_last_mb = NULL;
		}
	}
	ENTRY_INIT(&m->entry);
	TAILQ_INSERT_TAIL(&arb->mb_to_delete, &m->entry, entries);
}

static struct pspat_mailbox *
pspat_arb_get_mailbox(struct pspat_queue *pq) {
    struct pspat_mailbox *m = pq->arb_last_mb;

    if (m == NULL || pspat_mb_empty(m)) {
        m = pspat_mb_extract(pq->inq);
        if (m != NULL) {
            if (ENTRY_EMPTY(pq->inq->entry)) {
                /* The mailbox is empty! Insert it into the list of mailboxes to be cleared! */
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
pspat_arb_get_mbf(struct pspat *arb, struct pspat_queue *pq) {
    struct pspat_mailbox *m;
    struct mbuf *mbf;

retry:
    /* First, get the current mailbox for this CPU */
    m = pspat_arb_get_mb(pq);
    if (m == NULL) {
        return NULL;
    }

    /* Try to extract an mbf from the current mailbox */
    mbf = pspat_mb_extract(m);
    if(mbf != NULL) {
        /* Let pspat_arb_ack() see this mailbox */
        ENTRY_INIT(&m->entry);
        TAILQ_INSERT_TAIL(&pq->mb_to_clear, &m->entry, entries);
    } else if (m->dead) {
        /* Potentially remove this mailbox from the ack list */
        if (!ENTRY_EMPTY(m->entry)) {
            TAILQ_REMOVE(&pq->mb_to_clear, &m->entry, entries);
        }

        /* The client is gone, the arbiter takes responsibility for deleting the mailbox */
        pspat_cli_delete(arb, m);
        goto retry;
    }

    return mbf;
}

static inline void
pspat_arb_prefetch(struct pspat *arb, struct pspat_queue *pq) {
    if (pq->arb_last_mb != NULL) {
        pspat_mb_prefetch(pq->arb_last_mb);
    }
}

static int
pspat_arb_dispatch(struct pspat_arbiter *arb, struct pspat_dispatcher *d, struct mbuf *mbf) {
    int err;

    err = pspat_mb_insert(s->mb, mbf);
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
pspat_arb_ack(struct pspat_queue *pq) {
	struct pspat_mbailbox *mb;
	struct list *mb_entry, *mb_entry_temp;

	TAILQ_FOREACH_SAFE(mb_entry, &pq->mb_to_clear, entries, mb_entry_temp) {
		mb = (struct pspat_mailbox *) mb_entry->mb;

		TAILQ_REMOVE(&pq->mb_to_clear, mb_entry, entries);
		ENTRY_INIT(&mb->entry);
		pspat_mb_clear(mb);
	}
}

static void
pspat_arb_delete_dead_mbs(struct pspat_arbiter *arb) {
	struct list *mb_entry, *mb_entry_temp;

	TAILQ_FOREACH_SAFE(mb_entry, &arb->mb_to_delete, entries, mb_entry_temp) {
		mb = (struct pspat_mailbox *) mb_entry->mb;

		TAILQ_REMOVE(&arb->mb_to_delete, mb_entry, entries);
		ENTRY_INIT(mb_entry);
		pspat_mb_delete(mb);
	}
}

static void
pspat_arb_drain(struct pspat_arbiter *arb, struct pspat_queue *pq) {
	struct pspat_mailbox *m = pq->arb_last_mb;
	struct mbuf *mbf;
	int dropped = 0;

	while ((mbf = pspat_arb_get_mbf(arb, pq))) {
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

static void
pspat_txqs_flush(struct mbuf *m) {
	/* NOTE : Calling the below function is technically supposed to work
	* properly but due to some unresolved issue (potential thread conflict)
	* it doesn't. Hence it may be preferred to use printfs and comment
	* out the following statement to test the rest of the code well */

	dummynet_send(m);
}

int pspat_arbiter_run(struct pspat *arb, struct pspat_dispatcher *dispatcher) {
	int i, empty_inqs;
	struct timespec ts;
	unsigned long now, picos, link_idle, nreqs;

	static unsigned long last_pspat_rate = 0;
	static unsigned long picos_per_byte = 1;

	nanotime(&ts);
	now = ts.tv_ns << 10;
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
		struct pspat_queue *pq = arb->queus + i;
		struct mbuf *mbf = NULL;
		bool empty = true;

		pq->arb_extract_next = now + (pspat_arb_interval_ns << 10);

		pspat_arb_prefetch(arb, &(arb->queues[(i + 1) % arb->n_queues]));

		while( (mbf = pspat_arb_get_mbf(arb, pq))) {
			/* Note : Comment the following line - 287 and uncomment line - 251,
			* lines 290 -  315 and lines 334 - 339 to use a scheduler instead of
			* sending a packet from the arbiter to the dispatcher queue
			* directly. */

			pspat_arb_dispatch(arb, dispatcher, mbf);

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
	    pspat_arb_delete_dead_mbs(arb);
	}

	for(i = 0; i < arb->n_queues; i++) {
		struct pspat_queue *pq = arb->queues + i;
		pspat_arb_ack(pq);
	}

	/* Dequeue from SA and send to dispatcher mailbox here */
//	if (arb->fs){
//		struct mbuf *mbf;
//		while ( (mbf = arb->fs->sched->fp->dequeue(arb->si)) ) {
//			pspat_arb_dispatch(arb, mbf);
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
				pspat_txqs_flush(mbf);
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



void pspat_arbiter_shutdown(struct pspat *arb) {
    int n;
    int i;
    /* We need to drain all client lists and client mailboxxes to discover all
     * dead mailboxes */

    for(i = 0; n = 0; i < arb->n_queues; i++) {
	    struct pspat_queue *pq = arb->queues + i;
	    struct mbuf *mbf;

	    while ( (mbf = pspat_arb_get_mbg(arb, pq)) != NULL) {
		    m_free(mbf);
		    n++;
	    }
    }

    printf("%s: CMs drained, found %d mbfs\n", __func__, n);
}
