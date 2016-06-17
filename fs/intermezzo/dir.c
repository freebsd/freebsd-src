/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (C) 2000 Stelias Computing, Inc.
 *  Copyright (C) 2000 Red Hat, Inc.
 *  Copyright (C) 2000 Tacitus Systems
 *  Copyright (C) 2000 Peter J. Braam
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

#include <stdarg.h>

#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/smp_lock.h>

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

#include <linux/intermezzo_fs.h>
#include <linux/intermezzo_psdev.h>

static inline void presto_relock_sem(struct inode *dir) 
{
        /* the lock from sys_mkdir / lookup_create */
        down(&dir->i_sem);
        /* the rest is done by the do_{create,mkdir, ...} */
}

static inline void presto_relock_other(struct inode *dir) 
{
        /* vfs_mkdir locks */
        down(&dir->i_zombie);
        lock_kernel(); 
}

static inline void presto_fulllock(struct inode *dir) 
{
        /* the lock from sys_mkdir / lookup_create */
        down(&dir->i_sem);
        /* vfs_mkdir locks */
        down(&dir->i_zombie);
        lock_kernel(); 
}

static inline void presto_unlock(struct inode *dir) 
{
        /* vfs_mkdir locks */
        unlock_kernel(); 
        up(&dir->i_zombie);
        /* the lock from sys_mkdir / lookup_create */
        up(&dir->i_sem);
}


/*
 * these are initialized in super.c
 */
extern int presto_permission(struct inode *inode, int mask);
static int izo_authorized_uid = 0;

int izo_dentry_is_ilookup(struct dentry *dentry, ino_t *id,
                          unsigned int *generation)
{
        char tmpname[64];
        char *next;

        ENTRY;
        /* prefix is 7 characters: '...ino:' */
        if ( dentry->d_name.len < 7 || dentry->d_name.len > 64 ||
             memcmp(dentry->d_name.name, PRESTO_ILOOKUP_MAGIC, 7) != 0 ) {
                EXIT;
                return 0;
        }

        memcpy(tmpname, dentry->d_name.name + 7, dentry->d_name.len - 7);
        *(tmpname + dentry->d_name.len - 7) = '\0';

        /* name is of the form ...ino:<inode number>:<generation> */
        *id = simple_strtoul(tmpname, &next, 16);
        if ( *next == PRESTO_ILOOKUP_SEP ) {
                *generation = simple_strtoul(next + 1, 0, 16);
                CDEBUG(D_INODE, "ino string: %s, Id = %lx (%lu), "
                       "generation %x (%d)\n",
                       tmpname, *id, *id, *generation, *generation);
                EXIT;
                return 1;
        } else {
                EXIT;
                return 0;
        }
}

struct dentry *presto_tmpfs_ilookup(struct inode *dir, 
                                    struct dentry *dentry,
                                    ino_t ino, 
                                    unsigned int generation)
{
        return dentry; 
}


inline int presto_can_ilookup(void)
{
        return (current->euid == izo_authorized_uid ||
                capable(CAP_DAC_READ_SEARCH));
}

struct dentry *presto_iget_ilookup(struct inode *dir, 
                                          struct dentry *dentry,
                                          ino_t ino, 
                                          unsigned int generation)
{
        struct inode *inode;
        int error;

        ENTRY;

        if ( !presto_can_ilookup() ) {
                CERROR("ilookup denied: euid %u, authorized_uid %u\n",
                       current->euid, izo_authorized_uid);
                return ERR_PTR(-EPERM);
        }
        error = -ENOENT;
        inode = iget(dir->i_sb, ino);
        if (!inode) { 
                CERROR("fatal: NULL inode ino %lu\n", ino); 
                goto cleanup_iput;
        }
        if (is_bad_inode(inode) || inode->i_nlink == 0) {
                CERROR("fatal: bad inode ino %lu, links %d\n", ino, inode->i_nlink); 
                goto cleanup_iput;
        }
        if (inode->i_generation != generation) {
                CERROR("fatal: bad generation %u (want %u)\n",
                       inode->i_generation, generation);
                goto cleanup_iput;
        }

        d_instantiate(dentry, inode);
        dentry->d_flags |= DCACHE_NFSD_DISCONNECTED; /* NFS hack */

        EXIT;
        return NULL;

cleanup_iput:
        if (inode)
                iput(inode);
        return ERR_PTR(error);
}

struct dentry *presto_add_ilookup_dentry(struct dentry *parent,
                                         struct dentry *real)
{
        struct inode *inode = real->d_inode;
        struct dentry *de;
        char buf[32];
        char *ptr = buf;
        struct dentry *inodir;
        struct presto_dentry_data *dd;

        inodir = lookup_one_len("..iopen..", parent,  strlen("..iopen..")); 
        if (!inodir || IS_ERR(inodir) || !inodir->d_inode ) { 
                CERROR("%s: bad ..iopen.. lookup\n", __FUNCTION__); 
                return NULL; 
        }
        inodir->d_inode->i_op = &presto_dir_iops;

        snprintf(ptr, 32, "...ino:%lx:%x", inode->i_ino, inode->i_generation);

        de = lookup_one_len(ptr, inodir,  strlen(ptr)); 
        if (!de || IS_ERR(de)) {
                CERROR("%s: bad ...ino lookup %ld\n", 
                       __FUNCTION__, PTR_ERR(de)); 
                dput(inodir);
                return NULL; 
        }

