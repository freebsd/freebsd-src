/* $Id: rtrap.S,v 1.57.2.2 2002/03/03 10:31:56 davem Exp $
 * rtrap.S: Preparing for return from trap on Sparc V9.
 *
 * Copyright (C) 1997,1998 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 * Copyright (C) 1997 David S. Miller (davem@caip.rutgers.edu)
 */

#include <asm/asi.h>
#include <asm/pstate.h>
#include <asm/ptrace.h>
#include <asm/spitfire.h>
#include <asm/head.h>
#include <asm/visasm.h>
#include <asm/processor.h>

#define		RTRAP_PSTATE		(PSTATE_RMO|PSTATE_PEF|PSTATE_PRIV|PSTATE_IE)
#define		RTRAP_PSTATE_IRQOFF	(PSTATE_RMO|PSTATE_PEF|PSTATE_PRIV)
#define		RTRAP_PSTATE_AG_IRQOFF	(PSTATE_RMO|PSTATE_PEF|PSTATE_PRIV|PSTATE_AG)

		/* Register %l6 keeps track of whether we are returning
		 * from a system call or not.  It is cleared if we call
		 * do_signal, and it must not be otherwise modified until
		 * we fully commit to returning to userspace.
		 */

		.text
		.align			32
__handle_softirq:
		call			do_softirq
		 nop
		ba,a,pt			%xcc, __handle_softirq_continue
		 nop
__handle_preemption:
		call			schedule
		 wrpr			%g0, RTRAP_PSTATE, %pstate
		ba,pt			%xcc, __handle_preemption_continue
		 wrpr			%g0, RTRAP_PSTATE_IRQOFF, %pstate

__handle_user_windows:
		call			fault_in_user_windows
		 wrpr			%g0, RTRAP_PSTATE, %pstate
		wrpr			%g0, RTRAP_PSTATE_IRQOFF, %pstate
		/* Redo sched+sig checks */
		ldx			[%g6 + AOFF_task_need_resched], %l0
		brz,pt			%l0, 1f
		 nop
		call			schedule

		 wrpr			%g0, RTRAP_PSTATE, %pstate
		wrpr			%g0, RTRAP_PSTATE_IRQOFF, %pstate
1:		lduw			[%g6 + AOFF_task_sigpending], %l0
		brz,pt			%l0, __handle_user_windows_continue
		 nop
		clr			%o0
		mov			%l5, %o2
		mov			%l6, %o3

		add			%sp, PTREGS_OFF, %o1
		call			do_signal
		 wrpr			%g0, RTRAP_PSTATE, %pstate
		wrpr			%g0, RTRAP_PSTATE_IRQOFF, %pstate
		clr			%l6
		/* Signal delivery can modify pt_regs tstate, so we must
		 * reload it.
		 */
		ldx			[%sp + PTREGS_OFF + PT_V9_TSTATE], %l1
		sethi			%hi(0xf << 20), %l4
		and			%l1, %l4, %l4

		ba,pt			%xcc, __handle_user_windows_continue
		 andn			%l1, %l4, %l1
__handle_perfctrs:
		call			update_perfctrs
		 wrpr			%g0, RTRAP_PSTATE, %pstate
		wrpr			%g0, RTRAP_PSTATE_IRQOFF, %pstate
		ldub			[%g6 + AOFF_task_thread + AOFF_thread_w_saved], %o2
		brz,pt			%o2, 1f
		 nop

		/* Redo userwin+sched+sig checks */
		call			fault_in_user_windows
		 wrpr			%g0, RTRAP_PSTATE, %pstate
		wrpr			%g0, RTRAP_PSTATE_IRQOFF, %pstate
1:		ldx			[%g6 + AOFF_task_need_resched], %l0
		brz,pt			%l0, 1f
		 nop
		call			schedule
		 wrpr			%g0, RTRAP_PSTATE, %pstate

		wrpr			%g0, RTRAP_PSTATE_IRQOFF, %pstate
1:		lduw			[%g6 + AOFF_task_sigpending], %l0
		brz,pt			%l0, __handle_perfctrs_continue
		 sethi			%hi(TSTATE_PEF), %o0
		clr			%o0
		mov			%l5, %o2
		mov			%l6, %o3
		add			%sp, PTREGS_OFF, %o1

		call			do_signal
		 wrpr			%g0, RTRAP_PSTATE, %pstate
		wrpr			%g0, RTRAP_PSTATE_IRQOFF, %pstate
		clr			%l6
		/* Signal delivery can modify pt_regs tstate, so we must
		 * reload it.
		 */
		ldx			[%sp + PTREGS_OFF + PT_V9_TSTATE], %l1
		sethi			%hi(0xf << 20), %l4
		and			%l1, %l4, %l4
		andn			%l1, %l4, %l1

		ba,pt			%xcc, __handle_perfctrs_continue
		 sethi			%hi(TSTATE_PEF), %o0
