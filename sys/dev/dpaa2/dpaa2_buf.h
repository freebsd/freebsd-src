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

#ifndef	_DPAA2_BUF_H
#define	_DPAA2_BUF_H

#include <sys/types.h>
#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/bus.h>

#define DPAA2_RX_BUF_SIZE	(MJUM9BYTES)

struct dpaa2_buf {
	bus_addr_t		 paddr;
	caddr_t			 vaddr;
	bus_dma_tag_t		 dmat;
	bus_dmamap_t		 dmap;
	bus_dma_segment_t	 seg;
	int			 nseg;
	struct mbuf		*m;
	struct dpaa2_buf	*sgt;
	void			*opt;
};

#define DPAA2_BUF_INIT_TAGOPT(__buf, __tag, __opt) do {			\
	KASSERT((__buf) != NULL, ("%s: buf is NULL", __func__));	\
									\
	(__buf)->paddr = 0;						\
	(__buf)->vaddr = NULL;						\
	(__buf)->dmat = (__tag);					\
	(__buf)->dmap = NULL;						\
	(__buf)->seg.ds_addr = 0;					\
	(__buf)->seg.ds_len = 0;					\
	(__buf)->nseg = 0;						\
	(__buf)->m = NULL;						\
	(__buf)->sgt = NULL;						\
	(__buf)->opt = (__opt);						\
} while(0)
#define DPAA2_BUF_INIT(__buf)	DPAA2_BUF_INIT_TAGOPT((__buf), NULL, NULL)

#if defined(INVARIANTS)
/*
 * TXPREP/TXREADY macros allow to verify whether Tx buffer is prepared to be
 * seeded and/or ready to be used for transmission.
 *
 * NOTE: Any modification should be carefully analyzed and justified.
 */
#define DPAA2_BUF_ASSERT_TXPREP(__buf) do {				\
	struct dpaa2_buf *__sgt = (__buf)->sgt;				\
	KASSERT((__sgt) != NULL, ("%s: no S/G table?", __func__));	\
									\
	KASSERT((__buf)->paddr == 0,    ("%s: paddr set?", __func__));	\
	KASSERT((__buf)->vaddr == NULL, ("%s: vaddr set?", __func__));	\
	KASSERT((__buf)->dmat  != NULL, ("%s: no DMA tag?", __func__));	\
	KASSERT((__buf)->dmap  == NULL, ("%s: DMA map set?", __func__)); \
	KASSERT((__buf)->seg.ds_addr == 0, ("%s: already mapped?", __func__)); \
	KASSERT((__buf)->seg.ds_len  == 0, ("%s: already mapped?", __func__)); \
	KASSERT((__buf)->nseg  == 0,    ("%s: nseg > 0?", __func__));	\
	KASSERT((__buf)->m     == NULL, ("%s: mbuf set?", __func__));	\
	KASSERT((__buf)->opt   != NULL, ("%s: no Tx ring?", __func__));	\
									\
	KASSERT((__sgt)->paddr == 0,    ("%s: S/G paddr set?", __func__)); \
	KASSERT((__sgt)->vaddr == NULL, ("%s: S/G vaddr set?", __func__)); \
	KASSERT((__sgt)->dmat  != NULL, ("%s: no S/G DMA tag?", __func__)); \
	KASSERT((__sgt)->dmap  == NULL, ("%s: S/G DMA map set?", __func__)); \
	KASSERT((__sgt)->seg.ds_addr == 0, ("%s: S/G mapped?", __func__)); \
	KASSERT((__sgt)->seg.ds_len  == 0, ("%s: S/G mapped?", __func__)); \
	KASSERT((__sgt)->nseg  == 0,    ("%s: S/G nseg > 0?", __func__)); \
	KASSERT((__sgt)->m     == NULL, ("%s: S/G mbuf set?", __func__)); \
	KASSERT((__sgt)->opt == (__buf),("%s: buf not linked?", __func__)); \
} while(0)
#define DPAA2_BUF_ASSERT_TXREADY(__buf) do {				\
	struct dpaa2_buf *__sgt = (__buf)->sgt;				\
	KASSERT((__sgt) != NULL,        ("%s: no S/G table?", __func__)); \
									\
	KASSERT((__buf)->paddr == 0,    ("%s: paddr set?", __func__));	\
	KASSERT((__buf)->vaddr == NULL, ("%s: vaddr set?", __func__));	\
	KASSERT((__buf)->dmat  != NULL, ("%s: no DMA tag?", __func__));	\
	KASSERT((__buf)->dmap  != NULL, ("%s: no DMA map?", __func__)); \
	KASSERT((__buf)->seg.ds_addr == 0, ("%s: already mapped?", __func__)); \
	KASSERT((__buf)->seg.ds_len  == 0, ("%s: already mapped?", __func__)); \
	KASSERT((__buf)->nseg  == 0,    ("%s: nseg > 0?", __func__));	\
	KASSERT((__buf)->m     == NULL, ("%s: mbuf set?", __func__));	\
	KASSERT((__buf)->opt   != NULL, ("%s: no Tx ring?", __func__));	\
									\
	KASSERT((__sgt)->paddr == 0,    ("%s: S/G paddr set?", __func__)); \
	KASSERT((__sgt)->vaddr != NULL, ("%s: no S/G vaddr?", __func__)); \
	KASSERT((__sgt)->dmat  != NULL, ("%s: no S/G DMA tag?", __func__)); \
	KASSERT((__sgt)->dmap  != NULL, ("%s: no S/G DMA map?", __func__)); \
	KASSERT((__sgt)->seg.ds_addr == 0, ("%s: S/G mapped?", __func__)); \
	KASSERT((__sgt)->seg.ds_len  == 0, ("%s: S/G mapped?", __func__)); \
	KASSERT((__sgt)->nseg  == 0,    ("%s: S/G nseg > 0?", __func__)); \
	KASSERT((__sgt)->m     == NULL, ("%s: S/G mbuf set?", __func__)); \
	KASSERT((__sgt)->opt == (__buf),("%s: buf not linked?", __func__)); \
} while(0)
#else /* !INVARIANTS */
#define DPAA2_BUF_ASSERT_TXPREP(__buf) do {	\
} while(0)
#define DPAA2_BUF_ASSERT_TXREADY(__buf) do {	\
} while(0)
#endif /* INVARIANTS */

