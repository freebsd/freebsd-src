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
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/kernel.h>
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

#include <common/common.h>
#include <common/t4_msg.h>
#include <common/t4_regs.h>     /* for PCIE_MEM_ACCESS */
#include <tom/t4_tom.h>
#include "cxgbei.h"

#include "cxgbei_ulp2_ddp.h"

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
#include <cam/ctl/ctl_private.h>

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
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

/* forward declarations */
struct icl_pdu * icl_pdu_new_empty(struct icl_conn *, int );
void icl_pdu_free(struct icl_pdu *);

/* mbuf_tag management functions */
struct ulp_mbuf_cb *
get_ulp_mbuf_cb(struct mbuf *m)
{
	struct m_tag    *mtag = NULL;

	mtag = m_tag_get(CXGBE_ISCSI_MBUF_TAG, sizeof(struct ulp_mbuf_cb),
				M_NOWAIT);
	if (mtag == NULL) {
		printf("%s: mtag alloc failed\n", __func__);
		return NULL;
	}
	bzero(mtag + 1, sizeof(struct ulp_mbuf_cb));
	m_tag_prepend(m, mtag);

	return ((struct ulp_mbuf_cb *)(mtag + 1));
}

static struct ulp_mbuf_cb *
find_ulp_mbuf_cb(struct mbuf *m)
{
	struct m_tag    *mtag = NULL;

	if ((mtag = m_tag_find(m, CXGBE_ISCSI_MBUF_TAG, NULL)) == NULL)
		return (NULL);

	return ((struct ulp_mbuf_cb *)(mtag + 1));
}

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
	struct adapter *sc = toep->port->adapter;

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
t4_ddp_set_map(struct cxgbei_data *ci, void *isockp,
    struct cxgbei_ulp2_pagepod_hdr *hdr, u_int idx, u_int npods,
    struct cxgbei_ulp2_gather_list *gl, int reply)
{
	struct iscsi_socket *isock = (struct iscsi_socket *)isockp;
	struct toepcb *toep = isock->toep;
	int err;
	unsigned int pidx = 0, w_npods = 0, cnt;

	/*
	 * on T4, if we use a mix of IMMD and DSGL with ULP_MEM_WRITE,
	 * the order would not be garanteed, so we will stick with IMMD
	 */
	gl->tid = toep->tid;
	gl->port_id = toep->port->port_id;
	gl->egress_dev = (void *)toep->port->ifp;

	/* send via immediate data */
	for (; w_npods < npods; idx += cnt, w_npods += cnt,
		pidx += PPOD_PAGES) {
		cnt = npods - w_npods;
		if (cnt > ULPMEM_IDATA_MAX_NPPODS)
			cnt = ULPMEM_IDATA_MAX_NPPODS;
		err = ppod_write_idata(ci, hdr, idx, cnt, gl,
					pidx, toep);
		if (err) {
			printf("%s: ppod_write_idata failed\n", __func__);
			break;
		}
	}
	return err;
}

