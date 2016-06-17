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
 * $Id: erase.c,v 1.24.2.1 2003/11/02 13:51:17 dwmw2 Exp $
 *
 */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include <linux/jffs2.h>
#include <linux/interrupt.h>
#include "nodelist.h"
#include <linux/crc32.h>

struct erase_priv_struct {
	struct jffs2_eraseblock *jeb;
	struct jffs2_sb_info *c;
};
      
static void jffs2_erase_callback(struct erase_info *);
static void jffs2_free_all_node_refs(struct jffs2_sb_info *c, struct jffs2_eraseblock *jeb);

void jffs2_erase_block(struct jffs2_sb_info *c, struct jffs2_eraseblock *jeb)
{
	struct erase_info *instr;
	int ret;

	instr = kmalloc(sizeof(struct erase_info) + sizeof(struct erase_priv_struct), GFP_KERNEL);
	if (!instr) {
		printk(KERN_WARNING "kmalloc for struct erase_info in jffs2_erase_block failed. Refiling block for later\n");
		spin_lock_bh(&c->erase_completion_lock);
		list_del(&jeb->list);
		list_add(&jeb->list, &c->erase_pending_list);
		c->erasing_size -= c->sector_size;
		spin_unlock_bh(&c->erase_completion_lock);
		return;
	}

	memset(instr, 0, sizeof(*instr));

	instr->mtd = c->mtd;
	instr->addr = jeb->offset;
	instr->len = c->sector_size;
	instr->callback = jffs2_erase_callback;
	instr->priv = (unsigned long)(&instr[1]);
	
	((struct erase_priv_struct *)instr->priv)->jeb = jeb;
	((struct erase_priv_struct *)instr->priv)->c = c;

	ret = c->mtd->erase(c->mtd, instr);
	if (!ret) {
		return;
	}
	if (ret == -ENOMEM || ret == -EAGAIN) {
		/* Erase failed immediately. Refile it on the list */
		D1(printk(KERN_DEBUG "Erase at 0x%08x failed: %d. Refiling on erase_pending_list\n", jeb->offset, ret));
		spin_lock_bh(&c->erase_completion_lock);
		list_del(&jeb->list);
		list_add(&jeb->list, &c->erase_pending_list);
		c->erasing_size -= c->sector_size;
		spin_unlock_bh(&c->erase_completion_lock);
		kfree(instr);
		return;
	}

	if (ret == -EROFS) 
		printk(KERN_WARNING "Erase at 0x%08x failed immediately: -EROFS. Is the sector locked?\n", jeb->offset);
	else
		printk(KERN_WARNING "Erase at 0x%08x failed immediately: errno %d\n", jeb->offset, ret);
	spin_lock_bh(&c->erase_completion_lock);
	list_del(&jeb->list);
	list_add(&jeb->list, &c->bad_list);
	c->nr_erasing_blocks--;
	c->bad_size += c->sector_size;
	c->erasing_size -= c->sector_size;
	spin_unlock_bh(&c->erase_completion_lock);
	wake_up(&c->erase_wait);
	kfree(instr);
}

void jffs2_erase_pending_blocks(struct jffs2_sb_info *c)
{
	struct jffs2_eraseblock *jeb;

	spin_lock_bh(&c->erase_completion_lock);
	while (!list_empty(&c->erase_pending_list)) {

		jeb = list_entry(c->erase_pending_list.next, struct jffs2_eraseblock, list);

		D1(printk(KERN_DEBUG "Starting erase of pending block 0x%08x\n", jeb->offset));

		list_del(&jeb->list);
		c->erasing_size += c->sector_size;
		c->free_size -= jeb->free_size;
		c->used_size -= jeb->used_size;
		c->dirty_size -= jeb->dirty_size;
		jeb->used_size = jeb->dirty_size = jeb->free_size = 0;
		jffs2_free_all_node_refs(c, jeb);
		list_add(&jeb->list, &c->erasing_list);
		spin_unlock_bh(&c->erase_completion_lock);
		
		jffs2_erase_block(c, jeb);
		/* Be nice */
		if (current->need_resched)
			schedule();
		spin_lock_bh(&c->erase_completion_lock);
	}
	spin_unlock_bh(&c->erase_completion_lock);
	D1(printk(KERN_DEBUG "jffs2_erase_pending_blocks completed\n"));
}


