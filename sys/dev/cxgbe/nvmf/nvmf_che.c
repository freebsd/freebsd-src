/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
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

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/libkern.h>
#include <sys/kernel.h>
#include <sys/module.h>

#ifdef TCP_OFFLOAD
#include <sys/bitset.h>
#include <sys/capsicum.h>
#include <sys/file.h>
#include <sys/kthread.h>
#include <sys/ktr.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/nv.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp_var.h>
#include <netinet/toecore.h>

#include <dev/nvmf/nvmf.h>
#include <dev/nvmf/nvmf_proto.h>
#include <dev/nvmf/nvmf_tcp.h>
#include <dev/nvmf/nvmf_transport.h>
#include <dev/nvmf/nvmf_transport_internal.h>

#include <vm/pmap.h>
#include <vm/vm_page.h>

#include "common/common.h"
#include "common/t4_regs.h"
#include "common/t4_tcb.h"
#include "tom/t4_tom.h"

/* Status code values in CPL_NVMT_CMP. */
#define	CMP_STATUS_ERROR_MASK		0x7f
#define	CMP_STATUS_NO_ERROR		0
#define	CMP_STATUS_HEADER_DIGEST	1
#define	CMP_STATUS_DIRECTION_MISMATCH	2
#define	CMP_STATUS_DIGEST_FLAG_MISMATCH	3
#define	CMP_STATUS_SUCCESS_NOT_LAST	4
#define	CMP_STATUS_BAD_DATA_LENGTH	5
#define	CMP_STATUS_USER_MODE_UNALLOCATED	6
#define	CMP_STATUS_RQT_LIMIT		7
#define	CMP_STATUS_RQT_WRAP		8
#define	CMP_STATUS_RQT_BOUND		9
#define	CMP_STATUS_TPT_LIMIT		16
#define	CMP_STATUS_TPT_INVALID		17
#define	CMP_STATUS_TPT_COLOUR_MISMATCH	18
#define	CMP_STATUS_TPT_MISC		19
#define	CMP_STATUS_TPT_WRAP		20
#define	CMP_STATUS_TPT_BOUND		21
#define	CMP_STATUS_TPT_LAST_PDU_UNALIGNED	22
#define	CMP_STATUS_PBL_LIMIT		24
#define	CMP_STATUS_DATA_DIGEST		25
#define	CMP_STATUS_DDP			0x80

/*
 * Transfer tags and CIDs with the MSB set are "unallocated" tags that
 * pass data through to the freelist without using DDP.
 */
#define	CHE_FL_TAG_MASK		0x8000
#define	CHE_MAX_FL_TAG		0x7fff
#define	CHE_NUM_FL_TAGS		(CHE_MAX_FL_TAG + 1)

#define	CHE_TAG_IS_FL(ttag)	(((ttag) & CHE_FL_TAG_MASK) == CHE_FL_TAG_MASK)
#define	CHE_RAW_FL_TAG(ttag)	((ttag) & ~CHE_FL_TAG_MASK)
#define	CHE_DDP_TAG(stag_idx, color)	((stag_idx) << 4 | (color))
#define	CHE_STAG_COLOR(stag)	((stag) & 0xf)
#define	CHE_STAG_IDX(stag)	((stag) >> 4)
#define	CHE_DDP_MAX_COLOR	0xf

#define	CHE_DDP_NO_TAG		0xffff

/*
 * A bitmap of non-DDP CIDs in use on the host.  Since there is no
 * _BIT_FFC (find first clear), the bitset is inverted so that a clear
 * bit indicates an in-use CID.
 */
BITSET_DEFINE(fl_cid_set, CHE_NUM_FL_TAGS);
#define	FL_CID_INIT(p)		__BIT_FILL(CHE_NUM_FL_TAGS, p)
#define	FL_CID_BUSY(n, p)	__BIT_CLR(CHE_NUM_FL_TAGS, n, p)
#define	FL_CID_ISACTIVE(n, p)	!__BIT_ISSET(CHE_NUM_FL_TAGS, n, p)
#define	FL_CID_FREE(n, p)	__BIT_SET(CHE_NUM_FL_TAGS, n, p)
#define	FL_CID_FINDFREE_AT(p, start)	__BIT_FFS_AT(CHE_NUM_FL_TAGS, p, start)

/*
 * The TCP sequence number of both CPL_NVMT_DATA and CPL_NVMT_CMP
 * mbufs are saved here while the mbuf is in qp->rx_data and qp->rx_pdus.
 */
#define	nvmf_tcp_seq	PH_loc.thirtytwo[0]

/*
 * The CPL status of CPL_NVMT_CMP mbufs are saved here while the mbuf
 * is in qp->rx_pdus.
 */
#define	nvmf_cpl_status	PH_loc.eight[4]

struct nvmf_che_capsule;
struct nvmf_che_qpair;

struct nvmf_che_adapter {
	struct adapter *sc;

	u_int ddp_threshold;
	u_int max_transmit_pdu;
	u_int max_receive_pdu;
	bool nvmt_data_iqe;

	struct sysctl_ctx_list ctx;	/* from uld_activate to deactivate */
};

struct nvmf_che_command_buffer {
	struct nvmf_che_qpair *qp;

	struct nvmf_io_request io;
	size_t	data_len;
	size_t	data_xfered;
	uint32_t data_offset;

	u_int	refs;
	int	error;

	bool	ddp_ok;
	uint16_t cid;
	uint16_t ttag;
	uint16_t original_cid;	/* Host only */

	TAILQ_ENTRY(nvmf_che_command_buffer) link;

	/* Fields used for DDP. */
	struct fw_ri_tpte tpte;
	uint64_t *pbl;
	uint32_t pbl_addr;
	uint32_t pbl_len;

	/* Controller only */
	struct nvmf_che_capsule *cc;
};

struct nvmf_che_command_buffer_list {
	TAILQ_HEAD(, nvmf_che_command_buffer) head;
	struct mtx lock;
};

struct nvmf_che_qpair {
	struct nvmf_qpair qp;

	struct socket *so;
	struct toepcb *toep;
	struct nvmf_che_adapter *nca;

	volatile u_int refs;	/* Every allocated capsule holds a reference */
	uint8_t	txpda;
	uint8_t rxpda;
	bool header_digests;
	bool data_digests;
	uint32_t maxr2t;
	uint32_t maxh2cdata;	/* Controller only */
	uint32_t max_rx_data;
	uint32_t max_tx_data;
	uint32_t max_icd;	/* Host only */
	uint32_t max_ioccsz;	/* Controller only */
	union {
		uint16_t next_fl_ttag;	/* Controller only */
		uint16_t next_cid;	/* Host only */
	};
	uint16_t next_ddp_tag;
	u_int num_fl_ttags;	/* Controller only */
	u_int active_fl_ttags;	/* Controller only */
	u_int num_ddp_tags;
	u_int active_ddp_tags;
	bool send_success;	/* Controller only */
	uint8_t ddp_color;
	uint32_t tpt_offset;

	/* Receive state. */
	struct thread *rx_thread;
	struct cv rx_cv;
	bool	rx_shutdown;
	int	rx_error;
	struct mbufq rx_data;	/* Data received via CPL_NVMT_DATA. */
	struct mbufq rx_pdus;	/* PDU headers received via CPL_NVMT_CMP. */

	/* Transmit state. */
	struct thread *tx_thread;
	struct cv tx_cv;
	bool	tx_shutdown;
	STAILQ_HEAD(, nvmf_che_capsule) tx_capsules;

	struct nvmf_che_command_buffer_list tx_buffers;
	struct nvmf_che_command_buffer_list rx_buffers;

	/*
	 * For the controller, an RX command buffer can be in one of
	 * three locations, all protected by the rx_buffers.lock.  If
	 * a receive request is waiting for either an R2T slot for its
	 * command (due to exceeding MAXR2T), or a transfer tag it is
	 * placed on the rx_buffers list.  When a request is allocated
	 * an active transfer tag, it moves to either the
	 * open_ddp_tags[] or open_fl_ttags[] array (indexed by the
	 * tag) until it completes.
	 *
	 * For the host, an RX command buffer using DDP is in
	 * open_ddp_tags[], otherwise it is in rx_buffers.
	 */
	struct nvmf_che_command_buffer **open_ddp_tags;
	struct nvmf_che_command_buffer **open_fl_ttags;	/* Controller only */

	/*
	 * For the host, CIDs submitted by nvmf(4) must be rewritten
	 * to either use DDP or not use DDP.  The CID in response
	 * capsules must be restored to their original value.  For
	 * DDP, the original CID is stored in the command buffer.
	 * These variables manage non-DDP CIDs.
	 */
	uint16_t *fl_cids;		/* Host only */
	struct fl_cid_set *fl_cid_set;	/* Host only */
	struct mtx fl_cid_lock;		/* Host only */
};

struct nvmf_che_rxpdu {
	struct mbuf *m;
	const struct nvme_tcp_common_pdu_hdr *hdr;
	uint32_t data_len;
	bool data_digest_mismatch;
	bool ddp;
};

struct nvmf_che_capsule {
	struct nvmf_capsule nc;

	volatile u_int refs;

	struct nvmf_che_rxpdu rx_pdu;

	uint32_t active_r2ts;		/* Controller only */
#ifdef INVARIANTS
	uint32_t tx_data_offset;	/* Controller only */
	u_int pending_r2ts;		/* Controller only */
#endif

	STAILQ_ENTRY(nvmf_che_capsule) link;
};

#define	CCAP(nc)	((struct nvmf_che_capsule *)(nc))
#define	CQP(qp)		((struct nvmf_che_qpair *)(qp))

static void	che_release_capsule(struct nvmf_che_capsule *cc);
static void	che_free_qpair(struct nvmf_qpair *nq);

SYSCTL_NODE(_kern_nvmf, OID_AUTO, che, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "Chelsio TCP offload transport");

static u_int che_max_transmit_pdu = 32 * 1024;
SYSCTL_UINT(_kern_nvmf_che, OID_AUTO, max_transmit_pdu, CTLFLAG_RWTUN,
    &che_max_transmit_pdu, 0,
    "Maximum size of a transmitted PDU");

static u_int che_max_receive_pdu = 32 * 1024;
SYSCTL_UINT(_kern_nvmf_che, OID_AUTO, max_receive_pdu, CTLFLAG_RWTUN,
    &che_max_receive_pdu, 0,
    "Maximum size of a received PDU");

static int use_dsgl = 1;
SYSCTL_INT(_kern_nvmf_che, OID_AUTO, use_dsgl, CTLFLAG_RWTUN, &use_dsgl, 0,
    "Use DSGL for PBL/FastReg (default=1)");

static int inline_threshold = 256;
SYSCTL_INT(_kern_nvmf_che, OID_AUTO, inline_threshold, CTLFLAG_RWTUN,
    &inline_threshold, 0,
    "inline vs dsgl threshold (default=256)");

static int ddp_tags_per_qp = 128;
SYSCTL_INT(_kern_nvmf_che, OID_AUTO, ddp_tags_per_qp, CTLFLAG_RWTUN,
    &ddp_tags_per_qp, 0,
    "Number of DDP tags to reserve for each queue pair");

static MALLOC_DEFINE(M_NVMF_CHE, "nvmf_che", "Chelsio NVMe-TCP offload");

/*
 * PBL regions consist of N full-sized pages.  TPT entries support an
 * initial offset into the first page (FBO) and can handle a partial
 * length on the last page.
 */
static bool
che_ddp_io_check(struct nvmf_che_qpair *qp, const struct nvmf_io_request *io)
{
	const struct memdesc *mem = &io->io_mem;
	struct bus_dma_segment *ds;
	int i;

	if (io->io_len < qp->nca->ddp_threshold) {
		return (false);
	}

	switch (mem->md_type) {
	case MEMDESC_VADDR:
	case MEMDESC_PADDR:
	case MEMDESC_VMPAGES:
		return (true);
	case MEMDESC_VLIST:
	case MEMDESC_PLIST:
		/*
		 * Require all but the first segment to start on a
		 * page boundary.  Require all but the last segment to
		 * end on a page boundary.
		 */
		ds = mem->u.md_list;
		for (i = 0; i < mem->md_nseg; i++, ds++) {
			if (i != 0 && ds->ds_addr % PAGE_SIZE != 0)
				return (false);
			if (i != mem->md_nseg - 1 &&
			    (ds->ds_addr + ds->ds_len) % PAGE_SIZE != 0)
				return (false);
		}
		return (true);
	default:
		/*
		 * Other types could be validated with more work, but
		 * they aren't used currently by nvmf(4) or nvmft(4).
		 */
		return (false);
	}
}

static u_int
che_fbo(struct nvmf_che_command_buffer *cb)
{
	struct memdesc *mem = &cb->io.io_mem;

	switch (mem->md_type) {
	case MEMDESC_VADDR:
		return ((uintptr_t)mem->u.md_vaddr & PAGE_MASK);
	case MEMDESC_PADDR:
		return (mem->u.md_paddr & PAGE_MASK);
	case MEMDESC_VMPAGES:
		return (mem->md_offset);
	case MEMDESC_VLIST:
	case MEMDESC_PLIST:
		return (mem->u.md_list[0].ds_addr & PAGE_MASK);
	default:
		__assert_unreachable();
	}
}

static u_int
che_npages(struct nvmf_che_command_buffer *cb)
{
	return (howmany(che_fbo(cb) + cb->io.io_len, PAGE_SIZE));
}

static struct nvmf_che_command_buffer *
che_alloc_command_buffer(struct nvmf_che_qpair *qp,
    const struct nvmf_io_request *io, uint32_t data_offset, size_t data_len,
    uint16_t cid)
{
	struct nvmf_che_command_buffer *cb;

	cb = malloc(sizeof(*cb), M_NVMF_CHE, M_WAITOK);
	cb->qp = qp;
	cb->io = *io;
	cb->data_offset = data_offset;
	cb->data_len = data_len;
	cb->data_xfered = 0;
	refcount_init(&cb->refs, 1);
	cb->error = 0;
	cb->ddp_ok = che_ddp_io_check(qp, io);
	cb->cid = cid;
	cb->ttag = 0;
	cb->original_cid = 0;
	cb->cc = NULL;
	cb->pbl = NULL;

	return (cb);
}

static void
che_hold_command_buffer(struct nvmf_che_command_buffer *cb)
{
	refcount_acquire(&cb->refs);
}

static void
che_free_command_buffer(struct nvmf_che_command_buffer *cb)
{
	nvmf_complete_io_request(&cb->io, cb->data_xfered, cb->error);
	if (cb->cc != NULL)
		che_release_capsule(cb->cc);
	MPASS(cb->pbl == NULL);
	free(cb, M_NVMF_CHE);
}

static void
che_release_command_buffer(struct nvmf_che_command_buffer *cb)
{
	if (refcount_release(&cb->refs))
		che_free_command_buffer(cb);
}

static void
che_add_command_buffer(struct nvmf_che_command_buffer_list *list,
    struct nvmf_che_command_buffer *cb)
{
	mtx_assert(&list->lock, MA_OWNED);
	TAILQ_INSERT_HEAD(&list->head, cb, link);
}

