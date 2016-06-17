/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (C) 2000 Stelias Computing, Inc.
 *  Copyright (C) 2000 Red Hat, Inc.
 *  Copyright (C) 2000 Mountain View Data, Inc.
 *
 *  Extended Attribute Support
 *  Copyright (C) 2001 Shirish H. Phatak, Tacit Networks, Inc.
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
 */

#include <stdarg.h>

#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <asm/system.h>

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/ext2_fs.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/locks.h>
#include <linux/blkdev.h>
#include <linux/init.h>
#define __NO_VERSION__
#include <linux/module.h>

#include <linux/fsfilter.h>
#include <linux/intermezzo_fs.h>


int filter_print_entry = 0;
int filter_debug = 0xfffffff;
/*
 * The function in this file are responsible for setting up the 
 * correct methods layered file systems like InterMezzo and snapfs
 */


static struct filter_fs filter_oppar[FILTER_FS_TYPES];

/* get to the upper methods (intermezzo, snapfs) */
inline struct super_operations *filter_c2usops(struct filter_fs *cache)
{
        return &cache->o_fops.filter_sops;
}

inline struct inode_operations *filter_c2udiops(struct filter_fs *cache)
{
        return &cache->o_fops.filter_dir_iops;
}


inline struct inode_operations *filter_c2ufiops(struct filter_fs *cache)
{
        return &cache->o_fops.filter_file_iops;
}

inline struct inode_operations *filter_c2usiops(struct filter_fs *cache)
{
        return &cache->o_fops.filter_sym_iops;
}


inline struct file_operations *filter_c2udfops(struct filter_fs *cache)
{
        return &cache->o_fops.filter_dir_fops;
}

inline struct file_operations *filter_c2uffops(struct filter_fs *cache)
{
        return &cache->o_fops.filter_file_fops;
}

inline struct file_operations *filter_c2usfops(struct filter_fs *cache)
{
        return &cache->o_fops.filter_sym_fops;
}

inline struct dentry_operations *filter_c2udops(struct filter_fs *cache)
{
        return &cache->o_fops.filter_dentry_ops;
}

/* get to the cache (lower) methods */
inline struct super_operations *filter_c2csops(struct filter_fs *cache)
{
        return cache->o_caops.cache_sops;
}

inline struct inode_operations *filter_c2cdiops(struct filter_fs *cache)
{
        return cache->o_caops.cache_dir_iops;
}

inline struct inode_operations *filter_c2cfiops(struct filter_fs *cache)
{
        return cache->o_caops.cache_file_iops;
}

inline struct inode_operations *filter_c2csiops(struct filter_fs *cache)
{
        return cache->o_caops.cache_sym_iops;
}

inline struct file_operations *filter_c2cdfops(struct filter_fs *cache)
{
        return cache->o_caops.cache_dir_fops;
}

inline struct file_operations *filter_c2cffops(struct filter_fs *cache)
{
        return cache->o_caops.cache_file_fops;
}

inline struct file_operations *filter_c2csfops(struct filter_fs *cache)
{
        return cache->o_caops.cache_sym_fops;
}

inline struct dentry_operations *filter_c2cdops(struct filter_fs *cache)
{
        return cache->o_caops.cache_dentry_ops;
}


