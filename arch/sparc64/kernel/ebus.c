/* $Id: ebus.c,v 1.64.2.1 2002/03/12 18:46:14 davem Exp $
 * ebus.c: PCI to EBus bridge device.
 *
 * Copyright (C) 1997  Eddie C. Dost  (ecd@skynet.be)
 * Copyright (C) 1999  David S. Miller (davem@redhat.com)
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <asm/system.h>
#include <asm/page.h>
#include <asm/pbm.h>
#include <asm/ebus.h>
#include <asm/oplib.h>
#include <asm/bpp.h>
#include <asm/irq.h>

struct linux_ebus *ebus_chain = 0;

#ifdef CONFIG_SUN_AUXIO
extern void auxio_probe(void);
#endif

static inline void *ebus_alloc(size_t size)
{
	void *mem;

	mem = kmalloc(size, GFP_ATOMIC);
	if (!mem)
		panic("ebus_alloc: out of memory");
	memset((char *)mem, 0, size);
	return mem;
}

static void __init ebus_ranges_init(struct linux_ebus *ebus)
{
	int success;

	ebus->num_ebus_ranges = 0;
	success = prom_getproperty(ebus->prom_node, "ranges",
				   (char *)ebus->ebus_ranges,
				   sizeof(ebus->ebus_ranges));
	if (success != -1)
		ebus->num_ebus_ranges = (success/sizeof(struct linux_prom_ebus_ranges));
}

static void __init ebus_intmap_init(struct linux_ebus *ebus)
{
	int success;

	ebus->num_ebus_intmap = 0;
	success = prom_getproperty(ebus->prom_node, "interrupt-map",
				   (char *)ebus->ebus_intmap,
				   sizeof(ebus->ebus_intmap));
	if (success == -1)
		return;

	ebus->num_ebus_intmap = (success/sizeof(struct linux_prom_ebus_intmap));

	success = prom_getproperty(ebus->prom_node, "interrupt-map-mask",
				   (char *)&ebus->ebus_intmask,
				   sizeof(ebus->ebus_intmask));
	if (success == -1) {
		prom_printf("ebus: can't get interrupt-map-mask\n");
		prom_halt();
	}
}

int __init ebus_intmap_match(struct linux_ebus *ebus,
			     struct linux_prom_registers *reg,
			     int *interrupt)
{
	unsigned int hi, lo, irq;
	int i;

	if (!ebus->num_ebus_intmap)
		return 0;

	hi = reg->which_io & ebus->ebus_intmask.phys_hi;
	lo = reg->phys_addr & ebus->ebus_intmask.phys_lo;
	irq = *interrupt & ebus->ebus_intmask.interrupt;
	for (i = 0; i < ebus->num_ebus_intmap; i++) {
		if ((ebus->ebus_intmap[i].phys_hi == hi) &&
		    (ebus->ebus_intmap[i].phys_lo == lo) &&
		    (ebus->ebus_intmap[i].interrupt == irq)) {
			*interrupt = ebus->ebus_intmap[i].cinterrupt;
			return 0;
		}
	}
	return -1;
}

void __init fill_ebus_child(int node, struct linux_prom_registers *preg,
			    struct linux_ebus_child *dev, int non_standard_regs)
{
	int regs[PROMREG_MAX];
	int irqs[PROMREG_MAX];
	int i, len;

	dev->prom_node = node;
	prom_getstring(node, "name", dev->prom_name, sizeof(dev->prom_name));
	printk(" (%s)", dev->prom_name);

	len = prom_getproperty(node, "reg", (void *)regs, sizeof(regs));
	dev->num_addrs = len / sizeof(regs[0]);

	if (non_standard_regs) {
		/* This is to handle reg properties which are not
		 * in the parent relative format.  One example are
		 * children of the i2c device on CompactPCI systems.
		 *
		 * So, for such devices we just record the property
		 * raw in the child resources.
		 */
		for (i = 0; i < dev->num_addrs; i++)
			dev->resource[i].start = regs[i];
	} else {
		for (i = 0; i < dev->num_addrs; i++) {
			int rnum = regs[i];
			if (rnum >= dev->parent->num_addrs) {
				prom_printf("UGH: property for %s was %d, need < %d\n",
					    dev->prom_name, len, dev->parent->num_addrs);
				panic("fill_ebus_child");
			}
			dev->resource[i].start = dev->parent->resource[i].start;
			dev->resource[i].end = dev->parent->resource[i].end;
			dev->resource[i].flags = IORESOURCE_MEM;
			dev->resource[i].name = dev->prom_name;
		}
	}

	for (i = 0; i < PROMINTR_MAX; i++)
		dev->irqs[i] = PCI_IRQ_NONE;

	len = prom_getproperty(node, "interrupts", (char *)&irqs, sizeof(irqs));
	if ((len == -1) || (len == 0)) {
		dev->num_irqs = 0;
		/*
		 * Oh, well, some PROMs don't export interrupts
		 * property to children of EBus devices...
		 *
		 * Be smart about PS/2 keyboard and mouse.
		 */
		if (!strcmp(dev->parent->prom_name, "8042")) {
			if (!strcmp(dev->prom_name, "kb_ps2")) {
				dev->num_irqs = 1;
				dev->irqs[0] = dev->parent->irqs[0];
			} else {
				dev->num_irqs = 1;
				dev->irqs[0] = dev->parent->irqs[1];
			}
		}
	} else {
		dev->num_irqs = len / sizeof(irqs[0]);
		for (i = 0; i < dev->num_irqs; i++) {
			struct pci_pbm_info *pbm = dev->bus->parent;
			struct pci_controller_info *p = pbm->parent;

			if (ebus_intmap_match(dev->bus, preg, &irqs[i]) != -1) {
				dev->irqs[i] = p->irq_build(pbm,
							    dev->bus->self,
							    irqs[i]);
			} else {
				/* If we get a bogus interrupt property, just
				 * record the raw value instead of punting.
				 */
				dev->irqs[i] = irqs[i];
			}
		}
	}
}

