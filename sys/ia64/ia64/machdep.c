/*-
 * Copyright (c) 2000,2001 Doug Rabson
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
#include "opt_ski.h"
#include "opt_msgbuf.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>
#include <sys/sysproto.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/pcpu.h>
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
#include <machine/clock.h>
#include <machine/md_var.h>
#include <machine/reg.h>
#include <machine/fpu.h>
#include <machine/pal.h>
#include <machine/sal.h>
#include <machine/bootinfo.h>
#include <machine/mutex.h>
#include <machine/vmparam.h>
#include <machine/elf.h>
#include <ddb/ddb.h>
#include <sys/vnode.h>
#include <sys/ucontext.h>
#include <machine/sigframe.h>
#include <machine/efi.h>
#include <machine/inst.h>
#include <machine/rse.h>
#include <machine/unwind.h>
#include <i386/include/specialreg.h>

#ifdef SKI
extern void ia64_ski_init(void);
#endif

u_int64_t processor_frequency;
u_int64_t bus_frequency;
u_int64_t itc_frequency;
int cold = 1;

u_int64_t pa_bootinfo;
u_int64_t va_bootinfo;
struct bootinfo bootinfo;
int bootinfo_error; /* XXX temporary ad-hoc error mask to help debugging */

extern char kstack[]; 
struct user *proc0uarea;
vm_offset_t proc0kstack;

extern u_int64_t kernel_text[], _end[];
extern u_int64_t _ia64_unwind_start[];
extern u_int64_t _ia64_unwind_end[];

FPSWA_INTERFACE *fpswa_interface;

u_int64_t ia64_pal_base;
u_int64_t ia64_port_base;

char machine[] = MACHINE;
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

static void cpu_startup(void *);
SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL)

struct msgbuf *msgbufp=0;

int Maxmem = 0;
int physmem;				/* Physical conventional memory. */

vm_offset_t phys_avail[100];

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

static void identifycpu(void);

struct kva_md_info kmi;

static void
cpu_startup(dummy)
	void *dummy;
{

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
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

	vm_ksubmap_init(&kmi);

	printf("avail memory = %ld (%ldK bytes)\n", ptoa(cnt.v_free_count),
	    ptoa(cnt.v_free_count) / 1024);
 
	if (fpswa_interface == NULL)
		printf("Warning: no FPSWA package supplied\n");
	else
		printf("FPSWA Revision = 0x%lx, Entry = %p\n",
		    (long)fpswa_interface->Revision,
		    (void *)fpswa_interface->Fpswa);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();
	vm_pager_bufferinit();

#ifndef SKI
	/*
	 * Traverse the MADT to discover IOSAPIC and Local SAPIC
	 * information.
	 */
	ia64_probe_sapics();
#endif
}

void
cpu_pcpu_init(struct pcpu *pcpu, int cpuid, size_t size)
{
	KASSERT(size >= sizeof(struct pcpu) + sizeof(struct pcb),
	    ("%s: too small an allocation for pcpu", __func__));
	pcpu->pc_pcb = (void*)(pcpu+1);
}

static void
identifycpu(void)
{
	char vendor[17];
	u_int64_t t;
	int number, revision, model, family, archrev;
	u_int64_t features;

	/*
	 * Assumes little-endian.
	 */
	*(u_int64_t *) &vendor[0] = ia64_get_cpuid(0);
	*(u_int64_t *) &vendor[8] = ia64_get_cpuid(1);
	vendor[16] = '\0';

	t = ia64_get_cpuid(3);
	number = (t >> 0) & 0xff;
	revision = (t >> 8) & 0xff;
	model = (t >> 16) & 0xff;
	family = (t >> 24) & 0xff;
	archrev = (t >> 32) & 0xff;

	if (family == 0x7)
		strcpy(cpu_model, "Itanium");
	else if (family == 0x1f)
		strcpy(cpu_model, "McKinley");
	else
		snprintf(cpu_model, sizeof(cpu_model), "Family=%d", family);

	features = ia64_get_cpuid(4);

	printf("CPU: %s", cpu_model);
	if (processor_frequency)
		printf(" (%ld.%02ld-Mhz)\n",
		       (processor_frequency + 4999) / 1000000,
		       ((processor_frequency + 4999) / 10000) % 100);
	else
		printf("\n");
	printf("  Origin = \"%s\"  Model = %d  Revision = %d\n",
	       vendor, model, revision);
	printf("  Features = 0x%b\n", (u_int32_t) features,
	       "\020"
	       "\001LB");
}

