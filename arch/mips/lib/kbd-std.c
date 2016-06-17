/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Routines for standard PC style keyboards accessible via I/O ports.
 *
 * Copyright (C) 1998, 1999 by Ralf Baechle
 */
#include <linux/config.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/pc_keyb.h>
#include <asm/keyboard.h>
#include <asm/io.h>

#define KEYBOARD_IRQ 1
#define AUX_IRQ 12

static void std_kbd_request_region(void)
{
#ifdef CONFIG_MIPS_ITE8172
	request_region(0x14000060, 16, "keyboard");
#else
	request_region(0x60, 16, "keyboard");
#endif
}

static int std_kbd_request_irq(void (*handler)(int, void *, struct pt_regs *))
{
	return request_irq(KEYBOARD_IRQ, handler, 0, "keyboard", NULL);
}

static int std_aux_request_irq(void (*handler)(int, void *, struct pt_regs *))
{
	return request_irq(AUX_IRQ, handler, 0, "PS/2 Mouse", NULL);
}

static void std_aux_free_irq(void)
{
	free_irq(AUX_IRQ, NULL);
}

static unsigned char std_kbd_read_input(void)
{
	return inb(KBD_DATA_REG);
}

static void std_kbd_write_output(unsigned char val)
{
	int status;

	do {
		status = inb(KBD_CNTL_REG);
	} while (status & KBD_STAT_IBF);
	outb(val, KBD_DATA_REG);
}

static void std_kbd_write_command(unsigned char val)
{
	int status;

	do {
		status = inb(KBD_CNTL_REG);
	} while (status & KBD_STAT_IBF);
	outb(val, KBD_CNTL_REG);
}

static unsigned char std_kbd_read_status(void)
{
	return inb(KBD_STATUS_REG);
}

struct kbd_ops std_kbd_ops = {
	std_kbd_request_region,
	std_kbd_request_irq,

	std_aux_request_irq,
	std_aux_free_irq,

	std_kbd_read_input,
	std_kbd_write_output,
	std_kbd_write_command,
	std_kbd_read_status
};
