/*
 * arch/ppc/platforms/pplus_pci.c
 *
 * PCI setup for MCG PowerPlus
 *
 * Author: Randy Vinson <rvinson@mvista.com>
 *
 * Derived from original PowerPlus PReP work by
 * Cort Dougan, Johnnie Peters, Matt Porter, and
 * Troy Benjegerdes.
 *
 * Copyright 2001 MontaVista Software Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <asm/sections.h>
#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/ptrace.h>
#include <asm/pci-bridge.h>
#include <asm/residual.h>
#include <asm/processor.h>
#include <asm/irq.h>
#include <asm/machdep.h>

#include <asm/open_pic.h>
#include <asm/pplus.h>

unsigned char *Motherboard_map_name;

/* Tables for known hardware */

/* Motorola Mesquite */
static inline int
mesquite_map_irq(struct pci_dev *dev, unsigned char idsel, unsigned char pin)
{
	static char pci_irq_table[][4] =
	/*
	 * 	MPIC interrupts for various IDSEL values (MPIC IRQ0 =
	 * 	Linux IRQ16 (to leave room for ISA IRQs at 0-15).
	 *      PCI IDSEL/INTPIN->INTLINE
	 *         A   B   C   D
	 */
	{
		{ 18,  0,  0,  0 },     /* IDSEL 14 - Enet 0 */
		{  0,  0,  0,  0 },     /* IDSEL 15 - unused */
		{ 19, 19, 19, 19 },     /* IDSEL 16 - PMC Slot 1 */
		{  0,  0,  0,  0 },     /* IDSEL 17 - unused */
		{  0,  0,  0,  0 },     /* IDSEL 18 - unused */
		{  0,  0,  0,  0 },     /* IDSEL 19 - unused */
		{ 24, 25, 26, 27 },     /* IDSEL 20 - P2P bridge (to cPCI 1) */
		{  0,  0,  0,  0 },     /* IDSEL 21 - unused */
		{ 28, 29, 30, 31 }      /* IDSEL 22 - P2P bridge (to cPCI 2) */
	};

	const long min_idsel = 14, max_idsel = 22, irqs_per_slot = 4;
	return PCI_IRQ_TABLE_LOOKUP;
}

/* Motorola Sitka */
static inline int
sitka_map_irq(struct pci_dev *dev, unsigned char idsel, unsigned char pin)
{
	static char pci_irq_table[][4] =
	/*
	 * 	MPIC interrupts for various IDSEL values (MPIC IRQ0 =
	 * 	Linux IRQ16 (to leave room for ISA IRQs at 0-15).
	 *      PCI IDSEL/INTPIN->INTLINE
	 *         A   B   C   D
	 */
	{
		{ 18,  0,  0,  0 },     /* IDSEL 14 - Enet 0 */
		{  0,  0,  0,  0 },     /* IDSEL 15 - unused */
		{ 25, 26, 27, 28 },     /* IDSEL 16 - PMC Slot 1 */
		{ 28, 25, 26, 27 },     /* IDSEL 17 - PMC Slot 2 */
		{  0,  0,  0,  0 },     /* IDSEL 18 - unused */
		{  0,  0,  0,  0 },     /* IDSEL 19 - unused */
		{ 20,  0,  0,  0 }      /* IDSEL 20 - P2P bridge (to cPCI) */
	};

	const long min_idsel = 14, max_idsel = 20, irqs_per_slot = 4;
	return PCI_IRQ_TABLE_LOOKUP;
}

/* Motorola MTX */
static inline int
MTX_map_irq(struct pci_dev *dev, unsigned char idsel, unsigned char pin)
{
	static char pci_irq_table[][4] =
	/*
	 * 	MPIC interrupts for various IDSEL values (MPIC IRQ0 =
	 * 	Linux IRQ16 (to leave room for ISA IRQs at 0-15).
	 *      PCI IDSEL/INTPIN->INTLINE
	 *         A   B   C   D
	 */
	{
		{ 19,  0,  0,  0 },     /* IDSEL 12 - SCSI   */
		{ 0,   0,  0,  0 },     /* IDSEL 13 - unused */
		{ 18,  0,  0,  0 },     /* IDSEL 14 - Enet   */
		{  0,  0,  0,  0 },     /* IDSEL 15 - unused */
		{ 25, 26, 27, 28 },     /* IDSEL 16 - PMC Slot 1 */
		{ 26, 27, 28, 25 },     /* IDSEL 17 - PMC Slot 2 */
		{ 27, 28, 25, 26 }      /* IDSEL 18 - PCI Slot 3 */
	};

	const long min_idsel = 12, max_idsel = 18, irqs_per_slot = 4;
	return PCI_IRQ_TABLE_LOOKUP;
}

