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
#include <machine/rtas.h>
#include <machine/smp.h>
#include <machine/spr.h>
#include <machine/trap.h>

#include <dev/ofw/openfirm.h>
#include <machine/ofw_machdep.h>

#include "platform_if.h"

#ifdef SMP
extern void *ap_pcpu;
#endif

#ifdef __powerpc64__
static uint8_t splpar_vpa[MAXCPU][640] __aligned(128); /* XXX: dpcpu */
#endif

static vm_offset_t realmaxaddr = VM_MAX_ADDRESS;

static int chrp_probe(platform_t);
static int chrp_attach(platform_t);
void chrp_mem_regions(platform_t, struct mem_region *phys, int *physsz,
    struct mem_region *avail, int *availsz);
static vm_offset_t chrp_real_maxaddr(platform_t);
static u_long chrp_timebase_freq(platform_t, struct cpuref *cpuref);
static int chrp_smp_first_cpu(platform_t, struct cpuref *cpuref);
static int chrp_smp_next_cpu(platform_t, struct cpuref *cpuref);
static int chrp_smp_get_bsp(platform_t, struct cpuref *cpuref);
static void chrp_smp_ap_init(platform_t);
#ifdef SMP
static int chrp_smp_start_cpu(platform_t, struct pcpu *cpu);
static struct cpu_group *chrp_smp_topo(platform_t plat);
#endif
static void chrp_reset(platform_t);
#ifdef __powerpc64__
#include "phyp-hvcall.h"
static void phyp_cpu_idle(sbintime_t sbt);
#endif

static platform_method_t chrp_methods[] = {
	PLATFORMMETHOD(platform_probe, 		chrp_probe),
	PLATFORMMETHOD(platform_attach,		chrp_attach),
	PLATFORMMETHOD(platform_mem_regions,	chrp_mem_regions),
	PLATFORMMETHOD(platform_real_maxaddr,	chrp_real_maxaddr),
	PLATFORMMETHOD(platform_timebase_freq,	chrp_timebase_freq),
	
	PLATFORMMETHOD(platform_smp_ap_init,	chrp_smp_ap_init),
	PLATFORMMETHOD(platform_smp_first_cpu,	chrp_smp_first_cpu),
	PLATFORMMETHOD(platform_smp_next_cpu,	chrp_smp_next_cpu),
	PLATFORMMETHOD(platform_smp_get_bsp,	chrp_smp_get_bsp),
#ifdef SMP
	PLATFORMMETHOD(platform_smp_start_cpu,	chrp_smp_start_cpu),
	PLATFORMMETHOD(platform_smp_topo,	chrp_smp_topo),
#endif

	PLATFORMMETHOD(platform_reset,		chrp_reset),

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
	if (OF_finddevice("/memory") != -1 || OF_finddevice("/memory@0") != -1)
		return (BUS_PROBE_GENERIC);

	return (ENXIO);
}

static int
chrp_attach(platform_t plat)
{
#ifdef __powerpc64__
	int i;

	/* XXX: check for /rtas/ibm,hypertas-functions? */
	if (!(mfmsr() & PSL_HV)) {
		struct mem_region *phys, *avail;
		int nphys, navail;
		mem_regions(&phys, &nphys, &avail, &navail);
		realmaxaddr = phys[0].mr_size;

		pmap_mmu_install("mmu_phyp", BUS_PROBE_SPECIFIC);
		cpu_idle_hook = phyp_cpu_idle;

		/* Set up important VPA fields */
		for (i = 0; i < MAXCPU; i++) {
			bzero(splpar_vpa[i], sizeof(splpar_vpa));
			/* First two: VPA size */
			splpar_vpa[i][4] =
			    (uint8_t)((sizeof(splpar_vpa[i]) >> 8) & 0xff);
			splpar_vpa[i][5] =
			    (uint8_t)(sizeof(splpar_vpa[i]) & 0xff);
			splpar_vpa[i][0xba] = 1;	/* Maintain FPRs */
			splpar_vpa[i][0xbb] = 1;	/* Maintain PMCs */
			splpar_vpa[i][0xfc] = 0xff;	/* Maintain full SLB */
			splpar_vpa[i][0xfd] = 0xff;
			splpar_vpa[i][0xff] = 1;	/* Maintain Altivec */
		}
		mb();

		/* Set up hypervisor CPU stuff */
		chrp_smp_ap_init(plat);
	}
#endif

	/* Some systems (e.g. QEMU) need Open Firmware to stand down */
	ofw_quiesce();

	return (0);
}

