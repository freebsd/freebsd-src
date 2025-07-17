/*-
 * Copyright (c) 2014 Chelsio Communications, Inc.
 * All rights reserved.
 * Written by: Navdeep Parhar <np@FreeBSD.org>
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

#include <sys/cdefs.h>
#include "opt_inet.h"
#include "opt_inet6.h"

#ifdef DEV_NETMAP
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/eventhandler.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/selinfo.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <machine/bus.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_var.h>
#include <net/if_clone.h>
#include <net/if_types.h>
#include <net/netmap.h>
#include <dev/netmap/netmap_kern.h>

#include "common/common.h"
#include "common/t4_regs.h"
#include "common/t4_regs_values.h"

extern int fl_pad;	/* XXXNM */

/*
 * 0 = normal netmap rx
 * 1 = black hole
 * 2 = supermassive black hole (buffer packing enabled)
 */
int black_hole = 0;
SYSCTL_INT(_hw_cxgbe, OID_AUTO, nm_black_hole, CTLFLAG_RWTUN, &black_hole, 0,
    "Sink incoming packets.");

int rx_ndesc = 256;
SYSCTL_INT(_hw_cxgbe, OID_AUTO, nm_rx_ndesc, CTLFLAG_RWTUN,
    &rx_ndesc, 0, "# of rx descriptors after which the hw cidx is updated.");

int rx_nframes = 64;
SYSCTL_INT(_hw_cxgbe, OID_AUTO, nm_rx_nframes, CTLFLAG_RWTUN,
    &rx_nframes, 0, "max # of frames received before waking up netmap rx.");

int holdoff_tmr_idx = 2;
SYSCTL_INT(_hw_cxgbe, OID_AUTO, nm_holdoff_tmr_idx, CTLFLAG_RWTUN,
    &holdoff_tmr_idx, 0, "Holdoff timer index for netmap rx queues.");

/*
 * Congestion drops.
 * -1: no congestion feedback (not recommended).
 *  0: backpressure the channel instead of dropping packets right away.
 *  1: no backpressure, drop packets for the congested queue immediately.
 */
static int nm_cong_drop = 1;
SYSCTL_INT(_hw_cxgbe, OID_AUTO, nm_cong_drop, CTLFLAG_RWTUN,
    &nm_cong_drop, 0,
    "Congestion control for netmap rx queues (0 = backpressure, 1 = drop");

int starve_fl = 0;
SYSCTL_INT(_hw_cxgbe, OID_AUTO, starve_fl, CTLFLAG_RWTUN,
    &starve_fl, 0, "Don't ring fl db for netmap rx queues.");

/*
 * Try to process tx credits in bulk.  This may cause a delay in the return of
 * tx credits and is suitable for bursty or non-stop tx only.
 */
int lazy_tx_credit_flush = 1;
SYSCTL_INT(_hw_cxgbe, OID_AUTO, lazy_tx_credit_flush, CTLFLAG_RWTUN,
    &lazy_tx_credit_flush, 0, "lazy credit flush for netmap tx queues.");

/*
 * Split the netmap rx queues into two groups that populate separate halves of
 * the RSS indirection table.  This allows filters with hashmask to steer to a
 * particular group of queues.
 */
static int nm_split_rss = 0;
SYSCTL_INT(_hw_cxgbe, OID_AUTO, nm_split_rss, CTLFLAG_RWTUN,
    &nm_split_rss, 0, "Split the netmap rx queues into two groups.");

/*
 * netmap(4) says "netmap does not use features such as checksum offloading, TCP
 * segmentation offloading, encryption, VLAN encapsulation/decapsulation, etc."
 * but this knob can be used to get the hardware to checksum all tx traffic
 * anyway.
 */
static int nm_txcsum = 0;
SYSCTL_INT(_hw_cxgbe, OID_AUTO, nm_txcsum, CTLFLAG_RWTUN,
    &nm_txcsum, 0, "Enable transmit checksum offloading.");

static int free_nm_rxq_hwq(struct vi_info *, struct sge_nm_rxq *);
static int free_nm_txq_hwq(struct vi_info *, struct sge_nm_txq *);

int
alloc_nm_rxq(struct vi_info *vi, struct sge_nm_rxq *nm_rxq, int intr_idx,
    int idx)
{
	int rc;
	struct sysctl_oid *oid;
	struct sysctl_oid_list *children;
	struct sysctl_ctx_list *ctx;
	char name[16];
	size_t len;
	struct adapter *sc = vi->adapter;
	struct netmap_adapter *na = NA(vi->ifp);

	MPASS(na != NULL);

	len = vi->qsize_rxq * IQ_ESIZE;
	rc = alloc_ring(sc, len, &nm_rxq->iq_desc_tag, &nm_rxq->iq_desc_map,
	    &nm_rxq->iq_ba, (void **)&nm_rxq->iq_desc);
	if (rc != 0)
		return (rc);

	len = na->num_rx_desc * EQ_ESIZE + sc->params.sge.spg_len;
	rc = alloc_ring(sc, len, &nm_rxq->fl_desc_tag, &nm_rxq->fl_desc_map,
	    &nm_rxq->fl_ba, (void **)&nm_rxq->fl_desc);
	if (rc != 0)
		return (rc);

	nm_rxq->vi = vi;
	nm_rxq->nid = idx;
	nm_rxq->iq_cidx = 0;
	nm_rxq->iq_sidx = vi->qsize_rxq - sc->params.sge.spg_len / IQ_ESIZE;
	nm_rxq->iq_gen = F_RSPD_GEN;
	nm_rxq->fl_pidx = nm_rxq->fl_cidx = 0;
	nm_rxq->fl_sidx = na->num_rx_desc;
	nm_rxq->fl_sidx2 = nm_rxq->fl_sidx;	/* copy for rxsync cacheline */
	nm_rxq->intr_idx = intr_idx;
	nm_rxq->iq_cntxt_id = INVALID_NM_RXQ_CNTXT_ID;

	ctx = &vi->ctx;
	children = SYSCTL_CHILDREN(vi->nm_rxq_oid);

	snprintf(name, sizeof(name), "%d", idx);
	oid = SYSCTL_ADD_NODE(ctx, children, OID_AUTO, name,
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "rx queue");
	children = SYSCTL_CHILDREN(oid);

	SYSCTL_ADD_U16(ctx, children, OID_AUTO, "abs_id", CTLFLAG_RD,
	    &nm_rxq->iq_abs_id, 0, "absolute id of the queue");
	SYSCTL_ADD_U16(ctx, children, OID_AUTO, "cntxt_id", CTLFLAG_RD,
	    &nm_rxq->iq_cntxt_id, 0, "SGE context id of the queue");
	SYSCTL_ADD_U16(ctx, children, OID_AUTO, "cidx", CTLFLAG_RD,
	    &nm_rxq->iq_cidx, 0, "consumer index");

	children = SYSCTL_CHILDREN(oid);
	oid = SYSCTL_ADD_NODE(ctx, children, OID_AUTO, "fl",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "freelist");
	children = SYSCTL_CHILDREN(oid);

	SYSCTL_ADD_U16(ctx, children, OID_AUTO, "cntxt_id", CTLFLAG_RD,
	    &nm_rxq->fl_cntxt_id, 0, "SGE context id of the freelist");
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "cidx", CTLFLAG_RD,
	    &nm_rxq->fl_cidx, 0, "consumer index");
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "pidx", CTLFLAG_RD,
	    &nm_rxq->fl_pidx, 0, "producer index");

	return (rc);
}

