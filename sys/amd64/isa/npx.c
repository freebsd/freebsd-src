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
 *	$Id: npx.c,v 1.42 1997/04/26 11:46:03 peter Exp $
 */

#include "npx.h"
#if NNPX > 0

#include "opt_cpu.h"
#include "opt_math_emulate.h"
#include "opt_smp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/proc.h>
#ifdef NPX_DEBUG
#include <sys/syslog.h>
#endif
#include <sys/signalvar.h>

#include <machine/asmacros.h>
#include <machine/cpu.h>
#include <machine/pcb.h>
#include <machine/md_var.h>
#include <machine/trap.h>
#include <machine/clock.h>
#include <machine/specialreg.h>
#if defined(APIC_IO)
#include <machine/apic.h>
#include <machine/mpapic.h>
#endif /* APIC_IO */

#include <i386/isa/icu.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/isa.h>

/*
 * 387 and 287 Numeric Coprocessor Extension (NPX) Driver.
 */

/* Configuration flags. */
#define	NPX_DISABLE_I586_OPTIMIZED_BCOPY	(1 << 0)
#define	NPX_DISABLE_I586_OPTIMIZED_BZERO	(1 << 1)
#define	NPX_DISABLE_I586_OPTIMIZED_COPYIO	(1 << 2)

/* XXX - should be in header file. */
extern void (*bcopy_vector) __P((const void *from, void *to, size_t len));
extern void (*ovbcopy_vector) __P((const void *from, void *to, size_t len));
extern int (*copyin_vector) __P((const void *udaddr, void *kaddr, size_t len));
extern int (*copyout_vector) __P((const void *kaddr, void *udaddr, size_t len));

void	i586_bcopy __P((const void *from, void *to, size_t len));
void	i586_bzero __P((void *buf, size_t len));
int	i586_copyin __P((const void *udaddr, void *kaddr, size_t len));
int	i586_copyout __P((const void *kaddr, void *udaddr, size_t len));

#ifdef	__GNUC__

#define	fldcw(addr)		__asm("fldcw %0" : : "m" (*(addr)))
#define	fnclex()		__asm("fnclex")
#define	fninit()		__asm("fninit")
#define	fnop()			__asm("fnop")
#define	fnsave(addr)		__asm("fnsave %0" : "=m" (*(addr)))
#define	fnstcw(addr)		__asm("fnstcw %0" : "=m" (*(addr)))
#define	fnstsw(addr)		__asm("fnstsw %0" : "=m" (*(addr)))
#define	fp_divide_by_0()	__asm("fldz; fld1; fdiv %st,%st(1); fnop")
#define	frstor(addr)		__asm("frstor %0" : : "m" (*(addr)))
#define	start_emulating()	__asm("smsw %%ax; orb %0,%%al; lmsw %%ax" \
				      : : "n" (CR0_TS) : "ax")
#define	stop_emulating()	__asm("clts")

#else	/* not __GNUC__ */

void	fldcw		__P((caddr_t addr));
void	fnclex		__P((void));
void	fninit		__P((void));
void	fnop		__P((void));
void	fnsave		__P((caddr_t addr));
void	fnstcw		__P((caddr_t addr));
void	fnstsw		__P((caddr_t addr));
void	fp_divide_by_0	__P((void));
void	frstor		__P((caddr_t addr));
void	start_emulating	__P((void));
void	stop_emulating	__P((void));

#endif	/* __GNUC__ */

typedef u_char bool_t;

static	int	npxattach	__P((struct isa_device *dvp));
static	int	npxprobe	__P((struct isa_device *dvp));
static	int	npxprobe1	__P((struct isa_device *dvp));

struct	isa_driver npxdriver = {
	npxprobe, npxattach, "npx",
};

int	hw_float;		/* XXX currently just alias for npx_exists */

SYSCTL_INT(_hw,HW_FLOATINGPT, floatingpoint,
	CTLFLAG_RD, &hw_float, 0, 
	"Floatingpoint instructions executed in hardware");

static u_int	npx0_imask = SWI_CLOCK_MASK;
#ifdef SMP
#define npxproc	(SMPnpxproc[cpunumber()])
struct proc	*SMPnpxproc[NCPU];
#else
struct proc	*npxproc;
#endif

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
 * latch stuff in probeintr() can be moved to npxprobe().
 */
inthand_t probeintr;

#if defined(APIC_IO)

