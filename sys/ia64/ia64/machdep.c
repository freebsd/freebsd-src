/*-
 * Copyright (c) 2000 Doug Rabson
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "opt_compat.h"
#include "opt_ddb.h"
#include "opt_simos.h"
#include "opt_msgbuf.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>
#include <sys/sysproto.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/reboot.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/mbuf.h>
#include <sys/vmmeter.h>
#include <sys/msgbuf.h>
#include <sys/exec.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/linker.h>
#include <sys/random.h>
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
#include <machine/clock.h>
#include <machine/md_var.h>
#include <machine/reg.h>
#include <machine/fpu.h>
#include <machine/pal.h>
#include <machine/efi.h>
#include <machine/bootinfo.h>
#include <machine/mutex.h>
#include <machine/vmparam.h>
#include <machine/elf.h>
#include <ddb/ddb.h>
#include <alpha/alpha/db_instruction.h>
#include <sys/vnode.h>
#include <miscfs/procfs/procfs.h>
#include <machine/sigframe.h>

u_int64_t cycles_per_usec;
u_int32_t cycles_per_sec;
int cold = 1;
struct bootinfo_kernel bootinfo;

struct cpuhead cpuhead;

MUTEX_DECLARE( ,sched_lock);
MUTEX_DECLARE( ,Giant);

struct	user *proc0paddr;

char machine[] = "ia64";
SYSCTL_STRING(_hw, HW_MACHINE, machine, CTLFLAG_RD, machine, 0, "");

static char cpu_model[128];
SYSCTL_STRING(_hw, HW_MODEL, model, CTLFLAG_RD, cpu_model, 0, "");

#ifdef DDB
/* start and end of kernel symbol table */
void	*ksym_start, *ksym_end;
#endif

int	ia64_unaligned_print = 1;	/* warn about unaligned accesses */
int	ia64_unaligned_fix = 1;	/* fix up unaligned accesses */
int	ia64_unaligned_sigbus = 0;	/* don't SIGBUS on fixed-up accesses */

SYSCTL_INT(_machdep, CPU_UNALIGNED_PRINT, unaligned_print,
	CTLFLAG_RW, &ia64_unaligned_print, 0, "");

SYSCTL_INT(_machdep, CPU_UNALIGNED_FIX, unaligned_fix,
	CTLFLAG_RW, &ia64_unaligned_fix, 0, "");

SYSCTL_INT(_machdep, CPU_UNALIGNED_SIGBUS, unaligned_sigbus,
	CTLFLAG_RW, &ia64_unaligned_sigbus, 0, "");

static void cpu_startup __P((void *));
SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL)

static MALLOC_DEFINE(M_MBUF, "mbuf", "mbuf");

struct msgbuf *msgbufp=0;

int bootverbose = 0, Maxmem = 0;
long dumplo;

int	totalphysmem;		/* total amount of physical memory in system */
int	physmem;		/* physical memory used by NetBSD + some rsvd */
int	resvmem;		/* amount of memory reserved for PROM */
int	unusedmem;		/* amount of memory for OS that we don't use */
int	unknownmem;		/* amount of memory with an unknown use */
int	ncpus;			/* number of cpus */

vm_offset_t phys_avail[10];

static int
sysctl_hw_physmem(SYSCTL_HANDLER_ARGS)
{
	int error = sysctl_handle_int(oidp, 0, ia64_ptob(physmem), req);
	return (error);
}

SYSCTL_PROC(_hw, HW_PHYSMEM, physmem, CTLTYPE_INT|CTLFLAG_RD,
	0, 0, sysctl_hw_physmem, "I", "");

static int
sysctl_hw_usermem(SYSCTL_HANDLER_ARGS)
{
	int error = sysctl_handle_int(oidp, 0,
		ia64_ptob(physmem - cnt.v_wire_count), req);
	return (error);
}

SYSCTL_PROC(_hw, HW_USERMEM, usermem, CTLTYPE_INT|CTLFLAG_RD,
	0, 0, sysctl_hw_usermem, "I", "");

SYSCTL_INT(_hw, OID_AUTO, availpages, CTLFLAG_RD, &physmem, 0, "");

/* must be 2 less so 0 0 can signal end of chunks */
#define PHYS_AVAIL_ARRAY_END ((sizeof(phys_avail) / sizeof(vm_offset_t)) - 2)

static void identifycpu __P((void));

static vm_offset_t buffer_sva, buffer_eva;
vm_offset_t clean_sva, clean_eva;
static vm_offset_t pager_sva, pager_eva;

