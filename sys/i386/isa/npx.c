/*-
 * Copyright (c) 1990 William Jolitz.
 * Copyright (c) 1991 The Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)npx.c	7.2 (Berkeley) 5/12/91
 *	$Id: npx.c,v 1.6 1994/01/03 07:55:43 davidg Exp $
 */

#include "npx.h"
#if NNPX > 0

#include "param.h"
#include "systm.h"
#include "conf.h"
#include "file.h"
#include "proc.h"
#include "machine/cpu.h"
#include "machine/pcb.h"
#include "machine/trap.h"
#include "ioctl.h"
#include "machine/specialreg.h"
#include "i386/isa/icu.h"
#include "i386/isa/isa_device.h"
#include "i386/isa/isa.h"

/*
 * 387 and 287 Numeric Coprocessor Extension (NPX) Driver.
 */

#ifdef	__GNUC__

#define	disable_intr()		__asm("cli")
#define	enable_intr()		__asm("sti")
#define	fldcw(addr)		__asm("fldcw %0" : : "m" (*addr))
#define	fnclex()		__asm("fnclex")
#define	fninit()		__asm("fninit")
#define	fnsave(addr)		__asm("fnsave %0" : "=m" (*addr) : "0" (*addr))
#define	fnstcw(addr)		__asm("fnstcw %0" : "=m" (*addr) : "0" (*addr))
#define	fnstsw(addr)		__asm("fnstsw %0" : "=m" (*addr) : "0" (*addr))
#define	fp_divide_by_0()	__asm("fldz; fld1; fdiv %st,%st(1); fwait")
#define	frstor(addr)		__asm("frstor %0" : : "m" (*addr))
#define	fwait()			__asm("fwait")
#define	read_eflags()		({u_long ef; \
				  __asm("pushf; popl %0" : "=a" (ef)); \
				  ef; })
#define	start_emulating()	__asm("smsw %%ax; orb %0,%%al; lmsw %%ax" \
				      : : "n" (CR0_TS) : "ax")
#define	stop_emulating()	__asm("clts")
#define	write_eflags(ef)	__asm("pushl %0; popf" : : "a" ((u_long) ef))

#else	/* not __GNUC__ */

void	disable_intr	__P((void));
void	enable_intr	__P((void));
void	fldcw		__P((caddr_t addr));
void	fnclex		__P((void));
void	fninit		__P((void));
void	fnsave		__P((caddr_t addr));
void	fnstcw		__P((caddr_t addr));
void	fnstsw		__P((caddr_t addr));
void	fp_divide_by_0	__P((void));
void	frstor		__P((caddr_t addr));
void	fwait		__P((void));
u_long	read_eflags	__P((void));
void	start_emulating	__P((void));
void	stop_emulating	__P((void));
void	write_eflags	__P((u_long ef));

#endif	/* __GNUC__ */

typedef u_char bool_t;

extern	struct gate_descriptor idt[];

int	npxdna		__P((void));
void	npxexit		__P((struct proc *p));
void	npxinit		__P((u_int control));
void	npxintr		__P((struct intrframe frame));
void	npxsave		__P((struct save87 *addr));
static	int	npxattach	__P((struct isa_device *dvp));
static	int	npxprobe	__P((struct isa_device *dvp));
static	int	npxprobe1	__P((struct isa_device *dvp));

struct	isa_driver npxdriver = {
	npxprobe, npxattach, "npx",
};

u_int	npx0mask;
struct proc	*npxproc;

static	bool_t			npx_ex16;
static	bool_t			npx_exists;
static	struct gate_descriptor	npx_idt_probeintr;
static	int			npx_intrno;
static	volatile u_int		npx_intrs_while_probing;
static	bool_t			npx_irq13;
static	volatile u_int		npx_traps_while_probing;

/*
 * Special interrupt handlers.  Someday intr0-intr15 will be used to count
 * interrupts.  We'll still need a special exception 16 handler.  The busy
 * latch stuff in probintr() can be moved to npxprobe().
 */
