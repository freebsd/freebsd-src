/* 
   HCI USB driver for Linux Bluetooth protocol stack (BlueZ)
   Copyright (C) 2000-2001 Qualcomm Incorporated
   Written 2000,2001 by Maxim Krasnyansky <maxk@qualcomm.com>

   Copyright (C) 2003 Maxim Krasnyansky <maxk@qualcomm.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation;

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
   IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) AND AUTHOR(S) BE LIABLE FOR ANY
   CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES 
   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN 
   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF 
   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

   ALL LIABILITY, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PATENTS, 
   COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS, RELATING TO USE OF THIS 
   SOFTWARE IS DISCLAIMED.
*/

/*
 * Based on original USB Bluetooth driver for Linux kernel
 *    Copyright (c) 2000 Greg Kroah-Hartman        <greg@kroah.com>
 *    Copyright (c) 2000 Mark Douglas Corner       <mcorner@umich.edu>
 *
 * $Id: hci_usb.c,v 1.8 2002/07/18 17:23:09 maxk Exp $    
 */
#define VERSION "2.5"

#include <linux/config.h>
#include <linux/module.h>

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/interrupt.h>

#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/skbuff.h>

#include <linux/usb.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#include "hci_usb.h"

#ifndef HCI_USB_DEBUG
#undef  BT_DBG
#define BT_DBG( A... )
#undef  BT_DMP
#define BT_DMP( A... )
#endif

#ifndef CONFIG_BLUEZ_HCIUSB_ZERO_PACKET
#undef  USB_ZERO_PACKET
#define USB_ZERO_PACKET 0
#endif

static struct usb_driver hci_usb_driver; 

static struct usb_device_id bluetooth_ids[] = {
	/* Generic Bluetooth USB device */
	{ USB_DEVICE_INFO(HCI_DEV_CLASS, HCI_DEV_SUBCLASS, HCI_DEV_PROTOCOL) },

	/* AVM BlueFRITZ! USB v2.0 */
	{ USB_DEVICE(0x057c, 0x3800) },

	/* Ericsson with non-standard id */
	{ USB_DEVICE(0x0bdb, 0x1002) },

	/* ALPS Module with non-standard id */
	{ USB_DEVICE(0x044e, 0x3002) },

	/* Bluetooth Ultraport Module from IBM */
	{ USB_DEVICE(0x04bf, 0x030a) },

	{ }	/* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, bluetooth_ids);

static struct usb_device_id blacklist_ids[] = {
	/* Broadcom BCM2033 without firmware */
	{ USB_DEVICE(0x0a5c, 0x2033), driver_info: HCI_IGNORE },

	/* Broadcom BCM2035 */
	{ USB_DEVICE(0x0a5c, 0x200a), driver_info: HCI_RESET },

	/* Digianswer device */
	{ USB_DEVICE(0x08fd, 0x0001), driver_info: HCI_DIGIANSWER },

	{ }	/* Terminating entry */
};

struct _urb *_urb_alloc(int isoc, int gfp)
{
	struct _urb *_urb = kmalloc(sizeof(struct _urb) +
				sizeof(struct iso_packet_descriptor) * isoc, gfp);
	if (_urb) {
		memset(_urb, 0, sizeof(*_urb));
		spin_lock_init(&_urb->urb.lock);
	}
	return _urb;
}

struct _urb *_urb_dequeue(struct _urb_queue *q)
{
	struct _urb *_urb = NULL;
        unsigned long flags;
        spin_lock_irqsave(&q->lock, flags);
	{
		struct list_head *head = &q->head;
		struct list_head *next = head->next;
		if (next != head) {
			_urb = list_entry(next, struct _urb, list);
			list_del(next); _urb->queue = NULL;
		}
	}
	spin_unlock_irqrestore(&q->lock, flags);
	return _urb;
}

static void hci_usb_rx_complete(struct urb *urb);
static void hci_usb_tx_complete(struct urb *urb);

#define __pending_tx(husb, type)  (&husb->pending_tx[type-1])
#define __pending_q(husb, type)   (&husb->pending_q[type-1])
#define __completed_q(husb, type) (&husb->completed_q[type-1])
#define __transmit_q(husb, type)  (&husb->transmit_q[type-1])
#define __reassembly(husb, type)  (husb->reassembly[type-1])

