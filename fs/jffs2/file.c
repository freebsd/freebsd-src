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
 * $Id: file.c,v 1.58.2.7 2003/11/02 13:51:17 dwmw2 Exp $
 *
 */

#include <linux/kernel.h>
#include <linux/mtd/compatmac.h> /* for min() */
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/jffs2.h>
#include "nodelist.h"
#include <linux/crc32.h>

extern int generic_file_open(struct inode *, struct file *) __attribute__((weak));
extern loff_t generic_file_llseek(struct file *file, loff_t offset, int origin) __attribute__((weak));


int jffs2_null_fsync(struct file *filp, struct dentry *dentry, int datasync)
{
	/* Move along. Nothing to see here */
	return 0;
}

struct file_operations jffs2_file_operations =
{
	llseek:		generic_file_llseek,
	open:		generic_file_open,
	read:		generic_file_read,
	write:		generic_file_write,
	ioctl:		jffs2_ioctl,
	mmap:		generic_file_mmap,
	fsync:		jffs2_null_fsync
};

/* jffs2_file_inode_operations */

struct inode_operations jffs2_file_inode_operations =
{
	setattr:	jffs2_setattr
};

struct address_space_operations jffs2_file_address_operations =
{
	readpage:	jffs2_readpage,
	prepare_write:	jffs2_prepare_write,
	commit_write:	jffs2_commit_write
};

int jffs2_setattr (struct dentry *dentry, struct iattr *iattr)
{
	struct jffs2_full_dnode *old_metadata, *new_metadata;
	struct inode *inode = dentry->d_inode;
	struct jffs2_inode_info *f = JFFS2_INODE_INFO(inode);
	struct jffs2_sb_info *c = JFFS2_SB_INFO(inode->i_sb);
	struct jffs2_raw_inode *ri;
	unsigned short dev;
	unsigned char *mdata = NULL;
	int mdatalen = 0;
	unsigned int ivalid;
	__u32 phys_ofs, alloclen;
	int ret;
	D1(printk(KERN_DEBUG "jffs2_setattr(): ino #%lu\n", inode->i_ino));
	ret = inode_change_ok(inode, iattr);
	if (ret) 
		return ret;

	/* Special cases - we don't want more than one data node
	   for these types on the medium at any time. So setattr
	   must read the original data associated with the node
	   (i.e. the device numbers or the target name) and write
	   it out again with the appropriate data attached */
	if (S_ISBLK(inode->i_mode) || S_ISCHR(inode->i_mode)) {
		/* For these, we don't actually need to read the old node */
		dev =  (MAJOR(to_kdev_t(dentry->d_inode->i_rdev)) << 8) | 
			MINOR(to_kdev_t(dentry->d_inode->i_rdev));
		mdata = (char *)&dev;
		mdatalen = sizeof(dev);
		D1(printk(KERN_DEBUG "jffs2_setattr(): Writing %d bytes of kdev_t\n", mdatalen));
	} else if (S_ISLNK(inode->i_mode)) {
		mdatalen = f->metadata->size;
		mdata = kmalloc(f->metadata->size, GFP_USER);
		if (!mdata)
			return -ENOMEM;
		ret = jffs2_read_dnode(c, f->metadata, mdata, 0, mdatalen);
		if (ret) {
			kfree(mdata);
			return ret;
		}
		D1(printk(KERN_DEBUG "jffs2_setattr(): Writing %d bytes of symlink target\n", mdatalen));
	}

	ri = jffs2_alloc_raw_inode();
	if (!ri) {
		if (S_ISLNK(inode->i_mode))
			kfree(mdata);
		return -ENOMEM;
	}
		
	ret = jffs2_reserve_space(c, sizeof(*ri) + mdatalen, &phys_ofs, &alloclen, ALLOC_NORMAL);
	if (ret) {
		jffs2_free_raw_inode(ri);
		if (S_ISLNK(inode->i_mode))
			 kfree(mdata);
		return ret;
	}
	down(&f->sem);
        ivalid = iattr->ia_valid;
	
	ri->magic = JFFS2_MAGIC_BITMASK;
	ri->nodetype = JFFS2_NODETYPE_INODE;
	ri->totlen = sizeof(*ri) + mdatalen;
	ri->hdr_crc = crc32(0, ri, sizeof(struct jffs2_unknown_node)-4);

	ri->ino = inode->i_ino;
	ri->version = ++f->highest_version;

	ri->mode = (ivalid & ATTR_MODE)?iattr->ia_mode:inode->i_mode;
	ri->uid = (ivalid & ATTR_UID)?iattr->ia_uid:inode->i_uid;
	ri->gid = (ivalid & ATTR_GID)?iattr->ia_gid:inode->i_gid;

	if (ivalid & ATTR_MODE && ri->mode & S_ISGID &&
	    !in_group_p(ri->gid) && !capable(CAP_FSETID))
		ri->mode &= ~S_ISGID;

	ri->isize = (ivalid & ATTR_SIZE)?iattr->ia_size:inode->i_size;
	ri->atime = (ivalid & ATTR_ATIME)?iattr->ia_atime:inode->i_atime;
	ri->mtime = (ivalid & ATTR_MTIME)?iattr->ia_mtime:inode->i_mtime;
	ri->ctime = (ivalid & ATTR_CTIME)?iattr->ia_ctime:inode->i_ctime;

	ri->offset = 0;
	ri->csize = ri->dsize = mdatalen;
	ri->compr = JFFS2_COMPR_NONE;
	if (inode->i_size < ri->isize) {
		/* It's an extension. Make it a hole node */
		ri->compr = JFFS2_COMPR_ZERO;
		ri->dsize = ri->isize - inode->i_size;
		ri->offset = inode->i_size;
	}
	ri->node_crc = crc32(0, ri, sizeof(*ri)-8);
	if (mdatalen)
		ri->data_crc = crc32(0, mdata, mdatalen);
	else
		ri->data_crc = 0;

	new_metadata = jffs2_write_dnode(inode, ri, mdata, mdatalen, phys_ofs, NULL);
	if (S_ISLNK(inode->i_mode))
		kfree(mdata);

	jffs2_complete_reservation(c);
	
	if (IS_ERR(new_metadata)) {
		jffs2_free_raw_inode(ri);
		up(&f->sem);
		return PTR_ERR(new_metadata);
	}
	/* It worked. Update the inode */
	inode->i_atime = ri->atime;
	inode->i_ctime = ri->ctime;
	inode->i_mtime = ri->mtime;
	inode->i_mode = ri->mode;
	inode->i_uid = ri->uid;
	inode->i_gid = ri->gid;


	old_metadata = f->metadata;

	if (inode->i_size > ri->isize) {
		vmtruncate(inode, ri->isize);
		jffs2_truncate_fraglist (c, &f->fraglist, ri->isize);
	}

	if (inode->i_size < ri->isize) {
		jffs2_add_full_dnode_to_inode(c, f, new_metadata);
		inode->i_size = ri->isize;
		f->metadata = NULL;
	} else {
		f->metadata = new_metadata;
	}
	if (old_metadata) {
		jffs2_mark_node_obsolete(c, old_metadata->raw);
		jffs2_free_full_dnode(old_metadata);
	}
	jffs2_free_raw_inode(ri);
	up(&f->sem);
	return 0;
}

