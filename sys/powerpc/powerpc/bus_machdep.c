/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2006 Semihalf, Rafal Jaworowski <raj@semihalf.com>
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

#include <sys/cdefs.h>
#define	KTR_BE_IO	0
#define	KTR_LE_IO	0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/ktr.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <sys/endian.h>

#include <machine/bus.h>
#include <machine/pio.h>
#include <machine/md_var.h>

#define TODO panic("%s: not implemented", __func__)

#define	MAX_EARLYBOOT_MAPPINGS	6

static struct {
	vm_offset_t virt;
	bus_addr_t addr;
	bus_size_t size;
	int flags;
} earlyboot_mappings[MAX_EARLYBOOT_MAPPINGS];
static int earlyboot_map_idx = 0;

void bs_remap_earlyboot(void);

static __inline void *
__ppc_ba(bus_space_handle_t bsh, bus_size_t ofs)
{
	return ((void *)(bsh + ofs));
}

static int
bs_gen_map(bus_addr_t addr, bus_size_t size, int flags,
    bus_space_handle_t *bshp)
{
	vm_memattr_t ma;

	/*
	 * Record what we did if we haven't enabled the MMU yet. We
	 * will need to remap it as soon as the MMU comes up.
	 */
	if (!pmap_bootstrapped) {
		KASSERT(earlyboot_map_idx < MAX_EARLYBOOT_MAPPINGS,
		    ("%s: too many early boot mapping requests", __func__));
		earlyboot_mappings[earlyboot_map_idx].addr = addr;
		earlyboot_mappings[earlyboot_map_idx].virt =
		    pmap_early_io_map(addr, size);
		earlyboot_mappings[earlyboot_map_idx].size = size;
		earlyboot_mappings[earlyboot_map_idx].flags = flags;
		*bshp = earlyboot_mappings[earlyboot_map_idx].virt;
		earlyboot_map_idx++;
	} else {
		ma = VM_MEMATTR_DEFAULT;
		switch (flags) {
			case BUS_SPACE_MAP_CACHEABLE:
				ma = VM_MEMATTR_CACHEABLE;
				break;
			case BUS_SPACE_MAP_PREFETCHABLE:
				ma = VM_MEMATTR_PREFETCHABLE;
				break;
		}
		*bshp = (bus_space_handle_t)pmap_mapdev_attr(addr, size, ma);
	}

	return (0);
}

void
bs_remap_earlyboot(void)
{
	vm_paddr_t pa, spa;
	vm_offset_t va;
	int i;
	vm_memattr_t ma;

	for (i = 0; i < earlyboot_map_idx; i++) {
		spa = earlyboot_mappings[i].addr;

		if (hw_direct_map &&
		   PHYS_TO_DMAP(spa) == earlyboot_mappings[i].virt &&
		   pmap_dev_direct_mapped(spa, earlyboot_mappings[i].size) == 0)
			continue;

		ma = VM_MEMATTR_DEFAULT;
		switch (earlyboot_mappings[i].flags) {
			case BUS_SPACE_MAP_CACHEABLE:
				ma = VM_MEMATTR_CACHEABLE;
				break;
			case BUS_SPACE_MAP_PREFETCHABLE:
				ma = VM_MEMATTR_PREFETCHABLE;
				break;
		}

		pa = trunc_page(spa);
		va = trunc_page(earlyboot_mappings[i].virt);
		while (pa < spa + earlyboot_mappings[i].size) {
			pmap_kenter_attr(va, pa, ma);
			va += PAGE_SIZE;
			pa += PAGE_SIZE;
		}
	}
}

static void
bs_gen_unmap(bus_size_t size __unused)
{
}

static int
bs_gen_subregion(bus_space_handle_t bsh, bus_size_t ofs,
    bus_size_t size __unused, bus_space_handle_t *nbshp)
{
	*nbshp = bsh + ofs;
	return (0);
}

static int
bs_gen_alloc(bus_addr_t rstart __unused, bus_addr_t rend __unused,
    bus_size_t size __unused, bus_size_t alignment __unused,
    bus_size_t boundary __unused, int flags __unused,
    bus_addr_t *bpap __unused, bus_space_handle_t *bshp __unused)
{
	TODO;
}

