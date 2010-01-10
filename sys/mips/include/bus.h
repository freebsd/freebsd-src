/*	$NetBSD: bus.h,v 1.11 2003/07/28 17:35:54 thorpej Exp $	*/

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
 *	from: src/sys/alpha/include/bus.h,v 1.5 1999/08/28 00:38:40 peter
 * $FreeBSD$
*/

#ifndef _MACHINE_BUS_H_
#define	_MACHINE_BUS_H_

#ifdef TARGET_OCTEON
#include <machine/bus_octeon.h>
#else
#include <machine/_bus.h>
#include <machine/cpufunc.h>

/*
 * Values for the mips bus space tag, not to be used directly by MI code.
 */
#define	MIPS_BUS_SPACE_IO	0	/* space is i/o space */
#define	MIPS_BUS_SPACE_MEM	1	/* space is mem space */


#define	BUS_SPACE_MAXSIZE_24BIT	0xFFFFFF
#define	BUS_SPACE_MAXSIZE_32BIT 0xFFFFFFFF
#define	BUS_SPACE_MAXSIZE	0xFFFFFFFF /* Maximum supported size */
#define	BUS_SPACE_MAXADDR_24BIT	0xFFFFFF
#define	BUS_SPACE_MAXADDR_32BIT 0xFFFFFFFF
#define	BUS_SPACE_MAXADDR	0xFFFFFFFF

#define	BUS_SPACE_UNRESTRICTED	(~0)

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

void	bus_space_unmap(bus_space_tag_t t, bus_space_handle_t bsh,
	    bus_size_t size);

/*
 * Get a new handle for a subregion of an already-mapped area of bus space.
 */

int	bus_space_subregion(bus_space_tag_t t, bus_space_handle_t bsh,
	    bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp);

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

	if (tag == MIPS_BUS_SPACE_IO)
		return (inb(handle + offset));
	return (readb(handle + offset));
}

static __inline u_int16_t
bus_space_read_2(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_size_t offset)
{

	if (tag == MIPS_BUS_SPACE_IO)
		return (inw(handle + offset));
	return (readw(handle + offset));
}

static __inline u_int32_t
bus_space_read_4(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_size_t offset)
{

	if (tag == MIPS_BUS_SPACE_IO)
		return (inl(handle + offset));
	return (readl(handle + offset));
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

	if (tag == MIPS_BUS_SPACE_IO)
		while (count--)
			*addr++ = inb(bsh + offset);
	else
	while (count--)
		*addr++ = readb(bsh + offset);
}

static __inline void
bus_space_read_multi_2(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int16_t *addr, size_t count)
{
	bus_addr_t baddr = bsh + offset;

	if (tag == MIPS_BUS_SPACE_IO)
		while (count--)
			*addr++ = inw(baddr);
	else
		while (count--)
			*addr++ = readw(baddr);
}

static __inline void
bus_space_read_multi_4(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int32_t *addr, size_t count)
{
	bus_addr_t baddr = bsh + offset;

	if (tag == MIPS_BUS_SPACE_IO)
		while (count--)
			*addr++ = inl(baddr);
	else
		while (count--)
			*addr++ = readl(baddr);
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
	bus_addr_t baddr = bsh + offset;

	if (tag == MIPS_BUS_SPACE_IO)
		while (count--) {
			*addr++ = inb(baddr);
			baddr += 1;
		}
	else
		while (count--) {
			*addr++ = readb(baddr);
			baddr += 1;
		}
}

static __inline void
bus_space_read_region_2(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int16_t *addr, size_t count)
{
	bus_addr_t baddr = bsh + offset;

	if (tag == MIPS_BUS_SPACE_IO)
		while (count--) {
			*addr++ = inw(baddr);
			baddr += 2;
		}
	else
		while (count--) {
			*addr++ = readw(baddr);
			baddr += 2;
		}
}

