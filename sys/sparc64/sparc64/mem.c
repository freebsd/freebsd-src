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
#include <vm/pmap.h>
#include <vm/vm_extern.h>

#include <machine/cache.h>
#include <machine/pmap.h>
#include <machine/upa.h>

static dev_t memdev, kmemdev;

static	d_open_t	mmopen;
static	d_close_t	mmclose;
static	d_read_t	mmrw;

#define CDEV_MAJOR 2
static struct cdevsw mem_cdevsw = {
	/* open */	mmopen,
	/* close */	mmclose,
	/* read */	mmrw,
	/* write */	mmrw,
	/* ioctl */	noioctl,
	/* poll */	(d_poll_t *)seltrue,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"mem",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_MEM,
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
			error = securelevel_gt(td->td_proc->p_ucred, 0);
			if (error != 0)
				return (error);
		}
		break;
	default:
		return (ENXIO);
	}
	return (0);
}

#define	IOSTART		UPA_MEMSTART

/*ARGSUSED*/
static int
mmrw(dev_t dev, struct uio *uio, int flags)
{
	struct iovec *iov;
	int error = 0;
	vm_offset_t addr, eaddr, o, v = 0;
	vm_prot_t prot;
	vm_size_t c = 0;
	u_long asi;
	char *buf = NULL;

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
			if (buf == NULL) {
				buf = malloc(PAGE_SIZE, M_DEVBUF, M_WAITOK);
				if (buf == NULL) {
					error = ENOMEM;
					break;
				}
			}
			v = uio->uio_offset;
			asi = ASI_PHYS_USE_EC;
			/* Access device memory noncacheable. */
			if (v >= IOSTART)
				asi = ASI_PHYS_BYPASS_EC_WITH_EBIT;
			o = v & PAGE_MASK;
			c = ulmin(iov->iov_len, PAGE_SIZE - o);
			/*
			 * This double copy could be avoided, at the cost of
			 * inlining a version of uiomove. Since this is not
			 * performance-critical, it is probably not worth it.
			 */
			if (uio->uio_rw == UIO_READ)
				ascopyfrom(asi, v, buf, c);
			error = uiomove(buf, c, uio);
			if (error == 0 && uio->uio_rw == UIO_WRITE)
				ascopyto(buf, asi, v, c);
			/*
			 * If a write was evil enough to change kernel code,
			 * I$ must be flushed. Also, D$ must be flushed if there
			 * is a chance that there is a cacheable mapping to
			 * avoid working with stale data.
			 */
			if (v < IOSTART && uio->uio_rw == UIO_WRITE) {
				icache_inval_phys(v, v + c);
				dcache_inval_phys(v, v + c);
			}
			break;
		case 1:
			/* kmem (kernel memory) */
			c = iov->iov_len;

			/*
			 * Make sure that all of the pages are currently resident so
			 * that we don't create any zero-fill pages.
			 */
			addr = trunc_page(uio->uio_offset);
			eaddr = round_page(uio->uio_offset + c);

			for (; addr < eaddr; addr += PAGE_SIZE)
				if (pmap_extract(kernel_pmap, addr) == 0)
					return EFAULT;

			prot = (uio->uio_rw == UIO_READ) ? VM_PROT_READ :
			    VM_PROT_WRITE;
			v = uio->uio_offset;
			if (v < VM_MIN_DIRECT_ADDRESS &&
			    kernacc((caddr_t)v, c, prot) == FALSE)
				return (EFAULT);
			error = uiomove((caddr_t)v, c, uio);
			if (uio->uio_rw == UIO_WRITE)
				icache_flush(v, v + c);
		}
	}
	if (buf != NULL)
		free(buf, M_DEVBUF);
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
