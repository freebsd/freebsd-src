#if !defined(lint) && !defined(SABER)
static const char rcsid[] = "$Id: ctl_clnt.c,v 8.15 2000/11/14 01:10:36 vixie Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1998,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/* Extern. */

#include "port_before.h"

#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>

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

#include <isc/assertions.h>
#include <isc/ctl.h>
#include <isc/eventlib.h>
#include <isc/list.h>
#include <isc/memcluster.h>

#include "ctl_p.h"

#include "port_after.h"

/* Constants. */


/* Macros. */

#define donefunc_p(ctx) ((ctx).donefunc != NULL)
#define arpacode_p(line) (isdigit(line[0]) && isdigit(line[1]) && \
			  isdigit(line[2]))
#define arpacont_p(line) (line[3] == '-')
#define arpadone_p(line) (line[3] == ' ' || line[3] == '\t' || \
			  line[3] == '\r' || line[3] == '\0')

/* Types. */

enum state {
	initializing = 0, connecting, connected, destroyed
};

struct ctl_tran {
	LINK(struct ctl_tran)	link;
	LINK(struct ctl_tran)	wlink;
	struct ctl_cctx *	ctx;
	struct ctl_buf		outbuf;
	ctl_clntdone		donefunc;
	void *			uap;
};

struct ctl_cctx {
	enum state		state;
	evContext		ev;
	int			sock;
	ctl_logfunc		logger;
	ctl_clntdone		donefunc;
	void *			uap;
	evConnID		coID;
	evTimerID		tiID;
	evFileID		rdID;
	evStreamID		wrID;
	struct ctl_buf		inbuf;
	struct timespec		timeout;
	LIST(struct ctl_tran)	tran;
	LIST(struct ctl_tran)	wtran;
};

/* Forward. */

static struct ctl_tran *new_tran(struct ctl_cctx *, ctl_clntdone, void *, int);
static void		start_write(struct ctl_cctx *);
static void		destroy(struct ctl_cctx *, int);
static void		error(struct ctl_cctx *);
static void		new_state(struct ctl_cctx *, enum state);
static void		conn_done(evContext, void *, int,
				  const void *, int,
				  const void *, int);
static void		write_done(evContext, void *, int, int);
static void		start_read(struct ctl_cctx *);
static void		stop_read(struct ctl_cctx *);
static void		readable(evContext, void *, int, int);
static void		start_timer(struct ctl_cctx *);
static void		stop_timer(struct ctl_cctx *);
static void		touch_timer(struct ctl_cctx *);
static void		timer(evContext, void *,
			      struct timespec, struct timespec);

/* Private data. */

static const char * const state_names[] = {
	"initializing", "connecting", "connected", "destroyed"
};

/* Public. */

/*
 * void
 * ctl_client()
 *	create, condition, and connect to a listener on the control port.
 */
struct ctl_cctx *
ctl_client(evContext lev, const struct sockaddr *cap, size_t cap_len,
	   const struct sockaddr *sap, size_t sap_len,
	   ctl_clntdone donefunc, void *uap,
	   u_int timeout, ctl_logfunc logger)
{
	static const char me[] = "ctl_client";
	static const int on = 1;
	struct ctl_cctx *ctx;

	if (logger == NULL)
		logger = ctl_logger;
	ctx = memget(sizeof *ctx);
	if (ctx == NULL) {
		(*logger)(ctl_error, "%s: getmem: %s", me, strerror(errno));
		goto fatal;
	}
	ctx->state = initializing;
	ctx->ev = lev;
	ctx->logger = logger;
	ctx->timeout = evConsTime(timeout, 0);
	ctx->donefunc = donefunc;
	ctx->uap = uap;
	ctx->coID.opaque = NULL;
	ctx->tiID.opaque = NULL;
	ctx->rdID.opaque = NULL;
	ctx->wrID.opaque = NULL;
	buffer_init(ctx->inbuf);
	INIT_LIST(ctx->tran);
	INIT_LIST(ctx->wtran);
	ctx->sock = socket(sap->sa_family, SOCK_STREAM, PF_UNSPEC);
	if (ctx->sock > evHighestFD(ctx->ev)) {
		ctx->sock = -1;
		errno = ENOTSOCK;
	}
	if (ctx->sock < 0) {
		(*ctx->logger)(ctl_error, "%s: socket: %s",
			       me, strerror(errno));
		goto fatal;
	}
	if (cap != NULL) {
		if (setsockopt(ctx->sock, SOL_SOCKET, SO_REUSEADDR,
			       (char *)&on, sizeof on) != 0) {
			(*ctx->logger)(ctl_warning,
				       "%s: setsockopt(REUSEADDR): %s",
				       me, strerror(errno));
		}
		if (bind(ctx->sock, cap, cap_len) < 0) {
			(*ctx->logger)(ctl_error, "%s: bind: %s", me,
				       strerror(errno));
			goto fatal;
		}
	}
	if (evConnect(lev, ctx->sock, (struct sockaddr *)sap, sap_len,
		      conn_done, ctx, &ctx->coID) < 0) {
		(*ctx->logger)(ctl_error, "%s: evConnect(fd %d): %s",
			       me, (void *)ctx->sock, strerror(errno));
 fatal:
		if (ctx != NULL) {
			if (ctx->sock >= 0)
				close(ctx->sock);
			memput(ctx, sizeof *ctx);
		}
		return (NULL);
	}
	new_state(ctx, connecting);
	return (ctx);
}

