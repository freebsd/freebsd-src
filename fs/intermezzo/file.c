/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (C) 2000 Stelias Computing, Inc.
 *  Copyright (C) 2000 Red Hat, Inc.
 *  Copyright (C) 2000 TurboLinux, Inc.
 *  Copyright (C) 2000 Los Alamos National Laboratory.
 *  Copyright (C) 2000, 2001 Tacit Networks, Inc.
 *  Copyright (C) 2000 Peter J. Braam
 *  Copyright (C) 2001 Mountain View Data, Inc. 
 *  Copyright (C) 2001 Cluster File Systems, Inc. 
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
 *  This file manages file I/O
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
#include <linux/smp_lock.h>
#define __NO_VERSION__
#include <linux/module.h>

#include <linux/intermezzo_fs.h>
#include <linux/intermezzo_psdev.h>
#include <linux/fsfilter.h>
/*
 * these are initialized in super.c
 */
extern int presto_permission(struct inode *inode, int mask);


static int presto_open_upcall(int minor, struct dentry *de)
{
        int rc = 0;
        char *path, *buffer;
        struct presto_file_set *fset;
        int pathlen;
        struct lento_vfs_context info;
        struct presto_dentry_data *dd = presto_d2d(de);

        PRESTO_ALLOC(buffer, PAGE_SIZE);
        if ( !buffer ) {
                CERROR("PRESTO: out of memory!\n");
                return -ENOMEM;
        }
        fset = presto_fset(de);
        path = presto_path(de, fset->fset_dentry, buffer, PAGE_SIZE);
        pathlen = MYPATHLEN(buffer, path);
        
        CDEBUG(D_FILE, "de %p, dd %p\n", de, dd);
        if (dd->remote_ino == 0) {
                rc = presto_get_fileid(minor, fset, de);
        }
        memset (&info, 0, sizeof(info));
        if (dd->remote_ino > 0) {
                info.remote_ino = dd->remote_ino;
                info.remote_generation = dd->remote_generation;
        } else
                CERROR("get_fileid failed %d, ino: %Lx, fetching by name\n", rc,
                       dd->remote_ino);

        rc = izo_upc_open(minor, pathlen, path, fset->fset_name, &info);
        PRESTO_FREE(buffer, PAGE_SIZE);
        return rc;
}

static inline int open_check_dod(struct file *file,
                                 struct presto_file_set *fset)
{
        int gen, is_iopen = 0, minor;
        struct presto_cache *cache = fset->fset_cache;
        ino_t inum;

        minor = presto_c2m(cache);

        if ( ISLENTO(minor) ) {
                CDEBUG(D_CACHE, "is lento, not doing DOD.\n");
                return 0;
        }

        /* Files are only ever opened by inode during backfetches, when by
         * definition we have the authoritative copy of the data.  No DOD. */
        is_iopen = izo_dentry_is_ilookup(file->f_dentry, &inum, &gen);

        if (is_iopen) {
                CDEBUG(D_CACHE, "doing iopen, not doing DOD.\n");
                return 0;
        }

        if (!(fset->fset_flags & FSET_DATA_ON_DEMAND)) {
                CDEBUG(D_CACHE, "fileset not on demand.\n");
                return 0;
        }
                
        if (file->f_flags & O_TRUNC) {
                CDEBUG(D_CACHE, "fileset dod: O_TRUNC.\n");
                return 0;
        }
                
        if (presto_chk(file->f_dentry, PRESTO_DONT_JOURNAL)) {
                CDEBUG(D_CACHE, "file under .intermezzo, not doing DOD\n");
                return 0;
        }

        if (presto_chk(file->f_dentry, PRESTO_DATA)) {
                CDEBUG(D_CACHE, "PRESTO_DATA is set, not doing DOD.\n");
                return 0;
        }

        if (cache->cache_filter->o_trops->tr_all_data(file->f_dentry->d_inode)) {
                CDEBUG(D_CACHE, "file not sparse, not doing DOD.\n");
                return 0;
        }

        return 1;
}

