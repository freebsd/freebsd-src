/*
 * acm.c  Version 0.21
 *
 * Copyright (c) 1999 Armin Fuerst	<fuerst@in.tum.de>
 * Copyright (c) 1999 Pavel Machek	<pavel@suse.cz>
 * Copyright (c) 1999 Johannes Erdfelt	<johannes@erdfelt.com>
 * Copyright (c) 2000 Vojtech Pavlik	<vojtech@suse.cz>
 *
 * USB Abstract Control Model driver for USB modems and ISDN adapters
 *
 * Sponsored by SuSE
 *
 * ChangeLog:
 *	v0.9  - thorough cleaning, URBification, almost a rewrite
 *	v0.10 - some more cleanups
 *	v0.11 - fixed flow control, read error doesn't stop reads
 *	v0.12 - added TIOCM ioctls, added break handling, made struct acm kmalloced
 *	v0.13 - added termios, added hangup
 *	v0.14 - sized down struct acm
 *	v0.15 - fixed flow control again - characters could be lost
 *	v0.16 - added code for modems with swapped data and control interfaces
 *	v0.17 - added new style probing
 *	v0.18 - fixed new style probing for devices with more configurations
 *	v0.19 - fixed CLOCAL handling (thanks to Richard Shih-Ping Chan)
 *	v0.20 - switched to probing on interface (rather than device) class
 *	v0.21 - revert to probing on device for devices with multiple configs
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
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/module.h>
#include <linux/smp_lock.h>
#undef DEBUG
#include <linux/usb.h>

/*
 * Version Information
 */
#define DRIVER_VERSION "v0.21"
#define DRIVER_AUTHOR "Armin Fuerst, Pavel Machek, Johannes Erdfelt, Vojtech Pavlik"
#define DRIVER_DESC "USB Abstract Control Model driver for USB modems and ISDN adapters"

/*
 * CMSPAR, some architectures can't have space and mark parity.
 */

#ifndef CMSPAR
#define CMSPAR			0
#endif

/*
 * Major and minor numbers.
 */

#define ACM_TTY_MAJOR		166
#define ACM_TTY_MINORS		32

/*
 * Requests.
 */

#define USB_RT_ACM		(USB_TYPE_CLASS | USB_RECIP_INTERFACE)

#define ACM_REQ_COMMAND		0x00
#define ACM_REQ_RESPONSE	0x01
#define ACM_REQ_SET_FEATURE	0x02
#define ACM_REQ_GET_FEATURE	0x03
#define ACM_REQ_CLEAR_FEATURE	0x04

#define ACM_REQ_SET_LINE	0x20
#define ACM_REQ_GET_LINE	0x21
#define ACM_REQ_SET_CONTROL	0x22
#define ACM_REQ_SEND_BREAK	0x23

/*
 * IRQs.
 */

#define ACM_IRQ_NETWORK		0x00
#define ACM_IRQ_LINE_STATE	0x20

/*
 * Output control lines.
 */

#define ACM_CTRL_DTR		0x01
#define ACM_CTRL_RTS		0x02

/*
 * Input control lines and line errors.
 */

#define ACM_CTRL_DCD		0x01
#define ACM_CTRL_DSR		0x02
#define ACM_CTRL_BRK		0x04
#define ACM_CTRL_RI		0x08

#define ACM_CTRL_FRAMING	0x10
#define ACM_CTRL_PARITY		0x20
#define ACM_CTRL_OVERRUN	0x40

/*
 * Line speed and caracter encoding.
 */

struct acm_line {
	__u32 speed;
	__u8 stopbits;
	__u8 parity;
	__u8 databits;
} __attribute__ ((packed));

/*
 * Internal driver structures.
 */

struct acm {
	struct usb_device *dev;				/* the coresponding usb device */
	struct usb_interface *iface;			/* the interfaces - +0 control +1 data */
	struct tty_struct *tty;				/* the coresponding tty */
	struct urb ctrlurb, readurb, writeurb;		/* urbs */
	struct acm_line line;				/* line coding (bits, stop, parity) */
	struct tq_struct tqueue;			/* task queue for line discipline waking up */
	unsigned int ctrlin;				/* input control lines (DCD, DSR, RI, break, overruns) */
	unsigned int ctrlout;				/* output control lines (DTR, RTS) */
	unsigned int writesize;				/* max packet size for the output bulk endpoint */
	unsigned int used;				/* someone has this acm's device open */
	unsigned int minor;				/* acm minor number */
	unsigned char throttle;				/* throttled by tty layer */
	unsigned char clocal;				/* termios CLOCAL */
};

