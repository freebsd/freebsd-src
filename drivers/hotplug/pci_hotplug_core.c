/*
 * PCI HotPlug Controller Core
 *
 * Copyright (C) 2001-2002 Greg Kroah-Hartman (greg@kroah.com)
 * Copyright (C) 2001-2002 IBM Corp.
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 * NON INFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Send feedback to <greg@kroah.com>
 *
 * Filesystem portion based on work done by Pat Mochel on ddfs/driverfs
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/dnotify.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include "pci_hotplug.h"


#if !defined(CONFIG_HOTPLUG_PCI_MODULE)
	#define MY_NAME	"pci_hotplug"
#else
	#define MY_NAME	THIS_MODULE->name
#endif

#define dbg(fmt, arg...) do { if (debug) printk(KERN_DEBUG "%s: %s: " fmt , MY_NAME , __FUNCTION__ , ## arg); } while (0)
#define err(format, arg...) printk(KERN_ERR "%s: " format , MY_NAME , ## arg)
#define info(format, arg...) printk(KERN_INFO "%s: " format , MY_NAME , ## arg)
#define warn(format, arg...) printk(KERN_WARNING "%s: " format , MY_NAME , ## arg)


/* local variables */
static int debug;

#define DRIVER_VERSION	"0.5"
#define DRIVER_AUTHOR	"Greg Kroah-Hartman <greg@kroah.com>"
#define DRIVER_DESC	"PCI Hot Plug PCI Core"


//////////////////////////////////////////////////////////////////

/* Random magic number */
#define PCIHPFS_MAGIC 0x52454541

struct hotplug_slot_core {
	struct dentry	*dir_dentry;
	struct dentry	*power_dentry;
	struct dentry	*attention_dentry;
	struct dentry	*latch_dentry;
	struct dentry	*adapter_dentry;
	struct dentry	*test_dentry;
	struct dentry	*max_bus_speed_dentry;
	struct dentry	*cur_bus_speed_dentry;
};

static struct super_operations pcihpfs_ops;
static struct file_operations default_file_operations;
static struct inode_operations pcihpfs_dir_inode_operations;
static struct vfsmount *pcihpfs_mount;	/* one of the mounts of our fs for reference counting */
static int pcihpfs_mount_count;		/* times we have mounted our fs */
static spinlock_t mount_lock;		/* protects our mount_count */
static spinlock_t list_lock;

LIST_HEAD(pci_hotplug_slot_list);

/* these strings match up with the values in pci_bus_speed */
static char *pci_bus_speed_strings[] = {
	"33 MHz PCI",		/* 0x00 */
	"66 MHz PCI",		/* 0x01 */
	"66 MHz PCIX", 		/* 0x02 */
	"100 MHz PCIX",		/* 0x03 */
	"133 MHz PCIX",		/* 0x04 */
	NULL,			/* 0x05 */
	NULL,			/* 0x06 */
	NULL,			/* 0x07 */
	NULL,			/* 0x08 */
	"66 MHz PCIX 266",	/* 0x09 */
	"100 MHz PCIX 266",	/* 0x0a */
	"133 MHz PCIX 266",	/* 0x0b */
	NULL,			/* 0x0c */
	NULL,			/* 0x0d */
	NULL,			/* 0x0e */
	NULL,			/* 0x0f */
	NULL,			/* 0x10 */
	"66 MHz PCIX 533",	/* 0x11 */
	"100 MHz PCIX 533",	/* 0x12 */
	"133 MHz PCIX 533",	/* 0x13 */
};

static int pcihpfs_statfs (struct super_block *sb, struct statfs *buf)
{
	buf->f_type = PCIHPFS_MAGIC;
	buf->f_bsize = PAGE_CACHE_SIZE;
	buf->f_namelen = 255;
	return 0;
}

static struct dentry *pcihpfs_lookup (struct inode *dir, struct dentry *dentry)
{
	d_add(dentry, NULL);
	return NULL;
}

#ifdef CONFIG_PROC_FS		
extern struct proc_dir_entry *proc_bus_pci_dir;
static struct proc_dir_entry *slotdir = NULL;
static const char *slotdir_name = "slots";
#endif

static struct inode *pcihpfs_get_inode (struct super_block *sb, int mode, int dev)
{
	struct inode *inode = new_inode(sb);

