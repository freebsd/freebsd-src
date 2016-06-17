/* 
   RFCOMM implementation for Linux Bluetooth stack (BlueZ).
   Copyright (C) 2002 Maxim Krasnyansky <maxk@qualcomm.com>
   Copyright (C) 2002 Marcel Holtmann <marcel@holtmann.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation;

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
   IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) AND AUTHOR(S) BE LIABLE FOR ANY
   CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES 
   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN 
   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF 
   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

   ALL LIABILITY, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PATENTS, 
   COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS, RELATING TO USE OF THIS 
   SOFTWARE IS DISCLAIMED.
*/

/*
 * RFCOMM TTY.
 *
 * $Id: tty.c,v 1.26 2002/10/18 20:12:12 maxk Exp $
 */

#include <linux/config.h>
#include <linux/module.h>

#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>

#include <linux/slab.h>
#include <linux/skbuff.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/rfcomm.h>

#ifndef CONFIG_BLUEZ_RFCOMM_DEBUG
#undef  BT_DBG
#define BT_DBG(D...)
#endif

#define RFCOMM_TTY_MAGIC 0x6d02		/* magic number for rfcomm struct */
#define RFCOMM_TTY_PORTS RFCOMM_MAX_DEV	/* whole lotta rfcomm devices */
#define RFCOMM_TTY_MAJOR 216		/* device node major id of the usb/bluetooth.c driver */
#define RFCOMM_TTY_MINOR 0

struct rfcomm_dev {
	struct list_head	list;
	atomic_t		refcnt;

	char			name[12];
	int			id;
	unsigned long		flags;
	int			opened;
	int			err;

	bdaddr_t		src;
	bdaddr_t		dst;
	u8 			channel;

	uint 			modem_status;

	struct rfcomm_dlc	*dlc;
	struct tty_struct	*tty;
	wait_queue_head_t       wait;
	struct tasklet_struct   wakeup_task;

	atomic_t 		wmem_alloc;
};

static LIST_HEAD(rfcomm_dev_list);
static rwlock_t rfcomm_dev_lock = RW_LOCK_UNLOCKED;

static void rfcomm_dev_data_ready(struct rfcomm_dlc *dlc, struct sk_buff *skb);
static void rfcomm_dev_state_change(struct rfcomm_dlc *dlc, int err);
static void rfcomm_dev_modem_status(struct rfcomm_dlc *dlc, u8 v24_sig);

static void rfcomm_tty_wakeup(unsigned long arg);

/* ---- Device functions ---- */
static void rfcomm_dev_destruct(struct rfcomm_dev *dev)
{
	struct rfcomm_dlc *dlc = dev->dlc;

	BT_DBG("dev %p dlc %p", dev, dlc);

	rfcomm_dlc_lock(dlc);
	/* Detach DLC if it's owned by this dev */
	if (dlc->owner == dev)
		dlc->owner = NULL;
	rfcomm_dlc_unlock(dlc);

	rfcomm_dlc_put(dlc);
	kfree(dev);

	MOD_DEC_USE_COUNT;
}

static inline void rfcomm_dev_hold(struct rfcomm_dev *dev)
{
	atomic_inc(&dev->refcnt);
}

static inline void rfcomm_dev_put(struct rfcomm_dev *dev)
{
	/* The reason this isn't actually a race, as you no
	   doubt have a little voice screaming at you in your
	   head, is that the refcount should never actually
	   reach zero unless the device has already been taken
	   off the list, in rfcomm_dev_del(). And if that's not
	   true, we'll hit the BUG() in rfcomm_dev_destruct()
	   anyway. */
	if (atomic_dec_and_test(&dev->refcnt))
		rfcomm_dev_destruct(dev);
}

static struct rfcomm_dev *__rfcomm_dev_get(int id)
{
	struct rfcomm_dev *dev;
	struct list_head  *p;

