/****************************************************************
 *
 *     kaweth.c - driver for KL5KUSB101 based USB->Ethernet
 *
 *     (c) 2000 Interlan Communications
 *     (c) 2000 Stephane Alnet
 *     (C) 2001 Brad Hards
 *     (C) 2002 Oliver Neukum
 *
 *     Original author: The Zapman <zapman@interlan.net>
 *     Inspired by, and much credit goes to Michael Rothwell
 *     <rothwell@interlan.net> for the test equipment, help, and patience
 *     Based off of (and with thanks to) Petko Manolov's pegaus.c driver.
 *     Also many thanks to Joel Silverman and Ed Surprenant at Kawasaki
 *     for providing the firmware and driver resources.
 *
 *     This program is free software; you can redistribute it and/or
 *     modify it under the terms of the GNU General Public License as
 *     published by the Free Software Foundation; either version 2, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software Foundation,
 *     Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 ****************************************************************/

/* TODO:
 * Fix in_interrupt() problem
 * Develop test procedures for USB net interfaces
 * Run test procedures
 * Fix bugs from previous two steps
 * Snoop other OSs for any tricks we're not doing
 * SMP locking
 * Reduce arbitrary timeouts
 * Smart multicast support
 * Temporary MAC change support
 * Tunable SOFs parameter - ioctl()?
 * Ethernet stats collection
 * Code formatting improvements
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/usb.h>
#include <linux/types.h>
#include <linux/ethtool.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>
#include <asm/byteorder.h>

#define DEBUG

#ifdef DEBUG
#define kaweth_dbg(format, arg...) printk(KERN_DEBUG __FILE__ ": " format "\n" ,##arg)
#else
#define kaweth_dbg(format, arg...) do {} while (0)
#endif
#define kaweth_err(format, arg...) printk(KERN_ERR __FILE__ ": " format "\n" ,##arg)
#define kaweth_info(format, arg...) printk(KERN_INFO __FILE__ ": " format "\n" , ##arg)
#define kaweth_warn(format, arg...) printk(KERN_WARNING __FILE__ ": " format "\n" , ##arg)


#include "kawethfw.h"

#define KAWETH_MTU			1500
#define KAWETH_BUF_SIZE			1664
#define KAWETH_TX_TIMEOUT		(5 * HZ)
#define KAWETH_SCRATCH_SIZE		32
#define KAWETH_FIRMWARE_BUF_SIZE	4096
#define KAWETH_CONTROL_TIMEOUT		(30 * HZ)

#define KAWETH_STATUS_BROKEN		0x0000001
#define KAWETH_STATUS_CLOSING		0x0000002

#define KAWETH_PACKET_FILTER_PROMISCUOUS	0x01
#define KAWETH_PACKET_FILTER_ALL_MULTICAST	0x02
#define KAWETH_PACKET_FILTER_DIRECTED		0x04
#define KAWETH_PACKET_FILTER_BROADCAST		0x08
#define KAWETH_PACKET_FILTER_MULTICAST		0x10

/* Table 7 */
#define KAWETH_COMMAND_GET_ETHERNET_DESC	0x00
#define KAWETH_COMMAND_MULTICAST_FILTERS        0x01
#define KAWETH_COMMAND_SET_PACKET_FILTER	0x02
#define KAWETH_COMMAND_STATISTICS               0x03
#define KAWETH_COMMAND_SET_TEMP_MAC     	0x06
#define KAWETH_COMMAND_GET_TEMP_MAC             0x07
#define KAWETH_COMMAND_SET_URB_SIZE		0x08
#define KAWETH_COMMAND_SET_SOFS_WAIT		0x09
#define KAWETH_COMMAND_SCAN			0xFF

#define KAWETH_SOFS_TO_WAIT			0x05

#define INTBUFFERSIZE				4

#define STATE_OFFSET				0
#define STATE_MASK				0x40
#define	STATE_SHIFT				5


MODULE_AUTHOR("Michael Zappe <zapman@interlan.net>, Stephane Alnet <stephane@u-picardie.fr>, Brad Hards <bhards@bigpond.net.au> and Oliver Neukum <oliver@neukum.org>");
MODULE_DESCRIPTION("KL5USB101 USB Ethernet driver");
MODULE_LICENSE("GPL");

static const char driver_name[] = "kaweth";

static void *kaweth_probe(
            struct usb_device *dev,             /* the device */
            unsigned ifnum,                     /* what interface */
            const struct usb_device_id *id      /* from id_table */
	);
static void kaweth_disconnect(struct usb_device *dev, void *ptr);
int kaweth_internal_control_msg(struct usb_device *usb_dev, unsigned int pipe,
				struct usb_ctrlrequest *cmd, void *data,
				int len, int timeout);

/****************************************************************
 *     usb_device_id
 ****************************************************************/
