/*
 * Video spigot capture driver.
 *
 * Copyright (c) 1995, Jim Lowe.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. 2.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This is the minimum driver code required to make a spigot work.
 * Unfortunatly, I can't include a real driver since the information
 * on the spigot is under non-disclosure.  You can pick up a library
 * that will work with this driver from
 * ftp://ftp.cs.uwm.edu/pub/FreeBSD-UWM.  The library contains the
 * source that I can release as well as several object modules and
 * functions that allows one to read spigot data.  See the code for
 * spigot_grab.c that is included with the library data.
 *
 * The vendor will not allow me to release the spigot library code.
 * Please don't ask me for it.
 *
 * To use this driver you will need the spigot library.  The library is
 * available from:
 *
 *	ftp.cs.uwm.edu://pub/FreeBSD-UWM/spigot/spigot.tar.gz
 *
 * Version 1.7, December 1995.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include	"spigot.h"

#if NSPIGOT > 1
error "Can only have 1 spigot configured."
#endif

#include	"opt_spigot.h"

#include	<sys/param.h>
#include	<sys/systm.h>
#include	<sys/bus.h>
#include	<sys/conf.h>
#include	<sys/kernel.h>
#include	<sys/lock.h>
#include	<sys/mman.h>
#include	<sys/mutex.h>
#include	<sys/proc.h>
#include	<sys/signalvar.h>

#include	<machine/frame.h>
#include	<machine/md_var.h>
#include	<machine/spigot.h>
#include	<machine/psl.h>

#include	<i386/isa/isa_device.h>

static struct spigot_softc {
	u_long		flags;
	u_long	 	maddr;
	struct proc	*p;
	u_long		signal_num;
	u_short		irq;
} spigot_softc[NSPIGOT];

/* flags in softc */
#define	OPEN		0x01
#define	ALIVE		0x02

#define	UNIT(dev) minor(dev)

static int	spigot_probe(struct isa_device *id);
static int	spigot_attach(struct isa_device *id);

struct isa_driver	spigotdriver = {
	INTR_TYPE_MISC,
	spigot_probe,
	spigot_attach,
	"spigot"
};
COMPAT_ISA_DRIVER(spigot, spigotdriver);

static	d_open_t	spigot_open;
static	d_close_t	spigot_close;
static	d_read_t	spigot_read;
static	d_write_t	spigot_write;
static	d_ioctl_t	spigot_ioctl;
static	d_mmap_t	spigot_mmap;

#define CDEV_MAJOR 11
static struct cdevsw spigot_cdevsw = {
	.d_open =	spigot_open,
	.d_close =	spigot_close,
	.d_read =	spigot_read,
	.d_write =	spigot_write,
	.d_ioctl =	spigot_ioctl,
	.d_mmap =	spigot_mmap,
	.d_name =	"spigot",
	.d_maj =	CDEV_MAJOR,
};

static ointhand2_t	spigintr;

static int
spigot_probe(struct isa_device *devp)
{
int			status;
struct	spigot_softc	*ss=(struct spigot_softc *)&spigot_softc[devp->id_unit];

	ss->flags = 0;
	ss->maddr = 0;
	ss->irq = 0;

	if(devp->id_iobase != 0xad6 || inb(0xad9) == 0xff) 
		status = 0;	/* not found */
	else {
		status = 1;	/* found */
		ss->flags |= ALIVE;
	}

	return(status);
}

static int
spigot_attach(struct isa_device *devp)
{
	int	unit;
	struct	spigot_softc	*ss= &spigot_softc[unit = devp->id_unit];

	devp->id_ointr = spigintr;
	ss->maddr = kvtop(devp->id_maddr);
	ss->irq = devp->id_irq;
	make_dev(&spigot_cdevsw, unit, 0, 0, 0644, "spigot%d", unit);
	return 1;
}

static	int
spigot_open(dev_t dev, int flags, int fmt, struct thread *td)
{
int			error;
struct	spigot_softc	*ss = (struct spigot_softc *)&spigot_softc[UNIT(dev)];

	if((ss->flags & ALIVE) == 0)
		return ENXIO;

	if(ss->flags & OPEN)
		return EBUSY;

#if !defined(SPIGOT_UNSECURE)
	/*
	 * Don't allow open() unless the process has sufficient privileges,
	 * since mapping the i/o page and granting i/o privilege would
	 * require sufficient privilege soon and nothing much can be done
	 * without them.
	 */
	error = suser(td);
	if (error != 0)
		return error;
	error = securelevel_gt(td->td_ucred, 0);
	if (error != 0)
		return error;
#endif

	ss->flags |= OPEN;
	ss->p = 0;
	ss->signal_num = 0;

	return 0;
}

static	int
spigot_close(dev_t dev, int flags, int fmt, struct thread *td)
{
struct	spigot_softc	*ss = (struct spigot_softc *)&spigot_softc[UNIT(dev)];

	ss->flags &= ~OPEN;
	ss->p = 0;
	ss->signal_num = 0;

	outb(0xad6, 0);

	return 0;
}

static	int
spigot_write(dev_t dev, struct uio *uio, int ioflag)
{
	return ENXIO;
}

static	int
spigot_read(dev_t dev, struct uio *uio, int ioflag)
{
	return ENXIO;
}


static	int
spigot_ioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
int			error;
struct	spigot_softc	*ss = (struct spigot_softc *)&spigot_softc[UNIT(dev)];
struct	spigot_info	*info;

	if(!data) return(EINVAL);
	switch(cmd){
	case	SPIGOT_SETINT:
		ss->p = td->td_proc;
		ss->signal_num = *((int *)data);
		break;
	case	SPIGOT_IOPL_ON:	/* allow access to the IO PAGE */
#if !defined(SPIGOT_UNSECURE)
		error = suser(td);
		if (error != 0)
			return error;
		error = securelevel_gt(td->td_ucred, 0);
		if (error)
			return error;
#endif
		td->td_frame->tf_eflags |= PSL_IOPL;
		break;
	case	SPIGOT_IOPL_OFF: /* deny access to the IO PAGE */
		td->td_frame->tf_eflags &= ~PSL_IOPL;
		break;
	case	SPIGOT_GET_INFO:
		info = (struct spigot_info *)data;
		info->maddr  = ss->maddr;
		info->irq = ss->irq;
		break;
	default:
		return ENOTTY;
	}

	return 0;
}

/*
 * Interrupt procedure.
 * Just call a user level interrupt routine.
 */
static void
spigintr(int unit)
{
struct	spigot_softc	*ss = (struct spigot_softc *)&spigot_softc[unit];

	if(ss->p && ss->signal_num) {
		PROC_LOCK(ss->p);
		psignal(ss->p, ss->signal_num);
		PROC_UNLOCK(ss->p);
	}
}

static	int
spigot_mmap(dev_t dev, vm_offset_t offset, vm_paddr_t *paddr, int nprot)
{
struct	spigot_softc	*ss = (struct spigot_softc *)&spigot_softc[0];

	if(offset != 0) {
		printf("spigot mmap failed, offset = 0x%x != 0x0\n", offset);
		return -1;
	}

	if(nprot & PROT_EXEC)
		return -1;

	*paddr = ss->maddr;
	return 0;
}
