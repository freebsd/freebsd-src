/*-
 * Copyright (c) 1997 Berkeley Software Design, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Berkeley Software Design Inc's name may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN INC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from BSDI: locore.s,v 1.36.2.15 1999/08/23 22:34:41 cp Exp
 */
/*-
 * Copyright (c) 2002 Jake Burkholder.
 * Copyright (c) 2006 Kip Macy <kmacy@FreeBSD.org>.
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

#include "opt_trap_trace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>

#include <dev/ofw/openfirm.h>

#include <machine/asi.h>
#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/ofw_machdep.h>
#include <machine/pcb.h>
#include <machine/smp.h>
#include <machine/tick.h>
#include <machine/pstate.h>
#include <machine/tlb.h>
#include <machine/tte.h>
#include <machine/tte_hash.h>
#include <machine/tsb.h>
#include <machine/trap.h>
#include <machine/hypervisorvar.h>
#include <machine/hv_api.h>
#include <machine/asm.h>

/*
 * Argument area used to pass data to non-boot processors as they start up.
 * This must be statically initialized with a known invalid cpuid,
 * 
 */
struct	cpu_start_args cpu_start_args = { 0, -1, 0, -1 };
struct	ipi_cache_args ipi_cache_args;
struct	ipi_tlb_args ipi_tlb_args;
struct	pcb stoppcbs[MAXCPU];

struct	mtx ipi_mtx;

vm_offset_t mp_tramp;

u_int	mp_boot_mid;

static volatile cpumask_t	shutdown_cpus;

void cpu_mp_unleash(void *);
SYSINIT(cpu_mp_unleash, SI_SUB_SMP, SI_ORDER_FIRST, cpu_mp_unleash, NULL);

#ifdef TRAP_TRACING
#ifndef TRAP_TRACE_ENTRIES
#define TRAP_TRACE_ENTRIES	64
#endif
extern trap_trace_entry_t trap_trace_entry[MAXCPU][TRAP_TRACE_ENTRIES];

static void
mp_trap_trace_init(void)
{
	uint64_t ret, ret1;

	printf("curcpu %d trap_trace_entry %p TRAP_TRACE_ENTRIES %d\n", curcpu, &trap_trace_entry[curcpu][0], TRAP_TRACE_ENTRIES);

	/* Configure the trap trace buffer for the current CPU. */
	if ((ret = hv_ttrace_buf_conf((uint64_t) vtophys(&trap_trace_entry[curcpu][0]),
	    (uint64_t) TRAP_TRACE_ENTRIES, &ret1)) != 0)
		printf("%s: hv_ttrace_buf_conf error %lu\n", __FUNCTION__, ret);

	/* Enable trap tracing for the current CPU. */
	else if ((ret = hv_ttrace_enable((uint64_t) -1, &ret1)) != 0)
		printf("%s: hv_ttrace_enable error %lu\n", __FUNCTION__, ret);
}

void trap_trace_report(int);

static int	trace_trap_lock;

void
trap_trace_report(int cpuid)
{
	int i, j;

	while (!atomic_cmpset_acq_int(&trace_trap_lock, 0, 1))
		DELAY(10000);

	for (i = 0; i < MAXCPU; i++) {
		if (cpuid != -1 && cpuid != i)
			continue;

		for (j = 0; j < TRAP_TRACE_ENTRIES; j++) {
			trap_trace_entry_t *p = &trap_trace_entry[i][j];

			printf("0x%08jx [%02d][%04d] tpc 0x%jx type 0x%x hpstat 0x%x tl %u gl %u tt 0x%hx tag 0x%hx tstate 0x%jx f1 0x%jx f2 0x%jx f3 0x%jx f4 0x%jx\n",
			    p->tte_tick, i, j, p->tte_tpc,p->tte_type,p->tte_hpstat,
			    p->tte_tl,p->tte_gl,p->tte_tt,p->tte_tag,p->tte_tstate,
			    p->tte_f1,p->tte_f2,p->tte_f3,p->tte_f4);
		}
	}

	atomic_store_rel_int(&trace_trap_lock, 0);
}
#endif

