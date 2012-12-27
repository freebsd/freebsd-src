/*-
 * Copyright (c) 2012 EMC Corp.
 * All rights reserved.
 *
 * Copyright (c) 1997, 1998 Justin T. Gibbs.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_bus.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/uio.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/pmap.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>

#include <machine/bus.h>

/*
 * Map the buffer buf into bus space using the dmamap map.
 */
int
bus_dmamap_load(bus_dma_tag_t dmat, bus_dmamap_t map, void *buf,
    bus_size_t buflen, bus_dmamap_callback_t *callback,
    void *callback_arg, int flags)
{
	bus_dma_segment_t *segs;
	int error;
	int nsegs;

	if ((flags & BUS_DMA_NOWAIT) == 0)
		_bus_dmamap_mayblock(dmat, map, callback, callback_arg);

	nsegs = -1;
	error = _bus_dmamap_load_buffer(dmat, map, buf, buflen, kernel_pmap,
	    flags, NULL, &nsegs);
	nsegs++;

	CTR5(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d nsegs %d",
	    __func__, dmat, flags, error, nsegs + 1);

	if (error == EINPROGRESS)
		return (error);

	segs = _bus_dmamap_complete(dmat, map, NULL, nsegs, error);
	if (error)
		(*callback)(callback_arg, segs, 0, error);
	else
		(*callback)(callback_arg, segs, nsegs, 0);

	/*
	 * Return ENOMEM to the caller so that it can pass it up the stack.
	 * This error only happens when NOWAIT is set, so deferal is disabled.
	 */
	if (error == ENOMEM)
		return (error);

	return (0);
}

/*
 * Like _bus_dmamap_load(), but for mbufs.
 */
static __inline int
_bus_dmamap_load_mbuf_sg(bus_dma_tag_t dmat, bus_dmamap_t map,
    struct mbuf *m0, bus_dma_segment_t *segs, int *nsegs, int flags)
{
	struct mbuf *m;
	int error;

	M_ASSERTPKTHDR(m0);

	flags |= BUS_DMA_NOWAIT;
	*nsegs = -1;
	error = 0;

	for (m = m0; m != NULL && error == 0; m = m->m_next) {
		if (m->m_len > 0) {
			error = _bus_dmamap_load_buffer(dmat, map, m->m_data,
			    m->m_len, kernel_pmap, flags, segs, nsegs);
		}
	}

	++*nsegs;
	CTR5(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d nsegs %d",
	    __func__, dmat, flags, error, *nsegs);
	return (error);
}

int
bus_dmamap_load_mbuf(bus_dma_tag_t dmat, bus_dmamap_t map, struct mbuf *m0,
    bus_dmamap_callback2_t *callback, void *callback_arg, int flags)
{
	bus_dma_segment_t *segs;
	int nsegs, error;

	error = _bus_dmamap_load_mbuf_sg(dmat, map, m0, NULL, &nsegs, flags);

	segs = _bus_dmamap_complete(dmat, map, NULL, nsegs, error);
	if (error)
		(*callback)(callback_arg, segs, 0, 0, error);
	else
		(*callback)(callback_arg, segs, nsegs, m0->m_pkthdr.len, error);

	CTR5(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d nsegs %d",
	    __func__, dmat, flags, error, nsegs);
	return (error);
}

int
bus_dmamap_load_mbuf_sg(bus_dma_tag_t dmat, bus_dmamap_t map, struct mbuf *m0,
    bus_dma_segment_t *segs, int *nsegs, int flags)
{
	int error;

	error = _bus_dmamap_load_mbuf_sg(dmat, map, m0, segs, nsegs, flags);
	_bus_dmamap_complete(dmat, map, segs, *nsegs, error);
	return (error);
}

/*
 * Like _bus_dmamap_load(), but for uios.
 */
