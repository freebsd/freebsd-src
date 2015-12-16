/*-
 * Copyright (c) 2009 Marcel Moolenaar
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
 */
/* $FreeBSD$ */

#ifndef _MACHINE_BUS_H_
#define _MACHINE_BUS_H_

#include <machine/_bus.h>
#include <machine/cpufunc.h>

/*
 * I/O port reads with ia32 semantics.
 */
#define inb     bus_space_read_io_1
#define inw     bus_space_read_io_2
#define inl     bus_space_read_io_4

#define outb    bus_space_write_io_1
#define outw    bus_space_write_io_2
#define outl    bus_space_write_io_4

/*
 * Values for the ia64 bus space tag, not to be used directly by MI code.
 */
#define	IA64_BUS_SPACE_IO	0	/* space is i/o space */
#define IA64_BUS_SPACE_MEM	1	/* space is mem space */

#define	BUS_SPACE_BARRIER_READ	0x01	/* force read barrier */
#define	BUS_SPACE_BARRIER_WRITE	0x02	/* force write barrier */

#define BUS_SPACE_MAXSIZE_24BIT	0xFFFFFF
#define BUS_SPACE_MAXSIZE_32BIT 0xFFFFFFFF
#define BUS_SPACE_MAXSIZE	0xFFFFFFFFFFFFFFFF
#define BUS_SPACE_MAXADDR_24BIT	0xFFFFFF
#define BUS_SPACE_MAXADDR_32BIT 0xFFFFFFFF
#define BUS_SPACE_MAXADDR	0xFFFFFFFFFFFFFFFF

#define BUS_SPACE_UNRESTRICTED	(~0)

#ifdef _KERNEL

/*
 * Map and unmap a region of device bus space into CPU virtual address space.
 */
int
bus_space_map(bus_space_tag_t, bus_addr_t, bus_size_t, int,
    bus_space_handle_t *);

void
bus_space_unmap(bus_space_tag_t, bus_space_handle_t, bus_size_t size);

/*
 * Get a new handle for a subregion of an already-mapped area of bus space.
 */
static __inline int
bus_space_subregion(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, bus_size_t size __unused, bus_space_handle_t *nbshp)
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
static __inline void
bus_space_barrier(bus_space_tag_t bst __unused, bus_space_handle_t bsh __unused,
    bus_size_t ofs __unused, bus_size_t size __unused, int flags __unused)
{
	ia64_mf_a();
	ia64_mf();
}


/*
 * Read 1 unit of data from bus space described by the tag, handle and ofs
 * tuple. A unit of data can be 1 byte, 2 bytes, 4 bytes or 8 bytes. The
 * data is returned.
 */
uint8_t  bus_space_read_io_1(u_long);
uint16_t bus_space_read_io_2(u_long);
uint32_t bus_space_read_io_4(u_long);
uint64_t bus_space_read_io_8(u_long);

static __inline uint8_t
bus_space_read_1(bus_space_tag_t bst, bus_space_handle_t bsh, bus_size_t ofs)
{
	uint8_t val;

	val = (__predict_false(bst == IA64_BUS_SPACE_IO))
	    ? bus_space_read_io_1(bsh + ofs)
	    : ia64_ld1((void *)(bsh + ofs));
	return (val);
}

static __inline uint16_t
bus_space_read_2(bus_space_tag_t bst, bus_space_handle_t bsh, bus_size_t ofs)
{
	uint16_t val;

	val = (__predict_false(bst == IA64_BUS_SPACE_IO))
	    ? bus_space_read_io_2(bsh + ofs)
	    : ia64_ld2((void *)(bsh + ofs));
	return (val);
}

static __inline uint32_t
bus_space_read_4(bus_space_tag_t bst, bus_space_handle_t bsh, bus_size_t ofs)
{
	uint32_t val;

	val = (__predict_false(bst == IA64_BUS_SPACE_IO))
	    ? bus_space_read_io_4(bsh + ofs)
	    : ia64_ld4((void *)(bsh + ofs));
	return (val);
}