static struct nvmf_che_command_buffer *
che_find_command_buffer(struct nvmf_che_command_buffer_list *list,
    uint16_t cid)
{
	struct nvmf_che_command_buffer *cb;

	mtx_assert(&list->lock, MA_OWNED);
	TAILQ_FOREACH(cb, &list->head, link) {
		if (cb->cid == cid)
			return (cb);
	}
	return (NULL);
}

static void
che_remove_command_buffer(struct nvmf_che_command_buffer_list *list,
    struct nvmf_che_command_buffer *cb)
{
	mtx_assert(&list->lock, MA_OWNED);
	TAILQ_REMOVE(&list->head, cb, link);
}

static void
che_purge_command_buffer(struct nvmf_che_command_buffer_list *list,
    uint16_t cid)
{
	struct nvmf_che_command_buffer *cb;

	mtx_lock(&list->lock);
	cb = che_find_command_buffer(list, cid);
	if (cb != NULL) {
		che_remove_command_buffer(list, cb);
		mtx_unlock(&list->lock);
		che_release_command_buffer(cb);
	} else
		mtx_unlock(&list->lock);
}

static int
che_write_mem_inline(struct adapter *sc, struct toepcb *toep, uint32_t addr,
    uint32_t len, void *data, struct mbufq *wrq)
{
	struct mbuf *m;
	char *cp;
	int copy_len, i, num_wqe, wr_len;

#ifdef VERBOSE_TRACES
	CTR(KTR_CXGBE, "%s: addr 0x%x len %u", __func__, addr << 5, len);
#endif
	num_wqe = DIV_ROUND_UP(len, T4_MAX_INLINE_SIZE);
	cp = data;
	for (i = 0; i < num_wqe; i++) {
		copy_len = min(len, T4_MAX_INLINE_SIZE);
		wr_len = T4_WRITE_MEM_INLINE_LEN(copy_len);

		m = alloc_raw_wr_mbuf(wr_len);
		if (m == NULL)
			return (ENOMEM);
		t4_write_mem_inline_wr(sc, mtod(m, void *), wr_len, toep->tid,
		    addr, copy_len, cp, 0);
		if (cp != NULL)
			cp += T4_MAX_INLINE_SIZE;
		addr += T4_MAX_INLINE_SIZE >> 5;
		len -= T4_MAX_INLINE_SIZE;

		mbufq_enqueue(wrq, m);
	}
	return (0);
}

static int
che_write_mem_dma_aligned(struct adapter *sc, struct toepcb *toep,
    uint32_t addr, uint32_t len, void *data, struct mbufq *wrq)
{
	struct mbuf *m;
	vm_offset_t va;
	u_int todo;
	int wr_len;

	/* First page. */
	va = (vm_offset_t)data;
	todo = min(PAGE_SIZE - (va % PAGE_SIZE), len);
	wr_len = T4_WRITE_MEM_DMA_LEN;
	m = alloc_raw_wr_mbuf(wr_len);
	if (m == NULL)
		return (ENOMEM);
	t4_write_mem_dma_wr(sc, mtod(m, void *), wr_len, toep->tid, addr,
	    todo, pmap_kextract(va), 0);
	mbufq_enqueue(wrq, m);
	len -= todo;
	addr += todo >> 5;
	va += todo;

	while (len > 0) {
		MPASS(va == trunc_page(va));
		todo = min(PAGE_SIZE, len);
		m = alloc_raw_wr_mbuf(wr_len);
		if (m == NULL)
			return (ENOMEM);
		t4_write_mem_dma_wr(sc, mtod(m, void *), wr_len, toep->tid,
		    addr, todo, pmap_kextract(va), 0);
		mbufq_enqueue(wrq, m);
		len -= todo;
		addr += todo >> 5;
		va += todo;
	}
	return (0);
}

static int
che_write_adapter_mem(struct nvmf_che_qpair *qp, uint32_t addr, uint32_t len,
    void *data)
{
	struct adapter *sc = qp->nca->sc;
	struct toepcb *toep = qp->toep;
	struct socket *so = qp->so;
	struct inpcb *inp = sotoinpcb(so);
	struct mbufq mq;
	int error;

	mbufq_init(&mq, INT_MAX);
	if (!use_dsgl || len < inline_threshold || data == NULL)
		error = che_write_mem_inline(sc, toep, addr, len, data, &mq);
	else
		error = che_write_mem_dma_aligned(sc, toep, addr, len, data,
		    &mq);
	if (__predict_false(error != 0))
		goto error;

	INP_WLOCK(inp);
	if ((inp->inp_flags & INP_DROPPED) != 0) {
		INP_WUNLOCK(inp);
		error = ECONNRESET;
		goto error;
	}
	mbufq_concat(&toep->ulp_pduq, &mq);
	INP_WUNLOCK(inp);
	return (0);

error:
	mbufq_drain(&mq);
	return (error);
}

static bool
che_alloc_pbl(struct nvmf_che_qpair *qp, struct nvmf_che_command_buffer *cb)
{
	struct adapter *sc = qp->nca->sc;
	struct memdesc *mem = &cb->io.io_mem;
	uint64_t *pbl;
	uint32_t addr, len;
	u_int i, npages;
	int error;

	MPASS(cb->pbl == NULL);
	MPASS(cb->ddp_ok);

	/* Hardware limit?  iWARP only enforces this for T5. */
	if (cb->io.io_len >= (8 * 1024 * 1024 * 1024ULL))
		return (false);

	npages = che_npages(cb);
	len = roundup2(npages, 4) * sizeof(*cb->pbl);
	addr = t4_pblpool_alloc(sc, len);
	if (addr == 0)
		return (false);

	pbl = malloc(len, M_NVMF_CHE, M_NOWAIT | M_ZERO);
	if (pbl == NULL) {
		t4_pblpool_free(sc, addr, len);
		return (false);
	}

	switch (mem->md_type) {
	case MEMDESC_VADDR:
	{
		vm_offset_t va;

		va = trunc_page((uintptr_t)mem->u.md_vaddr);
		for (i = 0; i < npages; i++)
			pbl[i] = htobe64(pmap_kextract(va + i * PAGE_SIZE));
		break;
	}
	case MEMDESC_PADDR:
	{
		vm_paddr_t pa;

		pa = trunc_page(mem->u.md_paddr);
		for (i = 0; i < npages; i++)
			pbl[i] = htobe64(pa + i * PAGE_SIZE);
		break;
	}
	case MEMDESC_VMPAGES:
		for (i = 0; i < npages; i++)
			pbl[i] = htobe64(VM_PAGE_TO_PHYS(mem->u.md_ma[i]));
		break;
	case MEMDESC_VLIST:
	{
		struct bus_dma_segment *ds;
		vm_offset_t va;
		vm_size_t len;
		u_int j, k;

		i = 0;
		ds = mem->u.md_list;
		for (j = 0; j < mem->md_nseg; j++, ds++) {
			va = trunc_page((uintptr_t)ds->ds_addr);
			len = ds->ds_len;
			if (ds->ds_addr % PAGE_SIZE != 0)
				len += ds->ds_addr % PAGE_SIZE;
			for (k = 0; k < howmany(len, PAGE_SIZE); k++) {
				pbl[i] = htobe64(pmap_kextract(va +
					k * PAGE_SIZE));
				i++;
			}
		}
		MPASS(i == npages);
		break;
	}
	case MEMDESC_PLIST:
	{
		struct bus_dma_segment *ds;
		vm_paddr_t pa;
		vm_size_t len;
		u_int j, k;

		i = 0;
		ds = mem->u.md_list;
		for (j = 0; j < mem->md_nseg; j++, ds++) {
			pa = trunc_page((vm_paddr_t)ds->ds_addr);
			len = ds->ds_len;
			if (ds->ds_addr % PAGE_SIZE != 0)
				len += ds->ds_addr % PAGE_SIZE;
			for (k = 0; k < howmany(len, PAGE_SIZE); k++) {
				pbl[i] = htobe64(pa + k * PAGE_SIZE);
				i++;
			}
		}
		MPASS(i == npages);
		break;
	}
	default:
		__assert_unreachable();
	}

	error = che_write_adapter_mem(qp, addr >> 5, len, pbl);
	if (error != 0) {
		t4_pblpool_free(sc, addr, len);
		free(pbl, M_NVMF_CHE);
		return (false);
	}

	cb->pbl = pbl;
	cb->pbl_addr = addr;
	cb->pbl_len = len;

	return (true);
}

static void
che_free_pbl(struct nvmf_che_command_buffer *cb)
{
	free(cb->pbl, M_NVMF_CHE);
	t4_pblpool_free(cb->qp->nca->sc, cb->pbl_addr, cb->pbl_len);
	cb->pbl = NULL;
	cb->pbl_addr = 0;
	cb->pbl_len = 0;
}

static bool
che_write_tpt_entry(struct nvmf_che_qpair *qp,
    struct nvmf_che_command_buffer *cb, uint16_t stag)
{
	uint32_t tpt_addr;
	int error;

	cb->tpte.valid_to_pdid = htobe32(F_FW_RI_TPTE_VALID |
	    V_FW_RI_TPTE_STAGKEY(CHE_STAG_COLOR(stag)) |
	    F_FW_RI_TPTE_STAGSTATE |
	    V_FW_RI_TPTE_STAGTYPE(FW_RI_STAG_NSMR) |
	    V_FW_RI_TPTE_PDID(0));
	cb->tpte.locread_to_qpid = htobe32(
	    V_FW_RI_TPTE_PERM(FW_RI_MEM_ACCESS_REM_WRITE) |
	    V_FW_RI_TPTE_ADDRTYPE(FW_RI_ZERO_BASED_TO) |
	    V_FW_RI_TPTE_PS(PAGE_SIZE) |
	    V_FW_RI_TPTE_QPID(qp->toep->tid));
#define PBL_OFF(qp, a)	((a) - (qp)->nca->sc->vres.pbl.start)
	cb->tpte.nosnoop_pbladdr =
	    htobe32(V_FW_RI_TPTE_PBLADDR(PBL_OFF(qp, cb->pbl_addr) >> 3));
	cb->tpte.len_lo = htobe32(cb->data_len);
	cb->tpte.va_hi = 0;
	cb->tpte.va_lo_fbo = htobe32(che_fbo(cb));
	cb->tpte.dca_mwbcnt_pstag = 0;
	cb->tpte.len_hi = htobe32(cb->data_offset);

	tpt_addr = qp->tpt_offset + CHE_STAG_IDX(stag) +
	    (qp->nca->sc->vres.stag.start >> 5);

	error = che_write_adapter_mem(qp, tpt_addr, sizeof(cb->tpte),
	    &cb->tpte);
	return (error == 0);
}

static void
che_clear_tpt_entry(struct nvmf_che_qpair *qp, uint16_t stag)
{
	uint32_t tpt_addr;

	tpt_addr = qp->tpt_offset + CHE_STAG_IDX(stag) +
	    (qp->nca->sc->vres.stag.start >> 5);

	(void)che_write_adapter_mem(qp, tpt_addr, sizeof(struct fw_ri_tpte),
	    NULL);
}

static uint16_t
che_alloc_ddp_stag(struct nvmf_che_qpair *qp,
    struct nvmf_che_command_buffer *cb)
{
	uint16_t stag_idx;

	mtx_assert(&qp->rx_buffers.lock, MA_OWNED);
	MPASS(cb->ddp_ok);

	if (qp->active_ddp_tags == qp->num_ddp_tags)
		return (CHE_DDP_NO_TAG);

	MPASS(qp->num_ddp_tags != 0);

	stag_idx = qp->next_ddp_tag;
	for (;;) {
		if (qp->open_ddp_tags[stag_idx] == NULL)
			break;
		if (stag_idx == qp->num_ddp_tags - 1) {
			stag_idx = 0;
			if (qp->ddp_color == CHE_DDP_MAX_COLOR)
				qp->ddp_color = 0;
			else
				qp->ddp_color++;
		} else
			stag_idx++;
		MPASS(stag_idx != qp->next_ddp_tag);
	}
	if (stag_idx == qp->num_ddp_tags - 1)
		qp->next_ddp_tag = 0;
	else
		qp->next_ddp_tag = stag_idx + 1;

	qp->active_ddp_tags++;
	qp->open_ddp_tags[stag_idx] = cb;

	return (CHE_DDP_TAG(stag_idx, qp->ddp_color));
}

static void
che_free_ddp_stag(struct nvmf_che_qpair *qp, struct nvmf_che_command_buffer *cb,
    uint16_t stag)
{
	MPASS(!CHE_TAG_IS_FL(stag));

	mtx_assert(&qp->rx_buffers.lock, MA_OWNED);

	MPASS(qp->open_ddp_tags[CHE_STAG_IDX(stag)] == cb);

	qp->open_ddp_tags[CHE_STAG_IDX(stag)] = NULL;
	qp->active_ddp_tags--;
}

static uint16_t
che_alloc_ddp_tag(struct nvmf_che_qpair *qp,
    struct nvmf_che_command_buffer *cb)
{
	uint16_t stag;

	mtx_assert(&qp->rx_buffers.lock, MA_OWNED);

	if (!cb->ddp_ok)
		return (CHE_DDP_NO_TAG);

	stag = che_alloc_ddp_stag(qp, cb);
	if (stag == CHE_DDP_NO_TAG) {
		counter_u64_add(qp->toep->ofld_rxq->rx_nvme_ddp_setup_no_stag,
		    1);
		return (CHE_DDP_NO_TAG);
	}

	if (!che_alloc_pbl(qp, cb)) {
		che_free_ddp_stag(qp, cb, stag);
		counter_u64_add(qp->toep->ofld_rxq->rx_nvme_ddp_setup_error, 1);
		return (CHE_DDP_NO_TAG);
	}

	if (!che_write_tpt_entry(qp, cb, stag)) {
		che_free_pbl(cb);
		che_free_ddp_stag(qp, cb, stag);
		counter_u64_add(qp->toep->ofld_rxq->rx_nvme_ddp_setup_error, 1);
		return (CHE_DDP_NO_TAG);
	}

	counter_u64_add(qp->toep->ofld_rxq->rx_nvme_ddp_setup_ok, 1);
	return (stag);
}

static void
che_free_ddp_tag(struct nvmf_che_qpair *qp, struct nvmf_che_command_buffer *cb,
    uint16_t stag)
{
	MPASS(!CHE_TAG_IS_FL(stag));

	mtx_assert(&qp->rx_buffers.lock, MA_OWNED);

	MPASS(qp->open_ddp_tags[CHE_STAG_IDX(stag)] == cb);

	che_clear_tpt_entry(qp, stag);
	che_free_pbl(cb);
	che_free_ddp_stag(qp, cb, stag);
}

static void
nvmf_che_write_pdu(struct nvmf_che_qpair *qp, struct mbuf *m)
{
	struct epoch_tracker et;
	struct socket *so = qp->so;
	struct inpcb *inp = sotoinpcb(so);
	struct toepcb *toep = qp->toep;

	CURVNET_SET(so->so_vnet);
	NET_EPOCH_ENTER(et);
	INP_WLOCK(inp);
	if (__predict_false(inp->inp_flags & INP_DROPPED) ||
	    __predict_false((toep->flags & TPF_ATTACHED) == 0)) {
		m_freem(m);
	} else {
		mbufq_enqueue(&toep->ulp_pduq, m);
		t4_push_pdus(toep->vi->adapter, toep, 0);
	}
	INP_WUNLOCK(inp);
	NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();
}