static void
bs_gen_free(bus_space_handle_t bsh __unused, bus_size_t size __unused)
{
	TODO;
}

static void
bs_gen_barrier(bus_space_handle_t bsh __unused, bus_size_t ofs __unused,
    bus_size_t size __unused, int flags __unused)
{

	powerpc_iomb();
}

/*
 * Native-endian access functions
 */
static uint8_t
native_bs_rs_1(bus_space_handle_t bsh, bus_size_t ofs)
{
	volatile uint8_t *addr;
	uint8_t res;

	addr = __ppc_ba(bsh, ofs);
	res = *addr;
	powerpc_iomb();
	CTR4(KTR_BE_IO, "%s(bsh=%#x, ofs=%#x) = %#x", __func__, bsh, ofs, res);
	return (res);
}

static uint16_t
native_bs_rs_2(bus_space_handle_t bsh, bus_size_t ofs)
{
	volatile uint16_t *addr;
	uint16_t res;

	addr = __ppc_ba(bsh, ofs);
	res = *addr;
	powerpc_iomb();
	CTR4(KTR_BE_IO, "%s(bsh=%#x, ofs=%#x) = %#x", __func__, bsh, ofs, res);
	return (res);
}

static uint32_t
native_bs_rs_4(bus_space_handle_t bsh, bus_size_t ofs)
{
	volatile uint32_t *addr;
	uint32_t res;

	addr = __ppc_ba(bsh, ofs);
	res = *addr;
	powerpc_iomb();
	CTR4(KTR_BE_IO, "%s(bsh=%#x, ofs=%#x) = %#x", __func__, bsh, ofs, res);
	return (res);
}

static uint64_t
native_bs_rs_8(bus_space_handle_t bsh, bus_size_t ofs)
{
	volatile uint64_t *addr;
	uint64_t res;

	addr = __ppc_ba(bsh, ofs);
	res = *addr;
	powerpc_iomb();
	return (res);
}

static void
native_bs_rm_1(bus_space_handle_t bsh, bus_size_t ofs, uint8_t *addr, size_t cnt)
{
	ins8(__ppc_ba(bsh, ofs), addr, cnt);
}

static void
native_bs_rm_2(bus_space_handle_t bsh, bus_size_t ofs, uint16_t *addr, size_t cnt)
{
	ins16(__ppc_ba(bsh, ofs), addr, cnt);
}

static void
native_bs_rm_4(bus_space_handle_t bsh, bus_size_t ofs, uint32_t *addr, size_t cnt)
{
	ins32(__ppc_ba(bsh, ofs), addr, cnt);
}

static void
native_bs_rm_8(bus_space_handle_t bsh, bus_size_t ofs, uint64_t *addr, size_t cnt)
{
	ins64(__ppc_ba(bsh, ofs), addr, cnt);
}

static void
native_bs_rr_1(bus_space_handle_t bsh, bus_size_t ofs, uint8_t *addr, size_t cnt)
{
	volatile uint8_t *s = __ppc_ba(bsh, ofs);

	while (cnt--)
		*addr++ = *s++;
	powerpc_iomb();
}

static void
native_bs_rr_2(bus_space_handle_t bsh, bus_size_t ofs, uint16_t *addr, size_t cnt)
{
	volatile uint16_t *s = __ppc_ba(bsh, ofs);

	while (cnt--)
		*addr++ = *s++;
	powerpc_iomb();
}

static void
native_bs_rr_4(bus_space_handle_t bsh, bus_size_t ofs, uint32_t *addr, size_t cnt)
{
	volatile uint32_t *s = __ppc_ba(bsh, ofs);

	while (cnt--)
		*addr++ = *s++;
	powerpc_iomb();
}

static void
native_bs_rr_8(bus_space_handle_t bsh, bus_size_t ofs, uint64_t *addr, size_t cnt)
{
	volatile uint64_t *s = __ppc_ba(bsh, ofs);

	while (cnt--)
		*addr++ = *s++;
	powerpc_iomb();
}

static void
native_bs_ws_1(bus_space_handle_t bsh, bus_size_t ofs, uint8_t val)
{
	volatile uint8_t *addr;

	addr = __ppc_ba(bsh, ofs);
	*addr = val;
	powerpc_iomb();
	CTR4(KTR_BE_IO, "%s(bsh=%#x, ofs=%#x, val=%#x)", __func__, bsh, ofs, val);
}

