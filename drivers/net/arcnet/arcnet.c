/*
 * Linux ARCnet driver - device-independent routines
 * 
 * Written 1997 by David Woodhouse.
 * Written 1994-1999 by Avery Pennarun.
 * Written 1999-2000 by Martin Mares <mj@ucw.cz>.
 * Derived from skeleton.c by Donald Becker.
 *
 * Special thanks to Contemporary Controls, Inc. (www.ccontrols.com)
 *  for sponsoring the further development of this driver.
 *
 * **********************
 *
 * The original copyright was as follows:
 *
 * skeleton.c Written 1993 by Donald Becker.
 * Copyright 1993 United States Government as represented by the
 * Director, National Security Agency.  This software may only be used
 * and distributed according to the terms of the GNU General Public License as
 * modified by SRC, incorporated herein by reference.
 *
 * **********************
 * 
 * The change log is now in a file called ChangeLog in this directory.
 *
 * Sources:
 *  - Crynwr arcnet.com/arcether.com packet drivers.
 *  - arcnet.c v0.00 dated 1/1/94 and apparently by 
 *     Donald Becker - it didn't work :)
 *  - skeleton.c v0.05 dated 11/16/93 by Donald Becker
 *     (from Linux Kernel 1.1.45)
 *  - RFC's 1201 and 1051 - re: TCP/IP over ARCnet
 *  - The official ARCnet COM9026 data sheets (!) thanks to
 *     Ken Cornetet <kcornete@nyx10.cs.du.edu>
 *  - The official ARCnet COM20020 data sheets.
 *  - Information on some more obscure ARCnet controller chips, thanks
 *     to the nice people at SMSC.
 *  - net/inet/eth.c (from kernel 1.1.50) for header-building info.
 *  - Alternate Linux ARCnet source by V.Shergin <vsher@sao.stavropol.su>
 *  - Textual information and more alternate source from Joachim Koenig
 *     <jojo@repas.de>
 */

#define VERSION "arcnet: v3.93 BETA 2000/04/29 - by Avery Pennarun et al.\n"

#include <linux/module.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <net/arp.h>
#include <linux/init.h>
#include <linux/arcdevice.h>


/* "do nothing" functions for protocol drivers */
static void null_rx(struct net_device *dev, int bufnum,
		    struct archdr *pkthdr, int length);
static int null_build_header(struct sk_buff *skb, struct net_device *dev,
			     unsigned short type, uint8_t daddr);
static int null_prepare_tx(struct net_device *dev, struct archdr *pkt,
			   int length, int bufnum);


/*
 * one ArcProto per possible proto ID.  None of the elements of
 * arc_proto_map are allowed to be NULL; they will get set to
 * arc_proto_default instead.  It also must not be NULL; if you would like
 * to set it to NULL, set it to &arc_proto_null instead.
 */
struct ArcProto *arc_proto_map[256], *arc_proto_default, *arc_bcast_proto;

struct ArcProto arc_proto_null =
{
	'?',
	XMTU,
	null_rx,
	null_build_header,
	null_prepare_tx
};


/* Exported function prototypes */
int arcnet_debug = ARCNET_DEBUG;

EXPORT_SYMBOL(arc_proto_map);
EXPORT_SYMBOL(arc_proto_default);
EXPORT_SYMBOL(arc_bcast_proto);
EXPORT_SYMBOL(arc_proto_null);
EXPORT_SYMBOL(arcnet_unregister_proto);
EXPORT_SYMBOL(arcnet_debug);
EXPORT_SYMBOL(arcdev_setup);
EXPORT_SYMBOL(arcnet_interrupt);

/* Internal function prototypes */
static int arcnet_open(struct net_device *dev);
static int arcnet_close(struct net_device *dev);
static int arcnet_send_packet(struct sk_buff *skb, struct net_device *dev);
static void arcnet_timeout(struct net_device *dev);
static int arcnet_header(struct sk_buff *skb, struct net_device *dev,
			 unsigned short type, void *daddr, void *saddr,
			 unsigned len);
static int arcnet_rebuild_header(struct sk_buff *skb);
static struct net_device_stats *arcnet_get_stats(struct net_device *dev);
static int go_tx(struct net_device *dev);

