/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $NetBSD: trap.c,v 1.58 2002/03/04 04:07:35 dbj Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kdb.h>
#include <sys/proc.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pioctl.h>
#include <sys/ptrace.h>
#include <sys/reboot.h>
#include <sys/syscall.h>
#include <sys/sysent.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/signalvar.h>
#include <sys/vmmeter.h>

#include <security/audit/audit.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>

#include <machine/_inttypes.h>
#include <machine/altivec.h>
#include <machine/cpu.h>
#include <machine/db_machdep.h>
#include <machine/fpu.h>
#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/pmap.h>
#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/spr.h>
#include <machine/sr.h>

static void	trap_fatal(struct trapframe *frame);
static void	printtrap(u_int vector, struct trapframe *frame, int isfatal,
		    int user);
static int	trap_pfault(struct trapframe *frame, int user);
static int	fix_unaligned(struct thread *td, struct trapframe *frame);
static int	handle_onfault(struct trapframe *frame);
static void	syscall(struct trapframe *frame);

#ifdef __powerpc64__
       void	handle_kernel_slb_spill(int, register_t, register_t);
static int	handle_user_slb_spill(pmap_t pm, vm_offset_t addr);
extern int	n_slbs;
#endif

struct powerpc_exception {
	u_int	vector;
	char	*name;
};

#ifdef KDTRACE_HOOKS
#include <sys/dtrace_bsd.h>

int (*dtrace_invop_jump_addr)(struct trapframe *);
#endif

static struct powerpc_exception powerpc_exceptions[] = {
	{ 0x0100, "system reset" },
	{ 0x0200, "machine check" },
	{ 0x0300, "data storage interrupt" },
	{ 0x0380, "data segment exception" },
	{ 0x0400, "instruction storage interrupt" },
	{ 0x0480, "instruction segment exception" },
	{ 0x0500, "external interrupt" },
	{ 0x0600, "alignment" },
	{ 0x0700, "program" },
	{ 0x0800, "floating-point unavailable" },
	{ 0x0900, "decrementer" },
	{ 0x0c00, "system call" },
	{ 0x0d00, "trace" },
	{ 0x0e00, "floating-point assist" },
	{ 0x0f00, "performance monitoring" },
	{ 0x0f20, "altivec unavailable" },
	{ 0x1000, "instruction tlb miss" },
	{ 0x1100, "data load tlb miss" },
	{ 0x1200, "data store tlb miss" },
	{ 0x1300, "instruction breakpoint" },
	{ 0x1400, "system management" },
	{ 0x1600, "altivec assist" },
	{ 0x1700, "thermal management" },
	{ 0x2000, "run mode/trace" },
	{ 0x3000, NULL }
};

static const char *
trapname(u_int vector)
{
	struct	powerpc_exception *pe;

	for (pe = powerpc_exceptions; pe->vector != 0x3000; pe++) {
		if (pe->vector == vector)
			return (pe->name);
	}

	return ("unknown");
}