static struct usb_device_id usb_klsi_table[] = {
	{ USB_DEVICE(0x03e8, 0x0008) }, /* AOX Endpoints USB Ethernet */
	{ USB_DEVICE(0x04bb, 0x0901) }, /* I-O DATA USB-ET/T */
	{ USB_DEVICE(0x0506, 0x03e8) }, /* 3Com 3C19250 */
	{ USB_DEVICE(0x0506, 0x11f8) }, /* 3Com 3C460 */
	{ USB_DEVICE(0x0557, 0x2002) }, /* ATEN USB Ethernet */
	{ USB_DEVICE(0x0557, 0x4000) }, /* D-Link DSB-650C */
	{ USB_DEVICE(0x0565, 0x0002) }, /* Peracom Enet */
	{ USB_DEVICE(0x0565, 0x0003) }, /* Optus@Home UEP1045A */
	{ USB_DEVICE(0x0565, 0x0005) }, /* Peracom Enet2 */
	{ USB_DEVICE(0x05e9, 0x0008) }, /* KLSI KL5KUSB101B */
	{ USB_DEVICE(0x05e9, 0x0009) }, /* KLSI KL5KUSB101B (Board change) */
	{ USB_DEVICE(0x066b, 0x2202) }, /* Linksys USB10T */
	{ USB_DEVICE(0x06e1, 0x0008) }, /* ADS USB-10BT */
	{ USB_DEVICE(0x06e1, 0x0009) }, /* ADS USB-10BT */
	{ USB_DEVICE(0x0707, 0x0100) }, /* SMC 2202USB */
	{ USB_DEVICE(0x0785, 0x0002) }, /* NTT-TE MN128 USB-Ethernet Adapter */
	{ USB_DEVICE(0x07aa, 0x0001) }, /* Correga K.K. */
	{ USB_DEVICE(0x07b8, 0x4000) }, /* D-Link DU-E10 */
	{ USB_DEVICE(0x0846, 0x1001) }, /* NetGear EA-101 */
	{ USB_DEVICE(0x0846, 0x1002) }, /* NetGear EA-101 */
	{ USB_DEVICE(0x085a, 0x0008) }, /* PortGear Ethernet Adapter */
	{ USB_DEVICE(0x085a, 0x0009) }, /* PortGear Ethernet Adapter */
	{ USB_DEVICE(0x087d, 0x5704) }, /* Jaton USB Ethernet Device Adapter */
	{ USB_DEVICE(0x0951, 0x0008) }, /* Kingston Technology USB Ethernet Adapter */
	{ USB_DEVICE(0x095a, 0x3003) }, /* Portsmith Express Ethernet Adapter */
	{ USB_DEVICE(0x10bd, 0x1427) }, /* ASANTE USB To Ethernet Adapter */
	{ USB_DEVICE(0x1342, 0x0204) }, /* Mobility USB-Ethernet Adapter */
	{ USB_DEVICE(0x13d2, 0x0400) }, /* Shark Pocket Adapter */
	{ USB_DEVICE(0x1485, 0x0001) },	/* Silicom U2E */
	{ USB_DEVICE(0x1645, 0x0005) }, /* Entrega E45 */
	{ USB_DEVICE(0x1645, 0x0008) }, /* Entrega USB Ethernet Adapter */
	{ USB_DEVICE(0x1645, 0x8005) }, /* PortGear Ethernet Adapter */
	{ USB_DEVICE(0x2001, 0x4000) }, /* D-link DSB-650C */
	{} /* Null terminator */
};

MODULE_DEVICE_TABLE (usb, usb_klsi_table);

/****************************************************************
 *     kaweth_driver
 ****************************************************************/
static struct usb_driver kaweth_driver = {
	.name =		driver_name,
	.probe =	kaweth_probe,
	.disconnect =	kaweth_disconnect,
	.id_table =     usb_klsi_table,
};

typedef __u8 eth_addr_t[6];

/****************************************************************
 *     usb_eth_dev
 ****************************************************************/
struct usb_eth_dev {
	char *name;
	__u16 vendor;
	__u16 device;
	void *pdata;
};

/****************************************************************
 *     kaweth_ethernet_configuration
 *     Refer Table 8
 ****************************************************************/
struct kaweth_ethernet_configuration
{
	__u8 size;
	__u8 reserved1;
	__u8 reserved2;
	eth_addr_t hw_addr;
	__u32 statistics_mask;
	__u16 segment_size;
	__u16 max_multicast_filters;
	__u8 reserved3;
} __attribute__ ((packed));

/****************************************************************
 *     kaweth_device
 ****************************************************************/
struct kaweth_device
{
	spinlock_t device_lock;

	__u32 status;
	int end;
	int removed;
	int suspend_lowmem;
	int linkstate;

	struct usb_device *dev;
	struct net_device *net;
	wait_queue_head_t term_wait;

	struct urb *rx_urb;
	struct urb *tx_urb;
	struct urb *irq_urb;
	
	struct sk_buff *tx_skb;

	__u8 *firmware_buf;
	__u8 scratch[KAWETH_SCRATCH_SIZE];
	__u8 rx_buf[KAWETH_BUF_SIZE];
	__u8 intbuffer[INTBUFFERSIZE];
	__u16 packet_filter_bitmap;

	struct kaweth_ethernet_configuration configuration;

	struct net_device_stats stats;
} __attribute__ ((packed));


/****************************************************************
 *     kaweth_control
 ****************************************************************/
