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

#include "opt_cpu.h"
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
#include <sys/user.h>

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
#ifdef PC98
#include <pc98/pc98/pc98.h>
#else
#include <i386/isa/isa.h>
#endif
#endif
#include <i386/isa/intr_machdep.h>
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
#define	fnsave(addr)		__asm __volatile("fnsave %0" : "=m" (*(addr)))
#define	fnstcw(addr)		__asm __volatile("fnstcw %0" : "=m" (*(addr)))
#define	fnstsw(addr)		__asm __volatile("fnstsw %0" : "=m" (*(addr)))
#define	fp_divide_by_0()	__asm("fldz; fld1; fdiv %st,%st(1); fnop")
#define	frstor(addr)		__asm("frstor %0" : : "m" (*(addr)))
#ifdef CPU_ENABLE_SSE
#define	fxrstor(addr)		__asm("fxrstor %0" : : "m" (*(addr)))
#define	fxsave(addr)		__asm __volatile("fxsave %0" : "=m" (*(addr)))
#endif
#define	start_emulating()	__asm("smsw %%ax; orb %0,%%al; lmsw %%ax" \
				      : : "n" (CR0_TS) : "ax")
#define	stop_emulating()	__asm("clts")

#else	/* not __GNUC__ */

void	fldcw		__P((caddr_t addr));
void	fnclex		__P((void));
void	fninit		__P((void));
void	fnsave		__P((caddr_t addr));
void	fnstcw		__P((caddr_t addr));
void	fnstsw		__P((caddr_t addr));
void	fp_divide_by_0	__P((void));
void	frstor		__P((caddr_t addr));
#ifdef CPU_ENABLE_SSE
void	fxsave		__P((caddr_t addr));
void	fxrstor		__P((caddr_t addr));
#endif
void	start_emulating	__P((void));
void	stop_emulating	__P((void));

#endif	/* __GNUC__ */

#ifdef CPU_ENABLE_SSE
#define GET_FPU_CW(thread) \
	(cpu_fxsr ? \
		(thread)->td_pcb->pcb_save.sv_xmm.sv_env.en_cw : \
		(thread)->td_pcb->pcb_save.sv_87.sv_env.en_cw)
#define GET_FPU_SW(thread) \
	(cpu_fxsr ? \
		(thread)->td_pcb->pcb_save.sv_xmm.sv_env.en_sw : \
		(thread)->td_pcb->pcb_save.sv_87.sv_env.en_sw)
#define GET_FPU_EXSW_PTR(pcb) \
	(cpu_fxsr ? \
		&(pcb)->pcb_save.sv_xmm.sv_ex_sw : \
		&(pcb)->pcb_save.sv_87.sv_ex_sw)
#else /* CPU_ENABLE_SSE */
#define GET_FPU_CW(thread) \
	(thread->td_pcb->pcb_save.sv_87.sv_env.en_cw)
#define GET_FPU_SW(thread) \
	(thread->td_pcb->pcb_save.sv_87.sv_env.en_sw)
#define GET_FPU_EXSW_PTR(pcb) \
	(&(pcb)->pcb_save.sv_87.sv_ex_sw)
#endif /* CPU_ENABLE_SSE */

typedef u_char bool_t;

static	int	npx_attach	__P((device_t dev));
static	void	npx_identify	__P((driver_t *driver, device_t parent));
#ifndef SMP
static	void	npx_intr	__P((void *));
#endif
static	int	npx_probe	__P((device_t dev));
static	void	fpusave		__P((union savefpu *));
static	void	fpurstor	__P((union savefpu *));
#ifdef I586_CPU_XXX
static	long	timezero	__P((const char *funcname,
				     void (*func)(void *buf, size_t len)));
#endif /* I586_CPU */

int	hw_float;		/* XXX currently just alias for npx_exists */

SYSCTL_INT(_hw,HW_FLOATINGPT, floatingpoint,
	CTLFLAG_RD, &hw_float, 0, 
	"Floatingpoint instructions executed in hardware");

#ifndef SMP
static	volatile u_int		npx_intrs_while_probing;
static	volatile u_int		npx_traps_while_probing;
#endif

static	bool_t			npx_ex16;
static	bool_t			npx_exists;
static	bool_t			npx_irq13;

#ifndef SMP
alias_for_inthand_t probetrap;
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

#ifndef SMP
/*
 * Do minimal handling of npx interrupts to convert them to traps.
 */
