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
/* $FreeBSD: src/sys/i386/include/bus_at386.h,v 1.8.2.1 2000/04/14 13:08:54 nyan Exp $ */

#ifndef _I386_BUS_AT386_H_
#define _I386_BUS_AT386_H_

#include <machine/cpufunc.h>

/*
 * To remain compatible with NetBSD's interface, default to both memio and
 * pio when neither of them is defined.
 */ 
#if !defined(_I386_BUS_PIO_H_) && !defined(_I386_BUS_MEMIO_H_)
#define _I386_BUS_PIO_H_
#define _I386_BUS_MEMIO_H_
#endif

/*
 * Values for the i386 bus space tag, not to be used directly by MI code.
 */
#define	I386_BUS_SPACE_IO	0	/* space is i/o space */
#define I386_BUS_SPACE_MEM	1	/* space is mem space */

/*
 * Bus address and size types
 */
typedef u_int bus_addr_t;
typedef u_int bus_size_t;

#define BUS_SPACE_MAXSIZE_24BIT	0xFFFFFF
#define BUS_SPACE_MAXSIZE_32BIT 0xFFFFFFFF
#define BUS_SPACE_MAXSIZE	(64 * 1024) /* Maximum supported size */
#define BUS_SPACE_MAXADDR_24BIT	0xFFFFFF
#define BUS_SPACE_MAXADDR_32BIT 0xFFFFFFFF
#define BUS_SPACE_MAXADDR	0xFFFFFFFF

#define BUS_SPACE_UNRESTRICTED	(~0)

/*
 * Access methods for bus resources and address space.
 */
typedef	int bus_space_tag_t;
typedef	u_int bus_space_handle_t;

/*
 * Map a region of device bus space into CPU virtual address space.
 */

#define	BUS_SPACE_MAP_CACHEABLE		0x01
#define	BUS_SPACE_MAP_LINEAR		0x02

int	bus_space_map(bus_space_tag_t t, bus_addr_t addr, bus_size_t size,
		      int flags, bus_space_handle_t *bshp);

/*
 * Unmap a region of device bus space.
 */

void	bus_space_unmap(bus_space_tag_t t, bus_space_handle_t bsh,
			bus_size_t size);

/*
 * Get a new handle for a subregion of an already-mapped area of bus space.
 */

int	bus_space_subregion(bus_space_tag_t t, bus_space_handle_t bsh,
			    bus_size_t offset, bus_size_t size,
			    bus_space_handle_t *nbshp);

/*
 * Allocate a region of memory that is accessible to devices in bus space.
 */

int	bus_space_alloc(bus_space_tag_t t, bus_addr_t rstart,
			bus_addr_t rend, bus_size_t size, bus_size_t align,
			bus_size_t boundary, int flags, bus_addr_t *addrp,
			bus_space_handle_t *bshp);

/*
 * Free a region of bus space accessible memory.
 */

void	bus_space_free(bus_space_tag_t t, bus_space_handle_t bsh,
		       bus_size_t size);

#if defined(_I386_BUS_PIO_H_) || defined(_I386_BUS_MEMIO_H_)

/*
 * Read a 1, 2, 4, or 8 byte quantity from bus space
 * described by tag/handle/offset.
 */
static __inline u_int8_t bus_space_read_1(bus_space_tag_t tag,
					  bus_space_handle_t handle,
					  bus_size_t offset);

static __inline u_int16_t bus_space_read_2(bus_space_tag_t tag,
					   bus_space_handle_t handle,
					   bus_size_t offset);

static __inline u_int32_t bus_space_read_4(bus_space_tag_t tag,
					   bus_space_handle_t handle,
					   bus_size_t offset);

static __inline u_int8_t
bus_space_read_1(bus_space_tag_t tag, bus_space_handle_t handle,
		 bus_size_t offset)
{
#if defined (_I386_BUS_PIO_H_)
#if defined (_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
		return (inb(handle + offset));
#endif
#if defined (_I386_BUS_MEMIO_H_)
	return (*(volatile u_int8_t *)(handle + offset));
#endif
}

static __inline u_int16_t
bus_space_read_2(bus_space_tag_t tag, bus_space_handle_t handle,
		 bus_size_t offset)
{
#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
		return (inw(handle + offset));
#endif
#if defined(_I386_BUS_MEMIO_H_)
	return (*(volatile u_int16_t *)(handle + offset));
#endif
}

