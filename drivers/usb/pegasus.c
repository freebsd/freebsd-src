/*
 *  Copyright (c) 1999-2003 Petko Manolov (petkan@users.sourceforge.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *	
 *
 *	ChangeLog:
 *		....	Most of the time spend reading sources & docs.
 *		v0.2.x	First official release for the Linux kernel.
 *		v0.3.0	Beutified and structured, some bugs fixed.
 *		v0.3.x	URBifying bulk requests and bugfixing. First relatively
 *			stable release. Still can touch device's registers only
 *			from top-halves.
 *		v0.4.0	Control messages remained unurbified are now URBs.
 *			Now we can touch the HW at any time.
 *		v0.4.9	Control urbs again use process context to wait. Argh...
 *			Some long standing bugs (enable_net_traffic) fixed.
 *			Also nasty trick about resubmiting control urb from
 *			interrupt context used. Please let me know how it
 *			behaves. Pegasus II support added since this version.
 *			TODO: suppressing HCD warnings spewage on disconnect.
 *		v0.4.13	Ethernet address is now set at probe(), not at open()
 *			time as this seems to break dhcpd. 
 *		v0.4.25	ethtool support added.
 */

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/usb.h>
#include <linux/module.h>
#include <asm/uaccess.h>
#include "pegasus.h"

/*
 * Version Information
 */
#define DRIVER_VERSION "v0.4.32 (2003/06/06)"
#define DRIVER_AUTHOR "Petko Manolov <petkan@users.sourceforge.net>"
#define DRIVER_DESC "Pegasus/Pegasus II USB Ethernet driver"

static const char driver_name[] = "pegasus";

#define	PEGASUS_USE_INTR
#define	PEGASUS_WRITE_EEPROM
#define	BMSR_MEDIA	(BMSR_10HALF | BMSR_10FULL | BMSR_100HALF | \
			BMSR_100FULL | BMSR_ANEGCAPABLE)

static int loopback = 0;
static int mii_mode = 0;
static int multicast_filter_limit = 32;

static struct usb_eth_dev usb_dev_id[] = {
#define	PEGASUS_DEV(pn, vid, pid, flags)	\
	{name:pn, vendor:vid, device:pid, private:flags},
#include "pegasus.h"
#undef	PEGASUS_DEV
	{NULL, 0, 0, 0}
};

static struct usb_device_id pegasus_ids[] = {
#define	PEGASUS_DEV(pn, vid, pid, flags) \
	{match_flags: USB_DEVICE_ID_MATCH_DEVICE, idVendor:vid, idProduct:pid},
#include "pegasus.h"
#undef	PEGASUS_DEV
	{}
};

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
MODULE_PARM(loopback, "i");
MODULE_PARM(mii_mode, "i");
MODULE_PARM_DESC(loopback, "Enable MAC loopback mode (bit 0)");
MODULE_PARM_DESC(mii_mode, "Enable HomePNA mode (bit 0),default=MII mode = 0");

MODULE_DEVICE_TABLE(usb, pegasus_ids);

static int update_eth_regs_async(pegasus_t *);
/* Aargh!!! I _really_ hate such tweaks */
static void ctrl_callback(struct urb *urb)
{
	pegasus_t *pegasus = urb->context;

	if (!pegasus)
		return;

	switch (urb->status) {
	case 0:
		if (pegasus->flags & ETH_REGS_CHANGE) {
			pegasus->flags &= ~ETH_REGS_CHANGE;
			pegasus->flags |= ETH_REGS_CHANGED;
			update_eth_regs_async(pegasus);
			return;
		}
		break;
	case -EINPROGRESS:
		return;
	case -ENOENT:
		break;
	default:
		warn("%s: status %d", __FUNCTION__, urb->status);
	}
	pegasus->flags &= ~ETH_REGS_CHANGED;
	wake_up(&pegasus->ctrl_wait);
}

static int get_registers(pegasus_t * pegasus, u16 indx, u16 size,
			 void *data)
{
	int ret;
	char *buffer;
	DECLARE_WAITQUEUE(wait, current);

	buffer = kmalloc(size, GFP_DMA);
	if (!buffer) {
		warn("%s: looks like we're out of memory", __FUNCTION__);
		return -ENOMEM;
	}
	add_wait_queue(&pegasus->ctrl_wait, &wait);
	set_current_state(TASK_UNINTERRUPTIBLE);
	while (pegasus->flags & ETH_REGS_CHANGED)
		schedule();
	remove_wait_queue(&pegasus->ctrl_wait, &wait);
	set_current_state(TASK_RUNNING);

	pegasus->dr.bRequestType = PEGASUS_REQT_READ;
	pegasus->dr.bRequest = PEGASUS_REQ_GET_REGS;
	pegasus->dr.wValue = cpu_to_le16(0);
	pegasus->dr.wIndex = cpu_to_le16p(&indx);
	pegasus->dr.wLength = cpu_to_le16p(&size);
	pegasus->ctrl_urb->transfer_buffer_length = size;

	FILL_CONTROL_URB(pegasus->ctrl_urb, pegasus->usb,
			 usb_rcvctrlpipe(pegasus->usb, 0),
			 (char *) &pegasus->dr,
			 buffer, size, ctrl_callback, pegasus);

	add_wait_queue(&pegasus->ctrl_wait, &wait);
	set_current_state(TASK_UNINTERRUPTIBLE);

	if ((ret = usb_submit_urb(pegasus->ctrl_urb))) {
		err("%s: BAD CTRLs %d", __FUNCTION__, ret);
		goto out;
	}

	schedule();
out:
	remove_wait_queue(&pegasus->ctrl_wait, &wait);
	memcpy(data, buffer, size);
	kfree(buffer);

	return ret;
}

