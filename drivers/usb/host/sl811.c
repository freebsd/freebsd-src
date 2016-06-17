/*
 * SL811 Host Controller Interface driver for USB.
 *
 * Copyright (c) 2003/06, Courage Co., Ltd.
 *
 * Based on:
 *	1.uhci.c by Linus Torvalds, Johannes Erdfelt, Randy Dunlap,
 * 	  Georg Acher, Deti Fliegl, Thomas Sailer, Roman Weissgaerber,
 * 	  Adam Richter, Gregory P. Smith;
 * 	2.Original SL811 driver (hc_sl811.o) by Pei Liu <pbl@cypress.com>
 *	3.Rewrited as sl811.o by Yin Aihua <yinah:couragetech.com.cn>
 *
 * It's now support isochornous mode and more effective than hc_sl811.o
 * Support x86 architecture now.
 *
 * 19.09.2003 (05.06.2003) HNE
 * sl811_alloc_hc: Set "bus->bus_name" at init.
 * sl811_reg_test (hc_reset,regTest):
 *   Stop output at first failed pattern.
 * Down-Grade for Kernel 2.4.20 and from 2.4.22
 * Splitt hardware depens into file sl811-x86.h and sl811-arm.h.
 *
 * 22.09.2003 HNE
 * sl811_found_hc: First patterntest, than interrupt enable.
 * Do nothing, if patterntest failed. Release io, if failed.
 * Stop Interrupts first, than remove handle. (Old blocked Shred IRQ)
 * Alternate IO-Base for second Controller (CF/USB1).
 *
 * 24.09.2003 HNE
 * Remove all arm specific source (moved into include/asm/sl811-hw.h).
 *
 * 03.10.2003 HNE
 * Low level only for port io into hardware-include.

 *
 * To do:
 *	1.Modify the timeout part, it's some messy
 *	2.Use usb-a and usb-b set in Ping-Pong mode
 *	o Floppy do not work.
 *	o driver crash, if io region can't register
 * 	o Only as module tested! Compiled in Version not tested!

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/smp_lock.h>
#include <linux/list.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <linux/irq.h>
#include <linux/usb.h>

#include "../hcd.h"
#include "../hub.h"
#include "sl811.h"

#define DRIVER_VERSION "v0.30"
#define MODNAME "SL811"
#define DRIVER_AUTHOR "Yin Aihua <yinah@couragetech.com.cn>, Henry Nestler <hne@ist1.de>"
#define DRIVER_DESC "Sl811 USB Host Controller Alternate Driver"

static LIST_HEAD(sl811_hcd_list);

/*
 * 0, normal prompt and information
 * 1, error should not occur in normal
 * 2, error maybe occur in normal
 * 3, useful and detail debug information
 * 4, function level enter and level inforamtion
 * 5, endless information will output because of timer function or interrupt
 */
static int debug = 0;
MODULE_PARM(debug,"i");
MODULE_PARM_DESC(debug,"debug level");

#include <asm/sl811-hw.h>	/* Include hardware and board depens */

static void sl811_rh_int_timer_do(unsigned long ptr);
static void sl811_transfer_done(struct sl811_hc *hc, int sof);

/*
 * Read	a byte of data from the	SL811H/SL11H
 */
static __u8 inline sl811_read(struct sl811_hc *hc, __u8 offset)
{
	sl811_write_index (hc, offset);
	return (sl811_read_data (hc));
}

/*
 * Write a byte	of data	to the SL811H/SL11H
 */
static void inline sl811_write(struct sl811_hc *hc, __u8 offset, __u8 data)
{
	sl811_write_index_data (hc, offset, data);
}

/*
 * Read	consecutive bytes of data from the SL811H/SL11H	buffer
 */
static void inline sl811_read_buf(struct sl811_hc *hc, __u8 offset, __u8 *buf, __u8 size)
{
	sl811_write_index (hc, offset);
	while (size--) {
		*buf++ = sl811_read_data(hc);
	}
}

/*
 * Write consecutive bytes of data to the SL811H/SL11H buffer
 */
static void inline sl811_write_buf(struct sl811_hc *hc, __u8 offset, __u8 *buf, __u8 size)
{
	sl811_write_index (hc, offset);
	while (size--) {
		sl811_write_data (hc, *buf);
		buf++;
	}
}

/*
 * This	routine	test the Read/Write functionality of SL811HS registers
 */
static int sl811_reg_test(struct sl811_hc *hc)
{
	int i, data, result = 0;
	__u8 buf[256];

	for (i = 0x10; i < 256;	i++) {
		/* save	the original buffer */
		buf[i] = sl811_read(hc,	i);

		/* Write the new data to the buffer */
		sl811_write(hc,	i, i);
	}

	/* compare the written data */
	for (i = 0x10; i < 256;	i++) {
		data = sl811_read(hc, i);
		if (data != i) {
			PDEBUG(1, "Pattern test failed!! value = 0x%x, s/b 0x%x", data, i);
			result = -1;

			/* If no Debug, show only first failed Address */
			if (!debug)
			    break;
		}
	}

	/* restore the data */
	for (i = 0x10; i < 256;	i++)
		sl811_write(hc,	i, buf[i]);

	return result;
}

/*
 * Display all SL811HS register	values
 */
#if 0 /* unused (hne) */
static void sl811_reg_show(struct sl811_hc *hc)
{
	int i;

	for (i = 0; i <	256; i++)
		PDEBUG(4, "offset %d: 0x%x", i, sl811_read(hc, i));
}
#endif

/*
 * This	function enables SL811HS interrupts
 */
static void sl811_enable_interrupt(struct sl811_hc *hc)
{
	PDEBUG(4, "enter");
	sl811_write(hc, SL811_INTR, SL811_INTR_DONE_A | SL811_INTR_SOF | SL811_INTR_INSRMV);
}

/*
 * This	function disables SL811HS interrupts
 */
static void sl811_disable_interrupt(struct sl811_hc *hc)
{
	PDEBUG(4, "enter");
	// Disable all other interrupt except for insert/remove.
	sl811_write(hc,	SL811_INTR, SL811_INTR_INSRMV);
}

/*
 * SL811 Virtual Root Hub
 */

/* Device descriptor */
static __u8 sl811_rh_dev_des[] =
{
	0x12,       /*  __u8  bLength; */
	0x01,       /*  __u8  bDescriptorType; Device */
	0x10,	    /*  __u16 bcdUSB; v1.1 */
	0x01,
	0x09,	    /*  __u8  bDeviceClass; HUB_CLASSCODE */
	0x00,	    /*  __u8  bDeviceSubClass; */
	0x00,       /*  __u8  bDeviceProtocol; */
	0x08,       /*  __u8  bMaxPacketSize0; 8 Bytes */
	0x00,       /*  __u16 idVendor; */
	0x00,
	0x00,       /*  __u16 idProduct; */
 	0x00,
	0x00,       /*  __u16 bcdDevice; */
 	0x00,
	0x00,       /*  __u8  iManufacturer; */
	0x02,       /*  __u8  iProduct; */
	0x01,       /*  __u8  iSerialNumber; */
	0x01        /*  __u8  bNumConfigurations; */
};

/* Configuration descriptor */
static __u8 sl811_rh_config_des[] =
{
	0x09,       /*  __u8  bLength; */
	0x02,       /*  __u8  bDescriptorType; Configuration */
	0x19,       /*  __u16 wTotalLength; */
	0x00,
	0x01,       /*  __u8  bNumInterfaces; */
	0x01,       /*  __u8  bConfigurationValue; */
	0x00,       /*  __u8  iConfiguration; */
	0x40,       /*  __u8  bmAttributes;
                    Bit 7: Bus-powered, 6: Self-powered, 5 Remote-wakwup,
                    4..0: resvd */
	0x00,       /*  __u8  MaxPower; */

	/* interface */
	0x09,       /*  __u8  if_bLength; */
	0x04,       /*  __u8  if_bDescriptorType; Interface */
	0x00,       /*  __u8  if_bInterfaceNumber; */
	0x00,       /*  __u8  if_bAlternateSetting; */
	0x01,       /*  __u8  if_bNumEndpoints; */
	0x09,       /*  __u8  if_bInterfaceClass; HUB_CLASSCODE */
	0x00,       /*  __u8  if_bInterfaceSubClass; */
	0x00,       /*  __u8  if_bInterfaceProtocol; */
	0x00,       /*  __u8  if_iInterface; */

	/* endpoint */
	0x07,       /*  __u8  ep_bLength; */
	0x05,       /*  __u8  ep_bDescriptorType; Endpoint */
	0x81,       /*  __u8  ep_bEndpointAddress; IN Endpoint 1 */
 	0x03,       /*  __u8  ep_bmAttributes; Interrupt */
 	0x08,       /*  __u16 ep_wMaxPacketSize; */
 	0x00,
	0xff        /*  __u8  ep_bInterval; 255 ms */
};

/* root hub class descriptor*/
static __u8 sl811_rh_hub_des[] =
{
	0x09,			/*  __u8  bLength; */
	0x29,			/*  __u8  bDescriptorType; Hub-descriptor */
	0x01,			/*  __u8  bNbrPorts; */
	0x00,			/* __u16  wHubCharacteristics; */
	0x00,
	0x50,			/*  __u8  bPwrOn2pwrGood; 2ms */
	0x00,			/*  __u8  bHubContrCurrent; 0 mA */
	0xfc,			/*  __u8  DeviceRemovable; *** 7 Ports max *** */
	0xff			/*  __u8  PortPwrCtrlMask; *** 7 ports max *** */
};

/*
 * This function examine the port change in the virtual root hub. HUB INTERRUPT ENDPOINT.
 */
static int sl811_rh_send_irq(struct sl811_hc *hc, __u8 *rh_change, int rh_len)
{
	__u8 data = 0;
		
	PDEBUG(5, "enter");

	/*
	 * Right now, It is assume the power is good and no changes and only one port.
	 */
	if (hc->rh_status.wPortChange & (USB_PORT_STAT_CONNECTION | USB_PORT_STAT_ENABLE)) {  
		data = 1<<1;
		*(__u8 *)rh_change = data;
		return 1;
	} else
		return 0;
}

/*
 * This function creates a timer that act as interrupt pipe in the virtual hub.
 *
 * Note:  The virtual root hub's interrupt pipe are polled by the timer
 *        every "interval" ms
 */
static void sl811_rh_init_int_timer(struct urb * urb)
{
	 struct sl811_hc *hc = urb->dev->bus->hcpriv;
	 hc->rh.interval = urb->interval;

	 init_timer(&hc->rh.rh_int_timer);
	 hc->rh.rh_int_timer.function = sl811_rh_int_timer_do;
	 hc->rh.rh_int_timer.data = (unsigned long)urb;
	 hc->rh.rh_int_timer.expires = jiffies +
		(HZ * (urb->interval < 30? 30: urb->interval)) / 1000;
	 add_timer (&hc->rh.rh_int_timer);
}

/*
 * This function is called when the timer expires.  It gets the the port
 * change data and pass along to the upper protocol.
 */
