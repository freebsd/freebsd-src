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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/uio.h>
#if defined(__FreeBSD__)
#include <sys/sbuf.h>
#else
#include "sbuf/sbuf.h"
#endif
#include "lib9p.h"
#include "lib9p_impl.h"
#include "fcall.h"
#include "hashtable.h"
#include "log.h"
#include "linux_errno.h"

#define N(x)    (sizeof(x) / sizeof(x[0]))

static void l9p_dispatch_tversion(struct l9p_request *req);
static void l9p_dispatch_tattach(struct l9p_request *req);
static void l9p_dispatch_tclunk(struct l9p_request *req);
static void l9p_dispatch_tflush(struct l9p_request *req);
static void l9p_dispatch_tcreate(struct l9p_request *req);
static void l9p_dispatch_topen(struct l9p_request *req);
static void l9p_dispatch_tread(struct l9p_request *req);
static void l9p_dispatch_tremove(struct l9p_request *req);
static void l9p_dispatch_tstat(struct l9p_request *req);
static void l9p_dispatch_twalk(struct l9p_request *req);
static void l9p_dispatch_twrite(struct l9p_request *req);
static void l9p_dispatch_twstat(struct l9p_request *req);
static void l9p_dispatch_tstatfs(struct l9p_request *req);
static void l9p_dispatch_tlopen(struct l9p_request *req);
static void l9p_dispatch_tlcreate(struct l9p_request *req);
static void l9p_dispatch_tsymlink(struct l9p_request *req);
static void l9p_dispatch_tmknod(struct l9p_request *req);
static void l9p_dispatch_trename(struct l9p_request *req);
static void l9p_dispatch_treadlink(struct l9p_request *req);
static void l9p_dispatch_tgetattr(struct l9p_request *req);
static void l9p_dispatch_tsetattr(struct l9p_request *req);
static void l9p_dispatch_txattrwalk(struct l9p_request *req);
static void l9p_dispatch_txattrcreate(struct l9p_request *req);
static void l9p_dispatch_treaddir(struct l9p_request *req);
static void l9p_dispatch_tfsync(struct l9p_request *req);
static void l9p_dispatch_tlock(struct l9p_request *req);
static void l9p_dispatch_tgetlock(struct l9p_request *req);
static void l9p_dispatch_tlink(struct l9p_request *req);
static void l9p_dispatch_tmkdir(struct l9p_request *req);
static void l9p_dispatch_trenameat(struct l9p_request *req);
static void l9p_dispatch_tunlinkat(struct l9p_request *req);

struct l9p_handler {
	enum l9p_ftype type;
	void (*handler)(struct l9p_request *);
};

static const struct l9p_handler l9p_handlers_no_version[] = {
	{L9P_TVERSION, l9p_dispatch_tversion},
};

