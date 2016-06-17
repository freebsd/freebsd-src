/*
 *  Copyright (c) 2002 Petko Manolov (petkan@users.sourceforge.net)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License 
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/usb.h>
#include <linux/init.h>
#include <asm/uaccess.h>

/* Version Information */
#define DRIVER_VERSION "v0.4.3 (2002/12/31)"
#define DRIVER_AUTHOR "Petko Manolov <petkan@users.sourceforge.net>"
#define DRIVER_DESC "rtl8150 based usb-ethernet driver"

#define	IDR			0x0120
#define	MAR			0x0126
#define	CR			0x012e
#define	TCR			0x012f
#define	RCR			0x0130
#define	TSR			0x0132
#define	RSR			0x0133
#define	CON0			0x0135
#define	CON1			0x0136
#define	MSR			0x0137
#define	PHYADD			0x0138
#define	PHYDAT			0x0139
#define	PHYCNT			0x013b
#define	GPPC			0x013d
#define	BMCR			0x0140
#define	BMSR			0x0142
#define	ANAR			0x0144
#define	ANLP			0x0146
#define	AER			0x0148

#define	IDR_EEPROM		0x1202

#define	PHY_READ		0
#define	PHY_WRITE		0x20
#define	PHY_GO			0x40

#define	RTL8150_REQT_READ	0xc0
#define	RTL8150_REQT_WRITE	0x40
#define	RTL8150_REQ_GET_REGS	0x05
#define	RTL8150_REQ_SET_REGS	0x05

#define	RTL8150_MTU		1500
#define	RTL8150_MAX_MTU		1536

#define	RTL8150_TX_TIMEOUT	(HZ)

/* rtl8150 flags */
#define	RTL8150_FLAG_HWCRC	0
#define	RX_REG_SET		1
#define	RTL8150_UNPLUG		2

/* Define these values to match your device */
#define VENDOR_ID_REALTEK		0x0bda
#define	VENDOR_ID_MELCO			0x0411

#define PRODUCT_ID_RTL8150		0x8150
#define	PRODUCT_ID_LUAKTX		0x0012

#undef EEPROM_WRITE

/* table of devices that work with this driver */
static struct usb_device_id rtl8150_table[] = {
	{USB_DEVICE(VENDOR_ID_REALTEK, PRODUCT_ID_RTL8150)},
	{USB_DEVICE(VENDOR_ID_MELCO, PRODUCT_ID_LUAKTX)},
	{}
};

MODULE_DEVICE_TABLE(usb, rtl8150_table);

struct rtl8150 {
	unsigned int flags;
	struct usb_device *udev;
	struct usb_interface *interface;
	struct semaphore sem;
	struct net_device_stats stats;
	struct net_device *netdev;
	struct urb *rx_urb, *tx_urb, *intr_urb, *ctrl_urb;
	struct usb_ctrlrequest dr;
	int intr_interval;
	u16 rx_creg;
	u8 rx_buff[RTL8150_MAX_MTU];
	u8 tx_buff[RTL8150_MAX_MTU];
	u8 intr_buff[8];
	u8 phy;
};

typedef struct rtl8150 rtl8150_t;

/* the global usb devfs handle */
extern devfs_handle_t usb_devfs_handle;
unsigned long multicast_filter_limit = 32;

static void rtl8150_disconnect(struct usb_device *dev, void *ptr);
static void *rtl8150_probe(struct usb_device *dev, unsigned int ifnum,
			   const struct usb_device_id *id);

static const char driver_name[] = "rtl8150";

static struct usb_driver rtl8150_driver = {
	name:		driver_name,
	probe:		rtl8150_probe,
	disconnect:	rtl8150_disconnect,
	id_table:	rtl8150_table,
};

/*
**
**	device related part of the code
**
*/
static int get_registers(rtl8150_t * dev, u16 indx, u16 size, void *data)
{
	return usb_control_msg(dev->udev, usb_rcvctrlpipe(dev->udev, 0),
			       RTL8150_REQ_GET_REGS, RTL8150_REQT_READ,
			       indx, 0, data, size, HZ / 2);
}