vm_offset_t
mp_tramp_alloc(void)
{
	char *v;
	int i;

	v = OF_claim(NULL, PAGE_SIZE, PAGE_SIZE);
	if (v == NULL)
		panic("mp_tramp_alloc");
	bcopy(mp_tramp_code, v, mp_tramp_code_len);

	*(u_long *)(v + mp_tramp_func) = (u_long)mp_startup;

	for (i = 0; i < PAGE_SIZE; i += sizeof(long)*4 /* XXX L1 cacheline size */)
		flush(v + i);
	return (vm_offset_t)v;
}

void
mp_set_tsb_desc_ra(vm_paddr_t tsb_desc_ra)
{
	*(u_long *)(mp_tramp + mp_tramp_tsb_desc_ra) = tsb_desc_ra;
}

void
mp_add_nucleus_mapping(vm_offset_t va, tte_t tte_data)
{
	static int slot;
	uint64_t *entry;

	entry = (uint64_t *)(mp_tramp + mp_tramp_code_len + slot*sizeof(*entry)*2);
	*(entry) = va;
	*(entry + 1) = tte_data;
	*(uint64_t *)(mp_tramp + mp_tramp_tte_slots) = slot + 1;
	slot++;
}

/*
 * Probe for other cpus.
 */
void
cpu_mp_setmaxid(void)
{
	phandle_t child;
	phandle_t root;
	char buf[128];
	int cpus;

	all_cpus = 1 << PCPU_GET(cpuid);
	mp_ncpus = 1;

	cpus = 0;
	root = OF_peer(0);
	for (child = OF_child(root); child != 0; child = OF_peer(child)) {
		if (OF_getprop(child, "device_type", buf, sizeof(buf)) > 0 &&
		    strcmp(buf, "cpu") == 0)
			cpus++;
	}
	mp_maxid = cpus - 1;

}

int
cpu_mp_probe(void)
{
	return (mp_maxid > 0);
}

struct cpu_group *
cpu_topo(void)
{

	return smp_topo_none();
}

static int
start_ap_bycpuid(int cpuid, void *func, u_long arg)
{
	static struct {
		cell_t	name;
		cell_t	nargs;
		cell_t	nreturns;
		cell_t	cpuid;
		cell_t	func;
		cell_t	arg;
		cell_t	result;
	} args = {
		(cell_t)"SUNW,start-cpu-by-cpuid",
		3,
		1,
		0,
		0,
		0,
		0
	};

	args.cpuid = cpuid;
	args.func = (cell_t)func;
	args.arg = (cell_t)arg;
	ofw_entry(&args);
	return (int)args.result;
	
}

/*
 * Fire up any non-boot processors.
 */
void
cpu_mp_start(void)
{
	volatile struct cpu_start_args *csa;
	struct pcpu *pc;
	phandle_t child;
	phandle_t root;
	vm_offset_t va;
	char buf[128];
	u_int clock;
	int cpuid, bp_skipped;
	u_long s;

	root = OF_peer(0);
	csa = &cpu_start_args;
	clock = cpuid = bp_skipped = 0;
	for (child = OF_child(root); child != 0; child = OF_peer(child)) {
		if (OF_getprop(child, "device_type", buf, sizeof(buf)) <= 0 ||
		    strcmp(buf, "cpu") != 0)
			continue;
		/* skip boot processor */
		if (!bp_skipped) {
			bp_skipped = 1;
			continue;
		}
		cpuid++;

		if (OF_getprop(child, "clock-frequency", &clock,
		    sizeof(clock)) <= 0)
			panic("cpu_mp_start: can't get clock");

		csa->csa_state = 0;
		start_ap_bycpuid(cpuid, (void *)mp_tramp, (uint64_t)cpuid);
		s = intr_disable();
		while (csa->csa_state != CPU_INIT)
			;
		intr_restore(s);
		mp_ncpus = cpuid + 1;
#if 0
		cpu_identify(0, clock, cpuid);
#endif
		va = kmem_alloc(kernel_map, PCPU_PAGES * PAGE_SIZE);
		pc = (struct pcpu *)(va + (PCPU_PAGES * PAGE_SIZE)) - 1;
		pcpu_init(pc, cpuid, sizeof(*pc));
		dpcpu_init((void *)kmem_alloc(kernel_map, DPCPU_SIZE),
		    cpuid);
		pc->pc_addr = va;

		all_cpus |= 1 << cpuid;

		if (mp_ncpus == MAXCPU)
			break;
	}
	printf("%d cpus: UltraSparc T1 Processor (%d.%02d MHz CPU)\n", mp_ncpus,
	    (clock + 4999) / 1000000, ((clock + 4999) / 10000) % 100);

	PCPU_SET(other_cpus, all_cpus & ~(1 << PCPU_GET(cpuid)));
	smp_active = 1;
}

