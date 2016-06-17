/* 
 * Sony Programmable I/O Control Device driver for VAIO
 *
 * Copyright (C) 2001-2003 Stelian Pop <stelian@popies.net>
 *
 * Copyright (C) 2001-2002 Alcôve <www.alcove.com>
 *
 * Copyright (C) 2001 Michael Ashley <m.ashley@unsw.edu.au>
 *
 * Copyright (C) 2001 Junichi Morita <jun1m@mars.dti.ne.jp>
 *
 * Copyright (C) 2000 Takaya Kinjo <t-kinjo@tc4.so-net.ne.jp>
 *
 * Copyright (C) 2000 Andrew Tridgell <tridge@valinux.com>
 *
 * Earlier work by Werner Almesberger, Paul `Rusty' Russell and Paul Mackerras.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef _SONYPI_PRIV_H_ 
#define _SONYPI_PRIV_H_

#ifdef __KERNEL__

#define SONYPI_DRIVER_MAJORVERSION	 1
#define SONYPI_DRIVER_MINORVERSION	22

#define SONYPI_DEVICE_MODEL_TYPE1	1
#define SONYPI_DEVICE_MODEL_TYPE2	2

#include <linux/config.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/input.h>
#include <linux/pm.h>
#include <linux/acpi.h>
#include "linux/sonypi.h"

/* type1 models use those */
#define SONYPI_IRQ_PORT			0x8034
#define SONYPI_IRQ_SHIFT		22
#define SONYPI_BASE			0x50
#define SONYPI_G10A			(SONYPI_BASE+0x14)
#define SONYPI_TYPE1_REGION_SIZE	0x08
#define SONYPI_TYPE1_EVTYPE_OFFSET	0x04

/* type2 series specifics */
#define SONYPI_SIRQ			0x9b
#define SONYPI_SLOB			0x9c
#define SONYPI_SHIB			0x9d
#define SONYPI_TYPE2_REGION_SIZE	0x20
#define SONYPI_TYPE2_EVTYPE_OFFSET	0x12

/* battery / brightness addresses */
#define SONYPI_BAT_FLAGS	0x81
#define SONYPI_LCD_LIGHT	0x96
#define SONYPI_BAT1_PCTRM	0xa0
#define SONYPI_BAT1_LEFT	0xa2
#define SONYPI_BAT1_MAXRT	0xa4
#define SONYPI_BAT2_PCTRM	0xa8
#define SONYPI_BAT2_LEFT	0xaa
#define SONYPI_BAT2_MAXRT	0xac
#define SONYPI_BAT1_MAXTK	0xb0
#define SONYPI_BAT1_FULL	0xb2
#define SONYPI_BAT2_MAXTK	0xb8
#define SONYPI_BAT2_FULL	0xba

/* ioports used for brightness and type2 events */
#define SONYPI_DATA_IOPORT	0x62
#define SONYPI_CST_IOPORT	0x66

/* The set of possible ioports */
struct sonypi_ioport_list {
	u16	port1;
	u16	port2;
};

static struct sonypi_ioport_list sonypi_type1_ioport_list[] = {
	{ 0x10c0, 0x10c4 },	/* looks like the default on C1Vx */
	{ 0x1080, 0x1084 },
	{ 0x1090, 0x1094 },
	{ 0x10a0, 0x10a4 },
	{ 0x10b0, 0x10b4 },
	{ 0x0, 0x0 }
};

static struct sonypi_ioport_list sonypi_type2_ioport_list[] = {
	{ 0x1080, 0x1084 },
	{ 0x10a0, 0x10a4 },
	{ 0x10c0, 0x10c4 },
	{ 0x10e0, 0x10e4 },
	{ 0x0, 0x0 }
};

/* The set of possible interrupts */
struct sonypi_irq_list {
	u16	irq;
	u16	bits;
};

static struct sonypi_irq_list sonypi_type1_irq_list[] = {
	{ 11, 0x2 },	/* IRQ 11, GO22=0,GO23=1 in AML */
	{ 10, 0x1 },	/* IRQ 10, GO22=1,GO23=0 in AML */
	{  5, 0x0 },	/* IRQ  5, GO22=0,GO23=0 in AML */
	{  0, 0x3 }	/* no IRQ, GO22=1,GO23=1 in AML */
};

