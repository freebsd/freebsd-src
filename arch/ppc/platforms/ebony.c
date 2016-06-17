/*
 * arch/ppc/platforms/ebony.c
 *
 * Ebony board specific routines
 *
 * Matt Porter <mporter@mvista.com>
 *
 * Copyright 2002-2003 MontaVista Software Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/config.h>
#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/reboot.h>
#include <linux/pci.h>
#include <linux/kdev_t.h>
#include <linux/types.h>
#include <linux/major.h>
#include <linux/blk.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/irq.h>
#include <linux/seq_file.h>
#include <linux/tty.h>
#include <linux/serial.h>

#include <asm/system.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/dma.h>
#include <asm/io.h>
#include <asm/machdep.h>
#include <asm/pci-bridge.h>
#include <asm/time.h>
#include <asm/todc.h>
#include <asm/bootinfo.h>
#include <asm/ppc4xx_pic.h>

#include <kernel/ibm440gp_common.h>

extern void abort(void);
extern int pckbd_setkeycode(unsigned int scancode, unsigned int keycode);
extern int pckbd_getkeycode(unsigned int scancode);
extern int pckbd_translate(unsigned char scancode, unsigned char *keycode,
			   char raw_mode);
extern void gen550_progress(char *, unsigned short);
extern void gen550_init(int, struct serial_struct *);

/*
 * Ebony IRQ triggering/polarity settings
 */
static u_char ebony_IRQ_initsenses[] __initdata = {
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 0: UART 0 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 1: UART 1 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 2: IIC 0 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 3: IIC 1 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 4: PCI Inb Mess */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 5: PCI Cmd Wrt */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 6: PCI PM */
	(IRQ_SENSE_EDGE  | IRQ_POLARITY_POSITIVE),	/* 7: PCI MSI 0 */
	(IRQ_SENSE_EDGE  | IRQ_POLARITY_POSITIVE),	/* 8: PCI MSI 1 */
	(IRQ_SENSE_EDGE  | IRQ_POLARITY_POSITIVE),	/* 9: PCI MSI 2 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 10: MAL TX EOB */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 11: MAL RX EOB */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 12: DMA Chan 0 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 13: DMA Chan 1 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 14: DMA Chan 2 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 15: DMA Chan 3 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 16: Reserved */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 17: Reserved */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 18: GPT Timer 0 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 19: GPT Timer 1 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 20: GPT Timer 2 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 21: GPT Timer 3 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 22: GPT Timer 4 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* 23: Ext Int 0 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* 24: Ext Int 1 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* 25: Ext Int 2 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* 26: Ext Int 3 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 27: Ext Int 4 */
	(IRQ_SENSE_EDGE  | IRQ_POLARITY_NEGATIVE),	/* 28: Ext Int 5 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* 29: Ext Int 6 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 30: UIC1 NC Int */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 31: UIC1 Crit Int */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 32: MAL SERR */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 33: MAL TXDE */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 34: MAL RXDE */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 35: ECC Unc Err */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 36: ECC Corr Err */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 37: Ext Bus Ctrl */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 38: Ext Bus Mstr */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 39: OPB->PLB */
	(IRQ_SENSE_EDGE  | IRQ_POLARITY_POSITIVE),	/* 40: PCI MSI 3 */
	(IRQ_SENSE_EDGE  | IRQ_POLARITY_POSITIVE),	/* 41: PCI MSI 4 */
	(IRQ_SENSE_EDGE  | IRQ_POLARITY_POSITIVE),	/* 42: PCI MSI 5 */
	(IRQ_SENSE_EDGE  | IRQ_POLARITY_POSITIVE),	/* 43: PCI MSI 6 */
	(IRQ_SENSE_EDGE  | IRQ_POLARITY_POSITIVE),	/* 44: PCI MSI 7 */
	(IRQ_SENSE_EDGE  | IRQ_POLARITY_POSITIVE),	/* 45: PCI MSI 8 */
	(IRQ_SENSE_EDGE  | IRQ_POLARITY_POSITIVE),	/* 46: PCI MSI 9 */
	(IRQ_SENSE_EDGE  | IRQ_POLARITY_POSITIVE),	/* 47: PCI MSI 10 */
	(IRQ_SENSE_EDGE  | IRQ_POLARITY_POSITIVE),	/* 48: PCI MSI 11 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 49: PLB Perf Mon */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 50: Ext Int 7 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* 51: Ext Int 8 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* 52: Ext Int 9 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* 53: Ext Int 10 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* 54: Ext Int 11 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* 55: Ext Int 12 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 56: Ser ROM Err */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 57: Reserved */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 58: Reserved */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 59: PCI Async Err */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 60: EMAC 0 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 61: EMAC 0 WOL */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 62: EMAC 1 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* 63: EMAC 1 WOL */
};

