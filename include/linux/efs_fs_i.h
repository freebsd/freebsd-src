/*
 * efs_fs_i.h
 *
 * Copyright (c) 1999 Al Smith
 *
 * Portions derived from IRIX header files (c) 1988 Silicon Graphics
 */

#ifndef	__EFS_FS_I_H__
#define	__EFS_FS_I_H__

typedef	int32_t		efs_block_t;
typedef uint32_t	efs_ino_t;

#define	EFS_DIRECTEXTENTS	12

/*
 * layout of an extent, in memory and on disk. 8 bytes exactly.
 */
typedef union extent_u {
	unsigned char raw[8];
	struct extent_s {
		unsigned int	ex_magic:8;	/* magic # (zero) */
		unsigned int	ex_bn:24;	/* basic block */
		unsigned int	ex_length:8;	/* numblocks in this extent */
		unsigned int	ex_offset:24;	/* logical offset into file */
	} cooked;
} efs_extent;

typedef struct edevs {
	short		odev;
	unsigned int	ndev;
} efs_devs;

/*
 * extent based filesystem inode as it appears on disk.  The efs inode
 * is exactly 128 bytes long.
 */
struct	efs_dinode {
	u_short		di_mode;	/* mode and type of file */
	short		di_nlink;	/* number of links to file */
	u_short		di_uid;		/* owner's user id */
	u_short		di_gid;		/* owner's group id */
	int32_t		di_size;	/* number of bytes in file */
	int32_t		di_atime;	/* time last accessed */
	int32_t		di_mtime;	/* time last modified */
	int32_t		di_ctime;	/* time created */
	uint32_t	di_gen;		/* generation number */
	short		di_numextents;	/* # of extents */
	u_char		di_version;	/* version of inode */
	u_char		di_spare;	/* spare - used by AFS */
	union di_addr {
		efs_extent	di_extents[EFS_DIRECTEXTENTS];
		efs_devs	di_dev;	/* device for IFCHR/IFBLK */
	} di_u;
};

/* efs inode storage in memory */
struct efs_inode_info {
	int		numextents;
	int		lastextent;

	efs_extent	extents[EFS_DIRECTEXTENTS];
};

#endif	/* __EFS_FS_I_H__ */

