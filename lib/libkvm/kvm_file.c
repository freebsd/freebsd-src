/*-
 * Copyright (c) 1989, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)kvm_file.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */

/*
 * File list interface for kvm.  pstat, fstat and netstat are 
 * users of this code, so we've factored it out into a separate module.
 * Thus, we keep this grunge out of the other kvm applications (i.e.,
 * most other applications are interested only in open/close/read/nlist).
 */

#include <sys/param.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/exec.h>
#define KERNEL
#include <sys/file.h>
#undef KERNEL
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <nlist.h>
#include <kvm.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/swap_pager.h>

#include <sys/sysctl.h>

#include <limits.h>
#include <ndbm.h>
#include <paths.h>

#include "kvm_private.h"

#define KREAD(kd, addr, obj) \
	(kvm_read(kd, addr, obj, sizeof(*obj)) != sizeof(*obj))

/*
 * Get file structures.
 */
static
kvm_deadfiles(kd, op, arg, filehead_o, nfiles)
	kvm_t *kd;
	int op, arg, nfiles;
	long filehead_o;
{
	int buflen = kd->arglen, needed = buflen, error, n = 0;
	struct file *fp, file, *filehead;
	register char *where = kd->argspc;
	char *start = where;

	/*
	 * first copyout filehead
	 */
	if (buflen > sizeof (filehead)) {
		if (KREAD(kd, filehead_o, &filehead)) {
			_kvm_err(kd, kd->program, "can't read filehead");
			return (0);
		}
		buflen -= sizeof (filehead);
		where += sizeof (filehead);
		*(struct file **)kd->argspc = filehead;
	}
	/*
	 * followed by an array of file structures
	 */
	for (fp = filehead; fp != NULL; fp = fp->f_filef) {
		if (buflen > sizeof (struct file)) {
			if (KREAD(kd, (long)fp, ((struct file *)where))) {
				_kvm_err(kd, kd->program, "can't read kfp");
				return (0);
			}
			buflen -= sizeof (struct file);
			fp = (struct file *)where;
			where += sizeof (struct file);
			n++;
		}
	}
	if (n != nfiles) {
		_kvm_err(kd, kd->program, "inconsistant nfiles");
		return (0);
	}
	return (nfiles);
}

char *
kvm_getfiles(kd, op, arg, cnt)
	kvm_t *kd;
	int op, arg;
	int *cnt;
{
	int mib[2], size, st, nfiles;
	struct file *filehead, *fp, *fplim;

	if (ISALIVE(kd)) {
		size = 0;
		mib[0] = CTL_KERN;
		mib[1] = KERN_FILE;
		st = sysctl(mib, 2, NULL, &size, NULL, 0);
		if (st == -1) {
			_kvm_syserr(kd, kd->program, "kvm_getprocs");
			return (0);
		}
		if (kd->argspc == 0)
			kd->argspc = (char *)_kvm_malloc(kd, size);
		else if (kd->arglen < size)
			kd->argspc = (char *)_kvm_realloc(kd, kd->argspc, size);
		if (kd->argspc == 0)
			return (0);
		kd->arglen = size;
		st = sysctl(mib, 2, kd->argspc, &size, NULL, 0);
		if (st == -1 || size < sizeof(filehead)) {
			_kvm_syserr(kd, kd->program, "kvm_getfiles");
			return (0);
		}
		filehead = *(struct file **)kd->argspc;
		fp = (struct file *)(kd->argspc + sizeof (filehead));
		fplim = (struct file *)(kd->argspc + size);
		for (nfiles = 0; filehead && (fp < fplim); nfiles++, fp++)
			filehead = fp->f_filef;
	} else {
		struct nlist nl[3], *p;

		nl[0].n_name = "_filehead";
		nl[1].n_name = "_nfiles";
		nl[2].n_name = 0;

		if (kvm_nlist(kd, nl) != 0) {
			for (p = nl; p->n_type != 0; ++p)
				;
			_kvm_err(kd, kd->program,
				 "%s: no such symbol", p->n_name);
			return (0);
		}
		if (KREAD(kd, nl[0].n_value, &nfiles)) {
			_kvm_err(kd, kd->program, "can't read nfiles");
			return (0);
		}
		size = sizeof(filehead) + (nfiles + 10) * sizeof(struct file);
		if (kd->argspc == 0)
			kd->argspc = (char *)_kvm_malloc(kd, size);
		else if (kd->arglen < size)
			kd->argspc = (char *)_kvm_realloc(kd, kd->argspc, size);
		if (kd->argspc == 0)
			return (0);
		kd->arglen = size;
		nfiles = kvm_deadfiles(kd, op, arg, nl[1].n_value, nfiles);
		if (nfiles == 0)
			return (0);
	}
	*cnt = nfiles;
	return (kd->argspc);
}
