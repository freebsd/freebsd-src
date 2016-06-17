/*
 * BRIEF MODULE DESCRIPTION
 *	Au1x00 USB Device-Side Serial TTY Driver (function layer)
 *
 * Copyright 2001-2002 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *		stevel@mvista.com or source@mvista.com
 *
 *  Derived from drivers/usb/serial/usbserial.c:
 *
 *  Copyright (C) 1999 - 2001 Greg Kroah-Hartman (greg@kroah.com)
 *  Copyright (c) 2000 Peter Berger (pberger@brimson.com)
 *  Copyright (c) 2000 Al Borchers (borchers@steinerpoint.com)
 *
 *  This program is free software; you can redistribute	 it and/or modify it
 *  under  the terms of	 the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the	License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED	  ``AS	IS'' AND   ANY	EXPRESS OR IMPLIED
 *  WARRANTIES,	  INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO	EVENT  SHALL   THE AUTHOR  BE	 LIABLE FOR ANY	  DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED	  TO, PROCUREMENT OF  SUBSTITUTE GOODS	OR SERVICES; LOSS OF
 *  USE, DATA,	OR PROFITS; OR	BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN	 CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
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
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/smp_lock.h>
#undef DEBUG
#include <linux/usb.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/au1000.h>
#include <asm/au1000_usbdev.h>


/* local function prototypes */
static int serial_open(struct tty_struct *tty, struct file *filp);
static void serial_close(struct tty_struct *tty, struct file *filp);
static int serial_write(struct tty_struct *tty, int from_user,
			const unsigned char *buf, int count);
static int serial_write_room(struct tty_struct *tty);
static int serial_chars_in_buffer(struct tty_struct *tty);
static void serial_throttle(struct tty_struct *tty);
static void serial_unthrottle(struct tty_struct *tty);
static int serial_ioctl(struct tty_struct *tty, struct file *file,
			unsigned int cmd, unsigned long arg);
static void serial_set_termios (struct tty_struct *tty, struct termios * old);

#define SERIAL_TTY_MAJOR 189 // FIXME: need a legal major

#define MAX_NUM_PORTS 2

#define IN_MAX_PACKET_SIZE  32
#define OUT_MAX_PACKET_SIZE 32

// FIXME: when Au1x00 endpoints 3 and 5 are fixed, make NUM_PORTS=2
#define NUM_PORTS 2
#define NUM_EP 2*NUM_PORTS

#define CONFIG_DESC_LEN \
 USB_DT_CONFIG_SIZE + USB_DT_INTERFACE_SIZE + NUM_EP*USB_DT_ENDPOINT_SIZE

struct usb_serial_port {
	struct tty_struct *tty;	   /* the coresponding tty for this port */
	unsigned char number;
	spinlock_t port_lock;

	struct usb_endpoint_descriptor* out_desc;
	struct usb_endpoint_descriptor* in_desc;

	int out_ep_addr; /* endpoint address of OUT endpoint */
	int in_ep_addr;  /* endpoint address of IN endpoint */
	
	/* task queue for line discipline waking up on send packet complete */
	struct tq_struct send_complete_tq;
	/* task queue for line discipline wakeup on receive packet complete */
	struct tq_struct receive_complete_tq;

	int active;	/* someone has this device open */
	int writing;    /* a packet write is in progress */
	int open_count;	/* number of times this port has been opened */

};

static struct usb_serial {
	usbdev_state_t dev_state; // current state of device layer
	struct usb_device_descriptor* dev_desc;
	struct usb_config_descriptor* config_desc;
	struct usb_interface_descriptor* if_desc;
	struct usb_string_descriptor * str_desc[6];
	void* str_desc_buf;

	struct usb_serial_port port[NUM_PORTS];
} usbtty;

static int                 serial_refcount;
static struct tty_driver   serial_tty_driver;
static struct tty_struct * serial_tty[NUM_PORTS];
static struct termios *    serial_termios[NUM_PORTS];
static struct termios *    serial_termios_locked[NUM_PORTS];

