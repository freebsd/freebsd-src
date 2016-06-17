/* $Id: ix1_micro.c,v 1.1.4.1 2001/11/20 14:19:36 kai Exp $
 *
 * low level stuff for ITK ix1-micro Rev.2 isdn cards
 * derived from the original file teles3.c from Karsten Keil
 *
 * Author       Klaus-Peter Nischke
 * Copyright    by Klaus-Peter Nischke, ITK AG
 *                                   <klaus@nischke.do.eunet.de>
 *              by Karsten Keil      <keil@isdn4linux.de>
 * 
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 * Klaus-Peter Nischke
 * Deusener Str. 287
 * 44369 Dortmund
 * Germany
 */

#define __NO_VERSION__
#include <linux/init.h>
#include <linux/isapnp.h>
#include "hisax.h"
#include "isac.h"
#include "hscx.h"
#include "isdnl1.h"

extern const char *CardType[];
const char *ix1_revision = "$Revision: 1.1.4.1 $";

#define byteout(addr,val) outb(val,addr)
#define bytein(addr) inb(addr)

#define SPECIAL_PORT_OFFSET 3

#define ISAC_COMMAND_OFFSET 2
#define ISAC_DATA_OFFSET 0
#define HSCX_COMMAND_OFFSET 2
#define HSCX_DATA_OFFSET 1

#define TIMEOUT 50

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
	return (readreg(cs->hw.ix1.isac_ale, cs->hw.ix1.isac, offset));
}

static void
WriteISAC(struct IsdnCardState *cs, u_char offset, u_char value)
{
	writereg(cs->hw.ix1.isac_ale, cs->hw.ix1.isac, offset, value);
}

static void
ReadISACfifo(struct IsdnCardState *cs, u_char * data, int size)
{
	readfifo(cs->hw.ix1.isac_ale, cs->hw.ix1.isac, 0, data, size);
}

static void
WriteISACfifo(struct IsdnCardState *cs, u_char * data, int size)
{
	writefifo(cs->hw.ix1.isac_ale, cs->hw.ix1.isac, 0, data, size);
}

static u_char
ReadHSCX(struct IsdnCardState *cs, int hscx, u_char offset)
{
	return (readreg(cs->hw.ix1.hscx_ale,
			cs->hw.ix1.hscx, offset + (hscx ? 0x40 : 0)));
}

static void
WriteHSCX(struct IsdnCardState *cs, int hscx, u_char offset, u_char value)
{
	writereg(cs->hw.ix1.hscx_ale,
		 cs->hw.ix1.hscx, offset + (hscx ? 0x40 : 0), value);
}

#define READHSCX(cs, nr, reg) readreg(cs->hw.ix1.hscx_ale, \
		cs->hw.ix1.hscx, reg + (nr ? 0x40 : 0))
#define WRITEHSCX(cs, nr, reg, data) writereg(cs->hw.ix1.hscx_ale, \
		cs->hw.ix1.hscx, reg + (nr ? 0x40 : 0), data)

#define READHSCXFIFO(cs, nr, ptr, cnt) readfifo(cs->hw.ix1.hscx_ale, \
		cs->hw.ix1.hscx, (nr ? 0x40 : 0), ptr, cnt)

#define WRITEHSCXFIFO(cs, nr, ptr, cnt) writefifo(cs->hw.ix1.hscx_ale, \
		cs->hw.ix1.hscx, (nr ? 0x40 : 0), ptr, cnt)

#include "hscx_irq.c"

static void
ix1micro_interrupt(int intno, void *dev_id, struct pt_regs *regs)
{
	struct IsdnCardState *cs = dev_id;
	u_char val;

	if (!cs) {
		printk(KERN_WARNING "IX1: Spurious interrupt!\n");
		return;
	}
	val = readreg(cs->hw.ix1.hscx_ale, cs->hw.ix1.hscx, HSCX_ISTA + 0x40);
      Start_HSCX:
	if (val)
		hscx_int_main(cs, val);
	val = readreg(cs->hw.ix1.isac_ale, cs->hw.ix1.isac, ISAC_ISTA);
      Start_ISAC:
	if (val)
		isac_interrupt(cs, val);
	val = readreg(cs->hw.ix1.hscx_ale, cs->hw.ix1.hscx, HSCX_ISTA + 0x40);
	if (val) {
		if (cs->debug & L1_DEB_HSCX)
			debugl1(cs, "HSCX IntStat after IntRoutine");
		goto Start_HSCX;
	}
	val = readreg(cs->hw.ix1.isac_ale, cs->hw.ix1.isac, ISAC_ISTA);
	if (val) {
		if (cs->debug & L1_DEB_ISAC)
			debugl1(cs, "ISAC IntStat after IntRoutine");
		goto Start_ISAC;
	}
	writereg(cs->hw.ix1.hscx_ale, cs->hw.ix1.hscx, HSCX_MASK, 0xFF);
	writereg(cs->hw.ix1.hscx_ale, cs->hw.ix1.hscx, HSCX_MASK + 0x40, 0xFF);
	writereg(cs->hw.ix1.isac_ale, cs->hw.ix1.isac, ISAC_MASK, 0xFF);
	writereg(cs->hw.ix1.isac_ale, cs->hw.ix1.isac, ISAC_MASK, 0);
	writereg(cs->hw.ix1.hscx_ale, cs->hw.ix1.hscx, HSCX_MASK, 0);
	writereg(cs->hw.ix1.hscx_ale, cs->hw.ix1.hscx, HSCX_MASK + 0x40, 0);
}

void
release_io_ix1micro(struct IsdnCardState *cs)
{
	if (cs->hw.ix1.cfg_reg)
		release_region(cs->hw.ix1.cfg_reg, 4);
}

