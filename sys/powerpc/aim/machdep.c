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

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include "opt_ddb.h"
#include "opt_compat.h"
#include "opt_msgbuf.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>
#include <sys/sysproto.h>
#include <sys/mutex.h>
#include <sys/ktr.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/lock.h>
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
#include <machine/reg.h>
#include <machine/fpu.h>
#include <machine/globaldata.h>
#include <machine/vmparam.h>
#include <machine/elf.h>
#include <machine/trap.h>
#include <machine/powerpc.h>
#include <dev/ofw/openfirm.h>
#include <ddb/ddb.h>
#include <sys/vnode.h>
#include <fs/procfs/procfs.h>
#include <machine/sigframe.h>

int cold = 1;

struct mtx	sched_lock;
struct mtx	Giant;

struct user	*proc0paddr;

char		machine[] = "powerpc";
SYSCTL_STRING(_hw, HW_MACHINE, machine, CTLFLAG_RD, machine, 0, "");

static char	model[128];
SYSCTL_STRING(_hw, HW_MODEL, model, CTLFLAG_RD, model, 0, "");

char		bootpath[256];

#ifdef DDB
/* start and end of kernel symbol table */
void		*ksym_start, *ksym_end;
#endif /* DDB */

static void	cpu_startup(void *);
SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL)

void		powerpc_init(u_int, u_int, u_int, char *);

int		save_ofw_mapping(void);
int		restore_ofw_mapping(void);

void		install_extint(void (*)(void));

void		osendsig(sig_t, int, sigset_t *, u_long);

struct msgbuf	*msgbufp = 0;

int		bootverbose = 0, Maxmem = 0;
long		dumplo;

vm_offset_t	phys_avail[10];

static int	chosen;

struct pmap	ofw_pmap;
extern int	ofmsr;

struct bat	battable[16];

static void	identifycpu(void);

static vm_offset_t	buffer_sva, buffer_eva;
vm_offset_t		clean_sva, clean_eva;
static vm_offset_t	pager_sva, pager_eva;

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
	unsigned int	i;
	caddr_t		v;
	vm_offset_t	maxaddr;
	vm_size_t	size;
	vm_offset_t	firstaddr;
	vm_offset_t	minaddr;

	size = 0;

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	identifycpu();

	/* startrtclock(); */
#ifdef PERFMON
	perfmon_init();
#endif
	printf("real memory  = %ld (%ldK bytes)\n", ptoa(Maxmem),
	    ptoa(Maxmem) / 1024);

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

	/*
	 * Calculate callout wheel size
	 */
	for (callwheelsize = 1, callwheelbits = 0;
	     callwheelsize < ncallout;
	     callwheelsize <<= 1, ++callwheelbits)
		;
	callwheelmask = callwheelsize - 1;

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
	valloc(callwheel, struct callout_tailq, callwheelsize);

	/*
	 * The nominal buffer size (and minimum KVA allocation) is BKVASIZE.
	 * For the first 64MB of ram nominally allocate sufficient buffers to
	 * cover 1/4 of our ram.  Beyond the first 64MB allocate additional
	 * buffers to cover 1/20 of our ram over 64MB.
	 */

	if (nbuf == 0) {
		int factor;

		factor = 4 * BKVASIZE / PAGE_SIZE;
		nbuf = 50;
		if (Maxmem > 1024)
			nbuf += min((Maxmem - 1024) / factor, 16384 / factor);
		if (Maxmem > 16384)
			nbuf += (Maxmem - 16384) * 2 / (factor * 5);
	}
	nswbuf = max(min(nbuf/4, 64), 16);

	valloc(swbuf, struct buf, nswbuf);
	valloc(buf, struct buf, nbuf);
	v = bufhashinit(v);

	/*
	 * End of first pass, size has been calculated so allocate memory
	 */
	if (firstaddr == 0) {
		size = (vm_size_t)(v - firstaddr);
		firstaddr = (vm_offset_t)kmem_alloc(kernel_map,
		    round_page(size));
		if (firstaddr == 0)
			panic("startup: no room for tables");
		goto again;
	}

	/*
	 * End of second pass, addresses have been assigned
	 */
	if ((vm_size_t)(v - firstaddr) != size)
		panic("startup: table size inconsistency");

	clean_map = kmem_suballoc(kernel_map, &clean_sva, &clean_eva,
	    (nbuf*BKVASIZE) + (nswbuf*MAXPHYS) + pager_map_size);
	buffer_map = kmem_suballoc(clean_map, &buffer_sva, &buffer_eva,
	    (nbuf*BKVASIZE));
	pager_map = kmem_suballoc(clean_map, &pager_sva, &pager_eva,
	    (nswbuf*MAXPHYS) + pager_map_size);
	pager_map->system_map = 1;
	exec_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
	    (16*(ARG_MAX+(PAGE_SIZE*3))));

	/*
	 * XXX: Mbuf system machine-specific initializations should
	 *      go here, if anywhere.
	 */

	/*
	 * Initialize callouts
	 */
	SLIST_INIT(&callfree);
	for (i = 0; i < ncallout; i++) {
		callout_init(&callout[i], 0);
		callout[i].c_flags = CALLOUT_LOCAL_ALLOC;
		SLIST_INSERT_HEAD(&callfree, &callout[i], c_links.sle);
	}

	for (i = 0; i < callwheelsize; i++) {
		TAILQ_INIT(&callwheel[i]);
	}

	mtx_init(&callout_lock, "callout", MTX_SPIN);

