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
 * $Id: scan.c,v 1.51.2.4 2003/11/02 13:51:18 dwmw2 Exp $
 *
 */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/jffs2.h>
#include <linux/mtd/mtd.h>
#include <linux/pagemap.h>
#include "nodelist.h"
#include <linux/crc32.h>


#define DIRTY_SPACE(x) do { typeof(x) _x = (x); \
		c->free_size -= _x; c->dirty_size += _x; \
		jeb->free_size -= _x ; jeb->dirty_size += _x; \
		}while(0)
#define USED_SPACE(x) do { typeof(x) _x = (x); \
		c->free_size -= _x; c->used_size += _x; \
		jeb->free_size -= _x ; jeb->used_size += _x; \
		}while(0)

#define noisy_printk(noise, args...) do { \
	if (*(noise)) { \
		printk(KERN_NOTICE args); \
		 (*(noise))--; \
		 if (!(*(noise))) { \
			 printk(KERN_NOTICE "Further such events for this erase block will not be printed\n"); \
		 } \
	} \
} while(0)

static uint32_t pseudo_random;
static void jffs2_rotate_lists(struct jffs2_sb_info *c);

static int jffs2_scan_eraseblock (struct jffs2_sb_info *c, struct jffs2_eraseblock *jeb);

/* These helper functions _must_ increase ofs and also do the dirty/used space accounting. 
 * Returning an error will abort the mount - bad checksums etc. should just mark the space
 * as dirty.
 */
static int jffs2_scan_empty(struct jffs2_sb_info *c, struct jffs2_eraseblock *jeb, __u32 *ofs, int *noise);
static int jffs2_scan_inode_node(struct jffs2_sb_info *c, struct jffs2_eraseblock *jeb, __u32 *ofs);
static int jffs2_scan_dirent_node(struct jffs2_sb_info *c, struct jffs2_eraseblock *jeb, __u32 *ofs);


int jffs2_scan_medium(struct jffs2_sb_info *c)
{
	int i, ret;
	__u32 empty_blocks = 0;

	if (!c->blocks) {
		printk(KERN_WARNING "EEEK! c->blocks is NULL!\n");
		return -EINVAL;
	}
	for (i=0; i<c->nr_blocks; i++) {
		struct jffs2_eraseblock *jeb = &c->blocks[i];

		ret = jffs2_scan_eraseblock(c, jeb);
		if (ret < 0)
			return ret;

		ACCT_PARANOIA_CHECK(jeb);

		/* Now decide which list to put it on */
		if (ret == 1) {
			/* 
			 * Empty block.   Since we can't be sure it 
			 * was entirely erased, we just queue it for erase
			 * again.  It will be marked as such when the erase
			 * is complete.  Meanwhile we still count it as empty
			 * for later checks.
			 */
			list_add(&jeb->list, &c->erase_pending_list);
			empty_blocks++;
			c->nr_erasing_blocks++;
		} else if (jeb->used_size == PAD(sizeof(struct jffs2_unknown_node)) && !jeb->first_node->next_in_ino) {
			/* Only a CLEANMARKER node is valid */
			if (!jeb->dirty_size) {
				/* It's actually free */
				list_add(&jeb->list, &c->free_list);
				c->nr_free_blocks++;
			} else {
				/* Dirt */
				D1(printk(KERN_DEBUG "Adding all-dirty block at 0x%08x to erase_pending_list\n", jeb->offset));
				list_add(&jeb->list, &c->erase_pending_list);
				c->nr_erasing_blocks++;
			}
		} else if (jeb->used_size > c->sector_size - (2*sizeof(struct jffs2_raw_inode))) {
                        /* Full (or almost full) of clean data. Clean list */
                        list_add(&jeb->list, &c->clean_list);
                } else if (jeb->used_size) {
                        /* Some data, but not full. Dirty list. */
                        /* Except that we want to remember the block with most free space,
                           and stick it in the 'nextblock' position to start writing to it.
                           Later when we do snapshots, this must be the most recent block,
                           not the one with most free space.
                        */
                        if (jeb->free_size > 2*sizeof(struct jffs2_raw_inode) && 
                                (!c->nextblock || c->nextblock->free_size < jeb->free_size)) {
                                /* Better candidate for the next writes to go to */
                                if (c->nextblock)
                                        list_add(&c->nextblock->list, &c->dirty_list);
                                c->nextblock = jeb;
                        } else {
                                list_add(&jeb->list, &c->dirty_list);
                        }
		} else {
			/* Nothing valid - not even a clean marker. Needs erasing. */
                        /* For now we just put it on the erasing list. We'll start the erases later */
			printk(KERN_NOTICE "JFFS2: Erase block at 0x%08x is not formatted. It will be erased\n", jeb->offset);
                        list_add(&jeb->list, &c->erase_pending_list);
			c->nr_erasing_blocks++;
		}
	}
	/* Rotate the lists by some number to ensure wear levelling */
	jffs2_rotate_lists(c);

	if (c->nr_erasing_blocks) {
		if (!c->used_size && empty_blocks != c->nr_blocks) {
			printk(KERN_NOTICE "Cowardly refusing to erase blocks on filesystem with no valid JFFS2 nodes\n");
			return -EIO;
		}
		jffs2_erase_pending_trigger(c);
	}
	return 0;
}

