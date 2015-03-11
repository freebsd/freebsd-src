/*-
 * Copyright (c) 2012 Chelsio Communications, Inc.
 * All rights reserved.
 *
 * Chelsio T5xx iSCSI driver
 * cxgbei_ulp2_ddp.c: Chelsio iSCSI DDP Manager.
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

static inline int
cxgbei_counter_dec_and_read(volatile int *p)
{
	atomic_subtract_acq_int(p, 1);
	return atomic_load_acq_int(p);
}

static inline int
get_order(unsigned long size)
{
	int order;

	size = (size - 1) >> PAGE_SHIFT;
	order = 0;
	while (size) {
		order++;
		size >>= 1;
	}
	return (order);
}

/*
 * Map a single buffer address.
 */
static void
ulp2_dma_map_addr(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	bus_addr_t *ba = arg;
	if (error)
		return;

	KASSERT(nseg == 1, ("%s: %d segments returned!", __func__, nseg));

	*ba = segs->ds_addr;
}

static int
ulp2_dma_tag_create(struct cxgbei_ulp2_ddp_info *ddp)
{
	int rc;

	rc = bus_dma_tag_create(NULL, 1, 0, BUS_SPACE_MAXADDR,
		BUS_SPACE_MAXADDR, NULL, NULL, UINT32_MAX , 8,
		BUS_SPACE_MAXSIZE, BUS_DMA_ALLOCNOW, NULL, NULL,
		&ddp->ulp_ddp_tag);

	if (rc != 0) {
		printf("%s(%d): bus_dma_tag_create() "
			"failed (rc = %d)!\n",
			__FILE__, __LINE__, rc);
		return rc;
	}
	return 0;
}

/*
 * iSCSI Direct Data Placement
 *
 * T4/5 ulp2 h/w can directly place the iSCSI Data-In or Data-Out PDU's
 * payload into pre-posted final destination host-memory buffers based on the
 * Initiator Task Tag (ITT) in Data-In or Target Task Tag (TTT) in Data-Out
 * PDUs.
 *
 * The host memory address is programmed into h/w in the format of pagepod
 * entries.
 * The location of the pagepod entry is encoded into ddp tag which is used or
 * is the base for ITT/TTT.
 */

unsigned char ddp_page_order[DDP_PGIDX_MAX] = {0, 1, 2, 4};
unsigned char ddp_page_shift[DDP_PGIDX_MAX] = {12, 13, 14, 16};
unsigned char page_idx = DDP_PGIDX_MAX;

static inline int
ddp_find_unused_entries(struct cxgbei_ulp2_ddp_info *ddp,
			  unsigned int start, unsigned int max,
			  unsigned int count, unsigned int *idx,
			  struct cxgbei_ulp2_gather_list *gl)
{
	unsigned int i, j, k;

	/* not enough entries */
	if ((max - start) < count)
		return EBUSY;

	max -= count;
	mtx_lock(&ddp->map_lock);
	for (i = start; i < max;) {
		for (j = 0, k = i; j < count; j++, k++) {
			if (ddp->gl_map[k])
				break;
		}
		if (j == count) {
			for (j = 0, k = i; j < count; j++, k++)
				ddp->gl_map[k] = gl;
			mtx_unlock(&ddp->map_lock);
			*idx = i;
			return 0;
		}
		i += j + 1;
	}
	mtx_unlock(&ddp->map_lock);
	return EBUSY;
}

static inline void
ddp_unmark_entries(struct cxgbei_ulp2_ddp_info *ddp,
		      int start, int count)
{
	mtx_lock(&ddp->map_lock);
	memset(&ddp->gl_map[start], 0,
	       count * sizeof(struct cxgbei_ulp2_gather_list *));
	mtx_unlock(&ddp->map_lock);
}

/**
 * cxgbei_ulp2_ddp_find_page_index - return ddp page index for a given page size
 * @pgsz: page size
 * return the ddp page index, if no match is found return DDP_PGIDX_MAX.
 */
int
cxgbei_ulp2_ddp_find_page_index(unsigned long pgsz)
{
	int i;

	for (i = 0; i < DDP_PGIDX_MAX; i++) {
		if (pgsz == (1UL << ddp_page_shift[i]))
			return i;
	}
	CTR1(KTR_CXGBE, "ddp page size 0x%lx not supported.\n", pgsz);
	return DDP_PGIDX_MAX;
}

