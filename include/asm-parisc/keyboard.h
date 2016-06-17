/*
 *  WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *  ---------------------------------------------------------------
 *  This file will be removed as soon as we have converted
 *  hp_psaux.c and hp_keyb.c to the input layer !
 *  
 */


/*
 *  linux/include/asm-parisc/keyboard.h
 *
 *  Original by Geert Uytterhoeven
 *  updates by Alex deVries <adevries@thepuffingroup.com>
 *  portions copyright (1999) The Puffin Group
 *  mostly rewritten by Philipp Rumpf <prumpf@tux.org>,
 *   Copyright 2000 Philipp Rumpf
 */

/*
 *  We try to keep the amount of generic code as low as possible -
 *  we want to support all HIL, PS/2, and untranslated USB keyboards
 */

#ifndef _PARISC_KEYBOARD_H
#define _PARISC_KEYBOARD_H

#include <linux/config.h>

#ifdef __KERNEL__
#ifdef CONFIG_VT

#include <linux/kernel.h>
#include <linux/kd.h>

/*  These are basically the generic functions / variables.  The only
 *  unexpected detail is the initialization sequence for the keyboard
 *  driver is something like this:
 *
 *  detect keyboard port
 *  detect keyboard
 *  call register_kbd_ops 
 *  wait for init_hw
 *
 *  only after init_hw has been called you're allowed to call
 *  handle_scancode.  This means you either have to be extremely
 *  careful or use a global flag or something - I strongly suggest
 *  the latter.    prumpf */

extern struct kbd_ops {
	int (*setkeycode)(unsigned int, unsigned int);
	int (*getkeycode)(unsigned int);
	int (*translate)(unsigned char, unsigned char *, char);
	char (*unexpected_up)(unsigned char);
	void (*leds)(unsigned char);
	void (*init_hw)(void);

	unsigned char sysrq_key;
	unsigned char *sysrq_xlate;
} *kbd_ops;

#define kbd_setkeycode		(*kbd_ops->setkeycode)
#define kbd_getkeycode		(*kbd_ops->getkeycode)
#define kbd_translate		(*kbd_ops->translate)
#define kbd_unexpected_up	(*kbd_ops->unexpected_up)
#define kbd_leds		(*kbd_ops->leds)
#define kbd_init_hw		(*kbd_ops->init_hw)

#define SYSRQ_KEY		(kbd_ops->sysrq_key)
#define	kbd_sysrq_xlate		(kbd_ops->sysrq_xlate)
extern unsigned char hp_ps2kbd_sysrq_xlate[128]; 	/* from drivers/char/hp_keyb.c */

extern void unregister_kbd_ops(void);
extern void register_kbd_ops(struct kbd_ops *ops);

#endif /* CONFIG_VT */

#endif /* __KERNEL__ */

#endif /* __ASMPARISC_KEYBOARD_H */
