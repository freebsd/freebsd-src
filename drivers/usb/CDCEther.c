// Portions of this file taken from
// Petko Manolov - Petkan (petkan@dce.bg)
// from his driver pegasus.c

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


#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/module.h>
#include <linux/ethtool.h>
#include <asm/uaccess.h>

#define DEBUG
#include <linux/usb.h>

#include "CDCEther.h"

#define SHORT_DRIVER_DESC "CDC Ethernet Class"
#define DRIVER_VERSION "0.98.6"

static const char driver_name[] = "CDCEther";
static const char *version = __FILE__ ": " DRIVER_VERSION " 7 Jan 2002 Brad Hards and another";
// We only try to claim CDC Ethernet model devices */
static struct usb_device_id CDCEther_ids[] = {
	{ USB_INTERFACE_INFO(USB_CLASS_COMM, 6, 0) },
	{ }
};

/*
 * module parameter that provides an alternate upper limit on the
 * number of multicast filters we use, with a default to use all
 * the filters available to us. Note that the actual number used
 * is the lesser of this parameter and the number returned in the
 * descriptor for the particular device. See Table 41 of the CDC
 * spec for more info on the descriptor limit.
 */
static int multicast_filter_limit = 32767;

//////////////////////////////////////////////////////////////////////////////
// Callback routines from USB device /////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

static void read_bulk_callback( struct urb *urb )
{
	ether_dev_t *ether_dev = urb->context;
	struct net_device *net;
	int count = urb->actual_length, res;
	struct sk_buff	*skb;

	switch ( urb->status ) {
		case USB_ST_NOERROR:
			break;
		case USB_ST_URB_KILLED:
			return;
		default:
			dbg("rx status %d", urb->status);
	}

	// Sanity check
	if ( !ether_dev || !(ether_dev->flags & CDC_ETHER_RUNNING) ) {
		dbg("BULK IN callback but driver is not active!");
		return;
	}

	net = ether_dev->net;
	if ( !netif_device_present(net) ) {
		// Somebody killed our network interface...
		return;
	}

	if ( ether_dev->flags & CDC_ETHER_RX_BUSY ) {
		// Are we already trying to receive a frame???
		ether_dev->stats.rx_errors++;
		dbg("ether_dev Rx busy");
		return;
	}

	// We are busy, leave us alone!
	ether_dev->flags |= CDC_ETHER_RX_BUSY;

	switch ( urb->status ) {
		case USB_ST_NOERROR:
			break;
		case USB_ST_NORESPONSE:
			dbg( "no repsonse in BULK IN" );
			ether_dev->flags &= ~CDC_ETHER_RX_BUSY;
			break;
		default:
			dbg( "%s: RX status %d", net->name, urb->status );
			goto goon;
	}

	// Check to make sure we got some data...
	if ( !count ) {
		// We got no data!!!
		goto goon;
	}

	// Tell the kernel we want some memory
	if ( !(skb = dev_alloc_skb(count)) ) {
		// We got no receive buffer.
		goto goon;
	}

	// Here's where it came from
	skb->dev = net;
	
	// Now we copy it over
	eth_copy_and_sum(skb, ether_dev->rx_buff, count, 0);
	
	// Not sure
	skb_put(skb, count);
	// Not sure here either
	skb->protocol = eth_type_trans(skb, net);
	
	// Ship it off to the kernel
	netif_rx(skb);
	
	// update out statistics
	ether_dev->stats.rx_packets++;
	ether_dev->stats.rx_bytes += count;

goon:
	// Prep the USB to wait for another frame
	FILL_BULK_URB( &ether_dev->rx_urb, ether_dev->usb,
			usb_rcvbulkpipe(ether_dev->usb, ether_dev->data_ep_in),
			ether_dev->rx_buff, ether_dev->wMaxSegmentSize, 
			read_bulk_callback, ether_dev );
			
	// Give this to the USB subsystem so it can tell us 
	// when more data arrives.
	if ( (res = usb_submit_urb(&ether_dev->rx_urb)) ) {
		warn("%s failed submit rx_urb %d", __FUNCTION__, res);
	}
	
	// We are no longer busy, show us the frames!!!
	ether_dev->flags &= ~CDC_ETHER_RX_BUSY;
}

static void write_bulk_callback( struct urb *urb )
{
	ether_dev_t *ether_dev = urb->context;

	// Sanity check
	if ( !ether_dev || !(ether_dev->flags & CDC_ETHER_RUNNING) ) {
		// We are insane!!!
		err( "write_bulk_callback: device not running" );
		return;
	}

	// Do we still have a valid kernel network device?
	if ( !netif_device_present(ether_dev->net) ) {
		// Someone killed our network interface.
		err( "write_bulk_callback: net device not present" );
		return;
	}

	// Hmm...  What on Earth could have happened???
	if ( urb->status ) {
		dbg("%s: TX status %d", ether_dev->net->name, urb->status);
	}

	// Update the network interface and tell it we are
	// ready for another frame
	ether_dev->net->trans_start = jiffies;
	netif_wake_queue( ether_dev->net );

}

#if 0
static void setpktfilter_done( struct urb *urb )
{
	ether_dev_t *ether_dev = urb->context;
	struct net_device *net;

	if ( !ether_dev )
		return;
	dbg("got ctrl callback for setting packet filter");
	switch ( urb->status ) {
		case USB_ST_NOERROR:
			break;
		case USB_ST_URB_KILLED:
			return;
		default:
			dbg("intr status %d", urb->status);
	}
}
#endif 

static void intr_callback( struct urb *urb )
{
	ether_dev_t *ether_dev = urb->context;
	struct net_device *net;
	struct usb_ctrlrequest *event;
#define bNotification	bRequest

	if ( !ether_dev )
		return;
	net = ether_dev->net;
	switch ( urb->status ) {
		case USB_ST_NOERROR:
			break;
		case USB_ST_URB_KILLED:
		default:
			dbg("%s intr status %d", net->name, urb->status);
			return;
	}

	event = urb->transfer_buffer;
	if (event->bRequestType != 0xA1)
		dbg ("%s unknown event type %x", net->name,
			event->bRequestType);
	else switch (event->bNotification) {
	case 0x00:		// NETWORK CONNECTION
		dbg ("%s network %s", net->name,
			event->wValue ? "connect" : "disconnect");
		if (event->wValue)
			netif_carrier_on (net);
		else
			netif_carrier_off (net);
		break;
	case 0x2A:		// CONNECTION SPEED CHANGE
		dbg ("%s speed change", net->name);
		/* ignoring eight bytes of data */
		break;
	case 0x01:		// RESPONSE AVAILABLE (none requested)
	default:		// else undefined for CDC Ether
		err ("%s illegal notification %02x", net->name,
			event->bNotification);
	}
}