void __init arcnet_init(void)
{
	static int arcnet_inited;
	int count;

	if (arcnet_inited++)
		return;

	printk(VERSION);

#ifdef ALPHA_WARNING
	BUGLVL(D_EXTRA) {
		printk("arcnet: ***\n"
		"arcnet: * Read arcnet.txt for important release notes!\n"
		       "arcnet: *\n"
		       "arcnet: * This is an ALPHA version! (Last stable release: v3.02)  E-mail\n"
		       "arcnet: * me if you have any questions, comments, or bug reports.\n"
		       "arcnet: ***\n");
	}
#endif

	/* initialize the protocol map */
	arc_proto_default = arc_bcast_proto = &arc_proto_null;
	for (count = 0; count < 256; count++)
		arc_proto_map[count] = arc_proto_default;

	BUGLVL(D_DURING)
	    printk("arcnet: struct sizes: %d %d %d %d %d\n",
		 sizeof(struct arc_hardware), sizeof(struct arc_rfc1201),
		sizeof(struct arc_rfc1051), sizeof(struct arc_eth_encap),
		   sizeof(struct archdr));

#ifdef CONFIG_ARCNET		/* We're not built as a module */
	printk("arcnet: Available protocols:");
#ifdef CONFIG_ARCNET_1201
	printk(" RFC1201");
	arcnet_rfc1201_init();
#endif
#ifdef CONFIG_ARCNET_1051
	printk(" RFC1051");
	arcnet_rfc1051_init();
#endif
#ifdef CONFIG_ARCNET_RAW
	printk(" RAW");
	arcnet_raw_init();
#endif
	printk("\n");
#ifdef CONFIG_ARCNET_COM90xx
	com90xx_probe(NULL);
#endif
#endif
}


#ifdef MODULE

static int debug = ARCNET_DEBUG;
MODULE_PARM(debug, "i");
MODULE_LICENSE("GPL");

int __init init_module(void)
{
	arcnet_debug = debug;
	arcnet_init();
	return 0;
}

void cleanup_module(void)
{
}

#endif


/*
 * Dump the contents of an sk_buff
 */
#if ARCNET_DEBUG_MAX & D_SKB
void arcnet_dump_skb(struct net_device *dev, struct sk_buff *skb, char *desc)
{
	int i;
	unsigned long flags;

	save_flags(flags);
	cli();
	printk(KERN_DEBUG "%6s: skb dump (%s) follows:", dev->name, desc);
	for (i = 0; i < skb->len; i++) {
		if (i % 16 == 0)
			printk("\n" KERN_DEBUG "[%04X] ", i);
		printk("%02X ", ((u_char *) skb->data)[i]);
	}
	printk("\n");
	restore_flags(flags);
}

EXPORT_SYMBOL(arcnet_dump_skb);
#endif


/*
 * Dump the contents of an ARCnet buffer
 */
#if (ARCNET_DEBUG_MAX & (D_RX | D_TX))
void arcnet_dump_packet(struct net_device *dev, int bufnum, char *desc)
{
	struct arcnet_local *lp = (struct arcnet_local *) dev->priv;
	int i, length;
	unsigned long flags;
	static uint8_t buf[512];

	save_flags(flags);
	cli();

	lp->hw.copy_from_card(dev, bufnum, 0, buf, 512);

	/* if the offset[0] byte is nonzero, this is a 256-byte packet */
	length = (buf[2] ? 256 : 512);

	printk(KERN_DEBUG "%6s: packet dump (%s) follows:", dev->name, desc);
	for (i = 0; i < length; i++) {
		if (i % 16 == 0)
			printk("\n" KERN_DEBUG "[%04X] ", i);
		printk("%02X ", buf[i]);
	}
	printk("\n");

	restore_flags(flags);
}

EXPORT_SYMBOL(arcnet_dump_packet);
#endif


/*
 * Unregister a protocol driver from the arc_proto_map.  Protocol drivers
 * are responsible for registering themselves, but the unregister routine
 * is pretty generic so we'll do it here.
 */
void arcnet_unregister_proto(struct ArcProto *proto)
{
	int count;

	if (arc_proto_default == proto)
		arc_proto_default = &arc_proto_null;
	if (arc_bcast_proto == proto)
		arc_bcast_proto = arc_proto_default;

	for (count = 0; count < 256; count++) {
		if (arc_proto_map[count] == proto)
			arc_proto_map[count] = arc_proto_default;
	}
}


/*
 * Add a buffer to the queue.  Only the interrupt handler is allowed to do
 * this, unless interrupts are disabled.
 * 
 * Note: we don't check for a full queue, since there aren't enough buffers
 * to more than fill it.
 */
