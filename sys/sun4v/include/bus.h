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
 * 	from: NetBSD: bus.h,v 1.28 2001/07/19 15:32:19 thorpej Exp
 *	and
 *	from: FreeBSD: src/sys/alpha/include/bus.h,v 1.9 2001/01/09
 *
 * $FreeBSD: src/sys/sun4v/include/bus.h,v 1.3.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef	_MACHINE_BUS_H_
#define	_MACHINE_BUS_H_

#ifdef BUS_SPACE_DEBUG
#include <sys/ktr.h>
#endif

#include <machine/_bus.h>
#include <machine/cpufunc.h>
#include <machine/upa.h>

/*
 * Nexus and SBus spaces are non-cached and big endian
 * (except for RAM and PROM)
 *
 * PCI spaces are non-cached and little endian
 */
#define	NEXUS_BUS_SPACE		0
#define	SBUS_BUS_SPACE		1
#define	PCI_CONFIG_BUS_SPACE	2
#define	PCI_IO_BUS_SPACE	3
#define	PCI_MEMORY_BUS_SPACE	4
#define	LAST_BUS_SPACE		5

extern int bus_type_asi[];
extern int bus_stream_asi[];

#define __BUS_SPACE_HAS_STREAM_METHODS	1

#define BUS_SPACE_MAXSIZE_24BIT	0xFFFFFF
#define BUS_SPACE_MAXSIZE_32BIT 0xFFFFFFFF
#define BUS_SPACE_MAXSIZE	0xFFFFFFFFFFFFFFFF
#define BUS_SPACE_MAXADDR_24BIT	0xFFFFFF
#define BUS_SPACE_MAXADDR_32BIT 0xFFFFFFFF
#define BUS_SPACE_MAXADDR	0xFFFFFFFF

#define BUS_SPACE_UNRESTRICTED	(~0)

/*
 * Access methods for bus resources and address space.
 */
struct bus_space_tag {
	void		*bst_cookie;
	bus_space_tag_t	bst_parent;
	int		bst_type;

	void		(*bst_bus_barrier)(bus_space_tag_t, bus_space_handle_t,
	    bus_size_t, bus_size_t, int);
};

/*
 * Bus space function prototypes.
 */
static void bus_space_barrier(bus_space_tag_t, bus_space_handle_t, bus_size_t,
    bus_size_t, int);
static int bus_space_subregion(bus_space_tag_t, bus_space_handle_t,
    bus_size_t, bus_size_t, bus_space_handle_t *);

/*
 * Map a region of device bus space into CPU virtual address space.
 */

static __inline int bus_space_map(bus_space_tag_t t, bus_addr_t addr,
				  bus_size_t size, int flags,
				  bus_space_handle_t *bshp);

static __inline int
bus_space_map(bus_space_tag_t t __unused, bus_addr_t addr,
	      bus_size_t size __unused, int flags __unused,
	      bus_space_handle_t *bshp)
{

	*bshp = addr;
	return (0);
}

/*
 * Unmap a region of device bus space.
 */
static __inline void bus_space_unmap(bus_space_tag_t t, bus_space_handle_t bsh,
				     bus_size_t size);

static __inline void
bus_space_unmap(bus_space_tag_t t __unused, bus_space_handle_t bsh __unused,
		bus_size_t size __unused)
{
}

/* This macro finds the first "upstream" implementation of method `f' */
#define _BS_CALL(t,f)							\
	while (t->f == NULL)						\
		t = t->bst_parent;						\
	return (*(t)->f)

static __inline void
bus_space_barrier(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    bus_size_t s, int f)
{
	_BS_CALL(t, bst_bus_barrier)(t, h, o, s, f);
}

static __inline int
bus_space_subregion(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    bus_size_t s, bus_space_handle_t *hp)
{
	*hp = h + o;
	return (0);
}

/* flags for bus space map functions */
#define BUS_SPACE_MAP_CACHEABLE		0x0001
#define BUS_SPACE_MAP_LINEAR		0x0002
#define BUS_SPACE_MAP_READONLY		0x0004
#define BUS_SPACE_MAP_PREFETCHABLE	0x0008
/* placeholders for bus functions... */
#define BUS_SPACE_MAP_BUS1		0x0100
#define BUS_SPACE_MAP_BUS2		0x0200
#define BUS_SPACE_MAP_BUS3		0x0400
#define BUS_SPACE_MAP_BUS4		0x0800