static int jffs2_scan_eraseblock (struct jffs2_sb_info *c, struct jffs2_eraseblock *jeb) {
	struct jffs2_unknown_node node;
	__u32 ofs, prevofs;
	__u32 hdr_crc, nodetype;
	int err;
	int noise = 0;

	ofs = jeb->offset;
	prevofs = jeb->offset - 1;

	D1(printk(KERN_DEBUG "jffs2_scan_eraseblock(): Scanning block at 0x%x\n", ofs));

	err = jffs2_scan_empty(c, jeb, &ofs, &noise);
	if (err) return err;
	if (ofs == jeb->offset + c->sector_size) {
		D1(printk(KERN_DEBUG "Block at 0x%08x is empty (erased)\n", jeb->offset));
		return 1;	/* special return code */
	}
	
	noise = 10;

	while(ofs < jeb->offset + c->sector_size) {
		ssize_t retlen;
		ACCT_PARANOIA_CHECK(jeb);
		
		if (ofs & 3) {
			printk(KERN_WARNING "Eep. ofs 0x%08x not word-aligned!\n", ofs);
			ofs = (ofs+3)&~3;
			continue;
		}
		if (ofs == prevofs) {
			printk(KERN_WARNING "ofs 0x%08x has already been seen. Skipping\n", ofs);
			DIRTY_SPACE(4);
			ofs += 4;
			continue;
		}
		prevofs = ofs;
		
		if (jeb->offset + c->sector_size < ofs + sizeof(node)) {
			D1(printk(KERN_DEBUG "Fewer than %d bytes left to end of block. Not reading\n", sizeof(struct jffs2_unknown_node)));
			DIRTY_SPACE((jeb->offset + c->sector_size)-ofs);
			break;
		}

		err = c->mtd->read(c->mtd, ofs, sizeof(node), &retlen, (char *)&node);
		
		if (err) {
			D1(printk(KERN_WARNING "mtd->read(0x%x bytes from 0x%x) returned %d\n", sizeof(node), ofs, err));
			return err;
		}
		if (retlen < sizeof(node)) {
			D1(printk(KERN_WARNING "Read at 0x%x gave only 0x%x bytes\n", ofs, retlen));
			DIRTY_SPACE(retlen);
			ofs += retlen;
			continue;
		}

		if (node.magic == JFFS2_EMPTY_BITMASK && node.nodetype == JFFS2_EMPTY_BITMASK) {
			D1(printk(KERN_DEBUG "Found empty flash at 0x%x\n", ofs));
			err = jffs2_scan_empty(c, jeb, &ofs, &noise);
			if (err) return err;
			continue;
		}

		if (ofs == jeb->offset && node.magic == KSAMTIB_CIGAM_2SFFJ) {
			printk(KERN_WARNING "Magic bitmask is backwards at offset 0x%08x. Wrong endian filesystem?\n", ofs);
			DIRTY_SPACE(4);
			ofs += 4;
			continue;
		}
		if (node.magic == JFFS2_DIRTY_BITMASK) {
			D1(printk(KERN_DEBUG "Empty bitmask at 0x%08x\n", ofs));
			DIRTY_SPACE(4);
			ofs += 4;
			continue;
		}
		if (node.magic == JFFS2_OLD_MAGIC_BITMASK) {
			printk(KERN_WARNING "Old JFFS2 bitmask found at 0x%08x\n", ofs);
			printk(KERN_WARNING "You cannot use older JFFS2 filesystems with newer kernels\n");
			DIRTY_SPACE(4);
			ofs += 4;
			continue;
		}
		if (node.magic != JFFS2_MAGIC_BITMASK) {
			/* OK. We're out of possibilities. Whinge and move on */
			noisy_printk(&noise, "jffs2_scan_eraseblock(): Magic bitmask 0x%04x not found at 0x%08x: 0x%04x instead\n", JFFS2_MAGIC_BITMASK, ofs, node.magic);
			DIRTY_SPACE(4);
			ofs += 4;
			continue;
		}
		/* We seem to have a node of sorts. Check the CRC */
		nodetype = node.nodetype;
		node.nodetype |= JFFS2_NODE_ACCURATE;
		hdr_crc = crc32(0, &node, sizeof(node)-4);
		node.nodetype = nodetype;
		if (hdr_crc != node.hdr_crc) {
			noisy_printk(&noise, "jffs2_scan_eraseblock(): Node at 0x%08x {0x%04x, 0x%04x, 0x%08x) has invalid CRC 0x%08x (calculated 0x%08x)\n",
				     ofs, node.magic, node.nodetype, node.totlen, node.hdr_crc, hdr_crc);
			DIRTY_SPACE(4);
			ofs += 4;
			continue;
		}

		if (ofs + node.totlen > jeb->offset + c->sector_size) {
			/* Eep. Node goes over the end of the erase block. */
			printk(KERN_WARNING "Node at 0x%08x with length 0x%08x would run over the end of the erase block\n",
			       ofs, node.totlen);
			printk(KERN_WARNING "Perhaps the file system was created with the wrong erase size?\n");
			DIRTY_SPACE(4);
			ofs += 4;
			continue;
		}

		switch(node.nodetype | JFFS2_NODE_ACCURATE) {
		case JFFS2_NODETYPE_INODE:
			err = jffs2_scan_inode_node(c, jeb, &ofs);
			if (err) return err;
			break;
			
		case JFFS2_NODETYPE_DIRENT:
			err = jffs2_scan_dirent_node(c, jeb, &ofs);
			if (err) return err;
			break;

		case JFFS2_NODETYPE_CLEANMARKER:
			if (node.totlen != sizeof(struct jffs2_unknown_node)) {
				printk(KERN_NOTICE "CLEANMARKER node found at 0x%08x has totlen 0x%x != normal 0x%x\n", 
				       ofs, node.totlen, sizeof(struct jffs2_unknown_node));
				DIRTY_SPACE(PAD(sizeof(struct jffs2_unknown_node)));
			} else if (jeb->first_node) {
				printk(KERN_NOTICE "CLEANMARKER node found at 0x%08x, not first node in block (0x%08x)\n", ofs, jeb->offset);
				DIRTY_SPACE(PAD(sizeof(struct jffs2_unknown_node)));
				ofs += PAD(sizeof(struct jffs2_unknown_node));
				continue;
			} else {
				struct jffs2_raw_node_ref *marker_ref = jffs2_alloc_raw_node_ref();
				if (!marker_ref) {
					printk(KERN_NOTICE "Failed to allocate node ref for clean marker\n");
					return -ENOMEM;
				}
				marker_ref->next_in_ino = NULL;
				marker_ref->next_phys = NULL;
				marker_ref->flash_offset = ofs;
				marker_ref->totlen = sizeof(struct jffs2_unknown_node);
				jeb->first_node = jeb->last_node = marker_ref;
			     
				USED_SPACE(PAD(sizeof(struct jffs2_unknown_node)));
			}
			ofs += PAD(sizeof(struct jffs2_unknown_node));
			break;

		default:
			switch (node.nodetype & JFFS2_COMPAT_MASK) {
			case JFFS2_FEATURE_ROCOMPAT:
				printk(KERN_NOTICE "Read-only compatible feature node (0x%04x) found at offset 0x%08x\n", node.nodetype, ofs);
			        c->flags |= JFFS2_SB_FLAG_RO;
				if (!(OFNI_BS_2SFFJ(c)->s_flags & MS_RDONLY))
					return -EROFS;
				DIRTY_SPACE(PAD(node.totlen));
				ofs += PAD(node.totlen);
				continue;

			case JFFS2_FEATURE_INCOMPAT:
				printk(KERN_NOTICE "Incompatible feature node (0x%04x) found at offset 0x%08x\n", node.nodetype, ofs);
				return -EINVAL;

			case JFFS2_FEATURE_RWCOMPAT_DELETE:
				printk(KERN_NOTICE "Unknown but compatible feature node (0x%04x) found at offset 0x%08x\n", node.nodetype, ofs);
				DIRTY_SPACE(PAD(node.totlen));
				ofs += PAD(node.totlen);
				break;

			case JFFS2_FEATURE_RWCOMPAT_COPY:
				printk(KERN_NOTICE "Unknown but compatible feature node (0x%04x) found at offset 0x%08x\n", node.nodetype, ofs);
				USED_SPACE(PAD(node.totlen));
				ofs += PAD(node.totlen);
				break;
			}
		}
	}
	D1(printk(KERN_DEBUG "Block at 0x%08x: free 0x%08x, dirty 0x%08x, used 0x%08x\n", jeb->offset, 
		  jeb->free_size, jeb->dirty_size, jeb->used_size));
	return 0;
}

