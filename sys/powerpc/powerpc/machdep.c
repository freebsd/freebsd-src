/*
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
 *      This product includes software developed by TooLs GmbH.
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
 */
/*
 * Copyright (C) 2001 Benno Rice
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
 *
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *	$NetBSD: machdep.c,v 1.74.2.1 2000/11/01 16:13:48 tv Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"
#include "opt_compat.h"
#include "opt_msgbuf.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>
#include <sys/imgact.h>
#include <sys/sysproto.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/ktr.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/reboot.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/mbuf.h>
#include <sys/vmmeter.h>
#include <sys/msgbuf.h>
#include <sys/exec.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/linker.h>
#include <sys/cons.h>
#include <sys/ucontext.h>
#include <sys/sysent.h>
#include <net/netisr.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <sys/user.h>
#include <sys/ptrace.h>
#include <machine/bat.h>
#include <machine/clock.h>
#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/reg.h>
#include <machine/fpu.h>
#include <machine/vmparam.h>
#include <machine/elf.h>
#include <machine/trap.h>
#include <machine/powerpc.h>
#include <dev/ofw/openfirm.h>
#include <ddb/ddb.h>
#include <sys/vnode.h>
#include <machine/sigframe.h>

int cold = 1;

char		pcpu0[PAGE_SIZE];
char		uarea0[UAREA_PAGES * PAGE_SIZE];
struct		trapframe frame0;

vm_offset_t	kstack0;
vm_offset_t	kstack0_phys;

char		machine[] = "powerpc";
SYSCTL_STRING(_hw, HW_MACHINE, machine, CTLFLAG_RD, machine, 0, "");

static char	model[128];
SYSCTL_STRING(_hw, HW_MODEL, model, CTLFLAG_RD, model, 0, "");

static int cacheline_size = CACHELINESIZE;
SYSCTL_INT(_machdep, CPU_CACHELINE, cacheline_size,
	   CTLFLAG_RD, &cacheline_size, 0, "");

#ifdef DDB
/* start and end of kernel symbol table */
void		*ksym_start, *ksym_end;
#endif /* DDB */

static void	cpu_startup(void *);
SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL)

void		powerpc_init(u_int, u_int, u_int, void *);

int		save_ofw_mapping(void);
int		restore_ofw_mapping(void);

void		install_extint(void (*)(void));

int             setfault(faultbuf);             /* defined in locore.S */

long		Maxmem = 0;

struct pmap	ofw_pmap;
extern int	ofmsr;

struct bat	battable[16];

struct kva_md_info kmi;

static void
powerpc_ofw_shutdown(void *junk, int howto)
{
	if (howto & RB_HALT) {
		OF_exit();
	}
}

static void
cpu_startup(void *dummy)
{

	/*
	 * Initialise the decrementer-based clock.
	 */
	decr_init();

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	cpu_setup(PCPU_GET(cpuid));

	/* startrtclock(); */
#ifdef PERFMON
	perfmon_init();
#endif
	printf("real memory  = %ld (%ld MB)\n", ptoa(Maxmem),
	    ptoa(Maxmem) / 1048576);

	/*
	 * Display any holes after the first chunk of extended memory.
	 */
	if (bootverbose) {
		int indx;

		printf("Physical memory chunk(s):\n");
		for (indx = 0; phys_avail[indx + 1] != 0; indx += 2) {
			int size1 = phys_avail[indx + 1] - phys_avail[indx];

			printf("0x%08x - 0x%08x, %d bytes (%d pages)\n",
			    phys_avail[indx], phys_avail[indx + 1] - 1, size1,
			    size1 / PAGE_SIZE);
		}
	}

	vm_ksubmap_init(&kmi);

	printf("avail memory = %ld (%ld MB)\n", ptoa(cnt.v_free_count),
	    ptoa(cnt.v_free_count) / 1048576);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();
	vm_pager_bufferinit();

	EVENTHANDLER_REGISTER(shutdown_final, powerpc_ofw_shutdown, 0,
	    SHUTDOWN_PRI_LAST);

#ifdef SMP
	/*
	 * OK, enough kmem_alloc/malloc state should be up, lets get on with it!
	 */
	mp_start();			/* fire up the secondaries */
	mp_announce();
#endif  /* SMP */
}

