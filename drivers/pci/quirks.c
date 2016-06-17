/*
 * $Id: quirks.c,v 1.5 1998/05/02 19:24:14 mj Exp $
 *
 *  This file contains work-arounds for many known PCI hardware
 *  bugs.  Devices present only on certain architectures (host
 *  bridges et cetera) should be handled in arch-specific code.
 *
 *  Copyright (c) 1999 Martin Mares <mj@ucw.cz>
 *
 *  The bridge optimization stuff has been removed. If you really
 *  have a silly BIOS which is unable to set your host bridge right,
 *  use the PowerTweak utility (see http://powertweak.sourceforge.net).
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/delay.h>

#undef DEBUG

/* Deal with broken BIOS'es that neglect to enable passive release,
   which can cause problems in combination with the 82441FX/PPro MTRRs */
static void __init quirk_passive_release(struct pci_dev *dev)
{
	struct pci_dev *d = NULL;
	unsigned char dlc;

	/* We have to make sure a particular bit is set in the PIIX3
	   ISA bridge, so we have to go out and find it. */
	while ((d = pci_find_device(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371SB_0, d))) {
		pci_read_config_byte(d, 0x82, &dlc);
		if (!(dlc & 1<<1)) {
			printk(KERN_ERR "PCI: PIIX3: Enabling Passive Release on %s\n", d->slot_name);
			dlc |= 1<<1;
			pci_write_config_byte(d, 0x82, dlc);
		}
	}
}

/*  The VIA VP2/VP3/MVP3 seem to have some 'features'. There may be a workaround
    but VIA don't answer queries. If you happen to have good contacts at VIA
    ask them for me please -- Alan 
    
    This appears to be BIOS not version dependent. So presumably there is a 
    chipset level fix */
    

int isa_dma_bridge_buggy;		/* Exported */
    
static void __init quirk_isa_dma_hangs(struct pci_dev *dev)
{
	if (!isa_dma_bridge_buggy) {
		isa_dma_bridge_buggy=1;
		printk(KERN_INFO "Activating ISA DMA hang workarounds.\n");
	}
}

int pci_pci_problems;

/*
 *	Chipsets where PCI->PCI transfers vanish or hang
 */

static void __init quirk_nopcipci(struct pci_dev *dev)
{
	if((pci_pci_problems&PCIPCI_FAIL)==0)
	{
		printk(KERN_INFO "Disabling direct PCI/PCI transfers.\n");
		pci_pci_problems|=PCIPCI_FAIL;
	}
}

/*
 *	Triton requires workarounds to be used by the drivers
 */
 
static void __init quirk_triton(struct pci_dev *dev)
{
	if((pci_pci_problems&PCIPCI_TRITON)==0)
	{
		printk(KERN_INFO "Limiting direct PCI/PCI transfers.\n");
		pci_pci_problems|=PCIPCI_TRITON;
	}
}

/*
 *	VIA Apollo KT133 needs PCI latency patch
 *	Made according to a windows driver based patch by George E. Breese
 *	see PCI Latency Adjust on http://www.viahardware.com/download/viatweak.shtm
 *      Also see http://www.au-ja.org/review-kt133a-1-en.phtml for the info on which 
 *	Mr Breese based his work.
 *
 *	Updated based on further information from the site and also on
 *	information provided by VIA 
 */
