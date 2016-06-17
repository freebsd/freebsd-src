#ifndef _LINUX_FS_H
#define _LINUX_FS_H

/*
 * This file has definitions for some important file table
 * structures etc.
 */

#include <linux/config.h>
#include <linux/linkage.h>
#include <linux/limits.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/vfs.h>
#include <linux/net.h>
#include <linux/kdev_t.h>
#include <linux/ioctl.h>
#include <linux/list.h>
#include <linux/dcache.h>
#include <linux/stat.h>
#include <linux/cache.h>
#include <linux/stddef.h>
#include <linux/string.h>

#include <asm/atomic.h>
#include <asm/bitops.h>

struct poll_table_struct;


/*
 * It's silly to have NR_OPEN bigger than NR_FILE, but you can change
 * the file limit at runtime and only root can increase the per-process
 * nr_file rlimit, so it's safe to set up a ridiculously high absolute
 * upper limit on files-per-process.
 *
 * Some programs (notably those using select()) may have to be 
 * recompiled to take full advantage of the new limits..  
 */

/* Fixed constants first: */
#undef NR_OPEN
#define NR_OPEN (1024*1024)	/* Absolute upper limit on fd num */
#define INR_OPEN 1024		/* Initial setting for nfile rlimits */

#define BLOCK_SIZE_BITS 10
#define BLOCK_SIZE (1<<BLOCK_SIZE_BITS)

/* And dynamically-tunable limits and defaults: */
struct files_stat_struct {
	int nr_files;		/* read only */
	int nr_free_files;	/* read only */
	int max_files;		/* tunable */
};
extern struct files_stat_struct files_stat;

struct inodes_stat_t {
	int nr_inodes;
	int nr_unused;
	int dummy[5];
};
extern struct inodes_stat_t inodes_stat;

extern int leases_enable, dir_notify_enable, lease_break_time;

#define NR_FILE  8192	/* this can well be larger on a larger system */
#define NR_RESERVED_FILES 10 /* reserved for root */
#define NR_SUPER 256

#define MAY_EXEC 1
#define MAY_WRITE 2
#define MAY_READ 4

#define FMODE_READ 1
#define FMODE_WRITE 2

#define READ 0
#define WRITE 1
#define READA 2		/* read-ahead  - don't block if no resources */
#define SPECIAL 4	/* For non-blockdevice requests in request queue */

#define SEL_IN		1
#define SEL_OUT		2
#define SEL_EX		4

/* public flags for file_system_type */
#define FS_REQUIRES_DEV 1 
#define FS_NO_DCACHE	2 /* Only dcache the necessary things. */
#define FS_NO_PRELIM	4 /* prevent preloading of dentries, even if
			   * FS_NO_DCACHE is not set.
			   */
#define FS_SINGLE	8 /* Filesystem that can have only one superblock */
#define FS_NOMOUNT	16 /* Never mount from userland */
#define FS_LITTER	32 /* Keeps the tree in dcache */
#define FS_ODD_RENAME	32768	/* Temporary stuff; will go away as soon
				  * as nfs_rename() will be cleaned up
				  */
/*
 * These are the fs-independent mount-flags: up to 32 flags are supported
 */
#define MS_RDONLY	 1	/* Mount read-only */
#define MS_NOSUID	 2	/* Ignore suid and sgid bits */
#define MS_NODEV	 4	/* Disallow access to device special files */
#define MS_NOEXEC	 8	/* Disallow program execution */
#define MS_SYNCHRONOUS	16	/* Writes are synced at once */
#define MS_REMOUNT	32	/* Alter flags of a mounted FS */
#define MS_MANDLOCK	64	/* Allow mandatory locks on an FS */
#define MS_NOATIME	1024	/* Do not update access times. */
#define MS_NODIRATIME	2048	/* Do not update directory access times */
#define MS_BIND		4096
#define MS_MOVE		8192
#define MS_REC		16384
#define MS_VERBOSE	32768
#define MS_ACTIVE	(1<<30)
#define MS_NOUSER	(1<<31)

/*
 * Superblock flags that can be altered by MS_REMOUNT
 */
#define MS_RMT_MASK	(MS_RDONLY|MS_SYNCHRONOUS|MS_MANDLOCK|MS_NOATIME|\
			 MS_NODIRATIME)

/*
 * Old magic mount flag and mask
 */
#define MS_MGC_VAL 0xC0ED0000
#define MS_MGC_MSK 0xffff0000

/* Inode flags - they have nothing to superblock flags now */

#define S_SYNC		1	/* Writes are synced at once */
#define S_NOATIME	2	/* Do not update access times */
#define S_QUOTA		4	/* Quota initialized for file */
#define S_APPEND	8	/* Append-only file */
#define S_IMMUTABLE	16	/* Immutable file */
#define S_DEAD		32	/* removed, but still open directory */
#define S_NOQUOTA	64	/* Inode is not counted to quota */

/*
 * Note that nosuid etc flags are inode-specific: setting some file-system
 * flags just means all the inodes inherit those flags by default. It might be
 * possible to override it selectively if you really wanted to with some
 * ioctl() that is not currently implemented.
 *
 * Exception: MS_RDONLY is always applied to the entire file system.
 *
 * Unfortunately, it is possible to change a filesystems flags with it mounted
 * with files in use.  This means that all of the inodes will not have their
 * i_flags updated.  Hence, i_flags no longer inherit the superblock mount
 * flags, so these have to be checked separately. -- rmk@arm.uk.linux.org
 */
#define __IS_FLG(inode,flg) ((inode)->i_sb->s_flags & (flg))

#define IS_RDONLY(inode) ((inode)->i_sb->s_flags & MS_RDONLY)
#define IS_SYNC(inode)		(__IS_FLG(inode, MS_SYNCHRONOUS) || ((inode)->i_flags & S_SYNC))
#define IS_MANDLOCK(inode)	__IS_FLG(inode, MS_MANDLOCK)

#define IS_QUOTAINIT(inode)	((inode)->i_flags & S_QUOTA)
#define IS_NOQUOTA(inode)	((inode)->i_flags & S_NOQUOTA)
#define IS_APPEND(inode)	((inode)->i_flags & S_APPEND)
#define IS_IMMUTABLE(inode)	((inode)->i_flags & S_IMMUTABLE)
#define IS_NOATIME(inode)	(__IS_FLG(inode, MS_NOATIME) || ((inode)->i_flags & S_NOATIME))
#define IS_NODIRATIME(inode)	__IS_FLG(inode, MS_NODIRATIME)

#define IS_DEADDIR(inode)	((inode)->i_flags & S_DEAD)

/* the read-only stuff doesn't really belong here, but any other place is
   probably as bad and I don't want to create yet another include file. */

#define BLKROSET   _IO(0x12,93)	/* set device read-only (0 = read-write) */
#define BLKROGET   _IO(0x12,94)	/* get read-only status (0 = read_write) */
#define BLKRRPART  _IO(0x12,95)	/* re-read partition table */
#define BLKGETSIZE _IO(0x12,96)	/* return device size /512 (long *arg) */
#define BLKFLSBUF  _IO(0x12,97)	/* flush buffer cache */
#define BLKRASET   _IO(0x12,98)	/* Set read ahead for block device */
#define BLKRAGET   _IO(0x12,99)	/* get current read ahead setting */
#define BLKFRASET  _IO(0x12,100)/* set filesystem (mm/filemap.c) read-ahead */
#define BLKFRAGET  _IO(0x12,101)/* get filesystem (mm/filemap.c) read-ahead */
#define BLKSECTSET _IO(0x12,102)/* set max sectors per request (ll_rw_blk.c) */
#define BLKSECTGET _IO(0x12,103)/* get max sectors per request (ll_rw_blk.c) */
#define BLKSSZGET  _IO(0x12,104)/* get block device sector size */
#if 0
#define BLKPG      _IO(0x12,105)/* See blkpg.h */
#define BLKELVGET  _IOR(0x12,106,sizeof(blkelv_ioctl_arg_t))/* elevator get */
#define BLKELVSET  _IOW(0x12,107,sizeof(blkelv_ioctl_arg_t))/* elevator set */
/* This was here just to show that the number is taken -
   probably all these _IO(0x12,*) ioctls should be moved to blkpg.h. */
#endif
/* A jump here: 108-111 have been used for various private purposes. */
#define BLKBSZGET  _IOR(0x12,112,sizeof(int))
#define BLKBSZSET  _IOW(0x12,113,sizeof(int))
#define BLKGETSIZE64 _IOR(0x12,114,sizeof(u64))	/* return device size in bytes (u64 *arg) */

