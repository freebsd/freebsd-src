/*-
 * Copyright (c) 2010 Alexander Motin <mav@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Common routines to manage event timers hardware.
 */

#include "opt_device_polling.h"
#include "opt_kdtrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/kdb.h>
#include <sys/ktr.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/timeet.h>
#include <sys/timetc.h>

#include <machine/atomic.h>
#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/smp.h>

#ifdef KDTRACE_HOOKS
#include <sys/dtrace_bsd.h>
cyclic_clock_func_t	cyclic_clock_func[MAXCPU];
#endif

int			cpu_disable_deep_sleep = 0; /* Timer dies in C3. */

static void		setuptimer(void);
static void		loadtimer(struct bintime *now, int first);
static int		doconfigtimer(void);
static void		configtimer(int start);
static int		round_freq(struct eventtimer *et, int freq);

static void		getnextcpuevent(struct bintime *event, int idle);
static void		getnextevent(struct bintime *event);
static int		handleevents(struct bintime *now, int fake);
#ifdef SMP
static void		cpu_new_callout(int cpu, int ticks);
#endif

static struct mtx	et_hw_mtx;

#define	ET_HW_LOCK(state)						\
	{								\
		if (timer->et_flags & ET_FLAGS_PERCPU)			\
			mtx_lock_spin(&(state)->et_hw_mtx);		\
		else							\
			mtx_lock_spin(&et_hw_mtx);			\
	}

#define	ET_HW_UNLOCK(state)						\
	{								\
		if (timer->et_flags & ET_FLAGS_PERCPU)			\
			mtx_unlock_spin(&(state)->et_hw_mtx);		\
		else							\
			mtx_unlock_spin(&et_hw_mtx);			\
	}

static struct eventtimer *timer = NULL;
static struct bintime	timerperiod;	/* Timer period for periodic mode. */
static struct bintime	hardperiod;	/* hardclock() events period. */
static struct bintime	statperiod;	/* statclock() events period. */
static struct bintime	profperiod;	/* profclock() events period. */
static struct bintime	nexttick;	/* Next global timer tick time. */
static u_int		busy = 0;	/* Reconfiguration is in progress. */
static int		profiling = 0;	/* Profiling events enabled. */

static char		timername[32];	/* Wanted timer. */
TUNABLE_STR("kern.eventtimer.timer", timername, sizeof(timername));

static int		singlemul = 0;	/* Multiplier for periodic mode. */
TUNABLE_INT("kern.eventtimer.singlemul", &singlemul);
SYSCTL_INT(_kern_eventtimer, OID_AUTO, singlemul, CTLFLAG_RW, &singlemul,
    0, "Multiplier for periodic mode");

static u_int		idletick = 0;	/* Idle mode allowed. */
TUNABLE_INT("kern.eventtimer.idletick", &idletick);
SYSCTL_UINT(_kern_eventtimer, OID_AUTO, idletick, CTLFLAG_RW, &idletick,
    0, "Run periodic events when idle");

static int		periodic = 0;	/* Periodic or one-shot mode. */
static int		want_periodic = 0; /* What mode to prefer. */
TUNABLE_INT("kern.eventtimer.periodic", &want_periodic);

struct pcpu_state {
	struct mtx	et_hw_mtx;	/* Per-CPU timer mutex. */
	u_int		action;		/* Reconfiguration requests. */
	u_int		handle;		/* Immediate handle resuests. */
	struct bintime	now;		/* Last tick time. */
	struct bintime	nextevent;	/* Next scheduled event on this CPU. */
	struct bintime	nexttick;	/* Next timer tick time. */
	struct bintime	nexthard;	/* Next hardlock() event. */
	struct bintime	nextstat;	/* Next statclock() event. */
	struct bintime	nextprof;	/* Next profclock() event. */
	int		ipi;		/* This CPU needs IPI. */
	int		idle;		/* This CPU is in idle mode. */
};

