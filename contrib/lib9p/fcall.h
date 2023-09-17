/*
 * Copyright 2016 Jakub Klama <jceel@FreeBSD.org>
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * Based on libixp code: ©2007-2010 Kris Maglione <maglione.k at Gmail>
 */

#ifndef LIB9P_FCALL_H
#define LIB9P_FCALL_H

#include <stdint.h>

#define L9P_MAX_WELEM   256

/*
 * Function call/reply (Tfoo/Rfoo) numbers.
 *
 * These are protocol code numbers, so the exact values
 * matter.  However, __FIRST and __LAST_PLUS_ONE are for
 * debug code, and just need to encompass the entire range.
 *
 * Note that we rely (in the debug code) on Rfoo == Tfoo+1.
 */
enum l9p_ftype {
	L9P__FIRST = 6,		/* NB: must be <= all legal values */
	L9P_TLERROR = 6,	/* illegal; exists for parity with Rlerror */
	L9P_RLERROR,
	L9P_TSTATFS = 8,
	L9P_RSTATFS,
	L9P_TLOPEN = 12,
	L9P_RLOPEN,
	L9P_TLCREATE = 14,
	L9P_RLCREATE,
	L9P_TSYMLINK = 16,
	L9P_RSYMLINK,
	L9P_TMKNOD = 18,
	L9P_RMKNOD,
	L9P_TRENAME = 20,
	L9P_RRENAME,
	L9P_TREADLINK = 22,
	L9P_RREADLINK,
	L9P_TGETATTR = 24,
	L9P_RGETATTR,
	L9P_TSETATTR = 26,
	L9P_RSETATTR,
	L9P_TXATTRWALK = 30,
	L9P_RXATTRWALK,
	L9P_TXATTRCREATE = 32,
	L9P_RXATTRCREATE,
	L9P_TREADDIR = 40,
	L9P_RREADDIR,
	L9P_TFSYNC = 50,
	L9P_RFSYNC,
	L9P_TLOCK = 52,
	L9P_RLOCK,
	L9P_TGETLOCK = 54,
	L9P_RGETLOCK,
	L9P_TLINK = 70,
	L9P_RLINK,
	L9P_TMKDIR = 72,
	L9P_RMKDIR,
	L9P_TRENAMEAT = 74,
	L9P_RRENAMEAT,
	L9P_TUNLINKAT = 76,
	L9P_RUNLINKAT,
	L9P_TVERSION = 100,
	L9P_RVERSION,
	L9P_TAUTH = 102,
	L9P_RAUTH,
	L9P_TATTACH = 104,
	L9P_RATTACH,
	L9P_TERROR = 106, 	/* illegal */
	L9P_RERROR,
	L9P_TFLUSH = 108,
	L9P_RFLUSH,
	L9P_TWALK = 110,
	L9P_RWALK,
	L9P_TOPEN = 112,
	L9P_ROPEN,
	L9P_TCREATE = 114,
	L9P_RCREATE,
	L9P_TREAD = 116,
	L9P_RREAD,
	L9P_TWRITE = 118,
	L9P_RWRITE,
	L9P_TCLUNK = 120,
	L9P_RCLUNK,
	L9P_TREMOVE = 122,
	L9P_RREMOVE,
	L9P_TSTAT = 124,
	L9P_RSTAT,
	L9P_TWSTAT = 126,
	L9P_RWSTAT,
	L9P__LAST_PLUS_1,	/* NB: must be last */
};

/*
 * When a Tfoo request comes over the wire, we decode it
 * (pack.c) from wire format into a request laid out in
 * a "union l9p_fcall" object.  This object is not in wire
 * format, but rather in something more convenient for us
 * to operate on.
 *
 * We then dispatch the request (request.c, backend/fs.c) and
 * use another "union l9p_fcall" object to build a reply.
 * The reply is converted to wire format on the way back out
 * (pack.c again).
 *
 * All sub-objects start with a header containing the request
 * or reply type code and two-byte tag, and whether or not it
 * is needed, a four-byte fid.
 *
 * What this means here is that the data structures within
 * the union can be shared across various requests and replies.
 * For instance, replies to OPEN, CREATE, LCREATE, LOPEN, MKDIR, and
 * SYMLINK are all fairly similar (providing a qid and sometimes
 * an iounit) and hence can all use the l9p_f_ropen structure.
 * Which structures are used for which operations is somewhat
 * arbitrary; for programming ease, if an operation shares a
 * data structure, it still has its own name: there are union
 * members named ropen, rcreate, rlcreate, rlopen, rmkdir, and
 * rsymlink, even though all use struct l9p_f_ropen.
 *
 * The big exception to the above rule is struct l9p_f_io, which
 * is used as both request and reply for all of READ, WRITE, and
 * READDIR.  Moreover, the READDIR reply must be pre-packed into
 * wire format (it is handled like raw data a la READ).
 *
 * Some request messages (e.g., TREADLINK) fit in a header, having
 * just type code, tag, and fid.  These have no separate data
 * structure, nor union member name.  Similarly, some reply
 * messages (e.g., RCLUNK, RREMOVE, RRENAME) have just the type
 * code and tag.
 */

