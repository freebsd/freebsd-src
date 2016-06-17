/*
 * DLCI		Implementation of Frame Relay protocol for Linux, according to
 *		RFC 1490.  This generic device provides en/decapsulation for an
 *		underlying hardware driver.  Routes & IPs are assigned to these
 *		interfaces.  Requires 'dlcicfg' program to create usable 
 *		interfaces, the initial one, 'dlci' is for IOCTL use only.
 *
 * Version:	@(#)dlci.c	0.35	4 Jan 1997
 *
 * Author:	Mike McLagan <mike.mclagan@linux.org>
 *
 * Changes:
 *
 *		0.15	Mike Mclagan	Packet freeing, bug in kmalloc call
 *					DLCI_RET handling
 *		0.20	Mike McLagan	More conservative on which packets
 *					are returned for retry and whic are
 *					are dropped.  If DLCI_RET_DROP is
 *					returned from the FRAD, the packet is
 *				 	sent back to Linux for re-transmission
 *		0.25	Mike McLagan	Converted to use SIOC IOCTL calls
 *		0.30	Jim Freeman	Fixed to allow IPX traffic
 *		0.35	Michael Elizabeth	Fixed incorrect memcpy_fromfs
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */

#include <linux/config.h> /* for CONFIG_DLCI_COUNT */
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/init.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/uaccess.h>

#include <linux/errno.h>

#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/if_frad.h>

#include <net/sock.h>

static const char devname[] = "dlci";
static const char version[] = "DLCI driver v0.35, 4 Jan 1997, mike.mclagan@linux.org";

static struct net_device *open_dev[CONFIG_DLCI_COUNT];

static char *basename[16];

int dlci_init(struct net_device *dev);

/* allow FRAD's to register their name as a valid FRAD */
int register_frad(const char *name)
{
	int i;

	if (!name)
		return(-EINVAL);

	for (i=0;i<sizeof(basename) / sizeof(char *);i++)
	{
		if (!basename[i])
			break;

		/* take care of multiple registrations */
		if (strcmp(basename[i], name) == 0)
			return(0);
	}

	if (i == sizeof(basename) / sizeof(char *))
		return(-EMLINK);

	basename[i] = kmalloc(strlen(name) + 1, GFP_KERNEL);
	if (!basename[i])
		return(-ENOMEM);

	strcpy(basename[i], name);

	return(0);
}

int unregister_frad(const char *name)
{
	int i;

	if (!name)
		return(-EINVAL);

	for (i=0;i<sizeof(basename) / sizeof(char *);i++)
		if (basename[i] && (strcmp(basename[i], name) == 0))
			break;

	if (i == sizeof(basename) / sizeof(char *))
		return(-EINVAL);

	kfree(basename[i]);
	basename[i] = NULL;

	return(0);
}

/* 
 * these encapsulate the RFC 1490 requirements as well as 
 * deal with packet transmission and reception, working with
 * the upper network layers 
 */

static int dlci_header(struct sk_buff *skb, struct net_device *dev, 
                           unsigned short type, void *daddr, void *saddr, 
                           unsigned len)
{
	struct frhdr		hdr;
	struct dlci_local	*dlp;
	unsigned int		hlen;
	char			*dest;

	dlp = dev->priv;

	hdr.control = FRAD_I_UI;
	switch(type)
	{
		case ETH_P_IP:
			hdr.IP_NLPID = FRAD_P_IP;
			hlen = sizeof(hdr.control) + sizeof(hdr.IP_NLPID);
			break;

		/* feel free to add other types, if necessary */

		default:
			hdr.pad = FRAD_P_PADDING;
			hdr.NLPID = FRAD_P_SNAP;
			memset(hdr.OUI, 0, sizeof(hdr.OUI));
			hdr.PID = htons(type);
			hlen = sizeof(hdr);
			break;
	}

	dest = skb_push(skb, hlen);
	if (!dest)
		return(0);

	memcpy(dest, &hdr, hlen);

	return(hlen);
}

