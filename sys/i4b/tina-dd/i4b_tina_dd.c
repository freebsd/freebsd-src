/*
 *   Copyright (c) 1994, 1998 Hellmuth Michaelis. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *   SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	i4b_tina_dd.c - i4b Stollman Tina-dd control device driver
 *	----------------------------------------------------------
 *
 * $FreeBSD: src/sys/i4b/tina-dd/i4b_tina_dd.c,v 1.5 1999/09/25 18:24:19 phk Exp $
 *
 *	last edit-date: [Sat Dec  5 18:41:38 1998]
 *
 *---------------------------------------------------------------------------*/

#include "tina.h"

#if NTINA > 0

#include <sys/param.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <sys/ioccom.h>
#else
#include <sys/ioctl.h>
#endif
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <net/if.h>

#ifdef __FreeBSD__
#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#include <i386/isa/isa_device.h>
#else
#include <machine/bus.h>
#include <sys/device.h>
#include <i4b/i4b_debug.h>
#include <i4b/i4b_ioctl.h>
#endif

#include <i4b/include/i4b_mbuf.h>
#include <i4b/tina-dd/i4b_tina_ioctl.h>

static int openflag = 0;

int tinaprobe(struct isa_device *dev);
int tinaattach(struct isa_device *dev);
void tinaintr(int unit);

struct isa_driver tinadriver = {
	tinaprobe,
	tinaattach,
	"tina",
	0
};

static struct tina_softc {
	int sc_unit;
	int sc_iobase;
} tina_sc[NTINA];

static	d_open_t	tinaopen;
static	d_close_t	tinaclose;
static	d_ioctl_t	tinaioctl;
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
static d_poll_t		tinapoll;
#define POLLFIELD	tinapoll
#else
#define POLLFIELD	noselect
#endif

#define CDEV_MAJOR 54
static struct cdevsw tina_cdevsw = {
	/* open */	tinaopen,
	/* close */	tinaclose,
	/* read */	noread,
	/* write */	nowrite,
	/* ioctl */	tinaioctl,
	/* poll */	POLLFIELD,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"tina",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
	/* bmaj */	-1
};

static void setupaddr(unsigned short iobase, unsigned int addr);
static void readblock(unsigned short iobase, unsigned int addr,
	  unsigned char *dst, unsigned int len);
static void writeblock(unsigned short iobase, unsigned char *src,
	  unsigned long addr, int len);

/*---------------------------------------------------------------------------*
 *	tina - device driver probe routine
 *---------------------------------------------------------------------------*/
int
tinaprobe(struct isa_device *dev)
{
	u_char byte;
	
#define SETLOW	0x55
#define SETMID	0xaa
#define SETHIGH 0x06

	outb((dev->id_iobase + ADDR_CNTL), SETLOW);

	if((byte = inb(dev->id_iobase + ADDR_CNTL)) != SETLOW)
	{
		printf("tina%d: probe low failed, 0x%x != 0x%x\n",
			dev->id_unit, byte, SETLOW);		
		return(0);
	}
	
	outb((dev->id_iobase + ADDR_CNTM), SETMID);
	if((byte = inb(dev->id_iobase + ADDR_CNTM)) != SETMID)
	{
		printf("tina%d: probe mid failed, 0x%x != 0x%x\n",
			dev->id_unit, byte, SETMID);		
		return(0);
	}
	
	outb((dev->id_iobase + ADDR_CNTH), SETHIGH);
	if(((byte = inb(dev->id_iobase + ADDR_CNTH)) & 0x0f) != SETHIGH)
	{
		printf("tina%d: probe high failed, 0x%x != 0x%x\n",
			dev->id_unit, byte, SETHIGH);		
		return(0);
	}

	printf("tina%d: status register = 0x%x\n",
			dev->id_unit, inb(dev->id_iobase + CTRL_STAT));
	
	return(1);			/* board found */
}
#undef SETLOW
#undef SETMID
#undef SETHIGH

/*---------------------------------------------------------------------------*
 *	tina - device driver attach routine
 *---------------------------------------------------------------------------*/
int
tinaattach(struct isa_device *dev)
{
	struct tina_softc *sc = &tina_sc[dev->id_unit];

	sc->sc_unit = dev->id_unit;
	sc->sc_iobase = dev->id_iobase;	

	printf("tina%d: attaching Tina-dd\n", dev->id_unit);

	return(1);
}