/*
 * void
 * ctl_endclient(ctx)
 *	close a client and release all of its resources.
 */
void
ctl_endclient(struct ctl_cctx *ctx) {
	if (ctx->state != destroyed)
		destroy(ctx, 0);
	memput(ctx, sizeof *ctx);
}

/*
 * int
 * ctl_command(ctx, cmd, len, donefunc, uap)
 *	Queue a transaction, which will begin with sending cmd
 *	and complete by calling donefunc with the answer.
 */
int
ctl_command(struct ctl_cctx *ctx, const char *cmd, size_t len,
	    ctl_clntdone donefunc, void *uap)
{
	struct ctl_tran *tran;
	char *pc;
	int n;

	switch (ctx->state) {
	case destroyed:
		errno = ENOTCONN;
		return (-1);
	case connecting:
	case connected:
		break;
	default:
		abort();
	}
	if (len >= MAX_LINELEN) {
		errno = EMSGSIZE;
		return (-1);
	}
	tran = new_tran(ctx, donefunc, uap, 1);
	if (tran == NULL)
		return (-1);
	if (ctl_bufget(&tran->outbuf, ctx->logger) < 0)
		return (-1);
	memcpy(tran->outbuf.text, cmd, len);
	tran->outbuf.used = len;
	for (pc = tran->outbuf.text, n = 0; n < tran->outbuf.used; pc++, n++)
		if (!isascii(*pc) || !isprint(*pc))
			*pc = '\040';
	start_write(ctx);
	return (0);
}

/* Private. */

static struct ctl_tran *
new_tran(struct ctl_cctx *ctx, ctl_clntdone donefunc, void *uap, int w) {
	struct ctl_tran *new = memget(sizeof *new);

	if (new == NULL)
		return (NULL);
	new->ctx = ctx;
	buffer_init(new->outbuf);
	new->donefunc = donefunc;
	new->uap = uap;
	INIT_LINK(new, link);
	INIT_LINK(new, wlink);
	APPEND(ctx->tran, new, link);
	if (w)
		APPEND(ctx->wtran, new, wlink);
	return (new);
}

static void
start_write(struct ctl_cctx *ctx) {
	static const char me[] = "isc/ctl_clnt::start_write";
	struct ctl_tran *tran;
	struct iovec iov[2], *iovp = iov;

	REQUIRE(ctx->state == connecting || ctx->state == connected);
	/* If there is a write in progress, don't try to write more yet. */
	if (ctx->wrID.opaque != NULL)
		return;
	/* If there are no trans, make sure timer is off, and we're done. */
	if (EMPTY(ctx->wtran)) {
		if (ctx->tiID.opaque != NULL)
			stop_timer(ctx);
		return;
	}
	/* Pull it off the head of the write queue. */
	tran = HEAD(ctx->wtran);
	UNLINK(ctx->wtran, tran, wlink);
	/* Since there are some trans, make sure timer is successfully "on". */
	if (ctx->tiID.opaque != NULL)
		touch_timer(ctx);
	else
		start_timer(ctx);
	if (ctx->state == destroyed)
		return;
	/* Marshall a newline-terminated message and clock it out. */
	*iovp++ = evConsIovec(tran->outbuf.text, tran->outbuf.used);
	*iovp++ = evConsIovec("\r\n", 2);
	if (evWrite(ctx->ev, ctx->sock, iov, iovp - iov,
		    write_done, tran, &ctx->wrID) < 0) {
		(*ctx->logger)(ctl_error, "%s: evWrite: %s", me,
			       strerror(errno));
		error(ctx);
		return;
	}
	if (evTimeRW(ctx->ev, ctx->wrID, ctx->tiID) < 0) {
		(*ctx->logger)(ctl_error, "%s: evTimeRW: %s", me,
			       strerror(errno));
		error(ctx);
		return;
	}
}

