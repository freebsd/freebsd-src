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
 * Reintegration of KML records
 *
 */

#define __NO_VERSION__
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/mmu_context.h>
#include <linux/intermezzo_fs.h>
#include <linux/intermezzo_psdev.h>

static void kmlreint_pre_secure(struct kml_rec *rec, struct file *dir,
                                struct run_ctxt *saved)
{
        struct run_ctxt ctxt; 
        struct presto_dentry_data *dd = presto_d2d(dir->f_dentry);
        int i;

        ctxt.fsuid = rec->prefix.hdr->fsuid;
        ctxt.fsgid = rec->prefix.hdr->fsgid;
        ctxt.fs = KERNEL_DS; 
        ctxt.pwd = dd->dd_fset->fset_dentry;
        ctxt.pwdmnt = dd->dd_fset->fset_mnt;

        ctxt.root = ctxt.pwd;
        ctxt.rootmnt = ctxt.pwdmnt;
        if (rec->prefix.hdr->ngroups > 0) {
                ctxt.ngroups = rec->prefix.hdr->ngroups;
                for (i = 0; i< ctxt.ngroups; i++) 
                        ctxt.groups[i] = rec->prefix.groups[i];
        } else
                ctxt.ngroups = 0;

        push_ctxt(saved, &ctxt);
}


/* Append two strings in a less-retarded fashion. */
static char * path_join(char *p1, int p1len, char *p2, int p2len)
{
        int size = p1len + p2len + 2; /* possibly one extra /, one NULL */
        char *path;

        path = kmalloc(size, GFP_KERNEL);
        if (path == NULL)
                return NULL;

        memcpy(path, p1, p1len);
        if (path[p1len - 1] != '/') {
                path[p1len] = '/';
                p1len++;
        }
        memcpy(path + p1len, p2, p2len);
        path[p1len + p2len] = '\0';

        return path;
}

static inline int kml_recno_equal(struct kml_rec *rec,
                                  struct presto_file_set *fset)
{
        return (rec->suffix->recno == fset->fset_lento_recno + 1);
}

static inline int version_equal(struct presto_version *a, struct inode *inode)
{
        if (a == NULL)
                return 1;

        if (inode == NULL) {
                CERROR("InterMezzo: NULL inode in version_equal()\n");
                return 0;
        }

        if (inode->i_mtime == a->pv_mtime &&
            (S_ISDIR(inode->i_mode) || inode->i_size == a->pv_size))
                return 1;

        return 0;
}

static int reint_close(struct kml_rec *rec, struct file *file,
                       struct lento_vfs_context *given_info)
{
        struct run_ctxt saved_ctxt;
        int error;
        struct presto_file_set *fset;
        struct lento_vfs_context info; 
        ENTRY;

        memcpy(&info, given_info, sizeof(*given_info));


        CDEBUG (D_KML, "=====REINT_CLOSE::%s\n", rec->path);

        fset = presto_fset(file->f_dentry);
        if (fset->fset_flags & FSET_DATA_ON_DEMAND) {
                struct iattr iattr;

                iattr.ia_valid = ATTR_CTIME | ATTR_MTIME | ATTR_SIZE;
                iattr.ia_mtime = (time_t)rec->new_objectv->pv_mtime;
                iattr.ia_ctime = (time_t)rec->new_objectv->pv_ctime;
                iattr.ia_size = (time_t)rec->new_objectv->pv_size;

                /* no kml record, but update last rcvd */
                /* save fileid in dentry for later backfetch */
                info.flags |= LENTO_FL_EXPECT | LENTO_FL_SET_DDFILEID;
                info.remote_ino = rec->ino;
                info.remote_generation = rec->generation;
                info.flags &= ~LENTO_FL_KML;
                kmlreint_pre_secure(rec, file, &saved_ctxt);
                error = lento_setattr(rec->path, &iattr, &info);
                pop_ctxt(&saved_ctxt);

                presto_d2d(file->f_dentry)->dd_flags &= ~PRESTO_DATA;
        } else {
                int minor = presto_f2m(fset);

                info.updated_time = rec->new_objectv->pv_mtime;
                memcpy(&info.remote_version, rec->old_objectv, 
                       sizeof(*rec->old_objectv));
                info.remote_ino = rec->ino;
                info.remote_generation = rec->generation;
                error = izo_upc_backfetch(minor, rec->path, fset->fset_name,
                                          &info);
                if (error) {
                        CERROR("backfetch error %d\n", error);
                        /* if file doesn't exist anymore,  then ignore the CLOSE
                         * and just update the last_rcvd.
                         */
                        if (error == ENOENT) {
                                CDEBUG(D_KML, "manually updating remote offset uuid %s"
                                       "recno %d offset %Lu\n", info.uuid, info.recno, info.kml_offset);
                                error = izo_rcvd_upd_remote(fset, info.uuid, info.recno, info.kml_offset);
                                if(error)
                                        CERROR("izo_rcvd_upd_remote error %d\n", error);

                        } 
                }
                        
                /* propagate error to avoid further reint */
        }

