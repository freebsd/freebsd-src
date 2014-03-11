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
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/boot.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <vm/vm_param.h>

#include <xen/xen-os.h>
#include <xen/hypervisor.h>

/* Native initial function */
extern u_int64_t hammer_time(u_int64_t, u_int64_t);
/* Xen initial function */
uint64_t hammer_time_xen(start_info_t *, uint64_t);

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
		/* Each slot of the level 4 pages points to the same level 3 page */
		PT4[i] = ((uint64_t)&PT3[0]) - KERNBASE;
		PT4[i] |= PG_V | PG_RW | PG_U;

		/* Each slot of the level 3 pages points to the same level 2 page */
		PT3[i] = ((uint64_t)&PT2[0]) - KERNBASE;
		PT3[i] |= PG_V | PG_RW | PG_U;

		/* The level 2 page slots are mapped with 2MB pages for 1GB. */
		PT2[i] = i * (2 * 1024 * 1024);
		PT2[i] |= PG_V | PG_RW | PG_PS | PG_U;
	}
	load_cr3(((uint64_t)&PT4[0]) - KERNBASE);

	/* Now we can jump into the native init function */
	return (hammer_time(0, physfree));
}
