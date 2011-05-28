#if !defined(lint) && !defined(SABER)
static const char rcsid[] = "$Id: ctl_srvr.c,v 1.6.18.3 2008-02-18 04:04:06 marka Exp $";
#endif /* not lint */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1998,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* Extern. */

#include "port_before.h"

#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif

#include <isc/assertions.h>
#include <isc/ctl.h>
#include <isc/eventlib.h>
#include <isc/list.h>
#include <isc/logging.h>
#include <isc/memcluster.h>

#include "ctl_p.h"

#include "port_after.h"

#ifdef SPRINTF_CHAR
# define SPRINTF(x) strlen(sprintf/**/x)
#else
# define SPRINTF(x) ((size_t)sprintf x)
#endif

/* Macros. */

#define	lastverb_p(verb)	(verb->name == NULL || verb->func == NULL)
#define	address_expr		ctl_sa_ntop((struct sockaddr *)&sess->sa, \
					    tmp, sizeof tmp, ctx->logger)

/* Types. */

enum state {
	available = 0, initializing, writing, reading, reading_data,
	processing, idling, quitting, closing
};

union sa_un {
	struct sockaddr_in in;
#ifndef NO_SOCKADDR_UN
	struct sockaddr_un un;
#endif
};

struct ctl_sess {
	LINK(struct ctl_sess)	link;
	struct ctl_sctx *	ctx;
	enum state		state;
	int			sock;
	union sa_un		sa;
	evFileID		rdID;
	evStreamID		wrID;
	evTimerID		rdtiID;
	evTimerID		wrtiID;
	struct ctl_buf		inbuf;
	struct ctl_buf		outbuf;
	const struct ctl_verb *	verb;
	u_int			helpcode;
	const void *		respctx;
	u_int			respflags;
	ctl_srvrdone		donefunc;
	void *			uap;
	void *			csctx;
};

struct ctl_sctx {
	evContext		ev;
	void *			uctx;
	u_int			unkncode;
	u_int			timeoutcode;
	const struct ctl_verb *	verbs;
	const struct ctl_verb *	connverb;
	int			sock;
	int			max_sess;
	int			cur_sess;
	struct timespec		timeout;
	ctl_logfunc		logger;
	evConnID		acID;
	LIST(struct ctl_sess)	sess;
};

/* Forward. */

static void			ctl_accept(evContext, void *, int,
					   const void *, int,
					   const void *, int);
static void			ctl_close(struct ctl_sess *);
static void			ctl_new_state(struct ctl_sess *,
					      enum state,
					      const char *);
static void			ctl_start_read(struct ctl_sess *);
static void			ctl_stop_read(struct ctl_sess *);
static void			ctl_readable(evContext, void *, int, int);
static void			ctl_rdtimeout(evContext, void *,
					      struct timespec,
					      struct timespec);
static void			ctl_wrtimeout(evContext, void *,
					      struct timespec,
					      struct timespec);
static void			ctl_docommand(struct ctl_sess *);
static void			ctl_writedone(evContext, void *, int, int);
static void			ctl_morehelp(struct ctl_sctx *,
					     struct ctl_sess *,
					     const struct ctl_verb *,
					     const char *,
					     u_int, const void *, void *);
static void			ctl_signal_done(struct ctl_sctx *,
						struct ctl_sess *);

/* Private data. */

static const char *		state_names[] = {
	"available", "initializing", "writing", "reading",
	"reading_data", "processing", "idling", "quitting", "closing"
};

static const char		space[] = " ";

static const struct ctl_verb	fakehelpverb = {
	"fakehelp", ctl_morehelp , NULL
};

/* Public. */

/*%
 * void
 * ctl_server()
 *	create, condition, and start a listener on the control port.
 */