#define BMAP_IOCTL 1		/* obsolete - kept for compatibility */
#define FIBMAP	   _IO(0x00,1)	/* bmap access */
#define FIGETBSZ   _IO(0x00,2)	/* get the block size used for bmap */

#ifdef __KERNEL__

#include <asm/semaphore.h>
#include <asm/byteorder.h>

extern void update_atime (struct inode *);
extern void update_mctime (struct inode *);
#define UPDATE_ATIME(inode) update_atime (inode)

extern void buffer_init(unsigned long);
extern void inode_init(unsigned long);
extern void mnt_init(unsigned long);
extern void files_init(unsigned long mempages);

/* bh state bits */
enum bh_state_bits {
	BH_Uptodate,	/* 1 if the buffer contains valid data */
	BH_Dirty,	/* 1 if the buffer is dirty */
	BH_Lock,	/* 1 if the buffer is locked */
	BH_Req,		/* 0 if the buffer has been invalidated */
	BH_Mapped,	/* 1 if the buffer has a disk mapping */
	BH_New,		/* 1 if the buffer is new and not yet written out */
	BH_Async,	/* 1 if the buffer is under end_buffer_io_async I/O */
	BH_Wait_IO,	/* 1 if we should write out this buffer */
	BH_Launder,	/* 1 if we can throttle on this buffer */
	BH_Attached,	/* 1 if b_inode_buffers is linked into a list */
	BH_JBD,		/* 1 if it has an attached journal_head */
	BH_Sync,	/* 1 if the buffer is a sync read */
	BH_Delay,       /* 1 if the buffer is delayed allocate */

	BH_PrivateStart,/* not a state bit, but the first bit available
			 * for private allocation by other entities
			 */
};

#define MAX_BUF_PER_PAGE (PAGE_CACHE_SIZE / 512)

/*
 * Try to keep the most commonly used fields in single cache lines (16
 * bytes) to improve performance.  This ordering should be
 * particularly beneficial on 32-bit processors.
 * 
 * We use the first 16 bytes for the data which is used in searches
 * over the block hash lists (ie. getblk() and friends).
 * 
 * The second 16 bytes we use for lru buffer scans, as used by
 * sync_buffers() and refill_freelist().  -- sct
 */
struct buffer_head {
	/* First cache line: */
	struct buffer_head *b_next;	/* Hash queue list */
	unsigned long b_blocknr;	/* block number */
	unsigned short b_size;		/* block size */
	unsigned short b_list;		/* List that this buffer appears */
	kdev_t b_dev;			/* device (B_FREE = free) */

	atomic_t b_count;		/* users using this block */
	kdev_t b_rdev;			/* Real device */
	unsigned long b_state;		/* buffer state bitmap (see above) */
	unsigned long b_flushtime;	/* Time when (dirty) buffer should be written */

	struct buffer_head *b_next_free;/* lru/free list linkage */
	struct buffer_head *b_prev_free;/* doubly linked list of buffers */
	struct buffer_head *b_this_page;/* circular list of buffers in one page */
	struct buffer_head *b_reqnext;	/* request queue */

	struct buffer_head **b_pprev;	/* doubly linked list of hash-queue */
	char * b_data;			/* pointer to data block */
	struct page *b_page;		/* the page this bh is mapped to */
	void (*b_end_io)(struct buffer_head *bh, int uptodate); /* I/O completion */
 	void *b_private;		/* reserved for b_end_io */

	unsigned long b_rsector;	/* Real buffer location on disk */
	wait_queue_head_t b_wait;

	struct list_head     b_inode_buffers;	/* doubly linked list of inode dirty buffers */
};

typedef void (bh_end_io_t)(struct buffer_head *bh, int uptodate);
void init_buffer(struct buffer_head *, bh_end_io_t *, void *);

#define __buffer_state(bh, state)	(((bh)->b_state & (1UL << BH_##state)) != 0)

#define buffer_uptodate(bh)	__buffer_state(bh,Uptodate)
#define buffer_dirty(bh)	__buffer_state(bh,Dirty)
#define buffer_locked(bh)	__buffer_state(bh,Lock)
#define buffer_req(bh)		__buffer_state(bh,Req)
#define buffer_mapped(bh)	__buffer_state(bh,Mapped)
#define buffer_new(bh)		__buffer_state(bh,New)
#define buffer_async(bh)	__buffer_state(bh,Async)
#define buffer_launder(bh)	__buffer_state(bh,Launder)
#define buffer_delay(bh)	__buffer_state(bh,Delay)

#define bh_offset(bh)		((unsigned long)(bh)->b_data & ~PAGE_MASK)

extern void set_bh_page(struct buffer_head *bh, struct page *page, unsigned long offset);

#define touch_buffer(bh)	mark_page_accessed(bh->b_page)


#include <linux/pipe_fs_i.h>
#include <linux/minix_fs_i.h>
#include <linux/ext2_fs_i.h>
#include <linux/ext3_fs_i.h>
#include <linux/hpfs_fs_i.h>
#include <linux/ntfs_fs_i.h>
#include <linux/msdos_fs_i.h>
#include <linux/umsdos_fs_i.h>
#include <linux/iso_fs_i.h>
#include <linux/nfs_fs_i.h>
#include <linux/sysv_fs_i.h>
#include <linux/affs_fs_i.h>
#include <linux/ufs_fs_i.h>
#include <linux/efs_fs_i.h>
#include <linux/coda_fs_i.h>
#include <linux/romfs_fs_i.h>
#include <linux/shmem_fs.h>
#include <linux/smb_fs_i.h>
#include <linux/hfs_fs_i.h>
#include <linux/adfs_fs_i.h>
#include <linux/qnx4_fs_i.h>
#include <linux/reiserfs_fs_i.h>
#include <linux/bfs_fs_i.h>
#include <linux/udf_fs_i.h>
#include <linux/ncp_fs_i.h>
#include <linux/proc_fs_i.h>
#include <linux/usbdev_fs_i.h>
#include <linux/jffs2_fs_i.h>
#include <linux/cramfs_fs_sb.h>

/*
 * Attribute flags.  These should be or-ed together to figure out what
 * has been changed!
 */
#define ATTR_MODE	1
#define ATTR_UID	2
#define ATTR_GID	4
#define ATTR_SIZE	8
#define ATTR_ATIME	16
#define ATTR_MTIME	32
#define ATTR_CTIME	64
#define ATTR_ATIME_SET	128
#define ATTR_MTIME_SET	256
#define ATTR_FORCE	512	/* Not a change, but a change it */
#define ATTR_ATTR_FLAG	1024

/*
 * This is the Inode Attributes structure, used for notify_change().  It
 * uses the above definitions as flags, to know which values have changed.
 * Also, in this manner, a Filesystem can look at only the values it cares
 * about.  Basically, these are the attributes that the VFS layer can
 * request to change from the FS layer.
 *
 * Derek Atkins <warlord@MIT.EDU> 94-10-20
 */
struct iattr {
	unsigned int	ia_valid;
	umode_t		ia_mode;
	uid_t		ia_uid;
	gid_t		ia_gid;
	loff_t		ia_size;
	time_t		ia_atime;
	time_t		ia_mtime;
	time_t		ia_ctime;
	unsigned int	ia_attr_flags;
};

/*
 * This is the inode attributes flag definitions
 */
#define ATTR_FLAG_SYNCRONOUS	1 	/* Syncronous write */
#define ATTR_FLAG_NOATIME	2 	/* Don't update atime */
#define ATTR_FLAG_APPEND	4 	/* Append-only file */
#define ATTR_FLAG_IMMUTABLE	8 	/* Immutable file */
#define ATTR_FLAG_NODIRATIME	16 	/* Don't update atime for directory */

/*
 * Includes for diskquotas and mount structures.
 */
#include <linux/quota.h>
#include <linux/mount.h>

/*
 * oh the beauties of C type declarations.
 */
struct page;
struct address_space;
struct kiobuf;

