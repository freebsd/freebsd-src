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
 * $Id: nodelist.c,v 1.30.2.6 2003/02/24 21:49:33 dwmw2 Exp $
 *
 */

#include <linux/kernel.h>
#include <linux/jffs2.h>
#include <linux/fs.h>
#include <linux/mtd/mtd.h>
#include "nodelist.h"

void jffs2_add_fd_to_list(struct jffs2_sb_info *c, struct jffs2_full_dirent *new, struct jffs2_full_dirent **list)
{
	struct jffs2_full_dirent **prev = list;
	D1(printk(KERN_DEBUG "jffs2_add_fd_to_list( %p, %p (->%p))\n", new, list, *list));

	while ((*prev) && (*prev)->nhash <= new->nhash) {
		if ((*prev)->nhash == new->nhash && !strcmp((*prev)->name, new->name)) {
			/* Duplicate. Free one */
			if (new->version < (*prev)->version) {
				D1(printk(KERN_DEBUG "Eep! Marking new dirent node obsolete\n"));
				D1(printk(KERN_DEBUG "New dirent is \"%s\"->ino #%u. Old is \"%s\"->ino #%u\n", new->name, new->ino, (*prev)->name, (*prev)->ino));
				jffs2_mark_node_obsolete(c, new->raw);
				jffs2_free_full_dirent(new);
			} else {
				D1(printk(KERN_DEBUG "Marking old dirent node (ino #%u) obsolete\n", (*prev)->ino));
				new->next = (*prev)->next;
				jffs2_mark_node_obsolete(c, ((*prev)->raw));
				jffs2_free_full_dirent(*prev);
				*prev = new;
			}
			goto out;
		}
		prev = &((*prev)->next);
	}
	new->next = *prev;
	*prev = new;

 out:
	D2(while(*list) {
		printk(KERN_DEBUG "Dirent \"%s\" (hash 0x%08x, ino #%u\n", (*list)->name, (*list)->nhash, (*list)->ino);
		list = &(*list)->next;
	});
}

/* Put a new tmp_dnode_info into the list, keeping the list in 
   order of increasing version
*/
void jffs2_add_tn_to_list(struct jffs2_tmp_dnode_info *tn, struct jffs2_tmp_dnode_info **list)
{
	struct jffs2_tmp_dnode_info **prev = list;
	
	while ((*prev) && (*prev)->version < tn->version) {
		prev = &((*prev)->next);
	}
	tn->next = (*prev);
        *prev = tn;
}

/* Get tmp_dnode_info and full_dirent for all non-obsolete nodes associated
   with this ino, returning the former in order of version */