static const struct l9p_handler l9p_handlers_base[] = {
	{L9P_TVERSION, l9p_dispatch_tversion},
	{L9P_TATTACH, l9p_dispatch_tattach},
	{L9P_TCLUNK, l9p_dispatch_tclunk},
	{L9P_TFLUSH, l9p_dispatch_tflush},
	{L9P_TCREATE, l9p_dispatch_tcreate},
	{L9P_TOPEN, l9p_dispatch_topen},
	{L9P_TREAD, l9p_dispatch_tread},
	{L9P_TWRITE, l9p_dispatch_twrite},
	{L9P_TREMOVE, l9p_dispatch_tremove},
	{L9P_TSTAT, l9p_dispatch_tstat},
	{L9P_TWALK, l9p_dispatch_twalk},
	{L9P_TWSTAT, l9p_dispatch_twstat}
};
static const struct l9p_handler l9p_handlers_dotu[] = {
	{L9P_TVERSION, l9p_dispatch_tversion},
	{L9P_TATTACH, l9p_dispatch_tattach},
	{L9P_TCLUNK, l9p_dispatch_tclunk},
	{L9P_TFLUSH, l9p_dispatch_tflush},
	{L9P_TCREATE, l9p_dispatch_tcreate},
	{L9P_TOPEN, l9p_dispatch_topen},
	{L9P_TREAD, l9p_dispatch_tread},
	{L9P_TWRITE, l9p_dispatch_twrite},
	{L9P_TREMOVE, l9p_dispatch_tremove},
	{L9P_TSTAT, l9p_dispatch_tstat},
	{L9P_TWALK, l9p_dispatch_twalk},
	{L9P_TWSTAT, l9p_dispatch_twstat}
};
static const struct l9p_handler l9p_handlers_dotL[] = {
	{L9P_TVERSION, l9p_dispatch_tversion},
	{L9P_TATTACH, l9p_dispatch_tattach},
	{L9P_TCLUNK, l9p_dispatch_tclunk},
	{L9P_TFLUSH, l9p_dispatch_tflush},
	{L9P_TCREATE, l9p_dispatch_tcreate},
	{L9P_TOPEN, l9p_dispatch_topen},
	{L9P_TREAD, l9p_dispatch_tread},
	{L9P_TWRITE, l9p_dispatch_twrite},
	{L9P_TREMOVE, l9p_dispatch_tremove},
	{L9P_TSTAT, l9p_dispatch_tstat},
	{L9P_TWALK, l9p_dispatch_twalk},
	{L9P_TWSTAT, l9p_dispatch_twstat},
	{L9P_TSTATFS, l9p_dispatch_tstatfs},
	{L9P_TLOPEN, l9p_dispatch_tlopen},
	{L9P_TLCREATE, l9p_dispatch_tlcreate},
	{L9P_TSYMLINK, l9p_dispatch_tsymlink},
	{L9P_TMKNOD, l9p_dispatch_tmknod},
	{L9P_TRENAME, l9p_dispatch_trename},
	{L9P_TREADLINK, l9p_dispatch_treadlink},
	{L9P_TGETATTR, l9p_dispatch_tgetattr},
	{L9P_TSETATTR, l9p_dispatch_tsetattr},
	{L9P_TXATTRWALK, l9p_dispatch_txattrwalk},
	{L9P_TXATTRCREATE, l9p_dispatch_txattrcreate},
	{L9P_TREADDIR, l9p_dispatch_treaddir},
	{L9P_TFSYNC, l9p_dispatch_tfsync},
	{L9P_TLOCK, l9p_dispatch_tlock},
	{L9P_TGETLOCK, l9p_dispatch_tgetlock},
	{L9P_TLINK, l9p_dispatch_tlink},
	{L9P_TMKDIR, l9p_dispatch_tmkdir},
	{L9P_TRENAMEAT, l9p_dispatch_trenameat},
	{L9P_TUNLINKAT, l9p_dispatch_tunlinkat},
};

/*
 * NB: version index 0 is reserved for new connections, and
 * is a protocol that handles only L9P_TVERSION.  Once we get a
 * valid version, we start a new session using its dispatch table.
 */
static const struct {
	const char *name;
	const struct l9p_handler *handlers;
	int n_handlers;
} l9p_versions[] = {
	{ "<none>", l9p_handlers_no_version, N(l9p_handlers_no_version) },
	{ "9P2000", l9p_handlers_base, N(l9p_handlers_base) },
	{ "9P2000.u", l9p_handlers_dotu, N(l9p_handlers_dotu), },
	{ "9P2000.L", l9p_handlers_dotL, N(l9p_handlers_dotL), },
};

void
l9p_dispatch_request(struct l9p_request *req)
{
#if defined(L9P_DEBUG)
	struct sbuf *sb;
#endif
	size_t i, n;
	const struct l9p_handler *handlers;

#if defined(L9P_DEBUG)
	sb = sbuf_new_auto();
	l9p_describe_fcall(&req->lr_req, req->lr_conn->lc_version, sb);
	sbuf_done(sb);

	L9P_LOG(L9P_DEBUG, "%s", sbuf_data(sb));
	sbuf_delete(sb);
#endif

	req->lr_tag = req->lr_req.hdr.tag;

	handlers = l9p_versions[req->lr_conn->lc_version].handlers;
	n = (size_t)l9p_versions[req->lr_conn->lc_version].n_handlers;
	for (i = 0; i < n; i++) {
		if (req->lr_req.hdr.type == handlers[i].type) {
			handlers[i].handler(req);
			return;
		}
	}

	L9P_LOG(L9P_WARNING, "unknown request of type %d", req->lr_req.hdr.type);
	l9p_respond(req, ENOSYS);
}

/*
 * Translate BSD errno to Linux errno.
 */
