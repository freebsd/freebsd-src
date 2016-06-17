/*
 * BRIEF MODULE DESCRIPTION
 *	Au1x00 USB Device-Side Raw Block Driver (function layer)
 *
 * Copyright 2001-2002 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *		stevel@mvista.com or source@mvista.com
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

#define USBRAW_MAJOR 190 // FIXME: need a legal major
#define USBRAW_NAME "usbraw"

#define MAX_NUM_PORTS 2

#define IN_MAX_PACKET_SIZE  64
#define OUT_MAX_PACKET_SIZE 64

// FIXME: when Au1x00 endpoints 3 and 5 are fixed, make NUM_PORTS=2
#define NUM_PORTS 1
#define NUM_EP 2*NUM_PORTS

#define CONFIG_DESC_LEN \
 USB_DT_CONFIG_SIZE + USB_DT_INTERFACE_SIZE + NUM_EP*USB_DT_ENDPOINT_SIZE

/* must be power of two */
#define READ_BUF_SIZE (1<<12)

struct usb_raw_port {
	unsigned char number;
	spinlock_t port_lock;

	struct usb_endpoint_descriptor* out_desc;
	struct usb_endpoint_descriptor* in_desc;

	int out_ep_addr; /* endpoint address of OUT endpoint */
	int in_ep_addr;  /* endpoint address of IN endpoint */
	
	__u8 read_buf[READ_BUF_SIZE]; // FIXME: allocate with get_free_pages
	int read_nextin, read_nextout;
	int read_count;

	wait_queue_head_t wait;
	struct fasync_struct *fasync;     // asynch notification

	int active;	/* someone has this device open */
	int open_count;	/* number of times this port has been opened */
};

static struct usb_serial {
	struct usb_device_descriptor* dev_desc;
	struct usb_config_descriptor* config_desc;
	struct usb_interface_descriptor* if_desc;
	struct usb_string_descriptor * str_desc[6];
	void* str_desc_buf;

	usbdev_state_t dev_state;

	struct usb_raw_port port[NUM_PORTS];
} usbraw;

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
	"USB Raw Block Device",   // iProduct
	"0.1",                    // iSerialNumber
	"USB Raw Config",         // iConfiguration
	"USB Raw Interface"       // iInterface
};


static void
receive_callback(struct usb_raw_port *port)
{
	int i, pkt_size;
	usbdev_pkt_t* pkt;
	
	if ((pkt_size = usbdev_receive_packet(port->out_ep_addr,
					      &pkt)) <= 0) {
		dbg(__FUNCTION__ ": usbdev_receive_packet returns %d",
		    pkt_size);
		return;
	}

	dbg(__FUNCTION__ ": ep%d, size=%d", port->out_ep_addr, pkt_size);

	spin_lock(&port->port_lock);
	for (i=0; i < pkt_size; i++) {
		port->read_buf[port->read_nextin++] = pkt->payload[i];
		port->read_nextin &= (READ_BUF_SIZE - 1);
		if (++port->read_count == READ_BUF_SIZE)
			break;
	}
	spin_unlock(&port->port_lock);

	/* free the packet */
	kfree(pkt);
	
	// async notify
	if (port->fasync)
		kill_fasync(&port->fasync, SIGIO, POLL_IN);
	// wake up any read call
	if (waitqueue_active(&port->wait))
		wake_up_interruptible(&port->wait);
}

static void
transmit_callback(struct usb_raw_port *port, usbdev_pkt_t* pkt)
{
	dbg(__FUNCTION__ ": ep%d", port->in_ep_addr);
	/* just free the returned packet */
	kfree(pkt);
}


static void
usbraw_callback(usbdev_cb_type_t cb_type, unsigned long arg, void* data)
{
	usbdev_pkt_t* pkt;
	int i;
	
	switch (cb_type) {
	case CB_NEW_STATE:
		usbraw.dev_state = (usbdev_state_t)arg;
		break;
	case CB_PKT_COMPLETE:
		pkt = (usbdev_pkt_t*)arg;
		for (i=0; i<NUM_PORTS; i++) {
			struct usb_raw_port *port = &usbraw.port[i];
			if (pkt->ep_addr == port->in_ep_addr) {
				transmit_callback(port, pkt);
				break;
			} else if (pkt->ep_addr == port->out_ep_addr) {
				receive_callback(port);
				break;
			}
		}
		break;
	}
}

/*****************************************************************************
 * Here begins the driver interface functions
 *****************************************************************************/

