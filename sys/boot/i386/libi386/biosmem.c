/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
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
__FBSDID("$FreeBSD$");

/*
 * Obtain memory configuration information from the BIOS
 */
#include <stand.h>
#include <machine/pc/bios.h>
#include "libi386.h"
#include "btxv86.h"

vm_offset_t	memtop, memtop_copyin;
u_int32_t	bios_basemem, bios_extmem;

static struct bios_smap smap;

void
bios_getmem(void)
{

    /* Parse system memory map */
    v86.ebx = 0;
    do {
	v86.ctl = V86_FLAGS;
	v86.addr = 0x15;		/* int 0x15 function 0xe820*/
	v86.eax = 0xe820;
	v86.ecx = sizeof(struct bios_smap);
	v86.edx = SMAP_SIG;
	v86.es = VTOPSEG(&smap);
	v86.edi = VTOPOFF(&smap);
	v86int();
	if ((v86.efl & 1) || (v86.eax != SMAP_SIG))
	    break;
	/* look for a low-memory segment that's large enough */
	if ((smap.type == SMAP_TYPE_MEMORY) && (smap.base == 0) &&
	    (smap.length >= (512 * 1024)))
	    bios_basemem = smap.length;
	/* look for the first segment in 'extended' memory */
	if ((smap.type == SMAP_TYPE_MEMORY) && (smap.base == 0x100000)) {
	    bios_extmem = smap.length;
	}
    } while (v86.ebx != 0);

    /* Fall back to the old compatibility function for base memory */
    if (bios_basemem == 0) {
	v86.ctl = 0;
	v86.addr = 0x12;		/* int 0x12 */
	v86int();
	
	bios_basemem = (v86.eax & 0xffff) * 1024;
    }

    /* Fall back through several compatibility functions for extended memory */
    if (bios_extmem == 0) {
	v86.ctl = V86_FLAGS;
	v86.addr = 0x15;		/* int 0x15 function 0xe801*/
	v86.eax = 0xe801;
	v86int();
	if (!(v86.efl & 1)) {
	    bios_extmem = ((v86.ecx & 0xffff) + ((v86.edx & 0xffff) * 64)) * 1024;
	}
    }
    if (bios_extmem == 0) {
	v86.ctl = 0;
	v86.addr = 0x15;		/* int 0x15 function 0x88*/
	v86.eax = 0x8800;
	v86int();
	bios_extmem = (v86.eax & 0xffff) * 1024;
    }

    /* Set memtop to actual top of memory */
    memtop = memtop_copyin = 0x100000 + bios_extmem;

}    