static void __init quirk_vialatency(struct pci_dev *dev)
{
	struct pci_dev *p;
	u8 rev;
	u8 busarb;
	/* Ok we have a potential problem chipset here. Now see if we have
	   a buggy southbridge */
	   
	p=pci_find_device(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_82C686, NULL);
	if(p!=NULL)
	{
		pci_read_config_byte(p, PCI_CLASS_REVISION, &rev);
		/* 0x40 - 0x4f == 686B, 0x10 - 0x2f == 686A; thanks Dan Hollis */
		/* Check for buggy part revisions */
		if (rev < 0x40 || rev > 0x42) 
			return;
	}
	else
	{
		p = pci_find_device(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_8231, NULL);
		if(p==NULL)	/* No problem parts */
			return;
		pci_read_config_byte(p, PCI_CLASS_REVISION, &rev);
		/* Check for buggy part revisions */
		if (rev < 0x10 || rev > 0x12) 
			return;
	}
	
	/*
	 *	Ok we have the problem. Now set the PCI master grant to 
	 *	occur every master grant. The apparent bug is that under high
	 *	PCI load (quite common in Linux of course) you can get data
	 *	loss when the CPU is held off the bus for 3 bus master requests
	 *	This happens to include the IDE controllers....
	 *
	 *	VIA only apply this fix when an SB Live! is present but under
	 *	both Linux and Windows this isnt enough, and we have seen
	 *	corruption without SB Live! but with things like 3 UDMA IDE
	 *	controllers. So we ignore that bit of the VIA recommendation..
	 */

	pci_read_config_byte(dev, 0x76, &busarb);
	/* Set bit 4 and bi 5 of byte 76 to 0x01 
	   "Master priority rotation on every PCI master grant */
	busarb &= ~(1<<5);
	busarb |= (1<<4);
	pci_write_config_byte(dev, 0x76, busarb);
	printk(KERN_INFO "Applying VIA southbridge workaround.\n");
}

/*
 *	VIA Apollo VP3 needs ETBF on BT848/878
 */
 
static void __init quirk_viaetbf(struct pci_dev *dev)
{
	if((pci_pci_problems&PCIPCI_VIAETBF)==0)
	{
		printk(KERN_INFO "Limiting direct PCI/PCI transfers.\n");
		pci_pci_problems|=PCIPCI_VIAETBF;
	}
}
static void __init quirk_vsfx(struct pci_dev *dev)
{
	if((pci_pci_problems&PCIPCI_VSFX)==0)
	{
		printk(KERN_INFO "Limiting direct PCI/PCI transfers.\n");
		pci_pci_problems|=PCIPCI_VSFX;
	}
}

/*
 *	Ali Magik requires workarounds to be used by the drivers
 *	that DMA to AGP space. Latency must be set to 0xA and triton
 *	workaround applied too
 *	[Info kindly provided by ALi]
 */	
 
static void __init quirk_alimagik(struct pci_dev *dev)
{
	if((pci_pci_problems&PCIPCI_ALIMAGIK)==0)
	{
		printk(KERN_INFO "Limiting direct PCI/PCI transfers.\n");
		pci_pci_problems|=PCIPCI_ALIMAGIK|PCIPCI_TRITON;
	}
}

/*
 *	Natoma has some interesting boundary conditions with Zoran stuff
 *	at least
 */
 
static void __init quirk_natoma(struct pci_dev *dev)
{
	if((pci_pci_problems&PCIPCI_NATOMA)==0)
	{
		printk(KERN_INFO "Limiting direct PCI/PCI transfers.\n");
		pci_pci_problems|=PCIPCI_NATOMA;
	}
}

/*
 *  S3 868 and 968 chips report region size equal to 32M, but they decode 64M.
 *  If it's needed, re-allocate the region.
 */

static void __init quirk_s3_64M(struct pci_dev *dev)
{
	struct resource *r = &dev->resource[0];

	if ((r->start & 0x3ffffff) || r->end != r->start + 0x3ffffff) {
		r->start = 0;
		r->end = 0x3ffffff;
	}
}

static void __init quirk_io_region(struct pci_dev *dev, unsigned region, unsigned size, int nr)
{
	region &= ~(size-1);
	if (region) {
		struct resource *res = dev->resource + nr;

		res->name = dev->name;
		res->start = region;
		res->end = region + size - 1;
		res->flags = IORESOURCE_IO;
		pci_claim_resource(dev, nr);
	}
}	

/*
 *	ATI Northbridge setups MCE the processor if you even
 *	read somewhere between 0x3b0->0x3bb or read 0x3d3
 */
 
static void __devinit quirk_ati_exploding_mce(struct pci_dev *dev)
{
	printk(KERN_INFO "ATI Northbridge, reserving I/O ports 0x3b0 to 0x3bb.\n");
	request_region(0x3b0, 0x0C, "RadeonIGP");
	request_region(0x3d3, 0x01, "RadeonIGP");
}

