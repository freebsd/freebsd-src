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
 * We are working with the vendor so I can release the code, please don't
 * ask me for it.  When/if I can release it, I will.
 *
 * Version 1.1, Feb 1, 1995.
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
	int		flags;
	caddr_t	 	maddr;
	struct proc	*p;
	int		signal_num;
} spigot_softc[NSPIGOT];

/* flags in softc */
#define	OPEN		0x01	

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

	spigot_registerdev(devp);

	if(devp->id_iobase != 0xad6 || inb(0xad9) == 0xff) 	/* ff if board isn't there??? */
		status = 0;
	else
		status = 1;

	return(status);
}

int
spigot_attach(struct isa_device *devp)
{
	struct	spigot_softc	*ss=(struct spigot_softc *)&spigot_softc[devp->id_unit];
	kdc_spigot[devp->id_unit].kdc_state = DC_UNKNOWN;

	ss->flags = 0;
	ss->maddr = devp->id_maddr;

	return 1;
}

int
spigot_open(dev_t dev, int flag)
{
struct	spigot_softc	*ss = (struct spigot_softc *)&spigot_softc[UNIT(dev)];


	if(ss->flags & OPEN)
		return EBUSY;
	ss->flags |= OPEN;
	ss->p = 0;
	ss->signal_num = 0;

	return 0;
}

int
spigot_close(dev_t dev, int flag)
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
struct	spigot_softc	*ss = (struct spigot_softc *)&spigot_softc[UNIT(dev)];
struct	trapframe	*fp;

	switch(cmd){
	case	SPIGOT_SETINT:
		ss->p = p;
		ss->signal_num = *((int *)data);
		break;
	case	SPIGOT_IOPL_ON:	/* allow access to the IO PAGE */
		fp=(struct trapframe *)p->p_md.md_regs;
		fp->tf_eflags |= PSL_IOPL;
		break;
	case	SPIGOT_IOPL_OFF: /* deny access to the IO PAGE */
		fp=(struct trapframe *)p->p_md.md_regs;
		fp->tf_eflags &= ~PSL_IOPL;
		break;
	default:
		printf("spigot ioctl: cmd=0x%x, '%c', num = %d, len=%d, %s\n",
                        cmd, IOCGROUP(cmd), cmd & 0xff, IOCPARM_LEN(cmd),
                        cmd&IOC_IN ? "in" : "out");
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

	if(offset != 0)
		return 0;

	if(nprot & PROT_EXEC)
		return 0;

	return i386_btop(ss->maddr);
}

#endif /* NSPIGOT */