/*
 * Type code bits in (the first byte of) a qid.
 */
enum l9p_qid_type {
	L9P_QTDIR = 0x80, /* type bit for directories */
	L9P_QTAPPEND = 0x40, /* type bit for append only files */
	L9P_QTEXCL = 0x20, /* type bit for exclusive use files */
	L9P_QTMOUNT = 0x10, /* type bit for mounted channel */
	L9P_QTAUTH = 0x08, /* type bit for authentication file */
	L9P_QTTMP = 0x04, /* type bit for non-backed-up file */
	L9P_QTSYMLINK = 0x02, /* type bit for symbolic link */
	L9P_QTFILE = 0x00 /* type bits for plain file */
};

/*
 * Extra permission bits in create and file modes (stat).
 */
#define L9P_DMDIR 0x80000000
enum {
	L9P_DMAPPEND = 0x40000000,
	L9P_DMEXCL = 0x20000000,
	L9P_DMMOUNT = 0x10000000,
	L9P_DMAUTH = 0x08000000,
	L9P_DMTMP = 0x04000000,
	L9P_DMSYMLINK = 0x02000000,
	/* 9P2000.u extensions */
	L9P_DMDEVICE = 0x00800000,
	L9P_DMNAMEDPIPE = 0x00200000,
	L9P_DMSOCKET = 0x00100000,
	L9P_DMSETUID = 0x00080000,
	L9P_DMSETGID = 0x00040000,
};

/*
 * Open/create mode bits in 9P2000 and 9P2000.u operations
 * (not Linux lopen and lcreate flags, which are different).
 * Note that the mode field is only one byte wide.
 */
enum l9p_omode {
	L9P_OREAD = 0,	/* open for read */
	L9P_OWRITE = 1,	/* write */
	L9P_ORDWR = 2,	/* read and write */
	L9P_OEXEC = 3,	/* execute, == read but check execute permission */
	L9P_OACCMODE = 3, /* mask for the above access-mode bits */
	L9P_OTRUNC = 16,	/* or'ed in (except for exec), truncate file first */
	L9P_OCEXEC = 32,	/* or'ed in, close on exec */
	L9P_ORCLOSE = 64,	/* or'ed in, remove on close */
	L9P_ODIRECT = 128,	/* or'ed in, direct access */
};

/*
 * Flag bits in 9P2000.L operations (Tlopen, Tlcreate).  These are
 * basically just the Linux L_* flags.  The bottom 3 bits are the
 * same as for l9p_omode, although open-for-exec is not used:
 * instead, the client does a Tgetattr and checks the mode for
 * execute bits, then just opens for reading.
 *
 * Each L_O_xxx is just value O_xxx has on Linux in <fcntl.h>;
 * not all are necessarily used.  From observation, we do get
 * L_O_CREAT and L_O_EXCL when creating with exclusive, and always
 * get L_O_LARGEFILE.  We do get L_O_APPEND when opening for
 * append.  We also get both L_O_DIRECT and L_O_DIRECTORY set
 * when opening directories.
 *
 * We probably never get L_O_NOCTTY which makes no sense, and
 * some of the other options may need to be handled on the client.
 */
