/*
 * linux/drivers/char/dummy_keyb.c
 *
 * Allows CONFIG_VT on hardware without keyboards.
 *
 * Copyright (C) 1999, 2001 Bradley D. LaRonde
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * What is this for?
 *
 * Not all systems have keyboards.  Some don't even have a keyboard
 * port.  However, some of those systems have video support and can
 * use the virtual terminal support for display.  However, the virtual
 * terminal code expects a keyboard of some kind.  This driver keeps
 * the virtual terminal code happy by providing it a "keyboard", albeit
 * a very quiet one.
 *
 * If you want to use the virtual terminal support but your system
 * does not support a keyboard, define CONFIG_DUMMY_KEYB along with
 * CONFIG_VT.
 *
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/input.h>

void kbd_leds(unsigned char leds)
{
}

int kbd_setkeycode(unsigned int scancode, unsigned int keycode)
{
	return (scancode == keycode) ? 0 : -EINVAL;
}

int kbd_getkeycode(unsigned int scancode)
{
	return scancode;
}

#ifdef CONFIG_INPUT
static unsigned char e0_keys[128] = {
	0, 0, 0, KEY_KPCOMMA, 0, KEY_INTL3, 0, 0,		/* 0x00-0x07 */
	0, 0, 0, 0, KEY_LANG1, KEY_LANG2, 0, 0,			/* 0x08-0x0f */
	0, 0, 0, 0, 0, 0, 0, 0,					/* 0x10-0x17 */
	0, 0, 0, 0, KEY_KPENTER, KEY_RIGHTCTRL, KEY_VOLUMEUP, 0,/* 0x18-0x1f */
	0, 0, 0, 0, 0, KEY_VOLUMEDOWN, KEY_MUTE, 0,		/* 0x20-0x27 */
	0, 0, 0, 0, 0, 0, 0, 0,					/* 0x28-0x2f */
	0, 0, 0, 0, 0, KEY_KPSLASH, 0, KEY_SYSRQ,		/* 0x30-0x37 */
	KEY_RIGHTALT, KEY_BRIGHTNESSUP, KEY_BRIGHTNESSDOWN, 
		KEY_EJECTCD, 0, 0, 0, 0,			/* 0x38-0x3f */
	0, 0, 0, 0, 0, 0, 0, KEY_HOME,				/* 0x40-0x47 */
	KEY_UP, KEY_PAGEUP, 0, KEY_LEFT, 0, KEY_RIGHT, 0, KEY_END, /* 0x48-0x4f */
	KEY_DOWN, KEY_PAGEDOWN, KEY_INSERT, KEY_DELETE, 0, 0, 0, 0, /* 0x50-0x57 */
	0, 0, 0, KEY_LEFTMETA, KEY_RIGHTMETA, KEY_COMPOSE, KEY_POWER, 0, /* 0x58-0x5f */
	0, 0, 0, 0, 0, 0, 0, 0,					/* 0x60-0x67 */
	0, 0, 0, 0, 0, 0, 0, KEY_MACRO,				/* 0x68-0x6f */
	0, 0, 0, 0, 0, 0, 0, 0,					/* 0x70-0x77 */
	0, 0, 0, 0, 0, 0, 0, 0					/* 0x78-0x7f */
};

int kbd_translate(unsigned char scancode, unsigned char *keycode,
	char raw_mode)
{
	/* This code was copied from char/pc_keyb.c and will be
	 * superflous when the input layer is fully integrated.
	 * We don't need the high_keys handling, so this part
	 * has been removed.
	 */
	static int prev_scancode = 0;

	/* special prefix scancodes.. */
	if (scancode == 0xe0 || scancode == 0xe1) {
		prev_scancode = scancode;
		return 0;
	}

	scancode &= 0x7f;

	if (prev_scancode) {
		if (prev_scancode != 0xe0) {
			if (prev_scancode == 0xe1 && scancode == 0x1d) {
				prev_scancode = 0x100;
				return 0;
			} else if (prev_scancode == 0x100 && scancode == 0x45) {
				*keycode = KEY_PAUSE;
				prev_scancode = 0;
			} else {
				if (!raw_mode)
					printk(KERN_INFO "keyboard: unknown e1 escape sequence\n");
				prev_scancode = 0;
				return 0;
			}
		} else {
			prev_scancode = 0;
			if (scancode == 0x2a || scancode == 0x36)
				return 0;
		}
		if (e0_keys[scancode])
			*keycode = e0_keys[scancode];
		else {
			if (!raw_mode)
				printk(KERN_INFO "keyboard: unknown scancode e0 %02x\n",
				       scancode);
			return 0;
		}
	} else {
		switch (scancode) {
		case  91: scancode = KEY_LINEFEED; break;
		case  92: scancode = KEY_KPEQUAL; break;
		case 125: scancode = KEY_INTL1; break;
		}
		*keycode = scancode;
	}
	return 1;
}

#else
int kbd_translate(unsigned char scancode, unsigned char *keycode,
	char raw_mode)
{
	*keycode = scancode;

	return 1;
}
#endif

char kbd_unexpected_up(unsigned char keycode)
{
	return 0x80;
}

void __init kbd_init_hw(void)
{
	printk("Dummy keyboard driver installed.\n");
}