static __inline uint64_t
bus_space_read_8(bus_space_tag_t bst, bus_space_handle_t bsh, bus_size_t ofs)
{
	uint64_t val;

	val = (__predict_false(bst == IA64_BUS_SPACE_IO))
	    ? bus_space_read_io_8(bsh + ofs)
	    : ia64_ld8((void *)(bsh + ofs));
	return (val);
}


/*
 * Write 1 unit of data to bus space described by the tag, handle and ofs
 * tuple. A unit of data can be 1 byte, 2 bytes, 4 bytes or 8 bytes. The
 * data is passed by value.
 */
void bus_space_write_io_1(u_long, uint8_t);
void bus_space_write_io_2(u_long, uint16_t);
void bus_space_write_io_4(u_long, uint32_t);
void bus_space_write_io_8(u_long, uint64_t);

static __inline void
bus_space_write_1(bus_space_tag_t bst, bus_space_handle_t bsh, bus_size_t ofs,
    uint8_t val)
{

	if (__predict_false(bst == IA64_BUS_SPACE_IO))
		bus_space_write_io_1(bsh + ofs, val);
	else
		ia64_st1((void *)(bsh + ofs), val);
}

static __inline void
bus_space_write_2(bus_space_tag_t bst, bus_space_handle_t bsh, bus_size_t ofs,
    uint16_t val)
{

	if (__predict_false(bst == IA64_BUS_SPACE_IO))
		bus_space_write_io_2(bsh + ofs, val);
	else
		ia64_st2((void *)(bsh + ofs), val);
}

static __inline void
bus_space_write_4(bus_space_tag_t bst, bus_space_handle_t bsh, bus_size_t ofs,
    uint32_t val)
{

	if (__predict_false(bst == IA64_BUS_SPACE_IO))
		bus_space_write_io_4(bsh + ofs, val);
	else
		ia64_st4((void *)(bsh + ofs), val);
}

static __inline void
bus_space_write_8(bus_space_tag_t bst, bus_space_handle_t bsh, bus_size_t ofs,
    uint64_t val)
{

	if (__predict_false(bst == IA64_BUS_SPACE_IO))
		bus_space_write_io_8(bsh + ofs, val);
	else
		ia64_st8((void *)(bsh + ofs), val);
}


/*
 * Read count units of data from bus space described by the tag, handle and
 * ofs tuple. A unit of data can be 1 byte, 2 bytes, 4 bytes or 8 bytes. The
 * data is returned in the buffer passed by reference.
 */
void bus_space_read_multi_io_1(u_long, uint8_t *, size_t);
void bus_space_read_multi_io_2(u_long, uint16_t *, size_t);
void bus_space_read_multi_io_4(u_long, uint32_t *, size_t);
void bus_space_read_multi_io_8(u_long, uint64_t *, size_t);

static __inline void
bus_space_read_multi_1(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint8_t *bufp, size_t count)
{

	if (__predict_false(bst == IA64_BUS_SPACE_IO))
		bus_space_read_multi_io_1(bsh + ofs, bufp, count);
	else {
		while (count-- > 0)
			*bufp++ = ia64_ld1((void *)(bsh + ofs));
	}
}

static __inline void
bus_space_read_multi_2(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint16_t *bufp, size_t count)
{

	if (__predict_false(bst == IA64_BUS_SPACE_IO))
		bus_space_read_multi_io_2(bsh + ofs, bufp, count);
	else {
		while (count-- > 0)
			*bufp++ = ia64_ld2((void *)(bsh + ofs));
	}
}

static __inline void
bus_space_read_multi_4(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint32_t *bufp, size_t count)
{

	if (__predict_false(bst == IA64_BUS_SPACE_IO))
		bus_space_read_multi_io_4(bsh + ofs, bufp, count);
	else {
		while (count-- > 0)
			*bufp++ = ia64_ld4((void *)(bsh + ofs));
	}
}