int
free_nm_rxq(struct vi_info *vi, struct sge_nm_rxq *nm_rxq)
{
	struct adapter *sc = vi->adapter;

	if (!(vi->flags & VI_INIT_DONE))
		return (0);

	if (nm_rxq->iq_cntxt_id != INVALID_NM_RXQ_CNTXT_ID)
		free_nm_rxq_hwq(vi, nm_rxq);
	MPASS(nm_rxq->iq_cntxt_id == INVALID_NM_RXQ_CNTXT_ID);

	free_ring(sc, nm_rxq->iq_desc_tag, nm_rxq->iq_desc_map, nm_rxq->iq_ba,
	    nm_rxq->iq_desc);
	free_ring(sc, nm_rxq->fl_desc_tag, nm_rxq->fl_desc_map, nm_rxq->fl_ba,
	    nm_rxq->fl_desc);

	return (0);
}

int
alloc_nm_txq(struct vi_info *vi, struct sge_nm_txq *nm_txq, int iqidx, int idx)
{
	int rc;
	size_t len;
	struct port_info *pi = vi->pi;
	struct adapter *sc = pi->adapter;
	struct netmap_adapter *na = NA(vi->ifp);
	char name[16];
	struct sysctl_oid *oid;
	struct sysctl_oid_list *children = SYSCTL_CHILDREN(vi->nm_txq_oid);

	len = na->num_tx_desc * EQ_ESIZE + sc->params.sge.spg_len;
	rc = alloc_ring(sc, len, &nm_txq->desc_tag, &nm_txq->desc_map,
	    &nm_txq->ba, (void **)&nm_txq->desc);
	if (rc)
		return (rc);

	nm_txq->pidx = nm_txq->cidx = 0;
	nm_txq->sidx = na->num_tx_desc;
	nm_txq->nid = idx;
	nm_txq->iqidx = iqidx;
	nm_txq->cpl_ctrl0 = htobe32(V_TXPKT_OPCODE(CPL_TX_PKT) |
	    V_TXPKT_INTF(pi->tx_chan) | V_TXPKT_PF(sc->pf) |
	    V_TXPKT_VF(vi->vin) | V_TXPKT_VF_VLD(vi->vfvld));
	if (sc->params.fw_vers >= FW_VERSION32(1, 24, 11, 0))
		nm_txq->op_pkd = htobe32(V_FW_WR_OP(FW_ETH_TX_PKTS2_WR));
	else
		nm_txq->op_pkd = htobe32(V_FW_WR_OP(FW_ETH_TX_PKTS_WR));
	nm_txq->cntxt_id = INVALID_NM_TXQ_CNTXT_ID;

	snprintf(name, sizeof(name), "%d", idx);
	oid = SYSCTL_ADD_NODE(&vi->ctx, children, OID_AUTO, name,
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "netmap tx queue");
	children = SYSCTL_CHILDREN(oid);

	SYSCTL_ADD_UINT(&vi->ctx, children, OID_AUTO, "cntxt_id", CTLFLAG_RD,
	    &nm_txq->cntxt_id, 0, "SGE context id of the queue");
	SYSCTL_ADD_U16(&vi->ctx, children, OID_AUTO, "cidx", CTLFLAG_RD,
	    &nm_txq->cidx, 0, "consumer index");
	SYSCTL_ADD_U16(&vi->ctx, children, OID_AUTO, "pidx", CTLFLAG_RD,
	    &nm_txq->pidx, 0, "producer index");

	return (rc);
}

int
free_nm_txq(struct vi_info *vi, struct sge_nm_txq *nm_txq)
{
	struct adapter *sc = vi->adapter;

	if (!(vi->flags & VI_INIT_DONE))
		return (0);

	if (nm_txq->cntxt_id != INVALID_NM_TXQ_CNTXT_ID)
		free_nm_txq_hwq(vi, nm_txq);
	MPASS(nm_txq->cntxt_id == INVALID_NM_TXQ_CNTXT_ID);

	free_ring(sc, nm_txq->desc_tag, nm_txq->desc_map, nm_txq->ba,
	    nm_txq->desc);

	return (0);
}

static int
alloc_nm_rxq_hwq(struct vi_info *vi, struct sge_nm_rxq *nm_rxq)
{
	int rc, cntxt_id;
	__be32 v;
	struct adapter *sc = vi->adapter;
	struct port_info *pi = vi->pi;
	struct sge_params *sp = &sc->params.sge;
	struct netmap_adapter *na = NA(vi->ifp);
	struct fw_iq_cmd c;
	const int cong_drop = nm_cong_drop;
	const int cong_map = pi->rx_e_chan_map;

	MPASS(na != NULL);
	MPASS(nm_rxq->iq_desc != NULL);
	MPASS(nm_rxq->fl_desc != NULL);

	bzero(nm_rxq->iq_desc, vi->qsize_rxq * IQ_ESIZE);
	bzero(nm_rxq->fl_desc, na->num_rx_desc * EQ_ESIZE + sp->spg_len);

	bzero(&c, sizeof(c));
	c.op_to_vfn = htobe32(V_FW_CMD_OP(FW_IQ_CMD) | F_FW_CMD_REQUEST |
	    F_FW_CMD_WRITE | F_FW_CMD_EXEC | V_FW_IQ_CMD_PFN(sc->pf) |
	    V_FW_IQ_CMD_VFN(0));
	c.alloc_to_len16 = htobe32(F_FW_IQ_CMD_IQSTART | FW_LEN16(c));
	if (nm_rxq->iq_cntxt_id == INVALID_NM_RXQ_CNTXT_ID)
		c.alloc_to_len16 |= htobe32(F_FW_IQ_CMD_ALLOC);
	else {
		c.iqid = htobe16(nm_rxq->iq_cntxt_id);
		c.fl0id = htobe16(nm_rxq->fl_cntxt_id);
		c.fl1id = htobe16(0xffff);
		c.physiqid = htobe16(nm_rxq->iq_abs_id);
	}
	MPASS(!forwarding_intr_to_fwq(sc));
	KASSERT(nm_rxq->intr_idx < sc->intr_count,
	    ("%s: invalid direct intr_idx %d", __func__, nm_rxq->intr_idx));
	v = V_FW_IQ_CMD_IQANDSTINDEX(nm_rxq->intr_idx);
	c.type_to_iqandstindex = htobe32(v |
	    V_FW_IQ_CMD_TYPE(FW_IQ_TYPE_FL_INT_CAP) |
	    V_FW_IQ_CMD_VIID(vi->viid) |
	    V_FW_IQ_CMD_IQANUD(X_UPDATEDELIVERY_INTERRUPT));
	c.iqdroprss_to_iqesize = htobe16(V_FW_IQ_CMD_IQPCIECH(pi->tx_chan) |
	    F_FW_IQ_CMD_IQGTSMODE |
	    V_FW_IQ_CMD_IQINTCNTTHRESH(0) |
	    V_FW_IQ_CMD_IQESIZE(ilog2(IQ_ESIZE) - 4));
	c.iqsize = htobe16(vi->qsize_rxq);
	c.iqaddr = htobe64(nm_rxq->iq_ba);
	if (cong_drop != -1) {
		c.iqns_to_fl0congen = htobe32(F_FW_IQ_CMD_IQFLINTCONGEN |
		    V_FW_IQ_CMD_FL0CNGCHMAP(cong_map) | F_FW_IQ_CMD_FL0CONGCIF |
		    F_FW_IQ_CMD_FL0CONGEN);
	}
	c.iqns_to_fl0congen |=
	    htobe32(V_FW_IQ_CMD_FL0HOSTFCMODE(X_HOSTFCMODE_NONE) |
		V_FW_IQ_CMD_IQTYPE(FW_IQ_IQTYPE_NIC) |
		F_FW_IQ_CMD_FL0FETCHRO | F_FW_IQ_CMD_FL0DATARO |
		(fl_pad ? F_FW_IQ_CMD_FL0PADEN : 0) |
		(black_hole == 2 ? F_FW_IQ_CMD_FL0PACKEN : 0));
	c.fl0dcaen_to_fl0cidxfthresh =
	    htobe16(V_FW_IQ_CMD_FL0FBMIN(chip_id(sc) <= CHELSIO_T5 ?
		X_FETCHBURSTMIN_128B : X_FETCHBURSTMIN_64B_T6) |
		V_FW_IQ_CMD_FL0FBMAX(chip_id(sc) <= CHELSIO_T5 ?
		X_FETCHBURSTMAX_512B : X_FETCHBURSTMAX_256B));
	c.fl0size = htobe16(na->num_rx_desc / 8 + sp->spg_len / EQ_ESIZE);
	c.fl0addr = htobe64(nm_rxq->fl_ba);

	rc = -t4_wr_mbox(sc, sc->mbox, &c, sizeof(c), &c);
	if (rc != 0) {
		device_printf(sc->dev,
		    "failed to create netmap ingress queue: %d\n", rc);
		return (rc);
	}

	nm_rxq->iq_cidx = 0;
	MPASS(nm_rxq->iq_sidx == vi->qsize_rxq - sp->spg_len / IQ_ESIZE);
	nm_rxq->iq_gen = F_RSPD_GEN;
	nm_rxq->iq_cntxt_id = be16toh(c.iqid);
	nm_rxq->iq_abs_id = be16toh(c.physiqid);
	cntxt_id = nm_rxq->iq_cntxt_id - sc->sge.iq_start;
	if (cntxt_id >= sc->sge.iqmap_sz) {
		panic ("%s: nm_rxq->iq_cntxt_id (%d) more than the max (%d)",
		    __func__, cntxt_id, sc->sge.iqmap_sz - 1);
	}
	sc->sge.iqmap[cntxt_id] = (void *)nm_rxq;

	nm_rxq->fl_cntxt_id = be16toh(c.fl0id);
	nm_rxq->fl_pidx = nm_rxq->fl_cidx = 0;
	nm_rxq->fl_db_saved = 0;
	/* matches the X_FETCHBURSTMAX_512B or X_FETCHBURSTMAX_256B above. */
	nm_rxq->fl_db_threshold = chip_id(sc) <= CHELSIO_T5 ? 8 : 4;
	MPASS(nm_rxq->fl_sidx == na->num_rx_desc);
	cntxt_id = nm_rxq->fl_cntxt_id - sc->sge.eq_start;
	if (cntxt_id >= sc->sge.eqmap_sz) {
		panic("%s: nm_rxq->fl_cntxt_id (%d) more than the max (%d)",
		    __func__, cntxt_id, sc->sge.eqmap_sz - 1);
	}
	sc->sge.eqmap[cntxt_id] = (void *)nm_rxq;

	nm_rxq->fl_db_val = V_QID(nm_rxq->fl_cntxt_id) |
	    sc->chip_params->sge_fl_db;

	if (chip_id(sc) >= CHELSIO_T5 && cong_drop != -1) {
		t4_sge_set_conm_context(sc, nm_rxq->iq_cntxt_id, cong_drop,
		    cong_map);
	}

	t4_write_reg(sc, sc->sge_gts_reg,
	    V_INGRESSQID(nm_rxq->iq_cntxt_id) |
	    V_SEINTARM(V_QINTR_TIMER_IDX(holdoff_tmr_idx)));

	return (rc);
}

