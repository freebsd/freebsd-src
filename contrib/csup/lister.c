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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "attrstack.h"
#include "config.h"
#include "fattr.h"
#include "globtree.h"
#include "lister.h"
#include "misc.h"
#include "mux.h"
#include "proto.h"
#include "status.h"
#include "stream.h"

/* Internal error codes. */
#define	LISTER_ERR_WRITE	(-1)	/* Error writing to server. */
#define	LISTER_ERR_STATUS	(-2)	/* Status file error in lstr->errmsg. */

struct lister {
	struct config *config;
	struct stream *wr;
	char *errmsg;
};

static int	lister_batch(struct lister *);
static int	lister_coll(struct lister *, struct coll *, struct status *);
static int	lister_dodirdown(struct lister *, struct coll *,
		    struct statusrec *, struct attrstack *as);
static int	lister_dodirup(struct lister *, struct coll *,
    		    struct statusrec *, struct attrstack *as);
static int	lister_dofile(struct lister *, struct coll *,
		    struct statusrec *);
static int	lister_dodead(struct lister *, struct coll *,
		    struct statusrec *);

void *
lister(void *arg)
{
	struct thread_args *args;
	struct lister lbuf, *l;
	int error;

	args = arg;
	l = &lbuf;
	l->config = args->config;
	l->wr = args->wr;
	l->errmsg = NULL;
	error = lister_batch(l);
	switch (error) {
	case LISTER_ERR_WRITE:
		xasprintf(&args->errmsg,
		    "TreeList failed: Network write failure: %s",
		    strerror(errno));
		args->status = STATUS_TRANSIENTFAILURE;
		break;
	case LISTER_ERR_STATUS:
		xasprintf(&args->errmsg,
		    "TreeList failed: %s.  Delete it and try again.",
		    l->errmsg);
		free(l->errmsg);
		args->status = STATUS_FAILURE;
		break;
	default:
		assert(error == 0);
		args->status = STATUS_SUCCESS;
	};
	return (NULL);
}

static int
lister_batch(struct lister *l)
{
	struct config *config;
	struct stream *wr;
	struct status *st;
	struct coll *coll;
	int error;

	config = l->config;
	wr = l->wr;
	STAILQ_FOREACH(coll, &config->colls, co_next) {
		if (coll->co_options & CO_SKIP)
			continue;
		st = status_open(coll, -1, &l->errmsg);
		if (st == NULL)
			return (LISTER_ERR_STATUS);
		error = proto_printf(wr, "COLL %s %s\n", coll->co_name,
		    coll->co_release);
		if (error)
			return (LISTER_ERR_WRITE);
		stream_flush(wr);
		if (coll->co_options & CO_COMPRESS)
			stream_filter_start(wr, STREAM_FILTER_ZLIB, NULL);
		error = lister_coll(l, coll, st);
		status_close(st, NULL);
		if (error)
			return (error);
		if (coll->co_options & CO_COMPRESS)
			stream_filter_stop(wr);
		stream_flush(wr);
	}
	error = proto_printf(wr, ".\n");
	if (error)
		return (LISTER_ERR_WRITE);
	return (0);
}

/* List a single collection based on the status file. */
static int
lister_coll(struct lister *l, struct coll *coll, struct status *st)
{
	struct stream *wr;
	struct attrstack *as;
	struct statusrec *sr;
	struct fattr *fa;
	size_t i;
	int depth, error, ret, prunedepth;

	wr = l->wr;
	depth = 0;
	prunedepth = INT_MAX;
	as = attrstack_new();
	while ((ret = status_get(st, NULL, 0, 0, &sr)) == 1) {
		switch (sr->sr_type) {
		case SR_DIRDOWN:
			depth++;
			if (depth < prunedepth) {
				error = lister_dodirdown(l, coll, sr, as);
				if (error < 0)
					goto bad;
				if (error)
					prunedepth = depth;
			}
			break;
		case SR_DIRUP:
			if (depth < prunedepth) {
				error = lister_dodirup(l, coll, sr, as);
				if (error)
					goto bad;
			} else if (depth == prunedepth) {
				/* Finished pruning. */
				prunedepth = INT_MAX;
			}
			depth--;
			continue;
		case SR_CHECKOUTLIVE:
			if (depth < prunedepth) {
				error = lister_dofile(l, coll, sr);
				if (error)
					goto bad;
			}
			break;
		case SR_CHECKOUTDEAD:
			if (depth < prunedepth) {
				error = lister_dodead(l, coll, sr);
				if (error)
					goto bad;
			}
			break;
		}
	}
	if (ret == -1) {
		l->errmsg = status_errmsg(st);
		error = LISTER_ERR_STATUS;
		goto bad;
	}
	assert(status_eof(st));
	assert(depth == 0);
	error = proto_printf(wr, ".\n");
	attrstack_free(as);
	if (error)
		return (LISTER_ERR_WRITE);
	return (0);
bad:
	for (i = 0; i < attrstack_size(as); i++) {
		fa = attrstack_pop(as);
		fattr_free(fa);
	}
	attrstack_free(as);
	return (error);
}