static void dlci_receive(struct sk_buff *skb, struct net_device *dev)
{
	struct dlci_local *dlp;
	struct frhdr		*hdr;
	int					process, header;

	dlp = dev->priv;
	hdr = (struct frhdr *) skb->data;
	process = 0;
	header = 0;
	skb->dev = dev;

	if (hdr->control != FRAD_I_UI)
	{
		printk(KERN_NOTICE "%s: Invalid header flag 0x%02X.\n", dev->name, hdr->control);
		dlp->stats.rx_errors++;
	}
	else
		switch(hdr->IP_NLPID)
		{
			case FRAD_P_PADDING:
				if (hdr->NLPID != FRAD_P_SNAP)
				{
					printk(KERN_NOTICE "%s: Unsupported NLPID 0x%02X.\n", dev->name, hdr->NLPID);
					dlp->stats.rx_errors++;
					break;
				}
	 
				if (hdr->OUI[0] + hdr->OUI[1] + hdr->OUI[2] != 0)
				{
					printk(KERN_NOTICE "%s: Unsupported organizationally unique identifier 0x%02X-%02X-%02X.\n", dev->name, hdr->OUI[0], hdr->OUI[1], hdr->OUI[2]);
					dlp->stats.rx_errors++;
					break;
				}

				/* at this point, it's an EtherType frame */
				header = sizeof(struct frhdr);
				/* Already in network order ! */
				skb->protocol = hdr->PID;
				process = 1;
				break;

			case FRAD_P_IP:
				header = sizeof(hdr->control) + sizeof(hdr->IP_NLPID);
				skb->protocol = htons(ETH_P_IP);
				process = 1;
				break;

			case FRAD_P_SNAP:
			case FRAD_P_Q933:
			case FRAD_P_CLNP:
				printk(KERN_NOTICE "%s: Unsupported NLPID 0x%02X.\n", dev->name, hdr->pad);
				dlp->stats.rx_errors++;
				break;

			default:
				printk(KERN_NOTICE "%s: Invalid pad byte 0x%02X.\n", dev->name, hdr->pad);
				dlp->stats.rx_errors++;
				break;				
		}

	if (process)
	{
		/* we've set up the protocol, so discard the header */
		skb->mac.raw = skb->data; 
		skb_pull(skb, header);
		dlp->stats.rx_bytes += skb->len;
		netif_rx(skb);
		dlp->stats.rx_packets++;
		dev->last_rx = jiffies;
	}
	else
		dev_kfree_skb(skb);
}

static int dlci_transmit(struct sk_buff *skb, struct net_device *dev)
{
	struct dlci_local *dlp;
	int					ret;

	ret = 0;

	if (!skb || !dev)
		return(0);

	dlp = dev->priv;

	netif_stop_queue(dev);
	
	ret = dlp->slave->hard_start_xmit(skb, dlp->slave);
	switch (ret)
	{
		case DLCI_RET_OK:
			dlp->stats.tx_packets++;
			ret = 0;
			break;
			case DLCI_RET_ERR:
			dlp->stats.tx_errors++;
			ret = 0;
			break;
			case DLCI_RET_DROP:
			dlp->stats.tx_dropped++;
			ret = 1;
			break;
	}
	/* Alan Cox recommends always returning 0, and always freeing the packet */
	/* experience suggest a slightly more conservative approach */

	if (!ret)
	{
		dev_kfree_skb(skb);
		netif_wake_queue(dev);
	}
	return(ret);
}

int dlci_config(struct net_device *dev, struct dlci_conf *conf, int get)
{
	struct dlci_conf	config;
	struct dlci_local	*dlp;
	struct frad_local	*flp;
	int			err;

	dlp = dev->priv;

	flp = dlp->slave->priv;

	if (!get)
	{
		if(copy_from_user(&config, conf, sizeof(struct dlci_conf)))
			return -EFAULT;
		if (config.flags & ~DLCI_VALID_FLAGS)
			return(-EINVAL);
		memcpy(&dlp->config, &config, sizeof(struct dlci_conf));
		dlp->configured = 1;
	}

	err = (*flp->dlci_conf)(dlp->slave, dev, get);
	if (err)
		return(err);

	if (get)
	{
		if(copy_to_user(conf, &dlp->config, sizeof(struct dlci_conf)))
			return -EFAULT;
	}

	return(0);
}

