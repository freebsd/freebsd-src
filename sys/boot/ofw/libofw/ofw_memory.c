/*
 * Copyright (c) 2001 Benno Rice
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
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

#include <sys/param.h>
#include <sys/types.h>

#include <stand.h>

#include "libofw.h"
#include "openfirm.h"

static void		*heap_base = 0;
static unsigned int	heap_size = 0;

struct ofw_mapping {
        vm_offset_t     va;
        int             len;
        vm_offset_t     pa;
        int             mode;
};

void
ofw_memmap(void)
{
        phandle_t       mmup;
        int             nmapping, i;
        struct          ofw_mapping mappings[256];
   
        mmup = OF_instance_to_package(mmu);
 
        bzero(mappings, sizeof(mappings));
 
        nmapping = OF_getprop(mmup, "translations", mappings, sizeof(mappings));
	if (nmapping == -1) {
		printf("Could not get memory map (%d)\n",
		    nmapping);
		return;
	}
        nmapping /= sizeof(struct ofw_mapping);

        printf("%17s %17s %8s %6s\n", "Virtual Range", "Physical Range",
            "#Pages", "Mode");

        for (i = 0; i < nmapping; i++) {
                printf("%08x-%08x %08x-%08x %8d %6x\n", mappings[i].pa,
                    mappings[i].pa + mappings[i].len, mappings[i].va,
                    mappings[i].va + mappings[i].len, mappings[i].len / 0x1000,
                    mappings[i].mode);
	}
}

void *
ofw_alloc_heap(unsigned int size)
{
	phandle_t	memoryp;
	struct		ofw_reg available;
	void		*base;

	memoryp = OF_instance_to_package(memory);
	OF_getprop(memoryp, "available", &available, sizeof(available));

	heap_base = OF_claim((void *)available.base, size, sizeof(register_t));

	if (heap_base != (void *)-1) {
		heap_size = size;
	}

	return (heap_base);
}

void
ofw_release_heap(void)
{
	OF_release(heap_base, heap_size);
}