static void release_arcbuf(struct net_device *dev, int bufnum)
{
	struct arcnet_local *lp = (struct arcnet_local *) dev->priv;
	int i;

	lp->buf_queue[lp->first_free_buf++] = bufnum;
	lp->first_free_buf %= 5;

	BUGLVL(D_DURING) {
		BUGMSG(D_DURING, "release_arcbuf: freed #%d; buffer queue is now: ",
		       bufnum);
		for (i = lp->next_buf; i != lp->first_free_buf; i = ++i % 5)
			BUGMSG2(D_DURING, "#%d ", lp->buf_queue[i]);
		BUGMSG2(D_DURING, "\n");
	}
}


/*
 * Get a buffer from the queue.  If this returns -1, there are no buffers
 * available.
 */
static int get_arcbuf(struct net_device *dev)
{
	struct arcnet_local *lp = (struct arcnet_local *) dev->priv;
	int buf = -1, i;

	if (!atomic_dec_and_test(&lp->buf_lock))	/* already in this function */
		BUGMSG(D_NORMAL, "get_arcbuf: overlap (%d)!\n", lp->buf_lock.counter);
	else {			/* we can continue */
		if (lp->next_buf >= 5)
			lp->next_buf -= 5;

		if (lp->next_buf == lp->first_free_buf)
			BUGMSG(D_NORMAL, "get_arcbuf: BUG: no buffers are available??\n");
		else {
			buf = lp->buf_queue[lp->next_buf++];
			lp->next_buf %= 5;
		}
	}


	BUGLVL(D_DURING) {
		BUGMSG(D_DURING, "get_arcbuf: got #%d; buffer queue is now: ", buf);
		for (i = lp->next_buf; i != lp->first_free_buf; i = ++i % 5)
			BUGMSG2(D_DURING, "#%d ", lp->buf_queue[i]);
		BUGMSG2(D_DURING, "\n");
	}

	atomic_inc(&lp->buf_lock);
	return buf;
}


static int choose_mtu(void)
{
	int count, mtu = 65535;

	/* choose the smallest MTU of all available encaps */
	for (count = 0; count < 256; count++) {
		if (arc_proto_map[count] != &arc_proto_null
		    && arc_proto_map[count]->mtu < mtu) {
			mtu = arc_proto_map[count]->mtu;
		}
	}

	return mtu == 65535 ? XMTU : mtu;
}


/* Setup a struct device for ARCnet. */
void arcdev_setup(struct net_device *dev)
{
	dev->type = ARPHRD_ARCNET;
	dev->hard_header_len = sizeof(struct archdr);
	dev->mtu = choose_mtu();

	dev->addr_len = ARCNET_ALEN;
	dev->tx_queue_len = 30;
	dev->broadcast[0] = 0x00;	/* for us, broadcasts are address 0 */
	dev->watchdog_timeo = TX_TIMEOUT;

	/* New-style flags. */
	dev->flags = IFF_BROADCAST;

	/*
	 * Put in this stuff here, so we don't have to export the symbols to
	 * the chipset drivers.
	 */
	dev->open = arcnet_open;
	dev->stop = arcnet_close;
	dev->hard_start_xmit = arcnet_send_packet;
	dev->tx_timeout = arcnet_timeout;
	dev->get_stats = arcnet_get_stats;
	dev->hard_header = arcnet_header;
	dev->rebuild_header = arcnet_rebuild_header;
}


/*
 * Open/initialize the board.  This is called sometime after booting when
 * the 'ifconfig' program is run.
 *
 * This routine should set everything up anew at each open, even registers
 * that "should" only need to be set once at boot, so that there is
 * non-reboot way to recover if something goes wrong.
 */
