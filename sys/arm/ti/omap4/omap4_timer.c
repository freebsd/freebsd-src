/*-
 * Copyright (c) 2011
 *	Ben Gray <ben.r.gray@gmail.com>.
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
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BEN GRAY ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BEN GRAY BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/time.h>
#include <sys/timeet.h>
#include <sys/bus.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/timetc.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/frame.h>
#include <machine/resource.h>
#include <machine/intr.h>

#include <arm/ti/omap4/omap4var.h>
#include <arm/ti/omap_prcm.h>

#include <arm/ti/omap4/omap4_reg.h>

#include "omap4_if.h"

/**
 *	After writing the GPTIMER driver I began thinking that these are probably
 *	not the best timers to use for the tick timer and time count stuff, rather  
 *	perhaps it's a better idea to use the timers built into the A9 core.
 *
 *	My reasoning is that the GPTIMER driver really should (and currently does)
 *	take a mutex to protect access to the timer structure when reading the time
 *	counter, and this may not be a good idea within system callback functions
 *	like timecounter.  However that said the locks are currently SPIN_MTX
 *	types so I think this *shouldn't* be a problem.
 *
 *	Regardless I think it is probably better to just use the A9 Core global
 *	timer and be done with it.
 *
 *
 *	Current System Timer Design
 *	===========================
 *
 *		Tick Timer  => uses =>  Core 0 Private timer
 *	    Timecount   => uses =>  ARM Global timer 
 *
 */



/* Private (per-CPU) timer register map */
#define PRV_TIMER_LOAD                 0x0000
#define PRV_TIMER_COUNT                0x0004
#define PRV_TIMER_CTRL                 0x0008
#define PRV_TIMER_INTR                 0x000C

#define PRV_TIMER_IRQ_NUM              29

#define PRV_TIMER_CTR_PRESCALER_SHIFT  8
#define PRV_TIMER_CTRL_IRQ_ENABLE      (1UL << 2)
#define PRV_TIMER_CTRL_AUTO_RELOAD     (1UL << 1)
#define PRV_TIMER_CTRL_TIMER_ENABLE    (1UL << 0)

#define PRV_TIMER_INTR_EVENT           (1UL << 0)

/* Global timer register map */
#define GBL_TIMER_COUNT_LOW            0x0000
#define GBL_TIMER_COUNT_HIGH           0x0004
#define GBL_TIMER_CTRL                 0x0008
#define GBL_TIMER_INTR                 0x000C

#define GBL_TIMER_IRQ_NUM              27

#define GBL_TIMER_CTR_PRESCALER_SHIFT  8
#define GBL_TIMER_CTRL_AUTO_INC        (1UL << 3)
#define GBL_TIMER_CTRL_IRQ_ENABLE      (1UL << 2)
#define GBL_TIMER_CTRL_COMP_ENABLE     (1UL << 1)
#define GBL_TIMER_CTRL_TIMER_ENABLE    (1UL << 0)

#define GBL_TIMER_INTR_EVENT           (1UL << 0)

static device_t omap4_dev;

/**
 *	omap4_timer_get_timecount - returns the count in GPTIMER11, the system counter
 *	@tc: pointer to the timecounter structure used to register the callback
 *
 *	
 *
 *	RETURNS:
 *	the value in the counter
 */
static unsigned
omap4_timer_get_timecount(struct timecounter *tc)
{
	/* We only read the lower 32-bits, the timecount stuff only uses 32-bits
	 * so (for now?) ignore the upper 32-bits.
	 */
	return (OMAP4_GBL_TIMER_READ(omap4_dev, GBL_TIMER_COUNT_LOW));
}

static struct timecounter omap4_timecounter = {
	.tc_get_timecount  = omap4_timer_get_timecount,	/* get_timecount */
	.tc_poll_pps       = NULL,			/* no poll_pps */
	.tc_counter_mask   = ~0u,			/* counter_mask */
	.tc_frequency      = 0,				/* frequency */
	.tc_name           = "OMAP4 Timer",		/* name */
	.tc_quality        = 1000,			/* quality */
};

static struct eventtimer omap4_eventtimer;

/**
 *	omap4_clk_intr - 
 *
 *	Interrupt handler for the private interrupt.
 *
 *	RETURNS:
 *	nothing
 */
static int
omap4_clk_intr(void *arg)
{
	struct trapframe *frame = arg;

	OMAP4_PRV_TIMER_WRITE(omap4_dev,
	                  PRV_TIMER_INTR, PRV_TIMER_INTR_EVENT);
	
	hardclock(TRAPF_USERMODE(frame), TRAPF_PC(frame));
	return (FILTER_HANDLED);
}

