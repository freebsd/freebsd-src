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
 * that will work with this driver from ftp://ftp.cs.uwm.edu/pub/FreeBSD.
 * The library contains the source that I can release as well as several
 * object modules and functions that allows one to read spigot data.
 * See the code for spigot_grab.c that is included with the library
 * data.
 *
 * The vendor will not allow me to release the spigot library code.
 * Please don't ask me for it.
 *
 * To use this driver you will need the spigot library.  The library is
 * available from:
 *
 *	ftp.cs.uwm.edu://pub/FreeBSD/spigot/spigot.tar.gz
 *
 * Version 1.5, August 30, 1995.
 *
 */

#include	"spigot.h"
#if NSPIGOT > 0

#if NSPIGOT > 1
error "Can only have 1 spigot configured."
#endif

#include	<sys/param.h>
#include	<sys/systm.h>
#include	<sys/kernel.h>
#include	<sys/ioctl.h>
#include	<sys/proc.h>
#include	<sys/user.h>
#include	<sys/file.h>
#include	<sys/uio.h>
#include	<sys/malloc.h>
#include	<sys/devconf.h>
#include	<sys/errno.h>
#include	<sys/mman.h>

#include	<machine/spigot.h>
#include	<machine/psl.h>

#include	<i386/isa/isa.h>
#include	<i386/isa/isa_device.h>

struct spigot_softc {
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

int	spigot_probe(struct isa_device *id);
int	spigot_attach(struct isa_device *id);

struct isa_driver	spigotdriver = {spigot_probe, spigot_attach, "spigot"};

static struct kern_devconf kdc_spigot[NSPIGOT] = { {
	0,			/* kdc_next -> filled in by dev_attach() */
	0,			/* kdc_rlink -> filled in by dev_attach() */
	0,			/* kdc_number -> filled in by dev_attach() */
	"spigot",			/* kdc_name */
	0,				/* kdc_unit */
	{ 				/* kdc_md */
  	   MDDT_ISA,			/* mddc_devtype */
	   0				/* mddc_flags */
					/* mddc_imask[4] */
	},
	isa_generic_externalize,	/* kdc_externalize */
	0,				/* kdc_internalize */
	0,				/* kdc_goaway */
	ISA_EXTERNALLEN,		/* kdc_datalen */
	&kdc_isa0,			/* kdc_parent */
	0,				/* kdc_parentdata */
	DC_UNCONFIGURED,	/* kdc_state - not supported */
	"Video Spigot frame grabber",	/* kdc_description */
	DC_CLS_MISC		/* class */
} };

static inline void
spigot_registerdev(struct isa_device *id)
{
	if(id->id_unit)
		kdc_spigot[id->id_unit] = kdc_spigot[0];
	kdc_spigot[id->id_unit].kdc_unit = id->id_unit;
	kdc_spigot[id->id_unit].kdc_isa = id;
	dev_attach(&kdc_spigot[id->id_unit]);
}

int
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
		spigot_registerdev(devp);
	}

	return(status);
}

int
spigot_attach(struct isa_device *devp)
{
struct	spigot_softc	*ss=(struct spigot_softc *)&spigot_softc[devp->id_unit];

	kdc_spigot[devp->id_unit].kdc_state = DC_UNKNOWN;

	ss->maddr = kvtop(devp->id_maddr);
	ss->irq = devp->id_irq;

	return 1;
}

int
spigot_open(dev_t dev, int flags, int fmt, struct proc *p)
{
struct	spigot_softc	*ss = (struct spigot_softc *)&spigot_softc[UNIT(dev)];

	if((ss->flags & ALIVE) == 0)
		return ENXIO;

	if(ss->flags & OPEN)
		return EBUSY;

	ss->flags |= OPEN;
	ss->p = 0;
	ss->signal_num = 0;

	return 0;
}

int
spigot_close(dev_t dev, int flags, int fmt, struct proc *p)
{
struct	spigot_softc	*ss = (struct spigot_softc *)&spigot_softc[UNIT(dev)];

	ss->flags &= ~OPEN;
	ss->p = 0;
	ss->signal_num = 0;

	outb(0xad6, 0);

	return 0;
}

int
spigot_write(dev_t dev, struct uio *uio, int ioflag)
{
struct	spigot_softc	*ss = (struct spigot_softc *)&spigot_softc[UNIT(dev)];

	return ENXIO;
}

int
spigot_read(dev_t dev, struct uio *uio, int ioflag)
{
struct	spigot_softc	*ss = (struct spigot_softc *)&spigot_softc[UNIT(dev)];

	return ENXIO;
}


int
spigot_ioctl(dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{
int			error;
struct	spigot_softc	*ss = (struct spigot_softc *)&spigot_softc[UNIT(dev)];
struct	trapframe	*fp;
struct	spigot_info	*info;

	if(!data) return(EINVAL);
	switch(cmd){
	case	SPIGOT_SETINT:
		ss->p = p;
		ss->signal_num = *((int *)data);
		break;
	case	SPIGOT_IOPL_ON:	/* allow access to the IO PAGE */
		error = suser(p->p_ucred, &p->p_acflag);
		if (error != 0)
			return error;
		fp=(struct trapframe *)p->p_md.md_regs;
		fp->tf_eflags |= PSL_IOPL;
		break;
	case	SPIGOT_IOPL_OFF: /* deny access to the IO PAGE */
		fp=(struct trapframe *)p->p_md.md_regs;
		fp->tf_eflags &= ~PSL_IOPL;
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

int
spigot_select(dev_t dev, int rw, struct proc *p)
{
struct	spigot_softc	*ss = (struct spigot_softc *)&spigot_softc[UNIT(dev)];
int			s;
int			r;

	return ENXIO;
}

/*
 * Interrupt procedure.
 * Just call a user level interrupt routine.
 */
void
spigintr(int unit)
{
struct	spigot_softc	*ss = (struct spigot_softc *)&spigot_softc[unit];

	if(ss->p && ss->signal_num)
		psignal(ss->p, ss->signal_num);
}

int
spigot_mmap(dev_t dev, int offset, int nprot)
{
struct	spigot_softc	*ss = (struct spigot_softc *)&spigot_softc[0];

	if(offset != 0) {
		printf("spigot mmap failed, offset = 0x%x != 0x0\n", offset);
		return -1;
	}

	if(nprot & PROT_EXEC)
		return -1;

	return i386_btop(ss->maddr);
}

#endif /* NSPIGOT */