void filter_setup_journal_ops(struct filter_fs *ops, char *cache_type)
{
        if ( strlen(cache_type) == strlen("ext2") &&
             memcmp(cache_type, "ext2", strlen("ext2")) == 0 ) {
#if CONFIG_EXT2_FS
                ops->o_trops = &presto_ext2_journal_ops;
#else
                ops->o_trops = NULL;
#endif
                FDEBUG(D_SUPER, "ops at %p\n", ops);
        }

        if ( strlen(cache_type) == strlen("ext3") &&
             memcmp(cache_type, "ext3", strlen("ext3")) == 0 ) {
#if defined(CONFIG_EXT3_FS) || defined (CONFIG_EXT3_FS_MODULE)
                ops->o_trops = &presto_ext3_journal_ops;
#else
                ops->o_trops = NULL;
#endif
                FDEBUG(D_SUPER, "ops at %p\n", ops);
        }

        if ( strlen(cache_type) == strlen("tmpfs") &&
             memcmp(cache_type, "tmpfs", strlen("tmpfs")) == 0 ) {
#if defined(CONFIG_TMPFS)
                ops->o_trops = &presto_tmpfs_journal_ops;
#else
                ops->o_trops = NULL;
#endif
                FDEBUG(D_SUPER, "ops at %p\n", ops);
        }

        if ( strlen(cache_type) == strlen("reiserfs") &&
             memcmp(cache_type, "reiserfs", strlen("reiserfs")) == 0 ) {
#if 0
		/* #if defined(CONFIG_REISERFS_FS) || defined(CONFIG_REISERFS_FS_MODULE) */
                ops->o_trops = &presto_reiserfs_journal_ops;
#else
                ops->o_trops = NULL;
#endif
                FDEBUG(D_SUPER, "ops at %p\n", ops);
        }

        if ( strlen(cache_type) == strlen("xfs") &&
             memcmp(cache_type, "xfs", strlen("xfs")) == 0 ) {
#if 0
/*#if defined(CONFIG_XFS_FS) || defined (CONFIG_XFS_FS_MODULE) */
                ops->o_trops = &presto_xfs_journal_ops;
#else
                ops->o_trops = NULL;
#endif
                FDEBUG(D_SUPER, "ops at %p\n", ops);
        }

        if ( strlen(cache_type) == strlen("obdfs") &&
             memcmp(cache_type, "obdfs", strlen("obdfs")) == 0 ) {
#if defined(CONFIG_OBDFS_FS) || defined (CONFIG_OBDFS_FS_MODULE)
                ops->o_trops = presto_obdfs_journal_ops;
#else
                ops->o_trops = NULL;
#endif
                FDEBUG(D_SUPER, "ops at %p\n", ops);
        }
}


/* find the cache for this FS */
struct filter_fs *filter_get_filter_fs(const char *cache_type)
{
        struct filter_fs *ops = NULL;
        FENTRY;

        if ( strlen(cache_type) == strlen("ext2") &&
             memcmp(cache_type, "ext2", strlen("ext2")) == 0 ) {
                ops = &filter_oppar[FILTER_FS_EXT2];
                FDEBUG(D_SUPER, "ops at %p\n", ops);
        }

        if ( strlen(cache_type) == strlen("xfs") &&
             memcmp(cache_type, "xfs", strlen("xfs")) == 0 ) {
                ops = &filter_oppar[FILTER_FS_XFS];
                FDEBUG(D_SUPER, "ops at %p\n", ops);
        }

        if ( strlen(cache_type) == strlen("ext3") &&
             memcmp(cache_type, "ext3", strlen("ext3")) == 0 ) {
                ops = &filter_oppar[FILTER_FS_EXT3];
                FDEBUG(D_SUPER, "ops at %p\n", ops);
        }

        if ( strlen(cache_type) == strlen("tmpfs") &&
             memcmp(cache_type, "tmpfs", strlen("tmpfs")) == 0 ) {
                ops = &filter_oppar[FILTER_FS_TMPFS];
                FDEBUG(D_SUPER, "ops at %p\n", ops);
        }

        if ( strlen(cache_type) == strlen("reiserfs") &&
             memcmp(cache_type, "reiserfs", strlen("reiserfs")) == 0 ) {
                ops = &filter_oppar[FILTER_FS_REISERFS];
                FDEBUG(D_SUPER, "ops at %p\n", ops);
        }
        if ( strlen(cache_type) == strlen("obdfs") &&
             memcmp(cache_type, "obdfs", strlen("obdfs")) == 0 ) {
                ops = &filter_oppar[FILTER_FS_OBDFS];
                FDEBUG(D_SUPER, "ops at %p\n", ops);
        }

        if (ops == NULL) {
                CERROR("prepare to die: unrecognized cache type for Filter\n");
        }
        return ops;
        FEXIT;
}


/*
 *  Frobnicate the InterMezzo operations
 *    this establishes the link between the InterMezzo file system
 *    and the underlying file system used for the cache.
 */

