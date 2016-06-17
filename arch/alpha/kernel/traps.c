/*
 * arch/alpha/kernel/traps.c
 *
 * (C) Copyright 1994 Linus Torvalds
 */

/*
 * This file initializes the trap entry points
 */

#include <linux/config.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/smp_lock.h>

#include <asm/gentrap.h>
#include <asm/uaccess.h>
#include <asm/unaligned.h>
#include <asm/sysinfo.h>
#include <asm/hwrpb.h>
#include <asm/mmu_context.h>

#include "proto.h"

/* data/code implementing a work-around for some SRMs which
   mishandle opDEC faults
*/
static int opDEC_testing = 0;
static int opDEC_fix = 0;
static int opDEC_checked = 0;
static unsigned long opDEC_test_pc = 0;

static void
opDEC_check(void)
{
	unsigned long test_pc;

	if (opDEC_checked) return;

	lock_kernel();
	opDEC_testing = 1;

	__asm__ __volatile__(
		"       br      %0,1f\n"
		"1:     addq    %0,8,%0\n"
		"       stq     %0,%1\n"
		"       cvttq/svm $f31,$f31\n"
		: "=&r"(test_pc), "=m"(opDEC_test_pc)
		: );

	opDEC_testing = 0;
	opDEC_checked = 1;
	unlock_kernel();
}

void
dik_show_regs(struct pt_regs *regs, unsigned long *r9_15)
{
	printk("pc = [<%016lx>]  ra = [<%016lx>]  ps = %04lx    %s\n",
	       regs->pc, regs->r26, regs->ps, print_tainted());
	printk("v0 = %016lx  t0 = %016lx  t1 = %016lx\n",
	       regs->r0, regs->r1, regs->r2);
	printk("t2 = %016lx  t3 = %016lx  t4 = %016lx\n",
 	       regs->r3, regs->r4, regs->r5);
	printk("t5 = %016lx  t6 = %016lx  t7 = %016lx\n",
	       regs->r6, regs->r7, regs->r8);

	if (r9_15) {
		printk("s0 = %016lx  s1 = %016lx  s2 = %016lx\n",
		       r9_15[9], r9_15[10], r9_15[11]);
		printk("s3 = %016lx  s4 = %016lx  s5 = %016lx\n",
		       r9_15[12], r9_15[13], r9_15[14]);
		printk("s6 = %016lx\n", r9_15[15]);
	}

	printk("a0 = %016lx  a1 = %016lx  a2 = %016lx\n",
	       regs->r16, regs->r17, regs->r18);
	printk("a3 = %016lx  a4 = %016lx  a5 = %016lx\n",
 	       regs->r19, regs->r20, regs->r21);
 	printk("t8 = %016lx  t9 = %016lx  t10= %016lx\n",
	       regs->r22, regs->r23, regs->r24);
	printk("t11= %016lx  pv = %016lx  at = %016lx\n",
	       regs->r25, regs->r27, regs->r28);
	printk("gp = %016lx  sp = %p\n", regs->gp, regs+1);
#if 0
__halt();
#endif
}

#if 0
static char * ireg_name[] = {"v0", "t0", "t1", "t2", "t3", "t4", "t5", "t6",
			   "t7", "s0", "s1", "s2", "s3", "s4", "s5", "s6",
			   "a0", "a1", "a2", "a3", "a4", "a5", "t8", "t9",
			   "t10", "t11", "ra", "pv", "at", "gp", "sp", "zero"};
#endif

static void
dik_show_code(unsigned int *pc)
{
	long i;

	printk("Code:");
	for (i = -6; i < 2; i++) {
		unsigned int insn;
		if (__get_user(insn, pc+i))
			break;
		printk("%c%08x%c", i ? ' ' : '<', insn, i ? ' ' : '>');
	}
	printk("\n");
}

static void
dik_show_trace(unsigned long *sp)
{
	long i = 0;
	printk("Trace:");
	while (0x1ff8 & (unsigned long) sp) {
		extern unsigned long _stext, _etext;
		unsigned long tmp = *sp;
		sp++;
		if (tmp < (unsigned long) &_stext)
			continue;
		if (tmp >= (unsigned long) &_etext)
			continue;
		printk("%lx%c", tmp, ' ');
		if (i > 40) {
			printk(" ...");
			break;
		}
	}
	printk("\n");
}

