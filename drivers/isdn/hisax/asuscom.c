/* $Id: asuscom.c,v 1.1.4.1 2001/11/20 14:19:35 kai Exp $
 *
 * low level stuff for ASUSCOM NETWORK INC. ISDNLink cards
 *
 * Author       Karsten Keil
 * Copyright    by Karsten Keil      <keil@isdn4linux.de>
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 * Thanks to  ASUSCOM NETWORK INC. Taiwan and  Dynalink NL for information
 *
 */

#define __NO_VERSION__
#include <linux/init.h>
#include <linux/isapnp.h>
#include "hisax.h"
#include "isac.h"
#include "ipac.h"
#include "hscx.h"
#include "isdnl1.h"

extern const char *CardType[];

const char *Asuscom_revision = "$Revision: 1.1.4.1 $";

#define byteout(addr,val) outb(val,addr)
#define bytein(addr) inb(addr)

#define ASUS_ISAC	0
#define ASUS_HSCX	1
#define ASUS_ADR	2
#define ASUS_CTRL_U7	3
#define ASUS_CTRL_POTS	5

#define ASUS_IPAC_ALE	0
#define ASUS_IPAC_DATA	1

#define ASUS_ISACHSCX	1
#define ASUS_IPAC	2

/* CARD_ADR (Write) */
#define ASUS_RESET      0x80	/* Bit 7 Reset-Leitung */

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
	return (readreg(cs->hw.asus.adr, cs->hw.asus.isac, offset));
}

static void
WriteISAC(struct IsdnCardState *cs, u_char offset, u_char value)
{
	writereg(cs->hw.asus.adr, cs->hw.asus.isac, offset, value);
}

static void
ReadISACfifo(struct IsdnCardState *cs, u_char * data, int size)
{
	readfifo(cs->hw.asus.adr, cs->hw.asus.isac, 0, data, size);
}

static void
WriteISACfifo(struct IsdnCardState *cs, u_char * data, int size)
{
	writefifo(cs->hw.asus.adr, cs->hw.asus.isac, 0, data, size);
}

static u_char
ReadISAC_IPAC(struct IsdnCardState *cs, u_char offset)
{
	return (readreg(cs->hw.asus.adr, cs->hw.asus.isac, offset|0x80));
}

static void
WriteISAC_IPAC(struct IsdnCardState *cs, u_char offset, u_char value)
{
	writereg(cs->hw.asus.adr, cs->hw.asus.isac, offset|0x80, value);
}

static void
ReadISACfifo_IPAC(struct IsdnCardState *cs, u_char * data, int size)
{
	readfifo(cs->hw.asus.adr, cs->hw.asus.isac, 0x80, data, size);
}

static void
WriteISACfifo_IPAC(struct IsdnCardState *cs, u_char * data, int size)
{
	writefifo(cs->hw.asus.adr, cs->hw.asus.isac, 0x80, data, size);
}

static u_char
ReadHSCX(struct IsdnCardState *cs, int hscx, u_char offset)
{
	return (readreg(cs->hw.asus.adr,
			cs->hw.asus.hscx, offset + (hscx ? 0x40 : 0)));
}

static void
WriteHSCX(struct IsdnCardState *cs, int hscx, u_char offset, u_char value)
{
	writereg(cs->hw.asus.adr,
		 cs->hw.asus.hscx, offset + (hscx ? 0x40 : 0), value);
}

/*
 * fast interrupt HSCX stuff goes here
 */

#define READHSCX(cs, nr, reg) readreg(cs->hw.asus.adr, \
		cs->hw.asus.hscx, reg + (nr ? 0x40 : 0))
#define WRITEHSCX(cs, nr, reg, data) writereg(cs->hw.asus.adr, \
		cs->hw.asus.hscx, reg + (nr ? 0x40 : 0), data)

#define READHSCXFIFO(cs, nr, ptr, cnt) readfifo(cs->hw.asus.adr, \
		cs->hw.asus.hscx, (nr ? 0x40 : 0), ptr, cnt)

