/*
 *	Things to sort out:
 *
 *	o	tbusy handling
 *	o	allow users to set the parameters
 *	o	sync/async switching ?
 *
 *	Note: This does _not_ implement CCITT X.25 asynchronous framing
 *	recommendations. Its primarily for testing purposes. If you wanted
 *	to do CCITT then in theory all you need is to nick the HDLC async
 *	checksum routines from ppp.c
 *      Changes:
 *
 *	2000-10-29	Henner Eisen	lapb_data_indication() return status.
 */

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
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/x25.h>
#include <linux/lapb.h>
#include <linux/init.h>
#include "x25_asy.h"

typedef struct x25_ctrl {
	struct x25_asy	ctrl;		/* X.25 things			*/
	struct net_device	dev;		/* the device			*/
} x25_asy_ctrl_t;

static x25_asy_ctrl_t	**x25_asy_ctrls = NULL;

int x25_asy_maxdev = SL_NRUNIT;		/* Can be overridden with insmod! */

MODULE_PARM(x25_asy_maxdev, "i");
MODULE_LICENSE("GPL");

static struct tty_ldisc	x25_ldisc;

static int x25_asy_esc(unsigned char *p, unsigned char *d, int len);
static void x25_asy_unesc(struct x25_asy *sl, unsigned char c);

/* Find a free X.25 channel, and link in this `tty' line. */
static inline struct x25_asy *x25_asy_alloc(void)
{
	x25_asy_ctrl_t *slp = NULL;
	int i;

	if (x25_asy_ctrls == NULL)
		return NULL;	/* Master array missing ! */

	for (i = 0; i < x25_asy_maxdev; i++) 
	{
		slp = x25_asy_ctrls[i];
		/* Not allocated ? */
		if (slp == NULL)
			break;
		/* Not in use ? */
		if (!test_and_set_bit(SLF_INUSE, &slp->ctrl.flags))
			break;
	}
	/* SLP is set.. */

	/* Sorry, too many, all slots in use */
	if (i >= x25_asy_maxdev)
		return NULL;

	/* If no channels are available, allocate one */
	if (!slp &&
	    (x25_asy_ctrls[i] = (x25_asy_ctrl_t *)kmalloc(sizeof(x25_asy_ctrl_t),
						    GFP_KERNEL)) != NULL) {
		slp = x25_asy_ctrls[i];
		memset(slp, 0, sizeof(x25_asy_ctrl_t));

		/* Initialize channel control data */
		set_bit(SLF_INUSE, &slp->ctrl.flags);
		slp->ctrl.tty         = NULL;
		sprintf(slp->dev.name, "x25asy%d", i);
		slp->dev.base_addr    = i;
		slp->dev.priv         = (void*)&(slp->ctrl);
		slp->dev.next         = NULL;
		slp->dev.init         = x25_asy_init;
	}
	if (slp != NULL) 
	{

		/* register device so that it can be ifconfig'ed       */
		/* x25_asy_init() will be called as a side-effect      */
		/* SIDE-EFFECT WARNING: x25_asy_init() CLEARS slp->ctrl ! */

		if (register_netdev(&(slp->dev)) == 0) 
		{
			/* (Re-)Set the INUSE bit.   Very Important! */
			set_bit(SLF_INUSE, &slp->ctrl.flags);
			slp->ctrl.dev = &(slp->dev);
			slp->dev.priv = (void*)&(slp->ctrl);
			return (&(slp->ctrl));
		}
		else
		{
			clear_bit(SLF_INUSE,&(slp->ctrl.flags));
			printk("x25_asy_alloc() - register_netdev() failure.\n");
		}
	}
	return NULL;
}


/* Free an X.25 channel. */

static inline void x25_asy_free(struct x25_asy *sl)
{
	/* Free all X.25 frame buffers. */
	if (sl->rbuff)  {
		kfree(sl->rbuff);
	}
	sl->rbuff = NULL;
	if (sl->xbuff)  {
		kfree(sl->xbuff);
	}
	sl->xbuff = NULL;

	if (!test_and_clear_bit(SLF_INUSE, &sl->flags)) {
		printk("%s: x25_asy_free for already free unit.\n", sl->dev->name);
	}
}

