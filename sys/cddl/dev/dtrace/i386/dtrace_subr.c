/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 *
 * $FreeBSD$
 *
 */
/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/kmem.h>
#include <sys/smp.h>
#include <sys/dtrace_impl.h>
#include <sys/dtrace_bsd.h>
#include <machine/clock.h>
#include <machine/frame.h>
#include <vm/pmap.h>

extern uintptr_t 	kernelbase;
extern uintptr_t 	dtrace_in_probe_addr;
extern int		dtrace_in_probe;

int dtrace_invop(uintptr_t, uintptr_t *, uintptr_t);

typedef struct dtrace_invop_hdlr {
	int (*dtih_func)(uintptr_t, uintptr_t *, uintptr_t);
	struct dtrace_invop_hdlr *dtih_next;
} dtrace_invop_hdlr_t;

dtrace_invop_hdlr_t *dtrace_invop_hdlr;

int
dtrace_invop(uintptr_t addr, uintptr_t *stack, uintptr_t eax)
{
	dtrace_invop_hdlr_t *hdlr;
	int rval;

	for (hdlr = dtrace_invop_hdlr; hdlr != NULL; hdlr = hdlr->dtih_next)
		if ((rval = hdlr->dtih_func(addr, stack, eax)) != 0)
			return (rval);

	return (0);
}

void
dtrace_invop_add(int (*func)(uintptr_t, uintptr_t *, uintptr_t))
{
	dtrace_invop_hdlr_t *hdlr;

	hdlr = kmem_alloc(sizeof (dtrace_invop_hdlr_t), KM_SLEEP);
	hdlr->dtih_func = func;
	hdlr->dtih_next = dtrace_invop_hdlr;
	dtrace_invop_hdlr = hdlr;
}

void
dtrace_invop_remove(int (*func)(uintptr_t, uintptr_t *, uintptr_t))
{
	dtrace_invop_hdlr_t *hdlr = dtrace_invop_hdlr, *prev = NULL;

	for (;;) {
		if (hdlr == NULL)
			panic("attempt to remove non-existent invop handler");

		if (hdlr->dtih_func == func)
			break;

		prev = hdlr;
		hdlr = hdlr->dtih_next;
	}

	if (prev == NULL) {
		ASSERT(dtrace_invop_hdlr == hdlr);
		dtrace_invop_hdlr = hdlr->dtih_next;
	} else {
		ASSERT(dtrace_invop_hdlr != hdlr);
		prev->dtih_next = hdlr->dtih_next;
	}

	kmem_free(hdlr, 0);
}

void
dtrace_toxic_ranges(void (*func)(uintptr_t base, uintptr_t limit))
{
	(*func)(0, kernelbase);
}

void
dtrace_xcall(processorid_t cpu, dtrace_xcall_t func, void *arg)
{
	cpumask_t cpus;

	if (cpu == DTRACE_CPUALL)
		cpus = all_cpus;
	else
		cpus = (cpumask_t)1 << cpu;

	smp_rendezvous_cpus(cpus, smp_no_rendevous_barrier, func,
	    smp_no_rendevous_barrier, arg);
}

static void
dtrace_sync_func(void)
{
}

void
dtrace_sync(void)
{
        dtrace_xcall(DTRACE_CPUALL, (dtrace_xcall_t)dtrace_sync_func, NULL);
}

#ifdef notyet
int (*dtrace_fasttrap_probe_ptr)(struct regs *);
int (*dtrace_pid_probe_ptr)(struct regs *);
int (*dtrace_return_probe_ptr)(struct regs *);

