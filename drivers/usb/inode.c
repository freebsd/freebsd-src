/*****************************************************************************/

/*
 *	inode.c  --  Inode/Dentry functions for the USB device file system.
 *
 *	Copyright (C) 2000
 *          Thomas Sailer (sailer@ife.ee.ethz.ch)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  $Id: inode.c,v 1.3 2000/01/11 13:58:25 tom Exp $
 *
 *  History:
 *   0.1  04.01.2000  Created
 */

/*****************************************************************************/

#define __NO_VERSION__
#include <linux/config.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/smp_lock.h>
#include <linux/locks.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/usb.h>
#include <linux/usbdevice_fs.h>
#include <asm/uaccess.h>

/* --------------------------------------------------------------------- */

/*
 * This list of superblocks is still used,
 * but since usbdevfs became FS_SINGLE
 * there is only one super_block.
 */
static LIST_HEAD(superlist);

struct special {
	const char *name;
	struct file_operations *fops;
	struct inode *inode;
	struct list_head inodes;
};

static struct special special[] = { 
	{ "devices", &usbdevfs_devices_fops,  },
	{ "drivers", &usbdevfs_drivers_fops,  }
};

#define NRSPECIAL (sizeof(special)/sizeof(special[0]))

/* --------------------------------------------------------------------- */

static int dnumber(struct dentry *dentry)
{
	const char *name;
	unsigned int s;

	if (dentry->d_name.len != 3)
		return -1;
	name = dentry->d_name.name;
	if (name[0] < '0' || name[0] > '9' ||
	    name[1] < '0' || name[1] > '9' ||
	    name[2] < '0' || name[2] > '9')
		return -1;
	s = name[0] - '0';
	s = s * 10 + name[1] - '0';
	s = s * 10 + name[2] - '0';
	return s;
}

/*
 * utility functions; should be called with the kernel lock held
 * to protect against busses/devices appearing/disappearing
 */

static void new_dev_inode(struct usb_device *dev, struct super_block *sb)
{
	struct inode *inode;
	unsigned int devnum = dev->devnum;
	unsigned int busnum = dev->bus->busnum;

	if (devnum < 1 || devnum > 127 || busnum > 255)
		return;
	inode = iget(sb, IDEVICE | (busnum << 8) | devnum);
	if (!inode) {
		printk(KERN_ERR "usbdevfs: cannot create inode for bus %u device %u\n", busnum, devnum);
		return;
	}
	inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	inode->i_uid = sb->u.usbdevfs_sb.devuid;
	inode->i_gid = sb->u.usbdevfs_sb.devgid;
	inode->i_mode = sb->u.usbdevfs_sb.devmode | S_IFREG;
	inode->i_fop = &usbdevfs_device_file_operations;
	inode->i_size = sizeof(struct usb_device_descriptor);
	inode->u.usbdev_i.p.dev = dev;
	list_add_tail(&inode->u.usbdev_i.slist, &sb->u.usbdevfs_sb.ilist);
	list_add_tail(&inode->u.usbdev_i.dlist, &dev->inodes);
}

static void recurse_new_dev_inode(struct usb_device *dev, struct super_block *sb)
{
	unsigned int i;

	if (!dev)
		return;
	new_dev_inode(dev, sb);
	for (i = 0; i < dev->maxchild; i++) {
                if (!dev->children[i])
                        continue;
		recurse_new_dev_inode(dev->children[i], sb);
	}
}

static void new_bus_inode(struct usb_bus *bus, struct super_block *sb)
{
	struct inode *inode;
	unsigned int busnum = bus->busnum;

	if (busnum > 255)
		return;
	inode = iget(sb, IBUS | (busnum << 8));
	if (!inode) {
		printk(KERN_ERR "usbdevfs: cannot create inode for bus %u\n", busnum);
		return;
	}
	inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	inode->i_uid = sb->u.usbdevfs_sb.busuid;
	inode->i_gid = sb->u.usbdevfs_sb.busgid;
	inode->i_mode = sb->u.usbdevfs_sb.busmode | S_IFDIR;
	inode->i_op = &usbdevfs_bus_inode_operations;
	inode->i_fop = &usbdevfs_bus_file_operations;
	inode->u.usbdev_i.p.bus = bus;
	list_add_tail(&inode->u.usbdev_i.slist, &sb->u.usbdevfs_sb.ilist);
	list_add_tail(&inode->u.usbdev_i.dlist, &bus->inodes);
}

