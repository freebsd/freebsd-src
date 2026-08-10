/*-
 * Copyright (c) 2026 Justin Hibbits
 * Copyright (c) 2012 Semihalf.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_arp.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp_lro.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include "miibus_if.h"

#include "bman.h"
#include "dpaa_common.h"
#include "dpaa_eth.h"
#include "fman.h"
#include "fman_keygen.h"
#include "fman_parser.h"
#include "fman_port.h"
#include "fman_if.h"
#include "fman_port_if.h"
#include "if_dtsec.h"
#include "qman.h"
#include "qman_var.h"
#include "qman_portal_if.h"


#define DPAA_ETH_LOCK(sc)		mtx_lock(&(sc)->sc_lock)
#define DPAA_ETH_UNLOCK(sc)		mtx_unlock(&(sc)->sc_lock)
#define DPAA_ETH_LOCK_ASSERT(sc)	mtx_assert(&(sc)->sc_lock, MA_OWNED)

/*
 * On 64-bit Book-E the direct map is always present, and the driver's
 * UMA zones plus page-sized mbuf clusters live in it.  Bypass the
 * page-table walk in pmap_kextract() for those; fall back for
 * MJUM9BYTES/MJUM16BYTES clusters, which are kmem_alloc_contig()'d
 * into KVA.
 */
static inline vm_paddr_t
dpaa_eth_va_to_phys(vm_offset_t va)
{
	if (__predict_true(va >= DMAP_BASE_ADDRESS && va <= DMAP_MAX_ADDRESS))
		return (DMAP_TO_PHYS(va));
	return (pmap_kextract(va));
}

/**
 * @group dTSEC RM private defines.
 * @{
 */
#define	DTSEC_BPOOLS_USED	(1)
#define	DTSEC_MAX_TX_QUEUE_LEN	256
/*
 * Sample the hardware TX FQ counter every Nth packet.  The FQ counter is
 * 24 bits and the soft cap above is 256, so overshoot by N is trivial.
 */
#define	DTSEC_MAX_TX_QUEUE_CHECK_INTERVAL	32
/*
 * Confirmation callback drain-detection.  Fast path (TX not backpressured)
 * skips the MC call entirely; when flagged, we sample every Nth callback to
 * detect the drain-to-zero transition.
 */
#define	DTSEC_TX_CONF_CHECK_INTERVAL		32

struct dpaa_eth_frame_info {
	struct fman_internal_context	fi_ic;
	struct mbuf			*fi_mbuf;
	struct dpaa_sgte		fi_sgt[DPAA_NUM_OF_SG_TABLE_ENTRY];
};

/*
 * Loader-tunable override for the per-port RX FQ count.  0 (default)
 * means "auto".  Otherwise must be a power of two.
 */
static int dpaa_eth_nrxfqs_tunable = 0;
SYSCTL_NODE(_hw, OID_AUTO, dpaa, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "DPAA driver tunables");
static SYSCTL_NODE(_hw_dpaa, OID_AUTO, eth, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "DPAA Ethernet driver");
SYSCTL_INT(_hw_dpaa_eth, OID_AUTO, nrxfqs, CTLFLAG_RDTUN,
    &dpaa_eth_nrxfqs_tunable, 0,
    "Per-port RX FQ count override (0=auto, else power-of-2 in [1,8])");

enum dpaa_eth_pool_params {
	DTSEC_RM_POOL_RX_LOW_MARK	= 16,
	DTSEC_RM_POOL_RX_HIGH_MARK	= 64,
	DTSEC_RM_POOL_RX_MAX_SIZE	= 256,
	/*
	 * MAX_SIZE is a soft cap set well below the BMan hardware pool
	 * limit, so sampling the depth every N put-backs per CPU is safe:
	 * worst-case overshoot is N * ncpus buffers, still tiny vs. the
	 * hardware pool.
	 */
	DTSEC_RM_POOL_RX_CHECK_INTERVAL	= 32,

	DTSEC_RM_POOL_FI_LOW_MARK	= 16,
	DTSEC_RM_POOL_FI_HIGH_MARK	= 64,
	DTSEC_RM_POOL_FI_MAX_SIZE	= 256,
};

enum dpaa_eth_fq_params {
	DTSEC_RM_FQR_RX_WQ		= 1,
	DTSEC_RM_FQR_TX_WQ		= 1,
	DTSEC_RM_FQR_TX_CONF_WQ		= 1
};
/** @} */


/**
 * @group dTSEC Frame Info routines.
 * @{
 */
void
dpaa_eth_fi_pool_free(struct dpaa_eth_softc *sc)
{

	if (sc->sc_fi_zone != NULL)
		uma_zdestroy(sc->sc_fi_zone);
}

