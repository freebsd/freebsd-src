/*
 *  linux/arch/arm/mach-sa1100/dma-sa1111.c
 *
 *  Copyright (C) 2000 John Dorsey
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  4 September 2000 - created.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/errno.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/hardware/sa1111.h>

// #define DEBUG
#ifdef DEBUG
#define DPRINTK( s, arg... )  printk( "dma<%s>: " s, dma->device_id , ##arg )
#else
#define DPRINTK( x... )
#endif


/*
 * Control register structure for the SA1111 SAC DMA
 */

typedef struct {
	volatile u_long SAD_CS;
	volatile dma_addr_t SAD_SA;
	volatile u_long SAD_CA;
	volatile dma_addr_t SAD_SB;
	volatile u_long SAD_CB;
} dma_regs_t;

#include "dma.h"


void sa1111_reset_sac_dma(dmach_t channel)
{
	sa1100_dma_t *dma = &dma_chan[channel];
	dma->regs->SAD_CS = 0;
	mdelay(1);
	dma->dma_a = dma->dma_b = 0;
}


int start_sa1111_sac_dma(sa1100_dma_t *dma, dma_addr_t dma_ptr, size_t size)
{
  	dma_regs_t *sac_regs = dma->regs;

	DPRINTK(" SAC DMA %cCS %02x at %08x (%d)\n",
		(sac_regs==&SADTCS)?'T':'R', sac_regs->SAD_CS, dma_ptr, size);

	/* The minimum transfer length requirement has not yet been
	 * verified:
	 */
	if( size < SA1111_SAC_DMA_MIN_XFER )
	  printk(KERN_WARNING "Warning: SAC xfers below %u bytes may be buggy!"
		 " (%u bytes)\n", SA1111_SAC_DMA_MIN_XFER, size);

	if( dma->dma_a && dma->dma_b ){
	  	DPRINTK("  neither engine available! (A %d, B %d)\n",
			dma->dma_a, dma->dma_b);
	  	return -1;
	}

	if( sa1111_check_dma_bug(dma_ptr) )
	  	printk(KERN_WARNING "Warning: DMA address %08x is buggy!\n",
		       dma_ptr);

	if( (dma->last_dma || dma->dma_b) && dma->dma_a == 0 ){
	  	if( sac_regs->SAD_CS & SAD_CS_DBDB ){
		  	DPRINTK("  awaiting \"done B\" interrupt, not starting\n");
			return -1;
		}
		sac_regs->SAD_SA = SA1111_DMA_ADDR((u_int)dma_ptr);
		sac_regs->SAD_CA = size;
		sac_regs->SAD_CS = SAD_CS_DSTA | SAD_CS_DEN;
		++dma->dma_a;
		DPRINTK("  with A [%02lx %08lx %04lx]\n", sac_regs->SAD_CS,
			sac_regs->SAD_SA, sac_regs->SAD_CA);
	} else {
	  	if( sac_regs->SAD_CS & SAD_CS_DBDA ){
		  	DPRINTK("  awaiting \"done A\" interrupt, not starting\n");
			return -1;
		}
		sac_regs->SAD_SB = SA1111_DMA_ADDR((u_int)dma_ptr);
		sac_regs->SAD_CB = size;
		sac_regs->SAD_CS = SAD_CS_DSTB | SAD_CS_DEN;
		++dma->dma_b;
		DPRINTK("  with B [%02lx %08lx %04lx]\n", sac_regs->SAD_CS,
			sac_regs->SAD_SB, sac_regs->SAD_CB);
	}

	/* Additional delay to avoid DMA engine lockup during record: */
	if( sac_regs == (dma_regs_t*)&SADRCS )
	  	mdelay(1);	/* NP : wouuuh! ugly... */

	return 0;
}