	list_for_each(p, &rfcomm_dev_list) {
		dev = list_entry(p, struct rfcomm_dev, list);
		if (dev->id == id)
			return dev;
	}

	return NULL;
}

static inline struct rfcomm_dev *rfcomm_dev_get(int id)
{
	struct rfcomm_dev *dev;

	read_lock(&rfcomm_dev_lock);

	dev = __rfcomm_dev_get(id);
	if (dev)
		rfcomm_dev_hold(dev);

	read_unlock(&rfcomm_dev_lock);

	return dev;
}

static int rfcomm_dev_add(struct rfcomm_dev_req *req, struct rfcomm_dlc *dlc)
{
	struct rfcomm_dev *dev;
	struct list_head *head = &rfcomm_dev_list, *p;
	int err = 0;

	BT_DBG("id %d channel %d", req->dev_id, req->channel);
	
	dev = kmalloc(sizeof(struct rfcomm_dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	memset(dev, 0, sizeof(struct rfcomm_dev));

	write_lock_bh(&rfcomm_dev_lock);

	if (req->dev_id < 0) {
		dev->id = 0;

		list_for_each(p, &rfcomm_dev_list) {
			if (list_entry(p, struct rfcomm_dev, list)->id != dev->id)
				break;

			dev->id++;
			head = p;
		}
	} else {
		dev->id = req->dev_id;

		list_for_each(p, &rfcomm_dev_list) {
			struct rfcomm_dev *entry = list_entry(p, struct rfcomm_dev, list);

			if (entry->id == dev->id) {
				err = -EADDRINUSE;
				goto out;
			}

			if (entry->id > dev->id - 1)
				break;

			head = p;
		}
	}

	if ((dev->id < 0) || (dev->id > RFCOMM_MAX_DEV - 1)) {
		err = -ENFILE;
		goto out;
	}

	sprintf(dev->name, "rfcomm%d", dev->id);

	list_add(&dev->list, head);
	atomic_set(&dev->refcnt, 1);

	bacpy(&dev->src, &req->src);
	bacpy(&dev->dst, &req->dst);
	dev->channel = req->channel;

	dev->flags = req->flags & 
		((1 << RFCOMM_RELEASE_ONHUP) | (1 << RFCOMM_REUSE_DLC));

	init_waitqueue_head(&dev->wait);
	tasklet_init(&dev->wakeup_task, rfcomm_tty_wakeup, (unsigned long) dev);

	rfcomm_dlc_lock(dlc);
	dlc->data_ready   = rfcomm_dev_data_ready;
	dlc->state_change = rfcomm_dev_state_change;
	dlc->modem_status = rfcomm_dev_modem_status;

	dlc->owner = dev;
	dev->dlc   = dlc;
	rfcomm_dlc_unlock(dlc);

	MOD_INC_USE_COUNT;
	
out:
	write_unlock_bh(&rfcomm_dev_lock);

	if (err) {
		kfree(dev);
		return err;
	} else
		return dev->id;
}

static void rfcomm_dev_del(struct rfcomm_dev *dev)
{
	BT_DBG("dev %p", dev);

	write_lock_bh(&rfcomm_dev_lock);
	list_del_init(&dev->list);
	write_unlock_bh(&rfcomm_dev_lock);

	rfcomm_dev_put(dev);
}

/* ---- Send buffer ---- */

static inline unsigned int rfcomm_room(struct rfcomm_dlc *dlc)
{
	/* We can't let it be zero, because we don't get a callback 
	   when tx_credits becomes nonzero, hence we'd never wake up */
	return dlc->mtu * (dlc->tx_credits?:1);
}

static void rfcomm_wfree(struct sk_buff *skb)
{
	struct rfcomm_dev *dev = (void *) skb->sk;
	atomic_sub(skb->truesize, &dev->wmem_alloc);
	if (test_bit(RFCOMM_TTY_ATTACHED, &dev->flags))
		tasklet_schedule(&dev->wakeup_task);
	rfcomm_dev_put(dev);
}

static inline void rfcomm_set_owner_w(struct sk_buff *skb, struct rfcomm_dev *dev)
{
	rfcomm_dev_hold(dev);
	atomic_add(skb->truesize, &dev->wmem_alloc);
	skb->sk = (void *) dev;
	skb->destructor = rfcomm_wfree;
}

static struct sk_buff *rfcomm_wmalloc(struct rfcomm_dev *dev, unsigned long size, int force, int priority)
{
	if (force || atomic_read(&dev->wmem_alloc) < rfcomm_room(dev->dlc)) {
		struct sk_buff *skb = alloc_skb(size, priority);
		if (skb) {
			rfcomm_set_owner_w(skb, dev);
			return skb;
		}
	}
	return NULL;
}

/* ---- Device IOCTLs ---- */

#define NOCAP_FLAGS ((1 << RFCOMM_REUSE_DLC) | (1 << RFCOMM_RELEASE_ONHUP))

static int rfcomm_create_dev(struct sock *sk, unsigned long arg)
{
	struct rfcomm_dev_req req;
	struct rfcomm_dlc *dlc;
	int id;

	if (copy_from_user(&req, (void *) arg, sizeof(req)))
		return -EFAULT;

	BT_DBG("sk %p dev_id %id flags 0x%x", sk, req.dev_id, req.flags);

	if (req.flags != NOCAP_FLAGS && !capable(CAP_NET_ADMIN))
		return -EPERM;

	if (req.flags & (1 << RFCOMM_REUSE_DLC)) {
		/* Socket must be connected */
		if (sk->state != BT_CONNECTED)
			return -EBADFD;

		dlc = rfcomm_pi(sk)->dlc;
		rfcomm_dlc_hold(dlc);
	} else {
		dlc = rfcomm_dlc_alloc(GFP_KERNEL);
		if (!dlc)
			return -ENOMEM;
	}

	id = rfcomm_dev_add(&req, dlc);
	if (id < 0) {
		rfcomm_dlc_put(dlc);
		return id;
	}

	if (req.flags & (1 << RFCOMM_REUSE_DLC)) {
		/* DLC is now used by device.
		 * Socket must be disconnected */
		sk->state = BT_CLOSED;
	}

	return id;
}

static int rfcomm_release_dev(unsigned long arg)
{
	struct rfcomm_dev_req req;
	struct rfcomm_dev *dev;

	if (copy_from_user(&req, (void *) arg, sizeof(req)))
		return -EFAULT;

	BT_DBG("dev_id %id flags 0x%x", req.dev_id, req.flags);

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if (!(dev = rfcomm_dev_get(req.dev_id)))
		return -ENODEV;

	if (req.flags & (1 << RFCOMM_HANGUP_NOW))
		rfcomm_dlc_close(dev->dlc, 0);

	rfcomm_dev_del(dev);
	rfcomm_dev_put(dev);
	return 0;
}

static int rfcomm_get_dev_list(unsigned long arg)
{
	struct rfcomm_dev_list_req *dl;
	struct rfcomm_dev_info *di;
	struct list_head *p;
	int n = 0, size, err;
	u16 dev_num;

	BT_DBG("");

	if (get_user(dev_num, (u16 *) arg))
		return -EFAULT;

	if (!dev_num || dev_num > (PAGE_SIZE * 4) / sizeof(*di))
		return -EINVAL;

	size = sizeof(*dl) + dev_num * sizeof(*di);

	if (!(dl = kmalloc(size, GFP_KERNEL)))
		return -ENOMEM;

	di = dl->dev_info;

	read_lock_bh(&rfcomm_dev_lock);

	list_for_each(p, &rfcomm_dev_list) {
		struct rfcomm_dev *dev = list_entry(p, struct rfcomm_dev, list);
		(di + n)->id      = dev->id;
		(di + n)->flags   = dev->flags;
		(di + n)->state   = dev->dlc->state;
		(di + n)->channel = dev->channel;
		bacpy(&(di + n)->src, &dev->src);
		bacpy(&(di + n)->dst, &dev->dst);
		if (++n >= dev_num)
			break;
	}

	read_unlock_bh(&rfcomm_dev_lock);

	dl->dev_num = n;
	size = sizeof(*dl) + n * sizeof(*di);

	err = copy_to_user((void *) arg, dl, size);
	kfree(dl);

	return err ? -EFAULT : 0;
}

static int rfcomm_get_dev_info(unsigned long arg)
{
	struct rfcomm_dev *dev;
	struct rfcomm_dev_info di;
	int err = 0;

	BT_DBG("");

	if (copy_from_user(&di, (void *)arg, sizeof(di)))
		return -EFAULT;

	if (!(dev = rfcomm_dev_get(di.id)))
		return -ENODEV;

	di.flags   = dev->flags;
	di.channel = dev->channel;
	di.state   = dev->dlc->state;
	bacpy(&di.src, &dev->src);
	bacpy(&di.dst, &dev->dst);

	if (copy_to_user((void *)arg, &di, sizeof(di)))
		err = -EFAULT;

	rfcomm_dev_put(dev);
	return err;
}

int rfcomm_dev_ioctl(struct sock *sk, unsigned int cmd, unsigned long arg)
{
	BT_DBG("cmd %d arg %ld", cmd, arg);

	switch (cmd) {
	case RFCOMMCREATEDEV:
		return rfcomm_create_dev(sk, arg);

	case RFCOMMRELEASEDEV:
		return rfcomm_release_dev(arg);

	case RFCOMMGETDEVLIST:
		return rfcomm_get_dev_list(arg);

	case RFCOMMGETDEVINFO:
		return rfcomm_get_dev_info(arg);
	}

	return -EINVAL;
}

/* ---- DLC callbacks ---- */
static void rfcomm_dev_data_ready(struct rfcomm_dlc *dlc, struct sk_buff *skb)
{
	struct rfcomm_dev *dev = dlc->owner;
	struct tty_struct *tty;
       
	if (!dev || !(tty = dev->tty)) {
		kfree_skb(skb);
		return;
	}

	BT_DBG("dlc %p tty %p len %d", dlc, tty, skb->len);

	if (test_bit(TTY_DONT_FLIP, &tty->flags)) {
		register int i;
		for (i = 0; i < skb->len; i++) {
			if (tty->flip.count >= TTY_FLIPBUF_SIZE)
				tty_flip_buffer_push(tty);

			tty_insert_flip_char(tty, skb->data[i], 0);
		}
		tty_flip_buffer_push(tty);
	} else
		tty->ldisc.receive_buf(tty, skb->data, NULL, skb->len);

	kfree_skb(skb);
}

static void rfcomm_dev_state_change(struct rfcomm_dlc *dlc, int err)
{
	struct rfcomm_dev *dev = dlc->owner;
	if (!dev)
		return;
	
	BT_DBG("dlc %p dev %p err %d", dlc, dev, err);

	dev->err = err;
	wake_up_interruptible(&dev->wait);

	if (dlc->state == BT_CLOSED) {
		if (!dev->tty) {
			if (test_bit(RFCOMM_RELEASE_ONHUP, &dev->flags)) {
				rfcomm_dev_hold(dev);
				rfcomm_dev_del(dev);

				/* We have to drop DLC lock here, otherwise
				   rfcomm_dev_put() will dead lock if it's
				   the last reference. */
				rfcomm_dlc_unlock(dlc);
				rfcomm_dev_put(dev);
				rfcomm_dlc_lock(dlc);
			}
		} else 
			tty_hangup(dev->tty);
	}
}

static void rfcomm_dev_modem_status(struct rfcomm_dlc *dlc, u8 v24_sig)
{
	struct rfcomm_dev *dev = dlc->owner;
	if (!dev)
		return;
	
	BT_DBG("dlc %p dev %p v24_sig 0x%02x", dlc, dev, v24_sig);

	dev->modem_status = 
		((v24_sig & RFCOMM_V24_RTC) ? (TIOCM_DSR | TIOCM_DTR) : 0) |
		((v24_sig & RFCOMM_V24_RTR) ? (TIOCM_RTS | TIOCM_CTS) : 0) |
		((v24_sig & RFCOMM_V24_IC)  ? TIOCM_RI : 0) |
		((v24_sig & RFCOMM_V24_DV)  ? TIOCM_CD : 0);
}

/* ---- TTY functions ---- */
static void rfcomm_tty_wakeup(unsigned long arg)
{
	struct rfcomm_dev *dev = (void *) arg;
	struct tty_struct *tty = dev->tty;
	if (!tty)
		return;

	BT_DBG("dev %p tty %p", dev, tty);

	if (test_bit(TTY_DO_WRITE_WAKEUP, &tty->flags) && tty->ldisc.write_wakeup)
                (tty->ldisc.write_wakeup)(tty);

	wake_up_interruptible(&tty->write_wait);
#ifdef SERIAL_HAVE_POLL_WAIT
	wake_up_interruptible(&tty->poll_wait);
#endif
}

static int rfcomm_tty_open(struct tty_struct *tty, struct file *filp)
{
	DECLARE_WAITQUEUE(wait, current);
	struct rfcomm_dev *dev;
	struct rfcomm_dlc *dlc;
	int err, id;

	id = MINOR(tty->device) - tty->driver.minor_start;

	BT_DBG("tty %p id %d", tty, id);

	/* We don't leak this refcount. For reasons which are not entirely
	   clear, the TTY layer will call our ->close() method even if the
	   open fails. We decrease the refcount there, and decreasing it
	   here too would cause breakage. */
	dev = rfcomm_dev_get(id);
	if (!dev)
		return -ENODEV;

	BT_DBG("dev %p dst %s channel %d opened %d", dev, batostr(&dev->dst), dev->channel, dev->opened);

	if (dev->opened++ != 0)
		return 0;

	dlc = dev->dlc;

	/* Attach TTY and open DLC */

	rfcomm_dlc_lock(dlc);
	tty->driver_data = dev;
	dev->tty = tty;
	rfcomm_dlc_unlock(dlc);
	set_bit(RFCOMM_TTY_ATTACHED, &dev->flags);

	err = rfcomm_dlc_open(dlc, &dev->src, &dev->dst, dev->channel);
	if (err < 0)
		return err;

	/* Wait for DLC to connect */
	add_wait_queue(&dev->wait, &wait);
	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);

		if (dlc->state == BT_CLOSED) {
			err = -dev->err;
			break;
		}

		if (dlc->state == BT_CONNECTED)
			break;

		if (signal_pending(current)) {
			err = -EINTR;
			break;
		}

		schedule();
	}
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&dev->wait, &wait);

	return err;
}

