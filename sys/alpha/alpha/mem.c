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
 * $FreeBSD: src/sys/alpha/alpha/mem.c,v 1.19.2.3 2000/05/14 00:29:44 obrien Exp $
 */

/*
 * Memory special file
 */


#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/msgbuf.h>
#include <sys/random.h>
#include <sys/signalvar.h>

#include <machine/frame.h>
#include <machine/psl.h>
#ifdef PERFMON
#include <machine/perfmon.h>
#endif

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

static caddr_t zbuf;

static	d_open_t	mmopen;
static	d_close_t	mmclose;
static	d_read_t	mmrw;
static	d_ioctl_t	mmioctl;
static	d_mmap_t	memmmap;
static	d_poll_t	mmpoll;

#define CDEV_MAJOR 2
static struct cdevsw mem_cdevsw = {
	/* open */	mmopen,
	/* close */	mmclose,
	/* read */	mmrw,
	/* write */	mmrw,
	/* ioctl */	mmioctl,
	/* poll */	mmpoll,
	/* mmap */	memmmap,
	/* strategy */	nostrategy,
	/* name */	"mem",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_MEM,
	/* bmaj */	-1
};

/*
	XXX  the below should be used.  However there is too much "16"
	hardcodeing in kern_random.c right now. -- obrien
#if NHWI > 0
#define	ICU_LEN (NHWI)
#else
#define	ICU_LEN (NSWI)
#endif
*/
#define	ICU_LEN 16

static struct random_softc random_softc[ICU_LEN];
static int random_ioctl __P((dev_t, u_long, caddr_t, int, struct proc *));

static int
mmclose(dev, flags, fmt, p)
	dev_t dev;
	int flags;
	int fmt;
	struct proc *p;
{
	switch (minor(dev)) {
#ifdef PERFMON
	case 32:
		return perfmon_close(dev, flags, fmt, p);
#endif
	default:
		break;
	}
	return(0);
}

static int
mmopen(dev, flags, fmt, p)
	dev_t dev;
	int flags;
	int fmt;
	struct proc *p;
{

	switch (minor(dev)) {
	case 0:
	case 1:
		if (securelevel >= 1)
			return (EPERM);
		break;
	case 32:
#ifdef PERFMON
		return perfmon_open(dev, flags, fmt, p);
#else
		return ENODEV;
#endif
	default:
		break;
	}
	return(0);
}

/*ARGSUSED*/
int
mmrw(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	register vm_offset_t o, v;
	register int c;
	register struct iovec *iov;
	int error = 0, rw;
	u_int poolsize;
	caddr_t buf;

	buf = NULL;

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
		case 1: {
			vm_offset_t addr, eaddr;
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
				if (pmap_extract(kernel_pmap, addr) == 0)
					return EFAULT;
			
			if (!kernacc((caddr_t)v, c,
			    uio->uio_rw == UIO_READ ? 
			    VM_PROT_READ : VM_PROT_WRITE))
				return (EFAULT);
			error = uiomove((caddr_t)v, c, uio);
			continue;
		}

/* minor device 2 is EOF/rathole */
		case 2:
			if (uio->uio_rw == UIO_READ)
				return (0);
			c = iov->iov_len;
			break;

/* minor device 3 (/dev/random) is source of filth on read, rathole on write */
		case 3:
			if (uio->uio_rw == UIO_WRITE) {
				c = iov->iov_len;
				break;
			}
			if (buf == NULL)
				buf = (caddr_t)
				    malloc(PAGE_SIZE, M_TEMP, M_WAITOK);
			c = min(iov->iov_len, PAGE_SIZE);
			poolsize = read_random(buf, c);
			if (poolsize == 0) {
				if (buf)
					free(buf, M_TEMP);
				return (0);
			}
			c = min(c, poolsize);
			error = uiomove(buf, c, uio);
			continue;

/* minor device 4 (/dev/urandom) is source of muck on read, rathole on write */
		case 4:
			if (uio->uio_rw == UIO_WRITE) {
				c = iov->iov_len;
				break;
			}
			if (CURSIG(curproc) != 0) {
				/*
				 * Use tsleep() to get the error code right.
				 * It should return immediately.
				 */
				error = tsleep(&random_softc[0],
				    PZERO | PCATCH, "urand", 1);
				if (error != 0 && error != EWOULDBLOCK)
					continue;
			}
			if (buf == NULL)
				buf = (caddr_t)
				    malloc(PAGE_SIZE, M_TEMP, M_WAITOK);
			c = min(iov->iov_len, PAGE_SIZE);
			poolsize = read_random_unlimited(buf, c);
			c = min(c, poolsize);
			error = uiomove(buf, c, uio);
			continue;

/* minor device 12 (/dev/zero) is source of nulls on read, rathole on write */
		case 12:
			if (uio->uio_rw == UIO_WRITE) {
				c = iov->iov_len;
				break;
			}
			/*
			 * On the first call, allocate and zero a page
			 * of memory for use with /dev/zero.
			 */
			if (zbuf == NULL) {
				zbuf = (caddr_t)
				    malloc(PAGE_SIZE, M_TEMP, M_WAITOK);
				bzero(zbuf, PAGE_SIZE);
			}
			c = min(iov->iov_len, PAGE_SIZE);
			error = uiomove(zbuf, c, uio);
			continue;

		default:
			return (ENXIO);
		}
		if (error)
			break;
		iov->iov_base += c;
		iov->iov_len -= c;
		uio->uio_offset += c;
		uio->uio_resid -= c;
	}
	if (buf)
		free(buf, M_TEMP);
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
	 * a later time.  /dev/null just doesn't make any sense
	 * and /dev/zero is a hack that is handled via the default
	 * pager in mmap().
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