/*
 * Let's make the southbridge information explicit instead
 * of having to worry about people probing the ACPI areas,
 * for example.. (Yes, it happens, and if you read the wrong
 * ACPI register it will put the machine to sleep with no
 * way of waking it up again. Bummer).
 *
 * ALI M7101: Two IO regions pointed to by words at
 *	0xE0 (64 bytes of ACPI registers)
 *	0xE2 (32 bytes of SMB registers)
 */
static void __init quirk_ali7101_acpi(struct pci_dev *dev)
{
	u16 region;

	pci_read_config_word(dev, 0xE0, &region);
	quirk_io_region(dev, region, 64, PCI_BRIDGE_RESOURCES);
	pci_read_config_word(dev, 0xE2, &region);
	quirk_io_region(dev, region, 32, PCI_BRIDGE_RESOURCES+1);
}

/*
 * PIIX4 ACPI: Two IO regions pointed to by longwords at
 *	0x40 (64 bytes of ACPI registers)
 *	0x90 (32 bytes of SMB registers)
 */
static void __init quirk_piix4_acpi(struct pci_dev *dev)
{
	u32 region;

	pci_read_config_dword(dev, 0x40, &region);
	quirk_io_region(dev, region, 64, PCI_BRIDGE_RESOURCES);
	pci_read_config_dword(dev, 0x90, &region);
	quirk_io_region(dev, region, 32, PCI_BRIDGE_RESOURCES+1);
}

/*
 * ICH4, ICH4-M, ICH5, ICH5-M ACPI: Three IO regions pointed to by longwords at
 *	0x40 (128 bytes of ACPI, GPIO & TCO registers)
 *	0x58 (64 bytes of GPIO I/O space)
 */
static void __devinit quirk_ich4_lpc_acpi(struct pci_dev *dev)
{
	u32 region;

	pci_read_config_dword(dev, 0x40, &region);
	quirk_io_region(dev, region, 128, PCI_BRIDGE_RESOURCES);

	pci_read_config_dword(dev, 0x58, &region);
	quirk_io_region(dev, region, 64, PCI_BRIDGE_RESOURCES+1);
}

/*
 * VIA ACPI: One IO region pointed to by longword at
 *	0x48 or 0x20 (256 bytes of ACPI registers)
 */
static void __init quirk_vt82c586_acpi(struct pci_dev *dev)
{
	u8 rev;
	u32 region;

	pci_read_config_byte(dev, PCI_CLASS_REVISION, &rev);
	if (rev & 0x10) {
		pci_read_config_dword(dev, 0x48, &region);
		region &= PCI_BASE_ADDRESS_IO_MASK;
		quirk_io_region(dev, region, 256, PCI_BRIDGE_RESOURCES);
	}
}

/*
 * VIA VT82C686 ACPI: Three IO region pointed to by (long)words at
 *	0x48 (256 bytes of ACPI registers)
 *	0x70 (128 bytes of hardware monitoring register)
 *	0x90 (16 bytes of SMB registers)
 */
static void __init quirk_vt82c686_acpi(struct pci_dev *dev)
{
	u16 hm;
	u32 smb;

	quirk_vt82c586_acpi(dev);

	pci_read_config_word(dev, 0x70, &hm);
	hm &= PCI_BASE_ADDRESS_IO_MASK;
	quirk_io_region(dev, hm, 128, PCI_BRIDGE_RESOURCES + 1);

	pci_read_config_dword(dev, 0x90, &smb);
	smb &= PCI_BASE_ADDRESS_IO_MASK;
	quirk_io_region(dev, smb, 16, PCI_BRIDGE_RESOURCES + 2);
}


#ifdef CONFIG_X86_IO_APIC 
extern int nr_ioapics;

/*
 * VIA 686A/B: If an IO-APIC is active, we need to route all on-chip
 * devices to the external APIC.
 *
 * TODO: When we have device-specific interrupt routers,
 * this code will go away from quirks.
 */
static void __init quirk_via_ioapic(struct pci_dev *dev)
{
	u8 tmp;
	
	if (nr_ioapics < 1)
		tmp = 0;    /* nothing routed to external APIC */
	else
		tmp = 0x1f; /* all known bits (4-0) routed to external APIC */
		
	printk(KERN_INFO "PCI: %sbling Via external APIC routing\n",
	       tmp == 0 ? "Disa" : "Ena");

	/* Offset 0x58: External APIC IRQ output control */
	pci_write_config_byte (dev, 0x58, tmp);
}