/* Global Variables */
unsigned char __res[sizeof (bd_t)];

static void __init
ebony_calibrate_decr(void)
{
	unsigned int freq;

	/*
	 * Determine system clock speed
	 *
	 * If we are on Rev. B silicon, then use
	 * default external system clock.  If we are
	 * on Rev. C silicon then errata forces us to
	 * use the internal clock.
	 */
	switch (PVR_REV(mfspr(PVR))) {
		case PVR_REV(PVR_440GP_RB):
			freq = EBONY_440GP_RB_SYSCLK;
			break;
		case PVR_REV(PVR_440GP_RC1):
		default:
			freq = EBONY_440GP_RC_SYSCLK;
			break;
	}
	
	tb_ticks_per_jiffy = freq / HZ;
	tb_to_us = mulhwu_scale_factor(freq, 1000000);

	/* Set the time base to zero */
	mtspr(SPRN_TBWL, 0);
	mtspr(SPRN_TBWU, 0);

	/* Clear any pending timer interrupts */
	mtspr(SPRN_TSR, TSR_ENW | TSR_WIS | TSR_DIS | TSR_FIS);

	/* Enable decrementer interrupt */
	mtspr(SPRN_TCR, TCR_DIE);
}

static int
ebony_show_cpuinfo(struct seq_file *m)
{
	seq_printf(m, "vendor\t\t: IBM\n");
	seq_printf(m, "machine\t\t: Ebony\n");

	return 0;
}

static inline int
ebony_map_irq(struct pci_dev *dev, unsigned char idsel, unsigned char pin)
{
	static char pci_irq_table[][4] =
	/*
	 *	PCI IDSEL/INTPIN->INTLINE 
	 * 	   A   B   C   D
	 */
	{
		{ 23, 23, 23, 23 },	/* IDSEL 1 - PCI Slot 0 */
		{ 24, 24, 24, 24 },	/* IDSEL 2 - PCI Slot 1 */
		{ 25, 25, 25, 25 },	/* IDSEL 3 - PCI Slot 2 */
		{ 26, 26, 26, 26 },	/* IDSEL 4 - PCI Slot 3 */
	};

	const long min_idsel = 1, max_idsel = 4, irqs_per_slot = 4;
	return PCI_IRQ_TABLE_LOOKUP;
}

#define PCIX_READW(offset) \
	(readw((u32)pcix_reg_base+offset))

#define PCIX_WRITEW(value, offset) \
	(writew(value, (u32)pcix_reg_base+offset))

#define PCIX_WRITEL(value, offset) \
	(writel(value, (u32)pcix_reg_base+offset))

/*
 * FIXME: This is only here to "make it work".  This will move
 * to a ibm_pcix.c which will contain a generic IBM PCIX bridge
 * configuration library. -Matt
 */
