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
 *	@(#)cpu.h	5.4 (Berkeley) 5/9/91
 */

/*
 * Definitions unique to i386 cpu support.
 */
#include "machine/frame.h"
#include "machine/segments.h"

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#undef	COPY_SIGCODE		/* don't copy sigcode above user stack in exec */

/*
 * function vs. inline configuration;
 * these are defined to get generic functions
 * rather than inline or machine-dependent implementations
 */
#define	NEED_MINMAX		/* need {,i,l,ul}{min,max} functions */
#define	NEED_FFS		/* need ffs function */
#define	NEED_BCMP		/* need bcmp function */
#define	NEED_STRLEN		/* need strlen function */

#define	cpu_exec(p)	/* nothing */

/*
 * Arguments to hardclock, softclock and gatherstats
 * encapsulate the previous machine state in an opaque
 * clockframe; for now, use generic intrframe.
 */
typedef struct intrframe clockframe;

#define	CLKF_USERMODE(framep)	(ISPL((framep)->if_cs) == SEL_UPL)
#define	CLKF_BASEPRI(framep)	((framep)->if_ppl == 0)
#define	CLKF_PC(framep)		((framep)->if_eip)

#define	resettodr()	/* no todr to set */

/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
#define	need_resched()	{ want_resched++; aston(); }

/*
 * Give a profiling tick to the current process from the softclock
 * interrupt.  On tahoe, request an ast to send us through trap(),
 * marking the proc as needing a profiling tick.
 */
#define	profile_tick(p, framep)	{ (p)->p_flag |= SOWEUPC; aston(); }

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
#define	signotify(p)	aston()

#define aston() (astpending++)

int	astpending;		/* need to trap before returning to user mode */
int	want_resched;		/* resched() was called */

/*
 * Kinds of processor
 */

#define	CPU_386SX	0
#define	CPU_386		1
#define	CPU_486SX	2
#define	CPU_486		3
#define	CPU_586		4
