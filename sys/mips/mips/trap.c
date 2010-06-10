/*	$OpenBSD: trap.c,v 1.19 1998/09/30 12:40:41 pefo Exp $	*/
/* tracked to 1.23 */
/*-
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * from: Utah Hdr: trap.c 1.32 91/04/06
 *
 *	from: @(#)trap.c	8.5 (Berkeley) 1/11/94
 *	JNPR: trap.c,v 1.13.2.2 2007/08/29 10:03:49 girish
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"
#include "opt_global.h"
#include "opt_ktrace.h"

#define	NO_REG_DEFS	1	/* Prevent asm.h from including regdef.h */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysent.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/syscall.h>
#include <sys/lock.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_param.h>
#include <sys/vmmeter.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/pioctl.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/bus.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif
#include <net/netisr.h>

#include <machine/trap.h>
#include <machine/cpu.h>
#include <machine/pte.h>
#include <machine/pmap.h>
#include <machine/md_var.h>
#include <machine/mips_opcode.h>
#include <machine/frame.h>
#include <machine/regnum.h>
#include <machine/asm.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_sym.h>
#include <ddb/ddb.h>
#include <sys/kdb.h>
#endif

#include <sys/cdefs.h>
#include <sys/syslog.h>


#ifdef TRAP_DEBUG
int trap_debug = 1;
#endif

static void log_illegal_instruction(const char *, struct trapframe *);
static void log_bad_page_fault(char *, struct trapframe *, int);
static void log_frame_dump(struct trapframe *frame);
static void get_mapping_info(vm_offset_t, pd_entry_t **, pt_entry_t **);

#ifdef TRAP_DEBUG
static void trap_frame_dump(struct trapframe *frame);
#endif

void (*machExceptionTable[]) (void)= {
/*
 * The kernel exception handlers.
 */
	MipsKernIntr,		/* external interrupt */
	MipsKernGenException,	/* TLB modification */
	MipsTLBInvalidException,/* TLB miss (load or instr. fetch) */
	MipsTLBInvalidException,/* TLB miss (store) */
	MipsKernGenException,	/* address error (load or I-fetch) */
	MipsKernGenException,	/* address error (store) */
	MipsKernGenException,	/* bus error (I-fetch) */
	MipsKernGenException,	/* bus error (load or store) */
	MipsKernGenException,	/* system call */
	MipsKernGenException,	/* breakpoint */
	MipsKernGenException,	/* reserved instruction */
	MipsKernGenException,	/* coprocessor unusable */
	MipsKernGenException,	/* arithmetic overflow */
	MipsKernGenException,	/* trap exception */
	MipsKernGenException,	/* virtual coherence exception inst */
	MipsKernGenException,	/* floating point exception */
	MipsKernGenException,	/* reserved */
	MipsKernGenException,	/* reserved */
	MipsKernGenException,	/* reserved */
	MipsKernGenException,	/* reserved */
	MipsKernGenException,	/* reserved */
	MipsKernGenException,	/* reserved */
	MipsKernGenException,	/* reserved */
	MipsKernGenException,	/* watch exception */
	MipsKernGenException,	/* reserved */
	MipsKernGenException,	/* reserved */
	MipsKernGenException,	/* reserved */
	MipsKernGenException,	/* reserved */
	MipsKernGenException,	/* reserved */
	MipsKernGenException,	/* reserved */
	MipsKernGenException,	/* reserved */
	MipsKernGenException,	/* virtual coherence exception data */
/*
 * The user exception handlers.
 */
	MipsUserIntr,		/* 0 */
	MipsUserGenException,	/* 1 */
	MipsTLBInvalidException,/* 2 */
	MipsTLBInvalidException,/* 3 */
	MipsUserGenException,	/* 4 */
	MipsUserGenException,	/* 5 */
	MipsUserGenException,	/* 6 */
	MipsUserGenException,	/* 7 */
	MipsUserGenException,	/* 8 */
	MipsUserGenException,	/* 9 */
	MipsUserGenException,	/* 10 */
	MipsUserGenException,	/* 11 */
	MipsUserGenException,	/* 12 */
	MipsUserGenException,	/* 13 */
	MipsUserGenException,	/* 14 */
	MipsUserGenException,	/* 15 */
	MipsUserGenException,	/* 16 */
	MipsUserGenException,	/* 17 */
	MipsUserGenException,	/* 18 */
	MipsUserGenException,	/* 19 */
	MipsUserGenException,	/* 20 */
	MipsUserGenException,	/* 21 */
	MipsUserGenException,	/* 22 */
	MipsUserGenException,	/* 23 */
	MipsUserGenException,	/* 24 */
	MipsUserGenException,	/* 25 */
	MipsUserGenException,	/* 26 */
	MipsUserGenException,	/* 27 */
	MipsUserGenException,	/* 28 */
	MipsUserGenException,	/* 29 */
	MipsUserGenException,	/* 20 */
	MipsUserGenException,	/* 31 */
};

char *trap_type[] = {
	"external interrupt",
	"TLB modification",
	"TLB miss (load or instr. fetch)",
	"TLB miss (store)",
	"address error (load or I-fetch)",
	"address error (store)",
	"bus error (I-fetch)",
	"bus error (load or store)",
	"system call",
	"breakpoint",
	"reserved instruction",
	"coprocessor unusable",
	"arithmetic overflow",
	"trap",
	"virtual coherency instruction",
	"floating point",
	"reserved 16",
	"reserved 17",
	"reserved 18",
	"reserved 19",
	"reserved 20",
	"reserved 21",
	"reserved 22",
	"watch",
	"reserved 24",
	"reserved 25",
	"reserved 26",
	"reserved 27",
	"reserved 28",
	"reserved 29",
	"reserved 30",
	"virtual coherency data",
};

#if !defined(SMP) && (defined(DDB) || defined(DEBUG))
struct trapdebug trapdebug[TRAPSIZE], *trp = trapdebug;
#endif

#if defined(DDB) || defined(DEBUG)
void stacktrace(struct trapframe *);
void logstacktrace(struct trapframe *);
#endif

#define	KERNLAND(x)	((vm_offset_t)(x) >= VM_MIN_KERNEL_ADDRESS && (vm_offset_t)(x) < VM_MAX_KERNEL_ADDRESS)
#define	DELAYBRANCH(x)	((int)(x) < 0)