static inline struct _urb *__get_completed(struct hci_usb *husb, int type)
{
	return _urb_dequeue(__completed_q(husb, type)); 
}

#ifdef CONFIG_BLUEZ_HCIUSB_SCO
static void __fill_isoc_desc(struct urb *urb, int len, int mtu)
{
	int offset = 0, i;

	BT_DBG("len %d mtu %d", len, mtu);

	for (i=0; i < HCI_MAX_ISOC_FRAMES && len >= mtu; i++, offset += mtu, len -= mtu) {
		urb->iso_frame_desc[i].offset = offset;
		urb->iso_frame_desc[i].length = mtu;
		BT_DBG("desc %d offset %d len %d", i, offset, mtu);
	}
	if (len && i < HCI_MAX_ISOC_FRAMES) {
		urb->iso_frame_desc[i].offset = offset;
		urb->iso_frame_desc[i].length = len;
		BT_DBG("desc %d offset %d len %d", i, offset, len);
		i++;
	}
	urb->number_of_packets = i;
}
#endif

static int hci_usb_intr_rx_submit(struct hci_usb *husb)
{
	struct _urb *_urb;
	struct urb *urb;
	int err, pipe, interval, size;
	void *buf;

	BT_DBG("%s", husb->hdev.name);

        size = husb->intr_in_ep->wMaxPacketSize;

	buf = kmalloc(size, GFP_ATOMIC);
	if (!buf)
		return -ENOMEM;

	_urb = _urb_alloc(0, GFP_ATOMIC);
	if (!_urb) {
		kfree(buf);
		return -ENOMEM;
	}
	_urb->type = HCI_EVENT_PKT;
	_urb_queue_tail(__pending_q(husb, _urb->type), _urb);

	urb = &_urb->urb;
	pipe     = usb_rcvintpipe(husb->udev, husb->intr_in_ep->bEndpointAddress);
	interval = husb->intr_in_ep->bInterval;
	FILL_INT_URB(urb, husb->udev, pipe, buf, size, hci_usb_rx_complete, husb, interval);
	
	err = usb_submit_urb(urb);
	if (err) {
		BT_ERR("%s intr rx submit failed urb %p err %d",
				husb->hdev.name, urb, err);
		_urb_unlink(_urb);
		_urb_free(_urb);
		kfree(buf);
	}
	return err;
}

static int hci_usb_bulk_rx_submit(struct hci_usb *husb)
{
	struct _urb *_urb;
	struct urb *urb;
	int err, pipe, size = HCI_MAX_FRAME_SIZE;
	void *buf;

	buf = kmalloc(size, GFP_ATOMIC);
	if (!buf)
		return -ENOMEM;

	_urb = _urb_alloc(0, GFP_ATOMIC);
	if (!_urb) {
		kfree(buf);
		return -ENOMEM;
	}
	_urb->type = HCI_ACLDATA_PKT;
	_urb_queue_tail(__pending_q(husb, _urb->type), _urb);

	urb  = &_urb->urb;
	pipe = usb_rcvbulkpipe(husb->udev, husb->bulk_in_ep->bEndpointAddress);
        FILL_BULK_URB(urb, husb->udev, pipe, buf, size, hci_usb_rx_complete, husb);
        urb->transfer_flags = USB_QUEUE_BULK;

	BT_DBG("%s urb %p", husb->hdev.name, urb);

	err = usb_submit_urb(urb);
	if (err) {
		BT_ERR("%s bulk rx submit failed urb %p err %d",
				husb->hdev.name, urb, err);
		_urb_unlink(_urb);
		_urb_free(_urb);
		kfree(buf);
	}
	return err;
}

