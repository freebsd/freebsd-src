/* $Id: setup_hs7729pci.c,v 1.1.2.1 2003/06/24 08:45:47 dwmw2 Exp $
 *
 * linux/arch/sh/kernel/setup_hs7729pci.c
 *
 * Copyright (C) 2000  Kazumoto Kojima
 *
 * Hitachi Semiconductor and Devices HS7729PCI Support.
 *
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/irq.h>

#include <linux/hdreg.h>
#include <linux/ide.h>
#include <asm/io.h>
#include <asm/hitachi_hs7729pci.h>
#include <asm/smc37c93x.h>

static int interrupt_count;
static int sevenseg_count;

extern unsigned char aux_device_present;

/*
 * Configure the Super I/O chip
 */
static void __init smsc_config(int index, int data)
{
	outb_p(index, INDEX_PORT);
	outb_p(data, DATA_PORT);
}

static void __init init_smsc(void)
{
	outb_p(CONFIG_ENTER, CONFIG_PORT);
	outb_p(CONFIG_ENTER, CONFIG_PORT);

#if defined(CONFIG_BLK_DEV_FD)
	/* FDC */
	smsc_config(CURRENT_LDN_INDEX, LDN_FDC);
	smsc_config(ACTIVATE_INDEX, 0x01);
	smsc_config(IRQ_SELECT_INDEX, 6); /* IRQ6 */
#endif

#if defined(CONFIG_BLK_DEV_IDE)
	/* IDE1 */
	smsc_config(CURRENT_LDN_INDEX, LDN_IDE1);
	smsc_config(ACTIVATE_INDEX, 0x01);
	smsc_config(IRQ_SELECT_INDEX, 14); /* IRQ14 */
	smsc_config(IO_BASE_HI_INDEX, 0x01);
        smsc_config(IO_BASE_LO_INDEX, 0x70);
	smsc_config(0x62, 0x03);
        smsc_config(0x63, 0x76);

#if 0
#define SMSC_RD(adr) ({ outb(adr, 0x3f0), inb(0x3f1); })

	printk("IDE1 addrs 0x%02x%02x, 0x%02x%02x\n", SMSC_RD(0x60), SMSC_RD(0x61), SMSC_RD(0x62), SMSC_RD(0x63));
	printk("IDE1 Reg 0xF0: %02x. IDE1 Reg 0xF1: %02x\n", SMSC_RD(0xf0), SMSC_RD(0xf1));
	printk("Power reg: %02x\n", SMSC_RD(0x22));
	smsc_config(0x22, 0x3f);
	printk("Power reg changed to: %02x\n", SMSC_RD(0x22));
#endif	       
#endif

	/* AUXIO (GPIO): to use IDE1 */
	smsc_config(CURRENT_LDN_INDEX, LDN_AUXIO);
	smsc_config(GPIO46_INDEX, 0x00); /* nIOROP */
	smsc_config(GPIO47_INDEX, 0x00); /* nIOWOP */

#if defined(CONFIG_SERIAL)
	/* COM1 */
	smsc_config(CURRENT_LDN_INDEX, LDN_COM1);
	smsc_config(ACTIVATE_INDEX, 0x01);
	smsc_config(IO_BASE_HI_INDEX, 0x03);
	smsc_config(IO_BASE_LO_INDEX, 0xf8);
	smsc_config(IRQ_SELECT_INDEX, 4); /* IRQ4 */

	/* COM2 */
	smsc_config(CURRENT_LDN_INDEX, LDN_COM2);
	smsc_config(ACTIVATE_INDEX, 0x01);
	smsc_config(IO_BASE_HI_INDEX, 0x02);
	smsc_config(IO_BASE_LO_INDEX, 0xf8);
	smsc_config(IRQ_SELECT_INDEX, 3); /* IRQ3 */
#endif

	/* RTC */
	smsc_config(CURRENT_LDN_INDEX, LDN_RTC);
	smsc_config(ACTIVATE_INDEX, 0x01);
	smsc_config(IRQ_SELECT_INDEX, 8); /* IRQ8 */

#if defined(CONFIG_VT)
	disable_irq(1);

	/* KBD */
	smsc_config(CURRENT_LDN_INDEX, LDN_KBC);
	smsc_config(ACTIVATE_INDEX, 0x01);
	smsc_config(IRQ_SELECT_INDEX, 1); /* IRQ1 */
	smsc_config(IRQ_SELECT_INDEX+2, 12); /* IRQ12 */

	/* Let it probe for PS/2 port. It should find one. */
	aux_device_present = 0;
#endif

#if defined(CONFIG_PARPORT)
	/* PARALLEL */
	smsc_config(CURRENT_LDN_INDEX, LDN_PARPORT);
	smsc_config(ACTIVATE_INDEX, 0x01);
	smsc_config(IO_BASE_HI_INDEX, 0x03);
	smsc_config(IO_BASE_LO_INDEX, 0x78);
	smsc_config(IRQ_SELECT_INDEX, 5); /* IRQ5 */
#endif

	outb_p(CONFIG_EXIT, CONFIG_PORT);
}