asm
("
	.text
	.p2align 2,0x90
" __XSTRING(CNAME(probeintr)) ":
	ss
	incl	" __XSTRING(CNAME(npx_intrs_while_probing)) "
	pushl	%eax
	movl	" __XSTRING(CNAME(apic_base)) ",%eax	# EOI to local APIC
	movl	$0,0xb0(,%eax,1)	# movl $0, APIC_EOI(%eax)
	movb	$0,%al
	outb	%al,$0xf0		# clear BUSY# latch
	popl	%eax
	iret
");

#else

asm
("
	.text
	.p2align 2,0x90
" __XSTRING(CNAME(probeintr)) ":
	ss
	incl	" __XSTRING(CNAME(npx_intrs_while_probing)) "
	pushl	%eax
	movb	$0x20,%al	# EOI (asm in strings loses cpp features)
	outb	%al,$0xa0	# IO_ICU2
	outb	%al,$0x20	# IO_ICU1
	movb	$0,%al
	outb	%al,$0xf0	# clear BUSY# latch
	popl	%eax
	iret
");

#endif /* APIC_IO */

inthand_t probetrap;
asm
("
	.text
	.p2align 2,0x90
" __XSTRING(CNAME(probetrap)) ":
	ss
	incl	" __XSTRING(CNAME(npx_traps_while_probing)) "
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
#if defined(APIC_IO)
	u_int	save_apic_mask;
#else
	u_char	save_icu1_mask;
	u_char	save_icu2_mask;
#endif /* APIC_IO */
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
#if defined(APIC_IO)
	save_apic_mask = INTRGET();
#else
	save_icu1_mask = inb(IO_ICU1 + 1);
	save_icu2_mask = inb(IO_ICU2 + 1);
#endif /* APIC_IO */
	save_idt_npxintr = idt[npx_intrno];
	save_idt_npxtrap = idt[16];
#if defined(APIC_IO)
	INTRSET( ~dvp->id_irq );
#else
	outb(IO_ICU1 + 1, ~(IRQ_SLAVE | dvp->id_irq));
	outb(IO_ICU2 + 1, ~(dvp->id_irq >> 8));
#endif /* APIC_IO */
	setidt(16, probetrap, SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(npx_intrno, probeintr, SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	npx_idt_probeintr = idt[npx_intrno];
	enable_intr();
	result = npxprobe1(dvp);
	disable_intr();
#if defined(APIC_IO)
	INTRSET( save_apic_mask );
#else
	outb(IO_ICU1 + 1, save_icu1_mask);
	outb(IO_ICU2 + 1, save_icu2_mask);
#endif /* APIC_IO */
	idt[npx_intrno] = save_idt_npxintr;
	idt[16] = save_idt_npxtrap;
	write_eflags(save_eflags);
	return (result);
}

static int
npxprobe1(dvp)
	struct isa_device *dvp;
{
	u_short control;
	u_short status;

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
	/*
	 * Don't use fwait here because it might hang.
	 * Don't use fnop here because it usually hangs if there is no FPU.
	 */
	DELAY(1000);		/* wait for any IRQ13 */
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
			hw_float = npx_exists = 1;
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
				/*
				 * npxattach would be too late to set npx0_imask.
				 */
				npx0_imask |= dvp->id_irq;
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
	/* The caller has printed "irq 13" for the npx_irq13 case. */
	if (!npx_irq13) {
		printf("npx%d: ", dvp->id_unit);
		if (npx_ex16)
			printf("INT 16 interface\n");
#if defined(MATH_EMULATE) || defined(GPL_MATH_EMULATE)
		else if (npx_exists) {
			printf("error reporting broken; using 387 emulator\n");
			hw_float = npx_exists = 0;
		} else
			printf("387 emulator\n");
#else
		else
			printf("no 387 emulator in kernel!\n");
#endif
	}
	npxinit(__INITIAL_NPXCW__);

#if defined(I586_CPU) && !defined(SMP)
	/* FPU not working under SMP yet */
	if (cpu_class == CPUCLASS_586 && npx_ex16) {
		if (!(dvp->id_flags & NPX_DISABLE_I586_OPTIMIZED_BCOPY)) {
			bcopy_vector = i586_bcopy;
			ovbcopy_vector = i586_bcopy;
		}
		if (!(dvp->id_flags & NPX_DISABLE_I586_OPTIMIZED_BZERO))
			bzero = i586_bzero;
		if (!(dvp->id_flags & NPX_DISABLE_I586_OPTIMIZED_COPYIO)) {
			copyin_vector = i586_copyin;
			copyout_vector = i586_copyout;
		}
	}
#endif

	return (1);		/* XXX unused */
}

/*
 * Initialize floating point unit.
 */
void
npxinit(control)
	u_short control;
{
	struct save87 dummy;

	if (!npx_exists)
		return;
	/*
	 * fninit has the same h/w bugs as fnsave.  Use the detoxified
	 * fnsave to throw away any junk in the fpu.  npxsave() initializes
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

	if (p == npxproc)
		npxsave(&curpcb->pcb_savefpu);
#ifdef NPX_DEBUG
	if (npx_exists) {
		u_int	masked_exceptions;

		masked_exceptions = curpcb->pcb_savefpu.sv_env.en_cw
				    & curpcb->pcb_savefpu.sv_env.en_sw & 0x7f;
		/*
		 * Log exceptions that would have trapped with the old
		 * control word (overflow, divide by 0, and invalid operand).
		 */
		if (masked_exceptions & 0x0d)
			log(LOG_ERR,
	"pid %d (%s) exited with masked floating point exceptions 0x%02x\n",
			    p->p_pid, p->p_comm, masked_exceptions);
	}
#endif
}

/*
 * Preserve the FP status word, clear FP exceptions, then generate a SIGFPE.
 *
 * Clearing exceptions is necessary mainly to avoid IRQ13 bugs.  We now
 * depend on longjmp() restoring a usable state.  Restoring the state
 * or examining it might fail if we didn't clear exceptions.
 *
 * XXX there is no standard way to tell SIGFPE handlers about the error
 * state.  The old interface:
 *
 *	void handler(int sig, int code, struct sigcontext *scp);
 *
 * is broken because it is non-ANSI and because the FP state is not in
 * struct sigcontext.
 *
 * XXX the FP state is not preserved across signal handlers.  So signal
 * handlers cannot afford to do FP unless they preserve the state or
 * longjmp() out.  Both preserving the state and longjmp()ing may be
 * destroyed by IRQ13 bugs.  Clearing FP exceptions is not an acceptable
 * solution for signals other than SIGFPE.
 */
void
npxintr(unit)
	int unit;
{
	int code;
	struct intrframe *frame;

	if (npxproc == NULL || !npx_exists) {
		printf("npxintr: npxproc = %p, curproc = %p, npx_exists = %d\n",
		       npxproc, curproc, npx_exists);
		panic("npxintr from nowhere");
	}
	if (npxproc != curproc) {
		printf("npxintr: npxproc = %p, curproc = %p, npx_exists = %d\n",
		       npxproc, curproc, npx_exists);
		panic("npxintr from non-current process");
	}

	outb(0xf0, 0);
	fnstsw(&curpcb->pcb_savefpu.sv_ex_sw);
	fnclex();
	fnop();

	/*
	 * Pass exception to process.
	 */
	frame = (struct intrframe *)&unit;	/* XXX */
	if (ISPL(frame->if_cs) == SEL_UPL) {
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
		curproc->p_md.md_regs = (struct trapframe *)&frame->if_es;
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
		psignal(curproc, SIGFPE);
	}
}

/*
 * Implement device not available (DNA) exception
 *
 * It would be better to switch FP context here (if curproc != npxproc)
 * and not necessarily for every context switch, but it is too hard to
 * access foreign pcb's.
 */
int
npxdna()
{
	if (!npx_exists)
		return (0);
	if (npxproc != NULL) {
		printf("npxdna: npxproc = %p, curproc = %p\n",
		       npxproc, curproc);
		panic("npxdna");
	}
	stop_emulating();
	/*
	 * Record new context early in case frstor causes an IRQ13.
	 */
	npxproc = curproc;
	curpcb->pcb_savefpu.sv_ex_sw = 0;
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
#if defined(APIC_IO)
	u_int	apic_mask;
	u_int	old_apic_mask;
#else
	u_char	icu1_mask;
	u_char	icu2_mask;
	u_char	old_icu1_mask;
	u_char	old_icu2_mask;
#endif /* APIC_IO */
	struct gate_descriptor	save_idt_npxintr;

	disable_intr();
#if defined(APIC_IO)
	old_apic_mask = INTRGET();
#else
	old_icu1_mask = inb(IO_ICU1 + 1);
	old_icu2_mask = inb(IO_ICU2 + 1);
#endif /* APIC_IO */
	save_idt_npxintr = idt[npx_intrno];
#if defined(APIC_IO)
	/** FIXME: try clrIoApicMaskBit( npx0_imask ); */
	INTRSET( old_apic_mask & ~(npx0_imask & 0xffff) );
#else
	outb(IO_ICU1 + 1, old_icu1_mask & ~(IRQ_SLAVE | npx0_imask));
	outb(IO_ICU2 + 1, old_icu2_mask & ~(npx0_imask >> 8));
#endif /* APIC_IO */
	idt[npx_intrno] = npx_idt_probeintr;
	enable_intr();
	stop_emulating();
	fnsave(addr);
	fnop();
	start_emulating();
	npxproc = NULL;
	disable_intr();
#if defined(APIC_IO)
	apic_mask = INTRGET();		/* masks may have changed */
        INTRSET( (apic_mask & ~(npx0_imask & 0xffff)) |
		 (old_apic_mask & (npx0_imask & 0xffff)));
#else
	icu1_mask = inb(IO_ICU1 + 1);	/* masks may have changed */
	icu2_mask = inb(IO_ICU2 + 1);
	outb(IO_ICU1 + 1,
	     (icu1_mask & ~npx0_imask) | (old_icu1_mask & npx0_imask));
	outb(IO_ICU2 + 1,
	     (icu2_mask & ~(npx0_imask >> 8))
	     | (old_icu2_mask & (npx0_imask >> 8)));
#endif /* APIC_IO */
	idt[npx_intrno] = save_idt_npxintr;
	enable_intr();		/* back to usual state */
}

#endif /* NNPX > 0 */