/* Motorola MTX Plus */
/* Secondary bus interrupt routing is not supported yet */
static inline int
MTXplus_map_irq(struct pci_dev *dev, unsigned char idsel, unsigned char pin)
{
	static char pci_irq_table[][4] =
	/*
	 * 	MPIC interrupts for various IDSEL values (MPIC IRQ0 =
	 * 	Linux IRQ16 (to leave room for ISA IRQs at 0-15).
	 *      PCI IDSEL/INTPIN->INTLINE
	 *         A   B   C   D
	 */
	{
		{ 19,  0,  0,  0 },     /* IDSEL 12 - SCSI   */
		{ 0,   0,  0,  0 },     /* IDSEL 13 - unused */
		{ 18,  0,  0,  0 },     /* IDSEL 14 - Enet 1 */
		{  0,  0,  0,  0 },     /* IDSEL 15 - unused */
		{ 25, 26, 27, 28 },     /* IDSEL 16 - PCI Slot 1P */
		{ 26, 27, 28, 25 },     /* IDSEL 17 - PCI Slot 2P */
		{ 27, 28, 25, 26 },     /* IDSEL 18 - PCI Slot 3P */
		{ 26,  0,  0,  0 },	/* IDSEL 19 - Enet 2 */
		{  0,  0,  0,  0 } 	/* IDSEL 20 - P2P Bridge */
	};

	const long min_idsel = 12, max_idsel = 20, irqs_per_slot = 4;
	return PCI_IRQ_TABLE_LOOKUP;
}

static inline int
Genesis2_map_irq(struct pci_dev *dev, unsigned char idsel, unsigned char pin)
{
	/* 2600
	 * Raven 31
	 * ISA   11
	 * SCSI	 12 - IRQ3
	 * Univ  13
	 * eth   14 - IRQ2
	 * VGA   15 - IRQ4
	 * PMC1  16 - IRQ9,10,11,12 = PMC1 A-D
	 * PMC2  17 - IRQ12,9,10,11 = A-D
	 * SCSI2 18 - IRQ11
	 * eth2  19 - IRQ10
	 * PCIX  20 - IRQ9,10,11,12 = PCI A-D
	 */

	/* 2400
	 * Hawk 31
	 * ISA	11
	 * Univ 13
	 * eth  14 - IRQ2
	 * PMC1 16 - IRQ9,10,11,12 = PMC A-D
	 * PMC2 17 - IRQ12,9,10,11 = PMC A-D
	 * PCIX 20 - IRQ9,10,11,12 = PMC A-D
	 */

	/* 2300
	 * Raven 31
	 * ISA   11
	 * Univ  13
	 * eth   14 - IRQ2
	 * PMC1  16 - 9,10,11,12 = A-D
	 * PMC2  17 - 9,10,11,12 = B,C,D,A
	 */

	static char pci_irq_table[][4] =
	/*
	 * 	MPIC interrupts for various IDSEL values (MPIC IRQ0 =
	 * 	Linux IRQ16 (to leave room for ISA IRQs at 0-15).
	 *      PCI IDSEL/INTPIN->INTLINE
	 *         A   B   C   D
	 */
	{
		{ 19,  0,  0,  0 },     /* IDSEL 12 - SCSI   */
		{ 0,   0,  0,  0 },     /* IDSEL 13 - Universe PCI - VME */
		{ 18,  0,  0,  0 },     /* IDSEL 14 - Enet 1 */
		{  0,  0,  0,  0 },     /* IDSEL 15 - unused */
		{ 25, 26, 27, 28 },     /* IDSEL 16 - PCI/PMC Slot 1P */
		{ 28, 25, 26, 27 },     /* IDSEL 17 - PCI/PMC Slot 2P */
		{ 27, 28, 25, 26 },     /* IDSEL 18 - PCI Slot 3P */
		{ 26,  0,  0,  0 },	/* IDSEL 19 - Enet 2 */
		{ 25, 26, 27, 28 } 	/* IDSEL 20 - P2P Bridge */
	};

	const long min_idsel = 12, max_idsel = 20, irqs_per_slot = 4;
	return PCI_IRQ_TABLE_LOOKUP;
}

