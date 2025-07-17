/*-
 * Copyright (C) 2018 Breno Leitao
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/types.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/elf.h>
#include <machine/param.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "opt_md.h"

extern u_char *mfs_root;
extern int mfs_root_size;

static void ofw_initrd_probe_and_attach(void *junk);

SYSINIT(ofw_initrd_probe_and_attach, SI_SUB_KMEM, SI_ORDER_ANY,
    ofw_initrd_probe_and_attach, NULL);

static void
ofw_initrd_probe_and_attach(void *junk)
{
	phandle_t chosen;
	vm_paddr_t start, end;
	pcell_t cell[2];
	ssize_t size;
	u_char *taste;
	Elf_Ehdr ehdr;

	if (!hw_direct_map)
		return;

	chosen = OF_finddevice("/chosen");
	if (chosen <= 0)
		return;

	if (!OF_hasprop(chosen, "linux,initrd-start") ||
	    !OF_hasprop(chosen, "linux,initrd-end"))
		return;

	size = OF_getencprop(chosen, "linux,initrd-start", cell, sizeof(cell));
	if (size == 4)
		start = cell[0];
	else if (size == 8)
		start = (uint64_t)cell[0] << 32 | cell[1];
	else {
		printf("ofw_initrd: Wrong linux,initrd-start size\n");
		return;
	}

	size = OF_getencprop(chosen, "linux,initrd-end", cell, sizeof(cell));
	if (size == 4)
		end = cell[0];
	else if (size == 8)
		end = (uint64_t)cell[0] << 32 | cell[1];
	else{
		printf("ofw_initrd: Wrong linux,initrd-end size\n");
		return;
	}

	if (end - start > 0) {
		taste = (u_char*) PHYS_TO_DMAP(start);
		memcpy(&ehdr, taste, sizeof(ehdr));

		if (IS_ELF(ehdr)) {
			printf("ofw_initrd: initrd is kernel image!\n");
			return;
		}

		mfs_root = taste;
		mfs_root_size = end - start;
		printf("ofw_initrd: initrd loaded at 0x%08lx-0x%08lx\n",
			start, end);
	}
}