	if (inode) {
		inode->i_mode = mode;
		inode->i_uid = current->fsuid;
		inode->i_gid = current->fsgid;
		inode->i_blksize = PAGE_CACHE_SIZE;
		inode->i_blocks = 0;
		inode->i_rdev = NODEV;
		inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
		switch (mode & S_IFMT) {
		default:
			init_special_inode(inode, mode, dev);
			break;
		case S_IFREG:
			inode->i_fop = &default_file_operations;
			break;
		case S_IFDIR:
			inode->i_op = &pcihpfs_dir_inode_operations;
			inode->i_fop = &dcache_dir_ops;
			break;
		}
	}
	return inode; 
}

static int pcihpfs_mknod (struct inode *dir, struct dentry *dentry, int mode, int dev)
{
	struct inode *inode = pcihpfs_get_inode(dir->i_sb, mode, dev);
	int error = -ENOSPC;

	if (inode) {
		d_instantiate(dentry, inode);
		dget(dentry);
		error = 0;
	}
	return error;
}

static int pcihpfs_mkdir (struct inode *dir, struct dentry *dentry, int mode)
{
	return pcihpfs_mknod (dir, dentry, mode | S_IFDIR, 0);
}

static int pcihpfs_create (struct inode *dir, struct dentry *dentry, int mode)
{
 	return pcihpfs_mknod (dir, dentry, mode | S_IFREG, 0);
}

static inline int pcihpfs_positive (struct dentry *dentry)
{
	return dentry->d_inode && !d_unhashed(dentry);
}

static int pcihpfs_empty (struct dentry *dentry)
{
	struct list_head *list;

	spin_lock(&dcache_lock);

	list_for_each(list, &dentry->d_subdirs) {
		struct dentry *de = list_entry(list, struct dentry, d_child);
		if (pcihpfs_positive(de)) {
			spin_unlock(&dcache_lock);
			return 0;
		}
	}

	spin_unlock(&dcache_lock);
	return 1;
}

static int pcihpfs_unlink (struct inode *dir, struct dentry *dentry)
{
	int error = -ENOTEMPTY;

	if (pcihpfs_empty(dentry)) {
		struct inode *inode = dentry->d_inode;

		inode->i_nlink--;
		dput(dentry);
		error = 0;
	}
	return error;
}

#define pcihpfs_rmdir pcihpfs_unlink

/* default file operations */
static ssize_t default_read_file (struct file *file, char *buf, size_t count, loff_t *ppos)
{
	dbg ("\n");
	return 0;
}

static ssize_t default_write_file (struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	dbg ("\n");
	return count;
}

static loff_t default_file_lseek (struct file *file, loff_t offset, int orig)
{
	loff_t retval = -EINVAL;

	switch(orig) {
	case 0:
		if (offset > 0) {
			file->f_pos = offset;
			retval = file->f_pos;
		} 
		break;
	case 1:
		if ((offset + file->f_pos) > 0) {
			file->f_pos += offset;
			retval = file->f_pos;
		} 
		break;
	default:
		break;
	}
	return retval;
}

static int default_open (struct inode *inode, struct file *filp)
{
	if (inode->u.generic_ip)
		filp->private_data = inode->u.generic_ip;

	return 0;
}

static struct file_operations default_file_operations = {
	read:		default_read_file,
	write:		default_write_file,
	open:		default_open,
	llseek:		default_file_lseek,
};

/* file ops for the "power" files */
static ssize_t power_read_file (struct file *file, char *buf, size_t count, loff_t *offset);
static ssize_t power_write_file (struct file *file, const char *buf, size_t count, loff_t *ppos);
static struct file_operations power_file_operations = {
	read:		power_read_file,
	write:		power_write_file,
	open:		default_open,
	llseek:		default_file_lseek,
};

/* file ops for the "attention" files */
static ssize_t attention_read_file (struct file *file, char *buf, size_t count, loff_t *offset);
static ssize_t attention_write_file (struct file *file, const char *buf, size_t count, loff_t *ppos);
static struct file_operations attention_file_operations = {
	read:		attention_read_file,
	write:		attention_write_file,
	open:		default_open,
	llseek:		default_file_lseek,
};

/* file ops for the "latch" files */
static ssize_t latch_read_file (struct file *file, char *buf, size_t count, loff_t *offset);
static struct file_operations latch_file_operations = {
	read:		latch_read_file,
	write:		default_write_file,
	open:		default_open,
	llseek:		default_file_lseek,
};

/* file ops for the "presence" files */
static ssize_t presence_read_file (struct file *file, char *buf, size_t count, loff_t *offset);
static struct file_operations presence_file_operations = {
	read:		presence_read_file,
	write:		default_write_file,
	open:		default_open,
	llseek:		default_file_lseek,
};

