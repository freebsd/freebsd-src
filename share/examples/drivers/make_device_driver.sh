#!/bin/sh 
# This writes a skeleton driver and puts it into the kernel tree for you
#arg1 is lowercase "foo" 
#
# It also creates a directory under /usr/src/lkm to help you create
#loadable kernel modules, though without much use except for development.
#
# Trust me, RUN THIS SCRIPT :)
# $FreeBSD$"
#
#-------cut here------------------
if [ "${1}X" = "X" ] 
then
	echo "Hey , how about some help here.. give me a device name!"
	exit 1
fi
UPPER=`echo ${1} |tr "[:lower:]" "[:upper:]"` 

HERE=`pwd`
cd /sys
TOP=`pwd`

echo ${TOP}/modules/${1}
echo ${TOP}/i386/conf/files.${UPPER}
echo ${TOP}/i386/conf/${UPPER}
echo ${TOP}/dev/${1}
echo ${TOP}/dev/${1}/${1}.c
echo ${TOP}/sys/${1}io.h
echo ${TOP}/modules/${1}
echo ${TOP}/modules/${1}/Makefile

rm -rf ${TOP}/dev/${1}
rm -rf ${TOP}/modules/${1}
rm ${TOP}/i386/conf/files.${UPPER}
rm ${TOP}/i386/conf/${UPPER}
rm ${TOP}/sys/${1}io.h

if [ -d ${TOP}/modules/${1} ]
then
	echo "There appears to already be a module called ${1}"
	exit 1
else
	mkdir ${TOP}/modules/${1}
fi

#######################################################################
#######################################################################
#
# Create configuration information needed to create a kernel
# containing this driver.
#
# Not really needed if we are going to do this as a module.
#######################################################################
# First add the file to a local file list.
#######################################################################

cat >${TOP}/i386/conf/files.${UPPER} <<DONE
i386/isa/${1}.c	 optional ${1} device-driver
DONE

#######################################################################
# Then create a configuration file for a kernel that contains this driver.
#######################################################################
cat >${TOP}/i386/conf/${UPPER} <<DONE
# Configuration file for kernel type: ${UPPER}
ident	${UPPER}
# \$FreeBSD$"
DONE

grep -v GENERIC < /sys/i386/conf/GENERIC >>${TOP}/i386/conf/${UPPER}

cat >>${TOP}/i386/conf/${UPPER} <<DONE
options	DDB		# trust me, you'll need this
device ${1} at isa?
DONE

if [ ! -d ${TOP}/dev/${1} ]
then
	mkdir -p ${TOP}/dev/${1}
fi


















cat >${TOP}/dev/${1}/${1}.c <<DONE
/*
 * Copyright ME
 *
 * ${1} driver
 * \$FreeBSD$
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>		/* cdevsw stuff */
#include <sys/kernel.h>		/* SYSINIT stuff */
#include <sys/uio.h>		/* SYSINIT stuff */
#include <sys/malloc.h>		/* malloc region definitions */
#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
#include <sys/time.h>

#include <isa/isavar.h>
#include "isa_if.h"
#include <sys/${1}io.h>		/* ${1} IOCTL definitions */

#define ${UPPER}DEV2SOFTC(dev) ((dev)->si_drv1)
#define ${UPPER}_INB(port) bus_space_read_1( bt, bh, (port))
#define ${UPPER}_OUTB(port, val) bus_space_write_1( bt, bh, (port), (val))
#define SOME_PORT 123
#define EXPECTED_VALUE 0x42


/* Function prototypes (these should all be static) */
static int ${1}_isa_probe (device_t);
static int ${1}_isa_attach (device_t);
static int ${1}_isa_detach (device_t);

static d_open_t		${1}open;
static d_close_t	${1}close;
static d_read_t		${1}read;
static d_write_t	${1}write;
static d_ioctl_t	${1}ioctl;
static d_mmap_t		${1}mmap;
static d_poll_t		${1}poll;
#ifdef ${UPPER}_MODULE
static	ointhand2_t	${1}intr; /* should actually have type inthand2_t */
#endif
 