/*
 * Allow userland to select which interrupts will be used in the muck
 * gathering business.
 */
static int
mmioctl(dev, cmd, cmdarg, flags, p)
	dev_t dev;
	u_long cmd;
	caddr_t cmdarg;
	int flags;
	struct proc *p;
{
#if 0
	static u_int16_t interrupt_allowed = 0;
	u_int16_t interrupt_mask;
#endif

	switch(minor(dev)) {
	case 3:
	case 4:
		return random_ioctl(dev, cmd, cmdarg, flags, p);

#ifdef PERFMON
	case 32:
		return perfmon_ioctl(dev, cmd, cmdarg, flags, p);
#endif
	default:
		return ENODEV;
	}

	if (*(u_int16_t *)cmdarg >= 16)
		return (EINVAL);

#if 0
	/* Only root can do this */
	error = suser(p);
	if (error) {
		return (error);
	}
	interrupt_mask = 1 << *(u_int16_t *)cmdarg;

	switch (cmd) {

		case MEM_SETIRQ:
			if (!(interrupt_allowed & interrupt_mask)) {
				disable_intr();
				interrupt_allowed |= interrupt_mask;
				sec_intr_handler[*(u_int16_t *)cmdarg] =
					intr_handler[*(u_int16_t *)cmdarg];
				intr_handler[*(u_int16_t *)cmdarg] =
					add_interrupt_randomness;
				sec_intr_unit[*(u_int16_t *)cmdarg] =
					intr_unit[*(u_int16_t *)cmdarg];
				intr_unit[*(u_int16_t *)cmdarg] =
					*(u_int16_t *)cmdarg;
				enable_intr();
			}
			else return (EPERM);
			break;

		case MEM_CLEARIRQ:
			if (interrupt_allowed & interrupt_mask) {
				disable_intr();
				interrupt_allowed &= ~(interrupt_mask);
				intr_handler[*(u_int16_t *)cmdarg] =
					sec_intr_handler[*(u_int16_t *)cmdarg];
				intr_unit[*(u_int16_t *)cmdarg] =
					sec_intr_unit[*(u_int16_t *)cmdarg];
				enable_intr();
			}
			else return (EPERM);
			break;

		case MEM_RETURNIRQ:
			*(u_int16_t *)cmdarg = interrupt_allowed;
			break;

		default:
			return (ENOTTY);
	}
#endif
	return (0);
}

int
mmpoll(dev, events, p)
	dev_t dev;
	int events;
	struct proc *p;
{
	switch (minor(dev)) {
	case 3:		/* /dev/random */
	    /* return random_poll(dev, events, p);*/
	case 4:		/* /dev/urandom */
	default:
		return seltrue(dev, events, p);
	}
}

int
iszerodev(dev)
	dev_t dev;
{
	return (((major(dev) == mem_cdevsw.d_maj)
		 && minor(dev) == 12)
/* or the osf/1 zero device */
		||((major(dev) == 0) 
		   && (minor(dev) == 0x02600000)));
}

static void
mem_drvinit(void *unused)
{

	cdevsw_add(&mem_cdevsw);
	make_dev(&mem_cdevsw, 0, UID_ROOT, GID_KMEM, 0640, "mem");
	make_dev(&mem_cdevsw, 1, UID_ROOT, GID_KMEM, 0640, "kmem");
	make_dev(&mem_cdevsw, 2, UID_ROOT, GID_WHEEL, 0666, "null");
	make_dev(&mem_cdevsw, 3, UID_ROOT, GID_WHEEL, 0644, "random");
	make_dev(&mem_cdevsw, 4, UID_ROOT, GID_WHEEL, 0644, "urandom");
	make_dev(&mem_cdevsw, 12, UID_ROOT, GID_WHEEL, 0666, "zero");
#ifdef PERFMON
	make_dev(&mem_cdevsw, 32, UID_ROOT, GID_KMEM, 0640, "perfmon");
#endif /* PERFMON */
}

static int 
random_ioctl(dev, cmd, data, flags, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	return (0);
}

SYSINIT(memdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,mem_drvinit,NULL)
