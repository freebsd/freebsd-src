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
#include <machine/vmparam.h>
#include <machine/elf.h>
#include <machine/trap.h>
#include <machine/powerpc.h>
#include <dev/ofw/openfirm.h>
#include <ddb/ddb.h>
#include <sys/vnode.h>
#include <machine/sigframe.h>

int physmem = 0;
int cold = 1;

struct mtx	sched_lock;
struct mtx	Giant;

struct user	*proc0uarea;
vm_offset_t	proc0kstack;

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

#ifdef COMPAT_43
void		osendsig(sig_t, int, sigset_t *, u_long);
#endif

static int
sysctl_hw_physmem(SYSCTL_HANDLER_ARGS)
{
	int error = sysctl_handle_int(oidp, 0, ctob(physmem), req);
	return (error);
}

SYSCTL_PROC(_hw, HW_PHYSMEM, physmem, CTLTYPE_INT|CTLFLAG_RD,
	0, 0, sysctl_hw_physmem, "IU", "");

struct msgbuf	*msgbufp = 0;

int		Maxmem = 0;
long		dumplo;

vm_offset_t	phys_avail[10];

static int	chosen;

static struct trapframe		proc0_tf;

struct pmap	ofw_pmap;
extern int	ofmsr;

struct bat	battable[16];

static void	identifycpu(void);

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

	vm_ksubmap_init(&kmi);

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
	unsigned int pvr, version, revision;

	/*
	 * Find cpu type (Do it by OpenFirmware?)
	 */
	__asm ("mfpvr %0" : "=r"(pvr));
	version = pvr >> 16;
	revision = pvr & 0xffff;
	switch (version) {
	case 0x0001:
		sprintf(model, "601");
		break;
	case 0x0003:
		sprintf(model, "603 (Wart)");
		break;
	case 0x0004:
		sprintf(model, "604 (Zephyr)");
		break;
	case 0x0005:
		sprintf(model, "602 (Galahad)");
		break;
	case 0x0006:
		sprintf(model, "603e (Stretch)");
		break;
	case 0x0007:
		if ((revision && 0xf000) == 0x0000)
			sprintf(model, "603ev (Valiant)");
		else
			sprintf(model, "603r (Goldeneye)");
		break;
	case 0x0008:
		if ((revision && 0xf000) == 0x0000)
			sprintf(model, "G3 / 750 (Arthur)");
		else
			sprintf(model, "G3 / 755 (Goldfinger)");
		break;
	case 0x0009:
		if ((revision && 0xf000) == 0x0000)
			sprintf(model, "604e (Sirocco)");
		else
			sprintf(model, "604r (Mach V)");
		break;
	case 0x000a:
		sprintf(model, "604r (Mach V)");
		break;
	case 0x000c:
		sprintf(model, "G4 / 7400 (Max)");
		break;
	case 0x0014:
		sprintf(model, "620 (Red October)");
		break;
	case 0x0081:
		sprintf(model, "8240 (Kahlua)");
		break;
	case 0x8000:
		sprintf(model, "G4 / 7450 (V'ger)");
		break;
	case 0x800c:
		sprintf(model, "G4 / 7410 (Nitro)");
		break;
	case 0x8081:
		sprintf(model, "8245 (Kahlua II)");
		break;
	default:
		sprintf(model, "Version %x", version);
		break;
	}
	sprintf(model + strlen(model), " (Revision %x)", revision);
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

void
powerpc_init(u_int startkernel, u_int endkernel, u_int basekernel, char *args)
{
	unsigned int		exc, scratch;
	struct mem_region	*allmem, *availmem, *mp;
	struct pcpu	*pcpup;

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

	/* Calculate the physical memory in the machine */
	for (mp = allmem; mp->size; mp++)
		physmem += btoc(mp->size);

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

	pmap_setavailmem(startkernel, endkernel);

	proc_linkup(&proc0);

	proc0uarea = (struct user *)pmap_steal_memory(UAREA_PAGES * PAGE_SIZE);
	proc0kstack = pmap_steal_memory(KSTACK_PAGES * PAGE_SIZE);
	proc0.p_uarea = proc0uarea;
	thread0 = &proc0.p_thread;
	thread0->td_kstack = proc0kstack;
	thread0->td_pcb = (struct pcb *)
	    (thread0->td_kstack + KSTACK_PAGES * PAGE_SIZE) - 1;

	pcpup = pmap_steal_memory(round_page(sizeof(struct pcpu)));

	/*
	 * XXX: Pass 0 as CPU id.  This is bad.  We need to work out
	 * XXX: which CPU we are somehow.
	 */
	pcpu_init(pcpup, 0, sizeof(struct pcpu));
	__asm ("mtsprg 0, %0" :: "r"(pcpup));

	/* Init basic tunables, hz etc */
	init_param1();
	init_param2(physmem);

	/* setup curproc so the mutexes work */

	PCPU_SET(curthread, thread0);

	LIST_INIT(&thread0->td_contested);

/* XXX: NetBSDism I _think_.  Not sure yet. */
#if 0
	curpm = PCPU_GET(curpcb)->pcb_pmreal = PCPU_GET(curpcb)->pcb_pm = kernel_pmap;
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
		case EXC_DECR:
			bcopy(&decrint, (void *)EXC_DECR, (size_t)&decrsize);
			break;
#if 0 /* XXX: Not enabling these traps yet. */
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
#endif
		}
	}

#if 0 /* XXX: coming soon... */
	/*
	 * external interrupt handler install
	 */
	install_extint(ext_intr);
