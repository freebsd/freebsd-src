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
 * $Id: readinode.c,v 1.58.2.8 2003/11/02 13:54:20 dwmw2 Exp $
 *
 */

/* Given an inode, probably with existing list of fragments, add the new node
 * to the fragment list.
 */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/mtd/mtd.h>
#include <linux/jffs2.h>
#include "nodelist.h"
#include <linux/crc32.h>


D1(void jffs2_print_frag_list(struct jffs2_inode_info *f)
{
	struct jffs2_node_frag *this = f->fraglist;

	while(this) {
		if (this->node)
			printk(KERN_DEBUG "frag %04x-%04x: 0x%08x on flash (*%p->%p)\n", this->ofs, this->ofs+this->size, this->node->raw->flash_offset &~3, this, this->next);
		else 
			printk(KERN_DEBUG "frag %04x-%04x: hole (*%p->%p)\n", this->ofs, this->ofs+this->size, this, this->next);
		this = this->next;
	}
	if (f->metadata) {
		printk(KERN_DEBUG "metadata at 0x%08x\n", f->metadata->raw->flash_offset &~3);
	}
})


int jffs2_add_full_dnode_to_inode(struct jffs2_sb_info *c, struct jffs2_inode_info *f, struct jffs2_full_dnode *fn)
{
	int ret;
	D1(printk(KERN_DEBUG "jffs2_add_full_dnode_to_inode(ino #%u, f %p, fn %p)\n", f->inocache->ino, f, fn));

	ret = jffs2_add_full_dnode_to_fraglist(c, &f->fraglist, fn);

	D2(jffs2_print_frag_list(f));
	return ret;
}

static void jffs2_obsolete_node_frag(struct jffs2_sb_info *c, struct jffs2_node_frag *this)
{
	if (this->node) {
		this->node->frags--;
		if (!this->node->frags) {
			/* The node has no valid frags left. It's totally obsoleted */
			D2(printk(KERN_DEBUG "Marking old node @0x%08x (0x%04x-0x%04x) obsolete\n",
				  this->node->raw->flash_offset &~3, this->node->ofs, this->node->ofs+this->node->size));
			jffs2_mark_node_obsolete(c, this->node->raw);
			jffs2_free_full_dnode(this->node);
		} else {
			D2(printk(KERN_DEBUG "Not marking old node @0x%08x (0x%04x-0x%04x) obsolete. frags is %d\n",
				  this->node->raw->flash_offset &~3, this->node->ofs, this->node->ofs+this->node->size,
				  this->node->frags));
		}
		
	}
	jffs2_free_node_frag(this);
}

