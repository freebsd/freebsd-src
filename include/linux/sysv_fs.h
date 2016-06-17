#ifndef _LINUX_SYSV_FS_H
#define _LINUX_SYSV_FS_H

/*
 * The SystemV/Coherent filesystem constants/structures/macros
 */


/* This code assumes
   - sizeof(short) = 2, sizeof(int) = 4, sizeof(long) = 4,
   - alignof(short) = 2, alignof(long) = 4.
*/

#ifdef __GNUC__
#define __packed2__  __attribute__ ((packed, aligned(2)))
#else
#error I want gcc!
#endif

#include <linux/stat.h>		/* declares S_IFLNK etc. */
#include <linux/sched.h>	/* declares wake_up() */
#include <linux/sysv_fs_sb.h>	/* defines the sv_... shortcuts */


/* Layout on disk */
/* ============== */

static inline u32 PDP_swab(u32 x)
{
#ifdef __LITTLE_ENDIAN
	return ((x & 0xffff) << 16) | ((x & 0xffff0000) >> 16);
#else
#ifdef __BIG_ENDIAN
	return ((x & 0xff00ff) << 8) | ((x & 0xff00ff00) >> 8);
#else
#error BYTESEX
#endif
#endif
}

/* inode numbers are 16 bit */

typedef u16 sysv_ino_t;

/* Block numbers are 24 bit, sometimes stored in 32 bit.
   On Coherent FS, they are always stored in PDP-11 manner: the least
   significant 16 bits come last.
*/

typedef u32 sysv_zone_t;

/* Among the blocks ... */
/* Xenix FS, Coherent FS: block 0 is the boot block, block 1 the super-block.
   SystemV FS: block 0 contains both the boot sector and the super-block. */
/* The first inode zone is sb->sv_firstinodezone (1 or 2). */

/* Among the inodes ... */
/* 0 is non-existent */
#define SYSV_BADBL_INO	1	/* inode of bad blocks file */
#define SYSV_ROOT_INO	2	/* inode of root directory */


/* Xenix super-block data on disk */
#define XENIX_NICINOD	100	/* number of inode cache entries */
#define XENIX_NICFREE	100	/* number of free block list chunk entries */
struct xenix_super_block {
	u16		s_isize; /* index of first data zone */
	u32		s_fsize __packed2__; /* total number of zones of this fs */
	/* the start of the free block list: */
	u16		s_nfree;	/* number of free blocks in s_free, <= XENIX_NICFREE */
	u32		s_free[XENIX_NICFREE]; /* first free block list chunk */
	/* the cache of free inodes: */
	u16		s_ninode; /* number of free inodes in s_inode, <= XENIX_NICINOD */
	sysv_ino_t	s_inode[XENIX_NICINOD]; /* some free inodes */
	/* locks, not used by Linux: */
	char		s_flock;	/* lock during free block list manipulation */
	char		s_ilock;	/* lock during inode cache manipulation */
	char		s_fmod;		/* super-block modified flag */
	char		s_ronly;	/* flag whether fs is mounted read-only */
	u32		s_time __packed2__; /* time of last super block update */
	u32		s_tfree __packed2__; /* total number of free zones */
	u16		s_tinode;	/* total number of free inodes */
	s16		s_dinfo[4];	/* device information ?? */
	char		s_fname[6];	/* file system volume name */
	char		s_fpack[6];	/* file system pack name */
	char		s_clean;	/* set to 0x46 when filesystem is properly unmounted */
	char		s_fill[371];
	s32		s_magic;	/* version of file system */
	s32		s_type;		/* type of file system: 1 for 512 byte blocks
								2 for 1024 byte blocks
								3 for 2048 byte blocks */
								
};

/* SystemV FS comes in two variants:
 * sysv2: System V Release 2 (e.g. Microport), structure elements aligned(2).
 * sysv4: System V Release 4 (e.g. Consensys), structure elements aligned(4).
 */
#define SYSV_NICINOD	100	/* number of inode cache entries */
#define SYSV_NICFREE	50	/* number of free block list chunk entries */