static int presto_file_open(struct inode *inode, struct file *file)
{
        int rc = 0;
        struct file_operations *fops;
        struct presto_cache *cache;
        struct presto_file_set *fset;
        struct presto_file_data *fdata;
        int writable = (file->f_flags & (O_RDWR | O_WRONLY));
        int minor, i;

        ENTRY;

        if (presto_prep(file->f_dentry, &cache, &fset) < 0) {
                EXIT;
                return -EBADF;
        }

        minor = presto_c2m(cache);

        CDEBUG(D_CACHE, "DATA_OK: %d, ino: %ld, islento: %d\n",
               presto_chk(file->f_dentry, PRESTO_DATA), inode->i_ino,
               ISLENTO(minor));

        if ( !ISLENTO(minor) && (file->f_flags & O_RDWR ||
                                 file->f_flags & O_WRONLY)) {
                CDEBUG(D_CACHE, "calling presto_get_permit\n");
                if ( presto_get_permit(inode) < 0 ) {
                        EXIT;
                        return -EROFS;
                }
                presto_put_permit(inode);
        }

        if (open_check_dod(file, fset)) {
                CDEBUG(D_CACHE, "presto_open_upcall\n");
                CDEBUG(D_CACHE, "dentry: %p setting DATA, ATTR\n", file->f_dentry);
                presto_set(file->f_dentry, PRESTO_ATTR | PRESTO_DATA);
                rc = presto_open_upcall(minor, file->f_dentry);
                if (rc) {
                        EXIT;
                        CERROR("%s: returning error %d\n", __FUNCTION__, rc);
                        return rc;
                }

        }

        /* file was truncated upon open: do not refetch */
        if (file->f_flags & O_TRUNC) { 
                CDEBUG(D_CACHE, "setting DATA, ATTR\n");
                presto_set(file->f_dentry, PRESTO_ATTR | PRESTO_DATA);
        }

        fops = filter_c2cffops(cache->cache_filter);
        if ( fops->open ) {
                CDEBUG(D_CACHE, "calling fs open\n");
                rc = fops->open(inode, file);

                if (rc) {
                        EXIT;
                        return rc;
                }
        }

        if (writable) {
                PRESTO_ALLOC(fdata, sizeof(*fdata));
                if (!fdata) {
                        EXIT;
                        return -ENOMEM;
                }
                /* LOCK: XXX check that the kernel lock protects this alloc */
                fdata->fd_do_lml = 0;
                fdata->fd_bytes_written = 0;
                fdata->fd_fsuid = current->fsuid;
                fdata->fd_fsgid = current->fsgid;
                fdata->fd_mode = file->f_dentry->d_inode->i_mode;
                fdata->fd_uid = file->f_dentry->d_inode->i_uid;
                fdata->fd_gid = file->f_dentry->d_inode->i_gid;
                fdata->fd_ngroups = current->ngroups;
                for (i=0 ; i < current->ngroups ; i++)
                        fdata->fd_groups[i] = current->groups[i];
                if (!ISLENTO(minor)) 
                        fdata->fd_info.flags = LENTO_FL_KML; 
                else { 
                        /* this is for the case of DOD, 
                           reint_close will adjust flags if needed */
                        fdata->fd_info.flags = 0;
                }

                presto_getversion(&fdata->fd_version, inode);
                file->private_data = fdata;
        } else {
                file->private_data = NULL;
        }

        EXIT;
        return 0;
}

int presto_adjust_lml(struct file *file, struct lento_vfs_context *info)
{
        struct presto_file_data *fdata = 
                (struct presto_file_data *) file->private_data;

        if (!fdata) { 
                EXIT;
                return -EINVAL;
        }
                
        memcpy(&fdata->fd_info, info, sizeof(*info));
        EXIT;
        return 0; 
}


