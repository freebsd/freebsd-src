/*
 *
 *    Copyright (c) 1999-2000 Grant Erickson <grant@lcse.umn.edu>
 *
 *    Copyright 2000-2002 MontaVista Software Inc.
 *      Completed implementation.
 *      Author: MontaVista Software, Inc.  <source@mvista.com>
 *              Frank Rowand <frank_rowand@mvista.com>
 *              Debbie Chu   <debbie_chu@mvista.com>
 *
 *    Module name: ppc4xx_setup.c
 *
 *    Description:
 *      Architecture- / platform-specific boot-time initialization code for
 *      IBM PowerPC 4xx based boards. Adapted from original
 *      code by Gary Thomas, Cort Dougan <cort@fsmlabs.com>, and Dan Malek
 *      <dan@net4x.com>.
 *
 * 	History: 11/09/2001 - armin
 *	rename board_setup_nvram_access to board_init. board_init is
 *	used for all other board specific instructions needed during
 *	platform_init.
 *	moved RTC to board.c files
 *	moved VT/FB to board.c files
 *	moved r/w4 ide to redwood.c
 *
 *	History: 04/18/02 - Armin
 *	added ash to setting CETE bit in calibrate()
 *
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/threads.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/reboot.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/blk.h>
#include <linux/pci.h>
#include <linux/rtc.h>
#include <linux/console.h>
#include <linux/ide.h>
#include <linux/serial_reg.h>
#include <linux/seq_file.h>
#include <linux/module.h>

#include <asm/system.h>
#include <asm/processor.h>
#include <asm/machdep.h>
#include <asm/page.h>
#include <asm/kgdb.h>
#include <asm/ibm4xx.h>
#include <asm/time.h>
#include <asm/todc.h>
#include <asm/ppc4xx_pic.h>
#include <asm/pci-bridge.h>
#include <asm/bootinfo.h>

/* Function Prototypes */
static void ppc4xx_gdb_init(void);

extern void abort(void);
extern void ppc4xx_find_bridges(void);

extern int pckbd_setkeycode(unsigned int scancode, unsigned int keycode);
extern int pckbd_getkeycode(unsigned int scancode);
extern int pckbd_pretranslate(unsigned char scancode, char raw_mode);
extern int pckbd_translate(unsigned char scancode, unsigned char *keycode,
			   char raw_mode);
extern char pckbd_unexpected_up(unsigned char keycode);
extern void pckbd_leds(unsigned char leds);
extern void pckbd_init_hw(void);

extern int nonpci_ide_default_irq(ide_ioreg_t base);
extern void nonpci_ide_init_hwif_ports(hw_regs_t * hw, ide_ioreg_t data_port,
				       ide_ioreg_t ctrl_port, int *irq);

extern void ppc4xx_wdt_heartbeat(void);
extern int wdt_enable;
extern unsigned long wdt_period;
//extern void early_uart_init(void);

/* Board specific functions */
extern void board_setup_arch(void);
extern void board_io_mapping(void);
extern void board_setup_irq(void);
extern void board_init(void);

/* Global Variables */
unsigned char __res[sizeof (bd_t)];

#if defined(EMAC_NUMS) && EMAC_NUMS > 0
u32 emac_phy_map[EMAC_NUMS];
EXPORT_SYMBOL(emac_phy_map);
#endif

static void __init
ppc4xx_setup_arch(void)
{

	/* Setup PCI host bridges */

#ifdef CONFIG_PCI
	ppc4xx_find_bridges();
#endif

#if defined(CONFIG_FB)
	conswitchp = &dummy_con;
#endif
#if defined (CONFIG_SERIAL) && !defined (CONFIG_RAINIER)
//	early_uart_init();
#endif
	board_setup_arch();

	ppc4xx_gdb_init();
}

/*
 *   This routine pretty-prints the platform's internal CPU clock
 *   frequencies into the buffer for usage in /proc/cpuinfo.
 */

static int
ppc4xx_show_percpuinfo(struct seq_file *m, int i)
{
	bd_t *bip = (bd_t *) __res;

	seq_printf(m, "clock\t\t: %ldMHz\n", (long) bip->bi_intfreq / 1000000);

	return 0;
}

/*
 *   This routine pretty-prints the platform's internal bus clock
 *   frequencies into the buffer for usage in /proc/cpuinfo.
 */
static int
ppc4xx_show_cpuinfo(struct seq_file *m)
{
	bd_t *bip = (bd_t *) __res;

	seq_printf(m, "machine\t\t: %s\n", PPC4xx_MACHINE_NAME);
	seq_printf(m, "plb bus clock\t: %ldMHz\n",
		   (long) bip->bi_busfreq / 1000000);
#ifdef CONFIG_PCI
	seq_printf(m, "pci bus clock\t: %dMHz\n",
		   bip->bi_pci_busfreq / 1000000);
#endif

	return 0;
}