static struct sonypi_irq_list sonypi_type2_irq_list[] = {
	{ 11, 0x80 },	/* IRQ 11, 0x80 in SIRQ in AML */
	{ 10, 0x40 },	/* IRQ 10, 0x40 in SIRQ in AML */
	{  9, 0x20 },	/* IRQ  9, 0x20 in SIRQ in AML */
	{  6, 0x10 },	/* IRQ  6, 0x10 in SIRQ in AML */
	{  0, 0x00 }	/* no IRQ, 0x00 in SIRQ in AML */
};

#define SONYPI_CAMERA_BRIGHTNESS		0
#define SONYPI_CAMERA_CONTRAST			1
#define SONYPI_CAMERA_HUE			2
#define SONYPI_CAMERA_COLOR			3
#define SONYPI_CAMERA_SHARPNESS			4

#define SONYPI_CAMERA_PICTURE			5
#define SONYPI_CAMERA_EXPOSURE_MASK		0xC
#define SONYPI_CAMERA_WHITE_BALANCE_MASK	0x3
#define SONYPI_CAMERA_PICTURE_MODE_MASK		0x30
#define SONYPI_CAMERA_MUTE_MASK			0x40

/* the rest don't need a loop until not 0xff */
#define SONYPI_CAMERA_AGC			6
#define SONYPI_CAMERA_AGC_MASK			0x30
#define SONYPI_CAMERA_SHUTTER_MASK 		0x7

#define SONYPI_CAMERA_SHUTDOWN_REQUEST		7
#define SONYPI_CAMERA_CONTROL			0x10

#define SONYPI_CAMERA_STATUS 			7
#define SONYPI_CAMERA_STATUS_READY 		0x2
#define SONYPI_CAMERA_STATUS_POSITION		0x4

#define SONYPI_DIRECTION_BACKWARDS 		0x4

#define SONYPI_CAMERA_REVISION 			8
#define SONYPI_CAMERA_ROMVERSION 		9

/* Event masks */
#define SONYPI_JOGGER_MASK			0x00000001
#define SONYPI_CAPTURE_MASK			0x00000002
#define SONYPI_FNKEY_MASK			0x00000004
#define SONYPI_BLUETOOTH_MASK			0x00000008
#define SONYPI_PKEY_MASK			0x00000010
#define SONYPI_BACK_MASK			0x00000020
#define SONYPI_HELP_MASK			0x00000040
#define SONYPI_LID_MASK				0x00000080
#define SONYPI_ZOOM_MASK			0x00000100
#define SONYPI_THUMBPHRASE_MASK			0x00000200
#define SONYPI_MEYE_MASK			0x00000400
#define SONYPI_MEMORYSTICK_MASK			0x00000800
#define SONYPI_BATTERY_MASK			0x00001000

struct sonypi_event {
	u8	data;
	u8	event;
};

/* The set of possible button release events */
static struct sonypi_event sonypi_releaseev[] = {
	{ 0x00, SONYPI_EVENT_ANYBUTTON_RELEASED },
	{ 0, 0 }
};

/* The set of possible jogger events  */
static struct sonypi_event sonypi_joggerev[] = {
	{ 0x1f, SONYPI_EVENT_JOGDIAL_UP },
	{ 0x01, SONYPI_EVENT_JOGDIAL_DOWN },
	{ 0x5f, SONYPI_EVENT_JOGDIAL_UP_PRESSED },
	{ 0x41, SONYPI_EVENT_JOGDIAL_DOWN_PRESSED },
	{ 0x1e, SONYPI_EVENT_JOGDIAL_FAST_UP },
	{ 0x02, SONYPI_EVENT_JOGDIAL_FAST_DOWN },
	{ 0x5e, SONYPI_EVENT_JOGDIAL_FAST_UP_PRESSED },
	{ 0x42, SONYPI_EVENT_JOGDIAL_FAST_DOWN_PRESSED },
	{ 0x1d, SONYPI_EVENT_JOGDIAL_VFAST_UP },
	{ 0x03, SONYPI_EVENT_JOGDIAL_VFAST_DOWN },
	{ 0x5d, SONYPI_EVENT_JOGDIAL_VFAST_UP_PRESSED },
	{ 0x43, SONYPI_EVENT_JOGDIAL_VFAST_DOWN_PRESSED },
	{ 0x40, SONYPI_EVENT_JOGDIAL_PRESSED },
	{ 0, 0 }
};