struct address_space_operations {
	int (*writepage)(struct page *);
	int (*readpage)(struct file *, struct page *);
	int (*sync_page)(struct page *);
	/*
	 * ext3 requires that a successful prepare_write() call be followed
	 * by a commit_write() call - they must be balanced
	 */
	int (*prepare_write)(struct file *, struct page *, unsigned, unsigned);
	int (*commit_write)(struct file *, struct page *, unsigned, unsigned);
	/* Unfortunately this kludge is needed for FIBMAP. Don't use it */
	int (*bmap)(struct address_space *, long);
	int (*flushpage) (struct page *, unsigned long);
	int (*releasepage) (struct page *, int);
#define KERNEL_HAS_O_DIRECT /* this is for modules out of the kernel */
	int (*direct_IO)(int, struct inode *, struct kiobuf *, unsigned long, int);
#define KERNEL_HAS_DIRECT_FILEIO /* Unfortunate kludge due to lack of foresight */
	int (*direct_fileIO)(int, struct file *, struct kiobuf *, unsigned long, int);
	void (*removepage)(struct page *); /* called when page gets removed from the inode */
};

struct address_space {
	struct list_head	clean_pages;	/* list of clean pages */
	struct list_head	dirty_pages;	/* list of dirty pages */
	struct list_head	locked_pages;	/* list of locked pages */
	unsigned long		nrpages;	/* number of total pages */
	struct address_space_operations *a_ops;	/* methods */
	struct inode		*host;		/* owner: inode, block_device */
	struct vm_area_struct	*i_mmap;	/* list of private mappings */
	struct vm_area_struct	*i_mmap_shared; /* list of shared mappings */
	spinlock_t		i_shared_lock;  /* and spinlock protecting it */
	int			gfp_mask;	/* how to allocate the pages */
};

struct char_device {
	struct list_head	hash;
	atomic_t		count;
	dev_t			dev;
	atomic_t		openers;
	struct semaphore	sem;
};

struct block_device {
	struct list_head	bd_hash;
	atomic_t		bd_count;
	struct inode *		bd_inode;
	dev_t			bd_dev;  /* not a kdev_t - it's a search key */
	int			bd_openers;
	const struct block_device_operations *bd_op;
	struct semaphore	bd_sem;	/* open/close mutex */
	struct list_head	bd_inodes;
};

struct inode {
	struct list_head	i_hash;
	struct list_head	i_list;
	struct list_head	i_dentry;
	
	struct list_head	i_dirty_buffers;
	struct list_head	i_dirty_data_buffers;

	unsigned long		i_ino;
	atomic_t		i_count;
	kdev_t			i_dev;
	umode_t			i_mode;
	nlink_t			i_nlink;
	uid_t			i_uid;
	gid_t			i_gid;
	kdev_t			i_rdev;
	loff_t			i_size;
	time_t			i_atime;
	time_t			i_mtime;
	time_t			i_ctime;
	unsigned int		i_blkbits;
	unsigned long		i_blksize;
	unsigned long		i_blocks;
	unsigned long		i_version;
	unsigned short          i_bytes;
	struct semaphore	i_sem;
	struct rw_semaphore	i_alloc_sem;
	struct semaphore	i_zombie;
	struct inode_operations	*i_op;
	struct file_operations	*i_fop;	/* former ->i_op->default_file_ops */
	struct super_block	*i_sb;
	wait_queue_head_t	i_wait;
	struct file_lock	*i_flock;
	struct address_space	*i_mapping;
	struct address_space	i_data;
	struct dquot		*i_dquot[MAXQUOTAS];
	/* These three should probably be a union */
	struct list_head	i_devices;
	struct pipe_inode_info	*i_pipe;
	struct block_device	*i_bdev;
	struct char_device	*i_cdev;

	unsigned long		i_dnotify_mask; /* Directory notify events */
	struct dnotify_struct	*i_dnotify; /* for directory notifications */

	unsigned long		i_state;

	unsigned int		i_flags;
	unsigned char		i_sock;

	atomic_t		i_writecount;
	unsigned int		i_attr_flags;
	__u32			i_generation;
	union {
		struct minix_inode_info		minix_i;
		struct ext2_inode_info		ext2_i;
		struct ext3_inode_info		ext3_i;
		struct hpfs_inode_info		hpfs_i;
		struct ntfs_inode_info		ntfs_i;
		struct msdos_inode_info		msdos_i;
		struct umsdos_inode_info	umsdos_i;
		struct iso_inode_info		isofs_i;
		struct nfs_inode_info		nfs_i;
		struct sysv_inode_info		sysv_i;
		struct affs_inode_info		affs_i;
		struct ufs_inode_info		ufs_i;
		struct efs_inode_info		efs_i;
		struct romfs_inode_info		romfs_i;
		struct shmem_inode_info		shmem_i;
		struct coda_inode_info		coda_i;
		struct smb_inode_info		smbfs_i;
		struct hfs_inode_info		hfs_i;
		struct adfs_inode_info		adfs_i;
		struct qnx4_inode_info		qnx4_i;
		struct reiserfs_inode_info	reiserfs_i;
		struct bfs_inode_info		bfs_i;
		struct udf_inode_info		udf_i;
		struct ncp_inode_info		ncpfs_i;
		struct proc_inode_info		proc_i;
		struct socket			socket_i;
		struct usbdev_inode_info        usbdev_i;
		struct jffs2_inode_info		jffs2_i;
		void				*generic_ip;
	} u;
};

static inline void inode_add_bytes(struct inode *inode, loff_t bytes)
{
	inode->i_blocks += bytes >> 9;
	bytes &= 511;
	inode->i_bytes += bytes;
	if (inode->i_bytes >= 512) {
		inode->i_blocks++;
		inode->i_bytes -= 512;
	}
}

static inline void inode_sub_bytes(struct inode *inode, loff_t bytes)
{
	inode->i_blocks -= bytes >> 9;
	bytes &= 511;
	if (inode->i_bytes < bytes) {
		inode->i_blocks--;
		inode->i_bytes += 512;
	}
	inode->i_bytes -= bytes;
}

static inline loff_t inode_get_bytes(struct inode *inode)
{
	return (((loff_t)inode->i_blocks) << 9) + inode->i_bytes;
}

static inline void inode_set_bytes(struct inode *inode, loff_t bytes)
{
	inode->i_blocks = bytes >> 9;
	inode->i_bytes = bytes & 511;
}

struct fown_struct {
	int pid;		/* pid or -pgrp where SIGIO should be sent */
	uid_t uid, euid;	/* uid/euid of process setting the owner */
	int signum;		/* posix.1b rt signal to be delivered on IO */
};

struct file {
	struct list_head	f_list;
	struct dentry		*f_dentry;
	struct vfsmount         *f_vfsmnt;
	struct file_operations	*f_op;
	atomic_t		f_count;
	unsigned int 		f_flags;
	mode_t			f_mode;
	loff_t			f_pos;
	unsigned long 		f_reada, f_ramax, f_raend, f_ralen, f_rawin;
	struct fown_struct	f_owner;
	unsigned int		f_uid, f_gid;
	int			f_error;

	unsigned long		f_version;

	/* needed for tty driver, and maybe others */
	void			*private_data;

	/* preallocated helper kiobuf to speedup O_DIRECT */
	struct kiobuf		*f_iobuf;
	long			f_iobuf_lock;
};
extern spinlock_t files_lock;
#define file_list_lock() spin_lock(&files_lock);
#define file_list_unlock() spin_unlock(&files_lock);

#define get_file(x)	atomic_inc(&(x)->f_count)
#define file_count(x)	atomic_read(&(x)->f_count)

extern int init_private_file(struct file *, struct dentry *, int);

#define	MAX_NON_LFS	((1UL<<31) - 1)

/* Page cache limit. The filesystems should put that into their s_maxbytes 
   limits, otherwise bad things can happen in VM. */ 
#if BITS_PER_LONG==32
#define MAX_LFS_FILESIZE	(((u64)PAGE_CACHE_SIZE << (BITS_PER_LONG-1))-1) 
#elif BITS_PER_LONG==64
#define MAX_LFS_FILESIZE 	0x7fffffffffffffff
#endif

#define FL_POSIX	1
#define FL_FLOCK	2
#define FL_BROKEN	4	/* broken flock() emulation */
#define FL_ACCESS	8	/* for processes suspended by mandatory locking */
#define FL_LOCKD	16	/* lock held by rpc.lockd */
#define FL_LEASE	32	/* lease held on this file */

/*
 * The POSIX file lock owner is determined by
 * the "struct files_struct" in the thread group
 * (or NULL for no owner - BSD locks).
 *
 * Lockd stuffs a "host" pointer into this.
 */
typedef struct files_struct *fl_owner_t;