static int set_registers(pegasus_t * pegasus, u16 indx, u16 size,
			 void *data)
{
	int ret;
	char *buffer;
	DECLARE_WAITQUEUE(wait, current);

	buffer = kmalloc(size, GFP_DMA);
	if (!buffer) {
		warn("%s: looks like we're out of memory", __FUNCTION__);
		return -ENOMEM;
	}
	memcpy(buffer, data, size);

	add_wait_queue(&pegasus->ctrl_wait, &wait);
	set_current_state(TASK_UNINTERRUPTIBLE);
	while (pegasus->flags & ETH_REGS_CHANGED)
		schedule();
	remove_wait_queue(&pegasus->ctrl_wait, &wait);
	set_current_state(TASK_RUNNING);

	pegasus->dr.bRequestType = PEGASUS_REQT_WRITE;
	pegasus->dr.bRequest = PEGASUS_REQ_SET_REGS;
	pegasus->dr.wValue = cpu_to_le16(0);
	pegasus->dr.wIndex = cpu_to_le16p(&indx);
	pegasus->dr.wLength = cpu_to_le16p(&size);
	pegasus->ctrl_urb->transfer_buffer_length = size;

	FILL_CONTROL_URB(pegasus->ctrl_urb, pegasus->usb,
			 usb_sndctrlpipe(pegasus->usb, 0),
			 (char *) &pegasus->dr,
			 buffer, size, ctrl_callback, pegasus);

	add_wait_queue(&pegasus->ctrl_wait, &wait);
	set_current_state(TASK_UNINTERRUPTIBLE);

	if ((ret = usb_submit_urb(pegasus->ctrl_urb))) {
		err("%s: BAD CTRL %d", __FUNCTION__, ret);
		goto out;
	}

	schedule();
out:
	remove_wait_queue(&pegasus->ctrl_wait, &wait);
	kfree(buffer);

	return ret;
}

static int set_register(pegasus_t * pegasus, u16 indx, u8 data)
{
	int ret;
	char *tmp;
	DECLARE_WAITQUEUE(wait, current);

	tmp = kmalloc(1, GFP_DMA);
	if (!tmp) {
		warn("%s: looks like we're out of memory", __FUNCTION__);
		return -ENOMEM;
	}
	memcpy(tmp, &data, 1);
	add_wait_queue(&pegasus->ctrl_wait, &wait);
	set_current_state(TASK_UNINTERRUPTIBLE);
	while (pegasus->flags & ETH_REGS_CHANGED)
		schedule();
	remove_wait_queue(&pegasus->ctrl_wait, &wait);
	set_current_state(TASK_RUNNING);

	pegasus->dr.bRequestType = PEGASUS_REQT_WRITE;
	pegasus->dr.bRequest = PEGASUS_REQ_SET_REG;
	pegasus->dr.wValue = cpu_to_le16(data);
	pegasus->dr.wIndex = cpu_to_le16p(&indx);
	pegasus->dr.wLength = cpu_to_le16(1);
	pegasus->ctrl_urb->transfer_buffer_length = 1;

	FILL_CONTROL_URB(pegasus->ctrl_urb, pegasus->usb,
			 usb_sndctrlpipe(pegasus->usb, 0),
			 (char *) &pegasus->dr,
			 tmp, 1, ctrl_callback, pegasus);

	add_wait_queue(&pegasus->ctrl_wait, &wait);
	set_current_state(TASK_UNINTERRUPTIBLE);

	if ((ret = usb_submit_urb(pegasus->ctrl_urb))) {
		err("%s: BAD CTRL %d", __FUNCTION__, ret);
		goto out;
	}

	schedule();
out:
	remove_wait_queue(&pegasus->ctrl_wait, &wait);
	kfree(tmp);

	return ret;
}

static int update_eth_regs_async(pegasus_t * pegasus)
{
	int ret;

	pegasus->dr.bRequestType = PEGASUS_REQT_WRITE;
	pegasus->dr.bRequest = PEGASUS_REQ_SET_REGS;
	pegasus->dr.wValue = 0;
	pegasus->dr.wIndex = cpu_to_le16(EthCtrl0);
	pegasus->dr.wLength = cpu_to_le16(3);
	pegasus->ctrl_urb->transfer_buffer_length = 3;

	FILL_CONTROL_URB(pegasus->ctrl_urb, pegasus->usb,
			 usb_sndctrlpipe(pegasus->usb, 0),
			 (char *) &pegasus->dr,
			 pegasus->eth_regs, 3, ctrl_callback, pegasus);

	if ((ret = usb_submit_urb(pegasus->ctrl_urb)))
		err("%s: BAD CTRL %d, flgs %x", __FUNCTION__, ret,
		    pegasus->flags);

	return ret;
}

