/*
 * Copyright (C) 2013-2014 Universita` di Pisa. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This module implements netmap support on top of standard,
 * unmodified device drivers.
 *
 * A NIOCREGIF request is handled here if the device does not
 * have native support. TX and RX rings are emulated as follows:
 *
 * NIOCREGIF
 *	We preallocate a block of TX mbufs (roughly as many as
 *	tx descriptors; the number is not critical) to speed up
 *	operation during transmissions. The refcount on most of
 *	these buffers is artificially bumped up so we can recycle
 *	them more easily. Also, the destructor is intercepted
 *	so we use it as an interrupt notification to wake up
 *	processes blocked on a poll().
 *
 *	For each receive ring we allocate one "struct mbq"
 *	(an mbuf tailq plus a spinlock). We intercept packets
 *	(through if_input)
 *	on the receive path and put them in the mbq from which
 *	netmap receive routines can grab them.
 *
 * TX:
 *	in the generic_txsync() routine, netmap buffers are copied
 *	(or linked, in a future) to the preallocated mbufs
 *	and pushed to the transmit queue. Some of these mbufs
 *	(those with NS_REPORT, or otherwise every half ring)
 *	have the refcount=1, others have refcount=2.
 *	When the destructor is invoked, we take that as
 *	a notification that all mbufs up to that one in
 *	the specific ring have been completed, and generate
 *	the equivalent of a transmit interrupt.
 *
 * RX:
 *
 */

#ifdef __FreeBSD__

#include <sys/cdefs.h> /* prerequisite */
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/lock.h>   /* PROT_EXEC */
#include <sys/rwlock.h>
#include <sys/socket.h> /* sockaddrs */
#include <sys/selinfo.h>
#include <net/if.h>
#include <net/if_var.h>
#include <machine/bus.h>        /* bus_dmamap_* in netmap_kern.h */

// XXX temporary - D() defined here
#include <net/netmap.h>
#include <dev/netmap/netmap_kern.h>
#include <dev/netmap/netmap_mem2.h>

#define rtnl_lock() D("rtnl_lock called");
#define rtnl_unlock() D("rtnl_unlock called");
#define MBUF_TXQ(m)	((m)->m_pkthdr.flowid)
#define MBUF_RXQ(m)	((m)->m_pkthdr.flowid)
#define smp_mb()

/*
 * mbuf wrappers
 */

/*
 * we allocate an EXT_PACKET
 */
#define netmap_get_mbuf(len) m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR|M_NOFREE)

/* mbuf destructor, also need to change the type to EXT_EXTREF,
 * add an M_NOFREE flag, and then clear the flag and
 * chain into uma_zfree(zone_pack, mf)
 * (or reinstall the buffer ?)
 */
#define SET_MBUF_DESTRUCTOR(m, fn)	do {		\
	(m)->m_ext.ext_free = (void *)fn;	\
	(m)->m_ext.ext_type = EXT_EXTREF;	\
} while (0)


#define GET_MBUF_REFCNT(m)	((m)->m_ext.ref_cnt ? *(m)->m_ext.ref_cnt : -1)



#else /* linux */

#include "bsd_glue.h"

#include <linux/rtnetlink.h>    /* rtnl_[un]lock() */
#include <linux/ethtool.h>      /* struct ethtool_ops, get_ringparam */
#include <linux/hrtimer.h>

//#define RATE  /* Enables communication statistics. */

//#define REG_RESET

#endif /* linux */


/* Common headers. */
#include <net/netmap.h>
#include <dev/netmap/netmap_kern.h>
#include <dev/netmap/netmap_mem2.h>



/* ======================== usage stats =========================== */

#ifdef RATE
#define IFRATE(x) x
struct rate_stats {
	unsigned long txpkt;
	unsigned long txsync;
	unsigned long txirq;
	unsigned long rxpkt;
	unsigned long rxirq;
	unsigned long rxsync;
};

