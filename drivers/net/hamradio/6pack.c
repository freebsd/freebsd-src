/*
 * 6pack.c	This module implements the 6pack protocol for kernel-based
 *		devices like TTY. It interfaces between a raw TTY and the
 *		kernel's AX.25 protocol layers.
 *
 * Version:	@(#)6pack.c	0.3.0	04/07/98
 *
 * Authors:	Andreas Könsgen <ajk@iehk.rwth-aachen.de>
 *
 * Quite a lot of stuff "stolen" by Jörg Reuter from slip.c, written by
 *
 *		Laurence Culhane, <loz@holmes.demon.co.uk>
 *		Fred N. van Kempen, <waltje@uwalt.nl.mugnet.org>
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/tty.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/timer.h>
#include <net/ax25.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/if_arp.h>
#include <linux/init.h>
#include <linux/ip.h>
#include <linux/tcp.h>

#define SIXPACK_VERSION    "Revision: 0.3.0"

/* sixpack priority commands */
#define SIXP_SEOF	0x40	/* start and end of a 6pack frame */
#define SIXP_TX_URUN	0x48	/* transmit overrun */
#define SIXP_RX_ORUN	0x50	/* receive overrun */
#define SIXP_RX_BUF_OVL	0x58	/* receive buffer overflow */

#define SIXP_CHKSUM	0xFF	/* valid checksum of a 6pack frame */

/* masks to get certain bits out of the status bytes sent by the TNC */

#define SIXP_CMD_MASK		0xC0
#define SIXP_CHN_MASK		0x07
#define SIXP_PRIO_CMD_MASK	0x80
#define SIXP_STD_CMD_MASK	0x40
#define SIXP_PRIO_DATA_MASK	0x38
#define SIXP_TX_MASK		0x20
#define SIXP_RX_MASK		0x10
#define SIXP_RX_DCD_MASK	0x18
#define SIXP_LEDS_ON		0x78
#define SIXP_LEDS_OFF		0x60
#define SIXP_CON		0x08
#define SIXP_STA		0x10

#define SIXP_FOUND_TNC		0xe9
#define SIXP_CON_ON		0x68
#define SIXP_DCD_MASK		0x08
#define SIXP_DAMA_OFF		0

/* default level 2 parameters */
#define SIXP_TXDELAY			25	/* in 10 ms */
#define SIXP_PERSIST			50	/* in 256ths */
#define SIXP_SLOTTIME			10	/* in 10 ms */
#define SIXP_INIT_RESYNC_TIMEOUT	150	/* in 10 ms */
#define SIXP_RESYNC_TIMEOUT		500	/* in 10 ms */

/* 6pack configuration. */
#define SIXP_NRUNIT			31      /* MAX number of 6pack channels */
#define SIXP_MTU			256	/* Default MTU */

enum sixpack_flags {
	SIXPF_INUSE,	/* Channel in use	*/
	SIXPF_ERROR,	/* Parity, etc. error	*/
};

struct sixpack {
	int			magic;

	/* Various fields. */
	struct tty_struct	*tty;		/* ptr to TTY structure		*/
	struct net_device	*dev;		/* easy for intr handling	*/

	/* These are pointers to the malloc()ed frame buffers. */
	unsigned char		*rbuff;		/* receiver buffer		*/
	int			rcount;         /* received chars counter       */
	unsigned char		*xbuff;		/* transmitter buffer		*/
	unsigned char		*xhead;         /* pointer to next byte to XMIT */
	int			xleft;          /* bytes left in XMIT queue     */

	unsigned char		raw_buf[4];
	unsigned char		cooked_buf[400];

	unsigned int		rx_count;
	unsigned int		rx_count_cooked;

	/* 6pack interface statistics. */
	struct net_device_stats stats;

	int			mtu;		/* Our mtu (to spot changes!)   */
	int			buffsize;       /* Max buffers sizes            */

	unsigned long		flags;		/* Flag values/ mode etc	*/
	unsigned char		mode;		/* 6pack mode			*/

	/* 6pack stuff */
	unsigned char		tx_delay;
	unsigned char		persistance;
	unsigned char		slottime;
	unsigned char		duplex;
	unsigned char		led_state;
	unsigned char		status;
	unsigned char		status1;
	unsigned char		status2;
	unsigned char		tx_enable;
	unsigned char		tnc_ok;

	struct timer_list	tx_t;
	struct timer_list	resync_t;
};

#define AX25_6PACK_HEADER_LEN 0
#define SIXPACK_MAGIC 0x5304

