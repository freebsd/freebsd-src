/*
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
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
 *	@(#)nfs_prot.h	8.1 (Berkeley) 6/6/93
 *
 * $Id: nfs_prot.h,v 5.2.2.1 1992/02/09 15:09:29 jsp beta $
 *
 */

#define	xdr_nfsstat xdr_enum
#define	xdr_ftype xdr_enum

#define NFS_PORT 2049
#define NFS_MAXDATA 8192
#define NFS_MAXPATHLEN 1024
#define NFS_MAXNAMLEN 255
#define NFS_FHSIZE 32
#define NFS_COOKIESIZE 4
#define NFS_FIFO_DEV -1
#define NFSMODE_FMT 0170000
#define NFSMODE_DIR 0040000
#define NFSMODE_CHR 0020000
#define NFSMODE_BLK 0060000
#define NFSMODE_REG 0100000
#define NFSMODE_LNK 0120000
#define NFSMODE_SOCK 0140000
#define NFSMODE_FIFO 0010000

enum nfsstat {
	NFS_OK = 0,
	NFSERR_PERM = 1,
	NFSERR_NOENT = 2,
	NFSERR_IO = 5,
	NFSERR_NXIO = 6,
	NFSERR_ACCES = 13,
	NFSERR_EXIST = 17,
	NFSERR_NODEV = 19,
	NFSERR_NOTDIR = 20,
	NFSERR_ISDIR = 21,
	NFSERR_FBIG = 27,
	NFSERR_NOSPC = 28,
	NFSERR_ROFS = 30,
	NFSERR_NAMETOOLONG = 63,
	NFSERR_NOTEMPTY = 66,
	NFSERR_DQUOT = 69,
	NFSERR_STALE = 70,
	NFSERR_WFLUSH = 99
};
typedef enum nfsstat nfsstat;
bool_t xdr_nfsstat();


