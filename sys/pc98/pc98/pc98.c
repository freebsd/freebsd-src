/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	from: @(#)isa.c	7.2 (Berkeley) 5/13/91
 *	$Id: pc98.c,v 1.12 1996/12/27 13:06:31 kato Exp $
 */

/*
 * code to manage AT bus
 *
 * 92/08/18  Frank P. MacLachlan (fpm@crash.cts.com):
 * Fixed uninitialized variable problem and added code to deal
 * with DMA page boundaries in isa_dmarangecheck().  Fixed word
 * mode DMA count compution and reorganized DMA setup code in
 * isa_dmastart()
 */

/*
 * modified for PC9801 by A.Kojima F.Ukai M.Ishii 
 *			Kyoto University Microcomputer Club (KMC)
 */

#include "opt_auto_eoi.h"
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#include <machine/md_var.h>
#include <machine/segments.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <i386/isa/isa_device.h>
#ifdef PC98
#include <pc98/pc98/pc98.h>
#include <pc98/pc98/epsonio.h>
#else
#include <i386/isa/isa.h>
#endif
#include <i386/isa/icu.h>
#include <i386/isa/ic/i8237.h>
#include "vector.h"

/*
**  Register definitions for DMA controller 1 (channels 0..3):
*/
#ifdef PC98
#define DMA1_CHN(c)	(IO_DMA + (chan<<2))	/* addr reg for channel c */
#define DMA1_SMSK	(IO_DMA + 0x14)		/* single mask register */
#define DMA1_MODE	(IO_DMA + 0x16)		/* mode register */
#define DMA1_FFC	(IO_DMA + 0x18)		/* clear first/last FF */
#else
#define	DMA1_CHN(c)	(IO_DMA1 + 1*(2*(c)))	/* addr reg for channel c */
#define	DMA1_SMSK	(IO_DMA1 + 1*10)	/* single mask register */
#define	DMA1_MODE	(IO_DMA1 + 1*11)	/* mode register */
#define	DMA1_FFC	(IO_DMA1 + 1*12)	/* clear first/last FF */
#endif

/*
**  Register definitions for DMA controller 2 (channels 4..7):
*/
#define	DMA2_CHN(c)	(IO_DMA2 + 2*(2*(c)))	/* addr reg for channel c */
#define	DMA2_SMSK	(IO_DMA2 + 2*10)	/* single mask register */
#define	DMA2_MODE	(IO_DMA2 + 2*11)	/* mode register */
#define	DMA2_FFC	(IO_DMA2 + 2*12)	/* clear first/last FF */

u_long	*intr_countp[ICU_LEN];
inthand2_t *intr_handler[ICU_LEN];
u_int	intr_mask[ICU_LEN];
u_int*	intr_mptr[ICU_LEN];
int	intr_unit[ICU_LEN];

#ifdef DDB
unsigned int ddb_inb __P((unsigned int addr));
void ddb_outb __P((unsigned int addr, unsigned char dt));
#endif

static inthand_t *fastintr[ICU_LEN] = {
	&IDTVEC(fastintr0), &IDTVEC(fastintr1),
	&IDTVEC(fastintr2), &IDTVEC(fastintr3),
	&IDTVEC(fastintr4), &IDTVEC(fastintr5),
	&IDTVEC(fastintr6), &IDTVEC(fastintr7),
	&IDTVEC(fastintr8), &IDTVEC(fastintr9),
	&IDTVEC(fastintr10), &IDTVEC(fastintr11),
	&IDTVEC(fastintr12), &IDTVEC(fastintr13),
	&IDTVEC(fastintr14), &IDTVEC(fastintr15)
};

static inthand_t *slowintr[ICU_LEN] = {
	&IDTVEC(intr0), &IDTVEC(intr1), &IDTVEC(intr2), &IDTVEC(intr3),
	&IDTVEC(intr4), &IDTVEC(intr5), &IDTVEC(intr6), &IDTVEC(intr7),
	&IDTVEC(intr8), &IDTVEC(intr9), &IDTVEC(intr10), &IDTVEC(intr11),
	&IDTVEC(intr12), &IDTVEC(intr13), &IDTVEC(intr14), &IDTVEC(intr15)
};

static void config_isadev __P((struct isa_device *isdp, u_int *mp));
static void config_isadev_c __P((struct isa_device *isdp, u_int *mp,
				 int reconfig));
static void conflict __P((struct isa_device *dvp, struct isa_device *tmpdvp,
			  int item, char const *whatnot, char const *reason,
			  char const *format));
static int haveseen __P((struct isa_device *dvp, struct isa_device *tmpdvp,
			 u_int checkbits));
static int isa_dmarangecheck __P((caddr_t va, u_int length, int chan));
static inthand2_t isa_strayintr;
static void register_imask __P((struct isa_device *dvp, u_int mask));

/*
 * print a conflict message
 */
static void
conflict(dvp, tmpdvp, item, whatnot, reason, format)
	struct isa_device	*dvp;
	struct isa_device	*tmpdvp;
	int			item;
	char const		*whatnot;
	char const		*reason;
	char const		*format;
{
	printf("%s%d not %sed due to %s conflict with %s%d at ",
		dvp->id_driver->name, dvp->id_unit, whatnot, reason,
		tmpdvp->id_driver->name, tmpdvp->id_unit);
	printf(format, item);
	printf("\n");
}

/*
 * Check to see if things are already in use, like IRQ's, I/O addresses
 * and Memory addresses.
 */
static int
haveseen(dvp, tmpdvp, checkbits)
	struct isa_device *dvp;
	struct isa_device *tmpdvp;
	u_int	checkbits;
{
	/*
	 * Only check against devices that have already been found and are not
	 * unilaterally allowed to conflict anyway.
	 */
	if (tmpdvp->id_alive && !dvp->id_conflicts) {
		char const *whatnot;

		whatnot = checkbits & CC_ATTACH ? "attach" : "prob";
		/*
		 * Check for I/O address conflict.  We can only check the
		 * starting address of the device against the range of the
		 * device that has already been probed since we do not
		 * know how many I/O addresses this device uses.
		 */
		if (checkbits & CC_IOADDR && tmpdvp->id_alive != -1) {
			if ((dvp->id_iobase >= tmpdvp->id_iobase) &&
			    (dvp->id_iobase <=
				  (tmpdvp->id_iobase + tmpdvp->id_alive - 1))) {
				conflict(dvp, tmpdvp, dvp->id_iobase, whatnot,
					 "I/O address", "0x%x");
				return 1;
			}
		}
		/*
		 * Check for Memory address conflict.  We can check for
		 * range overlap, but it will not catch all cases since the
		 * driver may adjust the msize paramater during probe, for
		 * now we just check that the starting address does not
		 * fall within any allocated region.
		 * XXX could add a second check after the probe for overlap,
		 * since at that time we would know the full range.
		 * XXX KERNBASE is a hack, we should have vaddr in the table!
		 */
		if (checkbits & CC_MEMADDR && tmpdvp->id_maddr) {
			if ((KERNBASE + dvp->id_maddr >= tmpdvp->id_maddr) &&
			    (KERNBASE + dvp->id_maddr <=
			     (tmpdvp->id_maddr + tmpdvp->id_msize - 1))) {
				conflict(dvp, tmpdvp, (int)dvp->id_maddr,
					 whatnot, "maddr", "0x%x");
				return 1;
			}
		}
		/*
		 * Check for IRQ conflicts.
		 */
		if (checkbits & CC_IRQ && tmpdvp->id_irq) {
			if (tmpdvp->id_irq == dvp->id_irq) {
				conflict(dvp, tmpdvp, ffs(dvp->id_irq) - 1,
					 whatnot, "irq", "%d");
				return 1;
			}
		}
		/*
		 * Check for DRQ conflicts.
		 */
		if (checkbits & CC_DRQ && tmpdvp->id_drq != -1) {
			if (tmpdvp->id_drq == dvp->id_drq) {
				conflict(dvp, tmpdvp, dvp->id_drq, whatnot,
					 "drq", "%d");
				return 1;
			}
		}
	}
	return 0;
}

/*
 * Search through all the isa_devtab_* tables looking for anything that
 * conflicts with the current device.
 */
int
haveseen_isadev(dvp, checkbits)
	struct isa_device *dvp;
	u_int	checkbits;
{
	struct isa_device *tmpdvp;
	int	status = 0;

	for (tmpdvp = isa_devtab_tty; tmpdvp->id_driver; tmpdvp++) {
		status |= haveseen(dvp, tmpdvp, checkbits);
		if (status)
			return status;
	}
	for (tmpdvp = isa_devtab_bio; tmpdvp->id_driver; tmpdvp++) {
		status |= haveseen(dvp, tmpdvp, checkbits);
		if (status)
			return status;
	}
	for (tmpdvp = isa_devtab_net; tmpdvp->id_driver; tmpdvp++) {
		status |= haveseen(dvp, tmpdvp, checkbits);
		if (status)
			return status;
	}
	for (tmpdvp = isa_devtab_null; tmpdvp->id_driver; tmpdvp++) {
		status |= haveseen(dvp, tmpdvp, checkbits);
		if (status)
			return status;
	}
	return(status);
}

/*
 * Configure all ISA devices
 */
void
isa_configure() {
	struct isa_device *dvp;

	splhigh();
	printf("Probing for devices on the ISA bus:\n");
	/* First probe all the sensitive probes */
	for (dvp = isa_devtab_tty; dvp->id_driver; dvp++)
		if (dvp->id_driver->sensitive_hw)
			config_isadev(dvp, &tty_imask);
	for (dvp = isa_devtab_bio; dvp->id_driver; dvp++)
		if (dvp->id_driver->sensitive_hw)
			config_isadev(dvp, &bio_imask);
	for (dvp = isa_devtab_net; dvp->id_driver; dvp++)
		if (dvp->id_driver->sensitive_hw)
			config_isadev(dvp, &net_imask);
	for (dvp = isa_devtab_null; dvp->id_driver; dvp++)
		if (dvp->id_driver->sensitive_hw)
			config_isadev(dvp, (u_int *)NULL);

	/* Then all the bad ones */
	for (dvp = isa_devtab_tty; dvp->id_driver; dvp++)
		if (!dvp->id_driver->sensitive_hw)
			config_isadev(dvp, &tty_imask);
	for (dvp = isa_devtab_bio; dvp->id_driver; dvp++)
		if (!dvp->id_driver->sensitive_hw)
			config_isadev(dvp, &bio_imask);
	for (dvp = isa_devtab_net; dvp->id_driver; dvp++)
		if (!dvp->id_driver->sensitive_hw)
			config_isadev(dvp, &net_imask);
	for (dvp = isa_devtab_null; dvp->id_driver; dvp++)
		if (!dvp->id_driver->sensitive_hw)
			config_isadev(dvp, (u_int *)NULL);

	bio_imask |= SWI_CLOCK_MASK;
	net_imask |= SWI_NET_MASK;
	tty_imask |= SWI_TTY_MASK;

/*
 * XXX we should really add the tty device to net_imask when the line is
 * switched to SLIPDISC, and then remove it when it is switched away from
 * SLIPDISC.  No need to block out ALL ttys during a splimp when only one
 * of them is running slip.
 *
 * XXX actually, blocking all ttys during a splimp doesn't matter so much
 * with sio because the serial interrupt layer doesn't use tty_imask.  Only
 * non-serial ttys suffer.  It's more stupid that ALL 'net's are blocked
 * during spltty.
 */
#include "sl.h"
#if NSL > 0
	net_imask |= tty_imask;
	tty_imask = net_imask;
#endif

	/* bio_imask |= tty_imask ;  can some tty devices use buffers? */

	if (bootverbose)
		printf("imasks: bio %x, tty %x, net %x\n",
		       bio_imask, tty_imask, net_imask);

	/*
	 * Finish initializing intr_mask[].  Note that the partly
	 * constructed masks aren't actually used since we're at splhigh.
	 * For fully dynamic initialization, register_intr() and
	 * unregister_intr() will have to adjust the masks for _all_
	 * interrupts and for tty_imask, etc.
	 */
	for (dvp = isa_devtab_tty; dvp->id_driver; dvp++)
		register_imask(dvp, tty_imask);
	for (dvp = isa_devtab_bio; dvp->id_driver; dvp++)
		register_imask(dvp, bio_imask);
	for (dvp = isa_devtab_net; dvp->id_driver; dvp++)
		register_imask(dvp, net_imask);
	for (dvp = isa_devtab_null; dvp->id_driver; dvp++)
		register_imask(dvp, SWI_CLOCK_MASK);
	spl0();
}

/*
 * Configure an ISA device.
 */


static void
config_isadev(isdp, mp)
     struct isa_device *isdp;
     u_int *mp;
{
	config_isadev_c(isdp, mp, 0);
}

void
reconfig_isadev(isdp, mp)
	struct isa_device *isdp;
	u_int *mp;
{
	config_isadev_c(isdp, mp, 1);
}

static void
config_isadev_c(isdp, mp, reconfig)
	struct isa_device *isdp;
	u_int *mp;
	int reconfig;
{
	u_int checkbits;
	int id_alive;
	int last_alive;
	struct isa_driver *dp = isdp->id_driver;

	if (!isdp->id_enabled) {
		printf("%s%d: disabled, not probed.\n",
			dp->name, isdp->id_unit);
		return;
	}
	checkbits = CC_DRQ | CC_IOADDR | CC_MEMADDR;
	if (!reconfig && haveseen_isadev(isdp, checkbits))
		return;
	if (!reconfig && isdp->id_maddr) {
		isdp->id_maddr -= 0xa0000; /* XXX should be a define */
		isdp->id_maddr += atdevbase;
	}
	if (reconfig) {
		last_alive = isdp->id_alive;
		isdp->id_reconfig = 1;
	}
	else {
		last_alive = 0;
		isdp->id_reconfig = 0;
	}
	id_alive = (*dp->probe)(isdp);
	if (id_alive) {
		/*
		 * Only print the I/O address range if id_alive != -1
		 * Right now this is a temporary fix just for the new
		 * NPX code so that if it finds a 486 that can use trap
		 * 16 it will not report I/O addresses.
		 * Rod Grimes 04/26/94
		 */
		if (!isdp->id_reconfig) {
			printf("%s%d", dp->name, isdp->id_unit);
			if (id_alive != -1) {
				printf(" at 0x%x", isdp->id_iobase);
				if (isdp->id_iobase + id_alive - 1 !=
				    isdp->id_iobase) {
					printf("-0x%x",
					       isdp->id_iobase + id_alive - 1);
				}
			}
			if (isdp->id_irq)
				printf(" irq %d", ffs(isdp->id_irq) - 1);
			if (isdp->id_drq != -1)
				printf(" drq %d", isdp->id_drq);
			if (isdp->id_maddr)
				printf(" maddr 0x%lx", kvtop(isdp->id_maddr));
			if (isdp->id_msize)
				printf(" msize %d", isdp->id_msize);
			if (isdp->id_flags)
				printf(" flags 0x%x", isdp->id_flags);
#ifdef PC98
			printf (" on isa");
#else
			if (isdp->id_iobase && !(isdp->id_iobase & 0xf300)) {
				printf(" on motherboard");
			} else if (isdp->id_iobase >= 0x1000 &&
				    !(isdp->id_iobase & 0x300)) {
				printf (" on eisa slot %d",
					isdp->id_iobase >> 12);
			} else {
				printf (" on isa");
			}
#endif
			printf("\n");
			/*
			 * Check for conflicts again.  The driver may have
			 * changed *dvp.  We should weaken the early check
			 * since the driver may have been able to change
			 * *dvp to avoid conflicts if given a chance.  We
			 * already skip the early check for IRQs and force
			 * a check for IRQs in the next group of checks.
			 */
			checkbits |= CC_IRQ;
			if (haveseen_isadev(isdp, checkbits))
				return;
			isdp->id_alive = id_alive;
		}
		(*dp->attach)(isdp);
		if (isdp->id_irq) {
			if (mp)
				INTRMASK(*mp, isdp->id_irq);
			register_intr(ffs(isdp->id_irq) - 1, isdp->id_id,
				      isdp->id_ri_flags, isdp->id_intr,
				      mp, isdp->id_unit);
			INTREN(isdp->id_irq);
		}
	} else {
		if (isdp->id_reconfig) {
			(*dp->attach)(isdp); /* reconfiguration attach */
		}
		if (!last_alive) {
			if (!isdp->id_reconfig) {
				printf("%s%d not found",
				       dp->name, isdp->id_unit);
				if (isdp->id_iobase) {
					printf(" at 0x%x", isdp->id_iobase);
				}
				printf("\n");
			}
		}
		else {
			/* This code has not been tested.... */
			if (isdp->id_irq) {
				INTRDIS(isdp->id_irq);
				unregister_intr(ffs(isdp->id_irq) - 1,
						isdp->id_intr);
				if (mp)
					INTRUNMASK(*mp, isdp->id_irq);
			}
		}
	}
}

/*
 * Fill in default interrupt table (in case of spuruious interrupt
 * during configuration of kernel, setup interrupt control unit
 */
void
isa_defaultirq()
{
	int i;

	/* icu vectors */
	for (i = 0; i < ICU_LEN; i++)
		unregister_intr(i, (inthand2_t *)NULL);

	/* initialize 8259's */
#ifdef PC98
	outb(IO_ICU1, 0x11);		/* reset; program device, four bytes */
	outb(IO_ICU1+2, NRSVIDT);	/* starting at this vector index */
	outb(IO_ICU1+2, 1<<7);		/* slave on line 7 */
#ifdef AUTO_EOI_1
	outb(IO_ICU1+2, 0x1f);		/* (master) auto EOI, 8086 mode */
#else
	outb(IO_ICU1+2, 0x1d);		/* (master) 8086 mode */
#endif
	outb(IO_ICU1+2, 0xff);		/* leave interrupts masked */
	outb(IO_ICU1, 0x0a);		/* default to IRR on read */

	outb(IO_ICU2, 0x11);		/* reset; program device, four bytes */
	outb(IO_ICU2+2, NRSVIDT+8);	/* staring at this vector index */
	outb(IO_ICU2+2,7);		/* my slave id is 7 */
	outb(IO_ICU2+2,9);		/* 8086 mode */
	outb(IO_ICU2+2, 0xff);		/* leave interrupts masked */
	outb(IO_ICU2, 0x0a);		/* default to IRR on read */
#else
	outb(IO_ICU1, 0x11);		/* reset; program device, four bytes */
	outb(IO_ICU1+1, NRSVIDT);	/* starting at this vector index */
	outb(IO_ICU1+1, 1<<2);		/* slave on line 2 */
#ifdef AUTO_EOI_1
	outb(IO_ICU1+1, 2 | 1);		/* auto EOI, 8086 mode */
#else
	outb(IO_ICU1+1, 1);		/* 8086 mode */
#endif
	outb(IO_ICU1+1, 0xff);		/* leave interrupts masked */
	outb(IO_ICU1, 0x0a);		/* default to IRR on read */
	outb(IO_ICU1, 0xc0 | (3 - 1));	/* pri order 3-7, 0-2 (com2 first) */

	outb(IO_ICU2, 0x11);		/* reset; program device, four bytes */
	outb(IO_ICU2+1, NRSVIDT+8);	/* staring at this vector index */
	outb(IO_ICU2+1,2);		/* my slave id is 2 */
#ifdef AUTO_EOI_2
	outb(IO_ICU2+1, 2 | 1);		/* auto EOI, 8086 mode */
#else
	outb(IO_ICU2+1,1);		/* 8086 mode */
#endif
	outb(IO_ICU2+1, 0xff);		/* leave interrupts masked */
	outb(IO_ICU2, 0x0a);		/* default to IRR on read */
#endif
}

#ifdef PC98
caddr_t	dma_bouncebuf[4];
static u_int	dma_bouncebufsize[4];
#else
caddr_t	dma_bouncebuf[8];
static u_int	dma_bouncebufsize[8];
#endif
static u_int8_t	dma_bounced = 0;
static u_int8_t	dma_busy = 0;		/* Used in isa_dmastart() */
static u_int8_t	dma_inuse = 0;		/* User for acquire/release */

#ifdef PC98
#define VALID_DMA_MASK (3)
#else
#define VALID_DMA_MASK (7)
#endif

/* high byte of address is stored in this port for i-th dma channel */
#ifdef PC98
static int dmapageport[4] = { 0x27, 0x21, 0x23, 0x25 };
#else /* IBM-PC */
static int dmapageport[8] = { 0x87, 0x83, 0x81, 0x82, 0x8f, 0x8b, 0x89, 0x8a };
#endif

/*
 * Setup a DMA channel's bounce buffer.
 */
void
isa_dmainit(chan, bouncebufsize)
	int chan;
	u_int bouncebufsize;
{
	void *buf;

#ifdef DIAGNOSTIC
	if (chan & ~VALID_DMA_MASK)
		panic("isa_dmainit: channel out of range");

	if (dma_bouncebuf[chan] != NULL)
		panic("isa_dmainit: impossible request"); 
#endif

	dma_bouncebufsize[chan] = bouncebufsize;

	/* Try malloc() first.  It works better if it works. */
	buf = malloc(bouncebufsize, M_DEVBUF, M_NOWAIT);
	if (buf != NULL) {
		if (isa_dmarangecheck(buf, bouncebufsize, chan) == 0) {
			dma_bouncebuf[chan] = buf;
			return;
		}
		free(buf, M_DEVBUF);
	}
	buf = contigmalloc(bouncebufsize, M_DEVBUF, M_NOWAIT, 0ul, 0xfffffful,
			   1ul, chan & 4 ? 0x20000ul : 0x10000ul);
	if (buf == NULL)
		printf("isa_dmainit(%d, %d) failed\n", chan, bouncebufsize);
	else
		dma_bouncebuf[chan] = buf;
}

/*
 * Register a DMA channel's usage.  Usually called from a device driver
 * in open() or during it's initialization.
 */
int
isa_dma_acquire(chan)
	int chan;
{
#ifdef DIAGNOSTIC
	if (chan & ~VALID_DMA_MASK)
		panic("isa_dma_acquire: channel out of range");
#endif

	if (dma_inuse & (1 << chan)) {
		printf("isa_dma_acquire: channel %d already in use\n", chan);
		return (EBUSY);
	}
	dma_inuse |= (1 << chan);

	return (0);
}

/*
 * Unregister a DMA channel's usage.  Usually called from a device driver
 * during close() or during it's shutdown.
 */
void
isa_dma_release(chan)
	int chan;
{
#ifdef DIAGNOSTIC
	if (chan & ~VALID_DMA_MASK)
		panic("isa_dma_release: channel out of range");

	if (dma_inuse & (1 << chan) == 0)
		printf("isa_dma_release: channel %d not in use\n", chan);
#endif

	if (dma_busy & (1 << chan)) {
		dma_busy &= ~(1 << chan);
		/* 
		 * XXX We should also do "dma_bounced &= (1 << chan);"
		 * because we are acting on behalf of isa_dmadone() which
		 * was not called to end the last DMA operation.  This does
		 * not matter now, but it may in the future.
		 */
	}

	dma_inuse &= ~(1 << chan);
}

#ifndef PC98
/*
 * isa_dmacascade(): program 8237 DMA controller channel to accept
 * external dma control by a board.
 */
void isa_dmacascade(chan)
	int chan;
{
#ifdef DIAGNOSTIC
	if (chan & ~VALID_DMA_MASK)
		panic("isa_dmacascade: channel out of range");
#endif

	/* set dma channel mode, and set dma channel mode */
	if ((chan & 4) == 0) {
		outb(DMA1_MODE, DMA37MD_CASCADE | chan);
		outb(DMA1_SMSK, chan);
	} else {
		outb(DMA2_MODE, DMA37MD_CASCADE | (chan & 3));
		outb(DMA2_SMSK, chan & 3);
	}
}
#endif

/*
 * isa_dmastart(): program 8237 DMA controller channel, avoid page alignment
 * problems by using a bounce buffer.
 */
void isa_dmastart(int flags, caddr_t addr, u_int nbytes, int chan)
{
	vm_offset_t phys;
	int waport;
	caddr_t newaddr;

#ifdef DIAGNOSTIC
	if (chan & ~VALID_DMA_MASK)
		panic("isa_dmastart: channel out of range");

	if ((chan < 4 && nbytes > (1<<16))
	    || (chan >= 4 && (nbytes > (1<<17) || (u_int)addr & 1)))
		panic("isa_dmastart: impossible request");

	if (dma_inuse & (1 << chan) == 0)
		printf("isa_dmastart: channel %d not acquired\n", chan);
#endif

	if (dma_busy & (1 << chan))
		printf("isa_dmastart: channel %d busy\n", chan);

	dma_busy |= (1 << chan);

	if (isa_dmarangecheck(addr, nbytes, chan)) {
		if (dma_bouncebuf[chan] == NULL
		    || dma_bouncebufsize[chan] < nbytes)
			panic("isa_dmastart: bad bounce buffer"); 
		dma_bounced |= (1 << chan);
		newaddr = dma_bouncebuf[chan];

		/* copy bounce buffer on write */
		if (!(flags & B_READ))
			bcopy(addr, newaddr, nbytes);
		addr = newaddr;
	}

	/* translate to physical */
	phys = pmap_extract(pmap_kernel(), (vm_offset_t)addr);

#ifdef CYRIX_5X86
	asm("wbinvd");		/* wbinvd (WB cache flush) */
#endif

#ifndef PC98
	if ((chan & 4) == 0) {
		/*
		 * Program one of DMA channels 0..3.  These are
		 * byte mode channels.
		 */
#endif
		/* set dma channel mode, and reset address ff */

		/* If B_RAW flag is set, then use autoinitialise mode */
		if (flags & B_RAW) {
		  if (flags & B_READ)
			outb(DMA1_MODE, DMA37MD_AUTO|DMA37MD_WRITE|chan);
		  else
			outb(DMA1_MODE, DMA37MD_AUTO|DMA37MD_READ|chan);
		}
		else
		if (flags & B_READ)
			outb(DMA1_MODE, DMA37MD_SINGLE|DMA37MD_WRITE|chan);
		else
			outb(DMA1_MODE, DMA37MD_SINGLE|DMA37MD_READ|chan);
		outb(DMA1_FFC, 0);

		/* send start address */
		waport =  DMA1_CHN(chan);
		outb(waport, phys);
		outb(waport, phys>>8);
		outb(dmapageport[chan], phys>>16);

		/* send count */
		outb(waport + 2, --nbytes);
		outb(waport + 2, nbytes>>8);

		/* unmask channel */
		outb(DMA1_SMSK, chan);
#ifndef PC98
	} else {
		/*
		 * Program one of DMA channels 4..7.  These are
		 * word mode channels.
		 */
		/* set dma channel mode, and reset address ff */

		/* If B_RAW flag is set, then use autoinitialise mode */
		if (flags & B_RAW) {
		  if (flags & B_READ)
			outb(DMA2_MODE, DMA37MD_AUTO|DMA37MD_WRITE|(chan&3));
		  else
			outb(DMA2_MODE, DMA37MD_AUTO|DMA37MD_READ|(chan&3));
		}
		else
		if (flags & B_READ)
			outb(DMA2_MODE, DMA37MD_SINGLE|DMA37MD_WRITE|(chan&3));
		else
			outb(DMA2_MODE, DMA37MD_SINGLE|DMA37MD_READ|(chan&3));
		outb(DMA2_FFC, 0);

		/* send start address */
		waport = DMA2_CHN(chan - 4);
		outb(waport, phys>>1);
		outb(waport, phys>>9);
		outb(dmapageport[chan], phys>>16);

		/* send count */
		nbytes >>= 1;
		outb(waport + 2, --nbytes);
		outb(waport + 2, nbytes>>8);

		/* unmask channel */
		outb(DMA2_SMSK, chan & 3);
	}
#endif
}

void isa_dmadone(int flags, caddr_t addr, int nbytes, int chan)
{
#if defined(CYRIX_486DLC) || defined(IBM_486SLC)
	if (flags & B_READ) {
		/* cache flush only after reading 92/12/9 by A.Kojima */
		asm("	.byte 0x0f,0x08");	/* invd (cache flush) */
	}
#endif

#ifdef DIAGNOSTIC
	if (chan & ~VALID_DMA_MASK)
		panic("isa_dmadone: channel out of range");

	if (dma_inuse & (1 << chan) == 0)
		printf("isa_dmadone: channel %d not acquired\n", chan);
#endif

#if 0
	/*
	 * XXX This should be checked, but drivers like ad1848 only call
	 * isa_dmastart() once because they use Auto DMA mode.  If we
	 * leave this in, drivers that do this will print this continuously.
	 */
	if (dma_busy & (1 << chan) == 0)
		printf("isa_dmadone: channel %d not busy\n", chan);
#endif

	if (dma_bounced & (1 << chan)) {
		/* copy bounce buffer on read */
		if (flags & B_READ)
			bcopy(dma_bouncebuf[chan], addr, nbytes);

		dma_bounced &= ~(1 << chan);
	}
	dma_busy &= ~(1 << chan);
}

/*
 * Check for problems with the address range of a DMA transfer
 * (non-contiguous physical pages, outside of bus address space,
 * crossing DMA page boundaries).
 * Return true if special handling needed.
 */

static int
isa_dmarangecheck(caddr_t va, u_int length, int chan) {
	vm_offset_t phys, priorpage = 0, endva;
	u_int dma_pgmsk = (chan & 4) ?  ~(128*1024-1) : ~(64*1024-1);

	endva = (vm_offset_t)round_page(va + length);
	for (; va < (caddr_t) endva ; va += PAGE_SIZE) {
		phys = trunc_page(pmap_extract(pmap_kernel(), (vm_offset_t)va));
#ifdef EPSON_BOUNCEDMA
#define ISARAM_END   0xf00000
#else
#define ISARAM_END	RAM_END
#endif
		if (phys == 0)
			panic("isa_dmacheck: no physical page present");
		if (phys >= ISARAM_END)
			return (1);
		if (priorpage) {
			if (priorpage + PAGE_SIZE != phys)
				return (1);
			/* check if crossing a DMA page boundary */
			if (((u_int)priorpage ^ (u_int)phys) & dma_pgmsk)
				return (1);
		}
		priorpage = phys;
	}
	return (0);
}

#ifdef PC98
#define NMI_PARITY 0x04
#define NMI_EPARITY 0x02
#else
#define NMI_PARITY (1 << 7)
#define NMI_IOCHAN (1 << 6)
#define ENMI_WATCHDOG (1 << 7)
#define ENMI_BUSTIMER (1 << 6)
#define ENMI_IOSTATUS (1 << 5)
#endif

/*
 * Handle a NMI, possibly a machine check.
 * return true to panic system, false to ignore.
 */
int
isa_nmi(cd)
	int cd;
{
#ifdef PC98
 	int port = inb(0x33);
	if (epson_machine_id == 0x20)
		epson_outb(0xc16, epson_inb(0xc16) | 0x1);
	if (port & NMI_PARITY) {
		panic("BASE RAM parity error, likely hardware failure.");
	} else if (port & NMI_EPARITY) {
		panic("EXTENDED RAM parity error, likely hardware failure.");
	} else {
		printf("\nNMI Resume ??\n");
		return(0);
	}
#else /* IBM-PC */
	int isa_port = inb(0x61);
	int eisa_port = inb(0x461);
	if(isa_port & NMI_PARITY) {
		panic("RAM parity error, likely hardware failure.");
	} else if(isa_port & NMI_IOCHAN) {
		panic("I/O channel check, likely hardware failure.");
	} else if(eisa_port & ENMI_WATCHDOG) {
		panic("EISA watchdog timer expired, likely hardware failure.");
	} else if(eisa_port & ENMI_BUSTIMER) {
		panic("EISA bus timeout, likely hardware failure.");
	} else if(eisa_port & ENMI_IOSTATUS) {
		panic("EISA I/O port status error.");
	} else {
		printf("\nNMI ISA %x, EISA %x\n", isa_port, eisa_port);
		return(0);
	}
#endif
}

/*
 * Caught a stray interrupt, notify
 */
static void
isa_strayintr(d)
	int d;
{

	/* DON'T BOTHER FOR NOW! */
	/* for some reason, we get bursts of intr #7, even if not enabled! */
	/*
	 * Well the reason you got bursts of intr #7 is because someone
	 * raised an interrupt line and dropped it before the 8259 could
	 * prioritize it.  This is documented in the intel data book.  This
	 * means you have BAD hardware!  I have changed this so that only
	 * the first 5 get logged, then it quits logging them, and puts
	 * out a special message. rgrimes 3/25/1993
	 */
	/*
	 * XXX TODO print a different message for #7 if it is for a
	 * glitch.  Glitches can be distinguished from real #7's by
	 * testing that the in-service bit is _not_ set.  The test
	 * must be done before sending an EOI so it can't be done if
	 * we are using AUTO_EOI_1.
	 */
	if (intrcnt[NR_DEVICES + d] <= 5)
		log(LOG_ERR, "stray irq %d\n", d);
	if (intrcnt[NR_DEVICES + d] == 5)
		log(LOG_CRIT,
		    "too many stray irq %d's; not logging any more\n", d);
}

/*
 * Find the highest priority enabled display device.  Since we can't
 * distinguish display devices from ttys, depend on display devices
 * being sensitive and before sensitive non-display devices (if any)
 * in isa_devtab_tty.
 *
 * XXX we should add capability flags IAMDISPLAY and ISUPPORTCONSOLES.
 */
struct isa_device *
find_display()
{
	struct isa_device *dvp;

	for (dvp = isa_devtab_tty; dvp->id_driver != NULL; dvp++)
		if (dvp->id_driver->sensitive_hw && dvp->id_enabled)
			return (dvp);
	return (NULL);
}

/*
 * find an ISA device in a given isa_devtab_* table, given
 * the table to search, the expected id_driver entry, and the unit number.
 *
 * this function is defined in isa_device.h, and this location is debatable;
 * i put it there because it's useless w/o, and directly operates on
 * the other stuff in that file.
 *
 */

struct isa_device *find_isadev(table, driverp, unit)
     struct isa_device *table;
     struct isa_driver *driverp;
     int unit;
{
  if (driverp == NULL) /* sanity check */
    return NULL;

  while ((table->id_driver != driverp) || (table->id_unit != unit)) {
    if (table->id_driver == 0)
      return NULL;

    table++;
  }

  return table;
}

/*
 * Return nonzero if a (masked) irq is pending for a given device.
 */
int
isa_irq_pending(dvp)
	struct isa_device *dvp;
{
	unsigned id_irq;

	id_irq = dvp->id_irq;
	if (id_irq & 0xff)
		return (inb(IO_ICU1) & id_irq);
	return (inb(IO_ICU2) & (id_irq >> 8));
}

int
update_intr_masks(void)
{
	int intr, n=0;
	u_int mask,*maskptr;

	for (intr=0; intr < ICU_LEN; intr ++) {
#ifdef PC98
		if (intr==7) continue;
#else
		if (intr==2) continue;
#endif
		maskptr = intr_mptr[intr];
		if (!maskptr) continue;
		*maskptr |= 1 << intr;
		mask = *maskptr;
		if (mask != intr_mask[intr]) {
#if 0
			printf ("intr_mask[%2d] old=%08x new=%08x ptr=%p.\n",
				intr, intr_mask[intr], mask, maskptr);
#endif
			intr_mask[intr]=mask;
			n++;
		}

	}
	return (n);
}

int
register_intr(intr, device_id, flags, handler, maskptr, unit)
	int	intr;
	int	device_id;
	u_int	flags;
	inthand2_t *handler;
	u_int	*maskptr;
	int	unit;
{
	char	*cp;
	u_long	ef;
	int	id;
	u_int	mask = (maskptr ? *maskptr : 0);

#ifdef PC98
	if ((u_int)intr >= ICU_LEN || intr == 7
#else
	if ((u_int)intr >= ICU_LEN || intr == 2
#endif
	    || (u_int)device_id >= NR_DEVICES)
		return (EINVAL);
	if (intr_handler[intr] != isa_strayintr)
		return (EBUSY);
	ef = read_eflags();
	disable_intr();
	intr_countp[intr] = &intrcnt[device_id];
	intr_handler[intr] = handler;
	intr_mptr[intr] = maskptr;
	intr_mask[intr] = mask | (1 << intr);
	intr_unit[intr] = unit;
	setidt(ICU_OFFSET + intr,
	       flags & RI_FAST ? fastintr[intr] : slowintr[intr],
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	write_eflags(ef);
	for (cp = intrnames, id = 0; id <= device_id; id++)
		while (*cp++ != '\0')
			;
	if (cp > eintrnames)
		return (0);
	if (intr < 10) {
		cp[-3] = intr + '0';
		cp[-2] = ' ';
	} else {
		cp[-3] = '1';
		cp[-2] = intr - 10 + '0';
	}
	return (0);
}

static void
register_imask(dvp, mask)
	struct isa_device *dvp;
	u_int	mask;
{
	if (dvp->id_alive && dvp->id_irq) {
		int	intr;

		intr = ffs(dvp->id_irq) - 1;
		intr_mask[intr] = mask | (1 <<intr);
	}
	(void) update_intr_masks();
}

int
unregister_intr(intr, handler)
	int	intr;
	inthand2_t *handler;
{
	u_long	ef;

	if ((u_int)intr >= ICU_LEN || handler != intr_handler[intr])
		return (EINVAL);
	ef = read_eflags();
	disable_intr();
	intr_countp[intr] = &intrcnt[NR_DEVICES + intr];
	intr_handler[intr] = isa_strayintr;
	intr_mptr[intr] = NULL;
	intr_mask[intr] = HWI_MASK | SWI_MASK;
	intr_unit[intr] = intr;
	setidt(ICU_OFFSET + intr, slowintr[intr], SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	write_eflags(ef);
	return (0);
}

#ifdef DDB
unsigned int ddb_inb(unsigned int addr)
{
	return inb(addr);
}

void ddb_outb(unsigned int addr, unsigned char dt)
{
	outb(addr, dt);
}
#endif