//////////////////////////////////////////////////////////////////////////////
// Routines for turning net traffic on and off on the USB side ///////////////
//////////////////////////////////////////////////////////////////////////////

static inline int enable_net_traffic( ether_dev_t *ether_dev )
{
	struct usb_device *usb = ether_dev->usb;

	// Here would be the time to set the data interface to the configuration where
	// it has two endpoints that use a protocol we can understand.

	if (usb_set_interface( usb,
	                        ether_dev->data_bInterfaceNumber,
	                        ether_dev->data_bAlternateSetting_with_traffic ) )  {
		err("usb_set_interface() failed" );
		err("Attempted to set interface %d", ether_dev->data_bInterfaceNumber);
		err("To alternate setting       %d", ether_dev->data_bAlternateSetting_with_traffic);
		return -1;
	}
	return 0;
}

static inline void disable_net_traffic( ether_dev_t *ether_dev )
{
	// The thing to do is to set the data interface to the alternate setting that has
	// no endpoints.  This is what the spec suggests.

	if (ether_dev->data_interface_altset_num_without_traffic >= 0 ) {
		if (usb_set_interface( ether_dev->usb,
		                        ether_dev->data_bInterfaceNumber,
		                        ether_dev->data_bAlternateSetting_without_traffic ) ) 	{
			err("usb_set_interface() failed");
		}
	} else {
		// Some devices just may not support this...
		warn("No way to disable net traffic");
	}
}

//////////////////////////////////////////////////////////////////////////////
// Callback routines for kernel Ethernet Device //////////////////////////////
//////////////////////////////////////////////////////////////////////////////

static void CDCEther_tx_timeout( struct net_device *net )
{
	ether_dev_t *ether_dev = net->priv;

	// Sanity check
	if ( !ether_dev ) {
		// Seems to be a case of insanity here
		return;
	}

	// Tell syslog we are hosed.
	warn("%s: Tx timed out.", net->name);
	
	// Tear the waiting frame off the list
	ether_dev->tx_urb.transfer_flags |= USB_ASYNC_UNLINK;
	usb_unlink_urb( &ether_dev->tx_urb );
	
	// Update statistics
	ether_dev->stats.tx_errors++;
}

static int CDCEther_start_xmit( struct sk_buff *skb, struct net_device *net )
{
	ether_dev_t	*ether_dev = net->priv;
	int 	res;

	// Tell the kernel, "No more frames 'til we are done
	// with this one.'
	netif_stop_queue( net );

	// Copy it from kernel memory to OUR memory
	memcpy(ether_dev->tx_buff, skb->data, skb->len);

	// Fill in the URB for shipping it out.
	FILL_BULK_URB( &ether_dev->tx_urb, ether_dev->usb,
			usb_sndbulkpipe(ether_dev->usb, ether_dev->data_ep_out),
			ether_dev->tx_buff, ether_dev->wMaxSegmentSize, 
			write_bulk_callback, ether_dev );

	// Tell the URB how much it will be transporting today
	ether_dev->tx_urb.transfer_buffer_length = skb->len;

	/* Deal with the Zero Length packet problem, I hope */
	ether_dev->tx_urb.transfer_flags |= USB_ZERO_PACKET;

	// Send the URB on its merry way.
	if ((res = usb_submit_urb(&ether_dev->tx_urb)))  {
		// Hmm...  It didn't go. Tell someone...
		warn("failed tx_urb %d", res);
		// update some stats...
		ether_dev->stats.tx_errors++;
		// and tell the kernel to give us another.
		// Maybe we'll get it right next time.
		netif_start_queue( net );
	} else {
		// Okay, it went out.
		// Update statistics
		ether_dev->stats.tx_packets++;
		ether_dev->stats.tx_bytes += skb->len;
		// And tell the kernel when the last transmit occurred.
		net->trans_start = jiffies;
	}

	// We are done with the kernel's memory
	dev_kfree_skb(skb);

	// We are done here.
	return 0;
}

//////////////////////////////////////////////////////////////////////////////
// Standard routines for kernel Ethernet Device //////////////////////////////
//////////////////////////////////////////////////////////////////////////////
static struct net_device_stats *CDCEther_netdev_stats( struct net_device *net )
{
	// Easy enough!
	return &((ether_dev_t *)net->priv)->stats;
}

static int CDCEther_open(struct net_device *net)
{
	ether_dev_t *ether_dev = (ether_dev_t *)net->priv;
	int	res;

	// Turn on the USB and let the packets flow!!!
	if ( (res = enable_net_traffic( ether_dev )) ) {
		err("%s can't enable_net_traffic() - %d", __FUNCTION__, res );
		return -EIO;
	}

	/* Prep a receive URB */
	FILL_BULK_URB( &ether_dev->rx_urb, ether_dev->usb,
			usb_rcvbulkpipe(ether_dev->usb, ether_dev->data_ep_in),
			ether_dev->rx_buff, ether_dev->wMaxSegmentSize,
			read_bulk_callback, ether_dev );

	/* Put it out there so the device can send us stuff */
	if ( (res = usb_submit_urb(&ether_dev->rx_urb)) ) {
		/* Hmm...  Okay... */
		warn( "%s failed rx_urb %d", __FUNCTION__, res );
	}

	if (ether_dev->properties & HAVE_NOTIFICATION_ELEMENT) {
		/* Arm and submit the interrupt URB */
		FILL_INT_URB( &ether_dev->intr_urb,
			ether_dev->usb,
			usb_rcvintpipe(ether_dev->usb, ether_dev->comm_ep_in),
			ether_dev->intr_buff,
			sizeof ether_dev->intr_buff,
			intr_callback,
			ether_dev,
			(ether_dev->usb->speed == USB_SPEED_HIGH)
				? ( 1 << ether_dev->intr_interval)
				: ether_dev->intr_interval
			);
		if ( (res = usb_submit_urb(&ether_dev->intr_urb)) ) {
			warn("%s failed intr_urb %d", __FUNCTION__, res );
		}
	}

	// Tell the kernel we are ready to start receiving from it
	netif_start_queue( net );
	
	// We are up and running.
	ether_dev->flags |= CDC_ETHER_RUNNING;

	// Let's get ready to move frames!!!
	return 0;
}

