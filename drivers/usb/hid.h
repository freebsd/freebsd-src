#ifndef __HID_H
#define __HID_H

/*
 * $Id: hid.h,v 1.10 2001/05/10 15:56:07 vojtech Exp $
 *
 *  Copyright (c) 1999 Andreas Gal
 *  Copyright (c) 2000-2001 Vojtech Pavlik
 *
 *  Sponsored by SuSE
 */

/*
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Should you need to contact me, the author, you can do so either by
 * e-mail - mail your message to <vojtech@suse.cz>, or by paper mail:
 * Vojtech Pavlik, Ucitelska 1576, Prague 8, 182 00 Czech Republic
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/list.h>

/*
 * USB HID (Human Interface Device) interface class code
 */

#define USB_INTERFACE_CLASS_HID		3

/*
 * We parse each description item into this structure. Short items data
 * values are expanded to 32-bit signed int, long items contain a pointer
 * into the data area.
 */

struct hid_item {
	unsigned  format;
	__u8      size;
	__u8      type;
	__u8      tag;
	union {
	    __u8   u8;
	    __s8   s8;
	    __u16  u16;
	    __s16  s16;
	    __u32  u32;
	    __s32  s32;
	    __u8  *longdata;
	} data;
};

/*
 * HID report item format
 */

#define HID_ITEM_FORMAT_SHORT	0
#define HID_ITEM_FORMAT_LONG	1

/*
 * Special tag indicating long items
 */

#define HID_ITEM_TAG_LONG	15

/*
 * HID report descriptor item type (prefix bit 2,3)
 */

#define HID_ITEM_TYPE_MAIN		0
#define HID_ITEM_TYPE_GLOBAL		1
#define HID_ITEM_TYPE_LOCAL		2
#define HID_ITEM_TYPE_RESERVED		3

/*
 * HID report descriptor main item tags
 */

#define HID_MAIN_ITEM_TAG_INPUT			8
#define HID_MAIN_ITEM_TAG_OUTPUT		9
#define HID_MAIN_ITEM_TAG_FEATURE		11
#define HID_MAIN_ITEM_TAG_BEGIN_COLLECTION	10
#define HID_MAIN_ITEM_TAG_END_COLLECTION	12

/*
 * HID report descriptor main item contents
 */

#define HID_MAIN_ITEM_CONSTANT		0x001
#define HID_MAIN_ITEM_VARIABLE		0x002
#define HID_MAIN_ITEM_RELATIVE		0x004
#define HID_MAIN_ITEM_WRAP		0x008	
#define HID_MAIN_ITEM_NONLINEAR		0x010
#define HID_MAIN_ITEM_NO_PREFERRED	0x020
#define HID_MAIN_ITEM_NULL_STATE	0x040
#define HID_MAIN_ITEM_VOLATILE		0x080
#define HID_MAIN_ITEM_BUFFERED_BYTE	0x100

/*
 * HID report descriptor collection item types
 */

#define HID_COLLECTION_PHYSICAL		0
#define HID_COLLECTION_APPLICATION	1
#define HID_COLLECTION_LOGICAL		2

/*
 * HID report descriptor global item tags
 */

#define HID_GLOBAL_ITEM_TAG_USAGE_PAGE		0
#define HID_GLOBAL_ITEM_TAG_LOGICAL_MINIMUM	1
#define HID_GLOBAL_ITEM_TAG_LOGICAL_MAXIMUM	2
#define HID_GLOBAL_ITEM_TAG_PHYSICAL_MINIMUM	3
#define HID_GLOBAL_ITEM_TAG_PHYSICAL_MAXIMUM	4
#define HID_GLOBAL_ITEM_TAG_UNIT_EXPONENT	5
#define HID_GLOBAL_ITEM_TAG_UNIT		6
#define HID_GLOBAL_ITEM_TAG_REPORT_SIZE		7
#define HID_GLOBAL_ITEM_TAG_REPORT_ID		8
#define HID_GLOBAL_ITEM_TAG_REPORT_COUNT	9
#define HID_GLOBAL_ITEM_TAG_PUSH		10
#define HID_GLOBAL_ITEM_TAG_POP			11

/*
 * HID report descriptor local item tags
 */

