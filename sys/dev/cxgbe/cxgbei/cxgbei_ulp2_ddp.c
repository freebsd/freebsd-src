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

#ifdef TCP_OFFLOAD
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

#include <dev/iscsi/icl.h>
#include <dev/iscsi/iscsi_proto.h>

#include "common/common.h"
#include "common/t4_msg.h"
#include "common/t4_regs.h"     /* for PCIE_MEM_ACCESS */
#include "tom/t4_tom.h"
#include "cxgbei.h"
#include "cxgbei_ulp2_ddp.h"

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


static inline int
ddp_find_unused_entries(struct cxgbei_data *ci, u_int start, u_int max,
    u_int count, u_int *idx, struct cxgbei_ulp2_gather_list *gl)
{
	unsigned int i, j, k;

	/* not enough entries */
	if (max - start < count)
		return (EBUSY);

	max -= count;
	mtx_lock(&ci->map_lock);
	for (i = start; i < max;) {
		for (j = 0, k = i; j < count; j++, k++) {
			if (ci->gl_map[k])
				break;
		}
		if (j == count) {
			for (j = 0, k = i; j < count; j++, k++)
				ci->gl_map[k] = gl;
			mtx_unlock(&ci->map_lock);
			*idx = i;
			return (0);
		}
		i += j + 1;
	}
	mtx_unlock(&ci->map_lock);
	return (EBUSY);
}

static inline void
ddp_unmark_entries(struct cxgbei_data *ci, u_int start, u_int count)
{

	mtx_lock(&ci->map_lock);
	memset(&ci->gl_map[start], 0,
	       count * sizeof(struct cxgbei_ulp2_gather_list *));
	mtx_unlock(&ci->map_lock);
}

static inline void
ddp_gl_unmap(struct cxgbei_data *ci, struct cxgbei_ulp2_gather_list *gl)
{
	int i;

	if (!gl->pages[0])
		return;

	for (i = 0; i < gl->nelem; i++) {
		bus_dmamap_unload(ci->ulp_ddp_tag, gl->dma_sg[i].bus_map);
		bus_dmamap_destroy(ci->ulp_ddp_tag, gl->dma_sg[i].bus_map);
	}
}