static int presto_file_release(struct inode *inode, struct file *file)
{
        int rc;
        struct file_operations *fops;
        struct presto_cache *cache;
        struct presto_file_set *fset;
        struct presto_file_data *fdata = 
                (struct presto_file_data *)file->private_data;
        ENTRY;

        rc = presto_prep(file->f_dentry, &cache, &fset);
        if ( rc ) {
                EXIT;
                return rc;
        }

        fops = filter_c2cffops(cache->cache_filter);
        if (fops && fops->release)
                rc = fops->release(inode, file);

        CDEBUG(D_CACHE, "islento = %d (minor %d), rc %d, data %p\n",
               ISLENTO(cache->cache_psdev->uc_minor), 
               cache->cache_psdev->uc_minor, rc, fdata);

        /* this file was modified: ignore close errors, write KML */
        if (fdata && fdata->fd_do_lml) {
                /* XXX: remove when lento gets file granularity cd */
                if ( presto_get_permit(inode) < 0 ) {
                        EXIT;
                        return -EROFS;
                }
        
                fdata->fd_info.updated_time = file->f_dentry->d_inode->i_mtime;
                rc = presto_do_close(fset, file); 
                presto_put_permit(inode);
        }

        if (!rc && fdata) {
                PRESTO_FREE(fdata, sizeof(*fdata));
                file->private_data = NULL; 
        }
        
        EXIT;
        return rc;
}

static void presto_apply_write_policy(struct file *file,
                                      struct presto_file_set *fset, loff_t res)
{
        struct presto_file_data *fdata =
                (struct presto_file_data *)file->private_data;
        struct presto_cache *cache = fset->fset_cache;
        struct presto_version new_file_ver;
        int error;
        struct rec_info rec;

        /* Here we do a journal close after a fixed or a specified
         amount of KBytes, currently a global parameter set with
         sysctl. If files are open for a long time, this gives added
         protection. (XXX todo: per cache, add ioctl, handle
         journaling in a thread, add more options etc.)
        */ 
 
        if ((fset->fset_flags & FSET_JCLOSE_ON_WRITE) &&
            (!ISLENTO(cache->cache_psdev->uc_minor))) {
                fdata->fd_bytes_written += res;
 
                if (fdata->fd_bytes_written >= fset->fset_file_maxio) {
                        presto_getversion(&new_file_ver,
                                          file->f_dentry->d_inode);
                        /* This is really heavy weight and should be fixed
                           ASAP. At most we should be recording the number
                           of bytes written and not locking the kernel, 
                           wait for permits, etc, on the write path. SHP
                        */
                        lock_kernel();
                        if ( presto_get_permit(file->f_dentry->d_inode) < 0 ) {
                                EXIT;
                                /* we must be disconnected, not to worry */
                                unlock_kernel();
                                return; 
                        }
                        error = presto_journal_close(&rec, fset, file,
                                                     file->f_dentry,
                                                     &fdata->fd_version,
                                                     &new_file_ver);
                        presto_put_permit(file->f_dentry->d_inode);
                        unlock_kernel();
                        if ( error ) {
                                CERROR("presto_close: cannot journal close\n");
                                /* XXX these errors are really bad */
                                /* panic(); */
                                return;
                        }
                        fdata->fd_bytes_written = 0;
                }
        }
}

