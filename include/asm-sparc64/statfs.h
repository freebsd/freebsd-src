/* $Id: statfs.h,v 1.2 1997/04/14 17:05:22 jj Exp $ */
#ifndef _SPARC64_STATFS_H
#define _SPARC64_STATFS_H

#ifndef __KERNEL_STRICT_NAMES

#include <linux/types.h>

typedef __kernel_fsid_t	fsid_t;

#endif

struct statfs32 {
	int f_type;
	int f_bsize;
	int f_blocks;
	int f_bfree;
	int f_bavail;
	int f_files;
	int f_ffree;
	__kernel_fsid_t32 f_fsid;
	int f_namelen;  /* SunOS ignores this field. */
	int f_spare[6];
};

struct statfs {
	long f_type;
	long f_bsize;
	long f_blocks;
	long f_bfree;
	long f_bavail;
	long f_files;
	long f_ffree;
	__kernel_fsid_t f_fsid;
	long f_namelen;
	long f_spare[6];
};

#endif
