/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#include "opt_rss.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/endian.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/smp.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/if_vlan_var.h>
#include <net/iflib.h>
#ifdef RSS
#include <net/rss_config.h>
#endif

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include "ifdi_if.h"
#include "enic.h"

#include "opt_inet.h"
#include "opt_inet6.h"

static int enic_isc_txd_encap(void *, if_pkt_info_t);
static void enic_isc_txd_flush(void *, uint16_t, qidx_t);
static int enic_isc_txd_credits_update(void *, uint16_t, bool);
static int enic_isc_rxd_available(void *, uint16_t, qidx_t, qidx_t);
static int enic_isc_rxd_pkt_get(void *, if_rxd_info_t);
static void enic_isc_rxd_refill(void *, if_rxd_update_t);
static void enic_isc_rxd_flush(void *, uint16_t, uint8_t, qidx_t);
static int enic_legacy_intr(void *);
static void enic_initial_post_rx(struct enic *, struct vnic_rq *);
static int enic_wq_service(struct vnic_dev *, struct cq_desc *, u8, u16, u16,
    void *);
static int enic_rq_service(struct vnic_dev *, struct cq_desc *, u8, u16, u16,
    void *);

struct if_txrx	enic_txrx = {
	.ift_txd_encap = enic_isc_txd_encap,
	.ift_txd_flush = enic_isc_txd_flush,
	.ift_txd_credits_update = enic_isc_txd_credits_update,
	.ift_rxd_available = enic_isc_rxd_available,
	.ift_rxd_pkt_get = enic_isc_rxd_pkt_get,
	.ift_rxd_refill = enic_isc_rxd_refill,
	.ift_rxd_flush = enic_isc_rxd_flush,
	.ift_legacy_intr = enic_legacy_intr
};

static int
enic_isc_txd_encap(void *vsc, if_pkt_info_t pi)
{
	struct enic_softc *softc;
	struct enic *enic;
	struct vnic_wq *wq;
	int nsegs;
	int i;

	struct wq_enet_desc *desc;
	uint64_t bus_addr;
	uint16_t mss = 7;
	uint16_t header_len = 0;
	uint8_t offload_mode = 0;
	uint8_t eop = 0, cq;
	uint8_t vlan_tag_insert = 0;
	unsigned short vlan_id = 0;

	unsigned int wq_desc_avail;
	int head_idx;
	unsigned int desc_count, data_len;

	softc = vsc;
	enic = &softc->enic;
	if_softc_ctx_t scctx = softc->scctx;

	wq = &enic->wq[pi->ipi_qsidx];
	nsegs = pi->ipi_nsegs;

	ENIC_LOCK(softc);
	wq_desc_avail = vnic_wq_desc_avail(wq);
	head_idx = wq->head_idx;
	desc_count = wq->ring.desc_count;

	if ((scctx->isc_capenable & IFCAP_RXCSUM) != 0)
		offload_mode |= WQ_ENET_OFFLOAD_MODE_CSUM;

	for (i = 0; i < nsegs; i++) {
		eop = 0;
		cq = 0;
		wq->cq_pend++;
		if (i + 1 == nsegs) {
			eop = 1;
			cq = 1;
			wq->cq_pend = 0;
		}
		desc = wq->ring.descs;
		bus_addr = pi->ipi_segs[i].ds_addr;
		data_len = pi->ipi_segs[i].ds_len;

		wq_enet_desc_enc(&desc[head_idx], bus_addr, data_len, mss,
				 header_len, offload_mode, eop, cq, 0,
				 vlan_tag_insert, vlan_id, 0);

		head_idx = enic_ring_incr(desc_count, head_idx);
		wq_desc_avail--;
	}

	wq->ring.desc_avail = wq_desc_avail;
	wq->head_idx = head_idx;

	pi->ipi_new_pidx = head_idx;
	ENIC_UNLOCK(softc);

	return (0);
}

