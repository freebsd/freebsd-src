/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (C) 1998 Peter J. Braam
 *  Copyright (C) 2001 Cluster File Systems, Inc. 
 *  Copyright (C) 2001 Tacit Networks, Inc. <phil@off.net>
 *
 *  Support for journalling extended attributes
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
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/time.h>
#include <linux/errno.h>
#include <linux/locks.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/smp_lock.h>
#include <linux/intermezzo_fs.h>
#include <linux/intermezzo_psdev.h>

struct presto_reservation_data {
        unsigned int ri_recno;
        loff_t ri_offset;
        loff_t ri_size;
        struct list_head ri_list;
};

/* 
 *  Locking Semantics
 * 
 * write lock in struct presto_log_fd: 
 *  - name: fd_lock
 *  - required for: accessing any field in a presto_log_fd 
 *  - may not be held across I/O
 *  - 
 *  
 */

/*
 *  reserve record space and/or atomically request state of the log
 *  rec will hold the location reserved record upon return
 *  this reservation will be placed in the queue
 */ 
static void presto_reserve_record(struct presto_file_set *fset, 
                           struct presto_log_fd *fd, 
                           struct rec_info *rec,
                           struct presto_reservation_data *rd)
{
        int chunked_record = 0; 
        ENTRY;
        
        write_lock(&fd->fd_lock);
        if ( rec->is_kml ) { 
                int chunk = 1 << fset->fset_chunkbits;
                int chunk_mask = ~(chunk -1); 
                loff_t boundary; 

                boundary =  (fd->fd_offset + chunk - 1) & chunk_mask;
                if ( fd->fd_offset + rec->size >= boundary ) {
                        chunked_record = 1;
                        fd->fd_offset = boundary; 
                }
        }

        fd->fd_recno++;
        
        /* this moves the fd_offset back after truncation */ 
        if ( list_empty(&fd->fd_reservations) && 
             !chunked_record) { 
                fd->fd_offset = fd->fd_file->f_dentry->d_inode->i_size;
        }

        rec->offset = fd->fd_offset;
        if (rec->is_kml)
                rec->offset += fset->fset_kml_logical_off;

        rec->recno = fd->fd_recno;

        /* add the reservation data to the end of the list */
        rd->ri_offset = fd->fd_offset;
        rd->ri_size = rec->size;
        rd->ri_recno = rec->recno; 
        list_add(&rd->ri_list, fd->fd_reservations.prev);

        fd->fd_offset += rec->size;

        write_unlock(&fd->fd_lock); 

        EXIT;
}

static inline void presto_release_record(struct presto_log_fd *fd,
                                         struct presto_reservation_data *rd)
{
        write_lock(&fd->fd_lock);
        list_del(&rd->ri_list);
        write_unlock(&fd->fd_lock);
}

/* XXX should we ask for do_truncate to be exported? */
int izo_do_truncate(struct presto_file_set *fset, struct dentry *dentry,
                    loff_t length,  loff_t size_check)
{
        struct inode *inode = dentry->d_inode;
        int error;
        struct iattr newattrs;

        ENTRY;

        if (length < 0) {
                EXIT;
                return -EINVAL;
        }

        down(&inode->i_sem);
        lock_kernel();
        
        if (size_check != inode->i_size) { 
                unlock_kernel();
                up(&inode->i_sem);
                EXIT;
                return -EALREADY; 
        }

        newattrs.ia_size = length;
        newattrs.ia_valid = ATTR_SIZE | ATTR_CTIME;

        if (inode->i_op && inode->i_op->setattr)
                error = inode->i_op->setattr(dentry, &newattrs);
        else {
                inode_setattr(dentry->d_inode, &newattrs);
                error = 0;
        }

        unlock_kernel();
        up(&inode->i_sem);
        EXIT;
        return error;
}

static void presto_kml_truncate(struct presto_file_set *fset)
{
        int rc;
        ENTRY;

        write_lock(&fset->fset_kml.fd_lock);
        if (fset->fset_kml.fd_truncating == 1 ) {
                write_unlock(&fset->fset_kml.fd_lock);
                EXIT;
                return;
        }

        fset->fset_kml.fd_truncating = 1;
        write_unlock(&fset->fset_kml.fd_lock);

        CERROR("islento: %d, count: %d\n",
               ISLENTO(presto_i2m(fset->fset_dentry->d_inode)),
               fset->fset_permit_count);

        rc = izo_upc_kml_truncate(fset->fset_cache->cache_psdev->uc_minor,
                                fset->fset_lento_off, fset->fset_lento_recno,
                                fset->fset_name);

        /* Userspace is the only permitholder now, and will retain an exclusive
         * hold on the permit until KML truncation completes. */
        /* FIXME: double check this code path now that the precise semantics of
         * fset->fset_permit_count have changed. */

        if (rc != 0) {
                write_lock(&fset->fset_kml.fd_lock);
                fset->fset_kml.fd_truncating = 0;
                write_unlock(&fset->fset_kml.fd_lock);
        }

        EXIT;
}

void *presto_trans_start(struct presto_file_set *fset, struct inode *inode,
                         int op)
{
        ENTRY;
        if ( !fset->fset_cache->cache_filter->o_trops ) {
                EXIT;
                return NULL;
        }
        EXIT;
        return fset->fset_cache->cache_filter->o_trops->tr_start
                (fset, inode, op);
}

void presto_trans_commit(struct presto_file_set *fset, void *handle)
{
        ENTRY;
        if (!fset->fset_cache->cache_filter->o_trops ) {
                EXIT;
                return;
        }

        fset->fset_cache->cache_filter->o_trops->tr_commit(fset, handle);

        /* Check to see if the KML needs truncated. */
        if (fset->kml_truncate_size > 0 &&
            !fset->fset_kml.fd_truncating &&
            fset->fset_kml.fd_offset > fset->kml_truncate_size) {
                CDEBUG(D_JOURNAL, "kml size: %lu; truncating\n",
                       (unsigned long)fset->fset_kml.fd_offset);
                presto_kml_truncate(fset);
        }
        EXIT;
}

inline int presto_no_journal(struct presto_file_set *fset)
{
        int minor = fset->fset_cache->cache_psdev->uc_minor;
        return izo_channels[minor].uc_no_journal;
}

#define size_round(x)  (((x)+3) & ~0x3)

#define BUFF_FREE(buf) PRESTO_FREE(buf, PAGE_SIZE)
#define BUFF_ALLOC(newbuf, oldbuf)              \
        PRESTO_ALLOC(newbuf, PAGE_SIZE);        \
        if ( !newbuf ) {                        \
                if (oldbuf)                     \
                        BUFF_FREE(oldbuf);      \
                return -ENOMEM;                 \
        }

/*
 * "buflen" should be PAGE_SIZE or more.
 * Give relative path wrt to a fsetroot
 */
char * presto_path(struct dentry *dentry, struct dentry *root,
                   char *buffer, int buflen)
{
        char * end = buffer+buflen;
        char * retval;

        *--end = '\0';
        buflen--;
        if (dentry->d_parent != dentry && list_empty(&dentry->d_hash)) {
                buflen -= 10;
                end -= 10;
                memcpy(end, " (deleted)", 10);
        }

        /* Get '/' right */
        retval = end-1;
        *retval = '/';

        for (;;) {
                struct dentry * parent;
                int namelen;

                if (dentry == root)
                        break;
                parent = dentry->d_parent;
                if (dentry == parent)
                        break;
                namelen = dentry->d_name.len;
                buflen -= namelen + 1;
                if (buflen < 0)
                        break;
                end -= namelen;
                memcpy(end, dentry->d_name.name, namelen);
                *--end = '/';
                retval = end;
                dentry = parent;
        }
        return retval;
}

static inline char *logit(char *buf, const void *value, int size)
{
        char *ptr = (char *)value;

        memcpy(buf, ptr, size);
        buf += size;
        return buf;
}


static inline char *
journal_log_prefix_with_groups_and_ids(char *buf, int opcode, 
                                       struct rec_info *rec,
                                       __u32 ngroups, gid_t *groups,
                                       __u32 fsuid, __u32 fsgid)
{
        struct kml_prefix_hdr p;
        u32 loggroups[NGROUPS_MAX];

        int i; 

        p.len = cpu_to_le32(rec->size);
        p.version = KML_MAJOR_VERSION | KML_MINOR_VERSION;
        p.pid = cpu_to_le32(current->pid);
        p.auid = cpu_to_le32(current->uid);
        p.fsuid = cpu_to_le32(fsuid);
        p.fsgid = cpu_to_le32(fsgid);
        p.ngroups = cpu_to_le32(ngroups);
        p.opcode = cpu_to_le32(opcode);
        for (i=0 ; i < ngroups ; i++)
                loggroups[i] = cpu_to_le32((__u32) groups[i]);

        buf = logit(buf, &p, sizeof(struct kml_prefix_hdr));
        buf = logit(buf, &loggroups, sizeof(__u32) * ngroups);
        return buf;
}

static inline char *
journal_log_prefix(char *buf, int opcode, struct rec_info *rec)
{
        __u32 groups[NGROUPS_MAX]; 
        int i; 

        /* convert 16 bit gid's to 32 bit gid's */
        for (i=0; i<current->ngroups; i++) 
                groups[i] = (__u32) current->groups[i];
        
        return journal_log_prefix_with_groups_and_ids(buf, opcode, rec,
                                                      (__u32)current->ngroups,
                                                      groups,
                                                      (__u32)current->fsuid,
                                                      (__u32)current->fsgid);
}

static inline char *
journal_log_prefix_with_groups(char *buf, int opcode, struct rec_info *rec, 
                               __u32 ngroups, gid_t *groups)
{
        return journal_log_prefix_with_groups_and_ids(buf, opcode, rec,
                                                      ngroups, groups,
                                                      (__u32)current->fsuid,
                                                      (__u32)current->fsgid);
}

static inline char *log_dentry_version(char *buf, struct dentry *dentry)
{
        struct presto_version version;

        presto_getversion(&version, dentry->d_inode);
        
        version.pv_mtime = HTON__u64(version.pv_mtime);
        version.pv_ctime = HTON__u64(version.pv_ctime);
        version.pv_size = HTON__u64(version.pv_size);

        return logit(buf, &version, sizeof(version));
}