static int kaweth_control(struct kaweth_device *kaweth,
			  unsigned int pipe,
			  __u8 request,
			  __u8 requesttype,
			  __u16 value,
			  __u16 index,
			  void *data,
			  __u16 size,
			  int timeout)
{
	struct usb_ctrlrequest *dr;

	kaweth_dbg("kaweth_control()");

	if(in_interrupt()) {
		kaweth_dbg("in_interrupt()");
		return -EBUSY;
	}

	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_ATOMIC);

	if (!dr) {
		kaweth_dbg("kmalloc() failed");
		return -ENOMEM;
	}

	dr->bRequestType= requesttype;
	dr->bRequest = request;
	dr->wValue = cpu_to_le16p(&value);
	dr->wIndex = cpu_to_le16p(&index);
	dr->wLength = cpu_to_le16p(&size);

	return kaweth_internal_control_msg(kaweth->dev,
					pipe,
					dr,
					data,
					size,
					timeout);
}

/****************************************************************
 *     kaweth_read_configuration
 ****************************************************************/
static int kaweth_read_configuration(struct kaweth_device *kaweth)
{
	int retval;

	kaweth_dbg("Reading kaweth configuration");

	retval = kaweth_control(kaweth,
				usb_rcvctrlpipe(kaweth->dev, 0),
				KAWETH_COMMAND_GET_ETHERNET_DESC,
				USB_TYPE_VENDOR | USB_DIR_IN | USB_RECIP_DEVICE,
				0,
				0,
				(void *)&kaweth->configuration,
				sizeof(kaweth->configuration),
				KAWETH_CONTROL_TIMEOUT);

	return retval;
}

/****************************************************************
 *     kaweth_set_urb_size
 ****************************************************************/
static int kaweth_set_urb_size(struct kaweth_device *kaweth, __u16 urb_size)
{
	int retval;

	kaweth_dbg("Setting URB size to %d", (unsigned)urb_size);

	retval = kaweth_control(kaweth,
				usb_sndctrlpipe(kaweth->dev, 0),
				KAWETH_COMMAND_SET_URB_SIZE,
				USB_TYPE_VENDOR | USB_DIR_OUT | USB_RECIP_DEVICE,
				urb_size,
				0,
				(void *)&kaweth->scratch,
				0,
				KAWETH_CONTROL_TIMEOUT);

	return retval;
}

/****************************************************************
 *     kaweth_set_sofs_wait
 ****************************************************************/
static int kaweth_set_sofs_wait(struct kaweth_device *kaweth, __u16 sofs_wait)
{
	int retval;

	kaweth_dbg("Set SOFS wait to %d", (unsigned)sofs_wait);

	retval = kaweth_control(kaweth,
				usb_sndctrlpipe(kaweth->dev, 0),
				KAWETH_COMMAND_SET_SOFS_WAIT,
				USB_TYPE_VENDOR | USB_DIR_OUT | USB_RECIP_DEVICE,
				sofs_wait,
				0,
				(void *)&kaweth->scratch,
				0,
				KAWETH_CONTROL_TIMEOUT);

	return retval;
}

/****************************************************************
 *     kaweth_set_receive_filter
 ****************************************************************/
static int kaweth_set_receive_filter(struct kaweth_device *kaweth,
				     __u16 receive_filter)
{
	int retval;

	kaweth_dbg("Set receive filter to %d", (unsigned)receive_filter);

	retval = kaweth_control(kaweth,
				usb_sndctrlpipe(kaweth->dev, 0),
				KAWETH_COMMAND_SET_PACKET_FILTER,
				USB_TYPE_VENDOR | USB_DIR_OUT | USB_RECIP_DEVICE,
				receive_filter,
				0,
				(void *)&kaweth->scratch,
				0,
				KAWETH_CONTROL_TIMEOUT);

	return retval;
}

/****************************************************************
 *     kaweth_download_firmware
 ****************************************************************/
static int kaweth_download_firmware(struct kaweth_device *kaweth,
				    __u8 *data,
				    __u16 data_len,
				    __u8 interrupt,
				    __u8 type)
{
	if(data_len > KAWETH_FIRMWARE_BUF_SIZE)	{
		kaweth_err("Firmware too big: %d", data_len);
		return -ENOSPC;
	}

	memcpy(kaweth->firmware_buf, data, data_len);

	kaweth->firmware_buf[2] = (data_len & 0xFF) - 7;
	kaweth->firmware_buf[3] = data_len >> 8;
	kaweth->firmware_buf[4] = type;
	kaweth->firmware_buf[5] = interrupt;

	kaweth_dbg("High: %i, Low:%i", kaweth->firmware_buf[3],
		   kaweth->firmware_buf[2]);

	kaweth_dbg("Downloading firmware at %p to kaweth device at %p",
	    data,
	    kaweth);
	kaweth_dbg("Firmware length: %d", data_len);

	return kaweth_control(kaweth,
		              usb_sndctrlpipe(kaweth->dev, 0),
			      KAWETH_COMMAND_SCAN,
			      USB_TYPE_VENDOR | USB_DIR_OUT | USB_RECIP_DEVICE,
			      0,
			      0,
			      (void *)kaweth->firmware_buf,
			      data_len,
			      KAWETH_CONTROL_TIMEOUT);
}