#endif /* CONFIG_X86_IO_APIC */


/*
 * Via 686A/B:  The PCI_INTERRUPT_LINE register for the on-chip
 * devices, USB0/1, AC97, MC97, and ACPI, has an unusual feature:
 * when written, it makes an internal connection to the PIC.
 * For these devices, this register is defined to be 4 bits wide.
 * Normally this is fine.  However for IO-APIC motherboards, or
 * non-x86 architectures (yes Via exists on PPC among other places),
 * we must mask the PCI_INTERRUPT_LINE value versus 0xf to get
 * interrupts delivered properly.
 *
 * TODO: When we have device-specific interrupt routers,
 * quirk_via_irqpic will go away from quirks.
 */

/*
 * FIXME: it is questionable that quirk_via_acpi
 * is needed.  It shows up as an ISA bridge, and does not
 * support the PCI_INTERRUPT_LINE register at all.  Therefore
 * it seems like setting the pci_dev's 'irq' to the
 * value of the ACPI SCI interrupt is only done for convenience.
 *	-jgarzik
 */
static void __init quirk_via_acpi(struct pci_dev *d)
{
	/*
	 * VIA ACPI device: SCI IRQ line in PCI config byte 0x42
	 */
	u8 irq;
	pci_read_config_byte(d, 0x42, &irq);
	irq &= 0xf;
	if (irq && (irq != 2))
		d->irq = irq;
}

static void __init quirk_via_irqpic(struct pci_dev *dev)
{
	u8 irq, new_irq = dev->irq & 0xf;

	pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &irq);

	if (new_irq != irq) {
		printk(KERN_INFO "PCI: Via IRQ fixup for %s, from %d to %d\n",
		       dev->slot_name, irq, new_irq);

		udelay(15);
		pci_write_config_byte(dev, PCI_INTERRUPT_LINE, new_irq);
	}
}


/*
 * PIIX3 USB: We have to disable USB interrupts that are
 * hardwired to PIRQD# and may be shared with an
 * external device.
 *
 * Legacy Support Register (LEGSUP):
 *     bit13:  USB PIRQ Enable (USBPIRQDEN),
 *     bit4:   Trap/SMI On IRQ Enable (USBSMIEN).
 *
 * We mask out all r/wc bits, too.
 */
static void __init quirk_piix3_usb(struct pci_dev *dev)
{
	u16 legsup;

	pci_read_config_word(dev, 0xc0, &legsup);
	legsup &= 0x50ef;
	pci_write_config_word(dev, 0xc0, legsup);
}

/*
 * VIA VT82C598 has its device ID settable and many BIOSes
 * set it to the ID of VT82C597 for backward compatibility.
 * We need to switch it off to be able to recognize the real
 * type of the chip.
 */
static void __init quirk_vt82c598_id(struct pci_dev *dev)
{
	pci_write_config_byte(dev, 0xfc, 0);
	pci_read_config_word(dev, PCI_DEVICE_ID, &dev->device);
}

/*
 * CardBus controllers have a legacy base address that enables them
 * to respond as i82365 pcmcia controllers.  We don't want them to
 * do this even if the Linux CardBus driver is not loaded, because
 * the Linux i82365 driver does not (and should not) handle CardBus.
 */
static void __init quirk_cardbus_legacy(struct pci_dev *dev)
{
	if ((PCI_CLASS_BRIDGE_CARDBUS << 8) ^ dev->class)
		return;
	pci_write_config_dword(dev, PCI_CB_LEGACY_MODE_BASE, 0);
}

/*
 * The AMD io apic can hang the box when an apic irq is masked.
 * We check all revs >= B0 (yet not in the pre production!) as the bug
 * is currently marked NoFix
 *
 * We have multiple reports of hangs with this chipset that went away with
 * noapic specified. For the moment we assume its the errata. We may be wrong
 * of course. However the advice is demonstrably good even if so..
 */
 
static void __init quirk_amd_ioapic(struct pci_dev *dev)
{
	u8 rev;

	pci_read_config_byte(dev, PCI_REVISION_ID, &rev);
	if(rev >= 0x02)
	{
		printk(KERN_WARNING "I/O APIC: AMD Errata #22 may be present. In the event of instability try\n");
		printk(KERN_WARNING "        : booting with the \"noapic\" option.\n");
	}
}

