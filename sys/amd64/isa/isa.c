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
 *	$Id: isa.c,v 1.107 1997/11/21 18:13:58 bde Exp $
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <machine/ipl.h>
#include <machine/md_var.h>
#ifdef APIC_IO
#include <machine/smp.h>
#endif /* APIC_IO */
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/intr_machdep.h>
#include <i386/isa/isa.h>
#include <i386/isa/ic/i8237.h>

#include <sys/interrupt.h>

#include "pnp.h"
#if NPNP > 0
#include <i386/isa/pnp.h>
#endif

/*
**  Register definitions for DMA controller 1 (channels 0..3):
*/
#define	DMA1_CHN(c)	(IO_DMA1 + 1*(2*(c)))	/* addr reg for channel c */
#define	DMA1_SMSK	(IO_DMA1 + 1*10)	/* single mask register */
#define	DMA1_MODE	(IO_DMA1 + 1*11)	/* mode register */
#define	DMA1_FFC	(IO_DMA1 + 1*12)	/* clear first/last FF */

/*
**  Register definitions for DMA controller 2 (channels 4..7):
*/
#define	DMA2_CHN(c)	(IO_DMA2 + 2*(2*(c)))	/* addr reg for channel c */
#define	DMA2_SMSK	(IO_DMA2 + 2*10)	/* single mask register */
#define	DMA2_MODE	(IO_DMA2 + 2*11)	/* mode register */
#define	DMA2_FFC	(IO_DMA2 + 2*12)	/* clear first/last FF */

static void config_isadev __P((struct isa_device *isdp, u_int *mp));
static void config_isadev_c __P((struct isa_device *isdp, u_int *mp,
				 int reconfig));
static void conflict __P((struct isa_device *dvp, struct isa_device *tmpdvp,
			  int item, char const *whatnot, char const *reason,
			  char const *format));
static int haveseen __P((struct isa_device *dvp, struct isa_device *tmpdvp,
			 u_int checkbits));