static DPCPU_DEFINE(struct pcpu_state, timerstate);

#define FREQ2BT(freq, bt)						\
{									\
	(bt)->sec = 0;							\
	(bt)->frac = ((uint64_t)0x8000000000000000  / (freq)) << 1;	\
}
#define BT2FREQ(bt)							\
	(((uint64_t)0x8000000000000000 + ((bt)->frac >> 2)) /		\
	    ((bt)->frac >> 1))

/*
 * Timer broadcast IPI handler.
 */
int
hardclockintr(void)
{
	struct bintime now;
	struct pcpu_state *state;
	int done;

	if (doconfigtimer() || busy)
		return (FILTER_HANDLED);
	state = DPCPU_PTR(timerstate);
	now = state->now;
	CTR4(KTR_SPARE2, "ipi  at %d:    now  %d.%08x%08x",
	    curcpu, now.sec, (unsigned int)(now.frac >> 32),
			     (unsigned int)(now.frac & 0xffffffff));
	done = handleevents(&now, 0);
	return (done ? FILTER_HANDLED : FILTER_STRAY);
}

/*
 * Handle all events for specified time on this CPU
 */
static int
handleevents(struct bintime *now, int fake)
{
	struct bintime t;
	struct trapframe *frame;
	struct pcpu_state *state;
	uintfptr_t pc;
	int usermode;
	int done, runs;

	CTR4(KTR_SPARE2, "handle at %d:  now  %d.%08x%08x",
	    curcpu, now->sec, (unsigned int)(now->frac >> 32),
		     (unsigned int)(now->frac & 0xffffffff));
	done = 0;
	if (fake) {
		frame = NULL;
		usermode = 0;
		pc = 0;
	} else {
		frame = curthread->td_intr_frame;
		usermode = TRAPF_USERMODE(frame);
		pc = TRAPF_PC(frame);
	}
#ifdef KDTRACE_HOOKS
	/*
	 * If the DTrace hooks are configured and a callback function
	 * has been registered, then call it to process the high speed
	 * timers.
	 */
	if (!fake && cyclic_clock_func[curcpu] != NULL)
		(*cyclic_clock_func[curcpu])(frame);
#endif
	runs = 0;
	state = DPCPU_PTR(timerstate);
	while (bintime_cmp(now, &state->nexthard, >=)) {
		bintime_add(&state->nexthard, &hardperiod);
		runs++;
	}
	if (runs && fake < 2) {
		hardclock_anycpu(runs, usermode);
		done = 1;
	}
	while (bintime_cmp(now, &state->nextstat, >=)) {
		if (fake < 2)
			statclock(usermode);
		bintime_add(&state->nextstat, &statperiod);
		done = 1;
	}
	if (profiling) {
		while (bintime_cmp(now, &state->nextprof, >=)) {
			if (!fake)
				profclock(usermode, pc);
			bintime_add(&state->nextprof, &profperiod);
			done = 1;
		}
	} else
		state->nextprof = state->nextstat;
	getnextcpuevent(&t, 0);
	if (fake == 2) {
		state->nextevent = t;
		return (done);
	}
	ET_HW_LOCK(state);
	if (!busy) {
		state->idle = 0;
		state->nextevent = t;
		loadtimer(now, 0);
	}
	ET_HW_UNLOCK(state);
	return (done);
}

/*
 * Schedule binuptime of the next event on current CPU.
 */
static void
getnextcpuevent(struct bintime *event, int idle)
{
	struct bintime tmp;
	struct pcpu_state *state;
	int skip;

	state = DPCPU_PTR(timerstate);
	*event = state->nexthard;
	if (idle) { /* If CPU is idle - ask callouts for how long. */
		skip = 4;
		if (curcpu == CPU_FIRST() && tc_min_ticktock_freq > skip)
			skip = tc_min_ticktock_freq;
		skip = callout_tickstofirst(hz / skip) - 1;
		CTR2(KTR_SPARE2, "skip   at %d: %d", curcpu, skip);
		tmp = hardperiod;
		bintime_mul(&tmp, skip);
		bintime_add(event, &tmp);
	} else { /* If CPU is active - handle all types of events. */
		if (bintime_cmp(event, &state->nextstat, >))
			*event = state->nextstat;
		if (profiling &&
		    bintime_cmp(event, &state->nextprof, >))
			*event = state->nextprof;
	}
}