static struct usb_device_descriptor dev_desc = {
	bLength:USB_DT_DEVICE_SIZE,
	bDescriptorType:USB_DT_DEVICE,
	bcdUSB:USBDEV_REV,		//usb rev
	bDeviceClass:USB_CLASS_PER_INTERFACE,	//class    (none)
	bDeviceSubClass:0x00,	//subclass (none)
	bDeviceProtocol:0x00,	//protocol (none)
	bMaxPacketSize0:USBDEV_EP0_MAX_PACKET_SIZE, //max packet size for ep0
	idVendor:0x6d04,	//vendor  id
	idProduct:0x0bc0,	//product id
	bcdDevice:0x0001,	//BCD rev 0.1
	iManufacturer:0x01,	//manufactuer string index
	iProduct:0x02,		//product string index
	iSerialNumber:0x03,	//serial# string index
	bNumConfigurations:0x01	//num configurations
};

static struct usb_endpoint_descriptor ep_desc[] = {
	{
		// Bulk IN for Port 0
		bLength:USB_DT_ENDPOINT_SIZE,
		bDescriptorType:USB_DT_ENDPOINT,
		bEndpointAddress:USB_DIR_IN,
		bmAttributes:USB_ENDPOINT_XFER_BULK,
		wMaxPacketSize:IN_MAX_PACKET_SIZE,
		bInterval:0x00	// ignored for bulk
	},
	{
		// Bulk OUT for Port 0
		bLength:USB_DT_ENDPOINT_SIZE,
		bDescriptorType:USB_DT_ENDPOINT,
		bEndpointAddress:USB_DIR_OUT,
		bmAttributes:USB_ENDPOINT_XFER_BULK,
		wMaxPacketSize:OUT_MAX_PACKET_SIZE,
		bInterval:0x00	// ignored for bulk
	},
	{
		// Bulk IN for Port 1
		bLength:USB_DT_ENDPOINT_SIZE,
		bDescriptorType:USB_DT_ENDPOINT,
		bEndpointAddress:USB_DIR_IN,
		bmAttributes:USB_ENDPOINT_XFER_BULK,
		wMaxPacketSize:IN_MAX_PACKET_SIZE,
		bInterval:0x00	// ignored for bulk
	},
	{
		// Bulk OUT for Port 1
		bLength:USB_DT_ENDPOINT_SIZE,
		bDescriptorType:USB_DT_ENDPOINT,
		bEndpointAddress:USB_DIR_OUT,
		bmAttributes:USB_ENDPOINT_XFER_BULK,
		wMaxPacketSize:OUT_MAX_PACKET_SIZE,
		bInterval:0x00	// ignored for bulk
	}
};

static struct usb_interface_descriptor if_desc = {
	bLength:USB_DT_INTERFACE_SIZE,
	bDescriptorType:USB_DT_INTERFACE,
	bInterfaceNumber:0x00,
	bAlternateSetting:0x00,
	bNumEndpoints:NUM_EP,
	bInterfaceClass:0xff,
	bInterfaceSubClass:0xab,
	bInterfaceProtocol:0x00,
	iInterface:0x05
};

static struct usb_config_descriptor config_desc = {
	bLength:USB_DT_CONFIG_SIZE,
	bDescriptorType:USB_DT_CONFIG,
	wTotalLength:CONFIG_DESC_LEN,
	bNumInterfaces:0x01,
	bConfigurationValue:0x01,
	iConfiguration:0x04,	// configuration string
	bmAttributes:0xc0,	// self-powered
	MaxPower:20		// 40 mA
};

// String[0] is a list of Language IDs supported by this device
static struct usb_string_descriptor string_desc0 = {
	bLength:4,
	bDescriptorType:USB_DT_STRING,
	wData:{0x0409} // English, US
};

// These strings will be converted to Unicode in string_desc[]
static char *strings[5] = {
	"Alchemy Semiconductor",  // iManufacturer
	"WutzAMattaU",            // iProduct
	"1.0.doh!",               // iSerialNumber
	"Au1000 TTY Config",      // iConfiguration
	"Au1000 TTY Interface"    // iInterface
};

