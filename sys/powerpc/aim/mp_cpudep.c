/*-
 * Copyright (c) 2008 Marcel Moolenaar
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

#include <machine/bat.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/hid.h>
#include <machine/intr_machdep.h>
#include <machine/pcb.h>
#include <machine/psl.h>
#include <machine/smp.h>
#include <machine/spr.h>
#include <machine/trap_aim.h>

#include <dev/ofw/openfirm.h>
#include <machine/ofw_machdep.h>

extern void *rstcode;

void *ap_pcpu;

static int
powerpc_smp_fill_cpuref(struct cpuref *cpuref, phandle_t cpu)
{
	int cpuid, res;

	cpuref->cr_hwref = cpu;
	res = OF_getprop(cpu, "reg", &cpuid, sizeof(cpuid));
	if (res < 0)
		return (ENOENT);

	cpuref->cr_cpuid = cpuid & 0xff;
	return (0);
}

int
powerpc_smp_first_cpu(struct cpuref *cpuref)
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
	if (dev == 0)
		return (ENOENT);

	cpu = OF_child(dev);
	while (cpu != 0) {
		res = OF_getprop(cpu, "device_type", buf, sizeof(buf));
		if (res > 0 && strcmp(buf, "cpu") == 0)
			break;
		cpu = OF_peer(cpu);
	}
	if (cpu == 0)
		return (ENOENT);

	return (powerpc_smp_fill_cpuref(cpuref, cpu));
}

int
powerpc_smp_next_cpu(struct cpuref *cpuref)
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

	return (powerpc_smp_fill_cpuref(cpuref, cpu));
}

int
powerpc_smp_get_bsp(struct cpuref *cpuref)
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
	return (powerpc_smp_fill_cpuref(cpuref, bsp));
}

uint32_t
cpudep_ap_bootstrap(volatile uint32_t *trcp)
{
	uint32_t hid, msr, sp;

	trcp[0] = 0x2000;
	trcp[1] = (uint32_t)&cpudep_ap_bootstrap;

	__asm __volatile("mtsprg 0, %0" :: "r"(ap_pcpu));
	__asm __volatile("sync");

	trcp[0] = 0x2001;
	trcp[1] = (uint32_t)pcpup;

	hid = mfspr(SPR_HID0);
	hid &= ~(HID0_ICE | HID0_DCE);
	hid &= ~(HID0_DOZE | HID0_NAP | HID0_SLEEP);
	mtspr(SPR_HID0, hid);
	isync();

	trcp[0] = 0x2002;
	trcp[1] = hid;

	hid |= HID0_ICFI | HID0_DCFI;
	hid |= HID0_ICE | HID0_DCE;
	mtspr(SPR_HID0, hid);
	isync();

	trcp[0] = 0x2003;
	trcp[1] = hid;

	msr = PSL_IR | PSL_DR | PSL_ME | PSL_RI;
	mtmsr(msr);
	isync();

	trcp[0] = 0x2004;
	trcp[1] = msr;

	hid |= HID0_NAP | HID0_DPM;
	mtspr(SPR_HID0, hid);
	isync();

	trcp[0] = 0x2005;
	trcp[1] = hid;

	pcpup->pc_curthread = pcpup->pc_idlethread;
	pcpup->pc_curpcb = pcpup->pc_curthread->td_pcb;
	sp = pcpup->pc_curpcb->pcb_sp;

	trcp[0] = 0x2006;
	trcp[1] = sp;

	return (sp);
}

int
powerpc_smp_start_cpu(struct pcpu *pc)
{
	phandle_t cpu;
	volatile uint32_t *trcp;
	volatile uint8_t *rstvec;
	uint32_t trace;
	int res, reset, timeout;

	cpu = pc->pc_hwref;
	res = OF_getprop(cpu, "soft-reset", &reset, sizeof(reset));
	if (res < 0)
		return (ENXIO);

	trcp = (uint32_t *)(EXC_RST + 4);
	trace = *trcp;

	ap_pcpu = pc;

	rstvec = (uint8_t *)(0x80000000 + reset);

	*rstvec = 4;
	__asm __volatile("sync");
	DELAY(1);
	*rstvec = 0;
	__asm __volatile("sync");

	timeout = 1000;
	while (!pc->pc_awake && timeout--)
		DELAY(100);

	if (!pc->pc_awake)
		printf("XXX: timeout (trace=%x; data=%x)\n", trcp[0], trcp[1]);

	return (0);
}