static void __init
ebony_setup_pcix(void)
{
	void *pcix_reg_base;

	pcix_reg_base = ioremap64(PCIX0_REG_BASE, PCIX0_REG_SIZE);

	/* Enable PCIX0 I/O, Mem, and Busmaster cycles */
	PCIX_WRITEW(PCIX_READW(PCIX0_COMMAND) | PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER, PCIX0_COMMAND);

	/* Disable all windows */
	PCIX_WRITEL(0, PCIX0_POM0SA);
	PCIX_WRITEL(0, PCIX0_POM1SA);
	PCIX_WRITEL(0, PCIX0_POM2SA);
	PCIX_WRITEL(0, PCIX0_PIM0SA);
	PCIX_WRITEL(0, PCIX0_PIM1SA);
	PCIX_WRITEL(0, PCIX0_PIM2SA);
	
	/* Setup 2GB PLB->PCI outbound mem window (3_8000_0000->0_8000_0000) */
	PCIX_WRITEL(0x00000003, PCIX0_POM0LAH);
	PCIX_WRITEL(0x80000000, PCIX0_POM0LAL);
	PCIX_WRITEL(0x00000000, PCIX0_POM0PCIAH);
	PCIX_WRITEL(0x80000000, PCIX0_POM0PCIAL);
	PCIX_WRITEL(0x80000001, PCIX0_POM0SA);

	/* Setup 2GB PCI->PLB inbound memory window at 0, enable MSIs */
	PCIX_WRITEL(0x00000000, PCIX0_PIM0LAH);
	PCIX_WRITEL(0x00000000, PCIX0_PIM0LAL);
	PCIX_WRITEL(0x80000007, PCIX0_PIM0SA);

	eieio();
}

static void __init
ebony_setup_hose(void)
{
	struct pci_controller *hose;

	/* Configure windows on the PCI-X host bridge */
	ebony_setup_pcix();

	hose = pcibios_alloc_controller();

	if (!hose)
		return;

	hose->first_busno = 0;
	hose->last_busno = 0xff;

	hose->pci_mem_offset = EBONY_PCI_MEM_OFFSET;

	pci_init_resource(&hose->io_resource,
			EBONY_PCI_LOWER_IO,
			EBONY_PCI_UPPER_IO,
			IORESOURCE_IO,
			"PCI host bridge");

	pci_init_resource(&hose->mem_resources[0],
			EBONY_PCI_LOWER_MEM,
			EBONY_PCI_UPPER_MEM,
			IORESOURCE_MEM,
			"PCI host bridge");

	hose->io_space.start = EBONY_PCI_LOWER_IO;
	hose->io_space.end = EBONY_PCI_UPPER_IO;
	hose->mem_space.start = EBONY_PCI_LOWER_MEM;
	hose->mem_space.end = EBONY_PCI_UPPER_MEM;
	isa_io_base =
		(unsigned long)ioremap64(EBONY_PCI_IO_BASE, EBONY_PCI_IO_SIZE);
	hose->io_base_virt = (void *)isa_io_base;

	setup_indirect_pci(hose,
			EBONY_PCI_CFGA_PLB32,
			EBONY_PCI_CFGD_PLB32);
	hose->set_cfg_type = 1;

	hose->last_busno = pciauto_bus_scan(hose, hose->first_busno);

	ppc_md.pci_swizzle = common_swizzle;
	ppc_md.pci_map_irq = ebony_map_irq;
}

TODC_ALLOC();