/* MTU has been changed by the IP layer. Unfortunately we are not told
   about this, but we spot it ourselves and fix things up. We could be
   in an upcall from the tty driver, or in an ip packet queue. */

static void x25_asy_changed_mtu(struct x25_asy *sl)
{
	struct net_device *dev = sl->dev;
	unsigned char *xbuff, *rbuff, *oxbuff, *orbuff;
	int len;
	unsigned long flags;

	len = dev->mtu * 2;

	xbuff = (unsigned char *) kmalloc (len + 4, GFP_ATOMIC);
	rbuff = (unsigned char *) kmalloc (len + 4, GFP_ATOMIC);

	if (xbuff == NULL || rbuff == NULL)  
	{
		printk("%s: unable to grow X.25 buffers, MTU change cancelled.\n",
		       sl->dev->name);
		dev->mtu = sl->mtu;
		if (xbuff != NULL)  
			kfree(xbuff);
		if (rbuff != NULL)  
			kfree(rbuff);
		return;
	}

	save_flags(flags); 
	cli();

	oxbuff    = sl->xbuff;
	sl->xbuff = xbuff;
	orbuff    = sl->rbuff;
	sl->rbuff = rbuff;

	if (sl->xleft)  {
		if (sl->xleft <= len)  {
			memcpy(sl->xbuff, sl->xhead, sl->xleft);
		} else  {
			sl->xleft = 0;
			sl->tx_dropped++;
		}
	}
	sl->xhead = sl->xbuff;

	if (sl->rcount)  {
		if (sl->rcount <= len) {
			memcpy(sl->rbuff, orbuff, sl->rcount);
		} else  {
			sl->rcount = 0;
			sl->rx_over_errors++;
			set_bit(SLF_ERROR, &sl->flags);
		}
	}
	sl->mtu      = dev->mtu;

	sl->buffsize = len;

	restore_flags(flags);

	if (oxbuff != NULL) 
		kfree(oxbuff);
	if (orbuff != NULL)
		kfree(orbuff);
}


/* Set the "sending" flag.  This must be atomic, hence the ASM. */

static inline void x25_asy_lock(struct x25_asy *sl)
{
	netif_stop_queue(sl->dev);
}


/* Clear the "sending" flag.  This must be atomic, hence the ASM. */

static inline void x25_asy_unlock(struct x25_asy *sl)
{
	netif_wake_queue(sl->dev);
}

/* Send one completely decapsulated IP datagram to the IP layer. */

static void x25_asy_bump(struct x25_asy *sl)
{
	struct sk_buff *skb;
	int count;
	int err;

	count = sl->rcount;
	sl->rx_bytes+=count;
	
	skb = dev_alloc_skb(count+1);
	if (skb == NULL)  
	{
		printk("%s: memory squeeze, dropping packet.\n", sl->dev->name);
		sl->rx_dropped++;
		return;
	}
	skb_push(skb,1);	/* LAPB internal control */
	skb->dev = sl->dev;
	memcpy(skb_put(skb,count), sl->rbuff, count);
	skb->mac.raw=skb->data;
	skb->protocol=htons(ETH_P_X25);
	if((err=lapb_data_received(sl,skb))!=LAPB_OK)
	{
		kfree_skb(skb);
		printk(KERN_DEBUG "x25_asy: data received err - %d\n",err);
	}
	else
	{
		netif_rx(skb);
		sl->rx_packets++;
	}
}

