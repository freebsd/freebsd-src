/*
 *  Copyright (c) 2000-2004
 *          Diomidis D. Spinellis, Athens, Greece
 *      All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer as
 *     the first lines of this file unmodified.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY Diomidis D. Spinellis ``AS IS'' AND ANY
 *  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL Diomidis D. Spinellis BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 *  OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: pbio.c,v 1.12 2003/10/11 13:05:08 dds Exp dds $
 * $FreeBSD$
 *
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>		/* SYSINIT stuff */
#include <sys/bus.h>
#include <sys/resource.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/conf.h>		/* cdevsw stuff */
#include <sys/malloc.h>		/* malloc region definitions */
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/clock.h>	/* DELAY() */
#include <i386/isa/isa.h>	/* ISA bus port definitions etc. */
#include <i386/isa/isa_device.h>/* ISA bus configuration structures */
#include <sys/rman.h>
#include <sys/pbioio.h>		/* pbio IOCTL definitions */
#include <sys/uio.h>
#include <sys/fcntl.h>

#include "pbio.h"		/* generated file.. defines NPBIO */

/* Function prototypes (these should all be static) */
static  d_open_t	pbioopen;
static  d_close_t	pbioclose;
static  d_read_t	pbioread;
static  d_write_t	pbiowrite;
static  d_ioctl_t	pbioioctl;
static  d_poll_t	pbiopoll;
static	int		pbioprobe (device_t);
static	int		pbioattach (device_t);

/* Device registers */
#define	PBIO_PORTA	0
#define	PBIO_PORTB	1
#define	PBIO_PORTC	2
#define	PBIO_CFG	3
#define PBIO_IOSIZE	4

/* Per-port buffer size */
#define PBIO_BUFSIZ 64

/* Number of /dev entries */
#define PBIO_NPORTS 4

/* I/O port range */
#define IO_PBIOSIZE 4

static char *port_names[] = {"a", "b", "ch", "cl"};

#define	PBIO_PNAME(n)		(port_names[(n)])

#define UNIT(dev)		(minor(dev) >> 2)
#define PORT(dev)		(minor(dev) & 0x3)

#define CDEV_MAJOR 188

#define	PBIOPRI	((PZERO + 5) | PCATCH)

static struct cdevsw pbio_cdevsw = {
	pbioopen,
	pbioclose,
	pbioread,
	pbiowrite,
	pbioioctl,
	pbiopoll,
	nommap,
	nostrategy,
	"pbio",
	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
	/* bmaj */	-1
};

/*
 * Data specific to each I/O port
 */
struct portdata {
	dev_t	port;
	int	diff;			/* When true read only differences */
	int	ipace;			/* Input pace */
	int	opace;			/* Output pace */
	char	oldval;			/* Last value read */
	char	buff[PBIO_BUFSIZ];	/* Per-port data buffer */
};

/*
 * One of these per allocated device
 */
struct pbio_softc {
	int	iobase;			/* I/O base */
	struct portdata pd[PBIO_NPORTS];	/* Per port data */
	int	iomode;			/* Virtualized I/O mode port value */
					/* The real port is write-only */
} ;

typedef	struct pbio_softc *sc_p;

static device_method_t pbio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pbioprobe),
	DEVMETHOD(device_attach,	pbioattach),

	{ 0, 0 }
};

static	devclass_t	pbio_devclass;
#define	pbio_addr(unit)	((struct pbio_softc *) \
			 devclass_get_softc(pbio_devclass, unit))

static char driver_name[] = "pbio";

static driver_t pbio_driver = {
	driver_name,
	pbio_methods,
	sizeof(struct pbio_softc),
};


static int
pbioprobe (device_t dev)
{
	int iobase;
	int		rid;
	struct resource *port;
#ifdef GENERIC_PBIO_PROBE
	unsigned char val;
#endif

	rid = 0;
	port = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
				  0, ~0, IO_PBIOSIZE, RF_ACTIVE);
	if (!port)
		return (ENXIO);

	iobase = rman_get_start(port);

#ifdef GENERIC_PBIO_PROBE
	/*
	 * try see if the device is there.
	 * This probe works only if the device has no I/O attached to it
	 * XXX Better allow flags to abort testing
	 */
	/* Set all ports to output */
	outb(iobase + PBIO_CFG, 0x80);
	printf("pbio val(CFG: 0x%03x)=0x%02x (should be 0x80)\n",
		iobase, inb(iobase + PBIO_CFG));
	outb(iobase + PBIO_PORTA, 0xa5);
	val = inb(iobase + PBIO_PORTA);
	printf("pbio val=0x%02x (should be 0xa5)\n", val);
	if (val != 0xa5)
		return (ENXIO);
	outb(iobase + PBIO_PORTA, 0x5a);
	val = inb(iobase + PBIO_PORTA);
	printf("pbio val=0x%02x (should be 0x5a)\n", val);
	if (val != 0x5a)
		return (ENXIO);
