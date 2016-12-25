/*
 * Copyright (C) 2015 Stefano Garzarella
 * Copyright (C) 2016 Vincenzo Maffione
 * All rights reserved.
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
 *
 * $FreeBSD$
 */

/*
 * common headers
 */
#if defined(__FreeBSD__)
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/selinfo.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_var.h>
#include <machine/bus.h>

//#define usleep_range(_1, _2)
#define usleep_range(_1, _2) \
	pause_sbt("ptnetmap-sleep", SBT_1US * _1, SBT_1US * 1, C_ABSOLUTE)

#elif defined(linux)
#include <bsd_glue.h>
#endif

#include <net/netmap.h>
#include <dev/netmap/netmap_kern.h>
#include <net/netmap_virt.h>
#include <dev/netmap/netmap_mem2.h>

#ifdef WITH_PTNETMAP_HOST

/* RX cycle without receive any packets */
#define PTN_RX_DRY_CYCLES_MAX	10

/* Limit Batch TX to half ring.
 * Currently disabled, since it does not manage NS_MOREFRAG, which
 * results in random drops in the VALE txsync. */
//#define PTN_TX_BATCH_LIM(_n)	((_n >> 1))

//#define BUSY_WAIT

#define NETMAP_PT_DEBUG  /* Enables communication debugging. */
#ifdef NETMAP_PT_DEBUG
#define DBG(x) x
#else
#define DBG(x)
#endif


#undef RATE
//#define RATE  /* Enables communication statistics. */
#ifdef RATE
#define IFRATE(x) x
struct rate_batch_stats {
    unsigned long sync;
    unsigned long sync_dry;
    unsigned long pkt;
};

struct rate_stats {
    unsigned long gtxk;     /* Guest --> Host Tx kicks. */
    unsigned long grxk;     /* Guest --> Host Rx kicks. */
    unsigned long htxk;     /* Host --> Guest Tx kicks. */
    unsigned long hrxk;     /* Host --> Guest Rx Kicks. */
    unsigned long btxwu;    /* Backend Tx wake-up. */
    unsigned long brxwu;    /* Backend Rx wake-up. */
    struct rate_batch_stats txbs;
    struct rate_batch_stats rxbs;
};

struct rate_context {
    struct timer_list timer;
    struct rate_stats new;
    struct rate_stats old;
};

#define RATE_PERIOD  2
static void
rate_callback(unsigned long arg)
{
    struct rate_context * ctx = (struct rate_context *)arg;
    struct rate_stats cur = ctx->new;
    struct rate_batch_stats *txbs = &cur.txbs;
    struct rate_batch_stats *rxbs = &cur.rxbs;
    struct rate_batch_stats *txbs_old = &ctx->old.txbs;
    struct rate_batch_stats *rxbs_old = &ctx->old.rxbs;
    uint64_t tx_batch, rx_batch;
    unsigned long txpkts, rxpkts;
    unsigned long gtxk, grxk;
    int r;

    txpkts = txbs->pkt - txbs_old->pkt;
    rxpkts = rxbs->pkt - rxbs_old->pkt;

    tx_batch = ((txbs->sync - txbs_old->sync) > 0) ?
	       txpkts / (txbs->sync - txbs_old->sync): 0;
    rx_batch = ((rxbs->sync - rxbs_old->sync) > 0) ?
	       rxpkts / (rxbs->sync - rxbs_old->sync): 0;

    /* Fix-up gtxk and grxk estimates. */
    gtxk = (cur.gtxk - ctx->old.gtxk) - (cur.btxwu - ctx->old.btxwu);
    grxk = (cur.grxk - ctx->old.grxk) - (cur.brxwu - ctx->old.brxwu);

    printk("txpkts  = %lu Hz\n", txpkts/RATE_PERIOD);
    printk("gtxk    = %lu Hz\n", gtxk/RATE_PERIOD);
    printk("htxk    = %lu Hz\n", (cur.htxk - ctx->old.htxk)/RATE_PERIOD);
    printk("btxw    = %lu Hz\n", (cur.btxwu - ctx->old.btxwu)/RATE_PERIOD);
    printk("rxpkts  = %lu Hz\n", rxpkts/RATE_PERIOD);
    printk("grxk    = %lu Hz\n", grxk/RATE_PERIOD);
    printk("hrxk    = %lu Hz\n", (cur.hrxk - ctx->old.hrxk)/RATE_PERIOD);
    printk("brxw    = %lu Hz\n", (cur.brxwu - ctx->old.brxwu)/RATE_PERIOD);
    printk("txbatch = %llu avg\n", tx_batch);
    printk("rxbatch = %llu avg\n", rx_batch);
    printk("\n");

    ctx->old = cur;
    r = mod_timer(&ctx->timer, jiffies +
            msecs_to_jiffies(RATE_PERIOD * 1000));
    if (unlikely(r))
        D("[ptnetmap] Error: mod_timer()\n");
}

static void
rate_batch_stats_update(struct rate_batch_stats *bf, uint32_t pre_tail,
		        uint32_t act_tail, uint32_t num_slots)
{
    int n = (int)act_tail - pre_tail;

    if (n) {
        if (n < 0)
            n += num_slots;

        bf->sync++;
        bf->pkt += n;
    } else {
        bf->sync_dry++;
    }
}

#else /* !RATE */
#define IFRATE(x)
#endif /* RATE */

struct ptnetmap_state {
    /* Kthreads. */
    struct nm_kthread **kthreads;

    /* Shared memory with the guest (TX/RX) */
    struct ptnet_ring __user *ptrings;

    bool stopped;

    /* Netmap adapter wrapping the backend. */
    struct netmap_pt_host_adapter *pth_na;

    IFRATE(struct rate_context rate_ctx;)
};

static inline void
ptnetmap_kring_dump(const char *title, const struct netmap_kring *kring)
{
    RD(1, "%s - name: %s hwcur: %d hwtail: %d rhead: %d rcur: %d \
    		    rtail: %d head: %d cur: %d tail: %d",
            title, kring->name, kring->nr_hwcur,
            kring->nr_hwtail, kring->rhead, kring->rcur, kring->rtail,
            kring->ring->head, kring->ring->cur, kring->ring->tail);
}

