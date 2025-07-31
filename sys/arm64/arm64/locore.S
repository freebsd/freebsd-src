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
 */

#include "assym.inc"
#include "opt_kstack_pages.h"
#include <sys/elf_common.h>
#include <sys/syscall.h>
#include <machine/asm.h>
#include <machine/armreg.h>
#include <machine/cpu.h>
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
#elif PAGE_SIZE == PAGE_SIZE_4K
/*
 * Space for a level 3 table holding the end of the executable memory and
 * the start of the non-executable data.
 */
#define	L3_PAGE_COUNT	1
#endif

/*
 * The size of our bootstrap stack.
 */
#define	BOOT_STACK_SIZE	(KSTACK_PAGES * PAGE_SIZE)

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
	/* Enter the kernel exception level */
	bl	enter_kernel_el

	/* Set the context id */
	msr	contextidr_el1, xzr

	/* Get the virt -> phys offset */
	bl	get_load_phys_addr

	/*
	 * At this point:
	 * x28 = Our physical load address
	 */

	/* Create the page tables */
	bl	create_pagetables

	/*
	 * At this point:
	 * x27 = TTBR0 table
	 * x24 = TTBR1 table
	 * x22 = PTE shareability attributes
	 * x21 = BTI guarded page attribute if supported
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
	BTI_J

	/* Set up the stack */
	adrp	x25, initstack_end
	add	x25, x25, :lo12:initstack_end
	sub	sp, x25, #PCB_SIZE

	/* Zero the BSS */
	ldr	x15, .Lbss
	ldr	x14, .Lend