static void
enic_isc_txd_flush(void *vsc, uint16_t txqid, qidx_t pidx)
{
	struct enic_softc *softc;
	struct enic *enic;
	struct vnic_wq *wq;
	int head_idx;

	softc = vsc;
	enic = &softc->enic;

	ENIC_LOCK(softc);
	wq = &enic->wq[txqid];
	head_idx = wq->head_idx;

	ENIC_BUS_WRITE_4(wq->ctrl, TX_POSTED_INDEX, head_idx);
	ENIC_UNLOCK(softc);
}

static int
enic_isc_txd_credits_update(void *vsc, uint16_t txqid, bool clear)
{

	struct enic_softc *softc;
	struct enic *enic;
	struct vnic_wq *wq;
	struct vnic_cq *cq;
	int processed;
	unsigned int cq_wq;
	unsigned int wq_work_to_do = 10;
	unsigned int wq_work_avail;

	softc = vsc;
	enic = &softc->enic;
	wq = &softc->enic.wq[txqid];

	cq_wq = enic_cq_wq(enic, txqid);
	cq = &enic->cq[cq_wq];

	ENIC_LOCK(softc);
	wq_work_avail = vnic_cq_work(cq, wq_work_to_do);
	ENIC_UNLOCK(softc);

	if (wq_work_avail == 0)
		return (0);

	if (!clear)
		return (1);

	ENIC_LOCK(softc);
	vnic_cq_service(cq, wq_work_to_do,
		    enic_wq_service, NULL);

	processed = wq->processed;
	wq->processed = 0;

	ENIC_UNLOCK(softc);

	return (processed);
}

static int
enic_isc_rxd_available(void *vsc, uint16_t rxqid, qidx_t idx, qidx_t budget)
{
	struct enic_softc *softc;
	struct enic *enic;
	struct vnic_cq *cq;
	unsigned int rq_work_to_do = budget;
	unsigned int rq_work_avail = 0;
	unsigned int cq_rq;

	softc = vsc;
	enic = &softc->enic;

	cq_rq = enic_cq_rq(&softc->enic, rxqid);
	cq = &enic->cq[cq_rq];

	rq_work_avail = vnic_cq_work(cq, rq_work_to_do);
	return rq_work_avail;
}

static int
enic_isc_rxd_pkt_get(void *vsc, if_rxd_info_t ri)
{
	struct enic_softc *softc;
	struct enic *enic;
	struct vnic_cq *cq;
	unsigned int rq_work_to_do = 1;
	unsigned int rq_work_done = 0;
	unsigned int cq_rq;

	softc = vsc;
	enic = &softc->enic;

	cq_rq = enic_cq_rq(&softc->enic, ri->iri_qsidx);
	cq = &enic->cq[cq_rq];
	ENIC_LOCK(softc);
	rq_work_done = vnic_cq_service(cq, rq_work_to_do, enic_rq_service, ri);

	if (rq_work_done != 0) {
		vnic_intr_return_credits(&enic->intr[cq_rq], rq_work_done, 0,
		    1);
		ENIC_UNLOCK(softc);
		return (0);
	} else {
		ENIC_UNLOCK(softc);
		return (-1);
	}

}

static void
enic_isc_rxd_refill(void *vsc, if_rxd_update_t iru)
{
	struct enic_softc *softc;
	struct vnic_rq *rq;
	struct rq_enet_desc *rqd;

	uint64_t *paddrs;
	int count;
	uint32_t pidx;
	int len;
	int idx;
	int i;

	count = iru->iru_count;
	len = iru->iru_buf_size;
	paddrs = iru->iru_paddrs;
	pidx = iru->iru_pidx;

	softc = vsc;
	rq = &softc->enic.rq[iru->iru_qsidx];
	rqd = rq->ring.descs;

	idx = pidx;
	for (i = 0; i < count; i++, idx++) {

		if (idx == rq->ring.desc_count)
			idx = 0;
		rq_enet_desc_enc(&rqd[idx], paddrs[i],
				 RQ_ENET_TYPE_ONLY_SOP,
				 len);

	}

	rq->in_use = 1;

	if (rq->need_initial_post) {
		ENIC_BUS_WRITE_4(rq->ctrl, RX_FETCH_INDEX, 0);
	}

	enic_initial_post_rx(&softc->enic, rq);
}

