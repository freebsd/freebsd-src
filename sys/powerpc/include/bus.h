/*-
 * Copyright (c) 1996, 1997, 1998 The NetBSD Foundation, Inc.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 *	$NetBSD: bus.h,v 1.9.4.1 2000/06/30 16:27:30 simonb Exp $
 * $FreeBSD$
 */

#ifndef	_MACPPC_BUS_H_
#define	_MACPPC_BUS_H_

#include <machine/pio.h>

/*
 * Values for the macppc bus space tag, not to be used directly by MI code.
 */

#define	__BUS_SPACE_HAS_STREAM_METHODS

#define	MACPPC_BUS_ADDR_MASK	0xfffff000
#define	MACPPC_BUS_STRIDE_MASK	0x0000000f

#define macppc_make_bus_space_tag(addr, stride) \
	(((addr) & MACPPC_BUS_ADDR_MASK) | (stride))
#define	__BA(t, h, o) ((void *)((h) + ((o) << ((t) & MACPPC_BUS_STRIDE_MASK))))

/*
 * Bus address and size types
 */
typedef u_int32_t bus_addr_t;
typedef u_int32_t bus_size_t;

/*
 * Access methods for bus resources and address space.
 */
typedef u_int32_t bus_space_tag_t;
typedef u_int32_t bus_space_handle_t;

/*
 *	int bus_space_map(bus_space_tag_t t, bus_addr_t addr,
 *	    bus_size_t size, int flags, bus_space_handle_t *bshp));
 *
 * Map a region of bus space.
 */

#define	BUS_SPACE_MAP_CACHEABLE		0x01
#define	BUS_SPACE_MAP_LINEAR		0x02
#define	BUS_SPACE_MAP_PREFETCHABLE	0x04

extern void *mapiodev(vm_offset_t, vm_size_t);

static __inline int
bus_space_map(bus_space_tag_t t, bus_addr_t addr, bus_size_t size, int flags,
    bus_space_handle_t *bshp)
{
	vm_offset_t base = t & MACPPC_BUS_ADDR_MASK;
	int stride = t & MACPPC_BUS_STRIDE_MASK;

	*bshp = (bus_space_handle_t)
		mapiodev(base + (addr << stride), size << stride);
	return 0;
}

/*
 *	int bus_space_unmap(bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t size));
 *
 * Unmap a region of bus space.
 */

#define bus_space_unmap(t, bsh, size)

/*
 *	int bus_space_subregion(bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t offset, bus_size_t size,
 *	    bus_space_handle_t *nbshp));
 *
 * Get a new handle for a subregion of an already-mapped area of bus space.
 */

#define	bus_space_subregion(t, bsh, offset, size, bshp)			\
	((*(bshp) = (bus_space_handle_t)__BA(t, bsh, offset)), 0)

/*
 *	int bus_space_alloc(bus_space_tag_t t, bus_addr_t rstart,
 *	    bus_addr_t rend, bus_size_t size, bus_size_t align,
 *	    bus_size_t boundary, int flags, bus_addr_t *addrp,
 *	    bus_space_handle_t *bshp));
 *
 * Allocate a region of bus space.
 */

#if 0
#define	bus_space_alloc(t, rs, re, s, a, b, f, ap, hp)	!!! unimplemented !!!
#endif

/*
 *	int bus_space_free(bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t size));
 *
 * Free a region of bus space.
 */
#if 0
#define	bus_space_free(t, h, s)		!!! unimplemented !!!
#endif

/*
 *	u_intN_t bus_space_read_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset));
 *
 * Read a 1, 2, 4, or 8 byte quantity from bus space
 * described by tag/handle/offset.
 */

#define	bus_space_read_1(t, h, o)	(in8(__BA(t, h, o)))
#define	bus_space_read_2(t, h, o)	(in16rb(__BA(t, h, o)))
#define	bus_space_read_4(t, h, o)	(in32rb(__BA(t, h, o)))
#if 0	/* Cause a link error for bus_space_read_8 */
#define	bus_space_read_8(t, h, o)	!!! unimplemented !!!
#endif

#define	bus_space_read_stream_1(t, h, o)	(in8(__BA(t, h, o)))
#define	bus_space_read_stream_2(t, h, o)	(in16(__BA(t, h, o)))
#define	bus_space_read_stream_4(t, h, o)	(in32(__BA(t, h, o)))
#if 0	/* Cause a link error for bus_space_read_stream_8 */
#define	bus_space_read_8(t, h, o)	!!! unimplemented !!!
#endif

