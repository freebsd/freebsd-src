/*-
 * Copyright (c) 1992 Terrence R. Lambert.
 * Copyright (c) 1982, 1987, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	from: @(#)machdep.c	7.4 (Berkeley) 6/3/91
 *	$Id: machdep.c,v 1.3 1996/08/30 10:42:53 asami Exp $
 */

#include "npx.h"
#include "opt_sysvipc.h"
#include "opt_ddb.h"
#include "opt_bounce.h"
#include "opt_machdep.h"
#include "opt_perfmon.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/msgbuf.h>
#include <sys/ioctl.h>
#include <sys/sysent.h>
#include <sys/tty.h>
#include <sys/sysctl.h>
#include <sys/devconf.h>
#include <sys/vmmeter.h>

#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#ifdef SYSVMSG
#include <sys/msg.h>
#endif

#ifdef SYSVSEM
#include <sys/sem.h>
#endif

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <vm/lock.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_pager.h>
#include <vm/vm_extern.h>

#include <sys/user.h>
#include <sys/exec.h>
#include <sys/vnode.h>

#include <ddb/ddb.h>

#include <net/netisr.h>

#include <machine/cpu.h>
#include <machine/npx.h>
#include <machine/reg.h>
#include <machine/psl.h>
#include <machine/clock.h>
#include <machine/specialreg.h>
#include <machine/sysarch.h>
#include <machine/cons.h>
#include <machine/devconf.h>
#include <machine/bootinfo.h>
#include <machine/md_var.h>
#ifdef PERFMON
#include <machine/perfmon.h>
#endif

#include <i386/isa/isa_device.h>
#ifdef PC98
#include <pc98/pc98/pc98_machdep.h>
#else
#include <i386/isa/rtc.h>
#endif
#include <machine/random.h>

extern void init386 __P((int first));
extern int ptrace_set_pc __P((struct proc *p, unsigned int addr));
extern int ptrace_single_step __P((struct proc *p));
extern int ptrace_write_u __P((struct proc *p, vm_offset_t off, int data));
extern void dblfault_handler __P((void));

extern void identifycpu(void);	/* XXX header file */
extern void earlysetcpuclass(void);	/* same header file */

static void cpu_startup __P((void *));
SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL)


#ifdef BOUNCE_BUFFERS
extern char *bouncememory;
extern int maxbkva;
#ifdef BOUNCEPAGES
int	bouncepages = BOUNCEPAGES;
#else
int	bouncepages = 0;
#endif
#endif	/* BOUNCE_BUFFERS */

extern int freebufspace;
int	msgbufmapped = 0;		/* set when safe to use msgbuf */
int _udatasel, _ucodesel;
u_int	atdevbase;


int physmem = 0;
int cold = 1;

static int
sysctl_hw_physmem SYSCTL_HANDLER_ARGS
{
	int error = sysctl_handle_int(oidp, 0, ctob(physmem), req);
	return (error);
}

SYSCTL_PROC(_hw, HW_PHYSMEM, physmem, CTLTYPE_INT|CTLFLAG_RD,
	0, 0, sysctl_hw_physmem, "I", "");

static int
sysctl_hw_usermem SYSCTL_HANDLER_ARGS
{
	int error = sysctl_handle_int(oidp, 0,
		ctob(physmem - cnt.v_wire_count), req);
	return (error);
}

SYSCTL_PROC(_hw, HW_USERMEM, usermem, CTLTYPE_INT|CTLFLAG_RD,
	0, 0, sysctl_hw_usermem, "I", "");

int boothowto = 0, bootverbose = 0, Maxmem = 0;
static int	badpages = 0;
#ifdef PC98
int Maxmem_under16M = 0;
#endif
long dumplo;
extern int bootdev;

vm_offset_t phys_avail[10];

/* must be 2 less so 0 0 can signal end of chunks */
#define PHYS_AVAIL_ARRAY_END ((sizeof(phys_avail) / sizeof(vm_offset_t)) - 2)

static void setup_netisrs __P((struct linker_set *)); /* XXX declare elsewhere */

static vm_offset_t buffer_sva, buffer_eva;
vm_offset_t clean_sva, clean_eva;
static vm_offset_t pager_sva, pager_eva;
extern struct linker_set netisr_set;

#define offsetof(type, member)	((size_t)(&((type *)0)->member))

