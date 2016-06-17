/*
 *  linux/drivers/acorn/char/keyb_ps2.c
 *
 *  Copyright (C) 2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Keyboard driver for RiscPC ARM Linux.
 *
 *  Note!!! This driver talks directly to the keyboard.
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
#include <linux/kbd_ll.h>
#include <linux/delay.h>
#include <linux/init.h>

#include <asm/bitops.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/keyboard.h>
#include <asm/io.h>
#include <asm/hardware/iomd.h>
#include <asm/system.h>

extern struct tasklet_struct keyboard_tasklet;
extern void kbd_reset_kdown(void);
int kbd_read_mask;

#define TX_DONE 0
#define TX_SENT 1
#define TX_SEND 2

static volatile int tx_state;

#define VERSION 100

#define KBD_REPORT_ERR
#define KBD_REPORT_UNKN

#define KBD_ESCAPEE0	0xe0		/* in */
#define KBD_ESCAPEE1	0xe1		/* in */

#define ESCE0(x)	(0xe000|(x))
#define ESCE1(x)	(0xe100|(x))

#define KBD_BAT		0xaa		/* in */
#define KBD_SETLEDS	0xed		/* out */
#define KBD_ECHO	0xee		/* in/out */
#define KBD_BREAK	0xf0		/* in */
#define KBD_TYPRATEDLY	0xf3		/* out */
#define KBD_SCANENABLE	0xf4		/* out */
#define KBD_DEFDISABLE	0xf5		/* out */
#define KBD_DEFAULT	0xf6		/* out */
#define KBD_ACK		0xfa		/* in */
#define KBD_DIAGFAIL	0xfd		/* in */
#define KBD_RESEND	0xfe		/* in/out */
#define KBD_RESET	0xff		/* out */

#define CODE_BREAK	1
#define CODE_ESCAPEE0	2
#define CODE_ESCAPEE1	4
#define CODE_ESCAPE12	8

#define K_NONE		0x7f
#define K_ESC		0x00
#define K_F1		0x01
#define K_F2		0x02
#define K_F3		0x03
#define K_F4		0x04
#define K_F5		0x05
#define K_F6		0x06
#define K_F7		0x07
#define K_F8		0x08
#define K_F9		0x09
#define K_F10		0x0a
#define K_F11		0x0b
#define K_F12		0x0c
#define K_PRNT		0x0d
#define K_SCRL		0x0e
#define K_BRK		0x0f
#define K_AGR		0x10
#define K_1		0x11
#define K_2		0x12
#define K_3		0x13
#define K_4		0x14
#define K_5		0x15
#define K_6		0x16
#define K_7		0x17
#define K_8		0x18
#define K_9		0x19
#define K_0		0x1a
#define K_MINS		0x1b
#define K_EQLS		0x1c
#define K_BKSP		0x1e
#define K_INS		0x1f
#define K_HOME		0x20
#define K_PGUP		0x21
#define K_NUML		0x22
#define KP_SLH		0x23
#define KP_STR		0x24
#define KP_MNS		0x3a
#define K_TAB		0x26
#define K_Q		0x27
#define K_W		0x28
#define K_E		0x29
#define K_R		0x2a
#define K_T		0x2b
#define K_Y		0x2c
#define K_U		0x2d
#define K_I		0x2e
#define K_O		0x2f
#define K_P		0x30
#define K_LSBK		0x31
#define K_RSBK		0x32
#define K_ENTR		0x47
#define K_DEL		0x34
#define K_END		0x35
#define K_PGDN		0x36
#define KP_7		0x37
#define KP_8		0x38
#define KP_9		0x39
#define KP_PLS		0x4b
#define K_CAPS		0x5d
#define K_A		0x3c
#define K_S		0x3d
#define K_D		0x3e
#define K_F		0x3f
#define K_G		0x40
#define K_H		0x41
#define K_J		0x42
#define K_K		0x43
#define K_L		0x44
#define K_SEMI		0x45
#define K_SQOT		0x46
#define K_HASH		0x1d
#define KP_4		0x48
#define KP_5		0x49
#define KP_6		0x4a
#define K_LSFT		0x4c
#define K_BSLH		0x33
#define K_Z		0x4e
#define K_X		0x4f
#define K_C		0x50
#define K_V		0x51
#define K_B		0x52
#define K_N		0x53
#define K_M		0x54
#define K_COMA		0x55
#define K_DOT		0x56
#define K_FSLH		0x57
#define K_RSFT		0x58
#define K_UP		0x59
#define KP_1		0x5a
#define KP_2		0x5b
#define KP_3		0x5c
#define KP_ENT		0x67
#define K_LCTL		0x3b
#define K_LALT		0x5e
#define K_SPCE		0x5f
#define K_RALT		0x60
#define K_RCTL		0x61
#define K_LEFT		0x62
#define K_DOWN		0x63
#define K_RGHT		0x64
#define KP_0		0x65
#define KP_DOT		0x66