static inline int
to_linux(int errnum)
{
	static int const table[] = {
		[EDEADLK] = LINUX_EDEADLK,
		[EAGAIN] = LINUX_EAGAIN,
		[EINPROGRESS] = LINUX_EINPROGRESS,
		[EALREADY] = LINUX_EALREADY,
		[ENOTSOCK] = LINUX_ENOTSOCK,
		[EDESTADDRREQ] = LINUX_EDESTADDRREQ,
		[EMSGSIZE] = LINUX_EMSGSIZE,
		[EPROTOTYPE] = LINUX_EPROTOTYPE,
		[ENOPROTOOPT] = LINUX_ENOPROTOOPT,
		[EPROTONOSUPPORT] = LINUX_EPROTONOSUPPORT,
		[ESOCKTNOSUPPORT] = LINUX_ESOCKTNOSUPPORT,
		[EOPNOTSUPP] = LINUX_EOPNOTSUPP,
		[EPFNOSUPPORT] = LINUX_EPFNOSUPPORT,
		[EAFNOSUPPORT] = LINUX_EAFNOSUPPORT,
		[EADDRINUSE] = LINUX_EADDRINUSE,
		[EADDRNOTAVAIL] = LINUX_EADDRNOTAVAIL,
		[ENETDOWN] = LINUX_ENETDOWN,
		[ENETUNREACH] = LINUX_ENETUNREACH,
		[ENETRESET] = LINUX_ENETRESET,
		[ECONNABORTED] = LINUX_ECONNABORTED,
		[ECONNRESET] = LINUX_ECONNRESET,
		[ENOBUFS] = LINUX_ENOBUFS,
		[EISCONN] = LINUX_EISCONN,
		[ENOTCONN] = LINUX_ENOTCONN,
		[ESHUTDOWN] = LINUX_ESHUTDOWN,
		[ETOOMANYREFS] = LINUX_ETOOMANYREFS,
		[ETIMEDOUT] = LINUX_ETIMEDOUT,
		[ECONNREFUSED] = LINUX_ECONNREFUSED,
		[ELOOP] = LINUX_ELOOP,
		[ENAMETOOLONG] = LINUX_ENAMETOOLONG,
		[EHOSTDOWN] = LINUX_EHOSTDOWN,
		[EHOSTUNREACH] = LINUX_EHOSTUNREACH,
		[ENOTEMPTY] = LINUX_ENOTEMPTY,
		[EPROCLIM] = LINUX_EAGAIN,
		[EUSERS] = LINUX_EUSERS,
		[EDQUOT] = LINUX_EDQUOT,
		[ESTALE] = LINUX_ESTALE,
		[EREMOTE] = LINUX_EREMOTE,
		/* EBADRPC = unmappable? */
		/* ERPCMISMATCH = unmappable? */
		/* EPROGUNAVAIL = unmappable? */
		/* EPROGMISMATCH = unmappable? */
		/* EPROCUNAVAIL = unmappable? */
		[ENOLCK] = LINUX_ENOLCK,
		[ENOSYS] = LINUX_ENOSYS,
		/* EFTYPE = unmappable? */
		/* EAUTH = unmappable? */
		/* ENEEDAUTH = unmappable? */
		[EIDRM] = LINUX_EIDRM,
		[ENOMSG] = LINUX_ENOMSG,
		[EOVERFLOW] = LINUX_EOVERFLOW,
		[ECANCELED] = LINUX_ECANCELED,
		[EILSEQ] = LINUX_EILSEQ,
		/* EDOOFUS = unmappable? */
		[EBADMSG] = LINUX_EBADMSG,
		[EMULTIHOP] = LINUX_EMULTIHOP,
		[ENOLINK] = LINUX_ENOLINK,
		[EPROTO] = LINUX_EPROTO,
		/* ENOTCAPABLE = unmappable? */
		/* ECAPMODE = unmappable? */
#ifdef ENOTRECOVERABLE
		[ENOTRECOVERABLE] = LINUX_ENOTRECOVERABLE,
#endif
#ifdef EOWNERDEAD
		[EOWNERDEAD] = LINUX_EOWNERDEAD,
#endif
	};

	/*
	 * In case we want to return a raw Linux errno, allow negative
	 * values a la Linux kernel internals.
	 *
	 * Values up to ERANGE are shared across systems (see
	 * linux_errno.h), except for EAGAIN.
	 */
	if (errnum < 0)
		return (-errnum);
	if ((size_t)errnum < N(table) && table[errnum] != 0)
		return (table[errnum]);
	if (errnum <= ERANGE)
		return (errnum);
	return (LINUX_ENOTRECOVERABLE);	/* ??? */
}

