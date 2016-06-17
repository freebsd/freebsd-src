/*
 * arch/arm/kernel/dma-sa1100.c
 *
 * Support functions for the SA11x0 internal DMA channels.
 * (see also Documentation/arm/SA1100/DMA)
 *
 * Copyright (C) 2000 Nicolas Pitre
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/errno.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/mach/dma.h>


#undef DEBUG
#ifdef DEBUG
#define DPRINTK( s, arg... )  printk( "dma<%s>: " s, dma->device_id , ##arg )
#else
#define DPRINTK( x... )
#endif


/*
 * DMA control register structure
 */
typedef struct {
	volatile u_long DDAR;
	volatile u_long SetDCSR;
	volatile u_long ClrDCSR;
	volatile u_long RdDCSR;
	volatile dma_addr_t DBSA;
	volatile u_long DBTA;
	volatile dma_addr_t DBSB;
	volatile u_long DBTB;
} dma_regs_t;

#include "dma.h"

sa1100_dma_t dma_chan[MAX_SA1100_DMA_CHANNELS];

/*
 * Maximum physical DMA buffer size
 */
#define MAX_DMA_SIZE		0x1fff
#define MAX_DMA_ORDER		12


/*
 * DMA processing...
 */

static inline int start_sa1100_dma(sa1100_dma_t * dma, dma_addr_t dma_ptr, int size)
{
	dma_regs_t *regs = dma->regs;
	int status;

	status = regs->RdDCSR;

	/* If both DMA buffers are started, there's nothing else we can do. */
	if ((status & (DCSR_STRTA | DCSR_STRTB)) == (DCSR_STRTA | DCSR_STRTB)) {
		DPRINTK("start: st %#x busy\n", status);
		return -EBUSY;
	}

	if (((status & DCSR_BIU) && (status & DCSR_STRTB)) ||
	    (!(status & DCSR_BIU) && !(status & DCSR_STRTA))) {
		if (status & DCSR_DONEA) {
			/* give a chance for the interrupt to be processed */
			goto irq_pending;
		}
		regs->DBSA = dma_ptr;
		regs->DBTA = size;
		regs->SetDCSR = DCSR_STRTA | DCSR_IE | DCSR_RUN;
		DPRINTK("start a=%#x s=%d on A\n", dma_ptr, size);
	} else {
		if (status & DCSR_DONEB) {
			/* give a chance for the interrupt to be processed */
			goto irq_pending;
		}
		regs->DBSB = dma_ptr;
		regs->DBTB = size;
		regs->SetDCSR = DCSR_STRTB | DCSR_IE | DCSR_RUN;
		DPRINTK("start a=%#x s=%d on B\n", dma_ptr, size);
	}

	return 0;

irq_pending:
	return -EAGAIN;
}


static int start_dma(sa1100_dma_t *dma, dma_addr_t dma_ptr, int size)
{
	if (channel_is_sa1111_sac(dma - dma_chan))
		return start_sa1111_sac_dma(dma, dma_ptr, size);
	return start_sa1100_dma(dma, dma_ptr, size);
}


/* This must be called with IRQ disabled */
static void process_dma(sa1100_dma_t * dma)
{
	dma_buf_t *buf;
	int chunksize;

	for (;;) {
		buf = dma->tail;

		if (!buf || dma->stopped) {
			/* no more data available */
			DPRINTK("process: no more buf (dma %s)\n",
				dma->curr ? "active" : "inactive");
			/*
			 * Some devices may require DMA still sending data
			 * at any time for clock reference, etc.
			 * Note: if there is still a data buffer being
			 * processed then the ref count is negative.  This
			 * allows for the DMA termination to be accounted in
			 * the proper order.
			 */
			if (dma->spin_size && dma->spin_ref >= 0) {
				chunksize = dma->spin_size;
				if (chunksize > MAX_DMA_SIZE)
					chunksize = (1 << MAX_DMA_ORDER);
				while (start_dma(dma, dma->spin_addr, chunksize) == 0)
					dma->spin_ref++;
				if (dma->curr != NULL)
					dma->spin_ref = -dma->spin_ref;
			}
			break;
		}

		/*
		 * This improves latency if there are some active spinning
		 * buffers.  We kill them altogether.
		 */
		if (dma->spin_ref > 0) {
			if (channel_is_sa1111_sac(dma - dma_chan))
				sa1111_reset_sac_dma(dma - dma_chan);
			else
				dma->regs->ClrDCSR =
				    DCSR_STRTA|DCSR_STRTB|DCSR_DONEA|DCSR_DONEB;
			dma->spin_ref = 0;
		}

		/*
		 * Let's try to start DMA on the current buffer.
		 * If DMA is busy then we break here.
		 */
		chunksize = buf->size;
		if (chunksize > MAX_DMA_SIZE)
			chunksize = (1 << MAX_DMA_ORDER);
		DPRINTK("process: b=%#x s=%d\n", (int) buf->id, buf->size);
		if (start_dma(dma, buf->dma_ptr, chunksize) != 0)
			break;
		if (!dma->curr)
			dma->curr = buf;
		buf->ref++;
		buf->dma_ptr += chunksize;
		buf->size -= chunksize;
		if (buf->size == 0) {
			/* current buffer is done: move tail to the next one */
			dma->tail = buf->next;
			DPRINTK("process: next b=%#x\n", (int) dma->tail);
		}
	}
}


