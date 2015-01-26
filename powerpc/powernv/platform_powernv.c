/*-
 * Copyright (c) 2015 Nathan Whitehorn
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
#include <machine/rtas.h>
#include <machine/smp.h>
#include <machine/spr.h>
#include <machine/trap.h>

#include <dev/ofw/openfirm.h>
#include <machine/ofw_machdep.h>

#include "platform_if.h"
#include "opal.h"

#ifdef SMP
extern void *ap_pcpu;
#endif

static int powernv_probe(platform_t);
static int powernv_attach(platform_t);
void powernv_mem_regions(platform_t, struct mem_region *phys, int *physsz,
    struct mem_region *avail, int *availsz);
static u_long powernv_timebase_freq(platform_t, struct cpuref *cpuref);
static int powernv_smp_first_cpu(platform_t, struct cpuref *cpuref);
static int powernv_smp_next_cpu(platform_t, struct cpuref *cpuref);
static int powernv_smp_get_bsp(platform_t, struct cpuref *cpuref);
static void powernv_smp_ap_init(platform_t);
#ifdef SMP
static int powernv_smp_start_cpu(platform_t, struct pcpu *cpu);
static struct cpu_group *powernv_smp_topo(platform_t plat);
#endif
static void powernv_reset(platform_t);
static void powernv_cpu_idle(sbintime_t sbt);

static platform_method_t powernv_methods[] = {
	PLATFORMMETHOD(platform_probe, 		powernv_probe),
	PLATFORMMETHOD(platform_attach,		powernv_attach),
	PLATFORMMETHOD(platform_mem_regions,	powernv_mem_regions),
	PLATFORMMETHOD(platform_timebase_freq,	powernv_timebase_freq),
	
	PLATFORMMETHOD(platform_smp_ap_init,	powernv_smp_ap_init),
	PLATFORMMETHOD(platform_smp_first_cpu,	powernv_smp_first_cpu),
	PLATFORMMETHOD(platform_smp_next_cpu,	powernv_smp_next_cpu),
	PLATFORMMETHOD(platform_smp_get_bsp,	powernv_smp_get_bsp),
#ifdef SMP
	PLATFORMMETHOD(platform_smp_start_cpu,	powernv_smp_start_cpu),
	PLATFORMMETHOD(platform_smp_topo,	powernv_smp_topo),
#endif

	PLATFORMMETHOD(platform_reset,		powernv_reset),

	{ 0, 0 }
};

static platform_def_t powernv_platform = {
	"powernv",
	powernv_methods,
	0
};

PLATFORM_DEF(powernv_platform);

static int
powernv_probe(platform_t plat)
{
	if (opal_check() == 0)
		return (BUS_PROBE_SPECIFIC);

	return (ENXIO);
}

static int
powernv_attach(platform_t plat)
{
	/* Ping OPAL again just to make sure */
	opal_check();

	cpu_idle_hook = powernv_cpu_idle;

	/* Direct interrupts to SRR instead of HSRR */
	mtspr(SPR_LPCR, mfspr(SPR_LPCR) | LPCR_LPES);

	return (0);
}

void
powernv_mem_regions(platform_t plat, struct mem_region *phys, int *physsz,
    struct mem_region *avail, int *availsz)
{

	ofw_mem_regions(phys, physsz, avail, availsz);
}

static u_long
powernv_timebase_freq(platform_t plat, struct cpuref *cpuref)
{
	phandle_t phandle;
	int32_t ticks = -1;

	phandle = cpuref->cr_hwref;

	OF_getprop(phandle, "timebase-frequency", &ticks, sizeof(ticks));

	if (ticks <= 0)
		panic("Unable to determine timebase frequency!");

	return (ticks);
}

static int
powernv_smp_first_cpu(platform_t plat, struct cpuref *cpuref)
{
	char buf[8];
	phandle_t cpu, dev, root;
	int res, cpuid;

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

	cpuref->cr_hwref = cpu;
	res = OF_getprop(cpu, "ibm,ppc-interrupt-server#s", &cpuid,
	    sizeof(cpuid));
	if (res <= 0)
		res = OF_getprop(cpu, "reg", &cpuid, sizeof(cpuid));
	if (res <= 0)
		cpuid = 0;
	cpuref->cr_cpuid = cpuid;

	return (0);
}

