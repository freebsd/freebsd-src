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
 * $FreeBSD$
 */

#include "opt_debug_npx.h"
#include "opt_math_emulate.h"
#include "opt_npx.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <machine/bus.h>
#include <sys/rman.h>
#ifdef NPX_DEBUG
#include <sys/syslog.h>
#endif
#include <sys/signalvar.h>

#ifndef SMP
#include <machine/asmacros.h>
#endif
#include <machine/cputypes.h>
#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/psl.h>
#ifndef SMP
#include <machine/clock.h>
#endif
#include <machine/resource.h>
#include <machine/specialreg.h>
#include <machine/segments.h>

#ifndef SMP
#include <i386/isa/icu.h>
#include <i386/isa/intr_machdep.h>
#ifdef PC98
#include <pc98/pc98/pc98.h>
#else
#include <i386/isa/isa.h>
#endif
#endif
#include <isa/isavar.h>

/*
 * 387 and 287 Numeric Coprocessor Extension (NPX) Driver.
 */

/* Configuration flags. */
#define	NPX_DISABLE_I586_OPTIMIZED_BCOPY	(1 << 0)
#define	NPX_DISABLE_I586_OPTIMIZED_BZERO	(1 << 1)
#define	NPX_DISABLE_I586_OPTIMIZED_COPYIO	(1 << 2)
#define	NPX_PREFER_EMULATOR			(1 << 3)

#ifdef	__GNUC__

#define	fldcw(addr)		__asm("fldcw %0" : : "m" (*(addr)))
#define	fnclex()		__asm("fnclex")
#define	fninit()		__asm("fninit")
#define	fnop()			__asm("fnop")
#define	fnsave(addr)		__asm __volatile("fnsave %0" : "=m" (*(addr)))
#define	fnstcw(addr)		__asm __volatile("fnstcw %0" : "=m" (*(addr)))
#define	fnstsw(addr)		__asm __volatile("fnstsw %0" : "=m" (*(addr)))
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

static	int	npx_attach	__P((device_t dev));
	void	npx_intr	__P((void *));
static	void	npx_identify	__P((driver_t *driver, device_t parent));
static	int	npx_probe	__P((device_t dev));
static	int	npx_probe1	__P((device_t dev));
#ifdef I586_CPU
static	long	timezero	__P((const char *funcname,
				     void (*func)(void *buf, size_t len)));
#endif /* I586_CPU */

int	hw_float;		/* XXX currently just alias for npx_exists */

SYSCTL_INT(_hw,HW_FLOATINGPT, floatingpoint,
	CTLFLAG_RD, &hw_float, 0, 
	"Floatingpoint instructions executed in hardware");

#ifndef SMP
static	u_int			npx0_imask = 0;
static	struct gate_descriptor	npx_idt_probeintr;
static	int			npx_intrno;
static	volatile u_int		npx_intrs_while_probing;
static	volatile u_int		npx_traps_while_probing;
#endif

static	bool_t			npx_ex16;
static	bool_t			npx_exists;
static	bool_t			npx_irq13;
static	int			npx_irq;	/* irq number */

#ifndef SMP
/*
 * Special interrupt handlers.  Someday intr0-intr15 will be used to count
 * interrupts.  We'll still need a special exception 16 handler.  The busy
 * latch stuff in probeintr() can be moved to npxprobe().
 */