static void
enic_isc_rxd_flush(void *vsc, uint16_t rxqid, uint8_t flid, qidx_t pidx)
{

	struct enic_softc *softc;
	struct vnic_rq *rq;

	softc = vsc;
	rq = &softc->enic.rq[rxqid];

	/*
	 * pidx is the index of the last descriptor with a buffer the device
	 * can use, and the device needs to be told which index is one past
	 * that.
	 */

	ENIC_LOCK(softc);
	ENIC_BUS_WRITE_4(rq->ctrl, RX_POSTED_INDEX, pidx);
	ENIC_UNLOCK(softc);
}

static int
enic_legacy_intr(void *xsc)
{
	return (1);
}

static inline void
vnic_wq_service(struct vnic_wq *wq, struct cq_desc *cq_desc,
    u16 completed_index, void (*buf_service) (struct vnic_wq *wq,
    struct cq_desc *cq_desc, /* struct vnic_wq_buf * *buf, */ void *opaque),
    void *opaque)
{
	int processed;

	processed = completed_index - wq->ring.last_count;
	if (processed < 0)
		processed += wq->ring.desc_count;
	if (processed == 0)
		processed++;

	wq->ring.desc_avail += processed;
	wq->processed += processed;
	wq->ring.last_count = completed_index;
}

/*
 * Post the Rx buffers for the first time. enic_alloc_rx_queue_mbufs() has
 * allocated the buffers and filled the RQ descriptor ring. Just need to push
 * the post index to the NIC.
 */
static void
enic_initial_post_rx(struct enic *enic, struct vnic_rq *rq)
{
	struct enic_softc *softc = enic->softc;
	if (!rq->in_use || !rq->need_initial_post)
		return;

	ENIC_LOCK(softc);
	/* make sure all prior writes are complete before doing the PIO write */
	/* Post all but the last buffer to VIC. */
	rq->posted_index = rq->ring.desc_count - 1;

	rq->rx_nb_hold = 0;

	ENIC_BUS_WRITE_4(rq->ctrl, RX_POSTED_INDEX, rq->posted_index);

	rq->need_initial_post = false;
	ENIC_UNLOCK(softc);
}

static int
enic_wq_service(struct vnic_dev *vdev, struct cq_desc *cq_desc, u8 type,
    u16 q_number, u16 completed_index, void *opaque)
{
	struct enic *enic = vnic_dev_priv(vdev);

	vnic_wq_service(&enic->wq[q_number], cq_desc,
			completed_index, NULL, opaque);
	return (0);
}