void
l9p_respond(struct l9p_request *req, int errnum)
{
	struct l9p_connection *conn = req->lr_conn;
	size_t iosize;
#if defined(L9P_DEBUG)
	struct sbuf *sb;
#endif

	switch (req->lr_req.hdr.type) {

	case L9P_TATTACH:
		if (errnum != 0)
			l9p_connection_remove_fid(conn, req->lr_fid);

		break;

	case L9P_TCLUNK:
	case L9P_TREMOVE:
		if (req->lr_fid != NULL)
			l9p_connection_remove_fid(conn, req->lr_fid);
		break;

	case L9P_TWALK:
	case L9P_TXATTRWALK:
		if (errnum != 0 && req->lr_newfid != NULL &&
		    req->lr_newfid != req->lr_fid)
			l9p_connection_remove_fid(conn, req->lr_newfid);
		break;
	}

	req->lr_resp.hdr.tag = req->lr_req.hdr.tag;

	if (errnum == 0)
		req->lr_resp.hdr.type = req->lr_req.hdr.type + 1;
	else {
		if (conn->lc_version == L9P_2000L) {
			req->lr_resp.hdr.type = L9P_RLERROR;
			req->lr_resp.error.errnum = (uint32_t)to_linux(errnum);
		} else {
			req->lr_resp.hdr.type = L9P_RERROR;
			req->lr_resp.error.ename = strerror(errnum);
			req->lr_resp.error.errnum = (uint32_t)errnum;
		}
	}

#if defined(L9P_DEBUG)
	sb = sbuf_new_auto();
	l9p_describe_fcall(&req->lr_resp, conn->lc_version, sb);
	sbuf_done(sb);

	L9P_LOG(L9P_DEBUG, "%s", sbuf_data(sb));
	sbuf_delete(sb);
#endif

	if (l9p_pufcall(&req->lr_resp_msg, &req->lr_resp, conn->lc_version) != 0) {
		L9P_LOG(L9P_ERROR, "cannot pack response");
		goto out;
	}

	iosize = req->lr_resp_msg.lm_size;

	/* Include I/O size in calculation for Rread and Rreaddir responses */
	if (req->lr_resp.hdr.type == L9P_RREAD ||
	    req->lr_resp.hdr.type == L9P_RREADDIR)
		iosize += req->lr_resp.io.count;

	conn->lc_send_response(req, req->lr_resp_msg.lm_iov,
	    req->lr_resp_msg.lm_niov, iosize, conn->lc_send_response_aux);

out:
	l9p_freefcall(&req->lr_req);
	l9p_freefcall(&req->lr_resp);
	free(req);
}

/*
 * This allows a caller to iterate through the data in a
 * read or write request (creating the data if packing,
 * scanning through it if unpacking).  This is used for
 * writing readdir entries, so mode should be L9P_PACK
 * (but we allow L9P_UNPACK so that debug code can also scan
 * through the data later, if desired).
 *
 * This relies on the Tread op having positioned the request's
 * iov to the beginning of the data buffer (note the l9p_seek_iov
 * in l9p_dispatch_tread).
 */
void
l9p_init_msg(struct l9p_message *msg, struct l9p_request *req,
    enum l9p_pack_mode mode)
{

	msg->lm_size = 0;
	msg->lm_mode = mode;
	msg->lm_cursor_iov = 0;
	msg->lm_cursor_offset = 0;
	msg->lm_niov = req->lr_data_niov;
	memcpy(msg->lm_iov, req->lr_data_iov,
	    sizeof (struct iovec) * req->lr_data_niov);
}

/*
 * Generic handler for operations that require valid fid.
 *
 * Decodes header fid to file, then calls backend function
 * (*be)(softc, req).  Returns EBADF or ENOSYS if the fid is
 * invalid or the backend function is not there.
 */
