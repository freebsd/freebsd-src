/*-
 * Copyright (c) 2005 Nate Lawson (SDG)
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
 *
 * $FreeBSD$
 */

#ifndef _SYS_CPU_H_
#define _SYS_CPU_H_

/*
 * CPU device support.
 */

#define CPU_IVAR_PCPU		1

static __inline struct pcpu *cpu_get_pcpu(device_t dev)
{
	uintptr_t v = 0;
	BUS_READ_IVAR(device_get_parent(dev), dev, CPU_IVAR_PCPU, &v);
	return ((struct pcpu *)v);
}

/*
 * CPU frequency control interface.
 */

/* Each driver's CPU frequency setting is exported in this format. */
struct cf_setting {
	int	freq;	/* Processor clock in Mhz or percent (in 100ths.) */
	int	volts;	/* Voltage in mV. */
	int	power;	/* Power consumed in mW. */
	int	lat;	/* Transition latency in us. */
	device_t dev;	/* Driver providing this setting. */
};

/* Maximum number of settings a given driver can have. */
#define MAX_SETTINGS		24

/* A combination of settings is a level. */
struct cf_level {
	struct cf_setting	total_set;
	struct cf_setting	abs_set;
	struct cf_setting	rel_set[MAX_SETTINGS];
	int			rel_count;
	TAILQ_ENTRY(cf_level)	link;
};

TAILQ_HEAD(cf_level_lst, cf_level);

/* Drivers should set all unknown values to this. */
#define CPUFREQ_VAL_UNKNOWN	(-1)

/*
 * Every driver offers a type of CPU control.  Absolute levels are mutually
 * exclusive while relative levels modify the current absolute level.  There
 * may be multiple absolute and relative drivers available on a given
 * system.
 *
 * For example, consider a system with two absolute drivers that provide
 * frequency settings of 100, 200 and 300, 400 and a relative driver that
 * provides settings of 50%, 100%.  The cpufreq core would export frequency
 * levels of 50, 100, 150, 200, 300, 400.
 */
#define CPUFREQ_TYPE_RELATIVE	(1<<0)
#define CPUFREQ_TYPE_ABSOLUTE	(1<<1)

/*
 * When setting a level, the caller indicates the priority of this request.
 * Priorities determine, among other things, whether a level can be
 * overridden by other callers.  For example, if the user sets a level but
 * the system thermal driver needs to override it for emergency cooling,
 * the driver would use a higher priority.  Once the event has passed, the
 * driver would call cpufreq to resume any previous level.
 */
#define CPUFREQ_PRIO_HIGHEST	1000000
#define CPUFREQ_PRIO_KERN	1000
#define CPUFREQ_PRIO_USER	100
#define CPUFREQ_PRIO_LOWEST	0

/*
 * Register and unregister a driver with the cpufreq core.  Once a driver
 * is registered, it must support calls to its CPUFREQ_GET, CPUFREQ_GET_LEVEL,
 * and CPUFREQ_SET methods.  It must also unregister before returning from
 * its DEVICE_DETACH method.
 */
int	cpufreq_register(device_t dev);
int	cpufreq_unregister(device_t dev);

/* Allow values to be +/- a bit since sometimes we have to estimate. */
#define CPUFREQ_CMP(x, y)	(abs((x) - (y)) < 25)

/*
 * Machine-dependent functions.
 */

/* Estimate the current clock rate for the given CPU id. */
int	cpu_est_clockrate(int cpu_id, uint64_t *rate);

#endif /* !_SYS_CPU_H_ */
