/*
 * arch/sh/kernel/dma.c
 *
 * Copyright (C) 2000 Takashi YOSHII
 *
 * PC like DMA API for SuperH's DMAC.
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/module.h>

#include <asm/signal.h>
#include <asm/dma.h>

static struct dma_info_t *dma_info[MAX_DMA_CHANNELS];
static struct dma_info_t *autoinit_info[SH_MAX_DMA_CHANNELS] = {0};
static spinlock_t  dma_spin_lock;

static unsigned int calc_chcr(struct dma_info_t *info)
{
	unsigned int chcr;

	chcr = ( info->mode & DMA_MODE_WRITE )? info->mode_write : info->mode_read;
	if( info->mode & DMA_AUTOINIT )
		chcr |= CHCR_IE;
	return chcr;
}

static __inline__ int ts_shift(unsigned long chcr)
{
	return ((int[]){3,0,1,2,5,0,0,0})[(chcr>>4)&0x000007];
}

static void dma_tei(int irq, void *dev_id, struct pt_regs *regs)
{
	int chan = irq - DMTE_IRQ[0];
	struct dma_info_t *info = autoinit_info[chan];

	if( info->mode & DMA_MODE_WRITE )
		ctrl_outl(info->mem_addr, SAR[info->chan]);
	else
		ctrl_outl(info->mem_addr, DAR[info->chan]);

	ctrl_outl(info->count>>ts_shift(calc_chcr(info)), DMATCR[info->chan]);
	ctrl_outl(ctrl_inl(CHCR[info->chan])&~CHCR_TE, CHCR[info->chan]);
}

static struct irqaction irq_tei = { dma_tei, SA_INTERRUPT, 0, "dma_tei", NULL, NULL};

void setup_dma(unsigned int dmanr, struct dma_info_t *info)
{
	make_ipr_irq(DMTE_IRQ[info->chan], DMA_IPR_ADDR, DMA_IPR_POS, DMA_PRIORITY);
	setup_irq(DMTE_IRQ[info->chan], &irq_tei);
	dma_info[dmanr] = info;
}

unsigned long claim_dma_lock(void)
{
	unsigned long flags;
	spin_lock_irqsave(&dma_spin_lock, flags);
	return flags;
}

void release_dma_lock(unsigned long flags)
{
	spin_unlock_irqrestore(&dma_spin_lock, flags);
}

void enable_dma(unsigned int dmanr)
{
	struct dma_info_t *info = dma_info[dmanr];
	ctrl_outl(calc_chcr(info)|CHCR_DE, CHCR[info->chan]);
}

void disable_dma(unsigned int dmanr)
{
	struct dma_info_t *info = dma_info[dmanr];
	ctrl_outl(calc_chcr(info)&~CHCR_DE, CHCR[info->chan]);
}

void set_dma_mode(unsigned int dmanr, char mode)
{
	struct dma_info_t *info = dma_info[dmanr];

	info->mode = mode;
	set_dma_addr(dmanr, info->mem_addr);
	set_dma_count(dmanr, info->count);
	autoinit_info[info->chan] = info;
}

void set_dma_addr(unsigned int dmanr, unsigned int a)
{
	struct dma_info_t *info = dma_info[dmanr];
	unsigned long sar, dar;

	info->mem_addr = a;
	sar = (info->mode & DMA_MODE_WRITE)? info->mem_addr: info->dev_addr;
	dar = (info->mode & DMA_MODE_WRITE)? info->dev_addr: info->mem_addr;
	ctrl_outl(sar, SAR[info->chan]);
	ctrl_outl(dar, DAR[info->chan]);
}

void set_dma_count(unsigned int dmanr, unsigned int count)
{
	struct dma_info_t *info = dma_info[dmanr];
	info->count = count;
	ctrl_outl(count>>ts_shift(calc_chcr(info)), DMATCR[info->chan]);
}

int get_dma_residue(unsigned int dmanr)
{
	struct dma_info_t *info = dma_info[dmanr];
	return ctrl_inl(DMATCR[info->chan])<<ts_shift(calc_chcr(info));
}

#if defined(__SH4__)
static void dma_err(int irq, void *dev_id, struct pt_regs *regs)
{
	printk(KERN_WARNING "DMAE: DMAOR=%lx\n",ctrl_inl(DMAOR));
	ctrl_outl(ctrl_inl(DMAOR)&~DMAOR_NMIF, DMAOR);
	ctrl_outl(ctrl_inl(DMAOR)&~DMAOR_AE, DMAOR);
	ctrl_outl(ctrl_inl(DMAOR)|DMAOR_DME, DMAOR);
}
static struct irqaction irq_err = { dma_err, SA_INTERRUPT, 0, "dma_err", NULL, NULL};
#endif

int __init init_dma(void)
{
#if defined(__SH4__)
	make_ipr_irq(DMAE_IRQ, DMA_IPR_ADDR, DMA_IPR_POS, DMA_PRIORITY);
	setup_irq(DMAE_IRQ, &irq_err);
#endif

	ctrl_outl(DMAOR_DME, DMAOR);
	return 0;
}

static void __exit exit_dma(void)
{
#ifdef CONFIG_CPU_SH4
	free_irq(DMAE_IRQ);
#endif
}

module_init(init_dma);
module_exit(exit_dma);

MODULE_LICENSE("GPL");

EXPORT_SYMBOL(setup_dma);
EXPORT_SYMBOL(claim_dma_lock);
EXPORT_SYMBOL(release_dma_lock);
EXPORT_SYMBOL(enable_dma);
EXPORT_SYMBOL(disable_dma);
EXPORT_SYMBOL(set_dma_mode);
EXPORT_SYMBOL(set_dma_addr);
EXPORT_SYMBOL(set_dma_count);
EXPORT_SYMBOL(get_dma_residue);

