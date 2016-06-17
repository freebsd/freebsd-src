#ifndef _NFS_FS_I
#define _NFS_FS_I

#include <asm/types.h>
#include <linux/list.h>
#include <linux/nfs.h>

/*
 * nfs fs inode data in memory
 */
struct nfs_inode_info {
	/*
	 * The 64bit 'inode number'
	 */
	__u64 fileid;

	/*
	 * NFS file handle
	 */
	struct nfs_fh		fh;

	/*
	 * Various flags
	 */
	unsigned short		flags;

	/*
	 * read_cache_jiffies is when we started read-caching this inode,
	 * and read_cache_mtime is the mtime of the inode at that time.
	 * attrtimeo is for how long the cached information is assumed
	 * to be valid. A successful attribute revalidation doubles
	 * attrtimeo (up to acregmax/acdirmax), a failure resets it to
	 * acregmin/acdirmin.
	 *
	 * We need to revalidate the cached attrs for this inode if
	 *
	 *	jiffies - read_cache_jiffies > attrtimeo
	 *
	 * and invalidate any cached data/flush out any dirty pages if
	 * we find that
	 *
	 *	mtime != read_cache_mtime
	 */
	unsigned long		read_cache_jiffies;
	__u64			read_cache_ctime;
	__u64			read_cache_mtime;
	__u64			read_cache_isize;
	unsigned long		attrtimeo;
	unsigned long		attrtimeo_timestamp;

	/*
	 * Timestamp that dates the change made to read_cache_mtime.
	 * This is of use for dentry revalidation
	 */
	unsigned long		cache_mtime_jiffies;

	/*
	 * This is the cookie verifier used for NFSv3 readdir
	 * operations
	 */
	__u32			cookieverf[2];

	/*
	 * This is the list of dirty unwritten pages.
	 */
	struct list_head	read;
	struct list_head	dirty;
	struct list_head	commit;
	struct list_head	writeback;

	unsigned int		nread,
				ndirty,
				ncommit,
				npages;

	/* Credentials for shared mmap */
	struct rpc_cred		*mm_cred;
};

/*
 * Legal inode flag values
 */
#define NFS_INO_STALE		0x0001		/* possible stale inode */
#define NFS_INO_ADVISE_RDPLUS   0x0002          /* advise readdirplus */
#define NFS_INO_REVALIDATING	0x0004		/* revalidating attrs */
#define NFS_IS_SNAPSHOT		0x0010		/* a snapshot file */
#define NFS_INO_FLUSH		0x0020		/* inode is due for flushing */

/*
 * NFS lock info
 */
struct nfs_lock_info {
	u32		state;
	u32		flags;
	struct nlm_host	*host;
};

/*
 * Lock flag values
 */
#define NFS_LCK_GRANTED		0x0001		/* lock has been granted */
#define NFS_LCK_RECLAIM		0x0002		/* lock marked for reclaiming */

#endif
