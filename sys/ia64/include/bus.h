/*	$NetBSD: bus.h,v 1.12 1997/10/01 08:25:15 fvdl Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
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
 */
/* $FreeBSD$ */

#ifndef _MACHINE_BUS_H_
#define _MACHINE_BUS_H_

/*
 * Platform notes:
 * o	We don't use the _MACHINE_BUS_PIO_H_ and _MACHINE_BUS_MEMIO_H_
 *	macros to conditionally compile for I/O port, memory mapped I/O
 *	or both. It's a micro-optimization that is not worth the pain
 *	because there is no I/O port space. I/O ports are emulated by
 *	doing memory mapped I/O in a special memory range. The address
 *	translation is slightly magic for I/O port accesses, but it does
 *	not warrant the overhead.
 *
 */
#define	_MACHINE_BUS_MEMIO_H_
#define	_MACHINE_BUS_PIO_H_

#include <machine/cpufunc.h>

/*
 * Values for the ia64 bus space tag, not to be used directly by MI code.
 */
#define	IA64_BUS_SPACE_IO	0	/* space is i/o space */
#define IA64_BUS_SPACE_MEM	1	/* space is mem space */

/*
 * Bus address and size types
 */
typedef u_long bus_addr_t;
typedef u_long bus_size_t;

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
typedef	int bus_space_tag_t;
typedef	u_long bus_space_handle_t;


/*
 * Map a region of device bus space into CPU virtual address space.
 */
#define	BUS_SPACE_MAP_CACHEABLE		0x01
#define	BUS_SPACE_MAP_LINEAR		0x02

int
bus_space_map(bus_space_tag_t bst, bus_addr_t addr, bus_size_t size, int flags,
    bus_space_handle_t *bshp);


/*
 * Unmap a region of device bus space.
 */
static __inline void
bus_space_unmap(bus_space_tag_t bst __unused, bus_space_handle_t bsh __unused,
    bus_size_t size __unused)
{
}


/*
 * Get a new handle for a subregion of an already-mapped area of bus space.
 */
static __inline int
bus_space_subregion(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, bus_size_t size, bus_space_handle_t *nbshp)
{
	*nbshp = bsh + ofs;
	return (0);
}


/*
 * Allocate a region of memory that is accessible to devices in bus space.
 */
int
bus_space_alloc(bus_space_tag_t bst, bus_addr_t rstart, bus_addr_t rend,
    bus_size_t size, bus_size_t align, bus_size_t boundary, int flags,
    bus_addr_t *addrp, bus_space_handle_t *bshp);


/*
 * Free a region of bus space accessible memory.
 */
void
bus_space_free(bus_space_tag_t bst, bus_space_handle_t bsh, bus_size_t size);


/*
 * Bus read/write barrier method.
 */
#define	BUS_SPACE_BARRIER_READ	0x01		/* force read barrier */
#define	BUS_SPACE_BARRIER_WRITE	0x02		/* force write barrier */

static __inline void
bus_space_barrier(bus_space_tag_t bst, bus_space_handle_t bsh, bus_size_t ofs,
    bus_size_t size, int flags)
{
	ia64_mf_a();
	ia64_mf();
}


/*
 * Read 1 unit of data from bus space described by the tag, handle and ofs
 * tuple. A unit of data can be 1 byte, 2 bytes, 4 bytes or 8 bytes. The
 * data is returned.
 */
static __inline uint8_t
bus_space_read_1(bus_space_tag_t bst, bus_space_handle_t bsh, bus_size_t ofs)
{
	uint8_t __volatile *bsp;
	bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
	    __MEMIO_ADDR(bsh + ofs);
	return (*bsp);
}

static __inline uint16_t
bus_space_read_2(bus_space_tag_t bst, bus_space_handle_t bsh, bus_size_t ofs)
{
	uint16_t __volatile *bsp;
	bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
	    __MEMIO_ADDR(bsh + ofs);
	return (*bsp);
}

