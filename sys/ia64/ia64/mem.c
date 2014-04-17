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
#include <sys/efi.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/memrange.h>
#include <sys/systm.h>
#include <sys/uio.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

#include <machine/memdev.h>

struct mem_range_softc mem_range_softc;

static int
mem_phys2virt(vm_offset_t offset, int prot, void **ptr, u_long *limit)
{
	struct efi_md *md;

	if (prot & ~(VM_PROT_READ | VM_PROT_WRITE))
		return (EPERM);

	md = efi_md_find(offset);
	if (md == NULL)
		return (EFAULT);

	if (md->md_type == EFI_MD_TYPE_BAD)
		return (EIO);

	*ptr = (void *)((md->md_attr & EFI_MD_ATTR_WB)
	    ? IA64_PHYS_TO_RR7(offset) : IA64_PHYS_TO_RR6(offset));
	*limit = (md->md_pages * EFI_PAGE_SIZE) - (offset - md->md_phys);
	return (0);
}

/* ARGSUSED */
int
memrw(struct cdev *dev, struct uio *uio, int flags)
{
	struct iovec *iov;
	off_t ofs;
	vm_offset_t addr;
	void *ptr;
	u_long limit;
	int count, error, phys, rw;

	error = 0;
	rw = (uio->uio_rw == UIO_READ) ? VM_PROT_READ : VM_PROT_WRITE;

	while (uio->uio_resid > 0 && !error) {
		iov = uio->uio_iov;
		if (iov->iov_len == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			if (uio->uio_iovcnt < 0)
				panic("memrw");
			continue;
		}

		ofs = uio->uio_offset;

		phys = (dev2unit(dev) == CDEV_MINOR_MEM) ? 1 : 0;
		if (phys == 0 && ofs >= IA64_RR_BASE(6)) {
			ofs = IA64_RR_MASK(ofs);
			phys++;
		}

		if (phys) {
			error = mem_phys2virt(ofs, rw, &ptr, &limit);
			if (error)
				return (error);

			count = min(uio->uio_resid, limit);
			error = uiomove(ptr, count, uio);
		} else {
			ptr = (void *)ofs;
			count = iov->iov_len;

			/*
			 * Make sure that all of the pages are currently
			 * resident so that we don't create any zero-fill
			 * pages.
			 */
			limit = round_page(ofs + count);
			addr = trunc_page(ofs);
			if (addr < VM_MAXUSER_ADDRESS)
				return (EINVAL);
			for (; addr < limit; addr += PAGE_SIZE) {
				if (pmap_kextract(addr) == 0)
					return (EFAULT);
			}
			if (!kernacc(ptr, count, rw))
				return (EFAULT);
			error = uiomove(ptr, count, uio);
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
	void *ptr;
	u_long limit;
	int error;

	/*
	 * /dev/mem is the only one that makes sense through this
	 * interface.  For /dev/kmem any physaddr we return here
	 * could be transient and hence incorrect or invalid at
	 * a later time.
	 */
	if (dev2unit(dev) != CDEV_MINOR_MEM)
		return (ENXIO);

	error = mem_phys2virt(offset, prot, &ptr, &limit);
	if (error)
		return (error);

	*paddr = offset;
	*memattr = ((uintptr_t)ptr >= IA64_RR_BASE(7)) ?
	    VM_MEMATTR_WRITE_BACK : VM_MEMATTR_UNCACHEABLE;
	return (0);
}