static void
nvmf_che_report_error(struct nvmf_che_qpair *qp, uint16_t fes, uint32_t fei,
    struct mbuf *rx_pdu, u_int hlen)
{
	struct nvme_tcp_term_req_hdr *hdr;
	struct mbuf *m;

	if (hlen != 0) {
		hlen = min(hlen, NVME_TCP_TERM_REQ_ERROR_DATA_MAX_SIZE);
		hlen = min(hlen, m_length(rx_pdu, NULL));
	}

	m = m_get2(sizeof(*hdr) + hlen, M_WAITOK, MT_DATA, M_PKTHDR);
	m->m_len = sizeof(*hdr) + hlen;
	m->m_pkthdr.len = m->m_len;
	hdr = mtod(m, void *);
	memset(hdr, 0, sizeof(*hdr));
	hdr->common.pdu_type = qp->qp.nq_controller ?
	    NVME_TCP_PDU_TYPE_C2H_TERM_REQ : NVME_TCP_PDU_TYPE_H2C_TERM_REQ;
	hdr->common.hlen = sizeof(*hdr);
	hdr->common.plen = sizeof(*hdr) + hlen;
	hdr->fes = htole16(fes);
	le32enc(hdr->fei, fei);
	if (hlen != 0)
		m_copydata(rx_pdu, 0, hlen, (caddr_t)(hdr + 1));

	nvmf_che_write_pdu(qp, m);
}

static int
nvmf_che_validate_pdu(struct nvmf_che_qpair *qp, struct nvmf_che_rxpdu *pdu)
{
	const struct nvme_tcp_common_pdu_hdr *ch;
	struct mbuf *m = pdu->m;
	uint32_t data_len, fei, plen, rx_digest;
	u_int hlen, cpl_error;
	int error;
	uint16_t fes;

	/* Determine how large of a PDU header to return for errors. */
	ch = pdu->hdr;
	hlen = ch->hlen;
	plen = le32toh(ch->plen);
	if (hlen < sizeof(*ch) || hlen > plen)
		hlen = sizeof(*ch);

	cpl_error = m->m_pkthdr.nvmf_cpl_status & CMP_STATUS_ERROR_MASK;
	switch (cpl_error) {
	case CMP_STATUS_NO_ERROR:
		break;
	case CMP_STATUS_HEADER_DIGEST:
		counter_u64_add(
		    qp->toep->ofld_rxq->rx_nvme_header_digest_errors, 1);
		printf("NVMe/TCP: Header digest mismatch\n");
		rx_digest = le32dec(mtodo(m, ch->hlen));
		nvmf_che_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_HDGST_ERROR, rx_digest, m,
		    hlen);
		return (EBADMSG);
	case CMP_STATUS_DIRECTION_MISMATCH:
		counter_u64_add(qp->toep->ofld_rxq->rx_nvme_invalid_headers, 1);
		printf("NVMe/TCP: Invalid PDU type %u\n", ch->pdu_type);
		nvmf_che_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD,
		    offsetof(struct nvme_tcp_common_pdu_hdr, pdu_type), m,
		    hlen);
		return (EBADMSG);
	case CMP_STATUS_SUCCESS_NOT_LAST:
	case CMP_STATUS_DIGEST_FLAG_MISMATCH:
		counter_u64_add(qp->toep->ofld_rxq->rx_nvme_invalid_headers, 1);
		printf("NVMe/TCP: Invalid PDU header flags %#x\n", ch->flags);
		nvmf_che_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD,
		    offsetof(struct nvme_tcp_common_pdu_hdr, flags), m, hlen);
		return (EBADMSG);
	case CMP_STATUS_BAD_DATA_LENGTH:
		counter_u64_add(qp->toep->ofld_rxq->rx_nvme_invalid_headers, 1);
		printf("NVMe/TCP: Invalid PDU length %u\n", plen);
		nvmf_che_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD,
		    offsetof(struct nvme_tcp_common_pdu_hdr, plen), m, hlen);
		return (EBADMSG);
	case CMP_STATUS_USER_MODE_UNALLOCATED:
	case CMP_STATUS_RQT_LIMIT:
	case CMP_STATUS_RQT_WRAP:
	case CMP_STATUS_RQT_BOUND:
		device_printf(qp->nca->sc->dev,
		    "received invalid NVMET error %u\n",
		    cpl_error);
		return (ECONNRESET);
	case CMP_STATUS_TPT_LIMIT:
	case CMP_STATUS_TPT_INVALID:
	case CMP_STATUS_TPT_COLOUR_MISMATCH:
	case CMP_STATUS_TPT_MISC:
	case CMP_STATUS_TPT_WRAP:
	case CMP_STATUS_TPT_BOUND:
		counter_u64_add(qp->toep->ofld_rxq->rx_nvme_invalid_headers, 1);
		switch (ch->pdu_type) {
		case NVME_TCP_PDU_TYPE_H2C_DATA:
			nvmf_che_report_error(qp,
			    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD,
			    offsetof(struct nvme_tcp_h2c_data_hdr, ttag),
			    pdu->m, pdu->hdr->hlen);
			return (EBADMSG);
		case NVME_TCP_PDU_TYPE_C2H_DATA:
			nvmf_che_report_error(qp,
			    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD,
			    offsetof(struct nvme_tcp_c2h_data_hdr, cccid), m,
			    hlen);
			return (EBADMSG);
		default:
			device_printf(qp->nca->sc->dev,
			    "received DDP NVMET error %u for PDU %u\n",
			    cpl_error, ch->pdu_type);
			return (ECONNRESET);
		}
	case CMP_STATUS_TPT_LAST_PDU_UNALIGNED:
		counter_u64_add(qp->toep->ofld_rxq->rx_nvme_invalid_headers, 1);
		nvmf_che_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_PDU_SEQUENCE_ERROR, 0, m, hlen);
		return (EBADMSG);
	case CMP_STATUS_PBL_LIMIT:
		counter_u64_add(qp->toep->ofld_rxq->rx_nvme_invalid_headers, 1);
		nvmf_che_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_DATA_TRANSFER_OUT_OF_RANGE, 0, m,
		    hlen);
		return (EBADMSG);
	case CMP_STATUS_DATA_DIGEST:
		/* Handled below. */
		break;
	default:
		device_printf(qp->nca->sc->dev,
		    "received unknown NVMET error %u\n",
		    cpl_error);
		return (ECONNRESET);
	}

	error = nvmf_tcp_validate_pdu_header(ch, qp->qp.nq_controller,
	    qp->header_digests, qp->data_digests, qp->rxpda, &data_len, &fes,
	    &fei);
	if (error != 0) {
		if (error != ECONNRESET)
			nvmf_che_report_error(qp, fes, fei, m, hlen);
		return (error);
	}

	/* Check data digest if present. */
	pdu->data_digest_mismatch = false;
	if ((ch->flags & NVME_TCP_CH_FLAGS_DDGSTF) != 0) {
		if (cpl_error == CMP_STATUS_DATA_DIGEST) {
			printf("NVMe/TCP: Data digest mismatch\n");
			pdu->data_digest_mismatch = true;
			counter_u64_add(
			    qp->toep->ofld_rxq->rx_nvme_data_digest_errors, 1);
		}
	}

	pdu->data_len = data_len;

	return (0);
}

static void
nvmf_che_free_pdu(struct nvmf_che_rxpdu *pdu)
{
	m_freem(pdu->m);
	pdu->m = NULL;
	pdu->hdr = NULL;
}

static int
nvmf_che_handle_term_req(struct nvmf_che_rxpdu *pdu)
{
	const struct nvme_tcp_term_req_hdr *hdr;

	hdr = (const void *)pdu->hdr;

	printf("NVMe/TCP: Received termination request: fes %#x fei %#x\n",
	    le16toh(hdr->fes), le32dec(hdr->fei));
	nvmf_che_free_pdu(pdu);
	return (ECONNRESET);
}

static int
nvmf_che_save_command_capsule(struct nvmf_che_qpair *qp,
    struct nvmf_che_rxpdu *pdu)
{
	const struct nvme_tcp_cmd *cmd;
	struct nvmf_capsule *nc;
	struct nvmf_che_capsule *cc;

	cmd = (const void *)pdu->hdr;

	nc = nvmf_allocate_command(&qp->qp, &cmd->ccsqe, M_WAITOK);

	cc = CCAP(nc);
	cc->rx_pdu = *pdu;

	nvmf_capsule_received(&qp->qp, nc);
	return (0);
}

static int
nvmf_che_save_response_capsule(struct nvmf_che_qpair *qp,
    struct nvmf_che_rxpdu *pdu)
{
	const struct nvme_tcp_rsp *rsp;
	struct nvme_completion cpl;
	struct nvmf_capsule *nc;
	struct nvmf_che_capsule *cc;
	uint16_t cid;

	rsp = (const void *)pdu->hdr;

	/*
	 * Restore the original CID and ensure any command buffers
	 * associated with this CID have been released.  Once the CQE
	 * has been received, no further transfers to the command
	 * buffer for the associated CID can occur.
	 */
	cpl = rsp->rccqe;
	cid = le16toh(cpl.cid);
	if (CHE_TAG_IS_FL(cid)) {
		cid = CHE_RAW_FL_TAG(cid);
		mtx_lock(&qp->fl_cid_lock);
		MPASS(FL_CID_ISACTIVE(cid, qp->fl_cid_set));
		cpl.cid = qp->fl_cids[cid];
		FL_CID_FREE(cid, qp->fl_cid_set);
		mtx_unlock(&qp->fl_cid_lock);

		che_purge_command_buffer(&qp->rx_buffers, rsp->rccqe.cid);
		che_purge_command_buffer(&qp->tx_buffers, rsp->rccqe.cid);
	} else {
		struct nvmf_che_command_buffer *cb;

		mtx_lock(&qp->rx_buffers.lock);
		cb = qp->open_ddp_tags[CHE_STAG_IDX(cid)];
		MPASS(cb != NULL);
		MPASS(cb->cid == rsp->rccqe.cid);
		cpl.cid = cb->original_cid;
		che_free_ddp_tag(qp, cb, cid);
		mtx_unlock(&qp->rx_buffers.lock);
		che_release_command_buffer(cb);
	}
#ifdef VERBOSE_TRACES
	CTR(KTR_CXGBE, "%s: tid %u freed cid 0x%04x for 0x%04x", __func__,
	    qp->toep->tid, le16toh(rsp->rccqe.cid), cpl.cid);
#endif

	nc = nvmf_allocate_response(&qp->qp, &cpl, M_WAITOK);

	nc->nc_sqhd_valid = true;
	cc = CCAP(nc);
	cc->rx_pdu = *pdu;

	nvmf_capsule_received(&qp->qp, nc);
	return (0);
}

/*
 * Construct a PDU that contains an optional data payload.  This
 * includes dealing with the length fields in the common header.  The
 * adapter inserts digests and padding when the PDU is transmitted.
 */
static struct mbuf *
nvmf_che_construct_pdu(struct nvmf_che_qpair *qp, void *hdr, size_t hlen,
    struct mbuf *data, uint32_t data_len)
{
	struct nvme_tcp_common_pdu_hdr *ch;
	struct mbuf *top;
	uint32_t pdo, plen;
	uint8_t ulp_submode;

	plen = hlen;
	if (qp->header_digests)
		plen += sizeof(uint32_t);
	if (data_len != 0) {
		KASSERT(m_length(data, NULL) == data_len, ("length mismatch"));
		pdo = roundup(plen, qp->txpda);
		plen = pdo + data_len;
		if (qp->data_digests)
			plen += sizeof(uint32_t);
	} else {
		KASSERT(data == NULL, ("payload mbuf with zero length"));
		pdo = 0;
	}

	top = m_get2(hlen, M_WAITOK, MT_DATA, M_PKTHDR);
	top->m_len = hlen;
	top->m_pkthdr.len = hlen;
	ch = mtod(top, void *);
	memcpy(ch, hdr, hlen);
	ch->hlen = hlen;
	ulp_submode = 0;
	if (qp->header_digests) {
		ch->flags |= NVME_TCP_CH_FLAGS_HDGSTF;
		ulp_submode |= ULP_CRC_HEADER;
	}
	if (qp->data_digests && data_len != 0) {
		ch->flags |= NVME_TCP_CH_FLAGS_DDGSTF;
		ulp_submode |= ULP_CRC_DATA;
	}
	ch->pdo = pdo;
	ch->plen = htole32(plen);
	set_mbuf_ulp_submode(top, ulp_submode);

	if (data_len != 0) {
		top->m_pkthdr.len += data_len;
		top->m_next = data;
	}

	return (top);
}

/* Allocate the next free freelist transfer tag. */
static bool
nvmf_che_allocate_fl_ttag(struct nvmf_che_qpair *qp,
    struct nvmf_che_command_buffer *cb)
{
	uint16_t ttag;

	mtx_assert(&qp->rx_buffers.lock, MA_OWNED);

	if (qp->active_fl_ttags == qp->num_fl_ttags)
		return (false);

	ttag = qp->next_fl_ttag;
	for (;;) {
		if (qp->open_fl_ttags[ttag] == NULL)
			break;
		if (ttag == qp->num_fl_ttags - 1)
			ttag = 0;
		else
			ttag++;
		MPASS(ttag != qp->next_fl_ttag);
	}
	if (ttag == qp->num_fl_ttags - 1)
		qp->next_fl_ttag = 0;
	else
		qp->next_fl_ttag = ttag + 1;

	qp->active_fl_ttags++;
	qp->open_fl_ttags[ttag] = cb;

	cb->ttag = ttag | CHE_FL_TAG_MASK;
	return (true);
}

/* Attempt to allocate a free transfer tag and assign it to cb. */
static bool
nvmf_che_allocate_ttag(struct nvmf_che_qpair *qp,
    struct nvmf_che_command_buffer *cb)
{
	uint16_t stag;

	mtx_assert(&qp->rx_buffers.lock, MA_OWNED);

	stag = che_alloc_ddp_tag(qp, cb);
	if (stag == CHE_DDP_NO_TAG) {
		if (!nvmf_che_allocate_fl_ttag(qp, cb))
			return (false);
	} else {
		cb->ttag = stag;
	}
#ifdef VERBOSE_TRACES
	CTR(KTR_CXGBE, "%s: tid %u allocated ttag 0x%04x", __func__,
	    qp->toep->tid, cb->ttag);
#endif
	cb->cc->active_r2ts++;
	return (true);
}

/* Find the next command buffer eligible to schedule for R2T. */
static struct nvmf_che_command_buffer *
nvmf_che_next_r2t(struct nvmf_che_qpair *qp)
{
	struct nvmf_che_command_buffer *cb;

	mtx_assert(&qp->rx_buffers.lock, MA_OWNED);

	TAILQ_FOREACH(cb, &qp->rx_buffers.head, link) {
		/* NB: maxr2t is 0's based. */
		if (cb->cc->active_r2ts > qp->maxr2t)
			continue;

		if (!nvmf_che_allocate_ttag(qp, cb))
			return (NULL);
#ifdef INVARIANTS
		cb->cc->pending_r2ts--;
#endif
		TAILQ_REMOVE(&qp->rx_buffers.head, cb, link);
		return (cb);
	}
	return (NULL);
}

