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
 *	from: FreeBSD: src/sys/i386/i386/mem.c,v 1.94 2001/09/26
 *
 * $FreeBSD$
 */

/*
 * Memory special file
 *
 * NOTE: other architectures support mmap()'ing the mem and kmem devices; this
 * might cause illegal aliases to be created for the locked kernel page(s), so
 * it is not implemented.
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/systm.h>
#include <sys/uio.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

#include <machine/cache.h>
#include <machine/md_var.h>
#include <machine/pmap.h>
#include <machine/tlb.h>
#include <machine/upa.h>

static dev_t memdev, kmemdev;

static	d_open_t	mmopen;
static	d_close_t	mmclose;
static	d_read_t	mmrw;

#define CDEV_MAJOR 2
static struct cdevsw mem_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	mmopen,
	.d_close =	mmclose,
	.d_read =	mmrw,
	.d_write =	mmrw,
	.d_name =	"mem",
	.d_maj =	CDEV_MAJOR,
	.d_flags =	D_MEM | D_NEEDGIANT,
};

static int
mmclose(dev_t dev, int flags, int fmt, struct thread *td)
{

	return (0);
}

static int
mmopen(dev_t dev, int flags, int fmt, struct thread *td)
{
	int error;

	switch (minor(dev)) {
	case 0:
	case 1:
		if (flags & FWRITE) {
			error = securelevel_gt(td->td_ucred, 0);
			if (error != 0)
				return (error);
		}
		break;
	default:
		return (ENXIO);
	}
	return (0);
}

/*ARGSUSED*/
static int
mmrw(dev_t dev, struct uio *uio, int flags)
{
	struct iovec *iov;
	vm_offset_t eva;
	vm_offset_t off;
	vm_offset_t ova;
	vm_offset_t va;
	vm_prot_t prot;
	vm_paddr_t pa;
	vm_size_t cnt;
	vm_page_t m;
	int color;
	int error;
	int i;

	cnt = 0;
	error = 0;
	ova = 0;

	GIANT_REQUIRED;

	while (uio->uio_resid > 0 && error == 0) {
		iov = uio->uio_iov;
		if (iov->iov_len == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			if (uio->uio_iovcnt < 0)
				panic("mmrw");
			continue;
		}
		switch (minor(dev)) {
		case 0:
			/* mem (physical memory) */
			pa = uio->uio_offset & ~PAGE_MASK;
			if (!is_physical_memory(pa)) {
				error = EFAULT;
				break;
			}

			off = uio->uio_offset & PAGE_MASK;
			cnt = PAGE_SIZE - ((vm_offset_t)iov->iov_base &
			    PAGE_MASK);
			cnt = min(cnt, PAGE_SIZE - off);
			cnt = min(cnt, iov->iov_len);

			m = NULL;
			for (i = 0; phys_avail[i] != 0; i += 2) {
				if (pa >= phys_avail[i] &&
				    pa < phys_avail[i + 1]) {
					m = PHYS_TO_VM_PAGE(pa);
					break;
				}
			}

			if (m != NULL) {
				if (ova == 0) {
					ova = kmem_alloc_wait(kernel_map,
					    PAGE_SIZE * DCACHE_COLORS);
				}
				if ((color = m->md.color) == -1)
					va = ova;
				else
					va = ova + color * PAGE_SIZE;
				pmap_qenter(va, &m, 1);
				error = uiomove((void *)(va + off), cnt,
				    uio);
				pmap_qremove(va, 1);
			} else {
				va = TLB_PHYS_TO_DIRECT(pa);
				error = uiomove((void *)(va + off), cnt,
				    uio);
			}
			break;
		case 1:
			/* kmem (kernel memory) */
			va = trunc_page(uio->uio_offset);
			eva = round_page(uio->uio_offset + iov->iov_len);

			/*
			 * Make sure that all of the pages are currently
			 * resident so we don't create any zero fill pages.
			 */
			for (; va < eva; va += PAGE_SIZE)
				if (pmap_kextract(va) == 0)
					return (EFAULT);

			prot = (uio->uio_rw == UIO_READ) ? VM_PROT_READ :
			    VM_PROT_WRITE;
			va = uio->uio_offset;
			if (va < VM_MIN_DIRECT_ADDRESS &&
			    kernacc((void *)va, iov->iov_len, prot) == FALSE)
				return (EFAULT);

			error = uiomove((void *)va, iov->iov_len, uio);
			break;
		default:
			return (ENODEV);
		}
	}
	if (ova != 0)
		kmem_free_wakeup(kernel_map, ova, PAGE_SIZE * DCACHE_COLORS);
	return (error);
}

static int
mem_modevent(module_t mod, int type, void *data)
{
	switch(type) {
	case MOD_LOAD:
		if (bootverbose)
			printf("mem: <memory & I/O>\n");

		memdev = make_dev(&mem_cdevsw, 0, UID_ROOT, GID_KMEM,
			0640, "mem");
		kmemdev = make_dev(&mem_cdevsw, 1, UID_ROOT, GID_KMEM,
			0640, "kmem");
		return 0;

	case MOD_UNLOAD:
		destroy_dev(memdev);
		destroy_dev(kmemdev);
		return 0;

	case MOD_SHUTDOWN:
		return 0;

	default:
		return EOPNOTSUPP;
	}
}

DEV_MODULE(mem, mem_modevent, NULL);
