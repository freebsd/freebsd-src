/* -*- linux-c -*-
 * INET		802.1Q VLAN
 *		Ethernet-type device handling.
 *
 * Authors:	Ben Greear <greearb@candelatech.com>
 *              Please send support related email to: vlan@scry.wanfear.com
 *              VLAN Home Page: http://www.candelatech.com/~greear/vlan.html
 * 
 * Fixes:
 *              Fix for packet capture - Nick Eggleston <nick@dccinc.com>;
 *		Add HW acceleration hooks - David S. Miller <davem@redhat.com>;
 *		Correct all the locking - David S. Miller <davem@redhat.com>;
 *		Use hash table for VLAN groups - David S. Miller <davem@redhat.com>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */

#include <asm/uaccess.h> /* for copy_from_user */
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/datalink.h>
#include <linux/mm.h>
#include <linux/in.h>
#include <linux/init.h>
#include <net/p8022.h>
#include <net/arp.h>
#include <linux/rtnetlink.h>
#include <linux/brlock.h>
#include <linux/notifier.h>

#include <linux/if_vlan.h>
#include "vlan.h"
#include "vlanproc.h"

/* Global VLAN variables */

/* Our listing of VLAN group(s) */
struct vlan_group *vlan_group_hash[VLAN_GRP_HASH_SIZE];
spinlock_t vlan_group_lock = SPIN_LOCK_UNLOCKED;
#define vlan_grp_hashfn(IDX)	((((IDX) >> VLAN_GRP_HASH_SHIFT) ^ (IDX)) & VLAN_GRP_HASH_MASK)

static char vlan_fullname[] = "802.1Q VLAN Support";
static unsigned int vlan_version = 1;
static unsigned int vlan_release = 8;
static char vlan_copyright[] = "Ben Greear <greearb@candelatech.com>";
static char vlan_buggyright[] = "David S. Miller <davem@redhat.com>";

static int vlan_device_event(struct notifier_block *, unsigned long, void *);

struct notifier_block vlan_notifier_block = {
	notifier_call: vlan_device_event,
};

/* These may be changed at run-time through IOCTLs */

/* Determines interface naming scheme. */
unsigned short vlan_name_type = VLAN_NAME_TYPE_RAW_PLUS_VID_NO_PAD;

/* DO reorder the header by default */
unsigned short vlan_default_dev_flags = 1;

static struct packet_type vlan_packet_type = {
	type: __constant_htons(ETH_P_8021Q),
	dev:  NULL,
	func: vlan_skb_recv, /* VLAN receive method */
	data: (void *)(-1),  /* Set here '(void *)1' when this code can SHARE SKBs */
	next: NULL
};

/* End of global variables definitions. */

/*
 * Function vlan_proto_init (pro)
 *
 *    Initialize VLAN protocol layer, 
 *
 */
static int __init vlan_proto_init(void)
{
	int err;

	printk(VLAN_INF "%s v%u.%u %s\n",
	       vlan_fullname, vlan_version, vlan_release, vlan_copyright);
	printk(VLAN_INF "All bugs added by %s\n",
	       vlan_buggyright);

	/* proc file system initialization */
	err = vlan_proc_init();
	if (err < 0) {
		printk(KERN_ERR 
		       "%s %s: can't create entry in proc filesystem!\n",
		       __FUNCTION__, VLAN_NAME);
		return 1;
	}

	dev_add_pack(&vlan_packet_type);

	/* Register us to receive netdevice events */
	register_netdevice_notifier(&vlan_notifier_block);

	vlan_ioctl_hook = vlan_ioctl_handler;

	return 0;
}

/*
 *     Module 'remove' entry point.
 *     o delete /proc/net/router directory and static entries.
 */ 
static void __exit vlan_cleanup_module(void)
{
	int i;

	/* This table must be empty if there are no module
	 * references left.
	 */
	for (i = 0; i < VLAN_GRP_HASH_SIZE; i++) {
		if (vlan_group_hash[i] != NULL)
			BUG();
	}

	/* Un-register us from receiving netdevice events */
	unregister_netdevice_notifier(&vlan_notifier_block);

	dev_remove_pack(&vlan_packet_type);
	vlan_proc_cleanup();
	vlan_ioctl_hook = NULL;
}

