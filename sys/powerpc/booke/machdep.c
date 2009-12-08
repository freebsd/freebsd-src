/*-
 * Copyright (C) 2006 Semihalf, Marian Balakowicz <m8@semihalf.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
 * $NetBSD: machdep.c,v 1.74.2.1 2000/11/01 16:13:48 tv Exp $
 */
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"
#include "opt_ddb.h"
#include "opt_kstack_pages.h"
#include "opt_msgbuf.h"

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <sys/cpu.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/exec.h>
#include <sys/ktr.h>
#include <sys/sysproto.h>
#include <sys/signalvar.h>
#include <sys/sysent.h>
#include <sys/imgact.h>
#include <sys/msgbuf.h>
#include <sys/ptrace.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>

#include <machine/cpu.h>
#include <machine/kdb.h>
#include <machine/reg.h>
#include <machine/vmparam.h>
#include <machine/spr.h>
#include <machine/hid.h>
#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/md_var.h>
#include <machine/mmuvar.h>
#include <machine/sigframe.h>
#include <machine/metadata.h>
#include <machine/bootinfo.h>
#include <machine/platform.h>

#include <sys/linker.h>
#include <sys/reboot.h>

#include <powerpc/mpc85xx/ocpbus.h>
#include <powerpc/mpc85xx/mpc85xx.h>

#ifdef DDB
extern vm_offset_t ksym_start, ksym_end;
#endif

#ifdef  DEBUG
#define debugf(fmt, args...) printf(fmt, ##args)
#else
#define debugf(fmt, args...)
#endif

extern unsigned char kernel_text[];
extern unsigned char _etext[];
extern unsigned char _edata[];
extern unsigned char __bss_start[];
extern unsigned char __sbss_start[];
extern unsigned char __sbss_end[];
extern unsigned char _end[];

extern void dcache_enable(void);
extern void dcache_inval(void);
extern void icache_enable(void);
extern void icache_inval(void);

struct kva_md_info kmi;
struct pcpu __pcpu[MAXCPU];
struct trapframe frame0;
int cold = 1;
long realmem = 0;
long Maxmem = 0;

struct bootinfo *bootinfo;

char machine[] = "powerpc";
SYSCTL_STRING(_hw, HW_MACHINE, machine, CTLFLAG_RD, machine, 0, "");

int cacheline_size = 32;

SYSCTL_INT(_machdep, CPU_CACHELINE, cacheline_size,
	   CTLFLAG_RD, &cacheline_size, 0, "");

int hw_direct_map = 0;

static void cpu_e500_startup(void *);
SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_e500_startup, NULL);

void print_kernel_section_addr(void);
void print_bootinfo(void);
void print_kenv(void);
u_int e500_init(u_int32_t, u_int32_t, void *);

static void
cpu_e500_startup(void *dummy)
{
	int indx, size;

	/* Initialise the decrementer-based clock. */
	decr_init();

	/* Good {morning,afternoon,evening,night}. */
	cpu_setup(PCPU_GET(cpuid));

	printf("real memory  = %ld (%ld MB)\n", ptoa(physmem),
	    ptoa(physmem) / 1048576);
	realmem = physmem;

	/* Display any holes after the first chunk of extended memory. */
	if (bootverbose) {
		printf("Physical memory chunk(s):\n");
		for (indx = 0; phys_avail[indx + 1] != 0; indx += 2) {
			size = phys_avail[indx + 1] - phys_avail[indx];

			printf("0x%08x - 0x%08x, %d bytes (%d pages)\n",
			    phys_avail[indx], phys_avail[indx + 1] - 1,
			    size, size / PAGE_SIZE);
		}
	}

	vm_ksubmap_init(&kmi);

	printf("avail memory = %ld (%ld MB)\n", ptoa(cnt.v_free_count),
	    ptoa(cnt.v_free_count) / 1048576);

	/* Set up buffers, so they can be used to read disk labels. */
	bufinit();
	vm_pager_bufferinit();
}

static char *
kenv_next(char *cp)
{

	if (cp != NULL) {
		while (*cp != 0)
			cp++;
		cp++;
		if (*cp == 0)
			cp = NULL;
	}
	return (cp);
}

void
print_kenv(void)
{
	int len;
	char *cp;

	debugf("loader passed (static) kenv:\n");
	if (kern_envp == NULL) {
		debugf(" no env, null ptr\n");
		return;
	}
	debugf(" kern_envp = 0x%08x\n", (u_int32_t)kern_envp);

	len = 0;
	for (cp = kern_envp; cp != NULL; cp = kenv_next(cp))
		debugf(" %x %s\n", (u_int32_t)cp, cp);
}

