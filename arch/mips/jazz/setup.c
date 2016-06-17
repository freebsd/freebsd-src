/*
 * Setup pointers to hardware-dependent routines.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 1997, 1998, 2001 by Ralf Baechle
 * Copyright (C) 2001 MIPS Technologies, Inc.
 */
#include <linux/config.h>
#include <linux/hdreg.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/console.h>
#include <linux/fb.h>
#include <linux/mc146818rtc.h>
#include <linux/ide.h>
#include <asm/bootinfo.h>
#include <asm/keyboard.h>
#include <asm/irq.h>
#include <asm/jazz.h>
#include <asm/jazzdma.h>
#include <asm/ptrace.h>
#include <asm/reboot.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/traps.h>

/*
 * Initial irq handlers.
 */
static void no_action(int cpl, void *dev_id, struct pt_regs *regs) { }

/*
 * IRQ2 is cascade interrupt to second interrupt controller
 */
static struct irqaction irq2  = { no_action, 0, 0, "cascade", NULL, NULL};

extern asmlinkage void jazz_handle_int(void);

extern void jazz_machine_restart(char *command);
extern void jazz_machine_halt(void);
extern void jazz_machine_power_off(void);

extern struct ide_ops std_ide_ops;
extern struct rtc_ops jazz_rtc_ops;
extern struct kbd_ops jazz_kbd_ops;
extern struct fd_ops *fd_ops;
extern struct fd_ops jazz_fd_ops;

void (*board_time_init)(struct irqaction *irq);

static void __init jazz_time_init(struct irqaction *irq)
{
        /* set the clock to 100 Hz */
        r4030_write_reg32(JAZZ_TIMER_INTERVAL, 9);
        i8259_setup_irq(JAZZ_TIMER_IRQ, irq);
}

static void __init jazz_irq_setup(void)
{
        set_except_vector(0, jazz_handle_int);
	r4030_write_reg16(JAZZ_IO_IRQ_ENABLE,
			  JAZZ_IE_ETHERNET |
			  JAZZ_IE_SCSI     |
			  JAZZ_IE_SERIAL1  |
			  JAZZ_IE_SERIAL2  |
 			  JAZZ_IE_PARALLEL |
			  JAZZ_IE_FLOPPY);
	r4030_read_reg16(JAZZ_IO_IRQ_SOURCE); /* clear pending IRQs */
	r4030_read_reg32(JAZZ_R4030_INVAL_ADDR); /* clear error bits */
	change_c0_status(ST0_IM, IE_IRQ4 | IE_IRQ3 | IE_IRQ2 | IE_IRQ1);
	/* set the clock to 100 Hz */
	r4030_write_reg32(JAZZ_TIMER_INTERVAL, 9);
	request_region(0x20, 0x20, "pic1");
	request_region(0xa0, 0x20, "pic2");
	i8259_setup_irq(2, &irq2);
}


void __init jazz_setup(void)
{
	/* Map 0xe0000000 -> 0x0:800005C0, 0xe0010000 -> 0x1:30000580 */
	add_wired_entry (0x02000017, 0x03c00017, 0xe0000000, PM_64K);

	/* Map 0xe2000000 -> 0x0:900005C0, 0xe3010000 -> 0x0:910005C0 */
	add_wired_entry (0x02400017, 0x02440017, 0xe2000000, PM_16M);

	/* Map 0xe4000000 -> 0x0:600005C0, 0xe4100000 -> 400005C0 */
	add_wired_entry (0x01800017, 0x01000017, 0xe4000000, PM_4M);

	irq_setup = jazz_irq_setup;
	set_io_port_base(JAZZ_PORT_BASE);
	if (mips_machtype == MACH_MIPS_MAGNUM_4000)
		EISA_bus = 1;
	isa_slot_offset = 0xe3000000;
	request_region(0x00,0x20,"dma1");
	request_region(0x40,0x20,"timer");
	request_region(0x80,0x10,"dma page reg");
	request_region(0xc0,0x20,"dma2");
        board_time_init = jazz_time_init;
	/* The RTC is outside the port address space */

	_machine_restart = jazz_machine_restart;
	_machine_halt = jazz_machine_halt;
	_machine_power_off = jazz_machine_power_off;

#ifdef CONFIG_BLK_DEV_IDE
	ide_ops = &std_ide_ops;
#endif
#ifdef CONFIG_BLK_DEV_FD
	fd_ops = &jazz_fd_ops;
#endif
#ifdef CONFIG_VT
	conswitchp = &dummy_con;
#endif

#warning "Somebody should check if screen_info is ok for Jazz."

	screen_info = (struct screen_info) {
		0, 0,		/* orig-x, orig-y */
		0,		/* unused */
		0,		/* orig_video_page */
		0,		/* orig_video_mode */
		160,		/* orig_video_cols */
		0, 0, 0,	/* unused, ega_bx, unused */
		64,		/* orig_video_lines */
		0,		/* orig_video_isVGA */
		16		/* orig_video_points */
	};

	rtc_ops = &jazz_rtc_ops;
	kbd_ops = &jazz_kbd_ops;

	vdma_init();
}