static int read_mii_word(pegasus_t * pegasus, u8 phy, u8 indx, u16 * regd)
{
	int i;
	u8 data[4] = { phy, 0, 0, indx };
	u16 regdi;

	set_register(pegasus, PhyCtrl, 0);
	set_registers(pegasus, PhyAddr, sizeof(data), data);
	set_register(pegasus, PhyCtrl, (indx | PHY_READ));
	for (i = 0; i < REG_TIMEOUT; i++) {
		get_registers(pegasus, PhyCtrl, 1, data);
		if (data[0] & PHY_DONE)
			break;
	}
	if (i < REG_TIMEOUT) {
		get_registers(pegasus, PhyData, 2, &regdi);
		*regd = le16_to_cpu(regdi);
		return 0;
	}
	warn("%s: failed", __FUNCTION__);

	return 1;
}

static int mdio_read(struct net_device *dev, int phy_id, int loc)
{
	pegasus_t *pegasus = (pegasus_t *) dev->priv;
	int res;

	read_mii_word(pegasus, phy_id, loc, (u16 *) & res);
	return res & 0xffff;
}

static int write_mii_word(pegasus_t * pegasus, u8 phy, u8 indx, u16 regd)
{
	int i;
	u8 data[4] = { phy, 0, 0, indx };

	*(data + 1) = cpu_to_le16p(&regd);
	set_register(pegasus, PhyCtrl, 0);
	set_registers(pegasus, PhyAddr, 4, data);
	set_register(pegasus, PhyCtrl, (indx | PHY_WRITE));
	for (i = 0; i < REG_TIMEOUT; i++) {
		get_registers(pegasus, PhyCtrl, 1, data);
		if (data[0] & PHY_DONE)
			break;
	}
	if (i < REG_TIMEOUT)
		return 0;
	warn("%s: failed", __FUNCTION__);

	return 1;
}

static void mdio_write(struct net_device *dev, int phy_id, int loc, int val)
{
	pegasus_t *pegasus = (pegasus_t *) dev->priv;

	write_mii_word(pegasus, phy_id, loc, val);
}

static int read_eprom_word(pegasus_t * pegasus, u8 index, u16 * retdata)
{
	int i;
	u8 tmp;
	u16 retdatai;

	set_register(pegasus, EpromCtrl, 0);
	set_register(pegasus, EpromOffset, index);
	set_register(pegasus, EpromCtrl, EPROM_READ);

	for (i = 0; i < REG_TIMEOUT; i++) {
		get_registers(pegasus, EpromCtrl, 1, &tmp);
		if (tmp & EPROM_DONE)
			break;
	}
	if (i < REG_TIMEOUT) {
		get_registers(pegasus, EpromData, 2, &retdatai);
		*retdata = le16_to_cpu(retdatai);
		return 0;
	}
	warn("%s: failed", __FUNCTION__);

	return -1;
}

#ifdef	PEGASUS_WRITE_EEPROM
static inline void enable_eprom_write(pegasus_t * pegasus)
{
	u8 tmp;

	get_registers(pegasus, EthCtrl2, 1, &tmp);
	set_register(pegasus, EthCtrl2, tmp | EPROM_WR_ENABLE);
}

static inline void disable_eprom_write(pegasus_t * pegasus)
{
	u8 tmp;

	get_registers(pegasus, EthCtrl2, 1, &tmp);
	set_register(pegasus, EpromCtrl, 0);
	set_register(pegasus, EthCtrl2, tmp & ~EPROM_WR_ENABLE);
}

static int write_eprom_word(pegasus_t * pegasus, u8 index, u16 data)
{
	int i, tmp;
	u8 d[4] = { 0x3f, 0, 0, EPROM_WRITE };

	set_registers(pegasus, EpromOffset, 4, d);
	enable_eprom_write(pegasus);
	set_register(pegasus, EpromOffset, index);
	set_registers(pegasus, EpromData, 2, &data);
	set_register(pegasus, EpromCtrl, EPROM_WRITE);

	for (i = 0; i < REG_TIMEOUT; i++) {
		get_registers(pegasus, EpromCtrl, 1, &tmp);
		if (tmp & EPROM_DONE)
			break;
	}
	disable_eprom_write(pegasus);
	if (i < REG_TIMEOUT)
		return 0;
	warn("%s: failed", __FUNCTION__);
	return -1;
}
#endif				/* PEGASUS_WRITE_EEPROM */

static inline void get_node_id(pegasus_t * pegasus, u8 * id)
{
	int i;
	u16 w16;

	for (i = 0; i < 3; i++) {
		read_eprom_word(pegasus, i, &w16);
		((u16 *) id)[i] = cpu_to_le16p(&w16);
	}
}

static void set_ethernet_addr(pegasus_t * pegasus)
{
	u8 node_id[6];

	get_node_id(pegasus, node_id);
	set_registers(pegasus, EthID, sizeof(node_id), node_id);
	memcpy(pegasus->net->dev_addr, node_id, sizeof(node_id));
}

