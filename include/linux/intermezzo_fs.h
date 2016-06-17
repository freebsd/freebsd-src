/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (C) 2001, 2002 Cluster File Systems, Inc.
 *  Copyright (C) 2001 Tacitus Systems, Inc.
 *  Copyright (C) 2000 Stelias Computing, Inc.
 *  Copyright (C) 2000 Red Hat, Inc.
 *  Copyright (C) 2000 TurboLinux, Inc.
 *  Copyright (C) 2000 Los Alamos National Laboratory.
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

#ifndef __INTERMEZZO_FS_H_
#define __INTERMEZZO_FS_H_ 1

#include <linux/intermezzo_lib.h>
#include <linux/intermezzo_idl.h>


#ifdef __KERNEL__
typedef __u8 uuid_t[16];
#else
# include <uuid/uuid.h>
#endif

struct lento_vfs_context {
        __u64 kml_offset;
        __u64 updated_time;
        __u64 remote_ino;
        __u64 remote_generation;
        __u32 slot_offset;
        __u32 recno;
        __u32 flags;
        uuid_t uuid;
        struct presto_version remote_version;
};

static inline int izo_ioctl_is_invalid(struct izo_ioctl_data *data);

#ifdef __KERNEL__
# include <linux/smp.h>
# include <linux/fsfilter.h>
# include <linux/slab.h>
# include <linux/vmalloc.h>
# include <linux/smp_lock.h>

/* fixups for fs.h */
# ifndef fs_down
#  define fs_down(sem) down(sem)
# endif

# ifndef fs_up
#  define fs_up(sem) up(sem)
# endif

# define KML_IDLE                        0
# define KML_DECODE                      1
# define KML_OPTIMIZE                    2
# define KML_REINT                       3

# define KML_OPEN_REINT                  0x0100
# define KML_REINT_BEGIN                 0x0200
# define KML_BACKFETCH                   0x0400
# define KML_REINT_END                   0x0800
# define KML_CLOSE_REINT                 0x1000
# define KML_REINT_MAXBUF                (64 * 1024)

# define CACHE_CLIENT_RO       0x4
# define CACHE_LENTO_RO        0x8

/* global variables */
extern int presto_debug;
extern int presto_print_entry;
extern long presto_kmemory;
extern long presto_vmemory;

# define PRESTO_DEBUG
# ifdef PRESTO_DEBUG
/* debugging masks */
#  define D_SUPER       1
#  define D_INODE       2
#  define D_FILE        4
#  define D_CACHE       8  /* cache debugging */
#  define D_MALLOC     16  /* print malloc, de-alloc information */
#  define D_JOURNAL    32
#  define D_UPCALL     64  /* up and downcall debugging */
#  define D_PSDEV     128
#  define D_PIOCTL    256
#  define D_SPECIAL   512
#  define D_TIMING   1024
#  define D_DOWNCALL 2048
#  define D_KML      4096
#  define D_FSDATA   8192