static struct usb_driver acm_driver;
static struct tty_driver acm_tty_driver;
static struct acm *acm_table[ACM_TTY_MINORS];

#define ACM_READY(acm)	(acm && acm->dev && acm->used)

/*
 * Functions for ACM control messages.
 */

static int acm_ctrl_msg(struct acm *acm, int request, int value, void *buf, int len)
{
	int retval = usb_control_msg(acm->dev, usb_sndctrlpipe(acm->dev, 0),
		request, USB_RT_ACM, value, acm->iface[0].altsetting[0].bInterfaceNumber, buf, len, HZ * 5);
	dbg("acm_control_msg: rq: 0x%02x val: %#x len: %#x result: %d", request, value, len, retval);
	return retval < 0 ? retval : 0;
}

#define acm_set_control(acm, control)	acm_ctrl_msg(acm, ACM_REQ_SET_CONTROL, control, NULL, 0)
#define acm_set_line(acm, line)		acm_ctrl_msg(acm, ACM_REQ_SET_LINE, 0, line, sizeof(struct acm_line))
#define acm_send_break(acm, ms)		acm_ctrl_msg(acm, ACM_REQ_SEND_BREAK, ms, NULL, 0)

/*
 * Interrupt handler for various ACM control events
 */

static void acm_ctrl_irq(struct urb *urb)
{
	struct acm *acm = urb->context;
	struct usb_ctrlrequest *dr = urb->transfer_buffer;
	unsigned char *data = (unsigned char *)(dr + 1);
	int newctrl;

	if (!ACM_READY(acm)) return;

	if (urb->status < 0) {
		dbg("nonzero ctrl irq status received: %d", urb->status);
		return;
	}

	switch (dr->bRequest) {

		case ACM_IRQ_NETWORK:

			dbg("%s network", data[0] ? "connected to" : "disconnected from");
			return;

		case ACM_IRQ_LINE_STATE:

			newctrl = le16_to_cpup((__u16 *) data);

			if (acm->tty && !acm->clocal && (acm->ctrlin & ~newctrl & ACM_CTRL_DCD)) {
				dbg("calling hangup");
				tty_hangup(acm->tty);
			}

			acm->ctrlin = newctrl;

			dbg("input control lines: dcd%c dsr%c break%c ring%c framing%c parity%c overrun%c",
				acm->ctrlin & ACM_CTRL_DCD ? '+' : '-',	acm->ctrlin & ACM_CTRL_DSR ? '+' : '-',
				acm->ctrlin & ACM_CTRL_BRK ? '+' : '-',	acm->ctrlin & ACM_CTRL_RI  ? '+' : '-',
				acm->ctrlin & ACM_CTRL_FRAMING ? '+' : '-',	acm->ctrlin & ACM_CTRL_PARITY ? '+' : '-',
				acm->ctrlin & ACM_CTRL_OVERRUN ? '+' : '-');

			return;

		default:
			dbg("unknown control event received: request %d index %d len %d data0 %d data1 %d",
				dr->bRequest, dr->wIndex, dr->wLength, data[0], data[1]);
			return;
	}
}

static void acm_read_bulk(struct urb *urb)
{
	struct acm *acm = urb->context;
	struct tty_struct *tty = acm->tty;
	unsigned char *data = urb->transfer_buffer;
	int i = 0;

	if (!ACM_READY(acm)) return;

	if (urb->status)
		dbg("nonzero read bulk status received: %d", urb->status);

	if (!urb->status && !acm->throttle)  {
		for (i = 0; i < urb->actual_length && !acm->throttle; i++) {
			/* if we insert more than TTY_FLIPBUF_SIZE characters,
			 * we drop them. */
			if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
				tty_flip_buffer_push(tty);
			}
			tty_insert_flip_char(tty, data[i], 0);
		}
		tty_flip_buffer_push(tty);
	}

	if (acm->throttle) {
		memmove(data, data + i, urb->actual_length - i);
		urb->actual_length -= i;
		return;
	}

	urb->actual_length = 0;
	urb->dev = acm->dev;

	if (usb_submit_urb(urb))
		dbg("failed resubmitting read urb");
}