static void
native_bs_ws_2(bus_space_handle_t bsh, bus_size_t ofs, uint16_t val)
{
	volatile uint16_t *addr;

	addr = __ppc_ba(bsh, ofs);
	*addr = val;
	powerpc_iomb();
	CTR4(KTR_BE_IO, "%s(bsh=%#x, ofs=%#x, val=%#x)", __func__, bsh, ofs, val);
}

static void
native_bs_ws_4(bus_space_handle_t bsh, bus_size_t ofs, uint32_t val)
{
	volatile uint32_t *addr;

	addr = __ppc_ba(bsh, ofs);
	*addr = val;
	powerpc_iomb();
	CTR4(KTR_BE_IO, "%s(bsh=%#x, ofs=%#x, val=%#x)", __func__, bsh, ofs, val);
}

static void
native_bs_ws_8(bus_space_handle_t bsh, bus_size_t ofs, uint64_t val)
{
	volatile uint64_t *addr;

	addr = __ppc_ba(bsh, ofs);
	*addr = val;
	powerpc_iomb();
	CTR4(KTR_BE_IO, "%s(bsh=%#x, ofs=%#x, val=%#x)", __func__, bsh, ofs, val);
}

static void
native_bs_wm_1(bus_space_handle_t bsh, bus_size_t ofs, const uint8_t *addr,
    bus_size_t cnt)
{
	outsb(__ppc_ba(bsh, ofs), addr, cnt);
}

static void
native_bs_wm_2(bus_space_handle_t bsh, bus_size_t ofs, const uint16_t *addr,
    bus_size_t cnt)
{
	outsw(__ppc_ba(bsh, ofs), addr, cnt);
}

static void
native_bs_wm_4(bus_space_handle_t bsh, bus_size_t ofs, const uint32_t *addr,
    bus_size_t cnt)
{
	outsl(__ppc_ba(bsh, ofs), addr, cnt);
}

static void
native_bs_wm_8(bus_space_handle_t bsh, bus_size_t ofs, const uint64_t *addr,
    bus_size_t cnt)
{
	outsll(__ppc_ba(bsh, ofs), addr, cnt);
}

static void
native_bs_wr_1(bus_space_handle_t bsh, bus_size_t ofs, const uint8_t *addr,
    size_t cnt)
{
	volatile uint8_t *d = __ppc_ba(bsh, ofs);

	while (cnt--)
		*d++ = *addr++;
	powerpc_iomb();
}

static void
native_bs_wr_2(bus_space_handle_t bsh, bus_size_t ofs, const uint16_t *addr,
    size_t cnt)
{
	volatile uint16_t *d = __ppc_ba(bsh, ofs);

	while (cnt--)
		*d++ = *addr++;
	powerpc_iomb();
}

static void
native_bs_wr_4(bus_space_handle_t bsh, bus_size_t ofs, const uint32_t *addr,
    size_t cnt)
{
	volatile uint32_t *d = __ppc_ba(bsh, ofs);

	while (cnt--)
		*d++ = *addr++;
	powerpc_iomb();
}

static void
native_bs_wr_8(bus_space_handle_t bsh, bus_size_t ofs, const uint64_t *addr,
    size_t cnt)
{
	volatile uint64_t *d = __ppc_ba(bsh, ofs);

	while (cnt--)
		*d++ = *addr++;
	powerpc_iomb();
}

static void
native_bs_sm_1(bus_space_handle_t bsh, bus_size_t ofs, uint8_t val, size_t cnt)
{
	volatile uint8_t *d = __ppc_ba(bsh, ofs);

	while (cnt--)
		*d = val;
	powerpc_iomb();
}

static void
native_bs_sm_2(bus_space_handle_t bsh, bus_size_t ofs, uint16_t val, size_t cnt)
{
	volatile uint16_t *d = __ppc_ba(bsh, ofs);

	while (cnt--)
		*d = val;
	powerpc_iomb();
}

static void
native_bs_sm_4(bus_space_handle_t bsh, bus_size_t ofs, uint32_t val, size_t cnt)
{
	volatile uint32_t *d = __ppc_ba(bsh, ofs);

	while (cnt--)
		*d = val;
	powerpc_iomb();
}

