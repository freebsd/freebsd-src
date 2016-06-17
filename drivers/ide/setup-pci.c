/*
 *  linux/drivers/ide/setup-pci.c		Version 1.10	2002/08/19
 *
 *  Copyright (c) 1998-2000  Andre Hedrick <andre@linux-ide.org>
 *
 *  Copyright (c) 1995-1998  Mark Lord
 *  May be copied or modified under the terms of the GNU General Public License
 *
 *  Recent Changes
 *	Split the set up function into multiple functions
 *	Use pci_set_master
 *	Fix misreporting of I/O v MMIO problems
 *	Initial fixups for simplex devices
 */

/*
 *  This module provides support for automatic detection and
 *  configuration of all PCI IDE interfaces present in a system.  
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/ide.h>

#include <asm/io.h>
#include <asm/irq.h>


/**
 *	ide_match_hwif	-	match a PCI IDE against an ide_hwif
 *	@io_base: I/O base of device
 *	@bootable: set if its bootable
 *	@name: name of device
 *
 *	Match a PCI IDE port against an entry in ide_hwifs[],
 *	based on io_base port if possible. Return the matching hwif,
 *	or a new hwif. If we find an error (clashing, out of devices, etc)
 *	return NULL
 *
 *	FIXME: we need to handle mmio matches here too
 */

static ide_hwif_t *ide_match_hwif(unsigned long io_base, u8 bootable, const char *name)
{
	int h;
	ide_hwif_t *hwif;

	/*
	 * Look for a hwif with matching io_base specified using
	 * parameters to ide_setup().
	 */
	for (h = 0; h < MAX_HWIFS; ++h) {
		hwif = &ide_hwifs[h];
		if (hwif->io_ports[IDE_DATA_OFFSET] == io_base) {
			if (hwif->chipset == ide_generic)
				return hwif; /* a perfect match */
		}
	}
	/*
	 * Look for a hwif with matching io_base default value.
	 * If chipset is "ide_unknown", then claim that hwif slot.
	 * Otherwise, some other chipset has already claimed it..  :(
	 */
	for (h = 0; h < MAX_HWIFS; ++h) {
		hwif = &ide_hwifs[h];
		if (hwif->io_ports[IDE_DATA_OFFSET] == io_base) {
			if (hwif->chipset == ide_unknown)
				return hwif; /* match */
			printk(KERN_ERR "%s: port 0x%04lx already claimed by %s\n",
				name, io_base, hwif->name);
			return NULL;	/* already claimed */
		}
	}
	/*
	 * Okay, there is no hwif matching our io_base,
	 * so we'll just claim an unassigned slot.
	 * Give preference to claiming other slots before claiming ide0/ide1,
	 * just in case there's another interface yet-to-be-scanned
	 * which uses ports 1f0/170 (the ide0/ide1 defaults).
	 *
	 * Unless there is a bootable card that does not use the standard
	 * ports 1f0/170 (the ide0/ide1 defaults). The (bootable) flag.
	 */
	if (bootable) {
		for (h = 0; h < MAX_HWIFS; ++h) {
			hwif = &ide_hwifs[h];
			if (hwif->chipset == ide_unknown)
				return hwif;	/* pick an unused entry */
		}
	} else {
		for (h = 2; h < MAX_HWIFS; ++h) {
			hwif = ide_hwifs + h;
			if (hwif->chipset == ide_unknown)
				return hwif;	/* pick an unused entry */
		}
	}
	for (h = 0; h < 2; ++h) {
		hwif = ide_hwifs + h;
		if (hwif->chipset == ide_unknown)
			return hwif;	/* pick an unused entry */
	}
	printk(KERN_ERR "%s: too many IDE interfaces, no room in table\n", name);
	return NULL;
}

/**
 *	ide_setup_pci_baseregs	-	place a PCI IDE controller native
 *	@dev: PCI device of interface to switch native
 *	@name: Name of interface
 *
 *	We attempt to place the PCI interface into PCI native mode. If
 *	we succeed the BARs are ok and the controller is in PCI mode.
 *	Returns 0 on success or an errno code. 
 *
 *	FIXME: if we program the interface and then fail to set the BARS
 *	we don't switch it back to legacy mode. Do we actually care ??
 */
 
