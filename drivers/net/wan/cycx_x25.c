/*
* cycx_x25.c	Cyclom 2X WAN Link Driver.  X.25 module.
*
* Author:	Arnaldo Carvalho de Melo <acme@conectiva.com.br>
*
* Copyright:	(c) 1998-2001 Arnaldo Carvalho de Melo
*
* Based on sdla_x25.c by Gene Kozin <genek@compuserve.com>
*
*		This program is free software; you can redistribute it and/or
*		modify it under the terms of the GNU General Public License
*		as published by the Free Software Foundation; either version
*		2 of the License, or (at your option) any later version.
* ============================================================================
* 2001/01/12	acme		use dev_kfree_skb_irq on interrupt context
* 2000/04/02	acme		dprintk, cycx_debug
* 				fixed the bug introduced in get_dev_by_lcn and
* 				get_dev_by_dte_addr by the anonymous hacker
* 				that converted this driver to softnet
* 2000/01/08	acme		cleanup
* 1999/10/27	acme		use ARPHRD_HWX25 so that the X.25 stack know
*				that we have a X.25 stack implemented in
*				firmware onboard
* 1999/10/18	acme		support for X.25 sockets in if_send,
*				beware: socket(AF_X25...) IS WORK IN PROGRESS,
*				TCP/IP over X.25 via wanrouter not affected,
*				working.
* 1999/10/09	acme		chan_disc renamed to chan_disconnect,
* 				began adding support for X.25 sockets:
* 				conf->protocol in new_if
* 1999/10/05	acme		fixed return E... to return -E...
* 1999/08/10	acme		serialized access to the card thru a spinlock
*				in x25_exec
* 1999/08/09	acme		removed per channel spinlocks
*				removed references to enable_tx_int
* 1999/05/28	acme		fixed nibble_to_byte, ackvc now properly treated
*				if_send simplified
* 1999/05/25	acme		fixed t1, t2, t21 & t23 configuration
*				use spinlocks instead of cli/sti in some points
* 1999/05/24	acme		finished the x25_get_stat function
* 1999/05/23	acme		dev->type = ARPHRD_X25 (tcpdump only works,
*				AFAIT, with ARPHRD_ETHER). This seems to be
*				needed to use socket(AF_X25)...
*				Now the config file must specify a peer media
*				address for svc channels over a crossover cable.
*				Removed hold_timeout from x25_channel_t,
*				not used.
*				A little enhancement in the DEBUG processing
* 1999/05/22	acme		go to DISCONNECTED in disconnect_confirm_intr,
*				instead of chan_disc.
* 1999/05/16	marcelo		fixed timer initialization in SVCs
* 1999/01/05	acme		x25_configure now get (most of) all
*				parameters...
* 1999/01/05	acme		pktlen now (correctly) uses log2 (value
*				configured)
* 1999/01/03	acme		judicious use of data types (u8, u16, u32, etc)
* 1999/01/03	acme		cyx_isr: reset dpmbase to acknowledge
*				indication (interrupt from cyclom 2x)
* 1999/01/02	acme		cyx_isr: first hackings...
* 1999/01/0203  acme 		when initializing an array don't give less
*				elements than declared...
* 				example: char send_cmd[6] = "?\xFF\x10";
*          			you'll gonna lose a couple hours, 'cause your
*				brain won't admit that there's an error in the
*				above declaration...  the side effect is that
*				memset is put into the unresolved symbols
*				instead of using the inline memset functions...
* 1999/01/02    acme 		began chan_connect, chan_send, x25_send
* 1998/12/31	acme		x25_configure
*				this code can be compiled as non module
* 1998/12/27	acme		code cleanup
*				IPX code wiped out! let's decrease code
*				complexity for now, remember: I'm learning! :)
*                               bps_to_speed_code OK
* 1998/12/26	acme		Minimal debug code cleanup
* 1998/08/08	acme		Initial version.
*/

#define CYCLOMX_X25_DEBUG 1

#include <linux/version.h>
#include <linux/kernel.h>	/* printk(), and other useful stuff */
#include <linux/stddef.h>	/* offsetof(), etc. */
#include <linux/errno.h>	/* return codes */
#include <linux/string.h>	/* inline memset(), etc. */
#include <linux/slab.h>	/* kmalloc(), kfree() */
#include <linux/wanrouter.h>	/* WAN router definitions */
#include <asm/byteorder.h>	/* htons(), etc. */
#include <linux/if_arp.h>       /* ARPHRD_HWX25 */
#include <linux/cyclomx.h>	/* Cyclom 2X common user API definitions */
#include <linux/cycx_x25.h>	/* X.25 firmware API definitions */

/* Defines & Macros */
#define MAX_CMD_RETRY	5
#define X25_CHAN_MTU	2048	/* unfragmented logical channel MTU */

/* Data Structures */
/* This is an extension of the 'struct net_device' we create for each network
   interface to keep the rest of X.25 channel-specific data. */
typedef struct x25_channel {
	/* This member must be first. */
	struct net_device *slave;	/* WAN slave */

	char name[WAN_IFNAME_SZ+1];	/* interface name, ASCIIZ */
	char addr[WAN_ADDRESS_SZ+1];	/* media address, ASCIIZ */
	char *local_addr;		/* local media address, ASCIIZ -
					   svc thru crossover cable */
	s16 lcn;			/* logical channel number/conn.req.key*/
	u8 link;
	struct timer_list timer;	/* timer used for svc channel disc. */
	u16 protocol;			/* ethertype, 0 - multiplexed */
	u8 svc;				/* 0 - permanent, 1 - switched */
	u8 state;			/* channel state */
	u8 drop_sequence;		/* mark sequence for dropping */
	u32 idle_tmout;			/* sec, before disconnecting */
	struct sk_buff *rx_skb;		/* receive socket buffer */
	cycx_t *card;			/* -> owner */
	struct net_device_stats ifstats;/* interface statistics */
} x25_channel_t;

/* Function Prototypes */
/* WAN link driver entry points. These are called by the WAN router module. */
static int update (wan_device_t *wandev),
	   new_if (wan_device_t *wandev, struct net_device *dev,
		   wanif_conf_t *conf),
	   del_if (wan_device_t *wandev, struct net_device *dev);

/* Network device interface */
static int if_init (struct net_device *dev),
	   if_open (struct net_device *dev),
	   if_close (struct net_device *dev),
	   if_header (struct sk_buff *skb, struct net_device *dev,
		      u16 type, void *daddr, void *saddr, unsigned len),
	   if_rebuild_hdr (struct sk_buff *skb),
	   if_send (struct sk_buff *skb, struct net_device *dev);

static struct net_device_stats * if_stats (struct net_device *dev);