struct ctl_sctx *
ctl_server(evContext lev, const struct sockaddr *sap, size_t sap_len,
	   const struct ctl_verb *verbs,
	   u_int unkncode, u_int timeoutcode,
	   u_int timeout, int backlog, int max_sess,
	   ctl_logfunc logger, void *uctx)
{
	static const char me[] = "ctl_server";
	static const int on = 1;
	const struct ctl_verb *connverb;
	struct ctl_sctx *ctx;
	int save_errno;

	if (logger == NULL)
		logger = ctl_logger;
	for (connverb = verbs;
	     connverb->name != NULL && connverb->func != NULL;
	     connverb++)
		if (connverb->name[0] == '\0')
			break;
	if (connverb->func == NULL) {
		(*logger)(ctl_error, "%s: no connection verb found", me);
		return (NULL);
	}
	ctx = memget(sizeof *ctx);
	if (ctx == NULL) {
		(*logger)(ctl_error, "%s: getmem: %s", me, strerror(errno));
		return (NULL);
	}
	ctx->ev = lev;
	ctx->uctx = uctx;
	ctx->unkncode = unkncode;
	ctx->timeoutcode = timeoutcode;
	ctx->verbs = verbs;
	ctx->timeout = evConsTime(timeout, 0);
	ctx->logger = logger;
	ctx->connverb = connverb;
	ctx->max_sess = max_sess;
	ctx->cur_sess = 0;
	INIT_LIST(ctx->sess);
	ctx->sock = socket(sap->sa_family, SOCK_STREAM, PF_UNSPEC);
	if (ctx->sock > evHighestFD(ctx->ev)) {
		ctx->sock = -1;
		errno = ENOTSOCK;
	}
	if (ctx->sock < 0) {
		save_errno = errno;
		(*ctx->logger)(ctl_error, "%s: socket: %s",
			       me, strerror(errno));
		memput(ctx, sizeof *ctx);
		errno = save_errno;
		return (NULL);
	}
	if (ctx->sock > evHighestFD(lev)) {
		close(ctx->sock);
		(*ctx->logger)(ctl_error, "%s: file descriptor > evHighestFD");
		errno = ENFILE;
		memput(ctx, sizeof *ctx);
		return (NULL);
	}
#ifdef NO_UNIX_REUSEADDR
	if (sap->sa_family != AF_UNIX)
#endif
		if (setsockopt(ctx->sock, SOL_SOCKET, SO_REUSEADDR,
			       (const char *)&on, sizeof on) != 0) {
			(*ctx->logger)(ctl_warning,
				       "%s: setsockopt(REUSEADDR): %s",
				       me, strerror(errno));
		}
	if (bind(ctx->sock, sap, sap_len) < 0) {
		char tmp[MAX_NTOP];
		save_errno = errno;
		(*ctx->logger)(ctl_error, "%s: bind: %s: %s",
			       me, ctl_sa_ntop((const struct sockaddr *)sap,
			       tmp, sizeof tmp, ctx->logger),
			       strerror(save_errno));
		close(ctx->sock);
		memput(ctx, sizeof *ctx);
		errno = save_errno;
		return (NULL);
	}
	if (fcntl(ctx->sock, F_SETFD, 1) < 0) {
		(*ctx->logger)(ctl_warning, "%s: fcntl: %s", me,
			       strerror(errno));
	}
	if (evListen(lev, ctx->sock, backlog, ctl_accept, ctx,
		     &ctx->acID) < 0) {
		save_errno = errno;
		(*ctx->logger)(ctl_error, "%s: evListen(fd %d): %s",
			       me, ctx->sock, strerror(errno));
		close(ctx->sock);
		memput(ctx, sizeof *ctx);
		errno = save_errno;
		return (NULL);
	}
	(*ctx->logger)(ctl_debug, "%s: new ctx %p, sock %d",
		       me, ctx, ctx->sock);
	return (ctx);
}

/*%
 * void
 * ctl_endserver(ctx)
 *	if the control listener is open, close it.  clean out all eventlib
 *	stuff.  close all active sessions.
 */
