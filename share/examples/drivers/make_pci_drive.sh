#!/bin/sh 
# This writes a skeleton pci driver and puts it into the kernel tree for you
#arg1 is lowercase "foo" 
#
# It also creates a directory under /usr/src/sys/modules to help you create
# kernel loadable modules, though without much use except for development.
#
# files created:
#	/sys/i386/conf/files.FOO
#	/sys/i386/conf/FOO
#	/sys/pci/foo.c
#	/sys/sys/fooio.h
#	/usr/src/sys/modules/foo
#
# Trust me, RUN THIS SCRIPT :)
#
#-------cut here------------------
cd /sys/i386/conf

if [ "${1}X" = "X" ] 
then
	echo "Hey , how about some help here.. give me a device name!"
	exit 1
fi

if [ -d /usr/src/sys/modules ]
then
	mkdir /usr/src/sys/modules/${1}
fi

UPPER=`echo ${1} |tr "[:lower:]" "[:upper:]"` 
cat >files.${UPPER} <<DONE
pci/${1}.c      optional ${1} device-driver
DONE

cat >${UPPER} <<DONE
# Configuration file for kernel type: ${UPPER}
ident	${UPPER}
# \$Id: make_pci_driver.sh,v 1.0 1999/03/15 16:10:29 crb Exp $"
DONE

grep -v GENERIC < GENERIC >>${UPPER}

cat >>${UPPER} <<DONE
# trust me, you'll need this
options	DDB		
device ${1}0
DONE

cat >../../pci/${1}.c <<DONE
/*
 * Copyright ME
 *
 * ${1} driver
 * \$Id: make_pci_driver.sh,v 1.4 1998/03/15 16:10:29 crb Exp $
 */


#include "pci.h"
#if NPCI >0

#include "${1}.h"		/* generated file.. defines N${UPPER} */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/kernel.h>		/* SYSINIT stuff */
#include <sys/conf.h>		/* cdevsw stuff */
#include <sys/malloc.h>		/* malloc region definitions */
#include <machine/clock.h>	/* DELAY() */
#include <pci.h>                /* NCPI definitions */
#include <pci/pcivar.h>         /* pci variables etc. */
#include <pci/pcireg.h>         /* pci register definitions etc. */
#include <sys/${1}io.h>		/* ${1} IOCTL definitions */
#ifdef DEVFS
#include <sys/devfsext.h>	/* DEVFS defintitions */
#endif /* DEVFS */
#ifdef ${UPPER}_KLD_MODULE
#include <sys/module.h>		/* KLD module definitions */
#endif
#endif

#define ${UPPER}_DEV_PCI_ID CHANGE_ME       /* pci device id of your device */       

/* Function prototypes (these should all be static) */
static  d_open_t		${1}open;
static  d_close_t		${1}close;
static  d_read_t		${1}read;
static  d_write_t		${1}write;
static  d_ioctl_t		${1}ioctl;
static  d_mmap_t		${1}mmap;
static  d_poll_t		${1}poll;
static  const char			*${1}probe(pcici_t tag, pcidi_t type);
static  void			${1}attach(pcici_t tag, int     unit);
static  pci_inthand_t   ${1}intr;
#ifdef ${UPPER}_KLD_MODULE
static	ointhand2_t	${1}intr; /* should actually have type inthand2_t */
#endif
 
#define CDEV_MAJOR 20
static struct cdevsw ${1}_cdevsw = {
	${1}open,
	${1}close,
	${1}read,
	${1}write,        
	${1}ioctl,
	nullstop,
	nullreset,
	nodevtotty, 
	${1}poll,
	${1}mmap,
	NULL,
	"${1}",
	NULL,
	-1 };
 
static u_long ${1}count;

struct pci_device ${1}driver = {
        "${1}",
        ${1}probe,
        ${1}attach,
        &${1}count,
        NULL
};

