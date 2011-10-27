/*-
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
 */

#include "opt_compat.h"

/*
 * Traditional sbrk/grow interface to VM
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/racct.h>
#include <sys/resourcevar.h>
#include <sys/sysent.h>
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

/*
 * MPSAFE
 */
/* ARGSUSED */
int
sys_obreak(td, uap)
	struct thread *td;
	struct obreak_args *uap;
{
	struct vmspace *vm = td->td_proc->p_vmspace;
	vm_offset_t new, old, base;
	rlim_t datalim, vmemlim;
	int prot, rv;
	int error = 0;
	boolean_t do_map_wirefuture;

	PROC_LOCK(td->td_proc);
	datalim = lim_cur(td->td_proc, RLIMIT_DATA);
	vmemlim = lim_cur(td->td_proc, RLIMIT_VMEM);
	PROC_UNLOCK(td->td_proc);

	do_map_wirefuture = FALSE;
	new = round_page((vm_offset_t)uap->nsize);
	vm_map_lock(&vm->vm_map);

	base = round_page((vm_offset_t) vm->vm_daddr);
	old = base + ctob(vm->vm_dsize);
	if (new > base) {
		/*
		 * Check the resource limit, but allow a process to reduce
		 * its usage, even if it remains over the limit.
		 */
		if (new - base > datalim && new > old) {
			error = ENOMEM;
			goto done;
		}
		if (new > vm_map_max(&vm->vm_map)) {
			error = ENOMEM;
			goto done;
		}
	} else if (new < base) {
		/*
		 * This is simply an invalid value.  If someone wants to
		 * do fancy address space manipulations, mmap and munmap
		 * can do most of what the user would want.
		 */
		error = EINVAL;
		goto done;
	}
	if (new > old) {
		if (vm->vm_map.size + (new - old) > vmemlim) {
			error = ENOMEM;
			goto done;
		}
#ifdef RACCT
		PROC_LOCK(td->td_proc);
		error = racct_set(td->td_proc, RACCT_DATA, new - base);
		if (error != 0) {
			PROC_UNLOCK(td->td_proc);
			error = ENOMEM;
			goto done;
		}
		error = racct_set(td->td_proc, RACCT_VMEM,
		    vm->vm_map.size + (new - old));
		if (error != 0) {
			racct_set_force(td->td_proc, RACCT_DATA, old - base);
			PROC_UNLOCK(td->td_proc);
			error = ENOMEM;
			goto done;
		}
		PROC_UNLOCK(td->td_proc);
#endif
		prot = VM_PROT_RW;
#ifdef COMPAT_FREEBSD32
#if defined(__amd64__) || defined(__ia64__)
		if (i386_read_exec && SV_PROC_FLAG(td->td_proc, SV_ILP32))
			prot |= VM_PROT_EXECUTE;
#endif
#endif
		rv = vm_map_insert(&vm->vm_map, NULL, 0, old, new,
		    prot, VM_PROT_ALL, 0);
		if (rv != KERN_SUCCESS) {
#ifdef RACCT
			PROC_LOCK(td->td_proc);
			racct_set_force(td->td_proc, RACCT_DATA, old - base);
			racct_set_force(td->td_proc, RACCT_VMEM, vm->vm_map.size);
			PROC_UNLOCK(td->td_proc);
#endif
			error = ENOMEM;
			goto done;
		}
		vm->vm_dsize += btoc(new - old);
		/*
		 * Handle the MAP_WIREFUTURE case for legacy applications,
		 * by marking the newly mapped range of pages as wired.
		 * We are not required to perform a corresponding
		 * vm_map_unwire() before vm_map_delete() below, as
		 * it will forcibly unwire the pages in the range.
		 *
		 * XXX If the pages cannot be wired, no error is returned.
		 */
		if ((vm->vm_map.flags & MAP_WIREFUTURE) == MAP_WIREFUTURE) {
			if (bootverbose)
				printf("obreak: MAP_WIREFUTURE set\n");
			do_map_wirefuture = TRUE;
		}
	} else if (new < old) {
		rv = vm_map_delete(&vm->vm_map, new, old);
		if (rv != KERN_SUCCESS) {
			error = ENOMEM;
			goto done;
		}
		vm->vm_dsize -= btoc(old - new);
#ifdef RACCT
		PROC_LOCK(td->td_proc);
		racct_set_force(td->td_proc, RACCT_DATA, new - base);
		racct_set_force(td->td_proc, RACCT_VMEM, vm->vm_map.size);
		PROC_UNLOCK(td->td_proc);
#endif
	}
done:
	vm_map_unlock(&vm->vm_map);

	if (do_map_wirefuture)
		(void) vm_map_wire(&vm->vm_map, old, new,
		    VM_MAP_WIRE_USER|VM_MAP_WIRE_NOHOLES);

	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct ovadvise_args {
	int anom;
};
#endif

/*
 * MPSAFE
 */
/* ARGSUSED */
int
sys_ovadvise(td, uap)
	struct thread *td;
	struct ovadvise_args *uap;
{
	/* START_GIANT_OPTIONAL */
	/* END_GIANT_OPTIONAL */
	return (EINVAL);
}