#ifdef CONFIG_BLUEZ_HCIUSB_SCO
static int hci_usb_isoc_rx_submit(struct hci_usb *husb)
{
	struct _urb *_urb;
	struct urb *urb;
	int err, mtu, size;
	void *buf;

	mtu  = husb->isoc_in_ep->wMaxPacketSize;
        size = mtu * HCI_MAX_ISOC_FRAMES;

	buf = kmalloc(size, GFP_ATOMIC);
	if (!buf)
		return -ENOMEM;

	_urb = _urb_alloc(HCI_MAX_ISOC_FRAMES, GFP_ATOMIC);
	if (!_urb) {
		kfree(buf);
		return -ENOMEM;
	}
	_urb->type = HCI_SCODATA_PKT;
	_urb_queue_tail(__pending_q(husb, _urb->type), _urb);

	urb = &_urb->urb;

	urb->context  = husb;
	urb->dev      = husb->udev;
	urb->pipe     = usb_rcvisocpipe(husb->udev, husb->isoc_in_ep->bEndpointAddress);
	urb->complete = hci_usb_rx_complete;

	urb->transfer_buffer_length = size;
	urb->transfer_buffer = buf;
	urb->transfer_flags  = USB_ISO_ASAP;

	__fill_isoc_desc(urb, size, mtu);

	BT_DBG("%s urb %p", husb->hdev.name, urb);

	err = usb_submit_urb(urb);
	if (err) {
		BT_ERR("%s isoc rx submit failed urb %p err %d",
				husb->hdev.name, urb, err);
		_urb_unlink(_urb);
		_urb_free(_urb);
		kfree(buf);
	}
	return err;
}
#endif

/* Initialize device */
static int hci_usb_open(struct hci_dev *hdev)
{
	struct hci_usb *husb = (struct hci_usb *) hdev->driver_data;
	int i, err;
	unsigned long flags;

	BT_DBG("%s", hdev->name);

	if (test_and_set_bit(HCI_RUNNING, &hdev->flags))
		return 0;

	MOD_INC_USE_COUNT;

	write_lock_irqsave(&husb->completion_lock, flags);

	err = hci_usb_intr_rx_submit(husb);
	if (!err) {
		for (i = 0; i < HCI_MAX_BULK_RX; i++)
			hci_usb_bulk_rx_submit(husb);

#ifdef CONFIG_BLUEZ_HCIUSB_SCO
		if (husb->isoc_iface)
			for (i = 0; i < HCI_MAX_ISOC_RX; i++)
				hci_usb_isoc_rx_submit(husb);
#endif
	} else {
		clear_bit(HCI_RUNNING, &hdev->flags);
		MOD_DEC_USE_COUNT;
	}

	write_unlock_irqrestore(&husb->completion_lock, flags);
	return err;
}

/* Reset device */
static int hci_usb_flush(struct hci_dev *hdev)
{
	struct hci_usb *husb = (struct hci_usb *) hdev->driver_data;
	int i;

	BT_DBG("%s", hdev->name);

	for (i=0; i < 4; i++)
		skb_queue_purge(&husb->transmit_q[i]);
	return 0;
}

static void hci_usb_unlink_urbs(struct hci_usb *husb)
{
	int i;

	BT_DBG("%s", husb->hdev.name);

	for (i=0; i < 4; i++) {
		struct _urb *_urb;
		struct urb *urb;

		/* Kill pending requests */
		while ((_urb = _urb_dequeue(&husb->pending_q[i]))) {
			urb = &_urb->urb;
			BT_DBG("%s unlinking _urb %p type %d urb %p", 
					husb->hdev.name, _urb, _urb->type, urb);
			usb_unlink_urb(urb);
			_urb_queue_tail(__completed_q(husb, _urb->type), _urb);
		}

		/* Release completed requests */
		while ((_urb = _urb_dequeue(&husb->completed_q[i]))) {
			urb = &_urb->urb;
			BT_DBG("%s freeing _urb %p type %d urb %p",
					husb->hdev.name, _urb, _urb->type, urb);
			if (urb->setup_packet)
				kfree(urb->setup_packet);
			if (urb->transfer_buffer)
				kfree(urb->transfer_buffer);
			_urb_free(_urb);
		}

		/* Release reassembly buffers */
		if (husb->reassembly[i]) {
			kfree_skb(husb->reassembly[i]);
			husb->reassembly[i] = NULL;
		}
	}
}

