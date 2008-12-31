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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/pc98/cbus/cbus_dma.c,v 1.23.18.1 2008/11/25 02:59:29 kensmith Exp $");

/*
 * code to manage AT bus
 *
 * 92/08/18  Frank P. MacLachlan (fpm@crash.cts.com):
 * Fixed uninitialized variable problem and added code to deal
 * with DMA page boundaries in isa_dmarangecheck().  Fixed word
 * mode DMA count compution and reorganized DMA setup code in
 * isa_dmastart()
 */

#include "opt_pc98.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <machine/md_var.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <isa/isavar.h>
#include <pc98/cbus/cbus.h>
#include <pc98/cbus/cbus_dmareg.h>

static int isa_dmarangecheck(caddr_t va, u_int length, int chan);

static caddr_t	dma_bouncebuf[4];
static u_int	dma_bouncebufsize[4];
static u_int8_t	dma_bounced = 0;
static u_int8_t	dma_busy = 0;		/* Used in isa_dmastart() */
static u_int8_t	dma_inuse = 0;		/* User for acquire/release */
static u_int8_t dma_auto_mode = 0;

#define VALID_DMA_MASK (3)

/* high byte of address is stored in this port for i-th dma channel */
static int dmapageport[4] = { 0x27, 0x21, 0x23, 0x25 };

/*
 * Setup a DMA channel's bounce buffer.
 */
int
isa_dma_init(int chan, u_int bouncebufsize, int flag)
{
	void *buf;

#ifdef DIAGNOSTIC
	if (chan & ~VALID_DMA_MASK)
		panic("isa_dma_init: channel out of range");
	if (dma_bouncebuf[chan] != NULL)
		panic("isa_dma_init: impossible request"); 
#endif

	dma_bouncebufsize[chan] = bouncebufsize;

	/* Try malloc() first.  It works better if it works. */
	buf = malloc(bouncebufsize, M_DEVBUF, flag);
	if (buf != NULL) {
		if (isa_dmarangecheck(buf, bouncebufsize, chan) == 0) {
			dma_bouncebuf[chan] = buf;
			return (0);
		}
		free(buf, M_DEVBUF);
	}
	buf = contigmalloc(bouncebufsize, M_DEVBUF, flag, 0ul, 0xfffffful,
			   1ul, chan & 4 ? 0x20000ul : 0x10000ul);
	if (buf == NULL)
		return (ENOMEM);
	dma_bouncebuf[chan] = buf;
	return (0);
}

/*
 * Register a DMA channel's usage.  Usually called from a device driver
 * in open() or during its initialization.
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
 * during close() or during its shutdown.
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
 * isa_dmastart(): program 8237 DMA controller channel, avoid page alignment
 * problems by using a bounce buffer.
 */
void
isa_dmastart(int flags, caddr_t addr, u_int nbytes, int chan)
{
	vm_paddr_t phys;
	int waport;
	caddr_t newaddr;

	GIANT_REQUIRED;

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
		if (!(flags & ISADMA_READ))
			bcopy(addr, newaddr, nbytes);
		addr = newaddr;
	}

	/* translate to physical */
	phys = pmap_extract(kernel_pmap, (vm_offset_t)addr);

	if (flags & ISADMA_RAW) {
	    dma_auto_mode |= (1 << chan);
	} else { 
	    dma_auto_mode &= ~(1 << chan);
	}

	if (need_pre_dma_flush)
		wbinvd();		/* wbinvd (WB cache flush) */

	/* set dma channel mode, and reset address ff */

	/* If ISADMA_RAW flag is set, then use autoinitialise mode */
	if (flags & ISADMA_RAW) {
		if (flags & ISADMA_READ)
			outb(DMA1_MODE, DMA37MD_AUTO|DMA37MD_WRITE|chan);
		else
			outb(DMA1_MODE, DMA37MD_AUTO|DMA37MD_READ|chan);
	} else {
		if (flags & ISADMA_READ)
			outb(DMA1_MODE, DMA37MD_SINGLE|DMA37MD_WRITE|chan);
		else
			outb(DMA1_MODE, DMA37MD_SINGLE|DMA37MD_READ|chan);
	}
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
}

void
isa_dmadone(int flags, caddr_t addr, int nbytes, int chan)
{  

	if (flags & ISADMA_READ) {
		/* cache flush only after reading 92/12/9 by A.Kojima */
		if (need_post_dma_flush)
			invd();
	}

#ifdef DIAGNOSTIC
	if (chan & ~VALID_DMA_MASK)
		panic("isa_dmadone: channel out of range");

	if ((dma_inuse & (1 << chan)) == 0)
		printf("isa_dmadone: channel %d not acquired\n", chan);
#endif

	if (((dma_busy & (1 << chan)) == 0) && 
	    (dma_auto_mode & (1 << chan)) == 0 )
		printf("isa_dmadone: channel %d not busy\n", chan);

	if ((dma_auto_mode & (1 << chan)) == 0)
		outb(DMA1_SMSK, (chan & 3) | 4);

	if (dma_bounced & (1 << chan)) {
		/* copy bounce buffer on read */
		if (flags & ISADMA_READ)
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
	vm_paddr_t phys, priorpage = 0;
	vm_offset_t endva;
	u_int dma_pgmsk = (chan & 4) ?  ~(128*1024-1) : ~(64*1024-1);

	GIANT_REQUIRED;

	endva = (vm_offset_t)round_page((vm_offset_t)va + length);
	for (; va < (caddr_t) endva ; va += PAGE_SIZE) {
		phys = trunc_page(pmap_extract(kernel_pmap, (vm_offset_t)va));
#ifdef EPSON_BOUNCEDMA
#define ISARAM_END	0xf00000
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
	ffport = DMA1_FFC;
	waport = DMA1_CHN(chan) + 2;

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
 * Reached terminal count yet ?
 */
int
isa_dmatc(int chan)
{

	return(inb(DMA1_STATUS) & (1 << chan));
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
    
	if ((chan & 4) == 0)
		outb(DMA1_SMSK, (chan & 3) | 4 /* disable mask */);

	return(isa_dmastatus(chan));
}

/*
 * Attach to the ISA PnP descriptor for the AT DMA controller
 */
static struct isa_pnp_id atdma_ids[] = {
	{ 0x0002d041 /* PNP0200 */, "AT DMA controller" },
	{ 0 }
};

static int
atdma_probe(device_t dev)
{
	int result;
	
	if ((result = ISA_PNP_PROBE(device_get_parent(dev), dev, atdma_ids)) <= 0)
		device_quiet(dev);
	return(result);
}

static int
atdma_attach(device_t dev)
{
	return(0);
}

static device_method_t atdma_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		atdma_probe),
	DEVMETHOD(device_attach,	atdma_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	{ 0, 0 }
};

static driver_t atdma_driver = {
	"atdma",
	atdma_methods,
	1,		/* no softc */
};

static devclass_t atdma_devclass;

DRIVER_MODULE(atdma, isa, atdma_driver, atdma_devclass, 0, 0);
