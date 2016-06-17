/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright (C) 2001 Red Hat, Inc.
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
 * $Id: write.c,v 1.30.2.2 2003/11/02 13:51:18 dwmw2 Exp $
 *
 */

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/jffs2.h>
#include <linux/mtd/mtd.h>
#include "nodelist.h"
#include <linux/crc32.h>

/* jffs2_new_inode: allocate a new inode and inocache, add it to the hash,
   fill in the raw_inode while you're at it. */
struct inode *jffs2_new_inode (struct inode *dir_i, int mode, struct jffs2_raw_inode *ri)
{
	struct inode *inode;
	struct super_block *sb = dir_i->i_sb;
	struct jffs2_inode_cache *ic;
	struct jffs2_sb_info *c;
	struct jffs2_inode_info *f;

	D1(printk(KERN_DEBUG "jffs2_new_inode(): dir_i %ld, mode 0x%x\n", dir_i->i_ino, mode));

	c = JFFS2_SB_INFO(sb);
	memset(ri, 0, sizeof(*ri));

	ic = jffs2_alloc_inode_cache();
	if (!ic) {
		return ERR_PTR(-ENOMEM);
	}
	memset(ic, 0, sizeof(*ic));
	
	inode = new_inode(sb);
	
	if (!inode) {
		jffs2_free_inode_cache(ic);
		return ERR_PTR(-ENOMEM);
	}

	/* Alloc jffs2_inode_info when that's split in 2.5 */

	f = JFFS2_INODE_INFO(inode);
	memset(f, 0, sizeof(*f));
	init_MUTEX_LOCKED(&f->sem);
	f->inocache = ic;
	inode->i_nlink = f->inocache->nlink = 1;
	f->inocache->nodes = (struct jffs2_raw_node_ref *)f->inocache;
	f->inocache->ino = ri->ino = inode->i_ino = ++c->highest_ino;
	D1(printk(KERN_DEBUG "jffs2_new_inode(): Assigned ino# %d\n", ri->ino));
	jffs2_add_ino_cache(c, f->inocache);

	ri->magic = JFFS2_MAGIC_BITMASK;
	ri->nodetype = JFFS2_NODETYPE_INODE;
	ri->totlen = PAD(sizeof(*ri));
	ri->hdr_crc = crc32(0, ri, sizeof(struct jffs2_unknown_node)-4);
	ri->mode = mode;
	f->highest_version = ri->version = 1;
	ri->uid = current->fsuid;
	if (dir_i->i_mode & S_ISGID) {
		ri->gid = dir_i->i_gid;
		if (S_ISDIR(mode))
			ri->mode |= S_ISGID;
	} else {
		ri->gid = current->fsgid;
	}
	inode->i_mode = ri->mode;
	inode->i_gid = ri->gid;
	inode->i_uid = ri->uid;
	inode->i_atime = inode->i_ctime = inode->i_mtime = 
		ri->atime = ri->mtime = ri->ctime = CURRENT_TIME;
	inode->i_blksize = PAGE_SIZE;
	inode->i_blocks = 0;
	inode->i_size = 0;

	insert_inode_hash(inode);

	return inode;
}

/* This ought to be in core MTD code. All registered MTD devices
   without writev should have this put in place. Bug the MTD
   maintainer */
static int mtd_fake_writev(struct mtd_info *mtd, const struct iovec *vecs, unsigned long count, loff_t to, size_t *retlen)
{
	unsigned long i;
	size_t totlen = 0, thislen;
	int ret = 0;

	for (i=0; i<count; i++) {
		ret = mtd->write(mtd, to, vecs[i].iov_len, &thislen, vecs[i].iov_base);
		totlen += thislen;
		if (ret || thislen != vecs[i].iov_len)
			break;
		to += vecs[i].iov_len;
	}
	if (retlen)
		*retlen = totlen;
	return ret;
}


static inline int mtd_writev(struct mtd_info *mtd, const struct iovec *vecs, unsigned long count, loff_t to, size_t *retlen)
{
	if (mtd->writev)
		return mtd->writev(mtd,vecs,count,to,retlen);
	else
		return mtd_fake_writev(mtd, vecs, count, to, retlen);
}