/*
 * Return the virtual address representing the top of physical RAM.
 */
static unsigned long __init
ppc4xx_find_end_of_memory(void)
{
	bd_t *bip = (bd_t *) __res;

	return ((unsigned long) bip->bi_memsize);
}

static void __init
m4xx_map_io(void)
{
	io_block_mapping(PPC4xx_ONB_IO_VADDR,
			 PPC4xx_ONB_IO_PADDR, PPC4xx_ONB_IO_SIZE, _PAGE_IO);
#ifdef CONFIG_PCI
	io_block_mapping(PPC4xx_PCI_IO_VADDR,
			 PPC4xx_PCI_IO_PADDR, PPC4xx_PCI_IO_SIZE, _PAGE_IO);
	io_block_mapping(PPC4xx_PCI_CFG_VADDR,
			 PPC4xx_PCI_CFG_PADDR, PPC4xx_PCI_CFG_SIZE, _PAGE_IO);
	io_block_mapping(PPC4xx_PCI_LCFG_VADDR,
			 PPC4xx_PCI_LCFG_PADDR, PPC4xx_PCI_LCFG_SIZE, _PAGE_IO);
#endif
	board_io_mapping();
}

static void __init
ppc4xx_init_IRQ(void)
{
	int i;

	ppc4xx_pic_init();

	for (i = 0; i < NR_IRQS; i++)
		irq_desc[i].handler = ppc4xx_pic;

	/* give board specific code a chance to setup things */
	board_setup_irq();
	return;
}

static void
ppc4xx_restart(char *cmd)
{
	printk("%s\n", cmd);
	abort();
}

static void
ppc4xx_halt(void)
{
	printk("System Halted\n");
	__cli();
	while (1) ;
}

static void __init
ppc4xx_gdb_init(void)
{
#if !defined(CONFIG_BDI_SWITCH)
	/*
	 * The Abatron BDI JTAG debugger does not tolerate others
	 * mucking with the debug registers.
	 */
	mtspr(SPRN_DBCR0, (DBCR0_TDE | DBCR0_IDM));
	mtspr(SPRN_DBCR1, 0);
#endif
}

/*
 * This routine retrieves the internal processor frequency from the board
 * information structure, sets up the kernel timer decrementer based on
 * that value, enables the 4xx programmable interval timer (PIT) and sets
 * it up for auto-reload.
 */
static void __init
ppc4xx_calibrate_decr(void)
{
	unsigned int freq;
	bd_t *bip = (bd_t *) __res;

#if defined(CONFIG_WALNUT) || defined(CONFIG_CEDER) 	\
	|| defined(CONFIG_ASH) || defined(CONFIG_SYCAMORE)
	/* Openbios sets cpu  timers to CPU clk
	 * we want to use the external clk
	 * DCR CHCR1 (aka CPC0_CR1) bit CETE to 1 */

	mtdcr(DCRN_CHCR1, mfdcr(DCRN_CHCR1) & ~CHR1_CETE);
#endif

	freq = bip->bi_tbfreq;
//	tb_ticks_per_jiffy = freq / HZ;
//	tb_to_us = mulhwu_scale_factor(freq, 1000000);
	tb_ticks_per_jiffy = (freq + HZ/2) / HZ;
	tb_to_us = mulhwu_scale_factor(tb_ticks_per_jiffy*HZ, 1000000U);

	/* Set the time base to zero.
	   ** At 200 Mhz, time base will rollover in ~2925 years.
	 */

	mtspr(SPRN_TBWL, 0);
	mtspr(SPRN_TBWU, 0);

	/* Clear any pending timer interrupts */

	mtspr(SPRN_TSR, TSR_ENW | TSR_WIS | TSR_PIS | TSR_FIS);
	mtspr(SPRN_TCR, TCR_PIE | TCR_ARE);

	/* Set the PIT reload value and just let it run. */
	mtspr(SPRN_PIT, tb_ticks_per_jiffy);
}

#ifdef CONFIG_SERIAL_TEXT_DEBUG

/* We assume that the UART has already been initialized by the
   firmware or the boot loader */
static void
serial_putc(u8 * com_port, unsigned char c)
{
	while ((readb(com_port + (UART_LSR)) & UART_LSR_THRE) == 0) ;
	writeb(c, com_port);
}

static void
ppc4xx_progress(char *s, unsigned short hex)
{
	char c;
#ifdef SERIAL_DEBUG_IO_BASE
	u8 *com_port = (u8 *) SERIAL_DEBUG_IO_BASE;

	while ((c = *s++) != '\0') {
		serial_putc(com_port, c);
	}
	serial_putc(com_port, '\r');
	serial_putc(com_port, '\n');
#else
	printk("%s\r\n", s);
#endif
}
#endif				/* CONFIG_SERIAL_TEXT_DEBUG */

/*
 * IDE stuff.
 * should be generic for every IDE PCI chipset
 */
