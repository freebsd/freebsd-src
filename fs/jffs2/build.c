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
 * $Id: build.c,v 1.16.2.3 2003/04/30 09:43:32 dwmw2 Exp $
 *
 */

#include <linux/kernel.h>
#include <linux/jffs2.h>
#include <linux/slab.h>
#include "nodelist.h"

int jffs2_build_inode_pass1(struct jffs2_sb_info *, struct jffs2_inode_cache *);
int jffs2_build_remove_unlinked_inode(struct jffs2_sb_info *, struct jffs2_inode_cache *);

static inline struct jffs2_inode_cache *
first_inode_chain(int *i, struct jffs2_sb_info *c)
{
	for (; *i < INOCACHE_HASHSIZE; (*i)++) {
		if (c->inocache_list[*i])
			return c->inocache_list[*i];
	}
	return NULL;
}

static inline struct jffs2_inode_cache *
next_inode(int *i, struct jffs2_inode_cache *ic, struct jffs2_sb_info *c)
{
	/* More in this chain? */
	if (ic->next)
		return ic->next;
	(*i)++;
	return first_inode_chain(i, c);
}

#define for_each_inode(i, c, ic)			\
	for (i = 0, ic = first_inode_chain(&i, (c));	\
	     ic;					\
	     ic = next_inode(&i, ic, (c)))

/* Scan plan:
 - Scan physical nodes. Build map of inodes/dirents. Allocate inocaches as we go
 - Scan directory tree from top down, setting nlink in inocaches
 - Scan inocaches for inodes with nlink==0
*/
int jffs2_build_filesystem(struct jffs2_sb_info *c)
{
	int ret;
	int i;
	struct jffs2_inode_cache *ic;

	/* First, scan the medium and build all the inode caches with
	   lists of physical nodes */

	c->flags |= JFFS2_SB_FLAG_MOUNTING;
	ret = jffs2_scan_medium(c);
	c->flags &= ~JFFS2_SB_FLAG_MOUNTING;

	if (ret)
		return ret;

	D1(printk(KERN_DEBUG "Scanned flash completely\n"));
	/* Now build the data map for each inode, marking obsoleted nodes
	   as such, and also increase nlink of any children. */
	for_each_inode(i, c, ic) {
		D1(printk(KERN_DEBUG "Pass 1: ino #%u\n", ic->ino));
		ret = jffs2_build_inode_pass1(c, ic);
		if (ret) {
			D1(printk(KERN_WARNING "Eep. jffs2_build_inode_pass1 for ino %d returned %d\n", ic->ino, ret));
			return ret;
		}
	}
	D1(printk(KERN_DEBUG "Pass 1 complete\n"));

	/* Next, scan for inodes with nlink == 0 and remove them. If
	   they were directories, then decrement the nlink of their
	   children too, and repeat the scan. As that's going to be
	   a fairly uncommon occurrence, it's not so evil to do it this
	   way. Recursion bad. */
	do { 
		D1(printk(KERN_DEBUG "Pass 2 (re)starting\n"));
		ret = 0;
		for_each_inode(i, c, ic) {
			D1(printk(KERN_DEBUG "Pass 2: ino #%u, nlink %d, ic %p, nodes %p\n", ic->ino, ic->nlink, ic, ic->nodes));
			if (ic->nlink)
				continue;
			
			ret = jffs2_build_remove_unlinked_inode(c, ic);
			if (ret)
				break;
		/* -EAGAIN means the inode's nlink was zero, so we deleted it,
		   and furthermore that it had children and their nlink has now
		   gone to zero too. So we have to restart the scan. */
		} 
	} while(ret == -EAGAIN);
	
	D1(printk(KERN_DEBUG "Pass 2 complete\n"));
	
	/* Finally, we can scan again and free the dirent nodes and scan_info structs */
	for_each_inode(i, c, ic) {
		struct jffs2_scan_info *scan = ic->scan;
		struct jffs2_full_dirent *fd;
		D1(printk(KERN_DEBUG "Pass 3: ino #%u, ic %p, nodes %p\n", ic->ino, ic, ic->nodes));
		if (!scan) {
			if (ic->nlink) {
				D1(printk(KERN_WARNING "Why no scan struct for ino #%u which has nlink %d?\n", ic->ino, ic->nlink));
			}
			continue;
		}
		ic->scan = NULL;
		while(scan->dents) {
			fd = scan->dents;
			scan->dents = fd->next;
			jffs2_free_full_dirent(fd);
		}
		kfree(scan);
	}
	D1(printk(KERN_DEBUG "Pass 3 complete\n"));

	return ret;
}
	
