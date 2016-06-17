/******************************************************************************
 * vlanproc.c	VLAN Module. /proc filesystem interface.
 *
 *		This module is completely hardware-independent and provides
 *		access to the router using Linux /proc filesystem.
 *
 * Author:	Ben Greear, <greearb@candelatech.com> coppied from wanproc.c
 *               by: Gene Kozin	<genek@compuserve.com>
 *
 * Copyright:	(c) 1998 Ben Greear
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 * ============================================================================
 * Jan 20, 1998        Ben Greear     Initial Version
 *****************************************************************************/

#include <linux/config.h>
#include <linux/stddef.h>	/* offsetof(), etc. */
#include <linux/errno.h>	/* return codes */
#include <linux/kernel.h>
#include <linux/slab.h>		/* kmalloc(), kfree() */
#include <linux/mm.h>		/* verify_area(), etc. */
#include <linux/string.h>	/* inline mem*, str* functions */
#include <linux/init.h>		/* __initfunc et al. */
#include <asm/segment.h>	/* kernel <-> user copy */
#include <asm/byteorder.h>	/* htons(), etc. */
#include <asm/uaccess.h>	/* copy_to_user */
#include <asm/io.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/netdevice.h>
#include <linux/if_vlan.h>
#include "vlanproc.h"
#include "vlan.h"

/****** Function Prototypes *************************************************/

#ifdef CONFIG_PROC_FS

/* Proc filesystem interface */
static ssize_t vlan_proc_read(struct file *file, char *buf, size_t count,
                              loff_t *ppos);

/* Methods for preparing data for reading proc entries */

static int vlan_config_get_info(char *buf, char **start, off_t offs, int len);
static int vlandev_get_info(char *buf, char **start, off_t offs, int len);

/* Miscellaneous */

/*
 *	Global Data
 */

/*
 *	Names of the proc directory entries 
 */

static char name_root[]	 = "vlan";
static char name_conf[]	 = "config";
static char term_msg[]   = "***KERNEL:  Out of buffer space!***\n";

/*
 *	Structures for interfacing with the /proc filesystem.
 *	VLAN creates its own directory /proc/net/vlan with the folowing
 *	entries:
 *	config		device status/configuration
 *	<device>	entry for each  device
 */

/*
 *	Generic /proc/net/vlan/<file> file and inode operations 
 */

static struct file_operations vlan_fops = {
	read:	vlan_proc_read,
	ioctl: NULL, /* vlan_proc_ioctl */
};

/*
 *	/proc/net/vlan/<device> file and inode operations
 */

static struct file_operations vlandev_fops = {
	read:	vlan_proc_read,
	ioctl:	NULL, /* vlan_proc_ioctl */
};

/*
 * Proc filesystem derectory entries.
 */

/*
 *	/proc/net/vlan 
 */

static struct proc_dir_entry *proc_vlan_dir;

/*
 *	/proc/net/vlan/config 
 */

static struct proc_dir_entry *proc_vlan_conf;

/* Strings */
static char conf_hdr[] = "VLAN Dev name	 | VLAN ID\n";

/*
 *	Interface functions
 */

/*
 *	Clean up /proc/net/vlan entries
 */

void vlan_proc_cleanup(void)
{
	if (proc_vlan_conf)
		remove_proc_entry(name_conf, proc_vlan_dir);

	if (proc_vlan_dir)
		proc_net_remove(name_root);

	/* Dynamically added entries should be cleaned up as their vlan_device
	 * is removed, so we should not have to take care of it here...
	 */
}

/*
 *	Create /proc/net/vlan entries
 */

int __init vlan_proc_init(void)
{
	proc_vlan_dir = proc_mkdir(name_root, proc_net);
	if (proc_vlan_dir) {
		proc_vlan_conf = create_proc_entry(name_conf,
						   S_IFREG|S_IRUSR|S_IWUSR,
						   proc_vlan_dir);
		if (proc_vlan_conf) {
			proc_vlan_conf->proc_fops = &vlan_fops;
			proc_vlan_conf->get_info = vlan_config_get_info;
			return 0;
		}
	}
	vlan_proc_cleanup();
	return -ENOBUFS;
}

/*
 *	Add directory entry for VLAN device.
 */

int vlan_proc_add_dev (struct net_device *vlandev)
{
	struct vlan_dev_info *dev_info = VLAN_DEV_INFO(vlandev);

	if (!(vlandev->priv_flags & IFF_802_1Q_VLAN)) {
		printk(KERN_ERR
		       "ERROR:	vlan_proc_add, device -:%s:- is NOT a VLAN\n",
		       vlandev->name);
		return -EINVAL;
	}

	dev_info->dent = create_proc_entry(vlandev->name,
					   S_IFREG|S_IRUSR|S_IWUSR,
					   proc_vlan_dir);
	if (!dev_info->dent)
		return -ENOBUFS;

	dev_info->dent->proc_fops = &vlandev_fops;
	dev_info->dent->get_info = &vlandev_get_info;
	dev_info->dent->data = vlandev;

#ifdef VLAN_DEBUG
	printk(KERN_ERR "vlan_proc_add, device -:%s:- being added.\n",
	       vlandev->name);
#endif
	return 0;
}