/****************************************************************
 *     kaweth_trigger_firmware
 ****************************************************************/
static int kaweth_trigger_firmware(struct kaweth_device *kaweth,
				   __u8 interrupt)
{
	kaweth->firmware_buf[0] = 0xB6;
	kaweth->firmware_buf[1] = 0xC3;
	kaweth->firmware_buf[2] = 0x01;
	kaweth->firmware_buf[3] = 0x00;
	kaweth->firmware_buf[4] = 0x06;
	kaweth->firmware_buf[5] = interrupt;
	kaweth->firmware_buf[6] = 0x00;
	kaweth->firmware_buf[7] = 0x00;

	kaweth_dbg("Triggering firmware");

	return kaweth_control(kaweth,
			      usb_sndctrlpipe(kaweth->dev, 0),
			      KAWETH_COMMAND_SCAN,
			      USB_TYPE_VENDOR | USB_DIR_OUT | USB_RECIP_DEVICE,
			      0,
			      0,
			      (void *)kaweth->firmware_buf,
			      8,
			      KAWETH_CONTROL_TIMEOUT);
}

/****************************************************************
 *     kaweth_reset
 ****************************************************************/
static int kaweth_reset(struct kaweth_device *kaweth)
{
	int result;

	kaweth_dbg("kaweth_reset(%p)", kaweth);
	result = kaweth_control(kaweth,
				usb_sndctrlpipe(kaweth->dev, 0),
				USB_REQ_SET_CONFIGURATION,
				0,
				kaweth->dev->config[0].bConfigurationValue,
				0,
				NULL,
				0,
				KAWETH_CONTROL_TIMEOUT);

	udelay(10000);

	kaweth_dbg("kaweth_reset() returns %d.",result);

	return result;
}

static void kaweth_usb_receive(struct urb *);
static void kaweth_resubmit_rx_urb(struct kaweth_device *);

/****************************************************************
	int_callback
*****************************************************************/
static void int_callback(struct urb *u)
{
	struct kaweth_device *kaweth = u->context;
	int act_state;

	/* we abuse the interrupt urb for rebsubmitting under low memory saving a timer */
	if (kaweth->suspend_lowmem)
		kaweth_resubmit_rx_urb(kaweth);

	/* we check the link state to report changes */
	if (kaweth->linkstate != (act_state = ( kaweth->intbuffer[STATE_OFFSET] | STATE_MASK) >> STATE_SHIFT)) {
		if (!act_state)
			netif_carrier_on(kaweth->net);
		else
			netif_carrier_off(kaweth->net);

		kaweth->linkstate = act_state;
	}

}

/****************************************************************
 *     kaweth_resubmit_rx_urb
 ****************************************************************/
static void kaweth_resubmit_rx_urb(struct kaweth_device *kaweth)
{
	int result;
	long flags;

	FILL_BULK_URB(kaweth->rx_urb,
		      kaweth->dev,
		      usb_rcvbulkpipe(kaweth->dev, 1),
		      kaweth->rx_buf,
		      KAWETH_BUF_SIZE,
		      kaweth_usb_receive,
		      kaweth);

	spin_lock_irqsave(&kaweth->device_lock, flags);
	if (!kaweth->removed) { /* no resubmit if disconnecting */
		if((result = usb_submit_urb(kaweth->rx_urb))) {
			if (result == -ENOMEM)
				kaweth->suspend_lowmem = 1;
			kaweth_err("resubmitting rx_urb %d failed", result);
		} else {
			kaweth->suspend_lowmem = 0;
		}
	}
	spin_unlock_irqrestore(&kaweth->device_lock, flags);
}

static void kaweth_async_set_rx_mode(struct kaweth_device *kaweth);

/****************************************************************
 *     kaweth_usb_receive
 ****************************************************************/
static void kaweth_usb_receive(struct urb *urb)
{
	struct kaweth_device *kaweth = urb->context;
	struct net_device *net = kaweth->net;

	int count = urb->actual_length;
	int count2 = urb->transfer_buffer_length;

	__u16 pkt_len = le16_to_cpup((u16 *)kaweth->rx_buf);

	struct sk_buff *skb;

	if(urb->status == -ECONNRESET || urb->status == -ECONNABORTED)
	/* we are killed - set a flag and wake the disconnect handler */
	{
		kaweth->end = 1;
		wake_up(&kaweth->term_wait);
		return;
	}

	if (kaweth->status & KAWETH_STATUS_CLOSING)
		return;

	if(urb->status && urb->status != -EREMOTEIO && count != 1) {
		kaweth_err("%s RX status: %d count: %d packet_len: %d",
                           net->name,
			   urb->status,
			   count,
			   (int)pkt_len);
		kaweth_resubmit_rx_urb(kaweth);
                return;
	}

	if(kaweth->net && (count > 2)) {
		if(pkt_len > (count - 2)) {
			kaweth_err("Packet length too long for USB frame (pkt_len: %x, count: %x)",pkt_len, count);
			kaweth_err("Packet len & 2047: %x", pkt_len & 2047);
			kaweth_err("Count 2: %x", count2);
		        kaweth_resubmit_rx_urb(kaweth);
                        return;
                }

		if(!(skb = dev_alloc_skb(pkt_len+2))) {
		        kaweth_resubmit_rx_urb(kaweth);
                        return;
		}

		skb_reserve(skb, 2);    /* Align IP on 16 byte boundaries */

		skb->dev = net;

		eth_copy_and_sum(skb, kaweth->rx_buf + 2, pkt_len, 0);

		skb_put(skb, pkt_len);

		skb->protocol = eth_type_trans(skb, net);

		netif_rx(skb);

		kaweth->stats.rx_packets++;
		kaweth->stats.rx_bytes += pkt_len;
	}

	kaweth_resubmit_rx_urb(kaweth);
}

