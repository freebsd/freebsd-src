/*
 * Copyright (c) 1999 Adrian Chadd
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
 *	@(#)procfs_status.c	8.4 (Berkeley) 6/15/94
 *
 * $FreeBSD$
 */

/*
 * To get resource.h to include our rlimit_ident[] array of rlimit identifiers
 */

#define _RLIMIT_IDENT

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/resourcevar.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <miscfs/procfs/procfs.h>


int
procfs_dorlimit(curp, p, pfs, uio)
	struct proc *curp;
	struct proc *p;
	struct pfsnode *pfs;
	struct uio *uio;
{
	char *ps;
	int i;
	int error;
	char psbuf[512];		/* XXX - conservative */

	if (uio->uio_rw != UIO_READ)
		return (EOPNOTSUPP);


	ps = psbuf;

	for (i = 0; i < RLIM_NLIMITS; i++) {

		/*
		 * Add the rlimit ident
		 */

		ps += sprintf(ps, "%s ", rlimit_ident[i]);

		/* 
		 * Replace RLIM_INFINITY with -1 in the string
		 */

		/*
		 * current limit
		 */

		if (p->p_rlimit[i].rlim_cur == RLIM_INFINITY) {
			ps += sprintf(ps, "-1 ");
		} else {
			ps += sprintf(ps, "%llu ",
				(unsigned long long)p->p_rlimit[i].rlim_cur);
		}

		/*
		 * maximum limit
		 */

		if (p->p_rlimit[i].rlim_max == RLIM_INFINITY) {
			ps += sprintf(ps, "-1\n");
		} else {
			ps += sprintf(ps, "%llu\n",
				(unsigned long long)p->p_rlimit[i].rlim_max);
		}
	}

	error = uiomove_frombuf(psbuf, ps - psbuf, uio);
	return (error);
}