static int isa_dmarangecheck __P((caddr_t va, u_int length, int chan));

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
	 * Ignore all conflicts except IRQ ones if conflicts are allowed.
	 */
	if (dvp->id_conflicts)
		checkbits &= ~(CC_DRQ | CC_IOADDR | CC_MEMADDR);
	/*
	 * Only check against devices that have already been found.
	 */
	if (tmpdvp->id_alive) {
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

#ifdef RESOURCE_CHECK
#include <sys/drvresource.h>

static int
checkone (struct isa_device *dvp, int type, addr_t low, addr_t high, 
	  char *resname, char *resfmt, int attaching)
{
	int result = 0;
	if (bootverbose) {
		if (low == high)
			printf("\tcheck %s: 0x%x\n", resname, low);
		else
			printf("\tcheck %s: 0x%x to 0x%x\n", 
			       resname, low, high);
	}
	if (resource_check(type, RESF_NONE, low, high) != NULL) {
		char *whatnot = attaching ? "attach" : "prob";
		static struct isa_device dummydev;
		static struct isa_driver dummydrv;
		struct isa_device *tmpdvp = &dummydev;

		dummydev.id_driver = &dummydrv;
		dummydev.id_unit = 0;
		dummydrv.name = "pci";
		conflict(dvp, tmpdvp, low, whatnot, resname, resfmt);
		result = 1;
	} else if (attaching) {
		if (low == high)
			printf("\tregister %s: 0x%x\n", resname, low);
		else
			printf("\tregister %s: 0x%x to 0x%x\n",
			       resname, low, high);
		resource_claim(dvp, type, RESF_NONE, low, high);
	}
	return (result);
}

static int 
check_pciconflict(struct isa_device *dvp, int checkbits)
{
	int result = 0;
	int attaching = (checkbits & CC_ATTACH) != 0;

	if (checkbits & CC_MEMADDR) {
		long maddr = dvp->id_maddr;
		long msize = dvp->id_msize;
		if (msize > 0) {
			if (checkone(dvp, REST_MEM, maddr, maddr + msize - 1,
				     "maddr", "0x%x", attaching) != 0) {
				result = 1;
				attaching = 0;
			}
		}
	}
	if (checkbits & CC_IOADDR) {
		unsigned iobase = dvp->id_iobase;
		unsigned iosize = dvp->id_alive;
		if (iosize == -1)
			iosize = 1; /* XXX can't do much about this ... */
		if (iosize > 0) {
			if (checkone(dvp, REST_PORT, iobase, iobase + iosize -1,
				     "I/O address", "0x%x", attaching) != 0) {
				result = 1;
				attaching = 0;
			}
		}
	}
	if (checkbits & CC_IRQ) {
		int irq = ffs(dvp->id_irq) - 1;
		if (irq >= 0) {
			if (checkone(dvp, REST_INT, irq, irq, 
				     "irq", "%d", attaching) != 0) {
				result = 1;
				attaching = 0;
			}
		}
	}
	if (checkbits & CC_DRQ) {
		int drq = dvp->id_drq;
		if (drq >= 0) {
			if (checkone(dvp, REST_DMA, drq, drq, 
				     "drq", "%d", attaching) != 0) {
				result = 1;
				attaching = 0;
			}
		}
	}
	if (result != 0)
		resource_free (dvp);
	return (result);
}
#endif /* RESOURCE_CHECK */

/*
 * Search through all the isa_devtab_* tables looking for anything that
 * conflicts with the current device.
 */
int
haveseen_isadev(dvp, checkbits)
	struct isa_device *dvp;
	u_int	checkbits;
{
#if NPNP > 0
	struct pnp_dlist_node *nod;
#endif
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
	for (tmpdvp = isa_devtab_cam; tmpdvp->id_driver; tmpdvp++) {
		status |= haveseen(dvp, tmpdvp, checkbits);
		if (status)
			return status;
	}
	for (tmpdvp = isa_devtab_null; tmpdvp->id_driver; tmpdvp++) {
		status |= haveseen(dvp, tmpdvp, checkbits);
		if (status)
			return status;
	}
#if NPNP > 0
	for (nod = pnp_device_list; nod != NULL; nod = nod->next)
		if (status |= haveseen(dvp, &(nod->dev), checkbits))
			return status;
#endif
#ifdef RESOURCE_CHECK
	if (!dvp->id_conflicts)
		status = check_pciconflict(dvp, checkbits);
	else if (bootverbose)
		printf("\tnot checking for resource conflicts ...\n");
#endif /* RESOURCE_CHECK */
	return(status);
}

/*
 * Configure all ISA devices
 */
void
isa_configure()
{
	struct isa_device *dvp;

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
	for (dvp = isa_devtab_cam; dvp->id_driver; dvp++)
		if (dvp->id_driver->sensitive_hw)
			config_isadev(dvp, &cam_imask);
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
	for (dvp = isa_devtab_cam; dvp->id_driver; dvp++)
		if (!dvp->id_driver->sensitive_hw)
			config_isadev(dvp, &cam_imask);
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
	 * icu_unset() will have to adjust the masks for _all_
	 * interrupts and for tty_imask, etc.
	 */
	for (dvp = isa_devtab_tty; dvp->id_driver; dvp++)
		register_imask(dvp, tty_imask);
	for (dvp = isa_devtab_bio; dvp->id_driver; dvp++)
		register_imask(dvp, bio_imask);
	for (dvp = isa_devtab_net; dvp->id_driver; dvp++)
		register_imask(dvp, net_imask);
	for (dvp = isa_devtab_cam; dvp->id_driver; dvp++)
		register_imask(dvp, cam_imask);
	for (dvp = isa_devtab_null; dvp->id_driver; dvp++)
		register_imask(dvp, SWI_CLOCK_MASK);
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
	    if (bootverbose)
		printf("%s%d: disabled, not probed.\n", dp->name, isdp->id_unit);
	    return;
	}
	checkbits = CC_DRQ | CC_IOADDR | CC_MEMADDR;
	if (!reconfig && haveseen_isadev(isdp, checkbits))
		return;
	if (!reconfig && isdp->id_maddr) {
		isdp->id_maddr -= ISA_HOLE_START;
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
				if (isdp->id_iobase == -1)
					printf(" at ?");
				else {
					printf(" at 0x%x", isdp->id_iobase);
					if (isdp->id_iobase + id_alive - 1 !=
					    isdp->id_iobase) {
						printf("-0x%x",
						       isdp->id_iobase + id_alive - 1);
					}
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
			if (isdp->id_iobase && !(isdp->id_iobase & 0xf300)) {
				printf(" on motherboard");
			} else if (isdp->id_iobase >= 0x1000 &&
				    !(isdp->id_iobase & 0x300)) {
				printf (" on eisa slot %d",
					isdp->id_iobase >> 12);
			} else {
				printf (" on isa");
			}
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
#ifdef APIC_IO
			/*
			 * Some motherboards use upper IRQs for traditional
			 * ISA INTerrupt sources.  In particular we have
			 * seen the secondary IDE connected to IRQ20.
			 * This code detects and fixes this situation.
			 */
			u_int	apic_mask;
			int	rirq;

			apic_mask = isa_apic_mask(isdp->id_irq);
			if (apic_mask != isdp->id_irq) {
				rirq = ffs(isdp->id_irq) - 1;
				isdp->id_irq = apic_mask;
				undirect_isa_irq(rirq);	/* free for ISA */
			}
#endif /* APIC_IO */
			register_intr(ffs(isdp->id_irq) - 1, isdp->id_id,
				      isdp->id_ri_flags, isdp->id_intr,
				      mp, isdp->id_unit);
		}
	} else {
		if (isdp->id_reconfig) {
			(*dp->attach)(isdp); /* reconfiguration attach */
		}
		if (!last_alive) {
			if (!isdp->id_reconfig) {
				printf("%s%d not found",
				       dp->name, isdp->id_unit);
				if (isdp->id_iobase != -1)
					printf(" at 0x%x", isdp->id_iobase);
				printf("\n");
			}
		} else {
#if 0
			/* This code has not been tested.... */
			if (isdp->id_irq) {
				icu_unset(ffs(isdp->id_irq) - 1,
						isdp->id_intr);
				if (mp)
					INTRUNMASK(*mp, isdp->id_irq);
			}
#else
			printf ("icu_unset() not supported here ...\n");
#endif
		}
	}
}

