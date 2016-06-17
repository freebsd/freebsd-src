/*
 *  WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *  ---------------------------------------------------------------
 *  This source file will be removed as soon as we have converted
 *  hp_psaux.c and hp_keyb.c to the input layer !
 *  
 */

/*
 *  linux/arch/parisc/kernel/keyboard.c
 *
 *  Alex deVries <adevries@thepuffingroup.com>
 *  Copyright (1999) The Puffin Group
 *  Mostly rewritten by Philipp Rumpf <prumpf@tux.org>
 *  Copyright 2000 Philipp Rumpf
 */

#include <linux/errno.h>
#include <linux/keyboard.h>
#include <asm/keyboard.h>
#include <linux/module.h>

static int def_setkeycode(unsigned int x, unsigned int y)
{
	return 0;
}

static int def_getkeycode(unsigned int x)
{
	return 0;
}

static int def_translate(unsigned char scancode, unsigned char *keycode,
	char raw)
{
	*keycode = scancode;

	return 1;
}

static char def_unexpected_up(unsigned char c)
{
	return 128;
}

static void def_leds(unsigned char leds)
{
}

static void def_init_hw(void)
{
}

static char def_sysrq_xlate[NR_KEYS];

#define DEFAULT_KEYB_OPS \
	setkeycode:	def_setkeycode,	\
	getkeycode:	def_getkeycode, \
	translate:	def_translate, \
	unexpected_up:	def_unexpected_up, \
	leds:		def_leds, \
	init_hw:	def_init_hw, \
	sysrq_key:	0xff, \
	sysrq_xlate:	def_sysrq_xlate,

static struct kbd_ops def_kbd_ops = {
	DEFAULT_KEYB_OPS
};

struct kbd_ops *kbd_ops = &def_kbd_ops;

void unregister_kbd_ops(void)
{
	struct kbd_ops new_kbd_ops = { DEFAULT_KEYB_OPS };
	register_kbd_ops(&new_kbd_ops);
}
EXPORT_SYMBOL(unregister_kbd_ops);

void register_kbd_ops(struct kbd_ops *ops)
{
	if(ops->setkeycode)
		kbd_ops->setkeycode = ops->setkeycode;
	
	if(ops->getkeycode)
		kbd_ops->getkeycode = ops->getkeycode;
	
	if(ops->translate)
		kbd_ops->translate = ops->translate;
	
	if(ops->unexpected_up)
		kbd_ops->unexpected_up = ops->unexpected_up;
	
	if(ops->leds)
		kbd_ops->leds = ops->leds;
	
	if(ops->init_hw)
		kbd_ops->init_hw = ops->init_hw;
	
	kbd_ops->sysrq_key = ops->sysrq_key;
	kbd_ops->sysrq_xlate = ops->sysrq_xlate;
}