void
dtrace_user_probe(struct regs *rp, caddr_t addr, processorid_t cpuid)
{
	krwlock_t *rwp;
	proc_t *p = curproc;
	extern void trap(struct regs *, caddr_t, processorid_t);

	if (USERMODE(rp->r_cs) || (rp->r_ps & PS_VM)) {
		if (curthread->t_cred != p->p_cred) {
			cred_t *oldcred = curthread->t_cred;
			/*
			 * DTrace accesses t_cred in probe context.  t_cred
			 * must always be either NULL, or point to a valid,
			 * allocated cred structure.
			 */
			curthread->t_cred = crgetcred();
			crfree(oldcred);
		}
	}

	if (rp->r_trapno == T_DTRACE_RET) {
		uint8_t step = curthread->t_dtrace_step;
		uint8_t ret = curthread->t_dtrace_ret;
		uintptr_t npc = curthread->t_dtrace_npc;

		if (curthread->t_dtrace_ast) {
			aston(curthread);
			curthread->t_sig_check = 1;
		}

		/*
		 * Clear all user tracing flags.
		 */
		curthread->t_dtrace_ft = 0;

		/*
		 * If we weren't expecting to take a return probe trap, kill
		 * the process as though it had just executed an unassigned
		 * trap instruction.
		 */
		if (step == 0) {
			tsignal(curthread, SIGILL);
			return;
		}

		/*
		 * If we hit this trap unrelated to a return probe, we're
		 * just here to reset the AST flag since we deferred a signal
		 * until after we logically single-stepped the instruction we
		 * copied out.
		 */
		if (ret == 0) {
			rp->r_pc = npc;
			return;
		}

		/*
		 * We need to wait until after we've called the
		 * dtrace_return_probe_ptr function pointer to set %pc.
		 */
		rwp = &CPU->cpu_ft_lock;
		rw_enter(rwp, RW_READER);
		if (dtrace_return_probe_ptr != NULL)
			(void) (*dtrace_return_probe_ptr)(rp);
		rw_exit(rwp);
		rp->r_pc = npc;

	} else if (rp->r_trapno == T_DTRACE_PROBE) {
		rwp = &CPU->cpu_ft_lock;
		rw_enter(rwp, RW_READER);
		if (dtrace_fasttrap_probe_ptr != NULL)
			(void) (*dtrace_fasttrap_probe_ptr)(rp);
		rw_exit(rwp);

	} else if (rp->r_trapno == T_BPTFLT) {
		uint8_t instr;
		rwp = &CPU->cpu_ft_lock;

		/*
		 * The DTrace fasttrap provider uses the breakpoint trap
		 * (int 3). We let DTrace take the first crack at handling
		 * this trap; if it's not a probe that DTrace knowns about,
		 * we call into the trap() routine to handle it like a
		 * breakpoint placed by a conventional debugger.
		 */
		rw_enter(rwp, RW_READER);
		if (dtrace_pid_probe_ptr != NULL &&
		    (*dtrace_pid_probe_ptr)(rp) == 0) {
			rw_exit(rwp);
			return;
		}
		rw_exit(rwp);

		/*
		 * If the instruction that caused the breakpoint trap doesn't
		 * look like an int 3 anymore, it may be that this tracepoint
		 * was removed just after the user thread executed it. In
		 * that case, return to user land to retry the instuction.
		 */
		if (fuword8((void *)(rp->r_pc - 1), &instr) == 0 &&
		    instr != FASTTRAP_INSTR) {
			rp->r_pc--;
			return;
		}

		trap(rp, addr, cpuid);

	} else {
		trap(rp, addr, cpuid);
	}
}

void
dtrace_safe_synchronous_signal(void)
{
	kthread_t *t = curthread;
	struct regs *rp = lwptoregs(ttolwp(t));
	size_t isz = t->t_dtrace_npc - t->t_dtrace_pc;

	ASSERT(t->t_dtrace_on);

	/*
	 * If we're not in the range of scratch addresses, we're not actually
	 * tracing user instructions so turn off the flags. If the instruction
	 * we copied out caused a synchonous trap, reset the pc back to its
	 * original value and turn off the flags.
	 */
	if (rp->r_pc < t->t_dtrace_scrpc ||
	    rp->r_pc > t->t_dtrace_astpc + isz) {
		t->t_dtrace_ft = 0;
	} else if (rp->r_pc == t->t_dtrace_scrpc ||
	    rp->r_pc == t->t_dtrace_astpc) {
		rp->r_pc = t->t_dtrace_pc;
		t->t_dtrace_ft = 0;
	}
}