static int arcnet_open(struct net_device *dev)
{
	struct arcnet_local *lp = (struct arcnet_local *) dev->priv;
	int count, newmtu;

	BUGLVL(D_PROTO) {
		int count;
		BUGMSG(D_PROTO, "protocol map (default is '%c'): ",
		       arc_proto_default->suffix);
		for (count = 0; count < 256; count++)
			BUGMSG2(D_PROTO, "%c", arc_proto_map[count]->suffix);
		BUGMSG2(D_PROTO, "\n");
	}


	BUGMSG(D_INIT, "arcnet_open: resetting card.\n");

	/* try to put the card in a defined state - if it fails the first
	 * time, actually reset it.
	 */
	if (ARCRESET(0) && ARCRESET(1))
		return -ENODEV;

	newmtu = choose_mtu();
	if (newmtu < dev->mtu)
		dev->mtu = newmtu;

	/* autodetect the encapsulation for each host. */
	memset(lp->default_proto, 0, sizeof(lp->default_proto));

	/* the broadcast address is special - use the 'bcast' protocol */
	for (count = 0; count < 256; count++) {
		if (arc_proto_map[count] == arc_bcast_proto) {
			lp->default_proto[0] = count;
			break;
		}
	}

	/* initialize buffers */
	atomic_set(&lp->buf_lock, 1);
	lp->next_buf = lp->first_free_buf = 0;
	release_arcbuf(dev, 0);
	release_arcbuf(dev, 1);
	release_arcbuf(dev, 2);
	release_arcbuf(dev, 3);
	lp->cur_tx = lp->next_tx = -1;
	lp->cur_rx = -1;

	lp->rfc1201.sequence = 1;

	/* bring up the hardware driver */
	ARCOPEN(1);

	if (dev->dev_addr[0] == 0)
		BUGMSG(D_NORMAL, "WARNING!  Station address 00 is reserved "
		       "for broadcasts!\n");
	else if (dev->dev_addr[0] == 255)
		BUGMSG(D_NORMAL, "WARNING!  Station address FF may confuse "
		       "DOS networking programs!\n");

	if (ASTATUS() & RESETflag)
		ACOMMAND(CFLAGScmd | RESETclear);

	/* make sure we're ready to receive IRQ's. */
	AINTMASK(0);
	udelay(1);		/* give it time to set the mask before
				 * we reset it again. (may not even be
				 * necessary)
				 */
	lp->intmask = NORXflag | RECONflag;
	AINTMASK(lp->intmask);

	netif_start_queue(dev);

	return 0;
}


/* The inverse routine to arcnet_open - shuts down the card. */
static int arcnet_close(struct net_device *dev)
{
	struct arcnet_local *lp = (struct arcnet_local *) dev->priv;

	netif_stop_queue(dev);

	/* flush TX and disable RX */
	AINTMASK(0);
	ACOMMAND(NOTXcmd);	/* stop transmit */
	ACOMMAND(NORXcmd);	/* disable receive */
	mdelay(1);

	/* shut down the card */
	ARCOPEN(0);

	return 0;
}


static int arcnet_header(struct sk_buff *skb, struct net_device *dev,
			 unsigned short type, void *daddr, void *saddr,
			 unsigned len)
{
	struct arcnet_local *lp = (struct arcnet_local *) dev->priv;
	uint8_t _daddr, proto_num;
	struct ArcProto *proto;

	BUGMSG(D_DURING,
	    "create header from %d to %d; protocol %d (%Xh); size %u.\n",
	       saddr ? *(uint8_t *) saddr : -1,
	       daddr ? *(uint8_t *) daddr : -1,
	       type, type, len);

	if (len != skb->len)
		BUGMSG(D_NORMAL, "arcnet_header: Yikes!  skb->len(%d) != len(%d)!\n",
		       skb->len, len);

	/*
	 * if the dest addr isn't provided, we can't choose an encapsulation!
	 * Store the packet type (eg. ETH_P_IP) for now, and we'll push on a
	 * real header when we do rebuild_header. 
	 */
	if (!daddr) {
		*(uint16_t *) skb_push(skb, 2) = type;
		if (skb->nh.raw - skb->mac.raw != 2)
			BUGMSG(D_NORMAL, "arcnet_header: Yikes!  diff (%d) is not 2!\n",
			       skb->nh.raw - skb->mac.raw);
		return -2;	/* return error -- can't transmit yet! */
	}
	/* otherwise, we can just add the header as usual. */
	_daddr = *(uint8_t *) daddr;
	proto_num = lp->default_proto[_daddr];
	proto = arc_proto_map[proto_num];
	BUGMSG(D_DURING, "building header for %02Xh using protocol '%c'\n",
	       proto_num, proto->suffix);
	if (proto == &arc_proto_null && arc_bcast_proto != proto) {
		BUGMSG(D_DURING, "actually, let's use '%c' instead.\n",
		       arc_bcast_proto->suffix);
		proto = arc_bcast_proto;
	}
	return proto->build_header(skb, dev, type, _daddr);
}


