/*
 *  Copyright (c) 2001 Vojtech Pavlik
 *
 *  CATC EL1210A NetMate USB Ethernet driver
 *
 *  Sponsored by SuSE
 *
 *  Based on the work of
 *		Donald Becker
 * 
 *  Old chipset support added by Simon Evans <spse@secret.org.uk> 2002
 *    - adds support for Belkin F5U011
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
 * 
 * Should you need to contact me, the author, you can do so either by
 * e-mail - mail your message to <vojtech@suse.cz>, or by paper mail:
 * Vojtech Pavlik, Simunkova 1594, Prague 8, 182 00 Czech Republic
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/ethtool.h>
#include <linux/crc32.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>

#undef DEBUG

#include <linux/usb.h>

/*
 * Version information.
 */

#define DRIVER_VERSION "v2.8"
#define DRIVER_AUTHOR "Vojtech Pavlik <vojtech@suse.cz>"
#define DRIVER_DESC "CATC EL1210A NetMate USB Ethernet driver"
#define SHORT_DRIVER_DESC "EL1210A NetMate USB Ethernet"

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

static const char driver_name[] = "catc";

/*
 * Some defines.
 */ 

#define STATS_UPDATE		(HZ)	/* Time between stats updates */
#define TX_TIMEOUT		(5*HZ)	/* Max time the queue can be stopped */
#define PKT_SZ			1536	/* Max Ethernet packet size */
#define RX_MAX_BURST		15	/* Max packets per rx buffer (> 0, < 16) */
#define TX_MAX_BURST		15	/* Max full sized packets per tx buffer (> 0) */
#define CTRL_QUEUE		16	/* Max control requests in flight (power of two) */
#define RX_PKT_SZ		1600	/* Max size of receive packet for F5U011 */

/*
 * Control requests.
 */

enum control_requests {
	ReadMem =	0xf1,
	GetMac =	0xf2,
	Reset =		0xf4,
	SetMac =	0xf5,
	SetRxMode =     0xf5,  /* F5U011 only */
	WriteROM =	0xf8,
	SetReg =	0xfa,
	GetReg =	0xfb,
	WriteMem =	0xfc,
	ReadROM =	0xfd,
};

/*
 * Registers.
 */

enum register_offsets {
	TxBufCount =	0x20,
	RxBufCount =	0x21,
	OpModes =	0x22,
	TxQed =		0x23,
	RxQed =		0x24,
	MaxBurst =	0x25,
	RxUnit =	0x60,
	EthStatus =	0x61,
	StationAddr0 =	0x67,
	EthStats =	0x69,
	LEDCtrl =	0x81,
};

enum eth_stats {
	TxSingleColl =	0x00,
        TxMultiColl =	0x02,
        TxExcessColl =	0x04,
        RxFramErr =	0x06,
};

enum op_mode_bits {
	Op3MemWaits =	0x03,
	OpLenInclude =	0x08,
	OpRxMerge =	0x10,
	OpTxMerge =	0x20,
	OpWin95bugfix =	0x40,
	OpLoopback =	0x80,
};

enum rx_filter_bits {
	RxEnable =	0x01,
	RxPolarity =	0x02,
	RxForceOK =	0x04,
	RxMultiCast =	0x08,
	RxPromisc =	0x10,
	AltRxPromisc =  0x20, /* F5U011 uses different bit */
};

enum led_values {
	LEDFast = 	0x01,
	LEDSlow =	0x02,
	LEDFlash =	0x03,
	LEDPulse =	0x04,
	LEDLink =	0x08,
};

enum link_status {
	LinkNoChange = 0,
	LinkGood     = 1,
	LinkBad      = 2
};

/*
 * The catc struct.
 */

#define CTRL_RUNNING	0
#define RX_RUNNING	1
#define TX_RUNNING	2

struct catc {
	struct net_device *netdev;
	struct usb_device *usbdev;

	struct net_device_stats stats;
	unsigned long flags;

	unsigned int tx_ptr, tx_idx;
	unsigned int ctrl_head, ctrl_tail;
	spinlock_t tx_lock, ctrl_lock;

	u8 tx_buf[2][TX_MAX_BURST * (PKT_SZ + 2)];
	u8 rx_buf[RX_MAX_BURST * (PKT_SZ + 2)];
	u8 irq_buf[2];
	u8 ctrl_buf[64];
	struct usb_ctrlrequest ctrl_dr;