        dd = presto_d2d(real);
        if (!dd) 
                BUG();

        /* already exists */
        if (de->d_inode)
                BUG();
#if 0 
                if (de->d_inode != inode ) { 
                        CERROR("XX de->d_inode %ld, inode %ld\n", 
                               de->d_inode->i_ino, inode->i_ino); 
                        BUG();
                }
                if (dd->dd_inodentry) { 
                        CERROR("inodentry exists %ld \n", inode->i_ino);
                        BUG();
                }
                dput(inodir);
                return de;
        }
#endif 

        if (presto_d2d(de)) 
                BUG();

        atomic_inc(&inode->i_count);
        de->d_op = &presto_dentry_ops;
        d_add(de, inode);
        if (!de->d_op)
                CERROR("DD: no ops dentry %p, dd %p\n", de, dd);
        dd->dd_inodentry = de;
        dd->dd_count++;
        de->d_fsdata = dd;

        dput(inodir);
        return de;
}

struct dentry *presto_lookup(struct inode * dir, struct dentry *dentry)
{
        int rc = 0;
        struct dentry *de;
        struct presto_cache *cache;
        int minor;
        ino_t ino;
        unsigned int generation;
        struct inode_operations *iops;
        int is_ilookup = 0;

        ENTRY;
        cache = presto_get_cache(dir);
        if (cache == NULL) {
                CERROR("InterMezzo BUG: no cache in presto_lookup "
                       "(dir ino: %ld)!\n", dir->i_ino);
                EXIT;
                return NULL;
        }
        minor = presto_c2m(cache);

        iops = filter_c2cdiops(cache->cache_filter);
        if (!iops || !iops->lookup) {
                CERROR("InterMezzo BUG: filesystem has no lookup\n");
                EXIT;
                return NULL;
        }


        CDEBUG(D_CACHE, "dentry %p, dir ino: %ld, name: %*s, islento: %d\n",
               dentry, dir->i_ino, dentry->d_name.len, dentry->d_name.name,
               ISLENTO(minor));

        if (dentry->d_fsdata)
                CERROR("DD -- BAD dentry %p has data\n", dentry);
                       
        dentry->d_fsdata = NULL;
#if 0
        if (ext2_check_for_iopen(dir, dentry))
                de = NULL;
        else {
#endif
                if ( izo_dentry_is_ilookup(dentry, &ino, &generation) ) { 
                        de = cache->cache_filter->o_trops->tr_ilookup
                                (dir, dentry, ino, generation);
                        is_ilookup = 1;
                } else
                        de = iops->lookup(dir, dentry);
#if 0
        }
#endif

        if ( IS_ERR(de) ) {
                CERROR("dentry lookup error %ld\n", PTR_ERR(de));
                return de;
        }

        /* some file systems have no read_inode: set methods here */
        if (dentry->d_inode)
                presto_set_ops(dentry->d_inode, cache->cache_filter);

        filter_setup_dentry_ops(cache->cache_filter,
                                dentry->d_op, &presto_dentry_ops);
        dentry->d_op = filter_c2udops(cache->cache_filter);

        /* In lookup we will tolerate EROFS return codes from presto_set_dd
         * to placate NFS. EROFS indicates that a fileset was not found but
         * we should still be able to continue through a lookup.
         * Anything else is a hard error and must be returned to VFS. */
        if (!is_ilookup)
                rc = presto_set_dd(dentry);
        if (rc && rc != -EROFS) {
                CERROR("presto_set_dd failed (dir %ld, name %*s): %d\n",
                       dir->i_ino, dentry->d_name.len, dentry->d_name.name, rc);
                return ERR_PTR(rc);
        }

        EXIT;
        return NULL;
}

static inline int presto_check_set_fsdata (struct dentry *de)
{
        if (presto_d2d(de) == NULL) {
#ifdef PRESTO_NO_NFS
                CERROR("dentry without fsdata: %p: %*s\n", de, 
                                de->d_name.len, de->d_name.name);
                BUG();
#endif
                return presto_set_dd (de);
        }

        return 0;
}

int presto_setattr(struct dentry *de, struct iattr *iattr)
{
        int error;
        struct presto_cache *cache;
        struct presto_file_set *fset;
        struct lento_vfs_context info = { 0, 0, 0 };

        ENTRY;

        error = presto_prep(de, &cache, &fset);
        if ( error ) {
                EXIT;
                return error;
        }

        if (!iattr->ia_valid)
                CDEBUG(D_INODE, "presto_setattr: iattr is not valid\n");

        CDEBUG(D_INODE, "valid %#x, mode %#o, uid %u, gid %u, size %Lu, "
               "atime %lu mtime %lu ctime %lu flags %d\n",
               iattr->ia_valid, iattr->ia_mode, iattr->ia_uid, iattr->ia_gid,
               iattr->ia_size, iattr->ia_atime, iattr->ia_mtime,
               iattr->ia_ctime, iattr->ia_attr_flags);
        
        if ( presto_get_permit(de->d_inode) < 0 ) {
                EXIT;
                return -EROFS;
        }

        if (!ISLENTO(presto_c2m(cache)))
                info.flags = LENTO_FL_KML;
        info.flags |= LENTO_FL_IGNORE_TIME;
        error = presto_do_setattr(fset, de, iattr, &info);
        presto_put_permit(de->d_inode);
        return error;
}