void
ctl_endserver(struct ctl_sctx *ctx) {
	static const char me[] = "ctl_endserver";
	struct ctl_sess *this, *next;

	(*ctx->logger)(ctl_debug, "%s: ctx %p, sock %d, acID %p, sess %p",
		       me, ctx, ctx->sock, ctx->acID.opaque, ctx->sess);
	if (ctx->acID.opaque != NULL) {
		(void)evCancelConn(ctx->ev, ctx->acID);
		ctx->acID.opaque = NULL;
	}
	if (ctx->sock != -1) {
		(void) close(ctx->sock);
		ctx->sock = -1;
	}
	for (this = HEAD(ctx->sess); this != NULL; this = next) {
		next = NEXT(this, link);
		ctl_close(this);
	}
	memput(ctx, sizeof *ctx);
}

/*%
 * If body is non-NULL then it we add a "." line after it.
 * Caller must have  escaped lines with leading ".".
 */
void
ctl_response(struct ctl_sess *sess, u_int code, const char *text,
	     u_int flags, const void *respctx, ctl_srvrdone donefunc,
	     void *uap, const char *body, size_t bodylen)
{
	static const char me[] = "ctl_response";
	struct iovec iov[3], *iovp = iov;
	struct ctl_sctx *ctx = sess->ctx;
	char tmp[MAX_NTOP], *pc;
	int n;

	REQUIRE(sess->state == initializing ||
		sess->state == processing ||
		sess->state == reading_data ||
		sess->state == writing);
	REQUIRE(sess->wrtiID.opaque == NULL);
	REQUIRE(sess->wrID.opaque == NULL);
	ctl_new_state(sess, writing, me);
	sess->donefunc = donefunc;
	sess->uap = uap;
	if (!allocated_p(sess->outbuf) &&
	    ctl_bufget(&sess->outbuf, ctx->logger) < 0) {
		(*ctx->logger)(ctl_error, "%s: %s: cant get an output buffer",
			       me, address_expr);
		goto untimely;
	}
	if (sizeof "000-\r\n" + strlen(text) > (size_t)MAX_LINELEN) {
		(*ctx->logger)(ctl_error, "%s: %s: output buffer ovf, closing",
			       me, address_expr);
		goto untimely;
	}
	sess->outbuf.used = SPRINTF((sess->outbuf.text, "%03d%c%s\r\n",
				     code, (flags & CTL_MORE) != 0 ? '-' : ' ',
				     text));
	for (pc = sess->outbuf.text, n = 0;
	     n < (int)sess->outbuf.used-2; pc++, n++)
		if (!isascii((unsigned char)*pc) ||
		    !isprint((unsigned char)*pc))
			*pc = '\040';
	*iovp++ = evConsIovec(sess->outbuf.text, sess->outbuf.used);
	if (body != NULL) {
		char *tmp;
		DE_CONST(body, tmp);
		*iovp++ = evConsIovec(tmp, bodylen);
		DE_CONST(".\r\n", tmp);
		*iovp++ = evConsIovec(tmp, 3);
	}
	(*ctx->logger)(ctl_debug, "%s: [%d] %s", me,
		       sess->outbuf.used, sess->outbuf.text);
	if (evWrite(ctx->ev, sess->sock, iov, iovp - iov,
		    ctl_writedone, sess, &sess->wrID) < 0) {
		(*ctx->logger)(ctl_error, "%s: %s: evWrite: %s", me,
			       address_expr, strerror(errno));
		goto untimely;
	}
	if (evSetIdleTimer(ctx->ev, ctl_wrtimeout, sess, ctx->timeout,
			   &sess->wrtiID) < 0)
	{
		(*ctx->logger)(ctl_error, "%s: %s: evSetIdleTimer: %s", me,
			       address_expr, strerror(errno));
		goto untimely;
	}
	if (evTimeRW(ctx->ev, sess->wrID, sess->wrtiID) < 0) {
		(*ctx->logger)(ctl_error, "%s: %s: evTimeRW: %s", me,
			       address_expr, strerror(errno));
 untimely:
		ctl_signal_done(ctx, sess);
		ctl_close(sess);
		return;
	}
	sess->respctx = respctx;
	sess->respflags = flags;
}

