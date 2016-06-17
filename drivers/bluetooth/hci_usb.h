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
 * $Id: hci_usb.h,v 1.2 2002/03/18 19:10:04 maxk Exp $
 */

#ifdef __KERNEL__

/* Class, SubClass, and Protocol codes that describe a Bluetooth device */
#define HCI_DEV_CLASS        0xe0	/* Wireless class */
#define HCI_DEV_SUBCLASS     0x01	/* RF subclass */
#define HCI_DEV_PROTOCOL     0x01	/* Bluetooth programming protocol */

#define HCI_CTRL_REQ	     0x20
#define HCI_DIGI_REQ	     0x40

#define HCI_IGNORE           0x01
#define HCI_RESET            0x02
#define HCI_DIGIANSWER       0x04

#define HCI_MAX_IFACE_NUM	3 

#define HCI_MAX_BULK_TX     	4
#define HCI_MAX_BULK_RX     	1

#define HCI_MAX_ISOC_RX		2
#define HCI_MAX_ISOC_TX		2

#define HCI_MAX_ISOC_FRAMES     10

struct _urb_queue {
	struct list_head head;
	spinlock_t       lock;
};

struct _urb {
	struct list_head  list;
	struct _urb_queue *queue;
	int               type;
	void              *priv;
	struct urb        urb;
};

struct _urb *_urb_alloc(int isoc, int gfp);

static inline void _urb_free(struct _urb *_urb)
{
	kfree(_urb);
}

static inline void _urb_queue_init(struct _urb_queue *q)
{
	INIT_LIST_HEAD(&q->head);
	spin_lock_init(&q->lock);
}

static inline void _urb_queue_head(struct _urb_queue *q, struct _urb *_urb)
{
        unsigned long flags;
        spin_lock_irqsave(&q->lock, flags);
	list_add(&_urb->list, &q->head); _urb->queue = q;
	spin_unlock_irqrestore(&q->lock, flags);
}

static inline void _urb_queue_tail(struct _urb_queue *q, struct _urb *_urb)
{
        unsigned long flags;
        spin_lock_irqsave(&q->lock, flags);
	list_add_tail(&_urb->list, &q->head); _urb->queue = q;
	spin_unlock_irqrestore(&q->lock, flags);
}

static inline void _urb_unlink(struct _urb *_urb)
{
	struct _urb_queue *q = _urb->queue;
        unsigned long flags;
	if (q) {
        	spin_lock_irqsave(&q->lock, flags);
		list_del(&_urb->list); _urb->queue = NULL;
		spin_unlock_irqrestore(&q->lock, flags);
	}
}

struct _urb *_urb_dequeue(struct _urb_queue *q);

#ifndef container_of
#define container_of(ptr, type, member) ({                      \
		        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
			        (type *)( (char *)__mptr - offsetof(type,member) );})
#endif

struct hci_usb {
	struct hci_dev		hdev;

	unsigned long		state;
	
	struct usb_device 	*udev;
	
	struct usb_endpoint_descriptor	*bulk_in_ep;
	struct usb_endpoint_descriptor	*bulk_out_ep;
	struct usb_endpoint_descriptor	*intr_in_ep;

	struct usb_interface            *isoc_iface;
	struct usb_endpoint_descriptor	*isoc_out_ep;
	struct usb_endpoint_descriptor	*isoc_in_ep;

	__u8			ctrl_req;

	struct sk_buff_head	transmit_q[4];
	struct sk_buff		*reassembly[4]; // Reassembly buffers

	rwlock_t		completion_lock;

	atomic_t		pending_tx[4];  // Number of pending requests 
	struct _urb_queue	pending_q[4];   // Pending requests
	struct _urb_queue	completed_q[4]; // Completed requests
};

/* States  */
#define HCI_USB_TX_PROCESS	1
#define HCI_USB_TX_WAKEUP	2

#endif /* __KERNEL__ */