static void jffs2_erase_callback(struct erase_info *instr)
{
	struct erase_priv_struct *priv = (void *)instr->priv;

	if(instr->state != MTD_ERASE_DONE) {
		printk(KERN_WARNING "Erase at 0x%08x finished, but state != MTD_ERASE_DONE. State is 0x%x instead.\n", instr->addr, instr->state);
		spin_lock(&priv->c->erase_completion_lock);
		priv->c->erasing_size -= priv->c->sector_size;
		priv->c->bad_size += priv->c->sector_size;
		list_del(&priv->jeb->list);
		list_add(&priv->jeb->list, &priv->c->bad_list);
		priv->c->nr_erasing_blocks--;
		spin_unlock(&priv->c->erase_completion_lock);
		wake_up(&priv->c->erase_wait);
	} else {
		D1(printk(KERN_DEBUG "Erase completed successfully at 0x%08x\n", instr->addr));
		spin_lock(&priv->c->erase_completion_lock);
		list_del(&priv->jeb->list);
		list_add_tail(&priv->jeb->list, &priv->c->erase_complete_list);
		spin_unlock(&priv->c->erase_completion_lock);
	}	
	/* Make sure someone picks up the block off the erase_complete list */
	OFNI_BS_2SFFJ(priv->c)->s_dirt = 1;
	kfree(instr);
}

/* Hmmm. Maybe we should accept the extra space it takes and make
   this a standard doubly-linked list? */
static inline void jffs2_remove_node_refs_from_ino_list(struct jffs2_sb_info *c,
			struct jffs2_raw_node_ref *ref, struct jffs2_eraseblock *jeb)
{
	struct jffs2_inode_cache *ic = NULL;
	struct jffs2_raw_node_ref **prev;

	prev = &ref->next_in_ino;

	/* Walk the inode's list once, removing any nodes from this eraseblock */
	while (1) {
		if (!(*prev)->next_in_ino) {
			/* We're looking at the jffs2_inode_cache, which is 
			   at the end of the linked list. Stash it and continue
			   from the beginning of the list */
			ic = (struct jffs2_inode_cache *)(*prev);
			prev = &ic->nodes;
			continue;
		} 

		if (((*prev)->flash_offset & ~(c->sector_size -1)) == jeb->offset) {
			/* It's in the block we're erasing */
			struct jffs2_raw_node_ref *this;

			this = *prev;
			*prev = this->next_in_ino;
			this->next_in_ino = NULL;

			if (this == ref)
				break;

			continue;
		}
		/* Not to be deleted. Skip */
		prev = &((*prev)->next_in_ino);
	}

	/* PARANOIA */
	if (!ic) {
		printk(KERN_WARNING "inode_cache not found in remove_node_refs()!!\n");
		return;
	}

	D1(printk(KERN_DEBUG "Removed nodes in range 0x%08x-0x%08x from ino #%u\n",
		  jeb->offset, jeb->offset + c->sector_size, ic->ino));

	D2({
		int i=0;
		struct jffs2_raw_node_ref *this;
		printk(KERN_DEBUG "After remove_node_refs_from_ino_list: \n" KERN_DEBUG);

		this = ic->nodes;
	   
		while(this) {
			printk( "0x%08x(%d)->", this->flash_offset & ~3, this->flash_offset &3);
			if (++i == 5) {
				printk("\n" KERN_DEBUG);
				i=0;
			}
			this = this->next_in_ino;
		}
		printk("\n");
	});

	if (ic->nodes == (void *)ic) {
		D1(printk(KERN_DEBUG "inocache for ino #%u is all gone now. Freeing\n", ic->ino));
		jffs2_del_ino_cache(c, ic);
		jffs2_free_inode_cache(ic);
	}
}

static void jffs2_free_all_node_refs(struct jffs2_sb_info *c, struct jffs2_eraseblock *jeb)
{
	struct jffs2_raw_node_ref *ref;
	D1(printk(KERN_DEBUG "Freeing all node refs for eraseblock offset 0x%08x\n", jeb->offset));
	while(jeb->first_node) {
		ref = jeb->first_node;
		jeb->first_node = ref->next_phys;
		
		/* Remove from the inode-list */
		if (ref->next_in_ino)
			jffs2_remove_node_refs_from_ino_list(c, ref, jeb);
		/* else it was a non-inode node or already removed, so don't bother */

		jffs2_free_raw_node_ref(ref);
	}
	jeb->last_node = NULL;
}

void jffs2_erase_pending_trigger(struct jffs2_sb_info *c)
{
	OFNI_BS_2SFFJ(c)->s_dirt = 1;
}