/* flags for bus_space_barrier() */
#define	BUS_SPACE_BARRIER_READ		0x01	/* force read barrier */
#define	BUS_SPACE_BARRIER_WRITE		0x02	/* force write barrier */

#ifdef BUS_SPACE_DEBUG
#define	KTR_BUS				KTR_CT2
#define	BUS_HANDLE_MIN			UPA_MEMSTART
#define	__BUS_DEBUG_ACCESS(h, o, desc, sz) do {				\
	CTR4(KTR_BUS, "bus space: %s %d: handle %#lx, offset %#lx",	\
	    (desc), (sz), (h), (o));					\
	if ((h) + (o) < BUS_HANDLE_MIN)					\
		panic("bus space access at %#lx out of range",		\
		    (h) + (o));						\
} while (0)
#else
#define	__BUS_DEBUG_ACCESS(h, o, desc, sz)
#endif

static __inline uint8_t
bus_space_read_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{

	__BUS_DEBUG_ACCESS(h, o, "read", 1);
	return (lduba_nc((caddr_t)(h + o), bus_type_asi[t->bst_type]));
}

static __inline uint16_t
bus_space_read_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{

	__BUS_DEBUG_ACCESS(h, o, "read", 2);
	return (lduha_nc((caddr_t)(h + o), bus_type_asi[t->bst_type]));
}

static __inline uint32_t
bus_space_read_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{

	__BUS_DEBUG_ACCESS(h, o, "read", 4);
	return (lduwa_nc((caddr_t)(h + o), bus_type_asi[t->bst_type]));
}

static __inline uint64_t
bus_space_read_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{

	__BUS_DEBUG_ACCESS(h, o, "read", 8);
	return (ldxa_nc((caddr_t)(h + o), bus_type_asi[t->bst_type]));
}

static __inline void
bus_space_read_multi_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint8_t *a, size_t c)
{

	while (c-- > 0)
		*a++ = bus_space_read_1(t, h, o);
}

static __inline void
bus_space_read_multi_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint16_t *a, size_t c)
{

	while (c-- > 0)
		*a++ = bus_space_read_2(t, h, o);
}

static __inline void
bus_space_read_multi_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint32_t *a, size_t c)
{

	while (c-- > 0)
		*a++ = bus_space_read_4(t, h, o);
}

static __inline void
bus_space_read_multi_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint64_t *a, size_t c)
{

	while (c-- > 0)
		*a++ = bus_space_read_8(t, h, o);
}

static __inline void
bus_space_write_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint8_t v)
{

	__BUS_DEBUG_ACCESS(h, o, "write", 1);
	stba_nc((caddr_t)(h + o), bus_type_asi[t->bst_type], v);
}

static __inline void
bus_space_write_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint16_t v)
{

	__BUS_DEBUG_ACCESS(h, o, "write", 2);
	stha_nc((caddr_t)(h + o), bus_type_asi[t->bst_type], v);
}

static __inline void
bus_space_write_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint32_t v)
{

	__BUS_DEBUG_ACCESS(h, o, "write", 4);
	stwa_nc((caddr_t)(h + o), bus_type_asi[t->bst_type], v);
}

static __inline void
bus_space_write_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint64_t v)
{

	__BUS_DEBUG_ACCESS(h, o, "write", 8);
	stxa_nc((caddr_t)(h + o), bus_type_asi[t->bst_type], v);
}

static __inline void
bus_space_write_multi_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint8_t *a, size_t c)
{

	while (c-- > 0)
		bus_space_write_1(t, h, o, *a++);
}

static __inline void
bus_space_write_multi_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint16_t *a, size_t c)
{

	while (c-- > 0)
		bus_space_write_2(t, h, o, *a++);
}

static __inline void
bus_space_write_multi_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint32_t *a, size_t c)
{

	while (c-- > 0)
		bus_space_write_4(t, h, o, *a++);
}

static __inline void
bus_space_write_multi_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint64_t *a, size_t c)
{

	while (c-- > 0)
		bus_space_write_8(t, h, o, *a++);
}

