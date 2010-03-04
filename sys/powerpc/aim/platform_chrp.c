/*-
 * Copyright (c) 2008 Marcel Moolenaar
 * Copyright (c) 2009 Nathan Whitehorn
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/hid.h>
#include <machine/platformvar.h>
#include <machine/pmap.h>
#include <machine/smp.h>
#include <machine/spr.h>

#include <dev/ofw/openfirm.h>
#include <machine/ofw_machdep.h>

#include "platform_if.h"

#ifdef SMP
extern void *ap_pcpu;
#endif

static int chrp_probe(platform_t);
void chrp_mem_regions(platform_t, struct mem_region **phys, int *physsz,
    struct mem_region **avail, int *availsz);
static u_long chrp_timebase_freq(platform_t, struct cpuref *cpuref);
static int chrp_smp_first_cpu(platform_t, struct cpuref *cpuref);
static int chrp_smp_next_cpu(platform_t, struct cpuref *cpuref);
static int chrp_smp_get_bsp(platform_t, struct cpuref *cpuref);
static int chrp_smp_start_cpu(platform_t, struct pcpu *cpu);

static platform_method_t chrp_methods[] = {
	PLATFORMMETHOD(platform_probe, 		chrp_probe),
	PLATFORMMETHOD(platform_mem_regions,	chrp_mem_regions),
	PLATFORMMETHOD(platform_timebase_freq,	chrp_timebase_freq),
	
	PLATFORMMETHOD(platform_smp_first_cpu,	chrp_smp_first_cpu),
	PLATFORMMETHOD(platform_smp_next_cpu,	chrp_smp_next_cpu),
	PLATFORMMETHOD(platform_smp_get_bsp,	chrp_smp_get_bsp),
	PLATFORMMETHOD(platform_smp_start_cpu,	chrp_smp_start_cpu),

	{ 0, 0 }
};

static platform_def_t chrp_platform = {
	"chrp",
	chrp_methods,
	0
};

PLATFORM_DEF(chrp_platform);

static int
chrp_probe(platform_t plat)
{
	if (OF_finddevice("/memory") != -1)
		return (BUS_PROBE_GENERIC);

	return (ENXIO);
}

void
chrp_mem_regions(platform_t plat, struct mem_region **phys, int *physsz,
    struct mem_region **avail, int *availsz)
{
	ofw_mem_regions(phys,physsz,avail,availsz);
}

static u_long
chrp_timebase_freq(platform_t plat, struct cpuref *cpuref)
{
	phandle_t phandle;
	long ticks = -1;

	phandle = cpuref->cr_hwref;

	OF_getprop(phandle, "timebase-frequency", &ticks, sizeof(ticks));

	if (ticks <= 0)
		panic("Unable to determine timebase frequency!");

	return (ticks);
}


static int
chrp_smp_fill_cpuref(struct cpuref *cpuref, phandle_t cpu)
{
	int cpuid, res;

	cpuref->cr_hwref = cpu;
	res = OF_getprop(cpu, "reg", &cpuid, sizeof(cpuid));

	/*
	 * psim doesn't have a reg property, so assume 0 as for the
	 * uniprocessor case in the CHRP spec. 
	 */
	if (res < 0) {
		cpuid = 0;
	}

	cpuref->cr_cpuid = cpuid & 0xff;
	return (0);
}

static int
chrp_smp_first_cpu(platform_t plat, struct cpuref *cpuref)
{
	char buf[8];
	phandle_t cpu, dev, root;
	int res;

	root = OF_peer(0);

	dev = OF_child(root);
	while (dev != 0) {
		res = OF_getprop(dev, "name", buf, sizeof(buf));
		if (res > 0 && strcmp(buf, "cpus") == 0)
			break;
		dev = OF_peer(dev);
	}
	if (dev == 0) {
		/*
		 * psim doesn't have a name property on the /cpus node,
		 * but it can be found directly
		 */
		dev = OF_finddevice("/cpus");
		if (dev == 0)
			return (ENOENT);
	}

	cpu = OF_child(dev);

	while (cpu != 0) {
		res = OF_getprop(cpu, "device_type", buf, sizeof(buf));
		if (res > 0 && strcmp(buf, "cpu") == 0)
			break;
		cpu = OF_peer(cpu);
	}
	if (cpu == 0)
		return (ENOENT);

	return (chrp_smp_fill_cpuref(cpuref, cpu));
}

static int
chrp_smp_next_cpu(platform_t plat, struct cpuref *cpuref)
{
	char buf[8];
	phandle_t cpu;
	int res;

	cpu = OF_peer(cpuref->cr_hwref);
	while (cpu != 0) {
		res = OF_getprop(cpu, "device_type", buf, sizeof(buf));
		if (res > 0 && strcmp(buf, "cpu") == 0)
			break;
		cpu = OF_peer(cpu);
	}
	if (cpu == 0)
		return (ENOENT);

	return (chrp_smp_fill_cpuref(cpuref, cpu));
}

static int
chrp_smp_get_bsp(platform_t plat, struct cpuref *cpuref)
{
	ihandle_t inst;
	phandle_t bsp, chosen;
	int res;

	chosen = OF_finddevice("/chosen");
	if (chosen == 0)
		return (ENXIO);

	res = OF_getprop(chosen, "cpu", &inst, sizeof(inst));
	if (res < 0)
		return (ENXIO);

	bsp = OF_instance_to_package(inst);
	return (chrp_smp_fill_cpuref(cpuref, bsp));
}

static int
chrp_smp_start_cpu(platform_t plat, struct pcpu *pc)
{
#ifdef SMP
	phandle_t cpu;
	volatile uint8_t *rstvec;
	static volatile uint8_t *rstvec_virtbase = NULL;
	int res, reset, timeout;

	cpu = pc->pc_hwref;
	res = OF_getprop(cpu, "soft-reset", &reset, sizeof(reset));
	if (res < 0)
		return (ENXIO);

	ap_pcpu = pc;

	if (rstvec_virtbase == NULL)
		rstvec_virtbase = pmap_mapdev(0x80000000, PAGE_SIZE);

	rstvec = rstvec_virtbase + reset;

	*rstvec = 4;
	(void)(*rstvec);
	powerpc_sync();
	DELAY(1);
	*rstvec = 0;
	(void)(*rstvec);
	powerpc_sync();

	timeout = 10000;
	while (!pc->pc_awake && timeout--)
		DELAY(100);

	return ((pc->pc_awake) ? 0 : EBUSY);
#else
	/* No SMP support */
	return (ENXIO);
#endif
}