int
dpaa_eth_fi_pool_init(struct dpaa_eth_softc *sc)
{

	snprintf(sc->sc_fi_zname, sizeof(sc->sc_fi_zname), "%s: Frame Info",
	    device_get_nameunit(sc->sc_dev));

	sc->sc_fi_zone = uma_zcreate(sc->sc_fi_zname,
	    sizeof(struct dpaa_eth_frame_info), NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);

	return (0);
}

static struct dpaa_eth_frame_info *
dpaa_eth_fi_alloc(struct dpaa_eth_softc *sc)
{
	struct dpaa_eth_frame_info *fi;

	fi = uma_zalloc(sc->sc_fi_zone, M_NOWAIT | M_ZERO);

	return (fi);
}

static void
dpaa_eth_fi_free(struct dpaa_eth_softc *sc, struct dpaa_eth_frame_info *fi)
{

	uma_zfree(sc->sc_fi_zone, fi);
}
/** @} */


/**
 * @group dTSEC FMan PORT routines.
 * @{
 */
int
dpaa_eth_fm_port_rx_init(struct dpaa_eth_softc *sc)
{
	struct fman_port_params params;
	int error;

	/*
	 * dflt/err FQID is the base of the RSS range: non-hashable
	 * frames (ARP, IP fragments, non-IP) fall through to FQ #0.
	 */
	params.dflt_fqid = sc->sc_rx_fqid_base;
	params.err_fqid = sc->sc_rx_fqid_base;
	params.rx_params.num_pools = 1;
	params.rx_params.bpools[0].bpid = bman_get_bpid(sc->sc_rx_pool);
	params.rx_params.bpools[0].size = MCLBYTES;
	error = FMAN_PORT_CONFIG(sc->sc_rx_port, &params);
	error = FMAN_PORT_INIT(sc->sc_rx_port);
	if (error != 0) {
		device_printf(sc->sc_dev, "couldn't initialize FM Port RX.\n");
		return (ENXIO);
	}

	/*
	 * The RX port's own FMan hardware port ID (cell-index in the
	 * OFW node) is the index KG uses for scheme-binding.  It was
	 * previously left at zero, which made every port fight for KG
	 * port 0 -- only the first attach won, the rest got EBUSY.
	 */
	sc->sc_port_rx_hw_id = fman_port_get_id(sc->sc_rx_port);

	/*
	 * Wire up FMan KeyGen 5-tuple hashing across the sc_nrxfqs FQs
	 * created in dpaa_eth_fq_rx_init.  Skipped for the trivial N=1
	 * case (single-core system or forced fallback): with one FQ
	 * there's nothing to distribute.  Only flip the port's parser
	 * output to KG on success -- routing to KG with no bound scheme
	 * drops frames.
	 */
	if (sc->sc_nrxfqs > 1) {
		struct fman_softc *fman_sc =
		    device_get_softc(device_get_parent(sc->sc_rx_port));

		error = fman_kg_alloc_hash_scheme(fman_sc,
		    sc->sc_port_rx_hw_id, sc->sc_rx_fqid_base,
		    sc->sc_nrxfqs);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "fman_kg_alloc_hash_scheme failed: %d\n", error);
			/*
			 * Non-fatal: FMan port still delivers to
			 * dflt_fqid == sc_rx_fqid_base (FQ #0), which is
			 * a valid single-queue fallback.  Leave RFPNE at
			 * the BMI-enqueue default.
			 */
		} else {
			fman_port_rx_use_kg(sc->sc_rx_port, true);
		}
	}

	return (0);
}

int
dpaa_eth_fm_port_tx_init(struct dpaa_eth_softc *sc)
{
	struct fman_port_params params;
	int error;

	params.dflt_fqid = sc->sc_tx_conf_fqid;
	params.err_fqid = sc->sc_tx_conf_fqid;

	error = FMAN_PORT_CONFIG(sc->sc_tx_port, &params);
	error = FMAN_PORT_INIT(sc->sc_tx_port);
	if (error != 0) {
		device_printf(sc->sc_dev, "couldn't initialize FM Port TX.\n");
		return (ENXIO);
	}

	return (0);
}
/** @} */


/**
 * @group dTSEC buffer pools routines.
 * @{
 */
static int
dpaa_eth_pool_rx_put_buffer(struct dpaa_eth_softc *sc, uint8_t *buffer,
    void *context)
{

	uma_zfree(sc->sc_rx_zone, buffer);

	return (0);
}

