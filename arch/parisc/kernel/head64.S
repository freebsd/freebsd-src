/*
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1999 by Helge Deller
 * Copyright 1999 SuSE GmbH (Philipp Rumpf)
 * Copyright 1999 Philipp Rumpf (prumpf@tux.org)
 * Copyright 2000 Hewlett Packard (Paul Bame, bame@puffin.external.hp.com)
 * Copyright (C) 2001 Grant Grundler (Hewlett Packard)
 *
 * Initial Version 04-23-1999 by Helge Deller <deller@gmx.de>
 */


#include <linux/autoconf.h>	/* for CONFIG_SMP */

#include <asm/offset.h>
#include <asm/psw.h>

#include <asm/assembly.h>
#include <asm/pgtable.h>
#include <asm/pdc.h>		/* for PDC_PSW defines */


	.level 2.0w

	.section	.initcall.init
	.align		4
	.export __initcall_start
__initcall_start:
	.export __initcall_end
__initcall_end:
	.export __setup_start
__setup_start:
	.export __setup_end
__setup_end:

	.data

	.export boot_args
boot_args:
	.word 0 /* arg0 */
	.word 0 /* arg1 */
	.word 0 /* arg2 */
	.word 0 /* arg3 */

	.text
	.align	4	

	.export stext
	.export _stext,data		/* Kernel want it this way! */
_stext:
stext:
	.proc
	.callinfo

	/* Make sure sr4-sr7 are set to zero for the kernel address space */
	mtsp	%r0,%sr4
	mtsp	%r0,%sr5
	mtsp	%r0,%sr6
	mtsp	%r0,%sr7

	/* Clear BSS (shouldn't the boot loader do this?) */

	.import _edata,data
	.import _end,data

	ldil            L%PA(_edata),%r3
	ldo             R%PA(_edata)(%r3),%r3
	ldil            L%PA(_end),%r4
	ldo             R%PA(_end)(%r4),%r4
$bss_loop:
	cmpb,<<,n       %r3,%r4,$bss_loop
	stb,ma          %r0,1(%r3)

	/* Save away the arguments the boot loader passed in (32 bit args) */

	ldil            L%PA(boot_args),%r1
	ldo             R%PA(boot_args)(%r1),%r1
	stw,ma          %arg0,4(%r1)
	stw,ma          %arg1,4(%r1)
	stw,ma          %arg2,4(%r1)
	stw,ma          %arg3,4(%r1)

	/* Initialize startup VM. Just map first 8 MB of memory */

	ldil		L%PA(pg0),%r1
	ldo		R%PA(pg0)(%r1),%r1

	ldil		L%PA(pmd0),%r5
	ldo		R%PA(pmd0)(%r5),%r5
	ldo		_PAGE_TABLE(%r5),%r3

	ldil		L%PA(swapper_pg_dir),%r4
	ldo		R%PA(swapper_pg_dir)(%r4),%r4

	mtctl		%r4,%cr24	/* Initialize kernel root pointer */
	mtctl		%r4,%cr25	/* Initialize user root pointer */