/* file ops for the "max bus speed" files */
static ssize_t max_bus_speed_read_file (struct file *file, char *buf, size_t count, loff_t *offset);
static struct file_operations max_bus_speed_file_operations = {
	read:		max_bus_speed_read_file,
	write:		default_write_file,
	open:		default_open,
	llseek:		default_file_lseek,
};

/* file ops for the "current bus speed" files */
static ssize_t cur_bus_speed_read_file (struct file *file, char *buf, size_t count, loff_t *offset);
static struct file_operations cur_bus_speed_file_operations = {
	read:		cur_bus_speed_read_file,
	write:		default_write_file,
	open:		default_open,
	llseek:		default_file_lseek,
};

/* file ops for the "test" files */
static ssize_t test_write_file (struct file *file, const char *buf, size_t count, loff_t *ppos);
static struct file_operations test_file_operations = {
	read:		default_read_file,
	write:		test_write_file,
	open:		default_open,
	llseek:		default_file_lseek,
};

static struct inode_operations pcihpfs_dir_inode_operations = {
	create:		pcihpfs_create,
	lookup:		pcihpfs_lookup,
	unlink:		pcihpfs_unlink,
	mkdir:		pcihpfs_mkdir,
	rmdir:		pcihpfs_rmdir,
	mknod:		pcihpfs_mknod,
};

static struct super_operations pcihpfs_ops = {
	statfs:		pcihpfs_statfs,
	put_inode:	force_delete,
};

static struct super_block *pcihpfs_read_super (struct super_block *sb, void *data, int silent)
{
	struct inode *inode;
	struct dentry *root;

	sb->s_blocksize = PAGE_CACHE_SIZE;
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
	sb->s_magic = PCIHPFS_MAGIC;
	sb->s_op = &pcihpfs_ops;
	inode = pcihpfs_get_inode(sb, S_IFDIR | 0755, 0);

	if (!inode) {
		dbg("%s: could not get inode!\n",__FUNCTION__);
		return NULL;
	}

	root = d_alloc_root(inode);
	if (!root) {
		dbg("%s: could not get root dentry!\n",__FUNCTION__);
		iput(inode);
		return NULL;
	}
	sb->s_root = root;
	return sb;
}

static DECLARE_FSTYPE(pcihpfs_fs_type, "pcihpfs", pcihpfs_read_super, FS_SINGLE | FS_LITTER);

static int get_mount (void)
{
	struct vfsmount *mnt;

	spin_lock (&mount_lock);
	if (pcihpfs_mount) {
		mntget(pcihpfs_mount);
		++pcihpfs_mount_count;
		spin_unlock (&mount_lock);
		goto go_ahead;
	}

	spin_unlock (&mount_lock);
	mnt = kern_mount (&pcihpfs_fs_type);
	if (IS_ERR(mnt)) {
		err ("could not mount the fs...erroring out!\n");
		return -ENODEV;
	}
	spin_lock (&mount_lock);
	if (!pcihpfs_mount) {
		pcihpfs_mount = mnt;
		++pcihpfs_mount_count;
		spin_unlock (&mount_lock);
		goto go_ahead;
	}
	mntget(pcihpfs_mount);
	++pcihpfs_mount_count;
	spin_unlock (&mount_lock);
	mntput(mnt);

go_ahead:
	dbg("pcihpfs_mount_count = %d\n", pcihpfs_mount_count);
	return 0;
}

static void remove_mount (void)
{
	struct vfsmount *mnt;

	spin_lock (&mount_lock);
	mnt = pcihpfs_mount;
	--pcihpfs_mount_count;
	if (!pcihpfs_mount_count)
		pcihpfs_mount = NULL;

	spin_unlock (&mount_lock);
	mntput(mnt);
	dbg("pcihpfs_mount_count = %d\n", pcihpfs_mount_count);
}


/**
 * pcihpfs_create_by_name - create a file, given a name
 * @name:	name of file
 * @mode:	type of file
 * @parent:	dentry of directory to create it in
 * @dentry:	resulting dentry of file
 *
 * There is a bit of overhead in creating a file - basically, we 
 * have to hash the name of the file, then look it up. This will
 * prevent files of the same name. 
 * We then call the proper vfs_ function to take care of all the 
 * file creation details. 
 * This function handles both regular files and directories.
 */
static int pcihpfs_create_by_name (const char *name, mode_t mode,
				   struct dentry *parent, struct dentry **dentry)
{
	struct dentry *d = NULL;
	struct qstr qstr;
	int error;

