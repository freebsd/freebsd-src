/*-
 * Copyright (c) 2006 Marcel Moolenaar
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/boot/ia64/ski/skimd.c,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <stand.h>

#include <libia64.h>

#include "libski.h"

#define	PHYS_START	(4L*1024*1024*1024)
#define	PHYS_SIZE	(64L*1024*1024 - 4L*1024)

extern void acpi_stub_init(void);
extern void efi_stub_init(struct bootinfo *);
extern void sal_stub_init(void);

uint64_t
ldr_alloc(vm_offset_t va)
{

	if (va >= PHYS_SIZE)
		return (0);
	return (va + PHYS_START);
}

int
ldr_bootinfo(struct bootinfo *bi, uint64_t *bi_addr)
{
	static struct bootinfo bootinfo;

	efi_stub_init(bi);
	sal_stub_init();
	acpi_stub_init();

	*bi_addr = (uint64_t)(&bootinfo);
	bootinfo = *bi;
	return (0);
}

int
ldr_enter(const char *kernel)
{

	while (*kernel == '/')
		kernel++;
        ssc(0, (uint64_t)kernel, 0, 0, SSC_LOAD_SYMBOLS);
	return (0);
}