static void
cpu_startup(dummy)
	void *dummy;
{
	register unsigned i;
	register caddr_t v;
	vm_offset_t maxaddr;
	vm_size_t size = 0;
	int firstaddr;
	vm_offset_t minaddr;

	if (boothowto & RB_VERBOSE)
		bootverbose++;

	/*
	 * Initialize error message buffer (at end of core).
	 */

	/* avail_end was pre-decremented in init386() to compensate */
	for (i = 0; i < btoc(sizeof (struct msgbuf)); i++)
		pmap_enter(pmap_kernel(), (vm_offset_t)msgbufp,
			   avail_end + i * PAGE_SIZE,
			   VM_PROT_ALL, TRUE);
	msgbufmapped = 1;

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	earlysetcpuclass();
	startrtclock();
	identifycpu();
#ifdef PERFMON
	perfmon_init();
#endif
	printf("real memory  = %d (%dK bytes)\n", ptoa(Maxmem), ptoa(Maxmem) / 1024);
	/*
	 * Display any holes after the first chunk of extended memory.
	 */
	if (badpages != 0) {
		int indx = 1;

		/*
		 * XXX skip reporting ISA hole & unmanaged kernel memory
		 */
		if (phys_avail[0] == PAGE_SIZE)
			indx += 2;

		printf("Physical memory hole(s):\n");
		for (; phys_avail[indx + 1] != 0; indx += 2) {
			int size = phys_avail[indx + 1] - phys_avail[indx];

			printf("0x%08lx - 0x%08lx, %d bytes (%d pages)\n", phys_avail[indx],
			    phys_avail[indx + 1] - 1, size, size / PAGE_SIZE);
		}
	}

	/*
	 * Quickly wire in netisrs.
	 */
	setup_netisrs(&netisr_set);

/*
#ifdef ISDN
	DONET(isdnintr, NETISR_ISDN);
#endif
*/

	/*
	 * Allocate space for system data structures.
	 * The first available kernel virtual address is in "v".
	 * As pages of kernel virtual memory are allocated, "v" is incremented.
	 * As pages of memory are allocated and cleared,
	 * "firstaddr" is incremented.
	 * An index into the kernel page table corresponding to the
	 * virtual memory address maintained in "v" is kept in "mapaddr".
	 */

	/*
	 * Make two passes.  The first pass calculates how much memory is
	 * needed and allocates it.  The second pass assigns virtual
	 * addresses to the various data structures.
	 */
	firstaddr = 0;
again:
	v = (caddr_t)firstaddr;

#define	valloc(name, type, num) \
	    (name) = (type *)v; v = (caddr_t)((name)+(num))
#define	valloclim(name, type, num, lim) \
	    (name) = (type *)v; v = (caddr_t)((lim) = ((name)+(num)))
	valloc(callout, struct callout, ncallout);
#ifdef SYSVSHM
	valloc(shmsegs, struct shmid_ds, shminfo.shmmni);
#endif
#ifdef SYSVSEM
	valloc(sema, struct semid_ds, seminfo.semmni);
	valloc(sem, struct sem, seminfo.semmns);
	/* This is pretty disgusting! */
	valloc(semu, int, (seminfo.semmnu * seminfo.semusz) / sizeof(int));
#endif
#ifdef SYSVMSG
	valloc(msgpool, char, msginfo.msgmax);
	valloc(msgmaps, struct msgmap, msginfo.msgseg);
	valloc(msghdrs, struct msg, msginfo.msgtql);
	valloc(msqids, struct msqid_ds, msginfo.msgmni);
#endif

	if (nbuf == 0) {
		nbuf = 30;
		if( physmem > 1024)
			nbuf += min((physmem - 1024) / 12, 1024);
	}
	nswbuf = min(nbuf, 128);

	valloc(swbuf, struct buf, nswbuf);
	valloc(buf, struct buf, nbuf);

#ifdef BOUNCE_BUFFERS
	/*
	 * If there is more than 16MB of memory, allocate some bounce buffers
	 */
	if (Maxmem > 4096) {
		if (bouncepages == 0) {
			bouncepages = 64;
			bouncepages += ((Maxmem - 4096) / 2048) * 32;
		}
		v = (caddr_t)((vm_offset_t)round_page(v));
		valloc(bouncememory, char, bouncepages * PAGE_SIZE);
	}
#endif

	/*
	 * End of first pass, size has been calculated so allocate memory
	 */
	if (firstaddr == 0) {
		size = (vm_size_t)(v - firstaddr);
		firstaddr = (int)kmem_alloc(kernel_map, round_page(size));
		if (firstaddr == 0)
			panic("startup: no room for tables");
		goto again;
	}

	/*
	 * End of second pass, addresses have been assigned
	 */
	if ((vm_size_t)(v - firstaddr) != size)
		panic("startup: table size inconsistency");

#ifdef BOUNCE_BUFFERS
	clean_map = kmem_suballoc(kernel_map, &clean_sva, &clean_eva,
			(nbuf*MAXBSIZE) + (nswbuf*MAXPHYS) +
				maxbkva + pager_map_size, TRUE);
	io_map = kmem_suballoc(clean_map, &minaddr, &maxaddr, maxbkva, FALSE);
#else
	clean_map = kmem_suballoc(kernel_map, &clean_sva, &clean_eva,
			(nbuf*MAXBSIZE) + (nswbuf*MAXPHYS) + pager_map_size, TRUE);
#endif
	buffer_map = kmem_suballoc(clean_map, &buffer_sva, &buffer_eva,
				(nbuf*MAXBSIZE), TRUE);
	pager_map = kmem_suballoc(clean_map, &pager_sva, &pager_eva,
				(nswbuf*MAXPHYS) + pager_map_size, TRUE);
	exec_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				(16*ARG_MAX), TRUE);
	exech_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				(32*ARG_MAX), TRUE);
	u_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				(maxproc*UPAGES*PAGE_SIZE), FALSE);

	/*
	 * Finally, allocate mbuf pool.  Since mclrefcnt is an off-size
	 * we use the more space efficient malloc in place of kmem_alloc.
	 */
	mclrefcnt = (char *)malloc(nmbclusters+PAGE_SIZE/MCLBYTES,
				   M_MBUF, M_NOWAIT);
	bzero(mclrefcnt, nmbclusters+PAGE_SIZE/MCLBYTES);
	mcl_map = kmem_suballoc(kmem_map, (vm_offset_t *)&mbutl, &maxaddr,
			       nmbclusters * MCLBYTES, FALSE);
	{
		vm_size_t mb_map_size;
		mb_map_size = nmbufs * MSIZE;
		mb_map = kmem_suballoc(kmem_map, &minaddr, &maxaddr, 
				       round_page(mb_map_size), FALSE);
	}

	/*
	 * Initialize callouts
	 */
	callfree = callout;
	for (i = 1; i < ncallout; i++)
		callout[i-1].c_next = &callout[i];

        if (boothowto & RB_CONFIG) {
		userconfig();
		cninit();	/* the preferred console may have changed */
	}

#ifdef BOUNCE_BUFFERS
	/*
	 * init bounce buffers
	 */
	vm_bounce_init();
#endif

	printf("avail memory = %d (%dK bytes)\n", ptoa(cnt.v_free_count),
	    ptoa(cnt.v_free_count) / 1024);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();
	vm_pager_bufferinit();

	/*
	 * In verbose mode, print out the BIOS's idea of the disk geometries.
	 */
	if (bootverbose) {
		printf("BIOS Geometries:\n");
		for (i = 0; i < N_BIOS_GEOM; i++) {
			unsigned long bios_geom;
			int max_cylinder, max_head, max_sector;

			bios_geom = bootinfo.bi_bios_geom[i];

			/*
			 * XXX the bootstrap punts a 1200K floppy geometry
			 * when the get-disk-geometry interrupt fails.  Skip
			 * drives that have this geometry.
			 */
			if (bios_geom == 0x4f010f)
				continue;

			printf(" %x:%08lx ", i, bios_geom);
			max_cylinder = bios_geom >> 16;
			max_head = (bios_geom >> 8) & 0xff;
			max_sector = bios_geom & 0xff;
			printf(
		"0..%d=%d cylinders, 0..%d=%d heads, 1..%d=%d sectors\n",
			       max_cylinder, max_cylinder + 1,
			       max_head, max_head + 1,
			       max_sector, max_sector);
		}
		printf(" %d accounted for\n", bootinfo.bi_n_bios_used);
	}
}

