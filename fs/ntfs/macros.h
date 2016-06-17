/*
 * macros.h
 *
 * Copyright (C) 1995 Martin von Löwis
 * Copyright (C) 1996 Régis Duchesne
 * Copyright (c) 2001 Anton Altaparmakov
 */
#include <linux/ntfs_fs_i.h>
#include <linux/fs.h>
#include <asm/page.h>

#define NTFS_FD(vol)		((vol)->u.fd)

#define NTFS_SB(vol)		((struct super_block*)(vol)->sb)
#define NTFS_SB2VOL(sb)         (&(sb)->u.ntfs_sb)
#define NTFS_INO2VOL(ino)	(&((ino)->i_sb->u.ntfs_sb))
#define NTFS_LINO2NINO(ino)     ((struct ntfs_inode_info*)(&((ino)->u.ntfs_i)))
static inline struct inode *VFS_I(struct ntfs_inode_info *ntfs_ino)
{
	struct inode *i = (struct inode*)((char*)ntfs_ino -
			((char*)&(((struct inode*)NULL)->u.ntfs_i) -
			(char*)NULL));
#ifdef DEBUG
	if ((char*)NTFS_LINO2NINO(i) != (char*)ntfs_ino)
		BUG();
#endif
	return i;
}

#define IS_MAGIC(a,b)		(*(int*)(a) == *(int*)(b))
#define IS_MFT_RECORD(a)	IS_MAGIC((a),"FILE")
#define IS_INDEX_RECORD(a)	IS_MAGIC((a),"INDX")

/* 'NTFS' in little endian */
#define NTFS_SUPER_MAGIC	0x5346544E

#define NTFS_AFLAG_RO           1
#define NTFS_AFLAG_HIDDEN       2
#define NTFS_AFLAG_SYSTEM       4
#define NTFS_AFLAG_ARCHIVE      20
#define NTFS_AFLAG_COMPRESSED   0x800
#define NTFS_AFLAG_DIR          0x10000000