#endif

	__syncicache((void *)EXC_RST, EXC_LAST - EXC_RST + 0x100);

	/*
	 * Now enable translation (and machine checks/recoverable interrupts).
	 */
	__asm ("mfmsr %0" : "=r"(scratch));
	scratch |= PSL_IR | PSL_DR | PSL_ME | PSL_RI;
	__asm ("mtmsr %0" :: "r"(scratch));

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
	pmap_bootstrap();

	restore_ofw_mapping();

	PCPU_GET(next_asn) = 1;	/* 0 used for proc0 pmap */

	/* setup proc 0's pcb */
	thread0->td_pcb->pcb_flags = 0; /* XXXKSE */
	thread0->td_frame = &proc0_tf;
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

		if (va < 0x80000000)			/* XXX */
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

#ifdef COMPAT_43
void
osendsig(sig_t catcher, int sig, sigset_t *mask, u_long code)
{

	/* XXX: To be done */
	return;
}
#endif

void
sendsig(sig_t catcher, int sig, sigset_t *mask, u_long code)
{

	/* XXX: To be done */
	return;
}

#ifdef COMPAT_43
int
osigreturn(struct thread *td, struct osigreturn_args *uap)
{

	/* XXX: To be done */
	return(ENOSYS);
}
#endif

int
sigreturn(struct thread *td, struct sigreturn_args *uap)
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
setregs(struct thread *td, u_long entry, u_long stack, u_long ps_strings)
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

extern void	*extint, *extsize;
extern u_long	extint_call;

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

	msr = mfmsr();
	mtmsr(msr & ~PSL_EE);

	extint_call = (extint_call & 0xfc000003) | offset;
	bcopy(&extint, (void *)EXC_EXI, (size_t)&extsize);
	__syncicache((void *)&extint_call, sizeof extint_call);
	__syncicache((void *)EXC_EXI, (int)&extsize);

	mtmsr(msr);
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

int
ptrace_clear_single_step(struct thread *td)
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

void
enable_fpu(pcb)
	struct pcb *pcb;
{
	int msr, scratch;

	if (!(pcb->pcb_flags & PCB_FPU)) {
		bzero(&pcb->pcb_fpu, sizeof pcb->pcb_fpu);
		pcb->pcb_flags |= PCB_FPU;
	}
	__asm volatile ("mfmsr %0; ori %1,%0,%2; mtmsr %1; isync"
		      : "=r"(msr), "=r"(scratch) : "K"(PSL_FP));
	__asm volatile ("lfd 0,0(%0); mtfsf 0xff,0" :: "b"(&pcb->pcb_fpu.fpscr));
	__asm ("lfd 0,0(%0);"
	     "lfd 1,8(%0);"
	     "lfd 2,16(%0);"
	     "lfd 3,24(%0);"
	     "lfd 4,32(%0);"
	     "lfd 5,40(%0);"
	     "lfd 6,48(%0);"
	     "lfd 7,56(%0);"
	     "lfd 8,64(%0);"
	     "lfd 9,72(%0);"
	     "lfd 10,80(%0);"
	     "lfd 11,88(%0);"
	     "lfd 12,96(%0);"
	     "lfd 13,104(%0);"
	     "lfd 14,112(%0);"
	     "lfd 15,120(%0);"
	     "lfd 16,128(%0);"
	     "lfd 17,136(%0);"
	     "lfd 18,144(%0);"
	     "lfd 19,152(%0);"
	     "lfd 20,160(%0);"
	     "lfd 21,168(%0);"
	     "lfd 22,176(%0);"
	     "lfd 23,184(%0);"
	     "lfd 24,192(%0);"
	     "lfd 25,200(%0);"
	     "lfd 26,208(%0);"
	     "lfd 27,216(%0);"
	     "lfd 28,224(%0);"
	     "lfd 29,232(%0);"
	     "lfd 30,240(%0);"
	     "lfd 31,248(%0)" :: "b"(&pcb->pcb_fpu.fpr[0]));
	__asm volatile ("mtmsr %0; isync" :: "r"(msr));
}

void
save_fpu(pcb)
	struct pcb *pcb;
{
	int msr, scratch;
	
	__asm volatile ("mfmsr %0; ori %1,%0,%2; mtmsr %1; isync"
		      : "=r"(msr), "=r"(scratch) : "K"(PSL_FP));
	__asm ("stfd 0,0(%0);"
	     "stfd 1,8(%0);"
	     "stfd 2,16(%0);"
	     "stfd 3,24(%0);"
	     "stfd 4,32(%0);"
	     "stfd 5,40(%0);"
	     "stfd 6,48(%0);"
	     "stfd 7,56(%0);"
	     "stfd 8,64(%0);"
	     "stfd 9,72(%0);"
	     "stfd 10,80(%0);"
	     "stfd 11,88(%0);"
	     "stfd 12,96(%0);"
	     "stfd 13,104(%0);"
	     "stfd 14,112(%0);"
	     "stfd 15,120(%0);"
	     "stfd 16,128(%0);"
	     "stfd 17,136(%0);"
	     "stfd 18,144(%0);"
	     "stfd 19,152(%0);"
	     "stfd 20,160(%0);"
	     "stfd 21,168(%0);"
	     "stfd 22,176(%0);"
	     "stfd 23,184(%0);"
	     "stfd 24,192(%0);"
	     "stfd 25,200(%0);"
	     "stfd 26,208(%0);"
	     "stfd 27,216(%0);"
	     "stfd 28,224(%0);"
	     "stfd 29,232(%0);"
	     "stfd 30,240(%0);"
	     "stfd 31,248(%0)" :: "b"(&pcb->pcb_fpu.fpr[0]));
	__asm volatile ("mffs 0; stfd 0,0(%0)" :: "b"(&pcb->pcb_fpu.fpscr));
	__asm volatile ("mtmsr %0; isync" :: "r"(msr));
}