static int set_registers(rtl8150_t * dev, u16 indx, u16 size, void *data)
{
	return usb_control_msg(dev->udev, usb_sndctrlpipe(dev->udev, 0),
			       RTL8150_REQ_SET_REGS, RTL8150_REQT_WRITE,
			       indx, 0, data, size, HZ / 2);
}

static void ctrl_callback(struct urb *urb)
{
	rtl8150_t *dev;

	switch (urb->status) {
	case 0:
		break;
	case -EINPROGRESS:
		break;
	case -ENOENT:
		break;
	default:
		warn("ctrl urb status %d", urb->status);
	}
	dev = urb->context;
	clear_bit(RX_REG_SET, &dev->flags);
}

static int async_set_registers(rtl8150_t * dev, u16 indx, u16 size)
{
	int ret;

	if (test_bit(RX_REG_SET, &dev->flags))
		return -EAGAIN;

	dev->dr.bRequestType = RTL8150_REQT_WRITE;
	dev->dr.bRequest = RTL8150_REQ_SET_REGS;
	dev->dr.wValue = cpu_to_le16(indx);
	dev->dr.wIndex = 0;
	dev->dr.wLength = cpu_to_le16(2);
	dev->ctrl_urb->transfer_buffer_length = 2;
	FILL_CONTROL_URB(dev->ctrl_urb, dev->udev,
			 usb_sndctrlpipe(dev->udev, 0), (char *) &dev->dr,
			 &dev->rx_creg, 2, ctrl_callback, dev);
	if ((ret = usb_submit_urb(dev->ctrl_urb)))
		err("control request submission failed: %d", ret);
	else
		set_bit(RX_REG_SET, &dev->flags);

	return ret;
}

static int read_mii_word(rtl8150_t * dev, u8 phy, __u8 indx, u16 * reg)
{
	int i;
	u8 data[3], tmp;

	data[0] = phy;
	data[1] = data[2] = 0;
	tmp = indx | PHY_READ | PHY_GO;
	i = 0;

	set_registers(dev, PHYADD, sizeof(data), data);
	set_registers(dev, PHYCNT, 1, &tmp);
	do {
		get_registers(dev, PHYCNT, 1, data);
	} while ((data[0] & PHY_GO) && (i++ < HZ));

	if (i < HZ) {
		get_registers(dev, PHYDAT, 2, data);
		*reg = le16_to_cpup((u16 *)data);
		return 0;
	} else
		return 1;
}

static int write_mii_word(rtl8150_t * dev, u8 phy, __u8 indx, u16 reg)
{
	int i;
	u8 data[3], tmp;

	data[0] = phy;
	*(data + 1) = cpu_to_le16p(&reg);
	tmp = indx | PHY_WRITE | PHY_GO;
	i = 0;

	set_registers(dev, PHYADD, sizeof(data), data);
	set_registers(dev, PHYCNT, 1, &tmp);
	do {
		get_registers(dev, PHYCNT, 1, data);
	} while ((data[0] & PHY_GO) && (i++ < HZ));

	if (i < HZ)
		return 0;
	else
		return 1;
}

static inline void set_ethernet_addr(rtl8150_t * dev)
{
	u8 node_id[6];

	get_registers(dev, IDR, sizeof(node_id), node_id);
	memcpy(dev->netdev->dev_addr, node_id, sizeof(node_id));
}

static int rtl8150_set_mac_address(struct net_device *netdev, void *p)
{
	struct sockaddr *addr = p;
	rtl8150_t *dev;
	int i;

	if (netif_running(netdev))
		return -EBUSY;
	dev = netdev->priv;
	if (dev == NULL) {
		return -ENODEV;
	}
	memcpy(netdev->dev_addr, addr->sa_data, netdev->addr_len);
	dbg("%s: Setting MAC address to ", netdev->name);
	for (i = 0; i < 5; i++)
		printk("%02X:", netdev->dev_addr[i]);
	dbg("%02X\n", netdev->dev_addr[i]);
	/* Set the IDR registers. */
	set_registers(dev, IDR, sizeof(netdev->dev_addr), netdev->dev_addr);
#ifdef EEPROM_WRITE
	{
	u8 cr;
	/* Get the CR contents. */
	get_registers(dev, CR, 1, &cr);
	/* Set the WEPROM bit (eeprom write enable). */
	cr |= 0x20;
	set_registers(dev, CR, 1, &cr);
	/* Write the MAC address into eeprom. Eeprom writes must be word-sized,
	   so we need to split them up. */
	for (i = 0; i * 2 < netdev->addr_len; i++) {
		set_registers(dev, IDR_EEPROM + (i * 2), 2, 
		              netdev->dev_addr + (i * 2));
	}
	/* Clear the WEPROM bit (preventing accidental eeprom writes). */
	cr &= 0xdf;
	set_registers(dev, CR, 1, &cr);
	}
#endif	
	return 0;
}

