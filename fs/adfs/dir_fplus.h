/*
 *  linux/fs/adfs/dir_fplus.h
 *
 *  Copyright (C) 1999 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Structures of directories on the F+ format disk
 */

#define ADFS_FPLUS_NAME_LEN	255

#define BIGDIRSTARTNAME ('S' | 'B' << 8 | 'P' << 16 | 'r' << 24)
#define BIGDIRENDNAME	('o' | 'v' << 8 | 'e' << 16 | 'n' << 24)

struct adfs_bigdirheader {
	__u8	startmasseq;
	__u8	bigdirversion[3];
	__u32	bigdirstartname;
	__u32	bigdirnamelen;
	__u32	bigdirsize;
	__u32	bigdirentries;
	__u32	bigdirnamesize;
	__u32	bigdirparent;
	char	bigdirname[1];
};

struct adfs_bigdirentry {
	__u32	bigdirload;
	__u32	bigdirexec;
	__u32	bigdirlen;
	__u32	bigdirindaddr;
	__u32	bigdirattr;
	__u32	bigdirobnamelen;
	__u32	bigdirobnameptr;
};

struct adfs_bigdirtail {
	__u32	bigdirendname;
	__u8	bigdirendmasseq;
	__u8	reserved[2];
	__u8	bigdircheckbyte;
};
