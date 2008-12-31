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
__FBSDID("$FreeBSD: src/sys/ia64/ia64/efi.c,v 1.5.18.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <machine/bootinfo.h>
#include <machine/efi.h>
#include <machine/sal.h>

extern uint64_t ia64_call_efi_physical(uint64_t, uint64_t, uint64_t, uint64_t,
    uint64_t, uint64_t);

static struct efi_systbl *efi_systbl;
static struct efi_cfgtbl *efi_cfgtbl;
static struct efi_rt *efi_runtime;

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
				md->md_virt =
				    (void *)IA64_PHYS_TO_RR6(md->md_phys);
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
		if (!memcmp(&ct->ct_uuid, uuid, sizeof(*uuid)))
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

efi_status
efi_set_time(struct efi_tm *tm)
{

	return (efi_runtime->rt_settime(tm));
}