int jffs2_do_readpage_nolock (struct inode *inode, struct page *pg)
{
	struct jffs2_inode_info *f = JFFS2_INODE_INFO(inode);
	struct jffs2_sb_info *c = JFFS2_SB_INFO(inode->i_sb);
	struct jffs2_node_frag *frag = f->fraglist;
	__u32 offset = pg->index << PAGE_CACHE_SHIFT;
	__u32 end = offset + PAGE_CACHE_SIZE;
	unsigned char *pg_buf;
	int ret;

	D1(printk(KERN_DEBUG "jffs2_do_readpage_nolock(): ino #%lu, page at offset 0x%x\n", inode->i_ino, offset));

	if (!PageLocked(pg))
                PAGE_BUG(pg);

	while(frag && frag->ofs + frag->size  <= offset) {
		//		D1(printk(KERN_DEBUG "skipping frag %d-%d; before the region we care about\n", frag->ofs, frag->ofs + frag->size));
		frag = frag->next;
	}

	pg_buf = kmap(pg);

	/* XXX FIXME: Where a single physical node actually shows up in two
	   frags, we read it twice. Don't do that. */
	/* Now we're pointing at the first frag which overlaps our page */
	while(offset < end) {
		D2(printk(KERN_DEBUG "jffs2_readpage: offset %d, end %d\n", offset, end));
		if (!frag || frag->ofs > offset) {
			__u32 holesize = end - offset;
			if (frag) {
				D1(printk(KERN_NOTICE "Eep. Hole in ino %ld fraglist. frag->ofs = 0x%08x, offset = 0x%08x\n", inode->i_ino, frag->ofs, offset));
				holesize = min(holesize, frag->ofs - offset);
				D1(jffs2_print_frag_list(f));
			}
			D1(printk(KERN_DEBUG "Filling non-frag hole from %d-%d\n", offset, offset+holesize));
			memset(pg_buf, 0, holesize);
			pg_buf += holesize;
			offset += holesize;
			continue;
		} else if (frag->ofs < offset && (offset & (PAGE_CACHE_SIZE-1)) != 0) {
			D1(printk(KERN_NOTICE "Eep. Overlap in ino #%ld fraglist. frag->ofs = 0x%08x, offset = 0x%08x\n",
				  inode->i_ino, frag->ofs, offset));
			D1(jffs2_print_frag_list(f));
			memset(pg_buf, 0, end - offset);
			ClearPageUptodate(pg);
			SetPageError(pg);
			kunmap(pg);
			return -EIO;
		} else if (!frag->node) {
			__u32 holeend = min(end, frag->ofs + frag->size);
			D1(printk(KERN_DEBUG "Filling frag hole from %d-%d (frag 0x%x 0x%x)\n", offset, holeend, frag->ofs, frag->ofs + frag->size));
			memset(pg_buf, 0, holeend - offset);
			pg_buf += holeend - offset;
			offset = holeend;
			frag = frag->next;
			continue;
		} else {
			__u32 readlen;
			__u32 fragofs; /* offset within the frag to start reading */

			fragofs = offset - frag->ofs;
			readlen = min(frag->size - fragofs, end - offset);
			D1(printk(KERN_DEBUG "Reading %d-%d from node at 0x%x\n", frag->ofs+fragofs, 
				  fragofs+frag->ofs+readlen, frag->node->raw->flash_offset & ~3));
			ret = jffs2_read_dnode(c, frag->node, pg_buf, fragofs + frag->ofs - frag->node->ofs, readlen);
			D2(printk(KERN_DEBUG "node read done\n"));
			if (ret) {
				D1(printk(KERN_DEBUG"jffs2_readpage error %d\n",ret));
				memset(pg_buf, 0, readlen);
				ClearPageUptodate(pg);
				SetPageError(pg);
				kunmap(pg);
				return ret;
			}
		
			pg_buf += readlen;
			offset += readlen;
			frag = frag->next;
			D2(printk(KERN_DEBUG "node read was OK. Looping\n"));
		}
	}
	D2(printk(KERN_DEBUG "readpage finishing\n"));
	SetPageUptodate(pg);
	ClearPageError(pg);

	flush_dcache_page(pg);

	kunmap(pg);
	D1(printk(KERN_DEBUG "readpage finished\n"));
	return 0;
}