/*
 * TX functions to set/get and to handle host/guest kick.
 */


/* Enable or disable guest --> host kicks. */
static inline void
ptring_kick_enable(struct ptnet_ring __user *ptring, uint32_t val)
{
    CSB_WRITE(ptring, host_need_kick, val);
}

/* Are guest interrupt enabled or disabled? */
static inline uint32_t
ptring_intr_enabled(struct ptnet_ring __user *ptring)
{
    uint32_t v;

    CSB_READ(ptring, guest_need_kick, v);

    return v;
}

/* Enable or disable guest interrupts. */
static inline void
ptring_intr_enable(struct ptnet_ring __user *ptring, uint32_t val)
{
    CSB_WRITE(ptring, guest_need_kick, val);
}

/* Handle TX events: from the guest or from the backend */
static void
ptnetmap_tx_handler(void *data)
{
    struct netmap_kring *kring = data;
    struct netmap_pt_host_adapter *pth_na =
		(struct netmap_pt_host_adapter *)kring->na->na_private;
    struct ptnetmap_state *ptns = pth_na->ptns;
    struct ptnet_ring __user *ptring;
    struct netmap_ring shadow_ring; /* shadow copy of the netmap_ring */
    bool more_txspace = false;
    struct nm_kthread *kth;
    uint32_t num_slots;
    int batch;
    IFRATE(uint32_t pre_tail);

    if (unlikely(!ptns)) {
        D("ERROR ptnetmap state is NULL");
        return;
    }

    if (unlikely(ptns->stopped)) {
        RD(1, "backend netmap is being stopped");
        return;
    }

    if (unlikely(nm_kr_tryget(kring, 1, NULL))) {
        D("ERROR nm_kr_tryget()");
        return;
    }

    /* This is a guess, to be fixed in the rate callback. */
    IFRATE(ptns->rate_ctx.new.gtxk++);

    /* Get TX ptring pointer from the CSB. */
    ptring = ptns->ptrings + kring->ring_id;
    kth = ptns->kthreads[kring->ring_id];

    num_slots = kring->nkr_num_slots;
    shadow_ring.head = kring->rhead;
    shadow_ring.cur = kring->rcur;

    /* Disable guest --> host notifications. */
    ptring_kick_enable(ptring, 0);
    /* Copy the guest kring pointers from the CSB */
    ptnetmap_host_read_kring_csb(ptring, &shadow_ring, num_slots);

    for (;;) {
	/* If guest moves ahead too fast, let's cut the move so
	 * that we don't exceed our batch limit. */
        batch = shadow_ring.head - kring->nr_hwcur;
        if (batch < 0)
            batch += num_slots;

#ifdef PTN_TX_BATCH_LIM
        if (batch > PTN_TX_BATCH_LIM(num_slots)) {
            uint32_t head_lim = kring->nr_hwcur + PTN_TX_BATCH_LIM(num_slots);

            if (head_lim >= num_slots)
                head_lim -= num_slots;
            ND(1, "batch: %d head: %d head_lim: %d", batch, shadow_ring.head,
						     head_lim);
            shadow_ring.head = head_lim;
	    batch = PTN_TX_BATCH_LIM(num_slots);
        }
#endif /* PTN_TX_BATCH_LIM */

        if (nm_kr_txspace(kring) <= (num_slots >> 1)) {
            shadow_ring.flags |= NAF_FORCE_RECLAIM;
        }

        /* Netmap prologue */
	shadow_ring.tail = kring->rtail;
        if (unlikely(nm_txsync_prologue(kring, &shadow_ring) >= num_slots)) {
            /* Reinit ring and enable notifications. */
            netmap_ring_reinit(kring);
            ptring_kick_enable(ptring, 1);
            break;
        }

        if (unlikely(netmap_verbose & NM_VERB_TXSYNC)) {
            ptnetmap_kring_dump("pre txsync", kring);
	}

        IFRATE(pre_tail = kring->rtail);
        if (unlikely(kring->nm_sync(kring, shadow_ring.flags))) {
            /* Reenable notifications. */
            ptring_kick_enable(ptring, 1);
            D("ERROR txsync()");
	    break;
        }

        /*
         * Finalize
         * Copy host hwcur and hwtail into the CSB for the guest sync(), and
	 * do the nm_sync_finalize.
         */
        ptnetmap_host_write_kring_csb(ptring, kring->nr_hwcur,
				      kring->nr_hwtail);
        if (kring->rtail != kring->nr_hwtail) {
	    /* Some more room available in the parent adapter. */
	    kring->rtail = kring->nr_hwtail;
	    more_txspace = true;
        }

        IFRATE(rate_batch_stats_update(&ptns->rate_ctx.new.txbs, pre_tail,
				       kring->rtail, num_slots));

        if (unlikely(netmap_verbose & NM_VERB_TXSYNC)) {
            ptnetmap_kring_dump("post txsync", kring);
	}

#ifndef BUSY_WAIT
        /* Interrupt the guest if needed. */
        if (more_txspace && ptring_intr_enabled(ptring)) {
            /* Disable guest kick to avoid sending unnecessary kicks */
            ptring_intr_enable(ptring, 0);
            nm_os_kthread_send_irq(kth);
            IFRATE(ptns->rate_ctx.new.htxk++);
            more_txspace = false;
        }
#endif
        /* Read CSB to see if there is more work to do. */
        ptnetmap_host_read_kring_csb(ptring, &shadow_ring, num_slots);
#ifndef BUSY_WAIT
        if (shadow_ring.head == kring->rhead) {
            /*
             * No more packets to transmit. We enable notifications and
             * go to sleep, waiting for a kick from the guest when new
             * new slots are ready for transmission.
             */
            usleep_range(1,1);
            /* Reenable notifications. */
            ptring_kick_enable(ptring, 1);
            /* Doublecheck. */
            ptnetmap_host_read_kring_csb(ptring, &shadow_ring, num_slots);
            if (shadow_ring.head != kring->rhead) {
		/* We won the race condition, there are more packets to
		 * transmit. Disable notifications and do another cycle */
		ptring_kick_enable(ptring, 0);
		continue;
	    }
	    break;
        }

	if (nm_kr_txempty(kring)) {
	    /* No more available TX slots. We stop waiting for a notification
	     * from the backend (netmap_tx_irq). */
            ND(1, "TX ring");
            break;
        }
#endif
        if (unlikely(ptns->stopped)) {
            D("backend netmap is being stopped");
            break;
        }
    }

    nm_kr_put(kring);

    if (more_txspace && ptring_intr_enabled(ptring)) {
        ptring_intr_enable(ptring, 0);
        nm_os_kthread_send_irq(kth);
        IFRATE(ptns->rate_ctx.new.htxk++);
    }
}

