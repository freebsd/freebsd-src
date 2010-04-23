/*-
 * Copyright (c) 2003-2009 RMI Corporation
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
 * 3. Neither the name of RMI Corporation, nor the names of its contributors,
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * RMI_BSD 
 */

#include <sys/cdefs.h>		/* RCS ID & Copyright macro defns */
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/timetc.h>

#include <sys/module.h>
#include <sys/stdint.h>

#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/systm.h>
#include <sys/clock.h>

#include <machine/clock.h>
#include <machine/md_var.h>
#include <machine/hwfunc.h>
#include <machine/intr_machdep.h>

#include <mips/rmi/iomap.h>
#include <mips/rmi/clock.h>
#include <mips/rmi/interrupt.h>
#include <mips/rmi/pic.h>
#include <mips/rmi/shared_structs.h>

#ifdef XLR_PERFMON
#include <mips/rmi/perfmon.h>
#endif

uint64_t counter_freq;
uint64_t cycles_per_tick;
uint64_t cycles_per_usec;
uint64_t cycles_per_sec;
uint64_t cycles_per_hz;

u_int32_t counter_upper = 0;
u_int32_t counter_lower_last = 0;

#define STAT_PROF_CLOCK_SCALE_FACTOR 8

static int scale_factor;
static int count_scale_factor[32];

uint64_t
platform_get_frequency()
{
	return XLR_PIC_HZ;
}

void
mips_timer_early_init(uint64_t clock_hz)
{
	/* Initialize clock early so that we can use DELAY sooner */
	counter_freq = clock_hz;
	cycles_per_usec = (clock_hz / (1000 * 1000));

}

/*
* count_compare_clockhandler:
*
* Handle the clock interrupt when count becomes equal to
* compare.
*/
int
count_compare_clockhandler(struct trapframe *tf)
{
	int cpu = PCPU_GET(cpuid);
	uint32_t cycles;

	critical_enter();

	if (cpu == 0) {
		mips_wr_compare(0);
	} else {
		count_scale_factor[cpu]++;
		cycles = mips_rd_count();
		cycles += XLR_CPU_HZ / hz;
		mips_wr_compare(cycles);

		hardclock_cpu(TRAPF_USERMODE(tf));
		if (count_scale_factor[cpu] == STAT_PROF_CLOCK_SCALE_FACTOR) {
			statclock(TRAPF_USERMODE(tf));
			if (profprocs != 0) {
				profclock(TRAPF_USERMODE(tf), tf->pc);
			}
			count_scale_factor[cpu] = 0;
		}
		/* If needed , handle count compare tick skew here */
	}

	critical_exit();
	return (FILTER_HANDLED);
}

unsigned long clock_tick_foo=0;

int
pic_hardclockhandler(struct trapframe *tf)
{
	int cpu = PCPU_GET(cpuid);

	critical_enter();

	if (cpu == 0) {
		scale_factor++;
		clock_tick_foo++;
/*
		if ((clock_tick_foo % 10000) == 0) {
			printf("Clock tick foo at %ld\n", clock_tick_foo);
		}
*/
		hardclock(TRAPF_USERMODE(tf), tf->pc);
		if (scale_factor == STAT_PROF_CLOCK_SCALE_FACTOR) {
			statclock(TRAPF_USERMODE(tf));
			if (profprocs != 0) {
				profclock(TRAPF_USERMODE(tf), tf->pc);
			}
			scale_factor = 0;
		}
#ifdef XLR_PERFMON
		if (xlr_perfmon_started)
			xlr_perfmon_clockhandler();
#endif

	} else {
		/* If needed , handle count compare tick skew here */
	}
	critical_exit();
	return (FILTER_HANDLED);
}

int
pic_timecounthandler(struct trapframe *tf)
{
	return (FILTER_HANDLED);
}

void
rmi_early_counter_init()
{
	int cpu = PCPU_GET(cpuid);
	xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_PIC_OFFSET);

	/*
	 * We do this to get the PIC time counter running right after system
	 * start. Otherwise the DELAY() function will not be able to work
	 * since it won't have a TC to read.
	 */
	xlr_write_reg(mmio, PIC_TIMER_6_MAXVAL_0, (0xffffffff & 0xffffffff));
	xlr_write_reg(mmio, PIC_TIMER_6_MAXVAL_1, (0xffffffff & 0xffffffff));
	xlr_write_reg(mmio, PIC_IRT_0_TIMER_6, (1 << cpu));
	xlr_write_reg(mmio, PIC_IRT_1_TIMER_6, (1 << 31) | (0 << 30) | (1 << 6) | (PIC_TIMER_6_IRQ));
	pic_update_control(1 << (8 + 6), 0);
}

void tick_init(void);