static void free_inode(struct inode *inode)
{
	inode->u.usbdev_i.p.bus = NULL;
	inode->u.usbdev_i.p.dev = NULL;
	inode->i_mode &= ~S_IRWXUGO;
	inode->i_uid = inode->i_gid = 0;
	inode->i_size = 0;
	list_del(&inode->u.usbdev_i.slist);
	list_del(&inode->u.usbdev_i.dlist);
	iput(inode);
}

static int parse_options(struct super_block *s, char *data)
{
	uid_t devuid = 0, busuid = 0, listuid = 0;
	gid_t devgid = 0, busgid = 0, listgid = 0;
	umode_t devmode = S_IWUSR | S_IRUGO, busmode = S_IXUGO | S_IRUGO, listmode = S_IRUGO;
	char *curopt = NULL, *value;

	/* parse options */
	if (data)
		curopt = strtok(data, ",");
	for (; curopt; curopt = strtok(NULL, ",")) {
		if ((value = strchr(curopt, '=')) != NULL)
			*value++ = 0;
		if (!strcmp(curopt, "devuid")) {
			if (!value || !value[0])
				return -EINVAL;
			devuid = simple_strtoul(value, &value, 0);
			if (*value)
				return -EINVAL;
		}
		if (!strcmp(curopt, "devgid")) {
			if (!value || !value[0])
				return -EINVAL;
			devgid = simple_strtoul(value, &value, 0);
			if (*value)
				return -EINVAL;
		}
		if (!strcmp(curopt, "devmode")) {
			if (!value || !value[0])
				return -EINVAL;
			devmode = simple_strtoul(value, &value, 0) & S_IRWXUGO;
			if (*value)
				return -EINVAL;
		}
		if (!strcmp(curopt, "busuid")) {
			if (!value || !value[0])
				return -EINVAL;
			busuid = simple_strtoul(value, &value, 0);
			if (*value)
				return -EINVAL;
		}
		if (!strcmp(curopt, "busgid")) {
			if (!value || !value[0])
				return -EINVAL;
			busgid = simple_strtoul(value, &value, 0);
			if (*value)
				return -EINVAL;
		}
		if (!strcmp(curopt, "busmode")) {
			if (!value || !value[0])
				return -EINVAL;
			busmode = simple_strtoul(value, &value, 0) & S_IRWXUGO;
			if (*value)
				return -EINVAL;
		}
		if (!strcmp(curopt, "listuid")) {
			if (!value || !value[0])
				return -EINVAL;
			listuid = simple_strtoul(value, &value, 0);
			if (*value)
				return -EINVAL;
		}
		if (!strcmp(curopt, "listgid")) {
			if (!value || !value[0])
				return -EINVAL;
			listgid = simple_strtoul(value, &value, 0);
			if (*value)
				return -EINVAL;
		}
		if (!strcmp(curopt, "listmode")) {
			if (!value || !value[0])
				return -EINVAL;
			listmode = simple_strtoul(value, &value, 0) & S_IRWXUGO;
			if (*value)
				return -EINVAL;
		}
	}

	s->u.usbdevfs_sb.devuid = devuid;
	s->u.usbdevfs_sb.devgid = devgid;
	s->u.usbdevfs_sb.devmode = devmode;
	s->u.usbdevfs_sb.busuid = busuid;
	s->u.usbdevfs_sb.busgid = busgid;
	s->u.usbdevfs_sb.busmode = busmode;
	s->u.usbdevfs_sb.listuid = listuid;
	s->u.usbdevfs_sb.listgid = listgid;
	s->u.usbdevfs_sb.listmode = listmode;

	return 0;
}

static struct usb_bus *usbdevfs_findbus(int busnr)
{
        struct list_head *list;
        struct usb_bus *bus;

	down (&usb_bus_list_lock);
        for (list = usb_bus_list.next; list != &usb_bus_list; list = list->next) {
                bus = list_entry(list, struct usb_bus, bus_list);
                if (bus->busnum == busnr) {
			up (&usb_bus_list_lock);
                        return bus;
		}
        }
	up (&usb_bus_list_lock);
        return NULL;
}