/* Doesn't set inode->i_size */
int jffs2_add_full_dnode_to_fraglist(struct jffs2_sb_info *c, struct jffs2_node_frag **list, struct jffs2_full_dnode *fn)
{
	
	struct jffs2_node_frag *this, **prev, *old;
	struct jffs2_node_frag *newfrag, *newfrag2;
	__u32 lastend = 0;


	newfrag = jffs2_alloc_node_frag();
	if (!newfrag) {
		return -ENOMEM;
	}

	D2(if (fn->raw)
		printk(KERN_DEBUG "adding node %04x-%04x @0x%08x on flash, newfrag *%p\n", fn->ofs, fn->ofs+fn->size, fn->raw->flash_offset &~3, newfrag);
	else
		printk(KERN_DEBUG "adding hole node %04x-%04x on flash, newfrag *%p\n", fn->ofs, fn->ofs+fn->size, newfrag));
	
	prev = list;
	this = *list;

	if (!fn->size) {
		jffs2_free_node_frag(newfrag);
		return 0;
	}

	newfrag->ofs = fn->ofs;
	newfrag->size = fn->size;
	newfrag->node = fn;
	newfrag->node->frags = 1;
	newfrag->next = (void *)0xdeadbeef;

	/* Skip all the nodes which are completed before this one starts */
	while(this && fn->ofs >= this->ofs+this->size) {
		lastend = this->ofs + this->size;

		D2(printk(KERN_DEBUG "j_a_f_d_t_f: skipping frag 0x%04x-0x%04x; phys 0x%08x (*%p->%p)\n", 
			  this->ofs, this->ofs+this->size, this->node?(this->node->raw->flash_offset &~3):0xffffffff, this, this->next));
		prev = &this->next;
		this = this->next;
	}

	/* See if we ran off the end of the list */
	if (!this) {
		/* We did */
		if (lastend < fn->ofs) {
			/* ... and we need to put a hole in before the new node */
			struct jffs2_node_frag *holefrag = jffs2_alloc_node_frag();
			if (!holefrag)
				return -ENOMEM;
			holefrag->ofs = lastend;
			holefrag->size = fn->ofs - lastend;
			holefrag->next = NULL;
			holefrag->node = NULL;
			*prev = holefrag;
			prev = &holefrag->next;
		}
		newfrag->next = NULL;
		*prev = newfrag;
		return 0;
	}

	D2(printk(KERN_DEBUG "j_a_f_d_t_f: dealing with frag 0x%04x-0x%04x; phys 0x%08x (*%p->%p)\n", 
		  this->ofs, this->ofs+this->size, this->node?(this->node->raw->flash_offset &~3):0xffffffff, this, this->next));

	/* OK. 'this' is pointing at the first frag that fn->ofs at least partially obsoletes,
	 * - i.e. fn->ofs < this->ofs+this->size && fn->ofs >= this->ofs  
	 */
	if (fn->ofs > this->ofs) {
		/* This node isn't completely obsoleted. The start of it remains valid */
		if (this->ofs + this->size > fn->ofs + fn->size) {
			/* The new node splits 'this' frag into two */
			newfrag2 = jffs2_alloc_node_frag();
			if (!newfrag2) {
				jffs2_free_node_frag(newfrag);
				return -ENOMEM;
			}
			D1(printk(KERN_DEBUG "split old frag 0x%04x-0x%04x -->", this->ofs, this->ofs+this->size);
			if (this->node)
				printk("phys 0x%08x\n", this->node->raw->flash_offset &~3);
			else 
				printk("hole\n");
			   )
			newfrag2->ofs = fn->ofs + fn->size;
			newfrag2->size = (this->ofs+this->size) - newfrag2->ofs;
			newfrag2->next = this->next;
			newfrag2->node = this->node;
			if (this->node)
				this->node->frags++;
			newfrag->next = newfrag2;
			this->next = newfrag;
			this->size = newfrag->ofs - this->ofs;
			return 0;
		}
		/* New node just reduces 'this' frag in size, doesn't split it */
		this->size = fn->ofs - this->ofs;
		newfrag->next = this->next;
		this->next = newfrag;
		this = newfrag->next;
	} else {
		D2(printk(KERN_DEBUG "Inserting newfrag (*%p) in before 'this' (*%p)\n", newfrag, this));
		*prev = newfrag;
	        newfrag->next = this;
	}
	/* OK, now we have newfrag added in the correct place in the list, but
	   newfrag->next points to a fragment which may be overlapping it
	*/
	while (this && newfrag->ofs + newfrag->size >= this->ofs + this->size) {
		/* 'this' frag is obsoleted. */
		old = this;
		this = old->next;
		jffs2_obsolete_node_frag(c, old);
	}
	/* Now we're pointing at the first frag which isn't totally obsoleted by 
	   the new frag */
	newfrag->next = this;

	if (!this || newfrag->ofs + newfrag->size == this->ofs) {
		return 0;
	}
	/* Still some overlap */
	this->size = (this->ofs + this->size) - (newfrag->ofs + newfrag->size);
	this->ofs = newfrag->ofs + newfrag->size;
	return 0;
}