static __inline uint32_t
bus_space_read_4(bus_space_tag_t bst, bus_space_handle_t bsh, bus_size_t ofs)
{
	uint32_t __volatile *bsp;
	bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
	    __MEMIO_ADDR(bsh + ofs);
	return (*bsp);
}

static __inline uint64_t
bus_space_read_8(bus_space_tag_t bst, bus_space_handle_t bsh, bus_size_t ofs)
{
	uint64_t __volatile *bsp;
	bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
	    __MEMIO_ADDR(bsh + ofs);
	return (*bsp);
}


/*
 * Write 1 unit of data to bus space described by the tag, handle and ofs
 * tuple. A unit of data can be 1 byte, 2 bytes, 4 bytes or 8 bytes. The
 * data is passed by value.
 */
static __inline void
bus_space_write_1(bus_space_tag_t bst, bus_space_handle_t bsh, bus_size_t ofs,
    uint8_t val)
{
	uint8_t __volatile *bsp;
	bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
	    __MEMIO_ADDR(bsh + ofs);
	*bsp = val;
}

static __inline void
bus_space_write_2(bus_space_tag_t bst, bus_space_handle_t bsh, bus_size_t ofs,
    uint16_t val)
{
	uint16_t __volatile *bsp;
	bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
	    __MEMIO_ADDR(bsh + ofs);
	*bsp = val;
}

static __inline void
bus_space_write_4(bus_space_tag_t bst, bus_space_handle_t bsh, bus_size_t ofs,
    uint32_t val)
{
	uint32_t __volatile *bsp;
	bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
	    __MEMIO_ADDR(bsh + ofs);
	*bsp = val;
}

static __inline void
bus_space_write_8(bus_space_tag_t bst, bus_space_handle_t bsh, bus_size_t ofs,
    uint64_t val)
{
	uint64_t __volatile *bsp;
	bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
	    __MEMIO_ADDR(bsh + ofs);
	*bsp = val;
}


/*
 * Read count units of data from bus space described by the tag, handle and
 * ofs tuple. A unit of data can be 1 byte, 2 bytes, 4 bytes or 8 bytes. The
 * data is returned in the buffer passed by reference.
 */
static __inline void
bus_space_read_multi_1(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint8_t *bufp, size_t count)
{
	uint8_t __volatile *bsp;
	bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
	    __MEMIO_ADDR(bsh + ofs);
	while (count-- > 0)
		*bufp++ = *bsp;
}

static __inline void
bus_space_read_multi_2(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint16_t *bufp, size_t count)
{
	uint16_t __volatile *bsp;
	bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
	    __MEMIO_ADDR(bsh + ofs);
	while (count-- > 0)
		*bufp++ = *bsp;
}

static __inline void
bus_space_read_multi_4(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint32_t *bufp, size_t count)
{
	uint32_t __volatile *bsp;
	bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
	    __MEMIO_ADDR(bsh + ofs);
	while (count-- > 0)
		*bufp++ = *bsp;
}

static __inline void
bus_space_read_multi_8(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint64_t *bufp, size_t count)
{
	uint64_t __volatile *bsp;
	bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
	    __MEMIO_ADDR(bsh + ofs);
	while (count-- > 0)
		*bufp++ = *bsp;
}


/*
 * Write count units of data to bus space described by the tag, handle and
 * ofs tuple. A unit of data can be 1 byte, 2 bytes, 4 bytes or 8 bytes. The
 * data is read from the buffer passed by reference.
 */
static __inline void
bus_space_write_multi_1(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, const uint8_t *bufp, size_t count)
{
	uint8_t __volatile *bsp;
	bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
	    __MEMIO_ADDR(bsh + ofs);
	while (count-- > 0)
		*bsp = *bufp++;
}

static __inline void
bus_space_write_multi_2(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, const uint16_t *bufp, size_t count)
{
	uint16_t __volatile *bsp;
	bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
	    __MEMIO_ADDR(bsh + ofs);
	while (count-- > 0)
		*bsp = *bufp++;
}