static int
omap4_timer_start(struct eventtimer *et,
    struct bintime *first, struct bintime *period)
{

	return (0);
}

static int
omap4_timer_stop(struct eventtimer *et)
{

	return (0);
}

/*
 * omap4_init_timer - 
 * setup interrupt and device instance that manages access to registers
 */

void
omap4_init_timer(device_t dev)
{
	struct resource *irq;
	int rid = 0;
	void *ihl;

	omap4_dev = dev;

	/* Register an interrupt handler for general purpose timer 10 */
	irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid,
	    PRV_TIMER_IRQ_NUM, PRV_TIMER_IRQ_NUM, 1, RF_ACTIVE);
	if (!irq)
		panic("Unable to setup the clock irq handler.\n");
	else
		bus_setup_intr(dev, irq, INTR_TYPE_CLK,
		    omap4_clk_intr, NULL, NULL, &ihl);

}

/**
 *	cpu_initclocks - function called by the system in init the tick clock/timer
 *
 *	This is where both the timercount and system ticks timer are started.
 *
 *	RETURNS:
 *	nothing
 */
void
cpu_initclocks(void)
{
	u_int oldirqstate;
	unsigned int mpuclk_freq;
	unsigned int periphclk_freq;
	unsigned int prescaler;
	uint64_t load64;

	/* First ensure we have what we need */
	if (omap4_dev == NULL)
		panic("omap4xx device is not set before enabling global timer\n");


	oldirqstate = disable_interrupts(I32_bit);

	/* Ensure the timer is disabled before we start playing with it */
	OMAP4_PRV_TIMER_WRITE(omap4_dev, PRV_TIMER_CTRL, 0x00000000);
	

	/* Next get the "PERIPHCLK" freq - which I take to be the "LOCAL_INTCNT_FCLK"
	 * refered to in the OMAP44xx datasheet. The "LOCAL_INTCNT_FCLK" is half
	 * the ARM_FCLK.
	 */
	if (omap_prcm_clk_get_source_freq(MPU_CLK, &mpuclk_freq) != 0)
		panic("Failed to get the SYSCLK frequency\n");
	
	periphclk_freq = mpuclk_freq / 2;

	/* The timer counts down from a 'load' value to zero, when it reaches zero
	 * it loads the 'load' value back in and starts counting down again.
	 *
	 * So we need to calculate the load value, the larger the load the value
	 * the better the accuracy.
	 */
	for (prescaler = 1; prescaler < 255; prescaler++) {
		load64 = ((uint64_t)periphclk_freq / (uint64_t)hz) / (uint64_t)prescaler;
		if (load64 <= 0xFFFFFFFFULL) {
			break;
		}
	}
	
	if (prescaler == 255)
		panic("Couldn't fit timer tick in private counter\n");

	/* Set the load value */
	OMAP4_PRV_TIMER_WRITE(omap4_dev, PRV_TIMER_LOAD, (uint32_t)load64);
	OMAP4_PRV_TIMER_WRITE(omap4_dev, PRV_TIMER_COUNT, (uint32_t)load64);
	
	/* Setup and enable the timer */
	OMAP4_PRV_TIMER_WRITE(omap4_dev, PRV_TIMER_CTRL, 
	                       ((prescaler - 1) << PRV_TIMER_CTR_PRESCALER_SHIFT) | 
	                       PRV_TIMER_CTRL_IRQ_ENABLE |
	                       PRV_TIMER_CTRL_AUTO_RELOAD |
	                       PRV_TIMER_CTRL_TIMER_ENABLE);
	
	
	/* Setup and enable the global timer to use as the timecounter */
	OMAP4_GBL_TIMER_WRITE(omap4_dev, GBL_TIMER_CTRL, 
	                       (0x00 << GBL_TIMER_CTR_PRESCALER_SHIFT) | 
	                       GBL_TIMER_CTRL_TIMER_ENABLE);
	
		
	/* Save the system clock speed */
	omap4_timecounter.tc_frequency = periphclk_freq;

	/* Setup the time counter */
	tc_init(&omap4_timecounter);

        omap4_eventtimer.et_name = "OMAP4 Event Timer";
        omap4_eventtimer.et_flags = ET_FLAGS_PERIODIC | ET_FLAGS_ONESHOT;
        omap4_eventtimer.et_quality = 1000;

        omap4_eventtimer.et_frequency = 1000; /* XXX */
        omap4_eventtimer.et_min_period.sec = 0;
        omap4_eventtimer.et_min_period.frac =
            ((0x00000002LLU << 32) / omap4_eventtimer.et_frequency) << 32;
        omap4_eventtimer.et_max_period.sec = 0xfffffff0U / omap4_eventtimer.et_frequency;
        omap4_eventtimer.et_max_period.frac =
            ((0xfffffffeLLU << 32) / omap4_eventtimer.et_frequency) << 32;
        omap4_eventtimer.et_start = omap4_timer_start;
        omap4_eventtimer.et_stop = omap4_timer_stop;
        omap4_eventtimer.et_priv = NULL;
        et_register(&omap4_eventtimer);
	
	restore_interrupts(oldirqstate);
	
	cpu_initclocks_bsp();

#if 0
	unsigned int sysclk_freq;

	/* Number of microseconds between interrupts */
	tick = 1000000 / hz;

	/* Next setup one of the timers to be the system tick timer */
	if (omap_timer_activate(TICKTIMER_GPTIMER, OMAP_TIMER_SYSTICK_FLAG, tick,
	                        NULL, NULL)) {
		panic("Error: failed to activate system tick timer\n");
	} else if (omap_timer_start(TICKTIMER_GPTIMER)) {
		panic("Error: failed to start system tick timer\n");
	}


	/* Enable the ARM Coretex-A9 core global timer, this is used for calculating
	 * delay times in the DELAY() function ... and as the system timecount
	 * value ??
	 */
	


	/* Setup another timer to be the timecounter */
	if (omap_timer_activate(TIMECOUNT_GPTIMER, OMAP_TIMER_PERIODIC_FLAG, 0,
	                        NULL, NULL)) {
		printf("Error: failed to activate system tick timer\n");
	} else if (omap_timer_start(TIMECOUNT_GPTIMER)) {
		printf("Error: failed to start system tick timer\n");
	}

	/* Get the SYS_CLK frequency, this is the freq of the timer tick */
	if (omap_prcm_clk_get_source_freq(SYS_CLK, &sysclk_freq) != 0)
		panic("Failed to get the SYSCLK frequency\n");
	
	/* Save the system clock speed */
	omap4_timecounter.tc_frequency = sysclk_freq;

	/* Setup the time counter */
	tc_init(&omap4_timecounter);

#endif

}