/*
 * MIPS load/store access type
 */
enum {
	MIPS_LHU_ACCESS = 1,
	MIPS_LH_ACCESS,
	MIPS_LWU_ACCESS,
	MIPS_LW_ACCESS,
	MIPS_LD_ACCESS,
	MIPS_SH_ACCESS,
	MIPS_SW_ACCESS,
	MIPS_SD_ACCESS
};

char *access_name[] = {
	"Load Halfword Unsigned",
	"Load Halfword",
	"Load Word Unsigned",
	"Load Word",
	"Load Doubleword",
	"Store Halfword",
	"Store Word",
	"Store Doubleword"
};


static int allow_unaligned_acc = 1;

SYSCTL_INT(_vm, OID_AUTO, allow_unaligned_acc, CTLFLAG_RW,
    &allow_unaligned_acc, 0, "Allow unaligned accesses");

static int emulate_unaligned_access(struct trapframe *frame);

extern void fswintrberr(void); /* XXX */

/*
 * Handle an exception.
 * Called from MipsKernGenException() or MipsUserGenException()
 * when a processor trap occurs.
 * In the case of a kernel trap, we return the pc where to resume if
 * p->p_addr->u_pcb.pcb_onfault is set, otherwise, return old pc.
 */
register_t
trap(struct trapframe *trapframe)
{
	int type, usermode;
	int i = 0;
	unsigned ucode = 0;
	struct thread *td = curthread;
	struct proc *p = curproc;
	vm_prot_t ftype;
	pt_entry_t *pte;
	pmap_t pmap;
	int access_type;
	ksiginfo_t ksi;
	char *msg = NULL;
	intptr_t addr = 0;
	register_t pc;

	trapdebug_enter(trapframe, 0);
	
	type = (trapframe->cause & CR_EXC_CODE) >> CR_EXC_CODE_SHIFT;
	if (TRAPF_USERMODE(trapframe)) {
		type |= T_USER;
		usermode = 1;
	} else {
		usermode = 0;
	}

	/*
	 * Enable hardware interrupts if they were on before the trap. If it
	 * was off disable all so we don't accidently enable it when doing a
	 * return to userland.
	 */
	if (trapframe->sr & SR_INT_ENAB) {
		set_intr_mask(~(trapframe->sr & ALL_INT_MASK));
		intr_enable();
	} else {
		intr_disable();
	}

#ifdef TRAP_DEBUG
	if (trap_debug) {
		static vm_offset_t last_badvaddr = 0;
		static vm_offset_t this_badvaddr = 0;
		static int count = 0;
		u_int32_t pid;

		printf("trap type %x (%s - ", type,
		    trap_type[type & (~T_USER)]);

		if (type & T_USER)
			printf("user mode)\n");
		else
			printf("kernel mode)\n");

#ifdef SMP
		printf("cpuid = %d\n", PCPU_GET(cpuid));
#endif
		MachTLBGetPID(pid);
		printf("badaddr = %#jx, pc = %#jx, ra = %#jx, sp = %#jx, sr = %jx, pid = %d, ASID = %u\n",
		    (intmax_t)trapframe->badvaddr, (intmax_t)trapframe->pc, (intmax_t)trapframe->ra,
		    (intmax_t)trapframe->sp, (intmax_t)trapframe->sr,
		    (curproc ? curproc->p_pid : -1), pid);

		switch (type & ~T_USER) {
		case T_TLB_MOD:
		case T_TLB_LD_MISS:
		case T_TLB_ST_MISS:
		case T_ADDR_ERR_LD:
		case T_ADDR_ERR_ST:
			this_badvaddr = trapframe->badvaddr;
			break;
		case T_SYSCALL:
			this_badvaddr = trapframe->ra;
			break;
		default:
			this_badvaddr = trapframe->pc;
			break;
		}
		if ((last_badvaddr == this_badvaddr) &&
		    ((type & ~T_USER) != T_SYSCALL)) {
			if (++count == 3) {
				trap_frame_dump(trapframe);
				panic("too many faults at %p\n", (void *)last_badvaddr);
			}
		} else {
			last_badvaddr = this_badvaddr;
			count = 0;
		}
	}
#endif
	switch (type) {
	case T_MCHECK:
#ifdef DDB
		kdb_trap(type, 0, trapframe);
#endif
		panic("MCHECK\n");
		break;
	case T_TLB_MOD:
		/* check for kernel address */
		if (KERNLAND(trapframe->badvaddr)) {
			vm_offset_t pa;

			PMAP_LOCK(kernel_pmap);
			pte = pmap_pte(kernel_pmap, trapframe->badvaddr);
			if (pte == NULL)
				panic("trap: ktlbmod: can't find PTE");
#ifdef SMP
			/* It is possible that some other CPU changed m-bit */
			if (!mips_pg_v(*pte) || (*pte & mips_pg_m_bit())) {
				pmap_update_page(kernel_pmap,
				    trapframe->badvaddr, *pte);
				PMAP_UNLOCK(kernel_pmap);
				return (trapframe->pc);
			}
#else
			if (!mips_pg_v(*pte) || (*pte & mips_pg_m_bit()))
				panic("trap: ktlbmod: invalid pte");
#endif
			if (*pte & mips_pg_ro_bit()) {
				/* write to read only page in the kernel */
				ftype = VM_PROT_WRITE;
				PMAP_UNLOCK(kernel_pmap);
				goto kernel_fault;
			}
			*pte |= mips_pg_m_bit();
			pmap_update_page(kernel_pmap, trapframe->badvaddr, *pte);
			pa = mips_tlbpfn_to_paddr(*pte);
			if (!page_is_managed(pa))
				panic("trap: ktlbmod: unmanaged page");
			pmap_set_modified(pa);
			PMAP_UNLOCK(kernel_pmap);
			return (trapframe->pc);
		}
		/* FALLTHROUGH */

	case T_TLB_MOD + T_USER:
		{
			vm_offset_t pa;

			pmap = &p->p_vmspace->vm_pmap;

			PMAP_LOCK(pmap);
			pte = pmap_pte(pmap, trapframe->badvaddr);
			if (pte == NULL)
				panic("trap: utlbmod: can't find PTE");
#ifdef SMP
			/* It is possible that some other CPU changed m-bit */
			if (!mips_pg_v(*pte) || (*pte & mips_pg_m_bit())) {
				pmap_update_page(pmap, trapframe->badvaddr, *pte);
				PMAP_UNLOCK(pmap);
				goto out;
			}
#else
			if (!mips_pg_v(*pte) || (*pte & mips_pg_m_bit()))
				panic("trap: utlbmod: invalid pte");
#endif

			if (*pte & mips_pg_ro_bit()) {
				/* write to read only page */
				ftype = VM_PROT_WRITE;
				PMAP_UNLOCK(pmap);
				goto dofault;
			}
			*pte |= mips_pg_m_bit();
			pmap_update_page(pmap, trapframe->badvaddr, *pte);
			pa = mips_tlbpfn_to_paddr(*pte);
			if (!page_is_managed(pa))
				panic("trap: utlbmod: unmanaged page");
			pmap_set_modified(pa);

			PMAP_UNLOCK(pmap);
			if (!usermode) {
				return (trapframe->pc);
			}
			goto out;
		}

	case T_TLB_LD_MISS:
	case T_TLB_ST_MISS:
		ftype = (type == T_TLB_ST_MISS) ? VM_PROT_WRITE : VM_PROT_READ;
		/* check for kernel address */
		if (KERNLAND(trapframe->badvaddr)) {
			vm_offset_t va;
			int rv;

	kernel_fault:
			va = trunc_page((vm_offset_t)trapframe->badvaddr);
			rv = vm_fault(kernel_map, va, ftype, VM_FAULT_NORMAL);
			if (rv == KERN_SUCCESS)
				return (trapframe->pc);
			if (td->td_pcb->pcb_onfault != NULL) {
				pc = (register_t)(intptr_t)td->td_pcb->pcb_onfault;
				td->td_pcb->pcb_onfault = NULL;
				return (pc);
			}
			goto err;
		}

                /*
		 * It is an error for the kernel to access user space except
		 * through the copyin/copyout routines.
		 */
		if (td->td_pcb->pcb_onfault == NULL)
			goto err;

		/* check for fuswintr() or suswintr() getting a page fault */
		/* XXX There must be a nicer way to do this.  */
		if (td->td_pcb->pcb_onfault == fswintrberr) {
			pc = (register_t)(intptr_t)td->td_pcb->pcb_onfault;
			td->td_pcb->pcb_onfault = NULL;
			return (pc);
		}

		goto dofault;

	case T_TLB_LD_MISS + T_USER:
		ftype = VM_PROT_READ;
		goto dofault;

	case T_TLB_ST_MISS + T_USER:
		ftype = VM_PROT_WRITE;
dofault:
		{
			vm_offset_t va;
			struct vmspace *vm;
			vm_map_t map;
			int rv = 0;

			vm = p->p_vmspace;
			map = &vm->vm_map;
			va = trunc_page((vm_offset_t)trapframe->badvaddr);
			if (KERNLAND(trapframe->badvaddr)) {
				/*
				 * Don't allow user-mode faults in kernel
				 * address space.
				 */
				goto nogo;
			}

			/*
			 * Keep swapout from messing with us during this
			 * critical time.
			 */
			PROC_LOCK(p);
			++p->p_lock;
			PROC_UNLOCK(p);

			rv = vm_fault(map, va, ftype, VM_FAULT_NORMAL);

			PROC_LOCK(p);
			--p->p_lock;
			PROC_UNLOCK(p);
#ifdef VMFAULT_TRACE
			printf("vm_fault(%p (pmap %p), %p (%p), %x, %d) -> %x at pc %p\n",
			    map, &vm->vm_pmap, (void *)va, (void *)(intptr_t)trapframe->badvaddr,
			    ftype, VM_FAULT_NORMAL, rv, (void *)(intptr_t)trapframe->pc);
#endif

			if (rv == KERN_SUCCESS) {
				if (!usermode) {
					return (trapframe->pc);
				}
				goto out;
			}
	nogo:
			if (!usermode) {
				if (td->td_pcb->pcb_onfault != NULL) {
					pc = (register_t)(intptr_t)td->td_pcb->pcb_onfault;
					td->td_pcb->pcb_onfault = NULL;
					return (pc);
				}
				goto err;
			}
			ucode = ftype;
			i = ((rv == KERN_PROTECTION_FAILURE) ? SIGBUS : SIGSEGV);
			addr = trapframe->pc;

			msg = "BAD_PAGE_FAULT";
			log_bad_page_fault(msg, trapframe, type);

			break;
		}

	case T_ADDR_ERR_LD + T_USER:	/* misaligned or kseg access */
	case T_ADDR_ERR_ST + T_USER:	/* misaligned or kseg access */
		if (allow_unaligned_acc) {
			int mode;

			if (type == (T_ADDR_ERR_LD + T_USER))
				mode = VM_PROT_READ;
			else
				mode = VM_PROT_WRITE;

			/*
			 * ADDR_ERR faults have higher priority than TLB
			 * Miss faults.  Therefore, it is necessary to
			 * verify that the faulting address is a valid
			 * virtual address within the process' address space
			 * before trying to emulate the unaligned access.
			 */
			if (useracc((caddr_t)
			    (((vm_offset_t)trapframe->badvaddr) &
			    ~(sizeof(int) - 1)), sizeof(int) * 2, mode)) {
				access_type = emulate_unaligned_access(
				    trapframe);
				if (access_type != 0)
					goto out;
			}
		}
		msg = "ADDRESS_ERR";

		/* FALL THROUGH */

	case T_BUS_ERR_IFETCH + T_USER:	/* BERR asserted to cpu */
	case T_BUS_ERR_LD_ST + T_USER:	/* BERR asserted to cpu */
		ucode = 0;	/* XXX should be VM_PROT_something */
		i = SIGBUS;
		addr = trapframe->pc;
		if (!msg)
			msg = "BUS_ERR";
		log_bad_page_fault(msg, trapframe, type);
		break;

	case T_SYSCALL + T_USER:
		{
			struct trapframe *locr0 = td->td_frame;
			struct sysent *callp;
			unsigned int code;
			int nargs, nsaved;
			register_t args[8];

			bzero(args, sizeof args);

			/*
			 * note: PCPU_LAZY_INC() can only be used if we can
			 * afford occassional inaccuracy in the count.
			 */
			PCPU_LAZY_INC(cnt.v_syscall);
			if (td->td_ucred != p->p_ucred)
				cred_update_thread(td);
#ifdef KSE
			if (p->p_flag & P_SA)
				thread_user_enter(td);
#endif
			/* compute next PC after syscall instruction */
			td->td_pcb->pcb_tpc = trapframe->pc;	/* Remember if restart */
			if (DELAYBRANCH(trapframe->cause)) {	/* Check BD bit */
				locr0->pc = MipsEmulateBranch(locr0, trapframe->pc, 0,
				    0);
			} else {
				locr0->pc += sizeof(int);
			}
			code = locr0->v0;

			switch (code) {
			case SYS_syscall:
				/*
				 * Code is first argument, followed by
				 * actual args.
				 */
				code = locr0->a0;
				args[0] = locr0->a1;
				args[1] = locr0->a2;
				args[2] = locr0->a3;
				nsaved = 3;
				break;

			case SYS___syscall:
				/*
				 * Like syscall, but code is a quad, so as
				 * to maintain quad alignment for the rest
				 * of the arguments.
				 */
				if (_QUAD_LOWWORD == 0) {
					code = locr0->a0;
				} else {
					code = locr0->a1;
				}
				args[0] = locr0->a2;
				args[1] = locr0->a3;
				nsaved = 2;
				break;

			default:
				args[0] = locr0->a0;
				args[1] = locr0->a1;
				args[2] = locr0->a2;
				args[3] = locr0->a3;
				nsaved = 4;
			}
#ifdef TRAP_DEBUG
			printf("SYSCALL #%d pid:%u\n", code, p->p_pid);
#endif

			if (p->p_sysent->sv_mask)
				code &= p->p_sysent->sv_mask;

			if (code >= p->p_sysent->sv_size)
				callp = &p->p_sysent->sv_table[0];
			else
				callp = &p->p_sysent->sv_table[code];

			nargs = callp->sy_narg;

			if (nargs > nsaved) {
				i = copyin((caddr_t)(intptr_t)(locr0->sp +
				    4 * sizeof(register_t)), (caddr_t)&args[nsaved],
				    (u_int)(nargs - nsaved) * sizeof(register_t));
				if (i) {
					locr0->v0 = i;
					locr0->a3 = 1;
#ifdef KTRACE
					if (KTRPOINT(td, KTR_SYSCALL))
						ktrsyscall(code, nargs, args);
#endif
					goto done;
				}
			}
#ifdef KTRACE
			if (KTRPOINT(td, KTR_SYSCALL))
				ktrsyscall(code, nargs, args);
#endif
			td->td_retval[0] = 0;
			td->td_retval[1] = locr0->v1;

#if !defined(SMP) && (defined(DDB) || defined(DEBUG))
			if (trp == trapdebug)
				trapdebug[TRAPSIZE - 1].code = code;
			else
				trp[-1].code = code;
#endif
			STOPEVENT(p, S_SCE, nargs);

			PTRACESTOP_SC(p, td, S_PT_SCE);
			i = (*callp->sy_call) (td, args);
#if 0
			/*
			 * Reinitialize proc pointer `p' as it may be
			 * different if this is a child returning from fork
			 * syscall.
			 */
			td = curthread;
			locr0 = td->td_frame;
#endif
			trapdebug_enter(locr0, -code);
			cpu_set_syscall_retval(td, i);

			/*
			 * The sync'ing of I & D caches for SYS_ptrace() is
			 * done by procfs_domem() through procfs_rwmem()
			 * instead of being done here under a special check
			 * for SYS_ptrace().
			 */
	done:
			/*
			 * Check for misbehavior.
			 */
			WITNESS_WARN(WARN_PANIC, NULL, "System call %s returning",
			    (code >= 0 && code < SYS_MAXSYSCALL) ?
			    syscallnames[code] : "???");
			KASSERT(td->td_critnest == 0,
			    ("System call %s returning in a critical section",
			    (code >= 0 && code < SYS_MAXSYSCALL) ?
			    syscallnames[code] : "???"));
			KASSERT(td->td_locks == 0,
			    ("System call %s returning with %d locks held",
			    (code >= 0 && code < SYS_MAXSYSCALL) ?
			    syscallnames[code] : "???",
			    td->td_locks));
			userret(td, trapframe);
#ifdef KTRACE
			if (KTRPOINT(td, KTR_SYSRET))
				ktrsysret(code, i, td->td_retval[0]);
#endif
			/*
			 * This works because errno is findable through the
			 * register set.  If we ever support an emulation
			 * where this is not the case, this code will need
			 * to be revisited.
			 */
			STOPEVENT(p, S_SCX, code);

			PTRACESTOP_SC(p, td, S_PT_SCX);

			mtx_assert(&Giant, MA_NOTOWNED);
			return (trapframe->pc);
		}

#ifdef DDB
	case T_BREAK:
		kdb_trap(type, 0, trapframe);
		return (trapframe->pc);
#endif

	case T_BREAK + T_USER:
		{
			intptr_t va;
			uint32_t instr;

			/* compute address of break instruction */
			va = trapframe->pc;
			if (DELAYBRANCH(trapframe->cause))
				va += sizeof(int);

			/* read break instruction */
			instr = fuword((caddr_t)va);
#if 0
			printf("trap: %s (%d) breakpoint %x at %x: (adr %x ins %x)\n",
			    p->p_comm, p->p_pid, instr, trapframe->pc,
			    p->p_md.md_ss_addr, p->p_md.md_ss_instr);	/* XXX */
#endif
			if (td->td_md.md_ss_addr != va || instr != BREAK_SSTEP) {
				i = SIGTRAP;
				addr = trapframe->pc;
				break;
			}
			/*
			 * The restoration of the original instruction and
			 * the clearing of the berakpoint will be done later
			 * by the call to ptrace_clear_single_step() in
			 * issignal() when SIGTRAP is processed.
			 */
			addr = trapframe->pc;
			i = SIGTRAP;
			break;
		}

	case T_IWATCH + T_USER:
	case T_DWATCH + T_USER:
		{
			intptr_t va;

			/* compute address of trapped instruction */
			va = trapframe->pc;
			if (DELAYBRANCH(trapframe->cause))
				va += sizeof(int);
			printf("watch exception @ %p\n", (void *)va);
			i = SIGTRAP;
			addr = va;
			break;
		}

	case T_TRAP + T_USER:
		{
			intptr_t va;
			uint32_t instr;
			struct trapframe *locr0 = td->td_frame;

			/* compute address of trap instruction */
			va = trapframe->pc;
			if (DELAYBRANCH(trapframe->cause))
				va += sizeof(int);
			/* read break instruction */
			instr = fuword((caddr_t)va);

			if (DELAYBRANCH(trapframe->cause)) {	/* Check BD bit */
				locr0->pc = MipsEmulateBranch(locr0, trapframe->pc, 0,
				    0);
			} else {
				locr0->pc += sizeof(int);
			}
			addr = va;
			i = SIGEMT;	/* Stuff it with something for now */
			break;
		}

	case T_RES_INST + T_USER:
		log_illegal_instruction("RES_INST", trapframe);
		i = SIGILL;
		addr = trapframe->pc;
		break;
	case T_C2E:
	case T_C2E + T_USER:
		goto err;
		break;
	case T_COP_UNUSABLE:
		goto err;
		break;
	case T_COP_UNUSABLE + T_USER:
#if !defined(CPU_HAVEFPU)
		/* FP (COP1) instruction */
		if ((trapframe->cause & CR_COP_ERR) == 0x10000000) {
			log_illegal_instruction("COP1_UNUSABLE", trapframe);
			i = SIGILL;
			break;
		}
#endif
		if ((trapframe->cause & CR_COP_ERR) != 0x10000000) {
			log_illegal_instruction("COPn_UNUSABLE", trapframe);
			i = SIGILL;	/* only FPU instructions allowed */
			break;
		}
		addr = trapframe->pc;
		MipsSwitchFPState(PCPU_GET(fpcurthread), td->td_frame);
		PCPU_SET(fpcurthread, td);
		td->td_frame->sr |= SR_COP_1_BIT;
		td->td_md.md_flags |= MDTD_FPUSED;
		goto out;

	case T_FPE:
#if !defined(SMP) && (defined(DDB) || defined(DEBUG))
		trapDump("fpintr");
#else
		printf("FPU Trap: PC %#jx CR %x SR %x\n",
		    (intmax_t)trapframe->pc, (unsigned)trapframe->cause, (unsigned)trapframe->sr);
		goto err;
#endif

	case T_FPE + T_USER:
		MipsFPTrap(trapframe->sr, trapframe->cause, trapframe->pc);
		goto out;

	case T_OVFLOW + T_USER:
		i = SIGFPE;
		addr = trapframe->pc;
		break;

	case T_ADDR_ERR_LD:	/* misaligned access */
	case T_ADDR_ERR_ST:	/* misaligned access */
#ifdef TRAP_DEBUG
		printf("+++ ADDR_ERR: type = %d, badvaddr = %#jx\n", type,
		    (intmax_t)trapframe->badvaddr);
#endif
		/* Only allow emulation on a user address */
		if (allow_unaligned_acc &&
		    ((vm_offset_t)trapframe->badvaddr < VM_MAXUSER_ADDRESS)) {
			int mode;

			if (type == T_ADDR_ERR_LD)
				mode = VM_PROT_READ;
			else
				mode = VM_PROT_WRITE;

			/*
			 * ADDR_ERR faults have higher priority than TLB
			 * Miss faults.  Therefore, it is necessary to
			 * verify that the faulting address is a valid
			 * virtual address within the process' address space
			 * before trying to emulate the unaligned access.
			 */
			if (useracc((caddr_t)
			    (((vm_offset_t)trapframe->badvaddr) &
			    ~(sizeof(int) - 1)), sizeof(int) * 2, mode)) {
				access_type = emulate_unaligned_access(
				    trapframe);
				if (access_type != 0) {
					return (trapframe->pc);
				}
			}
		}
		/* FALLTHROUGH */

	case T_BUS_ERR_LD_ST:	/* BERR asserted to cpu */
		if (td->td_pcb->pcb_onfault != NULL) {
			pc = (register_t)(intptr_t)td->td_pcb->pcb_onfault;
			td->td_pcb->pcb_onfault = NULL;
			return (pc);
		}

		/* FALLTHROUGH */

	default:
err:

#if !defined(SMP) && defined(DEBUG)
		stacktrace(!usermode ? trapframe : td->td_frame);
		trapDump("trap");
#endif
#ifdef SMP
		printf("cpu:%d-", PCPU_GET(cpuid));
#endif
		printf("Trap cause = %d (%s - ", type,
		    trap_type[type & (~T_USER)]);

		if (type & T_USER)
			printf("user mode)\n");
		else
			printf("kernel mode)\n");

#ifdef TRAP_DEBUG
		printf("badvaddr = %#jx, pc = %#jx, ra = %#jx, sr = %#jxx\n",
		       (intmax_t)trapframe->badvaddr, (intmax_t)trapframe->pc, (intmax_t)trapframe->ra,
		       (intmax_t)trapframe->sr);
#endif

#ifdef KDB
		if (debugger_on_panic || kdb_active) {
			kdb_trap(type, 0, trapframe);
		}
#endif
		panic("trap");
	}
	td->td_frame->pc = trapframe->pc;
	td->td_frame->cause = trapframe->cause;
	td->td_frame->badvaddr = trapframe->badvaddr;
	ksiginfo_init_trap(&ksi);
	ksi.ksi_signo = i;
	ksi.ksi_code = ucode;
	ksi.ksi_addr = (void *)addr;
	ksi.ksi_trapno = type;
	trapsignal(td, &ksi);
out:

	/*
	 * Note: we should only get here if returning to user mode.
	 */
	userret(td, trapframe);
	mtx_assert(&Giant, MA_NOTOWNED);
	return (trapframe->pc);
}