static int CDCEther_close( struct net_device *net )
{
	ether_dev_t	*ether_dev = net->priv;

	// We are no longer running.
	ether_dev->flags &= ~CDC_ETHER_RUNNING;
	
	// Tell the kernel to stop sending us stuff
	netif_stop_queue( net );
	
	// If we are not already unplugged, turn off USB
	// traffic
	if ( !(ether_dev->flags & CDC_ETHER_UNPLUG) ) {
		disable_net_traffic( ether_dev );
	}

	// We don't need the URBs anymore.
	usb_unlink_urb( &ether_dev->rx_urb );
	usb_unlink_urb( &ether_dev->tx_urb );
	usb_unlink_urb( &ether_dev->intr_urb );
	usb_unlink_urb( &ether_dev->ctrl_urb );

	// That's it.  I'm done.
	return 0;
}

static int netdev_ethtool_ioctl(struct net_device *netdev, void *useraddr)
{
	ether_dev_t *ether_dev = netdev->priv;
	u32 cmd;
	char tmp[40];

	if (get_user(cmd, (u32 *)useraddr))
		return -EFAULT;

	switch (cmd) {
	/* get driver info */
	case ETHTOOL_GDRVINFO: {
	struct ethtool_drvinfo info = {ETHTOOL_GDRVINFO};
		strncpy(info.driver, driver_name, ETHTOOL_BUSINFO_LEN);
		strncpy(info.version, DRIVER_VERSION, ETHTOOL_BUSINFO_LEN);
		sprintf(tmp, "usb%d:%d", ether_dev->usb->bus->busnum, ether_dev->usb->devnum);
		strncpy(info.bus_info, tmp, ETHTOOL_BUSINFO_LEN);
		sprintf(tmp, "CDC %x.%x", ((ether_dev->bcdCDC & 0xff00)>>8), (ether_dev->bcdCDC & 0x00ff) );
		strncpy(info.fw_version, tmp, ETHTOOL_BUSINFO_LEN);
		if (copy_to_user(useraddr, &info, sizeof(info)))
			return -EFAULT;
		return 0;
	}
	/* get link status */
	case ETHTOOL_GLINK: {
		struct ethtool_value edata = {ETHTOOL_GLINK};
		edata.data = netif_carrier_ok(netdev);
		if (copy_to_user(useraddr, &edata, sizeof(edata)))
			return -EFAULT;
		return 0;
	}
	}
	dbg("Got unsupported ioctl: %x", cmd);
	return -EOPNOTSUPP; /* the ethtool user space tool relies on this */
}

static int CDCEther_ioctl( struct net_device *net, struct ifreq *rq, int cmd )
{
	switch(cmd) {
	case SIOCETHTOOL:
		return netdev_ethtool_ioctl(net, (void *) rq->ifr_data);
	default:
		return -ENOTTY; /* per ioctl man page */
	}
}

/* Multicast routines */

static void CDC_SetEthernetPacketFilter (ether_dev_t *ether_dev)
{
#if 0
	struct usb_ctrlrequest *dr = &ether_dev->ctrl_dr;
	int res;

	dr->bRequestType = USB_TYPE_CLASS | USB_DIR_OUT | USB_RECIP_INTERFACE;
	dr->bRequest = SET_ETHERNET_PACKET_FILTER;
	dr->wValue = cpu_to_le16(ether_dev->mode_flags);
	dr->wIndex = cpu_to_le16((u16)ether_dev->comm_interface);
	dr->wLength = 0;

	FILL_CONTROL_URB(&ether_dev->ctrl_urb,
			ether_dev->usb,
			usb_sndctrlpipe(ether_dev->usb, 0),
			dr,
			NULL,
			NULL,
			setpktfilter_done,
			ether_dev);
	if ( (res = usb_submit_urb(&ether_dev->ctrl_urb)) ) {
		warn("%s failed submit ctrl_urb %d", __FUNCTION__, res);
	}
#endif

}

static void CDCEther_set_multicast( struct net_device *net )
{
	ether_dev_t *ether_dev = net->priv;
	int i;
	__u8 *buff;

	// Tell the kernel to stop sending us frames while we get this
	// all set up.
	netif_stop_queue(net);

	/* Note: do not reorder, GCC is clever about common statements. */
	if (net->flags & IFF_PROMISC) {
		/* Unconditionally log net taps. */
		dbg( "%s: Promiscuous mode enabled", net->name);
		ether_dev->mode_flags = MODE_FLAG_PROMISCUOUS |
			MODE_FLAG_ALL_MULTICAST |
			MODE_FLAG_DIRECTED |
			MODE_FLAG_BROADCAST |
			MODE_FLAG_MULTICAST;
	} else if (net->mc_count > ether_dev->wNumberMCFilters) {
		/* Too many to filter perfectly -- accept all multicasts. */
		dbg("%s: too many MC filters for hardware, using allmulti", net->name);
		ether_dev->mode_flags = MODE_FLAG_ALL_MULTICAST |
			MODE_FLAG_DIRECTED |
			MODE_FLAG_BROADCAST |
			MODE_FLAG_MULTICAST;
	} else if (net->flags & IFF_ALLMULTI) {
		/* Filter in software */
		dbg("%s: using allmulti", net->name);
		ether_dev->mode_flags = MODE_FLAG_ALL_MULTICAST |
			MODE_FLAG_DIRECTED |
			MODE_FLAG_BROADCAST |
			MODE_FLAG_MULTICAST;
	} else {
		/* do multicast filtering in hardware */
		struct dev_mc_list *mclist;
		dbg("%s: set multicast filters", net->name);
		ether_dev->mode_flags = MODE_FLAG_ALL_MULTICAST |
			MODE_FLAG_DIRECTED |
			MODE_FLAG_BROADCAST |
			MODE_FLAG_MULTICAST;
		buff = kmalloc(6 * net->mc_count, GFP_ATOMIC);
		for (i = 0, mclist = net->mc_list;
			mclist && i < net->mc_count;
			i++, mclist = mclist->next) {
				memcpy(&mclist->dmi_addr, &buff[i * 6], 6);
		}
#if 0
		usb_control_msg(ether_dev->usb,
				usb_sndctrlpipe(ether_dev->usb, 0),
				SET_ETHERNET_MULTICAST_FILTER, /* request */
				USB_TYPE_CLASS | USB_DIR_OUT | USB_RECIP_INTERFACE, /* request type */
				cpu_to_le16(net->mc_count), /* value */
				cpu_to_le16((u16)ether_dev->comm_interface), /* index */
				buff,
				(6* net->mc_count), /* size */
				HZ); /* timeout */
#endif
		kfree(buff);
	}

	CDC_SetEthernetPacketFilter(ether_dev);

	/* Tell the kernel to start giving frames to us again. */
	netif_wake_queue(net);
}

