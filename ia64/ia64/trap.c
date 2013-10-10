/*-
 * Copyright (c) 2005 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kdb.h>
#include <sys/ktr.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/exec.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/vmmeter.h>
#include <sys/sysent.h>
#include <sys/signalvar.h>
#include <sys/syscall.h>
#include <sys/pioctl.h>
#include <sys/ptrace.h>
#include <sys/sysctl.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>
#include <sys/ptrace.h>
#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/reg.h>
#include <machine/pal.h>
#include <machine/fpu.h>
#include <machine/efi.h>
#include <machine/pcb.h>
#ifdef SMP
#include <machine/smp.h>
#endif

#include <security/audit/audit.h>

#include <ia64/disasm/disasm.h>

static int print_usertrap = 0;
SYSCTL_INT(_machdep, OID_AUTO, print_usertrap,
    CTLFLAG_RW, &print_usertrap, 0, "");

static void break_syscall(struct trapframe *tf);

/*
 * EFI-Provided FPSWA interface (Floating Point SoftWare Assist)
 */
extern struct fpswa_iface *fpswa_iface;

static const char *ia64_vector_names[] = {
	"VHPT Translation",			/* 0 */
	"Instruction TLB",			/* 1 */
	"Data TLB",				/* 2 */
	"Alternate Instruction TLB",		/* 3 */
	"Alternate Data TLB",			/* 4 */
	"Data Nested TLB",			/* 5 */
	"Instruction Key Miss",			/* 6 */
	"Data Key Miss",			/* 7 */
	"Dirty-Bit",				/* 8 */
	"Instruction Access-Bit",		/* 9 */
	"Data Access-Bit",			/* 10 */
	"Break Instruction",			/* 11 */
	"External Interrupt",			/* 12 */
	"Reserved 13",				/* 13 */
	"Reserved 14",				/* 14 */
	"Reserved 15",				/* 15 */
	"Reserved 16",				/* 16 */
	"Reserved 17",				/* 17 */
	"Reserved 18",				/* 18 */
	"Reserved 19",				/* 19 */
	"Page Not Present",			/* 20 */
	"Key Permission",			/* 21 */
	"Instruction Access Rights",		/* 22 */
	"Data Access Rights",			/* 23 */
	"General Exception",			/* 24 */
	"Disabled FP-Register",			/* 25 */
	"NaT Consumption",			/* 26 */
	"Speculation",				/* 27 */
	"Reserved 28",				/* 28 */
	"Debug",				/* 29 */
	"Unaligned Reference",			/* 30 */
	"Unsupported Data Reference",		/* 31 */
	"Floating-point Fault",			/* 32 */
	"Floating-point Trap",			/* 33 */
	"Lower-Privilege Transfer Trap",	/* 34 */
	"Taken Branch Trap",			/* 35 */
	"Single Step Trap",			/* 36 */
	"Reserved 37",				/* 37 */
	"Reserved 38",				/* 38 */
	"Reserved 39",				/* 39 */
	"Reserved 40",				/* 40 */
	"Reserved 41",				/* 41 */
	"Reserved 42",				/* 42 */
	"Reserved 43",				/* 43 */
	"Reserved 44",				/* 44 */
	"IA-32 Exception",			/* 45 */
	"IA-32 Intercept",			/* 46 */
	"IA-32 Interrupt",			/* 47 */
	"Reserved 48",				/* 48 */
	"Reserved 49",				/* 49 */
	"Reserved 50",				/* 50 */
	"Reserved 51",				/* 51 */
	"Reserved 52",				/* 52 */
	"Reserved 53",				/* 53 */
	"Reserved 54",				/* 54 */
	"Reserved 55",				/* 55 */
	"Reserved 56",				/* 56 */
	"Reserved 57",				/* 57 */
	"Reserved 58",				/* 58 */
	"Reserved 59",				/* 59 */
	"Reserved 60",				/* 60 */
	"Reserved 61",				/* 61 */
	"Reserved 62",				/* 62 */
	"Reserved 63",				/* 63 */
	"Reserved 64",				/* 64 */
	"Reserved 65",				/* 65 */
	"Reserved 66",				/* 66 */
	"Reserved 67",				/* 67 */
};

struct bitname {
	uint64_t mask;
	const char* name;
};