static __inline void
bus_space_write_multi_4(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, const uint32_t *bufp, size_t count)
{
	uint32_t __volatile *bsp;
	bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
	    __MEMIO_ADDR(bsh + ofs);
	while (count-- > 0)
		*bsp = *bufp++;
}

static __inline void
bus_space_write_multi_8(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, const uint64_t *bufp, size_t count)
{
	uint64_t __volatile *bsp;
	bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
	    __MEMIO_ADDR(bsh + ofs);
	while (count-- > 0)
		*bsp = *bufp++;
}


/*
 * Read count units of data from bus space described by the tag, handle and
 * ofs tuple. A unit of data can be 1 byte, 2 bytes, 4 bytes or 8 bytes. The
 * data is written to the buffer passed by reference and read from successive
 * bus space addresses. Access is unordered.
 */
static __inline void
bus_space_read_region_1(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint8_t *bufp, size_t count)
{
	uint8_t __volatile *bsp;
	while (count-- > 0) {
		bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
		    __MEMIO_ADDR(bsh + ofs);
		*bufp++ = *bsp;
		ofs += 1;
	}
}

static __inline void
bus_space_read_region_2(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint16_t *bufp, size_t count)
{
	uint16_t __volatile *bsp;
	while (count-- > 0) {
		bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
		    __MEMIO_ADDR(bsh + ofs);
		*bufp++ = *bsp;
		ofs += 2;
	}
}

static __inline void
bus_space_read_region_4(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint32_t *bufp, size_t count)
{
	uint32_t __volatile *bsp;
	while (count-- > 0) {
		bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
		    __MEMIO_ADDR(bsh + ofs);
		*bufp++ = *bsp;
		ofs += 4;
	}
}

static __inline void
bus_space_read_region_8(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint64_t *bufp, size_t count)
{
	uint64_t __volatile *bsp;
	while (count-- > 0) {
		bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
		    __MEMIO_ADDR(bsh + ofs);
		*bufp++ = *bsp;
		ofs += 8;
	}
}


/*
 * Write count units of data from bus space described by the tag, handle and
 * ofs tuple. A unit of data can be 1 byte, 2 bytes, 4 bytes or 8 bytes. The
 * data is read from the buffer passed by reference and written to successive
 * bus space addresses. Access is unordered.
 */
static __inline void
bus_space_write_region_1(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, const uint8_t *bufp, size_t count)
{
	uint8_t __volatile *bsp;
	while (count-- > 0) {
		bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
		    __MEMIO_ADDR(bsh + ofs);
		*bsp = *bufp++;
		ofs += 1;
	}
}

static __inline void
bus_space_write_region_2(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, const uint16_t *bufp, size_t count)
{
	uint16_t __volatile *bsp;
	while (count-- > 0) {
		bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
		    __MEMIO_ADDR(bsh + ofs);
		*bsp = *bufp++;
		ofs += 2;
	}
}

static __inline void
bus_space_write_region_4(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, const uint32_t *bufp, size_t count)
{
	uint32_t __volatile *bsp;
	while (count-- > 0) {
		bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
		    __MEMIO_ADDR(bsh + ofs);
		*bsp = *bufp++;
		ofs += 4;
	}
}

static __inline void
bus_space_write_region_8(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, const uint64_t *bufp, size_t count)
{
	uint64_t __volatile *bsp;
	while (count-- > 0) {
		bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
		    __MEMIO_ADDR(bsh + ofs);
		*bsp = *bufp++;
		ofs += 8;
	}
}


/*
 * Write count units of data from bus space described by the tag, handle and
 * ofs tuple. A unit of data can be 1 byte, 2 bytes, 4 bytes or 8 bytes. The
 * data is passed by value. Writes are unordered.
 */
static __inline void
bus_space_set_multi_1(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint8_t val, size_t count)
{
	uint8_t __volatile *bsp;
	bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
	    __MEMIO_ADDR(bsh + ofs);
	while (count-- > 0)
		*bsp = val;
}

