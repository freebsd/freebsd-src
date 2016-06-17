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
#ifdef CONFIG_OBDFS_FS
#include /usr/src/obd/include/linux/obdfs.h
#endif

#include <linux/intermezzo_fs.h>
#include <linux/intermezzo_psdev.h>

#ifdef CONFIG_OBDFS_FS


static unsigned long presto_obdfs_freespace(struct presto_file_set *fset,
                                         struct super_block *sb)
{
        return 0x0fffff; 
}

/* start the filesystem journal operations */
static void *presto_obdfs_trans_start(struct presto_file_set *fset, 
                                   struct inode *inode, 
                                   int op)
{

        return (void *) 1;
}

#if 0
        int jblocks;
        int trunc_blks, one_path_blks, extra_path_blks, 
                extra_name_blks, lml_blks; 
        __u32 avail_kmlblocks;

        if ( presto_no_journal(fset) ||
             strcmp(fset->fset_cache->cache_type, "ext3"))
          {
            CDEBUG(D_JOURNAL, "got cache_type \"%s\"\n",
                   fset->fset_cache->cache_type);
            return NULL;
          }

        avail_kmlblocks = inode->i_sb->u.ext3_sb.s_es->s_free_blocks_count;
        
        if ( avail_kmlblocks < 3 ) {
                return ERR_PTR(-ENOSPC);
        }
        
        if (  (op != PRESTO_OP_UNLINK && op != PRESTO_OP_RMDIR)
              && avail_kmlblocks < 6 ) {
                return ERR_PTR(-ENOSPC);
        }            

        /* Need journal space for:
             at least three writes to KML (two one block writes, one a path) 
             possibly a second name (unlink, rmdir)
             possibly a second path (symlink, rename)
             a one block write to the last rcvd file 
        */

        trunc_blks = EXT3_DATA_TRANS_BLOCKS + 1; 
        one_path_blks = 4*EXT3_DATA_TRANS_BLOCKS + MAX_PATH_BLOCKS(inode) + 3;
        lml_blks = 4*EXT3_DATA_TRANS_BLOCKS + MAX_PATH_BLOCKS(inode) + 2;
        extra_path_blks = EXT3_DATA_TRANS_BLOCKS + MAX_PATH_BLOCKS(inode); 
        extra_name_blks = EXT3_DATA_TRANS_BLOCKS + MAX_NAME_BLOCKS(inode); 

        /* additional blocks appear for "two pathname" operations
           and operations involving the LML records 
        */
        switch (op) {
        case PRESTO_OP_TRUNC:
                jblocks = one_path_blks + extra_name_blks + trunc_blks
                        + EXT3_DELETE_TRANS_BLOCKS; 
                break;
        case PRESTO_OP_RELEASE:
                /* 
                jblocks = one_path_blks + lml_blks + 2*trunc_blks; 
                */
                jblocks = one_path_blks; 
                break;
        case PRESTO_OP_SETATTR:
                jblocks = one_path_blks + trunc_blks + 1 ; 
                break;
        case PRESTO_OP_CREATE:
                jblocks = one_path_blks + trunc_blks 
                        + EXT3_DATA_TRANS_BLOCKS + 3; 
                break;
        case PRESTO_OP_LINK:
                jblocks = one_path_blks + trunc_blks 
                        + EXT3_DATA_TRANS_BLOCKS; 
                break;
        case PRESTO_OP_UNLINK:
                jblocks = one_path_blks + extra_name_blks + trunc_blks
                        + EXT3_DELETE_TRANS_BLOCKS; 
                break;
        case PRESTO_OP_SYMLINK:
                jblocks = one_path_blks + extra_path_blks + trunc_blks
                        + EXT3_DATA_TRANS_BLOCKS + 5; 
                break;
        case PRESTO_OP_MKDIR:
                jblocks = one_path_blks + trunc_blks
                        + EXT3_DATA_TRANS_BLOCKS + 4;
                break;
        case PRESTO_OP_RMDIR:
                jblocks = one_path_blks + extra_name_blks + trunc_blks
                        + EXT3_DELETE_TRANS_BLOCKS; 
                break;
        case PRESTO_OP_MKNOD:
                jblocks = one_path_blks + trunc_blks + 
                        EXT3_DATA_TRANS_BLOCKS + 3;
                break;
        case PRESTO_OP_RENAME:
                jblocks = one_path_blks + extra_path_blks + trunc_blks + 
                        2 * EXT3_DATA_TRANS_BLOCKS + 2;
                break;
        case PRESTO_OP_WRITE:
                jblocks = one_path_blks; 
                /*  add this when we can wrap our transaction with 
                    that of ext3_file_write (ordered writes)
                    +  EXT3_DATA_TRANS_BLOCKS;
                */
                break;
        default:
                CDEBUG(D_JOURNAL, "invalid operation %d for journal\n", op);
                return NULL;
        }

        CDEBUG(D_JOURNAL, "creating journal handle (%d blocks)\n", jblocks);
        return journal_start(EXT3_JOURNAL(inode), jblocks);
}
#endif

void presto_obdfs_trans_commit(struct presto_file_set *fset, void *handle)
{
#if 0
        if ( presto_no_journal(fset) || !handle)
                return;

        journal_stop(handle);
#endif
}

void presto_obdfs_journal_file_data(struct inode *inode)
{
#ifdef EXT3_JOURNAL_DATA_FL
        inode->u.ext3_i.i_flags |= EXT3_JOURNAL_DATA_FL;
#else
#warning You must have a facility to enable journaled writes for recovery!
#endif
}

struct journal_ops presto_obdfs_journal_ops = {
        .tr_avail        = presto_obdfs_freespace,
        .tr_start        =  presto_obdfs_trans_start,
        .tr_commit       = presto_obdfs_trans_commit,
        .tr_journal_data = presto_obdfs_journal_file_data
};

#endif