#if (__PAGE_OFFSET != 0x10000000UL)
Error! Code below (the next five std's) needs to be changed
#endif

	std             %r3,0x00(%r4)   /* Hardwired 0x1... kernel Vaddr start*/

	ldo		_PAGE_TABLE(%r1),%r3
	std             %r3,0x400(%r5)  /* Hardwired 0x1... kernel Vaddr start*/
	ldo		0x1000(%r3),%r3
	std             %r3,0x408(%r5)
	ldo		0x1000(%r3),%r3
	std             %r3,0x410(%r5)
	ldo		0x1000(%r3),%r3
	std             %r3,0x418(%r5)

	ldo		_PAGE_KERNEL(%r0),%r3 /* Hardwired 0 phys addr start */
$pgt_fill_loop:
	std,ma		%r3,8(%r1)
	ldo		0x1000(%r3),%r3
	bb,>=		%r3,8,$pgt_fill_loop
	nop

	/* And the RFI Target address too */
	load32          start_kernel, %r11

	/* And the stack pointer too */
	load32		PA(init_task_union+TASK_SZ_ALGN),%sp

	/* And the initial task pointer */

	load32          init_task_union,%r6
	mtctl           %r6,%cr30

	/* And the interrupt stack */

	load32          interrupt_stack,%r6
	mtctl           %r6,%cr31

	/* Act like PDC just called us - that's how slave CPUs enter */
#define MEM_PDC_LO 0x388
#define MEM_PDC_HI 0x35C
	ldw		MEM_PDC_LO(%r0),%r3
	ldw		MEM_PDC_HI(%r0),%r6
	depd		%r6, 31, 32, %r3	/* move to upper word */

#ifdef CONFIG_SMP
	/* Set the smp rendevous address into page zero.
	** It would be safer to do this in init_smp_config() but
	** it's just way easier to deal with here because
	** of 64-bit function ptrs and the address is local to this file.
	*/
	ldil		L%PA(smp_slave_stext),%r10
	ldo		R%PA(smp_slave_stext)(%r10),%r10
	stw		%r10,0x10(%r0)	/* MEM_RENDEZ */
	stw		%r0,0x28(%r0)	/* MEM_RENDEZ_HI - assume addr < 4GB */

	/* FALLTHROUGH */
	.procend

	/*
	** Code Common to both Monarch and Slave processors.
	** Entry:
	**    %r3	PDCE_PROC address
	**    %r11	RFI target address.
	**
	** Caller must init: SR4-7, %sp, %r10, %cr24/25, 
	*/
common_stext:
	.proc
	.callinfo
#else /* CONFIG_SMP */
	/* Clear PDC's CPU handoff address - we won't use it */
	stw		%r0,0x10(%r0)	/* MEM_RENDEZ */
	stw		%r0,0x28(%r0)	/* MEM_RENDEZ_HI */
#endif /* CONFIG_SMP */

	/* Save the rfi target address */
	std		%r11,  TASK_PT_GR11-TASK_SZ_ALGN(%sp)

#ifndef CONFIG_PDC_NARROW
	/* Switch to wide mode; Superdome doesn't support narrow PDC
	** calls.
	*/
1:	mfia		%rp		/* clear upper part of pcoq */
	ldo		2f-1b(%rp),%rp
	depdi		0,31,32,%rp
	bv		(%rp)
	ssm		PSW_SM_W,%r0
2:
#endif /* CONFIG_PDC_NARROW */

	/* Set Wide mode as the "Default" (eg for traps)
	** First trap occurs *right* after (or part of) rfi for slave CPUs.
	** Someday, palo might not do this for the Monarch either.
	*/

	ldo		PDC_PSW(%r0),%arg0		/* 21 */
	ldo		PDC_PSW_SET_DEFAULTS(%r0),%arg1	/* 2 */
	ldo		PDC_PSW_WIDE_BIT(%r0),%arg2	/* 2 */

	load32		PA(stext_pdc_ret), %rp

	bv		(%r3)
	copy		%r0,%arg3

stext_pdc_ret:
	/* restore rfi target address*/
	ldd		TASK_PT_GR11-TASK_SZ_ALGN(%sp), %r11

	/* PARANOID: clear user scratch/user space SR's */
	mtsp	%r0,%sr0
	mtsp	%r0,%sr1
	mtsp	%r0,%sr2
	mtsp	%r0,%sr3

	/* Initialize Protection Registers */
	mtctl	%r0,%cr8
	mtctl	%r0,%cr9
	mtctl	%r0,%cr12
	mtctl	%r0,%cr13

	/* Prepare to RFI! Man all the cannons! */
	tovirt_r1       %sp

	/* Initialize the global data pointer */
	load32		__gp,%dp

	/* Set up our interrupt table.  HPMCs might not work after this! */
	ldil		L%PA(fault_vector_20),%r10
	ldo		R%PA(fault_vector_20)(%r10),%r10
	mtctl		%r10,%cr14

	b		aligned_rfi
	nop

	/* the magic spell */
	.align          256
aligned_rfi:
	ssm		0,0
	nop		/* 1 */
	nop		/* 2 */
	nop		/* 3 */
	nop		/* 4 */
	nop		/* 5 */
	nop		/* 6 */
	nop		/* 7 */
	nop		/* 8 */

	/* turn off troublesome PSW bits */
	rsm		PSW_Q+PSW_I+PSW_D+PSW_P+PSW_R, %r0

	/* kernel PSW:
	 *  - no interruptions except HPMC and TOC (which are handled by PDC)
	 *  - Q bit set (IODC / PDC interruptions)
	 *  - big-endian
	 *  - virtually mapped
	 */
	load32		KERNEL_PSW,%r10
	mtctl		%r10,%ipsw

	/* Set the space pointers for the post-RFI world
	** Clear the two-level IIA Space Queue, effectively setting
	** Kernel space.
	*/
	mtctl		%r0,%cr17
	mtctl		%r0,%cr17

	/* Load RFI target into PC queue */
	mtctl		%r11,%cr18
	ldo		4(%r11),%r11
	mtctl		%r11,%cr18

	/* Jump to hyperspace */
	rfi
	nop

	.procend


#ifdef CONFIG_SMP

	.import smp_init_current_idle_task,data
	.import	smp_callin,code

/***************************************************************************
*
* smp_slave_stext is executed by all non-monarch Processors when the Monarch
* pokes the slave CPUs in smp.c:smp_boot_cpus().
*
* Once here, registers values are initialized in order to branch to virtual
* mode. Once all available/eligible CPUs are in virtual mode, all are
* released and start out by executing their own idle task.
*****************************************************************************/


smp_slave_stext:
        .proc
	.callinfo

	/*
	** Initialize Space registers
	*/
	mtsp	   %r0,%sr4
	mtsp	   %r0,%sr5
	mtsp	   %r0,%sr6
	mtsp	   %r0,%sr7

	/*  Initialize the SP - monarch sets up smp_init_current_idle_task */
	load32		PA(smp_init_current_idle_task),%sp
	ldd		0(%sp),%sp	/* load task address */
	mtctl           %sp,%cr30       /* store in cr30 */
	ldo             TASK_SZ_ALGN(%sp),%sp
	tophys_r1       %sp

	/* point CPU to kernel page tables */
	load32		PA(swapper_pg_dir),%r4
	mtctl		%r4,%cr24	/* Initialize kernel root pointer */
	mtctl		%r4,%cr25	/* Initialize user root pointer */

	/* Setup PDCE_PROC entry */
	copy		%arg0,%r3

	/* Load RFI target address.  */
	load32		smp_callin, %r11
	
	/* ok...common code can handle the rest */
	b		common_stext
	nop

	.procend
#endif /* CONFIG_SMP */