#define WRITEHSCXFIFO(cs, nr, ptr, cnt) writefifo(cs->hw.asus.adr, \
		cs->hw.asus.hscx, (nr ? 0x40 : 0), ptr, cnt)

#include "hscx_irq.c"

static void
asuscom_interrupt(int intno, void *dev_id, struct pt_regs *regs)
{
	struct IsdnCardState *cs = dev_id;
	u_char val;

	if (!cs) {
		printk(KERN_WARNING "ISDNLink: Spurious interrupt!\n");
		return;
	}
	val = readreg(cs->hw.asus.adr, cs->hw.asus.hscx, HSCX_ISTA + 0x40);
      Start_HSCX:
	if (val)
		hscx_int_main(cs, val);
	val = readreg(cs->hw.asus.adr, cs->hw.asus.isac, ISAC_ISTA);
      Start_ISAC:
	if (val)
		isac_interrupt(cs, val);
	val = readreg(cs->hw.asus.adr, cs->hw.asus.hscx, HSCX_ISTA + 0x40);
	if (val) {
		if (cs->debug & L1_DEB_HSCX)
			debugl1(cs, "HSCX IntStat after IntRoutine");
		goto Start_HSCX;
	}
	val = readreg(cs->hw.asus.adr, cs->hw.asus.isac, ISAC_ISTA);
	if (val) {
		if (cs->debug & L1_DEB_ISAC)
			debugl1(cs, "ISAC IntStat after IntRoutine");
		goto Start_ISAC;
	}
	writereg(cs->hw.asus.adr, cs->hw.asus.hscx, HSCX_MASK, 0xFF);
	writereg(cs->hw.asus.adr, cs->hw.asus.hscx, HSCX_MASK + 0x40, 0xFF);
	writereg(cs->hw.asus.adr, cs->hw.asus.isac, ISAC_MASK, 0xFF);
	writereg(cs->hw.asus.adr, cs->hw.asus.isac, ISAC_MASK, 0x0);
	writereg(cs->hw.asus.adr, cs->hw.asus.hscx, HSCX_MASK, 0x0);
	writereg(cs->hw.asus.adr, cs->hw.asus.hscx, HSCX_MASK + 0x40, 0x0);
}

static void
asuscom_interrupt_ipac(int intno, void *dev_id, struct pt_regs *regs)
{
	struct IsdnCardState *cs = dev_id;
	u_char ista, val, icnt = 5;

	if (!cs) {
		printk(KERN_WARNING "ISDNLink: Spurious interrupt!\n");
		return;
	}
	ista = readreg(cs->hw.asus.adr, cs->hw.asus.isac, IPAC_ISTA);
Start_IPAC:
	if (cs->debug & L1_DEB_IPAC)
		debugl1(cs, "IPAC ISTA %02X", ista);
	if (ista & 0x0f) {
		val = readreg(cs->hw.asus.adr, cs->hw.asus.hscx, HSCX_ISTA + 0x40);
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
		val = 0xfe & readreg(cs->hw.asus.adr, cs->hw.asus.isac, ISAC_ISTA | 0x80);
		if (val) {
			isac_interrupt(cs, val);
		}
	}
	if (ista & 0x10) {
		val = 0x01;
		isac_interrupt(cs, val);
	}
	ista  = readreg(cs->hw.asus.adr, cs->hw.asus.isac, IPAC_ISTA);
	if ((ista & 0x3f) && icnt) {
		icnt--;
		goto Start_IPAC;
	}
	if (!icnt)
		printk(KERN_WARNING "ASUS IRQ LOOP\n");
	writereg(cs->hw.asus.adr, cs->hw.asus.isac, IPAC_MASK, 0xFF);
	writereg(cs->hw.asus.adr, cs->hw.asus.isac, IPAC_MASK, 0xC0);
}

void
release_io_asuscom(struct IsdnCardState *cs)
{
	int bytecnt = 8;

	if (cs->hw.asus.cfg_reg)
		release_region(cs->hw.asus.cfg_reg, bytecnt);
}

