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
/*-
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

#include "opt_compat.h"
#include "opt_ddb.h"
#include "opt_kstack_pages.h"
#include "opt_msgbuf.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <sys/cpu.h>
#include <sys/eventhandler.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/ptrace.h>
#include <sys/reboot.h>
#include <sys/signalvar.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/ucontext.h>
#include <sys/uio.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>

#include <net/netisr.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>

#include <machine/altivec.h>
#include <machine/bat.h>
#include <machine/cpu.h>
#include <machine/elf.h>
#include <machine/fpu.h>
#include <machine/hid.h>
#include <machine/kdb.h>
#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/mmuvar.h>
#include <machine/pcb.h>
#include <machine/reg.h>
#include <machine/sigframe.h>
#include <machine/spr.h>
#include <machine/trap.h>
#include <machine/vmparam.h>

#include <ddb/ddb.h>

#include <dev/ofw/openfirm.h>

#ifdef DDB
extern vm_offset_t ksym_start, ksym_end;
#endif

int cold = 1;
int cacheline_size = 32;
int hw_direct_map = 1;

struct pcpu __pcpu[MAXCPU];

static struct trapframe frame0;

char		machine[] = "powerpc";
SYSCTL_STRING(_hw, HW_MACHINE, machine, CTLFLAG_RD, machine, 0, "");

static void	cpu_startup(void *);
SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL);

SYSCTL_INT(_machdep, CPU_CACHELINE, cacheline_size,
	   CTLFLAG_RD, &cacheline_size, 0, "");

u_int		powerpc_init(u_int, u_int, u_int, void *);

int		save_ofw_mapping(void);
int		restore_ofw_mapping(void);

void		install_extint(void (*)(void));

int             setfault(faultbuf);             /* defined in locore.S */

static int	grab_mcontext(struct thread *, mcontext_t *, int);

void		asm_panic(char *);

long		Maxmem = 0;
long		realmem = 0;

struct pmap	ofw_pmap;
extern int	ofmsr;

struct bat	battable[16];

struct kva_md_info kmi;

static void
powerpc_ofw_shutdown(void *junk, int howto)
{
	if (howto & RB_HALT) {
		OF_halt();
	}
	OF_reboot();
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

#ifdef PERFMON
	perfmon_init();
#endif
	printf("real memory  = %ld (%ld MB)\n", ptoa(physmem),
	    ptoa(physmem) / 1048576);
	realmem = physmem;

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
}

extern char	kernel_text[], _end[];

extern void	*testppc64, *testppc64size;
extern void	*restorebridge, *restorebridgesize;
extern void	*rfid_patch, *rfi_patch1, *rfi_patch2;
#ifdef SMP
extern void	*rstcode, *rstsize;
#endif
extern void	*trapcode, *trapcode64, *trapsize;
extern void	*alitrap, *alisize;
extern void	*dsitrap, *dsisize;
extern void	*decrint, *decrsize;
extern void     *extint, *extsize;
extern void	*dblow, *dbsize;