/* 
 * Rebuild the ARCnet hard header. This is called after an ARP (or in the
 * future other address resolution) has completed on this sk_buff. We now
 * let ARP fill in the destination field.
 */
static int arcnet_rebuild_header(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;
	struct arcnet_local *lp = (struct arcnet_local *) dev->priv;
	int status = 0;		/* default is failure */
	unsigned short type;
	uint8_t daddr=0;
	struct ArcProto *proto;

	if (skb->nh.raw - skb->mac.raw != 2) {
		BUGMSG(D_NORMAL,
		     "rebuild_header: shouldn't be here! (hdrsize=%d)\n",
		       skb->nh.raw - skb->mac.raw);
		return 0;
	}
	type = *(uint16_t *) skb_pull(skb, 2);

	if (type == ETH_P_IP) {
#ifdef CONFIG_INET
		BUGMSG(D_DURING, "rebuild header for ethernet protocol %Xh\n", type);
		status = arp_find(&daddr, skb) ? 1 : 0;
		BUGMSG(D_DURING, " rebuilt: dest is %d; protocol %Xh\n",
		       daddr, type);
#endif
	} else {
		BUGMSG(D_NORMAL,
		       "I don't understand ethernet protocol %Xh addresses!\n", type);
		lp->stats.tx_errors++;
		lp->stats.tx_aborted_errors++;
	}

	/* if we couldn't resolve the address... give up. */
	if (!status)
		return 0;

	/* add the _real_ header this time! */
	proto = arc_proto_map[lp->default_proto[daddr]];
	proto->build_header(skb, dev, type, daddr);

	return 1;		/* success */
}



/* Called by the kernel in order to transmit a packet. */
static int arcnet_send_packet(struct sk_buff *skb, struct net_device *dev)
{
	struct arcnet_local *lp = (struct arcnet_local *) dev->priv;
	struct archdr *pkt;
	struct arc_rfc1201 *soft;
	struct ArcProto *proto;
	int txbuf;

	BUGMSG(D_DURING,
	       "transmit requested (status=%Xh, txbufs=%d/%d, len=%d)\n",
	       ASTATUS(), lp->cur_tx, lp->next_tx, skb->len);

	pkt = (struct archdr *) skb->data;
	soft = &pkt->soft.rfc1201;
	proto = arc_proto_map[soft->proto];

	BUGMSG(D_SKB_SIZE, "skb: transmitting %d bytes to %02X\n",
		skb->len, pkt->hard.dest);
	BUGLVL(D_SKB) arcnet_dump_skb(dev, skb, "tx");

	/* fits in one packet? */
	if (skb->len - ARC_HDR_SIZE > XMTU && !proto->continue_tx) {
		BUGMSG(D_NORMAL, "fixme: packet too large: compensating badly!\n");
		dev_kfree_skb(skb);
		return 0;	/* don't try again */
	}

	/* We're busy transmitting a packet... */
	netif_stop_queue(dev);

	AINTMASK(0);

	txbuf = get_arcbuf(dev);
	if (txbuf != -1) {
		if (proto->prepare_tx(dev, pkt, skb->len, txbuf)) {
			/* done right away */
			lp->stats.tx_bytes += skb->len;
			dev_kfree_skb(skb);
		} else {
			/* do it the 'split' way */
			lp->outgoing.proto = proto;
			lp->outgoing.skb = skb;
			lp->outgoing.pkt = pkt;

			if (!proto->continue_tx)
				BUGMSG(D_NORMAL, "bug! prep_tx==0, but no continue_tx!\n");
			else if (proto->continue_tx(dev, txbuf)) {
				BUGMSG(D_NORMAL,
				       "bug! continue_tx finished the first time! "
				       "(proto='%c')\n", proto->suffix);
			}
		}

		lp->next_tx = txbuf;
	} else
		dev_kfree_skb(skb);

	/* make sure we didn't ignore a TX IRQ while we were in here */
	AINTMASK(0);
	lp->intmask |= TXFREEflag;
	AINTMASK(lp->intmask);

	return 0;		/* no need to try again */
}


/*
 * Actually start transmitting a packet that was loaded into a buffer
 * by prepare_tx.  This should _only_ be called by the interrupt handler.
 */