struct file_lock {
	struct file_lock *fl_next;	/* singly linked list for this inode  */
	struct list_head fl_link;	/* doubly linked list of all locks */
	struct list_head fl_block;	/* circular list of blocked processes */
	fl_owner_t fl_owner;
	unsigned int fl_pid;
	wait_queue_head_t fl_wait;
	struct file *fl_file;
	unsigned char fl_flags;
	unsigned char fl_type;
	loff_t fl_start;
	loff_t fl_end;

	void (*fl_notify)(struct file_lock *);	/* unblock callback */
	void (*fl_insert)(struct file_lock *);	/* lock insertion callback */
	void (*fl_remove)(struct file_lock *);	/* lock removal callback */

	struct fasync_struct *	fl_fasync; /* for lease break notifications */
	unsigned long fl_break_time;	/* for nonblocking lease breaks */

	union {
		struct nfs_lock_info	nfs_fl;
	} fl_u;
};

/* The following constant reflects the upper bound of the file/locking space */
#ifndef OFFSET_MAX
#define INT_LIMIT(x)	(~((x)1 << (sizeof(x)*8 - 1)))
#define OFFSET_MAX	INT_LIMIT(loff_t)
#define OFFT_OFFSET_MAX	INT_LIMIT(off_t)
#endif

extern struct list_head file_lock_list;

#include <linux/fcntl.h>

extern int fcntl_getlk(unsigned int, struct flock *);
extern int fcntl_setlk(unsigned int, unsigned int, struct flock *);

extern int fcntl_getlk64(unsigned int, struct flock64 *);
extern int fcntl_setlk64(unsigned int, unsigned int, struct flock64 *);

/* fs/locks.c */
extern void locks_init_lock(struct file_lock *);
extern void locks_copy_lock(struct file_lock *, struct file_lock *);
extern void locks_remove_posix(struct file *, fl_owner_t);
extern void locks_remove_flock(struct file *);
extern struct file_lock *posix_test_lock(struct file *, struct file_lock *);
extern int posix_lock_file(struct file *, struct file_lock *, unsigned int);
extern void posix_block_lock(struct file_lock *, struct file_lock *);
extern void posix_unblock_lock(struct file_lock *);
extern int posix_locks_deadlock(struct file_lock *, struct file_lock *);
extern int __get_lease(struct inode *inode, unsigned int flags);
extern time_t lease_get_mtime(struct inode *);
extern int lock_may_read(struct inode *, loff_t start, unsigned long count);
extern int lock_may_write(struct inode *, loff_t start, unsigned long count);
extern void steal_locks(fl_owner_t from);

struct fasync_struct {
	int	magic;
	int	fa_fd;
	struct	fasync_struct	*fa_next; /* singly linked list */
	struct	file 		*fa_file;
};

#define FASYNC_MAGIC 0x4601

/* SMP safe fasync helpers: */
extern int fasync_helper(int, struct file *, int, struct fasync_struct **);
/* can be called from interrupts */
extern void kill_fasync(struct fasync_struct **, int, int);
/* only for net: no internal synchronization */
extern void __kill_fasync(struct fasync_struct *, int, int);

struct nameidata {
	struct dentry *dentry;
	struct vfsmount *mnt;
	struct qstr last;
	unsigned int flags;
	int last_type;
};

/*
 *	Umount options
 */

#define MNT_FORCE	0x00000001	/* Attempt to forcibily umount */
#define MNT_DETACH	0x00000002	/* Just detach from the tree */

#include <linux/minix_fs_sb.h>
#include <linux/ext2_fs_sb.h>
#include <linux/ext3_fs_sb.h>
#include <linux/hpfs_fs_sb.h>
#include <linux/ntfs_fs_sb.h>
#include <linux/msdos_fs_sb.h>
#include <linux/iso_fs_sb.h>
#include <linux/nfs_fs_sb.h>
#include <linux/sysv_fs_sb.h>
#include <linux/affs_fs_sb.h>
#include <linux/ufs_fs_sb.h>
#include <linux/efs_fs_sb.h>
#include <linux/romfs_fs_sb.h>
#include <linux/smb_fs_sb.h>
#include <linux/hfs_fs_sb.h>
#include <linux/adfs_fs_sb.h>
#include <linux/qnx4_fs_sb.h>
#include <linux/reiserfs_fs_sb.h>
#include <linux/bfs_fs_sb.h>
#include <linux/udf_fs_sb.h>
#include <linux/ncp_fs_sb.h>
#include <linux/usbdev_fs_sb.h>
#include <linux/cramfs_fs_sb.h>
#include <linux/jffs2_fs_sb.h>

extern struct list_head super_blocks;
extern spinlock_t sb_lock;

#define sb_entry(list)	list_entry((list), struct super_block, s_list)
#define S_BIAS (1<<30)
struct super_block {
	struct list_head	s_list;		/* Keep this first */
	kdev_t			s_dev;
	unsigned long		s_blocksize;
	unsigned char		s_blocksize_bits;
	unsigned char		s_dirt;
	unsigned long long	s_maxbytes;	/* Max file size */
	struct file_system_type	*s_type;
	struct super_operations	*s_op;
	struct dquot_operations	*dq_op;
	struct quotactl_ops	*s_qcop;
	unsigned long		s_flags;
	unsigned long		s_magic;
	struct dentry		*s_root;
	struct rw_semaphore	s_umount;
	struct semaphore	s_lock;
	int			s_count;
	atomic_t		s_active;

	struct list_head	s_dirty;	/* dirty inodes */
	struct list_head	s_locked_inodes;/* inodes being synced */
	struct list_head	s_files;

	struct block_device	*s_bdev;
	struct list_head	s_instances;
	struct quota_info	s_dquot;	/* Diskquota specific options */

	union {
		struct minix_sb_info	minix_sb;
		struct ext2_sb_info	ext2_sb;
		struct ext3_sb_info	ext3_sb;
		struct hpfs_sb_info	hpfs_sb;
		struct ntfs_sb_info	ntfs_sb;
		struct msdos_sb_info	msdos_sb;
		struct isofs_sb_info	isofs_sb;
		struct nfs_sb_info	nfs_sb;
		struct sysv_sb_info	sysv_sb;
		struct affs_sb_info	affs_sb;
		struct ufs_sb_info	ufs_sb;
		struct efs_sb_info	efs_sb;
		struct shmem_sb_info	shmem_sb;
		struct romfs_sb_info	romfs_sb;
		struct smb_sb_info	smbfs_sb;
		struct hfs_sb_info	hfs_sb;
		struct adfs_sb_info	adfs_sb;
		struct qnx4_sb_info	qnx4_sb;
		struct reiserfs_sb_info	reiserfs_sb;
		struct bfs_sb_info	bfs_sb;
		struct udf_sb_info	udf_sb;
		struct ncp_sb_info	ncpfs_sb;
		struct usbdev_sb_info   usbdevfs_sb;
		struct jffs2_sb_info	jffs2_sb;
		struct cramfs_sb_info	cramfs_sb;
		void			*generic_sbp;
	} u;
	/*
	 * The next field is for VFS *only*. No filesystems have any business
	 * even looking at it. You had been warned.
	 */
	struct semaphore s_vfs_rename_sem;	/* Kludge */

	/* The next field is used by knfsd when converting a (inode number based)
	 * file handle into a dentry. As it builds a path in the dcache tree from
	 * the bottom up, there may for a time be a subpath of dentrys which is not
	 * connected to the main tree.  This semaphore ensure that there is only ever
	 * one such free path per filesystem.  Note that unconnected files (or other
	 * non-directories) are allowed, but not unconnected diretories.
	 */
	struct semaphore s_nfsd_free_path_sem;
};

/*
 * VFS helper functions..
 */
extern int vfs_create(struct inode *, struct dentry *, int);
extern int vfs_mkdir(struct inode *, struct dentry *, int);
extern int vfs_mknod(struct inode *, struct dentry *, int, dev_t);
extern int vfs_symlink(struct inode *, struct dentry *, const char *);
extern int vfs_link(struct dentry *, struct inode *, struct dentry *);
extern int vfs_rmdir(struct inode *, struct dentry *);
extern int vfs_unlink(struct inode *, struct dentry *);
extern int vfs_rename(struct inode *, struct dentry *, struct inode *, struct dentry *);

/*
 * File types
 */
#define DT_UNKNOWN	0
#define DT_FIFO		1
#define DT_CHR		2
#define DT_DIR		4
#define DT_BLK		6
#define DT_REG		8
#define DT_LNK		10
#define DT_SOCK		12
#define DT_WHT		14