#if !defined(SMP) && (defined(DDB) || defined(DEBUG))
void
trapDump(char *msg)
{
	register_t s;
	int i;

	s = intr_disable();
	printf("trapDump(%s)\n", msg);
	for (i = 0; i < TRAPSIZE; i++) {
		if (trp == trapdebug) {
			trp = &trapdebug[TRAPSIZE - 1];
		} else {
			trp--;
		}

		if (trp->cause == 0)
			break;

		printf("%s: ADR %jx PC %jx CR %jx SR %jx\n",
		    trap_type[(trp->cause & CR_EXC_CODE) >> CR_EXC_CODE_SHIFT],
		    (intmax_t)trp->vadr, (intmax_t)trp->pc, (intmax_t)trp->cause, (intmax_t)trp->status);

		printf("   RA %jx SP %jx code %d\n", (intmax_t)trp->ra, (intmax_t)trp->sp, (int)trp->code);
	}
	intr_restore(s);
}
#endif


/*
 * Return the resulting PC as if the branch was executed.
 */
uintptr_t
MipsEmulateBranch(struct trapframe *framePtr, uintptr_t instPC, int fpcCSR,
    uintptr_t instptr)
{
	InstFmt inst;
	register_t *regsPtr = (register_t *) framePtr;
	uintptr_t retAddr = 0;
	int condition;

#define	GetBranchDest(InstPtr, inst) \
	(InstPtr + 4 + ((short)inst.IType.imm << 2))


	if (instptr) {
		if (instptr < MIPS_KSEG0_START)
			inst.word = fuword32((void *)instptr);
		else
			inst = *(InstFmt *) instptr;
	} else {
		if ((vm_offset_t)instPC < MIPS_KSEG0_START)
			inst.word = fuword32((void *)instPC);
		else
			inst = *(InstFmt *) instPC;
	}

	switch ((int)inst.JType.op) {
	case OP_SPECIAL:
		switch ((int)inst.RType.func) {
		case OP_JR:
		case OP_JALR:
			retAddr = regsPtr[inst.RType.rs];
			break;

		default:
			retAddr = instPC + 4;
			break;
		}
		break;

	case OP_BCOND:
		switch ((int)inst.IType.rt) {
		case OP_BLTZ:
		case OP_BLTZL:
		case OP_BLTZAL:
		case OP_BLTZALL:
			if ((int)(regsPtr[inst.RType.rs]) < 0)
				retAddr = GetBranchDest(instPC, inst);
			else
				retAddr = instPC + 8;
			break;

		case OP_BGEZ:
		case OP_BGEZL:
		case OP_BGEZAL:
		case OP_BGEZALL:
			if ((int)(regsPtr[inst.RType.rs]) >= 0)
				retAddr = GetBranchDest(instPC, inst);
			else
				retAddr = instPC + 8;
			break;

		case OP_TGEI:
		case OP_TGEIU:
		case OP_TLTI:
		case OP_TLTIU:
		case OP_TEQI:
		case OP_TNEI:
			retAddr = instPC + 4;	/* Like syscall... */
			break;

		default:
			panic("MipsEmulateBranch: Bad branch cond");
		}
		break;

	case OP_J:
	case OP_JAL:
		retAddr = (inst.JType.target << 2) |
		    ((unsigned)(instPC + 4) & 0xF0000000);
		break;

	case OP_BEQ:
	case OP_BEQL:
		if (regsPtr[inst.RType.rs] == regsPtr[inst.RType.rt])
			retAddr = GetBranchDest(instPC, inst);
		else
			retAddr = instPC + 8;
		break;

	case OP_BNE:
	case OP_BNEL:
		if (regsPtr[inst.RType.rs] != regsPtr[inst.RType.rt])
			retAddr = GetBranchDest(instPC, inst);
		else
			retAddr = instPC + 8;
		break;

	case OP_BLEZ:
	case OP_BLEZL:
		if ((int)(regsPtr[inst.RType.rs]) <= 0)
			retAddr = GetBranchDest(instPC, inst);
		else
			retAddr = instPC + 8;
		break;

	case OP_BGTZ:
	case OP_BGTZL:
		if ((int)(regsPtr[inst.RType.rs]) > 0)
			retAddr = GetBranchDest(instPC, inst);
		else
			retAddr = instPC + 8;
		break;

	case OP_COP1:
		switch (inst.RType.rs) {
		case OP_BCx:
		case OP_BCy:
			if ((inst.RType.rt & COPz_BC_TF_MASK) == COPz_BC_TRUE)
				condition = fpcCSR & FPC_COND_BIT;
			else
				condition = !(fpcCSR & FPC_COND_BIT);
			if (condition)
				retAddr = GetBranchDest(instPC, inst);
			else
				retAddr = instPC + 8;
			break;

		default:
			retAddr = instPC + 4;
		}
		break;

	default:
		retAddr = instPC + 4;
	}
	return (retAddr);
}


