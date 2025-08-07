/*-
 * Copyright (c) 2004 Marcel Moolenaar
 * Copyright (c) 2001 Doug Rabson
 * Copyright (c) 2016, 2018 The FreeBSD Foundation
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
#include "opt_acpi.h"

#include <sys/param.h>
#include <sys/efi.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/msan.h>
#include <sys/mutex.h>
#include <sys/clock.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/vmmeter.h>

#include <machine/fpu.h>
#include <machine/efi.h>
#include <machine/metadata.h>
#include <machine/vmparam.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#ifdef DEV_ACPI
#include <contrib/dev/acpica/include/acpi.h>
#endif

#define EFI_TABLE_ALLOC_MAX 0x800000

static struct efi_systbl *efi_systbl;
static eventhandler_tag efi_shutdown_tag;
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

enum efi_table_type {
	TYPE_ESRT = 0,
	TYPE_PROP,
	TYPE_MEMORY_ATTR
};

static int efi_enter(void);
static void efi_leave(void);

int
efi_status_to_errno(efi_status status)
{
	u_long code;

	code = status & 0x3ffffffffffffffful;
	return (code < nitems(efi_status2err) ? efi_status2err[code] : EDOOFUS);
}

static struct mtx efi_lock;
SYSCTL_NODE(_hw, OID_AUTO, efi, CTLFLAG_RWTUN | CTLFLAG_MPSAFE, NULL,
    "EFI");
static bool efi_poweroff = true;
SYSCTL_BOOL(_hw_efi, OID_AUTO, poweroff, CTLFLAG_RWTUN, &efi_poweroff, 0,
    "If true, use EFI runtime services to power off in preference to ACPI");
extern int print_efirt_faults;
SYSCTL_INT(_hw_efi, OID_AUTO, print_faults, CTLFLAG_RWTUN,
    &print_efirt_faults, 0,
    "Print fault  information upon trap from EFIRT calls: "
    "0 - never, 1 - once, 2 - always");
extern u_long cnt_efirt_faults;
SYSCTL_ULONG(_hw_efi, OID_AUTO, total_faults, CTLFLAG_RD,
    &cnt_efirt_faults, 0,
    "Total number of faults that occurred during EFIRT calls");

static bool
efi_is_in_map(struct efi_md *map, int ndesc, int descsz, vm_offset_t addr)
{
	struct efi_md *p;
	int i;

	for (i = 0, p = map; i < ndesc; i++, p = efi_next_descriptor(p,
	    descsz)) {
		if ((p->md_attr & EFI_MD_ATTR_RT) == 0)
			continue;

		if (addr >= p->md_virt &&
		    addr < p->md_virt + p->md_pages * EFI_PAGE_SIZE)
			return (true);
	}

	return (false);
}

static void
efi_shutdown_final(void *dummy __unused, int howto)
{

	/*
	 * On some systems, ACPI S5 is missing or does not function properly.
	 * When present, shutdown via EFI Runtime Services instead, unless
	 * disabled.
	 */
	if ((howto & RB_POWEROFF) != 0 && efi_poweroff)
		(void)efi_reset_system(EFI_RESET_SHUTDOWN);
}

static int
efi_init(void)
{
	struct efi_map_header *efihdr;
	struct efi_md *map;
	struct efi_rt *rtdm;
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

	efihdr = (struct efi_map_header *)preload_search_info(preload_kmdp,
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

	/*
	 * We use SHUTDOWN_PRI_LAST - 1 to trigger after IPMI, but before ACPI.
	 */
	efi_shutdown_tag = EVENTHANDLER_REGISTER(shutdown_final,
	    efi_shutdown_final, NULL, SHUTDOWN_PRI_LAST - 1);

	return (0);
}

static void
efi_uninit(void)
{

	/* Most likely disabled by tunable */
	if (efi_runtime == NULL)
		return;
	if (efi_shutdown_tag != NULL)
		EVENTHANDLER_DEREGISTER(shutdown_final, efi_shutdown_tag);
	efi_destroy_1t1_map();

	efi_systbl = NULL;
	efi_cfgtbl = NULL;
	efi_runtime = NULL;

	mtx_destroy(&efi_lock);
}

static int
rt_ok(void)
{

	if (efi_runtime == NULL)
		return (ENXIO);
	return (0);
}

/*
 * The fpu_kern_enter() call in allows firmware to use FPU, as
 * mandated by the specification.  It also enters a critical section,
 * giving us neccessary protection against context switches.
 */
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
	fpu_kern_enter(td, NULL, FPU_KERN_NOCTX);
	error = efi_arch_enter();
	if (error != 0) {
		fpu_kern_leave(td, NULL);
		mtx_unlock(&efi_lock);
		PMAP_UNLOCK(curpmap);
	} else {
		MPASS((td->td_pflags & TDP_EFIRT) == 0);
		td->td_pflags |= TDP_EFIRT;
	}
	return (error);
}