void
cpu_mp_announce(void)
{
}

void
cpu_mp_unleash(void *v)
{
	volatile struct cpu_start_args *csa;
	struct pcpu *pc;
	u_long s;

	csa = &cpu_start_args;
	csa->csa_count = mp_ncpus;
	printf("mp_ncpus=%d\n", mp_ncpus);
	SLIST_FOREACH(pc, &cpuhead, pc_allcpu) {
		if (pc->pc_cpuid == PCPU_GET(cpuid)) 
			continue;

		KASSERT(pc->pc_idlethread != NULL,
		    ("cpu_mp_unleash: idlethread is NULL"));
		pc->pc_curthread = pc->pc_idlethread;	
		pc->pc_curpcb = pc->pc_curthread->td_pcb;
		pc->pc_curpmap = kernel_pmap;
		csa->csa_state = 0;
		csa->csa_pcpu = TLB_PHYS_TO_DIRECT(vtophys(pc->pc_addr));
		DELAY(300);
		/* allow AP to run */
		csa->csa_cpuid = pc->pc_cpuid;
		membar(Sync);
		s = intr_disable();
		while (csa->csa_state != CPU_BOOTSTRAP)
			;
		intr_restore(s);
	}

	membar(StoreLoad);
	csa->csa_count = 0; 
	smp_started = 1;
}

void
cpu_mp_bootstrap(struct pcpu *pc)
{
	volatile struct cpu_start_args *csa;

	csa = &cpu_start_args;
	cpu_setregs(pc);
	tsb_set_scratchpad_kernel(&kernel_pmap->pm_tsb);
	tte_hash_set_scratchpad_kernel(kernel_pmap->pm_hash);
	trap_init();
	cpu_intrq_init();

#ifdef TRAP_TRACING
	mp_trap_trace_init();
#endif

	/* 
	 * enable interrupts now that we have our trap table set
	 */
	intr_restore_all(PSTATE_KERNEL);

	smp_cpus++;
	KASSERT(curthread != NULL, ("cpu_mp_bootstrap: curthread"));
	PCPU_SET(other_cpus, all_cpus & ~(1 << curcpu));
	printf("AP: #%d\n", curcpu);
	csa->csa_count--;
	membar(StoreLoad);
	csa->csa_state = CPU_BOOTSTRAP;

	while (csa->csa_count != 0)
		;

	/* Start per-CPU event timers. */
	cpu_initclocks_ap();

	/* ok, now enter the scheduler */
	sched_throw(NULL);
}

void
cpu_mp_shutdown(void)
{
	int i;

	critical_enter();
	shutdown_cpus = PCPU_GET(other_cpus);
	if (stopped_cpus != PCPU_GET(other_cpus))	/* XXX */
		stop_cpus(stopped_cpus ^ PCPU_GET(other_cpus));
	i = 0;
	while (shutdown_cpus != 0) {
		if (i++ > 100000) {
			printf("timeout shutting down CPUs.\n");
			break;
		}
	}
	/* XXX: delay a bit to allow the CPUs to actually enter the PROM. */
	DELAY(100000);
	critical_exit();
}

void
cpu_ipi_ast(struct trapframe *tf)
{
}

void
cpu_ipi_stop(struct trapframe *tf)
{

	CTR1(KTR_SMP, "cpu_ipi_stop: stopped %d", curcpu);
	savectx(&stoppcbs[curcpu]);
	atomic_set_acq_int(&stopped_cpus, PCPU_GET(cpumask));
	while ((started_cpus & PCPU_GET(cpumask)) == 0) {
		if ((shutdown_cpus & PCPU_GET(cpumask)) != 0) {
			atomic_clear_int(&shutdown_cpus, PCPU_GET(cpumask));
		}
	}
	atomic_clear_rel_int(&started_cpus, PCPU_GET(cpumask));
	atomic_clear_rel_int(&stopped_cpus, PCPU_GET(cpumask));
	CTR1(KTR_SMP, "cpu_ipi_stop: restarted %d", curcpu);
}

