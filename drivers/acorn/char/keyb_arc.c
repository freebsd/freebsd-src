/*
 *  linux/drivers/acorn/char/keyb_arc.c
 *
 *  Copyright (C) 2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Acorn keyboard driver for ARM Linux.
 *
 *  The Acorn keyboard appears to have a ***very*** buggy reset protocol -
 *  every reset behaves differently.  We try to get round this by attempting
 *  a few things...
 */
#include <linux/config.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/ptrace.h>
#include <linux/signal.h>
#include <linux/timer.h>
#include <linux/random.h>
#include <linux/ctype.h>
#include <linux/init.h>
#include <linux/kbd_ll.h>
#include <linux/kbd_kern.h>
#include <linux/delay.h>

#include <asm/bitops.h>
#include <asm/keyboard.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/hardware/ioc.h>

#include "../../char/busmouse.h"

extern struct tasklet_struct keyboard_tasklet;
extern void kbd_reset_kdown(void);

#define VERSION 108

#define KBD_REPORT_ERR
#define KBD_REPORT_UNKN

#include <asm/io.h>
#include <asm/system.h>

static char kbd_txval[4];
static unsigned char kbd_txhead, kbd_txtail;
#define KBD_INCTXPTR(ptr) ((ptr) = ((ptr) + 1) & 3)
static int kbd_id = -1;
static DECLARE_WAIT_QUEUE_HEAD(kbd_waitq);
#ifdef CONFIG_KBDMOUSE
static int mousedev;
#endif

/*
 * Protocol codes to send the keyboard.
 */
#define HRST 0xff	/* reset keyboard */
#define RAK1 0xfe	/* reset response */
#define RAK2 0xfd	/* reset response */
#define BACK 0x3f	/* Ack for first keyboard pair */
#define SMAK 0x33	/* Last data byte ack (key scanning + mouse movement scanning) */
#define MACK 0x32	/* Last data byte ack (mouse movement scanning) */
#define SACK 0x31	/* Last data byte ack (key scanning) */
#define NACK 0x30	/* Last data byte ack (no scanning, mouse data) */
#define RQMP 0x22	/* Request mouse data */
#define PRST 0x21	/* nothing */
#define RQID 0x20	/* Request ID */

#define UP_FLAG 1

#ifdef CONFIG_MAGIC_SYSRQ
unsigned char a5kkbd_sysrq_xlate[] = 
{
    27,    0,    0,    0,    0,    0,    0,    0,
     0,    0,    0,    0,    0,    0,    0,    0,
   '`',  '1',  '2',  '3',  '4',  '5',  '6',  '7',
   '8',  '9',  '0',  '-',  '=',  '£',  127,    0,
     0,    0,    0,  '/',  '*',  '#',    9,  'q',
   'w',  'e',  'r',  't',  'y',  'u',  'i',  'o',
   'p',  '[',  ']', '\\',  22,    23,   25,  '7',
   '8',  '9',  '-',    0,  'a',  's',  'd',  'f',
   'g',  'h',  'j',  'k',  'l',  ';', '\'',   13,
   '4',  '5',  '6',  '+',    0,    0,  'z',  'x',
   'c',  'v',  'b',  'n',  'm',  ',',  '.',  '/',
     0,    0,  '1',  '2',  '3',    0,    0,  ' ',
     0,    0,    0,    0,    0,  '0',  '.',   10,
     0,    0,    0,    0,    0,    0,    0,    0,
     0,    0,    0,    0,    0,    0,    0,    0,
     0,    0,    0,    0,    0,    0,    0,    0,
};
#endif

/*
 * This array converts the scancode that we get from the keyboard to the
 * real rows/columns on the A5000 keyboard.  This might be keyboard specific...
 *
 * It is these values that we use to maintain the key down array.  That way, we
 * should pick up on the ghost key presses (which is what happens when you press
 * three keys, and the keyboard thinks you have pressed four!)
 *
 * Row 8 (0x80+c) is actually a column with one key per row.  It is isolated from
 * the other keys, and can't cause these problems (its used for shift, ctrl, alt etc).
 *
 * Illegal scancodes are denoted by an 0xff (in other words, we don't know about
 * them, and can't process them for ghosts).  This does however, cause problems with
 * autorepeat processing...
 */
