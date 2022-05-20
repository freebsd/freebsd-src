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
#include <sys/ptrace.h>
#include <sys/reboot.h>
#include <sys/syscall.h>
#include <sys/sysent.h>
#include <sys/systm.h>
#include <sys/kernel.h>
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
#include <machine/psl.h>
#include <machine/slb.h>
#include <machine/spr.h>
#include <machine/sr.h>
#include <machine/trap.h>

/* Below matches setjmp.S */
#define	FAULTBUF_LR	21
#define	FAULTBUF_R1	1
#define	FAULTBUF_R2	2
#define	FAULTBUF_CR	22
#define	FAULTBUF_R14	3

#define	MOREARGS(sp)	((caddr_t)((uintptr_t)(sp) + \
    sizeof(struct callframe) - 3*sizeof(register_t))) /* more args go here */

static void	trap_fatal(struct trapframe *frame);
static void	printtrap(u_int vector, struct trapframe *frame, int isfatal,
		    int user);
static bool	trap_pfault(struct trapframe *frame, bool user, int *signo,
		    int *ucode);
static int	fix_unaligned(struct thread *td, struct trapframe *frame);
static int	handle_onfault(struct trapframe *frame);
static void	syscall(struct trapframe *frame);

#if defined(__powerpc64__) && defined(AIM)
static void	normalize_inputs(void);
#endif

extern vm_offset_t __startkernel;

extern int	copy_fault(void);
extern int	fusufault(void);

#ifdef KDB
int db_trap_glue(struct trapframe *);		/* Called from trap_subr.S */
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
	{ EXC_CRIT,	"critical input" },
	{ EXC_RST,	"system reset" },
	{ EXC_MCHK,	"machine check" },
	{ EXC_DSI,	"data storage interrupt" },
	{ EXC_DSE,	"data segment exception" },
	{ EXC_ISI,	"instruction storage interrupt" },
	{ EXC_ISE,	"instruction segment exception" },
	{ EXC_EXI,	"external interrupt" },
	{ EXC_ALI,	"alignment" },
	{ EXC_PGM,	"program" },
	{ EXC_HEA,	"hypervisor emulation assistance" },
	{ EXC_FPU,	"floating-point unavailable" },
	{ EXC_APU,	"auxiliary proc unavailable" },
	{ EXC_DECR,	"decrementer" },
	{ EXC_FIT,	"fixed-interval timer" },
	{ EXC_WDOG,	"watchdog timer" },
	{ EXC_SC,	"system call" },
	{ EXC_TRC,	"trace" },
	{ EXC_FPA,	"floating-point assist" },
	{ EXC_DEBUG,	"debug" },
	{ EXC_PERF,	"performance monitoring" },
	{ EXC_VEC,	"altivec unavailable" },
	{ EXC_VSX,	"vsx unavailable" },
	{ EXC_FAC,	"facility unavailable" },
	{ EXC_ITMISS,	"instruction tlb miss" },
	{ EXC_DLMISS,	"data load tlb miss" },
	{ EXC_DSMISS,	"data store tlb miss" },
	{ EXC_BPT,	"instruction breakpoint" },
	{ EXC_SMI,	"system management" },
	{ EXC_VECAST_G4,	"altivec assist" },
	{ EXC_THRM,	"thermal management" },
	{ EXC_RUNMODETRC,	"run mode/trace" },
	{ EXC_SOFT_PATCH, "soft patch exception" },
	{ EXC_LAST,	NULL }
};

static int uprintf_signal;
SYSCTL_INT(_machdep, OID_AUTO, uprintf_signal, CTLFLAG_RWTUN,
    &uprintf_signal, 0,
    "Print debugging information on trap signal to ctty");

