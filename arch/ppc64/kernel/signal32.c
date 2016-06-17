/*
 * signal32.c: Support 32bit signal syscalls.
 *
 * Copyright (C) 2001 IBM
 * Copyright (C) 1997,1998 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 * Copyright (C) 1997 David S. Miller (davem@caip.rutgers.edu)
 *
 * These routines maintain argument size conversion between 32bit and 64bit
 * environment.
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <asm/ptrace.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h> 
#include <linux/mm.h> 
#include <linux/file.h> 
#include <linux/signal.h>
#include <linux/utime.h>
#include <linux/resource.h>
#include <linux/times.h>
#include <linux/utsname.h>
#include <linux/timex.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/sem.h>
#include <linux/msg.h>
#include <linux/shm.h>
#include <linux/slab.h>
#include <linux/uio.h>
#include <linux/nfs_fs.h>
#include <linux/smb_fs.h>
#include <linux/smb_mount.h>
#include <linux/ncp_fs.h>
#include <linux/quota.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/personality.h>
#include <linux/stat.h>
#include <linux/filter.h>
#include <asm/types.h>
#include <asm/ipc.h>
#include <asm/uaccess.h>
#include <linux/elf.h>
#include <asm/ppc32.h>
#include <asm/ppcdebug.h>
#include <asm/unistd.h>
#include <asm/ucontext.h>

#define _BLOCKABLE (~(sigmask(SIGKILL) | sigmask(SIGSTOP)))
/* 
 * These are the flags in the MSR that the user is allowed to change
 * by modifying the saved value of the MSR on the stack.  SE and BE
 * should not be in this list since gdb may want to change these.  I.e,
 * you should be able to step out of a signal handler to see what
 * instruction executes next after the signal handler completes.
 * Alternately, if you stepped into a signal handler, you should be
 * able to continue 'til the next breakpoint from within the signal
 * handler, even if the handler returns.
 */
#define MSR_USERCHANGE	(MSR_FE0 | MSR_FE1)

struct timespec32 {
	s32    tv_sec;
	s32    tv_nsec;
};

struct sigregs32 {
	/*
	 * the gp_regs array is 32 bit representation of the pt_regs
	 * structure that was stored on the kernle stack during the
	 * system call that was interrupted for the signal.
	 *
	 * Note that the entire pt_regs regs structure will fit in
	 * the gp_regs structure because the ELF_NREG value is 48 for
	 * PPC and the pt_regs structure contains 44 registers
	 */
	elf_gregset_t32	gp_regs;
	double		fp_regs[ELF_NFPREG];
	unsigned int	tramp[2];
	/*
	 * Programs using the rs6000/xcoff abi can save up to 19 gp
	 * regs and 18 fp regs below sp before decrementing it.
	 */
	int		abigap[56];
};


struct rt_sigframe_32 {
	/*
	 * Unused space at start of frame to allow for storing of
	 * stack pointers
	 */
	unsigned long _unused;
	/*
	 * This is a 32 bit pointer in user address space 
	 *     it is a pointer to the siginfo stucture in the rt stack frame 
	 */
	u32 pinfo;
	/*
	 * This is a 32 bit pointer in user address space
	 * it is a pointer to the user context in the rt stack frame
	 */
	u32 puc;
	struct siginfo32  info;
	struct ucontext32 uc;
};





extern asmlinkage long sys_wait4(pid_t pid,unsigned int * stat_addr, int options, struct rusage * ru);


/****************************************************************************/
/*  Start of nonRT signal support                                           */
/*                                                                          */
/*     sigset_t is 32 bits for non-rt signals                               */
/*                                                                          */
/*  System Calls                                                            */
/*       sigaction                sys32_sigaction                           */
/*       sigpending               sys32_sigpending                          */
/*       sigprocmask              sys32_sigprocmask                         */
/*       sigreturn                sys32_sigreturn                           */
/*                                                                          */
/*  Note sigsuspend has no special 32 bit routine - uses the 64 bit routine */ 
/*                                                                          */
/*  Other routines                                                          */
/*        setup_frame32                                                     */
/*                                                                          */
/****************************************************************************/


asmlinkage long sys32_sigaction(int sig, struct old_sigaction32 *act, struct old_sigaction32 *oact)
{
	struct k_sigaction new_ka, old_ka;
	int ret;
	
	PPCDBG(PPCDBG_SYS32, "sys32_sigaction - entered - pid=%ld current=%lx comm=%s\n", current->pid, current, current->comm);

	if (sig < 0)
		sig = -sig;

	if (act) {
		old_sigset_t32 mask;

		ret = get_user((long)new_ka.sa.sa_handler, &act->sa_handler);
		ret |= __get_user((long)new_ka.sa.sa_restorer, &act->sa_restorer);
		ret |= __get_user(new_ka.sa.sa_flags, &act->sa_flags);
		ret |= __get_user(mask, &act->sa_mask);
		if (ret)
			return ret;
		PPCDBG(PPCDBG_SIGNAL, "sys32_sigaction flags =%lx  \n", new_ka.sa.sa_flags);

		siginitset(&new_ka.sa.sa_mask, mask);
	}

	ret = do_sigaction(sig, act ? &new_ka : NULL, oact ? &old_ka : NULL);

	if (!ret && oact)
	{
		ret = put_user((long)old_ka.sa.sa_handler, &oact->sa_handler);
		ret |= __put_user((long)old_ka.sa.sa_restorer, &oact->sa_restorer);
		ret |= __put_user(old_ka.sa.sa_flags, &oact->sa_flags);
		ret |= __put_user(old_ka.sa.sa_mask.sig[0], &oact->sa_mask);
	}

	
	PPCDBG(PPCDBG_SYS32, "sys32_sigaction - exited - pid=%ld current=%lx comm=%s\n", current->pid, current, current->comm);

	return ret;
}




extern asmlinkage long sys_sigpending(old_sigset_t *set);

asmlinkage long sys32_sigpending(old_sigset_t32 *set)
{
	old_sigset_t s;
	int ret;
	mm_segment_t old_fs = get_fs();
	
	PPCDBG(PPCDBG_SYS32, "sys32_sigpending - entered - pid=%ld current=%lx comm=%s\n", current->pid, current, current->comm);
		
	set_fs (KERNEL_DS);
	ret = sys_sigpending(&s);
	set_fs (old_fs);
	if (put_user (s, set)) return -EFAULT;
	
	PPCDBG(PPCDBG_SYS32, "sys32_sigpending - exited - pid=%ld current=%lx comm=%s\n", current->pid, current, current->comm);

	return ret;
}




