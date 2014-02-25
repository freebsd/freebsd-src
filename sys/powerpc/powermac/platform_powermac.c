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

static int powermac_probe(platform_t);
static int powermac_attach(platform_t);
void powermac_mem_regions(platform_t, struct mem_region *phys, int *physsz,
    struct mem_region *avail, int *availsz);
static u_long powermac_timebase_freq(platform_t, struct cpuref *cpuref);
static int powermac_smp_first_cpu(platform_t, struct cpuref *cpuref);
static int powermac_smp_next_cpu(platform_t, struct cpuref *cpuref);
static int powermac_smp_get_bsp(platform_t, struct cpuref *cpuref);
static int powermac_smp_start_cpu(platform_t, struct pcpu *cpu);
static void powermac_reset(platform_t);

static platform_method_t powermac_methods[] = {
	PLATFORMMETHOD(platform_probe, 		powermac_probe),
	PLATFORMMETHOD(platform_attach,		powermac_attach),
	PLATFORMMETHOD(platform_mem_regions,	powermac_mem_regions),
	PLATFORMMETHOD(platform_timebase_freq,	powermac_timebase_freq),
	
	PLATFORMMETHOD(platform_smp_first_cpu,	powermac_smp_first_cpu),
	PLATFORMMETHOD(platform_smp_next_cpu,	powermac_smp_next_cpu),
	PLATFORMMETHOD(platform_smp_get_bsp,	powermac_smp_get_bsp),
	PLATFORMMETHOD(platform_smp_start_cpu,	powermac_smp_start_cpu),

	PLATFORMMETHOD(platform_reset,		powermac_reset),

	PLATFORMMETHOD_END
};

static platform_def_t powermac_platform = {
	"powermac",
	powermac_methods,
	0
};

PLATFORM_DEF(powermac_platform);

static int
powermac_probe(platform_t plat)
{
	char compat[255];
	ssize_t compatlen;
	char *curstr;
	phandle_t root;

	root = OF_peer(0);
	if (root == 0)
		return (ENXIO);

	compatlen = OF_getprop(root, "compatible", compat, sizeof(compat));
	
	for (curstr = compat; curstr < compat + compatlen;
	    curstr += strlen(curstr) + 1) {
		if (strncmp(curstr, "MacRISC", 7) == 0)
			return (BUS_PROBE_SPECIFIC);
	}

	return (ENXIO);
}

void
powermac_mem_regions(platform_t plat, struct mem_region *phys, int *physsz,
    struct mem_region *avail, int *availsz)
{
	phandle_t memory;
	cell_t memoryprop[PHYS_AVAIL_SZ * 2];
	ssize_t propsize, i, j;
	int physacells = 1;

	memory = OF_finddevice("/memory");

	/* "reg" has variable #address-cells, but #size-cells is always 1 */
	OF_getprop(OF_parent(memory), "#address-cells", &physacells,
	    sizeof(physacells));

	propsize = OF_getprop(memory, "reg", memoryprop, sizeof(memoryprop));
	propsize /= sizeof(cell_t);
	for (i = 0, j = 0; i < propsize; i += physacells+1, j++) {
		phys[j].mr_start = memoryprop[i];
		if (physacells == 2) {
#ifndef __powerpc64__
			/* On 32-bit PPC, ignore regions starting above 4 GB */
			if (memoryprop[i] != 0) {
				j--;
				continue;
			}
#else
			phys[j].mr_start <<= 32;
#endif
			phys[j].mr_start |= memoryprop[i+1];
		}
		phys[j].mr_size = memoryprop[i + physacells];
	}
	*physsz = j;

	/* "available" always has #address-cells = 1 */
	propsize = OF_getprop(memory, "available", memoryprop,
	    sizeof(memoryprop));
	propsize /= sizeof(cell_t);
	for (i = 0, j = 0; i < propsize; i += 2, j++) {
		avail[j].mr_start = memoryprop[i];
		avail[j].mr_size = memoryprop[i + 1];
	}

#ifdef __powerpc64__
	/* Add in regions above 4 GB to the available list */
	for (i = 0; i < *physsz; i++) {
		if (phys[i].mr_start > BUS_SPACE_MAXADDR_32BIT) {
			avail[j].mr_start = phys[i].mr_start;
			avail[j].mr_size = phys[i].mr_size;
			j++;
		}
	}
#endif
	*availsz = j;
}

static int
powermac_attach(platform_t plat)
{
	phandle_t rootnode;
	char model[32];


	/*
	 * Quiesce Open Firmware on PowerMac11,2 and 12,1. It is
	 * necessary there to shut down a background thread doing fan
	 * management, and is harmful on other machines (it will make OF
	 * shut off power to various system components it had turned on).
	 *
	 * Note: we don't need to worry about which OF module we are
	 * using since this is called only from very early boot, within
	 * OF's boot context.
	 */

	rootnode = OF_finddevice("/");
	if (OF_getprop(rootnode, "model", model, sizeof(model)) > 0) {
		if (strcmp(model, "PowerMac11,2") == 0 ||
		    strcmp(model, "PowerMac12,1") == 0) {
			ofw_quiesce();
		}
	}

	return (0);
}

static u_long
powermac_timebase_freq(platform_t plat, struct cpuref *cpuref)
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
powermac_smp_fill_cpuref(struct cpuref *cpuref, phandle_t cpu)
{
	cell_t cpuid;
	int res;

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
powermac_smp_first_cpu(platform_t plat, struct cpuref *cpuref)
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
		if (dev == -1)
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

	return (powermac_smp_fill_cpuref(cpuref, cpu));
}

static int
powermac_smp_next_cpu(platform_t plat, struct cpuref *cpuref)
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

	return (powermac_smp_fill_cpuref(cpuref, cpu));
}

