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

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include "miibus_if.h"

#include "bman.h"
#include "dpaa_common.h"
#include "dpaa_eth.h"
#include "fman.h"
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

/**
 * @group dTSEC RM private defines.
 * @{
 */
#define	DTSEC_BPOOLS_USED	(1)
#define	DTSEC_MAX_TX_QUEUE_LEN	256

struct dpaa_eth_frame_info {
	struct mbuf			*fi_mbuf;
	struct fman_internal_context	fi_ic;
	struct dpaa_sgte		fi_sgt[DPAA_NUM_OF_SG_TABLE_ENTRY];
};

enum dpaa_eth_pool_params {
	DTSEC_RM_POOL_RX_LOW_MARK	= 16,
	DTSEC_RM_POOL_RX_HIGH_MARK	= 64,
	DTSEC_RM_POOL_RX_MAX_SIZE	= 256,

	DTSEC_RM_POOL_FI_LOW_MARK	= 16,
	DTSEC_RM_POOL_FI_HIGH_MARK	= 64,
	DTSEC_RM_POOL_FI_MAX_SIZE	= 256,
};

#define	DTSEC_RM_FQR_RX_CHANNEL		0x401
#define	DTSEC_RM_FQR_TX_CONF_CHANNEL	0
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

	params.dflt_fqid = sc->sc_rx_fqid;
	params.err_fqid = sc->sc_rx_fqid;
	params.rx_params.num_pools = 1;
	params.rx_params.bpools[0].bpid = bman_get_bpid(sc->sc_rx_pool);
	params.rx_params.bpools[0].size = MCLBYTES;
	error = FMAN_PORT_CONFIG(sc->sc_rx_port, &params);
	error = FMAN_PORT_INIT(sc->sc_rx_port);
	if (error != 0) {
		device_printf(sc->sc_dev, "couldn't initialize FM Port RX.\n");
		return (ENXIO);
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
			pa = pmap_kextract((vm_offset_t)b);
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
	if (bman_count(sc->sc_rx_pool) <= DTSEC_RM_POOL_RX_MAX_SIZE)
		bman_put_buffer(sc->sc_rx_pool,
		    pmap_kextract((vm_offset_t)buffer), sc->sc_rx_bpid);
	else
		dpaa_eth_pool_rx_put_buffer(sc, buffer, NULL);
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
	struct dpaa_eth_softc *sc;
	struct mbuf *m;
	struct fman_internal_context *frame_ic;
	void *frame_va;

	m = NULL;
	sc = app;

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

	if_input(sc->sc_ifnet, m);

	return (1);

err:
	bman_put_buffer(sc->sc_rx_pool, frame->addr, sc->sc_rx_bpid);
	if (m != NULL)
		m_freem(m);

	return (1);
}

static int
dpaa_eth_fq_tx_confirm_callback(device_t portal, struct qman_fq *fq,
    struct qman_fd *frame, void *app)
{
	struct dpaa_eth_frame_info *fi;
	struct dpaa_eth_softc *sc;
	unsigned int qlen;
	struct dpaa_sgte *sgt0;

	sc = app;

	if ((frame->cmd_stat & DPAA_FD_TX_STAT_ERR_M) != 0)
		device_printf(sc->sc_dev, "TX error: 0x%08X\n",
		    frame->cmd_stat);

	/*
	 * We are storing struct dpaa_eth_frame_info in first entry
	 * of scatter-gather table.
	 */
	sgt0 = (struct dpaa_sgte *)PHYS_TO_DMAP(frame->addr + frame->offset);
	fi = (struct dpaa_eth_frame_info *)PHYS_TO_DMAP(sgt0->addr);

	/* Free transmitted frame */
	m_freem(fi->fi_mbuf);
	dpaa_eth_fi_free(sc, fi);

	qlen = qman_fq_get_counter(sc->sc_tx_conf_fq, QMAN_COUNTER_FRAME);

	if (qlen == 0) {
		DPAA_ETH_LOCK(sc);

		if (sc->sc_tx_fq_full) {
			sc->sc_tx_fq_full = 0;
			dpaa_eth_if_start_locked(sc);
		}

		DPAA_ETH_UNLOCK(sc);
	}

	return (1);
}

void
dpaa_eth_fq_rx_free(struct dpaa_eth_softc *sc)
{
	int cpu;

	if (sc->sc_rx_fq)
		qman_fq_free(sc->sc_rx_fq);
	if (sc->sc_rx_channel != 0) {
		CPU_FOREACH(cpu) {
			device_t portal = DPCPU_ID_GET(cpu, qman_affine_portal);
			QMAN_PORTAL_STATIC_DEQUEUE_RM_CHANNEL(portal,
			    sc->sc_rx_channel);
		}
		qman_free_channel(sc->sc_rx_channel);
	}
}

