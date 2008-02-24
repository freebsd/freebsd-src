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
 *	from: isa_dma.c,v 1.3 1999/05/09 23:56:00 peter Exp $
 * $FreeBSD: src/sys/ia64/isa/isa_dma.c,v 1.10 2007/07/09 04:58:16 marcel Exp $
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
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <isa/isareg.h>
#include <isa/isavar.h>
#include <isa/isa_dmareg.h>
#include <machine/bus.h>

static bus_dma_tag_t dma_tag[8];
static bus_dmamap_t dma_map[8];
static u_int8_t	dma_busy = 0;		/* Used in isa_dmastart() */
static u_int8_t	dma_inuse = 0;		/* User for acquire/release */
static u_int8_t dma_auto_mode = 0;
static u_int8_t dma_bounced = 0;

#define VALID_DMA_MASK (7)

/* high byte of address is stored in this port for i-th dma channel */
static int dmapageport[8] = { 0x87, 0x83, 0x81, 0x82, 0x8f, 0x8b, 0x89, 0x8a };

/*
 * Setup a DMA channel's bounce buffer.
 */
int
isa_dma_init(int chan, u_int bouncebufsize, int flag __unused)
{
	static int initted = 0;
	bus_addr_t boundary = chan >= 4 ? 0x20000 : 0x10000;

	if (!initted) {
		/*
		 * Reset the DMA hardware.
		 */
		outb(DMA1_RESET, 0);
		outb(DMA2_RESET, 0);
		isa_dmacascade(4);
	    
		initted = 1;
	}

#ifdef DIAGNOSTIC
	if (chan & ~VALID_DMA_MASK)
		panic("isa_dma_init: channel out of range");

	if (dma_tag[chan] || dma_map[chan])
		panic("isa_dma_init: impossible request"); 
#endif

	if (bus_dma_tag_create(/*parent*/NULL,
			       /*alignment*/2,
			       /*boundary*/boundary,
			       /*lowaddr*/BUS_SPACE_MAXADDR_24BIT,
			       /*highaddr*/BUS_SPACE_MAXADDR,
			       /*filter*/NULL, /*filterarg*/NULL,
			       /*maxsize*/bouncebufsize,
			       /*nsegments*/1, /*maxsegz*/0x3ffff,
			       /*flags*/BUS_DMA_ISA,
			       /*lockfunc*/busdma_lock_mutex,
			       /*lockarg*/&Giant,
			       &dma_tag[chan]) != 0) {
		panic("isa_dma_init: unable to create dma tag\n");
	}
	
	if (bus_dmamap_create(dma_tag[chan], 0, &dma_map[chan])) {
		panic("isa_dma_init: unable to create dma map\n");
	}

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
 * isa_dmastart(): program 8237 DMA controller channel.
 */

struct isa_dmastart_arg {
	caddr_t addr;
	int 	chan;
	int 	flags;
};

static void isa_dmastart_cb(void *arg, bus_dma_segment_t *segs, int nseg,
			    int error)
{
#if 0
	caddr_t addr = ((struct isa_dmastart_arg *) arg)->addr;
#endif
	int chan = ((struct isa_dmastart_arg *) arg)->chan;
	int flags = ((struct isa_dmastart_arg *) arg)->flags;
	bus_addr_t phys = segs->ds_addr;
	int nbytes = segs->ds_len;
	int waport;

	if (nseg != 1)
		panic("isa_dmastart: transfer mapping not contiguous");

#if 0
	if ((chipset.sgmap == NULL) && 
	    (pmap_extract(kernel_pmap, (vm_offset_t)addr)
		> BUS_SPACE_MAXADDR_24BIT)) { 
		/* we bounced */
		dma_bounced |= (1 << chan);
                /* copy bounce buffer on write */
                if (!(flags & ISADMA_READ)) 
                        bus_dmamap_sync(dma_tag[chan], dma_map[chan], 
			                  BUS_DMASYNC_PREWRITE);
	}
#endif
	
	if ((chan & 4) == 0) {
		/*
		 * Program one of DMA channels 0..3.  These are
		 * byte mode channels.
		 */
		/* set dma channel mode, and reset address ff */

		/* If ISADMA_RAW flag is set, then use autoinitialise mode */
		if (flags & ISADMA_RAW) {
		  if (flags & ISADMA_READ)
			outb(DMA1_MODE, DMA37MD_AUTO|DMA37MD_WRITE|chan);
		  else
			outb(DMA1_MODE, DMA37MD_AUTO|DMA37MD_READ|chan);
		}
		else
		if (flags & ISADMA_READ)
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

		/* If ISADMA_RAW flag is set, then use autoinitialise mode */
		if (flags & ISADMA_RAW) {
		  if (flags & ISADMA_READ)
			outb(DMA2_MODE, DMA37MD_AUTO|DMA37MD_WRITE|(chan&3));
		  else
			outb(DMA2_MODE, DMA37MD_AUTO|DMA37MD_READ|(chan&3));
		}
		else
		if (flags & ISADMA_READ)
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
isa_dmastart(int flags, caddr_t addr, u_int nbytes, int chan)
{
	struct isa_dmastart_arg args;

#ifdef DIAGNOSTIC
	if (chan & ~VALID_DMA_MASK)
		panic("isa_dmastart: channel out of range");

	if ((chan < 4 && nbytes > (1<<16))
	    || (chan >= 4 && (nbytes > (1<<17) || (uintptr_t)addr & 1)))
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

	if (!dma_tag[chan] || !dma_map[chan])
		panic("isa_dmastart: called without isa_dma_init");

	dma_busy |= (1 << chan);

	if (flags & ISADMA_RAW) {
		dma_auto_mode |= (1 << chan);
	} else { 
		dma_auto_mode &= ~(1 << chan);
	}

	/*
	 * Freeze dma while updating registers.
	 */
	outb(chan & 4 ? DMA2_SMSK : DMA1_SMSK, (chan & 3) | 4);

        args.addr = addr;
	args.chan = chan;
	args.flags = flags;
	bus_dmamap_load(dma_tag[chan], dma_map[chan], addr, nbytes,
			isa_dmastart_cb, &args, 0);
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
		if (flags & ISADMA_READ) {
			bus_dmamap_sync(dma_tag[chan], dma_map[chan],
			                  BUS_DMASYNC_POSTREAD);
		}
		dma_bounced &= ~(1 << chan);
	}

	if ((dma_auto_mode & (1 << chan)) == 0) {
		outb(chan & 4 ? DMA2_SMSK : DMA1_SMSK, (chan & 3) | 4);
		bus_dmamap_unload(dma_tag[chan], dma_map[chan]);
	}

	dma_busy &= ~(1 << chan);
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
	int s;

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

	s = splhigh();			/* no interrupts Mr Jones! */
	outb(ffport, 0);		/* clear register LSB flipflop */
	low1 = inb(waport);
	high1 = inb(waport);
	outb(ffport, 0);		/* clear again */
	low2 = inb(waport);
	high2 = inb(waport);
	splx(s);			/* enable interrupts again */

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

	if (chan < 4)
		return(inb(DMA1_STATUS) & (1 << chan));
	else
		return(inb(DMA2_STATUS) & (1 << (chan & 3)));
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