/*
 * This is the "filldir" function type, used by readdir() to let
 * the kernel specify what kind of dirent layout it wants to have.
 * This allows the kernel to read directories into kernel space or
 * to have different dirent layouts depending on the binary type.
 */
typedef int (*filldir_t)(void *, const char *, int, loff_t, ino_t, unsigned);

struct block_device_operations {
	int (*open) (struct inode *, struct file *);
	int (*release) (struct inode *, struct file *);
	int (*ioctl) (struct inode *, struct file *, unsigned, unsigned long);
	int (*check_media_change) (kdev_t);
	int (*revalidate) (kdev_t);
	struct module *owner;
};

/*
 * NOTE:
 * read, write, poll, fsync, readv, writev can be called
 *   without the big kernel lock held in all filesystems.
 */
struct file_operations {
	struct module *owner;
	loff_t (*llseek) (struct file *, loff_t, int);
	ssize_t (*read) (struct file *, char *, size_t, loff_t *);
	ssize_t (*write) (struct file *, const char *, size_t, loff_t *);
	int (*readdir) (struct file *, void *, filldir_t);
	unsigned int (*poll) (struct file *, struct poll_table_struct *);
	int (*ioctl) (struct inode *, struct file *, unsigned int, unsigned long);
	int (*mmap) (struct file *, struct vm_area_struct *);
	int (*open) (struct inode *, struct file *);
	int (*flush) (struct file *);
	int (*release) (struct inode *, struct file *);
	int (*fsync) (struct file *, struct dentry *, int datasync);
	int (*fasync) (int, struct file *, int);
	int (*lock) (struct file *, int, struct file_lock *);
	ssize_t (*readv) (struct file *, const struct iovec *, unsigned long, loff_t *);
	ssize_t (*writev) (struct file *, const struct iovec *, unsigned long, loff_t *);
	ssize_t (*sendpage) (struct file *, struct page *, int, size_t, loff_t *, int);
	unsigned long (*get_unmapped_area)(struct file *, unsigned long, unsigned long, unsigned long, unsigned long);
};

struct inode_operations {
	int (*create) (struct inode *,struct dentry *,int);
	struct dentry * (*lookup) (struct inode *,struct dentry *);
	int (*link) (struct dentry *,struct inode *,struct dentry *);
	int (*unlink) (struct inode *,struct dentry *);
	int (*symlink) (struct inode *,struct dentry *,const char *);
	int (*mkdir) (struct inode *,struct dentry *,int);
	int (*rmdir) (struct inode *,struct dentry *);
	int (*mknod) (struct inode *,struct dentry *,int,int);
	int (*rename) (struct inode *, struct dentry *,
			struct inode *, struct dentry *);
	int (*readlink) (struct dentry *, char *,int);
	int (*follow_link) (struct dentry *, struct nameidata *);
	void (*truncate) (struct inode *);
	int (*permission) (struct inode *, int);
	int (*revalidate) (struct dentry *);
	int (*setattr) (struct dentry *, struct iattr *);
	int (*getattr) (struct dentry *, struct iattr *);
	int (*setxattr) (struct dentry *, const char *, void *, size_t, int);
	ssize_t (*getxattr) (struct dentry *, const char *, void *, size_t);
	ssize_t (*listxattr) (struct dentry *, char *, size_t);
	int (*removexattr) (struct dentry *, const char *);
};

struct seq_file;

/*
 * NOTE: write_inode, delete_inode, clear_inode, put_inode can be called
 * without the big kernel lock held in all filesystems.
 */
struct super_operations {
   	struct inode *(*alloc_inode)(struct super_block *sb);
	void (*destroy_inode)(struct inode *);

	void (*read_inode) (struct inode *);
  
  	/* reiserfs kludge.  reiserfs needs 64 bits of information to
    	** find an inode.  We are using the read_inode2 call to get
   	** that information.  We don't like this, and are waiting on some
   	** VFS changes for the real solution.
   	** iget4 calls read_inode2, iff it is defined
   	*/
    	void (*read_inode2) (struct inode *, void *) ;
   	void (*dirty_inode) (struct inode *);
	void (*write_inode) (struct inode *, int);
	void (*put_inode) (struct inode *);
	void (*delete_inode) (struct inode *);
	void (*put_super) (struct super_block *);
	void (*write_super) (struct super_block *);
	int (*sync_fs) (struct super_block *);
	void (*write_super_lockfs) (struct super_block *);
	void (*unlockfs) (struct super_block *);
	int (*statfs) (struct super_block *, struct statfs *);
	int (*remount_fs) (struct super_block *, int *, char *);
	void (*clear_inode) (struct inode *);
	void (*umount_begin) (struct super_block *);

	/* Following are for knfsd to interact with "interesting" filesystems
	 * Currently just reiserfs, but possibly FAT and others later
	 *
	 * fh_to_dentry is given a filehandle fragement with length, and a type flag
	 *   and must return a dentry for the referenced object or, if "parent" is
	 *   set, a dentry for the parent of the object.
	 *   If a dentry cannot be found, a "root" dentry should be created and
	 *   flaged as DCACHE_NFSD_DISCONNECTED. nfsd_iget is an example implementation.
	 *
	 * dentry_to_fh is given a dentry and must generate the filesys specific
	 *   part of the file handle.  Available length is passed in *lenp and used
	 *   length should be returned therein.
	 *   If need_parent is set, then dentry_to_fh should encode sufficient information
	 *   to find the (current) parent.
	 *   dentry_to_fh should return a 1byte "type" which will be passed back in
	 *   the fhtype arguement to fh_to_dentry.  Type of 0 is reserved.
	 *   If filesystem was exportable before the introduction of fh_to_dentry,
	 *   types 1 and 2 should be used is that same way as the generic code.
	 *   Type 255 means error.
	 *
	 * Lengths are in units of 4bytes, not bytes.
	 */
	struct dentry * (*fh_to_dentry)(struct super_block *sb, __u32 *fh, int len, int fhtype, int parent);
	int (*dentry_to_fh)(struct dentry *, __u32 *fh, int *lenp, int need_parent);
	int (*show_options)(struct seq_file *, struct vfsmount *);
};

/* Inode state bits.. */
#define I_DIRTY_SYNC		1 /* Not dirty enough for O_DATASYNC */
#define I_DIRTY_DATASYNC	2 /* Data-related inode changes pending */
#define I_DIRTY_PAGES		4 /* Data-related inode changes pending */
#define I_LOCK			8
#define I_FREEING		16
#define I_CLEAR			32
#define I_NEW			64

#define I_DIRTY (I_DIRTY_SYNC | I_DIRTY_DATASYNC | I_DIRTY_PAGES)

extern void __mark_inode_dirty(struct inode *, int);
static inline void mark_inode_dirty(struct inode *inode)
{
	__mark_inode_dirty(inode, I_DIRTY);
}

static inline void mark_inode_dirty_sync(struct inode *inode)
{
	__mark_inode_dirty(inode, I_DIRTY_SYNC);
}

static inline void mark_inode_dirty_pages(struct inode *inode)
{
	__mark_inode_dirty(inode, I_DIRTY_PAGES);
}

struct file_system_type {
	const char *name;
	int fs_flags;
	struct super_block *(*read_super) (struct super_block *, void *, int);
	struct module *owner;
	struct file_system_type * next;
	struct list_head fs_supers;
};

#define DECLARE_FSTYPE(var,type,read,flags) \
struct file_system_type var = { \
	name:		type, \
	read_super:	read, \
	fs_flags:	flags, \
	owner:		THIS_MODULE, \
}

#define DECLARE_FSTYPE_DEV(var,type,read) \
	DECLARE_FSTYPE(var,type,read,FS_REQUIRES_DEV)

/* Alas, no aliases. Too much hassle with bringing module.h everywhere */
#define fops_get(fops) \
	(((fops) && (fops)->owner)	\
		? ( try_inc_mod_count((fops)->owner) ? (fops) : NULL ) \
		: (fops))

#define fops_put(fops) \
do {	\
	if ((fops) && (fops)->owner) \
		__MOD_DEC_USE_COUNT((fops)->owner);	\
} while(0)

extern int register_filesystem(struct file_system_type *);
extern int unregister_filesystem(struct file_system_type *);
extern struct vfsmount *kern_mount(struct file_system_type *);
extern int may_umount(struct vfsmount *);
extern long do_mount(char *, char *, char *, unsigned long, void *);

#define kern_umount mntput

extern int vfs_statfs(struct super_block *, struct statfs *);