static __inline u_int32_t
bus_space_read_4(bus_space_tag_t tag, bus_space_handle_t handle,
		 bus_size_t offset)
{
#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
		return (inl(handle + offset));
#endif
#if defined(_I386_BUS_MEMIO_H_)
	return (*(volatile u_int32_t *)(handle + offset));
#endif
}

#if 0	/* Cause a link error for bus_space_read_8 */
#define	bus_space_read_8(t, h, o)	!!! bus_space_read_8 unimplemented !!!
#endif

/*
 * Read `count' 1, 2, 4, or 8 byte quantities from bus space
 * described by tag/handle/offset and copy into buffer provided.
 */
static __inline void bus_space_read_multi_1(bus_space_tag_t tag,
					    bus_space_handle_t bsh,
					    bus_size_t offset, u_int8_t *addr,
					    size_t count);

static __inline void bus_space_read_multi_2(bus_space_tag_t tag,
					    bus_space_handle_t bsh,
					    bus_size_t offset, u_int16_t *addr,
					    size_t count);

static __inline void bus_space_read_multi_4(bus_space_tag_t tag,
					    bus_space_handle_t bsh,
					    bus_size_t offset, u_int32_t *addr,
					    size_t count);

static __inline void
bus_space_read_multi_1(bus_space_tag_t tag, bus_space_handle_t bsh,
		       bus_size_t offset, u_int8_t *addr, size_t count)
{
#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
		insb(bsh + offset, addr, count);
#endif
#if defined(_I386_BUS_MEMIO_H_)
#if defined(_I386_BUS_PIO_H_)
	else
#endif
	{
		__asm __volatile("				\n\
			cld					\n\
		1:	movb (%2),%%al				\n\
			stosb					\n\
			loop 1b"				:
		    "=D" (addr), "=c" (count)			:
		    "r" (bsh + offset), "0" (addr), "1" (count)	:
		    "%eax", "memory");
	}
#endif
}

static __inline void
bus_space_read_multi_2(bus_space_tag_t tag, bus_space_handle_t bsh,
		       bus_size_t offset, u_int16_t *addr, size_t count)
{
#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
		insw(bsh + offset, addr, count);
#endif
#if defined(_I386_BUS_MEMIO_H_)
#if defined(_I386_BUS_PIO_H_)
	else
#endif
	{
		__asm __volatile("				\n\
			cld					\n\
		1:	movw (%2),%%ax				\n\
			stosw					\n\
			loop 1b"				:
		    "=D" (addr), "=c" (count)			:
		    "r" (bsh + offset), "0" (addr), "1" (count)	:
		    "%eax", "memory");
	}
#endif
}

static __inline void
bus_space_read_multi_4(bus_space_tag_t tag, bus_space_handle_t bsh,
		       bus_size_t offset, u_int32_t *addr, size_t count)
{
#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
		insl(bsh + offset, addr, count);
#endif
#if defined(_I386_BUS_MEMIO_H_)
#if defined(_I386_BUS_PIO_H_)
	else
#endif
	{
		__asm __volatile("				\n\
			cld					\n\
		1:	movl (%2),%%eax				\n\
			stosl					\n\
			loop 1b"				:
		    "=D" (addr), "=c" (count)			:
		    "r" (bsh + offset), "0" (addr), "1" (count)	:
		    "%eax", "memory");
	}
#endif
}

#if 0	/* Cause a link error for bus_space_read_multi_8 */
#define	bus_space_read_multi_8	!!! bus_space_read_multi_8 unimplemented !!!
#endif

/*
 * Read `count' 1, 2, 4, or 8 byte quantities from bus space
 * described by tag/handle and starting at `offset' and copy into
 * buffer provided.
 */
static __inline void bus_space_read_region_1(bus_space_tag_t tag,
					     bus_space_handle_t bsh,
					     bus_size_t offset, u_int8_t *addr,
					     size_t count);

static __inline void bus_space_read_region_2(bus_space_tag_t tag,
					     bus_space_handle_t bsh,
					     bus_size_t offset, u_int16_t *addr,
					     size_t count);

static __inline void bus_space_read_region_4(bus_space_tag_t tag,
					     bus_space_handle_t bsh,
					     bus_size_t offset, u_int32_t *addr,
					     size_t count);


