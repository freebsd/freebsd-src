/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	from: @(#)cpu.h	5.4 (Berkeley) 5/9/91
 *	$FreeBSD$
 */

#ifndef _MACHINE_CPU_H_
#define	_MACHINE_CPU_H_

/*
 * Definitions unique to i386 cpu support.
 */
#include <machine/frame.h>
#include <machine/segments.h>

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#undef	COPY_SIGCODE		/* don't copy sigcode above user stack in exec */

#define	cpu_exec(p)	/* nothing */
#define cpu_swapin(p)	/* nothing */
#define cpu_setstack(p, ap)		((p)->p_md.md_regs[SP] = (ap))
#define cpu_set_init_frame(p, fp)	((p)->p_md.md_regs = (fp))

#define	CLKF_USERMODE(framep)	(ISPL((framep)->cf_cs) == SEL_UPL)
#define CLKF_INTR(framep)	(intr_nesting_level >= 2)
#if 0
/*
 * XXX splsoftclock() is very broken and barely worth fixing.  It doesn't
 * turn off the clock bit in imen or in the icu.  (This is not a serious
 * problem at 100 Hz but it is serious at 16000 Hz for pcaudio.  softclock()
 * can take more than 62.5 usec so clock interrupts are lost.)  It doesn't
 * check for pending interrupts being unmasked.  clkintr() and Xintr0()
 * assume that the ipl is high when hardclock() returns.  Our SWI_AST
 * handling is efficient enough that little is gained by calling
 * softclock() directly.
 */
#define	CLKF_BASEPRI(framep)	(((framep)->cf_ppl & ~SWI_AST_MASK) == 0)
#else
#define	CLKF_BASEPRI(framep)	(0)
#endif
#define	CLKF_PC(framep)		((framep)->cf_eip)

/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
#define	need_resched()	{ want_resched = 1; aston(); }

/*
 * Arrange to handle pending profiling ticks before returning to user mode.
 *
 * XXX this is now poorly named and implemented.  It used to handle only a
 * single tick and the P_OWEUPC flag served as a counter.  Now there is a
 * counter in the proc table and flag isn't really necessary.
 */
#define	need_proftick(p)	{ (p)->p_flag |= P_OWEUPC; aston(); }

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
#define	signotify(p)	aston()

#define aston()	setsoftast()
#define astoff()

/*
 * pull in #defines for kinds of processors
 */
#include <machine/cputypes.h>

struct cpu_nameclass {
	char *cpu_name;
	int  cpu_class;
};

/*
 * CTL_MACHDEP definitions.
 */
#define CPU_CONSDEV		1	/* dev_t: console terminal device */
#define	CPU_ADJKERNTZ		2	/* int:	timezone offset	(seconds) */
#define	CPU_DISRTCSET		3	/* int: disable resettodr() call */
#define CPU_BOOTINFO		4	/* struct: bootinfo */
#define	CPU_WALLCLOCK		5	/* int:	indicates wall CMOS clock */
#define	CPU_MAXID		6	/* number of valid machdep ids */

#define CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "console_device", CTLTYPE_STRUCT }, \
	{ "adjkerntz", CTLTYPE_INT }, \
	{ "disable_rtc_set", CTLTYPE_INT }, \
	{ "bootinfo", CTLTYPE_STRUCT }, \
	{ "wall_cmos_clock", CTLTYPE_INT }, \
}

#ifdef KERNEL
extern int	cpu;
extern int	cpu_class;
extern u_char	intr_nesting_level;
extern struct cpu_nameclass i386_cpus[];
extern int	want_resched;	/* resched was called */
#endif

#endif /* !_MACHINE_CPU_H_ */