/* This must be called with IRQ disabled */
void sa1100_dma_done (sa1100_dma_t *dma)
{
	dma_buf_t *buf = dma->curr;

	if (dma->spin_ref > 0) {
		dma->spin_ref--;
	} else if (buf) {
		buf->ref--;
		if (buf->ref == 0 && buf->size == 0) {
			/*
			 * Current buffer is done.
			 * Move current reference to the next one and send
			 * the processed buffer to the callback function,
			 * then discard it.
			 */
			DPRINTK("IRQ: buf done\n");
			dma->curr = buf->next;
			dma->spin_ref = -dma->spin_ref;
			if (dma->head == buf)
				dma->head = NULL;
			if (dma->callback) {
				int size = buf->dma_ptr - buf->dma_start;
				dma->callback(buf->id, size);
			}
			kfree(buf);
		}
	}

	process_dma(dma);
}


static void dma_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	sa1100_dma_t *dma = (sa1100_dma_t *) dev_id;
	int status = dma->regs->RdDCSR;

	DPRINTK("IRQ: b=%#x st=%#x\n", (int) dma->curr->id, status);

	if (status & (DCSR_ERROR)) {
		printk(KERN_ERR "DMA on \"%s\" caused an error\n", dma->device_id);
		dma->regs->ClrDCSR = DCSR_ERROR;
	}

	dma->regs->ClrDCSR = status & (DCSR_DONEA | DCSR_DONEB);
	if (status & DCSR_DONEA)
		sa1100_dma_done (dma);
	if (status & DCSR_DONEB)
		sa1100_dma_done (dma);
}


/*
 * DMA interface functions
 */

static spinlock_t dma_list_lock;

int sa1100_request_dma (dmach_t * channel, const char *device_id,
			dma_device_t device)
{
	sa1100_dma_t *dma = NULL;
	dma_regs_t *regs;
	int i, err;

	*channel = -1;		/* to be sure we catch the freeing of a misregistered channel */

	err = 0;
	spin_lock(&dma_list_lock);
	for (i = 0; i < SA1100_DMA_CHANNELS; i++) {
		if (dma_chan[i].in_use) {
			if (dma_chan[i].device == device) {
				err = -EBUSY;
				break;
			}
		} else if (!dma) {
			dma = &dma_chan[i];
		}
	}
	if (!err) {
	       if (dma)
		       dma->in_use = 1;
	       else
		       err = -ENOSR;
	}
	spin_unlock(&dma_list_lock);
	if (err)
		return err;

	err = request_irq(dma->irq, dma_irq_handler, SA_INTERRUPT,
			  device_id, (void *) dma);
	if (err) {
		printk(KERN_ERR
		       "%s: unable to request IRQ %d for DMA channel\n",
		       device_id, dma->irq);
		return err;
	}

	*channel = dma - dma_chan;
	dma->device_id = device_id;
	dma->device = device;
	dma->callback = NULL;
	dma->spin_size = 0;

	regs = dma->regs;
	regs->ClrDCSR =
		(DCSR_DONEA | DCSR_DONEB | DCSR_STRTA | DCSR_STRTB |
		 DCSR_IE | DCSR_ERROR | DCSR_RUN);
	regs->DDAR = device;
	DPRINTK("requested\n");
	return 0;
}


int sa1100_dma_set_callback(dmach_t channel, dma_callback_t cb)
{
	sa1100_dma_t *dma = &dma_chan[channel];

	if ((unsigned)channel >= MAX_SA1100_DMA_CHANNELS || !dma->in_use)
		return -EINVAL;

	dma->callback = cb;
	DPRINTK("cb = %p\n", cb);
	return 0;
}


int sa1100_dma_set_spin(dmach_t channel, dma_addr_t addr, int size)
{
	sa1100_dma_t *dma = &dma_chan[channel];
	int flags;

	if ((unsigned)channel >= MAX_SA1100_DMA_CHANNELS || !dma->in_use)
		return -EINVAL;

	DPRINTK("set spin %d at %#x\n", size, addr);
	local_irq_save(flags);
	dma->spin_addr = addr;
	dma->spin_size = size;
	if (size)
		process_dma(dma);
	local_irq_restore(flags);
	return 0;
}