static inline char *log_version(char *buf, struct presto_version *pv)
{
        struct presto_version version;

        memcpy(&version, pv, sizeof(version));
        
        version.pv_mtime = HTON__u64(version.pv_mtime);
        version.pv_ctime = HTON__u64(version.pv_ctime);
        version.pv_size = HTON__u64(version.pv_size);

        return logit(buf, &version, sizeof(version));
}

static inline char *log_rollback(char *buf, struct izo_rollback_data *rb)
{
        struct izo_rollback_data rollback;

        memcpy(&rollback, rb, sizeof(rollback));
        
        rollback.rb_mode = HTON__u32(rollback.rb_mode);
        rollback.rb_rdev = HTON__u32(rollback.rb_rdev);
        rollback.rb_uid = HTON__u64(rollback.rb_uid);
        rollback.rb_gid = HTON__u64(rollback.rb_gid);

        return logit(buf, &rollback, sizeof(rollback));
}

static inline char *journal_log_suffix(char *buf, char *log,
                                       struct presto_file_set *fset,
                                       struct dentry *dentry,
                                       struct rec_info *rec)
{
        struct kml_suffix s;
        struct kml_prefix_hdr *p = (struct kml_prefix_hdr *)log;

#if 0
        /* XXX needs to be done after reservation, 
           disable ths until version 1.2 */
        if ( dentry ) { 
                s.prevrec = cpu_to_le32(rec->offset - 
                                        presto_d2d(dentry)->dd_kml_offset);
                presto_d2d(dentry)->dd_kml_offset = rec->offset;
        } else { 
                s.prevrec = -1;
        }
#endif
        s.prevrec = 0; 

        /* record number needs to be filled in after reservation 
           s.recno = cpu_to_le32(rec->recno); */ 
        s.time = cpu_to_le32(CURRENT_TIME);
        s.len = p->len;
        return logit(buf, &s, sizeof(s));
}

int izo_log_close(struct presto_log_fd *logfd)
{
        int rc = 0;

        if (logfd->fd_file) {
                rc = filp_close(logfd->fd_file, 0);
                logfd->fd_file = NULL;
        } else
                CERROR("InterMezzo: %s: no filp\n", __FUNCTION__);
        if (rc != 0)
                CERROR("InterMezzo: close files: filp won't close: %d\n", rc);

        return rc;
}

int presto_fwrite(struct file *file, const char *str, int len, loff_t *off)
{
        int rc;
        mm_segment_t old_fs;
        ENTRY;

        rc = -EINVAL;
        if ( !off ) {
                EXIT;
                return rc;
        }

        if ( ! file ) {
                EXIT;
                return rc;
        }

        if ( ! file->f_op ) {
                EXIT;
                return rc;
        }

        if ( ! file->f_op->write ) {
                EXIT;
                return rc;
        }

        old_fs = get_fs();
        set_fs(get_ds());
        rc = file->f_op->write(file, str, len, off);
        if (rc != len) {
                CERROR("presto_fwrite: wrote %d bytes instead of "
                       "%d at %ld\n", rc, len, (long)*off);
                rc = -EIO; 
        }
        set_fs(old_fs);
        EXIT;
        return rc;
}

int presto_fread(struct file *file, char *str, int len, loff_t *off)
{
        int rc;
        mm_segment_t old_fs;
        ENTRY;

        if (len > 512)
                CERROR("presto_fread: read at %Ld for %d bytes, ino %ld\n",
                       *off, len, file->f_dentry->d_inode->i_ino); 

        rc = -EINVAL;
        if ( !off ) {
                EXIT;
                return rc;
        }

        if ( ! file ) {
                EXIT;
                return rc;
        }

        if ( ! file->f_op ) {
                EXIT;
                return rc;
        }

        if ( ! file->f_op->read ) {
                EXIT;
                return rc;
        }

        old_fs = get_fs();
        set_fs(get_ds());
        rc = file->f_op->read(file, str, len, off);
        if (rc != len) {
                CDEBUG(D_FILE, "presto_fread: read %d bytes instead of "
                       "%d at %Ld\n", rc, len, *off);
                rc = -EIO; 
        }
        set_fs(old_fs);
        EXIT;
        return rc;
}

loff_t presto_kml_offset(struct presto_file_set *fset)
{
        unsigned int kml_recno;
        struct presto_log_fd *fd = &fset->fset_kml;
        loff_t  offset;
        ENTRY;

        write_lock(&fd->fd_lock); 

        /* Determine the largest valid offset, i.e. up until the first
         * reservation held on the file. */
        if ( !list_empty(&fd->fd_reservations) ) {
                struct presto_reservation_data *rd;
                rd = list_entry(fd->fd_reservations.next, 
                                struct presto_reservation_data, 
                                ri_list);
                offset = rd->ri_offset;
                kml_recno = rd->ri_recno;
        } else {
                offset = fd->fd_file->f_dentry->d_inode->i_size;
                kml_recno = fset->fset_kml.fd_recno; 
        }
        write_unlock(&fd->fd_lock); 
        return offset; 
}

static int presto_kml_dispatch(struct presto_file_set *fset)
{
        int rc = 0;
        unsigned int kml_recno;
        struct presto_log_fd *fd = &fset->fset_kml;
        loff_t offset;
        ENTRY;

        write_lock(&fd->fd_lock); 

        /* Determine the largest valid offset, i.e. up until the first
         * reservation held on the file. */
        if ( !list_empty(&fd->fd_reservations) ) {
                struct presto_reservation_data *rd;
                rd = list_entry(fd->fd_reservations.next, 
                                struct presto_reservation_data, 
                                ri_list);
                offset = rd->ri_offset;
                kml_recno = rd->ri_recno;
        } else {
                offset = fd->fd_file->f_dentry->d_inode->i_size;
                kml_recno = fset->fset_kml.fd_recno; 
        }

        if ( kml_recno < fset->fset_lento_recno ) {
                CERROR("presto_kml_dispatch: smoke is coming\n"); 
                write_unlock(&fd->fd_lock);
                EXIT;
                return 0; 
        } else if ( kml_recno == fset->fset_lento_recno ) {
                write_unlock(&fd->fd_lock);
                EXIT;
                return 0; 
                /* XXX add a further "if" here to delay the KML upcall */ 
#if 0
        } else if ( kml_recno < fset->fset_lento_recno + 100) {
                write_unlock(&fd->fd_lock);
                EXIT;
                return 0;
#endif
        }
        CDEBUG(D_PIOCTL, "fset: %s\n", fset->fset_name);

        rc = izo_upc_kml(fset->fset_cache->cache_psdev->uc_minor,
                       fset->fset_lento_off, fset->fset_lento_recno,
                       offset + fset->fset_kml_logical_off, kml_recno,
                       fset->fset_name);

        if ( rc ) {
                write_unlock(&fd->fd_lock);
                EXIT;
                return rc;
        }

        fset->fset_lento_off = offset;
        fset->fset_lento_recno = kml_recno; 
        write_unlock(&fd->fd_lock);
        EXIT;
        return 0;
}

int izo_lookup_file(struct presto_file_set *fset, char *path,
                    struct nameidata *nd)
{
        int error = 0;

        CDEBUG(D_CACHE, "looking up: %s\n", path);

        if (path_init(path, LOOKUP_PARENT, nd))
                error = path_walk(path, nd);
        if (error) {
                EXIT;
                return error;
        }

        return 0;
}

/* FIXME: this function is a mess of locking and error handling.  There's got to
 * be a better way. */
static int do_truncate_rename(struct presto_file_set *fset, char *oldname,
                              char *newname)
{
        struct dentry *old_dentry, *new_dentry;
        struct nameidata oldnd, newnd;
        char *oldpath, *newpath;
        int error;

        ENTRY;

        oldpath = izo_make_path(fset, oldname);
        if (oldpath == NULL) {
                EXIT;
                return -ENOENT;
        }

        newpath = izo_make_path(fset, newname);
        if (newpath == NULL) {
                error = -ENOENT;
                EXIT;
                goto exit;
        }

        if ((error = izo_lookup_file(fset, oldpath, &oldnd)) != 0) {
                EXIT;
                goto exit1;
        }

        if ((error = izo_lookup_file(fset, newpath, &newnd)) != 0) {
                EXIT;
                goto exit2;
        }

        double_lock(newnd.dentry, oldnd.dentry);
        old_dentry = lookup_hash(&oldnd.last, oldnd.dentry);
        error = PTR_ERR(old_dentry);
        if (IS_ERR(old_dentry)) {
                EXIT;
                goto exit3;
        }
        error = -ENOENT;
        if (!old_dentry->d_inode) {
                EXIT;
                goto exit4;
        }
        new_dentry = lookup_hash(&newnd.last, newnd.dentry);
        error = PTR_ERR(new_dentry);
        if (IS_ERR(new_dentry)) {
                EXIT;
                goto exit4;
        }

        {
        extern int presto_rename(struct inode *old_dir,struct dentry *old_dentry,
                                struct inode *new_dir,struct dentry *new_dentry);
        error = presto_rename(old_dentry->d_parent->d_inode, old_dentry,
                              new_dentry->d_parent->d_inode, new_dentry);
        }

        dput(new_dentry);
        EXIT;
 exit4:
        dput(old_dentry);
 exit3:
        double_up(&newnd.dentry->d_inode->i_sem, &oldnd.dentry->d_inode->i_sem);
        path_release(&newnd);
 exit2:
        path_release(&oldnd);
 exit1:
        PRESTO_FREE(newpath, strlen(newpath) + 1);
 exit:
        PRESTO_FREE(oldpath, strlen(oldpath) + 1);
        return error;
}