static inline int reset_mac(pegasus_t * pegasus)
{
	u8 data = 0x8;
	int i;

	set_register(pegasus, EthCtrl1, data);
	for (i = 0; i < REG_TIMEOUT; i++) {
		get_registers(pegasus, EthCtrl1, 1, &data);
		if (~data & 0x08) {
			if (loopback & 1)
				break;
			if (mii_mode && (pegasus->features & HAS_HOME_PNA))
				set_register(pegasus, Gpio1, 0x34);
			else
				set_register(pegasus, Gpio1, 0x26);
			set_register(pegasus, Gpio0, pegasus->features);
			set_register(pegasus, Gpio0, DEFAULT_GPIO_SET);
			break;
		}
	}
	if (i == REG_TIMEOUT)
		return 1;

	if (usb_dev_id[pegasus->dev_index].vendor == VENDOR_LINKSYS ||
	    usb_dev_id[pegasus->dev_index].vendor == VENDOR_DLINK) {
		u16 auxmode;

		read_mii_word(pegasus, 1, 0x1b, &auxmode);
		write_mii_word(pegasus, 1, 0x1b, auxmode | 4);
	}
	if (usb_dev_id[pegasus->dev_index].vendor == VENDOR_ELCON) {
		u16 auxmode;
		read_mii_word(pegasus, 3, 0x1b, &auxmode);
		write_mii_word(pegasus, 3, 0x1b, auxmode | 4);
	}
	return 0;
}

static int enable_net_traffic(struct net_device *dev, struct usb_device *usb)
{
	u16 linkpart, bmsr;
	u8 data[4];
	pegasus_t *pegasus = dev->priv;

	/* read twice 'cos this is a latch bit */
	read_mii_word(pegasus, pegasus->phy, MII_BMSR, &bmsr);
	read_mii_word(pegasus, pegasus->phy, MII_BMSR, &bmsr);
	if (read_mii_word(pegasus, pegasus->phy, MII_LPA, &linkpart))
		return 2;
	if (!(linkpart & 1))
		warn("link partner stat %x", linkpart);

	data[0] = 0xc9;
	data[1] = 0;
	if (linkpart & (ADVERTISE_100FULL | ADVERTISE_10FULL))
		data[1] |= 0x20;	/* set full duplex */
	if (linkpart & (ADVERTISE_100FULL | ADVERTISE_100HALF))
		data[1] |= 0x10;	/* set 100 Mbps */
	if (mii_mode)
		data[1] |= 1;
	data[2] = (loopback & 1) ? 0x09 : 0x01;
	memcpy(pegasus->eth_regs, data, sizeof(data));
	set_registers(pegasus, EthCtrl0, 3, data);

	return 0;
}

static void read_bulk_callback(struct urb *urb)
{
	pegasus_t *pegasus = urb->context;
	struct net_device *net;
	int count = urb->actual_length, res;
	int rx_status;
	struct sk_buff *skb;
	u16 pkt_len;

	if (!pegasus || !(pegasus->flags & PEGASUS_RUNNING))
		return;

	net = pegasus->net;
	if (!netif_device_present(net))
		return;

	if (pegasus->flags & PEGASUS_RX_BUSY) {
		pegasus->stats.rx_errors++;
		dbg("pegasus Rx busy");
		return;
	}
	pegasus->flags |= PEGASUS_RX_BUSY;

	switch (urb->status) {
	case 0:
		break;
	case -ETIMEDOUT:
		dbg("reset MAC");
		pegasus->flags &= ~PEGASUS_RX_BUSY;
		break;
	default:
		dbg("%s: RX status %d", net->name, urb->status);
		goto goon;
	}

	if (!count)
		goto goon;

	rx_status = le32_to_cpu(*(int *) (pegasus->rx_buff + count - 4));
	if (rx_status & 0x000e0000) {
		dbg("%s: RX packet error %x", net->name, rx_status & 0xe0000);
		pegasus->stats.rx_errors++;
		if (rx_status & 0x060000)
			pegasus->stats.rx_length_errors++;
		if (rx_status & 0x080000)
			pegasus->stats.rx_crc_errors++;
		if (rx_status & 0x100000)
			pegasus->stats.rx_frame_errors++;
		goto goon;
	}

	if (pegasus->chip == 0x8513) {
		pkt_len = le32_to_cpu(*(int *)pegasus->rx_buff);
		pkt_len &= 0x0fff;
	} else {
		pkt_len = (rx_status & 0x0fff) - 8;
	}

	if (!(skb = dev_alloc_skb(pkt_len + 2)))
		goto goon;

	skb->dev = net;
	skb_reserve(skb, 2);
	eth_copy_and_sum(skb, pegasus->rx_buff, pkt_len, 0);
	skb_put(skb, pkt_len);

	if (pegasus->chip == 0x8513) {
		skb->data += 2;
	}

	skb->protocol = eth_type_trans(skb, net);
	netif_rx(skb);
	pegasus->stats.rx_packets++;
	pegasus->stats.rx_bytes += pkt_len;

goon:
	FILL_BULK_URB(pegasus->rx_urb, pegasus->usb,
		      usb_rcvbulkpipe(pegasus->usb, 1),
		      pegasus->rx_buff, PEGASUS_MAX_MTU,
		      read_bulk_callback, pegasus);
	if ((res = usb_submit_urb(pegasus->rx_urb)))
		warn("%s: failed submint rx_urb %d", __FUNCTION__, res);
	pegasus->flags &= ~PEGASUS_RX_BUSY;
}

static void write_bulk_callback(struct urb *urb)
{
	pegasus_t *pegasus = urb->context;
	struct net_device *net = pegasus->net;

	if (!pegasus || !(pegasus->flags & PEGASUS_RUNNING))
		return;

	if (!netif_device_present(net))
		return;

	switch (urb->status) {
	case -EPIPE:
		/* FIXME schedule_work() to clear the tx halt */
		netif_stop_queue(net);
		warn("%s: no tx stall recovery", net->name);
		return;
	case -ENOENT:
	case -ECONNRESET:
	case -ESHUTDOWN:
		dbg("%s: tx unlink, %d", net->name, urb->status);
		return;
	default:
		info("%s: TX status %d", net->name, urb->status);
		/* FALL THROUGH */
	case 0:
		break;
	}

	net->trans_start = jiffies;
	netif_wake_queue(net);
}