static void
add_kernel_unwind_tables(void *arg)
{
	/*
	 * Register the kernel's unwind table.
	 */
	ia64_add_unwind_table(kernel_text,
			      _ia64_unwind_start,
			      _ia64_unwind_end);
}
SYSINIT(unwind, SI_SUB_KMEM, SI_ORDER_ANY, add_kernel_unwind_tables, 0);

void
map_pal_code(void)
{
	struct ia64_pte pte;
	u_int64_t psr;

	if (ia64_pal_base == 0)
		return;

	bzero(&pte, sizeof(pte));
	pte.pte_p = 1;
	pte.pte_ma = PTE_MA_WB;
	pte.pte_a = 1;
	pte.pte_d = 1;
	pte.pte_pl = PTE_PL_KERN;
	pte.pte_ar = PTE_AR_RWX;
	pte.pte_ppn = ia64_pal_base >> 12;

	__asm __volatile("mov %0=psr;;" : "=r" (psr));
	__asm __volatile("rsm psr.ic|psr.i;; srlz.i;;");
	__asm __volatile("mov cr.ifa=%0" ::
	    "r"(IA64_PHYS_TO_RR7(ia64_pal_base)));
	__asm __volatile("mov cr.itir=%0" :: "r"(28 << 2));
	__asm __volatile("srlz.i;;");
	__asm __volatile("itr.i itr[%0]=%1;;" ::
	    "r"(2), "r"(*(u_int64_t*)&pte));
	__asm __volatile("srlz.i;;");
	__asm __volatile("mov psr.l=%0;; srlz.i;;" :: "r" (psr));
}

static void
calculate_frequencies(void)
{
	struct ia64_sal_result sal;
	struct ia64_pal_result pal;

	sal = ia64_sal_entry(SAL_FREQ_BASE, 0, 0, 0, 0, 0, 0, 0);
	pal = ia64_call_pal_static(PAL_FREQ_RATIOS, 0, 0, 0);

	if (sal.sal_status == 0 && pal.pal_status == 0) {
		if (bootverbose) {
			printf("Platform clock frequency %ld Hz\n",
			       sal.sal_result[0]);
			printf("Processor ratio %ld/%ld, Bus ratio %ld/%ld, "
			       "ITC ratio %ld/%ld\n",
			       pal.pal_result[0] >> 32,
			       pal.pal_result[0] & ((1L << 32) - 1),
			       pal.pal_result[1] >> 32,
			       pal.pal_result[1] & ((1L << 32) - 1),
			       pal.pal_result[2] >> 32,
			       pal.pal_result[2] & ((1L << 32) - 1));
		}
		processor_frequency =
			sal.sal_result[0] * (pal.pal_result[0] >> 32)
			/ (pal.pal_result[0] & ((1L << 32) - 1));
		bus_frequency =
			sal.sal_result[0] * (pal.pal_result[1] >> 32)
			/ (pal.pal_result[1] & ((1L << 32) - 1));
		itc_frequency =
			sal.sal_result[0] * (pal.pal_result[2] >> 32)
			/ (pal.pal_result[2] & ((1L << 32) - 1));
	}
}

