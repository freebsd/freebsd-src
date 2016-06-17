#ifndef _ASM_X86_64_STAT_H
#define _ASM_X86_64_STAT_H

struct stat {
	unsigned long	st_dev;
	unsigned long	st_ino;
	unsigned long	st_nlink;

	unsigned int	st_mode;
	unsigned int	st_uid;
	unsigned int	st_gid;
	unsigned int	__pad0;
	unsigned long	st_rdev;
	long		st_size;
	long		st_blksize;
	long		st_blocks;	/* Number 512-byte blocks allocated. */

	unsigned long	st_atime;
	unsigned long	__reserved0;	/* reserved for atime.nanoseconds */
	unsigned long	st_mtime;
	unsigned long	__reserved1;	/* reserved for atime.nanoseconds */
	unsigned long	st_ctime;
	unsigned long	__reserved2;	/* reserved for atime.nanoseconds */
  	long		__unused[3];
};

#endif