/*
 *  Now the meat: the fs operations that require journaling
 *
 *
 *  XXX: some of these need modifications for hierarchical filesets
 */

int presto_prep(struct dentry *dentry, struct presto_cache **cache,
                struct presto_file_set **fset)
{       
        int rc;

        /* NFS might pass us dentries which have not gone through lookup.
         * Test and set d_fsdata for such dentries
         */
        rc = presto_check_set_fsdata (dentry);
        if (rc) return rc;

        *fset = presto_fset(dentry);
        if ( *fset == NULL ) {
                CERROR("No file set for dentry at %p: %*s\n", dentry,
                                dentry->d_name.len, dentry->d_name.name);
                return -EROFS;
        }

        *cache = (*fset)->fset_cache;
        if ( *cache == NULL ) {
                CERROR("PRESTO: BAD, BAD: cannot find cache\n");
                return -EBADF;
        }

        CDEBUG(D_PIOCTL, "---> cache flags %x, fset flags %x\n",
              (*cache)->cache_flags, (*fset)->fset_flags);
        if( presto_is_read_only(*fset) ) {
                CERROR("PRESTO: cannot modify read-only fileset, minor %d.\n",
                       presto_c2m(*cache));
                return -EROFS;
        }
        return 0;
}

static int presto_create(struct inode * dir, struct dentry * dentry, int mode)
{
        int error;
        struct presto_cache *cache;
        struct dentry *parent = dentry->d_parent;
        struct lento_vfs_context info;
        struct presto_file_set *fset;

        ENTRY;
        error = presto_check_set_fsdata(dentry);
        if ( error ) {
                EXIT;
                return error;
        }

        error = presto_prep(dentry->d_parent, &cache, &fset);
        if ( error ) {
                EXIT;
                return error;
        }
        presto_unlock(dir);

        /* Does blocking and non-blocking behavious need to be 
           checked for.  Without blocking (return 1), the permit
           was acquired without reintegration
        */
        if ( presto_get_permit(dir) < 0 ) {
                EXIT;
                presto_fulllock(dir);
                return -EROFS;
        }

        presto_relock_sem(dir);
        parent = dentry->d_parent; 
        memset(&info, 0, sizeof(info));
        if (!ISLENTO(presto_c2m(cache)))
                info.flags = LENTO_FL_KML;
        info.flags |= LENTO_FL_IGNORE_TIME;
        error = presto_do_create(fset, parent, dentry, mode, &info);

        presto_relock_other(dir);
        presto_put_permit(dir);
        EXIT;
        return error;
}

static int presto_link(struct dentry *old_dentry, struct inode *dir,
                struct dentry *new_dentry)
{
        int error;
        struct presto_cache *cache, *new_cache;
        struct presto_file_set *fset, *new_fset;
        struct dentry *parent = new_dentry->d_parent;
        struct lento_vfs_context info;

        ENTRY;
        error = presto_prep(old_dentry, &cache, &fset);
        if ( error ) {
                EXIT;
                return error;
        }

        error = presto_check_set_fsdata(new_dentry);
        if ( error ) {
                EXIT;
                return error;
        }

        error = presto_prep(new_dentry->d_parent, &new_cache, &new_fset);
        if ( error ) {
                EXIT;
                return error;
        }

        if (fset != new_fset) { 
                EXIT;
                return -EXDEV;
        }

        presto_unlock(dir);
        if ( presto_get_permit(old_dentry->d_inode) < 0 ) {
                EXIT;
                presto_fulllock(dir);
                return -EROFS;
        }

        if ( presto_get_permit(dir) < 0 ) {
                EXIT;
                presto_fulllock(dir);
                return -EROFS;
        }

        presto_relock_sem(dir);
        parent = new_dentry->d_parent;

        memset(&info, 0, sizeof(info));
        if (!ISLENTO(presto_c2m(cache)))
                info.flags = LENTO_FL_KML;
        info.flags |= LENTO_FL_IGNORE_TIME;
        error = presto_do_link(fset, old_dentry, parent,
                               new_dentry, &info);

#if 0
        /* XXX for links this is not right */
        if (cache->cache_filter->o_trops->tr_add_ilookup ) { 
                struct dentry *d;
                d = cache->cache_filter->o_trops->tr_add_ilookup
                        (dir->i_sb->s_root, new_dentry, 1); 
        }
#endif 

        presto_relock_other(dir);
        presto_put_permit(dir);
        presto_put_permit(old_dentry->d_inode);
        return error;
}

static int presto_mkdir(struct inode * dir, struct dentry * dentry, int mode)
{
        int error;
        struct presto_file_set *fset;
        struct presto_cache *cache;
        struct dentry *parent = dentry->d_parent;
        struct lento_vfs_context info;

        ENTRY;

        error = presto_check_set_fsdata(dentry);
        if ( error  ) {
                EXIT;
                return error;
        }

        error = presto_prep(dentry->d_parent, &cache, &fset);
        if ( error  ) {
                EXIT;
                return error;
        }

        presto_unlock(dir); 

        if ( presto_get_permit(dir) < 0 ) {
                EXIT;
                presto_fulllock(dir);
                return -EROFS;
        }

        memset(&info, 0, sizeof(info));
        if (!ISLENTO(presto_c2m(cache)))
                info.flags = LENTO_FL_KML;
        info.flags |= LENTO_FL_IGNORE_TIME;

        presto_relock_sem(dir); 
        parent = dentry->d_parent;
        error = presto_do_mkdir(fset, parent, dentry, mode, &info);
        presto_relock_other(dir); 
        presto_put_permit(dir);
        return error;
}