void
platform_initclocks(void)
{
	int cpu = PCPU_GET(cpuid);
	void *cookie;

	/*
	 * Note: Passing #3 as NULL ensures that clockhandler gets called
	 * with trapframe
	 */
	/* profiling/process accounting timer interrupt for non-zero cpus */
	cpu_establish_hardintr("compare",
	    (driver_filter_t *) count_compare_clockhandler,
	    NULL,
	    NULL,
	    IRQ_TIMER,
	    INTR_TYPE_CLK | INTR_FAST, &cookie);

	/* timekeeping timer interrupt for cpu 0 */
	cpu_establish_hardintr("hardclk",
	    (driver_filter_t *) pic_hardclockhandler,
	    NULL,
	    NULL,
	    PIC_TIMER_7_IRQ,
	    INTR_TYPE_CLK | INTR_FAST,
	    &cookie);

	/* this is used by timecounter */
	cpu_establish_hardintr("timecount",
	    (driver_filter_t *) pic_timecounthandler, NULL,
	    NULL, PIC_TIMER_6_IRQ, INTR_TYPE_CLK | INTR_FAST,
	    &cookie);

	if (cpu == 0) {
		__uint64_t maxval = XLR_PIC_HZ / hz;
		xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_PIC_OFFSET);

		stathz = hz / STAT_PROF_CLOCK_SCALE_FACTOR;
		profhz = stathz;

		/* Setup PIC Interrupt */

		if (rmi_spin_mutex_safe)
			mtx_lock_spin(&xlr_pic_lock);
		xlr_write_reg(mmio, PIC_TIMER_7_MAXVAL_0, (maxval & 0xffffffff));	/* 0x100 + 7 */
		xlr_write_reg(mmio, PIC_TIMER_7_MAXVAL_1, (maxval >> 32) & 0xffffffff);	/* 0x110 + 7 */
		/* 0x40 + 8 */
		/* reg 40 is lower bits 31-0  and holds CPU mask */
		xlr_write_reg(mmio, PIC_IRT_0_TIMER_7, (1 << cpu));
		/* 0x80 + 8 */
		/* Reg 80 is upper bits 63-32 and holds                              */
		/* Valid   Edge    Local    IRQ */
		xlr_write_reg(mmio, PIC_IRT_1_TIMER_7, (1 << 31) | (0 << 30) | (1 << 6) | (PIC_TIMER_7_IRQ));

		pic_update_control(1 << (8 + 7), 1);
		xlr_write_reg(mmio, PIC_TIMER_6_MAXVAL_0, (0xffffffff & 0xffffffff));
		xlr_write_reg(mmio, PIC_TIMER_6_MAXVAL_1, (0xffffffff & 0xffffffff));
		xlr_write_reg(mmio, PIC_IRT_0_TIMER_6, (1 << cpu));
		xlr_write_reg(mmio, PIC_IRT_1_TIMER_6, (1 << 31) | (0 << 30) | (1 << 6) | (PIC_TIMER_6_IRQ));
		pic_update_control(1 << (8 + 6), 1);
		if (rmi_spin_mutex_safe)
			mtx_unlock_spin(&xlr_pic_lock);
	} else {
		/* Setup count-compare interrupt for vcpu[1-31] */
		mips_wr_compare((xlr_boot1_info.cpu_frequency) / hz);
	}
	tick_init();
}

unsigned
__attribute__((no_instrument_function))
platform_get_timecount(struct timecounter *tc __unused)
{
	xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_PIC_OFFSET);

	return 0xffffffffU - xlr_read_reg(mmio, PIC_TIMER_6_COUNTER_0);
}

void
DELAY(int n)
{
	uint32_t cur, last, delta, usecs;

	/*
	 * This works by polling the timer and counting the number of
	 * microseconds that go by.
	 */
	last = platform_get_timecount(NULL);
	delta = usecs = 0;

	while (n > usecs) {
		cur = platform_get_timecount(NULL);

		/* Check to see if the timer has wrapped around. */
		if (cur < last)
			delta += (cur + (cycles_per_hz - last));
		else
			delta += (cur - last);

		last = cur;

		if (delta >= cycles_per_usec) {
			usecs += delta / cycles_per_usec;
			delta %= cycles_per_usec;
		}
	}
}

static
uint64_t
read_pic_counter(void)
{
	xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_PIC_OFFSET);
	uint32_t lower, upper;
	uint64_t tc;

	/*
	 * Pull the value of the 64 bit counter which is stored in PIC
	 * register 120+N and 130+N
	 */
	upper = 0xffffffffU - xlr_read_reg(mmio, PIC_TIMER_6_COUNTER_1);
	lower = 0xffffffffU - xlr_read_reg(mmio, PIC_TIMER_6_COUNTER_0);
	tc = (((uint64_t) upper << 32) | (uint64_t) lower);
	return (tc);
}

extern struct timecounter counter_timecounter;

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
	 */
	if (double_count != 0)
		counter_freq /= 2;

	cycles_per_tick = counter_freq / 1000;
	cycles_per_hz = counter_freq / hz;
	cycles_per_usec = counter_freq / (1 * 1000 * 1000);
	cycles_per_sec = counter_freq;

	counter_timecounter.tc_frequency = counter_freq;
	printf("hz=%d cyl_per_hz:%jd cyl_per_usec:%jd freq:%jd cyl_per_hz:%jd cyl_per_sec:%jd\n",
	    hz,
	    cycles_per_tick,
	    cycles_per_usec,
	    counter_freq,
	    cycles_per_hz,
	    cycles_per_sec
	    );
	set_cputicker(read_pic_counter, counter_freq, 1);
}