/* Return value for VFS lock functions - tells locks.c to lock conventionally
 * REALLY kosha for root NFS and nfs_lock
 */ 
#define LOCK_USE_CLNT 1

#define FLOCK_VERIFY_READ  1
#define FLOCK_VERIFY_WRITE 2

extern int locks_mandatory_locked(struct inode *);
extern int locks_mandatory_area(int, struct inode *, struct file *, loff_t, size_t);

/*
 * Candidates for mandatory locking have the setgid bit set
 * but no group execute bit -  an otherwise meaningless combination.
 */
#define MANDATORY_LOCK(inode) \
	(IS_MANDLOCK(inode) && ((inode)->i_mode & (S_ISGID | S_IXGRP)) == S_ISGID)

static inline int locks_verify_locked(struct inode *inode)
{
	if (MANDATORY_LOCK(inode))
		return locks_mandatory_locked(inode);
	return 0;
}

static inline int locks_verify_area(int read_write, struct inode *inode,
				    struct file *filp, loff_t offset,
				    size_t count)
{
	if (inode->i_flock && MANDATORY_LOCK(inode))
		return locks_mandatory_area(read_write, inode, filp, offset, count);
	return 0;
}

static inline int locks_verify_truncate(struct inode *inode,
				    struct file *filp,
				    loff_t size)
{
	if (inode->i_flock && MANDATORY_LOCK(inode))
		return locks_mandatory_area(
			FLOCK_VERIFY_WRITE, inode, filp,
			size < inode->i_size ? size : inode->i_size,
			(size < inode->i_size ? inode->i_size - size
			 : size - inode->i_size)
		);
	return 0;
}

static inline int get_lease(struct inode *inode, unsigned int mode)
{
	if (inode->i_flock)
		return __get_lease(inode, mode);
	return 0;
}

/* fs/open.c */

asmlinkage long sys_open(const char *, int, int);
asmlinkage long sys_close(unsigned int);	/* yes, it's really unsigned */
extern int do_truncate(struct dentry *, loff_t start);

extern struct file *filp_open(const char *, int, int);
extern struct file * dentry_open(struct dentry *, struct vfsmount *, int);
extern int filp_close(struct file *, fl_owner_t id);
extern char * getname(const char *);

/* fs/dcache.c */
extern void vfs_caches_init(unsigned long);

#define __getname()	kmem_cache_alloc(names_cachep, SLAB_KERNEL)
#define putname(name)	kmem_cache_free(names_cachep, (void *)(name))

enum {BDEV_FILE, BDEV_SWAP, BDEV_FS, BDEV_RAW};
extern int register_blkdev(unsigned int, const char *, struct block_device_operations *);
extern int unregister_blkdev(unsigned int, const char *);
extern struct block_device *bdget(dev_t);
extern int bd_acquire(struct inode *inode);
extern void bd_forget(struct inode *inode);
extern void bdput(struct block_device *);
extern struct char_device *cdget(dev_t);
extern void cdput(struct char_device *);
extern int blkdev_open(struct inode *, struct file *);
extern int blkdev_close(struct inode *, struct file *);
extern struct file_operations def_blk_fops;
extern struct address_space_operations def_blk_aops;
extern struct file_operations def_fifo_fops;
extern int ioctl_by_bdev(struct block_device *, unsigned, unsigned long);
extern int blkdev_get(struct block_device *, mode_t, unsigned, int);
extern int blkdev_put(struct block_device *, int);

/* fs/devices.c */
extern const struct block_device_operations *get_blkfops(unsigned int);
extern int register_chrdev(unsigned int, const char *, struct file_operations *);
extern int unregister_chrdev(unsigned int, const char *);
extern int chrdev_open(struct inode *, struct file *);
extern const char * bdevname(kdev_t);
extern const char * cdevname(kdev_t);
extern const char * kdevname(kdev_t);
extern void init_special_inode(struct inode *, umode_t, int);

/* Invalid inode operations -- fs/bad_inode.c */
extern void make_bad_inode(struct inode *);
extern int is_bad_inode(struct inode *);

extern struct file_operations read_fifo_fops;
extern struct file_operations write_fifo_fops;
extern struct file_operations rdwr_fifo_fops;
extern struct file_operations read_pipe_fops;
extern struct file_operations write_pipe_fops;
extern struct file_operations rdwr_pipe_fops;

extern int fs_may_remount_ro(struct super_block *);

extern int FASTCALL(try_to_free_buffers(struct page *, unsigned int));
extern void refile_buffer(struct buffer_head * buf);
extern void create_empty_buffers(struct page *, kdev_t, unsigned long);
extern void end_buffer_io_sync(struct buffer_head *bh, int uptodate);
extern void end_buffer_io_async(struct buffer_head *bh, int uptodate);

/* reiserfs_writepage needs this */
extern void set_buffer_async_io(struct buffer_head *bh) ;

#define BUF_CLEAN	0
#define BUF_LOCKED	1	/* Buffers scheduled for write */
#define BUF_DIRTY	2	/* Dirty buffers, not yet scheduled for write */
#define NR_LIST		3

static inline void get_bh(struct buffer_head * bh)
{
        atomic_inc(&(bh)->b_count);
}

static inline void put_bh(struct buffer_head *bh)
{
        smp_mb__before_atomic_dec();
        atomic_dec(&bh->b_count);
}

/*
 * This is called by bh->b_end_io() handlers when I/O has completed.
 */
static inline void mark_buffer_uptodate(struct buffer_head * bh, int on)
{
	if (on)
		set_bit(BH_Uptodate, &bh->b_state);
	else
		clear_bit(BH_Uptodate, &bh->b_state);
}

#define atomic_set_buffer_clean(bh) test_and_clear_bit(BH_Dirty, &(bh)->b_state)

static inline void __mark_buffer_clean(struct buffer_head *bh)
{
	refile_buffer(bh);
}

static inline void mark_buffer_clean(struct buffer_head * bh)
{
	if (atomic_set_buffer_clean(bh))
		__mark_buffer_clean(bh);
}

extern void FASTCALL(__mark_dirty(struct buffer_head *bh));
extern void FASTCALL(__mark_buffer_dirty(struct buffer_head *bh));
extern void FASTCALL(mark_buffer_dirty(struct buffer_head *bh));

extern void FASTCALL(buffer_insert_list(struct buffer_head *, struct list_head *));

static inline void buffer_insert_inode_queue(struct buffer_head *bh, struct inode *inode)
{
	buffer_insert_list(bh, &inode->i_dirty_buffers);
}

static inline void buffer_insert_inode_data_queue(struct buffer_head *bh, struct inode *inode)
{
	buffer_insert_list(bh, &inode->i_dirty_data_buffers);
}

static inline int atomic_set_buffer_dirty(struct buffer_head *bh)
{
	return test_and_set_bit(BH_Dirty, &bh->b_state);
}

static inline void mark_buffer_async(struct buffer_head * bh, int on)
{
	if (on)
		set_bit(BH_Async, &bh->b_state);
	else
		clear_bit(BH_Async, &bh->b_state);
}

static inline void set_buffer_attached(struct buffer_head *bh)
{
	set_bit(BH_Attached, &bh->b_state);
}

static inline void clear_buffer_attached(struct buffer_head *bh)
{
	clear_bit(BH_Attached, &bh->b_state);
}

static inline int buffer_attached(struct buffer_head *bh)
{
	return test_bit(BH_Attached, &bh->b_state);
}

/*
 * If an error happens during the make_request, this function
 * has to be recalled. It marks the buffer as clean and not
 * uptodate, and it notifys the upper layer about the end
 * of the I/O.
 */
static inline void buffer_IO_error(struct buffer_head * bh)
{
	mark_buffer_clean(bh);
	/*
	 * b_end_io has to clear the BH_Uptodate bitflag in the error case!
	 */
	bh->b_end_io(bh, 0);
}

static inline void mark_buffer_dirty_inode(struct buffer_head *bh, struct inode *inode)
{
	mark_buffer_dirty(bh);
	buffer_insert_inode_queue(bh, inode);
}

