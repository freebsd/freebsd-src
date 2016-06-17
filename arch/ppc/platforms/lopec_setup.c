/*
 * arch/ppc/platforms/lopec_setup.c
 *
 * Setup routines for the Motorola LoPEC.
 *
 * Author: Dan Cox
 *         danc@mvista.com
 *
 * Copyright 2001-2002 MontaVista Software Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/major.h>
#include <linux/kdev_t.h>
#include <linux/ide.h>
#include <linux/irq.h>
#include <linux/seq_file.h>
#include <linux/console.h>
#include <linux/serial.h>
#include <linux/serialP.h>

#include <asm/system.h>
#include <asm/pgtable.h>
#include <asm/machdep.h>
#include <asm/page.h>
#include <asm/dma.h>
#include <asm/io.h>
#include <asm/time.h>
#include <asm/delay.h>
#include <asm/irq.h>
#include <asm/open_pic.h>
#include <asm/i8259.h>
#include <asm/pci-bridge.h>
#include <asm/todc.h>
#include <asm/bootinfo.h>
#include <asm/mpc10x.h>
#include <asm/keyboard.h>
#include <asm/prep_nvram.h>
#include <asm/serial.h>

extern char saved_command_line[];
extern void lopec_find_bridges(void);

extern int pckbd_setkeycode(unsigned int scancode, unsigned int keycode);
extern int pckbd_getkeycode(unsigned int scancode);
extern int pckbd_translate(unsigned char scancode, unsigned char *keycode,
			   char raw_mode);
extern char pckbd_unexpected_up(unsigned char keycode);
extern void pckbd_leds(unsigned char leds);
extern void pckbd_init_hw(void);
extern unsigned char pckbd_sysrq_xlate[128];
extern void gen550_progress(char *, unsigned short);
extern void gen550_init(int, struct serial_struct *);

/*
 * Define all of the IRQ senses and polarities.  Taken from the
 * LoPEC Programmer's Reference Guide */
static u_char lopec_openpic_initsenses[17] __initdata = {
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* IRQ 0 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* IRQ 1 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* IRQ 2 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* IRQ 3 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* IRQ 4 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* IRQ 5 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* IRQ 6 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* IRQ 7 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* IRQ 8 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* IRQ 9 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* IRQ 10 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* IRQ 11 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE),	/* IRQ 12 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_POSITIVE),	/* IRQ 13 */
	(IRQ_SENSE_EDGE | IRQ_POLARITY_NEGATIVE),	/* IRQ 14 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE)	/* IRQ 15 */
};

static int
lopec_show_cpuinfo(struct seq_file *m)
{
	seq_printf(m, "machine\t\t: Motorola LoPEC\n");
	return 0;
}

static u32
lopec_irq_cannonicalize(u32 irq)
{
	if (irq == 2)
		return 9;
	else
		return irq;
}

static void
lopec_restart(char *cmd)
{
#define LOPEC_SYSSTAT1 0xffe00000
	/* force a hard reset, if possible */
	unsigned char reg = *((unsigned char *) LOPEC_SYSSTAT1);
	reg |= 0x80;
	*((unsigned char *) LOPEC_SYSSTAT1) = reg;

	__cli();
	while(1);
#undef LOPEC_SYSSTAT1
}

static void
lopec_halt(void)
{
	__cli();
	while(1);
}

static void
lopec_power_off(void)
{
	lopec_halt();
}

#if defined(CONFIG_BLK_DEV_IDE) || defined(CONFIG_BLK_DEV_IDE_MODULE)
int lopec_ide_ports_known = 0;
static ide_ioreg_t lopec_ide_regbase[MAX_HWIFS];
static ide_ioreg_t lopec_ide_ctl_regbase[MAX_HWIFS];
static ide_ioreg_t lopec_idedma_regbase;

static void
lopec_ide_probe(void)
{
	struct pci_dev *dev = pci_find_device(PCI_VENDOR_ID_WINBOND,
					      PCI_DEVICE_ID_WINBOND_82C105,
					      NULL);
	lopec_ide_ports_known = 1;

	if (dev) {
		lopec_ide_regbase[0] = dev->resource[0].start;
		lopec_ide_regbase[1] = dev->resource[2].start;
		lopec_ide_ctl_regbase[0] = dev->resource[1].start;
		lopec_ide_ctl_regbase[1] = dev->resource[3].start;
		lopec_idedma_regbase = dev->resource[4].start;
	}
}

static int
lopec_ide_default_irq(ide_ioreg_t base)
{
	if (lopec_ide_ports_known == 0)
		lopec_ide_probe();

	if (base == lopec_ide_regbase[0])
		return 14;
	else if (base == lopec_ide_regbase[1])
		return 15;
	else
		return 0;
}

static ide_ioreg_t
lopec_ide_default_io_base(int index)
{
	if (lopec_ide_ports_known == 0)
		lopec_ide_probe();
	return lopec_ide_regbase[index];
}