static int go_tx(struct net_device *dev)
{
	struct arcnet_local *lp = (struct arcnet_local *) dev->priv;

	BUGMSG(D_DURING, "go_tx: status=%Xh, intmask=%Xh, next_tx=%d, cur_tx=%d\n",
	       ASTATUS(), lp->intmask, lp->next_tx, lp->cur_tx);

	if (lp->cur_tx != -1 || lp->next_tx == -1)
		return 0;

	BUGLVL(D_TX) arcnet_dump_packet(dev, lp->next_tx, "go_tx");

	lp->cur_tx = lp->next_tx;
	lp->next_tx = -1;

	/* start sending */
	ACOMMAND(TXcmd | (lp->cur_tx << 3));

	dev->trans_start = jiffies;
	lp->stats.tx_packets++;
	lp->lasttrans_dest = lp->lastload_dest;
	lp->lastload_dest = 0;
	lp->intmask |= TXFREEflag;

	return 1;
}


/* Called by the kernel when transmit times out */
static void arcnet_timeout(struct net_device *dev)
{
	unsigned long flags;
	struct arcnet_local *lp = (struct arcnet_local *) dev->priv;
	int status = ASTATUS();
	char *msg;

	save_flags(flags);
	cli();

	if (status & TXFREEflag) {	/* transmit _DID_ finish */
		msg = " - missed IRQ?";
	} else {
		msg = "";
		lp->stats.tx_aborted_errors++;
		lp->timed_out = 1;
		ACOMMAND(NOTXcmd | (lp->cur_tx << 3));
	}
	lp->stats.tx_errors++;

	/* make sure we didn't miss a TX IRQ */
	AINTMASK(0);
	lp->intmask |= TXFREEflag;
	AINTMASK(lp->intmask);

	restore_flags(flags);

	if (jiffies - lp->last_timeout > 10*HZ) {
		BUGMSG(D_EXTRA, "tx timed out%s (status=%Xh, intmask=%Xh, dest=%02Xh)\n",
		       msg, status, lp->intmask, lp->lasttrans_dest);
		lp->last_timeout = jiffies;
	}

	if (lp->cur_tx == -1)
		netif_wake_queue(dev);
}


/*
 * The typical workload of the driver: Handle the network interface
 * interrupts. Establish which device needs attention, and call the correct
 * chipset interrupt handler.
 */