int
dpaa_eth_fq_rx_init(struct dpaa_eth_softc *sc)
{
	void *fq;
	int error;
	int cpu;

	/* Default Frame Queue */
	if (sc->sc_rx_channel == 0)
		sc->sc_rx_channel = qman_alloc_channel();
	fq = qman_fq_create(1, sc->sc_rx_channel, DTSEC_RM_FQR_RX_WQ,
	    false, 0, false, false, true, false, 0, 0, 0);
	if (fq == NULL) {
		device_printf(sc->sc_dev,
		    "could not create default RX queue\n");
		return (EIO);
	}

	CPU_FOREACH(cpu) {
		device_t portal = DPCPU_ID_GET(cpu, qman_affine_portal);
		QMAN_PORTAL_STATIC_DEQUEUE_CHANNEL(portal, sc->sc_rx_channel);
	}

	sc->sc_rx_fq = fq;
	sc->sc_rx_fqid = qman_fq_get_fqid(fq);

	error = qman_fq_register_cb(fq, dpaa_eth_fq_rx_callback, sc);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not register RX callback\n");
		dpaa_eth_fq_rx_free(sc);
		return (EIO);
	}

	return (0);
}

void
dpaa_eth_fq_tx_free(struct dpaa_eth_softc *sc)
{

	if (sc->sc_tx_fq)
		qman_fq_free(sc->sc_tx_fq);

	if (sc->sc_tx_conf_fq)
		qman_fq_free(sc->sc_tx_conf_fq);
}

int
dpaa_eth_fq_tx_init(struct dpaa_eth_softc *sc)
{
	int error;
	void *fq;

	/* TX Frame Queue */
	fq = qman_fq_create(1, sc->sc_port_tx_qman_chan,
	    DTSEC_RM_FQR_TX_WQ, false, 0, false, false, true, false, 0, 0, 0);
	if (fq == NULL) {
		device_printf(sc->sc_dev, "could not create default TX queue"
		    "\n");
		return (EIO);
	}

	sc->sc_tx_fq = fq;

	if (sc->sc_rx_channel == 0)
		sc->sc_rx_channel = qman_alloc_channel();
	/* TX Confirmation Frame Queue */
	fq = qman_fq_create(1, sc->sc_rx_channel,
	    DTSEC_RM_FQR_TX_CONF_WQ, false, 0, false, false, true, false, 0, 0,
	    0);
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
	vm_size_t dsize, psize, ssize;
	struct dpaa_eth_frame_info *fi;
	unsigned int qlen, i;
	struct mbuf *m0, *m;
	vm_offset_t vaddr;
	struct dpaa_fd fd;

	DPAA_ETH_LOCK_ASSERT(sc);
	/* TODO: IFF_DRV_OACTIVE */

	if ((sc->sc_mii->mii_media_status & IFM_ACTIVE) == 0)
		return;

	if ((if_getdrvflags(sc->sc_ifnet) & IFF_DRV_RUNNING) != IFF_DRV_RUNNING)
		return;

	while (!if_sendq_empty(sc->sc_ifnet)) {
		/* Check length of the TX queue */
		qlen = qman_fq_get_counter(sc->sc_tx_fq, QMAN_COUNTER_FRAME);

		if (qlen >= DTSEC_MAX_TX_QUEUE_LEN) {
			sc->sc_tx_fq_full = 1;
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
		m = m0;
		psize = 0;
		dsize = 0;
		fi->fi_mbuf = m0;
		while (m && i < DPAA_NUM_OF_SG_TABLE_ENTRY) {
			if (m->m_len == 0)
				continue;

			/*
			 * First entry in scatter-gather table is used to keep
			 * pointer to frame info structure.
			 */
			fi->fi_sgt[i].addr = pmap_kextract((vm_offset_t)fi);
			i++;

			dsize = m->m_len;
			vaddr = (vm_offset_t)m->m_data;
			while (dsize > 0 && i < DPAA_NUM_OF_SG_TABLE_ENTRY) {
				ssize = PAGE_SIZE - (vaddr & PAGE_MASK);
				if (m->m_len < ssize)
					ssize = m->m_len;

				fi->fi_sgt[i].addr = pmap_kextract(vaddr);
				fi->fi_sgt[i].length = ssize;

				fi->fi_sgt[i].extension = 0;
				fi->fi_sgt[i].final = 0;
				fi->fi_sgt[i].bpid = 0;
				fi->fi_sgt[i].offset = 0;

				dsize -= ssize;
				vaddr += ssize;
				psize += ssize;
				i++;
			}

			if (dsize > 0)
				break;

			m = m->m_next;
		}

		/* Check if SG table was constructed properly */
		if (m != NULL || dsize != 0) {
			dpaa_eth_fi_free(sc, fi);
			m_freem(m0);
			continue;
		}

		fi->fi_sgt[i - 1].final = 1;

		fd.addr = pmap_kextract((vm_offset_t)&fi->fi_ic);
		fd.length = psize;
		fd.format = DPAA_FD_FORMAT_SHORT_MBSF;

		fd.liodn = 0;
		fd.bpid = 0;
		fd.eliodn = 0;
		fd.offset = offsetof(struct dpaa_eth_frame_info, fi_sgt) -
		    offsetof(struct dpaa_eth_frame_info, fi_ic);
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
