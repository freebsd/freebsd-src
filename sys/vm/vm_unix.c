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
 * $FreeBSD$
 */

/*
 * Traditional sbrk/grow interface to VM
 */
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sysproto.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#ifndef _SYS_SYSPROTO_H_
struct obreak_args {
	char *nsize;
};
#endif

/* ARGSUSED */
int
obreak(p, uap)
	struct proc *p;
	struct obreak_args *uap;
{
	register struct vmspace *vm = p->p_vmspace;
	vm_offset_t new, old, base;
	int rv;

	base = round_page((vm_offset_t) vm->vm_daddr);
	new = round_page((vm_offset_t)uap->nsize);
	old = base + ctob(vm->vm_dsize);
	if (new > base) {
		/*
		 * We check resource limits here, but alow processes to
		 * reduce their usage, even if they remain over the limit.
		 */
		if (new > old &&
		    (new - base) > (unsigned) p->p_rlimit[RLIMIT_DATA].rlim_cur)
			return ENOMEM;
		if (new >= VM_MAXUSER_ADDRESS)
			return (ENOMEM);
	} else if (new < base) {
		/*
		 * This is simply an invalid value.  If someone wants to
		 * do fancy address space manipulations, mmap and munmap
		 * can do most of what the user would want.
		 */
		return EINVAL;
	}

	if (new > old) {
		vm_size_t diff;

		diff = new - old;
		mtx_lock(&vm_mtx);
		rv = vm_map_find(&vm->vm_map, NULL, 0, &old, diff, FALSE,
			VM_PROT_ALL, VM_PROT_ALL, 0);
		if (rv != KERN_SUCCESS) {
			mtx_unlock(&vm_mtx);
			return (ENOMEM);
		}
		vm->vm_dsize += btoc(diff);
		mtx_unlock(&vm_mtx);
	} else if (new < old) {
		mtx_lock(&Giant);
		mtx_lock(&vm_mtx);
		rv = vm_map_remove(&vm->vm_map, new, old);
		if (rv != KERN_SUCCESS) {
			mtx_unlock(&vm_mtx);
			mtx_unlock(&Giant);
			return (ENOMEM);
		}
		vm->vm_dsize -= btoc(old - new);
		mtx_unlock(&vm_mtx);
		mtx_unlock(&Giant);
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
ovadvise(p, uap)
	struct proc *p;
	struct ovadvise_args *uap;
{

	return (EINVAL);
}