/* This function is called with the fset->fset_kml.fd_lock held */
int presto_finish_kml_truncate(struct presto_file_set *fset,
                               unsigned long int offset)
{
        struct lento_vfs_context info;
        void *handle;
        struct file *f;
        struct dentry *dentry;
        int error = 0, len;
        struct nameidata nd;
        char *kmlpath = NULL, *smlpath = NULL;
        ENTRY;

        if (offset == 0) {
                /* Lento couldn't do what it needed to; abort the truncation. */
                fset->fset_kml.fd_truncating = 0;
                EXIT;
                return 0;
        }

        /* someone is about to write to the end of the KML; try again later. */
        if ( !list_empty(&fset->fset_kml.fd_reservations) ) {
                EXIT;
                return -EAGAIN;
        }

        f = presto_copy_kml_tail(fset, offset);
        if (IS_ERR(f)) {
                EXIT;
                return PTR_ERR(f);
        }                        

        /* In a single transaction:
         *
         *   - unlink 'kml'
         *   - rename 'kml_tmp' to 'kml'
         *   - unlink 'sml'
         *   - rename 'sml_tmp' to 'sml'
         *   - rewrite the first record of last_rcvd with the new kml
         *     offset.
         */
        handle = presto_trans_start(fset, fset->fset_dentry->d_inode,
                                    KML_OPCODE_KML_TRUNC);
        if (IS_ERR(handle)) {
                presto_release_space(fset->fset_cache, PRESTO_REQLOW);
                CERROR("ERROR: presto_finish_kml_truncate: no space for transaction\n");
                EXIT;
                return -ENOMEM;
        }

        memset(&info, 0, sizeof(info));
        info.flags = LENTO_FL_IGNORE_TIME;

        kmlpath = izo_make_path(fset, "kml");
        if (kmlpath == NULL) {
                error = -ENOMEM;
                CERROR("make_path failed: ENOMEM\n");
                EXIT;
                goto exit_commit;
        }

        if ((error = izo_lookup_file(fset, kmlpath, &nd)) != 0) {
                CERROR("izo_lookup_file(kml) failed: %d.\n", error);
                EXIT;
                goto exit_commit;
        }
        down(&nd.dentry->d_inode->i_sem);
        dentry = lookup_hash(&nd.last, nd.dentry);
        error = PTR_ERR(dentry);
        if (IS_ERR(dentry)) {
                up(&nd.dentry->d_inode->i_sem);
                path_release(&nd);
                CERROR("lookup_hash failed\n");
                EXIT;
                goto exit_commit;
        }
        error = presto_do_unlink(fset, dentry->d_parent, dentry, &info);
        dput(dentry);
        up(&nd.dentry->d_inode->i_sem);
        path_release(&nd);

        if (error != 0) {
                CERROR("presto_do_unlink(kml) failed: %d.\n", error);
                EXIT;
                goto exit_commit;
        }

        smlpath = izo_make_path(fset, "sml");
        if (smlpath == NULL) {
                error = -ENOMEM;
                CERROR("make_path() failed: ENOMEM\n");
                EXIT;
                goto exit_commit;
        }

        if ((error = izo_lookup_file(fset, smlpath, &nd)) != 0) {
                CERROR("izo_lookup_file(sml) failed: %d.\n", error);
                EXIT;
                goto exit_commit;
        }
        down(&nd.dentry->d_inode->i_sem);
        dentry = lookup_hash(&nd.last, nd.dentry);
        error = PTR_ERR(dentry);
        if (IS_ERR(dentry)) {
                up(&nd.dentry->d_inode->i_sem);
                path_release(&nd);
                CERROR("lookup_hash failed\n");
                EXIT;
                goto exit_commit;
        }
        error = presto_do_unlink(fset, dentry->d_parent, dentry, &info);
        dput(dentry);
        up(&nd.dentry->d_inode->i_sem);
        path_release(&nd);

        if (error != 0) {
                CERROR("presto_do_unlink(sml) failed: %d.\n", error);
                EXIT;
                goto exit_commit;
        }

        error = do_truncate_rename(fset, "kml_tmp", "kml");
        if (error != 0)
                CERROR("do_truncate_rename(kml_tmp, kml) failed: %d\n", error);
        error = do_truncate_rename(fset, "sml_tmp", "sml");
        if (error != 0)
                CERROR("do_truncate_rename(sml_tmp, sml) failed: %d\n", error);

        /* Write a new 'last_rcvd' record with the new KML offset */
        fset->fset_kml_logical_off += offset;
        CDEBUG(D_CACHE, "new kml_logical_offset: %Lu\n",
               fset->fset_kml_logical_off);
        if (presto_write_kml_logical_offset(fset) != 0) {
                CERROR("presto_write_kml_logical_offset failed\n");
        }

        presto_trans_commit(fset, handle);

        /* Everything was successful, so swap the KML file descriptors */
        filp_close(fset->fset_kml.fd_file, NULL);
        fset->fset_kml.fd_file = f;
        fset->fset_kml.fd_offset -= offset;
        fset->fset_kml.fd_truncating = 0;

        EXIT;
        return 0;

 exit_commit:
        presto_trans_commit(fset, handle);
        len = strlen("/.intermezzo/") + strlen(fset->fset_name) +strlen("sml");
        if (kmlpath != NULL)
                PRESTO_FREE(kmlpath, len);
        if (smlpath != NULL)
                PRESTO_FREE(smlpath, len);
        return error;
}

/* structure of an extended log record:

   buf-prefix  buf-body [string1 [string2 [string3]]] buf-suffix

   note: moves offset forward
*/
static inline int presto_write_record(struct file *f, loff_t *off,
                        const char *buf, size_t size,
                        const char *string1, int len1, 
                        const char *string2, int len2,
                        const char *string3, int len3)
{
        size_t prefix_size; 
        int rc;

        prefix_size = size - sizeof(struct kml_suffix);
        rc = presto_fwrite(f, buf, prefix_size, off);
        if ( rc != prefix_size ) {
                CERROR("Write error!\n");
                EXIT;
                return -EIO;
        }

        if  ( string1  && len1 ) {
                rc = presto_fwrite(f, string1, len1, off);
                if ( rc != len1 ) {
                        CERROR("Write error!\n");
                        EXIT;
                        return -EIO;
                }
        }

        if  ( string2 && len2 ) {
                rc = presto_fwrite(f, string2, len2, off);
                if ( rc != len2 ) {
                        CERROR("Write error!\n");
                        EXIT;
                        return -EIO;
                }
        }

        if  ( string3 && len3 ) {
                rc = presto_fwrite(f, string3, len3, off);
                if ( rc != len3 ) {
                        CERROR("Write error!\n");
                        EXIT;
                        return -EIO;
                }
        }

        rc = presto_fwrite(f, buf + prefix_size,
                           sizeof(struct kml_suffix), off);
        if ( rc != sizeof(struct kml_suffix) ) {
                CERROR("Write error!\n");
                EXIT;
                return -EIO;
        }
        return 0;
}


/*
 * rec->size must be valid prior to calling this function.
 *
 * had to export this for branch_reinter in kml_reint.c 
 */
int presto_log(struct presto_file_set *fset, struct rec_info *rec,
               const char *buf, size_t size,
               const char *string1, int len1, 
               const char *string2, int len2,
               const char *string3, int len3)
{
        int rc;
        struct presto_reservation_data rd;
        loff_t offset;
        struct presto_log_fd *fd;
        struct kml_suffix *s;
        int prefix_size; 

        ENTRY;

        /* buf is NULL when no_journal is in effect */
        if (!buf) {
                EXIT;
                return -EINVAL;
        }

        if (rec->is_kml) {
                fd = &fset->fset_kml;
        } else {
                fd = &fset->fset_lml;
        }

        presto_reserve_record(fset, fd, rec, &rd);

        if (rec->is_kml) {
                if (rec->offset < fset->fset_kml_logical_off) {
                        CERROR("record with pre-trunc offset.  tell phil.\n");
                        BUG();
                }
                offset = rec->offset - fset->fset_kml_logical_off;
        } else {
                offset = rec->offset;
        }

        /* now we know the record number */ 
        prefix_size = size - sizeof(struct kml_suffix);
        s = (struct kml_suffix *) (buf + prefix_size); 
        s->recno = cpu_to_le32(rec->recno); 

        rc = presto_write_record(fd->fd_file, &offset, buf, size, 
                                 string1, len1, string2, len2, string3, len3); 
        if (rc) {
                CERROR("presto: error writing record to %s\n",
                        rec->is_kml ? "KML" : "LML"); 
                return rc;
        }
        presto_release_record(fd, &rd);

        rc = presto_kml_dispatch(fset);

        EXIT;
        return rc;
}

/* read from the record at tail */
static int presto_last_record(struct presto_log_fd *fd, loff_t *size, 
                             loff_t *tail_offset, __u32 *recno, loff_t tail)
{
        struct kml_suffix suffix;
        int rc;
        loff_t zeroes;

        *recno = 0;
        *tail_offset = 0;
        *size = 0;
        
        if (tail < sizeof(struct kml_prefix_hdr) + sizeof(suffix)) {
                EXIT;
                return 0;
        }

        zeroes = tail - sizeof(int);
        while ( zeroes >= 0 ) {
                int data;
                rc = presto_fread(fd->fd_file, (char *)&data, sizeof(data), 
                                  &zeroes);
                if ( rc != sizeof(data) ) { 
                        rc = -EIO;
                        return rc;
                }
                if (data)
                        break;
                zeroes -= 2 * sizeof(data);
        }

        /* zeroes at the begining of file. this is needed to prevent
           presto_fread errors  -SHP
        */
        if (zeroes <= 0) return 0;
                       
        zeroes -= sizeof(suffix) + sizeof(int);
        rc = presto_fread(fd->fd_file, (char *)&suffix, sizeof(suffix), &zeroes);
        if ( rc != sizeof(suffix) ) {
                EXIT;
                return rc;
        }
        if ( suffix.len > 500 ) {
                CERROR("InterMezzo: Warning long record tail at %ld, rec tail_offset at %ld (size %d)\n", 
                        (long) zeroes, (long)*tail_offset, suffix.len); 
        }

        *recno = suffix.recno;
        *size = suffix.len;
        *tail_offset = zeroes;
        return 0;
}

static int izo_kml_last_recno(struct presto_log_fd *logfd)
{
        int rc; 
        loff_t size;
        loff_t tail_offset;
        int recno;
        loff_t tail = logfd->fd_file->f_dentry->d_inode->i_size;

        rc = presto_last_record(logfd, &size, &tail_offset, &recno, tail);
        if (rc != 0) {
                EXIT;
                return rc;
        }

        logfd->fd_offset = tail_offset;
        logfd->fd_recno = recno;
        CDEBUG(D_JOURNAL, "setting fset_kml->fd_recno to %d, offset  %Ld\n",
               recno, tail_offset); 
        EXIT;
        return 0;
}

struct file *izo_log_open(struct presto_file_set *fset, char *name, int flags)
{
        struct presto_cache *cache = fset->fset_cache;
        struct file *f;
        int error;
        ENTRY;

        f = izo_fset_open(fset, name, flags, 0644);
        error = PTR_ERR(f);
        if (IS_ERR(f)) {
                EXIT;
                return f;
        }