/****************************************************************
 *     kaweth_open
 ****************************************************************/
static int kaweth_open(struct net_device *net)
{
	struct kaweth_device *kaweth = (struct kaweth_device *)net->priv;

	kaweth_dbg("Dev usage: %d", kaweth->dev->refcnt.counter);

	kaweth_dbg("Opening network device.");

	MOD_INC_USE_COUNT;

	kaweth_resubmit_rx_urb(kaweth);

	FILL_INT_URB(
		kaweth->irq_urb,
		kaweth->dev,
		usb_rcvintpipe(kaweth->dev, 3),
		kaweth->intbuffer,
		INTBUFFERSIZE,
		int_callback,
		kaweth,
		HZ/4);

	usb_submit_urb(kaweth->irq_urb);

	netif_start_queue(net);

	kaweth_async_set_rx_mode(kaweth);
	return 0;
}

/****************************************************************
 *     kaweth_close
 ****************************************************************/
static int kaweth_close(struct net_device *net)
{
	struct kaweth_device *kaweth = net->priv;

	netif_stop_queue(net);
	
	spin_lock_irq(&kaweth->device_lock);
	kaweth->status |= KAWETH_STATUS_CLOSING;
	spin_unlock_irq(&kaweth->device_lock);

	usb_unlink_urb(kaweth->irq_urb);
	usb_unlink_urb(kaweth->rx_urb);

	kaweth->status &= ~KAWETH_STATUS_CLOSING;

	MOD_DEC_USE_COUNT;

	printk("Dev usage: %d", kaweth->dev->refcnt.counter);

	return 0;
}

static int netdev_ethtool_ioctl(struct net_device *dev, void *useraddr)
{
	u32 ethcmd;
	
	if (copy_from_user(&ethcmd, useraddr, sizeof(ethcmd)))
		return -EFAULT;
	
	switch (ethcmd) {
	case ETHTOOL_GDRVINFO: {
		struct ethtool_drvinfo info = {ETHTOOL_GDRVINFO};
		strncpy(info.driver, driver_name, sizeof(info.driver)-1);
		if (copy_to_user(useraddr, &info, sizeof(info)))
			return -EFAULT;
		return 0;
	}
	}
	
	return -EOPNOTSUPP;
}

/****************************************************************
 *     kaweth_ioctl
 ****************************************************************/
static int kaweth_ioctl(struct net_device *net, struct ifreq *rq, int cmd)
{
	switch (cmd) {
	case SIOCETHTOOL:
		return netdev_ethtool_ioctl(net, (void *) rq->ifr_data);
	}
	return -EOPNOTSUPP;
}

/****************************************************************
 *     kaweth_usb_transmit_complete
 ****************************************************************/
static void kaweth_usb_transmit_complete(struct urb *urb)
{
	struct kaweth_device *kaweth = urb->context;
	struct sk_buff *skb = kaweth->tx_skb;

	if (urb->status != 0)
		kaweth_dbg("%s: TX status %d.", kaweth->net->name, urb->status);

	netif_wake_queue(kaweth->net);
	dev_kfree_skb_irq(skb);
}

/****************************************************************
 *     kaweth_start_xmit
 ****************************************************************/
static int kaweth_start_xmit(struct sk_buff *skb, struct net_device *net)
{
	struct kaweth_device *kaweth = net->priv;
	u16 *private_header;

	int res;

	spin_lock(&kaweth->device_lock);

	if (kaweth->removed) {
	/* our device is undergoing disconnection - we bail out */
		spin_unlock(&kaweth->device_lock);
		dev_kfree_skb_irq(skb);
		return 0;
	}

	kaweth_async_set_rx_mode(kaweth);
	netif_stop_queue(net);

	/* We now decide whether we can put our special header into the sk_buff */
	if (skb_cloned(skb) || skb_headroom(skb) < 2) {
		/* no such luck - we make our own */
		struct sk_buff *copied_skb;
		copied_skb = skb_copy_expand(skb, 2, 0, GFP_ATOMIC);
		dev_kfree_skb_irq(skb);
		skb = copied_skb;
		if (!copied_skb) {
			kaweth->stats.tx_errors++;
			netif_start_queue(net);
			spin_unlock(&kaweth->device_lock);
			return 0;
		}
	}

	private_header = __skb_push(skb, 2);
	*private_header = cpu_to_le16(skb->len-2);
	kaweth->tx_skb = skb;

	FILL_BULK_URB(kaweth->tx_urb,
		      kaweth->dev,
		      usb_sndbulkpipe(kaweth->dev, 2),
		      private_header,
		      skb->len,
		      kaweth_usb_transmit_complete,
		      kaweth);
	kaweth->end = 0;
	kaweth->tx_urb->transfer_flags |= USB_ASYNC_UNLINK;

	if((res = usb_submit_urb(kaweth->tx_urb)))
	{
		kaweth_warn("kaweth failed tx_urb %d", res);
		kaweth->stats.tx_errors++;

		netif_start_queue(net);
		dev_kfree_skb_irq(skb);
	}
	else
	{
		kaweth->stats.tx_packets++;
		kaweth->stats.tx_bytes += skb->len;
		net->trans_start = jiffies;
	}

	spin_unlock(&kaweth->device_lock);

	return 0;
}

