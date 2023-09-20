/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2023 Dmitry Salychev
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/bus.h>

#include "dpaa2_types.h"
#include "dpaa2_buf.h"
#include "dpaa2_bp.h"
#include "dpaa2_channel.h"
#include "dpaa2_swp.h"
#include "dpaa2_swp_if.h"
#include "dpaa2_ni.h"

MALLOC_DEFINE(M_DPAA2_RXB, "dpaa2_rxb", "DPAA2 DMA-mapped buffer (Rx)");

/**
 * @brief Allocate Rx buffers visible to QBMan and release them to the
 * buffer pool.
 */
int
dpaa2_buf_seed_pool(device_t dev, device_t bpdev, void *arg, uint32_t count,
    int size, struct mtx *dma_mtx)
{
	struct dpaa2_ni_softc *sc = device_get_softc(dev);
	struct dpaa2_bp_softc *bpsc = device_get_softc(bpdev);
	struct dpaa2_channel *ch = (struct dpaa2_channel *)arg;
	struct dpaa2_buf *buf;
	const int alloc = DPAA2_ATOMIC_READ(&sc->buf_num);
	const uint16_t bpid = bpsc->attr.bpid;
	bus_addr_t paddr[DPAA2_SWP_BUFS_PER_CMD];
	int error, bufn = 0;

#if defined(INVARIANTS)
	KASSERT(ch->rx_dmat != NULL, ("%s: no DMA tag?", __func__));
	if (dma_mtx != NULL) {
		mtx_assert(dma_mtx, MA_OWNED);
	}
#endif /* INVARIANTS */

#ifdef _notyet_
	/* Limit amount of buffers released to the pool */
	count = (alloc + count > DPAA2_NI_BUFS_MAX)
	    ? DPAA2_NI_BUFS_MAX - alloc : count;
#endif

	/* Release "count" buffers to the pool */
	for (int i = alloc; i < alloc + count; i++) {
		/* Enough buffers were allocated for a single command */
		if (bufn == DPAA2_SWP_BUFS_PER_CMD) {
			error = DPAA2_SWP_RELEASE_BUFS(ch->io_dev, bpid, paddr,
			    bufn);
			if (error) {
				device_printf(sc->dev, "%s: failed to release "
				    "buffers to the pool (1)\n", __func__);
				return (error);
			}
			DPAA2_ATOMIC_ADD(&sc->buf_num, bufn);
			bufn = 0;
		}

		buf = malloc(sizeof(struct dpaa2_buf), M_DPAA2_RXB, M_NOWAIT);
		if (buf == NULL) {
			device_printf(dev, "%s: malloc() failed\n", __func__);
			return (ENOMEM);
		}
		DPAA2_BUF_INIT_TAGOPT(buf, ch->rx_dmat, ch);

		error = dpaa2_buf_seed_rxb(dev, buf, size, dma_mtx);
		if (error != 0) {
			device_printf(dev, "%s: dpaa2_buf_seed_rxb() failed: "
			    "error=%d/n", __func__, error);
			break;
		}
		paddr[bufn] = buf->paddr;
		bufn++;
	}

	/* Release reminder of the buffers to the pool */
	if (bufn > 0) {
		error = DPAA2_SWP_RELEASE_BUFS(ch->io_dev, bpid, paddr, bufn);
		if (error) {
			device_printf(sc->dev, "%s: failed to release "
			    "buffers to the pool (2)\n", __func__);
			return (error);
		}
		DPAA2_ATOMIC_ADD(&sc->buf_num, bufn);
	}

	return (0);
}

/**
 * @brief Prepare Rx buffer to be released to the buffer pool.
 */
int
dpaa2_buf_seed_rxb(device_t dev, struct dpaa2_buf *buf, int size,
    struct mtx *dma_mtx)
{
	struct dpaa2_ni_softc *sc = device_get_softc(dev);
	struct dpaa2_fa *fa;
	bool map_created = false;
	bool mbuf_alloc = false;
	int error;

#if defined(INVARIANTS)
	DPAA2_BUF_ASSERT_RXPREP(buf);
	if (dma_mtx != NULL) {
		mtx_assert(dma_mtx, MA_OWNED);
	}
#endif /* INVARIANTS */	