static int ide_setup_pci_baseregs (struct pci_dev *dev, const char *name)
{
	u8 progif = 0;

	/*
	 * Place both IDE interfaces into PCI "native" mode:
	 */
	if (pci_read_config_byte(dev, PCI_CLASS_PROG, &progif) ||
			 (progif & 5) != 5) {
		if ((progif & 0xa) != 0xa) {
			printk(KERN_INFO "%s: device not capable of full "
				"native PCI mode\n", name);
			return -EOPNOTSUPP;
		}
		printk(KERN_INFO "%s: placing both ports into native PCI mode\n", name);
		(void) pci_write_config_byte(dev, PCI_CLASS_PROG, progif|5);
		if (pci_read_config_byte(dev, PCI_CLASS_PROG, &progif) ||
		    (progif & 5) != 5) {
			printk(KERN_ERR "%s: rewrite of PROGIF failed, wanted "
				"0x%04x, got 0x%04x\n",
				name, progif|5, progif);
			return -EOPNOTSUPP;
		}
	}
	return 0;
}

#ifdef CONFIG_BLK_DEV_IDEDMA_FORCED
/*
 * Long lost data from 2.0.34 that is now in 2.0.39
 *
 * This was used in ./drivers/block/triton.c to do DMA Base address setup
 * when PnP failed.  Oh the things we forget.  I believe this was part
 * of SFF-8038i that has been withdrawn from public access... :-((
 */
#define DEFAULT_BMIBA	0xe800	/* in case BIOS did not init it */
#define DEFAULT_BMCRBA	0xcc00	/* VIA's default value */
#define DEFAULT_BMALIBA	0xd400	/* ALI's default value */
#endif /* CONFIG_BLK_DEV_IDEDMA_FORCED */

/**
 *	ide_get_or_set_dma_base		-	setup BMIBA
 *	@hwif: Interface
 *
 *	Fetch the DMA Bus-Master-I/O-Base-Address (BMIBA) from PCI space:
 *	If need be we set up the DMA base. Where a device has a partner that
 *	is already in DMA mode we check and enforce IDE simplex rules.
 */

static unsigned long ide_get_or_set_dma_base (ide_hwif_t *hwif)
{
	unsigned long	dma_base = 0;
	struct pci_dev	*dev = hwif->pci_dev;

#ifdef CONFIG_BLK_DEV_IDEDMA_FORCED
	int second_chance = 0;

second_chance_to_dma:
#endif /* CONFIG_BLK_DEV_IDEDMA_FORCED */

	if (hwif->mmio && hwif->dma_base)
		return hwif->dma_base;
	else if (hwif->mate && hwif->mate->dma_base) {
		dma_base = hwif->mate->dma_base - (hwif->channel ? 0 : 8);
	} else {
		dma_base = (hwif->mmio) ?
			((unsigned long) hwif->hwif_data) :
			(pci_resource_start(dev, 4));
		if (!dma_base) {
			printk(KERN_ERR "%s: dma_base is invalid (0x%04lx)\n",
				hwif->cds->name, dma_base);
			dma_base = 0;
		}
	}

#ifdef CONFIG_BLK_DEV_IDEDMA_FORCED
	/* FIXME - should use pci_assign_resource surely */
	if ((!dma_base) && (!second_chance)) {
		unsigned long set_bmiba = 0;
		second_chance++;
		switch(dev->vendor) {
			case PCI_VENDOR_ID_AL:
				set_bmiba = DEFAULT_BMALIBA; break;
			case PCI_VENDOR_ID_VIA:
				set_bmiba = DEFAULT_BMCRBA; break;
			case PCI_VENDOR_ID_INTEL:
				set_bmiba = DEFAULT_BMIBA; break;
			default:
				return dma_base;
		}
		pci_write_config_dword(dev, 0x20, set_bmiba|1);
		goto second_chance_to_dma;
	}
#endif /* CONFIG_BLK_DEV_IDEDMA_FORCED */

	if (dma_base) {
		u8 simplex_stat = 0;
		dma_base += hwif->channel ? 8 : 0;

		switch(dev->device) {
			case PCI_DEVICE_ID_AL_M5219:
			case PCI_DEVICE_ID_AL_M5229:
			case PCI_DEVICE_ID_AMD_VIPER_7409:
			case PCI_DEVICE_ID_CMD_643:
			case PCI_DEVICE_ID_SERVERWORKS_CSB5IDE:
				simplex_stat = hwif->INB(dma_base + 2);
				hwif->OUTB((simplex_stat&0x60),(dma_base + 2));
				simplex_stat = hwif->INB(dma_base + 2);
				if (simplex_stat & 0x80) {
					printk(KERN_INFO "%s: simplex device: "
						"DMA forced\n",
						hwif->cds->name);
				}
				break;
			default:
				/*
				 * If the device claims "simplex" DMA,
				 * this means only one of the two interfaces
				 * can be trusted with DMA at any point in time.
				 * So we should enable DMA only on one of the
				 * two interfaces.
				 */
				simplex_stat = hwif->INB(dma_base + 2);
				if (simplex_stat & 0x80) {
					/* simplex device? */
#if 0					
/*
 *	At this point we haven't probed the drives so we can't make the
 *	appropriate decision. Really we should defer this problem
 *	until we tune the drive then try to grab DMA ownership if we want
 *	to be the DMA end. This has to be become dynamic to handle hot
 *	plug.
 */
					/* Don't enable DMA on a simplex channel with no drives */
					if (!hwif->drives[0].present && !hwif->drives[1].present)
					{
						printk(KERN_INFO "%s: simplex device with no drives: DMA disabled\n",
								hwif->cds->name);
						dma_base = 0;
					}
					/* If our other channel has DMA then we cannot */
					else 
#endif					
					if(hwif->mate && hwif->mate->dma_base) 
					{
						printk(KERN_INFO "%s: simplex device: "
							"DMA disabled\n",
							hwif->cds->name);
						dma_base = 0;
					}
				}
		}
	}
	return dma_base;
}