int
register_netisr(num, handler)
	int num;
	netisr_t *handler;
{
	
	if (num < 0 || num >= (sizeof(netisrs)/sizeof(*netisrs)) ) {
		printf("register_netisr: bad isr number: %d\n", num);
		return (EINVAL);
	}
	netisrs[num] = handler;
	return (0);
}

static void
setup_netisrs(ls)
	struct linker_set *ls;
{
	int i;
	const struct netisrtab *nit;

	for(i = 0; ls->ls_items[i]; i++) {
		nit = (const struct netisrtab *)ls->ls_items[i];
		register_netisr(nit->nit_num, nit->nit_isr);
	}
}

/*
 * Send an interrupt to process.
 *
 * Stack is set up to allow sigcode stored
 * at top to call routine, followed by kcall
 * to sigreturn routine below.  After sigreturn
 * resets the signal mask, the stack, and the
 * frame pointer, it returns to the user
 * specified pc, psl.
 */
void
sendsig(catcher, sig, mask, code)
	sig_t catcher;
	int sig, mask;
	u_long code;
{
	register struct proc *p = curproc;
	register int *regs;
	register struct sigframe *fp;
	struct sigframe sf;
	struct sigacts *psp = p->p_sigacts;
	int oonstack;

	regs = p->p_md.md_regs;
        oonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;
	/*
	 * Allocate and validate space for the signal handler context.
	 */
        if ((psp->ps_flags & SAS_ALTSTACK) && !oonstack &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct sigframe *)(psp->ps_sigstk.ss_sp +
		    psp->ps_sigstk.ss_size - sizeof(struct sigframe));
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else {
		fp = (struct sigframe *)regs[tESP] - 1;
	}

	/*
	 * grow() will return FALSE if the fp will not fit inside the stack
	 *	and the stack can not be grown. useracc will return FALSE
	 *	if access is denied.
	 */
	if ((grow(p, (int)fp) == FALSE) ||
	    (useracc((caddr_t)fp, sizeof (struct sigframe), B_WRITE) == FALSE)) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		SIGACTION(p, SIGILL) = SIG_DFL;
		sig = sigmask(SIGILL);
		p->p_sigignore &= ~sig;
		p->p_sigcatch &= ~sig;
		p->p_sigmask &= ~sig;
		psignal(p, SIGILL);
		return;
	}

	/*
	 * Build the argument list for the signal handler.
	 */
	if (p->p_sysent->sv_sigtbl) {
		if (sig < p->p_sysent->sv_sigsize)
			sig = p->p_sysent->sv_sigtbl[sig];
		else
			sig = p->p_sysent->sv_sigsize + 1;
	}
	sf.sf_signum = sig;
	sf.sf_code = code;
	sf.sf_scp = &fp->sf_sc;
	sf.sf_addr = (char *) regs[tERR];
	sf.sf_handler = catcher;

	/* save scratch registers */
	sf.sf_sc.sc_eax = regs[tEAX];
	sf.sf_sc.sc_ebx = regs[tEBX];
	sf.sf_sc.sc_ecx = regs[tECX];
	sf.sf_sc.sc_edx = regs[tEDX];
	sf.sf_sc.sc_esi = regs[tESI];
	sf.sf_sc.sc_edi = regs[tEDI];
	sf.sf_sc.sc_cs = regs[tCS];
	sf.sf_sc.sc_ds = regs[tDS];
	sf.sf_sc.sc_ss = regs[tSS];
	sf.sf_sc.sc_es = regs[tES];
	sf.sf_sc.sc_isp = regs[tISP];

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	sf.sf_sc.sc_onstack = oonstack;
	sf.sf_sc.sc_mask = mask;
	sf.sf_sc.sc_sp = regs[tESP];
	sf.sf_sc.sc_fp = regs[tEBP];
	sf.sf_sc.sc_pc = regs[tEIP];
	sf.sf_sc.sc_ps = regs[tEFLAGS];

	/*
	 * Copy the sigframe out to the user's stack.
	 */
	if (copyout(&sf, fp, sizeof(struct sigframe)) != 0) {
		/*
		 * Something is wrong with the stack pointer.
		 * ...Kill the process.
		 */
		sigexit(p, SIGILL);
	};

	regs[tESP] = (int)fp;
	regs[tEIP] = (int)(((char *)PS_STRINGS) - *(p->p_sysent->sv_szsigcode));
	regs[tEFLAGS] &= ~PSL_VM;
	regs[tCS] = _ucodesel;
	regs[tDS] = _udatasel;
	regs[tES] = _udatasel;
	regs[tSS] = _udatasel;
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above).
 * Return to previous pc and psl as specified by
 * context left by sendsig. Check carefully to
 * make sure that the user has not modified the
 * state to gain improper privileges.
 */
