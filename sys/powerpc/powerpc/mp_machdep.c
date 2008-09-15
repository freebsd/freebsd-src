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
#include <sys/sched.h>
#include <sys/smp.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr_machdep.h>
#include <machine/smp.h>

#include "pic_if.h"

extern struct pcpu __pcpu[MAXCPU];

volatile static int ap_awake;
volatile static u_int ap_state;
volatile static uint32_t ap_decr;

int mp_ipi_test = 0;

void
machdep_ap_bootstrap(volatile uint32_t *trcp)
{

	trcp[0] = 0x3000;
	trcp[1] = (uint32_t)&machdep_ap_bootstrap;

	// __asm __volatile("mtspr 1023,%0" :: "r"(PCPU_GET(cpuid)));
	__asm __volatile("mfspr %0,1023" : "=r"(pcpup->pc_pir));
	pcpup->pc_awake = 1;

	while (ap_state == 0)
		;

	__asm __volatile("mtdec %0" :: "r"(ap_decr));

	ap_awake++;

	/* Initialize curthread. */
	PCPU_SET(curthread, PCPU_GET(idlethread));

	mtmsr(mfmsr() | PSL_EE);
	sched_throw(NULL);
}

struct cpu_group *
cpu_topo(void)
{

	return (smp_topo_none());
}

void
cpu_mp_setmaxid(void)
{
	struct cpuref cpuref;
	int error;

	mp_ncpus = 0;
	error = powerpc_smp_first_cpu(&cpuref);
	while (!error) {
		mp_ncpus++;
		error = powerpc_smp_next_cpu(&cpuref);
	}
	/* Sanity. */
	if (mp_ncpus == 0)
		mp_ncpus = 1;

	/*
	 * Set the largest cpuid we're going to use. This is necessary
	 * for VM initialization.
	 */
	mp_maxid = min(mp_ncpus, MAXCPU) - 1;
}

int
cpu_mp_probe(void)
{

	/*
	 * We're not going to enable SMP if there's only 1 processor.
	 */
	return (mp_ncpus > 1);
}

void
cpu_mp_start(void)
{
	struct cpuref bsp, cpu;
	struct pcpu *pc;
	int error;

	error = powerpc_smp_get_bsp(&bsp);
	KASSERT(error == 0, ("Don't know BSP"));
	KASSERT(bsp.cr_cpuid == 0, ("%s: cpuid != 0", __func__));

	error = powerpc_smp_first_cpu(&cpu);
	while (!error) {
		if (cpu.cr_cpuid >= MAXCPU) {
			printf("SMP: cpu%d: skipped -- ID out of range\n",
			    cpu.cr_cpuid);
			goto next;
		}
		if (all_cpus & (1 << cpu.cr_cpuid)) {
			printf("SMP: cpu%d: skipped - duplicate ID\n",
			    cpu.cr_cpuid);
			goto next;
		}
		if (cpu.cr_cpuid != bsp.cr_cpuid) {
			pc = &__pcpu[cpu.cr_cpuid];
			pcpu_init(pc, cpu.cr_cpuid, sizeof(*pc));
		} else {
			pc = pcpup;
			pc->pc_cpuid = bsp.cr_cpuid;
			pc->pc_bsp = 1;
		}
		pc->pc_cpumask = 1 << pc->pc_cpuid;
		pc->pc_hwref = cpu.cr_hwref;
		all_cpus |= pc->pc_cpumask;

 next:
		error = powerpc_smp_next_cpu(&cpu);
	}
}

void
cpu_mp_announce(void)
{
	struct pcpu *pc;
	int i;

	for (i = 0; i <= mp_maxid; i++) {
		pc = pcpu_find(i);
		if (pc == NULL)
			continue;
		printf("cpu%d: dev=%x", i, pc->pc_hwref);
		if (pc->pc_bsp)
			printf(" (BSP)");
		printf("\n");
	}
}