static int presto_symlink(struct inode *dir, struct dentry *dentry,
                   const char *name)
{
        int error;
        struct presto_cache *cache;
        struct presto_file_set *fset;
        struct dentry *parent = dentry->d_parent;
        struct lento_vfs_context info;

        ENTRY;
        error = presto_check_set_fsdata(dentry);
        if ( error ) {
                EXIT;
                return error;
        }

        error = presto_prep(dentry->d_parent, &cache, &fset);
        if ( error ) {
                EXIT;
                return error;
        }

        presto_unlock(dir);
        if ( presto_get_permit(dir) < 0 ) {
                EXIT;
                presto_fulllock(dir);
                return -EROFS;
        }

        presto_relock_sem(dir);
        parent = dentry->d_parent;
        memset(&info, 0, sizeof(info));
        if (!ISLENTO(presto_c2m(cache)))
                info.flags = LENTO_FL_KML;
        info.flags |= LENTO_FL_IGNORE_TIME;
        error = presto_do_symlink(fset, parent, dentry, name, &info);
        presto_relock_other(dir);
        presto_put_permit(dir);
        return error;
}

int presto_unlink(struct inode *dir, struct dentry *dentry)
{
        int error;
        struct presto_cache *cache;
        struct presto_file_set *fset;
        struct dentry *parent = dentry->d_parent;
        struct lento_vfs_context info;

        ENTRY;
        error = presto_check_set_fsdata(dentry);
        if ( error ) {
                EXIT;
                return error;
        }

        error = presto_prep(dentry->d_parent, &cache, &fset);
        if ( error  ) {
                EXIT;
                return error;
        }

        presto_unlock(dir);
        if ( presto_get_permit(dir) < 0 ) {
                EXIT;
                presto_fulllock(dir);
                return -EROFS;
        }

        presto_relock_sem(dir);
        parent = dentry->d_parent;
        memset(&info, 0, sizeof(info));
        if (!ISLENTO(presto_c2m(cache)))
                info.flags = LENTO_FL_KML;
        info.flags |= LENTO_FL_IGNORE_TIME;

        error = presto_do_unlink(fset, parent, dentry, &info);

        presto_relock_other(dir);
        presto_put_permit(dir);
        return error;
}

static int presto_rmdir(struct inode *dir, struct dentry *dentry)
{
        int error;
        struct presto_cache *cache;
        struct presto_file_set *fset;
        struct dentry *parent = dentry->d_parent;
        struct lento_vfs_context info;

        ENTRY;
        CDEBUG(D_FILE, "prepping presto\n");
        error = presto_check_set_fsdata(dentry);

        if ( error ) {
                EXIT;
                return error;
        }

        error = presto_prep(dentry->d_parent, &cache, &fset);
        if ( error ) {
                EXIT;
                return error;
        }

        CDEBUG(D_FILE, "unlocking\n");
        /* We need to dget() before the dput in double_unlock, to ensure we
         * still have dentry references.  double_lock doesn't do dget for us.
         */
        unlock_kernel();
        if (d_unhashed(dentry))
                d_rehash(dentry);
        double_up(&dir->i_zombie, &dentry->d_inode->i_zombie);
        double_up(&dir->i_sem, &dentry->d_inode->i_sem);

        CDEBUG(D_FILE, "getting permit\n");
        if ( presto_get_permit(parent->d_inode) < 0 ) {
                EXIT;
                double_down(&dir->i_sem, &dentry->d_inode->i_sem);
                double_down(&dir->i_zombie, &dentry->d_inode->i_zombie);
                
                lock_kernel();
                return -EROFS;
        }
        CDEBUG(D_FILE, "locking\n");

        double_down(&dir->i_sem, &dentry->d_inode->i_sem);
        parent = dentry->d_parent;
        memset(&info, 0, sizeof(info));
        if (!ISLENTO(presto_c2m(cache)))
                info.flags = LENTO_FL_KML;
        info.flags |= LENTO_FL_IGNORE_TIME;
        error = presto_do_rmdir(fset, parent, dentry, &info);
        presto_put_permit(parent->d_inode);
        lock_kernel();
        EXIT;
        return error;
}

static int presto_mknod(struct inode * dir, struct dentry * dentry, int mode, int rdev)
{
        int error;
        struct presto_cache *cache;
        struct presto_file_set *fset;
        struct dentry *parent = dentry->d_parent;
        struct lento_vfs_context info;

        ENTRY;
        error = presto_check_set_fsdata(dentry);
        if ( error ) {
                EXIT;
                return error;
        }

        error = presto_prep(dentry->d_parent, &cache, &fset);
        if ( error  ) {
                EXIT;
                return error;
        }

        presto_unlock(dir);
        if ( presto_get_permit(dir) < 0 ) {
                EXIT;
                presto_fulllock(dir);
                return -EROFS;
        }
        
        presto_relock_sem(dir);
        parent = dentry->d_parent;
        memset(&info, 0, sizeof(info));
        if (!ISLENTO(presto_c2m(cache)))
                info.flags = LENTO_FL_KML;
        info.flags |= LENTO_FL_IGNORE_TIME;
        error = presto_do_mknod(fset, parent, dentry, mode, rdev, &info);
        presto_relock_other(dir);
        presto_put_permit(dir);
        EXIT;
        return error;
}