static unsigned char scancode_2_colrow[256] = {
  0x01, 0x42, 0x32, 0x33, 0x43, 0x56, 0x5a, 0x6c, 0x7c, 0x5c, 0x5b, 0x6b, 0x7b, 0x84, 0x70, 0x60,
  0x11, 0x51, 0x62, 0x63, 0x44, 0x54, 0x55, 0x45, 0x46, 0x4a, 0x3c, 0x4b, 0x59, 0x49, 0x69, 0x79,
  0x83, 0x40, 0x30, 0x3b, 0x39, 0x38, 0x31, 0x61, 0x72, 0x73, 0x64, 0x74, 0x75, 0x65, 0x66, 0x6a,
  0x1c, 0x2c, 0x7a, 0x36, 0x48, 0x68, 0x78, 0x20, 0x2b, 0x29, 0x28, 0x81, 0x71, 0x22, 0x23, 0x34,
  0x24, 0x25, 0x35, 0x26, 0x3a, 0x0c, 0x2a, 0x76, 0x10, 0x1b, 0x19, 0x18, 0x82, 0xff, 0x21, 0x12,
  0x13, 0x14, 0x04, 0x05, 0x15, 0x16, 0x1a, 0x0a, 0x85, 0x77, 0x00, 0x0b, 0x09, 0x02, 0x80, 0x03,
  0x87, 0x86, 0x06, 0x17, 0x27, 0x07, 0x37, 0x08, 0xff,
};

#define BITS_PER_SHORT (8*sizeof(unsigned short))
static unsigned short ghost_down[128/BITS_PER_SHORT];

static void a5kkbd_key(unsigned int keycode, unsigned int up_flag)
{
	unsigned int real_keycode;

	if (keycode > 0x72) {
#ifdef KBD_REPORT_UNKN
		printk ("kbd: unknown scancode 0x%04x\n", keycode);
#endif
		return;
	}
	if (keycode >= 0x70) {
#ifdef CONFIG_KBDMOUSE
		if (mousedev >= 0)
			switch (keycode) {
			case 0x70: /* Left mouse button */
				busmouse_add_buttons(mousedev, 4, up_flag ? 4 : 0);
				break;

			case 0x71: /* Middle mouse button */
				busmouse_add_buttons(mousedev, 2, up_flag ? 2 : 0);
				break;

			case 0x72:/* Right mouse button */
				busmouse_add_buttons(mousedev, 1, up_flag ? 1 : 0);
				break;
			}
#endif
		return;
	}

	/*
	 * We have to work out if we accept this key press as a real key, or
	 * if it is a ghost.  IE. If you press three keys, the keyboard will think
	 * that you've pressed a fourth: (@ = key down, # = ghost)
	 *
	 *   0 1 2 3 4 5 6 7
	 *   | | | | | | | |
	 * 0-+-+-+-+-+-+-+-+-
	 *   | | | | | | | |
	 * 1-+-@-+-+-+-@-+-+-
	 *   | | | | | | | |
	 * 2-+-+-+-+-+-+-+-+-
	 *   | | | | | | | |
	 * 3-+-@-+-+-+-#-+-+-
	 *   | | | | | | | |
	 *
	 * This is what happens when you have a matrix keyboard...
	 */

	real_keycode = scancode_2_colrow[keycode];

	if ((real_keycode & 0x80) == 0) {
		int rr, kc = (real_keycode >> 4) & 7;
		int cc;
		unsigned short res, kdownkc;

		kdownkc = ghost_down[kc] | (1 << (real_keycode & 15));

		for (rr = 0; rr < 128/BITS_PER_SHORT; rr++)
			if (rr != kc && (res = ghost_down[rr] & kdownkc)) {
			    	/*
				 * we have found a second row with at least one key pressed in the
			    	 * same column.
			    	 */
			    	for (cc = 0; res; res >>= 1)
					cc += (res & 1);
				if (cc > 1)
					return; /* ignore it */
			}
		if (up_flag)
			clear_bit (real_keycode, ghost_down);
		else
			set_bit (real_keycode, ghost_down);
	}

	handle_scancode(keycode, !up_flag);
}

static inline void a5kkbd_sendbyte(unsigned char val)
{
	kbd_txval[kbd_txhead] = val;
	KBD_INCTXPTR(kbd_txhead);
	enable_irq(IRQ_KEYBOARDTX);
}

static inline void a5kkbd_reset(void)
{
	int i;

	for (i = 0; i < NR_SCANCODES/BITS_PER_SHORT; i++)
		ghost_down[i] = 0;

	kbd_reset_kdown();
}

void a5kkbd_leds(unsigned char leds)
{
	leds =  ((leds & (1<<VC_SCROLLOCK))?4:0) | ((leds & (1<<VC_NUMLOCK))?2:0) |
		((leds & (1<<VC_CAPSLOCK))?1:0);
	a5kkbd_sendbyte(leds);
}