static __inline void
bus_space_read_multi_8(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint64_t *bufp, size_t count)
{

	if (__predict_false(bst == IA64_BUS_SPACE_IO))
		bus_space_read_multi_io_8(bsh + ofs, bufp, count);
	else {
		while (count-- > 0)
			*bufp++ = ia64_ld8((void *)(bsh + ofs));
	}
}


/*
 * Write count units of data to bus space described by the tag, handle and
 * ofs tuple. A unit of data can be 1 byte, 2 bytes, 4 bytes or 8 bytes. The
 * data is read from the buffer passed by reference.
 */
void bus_space_write_multi_io_1(u_long, const uint8_t *, size_t);
void bus_space_write_multi_io_2(u_long, const uint16_t *, size_t);
void bus_space_write_multi_io_4(u_long, const uint32_t *, size_t);
void bus_space_write_multi_io_8(u_long, const uint64_t *, size_t);

static __inline void
bus_space_write_multi_1(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, const uint8_t *bufp, size_t count)
{

	if (__predict_false(bst == IA64_BUS_SPACE_IO))
		bus_space_write_multi_io_1(bsh + ofs, bufp, count);
	else {
		while (count-- > 0)
			ia64_st1((void *)(bsh + ofs), *bufp++);
	}
}

static __inline void
bus_space_write_multi_2(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, const uint16_t *bufp, size_t count)
{

	if (__predict_false(bst == IA64_BUS_SPACE_IO))
		bus_space_write_multi_io_2(bsh + ofs, bufp, count);
	else {
		while (count-- > 0)
			ia64_st2((void *)(bsh + ofs), *bufp++);
	}
}

static __inline void
bus_space_write_multi_4(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, const uint32_t *bufp, size_t count)
{

	if (__predict_false(bst == IA64_BUS_SPACE_IO))
		bus_space_write_multi_io_4(bsh + ofs, bufp, count);
	else {
		while (count-- > 0)
			ia64_st4((void *)(bsh + ofs), *bufp++);
	}
}

static __inline void
bus_space_write_multi_8(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, const uint64_t *bufp, size_t count)
{

	if (__predict_false(bst == IA64_BUS_SPACE_IO))
		bus_space_write_multi_io_8(bsh + ofs, bufp, count);
	else {
		while (count-- > 0)
			ia64_st8((void *)(bsh + ofs), *bufp++);
	}
}


/*
 * Read count units of data from bus space described by the tag, handle and
 * ofs tuple. A unit of data can be 1 byte, 2 bytes, 4 bytes or 8 bytes. The
 * data is written to the buffer passed by reference and read from successive
 * bus space addresses. Access is unordered.
 */
void bus_space_read_region_io_1(u_long, uint8_t *, size_t);
void bus_space_read_region_io_2(u_long, uint16_t *, size_t);
void bus_space_read_region_io_4(u_long, uint32_t *, size_t);
void bus_space_read_region_io_8(u_long, uint64_t *, size_t);

static __inline void
bus_space_read_region_1(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint8_t *bufp, size_t count)
{

	if (__predict_false(bst == IA64_BUS_SPACE_IO))
		bus_space_read_region_io_1(bsh + ofs, bufp, count);
	else {
		uint8_t *bsp = (void *)(bsh + ofs);
		while (count-- > 0)
			*bufp++ = ia64_ld1(bsp++);
	}
}

static __inline void
bus_space_read_region_2(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint16_t *bufp, size_t count)
{

	if (__predict_false(bst == IA64_BUS_SPACE_IO))
		bus_space_read_region_io_2(bsh + ofs, bufp, count);
	else {
		uint16_t *bsp = (void *)(bsh + ofs);
		while (count-- > 0)
			*bufp++ = ia64_ld2(bsp++);
	}
}

static __inline void
bus_space_read_region_4(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint32_t *bufp, size_t count)
{

	if (__predict_false(bst == IA64_BUS_SPACE_IO))
		bus_space_read_region_io_4(bsh + ofs, bufp, count);
	else {
		uint32_t *bsp = (void *)(bsh + ofs);
		while (count-- > 0)
			*bufp++ = ia64_ld4(bsp++);
	}
}