extern char	kernel_text[], _end[];

extern void	*trapcode, *trapsize;
extern void	*alitrap, *alisize;
extern void	*dsitrap, *dsisize;
extern void	*isitrap, *isisize;
extern void	*decrint, *decrsize;
extern void	*tlbimiss, *tlbimsize;
extern void	*tlbdlmiss, *tlbdlmsize;
extern void	*tlbdsmiss, *tlbdsmsize;
extern void     *extint, *extsize;

#if 0 /* XXX: interrupt handler.  We'll get to this later */
extern void	ext_intr(void);
#endif

#ifdef DDB
extern		ddblow, ddbsize;
#endif
#ifdef IPKDB
extern		ipkdblow, ipkdbsize;
#endif

void
powerpc_init(u_int startkernel, u_int endkernel, u_int basekernel, void *mdp)
{
	struct		pcpu *pc;
	vm_offset_t	end, off;
	void		*kmdp;

	end = 0;
	kmdp = NULL;

	/*
	 * Parse metadata if present and fetch parameters.  Must be done
	 * before console is inited so cninit gets the right value of
	 * boothowto.
	 */
	if (mdp != NULL) {
		preload_metadata = mdp;
		kmdp = preload_search_by_type("elf kernel");
		if (kmdp != NULL) {
			boothowto = MD_FETCH(kmdp, MODINFOMD_HOWTO, int);
			kern_envp = MD_FETCH(kmdp, MODINFOMD_ENVP, char *);
			end = MD_FETCH(kmdp, MODINFOMD_KERNEND, vm_offset_t);
		}
	}

	/*
	 * Initialize the console before printing anything.
	 */
	cninit();

	/*
	 * Complain if there is no metadata.
	 */
	if (mdp == NULL || kmdp == NULL) {
		printf("powerpc_init: no loader metadata.\n");
	}

#ifdef DDB
	kdb_init();
#endif
	/*
	 * XXX: Initialize the interrupt tables.
	 */
	bcopy(&trapcode, (void *)EXC_MCHK, (size_t)&trapsize);
	bcopy(&dsitrap,  (void *)EXC_DSI,  (size_t)&dsisize);
	bcopy(&isitrap,  (void *)EXC_ISI,  (size_t)&isisize);
	bcopy(&trapcode, (void *)EXC_EXI,  (size_t)&trapsize);
	bcopy(&trapcode, (void *)EXC_ALI,  (size_t)&trapsize);
	bcopy(&trapcode, (void *)EXC_PGM,  (size_t)&trapsize);
	bcopy(&trapcode, (void *)EXC_FPU,  (size_t)&trapsize);
	bcopy(&trapcode, (void *)EXC_DECR, (size_t)&trapsize);
	bcopy(&trapcode, (void *)EXC_SC,   (size_t)&trapsize);
	bcopy(&trapcode, (void *)EXC_TRC,  (size_t)&trapsize);
	__syncicache(EXC_RSVD, EXC_LAST - EXC_RSVD);

	/*
	 * Start initializing proc0 and thread0.
	 */
	proc_linkup(&proc0, &ksegrp0, &kse0, &thread0);
	proc0.p_uarea = (struct user *)uarea0;
	proc0.p_stats = &proc0.p_uarea->u_stats;
	thread0.td_frame = &frame0;

	/*
	 * Set up per-cpu data.
	 */
	pc = (struct pcpu *)(pcpu0 + PAGE_SIZE) - 1;
	pcpu_init(pc, 0, sizeof(struct pcpu));
	pc->pc_curthread = &thread0;
	pc->pc_curpcb = thread0.td_pcb;
	pc->pc_cpuid = 0;
	/* pc->pc_mid = mid; */

	__asm __volatile("mtsprg 0, %0" :: "r"(pc));

	mutex_init();

	/*
	 * Make sure translation has been enabled
	 */
	mtmsr(mfmsr() | PSL_IR|PSL_DR|PSL_ME|PSL_RI);

	/*
	 * Initialise virtual memory.
	 */
	pmap_bootstrap(startkernel, endkernel);

	/*
	 * Initialize tunables.
	 */
	init_param1();
	init_param2(physmem);

	/*
	 * Finish setting up thread0.
	 */
	thread0.td_kstack = kstack0;
	thread0.td_pcb = (struct pcb *)
	    (thread0.td_kstack + KSTACK_PAGES * PAGE_SIZE) - 1;

	/*
	 * Map and initialise the message buffer.
	 */
	for (off = 0; off < round_page(MSGBUF_SIZE); off += PAGE_SIZE)
		pmap_kenter((vm_offset_t)msgbufp + off, msgbuf_phys + off);
	msgbufinit(msgbufp, MSGBUF_SIZE);
}

