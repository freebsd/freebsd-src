/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (C) 2001 Cluster File Systems, Inc. <braam@clusterfs.com>
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
 *  Managing filesets
 *
 */

#define __NO_VERSION__
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
#include <linux/module.h>

#include <linux/intermezzo_fs.h>
#include <linux/intermezzo_psdev.h>

static inline struct presto_file_set *presto_dentry2fset(struct dentry *dentry)
{
        if (presto_d2d(dentry) == NULL) {
                EXIT;
                return NULL;
        }
        return presto_d2d(dentry)->dd_fset;
}

/* find the fileset dentry for this dentry */
struct presto_file_set *presto_fset(struct dentry *de)
{
        struct dentry *fsde;
        ENTRY;
        if ( !de->d_inode ) {
                /* FIXME: is this ok to be NULL? */
                CDEBUG(D_INODE,"presto_fset: warning %*s has NULL inode.\n",
                de->d_name.len, de->d_name.name);
        }
        for (fsde = de;; fsde = fsde->d_parent) {
                if ( presto_dentry2fset(fsde) ) {
                        EXIT;
                        return presto_dentry2fset(fsde);
                }
                if (fsde->d_parent == fsde)
                        break;
        }
        EXIT;
        return NULL;
}

int presto_get_lastrecno(char *path, off_t *recno)
{
        struct nameidata nd; 
        struct presto_file_set *fset;
        struct dentry *dentry;
        int error;
        ENTRY;

        error = presto_walk(path, &nd);
        if (error) {
                EXIT;
                return error;
        }

        dentry = nd.dentry;

        error = -ENXIO;
        if ( !presto_ispresto(dentry->d_inode) ) {
                EXIT;
                goto kml_out;
        }

        error = -EINVAL;
        if ( ! presto_dentry2fset(dentry)) {
                EXIT;
                goto kml_out;
        }

        fset = presto_dentry2fset(dentry);
        if (!fset) {
                EXIT;
                goto kml_out;
        }
        error = 0;
        *recno = fset->fset_kml.fd_recno;

 kml_out:
        path_release(&nd);
        return error;
}

static char * _izo_make_path(char *fsetname, char *name)
{
        char *path = NULL;
        int len;

        len = strlen("/.intermezzo/") + strlen(fsetname) 
                + 1 + strlen(name) + 1;

        PRESTO_ALLOC(path, len);
        if (path == NULL)
                return NULL;

        sprintf(path, "/.intermezzo/%s/%s", fsetname, name);

        return path;
}

char * izo_make_path(struct presto_file_set *fset, char *name)
{
        return _izo_make_path(fset->fset_name, name);
}

static struct file *_izo_fset_open(char *fsetname, char *name, int flags, int mode) 
{
        char *path;
        struct file *f;
        int error;
        ENTRY;

        path = _izo_make_path(fsetname, name);
        if (path == NULL) {
                EXIT;
                return ERR_PTR(-ENOMEM);
        }

        CDEBUG(D_INODE, "opening file %s\n", path);
        f = filp_open(path, flags, mode);
        error = PTR_ERR(f);
        if (IS_ERR(f)) {
                CDEBUG(D_INODE, "Error %d\n", error);
        }

        PRESTO_FREE(path, strlen(path));

        EXIT;
        return f;

}

struct file *izo_fset_open(struct presto_file_set *fset, char *name, int flags, int mode) 
{
        return _izo_fset_open(fset->fset_name, name, flags, mode);
}



/*
 *  note: this routine "pins" a dentry for a fileset root
 */