inline void presto_triple_unlock(struct inode *old_dir, struct inode *new_dir, 
                                 struct dentry *old_dentry, 
                                 struct dentry *new_dentry, int triple)
{
        /* rename_dir case */ 
        if (S_ISDIR(old_dentry->d_inode->i_mode)) { 
                if (triple) {                   
                        triple_up(&old_dir->i_zombie,
                                  &new_dir->i_zombie,
                                  &new_dentry->d_inode->i_zombie);
                } else { 
                        double_up(&old_dir->i_zombie,
                                  &new_dir->i_zombie);
                }
                up(&old_dir->i_sb->s_vfs_rename_sem);
        } else /* this case is rename_other */
                double_up(&old_dir->i_zombie, &new_dir->i_zombie);
        /* done by do_rename */
        unlock_kernel();
        double_up(&old_dir->i_sem, &new_dir->i_sem);
}

inline void presto_triple_fulllock(struct inode *old_dir, 
                                   struct inode *new_dir, 
                                   struct dentry *old_dentry, 
                                   struct dentry *new_dentry, int triple)
{
        /* done by do_rename */
        double_down(&old_dir->i_sem, &new_dir->i_sem);
        lock_kernel();
        /* rename_dir case */ 
        if (S_ISDIR(old_dentry->d_inode->i_mode)) { 
                down(&old_dir->i_sb->s_vfs_rename_sem);
                if (triple) {                   
                        triple_down(&old_dir->i_zombie,
                                  &new_dir->i_zombie,
                                  &new_dentry->d_inode->i_zombie);
                } else { 
                        double_down(&old_dir->i_zombie,
                                  &new_dir->i_zombie);
                }
        } else /* this case is rename_other */
                double_down(&old_dir->i_zombie, &new_dir->i_zombie);
}

inline void presto_triple_relock_sem(struct inode *old_dir, 
                                   struct inode *new_dir, 
                                   struct dentry *old_dentry, 
                                   struct dentry *new_dentry, int triple)
{
        /* done by do_rename */
        double_down(&old_dir->i_sem, &new_dir->i_sem);
        lock_kernel();
}

inline void presto_triple_relock_other(struct inode *old_dir, 
                                   struct inode *new_dir, 
                                   struct dentry *old_dentry, 
                                   struct dentry *new_dentry, int triple)
{
        /* rename_dir case */ 
        if (S_ISDIR(old_dentry->d_inode->i_mode)) { 
                down(&old_dir->i_sb->s_vfs_rename_sem);
                if (triple) {                   
                        triple_down(&old_dir->i_zombie,
                                  &new_dir->i_zombie,
                                  &new_dentry->d_inode->i_zombie);
                } else { 
                        double_down(&old_dir->i_zombie,
                                  &new_dir->i_zombie);
                }
        } else /* this case is rename_other */
                double_down(&old_dir->i_zombie, &new_dir->i_zombie);
}


// XXX this can be optimized: renamtes across filesets only require 
//     multiple KML records, but can locally be executed normally. 
int presto_rename(struct inode *old_dir, struct dentry *old_dentry,
                  struct inode *new_dir, struct dentry *new_dentry)
{
        int error;
        struct presto_cache *cache, *new_cache;
        struct presto_file_set *fset, *new_fset;
        struct lento_vfs_context info;
        struct dentry *old_parent = old_dentry->d_parent;
        struct dentry *new_parent = new_dentry->d_parent;
        int triple;

        ENTRY;
        error = presto_prep(old_dentry, &cache, &fset);
        if ( error ) {
                EXIT;
                return error;
        }
        error = presto_prep(new_parent, &new_cache, &new_fset);
        if ( error ) {
                EXIT;
                return error;
        }

        if ( fset != new_fset ) {
                EXIT;
                return -EXDEV;
        }

        /* We need to do dget before the dput in double_unlock, to ensure we
         * still have dentry references.  double_lock doesn't do dget for us.
         */

        triple = (S_ISDIR(old_dentry->d_inode->i_mode) && new_dentry->d_inode)?
                1:0;

        presto_triple_unlock(old_dir, new_dir, old_dentry, new_dentry, triple); 

        if ( presto_get_permit(old_dir) < 0 ) {
                EXIT;
                presto_triple_fulllock(old_dir, new_dir, old_dentry, new_dentry, triple); 
                return -EROFS;
        }
        if ( presto_get_permit(new_dir) < 0 ) {
                EXIT;
                presto_triple_fulllock(old_dir, new_dir, old_dentry, new_dentry, triple); 
                return -EROFS;
        }