extern asmlinkage long sys_sigprocmask(int how, old_sigset_t *set, old_sigset_t *oset);

/* Note: it is necessary to treat how as an unsigned int, 
 * with the corresponding cast to a signed int to insure that the 
 * proper conversion (sign extension) between the register representation of a signed int (msr in 32-bit mode)
 * and the register representation of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_sigprocmask(u32 how, old_sigset_t32 *set, old_sigset_t32 *oset)
{
	old_sigset_t s;
	int ret;
	mm_segment_t old_fs = get_fs();
	
	PPCDBG(PPCDBG_SYS32, "sys32_sigprocmask - entered - pid=%ld current=%lx comm=%s\n", current->pid, current, current->comm);
	
	if (set && get_user(s, set))
		return -EFAULT;
	set_fs (KERNEL_DS);
	ret = sys_sigprocmask((int)how, set ? &s : NULL, oset ? &s : NULL);
	set_fs (old_fs);
	
	PPCDBG(PPCDBG_SYS32, "sys32_sigprocmask - exited - pid=%ld current=%lx comm=%s\n", current->pid, current, current->comm);

	if (ret)
		return ret;
	if (oset && put_user (s, oset))
		return -EFAULT;
	return 0;
}



/*
 * When we have signals to deliver, we set up on the
 * user stack, going down from the original stack pointer:
 *	a sigregs struct
 *	one or more sigcontext structs
 *	a gap of __SIGNAL_FRAMESIZE32 bytes
 *
 * Each of these things must be a multiple of 16 bytes in size.
 *
*/


/*
 * Do a signal return; undo the signal stack.
 */
long sys32_sigreturn(unsigned long r3, unsigned long r4, unsigned long r5,
		     unsigned long r6, unsigned long r7, unsigned long r8,
		     struct pt_regs *regs)
{
	struct sigcontext32 *sc, sigctx;
	struct sigregs32 *sr;
	int ret;
	elf_gregset_t32 saved_regs;  /* an array of ELF_NGREG unsigned ints (32 bits) */
	sigset_t set;
	unsigned int prevsp;
	int i;

	PPCDBG(PPCDBG_SIGNAL, "sys32_sigreturn - entered - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);

	sc = (struct sigcontext32 *)(regs->gpr[1] + __SIGNAL_FRAMESIZE32);
	if (copy_from_user(&sigctx, sc, sizeof(sigctx)))
		goto badframe;

	/*
	 * Note that PPC32 puts the upper 32 bits of the sigmask in the
	 * unused part of the signal stackframe
	 */
	set.sig[0] = sigctx.oldmask + ((long)(sigctx._unused[3])<< 32);
	sigdelsetmask(&set, ~_BLOCKABLE);
	spin_lock_irq(&current->sigmask_lock);
	current->blocked = set;
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);

	sc++;			/* Look at next sigcontext */
	/* If the next sigcontext is actually the sigregs (frame)  */
	/*   - then no more sigcontexts on the user stack          */  
	if (sc == (struct sigcontext32*)(u64)sigctx.regs)
	{
		/* Last stacked signal - restore registers */
		sr = (struct sigregs32*)(u64)sigctx.regs;
		if (regs->msr & MSR_FP )
			giveup_fpu(current);
		/* 
		 * Copy the 32 bit register values off the user stack
		 * into the 32 bit register area
		 */
		if (copy_from_user(saved_regs, &sr->gp_regs,sizeof(sr->gp_regs)))
			goto badframe;
		/*
		 * The saved reg structure in the frame is an elf_grepset_t32,
		 * it is a 32 bit register save of the registers in the
		 * pt_regs structure that was stored on the kernel stack
		 * during the system call when the system call was interrupted
		 * for the signal. Only 32 bits are saved because the
		 * sigcontext contains a pointer to the regs and the sig
		 * context address is passed as a pointer to the signal
		 * handler.  
		 *
		 * The entries in the elf_grepset have the same index as the
		 * elements in the pt_regs structure.
		 */
		saved_regs[PT_MSR] = (regs->msr & ~MSR_USERCHANGE)
			| (saved_regs[PT_MSR] & MSR_USERCHANGE);
		/*
		 * Register 2 is the kernel toc - should be reset on
		 * any calls into the kernel 
		 */
		for (i = 0; i < 32; i++)
			regs->gpr[i] = (u64)(saved_regs[i]) & 0xFFFFFFFF;

		/*
		 *  restore the non gpr registers 
		 */
		regs->msr = (u64)(saved_regs[PT_MSR]) & 0xFFFFFFFF;
		/*
		 * Insure that the interrupt mode is 64 bit, during 32 bit
		 * execution. (This is necessary because we only saved
		 * lower 32 bits of msr.)
		 */
		regs->msr = regs->msr | MSR_ISF;

		regs->nip = (u64)(saved_regs[PT_NIP]) & 0xFFFFFFFF;
		regs->orig_gpr3 = (u64)(saved_regs[PT_ORIG_R3]) & 0xFFFFFFFF; 
		regs->ctr = (u64)(saved_regs[PT_CTR]) & 0xFFFFFFFF; 
		regs->link = (u64)(saved_regs[PT_LNK]) & 0xFFFFFFFF; 
		regs->xer = (u64)(saved_regs[PT_XER]) & 0xFFFFFFFF; 
		regs->ccr = (u64)(saved_regs[PT_CCR]) & 0xFFFFFFFF;
		/* regs->softe is left unchanged (like the MSR.EE bit) */
		/******************************************************/
		/* the DAR and the DSISR are only relevant during a   */
		/*   data or instruction storage interrupt. The value */
		/*   will be set to zero.                             */
		/******************************************************/
		regs->dar = 0; 
		regs->dsisr = 0;
		regs->result = (u64)(saved_regs[PT_RESULT]) & 0xFFFFFFFF;

		if (copy_from_user(current->thread.fpr, &sr->fp_regs, sizeof(sr->fp_regs)))
			goto badframe;

		ret = regs->result;
	} else {
		/* More signals to go */
		regs->gpr[1] = (unsigned long)sc - __SIGNAL_FRAMESIZE32;
		if (copy_from_user(&sigctx, sc, sizeof(sigctx)))
			goto badframe;
		sr = (struct sigregs32*)(u64)sigctx.regs;
		regs->gpr[3] = ret = sigctx.signal;
		regs->gpr[4] = (unsigned long) sc;
		regs->link = (unsigned long) &sr->tramp;
		regs->nip = sigctx.handler;

		if (get_user(prevsp, &sr->gp_regs[PT_R1])
		    || put_user(prevsp, (unsigned int*) regs->gpr[1]))
			goto badframe;
		current->thread.fpscr = 0;
	}
  