static unsigned char keycode_translate[256] =
{
/* 00 */  K_NONE, K_F9  , K_NONE, K_F5  , K_F3  , K_F1  , K_F2  , K_F12 ,
/* 08 */  K_NONE, K_F10 , K_F8  , K_F6  , K_F4  , K_TAB , K_AGR , K_NONE,
/* 10 */  K_NONE, K_LALT, K_LSFT, K_NONE, K_LCTL, K_Q   , K_1   , K_NONE,
/* 18 */  K_NONE, K_NONE, K_Z   , K_S   , K_A   , K_W   , K_2   , K_NONE,
/* 20 */  K_NONE, K_C   , K_X   , K_D   , K_E   , K_4   , K_3   , K_NONE,
/* 28 */  K_NONE, K_SPCE, K_V   , K_F   , K_T   , K_R   , K_5   , K_NONE,
/* 30 */  K_NONE, K_N   , K_B   , K_H   , K_G   , K_Y   , K_6   , K_NONE,
/* 38 */  K_NONE, K_NONE, K_M   , K_J   , K_U   , K_7   , K_8   , K_NONE,
/* 40 */  K_NONE, K_COMA, K_K   , K_I   , K_O   , K_0   , K_9   , K_NONE,
/* 48 */  K_NONE, K_DOT , K_FSLH, K_L   , K_SEMI, K_P   , K_MINS, K_NONE,
/* 50 */  K_NONE, K_NONE, K_SQOT, K_NONE, K_LSBK, K_EQLS, K_NONE, K_NONE,
/* 58 */  K_CAPS, K_RSFT, K_ENTR, K_RSBK, K_NONE, K_HASH, K_NONE, K_NONE,
/* 60 */  K_NONE, K_BSLH, K_NONE, K_NONE, K_NONE, K_NONE, K_BKSP, K_NONE,
/* 68 */  K_NONE, KP_1  , K_NONE, KP_4  , KP_7  , K_NONE, K_NONE, K_NONE,
/* 70 */  KP_0  , KP_DOT, KP_2  , KP_5  , KP_6  , KP_8  , K_ESC , K_NUML,
/* 78 */  K_F11 , KP_PLS, KP_3  , KP_MNS, KP_STR, KP_9  , K_SCRL, K_NONE,
	  K_NONE, K_NONE, K_NONE, K_F7  , K_NONE, K_NONE, K_NONE, K_NONE,
	  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
	  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
	  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
	  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
	  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
	  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
	  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
	  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
	  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
	  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
	  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
	  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
	  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE
};

#ifdef CONFIG_MAGIC_SYSRQ
static unsigned char ps2kbd_sysrq_xlate[] = 
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

static inline void ps2kbd_key(unsigned int keycode, unsigned int up_flag)
{
	handle_scancode(keycode, !up_flag);
}

static inline void ps2kbd_sendbyte(unsigned char val)
{
	int tries = 3, timeout = 1000;

	tx_state = TX_SEND;

	do {
		switch (tx_state) {
		case TX_SEND:
			tx_state = TX_SENT;
			timeout = 1000;
			tries --;

			while(!(iomd_readb(IOMD_KCTRL) & (1 << 7)));
			iomd_writeb(val, IOMD_KARTTX);
			break;

		case TX_SENT:
			udelay(1000);
			if (--timeout == 0) {
				printk(KERN_ERR "Keyboard timeout\n");
				tx_state = TX_DONE;
			}
			break;

		case TX_DONE:
			break;
		}
	} while (tries > 0 && tx_state != TX_DONE);
}

static unsigned char status;
static unsigned char ncodes;
static unsigned char bi;
static unsigned char buffer[4];

static inline void ps2kbd_reset(void)
{
	status = 0;
	kbd_reset_kdown();
}