static inline int
port_paranoia_check(struct usb_serial_port *port, const char *function)
{
	if (!port) {
		err("%s: port is NULL", function);
		return -1;
	}
	if (!port->tty) {
		err("%s: port->tty is NULL", function);
		return -1;
	}

	return 0;
}


static void
port_rx_callback(struct usb_serial_port *port)
{
	dbg(__FUNCTION__ ": ep%d", port->out_ep_addr);
	// mark a bh to push this data up to the tty
	queue_task(&port->receive_complete_tq, &tq_immediate);
	mark_bh(IMMEDIATE_BH);
}

static void
port_tx_callback(struct usb_serial_port *port, usbdev_pkt_t* pkt)
{
	dbg(__FUNCTION__ ": ep%d", port->in_ep_addr);
	// mark a bh to wakeup any tty write system call on the port.
	queue_task(&port->send_complete_tq, &tq_immediate);
	mark_bh(IMMEDIATE_BH);

	/* free the returned packet */
	kfree(pkt);
}

static void
usbtty_callback(usbdev_cb_type_t cb_type, unsigned long arg, void* data)
{
	usbdev_pkt_t* pkt;
	int i;
	
	switch (cb_type) {
	case CB_NEW_STATE:
		dbg(__FUNCTION__ ": new dev_state=%d", (int)arg);
		usbtty.dev_state = (usbdev_state_t)arg;
		break;
	case CB_PKT_COMPLETE:
		pkt = (usbdev_pkt_t*)arg;
		for (i=0; i<NUM_PORTS; i++) {
			struct usb_serial_port *port = &usbtty.port[i];
			if (pkt->ep_addr == port->in_ep_addr) {
				port_tx_callback(port, pkt);
				break;
			} else if (pkt->ep_addr == port->out_ep_addr) {
				port_rx_callback(port);
				break;
			}
		}
		break;
	}
}


/*****************************************************************************
 * Here begins the tty driver interface functions
 *****************************************************************************/

static int serial_open(struct tty_struct *tty, struct file *filp)
{
	int portNumber;
	struct usb_serial_port *port;
	unsigned long flags;

	/* initialize the pointer incase something fails */
	tty->driver_data = NULL;

	MOD_INC_USE_COUNT;

	/* set up our port structure making the tty driver remember
	   our port object, and us it */
	portNumber = MINOR(tty->device);
	port = &usbtty.port[portNumber];
	tty->driver_data = port;
	port->tty = tty;

	if (usbtty.dev_state != CONFIGURED ||
	    port_paranoia_check(port, __FUNCTION__)) {
		/*
		 * the device-layer must be in the configured state before
		 * the function layer can operate.
		 */
		MOD_DEC_USE_COUNT;
		return -ENODEV;
	}
	
	dbg(__FUNCTION__ ": port %d", port->number);

	spin_lock_irqsave(&port->port_lock, flags);

	++port->open_count;

	if (!port->active) {
		port->active = 1;

		/*
		 * force low_latency on so that our tty_push actually forces
		 * the data through, otherwise it is scheduled, and with high
		 * data rates (like with OHCI) data can get lost.
		 */
		port->tty->low_latency = 1;

	}

	spin_unlock_irqrestore(&port->port_lock, flags);

	return 0;
}


static void serial_close(struct tty_struct *tty, struct file *filp)
{
	struct usb_serial_port *port =
		(struct usb_serial_port *) tty->driver_data;
	unsigned long flags;

	dbg(__FUNCTION__ ": port %d", port->number);

	if (!port->active) {
		err(__FUNCTION__ ": port not opened");
		return;
	}

	spin_lock_irqsave(&port->port_lock, flags);

	--port->open_count;

	if (port->open_count <= 0) {
		port->active = 0;
		port->open_count = 0;
	}

	spin_unlock_irqrestore(&port->port_lock, flags);
	MOD_DEC_USE_COUNT;
}