static void ide_setup_pci_noise (struct pci_dev *dev, ide_pci_device_t *d)
{
	if ((d->vendor != dev->vendor) && (d->device != dev->device)) {
		printk(KERN_INFO "%s: unknown IDE controller at PCI slot "
			"%s, VID=%04x, DID=%04x\n",
			d->name, dev->slot_name, dev->vendor, dev->device);
        } else {
		printk(KERN_INFO "%s: IDE controller at PCI slot %s\n",
			d->name, dev->slot_name);
	}
}

/**
 *	ide_pci_enable	-	do PCI enables
 *	@dev: PCI device
 *	@d: IDE pci device data
 *
 *	Enable the IDE PCI device. We attempt to enable the device in full
 *	but if that fails then we only need BAR4 so we will enable that.
 *	
 *	Returns zero on success or an error code
 */
 
static int ide_pci_enable(struct pci_dev *dev, ide_pci_device_t *d)
{
	
	if (pci_enable_device(dev)) {
		if (pci_enable_device_bars(dev, 1 << 4)) {
			printk(KERN_WARNING "%s: (ide_setup_pci_device:) "
				"Could not enable device.\n", d->name);
			return -EBUSY;
		} else
			printk(KERN_WARNING "%s: Not fully BIOS configured!\n", d->name);
	}

	/*
	 * assume all devices can do 32-bit dma for now. we can add a
	 * dma mask field to the ide_pci_device_t if we need it (or let
	 * lower level driver set the dma mask)
	 */
	if (pci_set_dma_mask(dev, 0xffffffff)) {
		printk(KERN_ERR "%s: can't set dma mask\n", d->name);
		return -EBUSY;
	}
	 
	/* FIXME: Temporary - until we put in the hotplug interface logic
	   Check that the bits we want are not in use by someone else */
	if (pci_request_region(dev, 4, "ide_tmp"))
		return -EBUSY;
	pci_release_region(dev, 4);
	
	return 0;	
}

/**
 *	ide_pci_configure	-	configure an unconfigured device
 *	@dev: PCI device
 *	@d: IDE pci device data
 *
 *	Enable and configure the PCI device we have been passed.
 *	Returns zero on success or an error code.
 */
 
