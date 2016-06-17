/* $Id: ifconfig_net.c,v 1.1 2002/02/28 17:31:25 marcelo Exp $
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 *  ifconfig_net - SGI's Persistent Network Device names.
 *
 * Copyright (C) 1992-1997, 2000-2003 Silicon Graphics, Inc.  All rights reserved.
 */

#include <linux/types.h>
#include <linux/config.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include <asm/sn/sgi.h>
#include <asm/uaccess.h>
#include <linux/devfs_fs.h>
#include <linux/devfs_fs_kernel.h>
#include <asm/io.h>
#include <asm/sn/iograph.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/labelcl.h>
#include <asm/sn/ifconfig_net.h>

#define SGI_IFCONFIG_NET "SGI-PERSISTENT NETWORK DEVICE NAME DRIVER"
#define SGI_IFCONFIG_NET_VERSION "1.0"

/*
 * Some Global definitions.
 */
static vertex_hdl_t ifconfig_net_handle;
static unsigned long ifconfig_net_debug;
static struct ifname_MAC *new_devices;
static struct ifname_num *ifname_num;


/*
 * ifconfig_net_open - Opens the special device node "/devhw/.ifconfig_net".
 */
static int ifconfig_net_open(struct inode * inode, struct file * filp)
{
	if (ifconfig_net_debug) {
        	printk("ifconfig_net_open called.\n");
	}

        return(0);

}

/*
 * ifconfig_net_close - Closes the special device node "/devhw/.ifconfig_net".
 */
static int ifconfig_net_close(struct inode * inode, struct file * filp)
{

	if (ifconfig_net_debug) {
        	printk("ifconfig_net_close called.\n");
	}

        return(0);
}

/*
 * assign_ifname - Assign the next available interface name from the persistent list.
 */
void
assign_ifname(struct net_device *dev,
		  struct ifname_num *ifname_num)

{

	/*
	 * Handle eth devices.
	 */
        if ( (memcmp(dev->name, "eth", 3) == 0) ) {
		if (ifname_num->next_eth != -1) {
			/*
			 * Assign it the next available eth interface number. 
			 */
			memset(dev->name, 0, strlen(dev->name));
			sprintf(dev->name, "eth%d", (int)ifname_num->next_eth);
			ifname_num->next_eth++;
		} 

                return;
        }

	/*
	 * Handle fddi devices.
	 */
	if ( (memcmp(dev->name, "fddi", 4) == 0) ) {
		if (ifname_num->next_fddi != -1) {
			/*
			 * Assign it the next available fddi interface number.
			 */
			memset(dev->name, 0, strlen(dev->name));
			sprintf(dev->name, "fddi%d", (int)ifname_num->next_fddi);
			ifname_num->next_fddi++;
		}

		return;
	}

	/*
	 * Handle hip devices.
	 */
	if ( (memcmp(dev->name, "hip", 3) == 0) ) {
		if (ifname_num->next_hip != -1) {
			/*
			 * Assign it the next available hip interface number.
			 */
			memset(dev->name, 0, strlen(dev->name));
			sprintf(dev->name, "hip%d", (int)ifname_num->next_hip);
			ifname_num->next_hip++;
		}

		return;
	}

	/*
	 * Handle tr devices.
	 */
	if ( (memcmp(dev->name, "tr", 2) == 0) ) {
		if (ifname_num->next_tr != -1) {
			/*
			 * Assign it the next available tr interface number.
			 */
			memset(dev->name, 0, strlen(dev->name));
			sprintf(dev->name, "tr%d", (int)ifname_num->next_tr);
			ifname_num->next_tr++;
		}

		return;
	}

	/*
	 * Handle fc devices.
	 */
	if ( (memcmp(dev->name, "fc", 2) == 0) ) {
		if (ifname_num->next_fc != -1) {
			/*
			 * Assign it the next available fc interface number.
			 */
			memset(dev->name, 0, strlen(dev->name));
			sprintf(dev->name, "fc%d", (int)ifname_num->next_fc);
			ifname_num->next_fc++;
		}

		return;
	}
}

/*
 * find_persistent_ifname: Returns the entry that was seen in previous boot.
 */
struct ifname_MAC *
find_persistent_ifname(struct net_device *dev,
	struct ifname_MAC *ifname_MAC)

{

	while (ifname_MAC->addr_len) {
		if (memcmp(dev->dev_addr, ifname_MAC->dev_addr, dev->addr_len) == 0)
			return(ifname_MAC);

		ifname_MAC++;
	}