int
dtrace_safe_defer_signal(void)
{
	kthread_t *t = curthread;
	struct regs *rp = lwptoregs(ttolwp(t));
	size_t isz = t->t_dtrace_npc - t->t_dtrace_pc;

	ASSERT(t->t_dtrace_on);

	/*
	 * If we're not in the range of scratch addresses, we're not actually
	 * tracing user instructions so turn off the flags.
	 */
	if (rp->r_pc < t->t_dtrace_scrpc ||
	    rp->r_pc > t->t_dtrace_astpc + isz) {
		t->t_dtrace_ft = 0;
		return (0);
	}

	/*
	 * If we've executed the original instruction, but haven't performed
	 * the jmp back to t->t_dtrace_npc or the clean up of any registers
	 * used to emulate %rip-relative instructions in 64-bit mode, do that
	 * here and take the signal right away. We detect this condition by
	 * seeing if the program counter is the range [scrpc + isz, astpc).
	 */
	if (t->t_dtrace_astpc - rp->r_pc <
	    t->t_dtrace_astpc - t->t_dtrace_scrpc - isz) {
#ifdef __amd64
		/*
		 * If there is a scratch register and we're on the
		 * instruction immediately after the modified instruction,
		 * restore the value of that scratch register.
		 */
		if (t->t_dtrace_reg != 0 &&
		    rp->r_pc == t->t_dtrace_scrpc + isz) {
			switch (t->t_dtrace_reg) {
			case REG_RAX:
				rp->r_rax = t->t_dtrace_regv;
				break;
			case REG_RCX:
				rp->r_rcx = t->t_dtrace_regv;
				break;
			case REG_R8:
				rp->r_r8 = t->t_dtrace_regv;
				break;
			case REG_R9:
				rp->r_r9 = t->t_dtrace_regv;
				break;
			}
		}
#endif
		rp->r_pc = t->t_dtrace_npc;
		t->t_dtrace_ft = 0;
		return (0);
	}

	/*
	 * Otherwise, make sure we'll return to the kernel after executing
	 * the copied out instruction and defer the signal.
	 */
	if (!t->t_dtrace_step) {
		ASSERT(rp->r_pc < t->t_dtrace_astpc);
		rp->r_pc += t->t_dtrace_astpc - t->t_dtrace_scrpc;
		t->t_dtrace_step = 1;
	}

	t->t_dtrace_ast = 1;

	return (1);
}
#endif

static int64_t	tgt_cpu_tsc;
static int64_t	hst_cpu_tsc;
static int64_t	tsc_skew[MAXCPU];
static uint64_t	nsec_scale;

/* See below for the explanation of this macro. */
#define SCALE_SHIFT	28

static void
dtrace_gethrtime_init_cpu(void *arg)
{
	uintptr_t cpu = (uintptr_t) arg;

	if (cpu == curcpu)
		tgt_cpu_tsc = rdtsc();
	else
		hst_cpu_tsc = rdtsc();
}

