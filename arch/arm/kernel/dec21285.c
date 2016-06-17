/*
 *  linux/arch/arm/kernel/dec21285.c: PCI functions for DC21285
 *
 *  Copyright (C) 1998-2000 Russell King, Phil Blundell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/ptrace.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/ioport.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/mach/pci.h>
#include <asm/hardware/dec21285.h>

#define MAX_SLOTS		21

#define PCICMD_ERROR_BITS ((PCI_STATUS_DETECTED_PARITY | \
			PCI_STATUS_REC_MASTER_ABORT | \
			PCI_STATUS_REC_TARGET_ABORT | \
			PCI_STATUS_PARITY) << 16)

extern int setup_arm_irq(int, struct irqaction *);
extern void pcibios_report_status(u_int status_mask, int warn);
extern void register_isa_ports(unsigned int, unsigned int, unsigned int);

static unsigned long
dc21285_base_address(struct pci_dev *dev)
{
	unsigned long addr = 0;
	unsigned int devfn = dev->devfn;

	if (dev->bus->number == 0) {
		if (PCI_SLOT(devfn) == 0)
			/*
			 * For devfn 0, point at the 21285
			 */
			addr = ARMCSR_BASE;
		else {
			devfn -= 1 << 3;

			if (devfn < PCI_DEVFN(MAX_SLOTS, 0))
				addr = PCICFG0_BASE | 0xc00000 | (devfn << 8);
		}
	} else
		addr = PCICFG1_BASE | (dev->bus->number << 16) | (devfn << 8);

	return addr;
}

static int
dc21285_read_config_byte(struct pci_dev *dev, int where, u8 *value)
{
	unsigned long addr = dc21285_base_address(dev);
	u8 v;

	if (addr)
		asm("ldr%?b	%0, [%1, %2]"
			: "=r" (v) : "r" (addr), "r" (where));
	else
		v = 0xff;

	*value = v;

	return PCIBIOS_SUCCESSFUL;
}

static int
dc21285_read_config_word(struct pci_dev *dev, int where, u16 *value)
{
	unsigned long addr = dc21285_base_address(dev);
	u16 v;

	if (addr)
		asm("ldr%?h	%0, [%1, %2]"
			: "=r" (v) : "r" (addr), "r" (where));
	else
		v = 0xffff;

	*value = v;

	return PCIBIOS_SUCCESSFUL;
}

static int
dc21285_read_config_dword(struct pci_dev *dev, int where, u32 *value)
{
	unsigned long addr = dc21285_base_address(dev);
	u32 v;

	if (addr)
		asm("ldr%?	%0, [%1, %2]"
			: "=r" (v) : "r" (addr), "r" (where));
	else
		v = 0xffffffff;

	*value = v;

	return PCIBIOS_SUCCESSFUL;
}

static int
dc21285_write_config_byte(struct pci_dev *dev, int where, u8 value)
{
	unsigned long addr = dc21285_base_address(dev);

	if (addr)
		asm("str%?b	%0, [%1, %2]"
			: : "r" (value), "r" (addr), "r" (where));

	return PCIBIOS_SUCCESSFUL;
}

static int
dc21285_write_config_word(struct pci_dev *dev, int where, u16 value)
{
	unsigned long addr = dc21285_base_address(dev);

	if (addr)
		asm("str%?h	%0, [%1, %2]"
			: : "r" (value), "r" (addr), "r" (where));

	return PCIBIOS_SUCCESSFUL;
}

static int
dc21285_write_config_dword(struct pci_dev *dev, int where, u32 value)
{
	unsigned long addr = dc21285_base_address(dev);

	if (addr)
		asm("str%?	%0, [%1, %2]"
			: : "r" (value), "r" (addr), "r" (where));

	return PCIBIOS_SUCCESSFUL;
}

static struct pci_ops dc21285_ops = {
	dc21285_read_config_byte,
	dc21285_read_config_word,
	dc21285_read_config_dword,
	dc21285_write_config_byte,
	dc21285_write_config_word,
	dc21285_write_config_dword,
};

static struct timer_list serr_timer;
static struct timer_list perr_timer;