/* The set of possible capture button events */
static struct sonypi_event sonypi_captureev[] = {
	{ 0x05, SONYPI_EVENT_CAPTURE_PARTIALPRESSED },
	{ 0x07, SONYPI_EVENT_CAPTURE_PRESSED },
	{ 0x01, SONYPI_EVENT_CAPTURE_PARTIALRELEASED },
	{ 0, 0 }
};

/* The set of possible fnkeys events */
static struct sonypi_event sonypi_fnkeyev[] = {
	{ 0x10, SONYPI_EVENT_FNKEY_ESC },
	{ 0x11, SONYPI_EVENT_FNKEY_F1 },
	{ 0x12, SONYPI_EVENT_FNKEY_F2 },
	{ 0x13, SONYPI_EVENT_FNKEY_F3 },
	{ 0x14, SONYPI_EVENT_FNKEY_F4 },
	{ 0x15, SONYPI_EVENT_FNKEY_F5 },
	{ 0x16, SONYPI_EVENT_FNKEY_F6 },
	{ 0x17, SONYPI_EVENT_FNKEY_F7 },
	{ 0x18, SONYPI_EVENT_FNKEY_F8 },
	{ 0x19, SONYPI_EVENT_FNKEY_F9 },
	{ 0x1a, SONYPI_EVENT_FNKEY_F10 },
	{ 0x1b, SONYPI_EVENT_FNKEY_F11 },
	{ 0x1c, SONYPI_EVENT_FNKEY_F12 },
	{ 0x21, SONYPI_EVENT_FNKEY_1 },
	{ 0x22, SONYPI_EVENT_FNKEY_2 },
	{ 0x31, SONYPI_EVENT_FNKEY_D },
	{ 0x32, SONYPI_EVENT_FNKEY_E },
	{ 0x33, SONYPI_EVENT_FNKEY_F },
	{ 0x34, SONYPI_EVENT_FNKEY_S },
	{ 0x35, SONYPI_EVENT_FNKEY_B },
	{ 0x36, SONYPI_EVENT_FNKEY_ONLY },
	{ 0, 0 }
};

/* The set of possible program key events */
static struct sonypi_event sonypi_pkeyev[] = {
	{ 0x01, SONYPI_EVENT_PKEY_P1 },
	{ 0x02, SONYPI_EVENT_PKEY_P2 },
	{ 0x04, SONYPI_EVENT_PKEY_P3 },
	{ 0x5c, SONYPI_EVENT_PKEY_P1 },
	{ 0, 0 }
};

/* The set of possible bluetooth events */
static struct sonypi_event sonypi_blueev[] = {
	{ 0x55, SONYPI_EVENT_BLUETOOTH_PRESSED },
	{ 0x59, SONYPI_EVENT_BLUETOOTH_ON },
	{ 0x5a, SONYPI_EVENT_BLUETOOTH_OFF },
	{ 0, 0 }
};

/* The set of possible back button events */
static struct sonypi_event sonypi_backev[] = {
	{ 0x20, SONYPI_EVENT_BACK_PRESSED },
	{ 0, 0 }
};

/* The set of possible help button events */
static struct sonypi_event sonypi_helpev[] = {
	{ 0x3b, SONYPI_EVENT_HELP_PRESSED },
	{ 0, 0 }
};


/* The set of possible lid events */
static struct sonypi_event sonypi_lidev[] = {
	{ 0x51, SONYPI_EVENT_LID_CLOSED },
	{ 0x50, SONYPI_EVENT_LID_OPENED },
	{ 0, 0 }
};

/* The set of possible zoom events */
static struct sonypi_event sonypi_zoomev[] = {
	{ 0x39, SONYPI_EVENT_ZOOM_PRESSED },
	{ 0, 0 }
};

/* The set of possible thumbphrase events */
static struct sonypi_event sonypi_thumbphraseev[] = {
	{ 0x3a, SONYPI_EVENT_THUMBPHRASE_PRESSED },
	{ 0, 0 }
};