void show_trace_task(struct task_struct * tsk)
{
	struct thread_struct * thread = &tsk->thread;
	unsigned long fp, sp = thread->ksp, base = (unsigned long) thread;
 
	if (sp > base && sp+6*8 < base + 16*1024) {
		fp = ((unsigned long*)sp)[6];
		if (fp > sp && fp < base + 16*1024)
			dik_show_trace((unsigned long *)fp);
	}
}

int kstack_depth_to_print = 24;

void show_stack(unsigned long *sp)
{
	unsigned long *stack;
	int i;

	/*
	 * debugging aid: "show_stack(NULL);" prints the
	 * back trace for this cpu.
	 */
	if(sp==NULL)
		sp=(unsigned long*)&sp;

	stack = sp;
	for(i=0; i < kstack_depth_to_print; i++) {
		if (((long) stack & (THREAD_SIZE-1)) == 0)
			break;
		if (i && ((i % 4) == 0))
			printk("\n       ");
		printk("%016lx ", *stack++);
	}
	printk("\n");
	dik_show_trace(sp);
}

void dump_stack(void)
{
	show_stack(NULL);
}

void
die_if_kernel(char * str, struct pt_regs *regs, long err, unsigned long *r9_15)
{
	if (regs->ps & 8)
		return;
#ifdef CONFIG_SMP
	printk("CPU %d ", hard_smp_processor_id());
#endif
	printk("%s(%d): %s %ld\n", current->comm, current->pid, str, err);
	dik_show_regs(regs, r9_15);
	dik_show_trace((unsigned long *)(regs+1));
	dik_show_code((unsigned int *)regs->pc);

	if (current->thread.flags & (1UL << 63)) {
		printk("die_if_kernel recursion detected.\n");
		sti();
		while (1);
	}
	current->thread.flags |= (1UL << 63);
	do_exit(SIGSEGV);
}

#ifndef CONFIG_MATHEMU
static long dummy_emul(void) { return 0; }
long (*alpha_fp_emul_imprecise)(struct pt_regs *regs, unsigned long writemask)
  = (void *)dummy_emul;
long (*alpha_fp_emul) (unsigned long pc)
  = (void *)dummy_emul;
#else
long alpha_fp_emul_imprecise(struct pt_regs *regs, unsigned long writemask);
long alpha_fp_emul (unsigned long pc);
#endif

asmlinkage void
do_entArith(unsigned long summary, unsigned long write_mask,
	    unsigned long a2, unsigned long a3, unsigned long a4,
	    unsigned long a5, struct pt_regs regs)
{
	if (summary & 1) {
		/* Software-completion summary bit is set, so try to
		   emulate the instruction.  */
		if (!amask(AMASK_PRECISE_TRAP)) {
			/* 21264 (except pass 1) has precise exceptions.  */
			if (alpha_fp_emul(regs.pc - 4))
				return;
		} else {
			if (alpha_fp_emul_imprecise(&regs, write_mask))
				return;
		}
	}

#if 0
	printk("%s: arithmetic trap at %016lx: %02lx %016lx\n",
		current->comm, regs.pc, summary, write_mask);
#endif
	die_if_kernel("Arithmetic fault", &regs, 0, 0);
	send_sig(SIGFPE, current, 1);
}

