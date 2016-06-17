/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (C) 1998 Peter J. Braam <braam@clusterfs.com>
 *  Copyright (C) 2000 Red Hat, Inc.
 *  Copyright (C) 2000 Los Alamos National Laboratory
 *  Copyright (C) 2000 TurboLinux, Inc.
 *  Copyright (C) 2001 Mountain View Data, Inc.
 *  Copyright (C) 2001 Tacit Networks, Inc. <phil@off.net>
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
#include <linux/smp_lock.h>
#if defined(CONFIG_TMPFS)
#include <linux/jbd.h>
#if defined(CONFIG_EXT3)
#include <linux/ext3_fs.h>
#include <linux/ext3_jbd.h>
#endif
#endif

#include <linux/intermezzo_fs.h>
#include <linux/intermezzo_psdev.h>

#if defined(CONFIG_TMPFS)

/* space requirements: 
   presto_do_truncate: 
        used to truncate the KML forward to next fset->chunksize boundary
          - zero partial block
          - update inode
   presto_write_record: 
        write header (< one block) 
        write one path (< MAX_PATHLEN) 
        possibly write another path (< MAX_PATHLEN)
        write suffix (< one block) 
   presto_update_last_rcvd
        write one block
*/

static loff_t presto_tmpfs_freespace(struct presto_cache *cache,
                                         struct super_block *sb)
{
        return (1<<30);
}

/* start the filesystem journal operations */
static void *presto_tmpfs_trans_start(struct presto_file_set *fset, 
                                   struct inode *inode, 
                                   int op)
{
        return (void *)1; 
}

static void presto_tmpfs_trans_commit(struct presto_file_set *fset, void *handle)
{
        return;
}

static void presto_tmpfs_journal_file_data(struct inode *inode)
{
        return; 
}

/* The logic here is a slightly modified version of ext3/inode.c:block_to_path
 */
static int presto_tmpfs_has_all_data(struct inode *inode)
{
        return 0;
}

struct journal_ops presto_tmpfs_journal_ops = {
        tr_all_data: presto_tmpfs_has_all_data,
        tr_avail: presto_tmpfs_freespace,
        tr_start:  presto_tmpfs_trans_start,
        tr_commit: presto_tmpfs_trans_commit,
        tr_journal_data: presto_tmpfs_journal_file_data,
        tr_ilookup: presto_tmpfs_ilookup,
        tr_add_ilookup: presto_add_ilookup_dentry
};

#endif /* CONFIG_EXT3_FS */