static int
dtsec_add_buffers(struct dpaa_eth_softc *sc, int count)
{
	struct bman_buffer bufs[8] = {};
	int err;
	int c;

	while (count > 0) {
		c = min(8, count);
		for (int i = 0; i < c; i++) {
			void *b;
			vm_paddr_t pa;

			b = uma_zalloc(sc->sc_rx_zone, M_NOWAIT);
			if (b == NULL)
				return (ENOMEM);
			pa = DMAP_TO_PHYS((vm_offset_t)b);
			bufs[i].buf_hi = (pa >> 32);
			bufs[i].buf_lo = (pa & 0xffffffff);
		}

		err = bman_put_buffers(sc->sc_rx_pool, bufs, c);
		if (err != 0)
			return (err);
		count -= c;
	}

	return (0);
}

static void
dpaa_eth_pool_rx_depleted(void *h_App, bool in)
{
	struct dpaa_eth_softc *sc;
	unsigned int count;

	sc = h_App;

	if (!in)
		return;

	while (1) {
		count = bman_count(sc->sc_rx_pool);
		if (count > DTSEC_RM_POOL_RX_HIGH_MARK)
			return;

		/* Can only release 8 buffers at a time */
		count = min(DTSEC_RM_POOL_RX_HIGH_MARK - count + 8, 8);
		if (dtsec_add_buffers(sc, count) != 0)
			return;
	}
}

void
dpaa_eth_pool_rx_free(struct dpaa_eth_softc *sc)
{

	if (sc->sc_rx_pool != NULL)
		bman_pool_destroy(sc->sc_rx_pool);

	if (sc->sc_rx_zone != NULL)
		uma_zdestroy(sc->sc_rx_zone);

	free(sc->sc_rx_pool_check_cnt, M_DEVBUF);
	sc->sc_rx_pool_check_cnt = NULL;
}

int
dpaa_eth_pool_rx_init(struct dpaa_eth_softc *sc)
{

	/* MCLBYTES must be less than PAGE_SIZE */
	CTASSERT(MCLBYTES < PAGE_SIZE);

	snprintf(sc->sc_rx_zname, sizeof(sc->sc_rx_zname), "%s: RX Buffers",
	    device_get_nameunit(sc->sc_dev));

	sc->sc_rx_zone = uma_zcreate(sc->sc_rx_zname, MCLBYTES, NULL,
	    NULL, NULL, NULL, MCLBYTES - 1, 0);

	sc->sc_rx_pool_check_cnt = malloc_aligned(
	    (mp_maxid + 1) * sizeof(struct dpaa_pcpu_cnt),
	    CACHE_LINE_SIZE, M_DEVBUF, M_WAITOK | M_ZERO);

	sc->sc_rx_pool = bman_pool_create(&sc->sc_rx_bpid, MCLBYTES,
	    DTSEC_RM_POOL_RX_MAX_SIZE, DTSEC_RM_POOL_RX_LOW_MARK,
	    DTSEC_RM_POOL_RX_HIGH_MARK, 0, 0, dpaa_eth_pool_rx_depleted, sc);
	if (sc->sc_rx_pool == NULL) {
		device_printf(sc->sc_dev, "NULL rx pool  somehow\n");
		dpaa_eth_pool_rx_free(sc);
		return (EIO);
	}

	dtsec_add_buffers(sc, DTSEC_RM_POOL_RX_HIGH_MARK);

	return (0);
}
/** @} */


/**
 * @group dTSEC Frame Queue Range routines.
 * @{
 */
static void
dpaa_eth_fq_mext_free(struct mbuf *m)
{
	struct dpaa_eth_softc *sc;
	void *buffer;

	buffer = m->m_ext.ext_arg1;
	sc = m->m_ext.ext_arg2;
	/*
	 * Sloppy per-CPU sampling: no pin, no atomic.  A stray migration
	 * between the curcpu read and the increment can only mis-attribute
	 * one bump to the wrong CPU's counter; the sampling rate stays
	 * within the acceptable slop window.
	 */
	if ((++sc->sc_rx_pool_check_cnt[curcpu].cnt &
	    (DTSEC_RM_POOL_RX_CHECK_INTERVAL - 1)) == 0 &&
	    bman_count(sc->sc_rx_pool) > DTSEC_RM_POOL_RX_MAX_SIZE)
		dpaa_eth_pool_rx_put_buffer(sc, buffer, NULL);
	else
		bman_put_buffer(sc->sc_rx_pool,
		    DMAP_TO_PHYS((vm_offset_t)buffer), sc->sc_rx_bpid);
}

static int
dpaa_eth_update_csum_flags(struct qman_fd *frame,
    struct fman_parse_result *prs, struct mbuf *m)
{
	uint16_t l3r = be16toh(prs->l3r);