static void
reset_asuscom(struct IsdnCardState *cs)
{
	long flags;

	if (cs->subtyp == ASUS_IPAC)
		writereg(cs->hw.asus.adr, cs->hw.asus.isac, IPAC_POTA2, 0x20);
	else
		byteout(cs->hw.asus.adr, ASUS_RESET);	/* Reset On */
	save_flags(flags);
	sti();
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout((10*HZ)/1000);
	if (cs->subtyp == ASUS_IPAC)
		writereg(cs->hw.asus.adr, cs->hw.asus.isac, IPAC_POTA2, 0x0);
	else
		byteout(cs->hw.asus.adr, 0);	/* Reset Off */
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout((10*HZ)/1000);
	if (cs->subtyp == ASUS_IPAC) {
		writereg(cs->hw.asus.adr, cs->hw.asus.isac, IPAC_CONF, 0x0);
		writereg(cs->hw.asus.adr, cs->hw.asus.isac, IPAC_ACFG, 0xff);
		writereg(cs->hw.asus.adr, cs->hw.asus.isac, IPAC_AOE, 0x0);
		writereg(cs->hw.asus.adr, cs->hw.asus.isac, IPAC_MASK, 0xc0);
		writereg(cs->hw.asus.adr, cs->hw.asus.isac, IPAC_PCFG, 0x12);
	}
	restore_flags(flags);
}

static int
Asus_card_msg(struct IsdnCardState *cs, int mt, void *arg)
{
	switch (mt) {
		case CARD_RESET:
			reset_asuscom(cs);
			return(0);
		case CARD_RELEASE:
			release_io_asuscom(cs);
			return(0);
		case CARD_INIT:
			cs->debug |= L1_DEB_IPAC;
			inithscxisac(cs, 3);
			return(0);
		case CARD_TEST:
			return(0);
	}
	return(0);
}

#ifdef __ISAPNP__
static struct isapnp_device_id asus_ids[] __initdata = {
	{ ISAPNP_VENDOR('A', 'S', 'U'), ISAPNP_FUNCTION(0x1688),
	  ISAPNP_VENDOR('A', 'S', 'U'), ISAPNP_FUNCTION(0x1688), 
	  (unsigned long) "Asus1688 PnP" },
	{ ISAPNP_VENDOR('A', 'S', 'U'), ISAPNP_FUNCTION(0x1690),
	  ISAPNP_VENDOR('A', 'S', 'U'), ISAPNP_FUNCTION(0x1690), 
	  (unsigned long) "Asus1690 PnP" },
	{ ISAPNP_VENDOR('S', 'I', 'E'), ISAPNP_FUNCTION(0x0020),
	  ISAPNP_VENDOR('S', 'I', 'E'), ISAPNP_FUNCTION(0x0020), 
	  (unsigned long) "Isurf2 PnP" },
	{ ISAPNP_VENDOR('E', 'L', 'F'), ISAPNP_FUNCTION(0x0000),
	  ISAPNP_VENDOR('E', 'L', 'F'), ISAPNP_FUNCTION(0x0000), 
	  (unsigned long) "Iscas TE320" },
	{ 0, }
};

static struct isapnp_device_id *adev = &asus_ids[0];
static struct pci_bus *pnp_c __devinitdata = NULL;
#endif

