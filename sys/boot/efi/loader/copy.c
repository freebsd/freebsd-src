/*-
 * Copyright (c) 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Benno Rice under sponsorship from
 * the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <stand.h>
#include <bootstrap.h>

#include <efi.h>
#include <efilib.h>

#include "loader_efi.h"

#ifndef EFI_STAGING_SIZE
#define	EFI_STAGING_SIZE	48
#endif

#define	STAGE_PAGES	EFI_SIZE_TO_PAGES((EFI_STAGING_SIZE) * 1024 * 1024)

EFI_PHYSICAL_ADDRESS	staging, staging_end;
int			stage_offset_set = 0;
ssize_t			stage_offset;

int
efi_copy_init(void)
{
	EFI_STATUS	status;

	status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData,
	    STAGE_PAGES, &staging);
	if (EFI_ERROR(status)) {
		printf("failed to allocate staging area: %lu\n",
		    EFI_ERROR_CODE(status));
		return (status);
	}
	staging_end = staging + STAGE_PAGES * EFI_PAGE_SIZE;

#if defined(__aarch64__) || defined(__arm__)
	/*
	 * Round the kernel load address to a 2MiB value. This is needed
	 * because the kernel builds a page table based on where it has
	 * been loaded in physical address space. As the kernel will use
	 * either a 1MiB or 2MiB page for this we need to make sure it
	 * is correctly aligned for both cases.
	 */
	staging = roundup2(staging, 2 * 1024 * 1024);
#endif

	return (0);
}

void *
efi_translate(vm_offset_t ptr)
{

	return ((void *)(ptr + stage_offset));
}

ssize_t
efi_copyin(const void *src, vm_offset_t dest, const size_t len)
{

	if (!stage_offset_set) {
		stage_offset = (vm_offset_t)staging - dest;
		stage_offset_set = 1;
	}

	/* XXX: Callers do not check for failure. */
	if (dest + stage_offset + len > staging_end) {
		errno = ENOMEM;
		return (-1);
	}
	bcopy(src, (void *)(dest + stage_offset), len);
	return (len);
}

ssize_t
efi_copyout(const vm_offset_t src, void *dest, const size_t len)
{

	/* XXX: Callers do not check for failure. */
	if (src + stage_offset + len > staging_end) {
		errno = ENOMEM;
		return (-1);
	}
	bcopy((void *)(src + stage_offset), dest, len);
	return (len);
}


ssize_t
efi_readin(const int fd, vm_offset_t dest, const size_t len)
{

	if (dest + stage_offset + len > staging_end) {
		errno = ENOMEM;
		return (-1);
	}
	return (read(fd, (void *)(dest + stage_offset), len));
}

void
efi_copy_finish(void)
{
	uint64_t	*src, *dst, *last;

	src = (uint64_t *)staging;
	dst = (uint64_t *)(staging - stage_offset);
	last = (uint64_t *)staging_end;

	while (src < last)
		*dst++ = *src++;
}
