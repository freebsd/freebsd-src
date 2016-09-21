/*-
 * Copyright (c) 2004 Marcel Moolenaar
 * Copyright (c) 2001 Doug Rabson
 * Copyright (c) 2016 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/efi.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/clock.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <machine/fpu.h>
#include <machine/efi.h>
#include <machine/metadata.h>
#include <machine/md_var.h>
#include <machine/smp.h>
#include <machine/vmparam.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>

static struct efi_systbl *efi_systbl;
static struct efi_cfgtbl *efi_cfgtbl;
static struct efi_rt *efi_runtime;

static int efi_status2err[25] = {
	0,		/* EFI_SUCCESS */
	ENOEXEC,	/* EFI_LOAD_ERROR */
	EINVAL,		/* EFI_INVALID_PARAMETER */
	ENOSYS,		/* EFI_UNSUPPORTED */
	EMSGSIZE, 	/* EFI_BAD_BUFFER_SIZE */
	EOVERFLOW,	/* EFI_BUFFER_TOO_SMALL */
	EBUSY,		/* EFI_NOT_READY */
	EIO,		/* EFI_DEVICE_ERROR */
	EROFS,		/* EFI_WRITE_PROTECTED */
	EAGAIN,		/* EFI_OUT_OF_RESOURCES */
	EIO,		/* EFI_VOLUME_CORRUPTED */
	ENOSPC,		/* EFI_VOLUME_FULL */
	ENXIO,		/* EFI_NO_MEDIA */
	ESTALE,		/* EFI_MEDIA_CHANGED */
	ENOENT,		/* EFI_NOT_FOUND */
	EACCES,		/* EFI_ACCESS_DENIED */
	ETIMEDOUT,	/* EFI_NO_RESPONSE */
	EADDRNOTAVAIL,	/* EFI_NO_MAPPING */
	ETIMEDOUT,	/* EFI_TIMEOUT */
	EDOOFUS,	/* EFI_NOT_STARTED */
	EALREADY,	/* EFI_ALREADY_STARTED */
	ECANCELED,	/* EFI_ABORTED */
	EPROTO,		/* EFI_ICMP_ERROR */
	EPROTO,		/* EFI_TFTP_ERROR */
	EPROTO		/* EFI_PROTOCOL_ERROR */
};

static int
efi_status_to_errno(efi_status status)
{
	u_long code;

	code = status & 0x3ffffffffffffffful;
	return (code < nitems(efi_status2err) ? efi_status2err[code] : EDOOFUS);
}

static struct mtx efi_lock;
static pml4_entry_t *efi_pml4;
static vm_object_t obj_1t1_pt;
static vm_page_t efi_pml4_page;

static void
efi_destroy_1t1_map(void)
{
	vm_page_t m;

	if (obj_1t1_pt != NULL) {
		VM_OBJECT_RLOCK(obj_1t1_pt);
		TAILQ_FOREACH(m, &obj_1t1_pt->memq, listq)
			m->wire_count = 0;
		atomic_subtract_int(&vm_cnt.v_wire_count,
		    obj_1t1_pt->resident_page_count);
		VM_OBJECT_RUNLOCK(obj_1t1_pt);
		vm_object_deallocate(obj_1t1_pt);
	}

	obj_1t1_pt = NULL;
	efi_pml4 = NULL;
	efi_pml4_page = NULL;
}

static vm_page_t
efi_1t1_page(vm_pindex_t idx)
{

	return (vm_page_grab(obj_1t1_pt, idx, VM_ALLOC_NOBUSY |
	    VM_ALLOC_WIRED | VM_ALLOC_ZERO));
}

static pt_entry_t *
efi_1t1_pte(vm_offset_t va)
{
	pml4_entry_t *pml4e;
	pdp_entry_t *pdpe;
	pd_entry_t *pde;
	pt_entry_t *pte;
	vm_page_t m;
	vm_pindex_t pml4_idx, pdp_idx, pd_idx;
	vm_paddr_t mphys;

	pml4_idx = pmap_pml4e_index(va);
	pml4e = &efi_pml4[pml4_idx];
	if (*pml4e == 0) {
		m = efi_1t1_page(1 + pml4_idx);
		mphys =  VM_PAGE_TO_PHYS(m);
		*pml4e = mphys | X86_PG_RW | X86_PG_V;
	} else {
		mphys = *pml4e & ~PAGE_MASK;
	}

	pdpe = (pdp_entry_t *)PHYS_TO_DMAP(mphys);
	pdp_idx = pmap_pdpe_index(va);
	pdpe += pdp_idx;
	if (*pdpe == 0) {
		m = efi_1t1_page(1 + NPML4EPG + (pml4_idx + 1) * (pdp_idx + 1));
		mphys =  VM_PAGE_TO_PHYS(m);
		*pdpe = mphys | X86_PG_RW | X86_PG_V;
	} else {
		mphys = *pdpe & ~PAGE_MASK;
	}

	pde = (pd_entry_t *)PHYS_TO_DMAP(mphys);
	pd_idx = pmap_pde_index(va);
	pde += pd_idx;
	if (*pde == 0) {
		m = efi_1t1_page(1 + NPML4EPG + NPML4EPG * NPDPEPG +
		    (pml4_idx + 1) * (pdp_idx + 1) * (pd_idx + 1));
		mphys = VM_PAGE_TO_PHYS(m);
		*pde = mphys | X86_PG_RW | X86_PG_V;
	} else {
		mphys = *pde & ~PAGE_MASK;
	}

	pte = (pt_entry_t *)PHYS_TO_DMAP(mphys);
	pte += pmap_pte_index(va);
	KASSERT(*pte == 0, ("va %#jx *pt %#jx", va, *pte));

	return (pte);
}