static void dc21285_enable_error(unsigned long __data)
{
	switch (__data) {
	case IRQ_PCI_SERR:
		del_timer(&serr_timer);
		break;

	case IRQ_PCI_PERR:
		del_timer(&perr_timer);
		break;
	}

	enable_irq(__data);
}

/*
 * Warn on PCI errors.
 */
static void dc21285_abort_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned int cmd;
	unsigned int status;

	cmd = *CSR_PCICMD;
	status = cmd >> 16;
	cmd = cmd & 0xffff;

	if (status & PCI_STATUS_REC_MASTER_ABORT) {
		printk(KERN_DEBUG "PCI: master abort: ");
		pcibios_report_status(PCI_STATUS_REC_MASTER_ABORT, 1);
		printk("\n");

		cmd |= PCI_STATUS_REC_MASTER_ABORT << 16;
	}

	if (status & PCI_STATUS_REC_TARGET_ABORT) {
		printk(KERN_DEBUG "PCI: target abort: ");
		pcibios_report_status(PCI_STATUS_SIG_TARGET_ABORT, 1);
		printk("\n");

		cmd |= PCI_STATUS_REC_TARGET_ABORT << 16;
	}

	*CSR_PCICMD = cmd;
}

static void dc21285_serr_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	struct timer_list *timer = dev_id;
	unsigned int cntl;

	printk(KERN_DEBUG "PCI: system error received: ");
	pcibios_report_status(PCI_STATUS_SIG_SYSTEM_ERROR, 1);
	printk("\n");

	cntl = *CSR_SA110_CNTL & 0xffffdf07;
	*CSR_SA110_CNTL = cntl | SA110_CNTL_RXSERR;

	/*
	 * back off this interrupt
	 */
	disable_irq(irq);
	timer->expires = jiffies + HZ;
	add_timer(timer);
}

static void dc21285_discard_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	printk(KERN_DEBUG "PCI: discard timer expired\n");
	*CSR_SA110_CNTL &= 0xffffde07;
}

static void dc21285_dparity_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned int cmd;

	printk(KERN_DEBUG "PCI: data parity error detected: ");
	pcibios_report_status(PCI_STATUS_PARITY | PCI_STATUS_DETECTED_PARITY, 1);
	printk("\n");

	cmd = *CSR_PCICMD & 0xffff;
	*CSR_PCICMD = cmd | 1 << 24;
}

static void dc21285_parity_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	struct timer_list *timer = dev_id;
	unsigned int cmd;

	printk(KERN_DEBUG "PCI: parity error detected: ");
	pcibios_report_status(PCI_STATUS_PARITY | PCI_STATUS_DETECTED_PARITY, 1);
	printk("\n");

	cmd = *CSR_PCICMD & 0xffff;
	*CSR_PCICMD = cmd | 1 << 31;

	/*
	 * back off this interrupt
	 */
	disable_irq(irq);
	timer->expires = jiffies + HZ;
	add_timer(timer);
}

void __init dc21285_setup_resources(struct resource **resource)
{
	struct resource *busmem, *busmempf;

	busmem = kmalloc(sizeof(*busmem), GFP_KERNEL);
	busmempf = kmalloc(sizeof(*busmempf), GFP_KERNEL);
	memset(busmem, 0, sizeof(*busmem));
	memset(busmempf, 0, sizeof(*busmempf));

	busmem->flags = IORESOURCE_MEM;
	busmem->name  = "Footbridge non-prefetch";
	busmempf->flags = IORESOURCE_MEM | IORESOURCE_PREFETCH;
	busmempf->name  = "Footbridge prefetch";

	allocate_resource(&iomem_resource, busmempf, 0x20000000,
			  0x80000000, 0xffffffff, 0x20000000, NULL, NULL);
	allocate_resource(&iomem_resource, busmem, 0x40000000,
			  0x80000000, 0xffffffff, 0x40000000, NULL, NULL);

	resource[0] = &ioport_resource;
	resource[1] = busmem;
	resource[2] = busmempf;
}