static void rfcomm_tty_close(struct tty_struct *tty, struct file *filp)
{
	struct rfcomm_dev *dev = (struct rfcomm_dev *) tty->driver_data;
	if (!dev)
		return;

	BT_DBG("tty %p dev %p dlc %p opened %d", tty, dev, dev->dlc, dev->opened);

	if (--dev->opened == 0) {
		/* Close DLC and dettach TTY */
		rfcomm_dlc_close(dev->dlc, 0);

		clear_bit(RFCOMM_TTY_ATTACHED, &dev->flags);
		tasklet_kill(&dev->wakeup_task);

		rfcomm_dlc_lock(dev->dlc);
		tty->driver_data = NULL;
		dev->tty = NULL;
		rfcomm_dlc_unlock(dev->dlc);
	}

	rfcomm_dev_put(dev);
}

static int rfcomm_tty_write(struct tty_struct *tty, int from_user, const unsigned char *buf, int count)
{
	struct rfcomm_dev *dev = (struct rfcomm_dev *) tty->driver_data;
	struct rfcomm_dlc *dlc = dev->dlc;
	struct sk_buff *skb;
	int err = 0, sent = 0, size;

	BT_DBG("tty %p from_user %d count %d", tty, from_user, count);

	while (count) {
		size = min_t(uint, count, dlc->mtu);

		if (from_user)
			skb = rfcomm_wmalloc(dev, size + RFCOMM_SKB_RESERVE, 0, GFP_KERNEL);
		else
			skb = rfcomm_wmalloc(dev, size + RFCOMM_SKB_RESERVE, 0, GFP_ATOMIC);
		
		if (!skb)
			break;

		skb_reserve(skb, RFCOMM_SKB_HEAD_RESERVE);

		if (from_user)
			copy_from_user(skb_put(skb, size), buf + sent, size);
		else
			memcpy(skb_put(skb, size), buf + sent, size);

		if ((err = rfcomm_dlc_send(dlc, skb)) < 0) {
			kfree_skb(skb);
			break;
		}

		sent  += size;
		count -= size;
	}

	return sent ? sent : err;
}