	/* TODO: nested protocols? */
	if ((l3r & L3R_FIRST_IP_M) != 0) {
		m->m_pkthdr.csum_flags |= CSUM_L3_CALC;
		if ((l3r & L3R_FIRST_ERROR) == 0)
			m->m_pkthdr.csum_flags |= CSUM_L3_VALID;
	}
	if (frame->cmd_stat & DPAA_FD_RX_STATUS_L4CV) {
		m->m_pkthdr.csum_flags |= CSUM_L4_CALC;
		m->m_pkthdr.csum_data = 0xffff;
		if ((prs->l4r & L4R_TYPE_M) != 0 &&
		    (prs->l4r & L4R_ERR) == 0)
			m->m_pkthdr.csum_flags |= CSUM_L4_VALID;
	}

	return (0);
}

static int
dpaa_eth_fq_rx_callback(device_t portal, struct qman_fq *fq,
    struct qman_fd *frame, void *app)
{
	struct dpaa_eth_rx_fq *rxfq;
	struct dpaa_eth_softc *sc;
	struct mbuf *m;
	struct fman_internal_context *frame_ic;
	void *frame_va;

	m = NULL;
	rxfq = app;
	sc = rxfq->sc;
	rxfq->frames_in++;

	frame_va = DPAA_FD_GET_ADDR(frame);
	frame_ic = frame_va;	/* internal context at head of the frame */
	/* Only simple (single- or multi-) frames are supported. */
	KASSERT(frame->format == 0 || frame->format == 4,
	    ("%s(): Got unsupported frame format 0x%02X!", __func__,
	    frame->format));

	if ((frame->cmd_stat & DPAA_FD_CMD_STAT_ERR_M) != 0) {
		device_printf(sc->sc_dev, "RX error: 0x%08X\n",
		    frame->cmd_stat);
		goto err;
	}

	m = m_gethdr(M_NOWAIT, MT_HEADER);
	if (m == NULL)
		goto err;

	if (frame->format == 0) {
		/* Single-frame format */
		m_extadd(m, (char *)frame_va + frame->offset, frame->length,
		    dpaa_eth_fq_mext_free, frame_va, sc, 0, EXT_NET_DRV);
	} else {
		struct dpaa_sgte *sgt =
		    (struct dpaa_sgte *)(char *)frame_va + frame->offset;
		/* Simple multi-frame format */
		for (int i = 0; i < DPAA_NUM_OF_SG_TABLE_ENTRY; i++) {
			if (sgt[i].length > 0)
				m_extadd(m, PHYS_TO_DMAP(sgt[i].addr),
				    sgt[i].length, dpaa_eth_fq_mext_free,
				    PHYS_TO_DMAP(sgt[i].addr), sc, 0,
				    EXT_NET_DRV);
			if (sgt[i].final)
				break;
		}
		/* Free the SGT buffer, it's no longer needed. */
		bman_put_buffer(sc->sc_rx_pool, frame->addr, sc->sc_rx_bpid);
	}

	if (if_getcapenable(sc->sc_ifnet) & (IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6))
		dpaa_eth_update_csum_flags(frame, &frame_ic->prs, m);

	m->m_pkthdr.rcvif = sc->sc_ifnet;
	m->m_len = frame->length;
	m_fixhdr(m);

	/*
	 * Offer to LRO first.
	 */
	if (rxfq->lro_inited &&
	    (if_getcapenable(sc->sc_ifnet) & IFCAP_LRO) != 0 &&
	    (m->m_pkthdr.csum_flags & (CSUM_L4_CALC | CSUM_L4_VALID)) ==
	     (CSUM_L4_CALC | CSUM_L4_VALID) &&
	    tcp_lro_rx(&rxfq->lro, m, 0) == 0)
		return (1);

	m->m_nextpkt = NULL;
	*rxfq->rx_tailp = m;
	rxfq->rx_tailp = &m->m_nextpkt;

	return (1);

err:
	bman_put_buffer(sc->sc_rx_pool, frame->addr, sc->sc_rx_bpid);
	if (m != NULL)
		m_freem(m);

	return (1);
}

/*
 * Post-poll flush hook invoked once per QMan portal poll on any
 * RX FQ that dispatched at least one frame this cycle.  Runs on
 * the FQ's affine CPU, outside the DQRR dispatch loop.
 */
static void
dpaa_eth_fq_rx_flush(struct qman_fq *fq __unused, void *ctx)
{
	struct dpaa_eth_rx_fq *rxfq = ctx;

	if (rxfq->rx_head != NULL) {
		struct mbuf *chain = rxfq->rx_head;

		rxfq->rx_head = NULL;
		rxfq->rx_tailp = &rxfq->rx_head;
		if_input(rxfq->sc->sc_ifnet, chain);
	}
	/*
	 * Flush LRO whenever initialised.  If the user disabled
	 * IFCAP_LRO between the last callback and this flush, entries
	 * queued in that window still need to be drained.
	 */
	if (rxfq->lro_inited)
		tcp_lro_flush_all(&rxfq->lro);
}

