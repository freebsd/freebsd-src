/*-
 * Copyright (c) 2006-2007 Bruce M. Simpson.
 * Copyright (c) 2003-2004 Juli Mallett.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
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

/*
 * Simple driver for the 32-bit interval counter built in to all
 * MIPS32 CPUs.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_cputype.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/power.h>
#include <sys/smp.h>
#include <sys/time.h>
#include <sys/timetc.h>

#include <machine/clock.h>
#include <machine/locore.h>
#include <machine/md_var.h>

uint64_t counter_freq;
uint64_t cycles_per_tick;
uint64_t cycles_per_usec;
uint64_t cycles_per_sec;
uint64_t cycles_per_hz;

u_int32_t counter_upper = 0;
u_int32_t counter_lower_last = 0;
int	tick_started = 0;

void platform_initclocks(void);

struct clk_ticks
{
	u_long hard_ticks;
	u_long stat_ticks;
	u_long prof_ticks;
	/*
	 * pad for cache line alignment of pcpu info
	 * cache-line-size - number of used bytes
	 */
	char   pad[32-(3*sizeof (u_long))];
} static pcpu_ticks[MAXCPU];

/*
 * Device methods
 */
static int clock_probe(device_t);
static void clock_identify(driver_t *, device_t);
static int clock_attach(device_t);
static unsigned counter_get_timecount(struct timecounter *tc);

static struct timecounter counter_timecounter = {
	counter_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	0xffffffffu,		/* counter_mask */
	0,			/* frequency */
	"MIPS32",		/* name */
	800,			/* quality (adjusted in code) */
};

void 
mips_timer_early_init(uint64_t clock_hz)
{
	/* Initialize clock early so that we can use DELAY sooner */
	counter_freq = clock_hz;
	cycles_per_usec = (clock_hz / (1000 * 1000));
}

void
platform_initclocks(void)
{
	if (!tick_started) {
	        tc_init(&counter_timecounter);
		tick_started++;
	}
}

static uint64_t
tick_ticker(void)
{
	uint64_t ret;
	uint32_t ticktock;

	/*
	 * XXX: MIPS64 platforms can read 64-bits of counter directly.
	 * Also: the tc code is supposed to cope with things wrapping
	 * from the time counter, so I'm not sure why all these hoops
	 * are even necessary.
	 */
	ticktock = mips_rd_count();
	critical_enter();
	if (ticktock < counter_lower_last)
		counter_upper++;
	counter_lower_last = ticktock;
	critical_exit();

	ret = ((uint64_t) counter_upper << 32) | counter_lower_last;
	return (ret);
}

void
mips_timer_init_params(uint64_t platform_counter_freq, int double_count)
{

	/*
	 * XXX: Do not use printf here: uart code 8250 may use DELAY so this
	 * function should  be called before cninit.
	 */
	counter_freq = platform_counter_freq;
	/*
	 * XXX: Some MIPS32 cores update the Count register only every two
	 * pipeline cycles.
	 * We know this because of status registers in CP0, make it automatic.
	 */
	if (double_count != 0)
		counter_freq /= 2;

	cycles_per_tick = counter_freq / 1000;
	cycles_per_hz = counter_freq / hz;
	cycles_per_usec = counter_freq / (1 * 1000 * 1000);
	cycles_per_sec =  counter_freq ;
	
	counter_timecounter.tc_frequency = counter_freq;
	printf("hz=%d cyl_per_hz:%jd cyl_per_usec:%jd freq:%jd cyl_per_hz:%jd cyl_per_sec:%jd\n",
	       hz,
	       cycles_per_tick,
	       cycles_per_usec,
	       counter_freq,
	       cycles_per_hz,
	       cycles_per_sec
	       );
	set_cputicker(tick_ticker, counter_freq, 1);
}

static int
sysctl_machdep_counter_freq(SYSCTL_HANDLER_ARGS)
{
	int error;
	uint64_t freq;

	if (counter_timecounter.tc_frequency == 0)
		return (EOPNOTSUPP);
	freq = counter_freq;
	error = sysctl_handle_int(oidp, &freq, sizeof(freq), req);
	if (error == 0 && req->newptr != NULL) {
		counter_freq = freq;
		counter_timecounter.tc_frequency = counter_freq;
	}
	return (error);
}

SYSCTL_PROC(_machdep, OID_AUTO, counter_freq, CTLTYPE_QUAD | CTLFLAG_RW,
    0, sizeof(u_int), sysctl_machdep_counter_freq, "IU",
    "Timecounter frequency in Hz");

