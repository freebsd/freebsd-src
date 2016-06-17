/* $Id: sedlbauer.c,v 1.1.4.1 2001/11/20 14:19:36 kai Exp $
 *
 * low level stuff for Sedlbauer cards
 * includes support for the Sedlbauer speed star (speed star II),
 * support for the Sedlbauer speed fax+,
 * support for the Sedlbauer ISDN-Controller PC/104 and
 * support for the Sedlbauer speed pci
 * derived from the original file asuscom.c from Karsten Keil
 *
 * Author       Marcus Niemann
 * Copyright    by Marcus Niemann    <niemann@www-bib.fh-bielefeld.de>
 * 
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 * Thanks to  Karsten Keil
 *            Sedlbauer AG for informations
 *            Edgar Toernig
 *
 */

/* Supported cards:
 * Card:	Chip:		Configuration:	Comment:
 * ---------------------------------------------------------------------
 * Speed Card	ISAC_HSCX	DIP-SWITCH
 * Speed Win	ISAC_HSCX	ISAPNP
 * Speed Fax+	ISAC_ISAR	ISAPNP		Full analog support
 * Speed Star	ISAC_HSCX	CARDMGR
 * Speed Win2	IPAC		ISAPNP
 * ISDN PC/104	IPAC		DIP-SWITCH
 * Speed Star2	IPAC		CARDMGR
 * Speed PCI	IPAC		PCI PNP
 * Speed Fax+ 	ISAC_ISAR	PCI PNP		Full analog support
 *
 * Important:
 * For the sedlbauer speed fax+ to work properly you have to download
 * the firmware onto the card.
 * For example: hisaxctrl <DriverID> 9 ISAR.BIN
*/

#define __NO_VERSION__
#include <linux/init.h>
#include <linux/config.h>
#include "hisax.h"
#include "isac.h"
#include "ipac.h"
#include "hscx.h"
#include "isar.h"
#include "isdnl1.h"
#include <linux/pci.h>
#include <linux/isapnp.h>

extern const char *CardType[];

const char *Sedlbauer_revision = "$Revision: 1.1.4.1 $";

const char *Sedlbauer_Types[] =
	{"None", "speed card/win", "speed star", "speed fax+",
	"speed win II / ISDN PC/104", "speed star II", "speed pci",
	"speed fax+ pyramid", "speed fax+ pci"};

#define PCI_SUBVENDOR_SPEEDFAX_PYRAMID	0x51
#define PCI_SUBVENDOR_SEDLBAUER_PCI	0x53
#define PCI_SUBVENDOR_SPEEDFAX_PCI	0x54
#define PCI_SUB_ID_SEDLBAUER		0x01

#define SEDL_SPEED_CARD_WIN	1
#define SEDL_SPEED_STAR 	2
#define SEDL_SPEED_FAX		3
#define SEDL_SPEED_WIN2_PC104 	4
#define SEDL_SPEED_STAR2 	5
#define SEDL_SPEED_PCI   	6
#define SEDL_SPEEDFAX_PYRAMID	7
#define SEDL_SPEEDFAX_PCI	8

#define SEDL_CHIP_TEST		0
#define SEDL_CHIP_ISAC_HSCX	1
#define SEDL_CHIP_ISAC_ISAR	2
#define SEDL_CHIP_IPAC		3

#define SEDL_BUS_ISA		1
#define SEDL_BUS_PCI		2
#define	SEDL_BUS_PCMCIA		3

#define byteout(addr,val) outb(val,addr)
#define bytein(addr) inb(addr)

#define SEDL_HSCX_ISA_RESET_ON	0
#define SEDL_HSCX_ISA_RESET_OFF	1
#define SEDL_HSCX_ISA_ISAC	2
#define SEDL_HSCX_ISA_HSCX	3
#define SEDL_HSCX_ISA_ADR	4

#define SEDL_HSCX_PCMCIA_RESET	0
#define SEDL_HSCX_PCMCIA_ISAC	1
#define SEDL_HSCX_PCMCIA_HSCX	2
#define SEDL_HSCX_PCMCIA_ADR	4

#define SEDL_ISAR_ISA_ISAC		4
#define SEDL_ISAR_ISA_ISAR		6
#define SEDL_ISAR_ISA_ADR		8
#define SEDL_ISAR_ISA_ISAR_RESET_ON	10
#define SEDL_ISAR_ISA_ISAR_RESET_OFF	12

#define SEDL_IPAC_ANY_ADR		0
#define SEDL_IPAC_ANY_IPAC		2