/**
 *	DELAY - Delay for at least N microseconds.
 *	@n: number of microseconds to delay by
 *
 *	This function is called all over the kernel and is suppose to provide a
 *	consistent delay.  It is a busy loop and blocks polling a timer when called.
 *
 *	RETURNS:
 *	nothing
 */
void
DELAY(int n)
{
	int32_t counts_per_usec;
	int32_t counts;
	uint32_t first, last;
	
	if (n <= 0)
		return;
	
	/* Check the timers are setup, if not just use a for loop for the meantime */
	if (omap4_timecounter.tc_frequency == 0) {

		/* If the CPU clock speed is defined we use that via the 'cycle count'
		 * register, this should give us a pretty accurate delay value.  If not
		 * defined we use a basic for loop with a very simply calculation.
		 */
#if defined(CPU_CLOCKSPEED)
		counts_per_usec = (CPU_CLOCKSPEED / 1000000);
		counts = counts_per_usec * 1000;
		
		__asm __volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (first));
		while (counts > 0) {
			__asm __volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (last));
			counts -= (int32_t)(last - first);
			first = last;
		}
#else
		uint32_t val;
		for (; n > 0; n--)
			for (val = 200; val > 0; val--)
				cpufunc_nullop(); /* 
						   * Prevent gcc from
						   * optimizing out the
						   * loop
						   */
						     
#endif		
		return;
	}
	
	/* Get the number of times to count */
	counts_per_usec = ((omap4_timecounter.tc_frequency / 1000000) + 1);

	/*
	 * Clamp the timeout at a maximum value (about 32 seconds with
	 * a 66MHz clock). *Nobody* should be delay()ing for anywhere
	 * near that length of time and if they are, they should be hung
	 * out to dry.
	 */
	if (n >= (0x80000000U / counts_per_usec))
		counts = (0x80000000U / counts_per_usec) - 1;
	else
		counts = n * counts_per_usec;
	
	first = OMAP4_GBL_TIMER_READ(omap4_dev, GBL_TIMER_COUNT_LOW);
	
	while (counts > 0) {
		last = OMAP4_GBL_TIMER_READ(omap4_dev, GBL_TIMER_COUNT_LOW);
		counts -= (int32_t)(last - first);
		first = last;
	}
}