static void
printbits(uint64_t mask, struct bitname *bn, int count)
{
	int i, first = 1;
	uint64_t bit;

	for (i = 0; i < count; i++) {
		/*
		 * Handle fields wider than one bit.
		 */
		bit = bn[i].mask & ~(bn[i].mask - 1);
		if (bn[i].mask > bit) {
			if (first)
				first = 0;
			else
				printf(",");
			printf("%s=%ld", bn[i].name,
			       (mask & bn[i].mask) / bit);
		} else if (mask & bit) {
			if (first)
				first = 0;
			else
				printf(",");
			printf("%s", bn[i].name);
		}
	}
}

struct bitname psr_bits[] = {
	{IA64_PSR_BE,	"be"},
	{IA64_PSR_UP,	"up"},
	{IA64_PSR_AC,	"ac"},
	{IA64_PSR_MFL,	"mfl"},
	{IA64_PSR_MFH,	"mfh"},
	{IA64_PSR_IC,	"ic"},
	{IA64_PSR_I,	"i"},
	{IA64_PSR_PK,	"pk"},
	{IA64_PSR_DT,	"dt"},
	{IA64_PSR_DFL,	"dfl"},
	{IA64_PSR_DFH,	"dfh"},
	{IA64_PSR_SP,	"sp"},
	{IA64_PSR_PP,	"pp"},
	{IA64_PSR_DI,	"di"},
	{IA64_PSR_SI,	"si"},
	{IA64_PSR_DB,	"db"},
	{IA64_PSR_LP,	"lp"},
	{IA64_PSR_TB,	"tb"},
	{IA64_PSR_RT,	"rt"},
	{IA64_PSR_CPL,	"cpl"},
	{IA64_PSR_IS,	"is"},
	{IA64_PSR_MC,	"mc"},
	{IA64_PSR_IT,	"it"},
	{IA64_PSR_ID,	"id"},
	{IA64_PSR_DA,	"da"},
	{IA64_PSR_DD,	"dd"},
	{IA64_PSR_SS,	"ss"},
	{IA64_PSR_RI,	"ri"},
	{IA64_PSR_ED,	"ed"},
	{IA64_PSR_BN,	"bn"},
	{IA64_PSR_IA,	"ia"},
};

static void
printpsr(uint64_t psr)
{
	printbits(psr, psr_bits, sizeof(psr_bits)/sizeof(psr_bits[0]));
}

struct bitname isr_bits[] = {
	{IA64_ISR_CODE,	"code"},
	{IA64_ISR_VECTOR, "vector"},
	{IA64_ISR_X,	"x"},
	{IA64_ISR_W,	"w"},
	{IA64_ISR_R,	"r"},
	{IA64_ISR_NA,	"na"},
	{IA64_ISR_SP,	"sp"},
	{IA64_ISR_RS,	"rs"},
	{IA64_ISR_IR,	"ir"},
	{IA64_ISR_NI,	"ni"},
	{IA64_ISR_SO,	"so"},
	{IA64_ISR_EI,	"ei"},
	{IA64_ISR_ED,	"ed"},
};

static void printisr(uint64_t isr)
{
	printbits(isr, isr_bits, sizeof(isr_bits)/sizeof(isr_bits[0]));
}

static void
printtrap(int vector, struct trapframe *tf, int isfatal, int user)
{
	printf("\n");
	printf("%s %s trap (cpu %d):\n", isfatal? "fatal" : "handled",
	       user ? "user" : "kernel", PCPU_GET(cpuid));
	printf("\n");
	printf("    trap vector = 0x%x (%s)\n",
	       vector, ia64_vector_names[vector]);
	printf("    cr.iip      = 0x%lx\n", tf->tf_special.iip);
	printf("    cr.ipsr     = 0x%lx (", tf->tf_special.psr);
	printpsr(tf->tf_special.psr);
	printf(")\n");
	printf("    cr.isr      = 0x%lx (", tf->tf_special.isr);
	printisr(tf->tf_special.isr);
	printf(")\n");
	printf("    cr.ifa      = 0x%lx\n", tf->tf_special.ifa);
	if (tf->tf_special.psr & IA64_PSR_IS) {
		printf("    ar.cflg     = 0x%lx\n", ia64_get_cflg());
		printf("    ar.csd      = 0x%lx\n", ia64_get_csd());
		printf("    ar.ssd      = 0x%lx\n", ia64_get_ssd());
	}
	printf("    curthread   = %p\n", curthread);
	if (curthread != NULL)
		printf("        pid = %d, comm = %s\n",
		       curthread->td_proc->p_pid, curthread->td_name);
	printf("\n");
}