#define SEDL_IPAC_PCI_BASE		0
#define SEDL_IPAC_PCI_ADR		0xc0
#define SEDL_IPAC_PCI_IPAC		0xc8
#define SEDL_ISAR_PCI_ADR		0xc8
#define SEDL_ISAR_PCI_ISAC		0xd0
#define SEDL_ISAR_PCI_ISAR		0xe0
#define SEDL_ISAR_PCI_ISAR_RESET_ON	0x01
#define SEDL_ISAR_PCI_ISAR_RESET_OFF	0x18
#define SEDL_ISAR_PCI_LED1		0x08
#define SEDL_ISAR_PCI_LED2		0x10

#define SEDL_RESET      0x3	/* same as DOS driver */

static inline u_char
readreg(unsigned int ale, unsigned int adr, u_char off)
{
	register u_char ret;
	long flags;

	save_flags(flags);
	cli();
	byteout(ale, off);
	ret = bytein(adr);
	restore_flags(flags);
	return (ret);
}

static inline void
readfifo(unsigned int ale, unsigned int adr, u_char off, u_char * data, int size)
{
	/* fifo read without cli because it's allready done  */

	byteout(ale, off);
	insb(adr, data, size);
}


static inline void
writereg(unsigned int ale, unsigned int adr, u_char off, u_char data)
{
	long flags;

	save_flags(flags);
	cli();
	byteout(ale, off);
	byteout(adr, data);
	restore_flags(flags);
}

static inline void
writefifo(unsigned int ale, unsigned int adr, u_char off, u_char * data, int size)
{
	/* fifo write without cli because it's allready done  */
	byteout(ale, off);
	outsb(adr, data, size);
}

/* Interface functions */

static u_char
ReadISAC(struct IsdnCardState *cs, u_char offset)
{
	return (readreg(cs->hw.sedl.adr, cs->hw.sedl.isac, offset));
}

static void
WriteISAC(struct IsdnCardState *cs, u_char offset, u_char value)
{
	writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, offset, value);
}

static void
ReadISACfifo(struct IsdnCardState *cs, u_char * data, int size)
{
	readfifo(cs->hw.sedl.adr, cs->hw.sedl.isac, 0, data, size);
}

static void
WriteISACfifo(struct IsdnCardState *cs, u_char * data, int size)
{
	writefifo(cs->hw.sedl.adr, cs->hw.sedl.isac, 0, data, size);
}

static u_char
ReadISAC_IPAC(struct IsdnCardState *cs, u_char offset)
{
	return (readreg(cs->hw.sedl.adr, cs->hw.sedl.isac, offset|0x80));
}

static void
WriteISAC_IPAC(struct IsdnCardState *cs, u_char offset, u_char value)
{
	writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, offset|0x80, value);
}

static void
ReadISACfifo_IPAC(struct IsdnCardState *cs, u_char * data, int size)
{
	readfifo(cs->hw.sedl.adr, cs->hw.sedl.isac, 0x80, data, size);
}

static void
WriteISACfifo_IPAC(struct IsdnCardState *cs, u_char * data, int size)
{
	writefifo(cs->hw.sedl.adr, cs->hw.sedl.isac, 0x80, data, size);
}

static u_char
ReadHSCX(struct IsdnCardState *cs, int hscx, u_char offset)
{
	return (readreg(cs->hw.sedl.adr,
			cs->hw.sedl.hscx, offset + (hscx ? 0x40 : 0)));
}

static void
WriteHSCX(struct IsdnCardState *cs, int hscx, u_char offset, u_char value)
{
	writereg(cs->hw.sedl.adr,
		 cs->hw.sedl.hscx, offset + (hscx ? 0x40 : 0), value);
}

/* ISAR access routines
 * mode = 0 access with IRQ on
 * mode = 1 access with IRQ off
 * mode = 2 access with IRQ off and using last offset
 */

static u_char
ReadISAR(struct IsdnCardState *cs, int mode, u_char offset)
{	
	if (mode == 0)
		return (readreg(cs->hw.sedl.adr, cs->hw.sedl.hscx, offset));
	else if (mode == 1)
		byteout(cs->hw.sedl.adr, offset);
	return(bytein(cs->hw.sedl.hscx));
}

static void
WriteISAR(struct IsdnCardState *cs, int mode, u_char offset, u_char value)
{
	if (mode == 0)
		writereg(cs->hw.sedl.adr, cs->hw.sedl.hscx, offset, value);
	else {
		if (mode == 1)
			byteout(cs->hw.sedl.adr, offset);
		byteout(cs->hw.sedl.hscx, value);
	}
}

/*
 * fast interrupt HSCX stuff goes here
 */

#define READHSCX(cs, nr, reg) readreg(cs->hw.sedl.adr, \
		cs->hw.sedl.hscx, reg + (nr ? 0x40 : 0))