static int ide_pci_configure(struct pci_dev *dev, ide_pci_device_t *d)
{
	u16 pcicmd = 0;
	/*
	 * PnP BIOS was *supposed* to have setup this device, but we
	 * can do it ourselves, so long as the BIOS has assigned an IRQ
	 * (or possibly the device is using a "legacy header" for IRQs).
	 * Maybe the user deliberately *disabled* the device,
	 * but we'll eventually ignore it again if no drives respond.
	 */
	if (ide_setup_pci_baseregs(dev, d->name) || pci_write_config_word(dev, PCI_COMMAND, pcicmd|PCI_COMMAND_IO)) 
	{
		printk(KERN_INFO "%s: device disabled (BIOS)\n", d->name);
		return -ENODEV;
	}
	if (pci_read_config_word(dev, PCI_COMMAND, &pcicmd)) {
		printk(KERN_ERR "%s: error accessing PCI regs\n", d->name);
		return -EIO;
	}
	if (!(pcicmd & PCI_COMMAND_IO)) {
		printk(KERN_ERR "%s: unable to enable IDE controller\n", d->name);
		return -ENXIO;
	}
	return 0;
}

/**
 *	ide_pci_check_iomem	-	check a register is I/O
 *	@dev: pci device
 *	@d: ide_pci_device
 *	@bar: bar number
 *
 *	Checks if a BAR is configured and points to MMIO space. If so
 *	print an error and return an error code. Otherwise return 0
 */
 
static int ide_pci_check_iomem(struct pci_dev *dev, ide_pci_device_t *d, int bar)
{
	ulong flags = pci_resource_flags(dev, bar);
	
	/* Unconfigured ? */
	if (!flags || pci_resource_len(dev, bar) == 0)
		return 0;

	/* I/O space */		
	if(flags & PCI_BASE_ADDRESS_IO_MASK)
		return 0;
		
	/* Bad */
	printk(KERN_ERR "%s: IO baseregs (BIOS) are reported "
			"as MEM, report to "
			"<andre@linux-ide.org>.\n", d->name);
	return -EINVAL;
}
	
		
/**
 *	ide_hwif_configure	-	configure an IDE interface
 *	@dev: PCI device holding interface
 *	@d: IDE pci data
 *	@mate: Paired interface if any
 *
 *	Perform the initial set up for the hardware interface structure. This
 *	is done per interface port rather than per PCI device. There may be
 *	more than one port per device.
 *
 *	Returns the new hardware interface structure, or NULL on a failure
 */
 
static ide_hwif_t *ide_hwif_configure(struct pci_dev *dev, ide_pci_device_t *d, ide_hwif_t *mate, int port, int irq)
{
	unsigned long ctl = 0, base = 0;
	ide_hwif_t *hwif;

	/*  Possibly we should fail if these checks report true */
	ide_pci_check_iomem(dev, d, 2*port);
	ide_pci_check_iomem(dev, d, 2*port+1);
 
	ctl  = pci_resource_start(dev, 2*port+1);
	base = pci_resource_start(dev, 2*port);
	if ((ctl && !base) || (base && !ctl)) {
		printk(KERN_ERR "%s: inconsistent baseregs (BIOS) "
			"for port %d, skipping\n", d->name, port);
		return NULL;
	}
	if (!ctl)
	{
		/* Use default values */
		ctl = port ? 0x374 : 0x3f4;
		base = port ? 0x170 : 0x1f0;
	}
	if ((hwif = ide_match_hwif(base, d->bootable, d->name)) == NULL)
		return NULL;	/* no room in ide_hwifs[] */
	if (hwif->io_ports[IDE_DATA_OFFSET] != base) {
fixup_address:
		ide_init_hwif_ports(&hwif->hw, base, (ctl | 2), NULL);
		memcpy(hwif->io_ports, hwif->hw.io_ports, sizeof(hwif->io_ports));
		hwif->noprobe = !hwif->io_ports[IDE_DATA_OFFSET];
	} else if (hwif->io_ports[IDE_CONTROL_OFFSET] != (ctl | 2)) {
		goto fixup_address;
	}
	hwif->chipset = ide_pci;
	hwif->pci_dev = dev;
	hwif->cds = (struct ide_pci_device_s *) d;
	hwif->channel = port;

	if (!hwif->irq)
		hwif->irq = irq;
	if (mate) {
		hwif->mate = mate;
		mate->mate = hwif;
	}
	return hwif;
}

