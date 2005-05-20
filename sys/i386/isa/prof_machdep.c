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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/asmacros.h>
#include <machine/timerreg.h>

/*
 * There are 2 definitions of MCOUNT to have a C version and an asm version
 * with the same name and not have LOCORE #ifdefs to distinguish them.
 * <machine/profile.h> provides a C version, and <machine/asmacros.h>
 * provides an asm version.  To avoid conflicts, #undef the asm version.
 */
#undef MCOUNT

#ifdef GUPROF
#include "opt_i586_guprof.h"
#include "opt_perfmon.h"

#include <sys/gmon.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <machine/clock.h>
#include <machine/perfmon.h>
#include <machine/profile.h>

#define	CPUTIME_CLOCK_UNINITIALIZED	0
#define	CPUTIME_CLOCK_I8254		1
#define	CPUTIME_CLOCK_TSC		2
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

#ifdef __GNUCLIKE_ASM
__asm("								\n\
GM_STATE	=	0					\n\
GMON_PROF_OFF	=	3					\n\
								\n\
	.text							\n\
	.p2align 4,0x90						\n\
	.globl	__mcount					\n\
	.type	__mcount,@function				\n\
__mcount:							\n\
	#							\n\
	# Check that we are profiling.  Do it early for speed.	\n\
	#							\n\
	cmpl	$GMON_PROF_OFF," __XSTRING(CNAME(_gmonparam)) "+GM_STATE \n\
 	je	.mcount_exit					\n\
 	#							\n\
 	# __mcount is the same as [.]mcount except the caller	\n\
 	# hasn't changed the stack except to call here, so the	\n\
	# caller's raddr is above our raddr.			\n\
	#							\n\
 	movl	4(%esp),%edx					\n\
 	jmp	.got_frompc					\n\
 								\n\
 	.p2align 4,0x90						\n\
 	.globl	" __XSTRING(HIDENAME(mcount)) "			\n\
" __XSTRING(HIDENAME(mcount)) ":				\n\
 	.globl	__cyg_profile_func_enter			\n\
__cyg_profile_func_enter:					\n\
	cmpl	$GMON_PROF_OFF," __XSTRING(CNAME(_gmonparam)) "+GM_STATE \n\
	je	.mcount_exit					\n\
	#							\n\
	# The caller's stack frame has already been built, so	\n\
	# %ebp is the caller's frame pointer.  The caller's	\n\
	# raddr is in the caller's frame following the caller's	\n\
	# caller's frame pointer.				\n\
	#							\n\
	movl	4(%ebp),%edx					\n\
.got_frompc:							\n\
	#							\n\
	# Our raddr is the caller's pc.				\n\
	#							\n\
	movl	(%esp),%eax					\n\
								\n\
	pushfl							\n\
	pushl	%eax						\n\
	pushl	%edx						\n\
	cli							\n\
	call	" __XSTRING(CNAME(mcount)) "			\n\
	addl	$8,%esp						\n\
	popfl							\n\
.mcount_exit:							\n\
	ret							\n\
");
#else /* !__GNUCLIKE_ASM */
#error this file needs to be ported to your compiler
#endif /* __GNUCLIKE_ASM */

#ifdef GUPROF
/*
 * [.]mexitcount saves the return register(s), loads selfpc and calls
 * mexitcount(selfpc) to do the work.  Someday it should be in a machine
 * dependent file together with cputime(), __mcount and [.]mcount.  cputime()
 * can't just be put in machdep.c because it has to be compiled without -pg.
 */
#ifdef __GNUCLIKE_ASM
__asm("								\n\
	.text							\n\
#								\n\
# Dummy label to be seen when gprof -u hides [.]mexitcount.	\n\
#								\n\
	.p2align 4,0x90						\n\
	.globl	__mexitcount					\n\
	.type	__mexitcount,@function				\n\
__mexitcount:							\n\
	nop							\n\
								\n\
GMON_PROF_HIRES	=	4					\n\
								\n\
	.p2align 4,0x90						\n\
	.globl	" __XSTRING(HIDENAME(mexitcount)) "		\n\
" __XSTRING(HIDENAME(mexitcount)) ":				\n\
 	.globl	__cyg_profile_func_exit				\n\
__cyg_profile_func_exit:					\n\
	cmpl	$GMON_PROF_HIRES," __XSTRING(CNAME(_gmonparam)) "+GM_STATE \n\
	jne	.mexitcount_exit				\n\
	pushl	%edx						\n\
	pushl	%eax						\n\
	movl	8(%esp),%eax					\n\
	pushfl							\n\
	pushl	%eax						\n\
	cli							\n\
	call	" __XSTRING(CNAME(mexitcount)) "		\n\
	addl	$4,%esp						\n\
	popfl							\n\
	popl	%eax						\n\
	popl	%edx						\n\
.mexitcount_exit:						\n\
	ret							\n\
");
#endif /* __GNUCLIKE_ASM */

