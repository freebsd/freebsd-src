/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
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
 */

/*
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/fasttrap_isa.h>
#include <sys/fasttrap_impl.h>
#include <sys/dtrace.h>
#include <sys/dtrace_impl.h>
#include <sys/cmn_err.h>
#include <sys/frame.h>
#include <sys/stack.h>
#include <sys/sysmacros.h>
#include <sys/trap.h>

#include <v9/sys/machpcb.h>
#include <v9/sys/privregs.h>

/*
 * Lossless User-Land Tracing on SPARC
 * -----------------------------------
 *
 * The Basic Idea
 *
 * The most important design constraint is, of course, correct execution of
 * the user thread above all else. The next most important goal is rapid
 * execution. We combine execution of instructions in user-land with
 * emulation of certain instructions in the kernel to aim for complete
 * correctness and maximal performance.
 *
 * We take advantage of the split PC/NPC architecture to speed up logical
 * single-stepping; when we copy an instruction out to the scratch space in
 * the ulwp_t structure (held in the %g7 register on SPARC), we can
 * effectively single step by setting the PC to our scratch space and leaving
 * the NPC alone. This executes the replaced instruction and then continues
 * on without having to reenter the kernel as with single- stepping. The
 * obvious caveat is for instructions whose execution is PC dependant --
 * branches, call and link instructions (call and jmpl), and the rdpc
 * instruction. These instructions cannot be executed in the manner described
 * so they must be emulated in the kernel.
 *
 * Emulation for this small set of instructions if fairly simple; the most
 * difficult part being emulating branch conditions.
 *
 *
 * A Cache Heavy Portfolio
 *
 * It's important to note at this time that copying an instruction out to the
 * ulwp_t scratch space in user-land is rather complicated. SPARC has
 * separate data and instruction caches so any writes to the D$ (using a
 * store instruction for example) aren't necessarily reflected in the I$.
 * The flush instruction can be used to synchronize the two and must be used
 * for any self-modifying code, but the flush instruction only applies to the
 * primary address space (the absence of a flusha analogue to the flush
 * instruction that accepts an ASI argument is an obvious omission from SPARC
 * v9 where the notion of the alternate address space was introduced on
 * SPARC). To correctly copy out the instruction we must use a block store
 * that doesn't allocate in the D$ and ensures synchronization with the I$;
 * see dtrace_blksuword32() for the implementation  (this function uses
 * ASI_BLK_COMMIT_S to write a block through the secondary ASI in the manner
 * described). Refer to the UltraSPARC I/II manual for details on the
 * ASI_BLK_COMMIT_S ASI.
 *
 *
 * Return Subtleties
 *
 * When we're firing a return probe we need to expose the value returned by
 * the function being traced. Since the function can set the return value
 * in its last instruction, we need to fire the return probe only _after_
 * the effects of the instruction are apparent. For instructions that we
 * emulate, we can call dtrace_probe() after we've performed the emulation;
 * for instructions that we execute after we return to user-land, we set
 * %pc to the instruction we copied out (as described above) and set %npc
 * to a trap instruction stashed in the ulwp_t structure. After the traced
 * instruction is executed, the trap instruction returns control to the
 * kernel where we can fire the return probe.
 *
 * This need for a second trap in cases where we execute the traced
 * instruction makes it all the more important to emulate the most common
 * instructions to avoid the second trip in and out of the kernel.
 *
 *
 * Making it Fast
 *
 * Since copying out an instruction is neither simple nor inexpensive for the
 * CPU, we should attempt to avoid doing it in as many cases as possible.
 * Since function entry and return are usually the most interesting probe
 * sites, we attempt to tune the performance of the fasttrap provider around
 * instructions typically in those places.
 *
 * Looking at a bunch of functions in libraries and executables reveals that
 * most functions begin with either a save or a sethi (to setup a larger
 * argument to the save) and end with a restore or an or (in the case of leaf
 * functions). To try to improve performance, we emulate all of these
 * instructions in the kernel.
 *
 * The save and restore instructions are a little tricky since they perform
 * register window maniplulation. Rather than trying to tinker with the
 * register windows from the kernel, we emulate the implicit add that takes
 * place as part of those instructions and set the %pc to point to a simple
 * save or restore we've hidden in the ulwp_t structure. If we're in a return
 * probe so want to make it seem as though the tracepoint has been completely
 * executed we need to remember that we've pulled this trick with restore and
 * pull registers from the previous window (the one that we'll switch to once
 * the simple store instruction is executed) rather than the current one. This
 * is why in the case of emulating a restore we set the DTrace CPU flag
 * CPU_DTRACE_FAKERESTORE before calling dtrace_probe() for the return probes
 * (see fasttrap_return_common()).
 */

#define	OP(x)		((x) >> 30)
#define	OP2(x)		(((x) >> 22) & 0x07)
#define	OP3(x)		(((x) >> 19) & 0x3f)
#define	RCOND(x)	(((x) >> 25) & 0x07)
#define	COND(x)		(((x) >> 25) & 0x0f)
#define	A(x)		(((x) >> 29) & 0x01)
#define	I(x)		(((x) >> 13) & 0x01)
#define	RD(x)		(((x) >> 25) & 0x1f)
#define	RS1(x)		(((x) >> 14) & 0x1f)
#define	RS2(x)		(((x) >> 0) & 0x1f)
#define	CC(x)		(((x) >> 20) & 0x03)
#define	DISP16(x)	((((x) >> 6) & 0xc000) | ((x) & 0x3fff))
#define	DISP22(x)	((x) & 0x3fffff)
#define	DISP19(x)	((x) & 0x7ffff)
#define	DISP30(x)	((x) & 0x3fffffff)
#define	SW_TRAP(x)	((x) & 0x7f)

#define	OP3_OR		0x02
#define	OP3_RD		0x28
#define	OP3_JMPL	0x38
#define	OP3_RETURN	0x39
#define	OP3_TCC		0x3a
#define	OP3_SAVE	0x3c
#define	OP3_RESTORE	0x3d

#define	OP3_PREFETCH	0x2d
#define	OP3_CASA	0x3c
#define	OP3_PREFETCHA	0x3d
#define	OP3_CASXA	0x3e

#define	OP2_ILLTRAP	0x0
#define	OP2_BPcc	0x1
#define	OP2_Bicc	0x2
#define	OP2_BPr		0x3
#define	OP2_SETHI	0x4
#define	OP2_FBPfcc	0x5
#define	OP2_FBfcc	0x6

#define	R_G0		0
#define	R_O0		8
#define	R_SP		14
#define	R_I0		24
#define	R_I1		25
#define	R_I2		26
#define	R_I3		27
#define	R_I4		28

/*
 * Check the comment in fasttrap.h when changing these offsets or adding
 * new instructions.
 */
#define	FASTTRAP_OFF_SAVE	64
#define	FASTTRAP_OFF_RESTORE	68
#define	FASTTRAP_OFF_FTRET	72
#define	FASTTRAP_OFF_RETURN	76