#define MOTOROLA_CPUTYPE_REG	0x800
#define MOTOROLA_BASETYPE_REG	0x803
#define MPIC_RAVEN_ID		0x48010000
#define	MPIC_HAWK_ID		0x48030000
#define	MOT_PROC2_BIT		0x800

static u_char pplus_openpic_initsenses[] __initdata = {
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),/* MVME2600_INT_SIO */
	(IRQ_SENSE_EDGE  | IRQ_POLARITY_NEGATIVE),/*MVME2600_INT_FALCN_ECC_ERR*/
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),/*MVME2600_INT_PCI_ETHERNET */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),/* MVME2600_INT_PCI_SCSI */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),/* MVME2600_INT_PCI_GRAPHICS*/
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),/* MVME2600_INT_PCI_VME0 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),/* MVME2600_INT_PCI_VME1 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),/* MVME2600_INT_PCI_VME2 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),/* MVME2600_INT_PCI_VME3 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),/* MVME2600_INT_PCI_INTA */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),/* MVME2600_INT_PCI_INTB */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),/* MVME2600_INT_PCI_INTC */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),/* MVME2600_INT_PCI_INTD */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),/* MVME2600_INT_LM_SIG0 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),/* MVME2600_INT_LM_SIG1 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),
};

int mot_entry = -1;
int prep_keybd_present = 1;
int mot_multi = 0;

int __init raven_init(void)
{
	unsigned short	devid;
	unsigned char	base_mod;

	/* set the MPIC base address */
	early_write_config_dword(0, 0, 0, PCI_BASE_ADDRESS_1, 0x3cfc0000);

	pplus_mpic_init(PREP_ISA_MEM_BASE);

	OpenPIC_InitSenses = pplus_openpic_initsenses;
	OpenPIC_NumInitSenses = sizeof(pplus_openpic_initsenses);

	ppc_md.get_irq = openpic_get_irq;

	/* This is a hack.  If this is a 2300 or 2400 mot board then there is
	 * no keyboard controller and we have to indicate that.
	 */

	early_read_config_word(0, 0, 0, PCI_VENDOR_ID, &devid);
	base_mod = inb(MOTOROLA_BASETYPE_REG);
	if ((devid == PCI_DEVICE_ID_MOTOROLA_HAWK) ||
	    (base_mod == 0xF9) ||
	    (base_mod == 0xFA) || (base_mod == 0xE1))
		prep_keybd_present = 0;

	return 1;
}

struct brd_info {
	int		cpu_type;	/* 0x100 mask assumes for Raven and Hawk boards that the level/edge are set */
					/* 0x200 if this board has a Hawk chip. */
	int		base_type;
	int		max_cpu;	/* ored with 0x80 if this board should be checked for multi CPU */
	const char	*name;
	int		(*map_irq)(struct pci_dev *,
			           unsigned char,
				   unsigned char);
};
struct brd_info mot_info[] = {
	{0x300, 0x00, 0x00, "MVME 2400", Genesis2_map_irq},
	{0x1E0, 0xE0, 0x00, "Mesquite cPCI (MCP750)", mesquite_map_irq},
	{0x1E0, 0xE1, 0x00, "Sitka cPCI (MCPN750)", sitka_map_irq},
	{0x1E0, 0xE2, 0x00, "Mesquite cPCI (MCP750) w/ HAC", mesquite_map_irq},
	{0x1E0, 0xF6, 0x80, "MTX Plus",	MTXplus_map_irq},
	{0x1E0, 0xF6, 0x81, "Dual MTX Plus", MTXplus_map_irq},
	{0x1E0, 0xF7, 0x80, "MTX wo/ Parallel Port", MTX_map_irq},
	{0x1E0, 0xF7, 0x81, "Dual MTX wo/ Parallel Port", MTX_map_irq},
	{0x1E0, 0xF8, 0x80, "MTX w/ Parallel Port", MTX_map_irq},
	{0x1E0, 0xF8, 0x81, "Dual MTX w/ Parallel Port", MTX_map_irq},
	{0x1E0, 0xF9, 0x00, "MVME 2300", Genesis2_map_irq},
	{0x1E0, 0xFA, 0x00, "MVME 2300SC/2600",	Genesis2_map_irq},
	{0x1E0, 0xFB, 0x00, "MVME 2600 with MVME712M", Genesis2_map_irq},
	{0x1E0, 0xFC, 0x00, "MVME 2600/2700 with MVME761", Genesis2_map_irq},
	{0x1E0, 0xFD, 0x80, "MVME 3600 with MVME712M", Genesis2_map_irq},
	{0x1E0, 0xFD, 0x81, "MVME 4600 with MVME712M", Genesis2_map_irq},
	{0x1E0, 0xFE, 0x80, "MVME 3600 with MVME761", Genesis2_map_irq},
	{0x1E0, 0xFE, 0x81, "MVME 4600 with MVME761", Genesis2_map_irq},
	{0x000, 0x00, 0x00, "",	NULL}
};

