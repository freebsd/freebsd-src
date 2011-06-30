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

#include <machine/atomic.h>
#include <machine/bootinfo.h>
#include <machine/cpu.h>
#include <machine/fpu.h>
#include <machine/intr.h>
#include <machine/mca.h>
#include <machine/md_var.h>
#include <machine/pal.h>
#include <machine/pcb.h>
#include <machine/sal.h>
#include <machine/smp.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>

extern uint64_t bdata[];

MALLOC_DEFINE(M_SMP, "SMP", "SMP related allocations");

void ia64_ap_startup(void);

#define	SAPIC_ID_GET_ID(x)	((u_int)((x) >> 8) & 0xff)
#define	SAPIC_ID_GET_EID(x)	((u_int)(x) & 0xff)
#define	SAPIC_ID_SET(id, eid)	((u_int)(((id) & 0xff) << 8) | ((eid) & 0xff))

/* State used to wake and bootstrap APs. */
struct ia64_ap_state ia64_ap_state;

int ia64_ipi_ast;
int ia64_ipi_hardclock;
int ia64_ipi_highfp;
int ia64_ipi_nmi;
int ia64_ipi_preempt;
int ia64_ipi_rndzvs;
int ia64_ipi_stop;

static u_int
sz2shft(uint64_t sz)
{
	uint64_t s;
	u_int shft;

	shft = 12;      /* Start with 4K */
	s = 1 << shft;
	while (s < sz) {
		shft++;
		s <<= 1;
	}
	return (shft);
}

static u_int
ia64_ih_ast(struct thread *td, u_int xiv, struct trapframe *tf)
{

	PCPU_INC(md.stats.pcs_nasts);
	CTR1(KTR_SMP, "IPI_AST, cpuid=%d", PCPU_GET(cpuid));
	return (0);
}

static u_int
ia64_ih_hardclock(struct thread *td, u_int xiv, struct trapframe *tf)
{

	PCPU_INC(md.stats.pcs_nhardclocks);
	CTR1(KTR_SMP, "IPI_HARDCLOCK, cpuid=%d", PCPU_GET(cpuid));
	hardclockintr();
	return (0);
}

static u_int
ia64_ih_highfp(struct thread *td, u_int xiv, struct trapframe *tf)
{

	PCPU_INC(md.stats.pcs_nhighfps);
	ia64_highfp_save_ipi();
	return (0);
}

static u_int
ia64_ih_preempt(struct thread *td, u_int xiv, struct trapframe *tf)
{

	PCPU_INC(md.stats.pcs_npreempts);
	CTR1(KTR_SMP, "IPI_PREEMPT, cpuid=%d", PCPU_GET(cpuid));
	sched_preempt(curthread);
	return (0);
}

static u_int
ia64_ih_rndzvs(struct thread *td, u_int xiv, struct trapframe *tf)
{

	PCPU_INC(md.stats.pcs_nrdvs);
	CTR1(KTR_SMP, "IPI_RENDEZVOUS, cpuid=%d", PCPU_GET(cpuid));
	smp_rendezvous_action();
	return (0);
}

static u_int
ia64_ih_stop(struct thread *td, u_int xiv, struct trapframe *tf)
{
	cpuset_t mybit;

	PCPU_INC(md.stats.pcs_nstops);
	mybit = PCPU_GET(cpumask);

	savectx(PCPU_PTR(md.pcb));

	CPU_OR_ATOMIC(&stopped_cpus, &mybit);
	while (!CPU_OVERLAP(&started_cpus, &mybit))
		cpu_spinwait();
	CPU_NAND_ATOMIC(&started_cpus, &mybit);
	CPU_NAND_ATOMIC(&stopped_cpus, &mybit);
	return (0);
}

struct cpu_group *
cpu_topo(void)
{

	return smp_topo_none();
}

