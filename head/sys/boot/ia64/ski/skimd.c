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
__FBSDID("$FreeBSD$");

#include <stand.h>

#include <libia64.h>

#include "libski.h"

extern void acpi_stub_init(void);
extern void efi_stub_init(struct bootinfo *);
extern void sal_stub_init(void);

vm_paddr_t
ia64_platform_alloc(vm_offset_t va, vm_size_t sz __unused)
{
	vm_paddr_t pa;

	if (va == 0)
		pa = 2 * 1024 * 1024;
	else
		pa = (va - IA64_PBVM_BASE) + (32 * 1024 * 1024);

	return (pa);
}

void
ia64_platform_free(vm_offset_t va __unused, vm_paddr_t pa __unused,
    vm_size_t sz __unused)
{
}

int
ia64_platform_bootinfo(struct bootinfo *bi, struct bootinfo **res)
{
	static struct bootinfo bootinfo;

	efi_stub_init(bi);
	sal_stub_init();
	acpi_stub_init();

	if (IS_LEGACY_KERNEL())
		*res = &bootinfo;

	return (0);
}

int
ia64_platform_enter(const char *kernel)
{

	while (*kernel == '/')
		kernel++;
	ssc(0, (uint64_t)kernel, 0, 0, SSC_LOAD_SYMBOLS);
	return (0);
}