void
trap(struct trapframe *frame)
{
	struct thread	*td;
	struct proc	*p;
#ifdef KDTRACE_HOOKS
	uint32_t inst;
#endif
	int		sig, type, user;
	u_int		ucode;
	ksiginfo_t	ksi;

	PCPU_INC(cnt.v_trap);

	td = curthread;
	p = td->td_proc;

	type = ucode = frame->exc;
	sig = 0;
	user = frame->srr1 & PSL_PR;

	CTR3(KTR_TRAP, "trap: %s type=%s (%s)", td->td_name,
	    trapname(type), user ? "user" : "kernel");

#ifdef KDTRACE_HOOKS
	/*
	 * A trap can occur while DTrace executes a probe. Before
	 * executing the probe, DTrace blocks re-scheduling and sets
	 * a flag in its per-cpu flags to indicate that it doesn't
	 * want to fault. On returning from the probe, the no-fault
	 * flag is cleared and finally re-scheduling is enabled.
	 *
	 * If the DTrace kernel module has registered a trap handler,
	 * call it and if it returns non-zero, assume that it has
	 * handled the trap and modified the trap frame so that this
	 * function can return normally.
	 */
	if (dtrace_trap_func != NULL && (*dtrace_trap_func)(frame))
		return;
#endif

	if (user) {
		td->td_pticks = 0;
		td->td_frame = frame;
		if (td->td_ucred != p->p_ucred)
			cred_update_thread(td);

		/* User Mode Traps */
		switch (type) {
		case EXC_RUNMODETRC:
		case EXC_TRC:
			frame->srr1 &= ~PSL_SE;
			sig = SIGTRAP;
			break;

#ifdef __powerpc64__
		case EXC_ISE:
		case EXC_DSE:
			if (handle_user_slb_spill(&p->p_vmspace->vm_pmap,
			    (type == EXC_ISE) ? frame->srr0 :
			    frame->cpu.aim.dar) != 0)
				sig = SIGSEGV;
			break;
#endif
		case EXC_DSI:
		case EXC_ISI:
			sig = trap_pfault(frame, 1);
			break;

		case EXC_SC:
			syscall(frame);
			break;

		case EXC_FPU:
			KASSERT((td->td_pcb->pcb_flags & PCB_FPU) != PCB_FPU,
			    ("FPU already enabled for thread"));
			enable_fpu(td);
			break;

		case EXC_VEC:
			KASSERT((td->td_pcb->pcb_flags & PCB_VEC) != PCB_VEC,
			    ("Altivec already enabled for thread"));
			enable_vec(td);
			break;

		case EXC_VECAST_G4:
		case EXC_VECAST_G5:
			/*
			 * We get a VPU assist exception for IEEE mode
			 * vector operations on denormalized floats.
			 * Emulating this is a giant pain, so for now,
			 * just switch off IEEE mode and treat them as
			 * zero.
			 */

			save_vec(td);
			td->td_pcb->pcb_vec.vscr |= ALTIVEC_VSCR_NJ;
			enable_vec(td);
			break;

		case EXC_ALI:
			if (fix_unaligned(td, frame) != 0)
				sig = SIGBUS;
			else
				frame->srr0 += 4;
			break;

		case EXC_PGM:
			/* Identify the trap reason */
			if (frame->srr1 & EXC_PGM_TRAP) {
#ifdef KDTRACE_HOOKS
				inst = fuword32((const void *)frame->srr0);
				if (inst == 0x0FFFDDDD && dtrace_pid_probe_ptr != NULL) {
					struct reg regs;
					fill_regs(td, &regs);
					(*dtrace_pid_probe_ptr)(&regs);
					break;
				}
#endif
 				sig = SIGTRAP;
			} else {
				sig = ppc_instr_emulate(frame, td->td_pcb);
			}
			break;

		default:
			trap_fatal(frame);
		}
	} else {
		/* Kernel Mode Traps */

		KASSERT(cold || td->td_ucred != NULL,
		    ("kernel trap doesn't have ucred"));
		switch (type) {
#ifdef KDTRACE_HOOKS
		case EXC_PGM:
			if (frame->srr1 & EXC_PGM_TRAP) {
				if (*(uint32_t *)frame->srr0 == 0x7c810808) {
					if (dtrace_invop_jump_addr != NULL) {
						dtrace_invop_jump_addr(frame);
						return;
					}
				}
			}
			break;
#endif
#ifdef __powerpc64__
		case EXC_DSE:
			if ((frame->cpu.aim.dar & SEGMENT_MASK) == USER_ADDR) {
				__asm __volatile ("slbmte %0, %1" ::
					"r"(td->td_pcb->pcb_cpu.aim.usr_vsid),
					"r"(USER_SLB_SLBE));
				return;
			}
			break;
#endif
		case EXC_DSI:
			if (trap_pfault(frame, 0) == 0)
 				return;
			break;
		case EXC_MCHK:
			if (handle_onfault(frame))
 				return;
			break;
		default:
			break;
		}
		trap_fatal(frame);
	}

	if (sig != 0) {
		if (p->p_sysent->sv_transtrap != NULL)
			sig = (p->p_sysent->sv_transtrap)(sig, type);
		ksiginfo_init_trap(&ksi);
		ksi.ksi_signo = sig;
		ksi.ksi_code = (int) ucode; /* XXX, not POSIX */
		/* ksi.ksi_addr = ? */
		ksi.ksi_trapno = type;
		trapsignal(td, &ksi);
	}

	userret(td, frame);
}