#if 0
static struct usb_device *finddev(struct usb_device *dev, int devnr)
{
        unsigned int i;
        struct usb_device *d2;

        if (!dev)
                return NULL;
        if (dev->devnum == devnr)
                return dev;
        for (i = 0; i < dev->maxchild; i++) {
                if (!dev->children[i])
                        continue;
                if ((d2 = finddev(dev->children[i], devnr)))
                        return d2;
        }
        return NULL;
}

static struct usb_device *usbdevfs_finddevice(struct usb_bus *bus, int devnr)
{
        return finddev(bus->root_hub, devnr);
}
#endif

/* --------------------------------------------------------------------- */

static int usbdevfs_revalidate(struct dentry *dentry, int flags)
{
	struct inode *inode = dentry->d_inode;

        if (!inode)
                return 0;
	if (ITYPE(inode->i_ino) == IBUS && !inode->u.usbdev_i.p.bus)
		return 0;
	if (ITYPE(inode->i_ino) == IDEVICE && !inode->u.usbdev_i.p.dev)
		return 0;
	return 1;
}

static struct dentry_operations usbdevfs_dentry_operations = {
	d_revalidate:	usbdevfs_revalidate,
};

static struct dentry *usbdevfs_root_lookup(struct inode *dir, struct dentry *dentry)
{
	int busnr;
	unsigned long ino = 0;
	unsigned int i;
	struct inode *inode;

	/* sanity check */
	if (dir->i_ino != IROOT)
		return ERR_PTR(-EINVAL);
	dentry->d_op = &usbdevfs_dentry_operations;
	busnr = dnumber(dentry);
	if (busnr >= 0 && busnr <= 255)
		ino = IBUS | (busnr << 8);
	if (!ino) {
		for (i = 0; i < NRSPECIAL; i++) {
			if (strlen(special[i].name) == dentry->d_name.len && 
			    !strncmp(special[i].name, dentry->d_name.name, dentry->d_name.len)) {
				ino = ISPECIAL | (i + IROOT + 1);
				break;
			}
		}
	}
	if (!ino)
		return ERR_PTR(-ENOENT);
	inode = iget(dir->i_sb, ino);
	if (!inode)
		return ERR_PTR(-EINVAL);
	if (inode && ITYPE(ino) == IBUS && inode->u.usbdev_i.p.bus == NULL) {
		iput(inode);
		inode = NULL;
	}
	d_add(dentry, inode);
	return NULL;
}

static struct dentry *usbdevfs_bus_lookup(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode;
	int devnr;

	/* sanity check */
	if (ITYPE(dir->i_ino) != IBUS)
		return ERR_PTR(-EINVAL);
	dentry->d_op = &usbdevfs_dentry_operations;
	devnr = dnumber(dentry);
	if (devnr < 1 || devnr > 127)
		return ERR_PTR(-ENOENT);
	inode = iget(dir->i_sb, IDEVICE | (dir->i_ino & (0xff << 8)) | devnr);
	if (!inode)
		return ERR_PTR(-EINVAL);
	if (inode && inode->u.usbdev_i.p.dev == NULL) {
		iput(inode);
		inode = NULL;
	}
	d_add(dentry, inode);
	return NULL;
}

static int usbdevfs_root_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode *inode = filp->f_dentry->d_inode;
	unsigned long ino = inode->i_ino;
	struct special *spec;
	struct list_head *list;
	struct usb_bus *bus;
	char numbuf[8];
	unsigned int i;

	/* sanity check */
	if (ino != IROOT)
		return -EINVAL;
	i = filp->f_pos;
	switch (i) {
	case 0:
		if (filldir(dirent, ".", 1, i, IROOT, DT_DIR) < 0)
			return 0;
		filp->f_pos++;
		i++;
		/* fall through */

	case 1:
		if (filldir(dirent, "..", 2, i, IROOT, DT_DIR) < 0)
			return 0;
		filp->f_pos++;
		i++;
		/* fall through */

	default:
		
		while (i >= 2 && i < 2+NRSPECIAL) {
			spec = &special[filp->f_pos-2];
			if (filldir(dirent, spec->name, strlen(spec->name), i, ISPECIAL | (filp->f_pos-2+IROOT), DT_UNKNOWN) < 0)
				return 0;
			filp->f_pos++;
			i++;
		}
		if (i < 2+NRSPECIAL)
			return 0;
		i -= 2+NRSPECIAL;
		down (&usb_bus_list_lock);
		for (list = usb_bus_list.next; list != &usb_bus_list; list = list->next) {
			if (i > 0) {
				i--;
				continue;
			}
			bus = list_entry(list, struct usb_bus, bus_list);
			sprintf(numbuf, "%03d", bus->busnum);
			if (filldir(dirent, numbuf, 3, filp->f_pos, IBUS | ((bus->busnum & 0xff) << 8), DT_UNKNOWN) < 0)
				break;
			filp->f_pos++;
		}
		up (&usb_bus_list_lock);
		return 0;
	}
}