void
bzero(void *buf, size_t len)
{
	caddr_t	p;

	p = buf;

	while (((vm_offset_t) p & (sizeof(u_long) - 1)) && len) {
		*p++ = 0;
		len--;
	}

	while (len >= sizeof(u_long) * 8) {
		*(u_long*) p = 0;
		*((u_long*) p + 1) = 0;
		*((u_long*) p + 2) = 0;
		*((u_long*) p + 3) = 0;
		len -= sizeof(u_long) * 8;
		*((u_long*) p + 4) = 0;
		*((u_long*) p + 5) = 0;
		*((u_long*) p + 6) = 0;
		*((u_long*) p + 7) = 0;
		p += sizeof(u_long) * 8;
	}

	while (len >= sizeof(u_long)) {
		*(u_long*) p = 0;
		len -= sizeof(u_long);
		p += sizeof(u_long);
	}

	while (len) {
		*p++ = 0;
		len--;
	}
}

void
sendsig(sig_t catcher, int sig, sigset_t *mask, u_long code)
{
	struct trapframe *tf;
	struct sigframe *sfp;
	struct sigacts *psp;
	struct sigframe sf;
	struct thread *td;
	struct proc *p;
	int oonstack, rndfsize;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	psp = p->p_sigacts;
	mtx_assert(&psp->ps_mtx, MA_OWNED);
	tf = td->td_frame;
	oonstack = sigonstack(tf->fixreg[1]);

	rndfsize = ((sizeof(sf) + 15) / 16) * 16;

	CTR4(KTR_SIG, "sendsig: td=%p (%s) catcher=%p sig=%d", td, p->p_comm,
	     catcher, sig);

	/*
	 * Save user context
	 */
	memset(&sf, 0, sizeof(sf));
	sf.sf_uc.uc_sigmask = *mask;
	sf.sf_uc.uc_stack = p->p_sigstk;
	sf.sf_uc.uc_stack.ss_flags = (p->p_flag & P_ALTSTACK)
	    ? ((oonstack) ? SS_ONSTACK : 0) : SS_DISABLE;

	sf.sf_uc.uc_mcontext.mc_onstack = (oonstack) ? 1 : 0;
	memcpy(&sf.sf_uc.uc_mcontext.mc_frame, tf, sizeof(struct trapframe));

	/*
	 * Allocate and validate space for the signal handler context. 
	 */
	if ((p->p_flag & P_ALTSTACK) != 0 && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		sfp = (struct sigframe *)((caddr_t)p->p_sigstk.ss_sp +
		   p->p_sigstk.ss_size - rndfsize);
	} else {
		sfp = (struct sigframe *)(tf->fixreg[1] - rndfsize);
	}

	/* 
	 * Translate the signal if appropriate (Linux emu ?)
	 */
	if (p->p_sysent->sv_sigtbl && sig <= p->p_sysent->sv_sigsize)
		sig = p->p_sysent->sv_sigtbl[_SIG_IDX(sig)];       

	/*
	 * Save the floating-point state, if necessary, then copy it. 
	 */
	/* XXX */

	/*
	 * Set up the registers to return to sigcode.
	 *
	 *   r1/sp - sigframe ptr
	 *   lr    - sig function, dispatched to by blrl in trampoline
	 *   r3    - sig number
	 *   r4    - SIGINFO ? &siginfo : exception code
	 *   r5    - user context
	 *   srr0  - trampoline function addr
	 */
	tf->lr = (register_t)catcher;
	tf->fixreg[1] = (register_t)sfp;
	tf->fixreg[FIRSTARG] = sig;
	tf->fixreg[FIRSTARG+2] = (register_t)&sfp->sf_uc;
	if (SIGISMEMBER(psp->ps_siginfo, sig)) {
		/* 
		 * Signal handler installed with SA_SIGINFO.
		 */
		tf->fixreg[FIRSTARG+1] = (register_t)&sfp->sf_si;

		/*
		 * Fill siginfo structure.
		 */
		sf.sf_si.si_signo = sig;
		sf.sf_si.si_code = code;
		sf.sf_si.si_addr = (void *)tf->srr0;
	} else {
		/* Old FreeBSD-style arguments. */
		tf->fixreg[FIRSTARG+1] = code;
	}
	mtx_unlock(&psp->ps_mtx);
	PROC_UNLOCK(p);

	tf->srr0 = (register_t)(PS_STRINGS - *(p->p_sysent->sv_szsigcode));

	/*
	 * copy the frame out to userland.
	 */
	if (copyout((caddr_t)&sf, (caddr_t)sfp, sizeof(sf)) != 0) {
		/*
		 * Process has trashed its stack. Kill it.
		 */
		CTR2(KTR_SIG, "sendsig: sigexit td=%p sfp=%p", td, sfp);
		PROC_LOCK(p);
		sigexit(td, SIGILL);
	}

	CTR3(KTR_SIG, "sendsig: return td=%p pc=%#x sp=%#x", td, 
	     tf->srr0, tf->fixreg[1]);

	PROC_LOCK(p);
	mtx_lock(&psp->ps_mtx);
}