static int
cxgbei_ulp2_ddp_adjust_page_table(void)
{
	int i;
	unsigned int base_order, order;

	if (PAGE_SIZE < (1UL << ddp_page_shift[0])) {
		CTR2(KTR_CXGBE, "PAGE_SIZE %u too small, min. %lu.\n",
				PAGE_SIZE, 1UL << ddp_page_shift[0]);
		return EINVAL;
	}

	base_order = get_order(1UL << ddp_page_shift[0]);
	order = get_order(1 << PAGE_SHIFT);
	for (i = 0; i < DDP_PGIDX_MAX; i++) {
		/* first is the kernel page size, then just doubling the size */
		ddp_page_order[i] = order - base_order + i;
		ddp_page_shift[i] = PAGE_SHIFT + i;
	}
	return 0;
}


static inline void
ddp_gl_unmap(struct toedev *tdev,
		struct cxgbei_ulp2_gather_list *gl)
{
	int i;
	struct adapter *sc = tdev->tod_softc;
	struct cxgbei_ulp2_ddp_info *ddp = sc->iscsi_softc;

	if (!gl->pages[0])
		return;

	for (i = 0; i < gl->nelem; i++) {
		bus_dmamap_unload(ddp->ulp_ddp_tag, gl->dma_sg[i].bus_map);
		bus_dmamap_destroy(ddp->ulp_ddp_tag, gl->dma_sg[i].bus_map);
	}
}

static inline int
ddp_gl_map(struct toedev *tdev,
	     struct cxgbei_ulp2_gather_list *gl)
{
	int i, rc;
	bus_addr_t pa;
	struct cxgbei_ulp2_ddp_info *ddp;
	struct adapter *sc = tdev->tod_softc;

	ddp = (struct cxgbei_ulp2_ddp_info *)sc->iscsi_softc;
	if (ddp == NULL) {
		printf("%s: DDP is NULL tdev:%p sc:%p ddp:%p\n",
			__func__, tdev, sc, ddp);
		return ENOMEM;
	}
	mtx_lock(&ddp->map_lock);
	for (i = 0; i < gl->nelem; i++) {
		rc = bus_dmamap_create(ddp->ulp_ddp_tag, 0,
					&gl->dma_sg[i].bus_map);
		if (rc != 0) {
			printf("%s: unable to map page 0x%p.\n",
					__func__, gl->pages[i]);
			goto unmap;
		}
		rc = bus_dmamap_load(ddp->ulp_ddp_tag, gl->dma_sg[i].bus_map,
				gl->pages[i], PAGE_SIZE, ulp2_dma_map_addr,
				&pa, BUS_DMA_NOWAIT);
		if (rc != 0) {
			printf("%s:unable to load page 0x%p.\n",
					__func__, gl->pages[i]);
			goto unmap;
		}
		gl->dma_sg[i].phys_addr = pa;
	}
	mtx_unlock(&ddp->map_lock);

	return 0;

unmap:
	if (i) {
		unsigned int nelem = gl->nelem;

		gl->nelem = i;
		ddp_gl_unmap(tdev, gl);
		gl->nelem = nelem;
	}
	return ENOMEM;
}

/**
 * cxgbei_ulp2_ddp_make_gl_from_iscsi_sgvec - build ddp page buffer list
 * @xferlen: total buffer length
 * @sgl: page buffer scatter-gather list (struct cxgbei_sgl)
 * @sgcnt: # of page buffers
 * @gfp: allocation mode
 *
 * construct a ddp page buffer list from the scsi scattergather list.
 * coalesce buffers as much as possible, and obtain dma addresses for
 * each page.
 *
 * Return the cxgbei_ulp2_gather_list constructed from the page buffers if the
 * memory can be used for ddp. Return NULL otherwise.
 */