	return(NULL);
}

/*
 * ifconfig_net_ioctl: ifconfig_net driver ioctl interface.
 */
static int ifconfig_net_ioctl(struct inode * inode, struct file * file,
        unsigned int cmd, unsigned long arg)
{

	extern struct net_device *__dev_get_by_name(const char *);
#ifdef CONFIG_NET
	struct net_device *dev;
	struct ifname_MAC *found;
	char temp[64];
#endif
	struct ifname_MAC *ifname_MAC;
	struct ifname_MAC *temp_new_devices;
	unsigned long size;


	if (ifconfig_net_debug) {
		printk("HCL: hcl_ioctl called.\n");
	}

	/*
	 * Read in the header and see how big of a buffer we really need to 
	 * allocate.
	 */
	ifname_num = (struct ifname_num *) kmalloc(sizeof(struct ifname_num), 
			GFP_KERNEL);
	if (copy_from_user( ifname_num, (char *) arg, sizeof(struct ifname_num)))
		return -EFAULT;
	size = ifname_num->size;
	kfree(ifname_num);
	ifname_num = (struct ifname_num *) kmalloc(size, GFP_KERNEL);
	if (ifname_num <= 0)
		return -ENOMEM;
	ifname_MAC = (struct ifname_MAC *) ((char *)ifname_num + (sizeof(struct ifname_num)) );

	if (copy_from_user( ifname_num, (char *) arg, size)){
		kfree(ifname_num);
		return -EFAULT;
	}
	new_devices =  kmalloc(size - sizeof(struct ifname_num), GFP_KERNEL);
	if (new_devices <= 0){
		kfree(ifname_num);
		return -ENOMEM;
	}
	temp_new_devices = new_devices;

	memset(new_devices, 0, size - sizeof(struct ifname_num));

#ifdef CONFIG_NET
	/*
	 * Go through the net device entries and make them persistent!
	 */
	for (dev = dev_base; dev != NULL; dev = dev->next) {
		/*
		 * Skip NULL entries or "lo"
		 */
		if ( (dev->addr_len == 0) || ( !strncmp(dev->name, "lo", strlen(dev->name))) ){
			continue;
		}

		/*
		 * See if we have a persistent interface name for this device.
		 */
		found = NULL;
		found = find_persistent_ifname(dev, ifname_MAC);
		if (found) {
			strcpy(dev->name, found->name);
		} else {
			/* Never seen this before .. */
			assign_ifname(dev, ifname_num);

			/* 
			 * Save the information for the next boot.
			 */
 			sprintf(temp,"%s %02x:%02x:%02x:%02x:%02x:%02x\n", dev->name,
				dev->dev_addr[0],  dev->dev_addr[1],  dev->dev_addr[2],
				dev->dev_addr[3],  dev->dev_addr[4],  dev->dev_addr[5]);
			strcpy(temp_new_devices->name, dev->name);
			temp_new_devices->addr_len = dev->addr_len;
			memcpy(temp_new_devices->dev_addr, dev->dev_addr, dev->addr_len);
			temp_new_devices++;
		}
		
	}
#endif

	/*
	 * Copy back to the User Buffer area any new devices encountered.
	 */
	kfree(new_devices);
	return copy_to_user((char *)arg + (sizeof(struct ifname_num)), new_devices, 
			size - sizeof(struct ifname_num))?-EFAULT:0;


}

struct file_operations ifconfig_net_fops = {
	ioctl:ifconfig_net_ioctl,	/* ioctl */
	open:ifconfig_net_open,		/* open */
	release:ifconfig_net_close	/* release */
};


/*
 * init_ifconfig_net() - Boot time initialization.  Ensure that it is called 
 *	after devfs has been initialized.
 *
 */
#ifdef MODULE
int init_module (void)
#else
int __init init_ifconfig_net(void)
#endif
{
	ifconfig_net_handle = NULL;
	ifconfig_net_handle = hwgraph_register(hwgraph_root, ".ifconfig_net",
			0, DEVFS_FL_AUTO_DEVNUM,
			0, 0,
			S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP, 0, 0,
			&ifconfig_net_fops, NULL);

	if (ifconfig_net_handle == NULL) {
		panic("Unable to create SGI PERSISTENT NETWORK DEVICE Name Driver.\n");
	}

	return(0);

}
