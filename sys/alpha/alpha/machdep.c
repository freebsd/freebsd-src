/*-
 * Copyright (c) 1998 Doug Rabson
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
/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center and by Chris G. Demetriou.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include "opt_compat.h"
#include "opt_ddb.h"
#include "opt_msgbuf.h"
#include "opt_maxmem.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>
#include <sys/imgact.h>
#include <sys/sysproto.h>
#include <sys/ktr.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/malloc.h>
#include <sys/reboot.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <sys/mbuf.h>
#include <sys/vmmeter.h>
#include <sys/msgbuf.h>
#include <sys/exec.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/linker.h>
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
#include <sys/ucontext.h>
#include <machine/clock.h>
#include <machine/md_var.h>
#include <machine/fpu.h>
#include <machine/pal.h>
#include <machine/cpuconf.h>
#include <machine/bootinfo.h>
#include <machine/rpb.h>
#include <machine/prom.h>
#include <machine/chipset.h>
#include <machine/vmparam.h>
#include <machine/elf.h>
#include <ddb/ddb.h>
#include <alpha/alpha/db_instruction.h>
#include <sys/vnode.h>
#include <machine/sigframe.h>


u_int64_t cycles_per_usec;
u_int32_t cycles_per_sec;
int cold = 1;
struct platform platform;
alpha_chipset_t chipset;
struct bootinfo_kernel bootinfo;

struct mtx icu_lock;

struct	user *proc0uarea;
vm_offset_t proc0kstack;

char machine[] = "alpha";
SYSCTL_STRING(_hw, HW_MACHINE, machine, CTLFLAG_RD, machine, 0, "");

static char cpu_model[128];
SYSCTL_STRING(_hw, HW_MODEL, model, CTLFLAG_RD, cpu_model, 0, "");

#ifdef DDB
/* start and end of kernel symbol table */
void	*ksym_start, *ksym_end;
db_regs_t	ddb_regs;
#endif

int	alpha_unaligned_print = 1;	/* warn about unaligned accesses */
int	alpha_unaligned_fix = 1;	/* fix up unaligned accesses */
int	alpha_unaligned_sigbus = 0;	/* don't SIGBUS on fixed-up accesses */

SYSCTL_INT(_machdep, CPU_UNALIGNED_PRINT, unaligned_print,
	CTLFLAG_RW, &alpha_unaligned_print, 0, "");

SYSCTL_INT(_machdep, CPU_UNALIGNED_FIX, unaligned_fix,
	CTLFLAG_RW, &alpha_unaligned_fix, 0, "");

SYSCTL_INT(_machdep, CPU_UNALIGNED_SIGBUS, unaligned_sigbus,
	CTLFLAG_RW, &alpha_unaligned_sigbus, 0, "");

static void cpu_startup(void *);
SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL)

struct msgbuf *msgbufp=0;

long Maxmem = 0;

long	totalphysmem;		/* total amount of physical memory in system */
long	resvmem;		/* amount of memory reserved for PROM */
long	unusedmem;		/* amount of memory for OS that we don't use */
long	unknownmem;		/* amount of memory with an unknown use */
int	ncpus;			/* number of cpus */

vm_offset_t phys_avail[10];

/* must be 2 less so 0 0 can signal end of chunks */
#define PHYS_AVAIL_ARRAY_END ((sizeof(phys_avail) / sizeof(vm_offset_t)) - 2)

#ifdef COMPAT_43
void osendsig(sig_t catcher, int sig, sigset_t *mask, u_long code);
#endif
#ifdef COMPAT_FREEBSD4
static void freebsd4_sendsig(sig_t catcher, int sig, sigset_t *mask,
    u_long code);
#endif

static void get_fpcontext(struct thread *td, mcontext_t *mcp);
static void identifycpu(void);
static int  set_fpcontext(struct thread *td, const mcontext_t *mcp);

struct kva_md_info kmi;

/*
 * Hooked into the shutdown chain; if the system is to be halted,
 * unconditionally drop back to the SRM console.
 */
static void
alpha_srm_shutdown(void *junk, int howto)
{
	if (howto & RB_HALT) {
		cpu_halt();
	}
}

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
	printf("real memory  = %ld (%ld MB)\n", alpha_ptob(Maxmem),
	    alpha_ptob(Maxmem) / 1048576);

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

	printf("avail memory = %ld (%ld MB)\n", ptoa(cnt.v_free_count),
	    ptoa(cnt.v_free_count) / 1048576);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();
	vm_pager_bufferinit();
	EVENTHANDLER_REGISTER(shutdown_final, alpha_srm_shutdown, 0,
			      SHUTDOWN_PRI_LAST);
}

/*
 * Retrieve the platform name from the DSR.
 */
const char *
alpha_dsr_sysname()
{
	struct dsrdb *dsr;
	const char *sysname;

	/*
	 * DSR does not exist on early HWRPB versions.
	 */
	if (hwrpb->rpb_version < HWRPB_DSRDB_MINVERS)
		return (NULL);

	dsr = (struct dsrdb *)(((caddr_t)hwrpb) + hwrpb->rpb_dsrdb_off);
	sysname = (const char *)((caddr_t)dsr + (dsr->dsr_sysname_off +
	    sizeof(u_int64_t)));
	return (sysname);
}

/*
 * Lookup the system specified system variation in the provided table,
 * returning the model string on match.
 */
const char *
alpha_variation_name(u_int64_t variation,
		     const struct alpha_variation_table *avtp)
{
	int i;

	for (i = 0; avtp[i].avt_model != NULL; i++)
		if (avtp[i].avt_variation == variation)
			return (avtp[i].avt_model);
	return (NULL);
}

/*
 * Generate a default platform name based for unknown system variations.
 */
const char *
alpha_unknown_sysname()
{
	static char s[128];		/* safe size */

	snprintf(s, sizeof(s), "%s family, unknown model variation 0x%lx",
	    platform.family, hwrpb->rpb_variation & SV_ST_MASK);
	return ((const char *)s);
}

static void
identifycpu(void)
{
	u_int64_t type, major, minor;
	u_int64_t amask;
	struct pcs *pcsp;
	char *cpuname[] = {
		"unknown",		/* 0 */
		"EV3",			/* 1 */
		"EV4 (21064)",		/* 2 */
		"Simulation",		/* 3 */
		"LCA Family",		/* 4 */
		"EV5 (21164)",		/* 5 */
		"EV45 (21064A)",	/* 6 */
		"EV56 (21164A)",	/* 7 */
		"EV6 (21264)",		/* 8 */
		"PCA56 (21164PC)",	/* 9 */
		"PCA57 (21164PC)",	/* 10 */
		"EV67 (21264A)",	/* 11 */
		"EV68CB (21264C)"	/* 12 */
		"EV68AL (21264B)",	/* 13 */
		"EV68CX (21264D)"	/* 14 */
	};

	/*
	 * print out CPU identification information.
	 */
	printf("%s\n%s, %ldMHz\n", platform.family, platform.model,
	    hwrpb->rpb_cc_freq / 1000000);	/* XXX true for 21164? */
	printf("%ld byte page size, %d processor%s.\n",
	    hwrpb->rpb_page_size, ncpus, ncpus == 1 ? "" : "s");
#if 0
	/* this isn't defined for any systems that we run on? */
	printf("serial number 0x%lx 0x%lx\n",
	    ((long *)hwrpb->rpb_ssn)[0], ((long *)hwrpb->rpb_ssn)[1]);

	/* and these aren't particularly useful! */
	printf("variation: 0x%lx, revision 0x%lx\n",
	    hwrpb->rpb_variation, *(long *)hwrpb->rpb_revision);
#endif
 	pcsp = LOCATE_PCS(hwrpb, hwrpb->rpb_primary_cpu_id);
	/* cpu type */
	type = pcsp->pcs_proc_type;
	major = (type & PCS_PROC_MAJOR) >> PCS_PROC_MAJORSHIFT;
	minor = (type & PCS_PROC_MINOR) >> PCS_PROC_MINORSHIFT;
	if (major < sizeof(cpuname)/sizeof(char *))
		printf("CPU: %s major=%lu minor=%lu",
			cpuname[major], major, minor);
	else
		printf("CPU: major=%lu minor=%lu\n", major, minor);
	/* amask */
	if (major >= PCS_PROC_EV56) {
		amask = 0xffffffff; /* 32 bit for printf */
		amask = (~alpha_amask(amask)) & amask;
		printf(" extensions=0x%b\n", (u_int32_t) amask,
			"\020"
			"\001BWX"
			"\002FIX"
			"\003CIX"
			"\011MVI"
			"\012PRECISE"
		);
	} else
		printf("\n");	
	/* PAL code */
	printf("OSF PAL rev: 0x%lx\n", pcsp->pcs_palrevisions[PALvar_OSF1]);
}

extern char kernel_text[], _end[];

