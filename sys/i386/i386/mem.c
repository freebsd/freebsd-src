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
 *	$Id: mem.c,v 1.36 1996/07/15 05:23:04 bde Exp $
 */

/*
 * Memory special file
 */

#include "opt_perfmon.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/buf.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /* DEVFS */
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <machine/cpu.h>
#include <machine/random.h>
#include <machine/psl.h>
#ifdef PERFMON
#include <machine/perfmon.h>
#endif

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/lock.h>
#include <vm/vm_prot.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>



static	d_open_t	mmopen;
static	d_close_t	mmclose;
static	d_read_t	mmrw;
static	d_ioctl_t	mmioctl;
static	d_mmap_t	memmmap;

#define CDEV_MAJOR 2
static struct cdevsw mem_cdevsw = 
	{ mmopen,	mmclose,	mmrw,		mmrw,		/*2*/
	  mmioctl,	nullstop,	nullreset,	nodevtotty,/* memory */
	  seltrue,	memmmap,	NULL,	"mem",	NULL, -1 };

static caddr_t	zbuf;

#ifdef DEVFS
static void *mem_devfs_token;
static void *kmem_devfs_token;
static void *null_devfs_token;
static void *random_devfs_token;
static void *urandom_devfs_token;
static void *zero_devfs_token;
static void *io_devfs_token;
#ifdef PERFMON
static void *perfmon_devfs_token;
#endif

static void memdevfs_init __P((void));

static void 
memdevfs_init()
{
    mem_devfs_token = 
	devfs_add_devswf(&mem_cdevsw, 0, DV_CHR, 
			 UID_ROOT, GID_KMEM, 0640, "mem");
    kmem_devfs_token = 
	devfs_add_devswf(&mem_cdevsw, 1, DV_CHR,
			 UID_ROOT, GID_KMEM, 0640, "kmem");
    null_devfs_token = 
	devfs_add_devswf(&mem_cdevsw, 2, DV_CHR, 
			 UID_ROOT, GID_WHEEL, 0666, "null");
    random_devfs_token = 
	devfs_add_devswf(&mem_cdevsw, 3, DV_CHR, 
			 UID_ROOT, GID_WHEEL, 0644, "random");
    urandom_devfs_token = 
	devfs_add_devswf(&mem_cdevsw, 4, DV_CHR, 
			 UID_ROOT, GID_WHEEL, 0644, "urandom");
    zero_devfs_token = 
	devfs_add_devswf(&mem_cdevsw, 12, DV_CHR, 
			 UID_ROOT, GID_WHEEL, 0666, "zero");
    io_devfs_token = 
	devfs_add_devswf(&mem_cdevsw, 14, DV_CHR, 
			 UID_ROOT, GID_WHEEL, 0600, "io");
#ifdef PERFMON
    perfmon_devfs_token = 
	devfs_add_devswf(&mem_cdevsw, 32, DV_CHR, 
			 UID_ROOT, GID_KMEM, 0640, "perfmon");
#endif /* PERFMON */
}
#endif /* DEVFS */

extern        char *ptvmmap;            /* poor name! */

static int
mmclose(dev, flags, fmt, p)
	dev_t dev;
	int flags;
	int fmt;
	struct proc *p;
{
	struct trapframe *fp;