void
ctl_sendhelp(struct ctl_sess *sess, u_int code) {
	static const char me[] = "ctl_sendhelp";
	struct ctl_sctx *ctx = sess->ctx;

	sess->helpcode = code;
	sess->verb = &fakehelpverb;
	ctl_morehelp(ctx, sess, NULL, me, CTL_MORE,
		     (const void *)ctx->verbs, NULL);
}

void *
ctl_getcsctx(struct ctl_sess *sess) {
	return (sess->csctx);
}

void *
ctl_setcsctx(struct ctl_sess *sess, void *csctx) {
	void *old = sess->csctx;

	sess->csctx = csctx;
	return (old);
}

/* Private functions. */

static void
ctl_accept(evContext lev, void *uap, int fd,
	   const void *lav, int lalen,
	   const void *rav, int ralen)
{
	static const char me[] = "ctl_accept";
	struct ctl_sctx *ctx = uap;
	struct ctl_sess *sess = NULL;
	char tmp[MAX_NTOP];

	UNUSED(lev);
	UNUSED(lalen);
	UNUSED(ralen);

	if (fd < 0) {
		(*ctx->logger)(ctl_error, "%s: accept: %s",
			       me, strerror(errno));
		return;
	}
	if (ctx->cur_sess == ctx->max_sess) {
		(*ctx->logger)(ctl_error, "%s: %s: too many control sessions",
			       me, ctl_sa_ntop((const struct sockaddr *)rav,
					       tmp, sizeof tmp,
					       ctx->logger));
		(void) close(fd);
		return;
	}
	sess = memget(sizeof *sess);
	if (sess == NULL) {
		(*ctx->logger)(ctl_error, "%s: memget: %s", me,
			       strerror(errno));
		(void) close(fd);
		return;
	}
	if (fcntl(fd, F_SETFD, 1) < 0) {
		(*ctx->logger)(ctl_warning, "%s: fcntl: %s", me,
			       strerror(errno));
	}
	ctx->cur_sess++;
	INIT_LINK(sess, link);
	APPEND(ctx->sess, sess, link);
	sess->ctx = ctx;
	sess->sock = fd;
	sess->wrID.opaque = NULL;
	sess->rdID.opaque = NULL;
	sess->wrtiID.opaque = NULL;
	sess->rdtiID.opaque = NULL;
	sess->respctx = NULL;
	sess->csctx = NULL;
	if (((const struct sockaddr *)rav)->sa_family == AF_UNIX)
		ctl_sa_copy((const struct sockaddr *)lav,
			    (struct sockaddr *)&sess->sa);
	else
		ctl_sa_copy((const struct sockaddr *)rav,
			    (struct sockaddr *)&sess->sa);
	sess->donefunc = NULL;
	buffer_init(sess->inbuf);
	buffer_init(sess->outbuf);
	sess->state = available;
	ctl_new_state(sess, initializing, me);
	sess->verb = ctx->connverb;
	(*ctx->logger)(ctl_debug, "%s: %s: accepting (fd %d)",
		       me, address_expr, sess->sock);
	(*ctx->connverb->func)(ctx, sess, ctx->connverb, "", 0,
			       (const struct sockaddr *)rav, ctx->uctx);
}

static void
ctl_new_state(struct ctl_sess *sess, enum state new_state, const char *reason)
{
	static const char me[] = "ctl_new_state";
	struct ctl_sctx *ctx = sess->ctx;
	char tmp[MAX_NTOP];

	(*ctx->logger)(ctl_debug, "%s: %s: %s -> %s (%s)",
		       me, address_expr,
		       state_names[sess->state],
		       state_names[new_state], reason);
	sess->state = new_state;
}

