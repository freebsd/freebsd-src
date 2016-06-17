/* $Id: stat.h,v 1.7 2000/08/04 05:35:55 davem Exp $ */
#ifndef _SPARC64_STAT_H
#define _SPARC64_STAT_H

#include <linux/types.h>
#include <linux/time.h>

struct stat32 {
	__kernel_dev_t32   st_dev;
	__kernel_ino_t32   st_ino;
	__kernel_mode_t32  st_mode;
	short   	   st_nlink;
	__kernel_uid_t32   st_uid;
	__kernel_gid_t32   st_gid;
	__kernel_dev_t32   st_rdev;
	__kernel_off_t32   st_size;
	__kernel_time_t32  st_atime;
	unsigned int       __unused1;
	__kernel_time_t32  st_mtime;
	unsigned int       __unused2;
	__kernel_time_t32  st_ctime;
	unsigned int       __unused3;
	__kernel_off_t32   st_blksize;
	__kernel_off_t32   st_blocks;
	unsigned int  __unused4[2];
};

struct stat {
	dev_t   st_dev;
	ino_t   st_ino;
	mode_t  st_mode;
	short   st_nlink;
	uid_t   st_uid;
	gid_t   st_gid;
	dev_t   st_rdev;
	off_t   st_size;
	time_t  st_atime;
	time_t  st_mtime;
	time_t  st_ctime;
	off_t   st_blksize;
	off_t   st_blocks;
	unsigned long  __unused4[2];
};

#ifdef __KERNEL__
/* This is sparc32 stat64 structure. */

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

#endif
