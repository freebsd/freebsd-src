/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2021-2022 Dmitry Salychev
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

#ifndef	_DPAA2_TYPES_H
#define	_DPAA2_TYPES_H

#include <machine/atomic.h>

#define DPAA2_MAGIC	((uint32_t) 0xD4AA2C0Du)

/**
 * @brief Types of the DPAA2 devices.
 */
enum dpaa2_dev_type {
	DPAA2_DEV_MC = 7500,	/* Management Complex (firmware bus) */
	DPAA2_DEV_RC,		/* Resource Container (firmware bus) */
	DPAA2_DEV_IO,		/* I/O object (to work with QBMan portal) */
	DPAA2_DEV_NI,		/* Network Interface */
	DPAA2_DEV_MCP,		/* MC portal */
	DPAA2_DEV_BP,		/* Buffer Pool */
	DPAA2_DEV_CON,		/* Concentrator */
	DPAA2_DEV_MAC,		/* MAC object */
	DPAA2_DEV_MUX,		/* MUX (Datacenter bridge) object */
	DPAA2_DEV_SW,		/* Ethernet Switch */

	DPAA2_DEV_NOTYPE	/* Shouldn't be assigned to any DPAA2 device. */
};

/**
 * @brief Types of the DPAA2 buffers.
 */
enum dpaa2_buf_type {
	DPAA2_BUF_RX = 75,	/* Rx buffer */
	DPAA2_BUF_TX,		/* Tx buffer */
	DPAA2_BUF_STORE		/* Channel storage, key configuration */
};

/**
 * @brief DMA-mapped buffer (for Rx/Tx buffers, channel storage, etc.).
 */
struct dpaa2_buf {
	enum dpaa2_buf_type		 type;
	union {
		struct {
			bus_dma_tag_t	 dmat; /* DMA tag for this buffer */
			bus_dmamap_t	 dmap;
			bus_addr_t	 paddr;
			void		*vaddr;

			struct mbuf	*m; /* associated mbuf */
		} rx;
		struct {
			bus_dma_tag_t	 dmat; /* DMA tag for this buffer */
			bus_dmamap_t	 dmap;
			bus_addr_t	 paddr;
			void		*vaddr;

			struct mbuf	*m; /* associated mbuf */
			uint64_t	 idx;

			/* for scatter/gather table */
			bus_dma_tag_t	 sgt_dmat;
			bus_dmamap_t	 sgt_dmap;
			bus_addr_t	 sgt_paddr;
			void		*sgt_vaddr;
		} tx;
		struct {
			bus_dma_tag_t	 dmat; /* DMA tag for this buffer */
			bus_dmamap_t	 dmap;
			bus_addr_t	 paddr;
			void		*vaddr;
		} store;
	};
};

struct dpaa2_atomic {
	volatile int counter;
};

/* Handy wrappers over atomic operations. */
#define DPAA2_ATOMIC_XCHG(a, val) \
	(atomic_swap_int(&(a)->counter, (val)))
#define DPAA2_ATOMIC_READ(a) \
	(atomic_load_acq_int(&(a)->counter))
#define DPAA2_ATOMIC_ADD(a, val) \
	(atomic_add_acq_int(&(a)->counter, (val)))

/* Convert DPAA2 type to/from string. */
const char		*dpaa2_ttos(enum dpaa2_dev_type type);
enum dpaa2_dev_type	 dpaa2_stot(const char *str);

#endif /* _DPAA2_TYPES_H */