	struct timer_list timer;
	u8 stats_buf[8];
	u16 stats_vals[4];
	unsigned long last_stats;

	u8 multicast[64];

	struct ctrl_queue {
		u8 dir;
		u8 request;
		u16 value;
		u16 index;
		void *buf;
		int len;
		void (*callback)(struct catc *catc, struct ctrl_queue *q);
	} ctrl_queue[CTRL_QUEUE];

	struct urb tx_urb, rx_urb, irq_urb, ctrl_urb;

	u8 is_f5u011;	/* Set if device is an F5U011 */
	u8 rxmode[2];	/* Used for F5U011 */
	atomic_t recq_sz; /* Used for F5U011 - counter of waiting rx packets */
};

/*
 * Useful macros.
 */

#define catc_get_mac(catc, mac)				catc_ctrl_msg(catc, USB_DIR_IN,  GetMac, 0, 0, mac,  6)
#define catc_reset(catc)				catc_ctrl_msg(catc, USB_DIR_OUT, Reset, 0, 0, NULL, 0)
#define catc_set_reg(catc, reg, val)			catc_ctrl_msg(catc, USB_DIR_OUT, SetReg, val, reg, NULL, 0)
#define catc_get_reg(catc, reg, buf)			catc_ctrl_msg(catc, USB_DIR_IN,  GetReg, 0, reg, buf, 1)
#define catc_write_mem(catc, addr, buf, size)		catc_ctrl_msg(catc, USB_DIR_OUT, WriteMem, 0, addr, buf, size)
#define catc_read_mem(catc, addr, buf, size)		catc_ctrl_msg(catc, USB_DIR_IN,  ReadMem, 0, addr, buf, size)

#define f5u011_rxmode(catc, rxmode)			catc_ctrl_msg(catc, USB_DIR_OUT, SetRxMode, 0, 1, rxmode, 2)
#define f5u011_rxmode_async(catc, rxmode)		catc_ctrl_async(catc, USB_DIR_OUT, SetRxMode, 0, 1, &rxmode, 2, NULL)
#define f5u011_mchash_async(catc, hash)			catc_ctrl_async(catc, USB_DIR_OUT, SetRxMode, 0, 2, &hash, 8, NULL)

#define catc_set_reg_async(catc, reg, val)		catc_ctrl_async(catc, USB_DIR_OUT, SetReg, val, reg, NULL, 0, NULL)
#define catc_get_reg_async(catc, reg, cb)		catc_ctrl_async(catc, USB_DIR_IN, GetReg, 0, reg, NULL, 1, cb)
#define catc_write_mem_async(catc, addr, buf, size)	catc_ctrl_async(catc, USB_DIR_OUT, WriteMem, 0, addr, buf, size, NULL)

/*
 * Receive routines.
 */

static void catc_rx_done(struct urb *urb)
{
	struct catc *catc = urb->context;
	u8 *pkt_start = urb->transfer_buffer;
	struct sk_buff *skb;
	int pkt_len, pkt_offset = 0;

	if (!catc->is_f5u011) {
		clear_bit(RX_RUNNING, &catc->flags);
		pkt_offset = 2;
	}

	if (urb->status) {
		dbg("rx_done, status %d, length %d", urb->status, urb->actual_length);
		return;
	}

	do {
		if(!catc->is_f5u011) {
			pkt_len = le16_to_cpup((u16*)pkt_start);
			if (pkt_len > urb->actual_length) {
				catc->stats.rx_length_errors++;
				catc->stats.rx_errors++;
				break;
			}
		} else {
			pkt_len = urb->actual_length;
		}

		if (!(skb = dev_alloc_skb(pkt_len)))
			return;

		skb->dev = catc->netdev;
		eth_copy_and_sum(skb, pkt_start + pkt_offset, pkt_len, 0);
		skb_put(skb, pkt_len);

		skb->protocol = eth_type_trans(skb, catc->netdev);
		netif_rx(skb);

		catc->stats.rx_packets++;
		catc->stats.rx_bytes += pkt_len;

		/* F5U011 only does one packet per RX */
		if (catc->is_f5u011)
			break;
		pkt_start += (((pkt_len + 1) >> 6) + 1) << 6;

	} while (pkt_start - (u8 *) urb->transfer_buffer < urb->actual_length);

	catc->netdev->last_rx = jiffies;

	if (catc->is_f5u011) {
		if (atomic_read(&catc->recq_sz)) {
			int status;
			atomic_dec(&catc->recq_sz);
			dbg("getting extra packet");
			urb->dev = catc->usbdev;
			if ((status = usb_submit_urb(urb)) < 0) {
				dbg("submit(rx_urb) status %d", status);
			}
		} else {
			clear_bit(RX_RUNNING, &catc->flags);
		}
	}
}