#define ESR_BITMASK							\
    "\20"								\
    "\040b0\037b1\036b2\035b3\034PIL\033PRR\032PTR\031FP"		\
    "\030ST\027b9\026DLK\025ILK\024b12\023b13\022BO\021PIE"		\
    "\020b16\017b17\016b18\015b19\014b20\013b21\012b22\011b23"		\
    "\010SPE\007EPID\006b26\005b27\004b28\003b29\002b30\001b31"
#define	MCSR_BITMASK							\
    "\20"								\
    "\040MCP\037ICERR\036DCERR\035TLBPERR\034L2MMU_MHIT\033b5\032b6\031b7"	\
    "\030b8\027b9\026b10\025NMI\024MAV\023MEA\022b14\021IF"		\
    "\020LD\017ST\016LDG\015b19\014b20\013b21\012b22\011b23"		\
    "\010b24\007b25\006b26\005b27\004b28\003b29\002TLBSYNC\001BSL2_ERR"
#define	MSSSR_BITMASK							\
    "\20"								\
    "\040b0\037b1\036b2\035b3\034b4\033b5\032b6\031b7"			\
    "\030b8\027b9\026b10\025b11\024b12\023L2TAG\022L2DAT\021L3TAG"	\
    "\020L3DAT\017APE\016DPE\015TEA\014b20\013b21\012b22\011b23"	\
    "\010b24\007b25\006b26\005b27\004b28\003b29\002b30\001b31"

static const char *
trapname(u_int vector)
{
	struct	powerpc_exception *pe;

	for (pe = powerpc_exceptions; pe->vector != EXC_LAST; pe++) {
		if (pe->vector == vector)
			return (pe->name);
	}

	return ("unknown");
}