static void sl811_rh_int_timer_do(unsigned long ptr)
{
	int len;
	struct urb *urb = (struct urb *)ptr;
	struct sl811_hc *hc = urb->dev->bus->hcpriv;
	PDEBUG (5, "enter");

	if(hc->rh.send) {
		len = sl811_rh_send_irq(hc, urb->transfer_buffer,
			urb->transfer_buffer_length);
		if (len > 0) {
			urb->actual_length = len;
			if (urb->complete)
				urb->complete(urb);
		}
	}

#ifdef SL811_TIMEOUT
	
{
	struct list_head *head, *tmp;
	struct sl811_urb_priv *urbp;
	struct urb *u;
	int i;
	static int timeout_count = 0;

// check time out every second
	if (++timeout_count > 4) {
		int max_scan = hc->active_urbs;
		timeout_count = 0;
		for (i = 0; i < 6; ++i) {
			head = &hc->urb_list[i];
			tmp = head->next;
			while (tmp != head && max_scan--) {
				u = list_entry(tmp, struct urb, urb_list);
				urbp = (struct sl811_urb_priv *)u->hcpriv;
				tmp = tmp->next;
				// Check if the URB timed out
				if (u->timeout && time_after_eq(jiffies, urbp->inserttime + u->timeout)) {
					PDEBUG(3, "urb = %p time out, we kill it", urb);
					u->transfer_flags |= USB_TIMEOUT_KILLED;
				}
			}
		}
	}
}

#endif
	// re-activate the timer
	sl811_rh_init_int_timer(urb);
}

/* helper macro */
#define OK(x)	len = (x); break

/*
 * This function handles all USB request to the the virtual root hub
 */
static int sl811_rh_submit_urb(struct urb *urb)
{
	struct usb_device *usb_dev = urb->dev;
	struct sl811_hc *hc = usb_dev->bus->hcpriv;
	struct usb_ctrlrequest *cmd = (struct usb_ctrlrequest *)urb->setup_packet;
	void *data = urb->transfer_buffer;
	int buf_len = urb->transfer_buffer_length;
	unsigned int pipe = urb->pipe;
	__u8 data_buf[16];
	__u8 *bufp = data_buf;
	int len = 0;
	int status = 0;
	
	__u16 bmRType_bReq;
	__u16 wValue;
	__u16 wIndex;
	__u16 wLength;

	if (usb_pipeint(pipe)) {
		hc->rh.urb =  urb;
		hc->rh.send = 1;
		hc->rh.interval = urb->interval;
		sl811_rh_init_int_timer(urb);
		urb->status = 0;

		return 0;
	}

	bmRType_bReq  = cmd->bRequestType | (cmd->bRequest << 8);
	wValue        = le16_to_cpu (cmd->wValue);
	wIndex        = le16_to_cpu (cmd->wIndex);
	wLength       = le16_to_cpu (cmd->wLength);

	PDEBUG(5, "submit rh urb, req = %d(%x) len=%d", bmRType_bReq, bmRType_bReq, wLength);

	/* Request Destination:
		   without flags: Device,
		   USB_RECIP_INTERFACE: interface,
		   USB_RECIP_ENDPOINT: endpoint,
		   USB_TYPE_CLASS means HUB here,
		   USB_RECIP_OTHER | USB_TYPE_CLASS  almost ever means HUB_PORT here
	*/
	switch (bmRType_bReq) {
	case RH_GET_STATUS:
		*(__u16 *)bufp = cpu_to_le16(1);
		OK(2);

	case RH_GET_STATUS | USB_RECIP_INTERFACE:
		*(__u16 *)bufp = cpu_to_le16(0);
		OK(2);

	case RH_GET_STATUS | USB_RECIP_ENDPOINT:
		*(__u16 *)bufp = cpu_to_le16(0);
		OK(2);

	case RH_GET_STATUS | USB_TYPE_CLASS:
		*(__u32 *)bufp = cpu_to_le32(0);
		OK(4);

	case RH_GET_STATUS | USB_RECIP_OTHER | USB_TYPE_CLASS:
		*(__u32 *)bufp = cpu_to_le32(hc->rh_status.wPortChange<<16 | hc->rh_status.wPortStatus);
		OK(4);

	case RH_CLEAR_FEATURE | USB_RECIP_ENDPOINT:
		switch (wValue)	{
		case 1: 
			OK(0);
		}
		break;

	case RH_CLEAR_FEATURE | USB_TYPE_CLASS:
		switch (wValue) {
		case C_HUB_LOCAL_POWER:
			OK(0);

		case C_HUB_OVER_CURRENT:
			OK(0);
		}
		break;

	case RH_CLEAR_FEATURE | USB_RECIP_OTHER | USB_TYPE_CLASS:
		switch (wValue) {
		case USB_PORT_FEAT_ENABLE:
			hc->rh_status.wPortStatus &= ~USB_PORT_STAT_ENABLE;
			OK(0);

		case USB_PORT_FEAT_SUSPEND:
			hc->rh_status.wPortStatus &= ~USB_PORT_STAT_SUSPEND;
			OK(0);

		case USB_PORT_FEAT_POWER:
			hc->rh_status.wPortStatus &= ~USB_PORT_STAT_POWER;
			OK(0);

		case USB_PORT_FEAT_C_CONNECTION:
			hc->rh_status.wPortChange &= ~USB_PORT_STAT_C_CONNECTION;
			OK(0);

		case USB_PORT_FEAT_C_ENABLE:
			hc->rh_status.wPortChange &= ~USB_PORT_STAT_C_ENABLE;
			OK(0);

		case USB_PORT_FEAT_C_SUSPEND:
			hc->rh_status.wPortChange &= ~USB_PORT_STAT_C_SUSPEND;
			OK(0);

		case USB_PORT_FEAT_C_OVER_CURRENT:
			hc->rh_status.wPortChange &= ~USB_PORT_STAT_C_OVERCURRENT;
			OK(0);

		case USB_PORT_FEAT_C_RESET:
			hc->rh_status.wPortChange &= ~USB_PORT_STAT_C_RESET;
			OK(0);
		}
		break;

	case RH_SET_FEATURE | USB_RECIP_OTHER | USB_TYPE_CLASS:
		switch (wValue) {
		case USB_PORT_FEAT_SUSPEND:
			hc->rh_status.wPortStatus |= USB_PORT_STAT_SUSPEND;
			OK(0);

		case USB_PORT_FEAT_RESET:
			hc->rh_status.wPortStatus |= USB_PORT_STAT_RESET;
			hc->rh_status.wPortChange = 0;
			hc->rh_status.wPortChange |= USB_PORT_STAT_C_RESET;
			hc->rh_status.wPortStatus &= ~USB_PORT_STAT_RESET;
			hc->rh_status.wPortStatus |= USB_PORT_STAT_ENABLE;
			OK(0);

		case USB_PORT_FEAT_POWER:
			hc->rh_status.wPortStatus |= USB_PORT_STAT_POWER;
			OK(0);

		case USB_PORT_FEAT_ENABLE:
			hc->rh_status.wPortStatus |= USB_PORT_STAT_ENABLE;
			OK(0);
		}
		break;

	case RH_SET_ADDRESS:
		hc->rh.devnum = wValue;
		OK(0);

	case RH_GET_DESCRIPTOR:
		switch ((wValue & 0xff00) >> 8) {
		case USB_DT_DEVICE:
			len = sizeof(sl811_rh_dev_des);
			bufp = sl811_rh_dev_des;
			OK(len);

		case USB_DT_CONFIG: 
			len = sizeof(sl811_rh_config_des);
			bufp = sl811_rh_config_des;
			OK(len);

		case USB_DT_STRING:
			len = usb_root_hub_string(wValue & 0xff, (int)(long)0,	"SL811HS", data, wLength);
			if (len > 0) {
				bufp = data;
				OK(len);
			}
		
		default:
			status = -EPIPE;
		}
		break;

	case RH_GET_DESCRIPTOR | USB_TYPE_CLASS:
		len = sizeof(sl811_rh_hub_des);
		bufp = sl811_rh_hub_des;
		OK(len);

	case RH_GET_CONFIGURATION:
		bufp[0] = 0x01;
		OK(1);

	case RH_SET_CONFIGURATION:
		OK(0);

	default:
		PDEBUG(1, "unsupported root hub command");
		status = -EPIPE;
	}

	len = min(len, buf_len);
	if (data != bufp)
		memcpy(data, bufp, len);
	urb->actual_length = len;
	urb->status = status;

	PDEBUG(5, "len = %d, status = %d", len, status);
	
	urb->hcpriv = NULL;
	urb->dev = NULL;
	if (urb->complete)
		urb->complete(urb);

	return 0;
}

/*
 * This function unlinks the URB
 */
static int sl811_rh_unlink_urb(struct urb *urb)
{
	struct sl811_hc *hc = urb->dev->bus->hcpriv;

	PDEBUG(5, "enter");
	
	if (hc->rh.urb == urb) {
		hc->rh.send = 0;
		del_timer(&hc->rh.rh_int_timer);
		hc->rh.urb = NULL;
		urb->hcpriv = NULL;
		usb_dec_dev_use(urb->dev);
		urb->dev = NULL;
		if (urb->transfer_flags & USB_ASYNC_UNLINK) {
			urb->status = -ECONNRESET;
			if (urb->complete)
				urb->complete(urb);
		} else
			urb->status = -ENOENT;
	}

	return 0;
}

/*
 * This function connect the virtual root hub to the USB stack
 */
static int sl811_connect_rh(struct sl811_hc * hc)
{
	struct usb_device *usb_dev;

	hc->rh.devnum = 0;
	usb_dev = usb_alloc_dev(NULL, hc->bus);
	if (!usb_dev)
		return -ENOMEM;

	hc->bus->root_hub = usb_dev;
	usb_connect(usb_dev);

	if (usb_new_device(usb_dev)) {
		usb_free_dev(usb_dev);
		return -ENODEV;
	}
	
	PDEBUG(5, "leave success");
	
	return 0;
}

/*
 * This function allocates private data space for the usb device
 */
static int sl811_alloc_dev_priv(struct usb_device *usb_dev)
{
	return 0;
}

/*
 * This function de-allocates private data space for the usb devic
 */
static int sl811_free_dev_priv (struct usb_device *usb_dev)
{
	return 0;
}

/*
 * This function allocates private data space for the urb
 */
static struct sl811_urb_priv* sl811_alloc_urb_priv(struct urb *urb)
{
	struct sl811_urb_priv *urbp;
	
	urbp = kmalloc(sizeof(*urbp), GFP_ATOMIC);
	if (!urbp)
		return NULL;
	
	memset(urbp, 0, sizeof(*urbp));
	
	INIT_LIST_HEAD(&urbp->td_list);
	
	urbp->urb = urb;
	urb->hcpriv = urbp;
	
	return urbp;
}

/*
 * This function free private data space for the urb
 */
static void sl811_free_urb_priv(struct urb *urb)
{
	struct sl811_urb_priv *urbp = urb->hcpriv;
	struct sl811_td *td;
	struct list_head *head, *tmp;
	
	if (!urbp)
		return ;
	
	head = &urbp->td_list;
	tmp = head->next;
	
	while (tmp != head) {
		td = list_entry(tmp, struct sl811_td, td_list);
		tmp = tmp->next;
		kfree(td);
	}
	
	kfree(urbp);
	urb->hcpriv = NULL;
	
	return ;
}