static int
free_nm_rxq_hwq(struct vi_info *vi, struct sge_nm_rxq *nm_rxq)
{
	struct adapter *sc = vi->adapter;
	int rc;

	rc = -t4_iq_free(sc, sc->mbox, sc->pf, 0, FW_IQ_TYPE_FL_INT_CAP,
	    nm_rxq->iq_cntxt_id, nm_rxq->fl_cntxt_id, 0xffff);
	if (rc != 0)
		device_printf(sc->dev, "%s: failed for iq %d, fl %d: %d\n",
		    __func__, nm_rxq->iq_cntxt_id, nm_rxq->fl_cntxt_id, rc);
	nm_rxq->iq_cntxt_id = INVALID_NM_RXQ_CNTXT_ID;
	return (rc);
}

static int
alloc_nm_txq_hwq(struct vi_info *vi, struct sge_nm_txq *nm_txq)
{
	int rc, cntxt_id;
	size_t len;
	struct adapter *sc = vi->adapter;
	struct netmap_adapter *na = NA(vi->ifp);
	struct fw_eq_eth_cmd c;

	MPASS(na != NULL);
	MPASS(nm_txq->desc != NULL);

	len = na->num_tx_desc * EQ_ESIZE + sc->params.sge.spg_len;
	bzero(nm_txq->desc, len);

	bzero(&c, sizeof(c));
	c.op_to_vfn = htobe32(V_FW_CMD_OP(FW_EQ_ETH_CMD) | F_FW_CMD_REQUEST |
	    F_FW_CMD_WRITE | F_FW_CMD_EXEC | V_FW_EQ_ETH_CMD_PFN(sc->pf) |
	    V_FW_EQ_ETH_CMD_VFN(0));
	c.alloc_to_len16 = htobe32(F_FW_EQ_ETH_CMD_EQSTART | FW_LEN16(c));
	if (nm_txq->cntxt_id == INVALID_NM_TXQ_CNTXT_ID)
		c.alloc_to_len16 |= htobe32(F_FW_EQ_ETH_CMD_ALLOC);
	else
		c.eqid_pkd = htobe32(V_FW_EQ_ETH_CMD_EQID(nm_txq->cntxt_id));
	c.autoequiqe_to_viid = htobe32(F_FW_EQ_ETH_CMD_AUTOEQUIQE |
	    F_FW_EQ_ETH_CMD_AUTOEQUEQE | V_FW_EQ_ETH_CMD_VIID(vi->viid));
	c.fetchszm_to_iqid =
	    htobe32(V_FW_EQ_ETH_CMD_HOSTFCMODE(X_HOSTFCMODE_NONE) |
		V_FW_EQ_ETH_CMD_PCIECHN(vi->pi->tx_chan) | F_FW_EQ_ETH_CMD_FETCHRO |
		V_FW_EQ_ETH_CMD_IQID(sc->sge.nm_rxq[nm_txq->iqidx].iq_cntxt_id));
	c.dcaen_to_eqsize =
	    htobe32(V_FW_EQ_ETH_CMD_FBMIN(chip_id(sc) <= CHELSIO_T5 ?
		X_FETCHBURSTMIN_64B : X_FETCHBURSTMIN_64B_T6) |
		V_FW_EQ_ETH_CMD_FBMAX(X_FETCHBURSTMAX_512B) |
		V_FW_EQ_ETH_CMD_EQSIZE(len / EQ_ESIZE));
	c.eqaddr = htobe64(nm_txq->ba);

	rc = -t4_wr_mbox(sc, sc->mbox, &c, sizeof(c), &c);
	if (rc != 0) {
		device_printf(vi->dev,
		    "failed to create netmap egress queue: %d\n", rc);
		return (rc);
	}

	nm_txq->cntxt_id = G_FW_EQ_ETH_CMD_EQID(be32toh(c.eqid_pkd));
	cntxt_id = nm_txq->cntxt_id - sc->sge.eq_start;
	if (cntxt_id >= sc->sge.eqmap_sz)
	    panic("%s: nm_txq->cntxt_id (%d) more than the max (%d)", __func__,
		cntxt_id, sc->sge.eqmap_sz - 1);
	sc->sge.eqmap[cntxt_id] = (void *)nm_txq;

	nm_txq->pidx = nm_txq->cidx = 0;
	MPASS(nm_txq->sidx == na->num_tx_desc);
	nm_txq->equiqidx = nm_txq->equeqidx = nm_txq->dbidx = 0;

	nm_txq->doorbells = sc->doorbells;
	if (isset(&nm_txq->doorbells, DOORBELL_UDB) ||
	    isset(&nm_txq->doorbells, DOORBELL_UDBWC) ||
	    isset(&nm_txq->doorbells, DOORBELL_WCWR)) {
		uint32_t s_qpp = sc->params.sge.eq_s_qpp;
		uint32_t mask = (1 << s_qpp) - 1;
		volatile uint8_t *udb;

		udb = sc->udbs_base + UDBS_DB_OFFSET;
		udb += (nm_txq->cntxt_id >> s_qpp) << PAGE_SHIFT;
		nm_txq->udb_qid = nm_txq->cntxt_id & mask;
		if (nm_txq->udb_qid >= PAGE_SIZE / UDBS_SEG_SIZE)
			clrbit(&nm_txq->doorbells, DOORBELL_WCWR);
		else {
			udb += nm_txq->udb_qid << UDBS_SEG_SHIFT;
			nm_txq->udb_qid = 0;
		}
		nm_txq->udb = (volatile void *)udb;
	}

	if (sc->params.fw_vers < FW_VERSION32(1, 25, 1, 0)) {
		uint32_t param, val;

		param = V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DMAQ) |
		    V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DMAQ_EQ_SCHEDCLASS_ETH) |
		    V_FW_PARAMS_PARAM_YZ(nm_txq->cntxt_id);
		val = 0xff;
		rc = -t4_set_params(sc, sc->mbox, sc->pf, 0, 1, &param, &val);
		if (rc != 0) {
			device_printf(vi->dev,
			    "failed to bind netmap txq %d to class 0xff: %d\n",
			    nm_txq->cntxt_id, rc);
			rc = 0;
		}
	}

	return (rc);
}