/*
 *	void bus_space_read_multi_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t *addr, size_t count));
 *
 * Read `count' 1, 2, 4, or 8 byte quantities from bus space
 * described by tag/handle/offset and copy into buffer provided.
 */

#define	bus_space_read_multi_1(t, h, o, a, c) do {			\
		ins8(__BA(t, h, o), (a), (c));				\
	} while (0)

#define	bus_space_read_multi_2(t, h, o, a, c) do {			\
		ins16rb(__BA(t, h, o), (a), (c));			\
	} while (0)

#define	bus_space_read_multi_4(t, h, o, a, c) do {			\
		ins32rb(__BA(t, h, o), (a), (c));			\
	} while (0)

#if 0	/* Cause a link error for bus_space_read_multi_8 */
#define	bus_space_read_multi_8		!!! unimplemented !!!
#endif

#define	bus_space_read_multi_stream_1(t, h, o, a, c) do {		\
		ins8(__BA(t, h, o), (a), (c));				\
	} while (0)

#define	bus_space_read_multi_stream_2(t, h, o, a, c) do {		\
		ins16(__BA(t, h, o), (a), (c));				\
	} while (0)

#define	bus_space_read_multi_stream_4(t, h, o, a, c) do {		\
		ins32(__BA(t, h, o), (a), (c));				\
	} while (0)

#if 0	/* Cause a link error for bus_space_read_multi_stream_8 */
#define	bus_space_read_multi_stream_8	!!! unimplemented !!!
#endif

/*
 *	void bus_space_read_region_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t *addr, size_t count));
 *
 * Read `count' 1, 2, 4, or 8 byte quantities from bus space
 * described by tag/handle and starting at `offset' and copy into
 * buffer provided.
 */

static __inline void
bus_space_read_region_1(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int8_t *addr, size_t count)
{
	volatile u_int8_t *s = __BA(tag, bsh, offset);

	while (count--)
		*addr++ = *s++;
	__asm __volatile("eieio; sync");
}

static __inline void
bus_space_read_region_2(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int16_t *addr, size_t count)
{
	volatile u_int16_t *s = __BA(tag, bsh, offset);

	while (count--)
		__asm __volatile("lhbrx %0, 0, %1" :
			"=r"(*addr++) : "r"(s++));
	__asm __volatile("eieio; sync");
}

static __inline void
bus_space_read_region_4(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int32_t *addr, size_t count)
{
	volatile u_int32_t *s = __BA(tag, bsh, offset);

	while (count--)
		__asm __volatile("lwbrx %0, 0, %1" :
			"=r"(*addr++) : "r"(s++));
	__asm __volatile("eieio; sync");
}

#if 0	/* Cause a link error for bus_space_read_region_8 */
#define	bus_space_read_region_8		!!! unimplemented !!!
#endif

static __inline void
bus_space_read_region_stream_2(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int16_t *addr, size_t count)
{
	volatile u_int16_t *s = __BA(tag, bsh, offset);

	while (count--)
		*addr++ = *s++;
	__asm __volatile("eieio; sync");
}

static __inline void
bus_space_read_region_stream_4(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int32_t *addr, size_t count)
{
	volatile u_int32_t *s = __BA(tag, bsh, offset);

	while (count--)
		*addr++ = *s++;
	__asm __volatile("eieio; sync");
}

#if 0	/* Cause a link error */
#define	bus_space_read_region_stream_8		!!! unimplemented !!!
#endif

/*
 *	void bus_space_write_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t value));
 *
 * Write the 1, 2, 4, or 8 byte value `value' to bus space
 * described by tag/handle/offset.
 */

#define bus_space_write_1(t, h, o, v)	out8(__BA(t, h, o), (v))
#define bus_space_write_2(t, h, o, v)	out16rb(__BA(t, h, o), (v))
#define bus_space_write_4(t, h, o, v)	out32rb(__BA(t, h, o), (v))

#define bus_space_write_stream_1(t, h, o, v)	out8(__BA(t, h, o), (v))
#define bus_space_write_stream_2(t, h, o, v)	out16(__BA(t, h, o), (v))
#define bus_space_write_stream_4(t, h, o, v)	out32(__BA(t, h, o), (v))

