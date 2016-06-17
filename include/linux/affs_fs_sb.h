#ifndef _AFFS_FS_SB
#define _AFFS_FS_SB

/*
 * super-block data in memory
 *
 * Block numbers are adjusted for their actual size
 *
 */

struct affs_bm_info {
	u32 bm_key;			/* Disk block number */
	u32 bm_free;			/* Free blocks in here */
};

struct affs_sb_info {
	int s_partition_size;		/* Partition size in blocks. */
	int s_reserved;			/* Number of reserved blocks. */
	//u32 s_blksize;			/* Initial device blksize */
	u32 s_data_blksize;		/* size of the data block w/o header */
	u32 s_root_block;		/* FFS root block number. */
	int s_hashsize;			/* Size of hash table. */
	unsigned long s_flags;		/* See below. */
	uid_t s_uid;			/* uid to override */
	gid_t s_gid;			/* gid to override */
	umode_t s_mode;			/* mode to override */
	struct buffer_head *s_root_bh;	/* Cached root block. */
	struct semaphore s_bmlock;	/* Protects bitmap access. */
	struct affs_bm_info *s_bitmap;	/* Bitmap infos. */
	u32 s_bmap_count;		/* # of bitmap blocks. */
	u32 s_bmap_bits;		/* # of bits in one bitmap blocks */
	u32 s_last_bmap;
	struct buffer_head *s_bmap_bh;
	char *s_prefix;			/* Prefix for volumes and assigns. */
	int s_prefix_len;		/* Length of prefix. */
	char s_volume[32];		/* Volume prefix for absolute symlinks. */
};

#define SF_INTL		0x0001		/* International filesystem. */
#define SF_BM_VALID	0x0002		/* Bitmap is valid. */
#define SF_IMMUTABLE	0x0004		/* Protection bits cannot be changed */
#define SF_QUIET	0x0008		/* chmod errors will be not reported */
#define SF_SETUID	0x0010		/* Ignore Amiga uid */
#define SF_SETGID	0x0020		/* Ignore Amiga gid */
#define SF_SETMODE	0x0040		/* Ignore Amiga protection bits */
#define SF_MUFS		0x0100		/* Use MUFS uid/gid mapping */
#define SF_OFS		0x0200		/* Old filesystem */
#define SF_PREFIX	0x0400		/* Buffer for prefix is allocated */
#define SF_VERBOSE	0x0800		/* Talk about fs when mounting */
#define SF_READONLY	0x1000		/* Don't allow to remount rw */

/* short cut to get to the affs specific sb data */
#define AFFS_SB		(&sb->u.affs_sb)

#endif
