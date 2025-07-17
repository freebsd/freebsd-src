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
 */
/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/dtrace_impl.h>
#include <sys/dtrace_bsd.h>
#include <cddl/dev/dtrace/dtrace_cddl.h>
#include <machine/armreg.h>
#include <machine/clock.h>
#include <machine/frame.h>
#include <machine/trap.h>
#include <machine/vmparam.h>
#include <vm/pmap.h>

extern dtrace_id_t	dtrace_probeid_error;
extern int (*dtrace_invop_jump_addr)(struct trapframe *);
extern void dtrace_getnanotime(struct timespec *tsp);
extern void dtrace_getnanouptime(struct timespec *tsp);

int dtrace_invop(uintptr_t, struct trapframe *, uintptr_t);
void dtrace_invop_init(void);
void dtrace_invop_uninit(void);

typedef struct dtrace_invop_hdlr {
	int (*dtih_func)(uintptr_t, struct trapframe *, uintptr_t);
	struct dtrace_invop_hdlr *dtih_next;
} dtrace_invop_hdlr_t;

dtrace_invop_hdlr_t *dtrace_invop_hdlr;

int
dtrace_invop(uintptr_t addr, struct trapframe *frame, uintptr_t eax)
{
	struct thread *td;
	dtrace_invop_hdlr_t *hdlr;
	int rval;

	rval = 0;
	td = curthread;
	td->t_dtrace_trapframe = frame;
	for (hdlr = dtrace_invop_hdlr; hdlr != NULL; hdlr = hdlr->dtih_next)
		if ((rval = hdlr->dtih_func(addr, frame, eax)) != 0)
			break;
	td->t_dtrace_trapframe = NULL;
	return (rval);
}

void
dtrace_invop_add(int (*func)(uintptr_t, struct trapframe *, uintptr_t))
{
	dtrace_invop_hdlr_t *hdlr;

	hdlr = kmem_alloc(sizeof (dtrace_invop_hdlr_t), KM_SLEEP);
	hdlr->dtih_func = func;
	hdlr->dtih_next = dtrace_invop_hdlr;
	dtrace_invop_hdlr = hdlr;
}

void
dtrace_invop_remove(int (*func)(uintptr_t, struct trapframe *, uintptr_t))
{
	dtrace_invop_hdlr_t *hdlr, *prev;

	hdlr = dtrace_invop_hdlr;
	prev = NULL;

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

/*ARGSUSED*/
void
dtrace_toxic_ranges(void (*func)(uintptr_t base, uintptr_t limit))
{

	(*func)(0, (uintptr_t)VM_MIN_KERNEL_ADDRESS);
}

void
dtrace_xcall(processorid_t cpu, dtrace_xcall_t func, void *arg)
{
	cpuset_t cpus;

	if (cpu == DTRACE_CPUALL)
		cpus = all_cpus;
	else
		CPU_SETOF(cpu, &cpus);

	smp_rendezvous_cpus(cpus, smp_no_rendezvous_barrier, func,
	    smp_no_rendezvous_barrier, arg);
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

static uint64_t nsec_scale;

#define SCALE_SHIFT	25

/*
 * Choose scaling factors which let us convert a cntvct_el0 value to nanoseconds
 * without overflow, as in the amd64 implementation.
 *
 * Documentation for the ARM generic timer states that typical counter
 * frequencies are in the range 1Mhz-50Mhz; in ARMv9 the frequency is fixed at
 * 1GHz.  The lower bound of 1MHz forces the shift to be at most 25 bits.  At
 * that frequency, the calculation (hi * scale) << (32 - shift) will not
 * overflow for over 100 years, assuming that the counter value starts at 0 upon
 * boot.
 */
static void
dtrace_gethrtime_init(void *arg __unused)
{
	uint64_t freq;

	freq = READ_SPECIALREG(cntfrq_el0);
	nsec_scale = ((uint64_t)NANOSEC << SCALE_SHIFT) / freq;
}
SYSINIT(dtrace_gethrtime_init, SI_SUB_DTRACE, SI_ORDER_ANY,
    dtrace_gethrtime_init, NULL);

/*
 * DTrace needs a high resolution time function which can be called from a
 * probe context and guaranteed not to have instrumented with probes itself.
 *
 * Returns nanoseconds since some arbitrary point in time (likely SoC reset?).
 */
uint64_t
dtrace_gethrtime(void)
{
	uint64_t count, freq;
	uint32_t lo, hi;

	count = READ_SPECIALREG(cntvct_el0);
	lo = count;
	hi = count >> 32;
	return (((lo * nsec_scale) >> SCALE_SHIFT) +
	    ((hi * nsec_scale) << (32 - SCALE_SHIFT)));
}

/*
 * Return a much lower resolution wallclock time based on the system clock
 * updated by the timer.  If needed, we could add a version interpolated from
 * the system clock as is the case with dtrace_gethrtime().
 */
uint64_t
dtrace_gethrestime(void)
{
	struct timespec current_time;

	dtrace_getnanotime(&current_time);

	return (current_time.tv_sec * 1000000000UL + current_time.tv_nsec);
}

/* Function to handle DTrace traps during probes. See arm64/arm64/trap.c */
int
dtrace_trap(struct trapframe *frame, u_int type)
{
	/*
	 * A trap can occur while DTrace executes a probe. Before
	 * executing the probe, DTrace blocks re-scheduling and sets
	 * a flag in its per-cpu flags to indicate that it doesn't
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
		case EXCP_DATA_ABORT:
			/* Flag a bad address. */
			cpu_core[curcpu].cpuc_dtrace_flags |= CPU_DTRACE_BADADDR;
			cpu_core[curcpu].cpuc_dtrace_illval = frame->tf_far;

			/*
			 * Offset the instruction pointer to the instruction
			 * following the one causing the fault.
			 */
			frame->tf_elr += 4;
			return (1);
		default:
			/* Handle all other traps in the usual way. */
			break;
		}
	}

	/* Handle the trap in the usual way. */
	return (0);
}

