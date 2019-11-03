#define PSPAT

#include "pspat.h"

#include <machine/atomic.h>

#include <sys/time.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/kthread.h>
#include <sys/types.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/rwlock.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_dummynet.h>
#include <netinet/if_ether.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>

#include <netpfil/ipfw/ip_fw_private.h>
#include <netpfil/ipfw/dn_heap.h>
#include <netpfil/ipfw/ip_dn_private.h>
#ifdef NEW_AQM
#include <netpfil/ipfw/dn_aqm.h>
#endif
#include <netpfil/ipfw/dn_sched.h>

static unsigned int mb_next_id = 0;

/*
 * Enqueues a mbuf into the given client queue
 * returns -ENOBUFS if the queue is full
 */
static int
pspat_enq_mbuf(struct pspat_queue *pq, struct mbuf *mbf) {
    struct pspat_mailbox *m;
    int err;

    if(curthread->pspat_mb == NULL) {
	    err = pspat_create_client_queue();
	    if (err) {
		    return err;
	    }

	    curthread->pspat_mb->identifier = atomic_fetchadd_int(&mb_next_id, 1);
    }

    m = curthread->pspat_mb;

    if (m->backpressure) {
	    m->backpressure = 0;
	    if (pspat_debug_xmit) {
		    printf("Mailbox %s backpressure\n", m->name);
	    }
	    return -ENOBUFS;
    }

    err = pspat_mb_insert(m, mbf);
    if (err) {
	    return err;
    }

    /* avoid duplicate notification */
    if (pq->cli_last_mb != m->identifier) {
	    mb(); /* let the arbiter see the insert above (TODO what?) */

	    err = pspat_mb_insert(pq->inq, m);
	    pq->cli_last_mb = m->identifier;
    }

    return 0;
}

int
pspat_client_handler(struct mbuf *mbf, struct ip_fw_args *fwa) {
	static struct mbuf *ins_mbf;
	/* Avoid duplicate intake of the same packet */
	if (mbf == ins_mbf) {
		return -ENOTTY;
	} else {
		ins_mbf = mbf;
	}

	int cpu, rc;
	struct pspat_queue *pq;
	struct pspat_arbiter *arb;

    rc = 0;

	rw_rlock(&pspat_rwlock);
	arb = &pspat->arbiter;
	rw_unlock(&pspat_rwlock);

	if (!pspat_enable || arb == NULL) {
		/* Not our business */
		return -ENOTTY;
	}

	cpu = curthread->td_oncpu;
	mbf->sender_cpu = cpu;
	mbf->fwa = fwa;
	mbf->ifp = fwa->ifp;

	pq = arb->queues + cpu;
	if (pspat_enq_mbuf(pq, mbf)) {
		pspat_stats[cpu].inq_drop++;
		rc = 1;
	}

	if (pspat_debug_xmit) {
		printf("cli_push(%p) -> %d\n", mbf, rc);
	}
	return rc;
}

int
pspat_create_client_queue() {
	struct pspat_mailbox *m;
	char name[PSPAT_MB_NAMSZ];
	int err;

	if (curthread->pspat_mb != NULL) {
		return 0;
	}

	snprintf(name, PSPAT_MB_NAMSZ, "CM-%d", curthread->td_tid);

	err = pspat_mb_new(name, pspat_mailbox_entries, pspat_mailbox_line_size, &m);

	if (err) {
		return err;
	} if (m == NULL) {
		return -ENOMEM;
	}

	curthread->pspat_mb = m;

	return 0;
}