#ifdef	PEGASUS_USE_INTR
static void intr_callback(struct urb *urb)
{
	pegasus_t *pegasus = urb->context;
	struct net_device *net;
	u8 *d;

	if (!pegasus)
		return;

	switch (urb->status) {
	case 0:
		break;
	case -ENOENT:
		return;
	default:
		info("intr status %d", urb->status);
	}

	d = urb->transfer_buffer;
	net = pegasus->net;
	if (d[0] & 0xfc) {
		pegasus->stats.tx_errors++;
		if (d[0] & TX_UNDERRUN)
			pegasus->stats.tx_fifo_errors++;
		if (d[0] & (EXCESSIVE_COL | JABBER_TIMEOUT))
			pegasus->stats.tx_aborted_errors++;
		if (d[0] & LATE_COL)
			pegasus->stats.tx_window_errors++;
		if (d[5] & LINK_STATUS) {
			netif_carrier_on(net);
		} else {
			netif_carrier_off(net);
			pegasus->stats.tx_carrier_errors++;
		}
	}
}
#endif

static void pegasus_tx_timeout(struct net_device *net)
{
	pegasus_t *pegasus = net->priv;

	if (!pegasus)
		return;

	warn("%s: Tx timed out.", net->name);
	pegasus->tx_urb->transfer_flags |= USB_ASYNC_UNLINK;
	usb_unlink_urb(pegasus->tx_urb);
	pegasus->stats.tx_errors++;
}

static int pegasus_start_xmit(struct sk_buff *skb, struct net_device *net)
{
	pegasus_t *pegasus = net->priv;
	int count = ((skb->len + 2) & 0x3f) ? skb->len + 2 : skb->len + 3;
	int res;
	u16 l16 = skb->len;

	netif_stop_queue(net);

	((u16 *) pegasus->tx_buff)[0] = cpu_to_le16(l16);
	memcpy(pegasus->tx_buff + 2, skb->data, skb->len);
	FILL_BULK_URB(pegasus->tx_urb, pegasus->usb,
		      usb_sndbulkpipe(pegasus->usb, 2),
		      pegasus->tx_buff, PEGASUS_MAX_MTU,
		      write_bulk_callback, pegasus);
	pegasus->tx_urb->transfer_buffer_length = count;
	if ((res = usb_submit_urb(pegasus->tx_urb))) {
		switch (res) {
		case -EPIPE:	/* stall, or disconnect from TT */
			/* cleanup should already have been scheduled */
			break;
		case -ENODEV:	/* disconnect() upcoming */
			break;
		default:
			pegasus->stats.tx_errors++;
			netif_start_queue(net);
		}
	} else {
		pegasus->stats.tx_packets++;
		pegasus->stats.tx_bytes += skb->len;
		net->trans_start = jiffies;
	}

	dev_kfree_skb(skb);

	return 0;
}

static struct net_device_stats *pegasus_netdev_stats(struct net_device *dev)
{
	return &((pegasus_t *) dev->priv)->stats;
}

static inline void disable_net_traffic(pegasus_t * pegasus)
{
	int tmp = 0;

	set_registers(pegasus, EthCtrl0, 2, &tmp);
}

static inline void get_interrupt_interval(pegasus_t * pegasus)
{
	u8 data[2];

	read_eprom_word(pegasus, 4, (u16 *) data);
	if (data[1] < 0x80) {
		info("intr interval will be changed from %ums to %ums",
		     data[1], 0x80);
		data[1] = 0x80;
#ifdef	PEGASUS_WRITE_EEPROM
		write_eprom_word(pegasus, 4, *(u16 *) data);
#endif
	}
	pegasus->intr_interval = data[1];
}

static void set_carrier(struct net_device *net)
{
	pegasus_t *pegasus;
	short tmp;

	pegasus = net->priv;
	read_mii_word(pegasus, pegasus->phy, MII_BMSR, &tmp);
	if (tmp & BMSR_LSTATUS)
		netif_carrier_on(net);
	else
		netif_carrier_off(net);

}

static int pegasus_open(struct net_device *net)
{
	pegasus_t *pegasus = (pegasus_t *) net->priv;
	int res;

	down(&pegasus->sem);

	set_registers(pegasus, EthID, 6, net->dev_addr);
	
	FILL_BULK_URB(pegasus->rx_urb, pegasus->usb,
		      usb_rcvbulkpipe(pegasus->usb, 1),
		      pegasus->rx_buff, PEGASUS_MAX_MTU,
		      read_bulk_callback, pegasus);
	if ((res = usb_submit_urb(pegasus->rx_urb)))
		warn("%s: failed rx_urb %d", __FUNCTION__, res);
#ifdef	PEGASUS_USE_INTR
	FILL_INT_URB(pegasus->intr_urb, pegasus->usb,
		     usb_rcvintpipe(pegasus->usb, 3),
		     pegasus->intr_buff, sizeof(pegasus->intr_buff),
		     intr_callback, pegasus, pegasus->intr_interval);
	if ((res = usb_submit_urb(pegasus->intr_urb)))
		warn("%s: failed intr_urb %d", __FUNCTION__, res);
#endif
	netif_start_queue(net);
	pegasus->flags |= PEGASUS_RUNNING;
	if ((res = enable_net_traffic(net, pegasus->usb))) {
		err("can't enable_net_traffic() - %d", res);
		res = -EIO;
		goto exit;
	}

	set_carrier(net);
	res = 0;
exit:
	up(&pegasus->sem);

	return res;
}