/* Close device */
static int hci_usb_close(struct hci_dev *hdev)
{
	struct hci_usb *husb = (struct hci_usb *) hdev->driver_data;
	unsigned long flags;
	
	if (!test_and_clear_bit(HCI_RUNNING, &hdev->flags))
		return 0;

	BT_DBG("%s", hdev->name);

	write_lock_irqsave(&husb->completion_lock, flags);
	
	hci_usb_unlink_urbs(husb);
	hci_usb_flush(hdev);

	write_unlock_irqrestore(&husb->completion_lock, flags);

	MOD_DEC_USE_COUNT;
	return 0;
}

static int __tx_submit(struct hci_usb *husb, struct _urb *_urb)
{
	struct urb *urb = &_urb->urb;
	int err;

	BT_DBG("%s urb %p type %d", husb->hdev.name, urb, _urb->type);
	
	_urb_queue_tail(__pending_q(husb, _urb->type), _urb);
	err = usb_submit_urb(urb);
	if (err) {
		BT_ERR("%s tx submit failed urb %p type %d err %d",
				husb->hdev.name, urb, _urb->type, err);
		_urb_unlink(_urb);
		_urb_queue_tail(__completed_q(husb, _urb->type), _urb);
	} else
		atomic_inc(__pending_tx(husb, _urb->type));

	return err;
}

static inline int hci_usb_send_ctrl(struct hci_usb *husb, struct sk_buff *skb)
{
	struct _urb *_urb = __get_completed(husb, skb->pkt_type);
	struct usb_ctrlrequest *dr;
	struct urb *urb;

	if (!_urb) {
	       	_urb = _urb_alloc(0, GFP_ATOMIC);
	       	if (!_urb)
			return -ENOMEM;
		_urb->type = skb->pkt_type;

		dr = kmalloc(sizeof(*dr), GFP_ATOMIC);
		if (!dr) {
			_urb_free(_urb);
			return -ENOMEM;
		}
	} else
		dr = (void *) _urb->urb.setup_packet;

	dr->bRequestType = husb->ctrl_req;
	dr->bRequest = 0;
	dr->wIndex   = 0;
	dr->wValue   = 0;
	dr->wLength  = __cpu_to_le16(skb->len);

	urb = &_urb->urb;
	FILL_CONTROL_URB(urb, husb->udev, usb_sndctrlpipe(husb->udev, 0),
		(void *) dr, skb->data, skb->len, hci_usb_tx_complete, husb);

	BT_DBG("%s skb %p len %d", husb->hdev.name, skb, skb->len);
	
	_urb->priv = skb;
	return __tx_submit(husb, _urb);
}

static inline int hci_usb_send_bulk(struct hci_usb *husb, struct sk_buff *skb)
{
	struct _urb *_urb = __get_completed(husb, skb->pkt_type);
	struct urb *urb;
	int pipe;

	if (!_urb) {
	       	_urb = _urb_alloc(0, GFP_ATOMIC);
	       	if (!_urb)
			return -ENOMEM;
		_urb->type = skb->pkt_type;
	}

	urb  = &_urb->urb;
	pipe = usb_sndbulkpipe(husb->udev, husb->bulk_out_ep->bEndpointAddress);
	FILL_BULK_URB(urb, husb->udev, pipe, skb->data, skb->len, 
			hci_usb_tx_complete, husb);
	urb->transfer_flags = USB_QUEUE_BULK | USB_ZERO_PACKET;

	BT_DBG("%s skb %p len %d", husb->hdev.name, skb, skb->len);

	_urb->priv = skb;
	return __tx_submit(husb, _urb);
}

#ifdef CONFIG_BLUEZ_HCIUSB_SCO
static inline int hci_usb_send_isoc(struct hci_usb *husb, struct sk_buff *skb)
{
	struct _urb *_urb = __get_completed(husb, skb->pkt_type);
	struct urb *urb;
	
	if (!_urb) {
	       	_urb = _urb_alloc(HCI_MAX_ISOC_FRAMES, GFP_ATOMIC);
	       	if (!_urb)
			return -ENOMEM;
		_urb->type = skb->pkt_type;
	}

	BT_DBG("%s skb %p len %d", husb->hdev.name, skb, skb->len);

	urb = &_urb->urb;
	
	urb->context  = husb;
	urb->dev      = husb->udev;
	urb->pipe     = usb_sndisocpipe(husb->udev, husb->isoc_out_ep->bEndpointAddress);
	urb->complete = hci_usb_tx_complete;
	urb->transfer_flags = USB_ISO_ASAP;

	urb->transfer_buffer = skb->data;
	urb->transfer_buffer_length = skb->len;
	
	__fill_isoc_desc(urb, skb->len, husb->isoc_out_ep->wMaxPacketSize);

	_urb->priv = skb;
	return __tx_submit(husb, _urb);
}
#endif