typedef struct sixpack_ctrl {
	struct sixpack	ctrl;			/* 6pack things			*/
	struct net_device	dev;		/* the device			*/
} sixpack_ctrl_t;
static sixpack_ctrl_t **sixpack_ctrls;

int sixpack_maxdev = SIXP_NRUNIT;	/* Can be overridden with insmod! */
MODULE_PARM(sixpack_maxdev, "i");
MODULE_PARM_DESC(sixpack_maxdev, "number of 6PACK devices");

static void sp_start_tx_timer(struct sixpack *);
static void sp_xmit_on_air(unsigned long);
static void resync_tnc(unsigned long);
static void sixpack_decode(struct sixpack *, unsigned char[], int);
static int encode_sixpack(unsigned char *, unsigned char *, int, unsigned char);
static int sixpack_init(struct net_device *dev);

static void decode_prio_command(unsigned char, struct sixpack *);
static void decode_std_command(unsigned char, struct sixpack *);
static void decode_data(unsigned char, struct sixpack *);

static int tnc_init(struct sixpack *);

/* Find a free 6pack channel, and link in this `tty' line. */
static inline struct sixpack *sp_alloc(void)
{
	sixpack_ctrl_t *spp = NULL;
	int i;

	for (i = 0; i < sixpack_maxdev; i++) {
		spp = sixpack_ctrls[i];

		if (spp == NULL)
			break;

		if (!test_and_set_bit(SIXPF_INUSE, &spp->ctrl.flags))
			break;
	}

	/* Too many devices... */
	if (i >= sixpack_maxdev)
		return NULL;

	/* If no channels are available, allocate one */
	if (!spp &&
	    (sixpack_ctrls[i] = (sixpack_ctrl_t *)kmalloc(sizeof(sixpack_ctrl_t),
						    GFP_KERNEL)) != NULL) {
		spp = sixpack_ctrls[i];
		memset(spp, 0, sizeof(sixpack_ctrl_t));

		/* Initialize channel control data */
		set_bit(SIXPF_INUSE, &spp->ctrl.flags);
		spp->ctrl.tty         = NULL;
		sprintf(spp->dev.name, "sp%d", i);
		spp->dev.base_addr    = i;
		spp->dev.priv         = (void *) &spp->ctrl;
		spp->dev.next         = NULL;
		spp->dev.init         = sixpack_init;
	}

	if (spp != NULL) {
		/* register device so that it can be ifconfig'ed       */
		/* sixpack_init() will be called as a side-effect         */
		/* SIDE-EFFECT WARNING: sixpack_init() CLEARS spp->ctrl ! */

		if (register_netdev(&spp->dev) == 0) {
			set_bit(SIXPF_INUSE, &spp->ctrl.flags);
			spp->ctrl.dev = &spp->dev;
			spp->dev.priv = (void *) &spp->ctrl;
			SET_MODULE_OWNER(&spp->dev);
			return &spp->ctrl;
		} else {
			clear_bit(SIXPF_INUSE, &spp->ctrl.flags);
			printk(KERN_WARNING "sp_alloc() - register_netdev() failure.\n");
		}
	}

	return NULL;
}


/* Free a 6pack channel. */
static inline void sp_free(struct sixpack *sp)
{
	/* Free all 6pack frame buffers. */
	if (sp->rbuff)
		kfree(sp->rbuff);
	sp->rbuff = NULL;
	if (sp->xbuff)
		kfree(sp->xbuff);
	sp->xbuff = NULL;

	if (!test_and_clear_bit(SIXPF_INUSE, &sp->flags))
		printk(KERN_WARNING "%s: sp_free for already free unit.\n", sp->dev->name);
}


/* Send one completely decapsulated IP datagram to the IP layer. */

/* This is the routine that sends the received data to the kernel AX.25.
   'cmd' is the KISS command. For AX.25 data, it is zero. */

static void sp_bump(struct sixpack *sp, char cmd)
{
	struct sk_buff *skb;
	int count;
	unsigned char *ptr;

	count = sp->rcount+1;

	sp->stats.rx_bytes += count;

	if ((skb = dev_alloc_skb(count)) == NULL) {
		printk(KERN_DEBUG "%s: memory squeeze, dropping packet.\n", sp->dev->name);
		sp->stats.rx_dropped++;
		return;
	}

	skb->dev = sp->dev;
	ptr = skb_put(skb, count);
	*ptr++ = cmd;	/* KISS command */

	memcpy(ptr, (sp->cooked_buf)+1, count);
	skb->mac.raw = skb->data;
	skb->protocol = htons(ETH_P_AX25);
	netif_rx(skb);
	sp->stats.rx_packets++;
}


