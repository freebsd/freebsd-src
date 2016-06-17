/* $Id: isurf.c,v 1.1.4.1 2001/11/20 14:19:36 kai Exp $
 *
 * low level stuff for Siemens I-Surf/I-Talk cards
 *
 * Author       Karsten Keil
 * Copyright    by Karsten Keil      <keil@isdn4linux.de>
 * 
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#define __NO_VERSION__
#include <linux/init.h>
#include "hisax.h"
#include "isac.h"
#include "isar.h"
#include "isdnl1.h"
#include <linux/isapnp.h>

extern const char *CardType[];

static const char *ISurf_revision = "$Revision: 1.1.4.1 $";

#define byteout(addr,val) outb(val,addr)
#define bytein(addr) inb(addr)

#define ISURF_ISAR_RESET	1
#define ISURF_ISAC_RESET	2
#define ISURF_ISAR_EA		4
#define ISURF_ARCOFI_RESET	8
#define ISURF_RESET (ISURF_ISAR_RESET | ISURF_ISAC_RESET | ISURF_ARCOFI_RESET)

#define ISURF_ISAR_OFFSET	0
#define ISURF_ISAC_OFFSET	0x100
#define ISURF_IOMEM_SIZE	0x400
/* Interface functions */

static u_char
ReadISAC(struct IsdnCardState *cs, u_char offset)
{
	return (readb(cs->hw.isurf.isac + offset));
}

static void
WriteISAC(struct IsdnCardState *cs, u_char offset, u_char value)
{
	writeb(value, cs->hw.isurf.isac + offset); mb();
}

static void
ReadISACfifo(struct IsdnCardState *cs, u_char * data, int size)
{
	register int i;
	for (i = 0; i < size; i++)
		data[i] = readb(cs->hw.isurf.isac);
}

static void
WriteISACfifo(struct IsdnCardState *cs, u_char * data, int size)
{
	register int i;
	for (i = 0; i < size; i++){
		writeb(data[i], cs->hw.isurf.isac);mb();
	}
}

/* ISAR access routines
 * mode = 0 access with IRQ on
 * mode = 1 access with IRQ off
 * mode = 2 access with IRQ off and using last offset
 */
  
static u_char
ReadISAR(struct IsdnCardState *cs, int mode, u_char offset)
{	
	return(readb(cs->hw.isurf.isar + offset));
}

static void
WriteISAR(struct IsdnCardState *cs, int mode, u_char offset, u_char value)
{
	writeb(value, cs->hw.isurf.isar + offset);mb();
}

static void
isurf_interrupt(int intno, void *dev_id, struct pt_regs *regs)
{
	struct IsdnCardState *cs = dev_id;
	u_char val;
	int cnt = 5;

	if (!cs) {
		printk(KERN_WARNING "ISurf: Spurious interrupt!\n");
		return;
	}

	val = readb(cs->hw.isurf.isar + ISAR_IRQBIT);
      Start_ISAR:
	if (val & ISAR_IRQSTA)
		isar_int_main(cs);
	val = readb(cs->hw.isurf.isac + ISAC_ISTA);
      Start_ISAC:
	if (val)
		isac_interrupt(cs, val);
	val = readb(cs->hw.isurf.isar + ISAR_IRQBIT);
	if ((val & ISAR_IRQSTA) && --cnt) {
		if (cs->debug & L1_DEB_HSCX)
			debugl1(cs, "ISAR IntStat after IntRoutine");
		goto Start_ISAR;
	}
	val = readb(cs->hw.isurf.isac + ISAC_ISTA);
	if (val && --cnt) {
		if (cs->debug & L1_DEB_ISAC)
			debugl1(cs, "ISAC IntStat after IntRoutine");
		goto Start_ISAC;
	}
	if (!cnt)
		printk(KERN_WARNING "ISurf IRQ LOOP\n");

	writeb(0, cs->hw.isurf.isar + ISAR_IRQBIT); mb();
	writeb(0xFF, cs->hw.isurf.isac + ISAC_MASK);mb();
	writeb(0, cs->hw.isurf.isac + ISAC_MASK);mb();
	writeb(ISAR_IRQMSK, cs->hw.isurf.isar + ISAR_IRQBIT); mb();
}

