/* $Id: VISsave.S,v 1.5 2001/03/08 22:08:51 davem Exp $
 * VISsave.S: Code for saving FPU register state for
 *            VIS routines. One should not call this directly,
 *            but use macros provided in <asm/visasm.h>.
 *
 * Copyright (C) 1998 Jakub Jelinek (jj@ultra.linux.cz)
 */

#include <asm/asi.h>
#include <asm/page.h>
#include <asm/ptrace.h>
#include <asm/visasm.h>

	.text
	.globl		VISenter, VISenterhalf

	/* On entry: %o5=current FPRS value, %g7 is callers address */
	/* May clobber %o5, %g1, %g2, %g3, %g7, %icc, %xcc */

	.align		32
VISenter:
	ldub		[%g6 + AOFF_task_thread + AOFF_thread_fpdepth], %g1
	brnz,a,pn	%g1, 1f
	 cmp		%g1, 1
	stb		%g0, [%g6 + AOFF_task_thread + AOFF_thread_fpsaved]
	stx		%fsr, [%g6 + AOFF_task_thread + AOFF_thread_xfsr]
9:	jmpl		%g7 + %g0, %g0
	 nop
1:	bne,pn		%icc, 2f

	 srl		%g1, 1, %g1
vis1:	ldub		[%g6 + AOFF_task_thread + AOFF_thread_fpsaved], %g3
	stx		%fsr, [%g6 + AOFF_task_thread + AOFF_thread_xfsr]
	or		%g3, %o5, %g3
	stb		%g3, [%g6 + AOFF_task_thread + AOFF_thread_fpsaved]
	rd		%gsr, %g3
	clr		%g1
	ba,pt		%xcc, 3f

	 stx		%g3, [%g6 + AOFF_task_thread + AOFF_thread_gsr]
2:	add		%g6, %g1, %g3
	cmp		%o5, FPRS_DU
	be,pn		%icc, 6f
	 sll		%g1, 3, %g1
	stb		%o5, [%g3 + AOFF_task_thread + AOFF_thread_fpsaved]
	rd		%gsr, %g2
	add		%g6, %g1, %g3
	stx		%g2, [%g3 + AOFF_task_thread + AOFF_thread_gsr]

	add		%g6, %g1, %g2
	stx		%fsr, [%g2 + AOFF_task_thread + AOFF_thread_xfsr]
	sll		%g1, 5, %g1
3:	andcc		%o5, FPRS_DL|FPRS_DU, %g0
	be,pn		%icc, 9b
	 add		%g6, AOFF_task_fpregs, %g2
	andcc		%o5, FPRS_DL, %g0
	membar		#StoreStore | #LoadStore

	be,pn		%icc, 4f
	 add		%g6, AOFF_task_fpregs+0x40, %g3
	stda		%f0, [%g2 + %g1] ASI_BLK_P
	stda		%f16, [%g3 + %g1] ASI_BLK_P
	andcc		%o5, FPRS_DU, %g0
	be,pn		%icc, 5f
4:	 add		%g1, 128, %g1
	stda		%f32, [%g2 + %g1] ASI_BLK_P

	stda		%f48, [%g3 + %g1] ASI_BLK_P
5:	membar		#Sync
	jmpl		%g7 + %g0, %g0
	 nop

6:	ldub		[%g3 + AOFF_task_thread + AOFF_thread_fpsaved], %o5
	or		%o5, FPRS_DU, %o5
	add		%g6, AOFF_task_fpregs+0x80, %g2
	stb		%o5, [%g3 + AOFF_task_thread + AOFF_thread_fpsaved]

	sll		%g1, 5, %g1
	add		%g6, AOFF_task_fpregs+0xc0, %g3
	wr		%g0, FPRS_FEF, %fprs
	membar		#StoreStore | #LoadStore
	stda		%f32, [%g2 + %g1] ASI_BLK_P
	stda		%f48, [%g3 + %g1] ASI_BLK_P
	membar		#Sync
	jmpl		%g7 + %g0, %g0

	 nop

	.align		32
VISenterhalf:
	ldub		[%g6 + AOFF_task_thread + AOFF_thread_fpdepth], %g1
	brnz,a,pn	%g1, 1f
	 cmp		%g1, 1
	stb		%g0, [%g6 + AOFF_task_thread + AOFF_thread_fpsaved]
	stx		%fsr, [%g6 + AOFF_task_thread + AOFF_thread_xfsr]
	clr		%o5
	jmpl		%g7 + %g0, %g0
	 wr		%g0, FPRS_FEF, %fprs

1:	bne,pn		%icc, 2f
	 srl		%g1, 1, %g1
	ba,pt		%xcc, vis1
	 sub		%g7, 8, %g7
2:	addcc		%g6, %g1, %g3
	sll		%g1, 3, %g1
	andn		%o5, FPRS_DU, %g2
	stb		%g2, [%g3 + AOFF_task_thread + AOFF_thread_fpsaved]

	rd		%gsr, %g2
	add		%g6, %g1, %g3
	stx		%g2, [%g3 + AOFF_task_thread + AOFF_thread_gsr]
	add		%g6, %g1, %g2
	stx		%fsr, [%g2 + AOFF_task_thread + AOFF_thread_xfsr]
	sll		%g1, 5, %g1
3:	andcc		%o5, FPRS_DL, %g0
	be,pn		%icc, 4f
	 add		%g6, AOFF_task_fpregs, %g2

	membar		#StoreStore | #LoadStore
	add		%g6, AOFF_task_fpregs+0x40, %g3
	stda		%f0, [%g2 + %g1] ASI_BLK_P
	stda		%f16, [%g3 + %g1] ASI_BLK_P
	membar		#Sync
4:	and		%o5, FPRS_DU, %o5
	jmpl		%g7 + %g0, %g0
	 wr		%o5, FPRS_FEF, %fprs