/*
 * This	function calculate the bus time need by this td.
 * Fix me! Can this use usb_calc_bus_time()?
 */
static void sl811_calc_td_time(struct sl811_td *td)
{
#if 1
	int time;
	int len = td->len;
	struct sl811_hc *hc = td->urb->dev->bus->hcpriv; 

	if (hc->rh_status.wPortStatus & USB_PORT_STAT_LOW_SPEED)
		time = 8*8*len + 1024;
	else {
		if (td->ctrl & SL811_USB_CTRL_PREAMBLE)
			time = 8*8*len + 2048;
		else
			time = 8*len + 256;
	}

	time += 2*10 * len;

	td->bustime = time;
	
#else

	unsigned long tmp;
	int time;
	int low_speed = usb_pipeslow(td->urb->pipe);
	int input_dir = usb_pipein(td->urb->pipe);
	int bytecount = td->len;
	int isoc = usb_pipeisoc(td->urb->pipe); 

	if (low_speed) {	/* no isoc. here */
		if (input_dir) {
			tmp = (67667L * (31L + 10L * BitTime (bytecount))) / 1000L;
			time =  (64060L + (2 * BW_HUB_LS_SETUP) + BW_HOST_DELAY + tmp);
		} else {
			tmp = (66700L * (31L + 10L * BitTime (bytecount))) / 1000L;
			time =  (64107L + (2 * BW_HUB_LS_SETUP) + BW_HOST_DELAY + tmp);
		}
	} else if (!isoc){	/* for full-speed: */
		tmp = (8354L * (31L + 10L * BitTime (bytecount))) / 1000L;
		time = (9107L + BW_HOST_DELAY + tmp);
	} else {		/* for isoc: */
		tmp = (8354L * (31L + 10L * BitTime (bytecount))) / 1000L;
		time =  (((input_dir) ? 7268L : 6265L) + BW_HOST_DELAY + tmp);
	}
	
	td->bustime = time / 84;

#endif		 
}

/*
 * This	function calculate the remainder bus time in current frame.
 */
static inline int sl811_calc_bus_remainder(struct sl811_hc *hc)
{
	return (sl811_read(hc, SL811_SOFCNTDIV) * 64);
}

/*
 * This function allocates td for the urb
 */
static struct sl811_td* sl811_alloc_td(struct urb *urb)
{
	struct sl811_urb_priv *urbp = urb->hcpriv;
	struct sl811_td *td;
	
	td = kmalloc(sizeof (*td), GFP_ATOMIC);
	if (!td)
		return NULL;
	
	memset(td, 0, sizeof(*td));
	
	INIT_LIST_HEAD(&td->td_list);
	
	td->urb = urb;
	list_add_tail(&td->td_list, &urbp->td_list);
	
	return td;
}

/*
 * Fill the td.
 */
static inline void sl811_fill_td(struct sl811_td *td, __u8 ctrl, __u8 addr, __u8 len, __u8 pidep, __u8 dev, __u8 *buf)
{
	td->ctrl = ctrl;
	td->addr = addr;
	td->len = len;
	td->pidep = pidep;
	td->dev = dev;
	td->buf = buf;
	td->left = len;
	td->errcnt = 3;
}

/*
 * Fill the td.
 */
static inline void sl811_reset_td(struct sl811_td *td)
{
	td->status = 0;
	td->left = td->len;
	td->done = 0;
	td->errcnt = 3;
	td->nakcnt = 0;
	td->td_status = 0;
}

static void sl811_print_td(int level, struct sl811_td *td)
{
	 PDEBUG(level, "td = %p, ctrl = %x, addr = %x, len = %x, pidep = %x\n "
		"dev = %x, status = %x, left = %x, errcnt = %x, done = %x\n "
		"buf = %p, bustime = %d, td_status = %d\n", 
		td, td->ctrl, td->addr, td->len, td->pidep,
		td->dev, td->status, td->left, td->errcnt, td->done,
		td->buf, td->bustime, td->td_status);
}

/*
 * Isochronous transfers
 */
static int sl811_submit_isochronous(struct urb *urb)
{
	__u8 dev = usb_pipedevice(urb->pipe);
	__u8 pidep = PIDEP(usb_packetid(urb->pipe), usb_pipeendpoint(urb->pipe));
	__u8 ctrl = 0;
	struct sl811_urb_priv *urbp = urb->hcpriv;
	struct sl811_td *td = NULL;
	int i;
	
	PDEBUG(4, "enter, urb = %p, urbp = %p", urb, urbp);
	
	/* Can't have low speed bulk transfers */
	if (usb_pipeslow(urb->pipe)) {
		PDEBUG(1, "error, urb = %p, low speed device", urb);
		return -EINVAL;
	}
	
	if (usb_pipeout(urb->pipe))
		ctrl |= SL811_USB_CTRL_DIR_OUT;
		
	ctrl |= SL811_USB_CTRL_ARM | SL811_USB_CTRL_ENABLE | SL811_USB_CTRL_ISO;
	
	for (i = 0; i < urb->number_of_packets; i++) {
		urb->iso_frame_desc[i].actual_length = 0;
		urb->iso_frame_desc[i].status = -EXDEV;
			
		td = sl811_alloc_td(urb);
		if (!td)
			return -ENOMEM;

		sl811_fill_td(td, ctrl, SL811_DATA_START, 
			urb->iso_frame_desc[i].length,
			pidep, dev,
			urb->transfer_buffer + urb->iso_frame_desc[i].offset);
		sl811_calc_td_time(td);
		if (urbp->cur_td == NULL)
			urbp->cur_td = urbp->first_td = td;	
	}

	urbp->last_td = td;	
	
	PDEBUG(4, "leave success");

/*	
// for debug
	{
		struct list_head *head, *tmp;
		struct sl811_td *td;
		int i = 0;
		head = &urbp->td_list;
		tmp = head->next;
	
		if (list_empty(&urbp->td_list)) {
			PDEBUG(1, "bug!!! td list is empty!");
			return -ENODEV;
		}
		
		while (tmp != head) {
			++i;
			td = list_entry(tmp, struct sl811_td, td_list);
			PDEBUG(2, "td = %p, i = %d", td, i);
			tmp = tmp->next;
		}
	}
*/	
	return 0;
}

/*
 * Reset isochronous transfers
 */
static void sl811_reset_isochronous(struct urb *urb)
{
	struct sl811_urb_priv *urbp = urb->hcpriv;
	struct sl811_td *td = NULL;
	struct list_head *head, *tmp;
	int i;

	PDEBUG(4, "enter, urb = %p", urb);
	
	for (i = 0; i < urb->number_of_packets; i++) {
		urb->iso_frame_desc[i].actual_length = 0;
		urb->iso_frame_desc[i].status = -EXDEV;
	}

	head = &urbp->td_list;
	tmp = head->next;
	while (tmp != head) {
		td = list_entry(tmp, struct sl811_td, td_list);
		tmp = tmp->next;
		sl811_reset_td(td);
	}
	
	urbp->cur_td = urbp->first_td;
	
	urb->status = -EINPROGRESS;
	urb->actual_length = 0;
	urb->error_count = 0;
}

/*
 * Result the iso urb.
 */
static void sl811_result_isochronous(struct urb *urb)
{
	struct list_head *tmp, *head;
	struct sl811_urb_priv *urbp = urb->hcpriv;
	int status = 0;
	struct sl811_td *td;
	int i;

	PDEBUG(4, "enter, urb = %p", urb);
		
	urb->actual_length = 0;

	i = 0;
	head = &urbp->td_list;
	tmp = head->next;
	while (tmp != head) {
		td = list_entry(tmp, struct sl811_td, td_list);
		tmp = tmp->next;
		
		if (!td->done) {
			if (urbp->unlink)
				urb->status = -ENOENT;
			else {
				PDEBUG(1, "we should not get here!");
				urb->status = -EXDEV;
			}
			return ;	
		}
		if (td->td_status) {
			status = td->td_status;
			urb->error_count++;
			PDEBUG(1, "error: td = %p, td status = %d", td, td->td_status);
		}

		urb->iso_frame_desc[i].actual_length = td->len - td->left;
		urb->actual_length += td->len - td->left;
		urb->iso_frame_desc[i].status = td->td_status;
		++i;
		if (td->left)
			PDEBUG(3, "short packet, td = %p, len = %d, left = %d", td, td->len, td->left);
	}

	urb->status = status;
/*
// for debug
	PDEBUG(2, "iso urb complete, len = %d, status =%d ", urb->actual_length, urb->status);		
*/
	PDEBUG(4, "leave success");
}

/*
 * Interrupt transfers
 */
static int sl811_submit_interrupt(struct urb *urb)
{
	int maxsze = usb_maxpacket(urb->dev, urb->pipe, usb_pipeout(urb->pipe));
	int len = urb->transfer_buffer_length;
	__u8 *data = urb->transfer_buffer;
	__u8 dev = usb_pipedevice(urb->pipe);
	__u8 pidep = PIDEP(usb_packetid(urb->pipe), usb_pipeendpoint(urb->pipe));
	__u8 ctrl = 0;
	struct sl811_hc *hc = urb->dev->bus->hcpriv;
	struct sl811_urb_priv *urbp = urb->hcpriv;
	struct sl811_td *td = NULL;
	
	PDEBUG(4, "enter, urb = %p", urb);
	
	if (len > maxsze) {
		PDEBUG(1, "length is big than max packet size, len = %d, max packet = %d", len, maxsze);
		return -EINVAL;
	}
	if (usb_pipeslow(urb->pipe) && !(hc->rh_status.wPortStatus & USB_PORT_STAT_LOW_SPEED))
		ctrl |= SL811_USB_CTRL_PREAMBLE;
	
	ctrl |= SL811_USB_CTRL_ARM | SL811_USB_CTRL_ENABLE;
	if (usb_gettoggle(urb->dev, usb_pipeendpoint(urb->pipe), usb_pipeout(urb->pipe)))
		ctrl |= SL811_USB_CTRL_TOGGLE_1;
	usb_dotoggle(urb->dev, usb_pipeendpoint(urb->pipe), usb_pipeout(urb->pipe));	
	td = sl811_alloc_td(urb);
	if (!td)
		return -ENOMEM;
		
	sl811_fill_td(td, ctrl, SL811_DATA_START, len, pidep, dev, data);
	sl811_calc_td_time(td);
	urbp->cur_td = urbp->first_td = urbp->last_td = td;
	urbp->interval = 0;
	
	PDEBUG(4, "leave success");
	
	return 0;
}

/*
 * Reset interrupt transfers
 */