void
release_io_isurf(struct IsdnCardState *cs)
{
	release_region(cs->hw.isurf.reset, 1);
	iounmap((unsigned char *)cs->hw.isurf.isar);
	release_mem_region(cs->hw.isurf.phymem, ISURF_IOMEM_SIZE);
}

static void
reset_isurf(struct IsdnCardState *cs, u_char chips)
{
	long flags;

	printk(KERN_INFO "ISurf: resetting card\n");

	byteout(cs->hw.isurf.reset, chips); /* Reset On */
	save_flags(flags);
	sti();
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout((10*HZ)/1000);
	byteout(cs->hw.isurf.reset, ISURF_ISAR_EA); /* Reset Off */
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout((10*HZ)/1000);
	restore_flags(flags);
}

static int
ISurf_card_msg(struct IsdnCardState *cs, int mt, void *arg)
{
	switch (mt) {
		case CARD_RESET:
			reset_isurf(cs, ISURF_RESET);
			return(0);
		case CARD_RELEASE:
			release_io_isurf(cs);
			return(0);
		case CARD_INIT:
			clear_pending_isac_ints(cs);
			writeb(0, cs->hw.isurf.isar+ISAR_IRQBIT);mb();
			initisac(cs);
			initisar(cs);
			/* Reenable ISAC IRQ */
			cs->writeisac(cs, ISAC_MASK, 0);
			/* RESET Receiver and Transmitter */
			cs->writeisac(cs, ISAC_CMDR, 0x41);
			return(0);
		case CARD_TEST:
			return(0);
	}
	return(0);
}

static int
isurf_auxcmd(struct IsdnCardState *cs, isdn_ctrl *ic) {
	int ret;

	if ((ic->command == ISDN_CMD_IOCTL) && (ic->arg == 9)) {
		ret = isar_auxcmd(cs, ic);
		if (!ret) {
			reset_isurf(cs, ISURF_ISAR_EA | ISURF_ISAC_RESET |
				ISURF_ARCOFI_RESET);
			initisac(cs);
			cs->writeisac(cs, ISAC_MASK, 0);
			cs->writeisac(cs, ISAC_CMDR, 0x41);
		}
		return(ret);
	}
	return(isar_auxcmd(cs, ic));
}

#ifdef __ISAPNP__
static struct pci_bus *pnp_surf __devinitdata = NULL;
#endif

