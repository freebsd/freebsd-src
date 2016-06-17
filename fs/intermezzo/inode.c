/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (C) 1996 Peter J. Braam <braam@maths.ox.ac.uk> and
 *    Michael Callahan <callahan@maths.ox.ac.uk>
 *  Copyright (C) 1999 Carnegie Mellon University
 *    Rewritten for Linux 2.1.  Peter Braam <braam@cs.cmu.edu>
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
 *
 * Super block/filesystem wide operations
 */

#define __NO_VERSION__
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/locks.h>
#include <linux/unistd.h>

#include <asm/system.h>
#include <asm/uaccess.h>

#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/locks.h>
#include <linux/string.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <asm/segment.h>

#include <linux/intermezzo_fs.h>
#include <linux/intermezzo_psdev.h>

extern void presto_free_cache(struct presto_cache *);

void presto_set_ops(struct inode *inode, struct  filter_fs *filter)
{
        ENTRY; 

        if (!inode || is_bad_inode(inode))
                return;

        if (S_ISREG(inode->i_mode)) {
                if ( !filter_c2cfiops(filter) ) {
                       filter_setup_file_ops(filter, 
                                             inode, &presto_file_iops,
                                             &presto_file_fops);
                }
                inode->i_op = filter_c2ufiops(filter);
                inode->i_fop = filter_c2uffops(filter);
                CDEBUG(D_INODE, "set file methods for %ld to %p\n",
                       inode->i_ino, inode->i_op);
        } else if (S_ISDIR(inode->i_mode)) {
                inode->i_op = filter_c2udiops(filter);
                inode->i_fop = filter_c2udfops(filter);
                CDEBUG(D_INODE, "set dir methods for %ld to %p ioctl %p\n",
                       inode->i_ino, inode->i_op, inode->i_fop->ioctl);
        } else if (S_ISLNK(inode->i_mode)) {
                if ( !filter_c2csiops(filter)) {
                        filter_setup_symlink_ops(filter, 
                                                 inode,
                                                 &presto_sym_iops, 
                                                 &presto_sym_fops);
                }
                inode->i_op = filter_c2usiops(filter);
                inode->i_fop = filter_c2usfops(filter);
                CDEBUG(D_INODE, "set link methods for %ld to %p\n",
                       inode->i_ino, inode->i_op);
        }
        EXIT;
}

void presto_read_inode(struct inode *inode)
{
        struct presto_cache *cache;

        cache = presto_get_cache(inode);
        if ( !cache ) {
                CERROR("PRESTO: BAD, BAD: cannot find cache\n");
                make_bad_inode(inode);
                return ;
        }

        filter_c2csops(cache->cache_filter)->read_inode(inode);

        CDEBUG(D_INODE, "presto_read_inode: ino %ld, gid %d\n", 
               inode->i_ino, inode->i_gid);

        presto_set_ops(inode, cache->cache_filter); 
        /* XXX handle special inodes here or not - probably not? */
}

static void presto_put_super(struct super_block *sb)
{
        struct presto_cache *cache;
        struct upc_channel *channel;
        struct super_operations *sops;
        struct list_head *lh;
        int err;

        ENTRY;
        cache = presto_cache_find(sb->s_dev);
        if (!cache) {
                EXIT;
                goto exit;
        }
        channel = &izo_channels[presto_c2m(cache)];
        sops = filter_c2csops(cache->cache_filter);
        err = izo_clear_all_fsetroots(cache); 
        if (err) { 
                CERROR("%s: err %d\n", __FUNCTION__, err);
        }
        PRESTO_FREE(cache->cache_vfsmount, sizeof(struct vfsmount));

        /* look at kill_super - fsync_super is not exported GRRR but 
           probably not needed */ 
        unlock_super(sb);
        shrink_dcache_parent(cache->cache_root); 
        dput(cache->cache_root); 
        //fsync_super(sb); 
        lock_super(sb);

        if (sops->write_super)
                sops->write_super(sb); 

        if (sops->put_super)
                sops->put_super(sb);

        /* free any remaining async upcalls when the filesystem is unmounted */
        spin_lock(&channel->uc_lock);
        lh = channel->uc_pending.next;
        while ( lh != &channel->uc_pending) {
                struct upc_req *req;
                req = list_entry(lh, struct upc_req, rq_chain);

                /* assignment must be here: we are about to free &lh */
                lh = lh->next;
                if ( ! (req->rq_flags & REQ_ASYNC) ) 
                        continue;
                list_del(&(req->rq_chain));
                PRESTO_FREE(req->rq_data, req->rq_bufsize);
                PRESTO_FREE(req, sizeof(struct upc_req));
        }
        list_del(&cache->cache_channel_list); 
        spin_unlock(&channel->uc_lock);

        presto_free_cache(cache);

exit:
        CDEBUG(D_MALLOC, "after umount: kmem %ld, vmem %ld\n",
               presto_kmemory, presto_vmemory);
        MOD_DEC_USE_COUNT;
        return ;
}

struct super_operations presto_super_ops = {
        .read_inode    = presto_read_inode,
        .put_super     = presto_put_super,
};


/* symlinks can be chowned */
struct inode_operations presto_sym_iops = {
        .setattr       = presto_setattr
};

/* NULL for now */
struct file_operations presto_sym_fops; 