/*
 *	Delete directory entry for VLAN device.
 */
int vlan_proc_rem_dev(struct net_device *vlandev)
{
	if (!vlandev) {
		printk(VLAN_ERR "%s: invalid argument: %p\n",
			__FUNCTION__, vlandev);
		return -EINVAL;
	}

	if (!(vlandev->priv_flags & IFF_802_1Q_VLAN)) {
		printk(VLAN_DBG "%s: invalid argument, device: %s is not a VLAN device, priv_flags: 0x%4hX.\n",
			__FUNCTION__, vlandev->name, vlandev->priv_flags);
		return -EINVAL;
	}

#ifdef VLAN_DEBUG
	printk(VLAN_DBG __FUNCTION__ ": dev: %p\n", vlandev);
#endif

	/** NOTE:  This will consume the memory pointed to by dent, it seems. */
	if (VLAN_DEV_INFO(vlandev)->dent) {
		remove_proc_entry(VLAN_DEV_INFO(vlandev)->dent->name, proc_vlan_dir);
		VLAN_DEV_INFO(vlandev)->dent = NULL;
	}

	return 0;
}

/****** Proc filesystem entry points ****************************************/

/*
 *	Read VLAN proc directory entry.
 *	This is universal routine for reading all entries in /proc/net/vlan
 *	directory.  Each directory entry contains a pointer to the 'method' for
 *	preparing data for that entry.
 *	o verify arguments
 *	o allocate kernel buffer
 *	o call get_info() to prepare data
 *	o copy data to user space
 *	o release kernel buffer
 *
 *	Return:	number of bytes copied to user space (0, if no data)
 *		<0	error
 */
static ssize_t vlan_proc_read(struct file *file, char *buf,
			      size_t count, loff_t *ppos)
{
	struct inode *inode = file->f_dentry->d_inode;
	struct proc_dir_entry *dent;
	char *page;
	int pos, offs, len;

	if (count <= 0)
		return 0;

	dent = inode->u.generic_ip;
	if ((dent == NULL) || (dent->get_info == NULL))
		return 0;

	page = kmalloc(VLAN_PROC_BUFSZ, GFP_KERNEL);
	VLAN_MEM_DBG("page malloc, addr: %p  size: %i\n",
		     page, VLAN_PROC_BUFSZ);

	if (page == NULL)
		return -ENOBUFS;

	pos = dent->get_info(page, dent->data, 0, 0);
	offs = file->f_pos;
	if (offs < pos) {
		len = min_t(int, pos - offs, count);
		if (copy_to_user(buf, (page + offs), len)) {
			kfree(page);
			return -EFAULT;
		}

		file->f_pos += len;
	} else {
		len = 0;
	}

	kfree(page);
	VLAN_FMEM_DBG("page free, addr: %p\n", page);
	return len;
}

/*
 * The following few functions build the content of /proc/net/vlan/config
 */

static int vlan_proc_get_vlan_info(char* buf, unsigned int cnt)
{
	struct net_device *vlandev = NULL;
	struct vlan_group *grp = NULL;
	int h, i;
	char *nm_type = NULL;
	struct vlan_dev_info *dev_info = NULL;

#ifdef VLAN_DEBUG
	printk(VLAN_DBG __FUNCTION__ ": cnt == %i\n", cnt);
#endif

	if (vlan_name_type == VLAN_NAME_TYPE_RAW_PLUS_VID) {
		nm_type = "VLAN_NAME_TYPE_RAW_PLUS_VID";
	} else if (vlan_name_type == VLAN_NAME_TYPE_PLUS_VID_NO_PAD) {
		nm_type = "VLAN_NAME_TYPE_PLUS_VID_NO_PAD";
	} else if (vlan_name_type == VLAN_NAME_TYPE_RAW_PLUS_VID_NO_PAD) {
		nm_type = "VLAN_NAME_TYPE_RAW_PLUS_VID_NO_PAD";
	} else if (vlan_name_type == VLAN_NAME_TYPE_PLUS_VID) {
		nm_type = "VLAN_NAME_TYPE_PLUS_VID";
	} else {
		nm_type = "UNKNOWN";
	}

	cnt += sprintf(buf + cnt, "Name-Type: %s\n", nm_type);

	spin_lock_bh(&vlan_group_lock);
	for (h = 0; h < VLAN_GRP_HASH_SIZE; h++) {
		for (grp = vlan_group_hash[h]; grp != NULL; grp = grp->next) {
			for (i = 0; i < VLAN_GROUP_ARRAY_LEN; i++) {
				vlandev = grp->vlan_devices[i];
				if (!vlandev)
					continue;

				if ((cnt + 100) > VLAN_PROC_BUFSZ) {
					if ((cnt+strlen(term_msg)) < VLAN_PROC_BUFSZ)
						cnt += sprintf(buf+cnt, "%s", term_msg);

					goto out;
				}

				dev_info = VLAN_DEV_INFO(vlandev);
				cnt += sprintf(buf + cnt, "%-15s| %d  | %s\n",
					       vlandev->name,
					       dev_info->vlan_id,
					       dev_info->real_dev->name);
			}
		}
	}
out:
	spin_unlock_bh(&vlan_group_lock);

	return cnt;
}