static __inline void
bus_space_read_region_8(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint64_t *bufp, size_t count)
{

	if (__predict_false(bst == IA64_BUS_SPACE_IO))
		bus_space_read_region_io_8(bsh + ofs, bufp, count);
	else {
		uint64_t *bsp = (void *)(bsh + ofs);
		while (count-- > 0)
			*bufp++ = ia64_ld8(bsp++);
	}
}


/*
 * Write count units of data from bus space described by the tag, handle and
 * ofs tuple. A unit of data can be 1 byte, 2 bytes, 4 bytes or 8 bytes. The
 * data is read from the buffer passed by reference and written to successive
 * bus space addresses. Access is unordered.
 */
void bus_space_write_region_io_1(u_long, const uint8_t *, size_t);
void bus_space_write_region_io_2(u_long, const uint16_t *, size_t);
void bus_space_write_region_io_4(u_long, const uint32_t *, size_t);
void bus_space_write_region_io_8(u_long, const uint64_t *, size_t);

static __inline void
bus_space_write_region_1(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, const uint8_t *bufp, size_t count)
{

	if (__predict_false(bst == IA64_BUS_SPACE_IO))
		bus_space_write_region_io_1(bsh + ofs, bufp, count);
	else {
		uint8_t *bsp = (void *)(bsh + ofs);
		while (count-- > 0)
			ia64_st1(bsp++, *bufp++);
	}
}

static __inline void
bus_space_write_region_2(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, const uint16_t *bufp, size_t count)
{

	if (__predict_false(bst == IA64_BUS_SPACE_IO))
		bus_space_write_region_io_2(bsh + ofs, bufp, count);
	else {
		uint16_t *bsp = (void *)(bsh + ofs);
		while (count-- > 0)
			ia64_st2(bsp++, *bufp++);
	}
}

static __inline void
bus_space_write_region_4(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, const uint32_t *bufp, size_t count)
{

	if (__predict_false(bst == IA64_BUS_SPACE_IO))
		bus_space_write_region_io_4(bsh + ofs, bufp, count);
	else {
		uint32_t *bsp = (void *)(bsh + ofs);
		while (count-- > 0)
			ia64_st4(bsp++, *bufp++);
	}
}