int
bus_dmamap_load_uio(bus_dma_tag_t dmat, bus_dmamap_t map, struct uio *uio,
    bus_dmamap_callback2_t *callback, void *callback_arg, int flags)
{
	bus_dma_segment_t *segs;
	int nsegs, error, i;
	bus_size_t resid;
	bus_size_t minlen;
	struct iovec *iov;
	caddr_t addr;
	pmap_t pmap;

	flags |= BUS_DMA_NOWAIT;
	resid = uio->uio_resid;
	iov = uio->uio_iov;

	if (uio->uio_segflg == UIO_USERSPACE) {
		KASSERT(uio->uio_td != NULL,
			("bus_dmamap_load_uio: USERSPACE but no proc"));
		pmap = vmspace_pmap(uio->uio_td->td_proc->p_vmspace);
	} else
		pmap = kernel_pmap;

	nsegs = -1;
	error = 0;
	for (i = 0; i < uio->uio_iovcnt && resid != 0 && !error; i++) {
		/*
		 * Now at the first iovec to load.  Load each iovec
		 * until we have exhausted the residual count.
		 */

		addr = (caddr_t) iov[i].iov_base;
		minlen = resid < iov[i].iov_len ? resid : iov[i].iov_len;
		if (minlen > 0) {
			error = _bus_dmamap_load_buffer(dmat, map, addr,
			    minlen, pmap, flags, NULL, &nsegs);
			resid -= minlen;
		}
	}

	nsegs++;
	segs = _bus_dmamap_complete(dmat, map, NULL, nsegs, error);
	if (error)
		(*callback)(callback_arg, segs, 0, 0, error);
	else
		(*callback)(callback_arg, segs, nsegs, uio->uio_resid, error);

	CTR5(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d nsegs %d",
	    __func__, dmat, dmat, error, nsegs + 1);
	return (error);
}

int
bus_dmamap_load_ccb(bus_dma_tag_t dmat, bus_dmamap_t map, union ccb *ccb,
		    bus_dmamap_callback_t *callback, void *callback_arg,
		    int flags)
{
	struct ccb_ataio *ataio;
	struct ccb_scsiio *csio;
	struct ccb_hdr *ccb_h;
	void *data_ptr;
	uint32_t dxfer_len;
	uint16_t sglist_cnt;

	ccb_h = &ccb->ccb_h;
	if ((ccb_h->flags & CAM_DIR_MASK) == CAM_DIR_NONE) {
		callback(callback_arg, NULL, 0, 0);
		return (0);
	}

	switch (ccb_h->func_code) {
	case XPT_SCSI_IO:
		csio = &ccb->csio;
		data_ptr = csio->data_ptr;
		dxfer_len = csio->dxfer_len;
		sglist_cnt = csio->sglist_cnt;
		break;
	case XPT_ATA_IO:
		ataio = &ccb->ataio;
		data_ptr = ataio->data_ptr;
		dxfer_len = ataio->dxfer_len;
		sglist_cnt = 0;
		break;
	default:
		panic("bus_dmamap_load_ccb: Unsupported func code %d",
		    ccb_h->func_code);
	}

	switch ((ccb_h->flags & CAM_DATA_MASK)) {
	case CAM_DATA_VADDR:
		return bus_dmamap_load(dmat,
				       map,
				       data_ptr,
				       dxfer_len,
				       callback,
				       callback_arg,
				       /*flags*/0);
	case CAM_DATA_PADDR: {
		bus_dma_segment_t seg;

		seg.ds_addr = (bus_addr_t)(vm_offset_t)data_ptr;
		seg.ds_len = dxfer_len;
		callback(callback_arg, &seg, 1, 0);
		break;
	}
	case CAM_DATA_SG: {
		bus_dma_segment_t *segs;
		int nsegs;
		int error;
		int i;

		flags |= BUS_DMA_NOWAIT;
		segs = (bus_dma_segment_t *)data_ptr;
		nsegs = -1;
		error = 0;
		for (i = 0; i < sglist_cnt && error == 0; i++) {
			error = _bus_dmamap_load_buffer(dmat, map,
			    (void *)segs[i].ds_addr, segs[i].ds_len,
			    kernel_pmap, flags, NULL, &nsegs);
		}
		nsegs++;
		segs = _bus_dmamap_complete(dmat, map, NULL, nsegs, error);
		if (error)
			(*callback)(callback_arg, segs, 0, error);
		else
			(*callback)(callback_arg, segs, nsegs, error);

		if (error == ENOMEM)
			return (error);
		break;
	}
	case CAM_DATA_SG_PADDR: {
		bus_dma_segment_t *segs;
		/* Just use the segments provided */
		segs = (bus_dma_segment_t *)data_ptr;
		callback(callback_arg, segs, sglist_cnt, 0);
		break;
	}
	case CAM_DATA_BIO:
	default:
		panic("bus_dmamap_load_ccb: flags 0x%X unimplemented",
		    ccb_h->flags);
	}
	return (0);
}