static inline bool
frame_is_trap_inst(struct trapframe *frame)
{
#ifdef AIM
	return (frame->exc == EXC_PGM && frame->srr1 & EXC_PGM_TRAP);
#else
	return ((frame->cpu.booke.esr & ESR_PTR) != 0);
#endif
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
	register_t 	addr, fscr;

	VM_CNT_INC(v_trap);

#ifdef KDB
	if (kdb_active) {
		kdb_reenter();
		return;
	}
#endif

	td = curthread;
	p = td->td_proc;

	type = ucode = frame->exc;
	sig = 0;
	user = frame->srr1 & PSL_PR;
	addr = 0;

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
	if (dtrace_trap_func != NULL && (*dtrace_trap_func)(frame, type) != 0)
		return;
#endif

	if (user) {
		td->td_pticks = 0;
		td->td_frame = frame;
		addr = frame->srr0;
		if (td->td_cowgen != atomic_load_int(&p->p_cowgen))
			thread_cow_update(td);

		/* User Mode Traps */
		switch (type) {
		case EXC_RUNMODETRC:
		case EXC_TRC:
			frame->srr1 &= ~PSL_SE;
			sig = SIGTRAP;
			ucode = TRAP_TRACE;
			break;

#if defined(__powerpc64__) && defined(AIM)
		case EXC_DSE:
			addr = frame->dar;
			/* FALLTHROUGH */
		case EXC_ISE:
			/* DSE/ISE are automatically fatal with radix pmap. */
			if (radix_mmu ||
			    handle_user_slb_spill(&p->p_vmspace->vm_pmap,
			    addr) != 0){
				sig = SIGSEGV;
				ucode = SEGV_MAPERR;
			}
			break;
#endif
		case EXC_DSI:
			addr = frame->dar;
			/* FALLTHROUGH */
		case EXC_ISI:
			if (trap_pfault(frame, true, &sig, &ucode))
				sig = 0;
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

		case EXC_VSX:
			KASSERT((td->td_pcb->pcb_flags & PCB_VSX) != PCB_VSX,
			    ("VSX already enabled for thread"));
			if (!(td->td_pcb->pcb_flags & PCB_VEC))
				enable_vec(td);
			if (td->td_pcb->pcb_flags & PCB_FPU)
				save_fpu(td);
			td->td_pcb->pcb_flags |= PCB_VSX;
			enable_fpu(td);
			break;

		case EXC_FAC:
			fscr = mfspr(SPR_FSCR);
			switch (fscr & FSCR_IC_MASK) {
			case FSCR_IC_HTM:
				CTR0(KTR_TRAP,
				    "Hardware Transactional Memory subsystem disabled");
				sig = SIGILL;
				ucode =	ILL_ILLOPC;
				break;
			case FSCR_IC_DSCR:
				td->td_pcb->pcb_flags |= PCB_CFSCR | PCB_CDSCR;
				fscr |= FSCR_DSCR;
				mtspr(SPR_DSCR, 0);
				break;
			case FSCR_IC_EBB:
				td->td_pcb->pcb_flags |= PCB_CFSCR;
				fscr |= FSCR_EBB;
				mtspr(SPR_EBBHR, 0);
				mtspr(SPR_EBBRR, 0);
				mtspr(SPR_BESCR, 0);
				break;
			case FSCR_IC_TAR:
				td->td_pcb->pcb_flags |= PCB_CFSCR;
				fscr |= FSCR_TAR;
				mtspr(SPR_TAR, 0);
				break;
			case FSCR_IC_LM:
				td->td_pcb->pcb_flags |= PCB_CFSCR;
				fscr |= FSCR_LM;
				mtspr(SPR_LMRR, 0);
				mtspr(SPR_LMSER, 0);
				break;
			default:
				sig = SIGILL;
				ucode =	ILL_ILLOPC;
			}
			mtspr(SPR_FSCR, fscr & ~FSCR_IC_MASK);
			break;
		case EXC_HEA:
			sig = SIGILL;
			ucode =	ILL_ILLOPC;
			break;

		case EXC_VECAST_E:
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
			if (fix_unaligned(td, frame) != 0) {
				sig = SIGBUS;
				ucode = BUS_ADRALN;
				addr = frame->dar;
			}
			else
				frame->srr0 += 4;
			break;

		case EXC_DEBUG:	/* Single stepping */
			mtspr(SPR_DBSR, mfspr(SPR_DBSR));
			frame->srr1 &= ~PSL_DE;
			frame->cpu.booke.dbcr0 &= ~(DBCR0_IDM | DBCR0_IC);
			sig = SIGTRAP;
			ucode = TRAP_TRACE;
			break;

		case EXC_PGM:
			/* Identify the trap reason */
			if (frame_is_trap_inst(frame)) {
#ifdef KDTRACE_HOOKS
				inst = fuword32((const void *)frame->srr0);
				if (inst == 0x0FFFDDDD &&
				    dtrace_pid_probe_ptr != NULL) {
					(*dtrace_pid_probe_ptr)(frame);
					break;
				}
#endif
 				sig = SIGTRAP;
				ucode = TRAP_BRKPT;
				break;
			}

			if ((frame->srr1 & EXC_PGM_FPENABLED) &&
			     (td->td_pcb->pcb_flags & PCB_FPU))
				sig = SIGFPE;
			else
				sig = ppc_instr_emulate(frame, td);

			if (sig == SIGILL) {
				if (frame->srr1 & EXC_PGM_PRIV)
					ucode = ILL_PRVOPC;
				else if (frame->srr1 & EXC_PGM_ILLEGAL)
					ucode = ILL_ILLOPC;
			} else if (sig == SIGFPE) {
				ucode = get_fpu_exception(td);
			}

			break;

		case EXC_MCHK:
			sig = cpu_machine_check(td, frame, &ucode);
			printtrap(frame->exc, frame, 0, (frame->srr1 & PSL_PR));
			break;

#if defined(__powerpc64__) && defined(AIM)
		case EXC_SOFT_PATCH:
			/*
			 * Point to the instruction that generated the exception to execute it again,
			 * and normalize the register values.
			 */
			frame->srr0 -= 4;
			normalize_inputs();
			break;
#endif

		default:
			trap_fatal(frame);
		}
	} else {
		/* Kernel Mode Traps */

		KASSERT(cold || td->td_ucred != NULL,
		    ("kernel trap doesn't have ucred"));
		switch (type) {
		case EXC_PGM:
#ifdef KDTRACE_HOOKS
			if (frame_is_trap_inst(frame)) {
				if (*(uint32_t *)frame->srr0 == EXC_DTRACE) {
					if (dtrace_invop_jump_addr != NULL) {
						dtrace_invop_jump_addr(frame);
						return;
					}
				}
			}
#endif
#ifdef KDB
			if (db_trap_glue(frame))
				return;
#endif
			break;
#if defined(__powerpc64__) && defined(AIM)
		case EXC_DSE:
			/* DSE on radix mmu is automatically fatal. */
			if (radix_mmu)
				break;
			if (td->td_pcb->pcb_cpu.aim.usr_vsid != 0 &&
			    (frame->dar & SEGMENT_MASK) == USER_ADDR) {
				__asm __volatile ("slbmte %0, %1" ::
					"r"(td->td_pcb->pcb_cpu.aim.usr_vsid),
					"r"(USER_SLB_SLBE));
				return;
			}
			break;
#endif
		case EXC_DSI:
			if (trap_pfault(frame, false, NULL, NULL))
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
		ksiginfo_init_trap(&ksi);
		ksi.ksi_signo = sig;
		ksi.ksi_code = (int) ucode; /* XXX, not POSIX */
		ksi.ksi_addr = (void *)addr;
		ksi.ksi_trapno = type;
		if (uprintf_signal) {
			uprintf("pid %d comm %s: signal %d code %d type 0x%x "
				"addr 0x%lx r1 0x%lx srr0 0x%lx srr1 0x%lx\n",
			        p->p_pid, p->p_comm, sig, ucode, type,
				(u_long)addr, (u_long)frame->fixreg[1],
				(u_long)frame->srr0, (u_long)frame->srr1);
		}

		trapsignal(td, &ksi);
	}

	userret(td, frame);
}

static void
trap_fatal(struct trapframe *frame)
{
#ifdef KDB
	bool handled;
#endif

	printtrap(frame->exc, frame, 1, (frame->srr1 & PSL_PR));
#ifdef KDB
	if (debugger_on_trap) {
		kdb_why = KDB_WHY_TRAP;
		handled = kdb_trap(frame->exc, 0, frame);
		kdb_why = KDB_WHY_UNSET;
		if (handled)
			return;
	}
#endif
	panic("%s trap", trapname(frame->exc));
}

static void
cpu_printtrap(u_int vector, struct trapframe *frame, int isfatal, int user)
{
#ifdef AIM
	uint16_t ver;

	switch (vector) {
	case EXC_MCHK:
		ver = mfpvr() >> 16;
		if (MPC745X_P(ver))
			printf("    msssr0         = 0x%b\n",
			    (int)mfspr(SPR_MSSSR0), MSSSR_BITMASK);
	case EXC_DSE:
	case EXC_DSI:
	case EXC_DTMISS:
		printf("   dsisr           = 0x%lx\n",
		    (u_long)frame->cpu.aim.dsisr);
		break;
	}
#elif defined(BOOKE)
	vm_paddr_t pa;

	switch (vector) {
	case EXC_MCHK:
		pa = mfspr(SPR_MCARU);
		pa = (pa << 32) | (u_register_t)mfspr(SPR_MCAR);
		printf("   mcsr            = 0x%b\n",
		    (int)mfspr(SPR_MCSR), MCSR_BITMASK);
		printf("   mcar            = 0x%jx\n", (uintmax_t)pa);
	}
	printf("   esr             = 0x%b\n",
	    (int)frame->cpu.booke.esr, ESR_BITMASK);
#endif
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
	case EXC_DTMISS:
	case EXC_ALI:
	case EXC_MCHK:
		printf("   virtual address = 0x%" PRIxPTR "\n", frame->dar);
		break;
	case EXC_ISE:
	case EXC_ISI:
	case EXC_ITMISS:
		printf("   virtual address = 0x%" PRIxPTR "\n", frame->srr0);
		break;
	}
	cpu_printtrap(vector, frame, isfatal, user);
	printf("   srr0            = 0x%" PRIxPTR " (0x%" PRIxPTR ")\n",
	    frame->srr0, frame->srr0 - (register_t)(__startkernel - KERNBASE));
	printf("   srr1            = 0x%lx\n", (u_long)frame->srr1);
	printf("   current msr     = 0x%" PRIxPTR "\n", mfmsr());
	printf("   lr              = 0x%" PRIxPTR " (0x%" PRIxPTR ")\n",
	    frame->lr, frame->lr - (register_t)(__startkernel - KERNBASE));
	printf("   frame           = %p\n", frame);
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
	jmp_buf		*fb;

	td = curthread;
#if defined(__powerpc64__) || defined(BOOKE)
	uintptr_t dispatch = (uintptr_t)td->td_pcb->pcb_onfault;

	if (dispatch == 0)
		return (0);
	/* Short-circuit radix and Book-E paths. */
	switch (dispatch) {
		case COPYFAULT:
			frame->srr0 = (uintptr_t)copy_fault;
			return (1);
		case FUSUFAULT:
			frame->srr0 = (uintptr_t)fusufault;
			return (1);
		default:
			break;
	}
#endif
	fb = td->td_pcb->pcb_onfault;
	if (fb != NULL) {
		frame->srr0 = (*fb)->_jb[FAULTBUF_LR];
		frame->fixreg[1] = (*fb)->_jb[FAULTBUF_R1];
		frame->fixreg[2] = (*fb)->_jb[FAULTBUF_R2];
		frame->fixreg[3] = 1;
		frame->cr = (*fb)->_jb[FAULTBUF_CR];
		bcopy(&(*fb)->_jb[FAULTBUF_R14], &frame->fixreg[14],
		    18 * sizeof(register_t));
		td->td_pcb->pcb_onfault = NULL; /* Returns twice, not thrice */
		return (1);
	}
	return (0);
}