        EXIT;
        return error;
}

static int reint_create(struct kml_rec *rec, struct file *dir,
                        struct lento_vfs_context *info)
{
        struct run_ctxt saved_ctxt;
        int     error;        ENTRY;

        CDEBUG (D_KML, "=====REINT_CREATE::%s\n", rec->path);
        info->updated_time = rec->new_objectv->pv_ctime;
        kmlreint_pre_secure(rec, dir, &saved_ctxt);
        error = lento_create(rec->path, rec->mode, info);
        pop_ctxt(&saved_ctxt); 

        EXIT;
        return error;
}

static int reint_link(struct kml_rec *rec, struct file *dir,
                      struct lento_vfs_context *info)
{
        struct run_ctxt saved_ctxt;
        int     error;

        ENTRY;

        CDEBUG (D_KML, "=====REINT_LINK::%s -> %s\n", rec->path, rec->target);
        info->updated_time = rec->new_objectv->pv_mtime;
        kmlreint_pre_secure(rec, dir, &saved_ctxt);
        error = lento_link(rec->path, rec->target, info);
        pop_ctxt(&saved_ctxt); 

        EXIT;
        return error;
}

static int reint_mkdir(struct kml_rec *rec, struct file *dir,
                       struct lento_vfs_context *info)
{
        struct run_ctxt saved_ctxt;
        int     error;

        ENTRY;

        CDEBUG (D_KML, "=====REINT_MKDIR::%s\n", rec->path);
        info->updated_time = rec->new_objectv->pv_ctime;
        kmlreint_pre_secure(rec, dir, &saved_ctxt);
        error = lento_mkdir(rec->path, rec->mode, info);
        pop_ctxt(&saved_ctxt); 

        EXIT;
        return error;
}

static int reint_mknod(struct kml_rec *rec, struct file *dir,
                       struct lento_vfs_context *info)
{
        struct run_ctxt saved_ctxt;
        int     error, dev;

        ENTRY;

        CDEBUG (D_KML, "=====REINT_MKNOD::%s\n", rec->path);
        info->updated_time = rec->new_objectv->pv_ctime;
        kmlreint_pre_secure(rec, dir, &saved_ctxt);

        dev = rec->rdev ?: MKDEV(rec->major, rec->minor);

        error = lento_mknod(rec->path, rec->mode, dev, info);
        pop_ctxt(&saved_ctxt); 

        EXIT;
        return error;
}


static int reint_noop(struct kml_rec *rec, struct file *dir,
                      struct lento_vfs_context *info)
{
        return 0;
}

static int reint_rename(struct kml_rec *rec, struct file *dir,
                        struct lento_vfs_context *info)
{
        struct run_ctxt saved_ctxt;
        int     error;

        ENTRY;