#define HID_LOCAL_ITEM_TAG_USAGE		0
#define HID_LOCAL_ITEM_TAG_USAGE_MINIMUM	1
#define HID_LOCAL_ITEM_TAG_USAGE_MAXIMUM	2
#define HID_LOCAL_ITEM_TAG_DESIGNATOR_INDEX	3
#define HID_LOCAL_ITEM_TAG_DESIGNATOR_MINIMUM	4
#define HID_LOCAL_ITEM_TAG_DESIGNATOR_MAXIMUM	5
#define HID_LOCAL_ITEM_TAG_STRING_INDEX		7
#define HID_LOCAL_ITEM_TAG_STRING_MINIMUM	8
#define HID_LOCAL_ITEM_TAG_STRING_MAXIMUM	9
#define HID_LOCAL_ITEM_TAG_DELIMITER		10

/*
 * HID usage tables
 */

#define HID_USAGE_PAGE		0xffff0000

#define HID_UP_GENDESK 		0x00010000
#define HID_UP_KEYBOARD 	0x00070000
#define HID_UP_LED 		0x00080000
#define HID_UP_BUTTON 		0x00090000
#define HID_UP_CONSUMER		0x000c0000
#define HID_UP_DIGITIZER 	0x000d0000
#define HID_UP_PID 		0x000f0000

#define HID_USAGE		0x0000ffff

#define HID_GD_POINTER		0x00010001
#define HID_GD_MOUSE		0x00010002
#define HID_GD_JOYSTICK		0x00010004
#define HID_GD_GAMEPAD		0x00010005
#define HID_GD_HATSWITCH	0x00010039

/*
 * HID report types --- Ouch! HID spec says 1 2 3!
 */

#define HID_INPUT_REPORT	0
#define HID_OUTPUT_REPORT	1
#define HID_FEATURE_REPORT	2

/*
 * HID device quirks.
 */

#define HID_QUIRK_INVERT	0x01
#define HID_QUIRK_NOTOUCH	0x02
#define HID_QUIRK_IGNORE	0x04
#define HID_QUIRK_NOGET		0x08
#define HID_QUIRK_HIDDEV	0x10
#define HID_QUIRK_BADPAD	0x20
#define HID_QUIRK_MULTI_INPUT	0x40

/*
 * This is the global environment of the parser. This information is
 * persistent for main-items. The global environment can be saved and
 * restored with PUSH/POP statements.
 */

struct hid_global {
	unsigned usage_page;
	__s32    logical_minimum;
	__s32    logical_maximum;
	__s32    physical_minimum;
	__s32    physical_maximum;
	unsigned unit_exponent;
	unsigned unit;
	unsigned report_id;
	unsigned report_size;
	unsigned report_count;
};

/*
 * This is the local environment. It is persistent up the next main-item.
 */

#define HID_MAX_DESCRIPTOR_SIZE		4096
#define HID_MAX_USAGES			1024
#define HID_MAX_APPLICATIONS		16
#define HID_DEFAULT_NUM_COLLECTIONS	16

struct hid_local {
	unsigned usage[HID_MAX_USAGES]; /* usage array */
	unsigned collection_index[HID_MAX_USAGES]; /* collection index array */
	unsigned usage_index;
	unsigned usage_minimum;
	unsigned delimiter_depth;
	unsigned delimiter_branch;
};

/*
 * This is the collection stack. We climb up the stack to determine
 * application and function of each field.
 */

struct hid_collection {
	unsigned type;
	unsigned usage;
	unsigned level;
};

struct hid_usage {
	unsigned  hid;			/* hid usage code */
	unsigned  collection_index;	/* index into collection array */
	__u16     code;			/* input driver code */
	__u8      type;			/* input driver type */
	__s8	  hat_min;		/* hat switch fun */
	__s8	  hat_max;		/* ditto */
};

struct hid_field {
	unsigned  physical;		/* physical usage for this field */
	unsigned  logical;		/* logical usage for this field */
	unsigned  application;		/* application usage for this field */
	struct hid_usage *usage;	/* usage table for this function */
	unsigned  maxusage;		/* maximum usage index */
	unsigned  flags;		/* main-item flags (i.e. volatile,array,constant) */
	unsigned  report_offset;	/* bit offset in the report */
	unsigned  report_size;		/* size of this field in the report */
	unsigned  report_count;		/* number of this field in the report */
	unsigned  report_type;		/* (input,output,feature) */
	__s32    *value;		/* last known value(s) */
	__s32     logical_minimum;
	__s32     logical_maximum;
	__s32     physical_minimum;
	__s32     physical_maximum;
	unsigned  unit_exponent;
	unsigned  unit;
	struct hid_report *report;	/* associated report */
	unsigned index;			/* index into report->field[] */
};

#define HID_MAX_FIELDS 64