//////////////////////////////////////////////////////////////////////////////
// Routines used to parse out the Functional Descriptors /////////////////////
//////////////////////////////////////////////////////////////////////////////

/* Header Descriptor - CDC Spec 5.2.3.1, Table 26 */
static int parse_header_functional_descriptor( int *bFunctionLength,
                                               int bDescriptorType,
                                               int bDescriptorSubtype,
                                               unsigned char *data,
                                               ether_dev_t *ether_dev,
                                               int *requirements )
{
	/* Check to make sure we haven't seen one of these already. */
	if ( (~*requirements) & REQ_HDR_FUNC_DESCR ) {
		err( "Multiple Header Functional Descriptors found." );
		return -1;
	}

	/* Check for appropriate length */
	if (*bFunctionLength != HEADER_FUNC_DESC_LEN) {
		dbg( "Invalid length in Header Functional Descriptor, working around it." );
		/* This is a hack to get around a particular device (NO NAMES)
		 * It has this function length set to the length of the
		 * whole class-specific descriptor */
		*bFunctionLength = HEADER_FUNC_DESC_LEN;
	}
	
	/* Nothing extremely useful here */
	/* We'll keep it for posterity */
	ether_dev->bcdCDC = data[0] + (data[1] << 8);
	dbg( "Found Header descriptor, CDC version %x.", ether_dev->bcdCDC);

	/* We've seen one of these */
	*requirements &= ~REQ_HDR_FUNC_DESCR;

	/* Success */
	return 0;
}

/* Union Descriptor - CDC Spec 5.2.3.8, Table 33 */
static int parse_union_functional_descriptor( int *bFunctionLength, 
                                              int bDescriptorType, 
                                              int bDescriptorSubtype,
                                              unsigned char *data,
                                              ether_dev_t *ether_dev,
                                              int *requirements )
{
	/* Check to make sure we haven't seen one of these already. */
	if ( (~*requirements) & REQ_UNION_FUNC_DESCR ) {
		err( "Multiple Union Functional Descriptors found." );
		return -1;
	}

	/* Check for appropriate length */
	if (*bFunctionLength != UNION_FUNC_DESC_LEN) {
		// It is NOT the size we expected.
		err( "Invalid length in Union Functional Descriptor." );
		return -1;
	}
	
	/* Sanity check of sorts */
	if (ether_dev->comm_interface != data[0]) {
		/* This tells us that we are chasing the wrong comm
		 * interface or we are crazy or something else weird. */
		if (ether_dev->comm_interface == data[1]) {
			dbg( "Probably broken Union descriptor, fudging data interface." );
			/* We'll need this in a few microseconds,
			 * so if the comm interface was the first slave,
			 * then probably the master interface is the data one
			 * Just hope for the best */
			ether_dev->data_interface = data[0];
		} else {
			err( "Union Functional Descriptor is broken beyond repair." );
			return -1;
		}
	} else{ /* Descriptor is OK */
		ether_dev->data_interface = data[1];
	}

	/* We've seen one of these */
	*requirements &= ~REQ_UNION_FUNC_DESCR;

	/* Success */
	return 0;
}

/* Ethernet Descriptor - CDC Spec 5.2.3.16, Table 41 */
static int parse_ethernet_functional_descriptor( int *bFunctionLength,
                                                 int bDescriptorType, 
                                                 int bDescriptorSubtype,
                                                 unsigned char *data,
                                                 ether_dev_t *ether_dev,
                                                 int *requirements )
{
	//* Check to make sure we haven't seen one of these already. */
	if ( (~*requirements) & REQ_ETH_FUNC_DESCR ) {
		err( "Multiple Ethernet Functional Descriptors found." );
		return -1;
	}
	
	/* Check for appropriate length */
	if (*bFunctionLength != ETHERNET_FUNC_DESC_LEN) {
		err( "Invalid length in Ethernet Networking Functional Descriptor." );
		return -1;
	}

	/* Lots of goodies from this one.  They are all important. */
	ether_dev->iMACAddress = data[0];
	ether_dev->bmEthernetStatistics = data[1] + (data[2] << 8) + (data[3] << 16) + (data[4] << 24);
	ether_dev->wMaxSegmentSize = data[5] + (data[6] << 8);
	ether_dev->wNumberMCFilters = (data[7] + (data[8] << 8));
	if (ether_dev->wNumberMCFilters & (1 << 15)) {
		ether_dev->properties |= PERFECT_FILTERING;
		dbg("Perfect filtering support");
	} else {
		dbg("Imperfect filtering support - need sw hashing");
	}
	if (0 == (ether_dev->wNumberMCFilters & (0x7f))) {
		ether_dev->properties |= NO_SET_MULTICAST;
		dbg("Can't use SetEthernetMulticastFilters request");
	}
	if (ether_dev->wNumberMCFilters > multicast_filter_limit) {
		ether_dev->wNumberMCFilters = multicast_filter_limit;
	}
	ether_dev->bNumberPowerFilters = data[9];
	
	/* We've seen one of these */
	*requirements &= ~REQ_ETH_FUNC_DESCR;

	/* Success */
	return 0;
}

