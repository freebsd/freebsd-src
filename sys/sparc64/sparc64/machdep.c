/*-
 * Copyright (c) 2001 Jake Burkholder.
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
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
#include <sys/cons.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/ptrace.h>
#include <sys/signalvar.h>
#include <sys/sysproto.h>
#include <sys/timetc.h>
#include <sys/user.h>

#include <dev/ofw/openfirm.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_pager.h>
#include <vm/vm_extern.h>

#include <ddb/ddb.h>

#include <machine/bootinfo.h>
#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/pmap.h>
#include <machine/pstate.h>
#include <machine/reg.h>

typedef int ofw_vec_t(void *);

extern char tl0_base[];

extern char _end[];

int physmem = 0;
int cold = 1;
long dumplo;
int Maxmem = 0;

struct mtx Giant;
struct mtx sched_lock;

struct globaldata __globaldata;
/*
 * This needs not be aligned as the other user areas, provided that process 0
 * does not have an fp state (which it doesn't normally).
 * This constraint is only here for debugging.
 */
char user0[UPAGES * PAGE_SIZE] __attribute__ ((aligned (64)));
struct user *proc0paddr;

vm_offset_t clean_sva;
vm_offset_t clean_eva;

u_long ofw_vec;
u_long ofw_tba;

static vm_offset_t buffer_sva;
static vm_offset_t buffer_eva;
static vm_offset_t pager_sva;
static vm_offset_t pager_eva;

static struct timecounter tick_tc;

static timecounter_get_t tick_get_timecount;
void sparc64_init(struct bootinfo *bi, ofw_vec_t *vec);

static void cpu_startup(void *);
SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL);

static void
cpu_startup(void *arg)
{
	vm_offset_t physmem_est;
	vm_offset_t minaddr;
	vm_offset_t maxaddr;
	phandle_t child;
	phandle_t root;
	vm_offset_t va;
	vm_size_t size;
	char name[32];
	char type[8];
	u_int clock;
	int factor;
	caddr_t p;
	int i;

	root = OF_peer(0);
	for (child = OF_child(root); child != 0; child = OF_peer(child)) {
		OF_getprop(child, "device_type", type, sizeof(type));
		if (strcmp(type, "cpu") == 0)
			break;
	}
	if (child == 0)
		panic("cpu_startup: no cpu\n");
	OF_getprop(child, "name", name, sizeof(name));
	OF_getprop(child, "clock-frequency", &clock, sizeof(clock));

	tick_tc.tc_get_timecount = tick_get_timecount;
	tick_tc.tc_poll_pps = NULL;
	tick_tc.tc_counter_mask = ~0u;
	tick_tc.tc_frequency = clock;
	tick_tc.tc_name = "tick";
	tc_init(&tick_tc);

	p = name;
	if (bcmp(p, "SUNW,", 5) == 0)
		p += 5;
	printf("CPU: %s Processor (%d.%02d MHz CPU)\n", p,
	    (clock + 4999) / 1000000, ((clock + 4999) / 10000) % 100);
#if 0
	ver = rdpr(ver);
	printf("manuf: %#lx impl: %#lx mask: %#lx maxtl: %#lx maxwin: %#lx\n",
	    VER_MANUF(ver), VER_IMPL(ver), VER_MASK(ver), VER_MAXTL(ver),
	    VER_MAXWIN(ver));
#endif

	/*
	 * XXX make most of this MI and move to sys/kern.
	 */

	/*
	 * Calculate callout wheel size.
	 */
	for (callwheelsize = 1, callwheelbits = 0; callwheelsize < ncallout;
	    callwheelsize <<= 1, ++callwheelbits)
		;
	callwheelmask = callwheelsize - 1;

	size = 0;
	va = 0;
again:
	p = (caddr_t)va;

#define	valloc(name, type, num)						\
	(name) = (type *)p; p = (caddr_t)((name) + (num))

	valloc(callout, struct callout, ncallout);
	valloc(callwheel, struct callout_tailq, callwheelsize);

	if (kernel_map->first_free == NULL) {
		printf("Warning: no free entries in kernel_map.\n");
		physmem_est = physmem;
	} else
		physmem_est = min(physmem,
		    kernel_map->max_offset - kernel_map->min_offset);

	if (nbuf == 0) {
		factor = 4 * BKVASIZE / PAGE_SIZE;
		nbuf = 50;
		if (physmem_est > 1024)
			nbuf += min((physmem_est - 1024) / factor,
			    16384 / factor);
		if (physmem_est > 16384)
			nbuf += (physmem_est - 16384) * 2 / (factor * 5);
	}

	if (nbuf > (kernel_map->max_offset - kernel_map->min_offset) /
	    (BKVASIZE * 2)) {
		nbuf = (kernel_map->max_offset - kernel_map->min_offset) /
		    (BKVASIZE * 2);
		printf("Warning: nbufs capped at %d\n", nbuf);
	}

	nswbuf = max(min(nbuf/4, 256), 16);

	valloc(swbuf, struct buf, nswbuf);
	valloc(buf, struct buf, nbuf);
	p = bufhashinit(p);

	if (va == 0) {
		size = (vm_size_t)(p - va);
		if ((va = kmem_alloc(kernel_map, round_page(size))) == 0)
			panic("startup: no room for tables");
		goto again;
	}

	if ((vm_size_t)(p - va) != size)
		panic("startup: table size inconsistency");

	clean_map = kmem_suballoc(kernel_map, &clean_sva, &clean_eva,
	    (nbuf*BKVASIZE) + (nswbuf*MAXPHYS) + pager_map_size);
	buffer_map = kmem_suballoc(clean_map, &buffer_sva, &buffer_eva,
	    (nbuf*BKVASIZE));
	buffer_map->system_map = 1;
	pager_map = kmem_suballoc(clean_map, &pager_sva, &pager_eva,
	    (nswbuf*MAXPHYS) + pager_map_size);
	pager_map->system_map = 1;
	exec_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
	    (16*(ARG_MAX+(PAGE_SIZE*3))));

	SLIST_INIT(&callfree);
	for (i = 0; i < ncallout; i++) {
		callout_init(&callout[i], 0);
		callout[i].c_flags = CALLOUT_LOCAL_ALLOC;
		SLIST_INSERT_HEAD(&callfree, &callout[i], c_links.sle);
	}

	for (i = 0; i < callwheelsize; i++)
		TAILQ_INIT(&callwheel[i]);

	mtx_init(&callout_lock, "callout", MTX_SPIN | MTX_RECURSE);

	bufinit();
	vm_pager_bufferinit();

	globaldata_register(globaldata);

}