static int
free_nm_txq_hwq(struct vi_info *vi, struct sge_nm_txq *nm_txq)
{
	struct adapter *sc = vi->adapter;
	int rc;

	rc = -t4_eth_eq_free(sc, sc->mbox, sc->pf, 0, nm_txq->cntxt_id);
	if (rc != 0)
		device_printf(sc->dev, "%s: failed for eq %d: %d\n", __func__,
		    nm_txq->cntxt_id, rc);
	nm_txq->cntxt_id = INVALID_NM_TXQ_CNTXT_ID;
	return (rc);
}

static int
cxgbe_netmap_simple_rss(struct adapter *sc, struct vi_info *vi,
    if_t ifp, struct netmap_adapter *na)
{
	struct netmap_kring *kring;
	struct sge_nm_rxq *nm_rxq;
	int rc, i, j, nm_state, defq;
	uint16_t *rss;

	/*
	 * Check if there's at least one active (or about to go active) netmap
	 * rx queue.
	 */
	defq = -1;
	for_each_nm_rxq(vi, j, nm_rxq) {
		nm_state = atomic_load_int(&nm_rxq->nm_state);
		kring = na->rx_rings[nm_rxq->nid];
		if ((nm_state != NM_OFF && !nm_kring_pending_off(kring)) ||
		    (nm_state == NM_OFF && nm_kring_pending_on(kring))) {
			MPASS(nm_rxq->iq_cntxt_id != INVALID_NM_RXQ_CNTXT_ID);
			if (defq == -1) {
				defq = nm_rxq->iq_abs_id;
				break;
			}
		}
	}

	if (defq == -1) {
		/* No active netmap queues.  Switch back to NIC queues. */
		rss = vi->rss;
		defq = vi->rss[0];
	} else {
		for (i = 0; i < vi->rss_size;) {
			for_each_nm_rxq(vi, j, nm_rxq) {
				nm_state = atomic_load_int(&nm_rxq->nm_state);
				kring = na->rx_rings[nm_rxq->nid];
				if ((nm_state != NM_OFF &&
				    !nm_kring_pending_off(kring)) ||
				    (nm_state == NM_OFF &&
				    nm_kring_pending_on(kring))) {
					MPASS(nm_rxq->iq_cntxt_id !=
					    INVALID_NM_RXQ_CNTXT_ID);
					vi->nm_rss[i++] = nm_rxq->iq_abs_id;
					if (i == vi->rss_size)
						break;
				}
			}
		}
		rss = vi->nm_rss;
	}

	rc = -t4_config_rss_range(sc, sc->mbox, vi->viid, 0, vi->rss_size, rss,
	    vi->rss_size);
	if (rc != 0)
		if_printf(ifp, "netmap rss_config failed: %d\n", rc);

	rc = -t4_config_vi_rss(sc, sc->mbox, vi->viid, vi->hashen, defq, 0, 0);
	if (rc != 0) {
		if_printf(ifp, "netmap defaultq config failed: %d\n", rc);
	}

	return (rc);
}

/*
 * Odd number of rx queues work best for split RSS mode as the first queue can
 * be dedicated for non-RSS traffic and the rest divided into two equal halves.
 */
static int
cxgbe_netmap_split_rss(struct adapter *sc, struct vi_info *vi,
    if_t ifp, struct netmap_adapter *na)
{
	struct netmap_kring *kring;
	struct sge_nm_rxq *nm_rxq;
	int rc, i, j, nm_state, defq;
	int nactive[2] = {0, 0};
	int dq[2] = {-1, -1};
	bool dq_norss;		/* default queue should not be in RSS table. */

	MPASS(nm_split_rss != 0);
	MPASS(vi->nnmrxq > 1);

	for_each_nm_rxq(vi, i, nm_rxq) {
		j = i / ((vi->nnmrxq + 1) / 2);
		nm_state = atomic_load_int(&nm_rxq->nm_state);
		kring = na->rx_rings[nm_rxq->nid];
		if ((nm_state != NM_OFF && !nm_kring_pending_off(kring)) ||
		    (nm_state == NM_OFF && nm_kring_pending_on(kring))) {
			MPASS(nm_rxq->iq_cntxt_id != INVALID_NM_RXQ_CNTXT_ID);
			nactive[j]++;
			if (dq[j] == -1) {
				dq[j] = nm_rxq->iq_abs_id;
				break;
			}
		}
	}

	if (nactive[0] == 0 || nactive[1] == 0)
		return (cxgbe_netmap_simple_rss(sc, vi, ifp, na));

	MPASS(dq[0] != -1 && dq[1] != -1);
	if (nactive[0] > nactive[1]) {
		defq = dq[0];
		dq_norss = true;
	} else if (nactive[0] < nactive[1]) {
		defq = dq[1];
		dq_norss = true;
	} else {
		defq = dq[0];
		dq_norss = false;
	}

	i = 0;
	nm_rxq = &sc->sge.nm_rxq[vi->first_nm_rxq];
	while (i < vi->rss_size / 2) {
		for (j = 0; j < (vi->nnmrxq + 1) / 2; j++) {
			nm_state = atomic_load_int(&nm_rxq[j].nm_state);
			kring = na->rx_rings[nm_rxq[j].nid];
			if ((nm_state == NM_OFF &&
			    !nm_kring_pending_on(kring)) ||
			    (nm_state == NM_ON &&
			    nm_kring_pending_off(kring))) {
				continue;
			}
			MPASS(nm_rxq[j].iq_cntxt_id != INVALID_NM_RXQ_CNTXT_ID);
			if (dq_norss && defq == nm_rxq[j].iq_abs_id)
				continue;
			vi->nm_rss[i++] = nm_rxq[j].iq_abs_id;
			if (i == vi->rss_size / 2)
				break;
		}
	}
	while (i < vi->rss_size) {
		for (j = (vi->nnmrxq + 1) / 2; j < vi->nnmrxq; j++) {
			nm_state = atomic_load_int(&nm_rxq[j].nm_state);
			kring = na->rx_rings[nm_rxq[j].nid];
			if ((nm_state == NM_OFF &&
			    !nm_kring_pending_on(kring)) ||
			    (nm_state == NM_ON &&
			    nm_kring_pending_off(kring))) {
				continue;
			}
			MPASS(nm_rxq[j].iq_cntxt_id != INVALID_NM_RXQ_CNTXT_ID);
			if (dq_norss && defq == nm_rxq[j].iq_abs_id)
				continue;
			vi->nm_rss[i++] = nm_rxq[j].iq_abs_id;
			if (i == vi->rss_size)
				break;
		}
	}

	rc = -t4_config_rss_range(sc, sc->mbox, vi->viid, 0, vi->rss_size,
	    vi->nm_rss, vi->rss_size);
	if (rc != 0)
		if_printf(ifp, "netmap split_rss_config failed: %d\n", rc);

	rc = -t4_config_vi_rss(sc, sc->mbox, vi->viid, vi->hashen, defq, 0, 0);
	if (rc != 0)
		if_printf(ifp, "netmap defaultq config failed: %d\n", rc);

	return (rc);
}

