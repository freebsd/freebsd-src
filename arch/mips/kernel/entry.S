/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994 - 2000, 2001 by Ralf Baechle
 * Copyright (C) 1999, 2000 Silicon Graphics, Inc.
 * Copyright (C) 2001 MIPS Technologies, Inc.
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/sys.h>

#include <asm/addrspace.h>
#include <asm/asm.h>
#include <asm/cacheops.h>
#include <asm/current.h>
#include <asm/errno.h>
#include <asm/mipsregs.h>
#include <asm/page.h>
#include <asm/pgtable-bits.h>
#include <asm/stackframe.h>
#include <asm/processor.h>
#include <asm/regdef.h>
#include <asm/fpregdef.h>
#include <asm/unistd.h>
#include <asm/isadep.h>


		.text
		.align	5
		.set	push
		.set	reorder
FEXPORT(ret_from_irq)
FEXPORT(ret_from_exception)
		lw	t0, PT_STATUS(sp)	# returning to kernel mode?
		andi	t0, t0, KU_USER
		beqz	t0, restore_all

FEXPORT(ret_from_sys_call)		# here to prevent code duplication
ret_from_schedule:
		mfc0	t0, CP0_STATUS	# need_resched and signals atomic test
		ori	t0, t0, 1
		xori	t0, t0, 1
		mtc0	t0, CP0_STATUS
		SSNOP; SSNOP; SSNOP

		lw	v0, TASK_NEED_RESCHED($28)
		lw	v1, TASK_SIGPENDING($28)
		bnez	v0, reschedule
		bnez	v1, signal_return
restore_all:	.set	noat
		RESTORE_ALL_AND_RET
		.set	at

/* Put this behind restore_all for the sake of the branch prediction.  */
signal_return:
		.type	signal_return, @function

		mfc0	t0, CP0_STATUS
		ori	t0, t0, 1
		mtc0	t0, CP0_STATUS

		move	a0, zero
		move	a1, sp
		jal	do_signal
		b	restore_all

reschedule:
		jal	schedule
		b	ret_from_schedule

/*
 * Common spurious interrupt handler.
 */
		.text
		.align  5
LEAF(spurious_interrupt)
		/*
		 * Someone tried to fool us by sending an interrupt but we
		 * couldn't find a cause for it.
		 */
		lui     t1,%hi(irq_err_count)
		lw      t0,%lo(irq_err_count)(t1)
		addiu   t0,1
		sw      t0,%lo(irq_err_count)(t1)
		j	ret_from_irq
END(spurious_interrupt)

		__INIT

		.set	reorder

NESTED(except_vec1_generic, 0, sp)
		PANIC("Exception vector 1 called")
END(except_vec1_generic)

		/*
		 * General exception vector.  Used for all CPUs except R4000
		 * and R4400 SC and MC versions.
		 */
NESTED(except_vec3_generic, 0, sp)
#if R5432_CP0_INTERRUPT_WAR
		mfc0	k0, CP0_INDEX
#endif
		mfc0	k1, CP0_CAUSE
		la	k0, exception_handlers
		andi	k1, k1, 0x7c
		addu	k0, k0, k1
		lw	k0, (k0)
		jr	k0
END(except_vec3_generic)
		.set	at

		/* General exception vector R4000 version. */
NESTED(except_vec3_r4000, 0, sp)
		.set	push
		.set    mips3
		.set	noat
		mfc0	k1, CP0_CAUSE
		li	k0, 31<<2
		andi	k1, k1, 0x7c
		.set	noreorder
		beq	k1, k0, handle_vced
		 li	k0, 14<<2
		beq	k1, k0, handle_vcei
		 la	k0, exception_handlers
		.set	reorder
		addu	k0, k0, k1
		lw	k0, (k0)
		jr	k0

		/*
		 * Big shit, we now may have two dirty primary cache lines for
		 * the same physical address.  We can savely invalidate the
		 * line pointed to by c0_badvaddr because after return from
		 * this exception handler the load / store will be re-executed.
		 */
handle_vced:
		mfc0	k0, CP0_BADVADDR
		li	k1, -4
		and	k0, k1
		mtc0	zero, CP0_TAGLO
		cache	Index_Store_Tag_D,(k0)
		cache	Hit_Writeback_Inv_SD,(k0)
#ifdef CONFIG_PROC_FS
		lui	k0, %hi(vced_count)
		lw	k1, %lo(vced_count)(k0)
		addiu	k1, 1
		sw	k1, %lo(vced_count)(k0)
#endif
		eret

handle_vcei:
		mfc0	k0, CP0_BADVADDR
		cache	Hit_Writeback_Inv_SD, (k0)	# also cleans pi