static int bus_readdir(struct usb_device *dev, unsigned long ino, int pos, struct file *filp, void *dirent, filldir_t filldir)
{
	char numbuf[8];
	unsigned int i;

	if (!dev)
		return pos;
	sprintf(numbuf, "%03d", dev->devnum);
	if (pos > 0)
		pos--;
	else {
		if (filldir(dirent, numbuf, 3, filp->f_pos, ino | (dev->devnum & 0xff), DT_UNKNOWN) < 0)
			return -1;
		filp->f_pos++;
	}
	for (i = 0; i < dev->maxchild; i++) {
		if (!dev->children[i])
			continue;
		pos = bus_readdir(dev->children[i], ino, pos, filp, dirent, filldir);
		if (pos < 0)
			return -1;
	}
	return pos;
}

static int usbdevfs_bus_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode *inode = filp->f_dentry->d_inode;
	unsigned long ino = inode->i_ino;
	struct usb_bus *bus;

	/* sanity check */
	if (ITYPE(ino) != IBUS)
		return -EINVAL;
	switch ((unsigned int)filp->f_pos) {
	case 0:
		if (filldir(dirent, ".", 1, filp->f_pos, ino, DT_DIR) < 0)
			return 0;
		filp->f_pos++;
		/* fall through */

	case 1:
		if (filldir(dirent, "..", 2, filp->f_pos, IROOT, DT_DIR) < 0)
			return 0;
		filp->f_pos++;
		/* fall through */

	default:
		lock_kernel();
		bus = usbdevfs_findbus(IBUSNR(ino));
		bus_readdir(bus->root_hub, IDEVICE | ((bus->busnum & 0xff) << 8), filp->f_pos-2, filp, dirent, filldir);
		unlock_kernel();
		return 0;
	}
}

static struct file_operations usbdevfs_root_file_operations = {
	readdir: usbdevfs_root_readdir,
};

static struct inode_operations usbdevfs_root_inode_operations = {
	lookup: usbdevfs_root_lookup,
};

static struct file_operations usbdevfs_bus_file_operations = {
	readdir: usbdevfs_bus_readdir,
};

static struct inode_operations usbdevfs_bus_inode_operations = {
	lookup: usbdevfs_bus_lookup,
};

static void usbdevfs_read_inode(struct inode *inode)
{
	struct special *spec;

	inode->i_ctime = inode->i_mtime = inode->i_atime = CURRENT_TIME;
	inode->i_mode = S_IFREG;
	inode->i_gid = inode->i_uid = 0;
	inode->u.usbdev_i.p.dev = NULL;
	inode->u.usbdev_i.p.bus = NULL;
	switch (ITYPE(inode->i_ino)) {
	case ISPECIAL:
		if (inode->i_ino == IROOT) {
			inode->i_op = &usbdevfs_root_inode_operations;
			inode->i_fop = &usbdevfs_root_file_operations;
			inode->i_mode = S_IFDIR | S_IRUGO | S_IXUGO;
			return;
		}
		if (inode->i_ino <= IROOT || inode->i_ino > IROOT+NRSPECIAL)
			return;
		spec = &special[inode->i_ino-(IROOT+1)];
		inode->i_fop = spec->fops;
		return;

	case IDEVICE:
		return;

	case IBUS:
		return;

	default:
		return;
        }
}

static void usbdevfs_put_super(struct super_block *sb)
{
	list_del(&sb->u.usbdevfs_sb.slist);
	INIT_LIST_HEAD(&sb->u.usbdevfs_sb.slist);
	while (!list_empty(&sb->u.usbdevfs_sb.ilist))
		free_inode(list_entry(sb->u.usbdevfs_sb.ilist.next, struct inode, u.usbdev_i.slist));
}

