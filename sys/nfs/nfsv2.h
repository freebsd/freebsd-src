/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)nfsv2.h	8.1 (Berkeley) 6/10/93
 * $FreeBSD$
 */

#include <nfs/nfsproto.h>

#if 0


/*
 * nfs definitions as per the Version 2 and 3 specs
 */

/*
 * Constants as defined in the Sun NFS Version 2 and 3 specs.
 * "NFS: Network File System Protocol Specification" RFC1094
 * and in the "NFS: Network File System Version 3 Protocol
 * Specification"
 */

#define NFS_PORT	2049
#define	NFS_PROG	100003
#define NFS_VER2	2
#define	NFS_VER3	3
#define	NFS_MAXDGRAMDATA 8192
#define	NFS_MAXDATA	32768
#define	NFS_MAXPATHLEN	1024
#define	NFS_MAXNAMLEN	255
#define	NFS_MAXPKTHDR	404
#define NFS_MAXPACKET	(NFS_MAXPKTHDR + NFS_MAXDATA)
#define	NFS_MINPACKET	20
#define	NFS_FABLKSIZE	512	/* Size in bytes of a block wrt fa_blocks */

/* Stat numbers for rpc returns (version 2 and 3) */
#define	NFS_OK			0
#define	NFSERR_PERM		1
#define	NFSERR_NOENT		2
#define	NFSERR_IO		5
#define	NFSERR_NXIO		6
#define	NFSERR_ACCES		13
#define	NFSERR_EXIST		17
#define	NFSERR_XDEV		18	/* Version 3 only */
#define	NFSERR_NODEV		19
#define	NFSERR_NOTDIR		20
#define	NFSERR_ISDIR		21
#define	NFSERR_INVAL		22	/* Version 3 only */
#define	NFSERR_FBIG		27
#define	NFSERR_NOSPC		28
#define	NFSERR_ROFS		30
#define	NFSERR_MLINK		31	/* Version 3 only */
#define	NFSERR_NAMETOL		63
#define	NFSERR_NOTEMPTY		66
#define	NFSERR_DQUOT		69
#define	NFSERR_STALE		70
#define	NFSERR_REMOTE		71	/* Version 3 only */
#define	NFSERR_WFLUSH		99	/* Version 2 only */
#define	NFSERR_BADHANDLE	10001	/* The rest Version 3 only */
#define	NFSERR_NOT_SYNC		10002
#define	NFSERR_BAD_COOKIE	10003
#define	NFSERR_NOTSUPP		10004
#define	NFSERR_TOOSMALL		10005
#define	NFSERR_SERVERFAULT	10006
#define	NFSERR_BADTYPE		10007
#define	NFSERR_JUKEBOX		10008

/* Sizes in bytes of various nfs rpc components */
#define	NFSX_UNSIGNED	4

/* specific to NFS Version 2 */
#define	NFSX_V2FH	32
#define	NFSX_V2FATTR	68
#define	NFSX_V2SATTR	32
#define	NFSX_V2COOKIE	4
#define NFSX_V2STATFS	20

/* specific to NFS Version 3 */
#define NFSX_V3FH	16	/* size this server uses */
#define	NFSX_V3FHMAX	64	/* max. allowed by protocol */
#define NFSX_V3FATTR	84
#define NFSX_V3SATTR	60	/* max. all fields filled in */
#define NFSX_V3COOKIEVERF 8

/* variants for both versions */
#define NFSX_FH(v3)		((v3) ? (NFSX_V3FHMAX + NFSX_UNSIGNED) : \
					NFSX_V2FH)
#define NFSX_SRVFH(v3)		((v3) ? NFSX_V3FH : NFSX_V2FH)
#define	NFSX_FATTR(v3)		((v3) ? NFSX_V3FATTR : NFSX_V2FATTR)
#define NFSX_POSTOPATTR(v3)	((v3) ? (NFSX_V3FATTR + NFSX_UNSIGNED) : 0)
#define NFSX_POSTOPORFATTR(v3)	((v3) ? (NFSX_V3FATTR + NFSX_UNSIGNED) : \
					NFSX_V2FATTR)