/* We're pointing at the first empty word on the flash. Scan and account for the whole dirty region */
static int jffs2_scan_empty(struct jffs2_sb_info *c, struct jffs2_eraseblock *jeb, __u32 *startofs, int *noise)
{
	__u32 *buf;
	__u32 scanlen = (jeb->offset + c->sector_size) - *startofs;
	__u32 curofs = *startofs;
	
	buf = kmalloc(min((__u32)PAGE_SIZE, scanlen), GFP_KERNEL);
	if (!buf) {
		printk(KERN_WARNING "Scan buffer allocation failed\n");
		return -ENOMEM;
	}
	while(scanlen) {
		ssize_t retlen;
		int ret, i;
		
		ret = c->mtd->read(c->mtd, curofs, min((__u32)PAGE_SIZE, scanlen), &retlen, (char *)buf);
		if(ret) {
			D1(printk(KERN_WARNING "jffs2_scan_empty(): Read 0x%x bytes at 0x%08x returned %d\n", min((__u32)PAGE_SIZE, scanlen), curofs, ret));
			kfree(buf);
			return ret;
		}
		if (retlen < 4) {
			D1(printk(KERN_WARNING "Eep. too few bytes read in scan_empty()\n"));
			kfree(buf);
			return -EIO;
		}
		for (i=0; i<(retlen / 4); i++) {
			if (buf[i] != 0xffffffff) {
				curofs += i*4;

				noisy_printk(noise, "jffs2_scan_empty(): Empty block at 0x%08x ends at 0x%08x (with 0x%08x)! Marking dirty\n", *startofs, curofs, buf[i]);
				DIRTY_SPACE(curofs - (*startofs));
				*startofs = curofs;
				kfree(buf);
				return 0;
			}
		}
		scanlen -= retlen&~3;
		curofs += retlen&~3;
	}

	D1(printk(KERN_DEBUG "Empty flash detected from 0x%08x to 0x%08x\n", *startofs, curofs));
	kfree(buf);
	*startofs = curofs;
	return 0;
}