void
alpha_init(pfn, ptb, bim, bip, biv)
	u_long pfn;		/* first free PFN number */
	u_long ptb;		/* PFN of current level 1 page table */
	u_long bim;		/* bootinfo magic */
	u_long bip;		/* bootinfo pointer */
	u_long biv;		/* bootinfo version */
{
	int phys_avail_cnt;
	char *bootinfo_msg, *bootinfo_booted_kernel;
	vm_offset_t kernstart, kernend;
	vm_offset_t kernstartpfn, kernendpfn, pfn0, pfn1;
	struct mddt *mddtp;
	struct mddt_cluster *memc;
	int i, mddtweird;
	int cputype;
	char *p;

	/* NO OUTPUT ALLOWED UNTIL FURTHER NOTICE */

	/*
	 * Turn off interrupts (not mchecks) and floating point.
	 * Make sure the instruction and data streams are consistent.
	 */
	(void)alpha_pal_swpipl(ALPHA_PSL_IPL_HIGH);
	/* alpha_pal_wrfen(0); */
	ALPHA_TBIA();
	alpha_pal_imb();

	/*
	 * Get critical system information (if possible, from the
	 * information provided by the boot program).
	 */
	bootinfo_msg = NULL;
	bootinfo_booted_kernel = NULL;
	if (bim == BOOTINFO_MAGIC) {
		if (biv == 0) {		/* backward compat */
			biv = *(u_long *)bip;
			bip += 8;
		}
		switch (biv) {
		case 1: {
			struct bootinfo_v1 *v1p = (struct bootinfo_v1 *)bip;

			bootinfo.ssym = v1p->ssym;
			bootinfo.esym = v1p->esym;
			bootinfo.kernend = v1p->kernend;
			bootinfo.modptr = v1p->modptr;
			bootinfo.envp = v1p->envp;
			/* hwrpb may not be provided by boot block in v1 */
			if (v1p->hwrpb != NULL) {
				bootinfo.hwrpb_phys =
				    ((struct rpb *)v1p->hwrpb)->rpb_phys;
				bootinfo.hwrpb_size = v1p->hwrpbsize;
			} else {
				bootinfo.hwrpb_phys =
				    ((struct rpb *)HWRPB_ADDR)->rpb_phys;
				bootinfo.hwrpb_size =
				    ((struct rpb *)HWRPB_ADDR)->rpb_size;
			}
			bcopy(v1p->boot_flags, bootinfo.boot_flags,
			    min(sizeof v1p->boot_flags,
			      sizeof bootinfo.boot_flags));
			bcopy(v1p->booted_kernel, bootinfo.booted_kernel,
			    min(sizeof v1p->booted_kernel,
			      sizeof bootinfo.booted_kernel));
			bootinfo_booted_kernel = bootinfo.booted_kernel;
			/* booted dev not provided in bootinfo */
			init_prom_interface((struct rpb *)
			    ALPHA_PHYS_TO_K0SEG(bootinfo.hwrpb_phys));
                	prom_getenv(PROM_E_BOOTED_DEV, bootinfo.booted_dev,
			    sizeof bootinfo.booted_dev);
			break;
		}
		default:
			bootinfo_msg = "unknown bootinfo version";
			goto nobootinfo;
		}
	} else {
		bootinfo_msg = "boot program did not pass bootinfo";
	nobootinfo:
		bootinfo.ssym = (u_long)&_end;
		bootinfo.esym = (u_long)&_end;
		bootinfo.hwrpb_phys = ((struct rpb *)HWRPB_ADDR)->rpb_phys;
		bootinfo.hwrpb_size = ((struct rpb *)HWRPB_ADDR)->rpb_size;
		init_prom_interface((struct rpb *)HWRPB_ADDR);
		prom_getenv(PROM_E_BOOTED_OSFLAGS, bootinfo.boot_flags,
			    sizeof bootinfo.boot_flags);
		prom_getenv(PROM_E_BOOTED_FILE, bootinfo.booted_kernel,
			    sizeof bootinfo.booted_kernel);
		prom_getenv(PROM_E_BOOTED_DEV, bootinfo.booted_dev,
			    sizeof bootinfo.booted_dev);
	}

	/*
	 * Initialize the kernel's mapping of the RPB.  It's needed for
	 * lots of things.
	 */
	hwrpb = (struct rpb *)ALPHA_PHYS_TO_K0SEG(bootinfo.hwrpb_phys);

	/*
	 * Remember how many cycles there are per microsecond, 
	 * so that we can use delay().  Round up, for safety.
	 */
	cycles_per_usec = (hwrpb->rpb_cc_freq + 999999) / 1000000;

	/*
	 * Remember how many cycles per closk for coping with missed
	 * clock interrupts.
	 */
	cycles_per_sec = hwrpb->rpb_cc_freq;

	/* Get the loader(8) metadata */
	preload_metadata = (caddr_t)bootinfo.modptr;
	if (envmode == 1)
		kern_envp = static_env;
	else
		kern_envp = bootinfo.envp;

	/* Do basic tuning, hz etc */
	init_param1();

	/*
	 * Initalize the (temporary) bootstrap console interface, so
	 * we can use printf until the VM system starts being setup.
	 * The real console is initialized before then.
	 */
	init_bootstrap_console();

	/* OUTPUT NOW ALLOWED */

	/* delayed from above */
	if (bootinfo_msg)
		printf("WARNING: %s (0x%lx, 0x%lx, 0x%lx)\n",
		       bootinfo_msg, bim, bip, biv);

	/*
	 * Point interrupt/exception vectors to our own.
	 */
	alpha_pal_wrent(XentInt, ALPHA_KENTRY_INT);
	alpha_pal_wrent(XentArith, ALPHA_KENTRY_ARITH);
	alpha_pal_wrent(XentMM, ALPHA_KENTRY_MM);
	alpha_pal_wrent(XentIF, ALPHA_KENTRY_IF);
	alpha_pal_wrent(XentUna, ALPHA_KENTRY_UNA);
	alpha_pal_wrent(XentSys, ALPHA_KENTRY_SYS);

	/*
	 * Clear pending machine checks and error reports, and enable
	 * system- and processor-correctable error reporting.
	 */
	alpha_pal_wrmces(alpha_pal_rdmces() &
			 ~(ALPHA_MCES_DSC|ALPHA_MCES_DPC));

	/*
	 * Find out what hardware we're on, and do basic initialization.
	 */
	cputype = hwrpb->rpb_type;
	if (cputype < 0) {
		/*
		 * At least some white-box (NT) systems have SRM which
		 * reports a systype that's the negative of their
		 * blue-box (UNIX/OVMS) counterpart.
		 */
		cputype = -cputype;
	}
	
	if (cputype >= API_ST_BASE) {
		if (cputype >= napi_cpuinit + API_ST_BASE) {
			platform_not_supported(cputype);
			/* NOTREACHED */
		}
		cputype -= API_ST_BASE;
		api_cpuinit[cputype].init(cputype);
	} else {
		if (cputype >= ncpuinit) {
			platform_not_supported(cputype);
			/* NOTREACHED */
		}	
		cpuinit[cputype].init(cputype);
	}
	snprintf(cpu_model, sizeof(cpu_model), "%s", platform.model);

	/*
	 * Initalize the real console, so the the bootstrap console is
	 * no longer necessary.
	 */
	if (platform.cons_init) {
		platform.cons_init();
		promcndetach();
	}
	/* NO MORE FIRMWARE ACCESS ALLOWED */
#ifdef _PMAP_MAY_USE_PROM_CONSOLE
	/*
	 * XXX (unless _PMAP_MAY_USE_PROM_CONSOLE is defined and
	 * XXX pmap_uses_prom_console() evaluates to non-zero.)
	 */
#endif

	/*
	 * find out this system's page size
	 */
	if (hwrpb->rpb_page_size != PAGE_SIZE)
		panic("page size %ld != 8192?!", hwrpb->rpb_page_size);


	/*
	 * Find the beginning and end of the kernel (and leave a
	 * bit of space before the beginning for the bootstrap
	 * stack).
	 */
	kernstart = trunc_page(kernel_text) - 2 * PAGE_SIZE;
#ifdef DDB
	ksym_start = (void *)bootinfo.ssym;
	ksym_end   = (void *)bootinfo.esym;
	kernend = (vm_offset_t)round_page(ksym_end);
#else
	kernend = (vm_offset_t)round_page(_end);
#endif
	/* But if the bootstrap tells us otherwise, believe it! */
	if (bootinfo.kernend)
		kernend = round_page(bootinfo.kernend);
	if (preload_metadata == NULL)
		printf("WARNING: loader(8) metadata is missing!\n");

	kernstartpfn = atop(ALPHA_K0SEG_TO_PHYS(kernstart));
	kernendpfn = atop(ALPHA_K0SEG_TO_PHYS(kernend));

	/*
	 * Find out how much memory is available, by looking at
	 * the memory cluster descriptors.  This also tries to do
	 * its best to detect things things that have never been seen
	 * before...
	 */
	mddtp = (struct mddt *)(((caddr_t)hwrpb) + hwrpb->rpb_memdat_off);

	/* MDDT SANITY CHECKING */
	mddtweird = 0;
	if (mddtp->mddt_cluster_cnt < 2) {
		mddtweird = 1;
		printf("WARNING: weird number of mem clusters: %ld\n",
		       mddtp->mddt_cluster_cnt);
	}

#ifdef DEBUG_CLUSTER
	printf("Memory cluster count: %d\n", mddtp->mddt_cluster_cnt);
#endif

	phys_avail_cnt = 0;
	for (i = 0; i < mddtp->mddt_cluster_cnt; i++) {
		memc = &mddtp->mddt_clusters[i];
#ifdef DEBUG_CLUSTER
		printf("MEMC %d: pfn 0x%lx cnt 0x%lx usage 0x%lx\n", i,
		       memc->mddt_pfn, memc->mddt_pg_cnt, memc->mddt_usage);
#endif
		totalphysmem += memc->mddt_pg_cnt;

		if (memc->mddt_usage & MDDT_mbz) {
			mddtweird = 1;
			printf("WARNING: mem cluster %d has weird "
			       "usage 0x%lx\n", i, memc->mddt_usage);
			unknownmem += memc->mddt_pg_cnt;
			continue;
		}
		if (memc->mddt_usage & MDDT_NONVOLATILE) {
			/* XXX should handle these... */
			printf("WARNING: skipping non-volatile mem "
			       "cluster %d\n", i);
			unusedmem += memc->mddt_pg_cnt;
			continue;
		}
		if (memc->mddt_usage & MDDT_PALCODE) {
			resvmem += memc->mddt_pg_cnt;
			continue;
		}

		/*
		 * We have a memory cluster available for system
		 * software use.  We must determine if this cluster
		 * holds the kernel.
		 */
		/*
		 * XXX If the kernel uses the PROM console, we only use the
		 * XXX memory after the kernel in the first system segment,
		 * XXX to avoid clobbering prom mapping, data, etc.
		 */
		physmem += memc->mddt_pg_cnt;
		pfn0 = memc->mddt_pfn;
		pfn1 = memc->mddt_pfn + memc->mddt_pg_cnt;
		if (pfn0 <= kernendpfn && kernstartpfn <= pfn1) {
			/*
			 * Must compute the location of the kernel
			 * within the segment.
			 */
#ifdef DEBUG_CLUSTER
			printf("Cluster %d contains kernel\n", i);
#endif
			if (!pmap_uses_prom_console()) {
				if (pfn0 < kernstartpfn) {
					/*
					 * There is a chunk before the kernel.
					 */
#ifdef DEBUG_CLUSTER
					printf("Loading chunk before kernel: "
					       "0x%lx / 0x%lx\n", pfn0, kernstartpfn);
#endif
					phys_avail[phys_avail_cnt] = alpha_ptob(pfn0);
					phys_avail[phys_avail_cnt+1] = alpha_ptob(kernstartpfn);
					phys_avail_cnt += 2;
				}
			}
			if (kernendpfn < pfn1) {
				/*
				 * There is a chunk after the kernel.
				 */
#ifdef DEBUG_CLUSTER
				printf("Loading chunk after kernel: "
				       "0x%lx / 0x%lx\n", kernendpfn, pfn1);
#endif
				phys_avail[phys_avail_cnt] = alpha_ptob(kernendpfn);
				phys_avail[phys_avail_cnt+1] = alpha_ptob(pfn1);
				phys_avail_cnt += 2;
			}
		} else {
			/*
			 * Just load this cluster as one chunk.
			 */
#ifdef DEBUG_CLUSTER
			printf("Loading cluster %d: 0x%lx / 0x%lx\n", i,
			       pfn0, pfn1);
#endif
			phys_avail[phys_avail_cnt] = alpha_ptob(pfn0);
			phys_avail[phys_avail_cnt+1] = alpha_ptob(pfn1);
			phys_avail_cnt += 2;
			
		}
	}
	phys_avail[phys_avail_cnt] = 0;

	/*
	 * Dump out the MDDT if it looks odd...
	 */
	if (mddtweird) {
		printf("\n");
		printf("complete memory cluster information:\n");
		for (i = 0; i < mddtp->mddt_cluster_cnt; i++) {
			printf("mddt %d:\n", i);
			printf("\tpfn %lx\n",
			       mddtp->mddt_clusters[i].mddt_pfn);
			printf("\tcnt %lx\n",
			       mddtp->mddt_clusters[i].mddt_pg_cnt);
			printf("\ttest %lx\n",
			       mddtp->mddt_clusters[i].mddt_pg_test);
			printf("\tbva %lx\n",
			       mddtp->mddt_clusters[i].mddt_v_bitaddr);
			printf("\tbpa %lx\n",
			       mddtp->mddt_clusters[i].mddt_p_bitaddr);
			printf("\tbcksum %lx\n",
			       mddtp->mddt_clusters[i].mddt_bit_cksum);
			printf("\tusage %lx\n",
			       mddtp->mddt_clusters[i].mddt_usage);
		}
		printf("\n");
	}

	Maxmem = physmem;

#ifdef MAXMEM
	/*
	 * MAXMEM define is in kilobytes.
	 */
	Maxmem = alpha_btop(MAXMEM * 1024);
#endif

	/*
	 * hw.physmem is a size in bytes; we also allow k, m, and g suffixes
	 * for the appropriate modifiers.  This overrides MAXMEM.
	 */
	if ((p = getenv("hw.physmem")) != NULL) {
		u_int64_t AllowMem, sanity;
		char *ep;

		sanity = AllowMem = strtouq(p, &ep, 0);
		if ((ep != p) && (*ep != 0)) {
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
		if (AllowMem == 0)
			printf("Ignoring invalid memory size of '%s'\n", p);
		else
			Maxmem = alpha_btop(AllowMem);
		freeenv(p);
	}

	while (physmem > Maxmem) {
		int i = phys_avail_cnt - 2;
		size_t sz = alpha_btop(phys_avail[i+1] - phys_avail[i]);
		size_t nsz;
		if (physmem - sz > Maxmem) {
			phys_avail[i] = 0;
			phys_avail[i+1] = 0;
			phys_avail_cnt -= 2;
			physmem -= sz;
		} else {
			nsz = sz - (physmem - Maxmem);
			phys_avail[i+1] = phys_avail[i] + alpha_ptob(nsz);
			physmem -= (sz - nsz);
		}
	}
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
		msgbufp = (struct msgbuf*) ALPHA_PHYS_TO_K0SEG(phys_avail[i+1]);

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

	proc_linkup(&proc0, &ksegrp0, &kse0, &thread0);
	/*
	 * Init mapping for u page(s) for proc 0
	 */
	proc0uarea = (struct user *)pmap_steal_memory(UAREA_PAGES * PAGE_SIZE);
	proc0kstack = pmap_steal_memory(KSTACK_PAGES * PAGE_SIZE);
	proc0.p_uarea = proc0uarea;
	thread0.td_kstack = proc0kstack;
	thread0.td_pcb = (struct pcb *)
	    (thread0.td_kstack + KSTACK_PAGES * PAGE_SIZE) - 1;

	/*
	 * Setup the per-CPU data for the bootstrap cpu.
	 */
	{
		/* This is not a 'struct user' */
		size_t sz = round_page(KSTACK_PAGES * PAGE_SIZE);
		pcpup = (struct pcpu *) pmap_steal_memory(sz);
		pcpu_init(pcpup, alpha_pal_whami(), sz);
		alpha_pal_wrval((u_int64_t) pcpup);
		PCPU_GET(next_asn) = 1;	/* 0 used for proc0 pmap */
		PCPU_SET(curthread, &thread0);
#ifdef SMP
		thread0.td_md.md_kernnest = 1;
#endif
	}

	/*
	 * Initialize the virtual memory system, and set the
	 * page table base register in proc 0's PCB.
	 */
	pmap_bootstrap(ALPHA_PHYS_TO_K0SEG(alpha_ptob(ptb)),
	    hwrpb->rpb_max_asn);
	hwrpb->rpb_vptb = VPTBASE;
	hwrpb->rpb_checksum = hwrpb_checksum();


	/*
	 * Initialize the rest of proc 0's PCB, and cache its physical
	 * address.
	 */
	thread0.td_md.md_pcbpaddr =
	    (struct pcb *)ALPHA_K0SEG_TO_PHYS((vm_offset_t)thread0.td_pcb);

	/*
	 * Set the kernel sp, reserving space for an (empty) trapframe,
	 * and make proc0's trapframe pointer point to it for sanity.
	 */
	thread0.td_frame = (struct trapframe *)thread0.td_pcb - 1;
	thread0.td_pcb->pcb_hw.apcb_ksp = (u_int64_t)thread0.td_frame;

	mutex_init();
	mtx_init(&clock_lock, "clk", NULL, MTX_SPIN | MTX_RECURSE);
	mtx_init(&icu_lock, "icu", NULL, MTX_SPIN);

	/*
	 * Look at arguments passed to us and compute boothowto.
	 */

#ifdef KADB
	boothowto |= RB_KDB;
#endif
/*	boothowto |= RB_KDB | RB_GDB; */
	for (p = bootinfo.boot_flags; p && *p != '\0'; p++) {
		/*
		 * Note that we'd really like to differentiate case here,
		 * but the Alpha AXP Architecture Reference Manual
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
		freeenv(p);
	}

	/*
	 * Pick up kernelname.
	 */
	if (bootinfo_booted_kernel) {
		strncpy(kernelname, bootinfo_booted_kernel,
		   min(sizeof(kernelname), sizeof bootinfo.booted_kernel) - 1);
	} else if ((p = getenv("kernelname")) != NULL) {
		strncpy(kernelname, p, sizeof(kernelname) - 1);
		freeenv(p);
	}

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

	/*
	 * Figure out the number of cpus in the box, from RPB fields.
	 * Really.  We mean it.
	 */
	for (i = 0; i < hwrpb->rpb_pcs_cnt; i++) {
		struct pcs *pcsp;

		pcsp = (struct pcs *)((char *)hwrpb + hwrpb->rpb_pcs_off +
		    (i * hwrpb->rpb_pcs_size));
		if ((pcsp->pcs_flags & PCS_PP) != 0)
			ncpus++;
	}

	/*
	 * Figure out our clock frequency, from RPB fields.
	 */
	hz = hwrpb->rpb_intr_freq >> 12;
	if (!(60 <= hz && hz <= 10240)) {
		hz = 1024;
#ifdef DIAGNOSTIC
		printf("WARNING: unbelievable rpb_intr_freq: %ld (%d hz)\n",
			hwrpb->rpb_intr_freq, hz);
#endif
	}

	hwrpb_restart_setup();

	alpha_pal_wrfen(0);
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
	unsigned long pcc0, pcc1, curcycle, cycles;
        int usec;

	if (n == 0)
		return;

        pcc0 = alpha_rpcc() & 0xffffffffUL;
	cycles = 0;
	usec = 0;

        while (usec <= n) {
		/*
		 * Get the next CPU cycle count. The assumption here
		 * is that we can't have wrapped twice past 32 bits worth
		 * of CPU cycles since we last checked.
		 */
		pcc1 = alpha_rpcc() & 0xffffffffUL;
		if (pcc1 < pcc0) {
			curcycle = (pcc1 + 0x100000000UL) - pcc0;
		} else {
			curcycle = pcc1 - pcc0;
		}

		/*
		 * We now have the number of processor cycles since we
		 * last checked. Add the current cycle count to the
		 * running total. If it's over cycles_per_usec, increment
		 * the usec counter.
		 */
		cycles += curcycle;
		while (cycles > cycles_per_usec) {
			usec++;
			cycles -= cycles_per_usec;
		}
		pcc0 = pcc1;
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
#ifdef COMPAT_43
void
osendsig(sig_t catcher, int sig, sigset_t *mask, u_long code)
{
	struct proc *p;
	struct thread *td;
	osiginfo_t *sip, ksi;
	struct trapframe *frame;
	struct sigacts *psp;
	int oonstack, fsize, rndfsize;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	psp = p->p_sigacts;
	mtx_assert(&psp->ps_mtx, MA_OWNED);

	frame = td->td_frame;
	fsize = sizeof ksi;
	rndfsize = ((fsize + 15) / 16) * 16;
	oonstack = sigonstack(alpha_pal_rdusp());

	/*
	 * Allocate and validate space for the signal handler
	 * context. Note that if the stack is in P0 space, the
	 * call to grow() is a nop, and the useracc() check
	 * will fail if the process has not already allocated
	 * the space with a `brk'.
	 */
	if ((p->p_flag & P_ALTSTACK) && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		sip = (osiginfo_t *)((caddr_t)p->p_sigstk.ss_sp +
		    p->p_sigstk.ss_size - rndfsize);
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
		p->p_sigstk.ss_flags |= SS_ONSTACK;
#endif
	} else
		sip = (osiginfo_t *)(alpha_pal_rdusp() - rndfsize);
	mtx_unlock(&psp->ps_mtx);
	PROC_UNLOCK(p);

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	ksi.si_sc.sc_onstack = (oonstack) ? 1 : 0;
	SIG2OSIG(*mask, ksi.si_sc.sc_mask);
	ksi.si_sc.sc_pc = frame->tf_regs[FRAME_PC];
	ksi.si_sc.sc_ps = frame->tf_regs[FRAME_PS];

	/* copy the registers. */
	fill_regs(td, (struct reg *)ksi.si_sc.sc_regs);
	ksi.si_sc.sc_regs[R_ZERO] = 0xACEDBADE;		/* magic number */
	ksi.si_sc.sc_regs[R_SP] = alpha_pal_rdusp();

	/* save the floating-point state, if necessary, then copy it. */
	alpha_fpstate_save(td, 1);		/* XXX maybe write=0 */
	ksi.si_sc.sc_ownedfp = td->td_md.md_flags & MDTD_FPUSED;
	bcopy(&td->td_pcb->pcb_fp, (struct fpreg *)ksi.si_sc.sc_fpregs,
	    sizeof(struct fpreg));
	ksi.si_sc.sc_fp_control = td->td_pcb->pcb_fp_control;
	bzero(ksi.si_sc.sc_reserved, sizeof ksi.si_sc.sc_reserved); /* XXX */
	ksi.si_sc.sc_xxx1[0] = 0;				/* XXX */
	ksi.si_sc.sc_xxx1[1] = 0;				/* XXX */
	ksi.si_sc.sc_traparg_a0 = frame->tf_regs[FRAME_TRAPARG_A0];
	ksi.si_sc.sc_traparg_a1 = frame->tf_regs[FRAME_TRAPARG_A1];
	ksi.si_sc.sc_traparg_a2 = frame->tf_regs[FRAME_TRAPARG_A2];
	ksi.si_sc.sc_xxx2[0] = 0;				/* XXX */
	ksi.si_sc.sc_xxx2[1] = 0;				/* XXX */
	ksi.si_sc.sc_xxx2[2] = 0;				/* XXX */
	/* Fill in POSIX parts */
	ksi.si_signo = sig;
	ksi.si_code = code;
	ksi.si_value.sigval_ptr = NULL;				/* XXX */

	/*
	 * copy the frame out to userland.
	 */
	if (copyout((caddr_t)&ksi, (caddr_t)sip, fsize) != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		PROC_LOCK(p);
		sigexit(td, SIGILL);
		return;
	}

	/*
	 * Set up the registers to return to sigcode.
	 */
	frame->tf_regs[FRAME_PC] = PS_STRINGS - szosigcode;
	frame->tf_regs[FRAME_A0] = sig;
	frame->tf_regs[FRAME_FLAGS] = 0; /* full restore */
	PROC_LOCK(p);
	mtx_lock(&psp->ps_mtx);
	if (SIGISMEMBER(psp->ps_siginfo, sig))
		frame->tf_regs[FRAME_A1] = (u_int64_t)sip;
	else
		frame->tf_regs[FRAME_A1] = code;
	frame->tf_regs[FRAME_A2] = (u_int64_t)&sip->si_sc;
	frame->tf_regs[FRAME_T12] = (u_int64_t)catcher;	/* t12 is pv */
	alpha_pal_wrusp((unsigned long)sip);
}
#endif

#ifdef COMPAT_FREEBSD4
static void
freebsd4_sendsig(sig_t catcher, int sig, sigset_t *mask, u_long code)
{
	struct proc *p;
	struct thread *td;
	struct trapframe *frame;
	struct sigacts *psp;
	struct sigframe4 sf, *sfp;
	int oonstack, rndfsize;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	psp = p->p_sigacts;
	mtx_assert(&psp->ps_mtx, MA_OWNED);

	frame = td->td_frame;
	oonstack = sigonstack(alpha_pal_rdusp());
	rndfsize = ((sizeof(sf) + 15) / 16) * 16;

	/* save user context */
	bzero(&sf, sizeof(sf));
	sf.sf_uc.uc_sigmask = *mask;
	sf.sf_uc.uc_stack = p->p_sigstk;
	sf.sf_uc.uc_stack.ss_flags = (p->p_flag & P_ALTSTACK)
	    ? ((oonstack) ? SS_ONSTACK : 0) : SS_DISABLE;
	sf.sf_uc.uc_mcontext.mc_onstack = (oonstack) ? 1 : 0;

	fill_regs(td, (struct reg *)sf.sf_uc.uc_mcontext.mc_regs);
	sf.sf_uc.uc_mcontext.mc_regs[R_SP] = alpha_pal_rdusp();
	sf.sf_uc.uc_mcontext.mc_regs[R_ZERO] = 0xACEDBADE;   /* magic number */
	sf.sf_uc.uc_mcontext.mc_regs[R_PS] = frame->tf_regs[FRAME_PS];
	sf.sf_uc.uc_mcontext.mc_regs[R_PC] = frame->tf_regs[FRAME_PC];
	sf.sf_uc.uc_mcontext.mc_regs[R_TRAPARG_A0] =
	    frame->tf_regs[FRAME_TRAPARG_A0];
	sf.sf_uc.uc_mcontext.mc_regs[R_TRAPARG_A1] =
	    frame->tf_regs[FRAME_TRAPARG_A1];
	sf.sf_uc.uc_mcontext.mc_regs[R_TRAPARG_A2] =
	    frame->tf_regs[FRAME_TRAPARG_A2];

	/*
	 * Allocate and validate space for the signal handler
	 * context. Note that if the stack is in P0 space, the
	 * call to grow() is a nop, and the useracc() check
	 * will fail if the process has not already allocated
	 * the space with a `brk'.
	 */
	if ((p->p_flag & P_ALTSTACK) != 0 && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		sfp = (struct sigframe4 *)((caddr_t)p->p_sigstk.ss_sp +
		    p->p_sigstk.ss_size - rndfsize);
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
		p->p_sigstk.ss_flags |= SS_ONSTACK;
#endif
	} else
		sfp = (struct sigframe4 *)(alpha_pal_rdusp() - rndfsize);
	mtx_unlock(&psp->ps_mtx);
	PROC_UNLOCK(p);

	/* save the floating-point state, if necessary, then copy it. */
	alpha_fpstate_save(td, 1);
	sf.sf_uc.uc_mcontext.mc_ownedfp = td->td_md.md_flags & MDTD_FPUSED;
	bcopy(&td->td_pcb->pcb_fp,
	      (struct fpreg *)sf.sf_uc.uc_mcontext.mc_fpregs,
	      sizeof(struct fpreg));
	sf.sf_uc.uc_mcontext.mc_fp_control = td->td_pcb->pcb_fp_control;

#ifdef COMPAT_OSF1
	/*
	 * XXX Create an OSF/1-style sigcontext and associated goo.
	 */
#endif

	/*
	 * copy the frame out to userland.
	 */
	if (copyout((caddr_t)&sf, (caddr_t)sfp, sizeof(sf)) != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		PROC_LOCK(p);
		sigexit(td, SIGILL);
		return;
	}

	/*
	 * Set up the registers to return to sigcode.
	 */
	frame->tf_regs[FRAME_PC] = PS_STRINGS - szfreebsd4_sigcode;
	frame->tf_regs[FRAME_A0] = sig;
	PROC_LOCK(p);
	mtx_lock(&psp->ps_mtx);
	if (SIGISMEMBER(psp->ps_siginfo, sig)) {
		frame->tf_regs[FRAME_A1] = (u_int64_t)&(sfp->sf_si);

		/* Fill in POSIX parts */
		sf.sf_si.si_signo = sig;
		sf.sf_si.si_code = code;
		sf.sf_si.si_addr = (void*)frame->tf_regs[FRAME_TRAPARG_A0];
	}
	else
		frame->tf_regs[FRAME_A1] = code;

	frame->tf_regs[FRAME_A2] = (u_int64_t)&(sfp->sf_uc);
	frame->tf_regs[FRAME_T12] = (u_int64_t)catcher;	/* t12 is pv */
	frame->tf_regs[FRAME_FLAGS] = 0; /* full restore */
	alpha_pal_wrusp((unsigned long)sfp);
}
#endif	/* COMPAT_FREEBSD4 */

void
sendsig(sig_t catcher, int sig, sigset_t *mask, u_long code)
{
	struct proc *p;
	struct thread *td;
	struct trapframe *frame;
	struct sigacts *psp;
	struct sigframe sf, *sfp;
	int oonstack, rndfsize;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	psp = p->p_sigacts;
	mtx_assert(&psp->ps_mtx, MA_OWNED);
#ifdef COMPAT_FREEBSD4
	if (SIGISMEMBER(psp->ps_freebsd4, sig)) {
		freebsd4_sendsig(catcher, sig, mask, code);
		return;
	}
#endif
#ifdef COMPAT_43
	if (SIGISMEMBER(psp->ps_osigset, sig)) {
		osendsig(catcher, sig, mask, code);
		return;
	}
#endif

	frame = td->td_frame;
	oonstack = sigonstack(alpha_pal_rdusp());
	rndfsize = ((sizeof(sf) + 15) / 16) * 16;

	/* save user context */
	bzero(&sf, sizeof(struct sigframe));
	sf.sf_uc.uc_sigmask = *mask;
	sf.sf_uc.uc_stack = p->p_sigstk;
	sf.sf_uc.uc_stack.ss_flags = (p->p_flag & P_ALTSTACK)
	    ? ((oonstack) ? SS_ONSTACK : 0) : SS_DISABLE;
	sf.sf_uc.uc_mcontext.mc_onstack = (oonstack) ? 1 : 0;

	fill_regs(td, (struct reg *)sf.sf_uc.uc_mcontext.mc_regs);
	sf.sf_uc.uc_mcontext.mc_regs[R_SP] = alpha_pal_rdusp();
	sf.sf_uc.uc_mcontext.mc_regs[R_ZERO] = 0xACEDBADE;   /* magic number */
	sf.sf_uc.uc_mcontext.mc_regs[R_PS] = frame->tf_regs[FRAME_PS];
	sf.sf_uc.uc_mcontext.mc_regs[R_PC] = frame->tf_regs[FRAME_PC];
	sf.sf_uc.uc_mcontext.mc_regs[R_TRAPARG_A0] =
	    frame->tf_regs[FRAME_TRAPARG_A0];
	sf.sf_uc.uc_mcontext.mc_regs[R_TRAPARG_A1] =
	    frame->tf_regs[FRAME_TRAPARG_A1];
	sf.sf_uc.uc_mcontext.mc_regs[R_TRAPARG_A2] =
	    frame->tf_regs[FRAME_TRAPARG_A2];
	sf.sf_uc.uc_mcontext.mc_format = _MC_REV0_SIGFRAME;

	/*
	 * Allocate and validate space for the signal handler
	 * context. Note that if the stack is in P0 space, the
	 * call to grow() is a nop, and the useracc() check
	 * will fail if the process has not already allocated
	 * the space with a `brk'.
	 */
	if ((p->p_flag & P_ALTSTACK) != 0 && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		sfp = (struct sigframe *)((caddr_t)p->p_sigstk.ss_sp +
		    p->p_sigstk.ss_size - rndfsize);
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
		p->p_sigstk.ss_flags |= SS_ONSTACK;
#endif
	} else
		sfp = (struct sigframe *)(alpha_pal_rdusp() - rndfsize);
	mtx_unlock(&psp->ps_mtx);
	PROC_UNLOCK(p);

	/* save the floating-point state, if necessary, then copy it. */
	alpha_fpstate_save(td, 1);
	sf.sf_uc.uc_mcontext.mc_ownedfp = td->td_md.md_flags & MDTD_FPUSED;
	bcopy(&td->td_pcb->pcb_fp,
	      (struct fpreg *)sf.sf_uc.uc_mcontext.mc_fpregs,
	      sizeof(struct fpreg));
	sf.sf_uc.uc_mcontext.mc_fp_control = td->td_pcb->pcb_fp_control;

#ifdef COMPAT_OSF1
	/*
	 * XXX Create an OSF/1-style sigcontext and associated goo.
	 */
#endif

	/*
	 * copy the frame out to userland.
	 */
	if (copyout((caddr_t)&sf, (caddr_t)sfp, sizeof(sf)) != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		PROC_LOCK(p);
		sigexit(td, SIGILL);
		return;
	}

	/*
	 * Set up the registers to return to sigcode.
	 */
	frame->tf_regs[FRAME_PC] = PS_STRINGS - szsigcode;
	frame->tf_regs[FRAME_A0] = sig;
	PROC_LOCK(p);
	mtx_lock(&psp->ps_mtx);
	if (SIGISMEMBER(psp->ps_siginfo, sig)) {
		frame->tf_regs[FRAME_A1] = (u_int64_t)&(sfp->sf_si);

		/* Fill in POSIX parts */
		sf.sf_si.si_signo = sig;
		sf.sf_si.si_code = code;
		sf.sf_si.si_addr = (void*)frame->tf_regs[FRAME_TRAPARG_A0];
	}
	else
		frame->tf_regs[FRAME_A1] = code;

	frame->tf_regs[FRAME_A2] = (u_int64_t)&(sfp->sf_uc);
	frame->tf_regs[FRAME_T12] = (u_int64_t)catcher;	/* t12 is pv */
	frame->tf_regs[FRAME_FLAGS] = 0; /* full restore */
	alpha_pal_wrusp((unsigned long)sfp);
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
#ifdef COMPAT_43
int
osigreturn(struct thread *td,
	struct osigreturn_args /* {
		struct osigcontext *sigcntxp;
	} */ *uap)
{
	struct osigcontext *scp, ksc;
	struct proc *p = td->td_proc;

	scp = uap->sigcntxp;

	/*
	 * Fetch the entire context structure at once for speed.
	 */
	if (copyin((caddr_t)scp, (caddr_t)&ksc, sizeof ksc))
		return (EFAULT);

	/*
	 * XXX - Should we do this. What if we get a "handcrafted"
	 * but valid sigcontext that hasn't the magic number?
	 */
	if (ksc.sc_regs[R_ZERO] != 0xACEDBADE)		/* magic number */
		return (EINVAL);

	PROC_LOCK(p);
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	/*
	 * Restore the user-supplied information
	 */
	if (ksc.sc_onstack)
		p->p_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigstk.ss_flags &= ~SS_ONSTACK;
#endif

	/*
	 * longjmp is still implemented by calling osigreturn. The new
	 * sigmask is stored in sc_reserved, sc_mask is only used for
	 * backward compatibility.
	 */
	SIGSETOLD(td->td_sigmask, ksc.sc_mask);
	SIG_CANTMASK(td->td_sigmask);
	signotify(td);
	PROC_UNLOCK(p);

	set_regs(td, (struct reg *)ksc.sc_regs);
	td->td_frame->tf_regs[FRAME_PC] = ksc.sc_pc;
	td->td_frame->tf_regs[FRAME_PS] =
	    (ksc.sc_ps | ALPHA_PSL_USERSET) & ~ALPHA_PSL_USERCLR;
	td->td_frame->tf_regs[FRAME_FLAGS] = 0; /* full restore */

	alpha_pal_wrusp(ksc.sc_regs[R_SP]);

	/* XXX ksc.sc_ownedfp ? */
	alpha_fpstate_drop(td);
	bcopy((struct fpreg *)ksc.sc_fpregs, &td->td_pcb->pcb_fp,
	    sizeof(struct fpreg));
	td->td_pcb->pcb_fp_control = ksc.sc_fp_control;
	return (EJUSTRETURN);
}
#endif /* COMPAT_43 */

#ifdef COMPAT_FREEBSD4
/*
 * MPSAFE
 */
int
freebsd4_sigreturn(struct thread *td,
	struct freebsd4_sigreturn_args /* {
		const struct ucontext4 *sigcntxp;
	} */ *uap)
{
	struct ucontext4 uc;
	const struct ucontext4 *ucp;
	struct pcb *pcb;
	unsigned long val;
	struct proc *p;
	int error;

	ucp = uap->sigcntxp;
	pcb = td->td_pcb;
	p = td->td_proc;

	/*
	 * Fetch the entire context structure at once for speed.
	 * Note that struct osigcontext is smaller than a ucontext_t,
	 * so even if copyin() faults, we may have actually gotten a complete
	 * struct osigcontext.
	 */
	error = copyin(ucp, &uc, sizeof(ucontext_t));
	if (error != 0) {
#ifdef COMPAT_43
		if (((struct osigcontext*)&uc)->sc_regs[R_ZERO] == 0xACEDBADE)
			return osigreturn(td, (struct osigreturn_args *)uap);
#endif
		return (error);
	}

#ifdef COMPAT_43
	 if (((struct osigcontext*)&uc)->sc_regs[R_ZERO] == 0xACEDBADE)
		  return osigreturn(td, (struct osigreturn_args *)uap);
#endif

	/*
	 * Restore the user-supplied information
	 */
	set_regs(td, (struct reg *)uc.uc_mcontext.mc_regs);
	val = (uc.uc_mcontext.mc_regs[R_PS] | ALPHA_PSL_USERSET) &
	    ~ALPHA_PSL_USERCLR;
	td->td_frame->tf_regs[FRAME_PS] = val;
	td->td_frame->tf_regs[FRAME_PC] = uc.uc_mcontext.mc_regs[R_PC];
	td->td_frame->tf_regs[FRAME_FLAGS] = 0; /* full restore */
	alpha_pal_wrusp(uc.uc_mcontext.mc_regs[R_SP]);

	PROC_LOCK(p);
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	if (uc.uc_mcontext.mc_onstack & 1)
		p->p_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigstk.ss_flags &= ~SS_ONSTACK;
#endif

	td->td_sigmask = uc.uc_sigmask;
	SIG_CANTMASK(td->td_sigmask);
	signotify(td);
	PROC_UNLOCK(p);

	/* XXX ksc.sc_ownedfp ? */
	alpha_fpstate_drop(td);
	bcopy((struct fpreg *)uc.uc_mcontext.mc_fpregs,
	      &td->td_pcb->pcb_fp, sizeof(struct fpreg));
	td->td_pcb->pcb_fp_control = uc.uc_mcontext.mc_fp_control;
	return (EJUSTRETURN);
}
#endif /* COMPAT_FREEBSD4 */

/*
 * MPSAFE
 */
int
sigreturn(struct thread *td,
	struct sigreturn_args /* {
		const struct __ucontext *sigcntxp;
	} */ *uap)
{
	ucontext_t uc;
	const ucontext_t *ucp;
	struct pcb *pcb;
	unsigned long val;
	struct proc *p;
	int error;

	ucp = uap->sigcntxp;
	pcb = td->td_pcb;
	p = td->td_proc;

	/*
	 * Fetch the entire context structure at once for speed.
	 * Note that struct osigcontext is smaller than a ucontext_t,
	 * so even if copyin() faults, we may have actually gotten a complete
	 * struct osigcontext.
	 * XXX we'll *still* be getting osigcontext's here due to longjmp(3)
	 * brain damage.
	 */
	error = copyin(ucp, &uc, sizeof(ucontext_t));
	if (error != 0) {
#ifdef COMPAT_43
		if (((struct osigcontext*)&uc)->sc_regs[R_ZERO] == 0xACEDBADE)
			return osigreturn(td, (struct osigreturn_args *)uap);
#endif
		return (error);
	}
#ifdef COMPAT_43
	if (((struct osigcontext*)&uc)->sc_regs[R_ZERO] == 0xACEDBADE)
		return osigreturn(td, (struct osigreturn_args *)uap);
#endif

	/*
	 * Restore the user-supplied information
	 */
	if ((error = set_fpcontext(td, &uc.uc_mcontext)) != 0)
		return (error);
	set_regs(td, (struct reg *)uc.uc_mcontext.mc_regs);
	val = (uc.uc_mcontext.mc_regs[R_PS] | ALPHA_PSL_USERSET) &
	    ~ALPHA_PSL_USERCLR;
	td->td_frame->tf_regs[FRAME_PS] = val;
	td->td_frame->tf_regs[FRAME_PC] = uc.uc_mcontext.mc_regs[R_PC];
	td->td_frame->tf_regs[FRAME_FLAGS] = 0; /* full restore */
	alpha_pal_wrusp(uc.uc_mcontext.mc_regs[R_SP]);

	PROC_LOCK(p);
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	if (uc.uc_mcontext.mc_onstack & 1)
		p->p_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigstk.ss_flags &= ~SS_ONSTACK;
#endif

	td->td_sigmask = uc.uc_sigmask;
	SIG_CANTMASK(td->td_sigmask);
	signotify(td);
	PROC_UNLOCK(p);

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
	prom_halt(1);
}

/*
 * Clear registers on exec
 */
void
exec_setregs(struct thread *td, u_long entry, u_long stack, u_long ps_strings)
{
	struct trapframe *tfp = td->td_frame;

	bzero(tfp->tf_regs, FRAME_SIZE * sizeof tfp->tf_regs[0]);
	bzero(&td->td_pcb->pcb_fp, sizeof td->td_pcb->pcb_fp);
	td->td_pcb->pcb_fp_control = 0;
	td->td_pcb->pcb_fp.fpr_cr = (FPCR_DYN_NORMAL
					  | FPCR_INVD | FPCR_DZED
					  | FPCR_OVFD | FPCR_INED
					  | FPCR_UNFD);

	alpha_pal_wrusp(stack);
	tfp->tf_regs[FRAME_PS] = ALPHA_PSL_USERSET;
	tfp->tf_regs[FRAME_PC] = entry & ~3;

	tfp->tf_regs[FRAME_A0] = stack;			/* a0 = sp */
	tfp->tf_regs[FRAME_A1] = 0;			/* a1 = rtld cleanup */
	tfp->tf_regs[FRAME_A2] = 0;			/* a2 = rtld object */
	tfp->tf_regs[FRAME_A3] = PS_STRINGS;		/* a3 = ps_strings */
	tfp->tf_regs[FRAME_T12] = tfp->tf_regs[FRAME_PC];	/* a.k.a. PV */
	tfp->tf_regs[FRAME_FLAGS] = 0;			/* full restore */

	td->td_md.md_flags &= ~MDTD_FPUSED;
	alpha_fpstate_drop(td);
}

int
ptrace_set_pc(struct thread *td, unsigned long addr)
{
	struct trapframe *tp = td->td_frame;
	tp->tf_regs[FRAME_PC] = addr;
	return 0;
}

static int
ptrace_read_int(struct thread *td, vm_offset_t addr, u_int32_t *v)
{
	struct iovec iov;
	struct uio uio;
	iov.iov_base = (caddr_t) v;
	iov.iov_len = sizeof(u_int32_t);
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = (off_t)addr;
	uio.uio_resid = sizeof(u_int32_t);
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_td = td;
	return proc_rwmem(td->td_proc, &uio);
}

static int
ptrace_write_int(struct thread *td, vm_offset_t addr, u_int32_t v)
{
	struct iovec iov;
	struct uio uio;
	iov.iov_base = (caddr_t) &v;
	iov.iov_len = sizeof(u_int32_t);
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = (off_t)addr;
	uio.uio_resid = sizeof(u_int32_t);
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_WRITE;
	uio.uio_td = td;
	return proc_rwmem(td->td_proc, &uio);
}

static u_int64_t
ptrace_read_register(struct thread *td, int regno)
{
	static int reg_to_frame[32] = {
		FRAME_V0,
		FRAME_T0,
		FRAME_T1,
		FRAME_T2,
		FRAME_T3,
		FRAME_T4,
		FRAME_T5,
		FRAME_T6,
		FRAME_T7,

		FRAME_S0,
		FRAME_S1,
		FRAME_S2,
		FRAME_S3,
		FRAME_S4,
		FRAME_S5,
		FRAME_S6,

		FRAME_A0,
		FRAME_A1,
		FRAME_A2,
		FRAME_A3,
		FRAME_A4,
		FRAME_A5,

		FRAME_T8,
		FRAME_T9,
		FRAME_T10,
		FRAME_T11,
		FRAME_RA,
		FRAME_T12,
		FRAME_AT,
		FRAME_GP,
		FRAME_SP,
		-1,		/* zero */
	};

	if (regno == R_ZERO)
		return 0;

	return td->td_frame->tf_regs[reg_to_frame[regno]];
}


static int
ptrace_clear_bpt(struct thread *td, struct mdbpt *bpt)
{
	return ptrace_write_int(td, bpt->addr, bpt->contents);
}

static int
ptrace_set_bpt(struct thread *td, struct mdbpt *bpt)
{
	int error;
	u_int32_t bpins = 0x00000080;
	error = ptrace_read_int(td, bpt->addr, &bpt->contents);
	if (error)
		return error;
	return ptrace_write_int(td, bpt->addr, bpins);
}

int
ptrace_clear_single_step(struct thread *td)
{
	if (td->td_md.md_flags & MDTD_STEP2) {
		ptrace_clear_bpt(td, &td->td_md.md_sstep[1]);
		ptrace_clear_bpt(td, &td->td_md.md_sstep[0]);
		td->td_md.md_flags &= ~MDTD_STEP2;
	} else if (td->td_md.md_flags & MDTD_STEP1) {
		ptrace_clear_bpt(td, &td->td_md.md_sstep[0]);
		td->td_md.md_flags &= ~MDTD_STEP1;
	}
	return 0;
}

int
ptrace_single_step(struct thread *td)
{
	int error;
	vm_offset_t pc = td->td_frame->tf_regs[FRAME_PC];
	alpha_instruction ins;
	vm_offset_t addr[2];	/* places to set breakpoints */
	int count = 0;		/* count of breakpoints */

	if (td->td_md.md_flags & (MDTD_STEP1|MDTD_STEP2))
		panic("ptrace_single_step: step breakpoints not removed");

	error = ptrace_read_int(td, pc, &ins.bits);
	if (error)
		return error;

	switch (ins.branch_format.opcode) {

	case op_j:
		/* Jump: target is register value */
		addr[0] = ptrace_read_register(td, ins.jump_format.rs) & ~3;
		count = 1;
		break;

	case op_br:
	case op_fbeq:
	case op_fblt:
	case op_fble:
	case op_bsr:
	case op_fbne:
	case op_fbge:
	case op_fbgt:
	case op_blbc:
	case op_beq:
	case op_blt:
	case op_ble:
	case op_blbs:
	case op_bne:
	case op_bge:
	case op_bgt:
		/* Branch: target is pc+4+4*displacement */
		addr[0] = pc + 4;
		addr[1] = pc + 4 + 4 * ins.branch_format.displacement;
		count = 2;
		break;

	default:
		addr[0] = pc + 4;
		count = 1;
	}

	td->td_md.md_sstep[0].addr = addr[0];
	error = ptrace_set_bpt(td, &td->td_md.md_sstep[0]);
	if (error)
		return error;
	if (count == 2) {
		td->td_md.md_sstep[1].addr = addr[1];
		error = ptrace_set_bpt(td, &td->td_md.md_sstep[1]);
		if (error) {
			ptrace_clear_bpt(td, &td->td_md.md_sstep[0]);
			return error;
		}
		td->td_md.md_flags |= MDTD_STEP2;
	} else
		td->td_md.md_flags |= MDTD_STEP1;

	return 0;
}

int
alpha_pa_access(vm_offset_t pa)
{
#if 0
	int i;

	for (i = 0; phys_avail[i] != 0; i += 2) {
		if (pa < phys_avail[i])
			continue;
		if (pa < phys_avail[i+1])
			return VM_PROT_READ|VM_PROT_WRITE;
	}
	return 0;
#else
	return VM_PROT_READ|VM_PROT_WRITE;
#endif
}

int
fill_regs(td, regs)
	struct thread *td;
	struct reg *regs;
{
	struct pcb *pcb = td->td_pcb;
	struct trapframe *tp = td->td_frame;

#define C(r)	regs->r_regs[R_ ## r] = tp->tf_regs[FRAME_ ## r]

	C(V0);
	C(T0); C(T1); C(T2); C(T3); C(T4); C(T5); C(T6); C(T7);
	C(S0); C(S1); C(S2); C(S3); C(S4); C(S5); C(S6);
	C(A0); C(A1); C(A2); C(A3); C(A4); C(A5);
	C(T8); C(T9); C(T10); C(T11);
	C(RA); C(T12); C(AT); C(GP);

#undef C

	regs->r_regs[R_ZERO] = tp->tf_regs[FRAME_PC];
	regs->r_regs[R_SP] = pcb->pcb_hw.apcb_usp;

	return (0);
}

int
set_regs(td, regs)
	struct thread *td;
	struct reg *regs;
{
	struct pcb *pcb = td->td_pcb;
	struct trapframe *tp = td->td_frame;

#define C(r)	tp->tf_regs[FRAME_ ## r] = regs->r_regs[R_ ## r]

	C(V0);
	C(T0); C(T1); C(T2); C(T3); C(T4); C(T5); C(T6); C(T7);
	C(S0); C(S1); C(S2); C(S3); C(S4); C(S5); C(S6);
	C(A0); C(A1); C(A2); C(A3); C(A4); C(A5);
	C(T8); C(T9); C(T10); C(T11);
	C(RA); C(T12); C(AT); C(GP);

#undef C

	tp->tf_regs[FRAME_PC] = regs->r_regs[R_ZERO];
	pcb->pcb_hw.apcb_usp = regs->r_regs[R_SP];

	return (0);
}

int
get_mcontext(struct thread *td, mcontext_t *mcp, int clear_ret)
{
	/*
	 * Use a trapframe for getsetcontext, so just copy the
	 * threads trapframe.
	 */
	bcopy(td->td_frame, &mcp->mc_regs, sizeof(struct trapframe));
	if (clear_ret != 0) {
		mcp->mc_regs[FRAME_V0] = 0;
		mcp->mc_regs[FRAME_A4] = 0;
	}

	/*
	 * When the thread is the current thread, the user stack pointer
	 * is not in the PCB; it must be read from the PAL.
	 */
	if (td == curthread)
		mcp->mc_regs[FRAME_SP] = alpha_pal_rdusp();

	mcp->mc_format = _MC_REV0_TRAPFRAME;
	PROC_LOCK(curthread->td_proc);
	mcp->mc_onstack = sigonstack(alpha_pal_rdusp()) ? 1 : 0;
	PROC_UNLOCK(curthread->td_proc);
	get_fpcontext(td, mcp);
	return (0);
}

int
set_mcontext(struct thread *td, const mcontext_t *mcp)
{
	int ret;
	unsigned long val;

	if ((mcp->mc_format != _MC_REV0_TRAPFRAME) &&
	    (mcp->mc_format != _MC_REV0_SIGFRAME))
		return (EINVAL);
	else if ((ret = set_fpcontext(td, mcp)) != 0)
		return (ret);

	if (mcp->mc_format == _MC_REV0_SIGFRAME) {
		set_regs(td, (struct reg *)&mcp->mc_regs);
		val = (mcp->mc_regs[R_PS] | ALPHA_PSL_USERSET) &
		    ~ALPHA_PSL_USERCLR;
		td->td_frame->tf_regs[FRAME_PS] = val;
		td->td_frame->tf_regs[FRAME_PC] = mcp->mc_regs[R_PC];
		td->td_frame->tf_regs[FRAME_FLAGS] = 0;
		if (td == curthread)
			alpha_pal_wrusp(mcp->mc_regs[R_SP]);

	} else {
		if (td == curthread)
			alpha_pal_wrusp(mcp->mc_regs[FRAME_SP]);
		/*
		 * The context is a trapframe, so just copy it over the
		 * threads frame.
		 */
		bcopy(&mcp->mc_regs, td->td_frame, sizeof(struct trapframe));
	}
	return (0);
}

static void
get_fpcontext(struct thread *td, mcontext_t *mcp)
{
	register_t s;

	s = intr_disable();
	if ((td->td_md.md_flags & MDTD_FPUSED) == 0) {
		intr_restore(s);
		mcp->mc_ownedfp = _MC_FPOWNED_NONE;
	} else if (PCPU_GET(fpcurthread) == td) {
		/* See comments in alpha_fpstate_save() regarding FEN. */
		if (td != curthread)
			alpha_pal_wrfen(1);
		/*
		 * The last field (fpr_cr) of struct fpreg isn't
		 * included in mc_fpregs, but it immediately follows
		 * it in mcontext_t.
		 */
		savefpstate((struct fpreg *)&mcp->mc_fpregs);
		if (td != curthread)
			alpha_pal_wrfen(0);
		intr_restore(s);
		mcp->mc_ownedfp = _MC_FPOWNED_FPU;
	} else {
		/*
		 * The thread doesn't own the FPU so get the state from
		 * the PCB.
		 */
		intr_restore(s);
		bcopy(&td->td_pcb->pcb_fp, &mcp->mc_fpregs,
		    sizeof(td->td_pcb->pcb_fp));
		mcp->mc_ownedfp = _MC_FPOWNED_PCB;
	}
	/* There's no harm in always doing the following. */
	mcp->mc_fp_control = td->td_pcb->pcb_fp_control;
}

static int
set_fpcontext(struct thread *td, const mcontext_t *mcp)
{
	register_t s;

	if (mcp->mc_ownedfp == _MC_FPOWNED_NONE) {
		/* XXX - Drop fpu state so we get a clean state? */
		alpha_fpstate_drop(td);
	}
	else if ((mcp->mc_ownedfp != _MC_FPOWNED_FPU) &&
	    (mcp->mc_ownedfp != _MC_FPOWNED_PCB))
		return (EINVAL);
	else {
		s = intr_disable();
		if (PCPU_GET(fpcurthread) == td) {
			/*
			 * The last field (fpr_cr) of struct fpreg isn't
			 * included in mc_fpregs, but it immediately follows
			 * it in mcontext_t.
			 */
			restorefpstate((struct fpreg *)&mcp->mc_fpregs);
			intr_restore(s);
		}
		else {
			/* Just save the state in the PCB. */
			intr_restore(s);
			bcopy(&mcp->mc_fpregs, &td->td_pcb->pcb_fp,
			    sizeof (td->td_pcb->pcb_fp));
		}
		td->td_pcb->pcb_fp_control = mcp->mc_fp_control;
	}
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
	alpha_fpstate_save(td, 0);

	bcopy(&td->td_pcb->pcb_fp, fpregs, sizeof *fpregs);
	return (0);
}

int
set_fpregs(td, fpregs)
	struct thread *td;
	struct fpreg *fpregs;
{
	alpha_fpstate_drop(td);

	bcopy(fpregs, &td->td_pcb->pcb_fp, sizeof *fpregs);
	return (0);
}

#ifndef DDB
void
Debugger(const char *msg)
{
	printf("Debugger(\"%s\") called.\n", msg);
}
#endif /* no DDB */

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
alpha_fpstate_check(struct thread *td)
{
	/*
	 * For SMP, we should check the fpcurthread of each cpu.
	 */
#ifndef SMP
	register_t s;

	s = intr_disable();
	if (td->td_pcb->pcb_hw.apcb_flags & ALPHA_PCB_FLAGS_FEN)
		if (td != PCPU_GET(fpcurthread))
			panic("alpha_check_fpcurthread: bogus");
	intr_restore(s);
#endif
}

#define SET_FEN(td) \
	(td)->td_pcb->pcb_hw.apcb_flags |= ALPHA_PCB_FLAGS_FEN

#define CLEAR_FEN(td) \
	(td)->td_pcb->pcb_hw.apcb_flags &= ~ALPHA_PCB_FLAGS_FEN

/*
 * Save the floating point state in the pcb. Use this to get read-only
 * access to the floating point state. If write is true, the current
 * fp process is cleared so that fp state can safely be modified. The
 * process will automatically reload the changed state by generating a 
 * FEN trap.
 */
void
alpha_fpstate_save(struct thread *td, int write)
{
	register_t s;

	s = intr_disable();
	if (td != NULL && td == PCPU_GET(fpcurthread)) {
		/*
		 * If curthread != fpcurthread, then we need to enable FEN 
		 * so that we can dump the fp state.
		 */
		alpha_pal_wrfen(1);

		/*
		 * Save the state in the pcb.
		 */
		savefpstate(&td->td_pcb->pcb_fp);

		if (write) {
			/*
			 * If fpcurthread == curthread, just ask the
			 * PALcode to disable FEN, otherwise we must
			 * clear the FEN bit in fpcurthread's pcb.
			 */
			if (PCPU_GET(fpcurthread) == curthread)
				alpha_pal_wrfen(0);
			else
				CLEAR_FEN(PCPU_GET(fpcurthread));
			PCPU_SET(fpcurthread, NULL);
		} else {
			/*
			 * Make sure that we leave FEN enabled if
			 * curthread == fpcurthread. We must have at most
			 * one process with FEN enabled. Note that FEN 
			 * must already be set in fpcurthread's pcb.
			 */
			if (curthread != PCPU_GET(fpcurthread))
				alpha_pal_wrfen(0);
		}
	}
	intr_restore(s);
}

/*
 * Relinquish ownership of the FP state. This is called instead of
 * alpha_save_fpstate() if the entire FP state is being changed
 * (e.g. on sigreturn).
 */
void
alpha_fpstate_drop(struct thread *td)
{
	register_t s;

	s = intr_disable();
	if (td == PCPU_GET(fpcurthread)) {
		if (td == curthread) {
			/*
			 * Disable FEN via the PALcode. This will
			 * clear the bit in the pcb as well.
			 */
			alpha_pal_wrfen(0);
		} else {
			/*
			 * Clear the FEN bit of the pcb.
			 */
			CLEAR_FEN(td);
		}
		PCPU_SET(fpcurthread, NULL);
	}
	intr_restore(s);
}

/*
 * Switch the current owner of the fp state to p, reloading the state
 * from the pcb.
 */
void
alpha_fpstate_switch(struct thread *td)
{
	register_t s;

	/*
	 * Enable FEN so that we can access the fp registers.
	 */
	s = intr_disable();
	alpha_pal_wrfen(1);
	if (PCPU_GET(fpcurthread)) {
		/*
		 * Dump the old fp state if its valid.
		 */
		savefpstate(&PCPU_GET(fpcurthread)->td_pcb->pcb_fp);
		CLEAR_FEN(PCPU_GET(fpcurthread));
	}

	/*
	 * Remember the new FP owner and reload its state.
	 */
	PCPU_SET(fpcurthread, td);
	restorefpstate(&PCPU_GET(fpcurthread)->td_pcb->pcb_fp);

	/*
	 * If the new owner is curthread, leave FEN enabled, otherwise
	 * mark its PCB so that it gets FEN when we context switch to
	 * it later.
	 */
	if (td != curthread) {
		alpha_pal_wrfen(0);
		SET_FEN(td);
	}

	td->td_md.md_flags |= MDTD_FPUSED;
	intr_restore(s);
}

/*
 * Initialise a struct pcpu.
 */
void
cpu_pcpu_init(struct pcpu *pcpu, int cpuid, size_t sz)
{

	pcpu->pc_idlepcbphys = vtophys((vm_offset_t) &pcpu->pc_idlepcb);
	pcpu->pc_idlepcb.apcb_ksp = (u_int64_t)
		((caddr_t) pcpu + sz - sizeof(struct trapframe));
	pcpu->pc_idlepcb.apcb_ptbr = thread0.td_pcb->pcb_hw.apcb_ptbr;
	pcpu->pc_current_asngen = 1;
}

intptr_t
casuptr(intptr_t *p, intptr_t old, intptr_t new)
{
	return (-1);
}
