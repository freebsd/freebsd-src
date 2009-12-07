/*-
 * Copyright (c) 2001-2005 Marcel Moolenaar
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_kstack_pages.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/proc.h>
#include <sys/bus.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/pcpu.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/uuid.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>

#include <machine/atomic.h>
#include <machine/cpu.h>
#include <machine/fpu.h>
#include <machine/intr.h>
#include <machine/mca.h>
#include <machine/md_var.h>
#include <machine/pal.h>
#include <machine/pcb.h>
#include <machine/pmap.h>
#include <machine/sal.h>
#include <machine/smp.h>
#include <i386/include/specialreg.h>

MALLOC_DEFINE(M_SMP, "SMP", "SMP related allocations");

void ia64_ap_startup(void);

#define	LID_SAPIC_ID(x)		((int)((x) >> 24) & 0xff)
#define	LID_SAPIC_EID(x)	((int)((x) >> 16) & 0xff)
#define	LID_SAPIC_SET(id,eid)	(((id & 0xff) << 8 | (eid & 0xff)) << 16);
#define	LID_SAPIC_MASK		0xffff0000UL

/* Variables used by os_boot_rendez and ia64_ap_startup */
struct pcpu *ap_pcpu;
void *ap_stack;
volatile int ap_delay;
volatile int ap_awake;
volatile int ap_spin;

static void cpu_mp_unleash(void *);

struct cpu_group *
cpu_topo(void)
{

	return smp_topo_none();
}

static void
ia64_store_mca_state(void* arg)
{
	unsigned int ncpu = (unsigned int)(uintptr_t)arg;
	struct thread* td;

	/* ia64_mca_save_state() is CPU-sensitive, so bind ourself to our target CPU */
	td = curthread;
	thread_lock(td);
	sched_bind(td, ncpu);
	thread_unlock(td);

	/*
	 * Get and save the CPU specific MCA records. Should we get the
	 * MCA state for each processor, or just the CMC state?
	 */
	ia64_mca_save_state(SAL_INFO_MCA);
	ia64_mca_save_state(SAL_INFO_CMC);

	kproc_exit(0);
}

void
ia64_ap_startup(void)
{
	volatile struct ia64_interrupt_block *ib = IA64_INTERRUPT_BLOCK;
	uint64_t vhpt;
	int vector;

	pcpup = ap_pcpu;
	ia64_set_k4((intptr_t)pcpup);

	vhpt = PCPU_GET(md.vhpt);
	map_vhpt(vhpt);
	ia64_set_pta(vhpt + (1 << 8) + (pmap_vhpt_log2size << 2) + 1);
	ia64_srlz_i();

	ap_awake = 1;
	ap_delay = 0;

	map_pal_code();
	map_gateway_page();

	ia64_set_fpsr(IA64_FPSR_DEFAULT);

	/* Wait until it's time for us to be unleashed */
	while (ap_spin)
		cpu_spinwait();

	/* Initialize curthread. */
	KASSERT(PCPU_GET(idlethread) != NULL, ("no idle thread"));
	PCPU_SET(curthread, PCPU_GET(idlethread));

	atomic_add_int(&ap_awake, 1);
	while (!smp_started)
		cpu_spinwait();

	CTR1(KTR_SMP, "SMP: cpu%d launched", PCPU_GET(cpuid));

	/* Acknowledge and EOI all interrupts. */
	vector = ia64_get_ivr();
	while (vector != 15) {
		ia64_srlz_d();
		if (vector == 0)
			vector = (int)ib->ib_inta;
		ia64_set_eoi(0);
		ia64_srlz_d();
		vector = ia64_get_ivr();
	}
	ia64_srlz_d();

	/* kick off the clock on this AP */
	pcpu_initclock();

	ia64_set_tpr(0);
	ia64_srlz_d();
	enable_intr();

	sched_throw(NULL);
	/* NOTREACHED */
}

void
cpu_mp_setmaxid(void)
{

	/*
	 * Count the number of processors in the system by walking the ACPI
	 * tables. Note that we record the actual number of processors, even
	 * if this is larger than MAXCPU. We only activate MAXCPU processors.
	 */
	mp_ncpus = ia64_count_cpus();

	/*
	 * Set the largest cpuid we're going to use. This is necessary for
	 * VM initialization.
	 */
	mp_maxid = min(mp_ncpus, MAXCPU) - 1;
}

int
cpu_mp_probe(void)
{

	/*
	 * If there's only 1 processor, or we don't have a wake-up vector,
	 * we're not going to enable SMP. Note that no wake-up vector can
	 * also mean that the wake-up mechanism is not supported. In this
	 * case we can have multiple processors, but we simply can't wake
	 * them up...
	 */
	return (mp_ncpus > 1 && ipi_vector[IPI_AP_WAKEUP] != 0);
}

