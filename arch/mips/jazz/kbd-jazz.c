/*
 * Low-level hardware access stuff for Jazz family machines.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995, 1996, 1997, 1998 by Ralf Baechle
 */
#include <linux/sched.h>
#include <linux/pc_keyb.h>
#include <asm/keyboard.h>
#include <asm/jazz.h>

#define jazz_kh ((keyboard_hardware *) JAZZ_KEYBOARD_ADDRESS)

static void jazz_request_region(void)
{
	/* No I/O ports are being used on Jazz.  */
}

static int jazz_request_irq(void (*handler)(int, void *, struct pt_regs *))
{
	int res;

	res = request_irq(JAZZ_KEYBOARD_IRQ, handler, 0, "keyboard", NULL);
	if (res != 0)
		return res;

	/* jazz_request_irq() should do this ...  */
	r4030_write_reg16(JAZZ_IO_IRQ_ENABLE,
	                  r4030_read_reg16(JAZZ_IO_IRQ_ENABLE)
	                  | JAZZ_IE_KEYBOARD);

	return 0;
}

static int jazz_aux_request_irq(void (*handler)(int, void *, struct pt_regs *))
{
	int ret;

	ret = request_irq(JAZZ_MOUSE_IRQ, handler, 0, "PS/2 Mouse", NULL);
	if (ret != 0)
		return ret;

		r4030_write_reg16(JAZZ_IO_IRQ_ENABLE,
				  r4030_read_reg16(JAZZ_IO_IRQ_ENABLE) |
				  JAZZ_IE_MOUSE);
	return 0;
}

static void jazz_aux_free_irq(void)
{
	r4030_write_reg16(JAZZ_IO_IRQ_ENABLE,
	                  r4030_read_reg16(JAZZ_IO_IRQ_ENABLE)
	                  | JAZZ_IE_MOUSE);
	free_irq(JAZZ_MOUSE_IRQ, NULL);
}

static unsigned char jazz_read_input(void)
{
	return jazz_kh->data;
}

static void jazz_write_output(unsigned char val)
{
	int status;

	do {
		status = jazz_kh->command;
	} while (status & KBD_STAT_IBF);
	jazz_kh->data = val;
}

static void jazz_write_command(unsigned char val)
{
	int status;

	do {
		status = jazz_kh->command;
	} while (status & KBD_STAT_IBF);
	jazz_kh->command = val;
}

static unsigned char jazz_read_status(void)
{
	return jazz_kh->command;
}

struct kbd_ops jazz_kbd_ops = {
	jazz_request_region,
	jazz_request_irq,

	jazz_aux_request_irq,
	jazz_aux_free_irq,

	jazz_read_input,
	jazz_write_output,
	jazz_write_command,
	jazz_read_status
};
