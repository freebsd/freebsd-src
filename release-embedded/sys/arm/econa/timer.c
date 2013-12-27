/*-
 * Copyright (c) 2009 Yohanes Nugroho <yohanes@gmail.com>.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/timetc.h>
#include <sys/watchdog.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include "econa_reg.h"
#include "econa_var.h"

#define	INITIAL_TIMECOUNTER	(0xffffffff)

static int timers_initialized = 0;

#define	HZ	100

extern unsigned int CPU_clock;
extern unsigned int AHB_clock;
extern unsigned int APB_clock;

static unsigned long timer_counter = 0;

struct ec_timer_softc {
	struct resource	*	timer_res[3];
	bus_space_tag_t		timer_bst;
	bus_space_handle_t	timer_bsh;
	struct mtx		timer_mtx;
};

static struct resource_spec ec_timer_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		1,	RF_ACTIVE },
	{ -1, 0 }
};

static unsigned ec_timer_get_timecount(struct timecounter *);

static struct timecounter ec_timecounter = {
	.tc_get_timecount = ec_timer_get_timecount,
	.tc_name = "CPU Timer",
	/* This is assigned on the fly in the init sequence */
	.tc_frequency = 0,
	.tc_counter_mask = ~0u,
	.tc_quality = 1000,
};

static struct ec_timer_softc *timer_softc = NULL;

static inline
void write_4(unsigned int val, unsigned int addr)
{
	bus_space_write_4(timer_softc->timer_bst,
			  timer_softc->timer_bsh, addr, val);

}

static inline
unsigned int read_4(unsigned int addr)
{

	return bus_space_read_4(timer_softc->timer_bst,
	    timer_softc->timer_bsh, addr);
}

#define	uSECS_PER_TICK	(1000000 / APB_clock)
#define	TICKS2USECS(x)	((x) * uSECS_PER_TICK)

static unsigned
read_timer_counter_noint(void)
{

	arm_mask_irq(0);
	unsigned int v = read_4(TIMER_TM1_COUNTER_REG);
	arm_unmask_irq(0);
	return v;
}

void
DELAY(int usec)
{
	uint32_t val, val_temp;
	int nticks;

	if (!timers_initialized) {
		for (; usec > 0; usec--)
			for (val = 100; val > 0; val--)
				;
		return;
	}

	val = read_timer_counter_noint();
	nticks = (((APB_clock / 1000) * usec) / 1000) + 100;

	while (nticks > 0) {
		val_temp = read_timer_counter_noint();
		if (val > val_temp)
			nticks -= (val - val_temp);
		else
			nticks -= (val + (timer_counter - val_temp));

		val = val_temp;
	}

}

/*
 * Setup timer
 */
static inline void
setup_timer(unsigned int counter_value)
{
	unsigned int control_value;
	unsigned int mask_value;

	control_value = read_4(TIMER_TM_CR_REG);

	mask_value = read_4(TIMER_TM_INTR_MASK_REG);
	write_4(counter_value, TIMER_TM1_COUNTER_REG);
	write_4(counter_value, TIMER_TM1_LOAD_REG);
	write_4(0, TIMER_TM1_MATCH1_REG);
	write_4(0,TIMER_TM1_MATCH2_REG);

	control_value &= ~(TIMER1_CLOCK_SOURCE);
	control_value |= TIMER1_UP_DOWN_COUNT;

	write_4(0, TIMER_TM2_COUNTER_REG);
	write_4(0, TIMER_TM2_LOAD_REG);
	write_4(~0u, TIMER_TM2_MATCH1_REG);
	write_4(~0u,TIMER_TM2_MATCH2_REG);

	control_value &= ~(TIMER2_CLOCK_SOURCE);
	control_value &= ~(TIMER2_UP_DOWN_COUNT);

	mask_value &= ~(63);

	write_4(control_value, TIMER_TM_CR_REG);
	write_4(mask_value, TIMER_TM_INTR_MASK_REG);
}

/*
 * Enable timer
 */
static inline void
timer_enable(void)
{
	unsigned int control_value;

	control_value = read_4(TIMER_TM_CR_REG);

	control_value |= TIMER1_OVERFLOW_ENABLE;
	control_value |= TIMER1_ENABLE;
	control_value |= TIMER2_OVERFLOW_ENABLE;
	control_value |= TIMER2_ENABLE;

	write_4(control_value, TIMER_TM_CR_REG);
}

static inline unsigned int
read_second_timer_counter(void)
{

	return read_4(TIMER_TM2_COUNTER_REG);
}

/*
 * Get timer interrupt status
 */
static inline unsigned int
read_timer_interrupt_status(void)
{

	return read_4(TIMER_TM_INTR_STATUS_REG);
}

