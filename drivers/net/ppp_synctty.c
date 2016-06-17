/*
 * PPP synchronous tty channel driver for Linux.
 *
 * This is a ppp channel driver that can be used with tty device drivers
 * that are frame oriented, such as synchronous HDLC devices.
 *
 * Complete PPP frames without encoding/decoding are exchanged between
 * the channel driver and the device driver.
 * 
 * The async map IOCTL codes are implemented to keep the user mode
 * applications happy if they call them. Synchronous PPP does not use
 * the async maps.
 *
 * Copyright 1999 Paul Mackerras.
 *
 * Also touched by the grubby hands of Paul Fulghum paulkf@microgate.com
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 * This driver provides the encapsulation and framing for sending
 * and receiving PPP frames over sync serial lines.  It relies on
 * the generic PPP layer to give it frames to send and to process
 * received frames.  It implements the PPP line discipline.
 *
 * Part of the code in this driver was inspired by the old async-only
 * PPP driver, written by Michael Callahan and Al Longyear, and
 * subsequently hacked by Paul Mackerras.
 *
 * ==FILEVERSION 20020125==
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/tty.h>
#include <linux/netdevice.h>
#include <linux/poll.h>
#include <linux/ppp_defs.h>
#include <linux/if_ppp.h>
#include <linux/ppp_channel.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>

#define PPP_VERSION	"2.4.2"

/* Structure for storing local state. */
struct syncppp {
	struct tty_struct *tty;
	unsigned int	flags;
	unsigned int	rbits;
	int		mru;
	spinlock_t	xmit_lock;
	spinlock_t	recv_lock;
	unsigned long	xmit_flags;
	u32		xaccm[8];
	u32		raccm;
	unsigned int	bytes_sent;
	unsigned int	bytes_rcvd;

	struct sk_buff	*tpkt;
	unsigned long	last_xmit;

	struct sk_buff	*rpkt;

	atomic_t	refcnt;
	struct semaphore dead_sem;
	struct ppp_channel chan;	/* interface to generic ppp layer */
};

/* Bit numbers in xmit_flags */
#define XMIT_WAKEUP	0
#define XMIT_FULL	1

/* Bits in rbits */
#define SC_RCV_BITS	(SC_RCV_B7_1|SC_RCV_B7_0|SC_RCV_ODDP|SC_RCV_EVNP)

#define PPPSYNC_MAX_RQLEN	32	/* arbitrary */

/*
 * Prototypes.
 */
static struct sk_buff* ppp_sync_txmunge(struct syncppp *ap, struct sk_buff *);
static int ppp_sync_send(struct ppp_channel *chan, struct sk_buff *skb);
static int ppp_sync_ioctl(struct ppp_channel *chan, unsigned int cmd,
			  unsigned long arg);
static int ppp_sync_push(struct syncppp *ap);
static void ppp_sync_flush_output(struct syncppp *ap);
static void ppp_sync_input(struct syncppp *ap, const unsigned char *buf,
			   char *flags, int count);

static struct ppp_channel_ops sync_ops = {
	ppp_sync_send,
	ppp_sync_ioctl
};

/*
 * Utility procedures to print a buffer in hex/ascii
 */
static void
ppp_print_hex (register __u8 * out, const __u8 * in, int count)
{
	register __u8 next_ch;
	static char hex[] = "0123456789ABCDEF";

	while (count-- > 0) {
		next_ch = *in++;
		*out++ = hex[(next_ch >> 4) & 0x0F];
		*out++ = hex[next_ch & 0x0F];
		++out;
	}
}

static void
ppp_print_char (register __u8 * out, const __u8 * in, int count)
{
	register __u8 next_ch;

	while (count-- > 0) {
		next_ch = *in++;

		if (next_ch < 0x20 || next_ch > 0x7e)
			*out++ = '.';
		else {
			*out++ = next_ch;
			if (next_ch == '%')   /* printk/syslogd has a bug !! */
				*out++ = '%';
		}
	}
	*out = '\0';
}

static void
ppp_print_buffer (const char *name, const __u8 *buf, int count)
{
	__u8 line[44];

	if (name != NULL)
		printk(KERN_DEBUG "ppp_synctty: %s, count = %d\n", name, count);

	while (count > 8) {
		memset (line, 32, 44);
		ppp_print_hex (line, buf, 8);
		ppp_print_char (&line[8 * 3], buf, 8);
		printk(KERN_DEBUG "%s\n", line);
		count -= 8;
		buf += 8;
	}

	if (count > 0) {
		memset (line, 32, 44);
		ppp_print_hex (line, buf, count);
		ppp_print_char (&line[8 * 3], buf, count);
		printk(KERN_DEBUG "%s\n", line);
	}
}


