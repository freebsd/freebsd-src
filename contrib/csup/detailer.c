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
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "detailer.h"
#include "fixups.h"
#include "globtree.h"
#include "misc.h"
#include "mux.h"
#include "proto.h"
#include "rcsfile.h"
#include "rsyncfile.h"
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
static int	detailer_dofile_co(struct detailer *, struct coll *,
		    struct status *, char *);
static int	detailer_dofile_rcs(struct detailer *, struct coll *, 
		    char *, char *);
static int	detailer_dofile_regular(struct detailer *, char *, char *);
static int	detailer_dofile_rsync(struct detailer *, char *, char *);
static int	detailer_checkrcsattr(struct detailer *, struct coll *, char *,
		    struct fattr *, int);
int		detailer_send_details(struct detailer *, struct coll *, char *,
		    char *, struct fattr *);

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
			if (coll->co_options & CO_CHECKOUTMODE)
				error = proto_printf(wr, "Y %s %s %s\n",
				    fixup->f_name, coll->co_tag, coll->co_date);
			else {
				error = proto_printf(wr, "A %s\n",
				    fixup->f_name);
			}
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
	struct fattr *rcsattr;
	struct stream *rd, *wr;
	char *attr, *cmd, *file, *line, *msg, *path, *target;
	int error, attic;

	rd = d->rd;
	wr = d->wr;
	attic = 0;
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
		case 'I':
		case 'i':
		case 'j':
			/* Directory operations. */
			file = proto_get_ascii(&line);
			if (file == NULL || line != NULL)
				return (DETAILER_ERR_PROTO);
			error = proto_printf(wr, "%s %s\n", cmd, file);
			if (error)
				return (DETAILER_ERR_WRITE);
			break;
		case 'J':
			/* Set directory attributes. */
			file = proto_get_ascii(&line);
			attr = proto_get_ascii(&line);
			if (file == NULL || line != NULL || attr == NULL)
				return (DETAILER_ERR_PROTO);
			error = proto_printf(wr, "%s %s %s\n", cmd, file, attr);
			if (error)
				return (DETAILER_ERR_WRITE);
			break;
		case 'H':
		case 'h':
			/* Create a hard link. */
			file = proto_get_ascii(&line);
			target = proto_get_ascii(&line);
			if (file == NULL || target == NULL)
				return (DETAILER_ERR_PROTO);
			error = proto_printf(wr, "%s %s %s\n", cmd, file,
			    target);
			break;
		case 't':
			file = proto_get_ascii(&line);
			attr = proto_get_ascii(&line);
			if (file == NULL || attr == NULL || line != NULL) {
				return (DETAILER_ERR_PROTO);
			}
			rcsattr = fattr_decode(attr);
			if (rcsattr == NULL) {
				return (DETAILER_ERR_PROTO);
			}
			error = detailer_checkrcsattr(d, coll, file, rcsattr,
			    1);
			break;

		case 'T':
			file = proto_get_ascii(&line);
			attr = proto_get_ascii(&line);
			if (file == NULL || attr == NULL || line != NULL)
				return (DETAILER_ERR_PROTO);
			rcsattr = fattr_decode(attr);
			if (rcsattr == NULL)
				return (DETAILER_ERR_PROTO);
			error = detailer_checkrcsattr(d, coll, file, rcsattr,
			    0);
			break;

		case 'U':
			/* Add or update file. */
			file = proto_get_ascii(&line);
			if (file == NULL || line != NULL)
				return (DETAILER_ERR_PROTO);
			if (coll->co_options & CO_CHECKOUTMODE) {
				error = detailer_dofile_co(d, coll, st, file);
			} else {
				path = cvspath(coll->co_prefix, file, 0);
				rcsattr = fattr_frompath(path, FATTR_NOFOLLOW);
				error = detailer_send_details(d, coll, file,
				    path, rcsattr);
				if (rcsattr != NULL)
					fattr_free(rcsattr);
				free(path);
			}
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

/*
 * Tell the server to update a regular file.
 */
static int
detailer_dofile_regular(struct detailer *d, char *name, char *path)
{
	struct stream *wr;
	struct stat st;
	char md5[MD5_DIGEST_SIZE];
	int error;
	
	wr = d->wr;
	error = stat(path, &st);
	/* If we don't have it or it's unaccessible, we want it again. */
	if (error) {
		proto_printf(wr, "A %s\n", name);
		return (0);
	}

	/* If not, we want the file to be updated. */
	error = MD5_File(path, md5);
	if (error) {
		lprintf(-1, "Error reading \"%s\"\n", name);
		return (error);
	}
	error = proto_printf(wr, "R %s %O %s\n", name, st.st_size, md5);
	if (error)
		return (DETAILER_ERR_WRITE);
	return (0);
}

