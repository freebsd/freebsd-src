/*
 * arch/ppc/platforms/pplus_setup.c
 *
 * Board setup routines for MCG PowerPlus
 *
 * Author: Randy Vinson <rvinson@mvista.com>
 *
 * Derived from original PowerPlus PReP work by
 * Cort Dougan, Johnnie Peters, Matt Porter, and
 * Troy Benjegerdes.
 *
 * Copyright 2001-2002 MontaVista Software Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/config.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/user.h>
#include <linux/a.out.h>
#include <linux/tty.h>
#include <linux/major.h>
#include <linux/interrupt.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/blk.h>
#include <linux/ioport.h>
#include <linux/console.h>
#include <linux/timex.h>
#include <linux/pci.h>
#include <linux/irq.h>
#include <linux/ide.h>
#include <linux/kdev_t.h>
#include <linux/seq_file.h>
#include <linux/serial.h>
#include <linux/serialP.h>

#include <asm/sections.h>
#include <asm/mmu.h>
#include <asm/processor.h>
#include <asm/system.h>
#include <asm/residual.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/cache.h>
#include <asm/dma.h>
#include <asm/machdep.h>
#include <asm/mk48t59.h>
#include <asm/prep_nvram.h>
#include <asm/raven.h>
#include <asm/keyboard.h>
#include <asm/vga.h>
#include <asm/time.h>

#include <asm/i8259.h>
#include <asm/open_pic.h>
#include <asm/pplus.h>
#include <asm/todc.h>
#include <asm/bootinfo.h>
#include <asm/serial.h>

#undef DUMP_DBATS

TODC_ALLOC();

extern int pckbd_setkeycode(unsigned int scancode, unsigned int keycode);
extern int pckbd_getkeycode(unsigned int scancode);
extern int pckbd_translate(unsigned char scancode, unsigned char *keycode,
		char raw_mode);
extern char pckbd_unexpected_up(unsigned char keycode);
extern void pckbd_leds(unsigned char leds);
extern void pckbd_init_hw(void);
extern unsigned char pckbd_sysrq_xlate[128];

extern char saved_command_line[];

extern void pplus_setup_hose(void);
extern void pplus_set_VIA_IDE_native(void);

extern unsigned long loops_per_jiffy;

extern void gen550_progress(char *, unsigned short);
extern void gen550_init(int, struct serial_struct *);

static int
pplus_show_cpuinfo(struct seq_file *m)
{
	extern char *Motherboard_map_name;

	seq_printf(m, "vendor\t\t: Motorola MCG\n");
	seq_printf(m, "machine\t\t: %s\n", Motherboard_map_name);

	return 0;
}

static void __init
pplus_setup_arch(void)
{
	unsigned char reg;

	if ( ppc_md.progress )
		ppc_md.progress("pplus_setup_arch: enter", 0);

	/* init to some ~sane value until calibrate_delay() runs */
	loops_per_jiffy = 50000000;

	if ( ppc_md.progress )
		ppc_md.progress("pplus_setup_arch: find_bridges", 0);

	/* Setup PCI host bridge */
	pplus_setup_hose();

	/* Set up floppy in PS/2 mode */
	outb(0x09, SIO_CONFIG_RA);
	reg = inb(SIO_CONFIG_RD);
	reg = (reg & 0x3F) | 0x40;
	outb(reg, SIO_CONFIG_RD);
	outb(reg, SIO_CONFIG_RD);	/* Have to write twice to change! */

	/* Enable L2.  Assume we don't need to flush -- Cort*/
	*(unsigned char *)(0x8000081c) |= 3;

#ifdef CONFIG_BLK_DEV_INITRD
	if (initrd_start)
		ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0); /* /dev/ram */
	else
#endif
#ifdef CONFIG_ROOT_NFS
		ROOT_DEV = to_kdev_t(0x00ff); /* /dev/nfs */
#else
		ROOT_DEV = to_kdev_t(0x0802); /* /dev/sda2 */
