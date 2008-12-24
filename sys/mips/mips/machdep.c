    /*	$OpenBSD: machdep.c,v 1.33 1998/09/15 10:58:54 pefo Exp $	*/
/* tracked to 1.38 */
/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department, The Mach Operating System project at
 * Carnegie-Mellon University and Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *	from: @(#)machdep.c	8.3 (Berkeley) 1/12/94
 *	Id: machdep.c,v 1.33 1998/09/15 10:58:54 pefo Exp
 *	JNPR: machdep.c,v 1.11.2.3 2007/08/29 12:24:49 girish
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"
#include "opt_md.h"
#include "opt_msgbuf.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/cpu.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/reboot.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_pager.h>
#include <vm/vm_extern.h>
#include <sys/socket.h>

#include <sys/user.h>
#include <sys/cons.h>
#include <sys/syslog.h>
#include <machine/cache.h>
#include <machine/cpu.h>
#include <machine/pltfm.h>
#include <net/netisr.h>
#include <machine/md_var.h>
#if 0
#include <machine/defs.h>
#endif
#include <machine/clock.h>
#include <machine/asm.h>
#include <machine/bootinfo.h>
#ifdef DDB
#include <sys/kdb.h>
#include <ddb/ddb.h>
#endif

#include <sys/random.h>
#include <net/if.h>

#define	BOOTINFO_DEBUG	0

char machine[] = "mips";
SYSCTL_STRING(_hw, HW_MACHINE, machine, CTLFLAG_RD, machine, 0, "Machine class");

static char cpu_model[30];
SYSCTL_STRING(_hw, HW_MODEL, model, CTLFLAG_RD, cpu_model, 0, "Machine model");

#if 0 /* see comment below */
static void getmemsize(void);
#endif

int cold = 1;
int Maxmem;
long realmem = 0;
int cpu_clock = MIPS_DEFAULT_HZ;
SYSCTL_INT(_hw, OID_AUTO, clockrate, CTLFLAG_RD, 
    &cpu_clock, 0, "CPU instruction clock rate");
int clocks_running = 0;

vm_offset_t kstack0;

#ifdef SMP
struct pcpu __pcpu[32];
char pcpu_boot_stack[KSTACK_PAGES * PAGE_SIZE * (MAXCPU-1)]; 
#else
struct pcpu pcpu;
struct pcpu *pcpup = &pcpu;
#endif

vm_offset_t phys_avail[10];
#ifdef UNIMPLEMENTED
struct platform platform;
#endif

vm_paddr_t	mips_wired_tlb_physmem_start;
vm_paddr_t	mips_wired_tlb_physmem_end;
u_int		need_wired_tlb_page_pool;

static void cpu_startup(void *);
SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL);

struct kva_md_info kmi;

int cpucfg;			/* Value of processor config register */
int num_tlbentries = 64;	/* Size of the CPU tlb */
int cputype;

extern char MipsException[], MipsExceptionEnd[];

/* TLB miss handler address and end */
extern char MipsTLBMiss[], MipsTLBMissEnd[];

/* Cache error handler */
extern char MipsCache[], MipsCacheEnd[];

extern char edata[], end[];

u_int32_t bootdev;
struct bootinfo bootinfo;


static void
cpu_startup(void *dummy)
{

	if (boothowto & RB_VERBOSE)
		bootverbose++;

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf("real memory  = %lu (%luK bytes)\n", ptoa(Maxmem),
	    ptoa(Maxmem) / 1024);
	realmem = Maxmem;
	/*
	 * Display any holes after the first chunk of extended memory.
	 */
	if (bootverbose) {
		int indx;

		printf("Physical memory chunk(s):\n");
		for (indx = 0; phys_avail[indx + 1] != 0; indx += 2) {
			int size1 = phys_avail[indx + 1] - phys_avail[indx];

			printf("0x%08x - 0x%08x, %u bytes (%u pages)\n",
			    phys_avail[indx], phys_avail[indx + 1] - 1, size1,
			    size1 / PAGE_SIZE);
		}
	}

	vm_ksubmap_init(&kmi);

	printf("avail memory = %lu (%luMB)\n", ptoa(cnt.v_free_count),
	    ptoa(cnt.v_free_count) / 1048576);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();
	vm_pager_bufferinit();
}

/*
 * Shutdown the CPU as much as possible
 */
void
cpu_reset(void)
{
	for (;;)
		;
}

/* Get current clock frequency for the given cpu id. */
int
cpu_est_clockrate(int cpu_id, uint64_t *rate)
{

    return (ENXIO);
}

/*
 * Shutdown the CPU as much as possible
 */
void
cpu_halt(void)
{
	for (;;)
		;
}

#ifdef PORT_TO_JMIPS

static int
sysctl_machdep_adjkerntz(SYSCTL_HANDLER_ARGS)
{
}

SYSCTL_PROC(_machdep, CPU_ADJKERNTZ, adjkerntz, CTLTYPE_INT | CTLFLAG_RW,
    &adjkerntz, 0, sysctl_machdep_adjkerntz, "I",
    "Local offset from GMT in seconds");
