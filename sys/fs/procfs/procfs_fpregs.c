/*
 * Copyright (c) 1993 Jan-Simon Pendry
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)procfs_fpregs.c	8.2 (Berkeley) 6/15/94
 *
 * From:
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/vnode.h>

#include <machine/reg.h>

#include <fs/procfs/procfs.h>

#include <vm/vm.h>

int
procfs_dofpregs(curp, p, pfs, uio)
	struct proc *curp;
	struct proc *p;
	struct pfsnode *pfs;
	struct uio *uio;
{
	int error;
	struct fpreg r;
	char *kv;
	int kl;

	if (p_candebug(curp, p))
		return EPERM;
	kl = sizeof(r);
	kv = (char *) &r;

	kv += uio->uio_offset;
	kl -= uio->uio_offset;
	if (kl > uio->uio_resid)
		kl = uio->uio_resid;

	PHOLD(p);

	if (kl < 0)
		error = EINVAL;
	else
		error = procfs_read_fpregs(p, &r);
	if (error == 0)
		error = uiomove(kv, kl, uio);
	if (error == 0 && uio->uio_rw == UIO_WRITE) {
		if (p->p_stat != SSTOP)
			error = EBUSY;
		else
			error = procfs_write_fpregs(p, &r);
	}
	PRELE(p);

	uio->uio_offset = 0;
	return (error);
}

int
procfs_validfpregs(p)
	struct proc *p;
{

	return ((p->p_flag & P_SYSTEM) == 0);
}