void filter_setup_super_ops(struct filter_fs *cache, struct super_operations *cache_sops, struct super_operations *filter_sops)
{
        /* Get ptr to the shared struct snapfs_ops structure. */
        struct filter_ops *props = &cache->o_fops;
        /* Get ptr to the shared struct cache_ops structure. */
        struct cache_ops *caops = &cache->o_caops;

        FENTRY;

        if ( cache->o_flags & FILTER_DID_SUPER_OPS ) {
                FEXIT;
                return;
        }
        cache->o_flags |= FILTER_DID_SUPER_OPS;

        /* Set the cache superblock operations to point to the
           superblock operations of the underlying file system.  */
        caops->cache_sops = cache_sops;

        /*
         * Copy the cache (real fs) superblock ops to the "filter"
         * superblock ops as defaults. Some will be changed below
         */
        memcpy(&props->filter_sops, cache_sops, sizeof(*cache_sops));

        /* 'put_super' unconditionally is that of filter */
        if (filter_sops->put_super) { 
                props->filter_sops.put_super = filter_sops->put_super;
        }

        if (cache_sops->read_inode) {
                props->filter_sops.read_inode = filter_sops->read_inode;
                FDEBUG(D_INODE, "setting filter_read_inode, cache_ops %p, cache %p, ri at %p\n",
                      cache, cache, props->filter_sops.read_inode);
        }

        if (cache_sops->remount_fs)
                props->filter_sops.remount_fs = filter_sops->remount_fs;
        FEXIT;
}


void filter_setup_dir_ops(struct filter_fs *cache, struct inode *inode, struct inode_operations *filter_iops, struct file_operations *filter_fops)
{
        struct inode_operations *cache_filter_iops;
        struct inode_operations *cache_iops = inode->i_op;
        struct file_operations *cache_fops = inode->i_fop;
        FENTRY;

        if ( cache->o_flags & FILTER_DID_DIR_OPS ) {
                FEXIT;
                return;
        }
        cache->o_flags |= FILTER_DID_DIR_OPS;

        /* former ops become cache_ops */
        cache->o_caops.cache_dir_iops = cache_iops;
        cache->o_caops.cache_dir_fops = cache_fops;
        FDEBUG(D_SUPER, "filter at %p, cache iops %p, iops %p\n",
               cache, cache_iops, filter_c2udiops(cache));

        /* setup our dir iops: copy and modify */
        memcpy(filter_c2udiops(cache), cache_iops, sizeof(*cache_iops));

        /* abbreviate */
        cache_filter_iops = filter_c2udiops(cache);

        /* methods that filter if cache filesystem has these ops */
        if (cache_iops->lookup && filter_iops->lookup)
                cache_filter_iops->lookup = filter_iops->lookup;
        if (cache_iops->create && filter_iops->create)
                cache_filter_iops->create = filter_iops->create;
        if (cache_iops->link && filter_iops->link)
                cache_filter_iops->link = filter_iops->link;
        if (cache_iops->unlink && filter_iops->unlink)
                cache_filter_iops->unlink = filter_iops->unlink;
        if (cache_iops->mkdir && filter_iops->mkdir)
                cache_filter_iops->mkdir = filter_iops->mkdir;
        if (cache_iops->rmdir && filter_iops->rmdir)
                cache_filter_iops->rmdir = filter_iops->rmdir;
        if (cache_iops->symlink && filter_iops->symlink)
                cache_filter_iops->symlink = filter_iops->symlink;
        if (cache_iops->rename && filter_iops->rename)
                cache_filter_iops->rename = filter_iops->rename;
        if (cache_iops->mknod && filter_iops->mknod)
                cache_filter_iops->mknod = filter_iops->mknod;
        if (cache_iops->permission && filter_iops->permission)
                cache_filter_iops->permission = filter_iops->permission;
        if (cache_iops->getattr)
                cache_filter_iops->getattr = filter_iops->getattr;
        /* Some filesystems do not use a setattr method of their own
           instead relying on inode_setattr/write_inode. We still need to
           journal these so we make setattr an unconditional operation. 
           XXX: we should probably check for write_inode. SHP
        */
        /*if (cache_iops->setattr)*/
                cache_filter_iops->setattr = filter_iops->setattr;
#ifdef CONFIG_FS_EXT_ATTR
	/* For now we assume that posix acls are handled through extended
	* attributes. If this is not the case, we must explicitly trap 
	* posix_set_acl. SHP
	*/
	if (cache_iops->set_ext_attr && filter_iops->set_ext_attr)
		cache_filter_iops->set_ext_attr = filter_iops->set_ext_attr;
#endif


        /* copy dir fops */
        memcpy(filter_c2udfops(cache), cache_fops, sizeof(*cache_fops));

        /* unconditional filtering operations */
        filter_c2udfops(cache)->ioctl = filter_fops->ioctl;

        FEXIT;
}


