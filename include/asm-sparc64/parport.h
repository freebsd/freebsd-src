/* $Id: parport.h,v 1.11 2001/05/11 07:54:24 davem Exp $
 * parport.h: sparc64 specific parport initialization and dma.
 *
 * Copyright (C) 1999  Eddie C. Dost  (ecd@skynet.be)
 */

#ifndef _ASM_SPARC64_PARPORT_H
#define _ASM_SPARC64_PARPORT_H 1

#include <asm/ebus.h>
#include <asm/isa.h>
#include <asm/ns87303.h>

#define PARPORT_PC_MAX_PORTS	PARPORT_MAX

static struct linux_ebus_dma *sparc_ebus_dmas[PARPORT_PC_MAX_PORTS];

static __inline__ void
reset_dma(unsigned int dmanr)
{
	unsigned int dcsr;

	writel(EBUS_DCSR_RESET, &sparc_ebus_dmas[dmanr]->dcsr);
	udelay(1);
	dcsr = EBUS_DCSR_BURST_SZ_16 | EBUS_DCSR_TCI_DIS |
	       EBUS_DCSR_EN_CNT | EBUS_DCSR_INT_EN;
	writel(dcsr, &sparc_ebus_dmas[dmanr]->dcsr);
}

static __inline__ void
enable_dma(unsigned int dmanr)
{
	unsigned int dcsr;

	dcsr = readl(&sparc_ebus_dmas[dmanr]->dcsr);
	dcsr |= EBUS_DCSR_EN_DMA;
	writel(dcsr, &sparc_ebus_dmas[dmanr]->dcsr);
}

static __inline__ void
disable_dma(unsigned int dmanr)
{
	unsigned int dcsr;

	dcsr = readl(&sparc_ebus_dmas[dmanr]->dcsr);
	if (dcsr & EBUS_DCSR_EN_DMA) {
		while (dcsr & EBUS_DCSR_DRAIN) {
			udelay(1);
			dcsr = readl(&sparc_ebus_dmas[dmanr]->dcsr);
		}
		dcsr &= ~(EBUS_DCSR_EN_DMA);
		writel(dcsr, &sparc_ebus_dmas[dmanr]->dcsr);

		dcsr = readl(&sparc_ebus_dmas[dmanr]->dcsr);
		if (dcsr & EBUS_DCSR_ERR_PEND)
			reset_dma(dmanr);
	}
}

static __inline__ void
clear_dma_ff(unsigned int dmanr)
{
	/* nothing */
}

static __inline__ void
set_dma_mode(unsigned int dmanr, char mode)
{
	unsigned int dcsr;

	dcsr = readl(&sparc_ebus_dmas[dmanr]->dcsr);
	dcsr |= EBUS_DCSR_EN_CNT | EBUS_DCSR_TC;
	if (mode == DMA_MODE_WRITE)
		dcsr &= ~(EBUS_DCSR_WRITE);
	else
		dcsr |= EBUS_DCSR_WRITE;
	writel(dcsr, &sparc_ebus_dmas[dmanr]->dcsr);
}

static __inline__ void
set_dma_addr(unsigned int dmanr, unsigned int addr)
{
	writel(addr, &sparc_ebus_dmas[dmanr]->dacr);
}

static __inline__ void
set_dma_count(unsigned int dmanr, unsigned int count)
{
	writel(count, &sparc_ebus_dmas[dmanr]->dbcr);
}

static __inline__ int
get_dma_residue(unsigned int dmanr)
{
	int res;

	res = readl(&sparc_ebus_dmas[dmanr]->dbcr);
	if (res != 0)
		reset_dma(dmanr);
	return res;
}

static int ebus_ecpp_p(struct linux_ebus_device *edev)
{
	if (!strcmp(edev->prom_name, "ecpp"))
		return 1;
	if (!strcmp(edev->prom_name, "parallel")) {
		char compat[19];
		prom_getstring(edev->prom_node,
			       "compatible",
			       compat, sizeof(compat));
		compat[18] = '\0';
		if (!strcmp(compat, "ecpp"))
			return 1;
		if (!strcmp(compat, "ns87317-ecpp") &&
		    !strcmp(compat + 13, "ecpp"))
			return 1;
	}
	return 0;
}

static int parport_isa_probe(int count)
{
	struct isa_bridge *isa_br;
	struct isa_device *isa_dev;

	for_each_isa(isa_br) {
		for_each_isadev(isa_dev, isa_br) {
			struct isa_device *child;
			unsigned long base;

			if (strcmp(isa_dev->prom_name, "dma"))
				continue;

			child = isa_dev->child;
			while (child) {
				if (!strcmp(child->prom_name, "parallel"))
					break;
				child = child->next;
			}
			if (!child)
				continue;

			base = child->resource.start;

			/* No DMA, see commentary in
			 * asm-sparc64/floppy.h:isa_floppy_init()
			 */
			if (parport_pc_probe_port(base, base + 0x400,
						  child->irq, PARPORT_DMA_NOFIFO,
						  child->bus->self))
				count++;
		}
	}

	return count;
}

static int parport_pc_find_nonpci_ports (int autoirq, int autodma)
{
	struct linux_ebus *ebus;
	struct linux_ebus_device *edev;
	int count = 0;

	if (!pci_present())
		return 0;

	for_each_ebus(ebus) {
		for_each_ebusdev(edev, ebus) {
			if (ebus_ecpp_p(edev)) {
				unsigned long base = edev->resource[0].start;
				unsigned long config = edev->resource[1].start;

				sparc_ebus_dmas[count] =
						(struct linux_ebus_dma *)
							edev->resource[2].start;
				reset_dma(count);

				/* Configure IRQ to Push Pull, Level Low */
				/* Enable ECP, set bit 2 of the CTR first */
				outb(0x04, base + 0x02);
				ns87303_modify(config, PCR,
					       PCR_EPP_ENABLE |
					       PCR_IRQ_ODRAIN,
					       PCR_ECP_ENABLE |
					       PCR_ECP_CLK_ENA |
					       PCR_IRQ_POLAR);

				/* CTR bit 5 controls direction of port */
				ns87303_modify(config, PTR,
					       0, PTR_LPT_REG_DIR);

				if (parport_pc_probe_port(base, base + 0x400,
							  edev->irqs[0],
							  count, ebus->self))
					count++;
			}
		}
	}

	count = parport_isa_probe(count);

	return count;
}

#endif /* !(_ASM_SPARC64_PARPORT_H */
