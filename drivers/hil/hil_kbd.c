/*
 * Generic linux-input device driver for keyboard devices
 *
 * Copyright (c) 2001 Brian S. Julin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL").
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *
 * References:
 * HP-HIL Technical Reference Manual.  Hewlett Packard Product No. 45918A
 *
 */

#include <linux/hil.h>
#include <linux/input.h>
#include <linux/serio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>

#ifdef DEBUG	/* DEBUG */
#undef input_report_key
#define input_report_key(a,b,c) { printk("input_report_key(%p, %d, %d)\n", a, b, !!(c)); input_event(a, EV_KEY, b, !!(c)); }
#endif

#define PREFIX "HIL KEYB: "
#define HIL_GENERIC_NAME "generic HIL keyboard device"

MODULE_AUTHOR("Brian S. Julin <bri@calyx.com>");
MODULE_DESCRIPTION(HIL_GENERIC_NAME " driver");
MODULE_LICENSE("Dual BSD/GPL");

#define HIL_KBD_MAX_LENGTH 16

#define HIL_KBD_SET1_UPBIT 0x01
#define HIL_KBD_SET1_SHIFT 1

static uint8_t hil_kbd_set1[128] = {
   KEY_5,		KEY_RESERVED,	KEY_RIGHTALT,	KEY_LEFTALT, 
   KEY_RIGHTSHIFT,	KEY_LEFTSHIFT,	KEY_LEFTCTRL,	KEY_SYSRQ,
   KEY_KP4,		KEY_KP8,	KEY_KP5,	KEY_KP9,
   KEY_KP6,		KEY_KP7,	KEY_KPCOMMA,	KEY_KPENTER,
   KEY_KP1,		KEY_KPSLASH,	KEY_KP2,	KEY_KPPLUS,
   KEY_KP3,		KEY_KPASTERISK,	KEY_KP0,	KEY_KPMINUS,
   KEY_B,		KEY_V,		KEY_C,		KEY_X,
   KEY_Z,		KEY_UNKNOWN,	KEY_RESERVED,   KEY_ESC,
   KEY_6,		KEY_F10,	KEY_3,		KEY_F11,
   KEY_KPDOT,		KEY_F9,		KEY_TAB /*KP*/,	KEY_F12,
   KEY_H,		KEY_G,		KEY_F,		KEY_D,
   KEY_S,		KEY_A,		KEY_RESERVED,	KEY_CAPSLOCK,
   KEY_U,		KEY_Y,		KEY_T,		KEY_R,
   KEY_E,		KEY_W,		KEY_Q,		KEY_TAB,
   KEY_7,		KEY_6,		KEY_5,		KEY_4,
   KEY_3,		KEY_2,		KEY_1,		KEY_GRAVE,
   KEY_INTL1,		KEY_INTL2,	KEY_INTL3,	KEY_INTL4, /*Buttons*/
   KEY_INTL5,		KEY_INTL6,	KEY_INTL7,	KEY_INTL8,
   KEY_MENU,		KEY_F4,		KEY_F3,		KEY_F2,
   KEY_F1,		KEY_VOLUMEUP,	KEY_STOP,	KEY_SENDFILE/*Enter/Print*/, 
   KEY_SYSRQ,		KEY_F5,		KEY_F6,		KEY_F7,
   KEY_F8,		KEY_VOLUMEDOWN,	KEY_CUT /*CLEAR_LINE*/, KEY_REFRESH /*CLEAR_DISPLAY*/,
   KEY_8,		KEY_9,		KEY_0,		KEY_MINUS,
   KEY_EQUAL,		KEY_BACKSPACE,	KEY_INSERT/*KPINSERT_LINE*/, KEY_DELETE /*KPDELETE_LINE*/,
   KEY_I,		KEY_O,		KEY_P,		KEY_LEFTBRACE,
   KEY_RIGHTBRACE,	KEY_BACKSLASH,	KEY_INSERT,	KEY_DELETE,
   KEY_J,		KEY_K,		KEY_L,		KEY_SEMICOLON,
   KEY_APOSTROPHE,	KEY_ENTER,	KEY_HOME,	KEY_SCROLLUP,
   KEY_M,		KEY_COMMA,	KEY_DOT,	KEY_SLASH,
   KEY_RESERVED,	KEY_OPEN/*Select*/,KEY_RESERVED,KEY_SCROLLDOWN/*KPNEXT*/,
   KEY_N,		KEY_SPACE,	KEY_SCROLLDOWN/*Next*/, KEY_UNKNOWN,
   KEY_LEFT,		KEY_DOWN,	KEY_UP,		KEY_RIGHT
};