static void
efi_leave(void)
{
	struct thread *td;
	pmap_t curpmap;

	td = curthread;
	MPASS((td->td_pflags & TDP_EFIRT) != 0);
	td->td_pflags &= ~TDP_EFIRT;

	efi_arch_leave();

	curpmap = &curproc->p_vmspace->vm_pmap;
	fpu_kern_leave(td, NULL);
	mtx_unlock(&efi_lock);
	PMAP_UNLOCK(curpmap);
}

static int
get_table(efi_guid_t *guid, void **ptr)
{
	struct efi_cfgtbl *ct;
	u_long count;
	int error;

	if (efi_cfgtbl == NULL || efi_systbl == NULL)
		return (ENXIO);
	error = efi_enter();
	if (error != 0)
		return (error);
	count = efi_systbl->st_entries;
	ct = efi_cfgtbl;
	while (count--) {
		if (!bcmp(&ct->ct_guid, guid, sizeof(*guid))) {
			*ptr = ct->ct_data;
			efi_leave();
			return (0);
		}
		ct++;
	}

	efi_leave();
	return (ENOENT);
}

static int
get_table_length(enum efi_table_type type, size_t *table_len, void **taddr)
{
	switch (type) {
	case TYPE_ESRT:
	{
		struct efi_esrt_table *esrt = NULL;
		efi_guid_t guid = EFI_TABLE_ESRT;
		uint32_t fw_resource_count = 0;
		size_t len = sizeof(*esrt);
		int error;
		void *buf;

		error = efi_get_table(&guid, (void **)&esrt);
		if (error != 0)
			return (error);

		buf = malloc(len, M_TEMP, M_WAITOK);
		error = physcopyout((vm_paddr_t)esrt, buf, len);
		if (error != 0) {
			free(buf, M_TEMP);
			return (error);
		}

		/* Check ESRT version */
		if (((struct efi_esrt_table *)buf)->fw_resource_version !=
		    ESRT_FIRMWARE_RESOURCE_VERSION) {
			free(buf, M_TEMP);
			return (ENODEV);
		}

		fw_resource_count = ((struct efi_esrt_table *)buf)->
		    fw_resource_count;
		if (fw_resource_count > EFI_TABLE_ALLOC_MAX /
		    sizeof(struct efi_esrt_entry_v1)) {
			free(buf, M_TEMP);
			return (ENOMEM);
		}

		len += fw_resource_count * sizeof(struct efi_esrt_entry_v1);
		*table_len = len;

		if (taddr != NULL)
			*taddr = esrt;
		free(buf, M_TEMP);
		return (0);
	}
	case TYPE_PROP:
	{
		efi_guid_t guid = EFI_PROPERTIES_TABLE;
		struct efi_prop_table *prop;
		size_t len = sizeof(*prop);
		uint32_t prop_len;
		int error;
		void *buf;

		error = efi_get_table(&guid, (void **)&prop);
		if (error != 0)
			return (error);

		buf = malloc(len, M_TEMP, M_WAITOK);
		error = physcopyout((vm_paddr_t)prop, buf, len);
		if (error != 0) {
			free(buf, M_TEMP);
			return (error);
		}

		prop_len = ((struct efi_prop_table *)buf)->length;
		if (prop_len > EFI_TABLE_ALLOC_MAX) {
			free(buf, M_TEMP);
			return (ENOMEM);
		}
		*table_len = prop_len;

		if (taddr != NULL)
			*taddr = prop;
		free(buf, M_TEMP);
		return (0);
	}
	case TYPE_MEMORY_ATTR:
	{
		efi_guid_t guid = EFI_MEMORY_ATTRIBUTES_TABLE;
		struct efi_memory_attribute_table *tbl_addr, *mem_addr;
		int error;
		void *buf;
		size_t len = sizeof(struct efi_memory_attribute_table);

		error = efi_get_table(&guid, (void **)&tbl_addr);
		if (error)
			return (error);

		buf = malloc(len, M_TEMP, M_WAITOK);
		error = physcopyout((vm_paddr_t)tbl_addr, buf, len);
		if (error) {
			free(buf, M_TEMP);
			return (error);
		}

		mem_addr = (struct efi_memory_attribute_table *)buf;
		if (mem_addr->version != 2) {
			free(buf, M_TEMP);
			return (EINVAL);
		}
		len += mem_addr->descriptor_size * mem_addr->num_ents;
		if (len > EFI_TABLE_ALLOC_MAX) {
			free(buf, M_TEMP);
			return (ENOMEM);
		}

		*table_len = len;
		if (taddr != NULL)
			*taddr = tbl_addr;
		free(buf, M_TEMP);
		return (0);
	}
	}
	return (ENOENT);
}