#define CDEV_MAJOR 20
static struct cdevsw ${1}_cdevsw = {
	/* open */	${1}open,
	/* close */	${1}close,
	/* read */	${1}read,
	/* write */	${1}write,
	/* ioctl */	${1}ioctl,
	/* poll */	${1}poll,
	/* mmap */	${1}mmap,
	/* strategy */	nostrategy,	/* not a block type device */
	/* name */	"${1}",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,		/* not a block type device */
	/* psize */	nopsize,	/* not a block type device */
	/* flags */	0,
	/* bmaj */	-1
};
 
/* 
 * device specific Misc defines 
 */
#define BUFFERSIZE 1024
#define NUMPORTS 4

/*
 * One of these per allocated device
 */
struct ${1}_softc {
	bus_space_tag_t bt;
	bus_space_handle_t bh;
	int port_rid;
	struct resource* port_res;	/* resource for port range */
	dev_t dev;
	device_t device;
	char	buffer[BUFFERSIZE];
} ;

typedef	struct ${1}_softc *sc_p;

devclass_t ${1}_devclass;

static struct isa_pnp_id ${1}_ids[] = {
	{0x12345678, "ABCco Widget"},
	{0xfedcba98, "shining moon Widget ripoff"},
	{0}
};

static device_method_t ${1}_methods[] = {
	DEVMETHOD(device_probe,		${1}_isa_probe),
	DEVMETHOD(device_attach,	${1}_isa_attach),
	DEVMETHOD(device_detach,	${1}_isa_detach),
	{ 0, 0 }
};

static driver_t ${1}_isa_driver = {
	"${1}",
	${1}_methods,
	sizeof (struct ${1}_softc)
};

DRIVER_MODULE(${1}, isa, ${1}_isa_driver, ${1}_devclass, 0, 0);


/*
 * The ISA code calls this for each device it knows about,
 * whether via the PNP code or via the hints etc.
 */
static int
${1}_isa_probe (device_t device)
{
	int error;
	sc_p scp = device_get_softc(device);
	bus_space_handle_t  bh;
	bus_space_tag_t bt;
	struct resource *port_res;
	int rid = 0;
	int	size = 16; /* SIZE of port range used */


	bzero(scp, sizeof(*scp));
	scp->device = device;

	/*
	 * Check for a PNP match..
	 * There are several possible outcomes.
	 * error == 0		We match a PNP device (possibly several?).
	 * error == ENXIO,	It is a PNP device but not ours.
	 * error == ENOENT,	I is not a PNP device.. try heuristic probes.
	 *    -- logic from if_ed_isa.c, added info from isa/isa_if.m:
	 */
	error = ISA_PNP_PROBE(device_get_parent(device), device, ${1}_ids);
	switch (error) {
	case 0:
		/*
		 * We found a PNP device.
		 * Fall through into the code that just looks
		 * for a non PNP device as that should 
		 * act as a good filter for bad stuff.
		 */
	case ENOENT:
		/*
		 * Well it didn't show up in the PNP tables
		 * so look directly at known ports (if we have any)
		 * in case we are looking for an old pre-PNP card.
		 *
		 * The ports etc should come from a 'hints' section
		 * buried somewhere. XXX - still not figured out.
		 * which is read in by code in isa/isahint.c
		 */
  
        	port_res = bus_alloc_resource(device, SYS_RES_IOPORT, &rid,
                                 0ul, ~0ul, size, RF_ACTIVE);
        	if (port_res == NULL) {
			error = ENXIO;
			break;
        	}

                scp->port_rid = rid;
                scp->port_res = port_res;
		scp->bt = bt = rman_get_bustag(port_res);
		scp->bh = bh = rman_get_bushandle(port_res);

		if ( ${UPPER}_INB(SOME_PORT) != EXPECTED_VALUE) {
			/* 
			 * It isn't what we expected,
			 * so release everything and quit looking for it.
			 */
			bus_release_resource(device, SYS_RES_IOPORT,
							rid, port_res);
			return (ENXIO);
		}
		error = 0;
		break;
	case  ENXIO:
		/* not ours, leave imediatly */
	default:
		error = ENXIO;
	}
	return (error);
}

/*
 * Called if the probe succeeded.
 * We can be destructive here as we know we have the device.
 */
static int
${1}_isa_attach (device_t device)
{
	int	unit = device_get_unit(device);
	sc_p scp = device_get_softc(device);

	scp->dev = make_dev(&${1}_cdevsw, 0, 0, 0, 0600, "${1}%d", unit);
	scp->dev->si_drv1 = scp;
	return 0;
}