/* ----------------------------------------------------------------------- */

/* Encapsulate one AX.25 frame and stuff into a TTY queue. */
static void sp_encaps(struct sixpack *sp, unsigned char *icp, int len)
{
	unsigned char *p;
	int actual, count;

	if (len > sp->mtu) {	/* sp->mtu = AX25_MTU = max. PACLEN = 256 */
		printk(KERN_DEBUG "%s: truncating oversized transmit packet!\n", sp->dev->name);
		sp->stats.tx_dropped++;
		netif_start_queue(sp->dev);
		return;
	}

	p = icp;

	if (p[0] > 5) {
		printk(KERN_DEBUG "%s: invalid KISS command -- dropped\n", sp->dev->name);
		netif_start_queue(sp->dev);
		return;
	}

	if ((p[0] != 0) && (len > 2)) {
		printk(KERN_DEBUG "%s: KISS control packet too long -- dropped\n", sp->dev->name);
		netif_start_queue(sp->dev);
		return;
	}

	if ((p[0] == 0) && (len < 15)) {
		printk(KERN_DEBUG "%s: bad AX.25 packet to transmit -- dropped\n", sp->dev->name);
		netif_start_queue(sp->dev);
		sp->stats.tx_dropped++;
		return;
	}

	count = encode_sixpack(p, (unsigned char *) sp->xbuff, len, sp->tx_delay);
	sp->tty->flags |= (1 << TTY_DO_WRITE_WAKEUP);

	switch (p[0]) {
		case 1:	sp->tx_delay = p[1];		return;
		case 2:	sp->persistance = p[1];		return;
		case 3: sp->slottime = p[1];		return;
		case 4: /* ignored */			return;
		case 5: sp->duplex = p[1];		return;
	}

	if (p[0] == 0) {
		/* in case of fullduplex or DAMA operation, we don't take care
		   about the state of the DCD or of any timers, as the determination
		   of the correct time to send is the job of the AX.25 layer. We send
		   immediately after data has arrived. */
		if (sp->duplex == 1) {
			sp->led_state = 0x70;
			sp->tty->driver.write(sp->tty, 0, &sp->led_state, 1);
			sp->tx_enable = 1;
			actual = sp->tty->driver.write(sp->tty, 0, sp->xbuff, count);
			sp->xleft = count - actual;
			sp->xhead = sp->xbuff + actual;
			sp->led_state = 0x60;
			sp->tty->driver.write(sp->tty, 0, &sp->led_state, 1);
		} else {
			sp->xleft = count;
			sp->xhead = sp->xbuff;
			sp->status2 = count;
			if (sp->duplex == 0)
				sp_start_tx_timer(sp);
		}
	}
}

/*
 * Called by the TTY driver when there's room for more data.  If we have
 * more packets to send, we send them here.
 */
static void sixpack_write_wakeup(struct tty_struct *tty)
{
	int actual;
	struct sixpack *sp = (struct sixpack *) tty->disc_data;

	/* First make sure we're connected. */
	if (!sp || sp->magic != SIXPACK_MAGIC ||
	    !netif_running(sp->dev))
		return;

	if (sp->xleft <= 0)  {
		/* Now serial buffer is almost free & we can start
		 * transmission of another packet */
		sp->stats.tx_packets++;
		tty->flags &= ~(1 << TTY_DO_WRITE_WAKEUP);
		sp->tx_enable = 0;
		netif_wake_queue(sp->dev);
		return;
	}

	if (sp->tx_enable == 1) {
		actual = tty->driver.write(tty, 0, sp->xhead, sp->xleft);
		sp->xleft -= actual;
		sp->xhead += actual;
	}
}

/* ----------------------------------------------------------------------- */

/* Encapsulate an IP datagram and kick it into a TTY queue. */

static int sp_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct sixpack *sp = (struct sixpack *) dev->priv;

	/* We were not busy, so we are now... :-) */
	netif_stop_queue(dev);
	sp->stats.tx_bytes += skb->len;
	sp_encaps(sp, skb->data, skb->len);
	dev_kfree_skb(skb);
	return 0;
}


/* perform the persistence/slottime algorithm for CSMA access. If the persistence
   check was successful, write the data to the serial driver. Note that in case
   of DAMA operation, the data is not sent here. */