#define NFSX_WCCDATA(v3)	((v3) ? (NFSX_V3FATTR + 32) : 0)
#define NFSX_WCCORFATTR(v3)	((v3) ? (NFSX_V3FATTR + 32) : NFSX_V2FATTR)
#define	NFSX_SATTR(v3)		((v3) ? NFSX_V3SATTR : NFSX_V2SATTR)
#define	NFSX_COOKIEVERF(v3)	((v3) ? NFSX_V3COOKIEVERF : 0)
#define	NFSX_STATFS(isv3)	((isv3) ? NFSX_NFSV3STATFS : NFSX_NFSSTATFS)

/* nfs rpc procedure numbers (before version mapping) */
#define	NFSPROC_NULL		0
#define	NFSPROC_GETATTR		1
#define	NFSPROC_SETATTR		2
#define	NFSPROC_LOOKUP		3
#define	NFSPROC_ACCESS		4
#define	NFSPROC_READLINK	5
#define	NFSPROC_READ		6
#define	NFSPROC_WRITE		7
#define	NFSPROC_CREATE		8
#define	NFSPROC_MKDIR		9
#define	NFSPROC_SYMLINK		10
#define	NFSPROC_MKNOD		11
#define	NFSPROC_REMOVE		12
#define	NFSPROC_RMDIR		13
#define	NFSPROC_RENAME		14
#define	NFSPROC_LINK		15
#define	NFSPROC_READDIR		16
#define	NFSPROC_READDIRPLUS	17
#define	NFSPROC_FSSTAT		18
#define	NFSPROC_FSINFO		19
#define	NFSPROC_PATHCONF	20
#define	NFSPROC_COMMIT		21

/* And leasing (nqnfs) procedure numbers */
#define	NQNFSPROC_GETLEASE	22
#define	NQNFSPROC_VACATED	23
#define	NQNFSPROC_EVICTED	24

#define	NFS_NPROCS		25

/* Actual Version 2 procedure numbers */
#define	NFSV2PROC_NULL		0
#define	NFSV2PROC_GETATTR	1
#define	NFSV2PROC_SETATTR	2
#define	NFSV2PROC_NOOP		3
#define	NFSV2PROC_ROOT		NFSV2PROC_NOOP	/* Obsolete */
#define	NFSV2PROC_LOOKUP	4
#define	NFSV2PROC_READLINK	5
#define	NFSV2PROC_READ		6
#define	NFSV2PROC_WRITECACHE	NFSV2PROC_NOOP	/* Obsolete */
#define	NFSV2PROC_WRITE		8
#define	NFSV2PROC_CREATE	9
#define	NFSV2PROC_REMOVE	10
#define	NFSV2PROC_RENAME	11
#define	NFSV2PROC_LINK		12
#define	NFSV2PROC_SYMLINK	13
#define	NFSV2PROC_MKDIR		14
#define	NFSV2PROC_RMDIR		15
#define	NFSV2PROC_READDIR	16
#define	NFSV2PROC_STATFS	17

/* Conversion macros */
extern int		vttoif_tab[];
#define	vtonfsv2_mode(t,m) \
		txdr_unsigned(((t) == VFIFO) ? MAKEIMODE(VCHR, (m)) : \
				MAKEIMODE((t), (m)))
#define vtonfsv3_mode(m)	txdr_unsigned((m) & 07777)
#define	nfstov_mode(a)		(fxdr_unsigned(u_short, (a))&07777)
#define	vtonfsv2_type(a)	txdr_unsigned(nfsv2_type[((long)(a))])
#define	vtonfsv3_type(a)	txdr_unsigned(nfsv3_type[((long)(a))])
#define	nfsv2tov_type(a)	nv2tov_type[fxdr_unsigned(u_long,(a))&0x7]
#define	nfsv3tov_type(a)	nv3tov_type[fxdr_unsigned(u_long,(a))&0x7]

/* File types */
typedef enum { NFNON=0, NFREG=1, NFDIR=2, NFBLK=3, NFCHR=4, NFLNK=5,
	NFSOCK=6, NFFIFO=7 } nfstype;

/* Structs for common parts of the rpc's */
struct nfsv2_time {
	u_long	nfsv2_sec;
	u_long	nfsv2_usec;
};
typedef struct nfsv2_time	nfstime2;

struct nfsv3_time {
	u_long	nfsv3_sec;
	u_long	nfsv3_nsec;
};
typedef struct nfsv3_time	nfstime3;

/*
 * Quads are defined as arrays of 2 longs to ensure dense packing for the
 * protocol and to facilitate xdr conversion.
 */