/****************************************************************
 *     kaweth_set_rx_mode
 ****************************************************************/
static void kaweth_set_rx_mode(struct net_device *net)
{
	struct kaweth_device *kaweth = net->priv;

	__u16 packet_filter_bitmap = KAWETH_PACKET_FILTER_DIRECTED |
                                     KAWETH_PACKET_FILTER_BROADCAST |
		                     KAWETH_PACKET_FILTER_MULTICAST;

	kaweth_dbg("Setting Rx mode to %d", packet_filter_bitmap);

	netif_stop_queue(net);

	if (net->flags & IFF_PROMISC) {
		packet_filter_bitmap |= KAWETH_PACKET_FILTER_PROMISCUOUS;
	}
	else if ((net->mc_count) || (net->flags & IFF_ALLMULTI)) {
		packet_filter_bitmap |= KAWETH_PACKET_FILTER_ALL_MULTICAST;
	}

	kaweth->packet_filter_bitmap = packet_filter_bitmap;
	netif_wake_queue(net);
}

/****************************************************************
 *     kaweth_async_set_rx_mode
 ****************************************************************/
static void kaweth_async_set_rx_mode(struct kaweth_device *kaweth)
{
	__u16 packet_filter_bitmap = kaweth->packet_filter_bitmap;
	kaweth->packet_filter_bitmap = 0;
	if(packet_filter_bitmap == 0) return;

	{
	int result;
	result = kaweth_control(kaweth,
				usb_sndctrlpipe(kaweth->dev, 0),
				KAWETH_COMMAND_SET_PACKET_FILTER,
				USB_TYPE_VENDOR | USB_DIR_OUT | USB_RECIP_DEVICE,
				packet_filter_bitmap,
				0,
				(void *)&kaweth->scratch,
				0,
				KAWETH_CONTROL_TIMEOUT);

	if(result < 0) {
		kaweth_err("Failed to set Rx mode: %d", result);
	}
	else {
		kaweth_dbg("Set Rx mode to %d", packet_filter_bitmap);
	}
	}
}

/****************************************************************
 *     kaweth_netdev_stats
 ****************************************************************/
static struct net_device_stats *kaweth_netdev_stats(struct net_device *dev)
{
	return &((struct kaweth_device *)dev->priv)->stats;
}

/****************************************************************
 *     kaweth_tx_timeout
 ****************************************************************/
static void kaweth_tx_timeout(struct net_device *net)
{
	struct kaweth_device *kaweth = net->priv;

	kaweth_warn("%s: Tx timed out. Resetting.", net->name);
	kaweth->stats.tx_errors++;
	net->trans_start = jiffies;

	usb_unlink_urb(kaweth->tx_urb);
}

/****************************************************************
 *     kaweth_probe
 ****************************************************************/