	if (__predict_false(buf->dmap == NULL)) {
		error = bus_dmamap_create(buf->dmat, 0, &buf->dmap);
		if (error != 0) {
			device_printf(dev, "%s: failed to create DMA map: "
			    "error=%d\n", __func__, error);
			goto fail_map_create;
		}
		map_created = true;
	}

	if (__predict_true(buf->m == NULL)) {
		buf->m = m_getjcl(M_NOWAIT, MT_DATA, M_PKTHDR, size);
		if (__predict_false(buf->m == NULL)) {
			device_printf(dev, "%s: m_getjcl() failed\n", __func__);
			error = ENOMEM;
			goto fail_mbuf_alloc;
		}
		buf->m->m_len = buf->m->m_ext.ext_size;
		buf->m->m_pkthdr.len = buf->m->m_ext.ext_size;
		mbuf_alloc = true;
	}

	error = bus_dmamap_load_mbuf_sg(buf->dmat, buf->dmap, buf->m, &buf->seg,
	    &buf->nseg, BUS_DMA_NOWAIT);
	KASSERT(buf->nseg == 1, ("%s: one segment expected: nseg=%d", __func__,
	    buf->nseg));
	KASSERT(error == 0, ("%s: bus_dmamap_load_mbuf_sg() failed: error=%d",
	    __func__, error));
	if (__predict_false(error != 0 || buf->nseg != 1)) {
		device_printf(sc->dev, "%s: bus_dmamap_load_mbuf_sg() failed: "
		    "error=%d, nsegs=%d\n", __func__, error, buf->nseg);
		goto fail_mbuf_map;
	}
	buf->paddr = buf->seg.ds_addr;
	buf->vaddr = buf->m->m_data;

	/* Populate frame annotation for future use */
	fa = (struct dpaa2_fa *)buf->vaddr;
	fa->magic = DPAA2_MAGIC;
	fa->buf = buf;

	bus_dmamap_sync(buf->dmat, buf->dmap, BUS_DMASYNC_PREREAD);

	DPAA2_BUF_ASSERT_RXREADY(buf);

	return (0);

fail_mbuf_map:
	if (mbuf_alloc) {
		m_freem(buf->m);
		buf->m = NULL;
	}
fail_mbuf_alloc:
	if (map_created) {
		(void)bus_dmamap_destroy(buf->dmat, buf->dmap);
	}
fail_map_create:
	return (error);
}

/**
 * @brief Prepare Tx buffer to be added to the Tx ring.
 */
int
dpaa2_buf_seed_txb(device_t dev, struct dpaa2_buf *buf)
{
	struct dpaa2_buf *sgt = buf->sgt;
	bool map_created = false;
	int error;

	DPAA2_BUF_ASSERT_TXPREP(buf);

	if (buf->dmap == NULL) {
		error = bus_dmamap_create(buf->dmat, 0, &buf->dmap);
		if (error != 0) {
			device_printf(dev, "%s: bus_dmamap_create() failed: "
			    "error=%d\n", __func__, error);
			goto fail_map_create;
		}
		map_created = true;
	}

	if (sgt->vaddr == NULL) {
		error = bus_dmamem_alloc(sgt->dmat, (void **)&sgt->vaddr,
		    BUS_DMA_ZERO | BUS_DMA_COHERENT, &sgt->dmap);
		if (error != 0) {
			device_printf(dev, "%s: bus_dmamem_alloc() failed: "
			    "error=%d\n", __func__, error);
			goto fail_mem_alloc;
		}
	}

	DPAA2_BUF_ASSERT_TXREADY(buf);

	return (0);

fail_mem_alloc:
	if (map_created) {
		(void)bus_dmamap_destroy(buf->dmat, buf->dmap);
	}
fail_map_create:
	return (error);
}