/**
 *	ide_hwif_setup_dma	-	configure DMA interface
 *	@dev: PCI device
 *	@d: IDE pci data
 *	@hwif: Hardware interface we are configuring
 *
 *	Set up the DMA base for the interface. Enable the master bits as
 *	neccessary and attempt to bring the device DMA into a ready to use
 *	state
 */
 
static void ide_hwif_setup_dma(struct pci_dev *dev, ide_pci_device_t *d, ide_hwif_t *hwif)
{
	u16 pcicmd;
	pci_read_config_word(dev, PCI_COMMAND, &pcicmd);

	if ((d->autodma == AUTODMA) ||
	    ((dev->class >> 8) == PCI_CLASS_STORAGE_IDE &&
	     (dev->class & 0x80))) {
		unsigned long dma_base = ide_get_or_set_dma_base(hwif);
		if (dma_base && !(pcicmd & PCI_COMMAND_MASTER)) {
			/*
 			 * Set up BM-DMA capability
			 * (PnP BIOS should have done this)
 			 */
			if (!((d->device == PCI_DEVICE_ID_CYRIX_5530_IDE && d->vendor == PCI_VENDOR_ID_CYRIX)
			    ||(d->device == PCI_DEVICE_ID_NS_SCx200_IDE && d->vendor == PCI_VENDOR_ID_NS)))
			{
				/*
				 * default DMA off if we had to
				 * configure it here
				 */
				hwif->autodma = 0;
			}
			pci_set_master(dev);
			if (pci_read_config_word(dev, PCI_COMMAND, &pcicmd) || !(pcicmd & PCI_COMMAND_MASTER)) {
				printk(KERN_ERR "%s: %s error updating PCICMD\n",
					hwif->name, d->name);
				dma_base = 0;
			}
		}
		if (dma_base) {
			if (d->init_dma) {
				d->init_dma(hwif, dma_base);
			} else {
				ide_setup_dma(hwif, dma_base, 8);
			}
		} else {
			printk(KERN_INFO "%s: %s Bus-Master DMA disabled "
				"(BIOS)\n", hwif->name, d->name);
		}
	}
}

/**
 *	ide_setup_pci_controller	-	set up IDE PCI
 *	@dev: PCI device
 *	@d: IDE PCI data
 *	@noisy: verbose flag
 *	@config: returned as 1 if we configured the hardware
 *
 *	Set up the PCI and controller side of the IDE interface. This brings
 *	up the PCI side of the device, checks that the device is enabled
 *	and enables it if need be
 */
 
static int ide_setup_pci_controller(struct pci_dev *dev, ide_pci_device_t *d, int noisy, int *config)
{
	int ret = 0;
	u32 class_rev;
	u16 pcicmd;

	if (!noautodma)
		ret = 1;

	if (noisy)
		ide_setup_pci_noise(dev, d);

	if (ide_pci_enable(dev, d))
		return -EBUSY;
		
	if (pci_read_config_word(dev, PCI_COMMAND, &pcicmd)) {
		printk(KERN_ERR "%s: error accessing PCI regs\n", d->name);
		return -EIO;
	}
	if (!(pcicmd & PCI_COMMAND_IO)) {	/* is device disabled? */
		if (ide_pci_configure(dev, d))
			return -ENODEV;
		/* default DMA off if we had to configure it here */
		ret = 0;
		*config = 1;
		printk(KERN_INFO "%s: device enabled (Linux)\n", d->name);
	}

	pci_read_config_dword(dev, PCI_CLASS_REVISION, &class_rev);
	class_rev &= 0xff;
	if (noisy)
		printk(KERN_INFO "%s: chipset revision %d\n", d->name, class_rev);
	return ret;
}