static void sp_xmit_on_air(unsigned long channel)
{
	struct sixpack *sp = (struct sixpack *) channel;
	int actual;
	static unsigned char random;

	random = random * 17 + 41;

	if (((sp->status1 & SIXP_DCD_MASK) == 0) && (random < sp->persistance)) {
		sp->led_state = 0x70;
		sp->tty->driver.write(sp->tty, 0, &sp->led_state, 1);
		sp->tx_enable = 1;
		actual = sp->tty->driver.write(sp->tty, 0, sp->xbuff, sp->status2);
		sp->xleft -= actual;
		sp->xhead += actual;
		sp->led_state = 0x60;
		sp->tty->driver.write(sp->tty, 0, &sp->led_state, 1);
		sp->status2 = 0;
	} else
		sp_start_tx_timer(sp);
}


/* Return the frame type ID */
static int sp_header(struct sk_buff *skb, struct net_device *dev, unsigned short type,
	  void *daddr, void *saddr, unsigned len)
{
#ifdef CONFIG_INET
	if (type != htons(ETH_P_AX25))
		return ax25_encapsulate(skb, dev, type, daddr, saddr, len);
#endif
	return 0;
}


static int sp_rebuild_header(struct sk_buff *skb)
{
#ifdef CONFIG_INET
	return ax25_rebuild_header(skb);
#else
	return 0;
#endif
}


/* Open the low-level part of the 6pack channel. */
static int sp_open(struct net_device *dev)
{
	struct sixpack *sp = (struct sixpack *) dev->priv;
	unsigned long len;

	if (sp->tty == NULL)
		return -ENODEV;

	/*
	 * Allocate the 6pack frame buffers:
	 *
	 * rbuff	Receive buffer.
	 * xbuff	Transmit buffer.
	 */

	/* !!! length of the buffers. MTU is IP MTU, not PACLEN!
	 */

	len = dev->mtu * 2;

	if ((sp->rbuff = kmalloc(len + 4, GFP_KERNEL)) == NULL)
		return -ENOMEM;

	if ((sp->xbuff = kmalloc(len + 4, GFP_KERNEL)) == NULL) {
		kfree(sp->rbuff);
		return -ENOMEM;
	}

	sp->mtu	     = AX25_MTU + 73;
	sp->buffsize = len;
	sp->rcount   = 0;
	sp->rx_count = 0;
	sp->rx_count_cooked = 0;
	sp->xleft    = 0;

	sp->flags   &= (1 << SIXPF_INUSE);      /* Clear ESCAPE & ERROR flags */

	sp->duplex = 0;
	sp->tx_delay    = SIXP_TXDELAY;
	sp->persistance = SIXP_PERSIST;
	sp->slottime    = SIXP_SLOTTIME;
	sp->led_state   = 0x60;
	sp->status      = 1;
	sp->status1     = 1;
	sp->status2     = 0;
	sp->tnc_ok      = 0;
	sp->tx_enable   = 0;

	netif_start_queue(dev);

	init_timer(&sp->tx_t);
	init_timer(&sp->resync_t);
	return 0;
}


/* Close the low-level part of the 6pack channel. */
static int sp_close(struct net_device *dev)
{
	struct sixpack *sp = (struct sixpack *) dev->priv;

	if (sp->tty == NULL)
		return -EBUSY;

	sp->tty->flags &= ~(1 << TTY_DO_WRITE_WAKEUP);

	netif_stop_queue(dev);
	return 0;
}

static int sixpack_receive_room(struct tty_struct *tty)
{
	return 65536;  /* We can handle an infinite amount of data. :-) */
}

/* !!! receive state machine */

/*
 * Handle the 'receiver data ready' interrupt.
 * This function is called by the 'tty_io' module in the kernel when
 * a block of 6pack data has been received, which can now be decapsulated
 * and sent on to some IP layer for further processing.
 */
static void sixpack_receive_buf(struct tty_struct *tty, const unsigned char *cp, char *fp, int count)
{
	unsigned char buf[512];
	unsigned long flags;
	int count1;

	struct sixpack *sp = (struct sixpack *) tty->disc_data;

	if (!sp || sp->magic != SIXPACK_MAGIC ||
	    !netif_running(sp->dev) || !count)
		return;

	save_flags(flags);
	cli();
	memcpy(buf, cp, count<sizeof(buf)? count:sizeof(buf));
	restore_flags(flags);

	/* Read the characters out of the buffer */

	count1 = count;
	while (count) {
		count--;
		if (fp && *fp++) {
			if (!test_and_set_bit(SIXPF_ERROR, &sp->flags))
				sp->stats.rx_errors++;
			continue;
		}
	}
	sixpack_decode(sp, buf, count1);
}

/*
 * Open the high-level part of the 6pack channel.
 * This function is called by the TTY module when the
 * 6pack line discipline is called for.  Because we are
 * sure the tty line exists, we only have to link it to
 * a free 6pcack channel...
 */
