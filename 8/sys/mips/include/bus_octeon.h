/*-
 * Copyright (c) 2006 Oleksandr Tymoshenko.
 * Copyright (c) KATO Takenori, 1999.
 *
 * All rights reserved.  Unpublished rights reserved under the copyright
 * laws of Japan.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
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
 */

#ifndef _MIPS_BUS_OCTEON_H_
#define _MIPS_BUS_OCTEON_H_

#include "../../mips32/octeon32/octeon_pcmap_regs.h"
#include <machine/_bus_octeon.h>
#include <machine/cpufunc.h>

/*
 * Values for the mips64 bus space tag, not to be used directly by MI code.
 */
#define	MIPS_BUS_SPACE_IO	0	/* space is i/o space */
#define MIPS_BUS_SPACE_MEM	1	/* space is mem space */

#define BUS_SPACE_MAXSIZE_24BIT	0xFFFFFF
#define BUS_SPACE_MAXSIZE_32BIT 0xFFFFFFFF
#define BUS_SPACE_MAXSIZE	0xFFFFFFFF
#define BUS_SPACE_MAXADDR_24BIT	0xFFFFFF
#define BUS_SPACE_MAXADDR_32BIT 0xFFFFFFFF
#define BUS_SPACE_MAXADDR	0xFFFFFFFF

#define BUS_SPACE_UNRESTRICTED	(~0)

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

/*
 * Get a new handle for a subregion of an already-mapped area of bus space.
 */

static __inline int bus_space_subregion(bus_space_tag_t t,
					bus_space_handle_t bsh,
					bus_size_t offset, bus_size_t size,
					bus_space_handle_t *nbshp);

static __inline int
bus_space_subregion(bus_space_tag_t t __unused, bus_space_handle_t bsh,
		    bus_size_t offset, bus_size_t size __unused,
		    bus_space_handle_t *nbshp)
{
	*nbshp = bsh + offset;
	return (0);
}

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

static __inline void bus_space_free(bus_space_tag_t t, bus_space_handle_t bsh,
				    bus_size_t size);

static __inline void
bus_space_free(bus_space_tag_t t __unused, bus_space_handle_t bsh __unused,
	       bus_size_t size __unused)
{
}


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
    	uint64_t ret_val;
        uint64_t oct64_addr;

        oct64_addr = handle + offset;
        ret_val = oct_read8(oct64_addr);
    	return ((u_int8_t) ret_val);
}

static __inline u_int16_t
bus_space_read_2(bus_space_tag_t tag, bus_space_handle_t handle,
		 bus_size_t offset)
{
    	uint64_t ret_val;
        uint64_t oct64_addr;

        oct64_addr = handle + offset;
        ret_val = oct_read16(oct64_addr);
    	return ((u_int16_t) ret_val);
}

static __inline u_int32_t
bus_space_read_4(bus_space_tag_t tag, bus_space_handle_t handle,
		 bus_size_t offset)
{
    	uint64_t ret_val;
        uint64_t oct64_addr;

        oct64_addr = handle + offset;
        ret_val = oct_read32(oct64_addr);
    	return ((u_int32_t) ret_val);
}


static __inline u_int64_t
bus_space_read_8(bus_space_tag_t tag, bus_space_handle_t handle,
		 bus_size_t offset)
{
    	uint64_t ret_val;
        uint64_t oct64_addr;

        oct64_addr = handle + offset;
        ret_val = oct_read64(oct64_addr);
    	return (ret_val);
}


