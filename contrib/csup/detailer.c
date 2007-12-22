/*-
 * Copyright (c) 2003-2006, Maxime Henrion <mux@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *
 * $FreeBSD$
 */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "detailer.h"
#include "fixups.h"
#include "misc.h"
#include "mux.h"
#include "proto.h"
#include "status.h"
#include "stream.h"

/* Internal error codes. */
#define	DETAILER_ERR_PROTO	(-1)	/* Protocol error. */
#define	DETAILER_ERR_MSG	(-2)	/* Error is in detailer->errmsg. */
#define	DETAILER_ERR_READ	(-3)	/* Error reading from server. */
#define	DETAILER_ERR_WRITE	(-4)	/* Error writing to server. */

struct detailer {
	struct config *config;
	struct stream *rd;
	struct stream *wr;
	char *errmsg;
};

static int	detailer_batch(struct detailer *);
static int	detailer_coll(struct detailer *, struct coll *,
		    struct status *);
static int	detailer_dofile(struct detailer *, struct coll *,
		    struct status *, char *);

void *
detailer(void *arg)
{
	struct thread_args *args;
	struct detailer dbuf, *d;
	int error;

	args = arg;

	d = &dbuf;
	d->config = args->config;
	d->rd = args->rd;
	d->wr = args->wr;
	d->errmsg = NULL;

	error = detailer_batch(d);
	switch (error) {
	case DETAILER_ERR_PROTO:
		xasprintf(&args->errmsg, "Detailer failed: Protocol error");
		args->status = STATUS_FAILURE;
		break;
	case DETAILER_ERR_MSG:
		xasprintf(&args->errmsg, "Detailer failed: %s", d->errmsg);
		free(d->errmsg);
		args->status = STATUS_FAILURE;
		break;
	case DETAILER_ERR_READ:
		if (stream_eof(d->rd)) {
			xasprintf(&args->errmsg, "Detailer failed: "
			    "Premature EOF from server");
		} else {
			xasprintf(&args->errmsg, "Detailer failed: "
			    "Network read failure: %s", strerror(errno));
		}
		args->status = STATUS_TRANSIENTFAILURE;
		break;
	case DETAILER_ERR_WRITE:
		xasprintf(&args->errmsg, "Detailer failed: "
		    "Network write failure: %s", strerror(errno));
		args->status = STATUS_TRANSIENTFAILURE;
		break;
	default:
		assert(error == 0);
		args->status = STATUS_SUCCESS;
	}
	return (NULL);
}

static int
detailer_batch(struct detailer *d)
{
	struct config *config;
	struct stream *rd, *wr;
	struct coll *coll;
	struct status *st;
	struct fixup *fixup;
	char *cmd, *collname, *line, *release;
	int error, fixupseof;

	config = d->config;
	rd = d->rd;
	wr = d->wr;
	STAILQ_FOREACH(coll, &config->colls, co_next) {
		if (coll->co_options & CO_SKIP)
			continue;
		line = stream_getln(rd, NULL);
		cmd = proto_get_ascii(&line);
		collname = proto_get_ascii(&line);
		release = proto_get_ascii(&line);
		error = proto_get_time(&line, &coll->co_scantime);
		if (error || line != NULL || strcmp(cmd, "COLL") != 0 ||
		    strcmp(collname, coll->co_name) != 0 ||
		    strcmp(release, coll->co_release) != 0)
			return (DETAILER_ERR_PROTO);
		error = proto_printf(wr, "COLL %s %s\n", coll->co_name,
		    coll->co_release);
		if (error)
			return (DETAILER_ERR_WRITE);
		stream_flush(wr);
		if (coll->co_options & CO_COMPRESS) {
			stream_filter_start(rd, STREAM_FILTER_ZLIB, NULL);
			stream_filter_start(wr, STREAM_FILTER_ZLIB, NULL);
		}
		st = status_open(coll, -1, &d->errmsg);
		if (st == NULL)
			return (DETAILER_ERR_MSG);
		error = detailer_coll(d, coll, st);
		status_close(st, NULL);
		if (error)
			return (error);
		if (coll->co_options & CO_COMPRESS) {
			stream_filter_stop(rd);
			stream_filter_stop(wr);
		}
		stream_flush(wr);
	}
	line = stream_getln(rd, NULL);
	if (line == NULL)
		return (DETAILER_ERR_READ);
	if (strcmp(line, ".") != 0)
		return (DETAILER_ERR_PROTO);
	error = proto_printf(wr, ".\n");
	if (error)
		return (DETAILER_ERR_WRITE);
	stream_flush(wr);

	/* Now send fixups if needed. */
	fixup = NULL;
	fixupseof = 0;
	STAILQ_FOREACH(coll, &config->colls, co_next) {
		if (coll->co_options & CO_SKIP)
			continue;
		error = proto_printf(wr, "COLL %s %s\n", coll->co_name,
		    coll->co_release);
		if (error)
			return (DETAILER_ERR_WRITE);
		if (coll->co_options & CO_COMPRESS)
			stream_filter_start(wr, STREAM_FILTER_ZLIB, NULL);
		while (!fixupseof) {
			if (fixup == NULL)
				fixup = fixups_get(config->fixups);
			if (fixup == NULL) {
				fixupseof = 1;
				break;
			}
			if (fixup->f_coll != coll)
				break;
			error = proto_printf(wr, "Y %s %s %s\n", fixup->f_name,
			    coll->co_tag, coll->co_date);
			if (error)
				return (DETAILER_ERR_WRITE);
			fixup = NULL;
		}
		error = proto_printf(wr, ".\n");
		if (error)
			return (DETAILER_ERR_WRITE);
		if (coll->co_options & CO_COMPRESS)
			stream_filter_stop(wr);
		stream_flush(wr);
	}
	error = proto_printf(wr, ".\n");
	if (error)
		return (DETAILER_ERR_WRITE);
	return (0);
}