static int parse_protocol_unit_functional_descriptor( int *bFunctionLength, 
                                                      int bDescriptorType, 
                                                      int bDescriptorSubtype,
                                                      unsigned char *data,
                                                      ether_dev_t *ether_dev,
                                                      int *requirements )
{
	/* There should only be one type if we are sane */
	if (bDescriptorType != CS_INTERFACE) {
		err( "Invalid bDescriptorType found." );
		return -1;
	}

	/* The Subtype tells the tale - CDC spec Table 25 */
	switch (bDescriptorSubtype) {
		case 0x00:	/* Header Functional Descriptor */
			return parse_header_functional_descriptor( bFunctionLength,
			                                           bDescriptorType,
			                                           bDescriptorSubtype,
			                                           data,
			                                           ether_dev,
			                                           requirements );
			break;
		case 0x06:	/* Union Functional Descriptor */
			return parse_union_functional_descriptor( bFunctionLength,
			                                          bDescriptorType,
			                                          bDescriptorSubtype,
			                                          data,
			                                          ether_dev,
			                                          requirements );
			break;
		case 0x0F:	/* Ethernet Networking Functional Descriptor */
			return parse_ethernet_functional_descriptor( bFunctionLength,
			                                             bDescriptorType,
			                                             bDescriptorSubtype,
			                                             data,
			                                             ether_dev,
			                                             requirements );
			break;
		default:	/* We don't support this at this time... */
				/* However that doesn't necessarily indicate an error. */
			dbg( "Unexpected header type %x.", bDescriptorSubtype );
			return 0;
	}
	/* How did we get here? */
	return -1;
}

static int parse_ethernet_class_information( unsigned char *data, int length, ether_dev_t *ether_dev )
{
	int loc = 0;
	int rc;
	int bFunctionLength;
	int bDescriptorType;
	int bDescriptorSubtype;
	int requirements = REQUIREMENTS_TOTAL; /* We init to our needs, and then clear
						* bits as we find the descriptors */

	/* As long as there is something here, we will try to parse it */
	/* All of the functional descriptors start with the same 3 byte pattern */
	while (loc < length) {
		/* Length */
		bFunctionLength = data[loc];
		loc++;

		/* Type */
		bDescriptorType = data[loc];
		loc++;

		/* Subtype */
		bDescriptorSubtype = data[loc];
		loc++;
		
		/* ship this off to be processed */
		rc = parse_protocol_unit_functional_descriptor( &bFunctionLength, 
		                                                bDescriptorType, 
		                                                bDescriptorSubtype, 
		                                                &data[loc],
		                                                ether_dev,
		                                                &requirements );
		/* Did it process okay? */
		if (rc)	{
			/* Something was hosed somewhere. */
			/*  No need to continue */
			err("Bad descriptor parsing: %x", rc );
			return -1;
		}
		/* We move the loc pointer along, remembering
		 * that we have already taken three bytes */
		loc += (bFunctionLength - 3);
	}
	/* Check to see if we got everything we need. */
	if (requirements) {
		// We missed some of the requirements...
		err( "Not all required functional descriptors present 0x%08X.", requirements );
		return -1;
	}
	/* We got everything */
	return 0;
}

//////////////////////////////////////////////////////////////////////////////
// Routine to check for the existence of the Functional Descriptors //////////
//////////////////////////////////////////////////////////////////////////////

static int find_and_parse_ethernet_class_information( struct usb_device *device, ether_dev_t *ether_dev )
{
	struct usb_config_descriptor *conf = NULL;
	struct usb_interface *comm_intf_group = NULL;
	struct usb_interface_descriptor *comm_intf = NULL;
	int rc = -1;
	/* The assumption here is that find_ethernet_comm_interface
	 * and find_valid_configuration
	 * have already filled in the information about where to find
	 * the a valid commication interface. */

	conf = &( device->config[ether_dev->configuration_num] );
	comm_intf_group = &( conf->interface[ether_dev->comm_interface] );
	comm_intf = &( comm_intf_group->altsetting[ether_dev->comm_interface_altset_num] );

	/* Let's check and see if it has the extra information we need */
	if (comm_intf->extralen > 0) {
		/* This is where the information is SUPPOSED to be */
		rc = parse_ethernet_class_information( comm_intf->extra, comm_intf->extralen, ether_dev );
	} else if (conf->extralen > 0) {
		/* This is a hack.  The spec says it should be at the interface
		 * location checked above.  However I have seen it here also.
		 * This is the same device that requires the functional descriptor hack above */
		dbg( "Ethernet information found at device configuration.  Trying to use it anyway." );
		rc = parse_ethernet_class_information( conf->extra, conf->extralen, ether_dev );
	} else 	{
		/* I don't know where else to look */
		err( "No ethernet information found." );
		rc = -1;
	}
	return rc;
}

//////////////////////////////////////////////////////////////////////////////
// Routines to verify the data interface /////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

static int get_data_interface_endpoints( struct usb_device *device, ether_dev_t *ether_dev )
{
	struct usb_config_descriptor *conf = NULL;
	struct usb_interface *data_intf_group = NULL;
	struct usb_interface_descriptor *data_intf = NULL;

	/* Walk through and get to the data interface we are checking. */
	conf = &( device->config[ether_dev->configuration_num] );
	data_intf_group = &( conf->interface[ether_dev->data_interface] );
	data_intf = &( data_intf_group->altsetting[ether_dev->data_interface_altset_num_with_traffic] );

	/* Start out assuming we won't find anything we can use */
	ether_dev->data_ep_in = 0;
	ether_dev->data_ep_out = 0;

	/* If these are not BULK endpoints, we don't want them */
	if ( data_intf->endpoint[0].bmAttributes != USB_ENDPOINT_XFER_BULK ) {
		return -1;
	}
	if ( data_intf->endpoint[1].bmAttributes != USB_ENDPOINT_XFER_BULK ) {
		return -1;
	}

	/* Check the first endpoint to see if it is IN or OUT */
	if ( data_intf->endpoint[0].bEndpointAddress & USB_DIR_IN ) {
		ether_dev->data_ep_in = data_intf->endpoint[0].bEndpointAddress & 0x7F;
	} else {
		ether_dev->data_ep_out = data_intf->endpoint[0].bEndpointAddress;
		ether_dev->data_ep_out_size = data_intf->endpoint[0].wMaxPacketSize;
	}

	/* Check the second endpoint to see if it is IN or OUT */
	if ( data_intf->endpoint[1].bEndpointAddress & USB_DIR_IN ) {
		ether_dev->data_ep_in = data_intf->endpoint[1].bEndpointAddress & 0x7F;
	} else	{
		ether_dev->data_ep_out = data_intf->endpoint[1].bEndpointAddress;
		ether_dev->data_ep_out_size = data_intf->endpoint[1].wMaxPacketSize;
	}

	/* Now make sure we got both an IN and an OUT */
	if (ether_dev->data_ep_in && ether_dev->data_ep_out) {
		dbg( "detected BULK OUT packets of size %d", ether_dev->data_ep_out_size );
		return 0;
	}
	return -1;
}