/*
 * We need RX kicks from the guest when (tail == head-1), where we wait
 * for the guest to refill.
 */
#ifndef BUSY_WAIT
static inline int
ptnetmap_norxslots(struct netmap_kring *kring, uint32_t g_head)
{
    return (NM_ACCESS_ONCE(kring->nr_hwtail) == nm_prev(g_head,
    			    kring->nkr_num_slots - 1));
}
#endif /* !BUSY_WAIT */

/* Handle RX events: from the guest or from the backend */
static void
ptnetmap_rx_handler(void *data)
{
    struct netmap_kring *kring = data;
    struct netmap_pt_host_adapter *pth_na =
		(struct netmap_pt_host_adapter *)kring->na->na_private;
    struct ptnetmap_state *ptns = pth_na->ptns;
    struct ptnet_ring __user *ptring;
    struct netmap_ring shadow_ring; /* shadow copy of the netmap_ring */
    struct nm_kthread *kth;
    uint32_t num_slots;
    int dry_cycles = 0;
    bool some_recvd = false;
    IFRATE(uint32_t pre_tail);

    if (unlikely(!ptns || !ptns->pth_na)) {
        D("ERROR ptnetmap state %p, ptnetmap host adapter %p", ptns,
	  ptns ? ptns->pth_na : NULL);
        return;
    }

    if (unlikely(ptns->stopped)) {
        RD(1, "backend netmap is being stopped");
	return;
    }

    if (unlikely(nm_kr_tryget(kring, 1, NULL))) {
        D("ERROR nm_kr_tryget()");
	return;
    }

    /* This is a guess, to be fixed in the rate callback. */
    IFRATE(ptns->rate_ctx.new.grxk++);

    /* Get RX ptring pointer from the CSB. */
    ptring = ptns->ptrings + (pth_na->up.num_tx_rings + kring->ring_id);
    kth = ptns->kthreads[pth_na->up.num_tx_rings + kring->ring_id];

    num_slots = kring->nkr_num_slots;
    shadow_ring.head = kring->rhead;
    shadow_ring.cur = kring->rcur;

    /* Disable notifications. */
    ptring_kick_enable(ptring, 0);
    /* Copy the guest kring pointers from the CSB */
    ptnetmap_host_read_kring_csb(ptring, &shadow_ring, num_slots);

    for (;;) {
	uint32_t hwtail;

        /* Netmap prologue */
	shadow_ring.tail = kring->rtail;
        if (unlikely(nm_rxsync_prologue(kring, &shadow_ring) >= num_slots)) {
            /* Reinit ring and enable notifications. */
            netmap_ring_reinit(kring);
            ptring_kick_enable(ptring, 1);
            break;
        }

        if (unlikely(netmap_verbose & NM_VERB_RXSYNC)) {
            ptnetmap_kring_dump("pre rxsync", kring);
	}

        IFRATE(pre_tail = kring->rtail);
        if (unlikely(kring->nm_sync(kring, shadow_ring.flags))) {
            /* Reenable notifications. */
            ptring_kick_enable(ptring, 1);
            D("ERROR rxsync()");
	    break;
        }
        /*
         * Finalize
         * Copy host hwcur and hwtail into the CSB for the guest sync()
         */
	hwtail = NM_ACCESS_ONCE(kring->nr_hwtail);
        ptnetmap_host_write_kring_csb(ptring, kring->nr_hwcur, hwtail);
        if (kring->rtail != hwtail) {
	    kring->rtail = hwtail;
            some_recvd = true;
            dry_cycles = 0;
        } else {
            dry_cycles++;
        }

        IFRATE(rate_batch_stats_update(&ptns->rate_ctx.new.rxbs, pre_tail,
	                               kring->rtail, num_slots));

        if (unlikely(netmap_verbose & NM_VERB_RXSYNC)) {
            ptnetmap_kring_dump("post rxsync", kring);
	}

#ifndef BUSY_WAIT
	/* Interrupt the guest if needed. */
        if (some_recvd && ptring_intr_enabled(ptring)) {
            /* Disable guest kick to avoid sending unnecessary kicks */
            ptring_intr_enable(ptring, 0);
            nm_os_kthread_send_irq(kth);
            IFRATE(ptns->rate_ctx.new.hrxk++);
            some_recvd = false;
        }
#endif
        /* Read CSB to see if there is more work to do. */
        ptnetmap_host_read_kring_csb(ptring, &shadow_ring, num_slots);
#ifndef BUSY_WAIT
        if (ptnetmap_norxslots(kring, shadow_ring.head)) {
            /*
             * No more slots available for reception. We enable notification and
             * go to sleep, waiting for a kick from the guest when new receive
	     * slots are available.
             */
            usleep_range(1,1);
            /* Reenable notifications. */
            ptring_kick_enable(ptring, 1);
            /* Doublecheck. */
            ptnetmap_host_read_kring_csb(ptring, &shadow_ring, num_slots);
            if (!ptnetmap_norxslots(kring, shadow_ring.head)) {
		/* We won the race condition, more slots are available. Disable
		 * notifications and do another cycle. */
                ptring_kick_enable(ptring, 0);
                continue;
	    }
            break;
        }

	hwtail = NM_ACCESS_ONCE(kring->nr_hwtail);
        if (unlikely(hwtail == kring->rhead ||
		     dry_cycles >= PTN_RX_DRY_CYCLES_MAX)) {
	    /* No more packets to be read from the backend. We stop and
	     * wait for a notification from the backend (netmap_rx_irq). */
            ND(1, "nr_hwtail: %d rhead: %d dry_cycles: %d",
	       hwtail, kring->rhead, dry_cycles);
            break;
        }
#endif
        if (unlikely(ptns->stopped)) {
            D("backend netmap is being stopped");
            break;
        }
    }

    nm_kr_put(kring);

    /* Interrupt the guest if needed. */
    if (some_recvd && ptring_intr_enabled(ptring)) {
        ptring_intr_enable(ptring, 0);
        nm_os_kthread_send_irq(kth);
        IFRATE(ptns->rate_ctx.new.hrxk++);
    }
}