void __init dc21285_init(void *sysdata)
{
	unsigned int mem_size, mem_mask;
	int cfn_mode;

	mem_size = (unsigned int)high_memory - PAGE_OFFSET;
	for (mem_mask = 0x00100000; mem_mask < 0x10000000; mem_mask <<= 1)
		if (mem_mask >= mem_size)
			break;		

	/*
	 * These registers need to be set up whether we're the
	 * central function or not.
	 */
	*CSR_SDRAMBASEMASK    = (mem_mask - 1) & 0x0ffc0000;
	*CSR_SDRAMBASEOFFSET  = 0;
	*CSR_ROMBASEMASK      = 0x80000000;
	*CSR_CSRBASEMASK      = 0;
	*CSR_CSRBASEOFFSET    = 0;
	*CSR_PCIADDR_EXTN     = 0;

	cfn_mode = __footbridge_cfn_mode();

	printk(KERN_INFO "PCI: DC21285 footbridge, revision %02lX, in "
		"%s mode\n", *CSR_CLASSREV & 0xff, cfn_mode ?
		"central function" : "addin");

	if (cfn_mode) {
		static struct resource csrmem, csrio;

		csrio.flags  = IORESOURCE_IO;
		csrio.name   = "Footbridge";
		csrmem.flags = IORESOURCE_MEM;
		csrmem.name  = "Footbridge";

		allocate_resource(&ioport_resource, &csrio, 128,
				  0xff00, 0xffff, 128, NULL, NULL);
		allocate_resource(&iomem_resource, &csrmem, 128,
				  0xf4000000, 0xf8000000, 128, NULL, NULL);

		/*
		 * Map our SDRAM at a known address in PCI space, just in case
		 * the firmware had other ideas.  Using a nonzero base is
		 * necessary, since some VGA cards forcefully use PCI addresses
		 * in the range 0x000a0000 to 0x000c0000. (eg, S3 cards).
		 */
		*CSR_PCICSRBASE       = csrmem.start;
		*CSR_PCICSRIOBASE     = csrio.start;
		*CSR_PCISDRAMBASE     = __virt_to_bus(PAGE_OFFSET);
		*CSR_PCIROMBASE       = 0;
		*CSR_PCICMD = PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER |
			      PCI_COMMAND_INVALIDATE | PCICMD_ERROR_BITS;

		pci_scan_bus(0, &dc21285_ops, sysdata);

		/*
		 * Clear any existing errors - we aren't
		 * interested in historical data...
		 */
		*CSR_SA110_CNTL	= (*CSR_SA110_CNTL & 0xffffde07) |
				  SA110_CNTL_RXSERR;
		*CSR_PCICMD = (*CSR_PCICMD & 0xffff) | PCICMD_ERROR_BITS;
	} else if (footbridge_cfn_mode() != 0) {
		/*
		 * If we are not compiled to accept "add-in" mode, then
		 * we are using a constant virt_to_bus translation which
		 * can not hope to cater for the way the host BIOS  has
		 * set up the machine.
		 */
		panic("PCI: this kernel is compiled for central "
			"function mode only");
	}

	/*
	 * Initialise PCI error IRQ after we've finished probing
	 */
	request_irq(IRQ_PCI_ABORT,     dc21285_abort_irq,   SA_INTERRUPT, "PCI abort",       NULL);
	request_irq(IRQ_DISCARD_TIMER, dc21285_discard_irq, SA_INTERRUPT, "Discard timer",   NULL);
	request_irq(IRQ_PCI_DPERR,     dc21285_dparity_irq, SA_INTERRUPT, "PCI data parity", NULL);

	init_timer(&serr_timer);
	init_timer(&perr_timer);

	serr_timer.data = IRQ_PCI_SERR;
	serr_timer.function = dc21285_enable_error;
	perr_timer.data = IRQ_PCI_PERR;
	perr_timer.function = dc21285_enable_error;

	request_irq(IRQ_PCI_SERR, dc21285_serr_irq, SA_INTERRUPT,
		    "PCI system error", &serr_timer);
	request_irq(IRQ_PCI_PERR, dc21285_parity_irq, SA_INTERRUPT,
		    "PCI parity error", &perr_timer);

	register_isa_ports(DC21285_PCI_MEM, DC21285_PCI_IO, 0);
}