static int verify_ethernet_data_interface( struct usb_device *device, ether_dev_t *ether_dev )
{
	struct usb_config_descriptor *conf = NULL;
	struct usb_interface *data_intf_group = NULL;
	struct usb_interface_descriptor *data_intf = NULL;
	int rc = -1;
	int status;
	int altset_num;

	// The assumption here is that parse_ethernet_class_information()
	// and find_valid_configuration() 
	// have already filled in the information about where to find
	// a data interface
	conf = &( device->config[ether_dev->configuration_num] );
	data_intf_group = &( conf->interface[ether_dev->data_interface] );

	// start out assuming we won't find what we are looking for.
	ether_dev->data_interface_altset_num_with_traffic = -1;
	ether_dev->data_bAlternateSetting_with_traffic = -1;
	ether_dev->data_interface_altset_num_without_traffic = -1;
	ether_dev->data_bAlternateSetting_without_traffic = -1;

	// Walk through every possible setting for this interface until
	// we find what makes us happy.
	for ( altset_num = 0; altset_num < data_intf_group->num_altsetting; altset_num++ ) {
		data_intf = &( data_intf_group->altsetting[altset_num] );

		// Is this a data interface we like?
		if ( ( data_intf->bInterfaceClass == 0x0A )
		   && ( data_intf->bInterfaceSubClass == 0x00 )
		   && ( data_intf->bInterfaceProtocol == 0x00 ) ) {
			if ( data_intf->bNumEndpoints == 2 ) {
				// We are required to have one of these.
				// An interface with 2 endpoints to send Ethernet traffic back and forth
				// It actually may be possible that the device might only
				// communicate in a vendor specific manner.
				// That would not be very nice.
				// We can add that one later.
				ether_dev->data_bInterfaceNumber = data_intf->bInterfaceNumber;
				ether_dev->data_interface_altset_num_with_traffic = altset_num;
				ether_dev->data_bAlternateSetting_with_traffic = data_intf->bAlternateSetting;
				status = get_data_interface_endpoints( device, ether_dev );
				if (!status) {
					rc = 0;
				}
			}
			if ( data_intf->bNumEndpoints == 0 ) {
				// According to the spec we are SUPPOSED to have one of these
				// In fact the device is supposed to come up in this state.
				// However, I have seen a device that did not have such an interface.
				// So it must be just optional for our driver...
				ether_dev->data_bInterfaceNumber = data_intf->bInterfaceNumber;
				ether_dev->data_interface_altset_num_without_traffic = altset_num;
				ether_dev->data_bAlternateSetting_without_traffic = data_intf->bAlternateSetting;
			}
		}
	}
	return rc;
}

//////////////////////////////////////////////////////////////////////////////
// Routine to find a communication interface /////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

static int find_ethernet_comm_interface( struct usb_device *device, ether_dev_t *ether_dev )
{
	struct usb_config_descriptor *conf = NULL;
	struct usb_interface *comm_intf_group = NULL;
	struct usb_interface_descriptor *comm_intf = NULL;
	int intf_num;
	int altset_num;
	int rc;

	conf = &( device->config[ether_dev->configuration_num] );

	// We need to check and see if any of these interfaces are something we want.
	// Walk through each interface one at a time
	for ( intf_num = 0; intf_num < conf->bNumInterfaces; intf_num++ ) {
		comm_intf_group = &( conf->interface[intf_num] );
		// Now for each of those interfaces, check every possible
		// alternate setting.
		for ( altset_num = 0; altset_num < comm_intf_group->num_altsetting; altset_num++ ) {
			comm_intf = &( comm_intf_group->altsetting[altset_num] );

			/* Good, we found one, we will try this one */
			/* Fill in the structure */
			ether_dev->comm_interface = intf_num;
			ether_dev->comm_bInterfaceNumber = comm_intf->bInterfaceNumber;
			ether_dev->comm_interface_altset_num = altset_num;
			ether_dev->comm_bAlternateSetting = comm_intf->bAlternateSetting;

			// Look for the Ethernet Functional Descriptors
			rc = find_and_parse_ethernet_class_information( device, ether_dev );
			if (rc) {
				// Nope this was no good after all.
				continue;
			}

			/* Check that we really can talk to the data interface
			 * This includes # of endpoints, protocols, etc. */
			rc = verify_ethernet_data_interface( device, ether_dev );
			if (rc)	{
				/* We got something we didn't like */
				continue;
			}
			/* It is a bit ambiguous whether the Ethernet model really requires
			 * the notification element (usually an interrupt endpoint) or not
			 * And some products (eg Sharp Zaurus) don't support it, so we
			 * only use the notification element if present */
			/* We check for a sane endpoint before using it */
			if ( (comm_intf->bNumEndpoints == 1) &&
				(comm_intf->endpoint[0].bEndpointAddress & USB_DIR_IN) &&
				(comm_intf->endpoint[0].bmAttributes == USB_ENDPOINT_XFER_INT)) {
					ether_dev->properties |= HAVE_NOTIFICATION_ELEMENT;
					ether_dev->comm_ep_in = (comm_intf->endpoint[0].bEndpointAddress & 0x7F);
					dbg("interrupt address: %x",ether_dev->comm_ep_in);
					ether_dev->intr_interval = (comm_intf->endpoint[0].bInterval);
					dbg("interrupt interval: %d",ether_dev->intr_interval);
			}
			// This communication interface seems to give us everything
			// we require.  We have all the ethernet info we need.

			return 0;
		} // end for altset_num
	} // end for intf_num
	return -1;
}

//////////////////////////////////////////////////////////////////////////////
// Routine to go through all configurations and find one that ////////////////
// is an Ethernet Networking Device //////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

static int find_valid_configuration( struct usb_device *device, ether_dev_t *ether_dev )
{
	struct usb_config_descriptor *conf = NULL;
	int conf_num;
	int rc;

	// We will try each and every possible configuration
	for ( conf_num = 0; conf_num < device->descriptor.bNumConfigurations; conf_num++ ) {
		conf = &( device->config[conf_num] );

		// Our first requirement : 2 interfaces
		if ( conf->bNumInterfaces != 2 ) {
			// I currently don't know how to handle devices with any number of interfaces
			// other than 2.
			continue;
		}

		// This one passed our first check, fill in some 
		// useful data
		ether_dev->configuration_num = conf_num;
		ether_dev->bConfigurationValue = conf->bConfigurationValue;

		// Now run it through the ringers and see what comes
		// out the other side.
		rc = find_ethernet_comm_interface( device, ether_dev );

		// Check if we found an ethernet Communcation Device
		if ( !rc ) {
			// We found one.
			return 0;
		}
	}
	// None of the configurations suited us.
	return -1;
}