module_init(vlan_proto_init);
module_exit(vlan_cleanup_module);

/* Must be invoked with vlan_group_lock held. */
static struct vlan_group *__vlan_find_group(int real_dev_ifindex)
{
	struct vlan_group *grp;

	for (grp = vlan_group_hash[vlan_grp_hashfn(real_dev_ifindex)];
	     grp != NULL;
	     grp = grp->next) {
		if (grp->real_dev_ifindex == real_dev_ifindex)
			break;
	}

	return grp;
}

/* Must hold vlan_group_lock. */
static void __grp_hash(struct vlan_group *grp)
{
	struct vlan_group **head;

	head = &vlan_group_hash[vlan_grp_hashfn(grp->real_dev_ifindex)];
	grp->next = *head;
	*head = grp;
}

/* Must hold vlan_group_lock. */
static void __grp_unhash(struct vlan_group *grp)
{
	struct vlan_group *next, **pprev;

	pprev = &vlan_group_hash[vlan_grp_hashfn(grp->real_dev_ifindex)];
	next = *pprev;
	while (next != grp) {
		pprev = &next->next;
		next = *pprev;
	}
	*pprev = grp->next;
}

/*  Find the protocol handler.  Assumes VID < VLAN_VID_MASK.
 *
 * Must be invoked with vlan_group_lock held.
 */
struct net_device *__find_vlan_dev(struct net_device *real_dev,
				   unsigned short VID)
{
	struct vlan_group *grp = __vlan_find_group(real_dev->ifindex);

	if (grp)
                return grp->vlan_devices[VID];

	return NULL;
}

/* This returns 0 if everything went fine.
 * It will return 1 if the group was killed as a result.
 * A negative return indicates failure.
 *
 * The RTNL lock must be held.
 */
static int unregister_vlan_dev(struct net_device *real_dev,
			       unsigned short vlan_id)
{
	struct net_device *dev = NULL;
	int real_dev_ifindex = real_dev->ifindex;
	struct vlan_group *grp;
	int i, ret;

#ifdef VLAN_DEBUG
	printk(VLAN_DBG "%s: VID: %i\n", __FUNCTION__, vlan_id);
#endif

	/* sanity check */
	if (vlan_id >= VLAN_VID_MASK)
		return -EINVAL;

	spin_lock_bh(&vlan_group_lock);
	grp = __vlan_find_group(real_dev_ifindex);
	spin_unlock_bh(&vlan_group_lock);

	ret = 0;

	if (grp) {
		dev = grp->vlan_devices[vlan_id];
		if (dev) {
			/* Remove proc entry */
			vlan_proc_rem_dev(dev);

			/* Take it out of our own structures, but be sure to
			 * interlock with HW accelerating devices or SW vlan
			 * input packet processing.
			 */
			if (real_dev->features &
			    (NETIF_F_HW_VLAN_RX | NETIF_F_HW_VLAN_FILTER)) {
				real_dev->vlan_rx_kill_vid(real_dev, vlan_id);
			}

			br_write_lock(BR_NETPROTO_LOCK);
			grp->vlan_devices[vlan_id] = NULL;
			br_write_unlock(BR_NETPROTO_LOCK);


			/* Caller unregisters (and if necessary, puts)
			 * VLAN device, but we get rid of the reference to
			 * real_dev here.
			 */
			dev_put(real_dev);

			/* If the group is now empty, kill off the
			 * group.
			 */
			for (i = 0; i < VLAN_VID_MASK; i++)
				if (grp->vlan_devices[i])
					break;

			if (i == VLAN_VID_MASK) {
				if (real_dev->features & NETIF_F_HW_VLAN_RX)
					real_dev->vlan_rx_register(real_dev, NULL);

				spin_lock_bh(&vlan_group_lock);
				__grp_unhash(grp);
				spin_unlock_bh(&vlan_group_lock);

				/* Free the group, after we have removed it
				 * from the hash.
				 */
				kfree(grp);
				grp = NULL;

				ret = 1;
			}

			MOD_DEC_USE_COUNT;
		}
	}

        return ret;
}

