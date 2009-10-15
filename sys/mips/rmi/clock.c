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
 * RMI_BSD */

#include <sys/cdefs.h>      /* RCS ID & Copyright macro defns */

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

#include <mips/xlr/iomap.h>
#include <mips/xlr/clock.h>
#include <mips/xlr/interrupt.h>
#include <mips/xlr/pic.h>
#include <mips/xlr/shared_structs.h>

#ifdef XLR_PERFMON
#include <mips/xlr/perfmon.h>
#endif

int hw_clockrate;
SYSCTL_INT(_hw, OID_AUTO, clockrate, CTLFLAG_RD, &hw_clockrate,
		 0, "CPU instruction clock rate");

#define STAT_PROF_CLOCK_SCALE_FACTOR 8

static int scale_factor;
static int count_scale_factor[32];

uint64_t platform_get_frequency()
{
	return XLR_PIC_HZ;
}

/*
* count_compare_clockhandler:
*
* Handle the clock interrupt when count becomes equal to 
* compare.
*/
void
count_compare_clockhandler(struct trapframe *tf)
{
	int cpu = PCPU_GET(cpuid);
	uint32_t cycles;

	critical_enter();

	if (cpu == 0) {
		mips_wr_compare(0);
	}
	else {
		count_scale_factor[cpu]++;
		cycles = mips_rd_count();
		cycles += XLR_CPU_HZ/hz;
		mips_wr_compare(cycles);

		hardclock_process((struct clockframe *)tf);
		if (count_scale_factor[cpu] == STAT_PROF_CLOCK_SCALE_FACTOR) {
			statclock((struct clockframe *)tf);
			if(profprocs != 0) {
				profclock((struct clockframe *)tf);
			}
			count_scale_factor[cpu] = 0;
		}

	/* If needed , handle count compare tick skew here */
	}

	critical_exit();
}

void
pic_hardclockhandler(struct trapframe *tf)
{
	int cpu = PCPU_GET(cpuid);

	critical_enter();

	if (cpu == 0) {
		scale_factor++;
		hardclock((struct clockframe *)tf);
		if (scale_factor == STAT_PROF_CLOCK_SCALE_FACTOR) {
			statclock((struct clockframe *)tf);
			if(profprocs != 0) {
				profclock((struct clockframe *)tf);
			}
			scale_factor = 0;
		}
#ifdef XLR_PERFMON
		if (xlr_perfmon_started)
			xlr_perfmon_clockhandler(); 
#endif

	}
	else {
		/* If needed , handle count compare tick skew here */
	}

	critical_exit();
}

void
pic_timecounthandler(struct trapframe *tf)
{
}

void
platform_initclocks(void)
{
	int cpu = PCPU_GET(cpuid);
	void *cookie;

	/* Note: Passing #3 as NULL ensures that clockhandler 
	* gets called with trapframe 
	*/
	/* profiling/process accounting timer interrupt for non-zero cpus */
	cpu_establish_intr("compare", IRQ_TIMER, 
		(driver_intr_t *)count_compare_clockhandler,
		NULL, INTR_TYPE_CLK|INTR_FAST, &cookie, NULL, NULL);

	/* timekeeping timer interrupt for cpu 0 */
	cpu_establish_intr("hardclk", PIC_TIMER_7_IRQ,
		(driver_intr_t *)pic_hardclockhandler,
		NULL,  INTR_TYPE_CLK|INTR_FAST, &cookie, NULL, NULL);

	/* this is used by timecounter */
	cpu_establish_intr("timecount", PIC_TIMER_6_IRQ,
		(driver_intr_t *)pic_timecounthandler,
		NULL, INTR_TYPE_CLK|INTR_FAST, &cookie, NULL, NULL);

	if (cpu == 0) {
		__uint64_t maxval = XLR_PIC_HZ/hz;
		xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_PIC_OFFSET);

		stathz = hz / STAT_PROF_CLOCK_SCALE_FACTOR;
		profhz = stathz;

		/* Setup PIC Interrupt */

		mtx_lock_spin(&xlr_pic_lock);
		xlr_write_reg(mmio, PIC_TIMER_7_MAXVAL_0, (maxval & 0xffffffff));
		xlr_write_reg(mmio, PIC_TIMER_7_MAXVAL_1, (maxval >> 32) & 0xffffffff);
		xlr_write_reg(mmio, PIC_IRT_0_TIMER_7, (1 << cpu));
		xlr_write_reg(mmio, PIC_IRT_1_TIMER_7, (1<<31)|(0<<30)|(1<<6)|(PIC_TIMER_7_IRQ));
		pic_update_control(1<<(8+7));

		xlr_write_reg(mmio, PIC_TIMER_6_MAXVAL_0, (0xffffffff & 0xffffffff));
		xlr_write_reg(mmio, PIC_TIMER_6_MAXVAL_1, (0x0) & 0xffffffff);
		xlr_write_reg(mmio, PIC_IRT_0_TIMER_6, (1 << cpu));
		xlr_write_reg(mmio, PIC_IRT_1_TIMER_6, (1<<31)|(0<<30)|(1<<6)|(PIC_TIMER_6_IRQ));
		pic_update_control(1<<(8+6));
		mtx_unlock_spin(&xlr_pic_lock);
	} else {
		/* Setup count-compare interrupt for vcpu[1-31] */
		mips_wr_compare((xlr_boot1_info.cpu_frequency)/hz);
	}
}



unsigned __attribute__((no_instrument_function))
platform_get_timecount(struct timecounter *tc)
{
	xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_PIC_OFFSET);

	return 0xffffffffU - xlr_read_reg(mmio, PIC_TIMER_6_COUNTER_0);
}