/*
 * Routines implementing the synchronous PPP line discipline.
 */

/*
 * We have a potential race on dereferencing tty->disc_data,
 * because the tty layer provides no locking at all - thus one
 * cpu could be running ppp_synctty_receive while another
 * calls ppp_synctty_close, which zeroes tty->disc_data and
 * frees the memory that ppp_synctty_receive is using.  The best
 * way to fix this is to use a rwlock in the tty struct, but for now
 * we use a single global rwlock for all ttys in ppp line discipline.
 */
static rwlock_t disc_data_lock = RW_LOCK_UNLOCKED;

static struct syncppp *sp_get(struct tty_struct *tty)
{
	struct syncppp *ap;

	read_lock(&disc_data_lock);
	ap = tty->disc_data;
	if (ap != NULL)
		atomic_inc(&ap->refcnt);
	read_unlock(&disc_data_lock);
	return ap;
}

static void sp_put(struct syncppp *ap)
{
	if (atomic_dec_and_test(&ap->refcnt))
		up(&ap->dead_sem);
}

/*
 * Called when a tty is put into sync-PPP line discipline.
 */
static int
ppp_sync_open(struct tty_struct *tty)
{
	struct syncppp *ap;
	int err;

	MOD_INC_USE_COUNT;
	ap = kmalloc(sizeof(*ap), GFP_KERNEL);
	err = -ENOMEM;
	if (ap == 0)
		goto out;

	/* initialize the syncppp structure */
	memset(ap, 0, sizeof(*ap));
	ap->tty = tty;
	ap->mru = PPP_MRU;
	spin_lock_init(&ap->xmit_lock);
	spin_lock_init(&ap->recv_lock);
	ap->xaccm[0] = ~0U;
	ap->xaccm[3] = 0x60000000U;
	ap->raccm = ~0U;

	atomic_set(&ap->refcnt, 1);
	init_MUTEX_LOCKED(&ap->dead_sem);

	ap->chan.private = ap;
	ap->chan.ops = &sync_ops;
	ap->chan.mtu = PPP_MRU;
	ap->chan.hdrlen = 2;	/* for A/C bytes */
	err = ppp_register_channel(&ap->chan);
	if (err)
		goto out_free;

	tty->disc_data = ap;

	return 0;

 out_free:
	kfree(ap);
 out:
	MOD_DEC_USE_COUNT;
	return err;
}

/*
 * Called when the tty is put into another line discipline
 * or it hangs up.  We have to wait for any cpu currently
 * executing in any of the other ppp_synctty_* routines to
 * finish before we can call ppp_unregister_channel and free
 * the syncppp struct.  This routine must be called from
 * process context, not interrupt or softirq context.
 */
static void
ppp_sync_close(struct tty_struct *tty)
{
	struct syncppp *ap;

	write_lock(&disc_data_lock);
	ap = tty->disc_data;
	tty->disc_data = 0;
	write_unlock(&disc_data_lock);
	if (ap == 0)
		return;

	/*
	 * We have now ensured that nobody can start using ap from now
	 * on, but we have to wait for all existing users to finish.
	 * Note that ppp_unregister_channel ensures that no calls to
	 * our channel ops (i.e. ppp_sync_send/ioctl) are in progress
	 * by the time it returns.
	 */
	if (!atomic_dec_and_test(&ap->refcnt))
		down(&ap->dead_sem);

	ppp_unregister_channel(&ap->chan);
	if (ap->rpkt != 0)
		kfree_skb(ap->rpkt);
	if (ap->tpkt != 0)
		kfree_skb(ap->tpkt);
	kfree(ap);
	MOD_DEC_USE_COUNT;
}

/*
 * Read does nothing - no data is ever available this way.
 * Pppd reads and writes packets via /dev/ppp instead.
 */
static ssize_t
ppp_sync_read(struct tty_struct *tty, struct file *file,
	       unsigned char *buf, size_t count)
{
	return -EAGAIN;
}

/*
 * Write on the tty does nothing, the packets all come in
 * from the ppp generic stuff.
 */