static void sa1111_sac_dma_irq(int irq, void *dev_id, struct pt_regs *regs)
{
  	sa1100_dma_t *dma = (sa1100_dma_t *) dev_id;

	DPRINTK("irq %d, last DMA serviced was %c, CS %02x\n", irq,
		dma->last_dma?'B':'A', dma->regs->SAD_CS);

	/* Occasionally, one of the DMA engines (A or B) will
	 * lock up. We try to deal with this by quietly kicking
	 * the control register for the afflicted transfer
	 * direction.
	 *
	 * Note for the debugging-inclined: we currently aren't
	 * properly flushing the DMA engines during channel
	 * shutdown. A slight hiccup at the beginning of playback
	 * after a channel has been stopped can be heard as
	 * evidence of this. Programmatically, this shows up
	 * as either a locked engine, or a spurious interrupt. -jd
	 */

	if(irq==AUDXMTDMADONEA || irq==AUDRCVDMADONEA){

	  	if(dma->last_dma == 0){
		  	DPRINTK("DMA B has locked up!\n");
			dma->regs->SAD_CS = 0;
			mdelay(1);
			dma->dma_a = dma->dma_b = 0;
		} else {
		  	if(dma->dma_a == 0)
			  	DPRINTK("spurious SAC IRQ %d\n", irq);
			else {
			  	--dma->dma_a;

				/* Servicing the SAC DMA engines too quickly
				 * after they issue a DONE interrupt causes
				 * them to lock up.
				 */
				if(irq==AUDRCVDMADONEA || irq==AUDRCVDMADONEB)
				  	mdelay(1);
			}
		}

		dma->regs->SAD_CS = SAD_CS_DBDA | SAD_CS_DEN; /* w1c */
		dma->last_dma = 0;

	} else {

	  	if(dma->last_dma == 1){
		  	DPRINTK("DMA A has locked up!\n");
			dma->regs->SAD_CS = 0;
			mdelay(1);
			dma->dma_a = dma->dma_b = 0;
		} else {
		  	if(dma->dma_b == 0)
			  	DPRINTK("spurious SAC IRQ %d\n", irq);
			else {
			  	--dma->dma_b;

				/* See lock-up note above. */
				if(irq==AUDRCVDMADONEA || irq==AUDRCVDMADONEB)
				  	mdelay(1);
			}
		}

		dma->regs->SAD_CS = SAD_CS_DBDB | SAD_CS_DEN; /* w1c */
		dma->last_dma = 1;

	}

	/* NP: maybe this shouldn't be called in all cases? */
	sa1100_dma_done (dma);
}


int sa1111_sac_request_dma(dmach_t *channel, const char *device_id,
			   unsigned int direction)
{
	sa1100_dma_t *dma = NULL;
	int ch, irq, err;

	*channel = -1;		/* to be sure we catch the freeing of a misregistered channel */

	ch = SA1111_SAC_DMA_BASE + direction;

	if (!channel_is_sa1111_sac(ch)) {
	  	printk(KERN_ERR "%s: invalid SA-1111 SAC DMA channel (%d)\n",
		       device_id, ch);
		return -1;
	}

	dma = &dma_chan[ch];

	if (xchg(&dma->in_use, 1) == 1) {
	  	printk(KERN_ERR "%s: SA-1111 SAC DMA channel %d in use\n",
		       device_id, ch);
		return -EBUSY;
	}

	irq = AUDXMTDMADONEA + direction;
	err = request_irq(irq, sa1111_sac_dma_irq, SA_INTERRUPT,
			  device_id, (void *) dma);
	if (err) {
		printk(KERN_ERR
		       "%s: unable to request IRQ %d for DMA channel %d (A)\n",
		       device_id, irq, ch);
		dma->in_use = 0;
		return err;
	}

	irq = AUDXMTDMADONEB + direction;
	err = request_irq(irq, sa1111_sac_dma_irq, SA_INTERRUPT,
			  device_id, (void *) dma);
	if (err) {
		printk(KERN_ERR
		       "%s: unable to request IRQ %d for DMA channel %d (B)\n",
		       device_id, irq, ch);
		dma->in_use = 0;
		return err;
	}

	*channel = ch;
	dma->device_id = device_id;
	dma->callback = NULL;
	dma->spin_size = 0;

	return 0;
}


/* FIXME:  need to complete the three following functions */