enum l9p_l_o_flags {
	L9P_L_O_CREAT =		000000100U,
	L9P_L_O_EXCL =		000000200U,
	L9P_L_O_NOCTTY =	000000400U,
	L9P_L_O_TRUNC =		000001000U,
	L9P_L_O_APPEND =	000002000U,
	L9P_L_O_NONBLOCK =	000004000U,
	L9P_L_O_DSYNC =		000010000U,
	L9P_L_O_FASYNC =	000020000U,
	L9P_L_O_DIRECT =	000040000U,
	L9P_L_O_LARGEFILE =	000100000U,
	L9P_L_O_DIRECTORY =	000200000U,
	L9P_L_O_NOFOLLOW =	000400000U,
	L9P_L_O_NOATIME =	001000000U,
	L9P_L_O_CLOEXEC =	002000000U,
	L9P_L_O_SYNC =		004000000U,
	L9P_L_O_PATH =		010000000U,
	L9P_L_O_TMPFILE =	020000000U,
};

struct l9p_hdr {
	uint8_t type;
	uint16_t tag;
	uint32_t fid;
};

struct l9p_qid {
	uint8_t  type;
	uint32_t version;
	uint64_t path;
};

struct l9p_stat {
	uint16_t type;
	uint32_t dev;
	struct l9p_qid qid;
	uint32_t mode;
	uint32_t atime;
	uint32_t mtime;
	uint64_t length;
	char *name;
	char *uid;
	char *gid;
	char *muid;
	char *extension;
	uint32_t n_uid;
	uint32_t n_gid;
	uint32_t n_muid;
};

#define	L9P_FSTYPE	 0x01021997

struct l9p_statfs {
	uint32_t type;		/* file system type */
	uint32_t bsize;		/* block size for I/O */
	uint64_t blocks;	/* file system size (bsize-byte blocks) */
	uint64_t bfree;		/* free blocks in fs */
	uint64_t bavail;	/* free blocks avail to non-superuser*/
	uint64_t files;		/* file nodes in file system (# inodes) */
	uint64_t ffree;		/* free file nodes in fs */
	uint64_t fsid;		/* file system identifier */
	uint32_t namelen;	/* maximum length of filenames */
};

struct l9p_f_version {
	struct l9p_hdr hdr;
	uint32_t msize;
	char *version;
};

struct l9p_f_tflush {
	struct l9p_hdr hdr;
	uint16_t oldtag;
};

struct l9p_f_error {
	struct l9p_hdr hdr;
	char *ename;
	uint32_t errnum;
};

struct l9p_f_ropen {
	struct l9p_hdr hdr;
	struct l9p_qid qid;
	uint32_t iounit;
};

struct l9p_f_rauth {
	struct l9p_hdr hdr;
	struct l9p_qid aqid;
};

struct l9p_f_attach {
	struct l9p_hdr hdr;
	uint32_t afid;
	char *uname;
	char *aname;
	uint32_t n_uname;
};
#define	L9P_NOFID ((uint32_t)-1)	/* in Tattach, no auth fid */
#define	L9P_NONUNAME ((uint32_t)-1)	/* in Tattach, no n_uname */

struct l9p_f_tcreate {
	struct l9p_hdr hdr;
	uint32_t perm;
	char *name;
	uint8_t mode; /* +Topen */
	char *extension;
};

struct l9p_f_twalk {
	struct l9p_hdr hdr;
	uint32_t newfid;
	uint16_t nwname;
	char *wname[L9P_MAX_WELEM];
};

struct l9p_f_rwalk {
	struct l9p_hdr hdr;
	uint16_t nwqid;
	struct l9p_qid wqid[L9P_MAX_WELEM];
};

struct l9p_f_io {
	struct l9p_hdr hdr;
	uint64_t offset; /* Tread, Twrite, Treaddir */
	uint32_t count; /* Tread, Twrite, Rread, Treaddir, Rreaddir */
};

struct l9p_f_rstat {
	struct l9p_hdr hdr;
	struct l9p_stat stat;
};

struct l9p_f_twstat {
	struct l9p_hdr hdr;
	struct l9p_stat stat;
};

struct l9p_f_rstatfs {
	struct l9p_hdr hdr;
	struct l9p_statfs statfs;
};

/* Used for Tlcreate, Tlopen, Tmkdir, Tunlinkat. */
struct l9p_f_tlcreate {
	struct l9p_hdr hdr;
	char *name;		/* Tlcreate, Tmkdir, Tunlinkat */
	uint32_t flags;		/* Tlcreate, Tlopen, Tmkdir, Tunlinkat */
	uint32_t mode;		/* Tlcreate, Tmkdir */
	uint32_t gid;		/* Tlcreate, Tmkdir */
};