/* The set of possible motioneye camera events */
static struct sonypi_event sonypi_meyeev[] = {
	{ 0x00, SONYPI_EVENT_MEYE_FACE },
	{ 0x01, SONYPI_EVENT_MEYE_OPPOSITE },
	{ 0, 0 }
};

/* The set of possible memorystick events */
static struct sonypi_event sonypi_memorystickev[] = {
	{ 0x53, SONYPI_EVENT_MEMORYSTICK_INSERT },
	{ 0x54, SONYPI_EVENT_MEMORYSTICK_EJECT },
	{ 0, 0 }
};

/* The set of possible battery events */
static struct sonypi_event sonypi_batteryev[] = {
	{ 0x20, SONYPI_EVENT_BATTERY_INSERT },
	{ 0x30, SONYPI_EVENT_BATTERY_REMOVE },
	{ 0, 0 }
};

struct sonypi_eventtypes {
	int			model;
	u8			data;
	unsigned long		mask;
	struct sonypi_event *	events;
} sonypi_eventtypes[] = {
	{ SONYPI_DEVICE_MODEL_TYPE1, 0, 0xffffffff, sonypi_releaseev },
	{ SONYPI_DEVICE_MODEL_TYPE1, 0x70, SONYPI_MEYE_MASK, sonypi_meyeev },
	{ SONYPI_DEVICE_MODEL_TYPE1, 0x30, SONYPI_LID_MASK, sonypi_lidev },
	{ SONYPI_DEVICE_MODEL_TYPE1, 0x60, SONYPI_CAPTURE_MASK, sonypi_captureev },
	{ SONYPI_DEVICE_MODEL_TYPE1, 0x10, SONYPI_JOGGER_MASK, sonypi_joggerev },
	{ SONYPI_DEVICE_MODEL_TYPE1, 0x20, SONYPI_FNKEY_MASK, sonypi_fnkeyev },
	{ SONYPI_DEVICE_MODEL_TYPE1, 0x30, SONYPI_BLUETOOTH_MASK, sonypi_blueev },
	{ SONYPI_DEVICE_MODEL_TYPE1, 0x40, SONYPI_PKEY_MASK, sonypi_pkeyev },
	{ SONYPI_DEVICE_MODEL_TYPE1, 0x30, SONYPI_MEMORYSTICK_MASK, sonypi_memorystickev },
	{ SONYPI_DEVICE_MODEL_TYPE1, 0x40, SONYPI_BATTERY_MASK, sonypi_batteryev },

	{ SONYPI_DEVICE_MODEL_TYPE2, 0, 0xffffffff, sonypi_releaseev },
	{ SONYPI_DEVICE_MODEL_TYPE2, 0x38, SONYPI_LID_MASK, sonypi_lidev },
	{ SONYPI_DEVICE_MODEL_TYPE2, 0x11, SONYPI_JOGGER_MASK, sonypi_joggerev },
	{ SONYPI_DEVICE_MODEL_TYPE2, 0x61, SONYPI_CAPTURE_MASK, sonypi_captureev },
	{ SONYPI_DEVICE_MODEL_TYPE2, 0x21, SONYPI_FNKEY_MASK, sonypi_fnkeyev },
	{ SONYPI_DEVICE_MODEL_TYPE2, 0x31, SONYPI_BLUETOOTH_MASK, sonypi_blueev },
	{ SONYPI_DEVICE_MODEL_TYPE2, 0x08, SONYPI_PKEY_MASK, sonypi_pkeyev },
	{ SONYPI_DEVICE_MODEL_TYPE2, 0x11, SONYPI_BACK_MASK, sonypi_backev },
	{ SONYPI_DEVICE_MODEL_TYPE2, 0x08, SONYPI_HELP_MASK, sonypi_helpev },
	{ SONYPI_DEVICE_MODEL_TYPE2, 0x21, SONYPI_ZOOM_MASK, sonypi_zoomev },
	{ SONYPI_DEVICE_MODEL_TYPE2, 0x20, SONYPI_THUMBPHRASE_MASK, sonypi_thumbphraseev },
	{ SONYPI_DEVICE_MODEL_TYPE2, 0x31, SONYPI_MEMORYSTICK_MASK, sonypi_memorystickev },
	{ SONYPI_DEVICE_MODEL_TYPE2, 0x41, SONYPI_BATTERY_MASK, sonypi_batteryev },
	{ SONYPI_DEVICE_MODEL_TYPE2, 0x31, SONYPI_PKEY_MASK, sonypi_pkeyev },

	{ 0, 0, 0, 0 }
};