void
cpu_mp_add(u_int acpiid, u_int apicid, u_int apiceid)
{
	struct pcpu *pc;
	u_int64_t lid;
	void *dpcpu;
	u_int cpuid;

	lid = LID_SAPIC_SET(apicid, apiceid);
	cpuid = ((ia64_get_lid() & LID_SAPIC_MASK) == lid) ? 0 : smp_cpus++;

	KASSERT((all_cpus & (1UL << cpuid)) == 0,
	    ("%s: cpu%d already in CPU map", __func__, acpiid));

	if (cpuid != 0) {
		pc = (struct pcpu *)malloc(sizeof(*pc), M_SMP, M_WAITOK);
		pcpu_init(pc, cpuid, sizeof(*pc));
		dpcpu = (void *)kmem_alloc(kernel_map, DPCPU_SIZE);
		dpcpu_init(dpcpu, cpuid);
	} else
		pc = pcpup;

	pc->pc_acpi_id = acpiid;
	pc->pc_md.lid = lid;
	all_cpus |= (1UL << cpuid);
}

void
cpu_mp_announce()
{
	struct pcpu *pc;
	int i;

	for (i = 0; i <= mp_maxid; i++) {
		pc = pcpu_find(i);
		if (pc != NULL) {
			printf("cpu%d: ACPI Id=%x, SAPIC Id=%x, SAPIC Eid=%x",
			    i, pc->pc_acpi_id, LID_SAPIC_ID(pc->pc_md.lid),
			    LID_SAPIC_EID(pc->pc_md.lid));
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

	ap_spin = 1;

	SLIST_FOREACH(pc, &cpuhead, pc_allcpu) {
		pc->pc_md.current_pmap = kernel_pmap;
		pc->pc_other_cpus = all_cpus & ~pc->pc_cpumask;
		if (pc->pc_cpuid > 0) {
			ap_pcpu = pc;
			pc->pc_md.vhpt = pmap_alloc_vhpt();
			if (pc->pc_md.vhpt == 0) {
				printf("SMP: WARNING: unable to allocate VHPT"
				    " for cpu%d", pc->pc_cpuid);
				continue;
			}
			ap_stack = malloc(KSTACK_PAGES * PAGE_SIZE, M_SMP,
			    M_WAITOK);
			ap_delay = 2000;
			ap_awake = 0;

			if (bootverbose)
				printf("SMP: waking up cpu%d\n", pc->pc_cpuid);

			ipi_send(pc, IPI_AP_WAKEUP);

			do {
				DELAY(1000);
			} while (--ap_delay > 0);
			pc->pc_md.awake = ap_awake;

			if (!ap_awake)
				printf("SMP: WARNING: cpu%d did not wake up\n",
				    pc->pc_cpuid);
		} else
			pc->pc_md.awake = 1;
	}
}

static void
cpu_mp_unleash(void *dummy)
{
	struct pcpu *pc;
	int cpus;

	if (mp_ncpus <= 1)
		return;

	cpus = 0;
	smp_cpus = 0;
	SLIST_FOREACH(pc, &cpuhead, pc_allcpu) {
		cpus++;
		if (pc->pc_md.awake) {
			kproc_create(ia64_store_mca_state,
			    (void*)((uintptr_t)pc->pc_cpuid), NULL, 0, 0,
			    "mca %u", pc->pc_cpuid);
			smp_cpus++;
		}
	}

	ap_awake = 1;
	ap_spin = 0;

	while (ap_awake != smp_cpus)
		cpu_spinwait();

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
ipi_selected(cpumask_t cpus, int ipi)
{
	struct pcpu *pc;

	SLIST_FOREACH(pc, &cpuhead, pc_allcpu) {
		if (cpus & pc->pc_cpumask)
			ipi_send(pc, ipi);
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
			ipi_send(pc, ipi);
	}
}

/*
 * Send an IPI to the specified processor. The lid parameter holds the
 * cr.lid (CR64) contents of the target processor. Only the id and eid
 * fields are used here.
 */
void
ipi_send(struct pcpu *cpu, int ipi)
{
	volatile uint64_t *pipi;
	uint64_t vector;

	pipi = __MEMIO_ADDR(ia64_lapic_address |
	    ((cpu->pc_md.lid & LID_SAPIC_MASK) >> 12));
	vector = (uint64_t)(ipi_vector[ipi] & 0xff);
	KASSERT(vector != 0, ("IPI %d is not assigned a vector", ipi));
	*pipi = vector;
	CTR3(KTR_SMP, "ipi_send(%p, %ld), cpuid=%d", pipi, vector,
	    PCPU_GET(cpuid));
}

SYSINIT(start_aps, SI_SUB_SMP, SI_ORDER_FIRST, cpu_mp_unleash, NULL);
