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

/* XEN has own timer routines now. */
#ifndef XEN

#include "opt_clock.h"
#include "opt_kdtrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/kdb.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/timeet.h>

#include <machine/atomic.h>
#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/smp.h>

#ifdef KDTRACE_HOOKS
#include <sys/dtrace_bsd.h>
cyclic_clock_func_t	cyclic_clock_func[MAXCPU];
#endif

static void		cpu_restartclocks(void);
static void		timercheck(void);
inline static int	doconfigtimer(int i);
static void		configtimer(int i);

static struct eventtimer *timer[2] = { NULL, NULL };
static int		timertest = 0;
static int		timerticks[2] = { 0, 0 };
static int		profiling_on = 0;
static struct bintime	timerperiod[2];

static char		timername[2][32];
TUNABLE_STR("kern.eventtimer.timer1", timername[0], sizeof(*timername));
TUNABLE_STR("kern.eventtimer.timer2", timername[1], sizeof(*timername));

static u_int		singlemul = 0;
TUNABLE_INT("kern.eventtimer.singlemul", &singlemul);
SYSCTL_INT(_kern_eventtimer, OID_AUTO, singlemul, CTLFLAG_RW, &singlemul,
    0, "Multiplier, used in single timer mode");

typedef u_int tc[2];
static DPCPU_DEFINE(tc, configtimer);

#define FREQ2BT(freq, bt)						\
{									\
	(bt)->sec = 0;							\
	(bt)->frac = ((uint64_t)0x8000000000000000  / (freq)) << 1;	\
}
#define BT2FREQ(bt, freq)						\
{									\
	*(freq) = ((uint64_t)0x8000000000000000 + ((bt)->frac >> 2)) /	\
		    ((bt)->frac >> 1);					\
}

/* Per-CPU timer1 handler. */
static int
hardclockhandler(struct trapframe *frame)
{

#ifdef KDTRACE_HOOKS
	/*
	 * If the DTrace hooks are configured and a callback function
	 * has been registered, then call it to process the high speed
	 * timers.
	 */
	int cpu = curcpu;
	if (cyclic_clock_func[cpu] != NULL)
		(*cyclic_clock_func[cpu])(frame);
#endif

	timer1clock(TRAPF_USERMODE(frame), TRAPF_PC(frame));
	return (FILTER_HANDLED);
}

/* Per-CPU timer2 handler. */
static int
statclockhandler(struct trapframe *frame)
{

	timer2clock(TRAPF_USERMODE(frame), TRAPF_PC(frame));
	return (FILTER_HANDLED);
}

/* timer1 broadcast IPI handler. */
int
hardclockintr(struct trapframe *frame)
{

	if (doconfigtimer(0))
		return (FILTER_HANDLED);
	return (hardclockhandler(frame));
}

/* timer2 broadcast IPI handler. */
int
statclockintr(struct trapframe *frame)
{

	if (doconfigtimer(1))
		return (FILTER_HANDLED);
	return (statclockhandler(frame));
}

/* timer1 callback. */
static void
timer1cb(struct eventtimer *et, void *arg)
{

#ifdef SMP
	/* Broadcast interrupt to other CPUs for non-per-CPU timers */
	if (smp_started && (et->et_flags & ET_FLAGS_PERCPU) == 0)
		ipi_all_but_self(IPI_HARDCLOCK);
#endif
	if (timertest) {
		if ((et->et_flags & ET_FLAGS_PERCPU) == 0 || curcpu == 0) {
			timerticks[0]++;
			if (timerticks[0] >= timer1hz) {
				ET_LOCK();
				timercheck();
				ET_UNLOCK();
			}
		}
	}
	hardclockhandler((struct trapframe *)arg);
}

/* timer2 callback. */
static void
timer2cb(struct eventtimer *et, void *arg)
{

#ifdef SMP
	/* Broadcast interrupt to other CPUs for non-per-CPU timers */
	if (smp_started && (et->et_flags & ET_FLAGS_PERCPU) == 0)
		ipi_all_but_self(IPI_STATCLOCK);
#endif
	if (timertest) {
		if ((et->et_flags & ET_FLAGS_PERCPU) == 0 || curcpu == 0) {
			timerticks[1]++;
			if (timerticks[1] >= timer2hz * 2) {
				ET_LOCK();
				timercheck();
				ET_UNLOCK();
			}
		}
	}
	statclockhandler((struct trapframe *)arg);
}

/*
 * Check that both timers are running with at least 1/4 of configured rate.
 * If not - replace the broken one.
 */