void
print_bootinfo(void)
{
	struct bi_mem_region *mr;
	struct bi_eth_addr *eth;
	int i, j;

	debugf("bootinfo:\n");
	if (bootinfo == NULL) {
		debugf(" no bootinfo, null ptr\n");
		return;
	}

	debugf(" version = 0x%08x\n", bootinfo->bi_version);
	debugf(" ccsrbar = 0x%08x\n", bootinfo->bi_bar_base);
	debugf(" cpu_clk = 0x%08x\n", bootinfo->bi_cpu_clk);
	debugf(" bus_clk = 0x%08x\n", bootinfo->bi_bus_clk);

	debugf(" mem regions:\n");
	mr = (struct bi_mem_region *)bootinfo->bi_data;
	for (i = 0; i < bootinfo->bi_mem_reg_no; i++, mr++)
		debugf("    #%d, base = 0x%08x, size = 0x%08x\n", i,
		    mr->mem_base, mr->mem_size);

	debugf(" eth addresses:\n");
	eth = (struct bi_eth_addr *)mr;
	for (i = 0; i < bootinfo->bi_eth_addr_no; i++, eth++) {
		debugf("    #%d, addr = ", i);
		for (j = 0; j < 6; j++)
			debugf("%02x ", eth->mac_addr[j]);
		debugf("\n");
	}
}

void
print_kernel_section_addr(void)
{

	debugf("kernel image addresses:\n");
	debugf(" kernel_text    = 0x%08x\n", (uint32_t)kernel_text);
	debugf(" _etext (sdata) = 0x%08x\n", (uint32_t)_etext);
	debugf(" _edata         = 0x%08x\n", (uint32_t)_edata);
	debugf(" __sbss_start   = 0x%08x\n", (uint32_t)__sbss_start);
	debugf(" __sbss_end     = 0x%08x\n", (uint32_t)__sbss_end);
	debugf(" __sbss_start   = 0x%08x\n", (uint32_t)__bss_start);
	debugf(" _end           = 0x%08x\n", (uint32_t)_end);
}

struct bi_mem_region *
bootinfo_mr(void)
{

	return ((struct bi_mem_region *)bootinfo->bi_data);
}

struct bi_eth_addr *
bootinfo_eth(void)
{
	struct bi_mem_region *mr;
	struct bi_eth_addr *eth;
	int i;

	/* Advance to the eth section */
	mr = bootinfo_mr();
	for (i = 0; i < bootinfo->bi_mem_reg_no; i++, mr++)
		;

	eth = (struct bi_eth_addr *)mr;
	return (eth);
}