struct cxgbei_ulp2_gather_list *
cxgbei_ulp2_ddp_make_gl_from_iscsi_sgvec
			(unsigned int xferlen, cxgbei_sgl *sgl,
			 unsigned int sgcnt, void *tdev,
			 int gfp)
{
	struct cxgbei_ulp2_gather_list *gl;
	cxgbei_sgl *sg = sgl;
	void *sgpage = (void *)((u64)sg->sg_addr & (~PAGE_MASK));
	unsigned int sglen = sg->sg_length;
	unsigned int sgoffset = (u64)sg->sg_addr & PAGE_MASK;
	unsigned int npages = (xferlen + sgoffset + PAGE_SIZE - 1) >>
			      PAGE_SHIFT;
	int i = 1, j = 0;

	if (xferlen <= DDP_THRESHOLD) {
		CTR2(KTR_CXGBE, "xfer %u < threshold %u, no ddp.\n",
			xferlen, DDP_THRESHOLD);
		return NULL;
	}

	gl = malloc(sizeof(struct cxgbei_ulp2_gather_list) +
		npages * (sizeof(struct dma_segments) + sizeof(void *)),
		M_DEVBUF, M_NOWAIT | M_ZERO);
	if (gl == NULL) {
		printf("%s: gl alloc failed\n", __func__);
		return NULL;
	}

	gl->pages = (void **)&gl->dma_sg[npages];
	gl->length = xferlen;
	gl->offset = sgoffset;
	gl->pages[0] = sgpage;
	CTR6(KTR_CXGBE,
		"%s: xferlen:0x%x len:0x%x off:0x%x sg_addr:%p npages:%d\n",
		__func__, xferlen, gl->length, gl->offset, sg->sg_addr, npages);

	for (i = 1, sg = sg_next(sg); i < sgcnt; i++, sg = sg_next(sg)) {
		void *page = sg->sg_addr;

		if (sgpage == page && sg->sg_offset == sgoffset + sglen)
			sglen += sg->sg_length;
		else {
			/* make sure the sgl is fit for ddp:
			 * each has the same page size, and
			 * all of the middle pages are used completely
			 */
			if ((j && sgoffset) ||
			    ((i != sgcnt - 1) &&
			     ((sglen + sgoffset) & ~CXGBEI_PAGE_MASK))){
				goto error_out;
			}

			j++;
			if (j == gl->nelem || sg->sg_offset) {
				goto error_out;
			}
			gl->pages[j] = page;
			sglen = sg->sg_length;
			sgoffset = sg->sg_offset;
			sgpage = page;
		}
	}
	gl->nelem = ++j;

	if (ddp_gl_map(tdev, gl) < 0)
		goto error_out;

	return gl;

error_out:
	free(gl, M_DEVBUF);
	return NULL;
}

/**
 * cxgbei_ulp2_ddp_release_gl - release a page buffer list
 * @gl: a ddp page buffer list
 * @pdev: pci_dev used for pci_unmap
 * free a ddp page buffer list resulted from cxgbei_ulp2_ddp_make_gl().
 */
void
cxgbei_ulp2_ddp_release_gl(struct cxgbei_ulp2_gather_list *gl, void *tdev)
{
	ddp_gl_unmap(tdev, gl);
	free(gl, M_DEVBUF);
}

/**
 * cxgbei_ulp2_ddp_tag_reserve - set up ddp for a data transfer
 * @ddp: adapter's ddp info
 * @tid: connection id
 * @tformat: tag format
 * @tagp: contains s/w tag initially, will be updated with ddp/hw tag
 * @gl: the page momory list
 * @gfp: allocation mode
 *
 * ddp setup for a given page buffer list and construct the ddp tag.
 * return 0 if success, < 0 otherwise.
 */