#define WRITEHSCX(cs, nr, reg, data) writereg(cs->hw.sedl.adr, \
		cs->hw.sedl.hscx, reg + (nr ? 0x40 : 0), data)

#define READHSCXFIFO(cs, nr, ptr, cnt) readfifo(cs->hw.sedl.adr, \
		cs->hw.sedl.hscx, (nr ? 0x40 : 0), ptr, cnt)

#define WRITEHSCXFIFO(cs, nr, ptr, cnt) writefifo(cs->hw.sedl.adr, \
		cs->hw.sedl.hscx, (nr ? 0x40 : 0), ptr, cnt)

#include "hscx_irq.c"

static void
sedlbauer_interrupt(int intno, void *dev_id, struct pt_regs *regs)
{
	struct IsdnCardState *cs = dev_id;
	u_char val;

	if (!cs) {
		printk(KERN_WARNING "Sedlbauer: Spurious interrupt!\n");
		return;
	}

	if ((cs->hw.sedl.bus == SEDL_BUS_PCMCIA) && (*cs->busy_flag == 1)) {
		/* The card tends to generate interrupts while being removed
		   causing us to just crash the kernel. bad. */
		printk(KERN_WARNING "Sedlbauer: card not available!\n");
		return;
	}

	val = readreg(cs->hw.sedl.adr, cs->hw.sedl.hscx, HSCX_ISTA + 0x40);
      Start_HSCX:
	if (val)
		hscx_int_main(cs, val);
	val = readreg(cs->hw.sedl.adr, cs->hw.sedl.isac, ISAC_ISTA);
      Start_ISAC:
	if (val)
		isac_interrupt(cs, val);
	val = readreg(cs->hw.sedl.adr, cs->hw.sedl.hscx, HSCX_ISTA + 0x40);
	if (val) {
		if (cs->debug & L1_DEB_HSCX)
			debugl1(cs, "HSCX IntStat after IntRoutine");
		goto Start_HSCX;
	}
	val = readreg(cs->hw.sedl.adr, cs->hw.sedl.isac, ISAC_ISTA);
	if (val) {
		if (cs->debug & L1_DEB_ISAC)
			debugl1(cs, "ISAC IntStat after IntRoutine");
		goto Start_ISAC;
	}
	writereg(cs->hw.sedl.adr, cs->hw.sedl.hscx, HSCX_MASK, 0xFF);
	writereg(cs->hw.sedl.adr, cs->hw.sedl.hscx, HSCX_MASK + 0x40, 0xFF);
	writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, ISAC_MASK, 0xFF);
	writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, ISAC_MASK, 0x0);
	writereg(cs->hw.sedl.adr, cs->hw.sedl.hscx, HSCX_MASK, 0x0);
	writereg(cs->hw.sedl.adr, cs->hw.sedl.hscx, HSCX_MASK + 0x40, 0x0);
}

static void
sedlbauer_interrupt_ipac(int intno, void *dev_id, struct pt_regs *regs)
{
	struct IsdnCardState *cs = dev_id;
	u_char ista, val, icnt = 5;

	if (!cs) {
		printk(KERN_WARNING "Sedlbauer: Spurious interrupt!\n");
		return;
	}
	ista = readreg(cs->hw.sedl.adr, cs->hw.sedl.isac, IPAC_ISTA);
Start_IPAC:
	if (cs->debug & L1_DEB_IPAC)
		debugl1(cs, "IPAC ISTA %02X", ista);
	if (ista & 0x0f) {
		val = readreg(cs->hw.sedl.adr, cs->hw.sedl.hscx, HSCX_ISTA + 0x40);
		if (ista & 0x01)
			val |= 0x01;
		if (ista & 0x04)
			val |= 0x02;
		if (ista & 0x08)
			val |= 0x04;
		if (val)
			hscx_int_main(cs, val);
	}
	if (ista & 0x20) {
		val = 0xfe & readreg(cs->hw.sedl.adr, cs->hw.sedl.isac, ISAC_ISTA | 0x80);
		if (val) {
			isac_interrupt(cs, val);
		}
	}
	if (ista & 0x10) {
		val = 0x01;
		isac_interrupt(cs, val);
	}
	ista  = readreg(cs->hw.sedl.adr, cs->hw.sedl.isac, IPAC_ISTA);
	if ((ista & 0x3f) && icnt) {
		icnt--;
		goto Start_IPAC;
	}
	if (!icnt)
		if (cs->debug & L1_DEB_ISAC)
			debugl1(cs, "Sedlbauer IRQ LOOP");
	writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, IPAC_MASK, 0xFF);
	writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, IPAC_MASK, 0xC0);
}