/* Encapsulate one IP datagram and stuff into a TTY queue. */
static void x25_asy_encaps(struct x25_asy *sl, unsigned char *icp, int len)
{
	unsigned char *p;
	int actual, count;


	if (sl->mtu != sl->dev->mtu) {	/* Someone has been ifconfigging */

		x25_asy_changed_mtu(sl);
	}

	if (len > sl->mtu) 
	{		/* Sigh, shouldn't occur BUT ... */
		len = sl->mtu;
		printk ("%s: truncating oversized transmit packet!\n", sl->dev->name);
		sl->tx_dropped++;
		x25_asy_unlock(sl);
		return;
	}

	p = icp;
	count = x25_asy_esc(p, (unsigned char *) sl->xbuff, len);

	/* Order of next two lines is *very* important.
	 * When we are sending a little amount of data,
	 * the transfer may be completed inside driver.write()
	 * routine, because it's running with interrupts enabled.
	 * In this case we *never* got WRITE_WAKEUP event,
	 * if we did not request it before write operation.
	 *       14 Oct 1994  Dmitry Gorodchanin.
	 */
	sl->tty->flags |= (1 << TTY_DO_WRITE_WAKEUP);
	actual = sl->tty->driver.write(sl->tty, 0, sl->xbuff, count);
	sl->xleft = count - actual;
	sl->xhead = sl->xbuff + actual;
	/* VSV */
	clear_bit(SLF_OUTWAIT, &sl->flags);	/* reset outfill flag */
}

/*
 * Called by the driver when there's room for more data.  If we have
 * more packets to send, we send them here.
 */
static void x25_asy_write_wakeup(struct tty_struct *tty)
{
	int actual;
	struct x25_asy *sl = (struct x25_asy *) tty->disc_data;

	/* First make sure we're connected. */
	if (!sl || sl->magic != X25_ASY_MAGIC || !netif_running(sl->dev))
		return;

	if (sl->xleft <= 0)  
	{
		/* Now serial buffer is almost free & we can start
		 * transmission of another packet */
		sl->tx_packets++;
		tty->flags &= ~(1 << TTY_DO_WRITE_WAKEUP);
		x25_asy_unlock(sl);
		return;
	}

	actual = tty->driver.write(tty, 0, sl->xhead, sl->xleft);
	sl->xleft -= actual;
	sl->xhead += actual;
}

static void x25_asy_timeout(struct net_device *dev)
{
	struct x25_asy *sl = (struct x25_asy*)(dev->priv);
	/* May be we must check transmitter timeout here ?
	 *      14 Oct 1994 Dmitry Gorodchanin.
	 */
	printk(KERN_WARNING "%s: transmit timed out, %s?\n", dev->name,
	       (sl->tty->driver.chars_in_buffer(sl->tty) || sl->xleft) ?
	       "bad line quality" : "driver error");
	sl->xleft = 0;
	sl->tty->flags &= ~(1 << TTY_DO_WRITE_WAKEUP);
	x25_asy_unlock(sl);
}

/* Encapsulate an IP datagram and kick it into a TTY queue. */

static int x25_asy_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct x25_asy *sl = (struct x25_asy*)(dev->priv);
	int err;

	if (!netif_running(sl->dev))
	{
		printk("%s: xmit call when iface is down\n", dev->name);
		return 1;
	}
	
	switch(skb->data[0])
	{
		case 0x00:break;
		case 0x01: /* Connection request .. do nothing */
			if((err=lapb_connect_request(sl))!=LAPB_OK)
				printk(KERN_ERR "x25_asy: lapb_connect_request error - %d\n", err);
			kfree_skb(skb);
			return 0;
		case 0x02: /* Disconnect request .. do nothing - hang up ?? */
			if((err=lapb_disconnect_request(sl))!=LAPB_OK)
				printk(KERN_ERR "x25_asy: lapb_disconnect_request error - %d\n", err);
		default:
			kfree_skb(skb);
			return  0;
	}
	skb_pull(skb,1);	/* Remove control byte */
	/*
	 * If we are busy already- too bad.  We ought to be able
	 * to queue things at this point, to allow for a little
	 * frame buffer.  Oh well...
	 * -----------------------------------------------------
	 * I hate queues in X.25 driver. May be it's efficient,
	 * but for me latency is more important. ;)
	 * So, no queues !
	 *        14 Oct 1994  Dmitry Gorodchanin.
	 */
	
	if((err=lapb_data_request(sl,skb))!=LAPB_OK)
	{
		printk(KERN_ERR "lapbeth: lapb_data_request error - %d\n", err);
		kfree_skb(skb);
		return 0;
	}
	return 0;
}