/* NB: cid and is little-endian already. */
static void
che_send_r2t(struct nvmf_che_qpair *qp, uint16_t cid, uint16_t ttag,
    uint32_t data_offset, uint32_t data_len)
{
	struct nvme_tcp_r2t_hdr r2t;
	struct mbuf *m;

	memset(&r2t, 0, sizeof(r2t));
	r2t.common.pdu_type = NVME_TCP_PDU_TYPE_R2T;
	r2t.cccid = cid;
	r2t.ttag = htole16(ttag);
	r2t.r2to = htole32(data_offset);
	r2t.r2tl = htole32(data_len);

	m = nvmf_che_construct_pdu(qp, &r2t, sizeof(r2t), NULL, 0);
	nvmf_che_write_pdu(qp, m);
}

/*
 * Release a transfer tag and schedule another R2T.
 *
 * NB: This drops the rx_buffers.lock mutex.
 */
static void
nvmf_che_send_next_r2t(struct nvmf_che_qpair *qp,
    struct nvmf_che_command_buffer *cb)
{
	struct nvmf_che_command_buffer *ncb;

	mtx_assert(&qp->rx_buffers.lock, MA_OWNED);

#ifdef VERBOSE_TRACES
	CTR(KTR_CXGBE, "%s: tid %u freed ttag 0x%04x", __func__, qp->toep->tid,
	    cb->ttag);
#endif
	if (CHE_TAG_IS_FL(cb->ttag)) {
		uint16_t ttag;

		ttag = CHE_RAW_FL_TAG(cb->ttag);
		MPASS(qp->open_fl_ttags[ttag] == cb);

		/* Release this transfer tag. */
		qp->open_fl_ttags[ttag] = NULL;
		qp->active_fl_ttags--;
	} else
		che_free_ddp_tag(qp, cb, cb->ttag);

	cb->cc->active_r2ts--;

	/* Schedule another R2T. */
	ncb = nvmf_che_next_r2t(qp);
	mtx_unlock(&qp->rx_buffers.lock);
	if (ncb != NULL)
		che_send_r2t(qp, ncb->cid, ncb->ttag, ncb->data_offset,
		    ncb->data_len);
}

/*
 * Copy len bytes starting at offset skip from an mbuf chain into an
 * I/O buffer at destination offset io_offset.
 */
static void
mbuf_copyto_io(struct mbuf *m, u_int skip, u_int len,
    struct nvmf_io_request *io, u_int io_offset)
{
	u_int todo;

	while (m->m_len <= skip) {
		skip -= m->m_len;
		m = m->m_next;
	}
	while (len != 0) {
		MPASS((m->m_flags & M_EXTPG) == 0);

		todo = min(m->m_len - skip, len);
		memdesc_copyback(&io->io_mem, io_offset, todo, mtodo(m, skip));
		skip = 0;
		io_offset += todo;
		len -= todo;
		m = m->m_next;
	}
}

static int
nvmf_che_handle_h2c_data(struct nvmf_che_qpair *qp, struct nvmf_che_rxpdu *pdu)
{
	const struct nvme_tcp_h2c_data_hdr *h2c;
	struct nvmf_che_command_buffer *cb;
	uint32_t data_len, data_offset;
	uint16_t ttag, fl_ttag;

	h2c = (const void *)pdu->hdr;
	if (le32toh(h2c->datal) > qp->maxh2cdata) {
		nvmf_che_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_DATA_TRANSFER_LIMIT_EXCEEDED, 0,
		    pdu->m, pdu->hdr->hlen);
		nvmf_che_free_pdu(pdu);
		return (EBADMSG);
	}

	ttag = le16toh(h2c->ttag);
	if (CHE_TAG_IS_FL(ttag)) {
		fl_ttag = CHE_RAW_FL_TAG(ttag);
		if (fl_ttag >= qp->num_fl_ttags) {
			nvmf_che_report_error(qp,
			    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD,
			    offsetof(struct nvme_tcp_h2c_data_hdr, ttag),
			    pdu->m, pdu->hdr->hlen);
			nvmf_che_free_pdu(pdu);
			return (EBADMSG);
		}

		mtx_lock(&qp->rx_buffers.lock);
		cb = qp->open_fl_ttags[fl_ttag];
	} else {
		if (CHE_STAG_IDX(ttag) >= qp->num_ddp_tags) {
			nvmf_che_report_error(qp,
			    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD,
			    offsetof(struct nvme_tcp_h2c_data_hdr, ttag),
			    pdu->m, pdu->hdr->hlen);
			nvmf_che_free_pdu(pdu);
			return (EBADMSG);
		}

		mtx_lock(&qp->rx_buffers.lock);
		cb = qp->open_ddp_tags[CHE_STAG_IDX(ttag)];
	}

	if (cb == NULL) {
		mtx_unlock(&qp->rx_buffers.lock);
		nvmf_che_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD,
		    offsetof(struct nvme_tcp_h2c_data_hdr, ttag), pdu->m,
		    pdu->hdr->hlen);
		nvmf_che_free_pdu(pdu);
		return (EBADMSG);
	}
	MPASS(cb->ttag == ttag);

	/* For a data digest mismatch, fail the I/O request. */
	if (pdu->data_digest_mismatch) {
		nvmf_che_send_next_r2t(qp, cb);
		cb->error = EINTEGRITY;
		che_release_command_buffer(cb);
		nvmf_che_free_pdu(pdu);
		return (0);
	}

	data_len = le32toh(h2c->datal);
	if (data_len != pdu->data_len) {
		mtx_unlock(&qp->rx_buffers.lock);
		nvmf_che_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD,
		    offsetof(struct nvme_tcp_h2c_data_hdr, datal), pdu->m,
		    pdu->hdr->hlen);
		nvmf_che_free_pdu(pdu);
		return (EBADMSG);
	}

	data_offset = le32toh(h2c->datao);
	if (data_offset < cb->data_offset ||
	    data_offset + data_len > cb->data_offset + cb->data_len) {
		mtx_unlock(&qp->rx_buffers.lock);
		nvmf_che_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_DATA_TRANSFER_OUT_OF_RANGE, 0, pdu->m,
		    pdu->hdr->hlen);
		nvmf_che_free_pdu(pdu);
		return (EBADMSG);
	}

	if (data_offset != cb->data_offset + cb->data_xfered) {
		if (CHE_TAG_IS_FL(ttag)) {
			mtx_unlock(&qp->rx_buffers.lock);
			nvmf_che_report_error(qp,
			    NVME_TCP_TERM_REQ_FES_PDU_SEQUENCE_ERROR, 0, pdu->m,
			    pdu->hdr->hlen);
			nvmf_che_free_pdu(pdu);
			return (EBADMSG);
		} else {
			uint32_t ddp_bytes;

			/* Account for PDUs silently received via DDP. */
			ddp_bytes = data_offset -
			    (cb->data_offset + cb->data_xfered);
			cb->data_xfered += ddp_bytes;
#ifdef VERBOSE_TRACES
			CTR(KTR_CXGBE, "%s: tid %u previous ddp_bytes %u",
			    __func__, qp->toep->tid, ddp_bytes);
#endif
			counter_u64_add(qp->toep->ofld_rxq->rx_nvme_ddp_octets,
			    ddp_bytes);
		}
	}

	if ((cb->data_xfered + data_len == cb->data_len) !=
	    ((pdu->hdr->flags & NVME_TCP_H2C_DATA_FLAGS_LAST_PDU) != 0)) {
		mtx_unlock(&qp->rx_buffers.lock);
		nvmf_che_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_PDU_SEQUENCE_ERROR, 0, pdu->m,
		    pdu->hdr->hlen);
		nvmf_che_free_pdu(pdu);
		return (EBADMSG);
	}

	cb->data_xfered += data_len;
	data_offset -= cb->data_offset;
	if (cb->data_xfered == cb->data_len) {
		nvmf_che_send_next_r2t(qp, cb);
	} else {
		che_hold_command_buffer(cb);
		mtx_unlock(&qp->rx_buffers.lock);
	}

	if (CHE_TAG_IS_FL(ttag))
		mbuf_copyto_io(pdu->m->m_next, 0, data_len, &cb->io,
		    data_offset);

	che_release_command_buffer(cb);
	nvmf_che_free_pdu(pdu);
	return (0);
}

static int
nvmf_che_handle_c2h_data(struct nvmf_che_qpair *qp, struct nvmf_che_rxpdu *pdu)
{
	const struct nvme_tcp_c2h_data_hdr *c2h;
	struct nvmf_che_command_buffer *cb;
	uint32_t data_len, data_offset;
	uint16_t cid, original_cid;

	/*
	 * Unlike freelist command buffers, DDP command buffers are
	 * not released until the response capsule is received to keep
	 * the STAG allocated until the command has completed.
	 */
	c2h = (const void *)pdu->hdr;

	cid = le16toh(c2h->cccid);
	if (CHE_TAG_IS_FL(cid)) {
		mtx_lock(&qp->rx_buffers.lock);
		cb = che_find_command_buffer(&qp->rx_buffers, c2h->cccid);
	} else {
		if (CHE_STAG_IDX(cid) >= qp->num_ddp_tags) {
			nvmf_che_report_error(qp,
			    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD,
			    offsetof(struct nvme_tcp_c2h_data_hdr, cccid),
			    pdu->m, pdu->hdr->hlen);
			nvmf_che_free_pdu(pdu);
			return (EBADMSG);
		}

		mtx_lock(&qp->rx_buffers.lock);
		cb = qp->open_ddp_tags[CHE_STAG_IDX(cid)];
	}

	if (cb == NULL) {
		mtx_unlock(&qp->rx_buffers.lock);
		/*
		 * XXX: Could be PDU sequence error if cccid is for a
		 * command that doesn't use a command buffer.
		 */
		nvmf_che_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD,
		    offsetof(struct nvme_tcp_c2h_data_hdr, cccid), pdu->m,
		    pdu->hdr->hlen);
		nvmf_che_free_pdu(pdu);
		return (EBADMSG);
	}

	/* For a data digest mismatch, fail the I/O request. */
	if (pdu->data_digest_mismatch) {
		cb->error = EINTEGRITY;
		if (CHE_TAG_IS_FL(cid)) {
			che_remove_command_buffer(&qp->rx_buffers, cb);
			mtx_unlock(&qp->rx_buffers.lock);
			che_release_command_buffer(cb);
		} else
			mtx_unlock(&qp->rx_buffers.lock);
		nvmf_che_free_pdu(pdu);
		return (0);
	}

	data_len = le32toh(c2h->datal);
	if (data_len != pdu->data_len) {
		mtx_unlock(&qp->rx_buffers.lock);
		nvmf_che_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD,
		    offsetof(struct nvme_tcp_c2h_data_hdr, datal), pdu->m,
		    pdu->hdr->hlen);
		nvmf_che_free_pdu(pdu);
		return (EBADMSG);
	}

	data_offset = le32toh(c2h->datao);
	if (data_offset < cb->data_offset ||
	    data_offset + data_len > cb->data_offset + cb->data_len) {
		mtx_unlock(&qp->rx_buffers.lock);
		nvmf_che_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_DATA_TRANSFER_OUT_OF_RANGE, 0,
		    pdu->m, pdu->hdr->hlen);
		nvmf_che_free_pdu(pdu);
		return (EBADMSG);
	}

	if (data_offset != cb->data_offset + cb->data_xfered) {
		if (CHE_TAG_IS_FL(cid)) {
			mtx_unlock(&qp->rx_buffers.lock);
			nvmf_che_report_error(qp,
			    NVME_TCP_TERM_REQ_FES_PDU_SEQUENCE_ERROR, 0, pdu->m,
			    pdu->hdr->hlen);
			nvmf_che_free_pdu(pdu);
			return (EBADMSG);
		} else {
			uint32_t ddp_bytes;

			/* Account for PDUs silently received via DDP. */
			ddp_bytes = data_offset -
			    (cb->data_offset + cb->data_xfered);
			cb->data_xfered += ddp_bytes;
#ifdef VERBOSE_TRACES
			CTR(KTR_CXGBE, "%s: tid %u previous ddp_bytes %u",
			    __func__, qp->toep->tid, ddp_bytes);
#endif
			counter_u64_add(qp->toep->ofld_rxq->rx_nvme_ddp_octets,
			    ddp_bytes);
		}
	}

	if ((cb->data_xfered + data_len == cb->data_len) !=
	    ((pdu->hdr->flags & NVME_TCP_C2H_DATA_FLAGS_LAST_PDU) != 0)) {
		mtx_unlock(&qp->rx_buffers.lock);
		nvmf_che_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_PDU_SEQUENCE_ERROR, 0, pdu->m,
		    pdu->hdr->hlen);
		nvmf_che_free_pdu(pdu);
		return (EBADMSG);
	}

	cb->data_xfered += data_len;
	original_cid = cb->original_cid;

	if (CHE_TAG_IS_FL(cid)) {
		data_offset -= cb->data_offset;
		if (cb->data_xfered == cb->data_len)
			che_remove_command_buffer(&qp->rx_buffers, cb);
		else
			che_hold_command_buffer(cb);
		mtx_unlock(&qp->rx_buffers.lock);

		if ((pdu->hdr->flags & NVME_TCP_C2H_DATA_FLAGS_SUCCESS) != 0) {
			/*
			 * Free the CID as the command has now been
			 * completed.
			 */
			cid = CHE_RAW_FL_TAG(cid);
			mtx_lock(&qp->fl_cid_lock);
			MPASS(FL_CID_ISACTIVE(cid, qp->fl_cid_set));
			MPASS(original_cid == qp->fl_cids[cid]);
			FL_CID_FREE(cid, qp->fl_cid_set);
			mtx_unlock(&qp->fl_cid_lock);
		}

		mbuf_copyto_io(pdu->m->m_next, 0, data_len, &cb->io,
		    data_offset);

		che_release_command_buffer(cb);
	} else {
		if ((pdu->hdr->flags & NVME_TCP_C2H_DATA_FLAGS_SUCCESS) != 0) {
			/*
			 * Free the command buffer and STAG as the
			 * command has now been completed.
			 */
			che_free_ddp_tag(qp, cb, cid);
			mtx_unlock(&qp->rx_buffers.lock);
			che_release_command_buffer(cb);
		} else
			mtx_unlock(&qp->rx_buffers.lock);
	}

	if ((pdu->hdr->flags & NVME_TCP_C2H_DATA_FLAGS_SUCCESS) != 0) {
		struct nvme_completion cqe;
		struct nvmf_capsule *nc;

		memset(&cqe, 0, sizeof(cqe));
		cqe.cid = original_cid;

		nc = nvmf_allocate_response(&qp->qp, &cqe, M_WAITOK);
		nc->nc_sqhd_valid = false;

		nvmf_capsule_received(&qp->qp, nc);
	}

	nvmf_che_free_pdu(pdu);
	return (0);
}

/* Called when m_free drops refcount to 0. */
static void
nvmf_che_mbuf_done(struct mbuf *m)
{
	struct nvmf_che_command_buffer *cb = m->m_ext.ext_arg1;

	che_free_command_buffer(cb);
}