static __inline void
bus_space_set_multi_2(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint16_t val, size_t count)
{
	uint16_t __volatile *bsp;
	bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
	    __MEMIO_ADDR(bsh + ofs);
	while (count-- > 0)
		*bsp = val;
}

static __inline void
bus_space_set_multi_4(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint32_t val, size_t count)
{
	uint32_t __volatile *bsp;
	bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
	    __MEMIO_ADDR(bsh + ofs);
	while (count-- > 0)
		*bsp = val;
}

static __inline void
bus_space_set_multi_8(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint64_t val, size_t count)
{
	uint64_t __volatile *bsp;
	bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
	    __MEMIO_ADDR(bsh + ofs);
	while (count-- > 0)
		*bsp = val;
}


/*
 * Write count units of data from bus space described by the tag, handle and
 * ofs tuple. A unit of data can be 1 byte, 2 bytes, 4 bytes or 8 bytes. The
 * data is passed by value and written to successive bus space addresses.
 * Writes are unordered.
 */
static __inline void
bus_space_set_region_1(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint8_t val, size_t count)
{
	uint8_t __volatile *bsp;
	while (count-- > 0) {
		bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
		    __MEMIO_ADDR(bsh + ofs);
		*bsp = val;
		ofs += 1;
	}
}

static __inline void
bus_space_set_region_2(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint16_t val, size_t count)
{
	uint16_t __volatile *bsp;
	while (count-- > 0) {
		bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
		    __MEMIO_ADDR(bsh + ofs);
		*bsp = val;
		ofs += 2;
	}
}

static __inline void
bus_space_set_region_4(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint32_t val, size_t count)
{
	uint32_t __volatile *bsp;
	while (count-- > 0) {
		bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
		    __MEMIO_ADDR(bsh + ofs);
		*bsp = val;
		ofs += 4;
	}
}

static __inline void
bus_space_set_region_8(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint64_t val, size_t count)
{
	uint64_t __volatile *bsp;
	while (count-- > 0) {
		bsp = (bst == IA64_BUS_SPACE_IO) ? __PIO_ADDR(bsh + ofs) :
		    __MEMIO_ADDR(bsh + ofs);
		*bsp = val;
		ofs += 8;
	}
}


/*
 * Copy count units of data from bus space described by the tag and the first
 * handle and ofs pair to bus space described by the tag and the second handle
 * and ofs pair. A unit of data can be 1 byte, 2 bytes, 4 bytes or 8 bytes.
 * The data is read from successive bus space addresses and also written to
 * successive bus space addresses. Both reads and writes are unordered.
 */
static __inline void
bus_space_copy_region_1(bus_space_tag_t bst, bus_space_handle_t bsh1,
    bus_size_t ofs1, bus_space_handle_t bsh2, bus_size_t ofs2, size_t count)
{
	bus_addr_t dst, src;
	uint8_t __volatile *dstp, *srcp;
	src = bsh1 + ofs1;
	dst = bsh2 + ofs2;
	if (dst > src) {
		src += count - 1;
		dst += count - 1;
		while (count-- > 0) {
			if (bst == IA64_BUS_SPACE_IO) {
				srcp = __PIO_ADDR(src);
				dstp = __PIO_ADDR(dst);
			} else {
				srcp = __MEMIO_ADDR(src);
				dstp = __MEMIO_ADDR(dst);
			}
			*dstp = *srcp;
			src -= 1;
			dst -= 1;
		}
	} else {
		while (count-- > 0) {
			if (bst == IA64_BUS_SPACE_IO) {
				srcp = __PIO_ADDR(src);
				dstp = __PIO_ADDR(dst);
			} else {
				srcp = __MEMIO_ADDR(src);
				dstp = __MEMIO_ADDR(dst);
			}
			*dstp = *srcp;
			src += 1;
			dst += 1;
		}
	}
}

