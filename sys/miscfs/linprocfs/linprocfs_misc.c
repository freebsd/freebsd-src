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
 *	@(#)procfs_status.c	8.4 (Berkeley) 6/15/94
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/jail.h>
#include <sys/vnode.h>
#include <sys/tty.h>
#include <sys/resourcevar.h>
#include <miscfs/linprocfs/linprocfs.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>
#include <sys/exec.h>

#include <machine/md_var.h>
#include <machine/cputypes.h>

struct proc;

int
linprocfs_domeminfo(curp, p, pfs, uio)
	struct proc *curp;
	struct proc *p;
	struct pfsnode *pfs;
	struct uio *uio;
{
	char *ps;
	int xlen;
	int error;
	char psbuf[512];		/* XXX - conservative */
	unsigned long memtotal, memfree, memshared;
	unsigned long buffers, cached, swaptotal, swapfree;

	if (uio->uio_rw != UIO_READ)
		return (EOPNOTSUPP);

	memtotal = 32768; memfree = 32768;
	buffers = 16384; cached = 8192;
	swaptotal = 32768; swapfree = 16384;
	memshared = 0;

	ps = psbuf;
	ps += sprintf(ps,
		"        total:    used:    free:  shared: buffers:  cached:\n"
		"Mem:  %ld %ld %ld %ld %ld %ld\nSwap: %ld %ld %ld\n"
		"MemTotal: %9ld kB\n"
		"MemFree:  %9ld kB\n"
		"MemShared:%9ld kB\n"
		"Buffers:  %9ld kB\n"
		"Cached:   %9ld kB\n"
		"SwapTotal:%9ld kB\n"
		"SwapFree: %9ld kB\n",
		memtotal*1024, (memtotal-memfree)*1024, memfree*1024,
		memshared*1024, buffers*1024, cached*1024,
		swaptotal*1024, (swaptotal-swapfree)*1024, swapfree*1024,
		memtotal, memfree, memshared, buffers, cached,
		swaptotal, swapfree);

	xlen = ps - psbuf;
	xlen -= uio->uio_offset;
	ps = psbuf + uio->uio_offset;
	xlen = imin(xlen, uio->uio_resid);
	if (xlen <= 0)
		error = 0;
	else
		error = uiomove(ps, xlen, uio);
	return (error);
}

int
linprocfs_docpuinfo(curp, p, pfs, uio)
	struct proc *curp;
	struct proc *p;
	struct pfsnode *pfs;
	struct uio *uio;
{
	char *ps;
	int xlen;
	int error;
	char psbuf[512];		/* XXX - conservative */
	char *class;
#if 0
	extern char *cpu_model;		/* Yuck */
#endif

	if (uio->uio_rw != UIO_READ)
		return (EOPNOTSUPP);

	switch (cpu_class) {
	case CPUCLASS_286:
		class = "286";
		break;
	case CPUCLASS_386:
		class = "386";
		break;
	case CPUCLASS_486:
		class = "486";
		break;
	case CPUCLASS_586:
		class = "586";
		break;
	case CPUCLASS_686:
		class = "686";
		break;
	default:
		class = "unknown";
		break;
	}

	ps = psbuf;
	ps += sprintf(ps,
			"processor       : %d\n"
			"cpu             : %.3s\n"
			"model           : %.20s\n"
			"vendor_id       : %.20s\n"
			"stepping        : %d\n",
			0, class, "unknown", cpu_vendor, cpu_id);

	xlen = ps - psbuf;
	xlen -= uio->uio_offset;
	ps = psbuf + uio->uio_offset;
	xlen = imin(xlen, uio->uio_resid);
	if (xlen <= 0)
		error = 0;
	else
		error = uiomove(ps, xlen, uio);
	return (error);
}