static void acm_write_bulk(struct urb *urb)
{
	struct acm *acm = (struct acm *)urb->context;

	if (!ACM_READY(acm)) return;

	if (urb->status)
		dbg("nonzero write bulk status received: %d", urb->status);

	queue_task(&acm->tqueue, &tq_immediate);
	mark_bh(IMMEDIATE_BH);
}

static void acm_softint(void *private)
{
	struct acm *acm = private;
	struct tty_struct *tty = acm->tty;

	if (!ACM_READY(acm)) return;

	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) && tty->ldisc.write_wakeup)
		(tty->ldisc.write_wakeup)(tty);

	wake_up_interruptible(&tty->write_wait);
}

/*
 * TTY handlers
 */

static int acm_tty_open(struct tty_struct *tty, struct file *filp)
{
	struct acm *acm = acm_table[MINOR(tty->device)];

	if (!acm || !acm->dev) return -EINVAL;

	tty->driver_data = acm;
	acm->tty = tty;

	MOD_INC_USE_COUNT;

        lock_kernel();

	if (acm->used++) {
                unlock_kernel();
                return 0;
        }

        unlock_kernel();

	acm->ctrlurb.dev = acm->dev;
	if (usb_submit_urb(&acm->ctrlurb))
		dbg("usb_submit_urb(ctrl irq) failed");

	acm->readurb.dev = acm->dev;
	if (usb_submit_urb(&acm->readurb))
		dbg("usb_submit_urb(read bulk) failed");

	acm_set_control(acm, acm->ctrlout = ACM_CTRL_DTR | ACM_CTRL_RTS);

	/* force low_latency on so that our tty_push actually forces the data through, 
	   otherwise it is scheduled, and with high data rates data can get lost. */
	tty->low_latency = 1;

	return 0;
}

static void acm_tty_close(struct tty_struct *tty, struct file *filp)
{
	struct acm *acm = tty->driver_data;

	if (!acm || !acm->used) return;

	if (!--acm->used) {
		if (acm->dev) {
			acm_set_control(acm, acm->ctrlout = 0);
			usb_unlink_urb(&acm->ctrlurb);
			usb_unlink_urb(&acm->writeurb);
			usb_unlink_urb(&acm->readurb);
		} else {
			tty_unregister_devfs(&acm_tty_driver, acm->minor);
			acm_table[acm->minor] = NULL;
			kfree(acm);
		}
	}
	MOD_DEC_USE_COUNT;
}

static int acm_tty_write(struct tty_struct *tty, int from_user, const unsigned char *buf, int count)
{
	struct acm *acm = tty->driver_data;

	if (!ACM_READY(acm)) return -EINVAL;
	if (acm->writeurb.status == -EINPROGRESS) return 0;
	if (!count) return 0;

	count = (count > acm->writesize) ? acm->writesize : count;

	if (from_user) {
		if (copy_from_user(acm->writeurb.transfer_buffer, buf, count))
			return -EFAULT;
	} else
		memcpy(acm->writeurb.transfer_buffer, buf, count);

	acm->writeurb.transfer_buffer_length = count;
	acm->writeurb.dev = acm->dev;

	if (usb_submit_urb(&acm->writeurb))
		dbg("usb_submit_urb(write bulk) failed");

	return count;
}

static int acm_tty_write_room(struct tty_struct *tty)
{
	struct acm *acm = tty->driver_data;
	if (!ACM_READY(acm)) return -EINVAL;
	return acm->writeurb.status == -EINPROGRESS ? 0 : acm->writesize;
}

static int acm_tty_chars_in_buffer(struct tty_struct *tty)
{
	struct acm *acm = tty->driver_data;
	if (!ACM_READY(acm)) return -EINVAL;
	return acm->writeurb.status == -EINPROGRESS ? acm->writeurb.transfer_buffer_length : 0;
}

static void acm_tty_throttle(struct tty_struct *tty)
{
	struct acm *acm = tty->driver_data;
	if (!ACM_READY(acm)) return;
	acm->throttle = 1;
}

