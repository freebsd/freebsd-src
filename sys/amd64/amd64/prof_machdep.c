/*-
 * Copyright (c) 1996 Bruce D. Evans.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: prof_machdep.c,v 1.3 1996/10/17 19:32:10 bde Exp $
 */

#ifdef GUPROF
#include "opt_cpu.h"
#include "opt_i586_guprof.h"
#include "opt_perfmon.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/gmon.h>

#include <machine/clock.h>
#include <machine/perfmon.h>
#include <machine/profile.h>
#endif

#ifdef PC98
#include <pc98/pc98/pc98.h>
#else
#include <i386/isa/isa.h>
#endif
#include <i386/isa/timerreg.h>

#ifdef GUPROF
#define	CPUTIME_CLOCK_UNINITIALIZED	0
#define	CPUTIME_CLOCK_I8254		1
#define	CPUTIME_CLOCK_I586_CTR		2
#define	CPUTIME_CLOCK_I586_PMC		3
#define	CPUTIME_CLOCK_I8254_SHIFT	7

int	cputime_bias = 1;	/* initialize for locality of reference */

static int	cputime_clock = CPUTIME_CLOCK_UNINITIALIZED;
#ifdef I586_PMC_GUPROF
static u_int	cputime_clock_pmc_conf = I586_PMC_GUPROF;
static int	cputime_clock_pmc_init;
static struct gmonparam saved_gmp;
#endif
#endif /* GUPROF */