#if 0	/* Cause a link error for bus_space_write_8 */
#define bus_space_write_8		!!! unimplemented !!!
#endif

/*
 *	void bus_space_write_multi_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    const u_intN_t *addr, size_t count));
 *
 * Write `count' 1, 2, 4, or 8 byte quantities from the buffer
 * provided to bus space described by tag/handle/offset.
 */

#define bus_space_write_multi_1(t, h, o, a, c) do {			\
		outsb(__BA(t, h, o), (a), (c));				\
	} while (0)

#define bus_space_write_multi_2(t, h, o, a, c) do {			\
		outsw(__BA(t, h, o), (a), (c));				\
	} while (0)

#define bus_space_write_multi_4(t, h, o, a, c) do {			\
		outsl(__BA(t, h, o), (a), (c));				\
	} while (0)

#if 0
#define bus_space_write_multi_8		!!! unimplemented !!!
#endif

#define bus_space_write_multi_stream_2(t, h, o, a, c) do {		\
		outsw(__BA(t, h, o), (a), (c));				\
	} while (0)

#define bus_space_write_multi_stream_4(t, h, o, a, c) do {		\
		outsl(__BA(t, h, o), (a), (c));				\
	} while (0)

#if 0
#define bus_space_write_multi_stream_8	!!! unimplemented !!!
#endif

/*
 *	void bus_space_write_region_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    const u_intN_t *addr, size_t count));
 *
 * Write `count' 1, 2, 4, or 8 byte quantities from the buffer provided
 * to bus space described by tag/handle starting at `offset'.
 */

static __inline void
bus_space_write_region_1(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, const u_int8_t *addr, size_t count)
{
	volatile u_int8_t *d = __BA(tag, bsh, offset);

	while (count--)
		*d++ = *addr++;
	__asm __volatile("eieio; sync");
}

static __inline void
bus_space_write_region_2(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, const u_int16_t *addr, size_t count)
{
	volatile u_int16_t *d = __BA(tag, bsh, offset);

	while (count--)
		__asm __volatile("sthbrx %0, 0, %1" ::
			"r"(*addr++), "r"(d++));
	__asm __volatile("eieio; sync");
}

static __inline void
bus_space_write_region_4(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, const u_int32_t *addr, size_t count)
{
	volatile u_int32_t *d = __BA(tag, bsh, offset);

	while (count--)
		__asm __volatile("stwbrx %0, 0, %1" ::
			"r"(*addr++), "r"(d++));
	__asm __volatile("eieio; sync");
}

#if 0
#define	bus_space_write_region_8 !!! bus_space_write_region_8 unimplemented !!!
#endif

static __inline void
bus_space_write_region_stream_2(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, const u_int16_t *addr, size_t count)
{
	volatile u_int16_t *d = __BA(tag, bsh, offset);

	while (count--)
		*d++ = *addr++;
	__asm __volatile("eieio; sync");
}

static __inline void
bus_space_write_region_stream_4(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, const u_int32_t *addr, size_t count)
{
	volatile u_int32_t *d = __BA(tag, bsh, offset);

	while (count--)
		*d++ = *addr++;
	__asm __volatile("eieio; sync");
}

#if 0
#define	bus_space_write_region_stream_8	!!! unimplemented !!!
#endif

/*
 *	void bus_space_set_multi_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset, u_intN_t val,
 *	    size_t count));
 *
 * Write the 1, 2, 4, or 8 byte value `val' to bus space described
 * by tag/handle/offset `count' times.
 */

static __inline void
bus_space_set_multi_1(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int8_t val, size_t count)
{
	volatile u_int8_t *d = __BA(tag, bsh, offset);

	while (count--)
		*d = val;
	__asm __volatile("eieio; sync");
}

static __inline void
bus_space_set_multi_2(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int16_t val, size_t count)
{
	volatile u_int16_t *d = __BA(tag, bsh, offset);

	while (count--)
		__asm __volatile("sthbrx %0, 0, %1" ::
			"r"(val), "r"(d));
	__asm __volatile("eieio; sync");
}

static __inline void
bus_space_set_multi_4(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int32_t val, size_t count)
{
	volatile u_int32_t *d = __BA(tag, bsh, offset);

	while (count--)
		__asm __volatile("stwbrx %0, 0, %1" ::
			"r"(val), "r"(d));
	__asm __volatile("eieio; sync");
}