static int
parse_drconf_memory(struct mem_region *ofmem, int *msz,
		    struct mem_region *ofavail, int *asz)
{
	phandle_t phandle;
	vm_offset_t base;
	int i, idx, len, lasz, lmsz, res;
	uint32_t flags, lmb_size[2];
	uint32_t *dmem;

	lmsz = *msz;
	lasz = *asz;

	phandle = OF_finddevice("/ibm,dynamic-reconfiguration-memory");
	if (phandle == -1)
		/* No drconf node, return. */
		return (0);

	res = OF_getencprop(phandle, "ibm,lmb-size", lmb_size,
	    sizeof(lmb_size));
	if (res == -1)
		return (0);
	printf("Logical Memory Block size: %d MB\n", lmb_size[1] >> 20);

	/* Parse the /ibm,dynamic-memory.
	   The first position gives the # of entries. The next two words
 	   reflect the address of the memory block. The next four words are
	   the DRC index, reserved, list index and flags.
	   (see PAPR C.6.6.2 ibm,dynamic-reconfiguration-memory)
	   
	    #el  Addr   DRC-idx  res   list-idx  flags
	   -------------------------------------------------
	   | 4 |   8   |   4   |   4   |   4   |   4   |....
	   -------------------------------------------------
	*/

	len = OF_getproplen(phandle, "ibm,dynamic-memory");
	if (len > 0) {

		/* We have to use a variable length array on the stack
		   since we have very limited stack space.
		*/
		cell_t arr[len/sizeof(cell_t)];

		res = OF_getencprop(phandle, "ibm,dynamic-memory", arr,
		    sizeof(arr));
		if (res == -1)
			return (0);

		/* Number of elements */
		idx = arr[0];

		/* First address, in arr[1], arr[2]*/
		dmem = &arr[1];
	
		for (i = 0; i < idx; i++) {
			base = ((uint64_t)dmem[0] << 32) + dmem[1];
			dmem += 4;
			flags = dmem[1];
			/* Use region only if available and not reserved. */
			if ((flags & 0x8) && !(flags & 0x80)) {
				ofmem[lmsz].mr_start = base;
				ofmem[lmsz].mr_size = (vm_size_t)lmb_size[1];
				ofavail[lasz].mr_start = base;
				ofavail[lasz].mr_size = (vm_size_t)lmb_size[1];
				lmsz++;
				lasz++;
			}
			dmem += 2;
		}
	}

	*msz = lmsz;
	*asz = lasz;

	return (1);
}

void
chrp_mem_regions(platform_t plat, struct mem_region *phys, int *physsz,
    struct mem_region *avail, int *availsz)
{
	vm_offset_t maxphysaddr;
	int i;

	ofw_mem_regions(phys, physsz, avail, availsz);
	parse_drconf_memory(phys, physsz, avail, availsz);

	/*
	 * On some firmwares (SLOF), some memory may be marked available that
	 * doesn't actually exist. This manifests as an extension of the last
	 * available segment past the end of physical memory, so truncate that
	 * one.
	 */
	maxphysaddr = 0;
	for (i = 0; i < *physsz; i++)
		if (phys[i].mr_start + phys[i].mr_size > maxphysaddr)
			maxphysaddr = phys[i].mr_start + phys[i].mr_size;

	for (i = 0; i < *availsz; i++)
		if (avail[i].mr_start + avail[i].mr_size > maxphysaddr)
			avail[i].mr_size = maxphysaddr - avail[i].mr_start;
}

static vm_offset_t
chrp_real_maxaddr(platform_t plat)
{
	return (realmaxaddr);
}

static u_long
chrp_timebase_freq(platform_t plat, struct cpuref *cpuref)
{
	phandle_t phandle;
	int32_t ticks = -1;

	phandle = cpuref->cr_hwref;

	OF_getencprop(phandle, "timebase-frequency", &ticks, sizeof(ticks));

	if (ticks <= 0)
		panic("Unable to determine timebase frequency!");

	return (ticks);
}

