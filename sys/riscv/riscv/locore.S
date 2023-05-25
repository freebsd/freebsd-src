/*-
 * Copyright (c) 2015-2018 Ruslan Bukin <br@bsdpad.com>
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
 *
 * $FreeBSD$
 */

#include "assym.inc"

#include <sys/syscall.h>
#include <machine/asm.h>
#include <machine/param.h>
#include <machine/trap.h>
#include <machine/riscvreg.h>
#include <machine/pte.h>

	.globl	kernbase
	.set	kernbase, KERNBASE

	.text
/*
 * Alternate entry point. Used when booting via SBI firmware. It must be placed
 * at the beginning of the .text section. Arguments are as follows:
 *  - a0 = hart ID
 *  - a1 = dtbp
 *
 * Multiple CPUs might enter from this point, so we perform a hart lottery and
 * send the losers to mpentry.
 */
	.globl _alt_start
_alt_start:
	/* Set the global pointer */
.option push
.option norelax
	lla	gp, __global_pointer$
.option pop

	/* Pick a hart to run the boot process. */
	lla	t0, hart_lottery
	li	t1, 1
	amoadd.w t0, t1, 0(t0)

	/*
	 * We must jump to mpentry in the non-BSP case because the offset is
	 * too large to fit in a 12-bit branch immediate.
	 */
	beqz	t0, 1f
	j	mpentry
1:
	/* Store the boot hart */
	lla	t0, boot_hart
	sw	a0, 0(t0)

	/* Load zero as modulep */
	mv	a0, zero
	j	pagetables

/*
 * Main entry point. This routine is marked as the ELF entry, and is where
 * loader(8) will enter the kernel. Arguments are as follows:
 *  - a0 = modulep
 *  - a1 = ???
 *
 * It is expected that only a single CPU will enter here.
 */
	.globl _start
_start:
	/* Set the global pointer */
.option push
.option norelax
	lla	gp, __global_pointer$
.option pop

	/*
	 * Zero a1 to indicate that we have no DTB pointer. It is already
	 * included in the loader(8) metadata.
	 */
	mv	a1, zero

	/*
	 * Set up page tables: map a 1GB region starting at KERNBASE using 2MB
	 * superpages, starting from the first 2MB physical page into which the
	 * kernel was loaded.  Also reserve an L2 page for the early device map
	 * and map the DTB, if any, using the second-last entry of that L2
	 * page.  This is hopefully enough to get us to pmap_bootstrap().
	 *
	 * Implementations are required to provide SV39 mode, so we use that
	 * initially and will optionally enable SV48 mode during kernel pmap
	 * initialization.
	 *
	 *  a0 - modulep or zero
	 *  a1 - zero or dtbp
	 */
pagetables:
	/* Get the kernel's load address */
	jal	get_physmem

	/* Add L1 entry for kernel */
	lla	s1, pagetable_l1
	lla	s2, pagetable_l2	/* Link to next level PN */
	srli	s2, s2, PAGE_SHIFT

	li	a5, KERNBASE
	srli	a5, a5, L1_SHIFT	/* >> L1_SHIFT */
	andi	a5, a5, Ln_ADDR_MASK	/* & Ln_ADDR_MASK */
	li	t4, PTE_V
	slli	t5, s2, PTE_PPN0_S	/* (s2 << PTE_PPN0_S) */
	or	t6, t4, t5

	/* Store L1 PTE entry to position */
	li	a6, PTE_SIZE
	mulw	a5, a5, a6
	add	t0, s1, a5
	sd	t6, (t0)

	/* Level 2 superpages (512 x 2MiB) */
	lla	s1, pagetable_l2
	srli	t4, s9, L2_SHIFT	/* Div physmem base by 2 MiB */
	li	t2, 512			/* Build 512 entries */
	add	t3, t4, t2
	li	t0, (PTE_KERN | PTE_X)
1:
	slli	t2, t4, PTE_PPN1_S	/* << PTE_PPN1_S */
	or	t5, t0, t2
	sd	t5, (s1)		/* Store PTE entry to position */
	addi	s1, s1, PTE_SIZE

	addi	t4, t4, 1
	bltu	t4, t3, 1b

	/* Create an L1 table entry for early devmap */
	lla	s1, pagetable_l1
	lla	s2, pagetable_l2_devmap	/* Link to next level PN */
	srli	s2, s2, PAGE_SHIFT

	li	a5, (VM_MAX_KERNEL_ADDRESS - PMAP_MAPDEV_EARLY_SIZE)
	srli	a5, a5, L1_SHIFT	/* >> L1_SHIFT */
	andi	a5, a5, Ln_ADDR_MASK	/* & Ln_ADDR_MASK */
	li	t4, PTE_V
	slli	t5, s2, PTE_PPN0_S	/* (s2 << PTE_PPN0_S) */
	or	t6, t4, t5

	/* Store the L1 table entry */
	li	a6, PTE_SIZE
	mulw	a5, a5, a6
	add	t0, s1, a5
	sd	t6, (t0)

	/* Check if we have a DTB that needs to be mapped */
	beqz	a1, 2f

	/* Create an L2 mapping for the DTB */
	lla	s1, pagetable_l2_devmap
	mv	s2, a1
	srli	s2, s2, PAGE_SHIFT
	/* Mask off any bits that aren't aligned */
	andi	s2, s2, ~((1 << (PTE_PPN1_S - PTE_PPN0_S)) - 1)

	li	t0, (PTE_KERN)
	slli	t2, s2, PTE_PPN0_S	/* << PTE_PPN0_S */
	or	t0, t0, t2

	/* Store the L2 table entry for the DTB */
	li	a6, PTE_SIZE
	li	a5, VM_EARLY_DTB_ADDRESS
	srli	a5, a5, L2_SHIFT    /* >> L2_SHIFT */
	andi	a5, a5, Ln_ADDR_MASK    /* & Ln_ADDR_MASK */
	mulw	a5, a5, a6
	add	t1, s1, a5
	sd	t0, (t1)

	/* Page tables END */

	/* Setup supervisor trap vector */
