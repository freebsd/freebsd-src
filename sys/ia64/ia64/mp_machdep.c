/*-
 * Copyright (c) 2000 Doug Rabson
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
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/pcpu.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>

#include <machine/atomic.h>
#include <machine/pal.h>
#include <machine/pmap.h>
#include <machine/clock.h>
#include <machine/sal.h>

void cpu_mp_add(uint, uint, uint);
void ia64_ap_startup(void);
void map_pal_code(void);

extern vm_offset_t vhpt_base, vhpt_size;

#define	LID_SAPIC_ID(x)		((int)((x) >> 24) & 0xff)
#define	LID_SAPIC_EID(x)	((int)((x) >> 16) & 0xff)
#define	LID_SAPIC_SET(id,eid)	(((id & 0xff) << 8 | (eid & 0xff)) << 16);
#define	LID_SAPIC_MASK		0xffff0000UL

int	mp_hardware = 0;
int	mp_ipi_vector[IPI_COUNT];
int	mp_ipi_test = 0;

/* Variables used by os_boot_rendez */
volatile vm_offset_t ap_stack;
volatile struct pcpu *ap_pcpu;
volatile int ap_delay;
volatile int ap_awake;

static void ipi_send(u_int64_t, int);
static void cpu_mp_unleash(void *);

void
ia64_ap_startup(void)
{
	__asm __volatile("mov cr.pta=%0;; srlz.i;;" ::
	    "r" (vhpt_base + (1<<8) + (vhpt_size<<2) + 1));

	ap_awake = 1;
	ap_delay = 0;

	/* Wait until it's time for us to be unleashed */
	while (!smp_started);

	CTR1(KTR_SMP, "SMP: cpu%d launched", PCPU_GET(cpuid));

	__asm __volatile("ssm psr.ic|psr.i;; srlz.i;;");

	microuptime(PCPU_PTR(switchtime));
	PCPU_SET(switchticks, ticks);

	mtx_lock_spin(&sched_lock);
	cpu_throw();
	panic(__func__ ": cpu_throw() returned");
}

int
cpu_mp_probe()
{
	/*
	 * We've already discovered any APs when they're present.
	 * Just return the result here.
	 */
	return (mp_hardware && mp_ncpus > 1);
}

void
cpu_mp_add(uint acpiid, uint apicid, uint apiceid)
{
	struct pcpu *pc;
	u_int64_t lid;

	/* Count all CPUs, even the ones we cannot use */
	mp_ncpus++;

	/* Ignore any processor numbers outside our range */
	if (acpiid >= MAXCPU) {
		printf("SMP: cpu%d skipped; increase MAXCPU\n", acpiid);
		return;
	}

	KASSERT((all_cpus & (1UL << acpiid)) == 0,
	    (__func__ ": cpu%d already in CPU map", acpiid));

	lid = LID_SAPIC_SET(apicid, apiceid);

	if ((ia64_get_lid() & LID_SAPIC_MASK) == lid) {
		KASSERT(acpiid == 0,
		    (__func__ ": the BSP must be cpu0"));
	}

	if (acpiid != 0) {
		pc = (struct pcpu *)kmem_alloc(kernel_map, PAGE_SIZE);
		pcpu_init(pc, acpiid, PAGE_SIZE);
	} else
		pc = pcpup;

	pc->pc_lid = lid;
	all_cpus |= (1UL << acpiid);
}

void
cpu_mp_announce()
{
	struct pcpu *pc;
	int i;

	for (i = 0; i < MAXCPU; i++) {
		pc = pcpu_find(i);
		if (pc != NULL) {
			printf("cpu%d: SAPIC Id=%x, SAPIC Eid=%x", i,
			    LID_SAPIC_ID(pc->pc_lid),
			    LID_SAPIC_EID(pc->pc_lid));
			if (i == 0)
				printf(" (BSP)\n");
			else
				printf("\n");
		}
	}
}

void
cpu_mp_start()
{
	struct pcpu *pc;

	SLIST_FOREACH(pc, &cpuhead, pc_allcpu) {
#if 0
		pc->pc_current_pmap = PCPU_GET(current_pmap);
#endif
		pc->pc_other_cpus = all_cpus & ~pc->pc_cpumask;
		if (pc->pc_cpuid > 0) {
			ap_stack = kmem_alloc(kernel_map,
			    KSTACK_PAGES * PAGE_SIZE);
			ap_pcpu = pc;
			ap_delay = 2000;
			ap_awake = 0;

			if (bootverbose)
				printf("SMP: waking up cpu%d\n", pc->pc_cpuid);

			ipi_send(pc->pc_lid, IPI_AP_WAKEUP);

			do {
				DELAY(1000);
			} while (--ap_delay > 0);
			pc->pc_awake = ap_awake;

			if (!ap_awake)
				printf("SMP: WARNING: cpu%d did not wake up\n",
				    pc->pc_cpuid);
		} else {
			pc->pc_awake = 1;
			ipi_self(IPI_TEST);
		}
	}
}

static void
cpu_mp_unleash(void *dummy)
{
	struct pcpu *pc;
	int cpus;

	if (!mp_hardware)
		return;

	if (mp_ipi_test != 1)
		printf("SMP: WARNING: sending of a test IPI failed\n");

	cpus = 0;
	smp_cpus = 0;
	SLIST_FOREACH(pc, &cpuhead, pc_allcpu) {
		cpus++;
		if (pc->pc_awake)
			smp_cpus++;
	}

	if (smp_cpus != cpus || cpus != mp_ncpus) {
		printf("SMP: %d CPUs found; %d CPUs usable; %d CPUs woken\n",
		    mp_ncpus, cpus, smp_cpus);
	}

	smp_active = 1;
	smp_started = 1;
}

/*
 * send an IPI to a set of cpus.
 */
void
ipi_selected(u_int64_t cpus, int ipi)
{
	struct pcpu *pc;

	SLIST_FOREACH(pc, &cpuhead, pc_allcpu) {
		if (cpus & pc->pc_cpumask)
			ipi_send(pc->pc_lid, ipi);
	}
}

/*
 * send an IPI to all CPUs, including myself.
 */
void
ipi_all(int ipi)
{
	struct pcpu *pc;

	SLIST_FOREACH(pc, &cpuhead, pc_allcpu) {
		ipi_send(pc->pc_lid, ipi);
	}
}

/*
 * send an IPI to all CPUs EXCEPT myself.
 */
void
ipi_all_but_self(int ipi)
{
	struct pcpu *pc;

	SLIST_FOREACH(pc, &cpuhead, pc_allcpu) {
		if (pc != pcpup)
			ipi_send(pc->pc_lid, ipi);
	}
}

/*
 * send an IPI to myself.
 */
void
ipi_self(int ipi)
{

	ipi_send(ia64_get_lid(), ipi);
}

/*
 * Send an IPI to the specified processor. The lid parameter holds the
 * cr.lid (CR64) contents of the target processor. Only the id and eid
 * fields are used here.
 */
static void
ipi_send(u_int64_t lid, int ipi)
{
	volatile u_int64_t *pipi;
	u_int64_t vector;

	pipi = ia64_memory_address(PAL_PIB_DEFAULT_ADDR |
	    ((lid & LID_SAPIC_MASK) >> 12));
	vector = (u_int64_t)(mp_ipi_vector[ipi] & 0xff);
	*pipi = vector;
	ia64_mf_a();
}

SYSINIT(start_aps, SI_SUB_SMP, SI_ORDER_FIRST, cpu_mp_unleash, NULL);
