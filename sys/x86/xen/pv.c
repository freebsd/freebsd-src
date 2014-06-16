/*
 * Copyright (c) 2004 Christian Limpach.
 * Copyright (c) 2004-2006,2008 Kip Macy
 * Copyright (c) 2013 Roger Pau Monné <roger.pau@citrix.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS AS IS'' AND
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/boot.h>
#include <sys/ctype.h>
#include <sys/mutex.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <vm/vm_param.h>

#include <x86/init.h>
#include <machine/pc/bios.h>
#include <machine/smp.h>

#include <xen/xen-os.h>
#include <xen/hypervisor.h>
#include <xen/xenstore/xenstorevar.h>
#include <xen/xen_pv.h>

#include <xen/interface/vcpu.h>

#include <dev/xen/timer/timer.h>

/* Native initial function */
extern u_int64_t hammer_time(u_int64_t, u_int64_t);
/* Xen initial function */
uint64_t hammer_time_xen(start_info_t *, uint64_t);

#define MAX_E820_ENTRIES	128

/*--------------------------- Forward Declarations ---------------------------*/
static caddr_t xen_pv_parse_preload_data(u_int64_t);
static void xen_pv_parse_memmap(caddr_t, vm_paddr_t *, int *);

#ifdef SMP
static int xen_pv_start_all_aps(void);
#endif

/*---------------------------- Extern Declarations ---------------------------*/
#ifdef SMP
/* Variables used by amd64 mp_machdep to start APs */
extern struct mtx ap_boot_mtx;
extern void *bootstacks[];
extern char *doublefault_stack;
extern char *nmi_stack;
extern void *dpcpu;
extern int bootAP;
extern char *bootSTK;
#endif

/*-------------------------------- Global Data -------------------------------*/
/* Xen init_ops implementation. */
struct init_ops xen_init_ops = {
	.parse_preload_data		= xen_pv_parse_preload_data,
	.early_clock_source_init	= xen_clock_init,
	.early_delay			= xen_delay,
	.parse_memmap			= xen_pv_parse_memmap,
#ifdef SMP
	.start_all_aps			= xen_pv_start_all_aps,
#endif
};

static struct bios_smap xen_smap[MAX_E820_ENTRIES];

/*-------------------------------- Xen PV init -------------------------------*/
/*
 * First function called by the Xen PVH boot sequence.
 *
 * Set some Xen global variables and prepare the environment so it is
 * as similar as possible to what native FreeBSD init function expects.
 */
uint64_t
hammer_time_xen(start_info_t *si, uint64_t xenstack)
{
	uint64_t physfree;
	uint64_t *PT4 = (u_int64_t *)xenstack;
	uint64_t *PT3 = (u_int64_t *)(xenstack + PAGE_SIZE);
	uint64_t *PT2 = (u_int64_t *)(xenstack + 2 * PAGE_SIZE);
	int i;

	xen_domain_type = XEN_PV_DOMAIN;
	vm_guest = VM_GUEST_XEN;

	if ((si == NULL) || (xenstack == 0)) {
		xc_printf("ERROR: invalid start_info or xen stack, halting\n");
		HYPERVISOR_shutdown(SHUTDOWN_crash);
	}

	xc_printf("FreeBSD PVH running on %s\n", si->magic);

	/* We use 3 pages of xen stack for the boot pagetables */
	physfree = xenstack + 3 * PAGE_SIZE - KERNBASE;

	/* Setup Xen global variables */
	HYPERVISOR_start_info = si;
	HYPERVISOR_shared_info =
	    (shared_info_t *)(si->shared_info + KERNBASE);

	/*
	 * Setup some misc global variables for Xen devices
	 *
	 * XXX: Devices that need these specific variables should
	 *      be rewritten to fetch this info by themselves from the
	 *      start_info page.
	 */
	xen_store = (struct xenstore_domain_interface *)
	    (ptoa(si->store_mfn) + KERNBASE);
	console_page = (char *)(ptoa(si->console.domU.mfn) + KERNBASE);

	/*
	 * Use the stack Xen gives us to build the page tables
	 * as native FreeBSD expects to find them (created
	 * by the boot trampoline).
	 */
	for (i = 0; i < (PAGE_SIZE / sizeof(uint64_t)); i++) {
		/*
		 * Each slot of the level 4 pages points
		 * to the same level 3 page
		 */
		PT4[i] = ((uint64_t)&PT3[0]) - KERNBASE;
		PT4[i] |= PG_V | PG_RW | PG_U;

		/*
		 * Each slot of the level 3 pages points
		 * to the same level 2 page
		 */
		PT3[i] = ((uint64_t)&PT2[0]) - KERNBASE;
		PT3[i] |= PG_V | PG_RW | PG_U;

		/*
		 * The level 2 page slots are mapped with
		 * 2MB pages for 1GB.
		 */
		PT2[i] = i * (2 * 1024 * 1024);
		PT2[i] |= PG_V | PG_RW | PG_PS | PG_U;
	}
	load_cr3(((uint64_t)&PT4[0]) - KERNBASE);

	/* Set the hooks for early functions that diverge from bare metal */
	init_ops = xen_init_ops;
	apic_ops = xen_apic_ops;

	/* Now we can jump into the native init function */
	return (hammer_time(0, physfree));
}

