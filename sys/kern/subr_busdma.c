/*-
 * Copyright (c) 2012 EMC Corp.
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
#include <sys/uio.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>

#include <machine/bus.h>

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
		struct bus_dma_segment seg;

		seg.ds_addr = (bus_addr_t)(vm_offset_t)data_ptr;
		seg.ds_len = dxfer_len;
		callback(callback_arg, &seg, 1, 0);
		break;
	}
	case CAM_DATA_SG: {
#if 0
		struct uio sguio;
		KASSERT((sizeof (sguio.uio_iov) == sizeof (data_ptr) &&
		    sizeof (sguio.uio_iovcnt) >= sizeof (sglist_cnt) &&
		    sizeof (sguio.uio_resid) >= sizeof (dxfer_len)),
		    ("uio won't fit csio data"));
		sguio.uio_iov = (struct iovec *)data_ptr;
		sguio.uio_iovcnt = csio->sglist_cnt;
		sguio.uio_resid = csio->dxfer_len;
		sguio.uio_segflg = UIO_SYSSPACE;
		return bus_dmamap_load_uio(dmat, map, &sguio, callback,
		    callback_arg, 0);
#else
		panic("bus_dmamap_load_ccb: flags 0x%X unimplemented",
		    ccb_h->flags);
#endif
	}
	case CAM_DATA_SG_PADDR: {
		struct bus_dma_segment *segs;
		/* Just use the segments provided */
		segs = (struct bus_dma_segment *)data_ptr;
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