static unsigned int usbraw_poll(struct file * filp, poll_table * wait)
{
	struct usb_raw_port *port = (struct usb_raw_port *)filp->private_data;
	unsigned long flags;
	int count;
	
	poll_wait(filp, &port->wait, wait);

	spin_lock_irqsave(&port->port_lock, flags);
	count = port->read_count;
	spin_unlock_irqrestore(&port->port_lock, flags);

	if (count > 0) {
		dbg(__FUNCTION__ ": count=%d", count);
		return POLLIN | POLLRDNORM;
	}
	
	return 0;
}

static int usbraw_fasync(int fd, struct file *filp, int mode)
{
	struct usb_raw_port *port = (struct usb_raw_port *)filp->private_data;
	return fasync_helper(fd, filp, mode, &port->fasync);
}

static int usbraw_open(struct inode * inode, struct file *filp)
{
	int portNumber;
	struct usb_raw_port *port;
	unsigned long flags;

	/*
	 * the device-layer must be in the configured state before the
	 * function layer can operate.
	 */
	if (usbraw.dev_state != CONFIGURED)
		return -ENODEV;
	
	MOD_INC_USE_COUNT;

	/* set up our port structure making the tty driver remember
	   our port object, and us it */
	portNumber = MINOR(inode->i_rdev);
	port = &usbraw.port[portNumber];
	filp->private_data = port;

	dbg(__FUNCTION__ ": port %d", port->number);

	spin_lock_irqsave(&port->port_lock, flags);

	++port->open_count;

	if (!port->active) {
		port->active = 1;
	}

	/* flush read buffer */
	port->read_nextin = port->read_nextout = port->read_count = 0;

	spin_unlock_irqrestore(&port->port_lock, flags);

	return 0;
}

static int usbraw_release(struct inode * inode, struct file * filp)
{
	struct usb_raw_port *port = (struct usb_raw_port *)filp->private_data;
	unsigned long flags;

	dbg(__FUNCTION__ ": port %d", port->number);

	if (!port->active) {
		err(__FUNCTION__ ": port not opened");
		return -ENODEV;
	}

	usbraw_fasync(-1, filp, 0);

	spin_lock_irqsave(&port->port_lock, flags);

	--port->open_count;

	if (port->open_count <= 0) {
		port->active = 0;
		port->open_count = 0;
	}

	spin_unlock_irqrestore(&port->port_lock, flags);
	MOD_DEC_USE_COUNT;
	return 0;
}


static ssize_t usbraw_read(struct file * filp, char * buf,
			   size_t count, loff_t * l)
{
	struct usb_raw_port *port = (struct usb_raw_port *)filp->private_data;
	unsigned long flags;
	int i, cnt;

	/*
	 * the device-layer must be in the configured state before the
	 * function layer can operate.
	 */
	if (usbraw.dev_state != CONFIGURED)
		return -ENODEV;

	do { 
		spin_lock_irqsave(&port->port_lock, flags);
		cnt = port->read_count;
		spin_unlock_irqrestore(&port->port_lock, flags);
		if (cnt == 0) {
			if (filp->f_flags & O_NONBLOCK)
				return -EAGAIN;
			interruptible_sleep_on(&port->wait);
			if (signal_pending(current))
				return -ERESTARTSYS;
		}
	} while (cnt == 0);

	count = (count > cnt) ? cnt : count;

	for (i=0; i<count; i++) {
		put_user(port->read_buf[port->read_nextout++], &buf[i]);
		port->read_nextout &= (READ_BUF_SIZE - 1);
		spin_lock_irqsave(&port->port_lock, flags);
		port->read_count--;
		spin_unlock_irqrestore(&port->port_lock, flags);
		if (port->read_count == 0)
			break;
	}

	return i+1;
}

static ssize_t usbraw_write(struct file * filp, const char * buf,
			    size_t count, loff_t *ppos)
{
	struct usb_raw_port *port = (struct usb_raw_port *)filp->private_data;
	usbdev_pkt_t* pkt;
	int ret, max_pkt_sz;
	
	/*
	 * the device-layer must be in the configured state before the
	 * function layer can operate.
	 */
	if (usbraw.dev_state != CONFIGURED)
		return -ENODEV;

	if (!port->active) {
		err(__FUNCTION__ ": port not opened");
		return -EINVAL;
	}

	if (count == 0) {
		dbg(__FUNCTION__ ": write request of 0 bytes");
		return (0);
	}

	max_pkt_sz = port->in_desc->wMaxPacketSize;
	count = (count > max_pkt_sz) ? max_pkt_sz : count;

	if ((ret = usbdev_alloc_packet(port->in_ep_addr, count, &pkt)) < 0)
		return ret;

	copy_from_user(pkt->payload, buf, count);
	
	return usbdev_send_packet(port->in_ep_addr, pkt);
}

