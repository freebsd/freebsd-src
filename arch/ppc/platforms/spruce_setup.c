/*
 * arch/ppc/platforms/spruce_setup.c
 *
 * Board setup routines for IBM Spruce
 *
 * Authors: Johnnie Peters <jpeters@mvista.com>
 *          Matt Porter <mporter@mvista.com>
 *
 * 2001-2002 (c) MontaVista, Software, Inc.  This file is licensed under
 * the terms of the GNU General Public License version 2.  This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
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
#include <linux/seq_file.h>
#include <linux/ide.h>
#include <linux/serial.h>

#include <asm/keyboard.h>
#include <asm/system.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/dma.h>
#include <asm/io.h>
#include <asm/machdep.h>
#include <asm/time.h>
#include <platforms/spruce.h>
#include <asm/todc.h>
#include <asm/bootinfo.h>

#include "cpc700.h"

extern void spruce_init_IRQ(void);
extern int spruce_get_irq(struct pt_regs *);
extern void spruce_setup_hose(void);

extern int pckbd_setkeycode(unsigned int, unsigned int);
extern int pckbd_getkeycode(unsigned int);
extern int pckbd_translate(unsigned char, unsigned char *, char);
extern char pckbd_unexpected_up(unsigned char);
extern void pckbd_leds(unsigned char);
extern void pckbd_init_hw(void);
extern unsigned char pckbd_sysrq_xlate[128];
extern char cmd_line[];

extern void gen550_progress(char *, unsigned short);
extern void gen550_init(int, struct serial_struct *);

/*
 * CPC700 PIC interrupt programming table
 *
 * First entry is the sensitivity (level/edge), second is the polarity.
 */
unsigned int cpc700_irq_assigns[32][2] = {
	{ 1, 1 },       /* IRQ  0: ECC Correctable Error - rising edge */
	{ 1, 1 },       /* IRQ  1: PCI Write Mem Range   - rising edge */
	{ 0, 1 },       /* IRQ  2: PCI Write Command Reg - active high */
	{ 0, 1 },       /* IRQ  3: UART 0                - active high */
	{ 0, 1 },       /* IRQ  4: UART 1                - active high */
	{ 0, 1 },       /* IRQ  5: ICC 0                 - active high */
	{ 0, 1 },       /* IRQ  6: ICC 1                 - active high */
	{ 0, 1 },       /* IRQ  7: GPT Compare 0         - active high */
	{ 0, 1 },       /* IRQ  8: GPT Compare 1         - active high */
	{ 0, 1 },       /* IRQ  9: GPT Compare 2         - active high */
	{ 0, 1 },       /* IRQ 10: GPT Compare 3         - active high */
	{ 0, 1 },       /* IRQ 11: GPT Compare 4         - active high */
	{ 0, 1 },       /* IRQ 12: GPT Capture 0         - active high */
	{ 0, 1 },       /* IRQ 13: GPT Capture 1         - active high */
	{ 0, 1 },       /* IRQ 14: GPT Capture 2         - active high */
	{ 0, 1 },       /* IRQ 15: GPT Capture 3         - active high */
	{ 0, 1 },       /* IRQ 16: GPT Capture 4         - active high */
	{ 0, 0 },       /* IRQ 17: Reserved */
	{ 0, 0 },       /* IRQ 18: Reserved */
	{ 0, 0 },       /* IRQ 19: Reserved */
	{ 0, 1 },       /* IRQ 20: FPGA EXT_IRQ0         - active high */
	{ 1, 1 },       /* IRQ 21: Mouse                 - rising edge */
	{ 1, 1 },       /* IRQ 22: Keyboard              - rising edge */
	{ 0, 0 },       /* IRQ 23: PCI Slot 3            - active low */
	{ 0, 0 },       /* IRQ 24: PCI Slot 2            - active low */
	{ 0, 0 },       /* IRQ 25: PCI Slot 1            - active low */
	{ 0, 0 },       /* IRQ 26: PCI Slot 0            - active low */
};

static void __init
spruce_calibrate_decr(void)
{
	int freq, divisor = 4;

	/* determine processor bus speed */
	freq = SPRUCE_BUS_SPEED;
	tb_ticks_per_jiffy = freq / HZ / divisor;
	tb_to_us = mulhwu_scale_factor(freq/divisor, 1000000);
}

