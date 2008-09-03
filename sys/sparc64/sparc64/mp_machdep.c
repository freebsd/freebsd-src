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
 * Copyright (c) 2007 Marius Strobl <marius@FreeBSD.org>
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
#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/ofw_machdep.h>
#include <machine/pcb.h>
#include <machine/smp.h>
#include <machine/tick.h>
#include <machine/tlb.h>
#include <machine/tte.h>
#include <machine/ver.h>

static ih_func_t cpu_ipi_ast;
static ih_func_t cpu_ipi_preempt;
static ih_func_t cpu_ipi_stop;

/*
 * Argument area used to pass data to non-boot processors as they start up.
 * This must be statically initialized with a known invalid CPU module ID,
 * since the other processors will use it before the boot CPU enters the
 * kernel.
 */
struct	cpu_start_args cpu_start_args = { 0, -1, -1, 0, 0, 0 };
struct	ipi_cache_args ipi_cache_args;
struct	ipi_tlb_args ipi_tlb_args;
struct	pcb stoppcbs[MAXCPU];

struct	mtx ipi_mtx;

cpu_ipi_selected_t *cpu_ipi_selected;

static vm_offset_t mp_tramp;
static u_int cpuid_to_mid[MAXCPU];
static int isjbus;
static volatile u_int shutdown_cpus;

static void cpu_mp_unleash(void *v);
static void spitfire_ipi_send(u_int mid, u_long d0, u_long d1, u_long d2);
static void sun4u_startcpu(phandle_t cpu, void *func, u_long arg);
static void sun4u_stopself(void);

static cpu_ipi_selected_t cheetah_ipi_selected;
static cpu_ipi_selected_t spitfire_ipi_selected;

SYSINIT(cpu_mp_unleash, SI_SUB_SMP, SI_ORDER_FIRST, cpu_mp_unleash, NULL);

CTASSERT(MAXCPU <= IDR_CHEETAH_MAX_BN_PAIRS);
CTASSERT(MAXCPU <= sizeof(u_int) * NBBY);
CTASSERT(MAXCPU <= sizeof(int) * NBBY);

void
mp_init(void)
{
	struct tte *tp;
	int i;

	mp_tramp = (vm_offset_t)OF_claim(NULL, PAGE_SIZE, PAGE_SIZE);
	if (mp_tramp == (vm_offset_t)-1)
		panic("%s", __func__);
	bcopy(mp_tramp_code, (void *)mp_tramp, mp_tramp_code_len);
	*(vm_offset_t *)(mp_tramp + mp_tramp_tlb_slots) = kernel_tlb_slots;
	*(vm_offset_t *)(mp_tramp + mp_tramp_func) = (vm_offset_t)mp_startup;
	tp = (struct tte *)(mp_tramp + mp_tramp_code_len);
	for (i = 0; i < kernel_tlb_slots; i++) {
		tp[i].tte_vpn = TV_VPN(kernel_tlbs[i].te_va, TS_4M);
		tp[i].tte_data = TD_V | TD_4M | TD_PA(kernel_tlbs[i].te_pa) |
		    TD_L | TD_CP | TD_CV | TD_P | TD_W;
	}
	for (i = 0; i < PAGE_SIZE; i += sizeof(vm_offset_t))
		flush(mp_tramp + i);

	/*
	 * On UP systems cpu_ipi_selected() can be called while
	 * cpu_mp_start() wasn't so initialize these here.
	 */
	if (cpu_impl == CPU_IMPL_ULTRASPARCIIIi ||
	    cpu_impl == CPU_IMPL_ULTRASPARCIIIip)
		isjbus = 1;
	if (cpu_impl >= CPU_IMPL_ULTRASPARCIII)
		cpu_ipi_selected = cheetah_ipi_selected;
	else
		cpu_ipi_selected = spitfire_ipi_selected;
}

/*
 * Probe for other CPUs.
 */
void
cpu_mp_setmaxid(void)
{
	char buf[128];
	phandle_t child;
	u_int cpus;

	all_cpus = 1 << curcpu;
	mp_ncpus = 1;

	cpus = 0;
	for (child = OF_child(OF_peer(0)); child != 0; child = OF_peer(child))
		if (OF_getprop(child, "device_type", buf, sizeof(buf)) > 0 &&
		    strcmp(buf, "cpu") == 0)
			cpus++;
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

	return (smp_topo_none());
}