static void
ia64_store_mca_state(void* arg)
{
	struct pcpu *pc = arg;
	struct thread *td = curthread;

	/*
	 * ia64_mca_save_state() is CPU-sensitive, so bind ourself to our
	 * target CPU.
	 */
	thread_lock(td);
	sched_bind(td, pc->pc_cpuid);
	thread_unlock(td);

	ia64_mca_init_ap();

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
	uint64_t vhpt;

	ia64_ap_state.as_trace = 0x100;

	ia64_set_rr(IA64_RR_BASE(5), (5 << 8) | (PAGE_SHIFT << 2) | 1);
	ia64_set_rr(IA64_RR_BASE(6), (6 << 8) | (PAGE_SHIFT << 2));
	ia64_set_rr(IA64_RR_BASE(7), (7 << 8) | (PAGE_SHIFT << 2));
	ia64_srlz_d();

	pcpup = ia64_ap_state.as_pcpu;
	ia64_set_k4((intptr_t)pcpup);

	ia64_ap_state.as_trace = 0x108;

	vhpt = PCPU_GET(md.vhpt);
	map_vhpt(vhpt);
	ia64_set_pta(vhpt + (1 << 8) + (pmap_vhpt_log2size << 2) + 1);
	ia64_srlz_i();

	ia64_ap_state.as_trace = 0x110;

	ia64_ap_state.as_awake = 1;
	ia64_ap_state.as_delay = 0;

	map_pal_code();
	map_gateway_page();

	ia64_set_fpsr(IA64_FPSR_DEFAULT);

	/* Wait until it's time for us to be unleashed */
	while (ia64_ap_state.as_spin)
		cpu_spinwait();

	/* Initialize curthread. */
	KASSERT(PCPU_GET(idlethread) != NULL, ("no idle thread"));
	PCPU_SET(curthread, PCPU_GET(idlethread));

	atomic_add_int(&ia64_ap_state.as_awake, 1);
	while (!smp_started)
		cpu_spinwait();

	CTR1(KTR_SMP, "SMP: cpu%d launched", PCPU_GET(cpuid));

	cpu_initclocks();

	ia64_set_tpr(0);
	ia64_srlz_d();

	ia64_enable_intr();

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
	return (mp_ncpus > 1 && ia64_ipi_wakeup != 0);
}

void
cpu_mp_add(u_int acpi_id, u_int id, u_int eid)
{
	struct pcpu *pc;
	void *dpcpu;
	u_int cpuid, sapic_id;

	sapic_id = SAPIC_ID_SET(id, eid);
	cpuid = (IA64_LID_GET_SAPIC_ID(ia64_get_lid()) == sapic_id)
	    ? 0 : smp_cpus++;

	KASSERT(!CPU_ISSET(cpuid, &all_cpus),
	    ("%s: cpu%d already in CPU map", __func__, acpi_id));

	if (cpuid != 0) {
		pc = (struct pcpu *)malloc(sizeof(*pc), M_SMP, M_WAITOK);
		pcpu_init(pc, cpuid, sizeof(*pc));
		dpcpu = (void *)kmem_alloc(kernel_map, DPCPU_SIZE);
		dpcpu_init(dpcpu, cpuid);
	} else
		pc = pcpup;

	pc->pc_acpi_id = acpi_id;
	pc->pc_md.lid = IA64_LID_SET_SAPIC_ID(sapic_id);

	CPU_SET(pc->pc_cpuid, &all_cpus);
}