inthand_t probeintr;
__asm("								\n\
	.text							\n\
	.p2align 2,0x90						\n\
	.type	" __XSTRING(CNAME(probeintr)) ",@function	\n\
" __XSTRING(CNAME(probeintr)) ":				\n\
	ss							\n\
	incl	" __XSTRING(CNAME(npx_intrs_while_probing)) "	\n\
	pushl	%eax						\n\
	movb	$0x20,%al	# EOI (asm in strings loses cpp features) \n\
#ifdef PC98
	outb	%al,$0x08	# IO_ICU2			\n\
	outb	%al,$0x00	# IO_ICU1			\n\
#else
	outb	%al,$0xa0	# IO_ICU2			\n\
	outb	%al,$0x20	# IO_ICU1			\n\
#endif
	movb	$0,%al						\n\
#ifdef PC98
	outb	%al,$0xf8	# clear BUSY# latch		\n\
#else
	outb	%al,$0xf0	# clear BUSY# latch		\n\
#endif
	popl	%eax						\n\
	iret							\n\
");

inthand_t probetrap;
__asm("								\n\
	.text							\n\
	.p2align 2,0x90						\n\
	.type	" __XSTRING(CNAME(probetrap)) ",@function	\n\
" __XSTRING(CNAME(probetrap)) ":				\n\
	ss							\n\
	incl	" __XSTRING(CNAME(npx_traps_while_probing)) "	\n\
	fnclex							\n\
	iret							\n\
");
#endif /* SMP */


/*
 * Identify routine.  Create a connection point on our parent for probing.
 */
static void
npx_identify(driver, parent)
	driver_t *driver;
	device_t parent;
{
	device_t child;

	child = BUS_ADD_CHILD(parent, 0, "npx", 0);
	if (child == NULL)
		panic("npx_identify");
}

/*
 * Probe routine.  Initialize cr0 to give correct behaviour for [f]wait
 * whether the device exists or not (XXX should be elsewhere).  Set flags
 * to tell npxattach() what to do.  Modify device struct if npx doesn't
 * need to use interrupts.  Return 1 if device exists.
 */
static int
npx_probe(dev)
	device_t dev;
{
#ifdef SMP

	if (resource_int_value("npx", 0, "irq", &npx_irq) != 0)
#ifdef PC98
		npx_irq = 8;
#else
		npx_irq = 13;
#endif
	return npx_probe1(dev);

#else /* SMP */

	int	result;
	critical_t	savecrit;
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
	if (resource_int_value("npx", 0, "irq", &npx_irq) != 0)
#ifdef PC98
		npx_irq = 8;
#else
		npx_irq = 13;
#endif
	npx_intrno = NRSVIDT + npx_irq;
	savecrit = critical_enter();
#ifdef PC98
	save_icu1_mask = inb(IO_ICU1 + 2);
	save_icu2_mask = inb(IO_ICU2 + 2);
#else
	save_icu1_mask = inb(IO_ICU1 + 1);
	save_icu2_mask = inb(IO_ICU2 + 1);
#endif
	save_idt_npxintr = idt[npx_intrno];
	save_idt_npxtrap = idt[16];
#ifdef PC98
	outb(IO_ICU1 + 2, (u_int8_t) ~IRQ_SLAVE);
	outb(IO_ICU2 + 2, ~(1 << (npx_irq - 8)));
#else
	outb(IO_ICU1 + 1, ~IRQ_SLAVE);
	outb(IO_ICU2 + 1, ~(1 << (npx_irq - 8)));
#endif
	setidt(16, probetrap, SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(npx_intrno, probeintr, SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	npx_idt_probeintr = idt[npx_intrno];

	/*
	 * XXX This looks highly bogus, but it appears that npc_probe1
	 * needs interrupts enabled.  Does this make any difference
	 * here?
	 */
	critical_exit(savecrit);
	result = npx_probe1(dev);
	savecrit = critical_enter();
#ifdef PC98
	outb(IO_ICU1 + 2, save_icu1_mask);
	outb(IO_ICU2 + 2, save_icu2_mask);
#else
	outb(IO_ICU1 + 1, save_icu1_mask);
	outb(IO_ICU2 + 1, save_icu2_mask);
#endif
	idt[npx_intrno] = save_idt_npxintr;
	idt[16] = save_idt_npxtrap;
	critical_exit(savecrit);
	return (result);

#endif /* SMP */
}

static int
npx_probe1(dev)
	device_t dev;
{
#ifndef SMP
	u_short control;
	u_short status;
#endif

	/*
	 * Partially reset the coprocessor, if any.  Some BIOS's don't reset
	 * it after a warm boot.
	 */
#ifdef PC98
	outb(0xf8,0);
#else
	outb(0xf1, 0);		/* full reset on some systems, NOP on others */
	outb(0xf0, 0);		/* clear BUSY# latch */
#endif
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

#ifdef SMP
	/*
	 * Exception 16 MUST work for SMP.
	 */
	npx_irq13 = 0;
	npx_ex16 = hw_float = npx_exists = 1;
	device_set_desc(dev, "math processor");
	return (0);

#else /* !SMP */
	device_set_desc(dev, "math processor");

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
#ifdef FPU_ERROR_BROKEN
			/*
			 * FPU error signal doesn't work on some CPU
			 * accelerator board.
			 */
			npx_ex16 = 1;
			return (0);
#endif
			npx_traps_while_probing = npx_intrs_while_probing = 0;
			fp_divide_by_0();
			if (npx_traps_while_probing != 0) {
				/*
				 * Good, exception 16 works.
				 */
				npx_ex16 = 1;
				return (0);
			}
			if (npx_intrs_while_probing != 0) {
				int	rid;
				struct	resource *r;
				void	*intr;
				/*
				 * Bad, we are stuck with IRQ13.
				 */
				npx_irq13 = 1;
				/*
				 * npxattach would be too late to set npx0_imask
				 */
				npx0_imask |= (1 << npx_irq);

				/*
				 * We allocate these resources permanently,
				 * so there is no need to keep track of them.
				 */
				rid = 0;
				r = bus_alloc_resource(dev, SYS_RES_IOPORT,
						       &rid, IO_NPX, IO_NPX,
						       IO_NPXSIZE, RF_ACTIVE);
				if (r == 0)
					panic("npx: can't get ports");
				rid = 0;
				r = bus_alloc_resource(dev, SYS_RES_IRQ,
						       &rid, npx_irq, npx_irq,
						       1, RF_ACTIVE);
				if (r == 0)
					panic("npx: can't get IRQ");
				BUS_SETUP_INTR(device_get_parent(dev),
					       dev, r,
					       INTR_TYPE_MISC | INTR_MPSAFE,
					       npx_intr, 0, &intr);
				if (intr == 0)
					panic("npx: can't create intr");

				return (0);
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
	return (0);
#endif /* SMP */
}

/*
 * Attach routine - announce which it is, and wire into system
 */
int
npx_attach(dev)
	device_t dev;
{
	int flags;

	if (resource_int_value("npx", 0, "flags", &flags) != 0)
		flags = 0;

	if (flags)
		device_printf(dev, "flags 0x%x ", flags);
	if (npx_irq13) {
		device_printf(dev, "using IRQ 13 interface\n");
	} else {
#if defined(MATH_EMULATE) || defined(GPL_MATH_EMULATE)
		if (npx_ex16) {
			if (!(flags & NPX_PREFER_EMULATOR))
				device_printf(dev, "INT 16 interface\n");
			else {
				device_printf(dev, "FPU exists, but flags request "
				    "emulator\n");
				hw_float = npx_exists = 0;
			}
		} else if (npx_exists) {
			device_printf(dev, "error reporting broken; using 387 emulator\n");
			hw_float = npx_exists = 0;
		} else
			device_printf(dev, "387 emulator\n");
#else
		if (npx_ex16) {
			device_printf(dev, "INT 16 interface\n");
			if (flags & NPX_PREFER_EMULATOR) {
				device_printf(dev, "emulator requested, but none compiled "
				    "into kernel, using FPU\n");
			}
		} else
			device_printf(dev, "no 387 emulator in kernel and no FPU!\n");
#endif
	}
	npxinit(__INITIAL_NPXCW__);

#ifdef I586_CPU_XXX
	if (cpu_class == CPUCLASS_586 && npx_ex16 && npx_exists &&
	    timezero("i586_bzero()", i586_bzero) <
	    timezero("bzero()", bzero) * 4 / 5) {
		if (!(flags & NPX_DISABLE_I586_OPTIMIZED_BCOPY)) {
			bcopy_vector = i586_bcopy;
			ovbcopy_vector = i586_bcopy;
		}
		if (!(flags & NPX_DISABLE_I586_OPTIMIZED_BZERO))
			bzero = i586_bzero;
		if (!(flags & NPX_DISABLE_I586_OPTIMIZED_COPYIO)) {
			copyin_vector = i586_copyin;
			copyout_vector = i586_copyout;
		}
	}
#endif

	return (0);		/* XXX unused */
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
	if (PCPU_GET(curpcb) != NULL)
		fnsave(&PCPU_GET(curpcb)->pcb_savefpu);
	start_emulating();
}

/*
 * Free coprocessor (if we have it).
 */
void
npxexit(p)
	struct proc *p;
{

	if (p == PCPU_GET(npxproc))
		npxsave(&PCPU_GET(curpcb)->pcb_savefpu);
#ifdef NPX_DEBUG
	if (npx_exists) {
		u_int	masked_exceptions;

		masked_exceptions = PCPU_GET(curpcb)->pcb_savefpu.sv_env.en_cw
		    & PCPU_GET(curpcb)->pcb_savefpu.sv_env.en_sw & 0x7f;
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
 * The following mechanism is used to ensure that the FPE_... value
 * that is passed as a trapcode to the signal handler of the user
 * process does not have more than one bit set.
 * 
 * Multiple bits may be set if the user process modifies the control
 * word while a status word bit is already set.  While this is a sign
 * of bad coding, we have no choise than to narrow them down to one
 * bit, since we must not send a trapcode that is not exactly one of
 * the FPE_ macros.
 *
 * The mechanism has a static table with 127 entries.  Each combination
 * of the 7 FPU status word exception bits directly translates to a
 * position in this table, where a single FPE_... value is stored.
 * This FPE_... value stored there is considered the "most important"
 * of the exception bits and will be sent as the signal code.  The
 * precedence of the bits is based upon Intel Document "Numerical
 * Applications", Chapter "Special Computational Situations".
 *
 * The macro to choose one of these values does these steps: 1) Throw
 * away status word bits that cannot be masked.  2) Throw away the bits
 * currently masked in the control word, assuming the user isn't
 * interested in them anymore.  3) Reinsert status word bit 7 (stack
 * fault) if it is set, which cannot be masked but must be presered.
 * 4) Use the remaining bits to point into the trapcode table.
 *
 * The 6 maskable bits in order of their preference, as stated in the
 * above referenced Intel manual:
 * 1  Invalid operation (FP_X_INV)
 * 1a   Stack underflow
 * 1b   Stack overflow
 * 1c   Operand of unsupported format
 * 1d   SNaN operand.
 * 2  QNaN operand (not an exception, irrelavant here)
 * 3  Any other invalid-operation not mentioned above or zero divide
 *      (FP_X_INV, FP_X_DZ)
 * 4  Denormal operand (FP_X_DNML)
 * 5  Numeric over/underflow (FP_X_OFL, FP_X_UFL)
 * 6  Inexact result (FP_X_IMP) 
 */
static char fpetable[128] = {
	0,
	FPE_FLTINV,	/*  1 - INV */
	FPE_FLTUND,	/*  2 - DNML */
	FPE_FLTINV,	/*  3 - INV | DNML */
	FPE_FLTDIV,	/*  4 - DZ */
	FPE_FLTINV,	/*  5 - INV | DZ */
	FPE_FLTDIV,	/*  6 - DNML | DZ */
	FPE_FLTINV,	/*  7 - INV | DNML | DZ */
	FPE_FLTOVF,	/*  8 - OFL */
	FPE_FLTINV,	/*  9 - INV | OFL */
	FPE_FLTUND,	/*  A - DNML | OFL */
	FPE_FLTINV,	/*  B - INV | DNML | OFL */
	FPE_FLTDIV,	/*  C - DZ | OFL */
	FPE_FLTINV,	/*  D - INV | DZ | OFL */
	FPE_FLTDIV,	/*  E - DNML | DZ | OFL */
	FPE_FLTINV,	/*  F - INV | DNML | DZ | OFL */
	FPE_FLTUND,	/* 10 - UFL */
	FPE_FLTINV,	/* 11 - INV | UFL */
	FPE_FLTUND,	/* 12 - DNML | UFL */
	FPE_FLTINV,	/* 13 - INV | DNML | UFL */
	FPE_FLTDIV,	/* 14 - DZ | UFL */
	FPE_FLTINV,	/* 15 - INV | DZ | UFL */
	FPE_FLTDIV,	/* 16 - DNML | DZ | UFL */
	FPE_FLTINV,	/* 17 - INV | DNML | DZ | UFL */
	FPE_FLTOVF,	/* 18 - OFL | UFL */
	FPE_FLTINV,	/* 19 - INV | OFL | UFL */
	FPE_FLTUND,	/* 1A - DNML | OFL | UFL */
	FPE_FLTINV,	/* 1B - INV | DNML | OFL | UFL */
	FPE_FLTDIV,	/* 1C - DZ | OFL | UFL */
	FPE_FLTINV,	/* 1D - INV | DZ | OFL | UFL */
	FPE_FLTDIV,	/* 1E - DNML | DZ | OFL | UFL */
	FPE_FLTINV,	/* 1F - INV | DNML | DZ | OFL | UFL */
	FPE_FLTRES,	/* 20 - IMP */
	FPE_FLTINV,	/* 21 - INV | IMP */
	FPE_FLTUND,	/* 22 - DNML | IMP */
	FPE_FLTINV,	/* 23 - INV | DNML | IMP */
	FPE_FLTDIV,	/* 24 - DZ | IMP */
	FPE_FLTINV,	/* 25 - INV | DZ | IMP */
	FPE_FLTDIV,	/* 26 - DNML | DZ | IMP */
	FPE_FLTINV,	/* 27 - INV | DNML | DZ | IMP */
	FPE_FLTOVF,	/* 28 - OFL | IMP */
	FPE_FLTINV,	/* 29 - INV | OFL | IMP */
	FPE_FLTUND,	/* 2A - DNML | OFL | IMP */
	FPE_FLTINV,	/* 2B - INV | DNML | OFL | IMP */
	FPE_FLTDIV,	/* 2C - DZ | OFL | IMP */
	FPE_FLTINV,	/* 2D - INV | DZ | OFL | IMP */
	FPE_FLTDIV,	/* 2E - DNML | DZ | OFL | IMP */
	FPE_FLTINV,	/* 2F - INV | DNML | DZ | OFL | IMP */
	FPE_FLTUND,	/* 30 - UFL | IMP */
	FPE_FLTINV,	/* 31 - INV | UFL | IMP */
	FPE_FLTUND,	/* 32 - DNML | UFL | IMP */
	FPE_FLTINV,	/* 33 - INV | DNML | UFL | IMP */
	FPE_FLTDIV,	/* 34 - DZ | UFL | IMP */
	FPE_FLTINV,	/* 35 - INV | DZ | UFL | IMP */
	FPE_FLTDIV,	/* 36 - DNML | DZ | UFL | IMP */
	FPE_FLTINV,	/* 37 - INV | DNML | DZ | UFL | IMP */
	FPE_FLTOVF,	/* 38 - OFL | UFL | IMP */
	FPE_FLTINV,	/* 39 - INV | OFL | UFL | IMP */
	FPE_FLTUND,	/* 3A - DNML | OFL | UFL | IMP */
	FPE_FLTINV,	/* 3B - INV | DNML | OFL | UFL | IMP */
	FPE_FLTDIV,	/* 3C - DZ | OFL | UFL | IMP */
	FPE_FLTINV,	/* 3D - INV | DZ | OFL | UFL | IMP */
	FPE_FLTDIV,	/* 3E - DNML | DZ | OFL | UFL | IMP */
	FPE_FLTINV,	/* 3F - INV | DNML | DZ | OFL | UFL | IMP */
	FPE_FLTSUB,	/* 40 - STK */
	FPE_FLTSUB,	/* 41 - INV | STK */
	FPE_FLTUND,	/* 42 - DNML | STK */
	FPE_FLTSUB,	/* 43 - INV | DNML | STK */
	FPE_FLTDIV,	/* 44 - DZ | STK */
	FPE_FLTSUB,	/* 45 - INV | DZ | STK */
	FPE_FLTDIV,	/* 46 - DNML | DZ | STK */
	FPE_FLTSUB,	/* 47 - INV | DNML | DZ | STK */
	FPE_FLTOVF,	/* 48 - OFL | STK */
	FPE_FLTSUB,	/* 49 - INV | OFL | STK */
	FPE_FLTUND,	/* 4A - DNML | OFL | STK */
	FPE_FLTSUB,	/* 4B - INV | DNML | OFL | STK */
	FPE_FLTDIV,	/* 4C - DZ | OFL | STK */
	FPE_FLTSUB,	/* 4D - INV | DZ | OFL | STK */
	FPE_FLTDIV,	/* 4E - DNML | DZ | OFL | STK */
	FPE_FLTSUB,	/* 4F - INV | DNML | DZ | OFL | STK */
	FPE_FLTUND,	/* 50 - UFL | STK */
	FPE_FLTSUB,	/* 51 - INV | UFL | STK */
	FPE_FLTUND,	/* 52 - DNML | UFL | STK */
	FPE_FLTSUB,	/* 53 - INV | DNML | UFL | STK */
	FPE_FLTDIV,	/* 54 - DZ | UFL | STK */
	FPE_FLTSUB,	/* 55 - INV | DZ | UFL | STK */
	FPE_FLTDIV,	/* 56 - DNML | DZ | UFL | STK */
	FPE_FLTSUB,	/* 57 - INV | DNML | DZ | UFL | STK */
	FPE_FLTOVF,	/* 58 - OFL | UFL | STK */
	FPE_FLTSUB,	/* 59 - INV | OFL | UFL | STK */
	FPE_FLTUND,	/* 5A - DNML | OFL | UFL | STK */
	FPE_FLTSUB,	/* 5B - INV | DNML | OFL | UFL | STK */
	FPE_FLTDIV,	/* 5C - DZ | OFL | UFL | STK */
	FPE_FLTSUB,	/* 5D - INV | DZ | OFL | UFL | STK */
	FPE_FLTDIV,	/* 5E - DNML | DZ | OFL | UFL | STK */
	FPE_FLTSUB,	/* 5F - INV | DNML | DZ | OFL | UFL | STK */
	FPE_FLTRES,	/* 60 - IMP | STK */
	FPE_FLTSUB,	/* 61 - INV | IMP | STK */
	FPE_FLTUND,	/* 62 - DNML | IMP | STK */
	FPE_FLTSUB,	/* 63 - INV | DNML | IMP | STK */
	FPE_FLTDIV,	/* 64 - DZ | IMP | STK */
	FPE_FLTSUB,	/* 65 - INV | DZ | IMP | STK */
	FPE_FLTDIV,	/* 66 - DNML | DZ | IMP | STK */
	FPE_FLTSUB,	/* 67 - INV | DNML | DZ | IMP | STK */
	FPE_FLTOVF,	/* 68 - OFL | IMP | STK */
	FPE_FLTSUB,	/* 69 - INV | OFL | IMP | STK */
	FPE_FLTUND,	/* 6A - DNML | OFL | IMP | STK */
	FPE_FLTSUB,	/* 6B - INV | DNML | OFL | IMP | STK */
	FPE_FLTDIV,	/* 6C - DZ | OFL | IMP | STK */
	FPE_FLTSUB,	/* 6D - INV | DZ | OFL | IMP | STK */
	FPE_FLTDIV,	/* 6E - DNML | DZ | OFL | IMP | STK */
	FPE_FLTSUB,	/* 6F - INV | DNML | DZ | OFL | IMP | STK */
	FPE_FLTUND,	/* 70 - UFL | IMP | STK */
	FPE_FLTSUB,	/* 71 - INV | UFL | IMP | STK */
	FPE_FLTUND,	/* 72 - DNML | UFL | IMP | STK */
	FPE_FLTSUB,	/* 73 - INV | DNML | UFL | IMP | STK */
	FPE_FLTDIV,	/* 74 - DZ | UFL | IMP | STK */
	FPE_FLTSUB,	/* 75 - INV | DZ | UFL | IMP | STK */
	FPE_FLTDIV,	/* 76 - DNML | DZ | UFL | IMP | STK */
	FPE_FLTSUB,	/* 77 - INV | DNML | DZ | UFL | IMP | STK */
	FPE_FLTOVF,	/* 78 - OFL | UFL | IMP | STK */
	FPE_FLTSUB,	/* 79 - INV | OFL | UFL | IMP | STK */
	FPE_FLTUND,	/* 7A - DNML | OFL | UFL | IMP | STK */
	FPE_FLTSUB,	/* 7B - INV | DNML | OFL | UFL | IMP | STK */
	FPE_FLTDIV,	/* 7C - DZ | OFL | UFL | IMP | STK */
	FPE_FLTSUB,	/* 7D - INV | DZ | OFL | UFL | IMP | STK */
	FPE_FLTDIV,	/* 7E - DNML | DZ | OFL | UFL | IMP | STK */
	FPE_FLTSUB,	/* 7F - INV | DNML | DZ | OFL | UFL | IMP | STK */
};

/*
 * Preserve the FP status word, clear FP exceptions, then generate a SIGFPE.
 *
 * Clearing exceptions is necessary mainly to avoid IRQ13 bugs.  We now
 * depend on longjmp() restoring a usable state.  Restoring the state
 * or examining it might fail if we didn't clear exceptions.
 *
 * The error code chosen will be one of the FPE_... macros. It will be
 * sent as the second argument to old BSD-style signal handlers and as
 * "siginfo_t->si_code" (second argument) to SA_SIGINFO signal handlers.
 *
 * XXX the FP state is not preserved across signal handlers.  So signal
 * handlers cannot afford to do FP unless they preserve the state or
 * longjmp() out.  Both preserving the state and longjmp()ing may be
 * destroyed by IRQ13 bugs.  Clearing FP exceptions is not an acceptable
 * solution for signals other than SIGFPE.
 */
void
npx_intr(dummy)
	void *dummy;
{
	int code;
	u_short control;
	struct intrframe *frame;

	if (!npx_exists) {
		printf("npxintr: npxproc = %p, curproc = %p, npx_exists = %d\n",
		       PCPU_GET(npxproc), curproc, npx_exists);
		panic("npxintr from nowhere");
	}
#ifdef	PC98
	outb(0xf8, 0);
#else
	outb(0xf0, 0);
#endif
	mtx_lock_spin(&sched_lock);
	if (PCPU_GET(npxproc) != curproc) {
		/*
		 * Interrupt handling (for this or another interrupt) has
		 * switched npxproc from underneath us before we managed
		 * to handle this interrupt.  Just ignore this interrupt.
		 * Control will eventually return to the instruction that
		 * caused it and it will repeat.  In the npx_ex16 case,
		 * then we will eventually (usually soon) win the race.
		 * In the npx_irq13 case, we will always lose the race
		 * because we have switched to the IRQ13 thread.  This will
		 * be fixed later.
		 */
		mtx_unlock_spin(&sched_lock);
		return;
	}
	fnstsw(&PCPU_GET(curpcb)->pcb_savefpu.sv_ex_sw);
	fnstcw(&control);
	fnclex();
	mtx_unlock_spin(&sched_lock);

	/*
	 * Pass exception to process.
	 */
	mtx_lock(&Giant);
	frame = (struct intrframe *)&dummy;	/* XXX */
	if ((ISPL(frame->if_cs) == SEL_UPL) || (frame->if_eflags & PSL_VM)) {
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
		curproc->p_md.md_regs = INTR_TO_TRAPFRAME(frame);
		/*
		 * Encode the appropriate code for detailed information on
		 * this exception.
		 */
		code = 
		    fpetable[(PCPU_GET(curpcb)->pcb_savefpu.sv_ex_sw & ~control & 0x3f) |
			(PCPU_GET(curpcb)->pcb_savefpu.sv_ex_sw & 0x40)];
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
		PROC_LOCK(curproc);
		psignal(curproc, SIGFPE);
		PROC_UNLOCK(curproc);
	}
	mtx_unlock(&Giant);
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
	critical_t s;

	if (!npx_exists)
		return (0);
	if (PCPU_GET(npxproc) != NULL) {
		printf("npxdna: npxproc = %p, curproc = %p\n",
		       PCPU_GET(npxproc), curproc);
		panic("npxdna");
	}
	s = critical_enter();
	stop_emulating();
	/*
	 * Record new context early in case frstor causes an IRQ13.
	 */
	PCPU_SET(npxproc, CURPROC);
	PCPU_GET(curpcb)->pcb_savefpu.sv_ex_sw = 0;
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
	frstor(&PCPU_GET(curpcb)->pcb_savefpu);
	critical_exit(s);

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
#ifdef SMP

	stop_emulating();
	fnsave(addr);
	/* fnop(); */
	start_emulating();
	PCPU_SET(npxproc, NULL);

#else /* SMP */

	critical_t savecrit;
	u_char	icu1_mask;
	u_char	icu2_mask;
	u_char	old_icu1_mask;
	u_char	old_icu2_mask;
	struct gate_descriptor	save_idt_npxintr;

	savecrit = critical_enter();
#ifdef PC98
	old_icu1_mask = inb(IO_ICU1 + 2);
	old_icu2_mask = inb(IO_ICU2 + 2);
#else
	old_icu1_mask = inb(IO_ICU1 + 1);
	old_icu2_mask = inb(IO_ICU2 + 1);
#endif
	save_idt_npxintr = idt[npx_intrno];
#ifdef PC98
	outb(IO_ICU1 + 2, old_icu1_mask & ~(IRQ_SLAVE | npx0_imask));
	outb(IO_ICU2 + 2, old_icu2_mask & ~(npx0_imask >> 8));
#else
	outb(IO_ICU1 + 1, old_icu1_mask & ~(IRQ_SLAVE | npx0_imask));
	outb(IO_ICU2 + 1, old_icu2_mask & ~(npx0_imask >> 8));
#endif
	idt[npx_intrno] = npx_idt_probeintr;
	critical_exit(savecrit);
	stop_emulating();
	fnsave(addr);
	fnop();
	start_emulating();
	PCPU_SET(npxproc, NULL);
	savecrit = critical_enter();
#ifdef PC98
	icu1_mask = inb(IO_ICU1 + 2);	/* masks may have changed */
	icu2_mask = inb(IO_ICU2 + 2);
	outb(IO_ICU1 + 2,
	     (icu1_mask & ~npx0_imask) | (old_icu1_mask & npx0_imask));
	outb(IO_ICU2 + 2,
	     (icu2_mask & ~(npx0_imask >> 8))
	     | (old_icu2_mask & (npx0_imask >> 8)));
#else
	icu1_mask = inb(IO_ICU1 + 1);	/* masks may have changed */
	icu2_mask = inb(IO_ICU2 + 1);
	outb(IO_ICU1 + 1,
	     (icu1_mask & ~npx0_imask) | (old_icu1_mask & npx0_imask));
	outb(IO_ICU2 + 1,
	     (icu2_mask & ~(npx0_imask >> 8))
	     | (old_icu2_mask & (npx0_imask >> 8)));
#endif
	idt[npx_intrno] = save_idt_npxintr;
	critical_exit(savecrit);		/* back to previous state */

#endif /* SMP */
}

#ifdef I586_CPU
static long
timezero(funcname, func)
	const char *funcname;
	void (*func) __P((void *buf, size_t len));

{
	void *buf;
#define	BUFSIZE		1048576
	long usec;
	struct timeval finish, start;

	buf = malloc(BUFSIZE, M_TEMP, M_NOWAIT);
	if (buf == NULL)
		return (BUFSIZE);
	microtime(&start);
	(*func)(buf, BUFSIZE);
	microtime(&finish);
	usec = 1000000 * (finish.tv_sec - start.tv_sec) +
	    finish.tv_usec - start.tv_usec;
	if (usec <= 0)
		usec = 1;
	if (bootverbose)
		printf("%s bandwidth = %lu kBps\n", funcname,
		    (u_int32_t)(((BUFSIZE >> 10) * 1000000) / usec));
	free(buf, M_TEMP);
	return (usec);
}
#endif /* I586_CPU */

static device_method_t npx_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	npx_identify),
	DEVMETHOD(device_probe,		npx_probe),
	DEVMETHOD(device_attach,	npx_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	
	{ 0, 0 }
};

static driver_t npx_driver = {
	"npx",
	npx_methods,
	1,			/* no softc */
};

static devclass_t npx_devclass;

/*
 * We prefer to attach to the root nexus so that the usual case (exception 16)
 * doesn't describe the processor as being `on isa'.
 */
DRIVER_MODULE(npx, nexus, npx_driver, npx_devclass, 0, 0);

/*
 * This sucks up the legacy ISA support assignments from PNPBIOS.
 */
static struct isa_pnp_id npxisa_ids[] = {
	{ 0x040cd041, "Legacy ISA coprocessor support" }, /* PNP0C04 */
	{ 0 }
};

static int
npxisa_probe(device_t dev)
{
	int result;
	if ((result = ISA_PNP_PROBE(device_get_parent(dev), dev, npxisa_ids)) <= 0) {
		device_quiet(dev);
	}
	return(result);
}

static int
npxisa_attach(device_t dev)
{
	return (0);
}

static device_method_t npxisa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		npxisa_probe),
	DEVMETHOD(device_attach,	npxisa_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	
	{ 0, 0 }
};

static driver_t npxisa_driver = {
	"npxisa",
	npxisa_methods,
	1,			/* no softc */
};

static devclass_t npxisa_devclass;

DRIVER_MODULE(npxisa, isa, npxisa_driver, npxisa_devclass, 0, 0);