static ssize_t
ppp_sync_write(struct tty_struct *tty, struct file *file,
		const unsigned char *buf, size_t count)
{
	return -EAGAIN;
}

static int
ppp_synctty_ioctl(struct tty_struct *tty, struct file *file,
		  unsigned int cmd, unsigned long arg)
{
	struct syncppp *ap = sp_get(tty);
	int err, val;

	if (ap == 0)
		return -ENXIO;
	err = -EFAULT;
	switch (cmd) {
	case PPPIOCGCHAN:
		err = -ENXIO;
		if (ap == 0)
			break;
		err = -EFAULT;
		if (put_user(ppp_channel_index(&ap->chan), (int *) arg))
			break;
		err = 0;
		break;

	case PPPIOCGUNIT:
		err = -ENXIO;
		if (ap == 0)
			break;
		err = -EFAULT;
		if (put_user(ppp_unit_number(&ap->chan), (int *) arg))
			break;
		err = 0;
		break;

	case TCGETS:
	case TCGETA:
		err = n_tty_ioctl(tty, file, cmd, arg);
		break;

	case TCFLSH:
		/* flush our buffers and the serial port's buffer */
		if (arg == TCIOFLUSH || arg == TCOFLUSH)
			ppp_sync_flush_output(ap);
		err = n_tty_ioctl(tty, file, cmd, arg);
		break;

	case FIONREAD:
		val = 0;
		if (put_user(val, (int *) arg))
			break;
		err = 0;
		break;

	default:
		err = -ENOIOCTLCMD;
	}

	sp_put(ap);
	return err;
}

/* No kernel lock - fine */
static unsigned int
ppp_sync_poll(struct tty_struct *tty, struct file *file, poll_table *wait)
{
	return 0;
}

static int
ppp_sync_room(struct tty_struct *tty)
{
	return 65535;
}

static void
ppp_sync_receive(struct tty_struct *tty, const unsigned char *buf,
		  char *flags, int count)
{
	struct syncppp *ap = sp_get(tty);

	if (ap == 0)
		return;
	spin_lock_bh(&ap->recv_lock);
	ppp_sync_input(ap, buf, flags, count);
	spin_unlock_bh(&ap->recv_lock);
	sp_put(ap);
	if (test_and_clear_bit(TTY_THROTTLED, &tty->flags)
	    && tty->driver.unthrottle)
		tty->driver.unthrottle(tty);
}

static void
ppp_sync_wakeup(struct tty_struct *tty)
{
	struct syncppp *ap = sp_get(tty);

	clear_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);
	if (ap == 0)
		return;
	if (ppp_sync_push(ap))
		ppp_output_wakeup(&ap->chan);
	sp_put(ap);
}


static struct tty_ldisc ppp_sync_ldisc = {
	magic:	TTY_LDISC_MAGIC,
	name:	"pppsync",
	open:	ppp_sync_open,
	close:	ppp_sync_close,
	read:	ppp_sync_read,
	write:	ppp_sync_write,
	ioctl:	ppp_synctty_ioctl,
	poll:	ppp_sync_poll,
	receive_room: ppp_sync_room,
	receive_buf: ppp_sync_receive,
	write_wakeup: ppp_sync_wakeup,
};

static int __init
ppp_sync_init(void)
{
	int err;

	err = tty_register_ldisc(N_SYNC_PPP, &ppp_sync_ldisc);
	if (err != 0)
		printk(KERN_ERR "PPP_sync: error %d registering line disc.\n",
		       err);
	return err;
}

/*
 * The following routines provide the PPP channel interface.
 */