static void sl811_reset_interrupt(struct urb *urb)
{
	struct sl811_urb_priv *urbp = urb->hcpriv;
	struct sl811_td *td = urbp->cur_td;
	
	PDEBUG(4, "enter, interval = %d", urb->interval);
	
	td->ctrl &= ~SL811_USB_CTRL_TOGGLE_1;
	if (usb_gettoggle(urb->dev, usb_pipeendpoint(urb->pipe), usb_pipeout(urb->pipe)))
		td->ctrl |= SL811_USB_CTRL_TOGGLE_1;
	usb_dotoggle(urb->dev, usb_pipeendpoint(urb->pipe), usb_pipeout(urb->pipe));	
	
	sl811_reset_td(td);

	urbp->interval = urb->interval;
	
	urb->status = -EINPROGRESS;
	urb->actual_length = 0;
}

/*
 * Result the interrupt urb.
 */
static void sl811_result_interrupt(struct urb *urb)
{
	struct list_head *tmp;
	struct sl811_urb_priv *urbp = urb->hcpriv;
	struct sl811_td *td;
	int toggle;
	
	PDEBUG(4, "enter, urb = %p", urb);
	
	urb->actual_length = 0;

	tmp = &urbp->td_list;
	tmp = tmp->next;
	td = list_entry(tmp, struct sl811_td, td_list);

	// success.
	if (td->done && td->td_status == 0) {
		urb->actual_length += td->len - td->left;
		urb->status = 0;
		return ;
	}
	// tranfer is done but fail, reset the toggle.
	else if (td->done && td->td_status) {
		urb->status = td->td_status;
reset_toggle:
		toggle = (td->ctrl & SL811_USB_CTRL_TOGGLE_1) ? 1 : 0;
		usb_settoggle(urb->dev, usb_pipeendpoint(urb->pipe), usb_pipeout(urb->pipe), toggle);
		PDEBUG(3, "error: td = %p, td status = %d", td, td->td_status);
		return ;
	}
	// unlink, and not do transfer yet
	else if (td->done == 0 && urbp->unlink && td->td_status == 0) {
		urb->status = -ENOENT;
		PDEBUG(3, "unlink and not transfer!");
		return ;
	}
	// unlink, and transfer not complete yet.
	else if (td->done == 0 && urbp->unlink && td->td_status) {
		urb->status = -ENOENT;
		PDEBUG(3, "unlink and not complete!");
		goto reset_toggle;
	}
	// must be bug!!!
	else {// (td->done == 0 && urbp->unlink == 0)
		PDEBUG(1, "we should not get here!");
		urb->status = -EPIPE;
		return ;
	}
}

/*
 * Control transfers
 */
static int sl811_submit_control(struct urb *urb)
{
	int maxsze = usb_maxpacket(urb->dev, urb->pipe, usb_pipeout(urb->pipe));
	int len = urb->transfer_buffer_length;
	__u8 *data = urb->transfer_buffer;
	__u8 dev = usb_pipedevice(urb->pipe);
	__u8 pidep = 0;
	__u8 ctrl = 0;
	struct sl811_hc *hc = urb->dev->bus->hcpriv;
	struct sl811_urb_priv *urbp = urb->hcpriv;
	struct sl811_td *td = NULL;
	
	PDEBUG(4, "enter, urb = %p", urb);
	
	if (usb_pipeslow(urb->pipe) && !(hc->rh_status.wPortStatus & USB_PORT_STAT_LOW_SPEED))
		ctrl |= SL811_USB_CTRL_PREAMBLE;
	
	/* Build SETUP TD */
	pidep = PIDEP(USB_PID_SETUP, usb_pipeendpoint(urb->pipe));
	ctrl |= SL811_USB_CTRL_ARM | SL811_USB_CTRL_ENABLE | SL811_USB_CTRL_DIR_OUT;
	td = sl811_alloc_td(urb);
	if (!td)
		return -ENOMEM;
		
	sl811_fill_td(td, ctrl, SL811_DATA_START, 8, pidep, dev, urb->setup_packet);
	sl811_calc_td_time(td);
	
	urbp->cur_td = urbp->first_td = td;
	
	/*
	 * If direction is "send", change the frame from SETUP (0x2D)
	 * to OUT (0xE1). Else change it from SETUP to IN (0x69).
	 */
	pidep = PIDEP(usb_packetid(urb->pipe), usb_pipeendpoint(urb->pipe));
	if (usb_pipeout(urb->pipe))
		ctrl |= SL811_USB_CTRL_DIR_OUT;
	else
		ctrl &= ~SL811_USB_CTRL_DIR_OUT;

	/* Build the DATA TD's */
	while (len > 0) {
		int pktsze = len;

		if (pktsze > maxsze)
			pktsze = maxsze;

		/* Alternate Data0/1 (start with Data1) */
		ctrl ^= SL811_USB_CTRL_TOGGLE_1;
	
		td = sl811_alloc_td(urb);
		if (!td)
			return -ENOMEM;

		sl811_fill_td(td, ctrl, SL811_DATA_START, pktsze, pidep, dev, data);	
		sl811_calc_td_time(td);
		
		data += pktsze;
		len -= pktsze;
	}

	/* Build the final TD for control status */
	td = sl811_alloc_td(urb);
	if (!td)
		return -ENOMEM;

	/* It's IN if the pipe is an output pipe or we're not expecting data back */
	if (usb_pipeout(urb->pipe) || !urb->transfer_buffer_length) {
		pidep = PIDEP(USB_PID_IN, usb_pipeendpoint(urb->pipe));
		ctrl &= ~SL811_USB_CTRL_DIR_OUT;	
	} else {
		pidep = PIDEP(USB_PID_OUT, usb_pipeendpoint(urb->pipe));
		ctrl |= SL811_USB_CTRL_DIR_OUT;
	}
		
	/* End in Data1 */
	ctrl |= SL811_USB_CTRL_TOGGLE_1;

	sl811_fill_td(td, ctrl, SL811_DATA_START, 0, pidep, dev, 0);
	sl811_calc_td_time(td);
	urbp->last_td = td;
/*	
// for debug
	{
		struct list_head *head, *tmp;
		struct sl811_td *td;
		int i = 0;
		head = &urbp->td_list;
		tmp = head->next;
	
		if (list_empty(&urbp->td_list)) {
			PDEBUG(1, "bug!!! td list is empty!");
			return -ENODEV;
		}
		
		while (tmp != head) {
			++i;
			td = list_entry(tmp, struct sl811_td, td_list);
			PDEBUG(3, "td = %p, i = %d", td, i);
			tmp = tmp->next;
		}
	}
*/	
	PDEBUG(4, "leave success");
	
	return 0;
}

/*
 * Result the control urb.
 */
static void sl811_result_control(struct urb *urb)
{
	struct list_head *tmp, *head;
	struct sl811_urb_priv *urbp = urb->hcpriv;
	struct sl811_td *td;

	PDEBUG(4, "enter, urb = %p", urb);
	
	if (list_empty(&urbp->td_list)) {
		PDEBUG(1, "td list is empty");
		return ;
	}

	head = &urbp->td_list;

	tmp = head->next;
	td = list_entry(tmp, struct sl811_td, td_list);

	/* The first TD is the SETUP phase, check the status, but skip the count */
	if (!td->done) {
		PDEBUG(3, "setup phase error, td = %p, done = %d", td, td->done);
		goto err_done;
	}
	if (td->td_status)  {
		PDEBUG(3, "setup phase error, td = %p, td status = %d", td, td->td_status);
		goto err_status;
	}

	urb->actual_length = 0;

	/* The rest of the TD's (but the last) are data */
	tmp = tmp->next;
	while (tmp != head && tmp->next != head) {
		td = list_entry(tmp, struct sl811_td, td_list);
		tmp = tmp->next;
		if (!td->done) {
			PDEBUG(3, "data phase error, td = %p, done = %d", td, td->done);
			goto err_done;
		}
		if (td->td_status)  {
			PDEBUG(3, "data phase error, td = %p, td status = %d", td, td->td_status);
			goto err_status;
		}

		urb->actual_length += td->len - td->left;
		// short packet.
		if (td->left) {
			PDEBUG(3, "data phase short packet, td = %p, count = %d", td, td->len - td->left);
			break;
		}
	}

	/* The last td is status phase */
	td = urbp->last_td;
	if (!td->done) {
		PDEBUG(3, "status phase error, td = %p, done = %d", td, td->done);
		goto err_done;
	}
	if (td->td_status)  {
		PDEBUG(3, "status phase error, td = %p, td status = %d", td, td->td_status);
		goto err_status;
	}
	
	PDEBUG(4, "leave success");
	
	urb->status = 0;
	return ;

err_done:
	if (urbp->unlink)
		urb->status = -ENOENT;
	else {
		PDEBUG(1, "we should not get here! td = %p", td);
		urb->status = -EPIPE;
	}
	return ;	

err_status:
	urb->status = td->td_status;		
	return ;
}

/*
 * Bulk transfers
 */
static int sl811_submit_bulk(struct urb *urb)
{
	int maxsze = usb_maxpacket(urb->dev, urb->pipe, usb_pipeout(urb->pipe));
	int len = urb->transfer_buffer_length;
	__u8 *data = urb->transfer_buffer;
	__u8 dev = usb_pipedevice(urb->pipe);
	__u8 pidep = PIDEP(usb_packetid(urb->pipe), usb_pipeendpoint(urb->pipe));
	__u8 ctrl = 0;
	struct sl811_urb_priv *urbp = urb->hcpriv;
	struct sl811_td *td = NULL;

	PDEBUG(4, "enter, urb = %p", urb);
		
	if (len < 0) {
		PDEBUG(1, "error, urb = %p, len = %d", urb, len);
		return -EINVAL;
	}

	/* Can't have low speed bulk transfers */
	if (usb_pipeslow(urb->pipe)) {
		PDEBUG(1, "error, urb = %p, low speed device", urb);
		return -EINVAL;
	}

	if (usb_pipeout(urb->pipe))
		ctrl |= SL811_USB_CTRL_DIR_OUT;
		
	ctrl |= SL811_USB_CTRL_ARM | SL811_USB_CTRL_ENABLE;
			
	/* Build the DATA TD's */
	do {	/* Allow zero length packets */
		int pktsze = len;

		if (pktsze > maxsze)
			pktsze = maxsze;

		td = sl811_alloc_td(urb);
		if (!td)
			return -ENOMEM;

		/* Alternate Data0/1 (start with Data1) */
		ctrl &= ~SL811_USB_CTRL_TOGGLE_1;
		if (usb_gettoggle(urb->dev, usb_pipeendpoint(urb->pipe), usb_pipeout(urb->pipe)))
			ctrl |= SL811_USB_CTRL_TOGGLE_1;
		usb_dotoggle(urb->dev, usb_pipeendpoint(urb->pipe), usb_pipeout(urb->pipe));		
		
		sl811_fill_td(td, ctrl, SL811_DATA_START, pktsze, pidep, dev, data);
		sl811_calc_td_time(td);
		
		if (urbp->cur_td == NULL)
			urbp->cur_td = urbp->first_td = td;
			
		data += pktsze;
		len -= maxsze;
	} while (len > 0);

	/*
	 * USB_ZERO_PACKET means adding a 0-length packet, if
	 * direction is OUT and the transfer_length was an
	 * exact multiple of maxsze, hence
	 * (len = transfer_length - N * maxsze) == 0
	 * however, if transfer_length == 0, the zero packet
	 * was already prepared above.
	 */
	if (usb_pipeout(urb->pipe) && (urb->transfer_flags & USB_ZERO_PACKET) &&
	   !len && urb->transfer_buffer_length) {
		
		td = sl811_alloc_td(urb);
		if (!td)
			return -ENOMEM;

		/* Alternate Data0/1 (start with Data1) */
		ctrl &= ~SL811_USB_CTRL_TOGGLE_1;
		if (usb_gettoggle(urb->dev, usb_pipeendpoint(urb->pipe), usb_pipeout(urb->pipe)))
			ctrl |= SL811_USB_CTRL_TOGGLE_1;
		usb_dotoggle(urb->dev, usb_pipeendpoint(urb->pipe), usb_pipeout(urb->pipe));
			
		sl811_fill_td(td, ctrl, SL811_DATA_START, 0, pidep, dev, 0);
		sl811_calc_td_time(td);
	}
	
	urbp->last_td = td;
	
	PDEBUG(4, "leave success");
	
	return 0;
}