2:
	lla	t0, va
	sub	t0, t0, s9
	li	t1, KERNBASE
	add	t0, t0, t1
	csrw	stvec, t0

	/* Set page tables base register */
	lla	s2, pagetable_l1
	srli	s2, s2, PAGE_SHIFT
	li	t0, SATP_MODE_SV39
	or	s2, s2, t0
	sfence.vma
	csrw	satp, s2

	.align 2
va:
	/* Set the global pointer again, this time with the virtual address. */
.option push
.option norelax
	lla	gp, __global_pointer$
.option pop

	/* Setup supervisor trap vector */
	la	t0, cpu_exception_handler
	csrw	stvec, t0

	/* Ensure sscratch is zero */
	li	t0, 0
	csrw	sscratch, t0

	/* Initialize stack pointer */
	la	sp, initstack_end

	/* Clear frame pointer */
	mv	s0, zero

	/* Allocate space for thread0 PCB and riscv_bootparams */
	addi	sp, sp, -(PCB_SIZE + RISCV_BOOTPARAMS_SIZE) & ~STACKALIGNBYTES

	/* Clear BSS */
	la	t0, _C_LABEL(__bss_start)
	la	t1, _C_LABEL(_end)
1:
	sd	zero, 0(t0)
	addi	t0, t0, 8
	bltu	t0, t1, 1b

	/* Fill riscv_bootparams */
	la	t0, pagetable_l1
	sd	t0, RISCV_BOOTPARAMS_KERN_L1PT(sp)
	sd	s9, RISCV_BOOTPARAMS_KERN_PHYS(sp)

	la	t0, initstack
	sd	t0, RISCV_BOOTPARAMS_KERN_STACK(sp)

	li	t0, (VM_EARLY_DTB_ADDRESS)
	/* Add offset of DTB within superpage */
	li	t1, (L2_OFFSET)
	and	t1, a1, t1
	add	t0, t0, t1
	sd	t0, RISCV_BOOTPARAMS_DTBP_VIRT(sp)
	sd	a1, RISCV_BOOTPARAMS_DTBP_PHYS(sp)

	sd	a0, RISCV_BOOTPARAMS_MODULEP(sp)

	mv	a0, sp
	call	_C_LABEL(initriscv)	/* Off we go */
	call	_C_LABEL(mi_startup)

	/* We should never reach here, but if so just hang. */
2:
	wfi
	j	2b

/*
 * Get the physical address the kernel is loaded to. Returned in s9.
 */
get_physmem:
	lla	t0, virt_map	/* physical address of virt_map */
	ld	t1, 0(t0)	/* virtual address of virt_map */
	sub	t1, t1, t0	/* calculate phys->virt delta */
	li	t2, KERNBASE
	sub	s9, t2, t1	/* s9 = physmem base */
	ret

	.align  4
initstack:
	.space  (PAGE_SIZE * KSTACK_PAGES)
initstack_end:

ENTRY(sigcode)
	mv	a0, sp
	addi	a0, a0, SF_UC

1:
	li	t0, SYS_sigreturn
	ecall

	/* sigreturn failed, exit */
	li	t0, SYS_exit
	ecall

	j	1b
END(sigcode)
	/* This may be copied to the stack, keep it 16-byte aligned */
	.align	3
esigcode:

	.data
	.align	3
	.global	szsigcode
szsigcode:
	.quad	esigcode - sigcode

	.align	12
pagetable_l1:
	.space	PAGE_SIZE
pagetable_l2:
	.space	PAGE_SIZE
pagetable_l2_devmap:
	.space	PAGE_SIZE

	.align 3
virt_map:
	.quad   virt_map
hart_lottery:
	.space	4

	.globl init_pt_va
init_pt_va:
	.quad pagetable_l2	/* XXX: Keep page tables VA */

#ifndef SMP
ENTRY(mpentry)
1:
	wfi
	j	1b
END(mpentry)
#else
/*
 * mpentry(unsigned long)
 *
 * Called by a core when it is being brought online.
 */
ENTRY(mpentry)
	/*
	 * Calculate the offset to __riscv_boot_ap
	 * for the current core, cpuid is in a0.
	 */
	li	t1, 4
	mulw	t1, t1, a0
	/* Get the pointer */
	lla	t0, __riscv_boot_ap
	add	t0, t0, t1

1:
	/* Wait the kernel to be ready */
	lw	t1, 0(t0)
	beqz	t1, 1b

	/* Setup stack pointer */
	lla	t0, bootstack
	ld	sp, 0(t0)

	/* Get the kernel's load address */
	jal	get_physmem

	/* Setup supervisor trap vector */
	lla	t0, mpva
	sub	t0, t0, s9
	li	t1, KERNBASE
	add	t0, t0, t1
	csrw	stvec, t0

	/* Set page tables base register */
	lla	s2, pagetable_l1
	srli	s2, s2, PAGE_SHIFT
	li	t0, SATP_MODE_SV39
	or	s2, s2, t0
	sfence.vma
	csrw	satp, s2

	.align 2
mpva:
	/* Set the global pointer again, this time with the virtual address. */
.option push
.option norelax
	lla	gp, __global_pointer$
.option pop

	/* Setup supervisor trap vector */
	la	t0, cpu_exception_handler
	csrw	stvec, t0

	/* Ensure sscratch is zero */
	li	t0, 0
	csrw	sscratch, t0

	call	init_secondary
END(mpentry)
#endif
