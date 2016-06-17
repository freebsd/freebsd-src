/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/mc146818rtc.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#ifdef CONFIG_BLK_DEV_IDE
#include <linux/ide.h>
#endif

#include <asm/cpu.h>
#include <asm/bootinfo.h>
#include <asm/irq.h>
#include <asm/mips-boards/generic.h>
#include <asm/mips-boards/prom.h>
#include <asm/mips-boards/malta.h>
#include <asm/mips-boards/maltaint.h>
#ifdef CONFIG_BLK_DEV_FD
#include <asm/floppy.h>
#endif
#include <asm/dma.h>
#include <asm/time.h>
#include <asm/traps.h>
#ifdef CONFIG_PC_KEYB
#include <asm/keyboard.h>
#endif
#ifdef CONFIG_VT
#include <linux/console.h>
#endif

#ifdef CONFIG_KGDB
extern void rs_kgdb_hook(int);
int remote_debug = 0;
#endif

extern struct ide_ops std_ide_ops;
extern struct fd_ops std_fd_ops;
extern struct rtc_ops malta_rtc_ops;
extern struct kbd_ops std_kbd_ops;

extern void mips_reboot_setup(void);

extern void mips_time_init(void);
extern void mips_timer_setup(struct irqaction *irq);
extern unsigned long mips_rtc_get_time(void);

struct resource standard_io_resources[] = {
	{ "dma1", 0x00, 0x1f, IORESOURCE_BUSY },
	{ "timer", 0x40, 0x5f, IORESOURCE_BUSY },
	{ "dma page reg", 0x80, 0x8f, IORESOURCE_BUSY },
	{ "dma2", 0xc0, 0xdf, IORESOURCE_BUSY },
};

#define STANDARD_IO_RESOURCES (sizeof(standard_io_resources)/sizeof(struct resource))

const char *get_system_type(void)
{
	return "MIPS Malta";
}

void __init malta_setup(void)
{
#ifdef CONFIG_KGDB
	int rs_putDebugChar(char);
	char rs_getDebugChar(void);
	extern int (*generic_putDebugChar)(char);
	extern char (*generic_getDebugChar)(void);
#endif
	char *argptr;
	int i;

	/* Request I/O space for devices used on the Malta board. */
	for (i = 0; i < STANDARD_IO_RESOURCES; i++)
		request_resource(&ioport_resource, standard_io_resources+i);

	/*
	 * Enable DMA channel 4 (cascade channel) in the PIIX4 south bridge.
	 */
	enable_dma(4);

#ifdef CONFIG_SERIAL_CONSOLE
	argptr = prom_getcmdline();
	if ((argptr = strstr(argptr, "console=")) == NULL) {
		argptr = prom_getcmdline();
		strcat(argptr, " console=ttyS0,38400");
	}
#endif

#ifdef CONFIG_KGDB
	argptr = prom_getcmdline();
	if ((argptr = strstr(argptr, "kgdb=ttyS")) != NULL) {
		int line;
		argptr += strlen("kgdb=ttyS");
		if (*argptr != '0' && *argptr != '1')
			printk("KGDB: Uknown serial line /dev/ttyS%c, "
			       "falling back to /dev/ttyS1\n", *argptr);
		line = *argptr == '0' ? 0 : 1;
		printk("KGDB: Using serial line /dev/ttyS%d for session\n",
		       line ? 1 : 0);

		rs_kgdb_hook(line);
		generic_putDebugChar = rs_putDebugChar;
		generic_getDebugChar = rs_getDebugChar;

		prom_printf("KGDB: Using serial line /dev/ttyS%d for session, "
			    "please connect your debugger\n", line ? 1 : 0);

		remote_debug = 1;
		/* Breakpoints are in init_IRQ() */
	}
#endif

	argptr = prom_getcmdline();
	if ((argptr = strstr(argptr, "nofpu")) != NULL)
		cpu_data[0].options &= ~MIPS_CPU_FPU;

	rtc_ops = &malta_rtc_ops;

#ifdef CONFIG_BLK_DEV_IDE
        ide_ops = &std_ide_ops;
#endif
#ifdef CONFIG_BLK_DEV_FD
        fd_ops = &std_fd_ops;
#endif
#ifdef CONFIG_PC_KEYB
	kbd_ops = &std_kbd_ops;
#endif
#ifdef CONFIG_VT
#if defined(CONFIG_VGA_CONSOLE)
        conswitchp = &vga_con;

	screen_info = (struct screen_info) {
		0, 25,			/* orig-x, orig-y */
		0,			/* unused */
		0,			/* orig-video-page */
		0,			/* orig-video-mode */
		80,			/* orig-video-cols */
		0,0,0,			/* ega_ax, ega_bx, ega_cx */
		25,			/* orig-video-lines */
		1,			/* orig-video-isVGA */
		16			/* orig-video-points */
	};
#elif defined(CONFIG_DUMMY_CONSOLE)
        conswitchp = &dummy_con;
#endif
#endif
	mips_reboot_setup();

	board_time_init = mips_time_init;
	board_timer_setup = mips_timer_setup;
	rtc_get_time = mips_rtc_get_time;
}