#if defined(DDB) || defined(DEBUG)
/*
 * Print a stack backtrace.
 */
void
stacktrace(struct trapframe *regs)
{
	stacktrace_subr(regs->pc, regs->sp, regs->ra, printf);
}
#endif

static void
log_frame_dump(struct trapframe *frame)
{
	log(LOG_ERR, "Trapframe Register Dump:\n");
	log(LOG_ERR, "\tzero: %#jx\tat: %#jx\tv0: %#jx\tv1: %#jx\n",
	    (intmax_t)0, (intmax_t)frame->ast, (intmax_t)frame->v0, (intmax_t)frame->v1);

	log(LOG_ERR, "\ta0: %#jx\ta1: %#jx\ta2: %#jx\ta3: %#jx\n",
	    (intmax_t)frame->a0, (intmax_t)frame->a1, (intmax_t)frame->a2, (intmax_t)frame->a3);

	log(LOG_ERR, "\tt0: %#jx\tt1: %#jx\tt2: %#jx\tt3: %#jx\n",
	    (intmax_t)frame->t0, (intmax_t)frame->t1, (intmax_t)frame->t2, (intmax_t)frame->t3);

	log(LOG_ERR, "\tt4: %#jx\tt5: %#jx\tt6: %#jx\tt7: %#jx\n",
	    (intmax_t)frame->t4, (intmax_t)frame->t5, (intmax_t)frame->t6, (intmax_t)frame->t7);

	log(LOG_ERR, "\tt8: %#jx\tt9: %#jx\ts0: %#jx\ts1: %#jx\n",
	    (intmax_t)frame->t8, (intmax_t)frame->t9, (intmax_t)frame->s0, (intmax_t)frame->s1);

	log(LOG_ERR, "\ts2: %#jx\ts3: %#jx\ts4: %#jx\ts5: %#jx\n",
	    (intmax_t)frame->s2, (intmax_t)frame->s3, (intmax_t)frame->s4, (intmax_t)frame->s5);

	log(LOG_ERR, "\ts6: %#jx\ts7: %#jx\tk0: %#jx\tk1: %#jx\n",
	    (intmax_t)frame->s6, (intmax_t)frame->s7, (intmax_t)frame->k0, (intmax_t)frame->k1);

	log(LOG_ERR, "\tgp: %#jx\tsp: %#jx\ts8: %#jx\tra: %#jx\n",
	    (intmax_t)frame->gp, (intmax_t)frame->sp, (intmax_t)frame->s8, (intmax_t)frame->ra);

	log(LOG_ERR, "\tsr: %#jx\tmullo: %#jx\tmulhi: %#jx\tbadvaddr: %#jx\n",
	    (intmax_t)frame->sr, (intmax_t)frame->mullo, (intmax_t)frame->mulhi, (intmax_t)frame->badvaddr);

#ifdef IC_REG
	log(LOG_ERR, "\tcause: %#jx\tpc: %#jx\tic: %#jx\n",
	    (intmax_t)frame->cause, (intmax_t)frame->pc, (intmax_t)frame->ic);
#else
	log(LOG_ERR, "\tcause: %#jx\tpc: %#jx\n",
	    (intmax_t)frame->cause, (intmax_t)frame->pc);
#endif
}