/*
 * Clear timer interrupt status
 */
static inline void
clear_timer_interrupt_status(unsigned int irq)
{
	unsigned int interrupt_status;

	interrupt_status =   read_4(TIMER_TM_INTR_STATUS_REG);
	if (irq == 0) {
		if (interrupt_status & (TIMER1_MATCH1_INTR))
			interrupt_status &= ~(TIMER1_MATCH1_INTR);
		if (interrupt_status & (TIMER1_MATCH2_INTR))
			interrupt_status &= ~(TIMER1_MATCH2_INTR);
		if (interrupt_status & (TIMER1_OVERFLOW_INTR))
			interrupt_status &= ~(TIMER1_OVERFLOW_INTR);
	}
	if (irq == 1) {
		if (interrupt_status & (TIMER2_MATCH1_INTR))
			interrupt_status &= ~(TIMER2_MATCH1_INTR);
		if (interrupt_status & (TIMER2_MATCH2_INTR))
			interrupt_status &= ~(TIMER2_MATCH2_INTR);
		if (interrupt_status & (TIMER2_OVERFLOW_INTR))
			interrupt_status &= ~(TIMER2_OVERFLOW_INTR);
	}

	write_4(interrupt_status, TIMER_TM_INTR_STATUS_REG);
}

static unsigned
ec_timer_get_timecount(struct timecounter *a)
{
	unsigned int ticks1;
	arm_mask_irq(1);
	ticks1 = read_second_timer_counter();
	arm_unmask_irq(1);
	return ticks1;
}

/*
 * Setup timer
 */
static inline void
do_setup_timer(void)
{

	timer_counter = APB_clock/HZ;
	/*
	 * setup timer-related values
	 */
	setup_timer(timer_counter);
}

void
cpu_initclocks(void)
{

	ec_timecounter.tc_frequency = APB_clock;
	tc_init(&ec_timecounter);
	timer_enable();
	timers_initialized = 1;
}

void
cpu_startprofclock(void)
{

}

void
cpu_stopprofclock(void)
{

}

static int
ec_timer_probe(device_t dev)
{

	device_set_desc(dev, "Econa CPU Timer");
	return (0);
}

static int
ec_reset(void *arg)
{

	arm_mask_irq(1);
	clear_timer_interrupt_status(1);
	arm_unmask_irq(1);
	return (FILTER_HANDLED);
}

static int
ec_hardclock(void *arg)
{
	struct	trapframe *frame;
	unsigned int val;
	/*clear timer interrupt status*/

	arm_mask_irq(0);

	val = read_4(TIMER_INTERRUPT_STATUS_REG);
	val &= ~(TIMER1_OVERFLOW_INTERRUPT);
	write_4(val, TIMER_INTERRUPT_STATUS_REG);

	frame = (struct trapframe *)arg;
	hardclock(TRAPF_USERMODE(frame), TRAPF_PC(frame));

	arm_unmask_irq(0);

	return (FILTER_HANDLED);
}

static int
ec_timer_attach(device_t dev)
{
	struct	ec_timer_softc *sc;
	int	error;
	void	*ihl;


	if (timer_softc != NULL)
		return (ENXIO);

	sc = (struct ec_timer_softc *)device_get_softc(dev);

	timer_softc = sc;

	error = bus_alloc_resources(dev, ec_timer_spec, sc->timer_res);
	if (error) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	sc->timer_bst = rman_get_bustag(sc->timer_res[0]);
	sc->timer_bsh = rman_get_bushandle(sc->timer_res[0]);

	do_setup_timer();

	if (bus_setup_intr(dev, sc->timer_res[1], INTR_TYPE_CLK,
	    ec_hardclock, NULL, NULL, &ihl) != 0) {
		bus_release_resources(dev, ec_timer_spec, sc->timer_res);
		device_printf(dev, "could not setup hardclock interrupt\n");
		return (ENXIO);
	}

	if (bus_setup_intr(dev, sc->timer_res[2], INTR_TYPE_CLK,
	    ec_reset, NULL, NULL, &ihl) != 0) {
		bus_release_resources(dev, ec_timer_spec, sc->timer_res);
		device_printf(dev, "could not setup timer interrupt\n");
		return (ENXIO);
	}

	return (0);
}

static device_method_t ec_timer_methods[] = {
	DEVMETHOD(device_probe, ec_timer_probe),
	DEVMETHOD(device_attach, ec_timer_attach),
	{ 0, 0 }
};

static driver_t ec_timer_driver = {
	"timer",
	ec_timer_methods,
	sizeof(struct ec_timer_softc),
};

static devclass_t ec_timer_devclass;

DRIVER_MODULE(timer, econaarm, ec_timer_driver, ec_timer_devclass, 0, 0);