/*
 *	Prepare data for reading 'Config' entry.
 *	Return length of data.
 */

static int vlan_config_get_info(char *buf, char **start,
				off_t offs, int len)
{
	strcpy(buf, conf_hdr);
	return vlan_proc_get_vlan_info(buf, (unsigned int)(strlen(conf_hdr)));
}

/*
 *	Prepare data for reading <device> entry.
 *	Return length of data.
 *
 *	On entry, the 'start' argument will contain a pointer to VLAN device
 *	data space.
 */

static int vlandev_get_info(char *buf, char **start,
			    off_t offs, int len)
{
	struct net_device *vlandev = (void *) start;
	struct net_device_stats *stats = NULL;
	struct vlan_dev_info *dev_info = NULL;
	struct vlan_priority_tci_mapping *mp;
	int cnt = 0;
	int i;

	if ((vlandev == NULL) || (!(vlandev->priv_flags & IFF_802_1Q_VLAN)))
		return 0;

	dev_info = VLAN_DEV_INFO(vlandev);

	cnt += sprintf(buf + cnt, "%s  VID: %d	 REORDER_HDR: %i  dev->priv_flags: %hx\n",
		       vlandev->name, dev_info->vlan_id,
		       (int)(dev_info->flags & 1), vlandev->priv_flags);

	stats = vlan_dev_get_stats(vlandev);

	cnt += sprintf(buf + cnt, "%30s: %12lu\n",
		       "total frames received", stats->rx_packets);

	cnt += sprintf(buf + cnt, "%30s: %12lu\n",
		       "total bytes received", stats->rx_bytes);

	cnt += sprintf(buf + cnt, "%30s: %12lu\n",
		       "Broadcast/Multicast Rcvd", stats->multicast);

	cnt += sprintf(buf + cnt, "\n%30s: %12lu\n",
		       "total frames transmitted", stats->tx_packets);

	cnt += sprintf(buf + cnt, "%30s: %12lu\n",
		       "total bytes transmitted", stats->tx_bytes);

	cnt += sprintf(buf + cnt, "%30s: %12lu\n",
		       "total headroom inc", dev_info->cnt_inc_headroom_on_tx);

	cnt += sprintf(buf + cnt, "%30s: %12lu\n",
		       "total encap on xmit", dev_info->cnt_encap_on_xmit);

	cnt += sprintf(buf + cnt, "Device: %s", dev_info->real_dev->name);

	/* now show all PRIORITY mappings relating to this VLAN */
	cnt += sprintf(buf + cnt, "\nINGRESS priority mappings: 0:%lu  1:%lu  2:%lu  3:%lu  4:%lu  5:%lu  6:%lu 7:%lu\n",
		       dev_info->ingress_priority_map[0],
		       dev_info->ingress_priority_map[1],
		       dev_info->ingress_priority_map[2],
		       dev_info->ingress_priority_map[3],
		       dev_info->ingress_priority_map[4],
		       dev_info->ingress_priority_map[5],
		       dev_info->ingress_priority_map[6],
		       dev_info->ingress_priority_map[7]);

	if ((cnt + 100) > VLAN_PROC_BUFSZ) {
		if ((cnt + strlen(term_msg)) >= VLAN_PROC_BUFSZ) {
			/* should never get here */
			return cnt;
		} else {
			cnt += sprintf(buf + cnt, "%s", term_msg);
			return cnt;
		}
	}

	cnt += sprintf(buf + cnt, "EGRESSS priority Mappings: ");

	for (i = 0; i < 16; i++) {
		mp = dev_info->egress_priority_map[i];
		while (mp) {
			cnt += sprintf(buf + cnt, "%lu:%hu ",
				       mp->priority, ((mp->vlan_qos >> 13) & 0x7));

			if ((cnt + 100) > VLAN_PROC_BUFSZ) {
				if ((cnt + strlen(term_msg)) >= VLAN_PROC_BUFSZ) {
					/* should never get here */
					return cnt;
				} else {
					cnt += sprintf(buf + cnt, "%s", term_msg);
					return cnt;
				}
			}
			mp = mp->next;
		}
	}

	cnt += sprintf(buf + cnt, "\n");

	return cnt;
}

#else /* No CONFIG_PROC_FS */

/*
 *	No /proc - output stubs
 */
 
int __init vlan_proc_init (void)
{
	return 0;
}

void vlan_proc_cleanup(void)
{
	return;
}


int vlan_proc_add_dev(struct net_device *vlandev)
{
	return 0;
}

int vlan_proc_rem_dev(struct net_device *vlandev)
{
	return 0;
}

#endif /* No CONFIG_PROC_FS */