int
sigreturn(p, uap, retval)
	struct proc *p;
	struct sigreturn_args /* {
		struct sigcontext *sigcntxp;
	} */ *uap;
	int *retval;
{
	register struct sigcontext *scp;
	register struct sigframe *fp;
	register int *regs = p->p_md.md_regs;
	int eflags;

	/*
	 * (XXX old comment) regs[tESP] points to the return address.
	 * The user scp pointer is above that.
	 * The return address is faked in the signal trampoline code
	 * for consistency.
	 */
	scp = uap->sigcntxp;
	fp = (struct sigframe *)
	     ((caddr_t)scp - offsetof(struct sigframe, sf_sc));

	if (useracc((caddr_t)fp, sizeof (*fp), 0) == 0)
		return(EINVAL);

	/*
	 * Don't allow users to change privileged or reserved flags.
	 */
#define	EFLAGS_SECURE(ef, oef)	((((ef) ^ (oef)) & ~PSL_USERCHANGE) == 0)
	eflags = scp->sc_ps;
	/*
	 * XXX do allow users to change the privileged flag PSL_RF.  The
	 * cpu sets PSL_RF in tf_eflags for faults.  Debuggers should
	 * sometimes set it there too.  tf_eflags is kept in the signal
	 * context during signal handling and there is no other place
	 * to remember it, so the PSL_RF bit may be corrupted by the
	 * signal handler without us knowing.  Corruption of the PSL_RF
	 * bit at worst causes one more or one less debugger trap, so
	 * allowing it is fairly harmless.
	 */
	if (!EFLAGS_SECURE(eflags & ~PSL_RF, regs[tEFLAGS] & ~PSL_RF)) {
#ifdef DEBUG
    		printf("sigreturn: eflags = 0x%x\n", eflags);
#endif
    		return(EINVAL);
	}

	/*
	 * Don't allow users to load a valid privileged %cs.  Let the
	 * hardware check for invalid selectors, excess privilege in
	 * other selectors, invalid %eip's and invalid %esp's.
	 */
#define	CS_SECURE(cs)	(ISPL(cs) == SEL_UPL)
	if (!CS_SECURE(scp->sc_cs)) {
#ifdef DEBUG
    		printf("sigreturn: cs = 0x%x\n", scp->sc_cs);
#endif
		trapsignal(p, SIGBUS, T_PROTFLT);
		return(EINVAL);
	}

	/* restore scratch registers */
	regs[tEAX] = scp->sc_eax;
	regs[tEBX] = scp->sc_ebx;
	regs[tECX] = scp->sc_ecx;
	regs[tEDX] = scp->sc_edx;
	regs[tESI] = scp->sc_esi;
	regs[tEDI] = scp->sc_edi;
	regs[tCS] = scp->sc_cs;
	regs[tDS] = scp->sc_ds;
	regs[tES] = scp->sc_es;
	regs[tSS] = scp->sc_ss;
	regs[tISP] = scp->sc_isp;

	if (useracc((caddr_t)scp, sizeof (*scp), 0) == 0)
		return(EINVAL);

	if (scp->sc_onstack & 01)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = scp->sc_mask &~
	    (sigmask(SIGKILL)|sigmask(SIGCONT)|sigmask(SIGSTOP));
	regs[tEBP] = scp->sc_fp;
	regs[tESP] = scp->sc_sp;
	regs[tEIP] = scp->sc_pc;
	regs[tEFLAGS] = eflags;
	return(EJUSTRETURN);
}

/*
 * Machine depdnetnt boot() routine
 *
 * I haven't seen anything too put here yet
 * Possibly some stuff might be grafted back here from boot()
 */
void
cpu_boot(int howto)
{
}

/*
 * Clear registers on exec
 */
void
setregs(p, entry, stack)
	struct proc *p;
	u_long entry;
	u_long stack;
{
	int *regs = p->p_md.md_regs;

#ifdef USER_LDT
	struct pcb *pcb = &p->p_addr->u_pcb;

	/* was i386_user_cleanup() in NetBSD */
	if (pcb->pcb_ldt) {
		if (pcb == curpcb)
			lldt(GSEL(GUSERLDT_SEL, SEL_KPL));
		kmem_free(kernel_map, (vm_offset_t)pcb->pcb_ldt,
			pcb->pcb_ldt_len * sizeof(union descriptor));
		pcb->pcb_ldt_len = (int)pcb->pcb_ldt = 0;
 	}
#endif
  
	bzero(regs, sizeof(struct trapframe));
	regs[tEIP] = entry;
	regs[tESP] = stack;
	regs[tEFLAGS] = PSL_USER | (regs[tEFLAGS] & PSL_T);
	regs[tSS] = _udatasel;
	regs[tDS] = _udatasel;
	regs[tES] = _udatasel;
	regs[tCS] = _ucodesel;

	p->p_addr->u_pcb.pcb_flags = 0;	/* no fp at all */
	load_cr0(rcr0() | CR0_TS);	/* start emulating */
#if	NNPX > 0
	npxinit(__INITIAL_NPXCW__);
#endif	/* NNPX > 0 */
}

static int
sysctl_machdep_adjkerntz SYSCTL_HANDLER_ARGS
{
	int error;
	error = sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2,
		req);
	if (!error && req->newptr)
		resettodr();
	return (error);
}

SYSCTL_PROC(_machdep, CPU_ADJKERNTZ, adjkerntz, CTLTYPE_INT|CTLFLAG_RW,
	&adjkerntz, 0, sysctl_machdep_adjkerntz, "I", "");

SYSCTL_INT(_machdep, CPU_DISRTCSET, disable_rtc_set,
	CTLFLAG_RW, &disable_rtc_set, 0, "");

SYSCTL_STRUCT(_machdep, CPU_BOOTINFO, bootinfo, 
	CTLFLAG_RD, &bootinfo, bootinfo, "");

SYSCTL_INT(_machdep, CPU_WALLCLOCK, wall_cmos_clock,
	CTLFLAG_RW, &wall_cmos_clock, 0, "");

/*
 * Initialize 386 and configure to run kernel
 */

/*
 * Initialize segments & interrupt table
 */

int currentldt;
int _default_ldt;
union descriptor gdt[NGDT];		/* global descriptor table */
struct gate_descriptor idt[NIDT];	/* interrupt descriptor table */
union descriptor ldt[NLDT];		/* local descriptor table */

static struct i386tss dblfault_tss;
static char dblfault_stack[PAGE_SIZE];

extern  struct user *proc0paddr;

