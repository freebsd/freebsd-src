/* $Id: swtch.s,v 1.1 1998/06/10 10:53:23 dfr Exp $ */
/* $NetBSD: locore.s,v 1.47 1998/03/22 07:26:32 thorpej Exp $ */

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <machine/asm.h>
#include "assym.s"

/**************************************************************************/

/*
 * Perform actions necessary to switch to a new context.  The
 * hwpcb should be in a0.
 */
#define	SWITCH_CONTEXT							\
	/* Make a note of the context we're running on. */		\
	stq	a0, curpcb;						\
									\
	/* Swap in the new context. */					\
	call_pal PAL_OSF1_swpctx
	
/*
 * savectx: save process context, i.e. callee-saved registers
 *
 * Note that savectx() only works for processes other than curproc,
 * since cpu_switch will copy over the info saved here.  (It _can_
 * sanely be used for curproc iff cpu_switch won't be called again, e.g.
 * from if called from boot().)
 *
 * Arguments:
 *	a0	'struct user *' of the process that needs its context saved
 *
 * Return:
 *	v0	0.  (note that for child processes, it seems
 *		like savectx() returns 1, because the return address
 *		in the PCB is set to the return address from savectx().)
 */

LEAF(savectx, 1)
	br	pv, Lsavectx1
Lsavectx1: LDGP(pv)
	stq	sp, U_PCB_HWPCB_KSP(a0)		/* store sp */
	stq	s0, U_PCB_CONTEXT+(0 * 8)(a0)	/* store s0 - s6 */
	stq	s1, U_PCB_CONTEXT+(1 * 8)(a0)
	stq	s2, U_PCB_CONTEXT+(2 * 8)(a0)
	stq	s3, U_PCB_CONTEXT+(3 * 8)(a0)
	stq	s4, U_PCB_CONTEXT+(4 * 8)(a0)
	stq	s5, U_PCB_CONTEXT+(5 * 8)(a0)
	stq	s6, U_PCB_CONTEXT+(6 * 8)(a0)
	stq	ra, U_PCB_CONTEXT+(7 * 8)(a0)	/* store ra */
	call_pal PAL_OSF1_rdps			/* NOTE: doesn't kill a0 */
	stq	v0, U_PCB_CONTEXT+(8 * 8)(a0)	/* store ps, for ipl */

	mov	zero, v0
	RET
	END(savectx)

/**************************************************************************/

IMPORT(whichqs, 4)
IMPORT(want_resched, 8)
IMPORT(Lev1map, 8)

/*
 * When no processes are on the runq, cpu_switch branches to idle
 * to wait for something to come ready.
 * Note: this is really a part of cpu_switch() but defined here for kernel
 * profiling.
 */
LEAF(idle, 0)
	br	pv, Lidle1
Lidle1:	LDGP(pv)
	stq	zero, curproc			/* curproc <- NULL for stats */
	mov	zero, a0			/* enable all interrupts */
	call_pal PAL_OSF1_swpipl
Lidle2:
	ldl	t0, whichqs			/* look for non-empty queue */
	beq	t0, Lidle2
	ldiq	a0, ALPHA_PSL_IPL_HIGH		/* disable all interrupts */
	call_pal PAL_OSF1_swpipl
	jmp	zero, sw1			/* jump back into the fray */
	END(idle)

/*
 * cpu_switch()
 * Find the highest priority process and resume it.
 */
LEAF(cpu_switch, 1)
	LDGP(pv)
	/* do an inline savectx(), to save old context */
	ldq	a1, P_ADDR(a0)
	/* NOTE: ksp is stored by the swpctx */
	stq	s0, U_PCB_CONTEXT+(0 * 8)(a1)	/* store s0 - s6 */
	stq	s1, U_PCB_CONTEXT+(1 * 8)(a1)
	stq	s2, U_PCB_CONTEXT+(2 * 8)(a1)
	stq	s3, U_PCB_CONTEXT+(3 * 8)(a1)
	stq	s4, U_PCB_CONTEXT+(4 * 8)(a1)
	stq	s5, U_PCB_CONTEXT+(5 * 8)(a1)
	stq	s6, U_PCB_CONTEXT+(6 * 8)(a1)
	stq	ra, U_PCB_CONTEXT+(7 * 8)(a1)	/* store ra */
	call_pal PAL_OSF1_rdps			/* NOTE: doesn't kill a0 */
	stq	v0, U_PCB_CONTEXT+(8 * 8)(a1)	/* store ps, for ipl */

	mov	a0, s0				/* save old curproc */
	mov	a1, s1				/* save old U-area */

	ldl	t0, whichqs			/* look for non-empty queue */
	beq	t0, idle			/* and if none, go idle */

	ldiq	a0, ALPHA_PSL_IPL_HIGH		/* disable all interrupts */
	call_pal PAL_OSF1_swpipl
sw1:
	br	pv, Lcs1
Lcs1:	LDGP(pv)
	ldl	t0, whichqs			/* look for non-empty queue */
	beq	t0, idle			/* and if none, go idle */
	mov	t0, t3				/* t3 = saved whichqs */
	mov	zero, t2			/* t2 = lowest bit set */
	blbs	t0, Lcs3			/* if low bit set, done! */