static __inline void
bus_space_copy_region_2(bus_space_tag_t bst, bus_space_handle_t bsh1,
    bus_size_t ofs1, bus_space_handle_t bsh2, bus_size_t ofs2, size_t count)
{
	bus_addr_t dst, src;
	uint16_t __volatile *dstp, *srcp;
	src = bsh1 + ofs1;
	dst = bsh2 + ofs2;
	if (dst > src) {
		src += (count - 1) << 1;
		dst += (count - 1) << 1;
		while (count-- > 0) {
			if (bst == IA64_BUS_SPACE_IO) {
				srcp = __PIO_ADDR(src);
				dstp = __PIO_ADDR(dst);
			} else {
				srcp = __MEMIO_ADDR(src);
				dstp = __MEMIO_ADDR(dst);
			}
			*dstp = *srcp;
			src -= 2;
			dst -= 2;
		}
	} else {
		while (count-- > 0) {
			if (bst == IA64_BUS_SPACE_IO) {
				srcp = __PIO_ADDR(src);
				dstp = __PIO_ADDR(dst);
			} else {
				srcp = __MEMIO_ADDR(src);
				dstp = __MEMIO_ADDR(dst);
			}
			*dstp = *srcp;
			src += 2;
			dst += 2;
		}
	}
}

static __inline void
bus_space_copy_region_4(bus_space_tag_t bst, bus_space_handle_t bsh1,
    bus_size_t ofs1, bus_space_handle_t bsh2, bus_size_t ofs2, size_t count)
{
	bus_addr_t dst, src;
	uint32_t __volatile *dstp, *srcp;
	src = bsh1 + ofs1;
	dst = bsh2 + ofs2;
	if (dst > src) {
		src += (count - 1) << 2;
		dst += (count - 1) << 2;
		while (count-- > 0) {
			if (bst == IA64_BUS_SPACE_IO) {
				srcp = __PIO_ADDR(src);
				dstp = __PIO_ADDR(dst);
			} else {
				srcp = __MEMIO_ADDR(src);
				dstp = __MEMIO_ADDR(dst);
			}
			*dstp = *srcp;
			src -= 4;
			dst -= 4;
		}
	} else {
		while (count-- > 0) {
			if (bst == IA64_BUS_SPACE_IO) {
				srcp = __PIO_ADDR(src);
				dstp = __PIO_ADDR(dst);
			} else {
				srcp = __MEMIO_ADDR(src);
				dstp = __MEMIO_ADDR(dst);
			}
			*dstp = *srcp;
			src += 4;
			dst += 4;
		}
	}
}

static __inline void
bus_space_copy_region_8(bus_space_tag_t bst, bus_space_handle_t bsh1,
    bus_size_t ofs1, bus_space_handle_t bsh2, bus_size_t ofs2, size_t count)
{
	bus_addr_t dst, src;
	uint64_t __volatile *dstp, *srcp;
	src = bsh1 + ofs1;
	dst = bsh2 + ofs2;
	if (dst > src) {
		src += (count - 1) << 3;
		dst += (count - 1) << 3;
		while (count-- > 0) {
			if (bst == IA64_BUS_SPACE_IO) {
				srcp = __PIO_ADDR(src);
				dstp = __PIO_ADDR(dst);
			} else {
				srcp = __MEMIO_ADDR(src);
				dstp = __MEMIO_ADDR(dst);
			}
			*dstp = *srcp;
			src -= 8;
			dst -= 8;
		}
	} else {
		while (count-- > 0) {
			if (bst == IA64_BUS_SPACE_IO) {
				srcp = __PIO_ADDR(src);
				dstp = __PIO_ADDR(dst);
			} else {
				srcp = __MEMIO_ADDR(src);
				dstp = __MEMIO_ADDR(dst);
			}
			*dstp = *srcp;
			src += 8;
			dst += 8;
		}
	}
}