static void rfcomm_tty_put_char(struct tty_struct *tty, unsigned char ch)
{
	struct rfcomm_dev *dev = (struct rfcomm_dev *) tty->driver_data;
	struct rfcomm_dlc *dlc = dev->dlc;
	struct sk_buff *skb;

	BT_DBG("tty %p char %x", tty, ch);

	skb = rfcomm_wmalloc(dev, 1 + RFCOMM_SKB_RESERVE, 1, GFP_ATOMIC);

	if (!skb)
		return;

	skb_reserve(skb, RFCOMM_SKB_HEAD_RESERVE);

	*(char *)skb_put(skb, 1) = ch;

	if ((rfcomm_dlc_send(dlc, skb)) < 0)
		kfree_skb(skb);	
}

static int rfcomm_tty_write_room(struct tty_struct *tty)
{
	struct rfcomm_dev *dev = (struct rfcomm_dev *) tty->driver_data;
	int room;
	
	BT_DBG("tty %p", tty);

	room = rfcomm_room(dev->dlc) - atomic_read(&dev->wmem_alloc);
	if (room < 0)
		room = 0;

	return room;
}

static int rfcomm_tty_set_modem_status(uint cmd, struct rfcomm_dlc *dlc, uint status)
{
	u8 v24_sig, mask;

	BT_DBG("dlc %p cmd 0x%02x", dlc, cmd);

	if (cmd == TIOCMSET)
		v24_sig = 0;
	else
		rfcomm_dlc_get_modem_status(dlc, &v24_sig);

	mask =  ((status & TIOCM_DSR) ? RFCOMM_V24_RTC : 0) |
		((status & TIOCM_DTR) ? RFCOMM_V24_RTC : 0) |
		((status & TIOCM_RTS) ? RFCOMM_V24_RTR : 0) |
		((status & TIOCM_CTS) ? RFCOMM_V24_RTR : 0) |
		((status & TIOCM_RI)  ? RFCOMM_V24_IC  : 0) |
		((status & TIOCM_CD)  ? RFCOMM_V24_DV  : 0);

	if (cmd == TIOCMBIC)
		v24_sig &= ~mask;
	else
		v24_sig |= mask;

	rfcomm_dlc_set_modem_status(dlc, v24_sig);
	return 0;
}

