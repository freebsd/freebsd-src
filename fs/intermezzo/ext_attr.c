/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 * 
 *  Copyright (C) 2001 Tacit Networks, Inc.
 *    Author: Shirish H. Phatak <shirish@tacitnetworks.com>
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
 * Extended attribute handling for presto.
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
#include <linux/smp_lock.h>

#include <linux/intermezzo_fs.h>
#include <linux/intermezzo_psdev.h>

#ifdef CONFIG_FS_EXT_ATTR
#include <linux/ext_attr.h>

extern inline void presto_debug_fail_blkdev(struct presto_file_set *fset,
                                            unsigned long value);


/* VFS interface */
/* XXX! Fixme test for user defined attributes */
int presto_set_ext_attr(struct inode *inode, 
                        const char *name, void *buffer,
                        size_t buffer_len, int flags) 
{
        int error;
        struct presto_cache *cache;
        struct presto_file_set *fset;
        struct lento_vfs_context info;
        struct dentry *dentry;
        int minor = presto_i2m(inode);
        char *buf = NULL;

        ENTRY;
        if (minor < 0) {
                EXIT;
                return -1;
        }

        if ( ISLENTO(minor) ) {
                EXIT;
                return -EINVAL;
        }

        /* BAD...vfs should really pass down the dentry to use, especially
         * since every other operation in iops does. But for now
         * we do a reverse mapping from inode to the first dentry 
         */
        if (list_empty(&inode->i_dentry)) {
                CERROR("No alias for inode %d\n", (int) inode->i_ino);
                EXIT;
                return -EINVAL;
        }

        dentry = list_entry(inode->i_dentry.next, struct dentry, d_alias);

        error = presto_prep(dentry, &cache, &fset);
        if ( error ) {
                EXIT;
                return error;
        }

        if ((buffer != NULL) && (buffer_len != 0)) {
            /* If buffer is a user space pointer copy it to kernel space
            * and reset the flag. We do this since the journal functions need
            * access to the contents of the buffer, and the file system
            * does not care. When we actually invoke the function, we remove
            * the EXT_ATTR_FLAG_USER flag.
            *
            * XXX:Check if the "fs does not care" assertion is always true -SHP
            * (works for ext3)
            */
            if (flags & EXT_ATTR_FLAG_USER) {
                PRESTO_ALLOC(buf, buffer_len);
                if (!buf) {
                        CERROR("InterMezzo: out of memory!!!\n");
                        return -ENOMEM;
                }
                error = copy_from_user(buf, buffer, buffer_len);
                if (error) 
                        return -EFAULT;
            } else 
                buf = buffer;
        } else
                buf = buffer;

        if ( presto_get_permit(inode) < 0 ) {
                EXIT;
                if (buffer_len && (flags & EXT_ATTR_FLAG_USER))
                        PRESTO_FREE(buf, buffer_len);
                return -EROFS;
        }

        /* Simulate presto_setup_info */
        memset(&info, 0, sizeof(info));
        /* For now redundant..but we keep it around just in case */
        info.flags = LENTO_FL_IGNORE_TIME;
        if (!ISLENTO(cache->cache_psdev->uc_minor))
            info.flags |= LENTO_FL_KML;

        /* We pass in the kernel space pointer and reset the 
         * EXT_ATTR_FLAG_USER flag.
         * See comments above. 
         */ 
        /* Note that mode is already set by VFS so we send in a NULL */
        error = presto_do_set_ext_attr(fset, dentry, name, buf,
                                       buffer_len, flags & ~EXT_ATTR_FLAG_USER,
                                       NULL, &info);
        presto_put_permit(inode);

        if (buffer_len && (flags & EXT_ATTR_FLAG_USER))
                PRESTO_FREE(buf, buffer_len);
        EXIT;
        return error;
}

/* Lento Interface */
/* XXX: ignore flags? We should be forcing these operations through? -SHP*/
int lento_set_ext_attr(const char *path, const char *name, 
                       void *buffer, size_t buffer_len, int flags, mode_t mode, 
                       struct lento_vfs_context *info) 
{
        int error;
        char * pathname;
        struct nameidata nd;
        struct dentry *dentry;
        struct presto_file_set *fset;

        ENTRY;
        lock_kernel();

        pathname=getname(path);
        error = PTR_ERR(pathname);
        if (IS_ERR(pathname)) {
                EXIT;
                goto exit;
        }

        /* Note that ext_attrs apply to both files and directories..*/
        error=presto_walk(pathname,&nd);
        if (error) 
		goto exit;
        dentry = nd.dentry;

        fset = presto_fset(dentry);
        error = -EINVAL;
        if ( !fset ) {
                CERROR("No fileset!\n");
                EXIT;
                goto exit_dentry;
        }

        if (buffer==NULL) buffer_len=0;

        error = presto_do_set_ext_attr(fset, dentry, name, buffer,
                                       buffer_len, flags, &mode, info);
exit_dentry:
        path_release(&nd);
exit_path:
        putname(pathname);
exit:
        unlock_kernel();
        return error; 
}

#endif /*CONFIG_FS_EXT_ATTR*/
