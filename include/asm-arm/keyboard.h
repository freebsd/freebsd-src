/*
 *  linux/include/asm-arm/keyboard.h
 *
 *  Copyright (C) 1998 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Keyboard driver definitions for ARM
 */
#ifndef __ASM_ARM_KEYBOARD_H
#define __ASM_ARM_KEYBOARD_H

#include <linux/kd.h>
#include <linux/pm.h>

/*
 * We provide a unified keyboard interface when in VC_MEDIUMRAW
 * mode.  This means that all keycodes must be common between
 * all supported keyboards.  This unfortunately puts us at odds
 * with the PC keyboard interface chip... but we can't do anything
 * about that now.
 */
#ifdef __KERNEL__

extern int  (*k_setkeycode)(unsigned int, unsigned int);
extern int  (*k_getkeycode)(unsigned int);
extern int  (*k_translate)(unsigned char, unsigned char *, char);
extern char (*k_unexpected_up)(unsigned char);
extern void (*k_leds)(unsigned char);


static inline int kbd_setkeycode(unsigned int sc, unsigned int kc)
{
	int ret = -EINVAL;

	if (k_setkeycode)
		ret = k_setkeycode(sc, kc);

	return ret;
}

static inline int kbd_getkeycode(unsigned int sc)
{
	int ret = -EINVAL;

	if (k_getkeycode)
		ret = k_getkeycode(sc);

	return ret;
}

static inline void kbd_leds(unsigned char leds)
{
	if (k_leds)
		k_leds(leds);
}

extern int k_sysrq_key;
extern unsigned char *k_sysrq_xlate;

#define SYSRQ_KEY		k_sysrq_key
#define kbd_sysrq_xlate		k_sysrq_xlate
#define kbd_translate		k_translate
#define kbd_unexpected_up	k_unexpected_up

#include <asm/arch/keyboard.h>

#endif /* __KERNEL__ */

#endif /* __ASM_ARM_KEYBOARD_H */
