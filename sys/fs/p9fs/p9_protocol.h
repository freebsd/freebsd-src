/*-
 * Copyright (c) 2017 Juniper Networks, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
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
 */

/* File contains 9P protocol definitions */

#ifndef FS_P9FS_P9_PROTOCOL_H
#define FS_P9FS_P9_PROTOCOL_H

#include <sys/types.h>

/* 9P message types */
enum p9_cmds_t {
	P9PROTO_TLERROR = 6,	/* not used */
	P9PROTO_RLERROR,	/* response for any failed request */
	P9PROTO_TSTATFS = 8,	/* file system status request */
	P9PROTO_RSTATFS,	/* file system status response */
	P9PROTO_TLOPEN = 12,	/* open a file (9P2000.L) */
	P9PROTO_RLOPEN,		/* response to opne request (9P2000.L) */
	P9PROTO_TLCREATE = 14,	/* prepare for handle for I/O on a new file (9P2000.L) */
	P9PROTO_RLCREATE,	/* response with file access information (9P2000.L) */
	P9PROTO_TSYMLINK = 16,	/* symlink creation request */
	P9PROTO_RSYMLINK,	/* symlink creation response */
	P9PROTO_TMKNOD = 18,	/* create a special file object request */
	P9PROTO_RMKNOD,		/* create a special file object response */
	P9PROTO_TRENAME = 20,	/* rename a file request */
	P9PROTO_RRENAME,	/* rename a file response */
	P9PROTO_TREADLINK = 22,	/* request to read value of symbolic link */
	P9PROTO_RREADLINK,	/* response to read value of symbolic link request */
	P9PROTO_TGETATTR = 24,	/* get file attributes request */
	P9PROTO_RGETATTR,	/* get file attributes response */
	P9PROTO_TSETATTR = 26,	/* set file attributes request */
	P9PROTO_RSETATTR,	/* set file attributes response */
	P9PROTO_TXATTRWALK = 30,/* request to read extended attributes */
	P9PROTO_RXATTRWALK,	/* response from server with attributes */
	P9PROTO_TXATTRCREATE = 32,/* request to set extended attribute */
	P9PROTO_RXATTRCREATE,	/* response from server for setting extended attribute */
	P9PROTO_TREADDIR = 40,	/* request to read a directory */
	P9PROTO_RREADDIR,	/* response from server for read request */
	P9PROTO_TFSYNC = 50,	/* request to flush an cached data to disk */
	P9PROTO_RFSYNC,		/* response when cache dat is flushed */
	P9PROTO_TLOCK = 52,	/* acquire or release a POSIX record lock */
	P9PROTO_RLOCK,		/* response with the status of the lock */
	P9PROTO_TGETLOCK = 54,	/* request to check for presence of a POSIX record lock */
	P9PROTO_RGETLOCK,	/* response with the details of the lock if acquired */
	P9PROTO_TLINK = 70,	/* request to create hard link */
	P9PROTO_RLINK,		/* create hard link response */
	P9PROTO_TMKDIR = 72,	/* create a directory request */
	P9PROTO_RMKDIR,		/* create a directory response */
	P9PROTO_TRENAMEAT = 74,	/* request to rename a file or directory */
	P9PROTO_RRENAMEAT,	/* reponse to rename request */
	P9PROTO_TUNLINKAT = 76,	/* unlink a file or directory */
	P9PROTO_RUNLINKAT,	/* reponse to unlink request */
	P9PROTO_TVERSION = 100,	/* request for version handshake */
	P9PROTO_RVERSION,	/* response for version handshake */
	P9PROTO_TAUTH = 102,	/* request to establish authentication channel */
	P9PROTO_RAUTH,		/* response with authentication information */
	P9PROTO_TATTACH = 104,	/* establish a user access to a file system*/
	P9PROTO_RATTACH,	/* response with top level handle to file hierarchy */
	P9PROTO_TERROR = 106,	/* not used */
	P9PROTO_RERROR,		/* response for any failed request */
	P9PROTO_TFLUSH = 108,	/* request to abort a previous request */
	P9PROTO_RFLUSH,		/* response when previous request has been cancelled */
	P9PROTO_TWALK = 110,	/* descend a directory hierarchy */
	P9PROTO_RWALK,		/* response with new handle for position within hierarchy */
	P9PROTO_TOPEN = 112,	/* prepare file handle for I/O for an existing file */
	P9PROTO_ROPEN,		/* response with file access information */
	P9PROTO_TCREATE = 114,	/* prepare for handle for I/O on a new file */
	P9PROTO_RCREATE,	/* response with file access information */
	P9PROTO_TREAD = 116,	/* request to transfer data from a file */
	P9PROTO_RREAD,		/* response with data requested */
	P9PROTO_TWRITE = 118,	/* request to transfer data to a file */
	P9PROTO_RWRITE,		/* response with how much data was written to the file */
	P9PROTO_TCLUNK = 120,	/* forget about a handle to a file within the File System */
	P9PROTO_RCLUNK,		/* response from the server for forgetting the file handle */
	P9PROTO_TREMOVE = 122,	/* request to remove a file */
	P9PROTO_RREMOVE,	/* response when server has removed the file */
	P9PROTO_TSTAT = 124,	/* request file entity attributes */
	P9PROTO_RSTAT,		/* response with file entity attributes */
	P9PROTO_TWSTAT = 126,	/* request to update file entity attributes */
	P9PROTO_RWSTAT,		/* response when file entity attributes are updated */
};