static int rfcomm_tty_ioctl(struct tty_struct *tty, struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct rfcomm_dev *dev = (struct rfcomm_dev *) tty->driver_data;
	struct rfcomm_dlc *dlc = dev->dlc;
	uint status;
	int err;

	BT_DBG("tty %p cmd 0x%02x", tty, cmd);

	switch (cmd) {
	case TCGETS:
		BT_DBG("TCGETS is not supported");
		return -ENOIOCTLCMD;

	case TCSETS:
		BT_DBG("TCSETS is not supported");
		return -ENOIOCTLCMD;

	case TIOCMGET:
		BT_DBG("TIOCMGET");

		return put_user(dev->modem_status, (unsigned int *)arg);

	case TIOCMSET: /* Turns on and off the lines as specified by the mask */
	case TIOCMBIS: /* Turns on the lines as specified by the mask */
	case TIOCMBIC: /* Turns off the lines as specified by the mask */
		if ((err = get_user(status, (unsigned int *)arg)))
			return err;
		return rfcomm_tty_set_modem_status(cmd, dlc, status);

	case TIOCMIWAIT:
		BT_DBG("TIOCMIWAIT");
		break;

	case TIOCGICOUNT:
		BT_DBG("TIOCGICOUNT");
		break;

	case TIOCGSERIAL:
		BT_ERR("TIOCGSERIAL is not supported");
		return -ENOIOCTLCMD;

	case TIOCSSERIAL:
		BT_ERR("TIOCSSERIAL is not supported");
		return -ENOIOCTLCMD;

	case TIOCSERGSTRUCT:
		BT_ERR("TIOCSERGSTRUCT is not supported");
		return -ENOIOCTLCMD;

	case TIOCSERGETLSR:
		BT_ERR("TIOCSERGETLSR is not supported");
		return -ENOIOCTLCMD;

	case TIOCSERCONFIG:
		BT_ERR("TIOCSERCONFIG is not supported");
		return -ENOIOCTLCMD;

	default:
		return -ENOIOCTLCMD;	/* ioctls which we must ignore */

	}

	return -ENOIOCTLCMD;
}