static inline int
cxgbe_netmap_rss(struct adapter *sc, struct vi_info *vi, if_t ifp,
    struct netmap_adapter *na)
{

	if (nm_split_rss == 0 || vi->nnmrxq == 1)
		return (cxgbe_netmap_simple_rss(sc, vi, ifp, na));
	else
		return (cxgbe_netmap_split_rss(sc, vi, ifp, na));
}

static int
cxgbe_netmap_on(struct adapter *sc, struct vi_info *vi, if_t ifp,
    struct netmap_adapter *na)
{
	struct netmap_slot *slot;
	struct netmap_kring *kring;
	struct sge_nm_rxq *nm_rxq;
	struct sge_nm_txq *nm_txq;
	int i, j, hwidx;
	struct rx_buf_info *rxb;

	ASSERT_SYNCHRONIZED_OP(sc);
	MPASS(vi->nnmrxq > 0);
	MPASS(vi->nnmtxq > 0);

	if ((vi->flags & VI_INIT_DONE) == 0 ||
	    (if_getdrvflags(ifp) & IFF_DRV_RUNNING) == 0) {
		if_printf(ifp, "cannot enable netmap operation because "
		    "interface is not UP.\n");
		return (EAGAIN);
	}

	rxb = &sc->sge.rx_buf_info[0];
	for (i = 0; i < SW_ZONE_SIZES; i++, rxb++) {
		if (rxb->size1 == NETMAP_BUF_SIZE(na)) {
			hwidx = rxb->hwidx1;
			break;
		}
		if (rxb->size2 == NETMAP_BUF_SIZE(na)) {
			hwidx = rxb->hwidx2;
			break;
		}
	}
	if (i >= SW_ZONE_SIZES) {
		if_printf(ifp, "no hwidx for netmap buffer size %d.\n",
		    NETMAP_BUF_SIZE(na));
		return (ENXIO);
	}

	/* Must set caps before calling netmap_reset */
	nm_set_native_flags(na);

	for_each_nm_rxq(vi, i, nm_rxq) {
		kring = na->rx_rings[nm_rxq->nid];
		if (!nm_kring_pending_on(kring))
			continue;

		alloc_nm_rxq_hwq(vi, nm_rxq);
		nm_rxq->fl_hwidx = hwidx;
		slot = netmap_reset(na, NR_RX, i, 0);
		MPASS(slot != NULL);	/* XXXNM: error check, not assert */

		/* We deal with 8 bufs at a time */
		MPASS((na->num_rx_desc & 7) == 0);
		MPASS(na->num_rx_desc == nm_rxq->fl_sidx);
		for (j = 0; j < nm_rxq->fl_sidx; j++) {
			uint64_t ba;

			PNMB(na, &slot[j], &ba);
			MPASS(ba != 0);
			nm_rxq->fl_desc[j] = htobe64(ba | hwidx);
		}
		j = nm_rxq->fl_pidx = nm_rxq->fl_sidx - 8;
		MPASS((j & 7) == 0);
		j /= 8;	/* driver pidx to hardware pidx */
		wmb();
		t4_write_reg(sc, sc->sge_kdoorbell_reg,
		    nm_rxq->fl_db_val | V_PIDX(j));

		(void) atomic_cmpset_int(&nm_rxq->nm_state, NM_OFF, NM_ON);
	}

	for_each_nm_txq(vi, i, nm_txq) {
		kring = na->tx_rings[nm_txq->nid];
		if (!nm_kring_pending_on(kring))
			continue;

		alloc_nm_txq_hwq(vi, nm_txq);
		slot = netmap_reset(na, NR_TX, i, 0);
		MPASS(slot != NULL);	/* XXXNM: error check, not assert */
	}

	if (vi->nm_rss == NULL) {
		vi->nm_rss = malloc(vi->rss_size * sizeof(uint16_t), M_CXGBE,
		    M_ZERO | M_WAITOK);
	}

	return (cxgbe_netmap_rss(sc, vi, ifp, na));
}

static int
cxgbe_netmap_off(struct adapter *sc, struct vi_info *vi, if_t ifp,
    struct netmap_adapter *na)
{
	struct netmap_kring *kring;
	int rc, i, nm_state, nactive;
	struct sge_nm_txq *nm_txq;
	struct sge_nm_rxq *nm_rxq;

	ASSERT_SYNCHRONIZED_OP(sc);
	MPASS(vi->nnmrxq > 0);
	MPASS(vi->nnmtxq > 0);

	if (!nm_netmap_on(na))
		return (0);

	if ((vi->flags & VI_INIT_DONE) == 0)
		return (0);

	/* First remove the queues that are stopping from the RSS table. */
	rc = cxgbe_netmap_rss(sc, vi, ifp, na);
	if (rc != 0)
		return (rc);	/* error message logged already. */

	for_each_nm_txq(vi, i, nm_txq) {
		kring = na->tx_rings[nm_txq->nid];
		if (!nm_kring_pending_off(kring))
			continue;
		MPASS(nm_txq->cntxt_id != INVALID_NM_TXQ_CNTXT_ID);

		rc = -t4_eth_eq_stop(sc, sc->mbox, sc->pf, 0, nm_txq->cntxt_id);
		if (rc != 0) {
			device_printf(vi->dev,
			    "failed to stop nm_txq[%d]: %d.\n", i, rc);
			return (rc);
		}

		/* XXX: netmap, not the driver, should do this. */
		kring->rhead = kring->rcur = kring->nr_hwcur = 0;
		kring->rtail = kring->nr_hwtail = kring->nkr_num_slots - 1;
	}
	nactive = 0;
	for_each_nm_rxq(vi, i, nm_rxq) {
		nm_state = atomic_load_int(&nm_rxq->nm_state);
		kring = na->rx_rings[nm_rxq->nid];
		if (nm_state != NM_OFF && !nm_kring_pending_off(kring))
			nactive++;
		if (!nm_kring_pending_off(kring))
			continue;
		MPASS(nm_state != NM_OFF);
		MPASS(nm_rxq->iq_cntxt_id != INVALID_NM_RXQ_CNTXT_ID);

		rc = -t4_iq_stop(sc, sc->mbox, sc->pf, 0, FW_IQ_TYPE_FL_INT_CAP,
		    nm_rxq->iq_cntxt_id, nm_rxq->fl_cntxt_id, 0xffff);
		if (rc != 0) {
			device_printf(vi->dev,
			    "failed to stop nm_rxq[%d]: %d.\n", i, rc);
			return (rc);
		}

		while (!atomic_cmpset_int(&nm_rxq->nm_state, NM_ON, NM_OFF))
			pause("nmst", 1);

		/* XXX: netmap, not the driver, should do this. */
		kring->rhead = kring->rcur = kring->nr_hwcur = 0;
		kring->rtail = kring->nr_hwtail = 0;
	}
	netmap_krings_mode_commit(na, 0);
	if (nactive == 0)
		nm_clear_native_flags(na);

	return (rc);
}