static bool
efi_create_1t1_map(struct efi_md *map, int ndesc, int descsz)
{
	struct efi_md *p;
	pt_entry_t *pte;
	vm_offset_t va;
	uint64_t idx;
	int bits, i, mode;

	obj_1t1_pt = vm_pager_allocate(OBJT_PHYS, NULL, 1 + NPML4EPG +
	    NPML4EPG * NPDPEPG + NPML4EPG * NPDPEPG * NPDEPG,
	    VM_PROT_ALL, 0, NULL);
	VM_OBJECT_WLOCK(obj_1t1_pt);
	efi_pml4_page = efi_1t1_page(0);
	VM_OBJECT_WUNLOCK(obj_1t1_pt);
	efi_pml4 = (pml4_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(efi_pml4_page));
	pmap_pinit_pml4(efi_pml4_page);

	for (i = 0, p = map; i < ndesc; i++, p = efi_next_descriptor(p,
	    descsz)) {
		if ((p->md_attr & EFI_MD_ATTR_RT) == 0)
			continue;
		if (p->md_virt != NULL) {
			if (bootverbose)
				printf("EFI Runtime entry %d is mapped\n", i);
			goto fail;
		}
		if ((p->md_phys & EFI_PAGE_MASK) != 0) {
			if (bootverbose)
				printf("EFI Runtime entry %d is not aligned\n",
				    i);
			goto fail;
		}
		if (p->md_phys + p->md_pages * EFI_PAGE_SIZE < p->md_phys ||
		    p->md_phys + p->md_pages * EFI_PAGE_SIZE >=
		    VM_MAXUSER_ADDRESS) {
			printf("EFI Runtime entry %d is not in mappable for RT:"
			    "base %#016jx %#jx pages\n",
			    i, (uintmax_t)p->md_phys,
			    (uintmax_t)p->md_pages);
			goto fail;
		}
		if ((p->md_attr & EFI_MD_ATTR_WB) != 0)
			mode = VM_MEMATTR_WRITE_BACK;
		else if ((p->md_attr & EFI_MD_ATTR_WT) != 0)
			mode = VM_MEMATTR_WRITE_THROUGH;
		else if ((p->md_attr & EFI_MD_ATTR_WC) != 0)
			mode = VM_MEMATTR_WRITE_COMBINING;
		else if ((p->md_attr & EFI_MD_ATTR_WP) != 0)
			mode = VM_MEMATTR_WRITE_PROTECTED;
		else if ((p->md_attr & EFI_MD_ATTR_UC) != 0)
			mode = VM_MEMATTR_UNCACHEABLE;
		else {
			if (bootverbose)
				printf("EFI Runtime entry %d mapping "
				    "attributes unsupported\n", i);
			mode = VM_MEMATTR_UNCACHEABLE;
		}
		bits = pmap_cache_bits(kernel_pmap, mode, FALSE) | X86_PG_RW |
		    X86_PG_V;
		VM_OBJECT_WLOCK(obj_1t1_pt);
		for (va = p->md_phys, idx = 0; idx < p->md_pages; idx++,
		    va += PAGE_SIZE) {
			pte = efi_1t1_pte(va);
			pte_store(pte, va | bits);
		}
		VM_OBJECT_WUNLOCK(obj_1t1_pt);
	}

	return (true);

fail:
	efi_destroy_1t1_map();
	return (false);
}