static void
native_bs_sm_8(bus_space_handle_t bsh, bus_size_t ofs, uint64_t val, size_t cnt)
{
	volatile uint64_t *d = __ppc_ba(bsh, ofs);

	while (cnt--)
		*d = val;
	powerpc_iomb();
}

static void
native_bs_sr_1(bus_space_handle_t bsh, bus_size_t ofs, uint8_t val, size_t cnt)
{
	volatile uint8_t *d = __ppc_ba(bsh, ofs);

	while (cnt--)
		*d++ = val;
	powerpc_iomb();
}

static void
native_bs_sr_2(bus_space_handle_t bsh, bus_size_t ofs, uint16_t val, size_t cnt)
{
	volatile uint16_t *d = __ppc_ba(bsh, ofs);

	while (cnt--)
		*d++ = val;
	powerpc_iomb();
}

static void
native_bs_sr_4(bus_space_handle_t bsh, bus_size_t ofs, uint32_t val, size_t cnt)
{
	volatile uint32_t *d = __ppc_ba(bsh, ofs);

	while (cnt--)
		*d++ = val;
	powerpc_iomb();
}

static void
native_bs_sr_8(bus_space_handle_t bsh, bus_size_t ofs, uint64_t val, size_t cnt)
{
	volatile uint64_t *d = __ppc_ba(bsh, ofs);

	while (cnt--)
		*d++ = val;
	powerpc_iomb();
}

/*
 * Byteswapped access functions
 */
static uint8_t
swapped_bs_rs_1(bus_space_handle_t bsh, bus_size_t ofs)
{
	volatile uint8_t *addr;
	uint8_t res;

	addr = __ppc_ba(bsh, ofs);
	res = *addr;
	powerpc_iomb();
	CTR4(KTR_LE_IO, "%s(bsh=%#x, ofs=%#x) = %#x", __func__, bsh, ofs, res);
	return (res);
}

static uint16_t
swapped_bs_rs_2(bus_space_handle_t bsh, bus_size_t ofs)
{
	volatile uint16_t *addr;
	uint16_t res;

	addr = __ppc_ba(bsh, ofs);
	__asm __volatile("lhbrx %0, 0, %1" : "=r"(res) : "r"(addr));
	powerpc_iomb();
	CTR4(KTR_LE_IO, "%s(bsh=%#x, ofs=%#x) = %#x", __func__, bsh, ofs, res);
	return (res);
}

static uint32_t
swapped_bs_rs_4(bus_space_handle_t bsh, bus_size_t ofs)
{
	volatile uint32_t *addr;
	uint32_t res;

	addr = __ppc_ba(bsh, ofs);
	__asm __volatile("lwbrx %0, 0, %1" : "=r"(res) : "r"(addr));
	powerpc_iomb();
	CTR4(KTR_LE_IO, "%s(bsh=%#x, ofs=%#x) = %#x", __func__, bsh, ofs, res);
	return (res);
}

static uint64_t
swapped_bs_rs_8(bus_space_handle_t bsh, bus_size_t ofs)
{
	volatile uint64_t *addr;
	uint64_t res;

	addr = __ppc_ba(bsh, ofs);
	res = le64toh(*addr);
	powerpc_iomb();
	CTR4(KTR_LE_IO, "%s(bsh=%#x, ofs=%#x) = %#x", __func__, bsh, ofs, res);
	return (res);
}

static void
swapped_bs_rm_1(bus_space_handle_t bsh, bus_size_t ofs, uint8_t *addr, size_t cnt)
{
	ins8(__ppc_ba(bsh, ofs), addr, cnt);
}

static void
swapped_bs_rm_2(bus_space_handle_t bsh, bus_size_t ofs, uint16_t *addr, size_t cnt)
{
	ins16rb(__ppc_ba(bsh, ofs), addr, cnt);
}

static void
swapped_bs_rm_4(bus_space_handle_t bsh, bus_size_t ofs, uint32_t *addr, size_t cnt)
{
	ins32rb(__ppc_ba(bsh, ofs), addr, cnt);
}

static void
swapped_bs_rm_8(bus_space_handle_t bshh, bus_size_t ofs, uint64_t *addr, size_t cnt)
{
	TODO;
}