static void handle_rawcode(int keyval)
{
	int keysym;

	if (keyval > 0x83) {
		switch (keyval) {
		case KBD_ESCAPEE0:
			ncodes = 2;
			bi = 0;
			break;

		case KBD_ESCAPEE1:
			ncodes = 3;
			bi = 0;
			break;

		case KBD_ACK:
			tx_state = TX_DONE;
			return;

		case KBD_RESEND:
			tx_state = TX_SEND;
			return;

		case KBD_BREAK:
			status |= CODE_BREAK;
			return;

		default:
			return;
		}
	}

	if (ncodes) {
		buffer[bi++] = keyval;
		ncodes -= 1;
		if (ncodes)
			return;
		keysym = K_NONE;
		switch (buffer[0] << 8 | buffer[1]) {
		case ESCE0(0x11): keysym = K_RALT; break;
		case ESCE0(0x14): keysym = K_RCTL; break;
		/*
		 * take care of MS extra keys (actually
		 * 0x7d - 0x7f, but last one is already K_NONE
		 */
		case ESCE0(0x1f): keysym = 124;    break;
		case ESCE0(0x27): keysym = 125;    break;
		case ESCE0(0x2f): keysym = 126;    break;
		case ESCE0(0x4a): keysym = KP_SLH; break;
		case ESCE0(0x5a): keysym = KP_ENT; break;
		case ESCE0(0x69): keysym = K_END;  break;
		case ESCE0(0x6b): keysym = K_LEFT; break;
		case ESCE0(0x6c): keysym = K_HOME; break;
		case ESCE0(0x70): keysym = K_INS;  break;
		case ESCE0(0x71): keysym = K_DEL;  break;
		case ESCE0(0x72): keysym = K_DOWN; break;
		case ESCE0(0x74): keysym = K_RGHT; break;
		case ESCE0(0x75): keysym = K_UP;   break;
		case ESCE0(0x7a): keysym = K_PGDN; break;
		case ESCE0(0x7c): keysym = K_PRNT; break;
		case ESCE0(0x7d): keysym = K_PGUP; break;
		case ESCE1(0x14):
			if (buffer[2] == 0x77)
				keysym = K_BRK;
			break;
		case ESCE0(0x12):		/* ignore escaped shift key */
			status = 0;
			return;
		}
	} else {
		bi = 0;
		keysym = keycode_translate[keyval];
	}

	if (keysym != K_NONE)
		ps2kbd_key(keysym, status & CODE_BREAK);
	status = 0;
}

static void ps2kbd_leds(unsigned char leds)
{
	ps2kbd_sendbyte(KBD_SETLEDS);
	ps2kbd_sendbyte(leds);
	ps2kbd_sendbyte(KBD_SCANENABLE);
}

static void ps2kbd_rx(int irq, void *dev_id, struct pt_regs *regs)
{
	kbd_pt_regs = regs;

	while (iomd_readb(IOMD_KCTRL) & (1 << 5))
		handle_rawcode(iomd_readb(IOMD_KARTRX));
	tasklet_schedule(&keyboard_tasklet);
}

static void ps2kbd_tx(int irq, void *dev_id, struct pt_regs *regs)
{
}

static int ps2kbd_translate(unsigned char scancode, unsigned char *keycode, char rawmode)
{
	*keycode = scancode;
	return 1;
}

static char ps2kbd_unexpected_up(unsigned char scancode)
{
	return 0200;
}

int __init ps2kbd_init_hw(void)
{
	/* Reset the keyboard state machine. */
	iomd_writeb(0, IOMD_KCTRL);
	iomd_writeb(8, IOMD_KCTRL);
	iomd_readb(IOMD_KARTRX);

	if (request_irq (IRQ_KEYBOARDRX, ps2kbd_rx, 0, "keyboard", NULL) != 0)
		panic("Could not allocate keyboard receive IRQ!");
	if (request_irq (IRQ_KEYBOARDTX, ps2kbd_tx, 0, "keyboard", NULL) != 0)
		panic("Could not allocate keyboard transmit IRQ!");

	k_translate	= ps2kbd_translate;
	k_unexpected_up	= ps2kbd_unexpected_up;
	k_leds		= ps2kbd_leds;
#ifdef CONFIG_MAGIC_SYSRQ
	k_sysrq_xlate	= ps2kbd_sysrq_xlate;
	k_sysrq_key	= 13;
#endif

	return 0;
}