#define	BREAKPOINT_INSTR	0x91d02001	/* ta 1 */

/*
 * Tunable to let users turn off the fancy save instruction optimization.
 * If a program is non-ABI compliant, there's a possibility that the save
 * instruction optimization could cause an error.
 */
int fasttrap_optimize_save = 1;

static uint64_t
fasttrap_anarg(struct regs *rp, int argno)
{
	uint64_t value;

	if (argno < 6)
		return ((&rp->r_o0)[argno]);

	if (curproc->p_model == DATAMODEL_NATIVE) {
		struct frame *fr = (struct frame *)(rp->r_sp + STACK_BIAS);

		DTRACE_CPUFLAG_SET(CPU_DTRACE_NOFAULT);
		value = dtrace_fulword(&fr->fr_argd[argno]);
		DTRACE_CPUFLAG_CLEAR(CPU_DTRACE_NOFAULT | CPU_DTRACE_BADADDR |
		    CPU_DTRACE_BADALIGN);
	} else {
		struct frame32 *fr = (struct frame32 *)rp->r_sp;

		DTRACE_CPUFLAG_SET(CPU_DTRACE_NOFAULT);
		value = dtrace_fuword32(&fr->fr_argd[argno]);
		DTRACE_CPUFLAG_CLEAR(CPU_DTRACE_NOFAULT | CPU_DTRACE_BADADDR |
		    CPU_DTRACE_BADALIGN);
	}

	return (value);
}

static ulong_t fasttrap_getreg(struct regs *, uint_t);
static void fasttrap_putreg(struct regs *, uint_t, ulong_t);

static void
fasttrap_usdt_args(fasttrap_probe_t *probe, struct regs *rp,
    uint_t fake_restore, int argc, uintptr_t *argv)
{
	int i, x, cap = MIN(argc, probe->ftp_nargs);
	int inc = (fake_restore ? 16 : 0);

	/*
	 * The only way we'll hit the fake_restore case is if a USDT probe is
	 * invoked as a tail-call. While it wouldn't be incorrect, we can
	 * avoid a call to fasttrap_getreg(), and safely use rp->r_sp
	 * directly since a tail-call can't be made if the invoked function
	 * would use the argument dump space (i.e. if there were more than
	 * 6 arguments). We take this shortcut because unconditionally rooting
	 * around for R_FP (R_SP + 16) would be unnecessarily painful.
	 */

	if (curproc->p_model == DATAMODEL_NATIVE) {
		struct frame *fr = (struct frame *)(rp->r_sp + STACK_BIAS);
		uintptr_t v;

		for (i = 0; i < cap; i++) {
			x = probe->ftp_argmap[i];

			if (x < 6)
				argv[i] = fasttrap_getreg(rp, R_O0 + x + inc);
			else if (fasttrap_fulword(&fr->fr_argd[x], &v) != 0)
				argv[i] = 0;
		}

	} else {
		struct frame32 *fr = (struct frame32 *)rp->r_sp;
		uint32_t v;

		for (i = 0; i < cap; i++) {
			x = probe->ftp_argmap[i];

			if (x < 6)
				argv[i] = fasttrap_getreg(rp, R_O0 + x + inc);
			else if (fasttrap_fuword32(&fr->fr_argd[x], &v) != 0)
				argv[i] = 0;
		}
	}

	for (; i < argc; i++) {
		argv[i] = 0;
	}
}

static void
fasttrap_return_common(struct regs *rp, uintptr_t pc, pid_t pid,
    uint_t fake_restore)
{
	fasttrap_tracepoint_t *tp;
	fasttrap_bucket_t *bucket;
	fasttrap_id_t *id;
	kmutex_t *pid_mtx;
	dtrace_icookie_t cookie;

	pid_mtx = &cpu_core[CPU->cpu_id].cpuc_pid_lock;
	mutex_enter(pid_mtx);
	bucket = &fasttrap_tpoints.fth_table[FASTTRAP_TPOINTS_INDEX(pid, pc)];

	for (tp = bucket->ftb_data; tp != NULL; tp = tp->ftt_next) {
		if (pid == tp->ftt_pid && pc == tp->ftt_pc &&
		    tp->ftt_proc->ftpc_acount != 0)
			break;
	}

	/*
	 * Don't sweat it if we can't find the tracepoint again; unlike
	 * when we're in fasttrap_pid_probe(), finding the tracepoint here
	 * is not essential to the correct execution of the process.
	 */
	if (tp == NULL || tp->ftt_retids == NULL) {
		mutex_exit(pid_mtx);
		return;
	}

	for (id = tp->ftt_retids; id != NULL; id = id->fti_next) {
		fasttrap_probe_t *probe = id->fti_probe;

		if (id->fti_ptype == DTFTP_POST_OFFSETS) {
			if (probe->ftp_argmap != NULL && fake_restore) {
				uintptr_t t[5];

				fasttrap_usdt_args(probe, rp, fake_restore,
				    sizeof (t) / sizeof (t[0]), t);

				cookie = dtrace_interrupt_disable();
				DTRACE_CPUFLAG_SET(CPU_DTRACE_FAKERESTORE);
				dtrace_probe(probe->ftp_id, t[0], t[1],
				    t[2], t[3], t[4]);
				DTRACE_CPUFLAG_CLEAR(CPU_DTRACE_FAKERESTORE);
				dtrace_interrupt_enable(cookie);

			} else if (probe->ftp_argmap != NULL) {
				uintptr_t t[5];

				fasttrap_usdt_args(probe, rp, fake_restore,
				    sizeof (t) / sizeof (t[0]), t);

				dtrace_probe(probe->ftp_id, t[0], t[1],
				    t[2], t[3], t[4]);

			} else if (fake_restore) {
				uintptr_t arg0 = fasttrap_getreg(rp, R_I0);
				uintptr_t arg1 = fasttrap_getreg(rp, R_I1);
				uintptr_t arg2 = fasttrap_getreg(rp, R_I2);
				uintptr_t arg3 = fasttrap_getreg(rp, R_I3);
				uintptr_t arg4 = fasttrap_getreg(rp, R_I4);

				cookie = dtrace_interrupt_disable();
				DTRACE_CPUFLAG_SET(CPU_DTRACE_FAKERESTORE);
				dtrace_probe(probe->ftp_id, arg0, arg1,
				    arg2, arg3, arg4);
				DTRACE_CPUFLAG_CLEAR(CPU_DTRACE_FAKERESTORE);
				dtrace_interrupt_enable(cookie);

			} else {
				dtrace_probe(probe->ftp_id, rp->r_o0, rp->r_o1,
				    rp->r_o2, rp->r_o3, rp->r_o4);
			}

			continue;
		}

		/*
		 * If this is only a possible return point, we must
		 * be looking at a potential tail call in leaf context.
		 * If the %npc is still within this function, then we
		 * must have misidentified a jmpl as a tail-call when it
		 * is, in fact, part of a jump table. It would be nice to
		 * remove this tracepoint, but this is neither the time
		 * nor the place.
		 */
		if ((tp->ftt_flags & FASTTRAP_F_RETMAYBE) &&
		    rp->r_npc - probe->ftp_faddr < probe->ftp_fsize)
			continue;

		/*
		 * It's possible for a function to branch to the delay slot
		 * of an instruction that we've identified as a return site.
		 * We can dectect this spurious return probe activation by
		 * observing that in this case %npc will be %pc + 4 and %npc
		 * will be inside the current function (unless the user is
		 * doing _crazy_ instruction picking in which case there's
		 * very little we can do). The second check is important
		 * in case the last instructions of a function make a tail-
		 * call to the function located immediately subsequent.
		 */
		if (rp->r_npc == rp->r_pc + 4 &&
		    rp->r_npc - probe->ftp_faddr < probe->ftp_fsize)
			continue;

		/*
		 * The first argument is the offset of return tracepoint
		 * in the function; the remaining arguments are the return
		 * values.
		 *
		 * If fake_restore is set, we need to pull the return values
		 * out of the %i's rather than the %o's -- a little trickier.
		 */
		if (!fake_restore) {
			dtrace_probe(probe->ftp_id, pc - probe->ftp_faddr,
			    rp->r_o0, rp->r_o1, rp->r_o2, rp->r_o3);
		} else {
			uintptr_t arg0 = fasttrap_getreg(rp, R_I0);
			uintptr_t arg1 = fasttrap_getreg(rp, R_I1);
			uintptr_t arg2 = fasttrap_getreg(rp, R_I2);
			uintptr_t arg3 = fasttrap_getreg(rp, R_I3);

			cookie = dtrace_interrupt_disable();
			DTRACE_CPUFLAG_SET(CPU_DTRACE_FAKERESTORE);
			dtrace_probe(probe->ftp_id, pc - probe->ftp_faddr,
			    arg0, arg1, arg2, arg3);
			DTRACE_CPUFLAG_CLEAR(CPU_DTRACE_FAKERESTORE);
			dtrace_interrupt_enable(cookie);
		}
	}

	mutex_exit(pid_mtx);
}

