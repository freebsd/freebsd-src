/*
 * Copyright (c) 1999 Matthew R. Green
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: NetBSD: iommuvar.h,v 1.9 2001/07/20 00:07:13 eeh Exp
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_IOMMUVAR_H_
#define _MACHINE_IOMMUVAR_H_

#define	IO_PAGE_SIZE		PAGE_SIZE_8K
#define	IO_PAGE_MASK		PAGE_MASK_8K
#define	IO_PAGE_SHIFT		PAGE_SHIFT_8K
#define	round_io_page(x)	round_page(x)
#define	trunc_io_page(x)	trunc_page(x)

/*
 * Per-IOMMU state. The parenthesized comments indicate the locking strategy:
 *	i - protected by iommu_mtx.
 *	r - read-only after initialization.
 *	* - comment refers to pointer target / target hardware registers
 *	    (for bus_addr_t).
 * iommu_map_lruq is also locked by iommu_mtx. Elements of iommu_tsb may only
 * be accessed from functions operating on the map owning the corresponding
 * resource, so the locking the user is required to do to protect the map is
 * sufficient. As soon as the TSBs are divorced, these will be moved into struct
 * iommu_state, and each state struct will get its own lock.
 * iommu_dvma_rman needs to be moved there too, but has its own internal lock.
 */
struct iommu_state {
	int			is_tsbsize;	/* (r) 0 = 8K, ... */
	u_int64_t		is_dvmabase;	/* (r) */
	int64_t			is_cr;		/* (r) Control reg value */

	vm_paddr_t		is_flushpa[2];	/* (r) */
	volatile int64_t	*is_flushva[2];	/* (r, *i) */
	/*
	 * (i)
	 * When a flush is completed, 64 bytes will be stored at the given
	 * location, the first double word being 1, to indicate completion.
	 * The lower 6 address bits are ignored, so the addresses need to be
	 * suitably aligned; over-allocate a large enough margin to be able
	 * to adjust it.
	 * Two such buffers are needed.
	 */
	volatile char		is_flush[STRBUF_FLUSHSYNC_NBYTES * 3 - 1];

	/* copies of our parents state, to allow us to be self contained */
	bus_space_tag_t		is_bustag;	/* (r) Our bus tag */
	bus_space_handle_t	is_bushandle;	/* (r) */
	bus_addr_t		is_iommu;	/* (r, *i) IOMMU registers */
	bus_addr_t		is_sb[2];	/* (r, *i) Streaming buffer */
	/* Tag diagnostics access */
	bus_addr_t		is_dtag;	/* (r, *r) */
	/* Data RAM diagnostic access */
	bus_addr_t		is_ddram;	/* (r, *r) */
	/* LRU queue diag. access */
	bus_addr_t		is_dqueue;	/* (r, *r) */
	/* Virtual address diagnostics register */
	bus_addr_t		is_dva;		/* (r, *r) */
	/* Tag compare diagnostics access */
	bus_addr_t		is_dtcmp;	/* (r, *r) */

	STAILQ_ENTRY(iommu_state)	is_link;	/* (r) */
};

/* interfaces for PCI/SBUS code */
void iommu_init(char *, struct iommu_state *, int, u_int32_t, int);
void iommu_reset(struct iommu_state *);
void iommu_decode_fault(struct iommu_state *, vm_offset_t);

extern struct bus_dma_methods iommu_dma_methods;

#endif /* !_MACHINE_IOMMUVAR_H_ */