int
cxgbei_ulp2_ddp_tag_reserve(struct cxgbei_ulp2_ddp_info *ddp,
				void *isock, unsigned int tid,
				struct cxgbei_ulp2_tag_format *tformat,
				u32 *tagp, struct cxgbei_ulp2_gather_list *gl,
				int gfp, int reply)
{
	struct cxgbei_ulp2_pagepod_hdr hdr;
	unsigned int npods, idx;
	int rv;
	u32 sw_tag = *tagp;
	u32 tag;

	if (page_idx >= DDP_PGIDX_MAX || !ddp || !gl || !gl->nelem ||
		gl->length < DDP_THRESHOLD) {
		CTR3(KTR_CXGBE, "pgidx %u, xfer %u/%u, NO ddp.\n",
			      page_idx, gl->length, DDP_THRESHOLD);
		return EINVAL;
	}

	npods = (gl->nelem + IPPOD_PAGES_MAX - 1) >> IPPOD_PAGES_SHIFT;

	if (ddp->idx_last == ddp->nppods)
		rv = ddp_find_unused_entries(ddp, 0, ddp->nppods,
						npods, &idx, gl);
	else {
		rv = ddp_find_unused_entries(ddp, ddp->idx_last + 1,
					      ddp->nppods, npods, &idx, gl);
		if (rv && ddp->idx_last >= npods) {
			rv = ddp_find_unused_entries(ddp, 0,
				min(ddp->idx_last + npods, ddp->nppods),
						      npods, &idx, gl);
		}
	}
	if (rv) {
		CTR3(KTR_CXGBE, "xferlen %u, gl %u, npods %u NO DDP.\n",
			      gl->length, gl->nelem, npods);
		return rv;
	}

	tag = cxgbei_ulp2_ddp_tag_base(idx, ddp, tformat, sw_tag);
	CTR4(KTR_CXGBE, "%s: sw_tag:0x%x idx:0x%x tag:0x%x\n",
			__func__, sw_tag, idx, tag);

	hdr.rsvd = 0;
	hdr.vld_tid = htonl(F_IPPOD_VALID | V_IPPOD_TID(tid));
	hdr.pgsz_tag_clr = htonl(tag & ddp->rsvd_tag_mask);
	hdr.maxoffset = htonl(gl->length);
	hdr.pgoffset = htonl(gl->offset);

	rv = ddp->ddp_set_map(ddp, isock, &hdr, idx, npods, gl, reply);
	if (rv < 0)
		goto unmark_entries;

	ddp->idx_last = idx;
	*tagp = tag;
	return 0;

unmark_entries:
	ddp_unmark_entries(ddp, idx, npods);
	return rv;
}

/**
 * cxgbei_ulp2_ddp_tag_release - release a ddp tag
 * @ddp: adapter's ddp info
 * @tag: ddp tag
 * ddp cleanup for a given ddp tag and release all the resources held
 */
void
cxgbei_ulp2_ddp_tag_release(struct cxgbei_ulp2_ddp_info *ddp, u32 tag,
				iscsi_socket *isock)
{
	u32 idx;

	if (ddp == NULL) {
		CTR2(KTR_CXGBE, "%s:release ddp tag 0x%x, ddp NULL.\n",
				__func__, tag);
		return;
	}
	 if (isock == NULL)
		return;

	idx = (tag >> IPPOD_IDX_SHIFT) & ddp->idx_mask;
	CTR3(KTR_CXGBE, "tag:0x%x idx:0x%x nppods:0x%x\n",
			tag, idx, ddp->nppods);
	if (idx < ddp->nppods) {
		struct cxgbei_ulp2_gather_list *gl = ddp->gl_map[idx];
		unsigned int npods;

		if (!gl || !gl->nelem) {
			CTR4(KTR_CXGBE,
				"release 0x%x, idx 0x%x, gl 0x%p, %u.\n",
				tag, idx, gl, gl ? gl->nelem : 0);
			return;
		}
		npods = (gl->nelem + IPPOD_PAGES_MAX - 1) >> IPPOD_PAGES_SHIFT;
		CTR3(KTR_CXGBE, "ddp tag 0x%x, release idx 0x%x, npods %u.\n",
			      tag, idx, npods);
		ddp->ddp_clear_map(ddp, gl, tag, idx, npods, isock);
		ddp_unmark_entries(ddp, idx, npods);
		cxgbei_ulp2_ddp_release_gl(gl, ddp->tdev);
	} else
		CTR3(KTR_CXGBE, "ddp tag 0x%x, idx 0x%x > max 0x%x.\n",
			      tag, idx, ddp->nppods);
}

/**
 * cxgbei_ulp2_adapter_ddp_info - read the adapter's ddp information
 * @ddp: adapter's ddp info
 * @tformat: tag format
 * @txsz: max tx pdu payload size, filled in by this func.
 * @rxsz: max rx pdu payload size, filled in by this func.
 * setup the tag format for a given iscsi entity
 */
