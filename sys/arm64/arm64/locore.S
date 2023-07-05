/*-
 * Copyright (c) 2012-2014 Andrew Turner
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

#include "assym.inc"
#include "opt_kstack_pages.h"
#include <sys/syscall.h>
#include <machine/asm.h>
#include <machine/armreg.h>
#include <machine/hypervisor.h>
#include <machine/param.h>
#include <machine/pte.h>
#include <machine/vm.h>
#include <machine/vmparam.h>

#define	VIRT_BITS	48

#if PAGE_SIZE == PAGE_SIZE_16K
/*
 * The number of level 3 tables to create. 32 will allow for 1G of address
 * space, the same as a single level 2 page with 4k pages.
 */
#define	L3_PAGE_COUNT	32
#endif

	.globl	kernbase
	.set	kernbase, KERNBASE

/*
 * We assume:
 *  MMU      on with an identity map, or off
 *  D-Cache: off
 *  I-Cache: on or off
 *  We are loaded at a 2MiB aligned address
 */

ENTRY(_start)
	/* Drop to EL1 */
	bl	drop_to_el1

	/*
	 * Disable the MMU. We may have entered the kernel with it on and
	 * will need to update the tables later. If this has been set up
	 * with anything other than a VA == PA map then this will fail,
	 * but in this case the code to find where we are running from
	 * would have also failed.
	 */
	dsb	sy
	mrs	x2, sctlr_el1
	bic	x2, x2, SCTLR_M
	msr	sctlr_el1, x2
	isb

	/* Set the context id */
	msr	contextidr_el1, xzr

	/* Get the virt -> phys offset */
	bl	get_virt_delta

	/*
	 * At this point:
	 * x29 = PA - VA
	 * x28 = Our physical load address
	 */

	/* Create the page tables */
	bl	create_pagetables

	/*
	 * At this point:
	 * x27 = TTBR0 table
	 * x26 = Kernel L1 table
	 * x24 = TTBR1 table
	 */

	/* Enable the mmu */
	bl	start_mmu

	/* Load the new ttbr0 pagetable */
	adrp	x27, pagetable_l0_ttbr0
	add	x27, x27, :lo12:pagetable_l0_ttbr0

	/* Jump to the virtual address space */
	ldr	x15, .Lvirtdone
	br	x15

virtdone:
	/* Set up the stack */
	adrp	x25, initstack_end
	add	x25, x25, :lo12:initstack_end
	mov	sp, x25
	sub	sp, sp, #PCB_SIZE

	/* Zero the BSS */
	ldr	x15, .Lbss
	ldr	x14, .Lend
1:
	str	xzr, [x15], #8
	cmp	x15, x14
	b.lo	1b

#if defined(PERTHREAD_SSP)
	/* Set sp_el0 to the boot canary for early per-thread SSP to work */
	adrp	x15, boot_canary
	add	x15, x15, :lo12:boot_canary
	msr	sp_el0, x15