extern void set_buffer_flushtime(struct buffer_head *);
extern inline int get_buffer_flushtime(void);
extern void balance_dirty(void);
extern int check_disk_change(kdev_t);
extern int invalidate_inodes(struct super_block *);
extern int invalidate_device(kdev_t, int);
extern void invalidate_inode_pages(struct inode *);
extern void invalidate_inode_pages2(struct address_space *);
extern void invalidate_inode_buffers(struct inode *);
#define invalidate_buffers(dev)	__invalidate_buffers((dev), 0)
#define destroy_buffers(dev)	__invalidate_buffers((dev), 1)
extern void invalidate_bdev(struct block_device *, int);
extern void __invalidate_buffers(kdev_t dev, int);
extern void sync_inodes(kdev_t);
extern void sync_unlocked_inodes(void);
extern void write_inode_now(struct inode *, int);
extern int sync_buffers(kdev_t, int);
extern void sync_dev(kdev_t);
extern int fsync_dev(kdev_t);
extern int fsync_super(struct super_block *);
extern int fsync_no_super(kdev_t);
extern void sync_inodes_sb(struct super_block *);
extern int fsync_buffers_list(struct list_head *);
static inline int fsync_inode_buffers(struct inode *inode)
{
	return fsync_buffers_list(&inode->i_dirty_buffers);
}
static inline int fsync_inode_data_buffers(struct inode *inode)
{
	return fsync_buffers_list(&inode->i_dirty_data_buffers);
}
extern int inode_has_buffers(struct inode *);
extern int do_fdatasync(struct file *);
extern int filemap_fdatawrite(struct address_space *);
extern int filemap_fdatasync(struct address_space *);
extern int filemap_fdatawait(struct address_space *);
extern void sync_supers(kdev_t dev, int wait);
extern int bmap(struct inode *, int);
extern int notify_change(struct dentry *, struct iattr *);
extern int permission(struct inode *, int);
extern int vfs_permission(struct inode *, int);
extern int get_write_access(struct inode *);
extern int deny_write_access(struct file *);
static inline void put_write_access(struct inode * inode)
{
	atomic_dec(&inode->i_writecount);
}
static inline void allow_write_access(struct file *file)
{
	if (file)
		atomic_inc(&file->f_dentry->d_inode->i_writecount);
}
extern int do_pipe(int *);

extern int open_namei(const char *, int, int, struct nameidata *);

extern int kernel_read(struct file *, unsigned long, char *, unsigned long);
extern struct file * open_exec(const char *);
 
/* fs/dcache.c -- generic fs support functions */
extern int is_subdir(struct dentry *, struct dentry *);
extern ino_t find_inode_number(struct dentry *, struct qstr *);

/*
 * Kernel pointers have redundant information, so we can use a
 * scheme where we can return either an error code or a dentry
 * pointer with the same return value.
 *
 * This should be a per-architecture thing, to allow different
 * error and pointer decisions.
 */
static inline void *ERR_PTR(long error)
{
	return (void *) error;
}

static inline long PTR_ERR(const void *ptr)
{
	return (long) ptr;
}

static inline long IS_ERR(const void *ptr)
{
	return (unsigned long)ptr > (unsigned long)-1000L;
}

/*
 * The bitmask for a lookup event:
 *  - follow links at the end
 *  - require a directory
 *  - ending slashes ok even for nonexistent files
 *  - internal "there are more path compnents" flag
 */
#define LOOKUP_FOLLOW		(1)
#define LOOKUP_DIRECTORY	(2)
#define LOOKUP_CONTINUE		(4)
#define LOOKUP_POSITIVE		(8)
#define LOOKUP_PARENT		(16)
#define LOOKUP_NOALT		(32)
/*
 * Type of the last component on LOOKUP_PARENT
 */
enum {LAST_NORM, LAST_ROOT, LAST_DOT, LAST_DOTDOT, LAST_BIND};

/*
 * "descriptor" for what we're up to with a read for sendfile().
 * This allows us to use the same read code yet
 * have multiple different users of the data that
 * we read from a file.
 *
 * The simplest case just copies the data to user
 * mode.
 */
typedef struct {
	size_t written;
	size_t count;
	char * buf;
	int error;
} read_descriptor_t;

typedef int (*read_actor_t)(read_descriptor_t *, struct page *, unsigned long, unsigned long);

/* needed for stackable file system support */
extern loff_t default_llseek(struct file *file, loff_t offset, int origin);

extern int FASTCALL(__user_walk(const char *, unsigned, struct nameidata *));
extern int FASTCALL(path_init(const char *, unsigned, struct nameidata *));
extern int FASTCALL(path_walk(const char *, struct nameidata *));
extern int FASTCALL(path_lookup(const char *, unsigned, struct nameidata *));
extern int FASTCALL(link_path_walk(const char *, struct nameidata *));
extern void path_release(struct nameidata *);
extern int follow_down(struct vfsmount **, struct dentry **);
extern int follow_up(struct vfsmount **, struct dentry **);
extern struct dentry * lookup_one_len(const char *, struct dentry *, int);
extern struct dentry * lookup_hash(struct qstr *, struct dentry *);
#define user_path_walk(name,nd)	 __user_walk(name, LOOKUP_FOLLOW|LOOKUP_POSITIVE, nd)
#define user_path_walk_link(name,nd) __user_walk(name, LOOKUP_POSITIVE, nd)

extern void inode_init_once(struct inode *);
extern void __inode_init_once(struct inode *);
extern void iput(struct inode *);
extern void refile_inode(struct inode *inode);
extern void force_delete(struct inode *);
extern struct inode * igrab(struct inode *);
extern struct inode * ilookup(struct super_block *, unsigned long);
extern ino_t iunique(struct super_block *, ino_t);
extern void unlock_new_inode(struct inode *);

typedef int (*find_inode_t)(struct inode *, unsigned long, void *);

extern struct inode * iget4_locked(struct super_block *, unsigned long,
				   find_inode_t, void *);

static inline struct inode *iget4(struct super_block *sb, unsigned long ino,
				  find_inode_t find_actor, void *opaque)
{
	struct inode *inode = iget4_locked(sb, ino, find_actor, opaque);

	if (inode && (inode->i_state & I_NEW)) {
		/*
		 * reiserfs-specific kludge that is expected to go away ASAP.
		 */
		if (sb->s_op->read_inode2)
			sb->s_op->read_inode2(inode, opaque);
		else
			sb->s_op->read_inode(inode);
		unlock_new_inode(inode);
	}

	return inode;
}

static inline struct inode *iget(struct super_block *sb, unsigned long ino)
{
	struct inode *inode = iget4_locked(sb, ino, NULL, NULL);

	if (inode && (inode->i_state & I_NEW)) {
		sb->s_op->read_inode(inode);
		unlock_new_inode(inode);
	}

	return inode;
}

static inline struct inode *iget_locked(struct super_block *sb, unsigned long ino)
{
	return iget4_locked(sb, ino, NULL, NULL);
}

extern void clear_inode(struct inode *);
extern struct inode *new_inode(struct super_block *sb);
extern void remove_suid(struct inode *inode);

extern void insert_inode_hash(struct inode *);
extern void remove_inode_hash(struct inode *);
extern struct file * get_empty_filp(void);
extern void file_move(struct file *f, struct list_head *list);
extern struct buffer_head * get_hash_table(kdev_t, int, int);
extern struct buffer_head * getblk(kdev_t, int, int);
extern void ll_rw_block(int, int, struct buffer_head * bh[]);
extern void submit_bh(int, struct buffer_head *);
extern int is_read_only(kdev_t);
extern void __brelse(struct buffer_head *);
static inline void brelse(struct buffer_head *buf)
{
	if (buf)
		__brelse(buf);
}
extern void __bforget(struct buffer_head *);
static inline void bforget(struct buffer_head *buf)
{
	if (buf)
		__bforget(buf);
}
extern int set_blocksize(kdev_t, int);
extern int sb_set_blocksize(struct super_block *, int);
extern int sb_min_blocksize(struct super_block *, int);
extern struct buffer_head * bread(kdev_t, int, int);
static inline struct buffer_head * sb_bread(struct super_block *sb, int block)
{
	return bread(sb->s_dev, block, sb->s_blocksize);
}
static inline struct buffer_head * sb_getblk(struct super_block *sb, int block)
{
	return getblk(sb->s_dev, block, sb->s_blocksize);
}
static inline struct buffer_head * sb_get_hash_table(struct super_block *sb, int block)
{
	return get_hash_table(sb->s_dev, block, sb->s_blocksize);
}
extern void wakeup_bdflush(void);
extern void wakeup_kupdate(void);
extern void put_unused_buffer_head(struct buffer_head * bh);
extern struct buffer_head * get_unused_buffer_head(int async);
extern int block_dump;

extern int brw_page(int, struct page *, kdev_t, int [], int);