static __inline void
bus_space_set_multi_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint8_t v, size_t c)
{

	while (c-- > 0)
		bus_space_write_1(t, h, o, v);
}

static __inline void
bus_space_set_multi_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint16_t v, size_t c)
{

	while (c-- > 0)
		bus_space_write_2(t, h, o, v);
}

static __inline void
bus_space_set_multi_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint32_t v, size_t c)
{

	while (c-- > 0)
		bus_space_write_4(t, h, o, v);
}

static __inline void
bus_space_set_multi_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint64_t v, size_t c)
{

	while (c-- > 0)
		bus_space_write_8(t, h, o, v);
}

static __inline void
bus_space_read_region_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    u_int8_t *a, bus_size_t c)
{
	for (; c; a++, c--, o++)
		*a = bus_space_read_1(t, h, o);
}

static __inline void
bus_space_read_region_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    u_int16_t *a, bus_size_t c)
{
	for (; c; a++, c--, o+=2)
		*a = bus_space_read_2(t, h, o);
}

static __inline void
bus_space_read_region_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    u_int32_t *a, bus_size_t c)
{
	for (; c; a++, c--, o+=4)
		*a = bus_space_read_4(t, h, o);
}

static __inline void
bus_space_read_region_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    u_int64_t *a, bus_size_t c)
{
	for (; c; a++, c--, o+=8)
		*a = bus_space_read_8(t, h, o);
}

static __inline void
bus_space_write_region_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    const u_int8_t *a, bus_size_t c)
{
	for (; c; a++, c--, o++)
		bus_space_write_1(t, h, o, *a);
}

static __inline void
bus_space_write_region_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    const u_int16_t *a, bus_size_t c)
{
	for (; c; a++, c--, o+=2)
		bus_space_write_2(t, h, o, *a);
}

static __inline void
bus_space_write_region_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    const u_int32_t *a, bus_size_t c)
{
	for (; c; a++, c--, o+=4)
		bus_space_write_4(t, h, o, *a);
}

static __inline void
bus_space_write_region_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    const u_int64_t *a, bus_size_t c)
{
	for (; c; a++, c--, o+=8)
		bus_space_write_8(t, h, o, *a);
}

static __inline void
bus_space_set_region_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    const u_int8_t v, bus_size_t c)
{
	for (; c; c--, o++)
		bus_space_write_1(t, h, o, v);
}

static __inline void
bus_space_set_region_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    const u_int16_t v, bus_size_t c)
{
	for (; c; c--, o+=2)
		bus_space_write_2(t, h, o, v);
}

static __inline void
bus_space_set_region_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    const u_int32_t v, bus_size_t c)
{
	for (; c; c--, o+=4)
		bus_space_write_4(t, h, o, v);
}

static __inline void
bus_space_set_region_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    const u_int64_t v, bus_size_t c)
{
	for (; c; c--, o+=8)
		bus_space_write_8(t, h, o, v);
}

static __inline void
bus_space_copy_region_1(bus_space_tag_t t, bus_space_handle_t h1,
    bus_size_t o1, bus_space_handle_t h2, bus_size_t o2, bus_size_t c)
{
	for (; c; c--, o1++, o2++)
	    bus_space_write_1(t, h1, o1, bus_space_read_1(t, h2, o2));
}

static __inline void
bus_space_copy_region_2(bus_space_tag_t t, bus_space_handle_t h1,
    bus_size_t o1, bus_space_handle_t h2, bus_size_t o2, bus_size_t c)
{
	for (; c; c--, o1+=2, o2+=2)
	    bus_space_write_2(t, h1, o1, bus_space_read_2(t, h2, o2));
}

static __inline void
bus_space_copy_region_4(bus_space_tag_t t, bus_space_handle_t h1,
    bus_size_t o1, bus_space_handle_t h2, bus_size_t o2, bus_size_t c)
{
	for (; c; c--, o1+=4, o2+=4)
	    bus_space_write_4(t, h1, o1, bus_space_read_4(t, h2, o2));
}

static __inline void
bus_space_copy_region_8(bus_space_tag_t t, bus_space_handle_t h1,
    bus_size_t o1, bus_space_handle_t h2, bus_size_t o2, bus_size_t c)
{
	for (; c; c--, o1+=8, o2+=8)
	    bus_space_write_8(t, h1, o1, bus_space_read_8(t, h2, o2));
}