/*
 * Reset bulk transfers
 */
static int sl811_reset_bulk(struct urb *urb)
{
	struct sl811_urb_priv *urbp = urb->hcpriv;
	struct sl811_td *td;
	struct list_head *head, *tmp;

	PDEBUG(4, "enter, urb = %p", urb);
	
	
	head = &urbp->td_list;	
	tmp = head->next;
	
	while (tmp != head) {
		td = list_entry(tmp, struct sl811_td, td_list);

		/* Alternate Data0/1 (start with Data1) */
		td->ctrl &= ~SL811_USB_CTRL_TOGGLE_1;
		if (usb_gettoggle(urb->dev, usb_pipeendpoint(urb->pipe), usb_pipeout(urb->pipe)))
			td->ctrl |= SL811_USB_CTRL_TOGGLE_1;
		usb_dotoggle(urb->dev, usb_pipeendpoint(urb->pipe), usb_pipeout(urb->pipe));
		
		sl811_reset_td(td);
	} 

	urb->status = -EINPROGRESS;
	urb->actual_length = 0;
	urbp->cur_td = urbp->first_td;

	PDEBUG(4, "leave success");
	
	return 0;
}

/*
 * Result the bulk urb.
 */
static void sl811_result_bulk(struct urb *urb)
{
	struct list_head *tmp, *head;
	struct sl811_urb_priv *urbp = urb->hcpriv;
	struct sl811_td *td = NULL;
	int toggle;

	PDEBUG(4, "enter, urb = %p", urb);
	
	urb->actual_length = 0;

	head = &urbp->td_list;
	tmp = head->next;
	while (tmp != head) {
		td = list_entry(tmp, struct sl811_td, td_list);
		tmp = tmp->next;

		// success.
		if (td->done && td->td_status == 0) {
			urb->actual_length += td->len - td->left;
			
			// short packet
			if (td->left) {
				urb->status = 0;
				PDEBUG(3, "short packet, td = %p, count = %d", td, td->len - td->left);
				goto reset_toggle;
			}
		}
		// tranfer is done but fail, reset the toggle.
		else if (td->done && td->td_status) {
			urb->status = td->td_status;
			PDEBUG(3, "error: td = %p, td status = %d", td, td->td_status);
			goto reset_toggle;
		}
		// unlink, and not do transfer yet
		else if (td->done == 0 && urbp->unlink && td->td_status == 0) {
			urb->status = -ENOENT;
			PDEBUG(3, "unlink and not transfer!");
			return ;
		}
		// unlink, and transfer not complete yet.
		else if (td->done == 0 && urbp->unlink && td->td_status) {
			PDEBUG(3, "unlink and not complete!");
			urb->status = -ENOENT;
			goto reset_toggle;
		}
		// must be bug!!!
		else {// (td->done == 0 && urbp->unlink == 0)
			urb->status = -EPIPE;
			PDEBUG(1, "we should not get here!");
			return ;
		}
	}
	
	PDEBUG(4, "leave success");		
	urb->status = 0;
	return ;

reset_toggle:
	toggle = (td->ctrl & SL811_USB_CTRL_TOGGLE_1) ? 1 : 0;
	usb_settoggle(urb->dev, usb_pipeendpoint(urb->pipe), usb_pipeout(urb->pipe), toggle);
}

/*
 * Find the first urb have the same dev and endpoint.
 */
static inline int sl811_find_same_urb(struct list_head *head, struct urb *urb)
{
	struct list_head *tmp;
	struct urb *u;
	
	if (!head || !urb)
		return 0;
		
	tmp = head->next;
	
	while (tmp != head) {
		u = list_entry(tmp, struct urb, urb_list);
		if (u == urb)
			return 1;
		tmp = tmp->next;	
	}
	
	return 0;
}

/*
 * Find the first urb have the same dev and endpoint.
 */
static inline struct urb* sl811_find_same_devep(struct list_head *head, struct urb *urb)
{
	struct list_head *tmp;
	struct urb *u;
	
	if (!head || !urb)
		return NULL;
		
	tmp = head->next;
	
	while (tmp != head) {
		u = list_entry(tmp, struct urb, urb_list);
		if ((usb_pipe_endpdev(u->pipe)) == (usb_pipe_endpdev(urb->pipe)))
			return u;
		tmp = tmp->next;	
	}
	
	return NULL;
}

/*
 * This function is called by the USB core API when an URB is available to
 * process. 
 */
static int sl811_submit_urb(struct urb *urb)
{
	struct sl811_hc *hc = urb->dev->bus->hcpriv;
	unsigned int pipe = urb->pipe;
	struct list_head *head = NULL;
	unsigned long flags;
	int bustime;
	int ret = 0;
	
	if (!urb) {
		PDEBUG(1, "urb is null");
		return -EINVAL;
	}
	
	if (urb->hcpriv) {
		PDEBUG(1, "urbp is not null, urb = %p, urbp = %p", urb, urb->hcpriv);
		return -EINVAL;
	}
	
	if (!urb->dev || !urb->dev->bus || !hc)  {
		PDEBUG(1, "dev or bus or hc is null");
		return -ENODEV;
	}
	
	if (usb_endpoint_halted(urb->dev, usb_pipeendpoint(pipe), usb_pipeout(pipe))) {
		PDEBUG(2, "sl811_submit_urb: endpoint_halted");
		return -EPIPE;
	}
	
	if (usb_maxpacket(urb->dev, urb->pipe, usb_pipeout(urb->pipe)) > SL811_DATA_LIMIT) {
		printk(KERN_ERR "Packet size is big for SL811, should < %d!\n", SL811_DATA_LIMIT);
		return -EINVAL;
	}
	
	/* a request to the virtual root hub */ 
	if (usb_pipedevice(pipe) == hc->rh.devnum)
		return sl811_rh_submit_urb(urb);
	
	spin_lock_irqsave(&hc->hc_lock, flags);
	spin_lock(&urb->lock);
	
	switch (usb_pipetype(urb->pipe)) {
	case PIPE_ISOCHRONOUS:
		head = &hc->iso_list;
		break;
	case PIPE_INTERRUPT:
		head = &hc->intr_list;
		break;
	case PIPE_CONTROL:
		head = &hc->ctrl_list;
		break;
	case PIPE_BULK:
		head = &hc->bulk_list;
		break;
	}
		
	if (sl811_find_same_devep(head, urb)) {
		list_add(&urb->urb_list, &hc->wait_list);
		PDEBUG(4, "add to wait list");
		goto out_unlock;
	}
	
	if (!sl811_alloc_urb_priv(urb)) {
		ret = -ENOMEM;
		goto out_unlock;
	}
	
	switch (usb_pipetype(urb->pipe)) {
	case PIPE_ISOCHRONOUS:
		if (urb->number_of_packets <= 0) {
			ret = -EINVAL;
			break;
		}
		bustime = usb_check_bandwidth(urb->dev, urb);
		if (bustime < 0) {
			ret = bustime;
			break;
		}
		if (!(ret = sl811_submit_isochronous(urb)))
			usb_claim_bandwidth(urb->dev, urb, bustime, 1);
		break;
	case PIPE_INTERRUPT:
		bustime = usb_check_bandwidth(urb->dev, urb);
		if (bustime < 0)
			ret = bustime;
		else if (!(ret = sl811_submit_interrupt(urb)))
			usb_claim_bandwidth(urb->dev, urb, bustime, 0);
		break;
	case PIPE_CONTROL:
		ret = sl811_submit_control(urb);
		break;
	case PIPE_BULK:
		ret = sl811_submit_bulk(urb);
		break;
	}
	
	if (!ret) {
		((struct sl811_urb_priv *)urb->hcpriv)->inserttime = jiffies;
		list_add(&urb->urb_list, head);
		PDEBUG(4, "add to type list");
		urb->status = -EINPROGRESS;
		if (++hc->active_urbs == 1)
			sl811_enable_interrupt(hc);
		goto out_unlock;	
	} else {
		PDEBUG(2, "submit urb fail! error = %d", ret);
		sl811_free_urb_priv(urb);
	}
	
out_unlock:	
	spin_unlock(&urb->lock);
	spin_unlock_irqrestore(&hc->hc_lock, flags);

	return ret;
}

/*
 * Submit the urb the wait list.
 */
static int sl811_submit_urb_with_lock(struct urb *urb)
{
	struct sl811_hc *hc = urb->dev->bus->hcpriv;
	struct list_head *head = NULL;
	int bustime;
	int ret = 0;
	
	spin_lock(&urb->lock);
	
	switch (usb_pipetype(urb->pipe)) {
	case PIPE_ISOCHRONOUS:
		head = &hc->iso_list;
		break;
	case PIPE_INTERRUPT:
		head = &hc->intr_list;
		break;
	case PIPE_CONTROL:
		head = &hc->ctrl_list;
		break;
	case PIPE_BULK:
		head = &hc->bulk_list;
		break;
	}
		
	if (!sl811_alloc_urb_priv(urb)) {
		ret = -ENOMEM;
		goto out_unlock;
	}
	
	switch (usb_pipetype(urb->pipe)) {
	case PIPE_ISOCHRONOUS:
		if (urb->number_of_packets <= 0) {
			ret = -EINVAL;
			break;
		}
		bustime = usb_check_bandwidth(urb->dev, urb);
		if (bustime < 0) {
			ret = bustime;
			break;
		}
		if (!(ret = sl811_submit_isochronous(urb)))
			usb_claim_bandwidth(urb->dev, urb, bustime, 1);
		break;
	case PIPE_INTERRUPT:
		bustime = usb_check_bandwidth(urb->dev, urb);
		if (bustime < 0)
			ret = bustime;
		else if (!(ret = sl811_submit_interrupt(urb)))
			usb_claim_bandwidth(urb->dev, urb, bustime, 0);
		break;
	case PIPE_CONTROL:
		ret = sl811_submit_control(urb);
		break;
	case PIPE_BULK:
		ret = sl811_submit_bulk(urb);
		break;
	}
	
	if (ret == 0) {
		((struct sl811_urb_priv *)urb->hcpriv)->inserttime = jiffies;
		list_add(&urb->urb_list, head);
		PDEBUG(4, "add to type list");
		urb->status = -EINPROGRESS;
		if (++hc->active_urbs == 1)
			sl811_enable_interrupt(hc);
		goto out_unlock;	
	} else {
		PDEBUG(2, "submit urb fail! error = %d", ret);
		sl811_free_urb_priv(urb);
	}
	
out_unlock:	
	spin_unlock(&urb->lock);

	return ret;
}