static void
trap_fatal(struct trapframe *frame)
{

	printtrap(frame->exc, frame, 1, (frame->srr1 & PSL_PR));
#ifdef KDB
	if ((debugger_on_panic || kdb_active) &&
	    kdb_trap(frame->exc, 0, frame))
		return;
#endif
	panic("%s trap", trapname(frame->exc));
}

static void
printtrap(u_int vector, struct trapframe *frame, int isfatal, int user)
{

	printf("\n");
	printf("%s %s trap:\n", isfatal ? "fatal" : "handled",
	    user ? "user" : "kernel");
	printf("\n");
	printf("   exception       = 0x%x (%s)\n", vector, trapname(vector));
	switch (vector) {
	case EXC_DSE:
	case EXC_DSI:
		printf("   virtual address = 0x%" PRIxPTR "\n",
		    frame->cpu.aim.dar);
		printf("   dsisr           = 0x%" PRIxPTR "\n",
		    frame->cpu.aim.dsisr);
		break;
	case EXC_ISE:
	case EXC_ISI:
		printf("   virtual address = 0x%" PRIxPTR "\n", frame->srr0);
		break;
	}
	printf("   srr0            = 0x%" PRIxPTR "\n", frame->srr0);
	printf("   srr1            = 0x%" PRIxPTR "\n", frame->srr1);
	printf("   lr              = 0x%" PRIxPTR "\n", frame->lr);
	printf("   curthread       = %p\n", curthread);
	if (curthread != NULL)
		printf("          pid = %d, comm = %s\n",
		    curthread->td_proc->p_pid, curthread->td_name);
	printf("\n");
}

/*
 * Handles a fatal fault when we have onfault state to recover.  Returns
 * non-zero if there was onfault recovery state available.
 */
static int
handle_onfault(struct trapframe *frame)
{
	struct		thread *td;
	faultbuf	*fb;

	td = curthread;
	fb = td->td_pcb->pcb_onfault;
	if (fb != NULL) {
		frame->srr0 = (*fb)[0];
		frame->fixreg[1] = (*fb)[1];
		frame->fixreg[2] = (*fb)[2];
		frame->fixreg[3] = 1;
		frame->cr = (*fb)[3];
		bcopy(&(*fb)[4], &frame->fixreg[13],
		    19 * sizeof(register_t));
		return (1);
	}
	return (0);
}

int
cpu_fetch_syscall_args(struct thread *td, struct syscall_args *sa)
{
	struct proc *p;
	struct trapframe *frame;
	caddr_t	params;
	size_t argsz;
	int error, n, i;

	p = td->td_proc;
	frame = td->td_frame;

	sa->code = frame->fixreg[0];
	params = (caddr_t)(frame->fixreg + FIRSTARG);
	n = NARGREG;

	if (sa->code == SYS_syscall) {
		/*
		 * code is first argument,
		 * followed by actual args.
		 */
		sa->code = *(register_t *) params;
		params += sizeof(register_t);
		n -= 1;
	} else if (sa->code == SYS___syscall) {
		/*
		 * Like syscall, but code is a quad,
		 * so as to maintain quad alignment
		 * for the rest of the args.
		 */
		if (SV_PROC_FLAG(p, SV_ILP32)) {
			params += sizeof(register_t);
			sa->code = *(register_t *) params;
			params += sizeof(register_t);
			n -= 2;
		} else {
			sa->code = *(register_t *) params;
			params += sizeof(register_t);
			n -= 1;
		}
	}

 	if (p->p_sysent->sv_mask)
		sa->code &= p->p_sysent->sv_mask;
	if (sa->code >= p->p_sysent->sv_size)
		sa->callp = &p->p_sysent->sv_table[0];
	else
		sa->callp = &p->p_sysent->sv_table[sa->code];

	sa->narg = sa->callp->sy_narg;

	if (SV_PROC_FLAG(p, SV_ILP32)) {
		argsz = sizeof(uint32_t);

		for (i = 0; i < n; i++)
			sa->args[i] = ((u_register_t *)(params))[i] &
			    0xffffffff;
	} else {
		argsz = sizeof(uint64_t);

		for (i = 0; i < n; i++)
			sa->args[i] = ((u_register_t *)(params))[i];
	}

	if (sa->narg > n)
		error = copyin(MOREARGS(frame->fixreg[1]), sa->args + n,
			       (sa->narg - n) * argsz);
	else
		error = 0;

#ifdef __powerpc64__
	if (SV_PROC_FLAG(p, SV_ILP32) && sa->narg > n) {
		/* Expand the size of arguments copied from the stack */

		for (i = sa->narg; i >= n; i--)
			sa->args[i] = ((uint32_t *)(&sa->args[n]))[i-n];
	}
#endif

	if (error == 0) {
		td->td_retval[0] = 0;
		td->td_retval[1] = frame->fixreg[FIRSTARG + 1];
	}
	return (error);
}