int jffs2_build_inode_pass1(struct jffs2_sb_info *c, struct jffs2_inode_cache *ic)
{
	struct jffs2_tmp_dnode_info *tn;
	struct jffs2_full_dirent *fd;
	struct jffs2_node_frag *fraglist = NULL;
	struct jffs2_tmp_dnode_info *metadata = NULL;

	D1(printk(KERN_DEBUG "jffs2_build_inode building inode #%u\n", ic->ino));
	if (ic->ino > c->highest_ino)
		c->highest_ino = ic->ino;

	if (!ic->scan->tmpnodes && ic->ino != 1) {
		D1(printk(KERN_DEBUG "jffs2_build_inode: ino #%u has no data nodes!\n", ic->ino));
	}
	/* Build the list to make sure any obsolete nodes are marked as such */
	while(ic->scan->tmpnodes) {
		tn = ic->scan->tmpnodes;
		ic->scan->tmpnodes = tn->next;
		
		if (metadata && tn->version > metadata->version) {
			D1(printk(KERN_DEBUG "jffs2_build_inode_pass1 ignoring old metadata at 0x%08x\n",
				  metadata->fn->raw->flash_offset &~3));
			
			jffs2_free_full_dnode(metadata->fn);
			jffs2_free_tmp_dnode_info(metadata);
			metadata = NULL;
		}
			
		if (tn->fn->size) {
			jffs2_add_full_dnode_to_fraglist (c, &fraglist, tn->fn);
			jffs2_free_tmp_dnode_info(tn);
		} else {
			if (!metadata) {
				metadata = tn;
			} else {
				D1(printk(KERN_DEBUG "jffs2_build_inode_pass1 ignoring new metadata at 0x%08x\n",
					  tn->fn->raw->flash_offset &~3));
				
				jffs2_free_full_dnode(tn->fn);
				jffs2_free_tmp_dnode_info(tn);
			}
		}
	}
		
	/* OK. Now clear up */
	if (metadata) {
		jffs2_free_full_dnode(metadata->fn);
		jffs2_free_tmp_dnode_info(metadata);
	}
	metadata = NULL;
	
	while (fraglist) {
		struct jffs2_node_frag *frag;
		frag = fraglist;
		fraglist = fraglist->next;
		
		if (frag->node && !(--frag->node->frags)) {
			jffs2_free_full_dnode(frag->node);
		}
		jffs2_free_node_frag(frag);
	}

	/* Now for each child, increase nlink */
	for(fd=ic->scan->dents; fd; fd = fd->next) {
		struct jffs2_inode_cache *child_ic;
		if (!fd->ino)
			continue;

		child_ic = jffs2_get_ino_cache(c, fd->ino);
		if (!child_ic) {
			printk(KERN_NOTICE "Eep. Child \"%s\" (ino #%u) of dir ino #%u doesn't exist!\n",
				  fd->name, fd->ino, ic->ino);
			continue;
		}

		if (child_ic->nlink++ && fd->type == DT_DIR) {
			printk(KERN_NOTICE "Child dir \"%s\" (ino #%u) of dir ino #%u appears to be a hard link\n", fd->name, fd->ino, ic->ino);
			if (fd->ino == 1 && ic->ino == 1) {
				printk(KERN_NOTICE "This is mostly harmless, and probably caused by creating a JFFS2 image\n");
				printk(KERN_NOTICE "using a buggy version of mkfs.jffs2. Use at least v1.17.\n");
			}
			/* What do we do about it? */
		}
		D1(printk(KERN_DEBUG "Increased nlink for child \"%s\" (ino #%u)\n", fd->name, fd->ino));
		/* Can't free them. We might need them in pass 2 */
	}
	return 0;
}

int jffs2_build_remove_unlinked_inode(struct jffs2_sb_info *c, struct jffs2_inode_cache *ic)
{
	struct jffs2_raw_node_ref *raw;
	struct jffs2_full_dirent *fd;
	int ret = 0;

	if(!ic->scan) {
		D1(printk(KERN_DEBUG "ino #%u was already removed\n", ic->ino));
		return 0;
	}

	D1(printk(KERN_DEBUG "JFFS2: Removing ino #%u with nlink == zero.\n", ic->ino));
	
	for (raw = ic->nodes; raw != (void *)ic; raw = raw->next_in_ino) {
		D1(printk(KERN_DEBUG "obsoleting node at 0x%08x\n", raw->flash_offset&~3));
		jffs2_mark_node_obsolete(c, raw);
	}

	if (ic->scan->dents) {
		printk(KERN_NOTICE "Inode #%u was a directory with children - removing those too...\n", ic->ino);
	
		while(ic->scan->dents) {
			struct jffs2_inode_cache *child_ic;

			fd = ic->scan->dents;
			ic->scan->dents = fd->next;

			D1(printk(KERN_DEBUG "Removing child \"%s\", ino #%u\n",
				  fd->name, fd->ino));
			
			child_ic = jffs2_get_ino_cache(c, fd->ino);
			if (!child_ic) {
				printk(KERN_NOTICE "Cannot remove child \"%s\", ino #%u, because it doesn't exist\n", fd->name, fd->ino);
				continue;
			}
			jffs2_free_full_dirent(fd);
			child_ic->nlink--;
		}
		ret = -EAGAIN;
	}
	kfree(ic->scan);
	ic->scan = NULL;
	//	jffs2_del_ino_cache(c, ic);
	//	jffs2_free_inode_cache(ic);
	return ret;
}