#endif

	printk(KERN_INFO "Motorola PowerPlus Platform\n");
	printk(KERN_INFO "Port by MontaVista Software, Inc. (source@mvista.com)\n");

	if ( ppc_md.progress )
		ppc_md.progress("pplus_setup_arch: raven_init", 0);

	raven_init();

#ifdef CONFIG_VGA_CONSOLE
	/* remap the VGA memory */
	vgacon_remap_base = 0xf0000000;
	conswitchp = &vga_con;
#elif defined(CONFIG_DUMMY_CONSOLE)
	conswitchp = &dummy_con;
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
	if ( ppc_md.progress )
		ppc_md.progress("pplus_setup_arch: exit", 0);
}

static void
pplus_restart(char *cmd)
{
	unsigned long i = 10000;

	__cli();

	/* set VIA IDE controller into native mode */
	pplus_set_VIA_IDE_native();

	/* set exception prefix high - to the prom */
	_nmask_and_or_msr(0, MSR_IP);

	/* make sure bit 0 (reset) is a 0 */
	outb( inb(0x92) & ~1L , 0x92 );
	/* signal a reset to system control port A - soft reset */
	outb( inb(0x92) | 1 , 0x92 );

	while ( i != 0 ) i++;
	panic("restart failed\n");
}

static void
pplus_halt(void)
{
	unsigned long flags;
	__cli();
	/* set exception prefix high - to the prom */
	save_flags( flags );
	restore_flags( flags|MSR_IP );

	/* make sure bit 0 (reset) is a 0 */
	outb( inb(0x92) & ~1L , 0x92 );
	/* signal a reset to system control port A - soft reset */
	outb( inb(0x92) | 1 , 0x92 );

	while ( 1 ) ;
	/*
	 * Not reached
	 */
}

static void
pplus_power_off(void)
{
	pplus_halt();
}

static unsigned int
pplus_irq_cannonicalize(u_int irq)
{
	if (irq == 2)
	{
		return 9;
	}
	else
	{
		return irq;
	}
}

static void __init
pplus_init_IRQ(void)
{
	int i;

	if (OpenPIC_Addr != NULL) {
		openpic_set_sources(0, 16, OpenPIC_Addr+0x10000);
		openpic_init(NUM_8259_INTERRUPTS);
		openpic_hookup_cascade(NUM_8259_INTERRUPTS, "82c59 cascade",
					i8259_irq);
	}

	for ( i = 0 ; i < NUM_8259_INTERRUPTS ; i++ )
		irq_desc[i].handler = &i8259_pic;
	i8259_init(0);
}

#if defined(CONFIG_BLK_DEV_IDE) || defined(CONFIG_BLK_DEV_IDE_MODULE)
/*
 * IDE stuff.
 */
static int
pplus_ide_default_irq(ide_ioreg_t base)
{
	switch (base) {
		case 0x1f0: return 14;
		case 0x170: return 15;
		default: return 0;
	}
}

static ide_ioreg_t
pplus_ide_default_io_base(int index)
{
	switch (index) {
		case 0: return 0x1f0;
		case 1: return 0x170;
		default:
			return 0;
	}
}

static void __init
pplus_ide_init_hwif_ports (hw_regs_t *hw, ide_ioreg_t data_port, ide_ioreg_t ctrl_port, int *irq)
{
	ide_ioreg_t reg = data_port;
	int i;

	for (i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++) {
		hw->io_ports[i] = reg;
		reg += 1;
	}
	if (ctrl_port) {
		hw->io_ports[IDE_CONTROL_OFFSET] = ctrl_port;
	} else {
		hw->io_ports[IDE_CONTROL_OFFSET] = hw->io_ports[IDE_DATA_OFFSET] + 0x206;
	}
	if (irq != NULL)
		*irq = pplus_ide_default_irq(data_port);
}
#endif

#ifdef CONFIG_SMP
/* PowerPlus (MTX) support */
static int __init
smp_pplus_probe(void)
{
	extern int mot_multi;

	if (mot_multi) {
		openpic_request_IPIs();
		smp_hw_index[1] = 1;
		return 2;
	}

	return 1;
}