static void __init
ebony_early_serial_map(void)
{
	struct serial_struct serial_req;

	/* Setup ioremapped serial port access */
	memset(&serial_req, 0, sizeof(serial_req));
	serial_req.line = 0;
	serial_req.baud_base = BASE_BAUD;
	serial_req.port = 0;
	serial_req.irq = 0;
	serial_req.flags = ASYNC_BOOT_AUTOCONF;
	serial_req.io_type = SERIAL_IO_MEM;
	serial_req.iomem_base = ioremap64(PPC440GP_UART0_ADDR, 8);
	serial_req.iomem_reg_shift = 0;

#if defined(CONFIG_SERIAL_TEXT_DEBUG) || defined(CONFIG_KGDB)
	/* Configure debug serial access */
	gen550_init(0, &serial_req);
#endif

	if (early_serial_setup(&serial_req) != 0) {
		printk("Early serial init of port 0 failed\n");
	}

	/* Assume early_serial_setup() doesn't modify serial_req */
	serial_req.line = 1;
	serial_req.port = 1;
	serial_req.irq = 1; 
	serial_req.iomem_base = ioremap64(PPC440GP_UART1_ADDR, 8);

#if defined(CONFIG_SERIAL_TEXT_DEBUG) || defined(CONFIG_KGDB)
	/* Configure debug serial access */
	gen550_init(1, &serial_req);
#endif

	if (early_serial_setup(&serial_req) != 0) {
		printk("Early serial init of port 1 failed\n");
	}
}

static void __init
ebony_setup_arch(void)
{
	unsigned char * vpd_base;
	struct ibm44x_clocks clocks;
	bd_t *bip = (bd_t *) __res;

#if !defined(CONFIG_BDI_SWITCH)
	/*
	 * The Abatron BDI JTAG debugger does not tolerate others
	 * mucking with the debug registers.
	 */
        mtspr(SPRN_DBCR0, (DBCR0_TDE | DBCR0_IDM));
#endif

	/* Retrieve MAC addresses */
	vpd_base = ioremap64(EBONY_VPD_BASE, EBONY_VPD_SIZE);
	memcpy(bip->bi_enetaddr[0],EBONY_NA0_ADDR(vpd_base),6);
	memcpy(bip->bi_enetaddr[1],EBONY_NA1_ADDR(vpd_base),6);

	/*
	 * Determine various clocks.
	 * To be completely correct we should get SysClk
	 * from FPGA, because it can be changed by on-board switches
	 * --ebs
	 */
	ibm440gp_get_clocks(&clocks, 33333333, 6 * 1843200);
	bip->bi_opb_busfreq = clocks.opb;
	
	/* Use IIC in standard (100 kHz) mode */
	bip->bi_iic_fast[0] = bip->bi_iic_fast[1] = 0;

	/* Setup TODC access */
	TODC_INIT(TODC_TYPE_DS1743,
			0,
			0,
			ioremap64(EBONY_RTC_ADDR, EBONY_RTC_SIZE),
			8);

	/* init to some ~sane value until calibrate_delay() runs */
        loops_per_jiffy = 50000000/HZ;

	/* Setup PCI host bridge */
	ebony_setup_hose();

#ifdef CONFIG_BLK_DEV_INITRD
	if (initrd_start)
		ROOT_DEV = to_kdev_t(0x0100); /* /dev/ram */
	else
#endif
#ifdef CONFIG_ROOT_NFS
		ROOT_DEV = to_kdev_t(0x00ff); /* /dev/nfs */
#else
		ROOT_DEV = to_kdev_t(0x0301); /* /dev/hda1 */
#endif

#ifdef CONFIG_VT
	conswitchp = &dummy_con;
#endif

	ebony_early_serial_map();

	ibm4xxPIC_InitSenses = ebony_IRQ_initsenses;
	ibm4xxPIC_NumInitSenses = sizeof(ebony_IRQ_initsenses);

	/* Identify the system */
	printk("IBM Ebony port (MontaVista Software, Inc. (source@mvista.com))\n");
}

static void
ebony_restart(char *cmd)
{
	local_irq_disable();
	abort();
}

static void
ebony_power_off(void)
{
	local_irq_disable();
	for(;;);
}

static void
ebony_halt(void)
{
	local_irq_disable();
	for(;;);
}

/*
 * Read the 440GP memory controller to get size of system memory.
 */