int
fasttrap_pid_probe(struct regs *rp)
{
	proc_t *p = curproc;
	fasttrap_tracepoint_t *tp, tp_local;
	fasttrap_id_t *id;
	pid_t pid;
	uintptr_t pc = rp->r_pc;
	uintptr_t npc = rp->r_npc;
	uintptr_t orig_pc = pc;
	fasttrap_bucket_t *bucket;
	kmutex_t *pid_mtx;
	uint_t fake_restore = 0, is_enabled = 0;
	dtrace_icookie_t cookie;

	/*
	 * It's possible that a user (in a veritable orgy of bad planning)
	 * could redirect this thread's flow of control before it reached the
	 * return probe fasttrap. In this case we need to kill the process
	 * since it's in a unrecoverable state.
	 */
	if (curthread->t_dtrace_step) {
		ASSERT(curthread->t_dtrace_on);
		fasttrap_sigtrap(p, curthread, pc);
		return (0);
	}

	/*
	 * Clear all user tracing flags.
	 */
	curthread->t_dtrace_ft = 0;
	curthread->t_dtrace_pc = 0;
	curthread->t_dtrace_npc = 0;
	curthread->t_dtrace_scrpc = 0;
	curthread->t_dtrace_astpc = 0;

	/*
	 * Treat a child created by a call to vfork(2) as if it were its
	 * parent. We know that there's only one thread of control in such a
	 * process: this one.
	 */
	while (p->p_flag & SVFORK) {
		p = p->p_parent;
	}

	pid = p->p_pid;
	pid_mtx = &cpu_core[CPU->cpu_id].cpuc_pid_lock;
	mutex_enter(pid_mtx);
	bucket = &fasttrap_tpoints.fth_table[FASTTRAP_TPOINTS_INDEX(pid, pc)];

	/*
	 * Lookup the tracepoint that the process just hit.
	 */
	for (tp = bucket->ftb_data; tp != NULL; tp = tp->ftt_next) {
		if (pid == tp->ftt_pid && pc == tp->ftt_pc &&
		    tp->ftt_proc->ftpc_acount != 0)
			break;
	}

	/*
	 * If we couldn't find a matching tracepoint, either a tracepoint has
	 * been inserted without using the pid<pid> ioctl interface (see
	 * fasttrap_ioctl), or somehow we have mislaid this tracepoint.
	 */
	if (tp == NULL) {
		mutex_exit(pid_mtx);
		return (-1);
	}

	for (id = tp->ftt_ids; id != NULL; id = id->fti_next) {
		fasttrap_probe_t *probe = id->fti_probe;
		int isentry = (id->fti_ptype == DTFTP_ENTRY);

		if (id->fti_ptype == DTFTP_IS_ENABLED) {
			is_enabled = 1;
			continue;
		}

		/*
		 * We note that this was an entry probe to help ustack() find
		 * the first caller.
		 */
		if (isentry) {
			cookie = dtrace_interrupt_disable();
			DTRACE_CPUFLAG_SET(CPU_DTRACE_ENTRY);
		}
		dtrace_probe(probe->ftp_id, rp->r_o0, rp->r_o1, rp->r_o2,
		    rp->r_o3, rp->r_o4);
		if (isentry) {
			DTRACE_CPUFLAG_CLEAR(CPU_DTRACE_ENTRY);
			dtrace_interrupt_enable(cookie);
		}
	}

	/*
	 * We're about to do a bunch of work so we cache a local copy of
	 * the tracepoint to emulate the instruction, and then find the
	 * tracepoint again later if we need to light up any return probes.
	 */
	tp_local = *tp;
	mutex_exit(pid_mtx);
	tp = &tp_local;

	/*
	 * If there's an is-enabled probe conntected to this tracepoint it
	 * means that there was a 'mov %g0, %o0' instruction that was placed
	 * there by DTrace when the binary was linked. As this probe is, in
	 * fact, enabled, we need to stuff 1 into %o0. Accordingly, we can
	 * bypass all the instruction emulation logic since we know the
	 * inevitable result. It's possible that a user could construct a
	 * scenario where the 'is-enabled' probe was on some other
	 * instruction, but that would be a rather exotic way to shoot oneself
	 * in the foot.
	 */
	if (is_enabled) {
		rp->r_o0 = 1;
		pc = rp->r_npc;
		npc = pc + 4;
		goto done;
	}

	/*
	 * We emulate certain types of instructions to ensure correctness
	 * (in the case of position dependent instructions) or optimize
	 * common cases. The rest we have the thread execute back in user-
	 * land.
	 */
	switch (tp->ftt_type) {
	case FASTTRAP_T_SAVE:
	{
		int32_t imm;

		/*
		 * This an optimization to let us handle function entry
		 * probes more efficiently. Many functions begin with a save
		 * instruction that follows the pattern:
		 *	save	%sp, <imm>, %sp
		 *
		 * Meanwhile, we've stashed the instruction:
		 *	save	%g1, %g0, %sp
		 *
		 * off of %g7, so all we have to do is stick the right value
		 * into %g1 and reset %pc to point to the instruction we've
		 * cleverly hidden (%npc should not be touched).
		 */

		imm = tp->ftt_instr << 19;
		imm >>= 19;
		rp->r_g1 = rp->r_sp + imm;
		pc = rp->r_g7 + FASTTRAP_OFF_SAVE;
		break;
	}

	case FASTTRAP_T_RESTORE:
	{
		ulong_t value;
		uint_t rd;

		/*
		 * This is an optimization to let us handle function
		 * return probes more efficiently. Most non-leaf functions
		 * end with the sequence:
		 *	ret
		 *	restore	<reg>, <reg_or_imm>, %oX
		 *
		 * We've stashed the instruction:
		 *	restore	%g0, %g0, %g0
		 *
		 * off of %g7 so we just need to place the correct value
		 * in the right %i register (since after our fake-o
		 * restore, the %i's will become the %o's) and set the %pc
		 * to point to our hidden restore. We also set fake_restore to
		 * let fasttrap_return_common() know that it will find the
		 * return values in the %i's rather than the %o's.
		 */

		if (I(tp->ftt_instr)) {
			int32_t imm;

			imm = tp->ftt_instr << 19;
			imm >>= 19;
			value = fasttrap_getreg(rp, RS1(tp->ftt_instr)) + imm;
		} else {
			value = fasttrap_getreg(rp, RS1(tp->ftt_instr)) +
			    fasttrap_getreg(rp, RS2(tp->ftt_instr));
		}

		/*
		 * Convert %o's to %i's; leave %g's as they are.
		 */
		rd = RD(tp->ftt_instr);
		fasttrap_putreg(rp, ((rd & 0x18) == 0x8) ? rd + 16 : rd, value);

		pc = rp->r_g7 + FASTTRAP_OFF_RESTORE;
		fake_restore = 1;
		break;
	}

	case FASTTRAP_T_RETURN:
	{
		uintptr_t target;

		/*
		 * A return instruction is like a jmpl (without the link
		 * part) that executes an implicit restore. We've stashed
		 * the instruction:
		 *	return %o0
		 *
		 * off of %g7 so we just need to place the target in %o0
		 * and set the %pc to point to the stashed return instruction.
		 * We use %o0 since that register disappears after the return
		 * executes, erasing any evidence of this tampering.
		 */
		if (I(tp->ftt_instr)) {
			int32_t imm;

			imm = tp->ftt_instr << 19;
			imm >>= 19;
			target = fasttrap_getreg(rp, RS1(tp->ftt_instr)) + imm;
		} else {
			target = fasttrap_getreg(rp, RS1(tp->ftt_instr)) +
			    fasttrap_getreg(rp, RS2(tp->ftt_instr));
		}

		fasttrap_putreg(rp, R_O0, target);

		pc = rp->r_g7 + FASTTRAP_OFF_RETURN;
		fake_restore = 1;
		break;
	}

	case FASTTRAP_T_OR:
	{
		ulong_t value;

		if (I(tp->ftt_instr)) {
			int32_t imm;

			imm = tp->ftt_instr << 19;
			imm >>= 19;
			value = fasttrap_getreg(rp, RS1(tp->ftt_instr)) | imm;
		} else {
			value = fasttrap_getreg(rp, RS1(tp->ftt_instr)) |
			    fasttrap_getreg(rp, RS2(tp->ftt_instr));
		}

		fasttrap_putreg(rp, RD(tp->ftt_instr), value);
		pc = rp->r_npc;
		npc = pc + 4;
		break;
	}

	case FASTTRAP_T_SETHI:
		if (RD(tp->ftt_instr) != R_G0) {
			uint32_t imm32 = tp->ftt_instr << 10;
			fasttrap_putreg(rp, RD(tp->ftt_instr), (ulong_t)imm32);
		}
		pc = rp->r_npc;
		npc = pc + 4;
		break;

	case FASTTRAP_T_CCR:
	{
		uint_t c, v, z, n, taken;
		uint_t ccr = rp->r_tstate >> TSTATE_CCR_SHIFT;

		if (tp->ftt_cc != 0)
			ccr >>= 4;

		c = (ccr >> 0) & 1;
		v = (ccr >> 1) & 1;
		z = (ccr >> 2) & 1;
		n = (ccr >> 3) & 1;

		switch (tp->ftt_code) {
		case 0x0:	/* BN */
			taken = 0;		break;
		case 0x1:	/* BE */
			taken = z;		break;
		case 0x2:	/* BLE */
			taken = z | (n ^ v);	break;
		case 0x3:	/* BL */
			taken = n ^ v;		break;
		case 0x4:	/* BLEU */
			taken = c | z;		break;
		case 0x5:	/* BCS (BLU) */
			taken = c;		break;
		case 0x6:	/* BNEG */
			taken = n;		break;
		case 0x7:	/* BVS */
			taken = v;		break;
		case 0x8:	/* BA */
			/*
			 * We handle the BA case differently since the annul
			 * bit means something slightly different.
			 */
			panic("fasttrap: mishandled a branch");
			taken = 1;		break;
		case 0x9:	/* BNE */
			taken = ~z;		break;
		case 0xa:	/* BG */
			taken = ~(z | (n ^ v));	break;
		case 0xb:	/* BGE */
			taken = ~(n ^ v);	break;
		case 0xc:	/* BGU */
			taken = ~(c | z);	break;
		case 0xd:	/* BCC (BGEU) */
			taken = ~c;		break;
		case 0xe:	/* BPOS */
			taken = ~n;		break;
		case 0xf:	/* BVC */
			taken = ~v;		break;
		}

		if (taken & 1) {
			pc = rp->r_npc;
			npc = tp->ftt_dest;
		} else if (tp->ftt_flags & FASTTRAP_F_ANNUL) {
			/*
			 * Untaken annulled branches don't execute the
			 * instruction in the delay slot.
			 */
			pc = rp->r_npc + 4;
			npc = pc + 4;
		} else {
			pc = rp->r_npc;
			npc = pc + 4;
		}
		break;
	}

	case FASTTRAP_T_FCC:
	{
		uint_t fcc;
		uint_t taken;
		uint64_t fsr;

		dtrace_getfsr(&fsr);

		if (tp->ftt_cc == 0) {
			fcc = (fsr >> 10) & 0x3;
		} else {
			uint_t shift;
			ASSERT(tp->ftt_cc <= 3);
			shift = 30 + tp->ftt_cc * 2;
			fcc = (fsr >> shift) & 0x3;
		}

		switch (tp->ftt_code) {
		case 0x0:	/* FBN */
			taken = (1 << fcc) & (0|0|0|0);	break;
		case 0x1:	/* FBNE */
			taken = (1 << fcc) & (8|4|2|0);	break;
		case 0x2:	/* FBLG */
			taken = (1 << fcc) & (0|4|2|0);	break;
		case 0x3:	/* FBUL */
			taken = (1 << fcc) & (8|0|2|0);	break;
		case 0x4:	/* FBL */
			taken = (1 << fcc) & (0|0|2|0);	break;
		case 0x5:	/* FBUG */
			taken = (1 << fcc) & (8|4|0|0);	break;
		case 0x6:	/* FBG */
			taken = (1 << fcc) & (0|4|0|0);	break;
		case 0x7:	/* FBU */
			taken = (1 << fcc) & (8|0|0|0);	break;
		case 0x8:	/* FBA */
			/*
			 * We handle the FBA case differently since the annul
			 * bit means something slightly different.
			 */
			panic("fasttrap: mishandled a branch");
			taken = (1 << fcc) & (8|4|2|1);	break;
		case 0x9:	/* FBE */
			taken = (1 << fcc) & (0|0|0|1);	break;
		case 0xa:	/* FBUE */
			taken = (1 << fcc) & (8|0|0|1);	break;
		case 0xb:	/* FBGE */
			taken = (1 << fcc) & (0|4|0|1);	break;
		case 0xc:	/* FBUGE */
			taken = (1 << fcc) & (8|4|0|1);	break;
		case 0xd:	/* FBLE */
			taken = (1 << fcc) & (0|0|2|1);	break;
		case 0xe:	/* FBULE */
			taken = (1 << fcc) & (8|0|2|1);	break;
		case 0xf:	/* FBO */
			taken = (1 << fcc) & (0|4|2|1);	break;
		}

		if (taken) {
			pc = rp->r_npc;
			npc = tp->ftt_dest;
		} else if (tp->ftt_flags & FASTTRAP_F_ANNUL) {
			/*
			 * Untaken annulled branches don't execute the
			 * instruction in the delay slot.
			 */
			pc = rp->r_npc + 4;
			npc = pc + 4;
		} else {
			pc = rp->r_npc;
			npc = pc + 4;
		}
		break;
	}

	case FASTTRAP_T_REG:
	{
		int64_t value;
		uint_t taken;
		uint_t reg = RS1(tp->ftt_instr);

		/*
		 * An ILP32 process shouldn't be using a branch predicated on
		 * an %i or an %l since it would violate the ABI. It's a
		 * violation of the ABI because we can't ensure deterministic
		 * behavior. We should have identified this case when we
		 * enabled the probe.
		 */
		ASSERT(p->p_model == DATAMODEL_LP64 || reg < 16);

		value = (int64_t)fasttrap_getreg(rp, reg);

		switch (tp->ftt_code) {
		case 0x1:	/* BRZ */
			taken = (value == 0);	break;
		case 0x2:	/* BRLEZ */
			taken = (value <= 0);	break;
		case 0x3:	/* BRLZ */
			taken = (value < 0);	break;
		case 0x5:	/* BRNZ */
			taken = (value != 0);	break;
		case 0x6:	/* BRGZ */
			taken = (value > 0);	break;
		case 0x7:	/* BRGEZ */
			taken = (value >= 0);	break;
		default:
		case 0x0:
		case 0x4:
			panic("fasttrap: mishandled a branch");
		}

		if (taken) {
			pc = rp->r_npc;
			npc = tp->ftt_dest;
		} else if (tp->ftt_flags & FASTTRAP_F_ANNUL) {
			/*
			 * Untaken annulled branches don't execute the
			 * instruction in the delay slot.
			 */
			pc = rp->r_npc + 4;
			npc = pc + 4;
		} else {
			pc = rp->r_npc;
			npc = pc + 4;
		}
		break;
	}

	case FASTTRAP_T_ALWAYS:
		/*
		 * BAs, BA,As...
		 */

		if (tp->ftt_flags & FASTTRAP_F_ANNUL) {
			/*
			 * Annulled branch always instructions never execute
			 * the instruction in the delay slot.
			 */
			pc = tp->ftt_dest;
			npc = tp->ftt_dest + 4;
		} else {
			pc = rp->r_npc;
			npc = tp->ftt_dest;
		}
		break;

	case FASTTRAP_T_RDPC:
		fasttrap_putreg(rp, RD(tp->ftt_instr), rp->r_pc);
		pc = rp->r_npc;
		npc = pc + 4;
		break;

	case FASTTRAP_T_CALL:
		/*
		 * It's a call _and_ link remember...
		 */
		rp->r_o7 = rp->r_pc;
		pc = rp->r_npc;
		npc = tp->ftt_dest;
		break;

	case FASTTRAP_T_JMPL:
		pc = rp->r_npc;

		if (I(tp->ftt_instr)) {
			uint_t rs1 = RS1(tp->ftt_instr);
			int32_t imm;

			imm = tp->ftt_instr << 19;
			imm >>= 19;
			npc = fasttrap_getreg(rp, rs1) + imm;
		} else {
			uint_t rs1 = RS1(tp->ftt_instr);
			uint_t rs2 = RS2(tp->ftt_instr);

			npc = fasttrap_getreg(rp, rs1) +
			    fasttrap_getreg(rp, rs2);
		}

		/*
		 * Do the link part of the jump-and-link instruction.
		 */
		fasttrap_putreg(rp, RD(tp->ftt_instr), rp->r_pc);

		break;

	case FASTTRAP_T_COMMON:
	{
		curthread->t_dtrace_scrpc = rp->r_g7;
		curthread->t_dtrace_astpc = rp->r_g7 + FASTTRAP_OFF_FTRET;

		/*
		 * Copy the instruction to a reserved location in the
		 * user-land thread structure, then set the PC to that
		 * location and leave the NPC alone. We take pains to ensure
		 * consistency in the instruction stream (See SPARC
		 * Architecture Manual Version 9, sections 8.4.7, A.20, and
		 * H.1.6; UltraSPARC I/II User's Manual, sections 3.1.1.1,
		 * and 13.6.4) by using the ASI ASI_BLK_COMMIT_S to copy the
		 * instruction into the user's address space without
		 * bypassing the I$. There's no AS_USER version of this ASI
		 * (as exist for other ASIs) so we use the lofault
		 * mechanism to catch faults.
		 */
		if (dtrace_blksuword32(rp->r_g7, &tp->ftt_instr, 1) == -1) {
			/*
			 * If the copyout fails, then the process's state
			 * is not consistent (the effects of the traced
			 * instruction will never be seen). This process
			 * cannot be allowed to continue execution.
			 */
			fasttrap_sigtrap(curproc, curthread, pc);
			return (0);
		}

		curthread->t_dtrace_pc = pc;
		curthread->t_dtrace_npc = npc;
		curthread->t_dtrace_on = 1;

		pc = curthread->t_dtrace_scrpc;

		if (tp->ftt_retids != NULL) {
			curthread->t_dtrace_step = 1;
			curthread->t_dtrace_ret = 1;
			npc = curthread->t_dtrace_astpc;
		}
		break;
	}

	default:
		panic("fasttrap: mishandled an instruction");
	}

	/*
	 * This bit me in the ass a couple of times, so lets toss this
	 * in as a cursory sanity check.
	 */
	ASSERT(pc != rp->r_g7 + 4);
	ASSERT(pc != rp->r_g7 + 8);

done:
	/*
	 * If there were no return probes when we first found the tracepoint,
	 * we should feel no obligation to honor any return probes that were
	 * subsequently enabled -- they'll just have to wait until the next
	 * time around.
	 */
	if (tp->ftt_retids != NULL) {
		/*
		 * We need to wait until the results of the instruction are
		 * apparent before invoking any return probes. If this
		 * instruction was emulated we can just call
		 * fasttrap_return_common(); if it needs to be executed, we
		 * need to wait until we return to the kernel.
		 */
		if (tp->ftt_type != FASTTRAP_T_COMMON) {
			fasttrap_return_common(rp, orig_pc, pid, fake_restore);
		} else {
			ASSERT(curthread->t_dtrace_ret != 0);
			ASSERT(curthread->t_dtrace_pc == orig_pc);
			ASSERT(curthread->t_dtrace_scrpc == rp->r_g7);
			ASSERT(npc == curthread->t_dtrace_astpc);
		}
	}

	ASSERT(pc != 0);
	rp->r_pc = pc;
	rp->r_npc = npc;

	return (0);
}