static void
swapped_bs_rr_1(bus_space_handle_t bsh, bus_size_t ofs, uint8_t *addr, size_t cnt)
{
	volatile uint8_t *s = __ppc_ba(bsh, ofs);

	while (cnt--)
		*addr++ = *s++;
	powerpc_iomb();
}

static void
swapped_bs_rr_2(bus_space_handle_t bsh, bus_size_t ofs, uint16_t *addr, size_t cnt)
{
	volatile uint16_t *s = __ppc_ba(bsh, ofs);

	while (cnt--)
		*addr++ = in16rb(s++);
	powerpc_iomb();
}

static void
swapped_bs_rr_4(bus_space_handle_t bsh, bus_size_t ofs, uint32_t *addr, size_t cnt)
{
	volatile uint32_t *s = __ppc_ba(bsh, ofs);

	while (cnt--)
		*addr++ = in32rb(s++);
	powerpc_iomb();
}

static void
swapped_bs_rr_8(bus_space_handle_t bsh, bus_size_t ofs, uint64_t *addr, size_t cnt)
{
	TODO;
}

static void
swapped_bs_ws_1(bus_space_handle_t bsh, bus_size_t ofs, uint8_t val)
{
	volatile uint8_t *addr;

	addr = __ppc_ba(bsh, ofs);
	*addr = val;
	powerpc_iomb();
	CTR4(KTR_LE_IO, "%s(bsh=%#x, ofs=%#x, val=%#x)", __func__, bsh, ofs, val);
}

static void
swapped_bs_ws_2(bus_space_handle_t bsh, bus_size_t ofs, uint16_t val)
{
	volatile uint16_t *addr;

	addr = __ppc_ba(bsh, ofs);
	__asm __volatile("sthbrx %0, 0, %1" :: "r"(val), "r"(addr));
	powerpc_iomb();
	CTR4(KTR_LE_IO, "%s(bsh=%#x, ofs=%#x, val=%#x)", __func__, bsh, ofs, val);
}

static void
swapped_bs_ws_4(bus_space_handle_t bsh, bus_size_t ofs, uint32_t val)
{
	volatile uint32_t *addr;

	addr = __ppc_ba(bsh, ofs);
	__asm __volatile("stwbrx %0, 0, %1" :: "r"(val), "r"(addr));
	powerpc_iomb();
	CTR4(KTR_LE_IO, "%s(bsh=%#x, ofs=%#x, val=%#x)", __func__, bsh, ofs, val);
}

static void
swapped_bs_ws_8(bus_space_handle_t bsh, bus_size_t ofs, uint64_t val)
{
	volatile uint64_t *addr;

	addr = __ppc_ba(bsh, ofs);
	*addr = htole64(val);
	powerpc_iomb();
	CTR4(KTR_LE_IO, "%s(bsh=%#x, ofs=%#x, val=%#x)", __func__, bsh, ofs, val);
}

static void
swapped_bs_wm_1(bus_space_handle_t bsh, bus_size_t ofs, const uint8_t *addr,
    bus_size_t cnt)
{
	outs8(__ppc_ba(bsh, ofs), addr, cnt);
}

static void
swapped_bs_wm_2(bus_space_handle_t bsh, bus_size_t ofs, const uint16_t *addr,
    bus_size_t cnt)
{
	outs16rb(__ppc_ba(bsh, ofs), addr, cnt);
}

static void
swapped_bs_wm_4(bus_space_handle_t bsh, bus_size_t ofs, const uint32_t *addr,
    bus_size_t cnt)
{
	outs32rb(__ppc_ba(bsh, ofs), addr, cnt);
}

static void
swapped_bs_wm_8(bus_space_handle_t bsh, bus_size_t ofs, const uint64_t *addr,
    bus_size_t cnt)
{
	TODO;
}

static void
swapped_bs_wr_1(bus_space_handle_t bsh, bus_size_t ofs, const uint8_t *addr,
    size_t cnt)
{
	volatile uint8_t *d = __ppc_ba(bsh, ofs);

	while (cnt--)
		*d++ = *addr++;
	powerpc_iomb();
}