static void
ix1_reset(struct IsdnCardState *cs)
{
	long flags;
	int cnt;

	/* reset isac */
	save_flags(flags);
	cnt = 3 * (HZ / 10) + 1;
	sti();
	while (cnt--) {
		byteout(cs->hw.ix1.cfg_reg + SPECIAL_PORT_OFFSET, 1);
		HZDELAY(1);	/* wait >=10 ms */
	}
	byteout(cs->hw.ix1.cfg_reg + SPECIAL_PORT_OFFSET, 0);
	restore_flags(flags);
}

static int
ix1_card_msg(struct IsdnCardState *cs, int mt, void *arg)
{
	switch (mt) {
		case CARD_RESET:
			ix1_reset(cs);
			return(0);
		case CARD_RELEASE:
			release_io_ix1micro(cs);
			return(0);
		case CARD_INIT:
			inithscxisac(cs, 3);
			return(0);
		case CARD_TEST:
			return(0);
	}
	return(0);
}

#ifdef __ISAPNP__
static struct isapnp_device_id itk_ids[] __initdata = {
	{ ISAPNP_VENDOR('I', 'T', 'K'), ISAPNP_FUNCTION(0x25),
	  ISAPNP_VENDOR('I', 'T', 'K'), ISAPNP_FUNCTION(0x25), 
	  (unsigned long) "ITK micro 2" },
	{ ISAPNP_VENDOR('I', 'T', 'K'), ISAPNP_FUNCTION(0x29),
	  ISAPNP_VENDOR('I', 'T', 'K'), ISAPNP_FUNCTION(0x29), 
	  (unsigned long) "ITK micro 2." },
	{ 0, }
};

static struct isapnp_device_id *idev = &itk_ids[0];
static struct pci_bus *pnp_c __devinitdata = NULL;
#endif


int __init
setup_ix1micro(struct IsdnCard *card)
{
	struct IsdnCardState *cs = card->cs;
	char tmp[64];

	strcpy(tmp, ix1_revision);
	printk(KERN_INFO "HiSax: ITK IX1 driver Rev. %s\n", HiSax_getrev(tmp));
	if (cs->typ != ISDN_CTYPE_IX1MICROR2)
		return (0);

#ifdef __ISAPNP__
	if (!card->para[1] && isapnp_present()) {
		struct pci_bus *pb;
		struct pci_dev *pd;

		while(idev->card_vendor) {
			if ((pb = isapnp_find_card(idev->card_vendor,
				idev->card_device, pnp_c))) {
				pnp_c = pb;
				pd = NULL;
				if ((pd = isapnp_find_dev(pnp_c,
					idev->vendor, idev->function, pd))) {
					printk(KERN_INFO "HiSax: %s detected\n",
						(char *)idev->driver_data);
					pd->prepare(pd);
					pd->deactivate(pd);
					pd->activate(pd);
					card->para[1] = pd->resource[0].start;
					card->para[0] = pd->irq_resource[0].start;
					if (!card->para[0] || !card->para[1]) {
						printk(KERN_ERR "ITK PnP:some resources are missing %ld/%lx\n",
						card->para[0], card->para[1]);
						pd->deactivate(pd);
						return(0);
					}
					break;
				} else {
					printk(KERN_ERR "ITK PnP: PnP error card found, no device\n");
				}
			}
			idev++;
			pnp_c=NULL;
		} 
		if (!idev->card_vendor) {
			printk(KERN_INFO "ITK PnP: no ISAPnP card found\n");
			return(0);
		}
	}
#endif
	/* IO-Ports */
	cs->hw.ix1.isac_ale = card->para[1] + ISAC_COMMAND_OFFSET;
	cs->hw.ix1.hscx_ale = card->para[1] + HSCX_COMMAND_OFFSET;
	cs->hw.ix1.isac = card->para[1] + ISAC_DATA_OFFSET;
	cs->hw.ix1.hscx = card->para[1] + HSCX_DATA_OFFSET;
	cs->hw.ix1.cfg_reg = card->para[1];
	cs->irq = card->para[0];
	if (cs->hw.ix1.cfg_reg) {
		if (check_region((cs->hw.ix1.cfg_reg), 4)) {
			printk(KERN_WARNING
			  "HiSax: %s config port %x-%x already in use\n",
			       CardType[card->typ],
			       cs->hw.ix1.cfg_reg,
			       cs->hw.ix1.cfg_reg + 4);
			return (0);
		} else
			request_region(cs->hw.ix1.cfg_reg, 4, "ix1micro cfg");
	}
	printk(KERN_INFO
	       "HiSax: %s config irq:%d io:0x%X\n",
	       CardType[cs->typ], cs->irq,
	       cs->hw.ix1.cfg_reg);
	ix1_reset(cs);
	cs->readisac = &ReadISAC;
	cs->writeisac = &WriteISAC;
	cs->readisacfifo = &ReadISACfifo;
	cs->writeisacfifo = &WriteISACfifo;
	cs->BC_Read_Reg = &ReadHSCX;
	cs->BC_Write_Reg = &WriteHSCX;
	cs->BC_Send_Data = &hscx_fill_fifo;
	cs->cardmsg = &ix1_card_msg;
	cs->irq_func = &ix1micro_interrupt;
	ISACVersion(cs, "ix1-Micro:");
	if (HscxVersion(cs, "ix1-Micro:")) {
		printk(KERN_WARNING
		    "ix1-Micro: wrong HSCX versions check IO address\n");
		release_io_ix1micro(cs);
		return (0);
	}
	return (1);
}