static void
destroy(struct ctl_cctx *ctx, int notify) {
	struct ctl_tran *this, *next;

	if (ctx->sock != -1) {
		(void) close(ctx->sock);
		ctx->sock = -1;
	}
	switch (ctx->state) {
	case connecting:
		REQUIRE(ctx->wrID.opaque == NULL);
		REQUIRE(EMPTY(ctx->tran));
		/*
		 * This test is nec'y since destroy() can be called from
		 * start_read() while the state is still "connecting".
		 */
		if (ctx->coID.opaque != NULL) {
			(void)evCancelConn(ctx->ev, ctx->coID);
			ctx->coID.opaque = NULL;
		}
		break;
	case connected:
		REQUIRE(ctx->coID.opaque == NULL);
		if (ctx->wrID.opaque != NULL) {
			(void)evCancelRW(ctx->ev, ctx->wrID);
			ctx->wrID.opaque = NULL;
		}
		if (ctx->rdID.opaque != NULL)
			stop_read(ctx);
		break;
	case destroyed:
		break;
	default:
		abort();
	}
	if (allocated_p(ctx->inbuf))
		ctl_bufput(&ctx->inbuf);
	for (this = HEAD(ctx->tran); this != NULL; this = next) {
		next = NEXT(this, link);
		if (allocated_p(this->outbuf))
			ctl_bufput(&this->outbuf);
		if (notify && this->donefunc != NULL)
			(*this->donefunc)(ctx, this->uap, NULL, 0);
		memput(this, sizeof *this);
	}
	if (ctx->tiID.opaque != NULL)
		stop_timer(ctx);
	new_state(ctx, destroyed);
}

static void
error(struct ctl_cctx *ctx) {
	REQUIRE(ctx->state != destroyed);
	destroy(ctx, 1);
}

static void
new_state(struct ctl_cctx *ctx, enum state new_state) {
	static const char me[] = "isc/ctl_clnt::new_state";

	(*ctx->logger)(ctl_debug, "%s: %s -> %s", me,
		       state_names[ctx->state], state_names[new_state]);
	ctx->state = new_state;
}

static void
conn_done(evContext ev, void *uap, int fd,
	  const void *la, int lalen,
	  const void *ra, int ralen)
{
	static const char me[] = "isc/ctl_clnt::conn_done";
	struct ctl_cctx *ctx = uap;
	struct ctl_tran *tran;

	ctx->coID.opaque = NULL;
	if (fd < 0) {
		(*ctx->logger)(ctl_error, "%s: evConnect: %s", me,
			       strerror(errno));
		error(ctx);
		return;
	}
	new_state(ctx, connected);
	tran = new_tran(ctx, ctx->donefunc, ctx->uap, 0);
	if (tran == NULL) {
		(*ctx->logger)(ctl_error, "%s: new_tran failed: %s", me,
			       strerror(errno));
		error(ctx);
		return;
	}
	start_read(ctx);
	if (ctx->state == destroyed) {
		(*ctx->logger)(ctl_error, "%s: start_read failed: %s",
			       me, strerror(errno));
		error(ctx);
		return;
	}
}

static void
write_done(evContext lev, void *uap, int fd, int bytes) {
	struct ctl_tran *tran = (struct ctl_tran *)uap;
	struct ctl_cctx *ctx = tran->ctx;

	ctx->wrID.opaque = NULL;
	if (ctx->tiID.opaque != NULL)
		touch_timer(ctx);
	ctl_bufput(&tran->outbuf);
	start_write(ctx);
	if (bytes < 0)
		destroy(ctx, 1);
	else
		start_read(ctx);
}

static void
start_read(struct ctl_cctx *ctx) {
	static const char me[] = "isc/ctl_clnt::start_read";

	REQUIRE(ctx->state == connecting || ctx->state == connected);
	REQUIRE(ctx->rdID.opaque == NULL);
	if (evSelectFD(ctx->ev, ctx->sock, EV_READ, readable, ctx,
		       &ctx->rdID) < 0)
	{
		(*ctx->logger)(ctl_error, "%s: evSelect(fd %d): %s", me,
			       ctx->sock, strerror(errno));
		error(ctx);
		return;
	}
}

static void
stop_read(struct ctl_cctx *ctx) {
	REQUIRE(ctx->coID.opaque == NULL);
	REQUIRE(ctx->rdID.opaque != NULL);
	(void)evDeselectFD(ctx->ev, ctx->rdID);
	ctx->rdID.opaque = NULL;
}