static int
dpaa_eth_fq_tx_confirm_callback(device_t portal, struct qman_fq *fq,
    struct qman_fd *frame, void *app)
{
	struct dpaa_eth_frame_info *fi;
	struct dpaa_eth_softc *sc;

	sc = app;

	if ((frame->cmd_stat & DPAA_FD_TX_STAT_ERR_M) != 0)
		device_printf(sc->sc_dev, "TX error: 0x%08X\n",
		    frame->cmd_stat);

	/*
	 * We are storing struct dpaa_eth_frame_info in first entry
	 * of scatter-gather table.
	 */
	fi = (struct dpaa_eth_frame_info *)PHYS_TO_DMAP(frame->addr);

	/* Free transmitted frame */
	m_freem(fi->fi_mbuf);
	dpaa_eth_fi_free(sc, fi);

	/*
	 * Fast path: TX isn't backpressured, so there's nothing to
	 * restart.  Acquire load pairs with the release store on the
	 * TX path so a concurrent set of the flag is observed here.
	 */
	if (atomic_load_acq_int(&sc->sc_tx_fq_full) == 0)
		return (1);

	/* Rate-limit the MC round-trip to detect drain-to-zero. */
	if ((sc->sc_tx_conf_check_cnt++ &
	    (DTSEC_TX_CONF_CHECK_INTERVAL - 1)) != 0)
		return (1);
	if (qman_fq_get_counter(sc->sc_tx_conf_fq, QMAN_COUNTER_FRAME) != 0)
		return (1);

	DPAA_ETH_LOCK(sc);
	if (sc->sc_tx_fq_full) {
		atomic_store_rel_int(&sc->sc_tx_fq_full, 0);
		dpaa_eth_if_start_locked(sc);
	}
	DPAA_ETH_UNLOCK(sc);

	return (1);
}

void
dpaa_eth_fq_rx_free(struct dpaa_eth_softc *sc)
{
	int i;

	/*
	 * Tear down the KG scheme first so no new frames land on FQs
	 * about to be retired.  Point the parser output back at BMI-
	 * enqueue before freeing the scheme so the port keeps
	 * delivering to dflt_fqid instead of dropping through an
	 * emptied KG.
	 */
	if (sc->sc_nrxfqs > 1 && sc->sc_rx_port != NULL) {
		struct fman_softc *fman_sc =
		    device_get_softc(device_get_parent(sc->sc_rx_port));

		fman_port_rx_use_kg(sc->sc_rx_port, false);
		(void)fman_kg_free_hash_scheme(fman_sc,
		    sc->sc_port_rx_hw_id);
	}

	if (sc->sc_rx_fqs != NULL) {
		for (i = 0; i < sc->sc_nrxfqs; i++) {
			if (sc->sc_rx_fqs[i].fq != NULL)
				qman_fq_free(sc->sc_rx_fqs[i].fq);
			/*
			 * Any pending non-LRO mbufs on the batch chain
			 * are dropped here rather than delivered late.
			 */
			if (sc->sc_rx_fqs[i].rx_head != NULL)
				m_freem(sc->sc_rx_fqs[i].rx_head);
			if (sc->sc_rx_fqs[i].lro_inited)
				tcp_lro_free(&sc->sc_rx_fqs[i].lro);
		}
		free(sc->sc_rx_fqs, M_DEVBUF);
		sc->sc_rx_fqs = NULL;
	}
	if (sc->sc_nrxfqs > 0) {
		qman_free_fqid_range(sc->sc_rx_fqid_base, sc->sc_nrxfqs);
		sc->sc_nrxfqs = 0;
		sc->sc_rx_fqid_base = 0;
	}
}