#ifdef NETMAP_PT_DEBUG
static void
ptnetmap_print_configuration(struct ptnetmap_cfg *cfg)
{
	int k;

	D("ptnetmap configuration:");
	D("  CSB ptrings @%p, num_rings=%u, cfgtype %08x", cfg->ptrings,
	  cfg->num_rings, cfg->cfgtype);
	for (k = 0; k < cfg->num_rings; k++) {
		switch (cfg->cfgtype) {
		case PTNETMAP_CFGTYPE_QEMU: {
			struct ptnetmap_cfgentry_qemu *e =
				(struct ptnetmap_cfgentry_qemu *)(cfg+1) + k;
			D("    ring #%d: ioeventfd=%lu, irqfd=%lu", k,
				(unsigned long)e->ioeventfd,
				(unsigned long)e->irqfd);
			break;
		}

		case PTNETMAP_CFGTYPE_BHYVE:
		{
			struct ptnetmap_cfgentry_bhyve *e =
				(struct ptnetmap_cfgentry_bhyve *)(cfg+1) + k;
			D("    ring #%d: wchan=%lu, ioctl_fd=%lu, "
			  "ioctl_cmd=%lu, msix_msg_data=%lu, msix_addr=%lu",
				k, (unsigned long)e->wchan,
				(unsigned long)e->ioctl_fd,
				(unsigned long)e->ioctl_cmd,
				(unsigned long)e->ioctl_data.msg_data,
				(unsigned long)e->ioctl_data.addr);
			break;
		}
		}
	}

}
#endif /* NETMAP_PT_DEBUG */

/* Copy actual state of the host ring into the CSB for the guest init */
static int
ptnetmap_kring_snapshot(struct netmap_kring *kring, struct ptnet_ring __user *ptring)
{
    if(CSB_WRITE(ptring, head, kring->rhead))
        goto err;
    if(CSB_WRITE(ptring, cur, kring->rcur))
        goto err;

    if(CSB_WRITE(ptring, hwcur, kring->nr_hwcur))
        goto err;
    if(CSB_WRITE(ptring, hwtail, NM_ACCESS_ONCE(kring->nr_hwtail)))
        goto err;

    DBG(ptnetmap_kring_dump("ptnetmap_kring_snapshot", kring);)

    return 0;
err:
    return EFAULT;
}

static struct netmap_kring *
ptnetmap_kring(struct netmap_pt_host_adapter *pth_na, int k)
{
	if (k < pth_na->up.num_tx_rings) {
		return pth_na->up.tx_rings + k;
	}
	return pth_na->up.rx_rings + k - pth_na->up.num_tx_rings;
}

static int
ptnetmap_krings_snapshot(struct netmap_pt_host_adapter *pth_na)
{
	struct ptnetmap_state *ptns = pth_na->ptns;
	struct netmap_kring *kring;
	unsigned int num_rings;
	int err = 0, k;

	num_rings = pth_na->up.num_tx_rings +
		    pth_na->up.num_rx_rings;

	for (k = 0; k < num_rings; k++) {
		kring = ptnetmap_kring(pth_na, k);
		err |= ptnetmap_kring_snapshot(kring, ptns->ptrings + k);
	}

	return err;
}

/*
 * Functions to create, start and stop the kthreads
 */

static int
ptnetmap_create_kthreads(struct netmap_pt_host_adapter *pth_na,
			 struct ptnetmap_cfg *cfg)
{
	struct ptnetmap_state *ptns = pth_na->ptns;
	struct nm_kthread_cfg nmk_cfg;
	unsigned int num_rings;
	uint8_t *cfg_entries = (uint8_t *)(cfg + 1);
	int k;

	num_rings = pth_na->up.num_tx_rings +
		    pth_na->up.num_rx_rings;

	for (k = 0; k < num_rings; k++) {
		nmk_cfg.attach_user = 1; /* attach kthread to user process */
		nmk_cfg.worker_private = ptnetmap_kring(pth_na, k);
		nmk_cfg.type = k;
		if (k < pth_na->up.num_tx_rings) {
			nmk_cfg.worker_fn = ptnetmap_tx_handler;
		} else {
			nmk_cfg.worker_fn = ptnetmap_rx_handler;
		}

		ptns->kthreads[k] = nm_os_kthread_create(&nmk_cfg,
			cfg->cfgtype, cfg_entries + k * cfg->entry_size);
		if (ptns->kthreads[k] == NULL) {
			goto err;
		}
	}

	return 0;
err:
	for (k = 0; k < num_rings; k++) {
		if (ptns->kthreads[k]) {
			nm_os_kthread_delete(ptns->kthreads[k]);
			ptns->kthreads[k] = NULL;
		}
	}
	return EFAULT;
}

static int
ptnetmap_start_kthreads(struct netmap_pt_host_adapter *pth_na)
{
	struct ptnetmap_state *ptns = pth_na->ptns;
	int num_rings;
	int error;
	int k;

	if (!ptns) {
		D("BUG ptns is NULL");
		return EFAULT;
	}

	ptns->stopped = false;

	num_rings = ptns->pth_na->up.num_tx_rings +
		    ptns->pth_na->up.num_rx_rings;
	for (k = 0; k < num_rings; k++) {
		//nm_os_kthread_set_affinity(ptns->kthreads[k], xxx);
		error = nm_os_kthread_start(ptns->kthreads[k]);
		if (error) {
			return error;
		}
	}

	return 0;
}