/*
 * ide_setup_pci_device() looks at the primary/secondary interfaces
 * on a PCI IDE device and, if they are enabled, prepares the IDE driver
 * for use with them.  This generic code works for most PCI chipsets.
 *
 * One thing that is not standardized is the location of the
 * primary/secondary interface "enable/disable" bits.  For chipsets that
 * we "know" about, this information is in the ide_pci_device_t struct;
 * for all other chipsets, we just assume both interfaces are enabled.
 */
static ata_index_t do_ide_setup_pci_device (struct pci_dev *dev, ide_pci_device_t *d, u8 noisy)
{
	u32 port, at_least_one_hwif_enabled = 0, autodma = 0;
	unsigned int pciirq = 0;
	int tried_config = 0;
	int drive0_tune, drive1_tune;
	ata_index_t index;
	u8 tmp = 0;
	ide_hwif_t *hwif, *mate = NULL;
	static int secondpdc = 0;

	index.all = 0xf0f0;

	if((autodma = ide_setup_pci_controller(dev, d, noisy, &tried_config)) < 0)
		return index;

	/*
	 * Can we trust the reported IRQ?
	 */
	pciirq = dev->irq;
	
	if ((dev->class & ~(0xfa)) != ((PCI_CLASS_STORAGE_IDE << 8) | 5)) {
		if (noisy)
			printk(KERN_INFO "%s: not 100%% native mode: "
				"will probe irqs later\n", d->name);
		/*
		 * This allows offboard ide-pci cards the enable a BIOS,
		 * verify interrupt settings of split-mirror pci-config
		 * space, place chipset into init-mode, and/or preserve
		 * an interrupt if the card is not native ide support.
		 */
		pciirq = (d->init_chipset) ? d->init_chipset(dev, d->name) : 0;
	} else if (tried_config) {
		if (noisy)
			printk(KERN_INFO "%s: will probe irqs later\n", d->name);
		pciirq = 0;
	} else if (!pciirq) {
		if (noisy)
			printk(KERN_WARNING "%s: bad irq (%d): will probe later\n",
				d->name, pciirq);
		pciirq = 0;
	} else {
		if (d->init_chipset)
			d->init_chipset(dev, d->name);

		if (noisy)
#ifdef __sparc__
			printk(KERN_INFO "%s: 100%% native mode on irq %s\n",
			       d->name, __irq_itoa(pciirq));
#else
			printk(KERN_INFO "%s: 100%% native mode on irq %d\n",
				d->name, pciirq);
#endif
	}
	
	/*
	 * Set up the IDE ports
	 */
	 
	for (port = 0; port <= 1; ++port) {
		ide_pci_enablebit_t *e = &(d->enablebits[port]);
	
		/* 
		 * If this is a Promise FakeRaid controller,
		 * the 2nd controller will be marked as 
		 * disabled while it is actually there and enabled
		 * by the bios for raid purposes. 
		 * Skip the normal "is it enabled" test for those.
		 */
		if (((d->vendor == PCI_VENDOR_ID_PROMISE) &&
		     ((d->device == PCI_DEVICE_ID_PROMISE_20262) ||
		      (d->device == PCI_DEVICE_ID_PROMISE_20265))) &&
		    (secondpdc++==1) && (port==1))
			goto controller_ok;
			
		if (e->reg && (pci_read_config_byte(dev, e->reg, &tmp) ||
		    (tmp & e->mask) != e->val))
			continue;	/* port not enabled */
controller_ok:

		if (d->channels	<= port)
			return index;
	
		if ((hwif = ide_hwif_configure(dev, d, mate, port, pciirq)) == NULL)
			continue;

		/* setup proper ancestral information */
		// 2.5 only hwif->gendev.parent = &dev->dev;

		if (hwif->channel) {
			index.b.high = hwif->index;
		} else {
			index.b.low = hwif->index;
		}

		
		if (d->init_iops)
			d->init_iops(hwif);

		if (d->autodma == NODMA)
			goto bypass_legacy_dma;
		if (d->autodma == NOAUTODMA)
			autodma = 0;
		if (autodma)
			hwif->autodma = 1;
		ide_hwif_setup_dma(dev, d, hwif);
bypass_legacy_dma:

		drive0_tune = hwif->drives[0].autotune;
		drive1_tune = hwif->drives[1].autotune;

		if (d->init_hwif)
			/* Call chipset-specific routine
			 * for each enabled hwif
			 */
			d->init_hwif(hwif);

		mate = hwif;
		at_least_one_hwif_enabled = 1;
	}
	if (!at_least_one_hwif_enabled)
		printk(KERN_INFO "%s: neither IDE port enabled (BIOS)\n", d->name);
	return index;
}