	PPCDBG(PPCDBG_SIGNAL, "sys32_sigreturn - normal exit returning %ld - pid=%ld current=%lx comm=%s \n", ret, current->pid, current, current->comm);
	return ret;

badframe:
	PPCDBG(PPCDBG_SYS32NI, "sys32_sigreturn - badframe - pid=%ld current=%lx comm=%s \n", current->pid, current, current->comm);
	do_exit(SIGSEGV);
}	

/*
 * Set up a signal frame.
 */
static void
setup_frame32(struct pt_regs *regs, struct sigregs32 *frame,
            unsigned int newsp)
{
	struct sigcontext32 *sc = (struct sigcontext32 *)(u64)newsp;
	int i;

	if (verify_area(VERIFY_WRITE, frame, sizeof(*frame)))
		goto badframe;
	if (regs->msr & MSR_FP)
		giveup_fpu(current);

	/*
	 * Copy the register contents for the pt_regs structure on the
	 *   kernel stack to the elf_gregset_t32 structure on the user
	 *   stack. This is a copy of 64 bit register values to 32 bit
	 *   register values. The high order 32 bits of the 64 bit
	 *   registers are not needed since a 32 bit application is
	 *   running and the saved registers are the contents of the
	 *   user registers at the time of a system call.
	 * 
	 * The values saved on the user stack will be restored into
	 *  the registers during the signal return processing
	 *
	 * Note the +1 is needed in order to get the lower 32 bits
	 * of 64 bit register
	 */
	for (i = 0; i < sizeof(struct pt_regs32)/sizeof(u32); i++) {
		if (__copy_to_user(&frame->gp_regs[i], (u32*)(&regs->gpr[i])+1, sizeof(u32)))
			goto badframe;
	}

	/*
	 * Now copy the floating point registers onto the user stack 
	 *
	 * Also set up so on the completion of the signal handler, the
	 * sys_sigreturn will get control to reset the stack
	 */
	if (__copy_to_user(&frame->fp_regs, current->thread.fpr,
			   ELF_NFPREG * sizeof(double))
	    /* li r0, __NR_sigreturn */
	    || __put_user(0x38000000U + __NR_sigreturn, &frame->tramp[0])
	    /* sc */
	    || __put_user(0x44000002U, &frame->tramp[1]))
		goto badframe;

	flush_icache_range((unsigned long) &frame->tramp[0],
			   (unsigned long) &frame->tramp[2]);
	current->thread.fpscr = 0;      /* turn off all fp exceptions */

	newsp -= __SIGNAL_FRAMESIZE32;
	if (put_user(regs->gpr[1], (u32*)(u64)newsp)
	    || get_user(regs->nip, &sc->handler)
	    || get_user(regs->gpr[3], &sc->signal))
		goto badframe;

	regs->gpr[1] = newsp & 0xFFFFFFFF;
	/*
	 * first parameter to the signal handler is the signal number
	 *  - the value is in gpr3
	 * second parameter to the signal handler is the sigcontext
	 *   - set the value into gpr4
	 */
	regs->gpr[4] = (unsigned long) sc;
	regs->link = (unsigned long) frame->tramp;
	return;

 badframe:
	udbg_printf("setup_frame32 - badframe in setup_frame, regs=%p frame=%p newsp=%lx\n", regs, frame, newsp);  PPCDBG_ENTER_DEBUGGER();
#if DEBUG_SIG
	printk("badframe in setup_frame32, regs=%p frame=%p newsp=%lx\n",
	       regs, frame, newsp);
#endif
	do_exit(SIGSEGV);
}


/*
 *  Start of RT signal support
 *
 *     sigset_t is 64 bits for rt signals
 *
 *  System Calls
 *       sigaction                sys32_rt_sigaction
 *       sigpending               sys32_rt_sigpending
 *       sigprocmask              sys32_rt_sigprocmask
 *       sigreturn                sys32_rt_sigreturn
 *       sigtimedwait             sys32_rt_sigtimedwait
 *       sigqueueinfo             sys32_rt_sigqueueinfo
 *       sigsuspend               sys32_rt_sigsuspend
 *
 *  Other routines
 *        setup_rt_frame32
 *        copy_siginfo_to_user32
 *        siginfo32to64
 */


/*
 * This code executes after the rt signal handler in 32 bit mode has
 * completed and returned  
 */
long sys32_rt_sigreturn(unsigned long r3, unsigned long r4, unsigned long r5,
			unsigned long r6, unsigned long r7, unsigned long r8,
			struct pt_regs * regs)
{
	struct rt_sigframe_32 *rt_stack_frame;
	struct sigcontext32 sigctx;
	struct sigregs32 *signalregs;
 
	int i, ret;
	elf_gregset_t32 saved_regs;   /* an array of 32 bit register values */
	sigset_t signal_set; 
	stack_t stack;
	unsigned int previous_stack;

	ret = 0;
	/* Adjust the inputted reg1 to point to the first rt signal frame */
	rt_stack_frame = (struct rt_sigframe_32 *)(regs->gpr[1] + __SIGNAL_FRAMESIZE32);
	/* Copy the information from the user stack  */
	if (copy_from_user(&sigctx, &rt_stack_frame->uc.uc_mcontext,sizeof(sigctx))
	    || copy_from_user(&signal_set, &rt_stack_frame->uc.uc_sigmask,sizeof(signal_set))
	    || copy_from_user(&stack,&rt_stack_frame->uc.uc_stack,sizeof(stack)))
		/* unable to copy from user storage */
		goto badframe;