#define SONYPI_BUF_SIZE	128
struct sonypi_queue {
	unsigned long head;
	unsigned long tail;
	unsigned long len;
	spinlock_t s_lock;
	wait_queue_head_t proc_list;
	struct fasync_struct *fasync;
	unsigned char buf[SONYPI_BUF_SIZE];
};

/* We enable input subsystem event forwarding if the input 
 * subsystem is compiled in, but only if sonypi is not into the
 * kernel and input as a module... */
#if defined(CONFIG_INPUT) || defined(CONFIG_INPUT_MODULE)
#if ! (defined(CONFIG_SONYPI) && defined(CONFIG_INPUT_MODULE))
#define SONYPI_USE_INPUT
#endif
#endif

/* The name of the Jog Dial for the input device drivers */
#define SONYPI_INPUTNAME	"Sony VAIO Jog Dial"

struct sonypi_device {
	struct pci_dev *dev;
	u16 irq;
	u16 bits;
	u16 ioport1;
	u16 ioport2;
	u16 region_size;
	u16 evtype_offset;
	int camera_power;
	int bluetooth_power;
	struct semaphore lock;
	struct sonypi_queue queue;
	int open_count;
	int model;
#ifdef SONYPI_USE_INPUT
	struct input_dev jog_dev;
#endif
#ifdef CONFIG_PM
	struct pm_dev *pm;
#endif
};

#define ITERATIONS_LONG		10000
#define ITERATIONS_SHORT	10

#define wait_on_command(quiet, command, iterations) { \
	unsigned int n = iterations; \
	while (--n && (command)) \
		udelay(1); \
	if (!n && (verbose || !quiet)) \
		printk(KERN_WARNING "sonypi command failed at %s : %s (line %d)\n", __FILE__, __FUNCTION__, __LINE__); \
}

#ifdef CONFIG_ACPI
#define SONYPI_ACPI_ACTIVE (!acpi_disabled)
#else
#define SONYPI_ACPI_ACTIVE 0
#endif /* CONFIG_ACPI */

extern int verbose;

static inline int sonypi_ec_write(u8 addr, u8 value) {
#ifdef CONFIG_ACPI_EC
	if (SONYPI_ACPI_ACTIVE)
		return ec_write(addr, value);
#endif
	wait_on_command(1, inb_p(SONYPI_CST_IOPORT) & 3, ITERATIONS_LONG);
	outb_p(0x81, SONYPI_CST_IOPORT);
	wait_on_command(0, inb_p(SONYPI_CST_IOPORT) & 2, ITERATIONS_LONG);
	outb_p(addr, SONYPI_DATA_IOPORT);
	wait_on_command(0, inb_p(SONYPI_CST_IOPORT) & 2, ITERATIONS_LONG);
	outb_p(value, SONYPI_DATA_IOPORT);
	wait_on_command(0, inb_p(SONYPI_CST_IOPORT) & 2, ITERATIONS_LONG);
	return 0;
}

static inline int sonypi_ec_read(u8 addr, u8 *value) {
#ifdef CONFIG_ACPI_EC
	if (SONYPI_ACPI_ACTIVE)
		return ec_read(addr, value);
#endif
	wait_on_command(1, inb_p(SONYPI_CST_IOPORT) & 3, ITERATIONS_LONG);
	outb_p(0x80, SONYPI_CST_IOPORT);
	wait_on_command(0, inb_p(SONYPI_CST_IOPORT) & 2, ITERATIONS_LONG);
	outb_p(addr, SONYPI_DATA_IOPORT);
	wait_on_command(0, inb_p(SONYPI_CST_IOPORT) & 2, ITERATIONS_LONG);
	*value = inb_p(SONYPI_DATA_IOPORT);
	return 0;
}

#endif /* __KERNEL__ */

#endif /* _SONYPI_PRIV_H_ */