        error = -EINVAL;
        if ( cache != presto_get_cache(f->f_dentry->d_inode) ) {
                CERROR("InterMezzo: %s cache does not match fset cache!\n",name);
                fset->fset_kml.fd_file = NULL;
                filp_close(f, NULL);
                f = NULL;
                EXIT;
                return f;
        }

        if (cache->cache_filter &&  cache->cache_filter->o_trops &&
            cache->cache_filter->o_trops->tr_journal_data) {
                cache->cache_filter->o_trops->tr_journal_data
                        (f->f_dentry->d_inode);
        } else {
                CERROR("InterMezzo WARNING: no file data logging!\n"); 
        }

        EXIT;

        return f;
}

int izo_init_kml_file(struct presto_file_set *fset, struct presto_log_fd *logfd)
{
        int error = 0;
        struct file *f;

        ENTRY;
        if (logfd->fd_file) {
                CDEBUG(D_INODE, "fset already has KML open\n");
                EXIT;
                return 0;
        }

        logfd->fd_lock = RW_LOCK_UNLOCKED;
        INIT_LIST_HEAD(&logfd->fd_reservations); 
        f = izo_log_open(fset, "kml",  O_RDWR | O_CREAT);
        if (IS_ERR(f)) {
                error = PTR_ERR(f);
                return error;
        }

        logfd->fd_file = f;
        error = izo_kml_last_recno(logfd);

        if (error) {
                logfd->fd_file = NULL;
                filp_close(f, NULL);
                CERROR("InterMezzo: IO error in KML of fset %s\n",
                       fset->fset_name);
                EXIT;
                return error;
        }
        fset->fset_lento_off = logfd->fd_offset;
        fset->fset_lento_recno = logfd->fd_recno;

        EXIT;
        return error;
}

int izo_init_last_rcvd_file(struct presto_file_set *fset, struct presto_log_fd *logfd)
{
        int error = 0;
        struct file *f;
        struct rec_info recinfo;

        ENTRY;
        if (logfd->fd_file != NULL) {
                CDEBUG(D_INODE, "fset already has last_rcvd open\n");
                EXIT;
                return 0;
        }

        logfd->fd_lock = RW_LOCK_UNLOCKED;
        INIT_LIST_HEAD(&logfd->fd_reservations); 
        f = izo_log_open(fset, "last_rcvd", O_RDWR | O_CREAT);
        if (IS_ERR(f)) {
                error = PTR_ERR(f);
                return error;
        }

        logfd->fd_file = f;
        logfd->fd_offset = f->f_dentry->d_inode->i_size;

        error = izo_rep_cache_init(fset);

        if (presto_read_kml_logical_offset(&recinfo, fset) == 0) {
                fset->fset_kml_logical_off = recinfo.offset;
        } else {
                /* The 'last_rcvd' file doesn't contain a kml offset record,
                 * probably because we just created 'last_rcvd'.  Write one. */
                fset->fset_kml_logical_off = 0;
                presto_write_kml_logical_offset(fset);
        }

        EXIT;
        return error;
}

int izo_init_lml_file(struct presto_file_set *fset, struct presto_log_fd *logfd)
{
        int error = 0;
        struct file *f;

        ENTRY;
        if (logfd->fd_file) {
                CDEBUG(D_INODE, "fset already has lml open\n");
                EXIT;
                return 0;
        }

        logfd->fd_lock = RW_LOCK_UNLOCKED;
        INIT_LIST_HEAD(&logfd->fd_reservations); 
        f = izo_log_open(fset, "lml", O_RDWR | O_CREAT);
        if (IS_ERR(f)) {
                error = PTR_ERR(f);
                return error;
        }

        logfd->fd_file = f;
        logfd->fd_offset = f->f_dentry->d_inode->i_size;

        EXIT;
        return error;
}

/* Get the KML-offset record from the last_rcvd file */
int presto_read_kml_logical_offset(struct rec_info *recinfo,
                                   struct presto_file_set *fset)
{
        loff_t off;
        struct izo_rcvd_rec rec;
        char uuid[16] = {0};

        off = izo_rcvd_get(&rec, fset, uuid);
        if (off < 0)
                return -1;

        recinfo->offset = rec.lr_local_offset;
        return 0;
}

int presto_write_kml_logical_offset(struct presto_file_set *fset)
{
        loff_t rc;
        struct izo_rcvd_rec rec;
        char uuid[16] = {0};

        rc = izo_rcvd_get(&rec, fset, uuid);
        if (rc < 0)
                memset(&rec, 0, sizeof(rec));

        rec.lr_local_offset =
                cpu_to_le64(fset->fset_kml_logical_off);

        return izo_rcvd_write(fset, &rec);
}

struct file * presto_copy_kml_tail(struct presto_file_set *fset,
                                   unsigned long int start)
{
        struct file *f;
        int len;
        loff_t read_off, write_off, bytes;

        ENTRY;

        /* Copy the tail of 'kml' to 'kml_tmp' */
        f = izo_log_open(fset, "kml_tmp", O_RDWR);
        if (IS_ERR(f)) {
                EXIT;
                return f;
        }

        write_off = 0;
        read_off = start;
        bytes = fset->fset_kml.fd_offset - start;
        while (bytes > 0) {
                char buf[4096];
                int toread;

                if (bytes > sizeof(buf))
                        toread = sizeof(buf);
                else
                        toread = bytes;

                len = presto_fread(fset->fset_kml.fd_file, buf, toread,
                                   &read_off);
                if (len <= 0)
                        break;

                if (presto_fwrite(f, buf, len, &write_off) != len) {
                        filp_close(f, NULL);
                        EXIT;
                        return ERR_PTR(-EIO);
                }

                bytes -= len;
        }

        EXIT;
        return f;
}


/* LML records here */
/* this writes an LML record to the LML file (rec->is_kml =0)  */
int presto_write_lml_close(struct rec_info *rec,
                           struct presto_file_set *fset, 
                           struct file *file,
                           __u64 remote_ino,
                           __u64 remote_generation,
                           struct presto_version *remote_version,
                           struct presto_version *new_file_ver)
{
        int opcode = KML_OPCODE_CLOSE;
        char *buffer;
        struct dentry *dentry = file->f_dentry; 
        __u64 ino;
        __u32 pathlen;
        char *path;
        __u32 generation;
        int size;
        char *logrecord;
        char record[292];
        struct dentry *root;
        int error;

        ENTRY;

        if ( presto_no_journal(fset) ) {
          EXIT;
          return 0;
        }
        root = fset->fset_dentry;

        BUFF_ALLOC(buffer, NULL);
        path = presto_path(dentry, root, buffer, PAGE_SIZE);
        CDEBUG(D_INODE, "Path: %s\n", path);
        pathlen = cpu_to_le32(MYPATHLEN(buffer, path));
        ino = cpu_to_le64(dentry->d_inode->i_ino);
        generation = cpu_to_le32(dentry->d_inode->i_generation);
        size =  sizeof(__u32) * current->ngroups + 
                sizeof(struct kml_prefix_hdr) + sizeof(*new_file_ver) +
                sizeof(ino) + sizeof(generation) + sizeof(pathlen) +
                sizeof(remote_ino) + sizeof(remote_generation) + 
                sizeof(remote_version) + sizeof(rec->offset) +
                sizeof(struct kml_suffix);

        if ( size > sizeof(record) )
                CERROR("InterMezzo: BUFFER OVERFLOW in %s!\n", __FUNCTION__);
        
        rec->is_kml = 0;
        rec->size = size + size_round(le32_to_cpu(pathlen));

        logrecord = journal_log_prefix(record, opcode, rec);
        logrecord = log_version(logrecord, new_file_ver);
        logrecord = logit(logrecord, &ino, sizeof(ino));
        logrecord = logit(logrecord, &generation, sizeof(generation));
        logrecord = logit(logrecord, &pathlen, sizeof(pathlen));
        logrecord = logit(logrecord, &remote_ino, sizeof(remote_ino));
        logrecord = logit(logrecord, &remote_generation,
                          sizeof(remote_generation));
        logrecord = log_version(logrecord, remote_version);
        logrecord = logit(logrecord, &rec->offset, sizeof(rec->offset));
        logrecord = journal_log_suffix(logrecord, record, fset, dentry, rec);

        error = presto_log(fset, rec, record, size,
                           path, size_round(le32_to_cpu(pathlen)),
                           NULL, 0, NULL, 0);

        BUFF_FREE(buffer);

        EXIT;
        return error;
}

/* 
 * Check if the given record is at the end of the file. If it is, truncate
 * the lml to the record's offset, removing it. Repeat on prior record,
 * until we reach an active record or a reserved record (as defined by the
 * reservations list).
 */
static int presto_truncate_lml_tail(struct presto_file_set *fset)
{
        loff_t lml_tail;
        loff_t lml_last_rec;
        loff_t lml_last_recsize;
        loff_t local_offset;
        int recno;
        struct kml_prefix_hdr prefix;
        struct inode *inode = fset->fset_lml.fd_file->f_dentry->d_inode;
        void *handle;
        int rc;

        ENTRY;
        /* If someone else is already truncating the LML, return. */
        write_lock(&fset->fset_lml.fd_lock); 
        if (fset->fset_lml.fd_truncating == 1 ) {
                write_unlock(&fset->fset_lml.fd_lock); 
                EXIT;
                return 0;
        }
        /* someone is about to write to the end of the LML */ 
        if ( !list_empty(&fset->fset_lml.fd_reservations) ) {
                write_unlock(&fset->fset_lml.fd_lock); 
                EXIT;
                return 0;
        }
       lml_tail = fset->fset_lml.fd_file->f_dentry->d_inode->i_size;
       /* Nothing to truncate?*/
       if (lml_tail == 0) {
                write_unlock(&fset->fset_lml.fd_lock); 
                EXIT;
                return 0;
       }
       fset->fset_lml.fd_truncating = 1;
       write_unlock(&fset->fset_lml.fd_lock); 

       presto_last_record(&fset->fset_lml, &lml_last_recsize,
                          &lml_last_rec, &recno, lml_tail);
       /* Do we have a record to check? If not we have zeroes at the
          beginning of the file. -SHP
       */
       if (lml_last_recsize != 0) {
                local_offset = lml_last_rec - lml_last_recsize;
                rc = presto_fread(fset->fset_lml.fd_file, (char *)&prefix,  
                                        sizeof(prefix), &local_offset); 
                if (rc != sizeof(prefix)) {
                        EXIT;
                        goto tr_out;
                }
       
                if ( prefix.opcode != KML_OPCODE_NOOP ) {
                        EXIT;
                        rc = 0;
                        /* We may have zeroes at the end of the file, should
                           we clear them out? -SHP
                        */
                        goto tr_out;
                }
        } else 
                lml_last_rec=0;

        handle = presto_trans_start(fset, inode, KML_OPCODE_TRUNC);
        if ( IS_ERR(handle) ) {
                EXIT;
                rc = -ENOMEM;
                goto tr_out;
        }

        rc = izo_do_truncate(fset, fset->fset_lml.fd_file->f_dentry, 
                                lml_last_rec - lml_last_recsize, lml_tail);
        presto_trans_commit(fset, handle); 
        if ( rc == 0 ) {
                rc = 1;
        }
        EXIT;

 tr_out:
        CDEBUG(D_JOURNAL, "rc = %d\n", rc);
        write_lock(&fset->fset_lml.fd_lock);
        fset->fset_lml.fd_truncating = 0;
        write_unlock(&fset->fset_lml.fd_lock);
        return rc;
}