	/*
	 * Unblock the signal that was processed 
	 *   After a signal handler runs - 
	 *     if the signal is blockable - the signal will be unblocked  
	 *       ( sigkill and sigstop are not blockable)
	 */
	sigdelsetmask(&signal_set, ~_BLOCKABLE); 
	/* update the current based on the sigmask found in the rt_stackframe */
	spin_lock_irq(&current->sigmask_lock);
	current->blocked = signal_set;
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);

	/* Set to point to the next rt_sigframe - this is used to determine whether this 
	 *   is the last signal to process
	 */
	rt_stack_frame ++;

	if (rt_stack_frame == (struct rt_sigframe_32 *)(u64)(sigctx.regs)) {
		signalregs = (struct sigregs32 *) (u64)sigctx.regs;
		/* If currently owning the floating point - give them up */
		if (regs->msr & MSR_FP)
			giveup_fpu(current);

		if (copy_from_user(saved_regs,&signalregs->gp_regs,sizeof(signalregs->gp_regs))) 
			goto badframe;

		/*
		 * The saved reg structure in the frame is an elf_grepset_t32,
		 * it is a 32 bit register save of the registers in the
		 * pt_regs structure that was stored on the kernel stack
		 * during the system call when the system call was interrupted
		 * for the signal. Only 32 bits are saved because the
		 * sigcontext contains a pointer to the regs and the sig
		 * context address is passed as a pointer to the signal handler
		 *
		 * The entries in the elf_grepset have the same index as
		 * the elements in the pt_regs structure.
		 */
		saved_regs[PT_MSR] = (regs->msr & ~MSR_USERCHANGE)
			| (saved_regs[PT_MSR] & MSR_USERCHANGE);
		/*
		 * Register 2 is the kernel toc - should be reset on any
		 * calls into the kernel
		 */
		for (i = 0; i < 32; i++)
			regs->gpr[i] = (u64)(saved_regs[i]) & 0xFFFFFFFF;
		/*
		 * restore the non gpr registers
		 */
		regs->msr = (u64)(saved_regs[PT_MSR]) & 0xFFFFFFFF;
		regs->nip = (u64)(saved_regs[PT_NIP]) & 0xFFFFFFFF;
		regs->orig_gpr3 = (u64)(saved_regs[PT_ORIG_R3]) & 0xFFFFFFFF; 
		regs->ctr = (u64)(saved_regs[PT_CTR]) & 0xFFFFFFFF; 
		regs->link = (u64)(saved_regs[PT_LNK]) & 0xFFFFFFFF; 
		regs->xer = (u64)(saved_regs[PT_XER]) & 0xFFFFFFFF; 
		regs->ccr = (u64)(saved_regs[PT_CCR]) & 0xFFFFFFFF;
		/* regs->softe is left unchanged (like MSR.EE) */
		/*
		 * the DAR and the DSISR are only relevant during a
		 *   data or instruction storage interrupt. The value
		 *   will be set to zero.
		 */
		regs->dar = 0; 
		regs->dsisr = 0;
		regs->result = (u64)(saved_regs[PT_RESULT]) & 0xFFFFFFFF;

		if (copy_from_user(current->thread.fpr, &signalregs->fp_regs, sizeof(signalregs->fp_regs))) 
			goto badframe;

		ret = regs->result;
	}
	else  /* more signals to go  */
	{
		regs->gpr[1] = (u64)rt_stack_frame - __SIGNAL_FRAMESIZE32;
		if (copy_from_user(&sigctx, &rt_stack_frame->uc.uc_mcontext,sizeof(sigctx)))
		{
			goto badframe;
		}
		signalregs = (struct sigregs32 *) (u64)sigctx.regs;
		/* first parm to signal handler is the signal number */
		regs->gpr[3] = ret = sigctx.signal;
		/* second parm is a pointer to sig info */
		get_user(regs->gpr[4], &rt_stack_frame->pinfo);
		/* third parm is a pointer to the ucontext */
		get_user(regs->gpr[5], &rt_stack_frame->puc);
		/* fourth parm is the stack frame */
		regs->gpr[6] = (u64)rt_stack_frame;
		/* Set up link register to return to sigreturn when the */
		/*  signal handler completes */
		regs->link = (u64)&signalregs->tramp;
		/* Set next instruction to the start fo the signal handler */
		regs->nip = sigctx.handler;
		/* Set the reg1 to look like a call to the signal handler */
		if (get_user(previous_stack,&signalregs->gp_regs[PT_R1])
		    || put_user(previous_stack, (unsigned long *)regs->gpr[1]))
		{
			goto badframe;
		}
		current->thread.fpscr = 0;

	}

	return ret;

 badframe:
	do_exit(SIGSEGV);     
}



asmlinkage long sys32_rt_sigaction(int sig, const struct sigaction32 *act, struct sigaction32 *oact, size_t sigsetsize)
{
	struct k_sigaction new_ka, old_ka;
	int ret;
	sigset32_t set32;

	PPCDBG(PPCDBG_SIGNAL, "sys32_rt_sigaction - entered - sig=%x \n", sig);

	/* XXX: Don't preclude handling different sized sigset_t's.  */
	if (sigsetsize != sizeof(sigset32_t))
		return -EINVAL;

	if (act) {
		ret = get_user((long)new_ka.sa.sa_handler, &act->sa_handler);
		ret |= __copy_from_user(&set32, &act->sa_mask,
					sizeof(sigset32_t));
		switch (_NSIG_WORDS) {
		case 4: new_ka.sa.sa_mask.sig[3] = set32.sig[6]
				| (((long)set32.sig[7]) << 32);
		case 3: new_ka.sa.sa_mask.sig[2] = set32.sig[4]
				| (((long)set32.sig[5]) << 32);
		case 2: new_ka.sa.sa_mask.sig[1] = set32.sig[2]
				| (((long)set32.sig[3]) << 32);
		case 1: new_ka.sa.sa_mask.sig[0] = set32.sig[0]
				| (((long)set32.sig[1]) << 32);
		}

		ret |= __get_user(new_ka.sa.sa_flags, &act->sa_flags);
		
		if (ret)
			return -EFAULT;
	}

	ret = do_sigaction(sig, act ? &new_ka : NULL, oact ? &old_ka : NULL);

	if (!ret && oact) {
		switch (_NSIG_WORDS) {
		case 4:
			set32.sig[7] = (old_ka.sa.sa_mask.sig[3] >> 32);
			set32.sig[6] = old_ka.sa.sa_mask.sig[3];
		case 3:
			set32.sig[5] = (old_ka.sa.sa_mask.sig[2] >> 32);
			set32.sig[4] = old_ka.sa.sa_mask.sig[2];
		case 2:
			set32.sig[3] = (old_ka.sa.sa_mask.sig[1] >> 32);
			set32.sig[2] = old_ka.sa.sa_mask.sig[1];
		case 1:
			set32.sig[1] = (old_ka.sa.sa_mask.sig[0] >> 32);
			set32.sig[0] = old_ka.sa.sa_mask.sig[0];
		}
		ret = put_user((long)old_ka.sa.sa_handler, &oact->sa_handler);
		ret |= __copy_to_user(&oact->sa_mask, &set32,
				      sizeof(sigset32_t));
		ret |= __put_user(old_ka.sa.sa_flags, &oact->sa_flags);
	}

  
	PPCDBG(PPCDBG_SIGNAL, "sys32_rt_sigaction - exiting - sig=%x \n", sig);
	return ret;
}