#if 0
#define	bus_space_set_multi_8 !!! bus_space_set_multi_8 unimplemented !!!
#endif

static __inline void
bus_space_set_multi_stream_2(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int16_t val, size_t count)
{
	volatile u_int16_t *d = __BA(tag, bsh, offset);

	while (count--)
		*d = val;
	__asm __volatile("eieio; sync");
}

static __inline void
bus_space_set_multi_stream_4(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int32_t val, size_t count)
{
	volatile u_int32_t *d = __BA(tag, bsh, offset);

	while (count--)
		*d = val;
	__asm __volatile("eieio; sync");
}

#if 0
#define	bus_space_set_multi_stream_8	!!! unimplemented !!!
#endif

/*
 *	void bus_space_set_region_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset, u_intN_t val,
 *	    size_t count));
 *
 * Write `count' 1, 2, 4, or 8 byte value `val' to bus space described
 * by tag/handle starting at `offset'.
 */

static __inline void
bus_space_set_region_1(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int8_t val, size_t count)
{
	volatile u_int8_t *d = __BA(tag, bsh, offset);

	while (count--)
		*d++ = val;
	__asm __volatile("eieio; sync");
}

static __inline void
bus_space_set_region_2(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int16_t val, size_t count)
{
	volatile u_int16_t *d = __BA(tag, bsh, offset);

	while (count--)
		__asm __volatile("sthbrx %0, 0, %1" ::
			"r"(val), "r"(d++));
	__asm __volatile("eieio; sync");
}

static __inline void
bus_space_set_region_4(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int32_t val, size_t count)
{
	volatile u_int32_t *d = __BA(tag, bsh, offset);

	while (count--)
		__asm __volatile("stwbrx %0, 0, %1" ::
			"r"(val), "r"(d++));
	__asm __volatile("eieio; sync");
}

#if 0
#define	bus_space_set_region_8 !!! bus_space_set_region_8 unimplemented !!!
#endif

static __inline void
bus_space_set_region_stream_2(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int16_t val, size_t count)
{
	volatile u_int16_t *d = __BA(tag, bsh, offset);

	while (count--)
		*d++ = val;
	__asm __volatile("eieio; sync");
}

static __inline void
bus_space_set_region_stream_4(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int32_t val, size_t count)
{
	volatile u_int32_t *d = __BA(tag, bsh, offset);

	while (count--)
		*d++ = val;
	__asm __volatile("eieio; sync");
}

#if 0
#define	bus_space_set_region_stream_8	!!! unimplemented !!!
#endif

/*
 *	void bus_space_copy_region_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh1, bus_size_t off1,
 *	    bus_space_handle_t bsh2, bus_size_t off2,
 *	    size_t count));
 *
 * Copy `count' 1, 2, 4, or 8 byte values from bus space starting
 * at tag/bsh1/off1 to bus space starting at tag/bsh2/off2.
 */

	/* XXX IMPLEMENT bus_space_copy_N() XXX */

/*
 * Bus read/write barrier methods.
 *
 *	void bus_space_barrier(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    bus_size_t len, int flags));
 *
 * Note: the macppc does not currently require barriers, but we must
 * provide the flags to MI code.
 */

#define bus_space_barrier(t, h, o, l, f)	\
	((void)((void)(t), (void)(h), (void)(o), (void)(l), (void)(f)))
#define	BUS_SPACE_BARRIER_READ	0x01		/* force read barrier */
#define	BUS_SPACE_BARRIER_WRITE	0x02		/* force write barrier */

#define	BUS_SPACE_ALIGNED_POINTER(p, t) ALIGNED_POINTER(p, t)

/*
 * Bus DMA methods.
 */

/*
 * Flags used in various bus DMA methods.
 */
#define	BUS_DMA_WAITOK		0x00	/* safe to sleep (pseudo-flag) */
#define	BUS_DMA_NOWAIT		0x01	/* not safe to sleep */
#define	BUS_DMA_ALLOCNOW	0x02	/* perform resource allocation now */
#define	BUS_DMA_COHERENT	0x04	/* hint: map memory DMA coherent */
#define	BUS_DMA_BUS1		0x10	/* placeholders for bus functions... */
#define	BUS_DMA_BUS2		0x20
#define	BUS_DMA_BUS3		0x40
#define	BUS_DMA_BUS4		0x80

/* Forwards needed by prototypes below. */
struct mbuf;
struct uio;

