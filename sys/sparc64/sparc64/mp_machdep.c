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

#include <machine/asi.h>
#include <machine/md_var.h>
#include <machine/smp.h>
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
struct	cpu_start_args cpu_start_args = { -1, -1, 0, 0 };

vm_offset_t mp_tramp;

static struct mtx ap_boot_mtx;

u_int	mp_boot_mid;

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
	for (i = 0; i < kernel_tlb_slots; i++)
		tp[i] = kernel_ttes[i];
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

	mtx_init(&ap_boot_mtx, "ap boot", MTX_SPIN);

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
		if (OF_getprop(child, "upa-portid", &mid, sizeof(mid)) <= 0)
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

		all_cpus |= 1 << cpuid;
	}
	PCPU_SET(other_cpus, all_cpus & ~(1 << PCPU_GET(cpuid)));
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

	ctx_min = 1;
	ctx_inc = (8192 - 1) / mp_ncpus;
	csa = &cpu_start_args;
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
			csa->csa_ttes[i].tte_vpn = TV_VPN(va);
			csa->csa_ttes[i].tte_data = TD_V | TD_8K | TD_PA(pa) |
			    TD_L | TD_CP | TD_CV | TD_P | TD_W;
		}
		csa->csa_state = 0;
		csa->csa_mid = pc->pc_mid;
		s = intr_disable();
		while (csa->csa_state != CPU_BOOTSTRAP)
			;
		intr_restore(s);
	}
}

void
cpu_mp_bootstrap(struct pcpu *pc)
{
	volatile struct cpu_start_args *csa;

	csa = &cpu_start_args;
	pmap_map_tsb();
	cpu_setregs(pc);

	smp_cpus++;
	PCPU_SET(other_cpus, all_cpus & ~(1 << PCPU_GET(cpuid)));
	printf("SMP: AP CPU #%d Launched!\n", PCPU_GET(cpuid));

	csa->csa_state = CPU_BOOTSTRAP;
	for (;;)
		;

	binuptime(PCPU_PTR(switchtime));
	PCPU_SET(switchticks, ticks);

	/* ok, now grab sched_lock and enter the scheduler */
	mtx_lock_spin(&sched_lock);
	cpu_throw();	/* doesn't return */
}

static void
cpu_ipi_ast(struct trapframe *tf)
{
}

static void
cpu_ipi_stop(struct trapframe *tf)
{
	TODO;
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
	u_long pstate;
	int i;

	KASSERT((ldxa(0, ASI_INTR_DISPATCH_STATUS) & IDR_BUSY) == 0,
	    ("ipi_send: outstanding dispatch"));
	pstate = rdpr(pstate);
	for (i = 0; i < IPI_RETRIES; i++) {
		if (pstate & PSTATE_IE)
			wrpr(pstate, pstate, PSTATE_IE);
		stxa(AA_SDB_INTR_D0, ASI_SDB_INTR_W, d0);
		stxa(AA_SDB_INTR_D1, ASI_SDB_INTR_W, d1);
		stxa(AA_SDB_INTR_D2, ASI_SDB_INTR_W, d2);
		stxa(AA_INTR_SEND | (mid << 14), ASI_SDB_INTR_W, 0);
		membar(Sync);
		while (ldxa(0, ASI_INTR_DISPATCH_STATUS) & IDR_BUSY)
			;
		wrpr(pstate, pstate, 0);
		if ((ldxa(0, ASI_INTR_DISPATCH_STATUS) & IDR_NACK) == 0)
			return;
	}
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
	TODO;
}

void
ipi_all_but_self(u_int ipi)
{
	cpu_ipi_selected(PCPU_GET(other_cpus), 0, (u_long)tl_ipi_level, ipi);
}