static __inline void
bus_space_write_region_8(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, const uint64_t *bufp, size_t count)
{

	if (__predict_false(bst == IA64_BUS_SPACE_IO))
		bus_space_write_region_io_8(bsh + ofs, bufp, count);
	else {
		uint64_t *bsp = (void *)(bsh + ofs);
		while (count-- > 0)
			ia64_st8(bsp++, *bufp++);
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

	while (count-- > 0)
		bus_space_write_1(bst, bsh, ofs, val);
}

static __inline void
bus_space_set_multi_2(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint16_t val, size_t count)
{

	while (count-- > 0)
		bus_space_write_2(bst, bsh, ofs, val);
}

static __inline void
bus_space_set_multi_4(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint32_t val, size_t count)
{

	while (count-- > 0)
		bus_space_write_4(bst, bsh, ofs, val);
}

static __inline void
bus_space_set_multi_8(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint64_t val, size_t count)
{

	while (count-- > 0)
		bus_space_write_8(bst, bsh, ofs, val);
}


/*
 * Write count units of data from bus space described by the tag, handle and
 * ofs tuple. A unit of data can be 1 byte, 2 bytes, 4 bytes or 8 bytes. The
 * data is passed by value and written to successive bus space addresses.
 * Writes are unordered.
 */
void bus_space_set_region_io_1(u_long, uint8_t, size_t);
void bus_space_set_region_io_2(u_long, uint16_t, size_t);
void bus_space_set_region_io_4(u_long, uint32_t, size_t);
void bus_space_set_region_io_8(u_long, uint64_t, size_t);

static __inline void
bus_space_set_region_1(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint8_t val, size_t count)
{

	if (__predict_false(bst == IA64_BUS_SPACE_IO))
		bus_space_set_region_io_1(bsh + ofs, val, count);
	else {
		uint8_t *bsp = (void *)(bsh + ofs);
		while (count-- > 0)
			ia64_st1(bsp++, val);
	}
}

static __inline void
bus_space_set_region_2(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint16_t val, size_t count)
{

	if (__predict_false(bst == IA64_BUS_SPACE_IO))
		bus_space_set_region_io_2(bsh + ofs, val, count);
	else {
		uint16_t *bsp = (void *)(bsh + ofs);
		while (count-- > 0)
			ia64_st2(bsp++, val);
	}
}

static __inline void
bus_space_set_region_4(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint32_t val, size_t count)
{

	if (__predict_false(bst == IA64_BUS_SPACE_IO))
		bus_space_set_region_io_4(bsh + ofs, val, count);
	else {
		uint32_t *bsp = (void *)(bsh + ofs);
		while (count-- > 0)
			ia64_st4(bsp++, val);
	}
}

static __inline void
bus_space_set_region_8(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t ofs, uint64_t val, size_t count)
{

	if (__predict_false(bst == IA64_BUS_SPACE_IO))
		bus_space_set_region_io_4(bsh + ofs, val, count);
	else {
		uint64_t *bsp = (void *)(bsh + ofs);
		while (count-- > 0)
			ia64_st8(bsp++, val);
	}
}


/*
 * Copy count units of data from bus space described by the tag and the first
 * handle and ofs pair to bus space described by the tag and the second handle
 * and ofs pair. A unit of data can be 1 byte, 2 bytes, 4 bytes or 8 bytes.
 * The data is read from successive bus space addresses and also written to
 * successive bus space addresses. Both reads and writes are unordered.
 */
void bus_space_copy_region_io_1(u_long, u_long, size_t);
void bus_space_copy_region_io_2(u_long, u_long, size_t);
void bus_space_copy_region_io_4(u_long, u_long, size_t);
void bus_space_copy_region_io_8(u_long, u_long, size_t);

static __inline void
bus_space_copy_region_1(bus_space_tag_t bst, bus_space_handle_t sbsh,
    bus_size_t sofs, bus_space_handle_t dbsh, bus_size_t dofs, size_t count)
{
	uint8_t *dst, *src;

	if (__predict_false(bst == IA64_BUS_SPACE_IO)) {
		bus_space_copy_region_io_1(sbsh + sofs, dbsh + dofs, count);
		return;
	}

	src = (void *)(sbsh + sofs);
	dst = (void *)(dbsh + dofs);
	if (src < dst) {
		src += count - 1;
		dst += count - 1;
		while (count-- > 0)
			ia64_st1(dst--, ia64_ld1(src--));
	} else {
		while (count-- > 0)
			ia64_st1(dst++, ia64_ld1(src++));
	}
}

static __inline void
bus_space_copy_region_2(bus_space_tag_t bst, bus_space_handle_t sbsh,
    bus_size_t sofs, bus_space_handle_t dbsh, bus_size_t dofs, size_t count)
{
	uint16_t *dst, *src;

	if (__predict_false(bst == IA64_BUS_SPACE_IO)) {
		bus_space_copy_region_io_2(sbsh + sofs, dbsh + dofs, count);
		return;
	}

	src = (void *)(sbsh + sofs);
	dst = (void *)(dbsh + dofs);
	if (src < dst) {
		src += count - 1;
		dst += count - 1;
		while (count-- > 0)
			ia64_st2(dst--, ia64_ld2(src--));
	} else {
		while (count-- > 0)
			ia64_st2(dst++, ia64_ld2(src++));
	}
}

static __inline void
bus_space_copy_region_4(bus_space_tag_t bst, bus_space_handle_t sbsh,
    bus_size_t sofs, bus_space_handle_t dbsh, bus_size_t dofs, size_t count)
{
	uint32_t *dst, *src;

	if (__predict_false(bst == IA64_BUS_SPACE_IO)) {
		bus_space_copy_region_io_4(sbsh + sofs, dbsh + dofs, count);
		return;
	}

	src = (void *)(sbsh + sofs);
	dst = (void *)(dbsh + dofs);
	if (src < dst) {
		src += count - 1;
		dst += count - 1;
		while (count-- > 0)
			ia64_st4(dst--, ia64_ld4(src--));
	} else {
		while (count-- > 0)
			ia64_st4(dst++, ia64_ld4(src++));
	}
}

static __inline void
bus_space_copy_region_8(bus_space_tag_t bst, bus_space_handle_t sbsh,
    bus_size_t sofs, bus_space_handle_t dbsh, bus_size_t dofs, size_t count)
{
	uint64_t *dst, *src;

	if (__predict_false(bst == IA64_BUS_SPACE_IO)) {
		bus_space_copy_region_io_8(sbsh + sofs, dbsh + dofs, count);
		return;
	}

	src = (void *)(sbsh + sofs);
	dst = (void *)(dbsh + dofs);
	if (src < dst) {
		src += count - 1;
		dst += count - 1;
		while (count-- > 0)
			ia64_st8(dst--, ia64_ld8(src--));
	} else {
		while (count-- > 0)
			ia64_st8(dst++, ia64_ld8(src++));
	}
}


/*
 * Stream accesses are the same as normal accesses on ia64; there are no
 * supported bus systems with an endianess different from the host one.
 */

#define	bus_space_read_stream_1		bus_space_read_1
#define	bus_space_read_stream_2		bus_space_read_2
#define	bus_space_read_stream_4		bus_space_read_4
#define	bus_space_read_stream_8		bus_space_read_8

#define	bus_space_write_stream_1	bus_space_write_1
#define	bus_space_write_stream_2	bus_space_write_2
#define	bus_space_write_stream_4	bus_space_write_4
#define	bus_space_write_stream_8	bus_space_write_8

#define	bus_space_read_multi_stream_1	bus_space_read_multi_1
#define	bus_space_read_multi_stream_2	bus_space_read_multi_2
#define	bus_space_read_multi_stream_4	bus_space_read_multi_4
#define	bus_space_read_multi_stream_8	bus_space_read_multi_8

#define	bus_space_write_multi_stream_1	bus_space_write_multi_1
#define	bus_space_write_multi_stream_2	bus_space_write_multi_2
#define	bus_space_write_multi_stream_4	bus_space_write_multi_4
#define	bus_space_write_multi_stream_8	bus_space_write_multi_8

#define	bus_space_read_region_stream_1	bus_space_read_region_1
#define	bus_space_read_region_stream_2	bus_space_read_region_2
#define	bus_space_read_region_stream_4	bus_space_read_region_4
#define	bus_space_read_region_stream_8	bus_space_read_region_8

#define	bus_space_write_region_stream_1	bus_space_write_region_1
#define	bus_space_write_region_stream_2	bus_space_write_region_2
#define	bus_space_write_region_stream_4	bus_space_write_region_4
#define	bus_space_write_region_stream_8	bus_space_write_region_8

#define	bus_space_set_multi_stream_1	bus_space_set_multi_1
#define	bus_space_set_multi_stream_2	bus_space_set_multi_2
#define	bus_space_set_multi_stream_4	bus_space_set_multi_4
#define	bus_space_set_multi_stream_8	bus_space_set_multi_8

#define	bus_space_set_region_stream_1	bus_space_set_region_1
#define	bus_space_set_region_stream_2	bus_space_set_region_2
#define	bus_space_set_region_stream_4	bus_space_set_region_4
#define	bus_space_set_region_stream_8	bus_space_set_region_8

#define	bus_space_copy_region_stream_1	bus_space_copy_region_1
#define	bus_space_copy_region_stream_2	bus_space_copy_region_2
#define	bus_space_copy_region_stream_4	bus_space_copy_region_4
#define	bus_space_copy_region_stream_8	bus_space_copy_region_8

#endif /* _KERNEL */

#include <machine/bus_dma.h>

#endif /* _MACHINE_BUS_H_ */