static void
sedlbauer_interrupt_isar(int intno, void *dev_id, struct pt_regs *regs)
{
	struct IsdnCardState *cs = dev_id;
	u_char val;
	int cnt = 5;

	if (!cs) {
		printk(KERN_WARNING "Sedlbauer: Spurious interrupt!\n");
		return;
	}

	val = readreg(cs->hw.sedl.adr, cs->hw.sedl.hscx, ISAR_IRQBIT);
      Start_ISAR:
	if (val & ISAR_IRQSTA)
		isar_int_main(cs);
	val = readreg(cs->hw.sedl.adr, cs->hw.sedl.isac, ISAC_ISTA);
      Start_ISAC:
	if (val)
		isac_interrupt(cs, val);
	val = readreg(cs->hw.sedl.adr, cs->hw.sedl.hscx, ISAR_IRQBIT);
	if ((val & ISAR_IRQSTA) && --cnt) {
		if (cs->debug & L1_DEB_HSCX)
			debugl1(cs, "ISAR IntStat after IntRoutine");
		goto Start_ISAR;
	}
	val = readreg(cs->hw.sedl.adr, cs->hw.sedl.isac, ISAC_ISTA);
	if (val && --cnt) {
		if (cs->debug & L1_DEB_ISAC)
			debugl1(cs, "ISAC IntStat after IntRoutine");
		goto Start_ISAC;
	}
	if (!cnt)
		if (cs->debug & L1_DEB_ISAC)
			debugl1(cs, "Sedlbauer IRQ LOOP");

	writereg(cs->hw.sedl.adr, cs->hw.sedl.hscx, ISAR_IRQBIT, 0);
	writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, ISAC_MASK, 0xFF);
	writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, ISAC_MASK, 0x0);
	writereg(cs->hw.sedl.adr, cs->hw.sedl.hscx, ISAR_IRQBIT, ISAR_IRQMSK);
}

void
release_io_sedlbauer(struct IsdnCardState *cs)
{
	int bytecnt = 8;

	if (cs->subtyp == SEDL_SPEED_FAX) {
		bytecnt = 16;
	} else if (cs->hw.sedl.bus == SEDL_BUS_PCI) {
		bytecnt = 256;
	}
	if (cs->hw.sedl.cfg_reg)
		release_region(cs->hw.sedl.cfg_reg, bytecnt);
}

static void
reset_sedlbauer(struct IsdnCardState *cs)
{
	long flags;

	printk(KERN_INFO "Sedlbauer: resetting card\n");

	if (!((cs->hw.sedl.bus == SEDL_BUS_PCMCIA) &&
	   (cs->hw.sedl.chip == SEDL_CHIP_ISAC_HSCX))) {
		if (cs->hw.sedl.chip == SEDL_CHIP_IPAC) {
			writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, IPAC_POTA2, 0x20);
			save_flags(flags);
			sti();
			set_current_state(TASK_UNINTERRUPTIBLE);
			schedule_timeout((10*HZ)/1000);
			writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, IPAC_POTA2, 0x0);
			set_current_state(TASK_UNINTERRUPTIBLE);
			schedule_timeout((10*HZ)/1000);
			writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, IPAC_CONF, 0x0);
			writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, IPAC_ACFG, 0xff);
			writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, IPAC_AOE, 0x0);
			writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, IPAC_MASK, 0xc0);
			writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, IPAC_PCFG, 0x12);
			restore_flags(flags);
		} else if ((cs->hw.sedl.chip == SEDL_CHIP_ISAC_ISAR) &&
			(cs->hw.sedl.bus == SEDL_BUS_PCI)) {
			byteout(cs->hw.sedl.cfg_reg +3, cs->hw.sedl.reset_on);
			save_flags(flags);
			sti();
			current->state = TASK_UNINTERRUPTIBLE;
			schedule_timeout((20*HZ)/1000);
			byteout(cs->hw.sedl.cfg_reg +3, cs->hw.sedl.reset_off);
			current->state = TASK_UNINTERRUPTIBLE;
			schedule_timeout((20*HZ)/1000);
			restore_flags(flags);
		} else {		
			byteout(cs->hw.sedl.reset_on, SEDL_RESET);	/* Reset On */
			save_flags(flags);
			sti();
			set_current_state(TASK_UNINTERRUPTIBLE);
			schedule_timeout((10*HZ)/1000);
			byteout(cs->hw.sedl.reset_off, 0);	/* Reset Off */
			set_current_state(TASK_UNINTERRUPTIBLE);
			schedule_timeout((10*HZ)/1000);
			restore_flags(flags);
		}
	}
}