static void
ptnetmap_stop_kthreads(struct netmap_pt_host_adapter *pth_na)
{
	struct ptnetmap_state *ptns = pth_na->ptns;
	int num_rings;
	int k;

	if (!ptns) {
		/* Nothing to do. */
		return;
	}

	ptns->stopped = true;

	num_rings = ptns->pth_na->up.num_tx_rings +
		    ptns->pth_na->up.num_rx_rings;
	for (k = 0; k < num_rings; k++) {
		nm_os_kthread_stop(ptns->kthreads[k]);
	}
}

static struct ptnetmap_cfg *
ptnetmap_read_cfg(struct nmreq *nmr)
{
	uintptr_t *nmr_ptncfg = (uintptr_t *)&nmr->nr_arg1;
	struct ptnetmap_cfg *cfg;
	struct ptnetmap_cfg tmp;
	size_t cfglen;

	if (copyin((const void *)*nmr_ptncfg, &tmp, sizeof(tmp))) {
		D("Partial copyin() failed");
		return NULL;
	}

	cfglen = sizeof(tmp) + tmp.num_rings * tmp.entry_size;
	cfg = malloc(cfglen, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!cfg) {
		return NULL;
	}

	if (copyin((const void *)*nmr_ptncfg, cfg, cfglen)) {
		D("Full copyin() failed");
		free(cfg, M_DEVBUF);
		return NULL;
	}

	return cfg;
}

static int nm_unused_notify(struct netmap_kring *, int);
static int nm_pt_host_notify(struct netmap_kring *, int);

/* Create ptnetmap state and switch parent adapter to ptnetmap mode. */
static int
ptnetmap_create(struct netmap_pt_host_adapter *pth_na,
		struct ptnetmap_cfg *cfg)
{
    struct ptnetmap_state *ptns;
    unsigned int num_rings;
    int ret, i;

    /* Check if ptnetmap state is already there. */
    if (pth_na->ptns) {
        D("ERROR adapter %p already in ptnetmap mode", pth_na->parent);
        return EINVAL;
    }

    num_rings = pth_na->up.num_tx_rings + pth_na->up.num_rx_rings;

    if (num_rings != cfg->num_rings) {
        D("ERROR configuration mismatch, expected %u rings, found %u",
           num_rings, cfg->num_rings);
        return EINVAL;
    }

    ptns = malloc(sizeof(*ptns) + num_rings * sizeof(*ptns->kthreads),
		  M_DEVBUF, M_NOWAIT | M_ZERO);
    if (!ptns) {
        return ENOMEM;
    }

    ptns->kthreads = (struct nm_kthread **)(ptns + 1);
    ptns->stopped = true;

    /* Cross-link data structures. */
    pth_na->ptns = ptns;
    ptns->pth_na = pth_na;

    /* Store the CSB address provided by the hypervisor. */
    ptns->ptrings = cfg->ptrings;

    DBG(ptnetmap_print_configuration(cfg));

    /* Create kthreads */
    if ((ret = ptnetmap_create_kthreads(pth_na, cfg))) {
        D("ERROR ptnetmap_create_kthreads()");
        goto err;
    }
    /* Copy krings state into the CSB for the guest initialization */
    if ((ret = ptnetmap_krings_snapshot(pth_na))) {
        D("ERROR ptnetmap_krings_snapshot()");
        goto err;
    }

    /* Overwrite parent nm_notify krings callback. */
    pth_na->parent->na_private = pth_na;
    pth_na->parent_nm_notify = pth_na->parent->nm_notify;
    pth_na->parent->nm_notify = nm_unused_notify;

    for (i = 0; i < pth_na->parent->num_rx_rings; i++) {
        pth_na->up.rx_rings[i].save_notify =
        	pth_na->up.rx_rings[i].nm_notify;
        pth_na->up.rx_rings[i].nm_notify = nm_pt_host_notify;
    }
    for (i = 0; i < pth_na->parent->num_tx_rings; i++) {
        pth_na->up.tx_rings[i].save_notify =
        	pth_na->up.tx_rings[i].nm_notify;
        pth_na->up.tx_rings[i].nm_notify = nm_pt_host_notify;
    }

#ifdef RATE
    memset(&ptns->rate_ctx, 0, sizeof(ptns->rate_ctx));
    setup_timer(&ptns->rate_ctx.timer, &rate_callback,
            (unsigned long)&ptns->rate_ctx);
    if (mod_timer(&ptns->rate_ctx.timer, jiffies + msecs_to_jiffies(1500)))
        D("[ptn] Error: mod_timer()\n");
#endif

    DBG(D("[%s] ptnetmap configuration DONE", pth_na->up.name));

    return 0;

err:
    pth_na->ptns = NULL;
    free(ptns, M_DEVBUF);
    return ret;
}

/* Switch parent adapter back to normal mode and destroy
 * ptnetmap state. */
static void
ptnetmap_delete(struct netmap_pt_host_adapter *pth_na)
{
    struct ptnetmap_state *ptns = pth_na->ptns;
    int num_rings;
    int i;

    if (!ptns) {
	/* Nothing to do. */
        return;
    }

    /* Restore parent adapter callbacks. */
    pth_na->parent->nm_notify = pth_na->parent_nm_notify;
    pth_na->parent->na_private = NULL;

    for (i = 0; i < pth_na->parent->num_rx_rings; i++) {
        pth_na->up.rx_rings[i].nm_notify =
        	pth_na->up.rx_rings[i].save_notify;
        pth_na->up.rx_rings[i].save_notify = NULL;
    }
    for (i = 0; i < pth_na->parent->num_tx_rings; i++) {
        pth_na->up.tx_rings[i].nm_notify =
        	pth_na->up.tx_rings[i].save_notify;
        pth_na->up.tx_rings[i].save_notify = NULL;
    }

    /* Delete kthreads. */
    num_rings = ptns->pth_na->up.num_tx_rings +
                ptns->pth_na->up.num_rx_rings;
    for (i = 0; i < num_rings; i++) {
        nm_os_kthread_delete(ptns->kthreads[i]);
	ptns->kthreads[i] = NULL;
    }

    IFRATE(del_timer(&ptns->rate_ctx.timer));

    free(ptns, M_DEVBUF);

    pth_na->ptns = NULL;

    DBG(D("[%s] ptnetmap deleted", pth_na->up.name));
}