static int sixpack_open(struct tty_struct *tty)
{
	struct sixpack *sp = (struct sixpack *) tty->disc_data;
	int err;

	/* First make sure we're not already connected. */

	if (sp && sp->magic == SIXPACK_MAGIC)
		return -EEXIST;

	/* OK.  Find a free 6pack channel to use. */
	if ((sp = sp_alloc()) == NULL)
		return -ENFILE;
	sp->tty = tty;
	tty->disc_data = sp;
	if (tty->driver.flush_buffer)
		tty->driver.flush_buffer(tty);

	if (tty->ldisc.flush_buffer)
		tty->ldisc.flush_buffer(tty);

	/* Restore default settings */
	sp->dev->type = ARPHRD_AX25;

	/* Perform the low-level 6pack initialization. */
	if ((err = sp_open(sp->dev)))
		return err;

	/* Done.  We have linked the TTY line to a channel. */

	tnc_init(sp);

	return sp->dev->base_addr;
}


/*
 * Close down a 6pack channel.
 * This means flushing out any pending queues, and then restoring the
 * TTY line discipline to what it was before it got hooked to 6pack
 * (which usually is TTY again).
 */
static void sixpack_close(struct tty_struct *tty)
{
	struct sixpack *sp = (struct sixpack *) tty->disc_data;

	/* First make sure we're connected. */
	if (!sp || sp->magic != SIXPACK_MAGIC)
		return;

	rtnl_lock();
	dev_close(sp->dev);

	del_timer(&sp->tx_t);
	del_timer(&sp->resync_t);

	tty->disc_data = 0;
	sp->tty = NULL;

	sp_free(sp);
	unregister_netdevice(sp->dev);
	rtnl_unlock();
}


static struct net_device_stats *sp_get_stats(struct net_device *dev)
{
	struct sixpack *sp = (struct sixpack *) dev->priv;
	return &sp->stats;
}


static int sp_set_mac_address(struct net_device *dev, void *addr)
{
	return copy_from_user(dev->dev_addr, addr, AX25_ADDR_LEN) ? -EFAULT : 0;
}

static int sp_set_dev_mac_address(struct net_device *dev, void *addr)
{
	struct sockaddr *sa = addr;
	memcpy(dev->dev_addr, sa->sa_data, AX25_ADDR_LEN);
	return 0;
}


/* Perform I/O control on an active 6pack channel. */
static int sixpack_ioctl(struct tty_struct *tty, void *file, int cmd, void *arg)
{
	struct sixpack *sp = (struct sixpack *) tty->disc_data;
	unsigned int tmp;

	/* First make sure we're connected. */
	if (!sp || sp->magic != SIXPACK_MAGIC)
		return -EINVAL;

	switch(cmd) {
	case SIOCGIFNAME:
		return copy_to_user(arg, sp->dev->name, strlen(sp->dev->name) + 1) ? -EFAULT : 0;

	case SIOCGIFENCAP:
		return put_user(0, (int *)arg);

	case SIOCSIFENCAP:
		if (get_user(tmp, (int *) arg))
			return -EFAULT;

		sp->mode = tmp;
		sp->dev->addr_len        = AX25_ADDR_LEN;	  /* sizeof an AX.25 addr */
		sp->dev->hard_header_len = AX25_KISS_HEADER_LEN + AX25_MAX_HEADER_LEN + 3;
		sp->dev->type            = ARPHRD_AX25;

		return 0;

	 case SIOCSIFHWADDR:
		return sp_set_mac_address(sp->dev, arg);

	/* Allow stty to read, but not set, the serial port */
	case TCGETS:
	case TCGETA:
		return n_tty_ioctl(tty, (struct file *) file, cmd, (unsigned long) arg);

	default:
		return -ENOIOCTLCMD;
	}
}

static int sp_open_dev(struct net_device *dev)
{
	struct sixpack *sp = (struct sixpack *) dev->priv;
	if (sp->tty == NULL)
		return -ENODEV;
	return 0;
}

/* Fill in our line protocol discipline */
static struct tty_ldisc sp_ldisc = {
	magic:		TTY_LDISC_MAGIC,
	name:		"6pack",
	open:		sixpack_open,
	close:		sixpack_close,
	ioctl:		(int (*)(struct tty_struct *, struct file *,
			unsigned int, unsigned long)) sixpack_ioctl,
	receive_buf:	sixpack_receive_buf,
	receive_room:	sixpack_receive_room,
	write_wakeup:	sixpack_write_wakeup,
};