static ssize_t presto_file_write(struct file *file, const char *buf,
                                 size_t size, loff_t *off)
{
        struct rec_info rec;
        int error;
        struct presto_cache *cache;
        struct presto_file_set *fset;
        struct file_operations *fops;
        ssize_t res;
        int do_lml_here;
        void *handle = NULL;
        unsigned long blocks;
        struct presto_file_data *fdata;
        loff_t res_size; 

        error = presto_prep(file->f_dentry, &cache, &fset);
        if ( error ) {
                EXIT;
                return error;
        }

        blocks = (size >> file->f_dentry->d_inode->i_sb->s_blocksize_bits) + 1;
        /* XXX 3 is for ext2 indirect blocks ... */ 
        res_size = 2 * PRESTO_REQHIGH + ((blocks+3) 
                << file->f_dentry->d_inode->i_sb->s_blocksize_bits);

        error = presto_reserve_space(fset->fset_cache, res_size); 
        CDEBUG(D_INODE, "Reserved %Ld for %d\n", res_size, size); 
        if ( error ) { 
                EXIT;
                return -ENOSPC;
        }

        CDEBUG(D_INODE, "islento %d, minor: %d\n", 
               ISLENTO(cache->cache_psdev->uc_minor),
               cache->cache_psdev->uc_minor); 

        /* 
         *  XXX this lock should become a per inode lock when 
         *  Vinny's changes are in; we could just use i_sem.
         */
        read_lock(&fset->fset_lml.fd_lock); 
        fdata = (struct presto_file_data *)file->private_data;
        do_lml_here = size && (fdata->fd_do_lml == 0) &&
                !presto_chk(file->f_dentry, PRESTO_DONT_JOURNAL);

        if (do_lml_here)
                fdata->fd_do_lml = 1;
        read_unlock(&fset->fset_lml.fd_lock); 

        /* XXX 
           There might be a bug here.  We need to make 
           absolutely sure that the ext3_file_write commits 
           after our transaction that writes the LML record.
           Nesting the file write helps if new blocks are allocated. 
        */
        res = 0;
        if (do_lml_here) {
                struct presto_version file_version;
                /* handle different space reqs from file system below! */
                handle = presto_trans_start(fset, file->f_dentry->d_inode, 
                                            KML_OPCODE_WRITE);
                if ( IS_ERR(handle) ) {
                        presto_release_space(fset->fset_cache, res_size); 
                        CERROR("presto_write: no space for transaction\n");
                        return -ENOSPC;
                }

                presto_getversion(&file_version, file->f_dentry->d_inode); 
                res = presto_write_lml_close(&rec, fset, file, 
                                             fdata->fd_info.remote_ino, 
                                             fdata->fd_info.remote_generation, 
                                             &fdata->fd_info.remote_version, 
                                             &file_version);
                fdata->fd_lml_offset = rec.offset;
                if ( res ) {
                        CERROR("intermezzo: PANIC failed to write LML\n");
                        *(int *)0 = 1;
                        EXIT;
                        goto exit_write;
                }
                presto_trans_commit(fset, handle);
        }

        fops = filter_c2cffops(cache->cache_filter);
        res = fops->write(file, buf, size, off);
        if ( res != size ) {
                CDEBUG(D_FILE, "file write returns short write: size %d, res %d\n", size, res); 
        }

        if ( (res > 0) && fdata ) 
                 presto_apply_write_policy(file, fset, res);

 exit_write:
        presto_release_space(fset->fset_cache, res_size); 
        return res;
}

struct file_operations presto_file_fops = {
        .write   = presto_file_write,
        .open    = presto_file_open,
        .release = presto_file_release,
        .ioctl   = presto_ioctl
};

struct inode_operations presto_file_iops = {
        .permission   = presto_permission,
        .setattr      = presto_setattr,
#ifdef CONFIG_FS_EXT_ATTR
        .set_ext_attr = presto_set_ext_attr,
#endif
};

/* FIXME: I bet we want to add a lock here and in presto_file_open. */
int izo_purge_file(struct presto_file_set *fset, char *file)
{
#if 0
        void *handle = NULL;
        char *path = NULL;
        struct nameidata nd;
        struct dentry *dentry;
        int rc = 0, len;
        loff_t oldsize;

        /* FIXME: not mtpt it's gone */
        len = strlen(fset->fset_cache->cache_mtpt) + strlen(file) + 1;
        PRESTO_ALLOC(path, len + 1);
        if (path == NULL)
                return -1;

        sprintf(path, "%s/%s", fset->fset_cache->cache_mtpt, file);
        rc = izo_lookup_file(fset, path, &nd);
        if (rc)
                goto error;
        dentry = nd.dentry;

        /* FIXME: take a lock here */

        if (dentry->d_inode->i_atime > CURRENT_TIME - 5) {
                /* We lost the race; this file was accessed while we were doing
                 * ioctls and lookups and whatnot. */
                rc = -EBUSY;
                goto error_unlock;
        }

        /* FIXME: Check if this file is open. */

        handle = presto_trans_start(fset, dentry->d_inode, KML_OPCODE_TRUNC);
        if (IS_ERR(handle)) {
                rc = -ENOMEM;
                goto error_unlock;
        }

        /* FIXME: Write LML record */

        oldsize = dentry->d_inode->i_size;
        rc = izo_do_truncate(fset, dentry, 0, oldsize);
        if (rc != 0)
                goto error_clear;
        rc = izo_do_truncate(fset, dentry, oldsize, 0);
        if (rc != 0)
                goto error_clear;

 error_clear:
        /* FIXME: clear LML record */

 error_unlock:
        /* FIXME: release the lock here */

 error:
        if (handle != NULL && !IS_ERR(handle))
                presto_trans_commit(fset, handle);
        if (path != NULL)
                PRESTO_FREE(path, len + 1);
        return rc;
#else
        return 0;
#endif
}