static void catc_irq_done(struct urb *urb)
{
	struct catc *catc = urb->context;
	u8 *data = urb->transfer_buffer;
	int status;
	unsigned int hasdata = 0, linksts = LinkNoChange;

	if (!catc->is_f5u011) {
		hasdata = data[1] & 0x80;
		if (data[1] & 0x40)
			linksts = LinkGood;
		else if (data[1] & 0x20)
			linksts = LinkBad;
	} else {
		hasdata = (unsigned int)(be16_to_cpup((u16*)data) & 0x0fff);
		if (data[0] == 0x90)
			linksts = LinkGood;
		else if (data[0] == 0xA0)
			linksts = LinkBad;
	}

	if (urb->status) {
		dbg("irq_done, status %d, data %02x %02x.", urb->status, data[0], data[1]);
		return;
	}

	if (linksts == LinkGood) {
		netif_carrier_on(catc->netdev);
		dbg("link ok");
	}

	if (linksts == LinkBad) {
		netif_carrier_off(catc->netdev);
		dbg("link bad");
	}

	if (hasdata) {
		if (test_and_set_bit(RX_RUNNING, &catc->flags)) {
			if (catc->is_f5u011)
				atomic_inc(&catc->recq_sz);
		} else {
			catc->rx_urb.dev = catc->usbdev;
			if ((status = usb_submit_urb(&catc->rx_urb)) < 0) {
				err("submit(rx_urb) status %d", status);
			}
		} 
	}
}

/*
 * Transmit routines.
 */

static void catc_tx_run(struct catc *catc)
{
	int status;

	if (catc->is_f5u011)
		catc->tx_ptr = (catc->tx_ptr + 63) & ~63;

	catc->tx_urb.transfer_buffer_length = catc->tx_ptr;
	catc->tx_urb.transfer_buffer = catc->tx_buf[catc->tx_idx];
	catc->tx_urb.dev = catc->usbdev;

	if ((status = usb_submit_urb(&catc->tx_urb)) < 0)
		err("submit(tx_urb), status %d", status);

	catc->tx_idx = !catc->tx_idx;
	catc->tx_ptr = 0;

	catc->netdev->trans_start = jiffies;
}

static void catc_tx_done(struct urb *urb)
{
	struct catc *catc = urb->context;
	unsigned long flags;

	if (urb->status == -ECONNRESET) {
		dbg("Tx Reset.");
		urb->transfer_flags &= ~USB_ASYNC_UNLINK;
		urb->status = 0;
		catc->netdev->trans_start = jiffies;
		catc->stats.tx_errors++;
		clear_bit(TX_RUNNING, &catc->flags);
		netif_wake_queue(catc->netdev);
		return;
	}

	if (urb->status) {
		dbg("tx_done, status %d, length %d", urb->status, urb->actual_length);
		return;
	}

	spin_lock_irqsave(&catc->tx_lock, flags);

	if (catc->tx_ptr)
		catc_tx_run(catc);
	else
		clear_bit(TX_RUNNING, &catc->flags);

	netif_wake_queue(catc->netdev);

	spin_unlock_irqrestore(&catc->tx_lock, flags);
}

static int catc_hard_start_xmit(struct sk_buff *skb, struct net_device *netdev)
{
	struct catc *catc = netdev->priv;
	unsigned long flags;
	char *tx_buf;

	spin_lock_irqsave(&catc->tx_lock, flags);

	catc->tx_ptr = (((catc->tx_ptr - 1) >> 6) + 1) << 6;
	tx_buf = catc->tx_buf[catc->tx_idx] + catc->tx_ptr;
	*((u16*)tx_buf) = (catc->is_f5u011) ? 
		cpu_to_be16((u16)skb->len) : cpu_to_le16((u16)skb->len);
	memcpy(tx_buf + 2, skb->data, skb->len);
	catc->tx_ptr += skb->len + 2;

	if (!test_and_set_bit(TX_RUNNING, &catc->flags))
		catc_tx_run(catc);

	if ((catc->is_f5u011 && catc->tx_ptr)
	     || (catc->tx_ptr >= ((TX_MAX_BURST - 1) * (PKT_SZ + 2))))
		netif_stop_queue(netdev);

	spin_unlock_irqrestore(&catc->tx_lock, flags);

	catc->stats.tx_bytes += skb->len;
	catc->stats.tx_packets++;

	dev_kfree_skb(skb);

	return 0;
}