void jffs2_truncate_fraglist (struct jffs2_sb_info *c, struct jffs2_node_frag **list, __u32 size)
{
	D1(printk(KERN_DEBUG "Truncating fraglist to 0x%08x bytes\n", size));

	while (*list) {
		if ((*list)->ofs >= size) {
			struct jffs2_node_frag *this = *list;
			*list = this->next;
			D1(printk(KERN_DEBUG "Removing frag 0x%08x-0x%08x\n", this->ofs, this->ofs+this->size));
			jffs2_obsolete_node_frag(c, this);
			continue;
		} else if ((*list)->ofs + (*list)->size > size) {
			D1(printk(KERN_DEBUG "Truncating frag 0x%08x-0x%08x\n", (*list)->ofs, (*list)->ofs + (*list)->size));
			(*list)->size = size - (*list)->ofs;
		}
		list = &(*list)->next;
	}
}

/* Scan the list of all nodes present for this ino, build map of versions, etc. */

void jffs2_read_inode (struct inode *inode)
{
	struct jffs2_tmp_dnode_info *tn_list, *tn;
	struct jffs2_full_dirent *fd_list;
	struct jffs2_inode_info *f;
	struct jffs2_full_dnode *fn = NULL;
	struct jffs2_sb_info *c;
	struct jffs2_raw_inode latest_node;
	__u32 latest_mctime, mctime_ver;
	__u32 mdata_ver = 0;
	int ret;
	ssize_t retlen;

	D1(printk(KERN_DEBUG "jffs2_read_inode(): inode->i_ino == %lu\n", inode->i_ino));

	f = JFFS2_INODE_INFO(inode);
	c = JFFS2_SB_INFO(inode->i_sb);

	memset(f, 0, sizeof(*f));
	D2(printk(KERN_DEBUG "getting inocache\n"));
	init_MUTEX(&f->sem);
	f->inocache = jffs2_get_ino_cache(c, inode->i_ino);
	D2(printk(KERN_DEBUG "jffs2_read_inode(): Got inocache at %p\n", f->inocache));

	if (!f->inocache && inode->i_ino == 1) {
		/* Special case - no root inode on medium */
		f->inocache = jffs2_alloc_inode_cache();
		if (!f->inocache) {
			printk(KERN_CRIT "jffs2_read_inode(): Cannot allocate inocache for root inode\n");
			make_bad_inode(inode);
			return;
		}
		D1(printk(KERN_DEBUG "jffs2_read_inode(): Creating inocache for root inode\n"));
		memset(f->inocache, 0, sizeof(struct jffs2_inode_cache));
		f->inocache->ino = f->inocache->nlink = 1;
		f->inocache->nodes = (struct jffs2_raw_node_ref *)f->inocache;
		jffs2_add_ino_cache(c, f->inocache);
	}
	if (!f->inocache) {
		printk(KERN_WARNING "jffs2_read_inode() on nonexistent ino %lu\n", (unsigned long)inode->i_ino);
		make_bad_inode(inode);
		return;
	}
	D1(printk(KERN_DEBUG "jffs2_read_inode(): ino #%lu nlink is %d\n", (unsigned long)inode->i_ino, f->inocache->nlink));
	inode->i_nlink = f->inocache->nlink;

	/* Grab all nodes relevant to this ino */
	ret = jffs2_get_inode_nodes(c, inode->i_ino, f, &tn_list, &fd_list, &f->highest_version, &latest_mctime, &mctime_ver);

	if (ret) {
		printk(KERN_CRIT "jffs2_get_inode_nodes() for ino %lu returned %d\n", inode->i_ino, ret);
		make_bad_inode(inode);
		return;
	}
	f->dents = fd_list;

	while (tn_list) {
		tn = tn_list;

		fn = tn->fn;

		if (f->metadata && tn->version > mdata_ver) {
			D1(printk(KERN_DEBUG "Obsoleting old metadata at 0x%08x\n", f->metadata->raw->flash_offset &~3));
			jffs2_mark_node_obsolete(c, f->metadata->raw);
			jffs2_free_full_dnode(f->metadata);
			f->metadata = NULL;
			
			mdata_ver = 0;
		}

		if (fn->size) {
			jffs2_add_full_dnode_to_inode(c, f, fn);
		} else {
			/* Zero-sized node at end of version list. Just a metadata update */
			D1(printk(KERN_DEBUG "metadata @%08x: ver %d\n", fn->raw->flash_offset &~3, tn->version));
			f->metadata = fn;
			mdata_ver = tn->version;
		}
		tn_list = tn->next;
		jffs2_free_tmp_dnode_info(tn);
	}
	if (!fn) {
		/* No data nodes for this inode. */
		if (inode->i_ino != 1) {
			printk(KERN_WARNING "jffs2_read_inode(): No data nodes found for ino #%lu\n", inode->i_ino);
			if (!fd_list) {
				make_bad_inode(inode);
				return;
			}
			printk(KERN_WARNING "jffs2_read_inode(): But it has children so we fake some modes for it\n");
		}
		inode->i_mode = S_IFDIR | S_IRUGO | S_IWUSR | S_IXUGO;
		latest_node.version = 0;
		inode->i_atime = inode->i_ctime = inode->i_mtime = CURRENT_TIME;
		inode->i_nlink = f->inocache->nlink;
		inode->i_size = 0;
	} else {
		__u32 crc;

		ret = c->mtd->read(c->mtd, fn->raw->flash_offset & ~3, sizeof(latest_node), &retlen, (void *)&latest_node);
		if (ret || retlen != sizeof(latest_node)) {
			printk(KERN_NOTICE "MTD read in jffs2_read_inode() failed: Returned %d, %ld of %d bytes read\n",
			       ret, (long)retlen, sizeof(latest_node));
			jffs2_clear_inode(inode);
			make_bad_inode(inode);
			return;
		}

		crc = crc32(0, &latest_node, sizeof(latest_node)-8);
		if (crc != latest_node.node_crc) {
			printk(KERN_NOTICE "CRC failed for read_inode of inode %ld at physical location 0x%x\n", inode->i_ino, fn->raw->flash_offset & ~3);
			jffs2_clear_inode(inode);
			make_bad_inode(inode);
			return;
		}

		inode->i_mode = latest_node.mode;
		inode->i_uid = latest_node.uid;
		inode->i_gid = latest_node.gid;
		inode->i_size = latest_node.isize;
		if (S_ISREG(inode->i_mode))
			jffs2_truncate_fraglist(c, &f->fraglist, latest_node.isize);
		inode->i_atime = latest_node.atime;
		inode->i_mtime = latest_node.mtime;
		inode->i_ctime = latest_node.ctime;
	}

	/* OK, now the special cases. Certain inode types should
	   have only one data node, and it's kept as the metadata
	   node */
	if (S_ISBLK(inode->i_mode) || S_ISCHR(inode->i_mode) ||
	    S_ISLNK(inode->i_mode)) {
		if (f->metadata) {
			printk(KERN_WARNING "Argh. Special inode #%lu with mode 0%o had metadata node\n", inode->i_ino, inode->i_mode);
			jffs2_clear_inode(inode);
			make_bad_inode(inode);
			return;
		}
		if (!f->fraglist) {
			printk(KERN_WARNING "Argh. Special inode #%lu with mode 0%o has no fragments\n", inode->i_ino, inode->i_mode);
			jffs2_clear_inode(inode);
			make_bad_inode(inode);
			return;
		}
		/* ASSERT: f->fraglist != NULL */
		if (f->fraglist->next) {
			printk(KERN_WARNING "Argh. Special inode #%lu with mode 0%o had more than one node\n", inode->i_ino, inode->i_mode);
			/* FIXME: Deal with it - check crc32, check for duplicate node, check times and discard the older one */
			jffs2_clear_inode(inode);
			make_bad_inode(inode);
			return;
		}
		/* OK. We're happy */
		f->metadata = f->fraglist->node;
		jffs2_free_node_frag(f->fraglist);
		f->fraglist = NULL;
	}			
	    
	inode->i_blksize = PAGE_SIZE;
	inode->i_blocks = (inode->i_size + 511) >> 9;
	
	switch (inode->i_mode & S_IFMT) {
		unsigned short rdev;

	case S_IFLNK:
		inode->i_op = &jffs2_symlink_inode_operations;
		/* Hack to work around broken isize in old symlink code.
		   Remove this when dwmw2 comes to his senses and stops
		   symlinks from being an entirely gratuitous special
		   case. */
		if (!inode->i_size)
			inode->i_size = latest_node.dsize;
		break;
		
	case S_IFDIR:
		if (mctime_ver > latest_node.version) {
			/* The times in the latest_node are actually older than
			   mctime in the latest dirent. Cheat. */
			inode->i_mtime = inode->i_ctime = inode->i_atime = 
				latest_mctime;
		}
		inode->i_op = &jffs2_dir_inode_operations;
		inode->i_fop = &jffs2_dir_operations;
		break;

	case S_IFREG:
		inode->i_op = &jffs2_file_inode_operations;
		inode->i_fop = &jffs2_file_operations;
		inode->i_mapping->a_ops = &jffs2_file_address_operations;
		inode->i_mapping->nrpages = 0;
		break;

	case S_IFBLK:
	case S_IFCHR:
		/* Read the device numbers from the media */
		D1(printk(KERN_DEBUG "Reading device numbers from flash\n"));
		if (jffs2_read_dnode(c, f->metadata, (char *)&rdev, 0, sizeof(rdev)) < 0) {
			/* Eep */
			printk(KERN_NOTICE "Read device numbers for inode %lu failed\n", (unsigned long)inode->i_ino);
			jffs2_clear_inode(inode);
			make_bad_inode(inode);
			return;
		}			

	case S_IFSOCK:
	case S_IFIFO:
		inode->i_op = &jffs2_file_inode_operations;
		init_special_inode(inode, inode->i_mode, kdev_t_to_nr(MKDEV(rdev>>8, rdev&0xff)));
		break;

	default:
		printk(KERN_WARNING "jffs2_read_inode(): Bogus imode %o for ino %lu", inode->i_mode, (unsigned long)inode->i_ino);
	}
	D1(printk(KERN_DEBUG "jffs2_read_inode() returning\n"));
}

