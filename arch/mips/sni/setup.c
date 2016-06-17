/*
 * Setup pointers to hardware-dependent routines.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 1997, 1998, 2000 by Ralf Baechle
 */
#include <asm/ptrace.h>
#include <linux/config.h>
#include <linux/hdreg.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/timex.h>
#include <linux/pci.h>
#include <linux/mc146818rtc.h>
#include <linux/console.h>
#include <linux/fb.h>
#include <linux/pc_keyb.h>
#include <linux/ide.h>

#include <asm/bcache.h>
#include <asm/bootinfo.h>
#include <asm/keyboard.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/processor.h>
#include <asm/reboot.h>
#include <asm/sni.h>
#include <asm/time.h>
#include <asm/traps.h>

extern void sni_machine_restart(char *command);
extern void sni_machine_halt(void);
extern void sni_machine_power_off(void);

extern struct ide_ops std_ide_ops;
extern struct rtc_ops std_rtc_ops;
extern struct kbd_ops std_kbd_ops;

static void __init sni_rm200_pci_time_init(struct irqaction *irq)
{
	/* set the clock to 100 Hz */
	outb_p(0x34,0x43);		/* binary, mode 2, LSB/MSB, ch 0 */
	outb_p(LATCH & 0xff , 0x40);	/* LSB */
	outb(LATCH >> 8 , 0x40);	/* MSB */
	setup_irq(0, irq);
}


extern unsigned char sni_map_isa_cache;

/*
 * A bit more gossip about the iron we're running on ...
 */
static inline void sni_pcimt_detect(void)
{
	char boardtype[80];
	unsigned char csmsr;
	char *p = boardtype;
	unsigned int asic;

	csmsr = *(volatile unsigned char *)PCIMT_CSMSR;

	p += sprintf(p, "%s PCI", (csmsr & 0x80) ? "RM200" : "RM300");
	if ((csmsr & 0x80) == 0)
		p += sprintf(p, ", board revision %s",
		             (csmsr & 0x20) ? "D" : "C");
	asic = csmsr & 0x80;
	asic = (csmsr & 0x08) ? asic : !asic;
	p += sprintf(p, ", ASIC PCI Rev %s", asic ? "1.0" : "1.1");
	printk("%s.\n", boardtype);
}

void __init sni_rm200_pci_setup(void)
{
	sni_pcimt_detect();
	sni_pcimt_sc_init();

	set_io_port_base(SNI_PORT_BASE);

	/*
	 * Setup (E)ISA I/O memory access stuff
	 */
	isa_slot_offset = 0xb0000000;
	// sni_map_isa_cache = 0;
	EISA_bus = 1;

	request_region(0x00,0x20,"dma1");
	request_region(0x40,0x20,"timer");
	/* XXX FIXME: CONFIG_RTC */
	request_region(0x70,0x10,"rtc");
	request_region(0x80,0x10,"dma page reg");
	request_region(0xc0,0x20,"dma2");
	board_time_init = sni_rm200_pci_time_init;

	_machine_restart = sni_machine_restart;
	_machine_halt = sni_machine_halt;
	_machine_power_off = sni_machine_power_off;

	aux_device_present = 0xaa;

	/*
	 * Some cluefull person has placed the PCI config data directly in
	 * the I/O port space ...
	 */
	request_region(0xcfc,0x04,"PCI config data");

#ifdef CONFIG_BLK_DEV_IDE
	ide_ops = &std_ide_ops;
#endif
	conswitchp = &vga_con;

	screen_info = (struct screen_info) {
		0, 0,		/* orig-x, orig-y */
		0,		/* unused */
		52,		/* orig_video_page */
		3,		/* orig_video_mode */
		80,		/* orig_video_cols */
		4626, 3, 9,	/* unused, ega_bx, unused */
		50,		/* orig_video_lines */
		0x22,		/* orig_video_isVGA */
		16		/* orig_video_points */
	};

	rtc_ops = &std_rtc_ops;
	kbd_ops = &std_kbd_ops;
#ifdef CONFIG_PSMOUSE
	aux_device_present = 0xaa;
#endif
}