#ifdef TRAP_DEBUG
static void
trap_frame_dump(struct trapframe *frame)
{
	printf("Trapframe Register Dump:\n");
	printf("\tzero: %#jx\tat: %#jx\tv0: %#jx\tv1: %#jx\n",
	    (intmax_t)0, (intmax_t)frame->ast, (intmax_t)frame->v0, (intmax_t)frame->v1);

	printf("\ta0: %#jx\ta1: %#jx\ta2: %#jx\ta3: %#jx\n",
	    (intmax_t)frame->a0, (intmax_t)frame->a1, (intmax_t)frame->a2, (intmax_t)frame->a3);

	printf("\tt0: %#jx\tt1: %#jx\tt2: %#jx\tt3: %#jx\n",
	    (intmax_t)frame->t0, (intmax_t)frame->t1, (intmax_t)frame->t2, (intmax_t)frame->t3);

	printf("\tt4: %#jx\tt5: %#jx\tt6: %#jx\tt7: %#jx\n",
	    (intmax_t)frame->t4, (intmax_t)frame->t5, (intmax_t)frame->t6, (intmax_t)frame->t7);

	printf("\tt8: %#jx\tt9: %#jx\ts0: %#jx\ts1: %#jx\n",
	    (intmax_t)frame->t8, (intmax_t)frame->t9, (intmax_t)frame->s0, (intmax_t)frame->s1);

	printf("\ts2: %#jx\ts3: %#jx\ts4: %#jx\ts5: %#jx\n",
	    (intmax_t)frame->s2, (intmax_t)frame->s3, (intmax_t)frame->s4, (intmax_t)frame->s5);

	printf("\ts6: %#jx\ts7: %#jx\tk0: %#jx\tk1: %#jx\n",
	    (intmax_t)frame->s6, (intmax_t)frame->s7, (intmax_t)frame->k0, (intmax_t)frame->k1);

	printf("\tgp: %#jx\tsp: %#jx\ts8: %#jx\tra: %#jx\n",
	    (intmax_t)frame->gp, (intmax_t)frame->sp, (intmax_t)frame->s8, (intmax_t)frame->ra);

	printf("\tsr: %#jx\tmullo: %#jx\tmulhi: %#jx\tbadvaddr: %#jx\n",
	    (intmax_t)frame->sr, (intmax_t)frame->mullo, (intmax_t)frame->mulhi, (intmax_t)frame->badvaddr);

#ifdef IC_REG
	printf("\tcause: %#jx\tpc: %#jx\tic: %#jx\n",
	    (intmax_t)frame->cause, (intmax_t)frame->pc, (intmax_t)frame->ic);
#else
	printf("\tcause: %#jx\tpc: %#jx\n",
	    (intmax_t)frame->cause, (intmax_t)frame->pc);
#endif
}