#ifdef __GNUC__
asm("
GM_STATE	=	0
GMON_PROF_OFF	=	3

	.text
	.align	4,0x90
	.globl	__mcount
__mcount:
	#
	# Check that we are profiling.  Do it early for speed.
	#
	cmpl	$GMON_PROF_OFF,__gmonparam+GM_STATE
 	je	Lmcount_exit
 	#
 	# __mcount is the same as mcount except the caller hasn't changed
 	# the stack except to call here, so the caller's raddr is above
 	# our raddr.
 	#
 	movl	4(%esp),%edx
 	jmp	Lgot_frompc
 
 	.align	4,0x90
 	.globl	mcount
mcount:
	cmpl	$GMON_PROF_OFF,__gmonparam+GM_STATE
	je	Lmcount_exit
	#
	# The caller's stack frame has already been built, so %ebp is
	# the caller's frame pointer.  The caller's raddr is in the
	# caller's frame following the caller's caller's frame pointer.
	#
	movl	4(%ebp),%edx
Lgot_frompc:
	#
	# Our raddr is the caller's pc.
	#
	movl	(%esp),%eax

	pushfl
	pushl	%eax
	pushl	%edx
	cli
	call	_mcount
	addl	$8,%esp
	popfl
Lmcount_exit:
	ret
");
#else /* !__GNUC__ */
#error
#endif /* __GNUC__ */

#ifdef GUPROF
/*
 * mexitcount saves the return register(s), loads selfpc and calls
 * mexitcount(selfpc) to do the work.  Someday it should be in a machine
 * dependent file together with cputime(), __mcount and mcount.  cputime()
 * can't just be put in machdep.c because it has to be compiled without -pg.
 */
#ifdef __GNUC__
asm("
	.text
#
# Dummy label to be seen when gprof -u hides mexitcount.
#
	.align	4,0x90
	.globl	__mexitcount
__mexitcount:
	nop

GMON_PROF_HIRES	=	4

	.align	4,0x90
	.globl	mexitcount
mexitcount:
	cmpl	$GMON_PROF_HIRES,__gmonparam+GM_STATE
	jne	Lmexitcount_exit
	pushl	%edx
	pushl	%eax
	movl	8(%esp),%eax
	pushfl
	pushl	%eax
	cli
	call	_mexitcount
	addl	$4,%esp
	popfl
	popl	%eax
	popl	%edx
Lmexitcount_exit:
	ret
");
#else /* !__GNUC__ */
#error
#endif /* __GNUC__ */

/*
 * Return the time elapsed since the last call.  The units are machine-
 * dependent.
 */
int
cputime()
{
	u_int count;
	int delta;
#ifdef I586_PMC_GUPROF
	u_quad_t event_count;
#endif
	u_char high, low;
	static u_int prev_count;

#if defined(I586_CPU) || defined(I686_CPU)
	if (cputime_clock == CPUTIME_CLOCK_I586_CTR) {
		count = (u_int)rdtsc();
		delta = (int)(count - prev_count);
		prev_count = count;
		return (delta);
	}
#ifdef I586_PMC_GUPROF
	if (cputime_clock == CPUTIME_CLOCK_I586_PMC) {
		/*
		 * XXX permon_read() should be inlined so that the
		 * perfmon module doesn't need to be compiled with
		 * profiling disabled and so that it is fast.
		 */
		perfmon_read(0, &event_count);

		count = (u_int)event_count;
		delta = (int)(count - prev_count);
		prev_count = count;
		return (delta);
	}
#endif /* I586_PMC_GUPROF */
#endif /* I586_CPU or I686_CPU */

	/*
	 * Read the current value of the 8254 timer counter 0.
	 */
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_LATCH);
	low = inb(TIMER_CNTR0);
	high = inb(TIMER_CNTR0);
	count = ((high << 8) | low) << CPUTIME_CLOCK_I8254_SHIFT;

	/*
	 * The timer counts down from TIMER_CNTR0_MAX to 0 and then resets.
	 * While profiling is enabled, this routine is called at least twice
	 * per timer reset (for mcounting and mexitcounting hardclock()),
	 * so at most one reset has occurred since the last call, and one
	 * has occurred iff the current count is larger than the previous
	 * count.  This allows counter underflow to be detected faster
	 * than in microtime().
	 */
	delta = prev_count - count;
	prev_count = count;
	if ((int) delta <= 0)
		return (delta + (timer0_max_count << CPUTIME_CLOCK_I8254_SHIFT));
	return (delta);
}

/*
 * The start and stop routines need not be here since we turn off profiling
 * before calling them.  They are here for convenience.
 */

void
startguprof(gp)
	struct gmonparam *gp;
{
	if (cputime_clock == CPUTIME_CLOCK_UNINITIALIZED) {
		cputime_clock = CPUTIME_CLOCK_I8254;
#if defined(I586_CPU) || defined(I686_CPU)
		if (i586_ctr_freq != 0)
			cputime_clock = CPUTIME_CLOCK_I586_CTR;
#endif
	}
	gp->profrate = timer_freq << CPUTIME_CLOCK_I8254_SHIFT;
#if defined(I586_CPU) || defined(I686_CPU)
	if (cputime_clock == CPUTIME_CLOCK_I586_CTR)
		gp->profrate = i586_ctr_freq;
#ifdef I586_PMC_GUPROF
	else if (cputime_clock == CPUTIME_CLOCK_I586_PMC) {
		if (perfmon_avail() &&
		    perfmon_setup(0, cputime_clock_pmc_conf) == 0) {
			if (perfmon_start(0) != 0)
				perfmon_fini(0);
			else {
				/* XXX 1 event == 1 us. */
				gp->profrate = 1000000;

				saved_gmp = *gp;

				/* Zap overheads.  They are invalid. */
				gp->cputime_overhead = 0;
				gp->mcount_overhead = 0;
				gp->mcount_post_overhead = 0;
				gp->mcount_pre_overhead = 0;
				gp->mexitcount_overhead = 0;
				gp->mexitcount_post_overhead = 0;
				gp->mexitcount_pre_overhead = 0;

				cputime_clock_pmc_init = TRUE;
			}
		}
	}
#endif /* I586_PMC_GUPROF */
#endif /* I586_CPU or I686_CPU */
	cputime_bias = 0;
	cputime();
}

void
stopguprof(gp)
	struct gmonparam *gp;
{
#if defined(PERFMON) && defined(I586_PMC_GUPROF)
	if (cputime_clock_pmc_init) {
		*gp = saved_gmp;
		perfmon_fini(0);
		cputime_clock_pmc_init = FALSE;
	}
#endif
}

#else /* !GUPROF */
#ifdef __GNUC__
asm("
	.text
	.align	4,0x90
	.globl	mexitcount
mexitcount:
	ret
");
#else /* !__GNUC__ */
#error
#endif /* __GNUC__ */
#endif /* GUPROF */