/*
 * We got a trap caused by a break instruction and the immediate was 0.
 * This indicates that we may have a break.b with some non-zero immediate.
 * The break.b doesn't cause the immediate to be put in cr.iim.  Hence,
 * we need to disassemble the bundle and return the immediate found there.
 * This may be a 0 value anyway.  Return 0 for any error condition.  This
 * will result in a SIGILL, which is pretty much the best thing to do.
 */
static uint64_t
trap_decode_break(struct trapframe *tf)
{
	struct asm_bundle bundle;
	struct asm_inst *inst;
	int slot;

	if (!asm_decode(tf->tf_special.iip, &bundle))
		return (0);

	slot = ((tf->tf_special.psr & IA64_PSR_RI) == IA64_PSR_RI_0) ? 0 :
            ((tf->tf_special.psr & IA64_PSR_RI) == IA64_PSR_RI_1) ? 1 : 2;
	inst = bundle.b_inst + slot;

	/*
	 * Sanity checking: It must be a break instruction and the operand
	 * that has the break value must be an immediate.
	 */
	if (inst->i_op != ASM_OP_BREAK ||
	    inst->i_oper[1].o_type != ASM_OPER_IMM)
		return (0);

	return (inst->i_oper[1].o_value);
}

void
trap_panic(int vector, struct trapframe *tf)
{

	printtrap(vector, tf, 1, TRAPF_USERMODE(tf));
#ifdef KDB
	kdb_trap(vector, 0, tf);
#endif
	panic("trap");
}

/*
 *
 */
int
do_ast(struct trapframe *tf)
{

	ia64_disable_intr();
	while (curthread->td_flags & (TDF_ASTPENDING|TDF_NEEDRESCHED)) {
		ia64_enable_intr();
		ast(tf);
		ia64_disable_intr();
	}
	/*
	 * Keep interrupts disabled. We return r10 as a favor to the EPC
	 * syscall code so that it can quicky determine if the syscall
	 * needs to be restarted or not.
	 */
	return (tf->tf_scratch.gr10);
}

/*
 * Trap is called from exception.s to handle most types of processor traps.
 */