#if defined(INVARIANTS)
/*
 * RXPREP/RXREADY macros allow to verify whether Rx buffer is prepared to be
 * seeded and/or ready to be used for reception.
 *
 * NOTE: Any modification should be carefully analyzed and justified.
 */
#define DPAA2_BUF_ASSERT_RXPREP(__buf) do {				\
	KASSERT((__buf)->paddr == 0,    ("%s: paddr set?", __func__));	\
	KASSERT((__buf)->vaddr == NULL, ("%s: vaddr set?", __func__));	\
	KASSERT((__buf)->dmat  != NULL, ("%s: no DMA tag?", __func__));	\
	/* KASSERT((__buf)->dmap  == NULL, ("%s: DMA map set?", __func__)); */ \
	KASSERT((__buf)->seg.ds_addr == 0, ("%s: already mapped?", __func__)); \
	KASSERT((__buf)->seg.ds_len  == 0, ("%s: already mapped?", __func__)); \
	KASSERT((__buf)->nseg  == 0,    ("%s: nseg > 0?", __func__));	\
	KASSERT((__buf)->m     == NULL, ("%s: mbuf set?", __func__));	\
	KASSERT((__buf)->sgt   == NULL, ("%s: S/G table set?", __func__)); \
	KASSERT((__buf)->opt   != NULL, ("%s: no channel?", __func__));	\
} while(0)
#define DPAA2_BUF_ASSERT_RXREADY(__buf) do {				\
	KASSERT((__buf)->paddr != 0,    ("%s: paddr not set?", __func__)); \
	KASSERT((__buf)->vaddr != NULL, ("%s: vaddr not set?", __func__)); \
	KASSERT((__buf)->dmat  != NULL, ("%s: no DMA tag?", __func__));	\
	KASSERT((__buf)->dmap  != NULL, ("%s: no DMA map?", __func__)); \
	KASSERT((__buf)->seg.ds_addr != 0, ("%s: not mapped?", __func__)); \
	KASSERT((__buf)->seg.ds_len  != 0, ("%s: not mapped?", __func__)); \
	KASSERT((__buf)->nseg  == 1,    ("%s: nseg != 1?", __func__));	\
	KASSERT((__buf)->m     != NULL, ("%s: no mbuf?", __func__));	\
	KASSERT((__buf)->sgt   == NULL, ("%s: S/G table set?", __func__)); \
	KASSERT((__buf)->opt   != NULL, ("%s: no channel?", __func__));	\
} while(0)
#else /* !INVARIANTS */
#define DPAA2_BUF_ASSERT_RXPREP(__buf) do {	\
} while(0)
#define DPAA2_BUF_ASSERT_RXREADY(__buf) do {	\
} while(0)
#endif /* INVARIANTS */

int dpaa2_buf_seed_pool(device_t, device_t, void *, uint32_t, int, struct mtx *);
int dpaa2_buf_seed_rxb(device_t, struct dpaa2_buf *, int, struct mtx *);
int dpaa2_buf_seed_txb(device_t, struct dpaa2_buf *);

#endif /* _DPAA2_BUF_H */
