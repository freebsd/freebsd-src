/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 The FreeBSD Foundation
 *
 * This software was developed by Mitchell Horne <mhorne@FreeBSD.org> under
 * sponsorship from the FreeBSD Foundation.
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
#include <sys/param.h>
#include <sys/systm.h>

#include <machine/thead.h>

bool has_errata_thead_pbmt = false;

/* ----------------- dcache ops --------------------- */


/* th.dcache.civa: clean & invalidate at VA stored in t0. */
#define	THEAD_DCACHE_CIVA	".long 0x0272800b\n"

/* th.dcache.iva: invalidate at VA stored in t0. */
#define	THEAD_DCACHE_IVA	".long 0x0262800b\n"

/* th.dcache.cva: clean at VA stored in t0. */
#define	THEAD_DCACHE_CVA	".long 0x0252800b\n"

/* th.sync.s: two-way instruction barrier */
#define	THEAD_SYNC_S		".long 0x0190000b\n"

/* MHTODO: we could parse this information from the device tree. */
#define	THEAD_DCACHE_SIZE	64

static void
thead_cpu_dcache_wbinv_range(vm_offset_t va, vm_size_t len)
{
	register vm_offset_t t0 __asm("t0") = rounddown(va, dcache_line_size);

	for (; t0 < va + len; t0 += dcache_line_size) {
		__asm __volatile(THEAD_DCACHE_CIVA
		                 :: "r" (t0) : "memory");
	}
	__asm __volatile(THEAD_SYNC_S ::: "memory");
}

static void
thead_cpu_dcache_inv_range(vm_offset_t va, vm_size_t len)
{
	register vm_offset_t t0 __asm("t0") = rounddown(va, dcache_line_size);

	for (; t0 < va + len; t0 += dcache_line_size) {
		__asm __volatile(THEAD_DCACHE_IVA
				 :: "r" (t0) : "memory");
	}
	__asm __volatile(THEAD_SYNC_S ::: "memory");
}

static void
thead_cpu_dcache_wb_range(vm_offset_t va, vm_size_t len)
{
	register vm_offset_t t0 __asm("t0") = rounddown(va, dcache_line_size);

	for (; t0 < va + len; t0 += dcache_line_size) {
		__asm __volatile(THEAD_DCACHE_CVA
				 :: "r" (t0) : "memory");
	}
	__asm __volatile(THEAD_SYNC_S ::: "memory");
}

void
thead_setup_cache(void)
{
	struct riscv_cache_ops thead_ops;

	thead_ops.dcache_wbinv_range = thead_cpu_dcache_wbinv_range;
	thead_ops.dcache_inv_range = thead_cpu_dcache_inv_range;
	thead_ops.dcache_wb_range = thead_cpu_dcache_wb_range;

	riscv_cache_install_hooks(&thead_ops, THEAD_DCACHE_SIZE);
}