static void acm_tty_unthrottle(struct tty_struct *tty)
{
	struct acm *acm = tty->driver_data;
	if (!ACM_READY(acm)) return;
	acm->throttle = 0;
	if (acm->readurb.status != -EINPROGRESS)
		acm_read_bulk(&acm->readurb);
}

static void acm_tty_break_ctl(struct tty_struct *tty, int state)
{
	struct acm *acm = tty->driver_data;
	if (!ACM_READY(acm)) return;
	if (acm_send_break(acm, state ? 0xffff : 0))
		dbg("send break failed");
}

static int acm_tty_ioctl(struct tty_struct *tty, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct acm *acm = tty->driver_data;
	unsigned int mask, newctrl;

	if (!ACM_READY(acm)) return -EINVAL;

	switch (cmd) {

		case TIOCMGET:

			return put_user((acm->ctrlout & ACM_CTRL_DTR ? TIOCM_DTR : 0) |
				(acm->ctrlout & ACM_CTRL_RTS ? TIOCM_RTS : 0) |
				(acm->ctrlin  & ACM_CTRL_DSR ? TIOCM_DSR : 0) |
				(acm->ctrlin  & ACM_CTRL_RI  ? TIOCM_RI  : 0) |
				(acm->ctrlin  & ACM_CTRL_DCD ? TIOCM_CD  : 0) |
				 TIOCM_CTS, (unsigned long *) arg);

		case TIOCMSET:
		case TIOCMBIS:
		case TIOCMBIC:

			if (get_user(mask, (unsigned long *) arg))
				return -EFAULT;

			newctrl = acm->ctrlout;
			mask = (mask & TIOCM_DTR ? ACM_CTRL_DTR : 0) | (mask & TIOCM_RTS ? ACM_CTRL_RTS : 0);

			switch (cmd) {
				case TIOCMSET: newctrl  =  mask; break;
				case TIOCMBIS: newctrl |=  mask; break;
				case TIOCMBIC: newctrl &= ~mask; break;
			}

			if (acm->ctrlout == newctrl) return 0;
			return acm_set_control(acm, acm->ctrlout = newctrl);
	}

	return -ENOIOCTLCMD;
}

static __u32 acm_tty_speed[] = {
	0, 50, 75, 110, 134, 150, 200, 300, 600,
	1200, 1800, 2400, 4800, 9600, 19200, 38400,
	57600, 115200, 230400, 460800, 500000, 576000,
	921600, 1000000, 1152000, 1500000, 2000000,
	2500000, 3000000, 3500000, 4000000
};

static __u8 acm_tty_size[] = {
	5, 6, 7, 8
};

static void acm_tty_set_termios(struct tty_struct *tty, struct termios *termios_old)
{
	struct acm *acm = tty->driver_data;
	struct termios *termios = tty->termios;
	struct acm_line newline;
	int newctrl = acm->ctrlout;

	if (!ACM_READY(acm)) return;

	newline.speed = cpu_to_le32p(acm_tty_speed +
		(termios->c_cflag & CBAUD & ~CBAUDEX) + (termios->c_cflag & CBAUDEX ? 15 : 0));
	newline.stopbits = termios->c_cflag & CSTOPB ? 2 : 0;
	newline.parity = termios->c_cflag & PARENB ?
		(termios->c_cflag & PARODD ? 1 : 2) + (termios->c_cflag & CMSPAR ? 2 : 0) : 0;
	newline.databits = acm_tty_size[(termios->c_cflag & CSIZE) >> 4];

	acm->clocal = ((termios->c_cflag & CLOCAL) != 0);

	if (!newline.speed) {
		newline.speed = acm->line.speed;
		newctrl &= ~ACM_CTRL_DTR;
	} else  newctrl |=  ACM_CTRL_DTR;

	if (newctrl != acm->ctrlout)
		acm_set_control(acm, acm->ctrlout = newctrl);

	if (memcmp(&acm->line, &newline, sizeof(struct acm_line))) {
		memcpy(&acm->line, &newline, sizeof(struct acm_line));
		dbg("set line: %d %d %d %d", newline.speed, newline.stopbits, newline.parity, newline.databits);
		acm_set_line(acm, &acm->line);
	}
}