#endif
	device_set_desc(dev, "Intel 8255 PPI (basic mode)");
	/* Set all ports to input */
	/* outb(iobase + PBIO_CFG, 0x9b); */
	bus_release_resource(dev, SYS_RES_IOPORT, rid, port);
	return (0);
}

/*
 * Called if the probe succeeded.
 * We can be destructive here as we know we have the device.
 * we can also trust the unit number.
 */
static int
pbioattach (device_t dev)
{
	int unit = device_get_unit(dev);
	int flags;
	int i;
	int iobase;
	int		rid;
	struct resource *port;
	struct pbio_softc *sc;

	rid = 0;
	port = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
				  0, ~0, IO_PBIOSIZE, RF_ACTIVE);
	if (!port)
		return (ENXIO);

	iobase = rman_get_start(port);
	sc = device_get_softc(dev);

	/*
	 * Allocate storage for this instance .
	 */
	bzero(sc, sizeof(*sc));
	flags = device_get_flags(dev);

	/*
	 * Store whatever seems wise.
	 */
	sc->iobase = iobase;
	sc->iomode = 0x9b;		/* All ports to input */

	for (i = 0; i < PBIO_NPORTS; i++)
		sc->pd[i].port = make_dev(&pbio_cdevsw, (unit << 2) + i, 0, 0, 0600,
				     "pbio%d%s", unit, PBIO_PNAME(i));
	return 0;
}

static int
pbioioctl (dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	int unit = UNIT(dev);
	int port = PORT(dev);
	struct pbio_softc *scp;
	
	scp = pbio_addr(unit);
	if (scp == NULL)
		return (ENODEV);
	switch (cmd) {
	    case PBIO_SETDIFF:
		scp->pd[port].diff = *(int *)data;
		break;
	    case PBIO_SETIPACE:
		scp->pd[port].ipace = *(int *)data;
		break;
	    case PBIO_SETOPACE:
		scp->pd[port].opace = *(int *)data;
		break;
	    case PBIO_GETDIFF:
		*(int *)data = scp->pd[port].diff;
		break;
	    case PBIO_GETIPACE:
		*(int *)data = scp->pd[port].ipace;
		break;
	    case PBIO_GETOPACE:
		*(int *)data = scp->pd[port].opace;
		break;
	    default:
		return ENXIO;
	}
	return (0);
}

static  int
pbioopen(dev_t dev, int oflags, int devtype, struct proc *p)
{
	int unit = UNIT(dev);
	int port = PORT(dev);
	struct pbio_softc *scp;
	int ocfg;
	int portbit;			/* Port configuration bit */
	
	scp = pbio_addr(unit);
	if (scp == NULL)
		return (ENODEV);
	
	switch (port) {
	case 0: portbit = 0x10; break;	/* Port A */
	case 1: portbit = 0x02; break;	/* Port B */
	case 2: portbit = 0x08; break;	/* Port CH */
	case 3: portbit = 0x01; break;	/* Port CL */
	default: return (ENODEV);
	}
	ocfg = scp->iomode;

	if (oflags & FWRITE)
		/* Writing == output; zero the bit */
		outb(scp->iobase + PBIO_CFG, scp->iomode = (ocfg & (~portbit)));
	else if (oflags & FREAD)
		/* Reading == input; set the bit */
		outb(scp->iobase + PBIO_CFG, scp->iomode = (ocfg | portbit));
	else
		return (EACCES);

	return (0);
}

static  int
pbioclose(dev_t dev, int fflag, int devtype, struct proc *p)
{
	int unit = UNIT(dev);
	struct pbio_softc *scp;
	
	scp = pbio_addr(unit);
	if (scp == NULL)
		return (ENODEV);
	
	return (0);
}

/*
 * Return the value of a given port on a given I/O base address
 * Handles the split C port nibbles and blocking
 */
static int
portval(int port, struct pbio_softc *scp, char *val)
{
	int err;

	for (;;) {
		switch (port) {
		case 0:
			*val = inb(scp->iobase + PBIO_PORTA);
			break;
		case 1:
			*val = inb(scp->iobase + PBIO_PORTB);
			break;
		case 2:
			*val = (inb(scp->iobase + PBIO_PORTC) >> 4) & 0xf;
			break;
		case 3:
			*val = inb(scp->iobase + PBIO_PORTC) & 0xf;
			break;
		default:
			*val = 0;
			break;
		}
		if (scp->pd[port].diff) {
			if (*val != scp->pd[port].oldval) {
				scp->pd[port].oldval = *val;
				return (0);
			}
			err = tsleep((caddr_t)&(scp->pd[port].diff), PBIOPRI,
				     "pbiopl", max(1, scp->pd[port].ipace));
			if (err == EINTR)
				return (EINTR);
		} else
			return (0);
	}
}