static int
cxgbe_netmap_reg(struct netmap_adapter *na, int on)
{
	if_t ifp = na->ifp;
	struct vi_info *vi = if_getsoftc(ifp);
	struct adapter *sc = vi->adapter;
	int rc;

	rc = begin_synchronized_op(sc, vi, SLEEP_OK | INTR_OK, "t4nmreg");
	if (rc != 0)
		return (rc);
	if (on)
		rc = cxgbe_netmap_on(sc, vi, ifp, na);
	else
		rc = cxgbe_netmap_off(sc, vi, ifp, na);
	end_synchronized_op(sc, 0);

	return (rc);
}

/* How many packets can a single type1 WR carry in n descriptors */
static inline int
ndesc_to_npkt(const int n)
{

	MPASS(n > 0 && n <= SGE_MAX_WR_NDESC);

	return (n * 2 - 1);
}
#define MAX_NPKT_IN_TYPE1_WR	(ndesc_to_npkt(SGE_MAX_WR_NDESC))

/*
 * Space (in descriptors) needed for a type1 WR (TX_PKTS or TX_PKTS2) that
 * carries n packets
 */
static inline int
npkt_to_ndesc(const int n)
{

	MPASS(n > 0 && n <= MAX_NPKT_IN_TYPE1_WR);

	return ((n + 2) / 2);
}

/*
 * Space (in 16B units) needed for a type1 WR (TX_PKTS or TX_PKTS2) that
 * carries n packets
 */
static inline int
npkt_to_len16(const int n)
{

	MPASS(n > 0 && n <= MAX_NPKT_IN_TYPE1_WR);

	return (n * 2 + 1);
}

#define NMIDXDIFF(q, idx) IDXDIFF((q)->pidx, (q)->idx, (q)->sidx)

static void
ring_nm_txq_db(struct adapter *sc, struct sge_nm_txq *nm_txq)
{
	int n;
	u_int db = nm_txq->doorbells;

	MPASS(nm_txq->pidx != nm_txq->dbidx);

	n = NMIDXDIFF(nm_txq, dbidx);
	if (n > 1)
		clrbit(&db, DOORBELL_WCWR);
	wmb();

	switch (ffs(db) - 1) {
	case DOORBELL_UDB:
		*nm_txq->udb = htole32(V_QID(nm_txq->udb_qid) | V_PIDX(n));
		break;

	case DOORBELL_WCWR: {
		volatile uint64_t *dst, *src;

		/*
		 * Queues whose 128B doorbell segment fits in the page do not
		 * use relative qid (udb_qid is always 0).  Only queues with
		 * doorbell segments can do WCWR.
		 */
		KASSERT(nm_txq->udb_qid == 0 && n == 1,
		    ("%s: inappropriate doorbell (0x%x, %d, %d) for nm_txq %p",
		    __func__, nm_txq->doorbells, n, nm_txq->pidx, nm_txq));

		dst = (volatile void *)((uintptr_t)nm_txq->udb +
		    UDBS_WR_OFFSET - UDBS_DB_OFFSET);
		src = (void *)&nm_txq->desc[nm_txq->dbidx];
		while (src != (void *)&nm_txq->desc[nm_txq->dbidx + 1])
			*dst++ = *src++;
		wmb();
		break;
	}

	case DOORBELL_UDBWC:
		*nm_txq->udb = htole32(V_QID(nm_txq->udb_qid) | V_PIDX(n));
		wmb();
		break;

	case DOORBELL_KDB:
		t4_write_reg(sc, sc->sge_kdoorbell_reg,
		    V_QID(nm_txq->cntxt_id) | V_PIDX(n));
		break;
	}
	nm_txq->dbidx = nm_txq->pidx;
}

/*
 * Write work requests to send 'npkt' frames and ring the doorbell to send them
 * on their way.  No need to check for wraparound.
 */
static void
cxgbe_nm_tx(struct adapter *sc, struct sge_nm_txq *nm_txq,
    struct netmap_kring *kring, int npkt, int npkt_remaining)
{
	struct netmap_ring *ring = kring->ring;
	struct netmap_slot *slot;
	const u_int lim = kring->nkr_num_slots - 1;
	struct fw_eth_tx_pkts_wr *wr = (void *)&nm_txq->desc[nm_txq->pidx];
	uint16_t len;
	uint64_t ba;
	struct cpl_tx_pkt_core *cpl;
	struct ulptx_sgl *usgl;
	int i, n;

	while (npkt) {
		n = min(npkt, MAX_NPKT_IN_TYPE1_WR);
		len = 0;

		wr = (void *)&nm_txq->desc[nm_txq->pidx];
		wr->op_pkd = nm_txq->op_pkd;
		wr->equiq_to_len16 = htobe32(V_FW_WR_LEN16(npkt_to_len16(n)));
		wr->npkt = n;
		wr->r3 = 0;
		wr->type = 1;
		cpl = (void *)(wr + 1);

		for (i = 0; i < n; i++) {
			slot = &ring->slot[kring->nr_hwcur];
			PNMB(kring->na, slot, &ba);
			MPASS(ba != 0);

			cpl->ctrl0 = nm_txq->cpl_ctrl0;
			cpl->pack = 0;
			cpl->len = htobe16(slot->len);
			cpl->ctrl1 = nm_txcsum ? 0 :
			    htobe64(F_TXPKT_IPCSUM_DIS | F_TXPKT_L4CSUM_DIS);

			usgl = (void *)(cpl + 1);
			usgl->cmd_nsge = htobe32(V_ULPTX_CMD(ULP_TX_SC_DSGL) |
			    V_ULPTX_NSGE(1));
			usgl->len0 = htobe32(slot->len);
			usgl->addr0 = htobe64(ba + nm_get_offset(kring, slot));

			slot->flags &= ~(NS_REPORT | NS_BUF_CHANGED);
			cpl = (void *)(usgl + 1);
			MPASS(slot->len + len <= UINT16_MAX);
			len += slot->len;
			kring->nr_hwcur = nm_next(kring->nr_hwcur, lim);
		}
		wr->plen = htobe16(len);

		npkt -= n;
		nm_txq->pidx += npkt_to_ndesc(n);
		MPASS(nm_txq->pidx <= nm_txq->sidx);
		if (__predict_false(nm_txq->pidx == nm_txq->sidx)) {
			/*
			 * This routine doesn't know how to write WRs that wrap
			 * around.  Make sure it wasn't asked to.
			 */
			MPASS(npkt == 0);
			nm_txq->pidx = 0;
		}

		if (npkt + npkt_remaining == 0) {
			/* All done. */
			if (lazy_tx_credit_flush == 0 ||
			    NMIDXDIFF(nm_txq, equiqidx) >= nm_txq->sidx / 2) {
				wr->equiq_to_len16 |= htobe32(F_FW_WR_EQUEQ |
				    F_FW_WR_EQUIQ);
				nm_txq->equeqidx = nm_txq->pidx;
				nm_txq->equiqidx = nm_txq->pidx;
			} else if (NMIDXDIFF(nm_txq, equeqidx) >= 64) {
				wr->equiq_to_len16 |= htobe32(F_FW_WR_EQUEQ);
				nm_txq->equeqidx = nm_txq->pidx;
			}
			ring_nm_txq_db(sc, nm_txq);
			return;
		}
		if (NMIDXDIFF(nm_txq, dbidx) >= 2 * SGE_MAX_WR_NDESC) {
			if (NMIDXDIFF(nm_txq, equeqidx) >= 64) {
				wr->equiq_to_len16 |= htobe32(F_FW_WR_EQUEQ);
				nm_txq->equeqidx = nm_txq->pidx;
			}
			ring_nm_txq_db(sc, nm_txq);
		}
	}

	/* Will get called again. */
	MPASS(npkt_remaining);
}

/* How many contiguous free descriptors starting at pidx */
static inline int
contiguous_ndesc_available(struct sge_nm_txq *nm_txq)
{

	if (nm_txq->cidx > nm_txq->pidx)
		return (nm_txq->cidx - nm_txq->pidx - 1);
	else if (nm_txq->cidx > 0)
		return (nm_txq->sidx - nm_txq->pidx);
	else
		return (nm_txq->sidx - nm_txq->pidx - 1);
}