static int
powermac_smp_get_bsp(platform_t plat, struct cpuref *cpuref)
{
	ihandle_t inst;
	phandle_t bsp, chosen;
	int res;

	chosen = OF_finddevice("/chosen");
	if (chosen == -1)
		return (ENXIO);

	res = OF_getprop(chosen, "cpu", &inst, sizeof(inst));
	if (res < 0)
		return (ENXIO);

	bsp = OF_instance_to_package(inst);
	return (powermac_smp_fill_cpuref(cpuref, bsp));
}

static int
powermac_smp_start_cpu(platform_t plat, struct pcpu *pc)
{
#ifdef SMP
	phandle_t cpu;
	volatile uint8_t *rstvec;
	static volatile uint8_t *rstvec_virtbase = NULL;
	int res, reset, timeout;

	cpu = pc->pc_hwref;
	res = OF_getprop(cpu, "soft-reset", &reset, sizeof(reset));
	if (res < 0) {
		reset = 0x58;

		switch (pc->pc_cpuid) {
		case 0:
			reset += 0x03;
			break;
		case 1:
			reset += 0x04;
			break;
		case 2:
			reset += 0x0f;
			break;
		case 3:
			reset += 0x10;
			break;
		default:
			return (ENXIO);
		}
	}

	ap_pcpu = pc;

	if (rstvec_virtbase == NULL)
		rstvec_virtbase = pmap_mapdev(0x80000000, PAGE_SIZE);

	rstvec = rstvec_virtbase + reset;

	*rstvec = 4;
	powerpc_sync();
	(void)(*rstvec);
	powerpc_sync();
	DELAY(1);
	*rstvec = 0;
	powerpc_sync();
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

/* From p3-53 of the MPC7450 RISC Microprocessor Family Reference Manual */
void
flush_disable_caches(void)
{
	register_t msr;
	register_t msscr0;
	register_t cache_reg;
	volatile uint32_t *memp;
	uint32_t temp;
	int i;
	int x;

	msr = mfmsr();
	powerpc_sync();
	mtmsr(msr & ~(PSL_EE | PSL_DR));
	msscr0 = mfspr(SPR_MSSCR0);
	msscr0 &= ~MSSCR0_L2PFE;
	mtspr(SPR_MSSCR0, msscr0);
	powerpc_sync();
	isync();
	__asm__ __volatile__("dssall; sync");
	powerpc_sync();
	isync();
	__asm__ __volatile__("dcbf 0,%0" :: "r"(0));
	__asm__ __volatile__("dcbf 0,%0" :: "r"(0));
	__asm__ __volatile__("dcbf 0,%0" :: "r"(0));

	/* Lock the L1 Data cache. */
	mtspr(SPR_LDSTCR, mfspr(SPR_LDSTCR) | 0xFF);
	powerpc_sync();
	isync();

	mtspr(SPR_LDSTCR, 0);

	/*
	 * Perform this in two stages: Flush the cache starting in RAM, then do it
	 * from ROM.
	 */
	memp = (volatile uint32_t *)0x00000000;
	for (i = 0; i < 128 * 1024; i++) {
		temp = *memp;
		__asm__ __volatile__("dcbf 0,%0" :: "r"(memp));
		memp += 32/sizeof(*memp);
	}

	memp = (volatile uint32_t *)0xfff00000;
	x = 0xfe;

	for (; x != 0xff;) {
		mtspr(SPR_LDSTCR, x);
		for (i = 0; i < 128; i++) {
			temp = *memp;
			__asm__ __volatile__("dcbf 0,%0" :: "r"(memp));
			memp += 32/sizeof(*memp);
		}
		x = ((x << 1) | 1) & 0xff;
	}
	mtspr(SPR_LDSTCR, 0);

	cache_reg = mfspr(SPR_L2CR);
	if (cache_reg & L2CR_L2E) {
		cache_reg &= ~(L2CR_L2IO_7450 | L2CR_L2DO_7450);
		mtspr(SPR_L2CR, cache_reg);
		powerpc_sync();
		mtspr(SPR_L2CR, cache_reg | L2CR_L2HWF);
		while (mfspr(SPR_L2CR) & L2CR_L2HWF)
			; /* Busy wait for cache to flush */
		powerpc_sync();
		cache_reg &= ~L2CR_L2E;
		mtspr(SPR_L2CR, cache_reg);
		powerpc_sync();
		mtspr(SPR_L2CR, cache_reg | L2CR_L2I);
		powerpc_sync();
		while (mfspr(SPR_L2CR) & L2CR_L2I)
			; /* Busy wait for L2 cache invalidate */
		powerpc_sync();
	}

	cache_reg = mfspr(SPR_L3CR);
	if (cache_reg & L3CR_L3E) {
		cache_reg &= ~(L3CR_L3IO | L3CR_L3DO);
		mtspr(SPR_L3CR, cache_reg);
		powerpc_sync();
		mtspr(SPR_L3CR, cache_reg | L3CR_L3HWF);
		while (mfspr(SPR_L3CR) & L3CR_L3HWF)
			; /* Busy wait for cache to flush */
		powerpc_sync();
		cache_reg &= ~L3CR_L3E;
		mtspr(SPR_L3CR, cache_reg);
		powerpc_sync();
		mtspr(SPR_L3CR, cache_reg | L3CR_L3I);
		powerpc_sync();
		while (mfspr(SPR_L3CR) & L3CR_L3I)
			; /* Busy wait for L3 cache invalidate */
		powerpc_sync();
	}

	mtspr(SPR_HID0, mfspr(SPR_HID0) & ~HID0_DCE);
	powerpc_sync();
	isync();

	mtmsr(msr);
}

static void
powermac_reset(platform_t platform)
{
	OF_reboot();
}