/*---------------------------------------------------------------------------*
 *	tina - device driver interrupt routine
 *---------------------------------------------------------------------------*/
void
tinaintr(int unit)
{
}

#if BSD > 199306 && defined(__FreeBSD__)
/*---------------------------------------------------------------------------*
 *	initialization at kernel load time
 *---------------------------------------------------------------------------*/
static void
tinainit(void *unused)
{

    cdevsw_add(&tina_cdevsw);
}

SYSINIT(tinadev, SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR, &tinainit, NULL);

#endif /* BSD > 199306 && defined(__FreeBSD__) */

/*---------------------------------------------------------------------------*
 *	tinaopen - device driver open routine
 *---------------------------------------------------------------------------*/
static int
tinaopen(dev_t dev, int flag, int fmt, struct proc *p)
{
	if(minor(dev))
		return (ENXIO);

	if(openflag)
		return (EBUSY);
	
	openflag = 1;
	
	return(0);
}

/*---------------------------------------------------------------------------*
 *	tinaclose - device driver close routine
 *---------------------------------------------------------------------------*/
static int
tinaclose(dev_t dev, int flag, int fmt, struct proc *p)
{
	openflag = 0;
	return(0);
}

/*---------------------------------------------------------------------------*
 *	tinaioctl - device driver ioctl routine
 *---------------------------------------------------------------------------*/
static int
tinaioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct tina_softc *sc = &tina_sc[minor(dev)];
	u_short iobase = sc->sc_iobase;
	int error = 0;
	int s;

	if(minor(dev))
		return(ENODEV);

	s = splimp();
	
	switch(cmd)
	{
		/* hardware layer - control & status register */

		case ISDN_GETCSR:	/* return control register */
			*(unsigned char *)data = inb(iobase + CTRL_STAT);
			break;
			
		case ISDN_SETCSR:	/* set status register */
			outb((iobase + CTRL_STAT), *(unsigned char *)data);
			break;
			
		/* hardware layer - dual ported memory */
		
		case ISDN_GETBLK:	/* get block from dual port mem */
			readblock(iobase, (*(struct record *)data).addr,
					  (*(struct record *)data).data,
					  (*(struct record *)data).length);
			break;

		case ISDN_SETBLK:	/* write block to dual port mem */
			writeblock(iobase, (*(struct record *)data).data,
					   (*(struct record *)data).addr,
					   (*(struct record *)data).length);
			break;

		default:
			error = ENOTTY;
			break;
	}
	return(error);
}

/*---------------------------------------------------------------------------*
 *	tinapoll - device driver poll routine
 *---------------------------------------------------------------------------*/
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
static int
tinapoll (dev_t dev, int events, struct proc *p)
{
	return (ENODEV);
}
#endif

/*===========================================================================*
 *	tina dual ported memory access
 *===========================================================================*/
 
/*---------------------------------------------------------------------------*
 *	setup address for accessing tina-dd ram
 *---------------------------------------------------------------------------*/
static void
setupaddr(unsigned short iobase, unsigned int addr)
{
	outb((iobase + ADDR_CNTL), (unsigned char) addr & 0xff);
	outb((iobase + ADDR_CNTM), (unsigned char) ((addr >> 8) & 0xff));
	outb((iobase + ADDR_CNTH), (unsigned char) ((addr >> 16) & 0xff));
}


/*---------------------------------------------------------------------------*
 *	read block from tina-dd dual ported ram
 *---------------------------------------------------------------------------*/
static void
readblock(unsigned short iobase, unsigned int addr,
	  unsigned char *dst, unsigned int len)
{
	setupaddr(iobase, addr);	/* setup start address */

	while(len--)			/* tina-dd mem -> pc mem */
		*dst++ = inb(iobase + DATA_LOW_INC);
}

/*---------------------------------------------------------------------------*
 *	write block to tina-dd dual ported ram
 *---------------------------------------------------------------------------*/
static void
writeblock(unsigned short iobase, unsigned char *src,
	  unsigned long addr, int len)
{
	setupaddr(iobase, addr);	/* setup start address */

	while(len--)			/* pc mem -> tina-dd mem */
		outb((iobase + DATA_LOW_INC), *src++);
}

#endif /* NTINA > 0 */