struct rate_context {
	unsigned refcount;
	struct timer_list timer;
	struct rate_stats new;
	struct rate_stats old;
};

#define RATE_PRINTK(_NAME_) \
	printk( #_NAME_ " = %lu Hz\n", (cur._NAME_ - ctx->old._NAME_)/RATE_PERIOD);
#define RATE_PERIOD  2
static void rate_callback(unsigned long arg)
{
	struct rate_context * ctx = (struct rate_context *)arg;
	struct rate_stats cur = ctx->new;
	int r;

	RATE_PRINTK(txpkt);
	RATE_PRINTK(txsync);
	RATE_PRINTK(txirq);
	RATE_PRINTK(rxpkt);
	RATE_PRINTK(rxsync);
	RATE_PRINTK(rxirq);
	printk("\n");

	ctx->old = cur;
	r = mod_timer(&ctx->timer, jiffies +
			msecs_to_jiffies(RATE_PERIOD * 1000));
	if (unlikely(r))
		D("[v1000] Error: mod_timer()");
}

static struct rate_context rate_ctx;

#else /* !RATE */
#define IFRATE(x)
#endif /* !RATE */


/* =============== GENERIC NETMAP ADAPTER SUPPORT ================= */
#define GENERIC_BUF_SIZE        netmap_buf_size    /* Size of the mbufs in the Tx pool. */

/*
 * Wrapper used by the generic adapter layer to notify
 * the poller threads. Differently from netmap_rx_irq(), we check
 * only IFCAP_NETMAP instead of NAF_NATIVE_ON to enable the irq.
 */
static void
netmap_generic_irq(struct ifnet *ifp, u_int q, u_int *work_done)
{
	if (unlikely(!(ifp->if_capenable & IFCAP_NETMAP)))
		return;

	netmap_common_irq(ifp, q, work_done);
}


/* Enable/disable netmap mode for a generic network interface. */
static int
generic_netmap_register(struct netmap_adapter *na, int enable)
{
	struct ifnet *ifp = na->ifp;
	struct netmap_generic_adapter *gna = (struct netmap_generic_adapter *)na;
	struct mbuf *m;
	int error;
	int i, r;

	if (!na)
		return EINVAL;

#ifdef REG_RESET
	error = ifp->netdev_ops->ndo_stop(ifp);
	if (error) {
		return error;
	}
#endif /* REG_RESET */

	if (enable) { /* Enable netmap mode. */
		/* Init the mitigation support. */
		gna->mit = malloc(na->num_rx_rings * sizeof(struct nm_generic_mit),
					M_DEVBUF, M_NOWAIT | M_ZERO);
		if (!gna->mit) {
			D("mitigation allocation failed");
			error = ENOMEM;
			goto out;
		}
		for (r=0; r<na->num_rx_rings; r++)
			netmap_mitigation_init(&gna->mit[r], na);

		/* Initialize the rx queue, as generic_rx_handler() can
		 * be called as soon as netmap_catch_rx() returns.
		 */
		for (r=0; r<na->num_rx_rings; r++) {
			mbq_safe_init(&na->rx_rings[r].rx_queue);
		}

		/*
		 * Preallocate packet buffers for the tx rings.
		 */
		for (r=0; r<na->num_tx_rings; r++)
			na->tx_rings[r].tx_pool = NULL;
		for (r=0; r<na->num_tx_rings; r++) {
			na->tx_rings[r].tx_pool = malloc(na->num_tx_desc * sizeof(struct mbuf *),
					M_DEVBUF, M_NOWAIT | M_ZERO);
			if (!na->tx_rings[r].tx_pool) {
				D("tx_pool allocation failed");
				error = ENOMEM;
				goto free_tx_pools;
			}
			for (i=0; i<na->num_tx_desc; i++)
				na->tx_rings[r].tx_pool[i] = NULL;
			for (i=0; i<na->num_tx_desc; i++) {
				m = netmap_get_mbuf(GENERIC_BUF_SIZE);
				if (!m) {
					D("tx_pool[%d] allocation failed", i);
					error = ENOMEM;
					goto free_tx_pools;
				}
				na->tx_rings[r].tx_pool[i] = m;
			}
		}
		rtnl_lock();
		/* Prepare to intercept incoming traffic. */
		error = netmap_catch_rx(na, 1);
		if (error) {
			D("netdev_rx_handler_register() failed (%d)", error);
			goto register_handler;
		}
		ifp->if_capenable |= IFCAP_NETMAP;

		/* Make netmap control the packet steering. */
		netmap_catch_tx(gna, 1);

		rtnl_unlock();

#ifdef RATE
		if (rate_ctx.refcount == 0) {
			D("setup_timer()");
			memset(&rate_ctx, 0, sizeof(rate_ctx));
			setup_timer(&rate_ctx.timer, &rate_callback, (unsigned long)&rate_ctx);
			if (mod_timer(&rate_ctx.timer, jiffies + msecs_to_jiffies(1500))) {
				D("Error: mod_timer()");
			}
		}
		rate_ctx.refcount++;
#endif /* RATE */

	} else if (na->tx_rings[0].tx_pool) {
		/* Disable netmap mode. We enter here only if the previous
		   generic_netmap_register(na, 1) was successfull.
		   If it was not, na->tx_rings[0].tx_pool was set to NULL by the
		   error handling code below. */
		rtnl_lock();

		ifp->if_capenable &= ~IFCAP_NETMAP;

		/* Release packet steering control. */
		netmap_catch_tx(gna, 0);

		/* Do not intercept packets on the rx path. */
		netmap_catch_rx(na, 0);

		rtnl_unlock();

		/* Free the mbufs going to the netmap rings */
		for (r=0; r<na->num_rx_rings; r++) {
			mbq_safe_purge(&na->rx_rings[r].rx_queue);
			mbq_safe_destroy(&na->rx_rings[r].rx_queue);
		}

		for (r=0; r<na->num_rx_rings; r++)
			netmap_mitigation_cleanup(&gna->mit[r]);
		free(gna->mit, M_DEVBUF);

		for (r=0; r<na->num_tx_rings; r++) {
			for (i=0; i<na->num_tx_desc; i++) {
				m_freem(na->tx_rings[r].tx_pool[i]);
			}
			free(na->tx_rings[r].tx_pool, M_DEVBUF);
		}

#ifdef RATE
		if (--rate_ctx.refcount == 0) {
			D("del_timer()");
			del_timer(&rate_ctx.timer);
		}
#endif
	}

#ifdef REG_RESET
	error = ifp->netdev_ops->ndo_open(ifp);
	if (error) {
		goto free_tx_pools;
	}
#endif

	return 0;

register_handler:
	rtnl_unlock();
free_tx_pools:
	for (r=0; r<na->num_tx_rings; r++) {
		if (na->tx_rings[r].tx_pool == NULL)
			continue;
		for (i=0; i<na->num_tx_desc; i++)
			if (na->tx_rings[r].tx_pool[i])
				m_freem(na->tx_rings[r].tx_pool[i]);
		free(na->tx_rings[r].tx_pool, M_DEVBUF);
		na->tx_rings[r].tx_pool = NULL;
	}
	for (r=0; r<na->num_rx_rings; r++) {
		netmap_mitigation_cleanup(&gna->mit[r]);
		mbq_safe_destroy(&na->rx_rings[r].rx_queue);
	}
	free(gna->mit, M_DEVBUF);
out:

	return error;
}

/*
 * Callback invoked when the device driver frees an mbuf used
 * by netmap to transmit a packet. This usually happens when
 * the NIC notifies the driver that transmission is completed.
 */
static void
generic_mbuf_destructor(struct mbuf *m)
{
	if (netmap_verbose)
		D("Tx irq (%p) queue %d", m, MBUF_TXQ(m));
	netmap_generic_irq(MBUF_IFP(m), MBUF_TXQ(m), NULL);
#ifdef __FreeBSD__
	m->m_ext.ext_type = EXT_PACKET;
	m->m_ext.ext_free = NULL;
	if (*(m->m_ext.ref_cnt) == 0)
		*(m->m_ext.ref_cnt) = 1;
	uma_zfree(zone_pack, m);
#endif /* __FreeBSD__ */
	IFRATE(rate_ctx.new.txirq++);
}

/* Record completed transmissions and update hwtail.
 *
 * The oldest tx buffer not yet completed is at nr_hwtail + 1,
 * nr_hwcur is the first unsent buffer.
 */
static u_int
generic_netmap_tx_clean(struct netmap_kring *kring)
{
	u_int const lim = kring->nkr_num_slots - 1;
	u_int nm_i = nm_next(kring->nr_hwtail, lim);
	u_int hwcur = kring->nr_hwcur;
	u_int n = 0;
	struct mbuf **tx_pool = kring->tx_pool;

	while (nm_i != hwcur) { /* buffers not completed */
		struct mbuf *m = tx_pool[nm_i];

		if (unlikely(m == NULL)) {
			/* this is done, try to replenish the entry */
			tx_pool[nm_i] = m = netmap_get_mbuf(GENERIC_BUF_SIZE);
			if (unlikely(m == NULL)) {
				D("mbuf allocation failed, XXX error");
				// XXX how do we proceed ? break ?
				return -ENOMEM;
			}
		} else if (GET_MBUF_REFCNT(m) != 1) {
			break; /* This mbuf is still busy: its refcnt is 2. */
		}
		n++;
		nm_i = nm_next(nm_i, lim);
	}
	kring->nr_hwtail = nm_prev(nm_i, lim);
	ND("tx completed [%d] -> hwtail %d", n, kring->nr_hwtail);

	return n;
}


/*
 * We have pending packets in the driver between nr_hwtail +1 and hwcur.
 * Compute a position in the middle, to be used to generate
 * a notification.
 */
static inline u_int
generic_tx_event_middle(struct netmap_kring *kring, u_int hwcur)
{
	u_int n = kring->nkr_num_slots;
	u_int ntc = nm_next(kring->nr_hwtail, n-1);
	u_int e;

	if (hwcur >= ntc) {
		e = (hwcur + ntc) / 2;
	} else { /* wrap around */
		e = (hwcur + n + ntc) / 2;
		if (e >= n) {
			e -= n;
		}
	}

	if (unlikely(e >= n)) {
		D("This cannot happen");
		e = 0;
	}

	return e;
}

/*
 * We have pending packets in the driver between nr_hwtail+1 and hwcur.
 * Schedule a notification approximately in the middle of the two.
 * There is a race but this is only called within txsync which does
 * a double check.
 */
static void
generic_set_tx_event(struct netmap_kring *kring, u_int hwcur)
{
	struct mbuf *m;
	u_int e;

	if (nm_next(kring->nr_hwtail, kring->nkr_num_slots -1) == hwcur) {
		return; /* all buffers are free */
	}
	e = generic_tx_event_middle(kring, hwcur);

	m = kring->tx_pool[e];
	if (m == NULL) {
		/* This can happen if there is already an event on the netmap
		   slot 'e': There is nothing to do. */
		return;
	}
	ND("Event at %d mbuf %p refcnt %d", e, m, GET_MBUF_REFCNT(m));
	kring->tx_pool[e] = NULL;
	SET_MBUF_DESTRUCTOR(m, generic_mbuf_destructor);

	// XXX wmb() ?
	/* Decrement the refcount an free it if we have the last one. */
	m_freem(m);
	smp_mb();
}


/*
 * generic_netmap_txsync() transforms netmap buffers into mbufs
 * and passes them to the standard device driver
 * (ndo_start_xmit() or ifp->if_transmit() ).
 * On linux this is not done directly, but using dev_queue_xmit(),
 * since it implements the TX flow control (and takes some locks).
 */
static int
generic_netmap_txsync(struct netmap_adapter *na, u_int ring_nr, int flags)
{
	struct ifnet *ifp = na->ifp;
	struct netmap_kring *kring = &na->tx_rings[ring_nr];
	struct netmap_ring *ring = kring->ring;
	u_int nm_i;	/* index into the netmap ring */ // j
	u_int const lim = kring->nkr_num_slots - 1;
	u_int const head = kring->rhead;

	IFRATE(rate_ctx.new.txsync++);

	// TODO: handle the case of mbuf allocation failure

	rmb();

	/*
	 * First part: process new packets to send.
	 */
	nm_i = kring->nr_hwcur;
	if (nm_i != head) {	/* we have new packets to send */
		while (nm_i != head) {
			struct netmap_slot *slot = &ring->slot[nm_i];
			u_int len = slot->len;
			void *addr = NMB(slot);

			/* device-specific */
			struct mbuf *m;
			int tx_ret;

			NM_CHECK_ADDR_LEN(addr, len);

			/* Tale a mbuf from the tx pool and copy in the user packet. */
			m = kring->tx_pool[nm_i];
			if (unlikely(!m)) {
				RD(5, "This should never happen");
				kring->tx_pool[nm_i] = m = netmap_get_mbuf(GENERIC_BUF_SIZE);
				if (unlikely(m == NULL)) {
					D("mbuf allocation failed");
					break;
				}
			}
			/* XXX we should ask notifications when NS_REPORT is set,
			 * or roughly every half frame. We can optimize this
			 * by lazily requesting notifications only when a
			 * transmission fails. Probably the best way is to
			 * break on failures and set notifications when
			 * ring->cur == ring->tail || nm_i != cur
			 */
			tx_ret = generic_xmit_frame(ifp, m, addr, len, ring_nr);
			if (unlikely(tx_ret)) {
				RD(5, "start_xmit failed: err %d [nm_i %u, head %u, hwtail %u]",
						tx_ret, nm_i, head, kring->nr_hwtail);
				/*
				 * No room for this mbuf in the device driver.
				 * Request a notification FOR A PREVIOUS MBUF,
				 * then call generic_netmap_tx_clean(kring) to do the
				 * double check and see if we can free more buffers.
				 * If there is space continue, else break;
				 * NOTE: the double check is necessary if the problem
				 * occurs in the txsync call after selrecord().
				 * Also, we need some way to tell the caller that not
				 * all buffers were queued onto the device (this was
				 * not a problem with native netmap driver where space
				 * is preallocated). The bridge has a similar problem
				 * and we solve it there by dropping the excess packets.
				 */
				generic_set_tx_event(kring, nm_i);
				if (generic_netmap_tx_clean(kring)) { /* space now available */
					continue;
				} else {
					break;
				}
			}
			slot->flags &= ~(NS_REPORT | NS_BUF_CHANGED);
			nm_i = nm_next(nm_i, lim);
			IFRATE(rate_ctx.new.txpkt ++);
		}

		/* Update hwcur to the next slot to transmit. */
		kring->nr_hwcur = nm_i; /* not head, we could break early */
	}

	/*
	 * Second, reclaim completed buffers
	 */
	if (flags & NAF_FORCE_RECLAIM || nm_kr_txempty(kring)) {
		/* No more available slots? Set a notification event
		 * on a netmap slot that will be cleaned in the future.
		 * No doublecheck is performed, since txsync() will be
		 * called twice by netmap_poll().
		 */
		generic_set_tx_event(kring, nm_i);
	}
	ND("tx #%d, hwtail = %d", n, kring->nr_hwtail);

	generic_netmap_tx_clean(kring);

	nm_txsync_finalize(kring);

	return 0;
}


/*
 * This handler is registered (through netmap_catch_rx())
 * within the attached network interface
 * in the RX subsystem, so that every mbuf passed up by
 * the driver can be stolen to the network stack.
 * Stolen packets are put in a queue where the
 * generic_netmap_rxsync() callback can extract them.
 */
void
generic_rx_handler(struct ifnet *ifp, struct mbuf *m)
{
	struct netmap_adapter *na = NA(ifp);
	struct netmap_generic_adapter *gna = (struct netmap_generic_adapter *)na;
	u_int work_done;
	u_int rr = MBUF_RXQ(m); // receive ring number

	if (rr >= na->num_rx_rings) {
		rr = rr % na->num_rx_rings; // XXX expensive...
	}

	/* limit the size of the queue */
	if (unlikely(mbq_len(&na->rx_rings[rr].rx_queue) > 1024)) {
		m_freem(m);
	} else {
		mbq_safe_enqueue(&na->rx_rings[rr].rx_queue, m);
	}

	if (netmap_generic_mit < 32768) {
		/* no rx mitigation, pass notification up */
		netmap_generic_irq(na->ifp, rr, &work_done);
		IFRATE(rate_ctx.new.rxirq++);
	} else {
		/* same as send combining, filter notification if there is a
		 * pending timer, otherwise pass it up and start a timer.
		 */
		if (likely(netmap_mitigation_active(&gna->mit[rr]))) {
			/* Record that there is some pending work. */
			gna->mit[rr].mit_pending = 1;
		} else {
			netmap_generic_irq(na->ifp, rr, &work_done);
			IFRATE(rate_ctx.new.rxirq++);
			netmap_mitigation_start(&gna->mit[rr]);
		}
	}
}

/*
 * generic_netmap_rxsync() extracts mbufs from the queue filled by
 * generic_netmap_rx_handler() and puts their content in the netmap
 * receive ring.
 * Access must be protected because the rx handler is asynchronous,
 */
static int
generic_netmap_rxsync(struct netmap_adapter *na, u_int ring_nr, int flags)
{
	struct netmap_kring *kring = &na->rx_rings[ring_nr];
	struct netmap_ring *ring = kring->ring;
	u_int nm_i;	/* index into the netmap ring */ //j,
	u_int n;
	u_int const lim = kring->nkr_num_slots - 1;
	u_int const head = nm_rxsync_prologue(kring);
	int force_update = (flags & NAF_FORCE_READ) || kring->nr_kflags & NKR_PENDINTR;

	if (head > lim)
		return netmap_ring_reinit(kring);

	/*
	 * First part: import newly received packets.
	 */
	if (netmap_no_pendintr || force_update) {
		/* extract buffers from the rx queue, stop at most one
		 * slot before nr_hwcur (stop_i)
		 */
		uint16_t slot_flags = kring->nkr_slot_flags;
		u_int stop_i = nm_prev(kring->nr_hwcur, lim);

		nm_i = kring->nr_hwtail; /* first empty slot in the receive ring */
		for (n = 0; nm_i != stop_i; n++) {
			int len;
			void *addr = NMB(&ring->slot[nm_i]);
			struct mbuf *m;

			/* we only check the address here on generic rx rings */
			if (addr == netmap_buffer_base) { /* Bad buffer */
				return netmap_ring_reinit(kring);
			}
			/*
			 * Call the locked version of the function.
			 * XXX Ideally we could grab a batch of mbufs at once
			 * and save some locking overhead.
			 */
			m = mbq_safe_dequeue(&kring->rx_queue);
			if (!m)	/* no more data */
				break;
			len = MBUF_LEN(m);
			m_copydata(m, 0, len, addr);
			ring->slot[nm_i].len = len;
			ring->slot[nm_i].flags = slot_flags;
			m_freem(m);
			nm_i = nm_next(nm_i, lim);
		}
		if (n) {
			kring->nr_hwtail = nm_i;
			IFRATE(rate_ctx.new.rxpkt += n);
		}
		kring->nr_kflags &= ~NKR_PENDINTR;
	}

	// XXX should we invert the order ?
	/*
	 * Second part: skip past packets that userspace has released.
	 */
	nm_i = kring->nr_hwcur;
	if (nm_i != head) {
		/* Userspace has released some packets. */
		for (n = 0; nm_i != head; n++) {
			struct netmap_slot *slot = &ring->slot[nm_i];

			slot->flags &= ~NS_BUF_CHANGED;
			nm_i = nm_next(nm_i, lim);
		}
		kring->nr_hwcur = head;
	}
	/* tell userspace that there might be new packets. */
	nm_rxsync_finalize(kring);
	IFRATE(rate_ctx.new.rxsync++);

	return 0;
}

static void
generic_netmap_dtor(struct netmap_adapter *na)
{
	struct ifnet *ifp = na->ifp;
	struct netmap_generic_adapter *gna = (struct netmap_generic_adapter*)na;
	struct netmap_adapter *prev_na = gna->prev;

	if (prev_na != NULL) {
		D("Released generic NA %p", gna);
		if_rele(na->ifp);
		netmap_adapter_put(prev_na);
	}
	if (ifp != NULL) {
		WNA(ifp) = prev_na;
		D("Restored native NA %p", prev_na);
		na->ifp = NULL;
	}
}

/*
 * generic_netmap_attach() makes it possible to use netmap on
 * a device without native netmap support.
 * This is less performant than native support but potentially
 * faster than raw sockets or similar schemes.
 *
 * In this "emulated" mode, netmap rings do not necessarily
 * have the same size as those in the NIC. We use a default
 * value and possibly override it if the OS has ways to fetch the
 * actual configuration.
 */
int
generic_netmap_attach(struct ifnet *ifp)
{
	struct netmap_adapter *na;
	struct netmap_generic_adapter *gna;
	int retval;
	u_int num_tx_desc, num_rx_desc;

	num_tx_desc = num_rx_desc = netmap_generic_ringsize; /* starting point */

	generic_find_num_desc(ifp, &num_tx_desc, &num_rx_desc);
	ND("Netmap ring size: TX = %d, RX = %d", num_tx_desc, num_rx_desc);

	gna = malloc(sizeof(*gna), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (gna == NULL) {
		D("no memory on attach, give up");
		return ENOMEM;
	}
	na = (struct netmap_adapter *)gna;
	na->ifp = ifp;
	na->num_tx_desc = num_tx_desc;
	na->num_rx_desc = num_rx_desc;
	na->nm_register = &generic_netmap_register;
	na->nm_txsync = &generic_netmap_txsync;
	na->nm_rxsync = &generic_netmap_rxsync;
	na->nm_dtor = &generic_netmap_dtor;
	/* when using generic, IFCAP_NETMAP is set so we force
	 * NAF_SKIP_INTR to use the regular interrupt handler
	 */
	na->na_flags = NAF_SKIP_INTR | NAF_HOST_RINGS;

	ND("[GNA] num_tx_queues(%d), real_num_tx_queues(%d), len(%lu)",
			ifp->num_tx_queues, ifp->real_num_tx_queues,
			ifp->tx_queue_len);
	ND("[GNA] num_rx_queues(%d), real_num_rx_queues(%d)",
			ifp->num_rx_queues, ifp->real_num_rx_queues);

	generic_find_num_queues(ifp, &na->num_tx_rings, &na->num_rx_rings);

	retval = netmap_attach_common(na);
	if (retval) {
		free(gna, M_DEVBUF);
	}

	return retval;
}

struct netmap_adapter *
netmap_getna(if_t ifp)
{
	return (NA((struct ifnet *)ifp));	
}
