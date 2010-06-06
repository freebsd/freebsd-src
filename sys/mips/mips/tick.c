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

#include <machine/hwfunc.h>
#include <machine/clock.h>
#include <machine/locore.h>
#include <machine/md_var.h>

uint64_t counter_freq;

struct timecounter *platform_timecounter;

static uint64_t cycles_per_tick;
static uint64_t cycles_per_usec;

static u_int32_t counter_upper = 0;
static u_int32_t counter_lower_last = 0;

static DPCPU_DEFINE(uint32_t, compare_ticks);
static DPCPU_DEFINE(uint32_t, lost_ticks);

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

	tc_init(&counter_timecounter);

	if (platform_timecounter != NULL)
		tc_init(platform_timecounter);
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

	/*
	 * We want to run stathz in the neighborhood of 128hz.  We would
	 * like profhz to run as often as possible, so we let it run on
	 * each clock tick.  We try to honor the requested 'hz' value as
	 * much as possible.
	 *
	 * If 'hz' is above 1500, then we just let the timer
	 * (and profhz) run at hz.  If 'hz' is below 1500 but above
	 * 750, then we let the timer run at 2 * 'hz'.  If 'hz'
	 * is below 750 then we let the timer run at 4 * 'hz'.
	 */
	if (hz >= 1500)
		timer1hz = hz;
	else if (hz >= 750)
		timer1hz = hz * 2;
	else
		timer1hz = hz * 4;

	if (timer1hz < 128)
		stathz = timer1hz;
	else
		stathz = timer1hz / (timer1hz / 128);
	profhz = timer1hz;

	cycles_per_tick = counter_freq / timer1hz;
	cycles_per_usec = counter_freq / (1 * 1000 * 1000);
	
	counter_timecounter.tc_frequency = counter_freq;
	printf("hz=%d timer1hz:%d cyl_per_tick:%jd cyl_per_usec:%jd freq:%jd\n",
	       hz,
	       timer1hz,
	       cycles_per_tick,
	       cycles_per_usec,
	       counter_freq);
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

/*
 * Device section of file below
 */
static int
clock_intr(void *arg)
{
	struct trapframe *tf;
	uint32_t count, compare_last, compare_next, lost_ticks;

	/*
	 * Set next clock edge.
	 */
	count = mips_rd_count();
	compare_last = DPCPU_GET(compare_ticks);
	compare_next = count + cycles_per_tick;
	DPCPU_SET(compare_ticks, compare_next);
	mips_wr_compare(compare_next);

	critical_enter();
	if (count < counter_lower_last) {
		counter_upper++;
		counter_lower_last = count;
	}

	/*
	 * Magic.  Setting up with an arg of NULL means we get passed tf.
	 */
	tf = (struct trapframe *)arg;

	/*
	 * Account for the "lost time" between when the timer interrupt fired
	 * and when 'clock_intr' actually started executing.
	 */
	lost_ticks = DPCPU_GET(lost_ticks);
	lost_ticks += count - compare_last;

	/*
	 * If the COUNT and COMPARE registers are no longer in sync then make
	 * up some reasonable value for the 'lost_ticks'.
	 *
	 * This could happen, for e.g., after we resume normal operations after
	 * exiting the debugger.
	 */
	if (lost_ticks > 2 * cycles_per_tick)
		lost_ticks = cycles_per_tick;

	while (lost_ticks >= cycles_per_tick) {
		timer1clock(TRAPF_USERMODE(tf), tf->pc);
		lost_ticks -= cycles_per_tick;
	}
	DPCPU_SET(lost_ticks, lost_ticks);

#ifdef KDTRACE_HOOKS
	/*
	 * If the DTrace hooks are configured and a callback function
	 * has been registered, then call it to process the high speed
	 * timers.
	 */
	int cpu = PCPU_GET(cpuid);
	if (cyclic_clock_func[cpu] != NULL)
		(*cyclic_clock_func[cpu])(tf);
#endif
	timer1clock(TRAPF_USERMODE(tf), tf->pc);
	critical_exit();
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