static struct mbuf *
nvmf_che_mbuf(void *arg, int how, void *data, size_t len)
{
	struct nvmf_che_command_buffer *cb = arg;
	struct mbuf *m;

	m = m_get(how, MT_DATA);
	m->m_flags |= M_RDONLY;
	m_extaddref(m, data, len, &cb->refs, nvmf_che_mbuf_done, cb, NULL);
	m->m_len = len;
	return (m);
}

static void
nvmf_che_free_mext_pg(struct mbuf *m)
{
	struct nvmf_che_command_buffer *cb = m->m_ext.ext_arg1;

	M_ASSERTEXTPG(m);
	che_release_command_buffer(cb);
}

static struct mbuf *
nvmf_che_mext_pg(void *arg, int how)
{
	struct nvmf_che_command_buffer *cb = arg;
	struct mbuf *m;

	m = mb_alloc_ext_pgs(how, nvmf_che_free_mext_pg, M_RDONLY);
	m->m_ext.ext_arg1 = cb;
	che_hold_command_buffer(cb);
	return (m);
}

/*
 * Return an mbuf chain for a range of data belonging to a command
 * buffer.
 *
 * The mbuf chain uses M_EXT mbufs which hold references on the
 * command buffer so that it remains "alive" until the data has been
 * fully transmitted.  If truncate_ok is true, then the mbuf chain
 * might return a short chain to avoid gratuitously splitting up a
 * page.
 */
static struct mbuf *
nvmf_che_command_buffer_mbuf(struct nvmf_che_command_buffer *cb,
    uint32_t data_offset, uint32_t data_len, uint32_t *actual_len,
    bool can_truncate)
{
	struct mbuf *m;
	size_t len;

	m = memdesc_alloc_ext_mbufs(&cb->io.io_mem, nvmf_che_mbuf,
	    nvmf_che_mext_pg, cb, M_WAITOK, data_offset, data_len, &len,
	    can_truncate);
	if (actual_len != NULL)
		*actual_len = len;
	return (m);
}

/* NB: cid and ttag and little-endian already. */
static void
che_send_h2c_pdu(struct nvmf_che_qpair *qp, uint16_t cid, uint16_t ttag,
    uint32_t data_offset, struct mbuf *m, size_t len, bool last_pdu)
{
	struct nvme_tcp_h2c_data_hdr h2c;
	struct mbuf *top;

	memset(&h2c, 0, sizeof(h2c));
	h2c.common.pdu_type = NVME_TCP_PDU_TYPE_H2C_DATA;
	if (last_pdu)
		h2c.common.flags |= NVME_TCP_H2C_DATA_FLAGS_LAST_PDU;
	h2c.cccid = cid;
	h2c.ttag = ttag;
	h2c.datao = htole32(data_offset);
	h2c.datal = htole32(len);

	top = nvmf_che_construct_pdu(qp, &h2c, sizeof(h2c), m, len);
	nvmf_che_write_pdu(qp, top);
}

static int
nvmf_che_handle_r2t(struct nvmf_che_qpair *qp, struct nvmf_che_rxpdu *pdu)
{
	const struct nvme_tcp_r2t_hdr *r2t;
	struct nvmf_che_command_buffer *cb;
	uint32_t data_len, data_offset;

	r2t = (const void *)pdu->hdr;

	mtx_lock(&qp->tx_buffers.lock);
	cb = che_find_command_buffer(&qp->tx_buffers, r2t->cccid);
	if (cb == NULL) {
		mtx_unlock(&qp->tx_buffers.lock);
		nvmf_che_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_INVALID_HEADER_FIELD,
		    offsetof(struct nvme_tcp_r2t_hdr, cccid), pdu->m,
		    pdu->hdr->hlen);
		nvmf_che_free_pdu(pdu);
		return (EBADMSG);
	}

	data_offset = le32toh(r2t->r2to);
	if (data_offset != cb->data_xfered) {
		mtx_unlock(&qp->tx_buffers.lock);
		nvmf_che_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_PDU_SEQUENCE_ERROR, 0, pdu->m,
		    pdu->hdr->hlen);
		nvmf_che_free_pdu(pdu);
		return (EBADMSG);
	}

	/*
	 * XXX: The spec does not specify how to handle R2T tranfers
	 * out of range of the original command.
	 */
	data_len = le32toh(r2t->r2tl);
	if (data_offset + data_len > cb->data_len) {
		mtx_unlock(&qp->tx_buffers.lock);
		nvmf_che_report_error(qp,
		    NVME_TCP_TERM_REQ_FES_DATA_TRANSFER_OUT_OF_RANGE, 0,
		    pdu->m, pdu->hdr->hlen);
		nvmf_che_free_pdu(pdu);
		return (EBADMSG);
	}

	cb->data_xfered += data_len;
	if (cb->data_xfered == cb->data_len)
		che_remove_command_buffer(&qp->tx_buffers, cb);
	else
		che_hold_command_buffer(cb);
	mtx_unlock(&qp->tx_buffers.lock);

	/*
	 * Queue one or more H2C_DATA PDUs containing the requested
	 * data.
	 */
	while (data_len > 0) {
		struct mbuf *m;
		uint32_t sent, todo;

		todo = min(data_len, qp->max_tx_data);
		m = nvmf_che_command_buffer_mbuf(cb, data_offset, todo, &sent,
		    todo < data_len);
		che_send_h2c_pdu(qp, r2t->cccid, r2t->ttag, data_offset, m,
		    sent, sent == data_len);

		data_offset += sent;
		data_len -= sent;
	}

	che_release_command_buffer(cb);
	nvmf_che_free_pdu(pdu);
	return (0);
}

static int
nvmf_che_dispatch_pdu(struct nvmf_che_qpair *qp, struct nvmf_che_rxpdu *pdu)
{
	/*
	 * The PDU header should always be contiguous in the mbuf from
	 * CPL_NVMT_CMP.
	 */
	pdu->hdr = mtod(pdu->m, void *);
	KASSERT(pdu->m->m_len == pdu->hdr->hlen +
	    ((pdu->hdr->flags & NVME_TCP_CH_FLAGS_HDGSTF) != 0 ?
	    sizeof(uint32_t) : 0),
	    ("%s: mismatched PDU header mbuf length", __func__));

	switch (pdu->hdr->pdu_type) {
	default:
		__assert_unreachable();
		break;
	case NVME_TCP_PDU_TYPE_H2C_TERM_REQ:
	case NVME_TCP_PDU_TYPE_C2H_TERM_REQ:
		return (nvmf_che_handle_term_req(pdu));
	case NVME_TCP_PDU_TYPE_CAPSULE_CMD:
		return (nvmf_che_save_command_capsule(qp, pdu));
	case NVME_TCP_PDU_TYPE_CAPSULE_RESP:
		return (nvmf_che_save_response_capsule(qp, pdu));
	case NVME_TCP_PDU_TYPE_H2C_DATA:
		return (nvmf_che_handle_h2c_data(qp, pdu));
	case NVME_TCP_PDU_TYPE_C2H_DATA:
		return (nvmf_che_handle_c2h_data(qp, pdu));
	case NVME_TCP_PDU_TYPE_R2T:
		return (nvmf_che_handle_r2t(qp, pdu));
	}
}

static int
nvmf_che_attach_pdu_data(struct nvmf_che_qpair *qp, struct nvmf_che_rxpdu *pdu)
{
	struct socket *so = qp->so;
	struct mbuf *m, *n;
	uint32_t tcp_seq;
	size_t len;
	int error;

	/* Check for DDP data. */
	if (pdu->ddp) {
		counter_u64_add(qp->toep->ofld_rxq->rx_nvme_ddp_pdus, 1);
		counter_u64_add(qp->toep->ofld_rxq->rx_nvme_ddp_octets,
		    pdu->data_len);
		return (0);
	}

	error = 0;
	len = pdu->data_len;
	tcp_seq = pdu->m->m_pkthdr.nvmf_tcp_seq;
	m = pdu->m;
	SOCKBUF_LOCK(&so->so_rcv);
	while (len > 0) {
		n = mbufq_dequeue(&qp->rx_data);
		KASSERT(n != NULL, ("%s: missing %zu data", __func__, len));
		if (n == NULL) {
			error = ENOBUFS;
			break;
		}

		KASSERT(n->m_pkthdr.nvmf_tcp_seq == tcp_seq,
		    ("%s: TCP seq mismatch", __func__));
		KASSERT(n->m_pkthdr.len <= len,
		    ("%s: too much data", __func__));
		if (n->m_pkthdr.nvmf_tcp_seq != tcp_seq ||
		    n->m_pkthdr.len > len) {
			m_freem(n);
			error = ENOBUFS;
			break;
		}

#ifdef VERBOSE_TRACES
		CTR(KTR_CXGBE, "%s: tid %u len %d seq %u", __func__,
		    qp->toep->tid, n->m_pkthdr.len, n->m_pkthdr.nvmf_tcp_seq);
#endif
		pdu->m->m_pkthdr.len += n->m_pkthdr.len;
		len -= n->m_pkthdr.len;
		tcp_seq += n->m_pkthdr.len;
		m_demote_pkthdr(n);
		m->m_next = n;
		m = m_last(n);
	}
	SOCKBUF_UNLOCK(&so->so_rcv);

	if (error == 0) {
		counter_u64_add(qp->toep->ofld_rxq->rx_nvme_fl_pdus, 1);
		counter_u64_add(qp->toep->ofld_rxq->rx_nvme_fl_octets,
		    pdu->data_len);
	}
	return (error);
}

static void
nvmf_che_receive(void *arg)
{
	struct nvmf_che_qpair *qp = arg;
	struct socket *so = qp->so;
	struct nvmf_che_rxpdu pdu;
	struct mbuf *m;
	int error, terror;

	SOCKBUF_LOCK(&so->so_rcv);
	while (!qp->rx_shutdown) {
		/* Wait for a PDU. */
		if (so->so_error != 0 || so->so_rerror != 0) {
			if (so->so_error != 0)
				error = so->so_error;
			else
				error = so->so_rerror;
			SOCKBUF_UNLOCK(&so->so_rcv);
		error:
			nvmf_qpair_error(&qp->qp, error);
			SOCKBUF_LOCK(&so->so_rcv);
			while (!qp->rx_shutdown)
				cv_wait(&qp->rx_cv, SOCKBUF_MTX(&so->so_rcv));
			break;
		}

		m = mbufq_dequeue(&qp->rx_pdus);
		if (m == NULL) {
			if ((so->so_rcv.sb_state & SBS_CANTRCVMORE) != 0) {
				error = 0;
				SOCKBUF_UNLOCK(&so->so_rcv);
				goto error;
			}
			cv_wait(&qp->rx_cv, SOCKBUF_MTX(&so->so_rcv));
			continue;
		}
		SOCKBUF_UNLOCK(&so->so_rcv);

		pdu.m = m;
		pdu.hdr = mtod(m, const void *);
		pdu.ddp = (m->m_pkthdr.nvmf_cpl_status & CMP_STATUS_DDP) != 0;

		error = nvmf_che_validate_pdu(qp, &pdu);
		if (error == 0 && pdu.data_len != 0)
			error = nvmf_che_attach_pdu_data(qp, &pdu);
		if (error != 0)
			nvmf_che_free_pdu(&pdu);
		else
			error = nvmf_che_dispatch_pdu(qp, &pdu);
		if (error != 0) {
			/*
			 * If we received a termination request, close
			 * the connection immediately.
			 */
			if (error == ECONNRESET)
				goto error;

			/*
			 * Wait for up to 30 seconds for the socket to
			 * be closed by the other end.
			 */
			SOCKBUF_LOCK(&so->so_rcv);
			if ((so->so_rcv.sb_state & SBS_CANTRCVMORE) == 0) {
				terror = cv_timedwait(&qp->rx_cv,
				    SOCKBUF_MTX(&so->so_rcv), 30 * hz);
				if (terror == ETIMEDOUT)
					printf("NVMe/TCP: Timed out after sending terminate request\n");
			}
			SOCKBUF_UNLOCK(&so->so_rcv);
			goto error;
		}

		SOCKBUF_LOCK(&so->so_rcv);
	}
	SOCKBUF_UNLOCK(&so->so_rcv);
	kthread_exit();
}

static int
nvmf_che_soupcall_receive(struct socket *so, void *arg, int waitflag)
{
	struct nvmf_che_qpair *qp = arg;

	cv_signal(&qp->rx_cv);
	return (SU_OK);
}

static int
do_nvmt_data(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	struct nvmf_che_adapter *nca = sc->nvme_ulp_softc;
	const struct cpl_nvmt_data *cpl;
	u_int tid;
	struct toepcb *toep;
	struct nvmf_che_qpair *qp;
	struct socket *so;
	struct inpcb *inp;
	struct tcpcb *tp;
	int len __diagused;

	if (nca->nvmt_data_iqe) {
		cpl = (const void *)(rss + 1);
	} else {
		cpl = mtod(m, const void *);

		/* strip off CPL header */
		m_adj(m, sizeof(*cpl));
	}
	tid = GET_TID(cpl);
	toep = lookup_tid(sc, tid);

	KASSERT(toep->tid == tid, ("%s: toep tid/atid mismatch", __func__));

	len = m->m_pkthdr.len;

	KASSERT(len == be16toh(cpl->length),
	    ("%s: payload length mismatch", __func__));

	inp = toep->inp;
	INP_WLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		CTR(KTR_CXGBE, "%s: tid %u, rx (%d bytes), inp_flags 0x%x",
		    __func__, tid, len, inp->inp_flags);
		INP_WUNLOCK(inp);
		m_freem(m);
		return (0);
	}

	/* Save TCP sequence number. */
	m->m_pkthdr.nvmf_tcp_seq = be32toh(cpl->seq);

	qp = toep->ulpcb;
	so = qp->so;
	SOCKBUF_LOCK(&so->so_rcv);
	mbufq_enqueue(&qp->rx_data, m);
	SOCKBUF_UNLOCK(&so->so_rcv);

	tp = intotcpcb(inp);
	tp->t_rcvtime = ticks;

#ifdef VERBOSE_TRACES
	CTR(KTR_CXGBE, "%s: tid %u len %d seq %u", __func__, tid, len,
	    be32toh(cpl->seq));
#endif

	INP_WUNLOCK(inp);
	return (0);
}

static int
do_nvmt_cmp(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_nvmt_cmp *cpl = mtod(m, const void *);
	u_int tid = GET_TID(cpl);
	struct toepcb *toep = lookup_tid(sc, tid);
	struct nvmf_che_qpair *qp = toep->ulpcb;
	struct socket *so = qp->so;
	struct inpcb *inp = toep->inp;
	u_int hlen __diagused;
	bool empty;

	KASSERT(toep->tid == tid, ("%s: toep tid/atid mismatch", __func__));
	KASSERT(!(toep->flags & TPF_SYNQE),
	    ("%s: toep %p claims to be a synq entry", __func__, toep));

	/* strip off CPL header */
	m_adj(m, sizeof(*cpl));
	hlen = m->m_pkthdr.len;

	KASSERT(hlen == be16toh(cpl->length),
	    ("%s: payload length mismatch", __func__));

	INP_WLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		CTR(KTR_CXGBE, "%s: tid %u, rx (hlen %u), inp_flags 0x%x",
		    __func__, tid, hlen, inp->inp_flags);
		INP_WUNLOCK(inp);
		m_freem(m);
		return (0);
	}

