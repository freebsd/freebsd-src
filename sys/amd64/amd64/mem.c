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
 *	Utah $Hdr: mem.c 1.13 89/10/08$
 *	@(#)mem.c	7.2 (Berkeley) 5/9/91
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Memory special file
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/memrange.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/systm.h>
#include <sys/uio.h>

#include <machine/db_machdep.h>
#include <machine/frame.h>
#include <machine/psl.h>
#include <machine/specialreg.h>
#include <machine/vmparam.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

static dev_t memdev, kmemdev, iodev;

static	d_open_t	mmopen;
static	d_close_t	mmclose;
static	d_read_t	mmrw;
static	d_ioctl_t	mmioctl;
static	d_mmap_t	memmmap;

#define CDEV_MAJOR 2
static struct cdevsw mem_cdevsw = {
	.d_open =	mmopen,
	.d_close =	mmclose,
	.d_read =	mmrw,
	.d_write =	mmrw,
	.d_ioctl =	mmioctl,
	.d_mmap =	memmmap,
	.d_name =	"mem",
	.d_maj =	CDEV_MAJOR,
	.d_flags =	D_MEM,
};

MALLOC_DEFINE(M_MEMDESC, "memdesc", "memory range descriptors");

struct mem_range_softc mem_range_softc;

static int
mmclose(dev_t dev, int flags, int fmt, struct thread *td)
{
	switch (minor(dev)) {
	case 14:
		td->td_frame->tf_rflags &= ~PSL_IOPL;
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
			if (error != 0)
				return (error);
		}
		break;
	case 14:
		error = suser(td);
		if (error != 0)
			return (error);
		error = securelevel_gt(td->td_ucred, 0);
		if (error != 0)
			return (error);
		td->td_frame->tf_rflags |= PSL_IOPL;
		break;
	}
	return (0);
}