static int serial_write(struct tty_struct *tty, int from_user,
			const unsigned char *buf, int count)
{
	struct usb_serial_port *port =
		(struct usb_serial_port *) tty->driver_data;
	usbdev_pkt_t* pkt;
	int max_pkt_sz, ret;
	unsigned long flags;
	
	/*
	 * the device-layer must be in the configured state before the
	 * function layer can operate.
	 */
	if (usbtty.dev_state != CONFIGURED)
		return -ENODEV;

	if (!port->active) {
		err(__FUNCTION__ ": port not open");
		return -EINVAL;
	}

	if (count == 0) {
		dbg(__FUNCTION__ ": request of 0 bytes");
		return (0);
	}

#if 0
	if (port->writing) {
		dbg(__FUNCTION__ ": already writing");
		return 0;
	}
#endif
	
	max_pkt_sz = port->in_desc->wMaxPacketSize;
	count = (count > max_pkt_sz) ? max_pkt_sz : count;

	if ((ret = usbdev_alloc_packet(port->in_ep_addr, count, &pkt)))
		return ret;

	if (from_user)
		copy_from_user(pkt->payload, buf, count);
	else
		memcpy(pkt->payload, buf, count);
	
	ret = usbdev_send_packet(port->in_ep_addr, pkt);

	spin_lock_irqsave(&port->port_lock, flags);
	port->writing = 1;
	spin_unlock_irqrestore(&port->port_lock, flags);

	return ret;
}


static int serial_write_room(struct tty_struct *tty)
{
	struct usb_serial_port *port =
		(struct usb_serial_port *) tty->driver_data;
	int room = 0;
	
	/*
	 * the device-layer must be in the configured state before the
	 * function layer can operate.
	 */
	if (usbtty.dev_state != CONFIGURED)
		return -ENODEV;

	if (!port->active) {
		err(__FUNCTION__ ": port not open");
		return -EINVAL;
	}

	//room = port->writing ? 0 : port->in_desc->wMaxPacketSize;
	room = port->in_desc->wMaxPacketSize;
	
	dbg(__FUNCTION__ ": %d", room);
	return room;
}


static int serial_chars_in_buffer(struct tty_struct *tty)
{
	struct usb_serial_port *port =
		(struct usb_serial_port *) tty->driver_data;
	int chars = 0;
	
	/*
	 * the device-layer must be in the configured state before the
	 * function layer can operate.
	 */
	if (usbtty.dev_state != CONFIGURED)
		return -ENODEV;

	if (!port->active) {
		err(__FUNCTION__ ": port not open");
		return -EINVAL;
	}

	//chars = port->writing ? usbdev_get_byte_count(port->in_ep_addr) : 0;
	chars = usbdev_get_byte_count(port->in_ep_addr);

	dbg(__FUNCTION__ ": %d", chars);
	return chars;
}


static void serial_throttle(struct tty_struct *tty)
{
	struct usb_serial_port *port =
		(struct usb_serial_port *) tty->driver_data;

	if (!port->active || usbtty.dev_state != CONFIGURED) {
		err(__FUNCTION__ ": port not open");
		return;
	}

	// FIXME: anything to do?
	dbg(__FUNCTION__);
}


static void serial_unthrottle(struct tty_struct *tty)
{
	struct usb_serial_port *port =
		(struct usb_serial_port *) tty->driver_data;

	if (!port->active || usbtty.dev_state != CONFIGURED) {
		err(__FUNCTION__ ": port not open");
		return;
	}

	// FIXME: anything to do?
	dbg(__FUNCTION__);
}


static int serial_ioctl(struct tty_struct *tty, struct file *file,
			unsigned int cmd, unsigned long arg)
{
	struct usb_serial_port *port =
		(struct usb_serial_port *) tty->driver_data;

	if (!port->active) {
		err(__FUNCTION__ ": port not open");
		return -ENODEV;
	}
	// FIXME: need any IOCTLs?
	dbg(__FUNCTION__);

	return -ENOIOCTLCMD;
}