void
ia64_init(u_int64_t arg1, u_int64_t arg2)
{
	int phys_avail_cnt;
	vm_offset_t kernstart, kernend;
	vm_offset_t kernstartpfn, kernendpfn, pfn0, pfn1;
	char *p;
	EFI_MEMORY_DESCRIPTOR *md, *mdp;
	int mdcount, i;

	/* NO OUTPUT ALLOWED UNTIL FURTHER NOTICE */

	/*
	 * TODO: Disable interrupts, floating point etc.
	 * Maybe flush cache and tlb
	 */
	ia64_set_fpsr(IA64_FPSR_DEFAULT);

	/*
	 * TODO: Get critical system information (if possible, from the
	 * information provided by the boot program).
	 */

	/*
	 * Gross and disgusting hack. The bootinfo is written into
	 * memory at a fixed address.
	 * To help transitioning to a non-fixed bootinfo block, we
	 * temporarily have to make this more gross and disgusting:
	 * o  pa_bootinfo is the physical address of the bootinfo block
	 *    as passed to us by the loader (initialized in locore.s)
	 *    (EFI loader version 0.3 and up). We only check this value.
	 *    We don't actively use it yet.
	 * o  va_bootinfo is the hardwired virtual (RR7) address of
	 *    the bootinfo block (old loaders). We still use it for the
	 *    moment.
	 */
	va_bootinfo = 0xe000000000508000; 	/* the fixed RR7 address */
	if (IA64_PHYS_TO_RR7(pa_bootinfo) != va_bootinfo)
		bootinfo_error |= 1;		/* XXX loader did not set r8 */

	/* copy the bootinfo block */
	bootinfo = *(struct bootinfo *)va_bootinfo;

	if (bootinfo.bi_magic != BOOTINFO_MAGIC || bootinfo.bi_version != 1) {
		bootinfo_error |= 2;		/* XXX bogus block */
		bzero(&bootinfo, sizeof(bootinfo));
		bootinfo.bi_kernend = (vm_offset_t) round_page(_end);
	}

	/*
	 * Look for the I/O ports first - we need them for console
	 * probing.
	 */
	mdcount = bootinfo.bi_memmap_size / bootinfo.bi_memdesc_size;
	md = (EFI_MEMORY_DESCRIPTOR *) IA64_PHYS_TO_RR7(bootinfo.bi_memmap);
	if (md == NULL || mdcount == 0) {
#ifdef SKI
		static EFI_MEMORY_DESCRIPTOR ski_md[2];
		/*
		 * XXX hack for ski. In reality, the loader will probably ask
		 * EFI and pass the results to us. Possibly, we will call EFI
		 * directly.
		 */
		ski_md[0].Type = EfiConventionalMemory;
		ski_md[0].PhysicalStart = 2L*1024*1024;
		ski_md[0].VirtualStart = 0;
		ski_md[0].NumberOfPages = (64L*1024*1024)>>12;
		ski_md[0].Attribute = EFI_MEMORY_WB;

		ski_md[1].Type = EfiMemoryMappedIOPortSpace;
		ski_md[1].PhysicalStart = 0xffffc000000;
		ski_md[1].VirtualStart = 0;
		ski_md[1].NumberOfPages = (64L*1024*1024)>>12;
		ski_md[1].Attribute = EFI_MEMORY_UC;

		md = ski_md;
		mdcount = 2;
#endif
	}

	for (i = 0, mdp = md; i < mdcount; i++,
	    mdp = NextMemoryDescriptor(mdp, bootinfo.bi_memdesc_size)) {
		if (mdp->Type == EfiMemoryMappedIOPortSpace)
			ia64_port_base = IA64_PHYS_TO_RR6(mdp->PhysicalStart);
		else if (mdp->Type == EfiPalCode)
			ia64_pal_base = mdp->PhysicalStart;
	}

	KASSERT(ia64_port_base != 0,
	    ("%s: no I/O memory region", __func__));

	/*
	 * Look at arguments passed to us and compute boothowto.
	 */
	boothowto = bootinfo.bi_boothowto;
#ifdef KADB
	boothowto |= RB_KDB;
#endif

	/*
	 * Catch case of boot_verbose set in environment.
	 */
	if ((p = getenv("boot_verbose")) != NULL) {
		if (strcmp(p, "yes") == 0 || strcmp(p, "YES") == 0) {
			boothowto |= RB_VERBOSE;
		}
	}

	if (boothowto & RB_VERBOSE)
		bootverbose = 1;

	/*
	 * Initialize the console before we print anything out.
	 */
	cninit();

	/* OUTPUT NOW ALLOWED */

	if (bootinfo_error & 1)
		printf("bootinfo: the loader did not not pass the address "
		    "of the block in r8.\n");

	if (bootinfo_error & 2)
		printf("bootinfo: block not valid; possibly not at hardwired "
		    "address.\n");

	if (ia64_pal_base != 0) {
		ia64_pal_base &= ~((1 << 28) - 1);
		/*
		 * We use a TR to map the first 256M of memory - this might
		 * cover the palcode too.
		 */
		if (ia64_pal_base == 0)
			printf("PAL code mapped by the kernel's TR\n");
	} else
		printf("PAL code not found\n");

	/*
	 * Wire things up so we can call the firmware.
	 */
	map_pal_code();
	ia64_efi_init();
#ifdef SKI
	ia64_ski_init();
#endif
	calculate_frequencies();

	/*
	 * Find the beginning and end of the kernel.
	 */
	kernstart = trunc_page(kernel_text);
	ksym_start = (void *)bootinfo.bi_symtab;
	ksym_end   = (void *)bootinfo.bi_esymtab;
	kernend = (vm_offset_t)round_page(ksym_end);
	/* But if the bootstrap tells us otherwise, believe it! */
	if (bootinfo.bi_kernend)
		kernend = round_page(bootinfo.bi_kernend);
	preload_metadata = (caddr_t)bootinfo.bi_modulep;
	if (envmode == 1)
		kern_envp = static_env;
	else
		kern_envp = (caddr_t)bootinfo.bi_envp;

	/* get fpswa interface */
	fpswa_interface = (FPSWA_INTERFACE*)IA64_PHYS_TO_RR7(bootinfo.bi_fpswa);

	/* Init basic tunables, including hz */
	init_param1();

	p = getenv("kernelname");
	if (p)
		strncpy(kernelname, p, sizeof(kernelname) - 1);

	kernstartpfn = atop(IA64_RR_MASK(kernstart));
	kernendpfn = atop(IA64_RR_MASK(kernend));

	/*
	 * Size the memory regions and load phys_avail[] with the results.
	 */

	/*
	 * Find out how much memory is available, by looking at
	 * the memory descriptors.
	 */

#ifdef DEBUG_MD
	printf("Memory descriptor count: %d\n", mdcount);
#endif

	phys_avail_cnt = 0;
	for (i = 0, mdp = md; i < mdcount; i++,
		 mdp = NextMemoryDescriptor(mdp, bootinfo.bi_memdesc_size)) {
#ifdef DEBUG_MD
		printf("MD %d: type %d pa 0x%lx cnt 0x%lx\n", i,
		       mdp->Type,
		       mdp->PhysicalStart,
		       mdp->NumberOfPages);
#endif

		pfn0 = ia64_btop(round_page(mdp->PhysicalStart));
		pfn1 = ia64_btop(trunc_page(mdp->PhysicalStart
					    + mdp->NumberOfPages * 4096));
		if (pfn1 <= pfn0)
			continue;

		if (mdp->Type != EfiConventionalMemory)
			continue;

		/*
		 * We have a memory descriptor that describes conventional
		 * memory that is for general use. We must determine if the
		 * loader has put the kernel in this region.
		 */
		physmem += (pfn1 - pfn0);
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
	init_param2(physmem);

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
		if (phys_avail[i] == phys_avail[i+1]) {
			phys_avail[i] = 0;
			phys_avail[i+1] = 0;
		}

		/* warn if the message buffer had to be shrunk */
		if (sz != round_page(MSGBUF_SIZE))
			printf("WARNING: %ld bytes not available for msgbuf in last cluster (%ld used)\n",
			    round_page(MSGBUF_SIZE), sz);

	}

	proc_linkup(&proc0, &proc0.p_ksegrp, &proc0.p_kse, &thread0);
	/*
	 * Init mapping for u page(s) for proc 0
	 */
	proc0uarea = (struct user *)pmap_steal_memory(UAREA_PAGES * PAGE_SIZE);
	proc0kstack = (vm_offset_t)kstack;
	proc0.p_uarea = proc0uarea;
	thread0.td_kstack = proc0kstack;
	thread0.td_pcb = (struct pcb *)
	    (thread0.td_kstack + KSTACK_PAGES * PAGE_SIZE) - 1;
	/*
	 * Setup the global data for the bootstrap cpu.
	 */
	pcpup = (struct pcpu *) pmap_steal_memory(PAGE_SIZE);
	pcpu_init(pcpup, 0, PAGE_SIZE);
	ia64_set_k4((u_int64_t) pcpup);
	PCPU_SET(curthread, &thread0);

	/*
	 * Set ia32 control registers.
	 */
	ia64_set_cflg((CR0_PE | CR0_PG)
		      | ((long)(CR4_XMM | CR4_FXSR) << 32));

	/* We pretend to own FP state so that ia64_fpstate_check() works */
	PCPU_SET(fpcurthread, &thread0);

	/*
	 * Initialize the rest of proc 0's PCB.
	 *
	 * Set the kernel sp, reserving space for an (empty) trapframe,
	 * and make proc0's trapframe pointer point to it for sanity.
	 * Initialise proc0's backing store to start after u area.
	 *
	 * XXX what is all this +/- 16 stuff?
	 */
	thread0.td_frame = (struct trapframe *)thread0.td_pcb - 1;
	thread0.td_pcb->pcb_sp = (u_int64_t)thread0.td_frame - 16;
	thread0.td_pcb->pcb_bspstore = (u_int64_t)proc0kstack;

	mutex_init();

	/*
	 * Initialize the virtual memory system.
	 */
	pmap_bootstrap();
	pcpup->pc_current_pmap = kernel_pmap;

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

int
ia64_running_in_simulator()
{
	return bootinfo.bi_systab == 0;
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
	u_int64_t start, end, now;

	start = ia64_get_itc();
	end = start + (itc_frequency * n) / 1000000;
	/* printf("DELAY from 0x%lx to 0x%lx\n", start, end); */
	do {
		now = ia64_get_itc();
	} while (now < end || (now > start && end < start));
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
	struct proc *p;
	struct thread *td;
	struct trapframe *frame;
	struct sigacts *psp;
	struct sigframe sf, *sfp;
	u_int64_t sbs = 0;
	int oonstack, rndfsize;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	psp = p->p_sigacts;
	frame = td->td_frame;
	oonstack = sigonstack(frame->tf_r[FRAME_SP]);
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
	sf.sf_uc.uc_stack.ss_flags = (p->p_flag & P_ALTSTACK)
	    ? ((oonstack) ? SS_ONSTACK : 0) : SS_DISABLE;
	sf.sf_uc.uc_mcontext.mc_flags = IA64_MC_FLAG_ONSTACK;
	sf.sf_uc.uc_mcontext.mc_onstack = (oonstack) ? 1 : 0;

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
		/*
		 * Align sp and bsp.
		 */
		sbs = (sbs + 15) & ~15;
		sfp = (struct sigframe *)((u_int64_t)sfp & ~15);
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
		p->p_sigstk.ss_flags |= SS_ONSTACK;
#endif
	} else
		sfp = (struct sigframe *)(frame->tf_r[FRAME_SP] - rndfsize);
	PROC_UNLOCK(p);

#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sendsig(%d): sig %d ssp %p usp %p\n", p->p_pid,
		       sig, &sf, sfp);
