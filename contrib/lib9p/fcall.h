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

enum l9p_ftype {
	L9P_TVERSION = 100,
	L9P_RVERSION,
	L9P_TAUTH = 102,
	L9P_RAUTH,
	L9P_TATTACH = 104,
	L9P_RATTACH,
	L9P_TERROR = 106, /* illegal */
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
};

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

enum {
	L9P_OREAD = 0,	/* open for read */
	L9P_OWRITE = 1,	/* write */
	L9P_ORDWR = 2,	/* read and write */
	L9P_OEXEC = 3,	/* execute, == read but check execute permission */
	L9P_OTRUNC = 16,	/* or'ed in (except for exec), truncate file first */
	L9P_OCEXEC = 32,	/* or'ed in, close on exec */
	L9P_ORCLOSE = 64,	/* or'ed in, remove on close */
	L9P_ODIRECT = 128,	/* or'ed in, direct access */
	L9P_ONONBLOCK = 256,	/* or'ed in, non-blocking call */
	L9P_OEXCL = 0x1000,	/* or'ed in, exclusive use (create only) */
	L9P_OLOCK = 0x2000,	/* or'ed in, lock after opening */
	L9P_OAPPEND = 0x4000	/* or'ed in, append only */
};

struct l9p_hdr {
	uint8_t type;
	uint16_t tag;
	uint32_t fid;
};

struct l9p_qid {
	enum l9p_qid_type type;
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
	uid_t n_uid;
	gid_t n_gid;
	uid_t n_muid;
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
	uid_t n_uname;
};

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
	uint64_t offset; /* Tread, Twrite */
	uint32_t count; /* Tread, Twrite, Rread */
	char *data; /* Twrite, Rread */
};

struct l9p_f_rstat {
	struct l9p_hdr hdr;
	struct l9p_stat stat;
};

struct l9p_f_twstat {
	struct l9p_hdr hdr;
	struct l9p_stat stat;
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
	struct l9p_f_io io;
};

#endif  /* LIB9P_FCALL_H */