int __init
setup_asuscom(struct IsdnCard *card)
{
	int bytecnt;
	struct IsdnCardState *cs = card->cs;
	u_char val;
	char tmp[64];

	strcpy(tmp, Asuscom_revision);
	printk(KERN_INFO "HiSax: Asuscom ISDNLink driver Rev. %s\n", HiSax_getrev(tmp));
	if (cs->typ != ISDN_CTYPE_ASUSCOM)
		return (0);
#ifdef __ISAPNP__
	if (!card->para[1] && isapnp_present()) {
		struct pci_bus *pb;
		struct pci_dev *pd;

		while(adev->card_vendor) {
			if ((pb = isapnp_find_card(adev->card_vendor,
				adev->card_device, pnp_c))) {
				pnp_c = pb;
				pd = NULL;
				if ((pd = isapnp_find_dev(pnp_c,
					adev->vendor, adev->function, pd))) {
					printk(KERN_INFO "HiSax: %s detected\n",
						(char *)adev->driver_data);
					pd->prepare(pd);
					pd->deactivate(pd);
					pd->activate(pd);
					card->para[1] = pd->resource[0].start;
					card->para[0] = pd->irq_resource[0].start;
					if (!card->para[0] || !card->para[1]) {
						printk(KERN_ERR "AsusPnP:some resources are missing %ld/%lx\n",
						card->para[0], card->para[1]);
						pd->deactivate(pd);
						return(0);
					}
					break;
				} else {
					printk(KERN_ERR "AsusPnP: PnP error card found, no device\n");
				}
			}
			adev++;
			pnp_c=NULL;
		} 
		if (!adev->card_vendor) {
			printk(KERN_INFO "AsusPnP: no ISAPnP card found\n");
			return(0);
		}
	}
#endif
	bytecnt = 8;
	cs->hw.asus.cfg_reg = card->para[1];
	cs->irq = card->para[0];
	if (check_region((cs->hw.asus.cfg_reg), bytecnt)) {
		printk(KERN_WARNING
		       "HiSax: %s config port %x-%x already in use\n",
		       CardType[card->typ],
		       cs->hw.asus.cfg_reg,
		       cs->hw.asus.cfg_reg + bytecnt);
		return (0);
	} else {
		request_region(cs->hw.asus.cfg_reg, bytecnt, "asuscom isdn");
	}
	printk(KERN_INFO "ISDNLink: defined at 0x%x IRQ %d\n",
		cs->hw.asus.cfg_reg, cs->irq);
	cs->BC_Read_Reg = &ReadHSCX;
	cs->BC_Write_Reg = &WriteHSCX;
	cs->BC_Send_Data = &hscx_fill_fifo;
	cs->cardmsg = &Asus_card_msg;
	val = readreg(cs->hw.asus.cfg_reg + ASUS_IPAC_ALE, 
		cs->hw.asus.cfg_reg + ASUS_IPAC_DATA, IPAC_ID);
	if ((val == 1) || (val == 2)) {
		cs->subtyp = ASUS_IPAC;
		cs->hw.asus.adr  = cs->hw.asus.cfg_reg + ASUS_IPAC_ALE;
		cs->hw.asus.isac = cs->hw.asus.cfg_reg + ASUS_IPAC_DATA;
		cs->hw.asus.hscx = cs->hw.asus.cfg_reg + ASUS_IPAC_DATA;
		test_and_set_bit(HW_IPAC, &cs->HW_Flags);
		cs->readisac = &ReadISAC_IPAC;
		cs->writeisac = &WriteISAC_IPAC;
		cs->readisacfifo = &ReadISACfifo_IPAC;
		cs->writeisacfifo = &WriteISACfifo_IPAC;
		cs->irq_func = &asuscom_interrupt_ipac;
		printk(KERN_INFO "Asus: IPAC version %x\n", val);
	} else {
		cs->subtyp = ASUS_ISACHSCX;
		cs->hw.asus.adr = cs->hw.asus.cfg_reg + ASUS_ADR;
		cs->hw.asus.isac = cs->hw.asus.cfg_reg + ASUS_ISAC;
		cs->hw.asus.hscx = cs->hw.asus.cfg_reg + ASUS_HSCX;
		cs->hw.asus.u7 = cs->hw.asus.cfg_reg + ASUS_CTRL_U7;
		cs->hw.asus.pots = cs->hw.asus.cfg_reg + ASUS_CTRL_POTS;
		cs->readisac = &ReadISAC;
		cs->writeisac = &WriteISAC;
		cs->readisacfifo = &ReadISACfifo;
		cs->writeisacfifo = &WriteISACfifo;
		cs->irq_func = &asuscom_interrupt;
		ISACVersion(cs, "ISDNLink:");
		if (HscxVersion(cs, "ISDNLink:")) {
			printk(KERN_WARNING
		     	"ISDNLink: wrong HSCX versions check IO address\n");
			release_io_asuscom(cs);
			return (0);
		}
	}
	printk(KERN_INFO "ISDNLink: resetting card\n");
	reset_asuscom(cs);
	return (1);
}