static __inline void
bus_space_read_region_1(bus_space_tag_t tag, bus_space_handle_t bsh,
			bus_size_t offset, u_int8_t *addr, size_t count)
{
#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
	{
		int _port_ = bsh + offset;			\
		__asm __volatile("				\n\
			cld					\n\
		1:	inb %w2,%%al				\n\
			stosb					\n\
			incl %2					\n\
			loop 1b"				:
		    "=D" (addr), "=c" (count), "=d" (_port_)	:
		    "0" (addr), "1" (count), "2" (_port_)	:
		    "%eax", "memory", "cc");
	}
#endif
#if defined(_I386_BUS_MEMIO_H_)
#if defined(_I386_BUS_PIO_H_)
	else
#endif
	{
		int _port_ = bsh + offset;			\
		__asm __volatile("				\n\
			cld					\n\
			repne					\n\
			movsb"					:
		    "=D" (addr), "=c" (count), "=S" (_port_)	:
		    "0" (addr), "1" (count), "2" (_port_)	:
		    "memory", "cc");
	}
#endif
}

static __inline void
bus_space_read_region_2(bus_space_tag_t tag, bus_space_handle_t bsh,
			bus_size_t offset, u_int16_t *addr, size_t count)
{
#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
	{
		int _port_ = bsh + offset;			\
		__asm __volatile("				\n\
			cld					\n\
		1:	inw %w2,%%ax				\n\
			stosw					\n\
			addl $2,%2				\n\
			loop 1b"				:
		    "=D" (addr), "=c" (count), "=d" (_port_)	:
		    "0" (addr), "1" (count), "2" (_port_)	:
		    "%eax", "memory", "cc");
	}
#endif
#if defined(_I386_BUS_MEMIO_H_)
#if defined(_I386_BUS_PIO_H_)
	else
#endif
	{
		int _port_ = bsh + offset;			\
		__asm __volatile("				\n\
			cld					\n\
			repne					\n\
			movsw"					:
		    "=D" (addr), "=c" (count), "=S" (_port_)	:
		    "0" (addr), "1" (count), "2" (_port_)	:
		    "memory", "cc");
	}
#endif
}

static __inline void
bus_space_read_region_4(bus_space_tag_t tag, bus_space_handle_t bsh,
			bus_size_t offset, u_int32_t *addr, size_t count)
{
#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
	{
		int _port_ = bsh + offset;			\
		__asm __volatile("				\n\
			cld					\n\
		1:	inl %w2,%%eax				\n\
			stosl					\n\
			addl $4,%2				\n\
			loop 1b"				:
		    "=D" (addr), "=c" (count), "=d" (_port_)	:
		    "0" (addr), "1" (count), "2" (_port_)	:
		    "%eax", "memory", "cc");
	}
#endif
#if defined(_I386_BUS_MEMIO_H_)
#if defined(_I386_BUS_PIO_H_)
	else
#endif
	{
		int _port_ = bsh + offset;			\
		__asm __volatile("				\n\
			cld					\n\
			repne					\n\
			movsl"					:
		    "=D" (addr), "=c" (count), "=S" (_port_)	:
		    "0" (addr), "1" (count), "2" (_port_)	:
		    "memory", "cc");
	}
#endif
}

#if 0	/* Cause a link error for bus_space_read_region_8 */
#define	bus_space_read_region_8	!!! bus_space_read_region_8 unimplemented !!!
#endif

/*
 * Write the 1, 2, 4, or 8 byte value `value' to bus space
 * described by tag/handle/offset.
 */

static __inline void bus_space_write_1(bus_space_tag_t tag,
				       bus_space_handle_t bsh,
				       bus_size_t offset, u_int8_t value);

static __inline void bus_space_write_2(bus_space_tag_t tag,
				       bus_space_handle_t bsh,
				       bus_size_t offset, u_int16_t value);

static __inline void bus_space_write_4(bus_space_tag_t tag,
				       bus_space_handle_t bsh,
				       bus_size_t offset, u_int32_t value);

static __inline void
bus_space_write_1(bus_space_tag_t tag, bus_space_handle_t bsh,
		       bus_size_t offset, u_int8_t value)
{
#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
		outb(bsh + offset, value);
#endif
#if defined(_I386_BUS_MEMIO_H_)
#if defined(_I386_BUS_PIO_H_)
	else
#endif
		*(volatile u_int8_t *)(bsh + offset) = value;
#endif
}

static __inline void
bus_space_write_2(bus_space_tag_t tag, bus_space_handle_t bsh,
		       bus_size_t offset, u_int16_t value)
{
#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
		outw(bsh + offset, value);
#endif
#if defined(_I386_BUS_MEMIO_H_)
#if defined(_I386_BUS_PIO_H_)
	else
#endif
		*(volatile u_int16_t *)(bsh + offset) = value;
#endif
}