static int unregister_vlan_device(const char *vlan_IF_name)
{
	struct net_device *dev = NULL;
	int ret;


	dev = dev_get_by_name(vlan_IF_name);
	ret = -EINVAL;
	if (dev) {
		if (dev->priv_flags & IFF_802_1Q_VLAN) {
			rtnl_lock();

			ret = unregister_vlan_dev(VLAN_DEV_INFO(dev)->real_dev,
						  VLAN_DEV_INFO(dev)->vlan_id);

			dev_put(dev);
			unregister_netdevice(dev);

			rtnl_unlock();

			if (ret == 1)
				ret = 0;
		} else {
			printk(VLAN_ERR 
			       "%s: ERROR:	Tried to remove a non-vlan device "
			       "with VLAN code, name: %s  priv_flags: %hX\n",
			       __FUNCTION__, dev->name, dev->priv_flags);
			dev_put(dev);
			ret = -EPERM;
		}
	} else {
#ifdef VLAN_DEBUG
		printk(VLAN_DBG "%s: WARNING: Could not find dev.\n", __FUNCTION__);
#endif
		ret = -EINVAL;
	}

	return ret;
}

/*  Attach a VLAN device to a mac address (ie Ethernet Card).
 *  Returns the device that was created, or NULL if there was
 *  an error of some kind.
 */