/*
 * Reset the urb
 */
static void sl811_reset_urb(struct urb *urb)
{
	struct sl811_urb_priv *urbp = urb->hcpriv;

	switch (usb_pipetype(urb->pipe)) {
	case PIPE_ISOCHRONOUS:
		sl811_reset_isochronous(urb);
		break;
	case PIPE_INTERRUPT:
		sl811_reset_interrupt(urb);
		break;
	case PIPE_CONTROL:
		return;
	case PIPE_BULK:
		sl811_reset_bulk(urb);
		break;
	}
	urbp->inserttime = jiffies;
}

/*
 * Return the result of a transfer
 */
static void sl811_result_urb(struct urb *urb)
{
	struct sl811_urb_priv *urbp = urb->hcpriv;
	struct sl811_hc *hc = urb->dev->bus->hcpriv;
	struct list_head *head = NULL;
	struct urb *u = NULL;
	int reset = 0;
	int ring = 0;

	if (urb->status != -EINPROGRESS) {
		PDEBUG(1, "urb status is not EINPROGRESS!");
		return ;
	}
	
	spin_lock(&urb->lock);
	
	switch (usb_pipetype(urb->pipe)) {
	case PIPE_ISOCHRONOUS:
		head = &hc->iso_list;
		sl811_result_isochronous(urb);
		
		// if the urb is not unlink and is in a urb "ring", we reset it
		if (!urbp->unlink && urb->next) 
			ring = 1;
		break;
	case PIPE_INTERRUPT:
		head = &hc->intr_list;
		sl811_result_interrupt(urb);
		
		// if the urb is not unlink and not "once" query, we reset.
		if (!urbp->unlink && urb->interval)
			reset = 1;
		break;
	case PIPE_CONTROL:
		head = &hc->ctrl_list;
		sl811_result_control(urb);
		break;
	case PIPE_BULK:
		head = &hc->bulk_list;
		sl811_result_bulk(urb);
		
		// if the urb is not unlink and is in a urb "ring", we reset it
		if (!urbp->unlink && urb->next)
			ring = 1;
		break;
	}
	
	PDEBUG(4, "result urb status = %d", urb->status);
	
	if (ring && urb->next == urb)
		reset = 1;
	
	if (!reset) {
		switch (usb_pipetype(urb->pipe)) {
		case PIPE_ISOCHRONOUS:
			usb_release_bandwidth(urb->dev, urb, 1);
			break;
		case PIPE_INTERRUPT:
			usb_release_bandwidth(urb->dev, urb, 0);
			break;
		}
		sl811_free_urb_priv(urb);
	}
	
	spin_unlock(&urb->lock);
	
	if (urb->complete)
		urb->complete(urb);
	
	if (reset) {
		spin_lock(&urb->lock);
		sl811_reset_urb(urb);
		if (usb_pipeint(urb->pipe))
			list_add(&urb->urb_list, &hc->idle_intr_list);
		else
			list_add(&urb->urb_list, head);
		spin_unlock(&urb->lock);
	} else {
		if (--hc->active_urbs <= 0) {
			hc->active_urbs = 0;
			sl811_disable_interrupt(hc);
		}
		
		if (ring) 
			u = urb->next;
		else
			u = sl811_find_same_devep(&hc->wait_list, urb);
			
		if (u) {
			if (!list_empty(&u->urb_list))
				list_del(&u->urb_list);
			if (sl811_submit_urb_with_lock(u))
				list_add(&u->urb_list, &hc->wait_list);
		}
	}
}


#ifdef SL811_TIMEOUT

/*
 * Unlink the urb from the urb list
 */
static int sl811_unlink_urb(struct urb *urb)
{
	unsigned long flags;
	struct sl811_hc *hc;
	struct sl811_urb_priv *urbp;
	int call = 0;
	int schedule = 0;
	int count = 0;
	
	if (!urb) {
		PDEBUG(1, "urb is null");
		return -EINVAL;
	}
	
	if (!urb->dev || !urb->dev->bus) {
		PDEBUG(1, "dev or bus is null");
		return -ENODEV;
	}
	
	hc = urb->dev->bus->hcpriv; 
	urbp = urb->hcpriv;
	
	/* a request to the virtual root hub */
	if (usb_pipedevice(urb->pipe) == hc->rh.devnum)
		return sl811_rh_unlink_urb(urb); 
	
	spin_lock_irqsave(&hc->hc_lock, flags);
	spin_lock(&urb->lock);

	// in wait list
	if (sl811_find_same_urb(&hc->wait_list, urb)) {	
		PDEBUG(4, "unlink urb in wait list");
		list_del_init(&urb->urb_list);
		urb->status = -ENOENT;
		call = 1;
		goto out;
	}
	
	// in intr idle list.
	if (sl811_find_same_urb(&hc->idle_intr_list, urb)) {	
		PDEBUG(4, "unlink urb in idle intr list");
		list_del_init(&urb->urb_list);
		urb->status = -ENOENT;
		sl811_free_urb_priv(urb);
		usb_release_bandwidth(urb->dev, urb, 0);
		if (--hc->active_urbs <= 0) {
			hc->active_urbs = 0;
			sl811_disable_interrupt(hc);
		}
		call = 1;
		goto out;
	}

	if (urb->status == -EINPROGRESS) {  
		PDEBUG(3, "urb is still in progress");
		urbp->unlink = 1;

re_unlink:
		// Is it in progress?
		urbp = urb->hcpriv;
		if (urbp && hc->cur_td == urbp->cur_td) {
			++count;
			if (sl811_read(hc, 0) & SL811_USB_CTRL_ARM) {
				PDEBUG(3, "unlink: cur td is still in progress! count = %d", count);
re_schedule:				
				schedule = 1;
				spin_unlock(&urb->lock);
				spin_unlock_irqrestore(&hc->hc_lock, flags);
				schedule_timeout(HZ/50);
				spin_lock_irqsave(&hc->hc_lock, flags);
				spin_lock(&urb->lock);
			} else {
				PDEBUG(3, "unlink: lost of interrupt? do parse! count = %d", count);
				spin_unlock(&urb->lock);
				sl811_transfer_done(hc, 0);
				spin_lock(&urb->lock);
			}
			goto re_unlink;
		}
		
		if (list_empty(&urb->urb_list)) {
			PDEBUG(3, "unlink: list empty!");
			goto out;
		}
			
		if (urb->transfer_flags & USB_TIMEOUT_KILLED) { 
			PDEBUG(3, "unlink: time out killed");
			// it is timeout killed by us 
			goto result;
		} else if (urb->transfer_flags & USB_ASYNC_UNLINK) { 
			// we do nothing, just let it be processing later
			PDEBUG(3, "unlink async, do nothing");
			goto out;
		} else {
			// synchron without callback
			PDEBUG(3, "unlink synchron, we wait the urb complete or timeout");
			if (schedule == 0) {
				PDEBUG(3, "goto re_schedule");
				goto re_schedule;
			} else {
				PDEBUG(3, "already scheduled");
				goto result;
			}
		}
	} else if (!list_empty(&urb->urb_list)) {
		PDEBUG(1, "urb = %p, status = %d is in a list, why?", urb, urb->status);
		//list_del_init(&urb->urb_list);
		//call = 1;
	}

out:
	spin_unlock(&urb->lock);
	spin_unlock_irqrestore(&hc->hc_lock, flags);
	
	if (call && urb->complete)
		urb->complete(urb);
	
	return 0;
	
result:
	spin_unlock(&urb->lock);
	
	list_del_init(&urb->urb_list);
	sl811_result_urb(urb);	
	
	spin_unlock_irqrestore(&hc->hc_lock, flags);
	
	return 0;
}

#else

/*
 * Unlink the urb from the urb list
 */
static int sl811_unlink_urb(struct urb *urb)
{
	unsigned long flags;
	struct sl811_hc *hc;
	struct sl811_urb_priv *urbp;
	int call = 0;
	
	if (!urb) {
		PDEBUG(1, "urb is null");
		return -EINVAL;
	}
	
	if (!urb->dev || !urb->dev->bus) {
		PDEBUG(1, "dev or bus is null");
		return -ENODEV;
	}
	
	hc = urb->dev->bus->hcpriv; 
	urbp = urb->hcpriv;
	
	/* a request to the virtual root hub */
	if (usb_pipedevice(urb->pipe) == hc->rh.devnum)
		return sl811_rh_unlink_urb(urb); 
	
	spin_lock_irqsave(&hc->hc_lock, flags);
	spin_lock(&urb->lock);

	// in wait list
	if (sl811_find_same_urb(&hc->wait_list, urb)) {	
		PDEBUG(2, "unlink urb in wait list");
		list_del_init(&urb->urb_list);
		urb->status = -ENOENT;
		call = 1;
		goto out;
	}
	
	if (urb->status == -EINPROGRESS) {  
		PDEBUG(2, "urb is still in progress");
		urbp->unlink = 1;

		// Is it in progress?
		urbp = urb->hcpriv;
		if (urbp && hc->cur_td == urbp->cur_td) {
			// simple, let it out
			PDEBUG(2, "unlink: cur td is still in progress!");
			hc->cur_td = NULL;
		}
		
		goto result;
	} else if (!list_empty(&urb->urb_list)) {
		PDEBUG(1, "urb = %p, status = %d is in a list, why?", urb, urb->status);
		list_del_init(&urb->urb_list);
		if (urbp)
			goto result;
		else
			call = 1;
	}

out:
	spin_unlock(&urb->lock);
	spin_unlock_irqrestore(&hc->hc_lock, flags);
	
	if (call && urb->complete)
		urb->complete(urb);
			
	return 0;
	
result:
	spin_unlock(&urb->lock);
	
	list_del_init(&urb->urb_list);
	sl811_result_urb(urb);	
	
	spin_unlock_irqrestore(&hc->hc_lock, flags);
	
	return 0;
}

#endif

static int sl811_get_current_frame_number(struct usb_device *usb_dev)
{
	return ((struct sl811_hc *)(usb_dev->bus->hcpriv))->frame_number;
}

static struct usb_operations sl811_device_operations = 
{
	sl811_alloc_dev_priv,
	sl811_free_dev_priv,
	sl811_get_current_frame_number,
	sl811_submit_urb,
	sl811_unlink_urb
};

/*
 * This	functions transmit a td.
 */
static inline void sl811_trans_cur_td(struct sl811_hc *hc, struct sl811_td *td)
{
	sl811_print_td(4, td);
	sl811_write_buf(hc, SL811_ADDR_A,  &td->addr, 4);
	if (td->len && (td->ctrl & SL811_USB_CTRL_DIR_OUT))
		sl811_write_buf(hc, td->addr,  td->buf, td->len);

	sl811_write(hc,	SL811_CTRL_A, td->ctrl);
}

		
/*
 * This	function checks	the status of the transmitted or received packet
 * and copy the	data from the SL811HS register into a buffer.
 */