//////////////////////////////////////////////////////////////////////////////
// Routine that checks a given configuration to see if any driver ////////////
// has claimed any of the devices interfaces /////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

static int check_for_claimed_interfaces( struct usb_config_descriptor *config )
{
	struct usb_interface *comm_intf_group;
	int intf_num;

	// Go through all the interfaces and make sure none are 
	// claimed by anybody else.
	for ( intf_num = 0; intf_num < config->bNumInterfaces; intf_num++ ) {
		comm_intf_group = &( config->interface[intf_num] );
		if ( usb_interface_claimed( comm_intf_group ) )	{
			// Somebody has beat us to this guy.
			// We can't change the configuration out from underneath of whoever
			// is using this device, so we will go ahead and give up.
			return -1;
		}
	}
	// We made it all the way through.
	// I guess no one has claimed any of these interfaces.
	return 0;
}

//////////////////////////////////////////////////////////////////////////////
// Routines to ask for and set the kernel network interface's MAC address ////
// Used by driver's probe routine ////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

static inline unsigned char hex2dec( unsigned char digit )
{
	/* Is there a standard way to do this??? */
	/* I have written this code TOO MANY times. */
	if ( (digit >= '0') && (digit <= '9') )	{
		return (digit - '0');
	}
	if ( (digit >= 'a') && (digit <= 'f') )	{
		return (digit - 'a' + 10);
	}
	if ( (digit >= 'A') && (digit <= 'F') )	{
		return (digit - 'A' + 10);
	}
	return 16;
}

/* CDC Ethernet devices provide the MAC address as a string */
/* We get an index to the string in the Ethernet functional header */
/* This routine retrieves the string, sanity checks it, and sets the */
/* MAC address in the network device */
/* The encoding is a bit wacky - see CDC Spec Table 41 for details */
static void set_ethernet_addr( ether_dev_t *ether_dev )
{
	unsigned char	mac_addr[6];
	int		i;
	int 		len;
	unsigned char	buffer[13];

	/* Let's assume we don't get anything */
	mac_addr[0] = 0x00;
	mac_addr[1] = 0x00;
	mac_addr[2] = 0x00;
	mac_addr[3] = 0x00;
	mac_addr[4] = 0x00;
	mac_addr[5] = 0x00;

	/* Let's ask the device */
	if (0 > (len = usb_string(ether_dev->usb, ether_dev->iMACAddress, buffer, 13))) {
		err("Attempting to get MAC address failed: %d", -1*len);
		return;
	}

	/* Sanity check */
	if (len != 12)	{
		/* You gotta love failing sanity checks */
		err("Attempting to get MAC address returned %d bytes", len);
		return;
	}

	/* Fill in the mac_addr */
	for (i = 0; i < 6; i++)	{
		if ((16 == buffer[2 * i]) || (16 == buffer[2 * i + 1])) {
			err("Bad value in MAC address");
		}
		else {
			mac_addr[i] = ( hex2dec( buffer[2 * i] ) << 4 ) + hex2dec( buffer[2 * i + 1] );
		}
	}

	/* Now copy it over to our network device structure */
	memcpy( ether_dev->net->dev_addr, mac_addr, sizeof(mac_addr) );
}

//////////////////////////////////////////////////////////////////////////////
// Routine to print to syslog information about the driver ///////////////////
// Used by driver's probe routine ////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

void log_device_info(ether_dev_t *ether_dev)
{
	int len;
	int string_num;
	unsigned char manu[256];
	unsigned char prod[256];
	unsigned char sern[256];
	unsigned char *mac_addr;

	/* Default empty strings in case we don't find a real one */
	manu[0] = 0x00;
	prod[0] = 0x00;
	sern[0] = 0x00;

	/*  Try to get the device Manufacturer */
	string_num = ether_dev->usb->descriptor.iManufacturer;
	if (string_num)	{
		// Put it into its buffer
		len = usb_string(ether_dev->usb, string_num, manu, 255);
		// Just to be safe
		manu[len] = 0x00;
	}

	/* Try to get the device Product Name */
	string_num = ether_dev->usb->descriptor.iProduct;
	if (string_num)	{
		// Put it into its buffer
		len = usb_string(ether_dev->usb, string_num, prod, 255);
		// Just to be safe
		prod[len] = 0x00;
	}

	/* Try to get the device Serial Number */
	string_num = ether_dev->usb->descriptor.iSerialNumber;
	if (string_num)	{
		// Put it into its buffer
		len = usb_string(ether_dev->usb, string_num, sern, 255);
		// Just to be safe
		sern[len] = 0x00;
	}

	/* This makes it easier for us to print */
	mac_addr = ether_dev->net->dev_addr;

	/* Now send everything we found to the syslog */
	info( "%s: %s %s %s", ether_dev->net->name, manu, prod, sern);
	dbg( "%s: %02X:%02X:%02X:%02X:%02X:%02X",
		ether_dev->net->name,
		mac_addr[0],
		mac_addr[1],
		mac_addr[2],
		mac_addr[3],
		mac_addr[4],
		mac_addr[5] );

}

/* Forward declaration */
static struct usb_driver CDCEther_driver ;

//////////////////////////////////////////////////////////////////////////////
// Module's probe routine ////////////////////////////////////////////////////
// claims interfaces if they are for an Ethernet CDC /////////////////////////
//////////////////////////////////////////////////////////////////////////////