#define RELEVANT_IFLAG(iflag) (iflag & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

static void rfcomm_tty_set_termios(struct tty_struct *tty, struct termios *old)
{
	BT_DBG("tty %p", tty);

	if ((tty->termios->c_cflag == old->c_cflag) &&
		(RELEVANT_IFLAG(tty->termios->c_iflag) == RELEVANT_IFLAG(old->c_iflag)))
		return;

	/* handle turning off CRTSCTS */
	if ((old->c_cflag & CRTSCTS) && !(tty->termios->c_cflag & CRTSCTS)) {
		BT_DBG("turning off CRTSCTS");
	}
}

static void rfcomm_tty_throttle(struct tty_struct *tty)
{
	struct rfcomm_dev *dev = (struct rfcomm_dev *) tty->driver_data;

	BT_DBG("tty %p dev %p", tty, dev);
	
	rfcomm_dlc_throttle(dev->dlc);
}

static void rfcomm_tty_unthrottle(struct tty_struct *tty)
{
	struct rfcomm_dev *dev = (struct rfcomm_dev *) tty->driver_data;

	BT_DBG("tty %p dev %p", tty, dev);
	
	rfcomm_dlc_unthrottle(dev->dlc);
}

static int rfcomm_tty_chars_in_buffer(struct tty_struct *tty)
{
	struct rfcomm_dev *dev = (struct rfcomm_dev *) tty->driver_data;
	struct rfcomm_dlc *dlc = dev->dlc;

	BT_DBG("tty %p dev %p", tty, dev);

	if (skb_queue_len(&dlc->tx_queue))
		return dlc->mtu;

	return 0;
}