static __inline void
bus_space_write_4(bus_space_tag_t tag, bus_space_handle_t bsh,
		       bus_size_t offset, u_int32_t value)
{
#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
		outl(bsh + offset, value);
#endif
#if defined(_I386_BUS_MEMIO_H_)
#if defined(_I386_BUS_PIO_H_)
	else
#endif
		*(volatile u_int32_t *)(bsh + offset) = value;
#endif
}

#if 0	/* Cause a link error for bus_space_write_8 */
#define	bus_space_write_8	!!! bus_space_write_8 not implemented !!!
#endif

/*
 * Write `count' 1, 2, 4, or 8 byte quantities from the buffer
 * provided to bus space described by tag/handle/offset.
 */

static __inline void bus_space_write_multi_1(bus_space_tag_t tag,
					     bus_space_handle_t bsh,
					     bus_size_t offset,
					     const u_int8_t *addr,
					     size_t count);
static __inline void bus_space_write_multi_2(bus_space_tag_t tag,
					     bus_space_handle_t bsh,
					     bus_size_t offset,
					     const u_int16_t *addr,
					     size_t count);

static __inline void bus_space_write_multi_4(bus_space_tag_t tag,
					     bus_space_handle_t bsh,
					     bus_size_t offset,
					     const u_int32_t *addr,
					     size_t count);

static __inline void
bus_space_write_multi_1(bus_space_tag_t tag, bus_space_handle_t bsh,
			bus_size_t offset, const u_int8_t *addr, size_t count)
{
#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
		outsb(bsh + offset, addr, count);
#endif
#if defined(_I386_BUS_MEMIO_H_)
#if defined(_I386_BUS_PIO_H_)
	else
#endif
	{
		__asm __volatile("				\n\
			cld					\n\
		1:	lodsb					\n\
			movb %%al,(%2)				\n\
			loop 1b"				:
		    "=S" (addr), "=c" (count)			:
		    "r" (bsh + offset), "0" (addr), "1" (count)	:
		    "%eax", "memory", "cc");
	}
#endif
}

static __inline void
bus_space_write_multi_2(bus_space_tag_t tag, bus_space_handle_t bsh,
			bus_size_t offset, const u_int16_t *addr, size_t count)
{
#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
		outsw(bsh + offset, addr, count);
#endif
#if defined(_I386_BUS_MEMIO_H_)
#if defined(_I386_BUS_PIO_H_)
	else
#endif
	{
		__asm __volatile("				\n\
			cld					\n\
		1:	lodsw					\n\
			movw %%ax,(%1)				\n\
			movw %%ax,(%2)				\n\
			loop 1b"				:
		    "=S" (addr), "=c" (count)			:
		    "r" (bsh + offset), "0" (addr), "1" (count)	:
		    "%eax", "memory", "cc");
	}
#endif
}

static __inline void
bus_space_write_multi_4(bus_space_tag_t tag, bus_space_handle_t bsh,
			bus_size_t offset, const u_int32_t *addr, size_t count)
{
#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
		outsl(bsh + offset, addr, count);
#endif
#if defined(_I386_BUS_MEMIO_H_)
#if defined(_I386_BUS_PIO_H_)
	else
#endif
	{
		__asm __volatile("				\n\
			cld					\n\
		1:	lodsl					\n\
			movl %%eax,(%2)				\n\
			loop 1b"				:
		    "=S" (addr), "=c" (count)			:
		    "r" (bsh + offset), "0" (addr), "1" (count)	:
		    "%eax", "memory", "cc");
	}
#endif
}

#if 0	/* Cause a link error for bus_space_write_multi_8 */
#define	bus_space_write_multi_8(t, h, o, a, c)				\
			!!! bus_space_write_multi_8 unimplemented !!!
#endif

/*
 * Write `count' 1, 2, 4, or 8 byte quantities from the buffer provided
 * to bus space described by tag/handle starting at `offset'.
 */

static __inline void bus_space_write_region_1(bus_space_tag_t tag,
					      bus_space_handle_t bsh,
					      bus_size_t offset,
					      const u_int8_t *addr,
					      size_t count);
static __inline void bus_space_write_region_2(bus_space_tag_t tag,
					      bus_space_handle_t bsh,
					      bus_size_t offset,
					      const u_int16_t *addr,
					      size_t count);