static void
npx_intr(dummy)
	void *dummy;
{
	struct thread *td;

#ifndef SMP
	npx_intrs_while_probing++;
#endif

	/*
	 * The BUSY# latch must be cleared in all cases so that the next
	 * unmasked npx exception causes an interrupt.
	 */
#ifdef PC98
	outb(0xf8, 0);
#else
	outb(0xf0, 0);
#endif

	/*
	 * fpcurthread is normally non-null here.  In that case, schedule an
	 * AST to finish the exception handling in the correct context
	 * (this interrupt may occur after the thread has entered the
	 * kernel via a syscall or an interrupt).  Otherwise, the npx
	 * state of the thread that caused this interrupt must have been
	 * pushed to the thread's pcb, and clearing of the busy latch
	 * above has finished the (essentially null) handling of this
	 * interrupt.  Control will eventually return to the instruction
	 * that caused it and it will repeat.  We will eventually (usually
	 * soon) win the race to handle the interrupt properly.
	 */
	td = PCPU_GET(fpcurthread);
	if (td != NULL) {
		td->td_pcb->pcb_flags |= PCB_NPXTRAP;
		mtx_lock_spin(&sched_lock);
		td->td_kse->ke_flags |= KEF_ASTPENDING;
		mtx_unlock_spin(&sched_lock);
	}
}
#endif /* !SMP */

/*
 * Probe routine.  Initialize cr0 to give correct behaviour for [f]wait
 * whether the device exists or not (XXX should be elsewhere).  Set flags
 * to tell npxattach() what to do.  Modify device struct if npx doesn't
 * need to use interrupts.  Return 0 if device exists.
 */
static int
npx_probe(dev)
	device_t dev;
{
#ifndef SMP
	struct gate_descriptor save_idt_npxtrap;
	struct resource *ioport_res, *irq_res;
	void *irq_cookie;
	int ioport_rid, irq_num, irq_rid;
	u_short control;
	u_short status;

	save_idt_npxtrap = idt[16];
	setidt(16, probetrap, SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	ioport_rid = 0;
	ioport_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &ioport_rid,
	    IO_NPX, IO_NPX, IO_NPXSIZE, RF_ACTIVE);
	if (ioport_res == NULL)
		panic("npx: can't get ports");
#ifdef PC98
	if (resource_int_value("npx", 0, "irq", &irq_num) != 0)
		irq_num = 8;
#else
	if (resource_int_value("npx", 0, "irq", &irq_num) != 0)
		irq_num = 13;
#endif
	irq_rid = 0;
	irq_res = bus_alloc_resource(dev, SYS_RES_IRQ, &ioport_rid, irq_num,
	    irq_num, 1, RF_ACTIVE);
	if (irq_res == NULL)
		panic("npx: can't get IRQ");
	if (bus_setup_intr(dev, irq_res, INTR_TYPE_MISC | INTR_FAST, npx_intr,
	    NULL, &irq_cookie) != 0)
		panic("npx: can't create intr");
#endif /* !SMP */

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
	 * pending, then we may get a bogus IRQ13, but npx_intr() will handle
	 * it OK.  Bogus halts have never been observed, but we enabled
	 * IRQ13 and cleared the BUSY# latch early to handle them anyway.
	 */
	fninit();

	device_set_desc(dev, "math processor");

#ifdef SMP

	/*
	 * Exception 16 MUST work for SMP.
	 */
	npx_ex16 = hw_float = npx_exists = 1;
	return (0);

#else /* !SMP */

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
				goto no_irq13;
			}
			if (npx_intrs_while_probing != 0) {
				/*
				 * Bad, we are stuck with IRQ13.
				 */
				npx_irq13 = 1;
				idt[16] = save_idt_npxtrap;
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
	/* FALLTHROUGH */
no_irq13:
	idt[16] = save_idt_npxtrap;
	bus_teardown_intr(dev, irq_res, irq_cookie);

	/*
	 * XXX hack around brokenness of bus_teardown_intr().  If we left the
	 * irq active then we would get it instead of exception 16.
	 */
	mtx_lock_spin(&icu_lock);
	INTRDIS(1 << irq_num);
	mtx_unlock_spin(&icu_lock);

	bus_release_resource(dev, SYS_RES_IRQ, irq_rid, irq_res);
	bus_release_resource(dev, SYS_RES_IOPORT, ioport_rid, ioport_res);
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
	static union savefpu dummy;
	critical_t savecrit;

	if (!npx_exists)
		return;
	/*
	 * fninit has the same h/w bugs as fnsave.  Use the detoxified
	 * fnsave to throw away any junk in the fpu.  npxsave() initializes
	 * the fpu and sets fpcurthread = NULL as important side effects.
	 */
	savecrit = cpu_critical_enter();
	npxsave(&dummy);
	stop_emulating();
#ifdef CPU_ENABLE_SSE
	/* XXX npxsave() doesn't actually initialize the fpu in the SSE case. */
	if (cpu_fxsr)
		fninit();
#endif
	fldcw(&control);
	if (PCPU_GET(curpcb) != NULL)
		fpusave(&PCPU_GET(curpcb)->pcb_save);
	start_emulating();
	cpu_critical_exit(savecrit);
}

/*
 * Free coprocessor (if we have it).
 */
void
npxexit(td)
	struct thread *td;
{
	critical_t savecrit;

