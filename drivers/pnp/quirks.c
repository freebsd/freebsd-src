/*
 *  This file contains quirk handling code for ISAPnP devices
 *  Some devices do not report all their resources, and need to have extra
 *  resources added. This is most easily accomplished at initialisation time
 *  when building up the resource structure for the first time.
 *
 *  Copyright (c) 2000 Peter Denison <peterd@pnd-pc.demon.co.uk>
 *
 *  Heavily based on PCI quirks handling which is
 *
 *  Copyright (c) 1999 Martin Mares <mj@ucw.cz>
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/isapnp.h>
#include <linux/string.h>

#if 0
#define ISAPNP_DEBUG
#endif

static void __init quirk_awe32_resources(struct pci_dev *dev)
{
	struct isapnp_port *port, *port2, *port3;
	struct isapnp_resources *res = dev->sysdata;

	/*
	 * Unfortunately the isapnp_add_port_resource is too tightly bound
	 * into the PnP discovery sequence, and cannot be used. Link in the
	 * two extra ports (at offset 0x400 and 0x800 from the one given) by
	 * hand.
	 */
	for ( ; res ; res = res->alt ) {
		port2 = isapnp_alloc(sizeof(struct isapnp_port));
		port3 = isapnp_alloc(sizeof(struct isapnp_port));
		if (!port2 || !port3)
			return;
		port = res->port;
		memcpy(port2, port, sizeof(struct isapnp_port));
		memcpy(port3, port, sizeof(struct isapnp_port));
		port->next = port2;
		port2->next = port3;
		port2->min += 0x400;
		port2->max += 0x400;
		port3->min += 0x800;
		port3->max += 0x800;
	}
	printk(KERN_INFO "isapnp: AWE32 quirk - adding two ports\n");
}

static void __init quirk_cmi8330_resources(struct pci_dev *dev)
{
	struct isapnp_resources *res = dev->sysdata;

	for ( ; res ; res = res->alt ) {

		struct isapnp_irq *irq;
		struct isapnp_dma *dma;
	
		for( irq = res->irq; irq; irq = irq->next )	// Valid irqs are 5, 7, 10
			irq->map = 0x04A0;						// 0000 0100 1010 0000

		for( dma = res->dma; dma; dma = dma->next ) // Valid 8bit dma channels are 1,3
			if( ( dma->flags & IORESOURCE_DMA_TYPE_MASK ) == IORESOURCE_DMA_8BIT )
				dma->map = 0x000A;
	}
	printk(KERN_INFO "isapnp: CMI8330 quirk - fixing interrupts and dma\n");
}

static void __init quirk_sb16audio_resources(struct pci_dev *dev)
{
	struct isapnp_port *port;
	struct isapnp_resources *res = dev->sysdata;
	int    changed = 0;

	/* 
	 * The default range on the mpu port for these devices is 0x388-0x388.
	 * Here we increase that range so that two such cards can be
	 * auto-configured.
	 */
	
	for( ; res ; res = res->alt ) {
		port = res->port;
		if(!port)
			continue;
		port = port->next;
		if(!port)
			continue;
		port = port->next;
		if(!port)
			continue;
		if(port->min != port->max)
			continue;
		port->max += 0x70;
		changed = 1;
	}
	if(changed)
		printk(KERN_INFO "isapnp: SB audio device quirk - increasing port range\n");
	return;
}

/*
 *  ISAPnP Quirks
 *  Cards or devices that need some tweaking due to broken hardware
 */

static struct isapnp_fixup isapnp_fixups[] __initdata = {
	/* Soundblaster awe io port quirk */
	{ ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0021),
		quirk_awe32_resources },
	{ ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0022),
		quirk_awe32_resources },
	{ ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0023),
		quirk_awe32_resources },
	/* CMI 8330 interrupt and dma fix */
	{ ISAPNP_VENDOR('@','X','@'), ISAPNP_DEVICE(0x0001),
		quirk_cmi8330_resources },
	/* Soundblaster audio device io port range quirk */
	{ ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0001),
		quirk_sb16audio_resources },
	{ ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0031),
		quirk_sb16audio_resources },
	{ ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0041),
		quirk_sb16audio_resources },
	{ ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0042),
		quirk_sb16audio_resources },
	{ ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0043),
		quirk_sb16audio_resources },
	{ ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0044),
		quirk_sb16audio_resources },
	{ ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0045),
		quirk_sb16audio_resources },
	{ 0 }
};

void isapnp_fixup_device(struct pci_dev *dev)
{
	int i = 0;

	while (isapnp_fixups[i].vendor != 0) {
		if ((isapnp_fixups[i].vendor == dev->vendor) &&
		    (isapnp_fixups[i].device == dev->device)) {
#ifdef ISAPNP_DEBUG
			printk(KERN_DEBUG "isapnp: Calling quirk for %02x:%02x\n",
			       dev->bus->number, dev->devfn);
#endif
			isapnp_fixups[i].quirk_function(dev);
		}
		i++;
	}
}