int
cpu_fetch_syscall_args(struct thread *td)
{
	struct proc *p;
	struct trapframe *frame;
	struct syscall_args *sa;
	caddr_t	params;
	size_t argsz;
	int error, n, narg, i;

	p = td->td_proc;
	frame = td->td_frame;
	sa = &td->td_sa;

	sa->code = frame->fixreg[0];
	sa->original_code = sa->code;
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

	if (sa->code >= p->p_sysent->sv_size)
		sa->callp = &p->p_sysent->sv_table[0];
	else
		sa->callp = &p->p_sysent->sv_table[sa->code];

	narg = sa->callp->sy_narg;

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

	if (narg > n)
		error = copyin(MOREARGS(frame->fixreg[1]), sa->args + n,
			       (narg - n) * argsz);
	else
		error = 0;

#ifdef __powerpc64__
	if (SV_PROC_FLAG(p, SV_ILP32) && narg > n) {
		/* Expand the size of arguments copied from the stack */

		for (i = narg; i >= n; i--)
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

	td = curthread;
	td->td_frame = frame;

#if defined(__powerpc64__) && defined(AIM)
	/*
	 * Speculatively restore last user SLB segment, which we know is
	 * invalid already, since we are likely to do copyin()/copyout().
	 */
	if (td->td_pcb->pcb_cpu.aim.usr_vsid != 0)
		__asm __volatile ("slbmte %0, %1; isync" ::
		    "r"(td->td_pcb->pcb_cpu.aim.usr_vsid), "r"(USER_SLB_SLBE));
#endif

	syscallenter(td);
	syscallret(td);
}

static bool
trap_pfault(struct trapframe *frame, bool user, int *signo, int *ucode)
{
	vm_offset_t	eva;
	struct		thread *td;
	struct		proc *p;
	vm_map_t	map;
	vm_prot_t	ftype;
	int		rv, is_user;

	td = curthread;
	p = td->td_proc;
	if (frame->exc == EXC_ISI) {
		eva = frame->srr0;
		ftype = VM_PROT_EXECUTE;
		if (frame->srr1 & SRR1_ISI_PFAULT)
			ftype |= VM_PROT_READ;
	} else {
		eva = frame->dar;
#ifdef BOOKE
		if (frame->cpu.booke.esr & ESR_ST)
#else
		if (frame->cpu.aim.dsisr & DSISR_STORE)
#endif
			ftype = VM_PROT_WRITE;
		else
			ftype = VM_PROT_READ;
	}
#if defined(__powerpc64__) && defined(AIM)
	if (radix_mmu && pmap_nofault(&p->p_vmspace->vm_pmap, eva, ftype) == 0)
		return (true);
#endif

	if (__predict_false((td->td_pflags & TDP_NOFAULTING) == 0)) {
		/*
		 * If we get a page fault while in a critical section, then
		 * it is most likely a fatal kernel page fault.  The kernel
		 * is already going to panic trying to get a sleep lock to
		 * do the VM lookup, so just consider it a fatal trap so the
		 * kernel can print out a useful trap message and even get
		 * to the debugger.
		 *
		 * If we get a page fault while holding a non-sleepable
		 * lock, then it is most likely a fatal kernel page fault.
		 * If WITNESS is enabled, then it's going to whine about
		 * bogus LORs with various VM locks, so just skip to the
		 * fatal trap handling directly.
		 */
		if (td->td_critnest != 0 ||
			WITNESS_CHECK(WARN_SLEEPOK | WARN_GIANTOK, NULL,
				"Kernel page fault") != 0) {
			trap_fatal(frame);
			return (false);
		}
	}
	if (user) {
		KASSERT(p->p_vmspace != NULL, ("trap_pfault: vmspace  NULL"));
		map = &p->p_vmspace->vm_map;
	} else {
		rv = pmap_decode_kernel_ptr(eva, &is_user, &eva);
		if (rv != 0)
			return (false);

		if (is_user)
			map = &p->p_vmspace->vm_map;
		else
			map = kernel_map;
	}

	/* Fault in the page. */
	rv = vm_fault_trap(map, eva, ftype, VM_FAULT_NORMAL, signo, ucode);
	/*
	 * XXXDTRACE: add dtrace_doubletrap_func here?
	 */

	if (rv == KERN_SUCCESS)
		return (true);

	if (!user && handle_onfault(frame))
		return (true);

	return (false);
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
#ifdef BOOKE
	uint32_t	inst;
#endif
	int		indicator, reg;
	double		*fpr;

#ifdef __SPE__
	indicator = (frame->cpu.booke.esr & (ESR_ST|ESR_SPE));
	if (indicator & ESR_SPE) {
		if (copyin((void *)frame->srr0, &inst, sizeof(inst)) != 0)
			return (-1);
		reg = EXC_ALI_INST_RST(inst);
		fpr = (double *)td->td_pcb->pcb_vec.vr[reg];
		fputhread = PCPU_GET(vecthread);

		/* Juggle the SPE to ensure that we've initialized
		 * the registers, and that their current state is in
		 * the PCB.
		 */
		if (fputhread != td) {
			if (fputhread)
				save_vec(fputhread);
			enable_vec(td);
		}
		save_vec(td);

		if (!(indicator & ESR_ST)) {
			if (copyin((void *)frame->dar, fpr,
			    sizeof(double)) != 0)
				return (-1);
			frame->fixreg[reg] = td->td_pcb->pcb_vec.vr[reg][1];
			enable_vec(td);
		} else {
			td->td_pcb->pcb_vec.vr[reg][1] = frame->fixreg[reg];
			if (copyout(fpr, (void *)frame->dar,
			    sizeof(double)) != 0)
				return (-1);
		}
		return (0);
	}
#else
#ifdef BOOKE
	indicator = (frame->cpu.booke.esr & ESR_ST) ? EXC_ALI_STFD : EXC_ALI_LFD;
#else
	indicator = EXC_ALI_OPCODE_INDICATOR(frame->cpu.aim.dsisr);
#endif

	switch (indicator) {
	case EXC_ALI_LFD:
	case EXC_ALI_STFD:
#ifdef BOOKE
		if (copyin((void *)frame->srr0, &inst, sizeof(inst)) != 0)
			return (-1);
		reg = EXC_ALI_INST_RST(inst);
#else
		reg = EXC_ALI_RST(frame->cpu.aim.dsisr);
#endif
		fpr = &td->td_pcb->pcb_fpu.fpr[reg].fpr;
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
			if (copyin((void *)frame->dar, fpr,
			    sizeof(double)) != 0)
				return (-1);
			enable_fpu(td);
		} else {
			if (copyout(fpr, (void *)frame->dar,
			    sizeof(double)) != 0)
				return (-1);
		}
		return (0);
		break;
	}
#endif

	return (-1);
}