void jffs2_clear_inode (struct inode *inode)
{
	/* We can forget about this inode for now - drop all 
	 *  the nodelists associated with it, etc.
	 */
	struct jffs2_sb_info *c = JFFS2_SB_INFO(inode->i_sb);
	struct jffs2_node_frag *frag, *frags;
	struct jffs2_full_dirent *fd, *fds;
	struct jffs2_inode_info *f = JFFS2_INODE_INFO(inode);
        int deleted;

	D1(printk(KERN_DEBUG "jffs2_clear_inode(): ino #%lu mode %o\n", inode->i_ino, inode->i_mode));

	down(&f->sem);
	deleted = f->inocache && !f->inocache->nlink;

	frags = f->fraglist;
	fds = f->dents;
	if (f->metadata) {
		if (deleted)
			jffs2_mark_node_obsolete(c, f->metadata->raw);
		jffs2_free_full_dnode(f->metadata);
	}

	while (frags) {
		frag = frags;
		frags = frag->next;
		D2(printk(KERN_DEBUG "jffs2_clear_inode: frag at 0x%x-0x%x: node %p, frags %d--\n", frag->ofs, frag->ofs+frag->size, frag->node, frag->node?frag->node->frags:0));

		if (frag->node && !(--frag->node->frags)) {
			/* Not a hole, and it's the final remaining frag of this node. Free the node */
			if (deleted)
				jffs2_mark_node_obsolete(c, frag->node->raw);

			jffs2_free_full_dnode(frag->node);
		}
		jffs2_free_node_frag(frag);
	}
	while(fds) {
		fd = fds;
		fds = fd->next;
		jffs2_free_full_dirent(fd);
	}

	up(&f->sem);
};

