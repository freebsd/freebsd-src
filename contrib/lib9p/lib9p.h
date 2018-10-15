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

#include <stdbool.h>
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
#include "threadpool.h"
#include "hashtable.h"

#define L9P_DEFAULT_MSIZE   8192
#define L9P_MAX_IOV         128
#define	L9P_NUMTHREADS      8

struct l9p_request;

/*
 * Functions to implement underlying transport for lib9p.
 *
 * The transport is responsible for:
 *
 *   - allocating a response buffer (filling in the iovec and niov)
 *     (gets req, pointer to base of iov array of size L9P_MAX_IOV,
 *      pointer to niov, lt_aux)
 *
 *   - sending a response, when a request has a reply ready
 *     (gets req, pointer to iov, niov, actual response length, lt_aux)
 *
 *   - dropping the response buffer, when a request has been
 *     flushed or otherwise dropped without a response
 *     (gets req, pointer to iov, niov, lt_aux)
 *
 * The transport is of course also responsible for feeding in
 * request-buffers, but that happens by the transport calling
 * l9p_connection_recv().
 */
struct l9p_transport {
	void	*lt_aux;
	int	(*lt_get_response_buffer)(struct l9p_request *,
					  struct iovec *, size_t *, void *);
	int	(*lt_send_response)(struct l9p_request *,
				    const struct iovec *, size_t, size_t,
				    void *);
	void	(*lt_drop_response)(struct l9p_request *,
				    const struct iovec *, size_t, void *);
};

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

/*
 * This structure is used for unpacking (decoding) incoming
 * requests and packing (encoding) outgoing results.  It has its
 * own copy of the iov array, with its own counters for working
 * through that array, but it borrows the actual DATA from the
 * original iov array associated with the original request (see
 * below).
 */
struct l9p_message {
	enum l9p_pack_mode lm_mode;
	struct iovec lm_iov[L9P_MAX_IOV];
	size_t lm_niov;
	size_t lm_cursor_iov;
	size_t lm_cursor_offset;
	size_t lm_size;
};

struct l9p_fid;

/*
 * Data structure for a request/response pair (Tfoo/Rfoo).
 *
 * Note that the response is not formatted out into raw data
 * (overwriting the request raw data) until we are really
 * responding, with the exception of read operations Tread
 * and Treaddir, which overlay their result-data into the
 * iov array in the process of reading.
 *
 * We have room for two incoming fids, in case we are
 * using 9P2000.L protocol.  Note that nothing that uses two
 * fids also has an output fid (newfid), so we could have a
 * union of lr_fid2 and lr_newfid, but keeping them separate
 * is probably a bit less error-prone.  (If we want to shave
 * memory requirements there are more places to look.)
 *
 * (The fid, fid2, and newfid fields should be removed via
 * reorganization, as they are only used for smuggling data
 * between request.c and the backend and should just be
 * parameters to backend ops.)
 */
struct l9p_request {
	struct l9p_message lr_req_msg;	/* for unpacking the request */
	struct l9p_message lr_resp_msg;	/* for packing the response */
	union l9p_fcall lr_req;		/* the request, decoded/unpacked */
	union l9p_fcall lr_resp;	/* the response, not yet packed */

	struct l9p_fid *lr_fid;
	struct l9p_fid *lr_fid2;
	struct l9p_fid *lr_newfid;

	struct l9p_connection *lr_conn;	/* containing connection */
	void *lr_aux;			/* reserved for transport layer */

	struct iovec lr_data_iov[L9P_MAX_IOV];	/* iovecs for req + resp */
	size_t lr_data_niov;			/* actual size of data_iov */

	int	lr_error;		/* result from l9p_dispatch_request */

	/* proteced by threadpool mutex */
	enum l9p_workstate lr_workstate;	/* threadpool: work state */
	enum l9p_flushstate lr_flushstate;	/* flush state if flushee */
	struct l9p_worker *lr_worker;		/* threadpool: worker */
	STAILQ_ENTRY(l9p_request) lr_worklink;	/* reserved to threadpool */

	/* protected by tag hash table lock */
	struct l9p_request_queue lr_flushq;	/* q of flushers */
	STAILQ_ENTRY(l9p_request) lr_flushlink;	/* link w/in flush queue */
};

/* N.B.: these dirents are variable length and for .L only */
struct l9p_dirent {
	struct l9p_qid qid;
	uint64_t offset;
	uint8_t type;
	char *name;
};

/*
 * The 9pfs protocol has the notion of a "session", which is
 * traffic between any two "Tversion" requests.  All fids
 * (lc_files, below) are specific to one particular session.
 *
 * We need a data structure per connection (client/server
 * pair). This data structure lasts longer than these 9pfs
 * sessions, but contains the request/response pairs and fids.
 * Logically, the per-session data should be separate, but
 * most of the time that would just require an extra
 * indirection.  Instead, a new session simply clunks all
 * fids, and otherwise keeps using this same connection.
 */
struct l9p_connection {
	struct l9p_server *lc_server;
	struct l9p_transport lc_lt;
	struct l9p_threadpool lc_tp;
	enum l9p_version lc_version;
	uint32_t lc_msize;
	uint32_t lc_max_io_size;
	struct ht lc_files;
	struct ht lc_requests;
	LIST_ENTRY(l9p_connection) lc_link;
};

struct l9p_backend;
struct l9p_server {
	struct l9p_backend *ls_backend;
	enum l9p_version ls_max_version;
	LIST_HEAD(, l9p_connection) ls_conns;
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
void l9p_connection_recv(struct l9p_connection *conn, const struct iovec *iov,
    size_t niov, void *aux);
void l9p_connection_close(struct l9p_connection *conn);
struct l9p_fid *l9p_connection_alloc_fid(struct l9p_connection *conn,
    uint32_t fid);
void l9p_connection_remove_fid(struct l9p_connection *conn,
    struct l9p_fid *fid);

int l9p_dispatch_request(struct l9p_request *req);
void l9p_respond(struct l9p_request *req, bool drop, bool rmtag);

void l9p_init_msg(struct l9p_message *msg, struct l9p_request *req,
    enum l9p_pack_mode mode);
void l9p_seek_iov(struct iovec *iov1, size_t niov1, struct iovec *iov2,
    size_t *niov2, size_t seek);
size_t l9p_truncate_iov(struct iovec *iov, size_t niov, size_t length);
void l9p_describe_fcall(union l9p_fcall *fcall, enum l9p_version version,
    struct sbuf *sb);
void l9p_freefcall(union l9p_fcall *fcall);
void l9p_freestat(struct l9p_stat *stat);

gid_t *l9p_getgrlist(const char *, gid_t, int *);

#endif  /* LIB9P_LIB9P_H */