/*-------------------------------- PV specific -------------------------------*/
#ifdef SMP
static bool
start_xen_ap(int cpu)
{
	struct vcpu_guest_context *ctxt;
	int ms, cpus = mp_naps;
	const size_t stacksize = KSTACK_PAGES * PAGE_SIZE;

	/* allocate and set up an idle stack data page */
	bootstacks[cpu] =
	    (void *)kmem_malloc(kernel_arena, stacksize, M_WAITOK | M_ZERO);
	doublefault_stack =
	    (char *)kmem_malloc(kernel_arena, PAGE_SIZE, M_WAITOK | M_ZERO);
	nmi_stack =
	    (char *)kmem_malloc(kernel_arena, PAGE_SIZE, M_WAITOK | M_ZERO);
	dpcpu =
	    (void *)kmem_malloc(kernel_arena, DPCPU_SIZE, M_WAITOK | M_ZERO);

	bootSTK = (char *)bootstacks[cpu] + KSTACK_PAGES * PAGE_SIZE - 8;
	bootAP = cpu;

	ctxt = malloc(sizeof(*ctxt), M_TEMP, M_WAITOK | M_ZERO);
	if (ctxt == NULL)
		panic("unable to allocate memory");

	ctxt->flags = VGCF_IN_KERNEL;
	ctxt->user_regs.rip = (unsigned long) init_secondary;
	ctxt->user_regs.rsp = (unsigned long) bootSTK;

	/* Set the AP to use the same page tables */
	ctxt->ctrlreg[3] = KPML4phys;

	if (HYPERVISOR_vcpu_op(VCPUOP_initialise, cpu, ctxt))
		panic("unable to initialize AP#%d", cpu);

	free(ctxt, M_TEMP);

	/* Launch the vCPU */
	if (HYPERVISOR_vcpu_op(VCPUOP_up, cpu, NULL))
		panic("unable to start AP#%d", cpu);

	/* Wait up to 5 seconds for it to start. */
	for (ms = 0; ms < 5000; ms++) {
		if (mp_naps > cpus)
			return (true);
		DELAY(1000);
	}

	return (false);
}

static int
xen_pv_start_all_aps(void)
{
	int cpu;

	mtx_init(&ap_boot_mtx, "ap boot", NULL, MTX_SPIN);

	for (cpu = 1; cpu < mp_ncpus; cpu++) {

		/* attempt to start the Application Processor */
		if (!start_xen_ap(cpu))
			panic("AP #%d failed to start!", cpu);

		CPU_SET(cpu, &all_cpus);	/* record AP in CPU map */
	}

	return (mp_naps);
}
#endif /* SMP */

/*
 * Functions to convert the "extra" parameters passed by Xen
 * into FreeBSD boot options.
 */
static void
xen_pv_set_env(void)
{
	char *cmd_line_next, *cmd_line;
	size_t env_size;

	cmd_line = HYPERVISOR_start_info->cmd_line;
	env_size = sizeof(HYPERVISOR_start_info->cmd_line);

	/* Skip leading spaces */
	for (; isspace(*cmd_line) && (env_size != 0); cmd_line++)
		env_size--;

	/* Replace ',' with '\0' */
	for (cmd_line_next = cmd_line; strsep(&cmd_line_next, ",") != NULL;)
		;

	init_static_kenv(cmd_line, env_size);
}

static void
xen_pv_set_boothowto(void)
{
	int i;

	/* get equivalents from the environment */
	for (i = 0; howto_names[i].ev != NULL; i++) {
		if (getenv(howto_names[i].ev) != NULL)
			boothowto |= howto_names[i].mask;
	}
}

static caddr_t
xen_pv_parse_preload_data(u_int64_t modulep)
{
	/* Parse the extra boot information given by Xen */
	xen_pv_set_env();
	xen_pv_set_boothowto();

	return (NULL);
}

static void
xen_pv_parse_memmap(caddr_t kmdp, vm_paddr_t *physmap, int *physmap_idx)
{
	struct xen_memory_map memmap;
	u_int32_t size;
	int rc;

	/* Fetch the E820 map from Xen */
	memmap.nr_entries = MAX_E820_ENTRIES;
	set_xen_guest_handle(memmap.buffer, xen_smap);
	rc = HYPERVISOR_memory_op(XENMEM_memory_map, &memmap);
	if (rc)
		panic("unable to fetch Xen E820 memory map");
	size = memmap.nr_entries * sizeof(xen_smap[0]);

	bios_add_smap_entries(xen_smap, size, physmap, physmap_idx);
}