asmlinkage void
do_entIF(unsigned long type, unsigned long a1,
	 unsigned long a2, unsigned long a3, unsigned long a4,
	 unsigned long a5, struct pt_regs regs)
{
	if (!opDEC_testing || type != 4) {
		die_if_kernel((type == 1 ? "Kernel Bug" : "Instruction fault"),
		      &regs, type, 0);
	}

	switch (type) {
	      case 0: /* breakpoint */
		if (ptrace_cancel_bpt(current)) {
			regs.pc -= 4;	/* make pc point to former bpt */
		}
		send_sig(SIGTRAP, current, 1);
		return;

	      case 1: /* bugcheck */
		send_sig(SIGTRAP, current, 1);
		return;

	      case 2: /* gentrap */
		/*
		 * The exception code should be passed on to the signal
		 * handler as the second argument.  Linux doesn't do that
		 * yet (also notice that Linux *always* behaves like
		 * DEC Unix with SA_SIGINFO off; see DEC Unix man page
		 * for sigaction(2)).
		 */
		switch ((long) regs.r16) {
		      case GEN_INTOVF: case GEN_INTDIV: case GEN_FLTOVF:
		      case GEN_FLTDIV: case GEN_FLTUND: case GEN_FLTINV:
		      case GEN_FLTINE: case GEN_ROPRAND:
			send_sig(SIGFPE, current, 1);
			return;

		      case GEN_DECOVF:
		      case GEN_DECDIV:
		      case GEN_DECINV:
		      case GEN_ASSERTERR:
		      case GEN_NULPTRERR:
		      case GEN_STKOVF:
		      case GEN_STRLENERR:
		      case GEN_SUBSTRERR:
		      case GEN_RANGERR:
		      case GEN_SUBRNG:
		      case GEN_SUBRNG1:
		      case GEN_SUBRNG2:
		      case GEN_SUBRNG3:
		      case GEN_SUBRNG4:
		      case GEN_SUBRNG5:
		      case GEN_SUBRNG6:
		      case GEN_SUBRNG7:
			send_sig(SIGTRAP, current, 1);
			return;
		}
		break;

	      case 4: /* opDEC */
		if (implver() == IMPLVER_EV4) {
			/* The some versions of SRM do not handle
			   the opDEC properly - they return the PC of the
			   opDEC fault, not the instruction after as the
			   Alpha architecture requires.  Here we fix it up.
			   We do this by intentionally causing an opDEC
			   fault during the boot sequence and testing if
			   we get the correct PC.  If not, we set a flag
			   to correct it every time through.
			*/
			if (opDEC_testing) {
				if (regs.pc == opDEC_test_pc) {
					opDEC_fix = 4;
					regs.pc += 4;
					printk("opDEC fixup enabled.\n");
				}
				return;
			}
			regs.pc += opDEC_fix; 
			
			/* EV4 does not implement anything except normal
			   rounding.  Everything else will come here as
			   an illegal instruction.  Emulate them.  */
			if (alpha_fp_emul(regs.pc-4))
				return;
		}
		break;

	      case 3: /* FEN fault */
		/* Irritating users can call PAL_clrfen to disable the
		   FPU for the process.  The kernel will then trap in
		   do_switch_stack and undo_switch_stack when we try
		   to save and restore the FP registers.

		   Given that GCC by default generates code that uses the
		   FP registers, PAL_clrfen is not useful except for DoS
		   attacks.  So turn the bleeding FPU back on and be done
		   with it.  */
		current->thread.pal_flags |= 1;
		__reload_thread(&current->thread);
		return;

	      case 5: /* illoc */
	      default: /* unexpected instruction-fault type */
		      ;
	}
	send_sig(SIGILL, current, 1);
}

/* There is an ifdef in the PALcode in MILO that enables a 
   "kernel debugging entry point" as an unpriviledged call_pal.

   We don't want to have anything to do with it, but unfortunately
   several versions of MILO included in distributions have it enabled,
   and if we don't put something on the entry point we'll oops.  */

asmlinkage void
do_entDbg(unsigned long type, unsigned long a1,
	  unsigned long a2, unsigned long a3, unsigned long a4,
	  unsigned long a5, struct pt_regs regs)
{
	die_if_kernel("Instruction fault", &regs, type, 0);
	force_sig(SIGILL, current);
}


/*
 * entUna has a different register layout to be reasonably simple. It
 * needs access to all the integer registers (the kernel doesn't use
 * fp-regs), and it needs to have them in order for simpler access.
 *
 * Due to the non-standard register layout (and because we don't want
 * to handle floating-point regs), user-mode unaligned accesses are
 * handled separately by do_entUnaUser below.
 *
 * Oh, btw, we don't handle the "gp" register correctly, but if we fault
 * on a gp-register unaligned load/store, something is _very_ wrong
 * in the kernel anyway..
 */
struct allregs {
	unsigned long regs[32];
	unsigned long ps, pc, gp, a0, a1, a2;
};

struct unaligned_stat {
	unsigned long count, va, pc;
} unaligned[2];


/* Macro for exception fixup code to access integer registers.  */
#define una_reg(r)  (regs.regs[(r) >= 16 && (r) <= 18 ? (r)+19 : (r)])


asmlinkage void
do_entUna(void * va, unsigned long opcode, unsigned long reg,
	  unsigned long a3, unsigned long a4, unsigned long a5,
	  struct allregs regs)
{
	long error, tmp1, tmp2, tmp3, tmp4;
	unsigned long pc = regs.pc - 4;
	unsigned fixup;

	unaligned[0].count++;
	unaligned[0].va = (unsigned long) va;
	unaligned[0].pc = pc;

	/* We don't want to use the generic get/put unaligned macros as
	   we want to trap exceptions.  Only if we actually get an
	   exception will we decide whether we should have caught it.  */

