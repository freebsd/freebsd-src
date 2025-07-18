/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2002 Marcel Moolenaar
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
#include "opt_watchdog.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kerneldump.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#ifdef __amd64__
#include <machine/elf.h>
#include <machine/vmparam.h>
#include <machine/md_var.h>
#include <machine/dump.h>
#endif

int do_minidump = 1;
SYSCTL_INT(_debug, OID_AUTO, minidump, CTLFLAG_RWTUN, &do_minidump, 0,
    "Enable mini crash dumps");

void
dumpsys_map_chunk(vm_paddr_t pa, size_t chunk, void **va)
{
	int i;
	vm_paddr_t a;

	for (i = 0; i < chunk; i++) {
		a = pa + i * PAGE_SIZE;
		*va = pmap_kenter_temporary(trunc_page(a), i);
	}
}

#ifdef __amd64__
int
dumpsys_write_aux_headers(struct dumperinfo *di)
{
	Elf_Phdr phdr;

	phdr.p_type = PT_DUMP_DELTA;
	phdr.p_flags = PF_R;
	phdr.p_offset = 0;
	phdr.p_vaddr = KERNBASE;
	phdr.p_paddr = kernphys;
	phdr.p_filesz = 0;
	phdr.p_memsz = 0;
	phdr.p_align = KERNLOAD;

	return (dumpsys_buf_write(di, (char *)&phdr, sizeof(phdr)));
}
#endif