static __inline void bus_space_write_region_4(bus_space_tag_t tag,
					      bus_space_handle_t bsh,
					      bus_size_t offset,
					      const u_int32_t *addr,
					      size_t count);

static __inline void
bus_space_write_region_1(bus_space_tag_t tag, bus_space_handle_t bsh,
			 bus_size_t offset, const u_int8_t *addr, size_t count)
{
#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
	{
		int _port_ = bsh + offset;			\
		__asm __volatile("				\n\
			cld					\n\
		1:	lodsb					\n\
			outb %%al,%w0				\n\
			incl %0					\n\
			loop 1b"				:
		    "=d" (_port_), "=S" (addr), "=c" (count)	:
		    "0" (_port_), "1" (addr), "2" (count)	:
		    "%eax", "memory", "cc");
	}
#endif
#if defined(_I386_BUS_MEMIO_H_)
#if defined(_I386_BUS_PIO_H_)
	else
#endif
	{
		int _port_ = bsh + offset;			\
		__asm __volatile("				\n\
			cld					\n\
			repne					\n\
			movsb"					:
		    "=D" (_port_), "=S" (addr), "=c" (count)	:
		    "0" (_port_), "1" (addr), "2" (count)	:
		    "memory", "cc");
	}
#endif
}

static __inline void
bus_space_write_region_2(bus_space_tag_t tag, bus_space_handle_t bsh,
			 bus_size_t offset, const u_int16_t *addr, size_t count)
{
#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
	{
		int _port_ = bsh + offset;			\
		__asm __volatile("				\n\
			cld					\n\
		1:	lodsw					\n\
			outw %%ax,%w0				\n\
			addl $2,%0				\n\
			loop 1b"				:
		    "=d" (_port_), "=S" (addr), "=c" (count)	:
		    "0" (_port_), "1" (addr), "2" (count)	:
		    "%eax", "memory", "cc");
	}
#endif
#if defined(_I386_BUS_MEMIO_H_)
#if defined(_I386_BUS_PIO_H_)
	else
#endif
	{
		int _port_ = bsh + offset;			\
		__asm __volatile("				\n\
			cld					\n\
			repne					\n\
			movsw"					:
		    "=D" (_port_), "=S" (addr), "=c" (count)	:
		    "0" (_port_), "1" (addr), "2" (count)	:
		    "memory", "cc");
	}
#endif
}

static __inline void
bus_space_write_region_4(bus_space_tag_t tag, bus_space_handle_t bsh,
			 bus_size_t offset, const u_int32_t *addr, size_t count)
{
#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
	{
		int _port_ = bsh + offset;			\
		__asm __volatile("				\n\
			cld					\n\
		1:	lodsl					\n\
			outl %%eax,%w0				\n\
			addl $4,%0				\n\
			loop 1b"				:
		    "=d" (_port_), "=S" (addr), "=c" (count)	:
		    "0" (_port_), "1" (addr), "2" (count)	:
		    "%eax", "memory", "cc");
	}
#endif
#if defined(_I386_BUS_MEMIO_H_)
#if defined(_I386_BUS_PIO_H_)
	else
#endif
	{
		int _port_ = bsh + offset;			\
		__asm __volatile("				\n\
			cld					\n\
			repne					\n\
			movsl"					:
		    "=D" (_port_), "=S" (addr), "=c" (count)	:
		    "0" (_port_), "1" (addr), "2" (count)	:
		    "memory", "cc");
	}
#endif
}

#if 0	/* Cause a link error for bus_space_write_region_8 */
#define	bus_space_write_region_8					\
			!!! bus_space_write_region_8 unimplemented !!!
#endif

/*
 * Write the 1, 2, 4, or 8 byte value `val' to bus space described
 * by tag/handle/offset `count' times.
 */

static __inline void bus_space_set_multi_1(bus_space_tag_t tag,
					   bus_space_handle_t bsh,
					   bus_size_t offset,
					   u_int8_t value, size_t count);
static __inline void bus_space_set_multi_2(bus_space_tag_t tag,
					   bus_space_handle_t bsh,
					   bus_size_t offset,
					   u_int16_t value, size_t count);
static __inline void bus_space_set_multi_4(bus_space_tag_t tag,
					   bus_space_handle_t bsh,
					   bus_size_t offset,
					   u_int32_t value, size_t count);