static void
ctl_close(struct ctl_sess *sess) {
	static const char me[] = "ctl_close";
	struct ctl_sctx *ctx = sess->ctx;
	char tmp[MAX_NTOP];

	REQUIRE(sess->state == initializing ||
		sess->state == writing ||
		sess->state == reading ||
		sess->state == processing ||
		sess->state == reading_data ||
		sess->state == idling);
	REQUIRE(sess->sock != -1);
	if (sess->state == reading || sess->state == reading_data)
		ctl_stop_read(sess);
	else if (sess->state == writing) {
		if (sess->wrID.opaque != NULL) {
			(void) evCancelRW(ctx->ev, sess->wrID);
			sess->wrID.opaque = NULL;
		}
		if (sess->wrtiID.opaque != NULL) {
			(void) evClearIdleTimer(ctx->ev, sess->wrtiID);
			sess->wrtiID.opaque = NULL;
		}
	}
	ctl_new_state(sess, closing, me);
	(void) close(sess->sock);
	if (allocated_p(sess->inbuf))
		ctl_bufput(&sess->inbuf);
	if (allocated_p(sess->outbuf))
		ctl_bufput(&sess->outbuf);
	(*ctx->logger)(ctl_debug, "%s: %s: closed (fd %d)",
		       me, address_expr, sess->sock);
	UNLINK(ctx->sess, sess, link);
	memput(sess, sizeof *sess);
	ctx->cur_sess--;
}

static void
ctl_start_read(struct ctl_sess *sess) {
	static const char me[] = "ctl_start_read";
	struct ctl_sctx *ctx = sess->ctx;
	char tmp[MAX_NTOP];

	REQUIRE(sess->state == initializing ||
		sess->state == writing ||
		sess->state == processing ||
		sess->state == idling);
	REQUIRE(sess->rdtiID.opaque == NULL);
	REQUIRE(sess->rdID.opaque == NULL);
	sess->inbuf.used = 0;
	if (evSetIdleTimer(ctx->ev, ctl_rdtimeout, sess, ctx->timeout,
			   &sess->rdtiID) < 0)
	{
		(*ctx->logger)(ctl_error, "%s: %s: evSetIdleTimer: %s", me,
			       address_expr, strerror(errno));
		ctl_close(sess);
		return;
	}
	if (evSelectFD(ctx->ev, sess->sock, EV_READ,
		       ctl_readable, sess, &sess->rdID) < 0) {
		(*ctx->logger)(ctl_error, "%s: %s: evSelectFD: %s", me,
			       address_expr, strerror(errno));
		return;
	}
	ctl_new_state(sess, reading, me);
}

static void
ctl_stop_read(struct ctl_sess *sess) {
	static const char me[] = "ctl_stop_read";
	struct ctl_sctx *ctx = sess->ctx;

	REQUIRE(sess->state == reading || sess->state == reading_data);
	REQUIRE(sess->rdID.opaque != NULL);
	(void) evDeselectFD(ctx->ev, sess->rdID);
	sess->rdID.opaque = NULL;
	if (sess->rdtiID.opaque != NULL) {
		(void) evClearIdleTimer(ctx->ev, sess->rdtiID);
		sess->rdtiID.opaque = NULL;
	}
	ctl_new_state(sess, idling, me);
}