static void
vnic_rq_service(struct vnic_rq *rq, struct cq_desc *cq_desc,
    u16 in_completed_index, int desc_return,
    void(*buf_service)(struct vnic_rq *rq, struct cq_desc *cq_desc,
    /* struct vnic_rq_buf * *buf, */ int skipped, void *opaque), void *opaque)
{
	if_softc_ctx_t scctx;
	if_rxd_info_t ri = (if_rxd_info_t) opaque;
	u8 type, color, eop, sop, ingress_port, vlan_stripped;
	u8 fcoe, fcoe_sof, fcoe_fc_crc_ok, fcoe_enc_error, fcoe_eof;
	u8 tcp_udp_csum_ok, udp, tcp, ipv4_csum_ok;
	u8 ipv6, ipv4, ipv4_fragment, fcs_ok, rss_type, csum_not_calc;
	u8 packet_error;
	u16 q_number, completed_index, bytes_written, vlan_tci, checksum;
	u32 rss_hash;
	int cqidx;
	if_rxd_frag_t frag;

	scctx = rq->vdev->softc->scctx;

	cq_enet_rq_desc_dec((struct cq_enet_rq_desc *)cq_desc,
	    &type, &color, &q_number, &completed_index,
	    &ingress_port, &fcoe, &eop, &sop, &rss_type,
	    &csum_not_calc, &rss_hash, &bytes_written,
	    &packet_error, &vlan_stripped, &vlan_tci, &checksum,
	    &fcoe_sof, &fcoe_fc_crc_ok, &fcoe_enc_error,
	    &fcoe_eof, &tcp_udp_csum_ok, &udp, &tcp,
	    &ipv4_csum_ok, &ipv6, &ipv4, &ipv4_fragment,
	    &fcs_ok);

	cqidx = ri->iri_cidx;

	frag = &ri->iri_frags[0];
	frag->irf_idx = cqidx;
	frag->irf_len = bytes_written;

	if (++cqidx == rq->ring.desc_count) {
		cqidx = 0;
	}

	ri->iri_cidx = cqidx;
	ri->iri_nfrags = 1;
	ri->iri_len = bytes_written;

	if ((scctx->isc_capenable & IFCAP_RXCSUM) != 0)
		if (!csum_not_calc && (tcp_udp_csum_ok || ipv4_csum_ok)) {
			ri->iri_csum_flags = (CSUM_IP_CHECKED | CSUM_IP_VALID);
		}
}

static int
enic_rq_service(struct vnic_dev *vdev, struct cq_desc *cq_desc,
		u8 type, u16 q_number, u16 completed_index, void *opaque)
{
	struct enic *enic = vnic_dev_priv(vdev);
	if_rxd_info_t ri = (if_rxd_info_t) opaque;

	vnic_rq_service(&enic->rq[ri->iri_qsidx], cq_desc, completed_index,
	    VNIC_RQ_RETURN_DESC, NULL, /* enic_rq_indicate_buf, */ opaque);

	return (0);
}

void
enic_prep_wq_for_simple_tx(struct enic *enic, uint16_t queue_idx)
{
	struct wq_enet_desc *desc;
	struct vnic_wq *wq;
	unsigned int i;

	/*
	 * Fill WQ descriptor fields that never change. Every descriptor is
	 * one packet, so set EOP. Also set CQ_ENTRY every ENIC_WQ_CQ_THRESH
	 * descriptors (i.e. request one completion update every 32 packets).
	 */
	wq = &enic->wq[queue_idx];
	desc = (struct wq_enet_desc *)wq->ring.descs;
	for (i = 0; i < wq->ring.desc_count; i++, desc++) {
		desc->header_length_flags = 1 << WQ_ENET_FLAGS_EOP_SHIFT;
		if (i % ENIC_WQ_CQ_THRESH == ENIC_WQ_CQ_THRESH - 1)
			desc->header_length_flags |=
			    (1 << WQ_ENET_FLAGS_CQ_ENTRY_SHIFT);
	}
}

void
enic_start_wq(struct enic *enic, uint16_t queue_idx)
{
	vnic_wq_enable(&enic->wq[queue_idx]);
}

int
enic_stop_wq(struct enic *enic, uint16_t queue_idx)
{
	int ret;

	ret = vnic_wq_disable(&enic->wq[queue_idx]);

	return (ret);
}

void
enic_start_rq(struct enic *enic, uint16_t queue_idx)
{
	struct vnic_rq *rq;

	rq = &enic->rq[queue_idx];
	vnic_rq_enable(rq);
	enic_initial_post_rx(enic, rq);
}

int
enic_stop_rq(struct enic *enic, uint16_t queue_idx)
{
	int ret;

	ret = vnic_rq_disable(&enic->rq[queue_idx]);

	return (ret);
}


void
enic_dev_disable(struct enic *enic) {
	vnic_dev_disable(enic->vdev);
}