static inline int
ddp_gl_map(struct cxgbei_data *ci, struct cxgbei_ulp2_gather_list *gl)
{
	int i, rc;
	bus_addr_t pa;

	MPASS(ci != NULL);

	mtx_lock(&ci->map_lock);
	for (i = 0; i < gl->nelem; i++) {
		rc = bus_dmamap_create(ci->ulp_ddp_tag, 0,
		    &gl->dma_sg[i].bus_map);
		if (rc != 0)
			goto unmap;
		rc = bus_dmamap_load(ci->ulp_ddp_tag, gl->dma_sg[i].bus_map,
				gl->pages[i], PAGE_SIZE, ulp2_dma_map_addr,
				&pa, BUS_DMA_NOWAIT);
		if (rc != 0)
			goto unmap;
		gl->dma_sg[i].phys_addr = pa;
	}
	mtx_unlock(&ci->map_lock);

	return (0);

unmap:
	if (i) {
		u_int nelem = gl->nelem;

		gl->nelem = i;
		ddp_gl_unmap(ci, gl);
		gl->nelem = nelem;
	}
	return (ENOMEM);
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
cxgbei_ulp2_ddp_make_gl_from_iscsi_sgvec(u_int xferlen, struct cxgbei_sgl *sgl,
    u_int sgcnt, struct cxgbei_data *ci, int gfp)
{
	struct cxgbei_ulp2_gather_list *gl;
	struct cxgbei_sgl *sg = sgl;
	void *sgpage = (void *)((u64)sg->sg_addr & (~PAGE_MASK));
	unsigned int sglen = sg->sg_length;
	unsigned int sgoffset = (u64)sg->sg_addr & PAGE_MASK;
	unsigned int npages = (xferlen + sgoffset + PAGE_SIZE - 1) >>
			      PAGE_SHIFT;
	int i = 1, j = 0;

	if (xferlen <= DDP_THRESHOLD) {
		CTR2(KTR_CXGBE, "xfer %u < threshold %u, no ddp.",
			xferlen, DDP_THRESHOLD);
		return NULL;
	}

	gl = malloc(sizeof(struct cxgbei_ulp2_gather_list) +
		npages * (sizeof(struct dma_segments) + sizeof(void *)),
		M_DEVBUF, M_NOWAIT | M_ZERO);
	if (gl == NULL)
		return (NULL);

	gl->pages = (void **)&gl->dma_sg[npages];
	gl->length = xferlen;
	gl->offset = sgoffset;
	gl->pages[0] = sgpage;
	CTR6(KTR_CXGBE,
		"%s: xferlen:0x%x len:0x%x off:0x%x sg_addr:%p npages:%d",
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

	if (ddp_gl_map(ci, gl) < 0)
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
cxgbei_ulp2_ddp_release_gl(struct cxgbei_data *ci,
    struct cxgbei_ulp2_gather_list *gl)
{

	ddp_gl_unmap(ci, gl);
	free(gl, M_DEVBUF);
}

/**
 * cxgbei_ulp2_ddp_tag_reserve - set up ddp for a data transfer
 * @ci: adapter's ddp info
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
cxgbei_ulp2_ddp_tag_reserve(struct cxgbei_data *ci, void *icc, u_int tid,
    struct cxgbei_ulp2_tag_format *tformat, u32 *tagp,
    struct cxgbei_ulp2_gather_list *gl, int gfp, int reply)
{
	struct cxgbei_ulp2_pagepod_hdr hdr;
	u_int npods, idx;
	int rc;
	u32 sw_tag = *tagp;
	u32 tag;

	MPASS(ci != NULL);

	if (!gl || !gl->nelem || gl->length < DDP_THRESHOLD)
		return (EINVAL);

	npods = (gl->nelem + IPPOD_PAGES_MAX - 1) >> IPPOD_PAGES_SHIFT;

	if (ci->idx_last == ci->nppods)
		rc = ddp_find_unused_entries(ci, 0, ci->nppods, npods, &idx,
		    gl);
	else {
		rc = ddp_find_unused_entries(ci, ci->idx_last + 1,
					      ci->nppods, npods, &idx, gl);
		if (rc && ci->idx_last >= npods) {
			rc = ddp_find_unused_entries(ci, 0,
				min(ci->idx_last + npods, ci->nppods),
						      npods, &idx, gl);
		}
	}
	if (rc) {
		CTR3(KTR_CXGBE, "xferlen %u, gl %u, npods %u NO DDP.",
			      gl->length, gl->nelem, npods);
		return (rc);
	}

	tag = cxgbei_ulp2_ddp_tag_base(idx, ci->colors, tformat, sw_tag);
	CTR4(KTR_CXGBE, "%s: sw_tag:0x%x idx:0x%x tag:0x%x",
			__func__, sw_tag, idx, tag);

	hdr.rsvd = 0;
	hdr.vld_tid = htonl(F_IPPOD_VALID | V_IPPOD_TID(tid));
	hdr.pgsz_tag_clr = htonl(tag & ci->rsvd_tag_mask);
	hdr.maxoffset = htonl(gl->length);
	hdr.pgoffset = htonl(gl->offset);

	rc = t4_ddp_set_map(ci, icc, &hdr, idx, npods, gl, reply);
	if (rc < 0)
		goto unmark_entries;

	ci->idx_last = idx;
	*tagp = tag;
	return (0);

unmark_entries:
	ddp_unmark_entries(ci, idx, npods);
	return (rc);
}

/**
 * cxgbei_ulp2_ddp_tag_release - release a ddp tag
 * @ci: adapter's ddp info
 * @tag: ddp tag
 * ddp cleanup for a given ddp tag and release all the resources held
 */
void
cxgbei_ulp2_ddp_tag_release(struct cxgbei_data *ci, uint32_t tag,
    struct icl_cxgbei_conn *icc)
{
	uint32_t idx;

	MPASS(ci != NULL);
	MPASS(icc != NULL);

	idx = (tag >> IPPOD_IDX_SHIFT) & ci->idx_mask;
	CTR3(KTR_CXGBE, "tag:0x%x idx:0x%x nppods:0x%x",
			tag, idx, ci->nppods);
	if (idx < ci->nppods) {
		struct cxgbei_ulp2_gather_list *gl = ci->gl_map[idx];
		unsigned int npods;

		if (!gl || !gl->nelem) {
			CTR4(KTR_CXGBE,
				"release 0x%x, idx 0x%x, gl 0x%p, %u.",
				tag, idx, gl, gl ? gl->nelem : 0);
			return;
		}
		npods = (gl->nelem + IPPOD_PAGES_MAX - 1) >> IPPOD_PAGES_SHIFT;
		CTR3(KTR_CXGBE, "ddp tag 0x%x, release idx 0x%x, npods %u.",
			      tag, idx, npods);
		t4_ddp_clear_map(ci, gl, tag, idx, npods, icc);
		ddp_unmark_entries(ci, idx, npods);
		cxgbei_ulp2_ddp_release_gl(ci, gl);
	} else
		CTR3(KTR_CXGBE, "ddp tag 0x%x, idx 0x%x > max 0x%x.",
			      tag, idx, ci->nppods);
}

/**
 * cxgbei_ddp_cleanup - release the adapter's ddp resources
 */
void
cxgbei_ddp_cleanup(struct cxgbei_data *ci)
{
	int i = 0;

	while (i < ci->nppods) {
		struct cxgbei_ulp2_gather_list *gl = ci->gl_map[i];
		if (gl) {
			int npods = (gl->nelem + IPPOD_PAGES_MAX - 1)
					>> IPPOD_PAGES_SHIFT;
			free(gl, M_DEVBUF);
			i += npods;
		} else
			i++;
	}
	free(ci->colors, M_CXGBE);
	free(ci->gl_map, M_CXGBE);
}
#endif