/*
 * Schedule binuptime of the next event on all CPUs.
 */
static void
getnextevent(struct bintime *event)
{
	struct pcpu_state *state;
#ifdef SMP
	int	cpu;
#endif
	int	c;

	state = DPCPU_PTR(timerstate);
	*event = state->nextevent;
	c = curcpu;
#ifdef SMP
	if ((timer->et_flags & ET_FLAGS_PERCPU) == 0) {
		CPU_FOREACH(cpu) {
			if (curcpu == cpu)
				continue;
			state = DPCPU_ID_PTR(cpu, timerstate);
			if (bintime_cmp(event, &state->nextevent, >)) {
				*event = state->nextevent;
				c = cpu;
			}
		}
	}
#endif
	CTR5(KTR_SPARE2, "next at %d:    next %d.%08x%08x by %d",
	    curcpu, event->sec, (unsigned int)(event->frac >> 32),
			     (unsigned int)(event->frac & 0xffffffff), c);
}

/* Hardware timer callback function. */
static void
timercb(struct eventtimer *et, void *arg)
{
	struct bintime now;
	struct bintime *next;
	struct pcpu_state *state;
#ifdef SMP
	int cpu, bcast;
#endif

	/* Do not touch anything if somebody reconfiguring timers. */
	if (busy)
		return;
	/* Update present and next tick times. */
	state = DPCPU_PTR(timerstate);
	if (et->et_flags & ET_FLAGS_PERCPU) {
		next = &state->nexttick;
	} else
		next = &nexttick;
	if (periodic) {
		now = *next;	/* Ex-next tick time becomes present time. */
		bintime_add(next, &timerperiod); /* Next tick in 1 period. */
	} else {
		binuptime(&now);	/* Get present time from hardware. */
		next->sec = -1;		/* Next tick is not scheduled yet. */
	}
	state->now = now;
	CTR4(KTR_SPARE2, "intr at %d:    now  %d.%08x%08x",
	    curcpu, now.sec, (unsigned int)(now.frac >> 32),
			     (unsigned int)(now.frac & 0xffffffff));

#ifdef SMP
	/* Prepare broadcasting to other CPUs for non-per-CPU timers. */
	bcast = 0;
	if ((et->et_flags & ET_FLAGS_PERCPU) == 0 && smp_started) {
		CPU_FOREACH(cpu) {
			state = DPCPU_ID_PTR(cpu, timerstate);
			ET_HW_LOCK(state);
			state->now = now;
			if (bintime_cmp(&now, &state->nextevent, >=)) {
				state->nextevent.sec++;
				if (curcpu != cpu) {
					state->ipi = 1;
					bcast = 1;
				}
			}
			ET_HW_UNLOCK(state);
		}
	}
#endif

	/* Handle events for this time on this CPU. */
	handleevents(&now, 0);

#ifdef SMP
	/* Broadcast interrupt to other CPUs for non-per-CPU timers. */
	if (bcast) {
		CPU_FOREACH(cpu) {
			if (curcpu == cpu)
				continue;
			state = DPCPU_ID_PTR(cpu, timerstate);
			if (state->ipi) {
				state->ipi = 0;
				ipi_cpu(cpu, IPI_HARDCLOCK);
			}
		}
	}
#endif
}

/*
 * Load new value into hardware timer.
 */