static void __init
smp_pplus_kick_cpu(int nr)
{
	*(unsigned long *)KERNELBASE = nr;
	asm volatile("dcbf 0,%0"::"r"(KERNELBASE):"memory");
	printk("CPU1 reset, waiting\n");
}

static void __init
smp_pplus_setup_cpu(int cpu_nr)
{
	if (OpenPIC_Addr)
		do_openpic_setup_cpu();
}

static struct smp_ops_t pplus_smp_ops = {
	smp_openpic_message_pass,
	smp_pplus_probe,
	smp_pplus_kick_cpu,
	smp_pplus_setup_cpu,
};
#endif /* CONFIG_SMP */

#ifdef DUMP_DBATS
static void print_dbat(int idx, u32 bat) {

	char str[64];

	sprintf(str, "DBAT%c%c = 0x%08x\n",
		(char)((idx - DBAT0U) / 2) + '0',
		(idx & 1) ? 'L' : 'U', bat);
	ppc_md.progress(str, 0);
}

#define DUMP_DBAT(x) \
	do { \
	u32 __temp = mfspr(x);\
	print_dbat(x, __temp); \
	} while (0)

static void dump_dbats(void) {

	if (ppc_md.progress) {
		DUMP_DBAT(DBAT0U);
		DUMP_DBAT(DBAT0L);
		DUMP_DBAT(DBAT1U);
		DUMP_DBAT(DBAT1L);
		DUMP_DBAT(DBAT2U);
		DUMP_DBAT(DBAT2L);
		DUMP_DBAT(DBAT3U);
		DUMP_DBAT(DBAT3L);
	}
}
#endif

static unsigned long __init
pplus_find_end_of_memory(void)
{
	unsigned long total;

	if (ppc_md.progress)
		ppc_md.progress("pplus_find_end_of_memory",0);

#ifdef DUMP_DBATS
	dump_dbats();
#endif

	total = pplus_get_mem_size(0xfef80000);
	return (total);
}

static void __init
pplus_map_io(void)
{
	io_block_mapping(0x80000000, 0x80000000, 0x10000000, _PAGE_IO);
	io_block_mapping(0xf0000000, 0xc0000000, 0x08000000, _PAGE_IO);
}

static void __init
pplus_init2(void)
{
#ifdef CONFIG_NVRAM
	request_region(PREP_NVRAM_AS0, 0x8, "nvram");
#endif
	request_region(0x20,0x20,"pic1");
	request_region(0xa0,0x20,"pic2");
	request_region(0x00,0x20,"dma1");
	request_region(0x40,0x20,"timer");
	request_region(0x80,0x10,"dma page reg");
	request_region(0xc0,0x20,"dma2");
}

/*
 * Set BAT 2 to access 0x8000000 so progress messages will work and set BAT 3
 * to 0xf0000000 to access Falcon/Raven or Hawk registers
 */
static __inline__ void
pplus_set_bat(void)
{
	static int      mapping_set = 0;

	if (!mapping_set) {

		/* wait for all outstanding memory accesses to complete */
		mb();

		/* setup DBATs */
		mtspr(DBAT2U, 0x80001ffe);
		mtspr(DBAT2L, 0x8000002a);
		mtspr(DBAT3U, 0xf0001ffe);
		mtspr(DBAT3L, 0xf000002a);

		/* wait for updates */
		mb();

		mapping_set = 1;
	}

	return;
}

#if defined(CONFIG_SERIAL) && \
	(defined(CONFIG_SERIAL_TEXT_DEBUG) || defined(CONFIG_KGDB))
extern struct serial_state rs_table[];

static void __init
pplus_early_serial_map(void)
{
	struct serial_struct serial_req;

	/* Setup serial port access */
	memset(&serial_req, 0, sizeof(serial_req));

	/*
	 * rs_table[] already set up by <asm/pc_serial.h> so use that info for
	 * gen550_init().  This also means early_serial_setup() doesn't
	 * have to be called.
	 */
	serial_req.port = rs_table[0].port;
	serial_req.io_type = rs_table[0].io_type;
	serial_req.iomem_reg_shift = rs_table[0].iomem_reg_shift;
#if defined(CONFIG_SERIAL_TEXT_DEBUG) || defined(CONFIG_KGDB)
	gen550_init(0, &serial_req);
#endif

	serial_req.port = rs_table[1].port;
	serial_req.io_type = rs_table[1].io_type;
	serial_req.iomem_reg_shift = rs_table[1].iomem_reg_shift;
#if defined(CONFIG_SERIAL_TEXT_DEBUG) || defined(CONFIG_KGDB)
	gen550_init(1, &serial_req);
#endif
}
#endif