static inline void l9p_fid_dispatch(struct l9p_request *req,
    void (*be)(void *, struct l9p_request *))
{
	struct l9p_connection *conn = req->lr_conn;

	req->lr_fid = ht_find(&conn->lc_files, req->lr_req.hdr.fid);
	if (req->lr_fid == NULL) {
		l9p_respond(req, EIO);
		return;
	}

	if (be == NULL) {
		l9p_respond(req, ENOSYS);
		return;
	}

	(*be)(conn->lc_server->ls_backend->softc, req);
}

/*
 * Generic handler for operations that need two fid's.
 * Note that the 2nd fid must be supplied by the caller; the
 * corresponding openfile goes into req->lr_f2.
 */
static inline void l9p_2fid_dispatch(struct l9p_request *req, uint32_t fid2,
    void (*be)(void *, struct l9p_request *))
{
	struct l9p_connection *conn = req->lr_conn;

	req->lr_fid = ht_find(&conn->lc_files, req->lr_req.hdr.fid);
	if (req->lr_fid == NULL) {
		l9p_respond(req, EBADF);
		return;
	}

	req->lr_fid2 = ht_find(&conn->lc_files, fid2);
	if (req->lr_fid2 == NULL) {
		l9p_respond(req, EBADF);
		return;
	}

	if (be == NULL) {
		l9p_respond(req, ENOSYS);
		return;
	}

	(*be)(conn->lc_server->ls_backend->softc, req);
}

/*
 * Generic handler for read-like operations (read and readdir).
 *
 * Backend function must exist.
 */
static inline void l9p_read_dispatch(struct l9p_request *req,
    void (*be)(void *, struct l9p_request *))
{
	struct l9p_connection *conn = req->lr_conn;

	req->lr_fid = ht_find(&conn->lc_files, req->lr_req.hdr.fid);
	if (!req->lr_fid) {
		l9p_respond(req, EBADF);
		return;
	}

	/*
	 * Adjust so that writing messages (packing data) starts
	 * right after the count field in the response.
	 *
	 * size[4] + Rread(dir)[1] + tag[2] + count[4] = 11
	 */
	l9p_seek_iov(req->lr_resp_msg.lm_iov, req->lr_resp_msg.lm_niov,
	    req->lr_data_iov, &req->lr_data_niov, 11);

	(*be)(conn->lc_server->ls_backend->softc, req);
}

/*
 * Append variable-size stat object and adjust io count.
 * Returns 0 if the entire stat object was packed, -1 if not.
 * A fully packed object updates the request's io count.
 *
 * Caller must use their own private l9p_message object since
 * a partially packed object will leave the message object in
 * a useless state.
 *
 * Frees the stat object.
 */
int
l9p_pack_stat(struct l9p_message *msg, struct l9p_request *req,
    struct l9p_stat *st)
{
	struct l9p_connection *conn = req->lr_conn;
	uint16_t size = l9p_sizeof_stat(st, conn->lc_version);
	int ret = 0;

	assert(msg->lm_mode == L9P_PACK);

	if (req->lr_resp.io.count + size > req->lr_req.io.count ||
	    l9p_pustat(msg, st, conn->lc_version) < 0)
		ret = -1;
	else
		req->lr_resp.io.count += size;
	l9p_freestat(st);
	return (ret);
}

static void
l9p_dispatch_tversion(struct l9p_request *req)
{
	struct l9p_connection *conn = req->lr_conn;
	struct l9p_server *server = conn->lc_server;
	enum l9p_version remote_version = L9P_INVALID_VERSION;
	size_t i;
	const char *remote_version_name;

	for (i = 0; i < N(l9p_versions); i++) {
		if (strcmp(req->lr_req.version.version,
		    l9p_versions[i].name) == 0) {
			remote_version = (enum l9p_version)i;
			break;
		}
	}

	if (remote_version == L9P_INVALID_VERSION) {
		L9P_LOG(L9P_ERROR, "unsupported remote version: %s",
		    req->lr_req.version.version);
		l9p_respond(req, ENOSYS);
		return;
	}

	remote_version_name = l9p_versions[remote_version].name;
	L9P_LOG(L9P_INFO, "remote version: %s", remote_version_name);
	L9P_LOG(L9P_INFO, "local version: %s",
	    l9p_versions[server->ls_max_version].name);

	conn->lc_version = MIN(remote_version, server->ls_max_version);
	conn->lc_msize = MIN(req->lr_req.version.msize, conn->lc_msize);
	conn->lc_max_io_size = conn->lc_msize - 24;
	req->lr_resp.version.version = strdup(remote_version_name);
	req->lr_resp.version.msize = conn->lc_msize;
	l9p_respond(req, 0);
}