struct l9p_f_tsymlink {
	struct l9p_hdr hdr;
	char *name;
	char *symtgt;
	uint32_t gid;
};

struct l9p_f_tmknod {
	struct l9p_hdr hdr;
	char *name;
	uint32_t mode;
	uint32_t major;
	uint32_t minor;
	uint32_t gid;
};

struct l9p_f_trename {
	struct l9p_hdr hdr;
	uint32_t dfid;
	char *name;
};

struct l9p_f_rreadlink {
	struct l9p_hdr hdr;
	char *target;
};

struct l9p_f_tgetattr {
	struct l9p_hdr hdr;
	uint64_t request_mask;
};

struct l9p_f_rgetattr {
	struct l9p_hdr hdr;
	uint64_t valid;
	struct l9p_qid qid;
	uint32_t mode;
	uint32_t uid;
	uint32_t gid;
	uint64_t nlink;
	uint64_t rdev;
	uint64_t size;
	uint64_t blksize;
	uint64_t blocks;
	uint64_t atime_sec;
	uint64_t atime_nsec;
	uint64_t mtime_sec;
	uint64_t mtime_nsec;
	uint64_t ctime_sec;
	uint64_t ctime_nsec;
	uint64_t btime_sec;
	uint64_t btime_nsec;
	uint64_t gen;
	uint64_t data_version;
};

/* Fields in req->request_mask and reply->valid for Tgetattr, Rgetattr. */
enum l9pl_getattr_flags {
	L9PL_GETATTR_MODE = 0x00000001,
	L9PL_GETATTR_NLINK = 0x00000002,
	L9PL_GETATTR_UID = 0x00000004,
	L9PL_GETATTR_GID = 0x00000008,
	L9PL_GETATTR_RDEV = 0x00000010,
	L9PL_GETATTR_ATIME = 0x00000020,
	L9PL_GETATTR_MTIME = 0x00000040,
	L9PL_GETATTR_CTIME = 0x00000080,
	L9PL_GETATTR_INO = 0x00000100,
	L9PL_GETATTR_SIZE = 0x00000200,
	L9PL_GETATTR_BLOCKS = 0x00000400,
	/* everything up to and including BLOCKS is BASIC */
	L9PL_GETATTR_BASIC = L9PL_GETATTR_MODE |
		L9PL_GETATTR_NLINK |
		L9PL_GETATTR_UID |
		L9PL_GETATTR_GID |
		L9PL_GETATTR_RDEV |
		L9PL_GETATTR_ATIME |
		L9PL_GETATTR_MTIME |
		L9PL_GETATTR_CTIME |
		L9PL_GETATTR_INO |
		L9PL_GETATTR_SIZE |
		L9PL_GETATTR_BLOCKS,
	L9PL_GETATTR_BTIME = 0x00000800,
	L9PL_GETATTR_GEN = 0x00001000,
	L9PL_GETATTR_DATA_VERSION = 0x00002000,
	/* BASIC + birthtime + gen + data-version = ALL */
	L9PL_GETATTR_ALL = L9PL_GETATTR_BASIC |
		L9PL_GETATTR_BTIME |
		L9PL_GETATTR_GEN |
		L9PL_GETATTR_DATA_VERSION,
};

struct l9p_f_tsetattr {
	struct l9p_hdr hdr;
	uint32_t valid;
	uint32_t mode;
	uint32_t uid;
	uint32_t gid;
	uint64_t size;
	uint64_t atime_sec;	/* if valid & L9PL_SETATTR_ATIME_SET */
	uint64_t atime_nsec;	/* (else use on-server time) */
	uint64_t mtime_sec;	/* if valid & L9PL_SETATTR_MTIME_SET */
	uint64_t mtime_nsec;	/* (else use on-server time) */
};

/* Fields in req->valid for Tsetattr. */
enum l9pl_setattr_flags {
	L9PL_SETATTR_MODE = 0x00000001,
	L9PL_SETATTR_UID = 0x00000002,
	L9PL_SETATTR_GID = 0x00000004,
	L9PL_SETATTR_SIZE = 0x00000008,
	L9PL_SETATTR_ATIME = 0x00000010,
	L9PL_SETATTR_MTIME = 0x00000020,
	L9PL_SETATTR_CTIME = 0x00000040,
	L9PL_SETATTR_ATIME_SET = 0x00000080,
	L9PL_SETATTR_MTIME_SET = 0x00000100,
};