u_int
e500_init(u_int32_t startkernel, u_int32_t endkernel, void *mdp)
{
	struct pcpu *pc;
	void *kmdp;
	vm_offset_t end;
	uint32_t csr;

	kmdp = NULL;

	end = endkernel;

	/*
	 * Parse metadata and fetch parameters. This must be done as the first
	 * step as we need bootinfo data to at least init the console
	 */
	if (mdp != NULL) {
		preload_metadata = mdp;
		kmdp = preload_search_by_type("elf kernel");
		if (kmdp != NULL) {
			bootinfo = (struct bootinfo *)preload_search_info(kmdp,
			    MODINFO_METADATA | MODINFOMD_BOOTINFO);

			boothowto = MD_FETCH(kmdp, MODINFOMD_HOWTO, int);
			kern_envp = MD_FETCH(kmdp, MODINFOMD_ENVP, char *);
			end = MD_FETCH(kmdp, MODINFOMD_KERNEND, vm_offset_t);
#ifdef DDB
			ksym_start = MD_FETCH(kmdp, MODINFOMD_SSYM, uintptr_t);
			ksym_end = MD_FETCH(kmdp, MODINFOMD_ESYM, uintptr_t);
#endif
		}
	} else {
		/*
		 * We should scream but how? - without CCSR bar (in bootinfo) 
		 * cannot even output anything...
		 */

		 /*
		  * FIXME add return value and handle in the locore so we can
		  * return to the loader maybe? (this seems not very easy to
		  * restore everything as the TLB have all been reprogrammed
		  * in the locore etc...)
		  */
		while(1);
	}

	/* Initialize TLB1 handling */
	tlb1_init(bootinfo->bi_bar_base);

	/* Reset Time Base */
	mttb(0);

	/* Init params/tunables that can be overridden by the loader. */
	init_param1();

	/* Start initializing proc0 and thread0. */
	proc_linkup(&proc0, &thread0);
	thread0.td_frame = &frame0;

	/* Set up per-cpu data and store the pointer in SPR general 0. */
	pc = &__pcpu[0];
	pcpu_init(pc, 0, sizeof(struct pcpu));
	pc->pc_curthread = &thread0;
	__asm __volatile("mtsprg 0, %0" :: "r"(pc));

	/* Initialize system mutexes. */
	mutex_init();

	/* Initialize the console before printing anything. */
	cninit();

	/* Print out some debug info... */
	debugf("e500_init: console initialized\n");
	debugf(" arg1 startkernel = 0x%08x\n", startkernel);
	debugf(" arg2 endkernel = 0x%08x\n", endkernel);
	debugf(" arg3 mdp = 0x%08x\n", (u_int32_t)mdp);
	debugf(" end = 0x%08x\n", (u_int32_t)end);
	debugf(" boothowto = 0x%08x\n", boothowto);
	debugf(" kernel ccsrbar = 0x%08x\n", CCSRBAR_VA);
	debugf(" MSR = 0x%08x\n", mfmsr());
	debugf(" HID0 = 0x%08x\n", mfspr(SPR_HID0));
	debugf(" HID1 = 0x%08x\n", mfspr(SPR_HID1));
	debugf(" BUCSR = 0x%08x\n", mfspr(SPR_BUCSR));

	__asm __volatile("msync; isync");
	csr = ccsr_read4(OCP85XX_L2CTL);
	debugf(" L2CTL = 0x%08x\n", csr);

	print_bootinfo();
	print_kernel_section_addr();
	print_kenv();
	//tlb1_print_entries();
	//tlb1_print_tlbentries();

	kdb_init();

#ifdef KDB
	if (boothowto & RB_KDB)
		kdb_enter(KDB_WHY_BOOTFLAGS, "Boot flags requested debugger");
#endif

	/* Initialise platform module */
	platform_probe_and_attach();

	/* Initialise virtual memory. */
	pmap_mmu_install(MMU_TYPE_BOOKE, 0);
	pmap_bootstrap(startkernel, end);
	debugf("MSR = 0x%08x\n", mfmsr());
	//tlb1_print_entries();
	//tlb1_print_tlbentries();

	/* Initialize params/tunables that are derived from memsize. */
	init_param2(physmem);

	/* Finish setting up thread0. */
	thread0.td_pcb = (struct pcb *)
	    ((thread0.td_kstack + thread0.td_kstack_pages * PAGE_SIZE -
	    sizeof(struct pcb)) & ~15);
	bzero((void *)thread0.td_pcb, sizeof(struct pcb));
	pc->pc_curpcb = thread0.td_pcb;

	/* Initialise the message buffer. */
	msgbufinit(msgbufp, MSGBUF_SIZE);

	/* Enable Machine Check interrupt. */
	mtmsr(mfmsr() | PSL_ME);
	isync();

	/* Enable D-cache if applicable */
	csr = mfspr(SPR_L1CSR0);
	if ((csr & L1CSR0_DCE) == 0) {
		dcache_inval();
		dcache_enable();
	}

	csr = mfspr(SPR_L1CSR0);
	if ((boothowto & RB_VERBOSE) != 0 || (csr & L1CSR0_DCE) == 0)
		printf("L1 D-cache %sabled\n",
		    (csr & L1CSR0_DCE) ? "en" : "dis");

	/* Enable L1 I-cache if applicable. */
	csr = mfspr(SPR_L1CSR1);
	if ((csr & L1CSR1_ICE) == 0) {
		icache_inval();
		icache_enable();
	}

	csr = mfspr(SPR_L1CSR1);
	if ((boothowto & RB_VERBOSE) != 0 || (csr & L1CSR1_ICE) == 0)
		printf("L1 I-cache %sabled\n",
		    (csr & L1CSR1_ICE) ? "en" : "dis");

	debugf("e500_init: SP = 0x%08x\n", ((uintptr_t)thread0.td_pcb - 16) & ~15);
	debugf("e500_init: e\n");

	return (((uintptr_t)thread0.td_pcb - 16) & ~15);
}

#define RES_GRANULE 32
extern uint32_t tlb0_miss_locks[];

