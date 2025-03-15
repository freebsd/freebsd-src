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

/* 9P client definitions */

#ifndef FS_P9FS_P9_CLIENT_H
#define FS_P9FS_P9_CLIENT_H

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/_unrhdr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/dirent.h>
#include <sys/stdarg.h>

#include <fs/p9fs/p9_protocol.h>

/* 9P protocol versions */
enum p9_proto_versions {
	p9_proto_legacy,	/* legacy version */
	p9_proto_2000u,		/* Unix version */
	p9_proto_2000L,		/* Linux version */
};

/* P9 Request exchanged between Host and Guest */
struct p9_req_t {
	struct p9_buffer *tc;	/* request buffer */
	struct p9_buffer *rc;	/* response buffer */
};

/* 9P transport status */
enum transport_status {
	P9FS_CONNECT,		/* transport is connected */
	P9FS_BEGIN_DISCONNECT,/* transport has begun to disconnect */
	P9FS_DISCONNECT,	/* transport has been dosconnected */
};

/* This is set by QEMU so we will oblige */
#define P9FS_MTU 8192

/*
 * Even though we have a 8k buffer, Qemu is typically doing 8168
 * because of a HDR of 24. Use that amount for transfers so that we dont
 * drop anything.
 */
#define P9FS_IOUNIT (P9FS_MTU - 24)
#define P9FS_DIRENT_LEN 256
#define P9_NOTAG 0

/* Client state information */
struct p9_client {
	struct p9_trans_module *ops;		/* module API instantiated with this client */
	void *handle;				/* module-specific client handle */
	struct mtx clnt_mtx;			/* mutex to lock the client */
	struct mtx req_mtx;			/* mutex to lock the request buffer */
	struct cv req_cv;			/* condition variable on which to wake up thread */
	unsigned int msize;			/* maximum data size */
	unsigned char proto_version;		/* 9P version to use */
	struct unrhdr fidpool;			/* fid handle accounting for session */
	struct unrhdr tagpool;			/* transaction id accounting for session */
	enum transport_status trans_status;	/* tranport instance state */
};

/* The main fid structure which keeps track of the file.*/
struct p9_fid {
	struct p9_client *clnt;	/* the instatntiating 9P client */
	uint32_t fid;		/* numeric identifier */
	int mode;		/* current mode of this fid */
	struct p9_qid qid;	/* server identifier */
	uint32_t mtu;		/* max transferrable unit at a time */
	uid_t uid;		/* numeric uid of the local user who owns this handle */
	int v_opens;		/* keep count on the number of opens called with this fiel handle */
	STAILQ_ENTRY(p9_fid) fid_next;	/* points to next fid in the list */
};

/* Directory entry structure */
struct p9_dirent {
	struct p9_qid qid;		/* 9P server qid for this dirent */
	uint64_t d_off;			/* offset to the next dirent */
	unsigned char d_type;		/* file type */
	char d_name[P9FS_DIRENT_LEN];	/* file name */
	int len;
};

void p9_init_zones(void);
void p9_destroy_zones(void);

/* Session and client Init Ops */
struct p9_client *p9_client_create(struct mount *mp, int *error,
    const char *mount_tag);
void p9_client_destroy(struct p9_client *clnt);
struct p9_fid *p9_client_attach(struct p9_client *clnt, struct p9_fid *fid,
    const char *uname, uid_t n_uname, const char *aname, int *error);

/* FILE OPS - These are individually called from the specific vop function */

int p9_client_open(struct p9_fid *fid, int mode);
int p9_client_close(struct p9_fid *fid);
struct p9_fid *p9_client_walk(struct p9_fid *oldfid, uint16_t nwnames,
    char **wnames, int clone, int *error);
struct p9_fid *p9_fid_create(struct p9_client *clnt);
void p9_fid_destroy(struct p9_fid *fid);
uint16_t p9_tag_create(struct p9_client *clnt);
void p9_tag_destroy(struct p9_client *clnt, uint16_t tag);
int p9_client_clunk(struct p9_fid *fid);
int p9_client_version(struct p9_client *clnt);
int p9_client_readdir(struct p9_fid *fid, char *data, uint64_t offset, uint32_t count);
int p9_client_read(struct p9_fid *fid, uint64_t offset, uint32_t count, char *data);
int p9_client_write(struct p9_fid *fid, uint64_t offset, uint32_t count, char *data);
int p9_client_file_create(struct p9_fid *fid, char *name, uint32_t perm, int mode,
    char *extension);
int p9_client_remove(struct p9_fid *fid);
int p9_client_unlink(struct p9_fid *dfid, const char *name, int32_t flags);
int p9_dirent_read(struct p9_client *clnt, char *buf, int start, int len,
    struct p9_dirent *dirent);
int p9_client_statfs(struct p9_fid *fid, struct p9_statfs *stat);
int p9_client_statread(struct p9_client *clnt, char *data, size_t len, struct p9_wstat *st);
int p9_is_proto_dotu(struct p9_client *clnt);
int p9_is_proto_dotl(struct p9_client *clnt);
void p9_client_cb(struct p9_client *c, struct p9_req_t *req);
int p9stat_read(struct p9_client *clnt, char *data, size_t len, struct p9_wstat *st);
void p9_client_disconnect(struct p9_client *clnt);
void p9_client_begin_disconnect(struct p9_client *clnt);
int p9_create_symlink(struct p9_fid *fid, char *name, char *symtgt, gid_t gid);
int p9_create_hardlink(struct p9_fid *dfid, struct p9_fid *oldfid, char *name);
int p9_readlink(struct p9_fid *fid, char **target);
int p9_client_renameat(struct p9_fid *oldfid, char *oldname, struct p9_fid *newfid, char *newname);
int p9_client_getattr(struct p9_fid *fid, struct p9_stat_dotl *stat_dotl,
    uint64_t request_mask);
int p9_client_setattr(struct p9_fid *fid, struct p9_iattr_dotl *p9attr);

int p9_buf_vwritef(struct p9_buffer *buf, int proto_version, const char *fmt,
    va_list ap);
int p9_buf_readf(struct p9_buffer *buf, int proto_version, const char *fmt, ...);
int p9_buf_prepare(struct p9_buffer *buf, int8_t type);
int p9_buf_finalize(struct p9_client *clnt, struct p9_buffer *buf);
void p9_buf_reset(struct p9_buffer *buf);

#endif /* FS_P9FS_P9_CLIENT_H */