/*ARGSUSED*/
void
trap(int vector, struct trapframe *tf)
{
	struct proc *p;
	struct thread *td;
	uint64_t ucode;
	int error, sig, user;
	ksiginfo_t ksi;

	user = TRAPF_USERMODE(tf) ? 1 : 0;

	PCPU_INC(cnt.v_trap);

	td = curthread;
	p = td->td_proc;
	ucode = 0;

	if (user) {
		ia64_set_fpsr(IA64_FPSR_DEFAULT);
		td->td_pticks = 0;
		td->td_frame = tf;
		if (td->td_ucred != p->p_ucred)
			cred_update_thread(td);
	} else {
		KASSERT(cold || td->td_ucred != NULL,
		    ("kernel trap doesn't have ucred"));
#ifdef KDB
		if (kdb_active)
			kdb_reenter();
#endif
	}

	sig = 0;
	switch (vector) {
	case IA64_VEC_VHPT:
		/*
		 * This one is tricky. We should hardwire the VHPT, but
		 * don't at this time. I think we're mostly lucky that
		 * the VHPT is mapped.
		 */
		trap_panic(vector, tf);
		break;

	case IA64_VEC_ITLB:
	case IA64_VEC_DTLB:
	case IA64_VEC_EXT_INTR:
		/* We never call trap() with these vectors. */
		trap_panic(vector, tf);
		break;

	case IA64_VEC_ALT_ITLB:
	case IA64_VEC_ALT_DTLB:
		/*
		 * These should never happen, because regions 0-4 use the
		 * VHPT. If we get one of these it means we didn't program
		 * the region registers correctly.
		 */
		trap_panic(vector, tf);
		break;

	case IA64_VEC_NESTED_DTLB:
		/*
		 * When the nested TLB handler encounters an unexpected
		 * condition, it'll switch to the backup stack and transfer
		 * here. All we need to do is panic.
		 */
		trap_panic(vector, tf);
		break;

	case IA64_VEC_IKEY_MISS:
	case IA64_VEC_DKEY_MISS:
	case IA64_VEC_KEY_PERMISSION:
		/*
		 * We don't use protection keys, so we should never get
		 * these faults.
		 */
		trap_panic(vector, tf);
		break;

	case IA64_VEC_DIRTY_BIT:
	case IA64_VEC_INST_ACCESS:
	case IA64_VEC_DATA_ACCESS:
		/*
		 * We get here if we read or write to a page of which the
		 * PTE does not have the access bit or dirty bit set and
		 * we can not find the PTE in our datastructures. This
		 * either means we have a stale PTE in the TLB, or we lost
		 * the PTE in our datastructures.
		 */
		trap_panic(vector, tf);
		break;

	case IA64_VEC_BREAK:
		if (user) {
			ucode = (int)tf->tf_special.ifa & 0x1FFFFF;
			if (ucode == 0) {
				/*
				 * A break.b doesn't cause the immediate to be
				 * stored in cr.iim (and saved in the TF in
				 * tf_special.ifa).  We need to decode the
				 * instruction to find out what the immediate
				 * was.  Note that if the break instruction
				 * didn't happen to be a break.b, but any
				 * other break with an immediate of 0, we
				 * will do unnecessary work to get the value
				 * we already had.  Not an issue, because a
				 * break 0 is invalid.
				 */
				ucode = trap_decode_break(tf);
			}
			if (ucode < 0x80000) {
				/* Software interrupts. */
				switch (ucode) {
				case 0:		/* Unknown error. */
					sig = SIGILL;
					break;
				case 1:		/* Integer divide by zero. */
					sig = SIGFPE;
					ucode = FPE_INTDIV;
					break;
				case 2:		/* Integer overflow. */
					sig = SIGFPE;
					ucode = FPE_INTOVF;
					break;
				case 3:		/* Range check/bounds check. */
					sig = SIGFPE;
					ucode = FPE_FLTSUB;
					break;
				case 6: 	/* Decimal overflow. */
				case 7: 	/* Decimal divide by zero. */
				case 8: 	/* Packed decimal error. */
				case 9: 	/* Invalid ASCII digit. */
				case 10:	/* Invalid decimal digit. */
					sig = SIGFPE;
					ucode = FPE_FLTINV;
					break;
				case 4:		/* Null pointer dereference. */
				case 5:		/* Misaligned data. */
				case 11:	/* Paragraph stack overflow. */
					sig = SIGSEGV;
					break;
				default:
					sig = SIGILL;
					break;
				}
			} else if (ucode < 0x100000) {
				/* Debugger breakpoint. */
				tf->tf_special.psr &= ~IA64_PSR_SS;
				sig = SIGTRAP;
			} else if (ucode == 0x100000) {
				break_syscall(tf);
				return;		/* do_ast() already called. */
			} else if (ucode == 0x180000) {
				mcontext_t mc;

				error = copyin((void*)tf->tf_scratch.gr8,
				    &mc, sizeof(mc));
				if (!error) {
					set_mcontext(td, &mc);
					return;	/* Don't call do_ast()!!! */
				}
				sig = SIGSEGV;
				ucode = tf->tf_scratch.gr8;
			} else
				sig = SIGILL;
		} else {
#ifdef KDB
			if (kdb_trap(vector, 0, tf))
				return;
			panic("trap");
#else
			trap_panic(vector, tf);
#endif
		}
		break;

	case IA64_VEC_PAGE_NOT_PRESENT:
	case IA64_VEC_INST_ACCESS_RIGHTS:
	case IA64_VEC_DATA_ACCESS_RIGHTS: {
		vm_offset_t va;
		struct vmspace *vm;
		vm_map_t map;
		vm_prot_t ftype;
		int rv;

		rv = 0;
		va = trunc_page(tf->tf_special.ifa);

		if (va >= VM_MAXUSER_ADDRESS) {
			/*
			 * Don't allow user-mode faults for kernel virtual
			 * addresses, including the gateway page.
			 */
			if (user)
				goto no_fault_in;
			map = kernel_map;
		} else {
			vm = (p != NULL) ? p->p_vmspace : NULL;
			if (vm == NULL)
				goto no_fault_in;
			map = &vm->vm_map;
		}

		if (tf->tf_special.isr & IA64_ISR_X)
			ftype = VM_PROT_EXECUTE;
		else if (tf->tf_special.isr & IA64_ISR_W)
			ftype = VM_PROT_WRITE;
		else
			ftype = VM_PROT_READ;

		if (map != kernel_map) {
			/*
			 * Keep swapout from messing with us during this
			 * critical time.
			 */
			PROC_LOCK(p);
			++p->p_lock;
			PROC_UNLOCK(p);

			/* Fault in the user page: */
			rv = vm_fault(map, va, ftype, VM_FAULT_NORMAL);

			PROC_LOCK(p);
			--p->p_lock;
			PROC_UNLOCK(p);
		} else {
			/*
			 * Don't have to worry about process locking or
			 * stacks in the kernel.
			 */
			rv = vm_fault(map, va, ftype, VM_FAULT_NORMAL);
		}

		if (rv == KERN_SUCCESS)
			goto out;

	no_fault_in:
		if (!user) {
			/* Check for copyin/copyout fault. */
			if (td != NULL && td->td_pcb->pcb_onfault != 0) {
				tf->tf_special.iip =
				    td->td_pcb->pcb_onfault;
				tf->tf_special.psr &= ~IA64_PSR_RI;
				td->td_pcb->pcb_onfault = 0;
				goto out;
			}
			trap_panic(vector, tf);
		}
		ucode = va;
		sig = (rv == KERN_PROTECTION_FAILURE) ? SIGBUS : SIGSEGV;
		break;
	}

	case IA64_VEC_GENERAL_EXCEPTION: {
		int code;

		if (!user)
			trap_panic(vector, tf);

		code = tf->tf_special.isr & (IA64_ISR_CODE & 0xf0ull);
		switch (code) {
		case 0x0:	/* Illegal Operation Fault. */
			sig = ia64_emulate(tf, td);
			break;
		default:
			sig = SIGILL;
			break;
		}
		if (sig == 0)
			goto out;
		ucode = vector;
		break;
	}

	case IA64_VEC_SPECULATION:
		/*
		 * The branching behaviour of the chk instruction is not
		 * implemented by the processor. All we need to do is
		 * compute the target address of the branch and make sure
		 * that control is transfered to that address.
		 * We should do this in the IVT table and not by entring
		 * the kernel...
		 */
		tf->tf_special.iip += tf->tf_special.ifa << 4;
		tf->tf_special.psr &= ~IA64_PSR_RI;
		goto out;

	case IA64_VEC_NAT_CONSUMPTION:
	case IA64_VEC_UNSUPP_DATA_REFERENCE:
		if (user) {
			ucode = vector;
			sig = SIGILL;
		} else
			trap_panic(vector, tf);
		break;

	case IA64_VEC_DISABLED_FP: {
		if (user)
			ia64_highfp_enable(td, tf);
		else
			trap_panic(vector, tf);
		goto out;
	}

	case IA64_VEC_DEBUG:
	case IA64_VEC_SINGLE_STEP_TRAP:
		tf->tf_special.psr &= ~IA64_PSR_SS;
		if (!user) {
#ifdef KDB
			if (kdb_trap(vector, 0, tf))
				return;
			panic("trap");
#else
			trap_panic(vector, tf);
#endif
		}
		sig = SIGTRAP;
		break;

	case IA64_VEC_UNALIGNED_REFERENCE:
		/*
		 * If user-land, do whatever fixups, printing, and
		 * signalling is appropriate (based on system-wide
		 * and per-process unaligned-access-handling flags).
		 */
		if (user) {
			sig = unaligned_fixup(tf, td);
			if (sig == 0)
				goto out;
			ucode = tf->tf_special.ifa;	/* VA */
		} else {
			/* Check for copyin/copyout fault. */
			if (td != NULL && td->td_pcb->pcb_onfault != 0) {
				tf->tf_special.iip =
				    td->td_pcb->pcb_onfault;
				tf->tf_special.psr &= ~IA64_PSR_RI;
				td->td_pcb->pcb_onfault = 0;
				goto out;
			}
			trap_panic(vector, tf);
		}
		break;

	case IA64_VEC_FLOATING_POINT_FAULT:
	case IA64_VEC_FLOATING_POINT_TRAP: {
		struct fpswa_bundle bundle;
		struct fpswa_fpctx fpctx;
		struct fpswa_ret ret;
		char *ip;
		u_long fault;

		/* Always fatal in kernel. Should never happen. */
		if (!user)
			trap_panic(vector, tf);

		if (fpswa_iface == NULL) {
			sig = SIGFPE;
			ucode = 0;
			break;
		}

		ip = (char *)tf->tf_special.iip;
		if (vector == IA64_VEC_FLOATING_POINT_TRAP &&
		    (tf->tf_special.psr & IA64_PSR_RI) == 0)
			ip -= 16;
		error = copyin(ip, &bundle, sizeof(bundle));
		if (error) {
			sig = SIGBUS;	/* EFAULT, basically */
			ucode = 0;	/* exception summary */
			break;
		}

		/* f6-f15 are saved in exception_save */
		fpctx.mask_low = 0xffc0;		/* bits 6 - 15 */
		fpctx.mask_high = 0;
		fpctx.fp_low_preserved = NULL;
		fpctx.fp_low_volatile = &tf->tf_scratch_fp.fr6;
		fpctx.fp_high_preserved = NULL;
		fpctx.fp_high_volatile = NULL;

		fault = (vector == IA64_VEC_FLOATING_POINT_FAULT) ? 1 : 0;

		/*
		 * We have the high FP registers disabled while in the
		 * kernel. Enable them for the FPSWA handler only.
		 */
		ia64_enable_highfp();

		/* The docs are unclear.  Is Fpswa reentrant? */
		ret = fpswa_iface->if_fpswa(fault, &bundle,
		    &tf->tf_special.psr, &tf->tf_special.fpsr,
		    &tf->tf_special.isr, &tf->tf_special.pr,
		    &tf->tf_special.cfm, &fpctx);

		ia64_disable_highfp();

		/*
		 * Update ipsr and iip to next instruction. We only
		 * have to do that for faults.
		 */
		if (fault && (ret.status == 0 || (ret.status & 2))) {
			int ei;

			ei = (tf->tf_special.isr >> 41) & 0x03;
			if (ei == 0) {		/* no template for this case */
				tf->tf_special.psr &= ~IA64_ISR_EI;
				tf->tf_special.psr |= IA64_ISR_EI_1;
			} else if (ei == 1) {	/* MFI or MFB */
				tf->tf_special.psr &= ~IA64_ISR_EI;
				tf->tf_special.psr |= IA64_ISR_EI_2;
			} else if (ei == 2) {	/* MMF */
				tf->tf_special.psr &= ~IA64_ISR_EI;
				tf->tf_special.iip += 0x10;
			}
		}

		if (ret.status == 0) {
			goto out;
		} else if (ret.status == -1) {
			printf("FATAL: FPSWA err1 %lx, err2 %lx, err3 %lx\n",
			    ret.err1, ret.err2, ret.err3);
			panic("fpswa fatal error on fp fault");
		} else {
			sig = SIGFPE;
			ucode = 0;		/* XXX exception summary */
			break;
		}
	}

	case IA64_VEC_LOWER_PRIVILEGE_TRANSFER:
		/*
		 * The lower-privilege transfer trap is used by the EPC
		 * syscall code to trigger re-entry into the kernel when the
		 * process should be single stepped. The problem is that
		 * there's no way to set single stepping directly without
		 * using the rfi instruction. So instead we enable the
		 * lower-privilege transfer trap and when we get here we
		 * know that the process is about to enter userland (and
		 * has already lowered its privilege).
		 * However, there's another gotcha. When the process has
		 * lowered it's privilege it's still running in the gateway
		 * page. If we enable single stepping, we'll be stepping
		 * the code in the gateway page. In and by itself this is
		 * not a problem, but it's an address debuggers won't know
		 * anything about. Hence, it can only cause confusion.
		 * We know that we need to branch to get out of the gateway
		 * page, so what we do here is enable the taken branch
		 * trap and just let the process continue. When we branch
		 * out of the gateway page we'll get back into the kernel
		 * and then we enable single stepping.
		 * Since this a rather round-about way of enabling single
		 * stepping, don't make things even more complicated by
		 * calling userret() and do_ast(). We do that later...
		 */
		tf->tf_special.psr &= ~IA64_PSR_LP;
		tf->tf_special.psr |= IA64_PSR_TB;
		return;

	case IA64_VEC_TAKEN_BRANCH_TRAP:
		/*
		 * Don't assume there aren't any branches other than the
		 * branch that takes us out of the gateway page. Check the
		 * iip and enable single stepping only when it's an user
		 * address.
		 */
		if (tf->tf_special.iip >= VM_MAXUSER_ADDRESS)
			return;
		tf->tf_special.psr &= ~IA64_PSR_TB;
		tf->tf_special.psr |= IA64_PSR_SS;
		return;

	case IA64_VEC_IA32_EXCEPTION:
	case IA64_VEC_IA32_INTERCEPT:
	case IA64_VEC_IA32_INTERRUPT:
		sig = SIGEMT;
		ucode = tf->tf_special.iip;
		break;

	default:
		/* Reserved vectors get here. Should never happen of course. */
		trap_panic(vector, tf);
		break;
	}

	KASSERT(sig != 0, ("foo"));

	if (print_usertrap)
		printtrap(vector, tf, 1, user);

	ksiginfo_init(&ksi);
	ksi.ksi_signo = sig;
	ksi.ksi_code = ucode;
	trapsignal(td, &ksi);

out:
	if (user) {
		userret(td, tf);
		do_ast(tf);
	}
	return;
}