static __inline void
bus_space_set_multi_1(bus_space_tag_t tag, bus_space_handle_t bsh,
		      bus_size_t offset, u_int8_t value, size_t count)
{
	bus_addr_t addr = bsh + offset;

#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
		while (count--)
			outb(addr, value);
#endif
#if defined(_I386_BUS_MEMIO_H_)
#if defined(_I386_BUS_PIO_H_)
	else
#endif
		while (count--)
			*(volatile u_int8_t *)(addr) = value;
#endif
}

static __inline void
bus_space_set_multi_2(bus_space_tag_t tag, bus_space_handle_t bsh,
		     bus_size_t offset, u_int16_t value, size_t count)
{
	bus_addr_t addr = bsh + offset;

#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
		while (count--)
			outw(addr, value);
#endif
#if defined(_I386_BUS_MEMIO_H_)
#if defined(_I386_BUS_PIO_H_)
	else
#endif
		while (count--)
			*(volatile u_int16_t *)(addr) = value;
#endif
}

static __inline void
bus_space_set_multi_4(bus_space_tag_t tag, bus_space_handle_t bsh,
		      bus_size_t offset, u_int32_t value, size_t count)
{
	bus_addr_t addr = bsh + offset;

#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
		while (count--)
			outl(addr, value);
#endif
#if defined(_I386_BUS_MEMIO_H_)
#if defined(_I386_BUS_PIO_H_)
	else
#endif
		while (count--)
			*(volatile u_int32_t *)(addr) = value;
#endif
}

#if 0	/* Cause a link error for bus_space_set_multi_8 */
#define	bus_space_set_multi_8 !!! bus_space_set_multi_8 unimplemented !!!
#endif

/*
 * Write `count' 1, 2, 4, or 8 byte value `val' to bus space described
 * by tag/handle starting at `offset'.
 */

static __inline void bus_space_set_region_1(bus_space_tag_t tag,
					    bus_space_handle_t bsh,
					    bus_size_t offset, u_int8_t value,
					    size_t count);
static __inline void bus_space_set_region_2(bus_space_tag_t tag,
					    bus_space_handle_t bsh,
					    bus_size_t offset, u_int16_t value,
					    size_t count);
static __inline void bus_space_set_region_4(bus_space_tag_t tag,
					    bus_space_handle_t bsh,
					    bus_size_t offset, u_int32_t value,
					    size_t count);

static __inline void
bus_space_set_region_1(bus_space_tag_t tag, bus_space_handle_t bsh,
		       bus_size_t offset, u_int8_t value, size_t count)
{
	bus_addr_t addr = bsh + offset;

#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
		for (; count != 0; count--, addr++)
			outb(addr, value);
#endif
#if defined(_I386_BUS_MEMIO_H_)
#if defined(_I386_BUS_PIO_H_)
	else
#endif
		for (; count != 0; count--, addr++)
			*(volatile u_int8_t *)(addr) = value;
#endif
}

static __inline void
bus_space_set_region_2(bus_space_tag_t tag, bus_space_handle_t bsh,
		       bus_size_t offset, u_int16_t value, size_t count)
{
	bus_addr_t addr = bsh + offset;

#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
		for (; count != 0; count--, addr += 2)
			outw(addr, value);
#endif
#if defined(_I386_BUS_MEMIO_H_)
#if defined(_I386_BUS_PIO_H_)
	else
#endif
		for (; count != 0; count--, addr += 2)
			*(volatile u_int16_t *)(addr) = value;
#endif
}

static __inline void
bus_space_set_region_4(bus_space_tag_t tag, bus_space_handle_t bsh,
		       bus_size_t offset, u_int32_t value, size_t count)
{
	bus_addr_t addr = bsh + offset;

#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
		for (; count != 0; count--, addr += 4)
			outl(addr, value);
#endif
#if defined(_I386_BUS_MEMIO_H_)
#if defined(_I386_BUS_PIO_H_)
	else
#endif
		for (; count != 0; count--, addr += 4)
			*(volatile u_int32_t *)(addr) = value;
#endif
}

#if 0	/* Cause a link error for bus_space_set_region_8 */
#define	bus_space_set_region_8	!!! bus_space_set_region_8 unimplemented !!!
#endif

/*
 * Copy `count' 1, 2, 4, or 8 byte values from bus space starting
 * at tag/bsh1/off1 to bus space starting at tag/bsh2/off2.
 */

static __inline void bus_space_copy_region_1(bus_space_tag_t tag,
					     bus_space_handle_t bsh1,
					     bus_size_t off1,
					     bus_space_handle_t bsh2,
					     bus_size_t off2, size_t count);