extern asmlinkage long sys_rt_sigprocmask(int how, sigset_t *set, sigset_t *oset,
					  size_t sigsetsize);

/*
 * Note: it is necessary to treat how as an unsigned int, with the
 * corresponding cast to a signed int to insure that the proper
 * conversion (sign extension) between the register representation
 * of a signed int (msr in 32-bit mode) and the register representation
 * of a signed int (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_rt_sigprocmask(u32 how, sigset32_t *set, sigset32_t *oset, size_t sigsetsize)
{
	sigset_t s;
	sigset32_t s32;
	int ret;
	mm_segment_t old_fs = get_fs();

	PPCDBG(PPCDBG_SIGNAL, "sys32_rt_sigprocmask - entered how=%x \n", (int)how);
	
	if (set) {
		if (copy_from_user (&s32, set, sizeof(sigset32_t)))
			return -EFAULT;
    
		switch (_NSIG_WORDS) {
		case 4: s.sig[3] = s32.sig[6] | (((long)s32.sig[7]) << 32);
		case 3: s.sig[2] = s32.sig[4] | (((long)s32.sig[5]) << 32);
		case 2: s.sig[1] = s32.sig[2] | (((long)s32.sig[3]) << 32);
		case 1: s.sig[0] = s32.sig[0] | (((long)s32.sig[1]) << 32);
		}
	}
	
	set_fs (KERNEL_DS);
	ret = sys_rt_sigprocmask((int)how, set ? &s : NULL, oset ? &s : NULL,
				 sigsetsize); 
	set_fs (old_fs);
	if (ret)
		return ret;
	if (oset) {
		switch (_NSIG_WORDS) {
		case 4: s32.sig[7] = (s.sig[3] >> 32); s32.sig[6] = s.sig[3];
		case 3: s32.sig[5] = (s.sig[2] >> 32); s32.sig[4] = s.sig[2];
		case 2: s32.sig[3] = (s.sig[1] >> 32); s32.sig[2] = s.sig[1];
		case 1: s32.sig[1] = (s.sig[0] >> 32); s32.sig[0] = s.sig[0];
		}
		if (copy_to_user (oset, &s32, sizeof(sigset32_t)))
			return -EFAULT;
	}
	return 0;
}


extern asmlinkage long sys_rt_sigpending(sigset_t *set, size_t sigsetsize);



asmlinkage long sys32_rt_sigpending(sigset32_t *set,   __kernel_size_t32 sigsetsize)
{

	sigset_t s;
	sigset32_t s32;
	int ret;
	mm_segment_t old_fs = get_fs();
		
	set_fs (KERNEL_DS);
	ret = sys_rt_sigpending(&s, sigsetsize);
	set_fs (old_fs);
	if (!ret) {
		switch (_NSIG_WORDS) {
		case 4: s32.sig[7] = (s.sig[3] >> 32); s32.sig[6] = s.sig[3];
		case 3: s32.sig[5] = (s.sig[2] >> 32); s32.sig[4] = s.sig[2];
		case 2: s32.sig[3] = (s.sig[1] >> 32); s32.sig[2] = s.sig[1];
		case 1: s32.sig[1] = (s.sig[0] >> 32); s32.sig[0] = s.sig[0];
		}
		if (copy_to_user (set, &s32, sizeof(sigset32_t)))
			return -EFAULT;
	}
	return ret;
}



siginfo_t32 *
siginfo64to32(siginfo_t32 *d, siginfo_t *s)
{
	memset (d, 0, sizeof(siginfo_t32));
	d->si_signo = s->si_signo;
	d->si_errno = s->si_errno;
	d->si_code = (short)s->si_code;
	if (s->si_signo >= SIGRTMIN) {
		d->si_pid = s->si_pid;
		d->si_uid = s->si_uid;
		
		d->si_int = s->si_int;
	} else switch (s->si_signo) {
	/* XXX: What about POSIX1.b timers */
	case SIGCHLD:
		d->si_pid = s->si_pid;
		d->si_status = s->si_status;
		d->si_utime = s->si_utime;
		d->si_stime = s->si_stime;
		break;
	case SIGSEGV:
	case SIGBUS:
	case SIGFPE:
	case SIGILL:
		d->si_addr = (unsigned int)(s->si_addr);
        break;
	case SIGPOLL:
		d->si_band = s->si_band;
		d->si_fd = s->si_fd;
		break;
	default:
		d->si_pid = s->si_pid;
		d->si_uid = s->si_uid;
		break;
	}
	return d;
}

extern asmlinkage long
sys_rt_sigtimedwait(const sigset_t *uthese, siginfo_t *uinfo,
		    const struct timespec *uts, size_t sigsetsize);

asmlinkage long
sys32_rt_sigtimedwait(sigset32_t *uthese, siginfo_t32 *uinfo,
		      struct timespec32 *uts, __kernel_size_t32 sigsetsize)
{
	sigset_t s;
	sigset32_t s32;
	struct timespec t;
	int ret;
	mm_segment_t old_fs = get_fs();
	siginfo_t info;
	siginfo_t32 info32;
		
	if (copy_from_user (&s32, uthese, sizeof(sigset32_t)))
		return -EFAULT;
	switch (_NSIG_WORDS) {
	case 4: s.sig[3] = s32.sig[6] | (((long)s32.sig[7]) << 32);
	case 3: s.sig[2] = s32.sig[4] | (((long)s32.sig[5]) << 32);
	case 2: s.sig[1] = s32.sig[2] | (((long)s32.sig[3]) << 32);
	case 1: s.sig[0] = s32.sig[0] | (((long)s32.sig[1]) << 32);
	}
	if (uts) {
		ret = get_user (t.tv_sec, &uts->tv_sec);
		ret |= __get_user (t.tv_nsec, &uts->tv_nsec);
		if (ret)
			return -EFAULT;
	}
	set_fs (KERNEL_DS);
	if (uts) 
		ret = sys_rt_sigtimedwait(&s, &info, &t, sigsetsize);
	else
		ret = sys_rt_sigtimedwait(&s, &info, (struct timespec *)uts,
				sigsetsize);
	set_fs (old_fs);
	if (ret >= 0 && uinfo) {
		if (copy_to_user (uinfo, siginfo64to32(&info32, &info),
				  sizeof(siginfo_t32)))
			return -EFAULT;
	}
	return ret;
}