/*
 * Handle break instruction based system calls.
 */
void
break_syscall(struct trapframe *tf)
{
	uint64_t *bsp, *tfp;
	uint64_t iip, psr;
	int error, nargs;

	/* Save address of break instruction. */
	iip = tf->tf_special.iip;
	psr = tf->tf_special.psr;

	/* Advance to the next instruction. */
	tf->tf_special.psr += IA64_PSR_RI_1;
	if ((tf->tf_special.psr & IA64_PSR_RI) > IA64_PSR_RI_2) {
		tf->tf_special.iip += 16;
		tf->tf_special.psr &= ~IA64_PSR_RI;
	}

	/*
	 * Copy the arguments on the register stack into the trapframe
	 * to avoid having interleaved NaT collections.
	 */
	tfp = &tf->tf_scratch.gr16;
	nargs = tf->tf_special.cfm & 0x7f;
	bsp = (uint64_t*)(curthread->td_kstack + tf->tf_special.ndirty +
	    (tf->tf_special.bspstore & 0x1ffUL));
	bsp -= (((uintptr_t)bsp & 0x1ff) < (nargs << 3)) ? (nargs + 1): nargs;
	while (nargs--) {
		*tfp++ = *bsp++;
		if (((uintptr_t)bsp & 0x1ff) == 0x1f8)
			bsp++;
	}
	error = syscall(tf);
	if (error == ERESTART) {
		tf->tf_special.iip = iip;
		tf->tf_special.psr = psr;
	}

	do_ast(tf);
}