static __inline void bus_space_copy_region_2(bus_space_tag_t tag,
					     bus_space_handle_t bsh1,
					     bus_size_t off1,
					     bus_space_handle_t bsh2,
					     bus_size_t off2, size_t count);

static __inline void bus_space_copy_region_4(bus_space_tag_t tag,
					     bus_space_handle_t bsh1,
					     bus_size_t off1,
					     bus_space_handle_t bsh2,
					     bus_size_t off2, size_t count);

static __inline void
bus_space_copy_region_1(bus_space_tag_t tag, bus_space_handle_t bsh1,
			bus_size_t off1, bus_space_handle_t bsh2,
			bus_size_t off2, size_t count)
{
	bus_addr_t addr1 = bsh1 + off1;
	bus_addr_t addr2 = bsh2 + off2;

#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
	{
		if (addr1 >= addr2) {
			/* src after dest: copy forward */
			for (; count != 0; count--, addr1++, addr2++)
				outb(addr2, inb(addr1));
		} else {
			/* dest after src: copy backwards */
			for (addr1 += (count - 1), addr2 += (count - 1);
			    count != 0; count--, addr1--, addr2--)
				outb(addr2, inb(addr1));
		}
	}
#endif
#if defined(_I386_BUS_MEMIO_H_)
#if defined(_I386_BUS_PIO_H_)
	else
#endif
	{
		if (addr1 >= addr2) {
			/* src after dest: copy forward */
			for (; count != 0; count--, addr1++, addr2++)
				*(volatile u_int8_t *)(addr2) =
				    *(volatile u_int8_t *)(addr1);
		} else {
			/* dest after src: copy backwards */
			for (addr1 += (count - 1), addr2 += (count - 1);
			    count != 0; count--, addr1--, addr2--)
				*(volatile u_int8_t *)(addr2) =
				    *(volatile u_int8_t *)(addr1);
		}
	}
#endif
}

static __inline void
bus_space_copy_region_2(bus_space_tag_t tag, bus_space_handle_t bsh1,
			bus_size_t off1, bus_space_handle_t bsh2,
			bus_size_t off2, size_t count)
{
	bus_addr_t addr1 = bsh1 + off1;
	bus_addr_t addr2 = bsh2 + off2;

#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
	{
		if (addr1 >= addr2) {
			/* src after dest: copy forward */
			for (; count != 0; count--, addr1 += 2, addr2 += 2)
				outw(addr2, inw(addr1));
		} else {
			/* dest after src: copy backwards */
			for (addr1 += 2 * (count - 1), addr2 += 2 * (count - 1);
			    count != 0; count--, addr1 -= 2, addr2 -= 2)
				outw(addr2, inw(addr1));
		}
	}
#endif
#if defined(_I386_BUS_MEMIO_H_)
#if defined(_I386_BUS_PIO_H_)
	else
#endif
	{
		if (addr1 >= addr2) {
			/* src after dest: copy forward */
			for (; count != 0; count--, addr1 += 2, addr2 += 2)
				*(volatile u_int16_t *)(addr2) =
				    *(volatile u_int16_t *)(addr1);
		} else {
			/* dest after src: copy backwards */
			for (addr1 += 2 * (count - 1), addr2 += 2 * (count - 1);
			    count != 0; count--, addr1 -= 2, addr2 -= 2)
				*(volatile u_int16_t *)(addr2) =
				    *(volatile u_int16_t *)(addr1);
		}
	}
#endif
}

static __inline void
bus_space_copy_region_4(bus_space_tag_t tag, bus_space_handle_t bsh1,
			bus_size_t off1, bus_space_handle_t bsh2,
			bus_size_t off2, size_t count)
{
	bus_addr_t addr1 = bsh1 + off1;
	bus_addr_t addr2 = bsh2 + off2;

#if defined(_I386_BUS_PIO_H_)
#if defined(_I386_BUS_MEMIO_H_)
	if (tag == I386_BUS_SPACE_IO)
#endif
	{
		if (addr1 >= addr2) {
			/* src after dest: copy forward */
			for (; count != 0; count--, addr1 += 4, addr2 += 4)
				outl(addr2, inl(addr1));
		} else {
			/* dest after src: copy backwards */
			for (addr1 += 4 * (count - 1), addr2 += 4 * (count - 1);
			    count != 0; count--, addr1 -= 4, addr2 -= 4)
				outl(addr2, inl(addr1));
		}
	}
#endif
#if defined(_I386_BUS_MEMIO_H_)
#if defined(_I386_BUS_PIO_H_)
	else
#endif
	{
		if (addr1 >= addr2) {
			/* src after dest: copy forward */
			for (; count != 0; count--, addr1 += 4, addr2 += 4)
				*(volatile u_int32_t *)(addr2) =
				    *(volatile u_int32_t *)(addr1);
		} else {
			/* dest after src: copy backwards */
			for (addr1 += 4 * (count - 1), addr2 += 4 * (count - 1);
			    count != 0; count--, addr1 -= 4, addr2 -= 4)
				*(volatile u_int32_t *)(addr2) =
				    *(volatile u_int32_t *)(addr1);
		}
	}
#endif
}