static int
spruce_show_cpuinfo(struct seq_file *m)
{
	seq_printf(m, "vendor\t\t: IBM\n");
	seq_printf(m, "machine\t\t: Spruce\n");

	return 0;
}

#ifdef CONFIG_SERIAL
static void __init
spruce_early_serial_map(void)
{
	u32 baud_base;
	struct serial_struct serial_req;

	if (SPRUCE_UARTCLK_IS_33M(readb(SPRUCE_FPGA_REG_A)))
		baud_base = SPRUCE_BAUD_33M;
	else
		baud_base = SPRUCE_BAUD_30M;

	/* Setup serial port access */
	memset(&serial_req, 0, sizeof(serial_req));
	serial_req.baud_base = baud_base;
	serial_req.line = 0;
	serial_req.port = 0;
	serial_req.irq = 3;
	serial_req.flags = ASYNC_BOOT_AUTOCONF;
	serial_req.io_type = SERIAL_IO_MEM;
	serial_req.iomem_base = (u_char *)UART0_IO_BASE;
	serial_req.iomem_reg_shift = 0;

#if defined(CONFIG_SERIAL_TEXT_DEBUG) || defined(CONFIG_KGDB)
	gen550_init(0, &serial_req);
#endif

	if (early_serial_setup(&serial_req) != 0)
		printk("Early serial init of port 0 failed\n");

	/* Assume early_serial_setup() doesn't modify serial_req */
	serial_req.line = 1;
	serial_req.port = 1;
	serial_req.irq = 4;
	serial_req.iomem_base = (u_char *)UART1_IO_BASE;

#if defined(CONFIG_SERIAL_TEXT_DEBUG) || defined(CONFIG_KGDB)
	gen550_init(1, &serial_req);
#endif

	if (early_serial_setup(&serial_req) != 0)
		printk("Early serial init of port 1 failed\n");
}
#endif

TODC_ALLOC();

static void __init
spruce_setup_arch(void)
{
	/* Setup TODC access */
	TODC_INIT(TODC_TYPE_DS1643, 0, 0, SPRUCE_RTC_BASE_ADDR, 8);

	/* init to some ~sane value until calibrate_delay() runs */
	loops_per_jiffy = 50000000 / HZ;

	/* Setup PCI host bridge */
	spruce_setup_hose();

#ifdef CONFIG_BLK_DEV_INITRD
	if (initrd_start)
		ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0); /* /dev/ram */
	else
#endif
#ifdef CONFIG_ROOT_NFS
		ROOT_DEV = to_kdev_t(0x00FF);	/* /dev/nfs pseudo device */
#else
		ROOT_DEV = to_kdev_t(0x0801);	/* /dev/sda1 */
#endif

#ifdef CONFIG_DUMMY_CONSOLE
	conswitchp = &dummy_con;
#endif

#ifdef CONFIG_SERIAL
	spruce_early_serial_map();
#endif

	/* Identify the system */
	printk("System Identification: IBM Spruce\n");
	printk("IBM Spruce port (C) 2001 MontaVista Software, Inc. (source@mvista.com)\n");
}

static void
spruce_restart(char *cmd)
{
	__cli();

	/* SRR0 has system reset vector, SRR1 has default MSR value */
	/* rfi restores MSR from SRR1 and sets the PC to the SRR0 value */
	asm volatile("	lis	3,0xfff0	\n\
			ori	3,3,0x0100	\n\
			mtspr	26,3		\n\
			li	3,0		\n\
			mtspr	27,3		\n\
			rfi");
	for(;;);
}

static void
spruce_power_off(void)
{
	for(;;);
}

static void
spruce_halt(void)
{
	spruce_restart(NULL);
}

static unsigned long __init
spruce_find_end_of_memory(void)
{
	return boot_mem_size;
}

static void __init
spruce_map_io(void)
{
	io_block_mapping(SPRUCE_PCI_IO_BASE, SPRUCE_PCI_PHY_IO_BASE,
			 0x08000000, _PAGE_IO);
}