/*
 * Return the time elapsed since the last call.  The units are machine-
 * dependent.
 */
int
cputime()
{
	u_int count;
	int delta;
#if (defined(I586_CPU) || defined(I686_CPU)) && !defined(SMP) && \
    defined(PERFMON) && defined(I586_PMC_GUPROF)
	u_quad_t event_count;
#endif
	u_char high, low;
	static u_int prev_count;

#if (defined(I586_CPU) || defined(I686_CPU)) && !defined(SMP)
	if (cputime_clock == CPUTIME_CLOCK_TSC) {
		/*
		 * Scale the TSC a little to make cputime()'s frequency
		 * fit in an int, assuming that the TSC frequency fits
		 * in a u_int.  Use a fixed scale since dynamic scaling
		 * would be slower and we can't really use the low bit
		 * of precision.
		 */
		count = (u_int)rdtsc() & ~1u;
		delta = (int)(count - prev_count) >> 1;
		prev_count = count;
		return (delta);
	}
#if defined(PERFMON) && defined(I586_PMC_GUPROF)
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
#endif /* PERFMON && I586_PMC_GUPROF */
#endif /* (I586_CPU || I686_CPU) && !SMP */

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

static int
sysctl_machdep_cputime_clock(SYSCTL_HANDLER_ARGS)
{
	int clock;
	int error;
#if defined(PERFMON) && defined(I586_PMC_GUPROF)
	int event;
	struct pmc pmc;
#endif

	clock = cputime_clock;
#if defined(PERFMON) && defined(I586_PMC_GUPROF)
	if (clock == CPUTIME_CLOCK_I586_PMC) {
		pmc.pmc_val = cputime_clock_pmc_conf;
		clock += pmc.pmc_event;
	}
#endif
	error = sysctl_handle_opaque(oidp, &clock, sizeof clock, req);
	if (error == 0 && req->newptr != NULL) {
#if defined(PERFMON) && defined(I586_PMC_GUPROF)
		if (clock >= CPUTIME_CLOCK_I586_PMC) {
			event = clock - CPUTIME_CLOCK_I586_PMC;
			if (event >= 256)
				return (EINVAL);
			pmc.pmc_num = 0;
			pmc.pmc_event = event;
			pmc.pmc_unit = 0;
			pmc.pmc_flags = PMCF_E | PMCF_OS | PMCF_USR;
			pmc.pmc_mask = 0;
			cputime_clock_pmc_conf = pmc.pmc_val;
			cputime_clock = CPUTIME_CLOCK_I586_PMC;
		} else
#endif
		{
			if (clock < 0 || clock >= CPUTIME_CLOCK_I586_PMC)
				return (EINVAL);
			cputime_clock = clock;
		}
	}
	return (error);
}

SYSCTL_PROC(_machdep, OID_AUTO, cputime_clock, CTLTYPE_INT | CTLFLAG_RW,
	    0, sizeof(u_int), sysctl_machdep_cputime_clock, "I", "");

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
#if (defined(I586_CPU) || defined(I686_CPU)) && !defined(SMP)
		if (tsc_freq != 0)
			cputime_clock = CPUTIME_CLOCK_TSC;
#endif
	}
	gp->profrate = timer_freq << CPUTIME_CLOCK_I8254_SHIFT;
#if (defined(I586_CPU) || defined(I686_CPU)) && !defined(SMP)
	if (cputime_clock == CPUTIME_CLOCK_TSC)
		gp->profrate = tsc_freq >> 1;
#if defined(PERFMON) && defined(I586_PMC_GUPROF)
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
#endif /* PERFMON && I586_PMC_GUPROF */
#endif /* (I586_CPU || I686_CPU) && !SMP */
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
#ifdef __GNUCLIKE_ASM
__asm("								\n\
	.text							\n\
	.p2align 4,0x90						\n\
	.globl	" __XSTRING(HIDENAME(mexitcount)) "		\n\
" __XSTRING(HIDENAME(mexitcount)) ":				\n\
	ret							\n\
");
#endif /* __GNUCLIKE_ASM */
#endif /* GUPROF */