int jffs2_do_readpage_unlock(struct inode *inode, struct page *pg)
{
	int ret = jffs2_do_readpage_nolock(inode, pg);
	UnlockPage(pg);
	return ret;
}


int jffs2_readpage (struct file *filp, struct page *pg)
{
	struct jffs2_inode_info *f = JFFS2_INODE_INFO(pg->mapping->host);
	int ret;
	
	down(&f->sem);
	ret = jffs2_do_readpage_unlock(pg->mapping->host, pg);
	up(&f->sem);
	return ret;
}

int jffs2_prepare_write (struct file *filp, struct page *pg, unsigned start, unsigned end)
{
	struct inode *inode = pg->mapping->host;
	struct jffs2_inode_info *f = JFFS2_INODE_INFO(inode);
	__u32 pageofs = pg->index << PAGE_CACHE_SHIFT;
	int ret = 0;

	D1(printk(KERN_DEBUG "jffs2_prepare_write() nrpages %ld\n", inode->i_mapping->nrpages));

	if (pageofs > inode->i_size) {
		/* Make new hole frag from old EOF to new page */
		struct jffs2_sb_info *c = JFFS2_SB_INFO(inode->i_sb);
		struct jffs2_raw_inode ri;
		struct jffs2_full_dnode *fn;
		__u32 phys_ofs, alloc_len;
		
		D1(printk(KERN_DEBUG "Writing new hole frag 0x%x-0x%x between current EOF and new page\n",
			  (unsigned int)inode->i_size, pageofs));

		ret = jffs2_reserve_space(c, sizeof(ri), &phys_ofs, &alloc_len, ALLOC_NORMAL);
		if (ret)
			return ret;

		down(&f->sem);
		memset(&ri, 0, sizeof(ri));

		ri.magic = JFFS2_MAGIC_BITMASK;
		ri.nodetype = JFFS2_NODETYPE_INODE;
		ri.totlen = sizeof(ri);
		ri.hdr_crc = crc32(0, &ri, sizeof(struct jffs2_unknown_node)-4);

		ri.ino = f->inocache->ino;
		ri.version = ++f->highest_version;
		ri.mode = inode->i_mode;
		ri.uid = inode->i_uid;
		ri.gid = inode->i_gid;
		ri.isize = max((__u32)inode->i_size, pageofs);
		ri.atime = ri.ctime = ri.mtime = CURRENT_TIME;
		ri.offset = inode->i_size;
		ri.dsize = pageofs - inode->i_size;
		ri.csize = 0;
		ri.compr = JFFS2_COMPR_ZERO;
		ri.node_crc = crc32(0, &ri, sizeof(ri)-8);
		ri.data_crc = 0;
		
		fn = jffs2_write_dnode(inode, &ri, NULL, 0, phys_ofs, NULL);
		jffs2_complete_reservation(c);
		if (IS_ERR(fn)) {
			ret = PTR_ERR(fn);
			up(&f->sem);
			return ret;
		}
		ret = jffs2_add_full_dnode_to_inode(c, f, fn);
		if (f->metadata) {
			jffs2_mark_node_obsolete(c, f->metadata->raw);
			jffs2_free_full_dnode(f->metadata);
			f->metadata = NULL;
		}
		if (ret) {
			D1(printk(KERN_DEBUG "Eep. add_full_dnode_to_inode() failed in prepare_write, returned %d\n", ret));
			jffs2_mark_node_obsolete(c, fn->raw);
			jffs2_free_full_dnode(fn);
			up(&f->sem);
			return ret;
		}
		inode->i_size = pageofs;
		up(&f->sem);
	}
	

	/* Read in the page if it wasn't already present, unless it's a whole page */
	if (!Page_Uptodate(pg) && (start || end < PAGE_CACHE_SIZE)) {
		down(&f->sem);
		ret = jffs2_do_readpage_nolock(inode, pg);
		up(&f->sem);
	}
	D1(printk(KERN_DEBUG "end prepare_write(). pg->flags %lx\n", pg->flags));
	return ret;
}