static  int
pbioread(dev_t dev, struct uio *uio, int ioflag)
{
	int unit = UNIT(dev);
	int port = PORT(dev);
	struct pbio_softc *scp;
	int     toread;
	int	err;
	int ret, i;
	char val;
	
	scp = pbio_addr(unit);
	if (scp == NULL)
		return (ENODEV);

	while (uio->uio_resid > 0) {
		toread = min(uio->uio_resid, PBIO_BUFSIZ);
		if ((ret = uiomove(scp->pd[port].buff, toread, uio)) != 0)
			return (ret);
		for (i = 0; i < toread; i++) {
			if ((err = portval(port, scp, &val)) != 0)
				return (err);
			scp->pd[port].buff[i] = val;
			if (!scp->pd[port].diff && scp->pd[port].ipace)
				tsleep((caddr_t)&(scp->pd[port].ipace), PBIOPRI,
					"pbioip", scp->pd[port].ipace);
		}
	}
	return 0;
}

static  int
pbiowrite(dev_t dev, struct uio *uio, int ioflag)
{
	int unit = UNIT(dev);
	int port = PORT(dev);
	struct pbio_softc *scp;
	char val, oval;
	int	towrite;
	int	ret;
	int iobase;
	int i;
	
	scp = pbio_addr(unit);
	if (scp == NULL)
		return (ENODEV);

	iobase = scp->iobase;
	while (uio->uio_resid > 0) {
		towrite = min(uio->uio_resid, PBIO_BUFSIZ);
		if ((ret = uiomove(scp->pd[port].buff, towrite, uio)) != 0)
			return (ret);
		for (i = 0; i < towrite; i++) {
			val = scp->pd[port].buff[i];
			switch (port) {
			case 0:
				outb(iobase + PBIO_PORTA, val);
				break;
			case 1:
				outb(iobase + PBIO_PORTB, val);
				break;
			case 2:
				oval = inb(iobase + PBIO_PORTC);
				oval &= 0xf;
				val <<= 4;
				outb(iobase + PBIO_PORTC, val | oval);
				break;
			case 3:
				oval = inb(iobase + PBIO_PORTC);
				oval &= 0xf0;
				val &= 0xf;
				outb(iobase + PBIO_PORTC, oval | val);
				break;
			}
			if (scp->pd[port].opace)
				tsleep((caddr_t)&(scp->pd[port].opace),
					PBIOPRI, "pbioop",
					scp->pd[port].opace);
		}
	}
	return (0);
}

static  int
pbiopoll(dev_t dev, int which, struct proc *p)
{
	int unit = UNIT(dev);
	struct pbio_softc *scp;
	
	scp = pbio_addr(unit);
	if (scp == NULL)
		return (ENODEV);
	
	/*
	 * Do processing
	 */
	return (0); /* this is the wrong value I'm sure */
}

#ifdef PBIO_MODULE

/* Here is the support for if we ARE a loadable kernel module */

#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/lkm.h>

MOD_DEV (pbio, LM_DT_CHAR, CDEV_MAJOR, &pbio_cdevsw);

static struct isa_device dev = {0, &pbiodriver, BASE_IO, IRQ, DMA, (caddr_t) PHYS_IO, PHYS_IO_SIZE, INT_INT, 0, FLAGS, 0, 0, 0, 0, 1, 0, 0};

static int
pbio_load (struct lkm_table *lkmtp, int cmd)
{
	if (pbioprobe (&dev)) {
		pbioattach (&dev);
		uprintf ("pbio driver loaded\n");
		uprintf ("pbio: interrupts not hooked\n");
		return 0;
	} else {
		uprintf ("pbio driver: probe failed\n");
		return 1;
	}
}

static int
pbio_unload (struct lkm_table *lkmtp, int cmd)
{
	uprintf ("pbio driver unloaded\n");
	return 0;
}

static int
pbio_stat (struct lkm_table *lkmtp, int cmd)
{
	return 0;
}

int
pbio_mod (struct lkm_table *lkmtp, int cmd, int ver)
{
	MOD_DISPATCH(pbio, lkmtp, cmd, ver,
		pbio_load, pbio_unload, pbio_stat);
}

#endif /* PBIO_MODULE */

DRIVER_MODULE(pbio, isa, pbio_driver, pbio_devclass, 0, 0);