static void
timercheck(void)
{

	if (!timertest)
		return;
	timertest = 0;
	if (timerticks[0] * 4 < timer1hz) {
		printf("Event timer \"%s\" is dead.\n", timer[0]->et_name);
		timer1hz = 0;
		configtimer(0);
		et_ban(timer[0]);
		et_free(timer[0]);
		timer[0] = et_find(NULL, ET_FLAGS_PERIODIC, ET_FLAGS_PERIODIC);
		if (timer[0] == NULL) {
			timer2hz = 0;
			configtimer(1);
			et_free(timer[1]);
			timer[1] = NULL;
			timer[0] = timer[1];
		}
		et_init(timer[0], timer1cb, NULL, NULL);
		cpu_restartclocks();
		return;
	}
	if (timerticks[1] * 4 < timer2hz) {
		printf("Event timer \"%s\" is dead.\n", timer[1]->et_name);
		timer2hz = 0;
		configtimer(1);
		et_ban(timer[1]);
		et_free(timer[1]);
		timer[1] = et_find(NULL, ET_FLAGS_PERIODIC, ET_FLAGS_PERIODIC);
		if (timer[1] != NULL)
			et_init(timer[1], timer2cb, NULL, NULL);
		cpu_restartclocks();
		return;
	}
}

/*
 * Reconfigure specified per-CPU timer on other CPU. Called from IPI handler.
 */
inline static int
doconfigtimer(int i)
{
	tc *conf;

	conf = DPCPU_PTR(configtimer);
	if (atomic_load_acq_int(*conf + i)) {
		if (i == 0 ? timer1hz : timer2hz)
			et_start(timer[i], NULL, &timerperiod[i]);
		else
			et_stop(timer[i]);
		atomic_store_rel_int(*conf + i, 0);
		return (1);
	}
	return (0);
}

/*
 * Reconfigure specified timer.
 * For per-CPU timers use IPI to make other CPUs to reconfigure.
 */
static void
configtimer(int i)
{
#ifdef SMP
	tc *conf;
	int cpu;

	critical_enter();
#endif
	/* Start/stop global timer or per-CPU timer of this CPU. */
	if (i == 0 ? timer1hz : timer2hz)
		et_start(timer[i], NULL, &timerperiod[i]);
	else
		et_stop(timer[i]);
#ifdef SMP
	if ((timer[i]->et_flags & ET_FLAGS_PERCPU) == 0 || !smp_started) {
		critical_exit();
		return;
	}
	/* Set reconfigure flags for other CPUs. */
	CPU_FOREACH(cpu) {
		conf = DPCPU_ID_PTR(cpu, configtimer);
		atomic_store_rel_int(*conf + i, (cpu == curcpu) ? 0 : 1);
	}
	/* Send reconfigure IPI. */
	ipi_all_but_self(i == 0 ? IPI_HARDCLOCK : IPI_STATCLOCK);
	/* Wait for reconfiguration completed. */
restart:
	cpu_spinwait();
	CPU_FOREACH(cpu) {
		if (cpu == curcpu)
			continue;
		conf = DPCPU_ID_PTR(cpu, configtimer);
		if (atomic_load_acq_int(*conf + i))
			goto restart;
	}
	critical_exit();
#endif
}

/*
 * Configure and start event timers.
 */
void
cpu_initclocks_bsp(void)
{
	int base, div;

	timer[0] = et_find(timername[0], ET_FLAGS_PERIODIC, ET_FLAGS_PERIODIC);
	if (timer[0] == NULL)
		timer[0] = et_find(NULL, ET_FLAGS_PERIODIC, ET_FLAGS_PERIODIC);
	et_init(timer[0], timer1cb, NULL, NULL);
	timer[1] = et_find(timername[1][0] ? timername[1] : NULL,
	    ET_FLAGS_PERIODIC, ET_FLAGS_PERIODIC);
	if (timer[1])
		et_init(timer[1], timer2cb, NULL, NULL);
	/*
	 * We honor the requested 'hz' value.
	 * We want to run stathz in the neighborhood of 128hz.
	 * We would like profhz to run as often as possible.
	 */
	if (singlemul == 0) {
		if (hz >= 1500 || (hz % 128) == 0)
			singlemul = 1;
		else if (hz >= 750)
			singlemul = 2;
		else
			singlemul = 4;
	}
	if (timer[1] == NULL) {
		base = hz * singlemul;
		if (base < 128)
			stathz = base;
		else {
			div = base / 128;
			if (div % 2 == 0)
				div++;
			stathz = base / div;
		}
		profhz = stathz;
		while ((profhz + stathz) <= 8192)
			profhz += stathz;
	} else {
		stathz = 128;
		profhz = stathz * 64;
	}
	ET_LOCK();
	cpu_restartclocks();
	ET_UNLOCK();
}