static void serial_set_termios(struct tty_struct *tty, struct termios *old)
{
	struct usb_serial_port *port =
		(struct usb_serial_port *) tty->driver_data;

	if (!port->active || usbtty.dev_state != CONFIGURED)  {
		err(__FUNCTION__ ": port not open");
		return;
	}

	dbg(__FUNCTION__);
	// FIXME: anything to do?
}


static void serial_break(struct tty_struct *tty, int break_state)
{
	struct usb_serial_port *port =
		(struct usb_serial_port *) tty->driver_data;

	if (!port->active || usbtty.dev_state != CONFIGURED)  {
		err(__FUNCTION__ ": port not open");
		return;
	}

	dbg(__FUNCTION__);
	// FIXME: anything to do?
}


static void port_send_complete(void *private)
{
	struct usb_serial_port *port = (struct usb_serial_port *) private;
	struct tty_struct *tty;
	unsigned long flags;

	dbg(__FUNCTION__ ": port %d, ep%d", port->number, port->in_ep_addr);

	tty = port->tty;
	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
	    tty->ldisc.write_wakeup) {
		dbg(__FUNCTION__ ": write wakeup call.");
		(tty->ldisc.write_wakeup) (tty);
	}

	wake_up_interruptible(&tty->write_wait);

	spin_lock_irqsave(&port->port_lock, flags);
	port->writing = usbdev_get_byte_count(port->in_ep_addr) <= 0 ? 0 : 1;
	spin_unlock_irqrestore(&port->port_lock, flags);
}


static void port_receive_complete(void *private)
{
	struct usb_serial_port *port = (struct usb_serial_port *) private;
	struct tty_struct *tty = port->tty;
	usbdev_pkt_t* pkt = NULL;
	int i, count;

	/* while there is a packet available */
	while ((count = usbdev_receive_packet(port->out_ep_addr,
					      &pkt)) != -ENODATA) {
		if (count < 0) {
			if (pkt)
				kfree(pkt);
			break; /* exit if error other than ENODATA */
		}
		
		dbg(__FUNCTION__ ": port %d, ep%d, size=%d",
		    port->number, port->out_ep_addr, count);

		for (i = 0; i < count; i++) {
			/* if we insert more than TTY_FLIPBUF_SIZE characters,
			   we drop them. */
			if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
				tty_flip_buffer_push(tty);
			}
			/* this doesn't actually push the data through
			   unless tty->low_latency is set */
			tty_insert_flip_char(tty, pkt->payload[i], 0);
		}
		tty_flip_buffer_push(tty);

		kfree(pkt); /* make sure we free the packet */
	}

}


static struct tty_driver serial_tty_driver = {
	magic:TTY_DRIVER_MAGIC,
	driver_name:"usbfn-tty",
	name:"usb/ttsdev/%d",
	major:SERIAL_TTY_MAJOR,
	minor_start:0,
	num:NUM_PORTS,
	type:TTY_DRIVER_TYPE_SERIAL,
	subtype:SERIAL_TYPE_NORMAL,
	flags:TTY_DRIVER_REAL_RAW | TTY_DRIVER_NO_DEVFS,
	refcount:&serial_refcount,
	table:serial_tty,
	termios:serial_termios,
	termios_locked:serial_termios_locked,

	open:serial_open,
	close:serial_close,
	write:serial_write,
	write_room:serial_write_room,
	ioctl:serial_ioctl,
	set_termios:serial_set_termios,
	throttle:serial_throttle,
	unthrottle:serial_unthrottle,
	break_ctl:serial_break,
	chars_in_buffer:serial_chars_in_buffer,
};


void usbfn_tty_exit(void)
{
	int i;
	
	/* kill the device layer */
	usbdev_exit();

	for (i=0; i < NUM_PORTS; i++) {
		tty_unregister_devfs(&serial_tty_driver, i);
		info("usb serial converter now disconnected from ttyUSBdev%d",
		     i);
	}

	tty_unregister_driver(&serial_tty_driver);

	if (usbtty.str_desc_buf)
		kfree(usbtty.str_desc_buf);
}


