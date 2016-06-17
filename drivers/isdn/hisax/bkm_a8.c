/* $Id: bkm_a8.c,v 1.1.4.1 2001/11/20 14:19:35 kai Exp $
 *
 * low level stuff for Scitel Quadro (4*S0, passive)
 *
 * Author       Roland Klabunde
 * Copyright    by Roland Klabunde   <R.Klabunde@Berkom.de>
 * 
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#define __NO_VERSION__

#include <linux/config.h>
#include <linux/init.h>
#include "hisax.h"
#include "isac.h"
#include "ipac.h"
#include "hscx.h"
#include "isdnl1.h"
#include <linux/pci.h>
#include "bkm_ax.h"

#if CONFIG_PCI

#define	ATTEMPT_PCI_REMAPPING	/* Required for PLX rev 1 */

extern const char *CardType[];

const char sct_quadro_revision[] = "$Revision: 1.1.4.1 $";

static const char *sct_quadro_subtypes[] =
{
	"",
	"#1",
	"#2",
	"#3",
	"#4"
};


#define wordout(addr,val) outw(val,addr)
#define wordin(addr) inw(addr)

static inline u_char
readreg(unsigned int ale, unsigned int adr, u_char off)
{
	register u_char ret;
	long flags;
	save_flags(flags);
	cli();
	wordout(ale, off);
	ret = wordin(adr) & 0xFF;
	restore_flags(flags);
	return (ret);
}

static inline void
readfifo(unsigned int ale, unsigned int adr, u_char off, u_char * data, int size)
{
	/* fifo read without cli because it's allready done  */
	int i;
	wordout(ale, off);
	for (i = 0; i < size; i++)
		data[i] = wordin(adr) & 0xFF;
}


static inline void
writereg(unsigned int ale, unsigned int adr, u_char off, u_char data)
{
	long flags;
	save_flags(flags);
	cli();
	wordout(ale, off);
	wordout(adr, data);
	restore_flags(flags);
}

static inline void
writefifo(unsigned int ale, unsigned int adr, u_char off, u_char * data, int size)
{
	/* fifo write without cli because it's allready done  */
	int i;
	wordout(ale, off);
	for (i = 0; i < size; i++)
		wordout(adr, data[i]);
}

/* Interface functions */

static u_char
ReadISAC(struct IsdnCardState *cs, u_char offset)
{
	return (readreg(cs->hw.ax.base, cs->hw.ax.data_adr, offset | 0x80));
}

static void
WriteISAC(struct IsdnCardState *cs, u_char offset, u_char value)
{
	writereg(cs->hw.ax.base, cs->hw.ax.data_adr, offset | 0x80, value);
}

static void
ReadISACfifo(struct IsdnCardState *cs, u_char * data, int size)
{
	readfifo(cs->hw.ax.base, cs->hw.ax.data_adr, 0x80, data, size);
}

static void
WriteISACfifo(struct IsdnCardState *cs, u_char * data, int size)
{
	writefifo(cs->hw.ax.base, cs->hw.ax.data_adr, 0x80, data, size);
}


static u_char
ReadHSCX(struct IsdnCardState *cs, int hscx, u_char offset)
{
	return (readreg(cs->hw.ax.base, cs->hw.ax.data_adr, offset + (hscx ? 0x40 : 0)));
}

static void
WriteHSCX(struct IsdnCardState *cs, int hscx, u_char offset, u_char value)
{
	writereg(cs->hw.ax.base, cs->hw.ax.data_adr, offset + (hscx ? 0x40 : 0), value);
}

/* Set the specific ipac to active */
static void
set_ipac_active(struct IsdnCardState *cs, u_int active)
{
	/* set irq mask */
	writereg(cs->hw.ax.base, cs->hw.ax.data_adr, IPAC_MASK,
		active ? 0xc0 : 0xff);
}

/*
 * fast interrupt HSCX stuff goes here
 */

#define READHSCX(cs, nr, reg) readreg(cs->hw.ax.base, \
	cs->hw.ax.data_adr, reg + (nr ? 0x40 : 0))
#define WRITEHSCX(cs, nr, reg, data) writereg(cs->hw.ax.base, \
	cs->hw.ax.data_adr, reg + (nr ? 0x40 : 0), data)
#define READHSCXFIFO(cs, nr, ptr, cnt) readfifo(cs->hw.ax.base, \
	cs->hw.ax.data_adr, (nr ? 0x40 : 0), ptr, cnt)
#define WRITEHSCXFIFO(cs, nr, ptr, cnt) writefifo(cs->hw.ax.base, \
	cs->hw.ax.data_adr, (nr ? 0x40 : 0), ptr, cnt)