/* Interrupt handlers */
static void cyx_isr (cycx_t *card),
	    tx_intr (cycx_t *card, TX25Cmd *cmd),
	    rx_intr (cycx_t *card, TX25Cmd *cmd),
	    log_intr (cycx_t *card, TX25Cmd *cmd),
	    stat_intr (cycx_t *card, TX25Cmd *cmd),
	    connect_confirm_intr (cycx_t *card, TX25Cmd *cmd),
	    disconnect_confirm_intr (cycx_t *card, TX25Cmd *cmd),
	    connect_intr (cycx_t *card, TX25Cmd *cmd),
	    disconnect_intr (cycx_t *card, TX25Cmd *cmd),
	    spur_intr (cycx_t *card, TX25Cmd *cmd);

/* X.25 firmware interface functions */
static int x25_configure (cycx_t *card, TX25Config *conf),
	   x25_get_stats (cycx_t *card),
	   x25_send (cycx_t *card, u8 link, u8 lcn, u8 bitm, int len,
		     void *buf),
	   x25_connect_response (cycx_t *card, x25_channel_t *chan),
	   x25_disconnect_response (cycx_t *card, u8 link, u8 lcn);

/* channel functions */
static int chan_connect (struct net_device *dev),
    	   chan_send (struct net_device *dev, struct sk_buff *skb);

static void chan_disconnect (struct net_device *dev),
	    chan_x25_send_event(struct net_device *dev, u8 event);

/* Miscellaneous functions */
static void set_chan_state (struct net_device *dev, u8 state),
	    chan_timer (unsigned long d);

static void nibble_to_byte (u8 *s, u8 *d, u8 len, u8 nibble),
	    reset_timer (struct net_device *dev);

static u8 bps_to_speed_code (u32 bps);
static u8 log2 (u32 n);

static unsigned dec_to_uint (u8 *str, int len);

static struct net_device *get_dev_by_lcn (wan_device_t *wandev, s16 lcn);
static struct net_device *get_dev_by_dte_addr (wan_device_t *wandev, char *dte);

#ifdef CYCLOMX_X25_DEBUG
static void hex_dump(char *msg, unsigned char *p, int len);
static void x25_dump_config(TX25Config *conf);
static void x25_dump_stats(TX25Stats *stats);
static void x25_dump_devs(wan_device_t *wandev);
#else
#define hex_dump(msg, p, len)
#define x25_dump_config(conf)
#define x25_dump_stats(stats)
#define x25_dump_devs(wandev)
#endif
/* Public Functions */

/* X.25 Protocol Initialization routine.
 *
 * This routine is called by the main Cyclom 2X module during setup.  At this
 * point adapter is completely initialized and X.25 firmware is running.
 *  o configure adapter
 *  o initialize protocol-specific fields of the adapter data space.
 *
 * Return:	0	o.k.
 *		< 0	failure.  */
int cyx_init (cycx_t *card, wandev_conf_t *conf)
{
	TX25Config cfg;

	/* Verify configuration ID */
	if (conf->config_id != WANCONFIG_X25) {
		printk(KERN_INFO "%s: invalid configuration ID %u!\n",
				 card->devname, conf->config_id);
		return -EINVAL;
	}

	/* Initialize protocol-specific fields */
	card->mbox  = card->hw.dpmbase + X25_MBOX_OFFS;
	card->u.x.connection_keys = 0;
	card->u.x.lock = SPIN_LOCK_UNLOCKED;

	/* Configure adapter. Here we set reasonable defaults, then parse
	 * device configuration structure and set configuration options.
	 * Most configuration options are verified and corrected (if
	 * necessary) since we can't rely on the adapter to do so and don't
	 * want it to fail either. */
	memset(&cfg, 0, sizeof(cfg));
	cfg.link = 0;
	cfg.clock = conf->clocking == WANOPT_EXTERNAL ? 8 : 55;
	cfg.speed = bps_to_speed_code(conf->bps);
	cfg.n3win = 7;
	cfg.n2win = 2;
	cfg.n2 = 5;
	cfg.nvc = 1;
	cfg.npvc = 1;
	cfg.flags = 0x02; /* default = V35 */
	cfg.t1 = 10;   /* line carrier timeout */
	cfg.t2 = 29;   /* tx timeout */
	cfg.t21 = 180; /* CALL timeout */
	cfg.t23 = 180; /* CLEAR timeout */

	/* adjust MTU */
	if (!conf->mtu || conf->mtu >= 512)
		card->wandev.mtu = 512;
	else if (conf->mtu >= 256)
		card->wandev.mtu = 256;
	else if (conf->mtu >= 128)
		card->wandev.mtu = 128;
	else
		card->wandev.mtu = 64;

	cfg.pktlen = log2(card->wandev.mtu);

	if (conf->station == WANOPT_DTE) {
		cfg.locaddr = 3; /* DTE */
		cfg.remaddr = 1; /* DCE */
	} else {
		cfg.locaddr = 1; /* DCE */
		cfg.remaddr = 3; /* DTE */
	}

        if (conf->interface == WANOPT_RS232)
	        cfg.flags = 0;      /* FIXME just reset the 2nd bit */

	if (conf->u.x25.hi_pvc) {
		card->u.x.hi_pvc = min_t(unsigned int, conf->u.x25.hi_pvc, 4095);
		card->u.x.lo_pvc = min_t(unsigned int, conf->u.x25.lo_pvc, card->u.x.hi_pvc);
	}

	if (conf->u.x25.hi_svc) {
		card->u.x.hi_svc = min_t(unsigned int, conf->u.x25.hi_svc, 4095);
		card->u.x.lo_svc = min_t(unsigned int, conf->u.x25.lo_svc, card->u.x.hi_svc);
	}

	if (card->u.x.lo_pvc == 255)
		cfg.npvc = 0;
	else
		cfg.npvc = card->u.x.hi_pvc - card->u.x.lo_pvc + 1;

	cfg.nvc = card->u.x.hi_svc - card->u.x.lo_svc + 1 + cfg.npvc;

	if (conf->u.x25.hdlc_window)
		cfg.n2win = min_t(unsigned int, conf->u.x25.hdlc_window, 7);

	if (conf->u.x25.pkt_window)
		cfg.n3win = min_t(unsigned int, conf->u.x25.pkt_window, 7);

	if (conf->u.x25.t1)
		cfg.t1 = min_t(unsigned int, conf->u.x25.t1, 30);

	if (conf->u.x25.t2)
		cfg.t2 = min_t(unsigned int, conf->u.x25.t2, 30);

	if (conf->u.x25.t11_t21)
		cfg.t21 = min_t(unsigned int, conf->u.x25.t11_t21, 30);

	if (conf->u.x25.t13_t23)
		cfg.t23 = min_t(unsigned int, conf->u.x25.t13_t23, 30);

	if (conf->u.x25.n2)
		cfg.n2 = min_t(unsigned int, conf->u.x25.n2, 30);

	/* initialize adapter */
	if (x25_configure(card, &cfg))
		return -EIO;

	/* Initialize protocol-specific fields of adapter data space */
	card->wandev.bps	= conf->bps;
	card->wandev.interface	= conf->interface;
	card->wandev.clocking	= conf->clocking;
	card->wandev.station	= conf->station;
	card->isr		= cyx_isr;
	card->exec		= NULL;
	card->wandev.update	= update;
	card->wandev.new_if	= new_if;
	card->wandev.del_if	= del_if;
	card->wandev.state	= WAN_DISCONNECTED;

	return 0;
}