static void
loadtimer(struct bintime *now, int start)
{
	struct pcpu_state *state;
	struct bintime new;
	struct bintime *next;
	uint64_t tmp;
	int eq;

	if (timer->et_flags & ET_FLAGS_PERCPU) {
		state = DPCPU_PTR(timerstate);
		next = &state->nexttick;
	} else
		next = &nexttick;
	if (periodic) {
		if (start) {
			/*
			 * Try to start all periodic timers aligned
			 * to period to make events synchronous.
			 */
			tmp = ((uint64_t)now->sec << 36) + (now->frac >> 28);
			tmp = (tmp % (timerperiod.frac >> 28)) << 28;
			new.sec = 0;
			new.frac = timerperiod.frac - tmp;
			if (new.frac < tmp)	/* Left less then passed. */
				bintime_add(&new, &timerperiod);
			CTR5(KTR_SPARE2, "load p at %d:   now %d.%08x first in %d.%08x",
			    curcpu, now->sec, (unsigned int)(now->frac >> 32),
			    new.sec, (unsigned int)(new.frac >> 32));
			*next = new;
			bintime_add(next, now);
			et_start(timer, &new, &timerperiod);
		}
	} else {
		getnextevent(&new);
		eq = bintime_cmp(&new, next, ==);
		CTR5(KTR_SPARE2, "load at %d:    next %d.%08x%08x eq %d",
		    curcpu, new.sec, (unsigned int)(new.frac >> 32),
			     (unsigned int)(new.frac & 0xffffffff),
			     eq);
		if (!eq) {
			*next = new;
			bintime_sub(&new, now);
			et_start(timer, &new, NULL);
		}
	}
}

/*
 * Prepare event timer parameters after configuration changes.
 */
static void
setuptimer(void)
{
	int freq;

	if (periodic && (timer->et_flags & ET_FLAGS_PERIODIC) == 0)
		periodic = 0;
	else if (!periodic && (timer->et_flags & ET_FLAGS_ONESHOT) == 0)
		periodic = 1;
	singlemul = MIN(MAX(singlemul, 1), 20);
	freq = hz * singlemul;
	while (freq < (profiling ? profhz : stathz))
		freq += hz;
	freq = round_freq(timer, freq);
	FREQ2BT(freq, &timerperiod);
}

/*
 * Reconfigure specified per-CPU timer on other CPU. Called from IPI handler.
 */
static int
doconfigtimer(void)
{
	struct bintime now;
	struct pcpu_state *state;

	state = DPCPU_PTR(timerstate);
	switch (atomic_load_acq_int(&state->action)) {
	case 1:
		binuptime(&now);
		ET_HW_LOCK(state);
		loadtimer(&now, 1);
		ET_HW_UNLOCK(state);
		state->handle = 0;
		atomic_store_rel_int(&state->action, 0);
		return (1);
	case 2:
		ET_HW_LOCK(state);
		et_stop(timer);
		ET_HW_UNLOCK(state);
		state->handle = 0;
		atomic_store_rel_int(&state->action, 0);
		return (1);
	}
	if (atomic_readandclear_int(&state->handle) && !busy) {
		binuptime(&now);
		handleevents(&now, 0);
		return (1);
	}
	return (0);
}

/*
 * Reconfigure specified timer.
 * For per-CPU timers use IPI to make other CPUs to reconfigure.
 */