static int rtl8150_reset(rtl8150_t * dev)
{
	u8 data = 0x10;
	int i = HZ;

	set_registers(dev, CR, 1, &data);
	do {
		get_registers(dev, CR, 1, &data);
	} while ((data & 0x10) && --i);

	return (i > 0) ? 0 : -1;
}

static int alloc_all_urbs(rtl8150_t * dev)
{
	dev->rx_urb = usb_alloc_urb(0);
	if (!dev->rx_urb)
		return 0;
	dev->tx_urb = usb_alloc_urb(0);
	if (!dev->tx_urb) {
		usb_free_urb(dev->rx_urb);
		return 0;
	}
	dev->intr_urb = usb_alloc_urb(0);
	if (!dev->intr_urb) {
		usb_free_urb(dev->rx_urb);
		usb_free_urb(dev->tx_urb);
		return 0;
	}
	dev->ctrl_urb = usb_alloc_urb(0);
	if (!dev->ctrl_urb) {
		usb_free_urb(dev->rx_urb);
		usb_free_urb(dev->tx_urb);
		usb_free_urb(dev->intr_urb);
		return 0;
	}

	return 1;
}

static void free_all_urbs(rtl8150_t * dev)
{
	usb_free_urb(dev->rx_urb);
	usb_free_urb(dev->tx_urb);
	usb_free_urb(dev->intr_urb);
	usb_free_urb(dev->ctrl_urb);
}

static void unlink_all_urbs(rtl8150_t * dev)
{
	usb_unlink_urb(dev->rx_urb);
	usb_unlink_urb(dev->tx_urb);
	usb_unlink_urb(dev->intr_urb);
	usb_unlink_urb(dev->ctrl_urb);
}

static void read_bulk_callback(struct urb *urb)
{
	rtl8150_t *dev;
	int pkt_len, res;
	struct sk_buff *skb;
	struct net_device *netdev;
	u16 rx_stat;

	dev = urb->context;
	if (!dev) {
		warn("!dev");
		return;
	}
	netdev = dev->netdev;
	if (!netif_device_present(netdev)) {
		warn("netdev is not present");
		return;
	}
	switch (urb->status) {
	case 0:
		break;
	case -ENOENT:
		return;
	case -ETIMEDOUT:
		warn("need a device reset?..");
		goto goon;
	default:
		warn("Rx status %d", urb->status);
		goto goon;
	}

	pkt_len = urb->actual_length - 4;
	rx_stat = le16_to_cpu(*(u16 *) (dev->rx_buff + pkt_len));

	if (!(skb = dev_alloc_skb(pkt_len + 2)))
		goto goon;
	skb->dev = netdev;
	skb_reserve(skb, 2);
	eth_copy_and_sum(skb, dev->rx_buff, pkt_len, 0);
	skb_put(skb, pkt_len);
	skb->protocol = eth_type_trans(skb, netdev);
	netif_rx(skb);
	dev->stats.rx_packets++;
	dev->stats.rx_bytes += pkt_len;
goon:
	FILL_BULK_URB(dev->rx_urb, dev->udev, usb_rcvbulkpipe(dev->udev, 1),
		      dev->rx_buff, RTL8150_MAX_MTU, read_bulk_callback, dev);
	if ((res = usb_submit_urb(dev->rx_urb)))
		warn("%s: Rx urb submission failed %d", netdev->name, res);
}

static void write_bulk_callback(struct urb *urb)
{
	rtl8150_t *dev;

	dev = urb->context;
	if (!dev)
		return;
	if (!netif_device_present(dev->netdev))
		return;
	if (urb->status)
		info("%s: Tx status %d", dev->netdev->name, urb->status);
	dev->netdev->trans_start = jiffies;
	netif_wake_queue(dev->netdev);
}