        CDEBUG (D_KML, "=====REINT_RENAME::%s -> %s\n", rec->path, rec->target);
        info->updated_time = rec->new_objectv->pv_mtime;
        kmlreint_pre_secure(rec, dir, &saved_ctxt);
        error = lento_rename(rec->path, rec->target, info);
        pop_ctxt(&saved_ctxt); 

        EXIT;
        return error;
}

static int reint_rmdir(struct kml_rec *rec, struct file *dir,
                       struct lento_vfs_context *info)
{
        struct run_ctxt saved_ctxt;
        int     error;
        char *path;

        ENTRY;

        path = path_join(rec->path, rec->pathlen - 1, rec->target, rec->targetlen);
        if (path == NULL) {
                EXIT;
                return -ENOMEM;
        }

        CDEBUG (D_KML, "=====REINT_RMDIR::%s\n", path);
        info->updated_time = rec->new_parentv->pv_mtime;
        kmlreint_pre_secure(rec, dir, &saved_ctxt);
        error = lento_rmdir(path, info);
        pop_ctxt(&saved_ctxt); 

        kfree(path);
        EXIT;
        return error;
}

static int reint_setattr(struct kml_rec *rec, struct file *dir,
                         struct lento_vfs_context *info)
{
        struct run_ctxt saved_ctxt;
        struct iattr iattr;
        int     error;

        ENTRY;

        iattr.ia_valid = rec->valid;
        iattr.ia_mode  = (umode_t)rec->mode;
        iattr.ia_uid   = (uid_t)rec->uid;
        iattr.ia_gid   = (gid_t)rec->gid;
        iattr.ia_size  = (off_t)rec->size;
        iattr.ia_ctime = (time_t)rec->ctime;
        iattr.ia_mtime = (time_t)rec->mtime;
        iattr.ia_atime = iattr.ia_mtime; /* We don't track atimes. */
        iattr.ia_attr_flags = rec->flags;

        CDEBUG (D_KML, "=====REINT_SETATTR::%s (%d)\n", rec->path, rec->valid);
        kmlreint_pre_secure(rec, dir, &saved_ctxt);
        error = lento_setattr(rec->path, &iattr, info);
        pop_ctxt(&saved_ctxt); 

        EXIT;
        return error;
}

static int reint_symlink(struct kml_rec *rec, struct file *dir,
                         struct lento_vfs_context *info)
{
        struct run_ctxt saved_ctxt;
        int     error;

        ENTRY;

        CDEBUG (D_KML, "=====REINT_SYMLINK::%s -> %s\n", rec->path, rec->target);
        info->updated_time = rec->new_objectv->pv_ctime;
        kmlreint_pre_secure(rec, dir, &saved_ctxt);
        error = lento_symlink(rec->target, rec->path, info);
        pop_ctxt(&saved_ctxt); 

        EXIT;
        return error;
}

static int reint_unlink(struct kml_rec *rec, struct file *dir,
                        struct lento_vfs_context *info)
{
        struct run_ctxt saved_ctxt;
        int     error;
        char *path;

        ENTRY;

        path = path_join(rec->path, rec->pathlen - 1, rec->target, rec->targetlen);
        if (path == NULL) {
                EXIT;
                return -ENOMEM;
        }

        CDEBUG (D_KML, "=====REINT_UNLINK::%s\n", path);
        info->updated_time = rec->new_parentv->pv_mtime;
        kmlreint_pre_secure(rec, dir, &saved_ctxt);
        error = lento_unlink(path, info);
        pop_ctxt(&saved_ctxt); 

        kfree(path);
        EXIT;
        return error;
}

static int branch_reint_rename(struct presto_file_set *fset, struct kml_rec *rec, 
                   struct file *dir, struct lento_vfs_context *info,
                   char * kml_data, __u64 kml_size)
{
        int     error;

        ENTRY;

        error = reint_rename(rec, dir, info);
        if (error == -ENOENT) {
                /* normal reint failed because path was not found */
                struct rec_info rec;
                
                CDEBUG(D_KML, "saving branch rename kml\n");
                rec.is_kml = 1;
                rec.size = kml_size;
                error = presto_log(fset, &rec, kml_data, kml_size,
                           NULL, 0, NULL, 0,  NULL, 0);
                if (error == 0)
                        error = presto_write_last_rcvd(&rec, fset, info);
        }

        EXIT;
        return error;
}