static void __init
lopec_ide_init_hwif_ports(hw_regs_t *hw, ide_ioreg_t data,
			  ide_ioreg_t ctl, int *irq)
{
	ide_ioreg_t reg = data;
	uint alt_status_base;
	int i;

	for(i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++)
		hw->io_ports[i] = reg++;

	if (data == lopec_ide_regbase[0]) {
		alt_status_base = lopec_ide_ctl_regbase[0] + 2;
		hw->irq = 14;
	}
	else if (data == lopec_ide_regbase[1]) {
		alt_status_base = lopec_ide_ctl_regbase[1] + 2;
		hw->irq = 15;
	}
	else {
		alt_status_base = 0;
		hw->irq = 0;
	}

	if (ctl)
		hw->io_ports[IDE_CONTROL_OFFSET] = ctl;
	else
		hw->io_ports[IDE_CONTROL_OFFSET] = alt_status_base;

	if (irq != NULL)
		*irq = hw->irq;

}
#endif /* BLK_DEV_IDE */

static void __init
lopec_init_IRQ(void)
{
	int i;

	OpenPIC_InitSenses = lopec_openpic_initsenses;
	OpenPIC_NumInitSenses = sizeof(lopec_openpic_initsenses);

	/*
	 * We need to tell openpic_set_sources where things actually are.
	 * mpc10x_common will setup OpenPIC_Addr at ioremap(EUMB phys base +
	 * EPIC offset (0x40000));  The EPIC IRQ Register Address Map -
	 * Interrupt Source Configuration Registers gives these numbers
	 * as offsets starting at 0x50200, we need to adjust occordinly.
	 */
	/* Map serial interrupts 0-15 */
	openpic_set_sources(0, 16, OpenPIC_Addr + 0x10200);
	/* Skip reserved space and map i2c and DMA Ch[01] */
	openpic_set_sources(16, 3, OpenPIC_Addr + 0x11020);
	/* Skip reserved space and map Message Unit Interrupt (I2O) */
	openpic_set_sources(19, 1, OpenPIC_Addr + 0x110C0);

	openpic_init(NUM_8259_INTERRUPTS);
	/* We have a cascade on OpenPIC IRQ 0, Linux IRQ 16 */
	openpic_hookup_cascade(NUM_8259_INTERRUPTS, "82c59 cascade",
			&i8259_irq);

	/* Map i8259 interrupts */
	for(i = 0; i < NUM_8259_INTERRUPTS; i++)
		irq_desc[i].handler = &i8259_pic;

	i8259_init(0);
}

static void __init
lopec_init2(void)
{
	outb(0x00, 0x4d0);
	outb(0xc0, 0x4d1);

	request_region(0x00, 0x20, "dma1");
	request_region(0x20, 0x20, "pic1");
	request_region(0x40, 0x20, "timer");
	request_region(0x80, 0x10, "dma page reg");
	request_region(0xa0, 0x20, "pic2");
	request_region(0xc0, 0x20, "dma2");
}

static void __init
lopec_map_io(void)
{
	io_block_mapping(0xf0000000, 0xf0000000, 0x10000000, _PAGE_IO);
	io_block_mapping(0xb0000000, 0xb0000000, 0x10000000, _PAGE_IO);
}

static void __init
lopec_set_bat(void)
{
	unsigned long batu, batl;

	__asm__ __volatile__(
		"lis %0,0xf800\n \
                 ori %1,%0,0x002a\n \
                 ori %0,%0,0x0ffe\n \
                 mtspr 0x21e,%0\n \
                 mtspr 0x21f,%1\n \
                 isync\n \
                 sync "
		: "=r" (batu), "=r" (batl));
}

static unsigned long __init
lopec_find_end_of_memory(void)
{
	return boot_mem_size;
}

TODC_ALLOC();

#ifdef CONFIG_SERIAL
static void __init
lopec_early_serial_map(void)
{
	struct serial_struct serial_req;

	memset(&serial_req, 0, sizeof(serial_req));

	/* Setup ioremapped serial port access */
	serial_req.line = 0;
	serial_req.baud_base = BASE_BAUD;
	serial_req.irq = UART0_INT;
	serial_req.flags = ASYNC_BOOT_AUTOCONF;
	serial_req.io_type = SERIAL_IO_MEM;
	serial_req.iomem_base = ioremap(UART0_PORT, 8);

#if defined(CONFIG_SERIAL_TEXT_DEBUG) || defined(CONFIG_KGDB)
	/* Configure debug serial access */
	gen550_init(0, &serial_req);
#endif

	if (early_serial_setup(&serial_req) != 0)
		printk("Early serial init of port 0 failed\n");

	/* Assume early_serial_setup() doesn't modify serial_req */
	serial_req.line = 1;
	serial_req.port = 1;
	serial_req.irq = UART1_INT;
	serial_req.iomem_base = ioremap(UART1_PORT, 8);

#if defined(CONFIG_SERIAL_TEXT_DEBUG) || defined(CONFIG_KGDB)
	/* Configure debug serial access */
	gen550_init(1, &serial_req);
#endif

	if (early_serial_setup(&serial_req) != 0)
		printk("Early serial init of port 1 failed\n");

	serial_req.line = 2;
	serial_req.port = 2;
	serial_req.irq = UART2_INT;
	serial_req.iomem_base = ioremap(UART2_PORT, 8);

#if defined(CONFIG_SERIAL_TEXT_DEBUG) || defined(CONFIG_KGDB)
	/* Configure debug serial access */
	gen550_init(2, &serial_req);
#endif

	if (early_serial_setup(&serial_req) != 0)
		printk("Early serial init of port 2 failed\n");
}
#endif