int dlci_dev_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct dlci_local *dlp;

	if (!capable(CAP_NET_ADMIN))
		return(-EPERM);

	dlp = dev->priv;

	switch(cmd)
	{
		case DLCI_GET_SLAVE:
			if (!*(short *)(dev->dev_addr))
				return(-EINVAL);

			strncpy(ifr->ifr_slave, dlp->slave->name, sizeof(ifr->ifr_slave));
			break;

		case DLCI_GET_CONF:
		case DLCI_SET_CONF:
			if (!*(short *)(dev->dev_addr))
				return(-EINVAL);

			return(dlci_config(dev, (struct dlci_conf *) ifr->ifr_data, cmd == DLCI_GET_CONF));
			break;

		default: 
			return(-EOPNOTSUPP);
	}
	return(0);
}

static int dlci_change_mtu(struct net_device *dev, int new_mtu)
{
	struct dlci_local *dlp;

	dlp = dev->priv;

	return((*dlp->slave->change_mtu)(dlp->slave, new_mtu));
}

static int dlci_open(struct net_device *dev)
{
	struct dlci_local	*dlp;
	struct frad_local	*flp;
	int			err;

	dlp = dev->priv;

	if (!*(short *)(dev->dev_addr))
		return(-EINVAL);

	if (!netif_running(dlp->slave))
		return(-ENOTCONN);

	flp = dlp->slave->priv;
	err = (*flp->activate)(dlp->slave, dev);
	if (err)
		return(err);

	netif_start_queue(dev);

	return 0;
}

static int dlci_close(struct net_device *dev)
{
	struct dlci_local	*dlp;
	struct frad_local	*flp;
	int			err;

	netif_stop_queue(dev);

	dlp = dev->priv;

	flp = dlp->slave->priv;
	err = (*flp->deactivate)(dlp->slave, dev);

	return 0;
}

static struct net_device_stats *dlci_get_stats(struct net_device *dev)
{
	struct dlci_local *dlp;

	dlp = dev->priv;

	return(&dlp->stats);
}

int dlci_add(struct dlci_add *dlci)
{
	struct net_device		*master, *slave;
	struct dlci_local	*dlp;
	struct frad_local	*flp;
	int			err, i;
	char			buf[10];

	/* validate slave device */
	slave = __dev_get_by_name(dlci->devname);
	if (!slave)
		return(-ENODEV);

	if (slave->type != ARPHRD_FRAD)
		return(-EINVAL);

	/* check for registration */
	for (i=0;i<sizeof(basename) / sizeof(char *); i++)
		if ((basename[i]) && 
			 (strncmp(dlci->devname, basename[i], strlen(basename[i])) == 0) && 
			 (strlen(dlci->devname) > strlen(basename[i])))
			break;

	if (i == sizeof(basename) / sizeof(char *))
		return(-EINVAL);

	/* check for too many open devices : should this be dynamic ? */
	for(i=0;i<CONFIG_DLCI_COUNT;i++)
		if (!open_dev[i])
			break;

	if (i == CONFIG_DLCI_COUNT)
		return(-ENOSPC);  /*  #### Alan: Comments on this?? */

	/* create device name */
	sprintf(buf, "%s%02i", devname, i);

	master = kmalloc(sizeof(*master), GFP_KERNEL);
	if (!master)
		return(-ENOMEM);

	memset(master, 0, sizeof(*master));

	strcpy(master->name, buf);
	master->init = dlci_init;
	master->flags = 0;

	err = register_netdev(master);
	if (err < 0)
	{
		kfree(master);
		return(err);
	}

	*(short *)(master->dev_addr) = dlci->dlci;

	dlp = (struct dlci_local *) master->priv;
	dlp->slave = slave;

	flp = slave->priv;
	err = flp ? (*flp->assoc)(slave, master) : -EINVAL;
	if (err < 0)
	{
		unregister_netdev(master);
		kfree(master->priv);
		kfree(master);
		return(err);
	}

	strcpy(dlci->devname, buf);
	open_dev[i] = master;
	MOD_INC_USE_COUNT;
	return(0);
}