void
dtrace_probe_error(dtrace_state_t *state, dtrace_epid_t epid, int which,
    int fault, int fltoffs, uintptr_t illval)
{

	dtrace_probe(dtrace_probeid_error, (uint64_t)(uintptr_t)state,
	    (uintptr_t)epid,
	    (uintptr_t)which, (uintptr_t)fault, (uintptr_t)fltoffs);
}

static void
dtrace_load64(uint64_t *addr, struct trapframe *frame, u_int reg)
{

	KASSERT(reg <= 31, ("dtrace_load64: Invalid register %u", reg));
	if (reg < nitems(frame->tf_x))
		frame->tf_x[reg] = *addr;
	else if (reg == 30) /* lr */
		frame->tf_lr = *addr;
	/* Nothing to do for load to xzr */
}

static void
dtrace_store64(uint64_t *addr, struct trapframe *frame, u_int reg)
{

	KASSERT(reg <= 31, ("dtrace_store64: Invalid register %u", reg));
	if (reg < nitems(frame->tf_x))
		*addr = frame->tf_x[reg];
	else if (reg == 30) /* lr */
		*addr = frame->tf_lr;
	else if (reg == 31) /* xzr */
		*addr = 0;
}

static int
dtrace_invop_start(struct trapframe *frame)
{
	int data, invop, tmp;

	invop = dtrace_invop(frame->tf_elr, frame, frame->tf_x[0]);

	tmp = (invop & LDP_STP_MASK);
	if (tmp == STP_64 || tmp == LDP_64) {
		register_t arg1, arg2, *sp;
		int offs;

		sp = (register_t *)frame->tf_sp;
		data = invop;
		arg1 = (data >> ARG1_SHIFT) & ARG1_MASK;
		arg2 = (data >> ARG2_SHIFT) & ARG2_MASK;

		offs = (data >> OFFSET_SHIFT) & OFFSET_MASK;

		switch (tmp) {
		case STP_64:
			if (offs >> (OFFSET_SIZE - 1))
				sp -= (~offs & OFFSET_MASK) + 1;
			else
				sp += (offs);
			dtrace_store64(sp + 0, frame, arg1);
			dtrace_store64(sp + 1, frame, arg2);
			break;
		case LDP_64:
			dtrace_load64(sp + 0, frame, arg1);
			dtrace_load64(sp + 1, frame, arg2);
			if (offs >> (OFFSET_SIZE - 1))
				sp -= (~offs & OFFSET_MASK) + 1;
			else
				sp += (offs);
			break;
		default:
			break;
		}

		/* Update the stack pointer and program counter to continue */
		frame->tf_sp = (register_t)sp;
		frame->tf_elr += INSN_SIZE;
		return (0);
	}

	if ((invop & SUB_MASK) == SUB_INSTR) {
		frame->tf_sp -= (invop >> SUB_IMM_SHIFT) & SUB_IMM_MASK;
		frame->tf_elr += INSN_SIZE;
		return (0);
	}

	if (invop == NOP_INSTR) {
		frame->tf_elr += INSN_SIZE;
		return (0);
	}

	if ((invop & B_MASK) == B_INSTR) {
		data = (invop & B_DATA_MASK);
		/* The data is the number of 4-byte words to change the pc */
		data *= 4;
		frame->tf_elr += data;
		return (0);
	}

	if (invop == RET_INSTR) {
		frame->tf_elr = frame->tf_lr;
		return (0);
	}

	return (-1);
}

void
dtrace_invop_init(void)
{

	dtrace_invop_jump_addr = dtrace_invop_start;
}

void
dtrace_invop_uninit(void)
{

	dtrace_invop_jump_addr = 0;
}