/* WAN Device Driver Entry Points */
/* Update device status & statistics. */
static int update (wan_device_t *wandev)
{
	/* sanity checks */
	if (!wandev || !wandev->private)
		return -EFAULT;

	if (wandev->state == WAN_UNCONFIGURED)
		return -ENODEV;

	x25_get_stats(wandev->private);

	return 0;
}

/* Create new logical channel.
 * This routine is called by the router when ROUTER_IFNEW IOCTL is being
 * handled.
 * o parse media- and hardware-specific configuration
 * o make sure that a new channel can be created
 * o allocate resources, if necessary
 * o prepare network device structure for registration.
 *
 * Return:	0	o.k.
 *		< 0	failure (channel will not be created) */
static int new_if (wan_device_t *wandev, struct net_device *dev,
		   wanif_conf_t *conf)
{
	cycx_t *card = wandev->private;
	x25_channel_t *chan;
	int err = 0;

	if (!conf->name[0] || strlen(conf->name) > WAN_IFNAME_SZ) {
		printk(KERN_INFO "%s: invalid interface name!\n",card->devname);
		return -EINVAL;
	}

	/* allocate and initialize private data */
	if ((chan = kmalloc(sizeof(x25_channel_t), GFP_KERNEL)) == NULL)
		return -ENOMEM;

	memset(chan, 0, sizeof(x25_channel_t));
	strcpy(chan->name, conf->name);
	chan->card = card;
	chan->link = conf->port;
	chan->protocol = conf->protocol ? ETH_P_X25 : ETH_P_IP;
	chan->rx_skb = NULL;
	/* only used in svc connected thru crossover cable */
	chan->local_addr = NULL;

	if (conf->addr[0] == '@') {	/* SVC */
		int len = strlen(conf->local_addr);

		if (len) {
			if (len > WAN_ADDRESS_SZ) {
				printk(KERN_ERR "%s: %s local addr too long!\n",
						wandev->name, chan->name);
				kfree(chan);
				return -EINVAL;
			} else {
				chan->local_addr = kmalloc(len + 1, GFP_KERNEL);

				if (!chan->local_addr) {
					kfree(chan);
					return -ENOMEM;
				}
			}

                	strncpy(chan->local_addr, conf->local_addr,
				WAN_ADDRESS_SZ);
		}

                chan->svc = 1;
                strncpy(chan->addr, &conf->addr[1], WAN_ADDRESS_SZ);
		init_timer(&chan->timer);
		chan->timer.function = chan_timer;
		chan->timer.data = (unsigned long)dev;

                /* Set channel timeouts (default if not specified) */
                chan->idle_tmout = conf->idle_timeout ? conf->idle_timeout : 90;
        } else if (is_digit(conf->addr[0])) {	/* PVC */
		s16 lcn = dec_to_uint(conf->addr, 0);

		if (lcn >= card->u.x.lo_pvc && lcn <= card->u.x.hi_pvc)
			chan->lcn = lcn;
		else {
			printk(KERN_ERR
				"%s: PVC %u is out of range on interface %s!\n",
				wandev->name, lcn, chan->name);
			err = -EINVAL;
		}
	} else {
		printk(KERN_ERR "%s: invalid media address on interface %s!\n",
				wandev->name, chan->name);
		err = -EINVAL;
	}

	if (err) {
		if (chan->local_addr)
			kfree(chan->local_addr);

		kfree(chan);
		return err;
	}

	/* prepare network device data space for registration */
	strcpy(dev->name, chan->name);
	dev->init = if_init;
	dev->priv = chan;

	return 0;
}

/* Delete logical channel. */
static int del_if (wan_device_t *wandev, struct net_device *dev)
{
	if (dev->priv) {
		x25_channel_t *chan = dev->priv;

		if (chan->svc) {
			if (chan->local_addr)
				kfree(chan->local_addr);

			if (chan->state == WAN_CONNECTED)
				del_timer(&chan->timer);
		}

		kfree(chan);
		dev->priv = NULL;
	}

	return 0;
}

/* Network Device Interface */
/* Initialize Linux network interface.
 *
 * This routine is called only once for each interface, during Linux network
 * interface registration.  Returning anything but zero will fail interface
 * registration. */
static int if_init (struct net_device *dev)
{
	x25_channel_t *chan = dev->priv;
	cycx_t *card = chan->card;
	wan_device_t *wandev = &card->wandev;

	/* Initialize device driver entry points */
	dev->open = if_open;
	dev->stop = if_close;
	dev->hard_header = if_header;
	dev->rebuild_header = if_rebuild_hdr;
	dev->hard_start_xmit = if_send;
	dev->get_stats = if_stats;

	/* Initialize media-specific parameters */
	dev->mtu = X25_CHAN_MTU;
	dev->type = ARPHRD_HWX25;	/* ARP h/w type */
	dev->hard_header_len = 0;	/* media header length */
	dev->addr_len = 0;		/* hardware address length */

	if (!chan->svc)
		*(u16*)dev->dev_addr = htons(chan->lcn);

	/* Initialize hardware parameters (just for reference) */
	dev->irq = wandev->irq;
	dev->dma = wandev->dma;
	dev->base_addr = wandev->ioport;
	dev->mem_start = (unsigned long)wandev->maddr;
	dev->mem_end = (unsigned long)(wandev->maddr + wandev->msize - 1);
	dev->flags |= IFF_NOARP;

        /* Set transmit buffer queue length */
        dev->tx_queue_len = 10;

	/* Initialize socket buffers */
	set_chan_state(dev, WAN_DISCONNECTED);

	return 0;
}

/* Open network interface.
 * o prevent module from unloading by incrementing use count
 * o if link is disconnected then initiate connection
 *
 * Return 0 if O.k. or errno.  */
static int if_open (struct net_device *dev)
{
	x25_channel_t *chan = dev->priv;
	cycx_t *card = chan->card;

	if (netif_running(dev))
		return -EBUSY; /* only one open is allowed */ 

	netif_start_queue(dev);
	cyclomx_mod_inc_use_count(card);

	return 0;
}

/* Close network interface.
 * o reset flags.
 * o if there's no more open channels then disconnect physical link. */
static int if_close (struct net_device *dev)
{
	x25_channel_t *chan = dev->priv;
	cycx_t *card = chan->card;

	netif_stop_queue(dev);
	
	if (chan->state == WAN_CONNECTED || chan->state == WAN_CONNECTING)
		chan_disconnect(dev);
		
	cyclomx_mod_dec_use_count(card);

	return 0;
}

/* Build media header.
 * o encapsulate packet according to encapsulation type.
 *
 * The trick here is to put packet type (Ethertype) into 'protocol' field of
 * the socket buffer, so that we don't forget it.  If encapsulation fails,
 * set skb->protocol to 0 and discard packet later.
 *
 * Return:	media header length. */