static int pegasus_close(struct net_device *net)
{
	pegasus_t *pegasus = net->priv;

	down(&pegasus->sem);
	pegasus->flags &= ~PEGASUS_RUNNING;
	netif_stop_queue(net);
	if (!(pegasus->flags & PEGASUS_UNPLUG))
		disable_net_traffic(pegasus);

	usb_unlink_urb(pegasus->rx_urb);
	usb_unlink_urb(pegasus->tx_urb);
	usb_unlink_urb(pegasus->ctrl_urb);
#ifdef	PEGASUS_USE_INTR
	usb_unlink_urb(pegasus->intr_urb);
#endif
	up(&pegasus->sem);

	return 0;
}
#if CONFIG_MII
static int pegasus_ethtool_ioctl(struct net_device *dev, void *useraddr)
{

	u32 ethcmd;
	pegasus_t *pegasus = dev->priv;

	if (copy_from_user(&ethcmd, useraddr, sizeof (ethcmd)))
		return -EFAULT;

	switch (ethcmd) {
	/* get driver-specific version/etc. info */
	case ETHTOOL_GDRVINFO:{
			struct ethtool_drvinfo info;

			memset (&info, 0, sizeof info);
			info.cmd = ETHTOOL_GDRVINFO;
			strncpy(info.driver, driver_name,
				sizeof (info.driver) - 1);
			strncpy(info.version, DRIVER_VERSION,
				sizeof (info.version) - 1);
			usb_make_path(pegasus->usb, info.bus_info,
			              sizeof info.bus_info);
			if (copy_to_user(useraddr, &info, sizeof (info)))
				return -EFAULT;
			return 0;
		}

	/* get settings */
	case ETHTOOL_GSET:{
			struct ethtool_cmd ecmd = { ETHTOOL_GSET };
			mii_ethtool_gset(&pegasus->mii, &ecmd);
			if (copy_to_user(useraddr, &ecmd, sizeof (ecmd)))
				return -EFAULT;
			return 0;
		}
	/* set settings */
	case ETHTOOL_SSET:{
			int r;
			struct ethtool_cmd ecmd;
			if (copy_from_user(&ecmd, useraddr, sizeof (ecmd)))
				return -EFAULT;
			r = mii_ethtool_sset(&pegasus->mii, &ecmd);
			return r;
		}
	/* restart autonegotiation */
	case ETHTOOL_NWAY_RST:{
			return mii_nway_restart(&pegasus->mii);
		}

	/* get link status */
	case ETHTOOL_GLINK:{
			struct ethtool_value edata = { ETHTOOL_GLINK };
			edata.data = mii_link_ok(&pegasus->mii);
			if (copy_to_user(useraddr, &edata, sizeof (edata)))
				return -EFAULT;
			return 0;
		}
	/* get message-level */
	case ETHTOOL_GMSGLVL:{
			struct ethtool_value edata = { ETHTOOL_GMSGLVL };
			/* edata.data = pegasus->msg_enable; FIXME */
			if (copy_to_user(useraddr, &edata, sizeof (edata)))
				return -EFAULT;
			return 0;
		}
	/* set message-level */
	case ETHTOOL_SMSGLVL:{
			struct ethtool_value edata;
			if (copy_from_user(&edata, useraddr, sizeof (edata)))
				return -EFAULT;
			/* sp->msg_enable = edata.data; FIXME */
			return 0;
		}

	}

	return -EOPNOTSUPP;

}
#else
static int pegasus_ethtool_ioctl(struct net_device *net, void *uaddr)
{
	pegasus_t *pegasus;
	int cmd;

	pegasus = net->priv;
	if (get_user(cmd, (int *) uaddr))
		return -EFAULT;
	switch (cmd) {
	case ETHTOOL_GDRVINFO:{
			struct ethtool_drvinfo info;

			memset (&info, 0, sizeof info);
			info.cmd = ETHTOOL_GDRVINFO;
			strncpy(info.driver, driver_name,
			        sizeof (info.driver) - 1);
			strncpy(info.driver, DRIVER_DESC, ETHTOOL_BUSINFO_LEN);
			strncpy(info.version, DRIVER_VERSION,
				sizeof (info.version) - 1);
			usb_make_path(pegasus->usb, info.bus_info,
			              sizeof (info.bus_info) -1);
			if (copy_to_user(uaddr, &info, sizeof(info)))
				return -EFAULT;
			return 0;
		}
	case ETHTOOL_GSET:{
			struct ethtool_cmd ecmd;
			short lpa, bmcr;
			u8 port;

			if (copy_from_user(&ecmd, uaddr, sizeof(ecmd)))
				return -EFAULT;
			ecmd.supported = (SUPPORTED_10baseT_Half |
					  SUPPORTED_10baseT_Full |
					  SUPPORTED_100baseT_Half |
					  SUPPORTED_100baseT_Full |
					  SUPPORTED_Autoneg |
					  SUPPORTED_TP | SUPPORTED_MII);
			get_registers(pegasus, Reg7b, 1, &port);
			if (port == 0)
				ecmd.port = PORT_MII;
			else
				ecmd.port = PORT_TP;
			ecmd.transceiver = XCVR_INTERNAL;
			ecmd.phy_address = pegasus->phy;
			read_mii_word(pegasus, pegasus->phy, MII_BMCR, &bmcr);
			read_mii_word(pegasus, pegasus->phy, MII_LPA, &lpa);
			if (bmcr & BMCR_ANENABLE) {
				ecmd.autoneg = AUTONEG_ENABLE;
				ecmd.speed = lpa & (LPA_100HALF | LPA_100FULL) ?
				    SPEED_100 : SPEED_10;
				if (ecmd.speed == SPEED_100)
					ecmd.duplex = lpa & LPA_100FULL ?
					    DUPLEX_FULL : DUPLEX_HALF;
				else
					ecmd.duplex = lpa & LPA_10FULL ?
					    DUPLEX_FULL : DUPLEX_HALF;
			} else {
				ecmd.autoneg = AUTONEG_DISABLE;
				ecmd.speed = bmcr & BMCR_SPEED100 ?
				    SPEED_100 : SPEED_10;
				ecmd.duplex = bmcr & BMCR_FULLDPLX ?
				    DUPLEX_FULL : DUPLEX_HALF;
			}
			if (copy_to_user(uaddr, &ecmd, sizeof(ecmd)))
				return -EFAULT;

			return 0;
		}
	case ETHTOOL_SSET:{
			return -EOPNOTSUPP;
		}
	case ETHTOOL_GLINK:{
			struct ethtool_value edata = { ETHTOOL_GLINK };
			edata.data = netif_carrier_ok(net);
			if (copy_to_user(uaddr, &edata, sizeof(edata)))
				return -EFAULT;
			return 0;
		}
	default:
		return -EOPNOTSUPP;
	}
}
#endif
static int pegasus_ioctl(struct net_device *net, struct ifreq *rq, int cmd)
{
	u16 *data = (u16 *) & rq->ifr_data;
	pegasus_t *pegasus = net->priv;
	int res;

	down(&pegasus->sem);
	switch (cmd) {
	case SIOCETHTOOL:
		res = pegasus_ethtool_ioctl(net, rq->ifr_data);
		break;
	case SIOCDEVPRIVATE:
		data[0] = pegasus->phy;
	case SIOCDEVPRIVATE + 1:
		read_mii_word(pegasus, data[0], data[1] & 0x1f, &data[3]);
		res = 0;
		break;
	case SIOCDEVPRIVATE + 2:
		if (!capable(CAP_NET_ADMIN)) {
			up(&pegasus->sem);
			return -EPERM;
		}
		write_mii_word(pegasus, pegasus->phy, data[1] & 0x1f, data[2]);
		res = 0;
		break;
	default:
		res = -EOPNOTSUPP;
	}
	up(&pegasus->sem);

	return res;
}