__handle_userfpu:
		rd			%fprs, %l5
		andcc			%l5, FPRS_FEF, %g0
		sethi			%hi(TSTATE_PEF), %o0
		be,a,pn			%icc, __handle_userfpu_continue
		 andn			%l1, %o0, %l1
		ba,a,pt			%xcc, __handle_userfpu_continue

__handle_signal:
		clr			%o0
		mov			%l5, %o2
		mov			%l6, %o3
		add			%sp, PTREGS_OFF, %o1
		call			do_signal
		 wrpr			%g0, RTRAP_PSTATE, %pstate
		wrpr			%g0, RTRAP_PSTATE_IRQOFF, %pstate
		clr			%l6

		/* Signal delivery can modify pt_regs tstate, so we must
		 * reload it.
		 */
		ldx			[%sp + PTREGS_OFF + PT_V9_TSTATE], %l1
		sethi			%hi(0xf << 20), %l4
		and			%l1, %l4, %l4
		ba,pt			%xcc, __handle_signal_continue
		 andn			%l1, %l4, %l1

		.align			64
		.globl			rtrap_clr_l6, rtrap, irqsz_patchme, rtrap_xcall
rtrap_clr_l6:	clr			%l6
rtrap:		lduw			[%g6 + AOFF_task_processor], %l0
		sethi			%hi(irq_stat), %l2	! &softirq_active
		or			%l2, %lo(irq_stat), %l2	! &softirq_active
irqsz_patchme:	sllx			%l0, 0, %l0
		lduw			[%l2 + %l0], %l1	! softirq_pending
		cmp			%l1, 0

		/* mm/ultra.S:xcall_report_regs KNOWS about this load. */
		bne,pn			%icc, __handle_softirq
		 ldx			[%sp + PTREGS_OFF + PT_V9_TSTATE], %l1

__handle_softirq_continue:
rtrap_xcall:
		sethi			%hi(0xf << 20), %l4
		andcc			%l1, TSTATE_PRIV, %l3
		and			%l1, %l4, %l4
		bne,pn			%icc, to_kernel
		 andn			%l1, %l4, %l1

		/* We must hold IRQs off and atomically test schedule+signal
		 * state, then hold them off all the way back to userspace.
		 * If we are returning to kernel, none of this matters.
		 *
		 * If we do not do this, there is a window where we would do
		 * the tests, later the signal/resched event arrives but we do
		 * not process it since we are still in kernel mode.  It would
		 * take until the next local IRQ before the signal/resched
		 * event would be handled.
		 *
		 * This also means that if we have to deal with performance
		 * counters or user windows, we have to redo all of these
		 * sched+signal checks with IRQs disabled.
		 */
to_user:	wrpr			%g0, RTRAP_PSTATE_IRQOFF, %pstate
		wrpr			0, %pil
__handle_preemption_continue:
		ldx			[%g6 + AOFF_task_need_resched], %l0
		brnz,pn			%l0, __handle_preemption
		 lduw			[%g6 + AOFF_task_sigpending], %l0
		brnz,pn			%l0, __handle_signal
		 nop
__handle_signal_continue:
		ldub			[%g6 + AOFF_task_thread + AOFF_thread_w_saved], %o2
		brnz,pn			%o2, __handle_user_windows
		 nop
__handle_user_windows_continue:
		ldub			[%g6 + AOFF_task_thread + AOFF_thread_flags], %l5
		andcc			%l5, SPARC_FLAG_PERFCTR, %g0
		sethi			%hi(TSTATE_PEF), %o0
		bne,pn			%xcc, __handle_perfctrs
__handle_perfctrs_continue:
		 andcc			%l1, %o0, %g0

		/* This fpdepth clear is neccessary for non-syscall rtraps only */
		bne,pn			%xcc, __handle_userfpu
		 stb			%g0, [%g6 + AOFF_task_thread + AOFF_thread_fpdepth]
__handle_userfpu_continue:

