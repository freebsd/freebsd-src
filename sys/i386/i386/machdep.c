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
 *	$Id: machdep.c,v 1.39 1994/03/19 23:58:58 wollman Exp $
 */

#include "npx.h"
#include "isa.h"

#include <stddef.h>
#include "param.h"
#include "systm.h"
#include "signalvar.h"
#include "kernel.h"
#include "map.h"
#include "proc.h"
#include "user.h"
#include "exec.h"            /* for PS_STRINGS */
#include "buf.h"
#include "reboot.h"
#include "conf.h"
#include "file.h"
#include "callout.h"
#include "malloc.h"
#include "mbuf.h"
#include "msgbuf.h"
#include "net/netisr.h"

#ifdef SYSVSHM
#include "sys/shm.h"
#endif

#ifdef SYSVMSG
#include "msg.h"
#endif

#ifdef SYSVSEM
#include "sem.h"
#endif

#include "vm/vm.h"
#include "vm/vm_kern.h"
#include "vm/vm_page.h"

#include "sys/exec.h"
#include "sys/vnode.h"

extern vm_offset_t avail_start, avail_end;

#include "machine/cpu.h"
#include "machine/reg.h"
#include "machine/psl.h"
#include "machine/specialreg.h"
#include "machine/sysarch.h"
#include "machine/cons.h"

#include "i386/isa/isa.h"
#include "i386/isa/rtc.h"

static void identifycpu(void);
static void initcpu(void);
static int test_page(int *, int);

extern int grow(struct proc *,int);
const char machine[] = "PC-Class";
const char *cpu_model;

#ifndef PANIC_REBOOT_WAIT_TIME
#define PANIC_REBOOT_WAIT_TIME 15 /* default to 15 seconds */
#endif

/*
 * Declare these as initialized data so we can patch them.
 */
int	nswbuf = 0;
#ifdef	NBUF
int	nbuf = NBUF;
#else
int	nbuf = 0;
#endif
#ifdef	BUFPAGES
int	bufpages = BUFPAGES;
#else
int	bufpages = 0;
#endif
#ifdef BOUNCEPAGES
int	bouncepages = BOUNCEPAGES;
#else
int	bouncepages = 0;
#endif
extern int freebufspace;
extern char *bouncememory;

int _udatasel, _ucodesel;

/*
 * Machine-dependent startup code
 */
int boothowto = 0, Maxmem = 0, maxmem = 0, badpages = 0, physmem = 0;
long dumplo;
extern int bootdev;
#ifdef SMALL
extern int forcemaxmem;
#endif
int biosmem;

vm_offset_t	phys_avail[6];

extern cyloffset;

int cpu_class;

void dumpsys __P((void));