	switch (opcode) {
	case 0x0c: /* ldwu */
		__asm__ __volatile__(
		"1:	ldq_u %1,0(%3)\n"
		"2:	ldq_u %2,1(%3)\n"
		"	extwl %1,%3,%1\n"
		"	extwh %2,%3,%2\n"
		"3:\n"
		".section __ex_table,\"a\"\n"
		"	.gprel32 1b\n"
		"	lda %1,3b-1b(%0)\n"
		"	.gprel32 2b\n"
		"	lda %2,3b-2b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2)
			: "r"(va), "0"(0));
		if (error)
			goto got_exception;
		una_reg(reg) = tmp1|tmp2;
		return;

	case 0x28: /* ldl */
		__asm__ __volatile__(
		"1:	ldq_u %1,0(%3)\n"
		"2:	ldq_u %2,3(%3)\n"
		"	extll %1,%3,%1\n"
		"	extlh %2,%3,%2\n"
		"3:\n"
		".section __ex_table,\"a\"\n"
		"	.gprel32 1b\n"
		"	lda %1,3b-1b(%0)\n"
		"	.gprel32 2b\n"
		"	lda %2,3b-2b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2)
			: "r"(va), "0"(0));
		if (error)
			goto got_exception;
		una_reg(reg) = (int)(tmp1|tmp2);
		return;

	case 0x29: /* ldq */
		__asm__ __volatile__(
		"1:	ldq_u %1,0(%3)\n"
		"2:	ldq_u %2,7(%3)\n"
		"	extql %1,%3,%1\n"
		"	extqh %2,%3,%2\n"
		"3:\n"
		".section __ex_table,\"a\"\n"
		"	.gprel32 1b\n"
		"	lda %1,3b-1b(%0)\n"
		"	.gprel32 2b\n"
		"	lda %2,3b-2b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2)
			: "r"(va), "0"(0));
		if (error)
			goto got_exception;
		una_reg(reg) = tmp1|tmp2;
		return;

	/* Note that the store sequences do not indicate that they change
	   memory because it _should_ be affecting nothing in this context.
	   (Otherwise we have other, much larger, problems.)  */
	case 0x0d: /* stw */
		__asm__ __volatile__(
		"1:	ldq_u %2,1(%5)\n"
		"2:	ldq_u %1,0(%5)\n"
		"	inswh %6,%5,%4\n"
		"	inswl %6,%5,%3\n"
		"	mskwh %2,%5,%2\n"
		"	mskwl %1,%5,%1\n"
		"	or %2,%4,%2\n"
		"	or %1,%3,%1\n"
		"3:	stq_u %2,1(%5)\n"
		"4:	stq_u %1,0(%5)\n"
		"5:\n"
		".section __ex_table,\"a\"\n"
		"	.gprel32 1b\n"
		"	lda %2,5b-1b(%0)\n"
		"	.gprel32 2b\n"
		"	lda %1,5b-2b(%0)\n"
		"	.gprel32 3b\n"
		"	lda $31,5b-3b(%0)\n"
		"	.gprel32 4b\n"
		"	lda $31,5b-4b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2),
			  "=&r"(tmp3), "=&r"(tmp4)
			: "r"(va), "r"(una_reg(reg)), "0"(0));
		if (error)
			goto got_exception;
		return;

	case 0x2c: /* stl */
		__asm__ __volatile__(
		"1:	ldq_u %2,3(%5)\n"
		"2:	ldq_u %1,0(%5)\n"
		"	inslh %6,%5,%4\n"
		"	insll %6,%5,%3\n"
		"	msklh %2,%5,%2\n"
		"	mskll %1,%5,%1\n"
		"	or %2,%4,%2\n"
		"	or %1,%3,%1\n"
		"3:	stq_u %2,3(%5)\n"
		"4:	stq_u %1,0(%5)\n"
		"5:\n"
		".section __ex_table,\"a\"\n"
		"	.gprel32 1b\n"
		"	lda %2,5b-1b(%0)\n"
		"	.gprel32 2b\n"
		"	lda %1,5b-2b(%0)\n"
		"	.gprel32 3b\n"
		"	lda $31,5b-3b(%0)\n"
		"	.gprel32 4b\n"
		"	lda $31,5b-4b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2),
			  "=&r"(tmp3), "=&r"(tmp4)
			: "r"(va), "r"(una_reg(reg)), "0"(0));
		if (error)
			goto got_exception;
		return;

	case 0x2d: /* stq */
		__asm__ __volatile__(
		"1:	ldq_u %2,7(%5)\n"
		"2:	ldq_u %1,0(%5)\n"
		"	insqh %6,%5,%4\n"
		"	insql %6,%5,%3\n"
		"	mskqh %2,%5,%2\n"
		"	mskql %1,%5,%1\n"
		"	or %2,%4,%2\n"
		"	or %1,%3,%1\n"
		"3:	stq_u %2,7(%5)\n"
		"4:	stq_u %1,0(%5)\n"
		"5:\n"
		".section __ex_table,\"a\"\n\t"
		"	.gprel32 1b\n"
		"	lda %2,5b-1b(%0)\n"
		"	.gprel32 2b\n"
		"	lda %1,5b-2b(%0)\n"
		"	.gprel32 3b\n"
		"	lda $31,5b-3b(%0)\n"
		"	.gprel32 4b\n"
		"	lda $31,5b-4b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2),
			  "=&r"(tmp3), "=&r"(tmp4)
			: "r"(va), "r"(una_reg(reg)), "0"(0));
		if (error)
			goto got_exception;
		return;
	}