int dlci_del(struct dlci_add *dlci)
{
	struct dlci_local	*dlp;
	struct frad_local	*flp;
	struct net_device		*master, *slave;
	int			i, err;

	/* validate slave device */
	master = __dev_get_by_name(dlci->devname);
	if (!master)
		return(-ENODEV);

	if (netif_running(master))
		return(-EBUSY);

	dlp = master->priv;
	slave = dlp->slave;
	flp = slave->priv;

	err = (*flp->deassoc)(slave, master);
	if (err)
		return(err);

	unregister_netdev(master);

	for(i=0;i<CONFIG_DLCI_COUNT;i++)
		if (master == open_dev[i])
			break;

	if (i<CONFIG_DLCI_COUNT)
		open_dev[i] = NULL;

	kfree(master->priv);
	kfree(master);

	MOD_DEC_USE_COUNT;

	return(0);
}

int dlci_ioctl(unsigned int cmd, void *arg)
{
	struct dlci_add add;
	int err;
	
	if (!capable(CAP_NET_ADMIN))
		return(-EPERM);

	if(copy_from_user(&add, arg, sizeof(struct dlci_add)))
		return -EFAULT;

	switch (cmd)
	{
		case SIOCADDDLCI:
			err = dlci_add(&add);

			if (!err)
				if(copy_to_user(arg, &add, sizeof(struct dlci_add)))
					return -EFAULT;
			break;

		case SIOCDELDLCI:
			err = dlci_del(&add);
			break;

		default:
			err = -EINVAL;
	}

	return(err);
}

int dlci_init(struct net_device *dev)
{
	struct dlci_local *dlp;

	dev->priv = kmalloc(sizeof(struct dlci_local), GFP_KERNEL);
	if (!dev->priv)
		return(-ENOMEM);

	memset(dev->priv, 0, sizeof(struct dlci_local));
	dlp = dev->priv;

	dev->flags		= 0;
	dev->open		= dlci_open;
	dev->stop		= dlci_close;
	dev->do_ioctl		= dlci_dev_ioctl;
	dev->hard_start_xmit	= dlci_transmit;
	dev->hard_header	= dlci_header;
	dev->get_stats		= dlci_get_stats;
	dev->change_mtu		= dlci_change_mtu;

	dlp->receive		= dlci_receive;

	dev->type		= ARPHRD_DLCI;
	dev->hard_header_len	= sizeof(struct frhdr);
	dev->addr_len		= sizeof(short);
	memset(dev->dev_addr, 0, sizeof(dev->dev_addr));

	return(0);
}

int __init dlci_setup(void)
{
	int i;

	printk("%s.\n", version);
	
	for(i=0;i<CONFIG_DLCI_COUNT;i++)
		open_dev[i] = NULL;

	for(i=0;i<sizeof(basename) / sizeof(char *);i++)
		basename[i] = NULL;

	return(0);
}

#ifdef MODULE

extern int (*dlci_ioctl_hook)(unsigned int, void *);

int init_module(void)
{
	dlci_ioctl_hook = dlci_ioctl;

	return(dlci_setup());
}

void cleanup_module(void)
{
	dlci_ioctl_hook = NULL;
}
#endif /* MODULE */

MODULE_AUTHOR("Mike McLagan");
MODULE_DESCRIPTION("Frame Relay DLCI layer");
MODULE_LICENSE("GPL");