static void
cpu_startup(dummy)
	void *dummy;
{
	unsigned int i;
	caddr_t v;
	vm_offset_t maxaddr;
	vm_size_t size = 0;
	vm_offset_t firstaddr;
	vm_offset_t minaddr;

	if (boothowto & RB_VERBOSE)
		bootverbose++;

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf("%s", version);
	identifycpu();

	/* startrtclock(); */
#ifdef PERFMON
	perfmon_init();
#endif
	printf("real memory  = %ld (%ldK bytes)\n", ia64_ptob(Maxmem), ia64_ptob(Maxmem) / 1024);

	/*
	 * Display any holes after the first chunk of extended memory.
	 */
	if (bootverbose) {
		int indx;

		printf("Physical memory chunk(s):\n");
		for (indx = 0; phys_avail[indx + 1] != 0; indx += 2) {
			int size1 = phys_avail[indx + 1] - phys_avail[indx];

			printf("0x%08lx - 0x%08lx, %d bytes (%d pages)\n", phys_avail[indx],
			    phys_avail[indx + 1] - 1, size1, size1 / PAGE_SIZE);
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
		int factor = 4 * BKVASIZE / PAGE_SIZE;

		nbuf = 50;
		if (physmem > 1024)
			nbuf += min((physmem - 1024) / factor, 16384 / factor);
		if (physmem > 16384)
			nbuf += (physmem - 16384) * 2 / (factor * 5);
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
		firstaddr = (vm_offset_t)kmem_alloc(kernel_map, round_page(size));
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
	 * Finally, allocate mbuf pool.  Since mclrefcnt is an off-size
	 * we use the more space efficient malloc in place of kmem_alloc.
	 */
	{
		vm_offset_t mb_map_size;

		mb_map_size = nmbufs * MSIZE + nmbclusters * MCLBYTES +
		    (nmbclusters + nmbufs / 4) * sizeof(union mext_refcnt);
		mb_map_size = roundup2(mb_map_size, max(MCLBYTES, PAGE_SIZE));
		mb_map = kmem_suballoc(kmem_map, (vm_offset_t *)&mbutl,
		    &maxaddr, mb_map_size);
		mb_map->system_map = 1;
	}

	/*
	 * Initialize callouts
	 */
	SLIST_INIT(&callfree);
	for (i = 0; i < ncallout; i++) {
		callout_init(&callout[i]);
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

int
unregister_netisr(num)
	int num;
{
	
	if (num < 0 || num >= (sizeof(netisrs)/sizeof(*netisrs)) ) {
		printf("unregister_netisr: bad isr number: %d\n", num);
		return (EINVAL);
	}
	netisrs[num] = NULL;
	return (0);
}

static void
identifycpu(void)
{
	/* print cpu type & version */
}

extern char kernel_text[], _end[];

#define DEBUG_MD

void
ia64_init()
{
	int phys_avail_cnt;
	vm_offset_t kernstart, kernend;
	vm_offset_t kernstartpfn, kernendpfn, pfn0, pfn1;
	char *p;
	struct efi_memory_descriptor ski_md[2]; /* XXX */
	struct efi_memory_descriptor *mdp;
	int mdcount, i;

	/* NO OUTPUT ALLOWED UNTIL FURTHER NOTICE */

	/*
	 * TODO: Disable interrupts, floating point etc.
	 * Maybe flush cache and tlb
	 */
	__asm __volatile("mov ar.fpsr=%0" :: "r"(IA64_FPSR_DEFAULT));

	/*
	 * TODO: Get critical system information (if possible, from the
	 * information provided by the boot program).
	 */

	/*
	 * Initalize the (temporary) bootstrap console interface, so
	 * we can use printf until the VM system starts being setup.
	 * The real console is initialized before then.
	 * TODO: I guess we start with a serial console here.
	 */
	ssccnattach();

	/* OUTPUT NOW ALLOWED */

	/*
	 * Find the beginning and end of the kernel.
	 */
	kernstart = trunc_page(kernel_text);
#ifdef DDBxx
	ksym_start = (void *)bootinfo.ssym;
	ksym_end   = (void *)bootinfo.esym;
	kernend = (vm_offset_t)round_page(ksym_end);
#else
	kernend = (vm_offset_t)round_page(_end);
#endif
	/* But if the bootstrap tells us otherwise, believe it! */
	if (bootinfo.kernend)
		kernend = round_page(bootinfo.kernend);
	preload_metadata = (caddr_t)bootinfo.modptr;
	kern_envp = bootinfo.envp;

	p = getenv("kernelname");
	if (p)
		strncpy(kernelname, p, sizeof(kernelname) - 1);

	kernstartpfn = atop(IA64_RR_MASK(kernstart));
	kernendpfn = atop(IA64_RR_MASK(kernend));

	/*
	 * Size the memory regions and load phys_avail[] with the results.
	 */

	/*
	 * XXX hack for ski. In reality, the loader will probably ask
	 * EFI and pass the results to us. Possibly, we will call EFI
	 * directly.
	 */
	ski_md[0].emd_type = EFI_CONVENTIONAL_MEMORY;
	ski_md[0].emd_physical_start = 2L*1024*1024;
	ski_md[0].emd_virtul_start = 0;
	ski_md[0].emd_number_of_pages = (64L*1024*1024)>>12;
	ski_md[0].emd_attribute = EFI_MEMORY_WB;

	ski_md[1].emd_type = EFI_CONVENTIONAL_MEMORY;
	ski_md[1].emd_physical_start = 4096L*1024*1024;
	ski_md[1].emd_virtul_start = 0;
	ski_md[1].emd_number_of_pages = (32L*1024*1024)>>12;
	ski_md[1].emd_attribute = EFI_MEMORY_WB;
	
	mdcount = 1;		/* ignore the high memory for now */

	/*
	 * Find out how much memory is available, by looking at
	 * the memory descriptors.
	 */
#ifdef DEBUG_MD
	printf("Memory descriptor count: %d\n", mdcount);
#endif

	phys_avail_cnt = 0;
	for (i = 0; i < mdcount; i++) {
		mdp = &ski_md[i];
#ifdef DEBUG_MD
		printf("MD %d: type %d pa 0x%lx cnt 0x%lx\n", i,
		       mdp->emd_type,
		       mdp->emd_physical_start,
		       mdp->emd_number_of_pages);
#endif
		totalphysmem += mdp->emd_number_of_pages;

		if (mdp->emd_type != EFI_CONVENTIONAL_MEMORY) {
			resvmem += mdp->emd_number_of_pages;
			continue;
		}

		/*
		 * We have a memory descriptors available for system
		 * software use.  We must determine if this cluster
		 * holds the kernel.
		 */
		physmem += mdp->emd_number_of_pages;
		pfn0 = atop(mdp->emd_physical_start);
		pfn1 = pfn0 + mdp->emd_number_of_pages;
		if (pfn0 <= kernendpfn && kernstartpfn <= pfn1) {
			/*
			 * Must compute the location of the kernel
			 * within the segment.
			 */
#ifdef DEBUG_MD
			printf("Descriptor %d contains kernel\n", i);
#endif
			if (pfn0 < kernstartpfn) {
				/*
				 * There is a chunk before the kernel.
				 */
#ifdef DEBUG_MD
				printf("Loading chunk before kernel: "
				       "0x%lx / 0x%lx\n", pfn0, kernstartpfn);
#endif
				phys_avail[phys_avail_cnt] = ia64_ptob(pfn0);
				phys_avail[phys_avail_cnt+1] = ia64_ptob(kernstartpfn);
				phys_avail_cnt += 2;
			}
			if (kernendpfn < pfn1) {
				/*
				 * There is a chunk after the kernel.
				 */
#ifdef DEBUG_MD
				printf("Loading chunk after kernel: "
				       "0x%lx / 0x%lx\n", kernendpfn, pfn1);
#endif
				phys_avail[phys_avail_cnt] = ia64_ptob(kernendpfn);
				phys_avail[phys_avail_cnt+1] = ia64_ptob(pfn1);
				phys_avail_cnt += 2;
			}
		} else {
			/*
			 * Just load this cluster as one chunk.
			 */
#ifdef DEBUG_MD
			printf("Loading descriptor %d: 0x%lx / 0x%lx\n", i,
			       pfn0, pfn1);
#endif
			phys_avail[phys_avail_cnt] = ia64_ptob(pfn0);
			phys_avail[phys_avail_cnt+1] = ia64_ptob(pfn1);
			phys_avail_cnt += 2;
			
		}
	}
	phys_avail[phys_avail_cnt] = 0;

	Maxmem = physmem;

	/*
	 * Initialize error message buffer (at end of core).
	 */
	{
		size_t sz = round_page(MSGBUF_SIZE);
		int i = phys_avail_cnt - 2;

		/* shrink so that it'll fit in the last segment */
		if (phys_avail[i+1] - phys_avail[i] < sz)
			sz = phys_avail[i+1] - phys_avail[i];

		phys_avail[i+1] -= sz;
		msgbufp = (struct msgbuf*) IA64_PHYS_TO_RR7(phys_avail[i+1]);

		msgbufinit(msgbufp, sz);

		/* Remove the last segment if it now has no pages. */
		if (phys_avail[i] == phys_avail[i+1])
			phys_avail[i] = 0;

		/* warn if the message buffer had to be shrunk */
		if (sz != round_page(MSGBUF_SIZE))
			printf("WARNING: %ld bytes not available for msgbuf in last cluster (%ld used)\n",
			    round_page(MSGBUF_SIZE), sz);

	}

	/*
	 * Init mapping for u page(s) for proc 0
	 */
	proc0paddr = proc0.p_addr =
	    (struct user *)pmap_steal_memory(UPAGES * PAGE_SIZE);

	/*
	 * Setup the global data for the bootstrap cpu.
	 */
	{
		size_t sz = round_page(UPAGES * PAGE_SIZE);
		globalp = (struct globaldata *) pmap_steal_memory(sz);
		globaldata_init(globalp, 0, sz);
		ia64_set_k4((u_int64_t) globalp);
		PCPU_GET(next_asn) = 1;	/* 0 used for proc0 pmap */
	}

	/*
	 * Initialize the virtual memory system, and set the
	 * page table base register in proc 0's PCB.
	 */
	pmap_bootstrap();

	/*
	 * Initialize the rest of proc 0's PCB.
	 *
	 * Set the kernel sp, reserving space for an (empty) trapframe,
	 * and make proc0's trapframe pointer point to it for sanity.
	 * Initialise proc0's backing store to start after u area.
	 */
	proc0.p_addr->u_pcb.pcb_sp =
	    (u_int64_t)proc0.p_addr + USPACE - sizeof(struct trapframe) - 16;
	proc0.p_addr->u_pcb.pcb_bspstore = (u_int64_t) (proc0.p_addr + 1);
	proc0.p_md.md_tf =
	    (struct trapframe *)(proc0.p_addr->u_pcb.pcb_sp + 16);

	PCPU_SET(curproc, &proc0);

	/*
	 * Record all cpus in a list.
	 */
	SLIST_INIT(&cpuhead);
	SLIST_INSERT_HEAD(&cpuhead, GLOBALP, gd_allcpu);

	/*
	 * Initialise mutexes.
	 */
	mtx_init(&Giant, "Giant", MTX_DEF | MTX_COLD);
	mtx_init(&sched_lock, "sched lock", MTX_SPIN | MTX_COLD);

#if 0
	/*
	 * Enable interrupts on first release (in switch_trampoline).
	 */
	sched_lock.mtx_saveipl = ALPHA_PSL_IPL_0;
#endif

	/*
	 * Look at arguments passed to us and compute boothowto.
	 */
	boothowto = 0;
#ifdef KADB
	boothowto |= RB_KDB;
#endif
/*	boothowto |= RB_KDB | RB_GDB; */
	for (p = bootinfo.boot_flags; p && *p != '\0'; p++) {
		/*
		 * Note that we'd really like to differentiate case here,
		 * but the Ia64 AXP Architecture Reference Manual
		 * says that we shouldn't.
		 */
		switch (*p) {
		case 'a': /* autoboot */
		case 'A':
			boothowto &= ~RB_SINGLE;
			break;

#ifdef DEBUG
		case 'c': /* crash dump immediately after autoconfig */
		case 'C':
			boothowto |= RB_DUMP;
			break;
#endif

#if defined(DDB)
		case 'd': /* break into the kernel debugger ASAP */
		case 'D':
			boothowto |= RB_KDB;
			break;
		case 'g': /* use kernel gdb */
		case 'G':
			boothowto |= RB_GDB;
			break;
#endif

		case 'h': /* always halt, never reboot */
		case 'H':
			boothowto |= RB_HALT;
			break;

#if 0
		case 'm': /* mini root present in memory */
		case 'M':
			boothowto |= RB_MINIROOT;
			break;
#endif

		case 'n': /* askname */
		case 'N':
			boothowto |= RB_ASKNAME;
			break;

		case 's': /* single-user (default, supported for sanity) */
		case 'S':
			boothowto |= RB_SINGLE;
			break;

		case 'v':
		case 'V':
			boothowto |= RB_VERBOSE;
			bootverbose = 1;
			break;

		default:
			printf("Unrecognized boot flag '%c'.\n", *p);
			break;
		}
	}

	/*
	 * Catch case of boot_verbose set in environment.
	 */
	if ((p = getenv("boot_verbose")) != NULL) {
		if (strcmp(p, "yes") == 0 || strcmp(p, "YES") == 0) {
			boothowto |= RB_VERBOSE;
			bootverbose = 1;
		}
	}

	/*
	 * Force single-user for a while.
	 */
	boothowto |= RB_SINGLE;

	/*
	 * Initialize debuggers, and break into them if appropriate.
	 */
#ifdef DDB
	kdb_init();
	if (boothowto & RB_KDB) {
		printf("Boot flags requested debugger\n");
		breakpoint();
	}
#endif
}

void
bzero(void *buf, size_t len)
{
	caddr_t p = buf;

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
DELAY(int n)
{
    /* TODO */
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
sendsig(sig_t catcher, int sig, sigset_t *mask, u_long code)
{
	struct proc *p = curproc;
	struct trapframe *frame;
	struct sigacts *psp = p->p_sigacts;
	struct sigframe sf, *sfp;
	u_int64_t sbs = 0;
	int oonstack, rndfsize;

	frame = p->p_md.md_tf;
	oonstack = (p->p_sigstk.ss_flags & SS_ONSTACK) ? 1 : 0;
	rndfsize = ((sizeof(sf) + 15) / 16) * 16;

	/*
	 * Make sure that we restore the entire trapframe after a
	 * signal.
	 */
	frame->tf_flags &= ~FRAME_SYSCALL;

	/* save user context */
	bzero(&sf, sizeof(struct sigframe));
	sf.sf_uc.uc_sigmask = *mask;
	sf.sf_uc.uc_stack = p->p_sigstk;
	sf.sf_uc.uc_mcontext.mc_flags = IA64_MC_FLAG_ONSTACK;

	sf.sf_uc.uc_mcontext.mc_nat     = 0; /* XXX */
	sf.sf_uc.uc_mcontext.mc_sp	= frame->tf_r[FRAME_SP];
	sf.sf_uc.uc_mcontext.mc_ip	= (frame->tf_cr_iip
					   | ((frame->tf_cr_ipsr >> 41) & 3));
	sf.sf_uc.uc_mcontext.mc_cfm     = frame->tf_cr_ifs & ~(1<<31);
	sf.sf_uc.uc_mcontext.mc_um      = frame->tf_cr_ipsr & 0x1fff;
	sf.sf_uc.uc_mcontext.mc_ar_rsc  = frame->tf_ar_rsc;
	sf.sf_uc.uc_mcontext.mc_ar_bsp  = frame->tf_ar_bspstore;
	sf.sf_uc.uc_mcontext.mc_ar_rnat = frame->tf_ar_rnat;
	sf.sf_uc.uc_mcontext.mc_ar_ccv  = frame->tf_ar_ccv;
	sf.sf_uc.uc_mcontext.mc_ar_unat = frame->tf_ar_unat;
	sf.sf_uc.uc_mcontext.mc_ar_fpsr = frame->tf_ar_fpsr;
	sf.sf_uc.uc_mcontext.mc_ar_pfs  = frame->tf_ar_pfs;
	sf.sf_uc.uc_mcontext.mc_pr      = frame->tf_pr;

	bcopy(&frame->tf_b[0],
	      &sf.sf_uc.uc_mcontext.mc_br[0],
	      8 * sizeof(unsigned long));
	sf.sf_uc.uc_mcontext.mc_gr[0] = 0;
	bcopy(&frame->tf_r[0],
	      &sf.sf_uc.uc_mcontext.mc_gr[1],
	      31 * sizeof(unsigned long));

	/* XXX mc_fr[] */

	/*
	 * Allocate and validate space for the signal handler
	 * context. Note that if the stack is in P0 space, the
	 * call to grow() is a nop, and the useracc() check
	 * will fail if the process has not already allocated
	 * the space with a `brk'.
	 */
	if ((p->p_flag & P_ALTSTACK) != 0 && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		sbs = (u_int64_t) p->p_sigstk.ss_sp;
		sfp = (struct sigframe *)((caddr_t)p->p_sigstk.ss_sp +
		    p->p_sigstk.ss_size - rndfsize);
		p->p_sigstk.ss_flags |= SS_ONSTACK;
		sf.sf_uc.uc_mcontext.mc_onstack |= 1;
	} else
		sfp = (struct sigframe *)(frame->tf_r[FRAME_SP] - rndfsize);

	(void)grow_stack(p, (u_long)sfp);
#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sendsig(%d): sig %d ssp %p usp %p\n", p->p_pid,
		       sig, &sf, sfp);
#endif
	if (!useracc((caddr_t)sfp, sizeof(sf), VM_PROT_WRITE)) {
#ifdef DEBUG
		if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
			printf("sendsig(%d): useracc failed on sig %d\n",
			       p->p_pid, sig);
#endif
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		SIGACTION(p, SIGILL) = SIG_DFL;
		SIGDELSET(p->p_sigignore, SIGILL);
		SIGDELSET(p->p_sigcatch, SIGILL);
		SIGDELSET(p->p_sigmask, SIGILL);
		psignal(p, SIGILL);
		return;
	}

#if 0
	/* save the floating-point state, if necessary, then copy it. */
	ia64_fpstate_save(p, 1);
	sf.sf_uc.uc_mcontext.mc_ownedfp = p->p_md.md_flags & MDP_FPUSED;
	bcopy(&p->p_addr->u_pcb.pcb_fp,
	      (struct fpreg *)sf.sf_uc.uc_mcontext.mc_fpregs,
	      sizeof(struct fpreg));
	sf.sf_uc.uc_mcontext.mc_fp_control = p->p_addr->u_pcb.pcb_fp_control;
#endif

	/*
	 * copy the frame out to userland.
	 */
	(void) copyout((caddr_t)&sf, (caddr_t)sfp, sizeof(sf));
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sendsig(%d): sig %d sfp %p code %lx\n", p->p_pid, sig,
		    sfp, code);
#endif

	/*
	 * Set up the registers to return to sigcode.
	 */
	frame->tf_cr_ipsr &= ~IA64_PSR_RI;
	frame->tf_cr_iip = PS_STRINGS - (esigcode - sigcode);
	frame->tf_r[FRAME_R1] = sig;
	if (SIGISMEMBER(p->p_sigacts->ps_siginfo, sig)) {
		frame->tf_r[FRAME_R15] = (u_int64_t)&(sfp->sf_si);

		/* Fill in POSIX parts */
		sf.sf_si.si_signo = sig;
		sf.sf_si.si_code = code;
		sf.sf_si.si_addr = (void*)frame->tf_cr_ifa;
	}
	else
		frame->tf_r[FRAME_R15] = code;

	frame->tf_r[FRAME_SP] = (u_int64_t)sfp - 16;
	frame->tf_r[FRAME_R14] = sig;
	frame->tf_r[FRAME_R15] = (u_int64_t) &sfp->sf_si;
	frame->tf_r[FRAME_R16] = (u_int64_t) &sfp->sf_uc;
	frame->tf_r[FRAME_R17] = (u_int64_t)catcher;
	frame->tf_r[FRAME_R18] = sbs;

#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sendsig(%d): pc %lx, catcher %lx\n", p->p_pid,
		    frame->tf_cr_iip, frame->tf_regs[FRAME_R4]);
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sendsig(%d): sig %d returns\n",
		    p->p_pid, sig);
#endif
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
osigreturn(struct proc *p,
	struct osigreturn_args /* {
		struct osigcontext *sigcntxp;
	} */ *uap)
{
	return EOPNOTSUPP;
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
sigreturn(struct proc *p,
	struct sigreturn_args /* {
		ucontext_t *sigcntxp;
	} */ *uap)
{
	ucontext_t uc, *ucp;
	struct pcb *pcb;
	struct trapframe *frame = p->p_md.md_tf;
	struct __mcontext *mcp;

	ucp = uap->sigcntxp;
	pcb = &p->p_addr->u_pcb;

#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
	    printf("sigreturn: pid %d, scp %p\n", p->p_pid, ucp);
#endif

	/*
	 * Fetch the entire context structure at once for speed.
	 * We don't use a normal argument to simplify RSE handling.
	 */
	if (copyin((caddr_t)frame->tf_r[FRAME_R4],
		   (caddr_t)&uc, sizeof(ucontext_t)))
		return (EFAULT);

	/*
	 * Restore the user-supplied information
	 */
	mcp = &uc.uc_mcontext;
	bcopy(&mcp->mc_br[0], &frame->tf_b[0], 8*sizeof(u_int64_t));
	bcopy(&mcp->mc_gr[1], &frame->tf_r[0], 31*sizeof(u_int64_t));
	/* XXX mc_fr */

	frame->tf_flags &= ~FRAME_SYSCALL;
	frame->tf_cr_iip = mcp->mc_ip & ~15;
	frame->tf_cr_ipsr &= ~IA64_PSR_RI;
	switch (mcp->mc_ip & 15) {
	case 1:
		frame->tf_cr_ipsr |= IA64_PSR_RI_1;
		break;
	case 2:
		frame->tf_cr_ipsr |= IA64_PSR_RI_2;
		break;
	}
	frame->tf_cr_ipsr     = ((frame->tf_cr_ipsr & ~0x1fff)
				 | (mcp->mc_um & 0x1fff));
	frame->tf_pr          = mcp->mc_pr;
	frame->tf_ar_rsc      = (mcp->mc_ar_rsc & 3) | 12; /* user, loadrs=0 */
	frame->tf_ar_pfs      = mcp->mc_ar_pfs;
	frame->tf_cr_ifs      = mcp->mc_cfm | (1UL<<63);
	frame->tf_ar_bspstore = mcp->mc_ar_bsp;
	frame->tf_ar_rnat     = mcp->mc_ar_rnat;
	frame->tf_ndirty      = 0; /* assumes flushrs in sigcode */
	frame->tf_ar_unat     = mcp->mc_ar_unat;
	frame->tf_ar_ccv      = mcp->mc_ar_ccv;
	frame->tf_ar_fpsr     = mcp->mc_ar_fpsr;

	frame->tf_r[FRAME_SP] = mcp->mc_sp;

	if (uc.uc_mcontext.mc_onstack & 1)
		p->p_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigstk.ss_flags &= ~SS_ONSTACK;

	p->p_sigmask = uc.uc_sigmask;
	SIG_CANTMASK(p->p_sigmask);

	/* XXX ksc.sc_ownedfp ? */
	ia64_fpstate_drop(p);
#if 0
	bcopy((struct fpreg *)uc.uc_mcontext.mc_fpregs,
	      &p->p_addr->u_pcb.pcb_fp, sizeof(struct fpreg));
	p->p_addr->u_pcb.pcb_fp_control =
		uc.uc_mcontext.mc_fp_control;
#endif

#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sigreturn(%d): returns\n", p->p_pid);
#endif
	return (EJUSTRETURN);
}

/*
 * Machine dependent boot() routine
 *
 * I haven't seen anything to put here yet
 * Possibly some stuff might be grafted back here from boot()
 */
void
cpu_boot(int howto)
{
}

/*
 * Shutdown the CPU as much as possible
 */
void
cpu_halt(void)
{
    /* TODO */
}

/*
 * Clear registers on exec
 */
void
setregs(struct proc *p, u_long entry, u_long stack, u_long ps_strings)
{
	struct trapframe *frame;

	frame = p->p_md.md_tf;

	/*
	 * Make sure that we restore the entire trapframe after an
	 * execve.
	 */
	frame->tf_flags &= ~FRAME_SYSCALL;

	bzero(frame->tf_r, sizeof(frame->tf_r));
	bzero(frame->tf_f, sizeof(frame->tf_f));
	frame->tf_cr_iip = entry;
	frame->tf_cr_ipsr = (IA64_PSR_IC
			     | IA64_PSR_I
			     | IA64_PSR_IT
			     | IA64_PSR_DT
			     | IA64_PSR_RT
			     | IA64_PSR_DFH
			     | IA64_PSR_BN
			     | IA64_PSR_CPL_USER);
	frame->tf_r[FRAME_SP] = stack;
	frame->tf_r[FRAME_R14] = ps_strings;

	/*
	 * Setup the new backing store and make sure the new image
	 * starts executing with an empty register stack frame.
	 */
	frame->tf_ar_bspstore = p->p_md.md_bspstore;
	frame->tf_ndirty = 0;
	frame->tf_cr_ifs = (1L<<63); /* ifm=0, v=1 */
	frame->tf_ar_rsc = 0xf;	/* user mode rsc */
	frame->tf_ar_fpsr = IA64_FPSR_DEFAULT;

	p->p_md.md_flags &= ~MDP_FPUSED;
	ia64_fpstate_drop(p);
}

int
ptrace_set_pc(struct proc *p, unsigned long addr)
{
	/* TODO set pc in trapframe */
	return 0;
}

int
ptrace_single_step(struct proc *p)
{
	/* TODO arrange for user process to single step */
	return 0;
}

int ptrace_read_u_check(struct proc *p, vm_offset_t addr, size_t len)
{
	vm_offset_t gap;

	if ((vm_offset_t) (addr + len) < addr)
		return EPERM;
	if ((vm_offset_t) (addr + len) <= sizeof(struct user))
		return 0;

	gap = (char *) p->p_md.md_tf - (char *) p->p_addr;
	
	if ((vm_offset_t) addr < gap)
		return EPERM;
	if ((vm_offset_t) (addr + len) <= 
	    (vm_offset_t) (gap + sizeof(struct trapframe)))
		return 0;
	return EPERM;
}

int
ptrace_write_u(struct proc *p, vm_offset_t off, long data)
{
	vm_offset_t min;
#if 0
	struct trapframe frame_copy;
	struct trapframe *tp;
#endif

	/*
	 * Privileged kernel state is scattered all over the user area.
	 * Only allow write access to parts of regs and to fpregs.
	 */
	min = (char *)p->p_md.md_tf - (char *)p->p_addr;
	if (off >= min && off <= min + sizeof(struct trapframe) - sizeof(int)) {
#if 0
		tp = p->p_md.md_tf;
		frame_copy = *tp;
		*(int *)((char *)&frame_copy + (off - min)) = data;
		if (!EFLAGS_SECURE(frame_copy.tf_eflags, tp->tf_eflags) ||
		    !CS_SECURE(frame_copy.tf_cs))
			return (EINVAL);
#endif
		*(int*)((char *)p->p_addr + off) = data;
		return (0);
	}
	min = offsetof(struct user, u_pcb);
	if (off >= min && off <= min + sizeof(struct pcb)) {
		*(int*)((char *)p->p_addr + off) = data;
		return (0);
	}
	return (EFAULT);
}

int
ia64_pa_access(vm_offset_t pa)
{
	return VM_PROT_READ|VM_PROT_WRITE;
}

int
fill_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
	/* TODO copy trapframe to regs */
	return (0);
}

int
set_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
	/* TODO copy regs to trapframe */
	return (0);
}

int
fill_fpregs(p, fpregs)
	struct proc *p;
	struct fpreg *fpregs;
{
	/* TODO copy fpu state to fpregs */
	ia64_fpstate_save(p, 0);

#if 0
	bcopy(&p->p_addr->u_pcb.pcb_fp, fpregs, sizeof *fpregs);
#endif
	return (0);
}

int
set_fpregs(p, fpregs)
	struct proc *p;
	struct fpreg *fpregs;
{
	/* TODO copy fpregs fpu state */
	ia64_fpstate_drop(p);

#if 0
	bcopy(fpregs, &p->p_addr->u_pcb.pcb_fp, sizeof *fpregs);
#endif
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

/*
 * Determine the size of the transfer, and make sure it is
 * within the boundaries of the partition. Adjust transfer
 * if needed, and signal errors or early completion.
 */
int
bounds_check_with_label(struct bio *bp, struct disklabel *lp, int wlabel)
{
#if 0
        struct partition *p = lp->d_partitions + dkpart(bp->bio_dev);
        int labelsect = lp->d_partitions[0].p_offset;
        int maxsz = p->p_size,
                sz = (bp->bio_bcount + DEV_BSIZE - 1) >> DEV_BSHIFT;

        /* overwriting disk label ? */
        /* XXX should also protect bootstrap in first 8K */
        if (bp->bio_blkno + p->p_offset <= LABELSECTOR + labelsect &&
#if LABELSECTOR != 0
            bp->bio_blkno + p->p_offset + sz > LABELSECTOR + labelsect &&
#endif
            (bp->bio_cmd == BIO_WRITE) && wlabel == 0) {
                bp->bio_error = EROFS;
                goto bad;
        }

#if     defined(DOSBBSECTOR) && defined(notyet)
        /* overwriting master boot record? */
        if (bp->bio_blkno + p->p_offset <= DOSBBSECTOR &&
            (bp->bio_cmd == BIO_WRITE) && wlabel == 0) {
                bp->bio_error = EROFS;
                goto bad;
        }
#endif

        /* beyond partition? */
        if (bp->bio_blkno < 0 || bp->bio_blkno + sz > maxsz) {
                /* if exactly at end of disk, return an EOF */
                if (bp->bio_blkno == maxsz) {
                        bp->bio_resid = bp->bio_bcount;
                        return(0);
                }
                /* or truncate if part of it fits */
                sz = maxsz - bp->bio_blkno;
                if (sz <= 0) {
                        bp->bio_error = EINVAL;
                        goto bad;
                }
                bp->bio_bcount = sz << DEV_BSHIFT;
        }

        bp->bio_pblkno = bp->bio_blkno + p->p_offset;
        return(1);

bad:
        bp->bio_flags |= BIO_ERROR;
#endif
        return(-1);

}

static int
sysctl_machdep_adjkerntz(SYSCTL_HANDLER_ARGS)
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

SYSCTL_INT(_machdep, CPU_WALLCLOCK, wall_cmos_clock,
	CTLFLAG_RW, &wall_cmos_clock, 0, "");

void
ia64_fpstate_check(struct proc *p)
{
	if ((p->p_md.md_tf->tf_cr_ipsr & IA64_PSR_DFH) == 0)
		if (p != PCPU_GET(fpcurproc))
			panic("ia64_check_fpcurproc: bogus");
}

/*
 * Save the high floating point state in the pcb. Use this to get
 * read-only access to the floating point state. If write is true, the
 * current fp process is cleared so that fp state can safely be
 * modified. The process will automatically reload the changed state
 * by generating a disabled fp trap.
 */
void
ia64_fpstate_save(struct proc *p, int write)
{
	if (p == PCPU_GET(fpcurproc)) {
		/*
		 * Save the state in the pcb.
		 */
		savehighfp(p->p_addr->u_pcb.pcb_highfp);

		if (write) {
			p->p_md.md_tf->tf_cr_ipsr |= IA64_PSR_DFH;
			PCPU_SET(fpcurproc, NULL);
		}
	}
}

/*
 * Relinquish ownership of the FP state. This is called instead of
 * ia64_save_fpstate() if the entire FP state is being changed
 * (e.g. on sigreturn).
 */
void
ia64_fpstate_drop(struct proc *p)
{
	if (p == PCPU_GET(fpcurproc)) {
		p->p_md.md_tf->tf_cr_ipsr |= IA64_PSR_DFH;
		PCPU_SET(fpcurproc, NULL);
	}
}

/*
 * Switch the current owner of the fp state to p, reloading the state
 * from the pcb.
 */
void
ia64_fpstate_switch(struct proc *p)
{
	if (PCPU_GET(fpcurproc)) {
		/*
		 * Dump the old fp state if its valid.
		 */
		savehighfp(PCPU_GET(fpcurproc)->p_addr->u_pcb.pcb_highfp);
		PCPU_GET(fpcurproc)->p_md.md_tf->tf_cr_ipsr |= IA64_PSR_DFH;
	}

	/*
	 * Remember the new FP owner and reload its state.
	 */
	PCPU_SET(fpcurproc, p);
	restorehighfp(p->p_addr->u_pcb.pcb_highfp);
	p->p_md.md_tf->tf_cr_ipsr &= ~IA64_PSR_DFH;

	p->p_md.md_flags |= MDP_FPUSED;
}