/*
 *	LAPB interface boilerplate
 */

/*
 *	Called when I frame data arrives. We did the work above - throw it
 *	at the net layer.
 */
  
static int x25_asy_data_indication(void *token, struct sk_buff *skb)
{
	return netif_rx(skb);
}

/*
 *	Data has emerged from the LAPB protocol machine. We don't handle
 *	busy cases too well. Its tricky to see how to do this nicely -
 *	perhaps lapb should allow us to bounce this ?
 */
 
static void x25_asy_data_transmit(void *token, struct sk_buff *skb)
{
	struct x25_asy *sl=token;
	if (netif_queue_stopped(sl->dev))
	{
		printk(KERN_ERR "x25_asy: tbusy drop\n");
		kfree_skb(skb);
		return;
	}
	/* We were not busy, so we are now... :-) */
	if (skb != NULL) 
	{
		x25_asy_lock(sl);
		sl->tx_bytes+=skb->len;
		x25_asy_encaps(sl, skb->data, skb->len);
		dev_kfree_skb(skb);
	}
}

/*
 *	LAPB connection establish/down information.
 */
 
static void x25_asy_connected(void *token, int reason)
{
	struct x25_asy *sl = token;
	struct sk_buff *skb;
	unsigned char *ptr;

	if ((skb = dev_alloc_skb(1)) == NULL) {
		printk(KERN_ERR "lapbeth: out of memory\n");
		return;
	}

	ptr  = skb_put(skb, 1);
	*ptr = 0x01;

	skb->dev      = sl->dev;
	skb->protocol = htons(ETH_P_X25);
	skb->mac.raw  = skb->data;
	skb->pkt_type = PACKET_HOST;

	netif_rx(skb);
}

static void x25_asy_disconnected(void *token, int reason)
{
	struct x25_asy *sl = token;
	struct sk_buff *skb;
	unsigned char *ptr;

	if ((skb = dev_alloc_skb(1)) == NULL) {
		printk(KERN_ERR "x25_asy: out of memory\n");
		return;
	}

	ptr  = skb_put(skb, 1);
	*ptr = 0x02;

	skb->dev      = sl->dev;
	skb->protocol = htons(ETH_P_X25);
	skb->mac.raw  = skb->data;
	skb->pkt_type = PACKET_HOST;

	netif_rx(skb);
}


/* Open the low-level part of the X.25 channel. Easy! */

static int x25_asy_open(struct net_device *dev)
{
	struct lapb_register_struct x25_asy_callbacks;
	struct x25_asy *sl = (struct x25_asy*)(dev->priv);
	unsigned long len;
	int err;

	if (sl->tty == NULL)
		return -ENODEV;

	/*
	 * Allocate the X.25 frame buffers:
	 *
	 * rbuff	Receive buffer.
	 * xbuff	Transmit buffer.
	 */

	len = dev->mtu * 2;

	sl->rbuff = (unsigned char *) kmalloc(len + 4, GFP_KERNEL);
	if (sl->rbuff == NULL)   {
		goto norbuff;
	}
	sl->xbuff = (unsigned char *) kmalloc(len + 4, GFP_KERNEL);
	if (sl->xbuff == NULL)   {
		goto noxbuff;
	}
	sl->mtu	     = dev->mtu;
	sl->buffsize = len;
	sl->rcount   = 0;
	sl->xleft    = 0;
	sl->flags   &= (1 << SLF_INUSE);      /* Clear ESCAPE & ERROR flags */

	netif_start_queue(dev);
			
	/*
	 *	Now attach LAPB
	 */
	 
	x25_asy_callbacks.connect_confirmation=x25_asy_connected;
	x25_asy_callbacks.connect_indication=x25_asy_connected;
	x25_asy_callbacks.disconnect_confirmation=x25_asy_disconnected;
	x25_asy_callbacks.disconnect_indication=x25_asy_disconnected;
	x25_asy_callbacks.data_indication=x25_asy_data_indication;
	x25_asy_callbacks.data_transmit=x25_asy_data_transmit;

	if((err=lapb_register(sl, &x25_asy_callbacks))==LAPB_OK)
		return 0;

	/* Cleanup */
	kfree(sl->xbuff);
noxbuff:
	kfree(sl->rbuff);
norbuff:
	return -ENOMEM;
}