/*
 * Called by netmap_ioctl().
 * Operation is indicated in nmr->nr_cmd.
 *
 * Called without NMG_LOCK.
 */
int
ptnetmap_ctl(struct nmreq *nmr, struct netmap_adapter *na)
{
    struct netmap_pt_host_adapter *pth_na;
    struct ptnetmap_cfg *cfg;
    char *name;
    int cmd, error = 0;

    name = nmr->nr_name;
    cmd = nmr->nr_cmd;

    DBG(D("name: %s", name));

    if (!nm_ptnetmap_host_on(na)) {
        D("ERROR Netmap adapter %p is not a ptnetmap host adapter", na);
        error = ENXIO;
        goto done;
    }
    pth_na = (struct netmap_pt_host_adapter *)na;

    NMG_LOCK();
    switch (cmd) {
    case NETMAP_PT_HOST_CREATE:
	/* Read hypervisor configuration from userspace. */
        cfg = ptnetmap_read_cfg(nmr);
        if (!cfg)
            break;
        /* Create ptnetmap state (kthreads, ...) and switch parent
	 * adapter to ptnetmap mode. */
        error = ptnetmap_create(pth_na, cfg);
	free(cfg, M_DEVBUF);
        if (error)
            break;
        /* Start kthreads. */
        error = ptnetmap_start_kthreads(pth_na);
        if (error)
            ptnetmap_delete(pth_na);
        break;

    case NETMAP_PT_HOST_DELETE:
        /* Stop kthreads. */
        ptnetmap_stop_kthreads(pth_na);
        /* Switch parent adapter back to normal mode and destroy
	 * ptnetmap state (kthreads, ...). */
        ptnetmap_delete(pth_na);
        break;

    default:
        D("ERROR invalid cmd (nmr->nr_cmd) (0x%x)", cmd);
        error = EINVAL;
        break;
    }
    NMG_UNLOCK();

done:
    return error;
}

/* nm_notify callbacks for ptnetmap */
static int
nm_pt_host_notify(struct netmap_kring *kring, int flags)
{
	struct netmap_adapter *na = kring->na;
	struct netmap_pt_host_adapter *pth_na =
		(struct netmap_pt_host_adapter *)na->na_private;
	struct ptnetmap_state *ptns;
	int k;

	/* First check that the passthrough port is not being destroyed. */
	if (unlikely(!pth_na)) {
		return NM_IRQ_COMPLETED;
	}

	ptns = pth_na->ptns;
	if (unlikely(!ptns || ptns->stopped)) {
		return NM_IRQ_COMPLETED;
	}

	k = kring->ring_id;

	/* Notify kthreads (wake up if needed) */
	if (kring->tx == NR_TX) {
		ND(1, "TX backend irq");
		IFRATE(ptns->rate_ctx.new.btxwu++);
	} else {
		k += pth_na->up.num_tx_rings;
		ND(1, "RX backend irq");
		IFRATE(ptns->rate_ctx.new.brxwu++);
	}
	nm_os_kthread_wakeup_worker(ptns->kthreads[k]);

	return NM_IRQ_COMPLETED;
}

static int
nm_unused_notify(struct netmap_kring *kring, int flags)
{
    D("BUG this should never be called");
    return ENXIO;
}

/* nm_config callback for bwrap */
static int
nm_pt_host_config(struct netmap_adapter *na, u_int *txr, u_int *txd,
        u_int *rxr, u_int *rxd)
{
    struct netmap_pt_host_adapter *pth_na =
        (struct netmap_pt_host_adapter *)na;
    struct netmap_adapter *parent = pth_na->parent;
    int error;

    //XXX: maybe calling parent->nm_config is better

    /* forward the request */
    error = netmap_update_config(parent);

    *rxr = na->num_rx_rings = parent->num_rx_rings;
    *txr = na->num_tx_rings = parent->num_tx_rings;
    *txd = na->num_tx_desc = parent->num_tx_desc;
    *rxd = na->num_rx_desc = parent->num_rx_desc;

    DBG(D("rxr: %d txr: %d txd: %d rxd: %d", *rxr, *txr, *txd, *rxd));

    return error;
}

/* nm_krings_create callback for ptnetmap */
static int
nm_pt_host_krings_create(struct netmap_adapter *na)
{
    struct netmap_pt_host_adapter *pth_na =
        (struct netmap_pt_host_adapter *)na;
    struct netmap_adapter *parent = pth_na->parent;
    enum txrx t;
    int error;

    DBG(D("%s", pth_na->up.name));

    /* create the parent krings */
    error = parent->nm_krings_create(parent);
    if (error) {
        return error;
    }

    /* A ptnetmap host adapter points the very same krings
     * as its parent adapter. These pointer are used in the
     * TX/RX worker functions. */
    na->tx_rings = parent->tx_rings;
    na->rx_rings = parent->rx_rings;
    na->tailroom = parent->tailroom;

    for_rx_tx(t) {
	struct netmap_kring *kring;

	/* Parent's kring_create function will initialize
	 * its own na->si. We have to init our na->si here. */
	nm_os_selinfo_init(&na->si[t]);

	/* Force the mem_rings_create() method to create the
	 * host rings independently on what the regif asked for:
	 * these rings are needed by the guest ptnetmap adapter
	 * anyway. */
	kring = &NMR(na, t)[nma_get_nrings(na, t)];
	kring->nr_kflags |= NKR_NEEDRING;
    }

    return 0;
}

/* nm_krings_delete callback for ptnetmap */
static void
nm_pt_host_krings_delete(struct netmap_adapter *na)
{
    struct netmap_pt_host_adapter *pth_na =
        (struct netmap_pt_host_adapter *)na;
    struct netmap_adapter *parent = pth_na->parent;

    DBG(D("%s", pth_na->up.name));

    parent->nm_krings_delete(parent);

    na->tx_rings = na->rx_rings = na->tailroom = NULL;
}