/* The DATA_SET macro includes the pci device in the set of pci devices
 * that will be probed at config time.
 */

DATA_SET (pcidevice_set, ${1}driver);

/* 
 * device  specific Misc defines 
 */
#define BUFFERSIZE 1024
#define UNIT(dev) minor(dev)	/* assume one minor number per unit */

/*
 * One of these per allocated device
 */
struct ${1}_softc {
#ifdef DEVFS
        static void *devfs_token;
#endif
        char    buffer[BUFFERSIZE];
        pcici_t     tag;            /* PCI tag, for doing PCI commands */
	int	opens;

} ;


typedef	struct ${1}_softc *sc_p;

static sc_p sca[N${UPPER}];
static int ${1}_in_use;

/* this function should discriminate if this device is
 * or is not handled by this driver, often this will be
 * as simple as testing the pci id of the device
 */

static const char *
${1}probe(pcici_t tag, pcidi_t type)
{
        switch (type) {
        case ${UPPER}_DEV_PCI_ID:        
                return("${1}");
        };
        return ((char *)0);
}

/*
 * Called if the probe succeeded.
 */
static void
${1}attach(pcici_t tag, int unit)
{
        sc_p scp  = sca[unit];
        
        /* 
         * Allocate storage for this instance .
         */
        scp = malloc(sizeof(*scp), M_DEVBUF, M_NOWAIT);
        if( scp == NULL) {
                printf("${1}%d failed to allocage driver strorage\n", unit);
                return;
        }
        bzero(scp, sizeof(*scp));
        sca[unit] = scp;

        /*
         * Store whatever seems wise.
         */
        scp->tag = tag;
#if DEVFS
        scp->devfs_token = devfs_add_devswf(&${1}_cdevsw, unit, DV_CHR,
            UID_ROOT, GID_KMEM, 0600, "${1}%d", unit);
#endif
        return;
}

/* 
 * Macro to check that the unit number is valid
 * Often this isn't needed as once the open() is performed,
 * the unit number is pretty much safe.. The exception would be if we
 * implemented devices that could "go away". in which case all these routines
 * would be wise to check the number, DIAGNOSTIC or not.
 */
#define CHECKUNIT(RETVAL)					\
do { /* the do-while is a safe way to do this grouping */	\
	if (unit > N${UPPER}) {					\
		printf(__FUNCTION__ ":bad unit %d\n", unit);	\
		return (RETVAL);				\
	}							\
	if (scp == NULL) { 					\
		printf( __FUNCTION__ ": unit %d not attached\n", unit);\
		return (RETVAL);				\
	}							\
} while (0)						
#ifdef	DIAGNOSTIC
#define	CHECKUNIT_DIAG(RETVAL) CHECKUNIT(RETVAL)
#else	/* DIAGNOSTIC */
#define	CHECKUNIT_DIAG(RETVAL)
#endif 	/* DIAGNOSTIC */

static void
${1}intr(void *arg)

{
        
        /* 
         * well we got an interupt, now what?
         * Theoretically we don't need to check the unit.
         */
        return;
}

