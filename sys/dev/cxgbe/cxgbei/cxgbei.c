/*-
 * Copyright (c) 2012 Chelsio Communications, Inc.
 * All rights reserved.
 *
 * Chelsio T5xx iSCSI driver
 *
 * Written by: Sreenivasa Honnur <shonnur@chelsio.com>
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
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>

#ifdef TCP_OFFLOAD
#include <sys/errno.h>
#include <sys/kthread.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/toecore.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_fsm.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_da.h>
#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl.h>
#include <cam/ctl/ctl_backend.h>
#include <cam/ctl/ctl_error.h>
#include <cam/ctl/ctl_frontend.h>
#include <cam/ctl/ctl_debug.h>
#include <cam/ctl/ctl_ha.h>
#include <cam/ctl/ctl_ioctl.h>

#include <dev/iscsi/icl.h>
#include <dev/iscsi/iscsi_proto.h>
#include <dev/iscsi/iscsi_ioctl.h>
#include <dev/iscsi/iscsi.h>
#include <cam/ctl/ctl_frontend_iscsi.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_xpt.h>
#include <cam/cam_debug.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_periph.h>
#include <cam/cam_compat.h>
#include <cam/scsi/scsi_message.h>

#include "common/common.h"
#include "common/t4_msg.h"
#include "common/t4_regs.h"     /* for PCIE_MEM_ACCESS */
#include "tom/t4_tom.h"
#include "cxgbei.h"
#include "cxgbei_ulp2_ddp.h"

static int worker_thread_count;
static struct cxgbei_worker_thread_softc *cwt_softc;
static struct proc *cxgbei_proc;

/* XXXNP some header instead. */
struct icl_pdu *icl_cxgbei_new_pdu(int);
void icl_cxgbei_new_pdu_set_conn(struct icl_pdu *, struct icl_conn *);
void icl_cxgbei_conn_pdu_free(struct icl_conn *, struct icl_pdu *);

/*
 * Direct Data Placement -
 * Directly place the iSCSI Data-In or Data-Out PDU's payload into pre-posted
 * final destination host-memory buffers based on the Initiator Task Tag (ITT)
 * in Data-In or Target Task Tag (TTT) in Data-Out PDUs.
 * The host memory address is programmed into h/w in the format of pagepod
 * entries.
 * The location of the pagepod entry is encoded into ddp tag which is used as
 * the base for ITT/TTT.
 */

/*
 * functions to program the pagepod in h/w
 */
static void inline
ppod_set(struct pagepod *ppod,
	struct cxgbei_ulp2_pagepod_hdr *hdr,
	struct cxgbei_ulp2_gather_list *gl,
	unsigned int pidx)
{
	int i;

	memcpy(ppod, hdr, sizeof(*hdr));

	for (i = 0; i < (PPOD_PAGES + 1); i++, pidx++) {
		ppod->addr[i] = pidx < gl->nelem ?
			cpu_to_be64(gl->dma_sg[pidx].phys_addr) : 0ULL;
	}
}

static void inline
ppod_clear(struct pagepod *ppod)
{
	memset(ppod, 0, sizeof(*ppod));
}

static inline void
ulp_mem_io_set_hdr(struct adapter *sc, int tid, struct ulp_mem_io *req,
		unsigned int wr_len, unsigned int dlen,
		unsigned int pm_addr)
{
	struct ulptx_idata *idata = (struct ulptx_idata *)(req + 1);

	INIT_ULPTX_WR(req, wr_len, 0, 0);
	req->cmd = cpu_to_be32(V_ULPTX_CMD(ULP_TX_MEM_WRITE) |
				V_ULP_MEMIO_ORDER(is_t4(sc)) |
				V_T5_ULP_MEMIO_IMM(is_t5(sc)));
	req->dlen = htonl(V_ULP_MEMIO_DATA_LEN(dlen >> 5));
	req->len16 = htonl(DIV_ROUND_UP(wr_len - sizeof(req->wr), 16)
				| V_FW_WR_FLOWID(tid));
	req->lock_addr = htonl(V_ULP_MEMIO_ADDR(pm_addr >> 5));

	idata->cmd_more = htonl(V_ULPTX_CMD(ULP_TX_SC_IMM));
	idata->len = htonl(dlen);
}

#define PPOD_SIZE		sizeof(struct pagepod)
#define ULPMEM_IDATA_MAX_NPPODS 1	/* 256/PPOD_SIZE */
#define PCIE_MEMWIN_MAX_NPPODS 16	/* 1024/PPOD_SIZE */