#define HIL_KBD_SET2_UPBIT 0x01
#define HIL_KBD_SET2_SHIFT 1

/* Set2 is user defined */

#define HIL_KBD_SET3_UPBIT 0x80
#define HIL_KBD_SET3_SHIFT 0

static uint8_t hil_kbd_set3[128] = {
  KEY_RESERVED,	KEY_ESC,	KEY_1,		KEY_2,
  KEY_3,	KEY_4,		KEY_5,		KEY_6,
  KEY_7,	KEY_8,		KEY_9,		KEY_0,
  KEY_MINUS,	KEY_EQUAL,	KEY_BACKSPACE,	KEY_TAB,
  KEY_Q,	KEY_W,		KEY_E,		KEY_R,
  KEY_T,	KEY_Y,		KEY_U,		KEY_I,
  KEY_O,	KEY_P,		KEY_LEFTBRACE,	KEY_RIGHTBRACE,
  KEY_ENTER,	KEY_LEFTCTRL,	KEY_A,		KEY_S,
  KEY_D,	KEY_F,		KEY_G,		KEY_H,
  KEY_J,	KEY_K,		KEY_L,		KEY_SEMICOLON,
  KEY_APOSTROPHE,KEY_GRAVE,	KEY_LEFTSHIFT,	KEY_BACKSLASH,
  KEY_Z,	KEY_X,		KEY_C,		KEY_V,
  KEY_B,	KEY_N,		KEY_M,		KEY_COMMA,
  KEY_DOT,	KEY_SLASH,	KEY_RIGHTSHIFT,	KEY_KPASTERISK,
  KEY_LEFTALT,	KEY_SPACE,	KEY_CAPSLOCK,	KEY_F1,
  KEY_F2,	KEY_F3,		KEY_F4,		KEY_F5,
  KEY_F6,	KEY_F7,		KEY_F8,		KEY_F9,
  KEY_F10,	KEY_NUMLOCK,	KEY_SCROLLLOCK,	KEY_KP7,
  KEY_KP8,	KEY_KP9,	KEY_KPMINUS,	KEY_KP4,
  KEY_KP5,	KEY_KP6,	KEY_KPPLUS,	KEY_KP1,
  KEY_KP2,	KEY_KP3,	KEY_KP0,	KEY_KPDOT,
  KEY_SYSRQ,	KEY_RESERVED,	KEY_RESERVED,	KEY_RESERVED,
  KEY_RESERVED,	KEY_RESERVED,	KEY_RESERVED,	KEY_RESERVED,
  KEY_RESERVED,	KEY_RESERVED,	KEY_UNKNOWN,	KEY_UNKNOWN,
  KEY_UP,	KEY_LEFT,	KEY_DOWN,	KEY_RIGHT,
  KEY_HOME,	KEY_PAGEUP,	KEY_END,	KEY_PAGEDOWN,
  KEY_INSERT,	KEY_DELETE,	KEY_102ND,	KEY_RESERVED,
  KEY_RESERVED,	KEY_RESERVED,	KEY_RESERVED,	KEY_RESERVED,
  KEY_F1,	KEY_F2,		KEY_F3,		KEY_F4,
  KEY_F5,	KEY_F6,		KEY_F7,		KEY_F8,
  KEY_RESERVED,	KEY_RESERVED,	KEY_RESERVED,	KEY_RESERVED,
  KEY_RESERVED,	KEY_RESERVED,	KEY_RESERVED,	KEY_RESERVED
};