static void writecheck(struct mtd_info *mtd, __u32 ofs)
{
	unsigned char buf[16];
	ssize_t retlen;
	int ret, i;

	ret = mtd->read(mtd, ofs, 16, &retlen, buf);
	if (ret && retlen != 16) {
		D1(printk(KERN_DEBUG "read failed or short in writecheck(). ret %d, retlen %d\n", ret, retlen));
		return;
	}
	ret = 0;
	for (i=0; i<16; i++) {
		if (buf[i] != 0xff)
			ret = 1;
	}
	if (ret) {
		printk(KERN_WARNING "ARGH. About to write node to 0x%08x on flash, but there's data already there:\n", ofs);
		printk(KERN_WARNING "0x%08x: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", 
		       ofs,
		       buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7],
		       buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
	}
}

	
	

/* jffs2_write_dnode - given a raw_inode, allocate a full_dnode for it, 
   write it to the flash, link it into the existing inode/fragment list */

struct jffs2_full_dnode *jffs2_write_dnode(struct inode *inode, struct jffs2_raw_inode *ri, const unsigned char *data, __u32 datalen, __u32 flash_ofs,  __u32 *writelen)

{
	struct jffs2_sb_info *c = JFFS2_SB_INFO(inode->i_sb);
	struct jffs2_inode_info *f = JFFS2_INODE_INFO(inode);
	struct jffs2_raw_node_ref *raw;
	struct jffs2_full_dnode *fn;
	ssize_t retlen;
	struct iovec vecs[2];
	int ret;

	D1(if(ri->hdr_crc != crc32(0, ri, sizeof(struct jffs2_unknown_node)-4)) {
		printk(KERN_CRIT "Eep. CRC not correct in jffs2_write_dnode()\n");
		BUG();
	}
	   );
	vecs[0].iov_base = ri;
	vecs[0].iov_len = sizeof(*ri);
	vecs[1].iov_base = (unsigned char *)data;
	vecs[1].iov_len = datalen;

	writecheck(c->mtd, flash_ofs);

	if (ri->totlen != sizeof(*ri) + datalen) {
		printk(KERN_WARNING "jffs2_write_dnode: ri->totlen (0x%08x) != sizeof(*ri) (0x%08x) + datalen (0x%08x)\n", ri->totlen, sizeof(*ri), datalen);
	}
	raw = jffs2_alloc_raw_node_ref();
	if (!raw)
		return ERR_PTR(-ENOMEM);
	
	fn = jffs2_alloc_full_dnode();
	if (!fn) {
		jffs2_free_raw_node_ref(raw);
		return ERR_PTR(-ENOMEM);
	}
	raw->flash_offset = flash_ofs;
	raw->totlen = PAD(ri->totlen);
	raw->next_phys = NULL;

	fn->ofs = ri->offset;
	fn->size = ri->dsize;
	fn->frags = 0;
	fn->raw = raw;

	ret = mtd_writev(c->mtd, vecs, 2, flash_ofs, &retlen);
	if (ret || (retlen != sizeof(*ri) + datalen)) {
		printk(KERN_NOTICE "Write of %d bytes at 0x%08x failed. returned %d, retlen %d\n", 
		       sizeof(*ri)+datalen, flash_ofs, ret, retlen);
		/* Mark the space as dirtied */
		if (retlen) {
			/* Doesn't belong to any inode */
			raw->next_in_ino = NULL;

			/* Don't change raw->size to match retlen. We may have 
			   written the node header already, and only the data will
			   seem corrupted, in which case the scan would skip over
			   any node we write before the original intended end of 
			   this node */
			jffs2_add_physical_node_ref(c, raw, sizeof(*ri)+datalen, 1);
			jffs2_mark_node_obsolete(c, raw);
		} else {
			printk(KERN_NOTICE "Not marking the space at 0x%08x as dirty because the flash driver returned retlen zero\n", raw->flash_offset);
			jffs2_free_raw_node_ref(raw);
		}

		/* Release the full_dnode which is now useless, and return */
		jffs2_free_full_dnode(fn);
		if (writelen)
			*writelen = retlen;
		return ERR_PTR(ret?ret:-EIO);
	}
	/* Mark the space used */
	jffs2_add_physical_node_ref(c, raw, retlen, 0);