/*
 * Following the PCI ordering rules is optional on the AMD762. I'm not
 * sure what the designers were smoking but let's not inhale...
 *
 * To be fair to AMD, it follows the spec by default, its BIOS people
 * who turn it off!
 */
 
static void __init quirk_amd_ordering(struct pci_dev *dev)
{
	u32 pcic;
	pci_read_config_dword(dev, 0x4C, &pcic);
	if((pcic&6)!=6)
	{
		pcic |= 6;
		printk(KERN_WARNING "BIOS failed to enable PCI standards compliance, fixing this error.\n");
		pci_write_config_dword(dev, 0x4C, pcic);
		pci_read_config_dword(dev, 0x84, &pcic);
		pcic |= (1<<23);	/* Required in this mode */
		pci_write_config_dword(dev, 0x84, pcic);
	}
}

#ifdef CONFIG_X86_IO_APIC

#define AMD8131_revA0        0x01
#define AMD8131_revB0        0x11
#define AMD8131_MISC         0x40
#define AMD8131_NIOAMODE_BIT 0

static void __init quirk_amd_8131_ioapic(struct pci_dev *dev) 
{ 
	unsigned char revid, tmp;
	
	if (nr_ioapics == 0) 
		return;

	pci_read_config_byte(dev, PCI_REVISION_ID, &revid);
	if (revid == AMD8131_revA0 || revid == AMD8131_revB0) {
		printk(KERN_INFO "Fixing up AMD8131 IOAPIC mode\n"); 
		pci_read_config_byte( dev, AMD8131_MISC, &tmp);
		tmp &= ~(1 << AMD8131_NIOAMODE_BIT);
		pci_write_config_byte( dev, AMD8131_MISC, tmp);
	}
} 
#endif


/*
 *	DreamWorks provided workaround for Dunord I-3000 problem
 *
 *	This card decodes and responds to addresses not apparently
 *	assigned to it. We force a larger allocation to ensure that
 *	nothing gets put too close to it.
 */

static void __init quirk_dunord ( struct pci_dev * dev )
{
	struct resource * r = & dev -> resource [ 1 ];
	r -> start = 0;
	r -> end = 0xffffff;
}

static void __init quirk_transparent_bridge(struct pci_dev *dev)
{
	dev->transparent = 1;
}

/*
 * Common misconfiguration of the MediaGX/Geode PCI master that will
 * reduce PCI bandwidth from 70MB/s to 25MB/s.  See the GXM/GXLV/GX1
 * datasheets found at http://www.national.com/ds/GX for info on what
 * these bits do.  <christer@weinigel.se>
 */
 
static void __init quirk_mediagx_master(struct pci_dev *dev)
{
	u8 reg;
	pci_read_config_byte(dev, 0x41, &reg);
	if (reg & 2) {
		reg &= ~2;
		printk(KERN_INFO "PCI: Fixup for MediaGX/Geode Slave Disconnect Boundary (0x41=0x%02x)\n", reg);
                pci_write_config_byte(dev, 0x41, reg);
	}
}

/*
 * As per PCI spec, ignore base address registers 0-3 of the IDE controllers
 * running in Compatible mode (bits 0 and 2 in the ProgIf for primary and
 * secondary channels respectively). If the device reports Compatible mode
 * but does use BAR0-3 for address decoding, we assume that firmware has
 * programmed these BARs with standard values (0x1f0,0x3f4 and 0x170,0x374).
 * Exceptions (if they exist) must be handled in chip/architecture specific
 * fixups.
 *
 * Note: for non x86 people. You may need an arch specific quirk to handle
 * moving IDE devices to native mode as well. Some plug in card devices power
 * up in compatible mode and assume the BIOS will adjust them.
 *
 * Q: should we load the 0x1f0,0x3f4 into the registers or zap them as
 * we do now ? We don't want is pci_enable_device to come along
 * and assign new resources. Both approaches work for that.
 */ 