#endif


static void
get_mapping_info(vm_offset_t va, pd_entry_t **pdepp, pt_entry_t **ptepp)
{
	pt_entry_t *ptep;
	pd_entry_t *pdep;
	struct proc *p = curproc;

	pdep = (&(p->p_vmspace->vm_pmap.pm_segtab[(va >> SEGSHIFT) & (NPDEPG - 1)]));
	if (*pdep)
		ptep = pmap_pte(&p->p_vmspace->vm_pmap, va);
	else
		ptep = (pt_entry_t *)0;

	*pdepp = pdep;
	*ptepp = ptep;
}

static void
log_illegal_instruction(const char *msg, struct trapframe *frame)
{
	pt_entry_t *ptep;
	pd_entry_t *pdep;
	unsigned int *addr;
	struct proc *p = curproc;
	register_t pc;

#ifdef SMP
	printf("cpuid = %d\n", PCPU_GET(cpuid));
#endif
	pc = frame->pc + (DELAYBRANCH(frame->cause) ? 4 : 0);
	log(LOG_ERR, "%s: pid %d (%s), uid %d: pc %#jx ra %#jx\n",
	    msg, p->p_pid, p->p_comm,
	    p->p_ucred ? p->p_ucred->cr_uid : -1,
	    (intmax_t)pc,
	    (intmax_t)frame->ra);

	/* log registers in trap frame */
	log_frame_dump(frame);

	get_mapping_info((vm_offset_t)pc, &pdep, &ptep);

	/*
	 * Dump a few words around faulting instruction, if the addres is
	 * valid.
	 */
	if (!(pc & 3) &&
	    useracc((caddr_t)(intptr_t)pc, sizeof(int) * 4, VM_PROT_READ)) {
		/* dump page table entry for faulting instruction */
		log(LOG_ERR, "Page table info for pc address %#jx: pde = %p, pte = %#x\n",
		    (intmax_t)pc, (void *)(intptr_t)*pdep, ptep ? *ptep : 0);

		addr = (unsigned int *)(intptr_t)pc;
		log(LOG_ERR, "Dumping 4 words starting at pc address %p: \n",
		    addr);
		log(LOG_ERR, "%08x %08x %08x %08x\n",
		    addr[0], addr[1], addr[2], addr[3]);
	} else {
		log(LOG_ERR, "pc address %#jx is inaccessible, pde = %p, pte = %#x\n",
		    (intmax_t)pc, (void *)(intptr_t)*pdep, ptep ? *ptep : 0);
	}
}