void arcnet_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = dev_id;
	struct arcnet_local *lp;
	int recbuf, status, didsomething, boguscount;

	BUGMSG(D_DURING, "\n");

	if (dev == NULL) {
		BUGMSG(D_DURING, "arcnet: irq %d for unknown device.\n", irq);
		return;
	}
	BUGMSG(D_DURING, "in arcnet_interrupt\n");

	lp = (struct arcnet_local *) dev->priv;
	if (!lp) {
		BUGMSG(D_DURING, "arcnet: irq ignored due to missing lp.\n");
		return;
	}
	/*
	 * RESET flag was enabled - if device is not running, we must clear it right
	 * away (but nothing else).
	 */
	if (!netif_running(dev)) {
		if (ASTATUS() & RESETflag)
			ACOMMAND(CFLAGScmd | RESETclear);
		AINTMASK(0);
		return;
	}

	BUGMSG(D_DURING, "in arcnet_inthandler (status=%Xh, intmask=%Xh)\n",
	       ASTATUS(), lp->intmask);

	boguscount = 5;
	do {
		status = ASTATUS();
		didsomething = 0;

		/*
		 * RESET flag was enabled - card is resetting and if RX is
		 * disabled, it's NOT because we just got a packet.
		 * 
		 * The card is in an undefined state.  Clear it out and start over.
		 */
		if (status & RESETflag) {
			BUGMSG(D_NORMAL, "spurious reset (status=%Xh)\n", status);
			arcnet_close(dev);
			arcnet_open(dev);

			/* get out of the interrupt handler! */
			break;
		}
		/* 
		 * RX is inhibited - we must have received something. Prepare to
		 * receive into the next buffer.
		 * 
		 * We don't actually copy the received packet from the card until
		 * after the transmit handler runs (and possibly launches the next
		 * tx); this should improve latency slightly if we get both types
		 * of interrupts at once. 
		 */
		recbuf = -1;
		if (status & lp->intmask & NORXflag) {
			recbuf = lp->cur_rx;
			BUGMSG(D_DURING, "Buffer #%d: receive irq (status=%Xh)\n",
			       recbuf, status);

			lp->cur_rx = get_arcbuf(dev);
			if (lp->cur_rx != -1) {
				BUGMSG(D_DURING, "enabling receive to buffer #%d\n",
				       lp->cur_rx);
				ACOMMAND(RXcmd | (lp->cur_rx << 3) | RXbcasts);
			}
			didsomething++;
		}
		/* a transmit finished, and we're interested in it. */
		if ((status & lp->intmask & TXFREEflag) || lp->timed_out) {
			lp->intmask &= ~TXFREEflag;

			BUGMSG(D_DURING, "TX IRQ (stat=%Xh)\n", status);

			if (lp->cur_tx != -1 && !(status & TXACKflag) && !lp->timed_out) {
				if (lp->lasttrans_dest != 0) {
					BUGMSG(D_EXTRA, "transmit was not acknowledged! "
					    "(status=%Xh, dest=%02Xh)\n",
					     status, lp->lasttrans_dest);
					lp->stats.tx_errors++;
					lp->stats.tx_carrier_errors++;
				} else {
					BUGMSG(D_DURING,
					       "broadcast was not acknowledged; that's normal "
					    "(status=%Xh, dest=%02Xh)\n",
					     status, lp->lasttrans_dest);
				}
			}
			if (lp->cur_tx != -1)
				release_arcbuf(dev, lp->cur_tx);

			lp->cur_tx = -1;
			lp->timed_out = 0;
			didsomething++;

			/* send another packet if there is one */
			go_tx(dev);

			/* continue a split packet, if any */
			if (lp->outgoing.proto && lp->outgoing.proto->continue_tx) {
				int txbuf = get_arcbuf(dev);
				if (txbuf != -1) {
					if (lp->outgoing.proto->continue_tx(dev, txbuf)) {
						/* that was the last segment */
						lp->stats.tx_bytes += lp->outgoing.skb->len;
						dev_kfree_skb_irq(lp->outgoing.skb);
						lp->outgoing.proto = NULL;
					}
					lp->next_tx = txbuf;
				}
			}
			/* inform upper layers of idleness, if necessary */
			if (lp->cur_tx == -1)
				netif_wake_queue(dev);
		}
		/* now process the received packet, if any */
		if (recbuf != -1) {
			BUGLVL(D_RX) arcnet_dump_packet(dev, recbuf, "rx irq");

			arcnet_rx(dev, recbuf);
			release_arcbuf(dev, recbuf);

			didsomething++;
		}
		if (status & lp->intmask & RECONflag) {
			ACOMMAND(CFLAGScmd | CONFIGclear);
			lp->stats.tx_carrier_errors++;

			BUGMSG(D_RECON, "Network reconfiguration detected (status=%Xh)\n",
			       status);

			/* is the RECON info empty or old? */
			if (!lp->first_recon || !lp->last_recon ||
			    jiffies - lp->last_recon > HZ * 10) {
				if (lp->network_down)
					BUGMSG(D_NORMAL, "reconfiguration detected: cabling restored?\n");
				lp->first_recon = lp->last_recon = jiffies;
				lp->num_recons = lp->network_down = 0;

				BUGMSG(D_DURING, "recon: clearing counters.\n");
			} else {	/* add to current RECON counter */
				lp->last_recon = jiffies;
				lp->num_recons++;

				BUGMSG(D_DURING, "recon: counter=%d, time=%lds, net=%d\n",
				       lp->num_recons,
				 (lp->last_recon - lp->first_recon) / HZ,
				       lp->network_down);

				/* if network is marked up;
				 * and first_recon and last_recon are 60+ apart;
				 * and the average no. of recons counted is
				 *    > RECON_THRESHOLD/min;
				 * then print a warning message.
				 */
				if (!lp->network_down
				    && (lp->last_recon - lp->first_recon) <= HZ * 60
				  && lp->num_recons >= RECON_THRESHOLD) {
					lp->network_down = 1;
					BUGMSG(D_NORMAL, "many reconfigurations detected: cabling problem?\n");
				} else if (!lp->network_down
					   && lp->last_recon - lp->first_recon > HZ * 60) {
					/* reset counters if we've gone for over a minute. */
					lp->first_recon = lp->last_recon;
					lp->num_recons = 1;
				}
			}
		} else if (lp->network_down && jiffies - lp->last_recon > HZ * 10) {
			if (lp->network_down)
				BUGMSG(D_NORMAL, "cabling restored?\n");
			lp->first_recon = lp->last_recon = 0;
			lp->num_recons = lp->network_down = 0;

			BUGMSG(D_DURING, "not recon: clearing counters anyway.\n");
		}
	}
	while (--boguscount && didsomething);

	BUGMSG(D_DURING, "arcnet_interrupt complete (status=%Xh, count=%d)\n",
	       ASTATUS(), boguscount);
	BUGMSG(D_DURING, "\n");


	AINTMASK(0);
	udelay(1);
	AINTMASK(lp->intmask);
}