static int
Sedl_card_msg(struct IsdnCardState *cs, int mt, void *arg)
{
	switch (mt) {
		case CARD_RESET:
			reset_sedlbauer(cs);
			return(0);
		case CARD_RELEASE:
			if (cs->hw.sedl.chip == SEDL_CHIP_ISAC_ISAR) {
				writereg(cs->hw.sedl.adr, cs->hw.sedl.hscx,
					ISAR_IRQBIT, 0);
				writereg(cs->hw.sedl.adr, cs->hw.sedl.isac,
					ISAC_MASK, 0xFF);
				reset_sedlbauer(cs);
				writereg(cs->hw.sedl.adr, cs->hw.sedl.hscx,
					ISAR_IRQBIT, 0);
				writereg(cs->hw.sedl.adr, cs->hw.sedl.isac,
					ISAC_MASK, 0xFF);
			}
			release_io_sedlbauer(cs);
			return(0);
		case CARD_INIT:
			if (cs->hw.sedl.chip == SEDL_CHIP_ISAC_ISAR) {
				clear_pending_isac_ints(cs);
				writereg(cs->hw.sedl.adr, cs->hw.sedl.hscx,
					ISAR_IRQBIT, 0);
				initisac(cs);
				initisar(cs);
				/* Reenable all IRQ */
				cs->writeisac(cs, ISAC_MASK, 0);
				/* RESET Receiver and Transmitter */
				cs->writeisac(cs, ISAC_CMDR, 0x41);
			} else {
				inithscxisac(cs, 3);
			}
			return(0);
		case CARD_TEST:
			return(0);
		case MDL_INFO_CONN:
			if (cs->subtyp != SEDL_SPEEDFAX_PYRAMID)
				return(0);
			if ((long) arg)
				cs->hw.sedl.reset_off &= ~SEDL_ISAR_PCI_LED2;
			else
				cs->hw.sedl.reset_off &= ~SEDL_ISAR_PCI_LED1;
			byteout(cs->hw.sedl.cfg_reg +3, cs->hw.sedl.reset_off);
			break;
		case MDL_INFO_REL:
			if (cs->subtyp != SEDL_SPEEDFAX_PYRAMID)
				return(0);
			if ((long) arg)
				cs->hw.sedl.reset_off |= SEDL_ISAR_PCI_LED2;
			else
				cs->hw.sedl.reset_off |= SEDL_ISAR_PCI_LED1;
			byteout(cs->hw.sedl.cfg_reg +3, cs->hw.sedl.reset_off);
			break;
	}
	return(0);
}

static struct pci_dev *dev_sedl __devinitdata = NULL;

#ifdef __ISAPNP__
static struct isapnp_device_id sedl_ids[] __initdata = {
	{ ISAPNP_VENDOR('S', 'A', 'G'), ISAPNP_FUNCTION(0x01),
	  ISAPNP_VENDOR('S', 'A', 'G'), ISAPNP_FUNCTION(0x01), 
	  (unsigned long) "Speed win" },
	{ ISAPNP_VENDOR('S', 'A', 'G'), ISAPNP_FUNCTION(0x02),
	  ISAPNP_VENDOR('S', 'A', 'G'), ISAPNP_FUNCTION(0x02), 
	  (unsigned long) "Speed Fax+" },
	{ 0, }
};

static struct isapnp_device_id *pdev = &sedl_ids[0];
static struct pci_bus *pnp_c __devinitdata = NULL;
#endif

