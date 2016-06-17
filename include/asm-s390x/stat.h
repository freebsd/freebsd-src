/*
 *  include/asm-s390x/stat.h
 *
 *  S390 version
 *
 *  Derived from "include/asm-i386/stat.h"
 */

#ifndef _S390_STAT_H
#define _S390_STAT_H

struct stat {
        unsigned long  st_dev;
        unsigned long  st_ino;
        unsigned long  st_nlink;
        unsigned int   st_mode;
        unsigned int   st_uid;
        unsigned int   st_gid;
        unsigned int   __pad1;
        unsigned long  st_rdev;
        unsigned long  st_size;
        unsigned long  st_atime;
	unsigned long	__reserved0;	/* reserved for atime.nanoseconds */
        unsigned long  st_mtime;
	unsigned long	__reserved1;	/* reserved for mtime.nanoseconds */
        unsigned long  st_ctime;
	unsigned long	__reserved2;	/* reserved for ctime.nanoseconds */
        unsigned long  st_blksize;
        long           st_blocks;
        unsigned long  __unused[3];
};

#endif
