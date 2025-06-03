/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Ruslan Bukin <br@bsdpad.com>
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

/* Cache Block Operations. */

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/cbo.h>

static void
cbo_zicbom_cpu_dcache_wbinv_range(vm_offset_t va, vm_size_t len)
{
	vm_offset_t addr;

	/*
	 * A flush operation atomically performs a clean operation followed by
	 * an invalidate operation.
	 */

	va &= ~(dcache_line_size - 1);
	for (addr = va; addr < va + len; addr += dcache_line_size)
		__asm __volatile(".option push; .option arch, +zicbom\n"
				 "cbo.flush (%0); .option pop\n" :: "r"(addr));
}

static void
cbo_zicbom_cpu_dcache_inv_range(vm_offset_t va, vm_size_t len)
{
	vm_offset_t addr;

	/*
	 * An invalidate operation makes data from store operations performed by
	 * a set of non-coherent agents visible to the set of coherent agents at
	 * a point common to both sets by deallocating all copies of a cache
	 * block from the set of coherent caches up to that point.
	 */

	va &= ~(dcache_line_size - 1);
	for (addr = va; addr < va + len; addr += dcache_line_size)
		__asm __volatile(".option push; .option arch, +zicbom\n"
				 "cbo.inval (%0); .option pop\n" :: "r"(addr));
}

static void
cbo_zicbom_cpu_dcache_wb_range(vm_offset_t va, vm_size_t len)
{
	vm_offset_t addr;

	/*
	 * A clean operation makes data from store operations performed by the
	 * set of coherent agents visible to a set of non-coherent agents at a
	 * point common to both sets by performing a write transfer of a copy of
	 * a cache block to that point provided a coherent agent performed a
	 * store operation that modified the data in the cache block since the
	 * previous invalidate, clean, or flush operation on the cache block.
	 */

	va &= ~(dcache_line_size - 1);
	for (addr = va; addr < va + len; addr += dcache_line_size)
		__asm __volatile(".option push; .option arch, +zicbom\n"
				 "cbo.clean (%0); .option pop\n" :: "r"(addr));
}

void
cbo_zicbom_setup_cache(int cbom_block_size)
{
	struct riscv_cache_ops zicbom_ops;

	if (cbom_block_size <= 0 || !powerof2(cbom_block_size)) {
		printf("Zicbom: could not initialise (invalid cache line %d)\n",
		    cbom_block_size);
		return;
	}

	zicbom_ops.dcache_wbinv_range = cbo_zicbom_cpu_dcache_wbinv_range;
	zicbom_ops.dcache_inv_range = cbo_zicbom_cpu_dcache_inv_range;
	zicbom_ops.dcache_wb_range = cbo_zicbom_cpu_dcache_wb_range;
	riscv_cache_install_hooks(&zicbom_ops, cbom_block_size);
}
