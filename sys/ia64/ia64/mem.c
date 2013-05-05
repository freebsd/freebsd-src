/*-
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department, and code derived from software contributed to
 * Berkeley by William Jolitz.
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
 *	from: Utah $Hdr: mem.c 1.13 89/10/08$
 *	from: @(#)mem.c	7.2 (Berkeley) 5/9/91
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Memory special file
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/memrange.h>
#include <sys/module.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/systm.h>
#include <sys/uio.h>

#include <machine/cpu.h>
#include <machine/frame.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

#include <machine/memdev.h>

struct mem_range_softc mem_range_softc;

static __inline int
ia64_pa_access(vm_offset_t pa)
{
	return (VM_PROT_READ|VM_PROT_WRITE);
}

/* ARGSUSED */
int
memrw(struct cdev *dev, struct uio *uio, int flags)
{
	struct iovec *iov;
	vm_offset_t addr, eaddr, o, v;
	int c, error, rw;

	error = 0;
	while (uio->uio_resid > 0 && !error) {
		iov = uio->uio_iov;
		if (iov->iov_len == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			if (uio->uio_iovcnt < 0)
				panic("memrw");
			continue;
		}

		if (dev2unit(dev) == CDEV_MINOR_MEM) {
			v = uio->uio_offset;
kmemphys:
			/* Allow reads only in RAM. */
			rw = (uio->uio_rw == UIO_READ)
			    ? VM_PROT_READ : VM_PROT_WRITE;
			if ((ia64_pa_access(v) & rw) != rw) {
				error = EFAULT;
				c = 0;
				break;
			}

			o = uio->uio_offset & PAGE_MASK;
			c = min(uio->uio_resid, (int)(PAGE_SIZE - o));
			error = uiomove((caddr_t)IA64_PHYS_TO_RR7(v), c, uio);
			continue;
		}
		else if (dev2unit(dev) == CDEV_MINOR_KMEM) {
			v = uio->uio_offset;

			if (v >= IA64_RR_BASE(6)) {
				v = IA64_RR_MASK(v);
				goto kmemphys;
			}

			c = min(iov->iov_len, MAXPHYS);

			/*
			 * Make sure that all of the pages are currently
			 * resident so that we don't create any zero-fill
			 * pages.
			 */
			addr = trunc_page(v);
			eaddr = round_page(v + c);
			if (addr < VM_MAXUSER_ADDRESS)
				return (EFAULT);
			for (; addr < eaddr; addr += PAGE_SIZE) {
				if (pmap_kextract(addr) == 0)
					return (EFAULT);
			}
			if (!kernacc((caddr_t)v, c, (uio->uio_rw == UIO_READ)
			    ? VM_PROT_READ : VM_PROT_WRITE))
				return (EFAULT);
			error = uiomove((caddr_t)v, c, uio);
			continue;
		}
		/* else panic! */
	}
	return (error);
}

/*
 * allow user processes to MMAP some memory sections
 * instead of going through read/write
 */
int
memmmap(struct cdev *dev, vm_ooffset_t offset, vm_paddr_t *paddr,
    int prot, vm_memattr_t *memattr)
{
	/*
	 * /dev/mem is the only one that makes sense through this
	 * interface.  For /dev/kmem any physaddr we return here
	 * could be transient and hence incorrect or invalid at
	 * a later time.
	 */
	if (dev2unit(dev) != CDEV_MINOR_MEM)
		return (-1);

	/*
	 * Allow access only in RAM.
	 */
	if ((prot & ia64_pa_access(atop((vm_offset_t)offset))) != prot)
		return (-1);
	*paddr = IA64_PHYS_TO_RR7(offset);
	return (0);
}