static int
powernv_smp_next_cpu(platform_t plat, struct cpuref *cpuref)
{
	char buf[8];
	phandle_t cpu;
	int i, res, cpuid;

	/* Check for whether it should be the next thread */
	res = OF_getproplen(cpuref->cr_hwref, "ibm,ppc-interrupt-server#s");
	if (res > 0) {
		cell_t interrupt_servers[res/sizeof(cell_t)];
		OF_getprop(cpuref->cr_hwref, "ibm,ppc-interrupt-server#s",
		    interrupt_servers, res);
		for (i = 0; i < res/sizeof(cell_t) - 1; i++) {
			if (interrupt_servers[i] == cpuref->cr_cpuid) {
				cpuref->cr_cpuid = interrupt_servers[i+1];
				return (0);
			}
		}
	}

	/* Next CPU core/package */
	cpu = OF_peer(cpuref->cr_hwref);
	while (cpu != 0) {
		res = OF_getprop(cpu, "device_type", buf, sizeof(buf));
		if (res > 0 && strcmp(buf, "cpu") == 0)
			break;
		cpu = OF_peer(cpu);
	}
	if (cpu == 0)
		return (ENOENT);

	cpuref->cr_hwref = cpu;
	res = OF_getprop(cpu, "ibm,ppc-interrupt-server#s", &cpuid,
	    sizeof(cpuid));
	if (res <= 0)
		res = OF_getprop(cpu, "reg", &cpuid, sizeof(cpuid));
	if (res <= 0)
		cpuid = 0;
	cpuref->cr_cpuid = cpuid;

	return (0);
}

static int
powernv_smp_get_bsp(platform_t plat, struct cpuref *cpuref)
{
	phandle_t chosen;
	int cpuid, res;
	struct cpuref i;

	chosen = OF_finddevice("/chosen");
	if (chosen == 0)
		return (ENOENT);

	res = OF_getencprop(chosen, "fdtbootcpu", &cpuid, sizeof(cpuid));
	if (res < 0)
		return (ENOENT);

	cpuref->cr_cpuid = cpuid;

	if (powernv_smp_first_cpu(plat, &i) != 0)
		return (ENOENT);
	cpuref->cr_hwref = i.cr_hwref;

	do {
		if (i.cr_cpuid == cpuid) {
			cpuref->cr_hwref = i.cr_hwref;
			break;
		}
	} while (powernv_smp_next_cpu(plat, &i) == 0);

	return (0);
}

#ifdef SMP
static int
powernv_smp_start_cpu(platform_t plat, struct pcpu *pc)
{
	int result;

	ap_pcpu = pc;
	powerpc_sync();

	result = opal_call(OPAL_START_CPU, pc->pc_cpuid, EXC_RST);
	if (result != OPAL_SUCCESS) {
		printf("OPAL error (%d): unable to start AP %d\n",
		    result, pc->pc_cpuid);
		return (ENXIO);
	}

	return (0);
}

static struct cpu_group *
powernv_smp_topo(platform_t plat)
{
	struct pcpu *pc, *last_pc;
	int i, ncores, ncpus;

	ncores = ncpus = 0;
	last_pc = NULL;
	for (i = 0; i <= mp_maxid; i++) {
		pc = pcpu_find(i);
		if (pc == NULL)
			continue;
		if (last_pc == NULL || pc->pc_hwref != last_pc->pc_hwref)
			ncores++;
		last_pc = pc;
		ncpus++;
	}

	if (ncpus % ncores != 0) {
		printf("WARNING: Irregular SMP topology. Performance may be "
		     "suboptimal (%d CPUS, %d cores)\n", ncpus, ncores);
		return (smp_topo_none());
	}

	/* Don't do anything fancier for non-threaded SMP */
	if (ncpus == ncores)
		return (smp_topo_none());

	return (smp_topo_1level(CG_SHARE_L1, ncpus / ncores, CG_FLAG_SMT));
}
#endif

static void
powernv_reset(platform_t platform)
{

	opal_call(OPAL_CEC_REBOOT);
}

static void
powernv_smp_ap_init(platform_t platform)
{
}

static void
powernv_cpu_idle(sbintime_t sbt)
{
}