static int
detailer_coll(struct detailer *d, struct coll *coll, struct status *st)
{
	struct stream *rd, *wr;
	char *cmd, *file, *line, *msg;
	int error;

	rd = d->rd;
	wr = d->wr;
	line = stream_getln(rd, NULL);
	if (line == NULL)
		return (DETAILER_ERR_READ);
	while (strcmp(line, ".") != 0) {
		cmd = proto_get_ascii(&line);
		if (cmd == NULL || strlen(cmd) != 1)
			return (DETAILER_ERR_PROTO);
		switch (cmd[0]) {
		case 'D':
			/* Delete file. */
			file = proto_get_ascii(&line);
			if (file == NULL || line != NULL)
				return (DETAILER_ERR_PROTO);
			error = proto_printf(wr, "D %s\n", file);
			if (error)
				return (DETAILER_ERR_WRITE);
			break;
		case 'U':
			/* Add or update file. */
			file = proto_get_ascii(&line);
			if (file == NULL || line != NULL)
				return (DETAILER_ERR_PROTO);
			error = detailer_dofile(d, coll, st, file);
			if (error)
				return (error);
			break;
		case '!':
			/* Warning from server. */
			msg = proto_get_rest(&line);
			if (msg == NULL)
				return (DETAILER_ERR_PROTO);
			lprintf(-1, "Server warning: %s\n", msg);
			break;
		default:
			return (DETAILER_ERR_PROTO);
		}
		stream_flush(wr);
		line = stream_getln(rd, NULL);
		if (line == NULL)
			return (DETAILER_ERR_READ);
	}
	error = proto_printf(wr, ".\n");
	if (error)
		return (DETAILER_ERR_WRITE);
	return (0);
}

static int
detailer_dofile(struct detailer *d, struct coll *coll, struct status *st,
    char *file)
{
	char md5[MD5_DIGEST_SIZE];
	struct stream *wr;
	struct fattr *fa;
	struct statusrec *sr;
	char *path;
	int error, ret;

	wr = d->wr;
	path = checkoutpath(coll->co_prefix, file);
	if (path == NULL)
		return (DETAILER_ERR_PROTO);
	fa = fattr_frompath(path, FATTR_NOFOLLOW);
	if (fa == NULL) {
		/* We don't have the file, so the only option at this
		   point is to tell the server to send it.  The server
		   may figure out that the file is dead, in which case
		   it will tell us. */
		error = proto_printf(wr, "C %s %s %s\n",
		    file, coll->co_tag, coll->co_date);
		free(path);
		if (error)
			return (DETAILER_ERR_WRITE);
		return (0);
	}
	ret = status_get(st, file, 0, 0, &sr);
	if (ret == -1) {
		d->errmsg = status_errmsg(st);
		free(path);
		return (DETAILER_ERR_MSG);
	}
	if (ret == 0)
		sr = NULL;

	/* If our recorded information doesn't match the file that the
	   client has, then ignore the recorded information. */
	if (sr != NULL && (sr->sr_type != SR_CHECKOUTLIVE ||
	    !fattr_equal(sr->sr_clientattr, fa)))
		sr = NULL;
	fattr_free(fa);
	if (sr != NULL && strcmp(sr->sr_revdate, ".") != 0) {
		error = proto_printf(wr, "U %s %s %s %s %s\n", file,
		    coll->co_tag, coll->co_date, sr->sr_revnum, sr->sr_revdate);
		free(path);
		if (error)
			return (DETAILER_ERR_WRITE);
		return (0);
	}

	/*
	 * We don't have complete and/or accurate recorded information
	 * about what version of the file we have.  Compute the file's
	 * checksum as an aid toward identifying which version it is.
	 */
	error = MD5_File(path, md5);
	if (error) {
		xasprintf(&d->errmsg,
		    "Cannot calculate checksum for \"%s\": %s", path,
		    strerror(errno));
		return (DETAILER_ERR_MSG);
	}
	free(path);
	if (sr == NULL) {
		error = proto_printf(wr, "S %s %s %s %s\n", file,
		    coll->co_tag, coll->co_date, md5);
	} else {
		error = proto_printf(wr, "s %s %s %s %s %s\n", file,
		    coll->co_tag, coll->co_date, sr->sr_revnum, md5);
	}
	if (error)
		return (DETAILER_ERR_WRITE);
	return (0);
}