static void __devinit quirk_ide_bases(struct pci_dev *dev)
{
       struct resource *res;
       int first_bar = 2, last_bar = 0;

       if ((dev->class >> 8) != PCI_CLASS_STORAGE_IDE)
               return;

       res = &dev->resource[0];

       /* primary channel: ProgIf bit 0, BAR0, BAR1 */
       if (!(dev->class & 1) && (res[0].flags || res[1].flags)) { 
               res[0].start = res[0].end = res[0].flags = 0;
               res[1].start = res[1].end = res[1].flags = 0;
               first_bar = 0;
               last_bar = 1;
       }

       /* secondary channel: ProgIf bit 2, BAR2, BAR3 */
       if (!(dev->class & 4) && (res[2].flags || res[3].flags)) { 
               res[2].start = res[2].end = res[2].flags = 0;
               res[3].start = res[3].end = res[3].flags = 0;
               last_bar = 3;
       }

       if (!last_bar)
               return;

       printk(KERN_INFO "PCI: Ignoring BAR%d-%d of IDE controller %s\n",
              first_bar, last_bar, dev->slot_name);
}

/*
 *	Ensure C0 rev restreaming is off. This is normally done by
 *	the BIOS but in the odd case it is not the results are corruption
 *	hence the presence of a Linux check
 */
 
static void __init quirk_disable_pxb(struct pci_dev *pdev)
{
	u16 config;
	u8 rev;
	
	pci_read_config_byte(pdev, PCI_REVISION_ID, &rev);
	if(rev != 0x04)		/* Only C0 requires this */
		return;
	pci_read_config_word(pdev, 0x40, &config);
	if(config & (1<<6))
	{
		config &= ~(1<<6);
		pci_write_config_word(pdev, 0x40, config);
		printk(KERN_INFO "PCI: C0 revision 450NX. Disabling PCI restreaming.\n");
	}
}

/*
 *	VIA northbridges care about PCI_INTERRUPT_LINE
 */
 
int interrupt_line_quirk;

static void __init quirk_via_bridge(struct pci_dev *pdev)
{
	if(pdev->devfn == 0)
		interrupt_line_quirk = 1;
}
	
/* 
 *	Serverworks CSB5 IDE does not fully support native mode
 */
static void __init quirk_svwks_csb5ide(struct pci_dev *pdev)
{
	u8 prog;
	pci_read_config_byte(pdev, PCI_CLASS_PROG, &prog);
	if (prog & 5) {
		prog &= ~5;
		pdev->class &= ~5;
		pci_write_config_byte(pdev, PCI_CLASS_PROG, prog);
		/* need to re-assign BARs for compat mode */
		quirk_ide_bases(pdev);
	}
}

/*
 * On ASUS P4B boards, the SMBus PCI Device within the ICH2/4 southbridge
 * is not activated. The myth is that Asus said that they do not want the
 * users to be irritated by just another PCI Device in the Win98 device
 * manager. (see the file prog/hotplug/README.p4b in the lm_sensors 
 * package 2.7.0 for details)
 *
 * The SMBus PCI Device can be activated by setting a bit in the ICH LPC 
 * bridge. Unfortunately, this device has no subvendor/subdevice ID. So it 
 * becomes necessary to do this tweak in two steps -- I've chosen the Host
 * bridge as trigger.
 */

static int __initdata asus_hides_smbus = 0;

static void __init asus_hides_smbus_hostbridge(struct pci_dev *dev)
{
	if (likely(dev->subsystem_vendor != PCI_VENDOR_ID_ASUSTEK))
		return;

	if (dev->device == PCI_DEVICE_ID_INTEL_82845_HB)
		switch(dev->subsystem_device) {
		case 0x8070: /* P4B */
	    	case 0x8088: /* P4B533 */
			asus_hides_smbus = 1;
		}
	if ((dev->device == PCI_DEVICE_ID_INTEL_82845G_HB) &&
	    (dev->subsystem_device == 0x80b2)) /* P4PE */
		asus_hides_smbus = 1;
	if ((dev->device == PCI_DEVICE_ID_INTEL_82850_HB) &&
	    (dev->subsystem_device == 0x8030)) /* P4T533 */
		asus_hides_smbus = 1;
	if ((dev->device == PCI_DEVICE_ID_INTEL_7205_0) &&
	    (dev->subsystem_device == 0x8070)) /* P4G8X Deluxe */
		asus_hides_smbus = 1;
	return;
}