int
cxgbei_ulp2_adapter_ddp_info(struct cxgbei_ulp2_ddp_info *ddp,
			    struct cxgbei_ulp2_tag_format *tformat,
			    unsigned int *txsz, unsigned int *rxsz)
{
	unsigned char idx_bits;

	if (tformat == NULL)
		return EINVAL;

	if (ddp == NULL)
		return EINVAL;

	idx_bits = 32 - tformat->sw_bits;
	tformat->sw_bits = ddp->idx_bits;
	tformat->rsvd_bits = ddp->idx_bits;
	tformat->rsvd_shift = IPPOD_IDX_SHIFT;
	tformat->rsvd_mask = (1 << tformat->rsvd_bits) - 1;

	CTR4(KTR_CXGBE, "tag format: sw %u, rsvd %u,%u, mask 0x%x.\n",
		      tformat->sw_bits, tformat->rsvd_bits,
		      tformat->rsvd_shift, tformat->rsvd_mask);

	*txsz = min(ULP2_MAX_PDU_PAYLOAD,
			ddp->max_txsz - ISCSI_PDU_NONPAYLOAD_LEN);
	*rxsz = min(ULP2_MAX_PDU_PAYLOAD,
			ddp->max_rxsz - ISCSI_PDU_NONPAYLOAD_LEN);
	CTR4(KTR_CXGBE, "max payload size: %u/%u, %u/%u.\n",
		     *txsz, ddp->max_txsz, *rxsz, ddp->max_rxsz);
	return 0;
}

/**
 * cxgbei_ulp2_ddp_cleanup - release the cxgbX adapter's ddp resource
 * @tdev: t4cdev adapter
 * release all the resource held by the ddp pagepod manager for a given
 * adapter if needed
 */
void
cxgbei_ulp2_ddp_cleanup(struct cxgbei_ulp2_ddp_info **ddp_pp)
{
	int i = 0;
	struct cxgbei_ulp2_ddp_info *ddp = *ddp_pp;

	if (ddp == NULL)
		return;

	CTR2(KTR_CXGBE, "tdev, release ddp 0x%p, ref %d.\n",
			ddp, atomic_load_acq_int(&ddp->refcnt));

	if (ddp && (cxgbei_counter_dec_and_read(&ddp->refcnt) == 0)) {
		*ddp_pp = NULL;
		while (i < ddp->nppods) {
			struct cxgbei_ulp2_gather_list *gl = ddp->gl_map[i];
			if (gl) {
				int npods = (gl->nelem + IPPOD_PAGES_MAX - 1)
						>> IPPOD_PAGES_SHIFT;
				CTR2(KTR_CXGBE,
					"tdev, ddp %d + %d.\n", i, npods);
				free(gl, M_DEVBUF);
				i += npods;
			} else
				i++;
		}
		bus_dmamap_unload(ddp->ulp_ddp_tag, ddp->ulp_ddp_map);
		cxgbei_ulp2_free_big_mem(ddp);
	}
}

/**
 * ddp_init - initialize the cxgb3/4 adapter's ddp resource
 * @tdev_name: device name
 * @tdev: device
 * @ddp: adapter's ddp info
 * @uinfo: adapter's iscsi info
 * initialize the ddp pagepod manager for a given adapter
 */
static void
ddp_init(void *tdev,
	struct cxgbei_ulp2_ddp_info **ddp_pp,
	struct ulp_iscsi_info *uinfo)
{
	struct cxgbei_ulp2_ddp_info *ddp = *ddp_pp;
	unsigned int ppmax, bits;
	int i, rc;

	if (uinfo->ulimit <= uinfo->llimit) {
		printf("%s: tdev, ddp 0x%x >= 0x%x.\n",
			__func__, uinfo->llimit, uinfo->ulimit);
		return;
	}
	if (ddp) {
		atomic_add_acq_int(&ddp->refcnt, 1);
		CTR2(KTR_CXGBE, "tdev, ddp 0x%p already set up, %d.\n",
				ddp, atomic_load_acq_int(&ddp->refcnt));
		return;
	}