static void
readable(evContext ev, void *uap, int fd, int evmask) {
	static const char me[] = "isc/ctl_clnt::readable";
	struct ctl_cctx *ctx = uap;
	struct ctl_tran *tran;
	ssize_t n;
	char *eos;

	REQUIRE(ctx != NULL);
	REQUIRE(fd >= 0);
	REQUIRE(evmask == EV_READ);
	REQUIRE(ctx->state == connected);
	REQUIRE(!EMPTY(ctx->tran));
	tran = HEAD(ctx->tran);
	if (!allocated_p(ctx->inbuf) &&
	    ctl_bufget(&ctx->inbuf, ctx->logger) < 0) {
		(*ctx->logger)(ctl_error, "%s: can't get an input buffer", me);
		error(ctx);
		return;
	}
	n = read(ctx->sock, ctx->inbuf.text + ctx->inbuf.used,
		 MAX_LINELEN - ctx->inbuf.used);
	if (n <= 0) {
		(*ctx->logger)(ctl_warning, "%s: read: %s", me,
			       (n == 0) ? "Unexpected EOF" : strerror(errno));
		error(ctx);
		return;
	}
	if (ctx->tiID.opaque != NULL)
		touch_timer(ctx);
	ctx->inbuf.used += n;
	(*ctx->logger)(ctl_debug, "%s: read %d, used %d", me,
		       n, ctx->inbuf.used);
 again:
	eos = memchr(ctx->inbuf.text, '\n', ctx->inbuf.used);
	if (eos != NULL && eos != ctx->inbuf.text && eos[-1] == '\r') {
		int done = 0;

		eos[-1] = '\0';
		if (!arpacode_p(ctx->inbuf.text)) {
			/* XXX Doesn't FTP do this sometimes? Is it legal? */
			(*ctx->logger)(ctl_error, "%s: no arpa code (%s)", me,
				       ctx->inbuf.text);
			error(ctx);
			return;
		}
		if (arpadone_p(ctx->inbuf.text))
			done = 1;
		else if (arpacont_p(ctx->inbuf.text))
			done = 0;
		else {
			/* XXX Doesn't FTP do this sometimes? Is it legal? */
			(*ctx->logger)(ctl_error, "%s: no arpa flag (%s)", me,
				       ctx->inbuf.text);
			error(ctx);
			return;
		}
		(*tran->donefunc)(ctx, tran->uap, ctx->inbuf.text,
				  (done ? 0 : CTL_MORE));
		ctx->inbuf.used -= ((eos - ctx->inbuf.text) + 1);
		if (ctx->inbuf.used == 0)
			ctl_bufput(&ctx->inbuf);
		else
			memmove(ctx->inbuf.text, eos + 1, ctx->inbuf.used);
		if (done) {
			UNLINK(ctx->tran, tran, link);
			memput(tran, sizeof *tran);
			stop_read(ctx);
			start_write(ctx);
			return;
		}
		if (allocated_p(ctx->inbuf))
			goto again;
		return;
	}
	if (ctx->inbuf.used == MAX_LINELEN) {
		(*ctx->logger)(ctl_error, "%s: line too long (%-10s...)", me,
			       ctx->inbuf.text);
		error(ctx);
	}
}

/* Timer related stuff. */

static void
start_timer(struct ctl_cctx *ctx) {
	static const char me[] = "isc/ctl_clnt::start_timer";

	REQUIRE(ctx->tiID.opaque == NULL);
	if (evSetIdleTimer(ctx->ev, timer, ctx, ctx->timeout, &ctx->tiID) < 0){
		(*ctx->logger)(ctl_error, "%s: evSetIdleTimer: %s", me,
			       strerror(errno));
		error(ctx);
		return;
	}
}

static void
stop_timer(struct ctl_cctx *ctx) {
	static const char me[] = "isc/ctl_clnt::stop_timer";

	REQUIRE(ctx->tiID.opaque != NULL);
	if (evClearIdleTimer(ctx->ev, ctx->tiID) < 0) {
		(*ctx->logger)(ctl_error, "%s: evClearIdleTimer: %s", me,
			       strerror(errno));
		error(ctx);
		return;
	}
	ctx->tiID.opaque = NULL;
}

static void
touch_timer(struct ctl_cctx *ctx) {
	REQUIRE(ctx->tiID.opaque != NULL);

	evTouchIdleTimer(ctx->ev, ctx->tiID);
}

static void
timer(evContext ev, void *uap, struct timespec due, struct timespec itv) {
	static const char me[] = "isc/ctl_clnt::timer";
	struct ctl_cctx *ctx = uap;

	ctx->tiID.opaque = NULL;
	(*ctx->logger)(ctl_error, "%s: timeout after %u seconds while %s", me,
		       ctx->timeout.tv_sec, state_names[ctx->state]);
	error(ctx);
}
