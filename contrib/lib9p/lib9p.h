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


#ifndef LIB9P_LIB9P_H
#define LIB9P_LIB9P_H

#include <stdio.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/uio.h>
#include <pthread.h>

#if defined(__FreeBSD__)
#include <sys/sbuf.h>
#else
#include "sbuf/sbuf.h"
#endif

#include "fcall.h"
#include "hashtable.h"

#define L9P_DEFAULT_MSIZE   8192
#define L9P_MAX_IOV         8

struct l9p_request;

typedef int (l9p_get_response_buffer_t) (struct l9p_request *,
    struct iovec *, size_t *, void *);

typedef int (l9p_send_response_t) (struct l9p_request *, const struct iovec *,
    const size_t, const size_t, void *);

enum l9p_pack_mode {
	L9P_PACK,
	L9P_UNPACK
};

enum l9p_integer_type {
	L9P_BYTE = 1,
	L9P_WORD = 2,
	L9P_DWORD = 4,
	L9P_QWORD = 8
};

enum l9p_version {
	L9P_INVALID_VERSION = 0,
	L9P_2000 = 1,
	L9P_2000U = 2,
	L9P_2000L = 3
};

struct l9p_message {
	enum l9p_pack_mode lm_mode;
	struct iovec lm_iov[L9P_MAX_IOV];
	size_t lm_niov;
	size_t lm_cursor_iov;
	size_t lm_cursor_offset;
	size_t lm_size;
};

/*
 * Data structure for a request/response pair (Tfoo/Rfoo).
 *
 * We have room for two incoming fids, in case we are
 * using 9P2000.L protocol.  Note that nothing that uses two
 * fids also has an output fid (newfid), so we could have a
 * union of lr_fid2 and lr_newfid, but keeping them separate
 * is probably a bit less error-prone.  (If we want to shave
 * memory requirements there are more places to look.)
 */
struct l9p_request {
	uint32_t lr_tag;
	struct l9p_message lr_req_msg;
	struct l9p_message lr_resp_msg;
	union l9p_fcall lr_req;
	union l9p_fcall lr_resp;
	struct l9p_openfile *lr_fid;
	struct l9p_openfile *lr_fid2;
	struct l9p_openfile *lr_newfid;
	struct l9p_connection *lr_conn;
	pthread_t lr_thread;
	void *lr_aux;
	struct iovec lr_data_iov[L9P_MAX_IOV];
	size_t lr_data_niov;
};

struct l9p_openfile {
	void *lo_aux;
	uint32_t lo_fid;
	struct l9p_qid lo_qid;
	struct l9p_connection *lo_conn;
};

/* N.B.: these dirents are variable length and for .L only */
struct l9p_dirent {
	struct l9p_qid qid;
	uint64_t offset;
	uint8_t type;
	char *name;
};

struct l9p_connection {
	struct l9p_server *lc_server;
	enum l9p_version lc_version;
	pthread_mutex_t lc_send_lock;
	uint32_t lc_msize;
	uint32_t lc_max_io_size;
	l9p_send_response_t *lc_send_response;
	l9p_get_response_buffer_t *lc_get_response_buffer;
	void *lc_get_response_buffer_aux;
	void *lc_send_response_aux;
	void *lc_softc;
	struct ht lc_files;
	struct ht lc_requests;
	LIST_ENTRY(l9p_connection) lc_link;
};

struct l9p_server {
	struct l9p_backend *ls_backend;
	enum l9p_version ls_max_version;
	LIST_HEAD(, l9p_connection) ls_conns;
};

struct l9p_backend {
	void *softc;
	void (*freefid)(void *, struct l9p_openfile *);
	void (*attach)(void *, struct l9p_request *);
	void (*clunk)(void *, struct l9p_request *);
	void (*create)(void *, struct l9p_request *);
	void (*flush)(void *, struct l9p_request *);
	void (*open)(void *, struct l9p_request *);
	void (*read)(void *, struct l9p_request *);
	void (*remove)(void *, struct l9p_request *);
	void (*stat)(void *, struct l9p_request *);
	void (*walk)(void *, struct l9p_request *);
	void (*write)(void *, struct l9p_request *);
	void (*wstat)(void *, struct l9p_request *);
	void (*statfs)(void *, struct l9p_request *);
	void (*lopen)(void *, struct l9p_request *);
	void (*lcreate)(void *, struct l9p_request *);
	void (*symlink)(void *, struct l9p_request *);
	void (*mknod)(void *, struct l9p_request *);
	void (*rename)(void *, struct l9p_request *);
	void (*readlink)(void *, struct l9p_request *);
	void (*getattr)(void *, struct l9p_request *);
	void (*setattr)(void *, struct l9p_request *);
	void (*xattrwalk)(void *, struct l9p_request *);
	void (*xattrcreate)(void *, struct l9p_request *);
	void (*readdir)(void *, struct l9p_request *);
	void (*fsync)(void *, struct l9p_request *);
	void (*lock)(void *, struct l9p_request *);
	void (*getlock)(void *, struct l9p_request *);
	void (*link)(void *, struct l9p_request *);
	void (*mkdir)(void *, struct l9p_request *);
	void (*renameat)(void *, struct l9p_request *);
	void (*unlinkat)(void *, struct l9p_request *);
};

int l9p_pufcall(struct l9p_message *msg, union l9p_fcall *fcall,
    enum l9p_version version);
ssize_t l9p_pustat(struct l9p_message *msg, struct l9p_stat *s,
    enum l9p_version version);
uint16_t l9p_sizeof_stat(struct l9p_stat *stat, enum l9p_version version);
int l9p_pack_stat(struct l9p_message *msg, struct l9p_request *req,
    struct l9p_stat *s);
ssize_t l9p_pudirent(struct l9p_message *msg, struct l9p_dirent *de);

int l9p_server_init(struct l9p_server **serverp, struct l9p_backend *backend);

int l9p_connection_init(struct l9p_server *server,
    struct l9p_connection **connp);
void l9p_connection_free(struct l9p_connection *conn);
void l9p_connection_on_send_response(struct l9p_connection *conn,
    l9p_send_response_t *cb, void *aux);
void l9p_connection_on_get_response_buffer(struct l9p_connection *conn,
    l9p_get_response_buffer_t *cb, void *aux);
void l9p_connection_recv(struct l9p_connection *conn, const struct iovec *iov,
    size_t niov, void *aux);
void l9p_connection_close(struct l9p_connection *conn);
struct l9p_openfile *l9p_connection_alloc_fid(struct l9p_connection *conn,
    uint32_t fid);
void l9p_connection_remove_fid(struct l9p_connection *conn,
    struct l9p_openfile *fid);

void l9p_dispatch_request(struct l9p_request *req);
void l9p_respond(struct l9p_request *req, int errnum);

void l9p_init_msg(struct l9p_message *msg, struct l9p_request *req,
    enum l9p_pack_mode mode);
void l9p_seek_iov(struct iovec *iov1, size_t niov1, struct iovec *iov2,
    size_t *niov2, size_t seek);
size_t l9p_truncate_iov(struct iovec *iov, size_t niov, size_t length);
void l9p_describe_fcall(union l9p_fcall *fcall, enum l9p_version version,
    struct sbuf *sb);
void l9p_freefcall(union l9p_fcall *fcall);
void l9p_freestat(struct l9p_stat *stat);

int l9p_backend_fs_init(struct l9p_backend **backendp, const char *root);

#endif  /* LIB9P_LIB9P_H */