/*
 * Operations performed by bus_dmamap_sync().
 */
#define	BUS_DMASYNC_PREREAD	0x01	/* pre-read synchronization */
#define	BUS_DMASYNC_POSTREAD	0x02	/* post-read synchronization */
#define	BUS_DMASYNC_PREWRITE	0x04	/* pre-write synchronization */
#define	BUS_DMASYNC_POSTWRITE	0x08	/* post-write synchronization */

typedef struct macppc_bus_dma_tag	*bus_dma_tag_t;
typedef struct macppc_bus_dmamap	*bus_dmamap_t;

/*
 *	bus_dma_segment_t
 *
 *	Describes a single contiguous DMA transaction.  Values
 *	are suitable for programming into DMA registers.
 */
struct macppc_bus_dma_segment {
	bus_addr_t	ds_addr;	/* DMA address */
	bus_size_t	ds_len;		/* length of transfer */
};
typedef struct macppc_bus_dma_segment	bus_dma_segment_t;

/*
 *	bus_dma_tag_t
 *
 *	A machine-dependent opaque type describing the implementation of
 *	DMA for a given bus.
 */

struct macppc_bus_dma_tag {
	/*
	 * The `bounce threshold' is checked while we are loading
	 * the DMA map.  If the physical address of the segment
	 * exceeds the threshold, an error will be returned.  The
	 * caller can then take whatever action is necessary to
	 * bounce the transfer.  If this value is 0, it will be
	 * ignored.
	 */
	bus_addr_t _bounce_thresh;

	/*
	 * DMA mapping methods.
	 */
	int	(*_dmamap_create)(bus_dma_tag_t, bus_size_t, int,
		    bus_size_t, bus_size_t, int, bus_dmamap_t *);
	void	(*_dmamap_destroy)(bus_dma_tag_t, bus_dmamap_t);
	int	(*_dmamap_load)(bus_dma_tag_t, bus_dmamap_t, void *,
		    bus_size_t, struct proc *, int);
	int	(*_dmamap_load_mbuf)(bus_dma_tag_t, bus_dmamap_t,
		    struct mbuf *, int);
	int	(*_dmamap_load_uio)(bus_dma_tag_t, bus_dmamap_t,
		    struct uio *, int);
	int	(*_dmamap_load_raw)(bus_dma_tag_t, bus_dmamap_t,
		    bus_dma_segment_t *, int, bus_size_t, int);
	void	(*_dmamap_unload)(bus_dma_tag_t, bus_dmamap_t);
	void	(*_dmamap_sync)(bus_dma_tag_t, bus_dmamap_t,
		    bus_addr_t, bus_size_t, int);

	/*
	 * DMA memory utility functions.
	 */
	int	(*_dmamem_alloc)(bus_dma_tag_t, bus_size_t, bus_size_t,
		    bus_size_t, bus_dma_segment_t *, int, int *, int);
	void	(*_dmamem_free)(bus_dma_tag_t, bus_dma_segment_t *, int);
	int	(*_dmamem_map)(bus_dma_tag_t, bus_dma_segment_t *,
		    int, size_t, caddr_t *, int);
	void	(*_dmamem_unmap)(bus_dma_tag_t, caddr_t, size_t);
	vm_offset_t	(*_dmamem_mmap)(bus_dma_tag_t, bus_dma_segment_t *,
		    int, off_t, int, int);
};

#define	bus_dmamap_create(t, s, n, m, b, f, p)			\
	(*(t)->_dmamap_create)((t), (s), (n), (m), (b), (f), (p))
#define	bus_dmamap_destroy(t, p)				\
	(*(t)->_dmamap_destroy)((t), (p))
#define	bus_dmamap_load(t, m, b, s, p, f)			\
	(*(t)->_dmamap_load)((t), (m), (b), (s), (p), (f))
#define	bus_dmamap_load_mbuf(t, m, b, f)			\
	(*(t)->_dmamap_load_mbuf)((t), (m), (b), (f))
#define	bus_dmamap_load_uio(t, m, u, f)				\
	(*(t)->_dmamap_load_uio)((t), (m), (u), (f))
#define	bus_dmamap_load_raw(t, m, sg, n, s, f)			\
	(*(t)->_dmamap_load_raw)((t), (m), (sg), (n), (s), (f))