void probeintr(void);
asm
("
	.text
_probeintr:
	ss
	incl	_npx_intrs_while_probing
	pushl	%eax
	movb	$0x20,%al	# EOI (asm in strings loses cpp features)
	outb	%al,$0xa0	# IO_ICU2
	outb	%al,$0x20	#IO_ICU1
	movb	$0,%al
	outb	%al,$0xf0	# clear BUSY# latch
	popl	%eax
	iret
");

void probetrap(void);
asm
("
	.text
_probetrap:
	ss
	incl	_npx_traps_while_probing
	fnclex
	iret
");

/*
 * Probe routine.  Initialize cr0 to give correct behaviour for [f]wait
 * whether the device exists or not (XXX should be elsewhere).  Set flags
 * to tell npxattach() what to do.  Modify device struct if npx doesn't
 * need to use interrupts.  Return 1 if device exists.
 */
static int
npxprobe(dvp)
	struct isa_device *dvp;
{
	int	result;
	u_long	save_eflags;
	u_char	save_icu1_mask;
	u_char	save_icu2_mask;
	struct	gate_descriptor save_idt_npxintr;
	struct	gate_descriptor save_idt_npxtrap;
	/*
	 * This routine is now just a wrapper for npxprobe1(), to install
	 * special npx interrupt and trap handlers, to enable npx interrupts
	 * and to disable other interrupts.  Someday isa_configure() will
	 * install suitable handlers and run with interrupts enabled so we
	 * won't need to do so much here.
	 */
	npx_intrno = NRSVIDT + ffs(dvp->id_irq) - 1;
	save_eflags = read_eflags();
	disable_intr();
	save_icu1_mask = inb(IO_ICU1 + 1);
	save_icu2_mask = inb(IO_ICU2 + 1);
	save_idt_npxintr = idt[npx_intrno];
	save_idt_npxtrap = idt[16];
	outb(IO_ICU1 + 1, ~(IRQ_SLAVE | dvp->id_irq));
	outb(IO_ICU2 + 1, ~(dvp->id_irq >> 8));
	setidt(16, probetrap, SDT_SYS386TGT, SEL_KPL);
	setidt(npx_intrno, probeintr, SDT_SYS386IGT, SEL_KPL);
	npx_idt_probeintr = idt[npx_intrno];
	enable_intr();
	result = npxprobe1(dvp);
	disable_intr();
	outb(IO_ICU1 + 1, save_icu1_mask);
	outb(IO_ICU2 + 1, save_icu2_mask);
	idt[npx_intrno] = save_idt_npxintr;
	idt[16] = save_idt_npxtrap;
	write_eflags(save_eflags);
	return (result);
}

static int
npxprobe1(dvp)
	struct isa_device *dvp;
{
	int control;
	int status;
#ifdef lint
	npxintr();
#endif
	/*
	 * Partially reset the coprocessor, if any.  Some BIOS's don't reset
	 * it after a warm boot.
	 */
	outb(0xf1, 0);		/* full reset on some systems, NOP on others */
	outb(0xf0, 0);		/* clear BUSY# latch */
	/*
	 * Prepare to trap all ESC (i.e., NPX) instructions and all WAIT
	 * instructions.  We must set the CR0_MP bit and use the CR0_TS
	 * bit to control the trap, because setting the CR0_EM bit does
	 * not cause WAIT instructions to trap.  It's important to trap
	 * WAIT instructions - otherwise the "wait" variants of no-wait
	 * control instructions would degenerate to the "no-wait" variants
	 * after FP context switches but work correctly otherwise.  It's
	 * particularly important to trap WAITs when there is no NPX -
	 * otherwise the "wait" variants would always degenerate.
	 *
	 * Try setting CR0_NE to get correct error reporting on 486DX's.
	 * Setting it should fail or do nothing on lesser processors.
	 */
	load_cr0(rcr0() | CR0_MP | CR0_NE);
	/*
	 * But don't trap while we're probing.
	 */
	stop_emulating();
	/*
	 * Finish resetting the coprocessor, if any.  If there is an error
	 * pending, then we may get a bogus IRQ13, but probeintr() will handle
	 * it OK.  Bogus halts have never been observed, but we enabled
	 * IRQ13 and cleared the BUSY# latch early to handle them anyway.
	 */
	fninit();
	DELAY(1000);		/* wait for any IRQ13 (fwait might hang) */
#ifdef DIAGNOSTIC
	if (npx_intrs_while_probing != 0)
		printf("fninit caused %u bogus npx interrupt(s)\n",
		       npx_intrs_while_probing);
	if (npx_traps_while_probing != 0)
		printf("fninit caused %u bogus npx trap(s)\n",
		       npx_traps_while_probing);
#endif
	/*
	 * Check for a status of mostly zero.
	 */
	status = 0x5a5a;
	fnstsw(&status);
	if ((status & 0xb8ff) == 0) {
		/*
		 * Good, now check for a proper control word.
		 */
		control = 0x5a5a;	
		fnstcw(&control);
		if ((control & 0x1f3f) == 0x033f) {
			npx_exists = 1;
			/*
			 * We have an npx, now divide by 0 to see if exception
			 * 16 works.
			 */
			control &= ~(1 << 2);	/* enable divide by 0 trap */
			fldcw(&control);
			npx_traps_while_probing = npx_intrs_while_probing = 0;
			fp_divide_by_0();
			if (npx_traps_while_probing != 0) {
				/*
				 * Good, exception 16 works.
				 */
				npx_ex16 = 1;
				dvp->id_irq = 0;	/* zap the interrupt */
				/*
				 * special return value to flag that we do not
				 * actually use any I/O registers
				 */
				return (-1);
			}
			if (npx_intrs_while_probing != 0) {
				/*
				 * Bad, we are stuck with IRQ13.
				 */
				npx_irq13 = 1;
				npx0mask = dvp->id_irq;	/* npxattach too late */
				return (IO_NPXSIZE);
			}
			/*
			 * Worse, even IRQ13 is broken.  Use emulator.
			 */
		}
	}
	/*
	 * Probe failed, but we want to get to npxattach to initialize the
	 * emulator and say that it has been installed.  XXX handle devices
	 * that aren't really devices better.
	 */
	dvp->id_irq = 0;
	/*
	 * special return value to flag that we do not
	 * actually use any I/O registers
	 */
	return (-1);
}

/*
 * Attach routine - announce which it is, and wire into system
 */
int
npxattach(dvp)
	struct isa_device *dvp;
{
	if (!npx_ex16 && !npx_irq13) {
		if (npx_exists)
			printf("npx%d: Error reporting broken, using 387 emulator\n",dvp->id_unit);
		else
			printf("npx%d: 387 Emulator\n",dvp->id_unit);
	}
	npxinit(__INITIAL_NPXCW__);
	return (1);		/* XXX unused */
}

/*
 * Initialize floating point unit.
 */
void
npxinit(control)
	u_int control;
{
	struct save87 dummy;

	if (!npx_exists)
		return;
	/*
	 * fninit has the same h/w bugs as fnsave.  Use the detoxified
	 * fnsave to throw away any junk in the fpu.  fnsave initializes
	 * the fpu and sets npxproc = NULL as important side effects.
	 */
	npxsave(&dummy);
	stop_emulating();
	fldcw(&control);
	if (curpcb != NULL)
		fnsave(&curpcb->pcb_savefpu);
	start_emulating();
}

/*
 * Free coprocessor (if we have it).
 */
void
npxexit(p)
	struct proc *p;
{

	if (p == npxproc) {
		start_emulating();
		npxproc = NULL;
	}
}

/*
 * Record the FPU state and reinitialize it all except for the control word.
 * Then generate a SIGFPE.
 *
 * Reinitializing the state allows naive SIGFPE handlers to longjmp without
 * doing any fixups.
 *
 * XXX there is currently no way to pass the full error state to signal
 * handlers, and if this is a nested interrupt there is no way to pass even
 * a status code!  So there is no way to have a non-naive SIGFPE handler.  At
 * best a handler could do an fninit followed by an fldcw of a static value.
 * fnclex would be of little use because it would leave junk on the FPU stack.
 * Returning from the handler would be even less safe than usual because
 * IRQ13 exception handling makes exceptions even less precise than usual.
 */
void
npxintr(frame)
	struct intrframe frame;
{
	int code;

	if (npxproc == NULL || !npx_exists) {
		/* XXX no %p in stand/printf.c.  Cast to quiet gcc -Wall. */
		printf("npxintr: npxproc = %lx, curproc = %lx, npx_exists = %d\n",
		       (u_long) npxproc, (u_long) curproc, npx_exists);
		panic("npxintr from nowhere");
	}
	if (npxproc != curproc) {
		printf("npxintr: npxproc = %lx, curproc = %lx, npx_exists = %d\n",
		       (u_long) npxproc, (u_long) curproc, npx_exists);
		panic("npxintr from non-current process");
	}
	/*
	 * Save state.  This does an implied fninit.  It had better not halt
	 * the cpu or we'll hang.
	 */
	outb(0xf0, 0);
	fnsave(&curpcb->pcb_savefpu);
	fwait();
	/*
	 * Restore control word (was clobbered by fnsave).
	 */
	fldcw(&curpcb->pcb_savefpu.sv_env.en_cw);
	fwait();
	/*
	 * Remember the exception status word and tag word.  The current
	 * (almost fninit'ed) fpu state is in the fpu and the exception
	 * state just saved will soon be junk.  However, the implied fninit
	 * doesn't change the error pointers or register contents, and we
	 * preserved the control word and will copy the status and tag
	 * words, so the complete exception state can be recovered.
	 */
	curpcb->pcb_savefpu.sv_ex_sw = curpcb->pcb_savefpu.sv_env.en_sw;
	curpcb->pcb_savefpu.sv_ex_tw = curpcb->pcb_savefpu.sv_env.en_tw;

	/*
	 * Pass exception to process.
	 */
	if (ISPL(frame.if_cs) == SEL_UPL) {
		/*
		 * Interrupt is essentially a trap, so we can afford to call
		 * the SIGFPE handler (if any) as soon as the interrupt
		 * returns.
		 *
		 * XXX little or nothing is gained from this, and plenty is
		 * lost - the interrupt frame has to contain the trap frame
		 * (this is otherwise only necessary for the rescheduling trap
		 * in doreti, and the frame for that could easily be set up
		 * just before it is used).
		 */
		curproc->p_regs = (int *)&frame.if_es;
#ifdef notyet
		/*
		 * Encode the appropriate code for detailed information on
		 * this exception.
		 */
		code = XXX_ENCODE(curpcb->pcb_savefpu.sv_ex_sw);
#else
		code = 0;	/* XXX */
#endif
		trapsignal(curproc, SIGFPE, code);
	} else {
		/*
		 * Nested interrupt.  These losers occur when:
		 *	o an IRQ13 is bogusly generated at a bogus time, e.g.:
		 *		o immediately after an fnsave or frstor of an
		 *		  error state.
		 *		o a couple of 386 instructions after
		 *		  "fstpl _memvar" causes a stack overflow.
		 *	  These are especially nasty when combined with a
		 *	  trace trap.
		 *	o an IRQ13 occurs at the same time as another higher-
		 *	  priority interrupt.
		 *
		 * Treat them like a true async interrupt.
		 */
		psignal(npxproc, SIGFPE);
	}
}

/*
 * Implement device not available (DNA) exception
 *
 * It would be better to switch FP context here (only).  This would require
 * saving the state in the proc table instead of in the pcb.
 */
int
npxdna()
{
	if (!npx_exists)
		return (0);
	if (npxproc != NULL) {
		printf("npxdna: npxproc = %lx, curproc = %lx\n",
		       (u_long) npxproc, (u_long) curproc);
		panic("npxdna");
	}
	stop_emulating();
	/*
	 * Record new context early in case frstor causes an IRQ13.
	 */
	npxproc = curproc;
	/*
	 * The following frstor may cause an IRQ13 when the state being
	 * restored has a pending error.  The error will appear to have been
	 * triggered by the current (npx) user instruction even when that
	 * instruction is a no-wait instruction that should not trigger an
	 * error (e.g., fnclex).  On at least one 486 system all of the
	 * no-wait instructions are broken the same as frstor, so our
	 * treatment does not amplify the breakage.  On at least one
	 * 386/Cyrix 387 system, fnclex works correctly while frstor and
	 * fnsave are broken, so our treatment breaks fnclex if it is the
	 * first FPU instruction after a context switch.
	 */
	frstor(&curpcb->pcb_savefpu);

	return (1);
}

/*
 * Wrapper for fnsave instruction to handle h/w bugs.  If there is an error
 * pending, then fnsave generates a bogus IRQ13 on some systems.  Force
 * any IRQ13 to be handled immediately, and then ignore it.  This routine is
 * often called at splhigh so it must not use many system services.  In
 * particular, it's much easier to install a special handler than to
 * guarantee that it's safe to use npxintr() and its supporting code.
 */
void
npxsave(addr)
	struct save87 *addr;
{
	u_char	icu1_mask;
	u_char	icu2_mask;
	u_char	old_icu1_mask;
	u_char	old_icu2_mask;
	struct gate_descriptor	save_idt_npxintr;

	disable_intr();
	old_icu1_mask = inb(IO_ICU1 + 1);
	old_icu2_mask = inb(IO_ICU2 + 1);
	save_idt_npxintr = idt[npx_intrno];
	outb(IO_ICU1 + 1, old_icu1_mask & ~(IRQ_SLAVE | npx0mask));
	outb(IO_ICU2 + 1, old_icu2_mask & ~(npx0mask >> 8));
	idt[npx_intrno] = npx_idt_probeintr;
	enable_intr();
	stop_emulating();
	fnsave(addr);
	fwait();
	start_emulating();
	npxproc = NULL;
	disable_intr();
	icu1_mask = inb(IO_ICU1 + 1);	/* masks may have changed */
	icu2_mask = inb(IO_ICU2 + 1);
	outb(IO_ICU1 + 1,
	     (icu1_mask & ~npx0mask) | (old_icu1_mask & npx0mask));
	outb(IO_ICU2 + 1,
	     (icu2_mask & ~(npx0mask >> 8))
	     | (old_icu2_mask & (npx0mask >> 8)));
	idt[npx_intrno] = save_idt_npxintr;
	enable_intr();		/* back to usual state */
}

#endif /* NNPX > 0 */