u_int
powerpc_init(u_int startkernel, u_int endkernel, u_int basekernel, void *mdp)
{
	struct		pcpu *pc;
	vm_offset_t	end;
	void		*generictrap;
	size_t		trap_offset;
	void		*kmdp;
        char		*env;
	uint32_t	msr, scratch;
	uint8_t		*cache_check;
	int		ppc64;

	end = 0;
	kmdp = NULL;
	trap_offset = 0;

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
#ifdef DDB
			ksym_start = MD_FETCH(kmdp, MODINFOMD_SSYM, uintptr_t);
			ksym_end = MD_FETCH(kmdp, MODINFOMD_ESYM, uintptr_t);
#endif
		}
	}

	/*
	 * Init params/tunables that can be overridden by the loader
	 */
	init_param1();

	/*
	 * Start initializing proc0 and thread0.
	 */
	proc_linkup0(&proc0, &thread0);
	thread0.td_frame = &frame0;

	/*
	 * Set up per-cpu data.
	 */
	pc = __pcpu;
	pcpu_init(pc, 0, sizeof(struct pcpu));
	pc->pc_curthread = &thread0;
	pc->pc_cpuid = 0;

	__asm __volatile("mtsprg 0, %0" :: "r"(pc));

	/*
	 * Init mutexes, which we use heavily in PMAP
	 */

	mutex_init();

	/*
	 * Install the OF client interface
	 */

	OF_bootstrap();

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

	/*
	 * Init KDB
	 */

	kdb_init();

	/*
	 * PowerPC 970 CPUs have a misfeature requested by Apple that makes
	 * them pretend they have a 32-byte cacheline. Turn this off
	 * before we measure the cacheline size.
	 */

	switch (mfpvr() >> 16) {
		case IBM970:
		case IBM970FX:
		case IBM970MP:
		case IBM970GX:
			scratch = mfspr64upper(SPR_HID5,msr);
			scratch &= ~HID5_970_DCBZ_SIZE_HI;
			mtspr64(SPR_HID5, scratch, mfspr(SPR_HID5), msr);
			break;
	}

	/*
	 * Initialize the interrupt tables and figure out our cache line
	 * size and whether or not we need the 64-bit bridge code.
	 */

	/*
	 * Disable translation in case the vector area hasn't been
	 * mapped (G5).
	 */

	msr = mfmsr();
	mtmsr((msr & ~(PSL_IR | PSL_DR)) | PSL_RI);
	isync();

	/*
	 * Measure the cacheline size using dcbz
	 *
	 * Use EXC_PGM as a playground. We are about to overwrite it
	 * anyway, we know it exists, and we know it is cache-aligned.
	 */

	cache_check = (void *)EXC_PGM;

	for (cacheline_size = 0; cacheline_size < 0x100; cacheline_size++)
		cache_check[cacheline_size] = 0xff;

	__asm __volatile("dcbz %0,0":: "r" (cache_check) : "memory");

	/* Find the first byte dcbz did not zero to get the cache line size */
	for (cacheline_size = 0; cacheline_size < 0x100 &&
	    cache_check[cacheline_size] == 0; cacheline_size++);

	/* Work around psim bug */
	if (cacheline_size == 0) {
		printf("WARNING: cacheline size undetermined, setting to 32\n");
		cacheline_size = 32;
	}

	/*
	 * Figure out whether we need to use the 64 bit PMAP. This works by
	 * executing an instruction that is only legal on 64-bit PPC (mtmsrd),
	 * and setting ppc64 = 0 if that causes a trap.
	 */

	ppc64 = 1;

	bcopy(&testppc64, (void *)EXC_PGM,  (size_t)&testppc64size);
	__syncicache((void *)EXC_PGM, (size_t)&testppc64size);

	__asm __volatile("\
		mfmsr %0;	\
		mtsprg2 %1;	\
				\
		mtmsrd %0;	\
		mfsprg2 %1;"
	    : "=r"(scratch), "=r"(ppc64));

	if (ppc64)
		cpu_features |= PPC_FEATURE_64;

	/*
	 * Now copy restorebridge into all the handlers, if necessary,
	 * and set up the trap tables.
	 */

	if (cpu_features & PPC_FEATURE_64) {
		/* Patch the two instances of rfi -> rfid */
		bcopy(&rfid_patch,&rfi_patch1,4);
	#ifdef KDB
		/* rfi_patch2 is at the end of dbleave */
		bcopy(&rfid_patch,&rfi_patch2,4);
	#endif

		/*
		 * Copy a code snippet to restore 32-bit bridge mode
		 * to the top of every non-generic trap handler
		 */

		trap_offset += (size_t)&restorebridgesize;
		bcopy(&restorebridge, (void *)EXC_RST, trap_offset); 
		bcopy(&restorebridge, (void *)EXC_DSI, trap_offset); 
		bcopy(&restorebridge, (void *)EXC_ALI, trap_offset); 
		bcopy(&restorebridge, (void *)EXC_PGM, trap_offset); 
		bcopy(&restorebridge, (void *)EXC_MCHK, trap_offset); 
		bcopy(&restorebridge, (void *)EXC_TRC, trap_offset); 
		bcopy(&restorebridge, (void *)EXC_BPT, trap_offset); 

		/*
		 * Set the common trap entry point to the one that
		 * knows to restore 32-bit operation on execution.
		 */

		generictrap = &trapcode64;
	} else {
		generictrap = &trapcode;
	}