static int usbdevfs_statfs(struct super_block *sb, struct statfs *buf)
{
        buf->f_type = USBDEVICE_SUPER_MAGIC;
        buf->f_bsize = PAGE_SIZE/sizeof(long);   /* ??? */
        buf->f_bfree = 0;
        buf->f_bavail = 0;
        buf->f_ffree = 0;
        buf->f_namelen = NAME_MAX;
        return 0;
}

static int usbdevfs_remount(struct super_block *s, int *flags, char *data)
{
	struct list_head *ilist = s->u.usbdevfs_sb.ilist.next;
	struct inode *inode;
	int ret;

	if ((ret = parse_options(s, data))) {
		printk(KERN_WARNING "usbdevfs: remount parameter error\n");
		return ret;
	}

	for (; ilist != &s->u.usbdevfs_sb.ilist; ilist = ilist->next) {
		inode = list_entry(ilist, struct inode, u.usbdev_i.slist);

		switch (ITYPE(inode->i_ino)) {
			case ISPECIAL :
				inode->i_uid = s->u.usbdevfs_sb.listuid;
				inode->i_gid = s->u.usbdevfs_sb.listgid;
				inode->i_mode = s->u.usbdevfs_sb.listmode | S_IFREG;
				break;
			case IBUS :
				inode->i_uid = s->u.usbdevfs_sb.busuid;
				inode->i_gid = s->u.usbdevfs_sb.busgid;
				inode->i_mode = s->u.usbdevfs_sb.busmode | S_IFDIR;
				break;
			case IDEVICE :
				inode->i_uid = s->u.usbdevfs_sb.devuid;
				inode->i_gid = s->u.usbdevfs_sb.devgid;
				inode->i_mode = s->u.usbdevfs_sb.devmode | S_IFREG;
				break;
		}
	}

	return 0;
}

static struct super_operations usbdevfs_sops = { 
	read_inode:	usbdevfs_read_inode,
	put_super:	usbdevfs_put_super,
	statfs:		usbdevfs_statfs,
	remount_fs:	usbdevfs_remount,
};

struct super_block *usbdevfs_read_super(struct super_block *s, void *data, int silent)
{
        struct inode *root_inode, *inode;
	struct list_head *blist;
	struct usb_bus *bus;
	unsigned int i;

	if (parse_options(s, data)) {
		printk(KERN_WARNING "usbdevfs: mount parameter error\n");
		return NULL;
	}

	/* fill superblock */
        s->s_blocksize = 1024;
        s->s_blocksize_bits = 10;
        s->s_magic = USBDEVICE_SUPER_MAGIC;
        s->s_op = &usbdevfs_sops;
	INIT_LIST_HEAD(&s->u.usbdevfs_sb.slist);
	INIT_LIST_HEAD(&s->u.usbdevfs_sb.ilist);
	root_inode = iget(s, IROOT);
        if (!root_inode)
                goto out_no_root;
        s->s_root = d_alloc_root(root_inode);
        if (!s->s_root)
                goto out_no_root;
	lock_kernel();
	list_add_tail(&s->u.usbdevfs_sb.slist, &superlist);
	for (i = 0; i < NRSPECIAL; i++) {
		if (!(inode = iget(s, IROOT+1+i)))
			continue;
		inode->i_uid = s->u.usbdevfs_sb.listuid;
		inode->i_gid = s->u.usbdevfs_sb.listgid;
		inode->i_mode = s->u.usbdevfs_sb.listmode | S_IFREG;
		special[i].inode = inode;
		list_add_tail(&inode->u.usbdev_i.slist, &s->u.usbdevfs_sb.ilist);
		list_add_tail(&inode->u.usbdev_i.dlist, &special[i].inodes);
	}
	down (&usb_bus_list_lock);
	for (blist = usb_bus_list.next; blist != &usb_bus_list; blist = blist->next) {
		bus = list_entry(blist, struct usb_bus, bus_list);
		new_bus_inode(bus, s);
		recurse_new_dev_inode(bus->root_hub, s);
	}
	up (&usb_bus_list_lock);
	unlock_kernel();
        return s;

