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
#if defined(CONFIG_EXT3_FS) || defined (CONFIG_EXT3_FS_MODULE)
#include <linux/jbd.h>
#include <linux/ext3_fs.h>
#include <linux/ext3_jbd.h>
#endif

#include <linux/intermezzo_fs.h>
#include <linux/intermezzo_psdev.h>

#if defined(CONFIG_EXT3_FS) || defined (CONFIG_EXT3_FS_MODULE)

#define MAX_PATH_BLOCKS(inode) (PATH_MAX >> EXT3_BLOCK_SIZE_BITS((inode)->i_sb))
#define MAX_NAME_BLOCKS(inode) (NAME_MAX >> EXT3_BLOCK_SIZE_BITS((inode)->i_sb))

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

static loff_t presto_e3_freespace(struct presto_cache *cache,
                                         struct super_block *sb)
{
        loff_t freebl = le32_to_cpu(sb->u.ext3_sb.s_es->s_free_blocks_count);
        loff_t avail =   freebl - 
                le32_to_cpu(sb->u.ext3_sb.s_es->s_r_blocks_count);
        return (avail <<  EXT3_BLOCK_SIZE_BITS(sb));
}

/* start the filesystem journal operations */
static void *presto_e3_trans_start(struct presto_file_set *fset, 
                                   struct inode *inode, 
                                   int op)
{
        int jblocks;
        int trunc_blks, one_path_blks, extra_path_blks, 
                extra_name_blks, lml_blks; 
        __u32 avail_kmlblocks;
        handle_t *handle;

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
        
        if (  (op != KML_OPCODE_UNLINK && op != KML_OPCODE_RMDIR)
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
        case KML_OPCODE_TRUNC:
                jblocks = one_path_blks + extra_name_blks + trunc_blks
                        + EXT3_DELETE_TRANS_BLOCKS; 
                break;
        case KML_OPCODE_KML_TRUNC:
                /* Hopefully this is a little better, but I'm still mostly
                 * guessing here. */
                /* unlink 1 */
                jblocks = extra_name_blks + trunc_blks +
                        EXT3_DELETE_TRANS_BLOCKS + 2; 

                /* unlink 2 */
                jblocks += extra_name_blks + trunc_blks +
                        EXT3_DELETE_TRANS_BLOCKS + 2; 

                /* rename 1 */
                jblocks += 2 * extra_path_blks + trunc_blks + 
                        2 * EXT3_DATA_TRANS_BLOCKS + 2 + 3;

                /* rename 2 */
                jblocks += 2 * extra_path_blks + trunc_blks + 
                        2 * EXT3_DATA_TRANS_BLOCKS + 2 + 3;
                break;
        case KML_OPCODE_RELEASE:
                /* 
                jblocks = one_path_blks + lml_blks + 2*trunc_blks; 
                */
                jblocks = one_path_blks; 
                break;
        case KML_OPCODE_SETATTR:
                jblocks = one_path_blks + trunc_blks + 1 ; 
                break;
        case KML_OPCODE_CREATE:
                jblocks = one_path_blks + trunc_blks 
                        + EXT3_DATA_TRANS_BLOCKS + 3 + 2; 
                break;
        case KML_OPCODE_LINK:
                jblocks = one_path_blks + trunc_blks 
                        + EXT3_DATA_TRANS_BLOCKS + 2; 
                break;
        case KML_OPCODE_UNLINK:
                jblocks = one_path_blks + extra_name_blks + trunc_blks
                        + EXT3_DELETE_TRANS_BLOCKS + 2; 
                break;
        case KML_OPCODE_SYMLINK:
                jblocks = one_path_blks + extra_path_blks + trunc_blks
                        + EXT3_DATA_TRANS_BLOCKS + 5; 
                break;
        case KML_OPCODE_MKDIR:
                jblocks = one_path_blks + trunc_blks
                        + EXT3_DATA_TRANS_BLOCKS + 4 + 2;
                break;
        case KML_OPCODE_RMDIR:
                jblocks = one_path_blks + extra_name_blks + trunc_blks
                        + EXT3_DELETE_TRANS_BLOCKS + 1; 
                break;
        case KML_OPCODE_MKNOD:
                jblocks = one_path_blks + trunc_blks + 
                        EXT3_DATA_TRANS_BLOCKS + 3 + 2;
                break;
        case KML_OPCODE_RENAME:
                jblocks = one_path_blks + extra_path_blks + trunc_blks + 
                        2 * EXT3_DATA_TRANS_BLOCKS + 2 + 3;
                break;
        case KML_OPCODE_WRITE:
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

        CDEBUG(D_JOURNAL, "creating journal handle (%d blocks) for op %d\n",
               jblocks, op);
        /* journal_start/stop does not do its own locking while updating
         * the handle/transaction information. Hence we create our own
         * critical section to protect these calls. -SHP
         */
        lock_kernel();
        handle = journal_start(EXT3_JOURNAL(inode), jblocks);
        unlock_kernel();
        return handle;
}

static void presto_e3_trans_commit(struct presto_file_set *fset, void *handle)
{
        if ( presto_no_journal(fset) || !handle)
                return;

        /* See comments before journal_start above. -SHP */
        lock_kernel();
        journal_stop(handle);
        unlock_kernel();
}

static void presto_e3_journal_file_data(struct inode *inode)
{
#ifdef EXT3_JOURNAL_DATA_FL
        inode->u.ext3_i.i_flags |= EXT3_JOURNAL_DATA_FL;
#else
#warning You must have a facility to enable journaled writes for recovery!
#endif
}

/* The logic here is a slightly modified version of ext3/inode.c:block_to_path
 */
static int presto_e3_has_all_data(struct inode *inode)
{
        int ptrs = EXT3_ADDR_PER_BLOCK(inode->i_sb);
        int ptrs_bits = EXT3_ADDR_PER_BLOCK_BITS(inode->i_sb);
        const long direct_blocks = EXT3_NDIR_BLOCKS,
                indirect_blocks = ptrs,
                double_blocks = (1 << (ptrs_bits * 2));
        long block = (inode->i_size + inode->i_sb->s_blocksize - 1) >>
                inode->i_sb->s_blocksize_bits;

        ENTRY;

        if (inode->i_size == 0) {
                EXIT;
                return 1;
        }

        if (block < direct_blocks) {
                /* No indirect blocks, no problem. */
        } else if (block < indirect_blocks + direct_blocks) {
                block++;
        } else if (block < double_blocks + indirect_blocks + direct_blocks) {
                block += 2;
        } else if (((block - double_blocks - indirect_blocks - direct_blocks)
                    >> (ptrs_bits * 2)) < ptrs) {
                block += 3;
        }

        block *= (inode->i_sb->s_blocksize / 512);

        CDEBUG(D_CACHE, "Need %ld blocks, have %ld.\n", block, inode->i_blocks);

        if (block > inode->i_blocks) {
                EXIT;
                return 0;
        }

        EXIT;
        return 1;
}

struct journal_ops presto_ext3_journal_ops = {
        .tr_all_data     = presto_e3_has_all_data,
        .tr_avail        = presto_e3_freespace,
        .tr_start        =  presto_e3_trans_start,
        .tr_commit       = presto_e3_trans_commit,
        .tr_journal_data = presto_e3_journal_file_data,
        .tr_ilookup      = presto_iget_ilookup
};

#endif /* CONFIG_EXT3_FS */
