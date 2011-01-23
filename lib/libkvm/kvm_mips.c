/*-
 * Copyright (C) 2006 Bruce M. Simpson.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bruce M. Simpson.
 * 4. The name of Bruce M. Simpson may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRUCE M. SIMPSON ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRUCE M. SIMPSON BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * MIPS machine dependent routines for kvm.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/elf32.h>
#include <sys/mman.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <machine/pmap.h>

#include <db.h>
#include <limits.h>
#include <kvm.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kvm_private.h"

/* minidump must be the first item! */
struct vmstate {
	int minidump;		/* 1 = minidump mode */
	void *mmapbase;
	size_t mmapsize;
};

void
_kvm_freevtop(kvm_t *kd)
{
	if (kd->vmst != 0) {
		if (kd->vmst->minidump)
			return (_kvm_minidump_freevtop(kd));
		if (kd->vmst->mmapbase != NULL)
			munmap(kd->vmst->mmapbase, kd->vmst->mmapsize);
		free(kd->vmst);
		kd->vmst = NULL;
	}
}

int
_kvm_initvtop(kvm_t *kd)
{
	char minihdr[8];

	if (!kd->rawdump) {
		if (pread(kd->pmfd, &minihdr, 8, 0) == 8) {
			if (memcmp(&minihdr, "minidump", 8) == 0)
				return (_kvm_minidump_initvtop(kd));
		} else {
			_kvm_err(kd, kd->program, "cannot read header");
			return (-1);
		}
	}

	_kvm_err(kd, 0, "_kvm_initvtop: Unsupported image type");
	return (-1);
}

int
_kvm_kvatop(kvm_t *kd, u_long va, off_t *pa)
{

	if (kd->vmst->minidump)
		return _kvm_minidump_kvatop(kd, va, pa);


	_kvm_err(kd, 0, "_kvm_kvatop: Unsupported image type");
	return (0);
}

/*
 * Machine-dependent initialization for ALL open kvm descriptors,
 * not just those for a kernel crash dump.  Some architectures
 * have to deal with these NOT being constants!  (i.e. m68k)
 */
#ifdef FBSD_NOT_YET
int
_kvm_mdopen(kvm_t *kd __unused)
{

	return (0);
}
#endif
