/*
 *  TUN - Universal TUN/TAP device driver.
 *  Copyright (C) 1999-2002 Maxim Krasnyansky <maxk@qualcomm.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  $Id: tun.c,v 1.15 2002/03/01 02:44:24 maxk Exp $
 */

/*
 *  Daniel Podlejski <underley@underley.eu.org>
 *    Modifications for 2.3.99-pre5 kernel.
 */

#define TUN_VER "1.5"

#include <linux/config.h>
#include <linux/module.h>

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/fcntl.h>
#include <linux/init.h>
#include <linux/random.h>

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/miscdevice.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/if_tun.h>

#include <asm/system.h>
#include <asm/uaccess.h>

#ifdef TUN_DEBUG
static int debug;
#endif

/* Network device part of the driver */

/* Net device open. */
static int tun_net_open(struct net_device *dev)
{
	netif_start_queue(dev);
	return 0;
}

/* Net device close. */
static int tun_net_close(struct net_device *dev)
{
	netif_stop_queue(dev);
	return 0;
}

/* Net device start xmit */
static int tun_net_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct tun_struct *tun = (struct tun_struct *)dev->priv;

	DBG(KERN_INFO "%s: tun_net_xmit %d\n", tun->name, skb->len);

	/* Drop packet if interface is not attached */
	if (!tun->attached)
		goto drop;

	/* Queue packet */
	if (!(tun->flags & TUN_ONE_QUEUE)) {
		/* Normal queueing mode.
		 * Packet scheduler handles dropping. */
		if (skb_queue_len(&tun->readq) >= TUN_READQ_SIZE)
			netif_stop_queue(dev);
	} else {
		/* Single queue mode.
		 * Driver handles dropping itself. */
		if (skb_queue_len(&tun->readq) >= dev->tx_queue_len)
			goto drop;
	}
	skb_queue_tail(&tun->readq, skb);

	/* Notify and wake up reader process */
	if (tun->flags & TUN_FASYNC)
		kill_fasync(&tun->fasync, SIGIO, POLL_IN);
	wake_up_interruptible(&tun->read_wait);
	return 0;

drop:
	tun->stats.tx_dropped++;
	kfree_skb(skb);
	return 0;
}

static void tun_net_mclist(struct net_device *dev)
{
	/* Nothing to do for multicast filters. 
	 * We always accept all frames. */
	return;
}

static struct net_device_stats *tun_net_stats(struct net_device *dev)
{
	struct tun_struct *tun = (struct tun_struct *)dev->priv;
	return &tun->stats;
}

/* Initialize net device. */
int tun_net_init(struct net_device *dev)
{
	struct tun_struct *tun = (struct tun_struct *)dev->priv;
   
	DBG(KERN_INFO "%s: tun_net_init\n", tun->name);

	SET_MODULE_OWNER(dev);
	dev->open = tun_net_open;
	dev->hard_start_xmit = tun_net_xmit;
	dev->stop = tun_net_close;
	dev->get_stats = tun_net_stats;

	switch (tun->flags & TUN_TYPE_MASK) {
	case TUN_TUN_DEV:
		/* Point-to-Point TUN Device */
		dev->hard_header_len = 0;
		dev->addr_len = 0;
		dev->mtu = 1500;

		/* Type PPP seems most suitable */
		dev->type = ARPHRD_PPP; 
		dev->flags = IFF_POINTOPOINT | IFF_NOARP | IFF_MULTICAST;
		dev->tx_queue_len = 10;
		break;

	case TUN_TAP_DEV:
		/* Ethernet TAP Device */
		dev->set_multicast_list = tun_net_mclist;

		/* Generate random Ethernet address.  */
		*(u16 *)dev->dev_addr = htons(0x00FF);
		get_random_bytes(dev->dev_addr + sizeof(u16), 4);

		ether_setup(dev);
		break;
	};

	return 0;
}

/* Character device part */