int
dpaa_eth_fq_rx_init(struct dpaa_eth_softc *sc)
{
	struct qman_fq *fq;
	uint32_t base_fqid;
	int align, nfqs;
	int error, i;

	if (dpaa_eth_nrxfqs_tunable > 0)
		nfqs = dpaa_eth_nrxfqs_tunable;
	else
		nfqs = mp_ncpus;

	align = 1 << ilog2(nfqs);
	error = qman_alloc_fqid_range(nfqs, align, &base_fqid);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "could not reserve %d contiguous FQIDs (aligned): %d\n",
		    nfqs, error);
		return (EIO);
	}

	sc->sc_nrxfqs = nfqs;
	sc->sc_rx_fqid_base = base_fqid;
	sc->sc_rx_fqs = malloc(nfqs * sizeof(*sc->sc_rx_fqs),
	    M_DEVBUF, M_WAITOK | M_ZERO);

	/*
	 * Create the N RX FQs, one per per-CPU channel.  QMan portal
	 * attach has already subscribed each portal to its own
	 * per-CPU channel, so frames land on the right core without
	 * per-driver static-dequeue plumbing.
	 *
	 * Stash 1 cacheline of frame annotation (parse result / IC)
	 * and 1 of frame data head into the destination core's cache
	 * when QMan dequeues an RX frame -- the RX callback reads
	 * both.
	 */
	for (i = 0; i < nfqs; i++) {
		int chan = qman_percpu_channel(i);

		if (chan == -1) {
			device_printf(sc->sc_dev,
			    "no per-CPU QMan channel for CPU %d\n", i);
			error = EIO;
			goto err;
		}
		fq = qman_fq_create(1, chan, DTSEC_RM_FQR_RX_WQ,
		    /*force_fqid=*/true, base_fqid + i,
		    false, false, true, false, 0, 0, 0, 1, 1);
		if (fq == NULL) {
			device_printf(sc->sc_dev,
			    "could not create RX FQ %d (fqid 0x%x)\n",
			    i, base_fqid + i);
			error = EIO;
			goto err;
		}
		sc->sc_rx_fqs[i].fq = fq;
		sc->sc_rx_fqs[i].fqid = base_fqid + i;
		sc->sc_rx_fqs[i].cpu = i;
		sc->sc_rx_fqs[i].sc = sc;
		sc->sc_rx_fqs[i].rx_head = NULL;
		sc->sc_rx_fqs[i].rx_tailp = &sc->sc_rx_fqs[i].rx_head;

		/* Best-effort LRO per FQ. */
		if (tcp_lro_init(&sc->sc_rx_fqs[i].lro) == 0) {
			sc->sc_rx_fqs[i].lro.ifp = sc->sc_ifnet;
			sc->sc_rx_fqs[i].lro_inited = true;
		}

		error = qman_fq_register_cb(fq, dpaa_eth_fq_rx_callback,
		    &sc->sc_rx_fqs[i]);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "could not register RX callback for FQ %d\n", i);
			goto err;
		}
		(void)qman_fq_register_flush_cb(fq, dpaa_eth_fq_rx_flush);
	}

	/*
	 * Expose per-FQ observability under dev.<port>.rx_fq.<i>.{cpu,
	 * fqid, frames}.  The sysctl_ctx owned by sc_dev handles all
	 * teardown at device detach, so nothing to unwind on the free
	 * path.
	 */
	{
		struct sysctl_ctx_list *ctx =
		    device_get_sysctl_ctx(sc->sc_dev);
		struct sysctl_oid *tree = device_get_sysctl_tree(sc->sc_dev);
		struct sysctl_oid *rxnode = SYSCTL_ADD_NODE(ctx,
		    SYSCTL_CHILDREN(tree), OID_AUTO, "rx_fq",
		    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL,
		    "Per-FQ RX statistics");

		for (i = 0; i < nfqs; i++) {
			char name[8];
			struct sysctl_oid *fqnode;

			snprintf(name, sizeof(name), "%d", i);
			fqnode = SYSCTL_ADD_NODE(ctx,
			    SYSCTL_CHILDREN(rxnode), OID_AUTO, name,
			    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "");
			SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(fqnode),
			    OID_AUTO, "fqid", CTLFLAG_RD,
			    &sc->sc_rx_fqs[i].fqid, 0, "FQID");
			SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(fqnode),
			    OID_AUTO, "cpu", CTLFLAG_RD,
			    &sc->sc_rx_fqs[i].cpu, 0,
			    "CPU affine to this FQ");
			SYSCTL_ADD_UQUAD(ctx, SYSCTL_CHILDREN(fqnode),
			    OID_AUTO, "frames", CTLFLAG_RD,
			    &sc->sc_rx_fqs[i].frames_in,
			    "Frames dispatched to the RX callback");
		}
	}

	if (bootverbose) {
		device_printf(sc->sc_dev,
		    "RSS: %d RX FQ%s starting at fqid 0x%x, %s\n",
		    nfqs, nfqs == 1 ? "" : "s", base_fqid,
		    nfqs == 1 ? "no distribution" :
		    "IP 5-tuple hash via KeyGen");
	}

	return (0);

err:
	dpaa_eth_fq_rx_free(sc);
	return (error);
}