static char *hil_language[] = { HIL_LOCALE_MAP };

struct hil_kbd {
	struct input_dev dev;
	struct serio *serio;

	/* Input buffer and index for packets from HIL bus. */
	hil_packet data[HIL_KBD_MAX_LENGTH];
	int idx4; /* four counts per packet */

	/* Raw device info records from HIL bus, see hil.h for fields. */
	char	idd[HIL_KBD_MAX_LENGTH];	/* DID byte and IDD record */
	char	rsc[HIL_KBD_MAX_LENGTH];	/* RSC record */
	char	exd[HIL_KBD_MAX_LENGTH];	/* EXD record */
	char	rnm[HIL_KBD_MAX_LENGTH + 1];	/* RNM record + NULL term. */

	/* Something to sleep around with. */
	struct semaphore sem;
};

/* Process a complete packet after transfer from the HIL */
static void hil_kbd_process_record(struct hil_kbd *kbd)
{
	struct input_dev *dev = &kbd->dev;
	hil_packet *data = kbd->data;
	hil_packet p;
	int idx, i, cnt;

	idx = kbd->idx4/4;
	p = data[idx - 1];

	if ((p & ~HIL_CMDCT_POL) == 
	    (HIL_ERR_INT | HIL_PKT_CMD | HIL_CMD_POL)) goto report;
	if ((p & ~HIL_CMDCT_RPL) == 
	    (HIL_ERR_INT | HIL_PKT_CMD | HIL_CMD_RPL)) goto report;

	/* Not a poll response.  See if we are loading config records. */
	switch (p & HIL_PKT_DATA_MASK) {
	case HIL_CMD_IDD:
		for (i = 0; i < idx; i++)
			kbd->idd[i] = kbd->data[i] & HIL_PKT_DATA_MASK;
		for (; i < HIL_KBD_MAX_LENGTH; i++)
			kbd->idd[i] = 0;
		break;
	case HIL_CMD_RSC:
		for (i = 0; i < idx; i++)
			kbd->rsc[i] = kbd->data[i] & HIL_PKT_DATA_MASK;
		for (; i < HIL_KBD_MAX_LENGTH; i++)
			kbd->rsc[i] = 0;
		break;
	case HIL_CMD_EXD:
		for (i = 0; i < idx; i++)
			kbd->exd[i] = kbd->data[i] & HIL_PKT_DATA_MASK;
		for (; i < HIL_KBD_MAX_LENGTH; i++)
			kbd->exd[i] = 0;
		break;
	case HIL_CMD_RNM:
		for (i = 0; i < idx; i++)
			kbd->rnm[i] = kbd->data[i] & HIL_PKT_DATA_MASK;
		for (; i < HIL_KBD_MAX_LENGTH + 1; i++)
			kbd->rnm[i] = '\0';
		break;
	default:
		/* These occur when device isn't present */
		if (p == (HIL_ERR_INT | HIL_PKT_CMD)) break; 
		/* Anything else we'd like to know about. */
		printk(KERN_WARNING PREFIX "Device sent unknown record %x\n", p);
		break;
	}
	goto out;

 report:
	cnt = 1;
	switch (kbd->data[0] & HIL_POL_CHARTYPE_MASK) {
	case HIL_POL_CHARTYPE_NONE:
		break;
	case HIL_POL_CHARTYPE_ASCII:
		while (cnt < idx - 1)
			input_report_key(dev, kbd->data[cnt++] & 0x7f, 1);
		break;
	case HIL_POL_CHARTYPE_RSVD1:
	case HIL_POL_CHARTYPE_RSVD2:
	case HIL_POL_CHARTYPE_BINARY:
		while (cnt < idx - 1)
			input_report_key(dev, kbd->data[cnt++], 1);
		break;
	case HIL_POL_CHARTYPE_SET1:
		while (cnt < idx - 1) {
			unsigned int key;
			int up;
			key = kbd->data[cnt++];
			up = key & HIL_KBD_SET1_UPBIT;
			key &= (~HIL_KBD_SET1_UPBIT & 0xff);
			key = key >> HIL_KBD_SET1_SHIFT;
			if (key != KEY_RESERVED && key != KEY_UNKNOWN)
				input_report_key(dev, hil_kbd_set1[key], !up);
		}
		break;
	case HIL_POL_CHARTYPE_SET2:
		while (cnt < idx - 1) {
			unsigned int key;
			int up;
			key = kbd->data[cnt++];
			up = key & HIL_KBD_SET2_UPBIT;
			key &= (~HIL_KBD_SET1_UPBIT & 0xff);
			key = key >> HIL_KBD_SET2_SHIFT;
			if (key != KEY_RESERVED && key != KEY_UNKNOWN)
				input_report_key(dev, key, !up);
		}
		break;
	case HIL_POL_CHARTYPE_SET3:
		while (cnt < idx - 1) {
			unsigned int key;
			int up;
			key = kbd->data[cnt++];
			up = key & HIL_KBD_SET3_UPBIT;
			key &= (~HIL_KBD_SET1_UPBIT & 0xff);
			key = key >> HIL_KBD_SET3_SHIFT;
			if (key != KEY_RESERVED && key != KEY_UNKNOWN)
				input_report_key(dev, hil_kbd_set3[key], !up);
		}
		break;
	}
 out:
	kbd->idx4 = 0;
	up(&kbd->sem);
}