int jffs2_commit_write (struct file *filp, struct page *pg, unsigned start, unsigned end)
{
	/* Actually commit the write from the page cache page we're looking at.
	 * For now, we write the full page out each time. It sucks, but it's simple
	 */
	struct inode *inode = pg->mapping->host;
	struct jffs2_inode_info *f = JFFS2_INODE_INFO(inode);
	struct jffs2_sb_info *c = JFFS2_SB_INFO(inode->i_sb);
	__u32 newsize = max_t(__u32, filp->f_dentry->d_inode->i_size, (pg->index << PAGE_CACHE_SHIFT) + end);
	__u32 file_ofs = (pg->index << PAGE_CACHE_SHIFT);
	__u32 writelen = min((__u32)PAGE_CACHE_SIZE, newsize - file_ofs);
	struct jffs2_raw_inode *ri;
	int ret = 0;
	ssize_t writtenlen = 0;

	D1(printk(KERN_DEBUG "jffs2_commit_write(): ino #%lu, page at 0x%lx, range %d-%d, flags %lx\n", inode->i_ino, pg->index << PAGE_CACHE_SHIFT, start, end, pg->flags));

	if (!start && end == PAGE_CACHE_SIZE) {
		/* We need to avoid deadlock with page_cache_read() in
		   jffs2_garbage_collect_pass(). So we have to mark the
		   page up to date, to prevent page_cache_read() from 
		   trying to re-lock it. */
		SetPageUptodate(pg);
	}

	ri = jffs2_alloc_raw_inode();
	if (!ri)
		return -ENOMEM;

	while(writelen) {
		struct jffs2_full_dnode *fn;
		unsigned char *comprbuf = NULL;
		unsigned char comprtype = JFFS2_COMPR_NONE;
		__u32 phys_ofs, alloclen;
		__u32 datalen, cdatalen;

		D2(printk(KERN_DEBUG "jffs2_commit_write() loop: 0x%x to write to 0x%x\n", writelen, file_ofs));

		ret = jffs2_reserve_space(c, sizeof(*ri) + JFFS2_MIN_DATA_LEN, &phys_ofs, &alloclen, ALLOC_NORMAL);
		if (ret) {
			SetPageError(pg);
			D1(printk(KERN_DEBUG "jffs2_reserve_space returned %d\n", ret));
			break;
		}
		down(&f->sem);
		datalen = writelen;
		cdatalen = min(alloclen - sizeof(*ri), writelen);

		comprbuf = kmalloc(cdatalen, GFP_KERNEL);
		if (comprbuf) {
			comprtype = jffs2_compress(page_address(pg)+ (file_ofs & (PAGE_CACHE_SIZE-1)), comprbuf, &datalen, &cdatalen);
		}
		if (comprtype == JFFS2_COMPR_NONE) {
			/* Either compression failed, or the allocation of comprbuf failed */
			if (comprbuf)
				kfree(comprbuf);
			comprbuf = page_address(pg) + (file_ofs & (PAGE_CACHE_SIZE -1));
			datalen = cdatalen;
		}
		/* Now comprbuf points to the data to be written, be it compressed or not.
		   comprtype holds the compression type, and comprtype == JFFS2_COMPR_NONE means
		   that the comprbuf doesn't need to be kfree()d. 
		*/

		ri->magic = JFFS2_MAGIC_BITMASK;
		ri->nodetype = JFFS2_NODETYPE_INODE;
		ri->totlen = sizeof(*ri) + cdatalen;
		ri->hdr_crc = crc32(0, ri, sizeof(struct jffs2_unknown_node)-4);

		ri->ino = inode->i_ino;
		ri->version = ++f->highest_version;
		ri->mode = inode->i_mode;
		ri->uid = inode->i_uid;
		ri->gid = inode->i_gid;
		ri->isize = max((__u32)inode->i_size, file_ofs + datalen);
		ri->atime = ri->ctime = ri->mtime = CURRENT_TIME;
		ri->offset = file_ofs;
		ri->csize = cdatalen;
		ri->dsize = datalen;
		ri->compr = comprtype;
		ri->node_crc = crc32(0, ri, sizeof(*ri)-8);
		ri->data_crc = crc32(0, comprbuf, cdatalen);

		fn = jffs2_write_dnode(inode, ri, comprbuf, cdatalen, phys_ofs, NULL);

		jffs2_complete_reservation(c);

		if (comprtype != JFFS2_COMPR_NONE)
			kfree(comprbuf);

		if (IS_ERR(fn)) {
			ret = PTR_ERR(fn);
			up(&f->sem);
			SetPageError(pg);
			break;
		}
		ret = jffs2_add_full_dnode_to_inode(c, f, fn);
		if (f->metadata) {
			jffs2_mark_node_obsolete(c, f->metadata->raw);
			jffs2_free_full_dnode(f->metadata);
			f->metadata = NULL;
		}
		up(&f->sem);
		if (ret) {
			/* Eep */
			D1(printk(KERN_DEBUG "Eep. add_full_dnode_to_inode() failed in commit_write, returned %d\n", ret));
			jffs2_mark_node_obsolete(c, fn->raw);
			jffs2_free_full_dnode(fn);
			SetPageError(pg);
			break;
		}
		inode->i_size = ri->isize;
		inode->i_blocks = (inode->i_size + 511) >> 9;
		inode->i_ctime = inode->i_mtime = ri->ctime;
		if (!datalen) {
			printk(KERN_WARNING "Eep. We didn't actually write any bloody data\n");
			ret = -EIO;
			SetPageError(pg);
			break;
		}
		D1(printk(KERN_DEBUG "increasing writtenlen by %d\n", datalen));
		writtenlen += datalen;
		file_ofs += datalen;
		writelen -= datalen;
	}

	jffs2_free_raw_inode(ri);

	if (writtenlen < end) {
		/* generic_file_write has written more to the page cache than we've
		   actually written to the medium. Mark the page !Uptodate so that 
		   it gets reread */
		D1(printk(KERN_DEBUG "jffs2_commit_write(): Not all bytes written. Marking page !uptodate\n"));
		SetPageError(pg);
		ClearPageUptodate(pg);
	}
	if (writtenlen <= start) {
		/* We didn't even get to the start of the affected part */
		ret = ret?ret:-ENOSPC;
		D1(printk(KERN_DEBUG "jffs2_commit_write(): Only %x bytes written to page. start (%x) not reached, returning %d\n", writtenlen, start, ret));
	}
	writtenlen = min(end-start, writtenlen-start);

	D1(printk(KERN_DEBUG "jffs2_commit_write() returning %d. nrpages is %ld\n",writtenlen?writtenlen:ret, inode->i_mapping->nrpages));
	return writtenlen?writtenlen:ret;
}