static int if_header (struct sk_buff *skb, struct net_device *dev,
		      u16 type, void *daddr, void *saddr, unsigned len)
{
	skb->protocol = type;

	return dev->hard_header_len;
}

/* * Re-build media header.
 * Return:	1	physical address resolved.
 *		0	physical address not resolved */
static int if_rebuild_hdr (struct sk_buff *skb)
{
	return 1;
}

/* Send a packet on a network interface.
 * o set busy flag (marks start of the transmission).
 * o check link state. If link is not up, then drop the packet.
 * o check channel status. If it's down then initiate a call.
 * o pass a packet to corresponding WAN device.
 * o free socket buffer
 *
 * Return:	0	complete (socket buffer must be freed)
 *		non-0	packet may be re-transmitted (tbusy must be set)
 *
 * Notes:
 * 1. This routine is called either by the protocol stack or by the "net
 *    bottom half" (with interrupts enabled).
 * 2. Setting tbusy flag will inhibit further transmit requests from the
 *    protocol stack and can be used for flow control with protocol layer. */
static int if_send (struct sk_buff *skb, struct net_device *dev)
{
	x25_channel_t *chan = dev->priv;
	cycx_t *card = chan->card;

	if (!chan->svc)
		chan->protocol = skb->protocol;

	if (card->wandev.state != WAN_CONNECTED)
		++chan->ifstats.tx_dropped;
        else if (chan->svc && chan->protocol &&
		 chan->protocol != skb->protocol) {
                printk(KERN_INFO
                        "%s: unsupported Ethertype 0x%04X on interface %s!\n",
                        card->devname, skb->protocol, dev->name);
                ++chan->ifstats.tx_errors;
        } else if (chan->protocol == ETH_P_IP) {
		switch (chan->state) {
			case WAN_DISCONNECTED:
				if (chan_connect(dev)) {
					netif_stop_queue(dev);
					return -EBUSY;
				}
				/* fall thru */
			case WAN_CONNECTED:
				reset_timer(dev);
				dev->trans_start = jiffies;
				netif_stop_queue(dev);

				if (chan_send(dev, skb))
					return -EBUSY;

				break;
			default:
				++chan->ifstats.tx_dropped;	
				++card->wandev.stats.tx_dropped;
		} 
	} else { /* chan->protocol == ETH_P_X25 */
		switch (skb->data[0]) {
			case 0: break;
			case 1: /* Connect request */
				chan_connect(dev);
				goto free_packet;
		        case 2: /* Disconnect request */
				chan_disconnect(dev);
				goto free_packet;
		        default:
				printk(KERN_INFO
				       "%s: unknown %d x25-iface request on %s!\n",
                       			card->devname, skb->data[0], dev->name);
               			++chan->ifstats.tx_errors;
				goto free_packet;
		}

		skb_pull(skb, 1); /* Remove control byte */
		reset_timer(dev);
		dev->trans_start = jiffies;
		netif_stop_queue(dev);
		
		if (chan_send(dev, skb)) {
			/* prepare for future retransmissions */
			skb_push(skb, 1);
			return -EBUSY;
		}
	}

free_packet:
	dev_kfree_skb(skb);

	return 0;
}

/* Get Ethernet-style interface statistics.
 * Return a pointer to struct net_device_stats */
static struct net_device_stats *if_stats (struct net_device *dev)
{
        x25_channel_t *chan = dev->priv;

	return chan ? &chan->ifstats : NULL;
}

/* Interrupt Handlers */
/* X.25 Interrupt Service Routine. */
static void cyx_isr (cycx_t *card)
{
	TX25Cmd cmd;
	u16 z = 0;

	card->in_isr = 1;
	card->buff_int_mode_unbusy = 0;
	cycx_peek(&card->hw, X25_RXMBOX_OFFS, &cmd, sizeof(cmd));

	switch (cmd.command) {
		case X25_DATA_INDICATION:
			rx_intr(card, &cmd);
			break;
		case X25_ACK_FROM_VC:
			tx_intr(card, &cmd);
			break;
		case X25_LOG: 
			log_intr(card, &cmd);
			break;
		case X25_STATISTIC: 
			stat_intr(card, &cmd);
			break;
		case X25_CONNECT_CONFIRM:
			connect_confirm_intr(card, &cmd);
			break;
		case X25_CONNECT_INDICATION:
			connect_intr(card, &cmd);
			break;
		case X25_DISCONNECT_INDICATION:
			disconnect_intr(card, &cmd);
			break;
		case X25_DISCONNECT_CONFIRM:
			disconnect_confirm_intr(card, &cmd);
			break;
		case X25_LINE_ON:
			cyclomx_set_state(card, WAN_CONNECTED);
			break;
		case X25_LINE_OFF:
			cyclomx_set_state(card, WAN_DISCONNECTED);
			break;
		default: 
			spur_intr(card, &cmd); /* unwanted interrupt */
	}

	cycx_poke(&card->hw, 0, &z, sizeof(z));
	cycx_poke(&card->hw, X25_RXMBOX_OFFS, &z, sizeof(z));
	card->in_isr = 0;
}

/* Transmit interrupt handler.
 *	o Release socket buffer
 *	o Clear 'tbusy' flag */
static void tx_intr (cycx_t *card, TX25Cmd *cmd)
{
	struct net_device *dev;
	wan_device_t *wandev = &card->wandev;
	u8 lcn;

	cycx_peek(&card->hw, cmd->buf, &lcn, sizeof(lcn));

	/* unbusy device and then dev_tint(); */
	if ((dev = get_dev_by_lcn(wandev, lcn)) != NULL) {
		card->buff_int_mode_unbusy = 1;
		netif_wake_queue(dev);
	} else
		printk(KERN_ERR "%s:ackvc for inexistent lcn %d\n",
				 card->devname, lcn);
}

/* Receive interrupt handler.
 * This routine handles fragmented IP packets using M-bit according to the
 * RFC1356.
 * o map logical channel number to network interface.
 * o allocate socket buffer or append received packet to the existing one.
 * o if M-bit is reset (i.e. it's the last packet in a sequence) then 
 *   decapsulate packet and pass socket buffer to the protocol stack.
 *
 * Notes:
 * 1. When allocating a socket buffer, if M-bit is set then more data is
 *    coming and we have to allocate buffer for the maximum IP packet size
 *    expected on this channel.
 * 2. If something goes wrong and X.25 packet has to be dropped (e.g. no
 *    socket buffers available) the whole packet sequence must be discarded. */