/* Initialise a struct pcpu. */
void
cpu_pcpu_init(struct pcpu *pcpu, int cpuid, size_t sz)
{

	pcpu->pc_tid_next = TID_MIN;

#ifdef SMP
	uint32_t *ptr;
	int words_per_gran = RES_GRANULE / sizeof(uint32_t);

	ptr = &tlb0_miss_locks[cpuid * words_per_gran];
	pcpu->pc_booke_tlb_lock = ptr;
	*ptr = MTX_UNOWNED;
	*(ptr + 1) = 0;		/* recurse counter */
#endif
}

/* Set set up registers on exec. */
void
exec_setregs(struct thread *td, u_long entry, u_long stack, u_long ps_strings)
{
	struct trapframe *tf;
	struct ps_strings arginfo;

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
	tf->srr1 = PSL_USERSET;
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
fill_fpregs(struct thread *td, struct fpreg *fpregs)
{

	return (0);
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
 * proc lock.
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

	/* XXX Altivec context ? */

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

	if (mcp->mc_vers != _MC_VERSION || mcp->mc_len != sizeof(*mcp))
		return (EINVAL);

	memcpy(tf, mcp->mc_frame, sizeof(mcp->mc_frame));

	/* XXX Altivec context? */

	return (0);
}

int
sigreturn(struct thread *td, struct sigreturn_args *uap)
{
	struct proc *p;
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

	p = td->td_proc;
	PROC_LOCK(p);
	td->td_sigmask = uc.uc_sigmask;
	SIG_CANTMASK(td->td_sigmask);
	signotify(td);
	PROC_UNLOCK(p);

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
 * cpu_idle
 *
 * Set Wait state enable.
 */
void
cpu_idle (int busy)
{
	register_t msr;

	msr = mfmsr();
#ifdef INVARIANTS
	if ((msr & PSL_EE) != PSL_EE) {
		struct thread *td = curthread;
		printf("td msr %x\n", td->td_md.md_saved_msr);
		panic("ints disabled in idleproc!");
	}
#endif
#if 0
	/*
	 * Freescale E500 core RM section 6.4.1
	 */
	msr = msr | PSL_WE;

	__asm__("	msync;"
		"	mtmsr	%0;"
		"	isync;"
		"loop:	b	loop" :
		/* no output */	:
		"r" (msr));
#endif
}

int
cpu_idle_wakeup(int cpu)
{

	return (0);
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

/* Shutdown the CPU as much as possible. */
void
cpu_halt(void)
{

	mtmsr(mfmsr() & ~(PSL_CE | PSL_EE | PSL_ME | PSL_DE));
	while (1);
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
fill_dbregs(struct thread *td, struct dbreg *dbregs)
{

	/* No debug registers on PowerPC */
	return (ENOSYS);
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
	tf->srr1 |= PSL_DE;
	tf->cpu.booke.dbcr0 |= (DBCR0_IDM | DBCR0_IC);
	return (0);
}

int
ptrace_clear_single_step(struct thread *td)
{
	struct trapframe *tf;

	tf = td->td_frame;
	tf->srr1 &= ~PSL_DE;
	tf->cpu.booke.dbcr0 &= ~(DBCR0_IDM | DBCR0_IC);
	return (0);
}

void
kdb_cpu_clear_singlestep(void)
{
	register_t r;

	r = mfspr(SPR_DBCR0);
	mtspr(SPR_DBCR0, r & ~DBCR0_IC);
	kdb_frame->srr1 &= ~PSL_DE;
}

void
kdb_cpu_set_singlestep(void)
{
	register_t r;

	r = mfspr(SPR_DBCR0);
	mtspr(SPR_DBCR0, r | DBCR0_IC | DBCR0_IDM);
	kdb_frame->srr1 |= PSL_DE;
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
	int sig, code;

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
		sfp = (struct sigframe *)((caddr_t)td->td_sigstk.ss_sp +
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
		sf.sf_si.si_addr = (void *) ((tf->exc == EXC_DSI) ?
		    tf->cpu.booke.dear : tf->srr0);
	} else {
		/* Old FreeBSD-style arguments. */
		tf->fixreg[FIRSTARG+1] = code;
		tf->fixreg[FIRSTARG+3] = (tf->exc == EXC_DSI) ?
		    tf->cpu.booke.dear : tf->srr0;
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

void
bzero(void *buf, size_t len)
{
	caddr_t p;

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

/*
 * XXX what is the better/proper place for this routine?
 */
int
mem_valid(vm_offset_t addr, int len)
{

	return (1);
}