struct l9p_f_txattrwalk {
	struct l9p_hdr hdr;
	uint32_t newfid;
	char *name;
};

struct l9p_f_rxattrwalk {
	struct l9p_hdr hdr;
	uint64_t size;
};

struct l9p_f_txattrcreate {
	struct l9p_hdr hdr;
	char *name;
	uint64_t attr_size;
	uint32_t flags;
};

struct l9p_f_tlock {
	struct l9p_hdr hdr;
	uint8_t type;		/* from l9pl_lock_type */
	uint32_t flags;		/* from l9pl_lock_flags */
	uint64_t start;
	uint64_t length;
	uint32_t proc_id;
	char *client_id;
};

enum l9pl_lock_type {
	L9PL_LOCK_TYPE_RDLOCK =	0,
	L9PL_LOCK_TYPE_WRLOCK =	1,
	L9PL_LOCK_TYPE_UNLOCK =	2,
};

enum l9pl_lock_flags {
	L9PL_LOCK_TYPE_BLOCK = 1,
	L9PL_LOCK_TYPE_RECLAIM = 2,
};

struct l9p_f_rlock {
	struct l9p_hdr hdr;
	uint8_t status;		/* from l9pl_lock_status */
};

enum l9pl_lock_status {
	L9PL_LOCK_SUCCESS = 0,
	L9PL_LOCK_BLOCKED = 1,
	L9PL_LOCK_ERROR = 2,
	L9PL_LOCK_GRACE = 3,
};

struct l9p_f_getlock {
	struct l9p_hdr hdr;
	uint8_t type;		/* from l9pl_lock_type */
	uint64_t start;
	uint64_t length;
	uint32_t proc_id;
	char *client_id;
};

struct l9p_f_tlink {
	struct l9p_hdr hdr;
	uint32_t dfid;
	char *name;
};

struct l9p_f_trenameat {
	struct l9p_hdr hdr;
	char *oldname;
	uint32_t newdirfid;
	char *newname;
};

/*
 * Flags in Tunlinkat (which re-uses f_tlcreate data structure but
 * with different meaning).
 */
enum l9p_l_unlinkat_flags {
	/* not sure if any other AT_* flags are passed through */
	L9PL_AT_REMOVEDIR =	0x0200,
};

union l9p_fcall {
	struct l9p_hdr hdr;
	struct l9p_f_version version;
	struct l9p_f_tflush tflush;
	struct l9p_f_ropen ropen;
	struct l9p_f_ropen rcreate;
	struct l9p_f_ropen rattach;
	struct l9p_f_error error;
	struct l9p_f_rauth rauth;
	struct l9p_f_attach tattach;
	struct l9p_f_attach tauth;
	struct l9p_f_tcreate tcreate;
	struct l9p_f_tcreate topen;
	struct l9p_f_twalk twalk;
	struct l9p_f_rwalk rwalk;
	struct l9p_f_twstat twstat;
	struct l9p_f_rstat rstat;
	struct l9p_f_rstatfs rstatfs;
	struct l9p_f_tlcreate tlopen;
	struct l9p_f_ropen rlopen;
	struct l9p_f_tlcreate tlcreate;
	struct l9p_f_ropen rlcreate;
	struct l9p_f_tsymlink tsymlink;
	struct l9p_f_ropen rsymlink;
	struct l9p_f_tmknod tmknod;
	struct l9p_f_ropen rmknod;
	struct l9p_f_trename trename;
	struct l9p_f_rreadlink rreadlink;
	struct l9p_f_tgetattr tgetattr;
	struct l9p_f_rgetattr rgetattr;
	struct l9p_f_tsetattr tsetattr;
	struct l9p_f_txattrwalk txattrwalk;
	struct l9p_f_rxattrwalk rxattrwalk;
	struct l9p_f_txattrcreate txattrcreate;
	struct l9p_f_tlock tlock;
	struct l9p_f_rlock rlock;
	struct l9p_f_getlock getlock;
	struct l9p_f_tlink tlink;
	struct l9p_f_tlcreate tmkdir;
	struct l9p_f_ropen rmkdir;
	struct l9p_f_trenameat trenameat;
	struct l9p_f_tlcreate tunlinkat;
	struct l9p_f_io io;
};

#endif  /* LIB9P_FCALL_H */