/* Initialize 6pack control device -- register 6pack line discipline */

static char msg_banner[]  __initdata = KERN_INFO "AX.25: 6pack driver, " SIXPACK_VERSION " (dynamic channels, max=%d)\n";
static char msg_nomem[]   __initdata = KERN_ERR  "6pack: can't allocate sixpack_ctrls[] array! No 6pack available.\n";
static char msg_regfail[] __initdata = KERN_ERR  "6pack: can't register line discipline (err = %d)\n";

static int __init sixpack_init_driver(void)
{
	int status;

	/* Do sanity checks on maximum device parameter. */
	if (sixpack_maxdev < 4)
		sixpack_maxdev = 4;

	printk(msg_banner, sixpack_maxdev);

	sixpack_ctrls = (sixpack_ctrl_t **) kmalloc(sizeof(void*)*sixpack_maxdev, GFP_KERNEL);
	if (sixpack_ctrls == NULL) {
		printk(msg_nomem);
		return -ENOMEM;
	}

	/* Clear the pointer array, we allocate devices when we need them */
	memset(sixpack_ctrls, 0, sizeof(void*)*sixpack_maxdev); /* Pointers */

	/* Register the provided line protocol discipline */
	if ((status = tty_register_ldisc(N_6PACK, &sp_ldisc)) != 0) {
		printk(msg_regfail, status);
		kfree(sixpack_ctrls);
	}

	return status;
}

static const char msg_unregfail[] __exitdata = KERN_ERR "6pack: can't unregister line discipline (err = %d)\n";

static void __exit sixpack_exit_driver(void)
{
	int i;

	if ((i = tty_register_ldisc(N_6PACK, NULL)))
		printk(msg_unregfail, i);

	for (i = 0; i < sixpack_maxdev; i++) {
		if (sixpack_ctrls[i]) {
			/*
			* VSV = if dev->start==0, then device
			* unregistered while close proc.
			*/
			if (netif_running(&sixpack_ctrls[i]->dev))
				 unregister_netdev(&sixpack_ctrls[i]->dev);

			kfree(sixpack_ctrls[i]);
		}
	}
	kfree(sixpack_ctrls);
}


/* Initialize the 6pack driver.  Called by DDI. */
static int sixpack_init(struct net_device *dev)
{
	struct sixpack *sp = (struct sixpack *) dev->priv;

	static char ax25_bcast[AX25_ADDR_LEN] =
		{'Q'<<1,'S'<<1,'T'<<1,' '<<1,' '<<1,' '<<1,'0'<<1};
	static char ax25_test[AX25_ADDR_LEN] =
		{'L'<<1,'I'<<1,'N'<<1,'U'<<1,'X'<<1,' '<<1,'1'<<1};

	if (sp == NULL)		/* Allocation failed ?? */
		return -ENODEV;

	/* Set up the "6pack Control Block". (And clear statistics) */

	memset(sp, 0, sizeof (struct sixpack));
	sp->magic  = SIXPACK_MAGIC;
	sp->dev	   = dev;

	/* Finish setting up the DEVICE info. */
	dev->mtu		= SIXP_MTU;
	dev->hard_start_xmit	= sp_xmit;
	dev->open		= sp_open_dev;
	dev->stop		= sp_close;
	dev->hard_header	= sp_header;
	dev->get_stats	        = sp_get_stats;
	dev->set_mac_address    = sp_set_dev_mac_address;
	dev->hard_header_len	= AX25_MAX_HEADER_LEN;
	dev->addr_len		= AX25_ADDR_LEN;
	dev->type		= ARPHRD_AX25;
	dev->tx_queue_len	= 10;
	dev->rebuild_header	= sp_rebuild_header;
	dev->tx_timeout		= NULL;

	memcpy(dev->broadcast, ax25_bcast, AX25_ADDR_LEN);	/* Only activated in AX.25 mode */
	memcpy(dev->dev_addr, ax25_test, AX25_ADDR_LEN);	/*    ""      ""       ""    "" */

	/* New-style flags. */
	dev->flags		= 0;

	return 0;
}




/* ----> 6pack timer interrupt handler and friends. <---- */
static void sp_start_tx_timer(struct sixpack *sp)
{
	int when = sp->slottime;

	del_timer(&sp->tx_t);
	sp->tx_t.data = (unsigned long) sp;
	sp->tx_t.function = sp_xmit_on_air;
	sp->tx_t.expires = jiffies + ((when+1)*HZ)/100;
	add_timer(&sp->tx_t);
}


/* encode an AX.25 packet into 6pack */