/* File Open Modes */
enum p9_open_mode_t {
	P9PROTO_OREAD = 0x00,	/* open file for reading only */
	P9PROTO_OWRITE = 0x01,	/* open file for writing only */
	P9PROTO_ORDWR = 0x02,	/* open file for both reading and writing */
	P9PROTO_OEXEC = 0x03,	/* open file for execution */
	P9PROTO_OTRUNC = 0x10,	/* truncate file to zero length  before opening it */
	P9PROTO_OREXEC = 0x20,	/* close the file when exec system call is made */
	P9PROTO_ORCLOSE = 0x40,	/* remove the file when it is closed */
	P9PROTO_OAPPEND = 0x80,	/* open the file and seek to the end of the file */
	P9PROTO_OEXCL = 0x1000,	/* only create a file and not open it */
};

/* FIle Permissions */
enum p9_perm_t {
	P9PROTO_DMDIR = 0x80000000,	/* permission  bit for directories */
	P9PROTO_DMAPPEND = 0x40000000,	/* permission bit  for is append-only */
	P9PROTO_DMEXCL = 0x20000000,	/* permission  bit for exclusive use (only one open handle allowed) */
	P9PROTO_DMMOUNT = 0x10000000,	/* permission  bit for mount points */
	P9PROTO_DMAUTH = 0x08000000,	/* permission  bit for authentication file */
	P9PROTO_DMTMP = 0x04000000,	/* permission bit for non-backed-up files */
	P9PROTO_DMSYMLINK = 0x02000000,	/* permission bit for symbolic link (9P2000.u) */
	P9PROTO_DMLINK = 0x01000000,	/* permission bit for hard-link (9P2000.u) */
	P9PROTO_DMDEVICE = 0x00800000,	/* permission bit for device files (9P2000.u) */
	P9PROTO_DMNAMEDPIPE = 0x00200000,/* permission bit for named pipe (9P2000.u) */
	P9PROTO_DMSOCKET = 0x00100000,	/* permission bit for socket (9P2000.u) */
	P9PROTO_DMSETUID = 0x00080000,	/* permission bit for setuid (9P2000.u) */
	P9PROTO_DMSETGID = 0x00040000,	/* permission bit for setgid (9P2000.u) */
	P9PROTO_DMSETVTX = 0x00010000,	/* permission bit for sticky bit (9P2000.u) */
};

/*
 * QID types - they are primarly used to
 * differentiate semantics for a file system
 */
enum p9_qid_t {
	P9PROTO_QTDIR = 0x80,		/* directory */
	P9PROTO_QTAPPEND = 0x40,	/* append-only */
	P9PROTO_QTEXCL = 0x20,		/* exclusive use (only one open handle allowed)*/
	P9PROTO_QTMOUNT = 0x10,		/* mount points */
	P9PROTO_QTAUTH = 0x08,		/* authentication file */
	P9PROTO_QTTMP = 0x04,		/* non-backed-up files */
	P9PROTO_QTSYMLINK = 0x02,	/* symbolic links */
	P9PROTO_QTLINK = 0x01,		/* hard link */
	P9PROTO_QTFILE = 0x00,		/* normal files */
};

/* P9 Magic Numbers */
#define P9PROTO_NOFID	(uint32_t)(~0)
#define P9_DEFUNAME	"nobody"
#define P9_DEFANAME	""
#define P9_NONUNAME	(uint32_t)(~0)
#define P9_MAXWELEM	16

/* Exchange unit between Qemu and Client */
struct p9_qid {
	uint8_t type;		/* the type of the file */
	uint32_t version;	/* version number for given path */
	uint64_t path;		/* the file servers unique id for file */
};

/* FS information stat structure */
struct p9_statfs {
	uint32_t type;		/* type of file system */
	uint32_t bsize;		/* optimal transfer block size */
	uint64_t blocks;	/* total data blocks in file system */
	uint64_t bfree;		/* free blocks in fs */
	uint64_t bavail;	/* free blocks avail to non-superuser */
	uint64_t files;		/* total file nodes in file system */
	uint64_t ffree;		/* free file nodes in fs */
	uint64_t fsid;		/* file system id */
	uint32_t namelen;	/* maximum length of filenames */
};