static void
ctl_readable(evContext lev, void *uap, int fd, int evmask) {
	static const char me[] = "ctl_readable";
	struct ctl_sess *sess = uap;
	struct ctl_sctx *ctx;
	char *eos, tmp[MAX_NTOP];
	ssize_t n;

	REQUIRE(sess != NULL);
	REQUIRE(fd >= 0);
	REQUIRE(evmask == EV_READ);
	REQUIRE(sess->state == reading || sess->state == reading_data);

	ctx = sess->ctx;
	evTouchIdleTimer(lev, sess->rdtiID);
	if (!allocated_p(sess->inbuf) &&
	    ctl_bufget(&sess->inbuf, ctx->logger) < 0) {
		(*ctx->logger)(ctl_error, "%s: %s: cant get an input buffer",
			       me, address_expr);
		ctl_close(sess);
		return;
	}
	n = read(sess->sock, sess->inbuf.text + sess->inbuf.used,
		 MAX_LINELEN - sess->inbuf.used);
	if (n <= 0) {
		(*ctx->logger)(ctl_debug, "%s: %s: read: %s",
			       me, address_expr,
			       (n == 0) ? "Unexpected EOF" : strerror(errno));
		ctl_close(sess);
		return;
	}
	sess->inbuf.used += n;
	eos = memchr(sess->inbuf.text, '\n', sess->inbuf.used);
	if (eos != NULL && eos != sess->inbuf.text && eos[-1] == '\r') {
		eos[-1] = '\0';
		if ((sess->respflags & CTL_DATA) != 0) {
			INSIST(sess->verb != NULL);
			(*sess->verb->func)(sess->ctx, sess, sess->verb,
					    sess->inbuf.text,
					    CTL_DATA, sess->respctx,
					    sess->ctx->uctx);
		} else {
			ctl_stop_read(sess);
			ctl_docommand(sess);
		}
		sess->inbuf.used -= ((eos - sess->inbuf.text) + 1);
		if (sess->inbuf.used == 0U)
			ctl_bufput(&sess->inbuf);
		else
			memmove(sess->inbuf.text, eos + 1, sess->inbuf.used);
		return;
	}
	if (sess->inbuf.used == (size_t)MAX_LINELEN) {
		(*ctx->logger)(ctl_error, "%s: %s: line too long, closing",
			       me, address_expr);
		ctl_close(sess);
	}
}

static void
ctl_wrtimeout(evContext lev, void *uap,
	      struct timespec due,
	      struct timespec itv)
{
	static const char me[] = "ctl_wrtimeout";
	struct ctl_sess *sess = uap;
	struct ctl_sctx *ctx = sess->ctx;
	char tmp[MAX_NTOP];
	
	UNUSED(lev);
	UNUSED(due);
	UNUSED(itv);

	REQUIRE(sess->state == writing);
	sess->wrtiID.opaque = NULL;
	(*ctx->logger)(ctl_warning, "%s: %s: write timeout, closing",
		       me, address_expr);
	if (sess->wrID.opaque != NULL) {
		(void) evCancelRW(ctx->ev, sess->wrID);
		sess->wrID.opaque = NULL;
	}
	ctl_signal_done(ctx, sess);
	ctl_new_state(sess, processing, me);
	ctl_close(sess);
}

static void
ctl_rdtimeout(evContext lev, void *uap,
	      struct timespec due,
	      struct timespec itv)
{
	static const char me[] = "ctl_rdtimeout";
	struct ctl_sess *sess = uap;
	struct ctl_sctx *ctx = sess->ctx;
	char tmp[MAX_NTOP];

	UNUSED(lev);
	UNUSED(due);
	UNUSED(itv);

	REQUIRE(sess->state == reading);
	sess->rdtiID.opaque = NULL;
	(*ctx->logger)(ctl_warning, "%s: %s: timeout, closing",
		       me, address_expr);
	if (sess->state == reading || sess->state == reading_data)
		ctl_stop_read(sess);
	ctl_signal_done(ctx, sess);
	ctl_new_state(sess, processing, me);
	ctl_response(sess, ctx->timeoutcode, "Timeout.", CTL_EXIT, NULL,
		     NULL, NULL, NULL, 0);
}