int sa1100_dma_queue_buffer(dmach_t channel, void *buf_id,
			    dma_addr_t data, int size)
{
	sa1100_dma_t *dma;
	dma_buf_t *buf;
	int flags;

	dma = &dma_chan[channel];
	if ((unsigned)channel >= MAX_SA1100_DMA_CHANNELS || !dma->in_use)
		return -EINVAL;

	buf = kmalloc(sizeof(*buf), GFP_ATOMIC);
	if (!buf)
		return -ENOMEM;

	buf->next = NULL;
	buf->ref = 0;
	buf->dma_ptr = buf->dma_start = data;
	buf->size = size;
	buf->id = buf_id;
	DPRINTK("queueing b=%#x a=%#x s=%d\n", (int) buf_id, data, size);

	local_irq_save(flags);
	if (dma->head)
		dma->head->next = buf;
	dma->head = buf;
	if (!dma->tail)
		dma->tail = buf;
	process_dma(dma);
	local_irq_restore(flags);

	return 0;
}


int sa1100_dma_get_current(dmach_t channel, void **buf_id, dma_addr_t *addr)
{
	sa1100_dma_t *dma = &dma_chan[channel];
	dma_regs_t *regs;
	int flags, ret;

	if ((unsigned)channel >= MAX_SA1100_DMA_CHANNELS || !dma->in_use)
		return -EINVAL;

	if (channel_is_sa1111_sac(channel))
		return sa1111_dma_get_current(channel, buf_id, addr);

	regs = dma->regs;
	local_irq_save(flags);
	if (dma->curr && dma->spin_ref <= 0) {
		dma_buf_t *buf = dma->curr;
		int status, using_bufa;

		status = regs->RdDCSR;
		/*
		 * If we got here, that's because there is, or recently was, a
		 * buffer being processed.  We must determine whether buffer
		 * A or B is active.  Two possibilities: either we are
		 * in the middle of a buffer, or the DMA controller just
		 * switched to the next toggle but the interrupt hasn't been
		 * serviced yet.  The former case is straight forward.  In
		 * the later case, we'll do like if DMA is just at the end
		 * of the previous toggle since all registers haven't been
		 * reset yet.  This goes around the edge case and since we're
		 * always a little behind anyways it shouldn't make a big
		 * difference.  If DMA has been stopped prior calling this
		 * then the position is always exact.
		 */
		using_bufa = ((!(status & DCSR_BIU) &&  (status & DCSR_STRTA)) ||
			      ( (status & DCSR_BIU) && !(status & DCSR_STRTB)));
		if (buf_id)
			*buf_id = buf->id;
		*addr = (using_bufa) ? regs->DBSA : regs->DBSB;
		/*
		 * Clamp funky pointers sometimes returned by the hardware
		 * on completed DMA transfers
		 */
		if (*addr < buf->dma_start ||
		    *addr > buf->dma_ptr)
			*addr = buf->dma_ptr;
		DPRINTK("curr_pos: b=%#x a=%#x\n", (int)dma->curr->id, *addr);
		ret = 0;
	} else if (dma->tail && dma->stopped) {
		dma_buf_t *buf = dma->tail;
		if (buf_id)
			*buf_id = buf->id;
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


int sa1100_dma_stop(dmach_t channel)
{
	sa1100_dma_t *dma = &dma_chan[channel];
	int flags;

	if (channel_is_sa1111_sac(channel))
		return sa1111_dma_stop(channel);

	if (dma->stopped)
		return 0;
	local_irq_save(flags);
	dma->stopped = 1;
	/*
	 * Stop DMA and tweak state variables so everything could restart
	 * from there when resume/wakeup occurs.
	 */
	dma->regs->ClrDCSR = DCSR_RUN | DCSR_IE;
	if (dma->curr) {
		dma_buf_t *buf = dma->curr;
		if (dma->spin_ref <= 0) {
			dma_addr_t curpos;
			sa1100_dma_get_current(channel, NULL, &curpos);
			buf->size += buf->dma_ptr - curpos;
			buf->dma_ptr = curpos;
		}
		buf->ref = 0;
		dma->tail = buf;
		dma->curr = NULL;
	}
	dma->spin_ref = 0;
	dma->regs->ClrDCSR = DCSR_STRTA|DCSR_STRTB|DCSR_DONEA|DCSR_DONEB;
	process_dma(dma);
	local_irq_restore(flags);
	return 0;
}


int sa1100_dma_resume(dmach_t channel)
{
	sa1100_dma_t *dma = &dma_chan[channel];

	if ((unsigned)channel >= MAX_SA1100_DMA_CHANNELS || !dma->in_use)
		return -EINVAL;

	if (channel_is_sa1111_sac(channel))
		return sa1111_dma_resume(channel);

	if (dma->stopped) {
		unsigned long flags;
		local_irq_save(flags);
		dma->stopped = 0;
		process_dma(dma);
		local_irq_restore(flags);
	}
	return 0;
}


int sa1100_dma_flush_all(dmach_t channel)
{
	sa1100_dma_t *dma = &dma_chan[channel];
	dma_buf_t *buf, *next_buf;
	int flags;

	if ((unsigned)channel >= MAX_SA1100_DMA_CHANNELS || !dma->in_use)
		return -EINVAL;

	local_irq_save(flags);
	if (channel_is_sa1111_sac(channel))
		sa1111_reset_sac_dma(channel);
	else
		dma->regs->ClrDCSR = DCSR_STRTA|DCSR_STRTB|DCSR_DONEA|DCSR_DONEB|DCSR_RUN|DCSR_IE;
	buf = dma->curr;
	if (!buf)
		buf = dma->tail;
	dma->head = dma->tail = dma->curr = NULL;
	dma->stopped = 0;
	dma->spin_ref = 0;
	process_dma(dma);
	local_irq_restore(flags);
	while (buf) {
		next_buf = buf->next;
		kfree(buf);
		buf = next_buf;
	}
	DPRINTK("flushed\n");
	return 0;
}


void sa1100_free_dma(dmach_t channel)
{
	sa1100_dma_t *dma;

	if ((unsigned)channel >= MAX_SA1100_DMA_CHANNELS)
		return;

	dma = &dma_chan[channel];
	if (!dma->in_use) {
		printk(KERN_ERR "Trying to free free DMA%d\n", channel);
		return;
	}

	sa1100_dma_set_spin(channel, 0, 0);
	sa1100_dma_flush_all(channel);

	if (channel_is_sa1111_sac(channel)) {
		sa1111_cleanup_sac_dma(channel);
	} else {
		free_irq(IRQ_DMA0 + channel, (void *) dma);
	}
	dma->in_use = 0;

	DPRINTK("freed\n");
}


EXPORT_SYMBOL(sa1100_request_dma);
EXPORT_SYMBOL(sa1100_dma_set_callback);
EXPORT_SYMBOL(sa1100_dma_set_spin);
EXPORT_SYMBOL(sa1100_dma_queue_buffer);
EXPORT_SYMBOL(sa1100_dma_get_current);
EXPORT_SYMBOL(sa1100_dma_stop);
EXPORT_SYMBOL(sa1100_dma_resume);
EXPORT_SYMBOL(sa1100_dma_flush_all);
EXPORT_SYMBOL(sa1100_free_dma);


#ifdef CONFIG_PM
/* Drivers should call this from their PM callback function */

int sa1100_dma_sleep(dmach_t channel)
{
        sa1100_dma_t *dma = &dma_chan[channel];
	int orig_state;

	if ((unsigned)channel >= MAX_SA1100_DMA_CHANNELS || !dma->in_use)
		return -EINVAL;

	if (channel_is_sa1111_sac(channel)) {
		/* We'll cheat a little until someone actually
		 * write the real thing.
		 */
		sa1111_reset_sac_dma(channel);
		return 0;
	}

	orig_state = dma->stopped;
	sa1100_dma_stop(channel);
	dma->regs->ClrDCSR = DCSR_RUN | DCSR_IE | DCSR_STRTA | DCSR_STRTB;
	dma->stopped = orig_state;
	dma->spin_ref = 0;
	return 0;
}

int sa1100_dma_wakeup(dmach_t channel)
{
        sa1100_dma_t *dma = &dma_chan[channel];
	dma_regs_t *regs;
	int flags;

	if ((unsigned)channel >= MAX_SA1100_DMA_CHANNELS || !dma->in_use)
		return -EINVAL;

	if (channel_is_sa1111_sac(channel)) {
		/* We'll cheat a little until someone actually
		 * write the real thing.
		 */
		return 0;
	}

	regs = dma->regs;
	regs->ClrDCSR =
		(DCSR_DONEA | DCSR_DONEB | DCSR_STRTA | DCSR_STRTB |
		 DCSR_IE | DCSR_ERROR | DCSR_RUN);
	regs->DDAR = dma->device;
	local_irq_save(flags);
	process_dma(dma);
	local_irq_restore(flags);
	return 0;
}

EXPORT_SYMBOL(sa1100_dma_sleep);
EXPORT_SYMBOL(sa1100_dma_wakeup);

#endif


static int __init sa1100_init_dma(void)
{
	int channel;
	for (channel = 0; channel < SA1100_DMA_CHANNELS; channel++) {
		dma_chan[channel].regs =
		    (dma_regs_t *) &DDAR(channel);
		dma_chan[channel].irq = IRQ_DMA0 + channel;
	}
	return 0;
}

__initcall(sa1100_init_dma);
