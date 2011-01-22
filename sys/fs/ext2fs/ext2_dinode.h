/*-
 * Copyright (c) 2009 Aditya Sarawgi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _FS_EXT2FS_EXT2_DINODE_H_
#define _FS_EXT2FS_EXT2_DINODE_H_

#define e2di_size_high	e2di_dacl

/*
 * Special inode numbers
 * The root inode is the root of the file system.  Inode 0 can't be used for
 * normal purposes and bad blocks are normally linked to inode 1, thus
 * the root inode is 2.
 * Inode 3 to 10 are reserved in ext2fs.
 */
#define	EXT2_BADBLKINO		((ino_t)1)
#define	EXT2_ROOTINO		((ino_t)2)
#define	EXT2_ACLIDXINO		((ino_t)3)
#define	EXT2_ACLDATAINO		((ino_t)4)
#define	EXT2_BOOTLOADERINO	((ino_t)5)
#define	EXT2_UNDELDIRINO	((ino_t)6)
#define	EXT2_RESIZEINO		((ino_t)7)
#define	EXT2_JOURNALINO		((ino_t)8)
#define	EXT2_FIRSTINO		((ino_t)11)

/*
 * Inode flags
 * The current implementation uses only EXT2_IMMUTABLE and EXT2_APPEND flags
 */
#define EXT2_SECRM		0x00000001	/* Secure deletion */
#define EXT2_UNRM		0x00000002	/* Undelete */
#define EXT2_COMPR		0x00000004	/* Compress file */
#define EXT2_SYNC		0x00000008	/* Synchronous updates */
#define EXT2_IMMUTABLE		0x00000010	/* Immutable file */
#define EXT2_APPEND		0x00000020	/* writes to file may only append */
#define EXT2_NODUMP		0x00000040	/* do not dump file */
#define EXT2_NOATIME		0x00000080	/* do not update atime */


/*
 * Structure of an inode on the disk
 */
struct ext2fs_dinode {
	u_int16_t	e2di_mode;	/*   0: IFMT, permissions; see below. */
	u_int16_t	e2di_uid;	/*   2: Owner UID */
	u_int32_t	e2di_size;	/*	 4: Size (in bytes) */
	u_int32_t	e2di_atime;	/*	 8: Access time */
	u_int32_t	e2di_ctime;	/*	12: Create time */
	u_int32_t	e2di_mtime;	/*	16: Modification time */
	u_int32_t	e2di_dtime;	/*	20: Deletion time */
	u_int16_t	e2di_gid;	/*  24: Owner GID */
	u_int16_t	e2di_nlink;	/*  26: File link count */
	u_int32_t	e2di_nblock;	/*  28: Blocks count */
	u_int32_t	e2di_flags;	/*  32: Status flags (chflags) */
	u_int32_t	e2di_linux_reserved1; /* 36 */
	u_int32_t	e2di_blocks[EXT2_N_BLOCKS]; /* 40: disk blocks */
	u_int32_t	e2di_gen;	/* 100: generation number */
	u_int32_t	e2di_facl;	/* 104: file ACL (not implemented) */
	u_int32_t	e2di_dacl;	/* 108: dir ACL (not implemented) */
	u_int32_t	e2di_faddr;	/* 112: fragment address */
	u_int8_t	e2di_nfrag;	/* 116: fragment number */
	u_int8_t	e2di_fsize;	/* 117: fragment size */
	u_int16_t	e2di_linux_reserved2; /* 118 */
	u_int16_t	e2di_uid_high;	/* 120: Owner UID top 16 bits */
	u_int16_t	e2di_gid_high;	/* 122: Owner GID top 16 bits */
	u_int32_t	e2di_linux_reserved3; /* 124 */
};

#endif /* !_FS_EXT2FS_EXT2_DINODE_H_ */