int presto_truncate_lml(struct presto_file_set *fset)
{
        int rc; 
        ENTRY;
        
        while ( (rc = presto_truncate_lml_tail(fset)) > 0);
        if ( rc < 0 && rc != -EALREADY) {
                CERROR("truncate_lml error %d\n", rc); 
        }
        EXIT;
        return rc;
}

int presto_clear_lml_close(struct presto_file_set *fset, loff_t lml_offset)
{
        int rc;
        struct kml_prefix_hdr record;
        loff_t offset = lml_offset;

        ENTRY;

        if ( presto_no_journal(fset) ) {
                EXIT;
                return 0;
        }

        CDEBUG(D_JOURNAL, "reading prefix: off %ld, size %d\n", 
               (long)lml_offset, sizeof(record));
        rc = presto_fread(fset->fset_lml.fd_file, (char *)&record,
                          sizeof(record), &offset);

        if ( rc != sizeof(record) ) {
                CERROR("presto: clear_lml io error %d\n", rc); 
                EXIT;
                return -EIO;
        }

        /* overwrite the prefix */ 
        CDEBUG(D_JOURNAL, "overwriting prefix: off %ld\n", (long)lml_offset);
        record.opcode = KML_OPCODE_NOOP;
        offset = lml_offset;
        /* note: this does just a single transaction in the cache */
        rc = presto_fwrite(fset->fset_lml.fd_file, (char *)(&record), 
                              sizeof(record), &offset);
        if ( rc != sizeof(record) ) {
                EXIT;
                return -EIO;
        }

        EXIT;
        return 0; 
}



/* now a journal function for every operation */

int presto_journal_setattr(struct rec_info *rec, struct presto_file_set *fset,
                           struct dentry *dentry, struct presto_version *old_ver,
                           struct izo_rollback_data *rb, struct iattr *iattr)
{
        int opcode = KML_OPCODE_SETATTR;
        char *buffer, *path, *logrecord, record[316];
        struct dentry *root;
        __u32 uid, gid, mode, valid, flags, pathlen;
        __u64 fsize, mtime, ctime;
        int error, size;

        ENTRY;
        if ( presto_no_journal(fset) ) {
                EXIT;
                return 0;
        }

        if (!dentry->d_inode || (dentry->d_inode->i_nlink == 0) 
            || ((dentry->d_parent != dentry) && list_empty(&dentry->d_hash))) {
                EXIT;
                return 0;
        }

        root = fset->fset_dentry;

        BUFF_ALLOC(buffer, NULL);
        path = presto_path(dentry, root, buffer, PAGE_SIZE);
        pathlen = cpu_to_le32(MYPATHLEN(buffer, path));
        size =  sizeof(__u32) * current->ngroups + 
                sizeof(struct kml_prefix_hdr) + sizeof(*old_ver) +
                sizeof(valid) + sizeof(mode) + sizeof(uid) + sizeof(gid) +
                sizeof(fsize) + sizeof(mtime) + sizeof(ctime) + sizeof(flags) +
                sizeof(pathlen) + sizeof(*rb) + sizeof(struct kml_suffix);

        if ( size > sizeof(record) )
                CERROR("InterMezzo: BUFFER OVERFLOW in %s!\n", __FUNCTION__);

        /* Only journal one kind of mtime, and not atime at all.  Also don't
         * journal bogus data in iattr, to make the journal more compressible.
         */
        if (iattr->ia_valid & ATTR_MTIME_SET)
                iattr->ia_valid = iattr->ia_valid | ATTR_MTIME;
        valid = cpu_to_le32(iattr->ia_valid & ~(ATTR_ATIME | ATTR_MTIME_SET |
                                                ATTR_ATIME_SET));
        mode = iattr->ia_valid & ATTR_MODE ? cpu_to_le32(iattr->ia_mode): 0;
        uid = iattr->ia_valid & ATTR_UID ? cpu_to_le32(iattr->ia_uid): 0;
        gid = iattr->ia_valid & ATTR_GID ? cpu_to_le32(iattr->ia_gid): 0;
        fsize = iattr->ia_valid & ATTR_SIZE ? cpu_to_le64(iattr->ia_size): 0;
        mtime = iattr->ia_valid & ATTR_MTIME ? cpu_to_le64(iattr->ia_mtime): 0;
        ctime = iattr->ia_valid & ATTR_CTIME ? cpu_to_le64(iattr->ia_ctime): 0;
        flags = iattr->ia_valid & ATTR_ATTR_FLAG ?
                cpu_to_le32(iattr->ia_attr_flags): 0;

        rec->is_kml = 1;
        rec->size = size + size_round(le32_to_cpu(pathlen));

        logrecord = journal_log_prefix(record, opcode, rec);
        logrecord = log_version(logrecord, old_ver);
        logrecord = logit(logrecord, &valid, sizeof(valid));
        logrecord = logit(logrecord, &mode, sizeof(mode));
        logrecord = logit(logrecord, &uid, sizeof(uid));
        logrecord = logit(logrecord, &gid, sizeof(gid));
        logrecord = logit(logrecord, &fsize, sizeof(fsize));
        logrecord = logit(logrecord, &mtime, sizeof(mtime));
        logrecord = logit(logrecord, &ctime, sizeof(ctime));
        logrecord = logit(logrecord, &flags, sizeof(flags));
        logrecord = log_rollback(logrecord, rb);
        logrecord = logit(logrecord, &pathlen, sizeof(pathlen));
        logrecord = journal_log_suffix(logrecord, record, fset, dentry, rec);

        error = presto_log(fset, rec, record, size,
                           path, size_round(le32_to_cpu(pathlen)),
                           NULL, 0, NULL, 0);

        BUFF_FREE(buffer);
        EXIT;
        return error;
}

int presto_get_fileid(int minor, struct presto_file_set *fset,
                      struct dentry *dentry)
{
        int opcode = KML_OPCODE_GET_FILEID;
        struct rec_info rec;
        char *buffer, *path, *logrecord, record[4096]; /*include path*/
        struct dentry *root;
        __u32 uid, gid, pathlen;
        int error, size;
        struct kml_suffix *suffix;

        ENTRY;

        root = fset->fset_dentry;

        uid = cpu_to_le32(dentry->d_inode->i_uid);
        gid = cpu_to_le32(dentry->d_inode->i_gid);
        BUFF_ALLOC(buffer, NULL);
        path = presto_path(dentry, root, buffer, PAGE_SIZE);
        pathlen = cpu_to_le32(MYPATHLEN(buffer, path));
        size =  sizeof(__u32) * current->ngroups + 
                sizeof(struct kml_prefix_hdr) + sizeof(pathlen) +
                size_round(le32_to_cpu(pathlen)) +
                sizeof(struct kml_suffix);

        CDEBUG(D_FILE, "kml size: %d\n", size);
        if ( size > sizeof(record) )
                CERROR("InterMezzo: BUFFER OVERFLOW in %s!\n", __FUNCTION__);

        memset(&rec, 0, sizeof(rec));
        rec.is_kml = 1;
        rec.size = size;

        logrecord = journal_log_prefix(record, opcode, &rec);
        logrecord = logit(logrecord, &pathlen, sizeof(pathlen));
        logrecord = logit(logrecord, path, size_round(le32_to_cpu(pathlen)));
        suffix = (struct kml_suffix *)logrecord;
        logrecord = journal_log_suffix(logrecord, record, fset, dentry, &rec);
        /* journal_log_suffix expects journal_log to set this */
        suffix->recno = 0;

        CDEBUG(D_FILE, "actual kml size: %d\n", logrecord - record);
        CDEBUG(D_FILE, "get fileid: uid %d, gid %d, path: %s\n", uid, gid,path);

        error = izo_upc_get_fileid(minor, size, record, 
                                   size_round(le32_to_cpu(pathlen)), path,
                                   fset->fset_name);

        BUFF_FREE(buffer);
        EXIT;
        return error;
}

int presto_journal_create(struct rec_info *rec, struct presto_file_set *fset,
                          struct dentry *dentry,
                          struct presto_version *tgt_dir_ver,
                          struct presto_version *new_file_ver, int mode)
{
        int opcode = KML_OPCODE_CREATE;
        char *buffer, *path, *logrecord, record[292];
        struct dentry *root;
        __u32 uid, gid, lmode, pathlen;
        int error, size;

        ENTRY;
        if ( presto_no_journal(fset) ) {
                EXIT;
                return 0;
        }

        root = fset->fset_dentry;

        uid = cpu_to_le32(dentry->d_inode->i_uid);
        gid = cpu_to_le32(dentry->d_inode->i_gid);
        lmode = cpu_to_le32(mode);
 
        BUFF_ALLOC(buffer, NULL);
        path = presto_path(dentry, root, buffer, PAGE_SIZE);
        pathlen = cpu_to_le32(MYPATHLEN(buffer, path));
        size =  sizeof(__u32) * current->ngroups + 
                sizeof(struct kml_prefix_hdr) + 3 * sizeof(*tgt_dir_ver) +
                sizeof(lmode) + sizeof(uid) + sizeof(gid) + sizeof(pathlen) +
                sizeof(struct kml_suffix);

