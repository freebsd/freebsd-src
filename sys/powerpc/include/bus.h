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

/*-
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
 * $FreeBSD: src/sys/powerpc/include/bus.h,v 1.19 2006/12/13 03:25:31 marcel Exp $
 */

#ifndef	_MACPPC_BUS_H_
#define	_MACPPC_BUS_H_

#include <machine/_bus.h>
#include <machine/pio.h>

#define BUS_SPACE_MAXSIZE_24BIT 0xFFFFFF
#define BUS_SPACE_MAXSIZE_32BIT 0xFFFFFFFF
#define BUS_SPACE_MAXSIZE       0xFFFFFFFF
#define BUS_SPACE_MAXADDR_24BIT 0xFFFFFF
#define BUS_SPACE_MAXADDR_32BIT 0xFFFFFFFF
#define BUS_SPACE_MAXADDR       0xFFFFFFFF

#define BUS_SPACE_UNRESTRICTED  (~0)

/*
 * Values for the macppc bus space tag, not to be used directly by MI code.
 */

#define	__BUS_SPACE_HAS_STREAM_METHODS 1

/*
 * Define the PPC tag values
 */
#define PPC_BUS_SPACE_MEM	1	/* space is mem space */
#define PPC_BUS_SPACE_IO	2	/* space is io space */

static __inline void *
__ppc_ba(bus_space_tag_t tag __unused, bus_space_handle_t handle, 
    bus_size_t offset)
{
	return ((void *)(handle + offset));
}

/*
 *	int bus_space_map(bus_space_tag_t t, bus_addr_t addr,
 *	    bus_size_t size, int flags, bus_space_handle_t *bshp));
 *
 * Map a region of bus space.
 */

static __inline int
bus_space_map(bus_space_tag_t t __unused, bus_addr_t addr,
	      bus_size_t size __unused, int flags __unused,
	      bus_space_handle_t *bshp)
{

	*bshp = addr;
	return (0);
}

/*
 *	int bus_space_unmap(bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t size));
 *
 * Unmap a region of bus space.
 */

static __inline void
bus_space_unmap(bus_space_tag_t t __unused, bus_space_handle_t bsh __unused,
                bus_size_t size __unused)
{
}

/*
 *	int bus_space_subregion(bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t offset, bus_size_t size,
 *	    bus_space_handle_t *nbshp));
 *
 * Get a new handle for a subregion of an already-mapped area of bus space.
 */

static __inline int
bus_space_subregion(bus_space_tag_t t __unused, bus_space_handle_t bsh, 
    bus_size_t offset, bus_size_t size __unused, bus_space_handle_t *nbshp)
{
	*nbshp = bsh + offset;
	return (0);
}

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

static __inline u_int8_t
bus_space_read_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return (in8(__ppc_ba(t, h, o)));
}

static __inline u_int16_t
bus_space_read_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return (in16rb(__ppc_ba(t, h, o)));
}

static __inline u_int32_t
bus_space_read_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return (in32rb(__ppc_ba(t, h, o)));
}

#if 0	/* Cause a link error for bus_space_read_8 */
#define	bus_space_read_8(t, h, o)	!!! unimplemented !!!
#endif

static __inline u_int8_t
bus_space_read_stream_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return (in8(__ppc_ba(t, h, o)));
}

static __inline u_int16_t
bus_space_read_stream_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return (in16(__ppc_ba(t, h, o)));
}

static __inline u_int32_t
bus_space_read_stream_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return (in32(__ppc_ba(t, h, o)));
}

#if 0	/* Cause a link error for bus_space_read_stream_8 */
#define	bus_space_read_stream_8(t, h, o)	!!! unimplemented !!!
#endif

/*
 *	void bus_space_read_multi_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t *addr, size_t count));
 *
 * Read `count' 1, 2, 4, or 8 byte quantities from bus space
 * described by tag/handle/offset and copy into buffer provided.
 */

static __inline void
bus_space_read_multi_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    u_int8_t *a, size_t c) 
{
	ins8(__ppc_ba(t, h, o), a, c);
}