/*
 * Stream accesses are the same as normal accesses on ia64; there are no
 * supported bus systems with an endianess different from the host one.
 */
#define	bus_space_read_stream_1(t, h, o)	\
	bus_space_read_1(t, h, o)
#define	bus_space_read_stream_2(t, h, o)	\
	bus_space_read_2(t, h, o)
#define	bus_space_read_stream_4(t, h, o)	\
	bus_space_read_4(t, h, o)
#define	bus_space_read_stream_8(t, h, o)	\
	bus_space_read_8(t, h, o)

#define	bus_space_read_multi_stream_1(t, h, o, a, c)	\
	bus_space_read_multi_1(t, h, o, a, c)
#define	bus_space_read_multi_stream_2(t, h, o, a, c)	\
	bus_space_read_multi_2(t, h, o, a, c)
#define	bus_space_read_multi_stream_4(t, h, o, a, c)	\
	bus_space_read_multi_4(t, h, o, a, c)
#define	bus_space_read_multi_stream_8(t, h, o, a, c)	\
	bus_space_read_multi_8(t, h, o, a, c)

#define	bus_space_write_stream_1(t, h, o, v)	\
	bus_space_write_1(t, h, o, v)
#define	bus_space_write_stream_2(t, h, o, v)	\
	bus_space_write_2(t, h, o, v)
#define	bus_space_write_stream_4(t, h, o, v)	\
	bus_space_write_4(t, h, o, v)
#define	bus_space_write_stream_8(t, h, o, v)	\
	bus_space_write_8(t, h, o, v)

#define	bus_space_write_multi_stream_1(t, h, o, a, c)	\
	bus_space_write_multi_1(t, h, o, a, c)
#define	bus_space_write_multi_stream_2(t, h, o, a, c)	\
	bus_space_write_multi_2(t, h, o, a, c)
#define	bus_space_write_multi_stream_4(t, h, o, a, c)	\
	bus_space_write_multi_4(t, h, o, a, c)
#define	bus_space_write_multi_stream_8(t, h, o, a, c)	\
	bus_space_write_multi_8(t, h, o, a, c)

#define	bus_space_set_multi_stream_1(t, h, o, v, c)	\
	bus_space_set_multi_1(t, h, o, v, c)
#define	bus_space_set_multi_stream_2(t, h, o, v, c)	\
	bus_space_set_multi_2(t, h, o, v, c)
#define	bus_space_set_multi_stream_4(t, h, o, v, c)	\
	bus_space_set_multi_4(t, h, o, v, c)
#define	bus_space_set_multi_stream_8(t, h, o, v, c)	\
	bus_space_set_multi_8(t, h, o, v, c)

#define	bus_space_read_region_stream_1(t, h, o, a, c)	\
	bus_space_read_region_1(t, h, o, a, c)
#define	bus_space_read_region_stream_2(t, h, o, a, c)	\
	bus_space_read_region_2(t, h, o, a, c)
#define	bus_space_read_region_stream_4(t, h, o, a, c)	\
	bus_space_read_region_4(t, h, o, a, c)
#define	bus_space_read_region_stream_8(t, h, o, a, c)	\
	bus_space_read_region_8(t, h, o, a, c)

#define	bus_space_write_region_stream_1(t, h, o, a, c)	\
	bus_space_write_region_1(t, h, o, a, c)
#define	bus_space_write_region_stream_2(t, h, o, a, c)	\
	bus_space_write_region_2(t, h, o, a, c)
#define	bus_space_write_region_stream_4(t, h, o, a, c)	\
	bus_space_write_region_4(t, h, o, a, c)
#define	bus_space_write_region_stream_8(t, h, o, a, c)	\
	bus_space_write_region_8(t, h, o, a, c)

#define	bus_space_set_region_stream_1(t, h, o, v, c)	\
	bus_space_set_region_1(t, h, o, v, c)
#define	bus_space_set_region_stream_2(t, h, o, v, c)	\
	bus_space_set_region_2(t, h, o, v, c)