#  define CDEBUG(mask, format, a...)                                    \
        do {                                                            \
                if (presto_debug & mask) {                              \
                        printk("(%s:%s,l. %d %d): " format, __FILE__,   \
                               __FUNCTION__, __LINE__, current->pid     \
                               , ## a);                                 \
                }                                                       \
        } while (0)

#define CERROR(format, a...)                                            \
do {                                                                    \
        printk("(%s:%s,l. %d %d): " format, __FILE__, __FUNCTION__,     \
               __LINE__, current->pid , ## a);                          \
} while (0)

#  define ENTRY                                                         \
        if (presto_print_entry)                                         \
                printk("Process %d entered %s\n", current->pid, __FUNCTION__)

#  define EXIT                                                          \
        if (presto_print_entry)                                         \
                printk("Process %d leaving %s at %d\n", current->pid,   \
                       __FUNCTION__, __LINE__)

#  define presto_kmem_inc(ptr, size) presto_kmemory += (size)
#  define presto_kmem_dec(ptr, size) presto_kmemory -= (size)
#  define presto_vmem_inc(ptr, size) presto_vmemory += (size)
#  define presto_vmem_dec(ptr, size) presto_vmemory -= (size)
# else /* !PRESTO_DEBUG */
#  define CDEBUG(mask, format, a...) do {} while (0)
#  define ENTRY do {} while (0)
#  define EXIT do {} while (0)
#  define presto_kmem_inc(ptr, size) do {} while (0)
#  define presto_kmem_dec(ptr, size) do {} while (0)
#  define presto_vmem_inc(ptr, size) do {} while (0)
#  define presto_vmem_dec(ptr, size) do {} while (0)
# endif /* PRESTO_DEBUG */


struct run_ctxt {
        struct vfsmount *pwdmnt;
        struct dentry   *pwd;
        struct vfsmount *rootmnt;
        struct dentry   *root;
        uid_t            fsuid;
        gid_t            fsgid;
        mm_segment_t     fs;
	int              ngroups;
	gid_t	         groups[NGROUPS];

};

static inline void push_ctxt(struct run_ctxt *save, struct run_ctxt *new)
{
        int i;
        save->fs = get_fs();
        save->pwd = dget(current->fs->pwd);
        save->pwdmnt = mntget(current->fs->pwdmnt);
        save->fsgid = current->fsgid;
        save->fsuid = current->fsuid;
        save->root = current->fs->root;
        save->rootmnt = current->fs->rootmnt;
        save->ngroups = current->ngroups;
        for (i = 0; i< current->ngroups; i++) 
                save->groups[i] = current->groups[i];

        set_fs(new->fs);
        lock_kernel();
        set_fs_pwd(current->fs, new->pwdmnt, new->pwd);
        if (new->root)
                set_fs_root(current->fs, new->rootmnt, new->root);
        unlock_kernel();
        current->fsuid = new->fsuid;
        current->fsgid = new->fsgid;
        if (new->ngroups > 0) {
                current->ngroups = new->ngroups;
                for (i = 0; i< new->ngroups; i++) 
                        current->groups[i] = new->groups[i];
        }
        
}

static inline void pop_ctxt(struct run_ctxt *saved)
{
        int i;

        set_fs(saved->fs);
        lock_kernel();
        set_fs_pwd(current->fs, saved->pwdmnt, saved->pwd);
        if (saved->root)
                set_fs_root(current->fs, saved->rootmnt, saved->root);
        unlock_kernel();
        current->fsuid = saved->fsuid;
        current->fsgid = saved->fsgid;
        current->ngroups = saved->ngroups;
        for (i = 0; i< saved->ngroups; i++) 
                current->groups[i] = saved->groups[i];

        mntput(saved->pwdmnt);
        dput(saved->pwd);
}

static inline struct presto_dentry_data *presto_d2d(struct dentry *dentry)
{
        return (struct presto_dentry_data *)(dentry->d_fsdata);
}

struct presto_cache {
        spinlock_t          cache_lock;
        loff_t              cache_reserved;
        struct  vfsmount   *cache_vfsmount;
        struct super_block *cache_sb;
        struct  dentry     *cache_root;
        struct list_head    cache_chain; /* for the dev/cache hash */

        int   cache_flags;

        kdev_t cache_dev;            /* underlying block device */

        char *cache_type;            /* filesystem type of cache */
        struct filter_fs *cache_filter;

        struct upc_channel *cache_psdev;  /* points to channel used */
        struct list_head cache_channel_list; 
        struct list_head cache_fset_list; /* filesets mounted in cache */
};

struct presto_log_fd {
        rwlock_t         fd_lock;
        loff_t           fd_offset;  /* offset where next record should go */
        struct file    *fd_file;
        int             fd_truncating;
        unsigned int   fd_recno;   /* last recno written */
        struct list_head  fd_reservations;
};

/* file sets */
# define CHUNK_BITS  16

struct presto_file_set {
        struct list_head fset_list;
        struct presto_log_fd fset_kml;
        struct presto_log_fd fset_lml;
        struct presto_log_fd fset_rcvd;
        struct list_head *fset_clients;  /* cache of clients */
        struct dentry *fset_dentry;
        struct vfsmount *fset_mnt;
        struct presto_cache *fset_cache;

        unsigned int fset_lento_recno;  /* last recno mentioned to lento */
        loff_t fset_lento_off;    /* last offset mentioned to lento */
        loff_t fset_kml_logical_off; /* logical offset of kml file byte 0 */
        char * fset_name;

        int fset_flags;
        int fset_chunkbits;
        char *fset_reint_buf; /* temporary buffer holds kml during reint */

        spinlock_t fset_permit_lock;
        int fset_permit_count;
        int fset_permit_upcall_count;
        /* This queue is used both for processes waiting for the kernel to give
         * up the permit as well as processes waiting for the kernel to be given
         * the permit, depending on the state of FSET_HASPERMIT. */
        wait_queue_head_t fset_permit_queue;

        loff_t  fset_file_maxio;  /* writing more than this causes a close */
        unsigned long int kml_truncate_size;
};

/* This is the default number of bytes written before a close is recorded*/
#define FSET_DEFAULT_MAX_FILEIO (1024<<10)

struct dentry *presto_tmpfs_ilookup(struct inode *dir, struct dentry *dentry, 
                                    ino_t ino, unsigned int generation);
struct dentry *presto_iget_ilookup(struct inode *dir, struct dentry *dentry, 
                                    ino_t ino, unsigned int generation);
struct dentry *presto_add_ilookup_dentry(struct dentry *parent,
                                         struct dentry *real);

struct journal_ops {
        int (*tr_all_data)(struct inode *);
        loff_t (*tr_avail)(struct presto_cache *fset, struct super_block *);
        void *(*tr_start)(struct presto_file_set *, struct inode *, int op);
        void (*tr_commit)(struct presto_file_set *, void *handle);
        void (*tr_journal_data)(struct inode *);
        struct dentry *(*tr_ilookup)(struct inode *dir, struct dentry *dentry, ino_t ino, unsigned int generation);
        struct dentry *(*tr_add_ilookup)(struct dentry *parent, struct dentry *real);
};

extern struct journal_ops presto_ext2_journal_ops;
extern struct journal_ops presto_ext3_journal_ops;
extern struct journal_ops presto_tmpfs_journal_ops;
extern struct journal_ops presto_xfs_journal_ops;
extern struct journal_ops presto_reiserfs_journal_ops;
extern struct journal_ops presto_obdfs_journal_ops;

# define LENTO_FL_KML            0x0001
# define LENTO_FL_EXPECT         0x0002
# define LENTO_FL_VFSCHECK       0x0004
# define LENTO_FL_JUSTLOG        0x0008
# define LENTO_FL_WRITE_KML      0x0010
# define LENTO_FL_CANCEL_LML     0x0020
# define LENTO_FL_WRITE_EXPECT   0x0040
# define LENTO_FL_IGNORE_TIME    0x0080
# define LENTO_FL_TOUCH_PARENT   0x0100
# define LENTO_FL_TOUCH_NEWOBJ   0x0200
# define LENTO_FL_SET_DDFILEID   0x0400

struct presto_cache *presto_get_cache(struct inode *inode);
int presto_sprint_mounts(char *buf, int buflen, int minor);
struct presto_file_set *presto_fset(struct dentry *de);
int presto_journal(struct dentry *dentry, char *buf, size_t size);
int presto_fwrite(struct file *file, const char *str, int len, loff_t *off);
int presto_ispresto(struct inode *);

/* super.c */
extern struct file_system_type presto_fs_type;
extern int init_intermezzo_fs(void);

/* fileset.c */
extern int izo_prepare_fileset(struct dentry *root, char *fsetname);
char * izo_make_path(struct presto_file_set *fset, char *name);
struct file *izo_fset_open(struct presto_file_set *fset, char *name, int flags, int mode);

/* psdev.c */
int izo_psdev_get_free_channel(void);
int presto_psdev_init(void);
int izo_psdev_setpid(int minor);
extern void presto_psdev_cleanup(void);
inline int presto_lento_up(int minor);
int izo_psdev_setchannel(struct file *file, int fd);

/* inode.c */
extern struct super_operations presto_super_ops;
void presto_set_ops(struct inode *inode, struct  filter_fs *filter);

/* dcache.c */
void presto_frob_dop(struct dentry *de);
char *presto_path(struct dentry *dentry, struct dentry *root,
                  char *buffer, int buflen);
inline struct presto_dentry_data *izo_alloc_ddata(void);
int presto_set_dd(struct dentry *);
int presto_init_ddata_cache(void);
void presto_cleanup_ddata_cache(void);
extern struct dentry_operations presto_dentry_ops;

/* dir.c */
extern struct inode_operations presto_dir_iops;
extern struct inode_operations presto_file_iops;
extern struct inode_operations presto_sym_iops;
extern struct file_operations presto_dir_fops;
extern struct file_operations presto_file_fops;
extern struct file_operations presto_sym_fops;
int presto_setattr(struct dentry *de, struct iattr *iattr);
int presto_settime(struct presto_file_set *fset, struct dentry *newobj,
                   struct dentry *parent, struct dentry *target,
                   struct lento_vfs_context *ctx, int valid);
int presto_ioctl(struct inode *inode, struct file *file,
                 unsigned int cmd, unsigned long arg);

extern int presto_ilookup_uid;
# define PRESTO_ILOOKUP_MAGIC "...ino:"
# define PRESTO_ILOOKUP_SEP ':'
int izo_dentry_is_ilookup(struct dentry *, ino_t *id, unsigned int *generation);
struct dentry *presto_lookup(struct inode * dir, struct dentry *dentry);

struct presto_dentry_data {
        int dd_count; /* how mnay dentries are using this dentry */
        struct presto_file_set *dd_fset;
        struct dentry *dd_inodentry; 
        loff_t dd_kml_offset;
        int dd_flags;
        __u64 remote_ino;
        __u64 remote_generation;
};

struct presto_file_data {
        int fd_do_lml;
        loff_t fd_lml_offset;
        size_t fd_bytes_written;
        /* authorization related data of file at open time */
        uid_t fd_uid;
        gid_t fd_gid;
        mode_t fd_mode;
        /* identification data of calling process */
        uid_t fd_fsuid;
        gid_t fd_fsgid;
        int fd_ngroups;
        gid_t fd_groups[NGROUPS_MAX];
        /* information how to complete the close operation */
        struct lento_vfs_context fd_info;
        struct presto_version fd_version;
};

/* presto.c and Lento::Downcall */

int presto_walk(const char *name, struct nameidata *nd);
int izo_clear_fsetroot(struct dentry *dentry);
int izo_clear_all_fsetroots(struct presto_cache *cache);
int presto_get_kmlsize(char *path, __u64 *size);
int presto_get_lastrecno(char *path, off_t *size);
int presto_set_fsetroot(struct dentry *dentry, char *fsetname,
                       unsigned int flags);
int presto_set_fsetroot_from_ioc(struct dentry *dentry, char *fsetname,
                                 unsigned int flags);
inline int presto_is_read_only(struct presto_file_set *);
int presto_truncate_lml(struct presto_file_set *fset);
int lento_write_lml(char *path,
                     __u64 remote_ino,
                     __u32 remote_generation,
                     __u32 remote_version,
                    struct presto_version *remote_file_version);
int lento_complete_closes(char *path);
inline int presto_f2m(struct presto_file_set *fset);
int presto_prep(struct dentry *, struct presto_cache **,
                       struct presto_file_set **);
/* cache.c */
extern struct presto_cache *presto_cache_init(void);
extern inline void presto_cache_add(struct presto_cache *cache, kdev_t dev);
extern inline void presto_cache_init_hash(void);

struct presto_cache *presto_cache_find(kdev_t dev);

#define PRESTO_REQLOW  (3 * 4096)
#define PRESTO_REQHIGH (6 * 4096)
void presto_release_space(struct presto_cache *cache, loff_t req);
int presto_reserve_space(struct presto_cache *cache, loff_t req);

#define PRESTO_DATA             0x00000002 /* cached data is valid */
#define PRESTO_ATTR             0x00000004 /* attributes cached */
#define PRESTO_DONT_JOURNAL     0x00000008 /* things like .intermezzo/ */

struct presto_file_set *presto_path2fileset(const char *name);
int izo_revoke_permit(struct dentry *, uuid_t uuid);
int presto_chk(struct dentry *dentry, int flag);
void presto_set(struct dentry *dentry, int flag);
int presto_get_permit(struct inode *inode);
int presto_put_permit(struct inode *inode);
int presto_set_max_kml_size(const char *path, unsigned long max_size);
int izo_mark_dentry(struct dentry *dentry, int and, int or, int *res);
int izo_mark_cache(struct dentry *dentry, int and_bits, int or_bits, int *);
int izo_mark_fset(struct dentry *dentry, int and_bits, int or_bits, int *);
void presto_getversion(struct presto_version *pv, struct inode *inode);
int presto_i2m(struct inode *inode);
int presto_c2m(struct presto_cache *cache);


/* file.c */
int izo_purge_file(struct presto_file_set *fset, char *file);
int presto_adjust_lml(struct file *file, struct lento_vfs_context *info);

/* journal.c */
struct rec_info {
        loff_t offset;
        int size;
        int recno;
        int is_kml;
};

void presto_trans_commit(struct presto_file_set *fset, void *handle);
void *presto_trans_start(struct presto_file_set *fset, struct inode *inode,
                         int op);
int presto_fread(struct file *file, char *str, int len, loff_t *off);
int presto_clear_lml_close(struct presto_file_set *fset,
                           loff_t  lml_offset);
int presto_complete_lml(struct presto_file_set *fset);
int presto_read_kml_logical_offset(struct rec_info *recinfo,
                                   struct presto_file_set *fset);
int presto_write_kml_logical_offset(struct presto_file_set *fset);
struct file *presto_copy_kml_tail(struct presto_file_set *fset,
                                  unsigned long int start);
int presto_finish_kml_truncate(struct presto_file_set *fset,
                               unsigned long int offset);
int izo_lookup_file(struct presto_file_set *fset, char *path,
                    struct nameidata *nd);
int izo_do_truncate(struct presto_file_set *fset, struct dentry *dentry,
                    loff_t length,  loff_t size_check);
int izo_log_close(struct presto_log_fd *logfd);
struct file *izo_log_open(struct presto_file_set *fset, char *name, int flags);
int izo_init_kml_file(struct presto_file_set *, struct presto_log_fd *);
int izo_init_lml_file(struct presto_file_set *, struct presto_log_fd *);
int izo_init_last_rcvd_file(struct presto_file_set *, struct presto_log_fd *);

/* vfs.c */

/* Extra data needed in the KML for rollback operations; this structure is
 * passed around during the KML-writing process. */
struct izo_rollback_data {
        __u32 rb_mode;
        __u32 rb_rdev;
        __u64 rb_uid;
        __u64 rb_gid;
};

int presto_write_last_rcvd(struct rec_info *recinfo,
                           struct presto_file_set *fset,
                           struct lento_vfs_context *info);
void izo_get_rollback_data(struct inode *inode, struct izo_rollback_data *rb);
int presto_do_close(struct presto_file_set *fset, struct file *file);
int presto_do_setattr(struct presto_file_set *fset, struct dentry *dentry,
                      struct iattr *iattr, struct lento_vfs_context *info);
int presto_do_create(struct presto_file_set *fset, struct dentry *dir,
                     struct dentry *dentry, int mode,
                     struct lento_vfs_context *info);
int presto_do_link(struct presto_file_set *fset, struct dentry *dir,
                   struct dentry *old_dentry, struct dentry *new_dentry,
                   struct lento_vfs_context *info);
int presto_do_unlink(struct presto_file_set *fset, struct dentry *dir,
                     struct dentry *dentry, struct lento_vfs_context *info);
int presto_do_symlink(struct presto_file_set *fset, struct dentry *dir,
                      struct dentry *dentry, const char *name,
                      struct lento_vfs_context *info);
int presto_do_mkdir(struct presto_file_set *fset, struct dentry *dir,
                    struct dentry *dentry, int mode,
                    struct lento_vfs_context *info);
int presto_do_rmdir(struct presto_file_set *fset, struct dentry *dir,
                    struct dentry *dentry, struct lento_vfs_context *info);
int presto_do_mknod(struct presto_file_set *fset, struct dentry *dir,
                    struct dentry *dentry, int mode, dev_t dev,
                    struct lento_vfs_context *info);
int do_rename(struct presto_file_set *fset, struct dentry *old_dir,
              struct dentry *old_dentry, struct dentry *new_dir,
              struct dentry *new_dentry, struct lento_vfs_context *info);
int presto_do_statfs (struct presto_file_set *fset,
                      struct statfs * buf);

int lento_setattr(const char *name, struct iattr *iattr,
                  struct lento_vfs_context *info);
int lento_create(const char *name, int mode, struct lento_vfs_context *info);
int lento_link(const char *oldname, const char *newname,
               struct lento_vfs_context *info);
int lento_unlink(const char *name, struct lento_vfs_context *info);
int lento_symlink(const char *oldname,const char *newname,
                  struct lento_vfs_context *info);
int lento_mkdir(const char *name, int mode, struct lento_vfs_context *info);
int lento_rmdir(const char *name, struct lento_vfs_context *info);
int lento_mknod(const char *name, int mode, dev_t dev,
                struct lento_vfs_context *info);
int lento_rename(const char *oldname, const char *newname,
                 struct lento_vfs_context *info);
int lento_iopen(const char *name, ino_t ino, unsigned int generation,int flags);

/* journal.c */

#define JOURNAL_PAGE_SZ  PAGE_SIZE

__inline__ int presto_no_journal(struct presto_file_set *fset);
int journal_fetch(int minor);
int presto_log(struct presto_file_set *fset, struct rec_info *rec,
               const char *buf, size_t size,
               const char *string1, int len1, 
               const char *string2, int len2,
               const char *string3, int len3);
int presto_get_fileid(int minor, struct presto_file_set *fset,
                      struct dentry *dentry);
int presto_journal_setattr(struct rec_info *rec, struct presto_file_set *fset,
                           struct dentry *dentry, struct presto_version *old_ver,
                           struct izo_rollback_data *, struct iattr *iattr);
int presto_journal_create(struct rec_info *rec, struct presto_file_set *fset,
                          struct dentry *dentry,
                          struct presto_version *tgt_dir_ver,
                          struct presto_version *new_file_ver, int mode);
int presto_journal_link(struct rec_info *rec, struct presto_file_set *fset,
                        struct dentry *src, struct dentry *tgt,
                        struct presto_version *tgt_dir_ver,
                        struct presto_version *new_link_ver);
int presto_journal_unlink(struct rec_info *rec, struct presto_file_set *fset,
                          struct dentry *dir,
                          struct presto_version *tgt_dir_ver,
                          struct presto_version *old_file_ver,
                          struct izo_rollback_data *, struct dentry *dentry,
                          char *old_target, int old_targetlen);
int presto_journal_symlink(struct rec_info *rec, struct presto_file_set *fset,
                           struct dentry *dentry, const char *target,
                           struct presto_version *tgt_dir_ver,
                           struct presto_version *new_link_ver);
int presto_journal_mkdir(struct rec_info *rec, struct presto_file_set *fset,
                         struct dentry *dentry,
                         struct presto_version *tgt_dir_ver,
                         struct presto_version *new_dir_ver, int mode);
int presto_journal_rmdir(struct rec_info *rec, struct presto_file_set *fset,
                         struct dentry *dentry,
                         struct presto_version *tgt_dir_ver,
                         struct presto_version *old_dir_ver,
                         struct izo_rollback_data *, int len, const char *name);
int presto_journal_mknod(struct rec_info *rec, struct presto_file_set *fset,
                         struct dentry *dentry,
                         struct presto_version *tgt_dir_ver,
                         struct presto_version *new_node_ver, int mode,
                         int dmajor, int dminor);
int presto_journal_rename(struct rec_info *rec, struct presto_file_set *fset,
                          struct dentry *src, struct dentry *tgt,
                          struct presto_version *src_dir_ver,
                          struct presto_version *tgt_dir_ver);
int presto_journal_open(struct rec_info *, struct presto_file_set *,
                        struct dentry *, struct presto_version *old_ver);
int presto_journal_close(struct rec_info *rec, struct presto_file_set *,
                         struct file *, struct dentry *,
                         struct presto_version *old_file_ver,
                         struct presto_version *new_file_ver);
int presto_write_lml_close(struct rec_info *rec,
                           struct presto_file_set *fset, 
                           struct file *file,
                           __u64 remote_ino,
                           __u64 remote_generation,
                           struct presto_version *remote_version,
                           struct presto_version *new_file_ver);
void presto_log_op(void *data, int len);
loff_t presto_kml_offset(struct presto_file_set *fset);

/* upcall.c */
#define SYNCHRONOUS 0
#define ASYNCHRONOUS 1
/* asynchronous calls */
int izo_upc_kml(int minor, __u64 offset, __u32 first_recno, __u64 length,
                __u32 last_recno, char *fsetname);
int izo_upc_kml_truncate(int minor, __u64 length, __u32 last_recno,
                         char *fsetname);
int izo_upc_go_fetch_kml(int minor, char *fsetname, uuid_t uuid, __u64 kmlsize);
int izo_upc_backfetch(int minor, char *path, char *fileset, 
                      struct lento_vfs_context *);

/* synchronous calls */
int izo_upc_get_fileid(int minor, __u32 reclen, char *rec, 
                       __u32 pathlen, char *path, char *fsetname);
int izo_upc_permit(int minor, struct dentry *, __u32 pathlen, char *path,
                   char *fset);
int izo_upc_open(int minor, __u32 pathlen, char *path, char *fsetname, 
                 struct lento_vfs_context *info);
int izo_upc_connect(int minor, __u64 ip_address, __u64 port, __u8 uuid[16],
                    int client_flag);
int izo_upc_revoke_permit(int minor, char *fsetname, uuid_t uuid);
int izo_upc_set_kmlsize(int minor, char *fsetname, uuid_t uuid, __u64 kmlsize);
int izo_upc_client_make_branch(int minor, char *fsetname);
int izo_upc_server_make_branch(int minor, char *fsetname);
int izo_upc_branch_undo(int minor, char *fsetname, char *branchname);
int izo_upc_branch_redo(int minor, char *fsetname, char *branchname);
int izo_upc_repstatus(int minor,  char * fsetname, struct izo_rcvd_rec *lr_server);

/* general mechanism */
int izo_upc_upcall(int minor, int *size, struct izo_upcall_hdr *, int async);

/* replicator.c */
int izo_repstatus(struct presto_file_set *fset, __u64 client_kmlsize, 
                  struct izo_rcvd_rec *lr_client, struct izo_rcvd_rec *lr_server);
int izo_rep_cache_init(struct presto_file_set *);
loff_t izo_rcvd_get(struct izo_rcvd_rec *, struct presto_file_set *, char *uuid);
loff_t izo_rcvd_write(struct presto_file_set *, struct izo_rcvd_rec *);
loff_t izo_rcvd_upd_remote(struct presto_file_set *fset, char * uuid,  __u64 remote_recno,
                           __u64 remote_offset);

/* sysctl.c */
int init_intermezzo_sysctl(void);
void cleanup_intermezzo_sysctl(void);

/* ext_attr.c */
/* We will be more tolerant than the default ea patch with attr name sizes and
 * the size of value. If these come via VFS from the default ea patches, the
 * corresponding character strings will be truncated anyway. During journalling- * we journal length for both name and value. See journal_set_ext_attr.
 */
#define PRESTO_EXT_ATTR_NAME_MAX 128
#define PRESTO_EXT_ATTR_VALUE_MAX 8192

#define PRESTO_ALLOC(ptr, size)                                         \
do {                                                                    \
        long s = (size);                                                \
        (ptr) = kmalloc(s, GFP_KERNEL);                                 \
        if ((ptr) == NULL)                                              \
                CERROR("IZO: out of memory at %s:%d (trying to "        \
                       "allocate %ld)\n", __FILE__, __LINE__, s);       \
        else {                                                          \
                presto_kmem_inc((ptr), s);                              \
                memset((ptr), 0, s);                                    \
        }                                                               \
        CDEBUG(D_MALLOC, "kmalloced: %ld at %p (tot %ld).\n",           \
               s, (ptr), presto_kmemory);                               \
} while (0)

#define PRESTO_FREE(ptr, size)                                          \
do {                                                                    \
        long s = (size);                                                \
        if ((ptr) == NULL) {                                            \
                CERROR("IZO: free NULL pointer (%ld bytes) at "         \
                       "%s:%d\n", s, __FILE__, __LINE__);               \
                break;                                                  \
        }                                                               \
        kfree(ptr);                                                     \
        CDEBUG(D_MALLOC, "kfreed: %ld at %p (tot %ld).\n",              \
               s, (ptr), presto_kmemory);                               \
        presto_kmem_dec((ptr), s);                                      \
} while (0)

static inline int dentry_name_cmp(struct dentry *dentry, char *name)
{
        return (strlen(name) == dentry->d_name.len &&
                memcmp(name, dentry->d_name.name, dentry->d_name.len) == 0);
}

static inline char *strdup(char *str)
{
        char *tmp;
        tmp = kmalloc(strlen(str) + 1, GFP_KERNEL);
        if (tmp)
                memcpy(tmp, str, strlen(str) + 1);
               
        return tmp;
}

/* buffer MUST be at least the size of izo_ioctl_hdr */
static inline int izo_ioctl_getdata(char *buf, char *end, void *arg)
{
        struct izo_ioctl_hdr *hdr;
        struct izo_ioctl_data *data;
        int err;
        ENTRY;

        hdr = (struct izo_ioctl_hdr *)buf;
        data = (struct izo_ioctl_data *)buf;

        err = copy_from_user(buf, (void *)arg, sizeof(*hdr));
        if ( err ) {
                EXIT;
                return err;
        }

        if (hdr->ioc_version != IZO_IOCTL_VERSION) {
                CERROR("IZO: version mismatch kernel vs application\n");
                return -EINVAL;
        }

        if (hdr->ioc_len + buf >= end) {
                CERROR("IZO: user buffer exceeds kernel buffer\n");
                return -EINVAL;
        }

        if (hdr->ioc_len < sizeof(struct izo_ioctl_data)) {
                CERROR("IZO: user buffer too small for ioctl\n");
                return -EINVAL;
        }

        err = copy_from_user(buf, (void *)arg, hdr->ioc_len);
        if ( err ) {
                EXIT;
                return err;
        }

        if (izo_ioctl_is_invalid(data)) {
                CERROR("IZO: ioctl not correctly formatted\n");
                return -EINVAL;
        }

        if (data->ioc_inllen1) {
                data->ioc_inlbuf1 = &data->ioc_bulk[0];
        }

        if (data->ioc_inllen2) {
                data->ioc_inlbuf2 = &data->ioc_bulk[0] +
                        size_round(data->ioc_inllen1);
        }

        EXIT;
        return 0;
}

# define MYPATHLEN(buffer, path) ((buffer) + PAGE_SIZE - (path))

# define free kfree
# define malloc(a) kmalloc(a, GFP_KERNEL)
# define printf printk
int kml_reint_rec(struct file *dir, struct izo_ioctl_data *data);
int izo_get_fileid(struct file *dir, struct izo_ioctl_data *data);
int izo_set_fileid(struct file *dir, struct izo_ioctl_data *data);

#else /* __KERNEL__ */
# include <stdlib.h>
# include <stdio.h>
# include <sys/types.h>
# include <sys/ioctl.h>
# include <string.h>

# define printk printf
# ifndef CERROR
#   define CERROR printf
# endif
# define kmalloc(a,b) malloc(a)

void init_fsreintdata (void);
int kml_fsreint(struct kml_rec *rec, char *basedir);
int kml_iocreint(__u32 size, char *ptr, __u32 offset, int dird,
                 uuid_t uuid, __u32 generate_kml);

static inline int izo_ioctl_packlen(struct izo_ioctl_data *data);

static inline void izo_ioctl_init(struct izo_ioctl_data *data)
{
        memset(data, 0, sizeof(*data));
        data->ioc_len = sizeof(*data);
        data->ioc_version = IZO_IOCTL_VERSION;
}

static inline int
izo_ioctl_pack(struct izo_ioctl_data *data, char **pbuf, int max)
{
        char *ptr;
        struct izo_ioctl_data *overlay;
        data->ioc_len = izo_ioctl_packlen(data);
        data->ioc_version = IZO_IOCTL_VERSION;

        if (*pbuf && izo_ioctl_packlen(data) > max)
                return 1;
        if (*pbuf == NULL)
                *pbuf = malloc(data->ioc_len);
        if (*pbuf == NULL)
                return 1;
        overlay = (struct izo_ioctl_data *)*pbuf;
        memcpy(*pbuf, data, sizeof(*data));

        ptr = overlay->ioc_bulk;
        if (data->ioc_inlbuf1)
                LOGL(data->ioc_inlbuf1, data->ioc_inllen1, ptr);
        if (data->ioc_inlbuf2)
                LOGL(data->ioc_inlbuf2, data->ioc_inllen2, ptr);
        if (izo_ioctl_is_invalid(overlay))
                return 1;

        return 0;
}

#endif /* __KERNEL__*/

#define IZO_ERROR_NAME 1
#define IZO_ERROR_UPDATE 2
#define IZO_ERROR_DELETE 3
#define IZO_ERROR_RENAME 4

static inline char *izo_error(int err)
{
#ifndef __KERNEL__
        if (err <= 0)
                return strerror(-err);
#endif
        switch (err) {
        case IZO_ERROR_NAME:
                return "InterMezzo name/name conflict";
        case IZO_ERROR_UPDATE:
                return "InterMezzo update/update conflict";
        case IZO_ERROR_DELETE:
                return "InterMezzo update/delete conflict";
        case IZO_ERROR_RENAME:
                return "InterMezzo rename/rename conflict";
        }
        return "Unknown InterMezzo error";
}

static inline int izo_ioctl_packlen(struct izo_ioctl_data *data)
{
        int len = sizeof(struct izo_ioctl_data);
        len += size_round(data->ioc_inllen1);
        len += size_round(data->ioc_inllen2);
        return len;
}

static inline int izo_ioctl_is_invalid(struct izo_ioctl_data *data)
{
        if (data->ioc_len > (1<<30)) {
                CERROR("IZO ioctl: ioc_len larger than 1<<30\n");
                return 1;
        }
        if (data->ioc_inllen1 > (1<<30)) {
                CERROR("IZO ioctl: ioc_inllen1 larger than 1<<30\n");
                return 1;
        }
        if (data->ioc_inllen2 > (1<<30)) {
                CERROR("IZO ioctl: ioc_inllen2 larger than 1<<30\n");
                return 1;
        }
        if (data->ioc_inlbuf1 && !data->ioc_inllen1) {
                CERROR("IZO ioctl: inlbuf1 pointer but 0 length\n");
                return 1;
        }
        if (data->ioc_inlbuf2 && !data->ioc_inllen2) {
                CERROR("IZO ioctl: inlbuf2 pointer but 0 length\n");
                return 1;
        }
        if (data->ioc_pbuf1 && !data->ioc_plen1) {
                CERROR("IZO ioctl: pbuf1 pointer but 0 length\n");
                return 1;
        }
        if (data->ioc_pbuf2 && !data->ioc_plen2) {
                CERROR("IZO ioctl: pbuf2 pointer but 0 length\n");
                return 1;
        }
        if (izo_ioctl_packlen(data) != data->ioc_len ) {
                CERROR("IZO ioctl: packlen exceeds ioc_len\n");
                return 1;
        }
        if (data->ioc_inllen1 &&
            data->ioc_bulk[data->ioc_inllen1 - 1] != '\0') {
                CERROR("IZO ioctl: inlbuf1 not 0 terminated\n");
                return 1;
        }
        if (data->ioc_inllen2 &&
            data->ioc_bulk[size_round(data->ioc_inllen1) + data->ioc_inllen2
                           - 1] != '\0') {
                CERROR("IZO ioctl: inlbuf2 not 0 terminated\n");
                return 1;
        }
        return 0;
}

/* kml_unpack.c */
char *kml_print_rec(struct kml_rec *rec, int brief);
int kml_unpack(struct kml_rec *rec, char **buf, char *end);

#endif