static __inline uint8_t
bus_space_read_stream_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{

	__BUS_DEBUG_ACCESS(h, o, "read stream", 1);
	return (lduba_nc((caddr_t)(h + o), bus_stream_asi[t->bst_type]));
}

static __inline uint16_t
bus_space_read_stream_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{

	__BUS_DEBUG_ACCESS(h, o, "read stream", 2);
	return (lduha_nc((caddr_t)(h + o), bus_stream_asi[t->bst_type]));
}

static __inline uint32_t
bus_space_read_stream_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{

	__BUS_DEBUG_ACCESS(h, o, "read stream", 4);
	return (lduwa_nc((caddr_t)(h + o), bus_stream_asi[t->bst_type]));
}

static __inline uint64_t
bus_space_read_stream_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{

	__BUS_DEBUG_ACCESS(h, o, "read stream", 8);
	return (ldxa_nc((caddr_t)(h + o), bus_stream_asi[t->bst_type]));
}

static __inline void
bus_space_read_multi_stream_1(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, uint8_t *a, size_t c)
{

	while (c-- > 0)
		*a++ = bus_space_read_stream_1(t, h, o);
}

static __inline void
bus_space_read_multi_stream_2(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, uint16_t *a, size_t c)
{

	while (c-- > 0)
		*a++ = bus_space_read_stream_2(t, h, o);
}

static __inline void
bus_space_read_multi_stream_4(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, uint32_t *a, size_t c)
{

	while (c-- > 0)
		*a++ = bus_space_read_stream_4(t, h, o);
}

static __inline void
bus_space_read_multi_stream_8(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, uint64_t *a, size_t c)
{

	while (c-- > 0)
		*a++ = bus_space_read_stream_8(t, h, o);
}

static __inline void
bus_space_write_stream_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint8_t v)
{

	__BUS_DEBUG_ACCESS(h, o, "write stream", 1);
	stba_nc((caddr_t)(h + o), bus_stream_asi[t->bst_type], v);
}

static __inline void
bus_space_write_stream_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint16_t v)
{

	__BUS_DEBUG_ACCESS(h, o, "write stream", 2);
	stha_nc((caddr_t)(h + o), bus_stream_asi[t->bst_type], v);
}

static __inline void
bus_space_write_stream_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint32_t v)
{

	__BUS_DEBUG_ACCESS(h, o, "write stream", 4);
	stwa_nc((caddr_t)(h + o), bus_stream_asi[t->bst_type], v);
}

static __inline void
bus_space_write_stream_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint64_t v)
{

	__BUS_DEBUG_ACCESS(h, o, "write stream", 8);
	stxa_nc((caddr_t)(h + o), bus_stream_asi[t->bst_type], v);
}

static __inline void
bus_space_write_multi_stream_1(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, const uint8_t *a, size_t c)
{

	while (c-- > 0)
		bus_space_write_stream_1(t, h, o, *a++);
}

static __inline void
bus_space_write_multi_stream_2(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, const uint16_t *a, size_t c)
{

	while (c-- > 0)
		bus_space_write_stream_2(t, h, o, *a++);
}

static __inline void
bus_space_write_multi_stream_4(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, const uint32_t *a, size_t c)
{

	while (c-- > 0)
		bus_space_write_stream_4(t, h, o, *a++);
}

static __inline void
bus_space_write_multi_stream_8(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, const uint64_t *a, size_t c)
{

	while (c-- > 0)
		bus_space_write_stream_8(t, h, o, *a++);
}

static __inline void
bus_space_set_multi_stream_1(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, uint8_t v, size_t c)
{

	while (c-- > 0)
		bus_space_write_stream_1(t, h, o, v);
}

static __inline void
bus_space_set_multi_stream_2(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, uint16_t v, size_t c)
{

	while (c-- > 0)
		bus_space_write_stream_2(t, h, o, v);
}

static __inline void
bus_space_set_multi_stream_4(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, uint32_t v, size_t c)
{

	while (c-- > 0)
		bus_space_write_stream_4(t, h, o, v);
}

static __inline void
bus_space_set_multi_stream_8(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, uint64_t v, size_t c)
{

	while (c-- > 0)
		bus_space_write_stream_8(t, h, o, v);
}