static int __init child_regs_nonstandard(struct linux_ebus_device *dev)
{
	if (!strcmp(dev->prom_name, "i2c") ||
	    !strcmp(dev->prom_name, "SUNW,lombus"))
		return 1;
	return 0;
}

void __init fill_ebus_device(int node, struct linux_ebus_device *dev)
{
	struct linux_prom_registers regs[PROMREG_MAX];
	struct linux_ebus_child *child;
	int irqs[PROMINTR_MAX];
	int i, n, len;

	dev->prom_node = node;
	prom_getstring(node, "name", dev->prom_name, sizeof(dev->prom_name));
	printk(" [%s", dev->prom_name);

	len = prom_getproperty(node, "reg", (void *)regs, sizeof(regs));
	if (len == -1) {
		dev->num_addrs = 0;
		goto probe_interrupts;
	}

	if (len % sizeof(struct linux_prom_registers)) {
		prom_printf("UGH: proplen for %s was %d, need multiple of %d\n",
			    dev->prom_name, len,
			    (int)sizeof(struct linux_prom_registers));
		prom_halt();
	}
	dev->num_addrs = len / sizeof(struct linux_prom_registers);

	for (i = 0; i < dev->num_addrs; i++) {
		/* XXX Learn how to interpret ebus ranges... -DaveM */
		if (regs[i].which_io >= 0x10)
			n = (regs[i].which_io - 0x10) >> 2;
		else
			n = regs[i].which_io;

		dev->resource[i].start  = dev->bus->self->resource[n].start;
		dev->resource[i].start += (unsigned long)regs[i].phys_addr;
		dev->resource[i].end    =
			(dev->resource[i].start + (unsigned long)regs[i].reg_size - 1UL);
		dev->resource[i].flags  = IORESOURCE_MEM;
		dev->resource[i].name   = dev->prom_name;
		request_resource(&dev->bus->self->resource[n],
				 &dev->resource[i]);
	}

probe_interrupts:
	for (i = 0; i < PROMINTR_MAX; i++)
		dev->irqs[i] = PCI_IRQ_NONE;

	len = prom_getproperty(node, "interrupts", (char *)&irqs, sizeof(irqs));
	if ((len == -1) || (len == 0)) {
		dev->num_irqs = 0;
	} else {
		dev->num_irqs = len / sizeof(irqs[0]);
		for (i = 0; i < dev->num_irqs; i++) {
			struct pci_pbm_info *pbm = dev->bus->parent;
			struct pci_controller_info *p = pbm->parent;

			if (ebus_intmap_match(dev->bus, &regs[0], &irqs[i]) != -1) {
				dev->irqs[i] = p->irq_build(pbm,
							    dev->bus->self,
							    irqs[i]);
			} else {
				/* If we get a bogus interrupt property, just
				 * record the raw value instead of punting.
				 */
				dev->irqs[i] = irqs[i];
			}
		}
	}

	if ((node = prom_getchild(node))) {
		printk(" ->");
		dev->children = ebus_alloc(sizeof(struct linux_ebus_child));

		child = dev->children;
		child->next = 0;
		child->parent = dev;
		child->bus = dev->bus;
		fill_ebus_child(node, &regs[0],
				child, child_regs_nonstandard(dev));

		while ((node = prom_getsibling(node))) {
			child->next = ebus_alloc(sizeof(struct linux_ebus_child));

			child = child->next;
			child->next = 0;
			child->parent = dev;
			child->bus = dev->bus;
			fill_ebus_child(node, &regs[0],
					child, child_regs_nonstandard(dev));
		}
	}
	printk("]");
}