/*
 * Read `count' 1, 2, 4, or 8 byte quantities from bus space
 * described by tag/handle/offset and copy into buffer provided.
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
	uint64_t ptr = ((uint64_t) bsh + (uint64_t) offset);

	for(; count > 0; count--, addr++, ptr++) {
		*addr = oct_read8(ptr);
	}
}

static __inline void
bus_space_read_region_2(bus_space_tag_t tag, bus_space_handle_t bsh,
		       bus_size_t offset, u_int16_t *addr, size_t count)
{
	uint64_t ptr = ((uint64_t) bsh + (uint64_t) offset);

	for(; count > 0; count--, addr++, ptr+=2) {
		*addr = oct_read16(ptr);
	}
}

static __inline void
bus_space_read_region_4(bus_space_tag_t tag, bus_space_handle_t bsh,
		       bus_size_t offset, u_int32_t *addr, size_t count)
{
	uint64_t ptr = ((uint64_t) bsh + (uint64_t) offset);

	for(; count > 0; count--, addr++, ptr+=4) {
		*addr = oct_read32(ptr);
	}
}

static __inline void
bus_space_read_region_8(bus_space_tag_t tag, bus_space_handle_t bsh,
		       bus_size_t offset, u_int64_t *addr, size_t count)
{
	uint64_t ptr = ((uint64_t) bsh + (uint64_t) offset);

	for(; count > 0; count--, addr++, ptr+=4) {
		*addr = oct_read64(ptr);
	}
}

/*
 * Read `count' 1, 2, 4, or 8 byte quantities from bus space
 * described by tag/handle and starting at `offset' and copy into
 * buffer provided.
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
	uint64_t ptr = ((uint64_t) bsh + (uint64_t) offset);

	for(; count > 0; count--, addr++) {
		*addr = oct_read8(ptr);
	}
}

static __inline void
bus_space_read_multi_2(bus_space_tag_t tag, bus_space_handle_t bsh,
			bus_size_t offset, u_int16_t *addr, size_t count)
{
	uint64_t ptr = ((uint64_t) bsh + (uint64_t) offset);

	for(; count > 0; count--, addr++) {
		*addr = oct_read16(ptr);
	}
}

static __inline void
bus_space_read_multi_4(bus_space_tag_t tag, bus_space_handle_t bsh,
			bus_size_t offset, u_int32_t *addr, size_t count)
{
	uint64_t ptr = ((uint64_t) bsh + (uint64_t) offset);

	for(; count > 0; count--, addr++) {
		*addr = oct_read32(ptr);
	}
}

static __inline void
bus_space_read_multi_8(bus_space_tag_t tag, bus_space_handle_t bsh,
			bus_size_t offset, u_int64_t *addr, size_t count)
{
	uint64_t ptr = ((uint64_t) bsh + (uint64_t) offset);

	for(; count > 0; count--, addr++) {
		*addr = oct_read64(ptr);
	}
}


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
      oct_write8(bsh+offset, value);
}

static __inline void
bus_space_write_2(bus_space_tag_t tag, bus_space_handle_t bsh,
		       bus_size_t offset, u_int16_t value)
{
        oct_write16(bsh+offset, value);
}

static __inline void
bus_space_write_4(bus_space_tag_t tag, bus_space_handle_t bsh,
		       bus_size_t offset, u_int32_t value)
{
        oct_write32(bsh+offset, value);
}

static __inline void
bus_space_write_8(bus_space_tag_t tag, bus_space_handle_t bsh,
		       bus_size_t offset, u_int64_t value)
{
        oct_write64(bsh+offset, value);
}

/*
 * Write `count' 1, 2, 4, or 8 byte quantities from the buffer
 * provided to bus space described by tag/handle/offset.
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
	uint64_t ptr = ((uint64_t) bsh + (uint64_t) offset);

	for(; count > 0; count--, addr++, ptr++) {
            	oct_write8(ptr, *addr);
	}
}

static __inline void
bus_space_write_region_2(bus_space_tag_t tag, bus_space_handle_t bsh,
			bus_size_t offset, const u_int16_t *addr, size_t count)
{
	uint64_t ptr = ((uint64_t) bsh + (uint64_t) offset);

	for(; count > 0; count--, addr++, ptr++) {
            	oct_write16(ptr, *addr);
	}
}

static __inline void
bus_space_write_region_4(bus_space_tag_t tag, bus_space_handle_t bsh,
			bus_size_t offset, const u_int32_t *addr, size_t count)
{
	uint64_t ptr = ((uint64_t) bsh + (uint64_t) offset);

	for(; count > 0; count--, addr++, ptr++) {
            	oct_write32(ptr, *addr);
	}
}

static __inline void
bus_space_write_region_8(bus_space_tag_t tag, bus_space_handle_t bsh,
			bus_size_t offset, const u_int64_t *addr, size_t count)
{
	uint64_t ptr = ((uint64_t) bsh + (uint64_t) offset);

	for(; count > 0; count--, addr++, ptr++) {
            	oct_write64(ptr, *addr);
	}
}

/*
 * Write `count' 1, 2, 4, or 8 byte quantities from the buffer provided
 * to bus space described by tag/handle starting at `offset'.
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
	uint64_t ptr = ((uint64_t) bsh + (uint64_t) offset);

	for(; count > 0; count--, addr++) {
            	oct_write8(ptr, *addr);
	}
}

static __inline void
bus_space_write_multi_2(bus_space_tag_t tag, bus_space_handle_t bsh,
			 bus_size_t offset, const u_int16_t *addr, size_t count)
{
	uint64_t ptr = ((uint64_t) bsh + (uint64_t) offset);

	for(; count > 0; count--, addr++) {
            	oct_write16(ptr, *addr);
	}
}

static __inline void
bus_space_write_multi_4(bus_space_tag_t tag, bus_space_handle_t bsh,
			 bus_size_t offset, const u_int32_t *addr, size_t count)
{
	uint64_t ptr = ((uint64_t) bsh + (uint64_t) offset);

	for(; count > 0; count--, addr++) {
            	oct_write32(ptr, *addr);
	}
}

static __inline void
bus_space_write_multi_8(bus_space_tag_t tag, bus_space_handle_t bsh,
			 bus_size_t offset, const u_int64_t *addr, size_t count)
{
	uint64_t ptr = ((uint64_t) bsh + (uint64_t) offset);

	for(; count > 0; count--, addr++) {
            	oct_write64(ptr, *addr);
	}
}

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
	uint64_t ptr = ((uint64_t) bsh + (uint64_t) offset);

	for(; count > 0; count--) {
            	oct_write8(ptr, value);
	}
}

static __inline void
bus_space_set_multi_2(bus_space_tag_t tag, bus_space_handle_t bsh,
		     bus_size_t offset, u_int16_t value, size_t count)
{
	uint64_t ptr = ((uint64_t) bsh + (uint64_t) offset);

	for(; count > 0; count--) {
            	oct_write16(ptr, value);
	}
}

static __inline void
bus_space_set_multi_4(bus_space_tag_t tag, bus_space_handle_t bsh,
		      bus_size_t offset, u_int32_t value, size_t count)
{
	uint64_t ptr = ((uint64_t) bsh + (uint64_t) offset);

	for(; count > 0; count--) {
            	oct_write32(ptr, value);
	}
}

static __inline void
bus_space_set_multi_8(bus_space_tag_t tag, bus_space_handle_t bsh,
		      bus_size_t offset, u_int64_t value, size_t count)
{
	uint64_t ptr = ((uint64_t) bsh + (uint64_t) offset);

	for(; count > 0; count--) {
            	oct_write64(ptr, value);
	}
}

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
	uint64_t ptr = ((uint64_t) bsh + (uint64_t) offset);

	for(; count > 0; count--, ptr++) {
		oct_write8(ptr, value);
	}
}

static __inline void
bus_space_set_region_2(bus_space_tag_t tag, bus_space_handle_t bsh,
		       bus_size_t offset, u_int16_t value, size_t count)
{
	uint64_t ptr = ((uint64_t) bsh + (uint64_t) offset);

	for(; count > 0; count--, ptr++) {
		oct_write16(ptr, value);
	}
}

static __inline void
bus_space_set_region_4(bus_space_tag_t tag, bus_space_handle_t bsh,
		       bus_size_t offset, u_int32_t value, size_t count)
{
	uint64_t ptr = ((uint64_t) bsh + (uint64_t) offset);

	for(; count > 0; count--, ptr++) {
		oct_write32(ptr, value);
	}
}

static __inline void
bus_space_set_region_8(bus_space_tag_t tag, bus_space_handle_t bsh,
		       bus_size_t offset, u_int64_t value, size_t count)
{
	uint64_t ptr = ((uint64_t) bsh + (uint64_t) offset);

	for(; count > 0; count--, ptr++) {
		oct_write64(ptr, value);
	}
}

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
	uint64_t ptr1 = ((uint64_t) bsh1 + (uint64_t) off1);
	uint64_t ptr2 = ((uint64_t) bsh2 + (uint64_t) off2);
        uint8_t val;

	for(; count > 0; count--, ptr1++, ptr2++) {
            	val = oct_read8(ptr1);
            	oct_write8(ptr2, val);
	}
}

static __inline void
bus_space_copy_region_2(bus_space_tag_t tag, bus_space_handle_t bsh1,
			bus_size_t off1, bus_space_handle_t bsh2,
			bus_size_t off2, size_t count)
{
	uint64_t ptr1 = ((uint64_t) bsh1 + (uint64_t) off1);
	uint64_t ptr2 = ((uint64_t) bsh2 + (uint64_t) off2);
        uint16_t val;

	for(; count > 0; count--, ptr1++, ptr2++) {
            	val = oct_read16(ptr1);
            	oct_write16(ptr2, val);
	}
}

static __inline void
bus_space_copy_region_4(bus_space_tag_t tag, bus_space_handle_t bsh1,
			bus_size_t off1, bus_space_handle_t bsh2,
			bus_size_t off2, size_t count)
{
	uint64_t ptr1 = ((uint64_t) bsh1 + (uint64_t) off1);
	uint64_t ptr2 = ((uint64_t) bsh2 + (uint64_t) off2);
        uint32_t val;

	for(; count > 0; count--, ptr1++, ptr2++) {
            	val = oct_read32(ptr1);
            	oct_write32(ptr2, val);
	}
}

static __inline void
bus_space_copy_region_8(bus_space_tag_t tag, bus_space_handle_t bsh1,
			bus_size_t off1, bus_space_handle_t bsh2,
			bus_size_t off2, size_t count)
{
	uint64_t ptr1 = ((uint64_t) bsh1 + (uint64_t) off1);
	uint64_t ptr2 = ((uint64_t) bsh2 + (uint64_t) off2);
        uint64_t val;

	for(; count > 0; count--, ptr1++, ptr2++) {
            	val = oct_read64(ptr1);
            	oct_write64(ptr2, val);
	}
}

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
    oct_read64(OCTEON_MIO_BOOT_BIST_STAT);
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

#endif /* _MIPS_BUS_OCTEON_H_ */