static int
ppod_write_idata(struct cxgbei_data *ci,
			struct cxgbei_ulp2_pagepod_hdr *hdr,
			unsigned int idx, unsigned int npods,
			struct cxgbei_ulp2_gather_list *gl,
			unsigned int gl_pidx, struct toepcb *toep)
{
	u_int dlen = PPOD_SIZE * npods;
	u_int pm_addr = idx * PPOD_SIZE + ci->llimit;
	u_int wr_len = roundup(sizeof(struct ulp_mem_io) +
	    sizeof(struct ulptx_idata) + dlen, 16);
	struct ulp_mem_io *req;
	struct ulptx_idata *idata;
	struct pagepod *ppod;
	u_int i;
	struct wrqe *wr;
	struct adapter *sc = toep->vi->pi->adapter;

	wr = alloc_wrqe(wr_len, toep->ctrlq);
	if (wr == NULL) {
		CXGBE_UNIMPLEMENTED("ppod_write_idata: alloc_wrqe failure");
		return (ENOMEM);
	}

	req = wrtod(wr);
	memset(req, 0, wr_len);
	ulp_mem_io_set_hdr(sc, toep->tid, req, wr_len, dlen, pm_addr);
	idata = (struct ulptx_idata *)(req + 1);

	ppod = (struct pagepod *)(idata + 1);
	for (i = 0; i < npods; i++, ppod++, gl_pidx += PPOD_PAGES) {
		if (!hdr) /* clear the pagepod */
			ppod_clear(ppod);
		else /* set the pagepod */
			ppod_set(ppod, hdr, gl, gl_pidx);
	}

	t4_wrq_tx(sc, wr);
	return 0;
}

int
t4_ddp_set_map(struct cxgbei_data *ci, void *iccp,
    struct cxgbei_ulp2_pagepod_hdr *hdr, u_int idx, u_int npods,
    struct cxgbei_ulp2_gather_list *gl, int reply)
{
	struct icl_cxgbei_conn *icc = (struct icl_cxgbei_conn *)iccp;
	struct toepcb *toep = icc->toep;
	int err;
	unsigned int pidx = 0, w_npods = 0, cnt;

	/*
	 * on T4, if we use a mix of IMMD and DSGL with ULP_MEM_WRITE,
	 * the order would not be guaranteed, so we will stick with IMMD
	 */
	gl->tid = toep->tid;
	gl->port_id = toep->vi->pi->port_id;
	gl->egress_dev = (void *)toep->vi->ifp;

	/* send via immediate data */
	for (; w_npods < npods; idx += cnt, w_npods += cnt,
		pidx += PPOD_PAGES) {
		cnt = npods - w_npods;
		if (cnt > ULPMEM_IDATA_MAX_NPPODS)
			cnt = ULPMEM_IDATA_MAX_NPPODS;
		err = ppod_write_idata(ci, hdr, idx, cnt, gl, pidx, toep);
		if (err) {
			printf("%s: ppod_write_idata failed\n", __func__);
			break;
		}
	}
	return err;
}

void
t4_ddp_clear_map(struct cxgbei_data *ci, struct cxgbei_ulp2_gather_list *gl,
    u_int tag, u_int idx, u_int npods, struct icl_cxgbei_conn *icc)
{
	struct toepcb *toep = icc->toep;
	int err = -1;
	u_int pidx = 0;
	u_int w_npods = 0;
	u_int cnt;

	for (; w_npods < npods; idx += cnt, w_npods += cnt,
		pidx += PPOD_PAGES) {
		cnt = npods - w_npods;
		if (cnt > ULPMEM_IDATA_MAX_NPPODS)
			cnt = ULPMEM_IDATA_MAX_NPPODS;
		err = ppod_write_idata(ci, NULL, idx, cnt, gl, 0, toep);
		if (err)
			break;
	}
}

static int
cxgbei_map_sg(struct cxgbei_sgl *sgl, struct ccb_scsiio *csio)
{
	unsigned int data_len = csio->dxfer_len;
	unsigned int sgoffset = (uint64_t)csio->data_ptr & PAGE_MASK;
	unsigned int nsge;
	unsigned char *sgaddr = csio->data_ptr;
	unsigned int len = 0;

	nsge = (csio->dxfer_len + sgoffset + PAGE_SIZE - 1) >> PAGE_SHIFT;
	sgl->sg_addr = sgaddr;
	sgl->sg_offset = sgoffset;
	if (data_len <  (PAGE_SIZE - sgoffset))
		len = data_len;
	else
		len = PAGE_SIZE - sgoffset;

	sgl->sg_length = len;

	data_len -= len;
	sgaddr += len;
	sgl = sgl+1;

	while (data_len > 0) {
		sgl->sg_addr = sgaddr;
		len = (data_len < PAGE_SIZE)? data_len: PAGE_SIZE;
		sgl->sg_length = len;
	        sgaddr += len;
		data_len -= len;
		sgl = sgl + 1;
	}

	return nsge;
}

static int
cxgbei_map_sg_tgt(struct cxgbei_sgl *sgl, union ctl_io *io)
{
	unsigned int data_len, sgoffset, nsge;
	unsigned char *sgaddr;
	unsigned int len = 0, index = 0, ctl_sg_count, i;
	struct ctl_sg_entry ctl_sg_entry, *ctl_sglist;

	if (io->scsiio.kern_sg_entries > 0) {
		ctl_sglist = (struct ctl_sg_entry *)io->scsiio.kern_data_ptr;
		ctl_sg_count = io->scsiio.kern_sg_entries;
	} else {
		ctl_sglist = &ctl_sg_entry;
		ctl_sglist->addr = io->scsiio.kern_data_ptr;
		ctl_sglist->len = io->scsiio.kern_data_len;
		ctl_sg_count = 1;
	}

	sgaddr = sgl->sg_addr = ctl_sglist[index].addr;
	sgoffset = sgl->sg_offset = (uint64_t)sgl->sg_addr & PAGE_MASK;
	data_len = ctl_sglist[index].len;

	if (data_len <  (PAGE_SIZE - sgoffset))
		len = data_len;
	else
		len = PAGE_SIZE - sgoffset;

	sgl->sg_length = len;

	data_len -= len;
	sgaddr += len;
	sgl = sgl+1;

	len = 0;
	for (i = 0;  i< ctl_sg_count; i++)
		len += ctl_sglist[i].len;
	nsge = (len + sgoffset + PAGE_SIZE -1) >> PAGE_SHIFT;
	while (data_len > 0) {
		sgl->sg_addr = sgaddr;
		len = (data_len < PAGE_SIZE)? data_len: PAGE_SIZE;
		sgl->sg_length = len;
		sgaddr += len;
		data_len -= len;
		sgl = sgl + 1;
		if (data_len == 0) {
			if (index == ctl_sg_count - 1)
				break;
			index++;
			sgaddr = ctl_sglist[index].addr;
			data_len = ctl_sglist[index].len;
		}
	}

	return nsge;
}