#if defined(CONFIG_BLK_DEV_IDEPCI)
static void
ppc4xx_ide_init_hwif_ports(hw_regs_t * hw, ide_ioreg_t data_port,
			   ide_ioreg_t ctrl_port, int *irq)
{
	int i;

	for (i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; ++i)
		hw->io_ports[i] = data_port + i - IDE_DATA_OFFSET;

	hw->io_ports[IDE_CONTROL_OFFSET] = ctrl_port;
}
#endif

TODC_ALLOC();

/*
 * Input(s):
 *   r3 - Optional pointer to a board information structure.
 *   r4 - Optional pointer to the physical starting address of the init RAM
 *        disk.
 *   r5 - Optional pointer to the physical ending address of the init RAM
 *        disk.
 *   r6 - Optional pointer to the physical starting address of any kernel
 *        command-line parameters.
 *   r7 - Optional pointer to the physical ending address of any kernel
 *        command-line parameters.
 */
void __init
platform_init(unsigned long r3, unsigned long r4, unsigned long r5,
	      unsigned long r6, unsigned long r7)
{
	parse_bootinfo(find_bootinfo());

	/*
	 * If we were passed in a board information, copy it into the
	 * residual data area.
	 */
	if (r3) {
		memcpy((void *) __res, (void *) (r3 + KERNELBASE),
		       sizeof (bd_t));

	}
#if defined(CONFIG_BLK_DEV_INITRD)
	/*
	 * If the init RAM disk has been configured in, and there's a valid
	 * starting address for it, set it up.
	 */
	if (r4) {
		initrd_start = r4 + KERNELBASE;
		initrd_end = r5 + KERNELBASE;
	}
#endif				/* CONFIG_BLK_DEV_INITRD */

	/* Copy the kernel command line arguments to a safe place. */

	if (r6) {
		*(char *) (r7 + KERNELBASE) = 0;
		strcpy(cmd_line, (char *) (r6 + KERNELBASE));
	}
#if defined(CONFIG_PPC405_WDT)
	ppc_md.heartbeat = ppc4xx_wdt_heartbeat;

/* Look for wdt= option on command line */
	if (strstr(cmd_line, "wdt=")) {
		int valid_wdt = 0;
		char *p, *q;
		for (q = cmd_line; (p = strstr(q, "wdt=")) != 0;) {
			q = p + 4;
			if (p > cmd_line && p[-1] != ' ')
				continue;
			wdt_period = simple_strtoul(q, &q, 0);
			valid_wdt = 1;
			++q;
		}
		wdt_enable = valid_wdt;
	}
#endif

	/* Initialize machine-dependency vectors */

	ppc_md.setup_arch = ppc4xx_setup_arch;
	ppc_md.show_percpuinfo = ppc4xx_show_percpuinfo;
	ppc_md.show_cpuinfo = ppc4xx_show_cpuinfo;
	ppc_md.init_IRQ = ppc4xx_init_IRQ;

	ppc_md.restart = ppc4xx_restart;
	ppc_md.power_off = ppc4xx_halt;
	ppc_md.halt = ppc4xx_halt;

	ppc_md.calibrate_decr = ppc4xx_calibrate_decr;

	ppc_md.find_end_of_memory = ppc4xx_find_end_of_memory;
	ppc_md.setup_io_mappings = m4xx_map_io;

#ifdef CONFIG_SERIAL_TEXT_DEBUG
	ppc_md.progress = ppc4xx_progress;
#endif
#ifdef CONFIG_KGDB
//	ppc_md.early_serial_map = early_uart_init;
#endif
#if defined(CONFIG_VT) && defined(CONFIG_PC_KEYBOARD)
#if defined(CONFIG_REDWOOD_4) && defined(CONFIG_STB_KB)
	redwood_irkb_init();
#else
	ppc_md.kbd_setkeycode = pckbd_setkeycode;
	ppc_md.kbd_getkeycode = pckbd_getkeycode;
	ppc_md.kbd_translate = pckbd_translate;
	ppc_md.kbd_unexpected_up = pckbd_unexpected_up;
	ppc_md.kbd_leds = pckbd_leds;
	ppc_md.kbd_init_hw = pckbd_init_hw;
#endif
#endif

/*
**   m8xx_setup.c, prep_setup.c use
**     defined(CONFIG_BLK_DEV_IDE) || defined(CONFIG_BLK_DEV_IDE_MODULE)
*/
#if defined (CONFIG_IDE)
#if defined(CONFIG_BLK_DEV_IDEPCI)
	ppc_ide_md.ide_init_hwif = ppc4xx_ide_init_hwif_ports;
#elif defined (CONFIG_DMA_NONPCI)	/* ON board IDE */
	ppc_ide_md.default_irq = nonpci_ide_default_irq;
	ppc_ide_md.ide_init_hwif = nonpci_ide_init_hwif_ports;
#endif
#endif
	board_init();

	return;
}