static void rfcomm_tty_flush_buffer(struct tty_struct *tty)
{
	struct rfcomm_dev *dev = (struct rfcomm_dev *) tty->driver_data;
	if (!dev)
		return;

	BT_DBG("tty %p dev %p", tty, dev);

	skb_queue_purge(&dev->dlc->tx_queue);

	if (test_bit(TTY_DO_WRITE_WAKEUP, &tty->flags) && tty->ldisc.write_wakeup)
		tty->ldisc.write_wakeup(tty);
}

static void rfcomm_tty_send_xchar(struct tty_struct *tty, char ch)
{
	BT_DBG("tty %p ch %c", tty, ch);
}

static void rfcomm_tty_wait_until_sent(struct tty_struct *tty, int timeout)
{
	BT_DBG("tty %p timeout %d", tty, timeout);
}

static void rfcomm_tty_hangup(struct tty_struct *tty)
{
	struct rfcomm_dev *dev = (struct rfcomm_dev *) tty->driver_data;
	if (!dev)
		return;

	BT_DBG("tty %p dev %p", tty, dev);

	rfcomm_tty_flush_buffer(tty);

	if (test_bit(RFCOMM_RELEASE_ONHUP, &dev->flags))
		rfcomm_dev_del(dev);
}

static int rfcomm_tty_read_proc(char *buf, char **start, off_t offset, int len, int *eof, void *unused)
{
	return 0;
}