static void
swapped_bs_wr_2(bus_space_handle_t bsh, bus_size_t ofs, const uint16_t *addr,
    size_t cnt)
{
	volatile uint16_t *d = __ppc_ba(bsh, ofs);

	while (cnt--)
		out16rb(d++, *addr++);
	powerpc_iomb();
}

static void
swapped_bs_wr_4(bus_space_handle_t bsh, bus_size_t ofs, const uint32_t *addr,
    size_t cnt)
{
	volatile uint32_t *d = __ppc_ba(bsh, ofs);

	while (cnt--)
		out32rb(d++, *addr++);
	powerpc_iomb();
}

static void
swapped_bs_wr_8(bus_space_handle_t bsh, bus_size_t ofs, const uint64_t *addr,
    size_t cnt)
{
	TODO;
}

static void
swapped_bs_sm_1(bus_space_handle_t bsh, bus_size_t ofs, uint8_t val, size_t cnt)
{
	volatile uint8_t *d = __ppc_ba(bsh, ofs);

	while (cnt--)
		*d = val;
	powerpc_iomb();
}

static void
swapped_bs_sm_2(bus_space_handle_t bsh, bus_size_t ofs, uint16_t val, size_t cnt)
{
	volatile uint16_t *d = __ppc_ba(bsh, ofs);

	while (cnt--)
		out16rb(d, val);
	powerpc_iomb();
}

static void
swapped_bs_sm_4(bus_space_handle_t bsh, bus_size_t ofs, uint32_t val, size_t cnt)
{
	volatile uint32_t *d = __ppc_ba(bsh, ofs);

	while (cnt--)
		out32rb(d, val);
	powerpc_iomb();
}

static void
swapped_bs_sm_8(bus_space_handle_t bsh, bus_size_t ofs, uint64_t val, size_t cnt)
{
	TODO;
}

static void
swapped_bs_sr_1(bus_space_handle_t bsh, bus_size_t ofs, uint8_t val, size_t cnt)
{
	volatile uint8_t *d = __ppc_ba(bsh, ofs);

	while (cnt--)
		*d++ = val;
	powerpc_iomb();
}

static void
swapped_bs_sr_2(bus_space_handle_t bsh, bus_size_t ofs, uint16_t val, size_t cnt)
{
	volatile uint16_t *d = __ppc_ba(bsh, ofs);

	while (cnt--)
		out16rb(d++, val);
	powerpc_iomb();
}

static void
swapped_bs_sr_4(bus_space_handle_t bsh, bus_size_t ofs, uint32_t val, size_t cnt)
{
	volatile uint32_t *d = __ppc_ba(bsh, ofs);

	while (cnt--)
		out32rb(d++, val);
	powerpc_iomb();
}

static void
swapped_bs_sr_8(bus_space_handle_t bsh, bus_size_t ofs, uint64_t val, size_t cnt)
{
	TODO;
}

#if BYTE_ORDER == LITTLE_ENDIAN
struct bus_space bs_le_tag = {
#else
struct bus_space bs_be_tag = {
#endif
	/* mapping/unmapping */
	.bs_map =	bs_gen_map,
	.bs_unmap =	bs_gen_unmap,
	.bs_subregion =	bs_gen_subregion,

	/* allocation/deallocation */
	.bs_alloc =	bs_gen_alloc,
	.bs_free =	bs_gen_free,

	/* barrier */
	.bs_barrier =	bs_gen_barrier,

	/* read (single) */
	.bs_r_1 =	native_bs_rs_1,
	.bs_r_2 =	native_bs_rs_2,
	.bs_r_4 =	native_bs_rs_4,
	.bs_r_8 =	native_bs_rs_8,

	/* read (single) stream */
	.bs_r_s_2 =	native_bs_rs_2,
	.bs_r_s_4 =	native_bs_rs_4,
	.bs_r_s_8 =	native_bs_rs_8,

	/* read multiple */
	.bs_rm_1 =	native_bs_rm_1,
	.bs_rm_2 =	native_bs_rm_2,
	.bs_rm_4 =	native_bs_rm_4,
	.bs_rm_8 =	native_bs_rm_8,

	/* read multiple stream */
	.bs_rm_s_2 =	native_bs_rm_2,
	.bs_rm_s_4 =	native_bs_rm_4,
	.bs_rm_s_8 =	native_bs_rm_8,