1:
	stp	xzr, xzr, [x15], #16
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

	str	x1,  [x0, #BP_MODULEP]
	adrp	x25, initstack
	add	x25, x25, :lo12:initstack
	str	x25, [x0, #BP_KERN_STACK]
	str	x27, [x0, #BP_KERN_TTBR0]
	str	x23, [x0, #BP_BOOT_EL]

	/* Set these before they are used in kasan_init_early */
	adrp	x1, pmap_sh_attr
	str	x22, [x1, :lo12:pmap_sh_attr]
#ifdef __ARM_FEATURE_BTI_DEFAULT
	adrp	x1, pmap_gp_attr
	str	x21, [x1, :lo12:pmap_gp_attr]
#endif

#ifdef KASAN
	/* Save bootparams */
	mov	x19, x0

	/* Bootstrap an early shadow map for the boot stack. */
	ldr	x0, [x0, #BP_KERN_STACK]
	ldr	x1, =BOOT_STACK_SIZE
	bl	kasan_init_early

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
 * void
 * mpentry_psci(unsigned long)
 *
 * Called by a core when it is being brought online with psci.
 * The data in x0 is passed straight to init_secondary.
 */
ENTRY(mpentry_psci)
	mov	x26, xzr
	b	mpentry_common
END(mpentry_psci)

/*
 * void
 * mpentry_spintable(void)
 *
 * Called by a core when it is being brought online with a spin-table.
 * Reads the new CPU ID and passes this to init_secondary.
 */
ENTRY(mpentry_spintable)
	ldr	x26, =spintable_wait
	b	mpentry_common
END(mpentry_spintable)

/* Wait for the current CPU to be released */
LENTRY(spintable_wait)
	/* Read the affinity bits from mpidr_el1 */
	mrs	x1, mpidr_el1
	ldr	x2, =CPU_AFF_MASK
	and	x1, x1, x2

	adrp	x2, ap_cpuid
1:
	ldr	x0, [x2, :lo12:ap_cpuid]
	cmp	x0, x1
	b.ne	1b

	str	xzr, [x2, :lo12:ap_cpuid]
	dsb	sy
	sev

	ret
LEND(mpentry_spintable)

LENTRY(mpentry_common)
	/* Disable interrupts */
	msr	daifset, #DAIF_INTR

	/* Enter the kernel exception level */
	bl	enter_kernel_el

	/* Set the context id */
	msr	contextidr_el1, xzr

	/* Load the kernel page table */
	adrp	x24, pagetable_l0_ttbr1
	add	x24, x24, :lo12:pagetable_l0_ttbr1
	/* Load the identity page table */
	adrp	x27, pagetable_l0_ttbr0_bootstrap
	add	x27, x27, :lo12:pagetable_l0_ttbr0_bootstrap

	/* Enable the mmu */
	bl	start_mmu

	/* Load the new ttbr0 pagetable */
	adrp	x27, pagetable_l0_ttbr0
	add	x27, x27, :lo12:pagetable_l0_ttbr0

	/* Jump to the virtual address space */
	ldr	x15, =mp_virtdone
	br	x15

mp_virtdone:
	BTI_J

	/*
	 * Allow this CPU to wait until the kernel is ready for it,
	 * e.g. with spin-table but each CPU uses the same release address
	 */
	cbz	x26, 1f
	blr	x26
1:

	/* Start using the AP boot stack */
	adrp	x4, bootstack
	ldr	x4, [x4, :lo12:bootstack]
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

	/*
	 * Initialize the per-CPU pointer before calling into C code, for the
	 * benefit of kernel sanitizers.
	 */
	adrp	x18, bootpcpu
	ldr	x18, [x18, :lo12:bootpcpu]
	msr	tpidr_el1, x18

	b	init_secondary
LEND(mpentry_common)
#endif

/*
 * Enter the exception level the kernel will use:
 *
 *  - If in EL1 continue in EL1
 *  - If the CPU supports FEAT_VHE then set HCR_E2H and HCR_TGE and continue
 *    in EL2
 *  - Configure EL2 to support running the kernel at EL1 and exit to that
 */
LENTRY(enter_kernel_el)
	mrs	x23, CurrentEL
	and	x23, x23, #(CURRENTEL_EL_MASK)
	cmp	x23, #(CURRENTEL_EL_EL2)
	b.eq	1f

	ldr	x2, =SCTLR_MMU_OFF
	msr	sctlr_el1, x2
	/* SCTLR_EOS is set so eret is a context synchronizing event so we
	 * need an isb here to ensure it's observed by later instructions,
	 * but don't need it in the eret below.
	 */
	isb

	/* Ensure SPSR_EL1 and pstate are in sync. The only wat to set the
	 * latter is to set the former and return from an exception with eret.
	 */
	mov	x2, #(PSR_DAIF | PSR_M_EL1h)
	msr	spsr_el1, x2
	msr	elr_el1, lr
	eret

1:
	dsb	sy
	/*
	 * Set just the reserved bits in sctlr_el2. This will disable the
	 * MMU which may have broken the kernel if we enter the kernel in
	 * EL2, e.g. when using VHE.
	 */
	ldr	x2, =(SCTLR_EL2_RES1 | SCTLR_EL2_EIS | SCTLR_EL2_EOS)
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

	/* Set the initial sctlr_el1 */
	ldr	x2, =SCTLR_MMU_OFF
	msr	sctlr_el1, x2

	/* Check for VHE */
	CHECK_CPU_FEAT(x2, ID_AA64MMFR1, VH, IMPL, .Lno_vhe)

	/*
	 * The kernel will be running in EL2, route exceptions here rather
	 * than EL1.
	 */
	orr	x4, x4, #HCR_E2H
	orr	x4, x4, #HCR_TGE
	msr	hcr_el2, x4
	isb

	msr	SCTLR_EL12_REG, x2
	mov	x2, xzr /* CPTR_EL2 is managed by vfp.c */
	ldr	x3, =(CNTHCTL_E2H_EL1PCTEN | CNTHCTL_E2H_EL1PTEN)
	ldr	x5, =(PSR_DAIF | PSR_M_EL2h)
	b	.Ldone_vhe

.Lno_vhe:
	/* Hypervisor trap functions */
	adrp	x2, hyp_stub_vectors
	add	x2, x2, :lo12:hyp_stub_vectors
	msr	vbar_el2, x2

	ldr	x2, =(CPTR_RES1)
	ldr	x3, =(CNTHCTL_EL1PCTEN | CNTHCTL_EL1PCEN)
	ldr	x5, =(PSR_DAIF | PSR_M_EL1h)

.Ldone_vhe:

	msr	cptr_el2, x2
	/* Enable access to the physical timers at EL1 */
	msr	cnthctl_el2, x3
	/* Set the return PSTATE */
	msr	spsr_el2, x5

	/*
	 * Configure the Extended Hypervisor register. This is only valid if
	 * FEAT_HCX is enabled.
	 */
	CHECK_CPU_FEAT(x2, ID_AA64MMFR1, HCX, IMPL, 2f)
	/* Extended Hypervisor Configuration */
	msr	HCRX_EL2_REG, xzr
	isb
2:

	/* Don't trap to EL2 for CP15 traps */
	msr	hstr_el2, xzr

	/* Set the counter offset to a known value */
	msr	cntvoff_el2, xzr

	/* Zero vttbr_el2 so a hypervisor can tell the host and guest apart */
	msr	vttbr_el2, xzr

	/* Check the CPU supports GIC, and configure the CPU interface */
	CHECK_CPU_FEAT(x2, ID_AA64PFR0, GIC, CPUIF_EN, 3f)

	mrs	x2, icc_sre_el2
	orr	x2, x2, #ICC_SRE_EL2_EN	/* Enable access from insecure EL1 */
	orr	x2, x2, #ICC_SRE_EL2_SRE	/* Enable system registers */
	msr	icc_sre_el2, x2
3:

	/* Set the address to return to our return address */
	msr	elr_el2, x30
	isb

	eret
LEND(enter_kernel_el)

/*
 * Get the physical address the kernel was loaded at.
 */
LENTRY(get_load_phys_addr)
	/* Load the offset of get_load_phys_addr from KERNBASE */
	ldr	x28, =(get_load_phys_addr - KERNBASE)
	/* Load the physical address of get_load_phys_addr */
	adr	x29, get_load_phys_addr
	/* Find the physical address of KERNBASE, i.e. our load address */
	sub	x28, x29, x28
	ret
LEND(get_load_phys_addr)

/*
 * This builds the page tables containing the identity map, and the kernel
 * virtual map.
 *
 * It relys on:
 *  We were loaded to an address that is on a 2MiB boundary
 *  All the memory must not cross a 1GiB boundaty
 *  x28 contains the physical address we were loaded from
 *
 *  There are 7 or 8 pages before that address for the page tables
 *   The pages used are:
 *    - The Kernel L3 tables (only for 16k kernel)
 *    - The Kernel L2 table
 *    - The Kernel L1 table
 *    - The Kernel L0 table             (TTBR1)
 *    - The identity (PA = VA) L2 table
 *    - The identity (PA = VA) L1 table
 *    - The identity (PA = VA) L0 table (Early TTBR0)
 *    - The Kernel empty L0 table       (Late TTBR0)
 */
LENTRY(create_pagetables)
	/* Save the Link register */
	mov	x5, x30

	/* Clean the page table */
	adrp	x6, pagetable
	add	x6, x6, :lo12:pagetable
	adrp	x27, pagetable_end
	add	x27, x27, :lo12:pagetable_end
1:
	stp	xzr, xzr, [x6], #16
	stp	xzr, xzr, [x6], #16
	stp	xzr, xzr, [x6], #16
	stp	xzr, xzr, [x6], #16
	cmp	x6, x27
	b.lo	1b

#ifdef __ARM_FEATURE_BTI_DEFAULT
	/*
	 * Check if the CPU supports BTI
	 */
	mrs	x6, id_aa64pfr1_el1		/* Read the ID register */
	and	x6, x6, ID_AA64PFR1_BT_MASK	/* Mask the field we need */
	cmp	x6, xzr				/* Check it's non-zero */
	cset	x6, ne				/* x6 is set if non-zero */
	lsl	x21, x6, ATTR_S1_GP_SHIFT	/* Shift to the correct bit */
#endif

	/*
	 * Find the shareability attribute we should use. If FEAT_LPA2 is
	 * enabled then the shareability field is moved from the page table
	 * to tcr_el1 and the bits in the page table are reused by the
	 * address field.
	 */
#if PAGE_SIZE == PAGE_SIZE_4K
#define	LPA2_MASK	ID_AA64MMFR0_TGran4_MASK
#define	LPA2_VAL	ID_AA64MMFR0_TGran4_LPA2
#elif PAGE_SIZE == PAGE_SIZE_16K
#define	LPA2_MASK	ID_AA64MMFR0_TGran16_MASK
#define	LPA2_VAL	ID_AA64MMFR0_TGran16_LPA2
#else
#error Unsupported page size
#endif
	mrs	x6, id_aa64mmfr0_el1
	mov	x7, LPA2_VAL
	and	x6, x6, LPA2_MASK
	cmp	x6, x7
	ldr	x22, =(ATTR_SH(ATTR_SH_IS))
	csel	x22, xzr, x22, eq
#undef LPA2_MASK
#undef LPA2_VAL

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
	 * Create L3 and L3C pages. The kernel will be loaded at a 2M aligned
	 * address, enabling the creation of L3C pages. However, when the page
	 * size is larger than 4k, L2 blocks are too large to map the kernel
	 * with 2M alignment.
	 */
#define	PTE_SHIFT	L3_SHIFT
#define	LL_PAGE_TABLE	pagetable_l3_ttbr1
#define	BUILD_PTE_FUNC	build_l3_page_pagetable
#else
#define	PTE_SHIFT	L2_SHIFT
#define	LL_PAGE_TABLE	pagetable_l2_ttbr1
#define	BUILD_PTE_FUNC	build_l2_block_pagetable
#endif

	/* Get the number of blocks/pages to allocate, rounded down */
	lsr	x14, x8, #(PTE_SHIFT)

	ldr	x26, =etext
#if PAGE_SIZE != PAGE_SIZE_4K
	ldr	x8, =((1 << PTE_SHIFT) - 1)
	add	x26, x26, x8
#endif
	mov	x8, #(KERNBASE)
	sub	x25, x26, x8
	lsr	x25, x25, #(PTE_SHIFT)

#if PAGE_SIZE == PAGE_SIZE_4K
	/* Calculate the number of executable level 3 pages to create */
	lsr	x26, x26, #(L3_SHIFT)
	bfc	x26, #(Ln_ENTRIES_SHIFT), #(64 - Ln_ENTRIES_SHIFT)

	/* Build the L3 table holding the end of the exectuable code */
	lsl	x15, x25, #(PTE_SHIFT)
	adrp	x6, pagetable_l3_ttbr1
	add	x6, x6, :lo12:pagetable_l3_ttbr1
	ldr	x7, =(ATTR_S1_IDX(VM_MEMATTR_WRITE_BACK) | \
	    ATTR_S1_AP(ATTR_S1_AP_RO))
	ldr	x8, =(KERNBASE)
	add	x8, x8, x15
	add	x9, x28, x15
	mov	x10, x26
	bl	build_l3_page_pagetable

	/* Build the remaining level 3 pages */
	ldr	x7, =(ATTR_S1_IDX(VM_MEMATTR_WRITE_BACK) | ATTR_S1_XN)
	lsl	x27, x26, #(L3_SHIFT)
	add	x8, x8, x27
	add	x9, x28, x15
	add	x9, x9, x27
	ldr	x10, =(Ln_ENTRIES)
	sub	x10, x10, x26
	bl	build_l3_page_pagetable

	/* Link the l2 -> l3 table */
	mov	x9, x6
	adrp	x6, pagetable_l2_ttbr1
	add	x6, x6, :lo12:pagetable_l2_ttbr1
	bl	link_l2_pagetable
#endif

	/* Create the kernel space PTE table */
	adrp	x6, LL_PAGE_TABLE
	add	x6, x6, :lo12:LL_PAGE_TABLE
	ldr	x7, =(ATTR_S1_IDX(VM_MEMATTR_WRITE_BACK) | \
	    ATTR_S1_AP(ATTR_S1_AP_RO))
	mov	x8, #(KERNBASE)
	mov	x9, x28
	mov	x10, x25
	bl	BUILD_PTE_FUNC

#if PAGE_SIZE == PAGE_SIZE_4K
	/* Skip memory mapped through the L2 table */
	add	x25, x25, #1
#endif

	/* Create the kernel space XN PTE table */
	lsl	x10, x25, #(PTE_SHIFT)
	ldr	x7, =(ATTR_S1_IDX(VM_MEMATTR_WRITE_BACK) | ATTR_S1_XN)
	ldr	x8, =(KERNBASE)
	add	x8, x8, x10
	add	x9, x28, x10
	sub	x10, x14, x25
	bl	BUILD_PTE_FUNC

#undef PTE_SHIFT
#undef LL_PAGE_TABLE
#undef BUILD_PTE_FUNC

#if PAGE_SIZE != PAGE_SIZE_4K
	/* Link the l2 -> l3 table */
	mov	x9, x6
	adrp	x6, pagetable_l2_ttbr1
	add	x6, x6, :lo12:pagetable_l2_ttbr1
	bl	link_l2_pagetable
#endif

	/* Link the l1 -> l2 table */
	mov	x9, x6
	adrp	x6, pagetable_l1_ttbr1
	add	x6, x6, :lo12:pagetable_l1_ttbr1
	bl	link_l1_pagetable

	/* Link the l0 -> l1 table */
	mov	x9, x6
	adrp	x6, pagetable_l0_ttbr1
	add	x6, x6, :lo12:pagetable_l0_ttbr1
	mov	x10, #1
	bl	link_l0_pagetable

	/* Save the TTBR1 table physical address */
	mov	x24, x6

	/*
	 * Build the TTBR0 maps.  As TTBR0 maps, they must specify ATTR_S1_nG.
	 * They are only needed early on, so the VA = PA map is uncached.
	 */

	adrp	x6, pagetable_l2_ttbr0_bootstrap
	add	x6, x6, :lo12:pagetable_l2_ttbr0_bootstrap

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

	/* Link the l1 -> l2 table */
	mov	x9, x6
	adrp	x6, pagetable_l1_ttbr0_bootstrap
	add	x6, x6, :lo12:pagetable_l1_ttbr0_bootstrap
	bl	link_l1_pagetable

	/* Link the l0 -> l1 table */
	mov	x9, x6
	adrp	x6, pagetable_l0_ttbr0_bootstrap
	add	x6, x6, :lo12:pagetable_l0_ttbr0_bootstrap
	mov	x10, #1
	bl	link_l0_pagetable

	/* Save the TTBR0 table physical address */
	mov	x27, x6

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
	orr	x12, x12, #(ATTR_AF)
	orr	x12, x12, #(ATTR_S1_UXN)
#ifdef __ARM_FEATURE_BTI_DEFAULT
	orr	x12, x12, x21
#endif
	/* Set the shareability attribute */
	orr	x12, x12, x22

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
 * Builds count level 3 page table entries. Uses ATTR_CONTIGUOUS to create
 * large page (L3C) mappings when the current VA and remaining count allow
 * it.
 *  x6  = L3 table
 *  x7  = Block attributes
 *  x8  = VA start
 *  x9  = PA start (trashed)
 *  x10 = Entry count (trashed)
 *  x11, x12 and x13 are trashed
 *
 * VA start (x8) modulo L3C_SIZE must equal PA start (x9) modulo L3C_SIZE.
 */
LENTRY(build_l3_page_pagetable)
	cbz	x10, 2f
	/*
	 * Build the L3 table entry.
	 */
	/* Find the table index */
	lsr	x11, x8, #L3_SHIFT
	and	x11, x11, #Ln_ADDR_MASK

	/* Build the L3 page entry */
	orr	x12, x7, #L3_PAGE
	orr	x12, x12, #(ATTR_AF)
	orr	x12, x12, #(ATTR_S1_UXN)
#ifdef __ARM_FEATURE_BTI_DEFAULT
	orr	x12, x12, x21
#endif
	/* Set the shareability attribute */
	orr	x12, x12, x22

	/* Only use the output address bits */
	lsr	x9, x9, #L3_SHIFT

	/* Check if an ATTR_CONTIGUOUS mapping is possible */
1:	tst	x11, #(L3C_ENTRIES - 1)
	b.ne	2f
	cmp	x10, #L3C_ENTRIES
	b.lo	3f
	orr	x12, x12, #(ATTR_CONTIGUOUS)
	b	2f
3:	and	x12, x12, #(~ATTR_CONTIGUOUS)

	/* Set the physical address for this virtual address */
2:	orr	x13, x12, x9, lsl #L3_SHIFT

	/* Store the entry */
	str	x13, [x6, x11, lsl #3]

	sub	x10, x10, #1
	add	x11, x11, #1
	add	x9, x9, #1
	cbnz	x10, 1b
2:

	ret
LEND(build_l3_page_pagetable)

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

	/* If x22 contains a non-zero value then LPA2 is not implemented */
	cbnz	x22, .Lno_lpa2
	ldr	x3, =(TCR_DS)
	orr	x2, x2, x3
.Lno_lpa2:

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
	 * Check if the HW supports access flag updates, and set
	 * TCR_EL1.HA accordingly. The TCR_EL1.HD flag to enable
	 * HW management of dirty state is set in C code as it may
	 * need to be disabled because of CPU errata.
	 */
	CHECK_CPU_FEAT(x3, ID_AA64MMFR1, HAFDBS, AF, 1f)
	orr	x2, x2, #(TCR_HA)
1:

	msr	tcr_el1, x2

	/*
	 * Setup SCTLR.
	 */
	ldr	x1, =SCTLR_MMU_ON
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

	.quad (TCR_TxSZ(64 - VIRT_BITS) | TCR_TG |			\
	    TCR_SH1_IS | TCR_ORGN1_WBWA | TCR_IRGN1_WBWA |		\
	    TCR_SH0_IS | TCR_ORGN0_WBWA | TCR_IRGN0_WBWA)
LEND(start_mmu)

ENTRY(abort)
	b abort
END(abort)

.bss
	.align	PAGE_SHIFT
initstack:
	.space	BOOT_STACK_SIZE
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
pagetable_l3_ttbr1:
	.space	(PAGE_SIZE * L3_PAGE_COUNT)
pagetable_l2_ttbr1:
	.space	PAGE_SIZE
pagetable_l1_ttbr1:
	.space	PAGE_SIZE
pagetable_l0_ttbr1:
	.space	PAGE_SIZE
pagetable_l2_ttbr0_bootstrap:
	.space	PAGE_SIZE
pagetable_l1_ttbr0_bootstrap:
	.space	PAGE_SIZE
pagetable_l0_ttbr0_bootstrap:
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

GNU_PROPERTY_AARCH64_FEATURE_1_NOTE(GNU_PROPERTY_AARCH64_FEATURE_1_VAL)
