#!/bin/sh 
# This writes a skeleton driver and puts it into the kernel tree for you
#arg1 is lowercase "foo" 
#
# It also creates a directory under /usr/src/lkm to help you create
#loadable kernel modules, though without much use except for development.
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

if [ -d /usr/src/lkm ]
then
	mkdir /usr/src/lkm/${1}
fi

UPPER=`echo ${1} |tr "[:lower:]" "[:upper:]"` 
cat >files.${UPPER} <<DONE
i386/isa/${1}.c      optional ${1} device-driver
DONE

cat >${UPPER} <<DONE
# Configuration file for kernel type: ${UPPER}
ident	${UPPER}
# \$FreeBSD$"
DONE

grep -v GENERIC < GENERIC >>${UPPER}

cat >>${UPPER} <<DONE
# trust me, you'll need this
options	DDB		
device ${1}0 at isa? port 0x234 bio irq 5
DONE

cat >../isa/${1}.c <<DONE
/*
 * Copyright ME
 *
 * ${1} driver
 * \$FreeBSD$
 */


#include "${1}.h"		/* generated file.. defines N${UPPER} */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>		/* SYSINIT stuff */
#include <sys/conf.h>		/* cdevsw stuff */
#include <sys/malloc.h>		/* malloc region definitions */
#include <machine/clock.h>	/* DELAY() */
#include <i386/isa/isa.h>	/* ISA bus port definitions etc. */
#include <i386/isa/isa_device.h>/* ISA bus configuration structures */
#include <sys/${1}io.h>		/* ${1} IOCTL definitions */



/* Function prototypes (these should all be static) */
static  d_open_t	${1}open;
static  d_close_t	${1}close;
static  d_read_t	${1}read;
static  d_write_t	${1}write;
static  d_ioctl_t	${1}ioctl;
static  d_mmap_t	${1}mmap;
static  d_poll_t	${1}poll;
static	int		${1}probe (struct isa_device *);
static	int		${1}attach (struct isa_device *);
#ifdef ${UPPER}_MODULE
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
 
struct isa_driver ${1}driver = {
	${1}probe,
	${1}attach,
	"${1}" };

/* 
 * device  specific Misc defines 
 */
#define BUFFERSIZE 1024
#define NUMPORTS 4
#define UNIT(dev) minor(dev)	/* assume one minor number per unit */

/*
 * One of these per allocated device
 */
struct ${1}_softc {
	struct isa_device *dev;
	char	buffer[BUFFERSIZE];
} ;

typedef	struct ${1}_softc *sc_p;

static sc_p sca[N${UPPER}];

/* add your own test to see if it exists */
/* should return the number of ports needed */
static int
${1}probe (struct isa_device *dev)
{
	char val;
	int unit = dev->id_unit;
	sc_p scp  = sca[unit];

	/*
	 * Check the unit makes sense.
	 */
	if (unit > N${UPPER}) {
		printf("bad unit (%d)\n", unit);
		return (0);
	}
	if (scp) {
		printf("unit %d already attached\n", unit);
		return (0);
	}

	/*
	 * try see if the device is there.
	 */
	val = inb (dev->id_iobase);
	if ( val != 42 ) {
		return (0);
	}

	/*
	 * ok, we got one we think 
	 * do some further (this time possibly destructive) tests.
	 */
	outb (dev->id_iobase, 0xff);
	DELAY (10000); /*  10 ms delay */
	val = inb (dev->id_iobase) & 0x0f;
	return ((val & 0x0f) == 0x0f)? NUMPORTS : 0 ;
}

/*
 * Called if the probe succeeded.
 * We can be destructive here as we know we have the device.
 * we can also trust the unit number.
 */