static int
reclaim_nm_tx_desc(struct sge_nm_txq *nm_txq)
{
	struct sge_qstat *spg = (void *)&nm_txq->desc[nm_txq->sidx];
	uint16_t hw_cidx = spg->cidx;	/* snapshot */
	struct fw_eth_tx_pkts_wr *wr;
	int n = 0;

	hw_cidx = be16toh(hw_cidx);

	while (nm_txq->cidx != hw_cidx) {
		wr = (void *)&nm_txq->desc[nm_txq->cidx];

		MPASS(wr->op_pkd == htobe32(V_FW_WR_OP(FW_ETH_TX_PKTS_WR)) ||
		    wr->op_pkd == htobe32(V_FW_WR_OP(FW_ETH_TX_PKTS2_WR)));
		MPASS(wr->type == 1);
		MPASS(wr->npkt > 0 && wr->npkt <= MAX_NPKT_IN_TYPE1_WR);

		n += wr->npkt;
		nm_txq->cidx += npkt_to_ndesc(wr->npkt);

		/*
		 * We never sent a WR that wrapped around so the credits coming
		 * back, WR by WR, should never cause the cidx to wrap around
		 * either.
		 */
		MPASS(nm_txq->cidx <= nm_txq->sidx);
		if (__predict_false(nm_txq->cidx == nm_txq->sidx))
			nm_txq->cidx = 0;
	}

	return (n);
}

static int
cxgbe_netmap_txsync(struct netmap_kring *kring, int flags)
{
	struct netmap_adapter *na = kring->na;
	if_t ifp = na->ifp;
	struct vi_info *vi = if_getsoftc(ifp);
	struct adapter *sc = vi->adapter;
	struct sge_nm_txq *nm_txq = &sc->sge.nm_txq[vi->first_nm_txq + kring->ring_id];
	const u_int head = kring->rhead;
	u_int reclaimed = 0;
	int n, d, npkt_remaining, ndesc_remaining;

	/*
	 * Tx was at kring->nr_hwcur last time around and now we need to advance
	 * to kring->rhead.  Note that the driver's pidx moves independent of
	 * netmap's kring->nr_hwcur (pidx counts descriptors and the relation
	 * between descriptors and frames isn't 1:1).
	 */

	npkt_remaining = head >= kring->nr_hwcur ? head - kring->nr_hwcur :
	    kring->nkr_num_slots - kring->nr_hwcur + head;
	while (npkt_remaining) {
		reclaimed += reclaim_nm_tx_desc(nm_txq);
		ndesc_remaining = contiguous_ndesc_available(nm_txq);
		/* Can't run out of descriptors with packets still remaining */
		MPASS(ndesc_remaining > 0);

		/* # of desc needed to tx all remaining packets */
		d = (npkt_remaining / MAX_NPKT_IN_TYPE1_WR) * SGE_MAX_WR_NDESC;
		if (npkt_remaining % MAX_NPKT_IN_TYPE1_WR)
			d += npkt_to_ndesc(npkt_remaining % MAX_NPKT_IN_TYPE1_WR);

		if (d <= ndesc_remaining)
			n = npkt_remaining;
		else {
			/* Can't send all, calculate how many can be sent */
			n = (ndesc_remaining / SGE_MAX_WR_NDESC) *
			    MAX_NPKT_IN_TYPE1_WR;
			if (ndesc_remaining % SGE_MAX_WR_NDESC)
				n += ndesc_to_npkt(ndesc_remaining % SGE_MAX_WR_NDESC);
		}

		/* Send n packets and update nm_txq->pidx and kring->nr_hwcur */
		npkt_remaining -= n;
		cxgbe_nm_tx(sc, nm_txq, kring, n, npkt_remaining);
	}
	MPASS(npkt_remaining == 0);
	MPASS(kring->nr_hwcur == head);
	MPASS(nm_txq->dbidx == nm_txq->pidx);

	/*
	 * Second part: reclaim buffers for completed transmissions.
	 */
	if (reclaimed || flags & NAF_FORCE_RECLAIM || nm_kr_txempty(kring)) {
		reclaimed += reclaim_nm_tx_desc(nm_txq);
		kring->nr_hwtail += reclaimed;
		if (kring->nr_hwtail >= kring->nkr_num_slots)
			kring->nr_hwtail -= kring->nkr_num_slots;
	}

	return (0);
}

static int
cxgbe_netmap_rxsync(struct netmap_kring *kring, int flags)
{
	struct netmap_adapter *na = kring->na;
	struct netmap_ring *ring = kring->ring;
	if_t ifp = na->ifp;
	struct vi_info *vi = if_getsoftc(ifp);
	struct adapter *sc = vi->adapter;
	struct sge_nm_rxq *nm_rxq = &sc->sge.nm_rxq[vi->first_nm_rxq + kring->ring_id];
	u_int const head = kring->rhead;
	u_int n;
	int force_update = (flags & NAF_FORCE_READ) || kring->nr_kflags & NKR_PENDINTR;

	if (black_hole)
		return (0);	/* No updates ever. */

	if (netmap_no_pendintr || force_update) {
		kring->nr_hwtail = atomic_load_acq_32(&nm_rxq->fl_cidx);
		kring->nr_kflags &= ~NKR_PENDINTR;
	}

	if (nm_rxq->fl_db_saved > 0 && starve_fl == 0) {
		wmb();
		t4_write_reg(sc, sc->sge_kdoorbell_reg,
		    nm_rxq->fl_db_val | V_PIDX(nm_rxq->fl_db_saved));
		nm_rxq->fl_db_saved = 0;
	}

	/* Userspace done with buffers from kring->nr_hwcur to head */
	n = head >= kring->nr_hwcur ? head - kring->nr_hwcur :
	    kring->nkr_num_slots - kring->nr_hwcur + head;
	n &= ~7U;
	if (n > 0) {
		u_int fl_pidx = nm_rxq->fl_pidx;
		struct netmap_slot *slot = &ring->slot[fl_pidx];
		uint64_t ba;
		int i, dbinc = 0, hwidx = nm_rxq->fl_hwidx;

		/*
		 * We always deal with 8 buffers at a time.  We must have
		 * stopped at an 8B boundary (fl_pidx) last time around and we
		 * must have a multiple of 8B buffers to give to the freelist.
		 */
		MPASS((fl_pidx & 7) == 0);
		MPASS((n & 7) == 0);

		IDXINCR(kring->nr_hwcur, n, kring->nkr_num_slots);
		IDXINCR(nm_rxq->fl_pidx, n, nm_rxq->fl_sidx2);

		while (n > 0) {
			for (i = 0; i < 8; i++, fl_pidx++, slot++) {
				PNMB(na, slot, &ba);
				MPASS(ba != 0);
				nm_rxq->fl_desc[fl_pidx] = htobe64(ba | hwidx);
				slot->flags &= ~NS_BUF_CHANGED;
				MPASS(fl_pidx <= nm_rxq->fl_sidx2);
			}
			n -= 8;
			if (fl_pidx == nm_rxq->fl_sidx2) {
				fl_pidx = 0;
				slot = &ring->slot[0];
			}
			if (++dbinc == nm_rxq->fl_db_threshold) {
				wmb();
				if (starve_fl)
					nm_rxq->fl_db_saved += dbinc;
				else {
					t4_write_reg(sc, sc->sge_kdoorbell_reg,
					    nm_rxq->fl_db_val | V_PIDX(dbinc));
				}
				dbinc = 0;
			}
		}
		MPASS(nm_rxq->fl_pidx == fl_pidx);

		if (dbinc > 0) {
			wmb();
			if (starve_fl)
				nm_rxq->fl_db_saved += dbinc;
			else {
				t4_write_reg(sc, sc->sge_kdoorbell_reg,
				    nm_rxq->fl_db_val | V_PIDX(dbinc));
			}
		}
	}

	return (0);
}