	lock_kernel();
	printk("Bad unaligned kernel access at %016lx: %p %lx %ld\n",
		pc, va, opcode, reg);
	do_exit(SIGSEGV);

got_exception:
	/* Ok, we caught the exception, but we don't want it.  Is there
	   someone to pass it along to?  */
	if ((fixup = search_exception_table(pc, regs.gp)) != 0) {
		unsigned long newpc;
		newpc = fixup_exception(una_reg, fixup, pc);

		printk("Forwarding unaligned exception at %lx (%lx)\n",
		       pc, newpc);

		(&regs)->pc = newpc;
		return;
	}

	/*
	 * Yikes!  No one to forward the exception to.
	 * Since the registers are in a weird format, dump them ourselves.
 	 */
	lock_kernel();

	printk("%s(%d): unhandled unaligned exception\n",
	       current->comm, current->pid);

	printk("pc = [<%016lx>]  ra = [<%016lx>]  ps = %04lx\n",
	       pc, una_reg(26), regs.ps);
	printk("r0 = %016lx  r1 = %016lx  r2 = %016lx\n",
	       una_reg(0), una_reg(1), una_reg(2));
	printk("r3 = %016lx  r4 = %016lx  r5 = %016lx\n",
 	       una_reg(3), una_reg(4), una_reg(5));
	printk("r6 = %016lx  r7 = %016lx  r8 = %016lx\n",
	       una_reg(6), una_reg(7), una_reg(8));
	printk("r9 = %016lx  r10= %016lx  r11= %016lx\n",
	       una_reg(9), una_reg(10), una_reg(11));
	printk("r12= %016lx  r13= %016lx  r14= %016lx\n",
	       una_reg(12), una_reg(13), una_reg(14));
	printk("r15= %016lx\n", una_reg(15));
	printk("r16= %016lx  r17= %016lx  r18= %016lx\n",
	       una_reg(16), una_reg(17), una_reg(18));
	printk("r19= %016lx  r20= %016lx  r21= %016lx\n",
 	       una_reg(19), una_reg(20), una_reg(21));
 	printk("r22= %016lx  r23= %016lx  r24= %016lx\n",
	       una_reg(22), una_reg(23), una_reg(24));
	printk("r25= %016lx  r27= %016lx  r28= %016lx\n",
	       una_reg(25), una_reg(27), una_reg(28));
	printk("gp = %016lx  sp = %p\n", regs.gp, &regs+1);

	dik_show_code((unsigned int *)pc);
	dik_show_trace((unsigned long *)(&regs+1));

	if (current->thread.flags & (1UL << 63)) {
		printk("die_if_kernel recursion detected.\n");
		sti();
		while (1);
	}
	current->thread.flags |= (1UL << 63);
	do_exit(SIGSEGV);
}

/*
 * Convert an s-floating point value in memory format to the
 * corresponding value in register format.  The exponent
 * needs to be remapped to preserve non-finite values
 * (infinities, not-a-numbers, denormals).
 */
static inline unsigned long
s_mem_to_reg (unsigned long s_mem)
{
	unsigned long frac    = (s_mem >>  0) & 0x7fffff;
	unsigned long sign    = (s_mem >> 31) & 0x1;
	unsigned long exp_msb = (s_mem >> 30) & 0x1;
	unsigned long exp_low = (s_mem >> 23) & 0x7f;
	unsigned long exp;

	exp = (exp_msb << 10) | exp_low;	/* common case */
	if (exp_msb) {
		if (exp_low == 0x7f) {
			exp = 0x7ff;
		}
	} else {
		if (exp_low == 0x00) {
			exp = 0x000;
		} else {
			exp |= (0x7 << 7);
		}
	}
	return (sign << 63) | (exp << 52) | (frac << 29);
}

