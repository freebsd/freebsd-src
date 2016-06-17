/*
 * efs_fs_sb.h
 *
 * Copyright (c) 1999 Al Smith
 *
 * Portions derived from IRIX header files (c) 1988 Silicon Graphics
 */

#ifndef __EFS_FS_SB_H__
#define __EFS_FS_SB_H__

/* statfs() magic number for EFS */
#define EFS_SUPER_MAGIC	0x414A53

/* EFS superblock magic numbers */
#define EFS_MAGIC	0x072959
#define EFS_NEWMAGIC	0x07295a

#define IS_EFS_MAGIC(x)	((x == EFS_MAGIC) || (x == EFS_NEWMAGIC))

#define EFS_SUPER		1
#define EFS_ROOTINODE		2

/* efs superblock on disk */
struct efs_super {
	int32_t		fs_size;        /* size of filesystem, in sectors */
	int32_t		fs_firstcg;     /* bb offset to first cg */
	int32_t		fs_cgfsize;     /* size of cylinder group in bb's */
	short		fs_cgisize;     /* bb's of inodes per cylinder group */
	short		fs_sectors;     /* sectors per track */
	short		fs_heads;       /* heads per cylinder */
	short		fs_ncg;         /* # of cylinder groups in filesystem */
	short		fs_dirty;       /* fs needs to be fsck'd */
	int32_t		fs_time;        /* last super-block update */
	int32_t		fs_magic;       /* magic number */
	char		fs_fname[6];    /* file system name */
	char		fs_fpack[6];    /* file system pack name */
	int32_t		fs_bmsize;      /* size of bitmap in bytes */
	int32_t		fs_tfree;       /* total free data blocks */
	int32_t		fs_tinode;      /* total free inodes */
	int32_t		fs_bmblock;     /* bitmap location. */
	int32_t		fs_replsb;      /* Location of replicated superblock. */
	int32_t		fs_lastialloc;  /* last allocated inode */
	char		fs_spare[20];   /* space for expansion - MUST BE ZERO */
	int32_t		fs_checksum;    /* checksum of volume portion of fs */
};

/* efs superblock information in memory */
struct efs_sb_info {
	int32_t	fs_magic;	/* superblock magic number */
	int32_t	fs_start;	/* first block of filesystem */
	int32_t	first_block;	/* first data block in filesystem */
	int32_t	total_blocks;	/* total number of blocks in filesystem */
	int32_t	group_size;	/* # of blocks a group consists of */ 
	int32_t	data_free;	/* # of free data blocks */
	int32_t	inode_free;	/* # of free inodes */
	short	inode_blocks;	/* # of blocks used for inodes in every grp */
	short	total_groups;	/* # of groups */
};

#endif /* __EFS_FS_SB_H__ */