/*
 * Build siginfo_t for SA thread
 */
void
thread_siginfo(int sig, u_long code, siginfo_t *si)
{
	struct proc *p;
	struct thread *td;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);

	bzero(si, sizeof(*si));
	si->si_signo = sig;
	si->si_code = code;
	/* XXXKSE fill other fields */
}

int
sigreturn(struct thread *td, struct sigreturn_args *uap)
{
	struct trapframe *tf;
	struct proc *p;
	ucontext_t uc;

	CTR2(KTR_SIG, "sigreturn: td=%p ucp=%p", td, uap->sigcntxp);

	if (copyin(uap->sigcntxp, &uc, sizeof(uc)) != 0) {
		CTR1(KTR_SIG, "sigreturn: efault td=%p", td);
		return (EFAULT);
	}

	/*
	 * Don't let the user set privileged MSR bits
	 */
	tf = td->td_frame;
	if ((uc.uc_mcontext.mc_frame.srr1 & PSL_USERSTATIC) != 
	    (tf->srr1 & PSL_USERSTATIC)) {
		return (EINVAL);
	}

	/*
	 * Restore the user-supplied context
	 */
	memcpy(tf, &uc.uc_mcontext.mc_frame, sizeof(struct trapframe));

	p = td->td_proc;
	PROC_LOCK(p);
	td->td_sigmask = uc.uc_sigmask;
	SIG_CANTMASK(td->td_sigmask);
	signotify(td);
	PROC_UNLOCK(p);

	/*
	 * Restore FP state
	 */
	/* XXX */

	CTR3(KTR_SIG, "sigreturn: return td=%p pc=%#x sp=%#x",
	     td, tf->srr0, tf->fixreg[1]);

	return (EJUSTRETURN);
}

#ifdef COMPAT_FREEBSD4
int
freebsd4_sigreturn(struct thread *td, struct freebsd4_sigreturn_args *uap)
{

	return sigreturn(td, (struct sigreturn_args *)uap);
}
#endif

int
get_mcontext(struct thread *td, mcontext_t *mcp, int clear_ret)
{

	return (ENOSYS);
}

int
set_mcontext(struct thread *td, const mcontext_t *mcp)
{

	return (ENOSYS);
}

void
cpu_boot(int howto)
{
}

/*
 * Shutdown the CPU as much as possible.
 */
void
cpu_halt(void)
{

	OF_exit();
}

/*
 * Set set up registers on exec.
 */