#if defined(USERCONFIG)
#if defined(USERCONFIG_BOOT)
	if (1)
#else
        if (boothowto & RB_CONFIG)
#endif
	{
		userconfig();
		cninit();	/* the preferred console may have changed */
	}
#endif

	printf("avail memory = %ld (%ldK bytes)\n", ptoa(cnt.v_free_count),
	    ptoa(cnt.v_free_count) / 1024);

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

void
identifycpu()
{
	int pvr, cpu;

	/*
	 * Find cpu type (Do it by OpenFirmware?)
	 */
	__asm ("mfpvr %0" : "=r"(pvr));
	cpu = pvr >> 16;
	switch (cpu) {
	case 1:
		sprintf(model, "601");
		break;
	case 3:
		sprintf(model, "603");
		break;
	case 4:
		sprintf(model, "604");
		break;
	case 5:
		sprintf(model, "602");
		break;
	case 6:
		sprintf(model, "603e");
		break;
	case 7:
		sprintf(model, "603ev");
		break;
	case 8:
		sprintf(model, "750 (G3)");
		break;
	case 9:
		sprintf(model, "604ev");
		break;
	case 12:
		sprintf(model, "7400 (G4)");
		break;
	case 20:
		sprintf(model, "620");
		break;
	default:
		sprintf(model, "Version %x", cpu);
		break;
	}
	sprintf(model + strlen(model), " (Revision %x)", pvr & 0xffff);
	printf("CPU: PowerPC %s\n", model);
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

#if 0 /* XXX: interrupt handler.  We'll get to this later */
extern void	ext_intr(void);
#endif

#ifdef DDB
extern		ddblow, ddbsize;
#endif
#ifdef IPKDB
extern		ipkdblow, ipkdbsize;
#endif

static struct globaldata	tmpglobal;

void
powerpc_init(u_int startkernel, u_int endkernel, u_int basekernel, char *args)
{
	int			exc, scratch;
	struct mem_region	*allmem, *availmem, *mp;
	struct globaldata	*globalp;

	/*
	 * Set up BAT0 to only map the lowest 256 MB area
	 */
	battable[0].batl = BATL(0x00000000, BAT_M, BAT_PP_RW);
	battable[0].batu = BATU(0x00000000, BAT_BL_256M, BAT_Vs);

	/*
	 * Map PCI memory space.
	 */
	battable[0x8].batl = BATL(0x80000000, BAT_I, BAT_PP_RW);
	battable[0x8].batu = BATU(0x80000000, BAT_BL_256M, BAT_Vs);

	battable[0x9].batl = BATL(0x90000000, BAT_I, BAT_PP_RW);
	battable[0x9].batu = BATU(0x90000000, BAT_BL_256M, BAT_Vs);

	battable[0xa].batl = BATL(0xa0000000, BAT_I, BAT_PP_RW);
	battable[0xa].batu = BATU(0xa0000000, BAT_BL_256M, BAT_Vs);

	/*
	 * Map obio devices.
	 */
	battable[0xf].batl = BATL(0xf0000000, BAT_I, BAT_PP_RW);
	battable[0xf].batu = BATU(0xf0000000, BAT_BL_256M, BAT_Vs);

	/*
	 * Now setup fixed bat registers
	 *
	 * Note that we still run in real mode, and the BAT
	 * registers were cleared above.
	 */
	/* BAT0 used for initial 256 MB segment */
	__asm __volatile ("mtibatl 0,%0; mtibatu 0,%1;"
		          "mtdbatl 0,%0; mtdbatu 0,%1;"
		          :: "r"(battable[0].batl), "r"(battable[0].batu));
	/*
	 * Set up battable to map all RAM regions.
	 * This is here because mem_regions() call needs bat0 set up.
	 */
	mem_regions(&allmem, &availmem);
	for (mp = allmem; mp->size; mp++) {
		vm_offset_t	pa = mp->start & 0xf0000000;
		vm_offset_t	end = mp->start + mp->size;

		do {
			u_int n = pa >> 28;

			battable[n].batl = BATL(pa, BAT_M, BAT_PP_RW);
			battable[n].batu = BATU(pa, BAT_BL_256M, BAT_Vs);
			pa += 0x10000000;
		} while (pa < end);
	}

	chosen = OF_finddevice("/chosen");
	save_ofw_mapping();

	proc0.p_addr = proc0paddr;
	bzero(proc0.p_addr, sizeof *proc0.p_addr);

	LIST_INIT(&proc0.p_contested);

/* XXX: NetBSDism I _think_.  Not sure yet. */
#if 0
	curpm = curpcb->pcb_pmreal = curpcb->pcb_pm = kernel_pmap;
#endif
	
	/*
	 * Initialise some mutexes.
	 */
	mtx_init(&Giant, "Giant", MTX_DEF | MTX_RECURSE);
	mtx_init(&sched_lock, "sched lock", MTX_SPIN | MTX_RECURSE);
	mtx_init(&proc0.p_mtx, "process lock", MTX_DEF);
	mtx_lock(&Giant);

	/*
	 * Initialise console.
	 */
	cninit();

#ifdef	__notyet__		/* Needs some rethinking regarding real/virtual OFW */
	OF_set_callback(callback);
#endif

	/*
	 * Set up trap vectors
	 */
	for (exc = EXC_RSVD; exc <= EXC_LAST; exc += 0x100) {
		switch (exc) {
		default:
			bcopy(&trapcode, (void *)exc, (size_t)&trapsize);
			break;
		case EXC_EXI:
			/*
			 * This one is (potentially) installed during autoconf
			 */
			break;
		case EXC_ALI:
			bcopy(&alitrap, (void *)EXC_ALI, (size_t)&alisize);
			break;
		case EXC_DSI:
			bcopy(&dsitrap, (void *)EXC_DSI, (size_t)&dsisize);
			break;
		case EXC_ISI:
			bcopy(&isitrap, (void *)EXC_ISI, (size_t)&isisize);
			break;
		case EXC_DECR:
			bcopy(&decrint, (void *)EXC_DECR, (size_t)&decrsize);
			break;
		case EXC_IMISS:
			bcopy(&tlbimiss, (void *)EXC_IMISS, (size_t)&tlbimsize);
			break;
		case EXC_DLMISS:
			bcopy(&tlbdlmiss, (void *)EXC_DLMISS, (size_t)&tlbdlmsize);
			break;
		case EXC_DSMISS:
			bcopy(&tlbdsmiss, (void *)EXC_DSMISS, (size_t)&tlbdsmsize);
			break;
#if defined(DDB) || defined(IPKDB)
		case EXC_TRC:
		case EXC_PGM:
		case EXC_BPT:
#if defined(DDB)
			bcopy(&ddblow, (void *)exc, (size_t)&ddbsize);
#else
			bcopy(&ipkdblow, (void *)exc, (size_t)&ipkdbsize);
#endif
			break;
#endif /* DDB || IPKDB */
		}
	}

#if 0 /* XXX: coming soon... */
	/*
	 * external interrupt handler install
	 */
	install_extint(ext_intr);

	__syncicache((void *)EXC_RST, EXC_LAST - EXC_RST + 0x100);
#endif

	/*
	 * Now enable translation (and machine checks/recoverable interrupts).
	 */
	__asm __volatile ("mfmsr %0; ori %0,%0,%1; mtmsr %0; isync"
		          : "=r"(scratch) : "K"(PSL_IR|PSL_DR|PSL_ME|PSL_RI));


	ofmsr &= ~PSL_IP;

	/*
	 * Parse arg string.
	 */
#ifdef DDB
	bcopy(args + strlen(args) + 1, &startsym, sizeof(startsym));
	bcopy(args + strlen(args) + 5, &endsym, sizeof(endsym));
	if (startsym == NULL || endsym == NULL)
		startsym = endsym = NULL;
#endif

	strcpy(bootpath, args);
	args = bootpath;
	while (*++args && *args != ' ');
	if (*args) {
		*args++ = 0;
		while (*args) {
			switch (*args++) {
			case 'a':
				boothowto |= RB_ASKNAME;
				break;
			case 's':
				boothowto |= RB_SINGLE;
				break;
			case 'd':
				boothowto |= RB_KDB;
				break;
			case 'v':
				boothowto |= RB_VERBOSE;
				break;
			}
		}
	}

#ifdef DDB
	ddb_init((int)((u_int)endsym - (u_int)startsym), startsym, endsym);
#endif
#ifdef IPKDB
	/*
	 * Now trap to IPKDB
	 */
	ipkdb_init();
	if (boothowto & RB_KDB)
		ipkdb_connect(0);
#endif

	/*
	 * Set the page size.
	 */
#if 0
	vm_set_page_size();
#endif

	/*
	 * Initialize pmap module.
	 */
	pmap_bootstrap(startkernel, endkernel);

	restore_ofw_mapping();

	/*
	 * Setup the global data for the bootstrap cpu.
	 */
	globalp = (struct globaldata *) &tmpglobal;

	/*
	 * XXX: Pass 0 as CPU id.  This is bad.  We need to work out
	 * XXX: which CPU we are somehow.
	 */
	globaldata_init(globalp, 0, sizeof(struct globaldata));
	__asm("mtsprg 0,%0\n" :: "r" (globalp));

	PCPU_GET(next_asn) = 1;	/* 0 used for proc0 pmap */
	PCPU_SET(curproc, &proc0);
	PCPU_SET(spinlocks, NULL);
}

static int N_mapping;
static struct {
	vm_offset_t	va;
	int		len;
	vm_offset_t	pa;
	int		mode;
} ofw_mapping[256];

int
save_ofw_mapping()
{
	int	mmui, mmu;

	OF_getprop(chosen, "mmu", &mmui, 4);
	mmu = OF_instance_to_package(mmui);

	bzero(ofw_mapping, sizeof(ofw_mapping));

	N_mapping =
	    OF_getprop(mmu, "translations", ofw_mapping, sizeof(ofw_mapping));
	N_mapping /= sizeof(ofw_mapping[0]);

	return 0;
}

int
restore_ofw_mapping()
{
	int		i;
	struct vm_page	pg;

	pmap_pinit(&ofw_pmap);

	ofw_pmap.pm_sr[KERNEL_SR] = KERNEL_SEGMENT;

	for (i = 0; i < N_mapping; i++) {
		vm_offset_t	pa = ofw_mapping[i].pa;
		vm_offset_t	va = ofw_mapping[i].va;
		int		size = ofw_mapping[i].len;

		if (va < 0x90000000)			/* XXX */
			continue;

		while (size > 0) {
			pg.phys_addr = pa;
			pmap_enter(&ofw_pmap, va, &pg, VM_PROT_ALL,
			    VM_PROT_ALL);
			pa += PAGE_SIZE;
			va += PAGE_SIZE;
			size -= PAGE_SIZE;
		}
	}

	return 0;
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

#if 0
void
delay(unsigned n)
{
	u_long tb;

	do {
		__asm __volatile("mftb %0" : "=r" (tb));
	} while (n > (int)(tb & 0xffffffff));
}
#endif

void
osendsig(sig_t catcher, int sig, sigset_t *mask, u_long code)
{

	/* XXX: To be done */
	return;
}

void
sendsig(sig_t catcher, int sig, sigset_t *mask, u_long code)
{

	/* XXX: To be done */
	return;
}

int
osigreturn(struct proc *p, struct osigreturn_args *uap)
{

	/* XXX: To be done */
	return(ENOSYS);
}

int
sigreturn(struct proc *p, struct sigreturn_args *uap)
{

	/* XXX: To be done */
	return(ENOSYS);
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
setregs(struct proc *p, u_long entry, u_long stack, u_long ps_strings)
{
	struct trapframe	*tf;
	struct ps_strings	arginfo;

	tf = trapframe(p);

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
	tf->fixreg[3] = arginfo.ps_nargvstr;
	tf->fixreg[4] = (register_t)arginfo.ps_argvstr;
	tf->fixreg[5] = (register_t)arginfo.ps_envstr;
	tf->fixreg[6] = 0;			/* auxillary vector */
	tf->fixreg[7] = 0;			/* termination vector */
	tf->fixreg[8] = (register_t)PS_STRINGS;	/* NetBSD extension */

	tf->srr0 = entry;
	tf->srr1 = PSL_MBO | PSL_USERSET | PSL_FE_DFLT;
	p->p_addr->u_pcb.pcb_flags = 0;
}

extern void	*extint, *extsize;
extern u_long	extint_call;

#if 0
void
install_extint(void (*handler)(void))
{
	u_long	offset;
	int	omsr, msr;

	offset = (u_long)handler - (u_long)&extint_call;

#ifdef	DIAGNOSTIC
	if (offset > 0x1ffffff)
		panic("install_extint: too far away");
#endif
	__asm __volatile ("mfmsr %0; andi. %1,%0,%2; mtmsr %1"
		          : "=r"(omsr), "=r"(msr) : "K"((u_short)~PSL_EE));
	extint_call = (extint_call & 0xfc000003) | offset;
	bcopy(&extint, (void *)EXC_EXI, (size_t)&extsize);
	__syncicache((void *)&extint_call, sizeof extint_call);
	__syncicache((void *)EXC_EXI, (int)&extsize);
	__asm __volatile ("mtmsr %0" :: "r"(omsr));
}
#endif

#if !defined(DDB)
void
Debugger(const char *msg)
{

	printf("Debugger(\"%s\") called.\n", msg);
}
#endif /* !defined(DDB) */

/* XXX: dummy {fill,set}_[fp]regs */
int
fill_regs(struct proc *p, struct reg *regs)
{

	return (ENOSYS);
}

int
fill_fpregs(struct proc *p, struct fpreg *fpregs)
{

	return (ENOSYS);
}

int
set_regs(struct proc *p, struct reg *regs)
{

	return (ENOSYS);
}

int
set_fpregs(struct proc *p, struct fpreg *fpregs)
{

	return (ENOSYS);
}

int
ptrace_set_pc(struct proc *p, unsigned long addr)
{

	/* XXX: coming soon... */
	return (ENOSYS);
}

int
ptrace_single_step(struct proc *p)
{

	/* XXX: coming soon... */
	return (ENOSYS);
}

int
ptrace_write_u(struct proc *p, vm_offset_t off, long data)
{

	/* XXX: coming soon... */
	return (ENOSYS);
}

int
ptrace_read_u_check(struct proc *p, vm_offset_t addr, size_t len)
{

	/* XXX: coming soon... */
	return (ENOSYS);
}

int
ptrace_clear_single_step(struct proc *p)
{

	/* XXX: coming soon... */
	return (ENOSYS);
}

/*
 * Initialise a struct globaldata.
 */
void
globaldata_init(struct globaldata *globaldata, int cpuid, size_t sz)
{ 

        bzero(globaldata, sz);
        globaldata->gd_cpuid = cpuid;
        globaldata->gd_next_asn = 0;
        globaldata->gd_current_asngen = 1;
}