int usbfn_tty_init(void)
{
	int ret = 0, i, str_desc_len;

	/* register the tty driver */
	serial_tty_driver.init_termios = tty_std_termios;
	serial_tty_driver.init_termios.c_cflag =
		B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	
	if (tty_register_driver(&serial_tty_driver)) {
		err(__FUNCTION__ ": failed to register tty driver");
		ret = -ENXIO;
		goto out;
	}

	/*
	 * initialize pointers to descriptors
	 */
	usbtty.dev_desc = &dev_desc;
	usbtty.config_desc = &config_desc;
	usbtty.if_desc = &if_desc;

	/*
	 * initialize the string descriptors
	 */

	/* alloc buffer big enough for all string descriptors */
	str_desc_len = string_desc0.bLength;
	for (i = 0; i < 5; i++)
		str_desc_len += 2 + 2 * strlen(strings[i]);
	usbtty.str_desc_buf = (void *) kmalloc(str_desc_len, GFP_KERNEL);
	if (!usbtty.str_desc_buf) {
		err(__FUNCTION__ ": failed to alloc string descriptors");
		ret = -ENOMEM;
		goto out;
	}

	usbtty.str_desc[0] =
		(struct usb_string_descriptor *)usbtty.str_desc_buf;
	memcpy(usbtty.str_desc[0], &string_desc0, string_desc0.bLength);
	usbtty.str_desc[1] = (struct usb_string_descriptor *)
		(usbtty.str_desc_buf + string_desc0.bLength);
	for (i = 1; i < 6; i++) {
		struct usb_string_descriptor *desc = usbtty.str_desc[i];
		char *str = strings[i - 1];
		int j, str_len = strlen(str);

		desc->bLength = 2 + 2 * str_len;
		desc->bDescriptorType = USB_DT_STRING;
		for (j = 0; j < str_len; j++) {
			desc->wData[j] = (u16) str[j];
		}
		if (i < 5)
			usbtty.str_desc[i + 1] =
				(struct usb_string_descriptor *)
				((u8 *) desc + desc->bLength);
	}

	/*
	 * start the device layer. The device layer assigns us
	 * our endpoint addresses
	 */
	if ((ret = usbdev_init(&dev_desc, &config_desc, &if_desc, ep_desc,
			       usbtty.str_desc, usbtty_callback, NULL))) {
		err(__FUNCTION__ ": device-layer init failed");
		goto out;
	}
	
	/* initialize the devfs nodes for this device and let the user
	   know what ports we are bound to */
	for (i = 0; i < NUM_PORTS; ++i) {
		struct usb_serial_port *port;
		tty_register_devfs(&serial_tty_driver, 0, i);
		info("usbdev serial attached to ttyUSBdev%d "
		     "(or devfs usb/ttsdev/%d)", i, i);
		port = &usbtty.port[i];
		port->number = i;
		port->in_desc = &ep_desc[NUM_PORTS*i];
		port->out_desc = &ep_desc[NUM_PORTS*i + 1];
		port->in_ep_addr = port->in_desc->bEndpointAddress & 0x0f;
		port->out_ep_addr = port->out_desc->bEndpointAddress & 0x0f;
		port->send_complete_tq.routine = port_send_complete;
		port->send_complete_tq.data = port;
		port->receive_complete_tq.routine = port_receive_complete;
		port->receive_complete_tq.data = port;
		spin_lock_init(&port->port_lock);
	}

 out:
	if (ret)
		usbfn_tty_exit();
	return ret;
}


/* Module information */
MODULE_AUTHOR("Steve Longerbeam, stevel@mvista.com, www.mvista.com");
MODULE_DESCRIPTION("Au1x00 USB Device-Side Serial TTY Driver");
MODULE_LICENSE("GPL");

module_init(usbfn_tty_init);
module_exit(usbfn_tty_exit);
