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
 *	$Id: isa.c,v 1.6 1993/11/07 21:47:19 wollman Exp $
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

#include "param.h"
#include "systm.h"
#include "conf.h"
#include "file.h"
#include "buf.h"
#include "uio.h"
#include "syslog.h"
#include "malloc.h"
#include "rlist.h"
#include "machine/segments.h"
#include "vm/vm.h"
#include "i386/isa/isa_device.h"
#include "i386/isa/isa.h"
#include "i386/isa/icu.h"
#include "i386/isa/ic/i8237.h"
#include "i386/isa/ic/i8042.h"

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

int config_isadev __P((struct isa_device *, u_int *));

/*
 * print a conflict message
 */
void
conflict(dvp, tmpdvp, item, reason, format)
	struct isa_device	*dvp, *tmpdvp;
	int			item;
	char			*reason;
	char			*format;
{
	printf("%s%d not probed due to %s conflict with %s%d at ",
		dvp->id_driver->name, dvp->id_unit, reason,
		tmpdvp->id_driver->name, tmpdvp->id_unit);
	printf(format, item);
	printf("\n");
}

/*
 * Check to see if things are alread in use, like IRQ's, I/O addresses
 * and Memory addresses.
 */
int
haveseen(dvp, tmpdvp)
	struct	isa_device *dvp, *tmpdvp;
{
	int	status = 0;

