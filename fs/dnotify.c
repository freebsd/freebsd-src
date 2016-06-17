/*
 * Directory notifications for Linux.
 *
 * Copyright (C) 2000,2001,2002 Stephen Rothwell
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/dnotify.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/slab.h>

extern void send_sigio(struct fown_struct *fown, int fd, int band);

int dir_notify_enable = 1;

static rwlock_t dn_lock = RW_LOCK_UNLOCKED;
static kmem_cache_t *dn_cache;

static void redo_inode_mask(struct inode *inode)
{
	unsigned long new_mask;
	struct dnotify_struct *dn;

	new_mask = 0;
	for (dn = inode->i_dnotify; dn != NULL; dn = dn->dn_next)
		new_mask |= dn->dn_mask & ~DN_MULTISHOT;
	inode->i_dnotify_mask = new_mask;
}

void dnotify_flush(struct file *filp, fl_owner_t id)
{
	struct dnotify_struct *dn;
	struct dnotify_struct **prev;
	struct inode *inode;

	inode = filp->f_dentry->d_inode;
	if (!S_ISDIR(inode->i_mode))
		return;
	write_lock(&dn_lock);
	prev = &inode->i_dnotify;
	while ((dn = *prev) != NULL) {
		if ((dn->dn_owner == id) && (dn->dn_filp == filp)) {
			*prev = dn->dn_next;
			redo_inode_mask(inode);
			kmem_cache_free(dn_cache, dn);
			break;
		}
		prev = &dn->dn_next;
	}
	write_unlock(&dn_lock);
}

int fcntl_dirnotify(int fd, struct file *filp, unsigned long arg)
{
	struct dnotify_struct *dn;
	struct dnotify_struct *odn;
	struct dnotify_struct **prev;
	struct inode *inode;
	fl_owner_t id = current->files;

	if ((arg & ~DN_MULTISHOT) == 0) {
		dnotify_flush(filp, id);
		return 0;
	}
	if (!dir_notify_enable)
		return -EINVAL;
	inode = filp->f_dentry->d_inode;
	if (!S_ISDIR(inode->i_mode))
		return -ENOTDIR;
	dn = kmem_cache_alloc(dn_cache, SLAB_KERNEL);
	if (dn == NULL)
		return -ENOMEM;
	write_lock(&dn_lock);
	prev = &inode->i_dnotify;
	while ((odn = *prev) != NULL) {
		if ((odn->dn_owner == id) && (odn->dn_filp == filp)) {
			odn->dn_fd = fd;
			odn->dn_mask |= arg;
			inode->i_dnotify_mask |= arg & ~DN_MULTISHOT;
			kmem_cache_free(dn_cache, dn);
			goto out;
		}
		prev = &odn->dn_next;
	}
	filp->f_owner.pid = current->pid;
	filp->f_owner.uid = current->uid;
	filp->f_owner.euid = current->euid;
	dn->dn_mask = arg;
	dn->dn_fd = fd;
	dn->dn_filp = filp;
	dn->dn_owner = id;
	inode->i_dnotify_mask |= arg & ~DN_MULTISHOT;
	dn->dn_next = inode->i_dnotify;
	inode->i_dnotify = dn;
out:
	write_unlock(&dn_lock);
	return 0;
}

void __inode_dir_notify(struct inode *inode, unsigned long event)
{
	struct dnotify_struct *	dn;
	struct dnotify_struct **prev;
	struct fown_struct *	fown;
	int			changed = 0;

	write_lock(&dn_lock);
	prev = &inode->i_dnotify;
	while ((dn = *prev) != NULL) {
		if ((dn->dn_mask & event) == 0) {
			prev = &dn->dn_next;
			continue;
		}
		fown = &dn->dn_filp->f_owner;
		if (fown->pid)
		        send_sigio(fown, dn->dn_fd, POLL_MSG);
		if (dn->dn_mask & DN_MULTISHOT)
			prev = &dn->dn_next;
		else {
			*prev = dn->dn_next;
			changed = 1;
			kmem_cache_free(dn_cache, dn);
		}
	}
	if (changed)
		redo_inode_mask(inode);
	write_unlock(&dn_lock);
}

static int __init dnotify_init(void)
{
	dn_cache = kmem_cache_create("dnotify_cache",
		sizeof(struct dnotify_struct), 0, 0, NULL, NULL);
	if (!dn_cache)
		panic("cannot create dnotify slab cache");
	return 0;
}

module_init(dnotify_init)