int
fasttrap_return_probe(struct regs *rp)
{
	proc_t *p = ttoproc(curthread);
	pid_t pid;
	uintptr_t pc = curthread->t_dtrace_pc;
	uintptr_t npc = curthread->t_dtrace_npc;

	curthread->t_dtrace_pc = 0;
	curthread->t_dtrace_npc = 0;
	curthread->t_dtrace_scrpc = 0;
	curthread->t_dtrace_astpc = 0;

	/*
	 * Treat a child created by a call to vfork(2) as if it were its
	 * parent. We know there's only one thread of control in such a
	 * process: this one.
	 */
	while (p->p_flag & SVFORK) {
		p = p->p_parent;
	}

	/*
	 * We set the %pc and %npc to their values when the traced
	 * instruction was initially executed so that it appears to
	 * dtrace_probe() that we're on the original instruction, and so that
	 * the user can't easily detect our complex web of lies.
	 * dtrace_return_probe() (our caller) will correctly set %pc and %npc
	 * after we return.
	 */
	rp->r_pc = pc;
	rp->r_npc = npc;

	pid = p->p_pid;
	fasttrap_return_common(rp, pc, pid, 0);

	return (0);
}

int
fasttrap_tracepoint_install(proc_t *p, fasttrap_tracepoint_t *tp)
{
	fasttrap_instr_t instr = FASTTRAP_INSTR;

	if (uwrite(p, &instr, 4, tp->ftt_pc) != 0)
		return (-1);

	return (0);
}