int jffs2_get_inode_nodes(struct jffs2_sb_info *c, ino_t ino, struct jffs2_inode_info *f,
			  struct jffs2_tmp_dnode_info **tnp, struct jffs2_full_dirent **fdp,
			  __u32 *highest_version, __u32 *latest_mctime,
			  __u32 *mctime_ver)
{
	struct jffs2_raw_node_ref *ref = f->inocache->nodes;
	struct jffs2_tmp_dnode_info *tn, *ret_tn = NULL;
	struct jffs2_full_dirent *fd, *ret_fd = NULL;

	union jffs2_node_union node;
	size_t retlen;
	int err;

	*mctime_ver = 0;

	D1(printk(KERN_DEBUG "jffs2_get_inode_nodes(): ino #%lu\n", ino));
	if (!f->inocache->nodes) {
		printk(KERN_WARNING "Eep. no nodes for ino #%lu\n", ino);
	}
	for (ref = f->inocache->nodes; ref && ref->next_in_ino; ref = ref->next_in_ino) {
		/* Work out whether it's a data node or a dirent node */
		if (ref->flash_offset & 1) {
			/* FIXME: On NAND flash we may need to read these */
			D1(printk(KERN_DEBUG "node at 0x%08x is obsoleted. Ignoring.\n", ref->flash_offset &~3));
			continue;
		}
		err = c->mtd->read(c->mtd, (ref->flash_offset & ~3), min(ref->totlen, sizeof(node)), &retlen, (void *)&node);
		if (err) {
			printk(KERN_WARNING "error %d reading node at 0x%08x in get_inode_nodes()\n", err, (ref->flash_offset) & ~3);
			goto free_out;
		}
			

			/* Check we've managed to read at least the common node header */
		if (retlen < min(ref->totlen, sizeof(node.u))) {
			printk(KERN_WARNING "short read in get_inode_nodes()\n");
			err = -EIO;
			goto free_out;
		}
			
		switch (node.u.nodetype) {
		case JFFS2_NODETYPE_DIRENT:
			D1(printk(KERN_DEBUG "Node at %08x is a dirent node\n", ref->flash_offset &~3));
			if (retlen < sizeof(node.d)) {
				printk(KERN_WARNING "short read in get_inode_nodes()\n");
				err = -EIO;
				goto free_out;
			}
			if (node.d.version > *highest_version)
				*highest_version = node.d.version;
			if (ref->flash_offset & 1) {
				/* Obsoleted */
				continue;
			}
			fd = jffs2_alloc_full_dirent(node.d.nsize+1);
			if (!fd) {
				err = -ENOMEM;
				goto free_out;
			}
			memset(fd,0,sizeof(struct jffs2_full_dirent) + node.d.nsize+1);
			fd->raw = ref;
			fd->version = node.d.version;
			fd->ino = node.d.ino;
			fd->type = node.d.type;

			/* Pick out the mctime of the latest dirent */
			if(fd->version > *mctime_ver) {
				*mctime_ver = fd->version;
				*latest_mctime = node.d.mctime;
			}

			/* memcpy as much of the name as possible from the raw
			   dirent we've already read from the flash
			*/
			if (retlen > sizeof(struct jffs2_raw_dirent))
				memcpy(&fd->name[0], &node.d.name[0], min((__u32)node.d.nsize, (retlen-sizeof(struct jffs2_raw_dirent))));
				
			/* Do we need to copy any more of the name directly
			   from the flash?
			*/
			if (node.d.nsize + sizeof(struct jffs2_raw_dirent) > retlen) {
				int already = retlen - sizeof(struct jffs2_raw_dirent);
					
				err = c->mtd->read(c->mtd, (ref->flash_offset & ~3) + retlen, 
						   node.d.nsize - already, &retlen, &fd->name[already]);
				if (!err && retlen != node.d.nsize - already)
					err = -EIO;
					
				if (err) {
					printk(KERN_WARNING "Read remainder of name in jffs2_get_inode_nodes(): error %d\n", err);
					jffs2_free_full_dirent(fd);
					goto free_out;
				}
			}
			fd->nhash = full_name_hash(fd->name, node.d.nsize);
			fd->next = NULL;
				/* Wheee. We now have a complete jffs2_full_dirent structure, with
				   the name in it and everything. Link it into the list 
				*/
			D1(printk(KERN_DEBUG "Adding fd \"%s\", ino #%u\n", fd->name, fd->ino));
			jffs2_add_fd_to_list(c, fd, &ret_fd);
			break;

		case JFFS2_NODETYPE_INODE:
			D1(printk(KERN_DEBUG "Node at %08x is a data node\n", ref->flash_offset &~3));
			if (retlen < sizeof(node.i)) {
				printk(KERN_WARNING "read too short for dnode\n");
				err = -EIO;
				goto free_out;
			}
			if (node.i.version > *highest_version)
				*highest_version = node.i.version;
			D1(printk(KERN_DEBUG "version %d, highest_version now %d\n", node.i.version, *highest_version));

			if (ref->flash_offset & 1) {
				D1(printk(KERN_DEBUG "obsoleted\n"));
				/* Obsoleted */
				continue;
			}
			tn = jffs2_alloc_tmp_dnode_info();
			if (!tn) {
				D1(printk(KERN_DEBUG "alloc tn failed\n"));
				err = -ENOMEM;
				goto free_out;
			}

			tn->fn = jffs2_alloc_full_dnode();
			if (!tn->fn) {
				D1(printk(KERN_DEBUG "alloc fn failed\n"));
				err = -ENOMEM;
				jffs2_free_tmp_dnode_info(tn);
				goto free_out;
			}
			tn->version = node.i.version;
			tn->fn->ofs = node.i.offset;
			/* There was a bug where we wrote hole nodes out with
			   csize/dsize swapped. Deal with it */
			if (node.i.compr == JFFS2_COMPR_ZERO && !node.i.dsize && node.i.csize)
				tn->fn->size = node.i.csize;
			else // normal case...
				tn->fn->size = node.i.dsize;
			tn->fn->raw = ref;
			D1(printk(KERN_DEBUG "dnode @%08x: ver %u, offset %04x, dsize %04x\n", ref->flash_offset &~3, node.i.version, node.i.offset, node.i.dsize));
			jffs2_add_tn_to_list(tn, &ret_tn);
			break;

		default:
			switch(node.u.nodetype & JFFS2_COMPAT_MASK) {
			case JFFS2_FEATURE_INCOMPAT:
				printk(KERN_NOTICE "Unknown INCOMPAT nodetype %04X at %08X\n", node.u.nodetype, ref->flash_offset & ~3);
				break;
			case JFFS2_FEATURE_ROCOMPAT:
				printk(KERN_NOTICE "Unknown ROCOMPAT nodetype %04X at %08X\n", node.u.nodetype, ref->flash_offset & ~3);
				break;
			case JFFS2_FEATURE_RWCOMPAT_COPY:
				printk(KERN_NOTICE "Unknown RWCOMPAT_COPY nodetype %04X at %08X\n", node.u.nodetype, ref->flash_offset & ~3);
				break;
			case JFFS2_FEATURE_RWCOMPAT_DELETE:
				printk(KERN_NOTICE "Unknown RWCOMPAT_DELETE nodetype %04X at %08X\n", node.u.nodetype, ref->flash_offset & ~3);
				break;
			}
		}
	}
	*tnp = ret_tn;
	*fdp = ret_fd;

	return 0;

 free_out:
	jffs2_free_tmp_dnode_info_list(ret_tn);
	jffs2_free_full_dirent_list(ret_fd);
	return err;
}