static __inline void
bus_space_read_multi_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    u_int16_t *a, size_t c) 
{
	ins16rb(__ppc_ba(t, h, o), a, c);
}

static __inline void
bus_space_read_multi_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    u_int32_t *a, size_t c) 
{
	ins32rb(__ppc_ba(t, h, o), a, c);
}

#if 0	/* Cause a link error for bus_space_read_multi_8 */
#define	bus_space_read_multi_8		!!! unimplemented !!!
#endif

static __inline void
bus_space_read_multi_stream_1(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, u_int8_t *a, size_t c)
{
	ins8(__ppc_ba(t, h, o), a, c);
}

static __inline void
bus_space_read_multi_stream_2(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, u_int16_t *a, size_t c)
{
	ins16(__ppc_ba(t, h, o), a, c);
}

static __inline void
bus_space_read_multi_stream_4(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, u_int32_t *a, size_t c)
{
	ins32(__ppc_ba(t, h, o), a, c);
}

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
	volatile u_int8_t *s = __ppc_ba(tag, bsh, offset);

	while (count--)
		*addr++ = *s++;
	__asm __volatile("eieio; sync");
}

static __inline void
bus_space_read_region_2(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int16_t *addr, size_t count)
{
	volatile u_int16_t *s = __ppc_ba(tag, bsh, offset);

	while (count--)
		__asm __volatile("lhbrx %0, 0, %1" :
			"=r"(*addr++) : "r"(s++));
	__asm __volatile("eieio; sync");
}

static __inline void
bus_space_read_region_4(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int32_t *addr, size_t count)
{
	volatile u_int32_t *s = __ppc_ba(tag, bsh, offset);

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
	volatile u_int16_t *s = __ppc_ba(tag, bsh, offset);

	while (count--)
		*addr++ = *s++;
	__asm __volatile("eieio; sync");
}

static __inline void
bus_space_read_region_stream_4(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int32_t *addr, size_t count)
{
	volatile u_int32_t *s = __ppc_ba(tag, bsh, offset);

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

static __inline void
bus_space_write_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint8_t v)
{
	out8(__ppc_ba(t, h, o), v);
}

static __inline void
bus_space_write_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint16_t v)
{
	out16rb(__ppc_ba(t, h, o), v);
}

static __inline void
bus_space_write_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint32_t v)
{
	out32rb(__ppc_ba(t, h, o), v);
}

#if 0	/* Cause a link error for bus_space_write_8 */
#define bus_space_write_8		!!! unimplemented !!!
#endif

static __inline void
bus_space_write_stream_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint8_t v)
{
	out8(__ppc_ba(t, h, o), v);
}

static __inline void
bus_space_write_stream_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint16_t v)
{
	out16(__ppc_ba(t, h, o), v);
}

static __inline void
bus_space_write_stream_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint32_t v)
{
	out32(__ppc_ba(t, h, o), v);
}

#if 0	/* Cause a link error for bus_space_write_stream_8 */
#define bus_space_write_stream_8       	!!! unimplemented !!!
#endif


/*
 *	void bus_space_write_multi_N(bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    const u_intN_t *addr, size_t count));
 *
 * Write `count' 1, 2, 4, or 8 byte quantities from the buffer
 * provided to bus space described by tag/handle/offset.
 */

static __inline void
bus_space_write_multi_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint8_t *a, size_t c)
{
	outsb(__ppc_ba(t, h, o), a, c);
}

static __inline void
bus_space_write_multi_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint16_t *a, size_t c)
{
	outsw(__ppc_ba(t, h, o), a, c);
}

static __inline void
bus_space_write_multi_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint32_t *a, size_t c)
{
	outsl(__ppc_ba(t, h, o), a, c);
}

#if 0
#define bus_space_write_multi_8		!!! unimplemented !!!
#endif

static __inline void
bus_space_write_multi_stream_1(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, const u_int8_t *a, size_t c)
{
	outsb(__ppc_ba(t, h, o), a, c);
}