static struct net_device *register_vlan_device(const char *eth_IF_name,
					       unsigned short VLAN_ID)
{
	struct vlan_group *grp;
	struct net_device *new_dev;
	struct net_device *real_dev; /* the ethernet device */
	int malloc_size = 0;
	int r;

#ifdef VLAN_DEBUG
	printk(VLAN_DBG "%s: if_name -:%s:-	vid: %i\n",
		__FUNCTION__, eth_IF_name, VLAN_ID);
#endif

	if (VLAN_ID >= VLAN_VID_MASK)
		goto out_ret_null;

	/* find the device relating to eth_IF_name. */
	real_dev = dev_get_by_name(eth_IF_name);
	if (!real_dev)
		goto out_ret_null;

	if (real_dev->features & NETIF_F_VLAN_CHALLENGED) {
		printk(VLAN_DBG "%s: VLANs not supported on %s.\n",
			__FUNCTION__, real_dev->name);
		goto out_put_dev;
	}

	if ((real_dev->features & NETIF_F_HW_VLAN_RX) &&
	    (real_dev->vlan_rx_register == NULL ||
	     real_dev->vlan_rx_kill_vid == NULL)) {
		printk(VLAN_DBG "%s: Device %s has buggy VLAN hw accel.\n",
			__FUNCTION__, real_dev->name);
		goto out_put_dev;
	}

	if ((real_dev->features & NETIF_F_HW_VLAN_FILTER) &&
	    (real_dev->vlan_rx_add_vid == NULL ||
	     real_dev->vlan_rx_kill_vid == NULL)) {
		printk(VLAN_DBG "%s: Device %s has buggy VLAN hw accel.\n",
			__FUNCTION__, real_dev->name);
		goto out_put_dev;
	}

	/* From this point on, all the data structures must remain
	 * consistent.
	 */
	rtnl_lock();

	/* The real device must be up and operating in order to
	 * assosciate a VLAN device with it.
	 */
	if (!(real_dev->flags & IFF_UP))
		goto out_unlock;

	spin_lock_bh(&vlan_group_lock);
	r = (__find_vlan_dev(real_dev, VLAN_ID) != NULL);
	spin_unlock_bh(&vlan_group_lock);

	if (r) {
		/* was already registered. */
		printk(VLAN_DBG "%s: ALREADY had VLAN registered\n", __FUNCTION__);
		goto out_unlock;
	}

	malloc_size = (sizeof(struct net_device));
	new_dev = (struct net_device *) kmalloc(malloc_size, GFP_KERNEL);
	VLAN_MEM_DBG("net_device malloc, addr: %p  size: %i\n",
		     new_dev, malloc_size);

	if (new_dev == NULL)
		goto out_unlock;

	memset(new_dev, 0, malloc_size);

	/* Set us up to have no queue, as the underlying Hardware device
	 * can do all the queueing we could want.
	 */
	new_dev->tx_queue_len = 0;

	/* Gotta set up the fields for the device. */
#ifdef VLAN_DEBUG
	printk(VLAN_DBG "About to allocate name, vlan_name_type: %i\n",
	       vlan_name_type);
#endif
	switch (vlan_name_type) {
	case VLAN_NAME_TYPE_RAW_PLUS_VID:
		/* name will look like:	 eth1.0005 */
		sprintf(new_dev->name, "%s.%.4i", real_dev->name, VLAN_ID);
		break;
	case VLAN_NAME_TYPE_PLUS_VID_NO_PAD:
		/* Put our vlan.VID in the name.
		 * Name will look like:	 vlan5
		 */
		sprintf(new_dev->name, "vlan%i", VLAN_ID);
		break;
	case VLAN_NAME_TYPE_RAW_PLUS_VID_NO_PAD:
		/* Put our vlan.VID in the name.
		 * Name will look like:	 eth0.5
		 */
		sprintf(new_dev->name, "%s.%i", real_dev->name, VLAN_ID);
		break;
	case VLAN_NAME_TYPE_PLUS_VID:
		/* Put our vlan.VID in the name.
		 * Name will look like:	 vlan0005
		 */
	default:
		sprintf(new_dev->name, "vlan%.4i", VLAN_ID);
	};
		    
#ifdef VLAN_DEBUG
	printk(VLAN_DBG "Allocated new name -:%s:-\n", new_dev->name);
#endif
	/* set up method calls */
	new_dev->init = vlan_dev_init;
	new_dev->destructor = vlan_dev_destruct;
	new_dev->features |= NETIF_F_DYNALLOC ; 
	    
	/* new_dev->ifindex = 0;  it will be set when added to
	 * the global list.
	 * iflink is set as well.
	 */
	new_dev->get_stats = vlan_dev_get_stats;
	    
	/* IFF_BROADCAST|IFF_MULTICAST; ??? */
	new_dev->flags = real_dev->flags;
	new_dev->flags &= ~IFF_UP;

	/* Make this thing known as a VLAN device */
	new_dev->priv_flags |= IFF_802_1Q_VLAN;
				
	/* need 4 bytes for extra VLAN header info,
	 * hope the underlying device can handle it.
	 */
	new_dev->mtu = real_dev->mtu;
	new_dev->change_mtu = vlan_dev_change_mtu;

	/* TODO: maybe just assign it to be ETHERNET? */
	new_dev->type = real_dev->type;

	new_dev->hard_header_len = real_dev->hard_header_len;
	if (!(real_dev->features & NETIF_F_HW_VLAN_TX)) {
		/* Regular ethernet + 4 bytes (18 total). */
		new_dev->hard_header_len += VLAN_HLEN;
	}

	new_dev->priv = kmalloc(sizeof(struct vlan_dev_info),
				GFP_KERNEL);
	VLAN_MEM_DBG("new_dev->priv malloc, addr: %p  size: %i\n",
		     new_dev->priv,
		     sizeof(struct vlan_dev_info));
	    
	if (new_dev->priv == NULL)
		goto out_free_newdev;

	memset(new_dev->priv, 0, sizeof(struct vlan_dev_info));

	memcpy(new_dev->broadcast, real_dev->broadcast, real_dev->addr_len);
	memcpy(new_dev->dev_addr, real_dev->dev_addr, real_dev->addr_len);
	new_dev->addr_len = real_dev->addr_len;

	new_dev->open = vlan_dev_open;
	new_dev->stop = vlan_dev_stop;

	if (real_dev->features & NETIF_F_HW_VLAN_TX) {
		new_dev->hard_header = real_dev->hard_header;
		new_dev->hard_start_xmit = vlan_dev_hwaccel_hard_start_xmit;
		new_dev->rebuild_header = real_dev->rebuild_header;
	} else {
		new_dev->hard_header = vlan_dev_hard_header;
		new_dev->hard_start_xmit = vlan_dev_hard_start_xmit;
		new_dev->rebuild_header = vlan_dev_rebuild_header;
	}
	new_dev->hard_header_parse = real_dev->hard_header_parse;
	new_dev->set_mac_address = vlan_dev_set_mac_address;
	new_dev->set_multicast_list = vlan_dev_set_multicast_list;

	VLAN_DEV_INFO(new_dev)->vlan_id = VLAN_ID; /* 1 through VLAN_VID_MASK */
	VLAN_DEV_INFO(new_dev)->real_dev = real_dev;
	VLAN_DEV_INFO(new_dev)->dent = NULL;
	VLAN_DEV_INFO(new_dev)->flags = vlan_default_dev_flags;

#ifdef VLAN_DEBUG
	printk(VLAN_DBG "About to go find the group for idx: %i\n",
	       real_dev->ifindex);
#endif
	    
	/* So, got the sucker initialized, now lets place
	 * it into our local structure.
	 */
	spin_lock_bh(&vlan_group_lock);
	grp = __vlan_find_group(real_dev->ifindex);
	spin_unlock_bh(&vlan_group_lock);

	/* Note, we are running under the RTNL semaphore
	 * so it cannot "appear" on us.
	 */
	if (!grp) { /* need to add a new group */
		grp = kmalloc(sizeof(struct vlan_group), GFP_KERNEL);
		if (!grp)
			goto out_free_newdev_priv;
					
		/* printk(KERN_ALERT "VLAN REGISTER:  Allocated new group.\n"); */
		memset(grp, 0, sizeof(struct vlan_group));
		grp->real_dev_ifindex = real_dev->ifindex;

		spin_lock_bh(&vlan_group_lock);
		__grp_hash(grp);
		spin_unlock_bh(&vlan_group_lock);

		if (real_dev->features & NETIF_F_HW_VLAN_RX)
			real_dev->vlan_rx_register(real_dev, grp);
	}
	    
	grp->vlan_devices[VLAN_ID] = new_dev;

	if (vlan_proc_add_dev(new_dev)<0)/* create it's proc entry */
            	printk(KERN_WARNING "VLAN: failed to add proc entry for %s\n",
					                 new_dev->name);

	if (real_dev->features & NETIF_F_HW_VLAN_FILTER)
		real_dev->vlan_rx_add_vid(real_dev, VLAN_ID);

	register_netdevice(new_dev);

	rtnl_unlock();
	    
	/* NOTE:  We have a reference to the real device,
	 * so hold on to the reference.
	 */
	MOD_INC_USE_COUNT; /* Add was a success!! */
#ifdef VLAN_DEBUG
	printk(VLAN_DBG "Allocated new device successfully, returning.\n");
#endif
	return new_dev;

out_free_newdev_priv:
	kfree(new_dev->priv);

out_free_newdev:
	kfree(new_dev);

out_unlock:
	rtnl_unlock();

out_put_dev:
	dev_put(real_dev);

out_ret_null:
	return NULL;
}