void
cpu_mp_announce()
{
	struct pcpu *pc;
	uint32_t sapic_id;
	int i;

	for (i = 0; i <= mp_maxid; i++) {
		pc = pcpu_find(i);
		if (pc != NULL) {
			sapic_id = IA64_LID_GET_SAPIC_ID(pc->pc_md.lid);
			printf("cpu%d: ACPI Id=%x, SAPIC Id=%x, SAPIC Eid=%x",
			    i, pc->pc_acpi_id, SAPIC_ID_GET_ID(sapic_id),
			    SAPIC_ID_GET_EID(sapic_id));
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
	struct ia64_sal_result result;
	struct ia64_fdesc *fd;
	struct pcpu *pc;
	uintptr_t state;
	u_char *stp;

	state = ia64_tpa((uintptr_t)&ia64_ap_state);
	fd = (struct ia64_fdesc *) os_boot_rendez;
	result = ia64_sal_entry(SAL_SET_VECTORS, SAL_OS_BOOT_RENDEZ,
	    ia64_tpa(fd->func), state, 0, 0, 0, 0);

	ia64_ap_state.as_pgtbl_pte = PTE_PRESENT | PTE_MA_WB |
	    PTE_ACCESSED | PTE_DIRTY | PTE_PL_KERN | PTE_AR_RW |
	    (bootinfo->bi_pbvm_pgtbl & PTE_PPN_MASK);
	ia64_ap_state.as_pgtbl_itir = sz2shft(bootinfo->bi_pbvm_pgtblsz) << 2;
	ia64_ap_state.as_text_va = IA64_PBVM_BASE;
	ia64_ap_state.as_text_pte = PTE_PRESENT | PTE_MA_WB |
	    PTE_ACCESSED | PTE_DIRTY | PTE_PL_KERN | PTE_AR_RX |
	    (ia64_tpa(IA64_PBVM_BASE) & PTE_PPN_MASK);
	ia64_ap_state.as_text_itir = bootinfo->bi_text_mapped << 2;
	ia64_ap_state.as_data_va = (uintptr_t)bdata;
	ia64_ap_state.as_data_pte = PTE_PRESENT | PTE_MA_WB |
	    PTE_ACCESSED | PTE_DIRTY | PTE_PL_KERN | PTE_AR_RW |
	    (ia64_tpa((uintptr_t)bdata) & PTE_PPN_MASK);
	ia64_ap_state.as_data_itir = bootinfo->bi_data_mapped << 2;

	/* Keep 'em spinning until we unleash them... */
	ia64_ap_state.as_spin = 1;

	STAILQ_FOREACH(pc, &cpuhead, pc_allcpu) {
		pc->pc_md.current_pmap = kernel_pmap;
		pc->pc_other_cpus = all_cpus;
		CPU_NAND(&pc->pc_other_cpus, &pc->pc_cpumask);
		/* The BSP is obviously running already. */
		if (pc->pc_cpuid == 0) {
			pc->pc_md.awake = 1;
			continue;
		}

		ia64_ap_state.as_pcpu = pc;
		pc->pc_md.vhpt = pmap_alloc_vhpt();
		if (pc->pc_md.vhpt == 0) {
			printf("SMP: WARNING: unable to allocate VHPT"
			    " for cpu%d", pc->pc_cpuid);
			continue;
		}

		stp = malloc(KSTACK_PAGES * PAGE_SIZE, M_SMP, M_WAITOK);
		ia64_ap_state.as_kstack = stp;
		ia64_ap_state.as_kstack_top = stp + KSTACK_PAGES * PAGE_SIZE;

		ia64_ap_state.as_trace = 0;
		ia64_ap_state.as_delay = 2000;
		ia64_ap_state.as_awake = 0;

		if (bootverbose)
			printf("SMP: waking up cpu%d\n", pc->pc_cpuid);

		/* Here she goes... */
		ipi_send(pc, ia64_ipi_wakeup);
		do {
			DELAY(1000);
		} while (--ia64_ap_state.as_delay > 0);

		pc->pc_md.awake = ia64_ap_state.as_awake;

		if (!ia64_ap_state.as_awake) {
			printf("SMP: WARNING: cpu%d did not wake up (code "
			    "%#lx)\n", pc->pc_cpuid,
			    ia64_ap_state.as_trace - state);
		}
	}
}

static void
cpu_mp_unleash(void *dummy)
{
	struct pcpu *pc;
	int cpus;

	if (mp_ncpus <= 1)
		return;

	/* Allocate XIVs for IPIs */
	ia64_ipi_ast = ia64_xiv_alloc(PI_DULL, IA64_XIV_IPI, ia64_ih_ast);
	ia64_ipi_hardclock = ia64_xiv_alloc(PI_REALTIME, IA64_XIV_IPI,
	    ia64_ih_hardclock);
	ia64_ipi_highfp = ia64_xiv_alloc(PI_AV, IA64_XIV_IPI, ia64_ih_highfp);
	ia64_ipi_preempt = ia64_xiv_alloc(PI_SOFT, IA64_XIV_IPI,
	    ia64_ih_preempt);
	ia64_ipi_rndzvs = ia64_xiv_alloc(PI_AV, IA64_XIV_IPI, ia64_ih_rndzvs);
	ia64_ipi_stop = ia64_xiv_alloc(PI_REALTIME, IA64_XIV_IPI, ia64_ih_stop);

	/* Reserve the NMI vector for IPI_STOP_HARD if possible */
	ia64_ipi_nmi = (ia64_xiv_reserve(2, IA64_XIV_IPI, ia64_ih_stop) != 0)
	    ? ia64_ipi_stop : 0x400;	/* DM=NMI, Vector=n/a */

	cpus = 0;
	smp_cpus = 0;
	STAILQ_FOREACH(pc, &cpuhead, pc_allcpu) {
		cpus++;
		if (pc->pc_md.awake) {
			kproc_create(ia64_store_mca_state, pc, NULL, 0, 0,
			    "mca %u", pc->pc_cpuid);
			smp_cpus++;
		}
	}

	ia64_ap_state.as_awake = 1;
	ia64_ap_state.as_spin = 0;

	while (ia64_ap_state.as_awake != smp_cpus)
		cpu_spinwait();

	if (smp_cpus != cpus || cpus != mp_ncpus) {
		printf("SMP: %d CPUs found; %d CPUs usable; %d CPUs woken\n",
		    mp_ncpus, cpus, smp_cpus);
	}

	smp_active = 1;
	smp_started = 1;

	/*
	 * Now that all CPUs are up and running, bind interrupts to each of
	 * them.
	 */
	ia64_bind_intr();
}

/*
 * send an IPI to a set of cpus.
 */
void
ipi_selected(cpuset_t cpus, int ipi)
{
	struct pcpu *pc;

	STAILQ_FOREACH(pc, &cpuhead, pc_allcpu) {
		if (CPU_OVERLAP(&cpus, &pc->pc_cpumask))
			ipi_send(pc, ipi);
	}
}

/*
 * send an IPI to a specific CPU.
 */
void
ipi_cpu(int cpu, u_int ipi)
{

	ipi_send(cpuid_to_pcpu[cpu], ipi);
}

/*
 * send an IPI to all CPUs EXCEPT myself.
 */
void
ipi_all_but_self(int ipi)
{
	struct pcpu *pc;

	STAILQ_FOREACH(pc, &cpuhead, pc_allcpu) {
		if (pc != pcpup)
			ipi_send(pc, ipi);
	}
}

/*
 * Send an IPI to the specified processor.
 */
void
ipi_send(struct pcpu *cpu, int xiv)
{
	u_int sapic_id;

	KASSERT(xiv != 0, ("ipi_send"));

	sapic_id = IA64_LID_GET_SAPIC_ID(cpu->pc_md.lid);

	ia64_mf();
	ia64_st8(&(ia64_pib->ib_ipi[sapic_id][0]), xiv);
	ia64_mf_a();
	CTR3(KTR_SMP, "ipi_send(%p, %d): cpuid=%d", cpu, xiv, PCPU_GET(cpuid));
}

SYSINIT(start_aps, SI_SUB_SMP, SI_ORDER_FIRST, cpu_mp_unleash, NULL);