	/* read region */
	.bs_rr_1 =	native_bs_rr_1,
	.bs_rr_2 =	native_bs_rr_2,
	.bs_rr_4 =	native_bs_rr_4,
	.bs_rr_8 =	native_bs_rr_8,

	/* read region stream */
	.bs_rr_s_2 =	native_bs_rr_2,
	.bs_rr_s_4 =	native_bs_rr_4,
	.bs_rr_s_8 =	native_bs_rr_8,

	/* write (single) */
	.bs_w_1 =	native_bs_ws_1,
	.bs_w_2 =	native_bs_ws_2,
	.bs_w_4 =	native_bs_ws_4,
	.bs_w_8 =	native_bs_ws_8,

	/* write (single) stream */
	.bs_w_s_2 =	native_bs_ws_2,
	.bs_w_s_4 =	native_bs_ws_4,
	.bs_w_s_8 =	native_bs_ws_8,

	/* write multiple */
	.bs_wm_1 =	native_bs_wm_1,
	.bs_wm_2 =	native_bs_wm_2,
	.bs_wm_4 =	native_bs_wm_4,
	.bs_wm_8 =	native_bs_wm_8,

	/* write multiple stream */
	.bs_wm_s_2 =	native_bs_wm_2,
	.bs_wm_s_4 =	native_bs_wm_4,
	.bs_wm_s_8 =	native_bs_wm_8,

	/* write region */
	.bs_wr_1 =	native_bs_wr_1,
	.bs_wr_2 =	native_bs_wr_2,
	.bs_wr_4 =	native_bs_wr_4,
	.bs_wr_8 =	native_bs_wr_8,

	/* write region stream */
	.bs_wr_s_2 =	native_bs_wr_2,
	.bs_wr_s_4 =	native_bs_wr_4,
	.bs_wr_s_8 =	native_bs_wr_8,

	/* set multiple */
	.bs_sm_1 =	native_bs_sm_1,
	.bs_sm_2 =	native_bs_sm_2,
	.bs_sm_4 =	native_bs_sm_4,
	.bs_sm_8 =	native_bs_sm_8,

	/* set multiple stream */
	.bs_sm_s_2 =	native_bs_sm_2,
	.bs_sm_s_4 =	native_bs_sm_4,
	.bs_sm_s_8 =	native_bs_sm_8,

	/* set region */
	.bs_sr_1 =	native_bs_sr_1,
	.bs_sr_2 =	native_bs_sr_2,
	.bs_sr_4 =	native_bs_sr_4,
	.bs_sr_8 =	native_bs_sr_8,

	/* set region stream */
	.bs_sr_s_2 =	native_bs_sr_2,
	.bs_sr_s_4 =	native_bs_sr_4,
	.bs_sr_s_8 =	native_bs_sr_8,

	/* copy region */
	.bs_cr_1 =	NULL, /* UNIMPLEMENTED */
	.bs_cr_2 =	NULL, /* UNIMPLEMENTED */
	.bs_cr_4 =	NULL, /* UNIMPLEMENTED */
	.bs_cr_8 =	NULL, /* UNIMPLEMENTED */

	/* copy region stream */
	.bs_cr_s_2 =	NULL, /* UNIMPLEMENTED */
	.bs_cr_s_4 =	NULL, /* UNIMPLEMENTED */
	.bs_cr_s_8 =	NULL, /* UNIMPLEMENTED */
};

#if BYTE_ORDER == LITTLE_ENDIAN
struct bus_space bs_be_tag = {
#else
struct bus_space bs_le_tag = {
#endif
	/* mapping/unmapping */
	.bs_map =	bs_gen_map,
	.bs_unmap =	bs_gen_unmap,
	.bs_subregion =	bs_gen_subregion,

	/* allocation/deallocation */
	.bs_alloc =	bs_gen_alloc,
	.bs_free =	bs_gen_free,

	/* barrier */
	.bs_barrier =	bs_gen_barrier,

	/* read (single) */
	.bs_r_1 =	swapped_bs_rs_1,
	.bs_r_2 =	swapped_bs_rs_2,
	.bs_r_4 =	swapped_bs_rs_4,
	.bs_r_8 =	swapped_bs_rs_8,

