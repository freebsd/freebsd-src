/*
 *
 *    Copyright (c) 1999 Grant Erickson <grant@lcse.umn.edu>
 *
 *    Module name: ppc4xx_pic.c
 *
 *    Description:
 *      Interrupt controller driver for PowerPC 4xx-based processors.
 */

/*
 * The PowerPC 403 cores' Asynchronous Interrupt Controller (AIC) has
 * 32 possible interrupts, a majority of which are not implemented on
 * all cores. There are six configurable, external interrupt pins and
 * there are eight internal interrupts for the on-chip serial port
 * (SPU), DMA controller, and JTAG controller.
 *
 * The PowerPC 405/440 cores' Universal Interrupt Controller (UIC) has
 * 32 possible interrupts as well.  Depending on the core and SoC
 * implementation, a portion of the interrrupts are used for on-chip
 * peripherals and a portion of the interrupts are available to be
 * configured for external devices generating interrupts.
 *
 * The PowerNP and 440GP (and most likely future implementations) have
 * cascaded UICs.
 *
 */

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/stddef.h>

#include <asm/processor.h>
#include <asm/system.h>
#include <asm/irq.h>
#include <asm/ppc4xx_pic.h>

/* Global Variables */
struct hw_interrupt_type *ppc4xx_pic;
/*
 * We define 4xxIRQ_InitSenses table thusly:
 * bit 0x1: sense, 0 for edge and 1 for level.
 * bit 0x2: polarity, 0 for negative, 1 for positive.
 */
unsigned int ibm4xxPIC_NumInitSenses __initdata = 0;
unsigned char *ibm4xxPIC_InitSenses __initdata = NULL;

/* Six of one, half dozen of the other....#ifdefs, separate files,
 * other tricks.....
 *
 * There are basically two types of interrupt controllers, the 403 AIC
 * and the "others" with UIC.  I just kept them both here separated
 * with #ifdefs, but it seems to change depending upon how supporting
 * files (like ppc4xx.h) change.		-- Dan.
 */

#ifdef CONFIG_403

/* Function Prototypes */

static void ppc403_aic_enable(unsigned int irq);
static void ppc403_aic_disable(unsigned int irq);
static void ppc403_aic_disable_and_ack(unsigned int irq);

static struct hw_interrupt_type ppc403_aic = {
	"403GC AIC",
	NULL,
	NULL,
	ppc403_aic_enable,
	ppc403_aic_disable,
	ppc403_aic_disable_and_ack,
	0
};

int
ppc403_pic_get_irq(struct pt_regs *regs)
{
	int irq;
	unsigned long bits;

	/*
	 * Only report the status of those interrupts that are actually
	 * enabled.
	 */

	bits = mfdcr(DCRN_EXISR) & mfdcr(DCRN_EXIER);

	/*
	 * Walk through the interrupts from highest priority to lowest, and
	 * report the first pending interrupt found.
	 * We want PPC, not C bit numbering, so just subtract the ffs()
	 * result from 32.
	 */
	irq = 32 - ffs(bits);

	if (irq == NR_AIC_IRQS)
		irq = -1;

	return (irq);
}

static void
ppc403_aic_enable(unsigned int irq)
{
	int bit, word;

	bit = irq & 0x1f;
	word = irq >> 5;

	ppc_cached_irq_mask[word] |= (1 << (31 - bit));
	mtdcr(DCRN_EXIER, ppc_cached_irq_mask[word]);
}

static void
ppc403_aic_disable(unsigned int irq)
{
	int bit, word;

	bit = irq & 0x1f;
	word = irq >> 5;

	ppc_cached_irq_mask[word] &= ~(1 << (31 - bit));
	mtdcr(DCRN_EXIER, ppc_cached_irq_mask[word]);
}

static void
ppc403_aic_disable_and_ack(unsigned int irq)
{
	int bit, word;

	bit = irq & 0x1f;
	word = irq >> 5;

	ppc_cached_irq_mask[word] &= ~(1 << (31 - bit));
	mtdcr(DCRN_EXIER, ppc_cached_irq_mask[word]);
	mtdcr(DCRN_EXISR, (1 << (31 - bit)));
}

#else

#ifndef UIC1
#define UIC1 UIC0
#endif