#ifdef CONFIG_PROC_FS
		lui	k0, %hi(vcei_count)
		lw	k1, %lo(vcei_count)(k0)
		addiu	k1, 1
		sw	k1, %lo(vcei_count)(k0)
#endif
		eret
		.set    pop
END(except_vec3_r4000)

		__FINIT

/*
 * Build a default exception handler for the exceptions that don't need
 * special handlers.  If you didn't know yet - I *like* playing games with
 * the C preprocessor ...
 */
#define __BUILD_clear_none(exception)
#define __BUILD_clear_sti(exception)                                    \
		STI
#define __BUILD_clear_cli(exception)                                    \
		CLI
#define __BUILD_clear_fpe(exception)                                    \
		cfc1	a1,fcr31;                                       \
		li	a2,~(0x3f<<12);                                 \
		and	a2,a1;                                          \
		ctc1	a2,fcr31;                                       \
		STI
#define __BUILD_clear_ade(exception)                                    \
		.set	reorder;					\
		MFC0	t0,CP0_BADVADDR;                                \
		.set	noreorder;					\
		REG_S	t0,PT_BVADDR(sp);                               \
		KMODE
#define __BUILD_silent(exception)

#define fmt "Got %s at %08lx.\n"

#define __BUILD_verbose(exception)                                      \
		la	a1,8f;                                          \
		TEXT	(#exception);                                   \
		REG_L	a2,PT_EPC(sp);                                  \
		PRINT(fmt)
#define __BUILD_count(exception)                                        \
		.set	reorder;                                        \
		lw	t0,exception_count_##exception;                 \
		.set	noreorder;                                      \
		addiu	t0, 1;                                          \
		sw	t0,exception_count_##exception;                 \
		.data;                                                  \
EXPORT(exception_count_##exception);                                    \
		.word	0;                                              \
		.previous;
#define BUILD_HANDLER(exception,handler,clear,verbose)                  \
		.align	5;                                              \
NESTED(handle_##exception, PT_SIZE, sp);		                \
		.set	noat;                                           \
		SAVE_ALL;                                               \
FEXPORT(handle_##exception##_int);					\
		__BUILD_clear_##clear(exception);                       \
		.set	at;                                             \
		__BUILD_##verbose(exception);                           \
		jal	do_##handler;                                   \
		 move	a0, sp;                                         \
		j	ret_from_exception;                             \
		 nop;                                                   \
END(handle_##exception)

		BUILD_HANDLER(adel,ade,ade,silent)		/* #4  */
		BUILD_HANDLER(ades,ade,ade,silent)		/* #5  */
		BUILD_HANDLER(ibe,be,cli,silent)		/* #6  */
		BUILD_HANDLER(dbe,be,cli,silent)		/* #7  */
		BUILD_HANDLER(bp,bp,sti,silent)			/* #9  */
		BUILD_HANDLER(ri,ri,sti,silent)			/* #10 */
		BUILD_HANDLER(cpu,cpu,sti,silent)		/* #11 */
		BUILD_HANDLER(ov,ov,sti,silent)			/* #12 */
		BUILD_HANDLER(tr,tr,sti,silent)			/* #13 */
		BUILD_HANDLER(fpe,fpe,fpe,silent)		/* #15 */
		BUILD_HANDLER(mdmx,mdmx,sti,silent)		/* #22 */
		BUILD_HANDLER(watch,watch,sti,silent)		/* #23 */
		BUILD_HANDLER(mcheck,mcheck,cli,silent)		/* #24 */
		BUILD_HANDLER(reserved,reserved,sti,silent)	/* others */

		.set	pop

/*
 * Table of syscalls
 */
		.data
		.align	PTRLOG
EXPORT(sys_call_table)
#define SYS(call, narg) PTR call

		/* Reserved space for all SVR4 syscalls. */
		.space	(1000)*PTRSIZE

#ifdef CONFIG_BINFMT_IRIX
		/* 32bit IRIX5 system calls. */
#include "irix5sys.h"
#else
		.space	(1000)*PTRSIZE		/* No IRIX syscalls */
#endif

		/* Reserved space for all the BSD43 and POSIX syscalls. */
		.space	(2000)*PTRSIZE

		/* Linux flavoured syscalls. */
#include "syscalls.h"

/*
 * Number of arguments of each syscall
 */
EXPORT(sys_narg_table)
#undef SYS
#define SYS(call, narg) .byte narg

		/* Reserved space for all SVR4 flavoured syscalls. */
		.space	(1000)

#ifdef CONFIG_BINFMT_IRIX
		/* 32bit IRIX5 system calls. */
#include "irix5sys.h"
#else
		.space	(1000)			/* No IRIX syscalls */
#endif

		/* Reserved space for all the BSD43 and POSIX syscalls. */
		.space	(2000)

		/* Linux flavoured syscalls. */
#include "syscalls.h"