static __inline void
bus_space_read_region_4(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int32_t *addr, size_t count)
{
	bus_addr_t baddr = bsh + offset;

	if (tag == MIPS_BUS_SPACE_IO)
		while (count--) {
			*addr++ = inl(baddr);
			baddr += 4;
		}
	else
		while (count--) {
			*addr++ = readb(baddr);
			baddr += 4;
		}
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

	if (tag == MIPS_BUS_SPACE_IO)
		outb(bsh + offset, value);
	else
		writeb(bsh + offset, value);
}

static __inline void
bus_space_write_2(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int16_t value)
{

	if (tag == MIPS_BUS_SPACE_IO)
		outw(bsh + offset, value);
	else
		writew(bsh + offset, value);
}

static __inline void
bus_space_write_4(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int32_t value)
{

	if (tag == MIPS_BUS_SPACE_IO)
		outl(bsh + offset, value);
	else
		writel(bsh + offset, value);
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
	bus_addr_t baddr = bsh + offset;

	if (tag == MIPS_BUS_SPACE_IO)
		while (count--)
			outb(baddr, *addr++);
	else
		while (count--)
			writeb(baddr, *addr++);
}

static __inline void
bus_space_write_multi_2(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, const u_int16_t *addr, size_t count)
{
	bus_addr_t baddr = bsh + offset;

	if (tag == MIPS_BUS_SPACE_IO)
		while (count--)
			outw(baddr, *addr++);
	else
		while (count--)
			writew(baddr, *addr++);
}

static __inline void
bus_space_write_multi_4(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, const u_int32_t *addr, size_t count)
{
	bus_addr_t baddr = bsh + offset;

	if (tag == MIPS_BUS_SPACE_IO)
		while (count--)
			outl(baddr, *addr++);
	else
		while (count--)
			writel(baddr, *addr++);
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
	bus_addr_t baddr = bsh + offset;

	if (tag == MIPS_BUS_SPACE_IO)
		while (count--) {
			outb(baddr, *addr++);
			baddr += 1;
		}
	else
		while (count--) {
			writeb(baddr, *addr++);
			baddr += 1;
		}
}

static __inline void
bus_space_write_region_2(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, const u_int16_t *addr, size_t count)
{
	bus_addr_t baddr = bsh + offset;

	if (tag == MIPS_BUS_SPACE_IO)
		while (count--) {
			outw(baddr, *addr++);
			baddr += 2;
		}
	else
		while (count--) {
			writew(baddr, *addr++);
			baddr += 2;
		}
}

static __inline void
bus_space_write_region_4(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, const u_int32_t *addr, size_t count)
{
	bus_addr_t baddr = bsh + offset;

	if (tag == MIPS_BUS_SPACE_IO)
		while (count--) {
			outl(baddr, *addr++);
			baddr += 4;
		}
	else
		while (count--) {
			writel(baddr, *addr++);
			baddr += 4;
		}
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

	if (tag == MIPS_BUS_SPACE_IO)
		while (count--)
			outb(addr, value);
	else
		while (count--)
			writeb(addr, value);
}

static __inline void
bus_space_set_multi_2(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int16_t value, size_t count)
{
	bus_addr_t addr = bsh + offset;

	if (tag == MIPS_BUS_SPACE_IO)
		while (count--)
			outw(addr, value);
	else
		while (count--)
			writew(addr, value);
}

static __inline void
bus_space_set_multi_4(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int32_t value, size_t count)
{
	bus_addr_t addr = bsh + offset;

	if (tag == MIPS_BUS_SPACE_IO)
		while (count--)
			outl(addr, value);
	else
		while (count--)
			writel(addr, value);
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

	if (tag == MIPS_BUS_SPACE_IO)
		for (; count != 0; count--, addr++)
			outb(addr, value);
	else
		for (; count != 0; count--, addr++)
			writeb(addr, value);
}

static __inline void
bus_space_set_region_2(bus_space_tag_t tag, bus_space_handle_t bsh,
		       bus_size_t offset, u_int16_t value, size_t count)
{
	bus_addr_t addr = bsh + offset;

	if (tag == MIPS_BUS_SPACE_IO)
		for (; count != 0; count--, addr += 2)
			outw(addr, value);
	else
		for (; count != 0; count--, addr += 2)
			writew(addr, value);
}

