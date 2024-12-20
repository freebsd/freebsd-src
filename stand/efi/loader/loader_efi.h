/*-
 * Copyright (c) 2013 The FreeBSD Foundation
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

#ifndef	_LOADER_EFI_COPY_H_
#define	_LOADER_EFI_COPY_H_

#include <stand.h>
#include <readin.h>
#include <efi.h>

#if defined(__amd64__) || defined(__i386__)
enum {
	COPY_STAGING_ENABLE,
	COPY_STAGING_DISABLE,
	COPY_STAGING_AUTO,
};
extern int copy_staging;
#endif

/* Useful for various calculations */
#define	M(x)	((x) * 1024 * 1024)
#define	G(x)	(1ULL * (x) * 1024 * 1024 * 1024)

extern EFI_LOADED_IMAGE *boot_img;

int	efi_autoload(void);

int	efi_copy_init(void);

ssize_t	efi_copyin(const void *src, vm_offset_t dest, const size_t len);
ssize_t	efi_copyout(const vm_offset_t src, void *dest, const size_t len);
ssize_t	efi_readin(readin_handle_t fd, vm_offset_t dest, const size_t len);
void * efi_translate(vm_offset_t ptr);

void	efi_copy_finish(void);
void	efi_copy_finish_nop(void);

#if defined(__amd64__) || defined(__i386__)
/* Need this to setup page tables */
extern EFI_PHYSICAL_ADDRESS staging;
#endif

int bi_load(char *args, vm_offset_t *modulep, vm_offset_t *kernendp,
    bool exit_bs);

#endif	/* _LOADER_EFI_COPY_H_ */