void
dpaa_eth_fq_tx_free(struct dpaa_eth_softc *sc)
{
	int cpu;

	if (sc->sc_tx_fq)
		qman_fq_free(sc->sc_tx_fq);

	if (sc->sc_tx_conf_fq)
		qman_fq_free(sc->sc_tx_conf_fq);

	/*
	 * The port's pool channel now hosts only TX confirms (RX has
	 * moved to per-CPU pool channels).  Unsubscribe every portal
	 * and release the channel here rather than in fq_rx_free.
	 */
	if (sc->sc_rx_channel != 0) {
		CPU_FOREACH(cpu) {
			device_t portal = DPCPU_ID_GET(cpu, qman_affine_portal);
			QMAN_PORTAL_STATIC_DEQUEUE_RM_CHANNEL(portal,
			    sc->sc_rx_channel);
		}
		qman_free_channel(sc->sc_rx_channel);
		sc->sc_rx_channel = 0;
	}
}

int
dpaa_eth_fq_tx_init(struct dpaa_eth_softc *sc)
{
	int error;
	int cpu;
	void *fq;

	/* TX Frame Queue */
	fq = qman_fq_create(1, sc->sc_port_tx_qman_chan,
	    DTSEC_RM_FQR_TX_WQ, false, 0, false, false, true, false, 0, 0, 0,
	    0, 0);
	if (fq == NULL) {
		device_printf(sc->sc_dev, "could not create default TX queue"
		    "\n");
		return (EIO);
	}

	sc->sc_tx_fq = fq;

	/*
	 * TX confirms need a pool channel with at least one subscriber.
	 * Historically the RX path allocated and subscribed sc_rx_channel
	 * for both RX and TX confirms; now RX uses per-CPU channels, so
	 * this path is the sole owner.  Allocate + subscribe here.
	 * (Confirm handling doesn't benefit from CPU pinning today; a
	 * follow-on could route TX confirms to the enqueue CPU via the
	 * same per-CPU channel infrastructure.)
	 */
	if (sc->sc_rx_channel == 0) {
		sc->sc_rx_channel = qman_alloc_channel();
		CPU_FOREACH(cpu) {
			device_t portal = DPCPU_ID_GET(cpu, qman_affine_portal);
			QMAN_PORTAL_STATIC_DEQUEUE_CHANNEL(portal,
			    sc->sc_rx_channel);
		}
	}

	/* TX Confirmation Frame Queue */
	fq = qman_fq_create(1, sc->sc_rx_channel,
	    DTSEC_RM_FQR_TX_CONF_WQ, false, 0, false, false, true, false, 0, 0,
	    0, 0, 0);
	if (fq == NULL) {
		device_printf(sc->sc_dev, "could not create TX confirmation "
		    "queue\n");
		dpaa_eth_fq_tx_free(sc);
		return (EIO);
	}

	sc->sc_tx_conf_fq = fq;
	sc->sc_tx_conf_fqid = qman_fq_get_fqid(fq);

	error = qman_fq_register_cb(fq, dpaa_eth_fq_tx_confirm_callback, sc);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not register TX confirmation "
		    "callback\n");
		dpaa_eth_fq_tx_free(sc);
		return (EIO);
	}

	return (0);
}
/** @} */

/* Returns the cmd_stat field for the frame descriptor */
static uint32_t
dpaa_eth_tx_add_csum(struct dpaa_eth_frame_info *fi)
{
	struct mbuf *m = fi->fi_mbuf;
	struct fman_parse_result *prs = &fi->fi_ic.prs;
	uint32_t csum_flags = m->m_pkthdr.csum_flags;
	uint8_t ether_size = ETHER_HDR_LEN;

	if ((csum_flags & CSUM_FLAGS_TX) == 0)
		return (0);

	if (m->m_flags & M_VLANTAG)
		ether_size += ETHER_VLAN_ENCAP_LEN;
	if (csum_flags & CSUM_IP)
		prs->l3r = L3R_FIRST_IPV4;
	if (csum_flags & CSUM_IP_UDP) {
		prs->l4r = L4R_TYPE_UDP;
		prs->l4_off = ether_size + sizeof(struct ip);
	} else if (csum_flags & CSUM_IP_TCP) {
		prs->l4r = L4R_TYPE_TCP;
		prs->l4_off = ether_size + sizeof(struct ip);
	} else if (csum_flags & CSUM_IP6_UDP) {
		prs->l3r = L3R_FIRST_IPV6;
		prs->l4r = L4R_TYPE_UDP;
		prs->l4_off = ether_size + sizeof(struct ip6_hdr);
	} else if (csum_flags & CSUM_IP6_TCP) {
		prs->l3r = L3R_FIRST_IPV6;
		prs->l4r = L4R_TYPE_TCP;
		prs->l4_off = ether_size + sizeof(struct ip6_hdr);
	}

	prs->ip_off[0] = ether_size;

	return (DPAA_FD_TX_CMD_RPD | DPAA_FD_TX_CMD_DTC);
}

/**
 * @group dTSEC IFnet routines.
 * @{
 */