/*
 * Create an environment for the EFI runtime code call.  The most
 * important part is creating the required 1:1 physical->virtual
 * mappings for the runtime segments.  To do that, we manually create
 * page table which unmap userspace but gives correct kernel mapping.
 * The 1:1 mappings for runtime segments usually occupy low 4G of the
 * physical address map.
 *
 * The 1:1 mappings were chosen over the SetVirtualAddressMap() EFI RT
 * service, because there are some BIOSes which fail to correctly
 * relocate itself on the call, requiring both 1:1 and virtual
 * mapping.  As result, we must provide 1:1 mapping anyway, so no
 * reason to bother with the virtual map, and no need to add a
 * complexity into loader.
 *
 * The fpu_kern_enter() call allows firmware to use FPU, as mandated
 * by the specification.  In particular, CR0.TS bit is cleared.  Also
 * it enters critical section, giving us neccessary protection against
 * context switch.
 *
 * There is no need to disable interrupts around the change of %cr3,
 * the kernel mappings are correct, while we only grabbed the
 * userspace portion of VA.  Interrupts handlers must not access
 * userspace.  Having interrupts enabled fixes the issue with
 * firmware/SMM long operation, which would negatively affect IPIs,
 * esp. TLB shootdown requests.
 */
static int
efi_enter(void)
{
	pmap_t curpmap;
	int error;

	if (efi_runtime == NULL)
		return (ENXIO);
	curpmap = PCPU_GET(curpmap);
	PMAP_LOCK(curpmap);
	mtx_lock(&efi_lock);
	error = fpu_kern_enter(curthread, NULL, FPU_KERN_NOCTX);
	if (error != 0) {
		PMAP_UNLOCK(curpmap);
		return (error);
	}
	load_cr3(VM_PAGE_TO_PHYS(efi_pml4_page) | (pmap_pcid_enabled ?
	    curpmap->pm_pcids[PCPU_GET(cpuid)].pm_pcid : 0));
	/*
	 * If PCID is enabled, the clear CR3_PCID_SAVE bit in the loaded %cr3
	 * causes TLB invalidation.
	 */
	if (!pmap_pcid_enabled)
		invltlb();
	return (0);
}

static void
efi_leave(void)
{
	pmap_t curpmap;

	curpmap = PCPU_GET(curpmap);
	load_cr3(curpmap->pm_cr3 | (pmap_pcid_enabled ?
	    curpmap->pm_pcids[PCPU_GET(cpuid)].pm_pcid : 0));
	if (!pmap_pcid_enabled)
		invltlb();

	fpu_kern_leave(curthread, NULL);
	mtx_unlock(&efi_lock);
	PMAP_UNLOCK(curpmap);
}

static int
efi_init(void)
{
	struct efi_map_header *efihdr;
	struct efi_md *map;
	caddr_t kmdp;
	size_t efisz;

	mtx_init(&efi_lock, "efi", NULL, MTX_DEF);

	if (efi_systbl_phys == 0) {
		if (bootverbose)
			printf("EFI systbl not available\n");
		return (ENXIO);
	}
	efi_systbl = (struct efi_systbl *)PHYS_TO_DMAP(efi_systbl_phys);
	if (efi_systbl->st_hdr.th_sig != EFI_SYSTBL_SIG) {
		efi_systbl = NULL;
		if (bootverbose)
			printf("EFI systbl signature invalid\n");
		return (ENXIO);
	}
	efi_cfgtbl = (efi_systbl->st_cfgtbl == 0) ? NULL :
	    (struct efi_cfgtbl *)efi_systbl->st_cfgtbl;
	if (efi_cfgtbl == NULL) {
		if (bootverbose)
			printf("EFI config table is not present\n");
	}

	kmdp = preload_search_by_type("elf kernel");
	if (kmdp == NULL)
		kmdp = preload_search_by_type("elf64 kernel");
	efihdr = (struct efi_map_header *)preload_search_info(kmdp,
	    MODINFO_METADATA | MODINFOMD_EFI_MAP);
	if (efihdr == NULL) {
		if (bootverbose)
			printf("EFI map is not present\n");
		return (ENXIO);
	}
	efisz = (sizeof(struct efi_map_header) + 0xf) & ~0xf;
	map = (struct efi_md *)((uint8_t *)efihdr + efisz);
	if (efihdr->descriptor_size == 0)
		return (ENOMEM);

	if (!efi_create_1t1_map(map, efihdr->memory_size /
	    efihdr->descriptor_size, efihdr->descriptor_size)) {
		if (bootverbose)
			printf("EFI cannot create runtime map\n");
		return (ENOMEM);
	}

	efi_runtime = (efi_systbl->st_rt == 0) ? NULL :
	    (struct efi_rt *)efi_systbl->st_rt;
	if (efi_runtime == NULL) {
		if (bootverbose)
			printf("EFI runtime services table is not present\n");
		efi_destroy_1t1_map();
		return (ENXIO);
	}

	return (0);
}