/* File system metadata information */
struct p9_wstat {
	uint16_t size;		/* total byte count of the following data */
	uint16_t type;		/* type of file */
	uint32_t dev;		/* id of device containing file */
	struct p9_qid qid;	/* identifier used by server for file system entity information */
	uint32_t mode;		/* protection */
	uint32_t atime;		/* time of last access */
	uint32_t mtime;		/* time of last modification */
	uint64_t length;	/* length of file in bytes */
	char *name;		/* file name */
	char *uid;		/* user ID of owner */
	char *gid;		/* group ID of owner */
	char *muid;		/* name of the user who last modified the file */
	char *extension;	/* 9p2000.u extensions */
	uid_t n_uid;		/* 9p2000.u extensions */
	gid_t n_gid;		/* 9p2000.u extensions */
	uid_t n_muid;		/* 9p2000.u extensions */
};

/* The linux version of FS information stat structure*/
struct p9_stat_dotl {
	uint64_t st_result_mask;/* indicates fields that are requested */
	struct p9_qid qid;	/* identifier used by server for file system entity information */
	uint32_t st_mode;	/* protection */
	uid_t st_uid;		/* user ID of owner */
	gid_t st_gid;		/* group ID of owner */
	uint64_t st_nlink;	/* number of hard links */
	uint64_t st_rdev;	/* device ID (if special file) */
	uint64_t st_size;	/* total size, in bytes */
	uint64_t st_blksize;	/* blocksize for file system I/O */
	uint64_t st_blocks;	/* number of 512B blocks allocated */
	uint64_t st_atime_sec;	/* time of last access, seconds */
	uint64_t st_atime_nsec;	/* time of last access, nanoseconds */
	uint64_t st_mtime_sec;	/* time of last modification, seconds */
	uint64_t st_mtime_nsec;	/* time of last modifictaion, nanoseconds */
	uint64_t st_ctime_sec;	/* time of last status change, seconds*/
	uint64_t st_ctime_nsec;	/* time of last status change, nanoseconds*/
	uint64_t st_btime_sec;	/* following memebers are reserved for future use */
	uint64_t st_btime_nsec;
	uint64_t st_gen;
	uint64_t st_data_version;
};

/* P9 inode attribute for setattr */
struct p9_iattr_dotl {
	uint32_t valid;		/* bit fields specifying which fields are valid */
	uint32_t mode;		/* protection */
	uid_t uid;		/* user id of owner */
	gid_t gid;		/* group id */
	uint64_t size;		/* file size */
	uint64_t atime_sec;	/* last access time in seconds */
	uint64_t atime_nsec;	/* last access time in nanoseconds */
	uint64_t mtime_sec;	/* last modification time in seconds */
	uint64_t mtime_nsec;	/* last modification time in nanoseconds */
};

#define P9PROTO_STATS_MODE		0x00000001ULL
#define P9PROTO_STATS_NLINK		0x00000002ULL
#define P9PROTO_STATS_UID		0x00000004ULL
#define P9PROTO_STATS_GID		0x00000008ULL
#define P9PROTO_STATS_RDEV		0x00000010ULL
#define P9PROTO_STATS_ATIME		0x00000020ULL
#define P9PROTO_STATS_MTIME		0x00000040ULL
#define P9PROTO_STATS_CTIME		0x00000080ULL
#define P9PROTO_STATS_INO		0x00000100ULL
#define P9PROTO_STATS_SIZE		0x00000200ULL
#define P9PROTO_STATS_BLOCKS		0x00000400ULL

#define P9PROTO_STATS_BTIME		0x00000800ULL
#define P9PROTO_STATS_GEN		0x00001000ULL
#define P9PROTO_STATS_DATA_VERSION	0x00002000ULL

#define P9PROTO_STATS_BASIC		0x000007ffULL /* Mask for fields up to BLOCKS */
#define P9PROTO_STATS_ALL		0x00003fffULL /* Mask for All fields above */

#define P9PROTO_SETATTR_MODE		0x00000001UL
#define P9PROTO_SETATTR_UID		0x00000002UL
#define P9PROTO_SETATTR_GID		0x00000004UL
#define P9PROTO_SETATTR_SIZE		0x00000008UL
#define P9PROTO_SETATTR_ATIME		0x00000010UL
#define P9PROTO_SETATTR_MTIME		0x00000020UL
#define P9PROTO_SETATTR_CTIME		0x00000040UL
#define P9PROTO_SETATTR_ATIME_SET	0x00000080UL
#define P9PROTO_SETATTR_MTIME_SET	0x00000100UL
#define P9PROTO_SETATTR_MASK		0x000001bfUL

#define P9PROTO_TGETATTR_BLK		512

#define	P9PROTO_UNLINKAT_REMOVEDIR	0x200

/* PDU buffer used for SG lists. */
struct p9_buffer {
	uint32_t size;
	uint16_t tag;
	uint8_t id;
	size_t offset;
	size_t capacity;
	uint8_t *sdata;
};

#endif /* FS_P9FS_P9_PROTOCOL_H */
