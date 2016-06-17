/* $Id: etrap.S,v 1.45 2001/09/07 21:04:40 kanoj Exp $
 * etrap.S: Preparing for entry into the kernel on Sparc V9.
 *
 * Copyright (C) 1996, 1997 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1997, 1998, 1999 Jakub Jelinek (jj@ultra.linux.cz)
 */

#include <asm/asi.h>
#include <asm/pstate.h>
#include <asm/ptrace.h>
#include <asm/page.h>
#include <asm/spitfire.h>
#include <asm/head.h>
#include <asm/processor.h>

#define		TASK_REGOFF		(THREAD_SIZE-TRACEREG_SZ-STACKFRAME_SZ)
#define		ETRAP_PSTATE1		(PSTATE_RMO | PSTATE_PRIV)
#define		ETRAP_PSTATE2		(PSTATE_RMO | PSTATE_PEF | PSTATE_PRIV | PSTATE_IE)

/*
 * On entry, %g7 is return address - 0x4.
 * %g4 and %g5 will be preserved %l4 and %l5 respectively.
 */

		.text		
		.align	64
		.globl	etrap, etrap_irq, etraptl1

etrap:		rdpr	%pil, %g2						! Single 	Group
etrap_irq:	rdpr	%tstate, %g1						! Single 	Group
		sllx	%g2, 20, %g3						! IEU0		Group
		andcc	%g1, TSTATE_PRIV, %g0					! IEU1
		or	%g1, %g3, %g1						! IEU0		Group
		bne,pn	%xcc, 1f							! CTI
		 sub	%sp, STACKFRAME_SZ+TRACEREG_SZ-STACK_BIAS, %g2		! IEU1
		wrpr	%g0, 7, %cleanwin					! Single	Group+4bubbles

		sethi	%hi(TASK_REGOFF), %g2					! IEU0		Group		
		sethi	%hi(TSTATE_PEF), %g3					! IEU1
		or	%g2, %lo(TASK_REGOFF), %g2				! IEU0		Group
		and	%g1, %g3, %g3						! IEU1
		brnz,pn	%g3, 1f							! CTI+IEU1	Group
		 add	%g6, %g2, %g2						! IEU0
		wr	%g0, 0, %fprs						! Single	Group+4bubbles
1:		rdpr	%tpc, %g3						! Single	Group

		stx	%g1, [%g2 + STACKFRAME_SZ + PT_V9_TSTATE]		! Store		Group
		rdpr	%tnpc, %g1						! Single	Group
		stx	%g3, [%g2 + STACKFRAME_SZ + PT_V9_TPC]			! Store		Group
		rd	%y, %g3							! Single	Group+4bubbles
		stx	%g1, [%g2 + STACKFRAME_SZ + PT_V9_TNPC]			! Store		Group
		st	%g3, [%g2 + STACKFRAME_SZ + PT_V9_Y]			! Store		Group
		save	%g2, -STACK_BIAS, %sp	! Ordering here is critical	! Single	Group
		mov	%g6, %l6						! IEU0		Group

		bne,pn	%xcc, 3f						! CTI		
		 mov	PRIMARY_CONTEXT, %l4					! IEU1
		rdpr	%canrestore, %g3					! Single	Group+4bubbles
		rdpr	%wstate, %g2						! Single	Group+4bubbles
		wrpr	%g0, 0, %canrestore					! Single	Group+4bubbles
		sll	%g2, 3, %g2						! IEU0		Group
		mov	1, %l5							! IEU1
		stb	%l5, [%l6 + AOFF_task_thread + AOFF_thread_fpdepth]	! Store

		wrpr	%g3, 0, %otherwin					! Single	Group+4bubbles
		wrpr	%g2, 0, %wstate						! Single	Group+4bubbles
		stxa	%g0, [%l4] ASI_DMMU					! Store		Group
		flush	%l6							! Single	Group+9bubbles
		wr	%g0, ASI_AIUS, %asi					! Single	Group+4bubbles