int __init
setup_isurf(struct IsdnCard *card)
{
	int ver;
	struct IsdnCardState *cs = card->cs;
	char tmp[64];

	strcpy(tmp, ISurf_revision);
	printk(KERN_INFO "HiSax: ISurf driver Rev. %s\n", HiSax_getrev(tmp));
	
 	if (cs->typ != ISDN_CTYPE_ISURF)
 		return(0);
	if (card->para[1] && card->para[2]) {
		cs->hw.isurf.reset = card->para[1];
		cs->hw.isurf.phymem = card->para[2];
		cs->irq = card->para[0];
	} else {
#ifdef __ISAPNP__
		struct pci_bus *pb;
		struct pci_dev *pd;

		if (isapnp_present()) {
			cs->subtyp = 0;
			if ((pb = isapnp_find_card(
				ISAPNP_VENDOR('S', 'I', 'E'),
				ISAPNP_FUNCTION(0x0010), pnp_surf))) {
				pnp_surf = pb;
				pd = NULL;
				if (!(pd = isapnp_find_dev(pnp_surf,
					ISAPNP_VENDOR('S', 'I', 'E'),
					ISAPNP_FUNCTION(0x0010), pd))) {
					printk(KERN_ERR "ISurfPnP: PnP error card found, no device\n");
					return (0);
				}
				pd->prepare(pd);
				pd->deactivate(pd);
				pd->activate(pd);
				/* The ISA-PnP logic apparently
				 * expects upper limit address to be
				 * set. Since the isa-pnp module
				 * doesn't do this, so we have to make
				 * up for it.
				 */
				isapnp_cfg_begin(pd->bus->number, pd->devfn);
				isapnp_write_word(ISAPNP_CFG_MEM+3, 
					pd->resource[8].end >> 8);
				isapnp_cfg_end();
				cs->hw.isurf.reset = pd->resource[0].start;
				cs->hw.isurf.phymem = pd->resource[8].start;
				cs->irq = pd->irq_resource[0].start;
				if (!cs->irq || !cs->hw.isurf.reset || !cs->hw.isurf.phymem) {
					printk(KERN_ERR "ISurfPnP:some resources are missing %d/%x/%lx\n",
						cs->irq, cs->hw.isurf.reset, cs->hw.isurf.phymem);
					pd->deactivate(pd);
					return(0);
				}
			} else {
				printk(KERN_INFO "ISurfPnP: no ISAPnP card found\n");
				return(0);
			}
		} else {
			printk(KERN_INFO "ISurfPnP: no ISAPnP bus found\n");
			return(0);
		}
#else
		printk(KERN_WARNING "HiSax: %s port/mem not set\n",
			CardType[card->typ]);
		return (0);
#endif
	}
	if (check_region(cs->hw.isurf.reset, 1)) {
		printk(KERN_WARNING
			"HiSax: %s config port %x already in use\n",
			CardType[card->typ],
			cs->hw.isurf.reset);
			return (0);
	} else {
		request_region(cs->hw.isurf.reset, 1, "isurf isdn");
	}
	if (check_mem_region(cs->hw.isurf.phymem, ISURF_IOMEM_SIZE)) {
		printk(KERN_WARNING
			"HiSax: %s memory region %lx-%lx already in use\n",
			CardType[card->typ],
			cs->hw.isurf.phymem,
			cs->hw.isurf.phymem + ISURF_IOMEM_SIZE);
		release_region(cs->hw.isurf.reset, 1);
		return (0);
	} else {
		request_mem_region(cs->hw.isurf.phymem, ISURF_IOMEM_SIZE,
			"isurf iomem");
	}
	cs->hw.isurf.isar =
		(unsigned long) ioremap(cs->hw.isurf.phymem, ISURF_IOMEM_SIZE);
	cs->hw.isurf.isac = cs->hw.isurf.isar + ISURF_ISAC_OFFSET;
	printk(KERN_INFO
	       "ISurf: defined at 0x%x 0x%lx IRQ %d\n",
	       cs->hw.isurf.reset,
	       cs->hw.isurf.phymem,
	       cs->irq);

	cs->cardmsg = &ISurf_card_msg;
	cs->irq_func = &isurf_interrupt;
	cs->auxcmd = &isurf_auxcmd;
	cs->readisac = &ReadISAC;
	cs->writeisac = &WriteISAC;
	cs->readisacfifo = &ReadISACfifo;
	cs->writeisacfifo = &WriteISACfifo;
	cs->bcs[0].hw.isar.reg = &cs->hw.isurf.isar_r;
	cs->bcs[1].hw.isar.reg = &cs->hw.isurf.isar_r;
	reset_isurf(cs, ISURF_RESET);
	test_and_set_bit(HW_ISAR, &cs->HW_Flags);
	ISACVersion(cs, "ISurf:");
	cs->BC_Read_Reg = &ReadISAR;
	cs->BC_Write_Reg = &WriteISAR;
	cs->BC_Send_Data = &isar_fill_fifo;
	ver = ISARVersion(cs, "ISurf:");
	if (ver < 0) {
		printk(KERN_WARNING
			"ISurf: wrong ISAR version (ret = %d)\n", ver);
		release_io_isurf(cs);
		return (0);
	}
	return (1);
}