int
fasttrap_tracepoint_remove(proc_t *p, fasttrap_tracepoint_t *tp)
{
	fasttrap_instr_t instr;

	/*
	 * Distinguish between read or write failures and a changed
	 * instruction.
	 */
	if (uread(p, &instr, 4, tp->ftt_pc) != 0)
		return (0);
	if (instr != FASTTRAP_INSTR && instr != BREAKPOINT_INSTR)
		return (0);
	if (uwrite(p, &tp->ftt_instr, 4, tp->ftt_pc) != 0)
		return (-1);

	return (0);
}

int
fasttrap_tracepoint_init(proc_t *p, fasttrap_tracepoint_t *tp, uintptr_t pc,
    fasttrap_probe_type_t type)
{
	uint32_t instr;
	int32_t disp;

	/*
	 * Read the instruction at the given address out of the process's
	 * address space. We don't have to worry about a debugger
	 * changing this instruction before we overwrite it with our trap
	 * instruction since P_PR_LOCK is set.
	 */
	if (uread(p, &instr, 4, pc) != 0)
		return (-1);

	/*
	 * Decode the instruction to fill in the probe flags. We can have
	 * the process execute most instructions on its own using a pc/npc
	 * trick, but pc-relative control transfer present a problem since
	 * we're relocating the instruction. We emulate these instructions
	 * in the kernel. We assume a default type and over-write that as
	 * needed.
	 *
	 * pc-relative instructions must be emulated for correctness;
	 * other instructions (which represent a large set of commonly traced
	 * instructions) are emulated or otherwise optimized for performance.
	 */
	tp->ftt_type = FASTTRAP_T_COMMON;
	if (OP(instr) == 1) {
		/*
		 * Call instructions.
		 */
		tp->ftt_type = FASTTRAP_T_CALL;
		disp = DISP30(instr) << 2;
		tp->ftt_dest = pc + (intptr_t)disp;

	} else if (OP(instr) == 0) {
		/*
		 * Branch instructions.
		 *
		 * Unconditional branches need careful attention when they're
		 * annulled: annulled unconditional branches never execute
		 * the instruction in the delay slot.
		 */
		switch (OP2(instr)) {
		case OP2_ILLTRAP:
		case 0x7:
			/*
			 * The compiler may place an illtrap after a call to
			 * a function that returns a structure. In the case of
			 * a returned structure, the compiler places an illtrap
			 * whose const22 field is the size of the returned
			 * structure immediately following the delay slot of
			 * the call. To stay out of the way, we refuse to
			 * place tracepoints on top of illtrap instructions.
			 *
			 * This is one of the dumbest architectural decisions
			 * I've ever had to work around.
			 *
			 * We also identify the only illegal op2 value (See
			 * SPARC Architecture Manual Version 9, E.2 table 31).
			 */
			return (-1);

		case OP2_BPcc:
			if (COND(instr) == 8) {
				tp->ftt_type = FASTTRAP_T_ALWAYS;
			} else {
				/*
				 * Check for an illegal instruction.
				 */
				if (CC(instr) & 1)
					return (-1);
				tp->ftt_type = FASTTRAP_T_CCR;
				tp->ftt_cc = CC(instr);
				tp->ftt_code = COND(instr);
			}

			if (A(instr) != 0)
				tp->ftt_flags |= FASTTRAP_F_ANNUL;

			disp = DISP19(instr);
			disp <<= 13;
			disp >>= 11;
			tp->ftt_dest = pc + (intptr_t)disp;
			break;

		case OP2_Bicc:
			if (COND(instr) == 8) {
				tp->ftt_type = FASTTRAP_T_ALWAYS;
			} else {
				tp->ftt_type = FASTTRAP_T_CCR;
				tp->ftt_cc = 0;
				tp->ftt_code = COND(instr);
			}

			if (A(instr) != 0)
				tp->ftt_flags |= FASTTRAP_F_ANNUL;

			disp = DISP22(instr);
			disp <<= 10;
			disp >>= 8;
			tp->ftt_dest = pc + (intptr_t)disp;
			break;

		case OP2_BPr:
			/*
			 * Check for an illegal instruction.
			 */
			if ((RCOND(instr) & 3) == 0)
				return (-1);

			/*
			 * It's a violation of the v8plus ABI to use a
			 * register-predicated branch in a 32-bit app if
			 * the register used is an %l or an %i (%gs and %os
			 * are legit because they're not saved to the stack
			 * in 32-bit words when we take a trap).
			 */
			if (p->p_model == DATAMODEL_ILP32 && RS1(instr) >= 16)
				return (-1);

			tp->ftt_type = FASTTRAP_T_REG;
			if (A(instr) != 0)
				tp->ftt_flags |= FASTTRAP_F_ANNUL;
			disp = DISP16(instr);
			disp <<= 16;
			disp >>= 14;
			tp->ftt_dest = pc + (intptr_t)disp;
			tp->ftt_code = RCOND(instr);
			break;

		case OP2_SETHI:
			tp->ftt_type = FASTTRAP_T_SETHI;
			break;

		case OP2_FBPfcc:
			if (COND(instr) == 8) {
				tp->ftt_type = FASTTRAP_T_ALWAYS;
			} else {
				tp->ftt_type = FASTTRAP_T_FCC;
				tp->ftt_cc = CC(instr);
				tp->ftt_code = COND(instr);
			}

			if (A(instr) != 0)
				tp->ftt_flags |= FASTTRAP_F_ANNUL;

			disp = DISP19(instr);
			disp <<= 13;
			disp >>= 11;
			tp->ftt_dest = pc + (intptr_t)disp;
			break;

		case OP2_FBfcc:
			if (COND(instr) == 8) {
				tp->ftt_type = FASTTRAP_T_ALWAYS;
			} else {
				tp->ftt_type = FASTTRAP_T_FCC;
				tp->ftt_cc = 0;
				tp->ftt_code = COND(instr);
			}

			if (A(instr) != 0)
				tp->ftt_flags |= FASTTRAP_F_ANNUL;

			disp = DISP22(instr);
			disp <<= 10;
			disp >>= 8;
			tp->ftt_dest = pc + (intptr_t)disp;
			break;
		}

	} else if (OP(instr) == 2) {
		switch (OP3(instr)) {
		case OP3_RETURN:
			tp->ftt_type = FASTTRAP_T_RETURN;
			break;

		case OP3_JMPL:
			tp->ftt_type = FASTTRAP_T_JMPL;
			break;

		case OP3_RD:
			if (RS1(instr) == 5)
				tp->ftt_type = FASTTRAP_T_RDPC;
			break;

		case OP3_SAVE:
			/*
			 * We optimize for save instructions at function
			 * entry; see the comment in fasttrap_pid_probe()
			 * (near FASTTRAP_T_SAVE) for details.
			 */
			if (fasttrap_optimize_save != 0 &&
			    type == DTFTP_ENTRY &&
			    I(instr) == 1 && RD(instr) == R_SP)
				tp->ftt_type = FASTTRAP_T_SAVE;
			break;

		case OP3_RESTORE:
			/*
			 * We optimize restore instructions at function
			 * return; see the comment in fasttrap_pid_probe()
			 * (near FASTTRAP_T_RESTORE) for details.
			 *
			 * rd must be an %o or %g register.
			 */
			if ((RD(instr) & 0x10) == 0)
				tp->ftt_type = FASTTRAP_T_RESTORE;
			break;

		case OP3_OR:
			/*
			 * A large proportion of instructions in the delay
			 * slot of retl instructions are or's so we emulate
			 * these downstairs as an optimization.
			 */
			tp->ftt_type = FASTTRAP_T_OR;
			break;

		case OP3_TCC:
			/*
			 * Breakpoint instructions are effectively position-
			 * dependent since the debugger uses the %pc value
			 * to lookup which breakpoint was executed. As a
			 * result, we can't actually instrument breakpoints.
			 */
			if (SW_TRAP(instr) == ST_BREAKPOINT)
				return (-1);
			break;

		case 0x19:
		case 0x1d:
		case 0x29:
		case 0x33:
		case 0x3f:
			/*
			 * Identify illegal instructions (See SPARC
			 * Architecture Manual Version 9, E.2 table 32).
			 */
			return (-1);
		}
	} else if (OP(instr) == 3) {
		uint32_t op3 = OP3(instr);

		/*
		 * Identify illegal instructions (See SPARC Architecture
		 * Manual Version 9, E.2 table 33).
		 */
		if ((op3 & 0x28) == 0x28) {
			if (op3 != OP3_PREFETCH && op3 != OP3_CASA &&
			    op3 != OP3_PREFETCHA && op3 != OP3_CASXA)
				return (-1);
		} else {
			if ((op3 & 0x0f) == 0x0c || (op3 & 0x3b) == 0x31)
				return (-1);
		}
	}

	tp->ftt_instr = instr;

	/*
	 * We don't know how this tracepoint is going to be used, but in case
	 * it's used as part of a function return probe, we need to indicate
	 * whether it's always a return site or only potentially a return
	 * site. If it's part of a return probe, it's always going to be a
	 * return from that function if it's a restore instruction or if
	 * the previous instruction was a return. If we could reliably
	 * distinguish jump tables from return sites, this wouldn't be
	 * necessary.
	 */
	if (tp->ftt_type != FASTTRAP_T_RESTORE &&
	    (uread(p, &instr, 4, pc - sizeof (instr)) != 0 ||
	    !(OP(instr) == 2 && OP3(instr) == OP3_RETURN)))
		tp->ftt_flags |= FASTTRAP_F_RETMAYBE;

	return (0);
}