siginfo_t *
siginfo32to64(siginfo_t *d, siginfo_t32 *s)
{
	d->si_signo = s->si_signo;
	d->si_errno = s->si_errno;
	d->si_code = s->si_code;
	if (s->si_signo >= SIGRTMIN) {
		d->si_pid = s->si_pid;
		d->si_uid = s->si_uid;
		d->si_int = s->si_int;
        
	} else switch (s->si_signo) {
	/* XXX: What about POSIX1.b timers */
	case SIGCHLD:
		d->si_pid = s->si_pid;
		d->si_status = s->si_status;
		d->si_utime = s->si_utime;
		d->si_stime = s->si_stime;
		break;
	case SIGSEGV:
	case SIGBUS:
	case SIGFPE:
	case SIGILL:
		d->si_addr = (void *)A(s->si_addr);
  		break;
	case SIGPOLL:
		d->si_band = s->si_band;
		d->si_fd = s->si_fd;
		break;
	default:
		d->si_pid = s->si_pid;
		d->si_uid = s->si_uid;
		break;
	}
	return d;
}


extern asmlinkage long sys_rt_sigqueueinfo(int pid, int sig, siginfo_t *uinfo);

/*
 * Note: it is necessary to treat pid and sig as unsigned ints, with the
 * corresponding cast to a signed int to insure that the proper conversion
 * (sign extension) between the register representation of a signed int
 * (msr in 32-bit mode) and the register representation of a signed int
 * (msr in 64-bit mode) is performed.
 */
asmlinkage long sys32_rt_sigqueueinfo(u32 pid, u32 sig, siginfo_t32 *uinfo)
{
	siginfo_t info;
	siginfo_t32 info32;
	int ret;
	mm_segment_t old_fs = get_fs();
	
	if (copy_from_user (&info32, uinfo, sizeof(siginfo_t32)))
		return -EFAULT;
    	/* XXX: Is this correct? */
	siginfo32to64(&info, &info32);

	set_fs (KERNEL_DS);
	ret = sys_rt_sigqueueinfo((int)pid, (int)sig, &info);
	set_fs (old_fs);
	return ret;
}


int do_signal(sigset_t *oldset, struct pt_regs *regs);
int sys32_rt_sigsuspend(sigset32_t* unewset, size_t sigsetsize, int p3, int p4, int p6, int p7, struct pt_regs *regs)
{
	sigset_t saveset, newset;
	
	sigset32_t s32;

	/* XXX: Don't preclude handling different sized sigset_t's.  */
	if (sigsetsize != sizeof(sigset_t))
		return -EINVAL;

	if (copy_from_user(&s32, unewset, sizeof(s32)))
		return -EFAULT;

	/*
	 * Swap the 2 words of the 64-bit sigset_t (they are stored
	 * in the "wrong" endian in 32-bit user storage).
	 */
	switch (_NSIG_WORDS) {
		case 4: newset.sig[3] = s32.sig[6] | (((long)s32.sig[7]) << 32);
		case 3: newset.sig[2] = s32.sig[4] | (((long)s32.sig[5]) << 32);
		case 2: newset.sig[1] = s32.sig[2] | (((long)s32.sig[3]) << 32);
		case 1: newset.sig[0] = s32.sig[0] | (((long)s32.sig[1]) << 32);
	}

	sigdelsetmask(&newset, ~_BLOCKABLE);

	spin_lock_irq(&current->sigmask_lock);
	saveset = current->blocked;
	current->blocked = newset;
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);

	regs->result = -EINTR;
	regs->gpr[3] = EINTR;
	regs->ccr |= 0x10000000;
	while (1) {
		current->state = TASK_INTERRUPTIBLE;
		schedule();
		if (do_signal(&saveset, regs))
			/*
			 * If a signal handler needs to be called,
			 * do_signal() has set R3 to the signal number (the
			 * first argument of the signal handler), so don't
			 * overwrite that with EINTR !
			 * In the other cases, do_signal() doesn't touch 
			 * R3, so it's still set to -EINTR (see above).
			 */
			return regs->gpr[3];
	}
}


/*
 * Set up a rt signal frame.
 */
static void
setup_rt_frame32(struct pt_regs *regs, struct sigregs32 *frame,
            unsigned int newsp)
{
	unsigned int copyreg4,copyreg5;
	struct rt_sigframe_32 * rt_sf = (struct rt_sigframe_32 *) (u64)newsp;
	int i;
  
	if (verify_area(VERIFY_WRITE, frame, sizeof(*frame)))
		goto badframe;
	if (regs->msr & MSR_FP)
		giveup_fpu(current);

	/*
	 * Copy the register contents for the pt_regs structure on the
	 *   kernel stack to the elf_gregset_t32 structure on the user
	 *   stack. This is a copy of 64 bit register values to 32 bit
	 *   register values. The high order 32 bits of the 64 bit
	 *   registers are not needed since a 32 bit application is
	 *   running and the saved registers are the contents of the
	 *   user registers at the time of a system call.
	 *
	 * The values saved on the user stack will be restored into
	 *  the registers during the signal return processing.
	 *
	 * Note the +1 is needed in order to get the lower 32 bits
	 * of 64 bit register
	 */
	for (i = 0; i < sizeof(struct pt_regs32)/sizeof(u32); i++) {
		if (__copy_to_user(&frame->gp_regs[i], (u32*)(&regs->gpr[i])+1, sizeof(u32)))
			goto badframe;
	}


	/*
	 * Now copy the floating point registers onto the user stack
	 *
	 * Also set up so on the completion of the signal handler, the
	 * sys_sigreturn will get control to reset the stack
	 */
	if (__copy_to_user(&frame->fp_regs, current->thread.fpr,
			   ELF_NFPREG * sizeof(double))
	    || __put_user(0x38000000U + __NR_rt_sigreturn, &frame->tramp[0])    /* li r0, __NR_rt_sigreturn */
	    || __put_user(0x44000002U, &frame->tramp[1]))   /* sc */
		goto badframe;

	flush_icache_range((unsigned long) &frame->tramp[0],
			   (unsigned long) &frame->tramp[2]);
	current->thread.fpscr = 0;	/* turn off all fp exceptions */
  
	/*
	 * Retrieve rt_sigframe from stack and
	 * set up registers for signal handler
	*/
	newsp -= __SIGNAL_FRAMESIZE32;
      

	if (put_user((u32)(regs->gpr[1]), (unsigned int *)(u64)newsp)
	    || get_user(regs->nip, &rt_sf->uc.uc_mcontext.handler)
	    || get_user(regs->gpr[3], &rt_sf->uc.uc_mcontext.signal)
	    || get_user(copyreg4, &rt_sf->pinfo)
	    || get_user(copyreg5, &rt_sf->puc))
		goto badframe;

	regs->gpr[4] = copyreg4;
	regs->gpr[5] = copyreg5;
	regs->gpr[1] = newsp;
	regs->gpr[6] = (unsigned long) rt_sf;
	regs->link = (unsigned long) frame->tramp;

	return;

 badframe:
	udbg_printf("setup_frame32 - badframe in setup_frame, regs=%p frame=%p newsp=%lx\n", regs, frame, newsp);  PPCDBG_ENTER_DEBUGGER();
#if DEBUG_SIG
	printk("badframe in setup_frame32, regs=%p frame=%p newsp=%lx\n",
	       regs, frame, newsp);
#endif
	do_exit(SIGSEGV);
}


