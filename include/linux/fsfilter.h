/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */

#ifndef __FILTER_H_
#define __FILTER_H_ 1

#ifdef __KERNEL__

/* cachetype.c */

/* 
 * it is important that things like inode, super and file operations
 * for intermezzo are not defined statically.  If methods are NULL
 * the VFS takes special action based on that.  Given that different
 * cache types have NULL ops at different slots, we must install opeation 
 * talbes for InterMezzo with NULL's in the same spot
 */

struct filter_ops { 
        struct super_operations filter_sops;

        struct inode_operations filter_dir_iops;
        struct inode_operations filter_file_iops;
        struct inode_operations filter_sym_iops;

        struct file_operations filter_dir_fops;
        struct file_operations filter_file_fops;
        struct file_operations filter_sym_fops;

        struct dentry_operations filter_dentry_ops;
};

struct cache_ops {
        /* operations on the file store */
        struct super_operations *cache_sops;

        struct inode_operations *cache_dir_iops;
        struct inode_operations *cache_file_iops;
        struct inode_operations *cache_sym_iops;

        struct file_operations *cache_dir_fops;
        struct file_operations *cache_file_fops;
        struct file_operations *cache_sym_fops;

        struct dentry_operations *cache_dentry_ops;
};


#define FILTER_DID_SUPER_OPS 0x1
#define FILTER_DID_INODE_OPS 0x2
#define FILTER_DID_FILE_OPS 0x4
#define FILTER_DID_DENTRY_OPS 0x8
#define FILTER_DID_DEV_OPS 0x10
#define FILTER_DID_SYMLINK_OPS 0x20
#define FILTER_DID_DIR_OPS 0x40

struct filter_fs {
        int o_flags;
        struct filter_ops o_fops;
        struct cache_ops  o_caops;
        struct journal_ops *o_trops;
        struct snapshot_ops *o_snops;
};

#define FILTER_FS_TYPES 6
#define FILTER_FS_EXT2 0
#define FILTER_FS_EXT3 1
#define FILTER_FS_REISERFS 2
#define FILTER_FS_XFS 3
#define FILTER_FS_OBDFS 4
#define FILTER_FS_TMPFS 5
extern struct filter_fs filter_oppar[FILTER_FS_TYPES];

struct filter_fs *filter_get_filter_fs(const char *cache_type);
void filter_setup_journal_ops(struct filter_fs *ops, char *cache_type);
inline struct super_operations *filter_c2usops(struct filter_fs *cache);
inline struct inode_operations *filter_c2ufiops(struct filter_fs *cache);
inline struct inode_operations *filter_c2udiops(struct filter_fs *cache);
inline struct inode_operations *filter_c2usiops(struct filter_fs *cache);
inline struct file_operations *filter_c2uffops(struct filter_fs *cache);
inline struct file_operations *filter_c2udfops(struct filter_fs *cache);
inline struct file_operations *filter_c2usfops(struct filter_fs *cache);
inline struct super_operations *filter_c2csops(struct filter_fs *cache);
inline struct inode_operations *filter_c2cfiops(struct filter_fs *cache);
inline struct inode_operations *filter_c2cdiops(struct filter_fs *cache);
inline struct inode_operations *filter_c2csiops(struct filter_fs *cache);
inline struct file_operations *filter_c2cffops(struct filter_fs *cache);
inline struct file_operations *filter_c2cdfops(struct filter_fs *cache);
inline struct file_operations *filter_c2csfops(struct filter_fs *cache);
inline struct dentry_operations *filter_c2cdops(struct filter_fs *cache);
inline struct dentry_operations *filter_c2udops(struct filter_fs *cache);

void filter_setup_super_ops(struct filter_fs *cache, struct super_operations *cache_ops, struct super_operations *filter_sops);
void filter_setup_dir_ops(struct filter_fs *cache, struct inode *cache_inode, struct inode_operations *filter_iops, struct file_operations *ffops);
void filter_setup_file_ops(struct filter_fs *cache, struct inode *cache_inode, struct inode_operations *filter_iops, struct file_operations *filter_op);
void filter_setup_symlink_ops(struct filter_fs *cache, struct inode *cache_inode, struct inode_operations *filter_iops, struct file_operations *filter_op);
void filter_setup_dentry_ops(struct filter_fs *cache, struct dentry_operations *cache_dop,  struct dentry_operations *filter_dop);


#define PRESTO_DEBUG
#ifdef PRESTO_DEBUG
/* debugging masks */
#define D_SUPER     1  
#define D_INODE     2   /* print entry and exit into procedure */
#define D_FILE      4
#define D_CACHE     8   /* cache debugging */
#define D_MALLOC    16  /* print malloc, de-alloc information */
#define D_JOURNAL   32
#define D_UPCALL    64  /* up and downcall debugging */
#define D_PSDEV    128
#define D_PIOCTL   256
#define D_SPECIAL  512
#define D_TIMING  1024
#define D_DOWNCALL 2048

#define FDEBUG(mask, format, a...)                                      \
        do {                                                            \
                if (filter_debug & mask) {                              \
                        printk("(%s,l. %d): ", __FUNCTION__, __LINE__); \
                        printk(format, ##a); }                          \
        } while (0)

#define FENTRY                                                          \
        if(filter_print_entry)                                          \
                printk("Process %d entered %s\n", current->pid, __FUNCTION__)

#define FEXIT                                                           \
        if(filter_print_entry)                                          \
                printk("Process %d leaving %s at %d\n", current->pid,   \
                       __FUNCTION__,__LINE__)
#endif
#endif
#endif