        if ( size > sizeof(record) )
                CERROR("InterMezzo: BUFFER OVERFLOW in %s!\n", __FUNCTION__);

        rec->is_kml = 1;
        rec->size = size + size_round(le32_to_cpu(pathlen));

        logrecord = journal_log_prefix(record, opcode, rec);
        logrecord = log_version(logrecord, tgt_dir_ver);
        logrecord = log_dentry_version(logrecord, dentry->d_parent);
        logrecord = log_version(logrecord, new_file_ver);
        logrecord = logit(logrecord, &lmode, sizeof(lmode));
        logrecord = logit(logrecord, &uid, sizeof(uid));
        logrecord = logit(logrecord, &gid, sizeof(gid));
        logrecord = logit(logrecord, &pathlen, sizeof(pathlen));
        logrecord = journal_log_suffix(logrecord, record, fset, dentry, rec);

        error = presto_log(fset, rec, record, size,
                           path, size_round(le32_to_cpu(pathlen)),
                           NULL, 0, NULL, 0);

        BUFF_FREE(buffer);
        EXIT;
        return error;
}

int presto_journal_symlink(struct rec_info *rec, struct presto_file_set *fset,
                           struct dentry *dentry, const char *target,
                           struct presto_version *tgt_dir_ver,
                           struct presto_version *new_link_ver)
{
        int opcode = KML_OPCODE_SYMLINK;
        char *buffer, *path, *logrecord, record[292];
        struct dentry *root;
        __u32 uid, gid, pathlen;
        __u32 targetlen = cpu_to_le32(strlen(target));
        int error, size;

        ENTRY;
        if ( presto_no_journal(fset) ) {
                EXIT;
                return 0;
        }

        root = fset->fset_dentry;

        uid = cpu_to_le32(dentry->d_inode->i_uid);
        gid = cpu_to_le32(dentry->d_inode->i_gid);

        BUFF_ALLOC(buffer, NULL);
        path = presto_path(dentry, root, buffer, PAGE_SIZE);
        pathlen = cpu_to_le32(MYPATHLEN(buffer, path));
        size =  sizeof(__u32) * current->ngroups + 
                sizeof(struct kml_prefix_hdr) + 3 * sizeof(*tgt_dir_ver) +
                sizeof(uid) + sizeof(gid) + sizeof(pathlen) +
                sizeof(targetlen) + sizeof(struct kml_suffix);

        if ( size > sizeof(record) )
                CERROR("InterMezzo: BUFFER OVERFLOW in %s!\n", __FUNCTION__);

        rec->is_kml = 1;
        rec->size = size + size_round(le32_to_cpu(pathlen)) +
                size_round(le32_to_cpu(targetlen));

        logrecord = journal_log_prefix(record, opcode, rec);
        logrecord = log_version(logrecord, tgt_dir_ver);
        logrecord = log_dentry_version(logrecord, dentry->d_parent);
        logrecord = log_version(logrecord, new_link_ver);
        logrecord = logit(logrecord, &uid, sizeof(uid));
        logrecord = logit(logrecord, &gid, sizeof(gid));
        logrecord = logit(logrecord, &pathlen, sizeof(pathlen));
        logrecord = logit(logrecord, &targetlen, sizeof(targetlen));
        logrecord = journal_log_suffix(logrecord, record, fset, dentry, rec);

        error = presto_log(fset, rec, record, size,
                           path, size_round(le32_to_cpu(pathlen)),
                           target, size_round(le32_to_cpu(targetlen)),
                           NULL, 0);

        BUFF_FREE(buffer);
        EXIT;
        return error;
}

int presto_journal_mkdir(struct rec_info *rec, struct presto_file_set *fset,
                         struct dentry *dentry,
                         struct presto_version *tgt_dir_ver,
                         struct presto_version *new_dir_ver, int mode)
{
        int opcode = KML_OPCODE_MKDIR;
        char *buffer, *path, *logrecord, record[292];
        struct dentry *root;
        __u32 uid, gid, lmode, pathlen;
        int error, size;

        ENTRY;
        if ( presto_no_journal(fset) ) {
                EXIT;
                return 0;
        }

        root = fset->fset_dentry;

        uid = cpu_to_le32(dentry->d_inode->i_uid);
        gid = cpu_to_le32(dentry->d_inode->i_gid);
        lmode = cpu_to_le32(mode);

        BUFF_ALLOC(buffer, NULL);
        path = presto_path(dentry, root, buffer, PAGE_SIZE);
        pathlen = cpu_to_le32(MYPATHLEN(buffer, path));
        size = sizeof(__u32) * current->ngroups + 
                sizeof(struct kml_prefix_hdr) + 3 * sizeof(*tgt_dir_ver) +
                sizeof(lmode) + sizeof(uid) + sizeof(gid) + sizeof(pathlen) +
                sizeof(struct kml_suffix);

        if ( size > sizeof(record) )
                CERROR("InterMezzo: BUFFER OVERFLOW in %s!\n", __FUNCTION__);

        rec->is_kml = 1;
        rec->size = size + size_round(le32_to_cpu(pathlen));
        logrecord = journal_log_prefix(record, opcode, rec);

        logrecord = log_version(logrecord, tgt_dir_ver);
        logrecord = log_dentry_version(logrecord, dentry->d_parent);
        logrecord = log_version(logrecord, new_dir_ver);
        logrecord = logit(logrecord, &lmode, sizeof(lmode));
        logrecord = logit(logrecord, &uid, sizeof(uid));
        logrecord = logit(logrecord, &gid, sizeof(gid));
        logrecord = logit(logrecord, &pathlen, sizeof(pathlen));
        logrecord = journal_log_suffix(logrecord, record, fset, dentry, rec);

        error = presto_log(fset, rec, record, size,
                           path, size_round(le32_to_cpu(pathlen)),
                           NULL, 0, NULL, 0);

        BUFF_FREE(buffer);
        EXIT;
        return error;
}


int
presto_journal_rmdir(struct rec_info *rec, struct presto_file_set *fset,
                     struct dentry *dir, struct presto_version *tgt_dir_ver,
                     struct presto_version *old_dir_ver,
                     struct izo_rollback_data *rb, int len, const char *name)
{
        int opcode = KML_OPCODE_RMDIR;
        char *buffer, *path, *logrecord, record[316];
        __u32 pathlen, llen;
        struct dentry *root;
        int error, size;

        ENTRY;
        if ( presto_no_journal(fset) ) {
                EXIT;
                return 0;
        }

        root = fset->fset_dentry;

        llen = cpu_to_le32(len);
        BUFF_ALLOC(buffer, NULL);
        path = presto_path(dir, root, buffer, PAGE_SIZE);
        pathlen = cpu_to_le32(MYPATHLEN(buffer, path));
        size =  sizeof(__u32) * current->ngroups + 
                sizeof(struct kml_prefix_hdr) + 3 * sizeof(*tgt_dir_ver) +
                sizeof(pathlen) + sizeof(llen) + sizeof(*rb) +
                sizeof(struct kml_suffix);

        if ( size > sizeof(record) )
                CERROR("InterMezzo: BUFFER OVERFLOW in %s!\n", __FUNCTION__);

        CDEBUG(D_JOURNAL, "path: %s (%d), name: %s (%d), size %d\n",
               path, pathlen, name, len, size);

        rec->is_kml = 1;
        rec->size = size + size_round(le32_to_cpu(pathlen)) + 
                size_round(len);

        logrecord = journal_log_prefix(record, opcode, rec);
        logrecord = log_version(logrecord, tgt_dir_ver);
        logrecord = log_dentry_version(logrecord, dir);
        logrecord = log_version(logrecord, old_dir_ver);
        logrecord = logit(logrecord, rb, sizeof(*rb));
        logrecord = logit(logrecord, &pathlen, sizeof(pathlen));
        logrecord = logit(logrecord, &llen, sizeof(llen));
        logrecord = journal_log_suffix(logrecord, record, fset, dir, rec);
        error = presto_log(fset, rec, record, size,
                           path, size_round(le32_to_cpu(pathlen)),
                           name, size_round(len),
                           NULL, 0);

        BUFF_FREE(buffer);
        EXIT;
        return error;
}


int
presto_journal_mknod(struct rec_info *rec, struct presto_file_set *fset,
                     struct dentry *dentry, struct presto_version *tgt_dir_ver,
                     struct presto_version *new_node_ver, int mode,
                     int dmajor, int dminor )
{
        int opcode = KML_OPCODE_MKNOD;
        char *buffer, *path, *logrecord, record[292];
        struct dentry *root;
        __u32 uid, gid, lmode, lmajor, lminor, pathlen;
        int error, size;

        ENTRY;
        if ( presto_no_journal(fset) ) {
                EXIT;
                return 0;
        }

        root = fset->fset_dentry;

        uid = cpu_to_le32(dentry->d_inode->i_uid);
        gid = cpu_to_le32(dentry->d_inode->i_gid);
        lmode = cpu_to_le32(mode);
        lmajor = cpu_to_le32(dmajor);
        lminor = cpu_to_le32(dminor);

        BUFF_ALLOC(buffer, NULL);
        path = presto_path(dentry, root, buffer, PAGE_SIZE);
        pathlen = cpu_to_le32(MYPATHLEN(buffer, path));
        size = sizeof(__u32) * current->ngroups + 
                sizeof(struct kml_prefix_hdr) + 3 * sizeof(*tgt_dir_ver) +
                sizeof(lmode) + sizeof(uid) + sizeof(gid) + sizeof(lmajor) +
                sizeof(lminor) + sizeof(pathlen) +
                sizeof(struct kml_suffix);

        if ( size > sizeof(record) )
                CERROR("InterMezzo: BUFFER OVERFLOW in %s!\n", __FUNCTION__);

        rec->is_kml = 1;
        rec->size = size + size_round(le32_to_cpu(pathlen));

        logrecord = journal_log_prefix(record, opcode, rec);
        logrecord = log_version(logrecord, tgt_dir_ver);
        logrecord = log_dentry_version(logrecord, dentry->d_parent);
        logrecord = log_version(logrecord, new_node_ver);
        logrecord = logit(logrecord, &lmode, sizeof(lmode));
        logrecord = logit(logrecord, &uid, sizeof(uid));
        logrecord = logit(logrecord, &gid, sizeof(gid));
        logrecord = logit(logrecord, &lmajor, sizeof(lmajor));
        logrecord = logit(logrecord, &lminor, sizeof(lminor));
        logrecord = logit(logrecord, &pathlen, sizeof(pathlen));
        logrecord = journal_log_suffix(logrecord, record, fset, dentry, rec);

        error = presto_log(fset, rec, record, size,
                           path, size_round(le32_to_cpu(pathlen)),
                           NULL, 0, NULL, 0);

        BUFF_FREE(buffer);
        EXIT;
        return error;
}