#ifdef VERBOSE_TRACES
	CTR(KTR_CXGBE, "%s: tid %u hlen %u seq %u status %u", __func__, tid,
	    hlen, be32toh(cpl->seq), cpl->status);
#endif

	/* Save TCP sequence number and CPL status. */
	m->m_pkthdr.nvmf_tcp_seq = be32toh(cpl->seq);
	m->m_pkthdr.nvmf_cpl_status = cpl->status;

	SOCKBUF_LOCK(&so->so_rcv);
	empty = mbufq_len(&qp->rx_pdus) == 0;
	mbufq_enqueue(&qp->rx_pdus, m);
	SOCKBUF_UNLOCK(&so->so_rcv);
	INP_WUNLOCK(inp);
	if (empty)
		cv_signal(&qp->rx_cv);
	return (0);
}

static uint16_t
che_alloc_fl_cid(struct nvmf_che_qpair *qp, uint16_t original_cid)
{
	uint16_t new_cid;

	mtx_lock(&qp->fl_cid_lock);
	new_cid = FL_CID_FINDFREE_AT(qp->fl_cid_set, qp->next_cid);
	if (new_cid == 0) {
		new_cid = FL_CID_FINDFREE_AT(qp->fl_cid_set, 0);
		MPASS(new_cid != 0);
	}
	new_cid--;
	FL_CID_BUSY(new_cid, qp->fl_cid_set);
	if (new_cid == CHE_MAX_FL_TAG)
		qp->next_cid = 0;
	else
		qp->next_cid = new_cid + 1;
	qp->fl_cids[new_cid] = original_cid;
	mtx_unlock(&qp->fl_cid_lock);

	return (new_cid | CHE_FL_TAG_MASK);
}

static uint16_t
che_alloc_ddp_cid(struct nvmf_che_qpair *qp, struct nvmf_che_command_buffer *cb)
{
	mtx_assert(&qp->rx_buffers.lock, MA_OWNED);

	return (che_alloc_ddp_tag(qp, cb));
}

static struct mbuf *
che_command_pdu(struct nvmf_che_qpair *qp, struct nvmf_che_capsule *cc)
{
	struct nvmf_capsule *nc = &cc->nc;
	struct nvmf_che_command_buffer *cb;
	struct nvme_sgl_descriptor *sgl;
	struct nvme_tcp_cmd cmd;
	struct mbuf *top, *m;
	uint16_t cid;
	bool use_icd;

	use_icd = false;
	cb = NULL;
	m = NULL;

	if (nc->nc_data.io_len != 0) {
		cb = che_alloc_command_buffer(qp, &nc->nc_data, 0,
		    nc->nc_data.io_len, nc->nc_sqe.cid);
		cb->original_cid = nc->nc_sqe.cid;

		if (nc->nc_send_data && nc->nc_data.io_len <= qp->max_icd) {
			cid = che_alloc_fl_cid(qp, nc->nc_sqe.cid);
			use_icd = true;
			m = nvmf_che_command_buffer_mbuf(cb, 0,
			    nc->nc_data.io_len, NULL, false);
			cb->data_xfered = nc->nc_data.io_len;
			che_release_command_buffer(cb);
		} else if (nc->nc_send_data) {
			cid = che_alloc_fl_cid(qp, nc->nc_sqe.cid);
			cb->cid = htole16(cid);
			mtx_lock(&qp->tx_buffers.lock);
			che_add_command_buffer(&qp->tx_buffers, cb);
			mtx_unlock(&qp->tx_buffers.lock);
		} else {
			mtx_lock(&qp->rx_buffers.lock);
			cid = che_alloc_ddp_cid(qp, cb);
			if (cid == CHE_DDP_NO_TAG) {
				cid = che_alloc_fl_cid(qp, nc->nc_sqe.cid);
				che_add_command_buffer(&qp->rx_buffers, cb);
			}
			cb->cid = htole16(cid);
			mtx_unlock(&qp->rx_buffers.lock);
		}
	} else
		cid = che_alloc_fl_cid(qp, nc->nc_sqe.cid);

#ifdef VERBOSE_TRACES
	CTR(KTR_CXGBE, "%s: tid %u allocated cid 0x%04x for 0x%04x", __func__,
	    qp->toep->tid, cid, nc->nc_sqe.cid);
#endif
	memset(&cmd, 0, sizeof(cmd));
	cmd.common.pdu_type = NVME_TCP_PDU_TYPE_CAPSULE_CMD;
	cmd.ccsqe = nc->nc_sqe;
	cmd.ccsqe.cid = htole16(cid);

	/* Populate SGL in SQE. */
	sgl = &cmd.ccsqe.sgl;
	memset(sgl, 0, sizeof(*sgl));
	sgl->address = 0;
	sgl->length = htole32(nc->nc_data.io_len);
	if (use_icd) {
		/* Use in-capsule data. */
		sgl->type = NVME_SGL_TYPE_ICD;
	} else {
		/* Use a command buffer. */
		sgl->type = NVME_SGL_TYPE_COMMAND_BUFFER;
	}

	top = nvmf_che_construct_pdu(qp, &cmd, sizeof(cmd), m, m != NULL ?
	    nc->nc_data.io_len : 0);
	return (top);
}

static struct mbuf *
che_response_pdu(struct nvmf_che_qpair *qp, struct nvmf_che_capsule *cc)
{
	struct nvmf_capsule *nc = &cc->nc;
	struct nvme_tcp_rsp rsp;

	memset(&rsp, 0, sizeof(rsp));
	rsp.common.pdu_type = NVME_TCP_PDU_TYPE_CAPSULE_RESP;
	rsp.rccqe = nc->nc_cqe;

	return (nvmf_che_construct_pdu(qp, &rsp, sizeof(rsp), NULL, 0));
}

static struct mbuf *
capsule_to_pdu(struct nvmf_che_qpair *qp, struct nvmf_che_capsule *cc)
{
	if (cc->nc.nc_qe_len == sizeof(struct nvme_command))
		return (che_command_pdu(qp, cc));
	else
		return (che_response_pdu(qp, cc));
}

static void
nvmf_che_send(void *arg)
{
	struct nvmf_che_qpair *qp = arg;
	struct nvmf_che_capsule *cc;
	struct socket *so = qp->so;
	struct mbuf *m;
	int error;

	m = NULL;
	SOCKBUF_LOCK(&so->so_snd);
	while (!qp->tx_shutdown) {
		if (so->so_error != 0) {
			error = so->so_error;
			SOCKBUF_UNLOCK(&so->so_snd);
			m_freem(m);
			nvmf_qpair_error(&qp->qp, error);
			SOCKBUF_LOCK(&so->so_snd);
			while (!qp->tx_shutdown)
				cv_wait(&qp->tx_cv, SOCKBUF_MTX(&so->so_snd));
			break;
		}

		if (STAILQ_EMPTY(&qp->tx_capsules)) {
			cv_wait(&qp->tx_cv, SOCKBUF_MTX(&so->so_snd));
			continue;
		}

		/* Convert a capsule into a PDU. */
		cc = STAILQ_FIRST(&qp->tx_capsules);
		STAILQ_REMOVE_HEAD(&qp->tx_capsules, link);
		SOCKBUF_UNLOCK(&so->so_snd);

		m = capsule_to_pdu(qp, cc);
		che_release_capsule(cc);

		nvmf_che_write_pdu(qp, m);

		SOCKBUF_LOCK(&so->so_snd);
	}
	SOCKBUF_UNLOCK(&so->so_snd);
	kthread_exit();
}

static int
nvmf_che_setsockopt(struct socket *so, u_int sspace, u_int rspace)
{
	struct sockopt opt;
	int error, one = 1;

	/* Don't lower the buffer sizes, just enforce a minimum. */
	SOCKBUF_LOCK(&so->so_snd);
	if (sspace < so->so_snd.sb_hiwat)
		sspace = so->so_snd.sb_hiwat;
	SOCKBUF_UNLOCK(&so->so_snd);
	SOCKBUF_LOCK(&so->so_rcv);
	if (rspace < so->so_rcv.sb_hiwat)
		rspace = so->so_rcv.sb_hiwat;
	SOCKBUF_UNLOCK(&so->so_rcv);

	error = soreserve(so, sspace, rspace);
	if (error != 0)
		return (error);
	SOCKBUF_LOCK(&so->so_snd);
	so->so_snd.sb_flags |= SB_AUTOSIZE;
	SOCKBUF_UNLOCK(&so->so_snd);
	SOCKBUF_LOCK(&so->so_rcv);
	so->so_rcv.sb_flags |= SB_AUTOSIZE;
	SOCKBUF_UNLOCK(&so->so_rcv);

	/*
	 * Disable Nagle.
	 */
	bzero(&opt, sizeof(opt));
	opt.sopt_dir = SOPT_SET;
	opt.sopt_level = IPPROTO_TCP;
	opt.sopt_name = TCP_NODELAY;
	opt.sopt_val = &one;
	opt.sopt_valsize = sizeof(one);
	error = sosetopt(so, &opt);
	if (error != 0)
		return (error);

	return (0);
}

static void
t4_nvme_set_tcb_field(struct toepcb *toep, uint16_t word, uint64_t mask,
    uint64_t val)
{
	struct adapter *sc = td_adapter(toep->td);

	t4_set_tcb_field(sc, &toep->ofld_txq->wrq, toep, word, mask, val, 0, 0);
}

static void
set_ulp_mode_nvme(struct toepcb *toep, u_int ulp_submode, uint8_t rxpda)
{
	uint64_t val;

	CTR(KTR_CXGBE, "%s: tid %u, ULP_MODE_NVMET, submode=%#x, rxpda=%u",
	    __func__, toep->tid, ulp_submode, rxpda);

	val = V_TCB_ULP_TYPE(ULP_MODE_NVMET) | V_TCB_ULP_RAW(ulp_submode);
	t4_nvme_set_tcb_field(toep, W_TCB_ULP_TYPE,
	    V_TCB_ULP_TYPE(M_TCB_ULP_TYPE) | V_TCB_ULP_RAW(M_TCB_ULP_RAW), val);

	val = V_TF_RX_FLOW_CONTROL_DISABLE(1ULL);
	t4_nvme_set_tcb_field(toep, W_TCB_T_FLAGS, val, val);

	val = V_TCB_RSVD((rxpda / 4) - 1);
	t4_nvme_set_tcb_field(toep, W_TCB_RSVD, V_TCB_RSVD(M_TCB_RSVD), val);

	/* 0 disables CPL_NVMT_CMP_IMM which is not useful in this driver. */
	val = 0;
	t4_nvme_set_tcb_field(toep, W_TCB_CMP_IMM_SZ,
	    V_TCB_CMP_IMM_SZ(M_TCB_CMP_IMM_SZ), val);
}

static u_int
pdu_max_data_len(const nvlist_t *nvl, u_int max_pdu_len, u_int hlen,
    uint8_t pda)
{
	u_int max_data_len;

	if (nvlist_get_bool(nvl, "header_digests"))
		hlen += sizeof(uint32_t);
	hlen = roundup(hlen, pda);
	max_data_len = max_pdu_len - hlen;
	if (nvlist_get_bool(nvl, "data_digests"))
		max_data_len -= sizeof(uint32_t);
	return (max_data_len);
}