int branch_reinter(struct presto_file_set *fset, struct kml_rec *rec, 
                   struct file *dir, struct lento_vfs_context *info,
                   char * kml_data, __u64 kml_size)
{
        int error = 0;
        int op = rec->prefix.hdr->opcode;

        if (op == KML_OPCODE_CLOSE) {
                /* regular close and backfetch */
                error = reint_close(rec, dir, info);
        } else if  (op == KML_OPCODE_RENAME) {
                /* rename only if name already exists  */
                error = branch_reint_rename(fset, rec, dir, info,
                                            kml_data, kml_size);
        } else {
                /* just rewrite kml into branch/kml and update last_rcvd */
                struct rec_info rec;
                
                CDEBUG(D_KML, "Saving branch kml\n");
                rec.is_kml = 1;
                rec.size = kml_size;
                error = presto_log(fset, &rec, kml_data, kml_size,
                           NULL, 0, NULL, 0,  NULL, 0);
                if (error == 0)
                        error = presto_write_last_rcvd(&rec, fset, info);
        }
                
        return error;
}

typedef int (*reinter_t)(struct kml_rec *rec, struct file *basedir,
                         struct lento_vfs_context *info);

static reinter_t presto_reinters[KML_OPCODE_NUM] =
{
        [KML_OPCODE_CLOSE] = reint_close,
        [KML_OPCODE_CREATE] = reint_create,
        [KML_OPCODE_LINK] = reint_link,
        [KML_OPCODE_MKDIR] = reint_mkdir,
        [KML_OPCODE_MKNOD] = reint_mknod,
        [KML_OPCODE_NOOP] = reint_noop,
        [KML_OPCODE_RENAME] = reint_rename,
        [KML_OPCODE_RMDIR] = reint_rmdir,
        [KML_OPCODE_SETATTR] = reint_setattr,
        [KML_OPCODE_SYMLINK] = reint_symlink,
        [KML_OPCODE_UNLINK] = reint_unlink,
};

static inline reinter_t get_reinter(int op)
{
        if (op < 0 || op >= sizeof(presto_reinters) / sizeof(reinter_t)) 
                return NULL; 
        else 
                return  presto_reinters[op];
}