void
cxgbe_nm_attach(struct vi_info *vi)
{
	struct port_info *pi;
	struct adapter *sc;
	struct netmap_adapter na;

	MPASS(vi->nnmrxq > 0);
	MPASS(vi->ifp != NULL);

	pi = vi->pi;
	sc = pi->adapter;

	bzero(&na, sizeof(na));

	na.ifp = vi->ifp;
	na.na_flags = NAF_BDG_MAYSLEEP | NAF_OFFSETS;

	/* Netmap doesn't know about the space reserved for the status page. */
	na.num_tx_desc = vi->qsize_txq - sc->params.sge.spg_len / EQ_ESIZE;

	/*
	 * The freelist's cidx/pidx drives netmap's rx cidx/pidx.  So
	 * num_rx_desc is based on the number of buffers that can be held in the
	 * freelist, and not the number of entries in the iq.  (These two are
	 * not exactly the same due to the space taken up by the status page).
	 */
	na.num_rx_desc = rounddown(vi->qsize_rxq, 8);
	na.nm_txsync = cxgbe_netmap_txsync;
	na.nm_rxsync = cxgbe_netmap_rxsync;
	na.nm_register = cxgbe_netmap_reg;
	na.num_tx_rings = vi->nnmtxq;
	na.num_rx_rings = vi->nnmrxq;
	na.rx_buf_maxsize = MAX_MTU + sc->params.sge.fl_pktshift;
	netmap_attach(&na);	/* This adds IFCAP_NETMAP to if_capabilities */
}

void
cxgbe_nm_detach(struct vi_info *vi)
{

	MPASS(vi->nnmrxq > 0);
	MPASS(vi->ifp != NULL);

	netmap_detach(vi->ifp);
}

static inline const void *
unwrap_nm_fw6_msg(const struct cpl_fw6_msg *cpl)
{

	MPASS(cpl->type == FW_TYPE_RSSCPL || cpl->type == FW6_TYPE_RSSCPL);

	/* data[0] is RSS header */
	return (&cpl->data[1]);
}

static void
handle_nm_sge_egr_update(struct adapter *sc, if_t ifp,
    const struct cpl_sge_egr_update *egr)
{
	uint32_t oq;
	struct sge_nm_txq *nm_txq;

	oq = be32toh(egr->opcode_qid);
	MPASS(G_CPL_OPCODE(oq) == CPL_SGE_EGR_UPDATE);
	nm_txq = (void *)sc->sge.eqmap[G_EGR_QID(oq) - sc->sge.eq_start];

	netmap_tx_irq(ifp, nm_txq->nid);
}

void
service_nm_rxq(struct sge_nm_rxq *nm_rxq)
{
	struct vi_info *vi = nm_rxq->vi;
	struct adapter *sc = vi->adapter;
	if_t ifp = vi->ifp;
	struct netmap_adapter *na = NA(ifp);
	struct netmap_kring *kring = na->rx_rings[nm_rxq->nid];
	struct netmap_ring *ring = kring->ring;
	struct iq_desc *d = &nm_rxq->iq_desc[nm_rxq->iq_cidx];
	const void *cpl;
	uint32_t lq;
	u_int work = 0;
	uint8_t opcode;
	uint32_t fl_cidx = atomic_load_acq_32(&nm_rxq->fl_cidx);
	u_int fl_credits = fl_cidx & 7;
	u_int ndesc = 0;	/* desc processed since last cidx update */
	u_int nframes = 0;	/* frames processed since last netmap wakeup */

	while ((d->rsp.u.type_gen & F_RSPD_GEN) == nm_rxq->iq_gen) {

		rmb();

		lq = be32toh(d->rsp.pldbuflen_qid);
		opcode = d->rss.opcode;
		cpl = &d->cpl[0];

		switch (G_RSPD_TYPE(d->rsp.u.type_gen)) {
		case X_RSPD_TYPE_FLBUF:

			/* fall through */

		case X_RSPD_TYPE_CPL:
			MPASS(opcode < NUM_CPL_CMDS);

			switch (opcode) {
			case CPL_FW4_MSG:
			case CPL_FW6_MSG:
				cpl = unwrap_nm_fw6_msg(cpl);
				/* fall through */
			case CPL_SGE_EGR_UPDATE:
				handle_nm_sge_egr_update(sc, ifp, cpl);
				break;
			case CPL_RX_PKT:
				/*
				 * Note that the application must have netmap
				 * offsets (NETMAP_REQ_OPT_OFFSETS) enabled on
				 * the ring or its rx will not work correctly
				 * when fl_pktshift > 0.
				 */
				nm_write_offset(kring, &ring->slot[fl_cidx],
				    sc->params.sge.fl_pktshift);
				ring->slot[fl_cidx].len = G_RSPD_LEN(lq) -
				    sc->params.sge.fl_pktshift;
				ring->slot[fl_cidx].flags = 0;
				nframes++;
				if (!(lq & F_RSPD_NEWBUF)) {
					MPASS(black_hole == 2);
					break;
				}
				fl_credits++;
				if (__predict_false(++fl_cidx == nm_rxq->fl_sidx))
					fl_cidx = 0;
				break;
			default:
				panic("%s: unexpected opcode 0x%x on nm_rxq %p",
				    __func__, opcode, nm_rxq);
			}
			break;

		case X_RSPD_TYPE_INTR:
			/* Not equipped to handle forwarded interrupts. */
			panic("%s: netmap queue received interrupt for iq %u\n",
			    __func__, lq);

		default:
			panic("%s: illegal response type %d on nm_rxq %p",
			    __func__, G_RSPD_TYPE(d->rsp.u.type_gen), nm_rxq);
		}

		d++;
		if (__predict_false(++nm_rxq->iq_cidx == nm_rxq->iq_sidx)) {
			nm_rxq->iq_cidx = 0;
			d = &nm_rxq->iq_desc[0];
			nm_rxq->iq_gen ^= F_RSPD_GEN;
		}

		if (__predict_false(++nframes == rx_nframes) && !black_hole) {
			atomic_store_rel_32(&nm_rxq->fl_cidx, fl_cidx);
			netmap_rx_irq(ifp, nm_rxq->nid, &work);
			nframes = 0;
		}

		if (__predict_false(++ndesc == rx_ndesc)) {
			if (black_hole && fl_credits >= 8) {
				fl_credits /= 8;
				IDXINCR(nm_rxq->fl_pidx, fl_credits * 8,
				    nm_rxq->fl_sidx);
				t4_write_reg(sc, sc->sge_kdoorbell_reg,
				    nm_rxq->fl_db_val | V_PIDX(fl_credits));
				fl_credits = fl_cidx & 7;
			}
			t4_write_reg(sc, sc->sge_gts_reg,
			    V_CIDXINC(ndesc) |
			    V_INGRESSQID(nm_rxq->iq_cntxt_id) |
			    V_SEINTARM(V_QINTR_TIMER_IDX(X_TIMERREG_UPDATE_CIDX)));
			ndesc = 0;
		}
	}

	atomic_store_rel_32(&nm_rxq->fl_cidx, fl_cidx);
	if (black_hole) {
		fl_credits /= 8;
		IDXINCR(nm_rxq->fl_pidx, fl_credits * 8, nm_rxq->fl_sidx);
		t4_write_reg(sc, sc->sge_kdoorbell_reg,
		    nm_rxq->fl_db_val | V_PIDX(fl_credits));
	} else if (nframes > 0)
		netmap_rx_irq(ifp, nm_rxq->nid, &work);

	t4_write_reg(sc, sc->sge_gts_reg, V_CIDXINC(ndesc) |
	    V_INGRESSQID((u32)nm_rxq->iq_cntxt_id) |
	    V_SEINTARM(V_QINTR_TIMER_IDX(holdoff_tmr_idx)));
}
#endif