static int
copy_table(efi_guid_t *guid, void **buf, size_t buf_len, size_t *table_len)
{
	static const struct known_table {
		efi_guid_t guid;
		enum efi_table_type type;
	} tables[] = {
		{ EFI_TABLE_ESRT,       TYPE_ESRT },
		{ EFI_PROPERTIES_TABLE, TYPE_PROP },
		{ EFI_MEMORY_ATTRIBUTES_TABLE, TYPE_MEMORY_ATTR }
	};
	size_t table_idx;
	void *taddr;
	int rc;

	for (table_idx = 0; table_idx < nitems(tables); table_idx++) {
		if (!bcmp(&tables[table_idx].guid, guid, sizeof(*guid)))
			break;
	}

	if (table_idx == nitems(tables))
		return (EINVAL);

	rc = get_table_length(tables[table_idx].type, table_len, &taddr);
	if (rc != 0)
		return rc;

	/* return table length to userspace */
	if (buf == NULL)
		return (0);

	*buf = malloc(*table_len, M_TEMP, M_WAITOK);
	rc = physcopyout((vm_paddr_t)taddr, *buf, *table_len);
	return (rc);
}

static int efi_rt_handle_faults = EFI_RT_HANDLE_FAULTS_DEFAULT;
SYSCTL_INT(_machdep, OID_AUTO, efi_rt_handle_faults, CTLFLAG_RWTUN,
    &efi_rt_handle_faults, 0,
    "Call EFI RT methods with fault handler wrapper around");

static int
efi_rt_arch_call_nofault(struct efirt_callinfo *ec)
{

	switch (ec->ec_argcnt) {
	case 0:
		ec->ec_efi_status = ((register_t EFIABI_ATTR (*)(void))
		    ec->ec_fptr)();
		break;
	case 1:
		ec->ec_efi_status = ((register_t EFIABI_ATTR (*)(register_t))
		    ec->ec_fptr)(ec->ec_arg1);
		break;
	case 2:
		ec->ec_efi_status = ((register_t EFIABI_ATTR (*)(register_t,
		    register_t))ec->ec_fptr)(ec->ec_arg1, ec->ec_arg2);
		break;
	case 3:
		ec->ec_efi_status = ((register_t EFIABI_ATTR (*)(register_t,
		    register_t, register_t))ec->ec_fptr)(ec->ec_arg1,
		    ec->ec_arg2, ec->ec_arg3);
		break;
	case 4:
		ec->ec_efi_status = ((register_t EFIABI_ATTR (*)(register_t,
		    register_t, register_t, register_t))ec->ec_fptr)(
		    ec->ec_arg1, ec->ec_arg2, ec->ec_arg3, ec->ec_arg4);
		break;
	case 5:
		ec->ec_efi_status = ((register_t EFIABI_ATTR (*)(register_t,
		    register_t, register_t, register_t, register_t))
		    ec->ec_fptr)(ec->ec_arg1, ec->ec_arg2, ec->ec_arg3,
		    ec->ec_arg4, ec->ec_arg5);
		break;
	default:
		panic("efi_rt_arch_call: %d args", (int)ec->ec_argcnt);
	}

	return (0);
}