Lcs2:	srl	t0, 1, t0			/* try next bit */
	addq	t2, 1, t2
	blbc	t0, Lcs2			/* if clear, try again */

Lcs3:
	/*
	 * Remove process from queue
	 */
	lda	t1, qs				/* get queues */
	sll	t2, 4, t0			/* queue head is 16 bytes */
	addq	t1, t0, t0			/* t0 = qp = &qs[firstbit] */

	ldq	t4, PH_LINK(t0)			/* t4 = p = highest pri proc */
	ldq	t5, P_FORW(t4)			/* t5 = p->p_forw */
	bne	t4, Lcs4			/* make sure p != NULL */
	PANIC("cpu_switch",Lcpu_switch_pmsg)	/* nothing in queue! */

Lcs4:
	stq	t5, PH_LINK(t0)			/* qp->ph_link = p->p_forw */
	stq	t0, P_BACK(t5)			/* p->p_forw->p_back = qp */
	stq	zero, P_BACK(t4)		/* firewall: p->p_back = NULL */
	cmpeq	t0, t5, t0			/* see if queue is empty */
	beq	t0, Lcs5			/* nope, it's not! */

	ldiq	t0, 1				/* compute bit in whichqs */
	sll	t0, t2, t0
	xor	t3, t0, t3			/* clear bit in whichqs */
	stl	t3, whichqs

Lcs5:
	mov	t4, s2				/* save new proc */
	ldq	s3, P_MD_PCBPADDR(s2)		/* save new pcbpaddr */

	/*
	 * Check to see if we're switching to ourself.  If we are,
	 * don't bother loading the new context.
	 *
	 * Note that even if we re-enter cpu_switch() from idle(),
	 * s0 will still contain the old curproc value because any
	 * users of that register between then and now must have
	 * saved it.  Also note that switch_exit() ensures that
	 * s0 is clear before jumping here to find a new process.
	 */
	cmpeq	s0, t4, t0			/* oldproc == newproc? */
	bne	t0, Lcs7			/* Yes!  Skip! */

	/*
	 * Deactivate the old address space before activating the
	 * new one.  We need to do this before activating the
	 * new process's address space in the event that new
	 * process is using the same vmspace as the old.  If we
	 * do this after we activate, then we might end up
	 * incorrectly marking the pmap inactive!
	 *
	 * We don't deactivate if we came here from switch_exit
	 * (old pmap no longer exists; vmspace has been freed).
	 * oldproc will be NULL in this case.  We have actually
	 * taken care of calling pmap_deactivate() in cpu_exit(),
	 * before the vmspace went away.
	 */
	beq	s0, Lcs6

	mov	s0, a0				/* pmap_deactivate(oldproc) */
	CALL(pmap_deactivate)

Lcs6:
	/*
	 * Activate the new process's address space and perform
	 * the actual context swap.
	 */

	mov	s2, a0				/* pmap_activate(p) */
	CALL(pmap_activate)

	mov	s3, a0				/* swap the context */
	SWITCH_CONTEXT

Lcs7:
	
	/*
	 * Now that the switch is done, update curproc and other
	 * globals.  We must do this even if switching to ourselves
	 * because we might have re-entered cpu_switch() from idle(),
	 * in which case curproc would be NULL.
	 */
	stq	s2, curproc			/* curproc = p */
	stq	zero, want_resched		/* we've rescheduled */

	/*
	 * Now running on the new u struct.
	 * Restore registers and return.
	 */
	ldq	t0, P_ADDR(s2)

	/* NOTE: ksp is restored by the swpctx */
	ldq	s0, U_PCB_CONTEXT+(0 * 8)(t0)		/* restore s0 - s6 */
	ldq	s1, U_PCB_CONTEXT+(1 * 8)(t0)
	ldq	s2, U_PCB_CONTEXT+(2 * 8)(t0)
	ldq	s3, U_PCB_CONTEXT+(3 * 8)(t0)
	ldq	s4, U_PCB_CONTEXT+(4 * 8)(t0)
	ldq	s5, U_PCB_CONTEXT+(5 * 8)(t0)
	ldq	s6, U_PCB_CONTEXT+(6 * 8)(t0)
	ldq	ra, U_PCB_CONTEXT+(7 * 8)(t0)		/* restore ra */
	ldq	a0, U_PCB_CONTEXT+(8 * 8)(t0)		/* restore ipl */
	and	a0, ALPHA_PSL_IPL_MASK, a0
	call_pal PAL_OSF1_swpipl

	ldiq	v0, 1				/* possible ret to savectx() */
	RET
	END(cpu_switch)


/*
 * switch_trampoline()
 *
 * Arrange for a function to be invoked neatly, after a cpu_switch().
 *
 * Invokes the function specified by the s0 register with the return
 * address specified by the s1 register and with one argument, a
 * pointer to the executing process's proc structure.
 */
LEAF(switch_trampoline, 0)
	mov	s0, pv
	mov	s1, ra
	mov	s2, a0
	jmp	zero, (pv)
	END(switch_trampoline)

	