static int
${1}attach (struct isa_device *dev)
{
	int unit = dev->id_unit;
	sc_p scp  = sca[unit];

	/*
	 * Attach our interrupt handler to the device struct.  Our caller
	 * will attach it to the hardware soon after we return.
	 */
	dev->id_ointr = ${1}intr;

	/* 
	 * Allocate storage for this instance .
	 */
	scp = malloc(sizeof(*scp), M_DEVBUF, M_NOWAIT);
	if( scp == NULL) {
		printf("${1}%d failed to allocage driver strorage\n", unit);
		return (0);
	}
	bzero(scp, sizeof(*scp));
	sca[unit] = scp;

	/*
	 * Store whatever seems wise.
	 */
	scp->dev = dev;
	return 1;
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
${1}intr(int unit)
{
	sc_p scp  = sca[unit];
	
	/* 
	 * well we got an interupt, now what?
	 * Theoretically we don't need to check the unit.
	 */
	return;
}

int ${1}ioctl (dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{
	int unit = UNIT (dev);
	sc_p scp  = sca[unit];
	
	CHECKUNIT_DIAG(ENXIO);
    
	switch (cmd) {
	    case DHIOCRESET:
		/*  whatever resets it */
		outb(scp->dev->id_iobase, 0xff);
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
	return (0);
}

static  int
${1}close(dev_t dev, int fflag, int devtype, struct proc *p)
{
	int unit = UNIT (dev);
	sc_p scp  = sca[unit];
	
	CHECKUNIT_DIAG(ENXIO);

	/* 
	 * Do processing
	 */
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
${1}mmap(dev_t dev, int offset, int nprot)
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

#ifndef ${UPPER}_MODULE

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

#else  /* ${UPPER}_MODULE */
/* Here is the support for if we ARE a loadable kernel module */

#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/lkm.h>

MOD_DEV (${1}, LM_DT_CHAR, CDEV_MAJOR, &${1}_cdevsw);

static struct isa_device dev = {0, &${1}driver, BASE_IO, IRQ, DMA, (caddr_t) PHYS_IO, PHYS_IO_SIZE, INT_INT, 0, FLAGS, 0, 0, 0, 0, 1, 0, 0};

static int
${1}_load (struct lkm_table *lkmtp, int cmd)
{
	if (${1}probe (&dev)) {
		${1}attach (&dev);
		uprintf ("${1} driver loaded\n");
		uprintf ("${1}: interrupts not hooked\n");
		return 0;
	} else {
		uprintf ("${1} driver: probe failed\n");
		return 1;
	}
}

static int
${1}_unload (struct lkm_table *lkmtp, int cmd)
{
	uprintf ("${1} driver unloaded\n");
	return 0;
}

static int
${1}_stat (struct lkm_table *lkmtp, int cmd)
{
	return 0;
}

int
${1}_mod (struct lkm_table *lkmtp, int cmd, int ver)
{
	MOD_DISPATCH(${1}, lkmtp, cmd, ver,
		${1}_load, ${1}_unload, ${1}_stat);
}

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

if [ -d /usr/src/lkm/${1} ]
then
	cat >/usr/src/lkm/${1}/Makefile <<DONE
#	${UPPER} Loadable Kernel Module
#
#	This happens not to work, actually. It's written for
#	a character ISA device driver, but they cannot be
#	be made into lkm's, because you have to hard code
#	everything you'll otherwise enter into the kernel
#	configuration file.

.PATH:	\${.CURDIR}/../../sys/i386/isa
KMOD	= ${1}_mod
SRCS	= ${1}.c ${1}.h

CFLAGS		+= -I. -D${UPPER}_MODULE
CLEANFILES	+= ${1}.h

BASE_IO=0		# Base IO address
IRQ=0			# IRQ number
DMA=-1			# DMA channel
PHYS_IO=0		# Physical IO Memory base address
PHYS_IO_SIZE=0		# Physical IO Memory size
INT_INT=0		# Interrupt interface
FLAGS=0			# Flags

CFLAGS+= -DBASE_IO=\${BASE_IO} -DIRQ=\${IRQ} -DDMA=\${DMA} -DPHYS_IO=\${PHYS_IO} -DPHYS_IO_SIZE=\${PHYS_IO_SIZE} -DINT_INT=\${INT_INT} -DFLAGS=\${FLAGS}

${1}.h:
	echo "#define N${UPPER} 1" > ${1}.h

afterinstall:
	\${INSTALL} -c -o \${BINOWN} -g \${BINGRP} -m \${BINMODE} \
	\${.CURDIR}/${1} \${DESTDIR}/usr/bin

.include <bsd.kmod.mk>
DONE
fi

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