static void
configtimer(int start)
{
	struct bintime now, next;
	struct pcpu_state *state;
	int cpu;

	if (start) {
		setuptimer();
		binuptime(&now);
	}
	critical_enter();
	ET_HW_LOCK(DPCPU_PTR(timerstate));
	if (start) {
		/* Initialize time machine parameters. */
		next = now;
		bintime_add(&next, &timerperiod);
		if (periodic)
			nexttick = next;
		else
			nexttick.sec = -1;
		CPU_FOREACH(cpu) {
			state = DPCPU_ID_PTR(cpu, timerstate);
			state->now = now;
			state->nextevent = next;
			if (periodic)
				state->nexttick = next;
			else
				state->nexttick.sec = -1;
			state->nexthard = next;
			state->nextstat = next;
			state->nextprof = next;
			hardclock_sync(cpu);
		}
		busy = 0;
		/* Start global timer or per-CPU timer of this CPU. */
		loadtimer(&now, 1);
	} else {
		busy = 1;
		/* Stop global timer or per-CPU timer of this CPU. */
		et_stop(timer);
	}
	ET_HW_UNLOCK(DPCPU_PTR(timerstate));
#ifdef SMP
	/* If timer is global or there is no other CPUs yet - we are done. */
	if ((timer->et_flags & ET_FLAGS_PERCPU) == 0 || !smp_started) {
		critical_exit();
		return;
	}
	/* Set reconfigure flags for other CPUs. */
	CPU_FOREACH(cpu) {
		state = DPCPU_ID_PTR(cpu, timerstate);
		atomic_store_rel_int(&state->action,
		    (cpu == curcpu) ? 0 : ( start ? 1 : 2));
	}
	/* Broadcast reconfigure IPI. */
	ipi_all_but_self(IPI_HARDCLOCK);
	/* Wait for reconfiguration completed. */
restart:
	cpu_spinwait();
	CPU_FOREACH(cpu) {
		if (cpu == curcpu)
			continue;
		state = DPCPU_ID_PTR(cpu, timerstate);
		if (atomic_load_acq_int(&state->action))
			goto restart;
	}
#endif
	critical_exit();
}

/*
 * Calculate nearest frequency supported by hardware timer.
 */
static int
round_freq(struct eventtimer *et, int freq)
{
	uint64_t div;

	if (et->et_frequency != 0) {
		div = lmax((et->et_frequency + freq / 2) / freq, 1);
		if (et->et_flags & ET_FLAGS_POW2DIV)
			div = 1 << (flsl(div + div / 2) - 1);
		freq = (et->et_frequency + div / 2) / div;
	}
	if (et->et_min_period.sec > 0)
		freq = 0;
	else if (et->et_min_period.frac != 0)
		freq = min(freq, BT2FREQ(&et->et_min_period));
	if (et->et_max_period.sec == 0 && et->et_max_period.frac != 0)
		freq = max(freq, BT2FREQ(&et->et_max_period));
	return (freq);
}

/*
 * Configure and start event timers (BSP part).
 */
void
cpu_initclocks_bsp(void)
{
	struct pcpu_state *state;
	int base, div, cpu;

	mtx_init(&et_hw_mtx, "et_hw_mtx", NULL, MTX_SPIN);
	CPU_FOREACH(cpu) {
		state = DPCPU_ID_PTR(cpu, timerstate);
		mtx_init(&state->et_hw_mtx, "et_hw_mtx", NULL, MTX_SPIN);
	}
#ifdef SMP
	callout_new_inserted = cpu_new_callout;
#endif
	periodic = want_periodic;
	/* Grab requested timer or the best of present. */
	if (timername[0])
		timer = et_find(timername, 0, 0);
	if (timer == NULL && periodic) {
		timer = et_find(NULL,
		    ET_FLAGS_PERIODIC, ET_FLAGS_PERIODIC);
	}
	if (timer == NULL) {
		timer = et_find(NULL,
		    ET_FLAGS_ONESHOT, ET_FLAGS_ONESHOT);
	}
	if (timer == NULL && !periodic) {
		timer = et_find(NULL,
		    ET_FLAGS_PERIODIC, ET_FLAGS_PERIODIC);
	}
	if (timer == NULL)
		panic("No usable event timer found!");
	et_init(timer, timercb, NULL, NULL);

	/* Adapt to timer capabilities. */
	if (periodic && (timer->et_flags & ET_FLAGS_PERIODIC) == 0)
		periodic = 0;
	else if (!periodic && (timer->et_flags & ET_FLAGS_ONESHOT) == 0)
		periodic = 1;
	if (timer->et_flags & ET_FLAGS_C3STOP)
		cpu_disable_deep_sleep++;

	/*
	 * We honor the requested 'hz' value.
	 * We want to run stathz in the neighborhood of 128hz.
	 * We would like profhz to run as often as possible.
	 */
	if (singlemul <= 0 || singlemul > 20) {
		if (hz >= 1500 || (hz % 128) == 0)
			singlemul = 1;
		else if (hz >= 750)
			singlemul = 2;
		else
			singlemul = 4;
	}
	if (periodic) {
		base = round_freq(timer, hz * singlemul);
		singlemul = max((base + hz / 2) / hz, 1);
		hz = (base + singlemul / 2) / singlemul;
		if (base <= 128)
			stathz = base;
		else {
			div = base / 128;
			if (div >= singlemul && (div % singlemul) == 0)
				div++;
			stathz = base / div;
		}
		profhz = stathz;
		while ((profhz + stathz) <= 128 * 64)
			profhz += stathz;
		profhz = round_freq(timer, profhz);
	} else {
		hz = round_freq(timer, hz);
		stathz = round_freq(timer, 127);
		profhz = round_freq(timer, stathz * 64);
	}
	tick = 1000000 / hz;
	FREQ2BT(hz, &hardperiod);
	FREQ2BT(stathz, &statperiod);
	FREQ2BT(profhz, &profperiod);
	ET_LOCK();
	configtimer(1);
	ET_UNLOCK();
}