void
dpaa_eth_if_start_locked(struct dpaa_eth_softc *sc)
{
	vm_size_t psize, ssize;
	struct dpaa_eth_frame_info *fi;
	unsigned int i;
	struct mbuf *m0, *m;
	vm_offset_t vaddr;
	struct dpaa_fd fd;

	DPAA_ETH_LOCK_ASSERT(sc);
	/* TODO: IFF_DRV_OACTIVE */

	if ((sc->sc_mii->mii_media_status & IFM_ACTIVE) == 0)
		return;

	if ((if_getdrvflags(sc->sc_ifnet) & IFF_DRV_RUNNING) != IFF_DRV_RUNNING)
		return;

	if (sc->sc_tx_fq_full)
		return;

	while (!if_sendq_empty(sc->sc_ifnet)) {
		if ((sc->sc_tx_queue_check_cnt++ &
		    (DTSEC_MAX_TX_QUEUE_CHECK_INTERVAL - 1)) == 0 &&
		    qman_fq_get_counter(sc->sc_tx_fq, QMAN_COUNTER_FRAME) >=
		    DTSEC_MAX_TX_QUEUE_LEN) {
			atomic_store_rel_int(&sc->sc_tx_fq_full, 1);
			sc->sc_tx_queue_check_cnt = 0;
			return;
		}

		fi = dpaa_eth_fi_alloc(sc);
		if (fi == NULL)
			return;

		m0 = if_dequeue(sc->sc_ifnet);
		if (m0 == NULL) {
			dpaa_eth_fi_free(sc, fi);
			return;
		}

		i = 0;
		psize = 0;
		fi->fi_mbuf = m0;

		for (m = m0; m != NULL && i < DPAA_NUM_OF_SG_TABLE_ENTRY;
		    m = m->m_next) {
			vm_size_t rem;

			if (m->m_len == 0)
				continue;

			vaddr = (vm_offset_t)m->m_data;
			rem = m->m_len;

			/*
			 * Fast path: the whole segment lives inside one
			 * page.  Covers every default-cluster mbuf
			 * (MCLBYTES < PAGE_SIZE) and skips the split loop
			 * in the common case.
			 */
			if ((vaddr & PAGE_MASK) + rem <= PAGE_SIZE) {
				fi->fi_sgt[i].addr = dpaa_eth_va_to_phys(vaddr);
				fi->fi_sgt[i].length = rem;
				fi->fi_sgt[i].extension = 0;
				fi->fi_sgt[i].final = 0;
				fi->fi_sgt[i].bpid = 0;
				fi->fi_sgt[i].offset = 0;
				psize += rem;
				i++;
				continue;
			}

			/*
			 * Slow path: mbuf crosses at least one page
			 * boundary (jumbo cluster, or an unusually-offset
			 * external buffer).  Emit one SGT entry per
			 * contiguous physical span.
			 */
			while (rem > 0 && i < DPAA_NUM_OF_SG_TABLE_ENTRY) {
				ssize = PAGE_SIZE - (vaddr & PAGE_MASK);
				if (rem < ssize)
					ssize = rem;

				fi->fi_sgt[i].addr = dpaa_eth_va_to_phys(vaddr);
				fi->fi_sgt[i].length = ssize;
				fi->fi_sgt[i].extension = 0;
				fi->fi_sgt[i].final = 0;
				fi->fi_sgt[i].bpid = 0;
				fi->fi_sgt[i].offset = 0;

				rem -= ssize;
				vaddr += ssize;
				psize += ssize;
				i++;
			}

			if (rem > 0)		/* SGT full mid-mbuf */
				break;
		}

		/*
		 * Reject the frame if we didn't consume the whole chain
		 * (SGT full mid-frame) or if the chain produced no SGT
		 * entries at all (all-zero-length mbufs).
		 */
		if (m != NULL || i == 0) {
			dpaa_eth_fi_free(sc, fi);
			m_freem(m0);
			continue;
		}

		fi->fi_sgt[i - 1].final = 1;

		fd.addr = DMAP_TO_PHYS((vm_offset_t)fi);
		fd.length = psize;
		fd.format = DPAA_FD_FORMAT_SHORT_MBSF;

		fd.liodn = 0;
		fd.bpid = 0;
		fd.eliodn = 0;
		fd.offset = offsetof(struct dpaa_eth_frame_info, fi_sgt);
		fd.cmd_stat = dpaa_eth_tx_add_csum(fi);

		DPAA_ETH_UNLOCK(sc);
		if (qman_fq_enqueue(sc->sc_tx_fq, &fd) != 0) {
			dpaa_eth_fi_free(sc, fi);
			m_freem(m0);
		}
		DPAA_ETH_LOCK(sc);
	}
}
/** @} */