int presto_set_fsetroot(struct dentry *ioctl_dentry, char *fsetname,
                        unsigned int flags)
{
        struct presto_file_set *fset = NULL;
        struct presto_cache *cache;
        int error;
        struct file  *fset_root;
        struct dentry *dentry;

        ENTRY;

        fset_root = _izo_fset_open(fsetname, "ROOT",  O_RDONLY, 000);
        if (IS_ERR(fset_root)) {
                CERROR("Can't open %s/ROOT\n", fsetname);
                EXIT;
                error = PTR_ERR(fset_root);
                goto out;
        }
        dentry = dget(fset_root->f_dentry);
        filp_close(fset_root, NULL);

        dentry->d_inode->i_op = ioctl_dentry->d_inode->i_op;
        dentry->d_inode->i_fop = ioctl_dentry->d_inode->i_fop;
        dentry->d_op = ioctl_dentry->d_op;
        fset = presto_dentry2fset(dentry);
        if (fset && (fset->fset_dentry == dentry) ) { 
                CERROR("Fsetroot already set (inode %ld)\n",
                       dentry->d_inode->i_ino);
                /* XXX: ignore because clear_fsetroot is broken  */
#if 0
                dput(dentry);
                EXIT;
                error = -EEXIST;
                goto out;
#endif
        }

        cache = presto_get_cache(dentry->d_inode);
        if (!cache) { 
                CERROR("No cache found for inode %ld\n",
                       dentry->d_inode->i_ino);
                EXIT;
                error = -ENODEV;
                goto out_free;
        }

        PRESTO_ALLOC(fset, sizeof(*fset));
        if ( !fset ) {
                CERROR("No memory allocating fset for %s\n", fsetname);
                EXIT;
                error = -ENOMEM;
                goto out_free;
        }
        CDEBUG(D_INODE, "fset at %p\n", fset);

        CDEBUG(D_INODE, "InterMezzo: fsetroot: inode %ld, fileset name %s\n",
               dentry->d_inode->i_ino, fsetname);

        fset->fset_mnt = mntget(current->fs->pwdmnt); 
        fset->fset_cache = cache;
        fset->fset_dentry = dentry; 
        fset->fset_name = strdup(fsetname);
        fset->fset_chunkbits = CHUNK_BITS;
        fset->fset_flags = flags;
        fset->fset_file_maxio = FSET_DEFAULT_MAX_FILEIO; 
        fset->fset_permit_lock = SPIN_LOCK_UNLOCKED;
        PRESTO_ALLOC(fset->fset_reint_buf, 64 * 1024);
        if (fset->fset_reint_buf == NULL) {
                EXIT;
                error = -ENOMEM;
                goto out_free;
        }
        init_waitqueue_head(&fset->fset_permit_queue);

        if (presto_d2d(dentry) == NULL) { 
                dentry->d_fsdata = izo_alloc_ddata();
        }
        if (presto_d2d(dentry) == NULL) {
                CERROR("InterMezzo: %s: no memory\n", __FUNCTION__);
                EXIT;
                error = -ENOMEM;
                goto out_free;
        }
        presto_d2d(dentry)->dd_fset = fset;
        list_add(&fset->fset_list, &cache->cache_fset_list);

        error = izo_init_kml_file(fset, &fset->fset_kml);
        if ( error ) {
                EXIT;
                CDEBUG(D_JOURNAL, "Error init_kml %d\n", error);
                goto out_list_del;
        }

        error = izo_init_lml_file(fset, &fset->fset_lml);
        if ( error ) {
                int rc;
                EXIT;
                rc = izo_log_close(&fset->fset_kml);
                CDEBUG(D_JOURNAL, "Error init_lml %d, cleanup %d\n", error, rc);
                goto out_list_del;
        }

        /* init_last_rcvd_file could trigger a presto_file_write(), which
         * requires that the lml structure be initialized. -phil */
        error = izo_init_last_rcvd_file(fset, &fset->fset_rcvd);
        if ( error ) {
                int rc;
                EXIT;
                rc = izo_log_close(&fset->fset_kml);
                rc = izo_log_close(&fset->fset_lml);
                CDEBUG(D_JOURNAL, "Error init_lastrcvd %d, cleanup %d\n", error, rc);
                goto out_list_del;
        }

        CDEBUG(D_PIOCTL, "-------> fset at %p, dentry at %p, mtpt %p,"
               "fset %s, cache %p, presto_d2d(dentry)->dd_fset %p\n",
               fset, dentry, fset->fset_dentry, fset->fset_name, cache,
               presto_d2d(dentry)->dd_fset);

        EXIT;
        return 0;

 out_list_del:
        list_del(&fset->fset_list);
        presto_d2d(dentry)->dd_fset = NULL;
 out_free:
        if (fset) {
                mntput(fset->fset_mnt); 
                if (fset->fset_reint_buf != NULL)
                        PRESTO_FREE(fset->fset_reint_buf, 64 * 1024);
                PRESTO_FREE(fset, sizeof(*fset));
        }
        dput(dentry); 
 out:
        return error;
}

static int izo_cleanup_fset(struct presto_file_set *fset)
{
        int error;
        struct presto_cache *cache;

        ENTRY;

        CERROR("Cleaning up fset %s\n", fset->fset_name);

        error = izo_log_close(&fset->fset_kml);
        if (error)
                CERROR("InterMezzo: Closing kml for fset %s: %d\n",
                       fset->fset_name, error);
        error = izo_log_close(&fset->fset_lml);
        if (error)
                CERROR("InterMezzo: Closing lml for fset %s: %d\n",
                       fset->fset_name, error);
        error = izo_log_close(&fset->fset_rcvd);
        if (error)
                CERROR("InterMezzo: Closing last_rcvd for fset %s: %d\n",
                       fset->fset_name, error);

        cache = fset->fset_cache;

        list_del(&fset->fset_list);

        presto_d2d(fset->fset_dentry)->dd_fset = NULL;
        dput(fset->fset_dentry);
        mntput(fset->fset_mnt);

        PRESTO_FREE(fset->fset_name, strlen(fset->fset_name) + 1);
        PRESTO_FREE(fset->fset_reint_buf, 64 * 1024);
        PRESTO_FREE(fset, sizeof(*fset));
        EXIT;
        return error;
}