	/* read (single) stream */
	.bs_r_s_2 =	native_bs_rs_2,
	.bs_r_s_4 =	native_bs_rs_4,
	.bs_r_s_8 =	native_bs_rs_8,

	/* read multiple */
	.bs_rm_1 =	swapped_bs_rm_1,
	.bs_rm_2 =	swapped_bs_rm_2,
	.bs_rm_4 =	swapped_bs_rm_4,
	.bs_rm_8 =	swapped_bs_rm_8,

	/* read multiple stream */
	.bs_rm_s_2 =	native_bs_rm_2,
	.bs_rm_s_4 =	native_bs_rm_4,
	.bs_rm_s_8 =	native_bs_rm_8,

	/* read region */
	.bs_rr_1 =	swapped_bs_rr_1,
	.bs_rr_2 =	swapped_bs_rr_2,
	.bs_rr_4 =	swapped_bs_rr_4,
	.bs_rr_8 =	swapped_bs_rr_8,

	/* read region stream */
	.bs_rr_s_2 =	native_bs_rr_2,
	.bs_rr_s_4 =	native_bs_rr_4,
	.bs_rr_s_8 =	native_bs_rr_8,

	/* write (single) */
	.bs_w_1 =	swapped_bs_ws_1,
	.bs_w_2 =	swapped_bs_ws_2,
	.bs_w_4 =	swapped_bs_ws_4,
	.bs_w_8 =	swapped_bs_ws_8,

	/* write (single) stream */
	.bs_w_s_2 =	native_bs_ws_2,
	.bs_w_s_4 =	native_bs_ws_4,
	.bs_w_s_8 =	native_bs_ws_8,

	/* write multiple */
	.bs_wm_1 =	swapped_bs_wm_1,
	.bs_wm_2 =	swapped_bs_wm_2,
	.bs_wm_4 =	swapped_bs_wm_4,
	.bs_wm_8 =	swapped_bs_wm_8,

	/* write multiple stream */
	.bs_wm_s_2 =	native_bs_wm_2,
	.bs_wm_s_4 =	native_bs_wm_4,
	.bs_wm_s_8 =	native_bs_wm_8,

	/* write region */
	.bs_wr_1 =	swapped_bs_wr_1,
	.bs_wr_2 =	swapped_bs_wr_2,
	.bs_wr_4 =	swapped_bs_wr_4,
	.bs_wr_8 =	swapped_bs_wr_8,

	/* write region stream */
	.bs_wr_s_2 =	native_bs_wr_2,
	.bs_wr_s_4 =	native_bs_wr_4,
	.bs_wr_s_8 =	native_bs_wr_8,

	/* set multiple */
	.bs_sm_1 =	swapped_bs_sm_1,
	.bs_sm_2 =	swapped_bs_sm_2,
	.bs_sm_4 =	swapped_bs_sm_4,
	.bs_sm_8 =	swapped_bs_sm_8,

	/* set multiple stream */
	.bs_sm_s_2 =	native_bs_sm_2,
	.bs_sm_s_4 =	native_bs_sm_4,
	.bs_sm_s_8 =	native_bs_sm_8,

	/* set region */
	.bs_sr_1 =	swapped_bs_sr_1,
	.bs_sr_2 =	swapped_bs_sr_2,
	.bs_sr_4 =	swapped_bs_sr_4,
	.bs_sr_8 =	swapped_bs_sr_8,

	/* set region stream */
	.bs_sr_s_2 =	native_bs_sr_2,
	.bs_sr_s_4 =	native_bs_sr_4,
	.bs_sr_s_8 =	native_bs_sr_8,

	/* copy region */
	.bs_cr_1 =	NULL, /* UNIMPLEMENTED */
	.bs_cr_2 =	NULL, /* UNIMPLEMENTED */
	.bs_cr_4 =	NULL, /* UNIMPLEMENTED */
	.bs_cr_8 =	NULL, /* UNIMPLEMENTED */

	/* copy region stream */
	.bs_cr_s_2 =	NULL, /* UNIMPLEMENTED */
	.bs_cr_s_4 =	NULL, /* UNIMPLEMENTED */
	.bs_cr_s_8 =	NULL, /* UNIMPLEMENTED */
};