#endif

#if 0
	/* save the floating-point state, if necessary, then copy it. */
	ia64_fpstate_save(td, 1);
	sf.sf_uc.uc_mcontext.mc_ownedfp = td->td_md.md_flags & MDP_FPUSED;
	bcopy(&td->td_pcb->pcb_fp,
	      (struct fpreg *)sf.sf_uc.uc_mcontext.mc_fpregs,
	      sizeof(struct fpreg));
	sf.sf_uc.uc_mcontext.mc_fp_control = td->td_pcb.pcb_fp_control;
#endif

	/*
	 * copy the frame out to userland.
	 */
	if (copyout((caddr_t)&sf, (caddr_t)sfp, sizeof(sf)) != 0) {
#ifdef DEBUG
		if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
			printf("sendsig(%d): copyout failed on sig %d\n",
			       p->p_pid, sig);
#endif
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		PROC_LOCK(p);
		SIGACTION(p, SIGILL) = SIG_DFL;
		SIGDELSET(p->p_sigignore, SIGILL);
		SIGDELSET(p->p_sigcatch, SIGILL);
		SIGDELSET(p->p_sigmask, SIGILL);
		psignal(p, SIGILL);
		return;
	}
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
	PROC_LOCK(p);
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
 * Stub to satisfy the reference to osigreturn in the syscall table.  This
 * is needed even for newer arches that don't support old signals because
 * the syscall table is machine-independent.
 */