/*ARGSUSED*/
static int
mmrw(dev_t dev, struct uio *uio, int flags)
{
	int o;
	u_long c = 0, v;
	struct iovec *iov;
	int error = 0;
	vm_offset_t addr, eaddr;

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

/* minor device 0 is physical memory */
		case 0:
			v = uio->uio_offset;
kmemphys:
			o = v & PAGE_MASK;
			c = min(uio->uio_resid, (u_int)(PAGE_SIZE - o));
			error = uiomove((void *)PHYS_TO_DMAP(v), (int)c, uio);
			continue;

/* minor device 1 is kernel memory */
		case 1:
			v = uio->uio_offset;

			if (v >= DMAP_MIN_ADDRESS && v < DMAP_MAX_ADDRESS) {
				v = DMAP_TO_PHYS(v);
				goto kmemphys;
			}

			c = iov->iov_len;
			/*
			 * Make sure that all of the pages are currently resident so
			 * that we don't create any zero-fill pages.
			 */
			addr = trunc_page(v);
			eaddr = round_page(v + c);

			if (addr < (vm_offset_t)KERNBASE)
				return (EFAULT);
			for (; addr < eaddr; addr += PAGE_SIZE) 
				if (pmap_extract(kernel_pmap, addr) == 0)
					return (EFAULT);

			if (!kernacc((caddr_t)(long)v, c,
			    uio->uio_rw == UIO_READ ? 
			    VM_PROT_READ : VM_PROT_WRITE))
				return (EFAULT);

			error = uiomove((caddr_t)(long)v, (int)c, uio);
			continue;

		default:
			return (ENODEV);
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
memmmap(dev_t dev, vm_offset_t offset, vm_paddr_t *paddr, int prot)
{
	switch (minor(dev))
	{

	/* minor device 0 is physical memory */
	case 0:
		*paddr = offset;
		break;

	/* minor device 1 is kernel memory */
	case 1:
        	*paddr = vtophys(offset);
		break;

	default:
		return (-1);
	}
	return (0);
}

/*
 * Operations for changing memory attributes.
 *
 * This is basically just an ioctl shim for mem_range_attr_get
 * and mem_range_attr_set.
 */
static int 
mmioctl(dev_t dev, u_long cmd, caddr_t data, int flags, struct thread *td)
{
	int nd, error = 0;
	struct mem_range_op *mo = (struct mem_range_op *)data;
	struct mem_range_desc *md;
	
	/* is this for us? */
	if ((cmd != MEMRANGE_GET) &&
	    (cmd != MEMRANGE_SET))
		return (ENOTTY);

	/* any chance we can handle this? */
	if (mem_range_softc.mr_op == NULL)
		return (EOPNOTSUPP);

	/* do we have any descriptors? */
	if (mem_range_softc.mr_ndesc == 0)
		return (ENXIO);

	switch (cmd) {
	case MEMRANGE_GET:
		nd = imin(mo->mo_arg[0], mem_range_softc.mr_ndesc);
		if (nd > 0) {
			md = (struct mem_range_desc *)
				malloc(nd * sizeof(struct mem_range_desc),
				       M_MEMDESC, M_WAITOK);
			error = mem_range_attr_get(md, &nd);
			if (!error)
				error = copyout(md, mo->mo_desc, 
					nd * sizeof(struct mem_range_desc));
			free(md, M_MEMDESC);
		}
		else
			nd = mem_range_softc.mr_ndesc;
		mo->mo_arg[0] = nd;
		break;
		
	case MEMRANGE_SET:
		md = (struct mem_range_desc *)malloc(sizeof(struct mem_range_desc),
						    M_MEMDESC, M_WAITOK);
		error = copyin(mo->mo_desc, md, sizeof(struct mem_range_desc));
		/* clamp description string */
		md->mr_owner[sizeof(md->mr_owner) - 1] = 0;
		if (error == 0)
			error = mem_range_attr_set(md, &mo->mo_arg[0]);
		free(md, M_MEMDESC);
		break;
	}
	return (error);
}

/*
 * Implementation-neutral, kernel-callable functions for manipulating
 * memory range attributes.
 */
int
mem_range_attr_get(struct mem_range_desc *mrd, int *arg)
{
	/* can we handle this? */
	if (mem_range_softc.mr_op == NULL)
		return (EOPNOTSUPP);

	if (*arg == 0)
		*arg = mem_range_softc.mr_ndesc;
	else
		bcopy(mem_range_softc.mr_desc, mrd,
			(*arg) * sizeof(struct mem_range_desc));
	return (0);
}

int
mem_range_attr_set(struct mem_range_desc *mrd, int *arg)
{
	/* can we handle this? */
	if (mem_range_softc.mr_op == NULL)
		return (EOPNOTSUPP);

	return (mem_range_softc.mr_op->set(&mem_range_softc, mrd, arg));
}

#ifdef SMP
void
mem_range_AP_init(void)
{
	if (mem_range_softc.mr_op && mem_range_softc.mr_op->initAP)
		(mem_range_softc.mr_op->initAP(&mem_range_softc));
}
#endif

static int
mem_modevent(module_t mod, int type, void *data)
{
	switch(type) {
	case MOD_LOAD:
		if (bootverbose)
			printf("mem: <memory & I/O>\n");
		/* Initialise memory range handling */
		if (mem_range_softc.mr_op != NULL)
			mem_range_softc.mr_op->init(&mem_range_softc);

		memdev = make_dev(&mem_cdevsw, 0, UID_ROOT, GID_KMEM,
			0640, "mem");
		kmemdev = make_dev(&mem_cdevsw, 1, UID_ROOT, GID_KMEM,
			0640, "kmem");
		iodev = make_dev(&mem_cdevsw, 14, UID_ROOT, GID_WHEEL,
			0600, "io");
		return (0);

	case MOD_UNLOAD:
		destroy_dev(memdev);
		destroy_dev(kmemdev);
		destroy_dev(iodev);
		return (0);

	case MOD_SHUTDOWN:
		return (0);

	default:
		return (EOPNOTSUPP);
	}
}

DEV_MODULE(mem, mem_modevent, NULL);