enum ftype {
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
typedef enum ftype ftype;
/* static bool_t xdr_ftype(); */


struct nfs_fh {
	char data[NFS_FHSIZE];
};
typedef struct nfs_fh nfs_fh;
bool_t xdr_nfs_fh();


struct nfstime {
	u_int seconds;
	u_int useconds;
};
typedef struct nfstime nfstime;
/* static bool_t xdr_nfstime(); */


struct fattr {
	ftype type;
	u_int mode;
	u_int nlink;
	u_int uid;
	u_int gid;
	u_int size;
	u_int blocksize;
	u_int rdev;
	u_int blocks;
	u_int fsid;
	u_int fileid;
	nfstime atime;
	nfstime mtime;
	nfstime ctime;
};
typedef struct fattr fattr;
/* static bool_t xdr_fattr(); */


struct sattr {
	u_int mode;
	u_int uid;
	u_int gid;
	u_int size;
	nfstime atime;
	nfstime mtime;
};
typedef struct sattr sattr;
/* static bool_t xdr_sattr(); */


typedef char *filename;
/* static bool_t xdr_filename(); */


typedef char *nfspath;
bool_t xdr_nfspath();


struct attrstat {
	nfsstat status;
	union {
		fattr attributes;
	} attrstat_u;
};
typedef struct attrstat attrstat;
bool_t xdr_attrstat();


struct sattrargs {
	nfs_fh file;
	sattr attributes;
};
typedef struct sattrargs sattrargs;
bool_t xdr_sattrargs();


struct diropargs {
	nfs_fh dir;
	filename name;
};
typedef struct diropargs diropargs;
bool_t xdr_diropargs();


struct diropokres {
	nfs_fh file;
	fattr attributes;
};
typedef struct diropokres diropokres;
bool_t xdr_diropokres();


struct diropres {
	nfsstat status;
	union {
		diropokres diropres;
	} diropres_u;
};
typedef struct diropres diropres;
bool_t xdr_diropres();


struct readlinkres {
	nfsstat status;
	union {
		nfspath data;
	} readlinkres_u;
};
typedef struct readlinkres readlinkres;
bool_t xdr_readlinkres();


struct readargs {
	nfs_fh file;
	u_int offset;
	u_int count;
	u_int totalcount;
};
typedef struct readargs readargs;
bool_t xdr_readargs();


struct readokres {
	fattr attributes;
	struct {
		u_int data_len;
		char *data_val;
	} data;
};
typedef struct readokres readokres;
bool_t xdr_readokres();


struct readres {
	nfsstat status;
	union {
		readokres reply;
	} readres_u;
};
typedef struct readres readres;
bool_t xdr_readres();


struct writeargs {
	nfs_fh file;
	u_int beginoffset;
	u_int offset;
	u_int totalcount;
	struct {
		u_int data_len;
		char *data_val;
	} data;
};
typedef struct writeargs writeargs;
bool_t xdr_writeargs();


struct createargs {
	diropargs where;
	sattr attributes;
};
typedef struct createargs createargs;
bool_t xdr_createargs();


struct renameargs {
	diropargs from;
	diropargs to;
};
typedef struct renameargs renameargs;
bool_t xdr_renameargs();


struct linkargs {
	nfs_fh from;
	diropargs to;
};
typedef struct linkargs linkargs;
bool_t xdr_linkargs();


struct symlinkargs {
	diropargs from;
	nfspath to;
	sattr attributes;
};
typedef struct symlinkargs symlinkargs;
bool_t xdr_symlinkargs();


typedef char nfscookie[NFS_COOKIESIZE];
/* static bool_t xdr_nfscookie(); */


struct readdirargs {
	nfs_fh dir;
	nfscookie cookie;
	u_int count;
};
typedef struct readdirargs readdirargs;
bool_t xdr_readdirargs();


struct entry {
	u_int fileid;
	filename name;
	nfscookie cookie;
	struct entry *nextentry;
};
typedef struct entry entry;
/* static bool_t xdr_entry(); */


struct dirlist {
	entry *entries;
	bool_t eof;
};
typedef struct dirlist dirlist;
/* static bool_t xdr_dirlist(); */


struct readdirres {
	nfsstat status;
	union {
		dirlist reply;
	} readdirres_u;
};
typedef struct readdirres readdirres;
bool_t xdr_readdirres();


struct statfsokres {
	u_int tsize;
	u_int bsize;
	u_int blocks;
	u_int bfree;
	u_int bavail;
};
typedef struct statfsokres statfsokres;
bool_t xdr_statfsokres();


struct statfsres {
	nfsstat status;
	union {
		statfsokres reply;
	} statfsres_u;
};
typedef struct statfsres statfsres;
bool_t xdr_statfsres();


#define NFS_PROGRAM ((u_long)100003)
#define NFS_VERSION ((u_long)2)
#define NFSPROC_NULL ((u_long)0)
extern voidp nfsproc_null_2();
#define NFSPROC_GETATTR ((u_long)1)
extern attrstat *nfsproc_getattr_2();
#define NFSPROC_SETATTR ((u_long)2)
extern attrstat *nfsproc_setattr_2();
#define NFSPROC_ROOT ((u_long)3)
extern voidp nfsproc_root_2();
#define NFSPROC_LOOKUP ((u_long)4)
extern diropres *nfsproc_lookup_2();
#define NFSPROC_READLINK ((u_long)5)
extern readlinkres *nfsproc_readlink_2();
#define NFSPROC_READ ((u_long)6)
extern readres *nfsproc_read_2();
#define NFSPROC_WRITECACHE ((u_long)7)
extern voidp nfsproc_writecache_2();
#define NFSPROC_WRITE ((u_long)8)
extern attrstat *nfsproc_write_2();
#define NFSPROC_CREATE ((u_long)9)
extern diropres *nfsproc_create_2();
#define NFSPROC_REMOVE ((u_long)10)
extern nfsstat *nfsproc_remove_2();
#define NFSPROC_RENAME ((u_long)11)
extern nfsstat *nfsproc_rename_2();
#define NFSPROC_LINK ((u_long)12)
extern nfsstat *nfsproc_link_2();
#define NFSPROC_SYMLINK ((u_long)13)
extern nfsstat *nfsproc_symlink_2();
#define NFSPROC_MKDIR ((u_long)14)
extern diropres *nfsproc_mkdir_2();
#define NFSPROC_RMDIR ((u_long)15)
extern nfsstat *nfsproc_rmdir_2();
#define NFSPROC_READDIR ((u_long)16)
extern readdirres *nfsproc_readdir_2();
#define NFSPROC_STATFS ((u_long)17)
extern statfsres *nfsproc_statfs_2();