static __inline void
bus_space_write_multi_stream_2(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, const u_int16_t *a, size_t c)
{
	outsw(__ppc_ba(t, h, o), a, c);
}

static __inline void
bus_space_write_multi_stream_4(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, const u_int32_t *a, size_t c)
{
	outsl(__ppc_ba(t, h, o), a, c);
}

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
	volatile u_int8_t *d = __ppc_ba(tag, bsh, offset);

	while (count--)
		*d++ = *addr++;
	__asm __volatile("eieio; sync");
}

static __inline void
bus_space_write_region_2(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, const u_int16_t *addr, size_t count)
{
	volatile u_int16_t *d = __ppc_ba(tag, bsh, offset);

	while (count--)
		__asm __volatile("sthbrx %0, 0, %1" ::
			"r"(*addr++), "r"(d++));
	__asm __volatile("eieio; sync");
}

static __inline void
bus_space_write_region_4(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, const u_int32_t *addr, size_t count)
{
	volatile u_int32_t *d = __ppc_ba(tag, bsh, offset);

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
	volatile u_int16_t *d = __ppc_ba(tag, bsh, offset);

	while (count--)
		*d++ = *addr++;
	__asm __volatile("eieio; sync");
}

static __inline void
bus_space_write_region_stream_4(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, const u_int32_t *addr, size_t count)
{
	volatile u_int32_t *d = __ppc_ba(tag, bsh, offset);

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
	volatile u_int8_t *d = __ppc_ba(tag, bsh, offset);

	while (count--)
		*d = val;
	__asm __volatile("eieio; sync");
}

static __inline void
bus_space_set_multi_2(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int16_t val, size_t count)
{
	volatile u_int16_t *d = __ppc_ba(tag, bsh, offset);

	while (count--)
		__asm __volatile("sthbrx %0, 0, %1" ::
			"r"(val), "r"(d));
	__asm __volatile("eieio; sync");
}

static __inline void
bus_space_set_multi_4(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int32_t val, size_t count)
{
	volatile u_int32_t *d = __ppc_ba(tag, bsh, offset);

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
	volatile u_int16_t *d = __ppc_ba(tag, bsh, offset);

	while (count--)
		*d = val;
	__asm __volatile("eieio; sync");
}

static __inline void
bus_space_set_multi_stream_4(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int32_t val, size_t count)
{
	volatile u_int32_t *d = __ppc_ba(tag, bsh, offset);

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
	volatile u_int8_t *d = __ppc_ba(tag, bsh, offset);

	while (count--)
		*d++ = val;
	__asm __volatile("eieio; sync");
}

static __inline void
bus_space_set_region_2(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int16_t val, size_t count)
{
	volatile u_int16_t *d = __ppc_ba(tag, bsh, offset);

	while (count--)
		__asm __volatile("sthbrx %0, 0, %1" ::
			"r"(val), "r"(d++));
	__asm __volatile("eieio; sync");
}

static __inline void
bus_space_set_region_4(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int32_t val, size_t count)
{
	volatile u_int32_t *d = __ppc_ba(tag, bsh, offset);

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
	volatile u_int16_t *d = __ppc_ba(tag, bsh, offset);

	while (count--)
		*d++ = val;
	__asm __volatile("eieio; sync");
}

static __inline void
bus_space_set_region_stream_4(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int32_t val, size_t count)
{
	volatile u_int32_t *d = __ppc_ba(tag, bsh, offset);

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

#define	BUS_SPACE_BARRIER_READ	0x01		/* force read barrier */
#define	BUS_SPACE_BARRIER_WRITE	0x02		/* force write barrier */

static __inline void
bus_space_barrier(bus_space_tag_t tag __unused,
    bus_space_handle_t bsh __unused, bus_size_t offset __unused,
    bus_size_t len __unused, int flags __unused)
{
	__asm __volatile("" : : : "memory");
}


#define	BUS_SPACE_ALIGNED_POINTER(p, t) ALIGNED_POINTER(p, t)

#include <machine/bus_dma.h>

#endif /* _MACPPC_BUS_H_ */
