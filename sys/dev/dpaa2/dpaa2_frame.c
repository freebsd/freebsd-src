/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Dmitry Salychev
 * Copyright (c) 2026 Bjoern A. Zeeb
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

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/endian.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/vmparam.h>

#include "dpaa2_types.h"
#include "dpaa2_frame.h"
#include "dpaa2_buf.h"
#include "dpaa2_swp.h"

/**
 * @brief Build a DPAA2 frame descriptor.
 */
int
dpaa2_fd_build(device_t dev, const uint16_t tx_data_off, struct dpaa2_buf *buf,
    bus_dma_segment_t *segs, const int nsegs, struct dpaa2_fd *fd)
{
	struct dpaa2_buf *sgt = buf->sgt;
	struct dpaa2_sg_entry *sge;
	struct dpaa2_swa *swa;
	int i, error;

	if (buf == NULL || segs == NULL || nsegs == 0 || fd == NULL)
		return (EINVAL);

	KASSERT(nsegs <= DPAA2_TX_SEGLIMIT, ("%s: too many segments", __func__));
	KASSERT(buf->opt != NULL, ("%s: no Tx ring?", __func__));
	KASSERT(sgt != NULL, ("%s: no S/G table?", __func__));
	KASSERT(sgt->vaddr != NULL, ("%s: no S/G vaddr?", __func__));

	memset(fd, 0, sizeof(*fd));

	/* Populate and map S/G table */
	if (__predict_true(nsegs <= DPAA2_TX_SEGLIMIT)) {
		sge = (struct dpaa2_sg_entry *)sgt->vaddr + tx_data_off;
		for (i = 0; i < nsegs; i++) {
			sge[i].addr = (uint64_t)segs[i].ds_addr;
			sge[i].len = (uint32_t)segs[i].ds_len;
			sge[i].offset_fmt = 0u;
		}
		sge[i-1].offset_fmt |= 0x8000u; /* set final entry flag */

		KASSERT(sgt->paddr == 0, ("%s: paddr(%#jx) != 0", __func__,
		    sgt->paddr));

		error = bus_dmamap_load(sgt->dmat, sgt->dmap, sgt->vaddr,
		    DPAA2_TX_SGT_SZ, dpaa2_dmamap_oneseg_cb, &sgt->paddr,
		    BUS_DMA_NOWAIT);
		if (__predict_false(error != 0)) {
			device_printf(dev, "%s: bus_dmamap_load() failed: "
			    "error=%d\n", __func__, error);
			return (error);
		}

		buf->paddr = sgt->paddr;
		buf->vaddr = sgt->vaddr;
	} else {
		return (EINVAL);
	}

	swa = (struct dpaa2_swa *)sgt->vaddr;
	swa->magic = DPAA2_MAGIC;
	swa->buf = buf;

	fd->addr = buf->paddr;
	fd->data_length = (uint32_t)buf->m->m_pkthdr.len;
	fd->bpid_ivp_bmt = 0;
	fd->offset_fmt_sl = 0x2000u | tx_data_off;
	fd->ctrl = (0x4u & DPAA2_FD_PTAC_MASK) << DPAA2_FD_PTAC_SHIFT;

	return (0);
}

int
dpaa2_fd_err(struct dpaa2_fd *fd)
{
	return ((fd->ctrl >> DPAA2_FD_ERR_SHIFT) & DPAA2_FD_ERR_MASK);
}

uint32_t
dpaa2_fd_data_len(struct dpaa2_fd *fd)
{
	if (dpaa2_fd_short_len(fd)) {
		return (fd->data_length & DPAA2_FD_LEN_MASK);
	}
	return (fd->data_length);
}

int
dpaa2_fd_format(struct dpaa2_fd *fd)
{
	return ((enum dpaa2_fd_format)((fd->offset_fmt_sl >>
	    DPAA2_FD_FMT_SHIFT) & DPAA2_FD_FMT_MASK));
}

bool
dpaa2_fd_short_len(struct dpaa2_fd *fd)
{
	return (((fd->offset_fmt_sl >> DPAA2_FD_SL_SHIFT)
	    & DPAA2_FD_SL_MASK) == 1);
}

int
dpaa2_fd_offset(struct dpaa2_fd *fd)
{
	return (fd->offset_fmt_sl & DPAA2_FD_OFFSET_MASK);
}

uint32_t
dpaa2_fd_get_frc(struct dpaa2_fd *fd)
{
	/* TODO: Convert endiannes in the other functions as well. */
	return (le32toh(fd->frame_ctx));
}

#ifdef _not_yet_
void
dpaa2_fd_set_frc(struct dpaa2_fd *fd, uint32_t frc)
{
	/* TODO: Convert endiannes in the other functions as well. */
	fd->frame_ctx = htole32(frc);
}
#endif

int
dpaa2_fa_get_swa(struct dpaa2_fd *fd, struct dpaa2_swa **swa)
{
	if (__predict_false(fd == NULL || swa == NULL))
		return (EINVAL);

	if (((fd->ctrl >> DPAA2_FD_PTAC_SHIFT) & DPAA2_FD_PTAC_PTA_MASK) == 0u) {
		*swa = NULL;
		return (ENOENT);
	}

	*swa = (struct dpaa2_swa *)PHYS_TO_DMAP((bus_addr_t)fd->addr);

	return (0);
}

int
dpaa2_fa_get_hwa(struct dpaa2_fd *fd, struct dpaa2_hwa **hwa)
{
	uint8_t *buf;
	uint32_t hwo; /* HW annotation offset */

	if (__predict_false(fd == NULL || hwa == NULL))
		return (EINVAL);

	/*
	 * As soon as the ASAL is in the 64-byte units, we don't need to
	 * calculate the exact length, but make sure that it isn't 0.
	 */
	if (((fd->ctrl >> DPAA2_FD_ASAL_SHIFT) & DPAA2_FD_ASAL_MASK) == 0u) {
		*hwa = NULL;
		return (ENOENT);
	}

	buf = (uint8_t *)PHYS_TO_DMAP((bus_addr_t)fd->addr);
	hwo = ((fd->ctrl >> DPAA2_FD_PTAC_SHIFT) & DPAA2_FD_PTAC_PTA_MASK) > 0u
	    ? DPAA2_FA_SWA_SIZE : 0u;
	*hwa = (struct dpaa2_hwa *)(buf + hwo);

	return (0);
}

int
dpaa2_fa_get_fas(struct dpaa2_fd *fd, struct dpaa2_hwa_fas *fas)
{
	struct dpaa2_hwa *hwa;
	struct dpaa2_hwa_fas *fasp;
	int rc;

	if (__predict_false(fd == NULL || fas == NULL))
		return (EINVAL);

	rc = dpaa2_fa_get_hwa(fd, &hwa);
	if (__predict_false(rc != 0))
		return (rc);

	fasp = (struct dpaa2_hwa_fas *)&hwa->fas;
	*fas = *fasp;

	return (rc);
}

#ifdef _not_yet_
int
dpaa2_fa_set_fas(struct dpaa2_fd *fd, struct dpaa2_hwa_fas *fas)
{
	struct dpaa2_hwa *hwa;
	uint64_t *valp;
	int rc;

	if (__predict_false(fd == NULL || fas == NULL))
		return (EINVAL);

	rc = dpaa2_fa_get_hwa(fd, &hwa);
	if (__predict_false(rc != 0))
		return (rc);

	valp = (uint64_t *)fas;
	hwa->fas = *valp;

	return (rc);
}
#endif