static int usbraw_ioctl(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	struct usb_raw_port *port = (struct usb_raw_port *)filp->private_data;

	if (!port->active) {
		err(__FUNCTION__ ": port not open");
		return -ENODEV;
	}
	// FIXME: need any IOCTLs?

	return -ENOIOCTLCMD;
}


static struct file_operations usbraw_fops = {
        owner:          THIS_MODULE,
        write:          usbraw_write,
	read:           usbraw_read,
	poll:           usbraw_poll,
	ioctl:		usbraw_ioctl,
	fasync:         usbraw_fasync,
	open:		usbraw_open,
	release:	usbraw_release,
};

void usbfn_raw_exit(void)
{
	/* kill the device layer */
	usbdev_exit();

	unregister_chrdev(USBRAW_MAJOR, USBRAW_NAME);

	if (usbraw.str_desc_buf)
		kfree(usbraw.str_desc_buf);
}


int usbfn_raw_init(void)
{
	int ret = 0, i, str_desc_len;

	/* register our character device */
	if ((ret = register_chrdev(USBRAW_MAJOR, USBRAW_NAME,
				   &usbraw_fops)) < 0) {
		err("can't get major number");
		return ret;
	}
	info("registered");

	/*
	 * initialize pointers to descriptors
	 */
	usbraw.dev_desc = &dev_desc;
	usbraw.config_desc = &config_desc;
	usbraw.if_desc = &if_desc;

	/*
	 * initialize the string descriptors
	 */

	/* alloc buffer big enough for all string descriptors */
	str_desc_len = string_desc0.bLength;
	for (i = 0; i < 5; i++)
		str_desc_len += 2 + 2 * strlen(strings[i]);
	usbraw.str_desc_buf = (void *) kmalloc(str_desc_len, GFP_KERNEL);
	if (!usbraw.str_desc_buf) {
		err(__FUNCTION__ ": failed to alloc string descriptors");
		ret = -ENOMEM;
		goto out;
	}

	usbraw.str_desc[0] =
		(struct usb_string_descriptor *)usbraw.str_desc_buf;
	memcpy(usbraw.str_desc[0], &string_desc0, string_desc0.bLength);
	usbraw.str_desc[1] = (struct usb_string_descriptor *)
		(usbraw.str_desc_buf + string_desc0.bLength);
	for (i = 1; i < 6; i++) {
		struct usb_string_descriptor *desc = usbraw.str_desc[i];
		char *str = strings[i - 1];
		int j, str_len = strlen(str);

		desc->bLength = 2 + 2 * str_len;
		desc->bDescriptorType = USB_DT_STRING;
		for (j = 0; j < str_len; j++) {
			desc->wData[j] = (u16) str[j];
		}
		if (i < 5)
			usbraw.str_desc[i + 1] =
				(struct usb_string_descriptor *)
				((u8 *) desc + desc->bLength);
	}

	/*
	 * start the device layer. The device layer assigns us
	 * our endpoint addresses
	 */
	if ((ret = usbdev_init(&dev_desc, &config_desc, &if_desc, ep_desc,
			       usbraw.str_desc, usbraw_callback, NULL))) {
		err(__FUNCTION__ ": device-layer init failed");
		goto out;
	}
	
	/* initialize the devfs nodes for this device and let the user
	   know what ports we are bound to */
	for (i = 0; i < NUM_PORTS; ++i) {
		struct usb_raw_port *port = &usbraw.port[i];

		port->number = i;
		port->in_desc = &ep_desc[NUM_PORTS*i];
		port->out_desc = &ep_desc[NUM_PORTS*i + 1];
		port->in_ep_addr = port->in_desc->bEndpointAddress & 0x0f;
		port->out_ep_addr = port->out_desc->bEndpointAddress & 0x0f;
		init_waitqueue_head(&port->wait);
		spin_lock_init(&port->port_lock);
	}

 out:
	if (ret)
		usbfn_raw_exit();
	return ret;
}


/* Module information */
MODULE_AUTHOR("Steve Longerbeam, stevel@mvista.com, www.mvista.com");
MODULE_DESCRIPTION("Au1x00 USB Device-Side Raw Block Driver");
MODULE_LICENSE("GPL");

module_init(usbfn_raw_init);
module_exit(usbfn_raw_exit);