void intr_callback(struct urb *urb)
{
	rtl8150_t *dev;

	dev = urb->context;
	if (!dev)
		return;
	switch (urb->status) {
	case 0:
		break;
	case -ENOENT:
		return;
	default:
		info("%s: intr status %d", dev->netdev->name, urb->status);
	}
}

/*
**
**	network related part of the code
**
*/

static int enable_net_traffic(rtl8150_t * dev)
{
	u8 cr, tcr, rcr, msr;

	if (rtl8150_reset(dev)) {
		warn("%s - device reset failed", __FUNCTION__);
	}
	rcr = 0x9e;	/* bit7=1 attach Rx info at the end */
	dev->rx_creg = cpu_to_le16(rcr);
	tcr = 0xd8;
	cr = 0x0c;
	set_registers(dev, RCR, 1, &rcr);
	set_registers(dev, TCR, 1, &tcr);
	set_registers(dev, CR, 1, &cr);
	get_registers(dev, MSR, 1, &msr);

	return 0;
}

static void disable_net_traffic(rtl8150_t * dev)
{
	u8 cr;

	get_registers(dev, CR, 1, &cr);
	cr &= 0xf3;
	set_registers(dev, CR, 1, &cr);
}

static struct net_device_stats *rtl8150_netdev_stats(struct net_device *dev)
{
	return &((rtl8150_t *) dev->priv)->stats;
}

static void rtl8150_tx_timeout(struct net_device *netdev)
{
	rtl8150_t *dev;

	dev = netdev->priv;
	if (!dev)
		return;
	warn("%s: Tx timeout.", netdev->name);
	dev->tx_urb->transfer_flags |= USB_ASYNC_UNLINK;
	usb_unlink_urb(dev->tx_urb);
	dev->stats.tx_errors++;
}

static void rtl8150_set_multicast(struct net_device *netdev)
{
	rtl8150_t *dev;

	dev = netdev->priv;
	netif_stop_queue(netdev);
	if (netdev->flags & IFF_PROMISC) {
		dev->rx_creg |= cpu_to_le16(0x0001);
		info("%s: promiscuous mode", netdev->name);
	} else if ((netdev->mc_count > multicast_filter_limit) ||
		   (netdev->flags & IFF_ALLMULTI)) {
		dev->rx_creg &= cpu_to_le16(0xfffe);
		dev->rx_creg |= cpu_to_le16(0x0002);
		info("%s: allmulti set", netdev->name);
	} else {
		/* ~RX_MULTICAST, ~RX_PROMISCUOUS */
		dev->rx_creg &= cpu_to_le16(0x00fc);
	}
	async_set_registers(dev, RCR, 2);
	netif_wake_queue(netdev);
}

static int rtl8150_start_xmit(struct sk_buff *skb, struct net_device *netdev)
{
	rtl8150_t *dev;
	int count, res;

	netif_stop_queue(netdev);
	dev = netdev->priv;
	count = (skb->len < 60) ? 60 : skb->len;
	count = (count & 0x3f) ? count : count + 1;
	memcpy(dev->tx_buff, skb->data, skb->len);
	FILL_BULK_URB(dev->tx_urb, dev->udev, usb_sndbulkpipe(dev->udev, 2),
		      dev->tx_buff, count, write_bulk_callback, dev);
	dev->tx_urb->transfer_buffer_length = count;

	if ((res = usb_submit_urb(dev->tx_urb))) {
		warn("failed tx_urb %d\n", res);
		dev->stats.tx_errors++;
		netif_start_queue(netdev);
	} else {
		dev->stats.tx_packets++;
		dev->stats.tx_bytes += skb->len;
		netdev->trans_start = jiffies;
	}
	dev_kfree_skb(skb);

	return 0;
}