int sa1111_dma_get_current(dmach_t channel, void **buf_id, dma_addr_t *addr)
{
	sa1100_dma_t *dma = &dma_chan[channel];
	int flags, ret;

	local_irq_save(flags);
	if (dma->curr && dma->spin_ref <= 0) {
		dma_buf_t *buf = dma->curr;
		if (buf_id)
			*buf_id = buf->id;
		/* not fully accurate but still... */
		*addr = buf->dma_ptr;
		ret = 0;
	} else {
		if (buf_id)
			*buf_id = NULL;
		*addr = 0;
		ret = -ENXIO;
	}
	local_irq_restore(flags);
	return ret;
}

int sa1111_dma_stop(dmach_t channel)
{
	return 0;
}

int sa1111_dma_resume(dmach_t channel)
{
	return 0;
}


void sa1111_cleanup_sac_dma(dmach_t channel)
{
	sa1100_dma_t *dma = &dma_chan[channel];
	free_irq(AUDXMTDMADONEA + (channel - SA1111_SAC_DMA_BASE), (void*) dma);
	free_irq(AUDXMTDMADONEB + (channel - SA1111_SAC_DMA_BASE), (void*) dma);
}


/* According to the "Intel StrongARM SA-1111 Microprocessor Companion
 * Chip Specification Update" (June 2000), erratum #7, there is a
 * significant bug in Serial Audio Controller DMA. If the SAC is
 * accessing a region of memory above 1MB relative to the bank base,
 * it is important that address bit 10 _NOT_ be asserted. Depending
 * on the configuration of the RAM, bit 10 may correspond to one
 * of several different (processor-relative) address bits.
 *
 * This routine only identifies whether or not a given DMA address
 * is susceptible to the bug.
 */
int sa1111_check_dma_bug(dma_addr_t addr){
	unsigned int physaddr=SA1111_DMA_ADDR((unsigned int)addr);

	/* Section 4.6 of the "Intel StrongARM SA-1111 Development Module
	 * User's Guide" mentions that jumpers R51 and R52 control the
	 * target of SA-1111 DMA (either SDRAM bank 0 on Assabet, or
	 * SDRAM bank 1 on Neponset). The default configuration selects
	 * Assabet, so any address in bank 1 is necessarily invalid.
	 */
	if((machine_is_assabet() || machine_is_pfs168() ||
            machine_is_graphicsmaster() || machine_is_adsagc()) && addr >= 0xc8000000)
	  	return -1;

	/* The bug only applies to buffers located more than one megabyte
	 * above the start of the target bank:
	 */
	if(physaddr<(1<<20))
	  	return 0;

	switch(FExtr(SBI_SMCR, SMCR_DRAC)){
	case 01: /* 10 row + bank address bits, A<20> must not be set */
	  	if(physaddr & (1<<20))
		  	return -1;
		break;
	case 02: /* 11 row + bank address bits, A<23> must not be set */
	  	if(physaddr & (1<<23))
		  	return -1;
		break;
	case 03: /* 12 row + bank address bits, A<24> must not be set */
	  	if(physaddr & (1<<24))
		  	return -1;
		break;
	case 04: /* 13 row + bank address bits, A<25> must not be set */
	  	if(physaddr & (1<<25))
		  	return -1;
		break;
	case 05: /* 14 row + bank address bits, A<20> must not be set */
	  	if(physaddr & (1<<20))
		  	return -1;
		break;
	case 06: /* 15 row + bank address bits, A<20> must not be set */
	  	if(physaddr & (1<<20))
		  	return -1;
		break;
	default:
	  	printk(KERN_ERR "%s(): invalid SMCR DRAC value 0%lo\n",
		       __FUNCTION__, FExtr(SBI_SMCR, SMCR_DRAC));
		return -1;
	}

	return 0;
}


EXPORT_SYMBOL(sa1111_sac_request_dma);
EXPORT_SYMBOL(sa1111_check_dma_bug);


static int __init sa1111_init_sac_dma(void)
{
	int channel = SA1111_SAC_DMA_BASE;
	dma_chan[channel++].regs = (dma_regs_t *) &SADTCS;
	dma_chan[channel++].regs = (dma_regs_t *) &SADRCS;
	return 0;
}

__initcall(sa1111_init_sac_dma);
