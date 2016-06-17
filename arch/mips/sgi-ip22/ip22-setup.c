/*
 * ip22-setup.c: SGI specific setup, including init of the feature struct.
 *
 * Copyright (C) 1996 David S. Miller (dm@engr.sgi.com)
 * Copyright (C) 1997, 1998 Ralf Baechle (ralf@gnu.org)
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/kbd_ll.h>
#include <linux/kernel.h>
#include <linux/kdev_t.h>
#include <linux/types.h>
#include <linux/console.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/pc_keyb.h>

#include <asm/addrspace.h>
#include <asm/bcache.h>
#include <asm/bootinfo.h>
#include <asm/keyboard.h>
#include <asm/irq.h>
#include <asm/reboot.h>
#include <asm/ds1286.h>
#include <asm/floppy.h>
#include <asm/time.h>
#include <asm/gdb-stub.h>
#include <asm/io.h>
#include <asm/traps.h>
#include <asm/sgialib.h>
#include <asm/sgi/mc.h>
#include <asm/sgi/hpc3.h>
#include <asm/sgi/ip22.h>

#ifdef CONFIG_KGDB
extern void rs_kgdb_hook(int);
extern void breakpoint(void);
static int remote_debug = 0;
#endif

#ifdef CONFIG_EISA
extern struct fd_ops std_fd_ops;
#endif
extern struct rtc_ops ip22_rtc_ops;

unsigned long sgi_gfxaddr;

#define KBD_STAT_IBF		0x02	/* Keyboard input buffer full */

static void sgi_request_region(void)
{
	/* No I/O ports are being used on the Indy.  */
}

static int sgi_request_irq(void (*handler)(int, void *, struct pt_regs *))
{
	return request_irq(SGI_KEYBD_IRQ, handler, 0, "keyboard", NULL);
}

static int sgi_aux_request_irq(void (*handler)(int, void *, struct pt_regs *))
{
	/* Nothing to do, interrupt is shared with the keyboard hw  */
	return 0;
}

static void sgi_aux_free_irq(void)
{
	/* Nothing to do, interrupt is shared with the keyboard hw  */
}

static unsigned char sgi_read_input(void)
{
	return sgioc->kbdmouse.data;
}

static void sgi_write_output(unsigned char val)
{
	unsigned char status;

	do {
		status = sgioc->kbdmouse.command;
	} while (status & KBD_STAT_IBF);
	sgioc->kbdmouse.data = val;
}

static void sgi_write_command(unsigned char val)
{
	unsigned char status;

	do {
		status = sgioc->kbdmouse.command;
	} while (status & KBD_STAT_IBF);
	sgioc->kbdmouse.command = val;
}

static unsigned char sgi_read_status(void)
{
	return sgioc->kbdmouse.command;
}

struct kbd_ops ip22_kbd_ops = {
	.kbd_request_region	= sgi_request_region,
	.kbd_request_irq	= sgi_request_irq,

	.aux_request_irq	= sgi_aux_request_irq,
	.aux_free_irq		= sgi_aux_free_irq,

	.kbd_read_input		= sgi_read_input,
	.kbd_write_output	= sgi_write_output,
	.kbd_write_command	= sgi_write_command,
	.kbd_read_status	= sgi_read_status,
};

extern void ip22_be_init(void) __init;
extern void ip22_time_init(void) __init;

void __init ip22_setup(void)
{
	struct console_cmdline *c;
	char *con;
#ifdef CONFIG_KGDB
	char *kgdb_ttyd;
#endif

	board_be_init = ip22_be_init;
	ip22_time_init();

	/* Init the INDY HPC I/O controller.  Need to call this before
	 * fucking with the memory controller because it needs to know the
	 * boardID and whether this is a Guiness or a FullHouse machine.
	 */
	sgihpc_init();

	/* Init INDY memory controller. */
	sgimc_init();

#ifdef CONFIG_BOARD_SCACHE
	/* Now enable boardcaches, if any. */
	indy_sc_init();
#endif

	/* Set EISA IO port base for Indigo2 */
	set_io_port_base(KSEG1ADDR(0x00080000));

	/* Nothing registered console before us, so simply use first entry */
	c = &console_cmdline[0];
	/* ARCS console environment variable is set to "g?" for
	 * graphics console, it is set to "d" for the first serial
	 * line and "d2" for the second serial line.
	 */
	con = ArcGetEnvironmentVariable("console");
	if (con && *con == 'd') {
		static char options[8];
		char *baud = ArcGetEnvironmentVariable("dbaud");
		strcpy(c->name, "ttyS");
		c->index = *(con + 1) == '2' ? 1 : 0;
		if (baud) {
			strcpy(options, baud);
			c->options = options;
		}
	} else if (!con || *con != 'g') {
		/* Use ARC if we don't want serial ('d') or Newport ('g'). */
		prom_flags |= PROM_FLAG_USE_AS_CONSOLE;
		strcpy(c->name, "arc");
	}

#ifdef CONFIG_KGDB
	kgdb_ttyd = prom_getcmdline();
	if ((kgdb_ttyd = strstr(kgdb_ttyd, "kgdb=ttyd")) != NULL) {
		int line;
		kgdb_ttyd += strlen("kgdb=ttyd");
		if (*kgdb_ttyd != '1' && *kgdb_ttyd != '2')
			printk(KERN_INFO "KGDB: Uknown serial line /dev/ttyd%c"
			       ", falling back to /dev/ttyd1\n", *kgdb_ttyd);
		line = *kgdb_ttyd == '2' ? 0 : 1;
		printk(KERN_INFO "KGDB: Using serial line /dev/ttyd%d for "
		       "session\n", line ? 1 : 2);
		rs_kgdb_hook(line);

		printk(KERN_INFO "KGDB: Using serial line /dev/ttyd%d for "
		       "session, please connect your debugger\n", line ? 1:2);

		remote_debug = 1;
		/* Breakpoints and stuff are in sgi_irq_setup() */
	}
#endif

#ifdef CONFIG_VT
	conswitchp = &dummy_con;
#ifdef CONFIG_SGI_NEWPORT_CONSOLE
	if (con && *con == 'g'){
		ULONG *gfxinfo;
		ULONG * (*__vec)(void) = (void *) (long)
			*((_PULONG *)(long)((PROMBLOCK)->pvector + 0x20));

		gfxinfo = __vec();
		sgi_gfxaddr = ((gfxinfo[1] >= 0xa0000000
			       && gfxinfo[1] <= 0xc0000000)
			       ? gfxinfo[1] - 0xa0000000 : 0);

		/* newport addresses? */
		if (sgi_gfxaddr == 0x1f0f0000 || sgi_gfxaddr == 0x1f4f0000) {
			conswitchp = &newport_con;

			screen_info = (struct screen_info) {
				.orig_x			= 0,
				.orig_y 		= 0,
				.orig_video_page	= 0,
				.orig_video_mode	= 0,
				.orig_video_cols	= 160,
				.orig_video_ega_bx	= 0,
				.orig_video_lines	= 64,
				.orig_video_isVGA	= 0,
				.orig_video_points	= 16,
			};
		}
	}
#endif
#endif

/*
 * Warning: this is broken, means it has to be known at kernel compile time
 * if a floppy module might ever be loaded
 */
#if defined(CONFIG_EISA) && defined(CONFIG_BLK_DEV_FD)
	fd_ops = &std_fd_ops;
#endif
	rtc_ops = &ip22_rtc_ops;
	kbd_ops = &ip22_kbd_ops;
#ifdef CONFIG_PSMOUSE
	aux_device_present = 0xaa;
#endif
}