int kml_reint_rec(struct file *dir, struct izo_ioctl_data *data)
{
        char *ptr;
        char *end;
        struct kml_rec rec;
        int error = 0;
        struct lento_vfs_context info;
        struct presto_cache *cache;
        struct presto_file_set *fset;
        struct presto_dentry_data *dd = presto_d2d(dir->f_dentry);
        int op;
        reinter_t reinter;

        struct izo_rcvd_rec lr_rec;
        int off;

        ENTRY;

        error = presto_prep(dir->f_dentry, &cache, &fset);
        if ( error  ) {
                CERROR("intermezzo: Reintegration on invalid file\n");
                return error;
        }

        if (!dd || !dd->dd_fset || dd->dd_fset->fset_dentry != dir->f_dentry) { 
                CERROR("intermezzo: reintegration on non-fset root (ino %ld)\n",
                       dir->f_dentry->d_inode->i_ino);
                    
                return -EINVAL;
        }

        if (data->ioc_plen1 > 64 * 1024) {
                EXIT;
                return -ENOSPC;
        }

        ptr = fset->fset_reint_buf;
        end = ptr + data->ioc_plen1;

        if (copy_from_user(ptr, data->ioc_pbuf1, data->ioc_plen1)) { 
                EXIT;
                error = -EFAULT;
                goto out;
        }

        error = kml_unpack(&rec, &ptr, end);
        if (error) { 
                EXIT;
                error = -EFAULT;
                goto out;
        }

        off = izo_rcvd_get(&lr_rec, fset, data->ioc_uuid);
        if (off < 0) {
                CERROR("No last_rcvd record, setting to 0\n");
                memset(&lr_rec, 0, sizeof(lr_rec));
        }
 
        data->ioc_kmlsize = ptr - fset->fset_reint_buf;

        if (rec.suffix->recno != lr_rec.lr_remote_recno + 1) {
                CERROR("KML record number %Lu expected, not %d\n",
                       lr_rec.lr_remote_recno + 1,
                       rec.suffix->recno);

#if 0
                if (!version_check(&rec, dd->dd_fset, &info)) {
                        /* FIXME: do an upcall to resolve conflicts */
                        CERROR("intermezzo: would be a conflict!\n");
                        error = -EINVAL;
                        EXIT;
                        goto out;
                }
#endif
        }

        op = rec.prefix.hdr->opcode;

        reinter = get_reinter(op);
        if (!reinter) { 
                CERROR("%s: Unrecognized KML opcode %d\n", __FUNCTION__, op);
                error = -EINVAL;
                EXIT;
                goto out;
        }

        info.kml_offset = data->ioc_offset + data->ioc_kmlsize;
        info.recno = rec.suffix->recno;
        info.flags = LENTO_FL_EXPECT;
        if (data->ioc_flags)
                info.flags |= LENTO_FL_KML;

        memcpy(info.uuid, data->ioc_uuid, sizeof(info.uuid));

        if (fset->fset_flags & FSET_IS_BRANCH && data->ioc_flags)
                error = branch_reinter(fset, &rec, dir, &info, fset->fset_reint_buf,
                                       data->ioc_kmlsize);
        else 
                error = reinter(&rec, dir, &info);
 out: 
        EXIT;
        return error;
}

int izo_get_fileid(struct file *dir, struct izo_ioctl_data *data)
{
        char *buf = NULL; 
        char *ptr;
        char *end;
        struct kml_rec rec;
        struct file *file;
        struct presto_cache *cache;
        struct presto_file_set *fset;
        struct presto_dentry_data *dd = presto_d2d(dir->f_dentry);
        struct run_ctxt saved_ctxt;
        int     error;

        ENTRY;

        error = presto_prep(dir->f_dentry, &cache, &fset);
        if ( error  ) {
                CERROR("intermezzo: Reintegration on invalid file\n");
                return error;
        }

        if (!dd || !dd->dd_fset || dd->dd_fset->fset_dentry != dir->f_dentry) { 
                CERROR("intermezzo: reintegration on non-fset root (ino %ld)\n",
                       dir->f_dentry->d_inode->i_ino);
                    
                return -EINVAL;
        }


        PRESTO_ALLOC(buf, data->ioc_plen1);
        if (!buf) { 
                EXIT;
                return -ENOMEM;
        }
        ptr = buf;
        end = buf + data->ioc_plen1;

        if (copy_from_user(buf, data->ioc_pbuf1, data->ioc_plen1)) { 
                EXIT;
                PRESTO_FREE(buf, data->ioc_plen1);
                return -EFAULT;
        }

        error = kml_unpack(&rec, &ptr, end);
        if (error) { 
                EXIT;
                PRESTO_FREE(buf, data->ioc_plen1);
                return -EFAULT;
        }

        kmlreint_pre_secure(&rec, dir, &saved_ctxt);

        file = filp_open(rec.path, O_RDONLY, 0);
        if (!file || IS_ERR(file)) { 
                error = PTR_ERR(file);
                goto out;
        }
        data->ioc_ino = file->f_dentry->d_inode->i_ino;
        data->ioc_generation = file->f_dentry->d_inode->i_generation; 
        filp_close(file, 0); 

        CDEBUG(D_FILE, "%s ino %Lx, gen %Lx\n", rec.path, 
               data->ioc_ino, data->ioc_generation);

 out:
        if (buf) 
                PRESTO_FREE(buf, data->ioc_plen1);
        pop_ctxt(&saved_ctxt); 
        EXIT;
        return error;
}


