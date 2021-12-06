/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Vladimir Kondratyev <wulf@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LINUX_EFI_H_
#define	_LINUX_EFI_H_

#include <sys/param.h>
#include <sys/linker.h>
#include <sys/systm.h>

#include <machine/md_var.h>
#include <machine/metadata.h>

#define	EFI_BOOT		0

static inline bool
__efi_enabled(int feature)
{
#if defined(MODINFOMD_EFI_MAP) && !defined(__amd64__)
	caddr_t kmdp;
#endif
	bool enabled = false;

	switch (feature) {
	case EFI_BOOT:
#ifdef __amd64__
		/* Use cached value on amd64 */
		enabled = efi_boot;
#elif defined(MODINFOMD_EFI_MAP)
		kmdp = preload_search_by_type("elf kernel");
		if (kmdp == NULL)
			kmdp = preload_search_by_type("elf64 kernel");
		enabled = preload_search_info(kmdp,
		    MODINFO_METADATA | MODINFOMD_EFI_MAP) != NULL;
#endif
		break;
	default:
		break;
	}

	return (enabled);
}

#define	efi_enabled(x)	({					\
	_Static_assert((x) == EFI_BOOT, "unsupported feature");	\
	__efi_enabled(x);					\
})

#endif	/* _LINUX_EFI_H_ */