#define	bus_dmamap_unload(t, p)					\
	(*(t)->_dmamap_unload)((t), (p))
#define	bus_dmamap_sync(t, p, o, l, ops)			\
	(void)((t)->_dmamap_sync ?				\
	    (*(t)->_dmamap_sync)((t), (p), (o), (l), (ops)) : (void)0)

#define	bus_dmamem_alloc(t, s, a, b, sg, n, r, f)		\
	(*(t)->_dmamem_alloc)((t), (s), (a), (b), (sg), (n), (r), (f))
#define	bus_dmamem_free(t, sg, n)				\
	(*(t)->_dmamem_free)((t), (sg), (n))
#define	bus_dmamem_map(t, sg, n, s, k, f)			\
	(*(t)->_dmamem_map)((t), (sg), (n), (s), (k), (f))
#define	bus_dmamem_unmap(t, k, s)				\
	(*(t)->_dmamem_unmap)((t), (k), (s))
#define	bus_dmamem_mmap(t, sg, n, o, p, f)			\
	(*(t)->_dmamem_mmap)((t), (sg), (n), (o), (p), (f))

/*
 *	bus_dmamap_t
 *
 *	Describes a DMA mapping.
 */
struct macppc_bus_dmamap {
	/*
	 * PRIVATE MEMBERS: not for use my machine-independent code.
	 */
	bus_size_t	 _dm_size;	/* largest DMA transfer mappable */
	int		 _dm_segcnt;	/* number of segs this map can map */
	bus_size_t	 _dm_maxsegsz;	/* largest possible segment */
	bus_size_t	 _dm_boundary;	/* don't cross this */
	bus_addr_t	 _dm_bounce_thresh; /* bounce threshold; see tag */
	int		 _dm_flags;	/* misc. flags */

	void		*_dm_cookie;	/* cookie for bus-specific functions */

	/*
	 * PUBLIC MEMBERS: these are used by machine-independent code.
	 */
	bus_size_t	dm_mapsize;	/* size of the mapping */
	int		dm_nsegs;	/* # valid segments in mapping */
	bus_dma_segment_t dm_segs[1];	/* segments; variable length */
};

#ifdef _MACPPC_BUS_DMA_PRIVATE
int	_bus_dmamap_create(bus_dma_tag_t, bus_size_t, int, bus_size_t,
	    bus_size_t, int, bus_dmamap_t *);
void	_bus_dmamap_destroy(bus_dma_tag_t, bus_dmamap_t);
int	_bus_dmamap_load(bus_dma_tag_t, bus_dmamap_t, void *,
	    bus_size_t, struct proc *, int);
int	_bus_dmamap_load_mbuf(bus_dma_tag_t, bus_dmamap_t, struct mbuf *, int);
int	_bus_dmamap_load_uio(bus_dma_tag_t, bus_dmamap_t, struct uio *, int);
int	_bus_dmamap_load_raw(bus_dma_tag_t, bus_dmamap_t,
	    bus_dma_segment_t *, int, bus_size_t, int);
void	_bus_dmamap_unload(bus_dma_tag_t, bus_dmamap_t);
void	_bus_dmamap_sync(bus_dma_tag_t, bus_dmamap_t, bus_addr_t,
	    bus_size_t, int);

int	_bus_dmamem_alloc(bus_dma_tag_t tag, bus_size_t size,
	    bus_size_t alignment, bus_size_t boundary,
	    bus_dma_segment_t *segs, int nsegs, int *rsegs, int flags);
void	_bus_dmamem_free(bus_dma_tag_t tag, bus_dma_segment_t *segs, int nsegs);
int	_bus_dmamem_map(bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs, size_t size, caddr_t *kvap, int flags);
void	_bus_dmamem_unmap(bus_dma_tag_t tag, caddr_t kva,
	    size_t size);
vm_offset_t	_bus_dmamem_mmap(bus_dma_tag_t tag,
	    bus_dma_segment_t *segs,
	    int nsegs, off_t off, int prot, int flags);

int	_bus_dmamem_alloc_range(bus_dma_tag_t tag, bus_size_t size,
	    bus_size_t alignment, bus_size_t boundary,
	    bus_dma_segment_t *segs, int nsegs, int *rsegs, int flags,
	    vm_offset_t low, vm_offset_t high);

#endif /* _MACPPC_BUS_DMA_PRIVATE */

#endif /* _MACPPC_BUS_H_ */