void
t4_ddp_clear_map(struct cxgbei_data *ci, struct cxgbei_ulp2_gather_list *gl,
    u_int tag, u_int idx, u_int npods, struct iscsi_socket *isock)
{
	struct toepcb *toep = isock->toep;
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
t4_sk_ddp_tag_reserve(struct cxgbei_data *ci, struct iscsi_socket *isock,
    u_int xferlen, struct cxgbei_sgl *sgl, u_int sgcnt, u_int *ddp_tag)
{
	struct cxgbei_ulp2_gather_list *gl;
	int err = -EINVAL;
	struct toepcb *toep = isock->toep;

	gl = cxgbei_ulp2_ddp_make_gl_from_iscsi_sgvec(xferlen, sgl, sgcnt, ci, 0);
	if (gl) {
		err = cxgbei_ulp2_ddp_tag_reserve(ci, isock, toep->tid,
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
	int xferlen = scmd->dxfer_len;
	struct cxgbei_task_data *tdata = NULL;
	struct cxgbei_sgl *sge = NULL;
	struct iscsi_socket *isock = ic->ic_ofld_prv0;
	struct toepcb *toep = isock->toep;
	struct adapter *sc = td_adapter(toep->td);
	struct cxgbei_data *ci = sc->iscsi_softc;
	int err = -1;

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
			err = t4_sk_ddp_tag_reserve(ci, isock, scmd->dxfer_len,
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
	struct iscsi_socket *isock = ic->ic_ofld_prv0;
	struct toepcb *toep = isock->toep;
	struct adapter *sc = td_adapter(toep->td);
	struct cxgbei_data *ci = sc->iscsi_softc;
	struct cxgbei_task_data *tdata = NULL;
	int xferlen, err = -1;
	struct cxgbei_sgl *sge = NULL;

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
		err = t4_sk_ddp_tag_reserve(ci, isock, xferlen, sge,
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
t4_sk_ddp_tag_release(struct iscsi_socket *isock, unsigned int ddp_tag)
{
	struct toepcb *toep = isock->toep;
	struct adapter *sc = td_adapter(toep->td);
	struct cxgbei_data *ci = sc->iscsi_softc;

	cxgbei_ulp2_ddp_tag_release(ci, ddp_tag, isock);

	return (0);
}

static int
cxgbei_ddp_init(struct adapter *sc, struct cxgbei_data *ci)
{
	int nppods, bits, max_sz, rc;
	static const u_int pgsz_order[] = {0, 1, 2, 3};

	MPASS(sc->vres.iscsi.size > 0);

	ci->llimit = sc->vres.iscsi.start;
	ci->ulimit = sc->vres.iscsi.start + sc->vres.iscsi.size - 1;
	max_sz = G_MAXRXDATA(t4_read_reg(sc, A_TP_PARA_REG2));

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
	ci->max_txsz = ci->max_rxsz = min(max_sz, ULP2_MAX_PKT_SIZE);
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

static void
process_rx_iscsi_hdr(struct toepcb *toep, struct mbuf *m)
{
	struct cpl_iscsi_hdr *cpl =  mtod(m, struct cpl_iscsi_hdr *);
	struct ulp_mbuf_cb *cb, *lcb;
	struct mbuf *lmbuf;
	u_char *byte;
	struct iscsi_socket *isock = toep->ulpcb;
	struct tcpcb *tp = intotcpcb(toep->inp);
	u_int hlen, dlen, plen;

	MPASS(isock != NULL);
	M_ASSERTPKTHDR(m);

	mtx_lock(&isock->iscsi_rcvq_lock);

	/* allocate m_tag to hold ulp info */
	cb = get_ulp_mbuf_cb(m);
	if (cb == NULL)
		CXGBE_UNIMPLEMENTED(__func__);

	/* strip off CPL header */
	m_adj(m, sizeof(*cpl));

	/* figure out if this is the pdu header or data */
	cb->ulp_mode = ULP_MODE_ISCSI;
	if (isock->mbuf_ulp_lhdr == NULL) {

		isock->mbuf_ulp_lhdr = lmbuf = m;
		lcb = cb;
		cb->flags = SBUF_ULP_FLAG_HDR_RCVD;
		/* we only update tp->rcv_nxt once per pdu */
		if (__predict_false(ntohl(cpl->seq) != tp->rcv_nxt)) {
			panic("%s: seq# 0x%x (expected 0x%x) for tid %u",
			    __func__, ntohl(cpl->seq), tp->rcv_nxt, toep->tid);
		}
		byte = m->m_data;
		hlen = ntohs(cpl->len);
		dlen = ntohl(*(unsigned int *)(byte + 4)) & 0xFFFFFF;

		plen = ntohs(cpl->pdu_len_ddp);
		lcb->pdulen = (hlen + dlen + 3) & (~0x3);
		/* workaround for cpl->pdu_len_ddp since it does not include
		the data digest count */
		if (dlen)
			lcb->pdulen += isock->s_dcrc_len;

		tp->rcv_nxt += lcb->pdulen;
		if (tp->rcv_wnd <= lcb->pdulen)
			CTR3(KTR_CXGBE, "%s: Neg rcv_wnd:0x%lx pdulen:0x%x",
				__func__, tp->rcv_wnd, lcb->pdulen);
			tp->rcv_wnd -= lcb->pdulen;
			tp->t_rcvtime = ticks;
	} else {
		lmbuf = isock->mbuf_ulp_lhdr;
		lcb = find_ulp_mbuf_cb(lmbuf);
		if (lcb == NULL)
			CXGBE_UNIMPLEMENTED(__func__);
		lcb->flags |= SBUF_ULP_FLAG_DATA_RCVD;
		cb->flags = SBUF_ULP_FLAG_DATA_RCVD;

		/* padding */
		if ((m->m_len % 4) != 0) {
			m->m_len += 4 - (m->m_len % 4);
		}
	}
	mbufq_enqueue(&isock->iscsi_rcvq, m);
	mtx_unlock(&isock->iscsi_rcvq_lock);
}

/* hand over received PDU to iscsi_initiator */
static void
iscsi_conn_receive_pdu(struct iscsi_socket *isock)
{
	struct icl_pdu *response = NULL;
	struct icl_conn *ic = (struct icl_conn*)isock->s_conn;
	struct mbuf *m;
	struct ulp_mbuf_cb *cb = NULL;
	int data_len;

	response = icl_pdu_new_empty(isock->s_conn, M_NOWAIT);
	if (response == NULL) {
		panic("%s: failed to alloc icl_pdu\n", __func__);
		return;
	}
	m = mbufq_first(&isock->iscsi_rcvq);
	if (m) {
		cb = find_ulp_mbuf_cb(m);
		if (cb == NULL) {
			panic("%s: m:%p cb is NULL\n", __func__, m);
			goto err_out;
		}
		if (!(cb->flags & SBUF_ULP_FLAG_STATUS_RCVD))
			goto err_out;
	}
	/* BHS */
	mbufq_dequeue(&isock->iscsi_rcvq);
	data_len = cb->pdulen;

	CTR5(KTR_CXGBE, "%s: response:%p m:%p m_len:%d data_len:%d",
		__func__, response, m, m->m_len, data_len);
	response->ip_bhs_mbuf = m;
	response->ip_bhs = mtod(response->ip_bhs_mbuf, struct iscsi_bhs *);

	/* data */
	if (cb->flags & SBUF_ULP_FLAG_DATA_RCVD) {
		m = mbufq_first(&isock->iscsi_rcvq);
		if (m == NULL) {
			CTR1(KTR_CXGBE, "%s:No Data", __func__);
			goto err_out;
		}
		mbufq_dequeue(&isock->iscsi_rcvq);
		response->ip_data_mbuf = m;
		response->ip_data_len += response->ip_data_mbuf->m_len;
	} else {
		/* Data is DDP'ed */
		response->ip_ofld_prv0 = 1;
	}
	(ic->ic_receive)(response);
	return;

err_out:
	icl_pdu_free(response);
	return;
}

static void
process_rx_data_ddp(struct toepcb *toep, const struct cpl_rx_data_ddp *cpl)
{
	struct mbuf *lmbuf;
	struct ulp_mbuf_cb *lcb, *lcb1;
	unsigned int val, pdulen;
	struct iscsi_socket *isock = toep->ulpcb;
	struct inpcb *inp = toep->inp;

	MPASS(isock != NULL);

	if (isock->mbuf_ulp_lhdr == NULL) {
		panic("%s: tid 0x%x, rcv RX_DATA_DDP w/o pdu header.\n",
		    __func__, toep->tid);
		return;
	}
	mtx_lock(&isock->iscsi_rcvq_lock);
	lmbuf = isock->mbuf_ulp_lhdr;
	if (lmbuf->m_nextpkt) {
		lcb1 = find_ulp_mbuf_cb(lmbuf->m_nextpkt);
		lcb1->flags |= SBUF_ULP_FLAG_STATUS_RCVD;
	}
	lcb = find_ulp_mbuf_cb(isock->mbuf_ulp_lhdr);
	if (lcb == NULL) {
		CTR2(KTR_CXGBE, "%s: mtag NULL lmbuf :%p", __func__, lmbuf);
		mtx_unlock(&isock->iscsi_rcvq_lock);
		return;
	}
	lcb->flags |= SBUF_ULP_FLAG_STATUS_RCVD;
	isock->mbuf_ulp_lhdr = NULL;

	if (ntohs(cpl->len) != lcb->pdulen) {
		CTR3(KTR_CXGBE, "tid 0x%x, RX_DATA_DDP pdulen %u != %u.",
			toep->tid, ntohs(cpl->len), lcb->pdulen);
		CTR4(KTR_CXGBE, "%s: lmbuf:%p lcb:%p lcb->flags:0x%x",
			__func__, lmbuf, lcb, lcb->flags);
	}

	lcb->ddigest = ntohl(cpl->ulp_crc);
	pdulen = lcb->pdulen;

	val = ntohl(cpl->ddpvld);
	if (val & F_DDP_PADDING_ERR)
		lcb->flags |= SBUF_ULP_FLAG_PAD_ERROR;
	if (val & F_DDP_HDRCRC_ERR)
		lcb->flags |= SBUF_ULP_FLAG_HCRC_ERROR;
	if (val & F_DDP_DATACRC_ERR)
		lcb->flags |= SBUF_ULP_FLAG_DCRC_ERROR;
	if (!(lcb->flags & SBUF_ULP_FLAG_DATA_RCVD)) {
		lcb->flags |= SBUF_ULP_FLAG_DATA_DDPED;
	}
#ifdef __T4_DBG_DDP_FAILURE__
//      else
	{
		unsigned char *bhs = lmbuf->m_data;
		unsigned char opcode = bhs[0];
		unsigned int dlen = ntohl(*(unsigned int *)(bhs + 4)) & 0xFFFFFF;
		unsigned int ttt = ntohl(*(unsigned int *)(bhs + 20));
		unsigned int offset = ntohl(*(unsigned int *)(bhs + 40));

		if (dlen >= 2096) {
		/* data_out and should be ddp'ed */
			if ((opcode & 0x3F) == 0x05 && ttt != 0xFFFFFFFF) {
			printf("CPL_RX_DATA_DDP: tid 0x%x, data-out %s ddp'ed\
			(%u+%u), ttt 0x%x, seq 0x%x, ddpvld 0x%x.\n",
			toep->tid,
			(lcb->flags & SBUF_ULP_FLAG_DATA_DDPED) ? "IS" : "NOT",
			offset, dlen, ttt, ntohl(cpl->seq), ntohl(cpl->ddpvld));
			}
			if ((opcode & 0x3F) == 0x25) {
			//if (!(lcb->flags & SBUF_ULP_FLAG_DATA_DDPED))
			printf("CPL_RX_DATA_DDP: tid 0x%x, data-in %s ddp'ed\
			(%u+%u), seq 0x%x, ddpvld 0x%x.\n",
			toep->tid,
			(lcb->flags & SBUF_ULP_FLAG_DATA_DDPED) ? "IS" : "NOT",
			offset, dlen, ntohl(cpl->seq), ntohl(cpl->ddpvld));
			}
		}
	}
#endif

	iscsi_conn_receive_pdu(isock);
	mtx_unlock(&isock->iscsi_rcvq_lock);

	/* update rx credits */
	INP_WLOCK(inp);
	/* XXXNP: does this want the so_rcv lock?  (happens to be the same) */
	SOCK_LOCK(inp->inp_socket);
	toep->sb_cc += pdulen;
	SOCK_UNLOCK(inp->inp_socket);
	t4_rcvd(&toep->td->tod, intotcpcb(inp));
	INP_WUNLOCK(inp);
	return;
}

static void
drop_fw_acked_ulp_data(struct toepcb *toep, int len)
{
	struct mbuf *m, *next;
	struct ulp_mbuf_cb *cb;
	struct icl_pdu *req;
	struct iscsi_socket *isock = toep->ulpcb;

	MPASS(len > 0);

	mtx_lock(&isock->ulp2_wrq_lock);
	while (len > 0) {
		m = mbufq_dequeue(&isock->ulp2_wrq);
		MPASS(m != NULL);	/* excess credits */

		for (next = m; next != NULL; next = next->m_next) {
			MPASS(len >= next->m_len);	/* excess credits */
			len -= next->m_len;
		}

		cb = find_ulp_mbuf_cb(m);
		if (cb && cb->pdu) {
			req = (struct icl_pdu *)cb->pdu;
			req->ip_bhs_mbuf = NULL;
			icl_pdu_free(req);
		}
		m_freem(m);
	}
	mtx_unlock(&isock->ulp2_wrq_lock);
}

static int
do_rx_iscsi_hdr(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	struct cpl_iscsi_hdr *cpl =  mtod(m, struct cpl_iscsi_hdr *); /* XXXNP */
	u_int tid = GET_TID(cpl);
	struct toepcb *toep = lookup_tid(sc, tid);

	process_rx_iscsi_hdr(toep, m);

	return (0);
}

static int
do_rx_iscsi_ddp(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_rx_data_ddp *cpl = (const void *)(rss + 1);
	u_int tid = GET_TID(cpl);
	struct toepcb *toep = lookup_tid(sc, tid);

	process_rx_data_ddp(toep, cpl);

	return (0);
}

static int
t4_ulp_mbuf_push(struct iscsi_socket *isock, struct mbuf *m)
{
	struct toepcb *toep = isock->toep;

	/* append mbuf to ULP queue */
	mtx_lock(&isock->ulp2_writeq_lock);
	mbufq_enqueue(&isock->ulp2_writeq, m);
	mtx_unlock(&isock->ulp2_writeq_lock);

	INP_WLOCK(toep->inp);
	t4_ulp_push_frames(toep->td->tod.tod_softc, toep, 0);
	INP_WUNLOCK(toep->inp);

	return (0);
}

static struct mbuf *
get_writeq_len(struct toepcb *toep, int *qlen)
{
	struct iscsi_socket *isock = toep->ulpcb;

	*qlen = mbufq_len(&isock->ulp2_writeq);
	return (mbufq_first(&isock->ulp2_writeq));
}

static struct mbuf *
do_writeq_next(struct toepcb *toep)
{
	struct iscsi_socket *isock = toep->ulpcb;
	struct mbuf *m;

	mtx_lock(&isock->ulp2_writeq_lock);
	m = mbufq_dequeue(&isock->ulp2_writeq);
	mtx_unlock(&isock->ulp2_writeq_lock);

	mtx_lock(&isock->ulp2_wrq_lock);
	mbufq_enqueue(&isock->ulp2_wrq, m);
	mtx_unlock(&isock->ulp2_wrq_lock);

	return (mbufq_first(&isock->ulp2_writeq));
}

static void
t4_register_cpl_handler_with_tom(struct adapter *sc)
{

	t4_register_cpl_handler(sc, CPL_ISCSI_HDR, do_rx_iscsi_hdr);
	t4_register_cpl_handler(sc, CPL_ISCSI_DATA, do_rx_iscsi_hdr);
	t4_register_cpl_handler(sc, CPL_RX_ISCSI_DDP, do_rx_iscsi_ddp);
}

static void
t4_unregister_cpl_handler_with_tom(struct adapter *sc)
{

	t4_register_cpl_handler(sc, CPL_ISCSI_HDR, NULL);
	t4_register_cpl_handler(sc, CPL_ISCSI_DATA, NULL);
	t4_register_cpl_handler(sc, CPL_RX_ISCSI_DDP, NULL);
}

static int
send_set_tcb_field(struct toepcb * toep, uint16_t word, uint64_t mask,
    uint64_t val, int no_reply)
{
	struct wrqe *wr;
	struct cpl_set_tcb_field *req;

	wr = alloc_wrqe(sizeof(*req), toep->ctrlq);
	if (wr == NULL)
		return EINVAL;
	req = wrtod(wr);

	INIT_TP_WR_MIT_CPL(req, CPL_SET_TCB_FIELD, toep->tid);
	req->reply_ctrl = htobe16(V_NO_REPLY(no_reply) |
		V_QUEUENO(toep->ofld_rxq->iq.abs_id));
	req->word_cookie = htobe16(V_WORD(word) | V_COOKIE(0));
	req->mask = htobe64(mask);
	req->val = htobe64(val);

	t4_wrq_tx(toep->td->tod.tod_softc, wr);

	return (0);
}

static int
cxgbei_set_ulp_mode(struct toepcb *toep, u_char hcrc, u_char dcrc)
{
	int val = 0;

	if (hcrc)
		val |= ULP_CRC_HEADER;
	if (dcrc)
		val |= ULP_CRC_DATA;
	val <<= 4;
	val |= ULP_MODE_ISCSI;

	return (send_set_tcb_field(toep, 0, 0xfff, val, 1));
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
	struct cxgbei_task_data *tdata = ofld_priv;
	struct iscsi_socket *isock = ic->ic_ofld_prv0;
	struct toepcb *toep = isock->toep;
	struct adapter *sc = td_adapter(toep->td);
	struct cxgbei_data *ci = sc->iscsi_softc;

	MPASS(isock != NULL);
	MPASS(tdata != NULL);

	if (cxgbei_ulp2_is_ddp_tag(&ci->tag_format, tdata->sc_ddp_tag))
		t4_sk_ddp_tag_release(isock, tdata->sc_ddp_tag);
	memset(tdata, 0, sizeof(*tdata));
}

static void
t4_sk_tx_mbuf_setmode(struct icl_pdu *req, void *toep, void *mbuf,
		unsigned char mode, unsigned char hcrc, unsigned char dcrc)
{
	struct mbuf *m = (struct mbuf *)mbuf;
	struct ulp_mbuf_cb *cb;

	cb = get_ulp_mbuf_cb(m);
	if (cb == NULL)
		return;
	cb->ulp_mode = ULP_MODE_ISCSI << 4;
	if (hcrc)
		cb->ulp_mode |= 1;
	if (dcrc)
		cb->ulp_mode |= 2;
	cb->pdu = req;
	return;
}

int
cxgbei_conn_xmit_pdu(struct icl_conn *ic, struct icl_pdu *req)
{
	struct mbuf *m = req->ip_bhs_mbuf;
	struct iscsi_socket *isock = ic->ic_ofld_prv0;
	struct toepcb *toep = isock->toep;

	t4_sk_tx_mbuf_setmode(req, toep, m, 2,
	    ic->ic_header_crc32c ? ISCSI_HEADER_DIGEST_SIZE : 0,
	    (req->ip_data_len && ic->ic_data_crc32c) ? ISCSI_DATA_DIGEST_SIZE : 0);

	t4_ulp_mbuf_push(isock, m);
	return (0);
}

int
cxgbei_conn_handoff(struct icl_conn *ic)
{
	struct tcpcb *tp = so_sototcpcb(ic->ic_socket);
	struct toepcb *toep;
	struct iscsi_socket *isock;

	if (!(tp->t_flags & TF_TOE))
		return (ENOTSUP);	/* Connection is not offloaded. */
	MPASS(tp->tod != NULL);
	MPASS(tp->t_toe != NULL);

	/*
	 * XXXNP: Seems broken.  How can we assume that the tod/toep is what we
	 * think it is?
	 */

	toep = tp->t_toe;
	if (toep->ulp_mode)
		return (EBUSY);	/* Stay away if ulp_mode is already set. */

	isock = malloc(sizeof(struct iscsi_socket), M_CXGBE, M_NOWAIT | M_ZERO);
	if (isock == NULL)
		return (ENOMEM);
	isock->s_conn = ic;
	isock->toep = toep;
	isock->s_dcrc_len = ic->ic_data_crc32c ? 4 : 0;

	mbufq_init(&isock->iscsi_rcvq, INT_MAX);
	mtx_init(&isock->iscsi_rcvq_lock,"isock_lock" , NULL, MTX_DEF);

	mbufq_init(&isock->ulp2_wrq, INT_MAX);
	mtx_init(&isock->ulp2_wrq_lock,"ulp2_wrq lock" , NULL, MTX_DEF);

	mbufq_init(&isock->ulp2_writeq, INT_MAX);
	mtx_init(&isock->ulp2_writeq_lock,"ulp2_writeq lock" , NULL, MTX_DEF);

	/* Move connection to ULP mode. */
	ic->ic_socket->so_options |= SO_NO_DDP;
	toep->ulp_mode = ULP_MODE_ISCSI;
	toep->ulpcb = isock;
	ic->ic_ofld_prv0 = isock;

	return (cxgbei_set_ulp_mode(toep, ic->ic_header_crc32c, ic->ic_data_crc32c));
}

int
cxgbei_conn_close(struct icl_conn *ic)
{
	struct iscsi_socket *isock = ic->ic_ofld_prv0;
	struct toepcb *toep = isock->toep;
	struct mbuf *m;
	struct ulp_mbuf_cb *cb;
	struct icl_pdu *req;

	MPASS(isock != NULL);

	/* free isock Qs */
	/*
	 * XXXNP: some drained with lock held, some without.  And the test for
	 * whether the lock has even been initialized is after it has been
	 * grabbed and released already.
	 *
	 * An even larger issue is whether the TCP connection is going down
	 * gracefully or not.  Can't simply throw away stuff in send/rcv buffers
	 * if the TCP shutdown is supposed to be graceful.
	 */
	mbufq_drain(&isock->iscsi_rcvq);
	mbufq_drain(&isock->ulp2_writeq);

	mtx_lock(&isock->ulp2_wrq_lock);
	while ((m = mbufq_dequeue(&isock->ulp2_wrq)) != NULL) {
		cb = find_ulp_mbuf_cb(m);
		if (cb && cb->pdu) {
			req = (struct icl_pdu *)cb->pdu;
			req->ip_bhs_mbuf = NULL;
			icl_pdu_free(req);
		}
		m_freem(m);
	}
	mtx_unlock(&isock->ulp2_wrq_lock);

	if (mtx_initialized(&isock->iscsi_rcvq_lock))
		mtx_destroy(&isock->iscsi_rcvq_lock);

	if (mtx_initialized(&isock->ulp2_wrq_lock))
		mtx_destroy(&isock->ulp2_wrq_lock);

	if (mtx_initialized(&isock->ulp2_writeq_lock))
		mtx_destroy(&isock->ulp2_writeq_lock);

	/* XXXNP: Should the ulpcb and ulp_mode be cleared here? */
	toep->ulp_mode = ULP_MODE_NONE; 	/* dubious without inp lock */

	free(isock, M_CXGBE);

	return (0);
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

	rc = cxgbei_ddp_init(sc, ci);
	if (rc != 0) {
		free(ci, M_CXGBE);
		return (rc);
	}

	t4_register_cpl_handler_with_tom(sc);
	sc->iscsi_softc = ci;

	return (0);
}

static int
cxgbei_deactivate(struct adapter *sc)
{

	ASSERT_SYNCHRONIZED_OP(sc);

	if (sc->iscsi_softc != NULL) {
		cxgbei_ddp_cleanup(sc->iscsi_softc);
		t4_unregister_cpl_handler_with_tom(sc);
		free(sc->iscsi_softc, M_CXGBE);
		sc->iscsi_softc = NULL;
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

enum {
	CWT_RUNNING = 1,
	CWT_STOP = 2,
	CWT_STOPPED = 3,
};

struct cxgbei_worker_thread_softc {
	struct mtx	cwt_lock;
	struct cv	cwt_cv;
	volatile int	cwt_state;
} __aligned(CACHE_LINE_SIZE);

int worker_thread_count;
static struct cxgbei_worker_thread_softc *cwt_softc;
static struct proc *cxgbei_proc;

static void
cwt_main(void *arg)
{
	struct cxgbei_worker_thread_softc *cwt = arg;

	MPASS(cwt != NULL);

	mtx_lock(&cwt->cwt_lock);
	MPASS(cwt->cwt_state == 0);
	cwt->cwt_state = CWT_RUNNING;
	cv_signal(&cwt->cwt_cv);
	for (;;) {
		cv_wait(&cwt->cwt_cv, &cwt->cwt_lock);
		if (cwt->cwt_state == CWT_STOP)
			break;
	}

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
		while (cwt->cwt_state != CWT_RUNNING)
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
		MPASS(cwt->cwt_state == CWT_RUNNING);
		cwt->cwt_state = CWT_STOP;
		cv_signal(&cwt->cwt_cv);
		do {
			cv_wait(&cwt->cwt_cv, &cwt->cwt_lock);
		} while (cwt->cwt_state != CWT_STOPPED);
		mtx_unlock(&cwt->cwt_lock);
	}
	free(cwt_softc, M_CXGBE);
}

extern void (*cxgbei_fw4_ack)(struct toepcb *, int);
extern void (*cxgbei_rx_data_ddp)(struct toepcb *,
    const struct cpl_rx_data_ddp *);
extern struct mbuf *(*cxgbei_writeq_len)(struct toepcb *, int *);
extern struct mbuf *(*cxgbei_writeq_next)(struct toepcb *);

static int
cxgbei_mod_load(void)
{
	int rc;

	cxgbei_fw4_ack = drop_fw_acked_ulp_data;
	cxgbei_rx_data_ddp = process_rx_data_ddp;
	cxgbei_writeq_len = get_writeq_len;
	cxgbei_writeq_next = do_writeq_next;

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

	return (0);
}

static int
cxgbei_modevent(module_t mod, int cmd, void *arg)
{
	int rc = 0;

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