int izo_clear_fsetroot(struct dentry *dentry)
{
        struct presto_file_set *fset;

        ENTRY;

        fset = presto_dentry2fset(dentry);
        if (!fset) {
                EXIT;
                return -EINVAL;
        }

        izo_cleanup_fset(fset);
        EXIT;
        return 0;
}

int izo_clear_all_fsetroots(struct presto_cache *cache)
{
        struct presto_file_set *fset;
        struct list_head *tmp,*tmpnext;
        int error;
 
        error = 0;
        tmp = &cache->cache_fset_list;
        tmpnext = tmp->next;
        while ( tmpnext != &cache->cache_fset_list) {
                tmp = tmpnext;
                tmpnext = tmp->next;
                fset = list_entry(tmp, struct presto_file_set, fset_list);

                error = izo_cleanup_fset(fset);
                if (error)
                        break;
        }
        return error;
}

static struct vfsmount *izo_alloc_vfsmnt(void)
{
        struct vfsmount *mnt;
        PRESTO_ALLOC(mnt, sizeof(*mnt));
        if (mnt) {
                memset(mnt, 0, sizeof(struct vfsmount));
                atomic_set(&mnt->mnt_count,1);
                INIT_LIST_HEAD(&mnt->mnt_hash);
                INIT_LIST_HEAD(&mnt->mnt_child);
                INIT_LIST_HEAD(&mnt->mnt_mounts);
                INIT_LIST_HEAD(&mnt->mnt_list);
        }
        return mnt;
}


static void izo_setup_ctxt(struct dentry *root, struct vfsmount *mnt,
                           struct run_ctxt *save) 
{
        struct run_ctxt new;

        mnt->mnt_root = root;
        mnt->mnt_sb = root->d_inode->i_sb;
        unlock_super(mnt->mnt_sb);

        new.rootmnt = mnt;
        new.root = root;
        new.pwdmnt = mnt;
        new.pwd = root;
        new.fsuid = 0;
        new.fsgid = 0;
        new.fs = get_fs(); 
        /* XXX where can we get the groups from? */
        new.ngroups = 0;

        push_ctxt(save, &new); 
}

static void izo_cleanup_ctxt(struct vfsmount *mnt, struct run_ctxt *save) 
{
        lock_super(mnt->mnt_sb);
        pop_ctxt(save); 
}

static int izo_simple_mkdir(struct dentry *dir, char *name, int mode)
{
        struct dentry *dchild; 
        int err;
        ENTRY;
        
        dchild = lookup_one_len(name, dir, strlen(name));
        if (IS_ERR(dchild)) { 
                EXIT;
                return PTR_ERR(dchild); 
        }

        if (dchild->d_inode) { 
                dput(dchild);
                EXIT;
                return -EEXIST;
        }

        err = vfs_mkdir(dir->d_inode, dchild, mode);
        dput(dchild);
        
        EXIT;
        return err;
}

static int izo_simple_symlink(struct dentry *dir, char *name, char *tgt)
{
        struct dentry *dchild; 
        int err;
        ENTRY;
        
        dchild = lookup_one_len(name, dir, strlen(name));
        if (IS_ERR(dchild)) { 
                EXIT;
                return PTR_ERR(dchild); 
        }

        if (dchild->d_inode) { 
                dput(dchild);
                EXIT;
                return -EEXIST;
        }

        err = vfs_symlink(dir->d_inode, dchild, tgt);
        dput(dchild);
        
        EXIT;
        return err;
}

/*
 * run set_fsetroot in chroot environment
 */
int presto_set_fsetroot_from_ioc(struct dentry *root, char *fsetname,
                                 unsigned int flags)
{
        int rc;
        struct presto_cache *cache;
        struct vfsmount *mnt;
        struct run_ctxt save;

        if (root != root->d_inode->i_sb->s_root) {
                CERROR ("IOC_SET_FSET must be called on mount point\n");
                return -ENODEV;
        }

        cache = presto_get_cache(root->d_inode);
        mnt = cache->cache_vfsmount;
        if (!mnt) { 
                EXIT;
                return -ENOMEM;
        }
        
        izo_setup_ctxt(root, mnt, &save); 
        rc = presto_set_fsetroot(root, fsetname, flags);
        izo_cleanup_ctxt(mnt, &save);
        return rc;
}