/*
 * Start per-CPU event timers on APs.
 */
void
cpu_initclocks_ap(void)
{
	struct bintime now;
	struct pcpu_state *state;

	state = DPCPU_PTR(timerstate);
	binuptime(&now);
	ET_HW_LOCK(state);
	if ((timer->et_flags & ET_FLAGS_PERCPU) == 0 && periodic) {
		state->now = nexttick;
		bintime_sub(&state->now, &timerperiod);
	} else
		state->now = now;
	hardclock_sync(curcpu);
	handleevents(&state->now, 2);
	if (timer->et_flags & ET_FLAGS_PERCPU)
		loadtimer(&now, 1);
	ET_HW_UNLOCK(state);
}

/*
 * Switch to profiling clock rates.
 */
void
cpu_startprofclock(void)
{

	ET_LOCK();
	if (periodic) {
		configtimer(0);
		profiling = 1;
		configtimer(1);
	} else
		profiling = 1;
	ET_UNLOCK();
}

/*
 * Switch to regular clock rates.
 */
void
cpu_stopprofclock(void)
{

	ET_LOCK();
	if (periodic) {
		configtimer(0);
		profiling = 0;
		configtimer(1);
	} else
		profiling = 0;
	ET_UNLOCK();
}

/*
 * Switch to idle mode (all ticks handled).
 */
void
cpu_idleclock(void)
{
	struct bintime now, t;
	struct pcpu_state *state;

	if (idletick || busy ||
	    (periodic && (timer->et_flags & ET_FLAGS_PERCPU))
#ifdef DEVICE_POLLING
	    || curcpu == CPU_FIRST()
#endif
	    )
		return;
	state = DPCPU_PTR(timerstate);
	if (periodic)
		now = state->now;
	else
		binuptime(&now);
	CTR4(KTR_SPARE2, "idle at %d:    now  %d.%08x%08x",
	    curcpu, now.sec, (unsigned int)(now.frac >> 32),
			     (unsigned int)(now.frac & 0xffffffff));
	getnextcpuevent(&t, 1);
	ET_HW_LOCK(state);
	state->idle = 1;
	state->nextevent = t;
	if (!periodic)
		loadtimer(&now, 0);
	ET_HW_UNLOCK(state);
}

/*
 * Switch to active mode (skip empty ticks).
 */