/* Close the low-level part of the X.25 channel. Easy! */
static int x25_asy_close(struct net_device *dev)
{
	struct x25_asy *sl = (struct x25_asy*)(dev->priv);
	int err;

	if (sl->tty == NULL)
		return -EBUSY;

	sl->tty->flags &= ~(1 << TTY_DO_WRITE_WAKEUP);
	netif_stop_queue(dev);
	if((err=lapb_unregister(sl))!=LAPB_OK)
		printk(KERN_ERR "x25_asy_close: lapb_unregister error -%d\n",err);
	return 0;
}

static int x25_asy_receive_room(struct tty_struct *tty)
{
	return 65536;  /* We can handle an infinite amount of data. :-) */
}

/*
 * Handle the 'receiver data ready' interrupt.
 * This function is called by the 'tty_io' module in the kernel when
 * a block of X.25 data has been received, which can now be decapsulated
 * and sent on to some IP layer for further processing.
 */
 
static void x25_asy_receive_buf(struct tty_struct *tty, const unsigned char *cp, char *fp, int count)
{
	struct x25_asy *sl = (struct x25_asy *) tty->disc_data;

	if (!sl || sl->magic != X25_ASY_MAGIC || !netif_running(sl->dev))
		return;

	/*
	 * Argh! mtu change time! - costs us the packet part received
	 * at the change
	 */
	if (sl->mtu != sl->dev->mtu)  {

		x25_asy_changed_mtu(sl);
	}

	/* Read the characters out of the buffer */
	while (count--) {
		if (fp && *fp++) {
			if (!test_and_set_bit(SLF_ERROR, &sl->flags))  {
				sl->rx_errors++;
			}
			cp++;
			continue;
		}
		x25_asy_unesc(sl, *cp++);
	}
}

/*
 * Open the high-level part of the X.25 channel.
 * This function is called by the TTY module when the
 * X.25 line discipline is called for.  Because we are
 * sure the tty line exists, we only have to link it to
 * a free X.25 channel...
 */

static int x25_asy_open_tty(struct tty_struct *tty)
{
	struct x25_asy *sl = (struct x25_asy *) tty->disc_data;
	int err;

	/* First make sure we're not already connected. */
	if (sl && sl->magic == X25_ASY_MAGIC) {
		return -EEXIST;
	}

	/* OK.  Find a free X.25 channel to use. */
	if ((sl = x25_asy_alloc()) == NULL) {
		return -ENFILE;
	}

	sl->tty = tty;
	tty->disc_data = sl;
	if (tty->driver.flush_buffer)  {
		tty->driver.flush_buffer(tty);
	}
	if (tty->ldisc.flush_buffer)  {
		tty->ldisc.flush_buffer(tty);
	}

	/* Restore default settings */
	sl->dev->type = ARPHRD_X25;
	
	/* Perform the low-level X.25 async init */
	if ((err = x25_asy_open(sl->dev)))
		return err;

	MOD_INC_USE_COUNT;

	/* Done.  We have linked the TTY line to a channel. */
	return sl->dev->base_addr;
}


/*
 * Close down an X.25 channel.
 * This means flushing out any pending queues, and then restoring the
 * TTY line discipline to what it was before it got hooked to X.25
 * (which usually is TTY again).
 */
static void x25_asy_close_tty(struct tty_struct *tty)
{
	struct x25_asy *sl = (struct x25_asy *) tty->disc_data;

	/* First make sure we're connected. */
	if (!sl || sl->magic != X25_ASY_MAGIC)
		return;

	if (sl->dev->flags & IFF_UP)
	{
		(void) dev_close(sl->dev);
	}

	tty->disc_data = 0;
	sl->tty = NULL;
	x25_asy_free(sl);
	unregister_netdev(sl->dev);
	MOD_DEC_USE_COUNT;
}