struct nfs_uquad {
	u_long	nfsuquad[2];
};
typedef	struct nfs_uquad	nfsuint64;

/*
 * File attributes and setable attributes. These structures cover both
 * NFS version 2 and the version 3 protocol. Note that the union is only
 * used so that one pointer can refer to both variants. These structures
 * go out on the wire and must be densely packed, so no quad data types
 * are used. (all fields are longs or u_longs or structures of same)
 * NB: You can't do sizeof(struct nfs_fattr), you must use the
 *     NFSX_FATTR(v3) macro.
 */
struct nfs_fattr {
	u_long	fa_type;
	u_long	fa_mode;
	u_long	fa_nlink;
	u_long	fa_uid;
	u_long	fa_gid;
	union {
		struct {
			u_long		nfsv2fa_size;
			u_long		nfsv2fa_blocksize;
			u_long		nfsv2fa_rdev;
			u_long		nfsv2fa_blocks;
			u_long		nfsv2fa_fsid;
			u_long		nfsv2fa_fileid;
			nfstime2	nfsv2fa_atime;
			nfstime2	nfsv2fa_mtime;
			nfstime2	nfsv2fa_ctime;
		} fa_nfsv2;
		struct {
			nfsuint64	nfsv3fa_size;
			nfsuint64	nfsv3fa_used;
			nfsuint64	nfsv3fa_rdev;
			nfsuint64	nfsv3fa_fsid;
			nfsuint64	nfsv3fa_fileid;
			nfstime3	nfsv3fa_atime;
			nfstime3	nfsv3fa_mtime;
			nfstime3	nfsv3fa_ctime;
		} fa_nfsv3;
	} fa_un;
};

/* and some ugly defines for accessing union components */
#define	fa2_size		fa_un.fa_nfsv2.nfsv2fa_size
#define	fa2_blocksize		fa_un.fa_nfsv2.nfsv2fa_blocksize
#define	fa2_rdev		fa_un.fa_nfsv2.nfsv2fa_rdev
#define	fa2_blocks		fa_un.fa_nfsv2.nfsv2fa_blocks
#define	fa2_fsid		fa_un.fa_nfsv2.nfsv2fa_fsid
#define	fa2_fileid		fa_un.fa_nfsv2.nfsv2fa_fileid
#define	fa2_atime		fa_un.fa_nfsv2.nfsv2fa_atime
#define	fa2_mtime		fa_un.fa_nfsv2.nfsv2fa_mtime
#define	fa2_ctime		fa_un.fa_nfsv2.nfsv2fa_ctime
#define	fa3_size		fa_un.fa_nfsv3.nfsv3fa_size
#define	fa3_used		fa_un.fa_nfsv3.nfsv3fa_used
#define	fa3_rdev		fa_un.fa_nfsv3.nfsv3fa_rdev
#define	fa3_fsid		fa_un.fa_nfsv3.nfsv3fa_fsid
#define	fa3_fileid		fa_un.fa_nfsv3.nfsv3fa_fileid
#define	fa3_atime		fa_un.fa_nfsv3.nfsv3fa_atime
#define	fa3_mtime		fa_un.fa_nfsv3.nfsv3fa_mtime
#define	fa3_ctime		fa_un.fa_nfsv3.nfsv3fa_ctime

struct nfsv2_sattr {
	u_long		sa_mode;
	u_long		sa_uid;
	u_long		sa_gid;
	u_long		sa_size;
	nfstime2	sa_atime;
	nfstime2	sa_mtime;
};

struct nfsv2_statfs {
	u_long	sf_tsize;
	u_long	sf_bsize;
	u_long	sf_blocks;
	u_long	sf_bfree;
	u_long	sf_bavail;
};

struct nfsv3_fsstat {
	nfsuint64	sf_tbytes;
	nfsuint64	sf_fbytes;
	nfsuint64	sf_abytes;
	nfsuint64	sf_tfiles;
	nfsuint64	sf_ffiles;
	nfsuint64	sf_afiles;
	u_long		sf_invarsec;
};

struct nfsv3_fsinfo {
	u_long		fs_rtmax;
	u_long		fs_rtpref;
	u_long		fs_rtmult;
	u_long		fs_wtmax;
	u_long		fs_wtpref;
	u_long		fs_wtmult;
	u_long		fs_dtpref;
	nfsuint64	fs_maxfilesize;
	nfstime3	fs_time_delta;
	u_long		fs_properties;
};
#endif