int
presto_journal_link(struct rec_info *rec, struct presto_file_set *fset,
                    struct dentry *src, struct dentry *tgt,
                    struct presto_version *tgt_dir_ver,
                    struct presto_version *new_link_ver)
{
        int opcode = KML_OPCODE_LINK;
        char *buffer, *srcbuffer, *path, *srcpath, *logrecord, record[292];
        __u32 pathlen, srcpathlen;
        struct dentry *root;
        int error, size;

        ENTRY;
        if ( presto_no_journal(fset) ) {
                EXIT;
                return 0;
        }

        root = fset->fset_dentry;

        BUFF_ALLOC(srcbuffer, NULL);
        srcpath = presto_path(src, root, srcbuffer, PAGE_SIZE);
        srcpathlen = cpu_to_le32(MYPATHLEN(srcbuffer, srcpath));

        BUFF_ALLOC(buffer, srcbuffer);
        path = presto_path(tgt, root, buffer, PAGE_SIZE);
        pathlen = cpu_to_le32(MYPATHLEN(buffer, path));
        size =  sizeof(__u32) * current->ngroups + 
                sizeof(struct kml_prefix_hdr) + 3 * sizeof(*tgt_dir_ver) +
                sizeof(srcpathlen) + sizeof(pathlen) +
                sizeof(struct kml_suffix);

        if ( size > sizeof(record) )
                CERROR("InterMezzo: BUFFER OVERFLOW in %s!\n", __FUNCTION__);

        rec->is_kml = 1;
        rec->size = size + size_round(le32_to_cpu(pathlen)) + 
                size_round(le32_to_cpu(srcpathlen));

        logrecord = journal_log_prefix(record, opcode, rec);
        logrecord = log_version(logrecord, tgt_dir_ver);
        logrecord = log_dentry_version(logrecord, tgt->d_parent);
        logrecord = log_version(logrecord, new_link_ver);
        logrecord = logit(logrecord, &srcpathlen, sizeof(srcpathlen));
        logrecord = logit(logrecord, &pathlen, sizeof(pathlen));
        logrecord = journal_log_suffix(logrecord, record, fset, tgt, rec);

        error = presto_log(fset, rec, record, size,
                           srcpath, size_round(le32_to_cpu(srcpathlen)),
                           path, size_round(le32_to_cpu(pathlen)),
                           NULL, 0);

        BUFF_FREE(srcbuffer);
        BUFF_FREE(buffer);
        EXIT;
        return error;
}


int presto_journal_rename(struct rec_info *rec, struct presto_file_set *fset,
                          struct dentry *src, struct dentry *tgt,
                          struct presto_version *src_dir_ver,
                          struct presto_version *tgt_dir_ver)
{
        int opcode = KML_OPCODE_RENAME;
        char *buffer, *srcbuffer, *path, *srcpath, *logrecord, record[292];
        __u32 pathlen, srcpathlen;
        struct dentry *root;
        int error, size;

        ENTRY;
        if ( presto_no_journal(fset) ) {
                EXIT;
                return 0;
        }

        root = fset->fset_dentry;

        BUFF_ALLOC(srcbuffer, NULL);
        srcpath = presto_path(src, root, srcbuffer, PAGE_SIZE);
        srcpathlen = cpu_to_le32(MYPATHLEN(srcbuffer, srcpath));

        BUFF_ALLOC(buffer, srcbuffer);
        path = presto_path(tgt, root, buffer, PAGE_SIZE);
        pathlen = cpu_to_le32(MYPATHLEN(buffer, path));
        size =  sizeof(__u32) * current->ngroups + 
                sizeof(struct kml_prefix_hdr) + 4 * sizeof(*src_dir_ver) +
                sizeof(srcpathlen) + sizeof(pathlen) +
                sizeof(struct kml_suffix);

        if ( size > sizeof(record) )
                CERROR("InterMezzo: BUFFER OVERFLOW in %s!\n", __FUNCTION__);

        rec->is_kml = 1;
        rec->size = size + size_round(le32_to_cpu(pathlen)) + 
                size_round(le32_to_cpu(srcpathlen));

        logrecord = journal_log_prefix(record, opcode, rec);
        logrecord = log_version(logrecord, src_dir_ver);
        logrecord = log_dentry_version(logrecord, src->d_parent);
        logrecord = log_version(logrecord, tgt_dir_ver);
        logrecord = log_dentry_version(logrecord, tgt->d_parent);
        logrecord = logit(logrecord, &srcpathlen, sizeof(srcpathlen));
        logrecord = logit(logrecord, &pathlen, sizeof(pathlen));
        logrecord = journal_log_suffix(logrecord, record, fset, tgt, rec);

        error = presto_log(fset, rec, record, size,
                           srcpath, size_round(le32_to_cpu(srcpathlen)),
                           path, size_round(le32_to_cpu(pathlen)),
                           NULL, 0);

        BUFF_FREE(buffer);
        BUFF_FREE(srcbuffer);
        EXIT;
        return error;
}

int presto_journal_unlink(struct rec_info *rec, struct presto_file_set *fset,
                          struct dentry *dir, struct presto_version *tgt_dir_ver,
                          struct presto_version *old_file_ver,
                          struct izo_rollback_data *rb, struct dentry *dentry,
                          char *old_target, int old_targetlen)
{
        int opcode = KML_OPCODE_UNLINK;
        char *buffer, *path, *logrecord, record[316];
        const char *name;
        __u32 pathlen, llen;
        struct dentry *root;
        int error, size, len;

        ENTRY;
        if ( presto_no_journal(fset) ) {
                EXIT;
                return 0;
        }

        root = fset->fset_dentry;

        name = dentry->d_name.name;
        len = dentry->d_name.len;

        llen = cpu_to_le32(len);
        BUFF_ALLOC(buffer, NULL);
        path = presto_path(dir, root, buffer, PAGE_SIZE);
        pathlen = cpu_to_le32(MYPATHLEN(buffer, path));
        size = sizeof(__u32) * current->ngroups + 
                sizeof(struct kml_prefix_hdr) + 3 * sizeof(*tgt_dir_ver) +
                sizeof(pathlen) + sizeof(llen) + sizeof(*rb) +
                sizeof(old_targetlen) + sizeof(struct kml_suffix);

        if ( size > sizeof(record) )
                CERROR("InterMezzo: BUFFER OVERFLOW in %s!\n", __FUNCTION__);

        rec->is_kml = 1;
        rec->size = size + size_round(le32_to_cpu(pathlen)) + size_round(len) +
                size_round(old_targetlen);

        logrecord = journal_log_prefix(record, opcode, rec);
        logrecord = log_version(logrecord, tgt_dir_ver);
        logrecord = log_dentry_version(logrecord, dir);
        logrecord = log_version(logrecord, old_file_ver);
        logrecord = log_rollback(logrecord, rb);
        logrecord = logit(logrecord, &pathlen, sizeof(pathlen));
        logrecord = logit(logrecord, &llen, sizeof(llen));
        logrecord = logit(logrecord, &old_targetlen, sizeof(old_targetlen));
        logrecord = journal_log_suffix(logrecord, record, fset, dir, rec);

        error = presto_log(fset, rec, record, size,
                           path, size_round(le32_to_cpu(pathlen)),
                           name, size_round(len),
                           old_target, size_round(old_targetlen));

        BUFF_FREE(buffer);
        EXIT;
        return error;
}

int
presto_journal_close(struct rec_info *rec, struct presto_file_set *fset,
                     struct file *file, struct dentry *dentry,
                     struct presto_version *old_file_ver,
                     struct presto_version *new_file_ver)
{
        int opcode = KML_OPCODE_CLOSE;
        struct presto_file_data *fd;
        char *buffer, *path, *logrecord, record[316];
        struct dentry *root;
        int error, size, i;
        __u32 pathlen, generation;
        __u64 ino;
        __u32 open_fsuid;
        __u32 open_fsgid;
        __u32 open_ngroups;
        __u32 open_groups[NGROUPS_MAX];
        __u32 open_mode;
        __u32 open_uid;
        __u32 open_gid;

        ENTRY;

        if ( presto_no_journal(fset) ) {
                EXIT;
                return 0;
        }

        if (!dentry->d_inode || (dentry->d_inode->i_nlink == 0) 
            || ((dentry->d_parent != dentry) && list_empty(&dentry->d_hash))) {
                EXIT;
                return 0;
        }

        root = fset->fset_dentry;

        fd = (struct presto_file_data *)file->private_data;
        if (fd) {
                open_ngroups = fd->fd_ngroups;
                for (i = 0; i < fd->fd_ngroups; i++)
                        open_groups[i] = (__u32) fd->fd_groups[i];
                open_mode = fd->fd_mode;
                open_uid = fd->fd_uid;
                open_gid = fd->fd_gid;
                open_fsuid = fd->fd_fsuid;
                open_fsgid = fd->fd_fsgid;
        } else {
                open_ngroups = current->ngroups;
                for (i=0; i<current->ngroups; i++)
                        open_groups[i] =  (__u32) current->groups[i]; 
                open_mode = dentry->d_inode->i_mode;
                open_uid = dentry->d_inode->i_uid;
                open_gid = dentry->d_inode->i_gid;
                open_fsuid = current->fsuid;
                open_fsgid = current->fsgid;
        }
        BUFF_ALLOC(buffer, NULL);
        path = presto_path(dentry, root, buffer, PAGE_SIZE);
        pathlen = cpu_to_le32(MYPATHLEN(buffer, path));
        ino = cpu_to_le64(dentry->d_inode->i_ino);
        generation = cpu_to_le32(dentry->d_inode->i_generation);
        size =  sizeof(__u32) * open_ngroups +
                sizeof(open_mode) + sizeof(open_uid) + sizeof(open_gid) +
                sizeof(struct kml_prefix_hdr) + sizeof(*old_file_ver) +
                sizeof(*new_file_ver) + sizeof(ino) + sizeof(generation) +
                sizeof(pathlen) + sizeof(struct kml_suffix);