int __devinit
setup_sedlbauer(struct IsdnCard *card)
{
	int bytecnt, ver, val;
	struct IsdnCardState *cs = card->cs;
	char tmp[64];
	u16 sub_vendor_id, sub_id;
	long flags;

	strcpy(tmp, Sedlbauer_revision);
	printk(KERN_INFO "HiSax: Sedlbauer driver Rev. %s\n", HiSax_getrev(tmp));
	
 	if (cs->typ == ISDN_CTYPE_SEDLBAUER) {
 		cs->subtyp = SEDL_SPEED_CARD_WIN;
		cs->hw.sedl.bus = SEDL_BUS_ISA;
		cs->hw.sedl.chip = SEDL_CHIP_TEST;
 	} else if (cs->typ == ISDN_CTYPE_SEDLBAUER_PCMCIA) {	
 		cs->subtyp = SEDL_SPEED_STAR;
		cs->hw.sedl.bus = SEDL_BUS_PCMCIA;
		cs->hw.sedl.chip = SEDL_CHIP_TEST;
 	} else if (cs->typ == ISDN_CTYPE_SEDLBAUER_FAX) {	
 		cs->subtyp = SEDL_SPEED_FAX;
		cs->hw.sedl.bus = SEDL_BUS_ISA;
		cs->hw.sedl.chip = SEDL_CHIP_ISAC_ISAR;
 	} else
		return (0);

	bytecnt = 8;
	if (card->para[1]) {
		cs->hw.sedl.cfg_reg = card->para[1];
		cs->irq = card->para[0];
		if (cs->hw.sedl.chip == SEDL_CHIP_ISAC_ISAR) {
			bytecnt = 16;
		}
	} else {
#ifdef __ISAPNP__
		if (isapnp_present()) {
			struct pci_bus *pb;
			struct pci_dev *pd;

			while(pdev->card_vendor) {
				if ((pb = isapnp_find_card(pdev->card_vendor,
					pdev->card_device, pnp_c))) {
					pnp_c = pb;
					pd = NULL;
					if ((pd = isapnp_find_dev(pnp_c,
						pdev->vendor, pdev->function, pd))) {
						printk(KERN_INFO "HiSax: %s detected\n",
							(char *)pdev->driver_data);
						pd->prepare(pd);
						pd->deactivate(pd);
						pd->activate(pd);
						card->para[1] =
							pd->resource[0].start;
						card->para[0] =
							pd->irq_resource[0].start;
						if (!card->para[0] || !card->para[1]) {
							printk(KERN_ERR "Sedlbauer PnP:some resources are missing %ld/%lx\n",
								card->para[0], card->para[1]);
							pd->deactivate(pd);
							return(0);
						}
						cs->hw.sedl.cfg_reg = card->para[1];
						cs->irq = card->para[0];
						if (pdev->function == ISAPNP_FUNCTION(0x2)) {
							cs->subtyp = SEDL_SPEED_FAX;
							cs->hw.sedl.chip = SEDL_CHIP_ISAC_ISAR;
							bytecnt = 16;
						} else {
							cs->subtyp = SEDL_SPEED_CARD_WIN;
							cs->hw.sedl.chip = SEDL_CHIP_TEST;
						}
						goto ready;
					} else {
						printk(KERN_ERR "Sedlbauer PnP: PnP error card found, no device\n");
						return(0);
					}
				}
				pdev++;
				pnp_c=NULL;
			} 
			if (!pdev->card_vendor) {
				printk(KERN_INFO "Sedlbauer PnP: no ISAPnP card found\n");
			}
		}
#endif
/* Probe for Sedlbauer speed pci */
#if CONFIG_PCI
		if (!pci_present()) {
			printk(KERN_ERR "Sedlbauer: no PCI bus present\n");
			return(0);
		}
		if ((dev_sedl = pci_find_device(PCI_VENDOR_ID_TIGERJET,
				PCI_DEVICE_ID_TIGERJET_100, dev_sedl))) {
			if (pci_enable_device(dev_sedl))
				return(0);
			cs->irq = dev_sedl->irq;
			if (!cs->irq) {
				printk(KERN_WARNING "Sedlbauer: No IRQ for PCI card found\n");
				return(0);
			}
			cs->hw.sedl.cfg_reg = pci_resource_start(dev_sedl, 0);
		} else {
			printk(KERN_WARNING "Sedlbauer: No PCI card found\n");
			return(0);
		}
		cs->irq_flags |= SA_SHIRQ;
		cs->hw.sedl.bus = SEDL_BUS_PCI;
		sub_vendor_id = dev_sedl->subsystem_vendor;
		sub_id = dev_sedl->subsystem_device;
		printk(KERN_INFO "Sedlbauer: PCI subvendor:%x subid %x\n",
			sub_vendor_id, sub_id);
		printk(KERN_INFO "Sedlbauer: PCI base adr %#x\n",
			cs->hw.sedl.cfg_reg);
		if (sub_id != PCI_SUB_ID_SEDLBAUER) {
			printk(KERN_ERR "Sedlbauer: unknown sub id %#x\n", sub_id);
			return(0);
		}
		if (sub_vendor_id == PCI_SUBVENDOR_SPEEDFAX_PYRAMID) {
			cs->hw.sedl.chip = SEDL_CHIP_ISAC_ISAR;
			cs->subtyp = SEDL_SPEEDFAX_PYRAMID;
		} else if (sub_vendor_id == PCI_SUBVENDOR_SPEEDFAX_PCI) {
			cs->hw.sedl.chip = SEDL_CHIP_ISAC_ISAR;
			cs->subtyp = SEDL_SPEEDFAX_PCI;
		} else if (sub_vendor_id == PCI_SUBVENDOR_SEDLBAUER_PCI) {
			cs->hw.sedl.chip = SEDL_CHIP_IPAC;
			cs->subtyp = SEDL_SPEED_PCI;
		} else {
			printk(KERN_ERR "Sedlbauer: unknown sub vendor id %#x\n",
				sub_vendor_id);
			return(0);
		}
		bytecnt = 256;
		cs->hw.sedl.reset_on = SEDL_ISAR_PCI_ISAR_RESET_ON;
		cs->hw.sedl.reset_off = SEDL_ISAR_PCI_ISAR_RESET_OFF;
		byteout(cs->hw.sedl.cfg_reg, 0xff);
		byteout(cs->hw.sedl.cfg_reg, 0x00);
		byteout(cs->hw.sedl.cfg_reg+ 2, 0xdd);
		byteout(cs->hw.sedl.cfg_reg+ 5, 0x02);
		byteout(cs->hw.sedl.cfg_reg +3, cs->hw.sedl.reset_on);
		save_flags(flags);
		sti();
		current->state = TASK_UNINTERRUPTIBLE;
		schedule_timeout((10*HZ)/1000);
		byteout(cs->hw.sedl.cfg_reg +3, cs->hw.sedl.reset_off);
		restore_flags(flags);
#else
		printk(KERN_WARNING "Sedlbauer: NO_PCI_BIOS\n");
		return (0);
#endif /* CONFIG_PCI */
	}	
ready:	
	/* In case of the sedlbauer pcmcia card, this region is in use,
	 * reserved for us by the card manager. So we do not check it
	 * here, it would fail.
	 */
	if (cs->hw.sedl.bus != SEDL_BUS_PCMCIA &&
		check_region((cs->hw.sedl.cfg_reg), bytecnt)) {
		printk(KERN_WARNING
			"HiSax: %s config port %x-%x already in use\n",
			CardType[card->typ],
			cs->hw.sedl.cfg_reg,
			cs->hw.sedl.cfg_reg + bytecnt);
			return (0);
	} else {
		request_region(cs->hw.sedl.cfg_reg, bytecnt, "sedlbauer isdn");
	}

	printk(KERN_INFO
	       "Sedlbauer: defined at 0x%x-0x%x IRQ %d\n",
	       cs->hw.sedl.cfg_reg,
	       cs->hw.sedl.cfg_reg + bytecnt,
	       cs->irq);

	cs->BC_Read_Reg = &ReadHSCX;
	cs->BC_Write_Reg = &WriteHSCX;
	cs->BC_Send_Data = &hscx_fill_fifo;
	cs->cardmsg = &Sedl_card_msg;

/*
 * testing ISA and PCMCIA Cards for IPAC, default is ISAC
 * do not test for PCI card, because ports are different
 * and PCI card uses only IPAC (for the moment)
 */	
	if (cs->hw.sedl.bus != SEDL_BUS_PCI) {
		val = readreg(cs->hw.sedl.cfg_reg + SEDL_IPAC_ANY_ADR,
			cs->hw.sedl.cfg_reg + SEDL_IPAC_ANY_IPAC, IPAC_ID);
		printk(KERN_DEBUG "Sedlbauer: testing IPAC version %x\n", val);
	        if ((val == 1) || (val == 2)) {
			/* IPAC */
			cs->subtyp = SEDL_SPEED_WIN2_PC104;
			if (cs->hw.sedl.bus == SEDL_BUS_PCMCIA) {
				cs->subtyp = SEDL_SPEED_STAR2;
			}
			cs->hw.sedl.chip = SEDL_CHIP_IPAC;
		} else {
			/* ISAC_HSCX oder ISAC_ISAR */
			if (cs->hw.sedl.chip == SEDL_CHIP_TEST) {
				cs->hw.sedl.chip = SEDL_CHIP_ISAC_HSCX;
			}
		}
	}

/*
 * hw.sedl.chip is now properly set
 */
	printk(KERN_INFO "Sedlbauer: %s detected\n",
		Sedlbauer_Types[cs->subtyp]);


	if (cs->hw.sedl.chip == SEDL_CHIP_IPAC) {
		if (cs->hw.sedl.bus == SEDL_BUS_PCI) {
	                cs->hw.sedl.adr  = cs->hw.sedl.cfg_reg + SEDL_IPAC_PCI_ADR;
			cs->hw.sedl.isac = cs->hw.sedl.cfg_reg + SEDL_IPAC_PCI_IPAC;
			cs->hw.sedl.hscx = cs->hw.sedl.cfg_reg + SEDL_IPAC_PCI_IPAC;
		} else {
	                cs->hw.sedl.adr  = cs->hw.sedl.cfg_reg + SEDL_IPAC_ANY_ADR;
			cs->hw.sedl.isac = cs->hw.sedl.cfg_reg + SEDL_IPAC_ANY_IPAC;
			cs->hw.sedl.hscx = cs->hw.sedl.cfg_reg + SEDL_IPAC_ANY_IPAC;
		}
		test_and_set_bit(HW_IPAC, &cs->HW_Flags);
		cs->readisac = &ReadISAC_IPAC;
		cs->writeisac = &WriteISAC_IPAC;
		cs->readisacfifo = &ReadISACfifo_IPAC;
		cs->writeisacfifo = &WriteISACfifo_IPAC;
		cs->irq_func = &sedlbauer_interrupt_ipac;

		val = readreg(cs->hw.sedl.adr, cs->hw.sedl.isac, IPAC_ID);
		printk(KERN_INFO "Sedlbauer: IPAC version %x\n", val);
		reset_sedlbauer(cs);
	} else {
		/* ISAC_HSCX oder ISAC_ISAR */
		cs->readisac = &ReadISAC;
		cs->writeisac = &WriteISAC;
		cs->readisacfifo = &ReadISACfifo;
		cs->writeisacfifo = &WriteISACfifo;
		if (cs->hw.sedl.chip == SEDL_CHIP_ISAC_ISAR) {
			if (cs->hw.sedl.bus == SEDL_BUS_PCI) {
				cs->hw.sedl.adr = cs->hw.sedl.cfg_reg +
							SEDL_ISAR_PCI_ADR;
				cs->hw.sedl.isac = cs->hw.sedl.cfg_reg +
							SEDL_ISAR_PCI_ISAC;
				cs->hw.sedl.hscx = cs->hw.sedl.cfg_reg +
							SEDL_ISAR_PCI_ISAR;
			} else {
				cs->hw.sedl.adr = cs->hw.sedl.cfg_reg +
							SEDL_ISAR_ISA_ADR;
				cs->hw.sedl.isac = cs->hw.sedl.cfg_reg +
							SEDL_ISAR_ISA_ISAC;
				cs->hw.sedl.hscx = cs->hw.sedl.cfg_reg +
							SEDL_ISAR_ISA_ISAR;
				cs->hw.sedl.reset_on = cs->hw.sedl.cfg_reg +
							SEDL_ISAR_ISA_ISAR_RESET_ON;
				cs->hw.sedl.reset_off = cs->hw.sedl.cfg_reg +
							SEDL_ISAR_ISA_ISAR_RESET_OFF;
			}
			cs->bcs[0].hw.isar.reg = &cs->hw.sedl.isar;
			cs->bcs[1].hw.isar.reg = &cs->hw.sedl.isar;
			test_and_set_bit(HW_ISAR, &cs->HW_Flags);
			cs->irq_func = &sedlbauer_interrupt_isar;
			cs->auxcmd = &isar_auxcmd;
			ISACVersion(cs, "Sedlbauer:");
			cs->BC_Read_Reg = &ReadISAR;
			cs->BC_Write_Reg = &WriteISAR;
			cs->BC_Send_Data = &isar_fill_fifo;
			ver = ISARVersion(cs, "Sedlbauer:");
			if (ver < 0) {
				printk(KERN_WARNING
					"Sedlbauer: wrong ISAR version (ret = %d)\n", ver);
				release_io_sedlbauer(cs);
				return (0);
			}
		} else {
			if (cs->hw.sedl.bus == SEDL_BUS_PCMCIA) {
				cs->hw.sedl.adr = cs->hw.sedl.cfg_reg + SEDL_HSCX_PCMCIA_ADR;
				cs->hw.sedl.isac = cs->hw.sedl.cfg_reg + SEDL_HSCX_PCMCIA_ISAC;
				cs->hw.sedl.hscx = cs->hw.sedl.cfg_reg + SEDL_HSCX_PCMCIA_HSCX;
				cs->hw.sedl.reset_on = cs->hw.sedl.cfg_reg + SEDL_HSCX_PCMCIA_RESET;
				cs->hw.sedl.reset_off = cs->hw.sedl.cfg_reg + SEDL_HSCX_PCMCIA_RESET;
			} else {
				cs->hw.sedl.adr = cs->hw.sedl.cfg_reg + SEDL_HSCX_ISA_ADR;
				cs->hw.sedl.isac = cs->hw.sedl.cfg_reg + SEDL_HSCX_ISA_ISAC;
				cs->hw.sedl.hscx = cs->hw.sedl.cfg_reg + SEDL_HSCX_ISA_HSCX;
				cs->hw.sedl.reset_on = cs->hw.sedl.cfg_reg + SEDL_HSCX_ISA_RESET_ON;
				cs->hw.sedl.reset_off = cs->hw.sedl.cfg_reg + SEDL_HSCX_ISA_RESET_OFF;
			}
			cs->irq_func = &sedlbauer_interrupt;
			ISACVersion(cs, "Sedlbauer:");
		
			if (HscxVersion(cs, "Sedlbauer:")) {
				printk(KERN_WARNING
					"Sedlbauer: wrong HSCX versions check IO address\n");
				release_io_sedlbauer(cs);
				return (0);
			}
			reset_sedlbauer(cs);
		}
	}
	return (1);
}