/*
 * This is a generic packet receiver that calls arcnet??_rx depending on the
 * protocol ID found.
 */
void arcnet_rx(struct net_device *dev, int bufnum)
{
	struct arcnet_local *lp = (struct arcnet_local *) dev->priv;
	struct archdr pkt;
	struct arc_rfc1201 *soft;
	int length, ofs;

	soft = &pkt.soft.rfc1201;

	lp->hw.copy_from_card(dev, bufnum, 0, &pkt, sizeof(ARC_HDR_SIZE));
	if (pkt.hard.offset[0]) {
		ofs = pkt.hard.offset[0];
		length = 256 - ofs;
	} else {
		ofs = pkt.hard.offset[1];
		length = 512 - ofs;
	}

	/* get the full header, if possible */
	if (sizeof(pkt.soft) < length)
		lp->hw.copy_from_card(dev, bufnum, ofs, soft, sizeof(pkt.soft));
	else {
		memset(&pkt.soft, 0, sizeof(pkt.soft));
		lp->hw.copy_from_card(dev, bufnum, ofs, soft, length);
	}

	BUGMSG(D_DURING, "Buffer #%d: received packet from %02Xh to %02Xh "
	       "(%d+4 bytes)\n",
	       bufnum, pkt.hard.source, pkt.hard.dest, length);

	lp->stats.rx_packets++;
	lp->stats.rx_bytes += length + ARC_HDR_SIZE;

	/* call the right receiver for the protocol */
	if (arc_proto_map[soft->proto] != &arc_proto_null) {
		BUGLVL(D_PROTO) {
			struct ArcProto
			*oldp = arc_proto_map[lp->default_proto[pkt.hard.source]],
			*newp = arc_proto_map[soft->proto];

			if (oldp != newp) {
				BUGMSG(D_PROTO,
				       "got protocol %02Xh; encap for host %02Xh is now '%c'"
				       " (was '%c')\n", soft->proto, pkt.hard.source,
				       newp->suffix, oldp->suffix);
			}
		}

		/* broadcasts will always be done with the last-used encap. */
		lp->default_proto[0] = soft->proto;

		/* in striking contrast, the following isn't a hack. */
		lp->default_proto[pkt.hard.source] = soft->proto;
	}
	/* call the protocol-specific receiver. */
	arc_proto_map[soft->proto]->rx(dev, bufnum, &pkt, length);
}



/* 
 * Get the current statistics.  This may be called with the card open or
 * closed.
 */
static struct net_device_stats *arcnet_get_stats(struct net_device *dev)
{
	struct arcnet_local *lp = (struct arcnet_local *) dev->priv;
	return &lp->stats;
}


static void null_rx(struct net_device *dev, int bufnum,
		    struct archdr *pkthdr, int length)
{
	BUGMSG(D_PROTO,
	"rx: don't know how to deal with proto %02Xh from host %02Xh.\n",
	       pkthdr->soft.rfc1201.proto, pkthdr->hard.source);
}


static int null_build_header(struct sk_buff *skb, struct net_device *dev,
			     unsigned short type, uint8_t daddr)
{
	struct arcnet_local *lp = (struct arcnet_local *) dev->priv;

	BUGMSG(D_PROTO,
	       "tx: can't build header for encap %02Xh; load a protocol driver.\n",
	       lp->default_proto[daddr]);

	/* always fails */
	return 0;
}


/* the "do nothing" prepare_tx function warns that there's nothing to do. */
static int null_prepare_tx(struct net_device *dev, struct archdr *pkt,
			   int length, int bufnum)
{
	struct arcnet_local *lp = (struct arcnet_local *) dev->priv;
	struct arc_hardware newpkt;

	BUGMSG(D_PROTO, "tx: no encap for this host; load a protocol driver.\n");

	/* send a packet to myself -- will never get received, of course */
	newpkt.source = newpkt.dest = dev->dev_addr[0];

	/* only one byte of actual data (and it's random) */
	newpkt.offset[0] = 0xFF;

	lp->hw.copy_to_card(dev, bufnum, 0, &newpkt, ARC_HDR_SIZE);

	return 1;		/* done */
}