int
osigreturn(struct thread *td, struct osigreturn_args *uap)
{

	return (nosys(td, (struct nosys_args *)uap));
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above).
 * Return to previous pc and psl as specified by
 * context left by sendsig. Check carefully to
 * make sure that the user has not modified the
 * state to gain improper privileges.
 *
 * MPSAFE
 */
int
sigreturn(struct thread *td,
	struct sigreturn_args /* {
		ucontext_t *sigcntxp;
	} */ *uap)
{
	ucontext_t uc;
	const ucontext_t *ucp;
	struct pcb *pcb;
	struct trapframe *frame = td->td_frame;
	struct __mcontext *mcp;
	struct proc *p;

	ucp = uap->sigcntxp;
	pcb = td->td_pcb;
	p = td->td_proc;

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

	if (frame->tf_ndirty != 0) {
	    printf("sigreturn: dirty user stacked registers\n");
	}

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

	PROC_LOCK(p);
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	if (uc.uc_mcontext.mc_onstack & 1)
		p->p_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigstk.ss_flags &= ~SS_ONSTACK;
#endif

	p->p_sigmask = uc.uc_sigmask;
	SIG_CANTMASK(p->p_sigmask);
	signotify(p);
	PROC_UNLOCK(p);

	/* XXX ksc.sc_ownedfp ? */
	ia64_fpstate_drop(td);
#if 0
	bcopy((struct fpreg *)uc.uc_mcontext.mc_fpregs,
	      &td->td_pcb->pcb_fp, sizeof(struct fpreg));
	td->td_pcb->pcb_fp_control = uc.uc_mcontext.mc_fp_control;
#endif

#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sigreturn(%d): returns\n", p->p_pid);
#endif
	return (EJUSTRETURN);
}

