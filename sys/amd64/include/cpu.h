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
 * $FreeBSD$
 */

#ifndef _MACHINE_CPU_H_
#define	_MACHINE_CPU_H_

/*
 * Definitions unique to i386 cpu support.
 */
#include <machine/psl.h>
#include <machine/frame.h>
#include <machine/segments.h>
#include <machine/globals.h>

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#undef	COPY_SIGCODE		/* don't copy sigcode above user stack in exec */

#define	cpu_exec(p)	/* nothing */
#define cpu_swapin(p)	/* nothing */
#define cpu_setstack(p, ap)		((p)->p_md.md_regs[SP] = (ap))

#define	CLKF_USERMODE(framep) \
	((ISPL((framep)->cf_cs) == SEL_UPL) || (framep->cf_eflags & PSL_VM))

#define CLKF_INTR(framep)	(intr_nesting_level >= 2)
#define	CLKF_PC(framep)		((framep)->cf_eip)

/*
 * astpending bits
 */
#define	AST_PENDING	0x00000001
#define	AST_RESCHED	0x00000002

/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 *
 * XXX: if astpending is later changed to an |= here due to more flags being
 * added, we will have an atomicy problem.  The type of atomicy we need is
 * a non-locked orl.
 */
#define	need_resched() do {						\
	PCPU_SET(astpending, AST_RESCHED|AST_PENDING);			\
} while (0)
#define	resched_wanted()	(astpending & AST_RESCHED)

/*
 * Arrange to handle pending profiling ticks before returning to user mode.
 *
 * XXX this is now poorly named and implemented.  It used to handle only a
 * single tick and the P_OWEUPC flag served as a counter.  Now there is a
 * counter in the proc table and flag isn't really necessary.
 */
#define	need_proftick(p) \
	do { (p)->p_flag |= P_OWEUPC; aston(); } while (0)

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 *
 * XXX: aston() really needs to be an atomic (not locked, but an orl),
 * in case need_resched() is set by an interrupt.  But with astpending a
 * per-cpu variable this is not trivial to do efficiently.  For now we blow
 * it off (asynchronous need_resched() conflicts are not critical).
 */
#define	signotify(p)	aston()
#define	aston() do {							\
	PCPU_SET(astpending, astpending | AST_PENDING);			\
} while (0)
#define astoff()

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

#ifdef _KERNEL
extern char	btext[];
extern char	etext[];

void	fork_trampoline __P((void));
void	fork_return __P((struct proc *, struct trapframe));
#endif

#endif /* !_MACHINE_CPU_H_ */