#endif	/* PORT_TO_JMIPS */

#ifdef PORT_TO_JMIPS
/* art */
SYSCTL_INT(_machdep, CPU_DISRTCSET, disable_rtc_set, CTLFLAG_RW,
    &disable_rtc_set, 0, "Disable setting the real time clock to system time");
#endif	/* PORT_TO_JMIPS */

SYSCTL_STRUCT(_machdep, CPU_BOOTINFO, bootinfo, CTLFLAG_RD, &bootinfo,
    bootinfo, "Bootinfo struct: kernel filename, BIOS harddisk geometry, etc");

#ifdef PORT_TO_JMIPS
/* dchu */
SYSCTL_INT(_machdep, CPU_WALLCLOCK, wall_cmos_clock, CTLFLAG_RW,
    &wall_cmos_clock, 0, "Wall CMOS clock assumed");
#endif	/* PORT_TO_JMIPS */

/*
 * Initialize mips and configure to run kernel
 */

void
mips_proc0_init(void)
{
	proc_linkup(&proc0, &thread0);
	thread0.td_kstack = kstack0;
	thread0.td_kstack_pages = KSTACK_PAGES - 1;
	if (thread0.td_kstack & (1 << PAGE_SHIFT))
		thread0.td_md.md_realstack = thread0.td_kstack + PAGE_SIZE;
	else
		thread0.td_md.md_realstack = thread0.td_kstack;
	/* Initialize pcpu info of cpu-zero */
#ifdef SMP
	pcpu_init(&__pcpu[0], 0, sizeof(struct pcpu));
#else
	pcpu_init(pcpup, 0, sizeof(struct pcpu));
#endif
	/* 
	 * Do not use cpu_thread_alloc to initialize these fields 
	 * thread0 is the only thread that has kstack located in KSEG0 
	 * while cpu_thread_alloc handles kstack allocated in KSEG2.
	 */
	thread0.td_pcb = (struct pcb *)(thread0.td_md.md_realstack +
	    (thread0.td_kstack_pages - 1) * PAGE_SIZE) - 1;
	thread0.td_frame = &thread0.td_pcb->pcb_regs;
	/*
	 * There is no need to initialize md_upte array for thread0 as it's
	 * located in .bss section and should be explicitly zeroed during 
	 * kernel initialization.
	 */

	PCPU_SET(curthread, &thread0);
	PCPU_SET(curpcb, thread0.td_pcb);
}

struct msgbuf *msgbufp=0;

#if 0
/*
 * This code has been moved to the platform_init code.  The only
 * thing that's beign done here that hasn't been moved is the wired tlb
 * pool stuff.  I'm still trying to understand that feature..., since
 * it maps from the end the kernel to 0x08000000 somehow.  But the stuff
 * was stripped out, so it is hard to say what's going on....
 */
u_int32_t	 freemem_start;

static void
getmemsize()
{
	vm_offset_t kern_start, kern_end;
	vm_offset_t AllowMem, memsize;
	const char *cp;
	size_t sz;
	int phys_avail_cnt;

	/* Determine memory layout */
	phys_avail_cnt = 0;
	kern_start = mips_trunc_page(MIPS_CACHED_TO_PHYS(btext));
	if (kern_start < freemem_start)
panic("kernel load address too low, overlapping with memory reserved for FPC IPC\n");

	if (kern_start > freemem_start) {
		phys_avail[phys_avail_cnt++] = freemem_start;
		/*
		 * Since the stack is setup just before kern_start,
		 * leave some space for stack to grow
		 */
		phys_avail[phys_avail_cnt++] = kern_start - PAGE_SIZE * 3;
		MIPS_DEBUG_PRINT("phys_avail : %p - %p",	\
		    phys_avail[phys_avail_cnt-2], phys_avail[phys_avail_cnt-1]);
	}

	kern_end = (vm_offset_t) end;
	kern_end = (vm_offset_t) mips_round_page(kern_end);
	MIPS_DEBUG_PRINT("kern_start : 0x%x, kern_end : 0x%x", btext, kern_end);
	phys_avail[phys_avail_cnt++] = MIPS_CACHED_TO_PHYS(kern_end);

	if (need_wired_tlb_page_pool) {
		mips_wired_tlb_physmem_start = MIPS_CACHED_TO_PHYS(kern_end);
		mips_wired_tlb_physmem_end = 0x08000000;
		MIPS_DEBUG_PRINT("%s: unmapped page start [0x%x]  end[0x%x]\n",\
		   __FUNCTION__, mips_wired_tlb_physmem_start, \
		   mips_wired_tlb_physmem_end);
		if (mips_wired_tlb_physmem_start > mips_wired_tlb_physmem_end)
		panic("Error in Page table page physical address assignment\n");
	}

	if (bootinfo.bi_memsizes_valid)
		memsize = bootinfo.bi_basemem * 1024;
	else {
		memsize = SDRAM_MEM_SIZE;
	}

	/*
	 * hw.physmem is a size in bytes; we also allow k, m, and g suffixes
	 * for the appropriate modifiers.
	 */
	if ((cp = getenv("hw.physmem")) != NULL) {
		vm_offset_t sanity;
		char *ep;

		sanity = AllowMem = strtouq(cp, &ep, 0);
		if ((ep != cp) && (*ep != 0)) {
			switch(*ep) {
			case 'g':
			case 'G':
				AllowMem <<= 10;
			case 'm':
			case 'M':
				AllowMem <<= 10;
			case 'k':
			case 'K':
				AllowMem <<= 10;
				break;
			default:
				AllowMem = sanity = 0;
			}
			if (AllowMem < sanity)
				AllowMem = 0;
		}
		if (!AllowMem || (AllowMem < (kern_end - KERNBASE)))
			printf("Ignoring invalid hw.physmem size of '%s'\n", cp);
	} else
		AllowMem = 0;

	if (AllowMem)
		memsize = (memsize > AllowMem) ? AllowMem : memsize;

	phys_avail[phys_avail_cnt++] = SDRAM_ADDR_START + memsize;
	MIPS_DEBUG_PRINT("phys_avail : 0x%x - 0x%x",	\
	    phys_avail[phys_avail_cnt-2], phys_avail[phys_avail_cnt-1]);
	phys_avail[phys_avail_cnt] = 0;

	physmem = btoc(memsize);
	Maxmem = physmem;

	/*
	 * Initialize error message buffer (at high end of memory).
	 */
	sz = round_page(MSGBUF_SIZE);
	msgbufp = (struct msgbuf *) pmap_steal_memory(sz);
	msgbufinit(msgbufp, sz);
	printf("%s: msgbufp[size=%d] = 0x%p\n", __FUNCTION__, sz, msgbufp);
}
#endif