/*
 * OK, we're invoking a handler
 */
static void
handle_signal32(unsigned long sig, struct k_sigaction *ka,
	      siginfo_t *info, sigset_t *oldset, struct pt_regs * regs,
	      unsigned int *newspp, unsigned int frame)
{
	struct sigcontext32 *sc;
	struct rt_sigframe_32 *rt_stack_frame;
	siginfo_t32 siginfo32bit;

	if (regs->trap == 0x0C00 /* System Call! */
	    && ((int)regs->result == -ERESTARTNOHAND ||
		((int)regs->result == -ERESTARTSYS &&
		 !(ka->sa.sa_flags & SA_RESTART))))
		regs->result = -EINTR;

	/* Set up the signal frame             */
	/*   Determine if an real time frame - siginfo required   */
	if (ka->sa.sa_flags & SA_SIGINFO)
	{
		siginfo64to32(&siginfo32bit,info);
		/* The ABI requires quadword alignment for the stack. */
		*newspp = (*newspp - sizeof(*rt_stack_frame)) & -16ul;
		rt_stack_frame = (struct rt_sigframe_32 *) (u64)(*newspp) ;
    
		if (verify_area(VERIFY_WRITE, rt_stack_frame, sizeof(*rt_stack_frame)))
		{
			goto badframe;
		}
		if (__put_user((u32)(u64)ka->sa.sa_handler, &rt_stack_frame->uc.uc_mcontext.handler)
		    || __put_user((u32)(u64)&rt_stack_frame->info, &rt_stack_frame->pinfo)
		    || __put_user((u32)(u64)&rt_stack_frame->uc, &rt_stack_frame->puc)
		    /*  put the siginfo on the user stack                    */
		    || __copy_to_user(&rt_stack_frame->info,&siginfo32bit,sizeof(siginfo32bit))
		    /*  set the ucontext on the user stack                   */ 
		    || __put_user(0,&rt_stack_frame->uc.uc_flags)
		    || __put_user(0,&rt_stack_frame->uc.uc_link)
		    || __put_user(current->sas_ss_sp, &rt_stack_frame->uc.uc_stack.ss_sp)
		    || __put_user(sas_ss_flags(regs->gpr[1]),
				  &rt_stack_frame->uc.uc_stack.ss_flags)
		    || __put_user(current->sas_ss_size, &rt_stack_frame->uc.uc_stack.ss_size)
		    || __copy_to_user(&rt_stack_frame->uc.uc_sigmask, oldset,sizeof(*oldset))
		    /* point the mcontext.regs to the pramble register frame  */
		    || __put_user(frame, &rt_stack_frame->uc.uc_mcontext.regs)
		    || __put_user(sig,&rt_stack_frame->uc.uc_mcontext.signal))
		{
			goto badframe; 
		}
	} else {
		/* Put a sigcontext on the stack */
		*newspp = (*newspp - sizeof(*sc)) & -16ul;
		sc = (struct sigcontext32 *)(u64)*newspp;
		if (verify_area(VERIFY_WRITE, sc, sizeof(*sc)))
			goto badframe;
		/*
		 * Note the upper 32 bits of the signal mask are stored
		 * in the unused part of the signal stack frame
		 */
		if (__put_user((u32)(u64)ka->sa.sa_handler, &sc->handler)
		    || __put_user(oldset->sig[0], &sc->oldmask)
		    || __put_user((oldset->sig[0] >> 32), &sc->_unused[3])
		    || __put_user((unsigned int)frame, &sc->regs)
		    || __put_user(sig, &sc->signal))
			goto badframe;
	}

	if (ka->sa.sa_flags & SA_ONESHOT)
		ka->sa.sa_handler = SIG_DFL;

	if (!(ka->sa.sa_flags & SA_NODEFER)) {
		spin_lock_irq(&current->sigmask_lock);
		sigorsets(&current->blocked,&current->blocked,&ka->sa.sa_mask);
		sigaddset(&current->blocked,sig);
		recalc_sigpending(current);
		spin_unlock_irq(&current->sigmask_lock);
	}
	
	return;

badframe:
#if DEBUG_SIG
	printk("badframe in handle_signal32, regs=%p frame=%lx newsp=%lx\n",
	       regs, frame, *newspp);
	printk("sc=%p sig=%d ka=%p info=%p oldset=%p\n", sc, sig, ka, info, oldset);
#endif
	do_exit(SIGSEGV);
}


/*
 *  Start Alternate signal stack support
 *
 *  System Calls
 *       sigaltatck               sys32_sigaltstack
 */