void
exec_setregs(struct thread *td, u_long entry, u_long stack, u_long ps_strings)
{
	struct trapframe	*tf;
	struct ps_strings	arginfo;

	tf = trapframe(td);
	bzero(tf, sizeof *tf);
	tf->fixreg[1] = -roundup(-stack + 8, 16);

	/*
	 * XXX Machine-independent code has already copied arguments and
	 * XXX environment to userland.  Get them back here.
	 */
	(void)copyin((char *)PS_STRINGS, &arginfo, sizeof(arginfo));

	/*
	 * Set up arguments for _start():
	 *	_start(argc, argv, envp, obj, cleanup, ps_strings);
	 *
	 * Notes:
	 *	- obj and cleanup are the auxilliary and termination
	 *	  vectors.  They are fixed up by ld.elf_so.
	 *	- ps_strings is a NetBSD extention, and will be
	 * 	  ignored by executables which are strictly
	 *	  compliant with the SVR4 ABI.
	 *
	 * XXX We have to set both regs and retval here due to different
	 * XXX calling convention in trap.c and init_main.c.
	 */
        /*
         * XXX PG: these get overwritten in the syscall return code.
         * execve() should return EJUSTRETURN, like it does on NetBSD.
         * Emulate by setting the syscall return value cells. The
         * registers still have to be set for init's fork trampoline.
         */
        td->td_retval[0] = arginfo.ps_nargvstr;
        td->td_retval[1] = (register_t)arginfo.ps_argvstr;
	tf->fixreg[3] = arginfo.ps_nargvstr;
	tf->fixreg[4] = (register_t)arginfo.ps_argvstr;
	tf->fixreg[5] = (register_t)arginfo.ps_envstr;
	tf->fixreg[6] = 0;			/* auxillary vector */
	tf->fixreg[7] = 0;			/* termination vector */
	tf->fixreg[8] = (register_t)PS_STRINGS;	/* NetBSD extension */

	tf->srr0 = entry;
	tf->srr1 = PSL_MBO | PSL_USERSET | PSL_FE_DFLT;
	td->td_pcb->pcb_flags = 0;
}

#if !defined(DDB)
void
Debugger(const char *msg)
{

	printf("Debugger(\"%s\") called.\n", msg);
}
#endif /* !defined(DDB) */

/* XXX: dummy {fill,set}_[fp]regs */
int
fill_regs(struct thread *td, struct reg *regs)
{

	return (ENOSYS);
}

int
fill_dbregs(struct thread *td, struct dbreg *dbregs)
{

	return (ENOSYS);
}

int
fill_fpregs(struct thread *td, struct fpreg *fpregs)
{

	return (ENOSYS);
}

int
set_regs(struct thread *td, struct reg *regs)
{

	return (ENOSYS);
}

int
set_dbregs(struct thread *td, struct dbreg *dbregs)
{

	return (ENOSYS);
}

int
set_fpregs(struct thread *td, struct fpreg *fpregs)
{

	return (ENOSYS);
}

int
ptrace_set_pc(struct thread *td, unsigned long addr)
{

	/* XXX: coming soon... */
	return (ENOSYS);
}

int
ptrace_single_step(struct thread *td)
{

	/* XXX: coming soon... */
	return (ENOSYS);
}

/*
 * Initialise a struct pcpu.
 */
void
cpu_pcpu_init(struct pcpu *pcpu, int cpuid, size_t sz)
{

	pcpu->pc_current_asngen = 1;
}

/*
 * kcopy(const void *src, void *dst, size_t len);
 *
 * Copy len bytes from src to dst, aborting if we encounter a fatal
 * page fault.
 *
 * kcopy() _must_ save and restore the old fault handler since it is
 * called by uiomove(), which may be in the path of servicing a non-fatal
 * page fault.
 */
int
kcopy(const void *src, void *dst, size_t len)
{
	struct thread	*td;
	faultbuf	env, *oldfault;
	int		rv;

	td = PCPU_GET(curthread);
	oldfault = td->td_pcb->pcb_onfault;
	if ((rv = setfault(env)) != 0) {
		td->td_pcb->pcb_onfault = oldfault;
		return rv;
	}

	memcpy(dst, src, len);

	td->td_pcb->pcb_onfault = oldfault;
	return (0);
}


intptr_t
casuptr(intptr_t *p, intptr_t old, intptr_t new)
{
	return (-1);
}