#if defined(__powerpc64__) && defined(AIM)
#define MSKNSHL(x, m, n) "(((" #x ") & " #m ") << " #n ")"
#define MSKNSHR(x, m, n) "(((" #x ") & " #m ") >> " #n ")"

/* xvcpsgndp instruction, built in opcode format.
 * This can be changed to use mnemonic after a toolchain update.
 */
#define XVCPSGNDP(xt, xa, xb) \
	__asm __volatile(".long (" \
		MSKNSHL(60, 0x3f, 26) " | " \
		MSKNSHL(xt, 0x1f, 21) " | " \
		MSKNSHL(xa, 0x1f, 16) " | " \
		MSKNSHL(xb, 0x1f, 11) " | " \
		MSKNSHL(240, 0xff, 3) " | " \
		MSKNSHR(xa,  0x20, 3) " | " \
		MSKNSHR(xa,  0x20, 4) " | " \
		MSKNSHR(xa,  0x20, 5) ")")

/* Macros to normalize 1 or 10 VSX registers */
#define NORM(x)	XVCPSGNDP(x, x, x)
#define NORM10(x) \
	NORM(x ## 0); NORM(x ## 1); NORM(x ## 2); NORM(x ## 3); NORM(x ## 4); \
	NORM(x ## 5); NORM(x ## 6); NORM(x ## 7); NORM(x ## 8); NORM(x ## 9)

static void
normalize_inputs(void)
{
	register_t msr;

	/* enable VSX */
	msr = mfmsr();
	mtmsr(msr | PSL_VSX);

	NORM(0);   NORM(1);   NORM(2);   NORM(3);   NORM(4);
	NORM(5);   NORM(6);   NORM(7);   NORM(8);   NORM(9);
	NORM10(1); NORM10(2); NORM10(3); NORM10(4); NORM10(5);
	NORM(60);  NORM(61);  NORM(62);  NORM(63);

	/* restore MSR */
	mtmsr(msr);
}
#endif

#ifdef KDB
int
db_trap_glue(struct trapframe *frame)
{

	if (!(frame->srr1 & PSL_PR)
	    && (frame->exc == EXC_TRC || frame->exc == EXC_RUNMODETRC
	    	|| frame_is_trap_inst(frame)
		|| frame->exc == EXC_BPT
		|| frame->exc == EXC_DEBUG
		|| frame->exc == EXC_DSI)) {
		int type = frame->exc;

		/* Ignore DTrace traps. */
		if (*(uint32_t *)frame->srr0 == EXC_DTRACE)
			return (0);
		if (frame_is_trap_inst(frame)) {
			type = T_BREAKPOINT;
		}
		return (kdb_trap(type, 0, frame));
	}

	return (0);
}
#endif