/* ---- TTY structure ---- */
static int    rfcomm_tty_refcount;       /* If we manage several devices */

static struct tty_struct *rfcomm_tty_table[RFCOMM_TTY_PORTS];
static struct termios *rfcomm_tty_termios[RFCOMM_TTY_PORTS];
static struct termios *rfcomm_tty_termios_locked[RFCOMM_TTY_PORTS];

static struct tty_driver rfcomm_tty_driver = {
	magic:			TTY_DRIVER_MAGIC,
	driver_name:		"rfcomm",
#ifdef CONFIG_DEVFS_FS
	name:			"bluetooth/rfcomm/%d",
#else
	name:			"rfcomm",
#endif
	major:			RFCOMM_TTY_MAJOR,
	minor_start:		RFCOMM_TTY_MINOR,
	num:			RFCOMM_TTY_PORTS,
	type:			TTY_DRIVER_TYPE_SERIAL,
	subtype:		SERIAL_TYPE_NORMAL,
	flags:			TTY_DRIVER_REAL_RAW,

	refcount:		&rfcomm_tty_refcount,
	table:			rfcomm_tty_table,
	termios:		rfcomm_tty_termios,
	termios_locked:		rfcomm_tty_termios_locked,

	open:			rfcomm_tty_open,
	close:			rfcomm_tty_close,
	put_char:		rfcomm_tty_put_char,
	write:			rfcomm_tty_write,
	write_room:		rfcomm_tty_write_room,
	chars_in_buffer:	rfcomm_tty_chars_in_buffer,
	flush_buffer:		rfcomm_tty_flush_buffer,
	ioctl:			rfcomm_tty_ioctl,
	throttle:		rfcomm_tty_throttle,
	unthrottle:		rfcomm_tty_unthrottle,
	set_termios:		rfcomm_tty_set_termios,
	send_xchar:		rfcomm_tty_send_xchar,
	stop:			NULL,
	start:			NULL,
	hangup:			rfcomm_tty_hangup,
	wait_until_sent:	rfcomm_tty_wait_until_sent,
	read_proc:		rfcomm_tty_read_proc,
};

int rfcomm_init_ttys(void)
{
	int i;

	/* Initalize our global data */
	for (i = 0; i < RFCOMM_TTY_PORTS; i++)
		rfcomm_tty_table[i] = NULL;

	/* Register the TTY driver */
	rfcomm_tty_driver.init_termios = tty_std_termios;
	rfcomm_tty_driver.init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	rfcomm_tty_driver.flags = TTY_DRIVER_REAL_RAW;

	if (tty_register_driver(&rfcomm_tty_driver)) {
		BT_ERR("Can't register RFCOMM TTY driver");
		return -1;
	}

	return 0;
}

void rfcomm_cleanup_ttys(void)
{
	tty_unregister_driver(&rfcomm_tty_driver);
	return;
}