static void sl811_parse_cur_td(struct sl811_hc *hc, struct sl811_td *td)
{
	struct urb *urb = td->urb;
#ifdef SL811_DEBUG
	int dev = usb_pipedevice(td->urb->pipe);
	int ep = usb_pipeendpoint(td->urb->pipe);
#endif

	sl811_read_buf(hc, SL811_STS_A, &td->status, 2);
	
	if (td->status & SL811_USB_STS_ACK) {
		td->done = 1;
		
/*		if ((td->ctrl & SL811_USB_CTRL_TOGGLE_1) != (td->status & SL811_USB_STS_TOGGLE_1)) {
			PDEBUG(2, "dev %d endpoint %d unexpect data toggle!", dev, ep);
			td->td_status = -EILSEQ;
		}
*/		
		if (!(td->ctrl & SL811_USB_CTRL_DIR_OUT) && td->len > 0)
			sl811_read_buf(hc, td->addr, td->buf, td->len - td->left);
			
		if (td->left && (urb->transfer_flags & USB_DISABLE_SPD)) {
			PDEBUG(2, "dev %d endpoint %d unexpect short packet! td = %p", dev, ep, td);
			td->td_status = -EREMOTEIO;
		} else
			td->td_status = 0;
	} else if (td->status & SL811_USB_STS_STALL) {
		PDEBUG(2, "dev %d endpoint %d halt, td = %p", dev, ep, td);
		td->td_status = -EPIPE;
		if (urb->dev)
			usb_endpoint_halt(td->urb->dev, usb_pipeendpoint(td->urb->pipe), usb_pipeout(td->urb->pipe));
		td->done = 1;
	} else if (td->status & SL811_USB_STS_OVERFLOW) {
		PDEBUG(1, "dev %d endpoint %d overflow, sl811 only support packet less than %d", dev, ep, SL811_DATA_LIMIT);	
		td->td_status = -EOVERFLOW;
		td->done = 1;
	} else if (td->status & SL811_USB_STS_TIMEOUT ) {
		PDEBUG(2, "dev %d endpoint %d timeout, td = %p", dev, ep, td);	
		td->td_status = -ETIMEDOUT;
		if (--td->errcnt == 0)
			td->done = 1;
	} else if (td->status & SL811_USB_STS_ERROR) {
		PDEBUG(2, "dev %d endpoint %d error, td = %p", dev, ep, td);
		td->td_status = -EILSEQ;
		if (--td->errcnt == 0)
			td->done = 1;
	} else if (td->status & SL811_USB_STS_NAK) {
		++td->nakcnt;
		PDEBUG(3, "dev %d endpoint %d nak, td = %p, count = %d", dev, ep, td, td->nakcnt);
		td->td_status = -EINPROGRESS;
		if (!usb_pipeslow(td->urb->pipe) && td->nakcnt > 1024) {
			PDEBUG(2, "too many naks, td = %p, count = %d", td, td->nakcnt);
			td->td_status = -ETIMEDOUT;
			td->done = 1;
		} 
	} 
	
	sl811_print_td(4, td);
}

/*
 * This	function checks	the status of current urb.
 */
static int sl811_parse_cur_urb(struct urb *urb)
{
	struct sl811_urb_priv *urbp = urb->hcpriv;
	struct sl811_td *td = urbp->cur_td;
	struct list_head *tmp;
	
	sl811_print_td(5, td);
	
	// this td not done yet.
	if (!td->done)
		return 0;
	
	// the last ld, so the urb is done.
	if (td == urbp->last_td) {
		PDEBUG(4, "urb = %p is done success", td->urb);
		if (usb_pipeisoc(td->urb->pipe))
			PDEBUG(4, "ISO URB DONE, td = %p", td);
		return 1;
	}
	
	// iso transfer, we always advance to next td 
	if (usb_pipeisoc(td->urb->pipe)) {
		tmp = &td->td_list;
		tmp = tmp->next;
		urbp->cur_td = list_entry(tmp, struct sl811_td, td_list);
		PDEBUG(4, "ISO NEXT, td = %p", urbp->cur_td);	
		return 0;
	}
		
	// some error occur, so the urb is done.
	if (td->td_status) {
		PDEBUG(3, "urb = %p is done error, td status is = %d", td->urb, td->td_status);
		return 1;
	}
		
	// short packet.
	if (td->left) {
		if (usb_pipecontrol(td->urb->pipe)) {
			// control packet, we advance to the last td
			PDEBUG(3, "ctrl short packet, advance to last td");
			urbp->cur_td = urbp->last_td;
			return 0;
		} else {
			// interrut and bulk packet, urb is over.
			PDEBUG(3, "bulk or intr short packet, urb is over");
			return 1;
		}
	}

	// we advance to next td.	
	tmp = &td->td_list;
	tmp = tmp->next;
	urbp->cur_td = list_entry(tmp, struct sl811_td, td_list);
#ifdef SL811_DEBUG
	PDEBUG(4, "advance to the next td, urb = %p, td = %p", urb, urbp->cur_td);
	sl811_print_td(5, urbp->cur_td);
	if (td == urbp->cur_td)
		PDEBUG(1, "bug!!!");
#endif		
	return 0;
}

/*
 * Find the next td to transfer.
 */
static inline struct sl811_td* sl811_schedule_next_td(struct urb *urb, struct sl811_td *cur_td)
{
	struct sl811_urb_priv *urbp = urb->hcpriv;
	
	PDEBUG(4, "urb at %p, cur td at %p", urb, cur_td);
	
	// iso don't schedule the td in the same frame.
	if (usb_pipeisoc(cur_td->urb->pipe))
		return NULL;
	
	// cur td is not complete
	if (!cur_td->done)
		return NULL;	
	
	// here, urbp->cur_td is already the next td;
	return urbp->cur_td;
}

/*
 * Scan the list to find a active urb
 */
static inline struct urb* sl811_get_list_next_urb(struct sl811_hc *hc, struct list_head *next)
{
	struct urb *urb;
	int i;
	
	if (list_empty(next))
		return NULL;
	
	if (next == hc->cur_list)
		return NULL;
			
	for (i = 0; i < 4; ++i) 
		if (next == &hc->urb_list[i])
			return NULL;
			
	urb = list_entry(next, struct urb, urb_list);
	PDEBUG(4, "next urb in list is at %p", urb);
		
	return urb;
}

/*
 * Find the next td to transfer.
 */
static struct sl811_td* sl811_schedule_next_urb(struct sl811_hc *hc, struct list_head *next)
{
	struct urb *urb = NULL;
	int back_loop = 1;
	struct list_head *old_list = hc->cur_list;
		
	// try to get next urb in the same list.
	if (next) {
		urb = sl811_get_list_next_urb(hc, next);
		if (!urb)
			++hc->cur_list;
	}

	// try other list.
	if (!urb) {			
re_loop:
		// try all the list.
		while (hc->cur_list < &hc->urb_list[4]) { 
			if ((urb = sl811_get_list_next_urb(hc, hc->cur_list->next)))
				return ((struct sl811_urb_priv *)urb->hcpriv)->cur_td;
			++hc->cur_list;
		}
		// the last list is try 
		if (back_loop && (old_list >= &hc->ctrl_list)) {
			hc->cur_list = &hc->ctrl_list;
			back_loop = 0;
			goto re_loop;
		}
	}
	
	if (hc->cur_list > &hc->urb_list[3])
		hc->cur_list = &hc->ctrl_list;
			
	return NULL;
}

/*
 * This function process the transfer rusult.
 */
static void sl811_transfer_done(struct sl811_hc *hc, int sof) 
{
	struct sl811_td *cur_td = hc->cur_td, *next_td = NULL;
	struct urb *cur_urb = NULL;
       	struct list_head *next = NULL;
       	int done;
	
	PDEBUG(5, "enter");
	
	if (cur_td == NULL) {
		PDEBUG(1, "in done interrupt, but td is null, be already parsed?");
		return ;
	}

	cur_urb = cur_td->urb;
	hc->cur_td = NULL;
	next = &cur_urb->urb_list;
	next = next->next;
	
	spin_lock(&cur_urb->lock);	
	sl811_parse_cur_td(hc, cur_td);
	done = sl811_parse_cur_urb(cur_urb);
	spin_unlock(&cur_urb->lock);
	
	if (done) {
		list_del_init(&cur_urb->urb_list);
		cur_td = NULL;
		sl811_result_urb(cur_urb);	
	}

	if (sof)
		return ;
	
	if (!done) {
		next_td = sl811_schedule_next_td(cur_urb, cur_td);
		if (next_td && next_td != cur_td && (sl811_calc_bus_remainder(hc) > next_td->bustime)) {
			hc->cur_td = next_td;
			PDEBUG(5, "ADD TD");
			sl811_trans_cur_td(hc, next_td);
			return ;
		}
	}
	
	while (1) {
		next_td = sl811_schedule_next_urb(hc, next);
		if (!next_td)
			return;
		if (next_td == cur_td)
			return;
		next = &next_td->urb->urb_list;
		next = next->next;
		if (sl811_calc_bus_remainder(hc) > next_td->bustime) {
			hc->cur_td = next_td;
			PDEBUG(5, "ADD TD");
			sl811_trans_cur_td(hc, next_td);
			return ;
		}
	}
}

/*
 *
 */
static void inline sl811_dec_intr_interval(struct sl811_hc *hc)
{
	struct list_head *head, *tmp;
	struct urb *urb;
	struct sl811_urb_priv *urbp;
	
	if (list_empty(&hc->idle_intr_list))
		return ;
	
	head = &hc->idle_intr_list;
	tmp = head->next;
	
	while (tmp != head) {
		urb = list_entry(tmp, struct urb, urb_list);
		tmp = tmp->next;
		spin_lock(&urb->lock);
		urbp = urb->hcpriv;
		if (--urbp->interval == 0) {
			list_del(&urb->urb_list);
			list_add(&urb->urb_list, &hc->intr_list);
			PDEBUG(4, "intr urb active");
		}
		spin_unlock(&urb->lock);
	}
}

/*
 * The sof interrupt is happen.	
 */