static void pegasus_set_multicast(struct net_device *net)
{
	pegasus_t *pegasus = net->priv;

	netif_stop_queue(net);

	if (net->flags & IFF_PROMISC) {
		pegasus->eth_regs[EthCtrl2] |= RX_PROMISCUOUS;
		info("%s: Promiscuous mode enabled", net->name);
	} else if ((net->mc_count > multicast_filter_limit) ||
		   (net->flags & IFF_ALLMULTI)) {
		pegasus->eth_regs[EthCtrl0] |= RX_MULTICAST;
		pegasus->eth_regs[EthCtrl2] &= ~RX_PROMISCUOUS;
		info("%s set allmulti", net->name);
	} else {
		pegasus->eth_regs[EthCtrl0] &= ~RX_MULTICAST;
		pegasus->eth_regs[EthCtrl2] &= ~RX_PROMISCUOUS;
	}

	pegasus->flags |= ETH_REGS_CHANGE;
	ctrl_callback(pegasus->ctrl_urb);

	netif_wake_queue(net);
}

static u8 mii_phy_probe(pegasus_t * pegasus)
{
	int i;
	u16 tmp;

	for (i = 0; i < 32; i++) {
		read_mii_word(pegasus, i, MII_BMSR, &tmp);
		if (tmp == 0 || tmp == 0xffff || (tmp & BMSR_MEDIA) == 0)
			continue;
		else
			return i;
	}

	return 0xff;
}

static inline void setup_pegasus_II(pegasus_t * pegasus)
{
	u16 data = 0xa5;
	
	set_register(pegasus, Reg1d, 0);
	set_register(pegasus, Reg7b, 1);
	mdelay(100);
	if ((pegasus->features & HAS_HOME_PNA) && mii_mode)
		set_register(pegasus, Reg7b, 0);
	else
		set_register(pegasus, Reg7b, 2);
	
	set_register(pegasus, 0x83, data);
	get_registers(pegasus, 0x83, 1, &data);
	
	if (data & 0xa5) {
		pegasus->chip = 0x8513;
	} else {
		pegasus->chip = 0;
	}
	
	set_register( pegasus, 0x80, 0xc0 );
	set_register( pegasus, 0x83, 0xff );
	set_register( pegasus, 0x84, 0x01 );
	
	if ((pegasus->features & HAS_HOME_PNA) && mii_mode)
		set_register(pegasus, Reg81, 6);
	else
		set_register(pegasus, Reg81, 2);
}