/* Handle a directory up entry found in the status file. */
static int
lister_dodirdown(struct lister *l, struct coll *coll, struct statusrec *sr,
    struct attrstack *as)
{
	struct config *config;
	struct stream *wr;
	struct fattr *fa, *fa2;
	char *path;
	int error;

	config = l->config;
	wr = l->wr;
	if (!globtree_test(coll->co_dirfilter, sr->sr_file))
		return (1);
	if (coll->co_options & CO_TRUSTSTATUSFILE) {
		fa = fattr_new(FT_DIRECTORY, -1);
	} else {
		xasprintf(&path, "%s/%s", coll->co_prefix, sr->sr_file);
		fa = fattr_frompath(path, FATTR_NOFOLLOW);
		if (fa == NULL) {
			/* The directory doesn't exist, prune
			 * everything below it. */
			free(path);
			return (1);
		}
		if (fattr_type(fa) == FT_SYMLINK) {
			fa2 = fattr_frompath(path, FATTR_FOLLOW);
			if (fa2 != NULL && fattr_type(fa2) == FT_DIRECTORY) {
				/* XXX - When not in checkout mode, CVSup warns
				 * here about the file being a symlink to a
				 * directory instead of a directory. */
				fattr_free(fa);
				fa = fa2;
			} else {
				fattr_free(fa2);
			}
		}
		free(path);
	}

	if (fattr_type(fa) != FT_DIRECTORY) {
		fattr_free(fa);
		/* Report it as something bogus so
		 * that it will be replaced. */
		error = proto_printf(wr, "F %s %F\n", pathlast(sr->sr_file),
		    fattr_bogus, config->fasupport, coll->co_attrignore);
		if (error)
			return (LISTER_ERR_WRITE);
		return (1);
	}

	/* It really is a directory. */
	attrstack_push(as, fa);
	error = proto_printf(wr, "D %s\n", pathlast(sr->sr_file));
	if (error)
		return (LISTER_ERR_WRITE);
	return (0);
}

/* Handle a directory up entry found in the status file. */
static int
lister_dodirup(struct lister *l, struct coll *coll, struct statusrec *sr,
    struct attrstack *as)
{
	struct config *config;
	const struct fattr *sendattr;
	struct stream *wr;
	struct fattr *fa, *fa2;
	int error;

	config = l->config;
	wr = l->wr;
	fa = attrstack_pop(as);
	if (coll->co_options & CO_TRUSTSTATUSFILE) {
		fattr_free(fa);
		fa = sr->sr_clientattr;
	}

	fa2 = sr->sr_clientattr;
	if (fattr_equal(fa, fa2))
		sendattr = fa;
	else
		sendattr = fattr_bogus;
	error = proto_printf(wr, "U %F\n", sendattr, config->fasupport,
	    coll->co_attrignore);
	if (error)
		return (LISTER_ERR_WRITE);
	if (!(coll->co_options & CO_TRUSTSTATUSFILE))
		fattr_free(fa);
	/* XXX CVSup flushes here for some reason with a comment saying
	   "Be smarter".  We don't flush when listing other file types. */
	stream_flush(wr);
	return (0);
}