static struct pci_dev *find_next_ebus(struct pci_dev *start, int *is_rio_p)
{
	struct pci_dev *pdev = start;

	do {
		pdev = pci_find_device(PCI_VENDOR_ID_SUN, PCI_ANY_ID, pdev);
		if (pdev &&
		    (pdev->device == PCI_DEVICE_ID_SUN_EBUS ||
		     pdev->device == PCI_DEVICE_ID_SUN_RIO_EBUS))
			break;
	} while (pdev != NULL);

	if (pdev && (pdev->device == PCI_DEVICE_ID_SUN_RIO_EBUS))
		*is_rio_p = 1;
	else
		*is_rio_p = 0;

	return pdev;
}

void __init ebus_init(void)
{
	struct pci_pbm_info *pbm;
	struct linux_ebus_device *dev;
	struct linux_ebus *ebus;
	struct pci_dev *pdev;
	struct pcidev_cookie *cookie;
	int nd, ebusnd, is_rio;
	int num_ebus = 0;

	if (!pci_present())
		return;

	pdev = find_next_ebus(NULL, &is_rio);
	if (!pdev) {
		printk("ebus: No EBus's found.\n");
		return;
	}

	cookie = pdev->sysdata;
	ebusnd = cookie->prom_node;

	ebus_chain = ebus = ebus_alloc(sizeof(struct linux_ebus));
	ebus->next = 0;
	ebus->is_rio = is_rio;

	while (ebusnd) {
		/* SUNW,pci-qfe uses four empty ebuses on it.
		   I think we should not consider them here,
		   as they have half of the properties this
		   code expects and once we do PCI hot-plug,
		   we'd have to tweak with the ebus_chain
		   in the runtime after initialization. -jj */
		if (!prom_getchild (ebusnd)) {
			pdev = find_next_ebus(pdev, &is_rio);
			if (!pdev) {
				if (ebus == ebus_chain) {
					ebus_chain = NULL;
					printk("ebus: No EBus's found.\n");
					return;
				}
				break;
			}
			ebus->is_rio = is_rio;
			cookie = pdev->sysdata;
			ebusnd = cookie->prom_node;
			continue;
		}
		printk("ebus%d:", num_ebus);

		prom_getstring(ebusnd, "name", ebus->prom_name, sizeof(ebus->prom_name));
		ebus->index = num_ebus;
		ebus->prom_node = ebusnd;
		ebus->self = pdev;
		ebus->parent = pbm = cookie->pbm;

		ebus_ranges_init(ebus);
		ebus_intmap_init(ebus);

		nd = prom_getchild(ebusnd);
		if (!nd)
			goto next_ebus;

		ebus->devices = ebus_alloc(sizeof(struct linux_ebus_device));

		dev = ebus->devices;
		dev->next = 0;
		dev->children = 0;
		dev->bus = ebus;
		fill_ebus_device(nd, dev);

		while ((nd = prom_getsibling(nd))) {
			dev->next = ebus_alloc(sizeof(struct linux_ebus_device));

			dev = dev->next;
			dev->next = 0;
			dev->children = 0;
			dev->bus = ebus;
			fill_ebus_device(nd, dev);
		}

	next_ebus:
		printk("\n");

		pdev = find_next_ebus(pdev, &is_rio);
		if (!pdev)
			break;

		cookie = pdev->sysdata;
		ebusnd = cookie->prom_node;

		ebus->next = ebus_alloc(sizeof(struct linux_ebus));
		ebus = ebus->next;
		ebus->next = 0;
		ebus->is_rio = is_rio;
		++num_ebus;
	}

#ifdef CONFIG_SUN_AUXIO
	auxio_probe();
#endif
}
