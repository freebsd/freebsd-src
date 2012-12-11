/*-
 * Copyright (c) 2012 Intel Corporation
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
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_BUS_H_
#define _MACHINE_BUS_H_

#include <sys/systm.h>
#include <x86/bus.h>

#define KASSERT_BUS_SPACE_MEM_ONLY(tag)                     \
	KASSERT((tag) == X86_BUS_SPACE_MEM,                 \
	    ("%s: can only handle mem space", __func__))

static __inline uint64_t
bus_space_read_8(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t ofs)
{

	KASSERT_BUS_SPACE_MEM_ONLY(tag);

	return (*(volatile uint64_t *)(bsh + ofs));
}

static __inline void
bus_space_write_8(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t ofs, uint64_t val)
{

	KASSERT_BUS_SPACE_MEM_ONLY(tag);

	*(volatile uint64_t *)(bsh + ofs) = val;
}

static __inline void
bus_space_read_region_8(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t ofs, uint64_t *bufp, size_t count)
{
	volatile uint64_t *bsp;

	KASSERT_BUS_SPACE_MEM_ONLY(tag);

	bsp = (void *)(bsh + ofs);
	while (count-- > 0)
		*bufp++ = *bsp++;
}

static __inline void
bus_space_write_region_8(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t ofs, uint64_t const *bufp, size_t count)
{
	volatile uint64_t *bsp;

	KASSERT_BUS_SPACE_MEM_ONLY(tag);

	bsp = (void *)(bsh + ofs);
	while (count-- > 0)
		*bsp++ = *bufp++;
}

static __inline void
bus_space_set_region_8(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t ofs, uint64_t val, size_t count)
{
	volatile uint64_t *bsp;

	KASSERT_BUS_SPACE_MEM_ONLY(tag);

	bsp = (void *)(bsh + ofs);
	while (count-- > 0)
		*bsp++ = val;
}

static __inline void
bus_space_copy_region_8(bus_space_tag_t tag, bus_space_handle_t sbsh,
    bus_size_t sofs, bus_space_handle_t dbsh, bus_size_t dofs, size_t count)
{
	volatile uint64_t *dst, *src;

	KASSERT_BUS_SPACE_MEM_ONLY(tag);

	src = (void *)(sbsh + sofs);
	dst = (void *)(dbsh + dofs);
	if (src < dst) {
		src += count - 1;
		dst += count - 1;
		while (count-- > 0)
			*dst-- = *src--;
	} else {
		while (count-- > 0)
			*dst++ = *src++;
	}
}

static __inline void
bus_space_read_multi_8(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t ofs, uint64_t *bufp, size_t count)
{

	KASSERT_BUS_SPACE_MEM_ONLY(tag);

	while (count-- > 0)
		*bufp++ = *(volatile uint64_t *)(bsh + ofs);
}

static __inline void
bus_space_write_multi_8(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t ofs, uint64_t const *bufp, size_t count)
{

	KASSERT_BUS_SPACE_MEM_ONLY(tag);

	while (count-- > 0)
		*(volatile uint64_t *)(bsh + ofs) = *bufp++;
}

static __inline void
bus_space_set_multi_8(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t ofs, uint64_t val, size_t count)
{

	KASSERT_BUS_SPACE_MEM_ONLY(tag);

	while (count-- > 0)
		*(volatile uint64_t *)(bsh + ofs) = val;
}

#endif /*_MACHINE_BUS_H_*/