struct jffs2_inode_cache *jffs2_get_ino_cache(struct jffs2_sb_info *c, uint32_t ino)
{
	struct jffs2_inode_cache *ret;

	D2(printk(KERN_DEBUG "jffs2_get_ino_cache(): ino %u\n", ino));
	spin_lock (&c->inocache_lock);
	ret = c->inocache_list[ino % INOCACHE_HASHSIZE];
	while (ret && ret->ino < ino) {
		ret = ret->next;
	}

	spin_unlock(&c->inocache_lock);

	if (ret && ret->ino != ino)
		ret = NULL;

	D2(printk(KERN_DEBUG "jffs2_get_ino_cache found %p for ino %u\n", ret, ino));
	return ret;
}

void jffs2_add_ino_cache (struct jffs2_sb_info *c, struct jffs2_inode_cache *new)
{
	struct jffs2_inode_cache **prev;
	D2(printk(KERN_DEBUG "jffs2_add_ino_cache: Add %p (ino #%u)\n", new, new->ino));
	spin_lock(&c->inocache_lock);
	
	prev = &c->inocache_list[new->ino % INOCACHE_HASHSIZE];

	while ((*prev) && (*prev)->ino < new->ino) {
		prev = &(*prev)->next;
	}
	new->next = *prev;
	*prev = new;
	spin_unlock(&c->inocache_lock);
}

void jffs2_del_ino_cache(struct jffs2_sb_info *c, struct jffs2_inode_cache *old)
{
	struct jffs2_inode_cache **prev;
	D2(printk(KERN_DEBUG "jffs2_del_ino_cache: Del %p (ino #%u)\n", old, old->ino));
	spin_lock(&c->inocache_lock);
	
	prev = &c->inocache_list[old->ino % INOCACHE_HASHSIZE];
	
	while ((*prev) && (*prev)->ino < old->ino) {
		prev = &(*prev)->next;
	}
	if ((*prev) == old) {
		*prev = old->next;
	}
	spin_unlock(&c->inocache_lock);
}

void jffs2_free_ino_caches(struct jffs2_sb_info *c)
{
	int i;
	struct jffs2_inode_cache *this, *next;
	
	for (i=0; i<INOCACHE_HASHSIZE; i++) {
		this = c->inocache_list[i];
		while (this) {
			next = this->next;
			D2(printk(KERN_DEBUG "jffs2_free_ino_caches: Freeing ino #%u at %p\n", this->ino, this));
			jffs2_free_inode_cache(this);
			this = next;
		}
		c->inocache_list[i] = NULL;
	}
}

void jffs2_free_raw_node_refs(struct jffs2_sb_info *c)
{
	int i;
	struct jffs2_raw_node_ref *this, *next;

	for (i=0; i<c->nr_blocks; i++) {
		this = c->blocks[i].first_node;
		while(this) {
			next = this->next_phys;
			jffs2_free_raw_node_ref(this);
			this = next;
		}
		c->blocks[i].first_node = c->blocks[i].last_node = NULL;
	}
}
	