/*
 * Tell the server to update a file with the rsync algorithm.
 */
static int
detailer_dofile_rsync(struct detailer *d, char *name, char *path)
{
	struct stream *wr;
	struct rsyncfile *rf;

	wr = d->wr;
	rf = rsync_open(path, 0, 1);
	if (rf == NULL) {
		/* Fallback if we fail in opening it. */
		proto_printf(wr, "A %s\n", name);
		return (0);
	}
	proto_printf(wr, "r %s %z %z\n", name, rsync_filesize(rf),
	    rsync_blocksize(rf));
	/* Detail the blocks. */
	while (rsync_nextblock(rf) != 0)
		proto_printf(wr, "%s %s\n", rsync_rsum(rf), rsync_blockmd5(rf));
	proto_printf(wr, ".\n");
	rsync_close(rf);
	return (0);
}

/*
 * Tell the server to update an RCS file that we have, or send it if we don't.
 */
static int
detailer_dofile_rcs(struct detailer *d, struct coll *coll, char *name,
    char *path)
{
	struct stream *wr;
	struct fattr *fa;
	struct rcsfile *rf;
	int error;

	wr = d->wr;
	path = atticpath(coll->co_prefix, name);
	fa = fattr_frompath(path, FATTR_NOFOLLOW);
	if (fa == NULL) {
		/* We don't have it, so send request to get it. */
		error = proto_printf(wr, "A %s\n", name);
		if (error)
			return (DETAILER_ERR_WRITE);
		free(path);
		return (0);
	}

	rf = rcsfile_frompath(path, name, coll->co_cvsroot, coll->co_tag, 1);
	free(path);
	if (rf == NULL) {
		error = proto_printf(wr, "A %s\n", name);
		if (error)
			return (DETAILER_ERR_WRITE);
		return (0);
	}
	/* Tell to update the RCS file. The client version details follow. */
	rcsfile_send_details(rf, wr);
	rcsfile_free(rf);
	fattr_free(fa);
	return (0);
}

static int
detailer_dofile_co(struct detailer *d, struct coll *coll, struct status *st,
    char *file)
{
	struct stream *wr;
	struct fattr *fa;
	struct statusrec *sr;
	char md5[MD5_DIGEST_SIZE];
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

int
detailer_checkrcsattr(struct detailer *d, struct coll *coll, char *name,
    struct fattr *server_attr, int attic)
{
	struct fattr *client_attr;
	char *attr, *path;
	int error;

	/*
	 * I don't think we can use the status file, since it only records file
	 * attributes in cvsmode.
	 */
	client_attr = NULL;
	path = cvspath(coll->co_prefix, name, attic);
	if (path == NULL) {
		return (DETAILER_ERR_PROTO);
	}

	if (access(path, F_OK) == 0 && 
	    ((client_attr = fattr_frompath(path, FATTR_NOFOLLOW)) != NULL) &&
	    fattr_equal(client_attr, server_attr)) {
		attr = fattr_encode(client_attr, NULL, 0);
		if (attic) {
			error = proto_printf(d->wr, "l %s %s\n", name, attr);
		} else {
			error = proto_printf(d->wr, "L %s %s\n", name, attr);
		}
		free(attr);
		free(path);
		fattr_free(client_attr);
		if (error)
			return (DETAILER_ERR_WRITE);
		return (0);
	}
	/* We don't have it, so tell the server to send it. */
	error = detailer_send_details(d, coll, name, path, client_attr);
	fattr_free(client_attr);
	free(path);
	return (error);
}

int
detailer_send_details(struct detailer *d, struct coll *coll, char *name,
    char *path, struct fattr *fa)
{
	int error;
	size_t len;

       /*
        * Try to check if the file exists either live or dead to see if we can
        * edit it and put it live or dead, rather than receiving the entire
        * file.
	*/
	if (fa == NULL) {
		path = atticpath(coll->co_prefix, name); 
		fa = fattr_frompath(path, FATTR_NOFOLLOW);
	}
	if (fa == NULL) {
		error = proto_printf(d->wr, "A %s\n", name);
		if (error)
			return (DETAILER_ERR_WRITE);
	} else if (fattr_type(fa) == FT_FILE) {
		if (isrcs(name, &len) && !(coll->co_options & CO_NORCS)) {
			detailer_dofile_rcs(d, coll, name, path);
		} else if (!(coll->co_options & CO_NORSYNC) &&
		    !globtree_test(coll->co_norsync, name)) {
			detailer_dofile_rsync(d, name, path);
		} else {
			detailer_dofile_regular(d, name, path);
		}
	} else {
		error = proto_printf(d->wr, "N %s\n", name);
		if (error)
			return (DETAILER_ERR_WRITE);
	}
	return (0);
}
