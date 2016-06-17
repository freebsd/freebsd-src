/* $Id: stat.h,v 1.12 2000/08/04 05:35:55 davem Exp $ */
#ifndef _SPARC_STAT_H
#define _SPARC_STAT_H

#include <linux/types.h>

struct __old_kernel_stat {
	unsigned short st_dev;
	unsigned short st_ino;
	unsigned short st_mode;
	unsigned short st_nlink;
	unsigned short st_uid;
	unsigned short st_gid;
	unsigned short st_rdev;
	unsigned long  st_size;
	unsigned long  st_atime;
	unsigned long  st_mtime;
	unsigned long  st_ctime;
};

struct stat {
	unsigned short	st_dev;
	unsigned long	st_ino;
	unsigned short	st_mode;
	short		st_nlink;
	unsigned short	st_uid;
	unsigned short	st_gid;
	unsigned short	st_rdev;
	long		st_size;
	long		st_atime;
	unsigned long	__unused1;
	long		st_mtime;
	unsigned long	__unused2;
	long		st_ctime;
	unsigned long	__unused3;
	long		st_blksize;
	long		st_blocks;
	unsigned long	__unused4[2];
};

struct stat64 {
	unsigned char	__pad0[6];
	unsigned short	st_dev;

	unsigned long long	st_ino;

	unsigned int	st_mode;
	unsigned int	st_nlink;

	unsigned int	st_uid;
	unsigned int	st_gid;

	unsigned char	__pad2[6];
	unsigned short	st_rdev;

	unsigned char	__pad3[8];

	long long	st_size;
	unsigned int	st_blksize;

	unsigned char	__pad4[8];
	unsigned int	st_blocks;

	unsigned int	st_atime;
	unsigned int	__unused1;

	unsigned int	st_mtime;
	unsigned int	__unused2;

	unsigned int	st_ctime;
	unsigned int	__unused3;

	unsigned int	__unused4;
	unsigned int	__unused5;
};

#endif