/* software prototypes -- in more palatable form */
struct soft_segment_descriptor gdt_segs[] = {
/* GNULL_SEL	0 Null Descriptor */
{	0x0,			/* segment base address  */
	0x0,			/* length */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
/* GCODE_SEL	1 Code Descriptor for kernel */
{	0x0,			/* segment base address  */
	0xfffff,		/* length - all address space */
	SDT_MEMERA,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	1,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
/* GDATA_SEL	2 Data Descriptor for kernel */
{	0x0,			/* segment base address  */
	0xfffff,		/* length - all address space */
	SDT_MEMRWA,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	1,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
/* GLDT_SEL	3 LDT Descriptor */
{	(int) ldt,		/* segment base address  */
	sizeof(ldt)-1,		/* length - all address space */
	SDT_SYSLDT,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	0,			/* unused - default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
/* GTGATE_SEL	4 Null Descriptor - Placeholder */
{	0x0,			/* segment base address  */
	0x0,			/* length - all address space */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
/* GPANIC_SEL	5 Panic Tss Descriptor */
{	(int) &dblfault_tss,	/* segment base address  */
	sizeof(struct i386tss)-1,/* length - all address space */
	SDT_SYS386TSS,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	0,			/* unused - default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
/* GPROC0_SEL	6 Proc 0 Tss Descriptor */
{	(int) kstack,		/* segment base address  */
	sizeof(struct i386tss)-1,/* length - all address space */
	SDT_SYS386TSS,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	0,			/* unused - default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
/* GUSERLDT_SEL	7 User LDT Descriptor per process */
{	(int) ldt,		/* segment base address  */
	(512 * sizeof(union descriptor)-1),		/* length */
	SDT_SYSLDT,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	0,			/* unused - default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
/* GAPMCODE32_SEL 8 APM BIOS 32-bit interface (32bit Code) */
{	0,			/* segment base address (overwritten by APM)  */
	0xfffff,		/* length */
	SDT_MEMERA,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	1,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
/* GAPMCODE16_SEL 9 APM BIOS 32-bit interface (16bit Code) */
{	0,			/* segment base address (overwritten by APM)  */
	0xfffff,		/* length */
	SDT_MEMERA,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
/* GAPMDATA_SEL	10 APM BIOS 32-bit interface (Data) */
{	0,			/* segment base address (overwritten by APM) */
	0xfffff,		/* length */
	SDT_MEMRWA,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	1,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
};

static struct soft_segment_descriptor ldt_segs[] = {
	/* Null Descriptor - overwritten by call gate */
{	0x0,			/* segment base address  */
	0x0,			/* length - all address space */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Null Descriptor - overwritten by call gate */
{	0x0,			/* segment base address  */
	0x0,			/* length - all address space */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Null Descriptor - overwritten by call gate */
{	0x0,			/* segment base address  */
	0x0,			/* length - all address space */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Code Descriptor for user */
{	0x0,			/* segment base address  */
	0xfffff,		/* length - all address space */
	SDT_MEMERA,		/* segment type */
	SEL_UPL,		/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	1,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
	/* Data Descriptor for user */
{	0x0,			/* segment base address  */
	0xfffff,		/* length - all address space */
	SDT_MEMRWA,		/* segment type */
	SEL_UPL,		/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	1,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
};

void
setidt(idx, func, typ, dpl, selec)
	int idx;
	inthand_t *func;
	int typ;
	int dpl;
	int selec;
{
	struct gate_descriptor *ip = idt + idx;

	ip->gd_looffset = (int)func;
	ip->gd_selector = selec;
	ip->gd_stkcpy = 0;
	ip->gd_xx = 0;
	ip->gd_type = typ;
	ip->gd_dpl = dpl;
	ip->gd_p = 1;
	ip->gd_hioffset = ((int)func)>>16 ;
}

#define	IDTVEC(name)	__CONCAT(X,name)

extern inthand_t
	IDTVEC(div), IDTVEC(dbg), IDTVEC(nmi), IDTVEC(bpt), IDTVEC(ofl),
	IDTVEC(bnd), IDTVEC(ill), IDTVEC(dna), IDTVEC(fpusegm),
	IDTVEC(tss), IDTVEC(missing), IDTVEC(stk), IDTVEC(prot),
	IDTVEC(page), IDTVEC(mchk), IDTVEC(rsvd), IDTVEC(fpu), IDTVEC(align),
	IDTVEC(syscall), IDTVEC(int0x80_syscall);

void
sdtossd(sd, ssd)
	struct segment_descriptor *sd;
	struct soft_segment_descriptor *ssd;
{
	ssd->ssd_base  = (sd->sd_hibase << 24) | sd->sd_lobase;
	ssd->ssd_limit = (sd->sd_hilimit << 16) | sd->sd_lolimit;
	ssd->ssd_type  = sd->sd_type;
	ssd->ssd_dpl   = sd->sd_dpl;
	ssd->ssd_p     = sd->sd_p;
	ssd->ssd_def32 = sd->sd_def32;
	ssd->ssd_gran  = sd->sd_gran;
}

void
init386(first)
	int first;
{
	int x;
	unsigned biosbasemem, biosextmem;
	struct gate_descriptor *gdp;
	int gsel_tss;
	/* table descriptors - used to load tables by microp */
	struct region_descriptor r_gdt, r_idt;
	int	pagesinbase, pagesinext;
	int	target_page, pa_indx;

	proc0.p_addr = proc0paddr;

	atdevbase = ISA_HOLE_START + KERNBASE;

	/*
	 * Initialize the console before we print anything out.
	 */
	cninit();

#ifdef PC98
	/*
	 * Initialize DMAC
	 */
	init_pc98_dmac();
#endif

	/*
	 * make gdt memory segments, the code segment goes up to end of the
	 * page with etext in it, the data segment goes to the end of
	 * the address space
	 */
	/*
	 * XXX text protection is temporarily (?) disabled.  The limit was
	 * i386_btop(round_page(etext)) - 1.
	 */
	gdt_segs[GCODE_SEL].ssd_limit = i386_btop(0) - 1;
	gdt_segs[GDATA_SEL].ssd_limit = i386_btop(0) - 1;
	for (x = 0; x < NGDT; x++)
		ssdtosd(&gdt_segs[x], &gdt[x].sd);

	/* make ldt memory segments */
	/*
	 * The data segment limit must not cover the user area because we
	 * don't want the user area to be writable in copyout() etc. (page
	 * level protection is lost in kernel mode on 386's).  Also, we
	 * don't want the user area to be writable directly (page level
	 * protection of the user area is not available on 486's with
	 * CR0_WP set, because there is no user-read/kernel-write mode).
	 *
	 * XXX - VM_MAXUSER_ADDRESS is an end address, not a max.  And it
	 * should be spelled ...MAX_USER...
	 */
#define VM_END_USER_RW_ADDRESS	VM_MAXUSER_ADDRESS
	/*
	 * The code segment limit has to cover the user area until we move
	 * the signal trampoline out of the user area.  This is safe because
	 * the code segment cannot be written to directly.
	 */
#define VM_END_USER_R_ADDRESS	(VM_END_USER_RW_ADDRESS + UPAGES * PAGE_SIZE)
	ldt_segs[LUCODE_SEL].ssd_limit = i386_btop(VM_END_USER_R_ADDRESS) - 1;
	ldt_segs[LUDATA_SEL].ssd_limit = i386_btop(VM_END_USER_RW_ADDRESS) - 1;
	/* Note. eventually want private ldts per process */
	for (x = 0; x < NLDT; x++)
		ssdtosd(&ldt_segs[x], &ldt[x].sd);

	/* exceptions */
	for (x = 0; x < NIDT; x++)
		setidt(x, &IDTVEC(rsvd), SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(0, &IDTVEC(div),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(1, &IDTVEC(dbg),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(2, &IDTVEC(nmi),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
 	setidt(3, &IDTVEC(bpt),  SDT_SYS386TGT, SEL_UPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(4, &IDTVEC(ofl),  SDT_SYS386TGT, SEL_UPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(5, &IDTVEC(bnd),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(6, &IDTVEC(ill),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(7, &IDTVEC(dna),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(8, 0,  SDT_SYSTASKGT, SEL_KPL, GSEL(GPANIC_SEL, SEL_KPL));
	setidt(9, &IDTVEC(fpusegm),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(10, &IDTVEC(tss),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(11, &IDTVEC(missing),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(12, &IDTVEC(stk),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(13, &IDTVEC(prot),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
#ifdef CYRIX_486DLC
	setidt(14, &IDTVEC(page),  SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
#else
	setidt(14, &IDTVEC(page),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
#endif
	setidt(15, &IDTVEC(rsvd),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(16, &IDTVEC(fpu),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(17, &IDTVEC(align), SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(18, &IDTVEC(mchk),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
 	setidt(0x80, &IDTVEC(int0x80_syscall),
			SDT_SYS386TGT, SEL_UPL, GSEL(GCODE_SEL, SEL_KPL));

#ifdef PC98
#include      "nec.h"
#include      "epson.h"
#if   NNEC > 0 || NEPSON > 0
	isa_defaultirq();
#endif
#else /* IBM-PC */    
#include	"isa.h"
#if	NISA >0
	isa_defaultirq();
#endif
#endif
	rand_initialize();

	r_gdt.rd_limit = sizeof(gdt) - 1;
	r_gdt.rd_base =  (int) gdt;
	lgdt(&r_gdt);

	r_idt.rd_limit = sizeof(idt) - 1;
	r_idt.rd_base = (int) idt;
	lidt(&r_idt);

	_default_ldt = GSEL(GLDT_SEL, SEL_KPL);
	lldt(_default_ldt);
	currentldt = _default_ldt;

#ifdef DDB
	kdb_init();
	if (boothowto & RB_KDB)
		Debugger("Boot flags requested debugger");
#endif

#ifdef PC98
#ifdef EPSON_MEMWIN
	init_epson_memwin();
#endif
	biosbasemem = 640;                      /* 640KB */
	biosextmem = (Maxmem * PAGE_SIZE - 0x100000)/1024;   /* extent memory */
#else /* IBM-PC */
	/* Use BIOS values stored in RTC CMOS RAM, since probing
	 * breaks certain 386 AT relics.
	 */
	biosbasemem = rtcin(RTC_BASELO)+ (rtcin(RTC_BASEHI)<<8);
	biosextmem = rtcin(RTC_EXTLO)+ (rtcin(RTC_EXTHI)<<8);

	/*
	 * Print a warning and set it to the safest value if the official
	 * BIOS interface (bootblock supplied) disagrees with the
	 * hackish interface used above.  Eventually only the official
	 * interface should be used.  This is necessary for some machines
	 * who 'steal' memory from the basemem for use as BIOS memory.
	 */
	if (bootinfo.bi_memsizes_valid) {
		if (bootinfo.bi_basemem != biosbasemem) {
			vm_offset_t pa, va,  tmpva;
			vm_size_t size;
			unsigned *pte;

			printf("BIOS basemem (%ldK) != RTC basemem (%dK), ",
			       bootinfo.bi_basemem, biosbasemem);
			printf("setting to BIOS value.\n");
			biosbasemem = bootinfo.bi_basemem;
			/*
			 * XXX - Map this 'hole' of memory in the same manner
			 * as the ISA_HOLE (read/write/non-cacheable), since
			 * the BIOS 'fudges' it to become part of the ISA_HOLE.
			 * This code is similar to the code used in
			 * pmap_mapdev, but since no memory needs to be
			 * allocated we simply change the mapping.
			 */
			pa = biosbasemem * 1024;
			va = pa + KERNBASE;
			size = roundup(ISA_HOLE_START - pa, PAGE_SIZE);
			for (tmpva = va; size > 0;) {
				pte = (unsigned *)vtopte(tmpva);
				*pte = pa | PG_RW | PG_V | PG_N;
				size -= PAGE_SIZE;
				tmpva += PAGE_SIZE;
				pa += PAGE_SIZE;
			}
		}
		if (bootinfo.bi_extmem != biosextmem)
			printf("BIOS extmem (%ldK) != RTC extmem (%dK)\n",
			       bootinfo.bi_extmem, biosextmem);
	}
#endif

	/*
	 * If BIOS tells us that it has more than 640k in the basemem,
	 *	don't believe it - set it to 640k.
	 */
	if (biosbasemem > 640)
		biosbasemem = 640;

	/*
	 * Some 386 machines might give us a bogus number for extended
	 *	mem. If this happens, stop now.
	 */
#ifndef PC98
#ifndef LARGEMEM
	if (biosextmem > 65536) {
		panic("extended memory beyond limit of 64MB");
		/* NOTREACHED */
	}
#endif
#endif

	pagesinbase = biosbasemem * 1024 / PAGE_SIZE;
	pagesinext = biosextmem * 1024 / PAGE_SIZE;

	/*
	 * Special hack for chipsets that still remap the 384k hole when
	 *	there's 16MB of memory - this really confuses people that
	 *	are trying to use bus mastering ISA controllers with the
	 *	"16MB limit"; they only have 16MB, but the remapping puts
	 *	them beyond the limit.
	 */
#ifndef PC98
	/*
	 * If extended memory is between 15-16MB (16-17MB phys address range),
	 *	chop it to 15MB.
	 */
	if ((pagesinext > 3840) && (pagesinext < 4096))
		pagesinext = 3840;
#endif

	/*
	 * Maxmem isn't the "maximum memory", it's one larger than the
	 * highest page of the physical address space.  It should be
	 * called something like "Maxphyspage".
	 */
	Maxmem = pagesinext + 0x100000/PAGE_SIZE;

#ifdef MAXMEM
	Maxmem = MAXMEM/4;
#endif

	/* call pmap initialization to make new kernel address space */
	pmap_bootstrap (first, 0);

	/*
	 * Size up each available chunk of physical memory.
	 */

	/*
	 * We currently don't bother testing base memory.
	 * XXX  ...but we probably should.
	 */
	pa_indx = 0;
	badpages = 0;
	if (pagesinbase > 1) {
		phys_avail[pa_indx++] = PAGE_SIZE;	/* skip first page of memory */
		phys_avail[pa_indx] = ptoa(pagesinbase);/* memory up to the ISA hole */
		physmem = pagesinbase - 1;
	} else {
		/* point at first chunk end */
		pa_indx++;
	}

#ifdef PC98
#ifdef notyet
	init_cpu_accel_mem();
#endif
#endif

	for (target_page = avail_start; target_page < ptoa(Maxmem); target_page += PAGE_SIZE) {
		int tmp, page_bad = FALSE;

#ifdef PC98
		/* skip system area */
		if (target_page>=ptoa(Maxmem_under16M) &&
				target_page < ptoa(4096))
			page_bad = TRUE;
#endif
		/*
		 * map page into kernel: valid, read/write, non-cacheable
		 */
		*(int *)CMAP1 = PG_V | PG_RW | PG_N | target_page;
		pmap_update();

		tmp = *(int *)CADDR1;
		/*
		 * Test for alternating 1's and 0's
		 */
		*(volatile int *)CADDR1 = 0xaaaaaaaa;
		if (*(volatile int *)CADDR1 != 0xaaaaaaaa) {
			page_bad = TRUE;
		}
		/*
		 * Test for alternating 0's and 1's
		 */
		*(volatile int *)CADDR1 = 0x55555555;
		if (*(volatile int *)CADDR1 != 0x55555555) {
			page_bad = TRUE;
		}
		/*
		 * Test for all 1's
		 */
		*(volatile int *)CADDR1 = 0xffffffff;
		if (*(volatile int *)CADDR1 != 0xffffffff) {
			page_bad = TRUE;
		}
		/*
		 * Test for all 0's
		 */
		*(volatile int *)CADDR1 = 0x0;
		if (*(volatile int *)CADDR1 != 0x0) {
			/*
			 * test of page failed
			 */
			page_bad = TRUE;
		}
		/*
		 * Restore original value.
		 */
		*(int *)CADDR1 = tmp;

		/*
		 * Adjust array of valid/good pages.
		 */
		if (page_bad == FALSE) {
			/*
			 * If this good page is a continuation of the
			 * previous set of good pages, then just increase
			 * the end pointer. Otherwise start a new chunk.
			 * Note that "end" points one higher than end,
			 * making the range >= start and < end.
			 */
			if (phys_avail[pa_indx] == target_page) {
				phys_avail[pa_indx] += PAGE_SIZE;
			} else {
				pa_indx++;
				if (pa_indx == PHYS_AVAIL_ARRAY_END) {
					printf("Too many holes in the physical address space, giving up\n");
					pa_indx--;
					break;
				}
				phys_avail[pa_indx++] = target_page;	/* start */
				phys_avail[pa_indx] = target_page + PAGE_SIZE;	/* end */
			}
			physmem++;
		} else {
			badpages++;
			page_bad = FALSE;
		}
	}

	*(int *)CMAP1 = 0;
	pmap_update();

	/*
	 * XXX
	 * The last chunk must contain at least one page plus the message
	 * buffer to avoid complicating other code (message buffer address
	 * calculation, etc.).
	 */
	while (phys_avail[pa_indx - 1] + PAGE_SIZE +
	    round_page(sizeof(struct msgbuf)) >= phys_avail[pa_indx]) {
		physmem -= atop(phys_avail[pa_indx] - phys_avail[pa_indx - 1]);
		phys_avail[pa_indx--] = 0;
		phys_avail[pa_indx--] = 0;
	}

	Maxmem = atop(phys_avail[pa_indx]);

	/* Trim off space for the message buffer. */
	phys_avail[pa_indx] -= round_page(sizeof(struct msgbuf));

	avail_end = phys_avail[pa_indx];

	/* now running on new page tables, configured,and u/iom is accessible */

	/* make a initial tss so microp can get interrupt stack on syscall! */
	proc0.p_addr->u_pcb.pcb_tss.tss_esp0 = (int) kstack + UPAGES*PAGE_SIZE;
	proc0.p_addr->u_pcb.pcb_tss.tss_ss0 = GSEL(GDATA_SEL, SEL_KPL) ;
	gsel_tss = GSEL(GPROC0_SEL, SEL_KPL);

	dblfault_tss.tss_esp = dblfault_tss.tss_esp0 = dblfault_tss.tss_esp1 =
	    dblfault_tss.tss_esp2 = (int) &dblfault_stack[sizeof(dblfault_stack)];
	dblfault_tss.tss_ss = dblfault_tss.tss_ss0 = dblfault_tss.tss_ss1 =
	    dblfault_tss.tss_ss2 = GSEL(GDATA_SEL, SEL_KPL);
	dblfault_tss.tss_cr3 = IdlePTD;
	dblfault_tss.tss_eip = (int) dblfault_handler;
	dblfault_tss.tss_eflags = PSL_KERNEL;
	dblfault_tss.tss_ds = dblfault_tss.tss_es = dblfault_tss.tss_fs = dblfault_tss.tss_gs =
		GSEL(GDATA_SEL, SEL_KPL);
	dblfault_tss.tss_cs = GSEL(GCODE_SEL, SEL_KPL);
	dblfault_tss.tss_ldt = GSEL(GLDT_SEL, SEL_KPL);

	((struct i386tss *)gdt_segs[GPROC0_SEL].ssd_base)->tss_ioopt =
		(sizeof(struct i386tss))<<16;

	ltr(gsel_tss);

	/* make a call gate to reenter kernel with */
	gdp = &ldt[LSYS5CALLS_SEL].gd;

	x = (int) &IDTVEC(syscall);
	gdp->gd_looffset = x++;
	gdp->gd_selector = GSEL(GCODE_SEL,SEL_KPL);
	gdp->gd_stkcpy = 1;
	gdp->gd_type = SDT_SYS386CGT;
	gdp->gd_dpl = SEL_UPL;
	gdp->gd_p = 1;
	gdp->gd_hioffset = ((int) &IDTVEC(syscall)) >>16;

	/* transfer to user mode */

	_ucodesel = LSEL(LUCODE_SEL, SEL_UPL);
	_udatasel = LSEL(LUDATA_SEL, SEL_UPL);

	/* setup proc 0's pcb */
	proc0.p_addr->u_pcb.pcb_flags = 0;
	proc0.p_addr->u_pcb.pcb_cr3 = IdlePTD;
}

/*
 * The registers are in the frame; the frame is in the user area of
 * the process in question; when the process is active, the registers
 * are in "the kernel stack"; when it's not, they're still there, but
 * things get flipped around.  So, since p->p_md.md_regs is the whole address
 * of the register set, take its offset from the kernel stack, and
 * index into the user block.  Don't you just *love* virtual memory?
 * (I'm starting to think seymour is right...)
 */
#define	TF_REGP(p)	((struct trapframe *) \
			 ((char *)(p)->p_addr \
			  + ((char *)(p)->p_md.md_regs - kstack)))

int
ptrace_set_pc(p, addr)
	struct proc *p;
	unsigned int addr;
{
	TF_REGP(p)->tf_eip = addr;
	return (0);
}

int
ptrace_single_step(p)
	struct proc *p;
{
	TF_REGP(p)->tf_eflags |= PSL_T;
	return (0);
}

int ptrace_write_u(p, off, data)
	struct proc *p;
	vm_offset_t off;
	int data;
{
	struct trapframe frame_copy;
	vm_offset_t min;
	struct trapframe *tp;

	/*
	 * Privileged kernel state is scattered all over the user area.
	 * Only allow write access to parts of regs and to fpregs.
	 */
	min = (char *)p->p_md.md_regs - kstack;
	if (off >= min && off <= min + sizeof(struct trapframe) - sizeof(int)) {
		tp = TF_REGP(p);
		frame_copy = *tp;
		*(int *)((char *)&frame_copy + (off - min)) = data;
		if (!EFLAGS_SECURE(frame_copy.tf_eflags, tp->tf_eflags) ||
		    !CS_SECURE(frame_copy.tf_cs))
			return (EINVAL);
		*(int*)((char *)p->p_addr + off) = data;
		return (0);
	}
	min = offsetof(struct user, u_pcb) + offsetof(struct pcb, pcb_savefpu);
	if (off >= min && off <= min + sizeof(struct save87) - sizeof(int)) {
		*(int*)((char *)p->p_addr + off) = data;
		return (0);
	}
	return (EFAULT);
}

int
fill_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
	struct trapframe *tp;

	tp = TF_REGP(p);
	regs->r_es = tp->tf_es;
	regs->r_ds = tp->tf_ds;
	regs->r_edi = tp->tf_edi;
	regs->r_esi = tp->tf_esi;
	regs->r_ebp = tp->tf_ebp;
	regs->r_ebx = tp->tf_ebx;
	regs->r_edx = tp->tf_edx;
	regs->r_ecx = tp->tf_ecx;
	regs->r_eax = tp->tf_eax;
	regs->r_eip = tp->tf_eip;
	regs->r_cs = tp->tf_cs;
	regs->r_eflags = tp->tf_eflags;
	regs->r_esp = tp->tf_esp;
	regs->r_ss = tp->tf_ss;
	return (0);
}

int
set_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
	struct trapframe *tp;

	tp = TF_REGP(p);
	if (!EFLAGS_SECURE(regs->r_eflags, tp->tf_eflags) ||
	    !CS_SECURE(regs->r_cs))
		return (EINVAL);
	tp->tf_es = regs->r_es;
	tp->tf_ds = regs->r_ds;
	tp->tf_edi = regs->r_edi;
	tp->tf_esi = regs->r_esi;
	tp->tf_ebp = regs->r_ebp;
	tp->tf_ebx = regs->r_ebx;
	tp->tf_edx = regs->r_edx;
	tp->tf_ecx = regs->r_ecx;
	tp->tf_eax = regs->r_eax;
	tp->tf_eip = regs->r_eip;
	tp->tf_cs = regs->r_cs;
	tp->tf_eflags = regs->r_eflags;
	tp->tf_esp = regs->r_esp;
	tp->tf_ss = regs->r_ss;
	return (0);
}

#ifndef DDB
void
Debugger(const char *msg)
{
	printf("Debugger(\"%s\") called.\n", msg);
}
#endif /* no DDB */

#include <sys/disklabel.h>
#define b_cylin	b_resid
/*
 * Determine the size of the transfer, and make sure it is
 * within the boundaries of the partition. Adjust transfer
 * if needed, and signal errors or early completion.
 */
int
bounds_check_with_label(struct buf *bp, struct disklabel *lp, int wlabel)
{
        struct partition *p = lp->d_partitions + dkpart(bp->b_dev);
        int labelsect = lp->d_partitions[0].p_offset;
        int maxsz = p->p_size,
                sz = (bp->b_bcount + DEV_BSIZE - 1) >> DEV_BSHIFT;

        /* overwriting disk label ? */
        /* XXX should also protect bootstrap in first 8K */
        if (bp->b_blkno + p->p_offset <= LABELSECTOR + labelsect &&
#if LABELSECTOR != 0
            bp->b_blkno + p->p_offset + sz > LABELSECTOR + labelsect &&
#endif
            (bp->b_flags & B_READ) == 0 && wlabel == 0) {
                bp->b_error = EROFS;
                goto bad;
        }

#if     defined(DOSBBSECTOR) && defined(notyet)
        /* overwriting master boot record? */
        if (bp->b_blkno + p->p_offset <= DOSBBSECTOR &&
            (bp->b_flags & B_READ) == 0 && wlabel == 0) {
                bp->b_error = EROFS;
                goto bad;
        }
#endif

        /* beyond partition? */
        if (bp->b_blkno < 0 || bp->b_blkno + sz > maxsz) {
                /* if exactly at end of disk, return an EOF */
                if (bp->b_blkno == maxsz) {
                        bp->b_resid = bp->b_bcount;
                        return(0);
                }
                /* or truncate if part of it fits */
                sz = maxsz - bp->b_blkno;
                if (sz <= 0) {
                        bp->b_error = EINVAL;
                        goto bad;
                }
                bp->b_bcount = sz << DEV_BSHIFT;
        }

        /* calculate cylinder for disksort to order transfers with */
        bp->b_pblkno = bp->b_blkno + p->p_offset;
        bp->b_cylin = bp->b_pblkno / lp->d_secpercyl;
        return(1);

bad:
        bp->b_flags |= B_ERROR;
        return(-1);
}

int
disk_externalize(int drive, struct sysctl_req *req)
{
	return SYSCTL_OUT(req, &drive, sizeof drive);
}