typedef int (get_block_t)(struct inode*,long,struct buffer_head*,int);

/* Generic buffer handling for block filesystems.. */
extern int try_to_release_page(struct page * page, int gfp_mask);
extern int discard_bh_page(struct page *, unsigned long, int);
#define block_flushpage(page, offset) discard_bh_page(page, offset, 1)
#define block_invalidate_page(page) discard_bh_page(page, 0, 0)
extern int block_symlink(struct inode *, const char *, int);
extern int block_write_full_page(struct page*, get_block_t*);
extern int block_read_full_page(struct page*, get_block_t*);
extern int block_prepare_write(struct page*, unsigned, unsigned, get_block_t*);
extern int cont_prepare_write(struct page*, unsigned, unsigned, get_block_t*,
				unsigned long *);
extern int generic_cont_expand(struct inode *inode, loff_t size) ;
extern int block_commit_write(struct page *page, unsigned from, unsigned to);
extern int block_sync_page(struct page *);

int generic_block_bmap(struct address_space *, long, get_block_t *);
int generic_commit_write(struct file *, struct page *, unsigned, unsigned);
int block_truncate_page(struct address_space *, loff_t, get_block_t *);
extern int generic_direct_IO(int, struct inode *, struct kiobuf *, unsigned long, int, get_block_t *);
extern int waitfor_one_page(struct page *);
extern int writeout_one_page(struct page *);

extern int generic_file_mmap(struct file *, struct vm_area_struct *);
extern int file_read_actor(read_descriptor_t * desc, struct page *page, unsigned long offset, unsigned long size);
extern ssize_t generic_file_read(struct file *, char *, size_t, loff_t *);
extern inline ssize_t do_generic_direct_read(struct file *, char *, size_t, loff_t *);
extern int precheck_file_write(struct file *, struct inode *, size_t *, loff_t *);
extern ssize_t generic_file_write(struct file *, const char *, size_t, loff_t *);
extern void do_generic_file_read(struct file *, loff_t *, read_descriptor_t *, read_actor_t);
extern ssize_t do_generic_file_write(struct file *, const char *, size_t, loff_t *);
extern ssize_t do_generic_direct_write(struct file *, const char *, size_t, loff_t *);
extern loff_t no_llseek(struct file *file, loff_t offset, int origin);
extern loff_t generic_file_llseek(struct file *file, loff_t offset, int origin);
extern ssize_t generic_read_dir(struct file *, char *, size_t, loff_t *);
extern int generic_file_open(struct inode * inode, struct file * filp);

extern struct file_operations generic_ro_fops;

extern int vfs_readlink(struct dentry *, char *, int, const char *);
extern int vfs_follow_link(struct nameidata *, const char *);
extern int page_readlink(struct dentry *, char *, int);
extern int page_follow_link(struct dentry *, struct nameidata *);
extern struct inode_operations page_symlink_inode_operations;

extern int vfs_readdir(struct file *, filldir_t, void *);
extern int dcache_dir_open(struct inode *, struct file *);
extern int dcache_dir_close(struct inode *, struct file *);
extern loff_t dcache_dir_lseek(struct file *, loff_t, int);
extern int dcache_dir_fsync(struct file *, struct dentry *, int);
extern int dcache_readdir(struct file *, void *, filldir_t);
extern struct file_operations dcache_dir_ops;

extern struct file_system_type *get_fs_type(const char *name);
extern struct super_block *get_super(kdev_t);
extern void drop_super(struct super_block *sb);
static inline int is_mounted(kdev_t dev)
{
	struct super_block *sb = get_super(dev);
	if (sb) {
		drop_super(sb);
		return 1;
	}
	return 0;
}
unsigned long generate_cluster(kdev_t, int b[], int);
unsigned long generate_cluster_swab32(kdev_t, int b[], int);
extern kdev_t ROOT_DEV;
extern char root_device_name[];


extern void show_buffers(void);

#ifdef CONFIG_BLK_DEV_INITRD
extern unsigned int real_root_dev;
#endif

extern ssize_t char_read(struct file *, char *, size_t, loff_t *);
extern ssize_t block_read(struct file *, char *, size_t, loff_t *);
extern int read_ahead[];

extern ssize_t char_write(struct file *, const char *, size_t, loff_t *);
extern ssize_t block_write(struct file *, const char *, size_t, loff_t *);

extern int file_fsync(struct file *, struct dentry *, int);
extern int generic_buffer_fdatasync(struct inode *inode, unsigned long start_idx, unsigned long end_idx);
extern int generic_osync_inode(struct inode *, int);
#define OSYNC_METADATA (1<<0)
#define OSYNC_DATA (1<<1)
#define OSYNC_INODE (1<<2)

extern int inode_change_ok(struct inode *, struct iattr *);
extern int inode_setattr(struct inode *, struct iattr *);

/* kernel/fork.c */
extern int unshare_files(void);

/*
 * Common dentry functions for inclusion in the VFS
 * or in other stackable file systems.  Some of these
 * functions were in linux/fs/ C (VFS) files.
 *
 */

/*
 * Locking the parent is needed to:
 *  - serialize directory operations
 *  - make sure the parent doesn't change from
 *    under us in the middle of an operation.
 *
 * NOTE! Right now we'd rather use a "struct inode"
 * for this, but as I expect things to move toward
 * using dentries instead for most things it is
 * probably better to start with the conceptually
 * better interface of relying on a path of dentries.
 */
static inline struct dentry *lock_parent(struct dentry *dentry)
{
	struct dentry *dir = dget(dentry->d_parent);

	down(&dir->d_inode->i_sem);
	return dir;
}

static inline struct dentry *get_parent(struct dentry *dentry)
{
	return dget(dentry->d_parent);
}

static inline void unlock_dir(struct dentry *dir)
{
	up(&dir->d_inode->i_sem);
	dput(dir);
}

/*
 * Whee.. Deadlock country. Happily there are only two VFS
 * operations that does this..
 */
static inline void double_down(struct semaphore *s1, struct semaphore *s2)
{
	if (s1 != s2) {
		if ((unsigned long) s1 < (unsigned long) s2) {
			struct semaphore *tmp = s2;
			s2 = s1; s1 = tmp;
		}
		down(s1);
	}
	down(s2);
}

/*
 * Ewwwwwwww... _triple_ lock. We are guaranteed that the 3rd argument is
 * not equal to 1st and not equal to 2nd - the first case (target is parent of
 * source) would be already caught, the second is plain impossible (target is
 * its own parent and that case would be caught even earlier). Very messy.
 * I _think_ that it works, but no warranties - please, look it through.
 * Pox on bloody lusers who mandated overwriting rename() for directories...
 */

static inline void triple_down(struct semaphore *s1,
			       struct semaphore *s2,
			       struct semaphore *s3)
{
	if (s1 != s2) {
		if ((unsigned long) s1 < (unsigned long) s2) {
			if ((unsigned long) s1 < (unsigned long) s3) {
				struct semaphore *tmp = s3;
				s3 = s1; s1 = tmp;
			}
			if ((unsigned long) s1 < (unsigned long) s2) {
				struct semaphore *tmp = s2;
				s2 = s1; s1 = tmp;
			}
		} else {
			if ((unsigned long) s1 < (unsigned long) s3) {
				struct semaphore *tmp = s3;
				s3 = s1; s1 = tmp;
			}
			if ((unsigned long) s2 < (unsigned long) s3) {
				struct semaphore *tmp = s3;
				s3 = s2; s2 = tmp;
			}
		}
		down(s1);
	} else if ((unsigned long) s2 < (unsigned long) s3) {
		struct semaphore *tmp = s3;
		s3 = s2; s2 = tmp;
	}
	down(s2);
	down(s3);
}

static inline void double_up(struct semaphore *s1, struct semaphore *s2)
{
	up(s1);
	if (s1 != s2)
		up(s2);
}

static inline void triple_up(struct semaphore *s1,
			     struct semaphore *s2,
			     struct semaphore *s3)
{
	up(s1);
	if (s1 != s2)
		up(s2);
	up(s3);
}

static inline void double_lock(struct dentry *d1, struct dentry *d2)
{
	double_down(&d1->d_inode->i_sem, &d2->d_inode->i_sem);
}

static inline void double_unlock(struct dentry *d1, struct dentry *d2)
{
	double_up(&d1->d_inode->i_sem,&d2->d_inode->i_sem);
	dput(d1);
	dput(d2);
}

#endif /* __KERNEL__ */

#endif /* _LINUX_FS_H */
