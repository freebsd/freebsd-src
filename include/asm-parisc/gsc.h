#ifndef ASM_PARISC_GSC_H
#define ASM_PARISC_GSC_H
#ifdef __KERNEL__

#include <linux/types.h>
#include <asm/io.h> /* temporary for __raw_{read,write} */

/* Please, call ioremap and use {read,write}[bwl] instead.  These functions
 * are not very fast.
 */
#define gsc_readb(x) __raw_readb((unsigned long)x)
#define gsc_readw(x) __raw_readw((unsigned long)x)
#define gsc_readl(x) __raw_readl((unsigned long)x)
#define gsc_writeb(x, y) __raw_writeb(x, (unsigned long)y)
#define gsc_writew(x, y) __raw_writew(x, (unsigned long)y)
#define gsc_writel(x, y) __raw_writel(x, (unsigned long)y)

struct gsc_irq {
	unsigned long txn_addr;	/* IRQ "target" */
	int txn_data;		/* HW "IRQ" */
	int irq;		/* virtual IRQ */
};

/* PA I/O Architected devices support at least 5 bits in the EIM register. */
#define GSC_EIM_WIDTH 5

extern int gsc_alloc_irq(struct gsc_irq *dev);	/* dev needs an irq */
extern int gsc_claim_irq(struct gsc_irq *dev, int irq);	/* dev needs this irq */

extern void probe_serial_gsc(void);

#endif /* __KERNEL__ */
#endif /* LINUX_GSC_H */