/* XXX: this function should detect if fsetname is already in use for
   the cache under root
*/ 
int izo_prepare_fileset(struct dentry *root, char *fsetname) 
{
        int err;
        struct dentry *dotizo = NULL, *fsetdir = NULL, *dotiopen = NULL; 
        struct presto_cache *cache;
        struct vfsmount *mnt;
        struct run_ctxt save;

        cache = presto_get_cache(root->d_inode);
        mnt = cache->cache_vfsmount = izo_alloc_vfsmnt();
        if (!mnt) { 
                EXIT;
                return -ENOMEM;
        }
        
        if (!fsetname) 
                fsetname = "rootfset"; 

        izo_setup_ctxt(root, mnt, &save); 

        err = izo_simple_mkdir(root, ".intermezzo", 0755);
        CDEBUG(D_CACHE, "mkdir on .intermezzo err %d\n", err); 

        err = izo_simple_mkdir(root, "..iopen..", 0755);
        CDEBUG(D_CACHE, "mkdir on ..iopen.. err %d\n", err); 

        dotiopen = lookup_one_len("..iopen..", root, strlen("..iopen.."));
        if (IS_ERR(dotiopen)) { 
                EXIT;
                goto out;
        }
        dotiopen->d_inode->i_op = &presto_dir_iops;
        dput(dotiopen);


        dotizo = lookup_one_len(".intermezzo", root, strlen(".intermezzo"));
        if (IS_ERR(dotizo)) { 
                EXIT;
                goto out;
        }


        err = izo_simple_mkdir(dotizo, fsetname, 0755);
        CDEBUG(D_CACHE, "mkdir err %d\n", err); 

        /* XXX find the dentry of the root of the fileset (root for now) */ 
        fsetdir = lookup_one_len(fsetname, dotizo, strlen(fsetname));
        if (IS_ERR(fsetdir)) { 
                EXIT;
                goto out;
        }

        err = izo_simple_symlink(fsetdir, "ROOT", "../.."); 

        /* XXX read flags from flags file */ 
        err =  presto_set_fsetroot(root, fsetname, 0); 
        CDEBUG(D_CACHE, "set_fsetroot err %d\n", err); 

 out:
        if (dotizo && !IS_ERR(dotizo)) 
                dput(dotizo); 
        if (fsetdir && !IS_ERR(fsetdir)) 
                dput(fsetdir); 
        izo_cleanup_ctxt(mnt, &save);
        return err; 
}

int izo_set_fileid(struct file *dir, struct izo_ioctl_data *data)
{
        int rc = 0;
        struct presto_cache *cache;
        struct vfsmount *mnt;
        struct run_ctxt save;
        struct nameidata nd;
        struct dentry *dentry;
        struct presto_dentry_data *dd;
        struct dentry *root;
        char *buf = NULL; 

        ENTRY;


        root = dir->f_dentry;

        /* actually, needs to be called on ROOT of fset, not mount point  
        if (root != root->d_inode->i_sb->s_root) {
                CERROR ("IOC_SET_FSET must be called on mount point\n");
                return -ENODEV;
        }
        */

        cache = presto_get_cache(root->d_inode);
        mnt = cache->cache_vfsmount;
        if (!mnt) { 
                EXIT;
                return -ENOMEM;
        }
        
        izo_setup_ctxt(root, mnt, &save); 
        
        PRESTO_ALLOC(buf, data->ioc_plen1);
        if (!buf) { 
                rc = -ENOMEM;
                EXIT;
                goto out;
        }
        if (copy_from_user(buf, data->ioc_pbuf1, data->ioc_plen1)) { 
                rc =  -EFAULT;
                EXIT;
                goto out;
        }

        rc = presto_walk(buf, &nd);
        if (rc) {
                CERROR("Unable to open: %s\n", buf);
                EXIT;
                goto out;
        }
        dentry = nd.dentry;
        if (!dentry) {
                CERROR("no dentry!\n");
                rc =  -EINVAL;
                EXIT;
                goto out_close;
        }
        dd = presto_d2d(dentry);
        if (!dd) {
                CERROR("no dentry_data!\n");
                rc = -EINVAL;
                EXIT;
                goto out_close;
        }

        CDEBUG(D_FILE,"de:%p dd:%p\n", dentry, dd);

        if (dd->remote_ino != 0) {
                CERROR("remote_ino already set? %Lx:%Lx\n", dd->remote_ino,
                       dd->remote_generation);
                rc = 0;
                EXIT;
                goto out_close;
        }


        CDEBUG(D_FILE,"setting %p %p, %s to %Lx:%Lx\n", dentry, dd, 
               buf, data->ioc_ino,
               data->ioc_generation);
        dd->remote_ino = data->ioc_ino;
        dd->remote_generation = data->ioc_generation;

        EXIT;
 out_close:
        path_release(&nd);
 out:
        if (buf)
                PRESTO_FREE(buf, data->ioc_plen1);
        izo_cleanup_ctxt(mnt, &save);
        return rc;
}