/*
 * USB probe and disconnect routines.
 */

static void *acm_probe(struct usb_device *dev, unsigned int ifnum,
		       const struct usb_device_id *id)
{
	struct acm *acm;
	struct usb_config_descriptor *cfacm;
	struct usb_interface_descriptor *ifcom, *ifdata;
	struct usb_endpoint_descriptor *epctrl, *epread, *epwrite;
	int readsize, ctrlsize, minor, i, j;
	unsigned char *buf;

	for (i = 0; i < dev->descriptor.bNumConfigurations; i++) {

		cfacm = dev->config + i;

		dbg("probing config %d", cfacm->bConfigurationValue);

		for (j = 0; j < cfacm->bNumInterfaces - 1; j++) {

			if (usb_interface_claimed(cfacm->interface + j) ||
			    usb_interface_claimed(cfacm->interface + j + 1))
				continue;

			ifcom = cfacm->interface[j].altsetting + 0;
			ifdata = cfacm->interface[j + 1].altsetting + 0;

			if (ifdata->bInterfaceClass != 10 || ifdata->bNumEndpoints < 2) {
				ifcom = cfacm->interface[j + 1].altsetting + 0;
				ifdata = cfacm->interface[j].altsetting + 0;
				if (ifdata->bInterfaceClass != 10 || ifdata->bNumEndpoints < 2)
					continue;
			}

			if (ifcom->bInterfaceClass != 2 || ifcom->bInterfaceSubClass != 2 ||
			    ifcom->bInterfaceProtocol < 1 || ifcom->bInterfaceProtocol > 6 ||
			    ifcom->bNumEndpoints < 1)
				continue;

			epctrl = ifcom->endpoint + 0;
			epread = ifdata->endpoint + 0;
			epwrite = ifdata->endpoint + 1;

			if ((epctrl->bEndpointAddress & 0x80) != 0x80 || (epctrl->bmAttributes & 3) != 3 ||
			    (epread->bmAttributes & 3) != 2 || (epwrite->bmAttributes & 3) != 2 ||
			    ((epread->bEndpointAddress & 0x80) ^ (epwrite->bEndpointAddress & 0x80)) != 0x80)
				continue;

			dbg("using interface %d\n", j);

			if ((epread->bEndpointAddress & 0x80) != 0x80) {
				epread = ifdata->endpoint + 1;
				epwrite = ifdata->endpoint + 0;
			}

			usb_set_configuration(dev, cfacm->bConfigurationValue);

			for (minor = 0; minor < ACM_TTY_MINORS && acm_table[minor]; minor++);
			if (acm_table[minor]) {
				err("no more free acm devices");
				return NULL;
			}

			if (!(acm = kmalloc(sizeof(struct acm), GFP_KERNEL))) {
				err("out of memory");
				return NULL;
			}
			memset(acm, 0, sizeof(struct acm));

			ctrlsize = epctrl->wMaxPacketSize;
			readsize = epread->wMaxPacketSize;
			acm->writesize = epwrite->wMaxPacketSize;
			acm->iface = cfacm->interface + j;
			acm->minor = minor;
			acm->dev = dev;

			acm->tqueue.routine = acm_softint;
			acm->tqueue.data = acm;

			if (!(buf = kmalloc(ctrlsize + readsize + acm->writesize, GFP_KERNEL))) {
				err("out of memory");
				kfree(acm);
				return NULL;
			}

			FILL_INT_URB(&acm->ctrlurb, dev, usb_rcvintpipe(dev, epctrl->bEndpointAddress),
				     buf, ctrlsize, acm_ctrl_irq, acm, epctrl->bInterval);

			FILL_BULK_URB(&acm->readurb, dev, usb_rcvbulkpipe(dev, epread->bEndpointAddress),
				      buf += ctrlsize, readsize, acm_read_bulk, acm);
			acm->readurb.transfer_flags |= USB_NO_FSBR;

			FILL_BULK_URB(&acm->writeurb, dev, usb_sndbulkpipe(dev, epwrite->bEndpointAddress),
				      buf += readsize, acm->writesize, acm_write_bulk, acm);
			acm->writeurb.transfer_flags |= USB_NO_FSBR;

			printk(KERN_INFO "ttyACM%d: USB ACM device\n", minor);

			acm_set_control(acm, acm->ctrlout);

			acm->line.speed = cpu_to_le32(9600);
			acm->line.databits = 8;
			acm_set_line(acm, &acm->line);

			usb_driver_claim_interface(&acm_driver, acm->iface + 0, acm);
			usb_driver_claim_interface(&acm_driver, acm->iface + 1, acm);

			tty_register_devfs(&acm_tty_driver, 0, minor);
			return acm_table[minor] = acm;
		}
	}

	return NULL;
}