static void * CDCEther_probe( struct usb_device *usb, unsigned int ifnum,
			     const struct usb_device_id *id)
{
	struct net_device	*net;
	ether_dev_t		*ether_dev;
	int 			rc;

	// First we should check the active configuration to see if 
	// any other driver has claimed any of the interfaces.
	if ( check_for_claimed_interfaces( usb->actconfig ) ) {
		// Someone has already put there grubby paws on this device.
		// We don't want it now...
		return NULL;
	}

	// We might be finding a device we can use.
	// We all go ahead and allocate our storage space.
	// We need to because we have to start filling in the data that
	// we are going to need later.
	if(!(ether_dev = kmalloc(sizeof(ether_dev_t), GFP_KERNEL))) {
		err("out of memory allocating device structure");
		return NULL;
	}

	// Zero everything out.
	memset(ether_dev, 0, sizeof(ether_dev_t));

	// Let's see if we can find a configuration we can use.
	rc = find_valid_configuration( usb, ether_dev );
	if (rc)	{
		// Nope we couldn't find one we liked.
		// This device was not meant for us to control.
		kfree( ether_dev );
		return	NULL;
	}

	// Now that we FOUND a configuration. let's try to make the 
	// device go into it.
	if ( usb_set_configuration( usb, ether_dev->bConfigurationValue ) ) {
		err("usb_set_configuration() failed");
		kfree( ether_dev );
		return NULL;
	}

	// Now set the communication interface up as required.
	if (usb_set_interface(usb, ether_dev->comm_bInterfaceNumber, ether_dev->comm_bAlternateSetting)) {
		err("usb_set_interface() failed");
		kfree( ether_dev );
		return NULL;
	}

	// Only turn traffic on right now if we must...
	if (ether_dev->data_interface_altset_num_without_traffic >= 0)	{
		// We found an alternate setting for the data
		// interface that allows us to turn off traffic.
		// We should use it.
		if (usb_set_interface( usb, 
		                       ether_dev->data_bInterfaceNumber, 
		                       ether_dev->data_bAlternateSetting_without_traffic)) {
			err("usb_set_interface() failed");
			kfree( ether_dev );
			return NULL;
		}
	} else	{
		// We didn't find an alternate setting for the data
		// interface that would let us turn off traffic.
		// Oh well, let's go ahead and do what we must...
		if (usb_set_interface( usb, 
		                       ether_dev->data_bInterfaceNumber, 
		                       ether_dev->data_bAlternateSetting_with_traffic)) {
			err("usb_set_interface() failed");
			kfree( ether_dev );
			return NULL;
		}
	}

	// Now we need to get a kernel Ethernet interface.
	net = init_etherdev( NULL, 0 );
	if ( !net ) {
		// Hmm...  The kernel is not sharing today...
		// Fine, we didn't want it anyway...
		err( "Unable to initialize ethernet device" );
		kfree( ether_dev );
		return	NULL;
	}

	// Now that we have an ethernet device, let's set it up
	// (And I don't mean "set [it] up the bomb".)
	net->priv = ether_dev;
	SET_MODULE_OWNER(net);
	net->open = CDCEther_open;
	net->stop = CDCEther_close;
	net->watchdog_timeo = CDC_ETHER_TX_TIMEOUT;
	net->tx_timeout = CDCEther_tx_timeout;   // TX timeout function
	net->do_ioctl = CDCEther_ioctl;
	net->hard_start_xmit = CDCEther_start_xmit;
	net->set_multicast_list = CDCEther_set_multicast;
	net->get_stats = CDCEther_netdev_stats;
	net->mtu = ether_dev->wMaxSegmentSize - 14;

	// We'll keep track of this information for later...
	ether_dev->usb = usb;
	ether_dev->net = net;
	
	// and don't forget the MAC address.
	set_ethernet_addr( ether_dev );

	// Send a message to syslog about what we are handling
	log_device_info( ether_dev );

	/* We need to manually claim the data interface, while the comm interface gets claimed in the return */
	usb_driver_claim_interface( &CDCEther_driver, 
	                            &(usb->config[ether_dev->configuration_num].interface[ether_dev->data_interface]), 
	                            ether_dev );

	// Does this REALLY do anything???
	usb_inc_dev_use( usb );

	// Okay, we are finally done...
	return ether_dev;
}


//////////////////////////////////////////////////////////////////////////////
// Module's disconnect routine ///////////////////////////////////////////////
// Called when the driver is unloaded or the device is unplugged /////////////
// (Whichever happens first assuming the driver suceeded at its probe) ///////
//////////////////////////////////////////////////////////////////////////////

static void CDCEther_disconnect( struct usb_device *usb, void *ptr )
{
	ether_dev_t *ether_dev = ptr;

	// Sanity check!!!
	if ( !ether_dev || !ether_dev->usb ) {
		// We failed.  We are insane!!!
		warn("unregistering non-existant device");
		return;
	}

	// Make sure we fail the sanity check if we try this again.
	ether_dev->usb = NULL;
	
	// It is possible that this function is called before
	// the "close" function.
	// This tells the close function we are already disconnected
	ether_dev->flags |= CDC_ETHER_UNPLUG;
	
	// We don't need the network device any more
	unregister_netdev( ether_dev->net );
	
	// For sanity checks
	ether_dev->net = NULL;

	// I ask again, does this do anything???
	usb_dec_dev_use( usb );

	// We are done with this interface
	usb_driver_release_interface( &CDCEther_driver, 
	                              &(usb->config[ether_dev->configuration_num].interface[ether_dev->comm_interface]) );

	// We are done with this interface too
	usb_driver_release_interface( &CDCEther_driver, 
	                              &(usb->config[ether_dev->configuration_num].interface[ether_dev->data_interface]) );

	// No more tied up kernel memory
	kfree( ether_dev );
	
	// This does no good, but it looks nice!
	ether_dev = NULL;
}

//////////////////////////////////////////////////////////////////////////////
// Driver info ///////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

static struct usb_driver CDCEther_driver = {
	name:		driver_name,
	probe:		CDCEther_probe,
	disconnect:	CDCEther_disconnect,
	id_table:	CDCEther_ids,
};

//////////////////////////////////////////////////////////////////////////////
// init and exit routines called when driver is installed and uninstalled ////
//////////////////////////////////////////////////////////////////////////////

int __init CDCEther_init(void)
{
	dbg( "%s", version );
	return usb_register( &CDCEther_driver );
}

void __exit CDCEther_exit(void)
{
	usb_deregister( &CDCEther_driver );
}

//////////////////////////////////////////////////////////////////////////////
// Module info ///////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

module_init( CDCEther_init );
module_exit( CDCEther_exit );

MODULE_AUTHOR("Brad Hards and another");
MODULE_DESCRIPTION("USB CDC Ethernet driver");
MODULE_LICENSE("GPL");

MODULE_DEVICE_TABLE (usb, CDCEther_ids);
MODULE_PARM (multicast_filter_limit, "i");
MODULE_PARM_DESC (multicast_filter_limit, "CDCEther maximum number of filtered multicast addresses");

//////////////////////////////////////////////////////////////////////////////
// End of file ///////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