static void
l9p_dispatch_tattach(struct l9p_request *req)
{
	struct l9p_connection *conn = req->lr_conn;

	req->lr_fid = l9p_connection_alloc_fid(conn, req->lr_req.hdr.fid);
	if (req->lr_fid == NULL)
		req->lr_fid = ht_find(&conn->lc_files, req->lr_req.hdr.fid);

	conn->lc_server->ls_backend->attach(conn->lc_server->ls_backend->softc, req);
}

static void
l9p_dispatch_tclunk(struct l9p_request *req)
{

	/* clunk is not optional but we can still use the generic dispatch */
	l9p_fid_dispatch(req, req->lr_conn->lc_server->ls_backend->clunk);
}

static void
l9p_dispatch_tflush(struct l9p_request *req)
{
	struct l9p_connection *conn = req->lr_conn;

	if (!conn->lc_server->ls_backend->flush) {
		l9p_respond(req, ENOSYS);
		return;
	}

	conn->lc_server->ls_backend->flush(conn->lc_server->ls_backend->softc, req);
}

static void
l9p_dispatch_tcreate(struct l9p_request *req)
{

	l9p_fid_dispatch(req, req->lr_conn->lc_server->ls_backend->create);
}

static void
l9p_dispatch_topen(struct l9p_request *req)
{

	l9p_fid_dispatch(req, req->lr_conn->lc_server->ls_backend->open);
}

static void
l9p_dispatch_tread(struct l9p_request *req)
{

	l9p_read_dispatch(req, req->lr_conn->lc_server->ls_backend->read);
}

static void
l9p_dispatch_tremove(struct l9p_request *req)
{

	l9p_fid_dispatch(req, req->lr_conn->lc_server->ls_backend->remove);
}

static void
l9p_dispatch_tstat(struct l9p_request *req)
{

	l9p_fid_dispatch(req, req->lr_conn->lc_server->ls_backend->stat);
}

static void
l9p_dispatch_twalk(struct l9p_request *req)
{
	struct l9p_connection *conn = req->lr_conn;

	req->lr_fid = ht_find(&conn->lc_files, req->lr_req.hdr.fid);
	if (req->lr_fid == NULL) {
		l9p_respond(req, EBADF);
		return;
	}

	if (req->lr_req.twalk.hdr.fid != req->lr_req.twalk.newfid) {
		req->lr_newfid = l9p_connection_alloc_fid(conn,
		    req->lr_req.twalk.newfid);
		if (req->lr_newfid == NULL) {
			l9p_respond(req, EBADF);
			return;
		}
	}

	if (!conn->lc_server->ls_backend->walk) {
		l9p_respond(req, ENOSYS);
		return;
	}

	conn->lc_server->ls_backend->walk(conn->lc_server->ls_backend->softc, req);
}

static void
l9p_dispatch_twrite(struct l9p_request *req)
{
	struct l9p_connection *conn = req->lr_conn;

	req->lr_fid = ht_find(&conn->lc_files, req->lr_req.twalk.hdr.fid);
	if (req->lr_fid == NULL) {
		l9p_respond(req, EBADF);
		return;
	}

	if (!conn->lc_server->ls_backend->write) {
		l9p_respond(req, ENOSYS);
		return;
	}

	/*
	 * Adjust to point to the data to be written (a la
	 * l9p_dispatch_tread, but we're pointing into the request
	 * buffer rather than the response):
	 *
	 * size[4] + Twrite[1] + tag[2] + fid[4] + offset[8] count[4] = 23
	 */
	l9p_seek_iov(req->lr_req_msg.lm_iov, req->lr_req_msg.lm_niov,
	    req->lr_data_iov, &req->lr_data_niov, 23);

	conn->lc_server->ls_backend->write(conn->lc_server->ls_backend->softc, req);
}

static void
l9p_dispatch_twstat(struct l9p_request *req)
{

	l9p_fid_dispatch(req, req->lr_conn->lc_server->ls_backend->wstat);
}

static void
l9p_dispatch_tstatfs(struct l9p_request *req)
{

	l9p_fid_dispatch(req, req->lr_conn->lc_server->ls_backend->statfs);
}

