/*-
 * Copyright (c) 1996, 1997, 1998, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1997-1999 Eduardo E. Horvath. All rights reserved.
 * Copyright (c) 1996 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * 	from: NetBSD: bus.h,v 1.58 2008/04/28 20:23:36 martin Exp
 *	and
 *	from: FreeBSD: src/sys/alpha/include/bus.h,v 1.9 2001/01/09
 *
 * $FreeBSD$
 */

#ifndef _SPARC64_BUS_DMA_H
#define _SPARC64_BUS_DMA_H

#include <sys/bus_dma.h>

/* DMA support */

/*
 * Method table for a bus_dma_tag.
 */
struct bus_dma_methods {
	int	(*dm_dmamap_create)(bus_dma_tag_t, int, bus_dmamap_t *);
	int	(*dm_dmamap_destroy)(bus_dma_tag_t, bus_dmamap_t);
	int	(*dm_dmamap_load_phys)(bus_dma_tag_t dmat, bus_dmamap_t map,
	    vm_paddr_t buf, bus_size_t buflen, int flags,
	    bus_dma_segment_t *segs, int *segp);
	int	(*dm_dmamap_load_buffer)(bus_dma_tag_t dmat, bus_dmamap_t map,
	    void *buf, bus_size_t buflen, struct pmap *pmap, int flags,
	    bus_dma_segment_t *segs, int *segp);
	void	(*dm_dmamap_waitok)(bus_dma_tag_t dmat, bus_dmamap_t map,
	    struct memdesc *mem, bus_dmamap_callback_t *callback,
	    void *callback_arg);
	bus_dma_segment_t *(*dm_dmamap_complete)(bus_dma_tag_t dmat,
	    bus_dmamap_t map, bus_dma_segment_t *segs, int nsegs, int error);
	void	(*dm_dmamap_unload)(bus_dma_tag_t, bus_dmamap_t);
	void	(*dm_dmamap_sync)(bus_dma_tag_t, bus_dmamap_t,
	    bus_dmasync_op_t);
	int	(*dm_dmamem_alloc)(bus_dma_tag_t, void **, int, bus_dmamap_t *);
	void	(*dm_dmamem_free)(bus_dma_tag_t, void *, bus_dmamap_t);
};

/*
 * bus_dma_tag_t
 *
 * A machine-dependent opaque type describing the implementation of
 * DMA for a given bus.
 */
struct bus_dma_tag {
	void		*dt_cookie;		/* cookie used in the guts */
	bus_dma_tag_t	dt_parent;
	bus_size_t	dt_alignment;
	bus_addr_t	dt_boundary;
	bus_addr_t	dt_lowaddr;
	bus_addr_t	dt_highaddr;
	bus_dma_filter_t	*dt_filter;
	void		*dt_filterarg;
	bus_size_t	dt_maxsize;
	int		dt_nsegments;
	bus_size_t	dt_maxsegsz;
	int		dt_flags;
	int		dt_ref_count;
	int		dt_map_count;
	bus_dma_lock_t	*dt_lockfunc;
	void *		*dt_lockfuncarg;
	bus_dma_segment_t *dt_segments;

	struct bus_dma_methods	*dt_mt;
};

#define	bus_dmamap_create(t, f, p)					\
	((t)->dt_mt->dm_dmamap_create((t), (f), (p)))
#define	bus_dmamap_destroy(t, p)					\
	((t)->dt_mt->dm_dmamap_destroy((t), (p)))
#define	_bus_dmamap_load_phys(t, m, b, l, f, s, sp)			\
	((t)->dt_mt->dm_dmamap_load_phys((t), (m), (b), (l),		\
	    (f), (s), (sp)))
#define	_bus_dmamap_load_buffer(t, m, b, l, p, f, s, sp)		\
	((t)->dt_mt->dm_dmamap_load_buffer((t), (m), (b), (l), (p),	\
	    (f), (s), (sp)))
#define	_bus_dmamap_waitok(t, m, mem, c, ca)				\
	((t)->dt_mt->dm_dmamap_waitok((t), (m), (mem), (c), (ca)))
#define	_bus_dmamap_complete(t, m, s, n, e)				\
	((t)->dt_mt->dm_dmamap_complete((t), (m), (s), (n), (e)))
#define	bus_dmamap_unload(t, p)						\
	((t)->dt_mt->dm_dmamap_unload((t), (p)))
#define	bus_dmamap_sync(t, m, op)					\
	((t)->dt_mt->dm_dmamap_sync((t), (m), (op)))
#define	bus_dmamem_alloc(t, v, f, m)					\
	((t)->dt_mt->dm_dmamem_alloc((t), (v), (f), (m)))
#define	bus_dmamem_free(t, v, m)					\
	((t)->dt_mt->dm_dmamem_free((t), (v), (m)))
#define _bus_dmamap_load_ma(t, m, a, tt, o, f, s, p)			\
	bus_dmamap_load_ma_triv((t), (m), (a), (tt), (o), (f), (s), (p))

#endif /* !_SPARC64_BUS_DMA_H_ */