void
cpu_startup()
{
	register int unixsize;
	register unsigned i;
	register struct pte *pte;
	int mapaddr, j;
	register caddr_t v;
	int maxbufs, base, residual;
	extern long Usrptsize;
	vm_offset_t minaddr, maxaddr;
	vm_size_t size = 0;
	int firstaddr;

	/*
	 * Initialize error message buffer (at end of core).
	 */

	/* avail_end was pre-decremented in init_386() to compensate */
	for (i = 0; i < btoc(sizeof (struct msgbuf)); i++)
		pmap_enter(pmap_kernel(), (vm_offset_t)msgbufp,
			   avail_end + i * NBPG,
			   VM_PROT_ALL, TRUE);
	msgbufmapped = 1;

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	identifycpu();
	printf("real memory  = %d (%d pages)\n", ptoa(physmem), physmem);
	if (badpages)
		printf("bad memory   = %d (%d pages)\n", ptoa(badpages), badpages);

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
	/*
	 * Determine how many buffers to allocate.
	 * Use 20% of memory of memory beyond the first 2MB
	 * Insure a minimum of 16 fs buffers.
	 * We allocate 1/2 as many swap buffer headers as file i/o buffers.
	 */
	if (bufpages == 0)
		bufpages = ((physmem << PGSHIFT) - 2048*1024) / NBPG / 5;
	if (bufpages < 64)
		bufpages = 64;

	/*
	 * We must still limit the maximum number of buffers to be no
	 * more than 2/5's of the size of the kernal malloc region, this
	 * will only take effect for machines with lots of memory
	 */
	bufpages = min(bufpages, (VM_KMEM_SIZE / NBPG) * 2 / 5);
	if (nbuf == 0) {
		nbuf = bufpages / 2;
		if (nbuf < 32)
			nbuf = 32;
	}
	freebufspace = bufpages * NBPG;
	if (nswbuf == 0) {
		nswbuf = (nbuf / 2) &~ 1;	/* force even */
		if (nswbuf > 256)
			nswbuf = 256;		/* sanity */
	}
	valloc(swbuf, struct buf, nswbuf);
	valloc(buf, struct buf, nbuf);

#ifndef NOBOUNCE
	/*
	 * If there is more than 16MB of memory, allocate some bounce buffers
	 */
	if (Maxmem > 4096) {
		if (bouncepages == 0)
			bouncepages = 96;	/* largest physio size + extra */
		v = (caddr_t)((vm_offset_t)((vm_offset_t)v + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
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

	/*
	 * Allocate a submap for buffer space allocations.
	 * XXX we are NOT using buffer_map, but due to
	 * the references to it we will just allocate 1 page of
	 * vm (not real memory) to make things happy...
	 */
	buffer_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				/* bufpages * */NBPG, TRUE);
	/*
	 * Allocate a submap for physio
	 */
	phys_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				 VM_PHYS_SIZE, TRUE);

	/*
	 * Finally, allocate mbuf pool.  Since mclrefcnt is an off-size
	 * we use the more space efficient malloc in place of kmem_alloc.
	 */
	mclrefcnt = (char *)malloc(NMBCLUSTERS+CLBYTES/MCLBYTES,
				   M_MBUF, M_NOWAIT);
	bzero(mclrefcnt, NMBCLUSTERS+CLBYTES/MCLBYTES);
	mb_map = kmem_suballoc(kmem_map, (vm_offset_t)&mbutl, &maxaddr,
			       VM_MBUF_SIZE, FALSE);
	/*
	 * Initialize callouts
	 */
	callfree = callout;
	for (i = 1; i < ncallout; i++)
		callout[i-1].c_next = &callout[i];

	printf("avail memory = %d (%d pages)\n", ptoa(vm_page_free_count), vm_page_free_count);
	printf("using %d buffers containing %d bytes of memory\n",
		nbuf, bufpages * CLBYTES);

#ifndef NOBOUNCE
	/*
	 * init bounce buffers
	 */
	vm_bounce_init();
#endif

	/*
	 * Set up CPU-specific registers, cache, etc.
	 */
	initcpu();

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();

	/*
	 * Configure the system.
	 */
	configure();
}


struct cpu_nameclass i386_cpus[] = {
	{ "Intel 80286",	CPUCLASS_286 },		/* CPU_286   */
	{ "i386SX",		CPUCLASS_386 },		/* CPU_386SX */
	{ "i386DX",		CPUCLASS_386 },		/* CPU_386   */
	{ "i486SX",		CPUCLASS_486 },		/* CPU_486SX */
	{ "i486DX",		CPUCLASS_486 },		/* CPU_486   */
	{ "i586",		CPUCLASS_586 },		/* CPU_586   */
};

static void
identifycpu()
{
	printf("CPU: ");
	if (cpu >= 0 && cpu < (sizeof i386_cpus/sizeof(struct cpu_nameclass))) {
		printf("%s", i386_cpus[cpu].cpu_name);
		cpu_class = i386_cpus[cpu].cpu_class;
		cpu_model = i386_cpus[cpu].cpu_name;
	} else {
		printf("unknown cpu type %d\n", cpu);
		panic("startup: bad cpu id");
	}
	printf(" (");
	switch(cpu_class) {
	case CPUCLASS_286:
		printf("286");
		break;
	case CPUCLASS_386:
		printf("386");
		break;
	case CPUCLASS_486:
		printf("486");
		break;
	case CPUCLASS_586:
		printf("586");
		break;
	default:
		printf("unknown");	/* will panic below... */
	}
	printf("-class CPU)");
	printf("\n");	/* cpu speed would be nice, but how? */

	/*
	 * Now that we have told the user what they have,
	 * let them know if that machine type isn't configured.
	 */
	switch (cpu_class) {
	case CPUCLASS_286:	/* a 286 should not make it this far, anyway */
#if !defined(I386_CPU) && !defined(I486_CPU) && !defined(I586_CPU)
#error This kernel is not configured for one of the supported CPUs
#endif
#if !defined(I386_CPU)
	case CPUCLASS_386:
#endif
#if !defined(I486_CPU)
	case CPUCLASS_486:
#endif
#if !defined(I586_CPU)
	case CPUCLASS_586:
#endif
		panic("CPU class not configured");
	default:
		break;
	}
}

#ifdef PGINPROF
/*
 * Return the difference (in microseconds)
 * between the  current time and a previous
 * time as represented  by the arguments.
 * If there is a pending clock interrupt
 * which has not been serviced due to high
 * ipl, return error code.
 */
/*ARGSUSED*/
vmtime(otime, olbolt, oicr)
	register int otime, olbolt, oicr;
{

	return (((time.tv_sec-otime)*60 + lbolt-olbolt)*16667);
}
#endif

extern int kstack[];

/*
 * Send an interrupt to process.
 *
 * Stack is set up to allow sigcode stored
 * in u. to call routine, followed by kcall
 * to sigreturn routine below.  After sigreturn
 * resets the signal mask, the stack, and the
 * frame pointer, it returns to the user
 * specified pc, psl.
 */
void
sendsig(catcher, sig, mask, code)
	sig_t catcher;
	int sig, mask;
	unsigned code;
{
	register struct proc *p = curproc;
	register int *regs;
	register struct sigframe *fp;
	struct sigacts *ps = p->p_sigacts;
	int oonstack, frmtrap;

	regs = p->p_regs;
        oonstack = ps->ps_onstack;
	/*
	 * Allocate and validate space for the signal handler
	 * context. Note that if the stack is in P0 space, the
	 * call to grow() is a nop, and the useracc() check
	 * will fail if the process has not already allocated
	 * the space with a `brk'.
	 */
        if (!ps->ps_onstack && (ps->ps_sigonstack & sigmask(sig))) {
		fp = (struct sigframe *)(ps->ps_sigsp
				- sizeof(struct sigframe));
                ps->ps_onstack = 1;
	} else {
		fp = (struct sigframe *)(regs[tESP]
			- sizeof(struct sigframe));
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
	fp->sf_signum = sig;
	fp->sf_code = code;
	fp->sf_scp = &fp->sf_sc;
	fp->sf_addr = (char *) regs[tERR];
	fp->sf_handler = catcher;

	/* save scratch registers */
	fp->sf_sc.sc_eax = regs[tEAX];
	fp->sf_sc.sc_ebx = regs[tEBX];
	fp->sf_sc.sc_ecx = regs[tECX];
	fp->sf_sc.sc_edx = regs[tEDX];
	fp->sf_sc.sc_esi = regs[tESI];
	fp->sf_sc.sc_edi = regs[tEDI];
	fp->sf_sc.sc_cs = regs[tCS];
	fp->sf_sc.sc_ds = regs[tDS];
	fp->sf_sc.sc_ss = regs[tSS];
	fp->sf_sc.sc_es = regs[tES];
	fp->sf_sc.sc_isp = regs[tISP];

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	fp->sf_sc.sc_onstack = oonstack;
	fp->sf_sc.sc_mask = mask;
	fp->sf_sc.sc_sp = regs[tESP];
	fp->sf_sc.sc_fp = regs[tEBP];
	fp->sf_sc.sc_pc = regs[tEIP];
	fp->sf_sc.sc_ps = regs[tEFLAGS];
	regs[tESP] = (int)fp;
	regs[tEIP] = (int)((struct pcb *)kstack)->pcb_sigc;
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
 * psl to gain improper priviledges or to cause
 * a machine fault.
 */
struct sigreturn_args {
	struct sigcontext *sigcntxp;
};

int
sigreturn(p, uap, retval)
	struct proc *p;
	struct sigreturn_args *uap;
	int *retval;
{
	register struct sigcontext *scp;
	register struct sigframe *fp;
	register int *regs = p->p_regs;
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

	eflags = scp->sc_ps;
	if ((eflags & PSL_USERCLR) != 0 ||
	    (eflags & PSL_USERSET) != PSL_USERSET ||
	    (eflags & PSL_IOPL) < (regs[tEFLAGS] & PSL_IOPL)) {
#ifdef DEBUG
    		printf("sigreturn:  eflags=0x%x\n", eflags);
#endif
    		return(EINVAL);
	}

	/*
	 * Sanity check the user's selectors and error if they
	 * are suspect.
	 */
#define max_ldt_sel(pcb) \
	((pcb)->pcb_ldt ? (pcb)->pcb_ldt_len : (sizeof(ldt) / sizeof(ldt[0])))

#define valid_ldt_sel(sel) \
	(ISLDT(sel) && ISPL(sel) == SEL_UPL && \
	 IDXSEL(sel) < max_ldt_sel(&p->p_addr->u_pcb))

#define null_sel(sel) \
	(!ISLDT(sel) && IDXSEL(sel) == 0)

	if ((scp->sc_cs&0xffff != _ucodesel && !valid_ldt_sel(scp->sc_cs)) ||
	    (scp->sc_ss&0xffff != _udatasel && !valid_ldt_sel(scp->sc_ss)) ||
	    (scp->sc_ds&0xffff != _udatasel && !valid_ldt_sel(scp->sc_ds) &&
	     !null_sel(scp->sc_ds)) ||
	    (scp->sc_es&0xffff != _udatasel && !valid_ldt_sel(scp->sc_es) &&
	     !null_sel(scp->sc_es))) {
#ifdef DEBUG
    		printf("sigreturn:  cs=0x%x ss=0x%x ds=0x%x es=0x%x\n",
			scp->sc_cs, scp->sc_ss, scp->sc_ds, scp->sc_es);
#endif
		trapsignal(p, SIGBUS, T_PROTFLT);
		return(EINVAL);
	}

#undef max_ldt_sel
#undef valid_ldt_sel
#undef null_sel

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

	p->p_sigacts->ps_onstack = scp->sc_onstack & 01;
	p->p_sigmask = scp->sc_mask &~
	    (sigmask(SIGKILL)|sigmask(SIGCONT)|sigmask(SIGSTOP));
	regs[tEBP] = scp->sc_fp;
	regs[tESP] = scp->sc_sp;
	regs[tEIP] = scp->sc_pc;
	regs[tEFLAGS] = eflags;
	return(EJUSTRETURN);
}

/*
 * a simple function to make the system panic (and dump a vmcore)
 * in a predictable fashion
 */
void diediedie()
{
	panic("because you said to!");
}

int	waittime = -1;
struct pcb dumppcb;

void
boot(arghowto)
	int arghowto;
{
	register long dummy;		/* r12 is reserved */
	register int howto;		/* r11 == how to boot */
	register int devtype;		/* r10 == major of root dev */
	extern int cold;
	int nomsg = 1;

	if (cold) {
		printf("hit reset please");
		for(;;);
	}
	howto = arghowto;
	if ((howto&RB_NOSYNC) == 0 && waittime < 0 && bfreelist[0].b_forw) {
		register struct buf *bp;
		int iter, nbusy;

		waittime = 0;
		(void) splnet();
		printf("syncing disks... ");
		/*
		 * Release inodes held by texts before update.
		 */
		if (panicstr == 0)
			vnode_pager_umount(NULL);
		sync(curproc, NULL, NULL);
		/*
		 * Unmount filesystems
		 */
#if 0
		if (panicstr == 0)
			vfs_unmountall();
#endif

		for (iter = 0; iter < 20; iter++) {
			nbusy = 0;
			for (bp = &buf[nbuf]; --bp >= buf; )
				if ((bp->b_flags & (B_BUSY|B_INVAL)) == B_BUSY)
					nbusy++;
			if (nbusy == 0)
				break;
			if (nomsg) {
				printf("updating disks before rebooting... ");
				nomsg = 0;
			}
			printf("%d ", nbusy);
			DELAY(40000 * iter);
		}
		if (nbusy)
			printf("giving up\n");
		else
			printf("done\n");
		DELAY(10000);			/* wait for printf to finish */
	}
	splhigh();
	devtype = major(rootdev);
	if (howto&RB_HALT) {
		printf("\n");
		printf("The operating system has halted.\n");
		printf("Please press any key to reboot.\n\n");
		cngetc();
	} else {
		if (howto & RB_DUMP) {
			savectx(&dumppcb, 0);
			dumppcb.pcb_ptd = rcr3();
			dumpsys();	

			if (PANIC_REBOOT_WAIT_TIME != 0) {
				if (PANIC_REBOOT_WAIT_TIME != -1) {
					int loop;
					printf("Automatic reboot in %d seconds - press a key on the console to abort\n",
						PANIC_REBOOT_WAIT_TIME);
					for (loop = PANIC_REBOOT_WAIT_TIME; loop > 0; --loop) {
						DELAY(1000 * 1000); /* one second */
						if (sgetc(1)) /* Did user type a key? */
							break;
					}
					if (!loop)
						goto die;
				}
			} else { /* zero time specified - reboot NOW */
				goto die;
			}
			printf("--> Press a key on the console to reboot <--\n");
			cngetc();
		}
	}
#ifdef lint
	dummy = 0; dummy = dummy;
	printf("howto %d, devtype %d\n", arghowto, devtype);
#endif
die:
	printf("Rebooting...\n");
	DELAY (100000);	/* wait 100ms for printf's to complete */
	cpu_reset();
	for(;;) ;
	/* NOTREACHED */
}

unsigned long	dumpmag = 0x8fca0101UL;	/* magic number for savecore */
int		dumpsize = 0;		/* also for savecore */
/*
 * Doadump comes here after turning off memory management and
 * getting on the dump stack, either when called above, or by
 * the auto-restart code.
 */
void
dumpsys()
{

	if (dumpdev == NODEV)
		return;
	if ((minor(dumpdev)&07) != 1)
		return;
	dumpsize = Maxmem;
	printf("\ndumping to dev %x, offset %d\n", dumpdev, dumplo);
	printf("dump ");
	switch ((*bdevsw[major(dumpdev)].d_dump)(dumpdev)) {

	case ENXIO:
		printf("device bad\n");
		break;

	case EFAULT:
		printf("device not ready\n");
		break;

	case EINVAL:
		printf("area improper\n");
		break;

	case EIO:
		printf("i/o error\n");
		break;

	case EINTR:
		printf("aborted from console\n");
		break;

	default:
		printf("succeeded\n");
		break;
	}
}

#ifdef HZ
/*
 * If HZ is defined we use this code, otherwise the code in
 * /sys/i386/i386/microtime.s is used.  The othercode only works
 * for HZ=100.
 */
microtime(tvp)
	register struct timeval *tvp;
{
	int s = splhigh();

	*tvp = time;
	tvp->tv_usec += tick;
	while (tvp->tv_usec > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	splx(s);
}
#endif /* HZ */

void
physstrat(bp, strat, prio)
	struct buf *bp;
	int (*strat)(), prio;
{
	register int s;
	caddr_t baddr;

	vmapbuf(bp);
	(*strat)(bp);
	/* pageout daemon doesn't wait for pushed pages */
	if (bp->b_flags & B_DIRTY)
		return;
	s = splbio();
	while ((bp->b_flags & B_DONE) == 0)
	  tsleep((caddr_t)bp, prio, "physstr", 0);
	splx(s);
	vunmapbuf(bp);
}

static void
initcpu()
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
	p->p_regs[tEBP] = 0;	/* bottom of the fp chain */
	p->p_regs[tEIP] = entry;
	p->p_regs[tESP] = stack;
	p->p_regs[tSS] = _udatasel;
	p->p_regs[tDS] = _udatasel;
	p->p_regs[tES] = _udatasel;
	p->p_regs[tCS] = _ucodesel;

	p->p_addr->u_pcb.pcb_flags = 0;	/* no fp at all */
	load_cr0(rcr0() | CR0_TS);	/* start emulating */
#if	NNPX > 0
	npxinit(__INITIAL_NPXCW__);
#endif	/* NNPX > 0 */
}

/*
 * Initialize 386 and configure to run kernel
 */

/*
 * Initialize segments & interrupt table
 */

union descriptor gdt[NGDT];
union descriptor ldt[NLDT];		/* local descriptor table */
struct gate_descriptor idt[NIDT];	/* interrupt descriptor table */

int _default_ldt, currentldt;

struct	i386tss	tss, panic_tss;

extern  struct user *proc0paddr;

/* software prototypes -- in more palatable form */
struct soft_segment_descriptor gdt_segs[] = {
	/* Null Descriptor */
{	0x0,			/* segment base address  */
	0x0,			/* length */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Code Descriptor for kernel */
{	0x0,			/* segment base address  */
	0xfffff,		/* length - all address space */
	SDT_MEMERA,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	1,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
	/* Data Descriptor for kernel */
{	0x0,			/* segment base address  */
	0xfffff,		/* length - all address space */
	SDT_MEMRWA,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	1,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
	/* LDT Descriptor */
{	(int) ldt,			/* segment base address  */
	sizeof(ldt)-1,		/* length - all address space */
	SDT_SYSLDT,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	0,			/* unused - default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Null Descriptor - Placeholder */
{	0x0,			/* segment base address  */
	0x0,			/* length - all address space */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Panic Tss Descriptor */
{	(int) &panic_tss,		/* segment base address  */
	sizeof(tss)-1,		/* length - all address space */
	SDT_SYS386TSS,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	0,			/* unused - default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Proc 0 Tss Descriptor */
{	(int) kstack,			/* segment base address  */
	sizeof(tss)-1,		/* length - all address space */
	SDT_SYS386TSS,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	0,			/* unused - default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* User LDT Descriptor per process */
{	(int) ldt,			/* segment base address  */
	(512 * sizeof(union descriptor)-1),		/* length */
	SDT_SYSLDT,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	0,			/* unused - default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
};

struct soft_segment_descriptor ldt_segs[] = {
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
	1  			/* limit granularity (byte/page units)*/ } };

void
setidt(idx, func, typ, dpl)
	int idx;
	void (*func)();
	int typ;
	int dpl;
{
	struct gate_descriptor *ip = idt + idx;

	ip->gd_looffset = (int)func;
	ip->gd_selector = 8;
	ip->gd_stkcpy = 0;
	ip->gd_xx = 0;
	ip->gd_type = typ;
	ip->gd_dpl = dpl;
	ip->gd_p = 1;
	ip->gd_hioffset = ((int)func)>>16 ;
}

#define	IDTVEC(name)	__CONCAT(X, name)
typedef void idtvec_t();

extern idtvec_t
	IDTVEC(div), IDTVEC(dbg), IDTVEC(nmi), IDTVEC(bpt), IDTVEC(ofl),
	IDTVEC(bnd), IDTVEC(ill), IDTVEC(dna), IDTVEC(dble), IDTVEC(fpusegm),
	IDTVEC(tss), IDTVEC(missing), IDTVEC(stk), IDTVEC(prot),
	IDTVEC(page), IDTVEC(rsvd), IDTVEC(fpu), IDTVEC(rsvd0),
	IDTVEC(rsvd1), IDTVEC(rsvd2), IDTVEC(rsvd3), IDTVEC(rsvd4),
	IDTVEC(rsvd5), IDTVEC(rsvd6), IDTVEC(rsvd7), IDTVEC(rsvd8),
	IDTVEC(rsvd9), IDTVEC(rsvd10), IDTVEC(rsvd11), IDTVEC(rsvd12),
	IDTVEC(rsvd13), IDTVEC(rsvd14), IDTVEC(rsvd14), IDTVEC(syscall);

int _gsel_tss;

void
init386(first)
	int first;
{
	extern ssdtosd(), lgdt(), lidt(), lldt(), etext; 
	int x, *pi;
	unsigned biosbasemem, biosextmem;
	struct gate_descriptor *gdp;
	extern int sigcode,szsigcode;
	/* table descriptors - used to load tables by microp */
	struct region_descriptor r_gdt, r_idt;
	int	pagesinbase, pagesinext;
	int	target_page;

	proc0.p_addr = proc0paddr;

	/*
	 * Initialize the console before we print anything out.
	 */

	cninit ();

	/*
	 * make gdt memory segments, the code segment goes up to end of the
	 * page with etext in it, the data segment goes to the end of
	 * the address space
	 */
	gdt_segs[GCODE_SEL].ssd_limit = i386_btop(i386_round_page(&etext)) - 1;
	gdt_segs[GDATA_SEL].ssd_limit = 0xffffffffUL;	/* XXX constant? */
	for (x=0; x < NGDT; x++) ssdtosd(gdt_segs+x, gdt+x);
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
#define VM_END_USER_R_ADDRESS	(VM_END_USER_RW_ADDRESS + UPAGES * NBPG)
	ldt_segs[LUCODE_SEL].ssd_limit = i386_btop(VM_END_USER_R_ADDRESS) - 1;
	ldt_segs[LUDATA_SEL].ssd_limit = i386_btop(VM_END_USER_RW_ADDRESS) - 1;
	/* Note. eventually want private ldts per process */
	for (x=0; x < 5; x++) ssdtosd(ldt_segs+x, ldt+x);

	/* exceptions */
	setidt(0, &IDTVEC(div),  SDT_SYS386TGT, SEL_KPL);
	setidt(1, &IDTVEC(dbg),  SDT_SYS386TGT, SEL_KPL);
	setidt(2, &IDTVEC(nmi),  SDT_SYS386TGT, SEL_KPL);
 	setidt(3, &IDTVEC(bpt),  SDT_SYS386TGT, SEL_UPL);
	setidt(4, &IDTVEC(ofl),  SDT_SYS386TGT, SEL_UPL);
	setidt(5, &IDTVEC(bnd),  SDT_SYS386TGT, SEL_KPL);
	setidt(6, &IDTVEC(ill),  SDT_SYS386TGT, SEL_KPL);
	setidt(7, &IDTVEC(dna),  SDT_SYS386TGT, SEL_KPL);
	setidt(8, &IDTVEC(dble),  SDT_SYS386TGT, SEL_KPL);
	setidt(9, &IDTVEC(fpusegm),  SDT_SYS386TGT, SEL_KPL);
	setidt(10, &IDTVEC(tss),  SDT_SYS386TGT, SEL_KPL);
	setidt(11, &IDTVEC(missing),  SDT_SYS386TGT, SEL_KPL);
	setidt(12, &IDTVEC(stk),  SDT_SYS386TGT, SEL_KPL);
	setidt(13, &IDTVEC(prot),  SDT_SYS386TGT, SEL_KPL);
	setidt(14, &IDTVEC(page),  SDT_SYS386TGT, SEL_KPL);
	setidt(15, &IDTVEC(rsvd),  SDT_SYS386TGT, SEL_KPL);
	setidt(16, &IDTVEC(fpu),  SDT_SYS386TGT, SEL_KPL);
	setidt(17, &IDTVEC(rsvd0),  SDT_SYS386TGT, SEL_KPL);
	setidt(18, &IDTVEC(rsvd1),  SDT_SYS386TGT, SEL_KPL);
	setidt(19, &IDTVEC(rsvd2),  SDT_SYS386TGT, SEL_KPL);
	setidt(20, &IDTVEC(rsvd3),  SDT_SYS386TGT, SEL_KPL);
	setidt(21, &IDTVEC(rsvd4),  SDT_SYS386TGT, SEL_KPL);
	setidt(22, &IDTVEC(rsvd5),  SDT_SYS386TGT, SEL_KPL);
	setidt(23, &IDTVEC(rsvd6),  SDT_SYS386TGT, SEL_KPL);
	setidt(24, &IDTVEC(rsvd7),  SDT_SYS386TGT, SEL_KPL);
	setidt(25, &IDTVEC(rsvd8),  SDT_SYS386TGT, SEL_KPL);
	setidt(26, &IDTVEC(rsvd9),  SDT_SYS386TGT, SEL_KPL);
	setidt(27, &IDTVEC(rsvd10),  SDT_SYS386TGT, SEL_KPL);
	setidt(28, &IDTVEC(rsvd11),  SDT_SYS386TGT, SEL_KPL);
	setidt(29, &IDTVEC(rsvd12),  SDT_SYS386TGT, SEL_KPL);
	setidt(30, &IDTVEC(rsvd13),  SDT_SYS386TGT, SEL_KPL);
	setidt(31, &IDTVEC(rsvd14),  SDT_SYS386TGT, SEL_KPL);

#include	"isa.h"
#if	NISA >0
	isa_defaultirq();
#endif

	r_gdt.rd_limit = sizeof(gdt) - 1;
	r_gdt.rd_base =  (int) gdt;
	lgdt(&r_gdt);
	r_idt.rd_limit = sizeof(idt) - 1;
	r_idt.rd_base = (int) idt;
	lidt(&r_idt);
	_default_ldt = GSEL(GLDT_SEL, SEL_KPL);
	lldt(_default_ldt);
	currentldt = _default_ldt;

#include "ddb.h"
#if NDDB > 0
	kdb_init();
	if (boothowto & RB_KDB)
		Debugger("Boot flags requested debugger");
#endif

	/* Use BIOS values stored in RTC CMOS RAM, since probing
	 * breaks certain 386 AT relics.
	 */
	biosbasemem = rtcin(RTC_BASELO)+ (rtcin(RTC_BASEHI)<<8);
	biosextmem = rtcin(RTC_EXTLO)+ (rtcin(RTC_EXTHI)<<8);

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
#ifndef LARGEMEM
	if (biosextmem > 65536) {
		panic("extended memory beyond limit of 64MB");
		/* NOTREACHED */
	}
#endif

	pagesinbase = biosbasemem * 1024 / NBPG;
	pagesinext = biosextmem * 1024 / NBPG;

	/*
	 * Special hack for chipsets that still remap the 384k hole when
	 *	there's 16MB of memory - this really confuses people that
	 *	are trying to use bus mastering ISA controllers with the
	 *	"16MB limit"; they only have 16MB, but the remapping puts
	 *	them beyond the limit.
	 * XXX - this should be removed when bounce buffers are
	 *	implemented.
	 */
	/*
	 * If extended memory is between 15-16MB (16-17MB phys address range),
	 *	chop it to 15MB.
	 */
	if ((pagesinext > 3840) && (pagesinext < 4096))
		pagesinext = 3840;

	/*
	 * Maxmem isn't the "maximum memory", it's the highest page of
	 * of the physical address space. It should be "Maxphyspage".
	 */
	Maxmem = pagesinext + 0x100000/PAGE_SIZE;

#ifdef MAXMEM
	if (MAXMEM/4 < Maxmem)
		Maxmem = MAXMEM/4;
#endif
	/*
	 * Calculate number of physical pages, but account for Maxmem
	 *	limitation above.
	 */
	physmem = pagesinbase +
	    (min(pagesinext + 0x100000/PAGE_SIZE, Maxmem) - 0x100000/PAGE_SIZE);

	/* call pmap initialization to make new kernel address space */
	pmap_bootstrap (first, 0);

	/*
	 * Do simple memory test over range of extended memory that BIOS
	 *	indicates exists. Adjust Maxmem to the highest page of
	 *	good memory.
	 */
	printf("Testing memory (%dMB)...", ptoa(Maxmem)/1024/1024);

	for (target_page = Maxmem - 1; target_page >= atop(first); target_page--) {
		extern struct pte *CMAP1;
		extern caddr_t CADDR1;

		/*
		 * map page into kernel: valid, read/write, non-cacheable
		 */
		*(int *)CMAP1 = PG_V | PG_KW | PG_N | ptoa(target_page);
		tlbflush();

		/*
		 * Test for alternating 1's and 0's
		 */
		filli(0xaaaaaaaa, CADDR1, PAGE_SIZE/sizeof(int));
		if (test_page((int *)CADDR1, 0xaaaaaaaa)) {
			Maxmem = target_page;
			badpages++;
			continue;
		}
		/*
		 * Test for alternating 0's and 1's
		 */
		filli(0x55555555, CADDR1, PAGE_SIZE/sizeof(int));
		if (test_page((int *)CADDR1, 0x55555555)) {
			Maxmem = target_page;
			badpages++;
			continue;
		}
		/*
		 * Test for all 1's
		 */
		filli(0xffffffff, CADDR1, PAGE_SIZE/sizeof(int));
		if (test_page((int *)CADDR1, 0xffffffff)) {
			Maxmem = target_page;
			badpages++;
			continue;
		}
		/*
		 * Test zeroing of page
		 */
		bzero(CADDR1, PAGE_SIZE);
		if (test_page((int *)CADDR1, 0)) {
			/*
			 * test of page failed
			 */
			Maxmem = target_page;
			badpages++;
			continue;
		}
	}
	printf("done.\n");
		
	maxmem = Maxmem - 1;	/* highest page of usable memory */
	avail_end = (maxmem << PAGE_SHIFT) - i386_round_page(sizeof(struct msgbuf));

	/*
	 * Initialize pointers to the two chunks of memory; for use
	 *	later in vm_page_startup.
	 */
	/* avail_start is initialized in pmap_bootstrap */
	x = 0;
	if (pagesinbase > 1) {
		phys_avail[x++] = NBPG;		/* skip first page of memory */
		phys_avail[x++] = pagesinbase * NBPG;	/* memory up to the ISA hole */
	}
	phys_avail[x++] = avail_start;	/* memory up to the end */
	phys_avail[x++] = avail_end;
	phys_avail[x++] = 0;		/* no more chunks */
	phys_avail[x++] = 0;

	/* now running on new page tables, configured,and u/iom is accessible */

	/* make a initial tss so microp can get interrupt stack on syscall! */
	proc0.p_addr->u_pcb.pcb_tss.tss_esp0 = (int) kstack + UPAGES*NBPG;
	proc0.p_addr->u_pcb.pcb_tss.tss_ss0 = GSEL(GDATA_SEL, SEL_KPL) ;
	_gsel_tss = GSEL(GPROC0_SEL, SEL_KPL);

	((struct i386tss *)gdt_segs[GPROC0_SEL].ssd_base)->tss_ioopt = 
		(sizeof(tss))<<16;

	ltr(_gsel_tss);

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
	bcopy(&sigcode, proc0.p_addr->u_pcb.pcb_sigc, szsigcode);
	proc0.p_addr->u_pcb.pcb_flags = 0;
	proc0.p_addr->u_pcb.pcb_ptd = IdlePTD;
}

int
test_page(address, pattern)
	int *address;
	int pattern;
{
	int *x;

	for (x = address; x < (int *)((char *)address + PAGE_SIZE); x++) {
		if (*x != pattern)
			return (1);
	}
	return(0);
}

/*aston() {
	schednetisr(NETISR_AST);
}*/

void
setsoftclock() {
	schednetisr(NETISR_SCLK);
}

/*
 * insert an element into a queue 
 */
#undef insque
void				/* XXX replace with inline FIXME! */
_insque(element, head)
	register struct prochd *element, *head;
{
	element->ph_link = head->ph_link;
	head->ph_link = (struct proc *)element;
	element->ph_rlink = (struct proc *)head;
	((struct prochd *)(element->ph_link))->ph_rlink=(struct proc *)element;
}

/*
 * remove an element from a queue
 */
#undef remque
void				/* XXX replace with inline FIXME! */
_remque(element)
	register struct prochd *element;
{
	((struct prochd *)(element->ph_link))->ph_rlink = element->ph_rlink;
	((struct prochd *)(element->ph_rlink))->ph_link = element->ph_link;
	element->ph_rlink = (struct proc *)0;
}

/*
 * The registers are in the frame; the frame is in the user area of
 * the process in question; when the process is active, the registers
 * are in "the kernel stack"; when it's not, they're still there, but
 * things get flipped around.  So, since p->p_regs is the whole address
 * of the register set, take its offset from the kernel stack, and
 * index into the user block.  Don't you just *love* virtual memory?
 * (I'm starting to think seymour is right...)
 */

int
ptrace_set_pc (struct proc *p, unsigned int addr) {
	void *regs = (char*)p->p_addr +
		((char*) p->p_regs - (char*) kstack);

	((struct trapframe *)regs)->tf_eip = addr;
	return 0;
}

int
ptrace_single_step (struct proc *p) {
	void *regs = (char*)p->p_addr +
		((char*) p->p_regs - (char*) kstack);

	((struct trapframe *)regs)->tf_eflags |= PSL_T;
	return 0;
}

/*
 * Copy the registers to user-space.
 */

int
ptrace_getregs (struct proc *p, unsigned int *addr) {
	int error;
	struct regs regs = {0};

	if (error = fill_regs (p, &regs))
		return error;
	  
	return copyout (&regs, addr, sizeof (regs));
}

int
ptrace_setregs (struct proc *p, unsigned int *addr) {
	int error;
	struct regs regs = {0};

	if (error = copyin (addr, &regs, sizeof(regs)))
		return error;

	return set_regs (p, &regs);
}

int
fill_regs(struct proc *p, struct regs *regs) {
	int error;
	struct trapframe *tp;
	void *ptr = (char*)p->p_addr +
		((char*) p->p_regs - (char*) kstack);

	tp = ptr;
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
	return 0;
}

int
set_regs (struct proc *p, struct regs *regs) {
	int error;
	struct trapframe *tp;
	void *ptr = (char*)p->p_addr +
		((char*) p->p_regs - (char*) kstack);

	tp = ptr;
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
	return 0;
}

#include "ddb.h"
#if NDDB <= 0
void
Debugger(const char *msg)
{
	printf("Debugger(\"%s\") called.", msg);
}
#endif /* no DDB */