#define	bus_space_set_region_stream_4(t, h, o, v, c)	\
	bus_space_set_region_4(t, h, o, v, c)
#define	bus_space_set_region_stream_8(t, h, o, v, c)	\
	bus_space_set_region_8(t, h, o, v, c)

#define	bus_space_copy_region_stream_1(t, h1, o1, h2, o2, c)	\
	bus_space_copy_region_1(t, h1, o1, h2, o2, c)
#define	bus_space_copy_region_stream_2(t, h1, o1, h2, o2, c)	\
	bus_space_copy_region_2(t, h1, o1, h2, o2, c)
#define	bus_space_copy_region_stream_4(t, h1, o1, h2, o2, c)	\
	bus_space_copy_region_4(t, h1, o1, h2, o2, c)
#define	bus_space_copy_region_stream_8(t, h1, o1, h2, o2, c)	\
	bus_space_copy_region_8(t, h1, o1, h2, o2, c)


/*
 * Flags used in various bus DMA methods.
 */
#define	BUS_DMA_WAITOK		0x00	/* safe to sleep (pseudo-flag) */
#define	BUS_DMA_NOWAIT		0x01	/* not safe to sleep */
#define	BUS_DMA_ALLOCNOW	0x02	/* perform resource allocation now */
#define	BUS_DMA_COHERENT	0x04	/* hint: map memory in a coherent way */
#define	BUS_DMA_ISA		0x10	/* map memory for ISA dma */
#define	BUS_DMA_BUS2		0x20	/* placeholders for bus functions... */
#define	BUS_DMA_BUS3		0x40
#define	BUS_DMA_BUS4		0x80

/* Forwards needed by prototypes below. */
struct mbuf;
struct uio;

/*
 *	Operations performed by bus_dmamap_sync().
 */
typedef int bus_dmasync_op_t;
#define	BUS_DMASYNC_PREREAD	1
#define	BUS_DMASYNC_POSTREAD	2
#define	BUS_DMASYNC_PREWRITE	4
#define	BUS_DMASYNC_POSTWRITE	8

/*
 *	bus_dma_tag_t
 *
 *	A machine-dependent opaque type describing the characteristics
 *	of how to perform DMA mappings.  This structure encapsultes
 *	information concerning address and alignment restrictions, number
 *	of S/G	segments, amount of data per S/G segment, etc.
 */
typedef struct bus_dma_tag *bus_dma_tag_t;

/*
 *	bus_dmamap_t
 *
 *	DMA mapping instance information.
 */
typedef struct bus_dmamap *bus_dmamap_t;

/*
 *	bus_dma_segment_t
 *
 *	Describes a single contiguous DMA transaction.  Values
 *	are suitable for programming into DMA registers.
 */
typedef struct bus_dma_segment {
	bus_addr_t	ds_addr;	/* DMA address */
	bus_size_t	ds_len;		/* length of transfer */
} bus_dma_segment_t;

/*
 * A function that returns 1 if the address cannot be accessed by
 * a device and 0 if it can be.
 */
typedef int bus_dma_filter_t(void *, bus_addr_t);

/*
 * Allocate a device specific dma_tag encapsulating the constraints of
 * the parent tag in addition to other restrictions specified:
 *
 *	alignment:	alignment for segments.
 *	boundary:	Boundary that segments cannot cross.
 *	lowaddr:	Low restricted address that cannot appear in a mapping.
 *	highaddr:	High restricted addr. that cannot appear in a mapping.
 *	filtfunc:	An optional function to further test if an address
 *			within the range of lowaddr and highaddr cannot appear
 *			in a mapping.
 *	filtfuncarg:	An argument that will be passed to filtfunc in addition
 *			to the address to test.
 *	maxsize:	Maximum mapping size supported by this tag.
 *	nsegments:	Number of discontinuities allowed in maps.
 *	maxsegsz:	Maximum size of a segment in the map.
 *	flags:		Bus DMA flags.
 *	dmat:		A pointer to set to a valid dma tag should the return
 *			value of this function indicate success.
 */