static unsigned long __init
ebony_find_end_of_memory(void)
{
	u32 i, bank_config;
	u32 mem_size = 0;

	for (i=0; i<4; i++)
	{
		switch (i)
		{
			case 0:
				mtdcr(DCRN_SDRAM0_CFGADDR, SDRAM0_B0CR);
				break;
			case 1:
				mtdcr(DCRN_SDRAM0_CFGADDR, SDRAM0_B1CR);
				break;
			case 2:
				mtdcr(DCRN_SDRAM0_CFGADDR, SDRAM0_B2CR);
				break;
			case 3:
				mtdcr(DCRN_SDRAM0_CFGADDR, SDRAM0_B3CR);
				break;
		}

		bank_config = mfdcr(DCRN_SDRAM0_CFGDATA);

		if (!(bank_config & SDRAM_CONFIG_BANK_ENABLE))
			continue;
		switch (SDRAM_CONFIG_BANK_SIZE(bank_config))
		{
			case SDRAM_CONFIG_SIZE_8M:
				mem_size += PPC44x_MEM_SIZE_8M;
				break;
			case SDRAM_CONFIG_SIZE_16M:
				mem_size += PPC44x_MEM_SIZE_16M;
				break;
			case SDRAM_CONFIG_SIZE_32M:
				mem_size += PPC44x_MEM_SIZE_32M;
				break;
			case SDRAM_CONFIG_SIZE_64M:
				mem_size += PPC44x_MEM_SIZE_64M;
				break;
			case SDRAM_CONFIG_SIZE_128M:
				mem_size += PPC44x_MEM_SIZE_128M;
				break;
			case SDRAM_CONFIG_SIZE_256M:
				mem_size += PPC44x_MEM_SIZE_256M;
				break;
			case SDRAM_CONFIG_SIZE_512M:
				mem_size += PPC44x_MEM_SIZE_512M;
				break;
		}
	}
	return mem_size;
}

static void __init
ebony_init_irq(void)
{
	int i;

	ppc4xx_pic_init();

	for (i = 0; i < NR_IRQS; i++)
		irq_desc[i].handler = ppc4xx_pic;
}

void __init
platform_init(unsigned long r3, unsigned long r4,
	      unsigned long r5, unsigned long r6, unsigned long r7)
{
	parse_bootinfo(find_bootinfo());

	ppc_md.setup_arch = ebony_setup_arch;
	ppc_md.show_cpuinfo = ebony_show_cpuinfo;
	ppc_md.init_IRQ = ebony_init_irq;
	ppc_md.get_irq = NULL;		/* Set in ppc4xx_pic_init() */

	ppc_md.find_end_of_memory = ebony_find_end_of_memory;

	ppc_md.restart = ebony_restart;
	ppc_md.power_off = ebony_power_off;
	ppc_md.halt = ebony_halt;

	ppc_md.calibrate_decr = ebony_calibrate_decr;
	ppc_md.time_init = todc_time_init;
	ppc_md.set_rtc_time = todc_set_rtc_time;
	ppc_md.get_rtc_time = todc_get_rtc_time;

	ppc_md.nvram_read_val = todc_direct_read_val;
	ppc_md.nvram_write_val = todc_direct_write_val;
#if defined(CONFIG_VT) 
	ppc_md.kbd_setkeycode = pckbd_setkeycode;
	ppc_md.kbd_getkeycode = pckbd_getkeycode;
	ppc_md.kbd_translate = pckbd_translate;
	ppc_md.kbd_unexpected_up = pckbd_unexpected_up;
	ppc_md.kbd_leds = pckbd_leds;
	ppc_md.kbd_init_hw = 0;
#endif

#ifdef CONFIG_SERIAL_TEXT_DEBUG
	ppc_md.progress = gen550_progress;
#endif /* CONFIG_SERIAL_TEXT_DEBUG */
#ifdef CONFIG_KGDB
	ppc_md.early_serial_map = ebony_early_serial_map;
#endif
}