#endif /* defined(_I386_BUS_PIO_H_) || defined(_I386_MEM_IO_H_) */

#if 0	/* Cause a link error for bus_space_copy_8 */
#define	bus_space_copy_region_8	!!! bus_space_copy_region_8 unimplemented !!!
#endif

/*
 * Bus read/write barrier methods.
 *
 *	void bus_space_barrier(bus_space_tag_t tag, bus_space_handle_t bsh,
 *			       bus_size_t offset, bus_size_t len, int flags);
 *
 * Note: the i386 does not currently require barriers, but we must
 * provide the flags to MI code.
 */
#define	bus_space_barrier(t, h, o, l, f)	\
	((void)((void)(t), (void)(h), (void)(o), (void)(l), (void)(f)))
#define	BUS_SPACE_BARRIER_READ	0x01		/* force read barrier */
#define	BUS_SPACE_BARRIER_WRITE	0x02		/* force write barrier */

/*
 * Flags used in various bus DMA methods.
 */
#define	BUS_DMA_WAITOK		0x00	/* safe to sleep (pseudo-flag) */
#define	BUS_DMA_NOWAIT		0x01	/* not safe to sleep */
#define	BUS_DMA_ALLOCNOW	0x02	/* perform resource allocation now */
#define	BUS_DMAMEM_NOSYNC	0x04	/* map memory to not require sync */
#define	BUS_DMA_BUS1		0x10	/* placeholders for bus functions... */
#define	BUS_DMA_BUS2		0x20
#define	BUS_DMA_BUS3		0x40
#define	BUS_DMA_BUS4		0x80

/* Forwards needed by prototypes below. */
struct mbuf;
struct uio;

/*
 *	bus_dmasync_op_t
 *
 *	Operations performed by bus_dmamap_sync().
 */
typedef enum {
	BUS_DMASYNC_PREREAD,
	BUS_DMASYNC_POSTREAD,
	BUS_DMASYNC_PREWRITE,
	BUS_DMASYNC_POSTWRITE
} bus_dmasync_op_t;

/*
 *	bus_dma_tag_t
 *
 *	A machine-dependent opaque type describing the characteristics
 *	of how to perform DMA mappings.  This structure encapsultes
 *	information concerning address and alignment restrictions, number
 *	of S/G	segments, amount of data per S/G segment, etc.
 */
typedef struct bus_dma_tag	*bus_dma_tag_t;

/*
 *	bus_dmamap_t
 *
 *	DMA mapping instance information.
 */
typedef struct bus_dmamap	*bus_dmamap_t;

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
 *	highaddr:	High restricted address that cannot appear in a mapping.
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
		       bus_size_t boundary, bus_addr_t lowaddr,
		       bus_addr_t highaddr, bus_dma_filter_t *filtfunc,
		       void *filtfuncarg, bus_size_t maxsize, int nsegments,
		       bus_size_t maxsegsz, int flags, bus_dma_tag_t *dmat);

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
		    bus_size_t buflen, bus_dmamap_callback_t *callback,
		    void *callback_arg, int flags);

/*
 * Perform a syncronization operation on the given map.
 */
void _bus_dmamap_sync(bus_dma_tag_t, bus_dmamap_t, bus_dmasync_op_t);
#define bus_dmamap_sync(dmat, dmamap, op) 		\
	if ((dmamap) != NULL)				\
		_bus_dmamap_sync(dmat, dmamap, op)

/*
 * Release the mapping held by map.
 */
void _bus_dmamap_unload(bus_dma_tag_t dmat, bus_dmamap_t map);
#define bus_dmamap_unload(dmat, dmamap) 		\
	if ((dmamap) != NULL)				\
		_bus_dmamap_unload(dmat, dmamap)

#endif /* _I386_BUS_AT386_H_ */