static void catc_tx_timeout(struct net_device *netdev)
{
	struct catc *catc = netdev->priv;

	warn("Transmit timed out.");
	catc->tx_urb.transfer_flags |= USB_ASYNC_UNLINK;
	usb_unlink_urb(&catc->tx_urb);
}

/*
 * Control messages.
 */

static int catc_ctrl_msg(struct catc *catc, u8 dir, u8 request, u16 value, u16 index, void *buf, int len)
{
        int retval = usb_control_msg(catc->usbdev,
		dir ? usb_rcvctrlpipe(catc->usbdev, 0) : usb_sndctrlpipe(catc->usbdev, 0),
		 request, 0x40 | dir, value, index, buf, len, HZ);
        return retval < 0 ? retval : 0;
}

static void catc_ctrl_run(struct catc *catc)
{
	struct ctrl_queue *q = catc->ctrl_queue + catc->ctrl_tail;
	struct usb_device *usbdev = catc->usbdev;
	struct urb *urb = &catc->ctrl_urb;
	struct usb_ctrlrequest *dr = &catc->ctrl_dr;
	int status;

	dr->bRequest = q->request;
	dr->bRequestType = 0x40 | q->dir;
	dr->wValue = cpu_to_le16(q->value);
	dr->wIndex = cpu_to_le16(q->index);
	dr->wLength = cpu_to_le16(q->len);

        urb->pipe = q->dir ? usb_rcvctrlpipe(usbdev, 0) : usb_sndctrlpipe(usbdev, 0);
	urb->transfer_buffer_length = q->len;
	urb->transfer_buffer = catc->ctrl_buf;
	urb->setup_packet = (void *) dr;
	urb->dev = usbdev;

	if (!q->dir && q->buf && q->len)
		memcpy(catc->ctrl_buf, q->buf, q->len);

	if ((status = usb_submit_urb(&catc->ctrl_urb)))
		err("submit(ctrl_urb) status %d", status);
}

static void catc_ctrl_done(struct urb *urb)
{
	struct catc *catc = urb->context;
	struct ctrl_queue *q;
	unsigned long flags;

	if (urb->status)
		dbg("ctrl_done, status %d, len %d.", urb->status, urb->actual_length);

	spin_lock_irqsave(&catc->ctrl_lock, flags);

	q = catc->ctrl_queue + catc->ctrl_tail;

	if (q->dir) {
		if (q->buf && q->len)
			memcpy(q->buf, catc->ctrl_buf, q->len);
		else
			q->buf = catc->ctrl_buf;
	}

	if (q->callback)
		q->callback(catc, q);

	catc->ctrl_tail = (catc->ctrl_tail + 1) & (CTRL_QUEUE - 1);

	if (catc->ctrl_head != catc->ctrl_tail)
		catc_ctrl_run(catc);
	else
		clear_bit(CTRL_RUNNING, &catc->flags);

	spin_unlock_irqrestore(&catc->ctrl_lock, flags);
}

static int catc_ctrl_async(struct catc *catc, u8 dir, u8 request, u16 value,
	u16 index, void *buf, int len, void (*callback)(struct catc *catc, struct ctrl_queue *q))
{
	struct ctrl_queue *q;
	int retval = 0;
	unsigned long flags;

	spin_lock_irqsave(&catc->ctrl_lock, flags);
	
	q = catc->ctrl_queue + catc->ctrl_head;

	q->dir = dir;
	q->request = request;
	q->value = value;
	q->index = index;
	q->buf = buf;
	q->len = len;
	q->callback = callback;

	catc->ctrl_head = (catc->ctrl_head + 1) & (CTRL_QUEUE - 1);

	if (catc->ctrl_head == catc->ctrl_tail) {
		err("ctrl queue full");
		catc->ctrl_tail = (catc->ctrl_tail + 1) & (CTRL_QUEUE - 1);
		retval = -1;
	}

	if (!test_and_set_bit(CTRL_RUNNING, &catc->flags))
		catc_ctrl_run(catc);

	spin_unlock_irqrestore(&catc->ctrl_lock, flags);

	return retval;
}