        if ( size > sizeof(record) )
                CERROR("InterMezzo: BUFFER OVERFLOW in %s!\n", __FUNCTION__);

        rec->is_kml = 1;
        rec->size = size + size_round(le32_to_cpu(pathlen));

        logrecord = journal_log_prefix_with_groups_and_ids(
                record, opcode, rec, open_ngroups, open_groups,
                open_fsuid, open_fsgid);
        logrecord = logit(logrecord, &open_mode, sizeof(open_mode));
        logrecord = logit(logrecord, &open_uid, sizeof(open_uid));
        logrecord = logit(logrecord, &open_gid, sizeof(open_gid));
        logrecord = log_version(logrecord, old_file_ver);
        logrecord = log_version(logrecord, new_file_ver);
        logrecord = logit(logrecord, &ino, sizeof(ino));
        logrecord = logit(logrecord, &generation, sizeof(generation));
        logrecord = logit(logrecord, &pathlen, sizeof(pathlen));
        logrecord = journal_log_suffix(logrecord, record, fset, dentry, rec);

        error = presto_log(fset, rec, record, size,
                           path, size_round(le32_to_cpu(pathlen)),
                           NULL, 0, NULL, 0);
        BUFF_FREE(buffer);

        EXIT;
        return error;
}

int presto_rewrite_close(struct rec_info *rec, struct presto_file_set *fset, 
                         char *path, __u32 pathlen, 
                         int ngroups, __u32 *groups, 
                         __u64 ino,     __u32 generation, 
                         struct presto_version *new_file_ver)
{
        int opcode = KML_OPCODE_CLOSE;
        char *logrecord, record[292];
        struct dentry *root;
        int error, size;

        ENTRY;

        if ( presto_no_journal(fset) ) {
                EXIT;
                return 0;
        }

        root = fset->fset_dentry;

        size =  sizeof(__u32) * ngroups + 
                sizeof(struct kml_prefix_hdr) + sizeof(*new_file_ver) +
                sizeof(ino) + sizeof(generation) + 
                sizeof(le32_to_cpu(pathlen)) +
                sizeof(struct kml_suffix);

        if ( size > sizeof(record) )
                CERROR("InterMezzo: BUFFER OVERFLOW in %s!\n", __FUNCTION__);

        rec->is_kml = 1;
        rec->size = size + size_round(le32_to_cpu(pathlen));

        logrecord = journal_log_prefix_with_groups(record, opcode, rec,
                                                   ngroups, groups);
        logrecord = log_version(logrecord, new_file_ver);
        logrecord = logit(logrecord, &ino, sizeof(ino));
        logrecord = logit(logrecord, &generation, sizeof(generation));
        logrecord = logit(logrecord, &pathlen, sizeof(pathlen));
        logrecord = journal_log_suffix(logrecord, record, fset, NULL, rec);

        error = presto_log(fset, rec, record, size,
                           path, size_round(le32_to_cpu(pathlen)),
                           NULL, 0, NULL, 0);

        EXIT;
        return error;
}


/* write closes for the local close records in the LML */ 
int presto_complete_lml(struct presto_file_set *fset)
{
        __u32 groups[NGROUPS_MAX];
        loff_t lml_offset;
        loff_t read_offset; 
        char *buffer;
        void *handle;
        struct rec_info rec;
        struct close_rec { 
                struct presto_version new_file_ver;
                __u64 ino;
                __u32 generation;
                __u32 pathlen;
                __u64 remote_ino;
                __u32 remote_generation;
                __u32 remote_version;
                __u64 lml_offset;
        } close_rec; 
        struct file *file = fset->fset_lml.fd_file;
        struct kml_prefix_hdr prefix;
        int rc = 0;
        ENTRY;

        lml_offset = 0; 
 again: 
        if (lml_offset >= file->f_dentry->d_inode->i_size) {
                EXIT;
                return rc;
        }

        read_offset = lml_offset;
        rc = presto_fread(file, (char *)&prefix,
                          sizeof(prefix), &read_offset);
        if ( rc != sizeof(prefix) ) {
                EXIT;
                CERROR("presto_complete_lml: ioerror - 1, tell Peter\n");
                return -EIO;
        }

        if ( prefix.opcode == KML_OPCODE_NOOP ) {
                lml_offset += prefix.len; 
                goto again; 
        }

        rc = presto_fread(file, (char *)groups, 
                          prefix.ngroups * sizeof(__u32), &read_offset); 
        if ( rc != prefix.ngroups * sizeof(__u32) ) {
                EXIT;
                CERROR("presto_complete_lml: ioerror - 2, tell Peter\n");
                return -EIO;
        }

        rc = presto_fread(file, (char *)&close_rec, 
                          sizeof(close_rec), &read_offset); 
        if ( rc != sizeof(close_rec) ) {
                EXIT;
                CERROR("presto_complete_lml: ioerror - 3, tell Peter\n");
                return -EIO;
        }

        /* is this a backfetch or a close record? */ 
        if ( le64_to_cpu(close_rec.remote_ino) != 0 ) { 
                lml_offset += prefix.len;
                goto again; 
        }

        BUFF_ALLOC(buffer, NULL);
        rc = presto_fread(file, (char *)buffer, 
                          le32_to_cpu(close_rec.pathlen), &read_offset); 
        if ( rc != le32_to_cpu(close_rec.pathlen) ) {
                EXIT;
                CERROR("presto_complete_lml: ioerror - 4, tell Peter\n");
                return -EIO;
        }
        
        handle = presto_trans_start(fset, file->f_dentry->d_inode, 
                                    KML_OPCODE_RELEASE);
        if ( IS_ERR(handle) ) {
                EXIT;
                return -ENOMEM; 
        }

        rc = presto_clear_lml_close(fset, lml_offset); 
        if ( rc ) {
                CERROR("error during clearing: %d\n", rc);
                presto_trans_commit(fset, handle);
                EXIT; 
                return rc; 
        }

        rc = presto_rewrite_close(&rec, fset, buffer, close_rec.pathlen, 
                                  prefix.ngroups, groups, 
                                  close_rec.ino, close_rec.generation,
                                  &close_rec.new_file_ver); 
        if ( rc ) {
                CERROR("error during rewrite close: %d\n", rc);
                presto_trans_commit(fset, handle);
                EXIT; 
                return rc; 
        }

        presto_trans_commit(fset, handle); 
        if ( rc ) { 
                CERROR("error during truncation: %d\n", rc);
                EXIT; 
                return rc;
        }
        
        lml_offset += prefix.len; 
        CDEBUG(D_JOURNAL, "next LML record at: %ld\n", (long)lml_offset);
        goto again;

        EXIT;
        return -EINVAL;
}


#ifdef CONFIG_FS_EXT_ATTR
/* Journal an ea operation. A NULL buffer implies the attribute is 
 * getting deleted. In this case we simply change the opcode, but nothing
 * else is affected.
 */
int presto_journal_set_ext_attr (struct rec_info *rec, 
                                 struct presto_file_set *fset, 
                                 struct dentry *dentry, 
                                 struct presto_version *ver, const char *name, 
                                 const char *buffer, int buffer_len, 
                                 int flags) 
{ 
        int opcode = (buffer == NULL) ? 
                     KML_OPCODE_DELEXTATTR : 
                     KML_OPCODE_SETEXTATTR ;
        char *temp, *path, *logrecord, record[292];
        struct dentry *root;
        int error, size;
        __u32 namelen=cpu_to_le32(strnlen(name,PRESTO_EXT_ATTR_NAME_MAX));
        __u32 buflen=(buffer != NULL)? cpu_to_le32(buffer_len): cpu_to_le32(0);
        __u32 mode, pathlen;

        ENTRY;
        if ( presto_no_journal(fset) ) {
                EXIT;
                return 0;
        }

        if (!dentry->d_inode || (dentry->d_inode->i_nlink == 0) 
            || ((dentry->d_parent != dentry) && list_empty(&dentry->d_hash))) {
                EXIT;
                return 0;
        }

        root = fset->fset_dentry;

        BUFF_ALLOC(temp, NULL);
        path = presto_path(dentry, root, temp, PAGE_SIZE);
        pathlen = cpu_to_le32(MYPATHLEN(temp, path));

        flags=cpu_to_le32(flags);
        /* Ugly, but needed. posix ACLs change the mode without using
         * setattr, we need to record these changes. The EA code per se
         * is not really affected.
         */
        mode=cpu_to_le32(dentry->d_inode->i_mode);

        size =  sizeof(__u32) * current->ngroups + 
                sizeof(struct kml_prefix_hdr) + 
                2 * sizeof(struct presto_version) +
                sizeof(flags) + sizeof(mode) + sizeof(namelen) + 
                sizeof(buflen) + sizeof(pathlen) + 
                sizeof(struct kml_suffix);

        if ( size > sizeof(record) )
                CERROR("InterMezzo: BUFFER OVERFLOW in %s!\n", __FUNCTION__);

        rec->is_kml = 1;
        /* Make space for a path, a attr name and value*/
        /* We use the buflen instead of buffer_len to make sure that we 
         * journal the right length. This may be a little paranoid, but
         * with 64 bits round the corner, I would rather be safe than sorry!
         * Also this handles deletes with non-zero buffer_lengths correctly.
         * SHP
         */
        rec->size = size + size_round(le32_to_cpu(pathlen)) +
                    size_round(le32_to_cpu(namelen)) + 
                    size_round(le32_to_cpu(buflen));

        logrecord = journal_log_prefix(record, opcode, rec);
        logrecord = log_version(logrecord, ver);
        logrecord = log_dentry_version(logrecord, dentry);
        logrecord = logit(logrecord, &flags, sizeof(flags));
        logrecord = logit(logrecord, &mode, sizeof(flags));
        logrecord = logit(logrecord, &pathlen, sizeof(pathlen));
        logrecord = logit(logrecord, &namelen, sizeof(namelen));
        logrecord = logit(logrecord, &buflen, sizeof(buflen));
        logrecord = journal_log_suffix(logrecord, record, fset, dentry, rec);

        error = presto_log(fset, rec, record, size,
                           path, size_round(le32_to_cpu(pathlen)),
                           name, size_round(le32_to_cpu(namelen)),
                           buffer, size_round(le32_to_cpu(buflen)));

        BUFF_FREE(temp);
        EXIT;
        return error;
}
#endif