static void
efi_uninit(void)
{

	efi_destroy_1t1_map();

	efi_systbl = NULL;
	efi_cfgtbl = NULL;
	efi_runtime = NULL;

	mtx_destroy(&efi_lock);
}

int
efi_get_table(struct uuid *uuid, void *ptr)
{
	struct efi_cfgtbl *ct;
	u_long count;

	if (efi_cfgtbl == NULL)
		return (ENXIO);
	count = efi_systbl->st_entries;
	ct = efi_cfgtbl;
	while (count--) {
		if (!bcmp(&ct->ct_uuid, uuid, sizeof(*uuid))) {
			ptr = (void *)PHYS_TO_DMAP(ct->ct_data);
			return (0);
		}
		ct++;
	}
	return (ENOENT);
}

int
efi_get_time_locked(struct efi_tm *tm)
{
	efi_status status;
	int error;

	mtx_assert(&resettodr_lock, MA_OWNED);
	error = efi_enter();
	if (error != 0)
		return (error);
	status = efi_runtime->rt_gettime(tm, NULL);
	efi_leave();
	error = efi_status_to_errno(status);
	return (error);
}

int
efi_get_time(struct efi_tm *tm)
{
	int error;

	if (efi_runtime == NULL)
		return (ENXIO);
	mtx_lock(&resettodr_lock);
	error = efi_get_time_locked(tm);
	mtx_unlock(&resettodr_lock);
	return (error);
}

int
efi_reset_system(void)
{
	int error;

	error = efi_enter();
	if (error != 0)
		return (error);
	efi_runtime->rt_reset(EFI_RESET_WARM, 0, 0, NULL);
	efi_leave();
	return (EIO);
}

int
efi_set_time_locked(struct efi_tm *tm)
{
	efi_status status;
	int error;

	mtx_assert(&resettodr_lock, MA_OWNED);
	error = efi_enter();
	if (error != 0)
		return (error);
	status = efi_runtime->rt_settime(tm);
	efi_leave();
	error = efi_status_to_errno(status);
	return (error);
}

int
efi_set_time(struct efi_tm *tm)
{
	int error;

	if (efi_runtime == NULL)
		return (ENXIO);
	mtx_lock(&resettodr_lock);
	error = efi_set_time_locked(tm);
	mtx_unlock(&resettodr_lock);
	return (error);
}

int
efi_var_get(efi_char *name, struct uuid *vendor, uint32_t *attrib,
    size_t *datasize, void *data)
{
	efi_status status;
	int error;

	error = efi_enter();
	if (error != 0)
		return (error);
	status = efi_runtime->rt_getvar(name, vendor, attrib, datasize, data);
	efi_leave();
	error = efi_status_to_errno(status);
	return (error);
}

int
efi_var_nextname(size_t *namesize, efi_char *name, struct uuid *vendor)
{
	efi_status status;
	int error;

	error = efi_enter();
	if (error != 0)
		return (error);
	status = efi_runtime->rt_scanvar(namesize, name, vendor);
	efi_leave();
	error = efi_status_to_errno(status);
	return (error);
}

int
efi_var_set(efi_char *name, struct uuid *vendor, uint32_t attrib,
    size_t datasize, void *data)
{
	efi_status status;
	int error;

	error = efi_enter();
	if (error != 0)
		return (error);
	status = efi_runtime->rt_setvar(name, vendor, attrib, datasize, data);
	efi_leave();
	error = efi_status_to_errno(status);
	return (error);
}

static int
efirt_modevents(module_t m, int event, void *arg __unused)
{

	switch (event) {
	case MOD_LOAD:
		return (efi_init());
		break;

	case MOD_UNLOAD:
		efi_uninit();
		return (0);

	case MOD_SHUTDOWN:
		return (0);

	default:
		return (EOPNOTSUPP);
	}
}

static moduledata_t efirt_moddata = {
	.name = "efirt",
	.evhand = efirt_modevents,
	.priv = NULL,
};
DECLARE_MODULE(efirt, efirt_moddata, SI_SUB_VM_CONF, SI_ORDER_ANY);
MODULE_VERSION(efirt, 1);

/* XXX debug stuff */
static int
efi_time_sysctl_handler(SYSCTL_HANDLER_ARGS)
{
	struct efi_tm tm;
	int error, val;

	val = 0;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	error = efi_get_time(&tm);
	if (error == 0) {
		uprintf("EFI reports: Year %d Month %d Day %d Hour %d Min %d "
		    "Sec %d\n", tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour,
		    tm.tm_min, tm.tm_sec);
	}
	return (error);
}

SYSCTL_PROC(_debug, OID_AUTO, efi_time, CTLTYPE_INT | CTLFLAG_RW, NULL, 0,
    efi_time_sysctl_handler, "I", "");