int
cpu_fetch_syscall_args(struct thread *td, struct syscall_args *sa)
{
	struct proc *p;
	struct trapframe *tf;

	p = td->td_proc;
	tf = td->td_frame;

	sa->code = tf->tf_scratch.gr15;
	sa->args = &tf->tf_scratch.gr16;

	/*
	 * syscall() and __syscall() are handled the same on
	 * the ia64, as everything is 64-bit aligned, anyway.
	 */
	if (sa->code == SYS_syscall || sa->code == SYS___syscall) {
		/*
		 * Code is first argument, followed by actual args.
		 */
		sa->code = sa->args[0];
		sa->args++;
	}

 	if (p->p_sysent->sv_mask)
 		sa->code &= p->p_sysent->sv_mask;
 	if (sa->code >= p->p_sysent->sv_size)
 		sa->callp = &p->p_sysent->sv_table[0];
 	else
		sa->callp = &p->p_sysent->sv_table[sa->code];
	sa->narg = sa->callp->sy_narg;

	td->td_retval[0] = 0;
	td->td_retval[1] = 0;

	return (0);
}

#include "../../kern/subr_syscall.c"

/*
 * Process a system call.
 *
 * See syscall.s for details as to how we get here. In order to support
 * the ERESTART case, we return the error to our caller. They deal with
 * the hairy details.
 */
int
syscall(struct trapframe *tf)
{
	struct syscall_args sa;
	struct thread *td;
	int error;

	td = curthread;
	td->td_frame = tf;

	ia64_set_fpsr(IA64_FPSR_DEFAULT);
	tf->tf_scratch.gr10 = EJUSTRETURN;

	error = syscallenter(td, &sa);
	syscallret(td, error, &sa);

	return (error);
}