static caddr_t	dma_bouncebuf[8];
static u_int	dma_bouncebufsize[8];
static u_int8_t	dma_bounced = 0;
static u_int8_t	dma_busy = 0;		/* Used in isa_dmastart() */
static u_int8_t	dma_inuse = 0;		/* User for acquire/release */
static u_int8_t dma_auto_mode = 0;

#define VALID_DMA_MASK (7)

/* high byte of address is stored in this port for i-th dma channel */
static int dmapageport[8] = { 0x87, 0x83, 0x81, 0x82, 0x8f, 0x8b, 0x89, 0x8a };

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
	dma_auto_mode &= ~(1 << chan);

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

	if ((dma_inuse & (1 << chan)) == 0)
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
	dma_auto_mode &= ~(1 << chan);
}

/*
 * isa_dmacascade(): program 8237 DMA controller channel to accept
 * external dma control by a board.
 */
void
isa_dmacascade(chan)
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

/*
 * isa_dmastart(): program 8237 DMA controller channel, avoid page alignment
 * problems by using a bounce buffer.
 */
void
isa_dmastart(int flags, caddr_t addr, u_int nbytes, int chan)
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

	if ((dma_inuse & (1 << chan)) == 0)
		printf("isa_dmastart: channel %d not acquired\n", chan);
#endif

#if 0
	/*
	 * XXX This should be checked, but drivers like ad1848 only call
	 * isa_dmastart() once because they use Auto DMA mode.  If we
	 * leave this in, drivers that do this will print this continuously.
	 */
	if (dma_busy & (1 << chan))
		printf("isa_dmastart: channel %d busy\n", chan);
#endif

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

	if (flags & B_RAW) {
	    dma_auto_mode |= (1 << chan);
	} else { 
	    dma_auto_mode &= ~(1 << chan);
	}

	if ((chan & 4) == 0) {
		/*
		 * Program one of DMA channels 0..3.  These are
		 * byte mode channels.
		 */
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
		outb(waport + 1, --nbytes);
		outb(waport + 1, nbytes>>8);

		/* unmask channel */
		outb(DMA1_SMSK, chan);
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
}