void __init
platform_init(unsigned long r3, unsigned long r4, unsigned long r5,
		unsigned long r6, unsigned long r7)
{
	parse_bootinfo(find_bootinfo());

	/* Map in board regs, etc. */
	pplus_set_bat();

	isa_io_base = PREP_ISA_IO_BASE;
	isa_mem_base = PREP_ISA_MEM_BASE;
	pci_dram_offset = PREP_PCI_DRAM_OFFSET;
	ISA_DMA_THRESHOLD = 0x00ffffff;
	DMA_MODE_READ = 0x44;
	DMA_MODE_WRITE = 0x48;

	ppc_md.setup_arch     = pplus_setup_arch;
	ppc_md.show_percpuinfo = NULL;
	ppc_md.show_cpuinfo    = pplus_show_cpuinfo;
	ppc_md.irq_cannonicalize = pplus_irq_cannonicalize;
	ppc_md.init_IRQ       = pplus_init_IRQ;
	/* this gets changed later on if we have an OpenPIC -- Cort */
	ppc_md.get_irq        = i8259_irq;
	ppc_md.init           = pplus_init2;

	ppc_md.restart        = pplus_restart;
	ppc_md.power_off      = pplus_power_off;
	ppc_md.halt           = pplus_halt;

	TODC_INIT(TODC_TYPE_MK48T59, PREP_NVRAM_AS0, PREP_NVRAM_AS1,
		  PREP_NVRAM_DATA, 8);

	ppc_md.time_init      = todc_time_init;
	ppc_md.set_rtc_time   = todc_set_rtc_time;
	ppc_md.get_rtc_time   = todc_get_rtc_time;
	ppc_md.calibrate_decr = todc_calibrate_decr;
	ppc_md.nvram_read_val = todc_m48txx_read_val;
	ppc_md.nvram_write_val = todc_m48txx_write_val;

	ppc_md.find_end_of_memory = pplus_find_end_of_memory;
	ppc_md.setup_io_mappings = pplus_map_io;

#if defined(CONFIG_BLK_DEV_IDE) || defined(CONFIG_BLK_DEV_IDE_MODULE)
	ppc_ide_md.default_irq = pplus_ide_default_irq;
	ppc_ide_md.default_io_base = pplus_ide_default_io_base;
	ppc_ide_md.ide_init_hwif = pplus_ide_init_hwif_ports;
#endif

#ifdef CONFIG_VT
	ppc_md.kbd_setkeycode    = pckbd_setkeycode;
	ppc_md.kbd_getkeycode    = pckbd_getkeycode;
	ppc_md.kbd_translate     = pckbd_translate;
	ppc_md.kbd_unexpected_up = pckbd_unexpected_up;
	ppc_md.kbd_leds          = pckbd_leds;
	ppc_md.kbd_init_hw       = pckbd_init_hw;
#ifdef CONFIG_MAGIC_SYSRQ
	ppc_md.ppc_kbd_sysrq_xlate	 = pckbd_sysrq_xlate;
	SYSRQ_KEY = 0x54;
#endif
#endif

#if defined(CONFIG_SERIAL) && \
	(defined(CONFIG_SERIAL_TEXT_DEBUG) || defined(CONFIG_KGDB))
	pplus_early_serial_map();

#ifdef CONFIG_SERIAL_TEXT_DEBUG
	ppc_md.progress = gen550_progress;
#endif /* CONFIG_SERIAL_TEXT_DEBUG */
#endif

#ifdef CONFIG_SMP
	ppc_md.smp_ops		 = &pplus_smp_ops;
#endif /* CONFIG_SMP */
}