static void __init
lopec_setup_arch(void)
{

	TODC_INIT(TODC_TYPE_MK48T37, 0, 0,
		  ioremap(0xffe80000, 0x8000), 8);

	loops_per_jiffy = 100000000/HZ;

	lopec_find_bridges();

#ifdef CONFIG_BLK_DEV_INITRD
	if (initrd_start)
		ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
	else
#elif defined(CONFIG_ROOT_NFS)
        	ROOT_DEV = to_kdev_t(0x00ff);
#elif defined(CONFIG_BLK_DEV_IDE)
	        ROOT_DEV = to_kdev_t(0x0301);
#else
        	ROOT_DEV = to_kdev_t(0x0801);
#endif

#ifdef CONFIG_DUMMY_CONSOLE
	conswitchp = &dummy_con;
#endif

#ifdef CONFIG_SERIAL
	lopec_early_serial_map();
#endif

#ifdef CONFIG_PPCBUG_NVRAM
	/* Read in NVRAM data */
	init_prep_nvram();

	/* if no bootargs, look in NVRAM */
	if ( cmd_line[0] == '\0' ) {
		char *bootargs;
		 bootargs = prep_nvram_get_var("bootargs");
		 if (bootargs != NULL) {
			 strcpy(cmd_line, bootargs);
			 /* again.. */
			 strcpy(saved_command_line, cmd_line);
		}
	}
#endif
}

void __init
platform_init(unsigned long r3, unsigned long r4, unsigned long r5,
	      unsigned long r6, unsigned long r7)
{
	parse_bootinfo(find_bootinfo());
	lopec_set_bat();

	isa_io_base = MPC10X_MAPB_ISA_IO_BASE;
	isa_mem_base = MPC10X_MAPB_ISA_MEM_BASE;
	pci_dram_offset = MPC10X_MAPB_DRAM_OFFSET;
	ISA_DMA_THRESHOLD = 0x00ffffff;
	DMA_MODE_READ = 0x44;
	DMA_MODE_WRITE = 0x48;

	ppc_md.setup_arch = lopec_setup_arch;
	ppc_md.show_cpuinfo = lopec_show_cpuinfo;
	ppc_md.irq_cannonicalize = lopec_irq_cannonicalize;
	ppc_md.init_IRQ = lopec_init_IRQ;
	ppc_md.get_irq = openpic_get_irq;
	ppc_md.init = lopec_init2;

	ppc_md.restart = lopec_restart;
	ppc_md.power_off = lopec_power_off;
	ppc_md.halt = lopec_halt;

	ppc_md.find_end_of_memory = lopec_find_end_of_memory;
	ppc_md.setup_io_mappings = lopec_map_io;

#ifdef CONFIG_VT
	ppc_md.kbd_setkeycode = pckbd_setkeycode;
	ppc_md.kbd_getkeycode = pckbd_getkeycode;
	ppc_md.kbd_translate = pckbd_translate;
	ppc_md.kbd_unexpected_up = pckbd_unexpected_up;
#ifdef CONFIG_MAGIC_SYSRQ
	ppc_md.ppc_kbd_sysrq_xlate = pckbd_sysrq_xlate;
	SYSRQ_KEY = 0x54;
#endif /* CONFIG_MAGIC_SYSRQ */
#endif /* CONFIG_VT */

	ppc_md.time_init = todc_time_init;
	ppc_md.set_rtc_time = todc_set_rtc_time;
	ppc_md.get_rtc_time = todc_get_rtc_time;
	ppc_md.calibrate_decr = todc_calibrate_decr;

	ppc_md.nvram_read_val = todc_direct_read_val;
	ppc_md.nvram_write_val = todc_direct_write_val;

#if defined(CONFIG_BLK_DEV_IDE) || defined(CONFIG_BLK_DEV_ID_MODULE)
	ppc_ide_md.default_irq = lopec_ide_default_irq;
	ppc_ide_md.default_io_base = lopec_ide_default_io_base;
	ppc_ide_md.ide_init_hwif = lopec_ide_init_hwif_ports;
#endif
#ifdef CONFIG_SERIAL
#ifdef CONFIG_SERIAL_TEXT_DEBUG
	ppc_md.progress = gen550_progress;
#endif
	ppc_md.early_serial_map = lopec_early_serial_map;
#endif
}