static struct jffs2_inode_cache *jffs2_scan_make_ino_cache(struct jffs2_sb_info *c, __u32 ino)
{
	struct jffs2_inode_cache *ic;

	ic = jffs2_get_ino_cache(c, ino);
	if (ic)
		return ic;

	ic = jffs2_alloc_inode_cache();
	if (!ic) {
		printk(KERN_NOTICE "jffs2_scan_make_inode_cache(): allocation of inode cache failed\n");
		return NULL;
	}
	memset(ic, 0, sizeof(*ic));
	ic->scan = kmalloc(sizeof(struct jffs2_scan_info), GFP_KERNEL);
	if (!ic->scan) {
		printk(KERN_NOTICE "jffs2_scan_make_inode_cache(): allocation of scan info for inode cache failed\n");
		jffs2_free_inode_cache(ic);
		return NULL;
	}
	memset(ic->scan, 0, sizeof(*ic->scan));
	ic->ino = ino;
	ic->nodes = (void *)ic;
	jffs2_add_ino_cache(c, ic);
	if (ino == 1)
		ic->nlink=1;
	return ic;
}

static int jffs2_scan_inode_node(struct jffs2_sb_info *c, struct jffs2_eraseblock *jeb, __u32 *ofs)
{
	struct jffs2_raw_node_ref *raw;
	struct jffs2_full_dnode *fn;
	struct jffs2_tmp_dnode_info *tn, **tn_list;
	struct jffs2_inode_cache *ic;
	struct jffs2_raw_inode ri;
	__u32 crc;
	__u16 oldnodetype;
	int ret;
	ssize_t retlen;

	D1(printk(KERN_DEBUG "jffs2_scan_inode_node(): Node at 0x%08x\n", *ofs));

	ret = c->mtd->read(c->mtd, *ofs, sizeof(ri), &retlen, (char *)&ri);
	if (ret) {
		printk(KERN_NOTICE "jffs2_scan_inode_node(): Read error at 0x%08x: %d\n", *ofs, ret);
		return ret;
	}
	if (retlen != sizeof(ri)) {
		printk(KERN_NOTICE "Short read: 0x%x bytes at 0x%08x instead of requested %x\n", 
		       retlen, *ofs, sizeof(ri));
		return -EIO;
	}

	/* We sort of assume that the node was accurate when it was 
	   first written to the medium :) */
	oldnodetype = ri.nodetype;
	ri.nodetype |= JFFS2_NODE_ACCURATE;
	crc = crc32(0, &ri, sizeof(ri)-8);
	ri.nodetype = oldnodetype;

	if(crc != ri.node_crc) {
		printk(KERN_NOTICE "jffs2_scan_inode_node(): CRC failed on node at 0x%08x: Read 0x%08x, calculated 0x%08x\n",
		       *ofs, ri.node_crc, crc);
		/* FIXME: Why do we believe totlen? */
		DIRTY_SPACE(4);
		*ofs += 4;
		return 0;
	}
	/* There was a bug where we wrote hole nodes out with csize/dsize
	   swapped. Deal with it */
	if (ri.compr == JFFS2_COMPR_ZERO && !ri.dsize && ri.csize) {
		ri.dsize = ri.csize;
		ri.csize = 0;
	}

	if (ri.csize) {
		/* Check data CRC too */
		unsigned char *dbuf;
		__u32 crc;

		dbuf = kmalloc(PAGE_CACHE_SIZE, GFP_KERNEL);
		if (!dbuf) {
			printk(KERN_NOTICE "jffs2_scan_inode_node(): allocation of temporary data buffer for CRC check failed\n");
			return -ENOMEM;
		}
		ret = c->mtd->read(c->mtd, *ofs+sizeof(ri), ri.csize, &retlen, dbuf);
		if (ret) {
			printk(KERN_NOTICE "jffs2_scan_inode_node(): Read error at 0x%08x: %d\n", *ofs+sizeof(ri), ret);
			kfree(dbuf);
			return ret;
		}
		if (retlen != ri.csize) {
			printk(KERN_NOTICE "Short read: 0x%x bytes at 0x%08x instead of requested %x\n", 
			       retlen, *ofs+ sizeof(ri), ri.csize);
			kfree(dbuf);
			return -EIO;
		}
		crc = crc32(0, dbuf, ri.csize);
		kfree(dbuf);
		if (crc != ri.data_crc) {
			printk(KERN_NOTICE "jffs2_scan_inode_node(): Data CRC failed on node at 0x%08x: Read 0x%08x, calculated 0x%08x\n",
			       *ofs, ri.data_crc, crc);
			DIRTY_SPACE(PAD(ri.totlen));
			*ofs += PAD(ri.totlen);
			return 0;
		}
	}

	/* Wheee. It worked */
	raw = jffs2_alloc_raw_node_ref();
	if (!raw) {
		printk(KERN_NOTICE "jffs2_scan_inode_node(): allocation of node reference failed\n");
		return -ENOMEM;
	}
	tn = jffs2_alloc_tmp_dnode_info();
	if (!tn) {
		jffs2_free_raw_node_ref(raw);
		return -ENOMEM;
	}
	fn = jffs2_alloc_full_dnode();
	if (!fn) {
		jffs2_free_tmp_dnode_info(tn);
		jffs2_free_raw_node_ref(raw);
		return -ENOMEM;
	}
	ic = jffs2_scan_make_ino_cache(c, ri.ino);
	if (!ic) {
		jffs2_free_full_dnode(fn);
		jffs2_free_tmp_dnode_info(tn);
		jffs2_free_raw_node_ref(raw);
		return -ENOMEM;
	}

	/* Build the data structures and file them for later */
	raw->flash_offset = *ofs;
	raw->totlen = PAD(ri.totlen);
	raw->next_phys = NULL;
	raw->next_in_ino = ic->nodes;
	ic->nodes = raw;
	if (!jeb->first_node)
		jeb->first_node = raw;
	if (jeb->last_node)
		jeb->last_node->next_phys = raw;
	jeb->last_node = raw;

	D1(printk(KERN_DEBUG "Node is ino #%u, version %d. Range 0x%x-0x%x\n", 
		  ri.ino, ri.version, ri.offset, ri.offset+ri.dsize));

	pseudo_random += ri.version;

	for (tn_list = &ic->scan->tmpnodes; *tn_list; tn_list = &((*tn_list)->next)) {
		if ((*tn_list)->version < ri.version)
			continue;
		if ((*tn_list)->version > ri.version) 
			break;
		/* Wheee. We've found another instance of the same version number.
		   We should obsolete one of them. 
		*/
		D1(printk(KERN_DEBUG "Duplicate version %d found in ino #%u. Previous one is at 0x%08x\n", ri.version, ic->ino, (*tn_list)->fn->raw->flash_offset &~3));
		if (!jeb->used_size) {
			D1(printk(KERN_DEBUG "No valid nodes yet found in this eraseblock 0x%08x, so obsoleting the new instance at 0x%08x\n", 
				  jeb->offset, raw->flash_offset & ~3));
			ri.nodetype &= ~JFFS2_NODE_ACCURATE;
			/* Perhaps we could also mark it as such on the medium. Maybe later */
		}
		break;
	}

	if (ri.nodetype & JFFS2_NODE_ACCURATE) {
		memset(fn,0,sizeof(*fn));

		fn->ofs = ri.offset;
		fn->size = ri.dsize;
		fn->frags = 0;
		fn->raw = raw;

		tn->next = NULL;
		tn->fn = fn;
		tn->version = ri.version;

		USED_SPACE(PAD(ri.totlen));
		jffs2_add_tn_to_list(tn, &ic->scan->tmpnodes);
		/* Make sure the one we just added is the _last_ in the list
		   with this version number, so the older ones get obsoleted */
		while (tn->next && tn->next->version == tn->version) {

			D1(printk(KERN_DEBUG "Shifting new node at 0x%08x after other node at 0x%08x for version %d in list\n",
				  fn->raw->flash_offset&~3, tn->next->fn->raw->flash_offset &~3, ri.version));

			if(tn->fn != fn)
				BUG();
			tn->fn = tn->next->fn;
			tn->next->fn = fn;
			tn = tn->next;
		}
	} else {
		jffs2_free_full_dnode(fn);
		jffs2_free_tmp_dnode_info(tn);
		raw->flash_offset |= 1;
		DIRTY_SPACE(PAD(ri.totlen));
	}		
	*ofs += PAD(ri.totlen);
	return 0;
}