static void __init asus_hides_smbus_lpc(struct pci_dev *dev)
{
	u16 val;
	
	if (likely(!asus_hides_smbus))
		return;

	pci_read_config_word(dev, 0xF2, &val);
	if (val & 0x8) {
		pci_write_config_word(dev, 0xF2, val & (~0x8));
		pci_read_config_word(dev, 0xF2, &val);
		if(val & 0x8)
			printk(KERN_INFO "PCI: i801 SMBus device continues to play 'hide and seek'! 0x%x\n", val);
		else
			printk(KERN_INFO "PCI: Enabled i801 SMBus device\n");
	}
}

/*
 *  The main table of quirks.
 */

static struct pci_fixup pci_fixups[] __initdata = {
	{ PCI_FIXUP_HEADER,	PCI_VENDOR_ID_DUNORD,	PCI_DEVICE_ID_DUNORD_I3000,	quirk_dunord },
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82441,	quirk_passive_release },
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82441,	quirk_passive_release },
	/*
	 * Its not totally clear which chipsets are the problematic ones
	 * We know 82C586 and 82C596 variants are affected.
	 */
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C586_0,	quirk_isa_dma_hangs },
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C596,	quirk_isa_dma_hangs },
	{ PCI_FIXUP_FINAL,      PCI_VENDOR_ID_INTEL,    PCI_DEVICE_ID_INTEL_82371SB_0,  quirk_isa_dma_hangs },
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82454NX,	quirk_disable_pxb },
	{ PCI_FIXUP_HEADER,	PCI_VENDOR_ID_S3,	PCI_DEVICE_ID_S3_868,		quirk_s3_64M },
	{ PCI_FIXUP_HEADER,	PCI_VENDOR_ID_S3,	PCI_DEVICE_ID_S3_968,		quirk_s3_64M },
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_INTEL, 	PCI_DEVICE_ID_INTEL_82437, 	quirk_triton }, 
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_INTEL, 	PCI_DEVICE_ID_INTEL_82437VX, 	quirk_triton }, 
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_INTEL, 	PCI_DEVICE_ID_INTEL_82439, 	quirk_triton }, 
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_INTEL, 	PCI_DEVICE_ID_INTEL_82439TX, 	quirk_triton }, 
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_INTEL, 	PCI_DEVICE_ID_INTEL_82441, 	quirk_natoma }, 
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_INTEL, 	PCI_DEVICE_ID_INTEL_82443LX_0, 	quirk_natoma }, 
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_INTEL, 	PCI_DEVICE_ID_INTEL_82443LX_1, 	quirk_natoma }, 
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_INTEL, 	PCI_DEVICE_ID_INTEL_82443BX_0, 	quirk_natoma }, 
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_INTEL, 	PCI_DEVICE_ID_INTEL_82443BX_1, 	quirk_natoma }, 
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_INTEL, 	PCI_DEVICE_ID_INTEL_82443BX_2, 	quirk_natoma },
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_AL, 	PCI_DEVICE_ID_AL_M1647, 	quirk_alimagik },
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_AL, 	PCI_DEVICE_ID_AL_M1651, 	quirk_alimagik },
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_5597,		quirk_nopcipci },
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_496,		quirk_nopcipci },
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8363_0,	quirk_vialatency },
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8371_1,	quirk_vialatency },
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8361,	quirk_vialatency },
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C576,	quirk_vsfx },
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C597_0,	quirk_viaetbf },
	{ PCI_FIXUP_HEADER,	PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C597_0,	quirk_vt82c598_id },
	{ PCI_FIXUP_HEADER,	PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C586_3,	quirk_vt82c586_acpi },
	{ PCI_FIXUP_HEADER,	PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C686_4,	quirk_vt82c686_acpi },
	{ PCI_FIXUP_HEADER,	PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82371AB_3,	quirk_piix4_acpi },
	{ PCI_FIXUP_HEADER,	PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801DB_12,	quirk_ich4_lpc_acpi },
	{ PCI_FIXUP_HEADER,	PCI_VENDOR_ID_AL,	PCI_DEVICE_ID_AL_M7101,		quirk_ali7101_acpi },
 	{ PCI_FIXUP_HEADER,	PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82371SB_2,	quirk_piix3_usb },
	{ PCI_FIXUP_HEADER,	PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82371AB_2,	quirk_piix3_usb },
	{ PCI_FIXUP_HEADER,     PCI_ANY_ID,             PCI_ANY_ID,                     quirk_ide_bases },
	{ PCI_FIXUP_HEADER,     PCI_VENDOR_ID_VIA,	PCI_ANY_ID,                     quirk_via_bridge },
	{ PCI_FIXUP_FINAL,	PCI_ANY_ID,		PCI_ANY_ID,			quirk_cardbus_legacy },

