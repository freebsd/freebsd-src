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
dev/${1}/${1}.c	 optional ${1}
DONE

#######################################################################
# Then create a configuration file for a kernel that contains this driver.
#######################################################################
cat >${TOP}/i386/conf/${UPPER} <<DONE
# Configuration file for kernel type: ${UPPER}
ident	${UPPER}
# \$Free\0x42SD: src/share/examples/drivers/make_device_driver.sh,v 1.8 2000/10/25 15:08:11 julian Exp $"
DONE

grep -v GENERIC < /sys/i386/conf/GENERIC >>${TOP}/i386/conf/${UPPER}

cat >>${TOP}/i386/conf/${UPPER} <<DONE
options		DDB		# trust me, you'll need this
device		${1}
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
static int ${1}_deallocate_resources(device_t device);
static int ${1}_allocate_resources(device_t device);

static d_open_t		${1}open;
static d_close_t	${1}close;
static d_read_t		${1}read;
static d_write_t	${1}write;
static d_ioctl_t	${1}ioctl;
static d_mmap_t		${1}mmap;
static d_poll_t		${1}poll;
static	void		${1}intr(void *arg);
 
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
#define MEMSIZE	1024*1024 /* imaginable h/w buffer size */

/*
 * One of these per allocated device
 */
struct ${1}_softc {
	bus_space_tag_t bt;
	bus_space_handle_t bh;
	int rid_ioport;
	int rid_memory;
	int rid_irq;
	int rid_drq;
	struct resource* res_ioport;	/* resource for port range */
	struct resource* res_memory;	/* resource for mem range */
	struct resource* res_irq;	/* resource for irq range */
	struct resource* res_drq;	/* resource for dma channel */
	device_t device;
	dev_t dev;
	void	*intr_cookie;
	char	buffer[BUFFERSIZE];
} ;

typedef	struct ${1}_softc *sc_p;

static devclass_t ${1}_devclass;

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
		 * Do nothing, as it's all done in attach()
		 */
		break;
	case ENOENT:
		/*
		 * Well it didn't show up in the PNP tables
		 * so look directly at known ports (if we have any)
		 * in case we are looking for an old pre-PNP card.
		 *
		 * I think the ports etc should come from a 'hints' section
		 * buried somewhere. XXX - still not figured out.
		 * which is read in by code in isa/isahint.c
		 */
#if 0 /* till I work out how to find ht eport from the hints */
		if ( ${UPPER}_INB(SOME_PORT) != EXPECTED_VALUE) {
			/* 
			 * It isn't what we expected, so quit looking for it.
			 */
			error = ENXIO;
		} else {
			/*
			 * We found one..
			 */
			error = 0;
		}
#endif
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
	int	unit	= device_get_unit(device);
	sc_p	scp	= device_get_softc(device);
	device_t parent	= device_get_parent(device);
	bus_space_handle_t  bh;
	bus_space_tag_t bt;

	scp->dev->si_drv1 = scp;
	scp->dev = make_dev(&${1}_cdevsw, 0,
			UID_ROOT, GID_OPERATOR, 0600, "${1}%d", unit);

	if (${1}_allocate_resources(device)) {
		goto errexit;
	}

	scp->bt = bt = rman_get_bustag(scp->res_ioport);
	scp->bh = bh = rman_get_bushandle(scp->res_ioport);

	/* register the interrupt handler */
	if (scp->res_irq) {
		/* default to the tty mask for registration */  /* XXX */
		if (BUS_SETUP_INTR(parent, device, scp->res_irq, INTR_TYPE_TTY,
				${1}intr, scp, &scp->intr_cookie) == 0) {
			/* do something if successfull */
		}
	}
	return 0;

errexit:
	/*
	 * Undo anything we may have done
	 */
	${1}_isa_detach(device);
	return (ENXIO);
}

static int
${1}_isa_detach (device_t device)
{
	sc_p scp = device_get_softc(device);
	device_t parent = device_get_parent(device);

	/*
	 * At this point stick a strong piece of wood into the device
	 * to make sure it is stopped safely. The alternative is to 
	 * simply REFUSE to detach if it's busy. What you do depends on 
	 * your specific situation.
	 */
	/* ZAP some register */

	/*
	 * Take our interrupt handler out of the list of handlers
	 * that can handle this irq.
	 */
	if (scp->intr_cookie != NULL) {
		if (BUS_TEARDOWN_INTR(parent, device,
			scp->res_irq, scp->intr_cookie) != 0) {
				printf("intr teardown failed.. continuing\n");
		}
		scp->intr_cookie = NULL;
	}

	/*
	 * deallocate any system resources we may have
	 * allocated on behalf of this driver.
	 */
	return ${1}_deallocate_resources(device);
}

static int
${1}_allocate_resources(device_t device)
{
	int error;
	sc_p scp = device_get_softc(device);
	int	size = 16; /* SIZE of port range used */

	scp->res_ioport = bus_alloc_resource(device, SYS_RES_IOPORT,
			&scp->rid_ioport, 0ul, ~0ul, size, RF_ACTIVE);
	if (scp->res_ioport == NULL) {
		goto errexit;
	}

	scp->res_irq = bus_alloc_resource(device, SYS_RES_IRQ,
			&scp->rid_irq, 0ul, ~0ul, 1, RF_SHAREABLE);
	if (scp->res_irq == NULL) {
		goto errexit;
	}

	scp->res_drq = bus_alloc_resource(device, SYS_RES_DRQ,
			&scp->rid_drq, 0ul, ~0ul, 1, RF_ACTIVE);
	if (scp->res_drq == NULL) {
		goto errexit;
	}

	scp->res_memory = bus_alloc_resource(device, SYS_RES_IOPORT,
			&scp->rid_memory, 0ul, ~0ul, MSIZE, RF_ACTIVE);
	if (scp->res_memory == NULL) {
		goto errexit;
	}

	return (0);

errexit:
	error = ENXIO;
	/* cleanup anything we may have assigned. */
	${1}_deallocate_resources(device);
	return (ENXIO); /* for want of a better idea */
}

static int
${1}_deallocate_resources(device_t device)
{
	sc_p scp = device_get_softc(device);

	if (scp->res_irq != 0) {
		bus_deactivate_resource(device, SYS_RES_IRQ,
			scp->rid_irq, scp->res_irq);
		bus_release_resource(device, SYS_RES_IRQ,
			scp->rid_irq, scp->res_irq);
		scp->res_irq = 0;
	}
	if (scp->res_ioport != 0) {
		bus_deactivate_resource(device, SYS_RES_IOPORT,
			scp->rid_ioport, scp->res_ioport);
		bus_release_resource(device, SYS_RES_IOPORT,
			scp->rid_ioport, scp->res_ioport);
		scp->res_ioport = 0;
	}
	if (scp->res_ioport != 0) {
		bus_deactivate_resource(device, SYS_RES_MEMORY,
			scp->rid_memory, scp->res_memory);
		bus_release_resource(device, SYS_RES_MEMORY,
			scp->rid_memory, scp->res_memory);
		scp->res_ioport = 0;
	}
	if (scp->res_drq != 0) {
		bus_deactivate_resource(device, SYS_RES_DRQ,
			scp->rid_drq, scp->res_drq);
		bus_release_resource(device, SYS_RES_DRQ,
			scp->rid_drq, scp->res_drq);
		scp->res_drq = 0;
	}
	if (scp->dev) {
		destroy_dev(scp->dev);
	}
	return (0);
}

static void
${1}intr(void *arg)
{
	sc_p scp	= arg;

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