	/* If the parent is not specified, we create it in the root.
	 * We need the root dentry to do this, which is in the super 
	 * block. A pointer to that is in the struct vfsmount that we
	 * have around.
	 */
	if (!parent ) {
		if (pcihpfs_mount && pcihpfs_mount->mnt_sb) {
			parent = pcihpfs_mount->mnt_sb->s_root;
		}
	}

	if (!parent) {
		dbg("Ah! can not find a parent!\n");
		return -EINVAL;
	}

	*dentry = NULL;
	qstr.name = name;
	qstr.len = strlen(name);
 	qstr.hash = full_name_hash(name,qstr.len);

	parent = dget(parent);

	down(&parent->d_inode->i_sem);

	d = lookup_hash(&qstr,parent);

	error = PTR_ERR(d);
	if (!IS_ERR(d)) {
		switch(mode & S_IFMT) {
		case 0: 
		case S_IFREG:
			error = vfs_create(parent->d_inode,d,mode);
			break;
		case S_IFDIR:
			error = vfs_mkdir(parent->d_inode,d,mode);
			break;
		default:
			err("cannot create special files\n");
		}
		*dentry = d;
	}
	up(&parent->d_inode->i_sem);

	dput(parent);
	return error;
}

static struct dentry *fs_create_file (const char *name, mode_t mode,
				      struct dentry *parent, void *data,
				      struct file_operations *fops)
{
	struct dentry *dentry;
	int error;

	dbg("creating file '%s'\n",name);

	error = pcihpfs_create_by_name(name,mode,parent,&dentry);
	if (error) {
		dentry = NULL;
	} else {
		if (dentry->d_inode) {
			if (data)
				dentry->d_inode->u.generic_ip = data;
			if (fops)
			dentry->d_inode->i_fop = fops;
		}
	}

	return dentry;
}

static void fs_remove_file (struct dentry *dentry)
{
	struct dentry *parent = dentry->d_parent;
	
	if (!parent || !parent->d_inode)
		return;

	down(&parent->d_inode->i_sem);
	if (pcihpfs_positive(dentry)) {
		if (dentry->d_inode) {
			if (S_ISDIR(dentry->d_inode->i_mode))
				vfs_rmdir(parent->d_inode,dentry);
			else
				vfs_unlink(parent->d_inode,dentry);
		}

		dput(dentry);
	}
	up(&parent->d_inode->i_sem);
}

