/*
 * NFS protocol definitions
 *
 * This file contains constants mostly for Version 2 of the protocol,
 * but also has a couple of NFSv3 bits in (notably the error codes).
 */
#ifndef _LINUX_NFS_H
#define _LINUX_NFS_H

#include <linux/sunrpc/msg_prot.h>

#define NFS_PROGRAM	100003
#define NFS_PORT	2049
#define NFS_MAXDATA	8192
#define NFS_MAXPATHLEN	1024
#define NFS_MAXNAMLEN	255
#define NFS_MAXGROUPS	16
#define NFS_FHSIZE	32
#define NFS_COOKIESIZE	4
#define NFS_FIFO_DEV	(-1)
#define NFSMODE_FMT	0170000
#define NFSMODE_DIR	0040000
#define NFSMODE_CHR	0020000
#define NFSMODE_BLK	0060000
#define NFSMODE_REG	0100000
#define NFSMODE_LNK	0120000
#define NFSMODE_SOCK	0140000
#define NFSMODE_FIFO	0010000

#define NFS_MNT_PROGRAM	100005
#define NFS_MNT_PORT	627

#define NFS_MAJOR   	UNNAMED_MAJOR
#define NFS_MINOR   	0xff

/*
 * NFS stats. The good thing with these values is that NFSv3 errors are
 * a superset of NFSv2 errors (with the exception of NFSERR_WFLUSH which
 * no-one uses anyway), so we can happily mix code as long as we make sure
 * no NFSv3 errors are returned to NFSv2 clients.
 * Error codes that have a `--' in the v2 column are not part of the
 * standard, but seem to be widely used nevertheless.
 */
 enum nfs_stat {
	NFS_OK = 0,			/* v2 v3 */
	NFSERR_PERM = 1,		/* v2 v3 */
	NFSERR_NOENT = 2,		/* v2 v3 */
	NFSERR_IO = 5,			/* v2 v3 */
	NFSERR_NXIO = 6,		/* v2 v3 */
	NFSERR_EAGAIN = 11,		/* v2 v3 */
	NFSERR_ACCES = 13,		/* v2 v3 */
	NFSERR_EXIST = 17,		/* v2 v3 */
	NFSERR_XDEV = 18,		/*    v3 */
	NFSERR_NODEV = 19,		/* v2 v3 */
	NFSERR_NOTDIR = 20,		/* v2 v3 */
	NFSERR_ISDIR = 21,		/* v2 v3 */
	NFSERR_INVAL = 22,		/* v2 v3 that Sun forgot */
	NFSERR_FBIG = 27,		/* v2 v3 */
	NFSERR_NOSPC = 28,		/* v2 v3 */
	NFSERR_ROFS = 30,		/* v2 v3 */
	NFSERR_MLINK = 31,		/*    v3 */
	NFSERR_OPNOTSUPP = 45,		/* v2 v3 */
	NFSERR_NAMETOOLONG = 63,	/* v2 v3 */
	NFSERR_NOTEMPTY = 66,		/* v2 v3 */
	NFSERR_DQUOT = 69,		/* v2 v3 */
	NFSERR_STALE = 70,		/* v2 v3 */
	NFSERR_REMOTE = 71,		/* v2 v3 */
	NFSERR_WFLUSH = 99,		/* v2    */
	NFSERR_BADHANDLE = 10001,	/*    v3 */
	NFSERR_NOT_SYNC = 10002,	/*    v3 */
	NFSERR_BAD_COOKIE = 10003,	/*    v3 */
	NFSERR_NOTSUPP = 10004,		/*    v3 */
	NFSERR_TOOSMALL = 10005,	/*    v3 */
	NFSERR_SERVERFAULT = 10006,	/*    v3 */
	NFSERR_BADTYPE = 10007,		/*    v3 */
	NFSERR_JUKEBOX = 10008		/*    v3 */
 };
 
/* NFSv2 file types - beware, these are not the same in NFSv3 */

enum nfs_ftype {
	NFNON = 0,
	NFREG = 1,
	NFDIR = 2,
	NFBLK = 3,
	NFCHR = 4,
	NFLNK = 5,
	NFSOCK = 6,
	NFBAD = 7,
	NFFIFO = 8
};

#if defined(__KERNEL__)
/*
 * This is the kernel NFS client file handle representation
 */
#define NFS_MAXFHSIZE		64
struct nfs_fh {
	unsigned short		size;
	unsigned char		data[NFS_MAXFHSIZE];
};

/*
 * This is really a general kernel constant, but since nothing like
 * this is defined in the kernel headers, I have to do it here.
 */
#define NFS_OFFSET_MAX		((__s64)((~(__u64)0) >> 1))


enum nfs3_stable_how {
	NFS_UNSTABLE = 0,
	NFS_DATA_SYNC = 1,
	NFS_FILE_SYNC = 2
};
#endif /* __KERNEL__ */
#endif /* _LINUX_NFS_H */