static void hci_usb_tx_process(struct hci_usb *husb)
{
	struct sk_buff_head *q;
	struct sk_buff *skb;

	BT_DBG("%s", husb->hdev.name);

	do {
		clear_bit(HCI_USB_TX_WAKEUP, &husb->state);

		/* Process command queue */
		q = __transmit_q(husb, HCI_COMMAND_PKT);
		if (!atomic_read(__pending_tx(husb, HCI_COMMAND_PKT)) &&
				(skb = skb_dequeue(q))) {
			if (hci_usb_send_ctrl(husb, skb) < 0)
				skb_queue_head(q, skb);
		}

#ifdef CONFIG_BLUEZ_HCIUSB_SCO
		/* Process SCO queue */
		q = __transmit_q(husb, HCI_SCODATA_PKT);
		if (atomic_read(__pending_tx(husb, HCI_SCODATA_PKT)) < HCI_MAX_ISOC_TX &&
				(skb = skb_dequeue(q))) {
			if (hci_usb_send_isoc(husb, skb) < 0)
				skb_queue_head(q, skb);
		}
#endif
		
		/* Process ACL queue */
		q = __transmit_q(husb, HCI_ACLDATA_PKT);
		while (atomic_read(__pending_tx(husb, HCI_ACLDATA_PKT)) < HCI_MAX_BULK_TX &&
				(skb = skb_dequeue(q))) {
			if (hci_usb_send_bulk(husb, skb) < 0) {
				skb_queue_head(q, skb);
				break;
			}
		}
	} while(test_bit(HCI_USB_TX_WAKEUP, &husb->state));
}

static inline void hci_usb_tx_wakeup(struct hci_usb *husb)
{
	/* Serialize TX queue processing to avoid data reordering */
	if (!test_and_set_bit(HCI_USB_TX_PROCESS, &husb->state)) {
		hci_usb_tx_process(husb);
		clear_bit(HCI_USB_TX_PROCESS, &husb->state);
	} else
		set_bit(HCI_USB_TX_WAKEUP, &husb->state);
}

/* Send frames from HCI layer */
static int hci_usb_send_frame(struct sk_buff *skb)
{
	struct hci_dev *hdev = (struct hci_dev *) skb->dev;
	struct hci_usb *husb;

	if (!hdev) {
		BT_ERR("frame for uknown device (hdev=NULL)");
		return -ENODEV;
	}

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		return -EBUSY;

	BT_DBG("%s type %d len %d", hdev->name, skb->pkt_type, skb->len);

	husb = (struct hci_usb *) hdev->driver_data;

	switch (skb->pkt_type) {
	case HCI_COMMAND_PKT:
		hdev->stat.cmd_tx++;
		break;

	case HCI_ACLDATA_PKT:
		hdev->stat.acl_tx++;
		break;

#ifdef CONFIG_BLUEZ_HCIUSB_SCO
	case HCI_SCODATA_PKT:
		hdev->stat.sco_tx++;
		break;
#endif

	default:
		kfree_skb(skb);
		return 0;
	}

	read_lock(&husb->completion_lock);

	skb_queue_tail(__transmit_q(husb, skb->pkt_type), skb);
	hci_usb_tx_wakeup(husb);

	read_unlock(&husb->completion_lock);
	return 0;
}