static int vlan_device_event(struct notifier_block *unused, unsigned long event, void *ptr)
{
	struct net_device *dev = (struct net_device *)(ptr);
	struct vlan_group *grp = NULL;
	int i, flgs;
	struct net_device *vlandev = NULL;

	spin_lock_bh(&vlan_group_lock);
	grp = __vlan_find_group(dev->ifindex);
	spin_unlock_bh(&vlan_group_lock);

	if (!grp)
		goto out;

	/* It is OK that we do not hold the group lock right now,
	 * as we run under the RTNL lock.
	 */

	switch (event) {
	case NETDEV_CHANGEADDR:
	case NETDEV_GOING_DOWN:
		/* Ignore for now */
		break;

	case NETDEV_DOWN:
		/* Put all VLANs for this dev in the down state too.  */
		for (i = 0; i < VLAN_GROUP_ARRAY_LEN; i++) {
			vlandev = grp->vlan_devices[i];
			if (!vlandev)
				continue;

			flgs = vlandev->flags;
			if (!(flgs & IFF_UP))
				continue;

			dev_change_flags(vlandev, flgs & ~IFF_UP);
		}
		break;

	case NETDEV_UP:
		/* Put all VLANs for this dev in the up state too.  */
		for (i = 0; i < VLAN_GROUP_ARRAY_LEN; i++) {
			vlandev = grp->vlan_devices[i];
			if (!vlandev)
				continue;
				
			flgs = vlandev->flags;
			if (flgs & IFF_UP)
				continue;

			dev_change_flags(vlandev, flgs | IFF_UP);
		}
		break;
		
	case NETDEV_UNREGISTER:
		/* Delete all VLANs for this dev. */
		for (i = 0; i < VLAN_GROUP_ARRAY_LEN; i++) {
			int ret;

			vlandev = grp->vlan_devices[i];
			if (!vlandev)
				continue;

			ret = unregister_vlan_dev(dev,
						  VLAN_DEV_INFO(vlandev)->vlan_id);

			dev_put(vlandev);
			unregister_netdevice(vlandev);

			/* Group was destroyed? */
			if (ret == 1)
				break;
		}
		break;
	};

out:
	return NOTIFY_DONE;
}