static int
efi_call(struct efirt_callinfo *ecp)
{
	int error;

	error = efi_enter();
	if (error != 0)
		return (error);
	error = efi_rt_handle_faults ? efi_rt_arch_call(ecp) :
	    efi_rt_arch_call_nofault(ecp);
	efi_leave();
	if (error == 0)
		error = efi_status_to_errno(ecp->ec_efi_status);
	else if (bootverbose)
		printf("EFI %s call faulted, error %d\n", ecp->ec_name, error);
	return (error);
}

#define	EFI_RT_METHOD_PA(method)				\
    ((uintptr_t)((struct efi_rt *)efi_phys_to_kva((uintptr_t)	\
    efi_runtime))->method)

static int
efi_get_time_locked(struct efi_tm *tm, struct efi_tmcap *tmcap)
{
	struct efirt_callinfo ec;
	int error;

	EFI_TIME_OWNED();
	if (efi_runtime == NULL)
		return (ENXIO);
	bzero(&ec, sizeof(ec));
	ec.ec_name = "rt_gettime";
	ec.ec_argcnt = 2;
	ec.ec_arg1 = (uintptr_t)tm;
	ec.ec_arg2 = (uintptr_t)tmcap;
	ec.ec_fptr = EFI_RT_METHOD_PA(rt_gettime);
	error = efi_call(&ec);
	if (error == 0)
		kmsan_mark(tm, sizeof(*tm), KMSAN_STATE_INITED);
	return (error);
}

static int
get_time(struct efi_tm *tm)
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

static int
get_waketime(uint8_t *enabled, uint8_t *pending, struct efi_tm *tm)
{
	struct efirt_callinfo ec;
	int error;
#ifdef DEV_ACPI
	UINT32 acpiRtcEnabled;
#endif

	if (efi_runtime == NULL)
		return (ENXIO);

	EFI_TIME_LOCK();
	bzero(&ec, sizeof(ec));
	ec.ec_name = "rt_getwaketime";
	ec.ec_argcnt = 3;
	ec.ec_arg1 = (uintptr_t)enabled;
	ec.ec_arg2 = (uintptr_t)pending;
	ec.ec_arg3 = (uintptr_t)tm;
	ec.ec_fptr = EFI_RT_METHOD_PA(rt_getwaketime);
	error = efi_call(&ec);
	EFI_TIME_UNLOCK();

#ifdef DEV_ACPI
	if (error == 0) {
		error = AcpiReadBitRegister(ACPI_BITREG_RT_CLOCK_ENABLE,
		    &acpiRtcEnabled);
		if (ACPI_SUCCESS(error)) {
			*enabled = *enabled && acpiRtcEnabled;
		} else
			error = EIO;
	}
#endif

	return (error);
}

static int
set_waketime(uint8_t enable, struct efi_tm *tm)
{
	struct efirt_callinfo ec;
	int error;

	if (efi_runtime == NULL)
		return (ENXIO);

	EFI_TIME_LOCK();
	bzero(&ec, sizeof(ec));
	ec.ec_name = "rt_setwaketime";
	ec.ec_argcnt = 2;
	ec.ec_arg1 = (uintptr_t)enable;
	ec.ec_arg2 = (uintptr_t)tm;
	ec.ec_fptr = EFI_RT_METHOD_PA(rt_setwaketime);
	error = efi_call(&ec);
	EFI_TIME_UNLOCK();

#ifdef DEV_ACPI
	if (error == 0) {
		error = AcpiWriteBitRegister(ACPI_BITREG_RT_CLOCK_ENABLE,
		    (enable != 0) ? 1 : 0);
		if (ACPI_FAILURE(error))
			error = EIO;
	}
#endif

	return (error);
}

static int
get_time_capabilities(struct efi_tmcap *tmcap)
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