/**************************************************************************/

/*
 * exception_return: return from trap, exception, or syscall
 */

IMPORT(ipending, 8)
IMPORT(astpending, 8)

LEAF(exception_return, 1)			/* XXX should be NESTED */
	br	pv, Ler1
Ler1:	LDGP(pv)

	ldq	s1, (FRAME_PS * 8)(sp)		/* get the saved PS */
	and	s1, ALPHA_PSL_IPL_MASK, t0	/* look at the saved IPL */
	bne	t0, Lrestoreregs		/* != 0: can't do AST or SIR */

	/* see if we can do an SIR */
	ldq	t1, ipending			/* SIR pending? */
	beq	t1, Lchkast			/* no, try an AST*/

	/* We've got a SIR. */
	CALL(do_sir)				/* do the SIR; lowers IPL */

Lchkast:
	ldiq	a0, ALPHA_PSL_IPL_0		/* drop IPL to zero*/
	call_pal PAL_OSF1_swpipl

	and	s1, ALPHA_PSL_USERMODE, t0	/* are we returning to user? */
	beq	t0, Lrestoreregs		/* no: just return */

	ldq	t2, astpending			/* AST pending? */
	beq	t2, Lsetfpenable		/* no: return & deal with FP */

	/* We've got an AST.  Handle it. */
	mov	sp, a0				/* only arg is frame */
	CALL(ast)

Lsetfpenable:
	/* enable FPU based on whether the current proc is fpcurproc */
	ldq	t0, curproc
	ldq	t1, fpcurproc
	cmpeq	t0, t1, t0
	mov	zero, a0
	cmovne	t0, 1, a0
	call_pal PAL_OSF1_wrfen

Lrestoreregs:
	/* restore the registers, and return */
	bsr	ra, exception_restore_regs	/* jmp/CALL trashes pv/t12 */
	ldq	ra,(FRAME_RA*8)(sp)
	.set noat
	ldq	at_reg,(FRAME_AT*8)(sp)

	lda	sp,(FRAME_SW_SIZE*8)(sp)
	call_pal PAL_OSF1_rti
	.set at
	END(exception_return)

LEAF(exception_save_regs, 0)
	stq	v0,(FRAME_V0*8)(sp)
	stq	a3,(FRAME_A3*8)(sp)
	stq	a4,(FRAME_A4*8)(sp)
	stq	a5,(FRAME_A5*8)(sp)
	stq	s0,(FRAME_S0*8)(sp)
	stq	s1,(FRAME_S1*8)(sp)
	stq	s2,(FRAME_S2*8)(sp)
	stq	s3,(FRAME_S3*8)(sp)
	stq	s4,(FRAME_S4*8)(sp)
	stq	s5,(FRAME_S5*8)(sp)
	stq	s6,(FRAME_S6*8)(sp)
	stq	t0,(FRAME_T0*8)(sp)
	stq	t1,(FRAME_T1*8)(sp)
	stq	t2,(FRAME_T2*8)(sp)
	stq	t3,(FRAME_T3*8)(sp)
	stq	t4,(FRAME_T4*8)(sp)
	stq	t5,(FRAME_T5*8)(sp)
	stq	t6,(FRAME_T6*8)(sp)
	stq	t7,(FRAME_T7*8)(sp)
	stq	t8,(FRAME_T8*8)(sp)
	stq	t9,(FRAME_T9*8)(sp)
	stq	t10,(FRAME_T10*8)(sp)
	stq	t11,(FRAME_T11*8)(sp)
	stq	t12,(FRAME_T12*8)(sp)
	RET
	END(exception_save_regs)

LEAF(exception_restore_regs, 0)
	ldq	v0,(FRAME_V0*8)(sp)
	ldq	a3,(FRAME_A3*8)(sp)
	ldq	a4,(FRAME_A4*8)(sp)
	ldq	a5,(FRAME_A5*8)(sp)
	ldq	s0,(FRAME_S0*8)(sp)
	ldq	s1,(FRAME_S1*8)(sp)
	ldq	s2,(FRAME_S2*8)(sp)
	ldq	s3,(FRAME_S3*8)(sp)
	ldq	s4,(FRAME_S4*8)(sp)
	ldq	s5,(FRAME_S5*8)(sp)
	ldq	s6,(FRAME_S6*8)(sp)
	ldq	t0,(FRAME_T0*8)(sp)
	ldq	t1,(FRAME_T1*8)(sp)
	ldq	t2,(FRAME_T2*8)(sp)
	ldq	t3,(FRAME_T3*8)(sp)
	ldq	t4,(FRAME_T4*8)(sp)
	ldq	t5,(FRAME_T5*8)(sp)
	ldq	t6,(FRAME_T6*8)(sp)
	ldq	t7,(FRAME_T7*8)(sp)
	ldq	t8,(FRAME_T8*8)(sp)
	ldq	t9,(FRAME_T9*8)(sp)
	ldq	t10,(FRAME_T10*8)(sp)
	ldq	t11,(FRAME_T11*8)(sp)
	ldq	t12,(FRAME_T12*8)(sp)
	RET
	END(exception_restore_regs)