static void acm_disconnect(struct usb_device *dev, void *ptr)
{
	struct acm *acm = ptr;

	if (!acm || !acm->dev) {
		dbg("disconnect on nonexisting interface");
		return;
	}

	acm->dev = NULL;

	usb_unlink_urb(&acm->ctrlurb);
	usb_unlink_urb(&acm->readurb);
	usb_unlink_urb(&acm->writeurb);

	kfree(acm->ctrlurb.transfer_buffer);

	usb_driver_release_interface(&acm_driver, acm->iface + 0);
	usb_driver_release_interface(&acm_driver, acm->iface + 1);

	if (!acm->used) {
		tty_unregister_devfs(&acm_tty_driver, acm->minor);
		acm_table[acm->minor] = NULL;
		kfree(acm);
		return;
	}

	if (acm->tty)
		tty_hangup(acm->tty);
}

/*
 * USB driver structure.
 */

static struct usb_device_id acm_ids[] = {
	{ USB_DEVICE(0x22B8, 0x1005) },		/* Motorola TimePort 280 */
	{ USB_DEVICE_INFO(USB_CLASS_COMM, 0, 0) },
	{ USB_DEVICE_INFO(USB_CLASS_COMM, 2, 0) },
	{ }
};

MODULE_DEVICE_TABLE (usb, acm_ids);

static struct usb_driver acm_driver = {
	name:		"acm",
	probe:		acm_probe,
	disconnect:	acm_disconnect,
	id_table:	acm_ids,
};

/*
 * TTY driver structures.
 */

static int acm_tty_refcount;

static struct tty_struct *acm_tty_table[ACM_TTY_MINORS];
static struct termios *acm_tty_termios[ACM_TTY_MINORS];
static struct termios *acm_tty_termios_locked[ACM_TTY_MINORS];

static struct tty_driver acm_tty_driver = {
	magic:			TTY_DRIVER_MAGIC,
	driver_name:		"acm",
	name:			"usb/acm/%d",
	major:			ACM_TTY_MAJOR,
	minor_start:		0,
	num:			ACM_TTY_MINORS,
	type:			TTY_DRIVER_TYPE_SERIAL,
	subtype:		SERIAL_TYPE_NORMAL,
	flags:			TTY_DRIVER_REAL_RAW | TTY_DRIVER_NO_DEVFS,

	refcount:		&acm_tty_refcount,

	table:			acm_tty_table,
	termios:		acm_tty_termios,
	termios_locked:		acm_tty_termios_locked,

	open:			acm_tty_open,
	close:			acm_tty_close,
	write:			acm_tty_write,
	write_room:		acm_tty_write_room,
	ioctl:			acm_tty_ioctl,
	throttle:		acm_tty_throttle,
	unthrottle:		acm_tty_unthrottle,
	chars_in_buffer:	acm_tty_chars_in_buffer,
	break_ctl:		acm_tty_break_ctl,
	set_termios:		acm_tty_set_termios
};

/*
 * Init / exit.
 */

static int __init acm_init(void)
{
	acm_tty_driver.init_termios =		tty_std_termios;
	acm_tty_driver.init_termios.c_cflag =	B9600 | CS8 | CREAD | HUPCL | CLOCAL;

	if (tty_register_driver(&acm_tty_driver))
		return -1;

	if (usb_register(&acm_driver) < 0) {
		tty_unregister_driver(&acm_tty_driver);
		return -1;
	}

	info(DRIVER_VERSION ":" DRIVER_DESC);

	return 0;
}

static void __exit acm_exit(void)
{
	usb_deregister(&acm_driver);
	tty_unregister_driver(&acm_tty_driver);
}

module_init(acm_init);
module_exit(acm_exit);

MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL");

