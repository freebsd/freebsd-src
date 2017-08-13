/*-
 * Copyright (c) 2015-2017 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory under DARPA/AFRL contract
 * FA8750-10-C-0237 ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by the University of Cambridge
 * Computer Laboratory as part of the CTSRD Project, with support from the
 * UK Higher Education Innovation Fund (HEIF).
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

#include <machine/asm.h>
#include <machine/param.h>
__FBSDID("$FreeBSD$");

	.text
	.align	2

.Lpage_mask:
	.word	PAGE_MASK

ENTRY(riscv_nullop)
	ret
END(riscv_nullop)

/*
 * Generic functions to read/modify/write the internal coprocessor registers
 */

ENTRY(riscv_tlb_flushID)
	sfence.vma
	ret
END(riscv_tlb_flushID)

ENTRY(riscv_tlb_flushID_SE)
	sfence.vma
	ret
END(riscv_tlb_flushID_SE)

/*
 * void riscv_dcache_wb_range(vm_offset_t, vm_size_t)
 */
ENTRY(riscv_dcache_wb_range)
	sfence.vma
	ret
END(riscv_dcache_wb_range)

/*
 * void riscv_dcache_wbinv_range(vm_offset_t, vm_size_t)
 */
ENTRY(riscv_dcache_wbinv_range)
	sfence.vma
	ret
END(riscv_dcache_wbinv_range)

/*
 * void riscv_dcache_inv_range(vm_offset_t, vm_size_t)
 */
ENTRY(riscv_dcache_inv_range)
	sfence.vma
	ret
END(riscv_dcache_inv_range)

/*
 * void riscv_idcache_wbinv_range(vm_offset_t, vm_size_t)
 */
ENTRY(riscv_idcache_wbinv_range)
	fence.i
	sfence.vma
	ret
END(riscv_idcache_wbinv_range)

/*
 * void riscv_icache_sync_range(vm_offset_t, vm_size_t)
 */
ENTRY(riscv_icache_sync_range)
	fence.i
	ret
END(riscv_icache_sync_range)