static __inline void
bus_space_set_region_4(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, u_int32_t value, size_t count)
{
	bus_addr_t addr = bsh + offset;

	if (tag == MIPS_BUS_SPACE_IO)
		for (; count != 0; count--, addr += 4)
			outl(addr, value);
	else
		for (; count != 0; count--, addr += 4)
			writel(addr, value);
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

	if (tag == MIPS_BUS_SPACE_IO)
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
	} else {
		if (addr1 >= addr2) {
			/* src after dest: copy forward */
			for (; count != 0; count--, addr1++, addr2++)
				writeb(addr2, readb(addr1));
		} else {
			/* dest after src: copy backwards */
			for (addr1 += (count - 1), addr2 += (count - 1);
			    count != 0; count--, addr1--, addr2--)
				writeb(addr2, readb(addr1));
		}
	}
}

static __inline void
bus_space_copy_region_2(bus_space_tag_t tag, bus_space_handle_t bsh1,
    bus_size_t off1, bus_space_handle_t bsh2,
    bus_size_t off2, size_t count)
{
	bus_addr_t addr1 = bsh1 + off1;
	bus_addr_t addr2 = bsh2 + off2;

	if (tag == MIPS_BUS_SPACE_IO)
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
	} else {
		if (addr1 >= addr2) {
			/* src after dest: copy forward */
			for (; count != 0; count--, addr1 += 2, addr2 += 2)
				writew(addr2, readw(addr1));
		} else {
			/* dest after src: copy backwards */
			for (addr1 += 2 * (count - 1), addr2 += 2 * (count - 1);
			    count != 0; count--, addr1 -= 2, addr2 -= 2)
				writew(addr2, readw(addr1));
		}
	}
}

static __inline void
bus_space_copy_region_4(bus_space_tag_t tag, bus_space_handle_t bsh1,
    bus_size_t off1, bus_space_handle_t bsh2,
    bus_size_t off2, size_t count)
{
	bus_addr_t addr1 = bsh1 + off1;
	bus_addr_t addr2 = bsh2 + off2;

	if (tag == MIPS_BUS_SPACE_IO)
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
	} else {
		if (addr1 >= addr2) {
			/* src after dest: copy forward */
			for (; count != 0; count--, addr1 += 4, addr2 += 4)
				writel(addr2, readl(addr1));
		} else {
			/* dest after src: copy backwards */
			for (addr1 += 4 * (count - 1), addr2 += 4 * (count - 1);
			    count != 0; count--, addr1 -= 4, addr2 -= 4)
				writel(addr2, readl(addr1));
		}
	}
}


#if 0	/* Cause a link error for bus_space_copy_8 */
#define	bus_space_copy_region_8	!!! bus_space_copy_region_8 unimplemented !!!
#endif


/*
 * Bus read/write barrier methods.
 *
 *	void bus_space_barrier(bus_space_tag_t tag, bus_space_handle_t bsh,
 *			       bus_size_t offset, bus_size_t len, int flags);
 *
 *
 * Note that BUS_SPACE_BARRIER_WRITE doesn't do anything other than
 * prevent reordering by the compiler; all Intel x86 processors currently
 * retire operations outside the CPU in program order.
 */
#define	BUS_SPACE_BARRIER_READ	0x01		/* force read barrier */
#define	BUS_SPACE_BARRIER_WRITE	0x02		/* force write barrier */

static __inline void
bus_space_barrier(bus_space_tag_t tag __unused, bus_space_handle_t bsh __unused,
		  bus_size_t offset __unused, bus_size_t len __unused, int flags)
{
#if 0
#ifdef __GNUCLIKE_ASM
	if (flags & BUS_SPACE_BARRIER_READ)
		__asm __volatile("lock; addl $0,0(%%rsp)" : : : "memory");
	else
		__asm __volatile("" : : : "memory");
#endif
#endif
}

#ifdef BUS_SPACE_NO_LEGACY
#undef inb
#undef outb
#define inb(a) compiler_error
#define inw(a) compiler_error
#define inl(a) compiler_error
#define outb(a, b) compiler_error
#define outw(a, b) compiler_error
#define outl(a, b) compiler_error
#endif