void jffs2_mark_erased_blocks(struct jffs2_sb_info *c)
{
	static struct jffs2_unknown_node marker = {JFFS2_MAGIC_BITMASK, JFFS2_NODETYPE_CLEANMARKER, sizeof(struct jffs2_unknown_node)};
	struct jffs2_eraseblock *jeb;
	struct jffs2_raw_node_ref *marker_ref;
	unsigned char *ebuf;
	ssize_t retlen;
	int ret;

	marker.hdr_crc = crc32(0, &marker, sizeof(struct jffs2_unknown_node)-4);

	spin_lock_bh(&c->erase_completion_lock);
	while (!list_empty(&c->erase_complete_list)) {
		jeb = list_entry(c->erase_complete_list.next, struct jffs2_eraseblock, list);
		list_del(&jeb->list);
		spin_unlock_bh(&c->erase_completion_lock);

		marker_ref = jffs2_alloc_raw_node_ref();
		if (!marker_ref) {
			printk(KERN_WARNING "Failed to allocate raw node ref for clean marker\n");
			/* Come back later */
			jffs2_erase_pending_trigger(c);
			return;
		}

		ebuf = kmalloc(PAGE_SIZE, GFP_KERNEL);
		if (!ebuf) {
			printk(KERN_WARNING "Failed to allocate page buffer for verifying erase at 0x%08x. Assuming it worked\n", jeb->offset);
		} else {
			__u32 ofs = jeb->offset;

			D1(printk(KERN_DEBUG "Verifying erase at 0x%08x\n", jeb->offset));
			while(ofs < jeb->offset + c->sector_size) {
				__u32 readlen = min((__u32)PAGE_SIZE, jeb->offset + c->sector_size - ofs);
				int i;

				ret = c->mtd->read(c->mtd, ofs, readlen, &retlen, ebuf);
				if (ret < 0) {
					printk(KERN_WARNING "Read of newly-erased block at 0x%08x failed: %d. Putting on bad_list\n", ofs, ret);
					goto bad;
				}
				if (retlen != readlen) {
					printk(KERN_WARNING "Short read from newly-erased block at 0x%08x. Wanted %d, got %d\n", ofs, readlen, retlen);
					goto bad;
				}
				for (i=0; i<readlen; i += sizeof(unsigned long)) {
					/* It's OK. We know it's properly aligned */
					unsigned long datum = *(unsigned long *)(&ebuf[i]);
					if (datum + 1) {
						printk(KERN_WARNING "Newly-erased block contained word 0x%lx at offset 0x%08x\n", datum, ofs + i);
					bad: 
						jffs2_free_raw_node_ref(marker_ref);
						kfree(ebuf);
					bad2:
						spin_lock_bh(&c->erase_completion_lock);
						c->erasing_size -= c->sector_size;
						c->bad_size += c->sector_size;

						list_add_tail(&jeb->list, &c->bad_list);
						c->nr_erasing_blocks--;
						spin_unlock_bh(&c->erase_completion_lock);
						wake_up(&c->erase_wait);
						return;
					}
				}
				ofs += readlen;
			}
			kfree(ebuf);
		}
					
		/* Write the erase complete marker */	
		D1(printk(KERN_DEBUG "Writing erased marker to block at 0x%08x\n", jeb->offset));
		ret = c->mtd->write(c->mtd, jeb->offset, sizeof(marker), &retlen, (char *)&marker);
		if (ret) {
			printk(KERN_WARNING "Write clean marker to block at 0x%08x failed: %d\n",
			       jeb->offset, ret);
			goto bad2;
		}
		if (retlen != sizeof(marker)) {
			printk(KERN_WARNING "Short write to newly-erased block at 0x%08x: Wanted %d, got %d\n",
			       jeb->offset, sizeof(marker), retlen);
			goto bad2;
		}

		marker_ref->next_in_ino = NULL;
		marker_ref->next_phys = NULL;
		marker_ref->flash_offset = jeb->offset;
		marker_ref->totlen = PAD(sizeof(marker));

		jeb->first_node = jeb->last_node = marker_ref;

		jeb->free_size = c->sector_size - marker_ref->totlen;
		jeb->used_size = marker_ref->totlen;
		jeb->dirty_size = 0;

		spin_lock_bh(&c->erase_completion_lock);
		c->erasing_size -= c->sector_size;
		c->free_size += jeb->free_size;
		c->used_size += jeb->used_size;

		ACCT_SANITY_CHECK(c,jeb);
		ACCT_PARANOIA_CHECK(jeb);

		list_add_tail(&jeb->list, &c->free_list);
		c->nr_erasing_blocks--;
		c->nr_free_blocks++;
		wake_up(&c->erase_wait);
	}
	spin_unlock_bh(&c->erase_completion_lock);
}
