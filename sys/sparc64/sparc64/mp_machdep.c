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
 * $FreeBSD$
 */

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>

#include <dev/ofw/openfirm.h>

#include <ddb/ddb.h>

#include <machine/asi.h>
#include <machine/atomic.h>
#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/ofw_machdep.h>
#include <machine/smp.h>
#include <machine/tick.h>
#include <machine/tlb.h>
#include <machine/tte.h>

static ih_func_t cpu_ipi_ast;
static ih_func_t cpu_ipi_stop;

/*
 * Argument area used to pass data to non-boot processors as they start up.
 * This must be statically initialized with a known invalid upa module id,
 * since the other processors will use it before the boot cpu enters the
 * kernel.
 */
struct	cpu_start_args cpu_start_args = { 0, -1, -1, 0, 0 };
struct	ipi_cache_args ipi_cache_args;
struct	ipi_tlb_args ipi_tlb_args;

struct	mtx ipi_mtx;

vm_offset_t mp_tramp;

u_int	mp_boot_mid;

static volatile u_int	shutdown_cpus;

void cpu_mp_unleash(void *);
SYSINIT(cpu_mp_unleash, SI_SUB_SMP, SI_ORDER_FIRST, cpu_mp_unleash, NULL);

vm_offset_t
mp_tramp_alloc(void)
{
	struct tte *tp;
	char *v;
	int i;

	v = OF_claim(NULL, PAGE_SIZE, PAGE_SIZE);
	if (v == NULL)
		panic("mp_tramp_alloc");
	bcopy(mp_tramp_code, v, mp_tramp_code_len);
	*(u_long *)(v + mp_tramp_tlb_slots) = kernel_tlb_slots;
	*(u_long *)(v + mp_tramp_func) = (u_long)mp_startup;
	tp = (struct tte *)(v + mp_tramp_code_len);
	for (i = 0; i < kernel_tlb_slots; i++) {
		tp[i].tte_vpn = TV_VPN(kernel_tlbs[i].te_va, TS_4M);
		tp[i].tte_data = TD_V | TD_4M | TD_PA(kernel_tlbs[i].te_pa) |
		    TD_L | TD_CP | TD_CV | TD_P | TD_W;
	}
	for (i = 0; i < PAGE_SIZE; i += sizeof(long))
		flush(v + i);
	return (vm_offset_t)v;
}

/*
 * Probe for other cpus.
 */