void __init pplus_set_board_type(void)
{
	unsigned char  cpu_type;
	unsigned char  base_mod;
	int	       entry;
	unsigned short devid;
	unsigned long  *ProcInfo = NULL;

	cpu_type = inb(MOTOROLA_CPUTYPE_REG) & 0xF0;
	base_mod = inb(MOTOROLA_BASETYPE_REG);
	early_read_config_word(0, 0, 0, PCI_VENDOR_ID, &devid);

	for (entry = 0; mot_info[entry].cpu_type != 0; entry++) {

		/* Check for Hawk chip */
		if (mot_info[entry].cpu_type & 0x200) {
			if (devid != PCI_DEVICE_ID_MOTOROLA_HAWK)
				continue;
		} else {
			/* store the system config register for later use. */
			ProcInfo = (unsigned long *)ioremap(0xfef80400, 4);

			/* Check non hawk boards */
			if ((mot_info[entry].cpu_type & 0xff) != cpu_type)
				continue;

			if (mot_info[entry].base_type == 0) {
				mot_entry = entry;
				break;
			}

			if (mot_info[entry].base_type != base_mod)
				continue;
		}

		if (!(mot_info[entry].max_cpu & 0x80)) {
			mot_entry = entry;
			break;
		}

		/* processor 1 not present and max processor zero indicated */
		if ((*ProcInfo & MOT_PROC2_BIT) && !(mot_info[entry].max_cpu & 0x7f)) {
			mot_entry = entry;
			break;
		}

		/* processor 1 present and max processor zero indicated */
		if (!(*ProcInfo & MOT_PROC2_BIT) && (mot_info[entry].max_cpu & 0x7f)) {
			mot_entry = entry;
			break;
		}

		/* Indicate to system if this is a multiprocessor board */
		if (!(*ProcInfo & MOT_PROC2_BIT)) {
			mot_multi = 1;
						        }
	}

	if (mot_entry == -1)

		/* No particular cpu type found - assume Mesquite (MCP750) */
		mot_entry = 1;

	Motherboard_map_name = (unsigned char *)mot_info[mot_entry].name;
	ppc_md.pci_map_irq = mot_info[mot_entry].map_irq;
}
void __init
pplus_pib_init(void)
{
	unsigned char   reg;
	unsigned short  short_reg;

	struct pci_dev *dev = NULL;

	/*
	 * Perform specific configuration for the Via Tech or
	 * or Winbond PCI-ISA-Bridge part.
	 */
	if ((dev = pci_find_device(PCI_VENDOR_ID_VIA,
					PCI_DEVICE_ID_VIA_82C586_1, dev))) {
		/*
		 * PPCBUG does not set the enable bits
		 * for the IDE device. Force them on here.
		 */
		pci_read_config_byte(dev, 0x40, &reg);

		reg |= 0x03; /* IDE: Chip Enable Bits */
		pci_write_config_byte(dev, 0x40, reg);
	}
	if ((dev = pci_find_device(PCI_VENDOR_ID_VIA,
					PCI_DEVICE_ID_VIA_82C586_2,
					dev)) && (dev->devfn = 0x5a)) {
		/* Force correct USB interrupt */
		dev->irq = 11;
		pci_write_config_byte(dev,
				PCI_INTERRUPT_LINE,
				dev->irq);
	}
	if ((dev = pci_find_device(PCI_VENDOR_ID_WINBOND,
					PCI_DEVICE_ID_WINBOND_83C553, dev))) {
		/* Clear PCI Interrupt Routing Control Register. */
		short_reg = 0x0000;
		pci_write_config_word(dev, 0x44, short_reg);
		/* Route IDE interrupts to IRQ 14 */
		reg = 0xEE;
		pci_write_config_byte(dev, 0x43, reg);
	}

	if ((dev = pci_find_device(PCI_VENDOR_ID_WINBOND,
					PCI_DEVICE_ID_WINBOND_82C105, dev))){
		/*
		 * Disable LEGIRQ mode so PCI INTS are routed
		 * directly to the 8259 and enable both channels
		 */
		pci_write_config_dword(dev, 0x40, 0x10ff0033);

		/* Force correct IDE interrupt */
		dev->irq = 14;
		pci_write_config_byte(dev,
				PCI_INTERRUPT_LINE,
				dev->irq);
	}
}