	/*
	 * Only check against devices that have already been found
	 */
	if (tmpdvp->id_alive) {
		/*
		 * Check for I/O address conflict.  We can only check the
		 * starting address of the device against the range of the
		 * device that has already been probed since we do not
		 * know how many I/O addresses this device uses.
		 */
		if (tmpdvp->id_alive != -1) {
			if ((dvp->id_iobase >= tmpdvp->id_iobase) &&
			    (dvp->id_iobase <=
				  (tmpdvp->id_iobase + tmpdvp->id_alive - 1))) {
				conflict(dvp, tmpdvp, dvp->id_iobase,
					 "I/O address", "0x%x");
				status = 1;
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
		if(tmpdvp->id_maddr) {
			if((KERNBASE + dvp->id_maddr >= tmpdvp->id_maddr) &&
			   (KERNBASE + dvp->id_maddr <=
			   (tmpdvp->id_maddr + tmpdvp->id_msize - 1))) {
				conflict(dvp, tmpdvp, dvp->id_maddr, "maddr",
					"0x%x");
				status = 1;
			}
		}
#ifndef COM_MULTIPORT
		/*
		 * Check for IRQ conflicts.
		 */
		if(tmpdvp->id_irq) {
			if (tmpdvp->id_irq == dvp->id_irq) {
				conflict(dvp, tmpdvp, ffs(dvp->id_irq) - 1,
					"irq", "%d");
				status = 1;
			}
		}
#endif
		/*
		 * Check for DRQ conflicts.
		 */
		if(tmpdvp->id_drq != -1) {
			if (tmpdvp->id_drq == dvp->id_drq) {
				conflict(dvp, tmpdvp, dvp->id_drq,
					"drq", "%d");
				status = 1;
			}
		}
	}
	return (status);
}

/*
 * Search through all the isa_devtab_* tables looking for anything that
 * conflicts with the current device.
 */
int
haveseen_isadev(dvp)
	struct isa_device *dvp;
{
	struct isa_device *tmpdvp;
	int	status = 0;

	for (tmpdvp = isa_devtab_tty; tmpdvp->id_driver; tmpdvp++) {
		status |= haveseen(dvp, tmpdvp);
	}
	for (tmpdvp = isa_devtab_bio; tmpdvp->id_driver; tmpdvp++) {
		status |= haveseen(dvp, tmpdvp);
	}
	for (tmpdvp = isa_devtab_net; tmpdvp->id_driver; tmpdvp++) {
		status |= haveseen(dvp, tmpdvp);
	}
	for (tmpdvp = isa_devtab_null; tmpdvp->id_driver; tmpdvp++) {
		status |= haveseen(dvp, tmpdvp);
	}
	return(status);
}

/*
 * Configure all ISA devices
 */
void
isa_configure() {
	struct isa_device *dvp;

	enable_intr();
	splhigh();
	INTREN(IRQ_SLAVE);
	printf("Probing for devices on the ISA bus:\n");
	for (dvp = isa_devtab_tty; dvp->id_driver; dvp++) {
		if (!haveseen_isadev(dvp))
			config_isadev(dvp,&ttymask);
	}
	for (dvp = isa_devtab_bio; dvp->id_driver; dvp++) {
		if (!haveseen_isadev(dvp))
			config_isadev(dvp,&biomask);
	}
	for (dvp = isa_devtab_net; dvp->id_driver; dvp++) {
		if (!haveseen_isadev(dvp))
			config_isadev(dvp,&netmask);
	}
	for (dvp = isa_devtab_null; dvp->id_driver; dvp++) {
		if (!haveseen_isadev(dvp))
			config_isadev(dvp,(u_int *) NULL);
	}
/*
 * XXX We should really add the tty device to netmask when the line is
 * switched to SLIPDISC, and then remove it when it is switched away from
 * SLIPDISC.  No need to block out ALL ttys during a splnet when only one
 * of them is running slip.
 */
#include "sl.h"
#if NSL > 0
	netmask |= ttymask;
	ttymask |= netmask;
#endif
	/* biomask |= ttymask ;  can some tty devices use buffers? */
	printf("biomask %x ttymask %x netmask %x\n", biomask, ttymask, netmask);
	splnone();
}

/*
 * Configure an ISA device.
 */
config_isadev(isdp, mp)
	struct isa_device *isdp;
	u_int *mp;
{
	struct isa_driver *dp = isdp->id_driver;
 
	if (isdp->id_maddr) {
		extern u_int atdevbase;

		isdp->id_maddr -= 0xa0000; /* XXX should be a define */
		isdp->id_maddr += atdevbase;
	}
	isdp->id_alive = (*dp->probe)(isdp);
	if (isdp->id_alive) {
		/*
		 * Only print the I/O address range if id_alive != -1
		 * Right now this is a temporary fix just for the new
		 * NPX code so that if it finds a 486 that can use trap
		 * 16 it will not report I/O addresses.
		 * Rod Grimes 04/26/94
		 */
		printf("%s%d", dp->name, isdp->id_unit);
		if (isdp->id_alive != -1) {
 			printf(" at 0x%x", isdp->id_iobase);
 			if ((isdp->id_iobase + isdp->id_alive - 1) !=
 			     isdp->id_iobase) {
 				printf("-0x%x",
				       isdp->id_iobase +
				       isdp->id_alive - 1);
			}
		}
		if(isdp->id_irq)
			printf(" irq %d", ffs(isdp->id_irq) - 1);
		if (isdp->id_drq != -1)
			printf(" drq %d", isdp->id_drq);
		if (isdp->id_maddr)
			printf(" maddr 0x%x", kvtop(isdp->id_maddr));
		if (isdp->id_msize)
			printf(" msize %d", isdp->id_msize);
		if (isdp->id_flags)
			printf(" flags 0x%x", isdp->id_flags);
		if (isdp->id_iobase < 0x100)
			printf(" on motherboard\n");
		else
			printf(" on isa\n");

		(*dp->attach)(isdp);

		if(isdp->id_irq) {
			int intrno;

			intrno = ffs(isdp->id_irq)-1;
			setidt(ICU_OFFSET+intrno, isdp->id_intr,
				 SDT_SYS386IGT, SEL_KPL);
			if(mp) {
				INTRMASK(*mp,isdp->id_irq);
			}
			INTREN(isdp->id_irq);
		}
	} else {
		printf("%s%d not found", dp->name, isdp->id_unit);
		if (isdp->id_iobase) {
			printf(" at 0x%x", isdp->id_iobase);
		}
		printf("\n");
	}
}

#define	IDTVEC(name)	__CONCAT(X,name)
/* default interrupt vector table entries */
extern	IDTVEC(intr0), IDTVEC(intr1), IDTVEC(intr2), IDTVEC(intr3),
	IDTVEC(intr4), IDTVEC(intr5), IDTVEC(intr6), IDTVEC(intr7),
	IDTVEC(intr8), IDTVEC(intr9), IDTVEC(intr10), IDTVEC(intr11),
	IDTVEC(intr12), IDTVEC(intr13), IDTVEC(intr14), IDTVEC(intr15);

static *defvec[16] = {
	&IDTVEC(intr0), &IDTVEC(intr1), &IDTVEC(intr2), &IDTVEC(intr3),
	&IDTVEC(intr4), &IDTVEC(intr5), &IDTVEC(intr6), &IDTVEC(intr7),
	&IDTVEC(intr8), &IDTVEC(intr9), &IDTVEC(intr10), &IDTVEC(intr11),
	&IDTVEC(intr12), &IDTVEC(intr13), &IDTVEC(intr14), &IDTVEC(intr15) };

/* out of range default interrupt vector gate entry */
extern	IDTVEC(intrdefault);

/*
 * Fill in default interrupt table (in case of spuruious interrupt
 * during configuration of kernel, setup interrupt control unit
 */
isa_defaultirq() {
	int i;

	/* icu vectors */
	for (i = NRSVIDT ; i < NRSVIDT+ICU_LEN ; i++)
		setidt(i, defvec[i],  SDT_SYS386IGT, SEL_KPL);
  
	/* out of range vectors */
	for (i = NRSVIDT; i < NIDT; i++)
		setidt(i, &IDTVEC(intrdefault), SDT_SYS386IGT, SEL_KPL);

	/* initialize 8259's */
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
}

/* region of physical memory known to be contiguous */
vm_offset_t isaphysmem;
static caddr_t dma_bounce[8];		/* XXX */
static char bounced[8];		/* XXX */
#define MAXDMASZ 512		/* XXX */

/* high byte of address is stored in this port for i-th dma channel */
static short dmapageport[8] =
	{ 0x87, 0x83, 0x81, 0x82, 0x8f, 0x8b, 0x89, 0x8a };

/*
 * isa_dmacascade(): program 8237 DMA controller channel to accept
 * external dma control by a board.
 */
void isa_dmacascade(unsigned chan)
{
	if (chan > 7)
		panic("isa_dmacascade: impossible request"); 

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
void isa_dmastart(int flags, caddr_t addr, unsigned nbytes, unsigned chan)
{	vm_offset_t phys;
	int waport;
	caddr_t newaddr;

	if (    chan > 7
	    || (chan < 4 && nbytes > (1<<16))
	    || (chan >= 4 && (nbytes > (1<<17) || (u_int)addr & 1)))
		panic("isa_dmastart: impossible request"); 

	if (isa_dmarangecheck(addr, nbytes, chan)) {
		if (dma_bounce[chan] == 0)
			dma_bounce[chan] =
				/*(caddr_t)malloc(MAXDMASZ, M_TEMP, M_WAITOK);*/
				(caddr_t) isaphysmem + NBPG*chan;
		bounced[chan] = 1;
		newaddr = dma_bounce[chan];
		*(int *) newaddr = 0;	/* XXX */

		/* copy bounce buffer on write */
		if (!(flags & B_READ))
			bcopy(addr, newaddr, nbytes);
		addr = newaddr;
	}

	/* translate to physical */
	phys = pmap_extract(pmap_kernel(), (vm_offset_t)addr);

	if ((chan & 4) == 0) {
		/*
		 * Program one of DMA channels 0..3.  These are
		 * byte mode channels.
		 */
		/* set dma channel mode, and reset address ff */
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

void isa_dmadone(int flags, caddr_t addr, int nbytes, int chan)
{

	/* copy bounce buffer on read */
	/*if ((flags & (B_PHYS|B_READ)) == (B_PHYS|B_READ))*/
	if (bounced[chan]) {
		bcopy(dma_bounce[chan], addr, nbytes);
		bounced[chan] = 0;
	}
}

/*
 * Check for problems with the address range of a DMA transfer
 * (non-contiguous physical pages, outside of bus address space,
 * crossing DMA page boundaries).
 * Return true if special handling needed.
 */

isa_dmarangecheck(caddr_t va, unsigned length, unsigned chan) {
	vm_offset_t phys, priorpage = 0, endva;
	u_int dma_pgmsk = (chan & 4) ?  ~(128*1024-1) : ~(64*1024-1);

	endva = (vm_offset_t)round_page(va + length);
	for (; va < (caddr_t) endva ; va += NBPG) {
		phys = trunc_page(pmap_extract(pmap_kernel(), (vm_offset_t)va));
#define ISARAM_END	RAM_END
		if (phys == 0)
			panic("isa_dmacheck: no physical page present");
		if (phys > ISARAM_END) 
			return (1);
		if (priorpage) {
			if (priorpage + NBPG != phys)
				return (1);
			/* check if crossing a DMA page boundary */
			if (((u_int)priorpage ^ (u_int)phys) & dma_pgmsk)
				return (1);
		}
		priorpage = phys;
	}
	return (0);
}

/* head of queue waiting for physmem to become available */
struct buf isa_physmemq;

/* blocked waiting for resource to become free for exclusive use */
static isaphysmemflag;
/* if waited for and call requested when free (B_CALL) */
static void (*isaphysmemunblock)(); /* needs to be a list */

/*
 * Allocate contiguous physical memory for transfer, returning
 * a *virtual* address to region. May block waiting for resource.
 * (assumed to be called at splbio())
 */
caddr_t
isa_allocphysmem(caddr_t va, unsigned length, void (*func)()) {
	
	isaphysmemunblock = func;
	while (isaphysmemflag & B_BUSY) {
		isaphysmemflag |= B_WANTED;
		tsleep(&isaphysmemflag, PRIBIO, "isaphys", 0);
	}
	isaphysmemflag |= B_BUSY;

	return((caddr_t)isaphysmem);
}

/*
 * Free contiguous physical memory used for transfer.
 * (assumed to be called at splbio())
 */
void
isa_freephysmem(caddr_t va, unsigned length) {

	isaphysmemflag &= ~B_BUSY;
	if (isaphysmemflag & B_WANTED) {
		isaphysmemflag &= B_WANTED;
		wakeup(&isaphysmemflag);
		if (isaphysmemunblock)
			(*isaphysmemunblock)();
	}
}
	
/*
 * Handle a NMI, possibly a machine check.
 * return true to panic system, false to ignore.
 */
isa_nmi(cd) {

	log(LOG_CRIT, "\nNMI port 61 %x, port 70 %x\n", inb(0x61), inb(0x70));
	return(0);
}

/*
 * Caught a stray interrupt, notify
 */
isa_strayintr(d) {

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
	extern u_long intrcnt_stray;

	intrcnt_stray++;
	if (intrcnt_stray <= 5)
		log(LOG_ERR,"ISA strayintr %x\n", d);
	if (intrcnt_stray == 5)
		log(LOG_CRIT,"Too many ISA strayintr not logging any more\n");
}

/*
 * Wait "n" microseconds.
 * Relies on timer 1 counting down from (TIMER_FREQ / hz) at
 * (1 * TIMER_FREQ) Hz.
 * Note: timer had better have been programmed before this is first used!
 * (The standard programming causes the timer to generate a square wave and
 * the counter is decremented twice every cycle.)
 */
#define	CF		(1 * TIMER_FREQ)
#define	TIMER_FREQ	1193182	/* XXX - should be elsewhere */

extern int hz;			/* XXX - should be elsewhere */

int DELAY(n)
	int n;
{
	int counter_limit;
	int prev_tick;
	int tick;
	int ticks_left;
	int sec;
	int usec;

#ifdef DELAYDEBUG
	int getit_calls = 1;
	int n1;
	static int state = 0;

	if (state == 0) {
		state = 1;
		for (n1 = 1; n1 <= 10000000; n1 *= 10)
			DELAY(n1);
		state = 2;
	}
	if (state == 1)
		printf("DELAY(%d)...", n);
#endif

	/*
	 * Read the counter first, so that the rest of the setup overhead is
	 * counted.  Guess the initial overhead is 20 usec (on most systems it
	 * takes about 1.5 usec for each of the i/o's in getit().  The loop
	 * takes about 6 usec on a 486/33 and 13 usec on a 386/20.  The
	 * multiplications and divisions to scale the count take a while).
	 */
	prev_tick = getit(0, 0);
	n -= 20;

	/*
	 * Calculate (n * (CF / 1e6)) without using floating point and without
	 * any avoidable overflows.
	 */
	sec = n / 1000000;
	usec = n - sec * 1000000;
	ticks_left = sec * CF
		     + usec * (CF / 1000000)
		     + usec * ((CF % 1000000) / 1000) / 1000
		     + usec * (CF % 1000) / 1000000;

	counter_limit = TIMER_FREQ / hz;
	while (ticks_left > 0) {
		tick = getit(0, 0);
#ifdef DELAYDEBUG
		++getit_calls;
#endif
		if (tick > prev_tick)
			ticks_left -= prev_tick - (tick - counter_limit);
		else
			ticks_left -= prev_tick - tick;
		prev_tick = tick;
	}
#ifdef DELAYDEBUG
	if (state == 1)
		printf(" %d calls to getit() at %d usec each\n",
		       getit_calls, (n + 5) / getit_calls);
#endif
}

getit(unit, timer) {
	int high;
	int low;

	/*
	 * XXX - isa.h defines bogus timers.  There's no such timer as
	 * IO_TIMER_2 = 0x48.  There's a timer in the CMOS RAM chip but
	 * its interface is quite different.  Neither timer is an 8252.
	 * We actually only call this with unit = 0 and timer = 0.  It
	 * could be static...
	 */
	/*
	 * Protect ourself against interrupts.
	 * XXX - sysbeep() and sysbeepstop() need protection.
	 */
	disable_intr();
	/*
	 * Latch the count for 'timer' (cc00xxxx, c = counter, x = any).
	 */
	outb(IO_TIMER1 + 3, timer << 6);

	low = inb(IO_TIMER1 + timer);
	high = inb(IO_TIMER1 + timer);
	enable_intr();
	return ((high << 8) | low);
}

static beeping;
static
sysbeepstop(f)
{
	/* disable counter 2 */
	outb(0x61, inb(0x61) & 0xFC);
	if (f)
		timeout(sysbeepstop, 0, f);
	else
		beeping = 0;
}

void sysbeep(int pitch, int period)
{

	outb(0x61, inb(0x61) | 3);	/* enable counter 2 */
	/*
	 * XXX - move timer stuff to clock.c.
	 * Program counter 2:
	 * ccaammmb, c counter, a = access, m = mode, b = BCD
	 * 1011x110, 11 for aa = LSB then MSB, x11 for mmm = square wave.
	 */
	outb(0x43, 0xb6);	/* set command for counter 2, 2 byte write */
	
	outb(0x42, pitch);
	outb(0x42, (pitch>>8));
	
	if (!beeping) {
		beeping = period;
		timeout(sysbeepstop, period/2, period);
	}
}

/*
 * Pass command to keyboard controller (8042)
 */
unsigned kbc_8042cmd(val) {
	
	while (inb(KBSTATP)&KBS_IBF);
	if (val) outb(KBCMDP, val);
	while (inb(KBSTATP)&KBS_IBF);
	return (inb(KBDATAP));
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

	id_irq = (unsigned short) dvp->id_irq;	/* XXX silly type in struct */
	if (id_irq & 0xff)
		return (inb(IO_ICU1) & id_irq);
	return (inb(IO_ICU2) & (id_irq >> 8));
}