static int
chrp_smp_first_cpu(platform_t plat, struct cpuref *cpuref)
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
	res = OF_getencprop(cpu, "ibm,ppc-interrupt-server#s", &cpuid,
	    sizeof(cpuid));
	if (res <= 0)
		res = OF_getencprop(cpu, "reg", &cpuid, sizeof(cpuid));
	if (res <= 0)
		cpuid = 0;
	cpuref->cr_cpuid = cpuid;

	return (0);
}

static int
chrp_smp_next_cpu(platform_t plat, struct cpuref *cpuref)
{
	char buf[8];
	phandle_t cpu;
	int i, res, cpuid;

	/* Check for whether it should be the next thread */
	res = OF_getproplen(cpuref->cr_hwref, "ibm,ppc-interrupt-server#s");
	if (res > 0) {
		cell_t interrupt_servers[res/sizeof(cell_t)];
		OF_getencprop(cpuref->cr_hwref, "ibm,ppc-interrupt-server#s",
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
	res = OF_getencprop(cpu, "ibm,ppc-interrupt-server#s", &cpuid,
	    sizeof(cpuid));
	if (res <= 0)
		res = OF_getencprop(cpu, "reg", &cpuid, sizeof(cpuid));
	if (res <= 0)
		cpuid = 0;
	cpuref->cr_cpuid = cpuid;

	return (0);
}

static int
chrp_smp_get_bsp(platform_t plat, struct cpuref *cpuref)
{
	ihandle_t inst;
	phandle_t bsp, chosen;
	int res, cpuid;

	chosen = OF_finddevice("/chosen");
	if (chosen == 0)
		return (ENXIO);

	res = OF_getencprop(chosen, "cpu", &inst, sizeof(inst));
	if (res < 0)
		return (ENXIO);

	bsp = OF_instance_to_package(inst);

	/* Pick the primary thread. Can it be any other? */
	cpuref->cr_hwref = bsp;
	res = OF_getencprop(bsp, "ibm,ppc-interrupt-server#s", &cpuid,
	    sizeof(cpuid));
	if (res <= 0)
		res = OF_getencprop(bsp, "reg", &cpuid, sizeof(cpuid));
	if (res <= 0)
		cpuid = 0;
	cpuref->cr_cpuid = cpuid;

	return (0);
}

#ifdef SMP
static int
chrp_smp_start_cpu(platform_t plat, struct pcpu *pc)
{
	cell_t start_cpu;
	int result, err, timeout;

	if (!rtas_exists()) {
		printf("RTAS uninitialized: unable to start AP %d\n",
		    pc->pc_cpuid);
		return (ENXIO);
	}

	start_cpu = rtas_token_lookup("start-cpu");
	if (start_cpu == -1) {
		printf("RTAS unknown method: unable to start AP %d\n",
		    pc->pc_cpuid);
		return (ENXIO);
	}

	ap_pcpu = pc;
	powerpc_sync();

	result = rtas_call_method(start_cpu, 3, 1, pc->pc_cpuid, EXC_RST, pc,
	    &err);
	if (result < 0 || err != 0) {
		printf("RTAS error (%d/%d): unable to start AP %d\n",
		    result, err, pc->pc_cpuid);
		return (ENXIO);
	}

	timeout = 10000;
	while (!pc->pc_awake && timeout--)
		DELAY(100);

	return ((pc->pc_awake) ? 0 : EBUSY);
}

static struct cpu_group *
chrp_smp_topo(platform_t plat)
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
chrp_reset(platform_t platform)
{
	OF_reboot();
}

#ifdef __powerpc64__
static void
phyp_cpu_idle(sbintime_t sbt)
{
	phyp_hcall(H_CEDE);
}

static void
chrp_smp_ap_init(platform_t platform)
{
	if (!(mfmsr() & PSL_HV)) {
		/* Register VPA */
		phyp_hcall(H_REGISTER_VPA, 1UL, PCPU_GET(cpuid),
		    splpar_vpa[PCPU_GET(cpuid)]);

		/* Set interrupt priority */
		phyp_hcall(H_CPPR, 0xff);
	}
}
#else
static void
chrp_smp_ap_init(platform_t platform)
{
}
#endif