unsigned char spruce_read_keyb_status(void)
{
	unsigned long kbd_status;

	__raw_writel(0x00000088, 0xff500008);
	eieio();

	__raw_writel(0x03000000, 0xff50000c);
	eieio();

	asm volatile("	lis	7,0xff88	\n\
			ori	7,7,0x8		\n\
			lswi	6,7,0x8		\n\
			mr	%0,6"
			: "=r" (kbd_status) :: "6", "7");

	__raw_writel(0x00000000, 0xff50000c);
	eieio();

	return (unsigned char)(kbd_status >> 24);
}

unsigned char spruce_read_keyb_data(void)
{
	unsigned long kbd_data;

	__raw_writel(0x00000088, 0xff500008);
	eieio();

	__raw_writel(0x03000000, 0xff50000c);
	eieio();

	asm volatile("	lis	7,0xff88	\n\
			lswi	6,7,0x8		\n\
			mr	%0,6"
			: "=r" (kbd_data) :: "6", "7");

	__raw_writel(0x00000000, 0xff50000c);
	eieio();

	return (unsigned char)(kbd_data >> 24);
}

#if defined(CONFIG_SERIAL_TEXT_DEBUG) || defined(CONFIG_KGDB)
/*
 * Set BAT 3 to map 0xf8000000 to end of physical memory space 1-to-1.
 */
static __inline__ void
spruce_set_bat(void)
{
	unsigned long	bat3u, bat3l;

	asm volatile("  lis	%0,0xf800	\n\
			ori	%1,%0,0x002a	\n\
			ori	%0,%0,0x0ffe	\n\
			mtspr	0x21e,%0	\n\
			mtspr	0x21f,%1	\n\
			isync			\n\
			sync "
			: "=r" (bat3u), "=r" (bat3l));
}
#endif

void __init
platform_init(unsigned long r3, unsigned long r4, unsigned long r5,
	      unsigned long r6, unsigned long r7)
{
	parse_bootinfo(find_bootinfo());

	isa_io_base = SPRUCE_ISA_IO_BASE;
	pci_dram_offset = SPRUCE_PCI_SYS_MEM_BASE;

	ppc_md.setup_arch = spruce_setup_arch;
	ppc_md.show_cpuinfo = spruce_show_cpuinfo;
	ppc_md.init_IRQ = cpc700_init_IRQ;
	ppc_md.get_irq = cpc700_get_irq;

	ppc_md.find_end_of_memory = spruce_find_end_of_memory;
	ppc_md.setup_io_mappings = spruce_map_io;

	ppc_md.restart = spruce_restart;
	ppc_md.power_off = spruce_power_off;
	ppc_md.halt = spruce_halt;

	ppc_md.time_init = todc_time_init;
	ppc_md.set_rtc_time = todc_set_rtc_time;
	ppc_md.get_rtc_time = todc_get_rtc_time;
	ppc_md.calibrate_decr = spruce_calibrate_decr;

	ppc_md.nvram_read_val = todc_direct_read_val;
	ppc_md.nvram_write_val = todc_direct_write_val;

#if defined(CONFIG_SERIAL) && (defined(CONFIG_SERIAL_TEXT_DEBUG) \
		|| defined(CONFIG_KGDB))
	spruce_set_bat();
	spruce_early_serial_map();

#ifdef CONFIG_SERIAL_TEXT_DEBUG
	ppc_md.progress = gen550_progress;
#endif /* CONFIG_SERIAL_TEXT_DEBUG */
	ppc_md.early_serial_map = spruce_early_serial_map;
#endif

#ifdef CONFIG_VT
	/* Spruce has a PS2 style keyboard */
	ppc_md.kbd_setkeycode = pckbd_setkeycode;
	ppc_md.kbd_getkeycode = pckbd_getkeycode;
	ppc_md.kbd_translate = pckbd_translate;
	ppc_md.kbd_unexpected_up = pckbd_unexpected_up;
	ppc_md.kbd_leds = pckbd_leds;
	ppc_md.kbd_init_hw = pckbd_init_hw;
#ifdef CONFIG_MAGIC_SYSRQ
	ppc_md.ppc_kbd_sysrq_xlate = pckbd_sysrq_xlate;
	SYSRQ_KEY = 0x54;
#endif
#endif
}