#include "../../kern/subr_syscall.c"

void
syscall(struct trapframe *frame)
{
	struct thread *td;
	struct syscall_args sa;
	int error;

	td = curthread;
	td->td_frame = frame;

#ifdef __powerpc64__
	/*
	 * Speculatively restore last user SLB segment, which we know is
	 * invalid already, since we are likely to do copyin()/copyout().
	 */
	__asm __volatile ("slbmte %0, %1; isync" ::
            "r"(td->td_pcb->pcb_cpu.aim.usr_vsid), "r"(USER_SLB_SLBE));
#endif

	error = syscallenter(td, &sa);
	syscallret(td, error, &sa);
}

#ifdef __powerpc64__
/* Handle kernel SLB faults -- runs in real mode, all seat belts off */
void
handle_kernel_slb_spill(int type, register_t dar, register_t srr0)
{
	struct slb *slbcache;
	uint64_t slbe, slbv;
	uint64_t esid, addr;
	int i;

	addr = (type == EXC_ISE) ? srr0 : dar;
	slbcache = PCPU_GET(slb);
	esid = (uintptr_t)addr >> ADDR_SR_SHFT;
	slbe = (esid << SLBE_ESID_SHIFT) | SLBE_VALID;
	
	/* See if the hardware flushed this somehow (can happen in LPARs) */
	for (i = 0; i < n_slbs; i++)
		if (slbcache[i].slbe == (slbe | (uint64_t)i))
			return;

	/* Not in the map, needs to actually be added */
	slbv = kernel_va_to_slbv(addr);
	if (slbcache[USER_SLB_SLOT].slbe == 0) {
		for (i = 0; i < n_slbs; i++) {
			if (i == USER_SLB_SLOT)
				continue;
			if (!(slbcache[i].slbe & SLBE_VALID))
				goto fillkernslb;
		}

		if (i == n_slbs)
			slbcache[USER_SLB_SLOT].slbe = 1;
	}

	/* Sacrifice a random SLB entry that is not the user entry */
	i = mftb() % n_slbs;
	if (i == USER_SLB_SLOT)
		i = (i+1) % n_slbs;

fillkernslb:
	/* Write new entry */
	slbcache[i].slbv = slbv;
	slbcache[i].slbe = slbe | (uint64_t)i;

	/* Trap handler will restore from cache on exit */
}

static int 
handle_user_slb_spill(pmap_t pm, vm_offset_t addr)
{
	struct slb *user_entry;
	uint64_t esid;
	int i;

	esid = (uintptr_t)addr >> ADDR_SR_SHFT;

	PMAP_LOCK(pm);
	user_entry = user_va_to_slb_entry(pm, addr);

	if (user_entry == NULL) {
		/* allocate_vsid auto-spills it */
		(void)allocate_user_vsid(pm, esid, 0);
	} else {
		/*
		 * Check that another CPU has not already mapped this.
		 * XXX: Per-thread SLB caches would be better.
		 */
		for (i = 0; i < pm->pm_slb_len; i++)
			if (pm->pm_slb[i] == user_entry)
				break;

		if (i == pm->pm_slb_len)
			slb_insert_user(pm, user_entry);
	}
	PMAP_UNLOCK(pm);

	return (0);
}
#endif