static struct net_device_stats *x25_asy_get_stats(struct net_device *dev)
{
	static struct net_device_stats stats;
	struct x25_asy *sl = (struct x25_asy*)(dev->priv);

	memset(&stats, 0, sizeof(struct net_device_stats));

	stats.rx_packets     = sl->rx_packets;
	stats.tx_packets     = sl->tx_packets;
	stats.rx_bytes	     = sl->rx_bytes;
	stats.tx_bytes	     = sl->tx_bytes;
	stats.rx_dropped     = sl->rx_dropped;
	stats.tx_dropped     = sl->tx_dropped;
	stats.tx_errors      = sl->tx_errors;
	stats.rx_errors      = sl->rx_errors;
	stats.rx_over_errors = sl->rx_over_errors;
	return (&stats);
}


 /************************************************************************
  *			STANDARD X.25 ENCAPSULATION		  	 *
  ************************************************************************/

int x25_asy_esc(unsigned char *s, unsigned char *d, int len)
{
	unsigned char *ptr = d;
	unsigned char c;

	/*
	 * Send an initial END character to flush out any
	 * data that may have accumulated in the receiver
	 * due to line noise.
	 */

	*ptr++ = X25_END;	/* Send 10111110 bit seq */

	/*
	 * For each byte in the packet, send the appropriate
	 * character sequence, according to the X.25 protocol.
	 */

	while (len-- > 0) 
	{
		switch(c = *s++) 
		{
			case X25_END:
				*ptr++ = X25_ESC;
				*ptr++ = X25_ESCAPE(X25_END);
				break;
			case X25_ESC:
				*ptr++ = X25_ESC;
				*ptr++ = X25_ESCAPE(X25_ESC);
				break;
			 default:
				*ptr++ = c;
				break;
		}
	}
	*ptr++ = X25_END;
	return (ptr - d);
}

static void x25_asy_unesc(struct x25_asy *sl, unsigned char s)
{

	switch(s) 
	{
		case X25_END:
			if (!test_and_clear_bit(SLF_ERROR, &sl->flags) && (sl->rcount > 2))  
			{
				x25_asy_bump(sl);
			}
			clear_bit(SLF_ESCAPE, &sl->flags);
			sl->rcount = 0;
			return;

		case X25_ESC:
			set_bit(SLF_ESCAPE, &sl->flags);
			return;
			
		case X25_ESCAPE(X25_ESC):
		case X25_ESCAPE(X25_END):
			if (test_and_clear_bit(SLF_ESCAPE, &sl->flags))
				s = X25_UNESCAPE(s);
			break;
	}
	if (!test_bit(SLF_ERROR, &sl->flags))  
	{
		if (sl->rcount < sl->buffsize)  
		{
			sl->rbuff[sl->rcount++] = s;
			return;
		}
		sl->rx_over_errors++;
		set_bit(SLF_ERROR, &sl->flags);
	}
}


/* Perform I/O control on an active X.25 channel. */
static int x25_asy_ioctl(struct tty_struct *tty, void *file, int cmd, void *arg)
{
	struct x25_asy *sl = (struct x25_asy *) tty->disc_data;

	/* First make sure we're connected. */
	if (!sl || sl->magic != X25_ASY_MAGIC) {
		return -EINVAL;
	}

	switch(cmd) 
	{
		case SIOCGIFNAME:
			if(copy_to_user(arg, sl->dev->name, strlen(sl->dev->name) + 1))
				return -EFAULT;
			return 0;

		case SIOCSIFHWADDR:
			return -EINVAL;

		/* Allow stty to read, but not set, the serial port */
		case TCGETS:
		case TCGETA:
			return n_tty_ioctl(tty, (struct file *) file, cmd, (unsigned long) arg);

		default:
			return -ENOIOCTLCMD;
	}
}

static int x25_asy_open_dev(struct net_device *dev)
{
	struct x25_asy *sl = (struct x25_asy*)(dev->priv);
	if(sl->tty==NULL)
		return -ENODEV;
	return 0;
}

/* Initialize X.25 control device -- register X.25 line discipline */