static void *kaweth_probe(
            struct usb_device *dev,             /* the device */
            unsigned ifnum,                      /* what interface */
            const struct usb_device_id *id      /* from id_table */
	)
{
	struct kaweth_device *kaweth;
	const eth_addr_t bcast_addr = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	int result = 0;

	kaweth_dbg("Kawasaki Device Probe (Device number:%d): 0x%4.4x:0x%4.4x:0x%4.4x",
		 dev->devnum,
		 (int)dev->descriptor.idVendor,
		 (int)dev->descriptor.idProduct,
		 (int)dev->descriptor.bcdDevice);

	kaweth_dbg("Device at %p", dev);

	kaweth_dbg("Descriptor length: %x type: %x",
		 (int)dev->descriptor.bLength,
		 (int)dev->descriptor.bDescriptorType);

	if(!(kaweth = kmalloc(sizeof(struct kaweth_device), GFP_KERNEL))) {
		kaweth_dbg("out of memory allocating device structure\n");
		return NULL;
	}

	memset(kaweth, 0, sizeof(struct kaweth_device));

	kaweth->dev = dev;
	spin_lock_init(&kaweth->device_lock);
	init_waitqueue_head(&kaweth->term_wait);

	kaweth_dbg("Resetting.");

	kaweth_reset(kaweth);

	/*
	 * If high byte of bcdDevice is nonzero, firmware is already
	 * downloaded. Don't try to do it again, or we'll hang the device.
	 */

	if (dev->descriptor.bcdDevice >> 8) {
		kaweth_info("Firmware present in device.");
	} else {
		/* Download the firmware */
		kaweth_info("Downloading firmware...");
		kaweth->firmware_buf = (__u8 *)__get_free_page(GFP_KERNEL);
		if ((result = kaweth_download_firmware(kaweth,
						      kaweth_new_code,
						      len_kaweth_new_code,
						      100,
						      2)) < 0) {
			kaweth_err("Error downloading firmware (%d)", result);
			free_page((unsigned long)kaweth->firmware_buf);
			kfree(kaweth);
			return NULL;
		}

		if ((result = kaweth_download_firmware(kaweth,
						      kaweth_new_code_fix,
						      len_kaweth_new_code_fix,
						      100,
						      3)) < 0) {
			kaweth_err("Error downloading firmware fix (%d)", result);
			free_page((unsigned long)kaweth->firmware_buf);
			kfree(kaweth);
			return NULL;
		}

		if ((result = kaweth_download_firmware(kaweth,
						      kaweth_trigger_code,
						      len_kaweth_trigger_code,
						      126,
						      2)) < 0) {
			kaweth_err("Error downloading trigger code (%d)", result);
			free_page((unsigned long)kaweth->firmware_buf);
			kfree(kaweth);
			return NULL;
		}

		if ((result = kaweth_download_firmware(kaweth,
						      kaweth_trigger_code_fix,
						      len_kaweth_trigger_code_fix,
						      126,
						      3)) < 0) {
			kaweth_err("Error downloading trigger code fix (%d)", result);
			free_page((unsigned long)kaweth->firmware_buf);
			kfree(kaweth);
			return NULL;
		}


		if ((result = kaweth_trigger_firmware(kaweth, 126)) < 0) {
			kaweth_err("Error triggering firmware (%d)", result);
			free_page((unsigned long)kaweth->firmware_buf);
			kfree(kaweth);
			return NULL;
		}

		/* Device will now disappear for a moment...  */
		kaweth_info("Firmware loaded.  I'll be back...");
		free_page((unsigned long)kaweth->firmware_buf);
		kfree(kaweth);
		return NULL;
	}

	result = kaweth_read_configuration(kaweth);

	if(result < 0) {
		kaweth_err("Error reading configuration (%d), no net device created", result);
		kfree(kaweth);
		return NULL;
	}

	kaweth_info("Statistics collection: %x", kaweth->configuration.statistics_mask);
	kaweth_info("Multicast filter limit: %x", kaweth->configuration.max_multicast_filters & ((1 << 15) - 1));
	kaweth_info("MTU: %d", le16_to_cpu(kaweth->configuration.segment_size));
	kaweth_info("Read MAC address %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x",
		 (int)kaweth->configuration.hw_addr[0],
		 (int)kaweth->configuration.hw_addr[1],
		 (int)kaweth->configuration.hw_addr[2],
		 (int)kaweth->configuration.hw_addr[3],
		 (int)kaweth->configuration.hw_addr[4],
		 (int)kaweth->configuration.hw_addr[5]);

	if(!memcmp(&kaweth->configuration.hw_addr,
                   &bcast_addr,
		   sizeof(bcast_addr))) {
		kaweth_err("Firmware not functioning properly, no net device created");
		kfree(kaweth);
		return NULL;
	}

	if(kaweth_set_urb_size(kaweth, KAWETH_BUF_SIZE) < 0) {
		kaweth_dbg("Error setting URB size");
		return kaweth;
	}

	if(kaweth_set_sofs_wait(kaweth, KAWETH_SOFS_TO_WAIT) < 0) {
		kaweth_err("Error setting SOFS wait");
		return kaweth;
	}

	result = kaweth_set_receive_filter(kaweth,
                                           KAWETH_PACKET_FILTER_DIRECTED |
                                           KAWETH_PACKET_FILTER_BROADCAST |
                                           KAWETH_PACKET_FILTER_MULTICAST);

	if(result < 0) {
		kaweth_err("Error setting receive filter");
		return kaweth;
	}

	kaweth_dbg("Initializing net device.");

	kaweth->tx_urb = usb_alloc_urb(0);
	if (!kaweth->tx_urb)
		goto err_no_urb;
	kaweth->rx_urb = usb_alloc_urb(0);
	if (!kaweth->rx_urb)
		goto err_only_tx;
	kaweth->irq_urb = usb_alloc_urb(0);
	if (!kaweth->irq_urb)
		goto err_tx_and_rx;

	kaweth->net = init_etherdev(0, 0);
	if (!kaweth->net) {
		kaweth_err("Error calling init_etherdev.");
		return kaweth;
	}

	memcpy(kaweth->net->broadcast, &bcast_addr, sizeof(bcast_addr));
	memcpy(kaweth->net->dev_addr,
               &kaweth->configuration.hw_addr,
               sizeof(kaweth->configuration.hw_addr));

	kaweth->net->priv = kaweth;
	kaweth->net->open = kaweth_open;
	kaweth->net->stop = kaweth_close;

	kaweth->net->watchdog_timeo = KAWETH_TX_TIMEOUT;
	kaweth->net->tx_timeout = kaweth_tx_timeout;

	kaweth->net->do_ioctl = kaweth_ioctl;
	kaweth->net->hard_start_xmit = kaweth_start_xmit;
	kaweth->net->set_multicast_list = kaweth_set_rx_mode;
	kaweth->net->get_stats = kaweth_netdev_stats;
	kaweth->net->mtu = le16_to_cpu(kaweth->configuration.segment_size < KAWETH_MTU ?
				       kaweth->configuration.segment_size : KAWETH_MTU);

	memset(&kaweth->stats, 0, sizeof(kaweth->stats));

	kaweth_info("kaweth interface created at %s", kaweth->net->name);

	kaweth_dbg("Kaweth probe returning.");

	return kaweth;

err_tx_and_rx:
	usb_free_urb(kaweth->rx_urb);
err_only_tx:
	usb_free_urb(kaweth->tx_urb);
err_no_urb:
	kfree(kaweth);
	return NULL;
}