static void
cpu_mp_unleash(void *dummy)
{
	struct pcpu *pc;
	int cpus;

	if (mp_ncpus <= 1)
		return;

	if (mp_ipi_test != 1) {
		printf("SMP: ERROR: sending of a test IPI failed\n");
		return;
	}

	cpus = 0;
	smp_cpus = 0;
	SLIST_FOREACH(pc, &cpuhead, pc_allcpu) {
		cpus++;
		pc->pc_other_cpus = all_cpus & ~pc->pc_cpumask;
		if (!pc->pc_bsp) {
			printf("Waking up CPU %d (dev=%x)\n", pc->pc_cpuid,
			    pc->pc_hwref);
			powerpc_smp_start_cpu(pc);
		} else {
			__asm __volatile("mfspr %0,1023" : "=r"(pc->pc_pir));
			pc->pc_awake = 1;
		}
		if (pc->pc_awake)
			smp_cpus++;
	}

	ap_awake = 1;
	__asm __volatile("mfdec %0" : "=r"(ap_decr));
	ap_state++;

	while (ap_awake < smp_cpus)
		;

	if (smp_cpus != cpus || cpus != mp_ncpus) {
		printf("SMP: %d CPUs found; %d CPUs usable; %d CPUs woken\n",
			mp_ncpus, cpus, smp_cpus);
	}

	smp_active = 1;
	smp_started = 1;
}

SYSINIT(start_aps, SI_SUB_SMP, SI_ORDER_FIRST, cpu_mp_unleash, NULL);

static u_int ipi_msg_cnt[32];

int
powerpc_ipi_handler(void *arg)
{
	cpumask_t self;
	uint32_t ipimask;
	int msg;

	ipimask = atomic_readandclear_32(&(pcpup->pc_ipimask));
	if (ipimask == 0)
		return (FILTER_STRAY);
	while ((msg = ffs(ipimask) - 1) != -1) {
		ipimask &= ~(1u << msg);
		ipi_msg_cnt[msg]++;
		switch (msg) {
		case IPI_AST:
			break;
		case IPI_PREEMPT:
			sched_preempt(curthread);
			break;
		case IPI_RENDEZVOUS:
			smp_rendezvous_action();
			break;
		case IPI_STOP:
			self = PCPU_GET(cpumask);
			savectx(PCPU_GET(curpcb));
			atomic_set_int(&stopped_cpus, self);
			while ((started_cpus & self) == 0)
				cpu_spinwait();
			atomic_clear_int(&started_cpus, self);
			atomic_clear_int(&stopped_cpus, self);
			break;
		case IPI_PPC_TEST:
			mp_ipi_test++;
			break;
		}
	}

	return (FILTER_HANDLED);
}

static void
ipi_send(struct pcpu *pc, int ipi)
{

	atomic_set_32(&pc->pc_ipimask, (1 << ipi));
	PIC_IPI(pic, pc->pc_cpuid);
}

/* Send an IPI to a set of cpus. */
void
ipi_selected(cpumask_t cpus, int ipi)
{
	struct pcpu *pc;

	SLIST_FOREACH(pc, &cpuhead, pc_allcpu) {
		if (cpus & pc->pc_cpumask)
			ipi_send(pc, ipi);
	}
}

/* Send an IPI to all CPUs, including myself. */
void
ipi_all(int ipi)
{
	struct pcpu *pc;

	SLIST_FOREACH(pc, &cpuhead, pc_allcpu) {
		ipi_send(pc, ipi);
	}
}

/* Send an IPI to all CPUs EXCEPT myself. */
void
ipi_all_but_self(int ipi)
{
	struct pcpu *pc;

	SLIST_FOREACH(pc, &cpuhead, pc_allcpu) {
		if (pc != pcpup)
			ipi_send(pc, ipi);
	}
}

/* Send an IPI to myself. */
void
ipi_self(int ipi)
{

	ipi_send(pcpup, ipi);
}