#include "hscx_irq.c"

static void
bkm_interrupt_ipac(int intno, void *dev_id, struct pt_regs *regs)
{
	struct IsdnCardState *cs = dev_id;
	u_char ista, val, icnt = 5;

	if (!cs) {
		printk(KERN_WARNING "HiSax: Scitel Quadro: Spurious interrupt!\n");
		return;
	}
	ista = readreg(cs->hw.ax.base, cs->hw.ax.data_adr, IPAC_ISTA);
	if (!(ista & 0x3f)) /* not this IPAC */
		return;
      Start_IPAC:
	if (cs->debug & L1_DEB_IPAC)
		debugl1(cs, "IPAC ISTA %02X", ista);
	if (ista & 0x0f) {
		val = readreg(cs->hw.ax.base, cs->hw.ax.data_adr, HSCX_ISTA + 0x40);
		if (ista & 0x01)
			val |= 0x01;
		if (ista & 0x04)
			val |= 0x02;
		if (ista & 0x08)
			val |= 0x04;
		if (val) {
			hscx_int_main(cs, val);
		}
	}
	if (ista & 0x20) {
		val = 0xfe & readreg(cs->hw.ax.base, cs->hw.ax.data_adr, ISAC_ISTA | 0x80);
		if (val) {
			isac_interrupt(cs, val);
		}
	}
	if (ista & 0x10) {
		val = 0x01;
		isac_interrupt(cs, val);
	}
	ista = readreg(cs->hw.ax.base, cs->hw.ax.data_adr, IPAC_ISTA);
	if ((ista & 0x3f) && icnt) {
		icnt--;
		goto Start_IPAC;
	}
	if (!icnt)
		printk(KERN_WARNING "HiSax: %s (%s) IRQ LOOP\n",
		       CardType[cs->typ],
		       sct_quadro_subtypes[cs->subtyp]);
	writereg(cs->hw.ax.base, cs->hw.ax.data_adr, IPAC_MASK, 0xFF);
	writereg(cs->hw.ax.base, cs->hw.ax.data_adr, IPAC_MASK, 0xC0);
}


void
release_io_sct_quadro(struct IsdnCardState *cs)
{
	release_region(cs->hw.ax.base & 0xffffffc0, 128);
	if (cs->subtyp == SCT_1)
		release_region(cs->hw.ax.plx_adr, 64);
}

static void
enable_bkm_int(struct IsdnCardState *cs, unsigned bEnable)
{
	if (cs->typ == ISDN_CTYPE_SCT_QUADRO) {
		if (bEnable)
			wordout(cs->hw.ax.plx_adr + 0x4C, (wordin(cs->hw.ax.plx_adr + 0x4C) | 0x41));
		else
			wordout(cs->hw.ax.plx_adr + 0x4C, (wordin(cs->hw.ax.plx_adr + 0x4C) & ~0x41));
	}
}

static void
reset_bkm(struct IsdnCardState *cs)
{
	long flags;

	if (cs->subtyp == SCT_1) {
		wordout(cs->hw.ax.plx_adr + 0x50, (wordin(cs->hw.ax.plx_adr + 0x50) & ~4));
		save_flags(flags);
		sti();
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout((10 * HZ) / 1000);
		/* Remove the soft reset */
		wordout(cs->hw.ax.plx_adr + 0x50, (wordin(cs->hw.ax.plx_adr + 0x50) | 4));
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout((10 * HZ) / 1000);
		restore_flags(flags);
	}
}

static int
BKM_card_msg(struct IsdnCardState *cs, int mt, void *arg)
{
	switch (mt) {
		case CARD_RESET:
			/* Disable ints */
			set_ipac_active(cs, 0);
			enable_bkm_int(cs, 0);
			reset_bkm(cs);
			return (0);
		case CARD_RELEASE:
			/* Sanity */
			set_ipac_active(cs, 0);
			enable_bkm_int(cs, 0);
			release_io_sct_quadro(cs);
			return (0);
		case CARD_INIT:
			cs->debug |= L1_DEB_IPAC;
			set_ipac_active(cs, 1);
			inithscxisac(cs, 3);
			/* Enable ints */
			enable_bkm_int(cs, 1);
			return (0);
		case CARD_TEST:
			return (0);
	}
	return (0);
}

int __init
sct_alloc_io(u_int adr, u_int len)
{
	if (check_region(adr, len)) {
		printk(KERN_WARNING
			"HiSax: Scitel port %#x-%#x already in use\n",
			adr, adr + len);
		return (1);
	} else {
		request_region(adr, len, "scitel");
	}
	return(0);
}