static __inline void
bus_space_read_region_stream_1(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, u_int8_t *a, bus_size_t c)
{

	for (; c; a++, c--, o++)
		*a = bus_space_read_stream_1(t, h, o);
}

static __inline void
bus_space_read_region_stream_2(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, u_int16_t *a, bus_size_t c)
{

	for (; c; a++, c--, o+=2)
		*a = bus_space_read_stream_2(t, h, o);
}

static __inline void
bus_space_read_region_stream_4(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, u_int32_t *a, bus_size_t c)
{

	for (; c; a++, c--, o+=4)
		*a = bus_space_read_stream_4(t, h, o);
}

static __inline void
bus_space_read_region_stream_8(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, u_int64_t *a, bus_size_t c)
{

	for (; c; a++, c--, o+=8)
		*a = bus_space_read_stream_8(t, h, o);
}

static __inline void
bus_space_write_region_stream_1(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, const u_int8_t *a, bus_size_t c)
{

	for (; c; a++, c--, o++)
		bus_space_write_stream_1(t, h, o, *a);
}

static __inline void
bus_space_write_region_stream_2(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, const u_int16_t *a, bus_size_t c)
{

	for (; c; a++, c--, o+=2)
		bus_space_write_stream_2(t, h, o, *a);
}

static __inline void
bus_space_write_region_stream_4(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, const u_int32_t *a, bus_size_t c)
{

	for (; c; a++, c--, o+=4)
		bus_space_write_stream_4(t, h, o, *a);
}

static __inline void
bus_space_write_region_stream_8(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, const u_int64_t *a, bus_size_t c)
{

	for (; c; a++, c--, o+=8)
		bus_space_write_stream_8(t, h, o, *a);
}

static __inline void
bus_space_set_region_stream_1(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, const u_int8_t v, bus_size_t c)
{

	for (; c; c--, o++)
		bus_space_write_stream_1(t, h, o, v);
}

static __inline void
bus_space_set_region_stream_2(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, const u_int16_t v, bus_size_t c)
{

	for (; c; c--, o+=2)
		bus_space_write_stream_2(t, h, o, v);
}

static __inline void
bus_space_set_region_stream_4(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, const u_int32_t v, bus_size_t c)
{

	for (; c; c--, o+=4)
		bus_space_write_stream_4(t, h, o, v);
}

static __inline void
bus_space_set_region_stream_8(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t o, const u_int64_t v, bus_size_t c)
{

	for (; c; c--, o+=8)
		bus_space_write_stream_8(t, h, o, v);
}

static __inline void
bus_space_copy_region_stream_1(bus_space_tag_t t, bus_space_handle_t h1,
    bus_size_t o1, bus_space_handle_t h2, bus_size_t o2, bus_size_t c)
{

	for (; c; c--, o1++, o2++)
	    bus_space_write_stream_1(t, h1, o1, bus_space_read_stream_1(t, h2,
		o2));
}

static __inline void
bus_space_copy_region_stream_2(bus_space_tag_t t, bus_space_handle_t h1,
    bus_size_t o1, bus_space_handle_t h2, bus_size_t o2, bus_size_t c)
{

	for (; c; c--, o1+=2, o2+=2)
	    bus_space_write_stream_2(t, h1, o1, bus_space_read_stream_2(t, h2,
		o2));
}

static __inline void
bus_space_copy_region_stream_4(bus_space_tag_t t, bus_space_handle_t h1,
    bus_size_t o1, bus_space_handle_t h2, bus_size_t o2, bus_size_t c)
{

	for (; c; c--, o1+=4, o2+=4)
	    bus_space_write_stream_4(t, h1, o1, bus_space_read_stream_4(t, h2,
		o2));
}

static __inline void
bus_space_copy_region_stream_8(bus_space_tag_t t, bus_space_handle_t h1,
    bus_size_t o1, bus_space_handle_t h2, bus_size_t o2, bus_size_t c)
{

	for (; c; c--, o1+=8, o2+=8)
	    bus_space_write_stream_8(t, h1, o1, bus_space_read_8(t, h2, o2));
}

#include <machine/bus_dma.h>

#endif /* !_MACHINE_BUS_H_ */