#ifdef SMP
	bcopy(&rstcode, (void *)(EXC_RST + trap_offset),  (size_t)&rstsize);
#else
	bcopy(generictrap, (void *)EXC_RST,  (size_t)&trapsize);
#endif

#ifdef KDB
	bcopy(&dblow,	(void *)(EXC_MCHK + trap_offset), (size_t)&dbsize);
	bcopy(&dblow,   (void *)(EXC_PGM + trap_offset),  (size_t)&dbsize);
	bcopy(&dblow,   (void *)(EXC_TRC + trap_offset),  (size_t)&dbsize);
	bcopy(&dblow,   (void *)(EXC_BPT + trap_offset),  (size_t)&dbsize);
#else
	bcopy(generictrap, (void *)EXC_MCHK, (size_t)&trapsize);
	bcopy(generictrap, (void *)EXC_PGM,  (size_t)&trapsize);
	bcopy(generictrap, (void *)EXC_TRC,  (size_t)&trapsize);
	bcopy(generictrap, (void *)EXC_BPT,  (size_t)&trapsize);
#endif
	bcopy(&dsitrap,  (void *)(EXC_DSI + trap_offset),  (size_t)&dsisize);
	bcopy(&alitrap,  (void *)(EXC_ALI + trap_offset),  (size_t)&alisize);
	bcopy(generictrap, (void *)EXC_ISI,  (size_t)&trapsize);
	bcopy(generictrap, (void *)EXC_EXI,  (size_t)&trapsize);
	bcopy(generictrap, (void *)EXC_FPU,  (size_t)&trapsize);
	bcopy(generictrap, (void *)EXC_DECR, (size_t)&trapsize);
	bcopy(generictrap, (void *)EXC_SC,   (size_t)&trapsize);
	bcopy(generictrap, (void *)EXC_FPA,  (size_t)&trapsize);
	bcopy(generictrap, (void *)EXC_VEC,  (size_t)&trapsize);
	bcopy(generictrap, (void *)EXC_VECAST, (size_t)&trapsize);
	bcopy(generictrap, (void *)EXC_THRM, (size_t)&trapsize);
	__syncicache(EXC_RSVD, EXC_LAST - EXC_RSVD);

	/*
	 * Restore MSR
	 */
	mtmsr(msr);
	isync();
	
	/*
	 * Choose a platform module so we can get the physical memory map.
	 */
	
	platform_probe_and_attach();

	/*
	 * Initialise virtual memory. Use BUS_PROBE_GENERIC priority
	 * in case the platform module had a better idea of what we
	 * should do.
	 */
	if (cpu_features & PPC_FEATURE_64)
		pmap_mmu_install(MMU_TYPE_G5, BUS_PROBE_GENERIC);
	else
		pmap_mmu_install(MMU_TYPE_OEA, BUS_PROBE_GENERIC);

	pmap_bootstrap(startkernel, endkernel);
	mtmsr(mfmsr() | PSL_IR|PSL_DR|PSL_ME|PSL_RI);
	isync();

	/*
	 * Initialize params/tunables that are derived from memsize
	 */
	init_param2(physmem);

	/*
	 * Grab booted kernel's name
	 */
        env = getenv("kernelname");
        if (env != NULL) {
		strlcpy(kernelname, env, sizeof(kernelname));
		freeenv(env);
	}

	/*
	 * Finish setting up thread0.
	 */
	thread0.td_pcb = (struct pcb *)
	    ((thread0.td_kstack + thread0.td_kstack_pages * PAGE_SIZE -
	    sizeof(struct pcb)) & ~15);
	bzero((void *)thread0.td_pcb, sizeof(struct pcb));
	pc->pc_curpcb = thread0.td_pcb;

	/* Initialise the message buffer. */
	msgbufinit(msgbufp, MSGBUF_SIZE);

#ifdef KDB
	if (boothowto & RB_KDB)
		kdb_enter(KDB_WHY_BOOTFLAGS,
		    "Boot flags requested debugger");