unsigned
tick_get_timecount(struct timecounter *tc)
{
	return ((unsigned)rd(tick));
}

void
sparc64_init(struct bootinfo *bi, ofw_vec_t *vec)
{
	struct trapframe *tf;

	/*
	 * Initialize openfirmware (needed for console).
	 */
	OF_init(vec);

	/*
	 * Initialize the console before printing anything.
	 */
	cninit();

	/*
	 * Check that the bootinfo struct is sane.
	 */
	if (bi->bi_version != BOOTINFO_VERSION)
		panic("sparc64_init: bootinfo version mismatch");
	if (bi->bi_metadata == 0)
		panic("sparc64_init: no loader metadata");
	preload_metadata = (caddr_t)bi->bi_metadata;

	init_param();

#ifdef DDB
	kdb_init();
#endif

	/*
	 * Initialize virtual memory.
	 */
	pmap_bootstrap(bi->bi_kpa, bi->bi_end);

	/*
	 * XXX Clear tick and disable the comparator.
	 */
	wrpr(tick, 0, 0);
	wr(asr23, 1L << 63, 0);

	/*
	 * Force trap level 1 and take over the trap table.
	 */
	wrpr(tl, 0, 1);
	wrpr(tba, tl0_base, 0);

	/*
	 * Initialize proc0 stuff (p_contested needs to be done early).
	 */
	LIST_INIT(&proc0.p_contested);
	proc0paddr = (struct user *)user0;
	proc0.p_addr = (struct user *)user0;
	tf = (struct trapframe *)(user0 + UPAGES * PAGE_SIZE - sizeof(*tf));
	proc0.p_frame = tf;
	tf->tf_tstate = 0;

	/*
	 * Initialize the per-cpu pointer so we can set curproc.
	 */
	globaldata = &__globaldata;

	/*
	 * Initialize curproc so that mutexes work.
	 */
	PCPU_SET(curproc, &proc0);
	PCPU_SET(curpcb, &((struct user *)user0)->u_pcb);
	PCPU_SET(spinlocks, NULL);

	/*
	 * Initialize mutexes.
	 */
	mtx_init(&sched_lock, "sched lock", MTX_SPIN | MTX_RECURSE);
	mtx_init(&Giant, "Giant", MTX_DEF | MTX_RECURSE);
	mtx_init(&proc0.p_mtx, "process lock", MTX_DEF);

	mtx_lock(&Giant);
}

void
set_openfirm_callback(ofw_vec_t *vec)
{
	ofw_tba = rdpr(tba);
	ofw_vec = (u_long)vec;
}

void
sendsig(sig_t catcher, int sig, sigset_t *mask, u_long code)
{
	TODO;
}

#ifndef	_SYS_SYSPROTO_H_
struct	sigreturn_args {
	ucontext_t *ucp;
};
#endif

int
sigreturn(struct proc *p, struct sigreturn_args *uap)
{
	TODO;
	return (0);
}

void
cpu_halt(void)
{
	TODO;
}

int
ptrace_set_pc(struct proc *p, u_long addr)
{
	TODO;
	return (0);
}

int
ptrace_single_step(struct proc *p)
{
	TODO;
	return (0);
}

void
setregs(struct proc *p, u_long entry, u_long stack, u_long ps_strings)
{
	struct pcb *pcb;

	pcb = &p->p_addr->u_pcb;
	mtx_lock_spin(&sched_lock);
	fp_init_pcb(pcb);
	/* XXX */
	p->p_frame->tf_tstate &= ~TSTATE_PEF;
	mtx_unlock_spin(&sched_lock);
	TODO;
}

void
Debugger(const char *msg)
{

	printf("Debugger(\"%s\")\n", msg);
	breakpoint();
}

int
fill_dbregs(struct proc *p, struct dbreg *dbregs)
{
	TODO;
	return (0);
}

int
set_dbregs(struct proc *p, struct dbreg *dbregs)
{
	TODO;
	return (0);
}

int
fill_regs(struct proc *p, struct reg *regs)
{
	TODO;
	return (0);
}

int
set_regs(struct proc *p, struct reg *regs)
{
	TODO;
	return (0);
}

int
fill_fpregs(struct proc *p, struct fpreg *fpregs)
{
	TODO;
	return (0);
}

int
set_fpregs(struct proc *p, struct fpreg *fpregs)
{
	TODO;
	return (0);
}