2:		wrpr	%g0, 0x0, %tl						! Single	Group+4bubbles
		mov	%g4, %l4						! IEU1
		mov	%g5, %l5						! IEU0		Group

		mov	%g7, %l2						! IEU1
		wrpr	%g0, ETRAP_PSTATE1, %pstate				! Single	Group+4bubbles
		stx	%g1, [%sp + PTREGS_OFF + PT_V9_G1]		! Store		Group
		stx	%g2, [%sp + PTREGS_OFF + PT_V9_G2]		! Store		Group
		stx	%g3, [%sp + PTREGS_OFF + PT_V9_G3]		! Store		Group
		stx	%g4, [%sp + PTREGS_OFF + PT_V9_G4]		! Store		Group
		stx	%g5, [%sp + PTREGS_OFF + PT_V9_G5]		! Store		Group
		stx	%g6, [%sp + PTREGS_OFF + PT_V9_G6]		! Store		Group

		stx	%g7, [%sp + PTREGS_OFF + PT_V9_G7]		! Store		Group
		stx	%i0, [%sp + PTREGS_OFF + PT_V9_I0]		! Store		Group
		stx	%i1, [%sp + PTREGS_OFF + PT_V9_I1]		! Store		Group
		stx	%i2, [%sp + PTREGS_OFF + PT_V9_I2]		! Store		Group
		stx	%i3, [%sp + PTREGS_OFF + PT_V9_I3]		! Store		Group
		stx	%i4, [%sp + PTREGS_OFF + PT_V9_I4]		! Store		Group
		sethi	%uhi(PAGE_OFFSET), %g4					! IEU0
		stx	%i5, [%sp + PTREGS_OFF + PT_V9_I5]		! Store		Group

		stx	%i6, [%sp + PTREGS_OFF + PT_V9_I6]		! Store		Group
		sllx	%g4, 32, %g4						! IEU0
		stx	%i7, [%sp + PTREGS_OFF + PT_V9_I7]		! Store		Group
		wrpr	%g0, ETRAP_PSTATE2, %pstate				! Single	Group+4bubbles
		jmpl	%l2 + 0x4, %g0						! CTI		Group
		 mov	%l6, %g6						! IEU0
		nop
		nop

3:		ldub	[%l6 + AOFF_task_thread + AOFF_thread_fpdepth], %l5	! Load		Group
		add	%l6, AOFF_task_thread + AOFF_thread_fpsaved + 1, %l4	! IEU0
		srl	%l5, 1, %l3						! IEU0		Group
		add	%l5, 2, %l5						! IEU1
		stb	%l5, [%l6 + AOFF_task_thread + AOFF_thread_fpdepth]	! Store
		ba,pt	%xcc, 2b						! CTI
		 stb	%g0, [%l4 + %l3]					! Store		Group
		nop

etraptl1:	/* Save tstate/tpc/tnpc of TL 1-->4 and the tl register itself.
		 * We place this right after pt_regs on the trap stack.  The layout
		 * is:
		 *	0x00	TL1's TSTATE
		 *	0x08	TL1's TPC
		 *	0x10	TL1's TNPC
		 *	0x18	TL1's TT
		 *	 ...
		 *	0x58	TL4's TT
		 *	0x60	TL
		 */
		sub	%sp, ((4 * 8) * 4) + 8, %g2
		rdpr	%tl, %g1

		wrpr	%g0, 1, %tl
		rdpr	%tstate, %g3
		stx	%g3, [%g2 + STACK_BIAS + 0x00]
		rdpr	%tpc, %g3
		stx	%g3, [%g2 + STACK_BIAS + 0x08]
		rdpr	%tnpc, %g3
		stx	%g3, [%g2 + STACK_BIAS + 0x10]
		rdpr	%tt, %g3
		stx	%g3, [%g2 + STACK_BIAS + 0x18]

		wrpr	%g0, 2, %tl
		rdpr	%tstate, %g3
		stx	%g3, [%g2 + STACK_BIAS + 0x20]
		rdpr	%tpc, %g3
		stx	%g3, [%g2 + STACK_BIAS + 0x28]
		rdpr	%tnpc, %g3
		stx	%g3, [%g2 + STACK_BIAS + 0x30]
		rdpr	%tt, %g3
		stx	%g3, [%g2 + STACK_BIAS + 0x38]

		wrpr	%g0, 3, %tl
		rdpr	%tstate, %g3
		stx	%g3, [%g2 + STACK_BIAS + 0x40]
		rdpr	%tpc, %g3
		stx	%g3, [%g2 + STACK_BIAS + 0x48]
		rdpr	%tnpc, %g3
		stx	%g3, [%g2 + STACK_BIAS + 0x50]
		rdpr	%tt, %g3
		stx	%g3, [%g2 + STACK_BIAS + 0x58]

		wrpr	%g0, 4, %tl
		rdpr	%tstate, %g3
		stx	%g3, [%g2 + STACK_BIAS + 0x60]
		rdpr	%tpc, %g3
		stx	%g3, [%g2 + STACK_BIAS + 0x68]
		rdpr	%tnpc, %g3
		stx	%g3, [%g2 + STACK_BIAS + 0x70]
		rdpr	%tt, %g3
		stx	%g3, [%g2 + STACK_BIAS + 0x78]

		wrpr	%g1, %tl
		stx	%g1, [%g2 + STACK_BIAS + 0x80]

		rdpr	%tstate, %g1						! Single	Group+4bubbles
		sub	%g2, STACKFRAME_SZ + TRACEREG_SZ - STACK_BIAS, %g2	! IEU1
		ba,pt	%xcc, 1b						! CTI		Group
		 andcc	%g1, TSTATE_PRIV, %g0					! IEU0

		.align	64
		.globl	scetrap