void ide_setup_pci_device (struct pci_dev *dev, ide_pci_device_t *d)
{
	do_ide_setup_pci_device(dev, d, 1);
}

EXPORT_SYMBOL_GPL(ide_setup_pci_device);

void ide_setup_pci_devices (struct pci_dev *dev, struct pci_dev *dev2, ide_pci_device_t *d)
{
	do_ide_setup_pci_device(dev, d, 1);
	do_ide_setup_pci_device(dev2, d, 0);
}

EXPORT_SYMBOL_GPL(ide_setup_pci_devices);

/*
 *	Module interfaces
 */
 
static int pre_init = 1;		/* Before first ordered IDE scan */
static LIST_HEAD(ide_pci_drivers);

/*
 *	ide_register_pci_driver		-	attach IDE driver
 *	@driver: pci driver
 *
 *	Registers a driver with the IDE layer. The IDE layer arranges that
 *	boot time setup is done in the expected device order and then 
 *	hands the controllers off to the core PCI code to do the rest of
 *	the work.
 *
 *	The driver_data of the driver table must point to an ide_pci_device_t
 *	describing the interface.
 *
 *	Returns are the same as for pci_register_driver
 */

int ide_pci_register_driver(struct pci_driver *driver)
{
	if(!pre_init)
		return pci_module_init(driver);
	list_add_tail(&driver->node, &ide_pci_drivers);
	return 0;
}

EXPORT_SYMBOL_GPL(ide_pci_register_driver);

/**
 *	ide_unregister_pci_driver	-	unregister an IDE driver
 *	@driver: driver to remove
 *
 *	Unregister a currently installed IDE driver. Returns are the same
 *	as for pci_unregister_driver
 */
 
void ide_pci_unregister_driver(struct pci_driver *driver)
{
	if(!pre_init)
		pci_unregister_driver(driver);
	else
		list_del(&driver->node);
}

EXPORT_SYMBOL_GPL(ide_pci_unregister_driver);

/**
 *	ide_scan_pcidev		-	find an IDE driver for a device
 *	@dev: PCI device to check
 *
 *	Look for an IDE driver to handle the device we are considering.
 *	This is only used during boot up to get the ordering correct. After
 *	boot up the pci layer takes over the job.
 */
 
static int __init ide_scan_pcidev(struct pci_dev *dev)
{
	struct list_head *l;
	struct pci_driver *d;
	
	list_for_each(l, &ide_pci_drivers)
	{
		d = list_entry(l, struct pci_driver, node);
		if(d->id_table)
		{
			const struct pci_device_id *id = pci_match_device(d->id_table, dev);
			if(id != NULL)
			{
				if(d->probe(dev, id) >= 0)
				{
					dev->driver = d;
					return 1;
				}
			}
		}
	}
	return 0;
}

/**
 *	ide_scan_pcibus		-	perform the initial IDE driver scan
 *	@scan_direction: set for reverse order scanning
 *
 *	Perform the initial bus rather than driver ordered scan of the
 *	PCI drivers. After this all IDE pci handling becomes standard
 *	module ordering not traditionally ordered.
 */
 	
void __init ide_scan_pcibus (int scan_direction)
{
	struct pci_dev *dev;
	struct pci_driver *d;
	struct list_head *l, *n;

	pre_init = 0;
	if (!scan_direction) {
		pci_for_each_dev(dev) {
			ide_scan_pcidev(dev);
		}
	} else {
		pci_for_each_dev_reverse(dev) {
			ide_scan_pcidev(dev);
		}
	}
	
	/*
	 *	Hand the drivers over to the PCI layer now we
	 *	are post init.
	 */

	list_for_each_safe(l, n, &ide_pci_drivers)
	{
		list_del(l);
		d = list_entry(l, struct pci_driver, node);
		pci_register_driver(d);
	}
}