/*
 * Statistics.
 */

static void catc_stats_done(struct catc *catc, struct ctrl_queue *q)
{
	int index = q->index - EthStats;
	u16 data, last;

	catc->stats_buf[index] = *((char *)q->buf);

	if (index & 1)
		return;

	data = ((u16)catc->stats_buf[index] << 8) | catc->stats_buf[index + 1];
	last = catc->stats_vals[index >> 1];

	switch (index) {
		case TxSingleColl:
		case TxMultiColl:
			catc->stats.collisions += data - last;
			break;
		case TxExcessColl:
			catc->stats.tx_aborted_errors += data - last;
			catc->stats.tx_errors += data - last;
			break;
		case RxFramErr:
			catc->stats.rx_frame_errors += data - last;
			catc->stats.rx_errors += data - last;
			break;
	}

	catc->stats_vals[index >> 1] = data;
}

static void catc_stats_timer(unsigned long data)
{
	struct catc *catc = (void *) data;
	int i;

	for (i = 0; i < 8; i++)
		catc_get_reg_async(catc, EthStats + 7 - i, catc_stats_done);

	mod_timer(&catc->timer, jiffies + STATS_UPDATE);
}

static struct net_device_stats *catc_get_stats(struct net_device *netdev)
{
	struct catc *catc = netdev->priv;
	return &catc->stats;
}

/*
 * Receive modes. Broadcast, Multicast, Promisc.
 */

static void catc_multicast(unsigned char *addr, u8 *multicast)
{
	u32 crc = ether_crc_le(6, addr);
	multicast[(crc >> 3) & 0x3f] |= 1 << (crc & 7);
}

static void catc_set_multicast_list(struct net_device *netdev)
{
	struct catc *catc = netdev->priv;
	struct dev_mc_list *mc;
	u8 broadcast[6];
	u8 rx = RxEnable | RxPolarity | RxMultiCast;
	int i;

	memset(broadcast, 0xff, 6);
	memset(catc->multicast, 0, 64);

	catc_multicast(broadcast, catc->multicast);
	catc_multicast(netdev->dev_addr, catc->multicast);

	if (netdev->flags & IFF_PROMISC) {
		memset(catc->multicast, 0xff, 64);
		rx |= (!catc->is_f5u011) ? RxPromisc : AltRxPromisc;
	} 

	if (netdev->flags & IFF_ALLMULTI) {
		memset(catc->multicast, 0xff, 64);
	} else {
		for (i = 0, mc = netdev->mc_list; mc && i < netdev->mc_count; i++, mc = mc->next) {
			u32 crc = ether_crc_le(6, mc->dmi_addr);
			if (!catc->is_f5u011) {
				catc->multicast[(crc >> 3) & 0x3f] |= 1 << (crc & 7);
			} else {
				catc->multicast[7-(crc >> 29)] |= 1 << ((crc >> 26) & 7);
			}
		}
	}
	if (!catc->is_f5u011) {
		catc_set_reg_async(catc, RxUnit, rx);
		catc_write_mem_async(catc, 0xfa80, catc->multicast, 64);
	} else {
		f5u011_mchash_async(catc, catc->multicast);
		if (catc->rxmode[0] != rx) {
			catc->rxmode[0] = rx;
			dbg("Setting RX mode to %2.2X %2.2X", catc->rxmode[0],
			    catc->rxmode[1]);
			f5u011_rxmode_async(catc, catc->rxmode);
		}
	}
}

/*
 * ioctl's
 */
static int netdev_ethtool_ioctl(struct net_device *dev, void *useraddr)
{
        struct catc *catc = dev->priv;
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
		sprintf(tmp, "usb%d:%d", catc->usbdev->bus->busnum, catc->usbdev->devnum);
                strncpy(info.bus_info, tmp,ETHTOOL_BUSINFO_LEN);
                if (copy_to_user(useraddr, &info, sizeof(info)))
                        return -EFAULT;
                return 0;
        }

	/* get settings */
	case ETHTOOL_GSET:
		if (catc->is_f5u011) {
			struct ethtool_cmd ecmd = { 
				ETHTOOL_GSET, 
				SUPPORTED_10baseT_Half | SUPPORTED_TP, 
				ADVERTISED_10baseT_Half | ADVERTISED_TP, 
				SPEED_10, 
				DUPLEX_HALF, 
				PORT_TP, 
				0, 
				XCVR_INTERNAL, 
				AUTONEG_DISABLE, 
				1, 
				1 
			};
			if (copy_to_user(useraddr, &ecmd, sizeof(ecmd)))
				return -EFAULT;
			return 0;
		} else {
			return -EOPNOTSUPP;
		}

        /* get link status */
        case ETHTOOL_GLINK: {
                struct ethtool_value edata = {ETHTOOL_GLINK};
                edata.data = netif_carrier_ok(dev);
                if (copy_to_user(useraddr, &edata, sizeof(edata)))
                        return -EFAULT;
                return 0;
        }
	}
        /* Note that the ethtool user space code requires EOPNOTSUPP */
        return -EOPNOTSUPP;
}