#include <machine/bus_dma.h>

/*
 * Stream accesses are the same as normal accesses on amd64; there are no
 * supported bus systems with an endianess different from the host one.
 */
#define	bus_space_read_stream_1(t, h, o)	bus_space_read_1((t), (h), (o))
#define	bus_space_read_stream_2(t, h, o)	bus_space_read_2((t), (h), (o))
#define	bus_space_read_stream_4(t, h, o)	bus_space_read_4((t), (h), (o))

#define	bus_space_read_multi_stream_1(t, h, o, a, c) \
	bus_space_read_multi_1((t), (h), (o), (a), (c))
#define	bus_space_read_multi_stream_2(t, h, o, a, c) \
	bus_space_read_multi_2((t), (h), (o), (a), (c))
#define	bus_space_read_multi_stream_4(t, h, o, a, c) \
	bus_space_read_multi_4((t), (h), (o), (a), (c))

#define	bus_space_write_stream_1(t, h, o, v) \
	bus_space_write_1((t), (h), (o), (v))
#define	bus_space_write_stream_2(t, h, o, v) \
	bus_space_write_2((t), (h), (o), (v))
#define	bus_space_write_stream_4(t, h, o, v) \
	bus_space_write_4((t), (h), (o), (v))

#define	bus_space_write_multi_stream_1(t, h, o, a, c) \
	bus_space_write_multi_1((t), (h), (o), (a), (c))
#define	bus_space_write_multi_stream_2(t, h, o, a, c) \
	bus_space_write_multi_2((t), (h), (o), (a), (c))
#define	bus_space_write_multi_stream_4(t, h, o, a, c) \
	bus_space_write_multi_4((t), (h), (o), (a), (c))

#define	bus_space_set_multi_stream_1(t, h, o, v, c) \
	bus_space_set_multi_1((t), (h), (o), (v), (c))
#define	bus_space_set_multi_stream_2(t, h, o, v, c) \
	bus_space_set_multi_2((t), (h), (o), (v), (c))
#define	bus_space_set_multi_stream_4(t, h, o, v, c) \
	bus_space_set_multi_4((t), (h), (o), (v), (c))

#define	bus_space_read_region_stream_1(t, h, o, a, c) \
	bus_space_read_region_1((t), (h), (o), (a), (c))
#define	bus_space_read_region_stream_2(t, h, o, a, c) \
	bus_space_read_region_2((t), (h), (o), (a), (c))
#define	bus_space_read_region_stream_4(t, h, o, a, c) \
	bus_space_read_region_4((t), (h), (o), (a), (c))

#define	bus_space_write_region_stream_1(t, h, o, a, c) \
	bus_space_write_region_1((t), (h), (o), (a), (c))
#define	bus_space_write_region_stream_2(t, h, o, a, c) \
	bus_space_write_region_2((t), (h), (o), (a), (c))
#define	bus_space_write_region_stream_4(t, h, o, a, c) \
	bus_space_write_region_4((t), (h), (o), (a), (c))

#define	bus_space_set_region_stream_1(t, h, o, v, c) \
	bus_space_set_region_1((t), (h), (o), (v), (c))
#define	bus_space_set_region_stream_2(t, h, o, v, c) \
	bus_space_set_region_2((t), (h), (o), (v), (c))
#define	bus_space_set_region_stream_4(t, h, o, v, c) \
	bus_space_set_region_4((t), (h), (o), (v), (c))

#define	bus_space_copy_region_stream_1(t, h1, o1, h2, o2, c) \
	bus_space_copy_region_1((t), (h1), (o1), (h2), (o2), (c))
#define	bus_space_copy_region_stream_2(t, h1, o1, h2, o2, c) \
	bus_space_copy_region_2((t), (h1), (o1), (h2), (o2), (c))
#define	bus_space_copy_region_stream_4(t, h1, o1, h2, o2, c) \
	bus_space_copy_region_4((t), (h1), (o1), (h2), (o2), (c))

#endif /* !TARGET_OCTEON */
#endif /* !_MACHINE_BUS_H_ */