int ${1}ioctl (dev_t dev, u_long cmd, caddr_t data, int fflag, struct proc *p)
{
	int unit = UNIT (dev);
	sc_p scp  = sca[unit];
	
	CHECKUNIT_DIAG(ENXIO);
    
	switch (cmd) {
	    case DHIOCRESET:
		/*  whatever resets it */
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
static  int
${1}open(dev_t dev, int oflags, int devtype, struct proc *p)
{
	int unit = UNIT (dev);
	sc_p scp  = sca[unit];
	
	CHECKUNIT(ENXIO);

	/* 
	 * Do processing
	 */
	if (scp->opens == 0)
		${1}_in_use++;
	scp->opens++;
	return (0);
}

/* close.. only called on the LAST close */
static  int
${1}close(dev_t dev, int fflag, int devtype, struct proc *p)
{
	int unit = UNIT (dev);
	sc_p scp  = sca[unit];
	
	CHECKUNIT_DIAG(ENXIO);

	/* 
	 * Do processing
	 */
	scp->opens = 0;
	${1}_in_use--;
	return (0);
}

static  int
${1}read(dev_t dev, struct uio *uio, int ioflag)
{
	int unit = UNIT (dev);
	sc_p scp  = sca[unit];
	int     toread;
	
	
	CHECKUNIT_DIAG(ENXIO);

	/* 
	 * Do processing
	 * read from buffer
	 */
	toread = (min(uio->uio_resid, sizeof(scp->buffer)));
	return(uiomove(scp->buffer, toread, uio));
}

static  int
${1}write(dev_t dev, struct uio *uio, int ioflag)
{
	int unit = UNIT (dev);
	sc_p scp  = sca[unit];
	int	towrite;
	
	CHECKUNIT_DIAG(ENXIO);

	/* 
	 * Do processing
	 * write to buffer
	 */
	towrite = (min(uio->uio_resid, sizeof(scp->buffer)));
	return(uiomove(scp->buffer, towrite, uio));
}

static  int
${1}mmap(dev_t dev, vm_offset_t offset, int nprot)
{
	int unit = UNIT (dev);
	sc_p scp  = sca[unit];
	
	CHECKUNIT_DIAG(-1);

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

static  int
${1}poll(dev_t dev, int which, struct proc *p)
{
	int unit = UNIT (dev);
	sc_p scp  = sca[unit];
	
	CHECKUNIT_DIAG(ENXIO);

	/* 
	 * Do processing
	 */
	return (0); /* this is the wrong value I'm sure */
}

#ifndef ${UPPER}_KLD_MODULE

/*
 * Now  for some driver initialisation.
 * Occurs ONCE during boot (very early).
 * This is if we are NOT a loadable module.
 */
static void             
${1}_drvinit(void *unused)
{
        dev_t dev;

	dev = makedev(CDEV_MAJOR, 0);
	cdevsw_add(&dev, &${1}_cdevsw, NULL);
}

SYSINIT(${1}dev, SI_SUB_DRIVERS, SI_ORDER_MIDDLE+CDEV_MAJOR,
		${1}_drvinit, NULL)

#else  /* ${UPPER}_KLD_MODULE */

static int
${1}_modevent(module_t mod, int type, void *unused)
{
        
        switch (type) {
        case MOD_LOAD:
                break;
        case MOD_UNLOAD:
		if (${1}_in_use)
			return (EBUSY);
                break;
        case MOD_SHUTDOWN:
                break;
        default:
                break;
        }
        return 0;
}

static moduledata_t ${1}mod = {
        "${1}",
        ${1}_modevent,
        NULL
};

DECLARE_MODULE(${1}, ${1}mod, SI_SUB_PSEUDO, SI_ORDER_ANY);

#endif /* ${UPPER}_MODULE */

DONE

cat >../../sys/${1}io.h <<DONE
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
#define DHIOCRESET _IO('D', 0)   /* reset the ${1} device */
#endif
DONE

if [ -d /usr/src/sys/modules/${1} ]
then
	cat >/usr/src/sys/modules/${1}/Makefile <<DONE
#       $Id: Makefile,v 1.0 1999/03/15 04:30:46 crb Exp $
#	${UPPER} KLD Module
#
#	This happens not to work, pci devices can't be modules
#	unless you preload them at kernel load time

.PATH:	\${.CURDIR}/../..pci/
KMOD	= ${1}
SRCS	= ${1}.c ${1}.h

CFLAGS		+= -I. -D${UPPER}_KLD_MODULE
CLEANFILES	+= ${1}.h

.include <bsd.kmod.mk>
DONE
fi

config -rg ${UPPER}
cd ../../compile/${UPPER}
make depend
make ${1}.o
#make
exit

#--------------end of script---------------
#
#edit to your taste..
#
# 