static void
ppc405_uic_enable(unsigned int irq)
{
	int bit, word;
	irq_desc_t *desc = irq_desc + irq;

	bit = irq & 0x1f;
	word = irq >> 5;

#ifdef UIC_DEBUG
	printk("ppc405_uic_enable - irq %d word %d bit 0x%x\n", irq, word, bit);
#endif
	ppc_cached_irq_mask[word] |= 1 << (31 - bit);
	switch (word) {
	case 0:
		mtdcr(DCRN_UIC_ER(UIC0), ppc_cached_irq_mask[word]);
		if ((mfdcr(DCRN_UIC_TR(UIC0)) & (1 << (31 - bit))) == 0)
			desc->status |= IRQ_LEVEL;
		else
		/* lets hope this works since in linux/irq.h
		 * there is no define for EDGE and it's assumed
		 * once you set status to LEVEL you would not
		 * want to change it - Armin
		 */
		desc->status = desc->status & ~IRQ_LEVEL;
		break;
	case 1:
		mtdcr(DCRN_UIC_ER(UIC1), ppc_cached_irq_mask[word]);
		if ((mfdcr(DCRN_UIC_TR(UIC1)) & (1 << (31 - bit))) == 0)
			desc->status |= IRQ_LEVEL;
		else
		/* lets hope this works since in linux/irq.h
		 * there is no define for EDGE and it's assumed
		 * once you set status to LEVEL you would not
		 * want to change it - Armin
		 */
		desc->status = desc->status & ~IRQ_LEVEL;
	break;
	}

}

static void
ppc405_uic_disable(unsigned int irq)
{
	int bit, word;

	bit = irq & 0x1f;
	word = irq >> 5;
#ifdef UIC_DEBUG
	printk("ppc405_uic_disable - irq %d word %d bit 0x%x\n", irq, word,
	       bit);
#endif
	ppc_cached_irq_mask[word] &= ~(1 << (31 - bit));
	switch (word) {
	case 0:
		mtdcr(DCRN_UIC_ER(UIC0), ppc_cached_irq_mask[word]);
		break;
	case 1:
		mtdcr(DCRN_UIC_ER(UIC1), ppc_cached_irq_mask[word]);
		break;
	}
}

static void
ppc405_uic_disable_and_ack(unsigned int irq)
{
	int bit, word;

	bit = irq & 0x1f;
	word = irq >> 5;

#ifdef UIC_DEBUG
	printk("ppc405_uic_disable_and_ack - irq %d word %d bit 0x%x\n", irq,
	       word, bit);
#endif
	ppc_cached_irq_mask[word] &= ~(1 << (31 - bit));
	switch (word) {
	case 0:
		mtdcr(DCRN_UIC_ER(UIC0), ppc_cached_irq_mask[word]);
		mtdcr(DCRN_UIC_SR(UIC0), (1 << (31 - bit)));
		break;
#if NR_UICS > 1
	case 1:
		mtdcr(DCRN_UIC_ER(UIC1), ppc_cached_irq_mask[word]);
		mtdcr(DCRN_UIC_SR(UIC1), (1 << (31 - bit)));
		/* ACK cascaded interrupt in UIC0 */
		mtdcr(DCRN_UIC_SR(UIC0), (1 << (31 - UIC0_UIC1NC)));
		break;
#endif
	}
}

static void
ppc405_uic_end(unsigned int irq)
{
	int bit, word;
	unsigned int tr_bits;

	bit = irq & 0x1f;
	word = irq >> 5;

#ifdef UIC_DEBUG
	printk("ppc405_uic_end - irq %d word %d bit 0x%x\n", irq, word, bit);
#endif

	switch (word) {
	case 0:
		tr_bits = mfdcr(DCRN_UIC_TR(UIC0));
		break;
	case 1:
		tr_bits = mfdcr(DCRN_UIC_TR(UIC1));
		break;
	}

	if ((tr_bits & (1 << (31 - bit))) == 0) {
		/* level trigger */
		switch (word) {
		case 0:
			mtdcr(DCRN_UIC_SR(UIC0), 1 << (31 - bit));
			break;
#if NR_UICS > 1
		case 1:
			mtdcr(DCRN_UIC_SR(UIC1), 1 << (31 - bit));
			/* ACK cascaded interrupt in UIC0 */
			mtdcr(DCRN_UIC_SR(UIC0), (1 << (31 - UIC0_UIC1NC)));
			break;
#endif
		}
	}

	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS))) {
		ppc_cached_irq_mask[word] |= 1 << (31 - bit);
		switch (word) {
		case 0:
			mtdcr(DCRN_UIC_ER(UIC0), ppc_cached_irq_mask[word]);
			break;
		case 1:
			mtdcr(DCRN_UIC_ER(UIC1), ppc_cached_irq_mask[word]);
			break;
		}
	}
}

static struct hw_interrupt_type ppc405_uic = {
#if (NR_UICS == 1)
	"IBM UIC",
#else
	"IBM UIC Cascade",
#endif
	NULL,
	NULL,
	ppc405_uic_enable,
	ppc405_uic_disable,
	ppc405_uic_disable_and_ack,
	ppc405_uic_end,
	0
};

