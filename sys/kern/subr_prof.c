/*-
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)subr_prof.c	8.3 (Berkeley) 9/23/93
 * $Id: subr_prof.c,v 1.15 1995/12/26 01:21:39 bde Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sysctl.h>

#include <machine/cpu.h>

#ifdef GPROF
#include <sys/malloc.h>
#include <sys/gmon.h>

static void kmstartup __P((void *));
SYSINIT(kmem, SI_SUB_KPROF, SI_ORDER_FIRST, kmstartup, NULL)

struct gmonparam _gmonparam = { GMON_PROF_OFF };

extern char btext[];
extern char etext[];

static void
kmstartup(dummy)
	void *dummy;
{
	char *cp;
	struct gmonparam *p = &_gmonparam;
#ifdef GUPROF
	fptrint_t kmstartup_addr;
	int i;
#endif

	/*
	 * Round lowpc and highpc to multiples of the density we're using
	 * so the rest of the scaling (here and in gprof) stays in ints.
	 */
	p->lowpc = ROUNDDOWN((u_long)btext, HISTFRACTION * sizeof(HISTCOUNTER));
	p->highpc = ROUNDUP((u_long)etext, HISTFRACTION * sizeof(HISTCOUNTER));
	p->textsize = p->highpc - p->lowpc;
	printf("Profiling kernel, textsize=%d [%x..%x]\n",
	       p->textsize, p->lowpc, p->highpc);
	p->kcountsize = p->textsize / HISTFRACTION;
	p->hashfraction = HASHFRACTION;
	p->fromssize = p->textsize / HASHFRACTION;
	p->tolimit = p->textsize * ARCDENSITY / 100;
	if (p->tolimit < MINARCS)
		p->tolimit = MINARCS;
	else if (p->tolimit > MAXARCS)
		p->tolimit = MAXARCS;
	p->tossize = p->tolimit * sizeof(struct tostruct);
	cp = (char *)malloc(p->kcountsize + p->fromssize + p->tossize,
	    M_GPROF, M_NOWAIT);
	if (cp == 0) {
		printf("No memory for profiling.\n");
		return;
	}
	bzero(cp, p->kcountsize + p->tossize + p->fromssize);
	p->tos = (struct tostruct *)cp;
	cp += p->tossize;
	p->kcount = (HISTCOUNTER *)cp;
	cp += p->kcountsize;
	p->froms = (u_short *)cp;

#ifdef GUPROF
	/*
	 * Initialize pointers to overhead counters.
	 */
	p->cputime_count = &KCOUNT(p, PC_TO_I(p, cputime));
	p->mcount_count = &KCOUNT(p, PC_TO_I(p, mcount));
	p->mexitcount_count = &KCOUNT(p, PC_TO_I(p, mexitcount));

	/*
	 * Determine overheads.
	 */
	disable_intr();
	p->state = GMON_PROF_HIRES;

	p->cputime_overhead = 0;
	(void)cputime();
	for (i = 0; i < CALIB_SCALE; i++)
		p->cputime_overhead += cputime();

	(void)cputime();
	for (i = 0; i < CALIB_SCALE; i++)
#if defined(i386) && __GNUC__ >= 2
		/*
		 * Underestimate slightly by always calling __mcount, never
		 * mcount.
		 */
		asm("pushl %0; call __mcount; popl %%ecx"
		    :
		    : "i" (kmstartup)
		    : "ax", "bx", "cx", "dx", "memory");
#else
#error
#endif
	p->mcount_overhead = KCOUNT(p, PC_TO_I(p, kmstartup));

	(void)cputime();
	for (i = 0; i < CALIB_SCALE; i++)
#if defined(i386) && __GNUC__ >= 2
		    asm("call mexitcount; 1:"
			: : : "ax", "bx", "cx", "dx", "memory");
	asm("movl $1b,%0" : "=rm" (kmstartup_addr));
#else
#error
#endif
	p->mexitcount_overhead = KCOUNT(p, PC_TO_I(p, kmstartup_addr));

	p->state = GMON_PROF_OFF;
	enable_intr();

	p->mcount_overhead_sub = p->mcount_overhead - p->cputime_overhead;
	p->mexitcount_overhead_sub = p->mexitcount_overhead
				     - p->cputime_overhead;
	printf("Profiling overheads: %u+%u %u+%u\n",
		p->cputime_overhead, p->mcount_overhead_sub,
		p->cputime_overhead, p->mexitcount_overhead_sub);
	p->cputime_overhead_frac = p->cputime_overhead % CALIB_SCALE;
	p->cputime_overhead /= CALIB_SCALE;
	p->mcount_overhead_frac = p->mcount_overhead_sub % CALIB_SCALE;
	p->mcount_overhead_sub /= CALIB_SCALE;
	p->mcount_overhead /= CALIB_SCALE;
	p->mexitcount_overhead_frac = p->mexitcount_overhead_sub % CALIB_SCALE;
	p->mexitcount_overhead_sub /= CALIB_SCALE;
	p->mexitcount_overhead /= CALIB_SCALE;
#endif /* GUPROF */
}

/*
 * Return kernel profiling information.
 */