/*
 * Initialize IRQ setting
 */
void __init init_hs7729pci_IRQ(void)
{
	/*
	 * Super I/O (Just mimic PC):
	 *  0: SD0001
	 *  1: keyboard
	 *  3: serial 0
	 *  4: serial 1
	 *  5: printer
	 *  6: floppy
	 *  8: rtc
	 * 12: mouse
	 * 14: ide0
	 */
	ctrl_outw(0x0130, BCR_ILCRA);
	ctrl_outw(0x0070, BCR_ILCRB);
	ctrl_outw(0x9abc, BCR_ILCRC);
	ctrl_outw(0xef84, BCR_ILCRD);
	ctrl_outw(0x0506, BCR_ILCRE);
	ctrl_outw(0x0002, BCR_ILCRF);
	ctrl_outw(0x000d, BCR_ILCRG);

	make_ipr_irq(15, BCR_ILCRA, 3, 0x00); /* IRQ15 */
	make_ipr_irq(14, BCR_ILCRA, 2, 0x01); /* IRQ14 */
	make_ipr_irq(12, BCR_ILCRA, 1, 0x03); /* IRQ12 */

	make_ipr_irq( 8, BCR_ILCRB, 1, 0x07); /* IRQ8 */

	make_ipr_irq( 6, BCR_ILCRC, 3, 0x09); /* IRQ6 */
	make_ipr_irq( 5, BCR_ILCRC, 2, 0x0a); /* IRQ5 */
	make_ipr_irq( 4, BCR_ILCRC, 1, 0x0b); /* IRQ4 */
	make_ipr_irq( 3, BCR_ILCRC, 0, 0x0c); /* IRQ3 */

	make_ipr_irq( 1, BCR_ILCRD, 3, 0x0e); /* IRQ1 */
	make_ipr_irq( 0, BCR_ILCRD, 2, 0x0f); /* PCI_IRQ0 */
	make_ipr_irq( 7, BCR_ILCRD, 1, 0x08); /* USB1_IRQ */
	make_ipr_irq(11, BCR_ILCRD, 0, 0x04); /* USB2_IRQ */

	make_ipr_irq(10, BCR_ILCRE, 2, 0x05); /* PC_IRQ2 */
	make_ipr_irq( 9, BCR_ILCRE, 0, 0x06); /* PC_IRQ1 */

	make_ipr_irq(13, BCR_ILCRF, 0, 0x02); /* SLOT_IRQ5 */

	make_ipr_irq( 2, BCR_ILCRG, 0, 0x0d); /* SLOT_IRQ1 */
}

/*
 * Initialize the board
 */

void __init setup_hs7729pci(void)
{
	init_smsc();
	/* XXX: RTC setting comes here */
	interrupt_count = 0;
	sevenseg_count = 0;
	ctrl_outw(0xffff, PA_LED);
	ctrl_outw(0x3f3f, PA_7SEG);
}

void debug_interrupt(void)
{
	ctrl_outw(interrupt_count, PA_LED);
	if (interrupt_count == 0xffff)
		interrupt_count = 0;
	else
		interrupt_count++;
}

static int sevenseg_data[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07,
			  0x7f, 0x6f, 0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71};

void set_sevenseg(unsigned char val)
{
	unsigned short data;

	data = sevenseg_data[(val & 0xf0) >> 4] << 8;
	data |= sevenseg_data[val & 0x0f];

	ctrl_outw(data, PA_7SEG);
}

void debug_sevenseg(void)
{
	set_sevenseg(sevenseg_count);

	if (sevenseg_count == 0xff)
		sevenseg_count = 0;
	else
		sevenseg_count++;
}