	savecrit = cpu_critical_enter();
	if (td == PCPU_GET(fpcurthread))
		npxsave(&PCPU_GET(curpcb)->pcb_save);
	cpu_critical_exit(savecrit);
#ifdef NPX_DEBUG
	if (npx_exists) {
		u_int	masked_exceptions;

		masked_exceptions = PCPU_GET(curpcb)->pcb_save.sv_87.sv_env.en_cw
		    & PCPU_GET(curpcb)->pcb_save.sv_87.sv_env.en_sw & 0x7f;
		/*
		 * Log exceptions that would have trapped with the old
		 * control word (overflow, divide by 0, and invalid operand).
		 */
		if (masked_exceptions & 0x0d)
			log(LOG_ERR,
	"pid %d (%s) exited with masked floating point exceptions 0x%02x\n",
			    td->td_proc->p_pid, td->td_proc->p_comm,
			    masked_exceptions);
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
int
npxtrap()
{
	critical_t savecrit;
	u_short control, status;
	u_long *exstat;

	if (!npx_exists) {
		printf("npxtrap: fpcurthread = %p, curthread = %p, npx_exists = %d\n",
		       PCPU_GET(fpcurthread), curthread, npx_exists);
		panic("npxtrap from nowhere");
	}
	savecrit = cpu_critical_enter();

	/*
	 * Interrupt handling (for another interrupt) may have pushed the
	 * state to memory.  Fetch the relevant parts of the state from
	 * wherever they are.
	 */
	if (PCPU_GET(fpcurthread) != curthread) {
		control = GET_FPU_CW(curthread);
		status = GET_FPU_SW(curthread);
	} else {
		fnstcw(&control);
		fnstsw(&status);
	}

	exstat = GET_FPU_EXSW_PTR(curthread->td_pcb);
	*exstat = status;
	if (PCPU_GET(fpcurthread) != curthread)
		GET_FPU_SW(curthread) &= ~0x80bf;
	else
		fnclex();
	cpu_critical_exit(savecrit);
	return (fpetable[status & ((~control & 0x3f) | 0x40)]);
}

/*
 * Implement device not available (DNA) exception
 *
 * It would be better to switch FP context here (if curthread != fpcurthread)
 * and not necessarily for every context switch, but it is too hard to
 * access foreign pcb's.
 */
int
npxdna()
{
	u_long *exstat;
	critical_t s;

	if (!npx_exists)
		return (0);
	if (PCPU_GET(fpcurthread) != NULL) {
		printf("npxdna: fpcurthread = %p, curthread = %p\n",
		       PCPU_GET(fpcurthread), curthread);
		panic("npxdna");
	}
	s = cpu_critical_enter();
	stop_emulating();
	/*
	 * Record new context early in case frstor causes an IRQ13.
	 */
	PCPU_SET(fpcurthread, curthread);

	exstat = GET_FPU_EXSW_PTR(PCPU_GET(curpcb));
	*exstat = 0;
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
	fpurstor(&PCPU_GET(curpcb)->pcb_save);
	cpu_critical_exit(s);

	return (1);
}

/*
 * Wrapper for fnsave instruction, partly to handle hardware bugs.  When npx
 * exceptions are reported via IRQ13, spurious IRQ13's may be triggered by
 * no-wait npx instructions.  See the Intel application note AP-578 for
 * details.  This doesn't cause any additional complications here.  IRQ13's
 * are inherently asynchronous unless the CPU is frozen to deliver them --
 * one that started in userland may be delivered many instructions later,
 * after the process has entered the kernel.  It may even be delivered after
 * the fnsave here completes.  A spurious IRQ13 for the fnsave is handled in
 * the same way as a very-late-arriving non-spurious IRQ13 from user mode:
 * it is normally ignored at first because we set fpcurthread to NULL; it is
 * normally retriggered in npxdna() after return to user mode.
 *
 * npxsave() must be called with interrupts disabled, so that it clears
 * fpcurthread atomically with saving the state.  We require callers to do the
 * disabling, since most callers need to disable interrupts anyway to call
 * npxsave() atomically with checking fpcurthread.
 *
 * A previous version of npxsave() went to great lengths to excecute fnsave
 * with interrupts enabled in case executing it froze the CPU.  This case
 * can't happen, at least for Intel CPU/NPX's.  Spurious IRQ13's don't imply
 * spurious freezes.
 */
void
npxsave(addr)
	union savefpu *addr;
{

	stop_emulating();
	fpusave(addr);

	start_emulating();
	PCPU_SET(fpcurthread, NULL);
}

static void
fpusave(addr)
	union savefpu *addr;
{
	
#ifdef CPU_ENABLE_SSE
	if (cpu_fxsr)
		fxsave(addr);
	else
#endif
		fnsave(addr);
}

static void
fpurstor(addr)
	union savefpu *addr;
{

#ifdef CPU_ENABLE_SSE
	if (cpu_fxsr)
		fxrstor(addr);
	else
#endif
		frstor(addr);
}

#ifdef I586_CPU_XXX
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
		printf("%s bandwidth = %u kBps\n", funcname,
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
 * This sucks up the legacy ISA support assignments from PNPBIOS/ACPI.
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
#ifndef PC98
DRIVER_MODULE(npxisa, acpi, npxisa_driver, npxisa_devclass, 0, 0);
#endif