static void rx_intr (cycx_t *card, TX25Cmd *cmd)
{
	wan_device_t *wandev = &card->wandev;
	struct net_device *dev;
	x25_channel_t *chan;
	struct sk_buff *skb;
	u8 bitm, lcn;
	int pktlen = cmd->len - 5;

	cycx_peek(&card->hw, cmd->buf, &lcn, sizeof(lcn));
	cycx_peek(&card->hw, cmd->buf + 4, &bitm, sizeof(bitm));
	bitm &= 0x10;

	if ((dev = get_dev_by_lcn(wandev, lcn)) == NULL) {
		/* Invalid channel, discard packet */
		printk(KERN_INFO "%s: receiving on orphaned LCN %d!\n",
				 card->devname, lcn);
		return;
	}

	chan = dev->priv;
	reset_timer(dev);

	if (chan->drop_sequence) {
		if (!bitm)
			chan->drop_sequence = 0;
		else
			return;
	}

	if ((skb = chan->rx_skb) == NULL) {
		/* Allocate new socket buffer */
		int bufsize = bitm ? dev->mtu : pktlen;

		if ((skb = dev_alloc_skb((chan->protocol == ETH_P_X25 ? 1 : 0) +
					 bufsize +
					 dev->hard_header_len)) == NULL) {
			printk(KERN_INFO "%s: no socket buffers available!\n",
					 card->devname);
			chan->drop_sequence = 1;
			++chan->ifstats.rx_dropped;
			return;
		}

		if (chan->protocol == ETH_P_X25) /* X.25 socket layer control */
			/* 0 = data packet (dev_alloc_skb zeroed skb->data) */
			skb_put(skb, 1); 

		skb->dev = dev;
		skb->protocol = htons(chan->protocol);
		chan->rx_skb = skb;
	}

	if (skb_tailroom(skb) < pktlen) {
		/* No room for the packet. Call off the whole thing! */
		dev_kfree_skb_irq(skb);
		chan->rx_skb = NULL;

		if (bitm)
			chan->drop_sequence = 1;

		printk(KERN_INFO "%s: unexpectedly long packet sequence "
			"on interface %s!\n", card->devname, dev->name);
		++chan->ifstats.rx_length_errors;
		return;
	}

	/* Append packet to the socket buffer  */
	cycx_peek(&card->hw, cmd->buf + 5, skb_put(skb, pktlen), pktlen);

	if (bitm)
		return; /* more data is coming */

	chan->rx_skb = NULL;		/* dequeue packet */

	++chan->ifstats.rx_packets;
	chan->ifstats.rx_bytes += pktlen;

	skb->mac.raw = skb->data;
	netif_rx(skb);
	dev->last_rx = jiffies;		/* timestamp */
}

/* Connect interrupt handler. */
static void connect_intr (cycx_t *card, TX25Cmd *cmd)
{
	wan_device_t *wandev = &card->wandev;
	struct net_device *dev = NULL;
	x25_channel_t *chan;
	u8 d[32],
	   loc[24],
	   rem[24];
	u8 lcn, sizeloc, sizerem;

	cycx_peek(&card->hw, cmd->buf, &lcn, sizeof(lcn));
	cycx_peek(&card->hw, cmd->buf + 5, &sizeloc, sizeof(sizeloc));
	cycx_peek(&card->hw, cmd->buf + 6, d, cmd->len - 6);

	sizerem = sizeloc >> 4;
	sizeloc &= 0x0F;

	loc[0] = rem[0] = '\0';

	if (sizeloc)
		nibble_to_byte(d, loc, sizeloc, 0);

	if (sizerem)
		nibble_to_byte(d + (sizeloc >> 1), rem, sizerem, sizeloc & 1);

	dprintk(1, KERN_INFO "connect_intr:lcn=%d, local=%s, remote=%s\n",
			  lcn, loc, rem);

	if ((dev = get_dev_by_dte_addr(wandev, rem)) == NULL) {
		/* Invalid channel, discard packet */
		printk(KERN_INFO "%s: connect not expected: remote %s!\n",
				 card->devname, rem);
		return;
	}

	chan = dev->priv;
	chan->lcn = lcn;
	x25_connect_response(card, chan);
	set_chan_state(dev, WAN_CONNECTED);
}

/* Connect confirm interrupt handler. */
static void connect_confirm_intr (cycx_t *card, TX25Cmd *cmd)
{
	wan_device_t *wandev = &card->wandev;
	struct net_device *dev;
	x25_channel_t *chan;
	u8 lcn, key;

	cycx_peek(&card->hw, cmd->buf, &lcn, sizeof(lcn));
	cycx_peek(&card->hw, cmd->buf + 1, &key, sizeof(key));
	dprintk(1, KERN_INFO "%s: connect_confirm_intr:lcn=%d, key=%d\n",
			  card->devname, lcn, key);

	if ((dev = get_dev_by_lcn(wandev, -key)) == NULL) {
		/* Invalid channel, discard packet */
		clear_bit(--key, (void*)&card->u.x.connection_keys);
		printk(KERN_INFO "%s: connect confirm not expected: lcn %d, "
				 "key=%d!\n", card->devname, lcn, key);
		return;
	}

	clear_bit(--key, (void*)&card->u.x.connection_keys);
	chan = dev->priv;
	chan->lcn = lcn;
	set_chan_state(dev, WAN_CONNECTED);
}

/* Disconnect confirm interrupt handler. */
static void disconnect_confirm_intr (cycx_t *card, TX25Cmd *cmd)
{
	wan_device_t *wandev = &card->wandev;
	struct net_device *dev;
	u8 lcn;

	cycx_peek(&card->hw, cmd->buf, &lcn, sizeof(lcn));
	dprintk(1, KERN_INFO "%s: disconnect_confirm_intr:lcn=%d\n",
			  card->devname, lcn);
	if ((dev = get_dev_by_lcn(wandev, lcn)) == NULL) {
		/* Invalid channel, discard packet */
		printk(KERN_INFO "%s:disconnect confirm not expected!:lcn %d\n",
				 card->devname, lcn);
		return;
	}

	set_chan_state(dev, WAN_DISCONNECTED);
}

/* disconnect interrupt handler. */
static void disconnect_intr (cycx_t *card, TX25Cmd *cmd)
{
	wan_device_t *wandev = &card->wandev;
	struct net_device *dev;
	u8 lcn;

	cycx_peek(&card->hw, cmd->buf, &lcn, sizeof(lcn));
	dprintk(1, KERN_INFO "disconnect_intr:lcn=%d\n", lcn);

	if ((dev = get_dev_by_lcn(wandev, lcn)) != NULL) {
		x25_channel_t *chan = dev->priv;

		x25_disconnect_response(card, chan->link, lcn);
		set_chan_state(dev, WAN_DISCONNECTED);
	} else
		x25_disconnect_response(card, 0, lcn);
}

