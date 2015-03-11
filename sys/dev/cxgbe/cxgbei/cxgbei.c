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
#include <cam/ctl/ctl_frontend_internal.h>
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
#define T4_DDP
#ifdef T4_DDP
/*
 * functions to program the pagepod in h/w
 */
static void *
t4_tdev2ddp(void *tdev)
{
	struct adapter *sc = ((struct toedev *)tdev)->tod_softc;
	return (sc->iscsi_softc);
}
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
ppod_write_idata(struct cxgbei_ulp2_ddp_info *ddp,
			struct cxgbei_ulp2_pagepod_hdr *hdr,
			unsigned int idx, unsigned int npods,
			struct cxgbei_ulp2_gather_list *gl,
			unsigned int gl_pidx, struct toepcb *toep)
{
	unsigned int dlen = PPOD_SIZE * npods;
	unsigned int pm_addr = idx * PPOD_SIZE + ddp->llimit;
	unsigned int wr_len = roundup(sizeof(struct ulp_mem_io) +
				 sizeof(struct ulptx_idata) + dlen, 16);
	struct ulp_mem_io *req;
	struct ulptx_idata *idata;
	struct pagepod *ppod;
	unsigned int i;
	struct wrqe *wr;
	struct adapter *sc = toep->port->adapter;

	wr = alloc_wrqe(wr_len, toep->ctrlq);
	if (wr == NULL) {
		printf("%s: alloc wrqe failed\n", __func__);
		return ENOMEM;
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

static int
t4_ddp_set_map(struct cxgbei_ulp2_ddp_info *ddp,
			void *isockp, struct cxgbei_ulp2_pagepod_hdr *hdr,
			unsigned int idx, unsigned int npods,
			struct cxgbei_ulp2_gather_list *gl, int reply)
{
	iscsi_socket *isock = (iscsi_socket *)isockp;
	struct socket *sk;
	struct toepcb *toep;
	struct tcpcb *tp;
	int err;
	unsigned int pidx = 0, w_npods = 0, cnt;

	if (isock == NULL)
		return EINVAL;
	sk = isock->sock;
	tp = so_sototcpcb(sk);
	toep = tp->t_toe;

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
		err = ppod_write_idata(ddp, hdr, idx, cnt, gl,
					pidx, toep);
		if (err) {
			printf("%s: ppod_write_idata failed\n", __func__);
			break;
		}
	}
	return err;
}

static void
t4_ddp_clear_map(struct cxgbei_ulp2_ddp_info *ddp,
			struct cxgbei_ulp2_gather_list *gl,
			unsigned int tag, unsigned int idx, unsigned int npods,
			iscsi_socket *isock)
{
	struct socket *sk;
	struct toepcb *toep;
	struct tcpcb *tp;
	int err = -1;

	sk = isock->sock;
	tp = so_sototcpcb(sk);
	toep = tp->t_toe;

	/* send via immediate data */
	unsigned int pidx = 0;
	unsigned int w_npods = 0;
	unsigned int cnt;

	for (; w_npods < npods; idx += cnt, w_npods += cnt,
		pidx += PPOD_PAGES) {
		cnt = npods - w_npods;
		if (cnt > ULPMEM_IDATA_MAX_NPPODS)
			cnt = ULPMEM_IDATA_MAX_NPPODS;
		err = ppod_write_idata(ddp, NULL, idx, cnt, gl, 0, toep);
		if (err)
			break;
	}
}
#endif

/*
 * cxgbei device management
 * maintains a list of the cxgbei devices
 */
typedef struct offload_device {
	SLIST_ENTRY(offload_device) link;
	unsigned char d_version;
	unsigned char d_tx_hdrlen;      /* CPL_TX_DATA, < 256 */
	unsigned char d_ulp_rx_datagap; /* for coalesced iscsi msg */
	unsigned char filler;

	unsigned int d_flag;
	unsigned int d_payload_tmax;
	unsigned int d_payload_rmax;

	struct cxgbei_ulp2_tag_format d_tag_format;
	void    *d_tdev;
	void    *d_pdev;
	void* (*tdev2ddp)(void *tdev);
}offload_device;

SLIST_HEAD(, offload_device) odev_list;

static void t4_unregister_cpl_handler_with_tom(struct adapter *sc);
static offload_device *
offload_device_new(void *tdev)
{
	offload_device *odev = NULL;
	odev = malloc(sizeof(struct offload_device),
			M_CXGBE, M_NOWAIT | M_ZERO);
	if (odev) {
		odev->d_tdev = tdev;
		SLIST_INSERT_HEAD(&odev_list, odev, link);
	}

	return odev;
}

static offload_device *
offload_device_find(struct toedev *tdev)
{
	offload_device *odev = NULL;

	if (!SLIST_EMPTY(&odev_list)) {
		SLIST_FOREACH(odev, &odev_list, link) {
		if (odev->d_tdev == tdev)
			break;
		}
	}
	return odev;
}

static void
cxgbei_odev_cleanup(offload_device *odev)
{
	struct toedev *tdev = odev->d_tdev;
	struct adapter *sc = (struct adapter *)tdev->tod_softc;

	/* de-register ULP CPL handlers with TOM */
	t4_unregister_cpl_handler_with_tom(sc);
	if (odev->d_flag & ODEV_FLAG_ULP_DDP_ENABLED) {
		if (sc->iscsi_softc)
			cxgbei_ulp2_ddp_cleanup(
			(struct cxgbei_ulp2_ddp_info **)&sc->iscsi_softc);
	}
	return;
}

static void
offload_device_remove()
{
	offload_device *odev = NULL, *next = NULL;

	if (SLIST_EMPTY(&odev_list))
		return;

	for (odev = SLIST_FIRST(&odev_list); odev != NULL; odev = next) {
		SLIST_REMOVE(&odev_list, odev, offload_device, link);
		next = SLIST_NEXT(odev, link);
		cxgbei_odev_cleanup(odev);
		free(odev, M_CXGBE);
	}

	return;
}

static int
cxgbei_map_sg(cxgbei_sgl *sgl, struct ccb_scsiio *csio)
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
cxgbei_map_sg_tgt(cxgbei_sgl *sgl, union ctl_io *io)
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
t4_sk_ddp_tag_reserve(iscsi_socket *isock, unsigned int xferlen,
			cxgbei_sgl *sgl, unsigned int sgcnt,
			unsigned int *ddp_tag)
{
	offload_device *odev = isock->s_odev;
	struct toedev *tdev = odev->d_tdev;
	struct cxgbei_ulp2_gather_list *gl;
	int err = -EINVAL;
	struct adapter *sc = tdev->tod_softc;
	struct cxgbei_ulp2_ddp_info *ddp;

	ddp = (struct cxgbei_ulp2_ddp_info *)sc->iscsi_softc;
	if (ddp == NULL)
		return ENOMEM;

	gl = cxgbei_ulp2_ddp_make_gl_from_iscsi_sgvec(xferlen, sgl, sgcnt,
					odev->d_tdev, 0);
	if (gl) {
		err = cxgbei_ulp2_ddp_tag_reserve(odev->tdev2ddp(tdev),
						isock,
						isock->s_tid,
						&odev->d_tag_format,
						ddp_tag, gl,
						0, 0);
		if (err) {
			CTR1(KTR_CXGBE,
				"%s: ddp_tag_reserve failed\n", __func__);
			cxgbei_ulp2_ddp_release_gl(gl, odev->d_tdev);
		}
	}

	return err;
}

static unsigned int
cxgbei_task_reserve_itt(struct icl_conn *ic, void **prv,
			struct ccb_scsiio *scmd, unsigned int *itt)
{
	int xferlen = scmd->dxfer_len;
	cxgbei_task_data *tdata = NULL;
	cxgbei_sgl *sge = NULL;
	struct socket *so = ic->ic_socket;
	iscsi_socket *isock = (iscsi_socket *)(so)->so_emuldata;
	int err = -1;
	offload_device *odev = isock->s_odev;

	tdata = (cxgbei_task_data *)*prv;
	if ((xferlen == 0) || (tdata == NULL)) {
		goto out;
	}
	if (xferlen < DDP_THRESHOLD)
		goto out;

	if ((scmd->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
		tdata->nsge = cxgbei_map_sg(tdata->sgl, scmd);
		if (tdata->nsge == 0) {
			CTR1(KTR_CXGBE, "%s: map_sg failed\n", __func__);
			return 0;
		}
		sge = tdata->sgl;

		tdata->sc_ddp_tag = *itt;

		CTR3(KTR_CXGBE, "%s: *itt:0x%x sc_ddp_tag:0x%x\n",
				__func__, *itt, tdata->sc_ddp_tag);
		if (cxgbei_ulp2_sw_tag_usable(&odev->d_tag_format,
							tdata->sc_ddp_tag)) {
			err = t4_sk_ddp_tag_reserve(isock, scmd->dxfer_len, sge,
					 tdata->nsge, &tdata->sc_ddp_tag);
		} else {
			CTR3(KTR_CXGBE,
				"%s: itt:0x%x sc_ddp_tag:0x%x not usable\n",
				__func__, *itt, tdata->sc_ddp_tag);
		}
	}
out:
	if (err < 0)
		tdata->sc_ddp_tag =
			cxgbei_ulp2_set_non_ddp_tag(&odev->d_tag_format, *itt);

	return tdata->sc_ddp_tag;
}

static unsigned int
cxgbei_task_reserve_ttt(struct icl_conn *ic, void **prv, union ctl_io *io,
				unsigned int *ttt)
{
	struct socket *so = ic->ic_socket;
	iscsi_socket *isock = (iscsi_socket *)(so)->so_emuldata;
	cxgbei_task_data *tdata = NULL;
	offload_device *odev = isock->s_odev;
	int xferlen, err = -1;
	cxgbei_sgl *sge = NULL;

	xferlen = (io->scsiio.kern_data_len - io->scsiio.ext_data_filled);
	tdata = (cxgbei_task_data *)*prv;
	if ((xferlen == 0) || (tdata == NULL))
		goto out;
	if (xferlen < DDP_THRESHOLD)
		goto out;
	tdata->nsge = cxgbei_map_sg_tgt(tdata->sgl, io);
	if (tdata->nsge == 0) {
		CTR1(KTR_CXGBE, "%s: map_sg failed\n", __func__);
		return 0;
	}
	sge = tdata->sgl;

	tdata->sc_ddp_tag = *ttt;
	if (cxgbei_ulp2_sw_tag_usable(&odev->d_tag_format, tdata->sc_ddp_tag)) {
		err = t4_sk_ddp_tag_reserve(isock, xferlen, sge, tdata->nsge,
						&tdata->sc_ddp_tag);
	} else {
		CTR2(KTR_CXGBE, "%s: sc_ddp_tag:0x%x not usable\n",
				__func__, tdata->sc_ddp_tag);
	}
out:
	if (err < 0)
		tdata->sc_ddp_tag =
			cxgbei_ulp2_set_non_ddp_tag(&odev->d_tag_format, *ttt);
	return tdata->sc_ddp_tag;
}

static int
t4_sk_ddp_tag_release(iscsi_socket *isock, unsigned int ddp_tag)
{
	offload_device *odev = isock->s_odev;
	struct toedev *tdev = odev->d_tdev;

	cxgbei_ulp2_ddp_tag_release(odev->tdev2ddp(tdev), ddp_tag, isock);
	return 0;
}
#ifdef T4_DDP
static struct cxgbei_ulp2_ddp_info *
t4_ddp_init(struct ifnet *dev, struct toedev *tdev)
{
	struct cxgbei_ulp2_ddp_info *ddp;
	struct adapter *sc = tdev->tod_softc;
	struct ulp_iscsi_info uinfo;

	memset(&uinfo, 0, sizeof(struct ulp_iscsi_info));
	uinfo.llimit = sc->vres.iscsi.start;
	uinfo.ulimit = sc->vres.iscsi.start + sc->vres.iscsi.size - 1;
	uinfo.max_rxsz = uinfo.max_txsz =
				G_MAXRXDATA(t4_read_reg(sc, A_TP_PARA_REG2));

	if (sc->vres.iscsi.size == 0) {
		printf("%s: iSCSI capabilities not enabled.\n", __func__);
		return NULL;
	}
	printf("T4, ddp 0x%x ~ 0x%x, size %u, iolen %u, ulpddp:0x%p\n",
		uinfo.llimit, uinfo.ulimit, sc->vres.iscsi.size,
		uinfo.max_rxsz, sc->iscsi_softc);

	cxgbei_ulp2_ddp_init((void *)tdev,
			(struct cxgbei_ulp2_ddp_info **)&sc->iscsi_softc,
			&uinfo);
	ddp = (struct cxgbei_ulp2_ddp_info *)sc->iscsi_softc;
	if (ddp) {
		unsigned int pgsz_order[4];
		int i;

		for (i = 0; i < 4; i++)
			pgsz_order[i] = uinfo.pgsz_factor[i];

		t4_iscsi_init(dev, uinfo.tagmask, pgsz_order);

		ddp->ddp_set_map = t4_ddp_set_map;
		ddp->ddp_clear_map = t4_ddp_clear_map;
	}
	return ddp;
}
#endif

static struct socket *
cpl_find_sock(struct adapter *sc,  unsigned int hwtid)
{
	struct socket *sk;
	struct toepcb *toep = lookup_tid(sc, hwtid);
	struct inpcb *inp = toep->inp;

	INP_WLOCK(inp);
	sk = inp->inp_socket;
	INP_WUNLOCK(inp);
	if (sk == NULL)
		CTR2(KTR_CXGBE,
			"%s: T4 CPL tid 0x%x, sk NULL.\n", __func__, hwtid);
	return sk;
}

static void
process_rx_iscsi_hdr(struct socket *sk, struct mbuf *m)
{
	struct tcpcb *tp = so_sototcpcb(sk);
        struct toepcb *toep = tp->t_toe;

	struct cpl_iscsi_hdr *cpl =  mtod(m, struct cpl_iscsi_hdr *);
	struct ulp_mbuf_cb *cb, *lcb;
	struct mbuf *lmbuf;
	unsigned char *byte;
	iscsi_socket *isock = (iscsi_socket *)(sk)->so_emuldata;
	unsigned int hlen, dlen, plen;

	if (isock == NULL)
		goto err_out;

	if (toep == NULL)
		goto err_out;
	if ((m->m_flags & M_PKTHDR) == 0) {
		printf("%s: m:%p no M_PKTHDR can't allocate m_tag\n",
				__func__, m);
		goto err_out;
	}

	mtx_lock(&isock->iscsi_rcv_mbufq.lock);

	/* allocate m_tag to hold ulp info */
	cb = get_ulp_mbuf_cb(m);
	if (cb == NULL) {
		printf("%s: Error allocation m_tag\n", __func__);
		goto err_out1;
	}
	cb->seq = ntohl(cpl->seq);

	/* strip off CPL header */
	m_adj(m, sizeof(*cpl));

	/* figure out if this is the pdu header or data */
	cb->ulp_mode = ULP_MODE_ISCSI;
	if (isock->mbuf_ulp_lhdr == NULL) {
		iscsi_socket *isock = (iscsi_socket *)(sk)->so_emuldata;

		isock->mbuf_ulp_lhdr = lmbuf = m;
		lcb = cb;
		cb->flags = SBUF_ULP_FLAG_HDR_RCVD |
			SBUF_ULP_FLAG_COALESCE_OFF;
		/* we only update tp->rcv_nxt once per pdu */
		if (cb->seq != tp->rcv_nxt) {
			CTR3(KTR_CXGBE,
			"tid 0x%x, CPL_ISCSI_HDR, BAD seq got 0x%x exp 0x%x.\n",
			toep->tid, cb->seq, tp->rcv_nxt);
			goto err_out1;
		}
		byte = m->m_data;
		hlen = ntohs(cpl->len);
		dlen = ntohl(*(unsigned int *)(byte + 4)) & 0xFFFFFF;

		plen = ntohs(cpl->pdu_len_ddp);
		lcb->ulp.iscsi.pdulen = (hlen + dlen + 3) & (~0x3);
		/* workaround for cpl->pdu_len_ddp since it does not include
		the data digest count */
		if (dlen)
			lcb->ulp.iscsi.pdulen += isock->s_dcrc_len;

		tp->rcv_nxt += lcb->ulp.iscsi.pdulen;
		if (tp->rcv_wnd <= lcb->ulp.iscsi.pdulen)
			CTR3(KTR_CXGBE, "%s: Neg rcv_wnd:0x%lx pdulen:0x%x\n",
				__func__, tp->rcv_wnd, lcb->ulp.iscsi.pdulen);
			tp->rcv_wnd -= lcb->ulp.iscsi.pdulen;
			tp->t_rcvtime = ticks;
	} else {
		lmbuf = isock->mbuf_ulp_lhdr;
		lcb = find_ulp_mbuf_cb(lmbuf);
		if (lcb == NULL) {
			printf("%s: lmbuf:%p lcb is NULL\n", __func__, lmbuf);
			goto err_out1;
		}
		lcb->flags |= SBUF_ULP_FLAG_DATA_RCVD |
			SBUF_ULP_FLAG_COALESCE_OFF;
		cb->flags = SBUF_ULP_FLAG_DATA_RCVD;

		/* padding */
		if ((m->m_len % 4) != 0) {
			m->m_len += 4 - (m->m_len % 4);
		}
	}
	mbufq_tail(&isock->iscsi_rcv_mbufq, m);
	mtx_unlock(&isock->iscsi_rcv_mbufq.lock);
	return;

err_out1:
	mtx_unlock(&isock->iscsi_rcv_mbufq.lock);
err_out:
	m_freem(m);
	return;
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
	m = mbufq_peek(&isock->iscsi_rcv_mbufq);
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
	mbufq_dequeue(&isock->iscsi_rcv_mbufq);
	data_len = cb->ulp.iscsi.pdulen;

	CTR5(KTR_CXGBE, "%s: response:%p m:%p m_len:%d data_len:%d\n",
		__func__, response, m, m->m_len, data_len);
	response->ip_bhs_mbuf = m;
	response->ip_bhs = mtod(response->ip_bhs_mbuf, struct iscsi_bhs *);

	/* data */
	if (cb->flags & SBUF_ULP_FLAG_DATA_RCVD) {
		m = mbufq_peek(&isock->iscsi_rcv_mbufq);
		if (m == NULL) {
			CTR1(KTR_CXGBE, "%s:No Data\n", __func__);
			goto err_out;
		}
		mbufq_dequeue(&isock->iscsi_rcv_mbufq);
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
process_rx_data_ddp(struct socket *sk, void *m)
{
	struct cpl_rx_data_ddp *cpl = (struct cpl_rx_data_ddp *)m;
	struct tcpcb *tp = so_sototcpcb(sk);
	struct toepcb *toep = tp->t_toe;
	struct inpcb *inp = toep->inp;
	struct mbuf *lmbuf;
	struct ulp_mbuf_cb *lcb, *lcb1;
	unsigned int val, pdulen;
	iscsi_socket *isock = (iscsi_socket *)(sk)->so_emuldata;

	if (isock == NULL)
		return;

	if (isock->mbuf_ulp_lhdr == NULL) {
		panic("%s: tid 0x%x, rcv RX_DATA_DDP w/o pdu header.\n",
				__func__, toep->tid);
		return;
	}
	mtx_lock(&isock->iscsi_rcv_mbufq.lock);
	lmbuf = isock->mbuf_ulp_lhdr;
	if (lmbuf->m_nextpkt) {
		lcb1 = find_ulp_mbuf_cb(lmbuf->m_nextpkt);
		lcb1->flags |= SBUF_ULP_FLAG_STATUS_RCVD;
	}
	lcb = find_ulp_mbuf_cb(isock->mbuf_ulp_lhdr);
	if (lcb == NULL) {
		CTR2(KTR_CXGBE, "%s: mtag NULL lmbuf :%p\n", __func__, lmbuf);
		mtx_unlock(&isock->iscsi_rcv_mbufq.lock);
		return;
	}
	lcb->flags |= SBUF_ULP_FLAG_STATUS_RCVD;
	isock->mbuf_ulp_lhdr = NULL;

	if (ntohs(cpl->len) != lcb->ulp.iscsi.pdulen) {
		CTR3(KTR_CXGBE, "tid 0x%x, RX_DATA_DDP pdulen %u != %u.\n",
			toep->tid, ntohs(cpl->len), lcb->ulp.iscsi.pdulen);
		CTR4(KTR_CXGBE, "%s: lmbuf:%p lcb:%p lcb->flags:0x%x\n",
			__func__, lmbuf, lcb, lcb->flags);
	}

	lcb->ulp.iscsi.ddigest = ntohl(cpl->ulp_crc);
	pdulen = lcb->ulp.iscsi.pdulen;

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
	mtx_unlock(&isock->iscsi_rcv_mbufq.lock);

	/* update rx credits */
	INP_WLOCK(inp);
	SOCK_LOCK(sk);
	toep->sb_cc += pdulen;
	SOCK_UNLOCK(sk);
	CTR4(KTR_CXGBE, "sk:%p sb_cc 0x%x, rcv_nxt 0x%x rcv_wnd:0x%lx.\n",
			sk, toep->sb_cc, tp->rcv_nxt, tp->rcv_wnd);
	t4_rcvd(&toep->td->tod, tp);
	INP_WUNLOCK(inp);
	return;
}

static void
drop_fw_acked_ulp_data(struct socket *sk, struct toepcb *toep, int len)
{
	struct mbuf *m, *next;
	struct ulp_mbuf_cb *cb;
	iscsi_socket *isock = (iscsi_socket *)(sk)->so_emuldata;
	struct icl_pdu *req;

	if (len == 0 || (isock == NULL))
		return;

	mtx_lock(&isock->ulp2_wrq.lock);
	while (len > 0) {
		m = mbufq_dequeue(&isock->ulp2_wrq);
		if(m == NULL) break;

		for(next = m; next !=NULL; next = next->m_next)
			len -= next->m_len;

		cb = find_ulp_mbuf_cb(m);

		if (cb && isock && cb->pdu) {
			req = (struct icl_pdu *)cb->pdu;
			req->ip_bhs_mbuf = NULL;
			icl_pdu_free(req);
		}
		m_freem(m);
	}
	mtx_unlock(&isock->ulp2_wrq.lock);
	return;
}

static void
process_fw4_ack(struct socket *sk, int *plen)
{
	struct tcpcb *tp = so_sototcpcb(sk);
	struct toepcb *toep = tp->t_toe;

	drop_fw_acked_ulp_data(sk, toep, *plen);

	return;
}

static int
do_set_tcb_rpl(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	return 0;
}

static int
do_rx_iscsi_hdr(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct socket *sk;
	struct adapter *sc = iq->adapter;
	struct cpl_iscsi_hdr *cpl =  mtod(m, struct cpl_iscsi_hdr *);
	sk = cpl_find_sock(sc, GET_TID(cpl));

	if (sk == NULL)
		return CPL_RET_UNKNOWN_TID | CPL_RET_BUF_DONE;

	process_rx_iscsi_hdr(sk, m);
	return 0;
}

static int
do_rx_data_ddp(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	return 0;
}

static int
do_rx_iscsi_ddp(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct socket *sk;
	struct adapter *sc;
	const struct cpl_rx_iscsi_ddp *cpl = (const void *)(rss + 1);

	if (iq == NULL)
		return 0;
	sc = iq->adapter;
	if (sc == NULL)
		return 0;

	sk = cpl_find_sock(sc, GET_TID(cpl));
	if (sk == NULL)
		return CPL_RET_UNKNOWN_TID | CPL_RET_BUF_DONE;

	process_rx_data_ddp(sk, (void *)cpl);
	return 0;
}
static int
t4_ulp_mbuf_push(struct socket *so, struct mbuf *m)
{
	struct tcpcb *tp = so_sototcpcb(so);
	struct toepcb *toep = tp->t_toe;
	struct inpcb *inp = so_sotoinpcb(so);
	iscsi_socket *isock = (iscsi_socket *)(so)->so_emuldata;;

	if (isock == NULL) {
		m_freem(m);
		return EINVAL;
	}

	/* append mbuf to ULP queue */
	mtx_lock(&isock->ulp2_writeq.lock);
	mbufq_tail(&isock->ulp2_writeq, m);
	mtx_unlock(&isock->ulp2_writeq.lock);

	INP_WLOCK(inp);
	t4_ulp_push_frames(toep->td->tod.tod_softc, toep, 0);
	INP_WUNLOCK(inp);
	return 0;
}

static struct mbuf *
iscsi_queue_handler_callback(struct socket *sk, unsigned int cmd, int *qlen)
{
	iscsi_socket *isock;
	struct mbuf *m0 = NULL;

	if (sk == NULL)
		return NULL;
	isock = (iscsi_socket *)(sk)->so_emuldata;
	if (isock == NULL)
		return NULL;

	switch (cmd) {
		case 0:/* PEEK */
			m0 = mbufq_peek(&isock->ulp2_writeq);
		break;
		case 1:/* QUEUE_LEN */
			*qlen = mbufq_len(&isock->ulp2_writeq);
			m0 = mbufq_peek(&isock->ulp2_writeq);
		break;
		case 2:/* DEQUEUE */
			mtx_lock(&isock->ulp2_writeq.lock);
			m0 = mbufq_dequeue(&isock->ulp2_writeq);
			mtx_unlock(&isock->ulp2_writeq.lock);

			mtx_lock(&isock->ulp2_wrq.lock);
			mbufq_tail(&isock->ulp2_wrq, m0);
			mtx_unlock(&isock->ulp2_wrq.lock);

			m0 = mbufq_peek(&isock->ulp2_writeq);
		break;
	}
	return m0;
}

static void
iscsi_cpl_handler_callback(struct tom_data *td, struct socket *sk,
                                        void *m, unsigned int op)
{
	if ((sk == NULL) || (sk->so_emuldata == NULL))
		return;

	switch (op) {
		case CPL_ISCSI_HDR:
			process_rx_iscsi_hdr(sk, m);
		break;
		case CPL_RX_DATA_DDP:
			process_rx_data_ddp(sk, m);
		break;
		case CPL_SET_TCB_RPL:
		break;
		case CPL_FW4_ACK:
			process_fw4_ack(sk, m);
		break;
		default:
		CTR2(KTR_CXGBE, "sk 0x%p, op 0x%x from TOM, NOT supported.\n",
					sk, op);
		break;
	}
}

static void
t4_register_cpl_handler_with_tom(struct adapter *sc)
{
	t4tom_register_cpl_iscsi_callback(iscsi_cpl_handler_callback);
	t4tom_register_queue_iscsi_callback(iscsi_queue_handler_callback);
	t4_register_cpl_handler(sc, CPL_ISCSI_HDR, do_rx_iscsi_hdr);
	t4_register_cpl_handler(sc, CPL_ISCSI_DATA, do_rx_iscsi_hdr);
	t4tom_cpl_handler_register_flag |=
		 1 << TOM_CPL_ISCSI_HDR_REGISTERED_BIT;

	if (!t4tom_cpl_handler_registered(sc, CPL_SET_TCB_RPL)) {
		t4_register_cpl_handler(sc, CPL_SET_TCB_RPL, do_set_tcb_rpl);
		t4tom_cpl_handler_register_flag |=
			1 << TOM_CPL_SET_TCB_RPL_REGISTERED_BIT;
		CTR0(KTR_CXGBE, "register t4 cpl handler CPL_SET_TCB_RPL.\n");
	}

	t4_register_cpl_handler(sc, CPL_RX_ISCSI_DDP, do_rx_iscsi_ddp);

	if (!t4tom_cpl_handler_registered(sc, CPL_RX_DATA_DDP)) {
		t4_register_cpl_handler(sc, CPL_RX_DATA_DDP, do_rx_data_ddp);
		t4tom_cpl_handler_register_flag |=
			1 << TOM_CPL_RX_DATA_DDP_REGISTERED_BIT;
		CTR0(KTR_CXGBE, "register t4 cpl handler CPL_RX_DATA_DDP.\n");
	}
}

static void
t4_unregister_cpl_handler_with_tom(struct adapter *sc)
{
	/* de-register CPL handles */
	t4tom_register_cpl_iscsi_callback(NULL);
	t4tom_register_queue_iscsi_callback(NULL);
	if (t4tom_cpl_handler_register_flag &
		(1 << TOM_CPL_ISCSI_HDR_REGISTERED_BIT)) {
		t4_register_cpl_handler(sc, CPL_ISCSI_HDR, NULL);
		t4_register_cpl_handler(sc, CPL_ISCSI_DATA, NULL);
	}
	if (t4tom_cpl_handler_register_flag &
		(1 << TOM_CPL_SET_TCB_RPL_REGISTERED_BIT))
		t4_register_cpl_handler(sc, CPL_SET_TCB_RPL, NULL);
	t4_register_cpl_handler(sc, CPL_RX_ISCSI_DDP, NULL);
	if (t4tom_cpl_handler_register_flag &
		(1 << TOM_CPL_RX_DATA_DDP_REGISTERED_BIT))
		t4_register_cpl_handler(sc, CPL_RX_DATA_DDP, NULL);
}

static int
send_set_tcb_field(struct socket *sk, u16 word, u64 mask, u64 val,
                                int no_reply)
{
	struct wrqe *wr;
	struct cpl_set_tcb_field *req;
	struct inpcb *inp = sotoinpcb(sk);
	struct tcpcb *tp = intotcpcb(inp);
	struct toepcb *toep = tp->t_toe;

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
	return 0;
}

static int
cxgbei_set_ulp_mode(struct socket *so, struct toepcb *toep,
				unsigned char hcrc, unsigned char dcrc)
{
	int rv = 0, val = 0;

	toep->ulp_mode = ULP_MODE_ISCSI;
	if (hcrc)
		val |= ULP_CRC_HEADER;
	if (dcrc)
		val |= ULP_CRC_DATA;
	val <<= 4;
	val |= ULP_MODE_ISCSI;
	rv = send_set_tcb_field(so, 0, 0xfff, val, 0);
	return rv;
}

static offload_device *
add_cxgbei_dev(struct ifnet *dev, struct toedev *tdev)
{
#ifdef T4_DDP
	struct cxgbei_ulp2_ddp_info *ddp;
#endif
	offload_device *odev = NULL;
	odev = offload_device_new(tdev);
	if (odev == NULL) {
		printf("%s: odev is NULL\n", __func__);
		return odev;
	}
	printf("%s:New T4 %s, tdev 0x%p, odev 0x%p.\n",
			__func__, dev->if_xname, tdev, odev);
	odev->d_tdev = tdev;
	odev->d_ulp_rx_datagap = sizeof(struct cpl_iscsi_hdr_no_rss);
	odev->d_flag = ODEV_FLAG_ULP_CRC_ENABLED;

#ifdef T4_DDP
	odev->tdev2ddp = t4_tdev2ddp;
	ddp = t4_ddp_init(dev, tdev);
	if (ddp) {
		printf("T4 %s, odev 0x%p, ddp 0x%p initialized.\n",
			dev->if_xname, odev, ddp);

		odev->d_flag |= ODEV_FLAG_ULP_DDP_ENABLED;
		cxgbei_ulp2_adapter_ddp_info(ddp,
			(struct cxgbei_ulp2_tag_format *)&odev->d_tag_format,
			&odev->d_payload_tmax, &odev->d_payload_rmax);
	}
#endif
	return odev;
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
	cxgbei_task_data *tdata = NULL;
	struct socket *so = NULL;
	iscsi_socket *isock = NULL;
	offload_device *odev = NULL;

	if (ic->ic_socket == NULL) return;

	so = ic->ic_socket;

	isock = (iscsi_socket *)(so)->so_emuldata;
	if (isock == NULL) return;
	odev = isock->s_odev;

	tdata = (cxgbei_task_data *)(ofld_priv);
	if (tdata == NULL) return;

	if (cxgbei_ulp2_is_ddp_tag(&odev->d_tag_format, tdata->sc_ddp_tag))
		t4_sk_ddp_tag_release(isock, tdata->sc_ddp_tag);
	memset(tdata, 0, sizeof(*tdata));
	return;
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
cxgbei_conn_xmit_pdu(void *conn, void *ioreq)
{
	struct icl_conn *ic = (struct icl_conn *)conn;
	struct icl_pdu *req = (struct icl_pdu *)ioreq;
	struct mbuf *m = req->ip_bhs_mbuf;
	struct socket *so = ic->ic_socket;
	struct tcpcb *tp = so_sototcpcb(so);

	t4_sk_tx_mbuf_setmode(req, tp->t_toe, m, 2,
		ic->ic_header_crc32c ? ISCSI_HEADER_DIGEST_SIZE : 0,
	(req->ip_data_len && ic->ic_data_crc32c) ? ISCSI_DATA_DIGEST_SIZE : 0);

	t4_ulp_mbuf_push(ic->ic_socket, m);
	return 0;
}

/* called from host iscsi, socket is passed as argument  */
int
cxgbei_conn_set_ulp_mode(struct socket *so, void *conn)
{
	struct tcpcb *tp = so_sototcpcb(so);
	struct toepcb *toep = tp->t_toe;
	struct adapter *sc = NULL;
	struct toedev *tdev = NULL;
	iscsi_socket *isock = NULL;
	struct ifnet *ifp = NULL;
	unsigned int tid = toep->tid;
	offload_device *odev = NULL;
	struct icl_conn *ic = (struct icl_conn*)conn;

	if (toep == NULL) return EINVAL;

	ifp = toep->port->ifp;
	if (ifp == NULL) return EINVAL;

	if (!(sototcpcb(so)->t_flags & TF_TOE) ||
                !(ifp->if_capenable & IFCAP_TOE)) {
		printf("%s: TOE not enabled on:%s\n", __func__, ifp->if_xname);
		return EINVAL;
	}

	/* if ULP_MODE is set by TOE driver, treat it as non-offloaded */
	if (toep->ulp_mode) {
		CTR3(KTR_CXGBE, "%s: T4 sk 0x%p, ulp mode already set 0x%x.\n",
				__func__, so, toep->ulp_mode);
		return EINVAL;
	}
	sc = toep->port->adapter;
	tdev = &toep->td->tod;
	/* if toe dev is not set, treat it as non-offloaded */
	if (tdev == NULL) {
		CTR2(KTR_CXGBE, "%s: T4 sk 0x%p, tdev NULL.\n", __func__, so);
		return EINVAL;
	}

	isock = (iscsi_socket *)malloc(sizeof(iscsi_socket), M_CXGBE,
				M_NOWAIT | M_ZERO);
	if (isock == NULL) {
		printf("%s: T4 sk 0x%p, isock alloc failed.\n", __func__, so);
		return EINVAL;
	}
	isock->mbuf_ulp_lhdr = NULL;
	isock->sock = so;
	isock->s_conn = conn;
	so->so_emuldata = isock;

	mtx_init(&isock->iscsi_rcv_mbufq.lock,"isock_lock" , NULL, MTX_DEF);
	mtx_init(&isock->ulp2_wrq.lock,"ulp2_wrq lock" , NULL, MTX_DEF);
	mtx_init(&isock->ulp2_writeq.lock,"ulp2_writeq lock" , NULL, MTX_DEF);

	CTR6(KTR_CXGBE,
		"%s: sc:%p toep:%p iscsi_start:0x%x iscsi_size:0x%x caps:%d.\n",
		__func__, sc, toep, sc->vres.iscsi.start,
		sc->vres.iscsi.size, sc->iscsicaps);
	/*
	 * Register ULP CPL handlers with TOM
	 * Register CPL_RX_ISCSI_HDR, CPL_RX_DATA_DDP callbacks with TOM
	 */
	t4_register_cpl_handler_with_tom(sc);

	/*
	 * DDP initialization. Once for each tdev
	 * check if DDP is already configured for this tdev
	 */
	odev = offload_device_find(tdev);
	if (odev == NULL) /* for each tdev we have a corresponding odev */
	{
		if ((odev = add_cxgbei_dev(ifp, tdev)) == NULL) {
			CTR3(KTR_CXGBE,
				"T4 sk 0x%p, tdev %s, 0x%p, odev NULL.\n",
				so, ifp->if_xname, tdev);
			return EINVAL;
		}
	}

	CTR3(KTR_CXGBE, "tdev:%p sc->iscsi_softc:%p odev:%p\n",
			tdev, sc->iscsi_softc, odev);
	isock->s_odev = odev;
	isock->s_tid = tid;

	isock->s_rmax = odev->d_payload_rmax;
	isock->s_tmax = odev->d_payload_tmax;

	/* XXX cap the xmit pdu size to be 12K for now until f/w is ready */
	if (isock->s_tmax > (12288 + ISCSI_PDU_NONPAYLOAD_LEN))
		isock->s_tmax = 12288 + ISCSI_PDU_NONPAYLOAD_LEN;

	/* set toe DDP off */
	so->so_options |= SO_NO_DDP;

	/* Move connection to ULP mode, SET_TCB_FIELD */
	cxgbei_set_ulp_mode(so, toep,
		ic->ic_header_crc32c, ic->ic_data_crc32c);

	isock->s_hcrc_len = (ic->ic_header_crc32c ? 4 : 0);
	isock->s_dcrc_len = (ic->ic_data_crc32c ? 4 : 0);
	return 0;
}

int
cxgbei_conn_close(struct socket *so)
{
	iscsi_socket *isock = NULL;
	isock = (iscsi_socket *)(so)->so_emuldata;
	struct mbuf *m;
	struct ulp_mbuf_cb *cb;
	struct icl_pdu *req;

	so->so_emuldata = NULL;

	/* free isock Qs */
	while ((m = mbufq_dequeue(&isock->iscsi_rcv_mbufq)) != NULL)
		m_freem(m);

	while ((m = mbufq_dequeue(&isock->ulp2_writeq)) != NULL)
		m_freem(m);

	mtx_lock(&isock->ulp2_wrq.lock);
	while ((m = mbufq_dequeue(&isock->ulp2_wrq)) != NULL) {
		cb = find_ulp_mbuf_cb(m);
		if (cb && isock && cb->pdu) {
			req = (struct icl_pdu *)cb->pdu;
			req->ip_bhs_mbuf = NULL;
			icl_pdu_free(req);
		}
		m_freem(m);
	}
	mtx_unlock(&isock->ulp2_wrq.lock);

	if (mtx_initialized(&isock->iscsi_rcv_mbufq.lock))
		mtx_destroy(&isock->iscsi_rcv_mbufq.lock);

	if (mtx_initialized(&isock->ulp2_wrq.lock))
		mtx_destroy(&isock->ulp2_wrq.lock);

	if (mtx_initialized(&isock->ulp2_writeq.lock))
		mtx_destroy(&isock->ulp2_writeq.lock);

	free(isock, M_CXGBE);

	return 0;
}

static int
cxgbei_loader(struct module *mod, int cmd, void *arg)
{
	int err = 0;

	switch (cmd) {
	case MOD_LOAD:
		SLIST_INIT(&odev_list);
		printf("cxgbei module loaded Sucessfully.\n");
		break;
	case MOD_UNLOAD:
		offload_device_remove();
		printf("cxgbei cleanup completed sucessfully.\n");
		break;
	default:
		err = (EINVAL);
		break;
	}

	return (err);
}

static moduledata_t cxgbei_mod = {
	"cxgbei",
	cxgbei_loader,
	NULL,
};

MODULE_VERSION(cxgbei, 1);
DECLARE_MODULE(cxgbei, cxgbei_mod, SI_SUB_EXEC, SI_ORDER_ANY);
MODULE_DEPEND(cxgbei, t4_tom, 1, 1, 1);
MODULE_DEPEND(cxgbei, cxgbe, 1, 1, 1);
MODULE_DEPEND(cxgbei, icl, 1, 1, 1);