/*
 * Machine dependent boot() routine
 */
void
cpu_boot(int howto)
{

	ia64_efi_runtime->ResetSystem(EfiResetWarm, EFI_SUCCESS, 0, 0);
}

/*
 * Shutdown the CPU as much as possible
 */
void
cpu_halt(void)
{

	ia64_efi_runtime->ResetSystem(EfiResetWarm, EFI_SUCCESS, 0, 0);
}

/*
 * Clear registers on exec
 */
void
setregs(struct thread *td, u_long entry, u_long stack, u_long ps_strings)
{
	struct trapframe *frame;

	frame = td->td_frame;

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
	/*
	 * Make sure that sp is aligned to a 16 byte boundary and
	 * reserve 16 bytes of scratch space for _start.
	 */
	frame->tf_r[FRAME_SP] = (stack & ~15) - 16;

	/*
	 * Write values for out0, out1 and out2 to the user's backing
	 * store and arrange for them to be restored into the user's
	 * initial register frame. Assumes that (bspstore & 0x1f8) <
	 * 0x1e0.
	 */
	frame->tf_ar_bspstore = td->td_md.md_bspstore + 24;
	suword((caddr_t) frame->tf_ar_bspstore - 24, stack);
	suword((caddr_t) frame->tf_ar_bspstore - 16, ps_strings);
	suword((caddr_t) frame->tf_ar_bspstore -  8, 0);
	frame->tf_ndirty = 0;
	frame->tf_cr_ifs = (1L<<63) | 3; /* sof=3, v=1 */

	frame->tf_ar_rsc = 0xf;	/* user mode rsc */
	frame->tf_ar_fpsr = IA64_FPSR_DEFAULT;

	td->td_md.md_flags &= ~MDP_FPUSED;
	ia64_fpstate_drop(td);
}

int
ptrace_set_pc(struct thread *td, unsigned long addr)
{
	/* TODO set pc in trapframe */
	return 0;
}

int
ptrace_single_step(struct thread *td)
{
	/* TODO arrange for user process to single step */
	return 0;
}

int
ia64_pa_access(vm_offset_t pa)
{
	return VM_PROT_READ|VM_PROT_WRITE;
}

int
fill_regs(td, regs)
	struct thread *td;
	struct reg *regs;
{
	/* TODO copy trapframe to regs */
	return (0);
}

int
set_regs(td, regs)
	struct thread *td;
	struct reg *regs;
{
	/* TODO copy regs to trapframe */
	return (0);
}

int
fill_dbregs(struct thread *td, struct dbreg *dbregs)
{

	return (ENOSYS);
}

int
set_dbregs(struct thread *td, struct dbreg *dbregs)
{

	return (ENOSYS);
}

int
fill_fpregs(td, fpregs)
	struct thread *td;
	struct fpreg *fpregs;
{
	/* TODO copy fpu state to fpregs */
	ia64_fpstate_save(td, 0);

#if 0
	bcopy(&td->td_pcb->pcb_fp, fpregs, sizeof *fpregs);
#endif
	return (0);
}