/* LOG interrupt handler. */
static void log_intr (cycx_t *card, TX25Cmd *cmd)
{
#if CYCLOMX_X25_DEBUG
	char bf[20];
	u16 size, toread, link, msg_code;
	u8 code, routine;

	cycx_peek(&card->hw, cmd->buf, &msg_code, sizeof(msg_code));
	cycx_peek(&card->hw, cmd->buf + 2, &link, sizeof(link));
	cycx_peek(&card->hw, cmd->buf + 4, &size, sizeof(size));
	/* at most 20 bytes are available... thanks to Daniela :) */
	toread = size < 20 ? size : 20;
	cycx_peek(&card->hw, cmd->buf + 10, &bf, toread);
	cycx_peek(&card->hw, cmd->buf + 10 + toread, &code, 1);
	cycx_peek(&card->hw, cmd->buf + 10 + toread + 1, &routine, 1);

	printk(KERN_INFO "cyx_isr: X25_LOG (0x4500) indic.:\n");
	printk(KERN_INFO "cmd->buf=0x%X\n", cmd->buf);
	printk(KERN_INFO "Log message code=0x%X\n", msg_code);
	printk(KERN_INFO "Link=%d\n", link);
	printk(KERN_INFO "log code=0x%X\n", code);
	printk(KERN_INFO "log routine=0x%X\n", routine);
	printk(KERN_INFO "Message size=%d\n", size);
	hex_dump("Message", bf, toread);
#endif
}

/* STATISTIC interrupt handler. */
static void stat_intr (cycx_t *card, TX25Cmd *cmd)
{
	cycx_peek(&card->hw, cmd->buf, &card->u.x.stats,
		  sizeof(card->u.x.stats));
	hex_dump("stat_intr", (unsigned char*)&card->u.x.stats,
		 sizeof(card->u.x.stats));
	x25_dump_stats(&card->u.x.stats);
	wake_up_interruptible(&card->wait_stats);
}

/* Spurious interrupt handler.
 * o print a warning
 * If number of spurious interrupts exceeded some limit, then ??? */
static void spur_intr (cycx_t *card, TX25Cmd *cmd)
{
	printk(KERN_INFO "%s: spurious interrupt (0x%X)!\n",
			 card->devname, cmd->command);
}
#ifdef CYCLOMX_X25_DEBUG
static void hex_dump(char *msg, unsigned char *p, int len)
{
	unsigned char hex[1024],
	    	* phex = hex;

	if (len >= (sizeof(hex) / 2))
		len = (sizeof(hex) / 2) - 1;

	while (len--) {
		sprintf(phex, "%02x", *p++);
		phex += 2;
	}

	printk(KERN_INFO "%s: %s\n", msg, hex);
}
#endif

/* Cyclom 2X Firmware-Specific Functions */
/* Exec X.25 command. */
static int x25_exec (cycx_t *card, int command, int link,
		     void *d1, int len1, void *d2, int len2)
{
	TX25Cmd c;
	unsigned long flags;
	u32 addr = 0x1200 + 0x2E0 * link + 0x1E2;
	u8 retry = MAX_CMD_RETRY;
	int err = 0;

	c.command = command;
	c.link = link;
	c.len = len1 + len2;

	spin_lock_irqsave(&card->u.x.lock, flags);

	/* write command */
	cycx_poke(&card->hw, X25_MBOX_OFFS, &c, sizeof(c) - sizeof(c.buf));

	/* write X.25 data */
	if (d1) {
		cycx_poke(&card->hw, addr, d1, len1);

		if (d2) {
			if (len2 > 254) {
				u32 addr1 = 0xA00 + 0x400 * link;

				cycx_poke(&card->hw, addr + len1, d2, 249);
				cycx_poke(&card->hw, addr1, ((u8*)d2) + 249,
					  len2 - 249);
			} else
				cycx_poke(&card->hw, addr + len1, d2, len2);
		}
	}

	/* generate interruption, executing command */
	cycx_intr(&card->hw);

	/* wait till card->mbox == 0 */
	do {
		err = cycx_exec(card->mbox);
	} while (retry-- && err);

	spin_unlock_irqrestore(&card->u.x.lock, flags);

	return err;
}

/* Configure adapter. */
static int x25_configure (cycx_t *card, TX25Config *conf)
{
	struct {
		u16 nlinks;
		TX25Config conf[2];
	} x25_cmd_conf;

	memset (&x25_cmd_conf, 0, sizeof(x25_cmd_conf));
	x25_cmd_conf.nlinks = 2;
	x25_cmd_conf.conf[0] = *conf;
	/* FIXME: we need to find a way in the wanrouter framework
		  to configure the second link, for now lets use it
		  with the same config from the first link, fixing
		  the interface type to RS232, the speed in 38400 and
		  the clock to external */
	x25_cmd_conf.conf[1] = *conf;
	x25_cmd_conf.conf[1].link = 1;
	x25_cmd_conf.conf[1].speed = 5; /* 38400 */
	x25_cmd_conf.conf[1].clock = 8;
	x25_cmd_conf.conf[1].flags = 0; /* default = RS232 */

	x25_dump_config(&x25_cmd_conf.conf[0]);
	x25_dump_config(&x25_cmd_conf.conf[1]);

	return x25_exec(card, X25_CONFIG, 0,
			&x25_cmd_conf, sizeof(x25_cmd_conf), NULL, 0);
}

/* Get protocol statistics. */
static int x25_get_stats (cycx_t *card)
{
	/* the firmware expects 20 in the size field!!!
	   thanks to Daniela */
	int err = x25_exec(card, X25_STATISTIC, 0, NULL, 20, NULL, 0);

	if (err)
		return err;

	interruptible_sleep_on(&card->wait_stats);

	if (signal_pending(current))
		return -EINTR;

	card->wandev.stats.rx_packets = card->u.x.stats.n2_rx_frames;
	card->wandev.stats.rx_over_errors = card->u.x.stats.rx_over_errors;
	card->wandev.stats.rx_crc_errors = card->u.x.stats.rx_crc_errors;
	card->wandev.stats.rx_length_errors = 0; /* not available from fw */
	card->wandev.stats.rx_frame_errors = 0; /* not available from fw */
	card->wandev.stats.rx_missed_errors = card->u.x.stats.rx_aborts;
	card->wandev.stats.rx_dropped = 0; /* not available from fw */
	card->wandev.stats.rx_errors = 0; /* not available from fw */
	card->wandev.stats.tx_packets = card->u.x.stats.n2_tx_frames;
	card->wandev.stats.tx_aborted_errors = card->u.x.stats.tx_aborts;
	card->wandev.stats.tx_dropped = 0; /* not available from fw */
	card->wandev.stats.collisions = 0; /* not available from fw */
	card->wandev.stats.tx_errors = 0; /* not available from fw */

	x25_dump_devs(&card->wandev);

	return 0;
}

/* return the number of nibbles */
static int byte_to_nibble(u8 *s, u8 *d, char *nibble)
{
        int i = 0;

        if (*nibble && *s) {
                d[i] |= *s++ - '0';
                *nibble = 0;
                ++i;
        }

        while (*s) {
                d[i] = (*s - '0') << 4;
                if (*(s + 1))
			d[i] |= *(s + 1) - '0';
                else {
                        *nibble = 1;
                        break;
                }
                ++i;
                s += 2;
        }

        return i;
}