static int
t4_sk_ddp_tag_reserve(struct cxgbei_data *ci, struct icl_cxgbei_conn *icc,
    u_int xferlen, struct cxgbei_sgl *sgl, u_int sgcnt, u_int *ddp_tag)
{
	struct cxgbei_ulp2_gather_list *gl;
	int err = -EINVAL;
	struct toepcb *toep = icc->toep;

	gl = cxgbei_ulp2_ddp_make_gl_from_iscsi_sgvec(xferlen, sgl, sgcnt, ci, 0);
	if (gl) {
		err = cxgbei_ulp2_ddp_tag_reserve(ci, icc, toep->tid,
		    &ci->tag_format, ddp_tag, gl, 0, 0);
		if (err) {
			cxgbei_ulp2_ddp_release_gl(ci, gl);
		}
	}

	return err;
}

static unsigned int
cxgbei_task_reserve_itt(struct icl_conn *ic, void **prv,
			struct ccb_scsiio *scmd, unsigned int *itt)
{
	struct icl_cxgbei_conn *icc = ic_to_icc(ic);
	int xferlen = scmd->dxfer_len;
	struct cxgbei_task_data *tdata = NULL;
	struct cxgbei_sgl *sge = NULL;
	struct toepcb *toep = icc->toep;
	struct adapter *sc = td_adapter(toep->td);
	struct cxgbei_data *ci = sc->iscsi_ulp_softc;
	int err = -1;

	MPASS(icc->icc_signature == CXGBEI_CONN_SIGNATURE);

	tdata = (struct cxgbei_task_data *)*prv;
	if (xferlen == 0 || tdata == NULL)
		goto out;
	if (xferlen < DDP_THRESHOLD)
		goto out;