scetrap:	rdpr	%pil, %g2						! Single 	Group
		rdpr	%tstate, %g1						! Single 	Group
		sllx	%g2, 20, %g3						! IEU0		Group
		andcc	%g1, TSTATE_PRIV, %g0					! IEU1
		or	%g1, %g3, %g1						! IEU0		Group
		bne,pn	%xcc, 1f						! CTI
		 sub	%sp, (STACKFRAME_SZ+TRACEREG_SZ-STACK_BIAS), %g2	! IEU1
		wrpr	%g0, 7, %cleanwin					! Single	Group+4bubbles

		sllx	%g1, 51, %g3						! IEU0		Group
		sethi	%hi(TASK_REGOFF), %g2					! IEU1
		or	%g2, %lo(TASK_REGOFF), %g2				! IEU0		Group
		brlz,pn	%g3, 1f							! CTI+IEU1
		 add	%g6, %g2, %g2						! IEU0		Group
		wr	%g0, 0, %fprs						! Single	Group+4bubbles
1:		rdpr	%tpc, %g3						! Single	Group
		stx	%g1, [%g2 + STACKFRAME_SZ + PT_V9_TSTATE]		! Store		Group

		rdpr	%tnpc, %g1						! Single	Group
		stx	%g3, [%g2 + STACKFRAME_SZ + PT_V9_TPC]			! Store		Group
		stx	%g1, [%g2 + STACKFRAME_SZ + PT_V9_TNPC]			! Store		Group
		save	%g2, -STACK_BIAS, %sp	! Ordering here is critical	! Single	Group
		mov	%g6, %l6						! IEU0		Group
		bne,pn	%xcc, 2f						! CTI		
		 mov	ASI_P, %l7						! IEU1
		rdpr	%canrestore, %g3					! Single	Group+4bubbles

		rdpr	%wstate, %g2						! Single	Group+4bubbles
		wrpr	%g0, 0, %canrestore					! Single	Group+4bubbles
		sll	%g2, 3, %g2						! IEU0		Group
		mov	PRIMARY_CONTEXT, %l4					! IEU1
		wrpr	%g3, 0, %otherwin					! Single	Group+4bubbles
		wrpr	%g2, 0, %wstate						! Single	Group+4bubbles
		stxa	%g0, [%l4] ASI_DMMU					! Store
		flush	%l6							! Single	Group+9bubbles

		mov	ASI_AIUS, %l7						! IEU0		Group
2:		mov	%g4, %l4						! IEU1
		mov	%g5, %l5						! IEU0		Group
		add	%g7, 0x4, %l2						! IEU1
		wrpr	%g0, ETRAP_PSTATE1, %pstate				! Single	Group+4bubbles
		stx	%g1, [%sp + PTREGS_OFF + PT_V9_G1]		! Store		Group
		stx	%g2, [%sp + PTREGS_OFF + PT_V9_G2]		! Store		Group
		sllx	%l7, 24, %l7						! IEU0

		stx	%g3, [%sp + PTREGS_OFF + PT_V9_G3]		! Store		Group
		rdpr	%cwp, %l0						! Single	Group
		stx	%g4, [%sp + PTREGS_OFF + PT_V9_G4]		! Store		Group
		stx	%g5, [%sp + PTREGS_OFF + PT_V9_G5]		! Store		Group
		stx	%g6, [%sp + PTREGS_OFF + PT_V9_G6]		! Store		Group
		stx	%g7, [%sp + PTREGS_OFF + PT_V9_G7]		! Store		Group
		or	%l7, %l0, %l7						! IEU0
		sethi	%hi(TSTATE_RMO | TSTATE_PEF), %l0			! IEU1

		or	%l7, %l0, %l7						! IEU0		Group
		wrpr	%l2, %tnpc						! Single	Group+4bubbles
		wrpr	%l7, (TSTATE_PRIV | TSTATE_IE), %tstate			! Single	Group+4bubbles
		stx	%i0, [%sp + PTREGS_OFF + PT_V9_I0]		! Store		Group
		stx	%i1, [%sp + PTREGS_OFF + PT_V9_I1]		! Store		Group
		stx	%i2, [%sp + PTREGS_OFF + PT_V9_I2]		! Store		Group
		stx	%i3, [%sp + PTREGS_OFF + PT_V9_I3]		! Store		Group
		stx	%i4, [%sp + PTREGS_OFF + PT_V9_I4]		! Store		Group

		sethi	%uhi(PAGE_OFFSET), %g4					! IEU0
		stx	%i5, [%sp + PTREGS_OFF + PT_V9_I5]		! Store		Group
		stx	%i6, [%sp + PTREGS_OFF + PT_V9_I6]		! Store		Group
		sllx	%g4, 32, %g4						! IEU0
		mov	%l6, %g6						! IEU1
		stx	%i7, [%sp + PTREGS_OFF + PT_V9_I7]		! Store		Group
		done
		nop

#undef TASK_REGOFF
#undef ETRAP_PSTATE1
#undef ETRAP_PSTATE2