static int encode_sixpack(unsigned char *tx_buf, unsigned char *tx_buf_raw, int length, unsigned char tx_delay)
{
	int count = 0;
	unsigned char checksum = 0, buf[400];
	int raw_count = 0;

	tx_buf_raw[raw_count++] = SIXP_PRIO_CMD_MASK | SIXP_TX_MASK;
	tx_buf_raw[raw_count++] = SIXP_SEOF;

	buf[0] = tx_delay;
	for (count = 1; count < length; count++)
		buf[count] = tx_buf[count];

	for (count = 0; count < length; count++)
		checksum += buf[count];
	buf[length] = (unsigned char) 0xff - checksum;

	for (count = 0; count <= length; count++) {
		if ((count % 3) == 0) {
			tx_buf_raw[raw_count++] = (buf[count] & 0x3f);
			tx_buf_raw[raw_count] = ((buf[count] >> 2) & 0x30);
		} else if ((count % 3) == 1) {
			tx_buf_raw[raw_count++] |= (buf[count] & 0x0f);
			tx_buf_raw[raw_count] =	((buf[count] >> 2) & 0x3c);
		} else {
			tx_buf_raw[raw_count++] |= (buf[count] & 0x03);
			tx_buf_raw[raw_count++] = (buf[count] >> 2);
		}
	}
	if ((length % 3) != 2)
		raw_count++;
	tx_buf_raw[raw_count++] = SIXP_SEOF;
	return raw_count;
}


/* decode a 6pack packet */

static void
sixpack_decode(struct sixpack *sp, unsigned char pre_rbuff[], int count)
{
	unsigned char inbyte;
	int count1;

	for (count1 = 0; count1 < count; count1++) {
		inbyte = pre_rbuff[count1];
		if (inbyte == SIXP_FOUND_TNC) {
			printk(KERN_INFO "6pack: TNC found.\n");
			sp->tnc_ok = 1;
			del_timer(&sp->resync_t);
		}
		if ((inbyte & SIXP_PRIO_CMD_MASK) != 0)
			decode_prio_command(inbyte, sp);
		else if ((inbyte & SIXP_STD_CMD_MASK) != 0)
			decode_std_command(inbyte, sp);
		else if ((sp->status & SIXP_RX_DCD_MASK) == SIXP_RX_DCD_MASK)
			decode_data(inbyte, sp);
	}
}

static int tnc_init(struct sixpack *sp)
{
	unsigned char inbyte = 0xe8;

	sp->tty->driver.write(sp->tty, 0, &inbyte, 1);

	del_timer(&sp->resync_t);
	sp->resync_t.data = (unsigned long) sp;
	sp->resync_t.function = resync_tnc;
	sp->resync_t.expires = jiffies + SIXP_RESYNC_TIMEOUT;
	add_timer(&sp->resync_t);

	return 0;
}


/* identify and execute a 6pack priority command byte */

static void decode_prio_command(unsigned char cmd, struct sixpack *sp)
{
	unsigned char channel;
	int actual;

	channel = cmd & SIXP_CHN_MASK;
	if ((cmd & SIXP_PRIO_DATA_MASK) != 0) {     /* idle ? */

	/* RX and DCD flags can only be set in the same prio command,
	   if the DCD flag has been set without the RX flag in the previous
	   prio command. If DCD has not been set before, something in the
	   transmission has gone wrong. In this case, RX and DCD are
	   cleared in order to prevent the decode_data routine from
	   reading further data that might be corrupt. */

		if (((sp->status & SIXP_DCD_MASK) == 0) &&
			((cmd & SIXP_RX_DCD_MASK) == SIXP_RX_DCD_MASK)) {
				if (sp->status != 1)
					printk(KERN_DEBUG "6pack: protocol violation\n");
				else
					sp->status = 0;
				cmd &= !SIXP_RX_DCD_MASK;
		}
		sp->status = cmd & SIXP_PRIO_DATA_MASK;
	}
	else { /* output watchdog char if idle */
		if ((sp->status2 != 0) && (sp->duplex == 1)) {
			sp->led_state = 0x70;
			sp->tty->driver.write(sp->tty, 0, &sp->led_state, 1);
			sp->tx_enable = 1;
			actual = sp->tty->driver.write(sp->tty, 0, sp->xbuff, sp->status2);
			sp->xleft -= actual;
			sp->xhead += actual;
			sp->led_state = 0x60;
			sp->status2 = 0;

		}
	}

	/* needed to trigger the TNC watchdog */
	sp->tty->driver.write(sp->tty, 0, &sp->led_state, 1);

        /* if the state byte has been received, the TNC is present,
           so the resync timer can be reset. */

	if (sp->tnc_ok == 1) {
		del_timer(&sp->resync_t);
		sp->resync_t.data = (unsigned long) sp;
		sp->resync_t.function = resync_tnc;
		sp->resync_t.expires = jiffies + SIXP_INIT_RESYNC_TIMEOUT;
		add_timer(&sp->resync_t);
	}

	sp->status1 = cmd & SIXP_PRIO_DATA_MASK;
}