        presto_triple_relock_sem(old_dir, new_dir, old_dentry, new_dentry, triple); 
        memset(&info, 0, sizeof(info));
        if (!ISLENTO(presto_c2m(cache)))
                info.flags = LENTO_FL_KML;
        info.flags |= LENTO_FL_IGNORE_TIME;
        error = do_rename(fset, old_parent, old_dentry, new_parent,
                          new_dentry, &info);
        presto_triple_relock_other(old_dir, new_dir, old_dentry, new_dentry, triple); 

        presto_put_permit(new_dir);
        presto_put_permit(old_dir);
        return error;
}

/* basically this allows the ilookup processes access to all files for
 * reading, while not making ilookup totally insecure.  This could all
 * go away if we could set the CAP_DAC_READ_SEARCH capability for the client.
 */
/* If posix acls are available, the underlying cache fs will export the
 * appropriate permission function. Thus we do not worry here about ACLs
 * or EAs. -SHP
 */
int presto_permission(struct inode *inode, int mask)
{
        unsigned short mode = inode->i_mode;
        struct presto_cache *cache;
        int rc;

        ENTRY;
        if ( presto_can_ilookup() && !(mask & S_IWOTH)) {
                CDEBUG(D_CACHE, "ilookup on %ld OK\n", inode->i_ino);
                EXIT;
                return 0;
        }

        cache = presto_get_cache(inode);

        if ( cache ) {
                /* we only override the file/dir permission operations */
                struct inode_operations *fiops = filter_c2cfiops(cache->cache_filter);
                struct inode_operations *diops = filter_c2cdiops(cache->cache_filter);

                if ( S_ISREG(mode) && fiops && fiops->permission ) {
                        EXIT;
                        return fiops->permission(inode, mask);
                }
                if ( S_ISDIR(mode) && diops && diops->permission ) {
                        EXIT;
                        return diops->permission(inode, mask);
                }
        }

        /* The cache filesystem doesn't have its own permission function,
         * but we don't want to duplicate the VFS code here.  In order
         * to avoid looping from permission calling this function again,
         * we temporarily override the permission operation while we call
         * the VFS permission function.
         */
        inode->i_op->permission = NULL;
        rc = permission(inode, mask);
        inode->i_op->permission = &presto_permission;

        EXIT;
        return rc;
}