static int
ppp_sync_ioctl(struct ppp_channel *chan, unsigned int cmd, unsigned long arg)
{
	struct syncppp *ap = chan->private;
	int err, val;
	u32 accm[8];

	err = -EFAULT;
	switch (cmd) {
	case PPPIOCGFLAGS:
		val = ap->flags | ap->rbits;
		if (put_user(val, (int *) arg))
			break;
		err = 0;
		break;
	case PPPIOCSFLAGS:
		if (get_user(val, (int *) arg))
			break;
		ap->flags = val & ~SC_RCV_BITS;
		spin_lock_bh(&ap->recv_lock);
		ap->rbits = val & SC_RCV_BITS;
		spin_unlock_bh(&ap->recv_lock);
		err = 0;
		break;

	case PPPIOCGASYNCMAP:
		if (put_user(ap->xaccm[0], (u32 *) arg))
			break;
		err = 0;
		break;
	case PPPIOCSASYNCMAP:
		if (get_user(ap->xaccm[0], (u32 *) arg))
			break;
		err = 0;
		break;

	case PPPIOCGRASYNCMAP:
		if (put_user(ap->raccm, (u32 *) arg))
			break;
		err = 0;
		break;
	case PPPIOCSRASYNCMAP:
		if (get_user(ap->raccm, (u32 *) arg))
			break;
		err = 0;
		break;

	case PPPIOCGXASYNCMAP:
		if (copy_to_user((void *) arg, ap->xaccm, sizeof(ap->xaccm)))
			break;
		err = 0;
		break;
	case PPPIOCSXASYNCMAP:
		if (copy_from_user(accm, (void *) arg, sizeof(accm)))
			break;
		accm[2] &= ~0x40000000U;	/* can't escape 0x5e */
		accm[3] |= 0x60000000U;		/* must escape 0x7d, 0x7e */
		memcpy(ap->xaccm, accm, sizeof(ap->xaccm));
		err = 0;
		break;

	case PPPIOCGMRU:
		if (put_user(ap->mru, (int *) arg))
			break;
		err = 0;
		break;
	case PPPIOCSMRU:
		if (get_user(val, (int *) arg))
			break;
		if (val < PPP_MRU)
			val = PPP_MRU;
		ap->mru = val;
		err = 0;
		break;

	default:
		err = -ENOTTY;
	}
	return err;
}

/*
 * Procedures for encapsulation and framing.
 */

struct sk_buff*
ppp_sync_txmunge(struct syncppp *ap, struct sk_buff *skb)
{
	int proto;
	unsigned char *data;
	int islcp;

	data  = skb->data;
	proto = (data[0] << 8) + data[1];

	/* LCP packets with codes between 1 (configure-request)
	 * and 7 (code-reject) must be sent as though no options
	 * have been negotiated.
	 */
	islcp = proto == PPP_LCP && 1 <= data[2] && data[2] <= 7;

	/* compress protocol field if option enabled */
	if (data[0] == 0 && (ap->flags & SC_COMP_PROT) && !islcp)
		skb_pull(skb,1);

	/* prepend address/control fields if necessary */
	if ((ap->flags & SC_COMP_AC) == 0 || islcp) {
		if (skb_headroom(skb) < 2) {
			struct sk_buff *npkt = dev_alloc_skb(skb->len + 2);
			if (npkt == NULL) {
				kfree_skb(skb);
				return NULL;
			}
			skb_reserve(npkt,2);
			memcpy(skb_put(npkt,skb->len), skb->data, skb->len);
			kfree_skb(skb);
			skb = npkt;
		}
		skb_push(skb,2);
		skb->data[0] = PPP_ALLSTATIONS;
		skb->data[1] = PPP_UI;
	}

	ap->last_xmit = jiffies;

	if (skb && ap->flags & SC_LOG_OUTPKT)
		ppp_print_buffer ("send buffer", skb->data, skb->len);

	return skb;
}

/*
 * Transmit-side routines.
 */

/*
 * Send a packet to the peer over an sync tty line.
 * Returns 1 iff the packet was accepted.
 * If the packet was not accepted, we will call ppp_output_wakeup
 * at some later time.
 */
static int
ppp_sync_send(struct ppp_channel *chan, struct sk_buff *skb)
{
	struct syncppp *ap = chan->private;

	ppp_sync_push(ap);

	if (test_and_set_bit(XMIT_FULL, &ap->xmit_flags))
		return 0;	/* already full */
	skb = ppp_sync_txmunge(ap, skb);
	if (skb != NULL)
		ap->tpkt = skb;
	else
		clear_bit(XMIT_FULL, &ap->xmit_flags);

	ppp_sync_push(ap);
	return 1;
}

/*
 * Push as much data as possible out to the tty.
 */
