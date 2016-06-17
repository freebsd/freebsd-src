/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (C) 1998 Peter J. Braam <braam@clusterfs.com>
 *  Copyright (C) 2000 Red Hat, Inc.
 *  Copyright (C) 2000 Los Alamos National Laboratory
 *  Copyright (C) 2000 TurboLinux, Inc.
 *  Copyright (C) 2001 Mountain View Data, Inc.
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
#include <linux/param.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/smp_lock.h>
#include <linux/locks.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#if 0
#if defined(CONFIG_REISERFS_FS) || defined(CONFIG_REISERFS_FS_MODULE)
#include <linux/reiserfs_fs.h>
#include <linux/reiserfs_fs_sb.h>
#include <linux/reiserfs_fs_i.h>
#endif

#include <linux/intermezzo_fs.h>
#include <linux/intermezzo_psdev.h>

#if defined(CONFIG_REISERFS_FS) || defined(CONFIG_REISERFS_FS_MODULE)


static loff_t presto_reiserfs_freespace(struct presto_cache *cache,
                                         struct super_block *sb)
{
        struct reiserfs_super_block * rs = SB_DISK_SUPER_BLOCK (sb);
	loff_t avail;

        avail =   le32_to_cpu(rs->s_free_blocks) * 
		le16_to_cpu(rs->s_blocksize);
        return avail; 
}

/* start the filesystem journal operations */
static void *presto_reiserfs_trans_start(struct presto_file_set *fset, 
                                   struct inode *inode, 
                                   int op)
{
	int jblocks;
        __u32 avail_kmlblocks;
	struct reiserfs_transaction_handle *th ;

	PRESTO_ALLOC(th, sizeof(*th));
	if (!th) { 
		CERROR("presto: No memory for trans handle\n");
		return NULL;
	}

        avail_kmlblocks = presto_reiserfs_freespace(fset->fset_cache, 
						    inode->i_sb);
        if ( presto_no_journal(fset) ||
             strcmp(fset->fset_cache->cache_type, "reiserfs"))
		{
			CDEBUG(D_JOURNAL, "got cache_type \"%s\"\n",
			       fset->fset_cache->cache_type);
			return NULL;
		}

        if ( avail_kmlblocks < 3 ) {
                return ERR_PTR(-ENOSPC);
        }
        
        if (  (op != PRESTO_OP_UNLINK && op != PRESTO_OP_RMDIR)
              && avail_kmlblocks < 6 ) {
                return ERR_PTR(-ENOSPC);
        }            

	jblocks = 3 + JOURNAL_PER_BALANCE_CNT * 4;
        CDEBUG(D_JOURNAL, "creating journal handle (%d blocks)\n", jblocks);

	lock_kernel();
	journal_begin(th, inode->i_sb, jblocks);
	unlock_kernel();
	return th; 
}

static void presto_reiserfs_trans_commit(struct presto_file_set *fset,
                                         void *handle)
{
	int jblocks;
	jblocks = 3 + JOURNAL_PER_BALANCE_CNT * 4;
	
	lock_kernel();
	journal_end(handle, fset->fset_cache->cache_sb, jblocks);
	unlock_kernel();
	PRESTO_FREE(handle, sizeof(struct reiserfs_transaction_handle));
}

static void presto_reiserfs_journal_file_data(struct inode *inode)
{
#ifdef EXT3_JOURNAL_DATA_FL
        inode->u.ext3_i.i_flags |= EXT3_JOURNAL_DATA_FL;
#else
#warning You must have a facility to enable journaled writes for recovery!
#endif
}

static int presto_reiserfs_has_all_data(struct inode *inode)
{
        BUG();
        return 0;
}

struct journal_ops presto_reiserfs_journal_ops = {
        .tr_all_data     = presto_reiserfs_has_all_data,
        .tr_avail        = presto_reiserfs_freespace,
        .tr_start        = presto_reiserfs_trans_start,
        .tr_commit       = presto_reiserfs_trans_commit,
        .tr_journal_data = presto_reiserfs_journal_file_data
};

#endif
#endif
