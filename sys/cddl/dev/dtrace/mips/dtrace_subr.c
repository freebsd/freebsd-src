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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
#include <machine/trap.h>
#include <vm/pmap.h>

#define	DELAYBRANCH(x)	((int)(x) < 0)
		
extern uintptr_t 	dtrace_in_probe_addr;
extern int		dtrace_in_probe;
extern dtrace_id_t	dtrace_probeid_error;

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


/*ARGSUSED*/
void
dtrace_toxic_ranges(void (*func)(uintptr_t base, uintptr_t limit))
{
	/*
	 * No toxic regions?
	 */
}

void
dtrace_xcall(processorid_t cpu, dtrace_xcall_t func, void *arg)
{
	cpuset_t cpus;

	if (cpu == DTRACE_CPUALL)
		cpus = all_cpus;
	else
		CPU_SETOF(cpu, &cpus);

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
	struct      timespec curtime;

	nanouptime(&curtime);

	return (curtime.tv_sec * 1000000000UL + curtime.tv_nsec);

}

uint64_t
dtrace_gethrestime(void)
{
	struct      timespec curtime;

	getnanotime(&curtime);

	return (curtime.tv_sec * 1000000000UL + curtime.tv_nsec);
}

/* Function to handle DTrace traps during probes. See amd64/amd64/trap.c */
int
dtrace_trap(struct trapframe *frame)
{
	u_int type;

	type = (trapframe->cause & MIPS_CR_EXC_CODE) >> MIPS_CR_EXC_CODE_SHIFT;

	/*
	 * A trap can occur while DTrace executes a probe. Before
	 * executing the probe, DTrace blocks re-scheduling and sets
	 * a flag in its per-cpu flags to indicate that it doesn't
	 * want to fault. On returning from the probe, the no-fault
	 * flag is cleared and finally re-scheduling is enabled.
	 *
	 * Check if DTrace has enabled 'no-fault' mode:
	 */
	if ((cpu_core[curcpu].cpuc_dtrace_flags & CPU_DTRACE_NOFAULT) != 0) {
		/*
		 * There are only a couple of trap types that are expected.
		 * All the rest will be handled in the usual way.
		 */
		switch (type) {
		/* Page fault. */
		case T_TLB_ST_MISS:
		case T_ADDR_ERR_ST:
		case T_TLB_LD_MISS:
		case T_ADDR_ERR_LD:
		case T_BUS_ERR_IFETCH:
			/* Flag a bad address. */
			cpu_core[curcpu].cpuc_dtrace_flags |= CPU_DTRACE_BADADDR;
			cpu_core[curcpu].cpuc_dtrace_illval = frame->badvaddr;

			/*
			 * Offset the instruction pointer to the instruction
			 * following the one causing the fault.
			 */
			if (DELAYBRANCH(frame->cause))	 /* Check BD bit */
			{
				/* XXX: check MipsEmulateBranch on MIPS64
				frame->pc = MipsEmulateBranch(frame, frame->pc,
				    0, 0);
 				*/
				panic("%s: delay slot at %jx, badvaddr = %jx\n",
				    __func__,
				    (intmax_t)frame->pc, (intmax_t)frame->badvaddr);
			}
			else
				frame->pc += sizeof(int);
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
