#ifndef _ASM_IA64_STAT_H
#define _ASM_IA64_STAT_H

/*
 * Modified 1998, 1999
 *	David Mosberger-Tang <davidm@hpl.hp.com>, Hewlett-Packard Co
 */

struct stat {
	unsigned long	st_dev;
	unsigned long	st_ino;
	unsigned long	st_nlink;
	unsigned int	st_mode;
	unsigned int	st_uid;
	unsigned int	st_gid;
	unsigned int	__pad0;
	unsigned long	st_rdev;
	unsigned long	st_size;
	unsigned long	st_atime;
	unsigned long	__reserved0;	/* reserved for atime.nanoseconds */
	unsigned long	st_mtime;
	unsigned long	__reserved1;	/* reserved for mtime.nanoseconds */
	unsigned long	st_ctime;
	unsigned long	__reserved2;	/* reserved for ctime.nanoseconds */
	unsigned long	st_blksize;
	long		st_blocks;
	unsigned long	__unused[3];
};

struct ia64_oldstat {
	unsigned int	st_dev;
	unsigned int	st_ino;
	unsigned int	st_mode;
	unsigned int	st_nlink;
	unsigned int	st_uid;
	unsigned int	st_gid;
	unsigned int	st_rdev;
	unsigned int	__pad1;
	unsigned long	st_size;
	unsigned long	st_atime;
	unsigned long	st_mtime;
	unsigned long	st_ctime;
	unsigned int	st_blksize;
	int		st_blocks;
	unsigned int	__unused1;
	unsigned int	__unused2;
};

#endif /* _ASM_IA64_STAT_H */