static void
dtrace_gethrtime_init(void *arg)
{
	uint64_t tsc_f;
	cpumask_t map;
	int i;

	/*
	 * Get TSC frequency known at this moment.
	 * This should be constant if TSC is invariant.
	 * Otherwise tick->time conversion will be inaccurate, but
	 * will preserve monotonic property of TSC.
	 */
	tsc_f = tsc_freq;

	/*
	 * The following line checks that nsec_scale calculated below
	 * doesn't overflow 32-bit unsigned integer, so that it can multiply
	 * another 32-bit integer without overflowing 64-bit.
	 * Thus minimum supported TSC frequency is 62.5MHz.
	 */
	KASSERT(tsc_f > (NANOSEC >> (32 - SCALE_SHIFT)), ("TSC frequency is too low"));

	/*
	 * We scale up NANOSEC/tsc_f ratio to preserve as much precision
	 * as possible.
	 * 2^28 factor was chosen quite arbitrarily from practical
	 * considerations:
	 * - it supports TSC frequencies as low as 62.5MHz (see above);
	 * - it provides quite good precision (e < 0.01%) up to THz
	 *   (terahertz) values;
	 */
	nsec_scale = ((uint64_t)NANOSEC << SCALE_SHIFT) / tsc_f;

	/* The current CPU is the reference one. */
	tsc_skew[curcpu] = 0;

	for (i = 0; i <= mp_maxid; i++) {
		if (i == curcpu)
			continue;

		if (pcpu_find(i) == NULL)
			continue;

		map = 0;
		map |= (1 << curcpu);
		map |= (1 << i);

		smp_rendezvous_cpus(map, NULL,
		    dtrace_gethrtime_init_cpu,
		    smp_no_rendevous_barrier, (void *)(uintptr_t) i);

		tsc_skew[i] = tgt_cpu_tsc - hst_cpu_tsc;
	}
}

SYSINIT(dtrace_gethrtime_init, SI_SUB_SMP, SI_ORDER_ANY, dtrace_gethrtime_init, NULL);

/*
 * DTrace needs a high resolution time function which can
 * be called from a probe context and guaranteed not to have
 * instrumented with probes itself.
 *
 * Returns nanoseconds since boot.
 */
uint64_t
dtrace_gethrtime()
{
	uint64_t tsc;
	uint32_t lo;
	uint32_t hi;

	/*
	 * We split TSC value into lower and higher 32-bit halves and separately
	 * scale them with nsec_scale, then we scale them down by 2^28
	 * (see nsec_scale calculations) taking into account 32-bit shift of
	 * the higher half and finally add.
	 */
	tsc = rdtsc() + tsc_skew[curcpu];
	lo = tsc;
	hi = tsc >> 32;
	return (((lo * nsec_scale) >> SCALE_SHIFT) +
	    ((hi * nsec_scale) << (32 - SCALE_SHIFT)));
}

uint64_t
dtrace_gethrestime(void)
{
	printf("%s(%d): XXX\n",__func__,__LINE__);
	return (0);
}

/* Function to handle DTrace traps during probes. See i386/i386/trap.c */
int
dtrace_trap(struct trapframe *frame, u_int type)
{
	/*
	 * A trap can occur while DTrace executes a probe. Before
	 * executing the probe, DTrace blocks re-scheduling and sets
	 * a flag in it's per-cpu flags to indicate that it doesn't
	 * want to fault. On returning from the probe, the no-fault
	 * flag is cleared and finally re-scheduling is enabled.
	 *
	 * Check if DTrace has enabled 'no-fault' mode:
	 *
	 */
	if ((cpu_core[curcpu].cpuc_dtrace_flags & CPU_DTRACE_NOFAULT) != 0) {
		/*
		 * There are only a couple of trap types that are expected.
		 * All the rest will be handled in the usual way.
		 */
		switch (type) {
		/* General protection fault. */
		case T_PROTFLT:
			/* Flag an illegal operation. */
			cpu_core[curcpu].cpuc_dtrace_flags |= CPU_DTRACE_ILLOP;

			/*
			 * Offset the instruction pointer to the instruction
			 * following the one causing the fault.
			 */
			frame->tf_eip += dtrace_instr_size((u_char *) frame->tf_eip);
			return (1);
		/* Page fault. */
		case T_PAGEFLT:
			/* Flag a bad address. */
			cpu_core[curcpu].cpuc_dtrace_flags |= CPU_DTRACE_BADADDR;
			cpu_core[curcpu].cpuc_dtrace_illval = rcr2();

			/*
			 * Offset the instruction pointer to the instruction
			 * following the one causing the fault.
			 */
			frame->tf_eip += dtrace_instr_size((u_char *) frame->tf_eip);
			return (1);
		default:
			/* Handle all other traps in the usual way. */
			break;
		}
	}

	/* Handle the trap in the usual way. */
	return (0);
}
