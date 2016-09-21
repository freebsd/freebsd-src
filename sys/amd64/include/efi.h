/*-
 * Copyright (c) 2016 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef __AMD64_INCLUDE_EFI_H_
#define __AMD64_INCLUDE_EFI_H_

/*
 * XXX: from gcc 6.2 manual:
 * Note, the ms_abi attribute for Microsoft Windows 64-bit targets
 * currently requires the -maccumulate-outgoing-args option.
 */
#define	EFIABI_ATTR	__attribute__((ms_abi))

#ifdef _KERNEL
struct uuid;
struct efi_tm;

int efi_get_table(struct uuid *uuid, void *ptr);
int efi_get_time(struct efi_tm *tm);
int efi_get_time_locked(struct efi_tm *tm);
int efi_reset_system(void);
int efi_set_time(struct efi_tm *tm);
int efi_set_time_locked(struct efi_tm *tm);
int efi_var_get(uint16_t *name, struct uuid *vendor, uint32_t *attrib,
    size_t *datasize, void *data);
int efi_var_nextname(size_t *namesize, uint16_t *name, struct uuid *vendor);
int efi_var_set(uint16_t *name, struct uuid *vendor, uint32_t attrib,
    size_t datasize, void *data);
#endif

#endif /* __AMD64_INCLUDE_EFI_H_ */