static int
ppp_sync_push(struct syncppp *ap)
{
	int sent, done = 0;
	struct tty_struct *tty = ap->tty;
	int tty_stuffed = 0;

	set_bit(XMIT_WAKEUP, &ap->xmit_flags);
	if (!spin_trylock_bh(&ap->xmit_lock))
		return 0;
	for (;;) {
		if (test_and_clear_bit(XMIT_WAKEUP, &ap->xmit_flags))
			tty_stuffed = 0;
		if (!tty_stuffed && ap->tpkt != 0) {
			set_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);
			sent = tty->driver.write(tty, 0, ap->tpkt->data, ap->tpkt->len);
			if (sent < 0)
				goto flush;	/* error, e.g. loss of CD */
			if (sent < ap->tpkt->len) {
				tty_stuffed = 1;
			} else {
				kfree_skb(ap->tpkt);
				ap->tpkt = 0;
				clear_bit(XMIT_FULL, &ap->xmit_flags);
				done = 1;
			}
			continue;
		}
		/* haven't made any progress */
		spin_unlock_bh(&ap->xmit_lock);
		if (!(test_bit(XMIT_WAKEUP, &ap->xmit_flags)
		      || (!tty_stuffed && ap->tpkt != 0)))
			break;
		if (!spin_trylock_bh(&ap->xmit_lock))
			break;
	}
	return done;

flush:
	if (ap->tpkt != 0) {
		kfree_skb(ap->tpkt);
		ap->tpkt = 0;
		clear_bit(XMIT_FULL, &ap->xmit_flags);
		done = 1;
	}
	spin_unlock_bh(&ap->xmit_lock);
	return done;
}

/*
 * Flush output from our internal buffers.
 * Called for the TCFLSH ioctl.
 */
static void
ppp_sync_flush_output(struct syncppp *ap)
{
	int done = 0;

	spin_lock_bh(&ap->xmit_lock);
	if (ap->tpkt != NULL) {
		kfree_skb(ap->tpkt);
		ap->tpkt = 0;
		clear_bit(XMIT_FULL, &ap->xmit_flags);
		done = 1;
	}
	spin_unlock_bh(&ap->xmit_lock);
	if (done)
		ppp_output_wakeup(&ap->chan);
}

/*
 * Receive-side routines.
 */

static inline void
process_input_packet(struct syncppp *ap)
{
	struct sk_buff *skb;
	unsigned char *p;
	int code = 0;

	skb = ap->rpkt;
	ap->rpkt = 0;

	/* strip address/control field if present */
	p = skb->data;
	if (p[0] == PPP_ALLSTATIONS && p[1] == PPP_UI) {
		/* chop off address/control */
		if (skb->len < 3)
			goto err;
		p = skb_pull(skb, 2);
	}

	/* decompress protocol field if compressed */
	if (p[0] & 1) {
		/* protocol is compressed */
		skb_push(skb, 1)[0] = 0;
	} else if (skb->len < 2)
		goto err;

	/* pass to generic layer */
	ppp_input(&ap->chan, skb);
	return;

 err:
	kfree_skb(skb);
	ppp_input_error(&ap->chan, code);
}

/* called when the tty driver has data for us. 
 *
 * Data is frame oriented: each call to ppp_sync_input is considered
 * a whole frame. If the 1st flag byte is non-zero then the whole
 * frame is considered to be in error and is tossed.
 */
static void
ppp_sync_input(struct syncppp *ap, const unsigned char *buf,
		char *flags, int count)
{
	struct sk_buff *skb;
	unsigned char *sp;

	if (count == 0)
		return;

	/* if flag set, then error, ignore frame */
	if (flags != 0 && *flags) {
		ppp_input_error(&ap->chan, *flags);
		return;
	}

	if (ap->flags & SC_LOG_INPKT)
		ppp_print_buffer ("receive buffer", buf, count);

	/* stuff the chars in the skb */
	if ((skb = ap->rpkt) == 0) {
		if ((skb = dev_alloc_skb(ap->mru + PPP_HDRLEN + 2)) == 0) {
			printk(KERN_ERR "PPPsync: no memory (input pkt)\n");
			ppp_input_error(&ap->chan, 0);
			return;
		}
		/* Try to get the payload 4-byte aligned */
		if (buf[0] != PPP_ALLSTATIONS)
			skb_reserve(skb, 2 + (buf[0] & 1));
		ap->rpkt = skb;
	}
	if (count > skb_tailroom(skb)) {
		/* packet overflowed MRU */
		ppp_input_error(&ap->chan, 1);
	} else {
		sp = skb_put(skb, count);
		memcpy(sp, buf, count);
		process_input_packet(ap);
	}
}

static void __exit
ppp_sync_cleanup(void)
{
	if (tty_register_ldisc(N_SYNC_PPP, NULL) != 0)
		printk(KERN_ERR "failed to unregister Sync PPP line discipline\n");
}

module_init(ppp_sync_init);
module_exit(ppp_sync_cleanup);
MODULE_LICENSE("GPL");