int
set_fpregs(td, fpregs)
	struct thread *td;
	struct fpreg *fpregs;
{
	/* TODO copy fpregs fpu state */
	ia64_fpstate_drop(td);

#if 0
	bcopy(fpregs, &td->td_pcb->pcb_fp, sizeof *fpregs);
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
#endif
        bp->bio_flags |= BIO_ERROR;
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
ia64_fpstate_check(struct thread *td)
{
	if ((td->td_frame->tf_cr_ipsr & IA64_PSR_DFH) == 0)
		if (td != PCPU_GET(fpcurthread))
			panic("ia64_fpstate_check: bogus");
}

/*
 * Save the high floating point state in the pcb. Use this to get
 * read-only access to the floating point state. If write is true, the
 * current fp process is cleared so that fp state can safely be
 * modified. The process will automatically reload the changed state
 * by generating a disabled fp trap.
 */
void
ia64_fpstate_save(struct thread *td, int write)
{
	if (td == PCPU_GET(fpcurthread)) {
		/*
		 * Save the state in the pcb.
		 */
		savehighfp(td->td_pcb->pcb_highfp);

		if (write) {
			td->td_frame->tf_cr_ipsr |= IA64_PSR_DFH;
			PCPU_SET(fpcurthread, NULL);
		}
	}
}

/*
 * Relinquish ownership of the FP state. This is called instead of
 * ia64_save_fpstate() if the entire FP state is being changed
 * (e.g. on sigreturn).
 */
void
ia64_fpstate_drop(struct thread *td)
{
	if (td == PCPU_GET(fpcurthread)) {
		td->td_frame->tf_cr_ipsr |= IA64_PSR_DFH;
		PCPU_SET(fpcurthread, NULL);
	}
}

/*
 * Switch the current owner of the fp state to p, reloading the state
 * from the pcb.
 */
void
ia64_fpstate_switch(struct thread *td)
{
	if (PCPU_GET(fpcurthread)) {
		/*
		 * Dump the old fp state if its valid.
		 */
		savehighfp(PCPU_GET(fpcurthread)->td_pcb->pcb_highfp);
		PCPU_GET(fpcurthread)->td_frame->tf_cr_ipsr |= IA64_PSR_DFH;
	}

	/*
	 * Remember the new FP owner and reload its state.
	 */
	PCPU_SET(fpcurthread, td);
	restorehighfp(td->td_pcb->pcb_highfp);
	td->td_frame->tf_cr_ipsr &= ~IA64_PSR_DFH;

	td->td_md.md_flags |= MDP_FPUSED;
}

/*
 * Utility functions for manipulating instruction bundles.
 */
void
ia64_unpack_bundle(u_int64_t low, u_int64_t high, struct ia64_bundle *bp)
{
	bp->template = low & 0x1f;
	bp->slot[0] = (low >> 5) & ((1L<<41) - 1);
	bp->slot[1] = (low >> 46) | ((high & ((1L<<23) - 1)) << 18);
	bp->slot[2] = (high >> 23);
}

void
ia64_pack_bundle(u_int64_t *lowp, u_int64_t *highp,
		 const struct ia64_bundle *bp)
{
	u_int64_t low, high;

	low = bp->template | (bp->slot[0] << 5) | (bp->slot[1] << 46);
	high = (bp->slot[1] >> 18) | (bp->slot[2] << 23);
	*lowp = low;
	*highp = high;
}

static int
rse_slot(u_int64_t *bsp)
{
	return ((u_int64_t) bsp >> 3) & 0x3f;
}

/*
 * Return the address of register regno (regno >= 32) given that bsp
 * points at the base of the register stack frame.
 */
u_int64_t *
ia64_rse_register_address(u_int64_t *bsp, int regno)
{
	int off = regno - 32;
	u_int64_t rnats = (rse_slot(bsp) + off) / 63;
	return bsp + off + rnats;
}

/*
 * Calculate the base address of the previous frame given that the
 * current frame's locals area is 'size'.
 */
u_int64_t *
ia64_rse_previous_frame(u_int64_t *bsp, int size)
{
	int slot = rse_slot(bsp);
	int rnats = 0;
	int count = size;

	while (count > slot) {
		count -= 63;
		rnats++;
		slot = 63;
	}
	return bsp - size - rnats;
}