static int catc_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
        switch(cmd) {
        case SIOCETHTOOL:
	       return netdev_ethtool_ioctl(dev, (void *) rq->ifr_data);
        default:
	       return -ENOTTY; /* Apparently this is the standard ioctl errno */
        }
}


/*
 * Open, close.
 */

static int catc_open(struct net_device *netdev)
{
	struct catc *catc = netdev->priv;
	int status;

	catc->irq_urb.dev = catc->usbdev;
	if ((status = usb_submit_urb(&catc->irq_urb)) < 0) {
		err("submit(irq_urb) status %d", status);
		return -1;
	}

	netif_start_queue(netdev);

	if (!catc->is_f5u011)
		mod_timer(&catc->timer, jiffies + STATS_UPDATE);

	return 0;
}

static int catc_stop(struct net_device *netdev)
{
	struct catc *catc = netdev->priv;

	netif_stop_queue(netdev);

	if (!catc->is_f5u011)
		del_timer_sync(&catc->timer);

	usb_unlink_urb(&catc->rx_urb);
	usb_unlink_urb(&catc->tx_urb);
	usb_unlink_urb(&catc->irq_urb);
	usb_unlink_urb(&catc->ctrl_urb);

	return 0;
}

/*
 * USB probe, disconnect.
 */

static void *catc_probe(struct usb_device *usbdev, unsigned int ifnum, const struct usb_device_id *id)
{
	struct net_device *netdev;
	struct catc *catc;
	u8 broadcast[6];
	int i, pktsz;

	if (usb_set_interface(usbdev, ifnum, 1)) {
                err("Can't set altsetting 1.");
		return NULL;
	}

	catc = kmalloc(sizeof(struct catc), GFP_KERNEL);
	if (!catc)
		return NULL;

	memset(catc, 0, sizeof(struct catc));

	netdev = init_etherdev(0, 0);
	if (!netdev) {
		kfree(catc);
		return NULL;
	}

	netdev->open = catc_open;
	netdev->hard_start_xmit = catc_hard_start_xmit;
	netdev->stop = catc_stop;
	netdev->get_stats = catc_get_stats;
	netdev->tx_timeout = catc_tx_timeout;
	netdev->watchdog_timeo = TX_TIMEOUT;
	netdev->set_multicast_list = catc_set_multicast_list;
	netdev->do_ioctl = catc_ioctl;
	netdev->priv = catc;

	catc->usbdev = usbdev;
	catc->netdev = netdev;

	catc->tx_lock = SPIN_LOCK_UNLOCKED;
	catc->ctrl_lock = SPIN_LOCK_UNLOCKED;

	init_timer(&catc->timer);
	catc->timer.data = (long) catc;
	catc->timer.function = catc_stats_timer;

	/* The F5U011 has the same vendor/product as the netmate 
	 *  but a device version of 0x130
	 */
	if (usbdev->descriptor.idVendor == 0x0423 && 
	    usbdev->descriptor.idProduct == 0xa &&
	    catc->usbdev->descriptor.bcdDevice == 0x0130) {
		dbg("Testing for f5u011");
		catc->is_f5u011 = 1;
		atomic_set(&catc->recq_sz, 0);
		pktsz = RX_PKT_SZ;
	} else {
		pktsz = RX_MAX_BURST * (PKT_SZ + 2);
	}

	FILL_CONTROL_URB(&catc->ctrl_urb, usbdev, usb_sndctrlpipe(usbdev, 0),
		NULL, NULL, 0, catc_ctrl_done, catc);

	FILL_BULK_URB(&catc->tx_urb, usbdev, usb_sndbulkpipe(usbdev, 1),
		NULL, 0, catc_tx_done, catc);

	FILL_BULK_URB(&catc->rx_urb, usbdev, usb_rcvbulkpipe(usbdev, 1),
		catc->rx_buf, pktsz, catc_rx_done, catc);

	FILL_INT_URB(&catc->irq_urb, usbdev, usb_rcvintpipe(usbdev, 2),
                catc->irq_buf, 2, catc_irq_done, catc, 1);

	if (!catc->is_f5u011) {
		dbg("Checking memory size\n");

		i = 0x12345678;
		catc_write_mem(catc, 0x7a80, &i, 4);
		i = 0x87654321;	
		catc_write_mem(catc, 0xfa80, &i, 4);
		catc_read_mem(catc, 0x7a80, &i, 4);
	  
		switch (i) {
		case 0x12345678:
			catc_set_reg(catc, TxBufCount, 8);
			catc_set_reg(catc, RxBufCount, 32);
			dbg("64k Memory\n");
			break;
		default:
			warn("Couldn't detect memory size, assuming 32k");
		case 0x87654321:
			catc_set_reg(catc, TxBufCount, 4);
			catc_set_reg(catc, RxBufCount, 16);
			dbg("32k Memory\n");
			break;
		}
	  
		dbg("Getting MAC from SEEROM.");
	  
		catc_get_mac(catc, netdev->dev_addr);
		
		dbg("Setting MAC into registers.");
	  
		for (i = 0; i < 6; i++)
			catc_set_reg(catc, StationAddr0 - i, netdev->dev_addr[i]);
		
		dbg("Filling the multicast list.");
	  
		memset(broadcast, 0xff, 6);
		catc_multicast(broadcast, catc->multicast);
		catc_multicast(netdev->dev_addr, catc->multicast);
		catc_write_mem(catc, 0xfa80, catc->multicast, 64);
		
		dbg("Clearing error counters.");
		
		for (i = 0; i < 8; i++)
			catc_set_reg(catc, EthStats + i, 0);
		catc->last_stats = jiffies;
		
		dbg("Enabling.");
		
		catc_set_reg(catc, MaxBurst, RX_MAX_BURST);
		catc_set_reg(catc, OpModes, OpTxMerge | OpRxMerge | OpLenInclude | Op3MemWaits);
		catc_set_reg(catc, LEDCtrl, LEDLink);
		catc_set_reg(catc, RxUnit, RxEnable | RxPolarity | RxMultiCast);
	} else {
		dbg("Performing reset\n");
		catc_reset(catc);
		catc_get_mac(catc, netdev->dev_addr);
		
		dbg("Setting RX Mode");
		catc->rxmode[0] = RxEnable | RxPolarity | RxMultiCast;
		catc->rxmode[1] = 0;
		f5u011_rxmode(catc, catc->rxmode);
	}
	dbg("Init done.");
	printk(KERN_INFO "%s: %s USB Ethernet at usb%d:%d.%d, ",
	       netdev->name, (catc->is_f5u011) ? "Belkin F5U011" : "CATC EL1210A NetMate",
	       usbdev->bus->busnum, usbdev->devnum, ifnum);
	for (i = 0; i < 5; i++) printk("%2.2x:", netdev->dev_addr[i]);
	printk("%2.2x.\n", netdev->dev_addr[i]);
	return catc;
}

static void catc_disconnect(struct usb_device *usbdev, void *dev_ptr)
{
	struct catc *catc = dev_ptr;
	unregister_netdev(catc->netdev);
	kfree(catc->netdev);
	kfree(catc);
}

/*
 * Module functions and tables.
 */

static struct usb_device_id catc_id_table [] = {
	{ USB_DEVICE(0x0423, 0xa) },	/* CATC Netmate, Belkin F5U011 */
	{ USB_DEVICE(0x0423, 0xc) },	/* CATC Netmate II, Belkin F5U111 */
	{ USB_DEVICE(0x08d1, 0x1) },	/* smartBridges smartNIC */
	{ }
};

MODULE_DEVICE_TABLE(usb, catc_id_table);

static struct usb_driver catc_driver = {
	name:		driver_name,
	probe:		catc_probe,
	disconnect:	catc_disconnect,
	id_table:	catc_id_table,
};

static int __init catc_init(void)
{
	info(DRIVER_VERSION " " DRIVER_DESC);
	usb_register(&catc_driver);
	return 0;
}

static void __exit catc_exit(void)
{
	usb_deregister(&catc_driver);
}

module_init(catc_init);
module_exit(catc_exit);
