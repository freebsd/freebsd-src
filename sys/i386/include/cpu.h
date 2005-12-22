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

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#undef	COPY_SIGCODE		/* don't copy sigcode above user stack in exec */

#define	cpu_exec(p)	/* nothing */
#define	cpu_swapin(p)	/* nothing */
#define	cpu_getstack(td)		((td)->td_frame->tf_esp)
#define	cpu_setstack(td, ap)		((td)->td_frame->tf_esp = (ap))
#define	cpu_spinwait()			ia32_pause()

#define	TRAPF_USERMODE(framep) \
	((ISPL((framep)->tf_cs) == SEL_UPL) || ((framep)->tf_eflags & PSL_VM))
#define	TRAPF_PC(framep)	((framep)->tf_eip)

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
extern u_int	tsc_present;

void	cpu_halt(void);
void	cpu_reset(void);
void	fork_trampoline(void);
void	swi_vm(void *);

/*
 * Return contents of in-cpu fast counter as a sort of "bogo-time"
 * for random-harvesting purposes.
 */
static __inline u_int64_t
get_cyclecount(void)
{
#if defined(I486_CPU) || defined(KLD_MODULE)
	struct bintime bt;

	if (!tsc_present) {
		binuptime(&bt);
		return (bt.frac ^ bt.sec);
	}
#endif
	return (rdtsc());
}

#endif

#endif /* !_MACHINE_CPU_H_ */