static void nibble_to_byte(u8 *s, u8 *d, u8 len, u8 nibble)
{
	if (nibble) {
		*d++ = '0' + (*s++ & 0x0F);
		--len;
	}

	while (len) {
		*d++ = '0' + (*s >> 4);

		if (--len) {
			*d++ = '0' + (*s & 0x0F);
			--len;
		} else break;
		
		++s;
	}

	*d = '\0';
}

/* Place X.25 call. */
static int x25_place_call (cycx_t *card, x25_channel_t *chan)
{
	int err = 0,
	    len;
	char d[64],
	     nibble = 0,
	     mylen = chan->local_addr ? strlen(chan->local_addr) : 0,
	     remotelen = strlen(chan->addr);
	u8 key;

	if (card->u.x.connection_keys == ~0UL) {
		printk(KERN_INFO "%s: too many simultaneous connection "
				 "requests!\n", card->devname);
		return -EAGAIN;
	}

	key = ffz(card->u.x.connection_keys);
	set_bit(key, (void*)&card->u.x.connection_keys);
	++key;
	dprintk(1, KERN_INFO "%s:x25_place_call:key=%d\n", card->devname, key);
	memset(d, 0, sizeof(d));
	d[1] = key; /* user key */
	d[2] = 0x10;
	d[4] = 0x0B;

	len = byte_to_nibble(chan->addr, d + 6, &nibble);

	if (chan->local_addr)
		len += byte_to_nibble(chan->local_addr, d + 6 + len, &nibble);

	if (nibble)
		++len;

	d[5] = mylen << 4 | remotelen;
	d[6 + len + 1] = 0xCC; /* TCP/IP over X.25, thanks to Daniela :) */
	
	if ((err = x25_exec(card, X25_CONNECT_REQUEST, chan->link,
			    &d, 7 + len + 1, NULL, 0)) != 0)
		clear_bit(--key, (void*)&card->u.x.connection_keys);
	else
                chan->lcn = -key;

        return err;
}

/* Place X.25 CONNECT RESPONSE. */
static int x25_connect_response (cycx_t *card, x25_channel_t *chan)
{
	u8 d[8];

	memset(d, 0, sizeof(d));
	d[0] = d[3] = chan->lcn;
	d[2] = 0x10;
	d[4] = 0x0F;
	d[7] = 0xCC; /* TCP/IP over X.25, thanks Daniela */

	return x25_exec(card, X25_CONNECT_RESPONSE, chan->link, &d, 8, NULL, 0);
}

/* Place X.25 DISCONNECT RESPONSE.  */
static int x25_disconnect_response (cycx_t *card, u8 link, u8 lcn)
{
	char d[5];

	memset(d, 0, sizeof(d));
	d[0] = d[3] = lcn;
	d[2] = 0x10;
	d[4] = 0x17;

	return x25_exec(card, X25_DISCONNECT_RESPONSE, link, &d, 5, NULL, 0);
}

/* Clear X.25 call.  */
static int x25_clear_call (cycx_t *card, u8 link, u8 lcn, u8 cause, u8 diagn)
{
	u8 d[7];

	memset(d, 0, sizeof(d));
	d[0] = d[3] = lcn;
	d[2] = 0x10;
	d[4] = 0x13;
	d[5] = cause;
	d[6] = diagn;

	return x25_exec(card, X25_DISCONNECT_REQUEST, link, d, 7, NULL, 0);
}

/* Send X.25 data packet. */
static int x25_send (cycx_t *card, u8 link, u8 lcn, u8 bitm, int len, void *buf)
{
	u8 d[] = "?\xFF\x10??"; 

	d[0] = d[3] = lcn;
	d[4] = bitm;

	return x25_exec(card, X25_DATA_REQUEST, link, &d, 5, buf, len);
}

/* Miscellaneous */
/* Find network device by its channel number.  */
static struct net_device *get_dev_by_lcn (wan_device_t *wandev, s16 lcn)
{
	struct net_device *dev = wandev->dev;
	x25_channel_t *chan;

	while (dev) {
		chan = (x25_channel_t*)dev->priv;

		if (chan->lcn == lcn)
			break;
		dev = chan->slave;
	}	
	return dev;
}

/* Find network device by its remote dte address. */
static struct net_device *get_dev_by_dte_addr (wan_device_t *wandev, char *dte)
{
	struct net_device *dev = wandev->dev;
	x25_channel_t *chan;

	while (dev) {
		chan = (x25_channel_t*)dev->priv;

		if (!strcmp(chan->addr, dte))
			break;
		dev = chan->slave;
	}
	return dev;
}

/* Initiate connection on the logical channel.
 * o for PVC we just get channel configuration
 * o for SVCs place an X.25 call
 *
 * Return:	0	connected
 *		>0	connection in progress
 *		<0	failure */
static int chan_connect (struct net_device *dev)
{
	x25_channel_t *chan = dev->priv;
	cycx_t *card = chan->card;

	if (chan->svc) {
                if (!chan->addr[0])
			return -EINVAL; /* no destination address */

                dprintk(1, KERN_INFO "%s: placing X.25 call to %s...\n",
				  card->devname, chan->addr);

                if (x25_place_call(card, chan))
			return -EIO;

                set_chan_state(dev, WAN_CONNECTING);
                return 1;
        } else 
		set_chan_state(dev, WAN_CONNECTED);

	return 0;
}

/* Disconnect logical channel.
 * o if SVC then clear X.25 call */
static void chan_disconnect (struct net_device *dev)
{
	x25_channel_t *chan = dev->priv;

	if (chan->svc) {
		x25_clear_call(chan->card, chan->link, chan->lcn, 0, 0);
		set_chan_state(dev, WAN_DISCONNECTING);
	} else
		set_chan_state(dev, WAN_DISCONNECTED);
}

/* Called by kernel timer */
static void chan_timer (unsigned long d)
{
	struct net_device *dev = (struct net_device *)d;
	x25_channel_t *chan = dev->priv;
	
	if (chan->state == WAN_CONNECTED)
		chan_disconnect(dev);
	else
		printk(KERN_ERR "%s: chan_timer for svc (%s) not connected!\n",
				chan->card->devname, dev->name);
}

/* Set logical channel state. */
static void set_chan_state (struct net_device *dev, u8 state)
{
	x25_channel_t *chan = dev->priv;
	cycx_t *card = chan->card;
	long flags;
	char *string_state = NULL;

	spin_lock_irqsave(&card->lock, flags);

	if (chan->state != state) {
		if (chan->svc && chan->state == WAN_CONNECTED)
			del_timer(&chan->timer);
	
		switch (state) {
			case WAN_CONNECTED:
				string_state = "connected!";
				*(u16*)dev->dev_addr = htons(chan->lcn);
				netif_wake_queue(dev);
				reset_timer(dev);

				if (chan->protocol == ETH_P_X25)
					chan_x25_send_event(dev, 1);

				break;

			case WAN_CONNECTING:
				string_state = "connecting...";
				break;

			case WAN_DISCONNECTING:
				string_state = "disconnecting...";
				break;

			case WAN_DISCONNECTED:
				string_state = "disconnected!";
				
				if (chan->svc) {
					*(unsigned short*)dev->dev_addr = 0;
					chan->lcn = 0;
                                }

				if (chan->protocol == ETH_P_X25)
					chan_x25_send_event(dev, 2);

				netif_wake_queue(dev);
				break;
		}

		printk (KERN_INFO "%s: interface %s %s\n", card->devname,
				  dev->name, string_state);
		chan->state = state;
	}

	spin_unlock_irqrestore(&card->lock, flags);
}