/* try to resync the TNC. Called by the resync timer defined in
  decode_prio_command */

static void resync_tnc(unsigned long channel)
{
	static char resync_cmd = 0xe8;
	struct sixpack *sp = (struct sixpack *) channel;

	printk(KERN_INFO "6pack: resyncing TNC\n");

	/* clear any data that might have been received */

	sp->rx_count = 0;
	sp->rx_count_cooked = 0;

	/* reset state machine */

	sp->status = 1;
	sp->status1 = 1;
	sp->status2 = 0;
	sp->tnc_ok = 0;

	/* resync the TNC */

	sp->led_state = 0x60;
	sp->tty->driver.write(sp->tty, 0, &sp->led_state, 1);
	sp->tty->driver.write(sp->tty, 0, &resync_cmd, 1);


	/* Start resync timer again -- the TNC might be still absent */

	del_timer(&sp->resync_t);
	sp->resync_t.data = (unsigned long) sp;
	sp->resync_t.function = resync_tnc;
	sp->resync_t.expires = jiffies + SIXP_RESYNC_TIMEOUT;
	add_timer(&sp->resync_t);
}



/* identify and execute a standard 6pack command byte */

static void decode_std_command(unsigned char cmd, struct sixpack *sp)
{
	unsigned char checksum = 0, rest = 0, channel;
	short i;

	channel = cmd & SIXP_CHN_MASK;
	switch (cmd & SIXP_CMD_MASK) {     /* normal command */
		case SIXP_SEOF:
			if ((sp->rx_count == 0) && (sp->rx_count_cooked == 0)) {
				if ((sp->status & SIXP_RX_DCD_MASK) ==
					SIXP_RX_DCD_MASK) {
					sp->led_state = 0x68;
					sp->tty->driver.write(sp->tty, 0, &sp->led_state, 1);
				}
			} else {
				sp->led_state = 0x60;
				/* fill trailing bytes with zeroes */
				sp->tty->driver.write(sp->tty, 0, &sp->led_state, 1);
				rest = sp->rx_count;
				if (rest != 0)
					 for (i = rest; i <= 3; i++)
						decode_data(0, sp);
				if (rest == 2)
					sp->rx_count_cooked -= 2;
				else if (rest == 3)
					sp->rx_count_cooked -= 1;
				for (i = 0; i < sp->rx_count_cooked; i++)
					checksum += sp->cooked_buf[i];
				if (checksum != SIXP_CHKSUM) {
					printk(KERN_DEBUG "6pack: bad checksum %2.2x\n", checksum);
				} else {
					sp->rcount = sp->rx_count_cooked-2;
					sp_bump(sp, 0);
				}
				sp->rx_count_cooked = 0;
			}
			break;
		case SIXP_TX_URUN: printk(KERN_DEBUG "6pack: TX underrun\n");
			break;
		case SIXP_RX_ORUN: printk(KERN_DEBUG "6pack: RX overrun\n");
			break;
		case SIXP_RX_BUF_OVL:
			printk(KERN_DEBUG "6pack: RX buffer overflow\n");
	}
}

/* decode 4 sixpack-encoded bytes into 3 data bytes */

static void decode_data(unsigned char inbyte, struct sixpack *sp)
{
	unsigned char *buf;

	if (sp->rx_count != 3)
		sp->raw_buf[sp->rx_count++] = inbyte;
	else {
		buf = sp->raw_buf;
		sp->cooked_buf[sp->rx_count_cooked++] =
			buf[0] | ((buf[1] << 2) & 0xc0);
		sp->cooked_buf[sp->rx_count_cooked++] =
			(buf[1] & 0x0f) | ((buf[2] << 2) & 0xf0);
		sp->cooked_buf[sp->rx_count_cooked++] =
			(buf[2] & 0x03) | (inbyte << 2);
		sp->rx_count = 0;
	}
}


MODULE_AUTHOR("Andreas Könsgen <ajk@ccac.rwth-aachen.de>");
MODULE_DESCRIPTION("6pack driver for AX.25");
MODULE_LICENSE("GPL");

module_init(sixpack_init_driver);
module_exit(sixpack_exit_driver);
