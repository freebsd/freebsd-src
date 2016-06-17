/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (C) 1998 Peter J. Braam <braam@clusterfs.com>
 *
 *   This file is part of InterMezzo, http://www.inter-mezzo.org.
 *
 *   InterMezzo is free software; you can redistribute it and/or
 *   modify it under the terms of version 2 of the GNU General Public
 *   License as published by the Free Software Foundation.
 *
 *   InterMezzo is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with InterMezzo; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/locks.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/ext2_fs.h> 

#include <linux/intermezzo_fs.h>
#include <linux/intermezzo_psdev.h>

#if defined(CONFIG_EXT2_FS)

/* EXT2 has no journalling, so these functions do nothing */
static loff_t presto_e2_freespace(struct presto_cache *cache,
                                         struct super_block *sb)
{
        unsigned long freebl = le32_to_cpu(sb->u.ext2_sb.s_es->s_free_blocks_count);
        unsigned long avail =   freebl - le32_to_cpu(sb->u.ext2_sb.s_es->s_r_blocks_count);
	return (avail <<  EXT2_BLOCK_SIZE_BITS(sb));
}

/* start the filesystem journal operations */
static void *presto_e2_trans_start(struct presto_file_set *fset, struct inode *inode, int op)
{
        __u32 avail_kmlblocks;

        if ( presto_no_journal(fset) ||
             strcmp(fset->fset_cache->cache_type, "ext2"))
                return NULL;

        avail_kmlblocks = inode->i_sb->u.ext2_sb.s_es->s_free_blocks_count;
        
        if ( avail_kmlblocks < 3 ) {
                return ERR_PTR(-ENOSPC);
        }
        
        if (  (op != KML_OPCODE_UNLINK && op != KML_OPCODE_RMDIR)
              && avail_kmlblocks < 6 ) {
                return ERR_PTR(-ENOSPC);
        }            
	return (void *) 1;
}

static void presto_e2_trans_commit(struct presto_file_set *fset, void *handle)
{
        do {} while (0);
}

static int presto_e2_has_all_data(struct inode *inode)
{
        BUG();
        return 0;
}

struct journal_ops presto_ext2_journal_ops = {
        tr_all_data: presto_e2_has_all_data,
        tr_avail: presto_e2_freespace,
        tr_start: presto_e2_trans_start,
        tr_commit: presto_e2_trans_commit,
        tr_journal_data: NULL
};

#endif /* CONFIG_EXT2_FS */