/* SystemV4 super-block data on disk */
struct sysv4_super_block {
	u16	s_isize;	/* index of first data zone */
	u16	s_pad0;
	u32	s_fsize;	/* total number of zones of this fs */
	/* the start of the free block list: */
	u16	s_nfree;	/* number of free blocks in s_free, <= SYSV_NICFREE */
	u16	s_pad1;
	u32	s_free[SYSV_NICFREE]; /* first free block list chunk */
	/* the cache of free inodes: */
	u16	s_ninode;	/* number of free inodes in s_inode, <= SYSV_NICINOD */
	u16	s_pad2;
	sysv_ino_t     s_inode[SYSV_NICINOD]; /* some free inodes */
	/* locks, not used by Linux: */
	char	s_flock;	/* lock during free block list manipulation */
	char	s_ilock;	/* lock during inode cache manipulation */
	char	s_fmod;		/* super-block modified flag */
	char	s_ronly;	/* flag whether fs is mounted read-only */
	u32	s_time;		/* time of last super block update */
	s16	s_dinfo[4];	/* device information ?? */
	u32	s_tfree;	/* total number of free zones */
	u16	s_tinode;	/* total number of free inodes */
	u16	s_pad3;
	char	s_fname[6];	/* file system volume name */
	char	s_fpack[6];	/* file system pack name */
	s32	s_fill[12];
	s32	s_state;	/* file system state: 0x7c269d38-s_time means clean */
	s32	s_magic;	/* version of file system */
	s32	s_type;		/* type of file system: 1 for 512 byte blocks
								2 for 1024 byte blocks */
};

/* SystemV2 super-block data on disk */
struct sysv2_super_block {
	u16	s_isize; 		/* index of first data zone */
	u32	s_fsize __packed2__;	/* total number of zones of this fs */
	/* the start of the free block list: */
	u16	s_nfree;		/* number of free blocks in s_free, <= SYSV_NICFREE */
	u32	s_free[SYSV_NICFREE];	/* first free block list chunk */
	/* the cache of free inodes: */
	u16	s_ninode;		/* number of free inodes in s_inode, <= SYSV_NICINOD */
	sysv_ino_t     s_inode[SYSV_NICINOD]; /* some free inodes */
	/* locks, not used by Linux: */
	char	s_flock;		/* lock during free block list manipulation */
	char	s_ilock;		/* lock during inode cache manipulation */
	char	s_fmod;			/* super-block modified flag */
	char	s_ronly;		/* flag whether fs is mounted read-only */
	u32	s_time __packed2__;	/* time of last super block update */
	s16	s_dinfo[4];		/* device information ?? */
	u32	s_tfree __packed2__;	/* total number of free zones */
	u16	s_tinode;		/* total number of free inodes */
	char	s_fname[6];		/* file system volume name */
	char	s_fpack[6];		/* file system pack name */
	s32	s_fill[14];
	s32	s_state;		/* file system state: 0xcb096f43 means clean */
	s32	s_magic;		/* version of file system */
	s32	s_type;			/* type of file system: 1 for 512 byte blocks
								2 for 1024 byte blocks */
};

/* V7 super-block data on disk */
#define V7_NICINOD     100     /* number of inode cache entries */
#define V7_NICFREE     50      /* number of free block list chunk entries */
struct v7_super_block {
	u16    s_isize;        /* index of first data zone */
	u32    s_fsize __packed2__; /* total number of zones of this fs */
	/* the start of the free block list: */
	u16    s_nfree;        /* number of free blocks in s_free, <= V7_NICFREE */
	u32    s_free[V7_NICFREE]; /* first free block list chunk */
	/* the cache of free inodes: */
	u16    s_ninode;       /* number of free inodes in s_inode, <= V7_NICINOD */
	sysv_ino_t      s_inode[V7_NICINOD]; /* some free inodes */
	/* locks, not used by Linux or V7: */
	char    s_flock;        /* lock during free block list manipulation */
	char    s_ilock;        /* lock during inode cache manipulation */
	char    s_fmod;         /* super-block modified flag */
	char    s_ronly;        /* flag whether fs is mounted read-only */
	u32     s_time __packed2__; /* time of last super block update */
	/* the following fields are not maintained by V7: */
	u32     s_tfree __packed2__; /* total number of free zones */
	u16     s_tinode;       /* total number of free inodes */
	u16     s_m;            /* interleave factor */
	u16     s_n;            /* interleave factor */
	char    s_fname[6];     /* file system name */
	char    s_fpack[6];     /* file system pack name */
};