static int
trap_pfault(struct trapframe *frame, int user)
{
	vm_offset_t	eva, va;
	struct		thread *td;
	struct		proc *p;
	vm_map_t	map;
	vm_prot_t	ftype;
	int		rv;
	register_t	user_sr;

	td = curthread;
	p = td->td_proc;
	if (frame->exc == EXC_ISI) {
		eva = frame->srr0;
		ftype = VM_PROT_EXECUTE;
		if (frame->srr1 & SRR1_ISI_PFAULT)
			ftype |= VM_PROT_READ;
	} else {
		eva = frame->cpu.aim.dar;
		if (frame->cpu.aim.dsisr & DSISR_STORE)
			ftype = VM_PROT_WRITE;
		else
			ftype = VM_PROT_READ;
	}

	if (user) {
		map = &p->p_vmspace->vm_map;
	} else {
		if ((eva >> ADDR_SR_SHFT) == (USER_ADDR >> ADDR_SR_SHFT)) {
			if (p->p_vmspace == NULL)
				return (SIGSEGV);

			map = &p->p_vmspace->vm_map;

			user_sr = td->td_pcb->pcb_cpu.aim.usr_segm;
			eva &= ADDR_PIDX | ADDR_POFF;
			eva |= user_sr << ADDR_SR_SHFT;
		} else {
			map = kernel_map;
		}
	}
	va = trunc_page(eva);

	if (map != kernel_map) {
		/*
		 * Keep swapout from messing with us during this
		 *	critical time.
		 */
		PROC_LOCK(p);
		++p->p_lock;
		PROC_UNLOCK(p);

		/* Fault in the user page: */
		rv = vm_fault(map, va, ftype, VM_FAULT_NORMAL);

		PROC_LOCK(p);
		--p->p_lock;
		PROC_UNLOCK(p);
		/*
		 * XXXDTRACE: add dtrace_doubletrap_func here?
		 */
	} else {
		/*
		 * Don't have to worry about process locking or stacks in the
		 * kernel.
		 */
		rv = vm_fault(map, va, ftype, VM_FAULT_NORMAL);
	}

	if (rv == KERN_SUCCESS)
		return (0);

	if (!user && handle_onfault(frame))
		return (0);

	return (SIGSEGV);
}

/*
 * For now, this only deals with the particular unaligned access case
 * that gcc tends to generate.  Eventually it should handle all of the
 * possibilities that can happen on a 32-bit PowerPC in big-endian mode.
 */

static int
fix_unaligned(struct thread *td, struct trapframe *frame)
{
	struct thread	*fputhread;
	int		indicator, reg;
	double		*fpr;

	indicator = EXC_ALI_OPCODE_INDICATOR(frame->cpu.aim.dsisr);

	switch (indicator) {
	case EXC_ALI_LFD:
	case EXC_ALI_STFD:
		reg = EXC_ALI_RST(frame->cpu.aim.dsisr);
		fpr = &td->td_pcb->pcb_fpu.fpr[reg];
		fputhread = PCPU_GET(fputhread);

		/* Juggle the FPU to ensure that we've initialized
		 * the FPRs, and that their current state is in
		 * the PCB.
		 */
		if (fputhread != td) {
			if (fputhread)
				save_fpu(fputhread);
			enable_fpu(td);
		}
		save_fpu(td);

		if (indicator == EXC_ALI_LFD) {
			if (copyin((void *)frame->cpu.aim.dar, fpr,
			    sizeof(double)) != 0)
				return -1;
			enable_fpu(td);
		} else {
			if (copyout(fpr, (void *)frame->cpu.aim.dar,
			    sizeof(double)) != 0)
				return -1;
		}
		return 0;
		break;
	}

	return -1;
}