/* Keyboard states:
 *  0 initial reset condition, receive HRST, send RRAK1
 *  1 Sent RAK1, wait for RAK1, send RRAK2
 *  2 Sent RAK2, wait for RAK2, send SMAK or RQID
 *  3 Sent RQID, expect KBID, send SMAK
 *  4 Sent SMAK, wait for anything
 *  5 Wait for second keyboard nibble for key pressed
 *  6 Wait for second keyboard nibble for key released
 *  7 Wait for second part of mouse data
 *
 * This function returns 1 when we successfully enter the IDLE state
 * (and hence need to do some keyboard processing).
 */
#define KBD_INITRST	0
#define KBD_RAK1	1
#define KBD_RAK2	2
#define KBD_ID		3
#define KBD_IDLE	4
#define KBD_KEYDOWN	5
#define KBD_KEYUP	6
#define KBD_MOUSE	7

static int handle_rawcode(unsigned int keyval)
{
	static signed char kbd_mousedx = 0;
	       signed char kbd_mousedy;
	static unsigned char kbd_state = KBD_INITRST;
	static unsigned char kbd_keyhigh = 0;

	if (keyval == HRST && kbd_state != KBD_INITRST && kbd_state != KBD_ID) {
		a5kkbd_sendbyte (HRST);
		a5kkbd_reset ();
		kbd_state = KBD_INITRST;
	} else switch(kbd_state) {
	case KBD_INITRST:			/* hard reset - sent HRST */
		if (keyval == HRST) {
			a5kkbd_sendbyte (RAK1);
			kbd_state = KBD_RAK1;
		} else if (keyval == RAK1) {
			/* Some A5000 keyboards are very fussy and don't follow Acorn's
			 * specs - this appears to fix them, but them it might stop
			 * them from being initialised.
			 *  fix by Philip Blundell
			 */
			printk(KERN_DEBUG "keyboard sent early RAK1 -- ignored\n");
		} else
			goto kbd_wontreset;
		break;

	case KBD_RAK1:				/* sent RAK1 - expect RAK1 and send RAK2 */
		if (keyval == RAK1) {
			a5kkbd_sendbyte (RAK2);
			kbd_state = KBD_RAK2;
		} else
			goto kbd_wontreset;
		break;

	case KBD_RAK2:				/* Sent RAK2 - expect RAK2 and send either RQID or SMAK */
		if (keyval == RAK2) {
			if (kbd_id == -1) {
				a5kkbd_sendbyte (NACK);
				a5kkbd_sendbyte (RQID);
				kbd_state = KBD_ID;
			} else {
				a5kkbd_sendbyte (SMAK);
				kbd_state = KBD_IDLE;
			}
		} else
			goto kbd_wontreset;
		break;

	case KBD_ID:				/* Sent RQID - expect KBID */
		if (keyval == HRST) {
			kbd_id = -2;
			a5kkbd_reset ();
			a5kkbd_sendbyte (HRST);
			kbd_state = KBD_INITRST;
			wake_up (&kbd_waitq);
		} else if ((keyval & 0xc0) == 0x80) {
			kbd_id = keyval & 0x3f;
			a5kkbd_sendbyte (SMAK);
			kbd_state = KBD_IDLE;
			wake_up (&kbd_waitq);
		}
		break;

	case KBD_IDLE:				/* Send SMAK, ready for any reply */
		switch (keyval & 0xf0) {
		default:	/* 0x00 - 0x7f */
			kbd_mousedx = keyval & 0x40 ? keyval|0x80 : keyval;
			kbd_state   = KBD_MOUSE;
			a5kkbd_sendbyte (BACK);
			break;

		case 0x80:
		case 0x90:
		case 0xa0:
		case 0xb0:
		    	if (kbd_id == -1)
				kbd_id = keyval & 0x3f;
			break;

		case 0xc0:
			kbd_keyhigh = keyval;
			kbd_state   = KBD_KEYDOWN;
			a5kkbd_sendbyte (BACK);
			break;

		case 0xd0:
			kbd_keyhigh = keyval;
			kbd_state   = KBD_KEYUP;
			a5kkbd_sendbyte (BACK);
			break;

		case 0xe0:
		case 0xf0:
			goto kbd_error;
		}
		break;

	case KBD_KEYDOWN:
		if ((keyval & 0xf0) != 0xc0)
			goto kbd_error;
		else {
			kbd_state = KBD_IDLE;
			a5kkbd_sendbyte (SMAK);
			if (((kbd_keyhigh ^ keyval) & 0xf0) == 0)
				a5kkbd_key ((keyval & 0x0f) | ((kbd_keyhigh << 4) & 0xf0), 0);
		}
		break;

	case KBD_KEYUP:
		if ((keyval & 0xf0) != 0xd0)
			goto kbd_error;
		else {
			kbd_state = KBD_IDLE;
			a5kkbd_sendbyte (SMAK);
			if (((kbd_keyhigh ^ keyval) & 0xf0) == 0)
				a5kkbd_key ((keyval & 0x0f) | ((kbd_keyhigh << 4) & 0xf0), UP_FLAG);
		}
		break;

	case KBD_MOUSE:
		if (keyval & 0x80)
			goto kbd_error;
		else {
			kbd_state = KBD_IDLE;
			a5kkbd_sendbyte (SMAK);
			kbd_mousedy = (char)(keyval & 0x40 ? keyval | 0x80 : keyval);
#ifdef CONFIG_KBDMOUSE
			if (mousedev >= 0)
				busmouse_add_movement(mousedev, (int)kbd_mousedx, (int)kbd_mousedy);
#endif
		}
	}
	return kbd_state == KBD_IDLE ? 1 : 0;

kbd_wontreset:
#ifdef KBD_REPORT_ERR
	printk ("kbd: keyboard won't reset (kbdstate %d, keyval %02X)\n",
		kbd_state, keyval);
#endif
	mdelay(1);
	ioc_readb(IOC_KARTRX);
	a5kkbd_sendbyte (HRST);
	kbd_state = KBD_INITRST;
	return 0;

kbd_error:
#ifdef KBD_REPORT_ERR
	printk ("kbd: keyboard out of sync - resetting\n");
#endif
	a5kkbd_sendbyte (HRST);
	kbd_state = KBD_INITRST;
	return 0;
}