static void hil_kbd_process_err(struct hil_kbd *kbd) {
	printk(KERN_WARNING PREFIX "errored HIL packet\n");
	kbd->idx4 = 0;
	up(&kbd->sem);
	return;
}

static void hil_kbd_interrupt(struct serio *serio, 
			      unsigned char data, 
			      unsigned int flags)
{
	struct hil_kbd *kbd;
	hil_packet packet;
	int idx;

	kbd = (struct hil_kbd *)serio->private;
	if (kbd == NULL) {
		BUG();
		return;
	}

	if (kbd->idx4 >= (HIL_KBD_MAX_LENGTH * sizeof(hil_packet))) {
		hil_kbd_process_err(kbd);
		return;
	}
	idx = kbd->idx4/4;
	if (!(kbd->idx4 % 4)) kbd->data[idx] = 0;
	packet = kbd->data[idx];
	packet |= ((hil_packet)data) << ((3 - (kbd->idx4 % 4)) * 8);
	kbd->data[idx] = packet;

	/* Records of N 4-byte hil_packets must terminate with a command. */
	if ((++(kbd->idx4)) % 4) return;
	if ((packet & 0xffff0000) != HIL_ERR_INT) {
		hil_kbd_process_err(kbd);
		return;
	}
	if (packet & HIL_PKT_CMD) hil_kbd_process_record(kbd);
}

static void hil_kbd_disconnect(struct serio *serio)
{
	struct hil_kbd *kbd;

	kbd = (struct hil_kbd *)serio->private;
	if (kbd == NULL) {
		BUG();
		return;
	}

	input_unregister_device(&kbd->dev);
	serio_close(serio);
	kfree(kbd);
}