static struct nvmf_qpair *
che_allocate_qpair(bool controller, const nvlist_t *nvl)
{
	struct nvmf_che_adapter *nca;
	struct nvmf_che_qpair *qp;
	struct adapter *sc;
	struct file *fp;
	struct socket *so;
	struct inpcb *inp;
	struct tcpcb *tp;
	struct toepcb *toep;
	cap_rights_t rights;
	u_int max_tx_pdu_len, num_ddp_tags;
	int error, ulp_submode;

	if (!nvlist_exists_number(nvl, "fd") ||
	    !nvlist_exists_number(nvl, "rxpda") ||
	    !nvlist_exists_number(nvl, "txpda") ||
	    !nvlist_exists_bool(nvl, "header_digests") ||
	    !nvlist_exists_bool(nvl, "data_digests") ||
	    !nvlist_exists_number(nvl, "maxr2t") ||
	    !nvlist_exists_number(nvl, "maxh2cdata") ||
	    !nvlist_exists_number(nvl, "max_icd"))
		return (NULL);

	error = fget(curthread, nvlist_get_number(nvl, "fd"),
	    cap_rights_init_one(&rights, CAP_SOCK_CLIENT), &fp);
	if (error != 0)
		return (NULL);
	if (fp->f_type != DTYPE_SOCKET) {
		fdrop(fp, curthread);
		return (NULL);
	}
	so = fp->f_data;
	if (so->so_type != SOCK_STREAM ||
	    so->so_proto->pr_protocol != IPPROTO_TCP) {
		fdrop(fp, curthread);
		return (NULL);
	}

	sc = find_offload_adapter(so);
	if (sc == NULL) {
		fdrop(fp, curthread);
		return (NULL);
	}
	nca = sc->nvme_ulp_softc;

	/*
	 * Controller: Require advertised MAXH2CDATA to be small
	 * enough.
	 */
	if (controller) {
		u_int max_rx_data;

		max_rx_data = pdu_max_data_len(nvl, nca->max_receive_pdu,
		    sizeof(struct nvme_tcp_h2c_data_hdr),
		    nvlist_get_number(nvl, "rxpda"));
		if (nvlist_get_number(nvl, "maxh2cdata") > max_rx_data) {
			fdrop(fp, curthread);
			return (NULL);
		}
	}

	/*
	 * Host: Require the queue size to be small enough that all of
	 * the command ids allocated by nvmf(4) will fit in the
	 * unallocated range.
	 *
	 * XXX: Alternatively this driver could just queue commands
	 * when an unallocated ID isn't available.
	 */
	if (!controller) {
		u_int num_commands;

		num_commands = nvlist_get_number(nvl, "qsize") - 1;
		if (nvlist_get_bool(nvl, "admin"))
			num_commands += 8;	/* Max AER */
		if (num_commands > CHE_NUM_FL_TAGS) {
			fdrop(fp, curthread);
			return (NULL);
		}
	}

	qp = malloc(sizeof(*qp), M_NVMF_CHE, M_WAITOK | M_ZERO);
	qp->txpda = nvlist_get_number(nvl, "txpda");
	qp->rxpda = nvlist_get_number(nvl, "rxpda");
	qp->header_digests = nvlist_get_bool(nvl, "header_digests");
	qp->data_digests = nvlist_get_bool(nvl, "data_digests");
	qp->maxr2t = nvlist_get_number(nvl, "maxr2t");
	if (controller)
		qp->maxh2cdata = nvlist_get_number(nvl, "maxh2cdata");

	if (controller) {
		/* NB: maxr2t is 0's based. */
		qp->num_fl_ttags = MIN(CHE_NUM_FL_TAGS,
		    nvlist_get_number(nvl, "qsize") *
		    ((uint64_t)qp->maxr2t + 1));
		qp->open_fl_ttags = mallocarray(qp->num_fl_ttags,
		    sizeof(*qp->open_fl_ttags), M_NVMF_CHE, M_WAITOK | M_ZERO);
	} else {
		qp->fl_cids = mallocarray(CHE_NUM_FL_TAGS,
		    sizeof(*qp->fl_cids), M_NVMF_CHE, M_WAITOK | M_ZERO);
		qp->fl_cid_set = malloc(sizeof(*qp->fl_cid_set), M_NVMF_CHE,
		    M_WAITOK);
		FL_CID_INIT(qp->fl_cid_set);
		mtx_init(&qp->fl_cid_lock,  "nvmf/che fl cids", NULL, MTX_DEF);
	}

	inp = sotoinpcb(so);
	INP_WLOCK(inp);
	tp = intotcpcb(inp);
	if (inp->inp_flags & INP_DROPPED) {
		INP_WUNLOCK(inp);
		free(qp->fl_cid_set, M_NVMF_CHE);
		free(qp->fl_cids, M_NVMF_CHE);
		free(qp->open_fl_ttags, M_NVMF_CHE);
		free(qp, M_NVMF_CHE);
		fdrop(fp, curthread);
		return (NULL);
	}

	MPASS(tp->t_flags & TF_TOE);
	MPASS(tp->tod != NULL);
	MPASS(tp->t_toe != NULL);
	toep = tp->t_toe;
	MPASS(toep->vi->adapter == sc);

	if (ulp_mode(toep) != ULP_MODE_NONE) {
		INP_WUNLOCK(inp);
		free(qp->fl_cid_set, M_NVMF_CHE);
		free(qp->fl_cids, M_NVMF_CHE);
		free(qp->open_fl_ttags, M_NVMF_CHE);
		free(qp, M_NVMF_CHE);
		fdrop(fp, curthread);
		return (NULL);
	}

	/* Claim socket from file descriptor. */
	fp->f_ops = &badfileops;
	fp->f_data = NULL;

	qp->so = so;
	qp->toep = toep;
	qp->nca = nca;
	refcount_init(&qp->refs, 1);

	/* NB: C2H and H2C headers are the same size. */
	qp->max_rx_data = pdu_max_data_len(nvl, nca->max_receive_pdu,
	    sizeof(struct nvme_tcp_c2h_data_hdr), qp->rxpda);
	qp->max_tx_data = pdu_max_data_len(nvl, nca->max_transmit_pdu,
	    sizeof(struct nvme_tcp_c2h_data_hdr), qp->txpda);
	if (!controller) {
		qp->max_tx_data = min(qp->max_tx_data,
		    nvlist_get_number(nvl, "maxh2cdata"));
		qp->max_icd = min(nvlist_get_number(nvl, "max_icd"),
		    pdu_max_data_len(nvl, nca->max_transmit_pdu,
		    sizeof(struct nvme_tcp_cmd), qp->txpda));
	} else {
		/*
		 * IOCCSZ represents the size of a logical command
		 * capsule including the 64 byte SQE and the
		 * in-capsule data.  Use pdu_max_data_len to compute
		 * the maximum supported ICD length.
		 */
		qp->max_ioccsz = rounddown(pdu_max_data_len(nvl,
		    nca->max_receive_pdu, sizeof(struct nvme_tcp_cmd),
		    qp->rxpda), 16) + sizeof(struct nvme_command);
	}

	ulp_submode = 0;
	if (qp->header_digests)
		ulp_submode |= FW_NVMET_ULPSUBMODE_HCRC;
	if (qp->data_digests)
		ulp_submode |= FW_NVMET_ULPSUBMODE_DCRC;
	if (!controller)
		ulp_submode |= FW_NVMET_ULPSUBMODE_ING_DIR;

	max_tx_pdu_len = sizeof(struct nvme_tcp_h2c_data_hdr);
	if (qp->header_digests)
		max_tx_pdu_len += sizeof(uint32_t);
	max_tx_pdu_len = roundup(max_tx_pdu_len, qp->txpda);
	max_tx_pdu_len += qp->max_tx_data;
	if (qp->data_digests)
		max_tx_pdu_len += sizeof(uint32_t);

	/* TODO: ISO limits */

	if (controller) {
		/* Use the SUCCESS flag if SQ flow control is disabled. */
		qp->send_success = !nvlist_get_bool(nvl, "sq_flow_control");
	}

	toep->params.ulp_mode = ULP_MODE_NVMET;
	toep->ulpcb = qp;

	send_txdataplen_max_flowc_wr(sc, toep,
	    roundup(/* max_iso_pdus * */ max_tx_pdu_len, tp->t_maxseg));
	set_ulp_mode_nvme(toep, ulp_submode, qp->rxpda);
	INP_WUNLOCK(inp);

	fdrop(fp, curthread);

	error = nvmf_che_setsockopt(so, max_tx_pdu_len, nca->max_receive_pdu);
	if (error != 0) {
		free(qp->fl_cid_set, M_NVMF_CHE);
		free(qp->fl_cids, M_NVMF_CHE);
		free(qp->open_fl_ttags, M_NVMF_CHE);
		free(qp, M_NVMF_CHE);
		return (NULL);
	}

	num_ddp_tags = ddp_tags_per_qp;
	if (num_ddp_tags > 0) {
		qp->tpt_offset = t4_stag_alloc(sc, num_ddp_tags);
		if (qp->tpt_offset != T4_STAG_UNSET) {
#ifdef VERBOSE_TRACES
			CTR(KTR_CXGBE,
			    "%s: tid %u using %u tags at offset 0x%x",
			    __func__, toep->tid, num_ddp_tags, qp->tpt_offset);
#endif
			qp->num_ddp_tags = num_ddp_tags;
			qp->open_ddp_tags = mallocarray(qp->num_ddp_tags,
			    sizeof(*qp->open_ddp_tags), M_NVMF_CHE, M_WAITOK |
			    M_ZERO);

			t4_nvme_set_tcb_field(toep, W_TCB_TPT_OFFSET,
			    M_TCB_TPT_OFFSET, V_TCB_TPT_OFFSET(qp->tpt_offset));
		}
	}

	TAILQ_INIT(&qp->rx_buffers.head);
	TAILQ_INIT(&qp->tx_buffers.head);
	mtx_init(&qp->rx_buffers.lock, "nvmf/che rx buffers", NULL, MTX_DEF);
	mtx_init(&qp->tx_buffers.lock, "nvmf/che tx buffers", NULL, MTX_DEF);

	cv_init(&qp->rx_cv, "-");
	cv_init(&qp->tx_cv, "-");
	mbufq_init(&qp->rx_data, 0);
	mbufq_init(&qp->rx_pdus, 0);
	STAILQ_INIT(&qp->tx_capsules);

	/* Register socket upcall for receive to handle remote FIN. */
	SOCKBUF_LOCK(&so->so_rcv);
	soupcall_set(so, SO_RCV, nvmf_che_soupcall_receive, qp);
	SOCKBUF_UNLOCK(&so->so_rcv);

	/* Spin up kthreads. */
	error = kthread_add(nvmf_che_receive, qp, NULL, &qp->rx_thread, 0, 0,
	    "nvmef che rx");
	if (error != 0) {
		che_free_qpair(&qp->qp);
		return (NULL);
	}
	error = kthread_add(nvmf_che_send, qp, NULL, &qp->tx_thread, 0, 0,
	    "nvmef che tx");
	if (error != 0) {
		che_free_qpair(&qp->qp);
		return (NULL);
	}

	return (&qp->qp);
}

static void
che_release_qpair(struct nvmf_che_qpair *qp)
{
	if (refcount_release(&qp->refs))
		free(qp, M_NVMF_CHE);
}

static void
che_free_qpair(struct nvmf_qpair *nq)
{
	struct nvmf_che_qpair *qp = CQP(nq);
	struct nvmf_che_command_buffer *ncb, *cb;
	struct nvmf_che_capsule *ncc, *cc;
	struct socket *so = qp->so;
	struct toepcb *toep = qp->toep;
	struct inpcb *inp = sotoinpcb(so);

	/* Shut down kthreads. */
	SOCKBUF_LOCK(&so->so_snd);
	qp->tx_shutdown = true;
	if (qp->tx_thread != NULL) {
		cv_signal(&qp->tx_cv);
		mtx_sleep(qp->tx_thread, SOCKBUF_MTX(&so->so_snd), 0,
		    "nvchetx", 0);
	}
	SOCKBUF_UNLOCK(&so->so_snd);

	SOCKBUF_LOCK(&so->so_rcv);
	qp->rx_shutdown = true;
	if (qp->rx_thread != NULL) {
		cv_signal(&qp->rx_cv);
		mtx_sleep(qp->rx_thread, SOCKBUF_MTX(&so->so_rcv), 0,
		    "nvcherx", 0);
	}
	soupcall_clear(so, SO_RCV);
	SOCKBUF_UNLOCK(&so->so_rcv);
	mbufq_drain(&qp->rx_data);
	mbufq_drain(&qp->rx_pdus);

	STAILQ_FOREACH_SAFE(cc, &qp->tx_capsules, link, ncc) {
		nvmf_abort_capsule_data(&cc->nc, ECONNABORTED);
		che_release_capsule(cc);
	}

	cv_destroy(&qp->tx_cv);
	cv_destroy(&qp->rx_cv);

	if (qp->open_fl_ttags != NULL) {
		for (u_int i = 0; i < qp->num_fl_ttags; i++) {
			cb = qp->open_fl_ttags[i];
			if (cb != NULL) {
				cb->cc->active_r2ts--;
				cb->error = ECONNABORTED;
				che_release_command_buffer(cb);
			}
		}
		free(qp->open_fl_ttags, M_NVMF_CHE);
	}
	if (qp->num_ddp_tags != 0) {
		for (u_int i = 0; i < qp->num_ddp_tags; i++) {
			cb = qp->open_ddp_tags[i];
			if (cb != NULL) {
				if (cb->cc != NULL)
					cb->cc->active_r2ts--;
				cb->error = ECONNABORTED;
				mtx_lock(&qp->rx_buffers.lock);
				che_free_ddp_tag(qp, cb, cb->ttag);
				mtx_unlock(&qp->rx_buffers.lock);
				che_release_command_buffer(cb);
			}
		}
		free(qp->open_ddp_tags, M_NVMF_CHE);
	}

	mtx_lock(&qp->rx_buffers.lock);
	TAILQ_FOREACH_SAFE(cb, &qp->rx_buffers.head, link, ncb) {
		che_remove_command_buffer(&qp->rx_buffers, cb);
		mtx_unlock(&qp->rx_buffers.lock);
#ifdef INVARIANTS
		if (cb->cc != NULL)
			cb->cc->pending_r2ts--;
#endif
		cb->error = ECONNABORTED;
		che_release_command_buffer(cb);
		mtx_lock(&qp->rx_buffers.lock);
	}
	mtx_destroy(&qp->rx_buffers.lock);

	mtx_lock(&qp->tx_buffers.lock);
	TAILQ_FOREACH_SAFE(cb, &qp->tx_buffers.head, link, ncb) {
		che_remove_command_buffer(&qp->tx_buffers, cb);
		mtx_unlock(&qp->tx_buffers.lock);
		cb->error = ECONNABORTED;
		che_release_command_buffer(cb);
		mtx_lock(&qp->tx_buffers.lock);
	}
	mtx_destroy(&qp->tx_buffers.lock);

	if (qp->num_ddp_tags != 0)
		t4_stag_free(qp->nca->sc, qp->tpt_offset, qp->num_ddp_tags);

	if (!qp->qp.nq_controller) {
		free(qp->fl_cids, M_NVMF_CHE);
		free(qp->fl_cid_set, M_NVMF_CHE);
		mtx_destroy(&qp->fl_cid_lock);
	}

	INP_WLOCK(inp);
	toep->ulpcb = NULL;
	mbufq_drain(&toep->ulp_pduq);

	/*
	 * Grab a reference to use when waiting for the final CPL to
	 * be received.  If toep->inp is NULL, then
	 * final_cpl_received() has already been called (e.g.  due to
	 * the peer sending a RST).
	 */
	if (toep->inp != NULL) {
		toep = hold_toepcb(toep);
		toep->flags |= TPF_WAITING_FOR_FINAL;
	} else
		toep = NULL;
	INP_WUNLOCK(inp);

	soclose(so);

	/*
	 * Wait for the socket to fully close.  This ensures any
	 * pending received data has been received (and in particular,
	 * any data that would be received by DDP has been handled).
	 */
	if (toep != NULL) {
		struct mtx *lock = mtx_pool_find(mtxpool_sleep, toep);

		mtx_lock(lock);
		while ((toep->flags & TPF_WAITING_FOR_FINAL) != 0)
			mtx_sleep(toep, lock, PSOCK, "conclo2", 0);
		mtx_unlock(lock);
		free_toepcb(toep);
	}

	che_release_qpair(qp);
}

static uint32_t
che_max_ioccsz(struct nvmf_qpair *nq)
{
	struct nvmf_che_qpair *qp = CQP(nq);

	/*
	 * Limit the command capsule size so that with maximum ICD it
	 * fits within the limit of the largest PDU the adapter can
	 * receive.
	 */
	return (qp->max_ioccsz);
}

static uint64_t
che_max_xfer_size(struct nvmf_qpair *nq)
{
	struct nvmf_che_qpair *qp = CQP(nq);

	/*
	 * Limit host transfers to the size of the data payload in the
	 * largest PDU the adapter can receive.
	 */
	return (qp->max_rx_data);
}

static struct nvmf_capsule *
che_allocate_capsule(struct nvmf_qpair *nq, int how)
{
	struct nvmf_che_qpair *qp = CQP(nq);
	struct nvmf_che_capsule *cc;

	cc = malloc(sizeof(*cc), M_NVMF_CHE, how | M_ZERO);
	if (cc == NULL)
		return (NULL);
	refcount_init(&cc->refs, 1);
	refcount_acquire(&qp->refs);
	return (&cc->nc);
}

static void
che_release_capsule(struct nvmf_che_capsule *cc)
{
	struct nvmf_che_qpair *qp = CQP(cc->nc.nc_qpair);

	if (!refcount_release(&cc->refs))
		return;

	MPASS(cc->active_r2ts == 0);
	MPASS(cc->pending_r2ts == 0);

	nvmf_che_free_pdu(&cc->rx_pdu);
	free(cc, M_NVMF_CHE);
	che_release_qpair(qp);
}

static void
che_free_capsule(struct nvmf_capsule *nc)
{
	che_release_capsule(CCAP(nc));
}

static int
che_transmit_capsule(struct nvmf_capsule *nc)
{
	struct nvmf_che_qpair *qp = CQP(nc->nc_qpair);
	struct nvmf_che_capsule *cc = CCAP(nc);
	struct socket *so = qp->so;

	refcount_acquire(&cc->refs);
	SOCKBUF_LOCK(&so->so_snd);
	STAILQ_INSERT_TAIL(&qp->tx_capsules, cc, link);
	cv_signal(&qp->tx_cv);
	SOCKBUF_UNLOCK(&so->so_snd);
	return (0);
}