/* Start per-CPU event timers on APs. */
void
cpu_initclocks_ap(void)
{

	ET_LOCK();
	if (timer[0]->et_flags & ET_FLAGS_PERCPU)
		et_start(timer[0], NULL, &timerperiod[0]);
	if (timer[1] && timer[1]->et_flags & ET_FLAGS_PERCPU)
		et_start(timer[1], NULL, &timerperiod[1]);
	ET_UNLOCK();
}

/* Reconfigure and restart event timers after configuration changes. */
static void
cpu_restartclocks(void)
{

	/* Stop all event timers. */
	timertest = 0;
	if (timer1hz) {
		timer1hz = 0;
		configtimer(0);
	}
	if (timer[1] && timer2hz) {
		timer2hz = 0;
		configtimer(1);
	}
	/* Calculate new event timers parameters. */
	if (timer[1] == NULL) {
		timer1hz = hz * singlemul;
		while (timer1hz < (profiling_on ? profhz : stathz))
			timer1hz += hz;
		timer2hz = 0;
	} else {
		timer1hz = hz;
		timer2hz = profiling_on ? profhz : stathz;
	}
	printf("Starting kernel event timers: %s @ %dHz, %s @ %dHz\n",
	    timer[0]->et_name, timer1hz,
	    timer[1] ? timer[1]->et_name : "NONE", timer2hz);
	/* Restart event timers. */
	FREQ2BT(timer1hz, &timerperiod[0]);
	configtimer(0);
	if (timer[1]) {
		timerticks[0] = 0;
		timerticks[1] = 0;
		FREQ2BT(timer2hz, &timerperiod[1]);
		configtimer(1);
		timertest = 1;
	}
}

/* Switch to profiling clock rates. */
void
cpu_startprofclock(void)
{

	ET_LOCK();
	profiling_on = 1;
	cpu_restartclocks();
	ET_UNLOCK();
}

/* Switch to regular clock rates. */
void
cpu_stopprofclock(void)
{

	ET_LOCK();
	profiling_on = 0;
	cpu_restartclocks();
	ET_UNLOCK();
}

/* Report or change the active event timers hardware. */
static int
sysctl_kern_eventtimer_timer1(SYSCTL_HANDLER_ARGS)
{
	char buf[32];
	struct eventtimer *et;
	int error;

	ET_LOCK();
	et = timer[0];
	snprintf(buf, sizeof(buf), "%s", et->et_name);
	ET_UNLOCK();
	error = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	ET_LOCK();
	et = timer[0];
	if (error != 0 || req->newptr == NULL ||
	    strcmp(buf, et->et_name) == 0) {
		ET_UNLOCK();
		return (error);
	}
	et = et_find(buf, ET_FLAGS_PERIODIC, ET_FLAGS_PERIODIC);
	if (et == NULL) {
		ET_UNLOCK();
		return (ENOENT);
	}
	timer1hz = 0;
	configtimer(0);
	et_free(timer[0]);
	timer[0] = et;
	et_init(timer[0], timer1cb, NULL, NULL);
	cpu_restartclocks();
	ET_UNLOCK();
	return (error);
}
SYSCTL_PROC(_kern_eventtimer, OID_AUTO, timer1,
    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, 0, sysctl_kern_eventtimer_timer1, "A", "Primary event timer");

static int
sysctl_kern_eventtimer_timer2(SYSCTL_HANDLER_ARGS)
{
	char buf[32];
	struct eventtimer *et;
	int error;

	ET_LOCK();
	et = timer[1];
	if (et == NULL)
		snprintf(buf, sizeof(buf), "NONE");
	else
		snprintf(buf, sizeof(buf), "%s", et->et_name);
	ET_UNLOCK();
	error = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	ET_LOCK();
	et = timer[1];
	if (error != 0 || req->newptr == NULL ||
	    strcmp(buf, et ? et->et_name : "NONE") == 0) {
		ET_UNLOCK();
		return (error);
	}
	et = et_find(buf, ET_FLAGS_PERIODIC, ET_FLAGS_PERIODIC);
	if (et == NULL && strcasecmp(buf, "NONE") != 0) {
		ET_UNLOCK();
		return (ENOENT);
	}
	if (timer[1] != NULL) {
		timer2hz = 0;
		configtimer(1);
		et_free(timer[1]);
	}
	timer[1] = et;
	if (timer[1] != NULL)
		et_init(timer[1], timer2cb, NULL, NULL);
	cpu_restartclocks();
	ET_UNLOCK();
	return (error);
}
SYSCTL_PROC(_kern_eventtimer, OID_AUTO, timer2,
    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, 0, sysctl_kern_eventtimer_timer2, "A", "Secondary event timer");

#endif