static void
ctl_docommand(struct ctl_sess *sess) {
	static const char me[] = "ctl_docommand";
	char *name, *rest, tmp[MAX_NTOP];
	struct ctl_sctx *ctx = sess->ctx;
	const struct ctl_verb *verb;

	REQUIRE(allocated_p(sess->inbuf));
	(*ctx->logger)(ctl_debug, "%s: %s: \"%s\" [%u]",
		       me, address_expr,
		       sess->inbuf.text, (u_int)sess->inbuf.used);
	ctl_new_state(sess, processing, me);
	name = sess->inbuf.text + strspn(sess->inbuf.text, space);
	rest = name + strcspn(name, space);
	if (*rest != '\0') {
		*rest++ = '\0';
		rest += strspn(rest, space);
	}
	for (verb = ctx->verbs;
	     verb != NULL && verb->name != NULL && verb->func != NULL;
	     verb++)
		if (verb->name[0] != '\0' && strcasecmp(name, verb->name) == 0)
			break;
	if (verb != NULL && verb->name != NULL && verb->func != NULL) {
		sess->verb = verb;
		(*verb->func)(ctx, sess, verb, rest, 0, NULL, ctx->uctx);
	} else {
		char buf[1100];

		if (sizeof "Unrecognized command \"\" (args \"\")" +
		    strlen(name) + strlen(rest) > sizeof buf)
			strcpy(buf, "Unrecognized command (buf ovf)");
		else
			sprintf(buf,
				"Unrecognized command \"%s\" (args \"%s\")",
				name, rest);
		ctl_response(sess, ctx->unkncode, buf, 0, NULL, NULL, NULL,
			     NULL, 0);
	}
}

static void
ctl_writedone(evContext lev, void *uap, int fd, int bytes) {
	static const char me[] = "ctl_writedone";
	struct ctl_sess *sess = uap;
	struct ctl_sctx *ctx = sess->ctx;
	char tmp[MAX_NTOP];
	int save_errno = errno;

	UNUSED(lev);
	UNUSED(uap);

	REQUIRE(sess->state == writing);
	REQUIRE(fd == sess->sock);
	REQUIRE(sess->wrtiID.opaque != NULL);
	sess->wrID.opaque = NULL;
	(void) evClearIdleTimer(ctx->ev, sess->wrtiID);
	sess->wrtiID.opaque = NULL;
	if (bytes < 0) {
		(*ctx->logger)(ctl_error, "%s: %s: %s",
			       me, address_expr, strerror(save_errno));
		ctl_close(sess);
		return;
	}

	INSIST(allocated_p(sess->outbuf));
	ctl_bufput(&sess->outbuf);
	if ((sess->respflags & CTL_EXIT) != 0) {
		ctl_signal_done(ctx, sess);
		ctl_close(sess);
		return;
	} else if ((sess->respflags & CTL_MORE) != 0) {
		INSIST(sess->verb != NULL);
		(*sess->verb->func)(sess->ctx, sess, sess->verb, "",
				    CTL_MORE, sess->respctx, sess->ctx->uctx);
	} else {
		ctl_signal_done(ctx, sess);
		ctl_start_read(sess);
	}
}

static void
ctl_morehelp(struct ctl_sctx *ctx, struct ctl_sess *sess,
	     const struct ctl_verb *verb, const char *text,
	     u_int respflags, const void *respctx, void *uctx)
{
	const struct ctl_verb *this = respctx, *next = this + 1;

	UNUSED(ctx);
	UNUSED(verb);
	UNUSED(text);
	UNUSED(uctx);

	REQUIRE(!lastverb_p(this));
	REQUIRE((respflags & CTL_MORE) != 0);
	if (lastverb_p(next))
		respflags &= ~CTL_MORE;
	ctl_response(sess, sess->helpcode, this->help, respflags, next,
		     NULL, NULL, NULL, 0);
}

static void
ctl_signal_done(struct ctl_sctx *ctx, struct ctl_sess *sess) {
	if (sess->donefunc != NULL) {
		(*sess->donefunc)(ctx, sess, sess->uap);
		sess->donefunc = NULL;
	}
}

/*! \file */