static int rtl8150_open(struct net_device *netdev)
{
	rtl8150_t *dev;
	int res;

	dev = netdev->priv;
	if (dev == NULL) {
		return -ENODEV;
	}

	down(&dev->sem);

	set_registers(dev, IDR, 6, netdev->dev_addr);
	
	FILL_BULK_URB(dev->rx_urb, dev->udev, usb_rcvbulkpipe(dev->udev, 1),
		      dev->rx_buff, RTL8150_MAX_MTU, read_bulk_callback, dev);
	if ((res = usb_submit_urb(dev->rx_urb)))
		warn("%s: rx_urb submit failed: %d", __FUNCTION__, res);
	FILL_INT_URB(dev->intr_urb, dev->udev, usb_rcvintpipe(dev->udev, 3),
		     dev->intr_buff, sizeof(dev->intr_buff), intr_callback,
		     dev, dev->intr_interval);
	if ((res = usb_submit_urb(dev->intr_urb)))
		warn("%s: intr_urb submit failed: %d", __FUNCTION__, res);
	netif_start_queue(netdev);
	enable_net_traffic(dev);
	up(&dev->sem);

	return res;
}

static int rtl8150_close(struct net_device *netdev)
{
	rtl8150_t *dev;
	int res = 0;

	dev = netdev->priv;
	if (!dev)
		return -ENODEV;

	down(&dev->sem);
	if (!test_bit(RTL8150_UNPLUG, &dev->flags))
		disable_net_traffic(dev);
	unlink_all_urbs(dev);
	netif_stop_queue(netdev);
	up(&dev->sem);

	return res;
}

static int rtl8150_ethtool_ioctl(struct net_device *netdev, void *uaddr)
{
	rtl8150_t *dev;
	int cmd;
	char tmp[128];

	dev = netdev->priv;
	if (get_user(cmd, (int *) uaddr))
		return -EFAULT;

	switch (cmd) {
	case ETHTOOL_GDRVINFO:{
			struct ethtool_drvinfo info = { ETHTOOL_GDRVINFO };

			strncpy(info.driver, driver_name, ETHTOOL_BUSINFO_LEN);
			strncpy(info.version, DRIVER_VERSION,
				ETHTOOL_BUSINFO_LEN);
			sprintf(tmp, "usb%d:%d", dev->udev->bus->busnum,
				dev->udev->devnum);
			strncpy(info.bus_info, tmp, ETHTOOL_BUSINFO_LEN);
			if (copy_to_user(uaddr, &info, sizeof(info)))
				return -EFAULT;
			return 0;
		}
	case ETHTOOL_GSET:{
			struct ethtool_cmd ecmd;
			short lpa, bmcr;

			if (copy_from_user(&ecmd, uaddr, sizeof(ecmd)))
				return -EFAULT;
			ecmd.supported = (SUPPORTED_10baseT_Half |
					  SUPPORTED_10baseT_Full |
					  SUPPORTED_100baseT_Half |
					  SUPPORTED_100baseT_Full |
					  SUPPORTED_Autoneg |
					  SUPPORTED_TP | SUPPORTED_MII);
			ecmd.port = PORT_TP;
			ecmd.transceiver = XCVR_INTERNAL;
			ecmd.phy_address = dev->phy;
			get_registers(dev, BMCR, 2, &bmcr);
			get_registers(dev, ANLP, 2, &lpa);
			if (bmcr & BMCR_ANENABLE) {
				ecmd.autoneg = AUTONEG_ENABLE;
				ecmd.speed =
				    (lpa & (LPA_100HALF | LPA_100FULL)) ?
				    SPEED_100 : SPEED_10;
				if (ecmd.speed == SPEED_100)
					ecmd.duplex = (lpa & LPA_100FULL) ?
					    DUPLEX_FULL : DUPLEX_HALF;
				else
					ecmd.duplex = (lpa & LPA_10FULL) ?
					    DUPLEX_FULL : DUPLEX_HALF;
			} else {
				ecmd.autoneg = AUTONEG_DISABLE;
				ecmd.speed = (bmcr & BMCR_SPEED100) ?
				    SPEED_100 : SPEED_10;
				ecmd.duplex = (bmcr & BMCR_FULLDPLX) ?
				    DUPLEX_FULL : DUPLEX_HALF;
			}
			if (copy_to_user(uaddr, &ecmd, sizeof(ecmd)))
				return -EFAULT;
			return 0;
		}
	case ETHTOOL_SSET:
		return -ENOTSUPP;
	case ETHTOOL_GLINK:{
			struct ethtool_value edata = { ETHTOOL_GLINK };

			edata.data = netif_carrier_ok(netdev);
			if (copy_to_user(uaddr, &edata, sizeof(edata)))
				return -EFAULT;
			return 0;
		}
	default:
		return -EOPNOTSUPP;
	}
}