#endif

	/* Backup the module pointer */
	mov	x1, x0

	sub	sp, sp, #BOOTPARAMS_SIZE
	mov	x0, sp

	/* Degate the delda so it is VA -> PA */
	neg	x29, x29

	str	x1,  [x0, #BP_MODULEP]
	str	x29, [x0, #BP_KERN_DELTA]
	adrp	x25, initstack
	add	x25, x25, :lo12:initstack
	str	x25, [x0, #BP_KERN_STACK]
	str	x27, [x0, #BP_KERN_TTBR0]
	str	x23, [x0, #BP_BOOT_EL]
	str	x4,  [x0, #BP_HCR_EL2]

#ifdef KASAN
	/* Save bootparams */
	mov	x19, x0

	/* Bootstrap an early shadow map for the boot stack. */
	bl	pmap_san_bootstrap

	/* Restore bootparams */
	mov	x0, x19
#endif

	/* trace back starts here */
	mov	fp, #0
	/* Branch to C code */
	bl	initarm
	/* We are done with the boot params */
	add	sp, sp, #BOOTPARAMS_SIZE

	/*
	 * Enable pointer authentication in the kernel. We set the keys for
	 * thread0 in initarm so have to wait until it returns to enable it.
	 * If we were to enable it in initarm then any authentication when
	 * returning would fail as it was called with pointer authentication
	 * disabled.
	 */
	bl	ptrauth_start

	bl	mi_startup

	/* We should not get here */
	brk	0

	.align 3
.Lvirtdone:
	.quad	virtdone
.Lbss:
	.quad	__bss_start
.Lend:
	.quad	__bss_end
END(_start)

#ifdef SMP
/*
 * mpentry(unsigned long)
 *
 * Called by a core when it is being brought online.
 * The data in x0 is passed straight to init_secondary.
 */
ENTRY(mpentry)
	/* Disable interrupts */
	msr	daifset, #DAIF_INTR

	/* Drop to EL1 */
	bl	drop_to_el1

	/* Set the context id */
	msr	contextidr_el1, xzr

	/* Load the kernel page table */
	adrp	x24, pagetable_l0_ttbr1
	add	x24, x24, :lo12:pagetable_l0_ttbr1
	/* Load the identity page table */
	adrp	x27, pagetable_l0_ttbr0_boostrap
	add	x27, x27, :lo12:pagetable_l0_ttbr0_boostrap

	/* Enable the mmu */
	bl	start_mmu

	/* Load the new ttbr0 pagetable */
	adrp	x27, pagetable_l0_ttbr0
	add	x27, x27, :lo12:pagetable_l0_ttbr0

	/* Jump to the virtual address space */
	ldr	x15, =mp_virtdone
	br	x15

mp_virtdone:
	/* Start using the AP boot stack */
	ldr	x4, =bootstack
	ldr	x4, [x4]
	mov	sp, x4

#if defined(PERTHREAD_SSP)
	/* Set sp_el0 to the boot canary for early per-thread SSP to work */
	adrp	x15, boot_canary
	add	x15, x15, :lo12:boot_canary
	msr	sp_el0, x15
#endif

	/* Load the kernel ttbr0 pagetable */
	msr	ttbr0_el1, x27
	isb

	/* Invalidate the TLB */
	tlbi	vmalle1
	dsb	sy
	isb

	b	init_secondary
END(mpentry)
#endif

/*
 * If we are started in EL2, configure the required hypervisor
 * registers and drop to EL1.
 */
LENTRY(drop_to_el1)
	mrs	x23, CurrentEL
	lsr	x23, x23, #2
	cmp	x23, #0x2
	b.eq	1f
	ret
1:
	/*
	 * Disable the MMU. If the HCR_EL2.E2H field is set we will clear it
	 * which may break address translation.
	 */
	dsb	sy
	mrs	x2, sctlr_el2
	bic	x2, x2, SCTLR_M
	msr	sctlr_el2, x2
	isb

	/* Configure the Hypervisor */
	ldr	x2, =(HCR_RW | HCR_APK | HCR_API)
	msr	hcr_el2, x2

	/* Stash value of HCR_EL2 for later */
	isb
	mrs	x4, hcr_el2

	/* Load the Virtualization Process ID Register */
	mrs	x2, midr_el1
	msr	vpidr_el2, x2

	/* Load the Virtualization Multiprocess ID Register */
	mrs	x2, mpidr_el1
	msr	vmpidr_el2, x2

	/* Set the bits that need to be 1 in sctlr_el1 */
	ldr	x2, .Lsctlr_res1
	msr	sctlr_el1, x2

	/*
	 * On some hardware, e.g., Apple M1, we can't clear E2H, so make sure we
	 * don't trap to EL2 for SIMD register usage to have at least a
	 * minimally usable system.
	 */
	tst	x4, #HCR_E2H
	mov	x3, #CPTR_RES1	/* HCR_E2H == 0 */
	mov	x5, #CPTR_FPEN	/* HCR_E2H == 1 */
	csel	x2, x3, x5, eq
	msr	cptr_el2, x2

	/* Don't trap to EL2 for CP15 traps */
	msr	hstr_el2, xzr

	/* Enable access to the physical timers at EL1 */
	mrs	x2, cnthctl_el2
	orr	x2, x2, #(CNTHCTL_EL1PCTEN | CNTHCTL_EL1PCEN)
	msr	cnthctl_el2, x2

	/* Set the counter offset to a known value */
	msr	cntvoff_el2, xzr

	/* Hypervisor trap functions */
	adrp	x2, hyp_stub_vectors
	add	x2, x2, :lo12:hyp_stub_vectors
	msr	vbar_el2, x2

	/* Zero vttbr_el2 so a hypervisor can tell the host and guest apart */
	msr	vttbr_el2, xzr

	mov	x2, #(PSR_DAIF | PSR_M_EL1h)
	msr	spsr_el2, x2

	/* Configure GICv3 CPU interface */
	mrs	x2, id_aa64pfr0_el1
	/* Extract GIC bits from the register */
	ubfx	x2, x2, #ID_AA64PFR0_GIC_SHIFT, #ID_AA64PFR0_GIC_BITS
	/* GIC[3:0] == 0001 - GIC CPU interface via special regs. supported */
	cmp	x2, #(ID_AA64PFR0_GIC_CPUIF_EN >> ID_AA64PFR0_GIC_SHIFT)
	b.ne	2f

	mrs	x2, icc_sre_el2
	orr	x2, x2, #ICC_SRE_EL2_EN	/* Enable access from insecure EL1 */
	orr	x2, x2, #ICC_SRE_EL2_SRE	/* Enable system registers */
	msr	icc_sre_el2, x2
2:

	/* Set the address to return to our return address */
	msr	elr_el2, x30
	isb

	eret

	.align 3
.Lsctlr_res1:
	.quad SCTLR_RES1
LEND(drop_to_el1)

/*
 * Get the delta between the physical address we were loaded to and the
 * virtual address we expect to run from. This is used when building the
 * initial page table.
 */
LENTRY(get_virt_delta)
	/* Load the physical address of virt_map */
	adrp	x29, virt_map
	add	x29, x29, :lo12:virt_map
	/* Load the virtual address of virt_map stored in virt_map */
	ldr	x28, [x29]
	/* Find PA - VA as PA' = VA' - VA + PA = VA' + (PA - VA) = VA' + x29 */
	sub	x29, x29, x28
	/* Find the load address for the kernel */
	mov	x28, #(KERNBASE)
	add	x28, x28, x29
	ret

	.align 3
virt_map:
	.quad	virt_map
LEND(get_virt_delta)

/*
 * This builds the page tables containing the identity map, and the kernel
 * virtual map.
 *
 * It relys on:
 *  We were loaded to an address that is on a 2MiB boundary
 *  All the memory must not cross a 1GiB boundaty
 *  x28 contains the physical address we were loaded from
 *
 * TODO: This is out of date.
 *  There are at least 5 pages before that address for the page tables
 *   The pages used are:
 *    - The Kernel L2 table
 *    - The Kernel L1 table
 *    - The Kernel L0 table             (TTBR1)
 *    - The identity (PA = VA) L1 table
 *    - The identity (PA = VA) L0 table (TTBR0)
 */
LENTRY(create_pagetables)
	/* Save the Link register */
	mov	x5, x30

	/* Clean the page table */
	adrp	x6, pagetable
	add	x6, x6, :lo12:pagetable
	mov	x26, x6
	adrp	x27, pagetable_end
	add	x27, x27, :lo12:pagetable_end
1:
	stp	xzr, xzr, [x6], #16
	stp	xzr, xzr, [x6], #16
	stp	xzr, xzr, [x6], #16
	stp	xzr, xzr, [x6], #16
	cmp	x6, x27
	b.lo	1b

	/*
	 * Build the TTBR1 maps.
	 */

	/* Find the size of the kernel */
	mov	x6, #(KERNBASE)

#if defined(LINUX_BOOT_ABI)
	/* X19 is used as 'map FDT data' flag */
	mov	x19, xzr

	/* No modules or FDT pointer ? */
	cbz	x0, booti_no_fdt

	/*
	 * Test if x0 points to modules descriptor(virtual address) or
	 * to FDT (physical address)
	 */
	cmp	x0, x6		/* x6 is #(KERNBASE) */
	b.lo	booti_fdt
#endif

	/* Booted with modules pointer */
	/* Find modulep - begin */
	sub	x8, x0, x6
	/*
	 * Add space for the module data. When PAGE_SIZE is 4k this will
	 * add at least 2 level 2 blocks (2 * 2MiB). When PAGE_SIZE is
	 * larger it will be at least as large as we use smaller level 3
	 * pages.
	 */
	ldr	x7, =((6 * 1024 * 1024) - 1)
	add	x8, x8, x7
	b	common

#if defined(LINUX_BOOT_ABI)
booti_fdt:
	/* Booted by U-Boot booti with FDT data */
	/* Set 'map FDT data' flag */
	mov	x19, #1

booti_no_fdt:
	/* Booted by U-Boot booti without FTD data */
	/* Find the end - begin */
	ldr     x7, .Lend
	sub     x8, x7, x6

	/*
	 * Add one 2MiB page for copy of FDT data (maximum FDT size),
	 * one for metadata and round up
	 */
	ldr	x7, =(3 * L2_SIZE - 1)
	add	x8, x8, x7
#endif

common:
#if PAGE_SIZE != PAGE_SIZE_4K
	/*
	 * Create L3 pages. The kernel will be loaded at a 2M aligned
	 * address, however L2 blocks are too large when the page size is
	 * not 4k to map the kernel with such an aligned address. However,
	 * when the page size is larger than 4k, L2 blocks are too large to
	 * map the kernel with such an alignment.
	 */

	/* Get the number of l3 pages to allocate, rounded down */
	lsr	x10, x8, #(L3_SHIFT)

	/* Create the kernel space L2 table */
	mov	x6, x26
	mov	x7, #(ATTR_S1_IDX(VM_MEMATTR_WRITE_BACK))
	mov	x8, #(KERNBASE)
	mov	x9, x28
	bl	build_l3_page_pagetable

	/* Move to the l2 table */
	ldr	x9, =(PAGE_SIZE * L3_PAGE_COUNT)
	add	x26, x26, x9

	/* Link the l2 -> l3 table */
	mov	x9, x6
	mov	x6, x26
	bl	link_l2_pagetable
#else
	/* Get the number of l2 pages to allocate, rounded down */
	lsr	x10, x8, #(L2_SHIFT)

	/* Create the kernel space L2 table */
	mov	x6, x26
	mov	x7, #(ATTR_S1_IDX(VM_MEMATTR_WRITE_BACK))
	mov	x8, #(KERNBASE)
	mov	x9, x28
	bl	build_l2_block_pagetable
#endif

	/* Move to the l1 table */
	add	x26, x26, #PAGE_SIZE

	/* Link the l1 -> l2 table */
	mov	x9, x6
	mov	x6, x26
	bl	link_l1_pagetable

	/* Move to the l0 table */
	add	x24, x26, #PAGE_SIZE

	/* Link the l0 -> l1 table */
	mov	x9, x6
	mov	x6, x24
	mov	x10, #1
	bl	link_l0_pagetable

	/*
	 * Build the TTBR0 maps.  As TTBR0 maps, they must specify ATTR_S1_nG.
	 * They are only needed early on, so the VA = PA map is uncached.
	 */
	add	x27, x24, #PAGE_SIZE

	mov	x6, x27		/* The initial page table */

	/* Create the VA = PA map */
	mov	x7, #(ATTR_S1_nG | ATTR_S1_IDX(VM_MEMATTR_WRITE_BACK))
	adrp	x16, _start
	and	x16, x16, #(~L2_OFFSET)
	mov	x9, x16		/* PA start */
	mov	x8, x16		/* VA start (== PA start) */
	mov	x10, #1
	bl	build_l2_block_pagetable

#if defined(SOCDEV_PA)
	/* Create a table for the UART */
	mov	x7, #(ATTR_S1_nG | ATTR_S1_IDX(VM_MEMATTR_DEVICE))
	ldr	x9, =(L2_SIZE)
	add	x16, x16, x9	/* VA start */
	mov	x8, x16

	/* Store the socdev virtual address */
	add	x17, x8, #(SOCDEV_PA & L2_OFFSET)
	adrp	x9, socdev_va
	str	x17, [x9, :lo12:socdev_va]

	mov	x9, #(SOCDEV_PA & ~L2_OFFSET)	/* PA start */
	mov	x10, #1
	bl	build_l2_block_pagetable
#endif

#if defined(LINUX_BOOT_ABI)
	/* Map FDT data ? */
	cbz	x19, 1f

	/* Create the mapping for FDT data (2 MiB max) */
	mov	x7, #(ATTR_S1_nG | ATTR_S1_IDX(VM_MEMATTR_WRITE_BACK))
	ldr	x9, =(L2_SIZE)
	add	x16, x16, x9	/* VA start */
	mov	x8, x16
	mov	x9, x0			/* PA start */
	/* Update the module pointer to point at the allocated memory */
	and	x0, x0, #(L2_OFFSET)	/* Keep the lower bits */
	add	x0, x0, x8		/* Add the aligned virtual address */

	mov	x10, #1
	bl	build_l2_block_pagetable

1:
#endif

	/* Move to the l1 table */
	add	x27, x27, #PAGE_SIZE

	/* Link the l1 -> l2 table */
	mov	x9, x6
	mov	x6, x27
	bl	link_l1_pagetable

	/* Move to the l0 table */
	add	x27, x27, #PAGE_SIZE

	/* Link the l0 -> l1 table */
	mov	x9, x6
	mov	x6, x27
	mov	x10, #1
	bl	link_l0_pagetable

	/* Restore the Link register */
	mov	x30, x5
	ret
LEND(create_pagetables)

/*
 * Builds an L0 -> L1 table descriptor
 *
 *  x6  = L0 table
 *  x8  = Virtual Address
 *  x9  = L1 PA (trashed)
 *  x10 = Entry count (trashed)
 *  x11, x12 and x13 are trashed
 */
LENTRY(link_l0_pagetable)
	/*
	 * Link an L0 -> L1 table entry.
	 */
	/* Find the table index */
	lsr	x11, x8, #L0_SHIFT
	and	x11, x11, #L0_ADDR_MASK

	/* Build the L0 block entry */
	mov	x12, #L0_TABLE
	orr	x12, x12, #(TATTR_UXN_TABLE | TATTR_AP_TABLE_NO_EL0)

	/* Only use the output address bits */
	lsr	x9, x9, #PAGE_SHIFT
1:	orr	x13, x12, x9, lsl #PAGE_SHIFT

	/* Store the entry */
	str	x13, [x6, x11, lsl #3]

	sub	x10, x10, #1
	add	x11, x11, #1
	add	x9, x9, #1
	cbnz	x10, 1b

	ret
LEND(link_l0_pagetable)

/*
 * Builds an L1 -> L2 table descriptor
 *
 *  x6  = L1 table
 *  x8  = Virtual Address
 *  x9  = L2 PA (trashed)
 *  x11, x12 and x13 are trashed
 */
LENTRY(link_l1_pagetable)
	/*
	 * Link an L1 -> L2 table entry.
	 */
	/* Find the table index */
	lsr	x11, x8, #L1_SHIFT
	and	x11, x11, #Ln_ADDR_MASK

	/* Build the L1 block entry */
	mov	x12, #L1_TABLE

	/* Only use the output address bits */
	lsr	x9, x9, #PAGE_SHIFT
	orr	x13, x12, x9, lsl #PAGE_SHIFT

	/* Store the entry */
	str	x13, [x6, x11, lsl #3]

	ret
LEND(link_l1_pagetable)

/*
 * Builds count 2 MiB page table entry
 *  x6  = L2 table
 *  x7  = Block attributes
 *  x8  = VA start
 *  x9  = PA start (trashed)
 *  x10 = Entry count (trashed)
 *  x11, x12 and x13 are trashed
 */
LENTRY(build_l2_block_pagetable)
	/*
	 * Build the L2 table entry.
	 */
	/* Find the table index */
	lsr	x11, x8, #L2_SHIFT
	and	x11, x11, #Ln_ADDR_MASK

	/* Build the L2 block entry */
	orr	x12, x7, #L2_BLOCK
	orr	x12, x12, #(ATTR_DEFAULT)
	orr	x12, x12, #(ATTR_S1_UXN)

	/* Only use the output address bits */
	lsr	x9, x9, #L2_SHIFT

	/* Set the physical address for this virtual address */
1:	orr	x13, x12, x9, lsl #L2_SHIFT

	/* Store the entry */
	str	x13, [x6, x11, lsl #3]

	sub	x10, x10, #1
	add	x11, x11, #1
	add	x9, x9, #1
	cbnz	x10, 1b

	ret
LEND(build_l2_block_pagetable)

#if PAGE_SIZE != PAGE_SIZE_4K
/*
 * Builds an L2 -> L3 table descriptor
 *
 *  x6  = L2 table
 *  x8  = Virtual Address
 *  x9  = L3 PA (trashed)
 *  x11, x12 and x13 are trashed
 */
LENTRY(link_l2_pagetable)
	/*
	 * Link an L2 -> L3 table entry.
	 */
	/* Find the table index */
	lsr	x11, x8, #L2_SHIFT
	and	x11, x11, #Ln_ADDR_MASK

	/* Build the L1 block entry */
	mov	x12, #L2_TABLE

	/* Only use the output address bits */
	lsr	x9, x9, #PAGE_SHIFT
	orr	x13, x12, x9, lsl #PAGE_SHIFT

	/* Store the entry */
	str	x13, [x6, x11, lsl #3]

	ret
LEND(link_l2_pagetable)

/*
 * Builds count level 3 page table entries
 *  x6  = L3 table
 *  x7  = Block attributes
 *  x8  = VA start
 *  x9  = PA start (trashed)
 *  x10 = Entry count (trashed)
 *  x11, x12 and x13 are trashed
 */
LENTRY(build_l3_page_pagetable)
	/*
	 * Build the L3 table entry.
	 */
	/* Find the table index */
	lsr	x11, x8, #L3_SHIFT
	and	x11, x11, #Ln_ADDR_MASK

	/* Build the L3 page entry */
	orr	x12, x7, #L3_PAGE
	orr	x12, x12, #(ATTR_DEFAULT)
	orr	x12, x12, #(ATTR_S1_UXN)

	/* Only use the output address bits */
	lsr	x9, x9, #L3_SHIFT

	/* Set the physical address for this virtual address */
1:	orr	x13, x12, x9, lsl #L3_SHIFT

	/* Store the entry */
	str	x13, [x6, x11, lsl #3]

	sub	x10, x10, #1
	add	x11, x11, #1
	add	x9, x9, #1
	cbnz	x10, 1b

	ret
LEND(build_l3_page_pagetable)
#endif

LENTRY(start_mmu)
	dsb	sy

	/* Load the exception vectors */
	ldr	x2, =exception_vectors
	msr	vbar_el1, x2

	/* Load ttbr0 and ttbr1 */
	msr	ttbr0_el1, x27
	msr	ttbr1_el1, x24
	isb

	/* Clear the Monitor Debug System control register */
	msr	mdscr_el1, xzr

	/* Invalidate the TLB */
	tlbi	vmalle1is
	dsb	ish
	isb

	ldr	x2, mair
	msr	mair_el1, x2

	/*
	 * Setup TCR according to the PARange and ASIDBits fields
	 * from ID_AA64MMFR0_EL1 and the HAFDBS field from the
	 * ID_AA64MMFR1_EL1.  More precisely, set TCR_EL1.AS
	 * to 1 only if the ASIDBits field equals 0b0010.
	 */
	ldr	x2, tcr
	mrs	x3, id_aa64mmfr0_el1

	/* Copy the bottom 3 bits from id_aa64mmfr0_el1 into TCR.IPS */
	bfi	x2, x3, #(TCR_IPS_SHIFT), #(TCR_IPS_WIDTH)
	and	x3, x3, #(ID_AA64MMFR0_ASIDBits_MASK)

	/* Check if the HW supports 16 bit ASIDS */
	cmp	x3, #(ID_AA64MMFR0_ASIDBits_16)
	/* If so x3 == 1, else x3 == 0 */
	cset	x3, eq
	/* Set TCR.AS with x3 */
	bfi	x2, x3, #(TCR_ASID_SHIFT), #(TCR_ASID_WIDTH)

	/*
	 * Check if the HW supports access flag and dirty state updates,
	 * and set TCR_EL1.HA and TCR_EL1.HD accordingly.
	 */
	mrs	x3, id_aa64mmfr1_el1
	and	x3, x3, #(ID_AA64MMFR1_HAFDBS_MASK)
	cmp	x3, #1
	b.ne	1f
	orr 	x2, x2, #(TCR_HA)
	b	2f
1:
	cmp	x3, #2
	b.ne	2f
	orr 	x2, x2, #(TCR_HA | TCR_HD)
2:
	msr	tcr_el1, x2

	/*
	 * Setup SCTLR.
	 */
	ldr	x2, sctlr_set
	ldr	x3, sctlr_clear
	mrs	x1, sctlr_el1
	bic	x1, x1, x3	/* Clear the required bits */
	orr	x1, x1, x2	/* Set the required bits */
	msr	sctlr_el1, x1
	isb

	ret

	.align 3
mair:
	.quad	MAIR_ATTR(MAIR_DEVICE_nGnRnE, VM_MEMATTR_DEVICE_nGnRnE) | \
		MAIR_ATTR(MAIR_NORMAL_NC, VM_MEMATTR_UNCACHEABLE)   |	\
		MAIR_ATTR(MAIR_NORMAL_WB, VM_MEMATTR_WRITE_BACK)    |	\
		MAIR_ATTR(MAIR_NORMAL_WT, VM_MEMATTR_WRITE_THROUGH) |	\
		MAIR_ATTR(MAIR_DEVICE_nGnRE, VM_MEMATTR_DEVICE_nGnRE)
tcr:
#if PAGE_SIZE == PAGE_SIZE_4K
#define	TCR_TG	(TCR_TG1_4K | TCR_TG0_4K)
#elif PAGE_SIZE == PAGE_SIZE_16K
#define	TCR_TG	(TCR_TG1_16K | TCR_TG0_16K)
#else
#error Unsupported page size
#endif

	.quad (TCR_TxSZ(64 - VIRT_BITS) | TCR_TG | \
	    TCR_CACHE_ATTRS | TCR_SMP_ATTRS)
sctlr_set:
	/* Bits to set */
	.quad (SCTLR_LSMAOE | SCTLR_nTLSMD | SCTLR_UCI | SCTLR_SPAN | \
	    SCTLR_nTWE | SCTLR_nTWI | SCTLR_UCT | SCTLR_DZE | \
	    SCTLR_I | SCTLR_SED | SCTLR_SA0 | SCTLR_SA | SCTLR_C | \
	    SCTLR_M | SCTLR_CP15BEN)
sctlr_clear:
	/* Bits to clear */
	.quad (SCTLR_EE | SCTLR_E0E | SCTLR_IESB | SCTLR_WXN | SCTLR_UMA | \
	    SCTLR_ITD | SCTLR_A)
LEND(start_mmu)

ENTRY(abort)
	b abort
END(abort)

.bss
	.align	PAGE_SHIFT
initstack:
	.space	(PAGE_SIZE * KSTACK_PAGES)
initstack_end:

	.section .init_pagetable, "aw", %nobits
	.align PAGE_SHIFT
	/*
	 * 6 initial tables (in the following order):
	 *           L2 for kernel (High addresses)
	 *           L1 for kernel
	 *           L0 for kernel
	 *           L1 bootstrap for user   (Low addresses)
	 *           L0 bootstrap for user
	 *           L0 for user
	 */
	.globl pagetable_l0_ttbr1
pagetable:
#if PAGE_SIZE != PAGE_SIZE_4K
	.space	(PAGE_SIZE * L3_PAGE_COUNT)
pagetable_l2_ttbr1:
#endif
	.space	PAGE_SIZE
pagetable_l1_ttbr1:
	.space	PAGE_SIZE
pagetable_l0_ttbr1:
	.space	PAGE_SIZE
pagetable_l2_ttbr0_bootstrap:
	.space	PAGE_SIZE
pagetable_l1_ttbr0_bootstrap:
	.space	PAGE_SIZE
pagetable_l0_ttbr0_boostrap:
	.space	PAGE_SIZE
pagetable_l0_ttbr0:
	.space	PAGE_SIZE
pagetable_end:

el2_pagetable:
	.space	PAGE_SIZE

	.section .rodata, "a", %progbits
	.globl	aarch32_sigcode
	.align 2
aarch32_sigcode:
	.word 0xe1a0000d	// mov r0, sp
	.word 0xe2800040	// add r0, r0, #SIGF_UC
	.word 0xe59f700c	// ldr r7, [pc, #12]
	.word 0xef000000	// swi #0
	.word 0xe59f7008	// ldr r7, [pc, #8]
	.word 0xef000000	// swi #0
	.word 0xeafffffa	// b . - 16
	.word SYS_sigreturn
	.word SYS_exit
	.align	3
	.size aarch32_sigcode, . - aarch32_sigcode
aarch32_esigcode:
	.data
	.global sz_aarch32_sigcode
sz_aarch32_sigcode:
	.quad aarch32_esigcode - aarch32_sigcode