void __init
pplus_set_VIA_IDE_legacy(void)
{
	unsigned short vend, dev;

	early_read_config_word(0, 0, PCI_DEVFN(0xb, 1), PCI_VENDOR_ID, &vend);
	early_read_config_word(0, 0, PCI_DEVFN(0xb, 1), PCI_DEVICE_ID, &dev);

	if ((vend == PCI_VENDOR_ID_VIA) &&
	    (dev == PCI_DEVICE_ID_VIA_82C586_1)) {

		unsigned char temp;

		/* put back original "standard" port base addresses */
		early_write_config_dword(0, 0, PCI_DEVFN(0xb, 1),
				         PCI_BASE_ADDRESS_0, 0x1f1);
		early_write_config_dword(0, 0, PCI_DEVFN(0xb, 1),
				         PCI_BASE_ADDRESS_1, 0x3f5);
		early_write_config_dword(0, 0, PCI_DEVFN(0xb, 1),
				         PCI_BASE_ADDRESS_2, 0x171);
		early_write_config_dword(0, 0, PCI_DEVFN(0xb, 1),
				         PCI_BASE_ADDRESS_3, 0x375);
		early_write_config_dword(0, 0, PCI_DEVFN(0xb, 1),
				         PCI_BASE_ADDRESS_4, 0xcc01);

		/* put into legacy mode */
		early_read_config_byte(0, 0, PCI_DEVFN(0xb, 1), PCI_CLASS_PROG,
				       &temp);
		temp &= ~0x05;
		early_write_config_byte(0, 0, PCI_DEVFN(0xb, 1), PCI_CLASS_PROG,
					temp);
	}
}

void
pplus_set_VIA_IDE_native(void)
{
	unsigned short vend, dev;

	early_read_config_word(0, 0, PCI_DEVFN(0xb, 1), PCI_VENDOR_ID, &vend);
	early_read_config_word(0, 0, PCI_DEVFN(0xb, 1), PCI_DEVICE_ID, &dev);

	if ((vend == PCI_VENDOR_ID_VIA) &&
	    (dev == PCI_DEVICE_ID_VIA_82C586_1)) {

		unsigned char temp;

		/* put into native mode */
		early_read_config_byte(0, 0, PCI_DEVFN(0xb, 1), PCI_CLASS_PROG,
				       &temp);
		temp |= 0x05;
		early_write_config_byte(0, 0, PCI_DEVFN(0xb, 1), PCI_CLASS_PROG,
					temp);
	}
}

void __init
pplus_pcibios_fixup(void)
{

	printk("Setting PCI interrupts for a \"%s\"\n", Motherboard_map_name);

	/* Setup the Winbond or Via PIB */
	pplus_pib_init();
}

static void __init
pplus_init_resource(struct resource *res, unsigned long start,
		   unsigned long end, int flags)
{
	res->flags = flags;
	res->start = start;
	res->end = end;
	res->name = "PCI host bridge";
	res->parent = NULL;
	res->sibling = NULL;
	res->child = NULL;
}

void __init
pplus_setup_hose(void)
{
	struct pci_controller* hose;

	hose = pcibios_alloc_controller();
	if (!hose)
		return;

	hose->first_busno = 0;
	hose->last_busno = 0xff;

	hose->pci_mem_offset = PREP_ISA_MEM_BASE;
	hose->io_base_virt = (void *)PREP_ISA_IO_BASE;

	pplus_init_resource(&hose->io_resource, 0x00000000, 0x0fffffff,
			    IORESOURCE_IO);
	pplus_init_resource(&hose->mem_resources[0], 0xc0000000, 0xfdffffff,
			    IORESOURCE_MEM);

	hose->io_space.start  = 0x00000000;
	hose->io_space.end    = 0x0fffffff;
	hose->mem_space.start = 0x00000000;
	hose->mem_space.end   = 0x3cfbffff; /* MPIC at 0x3cfc0000-0x3dffffff */

	setup_indirect_pci(hose, 0x80000cf8, 0x80000cfc);

	pplus_set_VIA_IDE_legacy();

	hose->last_busno = pciauto_bus_scan(hose, hose->first_busno);

	ppc_md.pcibios_fixup = pplus_pcibios_fixup;
	ppc_md.pci_swizzle = common_swizzle;
	pplus_set_board_type();
}