static void
sun4u_startcpu(phandle_t cpu, void *func, u_long arg)
{
	static struct {
		cell_t	name;
		cell_t	nargs;
		cell_t	nreturns;
		cell_t	cpu;
		cell_t	func;
		cell_t	arg;
	} args = {
		(cell_t)"SUNW,start-cpu",
		3,
	};

	args.cpu = cpu;
	args.func = (cell_t)func;
	args.arg = (cell_t)arg;
	openfirmware(&args);
}

/*
 * Stop the calling CPU.
 */
static void
sun4u_stopself(void)
{
	static struct {
		cell_t	name;
		cell_t	nargs;
		cell_t	nreturns;
	} args = {
		(cell_t)"SUNW,stop-self",
	};

	openfirmware_exit(&args);
	panic("%s: failed.", __func__);
}

/*
 * Fire up any non-boot processors.
 */
void
cpu_mp_start(void)
{
	char buf[128];
	volatile struct cpu_start_args *csa;
	struct pcpu *pc;
	register_t s;
	vm_offset_t va;
	phandle_t child;
	u_int mid;
	u_int clock;
	u_int cpuid;

	mtx_init(&ipi_mtx, "ipi", NULL, MTX_SPIN);

	intr_setup(PIL_AST, cpu_ipi_ast, -1, NULL, NULL);
	intr_setup(PIL_RENDEZVOUS, (ih_func_t *)smp_rendezvous_action,
	    -1, NULL, NULL);
	intr_setup(PIL_STOP, cpu_ipi_stop, -1, NULL, NULL);
	intr_setup(PIL_PREEMPT, cpu_ipi_preempt, -1, NULL, NULL);

	cpuid_to_mid[curcpu] = PCPU_GET(mid);

	csa = &cpu_start_args;
	for (child = OF_child(OF_peer(0)); child != 0 && mp_ncpus <= MAXCPU;
	    child = OF_peer(child)) {
		if (OF_getprop(child, "device_type", buf, sizeof(buf)) <= 0 ||
		    strcmp(buf, "cpu") != 0)
			continue;
		if (OF_getprop(child, cpu_impl < CPU_IMPL_ULTRASPARCIII ?
		    "upa-portid" : "portid", &mid, sizeof(mid)) <= 0)
			panic("%s: can't get module ID", __func__);
		if (mid == PCPU_GET(mid))
			continue;
		if (OF_getprop(child, "clock-frequency", &clock,
		    sizeof(clock)) <= 0)
			panic("%s: can't get clock", __func__);

		csa->csa_state = 0;
		sun4u_startcpu(child, (void *)mp_tramp, 0);
		s = intr_disable();
		while (csa->csa_state != CPU_TICKSYNC)
			;
		membar(StoreLoad);
		csa->csa_tick = rd(tick);
		if (cpu_impl >= CPU_IMPL_ULTRASPARCIII) {
			while (csa->csa_state != CPU_STICKSYNC)
				;
			membar(StoreLoad);
			csa->csa_stick = rdstick();
		}
		while (csa->csa_state != CPU_INIT)
			;
		csa->csa_tick = csa->csa_stick = 0;
		intr_restore(s);

		cpuid = mp_ncpus++;
		cpuid_to_mid[cpuid] = mid;
		cpu_identify(csa->csa_ver, clock, cpuid);

		va = kmem_alloc(kernel_map, PCPU_PAGES * PAGE_SIZE);
		pc = (struct pcpu *)(va + (PCPU_PAGES * PAGE_SIZE)) - 1;
		pcpu_init(pc, cpuid, sizeof(*pc));
		pc->pc_addr = va;
		pc->pc_clock = clock;
		pc->pc_mid = mid;
		pc->pc_node = child;

		cache_init(pc);

		all_cpus |= 1 << cpuid;
		intr_add_cpu(cpuid);
	}
	KASSERT(!isjbus || mp_ncpus <= IDR_JALAPENO_MAX_BN_PAIRS,
	    ("%s: can only IPI a maximum of %d JBus-CPUs",
	    __func__, IDR_JALAPENO_MAX_BN_PAIRS));
	PCPU_SET(other_cpus, all_cpus & ~(1 << curcpu));
	smp_active = 1;
}

void
cpu_mp_announce(void)
{

}