/* XXX Should probably allow specification of alignment */
int bus_dma_tag_create(bus_dma_tag_t parent, bus_size_t alignemnt,
    bus_size_t boundary, bus_addr_t lowaddr, bus_addr_t highaddr,
    bus_dma_filter_t *filtfunc, void *filtfuncarg, bus_size_t maxsize,
    int nsegments, bus_size_t maxsegsz, int flags, bus_dma_tag_t *dmat);

int bus_dma_tag_destroy(bus_dma_tag_t dmat);

/*
 * Allocate a handle for mapping from kva/uva/physical
 * address space into bus device space.
 */
int bus_dmamap_create(bus_dma_tag_t dmat, int flags, bus_dmamap_t *mapp);

/*
 * Destroy  a handle for mapping from kva/uva/physical
 * address space into bus device space.
 */
int bus_dmamap_destroy(bus_dma_tag_t dmat, bus_dmamap_t map);

/*
 * Allocate a piece of memory that can be efficiently mapped into
 * bus device space based on the constraints lited in the dma tag.
 * A dmamap to for use with dmamap_load is also allocated.
 */
int bus_dmamem_alloc(bus_dma_tag_t dmat, void** vaddr, int flags,
    bus_dmamap_t *mapp);

/*
 * Free a piece of memory and it's allociated dmamap, that was allocated
 * via bus_dmamem_alloc.
 */
void bus_dmamem_free(bus_dma_tag_t dmat, void *vaddr, bus_dmamap_t map);

/*
 * A function that processes a successfully loaded dma map or an error
 * from a delayed load map.
 */
typedef void bus_dmamap_callback_t(void *, bus_dma_segment_t *, int, int);

/*
 * Map the buffer buf into bus space using the dmamap map.
 */
int bus_dmamap_load(bus_dma_tag_t dmat, bus_dmamap_t map, void *buf,
    bus_size_t buflen, bus_dmamap_callback_t *callback, void *callback_arg,
    int flags);

/*
 * Like bus_dmamap_callback but includes map size in bytes.  This is
 * defined as a separate interface to maintain compatiiblity for users
 * of bus_dmamap_callback_t--at some point these interfaces should be merged.
 */
typedef void bus_dmamap_callback2_t(void *, bus_dma_segment_t *, int,
    bus_size_t, int);

/*
 * Like bus_dmamap_load but for mbufs.  Note the use of the
 * bus_dmamap_callback2_t interface.
 */
int bus_dmamap_load_mbuf(bus_dma_tag_t dmat, bus_dmamap_t map,
    struct mbuf *mbuf, bus_dmamap_callback2_t *callback, void *callback_arg,
    int flags);

/*
 * Like bus_dmamap_load but for uios.  Note the use of the
 * bus_dmamap_callback2_t interface.
 */
int bus_dmamap_load_uio(bus_dma_tag_t dmat, bus_dmamap_t map, struct uio *ui,
    bus_dmamap_callback2_t *callback, void *callback_arg, int flags);

/*
 * Perform a syncronization operation on the given map.
 */
void _bus_dmamap_sync(bus_dma_tag_t, bus_dmamap_t, int);
static __inline void
bus_dmamap_sync(bus_dma_tag_t dmat, bus_dmamap_t dmamap, bus_dmasync_op_t op)
{
	if ((dmamap) != NULL)
		_bus_dmamap_sync(dmat, dmamap, op);
}

/*
 * Release the mapping held by map.
 */
void _bus_dmamap_unload(bus_dma_tag_t dmat, bus_dmamap_t map);
static __inline void
bus_dmamap_unload(bus_dma_tag_t dmat, bus_dmamap_t dmamap)
{
	if ((dmamap) != NULL)
		_bus_dmamap_unload(dmat, dmamap);
}

#endif /* _MACHINE_BUS_H_ */