/* Handle a checkout live entry found in the status file. */
static int
lister_dofile(struct lister *l, struct coll *coll, struct statusrec *sr)
{
	struct config *config;
	struct stream *wr;
	const struct fattr *sendattr, *fa;
	struct fattr *fa2, *rfa;
	char *path, *spath;
	int error;

	if (!globtree_test(coll->co_filefilter, sr->sr_file))
		return (0);
	config = l->config;
	wr = l->wr;
	rfa = NULL;
	sendattr = NULL;
	error = 0;
	if (!(coll->co_options & CO_TRUSTSTATUSFILE)) {
		path = checkoutpath(coll->co_prefix, sr->sr_file);
		if (path == NULL) {
			spath = coll_statuspath(coll);
			xasprintf(&l->errmsg, "Error in \"%s\": "
			    "Invalid filename \"%s\"", spath, sr->sr_file);
			free(spath);
			return (LISTER_ERR_STATUS);
		}
		rfa = fattr_frompath(path, FATTR_NOFOLLOW);
		free(path);
		if (rfa == NULL) {
			/*
			 * According to the checkouts file we should have
			 * this file but we don't.  Maybe the user deleted
			 * the file, or maybe the checkouts file is wrong.
			 * List the file with bogus attributes to cause the
			 * server to get things back in sync again.
			 */
			sendattr = fattr_bogus;
			goto send;
		}
		fa = rfa;
	} else {
		fa = sr->sr_clientattr;
	}
	fa2 = fattr_forcheckout(sr->sr_serverattr, coll->co_umask);
	if (!fattr_equal(fa, sr->sr_clientattr) || !fattr_equal(fa, fa2) ||
	    strcmp(coll->co_tag, sr->sr_tag) != 0 ||
	    strcmp(coll->co_date, sr->sr_date) != 0) {
		/*
		 * The file corresponds to the information we have
		 * recorded about it, and its moded is correct for
		 * the requested umask setting.
		 */
		sendattr = fattr_bogus;
	} else {
		/*
		 * Either the file has been touched, or we are asking
		 * for a different revision than the one we recorded
		 * information about, or its mode isn't right (because
		 * it was last updated using a version of CVSup that
		 * wasn't so strict about modes).
		 */
		sendattr = sr->sr_serverattr;
	}
	fattr_free(fa2);
	if (rfa != NULL)
		fattr_free(rfa);
send:
	error = proto_printf(wr, "F %s %F\n", pathlast(sr->sr_file), sendattr,
	    config->fasupport, coll->co_attrignore);
	if (error)
		return (LISTER_ERR_WRITE);
	return (0);
}

/* Handle a checkout dead entry found in the status file. */
static int
lister_dodead(struct lister *l, struct coll *coll, struct statusrec *sr)
{
	struct config *config;
	struct stream *wr;
	const struct fattr *sendattr;
	struct fattr *fa;
	char *path, *spath;
	int error;

	if (!globtree_test(coll->co_filefilter, sr->sr_file))
		return (0);
	config = l->config;
	wr = l->wr;
	if (!(coll->co_options & CO_TRUSTSTATUSFILE)) {
		path = checkoutpath(coll->co_prefix, sr->sr_file);
		if (path == NULL) {
			spath = coll_statuspath(coll);
			xasprintf(&l->errmsg, "Error in \"%s\": "
			    "Invalid filename \"%s\"", spath, sr->sr_file);
			free(spath);
			return (LISTER_ERR_STATUS);
		}
		fa = fattr_frompath(path, FATTR_NOFOLLOW);
		free(path);
		if (fa != NULL && fattr_type(fa) != FT_DIRECTORY) {
			/*
			 * We shouldn't have this file but we do.  Report
			 * it to the server, which will either send a
			 * deletion request, of (if the file has come alive)
			 * sent the correct version.
			 */
			fattr_free(fa);
			error = proto_printf(wr, "F %s %F\n",
			    pathlast(sr->sr_file), fattr_bogus,
			    config->fasupport, coll->co_attrignore);
			if (error)
				return (LISTER_ERR_WRITE);
			return (0);
		}
		fattr_free(fa);
	}
	if (strcmp(coll->co_tag, sr->sr_tag) != 0 ||
	    strcmp(coll->co_date, sr->sr_date) != 0)
		sendattr = fattr_bogus;
	else
		sendattr = sr->sr_serverattr;
	error = proto_printf(wr, "f %s %F\n", pathlast(sr->sr_file), sendattr,
	    config->fasupport, coll->co_attrignore);
	if (error)
		return (LISTER_ERR_WRITE);
	return (0);
}