void
isa_dmadone(int flags, caddr_t addr, int nbytes, int chan)
{  
#ifdef DIAGNOSTIC
	if (chan & ~VALID_DMA_MASK)
		panic("isa_dmadone: channel out of range");

	if ((dma_inuse & (1 << chan)) == 0)
		printf("isa_dmadone: channel %d not acquired\n", chan);
#endif

	if (((dma_busy & (1 << chan)) == 0) && 
	    (dma_auto_mode & (1 << chan)) == 0 )
		printf("isa_dmadone: channel %d not busy\n", chan);


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
isa_dmarangecheck(caddr_t va, u_int length, int chan)
{
	vm_offset_t phys, priorpage = 0, endva;
	u_int dma_pgmsk = (chan & 4) ?  ~(128*1024-1) : ~(64*1024-1);

	endva = (vm_offset_t)round_page(va + length);
	for (; va < (caddr_t) endva ; va += PAGE_SIZE) {
		phys = trunc_page(pmap_extract(pmap_kernel(), (vm_offset_t)va));
#define ISARAM_END	RAM_END
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

/*
 * Query the progress of a transfer on a DMA channel.
 *
 * To avoid having to interrupt a transfer in progress, we sample
 * each of the high and low databytes twice, and apply the following
 * logic to determine the correct count.
 *
 * Reads are performed with interrupts disabled, thus it is to be
 * expected that the time between reads is very small.  At most
 * one rollover in the low count byte can be expected within the
 * four reads that are performed.
 *
 * There are three gaps in which a rollover can occur :
 *
 * - read low1
 *              gap1
 * - read high1
 *              gap2
 * - read low2
 *              gap3
 * - read high2
 *
 * If a rollover occurs in gap1 or gap2, the low2 value will be
 * greater than the low1 value.  In this case, low2 and high2 are a
 * corresponding pair. 
 *
 * In any other case, low1 and high1 can be considered to be correct.
 *
 * The function returns the number of bytes remaining in the transfer,
 * or -1 if the channel requested is not active.
 *
 */
int
isa_dmastatus(int chan)
{
	u_long	cnt = 0;
	int	ffport, waport;
	u_long	low1, high1, low2, high2;

	/* channel active? */
	if ((dma_inuse & (1 << chan)) == 0) {
		printf("isa_dmastatus: channel %d not active\n", chan);
		return(-1);
	}
	/* channel busy? */

	if (((dma_busy & (1 << chan)) == 0) &&
	    (dma_auto_mode & (1 << chan)) == 0 ) {
	    printf("chan %d not busy\n", chan);
	    return -2 ;
	}	
	if (chan < 4) {			/* low DMA controller */
		ffport = DMA1_FFC;
		waport = DMA1_CHN(chan) + 1;
	} else {			/* high DMA controller */
		ffport = DMA2_FFC;
		waport = DMA2_CHN(chan - 4) + 2;
	}

	disable_intr();			/* no interrupts Mr Jones! */
	outb(ffport, 0);		/* clear register LSB flipflop */
	low1 = inb(waport);
	high1 = inb(waport);
	outb(ffport, 0);		/* clear again */
	low2 = inb(waport);
	high2 = inb(waport);
	enable_intr();			/* enable interrupts again */

	/* 
	 * Now decide if a wrap has tried to skew our results.
	 * Note that after TC, the count will read 0xffff, while we want 
	 * to return zero, so we add and then mask to compensate.
	 */
	if (low1 >= low2) {
		cnt = (low1 + (high1 << 8) + 1) & 0xffff;
	} else {
		cnt = (low2 + (high2 << 8) + 1) & 0xffff;
	}

	if (chan >= 4)			/* high channels move words */
		cnt *= 2;
	return(cnt);
}

/*
 * Stop a DMA transfer currently in progress.
 */
int
isa_dmastop(int chan) 
{
	if ((dma_inuse & (1 << chan)) == 0)
		printf("isa_dmastop: channel %d not acquired\n", chan);  

	if (((dma_busy & (1 << chan)) == 0) &&
	    ((dma_auto_mode & (1 << chan)) == 0)) {
		printf("chan %d not busy\n", chan);
		return -2 ;
	}
    
	if ((chan & 4) == 0) {
		outb(DMA1_SMSK, (chan & 3) | 4 /* disable mask */);
	} else {
		outb(DMA2_SMSK, (chan & 3) | 4 /* disable mask */);
	}
	return(isa_dmastatus(chan));
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

struct isa_device *
find_isadev(table, driverp, unit)
	struct isa_device *table;
	struct isa_driver *driverp;
	int unit;
{
	if (driverp == NULL) /* sanity check */
		return (NULL);

	while ((table->id_driver != driverp) || (table->id_unit != unit)) {
		if (table->id_driver == 0)
			return NULL;

		table++;
	}

	return (table);
}
