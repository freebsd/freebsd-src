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
 * $FreeBSD$
 */

/*
 * Memory special file
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/memrange.h>
#include <sys/proc.h>
#include <sys/msgbuf.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/uio.h>

#ifdef PERFMON
#include <machine/perfmon.h>
#endif

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

static dev_t memdev, kmemdev;
#ifdef PERFMON
static dev_t perfdev;
#endif /* PERFMON */

static	d_open_t	mmopen;
static	d_close_t	mmclose;
static	d_read_t	mmrw;
static	d_ioctl_t	mmioctl;
static	d_mmap_t	memmmap;

#define CDEV_MAJOR 2
static struct cdevsw mem_cdevsw = {
	/* open */	mmopen,
	/* close */	mmclose,
	/* read */	mmrw,
	/* write */	mmrw,
	/* ioctl */	mmioctl,
	/* poll */	(d_poll_t *)seltrue,
	/* mmap */	memmmap,
	/* strategy */	nostrategy,
	/* name */	"mem",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_MEM,
};

struct mem_range_softc mem_range_softc;

static int
mmclose(dev_t dev, int flags, int fmt, struct thread *td)
{
	switch (minor(dev)) {
#ifdef PERFMON
	case 32:
		return perfmon_close(dev, flags, fmt, td);
#endif
	default:
		break;
	}
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
			if (error)
				return (error);
		}
		break;
	case 32:
#ifdef PERFMON
		return perfmon_open(dev, flags, fmt, td);
#else
		return ENODEV;
#endif
	default:
		break;
	}
	return (0);
}

/*ARGSUSED*/
static int
mmrw(dev_t dev, struct uio *uio, int flags)
{
	vm_offset_t o, v;
	int c = 0;
	struct iovec *iov;
	int error = 0, rw;
	vm_offset_t addr, eaddr;

	GIANT_REQUIRED;

	while (uio->uio_resid > 0 && !error) {
		iov = uio->uio_iov;
		if (iov->iov_len == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			if (uio->uio_iovcnt < 0)
				panic("mmrw");
			continue;
		}
		switch (minor(dev)) {

/* minor device 0 is physical memory */
		case 0:
			v = uio->uio_offset;
kmemphys:
			/* Allow reads only in RAM. */
			rw = (uio->uio_rw == UIO_READ) ? VM_PROT_READ : VM_PROT_WRITE;
			if ((alpha_pa_access(v) & rw) != rw) {
				error = EFAULT;
				c = 0;
				break;
			}

			o = uio->uio_offset & PAGE_MASK;
			c = min(uio->uio_resid, (int)(PAGE_SIZE - o));
			error =
			    uiomove((caddr_t)ALPHA_PHYS_TO_K0SEG(v), c, uio);
			continue;

/* minor device 1 is kernel memory */
		case 1:
			v = uio->uio_offset;

			if (v >= ALPHA_K0SEG_BASE && v <= ALPHA_K0SEG_END) {
				v = ALPHA_K0SEG_TO_PHYS(v);
				goto kmemphys;
			}

			c = min(iov->iov_len, MAXPHYS);
			/*
			 * Make sure that all of the pages are currently resident so
			 * that we don't create any zero-fill pages.
			 */
			addr = trunc_page(v);
			eaddr = round_page(v + c);
			for (; addr < eaddr; addr += PAGE_SIZE) 
				if (pmap_extract(kernel_pmap, addr) == 0) {
					return EFAULT;
				}
			if (!kernacc((caddr_t)v, c,
			    uio->uio_rw == UIO_READ ? 
			    VM_PROT_READ : VM_PROT_WRITE)) {
				return (EFAULT);
			}
			error = uiomove((caddr_t)v, c, uio);
			continue;
		}

		if (error)
			break;
		iov->iov_base = (char *)iov->iov_base + c;
		iov->iov_len -= c;
		uio->uio_offset += c;
		uio->uio_resid -= c;
	}
	return (error);
}

/*******************************************************\
* allow user processes to MMAP some memory sections	*
* instead of going through read/write			*
\*******************************************************/
static int
memmmap(dev_t dev, vm_offset_t offset, int prot)
{
	/*
	 * /dev/mem is the only one that makes sense through this
	 * interface.  For /dev/kmem any physaddr we return here
	 * could be transient and hence incorrect or invalid at
	 * a later time.
	 */
	if (minor(dev) != 0)
		return (-1);

	/*
	 * Allow access only in RAM.
	 */
	if ((prot & alpha_pa_access(atop((vm_offset_t)offset))) != prot)
		return (-1);
	return (alpha_btop(ALPHA_PHYS_TO_K0SEG(offset)));
}

static int
mmioctl(dev_t dev, u_long cmd, caddr_t cmdarg, int flags, struct thread *td)
{
	switch(minor(dev)) {
#ifdef PERFMON
	case 32:
		return perfmon_ioctl(dev, cmd, cmdarg, flags, td);
#endif
	default:
		return ENODEV;
	}

	return (0);
}

static int
mem_modevent(module_t mod, int type, void *data)
{
	switch(type) {
	case MOD_LOAD:
		if (bootverbose)
			printf("mem: <memory & I/O>\n");
/* XXX - ??? */
#if 0
		/* Initialise memory range handling */
		if (mem_range_softc.mr_op != NULL)
			mem_range_softc.mr_op->init(&mem_range_softc);
#endif

		memdev = make_dev(&mem_cdevsw, 0, UID_ROOT, GID_KMEM,
			0640, "mem");
		kmemdev = make_dev(&mem_cdevsw, 1, UID_ROOT, GID_KMEM,
			0640, "kmem");
#ifdef PERFMON
		perfdev = make_dev(&mem_cdevsw, 32, UID_ROOT, GID_KMEM,
			0640, "perfmon");
#endif /* PERFMON */
		return 0;

	case MOD_UNLOAD:
		destroy_dev(memdev);
		destroy_dev(kmemdev);
#ifdef PERFMON
		destroy_dev(perfdev);
#endif /* PERFMON */
		return 0;

	case MOD_SHUTDOWN:
		return 0;

	default:
		return EOPNOTSUPP;
	}
}

DEV_MODULE(mem, mem_modevent, NULL);