 out_no_root:
        printk("usbdevfs_read_super: get root inode failed\n");
        iput(root_inode);
        return NULL;
}

/*
 * The usbdevfs name is now deprecated (as of 2.4.19).
 * It will be removed when the 2.7.x development cycle is started.
 * You have been warned :)
 */
static DECLARE_FSTYPE(usbdevice_fs_type, "usbdevfs", usbdevfs_read_super, FS_SINGLE);
static DECLARE_FSTYPE(usbfs_type, "usbfs", usbdevfs_read_super, FS_SINGLE);

/* --------------------------------------------------------------------- */

static void update_special_inodes (void)
{
	int i;
	for (i = 0; i < NRSPECIAL; i++) {
		struct inode *inode = special[i].inode;
		if (inode)
			inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	}
}


void usbdevfs_add_bus(struct usb_bus *bus)
{
	struct list_head *slist;

	lock_kernel();
	for (slist = superlist.next; slist != &superlist; slist = slist->next)
		new_bus_inode(bus, list_entry(slist, struct super_block, u.usbdevfs_sb.slist));
	update_special_inodes();
	unlock_kernel();
	usbdevfs_conn_disc_event();
}

void usbdevfs_remove_bus(struct usb_bus *bus)
{
	lock_kernel();
	while (!list_empty(&bus->inodes))
		free_inode(list_entry(bus->inodes.next, struct inode, u.usbdev_i.dlist));
	update_special_inodes();
	unlock_kernel();
	usbdevfs_conn_disc_event();
}

void usbdevfs_add_device(struct usb_device *dev)
{
	struct list_head *slist;

	lock_kernel();
	for (slist = superlist.next; slist != &superlist; slist = slist->next)
		new_dev_inode(dev, list_entry(slist, struct super_block, u.usbdevfs_sb.slist));
	update_special_inodes();
	unlock_kernel();
	usbdevfs_conn_disc_event();
}

void usbdevfs_remove_device(struct usb_device *dev)
{
	struct dev_state *ds;
	struct siginfo sinfo;

	lock_kernel();
	while (!list_empty(&dev->inodes))
		free_inode(list_entry(dev->inodes.next, struct inode, u.usbdev_i.dlist));
	while (!list_empty(&dev->filelist)) {
		ds = list_entry(dev->filelist.next, struct dev_state, list);
		list_del(&ds->list);
		INIT_LIST_HEAD(&ds->list);
		down_write(&ds->devsem);
		ds->dev = NULL;
		up_write(&ds->devsem);
		if (ds->discsignr) {
			sinfo.si_signo = SIGPIPE;
			sinfo.si_errno = EPIPE;
			sinfo.si_code = SI_ASYNCIO;
			sinfo.si_addr = ds->disccontext;
			send_sig_info(ds->discsignr, &sinfo, ds->disctask);
		}
	}

	update_special_inodes();
	unlock_kernel();
	usbdevfs_conn_disc_event();
}

/* --------------------------------------------------------------------- */

#ifdef CONFIG_PROC_FS		
static struct proc_dir_entry *usbdir = NULL;
#endif	

int __init usbdevfs_init(void)
{
	int ret;

	for (ret = 0; ret < NRSPECIAL; ret++) {
		INIT_LIST_HEAD(&special[ret].inodes);
	}
	if ((ret = usb_register(&usbdevfs_driver)))
		return ret;
	if ((ret = register_filesystem(&usbdevice_fs_type))) {
		usb_deregister(&usbdevfs_driver);
		return ret;
	}
	if ((ret = register_filesystem(&usbfs_type))) {
		usb_deregister(&usbdevfs_driver);
		unregister_filesystem(&usbdevice_fs_type);
		return ret;
	}
#ifdef CONFIG_PROC_FS		
	/* create mount point for usbdevfs */
	usbdir = proc_mkdir("usb", proc_bus);
#endif	
	return ret;
}

void __exit usbdevfs_cleanup(void)
{
	usb_deregister(&usbdevfs_driver);
	unregister_filesystem(&usbdevice_fs_type);
	unregister_filesystem(&usbfs_type);
#ifdef CONFIG_PROC_FS	
        if (usbdir)
                remove_proc_entry("usb", proc_bus);
#endif
}

#if 0
module_init(usbdevfs_init);
module_exit(usbdevfs_cleanup);
#endif