/* Poll */
static unsigned int tun_chr_poll(struct file *file, poll_table * wait)
{  
	struct tun_struct *tun = (struct tun_struct *)file->private_data;
	unsigned int mask = POLLOUT | POLLWRNORM;

	if (!tun)
		return -EBADFD;

	DBG(KERN_INFO "%s: tun_chr_poll\n", tun->name);

	poll_wait(file, &tun->read_wait, wait);
 
	if (skb_queue_len(&tun->readq))
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

/* Get packet from user space buffer(already verified) */
static __inline__ ssize_t tun_get_user(struct tun_struct *tun, struct iovec *iv, size_t count)
{
	struct tun_pi pi = { 0, __constant_htons(ETH_P_IP) };
	struct sk_buff *skb;
	size_t len = count;

	if (!(tun->flags & TUN_NO_PI)) {
		if ((len -= sizeof(pi)) < 0)
			return -EINVAL;

		memcpy_fromiovec((void *)&pi, iv, sizeof(pi));
	}
 
	if (!(skb = alloc_skb(len + 2, GFP_KERNEL))) {
		tun->stats.rx_dropped++;
		return -ENOMEM;
	}

	skb_reserve(skb, 2);
	memcpy_fromiovec(skb_put(skb, len), iv, len);

	skb->dev = &tun->dev;
	switch (tun->flags & TUN_TYPE_MASK) {
	case TUN_TUN_DEV:
		skb->mac.raw = skb->data;
		skb->protocol = pi.proto;
		break;
	case TUN_TAP_DEV:
		skb->protocol = eth_type_trans(skb, &tun->dev);
		break;
	};

	if (tun->flags & TUN_NOCHECKSUM)
		skb->ip_summed = CHECKSUM_UNNECESSARY;
 
	netif_rx_ni(skb);
   
	tun->stats.rx_packets++;
	tun->stats.rx_bytes += len;

	return count;
} 

/* Writev */
static ssize_t tun_chr_writev(struct file * file, const struct iovec *iv, 
			      unsigned long count, loff_t *pos)
{
	struct tun_struct *tun = (struct tun_struct *)file->private_data;
	unsigned long i;
	size_t len;

	if (!tun)
		return -EBADFD;

	DBG(KERN_INFO "%s: tun_chr_write %d\n", tun->name, count);

	for (i = 0, len = 0; i < count; i++) {
		if (verify_area(VERIFY_READ, iv[i].iov_base, iv[i].iov_len))
			return -EFAULT;
		len += iv[i].iov_len;
	}

	return tun_get_user(tun, (struct iovec *) iv, len);
}

/* Write */
static ssize_t tun_chr_write(struct file * file, const char * buf, 
			     size_t count, loff_t *pos)
{
	struct iovec iv = { (void *) buf, count };
	return tun_chr_writev(file, &iv, 1, pos);
}

/* Put packet to the user space buffer (already verified) */
static __inline__ ssize_t tun_put_user(struct tun_struct *tun,
				       struct sk_buff *skb,
				       struct iovec *iv, int len)
{
	struct tun_pi pi = { 0, skb->protocol };
	ssize_t total = 0;

	if (!(tun->flags & TUN_NO_PI)) {
		if ((len -= sizeof(pi)) < 0)
			return -EINVAL;

		if (len < skb->len) {
			/* Packet will be striped */
			pi.flags |= TUN_PKT_STRIP;
		}
 
		memcpy_toiovec(iv, (void *) &pi, sizeof(pi));
		total += sizeof(pi);
	}       

	len = MIN(skb->len, len);

	skb_copy_datagram_iovec(skb, 0, iv, len);
	total += len;

	tun->stats.tx_packets++;
	tun->stats.tx_bytes += len;

	return total;
}

/* Readv */
static ssize_t tun_chr_readv(struct file *file, const struct iovec *iv,
			    unsigned long count, loff_t *pos)
{
	struct tun_struct *tun = (struct tun_struct *)file->private_data;
	DECLARE_WAITQUEUE(wait, current);
	struct sk_buff *skb;
	ssize_t len, ret = 0;
	unsigned long i;

	if (!tun)
		return -EBADFD;

	DBG(KERN_INFO "%s: tun_chr_read\n", tun->name);

	for (i = 0, len = 0; i < count; i++) {
		if (verify_area(VERIFY_WRITE, iv[i].iov_base, iv[i].iov_len))
			return -EFAULT;
		len += iv[i].iov_len;
	}

	add_wait_queue(&tun->read_wait, &wait);
	while (len) {
		current->state = TASK_INTERRUPTIBLE;

		/* Read frames from the queue */
		if (!(skb=skb_dequeue(&tun->readq))) {
			if (file->f_flags & O_NONBLOCK) {
				ret = -EAGAIN;
				break;
			}
			if (signal_pending(current)) {
				ret = -ERESTARTSYS;
				break;
			}

			/* Nothing to read, let's sleep */
			schedule();
			continue;
		}
		netif_start_queue(&tun->dev);

		ret = tun_put_user(tun, skb, (struct iovec *) iv, len);

		kfree_skb(skb);
		break;
	}

	current->state = TASK_RUNNING;
	remove_wait_queue(&tun->read_wait, &wait);

	return ret;
}

/* Read */
static ssize_t tun_chr_read(struct file * file, char * buf, 
			    size_t count, loff_t *pos)
{
	struct iovec iv = { buf, count };
	return tun_chr_readv(file, &iv, 1, pos);
}

static int tun_set_iff(struct file *file, struct ifreq *ifr)
{
	struct tun_struct *tun;
	struct net_device *dev;
	int err;

	dev = __dev_get_by_name(ifr->ifr_name);
	if (dev) {
		/* Device exist */
		tun = dev->priv;

		if (dev->init != tun_net_init || tun->attached)
			return -EBUSY;

		/* Check permissions */
		if (tun->owner != -1)
			if (current->euid != tun->owner && !capable(CAP_NET_ADMIN))
				return -EPERM;
	} else {
		char *name;

		/* Allocate new device */
		if (!(tun = kmalloc(sizeof(struct tun_struct), GFP_KERNEL)) )
			return -ENOMEM;
		memset(tun, 0, sizeof(struct tun_struct));

		skb_queue_head_init(&tun->readq);
		init_waitqueue_head(&tun->read_wait);

		tun->owner = -1;
		tun->dev.init = tun_net_init;
		tun->dev.priv = tun;

		err = -EINVAL;

		/* Set dev type */
		if (ifr->ifr_flags & IFF_TUN) {
			/* TUN device */
			tun->flags |= TUN_TUN_DEV;
			name = "tun%d";
		} else if (ifr->ifr_flags & IFF_TAP) {
			/* TAP device */
			tun->flags |= TUN_TAP_DEV;
			name = "tap%d";
		} else 
			goto failed;
   
		if (*ifr->ifr_name)
			name = ifr->ifr_name;

		if ((err = dev_alloc_name(&tun->dev, name)) < 0)
			goto failed;
		if ((err = register_netdevice(&tun->dev)))
			goto failed;
	
		MOD_INC_USE_COUNT;

		tun->name = tun->dev.name;
	}

	DBG(KERN_INFO "%s: tun_set_iff\n", tun->name);

	if (ifr->ifr_flags & IFF_NO_PI)
		tun->flags |= TUN_NO_PI;

	if (ifr->ifr_flags & IFF_ONE_QUEUE)
		tun->flags |= TUN_ONE_QUEUE;

	file->private_data = tun;
	tun->attached = 1;

	strcpy(ifr->ifr_name, tun->name);
	return 0;

failed:
	kfree(tun);
	return err;
}

static int tun_chr_ioctl(struct inode *inode, struct file *file, 
			 unsigned int cmd, unsigned long arg)
{
	struct tun_struct *tun = (struct tun_struct *)file->private_data;

	if (cmd == TUNSETIFF && !tun) {
		struct ifreq ifr;
		int err;

		if (copy_from_user(&ifr, (void *)arg, sizeof(ifr)))
			return -EFAULT;
		ifr.ifr_name[IFNAMSIZ-1] = '\0';

		rtnl_lock();
		err = tun_set_iff(file, &ifr);
		rtnl_unlock();

		if (err)
			return err;

		copy_to_user((void *)arg, &ifr, sizeof(ifr));
		return 0;
	}

	if (!tun)
		return -EBADFD;

	DBG(KERN_INFO "%s: tun_chr_ioctl cmd %d\n", tun->name, cmd);

	switch (cmd) {
	case TUNSETNOCSUM:
		/* Disable/Enable checksum */
		if (arg)
			tun->flags |= TUN_NOCHECKSUM;
		else
			tun->flags &= ~TUN_NOCHECKSUM;

		DBG(KERN_INFO "%s: checksum %s\n",
		    tun->name, arg ? "disabled" : "enabled");
		break;

	case TUNSETPERSIST:
		/* Disable/Enable persist mode */
		if (arg)
			tun->flags |= TUN_PERSIST;
		else
			tun->flags &= ~TUN_PERSIST;

		DBG(KERN_INFO "%s: persist %s\n",
		    tun->name, arg ? "disabled" : "enabled");
		break;

	case TUNSETOWNER:
		/* Set owner of the device */
		tun->owner = (uid_t) arg;

		DBG(KERN_INFO "%s: owner set to %d\n", tun->owner);
		break;

#ifdef TUN_DEBUG
	case TUNSETDEBUG:
		tun->debug = arg;
		break;
#endif

	default:
		return -EINVAL;
	};

	return 0;
}

static int tun_chr_fasync(int fd, struct file *file, int on)
{
	struct tun_struct *tun = (struct tun_struct *)file->private_data;
	int ret;

	if (!tun)
		return -EBADFD;

	DBG(KERN_INFO "%s: tun_chr_fasync %d\n", tun->name, on);

	if ((ret = fasync_helper(fd, file, on, &tun->fasync)) < 0)
		return ret; 
 
	if (on) {
		tun->flags |= TUN_FASYNC;
		if (!file->f_owner.pid) {
			file->f_owner.pid  = current->pid;
			file->f_owner.uid  = current->uid;
			file->f_owner.euid = current->euid;
		}
	} else 
		tun->flags &= ~TUN_FASYNC;

	return 0;
}

static int tun_chr_open(struct inode *inode, struct file * file)
{
	DBG1(KERN_INFO "tunX: tun_chr_open\n");
	file->private_data = NULL;
	return 0;
}

static int tun_chr_close(struct inode *inode, struct file *file)
{
	struct tun_struct *tun = (struct tun_struct *)file->private_data;

	if (!tun)
		return 0;

	DBG(KERN_INFO "%s: tun_chr_close\n", tun->name);

	tun_chr_fasync(-1, file, 0);

	rtnl_lock();

	/* Detach from net device */
	file->private_data = NULL;
	tun->attached = 0;

	/* Drop read queue */
	skb_queue_purge(&tun->readq);

	if (!(tun->flags & TUN_PERSIST)) {
		dev_close(&tun->dev);
		unregister_netdevice(&tun->dev);
		kfree(tun);
		MOD_DEC_USE_COUNT;
	}

	rtnl_unlock();
	return 0;
}

static struct file_operations tun_fops = {
	owner:	THIS_MODULE,	
	llseek:	no_llseek,
	read:	tun_chr_read,
	readv:	tun_chr_readv,
	write:	tun_chr_write,
	writev:	tun_chr_writev,
	poll:	tun_chr_poll,
	ioctl:	tun_chr_ioctl,
	open:	tun_chr_open,
	release:tun_chr_close,
	fasync:	tun_chr_fasync		
};

static struct miscdevice tun_miscdev=
{
	TUN_MINOR,
	"net/tun",
	&tun_fops
};

int __init tun_init(void)
{
	printk(KERN_INFO "Universal TUN/TAP device driver %s " 
	       "(C)1999-2002 Maxim Krasnyansky\n", TUN_VER);

	if (misc_register(&tun_miscdev)) {
		printk(KERN_ERR "tun: Can't register misc device %d\n", TUN_MINOR);
		return -EIO;
	}

	return 0;
}

void tun_cleanup(void)
{
	misc_deregister(&tun_miscdev);  
}

module_init(tun_init);
module_exit(tun_cleanup);
MODULE_LICENSE("GPL");