/*
 * Initialize the hardware exception vectors, and the jump table used to
 * call locore cache and TLB management functions, based on the kind
 * of CPU the kernel is running on.
 */
void
mips_vector_init(void)
{
	/*
	 * Copy down exception vector code.
	 */
	if (MipsTLBMissEnd - MipsTLBMiss > 0x80)
		panic("startup: UTLB code too large");

	if (MipsCacheEnd - MipsCache > 0x80)
		panic("startup: Cache error code too large");

	bcopy(MipsTLBMiss, (void *)TLB_MISS_EXC_VEC,
	      MipsTLBMissEnd - MipsTLBMiss);

#ifdef TARGET_OCTEON
/* Fake, but sufficient, for the 32-bit with 64-bit hardware addresses  */
	bcopy(MipsTLBMiss, (void *)XTLB_MISS_EXC_VEC,
	      MipsTLBMissEnd - MipsTLBMiss);
#endif

	bcopy(MipsException, (void *)GEN_EXC_VEC,
	      MipsExceptionEnd - MipsException);

	bcopy(MipsCache, (void *)CACHE_ERR_EXC_VEC,
	      MipsCacheEnd - MipsCache);

	/*
	 * Clear out the I and D caches.
	 */
	mips_icache_sync_all();
	mips_dcache_wbinv_all();

	/* 
	 * Mask all interrupts. Each interrupt will be enabled
	 * when handler is installed for it
	 */
	set_intr_mask (ALL_INT_MASK);
	/* Clear BEV in SR so we start handling our own exceptions */
	mips_cp0_status_write(mips_cp0_status_read() & ~SR_BOOT_EXC_VEC);

}

/*
 * Initialise a struct pcpu.
 */
void
cpu_pcpu_init(struct pcpu *pcpu, int cpuid, size_t size)
{
#ifdef SMP
	if (cpuid != 0)
		pcpu->pc_boot_stack = (void *)(pcpu_boot_stack + cpuid *
		    (KSTACK_PAGES * PAGE_SIZE));
#endif
	pcpu->pc_next_asid = 1;
	pcpu->pc_asid_generation = 1;
}

int
sysarch(struct thread *td, register struct sysarch_args *uap)
{
	return (ENOSYS);
}

int
fill_dbregs(struct thread *td, struct dbreg *dbregs)
{

	/* No debug registers on mips */
	return (ENOSYS);
}

int
set_dbregs(struct thread *td, struct dbreg *dbregs)
{

	/* No debug registers on mips */
	return (ENOSYS);
}

int spinco;
void
spinlock_enter(void)
{
	struct thread *td;

	td = curthread;
	if (td->td_md.md_spinlock_count == 0)
		td->td_md.md_saved_intr = disableintr();
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
		restoreintr(td->td_md.md_saved_intr);
}

u_int32_t
get_cyclecount(void)
{
	u_int32_t count;

	mfc0_macro(count, 9);
	return (count);
}

/*
 * call platform specific code to halt (until next interrupt) for the idle loop
 */
void
cpu_idle(int busy)
{
	if (mips_cp0_status_read() & SR_INT_ENAB)
		__asm __volatile ("wait");
	else
		panic("ints disabled in idleproc!");
}

int
cpu_idle_wakeup(int cpu)
{

	return (0);
}

void
dumpsys(struct dumperinfo *di __unused)
{

	printf("Kernel dumps not implemented on this architecture\n");
}