static int jffs2_scan_dirent_node(struct jffs2_sb_info *c, struct jffs2_eraseblock *jeb, __u32 *ofs)
{
	struct jffs2_raw_node_ref *raw;
	struct jffs2_full_dirent *fd;
	struct jffs2_inode_cache *ic;
	struct jffs2_raw_dirent rd;
	__u16 oldnodetype;
	int ret;
	__u32 crc;
	ssize_t retlen;

	D1(printk(KERN_DEBUG "jffs2_scan_dirent_node(): Node at 0x%08x\n", *ofs));

	ret = c->mtd->read(c->mtd, *ofs, sizeof(rd), &retlen, (char *)&rd);
	if (ret) {
		printk(KERN_NOTICE "jffs2_scan_dirent_node(): Read error at 0x%08x: %d\n", *ofs, ret);
		return ret;
	}
	if (retlen != sizeof(rd)) {
		printk(KERN_NOTICE "Short read: 0x%x bytes at 0x%08x instead of requested %x\n", 
		       retlen, *ofs, sizeof(rd));
		return -EIO;
	}

	/* We sort of assume that the node was accurate when it was 
	   first written to the medium :) */
	oldnodetype = rd.nodetype;
	rd.nodetype |= JFFS2_NODE_ACCURATE;
	crc = crc32(0, &rd, sizeof(rd)-8);
	rd.nodetype = oldnodetype;

	if (crc != rd.node_crc) {
		printk(KERN_NOTICE "jffs2_scan_dirent_node(): Node CRC failed on node at 0x%08x: Read 0x%08x, calculated 0x%08x\n",
		       *ofs, rd.node_crc, crc);
		/* FIXME: Why do we believe totlen? */
		DIRTY_SPACE(4);
		*ofs += 4;
		return 0;
	}

	pseudo_random += rd.version;

	fd = jffs2_alloc_full_dirent(rd.nsize+1);
	if (!fd) {
		return -ENOMEM;
}
	ret = c->mtd->read(c->mtd, *ofs + sizeof(rd), rd.nsize, &retlen, &fd->name[0]);
	if (ret) {
		jffs2_free_full_dirent(fd);
		printk(KERN_NOTICE "jffs2_scan_dirent_node(): Read error at 0x%08x: %d\n", 
		       *ofs + sizeof(rd), ret);
		return ret;
	}
	if (retlen != rd.nsize) {
		jffs2_free_full_dirent(fd);
		printk(KERN_NOTICE "Short read: 0x%x bytes at 0x%08x instead of requested %x\n", 
		       retlen, *ofs + sizeof(rd), rd.nsize);
		return -EIO;
	}
	crc = crc32(0, fd->name, rd.nsize);
	if (crc != rd.name_crc) {
		printk(KERN_NOTICE "jffs2_scan_dirent_node(): Name CRC failed on node at 0x%08x: Read 0x%08x, calculated 0x%08x\n",
		       *ofs, rd.name_crc, crc);	
		fd->name[rd.nsize]=0;
		D1(printk(KERN_NOTICE "Name for which CRC failed is (now) '%s', ino #%d\n", fd->name, rd.ino));
		jffs2_free_full_dirent(fd);
		/* FIXME: Why do we believe totlen? */
		DIRTY_SPACE(PAD(rd.totlen));
		*ofs += PAD(rd.totlen);
		return 0;
	}
	raw = jffs2_alloc_raw_node_ref();
	if (!raw) {
		jffs2_free_full_dirent(fd);
		printk(KERN_NOTICE "jffs2_scan_dirent_node(): allocation of node reference failed\n");
		return -ENOMEM;
	}
	ic = jffs2_scan_make_ino_cache(c, rd.pino);
	if (!ic) {
		jffs2_free_full_dirent(fd);
		jffs2_free_raw_node_ref(raw);
		return -ENOMEM;
	}
	
	raw->totlen = PAD(rd.totlen);
	raw->flash_offset = *ofs;
	raw->next_phys = NULL;
	raw->next_in_ino = ic->nodes;
	ic->nodes = raw;
	if (!jeb->first_node)
		jeb->first_node = raw;
	if (jeb->last_node)
		jeb->last_node->next_phys = raw;
	jeb->last_node = raw;

	if (rd.nodetype & JFFS2_NODE_ACCURATE) {
		fd->raw = raw;
		fd->next = NULL;
		fd->version = rd.version;
		fd->ino = rd.ino;
		fd->name[rd.nsize]=0;
		fd->nhash = full_name_hash(fd->name, rd.nsize);
		fd->type = rd.type;

		USED_SPACE(PAD(rd.totlen));
		jffs2_add_fd_to_list(c, fd, &ic->scan->dents);
	} else {
		raw->flash_offset |= 1;
		jffs2_free_full_dirent(fd);

		DIRTY_SPACE(PAD(rd.totlen));
	} 
	*ofs += PAD(rd.totlen);
	return 0;
}

static int count_list(struct list_head *l)
{
	uint32_t count = 0;
	struct list_head *tmp;

	list_for_each(tmp, l) {
		count++;
	}
	return count;
}

/* Note: This breaks if list_empty(head). I don't care. You
   might, if you copy this code and use it elsewhere :) */
static void rotate_list(struct list_head *head, uint32_t count)
{
	struct list_head *n = head->next;

	list_del(head);
	while(count--)
		n = n->next;
	list_add(head, n);
}

static void jffs2_rotate_lists(struct jffs2_sb_info *c)
{
	uint32_t x;

	x = count_list(&c->clean_list);
	if (x)
		rotate_list((&c->clean_list), pseudo_random % x);

	x = count_list(&c->dirty_list);
	if (x)
		rotate_list((&c->dirty_list), pseudo_random % x);

	if (c->nr_erasing_blocks)
		rotate_list((&c->erase_pending_list), pseudo_random % c->nr_erasing_blocks);

	if (c->nr_free_blocks) /* Not that it should ever be zero */
		rotate_list((&c->free_list), pseudo_random % c->nr_free_blocks);
}