/*
 * Convert an s-floating point value in register format to the
 * corresponding value in memory format.
 */
static inline unsigned long
s_reg_to_mem (unsigned long s_reg)
{
	return ((s_reg >> 62) << 30) | ((s_reg << 5) >> 34);
}

/*
 * Handle user-level unaligned fault.  Handling user-level unaligned
 * faults is *extremely* slow and produces nasty messages.  A user
 * program *should* fix unaligned faults ASAP.
 *
 * Notice that we have (almost) the regular kernel stack layout here,
 * so finding the appropriate registers is a little more difficult
 * than in the kernel case.
 *
 * Finally, we handle regular integer load/stores only.  In
 * particular, load-linked/store-conditionally and floating point
 * load/stores are not supported.  The former make no sense with
 * unaligned faults (they are guaranteed to fail) and I don't think
 * the latter will occur in any decent program.
 *
 * Sigh. We *do* have to handle some FP operations, because GCC will
 * uses them as temporary storage for integer memory to memory copies.
 * However, we need to deal with stt/ldt and sts/lds only.
 */

#define OP_INT_MASK	( 1L << 0x28 | 1L << 0x2c   /* ldl stl */	\
			| 1L << 0x29 | 1L << 0x2d   /* ldq stq */	\
			| 1L << 0x0c | 1L << 0x0d   /* ldwu stw */	\
			| 1L << 0x0a | 1L << 0x0e ) /* ldbu stb */

#define OP_WRITE_MASK	( 1L << 0x26 | 1L << 0x27   /* sts stt */	\
			| 1L << 0x2c | 1L << 0x2d   /* stl stq */	\
			| 1L << 0x0d | 1L << 0x0e ) /* stw stb */

#define R(x)	((size_t) &((struct pt_regs *)0)->x)

static int unauser_reg_offsets[32] = {
	R(r0), R(r1), R(r2), R(r3), R(r4), R(r5), R(r6), R(r7), R(r8),
	/* r9 ... r15 are stored in front of regs.  */
	-56, -48, -40, -32, -24, -16, -8,
	R(r16), R(r17), R(r18),
	R(r19), R(r20), R(r21), R(r22), R(r23), R(r24), R(r25), R(r26),
	R(r27), R(r28), R(gp),
	0, 0
};

#undef R