static int
reset_system(enum efi_reset type)
{
	struct efirt_callinfo ec;

	switch (type) {
	case EFI_RESET_COLD:
	case EFI_RESET_WARM:
	case EFI_RESET_SHUTDOWN:
		break;
	default:
		return (EINVAL);
	}
	if (efi_runtime == NULL)
		return (ENXIO);
	bzero(&ec, sizeof(ec));
	ec.ec_name = "rt_reset";
	ec.ec_argcnt = 4;
	ec.ec_arg1 = (uintptr_t)type;
	ec.ec_arg2 = (uintptr_t)0;
	ec.ec_arg3 = (uintptr_t)0;
	ec.ec_arg4 = (uintptr_t)NULL;
	ec.ec_fptr = EFI_RT_METHOD_PA(rt_reset);
	return (efi_call(&ec));
}

static int
efi_set_time_locked(struct efi_tm *tm)
{
	struct efirt_callinfo ec;

	EFI_TIME_OWNED();
	if (efi_runtime == NULL)
		return (ENXIO);
	bzero(&ec, sizeof(ec));
	ec.ec_name = "rt_settime";
	ec.ec_argcnt = 1;
	ec.ec_arg1 = (uintptr_t)tm;
	ec.ec_fptr = EFI_RT_METHOD_PA(rt_settime);
	return (efi_call(&ec));
}

static int
set_time(struct efi_tm *tm)
{
	int error;

	if (efi_runtime == NULL)
		return (ENXIO);
	EFI_TIME_LOCK();
	error = efi_set_time_locked(tm);
	EFI_TIME_UNLOCK();
	return (error);
}

static int
var_get(efi_char *name, efi_guid_t *vendor, uint32_t *attrib,
    size_t *datasize, void *data)
{
	struct efirt_callinfo ec;
	int error;

	if (efi_runtime == NULL)
		return (ENXIO);
	bzero(&ec, sizeof(ec));
	ec.ec_argcnt = 5;
	ec.ec_name = "rt_getvar";
	ec.ec_arg1 = (uintptr_t)name;
	ec.ec_arg2 = (uintptr_t)vendor;
	ec.ec_arg3 = (uintptr_t)attrib;
	ec.ec_arg4 = (uintptr_t)datasize;
	ec.ec_arg5 = (uintptr_t)data;
	ec.ec_fptr = EFI_RT_METHOD_PA(rt_getvar);
	error = efi_call(&ec);
	if (error == 0)
		kmsan_mark(data, *datasize, KMSAN_STATE_INITED);
	return (error);
}

static int
var_nextname(size_t *namesize, efi_char *name, efi_guid_t *vendor)
{
	struct efirt_callinfo ec;
	int error;

	if (efi_runtime == NULL)
		return (ENXIO);
	bzero(&ec, sizeof(ec));
	ec.ec_argcnt = 3;
	ec.ec_name = "rt_scanvar";
	ec.ec_arg1 = (uintptr_t)namesize;
	ec.ec_arg2 = (uintptr_t)name;
	ec.ec_arg3 = (uintptr_t)vendor;
	ec.ec_fptr = EFI_RT_METHOD_PA(rt_scanvar);
	error = efi_call(&ec);
	if (error == 0)
		kmsan_mark(name, *namesize, KMSAN_STATE_INITED);
	return (error);
}

static int
var_set(efi_char *name, efi_guid_t *vendor, uint32_t attrib,
    size_t datasize, void *data)
{
	struct efirt_callinfo ec;

	if (efi_runtime == NULL)
		return (ENXIO);
	bzero(&ec, sizeof(ec));
	ec.ec_argcnt = 5;
	ec.ec_name = "rt_setvar";
	ec.ec_arg1 = (uintptr_t)name;
	ec.ec_arg2 = (uintptr_t)vendor;
	ec.ec_arg3 = (uintptr_t)attrib;
	ec.ec_arg4 = (uintptr_t)datasize;
	ec.ec_arg5 = (uintptr_t)data;
	ec.ec_fptr = EFI_RT_METHOD_PA(rt_setvar);
	return (efi_call(&ec));
}

const static struct efi_ops efi_ops = {
	.rt_ok = rt_ok,
	.get_table = get_table,
	.copy_table = copy_table,
	.get_time = get_time,
	.get_time_capabilities = get_time_capabilities,
	.reset_system = reset_system,
	.set_time = set_time,
	.get_waketime = get_waketime,
	.set_waketime = set_waketime,
	.var_get = var_get,
	.var_nextname = var_nextname,
	.var_set = var_set,
};
const struct efi_ops *active_efi_ops = &efi_ops;

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