static int
sysctl_kern_prof SYSCTL_HANDLER_ARGS
{
	int *name = (int *) arg1;
	u_int namelen = arg2;
	struct gmonparam *gp = &_gmonparam;
	int error;
	int state;

	/* all sysctl names at this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);		/* overloaded */

	switch (name[0]) {
	case GPROF_STATE:
		state = gp->state;
		error = sysctl_handle_int(oidp, &state, 0, req);
		if (error)
			return (error);
		if (!req->newptr)
			return (0);
		if (state == GMON_PROF_OFF) {
			stopprofclock(&proc0);
			gp->state = state;
		} else if (state == GMON_PROF_ON) {
			gp->profrate = profhz;
			gp->state = state;
			startprofclock(&proc0);
#ifdef GUPROF
		} else if (state == GMON_PROF_HIRES) {
			gp->profrate = 1193182;	/* XXX */
			stopprofclock(&proc0);
			gp->state = state;
#endif
		} else if (state != gp->state)
			return (EINVAL);
		return (0);
	case GPROF_COUNT:
		return (sysctl_handle_opaque(oidp, 
			gp->kcount, gp->kcountsize, req));
	case GPROF_FROMS:
		return (sysctl_handle_opaque(oidp, 
			gp->froms, gp->fromssize, req));
	case GPROF_TOS:
		return (sysctl_handle_opaque(oidp, 
			gp->tos, gp->tossize, req));
	case GPROF_GMONPARAM:
		return (sysctl_handle_opaque(oidp, gp, sizeof *gp, req));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

SYSCTL_NODE(_kern, KERN_PROF, prof, CTLFLAG_RW, sysctl_kern_prof, "");
#endif /* GPROF */

/*
 * Profiling system call.
 *
 * The scale factor is a fixed point number with 16 bits of fraction, so that
 * 1.0 is represented as 0x10000.  A scale factor of 0 turns off profiling.
 */
#ifndef _SYS_SYSPROTO_H_
struct profil_args {
	caddr_t	samples;
	u_int	size;
	u_int	offset;
	u_int	scale;
};
#endif
/* ARGSUSED */
int
profil(p, uap, retval)
	struct proc *p;
	register struct profil_args *uap;
	int *retval;
{
	register struct uprof *upp;
	int s;

	if (uap->scale > (1 << 16))
		return (EINVAL);
	if (uap->scale == 0) {
		stopprofclock(p);
		return (0);
	}
	upp = &p->p_stats->p_prof;

	/* Block profile interrupts while changing state. */
	s = splstatclock();
	upp->pr_off = uap->offset;
	upp->pr_scale = uap->scale;
	upp->pr_base = uap->samples;
	upp->pr_size = uap->size;
	startprofclock(p);
	splx(s);

	return (0);
}

/*
 * Scale is a fixed-point number with the binary point 16 bits
 * into the value, and is <= 1.0.  pc is at most 32 bits, so the
 * intermediate result is at most 48 bits.
 */
#define	PC_TO_INDEX(pc, prof) \
	((int)(((u_quad_t)((pc) - (prof)->pr_off) * \
	    (u_quad_t)((prof)->pr_scale)) >> 16) & ~1)

/*
 * Collect user-level profiling statistics; called on a profiling tick,
 * when a process is running in user-mode.  This routine may be called
 * from an interrupt context.  We try to update the user profiling buffers
 * cheaply with fuswintr() and suswintr().  If that fails, we revert to
 * an AST that will vector us to trap() with a context in which copyin
 * and copyout will work.  Trap will then call addupc_task().
 *
 * Note that we may (rarely) not get around to the AST soon enough, and
 * lose profile ticks when the next tick overwrites this one, but in this
 * case the system is overloaded and the profile is probably already
 * inaccurate.
 */
void
addupc_intr(p, pc, ticks)
	register struct proc *p;
	register u_long pc;
	u_int ticks;
{
	register struct uprof *prof;
	register caddr_t addr;
	register u_int i;
	register int v;

	if (ticks == 0)
		return;
	prof = &p->p_stats->p_prof;
	if (pc < prof->pr_off ||
	    (i = PC_TO_INDEX(pc, prof)) >= prof->pr_size)
		return;			/* out of range; ignore */

	addr = prof->pr_base + i;
	if ((v = fuswintr(addr)) == -1 || suswintr(addr, v + ticks) == -1) {
		prof->pr_addr = pc;
		prof->pr_ticks = ticks;
		need_proftick(p);
	}
}

/*
 * Much like before, but we can afford to take faults here.  If the
 * update fails, we simply turn off profiling.
 */
void
addupc_task(p, pc, ticks)
	register struct proc *p;
	register u_long pc;
	u_int ticks;
{
	register struct uprof *prof;
	register caddr_t addr;
	register u_int i;
	u_short v;

	/* Testing P_PROFIL may be unnecessary, but is certainly safe. */
	if ((p->p_flag & P_PROFIL) == 0 || ticks == 0)
		return;

	prof = &p->p_stats->p_prof;
	if (pc < prof->pr_off ||
	    (i = PC_TO_INDEX(pc, prof)) >= prof->pr_size)
		return;

	addr = prof->pr_base + i;
	if (copyin(addr, (caddr_t)&v, sizeof(v)) == 0) {
		v += ticks;
		if (copyout((caddr_t)&v, addr, sizeof(v)) == 0)
			return;
	}
	stopprofclock(p);
}