	switch (minor(dev)) {
#ifdef PERFMON
	case 32:
		return perfmon_close(dev, flags, fmt, p);
#endif
	case 14:
		fp = (struct trapframe *)curproc->p_md.md_regs;
		fp->tf_eflags &= ~PSL_IOPL;
		break;
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
	struct trapframe *fp;

	switch (minor(dev)) {
	case 32:
#ifdef PERFMON
		return perfmon_open(dev, flags, fmt, p);
#else
		return ENODEV;
#endif
	case 14:
		fp = (struct trapframe *)curproc->p_md.md_regs;
		fp->tf_eflags |= PSL_IOPL;
		break;
	default:
		break;
	}
	return(0);
}

static int
mmrw(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	register int o;
	register u_int c, v;
	u_int poolsize;
	register struct iovec *iov;
	int error = 0;
	caddr_t buf = NULL;

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

/* minor device 0 is physical memory */
		case 0:
			v = uio->uio_offset;
			pmap_enter(kernel_pmap, (vm_offset_t)ptvmmap, v,
				uio->uio_rw == UIO_READ ? VM_PROT_READ : VM_PROT_WRITE,
				TRUE);
			o = (int)uio->uio_offset & PAGE_MASK;
			c = (u_int)(PAGE_SIZE - ((int)iov->iov_base & PAGE_MASK));
			c = min(c, (u_int)(PAGE_SIZE - o));
			c = min(c, (u_int)iov->iov_len);
			error = uiomove((caddr_t)&ptvmmap[o], (int)c, uio);
			pmap_remove(kernel_pmap, (vm_offset_t)ptvmmap,
				    (vm_offset_t)&ptvmmap[PAGE_SIZE]);
			continue;

/* minor device 1 is kernel memory */
		case 1: {
			vm_offset_t addr, eaddr;
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
			
			if (!kernacc((caddr_t)(int)uio->uio_offset, c,
			    uio->uio_rw == UIO_READ ? B_READ : B_WRITE))
				return(EFAULT);
			error = uiomove((caddr_t)(int)uio->uio_offset, (int)c, uio);
			continue;
		}

/* minor device 2 is EOF/RATHOLE */
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
			error = uiomove(buf, (int)c, uio);
			continue;

/* minor device 4 (/dev/urandom) is source of muck on read, rathole on write */
		case 4:
			if (uio->uio_rw == UIO_WRITE) {
				c = iov->iov_len;
				break;
			}
			if (buf == NULL)
				buf = (caddr_t)
				    malloc(PAGE_SIZE, M_TEMP, M_WAITOK);
			c = min(iov->iov_len, PAGE_SIZE);
			poolsize = read_random_unlimited(buf, c);
			c = min(c, poolsize);
			error = uiomove(buf, (int)c, uio);
			continue;

/* minor device 12 (/dev/zero) is source of nulls on read, rathole on write */
		case 12:
			if (uio->uio_rw == UIO_WRITE) {
				c = iov->iov_len;
				break;
			}
			if (zbuf == NULL) {
				zbuf = (caddr_t)
				    malloc(PAGE_SIZE, M_TEMP, M_WAITOK);
				bzero(zbuf, PAGE_SIZE);
			}
			c = min(iov->iov_len, PAGE_SIZE);
			error = uiomove(zbuf, (int)c, uio);
			continue;

#ifdef notyet
/* 386 I/O address space (/dev/ioport[bwl]) is a read/write access to seperate
   i/o device address bus, different than memory bus. Semantics here are
   very different than ordinary read/write, as if iov_len is a multiple
   an implied string move from a single port will be done. Note that lseek
   must be used to set the port number reliably. */
		case 14:
			if (iov->iov_len == 1) {
				u_char tmp;
				tmp = inb(uio->uio_offset);
				error = uiomove (&tmp, iov->iov_len, uio);
			} else {
				if (!useracc((caddr_t)iov->iov_base,
					iov->iov_len, uio->uio_rw))
					return (EFAULT);
				insb(uio->uio_offset, iov->iov_base,
					iov->iov_len);
			}
			break;
		case 15:
			if (iov->iov_len == sizeof (short)) {
				u_short tmp;
				tmp = inw(uio->uio_offset);
				error = uiomove (&tmp, iov->iov_len, uio);
			} else {
				if (!useracc((caddr_t)iov->iov_base,
					iov->iov_len, uio->uio_rw))
					return (EFAULT);
				insw(uio->uio_offset, iov->iov_base,
					iov->iov_len/ sizeof (short));
			}
			break;
		case 16:
			if (iov->iov_len == sizeof (long)) {
				u_long tmp;
				tmp = inl(uio->uio_offset);
				error = uiomove (&tmp, iov->iov_len, uio);
			} else {
				if (!useracc((caddr_t)iov->iov_base,
					iov->iov_len, uio->uio_rw))
					return (EFAULT);
				insl(uio->uio_offset, iov->iov_base,
					iov->iov_len/ sizeof (long));
			}
			break;
#endif

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
memmmap(dev_t dev, int offset, int nprot)
{
	switch (minor(dev))
	{

/* minor device 0 is physical memory */
	case 0:
        	return i386_btop(offset);

/* minor device 1 is kernel memory */
	case 1:
        	return i386_btop(vtophys(offset));

	default:
		return -1;
	}
}

/*
 * Allow userland to select which interrupts will be used in the muck
 * gathering business.
 */
static int
mmioctl(dev, cmd, cmdarg, flags, p)
	dev_t dev;
	int cmd;
	caddr_t cmdarg;
	int flags;
	struct proc *p;
{
	static u_int16_t interrupt_allowed = 0;
	u_int16_t interrupt_mask;
	int error;

	switch(minor(dev)) {
	case 3:
	case 4:
		break;

#ifdef PERFMON
	case 32:
		return perfmon_ioctl(dev, cmd, cmdarg, flags, p);
#endif
	default:
		return ENODEV;
	}

	if (*(u_int16_t *)cmdarg >= 16)
		return (EINVAL);

	/* Only root can do this */
	error = suser(p->p_ucred, &p->p_acflag);
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
	return (0);
}

/*
 * Routine that identifies /dev/mem and /dev/kmem.
 *
 * A minimal stub routine can always return 0.
 */
int
iskmemdev(dev)
	dev_t dev;
{

	return ((major(dev) == mem_cdevsw.d_maj)
	      && (minor(dev) == 0 || minor(dev) == 1));
}

int
iszerodev(dev)
	dev_t dev;
{
	return ((major(dev) == mem_cdevsw.d_maj)
	  && minor(dev) == 12);
}



static mem_devsw_installed = 0;

static void
mem_drvinit(void *unused)
{
	dev_t dev;

	if( ! mem_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&mem_cdevsw, NULL);
		mem_devsw_installed = 1;
#ifdef DEVFS
		memdevfs_init();
#endif
	}
}

SYSINIT(memdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,mem_drvinit,NULL)