/*ARGSUSED*/
uint64_t
fasttrap_pid_getarg(void *arg, dtrace_id_t id, void *parg, int argno,
    int aframes)
{
	return (fasttrap_anarg(ttolwp(curthread)->lwp_regs, argno));
}

/*ARGSUSED*/
uint64_t
fasttrap_usdt_getarg(void *arg, dtrace_id_t id, void *parg, int argno,
    int aframes)
{
	return (fasttrap_anarg(ttolwp(curthread)->lwp_regs, argno));
}

static uint64_t fasttrap_getreg_fast_cnt;
static uint64_t fasttrap_getreg_mpcb_cnt;
static uint64_t fasttrap_getreg_slow_cnt;

static ulong_t
fasttrap_getreg(struct regs *rp, uint_t reg)
{
	ulong_t value;
	dtrace_icookie_t cookie;
	struct machpcb *mpcb;
	extern ulong_t dtrace_getreg_win(uint_t, uint_t);

	/*
	 * We have the %os and %gs in our struct regs, but if we need to
	 * snag a %l or %i we need to go scrounging around in the process's
	 * address space.
	 */
	if (reg == 0)
		return (0);

	if (reg < 16)
		return ((&rp->r_g1)[reg - 1]);

	/*
	 * Before we look at the user's stack, we'll check the register
	 * windows to see if the information we want is in there.
	 */
	cookie = dtrace_interrupt_disable();
	if (dtrace_getotherwin() > 0) {
		value = dtrace_getreg_win(reg, 1);
		dtrace_interrupt_enable(cookie);

		atomic_add_64(&fasttrap_getreg_fast_cnt, 1);

		return (value);
	}
	dtrace_interrupt_enable(cookie);

	/*
	 * First check the machpcb structure to see if we've already read
	 * in the register window we're looking for; if we haven't, (and
	 * we probably haven't) try to copy in the value of the register.
	 */
	/* LINTED - alignment */
	mpcb = (struct machpcb *)((caddr_t)rp - REGOFF);

	if (get_udatamodel() == DATAMODEL_NATIVE) {
		struct frame *fr = (struct frame *)(rp->r_sp + STACK_BIAS);

		if (mpcb->mpcb_wbcnt > 0) {
			struct rwindow *rwin = (void *)mpcb->mpcb_wbuf;
			int i = mpcb->mpcb_wbcnt;
			do {
				i--;
				if ((long)mpcb->mpcb_spbuf[i] != rp->r_sp)
					continue;

				atomic_add_64(&fasttrap_getreg_mpcb_cnt, 1);
				return (rwin[i].rw_local[reg - 16]);
			} while (i > 0);
		}

		if (fasttrap_fulword(&fr->fr_local[reg - 16], &value) != 0)
			goto err;
	} else {
		struct frame32 *fr =
		    (struct frame32 *)(uintptr_t)(caddr32_t)rp->r_sp;
		uint32_t *v32 = (uint32_t *)&value;

		if (mpcb->mpcb_wbcnt > 0) {
			struct rwindow32 *rwin = (void *)mpcb->mpcb_wbuf;
			int i = mpcb->mpcb_wbcnt;
			do {
				i--;
				if ((long)mpcb->mpcb_spbuf[i] != rp->r_sp)
					continue;

				atomic_add_64(&fasttrap_getreg_mpcb_cnt, 1);
				return (rwin[i].rw_local[reg - 16]);
			} while (i > 0);
		}

		if (fasttrap_fuword32(&fr->fr_local[reg - 16], &v32[1]) != 0)
			goto err;

		v32[0] = 0;
	}

	atomic_add_64(&fasttrap_getreg_slow_cnt, 1);
	return (value);

err:
	/*
	 * If the copy in failed, the process will be in a irrecoverable
	 * state, and we have no choice but to kill it.
	 */
	psignal(ttoproc(curthread), SIGILL);
	return (0);
}