/* Coherent super-block data on disk */
#define COH_NICINOD	100	/* number of inode cache entries */
#define COH_NICFREE	64	/* number of free block list chunk entries */
struct coh_super_block {
	u16		s_isize;	/* index of first data zone */
	u32		s_fsize __packed2__; /* total number of zones of this fs */
	/* the start of the free block list: */
	u16 s_nfree;	/* number of free blocks in s_free, <= COH_NICFREE */
	u32		s_free[COH_NICFREE] __packed2__; /* first free block list chunk */
	/* the cache of free inodes: */
	u16		s_ninode;	/* number of free inodes in s_inode, <= COH_NICINOD */
	sysv_ino_t	s_inode[COH_NICINOD]; /* some free inodes */
	/* locks, not used by Linux: */
	char		s_flock;	/* lock during free block list manipulation */
	char		s_ilock;	/* lock during inode cache manipulation */
	char		s_fmod;		/* super-block modified flag */
	char		s_ronly;	/* flag whether fs is mounted read-only */
	u32		s_time __packed2__; /* time of last super block update */
	u32		s_tfree __packed2__; /* total number of free zones */
	u16		s_tinode;	/* total number of free inodes */
	u16		s_interleave_m;	/* interleave factor */
	u16		s_interleave_n;
	char		s_fname[6];	/* file system volume name */
	char		s_fpack[6];	/* file system pack name */
	u32		s_unique;	/* zero, not used */
};

/* SystemV/Coherent inode data on disk */

struct sysv_inode {
	u16 i_mode;
	u16 i_nlink;
	u16 i_uid;
	u16 i_gid;
	u32 i_size;
	union { /* directories, regular files, ... */
		unsigned char i_addb[3*(10+1+1+1)+1]; /* zone numbers: max. 10 data blocks,
					      * then 1 indirection block,
					      * then 1 double indirection block,
					      * then 1 triple indirection block.
					      * Then maybe a "file generation number" ??
					      */
		/* named pipes on Coherent */
		struct {
			char p_addp[30];
			s16 p_pnc;
			s16 p_prx;
			s16 p_pwx;
		} i_p;
	} i_a;
	u32 i_atime;	/* time of last access */
	u32 i_mtime;	/* time of last modification */
	u32 i_ctime;	/* time of creation */
};

/* Admissible values for i_nlink: 0.._LINK_MAX */
enum {
	XENIX_LINK_MAX	=	126,	/* ?? */
	SYSV_LINK_MAX	=	126,	/* 127? 251? */
	V7_LINK_MAX     =	126,	/* ?? */
	COH_LINK_MAX	=	10000,
};

/* The number of inodes per block is
   sb->sv_inodes_per_block = block_size / sizeof(struct sysv_inode) */
/* The number of indirect pointers per block is
   sb->sv_ind_per_block = block_size / sizeof(u32) */


/* SystemV/Coherent directory entry on disk */

#define SYSV_NAMELEN	14	/* max size of name in struct sysv_dir_entry */

struct sysv_dir_entry {
	sysv_ino_t inode;
	char name[SYSV_NAMELEN]; /* up to 14 characters, the rest are zeroes */
};

#define SYSV_DIRSIZE	sizeof(struct sysv_dir_entry)	/* size of every directory entry */


/* Operations */
/* ========== */

/* identify the FS in memory */
enum {
	FSTYPE_NONE = 0,
	FSTYPE_XENIX,
	FSTYPE_SYSV4,
	FSTYPE_SYSV2,
	FSTYPE_COH,
	FSTYPE_V7,
	FSTYPE_AFS,
	FSTYPE_END,
};

#define SYSV_MAGIC_BASE		0x012FF7B3

#define XENIX_SUPER_MAGIC	(SYSV_MAGIC_BASE+FSTYPE_XENIX)
#define SYSV4_SUPER_MAGIC	(SYSV_MAGIC_BASE+FSTYPE_SYSV4)
#define SYSV2_SUPER_MAGIC	(SYSV_MAGIC_BASE+FSTYPE_SYSV2)
#define COH_SUPER_MAGIC		(SYSV_MAGIC_BASE+FSTYPE_COH)