#define GET_STATUS(name,type)	\
static int get_##name (struct hotplug_slot *slot, type *value)		\
{									\
	struct hotplug_slot_ops *ops = slot->ops;			\
	int retval = 0;							\
	if (ops->owner)							\
		__MOD_INC_USE_COUNT(ops->owner);			\
	if (ops->get_##name)						\
		retval = ops->get_##name (slot, value);			\
	else								\
		*value = slot->info->name;				\
	if (ops->owner)							\
		__MOD_DEC_USE_COUNT(ops->owner);			\
	return retval;							\
}

GET_STATUS(power_status, u8)
GET_STATUS(attention_status, u8)
GET_STATUS(latch_status, u8)
GET_STATUS(adapter_status, u8)
GET_STATUS(max_bus_speed, enum pci_bus_speed)
GET_STATUS(cur_bus_speed, enum pci_bus_speed)

static ssize_t power_read_file (struct file *file, char *buf, size_t count, loff_t *offset)
{
	struct hotplug_slot *slot = file->private_data;
	unsigned char *page;
	int retval;
	int len;
	u8 value;

	dbg(" count = %d, offset = %lld\n", count, *offset);

	if (*offset < 0)
		return -EINVAL;
	if (count == 0 || count > 16384)
		return 0;
	if (*offset != 0)
		return 0;

	if (slot == NULL) {
		dbg("slot == NULL???\n");
		return -ENODEV;
	}

	page = (unsigned char *)__get_free_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	retval = get_power_status (slot, &value);
	if (retval)
		goto exit;
	len = sprintf (page, "%d\n", value);

	if (copy_to_user (buf, page, len)) {
		retval = -EFAULT;
		goto exit;
	}
	*offset += len;
	retval = len;

exit:
	free_page((unsigned long)page);
	return retval;
}

static ssize_t power_write_file (struct file *file, const char *ubuff, size_t count, loff_t *offset)
{
	struct hotplug_slot *slot = file->private_data;
	char *buff;
	unsigned long lpower;
	u8 power;
	int retval = 0;

	if (*offset < 0)
		return -EINVAL;
	if (count == 0 || count > 16384)
		return 0;
	if (*offset != 0)
		return 0;

	if (slot == NULL) {
		dbg("slot == NULL???\n");
		return -ENODEV;
	}

	buff = kmalloc (count + 1, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;
	memset (buff, 0x00, count + 1);
 
	if (copy_from_user ((void *)buff, (void *)ubuff, count)) {
		retval = -EFAULT;
		goto exit;
	}
	
	lpower = simple_strtoul (buff, NULL, 10);
	power = (u8)(lpower & 0xff);
	dbg ("power = %d\n", power);

	switch (power) {
		case 0:
			if (!slot->ops->disable_slot)
				break;
			if (slot->ops->owner)
				__MOD_INC_USE_COUNT(slot->ops->owner);
			retval = slot->ops->disable_slot(slot);
			if (slot->ops->owner)
				__MOD_DEC_USE_COUNT(slot->ops->owner);
			break;

		case 1:
			if (!slot->ops->enable_slot)
				break;
			if (slot->ops->owner)
				__MOD_INC_USE_COUNT(slot->ops->owner);
			retval = slot->ops->enable_slot(slot);
			if (slot->ops->owner)
				__MOD_DEC_USE_COUNT(slot->ops->owner);
			break;

		default:
			err ("Illegal value specified for power\n");
			retval = -EINVAL;
	}

exit:	
	kfree (buff);

	if (retval)
		return retval;
	return count;
}

static ssize_t attention_read_file (struct file *file, char *buf, size_t count, loff_t *offset)
{
	struct hotplug_slot *slot = file->private_data;
	unsigned char *page;
	int retval;
	int len;
	u8 value;

	dbg("count = %d, offset = %lld\n", count, *offset);

	if (*offset < 0)
		return -EINVAL;
	if (count == 0 || count > 16384)
		return 0;
	if (*offset != 0)
		return 0;

	if (slot == NULL) {
		dbg("slot == NULL???\n");
		return -ENODEV;
	}

	page = (unsigned char *)__get_free_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	retval = get_attention_status (slot, &value);
	if (retval)
		goto exit;
	len = sprintf (page, "%d\n", value);

	if (copy_to_user (buf, page, len)) {
		retval = -EFAULT;
		goto exit;
	}
	*offset += len;
	retval = len;

exit:
	free_page((unsigned long)page);
	return retval;
}

static ssize_t attention_write_file (struct file *file, const char *ubuff, size_t count, loff_t *offset)
{
	struct hotplug_slot *slot = file->private_data;
	char *buff;
	unsigned long lattention;
	u8 attention;
	int retval = 0;

	if (*offset < 0)
		return -EINVAL;
	if (count == 0 || count > 16384)
		return 0;
	if (*offset != 0)
		return 0;

	if (slot == NULL) {
		dbg("slot == NULL???\n");
		return -ENODEV;
	}

	buff = kmalloc (count + 1, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;
	memset (buff, 0x00, count + 1);

	if (copy_from_user ((void *)buff, (void *)ubuff, count)) {
		retval = -EFAULT;
		goto exit;
	}
	
	lattention = simple_strtoul (buff, NULL, 10);
	attention = (u8)(lattention & 0xff);
	dbg (" - attention = %d\n", attention);

	if (slot->ops->set_attention_status) {
		if (slot->ops->owner)
			__MOD_INC_USE_COUNT(slot->ops->owner);
		retval = slot->ops->set_attention_status(slot, attention);
		if (slot->ops->owner)
			__MOD_DEC_USE_COUNT(slot->ops->owner);
	}

exit:	
	kfree (buff);

	if (retval)
		return retval;
	return count;
}

static ssize_t latch_read_file (struct file *file, char *buf, size_t count, loff_t *offset)
{
	struct hotplug_slot *slot = file->private_data;
	unsigned char *page;
	int retval;
	int len;
	u8 value;

	dbg("count = %d, offset = %lld\n", count, *offset);

	if (*offset < 0)
		return -EINVAL;
	if (count <= 0)
		return 0;
	if (*offset != 0)
		return 0;

	if (slot == NULL) {
		dbg("slot == NULL???\n");
		return -ENODEV;
	}

	page = (unsigned char *)__get_free_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	retval = get_latch_status (slot, &value);
	if (retval)
		goto exit;
	len = sprintf (page, "%d\n", value);

	if (copy_to_user (buf, page, len)) {
		retval = -EFAULT;
		goto exit;
	}
	*offset += len;
	retval = len;

exit:
	free_page((unsigned long)page);
	return retval;
}

static ssize_t presence_read_file (struct file *file, char *buf, size_t count, loff_t *offset)
{
	struct hotplug_slot *slot = file->private_data;
	unsigned char *page;
	int retval;
	int len;
	u8 value;

	dbg("count = %d, offset = %lld\n", count, *offset);

	if (*offset < 0)
		return -EINVAL;
	if (count <= 0)
		return 0;
	if (*offset != 0)
		return 0;

	if (slot == NULL) {
		dbg("slot == NULL???\n");
		return -ENODEV;
	}

	page = (unsigned char *)__get_free_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	retval = get_adapter_status (slot, &value);
	if (retval)
		goto exit;
	len = sprintf (page, "%d\n", value);

	if (copy_to_user (buf, page, len)) {
		retval = -EFAULT;
		goto exit;
	}
	*offset += len;
	retval = len;

exit:
	free_page((unsigned long)page);
	return retval;
}

static char *unknown_speed = "Unknown bus speed";

static ssize_t max_bus_speed_read_file (struct file *file, char *buf, size_t count, loff_t *offset)
{
	struct hotplug_slot *slot = file->private_data;
	unsigned char *page;
	char *speed_string;
	int retval;
	int len = 0;
	enum pci_bus_speed value;
	
	dbg ("count = %d, offset = %lld\n", count, *offset);

	if (*offset < 0)
		return -EINVAL;
	if (count <= 0)
		return 0;
	if (*offset != 0)
		return 0;

	if (slot == NULL) {
		dbg("slot == NULL???\n");
		return -ENODEV;
	}

	page = (unsigned char *)__get_free_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	retval = get_max_bus_speed (slot, &value);
	if (retval)
		goto exit;

	if (value == PCI_SPEED_UNKNOWN)
		speed_string = unknown_speed;
	else
		speed_string = pci_bus_speed_strings[value];
	
	len = sprintf (page, "%s\n", speed_string);

	if (copy_to_user (buf, page, len)) {
		retval = -EFAULT;
		goto exit;
	}
	*offset += len;
	retval = len;

exit:
	free_page((unsigned long)page);
	return retval;
}

static ssize_t cur_bus_speed_read_file (struct file *file, char *buf, size_t count, loff_t *offset)
{
	struct hotplug_slot *slot = file->private_data;
	unsigned char *page;
	char *speed_string;
	int retval;
	int len = 0;
	enum pci_bus_speed value;

	dbg ("count = %d, offset = %lld\n", count, *offset);

	if (*offset < 0)
		return -EINVAL;
	if (count <= 0)
		return 0;
	if (*offset != 0)
		return 0;

	if (slot == NULL) {
		dbg("slot == NULL???\n");
		return -ENODEV;
	}

	page = (unsigned char *)__get_free_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	retval = get_cur_bus_speed (slot, &value);
	if (retval)
		goto exit;

	if (value == PCI_SPEED_UNKNOWN)
		speed_string = unknown_speed;
	else
		speed_string = pci_bus_speed_strings[value];
	
	len = sprintf (page, "%s\n", speed_string);

	if (copy_to_user (buf, page, len)) {
		retval = -EFAULT;
		goto exit;
	}
	*offset += len;
	retval = len;

exit:
	free_page((unsigned long)page);
	return retval;
}

static ssize_t test_write_file (struct file *file, const char *ubuff, size_t count, loff_t *offset)
{
	struct hotplug_slot *slot = file->private_data;
	char *buff;
	unsigned long ltest;
	u32 test;
	int retval = 0;

	if (*offset < 0)
		return -EINVAL;
	if (count == 0 || count > 16384)
		return 0;
	if (*offset != 0)
		return 0;

	if (slot == NULL) {
		dbg("slot == NULL???\n");
		return -ENODEV;
	}

	buff = kmalloc (count + 1, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;
	memset (buff, 0x00, count + 1);

	if (copy_from_user ((void *)buff, (void *)ubuff, count)) {
		retval = -EFAULT;
		goto exit;
	}
	
	ltest = simple_strtoul (buff, NULL, 10);
	test = (u32)(ltest & 0xffffffff);
	dbg ("test = %d\n", test);

	if (slot->ops->hardware_test) {
		if (slot->ops->owner)
			__MOD_INC_USE_COUNT(slot->ops->owner);
		retval = slot->ops->hardware_test(slot, test);
		if (slot->ops->owner)
			__MOD_DEC_USE_COUNT(slot->ops->owner);
	}

exit:	
	kfree (buff);

	if (retval)
		return retval;
	return count;
}

static int fs_add_slot (struct hotplug_slot *slot)
{
	struct hotplug_slot_core *core = slot->core_priv;
	int result;

	result = get_mount();
	if (result)
		return result;

	core->dir_dentry = fs_create_file (slot->name,
					   S_IFDIR | S_IXUGO | S_IRUGO,
					   NULL, NULL, NULL);
	if (core->dir_dentry != NULL) {
		if ((slot->ops->enable_slot) ||
		    (slot->ops->disable_slot) ||
		    (slot->ops->get_power_status))
			core->power_dentry = 
				fs_create_file ("power",
						S_IFREG | S_IRUGO | S_IWUSR,
						core->dir_dentry, slot,
						&power_file_operations);

		if ((slot->ops->set_attention_status) ||
		    (slot->ops->get_attention_status))
			core->attention_dentry =
				fs_create_file ("attention",
						S_IFREG | S_IRUGO | S_IWUSR,
						core->dir_dentry, slot,
						&attention_file_operations);

		if (slot->ops->get_latch_status)
			core->latch_dentry = 
				fs_create_file ("latch",
						S_IFREG | S_IRUGO,
						core->dir_dentry, slot,
						&latch_file_operations);

		if (slot->ops->get_adapter_status)
			core->adapter_dentry = 
				fs_create_file ("adapter",
						S_IFREG | S_IRUGO,
						core->dir_dentry, slot,
						&presence_file_operations);

		if (slot->ops->get_max_bus_speed)
			core->max_bus_speed_dentry = 
				fs_create_file ("max_bus_speed",
						S_IFREG | S_IRUGO,
						core->dir_dentry, slot,
						&max_bus_speed_file_operations);

		if (slot->ops->get_cur_bus_speed)
			core->cur_bus_speed_dentry =
				fs_create_file ("cur_bus_speed",
						S_IFREG | S_IRUGO,
						core->dir_dentry, slot,
						&cur_bus_speed_file_operations);

		if (slot->ops->hardware_test)
			core->test_dentry =
				fs_create_file ("test",
						S_IFREG | S_IRUGO | S_IWUSR,
						core->dir_dentry, slot,
						&test_file_operations);
	}
	return 0;
}

static void fs_remove_slot (struct hotplug_slot *slot)
{
	struct hotplug_slot_core *core = slot->core_priv;

	if (core->dir_dentry) {
		if (core->power_dentry)
			fs_remove_file (core->power_dentry);
		if (core->attention_dentry)
			fs_remove_file (core->attention_dentry);
		if (core->latch_dentry)
			fs_remove_file (core->latch_dentry);
		if (core->adapter_dentry)
			fs_remove_file (core->adapter_dentry);
		if (core->max_bus_speed_dentry)
			fs_remove_file (core->max_bus_speed_dentry);
		if (core->cur_bus_speed_dentry)
			fs_remove_file (core->cur_bus_speed_dentry);
		if (core->test_dentry)
			fs_remove_file (core->test_dentry);
		fs_remove_file (core->dir_dentry);
	}

	remove_mount();
}

static struct hotplug_slot *get_slot_from_name (const char *name)
{
	struct hotplug_slot *slot;
	struct list_head *tmp;

	list_for_each (tmp, &pci_hotplug_slot_list) {
		slot = list_entry (tmp, struct hotplug_slot, slot_list);
		if (strcmp(slot->name, name) == 0)
			return slot;
	}
	return NULL;
}

/**
 * pci_hp_register - register a hotplug_slot with the PCI hotplug subsystem
 * @slot: pointer to the &struct hotplug_slot to register
 *
 * Registers a hotplug slot with the pci hotplug subsystem, which will allow
 * userspace interaction to the slot.
 *
 * Returns 0 if successful, anything else for an error.
 */
int pci_hp_register (struct hotplug_slot *slot)
{
	struct hotplug_slot_core *core;
	int result;

	if (slot == NULL)
		return -ENODEV;
	if ((slot->info == NULL) || (slot->ops == NULL))
		return -EINVAL;

	core = kmalloc (sizeof (struct hotplug_slot_core), GFP_KERNEL);
	if (!core)
		return -ENOMEM;

	/* make sure we have not already registered this slot */
	spin_lock (&list_lock);
	if (get_slot_from_name (slot->name) != NULL) {
		spin_unlock (&list_lock);
		kfree (core);
		return -EEXIST;
	}

	memset (core, 0, sizeof (struct hotplug_slot_core));
	slot->core_priv = core;

	list_add (&slot->slot_list, &pci_hotplug_slot_list);
	spin_unlock (&list_lock);

	result = fs_add_slot (slot);
	dbg ("Added slot %s to the list\n", slot->name);
	return result;
}

/**
 * pci_hp_deregister - deregister a hotplug_slot with the PCI hotplug subsystem
 * @slot: pointer to the &struct hotplug_slot to deregister
 *
 * The @slot must have been registered with the pci hotplug subsystem
 * previously with a call to pci_hp_register().
 *
 * Returns 0 if successful, anything else for an error.
 */
int pci_hp_deregister (struct hotplug_slot *slot)
{
	struct hotplug_slot *temp;

	if (slot == NULL)
		return -ENODEV;

	/* make sure we have this slot in our list before trying to delete it */
	spin_lock (&list_lock);
	temp = get_slot_from_name (slot->name);
	if (temp != slot) {
		spin_unlock (&list_lock);
		return -ENODEV;
	}

	list_del (&slot->slot_list);
	spin_unlock (&list_lock);

	fs_remove_slot (slot);
	kfree(slot->core_priv);
	dbg ("Removed slot %s from the list\n", slot->name);
	return 0;
}

static inline void update_dentry_inode_time (struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	if (inode) {
		inode->i_mtime = CURRENT_TIME;
		dnotify_parent(dentry, DN_MODIFY);
	}
}

/**
 * pci_hp_change_slot_info - changes the slot's information structure in the core
 * @name: the name of the slot whose info has changed
 * @info: pointer to the info copy into the slot's info structure
 *
 * A slot with @name must have been registered with the pci 
 * hotplug subsystem previously with a call to pci_hp_register().
 *
 * Returns 0 if successful, anything else for an error.
 */
int pci_hp_change_slot_info (const char *name, struct hotplug_slot_info *info)
{
	struct hotplug_slot *temp;
	struct hotplug_slot_core *core;

	if (info == NULL)
		return -ENODEV;

	spin_lock (&list_lock);
	temp = get_slot_from_name (name);
	if (temp == NULL) {
		spin_unlock (&list_lock);
		return -ENODEV;
	}

	/*
	 * check all fields in the info structure, and update timestamps
	 * for the files referring to the fields that have now changed.
	 */
	core = temp->core_priv;
	if ((core->power_dentry) &&
	    (temp->info->power_status != info->power_status))
		update_dentry_inode_time (core->power_dentry);
	if ((core->attention_dentry) &&
	    (temp->info->attention_status != info->attention_status))
		update_dentry_inode_time (core->attention_dentry);
	if ((core->latch_dentry) &&
	    (temp->info->latch_status != info->latch_status))
		update_dentry_inode_time (core->latch_dentry);
	if ((core->adapter_dentry) &&
	    (temp->info->adapter_status != info->adapter_status))
		update_dentry_inode_time (core->adapter_dentry);
	if ((core->cur_bus_speed_dentry) &&
	    (temp->info->cur_bus_speed != info->cur_bus_speed))
		update_dentry_inode_time (core->cur_bus_speed_dentry);

	memcpy (temp->info, info, sizeof (struct hotplug_slot_info));
	spin_unlock (&list_lock);
	return 0;
}

static int __init pci_hotplug_init (void)
{
	int result;

	spin_lock_init(&mount_lock);
	spin_lock_init(&list_lock);

	dbg("registering filesystem.\n");
	result = register_filesystem(&pcihpfs_fs_type);
	if (result) {
		err("register_filesystem failed with %d\n", result);
		goto exit;
	}

#ifdef CONFIG_PROC_FS
	/* create mount point for pcihpfs */
	slotdir = proc_mkdir(slotdir_name, proc_bus_pci_dir);
#endif

	info (DRIVER_DESC " version: " DRIVER_VERSION "\n");

exit:
	return result;
}

static void __exit pci_hotplug_exit (void)
{
	unregister_filesystem(&pcihpfs_fs_type);

#ifdef CONFIG_PROC_FS
	if (slotdir)
		remove_proc_entry(slotdir_name, proc_bus_pci_dir);
#endif
}

module_init(pci_hotplug_init);
module_exit(pci_hotplug_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Debugging mode enabled or not");

EXPORT_SYMBOL_GPL(pci_hp_register);
EXPORT_SYMBOL_GPL(pci_hp_deregister);
EXPORT_SYMBOL_GPL(pci_hp_change_slot_info);

