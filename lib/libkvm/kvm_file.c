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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)kvm_file.c	8.1 (Berkeley) 6/4/93";
#endif
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
#define _KERNEL
#include <sys/file.h>
#undef _KERNEL
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <nlist.h>
#include <kvm.h>

#include <vm/vm.h>
#include <vm/vm_param.h>

#include <sys/sysctl.h>

#include <limits.h>
#include <ndbm.h>
#include <paths.h>

#include "kvm_private.h"

#define KREAD(kd, addr, obj) \
	(kvm_read(kd, addr, obj, sizeof(*obj)) != sizeof(*obj))

#define KREADN(kd, addr, obj, cnt) \
	(kvm_read(kd, addr, obj, (cnt)) != (cnt))

/*
 * Get file structures.
 */
static int
kvm_deadfiles(kd, op, arg, allproc_o, nprocs)
	kvm_t *kd;
	int op, arg, nprocs;
	long allproc_o;
{
	struct proc proc;
	struct filedesc filed;
	int buflen = kd->arglen, ocnt = 0, n = 0, once = 0, i;
	struct file **ofiles;
	struct file *fp;
	struct proc *p;
	char *where = kd->argspc;

	if (buflen < sizeof (struct file *) + sizeof (struct file))
		return (0);
	if (KREAD(kd, allproc_o, &p)) {
		_kvm_err(kd, kd->program, "cannot read allproc");
		return (0);
	}
	for (; p != NULL; p = LIST_NEXT(&proc, p_list)) {
		if (KREAD(kd, (u_long)p, &proc)) {
			_kvm_err(kd, kd->program, "can't read proc at %x", p);
			goto fail;
		}
		if (proc.p_state == PRS_NEW)
			continue;
		if (proc.p_fd == NULL)
			continue;
		if (KREAD(kd, (u_long)p->p_fd, &filed)) {
			_kvm_err(kd, kd->program, "can't read filedesc at %x",
			    p->p_fd);
			goto fail;
		}
		if (filed.fd_lastfile + 1 > ocnt) {
			ocnt = filed.fd_lastfile + 1;
			free(ofiles);
			ofiles = (struct file **)_kvm_malloc(kd,
				ocnt * sizeof(struct file *));
			if (ofiles == 0)
				return (0);
		}
		if (KREADN(kd, (u_long)filed.fd_ofiles, ofiles,
		    ocnt * sizeof(struct file *))) {
			_kvm_err(kd, kd->program, "can't read ofiles at %x",
			    filed.fd_ofiles);
			return (0);
		}
		for (i = 0; i <= filed.fd_lastfile; i++) {
			if ((fp = ofiles[i]) == NULL)
				continue;
			/*
			 * copyout filehead (legacy)
			 */
			if (!once) {
				*(struct file **)kd->argspc = fp;
				*(struct file **)where = fp;
				buflen -= sizeof (fp);
				where += sizeof (fp);
				once = 1;
			}
			if (buflen < sizeof (struct file))
				goto fail;
			if (KREAD(kd, (long)fp, ((struct file *)where))) {
				_kvm_err(kd, kd->program, "can't read kfp");
				goto fail;
			}
			buflen -= sizeof (struct file);
			fp = (struct file *)where;
			where += sizeof (struct file);
			n++;
		}
	}
	free(ofiles);
	return (n);
fail:
	free(ofiles);
	return (0);
	
}

char *
kvm_getfiles(kd, op, arg, cnt)
	kvm_t *kd;
	int op, arg;
	int *cnt;
{
	int mib[2], st, n, nfiles, nprocs;
	size_t size;

	_kvm_syserr(kd, kd->program, "kvm_getfiles has been broken for years");
	return (0);
	if (ISALIVE(kd)) {
		size = 0;
		mib[0] = CTL_KERN;
		mib[1] = KERN_FILE;
		st = sysctl(mib, 2, NULL, &size, NULL, 0);
		if (st == -1) {
			_kvm_syserr(kd, kd->program, "kvm_getfiles");
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
		if (st != 0) {
			_kvm_syserr(kd, kd->program, "kvm_getfiles");
			return (0);
		}
		nfiles = size / sizeof(struct xfile);
	} else {
		struct nlist nl[4], *p;

		nl[0].n_name = "_allproc";
		nl[1].n_name = "_nprocs";
		nl[2].n_name = "_nfiles";
		nl[3].n_name = 0;

		if (kvm_nlist(kd, nl) != 0) {
			for (p = nl; p->n_type != 0; ++p)
				;
			_kvm_err(kd, kd->program,
				 "%s: no such symbol", p->n_name);
			return (0);
		}
		if (KREAD(kd, nl[1].n_value, &nprocs)) {
			_kvm_err(kd, kd->program, "can't read nprocs");
			return (0);
		}
		if (KREAD(kd, nl[2].n_value, &nfiles)) {
			_kvm_err(kd, kd->program, "can't read nfiles");
			return (0);
		}
		size = sizeof(void *) + (nfiles + 10) * sizeof(struct file);
		if (kd->argspc == 0)
			kd->argspc = (char *)_kvm_malloc(kd, size);
		else if (kd->arglen < size)
			kd->argspc = (char *)_kvm_realloc(kd, kd->argspc, size);
		if (kd->argspc == 0)
			return (0);
		kd->arglen = size;
		n = kvm_deadfiles(kd, op, arg, nl[0].n_value, nprocs);
		if (n != nfiles) {
			_kvm_err(kd, kd->program, "inconsistant nfiles");
			return (0);
		}
		nfiles = n;
	}
	*cnt = nfiles;
	return (kd->argspc);
}
