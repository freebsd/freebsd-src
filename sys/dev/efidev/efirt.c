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
#include <sys/vmmeter.h>

#include <machine/fpu.h>
#include <machine/efi.h>
#include <machine/metadata.h>
#include <machine/vmparam.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

static struct efi_systbl *efi_systbl;
/*
 * The following pointers point to tables in the EFI runtime service data pages.
 * Care should be taken to make sure that we've properly entered the EFI runtime
 * environment (efi_enter()) before dereferencing them.
 */
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

static int efi_enter(void);
static void efi_leave(void);

static int
efi_status_to_errno(efi_status status)
{
	u_long code;

	code = status & 0x3ffffffffffffffful;
	return (code < nitems(efi_status2err) ? efi_status2err[code] : EDOOFUS);
}

static struct mtx efi_lock;

static bool
efi_is_in_map(struct efi_md *map, int ndesc, int descsz, vm_offset_t addr)
{
	struct efi_md *p;
	int i;

	for (i = 0, p = map; i < ndesc; i++, p = efi_next_descriptor(p,
	    descsz)) {
		if ((p->md_attr & EFI_MD_ATTR_RT) == 0)
			continue;

		if (addr >= (uintptr_t)p->md_virt &&
		    addr < (uintptr_t)p->md_virt + p->md_pages * PAGE_SIZE)
			return (true);
	}

	return (false);
}

static int
efi_init(void)
{
	struct efi_map_header *efihdr;
	struct efi_md *map;
	struct efi_rt *rtdm;
	caddr_t kmdp;
	size_t efisz;
	int ndesc, rt_disabled;

	rt_disabled = 0;
	TUNABLE_INT_FETCH("efi.rt.disabled", &rt_disabled);
	if (rt_disabled == 1)
		return (0);
	mtx_init(&efi_lock, "efi", NULL, MTX_DEF);

	if (efi_systbl_phys == 0) {
		if (bootverbose)
			printf("EFI systbl not available\n");
		return (0);
	}

	efi_systbl = (struct efi_systbl *)efi_phys_to_kva(efi_systbl_phys);
	if (efi_systbl == NULL || efi_systbl->st_hdr.th_sig != EFI_SYSTBL_SIG) {
		efi_systbl = NULL;
		if (bootverbose)
			printf("EFI systbl signature invalid\n");
		return (0);
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
		return (0);
	}
	efisz = (sizeof(struct efi_map_header) + 0xf) & ~0xf;
	map = (struct efi_md *)((uint8_t *)efihdr + efisz);
	if (efihdr->descriptor_size == 0)
		return (ENOMEM);

	ndesc = efihdr->memory_size / efihdr->descriptor_size;
	if (!efi_create_1t1_map(map, ndesc, efihdr->descriptor_size)) {
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

#if defined(__aarch64__) || defined(__amd64__)
	/*
	 * Some UEFI implementations have multiple implementations of the
	 * RS->GetTime function. They switch from one we can only use early
	 * in the boot process to one valid as a RunTime service only when we
	 * call RS->SetVirtualAddressMap. As this is not always the case, e.g.
	 * with an old loader.efi, check if the RS->GetTime function is within
	 * the EFI map, and fail to attach if not.
	 */
	rtdm = (struct efi_rt *)efi_phys_to_kva((uintptr_t)efi_runtime);
	if (rtdm == NULL || !efi_is_in_map(map, ndesc, efihdr->descriptor_size,
	    (vm_offset_t)rtdm->rt_gettime)) {
		if (bootverbose)
			printf(
			 "EFI runtime services table has an invalid pointer\n");
		efi_runtime = NULL;
		efi_destroy_1t1_map();
		return (ENXIO);
	}
#endif

	return (0);
}

static void
efi_uninit(void)
{

	/* Most likely disabled by tunable */
	if (efi_runtime == NULL)
		return;
	efi_destroy_1t1_map();

	efi_systbl = NULL;
	efi_cfgtbl = NULL;
	efi_runtime = NULL;

	mtx_destroy(&efi_lock);
}

int
efi_rt_ok(void)
{

	if (efi_runtime == NULL)
		return (ENXIO);
	return (0);
}

static int
efi_enter(void)
{
	struct thread *td;
	pmap_t curpmap;
	int error;

	if (efi_runtime == NULL)
		return (ENXIO);
	td = curthread;
	curpmap = &td->td_proc->p_vmspace->vm_pmap;
	PMAP_LOCK(curpmap);
	mtx_lock(&efi_lock);
	error = fpu_kern_enter(td, NULL, FPU_KERN_NOCTX);
	if (error != 0) {
		PMAP_UNLOCK(curpmap);
		return (error);
	}

	return (efi_arch_enter());
}

static void
efi_leave(void)
{
	struct thread *td;
	pmap_t curpmap;

	efi_arch_leave();

	curpmap = &curproc->p_vmspace->vm_pmap;
	td = curthread;
	fpu_kern_leave(td, NULL);
	mtx_unlock(&efi_lock);
	PMAP_UNLOCK(curpmap);
}

int
efi_get_table(struct uuid *uuid, void **ptr)
{
	struct efi_cfgtbl *ct;
	u_long count;

	if (efi_cfgtbl == NULL || efi_systbl == NULL)
		return (ENXIO);
	count = efi_systbl->st_entries;
	ct = efi_cfgtbl;
	while (count--) {
		if (!bcmp(&ct->ct_uuid, uuid, sizeof(*uuid))) {
			*ptr = (void *)efi_phys_to_kva(ct->ct_data);
			return (0);
		}
		ct++;
	}
	return (ENOENT);
}

static int
efi_get_time_locked(struct efi_tm *tm, struct efi_tmcap *tmcap)
{
	efi_status status;
	int error;

	EFI_TIME_OWNED();
	error = efi_enter();
	if (error != 0)
		return (error);
	status = efi_runtime->rt_gettime(tm, tmcap);
	efi_leave();
	error = efi_status_to_errno(status);
	return (error);
}

int
efi_get_time(struct efi_tm *tm)
{
	struct efi_tmcap dummy;
	int error;

	if (efi_runtime == NULL)
		return (ENXIO);
	EFI_TIME_LOCK();
	/*
	 * UEFI spec states that the Capabilities argument to GetTime is
	 * optional, but some UEFI implementations choke when passed a NULL
	 * pointer. Pass a dummy efi_tmcap, even though we won't use it,
	 * to workaround such implementations.
	 */
	error = efi_get_time_locked(tm, &dummy);
	EFI_TIME_UNLOCK();
	return (error);
}

int
efi_get_time_capabilities(struct efi_tmcap *tmcap)
{
	struct efi_tm dummy;
	int error;

	if (efi_runtime == NULL)
		return (ENXIO);
	EFI_TIME_LOCK();
	error = efi_get_time_locked(&dummy, tmcap);
	EFI_TIME_UNLOCK();
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

static int
efi_set_time_locked(struct efi_tm *tm)
{
	efi_status status;
	int error;

	EFI_TIME_OWNED();
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
	EFI_TIME_LOCK();
	error = efi_set_time_locked(tm);
	EFI_TIME_UNLOCK();
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
/* After fpuinitstate, before efidev */
DECLARE_MODULE(efirt, efirt_moddata, SI_SUB_DRIVERS, SI_ORDER_SECOND);
MODULE_VERSION(efirt, 1);