#endif

	return (((uintptr_t)thread0.td_pcb - 16) & ~15);
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
sendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask)
{
	struct trapframe *tf;
	struct sigframe *sfp;
	struct sigacts *psp;
	struct sigframe sf;
	struct thread *td;
	struct proc *p;
	int oonstack, rndfsize;
	int sig;
	int code;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	sig = ksi->ksi_signo;
	code = ksi->ksi_code;
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
	grab_mcontext(td, &sf.sf_uc.uc_mcontext, 0);
	sf.sf_uc.uc_sigmask = *mask;
	sf.sf_uc.uc_stack = td->td_sigstk;
	sf.sf_uc.uc_stack.ss_flags = (td->td_pflags & TDP_ALTSTACK)
	    ? ((oonstack) ? SS_ONSTACK : 0) : SS_DISABLE;

	sf.sf_uc.uc_mcontext.mc_onstack = (oonstack) ? 1 : 0;

	/*
	 * Allocate and validate space for the signal handler context.
	 */
	if ((td->td_pflags & TDP_ALTSTACK) != 0 && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		sfp = (struct sigframe *)(td->td_sigstk.ss_sp +
		   td->td_sigstk.ss_size - rndfsize);
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
		sf.sf_si = ksi->ksi_info;
		sf.sf_si.si_signo = sig;
		sf.sf_si.si_addr = (void *)((tf->exc == EXC_DSI) ? 
		    tf->cpu.aim.dar : tf->srr0);
	} else {
		/* Old FreeBSD-style arguments. */
		tf->fixreg[FIRSTARG+1] = code;
		tf->fixreg[FIRSTARG+3] = (tf->exc == EXC_DSI) ? 
		    tf->cpu.aim.dar : tf->srr0;
	}
	mtx_unlock(&psp->ps_mtx);
	PROC_UNLOCK(p);

	tf->srr0 = (register_t)(PS_STRINGS - *(p->p_sysent->sv_szsigcode));

	/*
	 * copy the frame out to userland.
	 */
	if (copyout(&sf, sfp, sizeof(*sfp)) != 0) {
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

int
sigreturn(struct thread *td, struct sigreturn_args *uap)
{
	ucontext_t uc;
	int error;

	CTR2(KTR_SIG, "sigreturn: td=%p ucp=%p", td, uap->sigcntxp);

	if (copyin(uap->sigcntxp, &uc, sizeof(uc)) != 0) {
		CTR1(KTR_SIG, "sigreturn: efault td=%p", td);
		return (EFAULT);
	}

	error = set_mcontext(td, &uc.uc_mcontext);
	if (error != 0)
		return (error);

	kern_sigprocmask(td, SIG_SETMASK, &uc.uc_sigmask, NULL, 0);

	CTR3(KTR_SIG, "sigreturn: return td=%p pc=%#x sp=%#x",
	     td, uc.uc_mcontext.mc_srr0, uc.uc_mcontext.mc_gpr[1]);

	return (EJUSTRETURN);
}

#ifdef COMPAT_FREEBSD4
int
freebsd4_sigreturn(struct thread *td, struct freebsd4_sigreturn_args *uap)
{

	return sigreturn(td, (struct sigreturn_args *)uap);
}
#endif

/*
 * Construct a PCB from a trapframe. This is called from kdb_trap() where
 * we want to start a backtrace from the function that caused us to enter
 * the debugger. We have the context in the trapframe, but base the trace
 * on the PCB. The PCB doesn't have to be perfect, as long as it contains
 * enough for a backtrace.
 */
void
makectx(struct trapframe *tf, struct pcb *pcb)
{

	pcb->pcb_lr = tf->srr0;
	pcb->pcb_sp = tf->fixreg[1];
}

/*
 * get_mcontext/sendsig helper routine that doesn't touch the
 * proc lock
 */
static int
grab_mcontext(struct thread *td, mcontext_t *mcp, int flags)
{
	struct pcb *pcb;

	pcb = td->td_pcb;

	memset(mcp, 0, sizeof(mcontext_t));

	mcp->mc_vers = _MC_VERSION;
	mcp->mc_flags = 0;
	memcpy(&mcp->mc_frame, td->td_frame, sizeof(struct trapframe));
	if (flags & GET_MC_CLEAR_RET) {
		mcp->mc_gpr[3] = 0;
		mcp->mc_gpr[4] = 0;
	}

	/*
	 * This assumes that floating-point context is *not* lazy,
	 * so if the thread has used FP there would have been a
	 * FP-unavailable exception that would have set things up
	 * correctly.
	 */
	if (pcb->pcb_flags & PCB_FPU) {
		KASSERT(td == curthread,
			("get_mcontext: fp save not curthread"));
		critical_enter();
		save_fpu(td);
		critical_exit();
		mcp->mc_flags |= _MC_FP_VALID;
		memcpy(&mcp->mc_fpscr, &pcb->pcb_fpu.fpscr, sizeof(double));
		memcpy(mcp->mc_fpreg, pcb->pcb_fpu.fpr, 32*sizeof(double));
	}

	/*
	 * Repeat for Altivec context
	 */

	if (pcb->pcb_flags & PCB_VEC) {
		KASSERT(td == curthread,
			("get_mcontext: fp save not curthread"));
		critical_enter();
		save_vec(td);
		critical_exit();
		mcp->mc_flags |= _MC_AV_VALID;
		mcp->mc_vscr  = pcb->pcb_vec.vscr;
		mcp->mc_vrsave =  pcb->pcb_vec.vrsave;
		memcpy(mcp->mc_avec, pcb->pcb_vec.vr, sizeof(mcp->mc_avec));
	}

	mcp->mc_len = sizeof(*mcp);

	return (0);
}

int
get_mcontext(struct thread *td, mcontext_t *mcp, int flags)
{
	int error;

	error = grab_mcontext(td, mcp, flags);
	if (error == 0) {
		PROC_LOCK(curthread->td_proc);
		mcp->mc_onstack = sigonstack(td->td_frame->fixreg[1]);
		PROC_UNLOCK(curthread->td_proc);
	}

	return (error);
}

int
set_mcontext(struct thread *td, const mcontext_t *mcp)
{
	struct pcb *pcb;
	struct trapframe *tf;

	pcb = td->td_pcb;
	tf = td->td_frame;

	if (mcp->mc_vers != _MC_VERSION ||
	    mcp->mc_len != sizeof(*mcp))
		return (EINVAL);

	/*
	 * Don't let the user set privileged MSR bits
	 */
	if ((mcp->mc_srr1 & PSL_USERSTATIC) != (tf->srr1 & PSL_USERSTATIC)) {
		return (EINVAL);
	}

	memcpy(tf, mcp->mc_frame, sizeof(mcp->mc_frame));

	if (mcp->mc_flags & _MC_FP_VALID) {
		if ((pcb->pcb_flags & PCB_FPU) != PCB_FPU) {
			critical_enter();
			enable_fpu(td);
			critical_exit();
		}
		memcpy(&pcb->pcb_fpu.fpscr, &mcp->mc_fpscr, sizeof(double));
		memcpy(pcb->pcb_fpu.fpr, mcp->mc_fpreg, 32*sizeof(double));
	}

	if (mcp->mc_flags & _MC_AV_VALID) {
		if ((pcb->pcb_flags & PCB_VEC) != PCB_VEC) {
			critical_enter();
			enable_vec(td);
			critical_exit();
		}
		pcb->pcb_vec.vscr = mcp->mc_vscr;
		pcb->pcb_vec.vrsave = mcp->mc_vrsave;
		memcpy(pcb->pcb_vec.vr, mcp->mc_avec, sizeof(mcp->mc_avec));
	}


	return (0);
}

void
cpu_boot(int howto)
{
}

/*
 * Flush the D-cache for non-DMA I/O so that the I-cache can
 * be made coherent later.
 */
void
cpu_flush_dcache(void *ptr, size_t len)
{
	/* TBD */
}

void
cpu_initclocks(void)
{

	decr_tc_init();
	stathz = hz;
	profhz = hz;
}

/*
 * Shutdown the CPU as much as possible.
 */
void
cpu_halt(void)
{

	OF_exit();
}

void
cpu_idle(int busy)
{
	uint32_t msr;
	uint16_t vers;

	msr = mfmsr();
	vers = mfpvr() >> 16;

#ifdef INVARIANTS
	if ((msr & PSL_EE) != PSL_EE) {
		struct thread *td = curthread;
		printf("td msr %x\n", td->td_md.md_saved_msr);
		panic("ints disabled in idleproc!");
	}
#endif
	if (powerpc_pow_enabled) {
		switch (vers) {
		case IBM970:
		case IBM970FX:
		case IBM970MP:
		case MPC7447A:
		case MPC7448:
		case MPC7450:
		case MPC7455:
		case MPC7457:
			__asm __volatile("\
			    dssall; sync; mtmsr %0; isync"
			    :: "r"(msr | PSL_POW));
			break;
		default:
			powerpc_sync();
			mtmsr(msr | PSL_POW);
			isync();
			break;
		}
	}
}

int
cpu_idle_wakeup(int cpu)
{

	return (0);
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

int
fill_regs(struct thread *td, struct reg *regs)
{
	struct trapframe *tf;

	tf = td->td_frame;
	memcpy(regs, tf, sizeof(struct reg));

	return (0);
}

int
fill_dbregs(struct thread *td, struct dbreg *dbregs)
{
	/* No debug registers on PowerPC */
	return (ENOSYS);
}

int
fill_fpregs(struct thread *td, struct fpreg *fpregs)
{
	struct pcb *pcb;

	pcb = td->td_pcb;

	if ((pcb->pcb_flags & PCB_FPU) == 0)
		memset(fpregs, 0, sizeof(struct fpreg));
	else
		memcpy(fpregs, &pcb->pcb_fpu, sizeof(struct fpreg));

	return (0);
}

int
set_regs(struct thread *td, struct reg *regs)
{
	struct trapframe *tf;

	tf = td->td_frame;
	memcpy(tf, regs, sizeof(struct reg));
	
	return (0);
}

int
set_dbregs(struct thread *td, struct dbreg *dbregs)
{
	/* No debug registers on PowerPC */
	return (ENOSYS);
}

int
set_fpregs(struct thread *td, struct fpreg *fpregs)
{
	struct pcb *pcb;

	pcb = td->td_pcb;
	if ((pcb->pcb_flags & PCB_FPU) == 0)
		enable_fpu(td);
	memcpy(&pcb->pcb_fpu, fpregs, sizeof(struct fpreg));

	return (0);
}

int
ptrace_set_pc(struct thread *td, unsigned long addr)
{
	struct trapframe *tf;

	tf = td->td_frame;
	tf->srr0 = (register_t)addr;

	return (0);
}

int
ptrace_single_step(struct thread *td)
{
	struct trapframe *tf;
	
	tf = td->td_frame;
	tf->srr1 |= PSL_SE;

	return (0);
}

int
ptrace_clear_single_step(struct thread *td)
{
	struct trapframe *tf;

	tf = td->td_frame;
	tf->srr1 &= ~PSL_SE;

	return (0);
}

void
kdb_cpu_clear_singlestep(void)
{

	kdb_frame->srr1 &= ~PSL_SE;
}

void
kdb_cpu_set_singlestep(void)
{

	kdb_frame->srr1 |= PSL_SE;
}

/*
 * Initialise a struct pcpu.
 */
void
cpu_pcpu_init(struct pcpu *pcpu, int cpuid, size_t sz)
{

}

void
spinlock_enter(void)
{
	struct thread *td;

	td = curthread;
	if (td->td_md.md_spinlock_count == 0)
		td->td_md.md_saved_msr = intr_disable();
	td->td_md.md_spinlock_count++;
	critical_enter();
}

void
spinlock_exit(void)
{
	struct thread *td;

	td = curthread;
	critical_exit();
	td->td_md.md_spinlock_count--;
	if (td->td_md.md_spinlock_count == 0)
		intr_restore(td->td_md.md_saved_msr);
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

void
asm_panic(char *pstr)
{
	panic(pstr);
}

int db_trap_glue(struct trapframe *);		/* Called from trap_subr.S */

int
db_trap_glue(struct trapframe *frame)
{
	if (!(frame->srr1 & PSL_PR)
	    && (frame->exc == EXC_TRC || frame->exc == EXC_RUNMODETRC
		|| (frame->exc == EXC_PGM
		    && (frame->srr1 & 0x20000))
		|| frame->exc == EXC_BPT
		|| frame->exc == EXC_DSI)) {
		int type = frame->exc;
		if (type == EXC_PGM && (frame->srr1 & 0x20000)) {
			type = T_BREAKPOINT;
		}
		return (kdb_trap(type, 0, frame));
	}

	return (0);
}