static struct pci_dev *dev_a8 __initdata = NULL;
static u16  sub_vendor_id __initdata = 0;
static u16  sub_sys_id __initdata = 0;
static u_char pci_bus __initdata = 0;
static u_char pci_device_fn __initdata = 0;
static u_char pci_irq __initdata = 0;

#endif /* CONFIG_PCI */

int __init
setup_sct_quadro(struct IsdnCard *card)
{
#if CONFIG_PCI
	struct IsdnCardState *cs = card->cs;
	char tmp[64];
	u_char pci_rev_id;
	u_int found = 0;
	u_int pci_ioaddr1, pci_ioaddr2, pci_ioaddr3, pci_ioaddr4, pci_ioaddr5;

	strcpy(tmp, sct_quadro_revision);
	printk(KERN_INFO "HiSax: T-Berkom driver Rev. %s\n", HiSax_getrev(tmp));
	if (cs->typ == ISDN_CTYPE_SCT_QUADRO) {
		cs->subtyp = SCT_1;	/* Preset */
	} else
		return (0);

	/* Identify subtype by para[0] */
	if (card->para[0] >= SCT_1 && card->para[0] <= SCT_4)
		cs->subtyp = card->para[0];
	else {
		printk(KERN_WARNING "HiSax: %s: Invalid subcontroller in configuration, default to 1\n",
			CardType[card->typ]);
		return (0);
	}
	if ((cs->subtyp != SCT_1) && ((sub_sys_id != PCI_DEVICE_ID_BERKOM_SCITEL_QUADRO) ||
		(sub_vendor_id != PCI_VENDOR_ID_BERKOM)))
		return (0);
	if (cs->subtyp == SCT_1) {
		if (!pci_present()) {
			printk(KERN_ERR "bkm_a4t: no PCI bus present\n");
			return (0);
		}
		while ((dev_a8 = pci_find_device(PCI_VENDOR_ID_PLX,
			PCI_DEVICE_ID_PLX_9050, dev_a8))) {
			
			sub_vendor_id = dev_a8->subsystem_vendor;
			sub_sys_id = dev_a8->subsystem_device;
			if ((sub_sys_id == PCI_DEVICE_ID_BERKOM_SCITEL_QUADRO) &&
				(sub_vendor_id == PCI_VENDOR_ID_BERKOM)) {
				if (pci_enable_device(dev_a8))
					return(0);
				pci_ioaddr1 = pci_resource_start(dev_a8, 1);
				pci_irq = dev_a8->irq;
				pci_bus = dev_a8->bus->number;
				pci_device_fn = dev_a8->devfn;
				found = 1;
				break;
			}
		}
		if (!found) {
			printk(KERN_WARNING "HiSax: %s (%s): Card not found\n",
				CardType[card->typ],
				sct_quadro_subtypes[cs->subtyp]);
			return (0);
		}
#ifdef ATTEMPT_PCI_REMAPPING
/* HACK: PLX revision 1 bug: PLX address bit 7 must not be set */
		pcibios_read_config_byte(pci_bus, pci_device_fn,
			PCI_REVISION_ID, &pci_rev_id);
		if ((pci_ioaddr1 & 0x80) && (pci_rev_id == 1)) {
			printk(KERN_WARNING "HiSax: %s (%s): PLX rev 1, remapping required!\n",
				CardType[card->typ],
				sct_quadro_subtypes[cs->subtyp]);
			/* Restart PCI negotiation */
			pcibios_write_config_dword(pci_bus, pci_device_fn,
				PCI_BASE_ADDRESS_1, (u_int) - 1);
			/* Move up by 0x80 byte */
			pci_ioaddr1 += 0x80;
			pci_ioaddr1 &= PCI_BASE_ADDRESS_IO_MASK;
			pcibios_write_config_dword(pci_bus, pci_device_fn,
				PCI_BASE_ADDRESS_1, pci_ioaddr1);
			dev_a8->resource[ 1].start = pci_ioaddr1;
		}
#endif /* End HACK */
	}
	if (!pci_irq) {		/* IRQ range check ?? */
		printk(KERN_WARNING "HiSax: %s (%s): No IRQ\n",
		       CardType[card->typ],
		       sct_quadro_subtypes[cs->subtyp]);
		return (0);
	}
	pcibios_read_config_dword(pci_bus, pci_device_fn, PCI_BASE_ADDRESS_1, &pci_ioaddr1);
	pcibios_read_config_dword(pci_bus, pci_device_fn, PCI_BASE_ADDRESS_2, &pci_ioaddr2);
	pcibios_read_config_dword(pci_bus, pci_device_fn, PCI_BASE_ADDRESS_3, &pci_ioaddr3);
	pcibios_read_config_dword(pci_bus, pci_device_fn, PCI_BASE_ADDRESS_4, &pci_ioaddr4);
	pcibios_read_config_dword(pci_bus, pci_device_fn, PCI_BASE_ADDRESS_5, &pci_ioaddr5);
	if (!pci_ioaddr1 || !pci_ioaddr2 || !pci_ioaddr3 || !pci_ioaddr4 || !pci_ioaddr5) {
		printk(KERN_WARNING "HiSax: %s (%s): No IO base address(es)\n",
		       CardType[card->typ],
		       sct_quadro_subtypes[cs->subtyp]);
		return (0);
	}
	pci_ioaddr1 &= PCI_BASE_ADDRESS_IO_MASK;
	pci_ioaddr2 &= PCI_BASE_ADDRESS_IO_MASK;
	pci_ioaddr3 &= PCI_BASE_ADDRESS_IO_MASK;
	pci_ioaddr4 &= PCI_BASE_ADDRESS_IO_MASK;
	pci_ioaddr5 &= PCI_BASE_ADDRESS_IO_MASK;
	/* Take over */
	cs->irq = pci_irq;
	cs->irq_flags |= SA_SHIRQ;
	/* pci_ioaddr1 is unique to all subdevices */
	/* pci_ioaddr2 is for the fourth subdevice only */
	/* pci_ioaddr3 is for the third subdevice only */
	/* pci_ioaddr4 is for the second subdevice only */
	/* pci_ioaddr5 is for the first subdevice only */
	cs->hw.ax.plx_adr = pci_ioaddr1;
	/* Enter all ipac_base addresses */
	switch(cs->subtyp) {
		case 1:
			cs->hw.ax.base = pci_ioaddr5 + 0x00;
			if (sct_alloc_io(pci_ioaddr1, 128))
				return(0);
			if (sct_alloc_io(pci_ioaddr5, 64))
				return(0);
			/* disable all IPAC */
			writereg(pci_ioaddr5, pci_ioaddr5 + 4,
				IPAC_MASK, 0xFF);
			writereg(pci_ioaddr4 + 0x08, pci_ioaddr4 + 0x0c,
				IPAC_MASK, 0xFF);
			writereg(pci_ioaddr3 + 0x10, pci_ioaddr3 + 0x14,
				IPAC_MASK, 0xFF);
			writereg(pci_ioaddr2 + 0x20, pci_ioaddr2 + 0x24,
				IPAC_MASK, 0xFF);
			break;
		case 2:
			cs->hw.ax.base = pci_ioaddr4 + 0x08;
			if (sct_alloc_io(pci_ioaddr4, 64))
				return(0);
			break;
		case 3:
			cs->hw.ax.base = pci_ioaddr3 + 0x10;
			if (sct_alloc_io(pci_ioaddr3, 64))
				return(0);
			break;
		case 4:
			cs->hw.ax.base = pci_ioaddr2 + 0x20;
			if (sct_alloc_io(pci_ioaddr2, 64))
				return(0);
			break;
	}	
	/* For isac and hscx data path */
	cs->hw.ax.data_adr = cs->hw.ax.base + 4;

	printk(KERN_INFO "HiSax: %s (%s) configured at 0x%.4lX, 0x%.4lX, 0x%.4lX and IRQ %d\n",
	       CardType[card->typ],
	       sct_quadro_subtypes[cs->subtyp],
	       cs->hw.ax.plx_adr,
	       cs->hw.ax.base,
	       cs->hw.ax.data_adr,
	       cs->irq);

	test_and_set_bit(HW_IPAC, &cs->HW_Flags);

	cs->readisac = &ReadISAC;
	cs->writeisac = &WriteISAC;
	cs->readisacfifo = &ReadISACfifo;
	cs->writeisacfifo = &WriteISACfifo;

	cs->BC_Read_Reg = &ReadHSCX;
	cs->BC_Write_Reg = &WriteHSCX;
	cs->BC_Send_Data = &hscx_fill_fifo;
	cs->cardmsg = &BKM_card_msg;
	cs->irq_func = &bkm_interrupt_ipac;

	printk(KERN_INFO "HiSax: %s (%s): IPAC Version %d\n",
		CardType[card->typ],
		sct_quadro_subtypes[cs->subtyp],
		readreg(cs->hw.ax.base, cs->hw.ax.data_adr, IPAC_ID));
	return (1);
#else
	printk(KERN_ERR "HiSax: bkm_a8 only supported on PCI Systems\n");
#endif /* CONFIG_PCI */
}