void
cpu_ipi_preempt(struct trapframe *tf)
{
	sched_preempt(curthread);
}

void
cpu_ipi_hardclock(struct trapframe *tf)
{
	struct trapframe *oldframe;
	struct thread *td;

	critical_enter();
	td = curthread;
	td->td_intr_nesting_level++;
	oldframe = td->td_intr_frame;
	td->td_intr_frame = tf;
	hardclockintr();
	td->td_intr_frame = oldframe;
	td->td_intr_nesting_level--;
	critical_exit();
}

void
cpu_ipi_selected(int cpu_count, uint16_t *cpulist, u_long d0, u_long d1, u_long d2, uint64_t *ackmask)
{

	int i, retries;

	init_mondo(d0, d1, d2, (uint64_t)pmap_kextract((vm_offset_t)ackmask));

	retries = 0;

retry:
	if (cpu_count) {
		int error, new_cpu_count;
		vm_paddr_t cpulist_ra;

		cpulist_ra = TLB_DIRECT_TO_PHYS((vm_offset_t)cpulist);
		if ((error = hv_cpu_mondo_send(cpu_count, cpulist_ra)) == H_EWOULDBLOCK) {
			new_cpu_count = 0;
			for (i = 0; i < cpu_count; i++) {
				if (cpulist[i] != 0xffff)
					cpulist[new_cpu_count++] = cpulist[i];
			}      
			cpu_count = new_cpu_count;
			retries++;
			if (cpu_count == 0) {
				printf("no more cpus to send to but mondo_send returned EWOULDBLOCK\n");
				return;
			}
			if ((retries & 0x1) == 0x1)
				DELAY(10);

			if (retries < 50000)
				goto retry;
			else {
				printf("used up retries - cpus remaining: %d  - cpus: ",
				       cpu_count);
				for (i = 0; i < cpu_count; i++)
					printf("#%d ", cpulist[i]);
				printf("\n");
			}
		}
		if (error == H_ENOCPU) {
			printf("bad cpuid: ");
			for (i = 0; i < cpu_count; i++)
				printf("#%d ", cpulist[i]);
			printf("\n");
		}		
		if (error)
			panic("can't handle error %d from cpu_mondo_send\n", error);
	}
}

void
ipi_selected(cpumask_t icpus, u_int ipi)
{
	int i, cpu_count;
	uint16_t *cpulist;
	cpumask_t cpus;
	uint64_t ackmask;

	/* 
	 * 
	 * 3) forward_wakeup appears to abuse ASTs
	 * 4) handling 4-way threading vs 2-way threading should happen here
	 *    and not in forward wakeup
	 */
	cpulist = PCPU_GET(cpulist);
	cpus = (icpus & ~PCPU_GET(cpumask));
	
	for (cpu_count = 0, i = 0; i < 32 && cpus; cpus = cpus >> 1, i++) {
		if (!(cpus & 0x1))
			continue;
		
		cpulist[cpu_count] = (uint16_t)i;
		cpu_count++;
	}

	cpu_ipi_selected(cpu_count, cpulist, (u_long)tl_ipi_level, ipi, 0,
	    &ackmask);
}

void
ipi_cpu(int cpu, u_int ipi)
{
	int cpu_count;
	uint16_t *cpulist;
	uint64_t ackmask;

	/* 
	 * 
	 * 3) forward_wakeup appears to abuse ASTs
	 * 4) handling 4-way threading vs 2-way threading should happen here
	 *    and not in forward wakeup
	 */
	cpulist = PCPU_GET(cpulist);
	if (PCPU_GET(cpumask) & (1 << cpu))
		cpu_count = 0;
	else {
		cpulist[0] = (uint16_t)cpu;
		cpu_count = 1;
	}
	cpu_ipi_selected(cpu_count, cpulist, (u_long)tl_ipi_level, ipi, 0,
	    &ackmask);
}

void
ipi_all_but_self(u_int ipi)
{
	ipi_selected(PCPU_GET(other_cpus), ipi);
}