static void hil_kbd_connect(struct serio *serio, struct serio_dev *dev)
{
	struct hil_kbd	*kbd;
	uint8_t		did, *idd;
	int		i;
	
	if (serio->type != (SERIO_HIL_MLC | SERIO_HIL)) return;

	if (!(kbd = kmalloc(sizeof(struct hil_kbd), GFP_KERNEL))) return;
	memset(kbd, 0, sizeof(struct hil_kbd));

	if (serio_open(serio, dev)) goto bail0;

	serio->private = kbd;
	kbd->serio = serio;
	kbd->dev.private = kbd;

	init_MUTEX_LOCKED(&(kbd->sem));

	/* Get device info.  MLC driver supplies devid/status/etc. */
	serio->write(serio, 0);
	serio->write(serio, 0);
	serio->write(serio, HIL_PKT_CMD >> 8);
	serio->write(serio, HIL_CMD_IDD);
	down(&(kbd->sem));

	serio->write(serio, 0);
	serio->write(serio, 0);
	serio->write(serio, HIL_PKT_CMD >> 8);
	serio->write(serio, HIL_CMD_RSC);
	down(&(kbd->sem));

	serio->write(serio, 0);
	serio->write(serio, 0);
	serio->write(serio, HIL_PKT_CMD >> 8);
	serio->write(serio, HIL_CMD_RNM);
	down(&(kbd->sem));

	serio->write(serio, 0);
	serio->write(serio, 0);
	serio->write(serio, HIL_PKT_CMD >> 8);
	serio->write(serio, HIL_CMD_EXD);
	down(&(kbd->sem));

	up(&(kbd->sem));

	did = kbd->idd[0];
	idd = kbd->idd + 1;
	switch (did & HIL_IDD_DID_TYPE_MASK) {
	case HIL_IDD_DID_TYPE_KB_INTEGRAL:
	case HIL_IDD_DID_TYPE_KB_ITF:
	case HIL_IDD_DID_TYPE_KB_RSVD:
	case HIL_IDD_DID_TYPE_CHAR:
		printk(KERN_INFO PREFIX "HIL keyboard found (did = 0x%02x, lang = %s)\n",
			did, hil_language[did & HIL_IDD_DID_TYPE_KB_LANG_MASK]);
		break;
	default:
		goto bail1;
	}

	if(HIL_IDD_NUM_BUTTONS(idd) || HIL_IDD_NUM_AXES_PER_SET(*idd)) {
		printk(KERN_INFO PREFIX "keyboards only, no combo devices supported.\n");
		goto bail1;
	}

	kbd->dev.name = strlen(kbd->rnm) ? kbd->rnm : HIL_GENERIC_NAME;

	kbd->dev.idbus		= BUS_HIL;
	kbd->dev.idvendor	= SERIO_HIL;
	kbd->dev.idproduct	= 0x0001; /* TODO: get from kbd->rsc */
	kbd->dev.idversion	= 0x0100; /* TODO: get from kbd->rsc */

	kbd->dev.evbit[0] |= BIT(EV_KEY);

	for (i = 0; i < 128; i++) {
		set_bit(hil_kbd_set1[i], kbd->dev.keybit);
		set_bit(hil_kbd_set3[i], kbd->dev.keybit);
	}
	clear_bit(0, kbd->dev.keybit);

#if 1
	/* XXX: HACK !!!
	 * remove this call if hp_psaux.c/hp_keyb.c is converted
	 * to the input layer... */
	register_ps2_keybfuncs();
#endif
	
	input_register_device(&kbd->dev);
	printk(KERN_INFO "input%d: %s on hil%d\n",
		kbd->dev.number, "HIL keyboard", 0);

	/* HIL keyboards don't have a numlock key,
	 * simulate a up-down sequence of numlock to 
	 * make the keypad work at expected. */
	input_report_key(&kbd->dev, KEY_NUMLOCK, 1);
/*	input_report_key(&kbd->dev, KEY_NUMLOCK, 0); */

	serio->write(serio, 0);
	serio->write(serio, 0);
	serio->write(serio, HIL_PKT_CMD >> 8);
	serio->write(serio, HIL_CMD_EK1); /* Enable Keyswitch Autorepeat 1 */
	down(&(kbd->sem));
	up(&(kbd->sem));

	return;
 bail1:
	serio_close(serio);
 bail0:
	kfree(kbd);
	return;
}


struct serio_dev hil_kbd_serio_dev = {
	.connect =	hil_kbd_connect,
	.disconnect =	hil_kbd_disconnect,
	.interrupt =	hil_kbd_interrupt
};

static int __init hil_kbd_init(void)
{
	serio_register_device(&hil_kbd_serio_dev);
        return 0;
}
                
static void __exit hil_kbd_exit(void)
{
	serio_unregister_device(&hil_kbd_serio_dev);

#if 1
	/* XXX: HACK !!!
	 * remove this call if hp_psaux.c/hp_keyb.c is converted
	 * to the input layer... */
	unregister_kbd_ops();
#endif
}
                        
module_init(hil_kbd_init);
module_exit(hil_kbd_exit);