int __init x25_asy_init_ctrl_dev(void)
{
	int status;

	if (x25_asy_maxdev < 4) x25_asy_maxdev = 4; /* Sanity */

	printk(KERN_INFO "X.25 async: version 0.00 ALPHA (dynamic channels, max=%d).\n",
		x25_asy_maxdev );
	x25_asy_ctrls = (x25_asy_ctrl_t **) kmalloc(sizeof(void*)*x25_asy_maxdev, GFP_KERNEL);
	if (x25_asy_ctrls == NULL)
	{
		printk("X25 async: Can't allocate x25_asy_ctrls[] array!  Uaargh! (-> No X.25 available)\n");
		return -ENOMEM;
	}

	/* Clear the pointer array, we allocate devices when we need them */
	memset(x25_asy_ctrls, 0, sizeof(void*)*x25_asy_maxdev); /* Pointers */

	/* Fill in our line protocol discipline, and register it */
	memset(&x25_ldisc, 0, sizeof(x25_ldisc));
	x25_ldisc.magic  = TTY_LDISC_MAGIC;
	x25_ldisc.name   = "X.25";
	x25_ldisc.flags  = 0;
	x25_ldisc.open   = x25_asy_open_tty;
	x25_ldisc.close  = x25_asy_close_tty;
	x25_ldisc.read   = NULL;
	x25_ldisc.write  = NULL;
	x25_ldisc.ioctl  = (int (*)(struct tty_struct *, struct file *,
				   unsigned int, unsigned long)) x25_asy_ioctl;
	x25_ldisc.poll   = NULL;
	x25_ldisc.receive_buf = x25_asy_receive_buf;
	x25_ldisc.receive_room = x25_asy_receive_room;
	x25_ldisc.write_wakeup = x25_asy_write_wakeup;
	if ((status = tty_register_ldisc(N_X25, &x25_ldisc)) != 0)  {
		printk("X.25 async: can't register line discipline (err = %d)\n", status);
	}

	return status;
}


/* Initialise the X.25 driver.  Called by the device init code */

int x25_asy_init(struct net_device *dev)
{
	struct x25_asy *sl = (struct x25_asy*)(dev->priv);

	if (sl == NULL)		/* Allocation failed ?? */
		return -ENODEV;

	/* Set up the control block. (And clear statistics) */

	memset(sl, 0, sizeof (struct x25_asy));
	sl->magic  = X25_ASY_MAGIC;
	sl->dev	   = dev;

	/*
	 *	Finish setting up the DEVICE info. 
	 */
	 
	dev->mtu		= SL_MTU;
	dev->hard_start_xmit	= x25_asy_xmit;
	dev->tx_timeout		= x25_asy_timeout;
	dev->watchdog_timeo	= HZ*20;
	dev->open		= x25_asy_open_dev;
	dev->stop		= x25_asy_close;
	dev->get_stats	        = x25_asy_get_stats;
	dev->hard_header_len	= 0;
	dev->addr_len		= 0;
	dev->type		= ARPHRD_X25;
	dev->tx_queue_len	= 10;

	/* New-style flags. */
	dev->flags		= IFF_NOARP;

	return 0;
}
#ifdef MODULE

int
init_module(void)
{
	return x25_asy_init_ctrl_dev();
}

void
cleanup_module(void)
{
	int i;

	if (x25_asy_ctrls != NULL)
	{
		for (i = 0; i < x25_asy_maxdev; i++)
		{
			if (x25_asy_ctrls[i])
			{
				/*
				 * VSV = if dev->start==0, then device
				 * unregistered while close proc.
				 */
				if (netif_running(&(x25_asy_ctrls[i]->dev)))
					unregister_netdev(&(x25_asy_ctrls[i]->dev));

				kfree(x25_asy_ctrls[i]);
				x25_asy_ctrls[i] = NULL;
			}
		}
		kfree(x25_asy_ctrls);
		x25_asy_ctrls = NULL;
	}
	if ((i = tty_register_ldisc(N_X25, NULL)))
	{
		printk("X.25 async: can't unregister line discipline (err = %d)\n", i);
	}
}
#endif /* MODULE */