static void
log_bad_page_fault(char *msg, struct trapframe *frame, int trap_type)
{
	pt_entry_t *ptep;
	pd_entry_t *pdep;
	unsigned int *addr;
	struct proc *p = curproc;
	char *read_or_write;
	register_t pc;

	trap_type &= ~T_USER;

#ifdef SMP
	printf("cpuid = %d\n", PCPU_GET(cpuid));
#endif
	switch (trap_type) {
	case T_TLB_ST_MISS:
	case T_ADDR_ERR_ST:
		read_or_write = "write";
		break;
	case T_TLB_LD_MISS:
	case T_ADDR_ERR_LD:
	case T_BUS_ERR_IFETCH:
		read_or_write = "read";
		break;
	default:
		read_or_write = "";
	}

	pc = frame->pc + (DELAYBRANCH(frame->cause) ? 4 : 0);
	log(LOG_ERR, "%s: pid %d (%s), uid %d: pc %#jx got a %s fault at %#jx\n",
	    msg, p->p_pid, p->p_comm,
	    p->p_ucred ? p->p_ucred->cr_uid : -1,
	    (intmax_t)pc,
	    read_or_write,
	    (intmax_t)frame->badvaddr);

	/* log registers in trap frame */
	log_frame_dump(frame);

	get_mapping_info((vm_offset_t)pc, &pdep, &ptep);

	/*
	 * Dump a few words around faulting instruction, if the addres is
	 * valid.
	 */
	if (!(pc & 3) && (pc != frame->badvaddr) &&
	    (trap_type != T_BUS_ERR_IFETCH) &&
	    useracc((caddr_t)(intptr_t)pc, sizeof(int) * 4, VM_PROT_READ)) {
		/* dump page table entry for faulting instruction */
		log(LOG_ERR, "Page table info for pc address %#jx: pde = %p, pte = %#x\n",
		    (intmax_t)pc, (void *)(intptr_t)*pdep, ptep ? *ptep : 0);

		addr = (unsigned int *)(intptr_t)pc;
		log(LOG_ERR, "Dumping 4 words starting at pc address %p: \n",
		    addr);
		log(LOG_ERR, "%08x %08x %08x %08x\n",
		    addr[0], addr[1], addr[2], addr[3]);
	} else {
		log(LOG_ERR, "pc address %#jx is inaccessible, pde = %p, pte = %#x\n",
		    (intmax_t)pc, (void *)(intptr_t)*pdep, ptep ? *ptep : 0);
	}

	get_mapping_info((vm_offset_t)frame->badvaddr, &pdep, &ptep);
	log(LOG_ERR, "Page table info for bad address %#jx: pde = %p, pte = %#x\n",
	    (intmax_t)frame->badvaddr, (void *)(intptr_t)*pdep, ptep ? *ptep : 0);
}