#ifdef CONFIG_X86_IO_APIC 
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C686,	quirk_via_ioapic },
#endif
	{ PCI_FIXUP_HEADER,	PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C586_3,	quirk_via_acpi },
	{ PCI_FIXUP_HEADER,	PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C686_4,	quirk_via_acpi },
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C586_2,	quirk_via_irqpic },
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C686_5,	quirk_via_irqpic },
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C686_6,	quirk_via_irqpic },

	{ PCI_FIXUP_FINAL, 	PCI_VENDOR_ID_AMD,	PCI_DEVICE_ID_AMD_VIPER_7410,	quirk_amd_ioapic },
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_AMD,	PCI_DEVICE_ID_AMD_FE_GATE_700C, quirk_amd_ordering },
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_ATI,	PCI_DEVICE_ID_ATI_RADEON_IGP,   quirk_ati_exploding_mce },
	/*
	 * i82380FB mobile docking controller: its PCI-to-PCI bridge
	 * is subtractive decoding (transparent), and does indicate this
	 * in the ProgIf. Unfortunately, the ProgIf value is wrong - 0x80
	 * instead of 0x01.
	 */
	{ PCI_FIXUP_HEADER,	PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82380FB,	quirk_transparent_bridge },
	{ PCI_FIXUP_HEADER,	PCI_VENDOR_ID_TOSHIBA,	0x605,				quirk_transparent_bridge },

	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_CYRIX,	PCI_DEVICE_ID_CYRIX_PCI_MASTER, quirk_mediagx_master },

	{ PCI_FIXUP_HEADER,	PCI_VENDOR_ID_SERVERWORKS, PCI_DEVICE_ID_SERVERWORKS_CSB5IDE, quirk_svwks_csb5ide },

#ifdef CONFIG_X86_IO_APIC
	{ PCI_FIXUP_FINAL,	PCI_VENDOR_ID_AMD,      PCI_DEVICE_ID_AMD_8131_APIC, 
	  quirk_amd_8131_ioapic }, 
#endif

	/*
	 * on Asus P4B boards, the i801SMBus device is disabled at startup.
	 */
	{ PCI_FIXUP_HEADER,	PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82845_HB,	asus_hides_smbus_hostbridge },
	{ PCI_FIXUP_HEADER,	PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82845G_HB,	asus_hides_smbus_hostbridge },
	{ PCI_FIXUP_HEADER,	PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82850_HB,	asus_hides_smbus_hostbridge },
	{ PCI_FIXUP_HEADER,	PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_7205_0,	asus_hides_smbus_hostbridge },
	{ PCI_FIXUP_HEADER,	PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801DB_0,	asus_hides_smbus_lpc },
	{ PCI_FIXUP_HEADER,	PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801BA_0,	asus_hides_smbus_lpc },

	{ 0 }
};


static void pci_do_fixups(struct pci_dev *dev, int pass, struct pci_fixup *f)
{
	while (f->pass) {
		if (f->pass == pass &&
 		    (f->vendor == dev->vendor || f->vendor == (u16) PCI_ANY_ID) &&
 		    (f->device == dev->device || f->device == (u16) PCI_ANY_ID)) {
#ifdef DEBUG
			printk(KERN_INFO "PCI: Calling quirk %p for %s\n", f->hook, dev->slot_name);
#endif
			f->hook(dev);
		}
		f++;
	}
}

void pci_fixup_device(int pass, struct pci_dev *dev)
{
	pci_do_fixups(dev, pass, pcibios_fixups);
	pci_do_fixups(dev, pass, pci_fixups);
}