void filter_setup_file_ops(struct filter_fs *cache, struct inode *inode, struct inode_operations *filter_iops, struct file_operations *filter_fops)
{
        struct inode_operations *pr_iops;
        struct inode_operations *cache_iops = inode->i_op;
        struct file_operations *cache_fops = inode->i_fop;
        FENTRY;

        if ( cache->o_flags & FILTER_DID_FILE_OPS ) {
                FEXIT;
                return;
        }
        cache->o_flags |= FILTER_DID_FILE_OPS;

        /* steal the old ops */
        /* former ops become cache_ops */
        cache->o_caops.cache_file_iops = cache_iops;
        cache->o_caops.cache_file_fops = cache_fops;
        
        /* abbreviate */
        pr_iops = filter_c2ufiops(cache); 

        /* setup our dir iops: copy and modify */
        memcpy(pr_iops, cache_iops, sizeof(*cache_iops));

        /* copy dir fops */
        CERROR("*** cache file ops at %p\n", cache_fops);
        memcpy(filter_c2uffops(cache), cache_fops, sizeof(*cache_fops));

        /* assign */
        /* See comments above in filter_setup_dir_ops. SHP */
        /*if (cache_iops->setattr)*/
                pr_iops->setattr = filter_iops->setattr;
        if (cache_iops->getattr)
                pr_iops->getattr = filter_iops->getattr;
        /* XXX Should this be conditional rmr ? */
        pr_iops->permission = filter_iops->permission;
#ifdef CONFIG_FS_EXT_ATTR
    	/* For now we assume that posix acls are handled through extended
	* attributes. If this is not the case, we must explicitly trap and 
	* posix_set_acl
	*/
	if (cache_iops->set_ext_attr && filter_iops->set_ext_attr)
		pr_iops->set_ext_attr = filter_iops->set_ext_attr;
#endif


        /* unconditional filtering operations */
        filter_c2uffops(cache)->open = filter_fops->open;
        filter_c2uffops(cache)->release = filter_fops->release;
        filter_c2uffops(cache)->write = filter_fops->write;
        filter_c2uffops(cache)->ioctl = filter_fops->ioctl;

        FEXIT;
}

/* XXX in 2.3 there are "fast" and "slow" symlink ops for ext2 XXX */
void filter_setup_symlink_ops(struct filter_fs *cache, struct inode *inode, struct inode_operations *filter_iops, struct file_operations *filter_fops)
{
        struct inode_operations *pr_iops;
        struct inode_operations *cache_iops = inode->i_op;
        struct file_operations *cache_fops = inode->i_fop;
        FENTRY;

        if ( cache->o_flags & FILTER_DID_SYMLINK_OPS ) {
                FEXIT;
                return;
        }
        cache->o_flags |= FILTER_DID_SYMLINK_OPS;

        /* steal the old ops */
        cache->o_caops.cache_sym_iops = cache_iops;
        cache->o_caops.cache_sym_fops = cache_fops;

        /* abbreviate */
        pr_iops = filter_c2usiops(cache); 

        /* setup our dir iops: copy and modify */
        memcpy(pr_iops, cache_iops, sizeof(*cache_iops));

        /* See comments above in filter_setup_dir_ops. SHP */
        /* if (cache_iops->setattr) */
                pr_iops->setattr = filter_iops->setattr;
        if (cache_iops->getattr)
                pr_iops->getattr = filter_iops->getattr;

        /* assign */
        /* copy fops - careful for symlinks they might be NULL */
        if ( cache_fops ) { 
                memcpy(filter_c2usfops(cache), cache_fops, sizeof(*cache_fops));
        }

        FEXIT;
}

void filter_setup_dentry_ops(struct filter_fs *cache,
                             struct dentry_operations *cache_dop,
                             struct dentry_operations *filter_dop)
{
        if ( cache->o_flags & FILTER_DID_DENTRY_OPS ) {
                FEXIT;
                return;
        }
        cache->o_flags |= FILTER_DID_DENTRY_OPS;

        cache->o_caops.cache_dentry_ops = cache_dop;
        memcpy(&cache->o_fops.filter_dentry_ops,
               filter_dop, sizeof(*filter_dop));
        
        if (cache_dop &&  cache_dop != filter_dop && cache_dop->d_revalidate){
                CERROR("WARNING: filter overriding revalidation!\n");
        }
        return;
}