/*
 *	VLAN IOCTL handler.
 *	o execute requested action or pass command to the device driver
 *   arg is really a void* to a vlan_ioctl_args structure.
 */
int vlan_ioctl_handler(unsigned long arg)
{
	int err = 0;
        unsigned short vid = 0;
	struct vlan_ioctl_args args;

	if (copy_from_user(&args, (void*)arg,
                           sizeof(struct vlan_ioctl_args)))
		return -EFAULT;

	/* Null terminate this sucker, just in case. */
	args.device1[23] = 0;
	args.u.device2[23] = 0;

#ifdef VLAN_DEBUG
	printk(VLAN_DBG "%s: args.cmd: %x\n", __FUNCTION__, args.cmd);
#endif

	switch (args.cmd) {
	case SET_VLAN_INGRESS_PRIORITY_CMD:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		err = vlan_dev_set_ingress_priority(args.device1,
						    args.u.skb_priority,
						    args.vlan_qos);
		break;

	case SET_VLAN_EGRESS_PRIORITY_CMD:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		err = vlan_dev_set_egress_priority(args.device1,
						   args.u.skb_priority,
						   args.vlan_qos);
		break;

	case SET_VLAN_FLAG_CMD:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		err = vlan_dev_set_vlan_flag(args.device1,
					     args.u.flag,
					     args.vlan_qos);
		break;

	case SET_VLAN_NAME_TYPE_CMD:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		if ((args.u.name_type >= 0) &&
		    (args.u.name_type < VLAN_NAME_TYPE_HIGHEST)) {
			vlan_name_type = args.u.name_type;
			err = 0;
		} else {
			err = -EINVAL;
		}
		break;

	case ADD_VLAN_CMD:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		/* we have been given the name of the Ethernet Device we want to
		 * talk to:  args.dev1	 We also have the
		 * VLAN ID:  args.u.VID
		 */
		if (register_vlan_device(args.device1, args.u.VID)) {
			err = 0;
		} else {
			err = -EINVAL;
		}
		break;

	case DEL_VLAN_CMD:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		/* Here, the args.dev1 is the actual VLAN we want
		 * to get rid of.
		 */
		err = unregister_vlan_device(args.device1);
		break;

	case GET_VLAN_INGRESS_PRIORITY_CMD:
		/* TODO:  Implement
		err = vlan_dev_get_ingress_priority(args);
		if (copy_to_user((void*)arg, &args,
				 sizeof(struct vlan_ioctl_args))) {
			err = -EFAULT;
		}
		*/
		err = -EINVAL;
		break;

	case GET_VLAN_EGRESS_PRIORITY_CMD:
		/* TODO:  Implement
		err = vlan_dev_get_egress_priority(args.device1, &(args.args);
		if (copy_to_user((void*)arg, &args,
				 sizeof(struct vlan_ioctl_args))) {
			err = -EFAULT;
		}
		*/
		err = -EINVAL;
		break;

	case GET_VLAN_REALDEV_NAME_CMD:
		err = vlan_dev_get_realdev_name(args.device1, args.u.device2);
		if (copy_to_user((void*)arg, &args,
				 sizeof(struct vlan_ioctl_args))) {
			err = -EFAULT;
		}
		break;

	case GET_VLAN_VID_CMD:
		err = vlan_dev_get_vid(args.device1, &vid);
		args.u.VID = vid;
		if (copy_to_user((void*)arg, &args,
				 sizeof(struct vlan_ioctl_args))) {
			err = -EFAULT;
		}
		break;
		
	default:
		/* pass on to underlying device instead?? */
		printk(VLAN_DBG "%s: Unknown VLAN CMD: %x \n",
			__FUNCTION__, args.cmd);
		return -EINVAL;
	};

	return err;
}

MODULE_LICENSE("GPL");