static uint8_t
che_validate_command_capsule(struct nvmf_capsule *nc)
{
	struct nvmf_che_capsule *cc = CCAP(nc);
	struct nvme_sgl_descriptor *sgl;

	KASSERT(cc->rx_pdu.hdr != NULL, ("capsule wasn't received"));

	sgl = &nc->nc_sqe.sgl;
	switch (sgl->type) {
	case NVME_SGL_TYPE_ICD:
		if (cc->rx_pdu.data_len != le32toh(sgl->length)) {
			printf("NVMe/TCP: Command Capsule with mismatched ICD length\n");
			return (NVME_SC_DATA_SGL_LENGTH_INVALID);
		}
		break;
	case NVME_SGL_TYPE_COMMAND_BUFFER:
		if (cc->rx_pdu.data_len != 0) {
			printf("NVMe/TCP: Command Buffer SGL with ICD\n");
			return (NVME_SC_INVALID_FIELD);
		}
		break;
	default:
		printf("NVMe/TCP: Invalid SGL type in Command Capsule\n");
		return (NVME_SC_SGL_DESCRIPTOR_TYPE_INVALID);
	}

	if (sgl->address != 0) {
		printf("NVMe/TCP: Invalid SGL offset in Command Capsule\n");
		return (NVME_SC_SGL_OFFSET_INVALID);
	}

	return (NVME_SC_SUCCESS);
}

static size_t
che_capsule_data_len(const struct nvmf_capsule *nc)
{
	MPASS(nc->nc_qe_len == sizeof(struct nvme_command));
	return (le32toh(nc->nc_sqe.sgl.length));
}

static void
che_receive_r2t_data(struct nvmf_capsule *nc, uint32_t data_offset,
    struct nvmf_io_request *io)
{
	struct nvmf_che_qpair *qp = CQP(nc->nc_qpair);
	struct nvmf_che_capsule *cc = CCAP(nc);
	struct nvmf_che_command_buffer *cb;

	cb = che_alloc_command_buffer(qp, io, data_offset, io->io_len,
	    nc->nc_sqe.cid);

	cb->cc = cc;
	refcount_acquire(&cc->refs);

	/*
	 * If this command has too many active R2Ts or there are no
	 * available transfer tags, queue the request for later.
	 *
	 * NB: maxr2t is 0's based.
	 */
	mtx_lock(&qp->rx_buffers.lock);
	if (cc->active_r2ts > qp->maxr2t ||
	    !nvmf_che_allocate_ttag(qp, cb)) {
#ifdef INVARIANTS
		cc->pending_r2ts++;
#endif
		TAILQ_INSERT_TAIL(&qp->rx_buffers.head, cb, link);
		mtx_unlock(&qp->rx_buffers.lock);
		return;
	}
	mtx_unlock(&qp->rx_buffers.lock);

	che_send_r2t(qp, nc->nc_sqe.cid, cb->ttag, data_offset, io->io_len);
}

static void
che_receive_icd_data(struct nvmf_capsule *nc, uint32_t data_offset,
    struct nvmf_io_request *io)
{
	struct nvmf_che_capsule *cc = CCAP(nc);

	/*
	 * The header is in rx_pdu.m, the padding is discarded, and
	 * the data starts at rx_pdu.m->m_next.
	 */
	mbuf_copyto_io(cc->rx_pdu.m->m_next, data_offset, io->io_len, io, 0);
	nvmf_complete_io_request(io, io->io_len, 0);
}

static int
che_receive_controller_data(struct nvmf_capsule *nc, uint32_t data_offset,
    struct nvmf_io_request *io)
{
	struct nvme_sgl_descriptor *sgl;
	size_t data_len;

	if (nc->nc_qe_len != sizeof(struct nvme_command) ||
	    !nc->nc_qpair->nq_controller)
		return (EINVAL);

	sgl = &nc->nc_sqe.sgl;
	data_len = le32toh(sgl->length);
	if (data_offset + io->io_len > data_len)
		return (EFBIG);

	if (sgl->type == NVME_SGL_TYPE_ICD)
		che_receive_icd_data(nc, data_offset, io);
	else
		che_receive_r2t_data(nc, data_offset, io);
	return (0);
}

/* NB: cid is little-endian already. */
static void
che_send_c2h_pdu(struct nvmf_che_qpair *qp, uint16_t cid, uint32_t data_offset,
    struct mbuf *m, size_t len, bool last_pdu, bool success)
{
	struct nvme_tcp_c2h_data_hdr c2h;
	struct mbuf *top;

	memset(&c2h, 0, sizeof(c2h));
	c2h.common.pdu_type = NVME_TCP_PDU_TYPE_C2H_DATA;
	if (last_pdu)
		c2h.common.flags |= NVME_TCP_C2H_DATA_FLAGS_LAST_PDU;
	if (success)
		c2h.common.flags |= NVME_TCP_C2H_DATA_FLAGS_SUCCESS;
	c2h.cccid = cid;
	c2h.datao = htole32(data_offset);
	c2h.datal = htole32(len);

	top = nvmf_che_construct_pdu(qp, &c2h, sizeof(c2h), m, len);
	nvmf_che_write_pdu(qp, top);
}

static u_int
che_send_controller_data(struct nvmf_capsule *nc, uint32_t data_offset,
    struct mbuf *m, size_t len)
{
	struct nvmf_che_qpair *qp = CQP(nc->nc_qpair);
	struct nvme_sgl_descriptor *sgl;
	uint32_t data_len;
	bool last_pdu, last_xfer;

	if (nc->nc_qe_len != sizeof(struct nvme_command) ||
	    !qp->qp.nq_controller) {
		m_freem(m);
		return (NVME_SC_INVALID_FIELD);
	}

	sgl = &nc->nc_sqe.sgl;
	data_len = le32toh(sgl->length);
	if (data_offset + len > data_len) {
		m_freem(m);
		return (NVME_SC_INVALID_FIELD);
	}
	last_xfer = (data_offset + len == data_len);

	if (sgl->type != NVME_SGL_TYPE_COMMAND_BUFFER) {
		m_freem(m);
		return (NVME_SC_INVALID_FIELD);
	}

	KASSERT(data_offset == CCAP(nc)->tx_data_offset,
	    ("%s: starting data_offset %u doesn't match end of previous xfer %u",
	    __func__, data_offset, CCAP(nc)->tx_data_offset));

	/* Queue one or more C2H_DATA PDUs containing the data from 'm'. */
	while (m != NULL) {
		struct mbuf *n;
		uint32_t todo;

		if (m->m_len > qp->max_tx_data) {
			n = m_split(m, qp->max_tx_data, M_WAITOK);
			todo = m->m_len;
		} else {
			struct mbuf *p;

			todo = m->m_len;
			p = m;
			n = p->m_next;
			while (n != NULL) {
				if (todo + n->m_len > qp->max_tx_data) {
					p->m_next = NULL;
					break;
				}
				todo += n->m_len;
				p = n;
				n = p->m_next;
			}
			MPASS(m_length(m, NULL) == todo);
		}

		last_pdu = (n == NULL && last_xfer);
		che_send_c2h_pdu(qp, nc->nc_sqe.cid, data_offset, m, todo,
		    last_pdu, last_pdu && qp->send_success);

		data_offset += todo;
		data_len -= todo;
		m = n;
	}
	MPASS(data_len == 0);

#ifdef INVARIANTS
	CCAP(nc)->tx_data_offset = data_offset;
#endif
	if (!last_xfer)
		return (NVMF_MORE);
	else if (qp->send_success)
		return (NVMF_SUCCESS_SENT);
	else
		return (NVME_SC_SUCCESS);
}

struct nvmf_transport_ops che_ops = {
	.allocate_qpair = che_allocate_qpair,
	.free_qpair = che_free_qpair,
	.max_ioccsz = che_max_ioccsz,
	.max_xfer_size = che_max_xfer_size,
	.allocate_capsule = che_allocate_capsule,
	.free_capsule = che_free_capsule,
	.transmit_capsule = che_transmit_capsule,
	.validate_command_capsule = che_validate_command_capsule,
	.capsule_data_len = che_capsule_data_len,
	.receive_controller_data = che_receive_controller_data,
	.send_controller_data = che_send_controller_data,
	.trtype = NVMF_TRTYPE_TCP,
	.priority = 10,
};

NVMF_TRANSPORT(che, che_ops);

static void
read_pdu_limits(struct adapter *sc, u_int *max_tx_pdu_len,
    uint32_t *max_rx_pdu_len)
{
	uint32_t tx_len, rx_len, r, v;

	/* Copied from cxgbei, but not sure if this is correct. */
	rx_len = t4_read_reg(sc, A_TP_PMM_RX_PAGE_SIZE);
	tx_len = t4_read_reg(sc, A_TP_PMM_TX_PAGE_SIZE);

	r = t4_read_reg(sc, A_TP_PARA_REG2);
	rx_len = min(rx_len, G_MAXRXDATA(r));
	tx_len = min(tx_len, G_MAXRXDATA(r));

	r = t4_read_reg(sc, A_TP_PARA_REG7);
	v = min(G_PMMAXXFERLEN0(r), G_PMMAXXFERLEN1(r));
	rx_len = min(rx_len, v);
	tx_len = min(tx_len, v);

	/* Cannot be larger than 32KB - 256. */
	rx_len = min(rx_len, 32512);
	tx_len = min(tx_len, 32512);

	*max_tx_pdu_len = tx_len;
	*max_rx_pdu_len = rx_len;
}

static int
nvmf_che_init(struct adapter *sc, struct nvmf_che_adapter *nca)
{
	struct sysctl_oid *oid;
	struct sysctl_oid_list *children;
	uint32_t val;

	read_pdu_limits(sc, &nca->max_transmit_pdu, &nca->max_receive_pdu);
	if (nca->max_transmit_pdu > che_max_transmit_pdu)
		nca->max_transmit_pdu = che_max_transmit_pdu;
	if (nca->max_receive_pdu > che_max_receive_pdu)
		nca->max_receive_pdu = che_max_receive_pdu;
	val = t4_read_reg(sc, A_SGE_CONTROL2);
	nca->nvmt_data_iqe = (val & F_RXCPLMODE_NVMT) != 0;

	sysctl_ctx_init(&nca->ctx);
	oid = device_get_sysctl_tree(sc->dev);	/* dev.che.X */
	children = SYSCTL_CHILDREN(oid);

	oid = SYSCTL_ADD_NODE(&nca->ctx, children, OID_AUTO, "nvme",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "NVMe ULP settings");
	children = SYSCTL_CHILDREN(oid);

	nca->ddp_threshold = 8192;
	SYSCTL_ADD_UINT(&nca->ctx, children, OID_AUTO, "ddp_threshold",
	    CTLFLAG_RW, &nca->ddp_threshold, 0, "Rx zero copy threshold");

	SYSCTL_ADD_UINT(&nca->ctx, children, OID_AUTO, "max_transmit_pdu",
	    CTLFLAG_RW, &nca->max_transmit_pdu, 0,
	    "Maximum size of a transmitted PDU");

	SYSCTL_ADD_UINT(&nca->ctx, children, OID_AUTO, "max_receive_pdu",
	    CTLFLAG_RW, &nca->max_receive_pdu, 0,
	    "Maximum size of a received PDU");

	return (0);
}

static void
nvmf_che_destroy(struct nvmf_che_adapter *nca)
{
	sysctl_ctx_free(&nca->ctx);
	free(nca, M_CXGBE);
}

static int
nvmf_che_activate(struct adapter *sc)
{
	struct nvmf_che_adapter *nca;
	int rc;

	ASSERT_SYNCHRONIZED_OP(sc);

	if (uld_active(sc, ULD_NVME)) {
		KASSERT(0, ("%s: NVMe offload already enabled on adapter %p",
		    __func__, sc));
		return (0);
	}

	if ((sc->nvmecaps & FW_CAPS_CONFIG_NVME_TCP) == 0) {
		device_printf(sc->dev,
		    "not NVMe offload capable, or capability disabled\n");
		return (ENOSYS);
	}

	/* per-adapter softc for NVMe */
	nca = malloc(sizeof(*nca), M_CXGBE, M_ZERO | M_WAITOK);
	nca->sc = sc;

	rc = nvmf_che_init(sc, nca);
	if (rc != 0) {
		free(nca, M_CXGBE);
		return (rc);
	}

	sc->nvme_ulp_softc = nca;

	return (0);
}

static int
nvmf_che_deactivate(struct adapter *sc)
{
	struct nvmf_che_adapter *nca = sc->nvme_ulp_softc;

	ASSERT_SYNCHRONIZED_OP(sc);

	if (nca != NULL) {
		nvmf_che_destroy(nca);
		sc->nvme_ulp_softc = NULL;
	}

	return (0);
}

static void
nvmf_che_activate_all(struct adapter *sc, void *arg __unused)
{
	if (begin_synchronized_op(sc, NULL, SLEEP_OK | INTR_OK, "t7nvact") != 0)
		return;

	/* Activate NVMe if any port on this adapter has IFCAP_TOE enabled. */
	if (sc->offload_map && !uld_active(sc, ULD_NVME))
		(void) t4_activate_uld(sc, ULD_NVME);

	end_synchronized_op(sc, 0);
}

static void
nvmf_che_deactivate_all(struct adapter *sc, void *arg __unused)
{
	if (begin_synchronized_op(sc, NULL, SLEEP_OK | INTR_OK, "t7nvdea") != 0)
		return;

	if (uld_active(sc, ULD_NVME))
	    (void) t4_deactivate_uld(sc, ULD_NVME);

	end_synchronized_op(sc, 0);
}

static struct uld_info nvmf_che_uld_info = {
	.uld_activate = nvmf_che_activate,
	.uld_deactivate = nvmf_che_deactivate,
};

static int
nvmf_che_mod_load(void)
{
	int rc;

	t4_register_cpl_handler(CPL_NVMT_CMP, do_nvmt_cmp);
	t4_register_cpl_handler(CPL_NVMT_DATA, do_nvmt_data);

	rc = t4_register_uld(&nvmf_che_uld_info, ULD_NVME);
	if (rc != 0)
		return (rc);

	t4_iterate(nvmf_che_activate_all, NULL);

	return (rc);
}

static int
nvmf_che_mod_unload(void)
{
	t4_iterate(nvmf_che_deactivate_all, NULL);

	if (t4_unregister_uld(&nvmf_che_uld_info, ULD_NVME) == EBUSY)
		return (EBUSY);

	t4_register_cpl_handler(CPL_NVMT_CMP, NULL);
	t4_register_cpl_handler(CPL_NVMT_DATA, NULL);

	return (0);
}
#endif

static int
nvmf_che_modevent(module_t mod, int cmd, void *arg)
{
	int rc;

#ifdef TCP_OFFLOAD
	switch (cmd) {
	case MOD_LOAD:
		rc = nvmf_che_mod_load();
		break;
	case MOD_UNLOAD:
		rc = nvmf_che_mod_unload();
		break;
	default:
		rc = EOPNOTSUPP;
		break;
	}
#else
	printf("nvmf_che: compiled without TCP_OFFLOAD support.\n");
	rc = EOPNOTSUPP;
#endif

	return (rc);
}

static moduledata_t nvmf_che_mod = {
	"nvmf_che",
	nvmf_che_modevent,
	NULL,
};

MODULE_VERSION(nvmf_che, 1);
DECLARE_MODULE(nvmf_che, nvmf_che_mod, SI_SUB_EXEC, SI_ORDER_ANY);
MODULE_DEPEND(nvmf_che, t4_tom, 1, 1, 1);
MODULE_DEPEND(nvmf_che, cxgbe, 1, 1, 1);