static void
l9p_dispatch_tlopen(struct l9p_request *req)
{

	l9p_fid_dispatch(req, req->lr_conn->lc_server->ls_backend->lopen);
}

static void
l9p_dispatch_tlcreate(struct l9p_request *req)
{

	l9p_fid_dispatch(req, req->lr_conn->lc_server->ls_backend->lcreate);
}

static void
l9p_dispatch_tsymlink(struct l9p_request *req)
{

	l9p_fid_dispatch(req, req->lr_conn->lc_server->ls_backend->symlink);
}

static void
l9p_dispatch_tmknod(struct l9p_request *req)
{

	l9p_fid_dispatch(req, req->lr_conn->lc_server->ls_backend->mknod);
}

static void
l9p_dispatch_trename(struct l9p_request *req)
{

	l9p_2fid_dispatch(req, req->lr_req.trename.dfid,
	    req->lr_conn->lc_server->ls_backend->mknod);
}

static void
l9p_dispatch_treadlink(struct l9p_request *req)
{

	l9p_fid_dispatch(req, req->lr_conn->lc_server->ls_backend->readlink);
}

static void
l9p_dispatch_tgetattr(struct l9p_request *req)
{

	l9p_fid_dispatch(req, req->lr_conn->lc_server->ls_backend->getattr);
}

static void
l9p_dispatch_tsetattr(struct l9p_request *req)
{

	l9p_fid_dispatch(req, req->lr_conn->lc_server->ls_backend->setattr);
}

static void
l9p_dispatch_txattrwalk(struct l9p_request *req)
{
	struct l9p_connection *conn = req->lr_conn;

	req->lr_fid = ht_find(&conn->lc_files, req->lr_req.hdr.fid);
	if (req->lr_fid == NULL) {
		l9p_respond(req, EBADF);
		return;
	}

	/*
	 * XXX
	 *
	 * We need a way to mark a fid as used-for-xattr.  (Or
	 * maybe these xattr fids should be completely separate
	 * from file/directory fids?)
	 *
	 * For now, this relies on the backend always failing the
	 * operation.
	 */
	if (req->lr_req.twalk.hdr.fid != req->lr_req.twalk.newfid) {
		req->lr_newfid = l9p_connection_alloc_fid(conn,
		    req->lr_req.twalk.newfid);
		if (req->lr_newfid == NULL) {
			l9p_respond(req, EBADF);
			return;
		}
	}

	if (!conn->lc_server->ls_backend->xattrwalk) {
		l9p_respond(req, ENOSYS);
		return;
	}

	conn->lc_server->ls_backend->xattrwalk(
	    conn->lc_server->ls_backend->softc, req);
}

static void
l9p_dispatch_txattrcreate(struct l9p_request *req)
{

	l9p_fid_dispatch(req, req->lr_conn->lc_server->ls_backend->xattrcreate);
}

static void
l9p_dispatch_treaddir(struct l9p_request *req)
{

	l9p_read_dispatch(req, req->lr_conn->lc_server->ls_backend->readdir);
}

static void
l9p_dispatch_tfsync(struct l9p_request *req)
{

	l9p_fid_dispatch(req, req->lr_conn->lc_server->ls_backend->fsync);
}

static void
l9p_dispatch_tlock(struct l9p_request *req)
{

	l9p_fid_dispatch(req, req->lr_conn->lc_server->ls_backend->lock);
}

static void
l9p_dispatch_tgetlock(struct l9p_request *req)
{

	l9p_fid_dispatch(req, req->lr_conn->lc_server->ls_backend->getlock);
}

static void
l9p_dispatch_tlink(struct l9p_request *req)
{

	l9p_2fid_dispatch(req, req->lr_req.tlink.dfid,
	    req->lr_conn->lc_server->ls_backend->link);
}

static void
l9p_dispatch_tmkdir(struct l9p_request *req)
{

	l9p_fid_dispatch(req, req->lr_conn->lc_server->ls_backend->mkdir);
}

static void
l9p_dispatch_trenameat(struct l9p_request *req)
{

	l9p_2fid_dispatch(req, req->lr_req.trenameat.newdirfid,
	    req->lr_conn->lc_server->ls_backend->renameat);
}

static void
l9p_dispatch_tunlinkat(struct l9p_request *req)
{

	l9p_fid_dispatch(req, req->lr_conn->lc_server->ls_backend->unlinkat);
}