/****************************************************************
 *     kaweth_disconnect
 ****************************************************************/
static void kaweth_disconnect(struct usb_device *dev, void *ptr)
{
	struct kaweth_device *kaweth = ptr;

	kaweth_info("Unregistering");

	if (!kaweth) {
		kaweth_warn("unregistering non-existant device");
		return;
	}

	kaweth->removed = 1;
	usb_unlink_urb(kaweth->irq_urb);
	usb_unlink_urb(kaweth->rx_urb);

	/* we need to wait for the urb to be cancelled, if it is active */
	spin_lock_irq(&kaweth->device_lock);
	if (usb_unlink_urb(kaweth->tx_urb) == -EINPROGRESS) {
		spin_unlock_irq(&kaweth->device_lock);
		wait_event(kaweth->term_wait, kaweth->end);
	} else {
		spin_unlock_irq(&kaweth->device_lock);
	}

	if(kaweth->net) {
		if(kaweth->net->flags & IFF_UP) {
			kaweth_dbg("Closing net device");
			dev_close(kaweth->net);
		}

		kaweth_dbg("Unregistering net device");
		unregister_netdev(kaweth->net);
	}

	usb_free_urb(kaweth->rx_urb);
	usb_free_urb(kaweth->tx_urb);

	kfree(kaweth);
}


// FIXME this completion stuff is a modified clone of
// an OLD version of some stuff in usb.c ...
struct kw_api_data {
	wait_queue_head_t wqh;
	int done;
};

/*-------------------------------------------------------------------*
 * completion handler for compatibility wrappers (sync control/bulk) *
 *-------------------------------------------------------------------*/
static void usb_api_blocking_completion(struct urb *urb)
{
        struct kw_api_data *awd = (struct kw_api_data *)urb->context;

	awd->done=1;
	wake_up(&awd->wqh);
}

/*-------------------------------------------------------------------*
 *                         COMPATIBILITY STUFF                       *
 *-------------------------------------------------------------------*/

// Starts urb and waits for completion or timeout
static int usb_start_wait_urb(struct urb *urb, int timeout, int* actual_length)
{
        DECLARE_WAITQUEUE(wait, current);
	struct kw_api_data awd;
        int status;

        init_waitqueue_head(&awd.wqh);
        awd.done = 0;

        set_current_state(TASK_INTERRUPTIBLE);
        add_wait_queue(&awd.wqh, &wait);
        urb->context = &awd;
        status = usb_submit_urb(urb);
        if (status) {
                // something went wrong
                usb_free_urb(urb);
                set_current_state(TASK_RUNNING);
                remove_wait_queue(&awd.wqh, &wait);
                return status;
        }

	while (timeout && !awd.done)
		timeout = schedule_timeout(timeout);

        set_current_state(TASK_RUNNING);
        remove_wait_queue(&awd.wqh, &wait);

        if (!timeout) {
                // timeout
                kaweth_warn("usb_control/bulk_msg: timeout");
                usb_unlink_urb(urb);  // remove urb safely
                status = -ETIMEDOUT;
        }
	else {
                status = urb->status;
	}

        if (actual_length) {
                *actual_length = urb->actual_length;
	}

        usb_free_urb(urb);
        return status;
}

/*-------------------------------------------------------------------*/
// returns status (negative) or length (positive)
int kaweth_internal_control_msg(struct usb_device *usb_dev, unsigned int pipe,
                            struct usb_ctrlrequest *cmd, void *data, int len,
			    int timeout)
{
        struct urb *urb;
        int retv;
        int length;

        urb = usb_alloc_urb(0);
        if (!urb)
                return -ENOMEM;

        FILL_CONTROL_URB(urb, usb_dev, pipe, (unsigned char*)cmd, data,
			 len, (usb_complete_t)usb_api_blocking_completion,0);

        retv = usb_start_wait_urb(urb, timeout, &length);
        if (retv < 0) {
                return retv;
	}
        else {
                return length;
	}
}


/****************************************************************
 *     kaweth_init
 ****************************************************************/
int __init kaweth_init(void)
{
	kaweth_dbg("Driver loading");
	return usb_register(&kaweth_driver);
}

/****************************************************************
 *     kaweth_exit
 ****************************************************************/
void __exit kaweth_exit(void)
{
	usb_deregister(&kaweth_driver);
}

module_init(kaweth_init);
module_exit(kaweth_exit);