static inline int __recv_frame(struct hci_usb *husb, int type, void *data, int count)
{
	BT_DBG("%s type %d data %p count %d", husb->hdev.name, type, data, count);

	husb->hdev.stat.byte_rx += count;

	while (count) {
		struct sk_buff *skb = __reassembly(husb, type);
		struct { int expect; } *scb;
		int len = 0;
	
		if (!skb) {
			/* Start of the frame */

			switch (type) {
			case HCI_EVENT_PKT:
				if (count >= HCI_EVENT_HDR_SIZE) {
					hci_event_hdr *h = data;
					len = HCI_EVENT_HDR_SIZE + h->plen;
				} else
					return -EILSEQ;
				break;

			case HCI_ACLDATA_PKT:
				if (count >= HCI_ACL_HDR_SIZE) {
					hci_acl_hdr *h = data;
					len = HCI_ACL_HDR_SIZE + __le16_to_cpu(h->dlen);
				} else
					return -EILSEQ;
				break;
#ifdef CONFIG_BLUEZ_HCIUSB_SCO
			case HCI_SCODATA_PKT:
				if (count >= HCI_SCO_HDR_SIZE) {
					hci_sco_hdr *h = data;
					len = HCI_SCO_HDR_SIZE + h->dlen;
				} else 
					return -EILSEQ;
				break;
#endif
			}
			BT_DBG("new packet len %d", len);

			skb = bluez_skb_alloc(len, GFP_ATOMIC);
			if (!skb) {
				BT_ERR("%s no memory for the packet", husb->hdev.name);
				return -ENOMEM;
			}
			skb->dev = (void *) &husb->hdev;
			skb->pkt_type = type;
	
			__reassembly(husb, type) = skb;

			scb = (void *) skb->cb;
			scb->expect = len;
		} else {
			/* Continuation */
			scb = (void *) skb->cb;
			len = scb->expect;
		}

		len = min(len, count);
		
		memcpy(skb_put(skb, len), data, len);

		scb->expect -= len;
		if (!scb->expect) {
			/* Complete frame */
			__reassembly(husb, type) = NULL;
			hci_recv_frame(skb);
		}

		count -= len; data += len;
	}
	return 0;
}

static void hci_usb_rx_complete(struct urb *urb)
{
	struct _urb *_urb = container_of(urb, struct _urb, urb);
	struct hci_usb *husb = (void *) urb->context;
	struct hci_dev *hdev = &husb->hdev;
	int    err, count = urb->actual_length;

	BT_DBG("%s urb %p type %d status %d count %d flags %x", hdev->name, urb,
			_urb->type, urb->status, count, urb->transfer_flags);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		return;

	read_lock(&husb->completion_lock);

	if (urb->status || !count)
		goto resubmit;

	if (_urb->type == HCI_SCODATA_PKT) {
#ifdef CONFIG_BLUEZ_HCIUSB_SCO
		int i;
		for (i=0; i < urb->number_of_packets; i++) {
			BT_DBG("desc %d status %d offset %d len %d", i,
					urb->iso_frame_desc[i].status,
					urb->iso_frame_desc[i].offset,
					urb->iso_frame_desc[i].actual_length);
	
			if (!urb->iso_frame_desc[i].status)
				__recv_frame(husb, _urb->type, 
					urb->transfer_buffer + urb->iso_frame_desc[i].offset,
					urb->iso_frame_desc[i].actual_length);
		}
#else
		;
#endif
	} else {
		err = __recv_frame(husb, _urb->type, urb->transfer_buffer, count);
		if (err < 0) { 
			BT_ERR("%s corrupted packet: type %d count %d",
					husb->hdev.name, _urb->type, count);
			hdev->stat.err_rx++;
		}
	}

resubmit:
	if (_urb->type != HCI_EVENT_PKT) {
		urb->dev = husb->udev;
		err      = usb_submit_urb(urb);
		BT_DBG("%s urb %p type %d resubmit status %d", hdev->name, urb,
				_urb->type, err);
	}
	read_unlock(&husb->completion_lock);
}

static void hci_usb_tx_complete(struct urb *urb)
{
	struct _urb *_urb = container_of(urb, struct _urb, urb);
	struct hci_usb *husb = (void *) urb->context;
	struct hci_dev *hdev = &husb->hdev;

	BT_DBG("%s urb %p status %d flags %x", hdev->name, urb,
			urb->status, urb->transfer_flags);

	atomic_dec(__pending_tx(husb, _urb->type));

	urb->transfer_buffer = NULL;
	kfree_skb((struct sk_buff *) _urb->priv);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		return;

	if (!urb->status)
		hdev->stat.byte_tx += urb->transfer_buffer_length;
	else
		hdev->stat.err_tx++;

	read_lock(&husb->completion_lock);

	_urb_unlink(_urb);
	_urb_queue_tail(__completed_q(husb, _urb->type), _urb);

	hci_usb_tx_wakeup(husb);
	
	read_unlock(&husb->completion_lock);
}