int
ppc405_pic_get_irq(struct pt_regs *regs)
{
	int irq, cas_irq;
	unsigned long bits;
	cas_irq = 0;
	/*
	 * Only report the status of those interrupts that are actually
	 * enabled.
	 */

	bits = mfdcr(DCRN_UIC_MSR(UIC0));

#if (NR_UICS > 1)
	if (bits & UIC_CASCADE_MASK) {
		bits = mfdcr(DCRN_UIC_MSR(UIC1));
		cas_irq = 32 - ffs(bits);
		irq = 32 + cas_irq;
	} else {
		irq = 32 - ffs(bits);
		if (irq == 32)
			irq = -1;
	}
#else
	/*
	 * Walk through the interrupts from highest priority to lowest, and
	 * report the first pending interrupt found.
	 * We want PPC, not C bit numbering, so just subtract the ffs()
	 * result from 32.
	 */
	irq = 32 - ffs(bits);
#endif
	if (irq == (NR_UIC_IRQS * NR_UICS))
		irq = -1;

#ifdef UIC_DEBUG
	printk("ppc405_pic_get_irq - irq %d bit 0x%x\n", irq, bits);
#endif

	return (irq);
}
#endif

void __init
ppc4xx_extpic_init(void)
{
	unsigned int sense, irq;
	int bit, word;
	unsigned long ppc_cached_sense_mask[NR_MASK_WORDS];
	unsigned long ppc_cached_pol_mask[NR_MASK_WORDS];
	ppc_cached_sense_mask[0] = 0;
	ppc_cached_sense_mask[1] = 0;
	ppc_cached_pol_mask[0] = 0;
	ppc_cached_pol_mask[1] = 0;

	for (irq = 0; irq < NR_IRQS; irq++) {

		bit = irq & 0x1f;
		word = irq >> 5;

		sense = (irq < ibm4xxPIC_NumInitSenses) ?
			ibm4xxPIC_InitSenses[irq] :
			IRQ_SENSE_EDGE | IRQ_POLARITY_POSITIVE;
#ifdef PPC4xx_PIC_DEBUG
		printk("PPC4xx_picext %d word:%x bit:%x sense:%x", irq, word,
		       bit, sense);
#endif
		ppc_cached_sense_mask[word] |=
		    (~sense & IRQ_SENSE_MASK) << (31 - bit);
		ppc_cached_pol_mask[word] |=
		    ((sense & IRQ_POLARITY_MASK) >> 1) << (31 - bit);
		switch (word) {
		case 0:
#ifdef PPC4xx_PIC_DEBUG
			printk("Pol %x ", mfdcr(DCRN_UIC_PR(UIC0)));
			printk("Level %x\n", mfdcr(DCRN_UIC_TR(UIC0)));
#endif
			/* polarity  setting */
			mtdcr(DCRN_UIC_PR(UIC0), ppc_cached_pol_mask[word]);

			/* Level setting */
			mtdcr(DCRN_UIC_TR(UIC0), ppc_cached_sense_mask[word]);

			break;
		case 1:
#ifdef PPC4xx_PIC_DEBUG
			printk("Pol %x ", mfdcr(DCRN_UIC_PR(UIC1)));
			printk("Level %x\n", mfdcr(DCRN_UIC_TR(UIC1)));
#endif
			/* polarity  setting */
			mtdcr(DCRN_UIC_PR(UIC1), ppc_cached_pol_mask[word]);

			/* Level setting */
			mtdcr(DCRN_UIC_TR(UIC1), ppc_cached_sense_mask[word]);

			break;
		}
	}

}
void __init
ppc4xx_pic_init(void)
{

	/*
	 * Disable all external interrupts until they are
	 * explicity requested.
	 */
	ppc_cached_irq_mask[0] = 0;
	ppc_cached_irq_mask[1] = 0;

#if defined CONFIG_403
	mtdcr(DCRN_EXIER, ppc_cached_irq_mask[0]);

	ppc4xx_pic = &ppc403_aic;
	ppc_md.get_irq = ppc403_pic_get_irq;
#else
#if  (NR_UICS > 1)
	ppc_cached_irq_mask[0] |= 1 << (31 - UIC0_UIC1NC);	/* enable cascading interrupt */
	mtdcr(DCRN_UIC_ER(UIC1), ppc_cached_irq_mask[1]);
	mtdcr(DCRN_UIC_CR(UIC1), 0);

#endif
	mtdcr(DCRN_UIC_ER(UIC0), ppc_cached_irq_mask[0]);
	mtdcr(DCRN_UIC_CR(UIC0), 0);

	if (ibm4xxPIC_InitSenses != NULL)
		ppc4xx_extpic_init();

	/* Clear any pending interrupts */
#if (NR_UICS > 1)
	mtdcr(DCRN_UIC_SR(UIC1), 0xffffffff);
#endif
	mtdcr(DCRN_UIC_SR(UIC0), 0xffffffff);

	ppc4xx_pic = &ppc405_uic;
	ppc_md.get_irq = ppc405_pic_get_irq;
#endif

}