/* Send packet on a logical channel.
 *	When this function is called, tx_skb field of the channel data space
 *	points to the transmit socket buffer.  When transmission is complete,
 *	release socket buffer and reset 'tbusy' flag.
 *
 * Return:	0	- transmission complete
 *		1	- busy
 *
 * Notes:
 * 1. If packet length is greater than MTU for this channel, we'll fragment
 *    the packet into 'complete sequence' using M-bit.
 * 2. When transmission is complete, an event notification should be issued
 *    to the router.  */
static int chan_send (struct net_device *dev, struct sk_buff *skb)
{
	x25_channel_t *chan = dev->priv;
	cycx_t *card = chan->card;
	int bitm = 0;		/* final packet */
	unsigned len = skb->len;

	if (skb->len > card->wandev.mtu) {
		len = card->wandev.mtu;
		bitm = 0x10;		/* set M-bit (more data) */
	}

	if (x25_send(card, chan->link, chan->lcn, bitm, len, skb->data))
		return 1;
		
	if (bitm) {
		skb_pull(skb, len);
		return 1;
	}

	++chan->ifstats.tx_packets;
	chan->ifstats.tx_bytes += len;

	return 0;
}

/* Send event (connection, disconnection, etc) to X.25 socket layer */

static void chan_x25_send_event(struct net_device *dev, u8 event)
{
        struct sk_buff *skb;
        unsigned char *ptr;

        if ((skb = dev_alloc_skb(1)) == NULL) {
                printk(KERN_ERR "%s: out of memory\n", __FUNCTION__);
                return;
        }

        ptr  = skb_put(skb, 1);
        *ptr = event;

        skb->dev = dev;
        skb->protocol = htons(ETH_P_X25);
        skb->mac.raw = skb->data;
        skb->pkt_type = PACKET_HOST;

        netif_rx(skb);
	dev->last_rx = jiffies;		/* timestamp */
}

/* Convert line speed in bps to a number used by cyclom 2x code. */
static u8 bps_to_speed_code (u32 bps)
{
	u8 number = 0; /* defaults to the lowest (1200) speed ;> */

	     if (bps >= 512000) number = 8;
	else if (bps >= 256000) number = 7;
	else if (bps >= 64000)  number = 6;
	else if (bps >= 38400)  number = 5;
	else if (bps >= 19200)  number = 4;
	else if (bps >= 9600)   number = 3;
	else if (bps >= 4800)   number = 2;
	else if (bps >= 2400)   number = 1;

	return number;
}

/* log base 2 */
static u8 log2 (u32 n)
{
        u8 log = 0;

        if (!n)
		return 0;

        while (n > 1) {
                n >>= 1;
                ++log;
        }

        return log;
}

/* Convert decimal string to unsigned integer.
 * If len != 0 then only 'len' characters of the string are converted. */
static unsigned dec_to_uint (u8 *str, int len)
{
	unsigned val = 0;

	if (!len)
		len = strlen(str);

	for (; len && is_digit(*str); ++str, --len)
		val = (val * 10) + (*str - (unsigned) '0');

	return val;
}

static void reset_timer(struct net_device *dev)
{
	x25_channel_t *chan = dev->priv;

	if (chan->svc)
		mod_timer(&chan->timer, jiffies+chan->idle_tmout*HZ);
}
#ifdef CYCLOMX_X25_DEBUG
static void x25_dump_config(TX25Config *conf)
{
	printk(KERN_INFO "X.25 configuration\n");
	printk(KERN_INFO "-----------------\n");
	printk(KERN_INFO "link number=%d\n", conf->link);
	printk(KERN_INFO "line speed=%d\n", conf->speed);
	printk(KERN_INFO "clock=%sternal\n", conf->clock == 8 ? "Ex" : "In");
	printk(KERN_INFO "# level 2 retransm.=%d\n", conf->n2);
	printk(KERN_INFO "level 2 window=%d\n", conf->n2win);
	printk(KERN_INFO "level 3 window=%d\n", conf->n3win);
	printk(KERN_INFO "# logical channels=%d\n", conf->nvc);
	printk(KERN_INFO "level 3 pkt len=%d\n", conf->pktlen);
	printk(KERN_INFO "my address=%d\n", conf->locaddr);
	printk(KERN_INFO "remote address=%d\n", conf->remaddr);
	printk(KERN_INFO "t1=%d seconds\n", conf->t1);
	printk(KERN_INFO "t2=%d seconds\n", conf->t2);
	printk(KERN_INFO "t21=%d seconds\n", conf->t21);
	printk(KERN_INFO "# PVCs=%d\n", conf->npvc);
	printk(KERN_INFO "t23=%d seconds\n", conf->t23);
	printk(KERN_INFO "flags=0x%x\n", conf->flags);
}

static void x25_dump_stats(TX25Stats *stats)
{
	printk(KERN_INFO "X.25 statistics\n");
	printk(KERN_INFO "--------------\n");
	printk(KERN_INFO "rx_crc_errors=%d\n", stats->rx_crc_errors);
	printk(KERN_INFO "rx_over_errors=%d\n", stats->rx_over_errors);
	printk(KERN_INFO "n2_tx_frames=%d\n", stats->n2_tx_frames);
	printk(KERN_INFO "n2_rx_frames=%d\n", stats->n2_rx_frames);
	printk(KERN_INFO "tx_timeouts=%d\n", stats->tx_timeouts);
	printk(KERN_INFO "rx_timeouts=%d\n", stats->rx_timeouts);
	printk(KERN_INFO "n3_tx_packets=%d\n", stats->n3_tx_packets);
	printk(KERN_INFO "n3_rx_packets=%d\n", stats->n3_rx_packets);
	printk(KERN_INFO "tx_aborts=%d\n", stats->tx_aborts);
	printk(KERN_INFO "rx_aborts=%d\n", stats->rx_aborts);
}

static void x25_dump_devs(wan_device_t *wandev)
{
	struct net_device *dev = wandev->dev;

	printk(KERN_INFO "X.25 dev states\n");
	printk(KERN_INFO "name: addr:           txoff:  protocol:\n");
	printk(KERN_INFO "---------------------------------------\n");

	while(dev) {
		x25_channel_t *chan = dev->priv;

		printk(KERN_INFO "%-5.5s %-15.15s   %d     ETH_P_%s\n",
				 chan->name, chan->addr, netif_queue_stopped(dev),
				 chan->protocol == ETH_P_IP ? "IP" : "X25");
		dev = chan->slave;
	}
}

#endif /* CYCLOMX_X25_DEBUG */
/* End */