static void hci_usb_destruct(struct hci_dev *hdev)
{
	struct hci_usb *husb = (struct hci_usb *) hdev->driver_data;

	BT_DBG("%s", hdev->name);

	kfree(husb);
}

static void *hci_usb_probe(struct usb_device *udev, unsigned int ifnum, const struct usb_device_id *id)
{
	struct usb_endpoint_descriptor *bulk_out_ep[HCI_MAX_IFACE_NUM];
	struct usb_endpoint_descriptor *isoc_out_ep[HCI_MAX_IFACE_NUM];
	struct usb_endpoint_descriptor *bulk_in_ep[HCI_MAX_IFACE_NUM];
	struct usb_endpoint_descriptor *isoc_in_ep[HCI_MAX_IFACE_NUM];
	struct usb_endpoint_descriptor *intr_in_ep[HCI_MAX_IFACE_NUM];
	struct usb_interface_descriptor *uif;
	struct usb_endpoint_descriptor *ep;
	struct usb_interface *iface, *isoc_iface;
	struct hci_usb *husb;
	struct hci_dev *hdev;
	int i, a, e, size, ifn, isoc_ifnum, isoc_alts;

	BT_DBG("udev %p ifnum %d", udev, ifnum);

	iface = &udev->actconfig->interface[0];

	if (!id->driver_info) {
		const struct usb_device_id *match;
		match = usb_match_id(udev, iface, blacklist_ids);
		if (match)
			id = match;
	}

	if (id->driver_info & HCI_IGNORE)
		return NULL;

	/* Check number of endpoints */
	if (udev->actconfig->interface[ifnum].altsetting[0].bNumEndpoints < 3)
		return NULL;

	memset(bulk_out_ep, 0, sizeof(bulk_out_ep));
	memset(isoc_out_ep, 0, sizeof(isoc_out_ep));
	memset(bulk_in_ep,  0, sizeof(bulk_in_ep));
	memset(isoc_in_ep,  0, sizeof(isoc_in_ep));
	memset(intr_in_ep,  0, sizeof(intr_in_ep));

	size = 0; 
	isoc_iface = NULL;
	isoc_alts  = isoc_ifnum = 0;
	
	/* Find endpoints that we need */

	ifn = MIN(udev->actconfig->bNumInterfaces, HCI_MAX_IFACE_NUM);
	for (i = 0; i < ifn; i++) {
		iface = &udev->actconfig->interface[i];
		for (a = 0; a < iface->num_altsetting; a++) {
			uif = &iface->altsetting[a];
			for (e = 0; e < uif->bNumEndpoints; e++) {
				ep = &uif->endpoint[e];

				switch (ep->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) {
				case USB_ENDPOINT_XFER_INT:
					if (ep->bEndpointAddress & USB_DIR_IN)
						intr_in_ep[i] = ep;
					break;

				case USB_ENDPOINT_XFER_BULK:
					if (ep->bEndpointAddress & USB_DIR_IN)
						bulk_in_ep[i]  = ep;
					else
						bulk_out_ep[i] = ep;
					break;

#ifdef CONFIG_BLUEZ_HCIUSB_SCO
				case USB_ENDPOINT_XFER_ISOC:
					if (ep->wMaxPacketSize < size || a > 2)
						break;
					size = ep->wMaxPacketSize;

					isoc_iface = iface;
					isoc_alts  = a;
					isoc_ifnum = i;

					if (ep->bEndpointAddress & USB_DIR_IN)
						isoc_in_ep[i]  = ep;
					else
						isoc_out_ep[i] = ep;
					break;
#endif
				}
			}
		}
	}

	if (!bulk_in_ep[0] || !bulk_out_ep[0] || !intr_in_ep[0]) {
		BT_DBG("Bulk endpoints not found");
		goto done;
	}

#ifdef CONFIG_BLUEZ_HCIUSB_SCO
	if (!isoc_in_ep[1] || !isoc_out_ep[1]) {
		BT_DBG("Isoc endpoints not found");
		isoc_iface = NULL;
	}
#endif

	if (!(husb = kmalloc(sizeof(struct hci_usb), GFP_KERNEL))) {
		BT_ERR("Can't allocate: control structure");
		goto done;
	}

	memset(husb, 0, sizeof(struct hci_usb));

	husb->udev = udev;
	husb->bulk_out_ep = bulk_out_ep[0];
	husb->bulk_in_ep  = bulk_in_ep[0];
	husb->intr_in_ep  = intr_in_ep[0];

	if (id->driver_info & HCI_DIGIANSWER)
		husb->ctrl_req = HCI_DIGI_REQ;
	else
		husb->ctrl_req = HCI_CTRL_REQ;

#ifdef CONFIG_BLUEZ_HCIUSB_SCO
	if (isoc_iface) {
		BT_DBG("isoc ifnum %d alts %d", isoc_ifnum, isoc_alts);
		if (usb_set_interface(udev, isoc_ifnum, isoc_alts)) {
			BT_ERR("Can't set isoc interface settings");
			isoc_iface = NULL;
		}
		usb_driver_claim_interface(&hci_usb_driver, isoc_iface, husb);
		husb->isoc_iface  = isoc_iface;
		husb->isoc_in_ep  = isoc_in_ep[isoc_ifnum];
		husb->isoc_out_ep = isoc_out_ep[isoc_ifnum];
	}
#endif
	
	husb->completion_lock = RW_LOCK_UNLOCKED;

	for (i = 0; i < 4; i++) {	
		skb_queue_head_init(&husb->transmit_q[i]);
		_urb_queue_init(&husb->pending_q[i]);
		_urb_queue_init(&husb->completed_q[i]);
	}

	/* Initialize and register HCI device */
	hdev = &husb->hdev;

	hdev->type  = HCI_USB;
	hdev->driver_data = husb;

	hdev->open  = hci_usb_open;
	hdev->close = hci_usb_close;
	hdev->flush = hci_usb_flush;
	hdev->send  = hci_usb_send_frame;
	hdev->destruct = hci_usb_destruct;

	if (id->driver_info & HCI_RESET)
		set_bit(HCI_QUIRK_RESET_ON_INIT, &hdev->quirks);

	if (hci_register_dev(hdev) < 0) {
		BT_ERR("Can't register HCI device");
		goto probe_error;
	}

	return husb;

probe_error:
	kfree(husb);

done:
	return NULL;
}