	if ((scmd->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
		tdata->nsge = cxgbei_map_sg(tdata->sgl, scmd);
		if (tdata->nsge == 0) {
			CTR1(KTR_CXGBE, "%s: map_sg failed", __func__);
			return 0;
		}
		sge = tdata->sgl;

		tdata->sc_ddp_tag = *itt;

		CTR3(KTR_CXGBE, "%s: *itt:0x%x sc_ddp_tag:0x%x",
				__func__, *itt, tdata->sc_ddp_tag);
		if (cxgbei_ulp2_sw_tag_usable(&ci->tag_format,
							tdata->sc_ddp_tag)) {
			err = t4_sk_ddp_tag_reserve(ci, icc, scmd->dxfer_len,
			    sge, tdata->nsge, &tdata->sc_ddp_tag);
		} else {
			CTR3(KTR_CXGBE,
				"%s: itt:0x%x sc_ddp_tag:0x%x not usable",
				__func__, *itt, tdata->sc_ddp_tag);
		}
	}
out:
	if (err < 0)
		tdata->sc_ddp_tag =
			cxgbei_ulp2_set_non_ddp_tag(&ci->tag_format, *itt);

	return tdata->sc_ddp_tag;
}

static unsigned int
cxgbei_task_reserve_ttt(struct icl_conn *ic, void **prv, union ctl_io *io,
				unsigned int *ttt)
{
	struct icl_cxgbei_conn *icc = ic_to_icc(ic);
	struct toepcb *toep = icc->toep;
	struct adapter *sc = td_adapter(toep->td);
	struct cxgbei_data *ci = sc->iscsi_ulp_softc;
	struct cxgbei_task_data *tdata = NULL;
	int xferlen, err = -1;
	struct cxgbei_sgl *sge = NULL;

	MPASS(icc->icc_signature == CXGBEI_CONN_SIGNATURE);

	xferlen = (io->scsiio.kern_data_len - io->scsiio.ext_data_filled);
	tdata = (struct cxgbei_task_data *)*prv;
	if ((xferlen == 0) || (tdata == NULL))
		goto out;
	if (xferlen < DDP_THRESHOLD)
		goto out;
	tdata->nsge = cxgbei_map_sg_tgt(tdata->sgl, io);
	if (tdata->nsge == 0) {
		CTR1(KTR_CXGBE, "%s: map_sg failed", __func__);
		return 0;
	}
	sge = tdata->sgl;

	tdata->sc_ddp_tag = *ttt;
	if (cxgbei_ulp2_sw_tag_usable(&ci->tag_format, tdata->sc_ddp_tag)) {
		err = t4_sk_ddp_tag_reserve(ci, icc, xferlen, sge,
		    tdata->nsge, &tdata->sc_ddp_tag);
	} else {
		CTR2(KTR_CXGBE, "%s: sc_ddp_tag:0x%x not usable",
				__func__, tdata->sc_ddp_tag);
	}
out:
	if (err < 0)
		tdata->sc_ddp_tag =
			cxgbei_ulp2_set_non_ddp_tag(&ci->tag_format, *ttt);
	return tdata->sc_ddp_tag;
}

static int
t4_sk_ddp_tag_release(struct icl_cxgbei_conn *icc, unsigned int ddp_tag)
{
	struct toepcb *toep = icc->toep;
	struct adapter *sc = td_adapter(toep->td);
	struct cxgbei_data *ci = sc->iscsi_ulp_softc;

	cxgbei_ulp2_ddp_tag_release(ci, ddp_tag, icc);

	return (0);
}

static void
read_pdu_limits(struct adapter *sc, uint32_t *max_tx_pdu_len,
    uint32_t *max_rx_pdu_len)
{
	uint32_t tx_len, rx_len, r, v;

	rx_len = t4_read_reg(sc, A_TP_PMM_RX_PAGE_SIZE);
	tx_len = t4_read_reg(sc, A_TP_PMM_TX_PAGE_SIZE);

	r = t4_read_reg(sc, A_TP_PARA_REG2);
	rx_len = min(rx_len, G_MAXRXDATA(r));
	tx_len = min(tx_len, G_MAXRXDATA(r));

	r = t4_read_reg(sc, A_TP_PARA_REG7);
	v = min(G_PMMAXXFERLEN0(r), G_PMMAXXFERLEN1(r));
	rx_len = min(rx_len, v);
	tx_len = min(tx_len, v);

	/* Remove after FW_FLOWC_MNEM_TXDATAPLEN_MAX fix in firmware. */
	tx_len = min(tx_len, 3 * 4096);

	*max_tx_pdu_len = rounddown2(tx_len, 512);
	*max_rx_pdu_len = rounddown2(rx_len, 512);
}

/*
 * Initialize the software state of the iSCSI ULP driver.
 *
 * ENXIO means firmware didn't set up something that it was supposed to.
 */
static int
cxgbei_init(struct adapter *sc, struct cxgbei_data *ci)
{
	int nppods, bits, rc;
	static const u_int pgsz_order[] = {0, 1, 2, 3};

	MPASS(sc->vres.iscsi.size > 0);

	ci->llimit = sc->vres.iscsi.start;
	ci->ulimit = sc->vres.iscsi.start + sc->vres.iscsi.size - 1;
	read_pdu_limits(sc, &ci->max_tx_pdu_len, &ci->max_rx_pdu_len);

	nppods = sc->vres.iscsi.size >> IPPOD_SIZE_SHIFT;
	if (nppods <= 1024)
		return (ENXIO);

	bits = fls(nppods);
	if (bits > IPPOD_IDX_MAX_SIZE)
		bits = IPPOD_IDX_MAX_SIZE;
	nppods = (1 << (bits - 1)) - 1;

	rc = bus_dma_tag_create(NULL, 1, 0, BUS_SPACE_MAXADDR,
	    BUS_SPACE_MAXADDR, NULL, NULL, UINT32_MAX , 8, BUS_SPACE_MAXSIZE,
	    BUS_DMA_ALLOCNOW, NULL, NULL, &ci->ulp_ddp_tag);
	if (rc != 0) {
		device_printf(sc->dev, "%s: failed to create DMA tag: %u.\n",
		    __func__, rc);
		return (rc);
	}

	ci->colors = malloc(nppods * sizeof(char), M_CXGBE, M_NOWAIT | M_ZERO);
	ci->gl_map = malloc(nppods * sizeof(struct cxgbei_ulp2_gather_list *),
	    M_CXGBE, M_NOWAIT | M_ZERO);
	if (ci->colors == NULL || ci->gl_map == NULL) {
		bus_dma_tag_destroy(ci->ulp_ddp_tag);
		free(ci->colors, M_CXGBE);
		free(ci->gl_map, M_CXGBE);
		return (ENOMEM);
	}

	mtx_init(&ci->map_lock, "ddp lock", NULL, MTX_DEF | MTX_DUPOK);
	ci->nppods = nppods;
	ci->idx_last = nppods;
	ci->idx_bits = bits;
	ci->idx_mask = (1 << bits) - 1;
	ci->rsvd_tag_mask = (1 << (bits + IPPOD_IDX_SHIFT)) - 1;

	ci->tag_format.sw_bits = bits;
	ci->tag_format.rsvd_bits = bits;
	ci->tag_format.rsvd_shift = IPPOD_IDX_SHIFT;
	ci->tag_format.rsvd_mask = ci->idx_mask;

	t4_iscsi_init(sc, ci->idx_mask << IPPOD_IDX_SHIFT, pgsz_order);

	return (rc);
}

static int
do_rx_iscsi_hdr(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	struct cpl_iscsi_hdr *cpl = mtod(m, struct cpl_iscsi_hdr *);
	u_int tid = GET_TID(cpl);
	struct toepcb *toep = lookup_tid(sc, tid);
	struct icl_pdu *ip;
	struct icl_cxgbei_pdu *icp;

	M_ASSERTPKTHDR(m);

	ip = icl_cxgbei_new_pdu(M_NOWAIT);
	if (ip == NULL)
		CXGBE_UNIMPLEMENTED("PDU allocation failure");
	icp = ip_to_icp(ip);
	bcopy(mtod(m, caddr_t) + sizeof(*cpl), icp->ip.ip_bhs, sizeof(struct
	    iscsi_bhs));
	icp->icp_seq = ntohl(cpl->seq);
	icp->icp_flags = ICPF_RX_HDR;

	/* This is the start of a new PDU.  There should be no old state. */
	MPASS(toep->ulpcb2 == NULL);
	toep->ulpcb2 = icp;

#if 0
	CTR4(KTR_CXGBE, "%s: tid %u, cpl->len hlen %u, m->m_len hlen %u",
	    __func__, tid, ntohs(cpl->len), m->m_len);
#endif

	m_freem(m);
	return (0);
}

static int
do_rx_iscsi_data(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	struct cpl_iscsi_data *cpl =  mtod(m, struct cpl_iscsi_data *);
	u_int tid = GET_TID(cpl);
	struct toepcb *toep = lookup_tid(sc, tid);
	struct icl_cxgbei_pdu *icp = toep->ulpcb2;

	M_ASSERTPKTHDR(m);

	/* Must already have received the header (but not the data). */
	MPASS(icp != NULL);
	MPASS(icp->icp_flags == ICPF_RX_HDR);
	MPASS(icp->ip.ip_data_mbuf == NULL);
	MPASS(icp->ip.ip_data_len == 0);

	m_adj(m, sizeof(*cpl));

	icp->icp_flags |= ICPF_RX_FLBUF;
	icp->ip.ip_data_mbuf = m;
	icp->ip.ip_data_len = m->m_pkthdr.len;

#if 0
	CTR4(KTR_CXGBE, "%s: tid %u, cpl->len dlen %u, m->m_len dlen %u",
	    __func__, tid, ntohs(cpl->len), m->m_len);
#endif

	return (0);
}

static int
do_rx_iscsi_ddp(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_rx_data_ddp *cpl = (const void *)(rss + 1);
	u_int tid = GET_TID(cpl);
	struct toepcb *toep = lookup_tid(sc, tid);
	struct inpcb *inp = toep->inp;
	struct socket *so;
	struct sockbuf *sb;
	struct tcpcb *tp;
	struct icl_cxgbei_conn *icc;
	struct icl_conn *ic;
	struct icl_cxgbei_pdu *icp = toep->ulpcb2;
	struct icl_pdu *ip;
	u_int pdu_len, val;

	MPASS(m == NULL);

	/* Must already be assembling a PDU. */
	MPASS(icp != NULL);
	MPASS(icp->icp_flags & ICPF_RX_HDR);	/* Data is optional. */
	ip = &icp->ip;
	icp->icp_flags |= ICPF_RX_STATUS;
	val = ntohl(cpl->ddpvld);
	if (val & F_DDP_PADDING_ERR)
		icp->icp_flags |= ICPF_PAD_ERR;
	if (val & F_DDP_HDRCRC_ERR)
		icp->icp_flags |= ICPF_HCRC_ERR;
	if (val & F_DDP_DATACRC_ERR)
		icp->icp_flags |= ICPF_DCRC_ERR;
	if (ip->ip_data_mbuf == NULL) {
		/* XXXNP: what should ip->ip_data_len be, and why? */
		icp->icp_flags |= ICPF_RX_DDP;
	}
	pdu_len = ntohs(cpl->len);	/* includes everything. */

	INP_WLOCK(inp);
	if (__predict_false(inp->inp_flags & (INP_DROPPED | INP_TIMEWAIT))) {
		CTR4(KTR_CXGBE, "%s: tid %u, rx (%d bytes), inp_flags 0x%x",
		    __func__, tid, pdu_len, inp->inp_flags);
		INP_WUNLOCK(inp);
		icl_cxgbei_conn_pdu_free(NULL, ip);
#ifdef INVARIANTS
		toep->ulpcb2 = NULL;
#endif
		return (0);
	}

	tp = intotcpcb(inp);
	MPASS(icp->icp_seq == tp->rcv_nxt);
	MPASS(tp->rcv_wnd >= pdu_len);
	tp->rcv_nxt += pdu_len;
	tp->rcv_wnd -= pdu_len;
	tp->t_rcvtime = ticks;

	/* update rx credits */
	toep->rx_credits += pdu_len;
	t4_rcvd(&toep->td->tod, tp);	/* XXX: sc->tom_softc.tod */

	so = inp->inp_socket;
	sb = &so->so_rcv;
	SOCKBUF_LOCK(sb);

	icc = toep->ulpcb;
	if (__predict_false(icc == NULL || sb->sb_state & SBS_CANTRCVMORE)) {
		CTR5(KTR_CXGBE,
		    "%s: tid %u, excess rx (%d bytes), icc %p, sb_state 0x%x",
		    __func__, tid, pdu_len, icc, sb->sb_state);
		SOCKBUF_UNLOCK(sb);
		INP_WUNLOCK(inp);

		INP_INFO_RLOCK(&V_tcbinfo);
		INP_WLOCK(inp);
		tp = tcp_drop(tp, ECONNRESET);
		if (tp)
			INP_WUNLOCK(inp);
		INP_INFO_RUNLOCK(&V_tcbinfo);

		icl_cxgbei_conn_pdu_free(NULL, ip);
#ifdef INVARIANTS
		toep->ulpcb2 = NULL;
#endif
		return (0);
	}
	MPASS(icc->icc_signature == CXGBEI_CONN_SIGNATURE);
	ic = &icc->ic;
	icl_cxgbei_new_pdu_set_conn(ip, ic);

	MPASS(m == NULL); /* was unused, we'll use it now. */
	m = sbcut_locked(sb, sbused(sb)); /* XXXNP: toep->sb_cc accounting? */
	if (__predict_false(m != NULL)) {
		int len = m_length(m, NULL);

		/*
		 * PDUs were received before the tid transitioned to ULP mode.
		 * Convert them to icl_cxgbei_pdus and send them to ICL before
		 * the PDU in icp/ip.
		 */
		CTR3(KTR_CXGBE, "%s: tid %u, %u bytes in so_rcv", __func__, tid,
		    len);

		/* XXXNP: needs to be rewritten. */
		if (len == sizeof(struct iscsi_bhs) || len == 4 + sizeof(struct
		    iscsi_bhs)) {
			struct icl_cxgbei_pdu *icp0;
			struct icl_pdu *ip0;

			ip0 = icl_cxgbei_new_pdu(M_NOWAIT);
			icl_cxgbei_new_pdu_set_conn(ip0, ic);
			if (ip0 == NULL)
				CXGBE_UNIMPLEMENTED("PDU allocation failure");
			icp0 = ip_to_icp(ip0);
			icp0->icp_seq = 0; /* XXX */
			icp0->icp_flags = ICPF_RX_HDR | ICPF_RX_STATUS;
			m_copydata(m, 0, sizeof(struct iscsi_bhs), (void *)ip0->ip_bhs);
			STAILQ_INSERT_TAIL(&icc->rcvd_pdus, ip0, ip_next);
		}
		m_freem(m);
	}

#if 0
	CTR4(KTR_CXGBE, "%s: tid %u, pdu_len %u, pdu_flags 0x%x",
	    __func__, tid, pdu_len, icp->icp_flags);
#endif

	STAILQ_INSERT_TAIL(&icc->rcvd_pdus, ip, ip_next);
	if ((icc->rx_flags & RXF_ACTIVE) == 0) {
		struct cxgbei_worker_thread_softc *cwt = &cwt_softc[icc->cwt];

		mtx_lock(&cwt->cwt_lock);
		icc->rx_flags |= RXF_ACTIVE;
		TAILQ_INSERT_TAIL(&cwt->rx_head, icc, rx_link);
		if (cwt->cwt_state == CWT_SLEEPING) {
			cwt->cwt_state = CWT_RUNNING;
			cv_signal(&cwt->cwt_cv);
		}
		mtx_unlock(&cwt->cwt_lock);
	}
	SOCKBUF_UNLOCK(sb);
	INP_WUNLOCK(inp);

#ifdef INVARIANTS
	toep->ulpcb2 = NULL;
#endif

	return (0);
}

/* initiator */
void
cxgbei_conn_task_reserve_itt(void *conn, void **prv,
				void *scmd, unsigned int *itt)
{
	unsigned int tag;
	tag = cxgbei_task_reserve_itt(conn, prv, scmd, itt);
	if (tag)
		*itt = htonl(tag);
	return;
}

/* target */
void
cxgbei_conn_transfer_reserve_ttt(void *conn, void **prv,
				void *scmd, unsigned int *ttt)
{
	unsigned int tag;
	tag = cxgbei_task_reserve_ttt(conn, prv, scmd, ttt);
	if (tag)
		*ttt = htonl(tag);
	return;
}

void
cxgbei_cleanup_task(void *conn, void *ofld_priv)
{
	struct icl_conn *ic = (struct icl_conn *)conn;
	struct icl_cxgbei_conn *icc = ic_to_icc(ic);
	struct cxgbei_task_data *tdata = ofld_priv;
	struct adapter *sc = icc->sc;
	struct cxgbei_data *ci = sc->iscsi_ulp_softc;

	MPASS(icc->icc_signature == CXGBEI_CONN_SIGNATURE);
	MPASS(tdata != NULL);

	if (cxgbei_ulp2_is_ddp_tag(&ci->tag_format, tdata->sc_ddp_tag))
		t4_sk_ddp_tag_release(icc, tdata->sc_ddp_tag);
	memset(tdata, 0, sizeof(*tdata));
}

static int
cxgbei_activate(struct adapter *sc)
{
	struct cxgbei_data *ci;
	int rc;

	ASSERT_SYNCHRONIZED_OP(sc);

	if (uld_active(sc, ULD_ISCSI)) {
		KASSERT(0, ("%s: iSCSI offload already enabled on adapter %p",
		    __func__, sc));
		return (0);
	}

	if (sc->iscsicaps == 0 || sc->vres.iscsi.size == 0) {
		device_printf(sc->dev,
		    "not iSCSI offload capable, or capability disabled.\n");
		return (ENOSYS);
	}

	/* per-adapter softc for iSCSI */
	ci = malloc(sizeof(*ci), M_CXGBE, M_ZERO | M_NOWAIT);
	if (ci == NULL)
		return (ENOMEM);

	rc = cxgbei_init(sc, ci);
	if (rc != 0) {
		free(ci, M_CXGBE);
		return (rc);
	}

	sc->iscsi_ulp_softc = ci;

	return (0);
}

static int
cxgbei_deactivate(struct adapter *sc)
{

	ASSERT_SYNCHRONIZED_OP(sc);

	if (sc->iscsi_ulp_softc != NULL) {
		cxgbei_ddp_cleanup(sc->iscsi_ulp_softc);
		free(sc->iscsi_ulp_softc, M_CXGBE);
		sc->iscsi_ulp_softc = NULL;
	}

	return (0);
}

static void
cxgbei_activate_all(struct adapter *sc, void *arg __unused)
{

	if (begin_synchronized_op(sc, NULL, SLEEP_OK | INTR_OK, "t4isact") != 0)
		return;

	/* Activate iSCSI if any port on this adapter has IFCAP_TOE enabled. */
	if (sc->offload_map && !uld_active(sc, ULD_ISCSI))
		(void) t4_activate_uld(sc, ULD_ISCSI);

	end_synchronized_op(sc, 0);
}

static void
cxgbei_deactivate_all(struct adapter *sc, void *arg __unused)
{

	if (begin_synchronized_op(sc, NULL, SLEEP_OK | INTR_OK, "t4isdea") != 0)
		return;

	if (uld_active(sc, ULD_ISCSI))
	    (void) t4_deactivate_uld(sc, ULD_ISCSI);

	end_synchronized_op(sc, 0);
}

static struct uld_info cxgbei_uld_info = {
	.uld_id = ULD_ISCSI,
	.activate = cxgbei_activate,
	.deactivate = cxgbei_deactivate,
};

static void
cwt_main(void *arg)
{
	struct cxgbei_worker_thread_softc *cwt = arg;
	struct icl_cxgbei_conn *icc = NULL;
	struct icl_conn *ic;
	struct icl_pdu *ip;
	struct sockbuf *sb;
	STAILQ_HEAD(, icl_pdu) rx_pdus = STAILQ_HEAD_INITIALIZER(rx_pdus);

	MPASS(cwt != NULL);

	mtx_lock(&cwt->cwt_lock);
	MPASS(cwt->cwt_state == 0);
	cwt->cwt_state = CWT_RUNNING;
	cv_signal(&cwt->cwt_cv);

	while (__predict_true(cwt->cwt_state != CWT_STOP)) {
		cwt->cwt_state = CWT_RUNNING;
		while ((icc = TAILQ_FIRST(&cwt->rx_head)) != NULL) {
			TAILQ_REMOVE(&cwt->rx_head, icc, rx_link);
			mtx_unlock(&cwt->cwt_lock);

			ic = &icc->ic;
			sb = &ic->ic_socket->so_rcv;

			SOCKBUF_LOCK(sb);
			MPASS(icc->rx_flags & RXF_ACTIVE);
			if (__predict_true(!(sb->sb_state & SBS_CANTRCVMORE))) {
				MPASS(STAILQ_EMPTY(&rx_pdus));
				STAILQ_SWAP(&icc->rcvd_pdus, &rx_pdus, icl_pdu);
				SOCKBUF_UNLOCK(sb);

				/* Hand over PDUs to ICL. */
				while ((ip = STAILQ_FIRST(&rx_pdus)) != NULL) {
					STAILQ_REMOVE_HEAD(&rx_pdus, ip_next);
					ic->ic_receive(ip);
				}

				SOCKBUF_LOCK(sb);
				MPASS(STAILQ_EMPTY(&rx_pdus));
			}
			MPASS(icc->rx_flags & RXF_ACTIVE);
			if (STAILQ_EMPTY(&icc->rcvd_pdus) ||
			    __predict_false(sb->sb_state & SBS_CANTRCVMORE)) {
				icc->rx_flags &= ~RXF_ACTIVE;
			} else {
				/*
				 * More PDUs were received while we were busy
				 * handing over the previous batch to ICL.
				 * Re-add this connection to the end of the
				 * queue.
				 */
				mtx_lock(&cwt->cwt_lock);
				TAILQ_INSERT_TAIL(&cwt->rx_head, icc,
				    rx_link);
				mtx_unlock(&cwt->cwt_lock);
			}
			SOCKBUF_UNLOCK(sb);

			mtx_lock(&cwt->cwt_lock);
		}

		/* Inner loop doesn't check for CWT_STOP, do that first. */
		if (__predict_false(cwt->cwt_state == CWT_STOP))
			break;
		cwt->cwt_state = CWT_SLEEPING;
		cv_wait(&cwt->cwt_cv, &cwt->cwt_lock);
	}

	MPASS(TAILQ_FIRST(&cwt->rx_head) == NULL);
	mtx_assert(&cwt->cwt_lock, MA_OWNED);
	cwt->cwt_state = CWT_STOPPED;
	cv_signal(&cwt->cwt_cv);
	mtx_unlock(&cwt->cwt_lock);
	kthread_exit();
}

static int
start_worker_threads(void)
{
	int i, rc;
	struct cxgbei_worker_thread_softc *cwt;

	worker_thread_count = min(mp_ncpus, 32);
	cwt_softc = malloc(worker_thread_count * sizeof(*cwt), M_CXGBE,
	    M_WAITOK | M_ZERO);

	MPASS(cxgbei_proc == NULL);
	for (i = 0, cwt = &cwt_softc[0]; i < worker_thread_count; i++, cwt++) {
		mtx_init(&cwt->cwt_lock, "cwt lock", NULL, MTX_DEF);
		cv_init(&cwt->cwt_cv, "cwt cv");
		TAILQ_INIT(&cwt->rx_head);
		rc = kproc_kthread_add(cwt_main, cwt, &cxgbei_proc, NULL, 0, 0,
		    "cxgbei", "%d", i);
		if (rc != 0) {
			printf("cxgbei: failed to start thread #%d/%d (%d)\n",
			    i + 1, worker_thread_count, rc);
			mtx_destroy(&cwt->cwt_lock);
			cv_destroy(&cwt->cwt_cv);
			bzero(&cwt, sizeof(*cwt));
			if (i == 0) {
				free(cwt_softc, M_CXGBE);
				worker_thread_count = 0;

				return (rc);
			}

			/* Not fatal, carry on with fewer threads. */
			worker_thread_count = i;
			rc = 0;
			break;
		}

		/* Wait for thread to start before moving on to the next one. */
		mtx_lock(&cwt->cwt_lock);
		while (cwt->cwt_state == 0)
			cv_wait(&cwt->cwt_cv, &cwt->cwt_lock);
		mtx_unlock(&cwt->cwt_lock);
	}

	MPASS(cwt_softc != NULL);
	MPASS(worker_thread_count > 0);
	return (0);
}

static void
stop_worker_threads(void)
{
	int i;
	struct cxgbei_worker_thread_softc *cwt = &cwt_softc[0];

	MPASS(worker_thread_count >= 0);

	for (i = 0, cwt = &cwt_softc[0]; i < worker_thread_count; i++, cwt++) {
		mtx_lock(&cwt->cwt_lock);
		MPASS(cwt->cwt_state == CWT_RUNNING ||
		    cwt->cwt_state == CWT_SLEEPING);
		cwt->cwt_state = CWT_STOP;
		cv_signal(&cwt->cwt_cv);
		do {
			cv_wait(&cwt->cwt_cv, &cwt->cwt_lock);
		} while (cwt->cwt_state != CWT_STOPPED);
		mtx_unlock(&cwt->cwt_lock);
	}
	free(cwt_softc, M_CXGBE);
}

/* Select a worker thread for a connection. */
u_int
cxgbei_select_worker_thread(struct icl_cxgbei_conn *icc)
{
	struct adapter *sc = icc->sc;
	struct toepcb *toep = icc->toep;
	u_int i, n;

	n = worker_thread_count / sc->sge.nofldrxq;
	if (n > 0)
		i = toep->vi->pi->port_id * n + arc4random() % n;
	else
		i = arc4random() % worker_thread_count;

	CTR3(KTR_CXGBE, "%s: tid %u, cwt %u", __func__, toep->tid, i);

	return (i);
}

static int
cxgbei_mod_load(void)
{
	int rc;

	t4_register_cpl_handler(CPL_ISCSI_HDR, do_rx_iscsi_hdr);
	t4_register_cpl_handler(CPL_ISCSI_DATA, do_rx_iscsi_data);
	t4_register_cpl_handler(CPL_RX_ISCSI_DDP, do_rx_iscsi_ddp);

	rc = start_worker_threads();
	if (rc != 0)
		return (rc);

	rc = t4_register_uld(&cxgbei_uld_info);
	if (rc != 0) {
		stop_worker_threads();
		return (rc);
	}

	t4_iterate(cxgbei_activate_all, NULL);

	return (rc);
}

static int
cxgbei_mod_unload(void)
{

	t4_iterate(cxgbei_deactivate_all, NULL);

	if (t4_unregister_uld(&cxgbei_uld_info) == EBUSY)
		return (EBUSY);

	stop_worker_threads();

	t4_register_cpl_handler(CPL_ISCSI_HDR, NULL);
	t4_register_cpl_handler(CPL_ISCSI_DATA, NULL);
	t4_register_cpl_handler(CPL_RX_ISCSI_DDP, NULL);

	return (0);
}
#endif

static int
cxgbei_modevent(module_t mod, int cmd, void *arg)
{
	int rc = 0;

#ifdef TCP_OFFLOAD
	switch (cmd) {
	case MOD_LOAD:
		rc = cxgbei_mod_load();
		break;

	case MOD_UNLOAD:
		rc = cxgbei_mod_unload();
		break;

	default:
		rc = EINVAL;
	}
#else
	printf("cxgbei: compiled without TCP_OFFLOAD support.\n");
	rc = EOPNOTSUPP;
#endif

	return (rc);
}

static moduledata_t cxgbei_mod = {
	"cxgbei",
	cxgbei_modevent,
	NULL,
};

MODULE_VERSION(cxgbei, 1);
DECLARE_MODULE(cxgbei, cxgbei_mod, SI_SUB_EXEC, SI_ORDER_ANY);
MODULE_DEPEND(cxgbei, t4_tom, 1, 1, 1);
MODULE_DEPEND(cxgbei, cxgbe, 1, 1, 1);
MODULE_DEPEND(cxgbei, icl, 1, 1, 1);