static void a5kkbd_rx(int irq, void *dev_id, struct pt_regs *regs)
{
	kbd_pt_regs = regs;
	if (handle_rawcode(ioc_readb(IOC_KARTRX)))
		tasklet_schedule(&keyboard_tasklet);
}

static void a5kkbd_tx(int irq, void *dev_id, struct pt_regs *regs)
{
	ioc_writeb (kbd_txval[kbd_txtail], IOC_KARTTX);
	KBD_INCTXPTR(kbd_txtail);
	if (kbd_txtail == kbd_txhead)
		disable_irq(irq);
}

static int a5kkbd_setkeycode(unsigned int scancode, unsigned int keycode)
{
	return -EINVAL;
}

static int a5kkbd_getkeycode(unsigned int scancode)
{
	return -EINVAL;
}

static int a5kkbd_translate(unsigned char scancode, unsigned char *keycode, char rawmode)
{
	*keycode = scancode;
	return 1;
}

static char a5kkbd_unexpected_up(unsigned char keycode)
{
	return 0200;
}

static int a5kkbd_rate(struct kbd_repeat *rep)
{
	return -EINVAL;
}

#ifdef CONFIG_KBDMOUSE
static struct busmouse a5kkbd_mouse = {
	6, "kbdmouse", NULL, NULL, NULL, 7
};
#endif

struct kbd_ops_struct a5k_kbd_ops = {
	k_setkeycode:		a5kkbd_setkeycode,
	k_getkeycode:		a5kkbd_getkeycode,
	k_translate:		a5kkbd_translate,
	k_unexpected_up:	a5kkbd_unexpected_up,
	k_leds:			a5kkbd_leds,
	k_rate:			a5kkbd_rate,
#ifdef CONFIG_MAGIC_SYSRQ
	k_sysrq_xlate:		a5kkbd_sysrq_xlate,
	k_sysrq_key:		13,
#endif
};

void __init a5kkbd_init_hw (void)
{
	if (request_irq (IRQ_KEYBOARDTX, a5kkbd_tx, 0, "keyboard", NULL) != 0)
		panic("Could not allocate keyboard transmit IRQ!");
	(void)ioc_readb(IOC_KARTRX);
	if (request_irq (IRQ_KEYBOARDRX, a5kkbd_rx, 0, "keyboard", NULL) != 0)
		panic("Could not allocate keyboard receive IRQ!");

	a5kkbd_sendbyte (HRST);	/* send HRST (expect HRST) */

	/* wait 1s for keyboard to initialise */
	interruptible_sleep_on_timeout(&kbd_waitq, HZ);

#ifdef CONFIG_KBDMOUSE
	mousedev = register_busmouse(&a5kkbd_mouse);
	if (mousedev < 0)
		printk(KERN_ERR "Unable to register mouse driver\n");
#endif

	printk (KERN_INFO "Keyboard driver v%d.%02d. (", VERSION/100, VERSION%100);
	if (kbd_id != -1)
	      printk ("id=%d ", kbd_id);
	printk ("English)\n");
}