asmlinkage int sys32_sigaltstack(u32 newstack, u32 oldstack, int p3, int p4, int p6,
				 int p7, struct pt_regs *regs)
{
	stack_t uss, uoss;
	int ret;
	mm_segment_t old_fs;
	unsigned long sp;

	/*
	 * set sp to the user stack on entry to the system call
	 * the system call router sets R9 to the saved registers
	 */
	sp = regs->gpr[1];

	/*  Put new stack info in local 64 bit stack struct                      */ 
	if (newstack && (get_user((long)uss.ss_sp, &((stack_32_t *)(long)newstack)->ss_sp) ||
			 __get_user(uss.ss_flags, &((stack_32_t *)(long)newstack)->ss_flags) ||
			 __get_user(uss.ss_size, &((stack_32_t *)(long)newstack)->ss_size)))
		return -EFAULT; 

   
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	ret = do_sigaltstack(newstack ? &uss : NULL, oldstack ? &uoss : NULL, sp);
	set_fs(old_fs);
	/* Copy the stack information to the user output buffer                  */
	if (!ret && oldstack  && (put_user((long)uoss.ss_sp, &((stack_32_t *)(long)oldstack)->ss_sp) ||
				  __put_user(uoss.ss_flags, &((stack_32_t *)(long)oldstack)->ss_flags) ||
				  __put_user(uoss.ss_size, &((stack_32_t *)(long)oldstack)->ss_size)))
		return -EFAULT;
	return ret;
}



/*
 *  Start of do_signal32 routine
 *
 *   This routine gets control when a pending signal needs to be processed
 *     in the 32 bit target thread -
 *
 *   It handles both rt and non-rt signals
 */

/*
 * Note that 'init' is a special process: it doesn't get signals it doesn't
 * want to handle. Thus you cannot kill init even with a SIGKILL even by
 * mistake.
 */

int do_signal32(sigset_t *oldset, struct pt_regs *regs)
{
	siginfo_t info;
	struct k_sigaction *ka;
	unsigned int frame, newsp;

	if (!oldset)
		oldset = &current->blocked;

	newsp = frame = 0;

	for (;;) {
		unsigned long signr;
		
		spin_lock_irq(&current->sigmask_lock);
		signr = dequeue_signal(&current->blocked, &info);
		spin_unlock_irq(&current->sigmask_lock);
		ifppcdebug(PPCDBG_SYS32) {
			if (signr)
				udbg_printf("do_signal32 - processing signal=%2lx - pid=%ld, comm=%s \n", signr, current->pid, current->comm);
		}

		if (!signr)
			break;

		if ((current->ptrace & PT_PTRACED) && signr != SIGKILL) {
			/* Let the debugger run.  */
			current->exit_code = signr;
			current->state = TASK_STOPPED;
			notify_parent(current, SIGCHLD);
			schedule();

			/* We're back.  Did the debugger cancel the sig?  */
			if (!(signr = current->exit_code))
				continue;
			current->exit_code = 0;

			/* The debugger continued.  Ignore SIGSTOP.  */
			if (signr == SIGSTOP)
				continue;

			/* Update the siginfo structure.  Is this good?  */
			if (signr != info.si_signo) {
				info.si_signo = signr;
				info.si_errno = 0;
				info.si_code = SI_USER;
				info.si_pid = current->p_pptr->pid;
				info.si_uid = current->p_pptr->uid;
			}

			/* If the (new) signal is now blocked, requeue it.  */
			if (sigismember(&current->blocked, signr)) {
				send_sig_info(signr, &info, current);
				continue;
			}
		}

		ka = &current->sig->action[signr-1];

		if (ka->sa.sa_handler == SIG_IGN) {
			if (signr != SIGCHLD)
				continue;
			/* Check for SIGCHLD: it's special.  */
			while (sys_wait4(-1, NULL, WNOHANG, NULL) > 0)
				/* nothing */;
			continue;
		}

		if (ka->sa.sa_handler == SIG_DFL) {
			int exit_code = signr;

			/* Init gets no signals it doesn't want.  */
			if (current->pid == 1)
				continue;

			switch (signr) {
			case SIGCONT: case SIGCHLD: case SIGWINCH: case SIGURG:
				continue;

			case SIGTSTP: case SIGTTIN: case SIGTTOU:
				if (is_orphaned_pgrp(current->pgrp))
					continue;
				/* FALLTHRU */

			case SIGSTOP:
				current->state = TASK_STOPPED;
				current->exit_code = signr;
				if (!(current->p_pptr->sig->action[SIGCHLD-1].sa.sa_flags & SA_NOCLDSTOP))
					notify_parent(current, SIGCHLD);
				schedule();
				continue;

			case SIGQUIT: case SIGILL: case SIGTRAP:
			case SIGABRT: case SIGFPE: case SIGSEGV:
			case SIGBUS: case SIGSYS: case SIGXCPU: case SIGXFSZ:
				if (do_coredump(signr, regs))
					exit_code |= 0x80;
				/* FALLTHRU */

			default:
				sig_exit(signr, exit_code, &info);
				/* NOTREACHED */
			}
		}

		PPCDBG(PPCDBG_SIGNAL, " do signal :sigaction flags = %lx \n" ,ka->sa.sa_flags);
		PPCDBG(PPCDBG_SIGNAL, " do signal :on sig stack  = %lx \n" ,on_sig_stack(regs->gpr[1]));
		PPCDBG(PPCDBG_SIGNAL, " do signal :reg1  = %lx \n" ,regs->gpr[1]);
		PPCDBG(PPCDBG_SIGNAL, " do signal :alt stack  = %lx \n" ,current->sas_ss_sp);
		PPCDBG(PPCDBG_SIGNAL, " do signal :alt stack size  = %lx \n" ,current->sas_ss_size);



		if ( (ka->sa.sa_flags & SA_ONSTACK)
		     && (! on_sig_stack(regs->gpr[1])))
			newsp = (current->sas_ss_sp + current->sas_ss_size);
		else
			newsp = regs->gpr[1];
		/* The ABI requires quadword alignment for the stack. */
		newsp = frame = (newsp - sizeof(struct sigregs32)) & -16ul;

		/* Whee!  Actually deliver the signal.  */
		handle_signal32(signr, ka, &info, oldset, regs, &newsp, frame);
		break;
	}

	if (regs->trap == 0x0C00 /* System Call! */ &&
	    ((int)regs->result == -ERESTARTNOHAND ||
	     (int)regs->result == -ERESTARTSYS ||
	     (int)regs->result == -ERESTARTNOINTR)) {
		regs->gpr[3] = regs->orig_gpr3;
		regs->nip -= 4;		/* Back up & retry system call */
		regs->result = 0;
	}

	if (newsp == frame)
		return 0;		/* no signals delivered */

	/* Invoke correct stack setup routine */
	if (ka->sa.sa_flags & SA_SIGINFO) 
		setup_rt_frame32(regs, (struct sigregs32*)(u64)frame, newsp);
	else
		setup_frame32(regs, (struct sigregs32*)(u64)frame, newsp);
	return 1;
}