/* nm_register callback */
static int
nm_pt_host_register(struct netmap_adapter *na, int onoff)
{
    struct netmap_pt_host_adapter *pth_na =
        (struct netmap_pt_host_adapter *)na;
    struct netmap_adapter *parent = pth_na->parent;
    int error;
    DBG(D("%s onoff %d", pth_na->up.name, onoff));

    if (onoff) {
        /* netmap_do_regif has been called on the ptnetmap na.
         * We need to pass the information about the
         * memory allocator to the parent before
         * putting it in netmap mode
         */
        parent->na_lut = na->na_lut;
    }

    /* forward the request to the parent */
    error = parent->nm_register(parent, onoff);
    if (error)
        return error;


    if (onoff) {
        na->na_flags |= NAF_NETMAP_ON | NAF_PTNETMAP_HOST;
    } else {
        ptnetmap_delete(pth_na);
        na->na_flags &= ~(NAF_NETMAP_ON | NAF_PTNETMAP_HOST);
    }

    return 0;
}

/* nm_dtor callback */
static void
nm_pt_host_dtor(struct netmap_adapter *na)
{
    struct netmap_pt_host_adapter *pth_na =
        (struct netmap_pt_host_adapter *)na;
    struct netmap_adapter *parent = pth_na->parent;

    DBG(D("%s", pth_na->up.name));

    /* The equivalent of NETMAP_PT_HOST_DELETE if the hypervisor
     * didn't do it. */
    ptnetmap_stop_kthreads(pth_na);
    ptnetmap_delete(pth_na);

    parent->na_flags &= ~NAF_BUSY;

    netmap_adapter_put(pth_na->parent);
    pth_na->parent = NULL;
}

/* check if nmr is a request for a ptnetmap adapter that we can satisfy */
int
netmap_get_pt_host_na(struct nmreq *nmr, struct netmap_adapter **na, int create)
{
    struct nmreq parent_nmr;
    struct netmap_adapter *parent; /* target adapter */
    struct netmap_pt_host_adapter *pth_na;
    struct ifnet *ifp = NULL;
    int error;

    /* Check if it is a request for a ptnetmap adapter */
    if ((nmr->nr_flags & (NR_PTNETMAP_HOST)) == 0) {
        return 0;
    }

    D("Requesting a ptnetmap host adapter");

    pth_na = malloc(sizeof(*pth_na), M_DEVBUF, M_NOWAIT | M_ZERO);
    if (pth_na == NULL) {
        D("ERROR malloc");
        return ENOMEM;
    }

    /* first, try to find the adapter that we want to passthrough
     * We use the same nmr, after we have turned off the ptnetmap flag.
     * In this way we can potentially passthrough everything netmap understands.
     */
    memcpy(&parent_nmr, nmr, sizeof(parent_nmr));
    parent_nmr.nr_flags &= ~(NR_PTNETMAP_HOST);
    error = netmap_get_na(&parent_nmr, &parent, &ifp, create);
    if (error) {
        D("parent lookup failed: %d", error);
        goto put_out_noputparent;
    }
    DBG(D("found parent: %s", parent->name));

    /* make sure the interface is not already in use */
    if (NETMAP_OWNED_BY_ANY(parent)) {
        D("NIC %s busy, cannot ptnetmap", parent->name);
        error = EBUSY;
        goto put_out;
    }

    pth_na->parent = parent;

    /* Follow netmap_attach()-like operations for the host
     * ptnetmap adapter. */

    //XXX pth_na->up.na_flags = parent->na_flags;
    pth_na->up.num_rx_rings = parent->num_rx_rings;
    pth_na->up.num_tx_rings = parent->num_tx_rings;
    pth_na->up.num_tx_desc = parent->num_tx_desc;
    pth_na->up.num_rx_desc = parent->num_rx_desc;

    pth_na->up.nm_dtor = nm_pt_host_dtor;
    pth_na->up.nm_register = nm_pt_host_register;

    /* Reuse parent's adapter txsync and rxsync methods. */
    pth_na->up.nm_txsync = parent->nm_txsync;
    pth_na->up.nm_rxsync = parent->nm_rxsync;

    pth_na->up.nm_krings_create = nm_pt_host_krings_create;
    pth_na->up.nm_krings_delete = nm_pt_host_krings_delete;
    pth_na->up.nm_config = nm_pt_host_config;

    /* Set the notify method only or convenience, it will never
     * be used, since - differently from default krings_create - we
     * ptnetmap krings_create callback inits kring->nm_notify
     * directly. */
    pth_na->up.nm_notify = nm_unused_notify;

    pth_na->up.nm_mem = parent->nm_mem;

    pth_na->up.na_flags |= NAF_HOST_RINGS;

    error = netmap_attach_common(&pth_na->up);
    if (error) {
        D("ERROR netmap_attach_common()");
        goto put_out;
    }

    *na = &pth_na->up;
    netmap_adapter_get(*na);

    /* set parent busy, because attached for ptnetmap */
    parent->na_flags |= NAF_BUSY;

    strncpy(pth_na->up.name, parent->name, sizeof(pth_na->up.name));
    strcat(pth_na->up.name, "-PTN");

    DBG(D("%s ptnetmap request DONE", pth_na->up.name));

    /* drop the reference to the ifp, if any */
    if (ifp)
        if_rele(ifp);

    return 0;

put_out:
    netmap_adapter_put(parent);
    if (ifp)
	if_rele(ifp);
put_out_noputparent:
    free(pth_na, M_DEVBUF);
    return error;
}
#endif /* WITH_PTNETMAP_HOST */

#ifdef WITH_PTNETMAP_GUEST
/*
 * Guest ptnetmap txsync()/rxsync() routines, used in ptnet device drivers.
 * These routines are reused across the different operating systems supported
 * by netmap.
 */

