/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
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
 * from: Utah $Hdr: vm_unix.c 1.1 89/11/07$
 *
 *	@(#)vm_unix.c	8.1 (Berkeley) 6/11/93
 * $Id: vm_unix.c,v 1.7 1995/11/11 06:40:35 bde Exp $
 */

/*
 * Traditional sbrk/grow interface to VM
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>

#include <vm/vm.h>
#include <vm/swap_pager.h>

#ifndef _SYS_SYSPROTO_H_
struct obreak_args {
	char *nsize;
};
#endif

/* ARGSUSED */
int
obreak(p, uap, retval)
	struct proc *p;
	struct obreak_args *uap;
	int *retval;
{
	register struct vmspace *vm = p->p_vmspace;
	vm_offset_t new, old;
	int rv;
	register int diff;

	old = (vm_offset_t) vm->vm_daddr;
	new = round_page(uap->nsize);
	if ((int) (new - old) > p->p_rlimit[RLIMIT_DATA].rlim_cur)
		return (ENOMEM);
	old = round_page(old + ctob(vm->vm_dsize));
	diff = new - old;
	if (diff > 0) {
		if (swap_pager_full) {
			return (ENOMEM);
		}
		rv = vm_map_find(&vm->vm_map, NULL, 0, &old, diff, FALSE);
		if (rv != KERN_SUCCESS) {
			return (ENOMEM);
		}
		vm->vm_dsize += btoc(diff);
	} else if (diff < 0) {
		diff = -diff;
		rv = vm_map_remove(&vm->vm_map, new, new + diff);
		if (rv != KERN_SUCCESS) {
			return (ENOMEM);
		}
		vm->vm_dsize -= btoc(diff);
	}
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct ovadvise_args {
	int anom;
};
#endif

/* ARGSUSED */
int
ovadvise(p, uap, retval)
	struct proc *p;
	struct ovadvise_args *uap;
	int *retval;
{

	return (EINVAL);
}