struct hid_report {
	struct list_head list;
	unsigned id;					/* id of this report */
	unsigned type;					/* report type */
	struct hid_field *field[HID_MAX_FIELDS];	/* fields of the report */
	unsigned maxfield;				/* maximum valid field index */
	unsigned size;					/* size of the report (bits) */
	unsigned idx;					/* where we're in data */
	unsigned char *data;				/* data for multi-packet reports */
	struct hid_device *device;			/* associated device */
};

struct hid_report_enum {
	unsigned numbered;
	struct list_head report_list;
	struct hid_report *report_id_hash[256];
};

#define HID_REPORT_TYPES 3

#define HID_BUFFER_SIZE		32
#define HID_CONTROL_FIFO_SIZE	8

struct hid_control_fifo {
	struct usb_ctrlrequest dr;
	char buffer[HID_BUFFER_SIZE];
};

#define HID_CLAIMED_INPUT	1
#define HID_CLAIMED_HIDDEV	2

struct hid_input {
	struct list_head list;
	struct hid_report *report;
	struct input_dev input;
};

struct hid_device {							/* device report descriptor */
	 __u8 *rdesc;
	unsigned rsize;
	struct hid_collection *collection;                              /* List of HID collections */
	unsigned collection_size;                                       /* Number of allocated hid_collections */
	unsigned maxcollection;                                         /* Number of parsed collections */
	unsigned maxapplication;					/* Number of applications */
	unsigned version;						/* HID version */
	unsigned country;						/* HID country */
	struct hid_report_enum report_enum[HID_REPORT_TYPES];

	struct usb_device *dev;						/* USB device */
	int ifnum;							/* USB interface number */

	struct urb urb;							/* USB URB structure */
	char buffer[HID_BUFFER_SIZE];					/* Rx buffer */

	struct urb urbout;						/* Output URB */
	struct hid_control_fifo out[HID_CONTROL_FIFO_SIZE];		/* Transmit buffer */
	unsigned char outhead, outtail;					/* Tx buffer head & tail */

	unsigned claimed;						/* Claimed by hidinput, hiddev? */	
	unsigned quirks;						/* Various quirks the device can pull on us */

	struct list_head inputs;					/* The list of inputs */
	void *hiddev;							/* The hiddev structure */
	int minor;							/* Hiddev minor number */

	int open;							/* is the device open by anyone? */
	char name[128];							/* Device name */
};

#define HID_GLOBAL_STACK_SIZE 4
#define HID_COLLECTION_STACK_SIZE 4

struct hid_parser {
	struct hid_global     global;
	struct hid_global     global_stack[HID_GLOBAL_STACK_SIZE];
	unsigned              global_stack_ptr;
	struct hid_local      local;
	unsigned              collection_stack[HID_COLLECTION_STACK_SIZE];
	unsigned              collection_stack_ptr;
	struct hid_device    *device;
};

struct hid_class_descriptor {
	__u8  bDescriptorType;
	__u16 wDescriptorLength;
} __attribute__ ((packed));

struct hid_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__u16 bcdHID;
	__u8  bCountryCode;
	__u8  bNumDescriptors;

	struct hid_class_descriptor desc[1];
} __attribute__ ((packed));


#ifdef DEBUG
#include "hid-debug.h"
#else
#define hid_dump_input(a,b)	do { } while (0)
#define hid_dump_device(c)	do { } while (0)
#endif

#endif

#ifdef CONFIG_USB_HIDINPUT
#define IS_INPUT_APPLICATION(a) (((a >= 0x00010000) && (a <= 0x00010008)) || (a == 0x00010080) || ( a == 0x000c0001))
extern void hidinput_hid_event(struct hid_device *, struct hid_field *, struct hid_usage *, __s32);
extern int hidinput_connect(struct hid_device *);
extern void hidinput_disconnect(struct hid_device *);
#else
#define IS_INPUT_APPLICATION(a) (0)
static inline void hidinput_hid_event(struct hid_device *hid, struct hid_field *field, struct hid_usage *usage, __s32 value) { }
static inline int hidinput_connect(struct hid_device *hid) { return -ENODEV; }
static inline void hidinput_disconnect(struct hid_device *hid) { }
#endif

int hid_open(struct hid_device *);
void hid_close(struct hid_device *);
int hid_find_field(struct hid_device *, unsigned int, unsigned int, struct hid_field **);
int hid_set_field(struct hid_field *, unsigned, __s32);
void hid_write_report(struct hid_device *, struct hid_report *);
void hid_read_report(struct hid_device *, struct hid_report *);
void hid_init_reports(struct hid_device *hid);