rt_continue:	ldx			[%sp + PTREGS_OFF + PT_V9_G1], %g1
		ldx			[%sp + PTREGS_OFF + PT_V9_G2], %g2

		ldx			[%sp + PTREGS_OFF + PT_V9_G3], %g3
		ldx			[%sp + PTREGS_OFF + PT_V9_G4], %g4
		ldx			[%sp + PTREGS_OFF + PT_V9_G5], %g5
		ldx			[%sp + PTREGS_OFF + PT_V9_G6], %g6
		ldx			[%sp + PTREGS_OFF + PT_V9_G7], %g7
		wrpr			%g0, RTRAP_PSTATE_AG_IRQOFF, %pstate
		ldx			[%sp + PTREGS_OFF + PT_V9_I0], %i0
		ldx			[%sp + PTREGS_OFF + PT_V9_I1], %i1

		ldx			[%sp + PTREGS_OFF + PT_V9_I2], %i2
		ldx			[%sp + PTREGS_OFF + PT_V9_I3], %i3
		ldx			[%sp + PTREGS_OFF + PT_V9_I4], %i4
		ldx			[%sp + PTREGS_OFF + PT_V9_I5], %i5
		ldx			[%sp + PTREGS_OFF + PT_V9_I6], %i6
		ldx			[%sp + PTREGS_OFF + PT_V9_I7], %i7
		ldx			[%sp + PTREGS_OFF + PT_V9_TPC], %l2
		ldx			[%sp + PTREGS_OFF + PT_V9_TNPC], %o2

		ld			[%sp + PTREGS_OFF + PT_V9_Y], %o3
		wr			%o3, %g0, %y
		srl			%l4, 20, %l4
		wrpr			%l4, 0x0, %pil
		wrpr			%g0, 0x1, %tl
		wrpr			%l1, %g0, %tstate
		wrpr			%l2, %g0, %tpc
		wrpr			%o2, %g0, %tnpc

		brnz,pn			%l3, kern_rtt
		 mov			PRIMARY_CONTEXT, %l7
		ldxa			[%l7 + %l7] ASI_DMMU, %l0
		stxa			%l0, [%l7] ASI_DMMU
		flush			%g6
		rdpr			%wstate, %l1
		rdpr			%otherwin, %l2
		srl			%l1, 3, %l1

		wrpr			%l2, %g0, %canrestore
		wrpr			%l1, %g0, %wstate
		wrpr			%g0, %g0, %otherwin
		restore
		rdpr			%canrestore, %g1
		wrpr			%g1, 0x0, %cleanwin
		retry
		nop

kern_rtt:	restore
		retry
to_kernel:	ldub			[%g6 + AOFF_task_thread + AOFF_thread_fpdepth], %l5
		brz,pt			%l5, rt_continue
		 srl			%l5, 1, %o0
		add			%g6, AOFF_task_thread + AOFF_thread_fpsaved, %l6
		ldub			[%l6 + %o0], %l2
		sub			%l5, 2, %l5

		add			%g6, AOFF_task_thread + AOFF_thread_gsr, %o1
		andcc			%l2, (FPRS_FEF|FPRS_DU), %g0
		be,pt			%icc, 2f
		 and			%l2, FPRS_DL, %l6
		andcc			%l2, FPRS_FEF, %g0
		be,pn			%icc, 5f
		 sll			%o0, 3, %o5
		rd			%fprs, %g5

		wr			%g5, FPRS_FEF, %fprs
		ldx			[%o1 + %o5], %g5
		add			%g6, AOFF_task_thread + AOFF_thread_xfsr, %o1
		membar			#StoreLoad | #LoadLoad
		sll			%o0, 8, %o2
		add			%g6, AOFF_task_fpregs, %o3
		brz,pn			%l6, 1f
		 add			%g6, AOFF_task_fpregs+0x40, %o4

		ldda			[%o3 + %o2] ASI_BLK_P, %f0
		ldda			[%o4 + %o2] ASI_BLK_P, %f16
1:		andcc			%l2, FPRS_DU, %g0
		be,pn			%icc, 1f
		 wr			%g5, 0, %gsr
		add			%o2, 0x80, %o2
		ldda			[%o3 + %o2] ASI_BLK_P, %f32
		ldda			[%o4 + %o2] ASI_BLK_P, %f48

1:		membar			#Sync
		ldx			[%o1 + %o5], %fsr
2:		stb			%l5, [%g6 + AOFF_task_thread + AOFF_thread_fpdepth]
		ba,pt			%xcc, rt_continue
		 nop
5:		wr			%g0, FPRS_FEF, %fprs
		membar			#StoreLoad | #LoadLoad
		sll			%o0, 8, %o2

		add			%g6, AOFF_task_fpregs+0x80, %o3
		add			%g6, AOFF_task_fpregs+0xc0, %o4
		ldda			[%o3 + %o2] ASI_BLK_P, %f32
		ldda			[%o4 + %o2] ASI_BLK_P, %f48
		membar			#Sync
		wr			%g0, FPRS_DU, %fprs
		ba,pt			%xcc, rt_continue
		 stb			%l5, [%g6 + AOFF_task_thread + AOFF_thread_fpdepth]