/*
 * Reconcile host and guest views of the transmit ring.
 *
 * Guest user wants to transmit packets up to the one before ring->head,
 * and guest kernel knows tx_ring->hwcur is the first packet unsent
 * by the host kernel.
 *
 * We push out as many packets as possible, and possibly
 * reclaim buffers from previously completed transmission.
 *
 * Notifications from the host are enabled only if the user guest would
 * block (no space in the ring).
 */
bool
netmap_pt_guest_txsync(struct ptnet_ring *ptring, struct netmap_kring *kring,
		       int flags)
{
	bool notify = false;

	/* Disable notifications */
	ptring->guest_need_kick = 0;

	/*
	 * First part: tell the host (updating the CSB) to process the new
	 * packets.
	 */
	kring->nr_hwcur = ptring->hwcur;
	ptnetmap_guest_write_kring_csb(ptring, kring->rcur, kring->rhead);

        /* Ask for a kick from a guest to the host if needed. */
	if ((kring->rhead != kring->nr_hwcur &&
		NM_ACCESS_ONCE(ptring->host_need_kick)) ||
			(flags & NAF_FORCE_RECLAIM)) {
		ptring->sync_flags = flags;
		notify = true;
	}

	/*
	 * Second part: reclaim buffers for completed transmissions.
	 */
	if (nm_kr_txempty(kring) || (flags & NAF_FORCE_RECLAIM)) {
                ptnetmap_guest_read_kring_csb(ptring, kring);
	}

        /*
         * No more room in the ring for new transmissions. The user thread will
	 * go to sleep and we need to be notified by the host when more free
	 * space is available.
         */
	if (nm_kr_txempty(kring)) {
		/* Reenable notifications. */
		ptring->guest_need_kick = 1;
                /* Double check */
                ptnetmap_guest_read_kring_csb(ptring, kring);
                /* If there is new free space, disable notifications */
		if (unlikely(!nm_kr_txempty(kring))) {
			ptring->guest_need_kick = 0;
		}
	}

	ND(1, "TX - CSB: head:%u cur:%u hwtail:%u - KRING: head:%u cur:%u tail: %u",
			ptring->head, ptring->cur, ptring->hwtail,
			kring->rhead, kring->rcur, kring->nr_hwtail);

	return notify;
}

/*
 * Reconcile host and guest view of the receive ring.
 *
 * Update hwcur/hwtail from host (reading from CSB).
 *
 * If guest user has released buffers up to the one before ring->head, we
 * also give them to the host.
 *
 * Notifications from the host are enabled only if the user guest would
 * block (no more completed slots in the ring).
 */
bool
netmap_pt_guest_rxsync(struct ptnet_ring *ptring, struct netmap_kring *kring,
		       int flags)
{
	bool notify = false;

        /* Disable notifications */
	ptring->guest_need_kick = 0;

	/*
	 * First part: import newly received packets, by updating the kring
	 * hwtail to the hwtail known from the host (read from the CSB).
	 * This also updates the kring hwcur.
	 */
        ptnetmap_guest_read_kring_csb(ptring, kring);
	kring->nr_kflags &= ~NKR_PENDINTR;

	/*
	 * Second part: tell the host about the slots that guest user has
	 * released, by updating cur and head in the CSB.
	 */
	if (kring->rhead != kring->nr_hwcur) {
		ptnetmap_guest_write_kring_csb(ptring, kring->rcur,
					       kring->rhead);
                /* Ask for a kick from the guest to the host if needed. */
		if (NM_ACCESS_ONCE(ptring->host_need_kick)) {
			ptring->sync_flags = flags;
			notify = true;
		}
	}

        /*
         * No more completed RX slots. The user thread will go to sleep and
	 * we need to be notified by the host when more RX slots have been
	 * completed.
         */
	if (nm_kr_rxempty(kring)) {
		/* Reenable notifications. */
                ptring->guest_need_kick = 1;
                /* Double check */
                ptnetmap_guest_read_kring_csb(ptring, kring);
                /* If there are new slots, disable notifications. */
		if (!nm_kr_rxempty(kring)) {
                        ptring->guest_need_kick = 0;
                }
        }

	ND(1, "RX - CSB: head:%u cur:%u hwtail:%u - KRING: head:%u cur:%u",
		ptring->head, ptring->cur, ptring->hwtail,
		kring->rhead, kring->rcur);

	return notify;
}

/*
 * Callbacks for ptnet drivers: nm_krings_create, nm_krings_delete, nm_dtor.
 */
int
ptnet_nm_krings_create(struct netmap_adapter *na)
{
	struct netmap_pt_guest_adapter *ptna =
			(struct netmap_pt_guest_adapter *)na; /* Upcast. */
	struct netmap_adapter *na_nm = &ptna->hwup.up;
	struct netmap_adapter *na_dr = &ptna->dr.up;
	int ret;

	if (ptna->backend_regifs) {
		return 0;
	}

	/* Create krings on the public netmap adapter. */
	ret = netmap_hw_krings_create(na_nm);
	if (ret) {
		return ret;
	}

	/* Copy krings into the netmap adapter private to the driver. */
	na_dr->tx_rings = na_nm->tx_rings;
	na_dr->rx_rings = na_nm->rx_rings;

	return 0;
}

void
ptnet_nm_krings_delete(struct netmap_adapter *na)
{
	struct netmap_pt_guest_adapter *ptna =
			(struct netmap_pt_guest_adapter *)na; /* Upcast. */
	struct netmap_adapter *na_nm = &ptna->hwup.up;
	struct netmap_adapter *na_dr = &ptna->dr.up;

	if (ptna->backend_regifs) {
		return;
	}

	na_dr->tx_rings = NULL;
	na_dr->rx_rings = NULL;

	netmap_hw_krings_delete(na_nm);
}

void
ptnet_nm_dtor(struct netmap_adapter *na)
{
	struct netmap_pt_guest_adapter *ptna =
			(struct netmap_pt_guest_adapter *)na;

	netmap_mem_put(ptna->dr.up.nm_mem);
	memset(&ptna->dr, 0, sizeof(ptna->dr));
	netmap_mem_pt_guest_ifp_del(na->nm_mem, na->ifp);
}

#endif /* WITH_PTNETMAP_GUEST */