/*
 * Unaligned load/store emulation
 */
static int
mips_unaligned_load_store(struct trapframe *frame, register_t addr, register_t pc)
{
	register_t *reg = (register_t *) frame;
	u_int32_t inst = *((u_int32_t *)(intptr_t)pc);
	u_int32_t value_msb, value;
	int access_type = 0;

	switch (MIPS_INST_OPCODE(inst)) {
	case OP_LHU:
		lbu_macro(value_msb, addr);
		addr += 1;
		lbu_macro(value, addr);
		value |= value_msb << 8;
		reg[MIPS_INST_RT(inst)] = value;
		access_type = MIPS_LHU_ACCESS;
		break;

	case OP_LH:
		lb_macro(value_msb, addr);
		addr += 1;
		lbu_macro(value, addr);
		value |= value_msb << 8;
		reg[MIPS_INST_RT(inst)] = value;
		access_type = MIPS_LH_ACCESS;
		break;

	case OP_LWU:
		lwl_macro(value, addr);
		addr += 3;
		lwr_macro(value, addr);
		value &= 0xffffffff;
		reg[MIPS_INST_RT(inst)] = value;
		access_type = MIPS_LWU_ACCESS;
		break;

	case OP_LW:
		lwl_macro(value, addr);
		addr += 3;
		lwr_macro(value, addr);
		reg[MIPS_INST_RT(inst)] = value;
		access_type = MIPS_LW_ACCESS;
		break;

	case OP_SH:
		value = reg[MIPS_INST_RT(inst)];
		value_msb = value >> 8;
		sb_macro(value_msb, addr);
		addr += 1;
		sb_macro(value, addr);
		access_type = MIPS_SH_ACCESS;
		break;

	case OP_SW:
		value = reg[MIPS_INST_RT(inst)];
		swl_macro(value, addr);
		addr += 3;
		swr_macro(value, addr);
		access_type = MIPS_SW_ACCESS;
		break;

	default:
		break;
	}

	return access_type;
}


static int
emulate_unaligned_access(struct trapframe *frame)
{
	register_t pc;
	int access_type = 0;

	pc = frame->pc + (DELAYBRANCH(frame->cause) ? 4 : 0);

	/*
	 * Fall through if it's instruction fetch exception
	 */
	if (!((pc & 3) || (pc == frame->badvaddr))) {

		/*
		 * Handle unaligned load and store
		 */

		/*
		 * Return access type if the instruction was emulated.
		 * Otherwise restore pc and fall through.
		 */
		access_type = mips_unaligned_load_store(frame,
		    frame->badvaddr, pc);

		if (access_type) {
			if (DELAYBRANCH(frame->cause))
				frame->pc = MipsEmulateBranch(frame, frame->pc,
				    0, 0);
			else
				frame->pc += 4;

			log(LOG_INFO, "Unaligned %s: pc=%#jx, badvaddr=%#jx\n",
			    access_name[access_type - 1], (intmax_t)pc,
			    (intmax_t)frame->badvaddr);
		}
	}
	return access_type;
}