	ppmax = (uinfo->ulimit - uinfo->llimit + 1) >> IPPOD_SIZE_SHIFT;
	if (ppmax <= 1024) {
		CTR3(KTR_CXGBE, "tdev, ddp 0x%x ~ 0x%x, nppod %u < 1K.\n",
			uinfo->llimit, uinfo->ulimit, ppmax);
		return;
	}
	bits = (fls(ppmax) - 1) + 1;

	if (bits > IPPOD_IDX_MAX_SIZE)
		bits = IPPOD_IDX_MAX_SIZE;
	ppmax = (1 << (bits - 1)) - 1;

	ddp = cxgbei_ulp2_alloc_big_mem(sizeof(struct cxgbei_ulp2_ddp_info) +
                        ppmax * (sizeof(struct cxgbei_ulp2_gather_list *) +
                        sizeof(unsigned char)));
	if (ddp == NULL) {
		CTR1(KTR_CXGBE, "unable to alloc ddp 0x%d, ddp disabled.\n",
			     ppmax);
		return;
	}
	ddp->colors = (unsigned char *)(ddp + 1);
	ddp->gl_map = (struct cxgbei_ulp2_gather_list **)(ddp->colors +
			ppmax * sizeof(unsigned char));
	*ddp_pp = ddp;

	mtx_init(&ddp->map_lock, "ddp lock", NULL,
			MTX_DEF | MTX_DUPOK| MTX_RECURSE);

	atomic_set_acq_int(&ddp->refcnt, 1);

	/* dma_tag create */
	rc = ulp2_dma_tag_create(ddp);
	if (rc) {
		printf("%s: unable to alloc ddp 0x%d, ddp disabled.\n",
			     __func__, ppmax);
		return;
	}

	ddp->tdev = tdev;
	ddp->max_txsz = min(uinfo->max_txsz, ULP2_MAX_PKT_SIZE);
	ddp->max_rxsz = min(uinfo->max_rxsz, ULP2_MAX_PKT_SIZE);
	ddp->llimit = uinfo->llimit;
	ddp->ulimit = uinfo->ulimit;
	ddp->nppods = ppmax;
	ddp->idx_last = ppmax;
	ddp->idx_bits = bits;
	ddp->idx_mask = (1 << bits) - 1;
	ddp->rsvd_tag_mask = (1 << (bits + IPPOD_IDX_SHIFT)) - 1;

	CTR2(KTR_CXGBE,
		"gl map 0x%p, idx_last %u.\n", ddp->gl_map, ddp->idx_last);
	uinfo->tagmask = ddp->idx_mask << IPPOD_IDX_SHIFT;
	for (i = 0; i < DDP_PGIDX_MAX; i++)
		uinfo->pgsz_factor[i] = ddp_page_order[i];
	uinfo->ulimit = uinfo->llimit + (ppmax << IPPOD_SIZE_SHIFT);

	printf("nppods %u, bits %u, mask 0x%x,0x%x pkt %u/%u,"
			" %u/%u.\n",
			ppmax, ddp->idx_bits, ddp->idx_mask,
			ddp->rsvd_tag_mask, ddp->max_txsz, uinfo->max_txsz,
			ddp->max_rxsz, uinfo->max_rxsz);

	rc = bus_dmamap_create(ddp->ulp_ddp_tag, 0, &ddp->ulp_ddp_map);
	if (rc != 0) {
		printf("%s: bus_dmamap_Create failed\n", __func__);
		return;
	}
}

/**
 * cxgbei_ulp2_ddp_init - initialize ddp functions
 */
void
cxgbei_ulp2_ddp_init(void *tdev,
			struct cxgbei_ulp2_ddp_info **ddp_pp,
			struct ulp_iscsi_info *uinfo)
{
	if (page_idx == DDP_PGIDX_MAX) {
		page_idx = cxgbei_ulp2_ddp_find_page_index(PAGE_SIZE);

		if (page_idx == DDP_PGIDX_MAX) {
			if (cxgbei_ulp2_ddp_adjust_page_table()) {
				CTR1(KTR_CXGBE, "PAGE_SIZE %x, ddp disabled.\n",
						PAGE_SIZE);
				return;
			}
		}
		page_idx = cxgbei_ulp2_ddp_find_page_index(PAGE_SIZE);
	}

	ddp_init(tdev, ddp_pp, uinfo);
}
