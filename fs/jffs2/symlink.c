/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright (C) 2001, 2002 Red Hat, Inc.
 *
 * Created by David Woodhouse <dwmw2@cambridge.redhat.com>
 *
 * The original JFFS, from which the design for JFFS2 was derived,
 * was designed and implemented by Axis Communications AB.
 *
 * The contents of this file are subject to the Red Hat eCos Public
 * License Version 1.1 (the "Licence"); you may not use this file
 * except in compliance with the Licence.  You may obtain a copy of
 * the Licence at http://www.redhat.com/
 *
 * Software distributed under the Licence is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.
 * See the Licence for the specific language governing rights and
 * limitations under the Licence.
 *
 * The Original Code is JFFS2 - Journalling Flash File System, version 2
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License version 2 (the "GPL"), in
 * which case the provisions of the GPL are applicable instead of the
 * above.  If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the RHEPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GPL.  If you do not delete the
 * provisions above, a recipient may use your version of this file
 * under either the RHEPL or the GPL.
 *
 * $Id: symlink.c,v 1.5.2.1 2002/01/15 10:39:06 dwmw2 Exp $
 *
 */


#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/jffs2.h>
#include "nodelist.h"

int jffs2_readlink(struct dentry *dentry, char *buffer, int buflen);
int jffs2_follow_link(struct dentry *dentry, struct nameidata *nd);

struct inode_operations jffs2_symlink_inode_operations =
{	
	readlink:	jffs2_readlink,
	follow_link:	jffs2_follow_link,
	setattr:	jffs2_setattr
};

static char *jffs2_getlink(struct dentry *dentry)
{
	struct jffs2_inode_info *f = JFFS2_INODE_INFO(dentry->d_inode);
	char *buf;
	int ret;

	down(&f->sem);
	if (!f->metadata) {
		up(&f->sem);
		printk(KERN_NOTICE "No metadata for symlink inode #%lu\n", dentry->d_inode->i_ino);
		return ERR_PTR(-EINVAL);
	}
	buf = kmalloc(f->metadata->size+1, GFP_USER);
	if (!buf) {
		up(&f->sem);
		return ERR_PTR(-ENOMEM);
	}
	buf[f->metadata->size]=0;

	ret = jffs2_read_dnode(JFFS2_SB_INFO(dentry->d_inode->i_sb), f->metadata, buf, 0, f->metadata->size);
	up(&f->sem);
	if (ret) {
		kfree(buf);
		return ERR_PTR(ret);
	}
	return buf;

}
int jffs2_readlink(struct dentry *dentry, char *buffer, int buflen)
{
	unsigned char *kbuf;
	int ret;

	kbuf = jffs2_getlink(dentry);
	if (IS_ERR(kbuf))
		return PTR_ERR(kbuf);

	ret = vfs_readlink(dentry, buffer, buflen, kbuf);
	kfree(kbuf);
	return ret;
}

int jffs2_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	unsigned char *buf;
	int ret;

	buf = jffs2_getlink(dentry);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	ret = vfs_follow_link(nd, buf);
	kfree(buf);
	return ret;
}