static uint64_t fasttrap_putreg_fast_cnt;
static uint64_t fasttrap_putreg_mpcb_cnt;
static uint64_t fasttrap_putreg_slow_cnt;

static void
fasttrap_putreg(struct regs *rp, uint_t reg, ulong_t value)
{
	dtrace_icookie_t cookie;
	struct machpcb *mpcb;
	extern void dtrace_putreg_win(uint_t, ulong_t);

	if (reg == 0)
		return;

	if (reg < 16) {
		(&rp->r_g1)[reg - 1] = value;
		return;
	}

	/*
	 * If the user process is still using some register windows, we
	 * can just place the value in the correct window.
	 */
	cookie = dtrace_interrupt_disable();
	if (dtrace_getotherwin() > 0) {
		dtrace_putreg_win(reg, value);
		dtrace_interrupt_enable(cookie);
		atomic_add_64(&fasttrap_putreg_fast_cnt, 1);
		return;
	}
	dtrace_interrupt_enable(cookie);

	/*
	 * First see if there's a copy of the register window in the
	 * machpcb structure that we can modify; if there isn't try to
	 * copy out the value. If that fails, we try to create a new
	 * register window in the machpcb structure. While this isn't
	 * _precisely_ the intended use of the machpcb structure, it
	 * can't cause any problems since we know at this point in the
	 * code that all of the user's data have been flushed out of the
	 * register file (since %otherwin is 0).
	 */
	/* LINTED - alignment */
	mpcb = (struct machpcb *)((caddr_t)rp - REGOFF);

	if (get_udatamodel() == DATAMODEL_NATIVE) {
		struct frame *fr = (struct frame *)(rp->r_sp + STACK_BIAS);
		/* LINTED - alignment */
		struct rwindow *rwin = (struct rwindow *)mpcb->mpcb_wbuf;

		if (mpcb->mpcb_wbcnt > 0) {
			int i = mpcb->mpcb_wbcnt;
			do {
				i--;
				if ((long)mpcb->mpcb_spbuf[i] != rp->r_sp)
					continue;

				rwin[i].rw_local[reg - 16] = value;
				atomic_add_64(&fasttrap_putreg_mpcb_cnt, 1);
				return;
			} while (i > 0);
		}

		if (fasttrap_sulword(&fr->fr_local[reg - 16], value) != 0) {
			if (mpcb->mpcb_wbcnt >= MAXWIN || copyin(fr,
			    &rwin[mpcb->mpcb_wbcnt], sizeof (*rwin)) != 0)
				goto err;

			rwin[mpcb->mpcb_wbcnt].rw_local[reg - 16] = value;
			mpcb->mpcb_spbuf[mpcb->mpcb_wbcnt] = (caddr_t)rp->r_sp;
			mpcb->mpcb_wbcnt++;
			atomic_add_64(&fasttrap_putreg_mpcb_cnt, 1);
			return;
		}
	} else {
		struct frame32 *fr =
		    (struct frame32 *)(uintptr_t)(caddr32_t)rp->r_sp;
		/* LINTED - alignment */
		struct rwindow32 *rwin = (struct rwindow32 *)mpcb->mpcb_wbuf;
		uint32_t v32 = (uint32_t)value;

		if (mpcb->mpcb_wbcnt > 0) {
			int i = mpcb->mpcb_wbcnt;
			do {
				i--;
				if ((long)mpcb->mpcb_spbuf[i] != rp->r_sp)
					continue;

				rwin[i].rw_local[reg - 16] = v32;
				atomic_add_64(&fasttrap_putreg_mpcb_cnt, 1);
				return;
			} while (i > 0);
		}

		if (fasttrap_suword32(&fr->fr_local[reg - 16], v32) != 0) {
			if (mpcb->mpcb_wbcnt >= MAXWIN || copyin(fr,
			    &rwin[mpcb->mpcb_wbcnt], sizeof (*rwin)) != 0)
				goto err;

			rwin[mpcb->mpcb_wbcnt].rw_local[reg - 16] = v32;
			mpcb->mpcb_spbuf[mpcb->mpcb_wbcnt] = (caddr_t)rp->r_sp;
			mpcb->mpcb_wbcnt++;
			atomic_add_64(&fasttrap_putreg_mpcb_cnt, 1);
			return;
		}
	}

	atomic_add_64(&fasttrap_putreg_slow_cnt, 1);
	return;

err:
	/*
	 * If we couldn't record this register's value, the process is in an
	 * irrecoverable state and we have no choice but to euthanize it.
	 */
	psignal(ttoproc(curthread), SIGILL);
}