static int rtl8150_ioctl(struct net_device *netdev, struct ifreq *rq, int cmd)
{
	rtl8150_t *dev;
	u16 *data;
	int res;

	dev = netdev->priv;
	data = (u16 *) & rq->ifr_data;
	res = 0;

	down(&dev->sem);
	switch (cmd) {
	case SIOCETHTOOL:
		res = rtl8150_ethtool_ioctl(netdev, rq->ifr_data);
		break;
	case SIOCDEVPRIVATE:
		data[0] = dev->phy;
	case SIOCDEVPRIVATE + 1:
		read_mii_word(dev, dev->phy, (data[1] & 0x1f), &data[3]);
		break;
	case SIOCDEVPRIVATE + 2:
		if (!capable(CAP_NET_ADMIN)) {
			up(&dev->sem);
			return -EPERM;
		}
		write_mii_word(dev, dev->phy, (data[1] & 0x1f), data[2]);
		break;
	default:
		res = -EOPNOTSUPP;
	}
	up(&dev->sem);

	return res;
}

static void *rtl8150_probe(struct usb_device *udev, unsigned int ifnum,
			   const struct usb_device_id *id)
{
	rtl8150_t *dev;
	struct net_device *netdev;

	udev->config[0].bConfigurationValue = 1;
	if (usb_set_configuration(udev, udev->config[0].bConfigurationValue)) {
		err("usb_set_configuration() failed");
		return NULL;
	}
	dev = kmalloc(sizeof(rtl8150_t), GFP_KERNEL);
	if (!dev) {
		err("Out of memory");
		goto exit;
	} else
		memset(dev, 0, sizeof(rtl8150_t));

	netdev = init_etherdev(NULL, 0);
	if (!netdev) {
		kfree(dev);
		err("Oh boy, out of memory again?!?");
		dev = NULL;
		goto exit;
	}

	init_MUTEX(&dev->sem);
	dev->udev = udev;
	dev->netdev = netdev;
	SET_MODULE_OWNER(netdev);
	netdev->priv = dev;
	netdev->open = rtl8150_open;
	netdev->stop = rtl8150_close;
	netdev->do_ioctl = rtl8150_ioctl;
	netdev->watchdog_timeo = RTL8150_TX_TIMEOUT;
	netdev->tx_timeout = rtl8150_tx_timeout;
	netdev->hard_start_xmit = rtl8150_start_xmit;
	netdev->set_multicast_list = rtl8150_set_multicast;
	netdev->set_mac_address = rtl8150_set_mac_address;
	netdev->get_stats = rtl8150_netdev_stats;
	netdev->mtu = RTL8150_MTU;
	dev->intr_interval = 100;	/* 100ms */

	if (rtl8150_reset(dev) || !alloc_all_urbs(dev)) {
		err("couldn't reset the device");
		free_all_urbs(dev);
		unregister_netdev(dev->netdev);
		kfree(netdev);
		kfree(dev);
		dev = NULL;
		goto exit;
	}

	set_ethernet_addr(dev);
	/* let's not be very nasty :-) */
	info("%s: rtl8150 is detected", netdev->name);
exit:
	return dev;
}

static void rtl8150_disconnect(struct usb_device *udev, void *ptr)
{
	rtl8150_t *dev;

	dev = ptr;
	set_bit(RTL8150_UNPLUG, &dev->flags);
	unregister_netdev(dev->netdev);
	unlink_all_urbs(dev);
	free_all_urbs(dev);
	kfree(dev->netdev);
	kfree(dev);
	dev->netdev = NULL;
	dev = NULL;
}

static int __init usb_rtl8150_init(void)
{
	info(DRIVER_DESC " " DRIVER_VERSION);
	return usb_register(&rtl8150_driver);
}

static void __exit usb_rtl8150_exit(void)
{
	usb_deregister(&rtl8150_driver);
}

module_init(usb_rtl8150_init);
module_exit(usb_rtl8150_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
