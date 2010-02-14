/*-
 * Copyright (c) 2004 Marcel Moolenaar
 * Copyright (c) 2001 Doug Rabson
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <machine/bootinfo.h>
#include <machine/efi.h>
#include <machine/sal.h>
#include <vm/vm.h>
#include <vm/pmap.h>

extern uint64_t ia64_call_efi_physical(uint64_t, uint64_t, uint64_t, uint64_t,
    uint64_t, uint64_t);

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
	int error;

	code = status & 0x3ffffffffffffffful;
	error = (code < 25) ? efi_status2err[code] : EDOOFUS;
	return (error);
}

void
efi_boot_finish(void)
{
}

/*
 * Collect the entry points for PAL and SAL. Be extra careful about NULL
 * pointer values. We're running pre-console, so it's better to return
 * error values than to cause panics, machine checks and other traps and
 * faults. Keep this minimal...
 */
int
efi_boot_minimal(uint64_t systbl)
{
	struct efi_md *md;
	efi_status status;

	if (systbl == 0)
		return (EINVAL);
	efi_systbl = (struct efi_systbl *)IA64_PHYS_TO_RR7(systbl);
	if (efi_systbl->st_hdr.th_sig != EFI_SYSTBL_SIG) {
		efi_systbl = NULL;
		return (EFAULT);
	}
	efi_cfgtbl = (efi_systbl->st_cfgtbl == 0) ? NULL :
	    (struct efi_cfgtbl *)IA64_PHYS_TO_RR7(efi_systbl->st_cfgtbl);
	if (efi_cfgtbl == NULL)
		return (ENOENT);
	efi_runtime = (efi_systbl->st_rt == 0) ? NULL :
	    (struct efi_rt *)IA64_PHYS_TO_RR7(efi_systbl->st_rt);
	if (efi_runtime == NULL)
		return (ENOENT);

	/*
	 * Relocate runtime memory segments for firmware.
	 */
	md = efi_md_first();
	while (md != NULL) {
		if (md->md_attr & EFI_MD_ATTR_RT) {
			if (md->md_attr & EFI_MD_ATTR_WB)
				md->md_virt =
				    (void *)IA64_PHYS_TO_RR7(md->md_phys);
			else if (md->md_attr & EFI_MD_ATTR_UC)
				md->md_virt = pmap_mapdev(md->md_phys,
				    md->md_pages * EFI_PAGE_SIZE);
		}
		md = efi_md_next(md);
	}
	status = ia64_call_efi_physical((uint64_t)efi_runtime->rt_setvirtual,
	    bootinfo.bi_memmap_size, bootinfo.bi_memdesc_size,
	    bootinfo.bi_memdesc_version, bootinfo.bi_memmap, 0);
	return ((status < 0) ? EFAULT : 0);
}

void *
efi_get_table(struct uuid *uuid)
{
	struct efi_cfgtbl *ct;
	u_long count;

	if (efi_cfgtbl == NULL)
		return (NULL);
	count = efi_systbl->st_entries;
	ct = efi_cfgtbl;
	while (count--) {
		if (!bcmp(&ct->ct_uuid, uuid, sizeof(*uuid)))
			return ((void *)IA64_PHYS_TO_RR7(ct->ct_data));
		ct++;
	}
	return (NULL);
}

void
efi_get_time(struct efi_tm *tm)
{

	efi_runtime->rt_gettime(tm, NULL);
}

struct efi_md *
efi_md_first(void)
{

	if (bootinfo.bi_memmap == 0)
		return (NULL);
	return ((struct efi_md *)IA64_PHYS_TO_RR7(bootinfo.bi_memmap));
}

struct efi_md *
efi_md_next(struct efi_md *md)
{
	uint64_t plim;

	plim = IA64_PHYS_TO_RR7(bootinfo.bi_memmap + bootinfo.bi_memmap_size);
	md = (struct efi_md *)((uintptr_t)md + bootinfo.bi_memdesc_size);
	return ((md >= (struct efi_md *)plim) ? NULL : md);
}

void
efi_reset_system(void)
{

	if (efi_runtime != NULL)
		efi_runtime->rt_reset(EFI_RESET_WARM, 0, 0, NULL);
	panic("%s: unable to reset the machine", __func__);
}

int
efi_set_time(struct efi_tm *tm)
{

	return (efi_status_to_errno(efi_runtime->rt_settime(tm)));
}

int
efi_var_get(efi_char *name, struct uuid *vendor, uint32_t *attrib,
    size_t *datasize, void *data)
{
	efi_status status;

	status = efi_runtime->rt_getvar(name, vendor, attrib, datasize, data);
	return (efi_status_to_errno(status));
}

int
efi_var_nextname(size_t *namesize, efi_char *name, struct uuid *vendor)
{
	efi_status status;

	status = efi_runtime->rt_scanvar(namesize, name, vendor);
	return (efi_status_to_errno(status));
}
 
int
efi_var_set(efi_char *name, struct uuid *vendor, uint32_t attrib,
    size_t datasize, void *data)
{
	efi_status status;
 
	status = efi_runtime->rt_setvar(name, vendor, attrib, datasize, data);
	return (efi_status_to_errno(status));
}