static void *pegasus_probe(struct usb_device *dev, unsigned int ifnum,
			   const struct usb_device_id *id)
{
	struct net_device *net;
	pegasus_t *pegasus;
	int dev_index = id - pegasus_ids;

	if (usb_set_configuration(dev, dev->config[0].bConfigurationValue)) {
		err("usb_set_configuration() failed");
		return NULL;
	}

	if (!(pegasus = kmalloc(sizeof(struct pegasus), GFP_KERNEL))) {
		err("out of memory allocating device structure");
		return NULL;
	}

	usb_inc_dev_use(dev);
	memset(pegasus, 0, sizeof(struct pegasus));
	pegasus->dev_index = dev_index;
	init_waitqueue_head(&pegasus->ctrl_wait);

	pegasus->ctrl_urb = usb_alloc_urb(0);
	if (!pegasus->ctrl_urb) {
		kfree(pegasus);
		return NULL;
	}
	pegasus->rx_urb = usb_alloc_urb(0);
	if (!pegasus->rx_urb) {
		usb_free_urb(pegasus->ctrl_urb);
		kfree(pegasus);
		return NULL;
	}
	pegasus->tx_urb = usb_alloc_urb(0);
	if (!pegasus->tx_urb) {
		usb_free_urb(pegasus->rx_urb);
		usb_free_urb(pegasus->ctrl_urb);
		kfree(pegasus);
		return NULL;
	}
	pegasus->intr_urb = usb_alloc_urb(0);
	if (!pegasus->intr_urb) {
		usb_free_urb(pegasus->tx_urb);
		usb_free_urb(pegasus->rx_urb);
		usb_free_urb(pegasus->ctrl_urb);
		kfree(pegasus);
		return NULL;
	}

	net = init_etherdev(NULL, 0);
	if (!net) {
		usb_free_urb(pegasus->tx_urb);
		usb_free_urb(pegasus->rx_urb);
		usb_free_urb(pegasus->ctrl_urb);
		kfree(pegasus);
		return NULL;
	}

	init_MUTEX(&pegasus->sem);
	down(&pegasus->sem);
	pegasus->usb = dev;
	pegasus->net = net;
	SET_MODULE_OWNER(net);
	net->priv = pegasus;
	net->open = pegasus_open;
	net->stop = pegasus_close;
	net->watchdog_timeo = PEGASUS_TX_TIMEOUT;
	net->tx_timeout = pegasus_tx_timeout;
	net->do_ioctl = pegasus_ioctl;
	net->hard_start_xmit = pegasus_start_xmit;
	net->set_multicast_list = pegasus_set_multicast;
	net->get_stats = pegasus_netdev_stats;
	net->mtu = PEGASUS_MTU;
	pegasus->mii.dev = net;
	pegasus->mii.mdio_read = mdio_read;
	pegasus->mii.mdio_write = mdio_write;
	pegasus->mii.phy_id_mask = 0x1f;
	pegasus->mii.reg_num_mask = 0x1f;

	pegasus->features = usb_dev_id[dev_index].private;
#ifdef	PEGASUS_USE_INTR
	get_interrupt_interval(pegasus);
#endif
	if (reset_mac(pegasus)) {
		err("can't reset MAC");
		unregister_netdev(pegasus->net);
		usb_free_urb(pegasus->tx_urb);
		usb_free_urb(pegasus->rx_urb);
		usb_free_urb(pegasus->ctrl_urb);
		kfree(pegasus->net);
		kfree(pegasus);
		pegasus = NULL;
		goto exit;
	}

	info("%s: %s", net->name, usb_dev_id[dev_index].name);

	set_ethernet_addr(pegasus);

	if (pegasus->features & PEGASUS_II) {
		info("setup Pegasus II specific registers");
		setup_pegasus_II(pegasus);
	}

	pegasus->phy = mii_phy_probe(pegasus);
	if (pegasus->phy == 0xff) {
		warn("can't locate MII phy, using default");
		pegasus->phy = 1;
	}
exit:
	up(&pegasus->sem);
	
	return pegasus;
}

static void pegasus_disconnect(struct usb_device *dev, void *ptr)
{
	struct pegasus *pegasus = ptr;

	if (!pegasus) {
		warn("unregistering non-existant device");
		return;
	}

	pegasus->flags |= PEGASUS_UNPLUG;
	unregister_netdev(pegasus->net);
	usb_dec_dev_use(dev);
	usb_unlink_urb(pegasus->intr_urb);
	usb_unlink_urb(pegasus->tx_urb);
	usb_unlink_urb(pegasus->rx_urb);
	usb_unlink_urb(pegasus->ctrl_urb);
	usb_free_urb(pegasus->intr_urb);
	usb_free_urb(pegasus->tx_urb);
	usb_free_urb(pegasus->rx_urb);
	usb_free_urb(pegasus->ctrl_urb);
	kfree(pegasus->net);
	kfree(pegasus);
	pegasus = NULL;
}

static struct usb_driver pegasus_driver = {
	name:		driver_name,
	probe:		pegasus_probe,
	disconnect:	pegasus_disconnect,
	id_table:	pegasus_ids,
};

int __init pegasus_init(void)
{
	info(DRIVER_VERSION ":" DRIVER_DESC);
	return usb_register(&pegasus_driver);
}

void __exit pegasus_exit(void)
{
	usb_deregister(&pegasus_driver);
}

module_init(pegasus_init);
module_exit(pegasus_exit);
