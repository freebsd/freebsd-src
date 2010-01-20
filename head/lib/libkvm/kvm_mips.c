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

#include "kvm_private.h"

void
_kvm_freevtop(kvm_t *kd)
{

	_kvm_err(kd, 0, "Unimplemented function");
}

int
_kvm_initvtop(kvm_t *kd)
{

	_kvm_err(kd, 0, "Unimplemented function");
	return (0);
}

int
_kvm_kvatop(kvm_t *kd, u_long va __unused, off_t *pa __unused)
{

	_kvm_err(kd, 0, "Unimplemented function");
	return (0);
}

/*
 * Machine-dependent initialization for ALL open kvm descriptors,
 * not just those for a kernel crash dump.  Some architectures
 * have to deal with these NOT being constants!  (i.e. m68k)
 */
int
_kvm_mdopen(kd)
	kvm_t	*kd;
{

	_kvm_err(kd, 0, "Unimplemented function");
	return (0);
}