asmlinkage void
do_entUnaUser(void * va, unsigned long opcode,
	      unsigned long reg, struct pt_regs *regs)
{
	static int cnt = 0;
	static long last_time = 0;

	unsigned long tmp1, tmp2, tmp3, tmp4;
	unsigned long fake_reg, *reg_addr = &fake_reg;
	unsigned long uac_bits;
	long error;

	/* Check the UAC bits to decide what the user wants us to do
	   with the unaliged access.  */

	uac_bits = (current->thread.flags >> UAC_SHIFT) & UAC_BITMASK;
	if (!(uac_bits & UAC_NOPRINT)) {
		if (cnt >= 5 && jiffies - last_time > 5*HZ) {
			cnt = 0;
		}
		if (++cnt < 5) {
			printk("%s(%d): unaligned trap at %016lx: %p %lx %ld\n",
			       current->comm, current->pid,
			       regs->pc - 4, va, opcode, reg);
		}
		last_time = jiffies;
	}
	if (uac_bits & UAC_SIGBUS) {
		goto give_sigbus;
	}
	if (uac_bits & UAC_NOFIX) {
		/* Not sure why you'd want to use this, but... */
		return;
	}

	/* Don't bother reading ds in the access check since we already
	   know that this came from the user.  Also rely on the fact that
	   the page at TASK_SIZE is unmapped and so can't be touched anyway. */
	if (!__access_ok((unsigned long)va, 0, USER_DS))
		goto give_sigsegv;

	++unaligned[1].count;
	unaligned[1].va = (unsigned long)va;
	unaligned[1].pc = regs->pc - 4;

	if ((1L << opcode) & OP_INT_MASK) {
		/* it's an integer load/store */
		if (reg < 30) {
			reg_addr = (unsigned long *)
			  ((char *)regs + unauser_reg_offsets[reg]);
		} else if (reg == 30) {
			/* usp in PAL regs */
			fake_reg = rdusp();
		} else {
			/* zero "register" */
			fake_reg = 0;
		}
	}

	/* We don't want to use the generic get/put unaligned macros as
	   we want to trap exceptions.  Only if we actually get an
	   exception will we decide whether we should have caught it.  */

	switch (opcode) {
	case 0x0c: /* ldwu */
		__asm__ __volatile__(
		"1:	ldq_u %1,0(%3)\n"
		"2:	ldq_u %2,1(%3)\n"
		"	extwl %1,%3,%1\n"
		"	extwh %2,%3,%2\n"
		"3:\n"
		".section __ex_table,\"a\"\n"
		"	.gprel32 1b\n"
		"	lda %1,3b-1b(%0)\n"
		"	.gprel32 2b\n"
		"	lda %2,3b-2b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2)
			: "r"(va), "0"(0));
		if (error)
			goto give_sigsegv;
		*reg_addr = tmp1|tmp2;
		break;

	case 0x22: /* lds */
		__asm__ __volatile__(
		"1:	ldq_u %1,0(%3)\n"
		"2:	ldq_u %2,3(%3)\n"
		"	extll %1,%3,%1\n"
		"	extlh %2,%3,%2\n"
		"3:\n"
		".section __ex_table,\"a\"\n"
		"	.gprel32 1b\n"
		"	lda %1,3b-1b(%0)\n"
		"	.gprel32 2b\n"
		"	lda %2,3b-2b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2)
			: "r"(va), "0"(0));
		if (error)
			goto give_sigsegv;
		alpha_write_fp_reg(reg, s_mem_to_reg((int)(tmp1|tmp2)));
		return;

	case 0x23: /* ldt */
		__asm__ __volatile__(
		"1:	ldq_u %1,0(%3)\n"
		"2:	ldq_u %2,7(%3)\n"
		"	extql %1,%3,%1\n"
		"	extqh %2,%3,%2\n"
		"3:\n"
		".section __ex_table,\"a\"\n"
		"	.gprel32 1b\n"
		"	lda %1,3b-1b(%0)\n"
		"	.gprel32 2b\n"
		"	lda %2,3b-2b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2)
			: "r"(va), "0"(0));
		if (error)
			goto give_sigsegv;
		alpha_write_fp_reg(reg, tmp1|tmp2);
		return;

	case 0x28: /* ldl */
		__asm__ __volatile__(
		"1:	ldq_u %1,0(%3)\n"
		"2:	ldq_u %2,3(%3)\n"
		"	extll %1,%3,%1\n"
		"	extlh %2,%3,%2\n"
		"3:\n"
		".section __ex_table,\"a\"\n"
		"	.gprel32 1b\n"
		"	lda %1,3b-1b(%0)\n"
		"	.gprel32 2b\n"
		"	lda %2,3b-2b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2)
			: "r"(va), "0"(0));
		if (error)
			goto give_sigsegv;
		*reg_addr = (int)(tmp1|tmp2);
		break;

	case 0x29: /* ldq */
		__asm__ __volatile__(
		"1:	ldq_u %1,0(%3)\n"
		"2:	ldq_u %2,7(%3)\n"
		"	extql %1,%3,%1\n"
		"	extqh %2,%3,%2\n"
		"3:\n"
		".section __ex_table,\"a\"\n"
		"	.gprel32 1b\n"
		"	lda %1,3b-1b(%0)\n"
		"	.gprel32 2b\n"
		"	lda %2,3b-2b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2)
			: "r"(va), "0"(0));
		if (error)
			goto give_sigsegv;
		*reg_addr = tmp1|tmp2;
		break;

	/* Note that the store sequences do not indicate that they change
	   memory because it _should_ be affecting nothing in this context.
	   (Otherwise we have other, much larger, problems.)  */
	case 0x0d: /* stw */
		__asm__ __volatile__(
		"1:	ldq_u %2,1(%5)\n"
		"2:	ldq_u %1,0(%5)\n"
		"	inswh %6,%5,%4\n"
		"	inswl %6,%5,%3\n"
		"	mskwh %2,%5,%2\n"
		"	mskwl %1,%5,%1\n"
		"	or %2,%4,%2\n"
		"	or %1,%3,%1\n"
		"3:	stq_u %2,1(%5)\n"
		"4:	stq_u %1,0(%5)\n"
		"5:\n"
		".section __ex_table,\"a\"\n"
		"	.gprel32 1b\n"
		"	lda %2,5b-1b(%0)\n"
		"	.gprel32 2b\n"
		"	lda %1,5b-2b(%0)\n"
		"	.gprel32 3b\n"
		"	lda $31,5b-3b(%0)\n"
		"	.gprel32 4b\n"
		"	lda $31,5b-4b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2),
			  "=&r"(tmp3), "=&r"(tmp4)
			: "r"(va), "r"(*reg_addr), "0"(0));
		if (error)
			goto give_sigsegv;
		return;

	case 0x26: /* sts */
		fake_reg = s_reg_to_mem(alpha_read_fp_reg(reg));
		/* FALLTHRU */

	case 0x2c: /* stl */
		__asm__ __volatile__(
		"1:	ldq_u %2,3(%5)\n"
		"2:	ldq_u %1,0(%5)\n"
		"	inslh %6,%5,%4\n"
		"	insll %6,%5,%3\n"
		"	msklh %2,%5,%2\n"
		"	mskll %1,%5,%1\n"
		"	or %2,%4,%2\n"
		"	or %1,%3,%1\n"
		"3:	stq_u %2,3(%5)\n"
		"4:	stq_u %1,0(%5)\n"
		"5:\n"
		".section __ex_table,\"a\"\n"
		"	.gprel32 1b\n"
		"	lda %2,5b-1b(%0)\n"
		"	.gprel32 2b\n"
		"	lda %1,5b-2b(%0)\n"
		"	.gprel32 3b\n"
		"	lda $31,5b-3b(%0)\n"
		"	.gprel32 4b\n"
		"	lda $31,5b-4b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2),
			  "=&r"(tmp3), "=&r"(tmp4)
			: "r"(va), "r"(*reg_addr), "0"(0));
		if (error)
			goto give_sigsegv;
		return;

	case 0x27: /* stt */
		fake_reg = alpha_read_fp_reg(reg);
		/* FALLTHRU */

	case 0x2d: /* stq */
		__asm__ __volatile__(
		"1:	ldq_u %2,7(%5)\n"
		"2:	ldq_u %1,0(%5)\n"
		"	insqh %6,%5,%4\n"
		"	insql %6,%5,%3\n"
		"	mskqh %2,%5,%2\n"
		"	mskql %1,%5,%1\n"
		"	or %2,%4,%2\n"
		"	or %1,%3,%1\n"
		"3:	stq_u %2,7(%5)\n"
		"4:	stq_u %1,0(%5)\n"
		"5:\n"
		".section __ex_table,\"a\"\n\t"
		"	.gprel32 1b\n"
		"	lda %2,5b-1b(%0)\n"
		"	.gprel32 2b\n"
		"	lda %1,5b-2b(%0)\n"
		"	.gprel32 3b\n"
		"	lda $31,5b-3b(%0)\n"
		"	.gprel32 4b\n"
		"	lda $31,5b-4b(%0)\n"
		".previous"
			: "=r"(error), "=&r"(tmp1), "=&r"(tmp2),
			  "=&r"(tmp3), "=&r"(tmp4)
			: "r"(va), "r"(*reg_addr), "0"(0));
		if (error)
			goto give_sigsegv;
		return;

	default:
		/* What instruction were you trying to use, exactly?  */
		goto give_sigbus;
	}

	/* Only integer loads should get here; everyone else returns early. */
	if (reg == 30)
		wrusp(fake_reg);
	return;

give_sigsegv:
	regs->pc -= 4;  /* make pc point to faulting insn */
	send_sig(SIGSEGV, current, 1);
	return;

give_sigbus:
	regs->pc -= 4;
	send_sig(SIGBUS, current, 1);
	return;
}

/*
 * Unimplemented system calls.
 */
asmlinkage long
alpha_ni_syscall(unsigned long a0, unsigned long a1, unsigned long a2,
		 unsigned long a3, unsigned long a4, unsigned long a5,
		 struct pt_regs regs)
{
	/* We only get here for OSF system calls, minus #112;
	   the rest go to sys_ni_syscall.  */
#if 0
	printk("<sc %ld(%lx,%lx,%lx)>", regs.r0, a0, a1, a2);
#endif
	return -ENOSYS;
}

void
trap_init(void)
{
	/* Tell PAL-code what global pointer we want in the kernel.  */
	register unsigned long gptr __asm__("$29");
	wrkgp(gptr);

	wrent(entArith, 1);
	wrent(entMM, 2);
	wrent(entIF, 3);
	wrent(entUna, 4);
	wrent(entSys, 5);
	wrent(entDbg, 6);

	/* Hack for Multia (UDB) and JENSEN: some of their SRMs have
	 * a bug in the handling of the opDEC fault.  Fix it up if so.
	 */
	if (implver() == IMPLVER_EV4) {
		opDEC_check();
	}
}