int presto_ioctl(struct inode *inode, struct file *file,
                        unsigned int cmd, unsigned long arg)
{
        char buf[1024];
        struct izo_ioctl_data *data = NULL;
        struct presto_dentry_data *dd;
        int rc;

        ENTRY;

        /* Try the filesystem's ioctl first, and return if it succeeded. */
        dd = presto_d2d(file->f_dentry); 
        if (dd && dd->dd_fset) { 
                int (*cache_ioctl)(struct inode *, struct file *, unsigned int, unsigned long ) = filter_c2cdfops(dd->dd_fset->fset_cache->cache_filter)->ioctl;
                rc = -ENOTTY;
                if (cache_ioctl)
                        rc = cache_ioctl(inode, file, cmd, arg);
                if (rc != -ENOTTY) {
                        EXIT;
                        return rc;
                }
        }

        if (current->euid != 0 && current->euid != izo_authorized_uid) {
                EXIT;
                return -EPERM;
        }

        memset(buf, 0, sizeof(buf));
        
        if (izo_ioctl_getdata(buf, buf + 1024, (void *)arg)) { 
                CERROR("intermezzo ioctl: data error\n");
                return -EINVAL;
        }
        data = (struct izo_ioctl_data *)buf;
        
        switch(cmd) {
        case IZO_IOC_REINTKML: { 
                int rc;
                int cperr;
                rc = kml_reint_rec(file, data);

                EXIT;
                cperr = copy_to_user((char *)arg, data, sizeof(*data));
                if (cperr) { 
                        CERROR("WARNING: cperr %d\n", cperr); 
                        rc = -EFAULT;
                }
                return rc;
        }

        case IZO_IOC_GET_RCVD: {
                struct izo_rcvd_rec rec;
                struct presto_file_set *fset;
                int rc;

                fset = presto_fset(file->f_dentry);
                if (fset == NULL) {
                        EXIT;
                        return -ENODEV;
                }
                rc = izo_rcvd_get(&rec, fset, data->ioc_uuid);
                if (rc < 0) {
                        EXIT;
                        return rc;
                }

                EXIT;
                return copy_to_user((char *)arg, &rec, sizeof(rec))? -EFAULT : 0;
        }

        case IZO_IOC_REPSTATUS: {
                __u64 client_kmlsize;
                struct izo_rcvd_rec *lr_client;
                struct izo_rcvd_rec rec;
                struct presto_file_set *fset;
                int minor;
                int rc;

                fset = presto_fset(file->f_dentry);
                if (fset == NULL) {
                        EXIT;
                        return -ENODEV;
                }
                minor = presto_f2m(fset);

                client_kmlsize = data->ioc_kmlsize;
                lr_client =  (struct izo_rcvd_rec *) data->ioc_pbuf1;

                rc = izo_repstatus(fset, client_kmlsize, 
                                       lr_client, &rec);
                if (rc < 0) {
                        EXIT;
                        return rc;
                }

                EXIT;
                return copy_to_user((char *)arg, &rec, sizeof(rec))? -EFAULT : 0;
        }

        case IZO_IOC_GET_CHANNEL: {
                struct presto_file_set *fset;

                fset = presto_fset(file->f_dentry);
                if (fset == NULL) {
                        EXIT;
                        return -ENODEV;
                }
                
                data->ioc_dev = fset->fset_cache->cache_psdev->uc_minor;
                CDEBUG(D_PSDEV, "CHANNEL %d\n", data->ioc_dev); 
                EXIT;
                return copy_to_user((char *)arg, data, sizeof(*data))? -EFAULT : 0;
        }

        case IZO_IOC_SET_IOCTL_UID:
                izo_authorized_uid = data->ioc_uid;
                EXIT;
                return 0;

        case IZO_IOC_SET_PID:
                rc = izo_psdev_setpid(data->ioc_dev);
                EXIT;
                return rc;

        case IZO_IOC_SET_CHANNEL:
                rc = izo_psdev_setchannel(file, data->ioc_dev);
                EXIT;
                return rc;

        case IZO_IOC_GET_KML_SIZE: {
                struct presto_file_set *fset;
                __u64 kmlsize;

                fset = presto_fset(file->f_dentry);
                if (fset == NULL) {
                        EXIT;
                        return -ENODEV;
                }

                kmlsize = presto_kml_offset(fset) + fset->fset_kml_logical_off;

                EXIT;
                return copy_to_user((char *)arg, &kmlsize, sizeof(kmlsize))?-EFAULT : 0;
        }

        case IZO_IOC_PURGE_FILE_DATA: {
                struct presto_file_set *fset;

                fset = presto_fset(file->f_dentry);
                if (fset == NULL) {
                        EXIT;
                        return -ENODEV;
                }

                rc = izo_purge_file(fset, data->ioc_inlbuf1);
                EXIT;
                return rc;
        }

        case IZO_IOC_GET_FILEID: {
                rc = izo_get_fileid(file, data);
                EXIT;
                if (rc)
                        return rc;
                return copy_to_user((char *)arg, data, sizeof(*data))? -EFAULT : 0;
        }

        case IZO_IOC_SET_FILEID: {
                rc = izo_set_fileid(file, data);
                EXIT;
                if (rc)
                        return rc;
                return copy_to_user((char *)arg, data, sizeof(*data))? -EFAULT  : 0;
        }

        case IZO_IOC_ADJUST_LML: { 
                struct lento_vfs_context *info; 
                info = (struct lento_vfs_context *)data->ioc_inlbuf1;
                rc = presto_adjust_lml(file, info); 
                EXIT;
                return rc;
        }

        case IZO_IOC_CONNECT: {
                struct presto_file_set *fset;
                int minor;

                fset = presto_fset(file->f_dentry);
                if (fset == NULL) {
                        EXIT;
                        return -ENODEV;
                }
                minor = presto_f2m(fset);

                rc = izo_upc_connect(minor, data->ioc_ino,
                                     data->ioc_generation, data->ioc_uuid,
                                     data->ioc_flags);
                EXIT;
                return rc;
        }

        case IZO_IOC_GO_FETCH_KML: {
                struct presto_file_set *fset;
                int minor;

                fset = presto_fset(file->f_dentry);
                if (fset == NULL) {
                        EXIT;
                        return -ENODEV;
                }
                minor = presto_f2m(fset);

                rc = izo_upc_go_fetch_kml(minor, fset->fset_name,
                                          data->ioc_uuid, data->ioc_kmlsize);
                EXIT;
                return rc;
        }

        case IZO_IOC_REVOKE_PERMIT:
                if (data->ioc_flags)
                        rc = izo_revoke_permit(file->f_dentry, data->ioc_uuid);
                else
                        rc = izo_revoke_permit(file->f_dentry, NULL);
                EXIT;
                return rc;

        case IZO_IOC_CLEAR_FSET:
                rc = izo_clear_fsetroot(file->f_dentry);
                EXIT;
                return rc;

        case IZO_IOC_CLEAR_ALL_FSETS: { 
                struct presto_file_set *fset;

                fset = presto_fset(file->f_dentry);
                if (fset == NULL) {
                        EXIT;
                        return -ENODEV;
                }

                rc = izo_clear_all_fsetroots(fset->fset_cache);
                EXIT;
                return rc;
        }

        case IZO_IOC_SET_FSET:
                /*
                 * Mark this dentry as being a fileset root.
                 */
                rc = presto_set_fsetroot_from_ioc(file->f_dentry, 
                                                  data->ioc_inlbuf1,
                                                  data->ioc_flags);
                EXIT;
                return rc;


        case IZO_IOC_MARK: {
                int res = 0;  /* resulting flags - returned to user */
                int error;

                CDEBUG(D_DOWNCALL, "mark inode: %ld, and: %x, or: %x, what %d\n",
                       file->f_dentry->d_inode->i_ino, data->ioc_and_flag,
                       data->ioc_or_flag, data->ioc_mark_what);

                switch (data->ioc_mark_what) {
                case MARK_DENTRY:               
                        error = izo_mark_dentry(file->f_dentry,
                                                   data->ioc_and_flag,
                                                   data->ioc_or_flag, &res);
                        break;
                case MARK_FSET:
                        error = izo_mark_fset(file->f_dentry,
                                                 data->ioc_and_flag,
                                                 data->ioc_or_flag, &res);
                        break;
                case MARK_CACHE:
                        error = izo_mark_cache(file->f_dentry,
                                                  data->ioc_and_flag,
                                                  data->ioc_or_flag, &res);
                        break;
                case MARK_GETFL: {
                        int fflags, cflags;
                        data->ioc_and_flag = 0xffffffff;
                        data->ioc_or_flag = 0; 
                        error = izo_mark_dentry(file->f_dentry,
                                                   data->ioc_and_flag,
                                                   data->ioc_or_flag, &res);
                        if (error) 
                                break;
                        error = izo_mark_fset(file->f_dentry,
                                                 data->ioc_and_flag,
                                                 data->ioc_or_flag, &fflags);
                        if (error) 
                                break;
                        error = izo_mark_cache(file->f_dentry,
                                                  data->ioc_and_flag,
                                                  data->ioc_or_flag,
                                                  &cflags);

                        if (error) 
                                break;
                        data->ioc_and_flag = fflags;
                        data->ioc_or_flag = cflags;
                        break;
                }
                default:
                        error = -EINVAL;
                }

                if (error) { 
                        EXIT;
                        return error;
                }
                data->ioc_mark_what = res;
                CDEBUG(D_DOWNCALL, "mark inode: %ld, and: %x, or: %x, what %x\n",
                       file->f_dentry->d_inode->i_ino, data->ioc_and_flag,
                       data->ioc_or_flag, data->ioc_mark_what);

                EXIT;
                return copy_to_user((char *)arg, data, sizeof(*data))? -EFAULT : 0;
        }
#if 0
        case IZO_IOC_CLIENT_MAKE_BRANCH: {
                struct presto_file_set *fset;
                int minor;

                fset = presto_fset(file->f_dentry);
                if (fset == NULL) {
                        EXIT;
                        return -ENODEV;
                }
                minor = presto_f2m(fset);

                rc = izo_upc_client_make_branch(minor, fset->fset_name,
                                                data->ioc_inlbuf1,
                                                data->ioc_inlbuf2);
                EXIT;
                return rc;
        }
#endif
        case IZO_IOC_SERVER_MAKE_BRANCH: {
                struct presto_file_set *fset;
                int minor;

                fset = presto_fset(file->f_dentry);
                if (fset == NULL) {
                        EXIT;
                        return -ENODEV;
                }
                minor = presto_f2m(fset);

                izo_upc_server_make_branch(minor, data->ioc_inlbuf1);
                EXIT;
                return 0;
        }
        case IZO_IOC_SET_KMLSIZE: {
                struct presto_file_set *fset;
                int minor;
                struct izo_rcvd_rec rec;

                fset = presto_fset(file->f_dentry);
                if (fset == NULL) {
                        EXIT;
                        return -ENODEV;
                }
                minor = presto_f2m(fset);

                rc = izo_upc_set_kmlsize(minor, fset->fset_name, data->ioc_uuid,
                                         data->ioc_kmlsize);

                if (rc != 0) {
                        EXIT;
                        return rc;
                }

                rc = izo_rcvd_get(&rec, fset, data->ioc_uuid);
                if (rc == -EINVAL) {
                        /* We don't know anything about this uuid yet; no
                         * worries. */
                        memset(&rec, 0, sizeof(rec));
                } else if (rc <= 0) {
                        CERROR("InterMezzo: error reading last_rcvd: %d\n", rc);
                        EXIT;
                        return rc;
                }
                rec.lr_remote_offset = data->ioc_kmlsize;
                rc = izo_rcvd_write(fset, &rec);
                if (rc <= 0) {
                        CERROR("InterMezzo: error writing last_rcvd: %d\n", rc);
                        EXIT;
                        return rc;
                }
                EXIT;
                return rc;
        }
        case IZO_IOC_BRANCH_UNDO: {
                struct presto_file_set *fset;
                int minor;

                fset = presto_fset(file->f_dentry);
                if (fset == NULL) {
                        EXIT;
                        return -ENODEV;
                }
                minor = presto_f2m(fset);

                rc = izo_upc_branch_undo(minor, fset->fset_name,
                                         data->ioc_inlbuf1);
                EXIT;
                return rc;
        }
        case IZO_IOC_BRANCH_REDO: {
                struct presto_file_set *fset;
                int minor;

                fset = presto_fset(file->f_dentry);
                if (fset == NULL) {
                        EXIT;
                        return -ENODEV;
                }
                minor = presto_f2m(fset);

                rc = izo_upc_branch_redo(minor, fset->fset_name,
                                         data->ioc_inlbuf1);
                EXIT;
                return rc;
        }

        case TCGETS:
                EXIT;
                return -EINVAL;

        default:
                EXIT;
                return -EINVAL;
                
        }
        EXIT;
        return 0;
}

struct file_operations presto_dir_fops = {
        .ioctl =  presto_ioctl
};

struct inode_operations presto_dir_iops = {
        .create       = presto_create,
        .lookup       = presto_lookup,
        .link         = presto_link,
        .unlink       = presto_unlink,
        .symlink      = presto_symlink,
        .mkdir        = presto_mkdir,
        .rmdir        = presto_rmdir,
        .mknod        = presto_mknod,
        .rename       = presto_rename,
        .permission   = presto_permission,
        .setattr      = presto_setattr,
#ifdef CONFIG_FS_EXT_ATTR
        .set_ext_attr = presto_set_ext_attr,
#endif
};