static void hci_usb_disconnect(struct usb_device *udev, void *ptr)
{
	struct hci_usb *husb = (struct hci_usb *) ptr;
	struct hci_dev *hdev = &husb->hdev;

	if (!husb)
		return;

	BT_DBG("%s", hdev->name);

	hci_usb_close(hdev);

	if (husb->isoc_iface)
		usb_driver_release_interface(&hci_usb_driver, husb->isoc_iface);

	if (hci_unregister_dev(hdev) < 0)
		BT_ERR("Can't unregister HCI device %s", hdev->name);
}

static struct usb_driver hci_usb_driver = {
	name:           "hci_usb",
	probe:          hci_usb_probe,
	disconnect:     hci_usb_disconnect,
	id_table:       bluetooth_ids,
};

int hci_usb_init(void)
{
	int err;

	BT_INFO("BlueZ HCI USB driver ver %s Copyright (C) 2000,2001 Qualcomm Inc",  
		VERSION);
	BT_INFO("Written 2000,2001 by Maxim Krasnyansky <maxk@qualcomm.com>");

	if ((err = usb_register(&hci_usb_driver)) < 0)
		BT_ERR("Failed to register HCI USB driver");

	return err;
}

void hci_usb_cleanup(void)
{
	usb_deregister(&hci_usb_driver);
}

module_init(hci_usb_init);
module_exit(hci_usb_cleanup);

MODULE_AUTHOR("Maxim Krasnyansky <maxk@qualcomm.com>, Marcel Holtmann <marcel@holtmann.org>");
MODULE_DESCRIPTION("BlueZ HCI USB driver ver " VERSION);
MODULE_LICENSE("GPL");