static int
${1}_isa_detach (device_t device)
{
	sc_p scp = device_get_softc(device);

	bus_release_resource(device, SYS_RES_IOPORT,
					scp->port_rid, scp->port_res);
	destroy_dev(scp->dev);
	return (0);
}

static void
${1}intr(void *arg)
{
	/*device_t dev = (device_t)arg;*/
	/* sc_p scp	= device_get_softc(dev);*/

	/* 
	 * well we got an interupt, now what?
	 */
	return;
}

static int
${1}ioctl (dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	sc_p scp	= ${UPPER}DEV2SOFTC(dev);
	bus_space_handle_t  bh = scp->bh;
	bus_space_tag_t bt = scp->bt;

	switch (cmd) {
	case DHIOCRESET:
		/* whatever resets it */
		${UPPER}_OUTB(SOME_PORT, 0xff) ;
		break;
	default:
		return ENXIO;
	}
	return (0);
}
/*
 * You also need read, write, open, close routines.
 * This should get you started
 */
static int
${1}open(dev_t dev, int oflags, int devtype, struct proc *p)
{
	sc_p scp	= ${UPPER}DEV2SOFTC(dev);

	/* 
	 * Do processing
	 */
	return (0);
}

static int
${1}close(dev_t dev, int fflag, int devtype, struct proc *p)
{
	sc_p scp	= ${UPPER}DEV2SOFTC(dev);

	/* 
	 * Do processing
	 */
	return (0);
}

static int
${1}read(dev_t dev, struct uio *uio, int ioflag)
{
	sc_p scp	= ${UPPER}DEV2SOFTC(dev);
	int	 toread;


	/* 
	 * Do processing
	 * read from buffer
	 */
	toread = (min(uio->uio_resid, sizeof(scp->buffer)));
	return(uiomove(scp->buffer, toread, uio));
}

static int
${1}write(dev_t dev, struct uio *uio, int ioflag)
{
	sc_p scp	= ${UPPER}DEV2SOFTC(dev);
	int	towrite;

	/* 
	 * Do processing
	 * write to buffer
	 */
	towrite = (min(uio->uio_resid, sizeof(scp->buffer)));
	return(uiomove(scp->buffer, towrite, uio));
}

static int
${1}mmap(dev_t dev, vm_offset_t offset, int nprot)
{
	sc_p scp	= ${UPPER}DEV2SOFTC(dev);

	/* 
	 * Do processing
	 */
#if 0	/* if we had a frame buffer or whatever.. do this */
	if (offset > FRAMEBUFFERSIZE - PAGE_SIZE) {
		return (-1);
	}
	return i386_btop((FRAMEBASE + offset));
#else
	return (-1);
#endif
}

static int
${1}poll(dev_t dev, int which, struct proc *p)
{
	sc_p scp	= ${UPPER}DEV2SOFTC(dev);

	/* 
	 * Do processing
	 */
	return (0); /* this is the wrong value I'm sure */
}

DONE

cat >${TOP}/sys/${1}io.h <<DONE
/*
 * Definitions needed to access the ${1} device (ioctls etc)
 * see mtio.h , ioctl.h as examples
 */
#ifndef SYS_DHIO_H
#define SYS_DHIO_H

#ifndef KERNEL
#include <sys/types.h>
#endif
#include <sys/ioccom.h>

/*
 * define an ioctl here
 */
#define DHIOCRESET _IO('D', 0) /* reset the ${1} device */
#endif
DONE

if [ ! -d ${TOP}/modules/${1} ]
then
	mkdir -p ${TOP}/modules/${1}
fi

cat >${TOP}/modules/${1}/Makefile <<DONE
#	${UPPER} Loadable Kernel Module
#
# $FreeBSD$
 
.PATH:  \${.CURDIR}/../../dev/${1}
KMOD    = ${1}
SRCS    = ${1}.c
SRCS    += opt_inet.h device_if.h bus_if.h pci_if.h isa_if.h
  
# you may need to do this is your device is an if_xxx driver
opt_inet.h:
	echo "#define INET 1" > opt_inet.h
	   
.include <bsd.kmod.mk>
DONE

(cd ${TOP}/modules/${1}; make depend; make )
exit

config ${UPPER}
cd ../../compile/${UPPER}
make depend
make ${1}.o
make
exit

#--------------end of script---------------
#
#edit to your taste..
#
# 