static void sl811_start_sof(struct sl811_hc *hc)
{
	struct sl811_td *next_td;
#ifdef SL811_DEBUG
	static struct sl811_td *repeat_td = NULL;
	static int repeat_cnt = 1;
#endif	
	if (++hc->frame_number > 1024)
		hc->frame_number = 0;
	
	if (hc->active_urbs == 0)
		return ;
	
	sl811_dec_intr_interval(hc);
	
	if (hc->cur_td) {
		if (sl811_read(hc, 0) & SL811_USB_CTRL_ARM) {
#ifdef SL811_DEBUG
			if (repeat_td == hc->cur_td) 
				++repeat_cnt;
			else {
				if (repeat_cnt >= 2)
					PDEBUG(2, "cur td = %p repeat %d", hc->cur_td, repeat_cnt);
				repeat_cnt = 1;
				repeat_td = hc->cur_td;
			}
#endif
			return ;
		} else {
			PDEBUG(2, "lost of interrupt in sof? do parse!");
			sl811_transfer_done(hc, 1);
			
			// let this frame idle	
			return;
		}
	}
	
	hc->cur_list = &hc->iso_list;
	
	if (hc->active_urbs == 0)
		return ;
	
	next_td = sl811_schedule_next_urb(hc, NULL);
	if (!next_td) {
#ifdef SL811_DEBUG
		if (list_empty(&hc->idle_intr_list))
			PDEBUG(2, "not schedule a td, why? urbs = %d", hc->active_urbs);
#endif
		return; 
	}
	if (sl811_calc_bus_remainder(hc) > next_td->bustime) {
		hc->cur_td = next_td;
		sl811_trans_cur_td(hc, next_td);
	} else
		PDEBUG(2, "bus time if not enough, why?");
}

/*
 * This	function resets	SL811HS	controller and detects the speed of
 * the connecting device
 *
 * Return: 0 = no device attached; 1 = USB device attached
 */
static int sl811_hc_reset(struct sl811_hc *hc)
{
	int status ;

	sl811_write(hc,	SL811_CTRL2, SL811_CTL2_HOST | SL811_12M_HI);
	sl811_write(hc,	SL811_CTRL1, SL811_CTRL1_RESET);

	mdelay(20);
	
	// Disable hardware SOF generation, clear all irq status.
	sl811_write(hc,	SL811_CTRL1, 0);
	mdelay(2);
	sl811_write(hc, SL811_INTRSTS, 0xff); 
	status = sl811_read(hc, SL811_INTRSTS);

	if (status & SL811_INTR_NOTPRESENT) {
		// Device is not present
		PDEBUG(0, "Device not present");
		hc->rh_status.wPortStatus &= ~(USB_PORT_STAT_CONNECTION | USB_PORT_STAT_ENABLE);
		hc->rh_status.wPortChange |= USB_PORT_STAT_C_CONNECTION;
		sl811_write(hc,	SL811_INTR, SL811_INTR_INSRMV);
		return 0;
	}

	// Send SOF to address 0, endpoint 0.
	sl811_write(hc, SL811_LEN_B, 0);
	sl811_write(hc, SL811_PIDEP_B, PIDEP(USB_PID_SOF, 0));
	sl811_write(hc, SL811_DEV_B, 0x00);
	sl811_write (hc, SL811_SOFLOW, SL811_12M_HI);

	if (status & SL811_INTR_SPEED_FULL) {
		/* full	speed device connect directly to root hub */
		PDEBUG (0, "Full speed Device attached");
		
		sl811_write(hc, SL811_CTRL1, SL811_CTRL1_RESET);
		mdelay(20);
		sl811_write(hc,	SL811_CTRL2, SL811_CTL2_HOST | SL811_12M_HI);
		sl811_write(hc, SL811_CTRL1, SL811_CTRL1_SOF);

		/* start the SOF or EOP	*/
		sl811_write(hc, SL811_CTRL_B, SL811_USB_CTRL_ARM);
		hc->rh_status.wPortStatus |= USB_PORT_STAT_CONNECTION;
		hc->rh_status.wPortStatus &= ~USB_PORT_STAT_LOW_SPEED;
		mdelay(2);
		sl811_write (hc, SL811_INTRSTS, 0xff);
	} else {
		/* slow	speed device connect directly to root-hub */
		PDEBUG(0, "Low speed Device attached");
		
		sl811_write(hc, SL811_CTRL1, SL811_CTRL1_RESET);
		mdelay(20);
		sl811_write(hc,	SL811_CTRL2, SL811_CTL2_HOST | SL811_CTL2_DSWAP | SL811_12M_HI);
		sl811_write(hc, SL811_CTRL1, SL811_CTRL1_SPEED_LOW | SL811_CTRL1_SOF);

		/* start the SOF or EOP	*/
		sl811_write(hc, SL811_CTRL_B, SL811_USB_CTRL_ARM);
		hc->rh_status.wPortStatus |= USB_PORT_STAT_CONNECTION | USB_PORT_STAT_LOW_SPEED;
		mdelay(2);
		sl811_write(hc, SL811_INTRSTS, 0xff);
	}

	hc->rh_status.wPortChange |= USB_PORT_STAT_C_CONNECTION;
	sl811_write(hc,	SL811_INTR, SL811_INTR_INSRMV);
	
	return 1;
}

/*
 * Interrupt service routine.
 */
static void sl811_interrupt(int irq, void *__hc, struct pt_regs * r)
{
	__u8 status;
	struct sl811_hc *hc = __hc;

	status = sl811_read(hc, SL811_INTRSTS);
	if (status == 0)
		return ; /* Not me */

	sl811_write(hc,	SL811_INTRSTS, 0xff);

	if (status & SL811_INTR_INSRMV) {
		sl811_write(hc,	SL811_INTR, 0);
		sl811_write(hc,	SL811_CTRL1, 0);
		// wait	for device stable
		mdelay(100);			
		sl811_hc_reset(hc);
		return ;
	}

	spin_lock(&hc->hc_lock);
	
	if (status & SL811_INTR_DONE_A) {
		if (status & SL811_INTR_SOF) {
			sl811_transfer_done(hc, 1);
			PDEBUG(4, "sof in done!");
			sl811_start_sof(hc);
		} else
			sl811_transfer_done(hc, 0);
	} else if (status & SL811_INTR_SOF)
		sl811_start_sof(hc);	

	spin_unlock(&hc->hc_lock);	

	return ;
}

/*
 * This	function allocates all data structure and store	in the
 * private data	structure.
 *
 * Return value	 : data structure for the host controller
 */
static struct sl811_hc* __devinit sl811_alloc_hc(void)
{
	struct sl811_hc *hc;
	struct usb_bus *bus;
	int i;

	PDEBUG(5, "enter");

	hc = (struct sl811_hc *)kmalloc(sizeof(struct sl811_hc), GFP_KERNEL);
	if (!hc)
		return NULL;

	memset(hc, 0, sizeof(struct sl811_hc));

	hc->rh_status.wPortStatus = USB_PORT_STAT_POWER;
	hc->rh_status.wPortChange = 0;

	hc->active_urbs	= 0;
	INIT_LIST_HEAD(&hc->hc_hcd_list);
	list_add(&hc->hc_hcd_list, &sl811_hcd_list);
	
	init_waitqueue_head(&hc->waitq);
	
	for (i = 0; i < 6; ++i)
		INIT_LIST_HEAD(&hc->urb_list[i]);
	
	hc->cur_list = &hc->iso_list;

	bus = usb_alloc_bus(&sl811_device_operations);
	if (!bus) {
		kfree (hc);
		return NULL;
	}

	hc->bus	= bus;
	bus->bus_name = MODNAME;
	bus->hcpriv = hc;

	return hc;
}

/*
 * This	function De-allocate all resources
 */
static void sl811_release_hc(struct sl811_hc *hc)
{
	PDEBUG(5, "enter");

	/* disconnect all devices */
	if (hc->bus->root_hub)
		usb_disconnect(&hc->bus->root_hub);

	// Stop interrupt handle
	if (hc->irq)
		free_irq(hc->irq, hc);
	hc->irq = 0;

	/* Stop interrupt for sharing, but only, if PatternTest ok */
	if (hc->addr_io) {
		/* Disable Interrupts */
		sl811_write(hc,	SL811_INTR, 0);

		/* Remove all Interrupt events */
		mdelay(2);
		sl811_write(hc,	SL811_INTRSTS, 0xff);
	}

	/* free io regions */
	sl811_release_regions(hc);

	usb_deregister_bus(hc->bus);
	usb_free_bus(hc->bus);

	list_del(&hc->hc_hcd_list);
	INIT_LIST_HEAD(&hc->hc_hcd_list);

	kfree (hc);
}

/*
 * This	function request IO memory regions, request IRQ, and
 * allocate all	other resources.
 *
 * Input: addr_io = first IO address
 *	  data_io = second IO address
 *	  irq =	interrupt number
 *
 * Return: 0 = success or error	condition
 */
static int __devinit sl811_found_hc(int addr_io, int data_io, int irq)
{
	struct sl811_hc *hc;

	PDEBUG(5, "enter");

	hc = sl811_alloc_hc();
	if (!hc)
		return -ENOMEM;

	if (sl811_request_regions (hc, addr_io, data_io, MODNAME)) {
		PDEBUG(1, "ioport %X,%X is in use!", addr_io, data_io);
		sl811_release_hc(hc);
		return -EBUSY;
	}

	if (sl811_reg_test(hc)) {
		PDEBUG(1, "SL811 register test failed!");
		sl811_release_hc(hc);
		return -ENODEV;
	}
	
#ifdef SL811_DEBUG_VERBOSE
	{
	    __u8 u = SL811Read (hci, SL11H_HWREVREG);
	    
	    // Show the hardware revision of chip
	    PDEBUG(1, "SL811 HW: %02Xh", u);
	    switch (u & 0xF0) {
	    case 0x00: PDEBUG(1, "SL11H");		break;
	    case 0x10: PDEBUG(1, "SL811HS rev1.2");	break;
	    case 0x20: PDEBUG(1, "SL811HS rev1.5");	break;
	    default:   PDEBUG(1, "Revision unknown!");
	    }
	}
#endif // SL811_DEBUG_VERBOSE

	sl811_init_irq();

	usb_register_bus(hc->bus);

	if (request_irq(irq, sl811_interrupt, SA_SHIRQ, MODNAME, hc)) {
		PDEBUG(1, "request interrupt %d failed", irq);
		sl811_release_hc(hc);
		return -EBUSY;
	}
	hc->irq	= irq;

	printk(KERN_INFO __FILE__ ": USB SL811 at %x,%x, IRQ %d\n",
		addr_io, data_io, irq);

	sl811_hc_reset(hc);
	sl811_connect_rh(hc);
	
	return 0;
}

/*
 * This	is an init function, and it is the first function being	called
 *
 * Return: 0 = success or error	condition
 */
static int __init sl811_hcd_init(void)
{
	int ret = -ENODEV;
        int count;
	
	PDEBUG(5, "enter");

	info(DRIVER_VERSION " : " DRIVER_DESC);

        // registering some instance
	for (count = 0; count < MAX_CONTROLERS; count++) {
		if (io[count]) {
			ret = sl811_found_hc(io[count], io[count]+OFFSET_DATA_REG, irq[count]);
			if (ret)
				return (ret);
		}
	}

	return ret;
}

/*
 * This	is a cleanup function, and it is called	when module is unloaded.
 */
static void __exit sl811_hcd_cleanup(void)
{
	struct list_head *list = sl811_hcd_list.next;
	struct sl811_hc *hc;

	PDEBUG(5, "enter");

	for (; list != &sl811_hcd_list; ) {
		hc = list_entry(list, struct sl811_hc, hc_hcd_list);
		list = list->next;
		sl811_release_hc(hc);
	}
}

module_init(sl811_hcd_init);
module_exit(sl811_hcd_cleanup);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