int
cpu_mp_probe(void)
{
	phandle_t child;
	phandle_t root;
	char buf[128];
	int cpus;

	all_cpus = 1 << PCPU_GET(cpuid);
	mp_boot_mid = PCPU_GET(mid);
	mp_ncpus = 1;

	cpus = 0;
	root = OF_peer(0);
	for (child = OF_child(root); child != 0; child = OF_peer(child)) {
		if (OF_getprop(child, "device_type", buf, sizeof(buf)) > 0 &&
		    strcmp(buf, "cpu") == 0)
			cpus++;
	}
	mp_maxid = cpus;
	return (cpus > 1);
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
		0,
		0,
		0,
		0
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
		0,
		0,
	};

	openfirmware_exit(&args);
	panic("sun4u_stopself: failed.");
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
	int cpuid;
	u_int mid;
	u_long s;

	mtx_init(&ipi_mtx, "ipi", NULL, MTX_SPIN);

	intr_setup(PIL_AST, cpu_ipi_ast, -1, NULL, NULL);
	intr_setup(PIL_RENDEZVOUS, (ih_func_t *)smp_rendezvous_action,
	    -1, NULL, NULL);
	intr_setup(PIL_STOP, cpu_ipi_stop, -1, NULL, NULL);

	root = OF_peer(0);
	csa = &cpu_start_args;
	for (child = OF_child(root); child != 0; child = OF_peer(child)) {
		if (OF_getprop(child, "device_type", buf, sizeof(buf)) <= 0 ||
		    strcmp(buf, "cpu") != 0)
			continue;
		if (OF_getprop(child, "upa-portid", &mid, sizeof(mid)) <= 0 &&
		    OF_getprop(child, "portid", &mid, sizeof(mid)) <= 0)
			panic("cpu_mp_start: can't get module id");
		if (mid == mp_boot_mid)
			continue;
		if (OF_getprop(child, "clock-frequency", &clock,
		    sizeof(clock)) <= 0)
			panic("cpu_mp_start: can't get clock");

		csa->csa_state = 0;
		sun4u_startcpu(child, (void *)mp_tramp, 0);
		s = intr_disable();
		while (csa->csa_state != CPU_CLKSYNC)
			;
		membar(StoreLoad);
		csa->csa_tick = rd(tick);
		while (csa->csa_state != CPU_INIT)
			;
		csa->csa_tick = 0;
		intr_restore(s);

		cpuid = mp_ncpus++;
		cpu_identify(csa->csa_ver, clock, cpuid);

		va = kmem_alloc(kernel_map, PCPU_PAGES * PAGE_SIZE);
		pc = (struct pcpu *)(va + (PCPU_PAGES * PAGE_SIZE)) - 1;
		pcpu_init(pc, cpuid, sizeof(*pc));
		pc->pc_addr = va;
		pc->pc_mid = mid;
		pc->pc_node = child;

		all_cpus |= 1 << cpuid;
	}
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
	vm_offset_t pa;
	vm_offset_t va;
	u_int ctx_min;
	u_int ctx_inc;
	u_long s;
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

		if (pc->pc_cpuid == PCPU_GET(cpuid))
			continue;
		KASSERT(pc->pc_idlethread != NULL,
		    ("cpu_mp_unleash: idlethread"));
		KASSERT(pc->pc_curthread == pc->pc_idlethread,
		    ("cpu_mp_unleash: curthread"));
	
		pc->pc_curpcb = pc->pc_curthread->td_pcb;
		for (i = 0; i < PCPU_PAGES; i++) {
			va = pc->pc_addr + i * PAGE_SIZE;
			pa = pmap_kextract(va);
			if (pa == 0)
				panic("cpu_mp_unleash: pmap_kextract\n");
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
	u_long tag;
	int i;

	/*
	 * When secondary cpus start up they often have junk in their tlb.
	 * Sometimes both the lock bit and the valid bit will be set in the
	 * tlb entries, which can cause our locked mappings to be replaced,
	 * and other random behvaiour.  The tags always seems to be zero, so
	 * we flush all mappings with a tag of zero, regardless of the lock
	 * and/or valid bits.
	 */
	for (i = 0; i < tlb_dtlb_entries; i++) {
		tag = ldxa(TLB_DAR_SLOT(i), ASI_DTLB_TAG_READ_REG);
		if (tag == 0)
			stxa_sync(TLB_DAR_SLOT(i), ASI_DTLB_DATA_ACCESS_REG, 0);
		tag = ldxa(TLB_DAR_SLOT(i), ASI_ITLB_TAG_READ_REG);
		if (tag == 0)
			stxa_sync(TLB_DAR_SLOT(i), ASI_ITLB_DATA_ACCESS_REG, 0);
	}

	csa = &cpu_start_args;
	pmap_map_tsb();
	cpu_setregs(pc);
	tick_start_ap();

	smp_cpus++;
	PCPU_SET(other_cpus, all_cpus & ~(1 << PCPU_GET(cpuid)));
	printf("SMP: AP CPU #%d Launched!\n", PCPU_GET(cpuid));

	csa->csa_count--;
	membar(StoreLoad);
	csa->csa_state = CPU_BOOTSTRAP;
	while (csa->csa_count != 0)
		;

	binuptime(PCPU_PTR(switchtime));
	PCPU_SET(switchticks, ticks);

	/* ok, now grab sched_lock and enter the scheduler */
	mtx_lock_spin(&sched_lock);
	cpu_throw();	/* doesn't return */
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

	CTR1(KTR_SMP, "cpu_ipi_stop: stopped %d", PCPU_GET(cpuid));
	atomic_set_acq_int(&stopped_cpus, PCPU_GET(cpumask));
	while ((started_cpus & PCPU_GET(cpumask)) == 0) {
		if ((shutdown_cpus & PCPU_GET(cpumask)) != 0) {
			atomic_clear_int(&shutdown_cpus, PCPU_GET(cpumask));
			sun4u_stopself();
		}
	}
	atomic_clear_rel_int(&started_cpus, PCPU_GET(cpumask));
	atomic_clear_rel_int(&stopped_cpus, PCPU_GET(cpumask));
	CTR1(KTR_SMP, "cpu_ipi_stop: restarted %d", PCPU_GET(cpuid));
}

void
cpu_ipi_selected(u_int cpus, u_long d0, u_long d1, u_long d2)
{
	struct pcpu *pc;
	u_int cpu;

	while (cpus) {
		cpu = ffs(cpus) - 1;
		cpus &= ~(1 << cpu);
		pc = pcpu_find(cpu);
		cpu_ipi_send(pc->pc_mid, d0, d1, d2);
	}
}

void
cpu_ipi_send(u_int mid, u_long d0, u_long d1, u_long d2)
{
	u_long s;
	int i;

	KASSERT((ldxa(0, ASI_INTR_DISPATCH_STATUS) & IDR_BUSY) == 0,
	    ("ipi_send: outstanding dispatch"));
	for (i = 0; i < IPI_RETRIES; i++) {
		s = intr_disable();
		stxa(AA_SDB_INTR_D0, ASI_SDB_INTR_W, d0);
		stxa(AA_SDB_INTR_D1, ASI_SDB_INTR_W, d1);
		stxa(AA_SDB_INTR_D2, ASI_SDB_INTR_W, d2);
		stxa(AA_INTR_SEND | (mid << 14), ASI_SDB_INTR_W, 0);
		membar(Sync);
		while (ldxa(0, ASI_INTR_DISPATCH_STATUS) & IDR_BUSY)
			;
		intr_restore(s);
		if ((ldxa(0, ASI_INTR_DISPATCH_STATUS) & IDR_NACK) == 0)
			return;
	}
	if (
#ifdef DDB
	    db_active ||
#endif
	    panicstr != NULL)
		printf("ipi_send: couldn't send ipi to module %u\n", mid);
	else
		panic("ipi_send: couldn't send ipi");
}

void
ipi_selected(u_int cpus, u_int ipi)
{
	cpu_ipi_selected(cpus, 0, (u_long)tl_ipi_level, ipi);
}

void
ipi_all(u_int ipi)
{
	panic("ipi_all");
}

void
ipi_all_but_self(u_int ipi)
{
	cpu_ipi_selected(PCPU_GET(other_cpus), 0, (u_long)tl_ipi_level, ipi);
}
