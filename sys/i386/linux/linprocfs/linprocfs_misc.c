/*
 * Copyright (c) 2000 Dag-Erling Coïdan Smørgrav
 * Copyright (c) 1999 Pierre Beyssac
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
#include <sys/proc.h>
#include <sys/jail.h>
#include <sys/vnode.h>
#include <sys/blist.h>
#include <sys/tty.h>
#include <sys/resourcevar.h>
#include <i386/linux/linprocfs/linprocfs.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/swap_pager.h>
#include <sys/vmmeter.h>
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
	unsigned long memtotal;		/* total memory in bytes */
	unsigned long memused;		/* used memory in bytes */
	unsigned long memfree;		/* free memory in bytes */
	unsigned long memshared;	/* shared memory ??? */
	unsigned long buffers, cached;	/* buffer / cache memory ??? */
	unsigned long swaptotal;	/* total swap space in bytes */
	unsigned long swapused;		/* used swap space in bytes */
	unsigned long swapfree;		/* free swap space in bytes */
	vm_object_t object;

	if (uio->uio_rw != UIO_READ)
		return (EOPNOTSUPP);

	memtotal = physmem * PAGE_SIZE;
	/*
	 * The correct thing here would be:
	 *
	memfree = cnt.v_free_count * PAGE_SIZE;
	memused = memtotal - memfree;
	 *
	 * but it might mislead linux binaries into thinking there
	 * is very little memory left, so we cheat and tell them that
	 * all memory that isn't wired down is free.
	 */
	memused = cnt.v_wire_count * PAGE_SIZE;
	memfree = memtotal - memused;
	if (swapblist == NULL) {
		swaptotal = 0;
		swapfree = 0;
	} else {
		swaptotal = swapblist->bl_blocks * 1024; /* XXX why 1024? */
		swapfree = swapblist->bl_root->u.bmu_avail * PAGE_SIZE;
	}
	swapused = swaptotal - swapfree;
	memshared = 0;
	for (object = TAILQ_FIRST(&vm_object_list); object != NULL;
	    object = TAILQ_NEXT(object, object_list))
		if (object->shadow_count > 1)
			memshared += object->resident_page_count;
	memshared *= PAGE_SIZE;
	/*
	 * We'd love to be able to write:
	 *
	buffers = bufspace;
	 *
	 * but bufspace is internal to vfs_bio.c and we don't feel
	 * like unstaticizing it just for linprocfs's sake.
	 */
	buffers = 0;
	cached = cnt.v_cache_count * PAGE_SIZE;

	ps = psbuf;
	ps += sprintf(ps,
		"        total:    used:    free:  shared: buffers:  cached:\n"
		"Mem:  %lu %lu %lu %lu %lu %lu\n"
		"Swap: %lu %lu %lu\n"
		"MemTotal: %9lu kB\n"
		"MemFree:  %9lu kB\n"
		"MemShared:%9lu kB\n"
		"Buffers:  %9lu kB\n"
		"Cached:   %9lu kB\n"
		"SwapTotal:%9lu kB\n"
		"SwapFree: %9lu kB\n",
		memtotal, memused, memfree, memshared, buffers, cached,
		swaptotal, swapused, swapfree,
		memtotal >> 10, memfree >> 10,
		memshared >> 10, buffers >> 10, cached >> 10,
		swaptotal >> 10, swapfree >> 10);

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