	/* Link into per-inode list */
	raw->next_in_ino = f->inocache->nodes;
	f->inocache->nodes = raw;

	D1(printk(KERN_DEBUG "jffs2_write_dnode wrote node at 0x%08x with dsize 0x%x, csize 0x%x, node_crc 0x%08x, data_crc 0x%08x, totlen 0x%08x\n", flash_ofs, ri->dsize, ri->csize, ri->node_crc, ri->data_crc, ri->totlen));
	if (writelen)
		*writelen = retlen;

	f->inocache->nodes = raw;
	return fn;
}

struct jffs2_full_dirent *jffs2_write_dirent(struct inode *inode, struct jffs2_raw_dirent *rd, const unsigned char *name, __u32 namelen, __u32 flash_ofs,  __u32 *writelen)
{
	struct jffs2_sb_info *c = JFFS2_SB_INFO(inode->i_sb);
	struct jffs2_inode_info *f = JFFS2_INODE_INFO(inode);
	struct jffs2_raw_node_ref *raw;
	struct jffs2_full_dirent *fd;
	ssize_t retlen;
	struct iovec vecs[2];
	int ret;

	D1(printk(KERN_DEBUG "jffs2_write_dirent(ino #%u, name at *0x%p \"%s\"->ino #%u, name_crc 0x%08x)\n", rd->pino, name, name, rd->ino, rd->name_crc));
	writecheck(c->mtd, flash_ofs);

	D1(if(rd->hdr_crc != crc32(0, rd, sizeof(struct jffs2_unknown_node)-4)) {
		printk(KERN_CRIT "Eep. CRC not correct in jffs2_write_dirent()\n");
		BUG();
	}
	   );

	vecs[0].iov_base = rd;
	vecs[0].iov_len = sizeof(*rd);
	vecs[1].iov_base = (unsigned char *)name;
	vecs[1].iov_len = namelen;
	
	raw = jffs2_alloc_raw_node_ref();

	if (!raw)
		return ERR_PTR(-ENOMEM);

	fd = jffs2_alloc_full_dirent(namelen+1);
	if (!fd) {
		jffs2_free_raw_node_ref(raw);
		return ERR_PTR(-ENOMEM);
	}
	raw->flash_offset = flash_ofs;
	raw->totlen = PAD(rd->totlen);
	raw->next_in_ino = f->inocache->nodes;
	f->inocache->nodes = raw;
	raw->next_phys = NULL;

	fd->version = rd->version;
	fd->ino = rd->ino;
	fd->nhash = full_name_hash(name, strlen(name));
	fd->type = rd->type;
	memcpy(fd->name, name, namelen);
	fd->name[namelen]=0;
	fd->raw = raw;

	ret = mtd_writev(c->mtd, vecs, 2, flash_ofs, &retlen);
		if (ret || (retlen != sizeof(*rd) + namelen)) {
			printk(KERN_NOTICE "Write of %d bytes at 0x%08x failed. returned %d, retlen %d\n", 
			       sizeof(*rd)+namelen, flash_ofs, ret, retlen);
		/* Mark the space as dirtied */
			if (retlen) {
				jffs2_add_physical_node_ref(c, raw, sizeof(*rd)+namelen, 1);
				jffs2_mark_node_obsolete(c, raw);
			} else {
				printk(KERN_NOTICE "Not marking the space at 0x%08x as dirty because the flash driver returned retlen zero\n", raw->flash_offset);
				jffs2_free_raw_node_ref(raw);
			}

		/* Release the full_dnode which is now useless, and return */
		jffs2_free_full_dirent(fd);
		if (writelen)
			*writelen = retlen;
		return ERR_PTR(ret?ret:-EIO);
	}
	/* Mark the space used */
	jffs2_add_physical_node_ref(c, raw, retlen, 0);
	if (writelen)
		*writelen = retlen;

	f->inocache->nodes = raw;
	return fd;
}