static void
cpu_mp_unleash(void *v)
{
	volatile struct cpu_start_args *csa;
	struct pcpu *pc;
	register_t s;
	vm_offset_t va;
	vm_paddr_t pa;
	u_int ctx_inc;
	u_int ctx_min;
	int i;

	ctx_min = TLB_CTX_USER_MIN;
	ctx_inc = (TLB_CTX_USER_MAX - 1) / mp_ncpus;
	csa = &cpu_start_args;
	csa->csa_count = mp_ncpus;
	SLIST_FOREACH(pc, &cpuhead, pc_allcpu) {
		pc->pc_tlb_ctx = ctx_min;
		pc->pc_tlb_ctx_min = ctx_min;
		pc->pc_tlb_ctx_max = ctx_min + ctx_inc;
		ctx_min += ctx_inc;

		if (pc->pc_cpuid == curcpu)
			continue;
		KASSERT(pc->pc_idlethread != NULL,
		    ("%s: idlethread", __func__));
		pc->pc_curthread = pc->pc_idlethread;
		pc->pc_curpcb = pc->pc_curthread->td_pcb;
		for (i = 0; i < PCPU_PAGES; i++) {
			va = pc->pc_addr + i * PAGE_SIZE;
			pa = pmap_kextract(va);
			if (pa == 0)
				panic("%s: pmap_kextract", __func__);
			csa->csa_ttes[i].tte_vpn = TV_VPN(va, TS_8K);
			csa->csa_ttes[i].tte_data = TD_V | TD_8K | TD_PA(pa) |
			    TD_L | TD_CP | TD_CV | TD_P | TD_W;
		}
		csa->csa_state = 0;
		csa->csa_pcpu = pc->pc_addr;
		csa->csa_mid = pc->pc_mid;
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
	pmap_map_tsb();
	/*
	 * Flush all non-locked TLB entries possibly left over by the
	 * firmware.
	 */
	tlb_flush_nonlocked();
	cpu_setregs(pc);
	tick_start();

	smp_cpus++;
	KASSERT(curthread != NULL, ("%s: curthread", __func__));
	PCPU_SET(other_cpus, all_cpus & ~(1 << curcpu));
	printf("SMP: AP CPU #%d Launched!\n", curcpu);

	csa->csa_count--;
	membar(StoreLoad);
	csa->csa_state = CPU_BOOTSTRAP;
	while (csa->csa_count != 0)
		;

	/* Ok, now enter the scheduler. */
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

static void
cpu_ipi_ast(struct trapframe *tf)
{

}

static void
cpu_ipi_stop(struct trapframe *tf)
{

	CTR2(KTR_SMP, "%s: stopped %d", __func__, curcpu);
	savectx(&stoppcbs[curcpu]);
	atomic_set_acq_int(&stopped_cpus, PCPU_GET(cpumask));
	while ((started_cpus & PCPU_GET(cpumask)) == 0) {
		if ((shutdown_cpus & PCPU_GET(cpumask)) != 0) {
			atomic_clear_int(&shutdown_cpus, PCPU_GET(cpumask));
			sun4u_stopself();
		}
	}
	atomic_clear_rel_int(&started_cpus, PCPU_GET(cpumask));
	atomic_clear_rel_int(&stopped_cpus, PCPU_GET(cpumask));
	CTR2(KTR_SMP, "%s: restarted %d", __func__, curcpu);
}

static void
cpu_ipi_preempt(struct trapframe *tf)
{

	sched_preempt(curthread);
}

static void
spitfire_ipi_selected(u_int cpus, u_long d0, u_long d1, u_long d2)
{
	u_int cpu;

	KASSERT((cpus & (1 << curcpu)) == 0,
	    ("%s: CPU can't IPI itself", __func__));
	while (cpus) {
		cpu = ffs(cpus) - 1;
		cpus &= ~(1 << cpu);
		spitfire_ipi_send(cpuid_to_mid[cpu], d0, d1, d2);
	}
}

static void
spitfire_ipi_send(u_int mid, u_long d0, u_long d1, u_long d2)
{
	register_t s;
	u_long ids;
	int i;

	KASSERT((ldxa(0, ASI_INTR_DISPATCH_STATUS) & IDR_BUSY) == 0,
	    ("%s: outstanding dispatch", __func__));
	for (i = 0; i < IPI_RETRIES; i++) {
		s = intr_disable();
		stxa(AA_SDB_INTR_D0, ASI_SDB_INTR_W, d0);
		stxa(AA_SDB_INTR_D1, ASI_SDB_INTR_W, d1);
		stxa(AA_SDB_INTR_D2, ASI_SDB_INTR_W, d2);
		membar(Sync);
		stxa(AA_INTR_SEND | (mid << IDC_ITID_SHIFT),
		    ASI_SDB_INTR_W, 0);
		/*
		 * Workaround for SpitFire erratum #54; do a dummy read
		 * from a SDB internal register before the MEMBAR #Sync
		 * for the write to ASI_SDB_INTR_W (requiring another
		 * MEMBAR #Sync in order to make sure the write has
		 * occurred before the load).
		 */
		membar(Sync);
		(void)ldxa(AA_SDB_CNTL_HIGH, ASI_SDB_CONTROL_R);
		membar(Sync);
		while (((ids = ldxa(0, ASI_INTR_DISPATCH_STATUS)) &
		    IDR_BUSY) != 0)
			;
		intr_restore(s);
		if ((ids & (IDR_BUSY | IDR_NACK)) == 0)
			return;
		/*
		 * Leave interrupts enabled for a bit before retrying
		 * in order to avoid deadlocks if the other CPU is also
		 * trying to send an IPI.
		 */
		DELAY(2);
	}
	if (
#ifdef KDB
	    kdb_active ||
#endif
	    panicstr != NULL)
		printf("%s: couldn't send IPI to module 0x%u\n",
		    __func__, mid);
	else
		panic("%s: couldn't send IPI", __func__);
}

static void
cheetah_ipi_selected(u_int cpus, u_long d0, u_long d1, u_long d2)
{
	register_t s;
	u_long ids;
	u_int bnp;
	u_int cpu;
	int i;

	KASSERT((cpus & (1 << curcpu)) == 0,
	    ("%s: CPU can't IPI itself", __func__));
	KASSERT((ldxa(0, ASI_INTR_DISPATCH_STATUS) &
	    IDR_CHEETAH_ALL_BUSY) == 0,
	    ("%s: outstanding dispatch", __func__));
	if (cpus == 0)
		return;
	ids = 0;
	for (i = 0; i < IPI_RETRIES * mp_ncpus; i++) {
		s = intr_disable();
		stxa(AA_SDB_INTR_D0, ASI_SDB_INTR_W, d0);
		stxa(AA_SDB_INTR_D1, ASI_SDB_INTR_W, d1);
		stxa(AA_SDB_INTR_D2, ASI_SDB_INTR_W, d2);
		membar(Sync);
		bnp = 0;
		for (cpu = 0; cpu < mp_ncpus; cpu++) {
			if ((cpus & (1 << cpu)) != 0) {
				stxa(AA_INTR_SEND |
				    (cpuid_to_mid[cpu] << IDC_ITID_SHIFT) |
				    (isjbus ? 0 : bnp << IDC_BN_SHIFT),
				    ASI_SDB_INTR_W, 0);
				membar(Sync);
				bnp++;
			}
		}
		while (((ids = ldxa(0, ASI_INTR_DISPATCH_STATUS)) &
		    IDR_CHEETAH_ALL_BUSY) != 0)
			;
		intr_restore(s);
		if ((ids & (IDR_CHEETAH_ALL_BUSY | IDR_CHEETAH_ALL_NACK)) == 0)
			return;
		bnp = 0;
		for (cpu = 0; cpu < mp_ncpus; cpu++) {
			if ((cpus & (1 << cpu)) != 0) {
				if ((ids & (IDR_NACK << (isjbus ?
				    (2 * cpuid_to_mid[cpu]) :
				    (2 * bnp)))) == 0)
					cpus &= ~(1 << cpu);
				bnp++;
			}
		}
		/*
		 * Leave interrupts enabled for a bit before retrying
		 * in order to avoid deadlocks if the other CPUs are
		 * also trying to send IPIs.
		 */
		DELAY(2 * bnp);
	}
	if (
#ifdef KDB
	    kdb_active ||
#endif
	    panicstr != NULL)
		printf("%s: couldn't send IPI (cpus=0x%u ids=0x%lu)\n",
		    __func__, cpus, ids);
	else
		panic("%s: couldn't send IPI", __func__);
}

void
ipi_selected(u_int cpus, u_int ipi)
{

	cpu_ipi_selected(cpus, 0, (u_long)tl_ipi_level, ipi);
}

void
ipi_all(u_int ipi)
{

	panic("%s", __func__);
}

void
ipi_all_but_self(u_int ipi)
{

	cpu_ipi_selected(PCPU_GET(other_cpus), 0, (u_long)tl_ipi_level, ipi);
}