static unsigned
counter_get_timecount(struct timecounter *tc)
{

	return (mips_rd_count());
}


void
cpu_startprofclock(void)
{
	/* nothing to do */
}

void
cpu_stopprofclock(void)
{
	/* nothing to do */
}

/*
 * Wait for about n microseconds (at least!).
 */
void
DELAY(int n)
{
	uint32_t cur, last, delta, usecs;

	/*
	 * This works by polling the timer and counting the number of
	 * microseconds that go by.
	 */
	last = mips_rd_count();
	delta = usecs = 0;

	while (n > usecs) {
		cur = mips_rd_count();

		/* Check to see if the timer has wrapped around. */
		if (cur < last)
			delta += cur + (0xffffffff - last) + 1;
		else
			delta += cur - last;

		last = cur;

		if (delta >= cycles_per_usec) {
			usecs += delta / cycles_per_usec;
			delta %= cycles_per_usec;
		}
	}
}

#ifdef TARGET_OCTEON
int64_t wheel_run = 0;

void octeon_led_run_wheel(void);

#endif
/*
 * Device section of file below
 */
static int
clock_intr(void *arg)
{
	struct clk_ticks *cpu_ticks;
	struct trapframe *tf;
	uint32_t ltick;
	/*
	 * Set next clock edge.
	 */
	ltick = mips_rd_count();
	mips_wr_compare(ltick + cycles_per_tick);
	cpu_ticks = &pcpu_ticks[PCPU_GET(cpuid)];
	critical_enter();
	if (ltick < counter_lower_last) {
		counter_upper++;
		counter_lower_last = ltick;
	}
	/*
	 * Magic.  Setting up with an arg of NULL means we get passed tf.
	 */
	tf = (struct trapframe *)arg;

	/* Fire hardclock at hz. */
	cpu_ticks->hard_ticks += cycles_per_tick;
	if (cpu_ticks->hard_ticks >= cycles_per_hz) {
	        cpu_ticks->hard_ticks -= cycles_per_hz;
		if (PCPU_GET(cpuid) == 0)
			hardclock(USERMODE(tf->sr), tf->pc);
		else
			hardclock_cpu(USERMODE(tf->sr));
	}
	/* Fire statclock at stathz. */
	cpu_ticks->stat_ticks += stathz;
	if (cpu_ticks->stat_ticks >= cycles_per_hz) {
		cpu_ticks->stat_ticks -= cycles_per_hz;
		statclock(USERMODE(tf->sr));
	}

	/* Fire profclock at profhz, but only when needed. */
	cpu_ticks->prof_ticks += profhz;
	if (cpu_ticks->prof_ticks >= cycles_per_hz) {
		cpu_ticks->prof_ticks -= cycles_per_hz;
		if (profprocs != 0)
			profclock(USERMODE(tf->sr), tf->pc);
	}
	critical_exit();
#ifdef TARGET_OCTEON
	/* Run the FreeBSD display once every hz ticks  */
	wheel_run += cycles_per_tick;
	if (wheel_run >= cycles_per_sec) {
		wheel_run = 0;
		octeon_led_run_wheel();
	}
#endif
	return (FILTER_HANDLED);
}

static int
clock_probe(device_t dev)
{

	if (device_get_unit(dev) != 0)
		panic("can't attach more clocks");

	device_set_desc(dev, "Generic MIPS32 ticker");
	return (0);
}

static void
clock_identify(driver_t * drv, device_t parent)
{

	BUS_ADD_CHILD(parent, 0, "clock", 0);
}

static int
clock_attach(device_t dev)
{
	struct resource *irq;
	int error;
	int rid;

	rid = 0;
	irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 5, 5, 1, RF_ACTIVE);
	if (irq == NULL) {
		device_printf(dev, "failed to allocate irq\n");
		return (ENXIO);
	}
	error = bus_setup_intr(dev, irq, INTR_TYPE_CLK, clock_intr, NULL,
	    NULL, NULL);

	if (error != 0) {
		device_printf(dev, "bus_setup_intr returned %d\n", error);
		return (error);
	}

	mips_wr_compare(mips_rd_count() + counter_freq / hz);
	return (0);
}

static device_method_t clock_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, clock_probe),
	DEVMETHOD(device_identify, clock_identify),
	DEVMETHOD(device_attach, clock_attach),
	DEVMETHOD(device_detach, bus_generic_detach),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),

	{0, 0}
};

static driver_t clock_driver = {
	"clock", clock_methods, 32
};

static devclass_t clock_devclass;

DRIVER_MODULE(clock, nexus, clock_driver, clock_devclass, 0, 0);