void
cpu_activeclock(void)
{
	struct bintime now;
	struct pcpu_state *state;
	struct thread *td;

	state = DPCPU_PTR(timerstate);
	if (state->idle == 0 || busy)
		return;
	if (periodic)
		now = state->now;
	else
		binuptime(&now);
	CTR4(KTR_SPARE2, "active at %d:  now  %d.%08x%08x",
	    curcpu, now.sec, (unsigned int)(now.frac >> 32),
			     (unsigned int)(now.frac & 0xffffffff));
	spinlock_enter();
	td = curthread;
	td->td_intr_nesting_level++;
	handleevents(&now, 1);
	td->td_intr_nesting_level--;
	spinlock_exit();
}

#ifdef SMP
static void
cpu_new_callout(int cpu, int ticks)
{
	struct bintime tmp;
	struct pcpu_state *state;

	CTR3(KTR_SPARE2, "new co at %d:    on %d in %d",
	    curcpu, cpu, ticks);
	state = DPCPU_ID_PTR(cpu, timerstate);
	ET_HW_LOCK(state);
	if (state->idle == 0 || busy) {
		ET_HW_UNLOCK(state);
		return;
	}
	/*
	 * If timer is periodic - just update next event time for target CPU.
	 * If timer is global - there is chance it is already programmed.
	 */
	if (periodic || (timer->et_flags & ET_FLAGS_PERCPU) == 0) {
		state->nextevent = state->nexthard;
		tmp = hardperiod;
		bintime_mul(&tmp, ticks - 1);
		bintime_add(&state->nextevent, &tmp);
		if (periodic ||
		    bintime_cmp(&state->nextevent, &nexttick, >=)) {
			ET_HW_UNLOCK(state);
			return;
		}
	}
	/*
	 * Otherwise we have to wake that CPU up, as we can't get present
	 * bintime to reprogram global timer from here. If timer is per-CPU,
	 * we by definition can't do it from here.
	 */
	ET_HW_UNLOCK(state);
	if (timer->et_flags & ET_FLAGS_PERCPU) {
		state->handle = 1;
		ipi_cpu(cpu, IPI_HARDCLOCK);
	} else {
		if (!cpu_idle_wakeup(cpu))
			ipi_cpu(cpu, IPI_AST);
	}
}
#endif

/*
 * Report or change the active event timers hardware.
 */
static int
sysctl_kern_eventtimer_timer(SYSCTL_HANDLER_ARGS)
{
	char buf[32];
	struct eventtimer *et;
	int error;

	ET_LOCK();
	et = timer;
	snprintf(buf, sizeof(buf), "%s", et->et_name);
	ET_UNLOCK();
	error = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	ET_LOCK();
	et = timer;
	if (error != 0 || req->newptr == NULL ||
	    strcasecmp(buf, et->et_name) == 0) {
		ET_UNLOCK();
		return (error);
	}
	et = et_find(buf, 0, 0);
	if (et == NULL) {
		ET_UNLOCK();
		return (ENOENT);
	}
	configtimer(0);
	et_free(timer);
	if (et->et_flags & ET_FLAGS_C3STOP)
		cpu_disable_deep_sleep++;
	if (timer->et_flags & ET_FLAGS_C3STOP)
		cpu_disable_deep_sleep--;
	periodic = want_periodic;
	timer = et;
	et_init(timer, timercb, NULL, NULL);
	configtimer(1);
	ET_UNLOCK();
	return (error);
}
SYSCTL_PROC(_kern_eventtimer, OID_AUTO, timer,
    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, 0, sysctl_kern_eventtimer_timer, "A", "Chosen event timer");

/*
 * Report or change the active event timer periodicity.
 */
static int
sysctl_kern_eventtimer_periodic(SYSCTL_HANDLER_ARGS)
{
	int error, val;

	val = periodic;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	ET_LOCK();
	configtimer(0);
	periodic = want_periodic = val;
	configtimer(1);
	ET_UNLOCK();
	return (error);
}
SYSCTL_PROC(_kern_eventtimer, OID_AUTO, periodic,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, 0, sysctl_kern_eventtimer_periodic, "I", "Enable event timer periodic mode");