#ifdef __KERNEL__

enum {
	BYTESEX_LE,
	BYTESEX_PDP,
	BYTESEX_BE,
};

/*
 * Function prototypes
 */

extern struct inode * sysv_new_inode(const struct inode *, mode_t);
extern void sysv_free_inode(struct inode *);
extern unsigned long sysv_count_free_inodes(struct super_block *);
extern u32 sysv_new_block(struct super_block *);
extern void sysv_free_block(struct super_block *, u32);
extern unsigned long sysv_count_free_blocks(struct super_block *);

extern void sysv_truncate(struct inode *);

extern void sysv_write_inode(struct inode *, int);
extern int sysv_sync_inode(struct inode *);
extern int sysv_sync_file(struct file *, struct dentry *, int);
extern void sysv_set_inode(struct inode *, dev_t);

extern struct sysv_dir_entry *sysv_find_entry(struct dentry*, struct page**);
extern int sysv_add_link(struct dentry*, struct inode*);
extern int sysv_delete_entry(struct sysv_dir_entry*, struct page*);
extern int sysv_make_empty(struct inode*, struct inode*);
extern int sysv_empty_dir(struct inode*);
extern void sysv_set_link(struct sysv_dir_entry*, struct page*, struct inode*);
extern struct sysv_dir_entry *sysv_dotdot(struct inode*, struct page**);
extern ino_t sysv_inode_by_name(struct dentry*);

extern struct inode_operations sysv_file_inode_operations;
extern struct inode_operations sysv_dir_inode_operations;
extern struct inode_operations sysv_fast_symlink_inode_operations;
extern struct file_operations sysv_file_operations;
extern struct file_operations sysv_dir_operations;
extern struct address_space_operations sysv_aops;
extern struct super_operations sysv_sops;
extern struct dentry_operations sysv_dentry_operations;

extern struct sysv_inode *sysv_raw_inode(struct super_block *, unsigned, struct buffer_head **);

static inline void dirty_sb(struct super_block *sb)
{
	mark_buffer_dirty(sb->sv_bh1);
	if (sb->sv_bh1 != sb->sv_bh2)
		mark_buffer_dirty(sb->sv_bh2);
	sb->s_dirt = 1;
}

static inline u32 fs32_to_cpu(struct super_block *sb, u32 n)
{
	if (sb->sv_bytesex == BYTESEX_PDP)
		return PDP_swab(n);
	else if (sb->sv_bytesex == BYTESEX_LE)
		return le32_to_cpu(n);
	else
		return be32_to_cpu(n);
}

static inline u32 cpu_to_fs32(struct super_block *sb, u32 n)
{
	if (sb->sv_bytesex == BYTESEX_PDP)
		return PDP_swab(n);
	else if (sb->sv_bytesex == BYTESEX_LE)
		return cpu_to_le32(n);
	else
		return cpu_to_be32(n);
}

static inline u32 fs32_add(struct super_block *sb, u32 *n, int d)
{
	if (sb->sv_bytesex == BYTESEX_PDP)
		return *n = PDP_swab(PDP_swab(*n)+d);
	else if (sb->sv_bytesex == BYTESEX_LE)
		return *n = cpu_to_le32(le32_to_cpu(*n)+d);
	else
		return *n = cpu_to_be32(be32_to_cpu(*n)+d);
}

static inline u16 fs16_to_cpu(struct super_block *sb, u16 n)
{
	if (sb->sv_bytesex != BYTESEX_BE)
		return le16_to_cpu(n);
	else
		return be16_to_cpu(n);
}

static inline u16 cpu_to_fs16(struct super_block *sb, u16 n)
{
	if (sb->sv_bytesex != BYTESEX_BE)
		return cpu_to_le16(n);
	else
		return cpu_to_be16(n);
}

static inline u16 fs16_add(struct super_block *sb, u16 *n, int d)
{
	if (sb->sv_bytesex != BYTESEX_BE)
		return *n = cpu_to_le16(le16_to_cpu(*n)+d);
	else
		return *n = cpu_to_be16(be16_to_cpu(*n)+d);
}

#endif /* __KERNEL__ */

#endif
