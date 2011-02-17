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

#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "diff.h"
#include "fattr.h"
#include "fixups.h"
#include "keyword.h"
#include "updater.h"
#include "misc.h"
#include "mux.h"
#include "proto.h"
#include "rcsfile.h"
#include "status.h"
#include "stream.h"

/* Internal error codes. */
#define	UPDATER_ERR_PROTO	(-1)	/* Protocol error. */
#define	UPDATER_ERR_MSG		(-2)	/* Error is in updater->errmsg. */
#define	UPDATER_ERR_READ	(-3)	/* Error reading from server. */
#define	UPDATER_ERR_DELETELIM	(-4)	/* File deletion limit exceeded. */

#define BUFSIZE 4096

/* Everything needed to update a file. */
struct file_update {
	struct statusrec srbuf;
	char *destpath;
	char *temppath;
	char *origpath;
	char *coname;		/* Points somewhere in destpath. */
	char *wantmd5;
	struct coll *coll;
	struct status *st;
	/* Those are only used for diff updating. */
	char *author;
	struct stream *orig;
	struct stream *to;
	int attic;
	int expand;
};

struct updater {
	struct config *config;
	struct stream *rd;
	char *errmsg;
	int deletecount;
};

static struct file_update	*fup_new(struct coll *, struct status *);
static int	 fup_prepare(struct file_update *, char *, int);
static void	 fup_cleanup(struct file_update *);
static void	 fup_free(struct file_update *);

static void	 updater_prunedirs(char *, char *);
static int	 updater_batch(struct updater *, int);
static int	 updater_docoll(struct updater *, struct file_update *, int);
static int	 updater_delete(struct updater *, struct file_update *);
static void	 updater_deletefile(const char *);
static int	 updater_checkout(struct updater *, struct file_update *, int);
static int	 updater_addfile(struct updater *, struct file_update *,
		     char *, int);
int		 updater_addelta(struct rcsfile *, struct stream *, char *);
static int	 updater_setattrs(struct updater *, struct file_update *,
		     char *, char *, char *, char *, char *, struct fattr *);
static int	updater_setdirattrs(struct updater *, struct coll *,
		     struct file_update *, char *, char *);
static int	 updater_updatefile(struct updater *, struct file_update *fup,
		     const char *, int);
static int	 updater_updatenode(struct updater *, struct coll *, 
		     struct file_update *, char *, char *);
static int	 updater_diff(struct updater *, struct file_update *);
static int	 updater_diff_batch(struct updater *, struct file_update *);
static int	 updater_diff_apply(struct updater *, struct file_update *,
		     char *);
static int	 updater_rcsedit(struct updater *, struct file_update *, char *,
		     char *);
int		 updater_append_file(struct updater *, struct file_update *,
		     off_t);
static int	 updater_rsync(struct updater *, struct file_update *, size_t);
static int	 updater_read_checkout(struct stream *, struct stream *);

static struct file_update *
fup_new(struct coll *coll, struct status *st)
{
	struct file_update *fup;

	fup = xmalloc(sizeof(struct file_update));
	memset(fup, 0, sizeof(*fup));
	fup->coll = coll;
	fup->st = st;
	return (fup);
}

static int
fup_prepare(struct file_update *fup, char *name, int attic)
{
	struct coll *coll;

	coll = fup->coll;
	fup->attic = 0;
	fup->origpath = NULL;

	if (coll->co_options & CO_CHECKOUTMODE)
		fup->destpath = checkoutpath(coll->co_prefix, name);
	else {
		fup->destpath = cvspath(coll->co_prefix, name, attic);
		fup->origpath = atticpath(coll->co_prefix, name);
		/* If they're equal, we don't need special care. */
		if (fup->origpath != NULL &&
		    strcmp(fup->origpath, fup->destpath) == 0) {
			free(fup->origpath);
			fup->origpath = NULL;
		}
		fup->attic = attic;
	}
	if (fup->destpath == NULL)
		return (-1);
	fup->coname = fup->destpath + coll->co_prefixlen + 1;
	return (0);
}

/* Called after each file update to reinit the structure. */
static void
fup_cleanup(struct file_update *fup)
{
	struct statusrec *sr;

	sr = &fup->srbuf;

	if (fup->destpath != NULL) {
		free(fup->destpath);
		fup->destpath = NULL;
	}
	if (fup->temppath != NULL) {
		free(fup->temppath);
		fup->temppath = NULL;
	}
	if (fup->origpath != NULL) {
		free(fup->origpath);
		fup->origpath = NULL;
	}
	fup->coname = NULL;
	if (fup->author != NULL) {
		free(fup->author);
		fup->author = NULL;
	}
	fup->expand = 0;
	if (fup->wantmd5 != NULL) {
		free(fup->wantmd5);
		fup->wantmd5 = NULL;
	}
	if (fup->orig != NULL) {
		stream_close(fup->orig);
		fup->orig = NULL;
	}
	if (fup->to != NULL) {
		stream_close(fup->to);
		fup->to = NULL;
	}
	if (sr->sr_file != NULL)
		free(sr->sr_file);
	if (sr->sr_tag != NULL)
		free(sr->sr_tag);
	if (sr->sr_date != NULL)
		free(sr->sr_date);
	if (sr->sr_revnum != NULL)
		free(sr->sr_revnum);
	if (sr->sr_revdate != NULL)
		free(sr->sr_revdate);
	fattr_free(sr->sr_clientattr);
	fattr_free(sr->sr_serverattr);
	memset(sr, 0, sizeof(*sr));
}

static void
fup_free(struct file_update *fup)
{

	fup_cleanup(fup);
	free(fup);
}

void *
updater(void *arg)
{
	struct thread_args *args;
	struct updater upbuf, *up;
	int error;

	args = arg;

	up = &upbuf;
	up->config = args->config;
	up->rd = args->rd;
	up->errmsg = NULL;
	up->deletecount = 0;

	error = updater_batch(up, 0);

	/*
	 * Make sure to close the fixups even in case of an error,
	 * so that the lister thread doesn't block indefinitely.
	 */
	fixups_close(up->config->fixups);
	if (!error)
		error = updater_batch(up, 1);
	switch (error) {
	case UPDATER_ERR_PROTO:
		xasprintf(&args->errmsg, "Updater failed: Protocol error");
		args->status = STATUS_FAILURE;
		break;
	case UPDATER_ERR_MSG:
		xasprintf(&args->errmsg, "Updater failed: %s", up->errmsg);
		free(up->errmsg);
		args->status = STATUS_FAILURE;
		break;
	case UPDATER_ERR_READ:
		if (stream_eof(up->rd)) {
			xasprintf(&args->errmsg, "Updater failed: "
			    "Premature EOF from server");
		} else {
			xasprintf(&args->errmsg, "Updater failed: "
			    "Network read failure: %s", strerror(errno));
		}
		args->status = STATUS_TRANSIENTFAILURE;
		break;
	case UPDATER_ERR_DELETELIM:
		xasprintf(&args->errmsg, "Updater failed: "
		    "File deletion limit exceeded");
		args->status = STATUS_FAILURE;
		break;
	default:
		assert(error == 0);
		args->status = STATUS_SUCCESS;
	};
	return (NULL);
}

static int
updater_batch(struct updater *up, int isfixups)
{
	struct stream *rd;
	struct coll *coll;
	struct status *st;
	struct file_update *fup;
	char *line, *cmd, *errmsg, *collname, *release;
	int error;

	rd = up->rd;
	STAILQ_FOREACH(coll, &up->config->colls, co_next) {
		if (coll->co_options & CO_SKIP)
			continue;
		umask(coll->co_umask);
		line = stream_getln(rd, NULL);
		if (line == NULL)
			return (UPDATER_ERR_READ);
		cmd = proto_get_ascii(&line);
		collname = proto_get_ascii(&line);
		release = proto_get_ascii(&line);
		if (release == NULL || line != NULL)
			return (UPDATER_ERR_PROTO);
		if (strcmp(cmd, "COLL") != 0 ||
		    strcmp(collname, coll->co_name) != 0 ||
		    strcmp(release, coll->co_release) != 0)
			return (UPDATER_ERR_PROTO);

		if (!isfixups)
			lprintf(1, "Updating collection %s/%s\n", coll->co_name,
			    coll->co_release);

		if (coll->co_options & CO_COMPRESS)
			stream_filter_start(rd, STREAM_FILTER_ZLIB, NULL);

		st = status_open(coll, coll->co_scantime, &errmsg);
		if (st == NULL) {
			up->errmsg = errmsg;
			return (UPDATER_ERR_MSG);
		}
		fup = fup_new(coll, st);
		error = updater_docoll(up, fup, isfixups);
		status_close(st, &errmsg);
		fup_free(fup);
		if (errmsg != NULL) {
			/* Discard previous error. */
			if (up->errmsg != NULL)
				free(up->errmsg);
			up->errmsg = errmsg;
			return (UPDATER_ERR_MSG);
		}
		if (error)
			return (error);

		if (coll->co_options & CO_COMPRESS)
			stream_filter_stop(rd);
	}
	line = stream_getln(rd, NULL);
	if (line == NULL)
		return (UPDATER_ERR_READ);
	if (strcmp(line, ".") != 0)
		return (UPDATER_ERR_PROTO);
	return (0);
}

static int
updater_docoll(struct updater *up, struct file_update *fup, int isfixups)
{
	struct stream *rd;
	struct coll *coll;
	struct statusrec srbuf, *sr;
	struct fattr *rcsattr, *tmp;
	char *attr, *cmd, *blocksize, *line, *msg;
	char *name, *tag, *date, *revdate;
	char *expand, *wantmd5, *revnum;
	char *optstr, *rcsopt, *pos;
	time_t t;
	off_t position;
	int attic, error, needfixupmsg;

	error = 0;
	rd = up->rd;
	coll = fup->coll;
	needfixupmsg = isfixups;
	while ((line = stream_getln(rd, NULL)) != NULL) {
		if (strcmp(line, ".") == 0)
			break;
		memset(&srbuf, 0, sizeof(srbuf));
		if (needfixupmsg) {
			lprintf(1, "Applying fixups for collection %s/%s\n",
			    coll->co_name, coll->co_release);
			needfixupmsg = 0;
		}
		cmd = proto_get_ascii(&line);
		if (cmd == NULL || strlen(cmd) != 1)
			return (UPDATER_ERR_PROTO);
		switch (cmd[0]) {
		case 'T':
			/* Update recorded information for checked-out file. */
			name = proto_get_ascii(&line);
			tag = proto_get_ascii(&line);
			date = proto_get_ascii(&line);
			revnum = proto_get_ascii(&line);
			revdate = proto_get_ascii(&line);
			attr = proto_get_ascii(&line);
			if (attr == NULL || line != NULL)
				return (UPDATER_ERR_PROTO);

			rcsattr = fattr_decode(attr);
			if (rcsattr == NULL)
				return (UPDATER_ERR_PROTO);

			error = fup_prepare(fup, name, 0);
			if (error)
				return (UPDATER_ERR_PROTO);
			error = updater_setattrs(up, fup, name, tag, date,
			    revnum, revdate, rcsattr);
			fattr_free(rcsattr);
			if (error)
				return (error);
			break;
		case 'c':
			/* Checkout dead file. */
			name = proto_get_ascii(&line);
			tag = proto_get_ascii(&line);
			date = proto_get_ascii(&line);
			attr = proto_get_ascii(&line);
			if (attr == NULL || line != NULL)
				return (UPDATER_ERR_PROTO);

			error = fup_prepare(fup, name, 0);
			if (error)
				return (UPDATER_ERR_PROTO);
			/* Theoritically, the file does not exist on the client.
			   Just to make sure, we'll delete it here, if it
			   exists. */
			if (access(fup->destpath, F_OK) == 0) {
				error = updater_delete(up, fup);
				if (error)
					return (error);
			}

			sr = &srbuf;
			sr->sr_type = SR_CHECKOUTDEAD;
			sr->sr_file = name;
			sr->sr_tag = tag;
			sr->sr_date = date;
			sr->sr_serverattr = fattr_decode(attr);
			if (sr->sr_serverattr == NULL)
				return (UPDATER_ERR_PROTO);

			error = status_put(fup->st, sr);
			fattr_free(sr->sr_serverattr);
			if (error) {
				up->errmsg = status_errmsg(fup->st);
				return (UPDATER_ERR_MSG);
			}
			break;
		case 'U':
			/* Update live checked-out file. */
			name = proto_get_ascii(&line);
			tag = proto_get_ascii(&line);
			date = proto_get_ascii(&line);
			proto_get_ascii(&line);	/* XXX - oldRevNum */
			proto_get_ascii(&line);	/* XXX - fromAttic */
			proto_get_ascii(&line);	/* XXX - logLines */
			expand = proto_get_ascii(&line);
			attr = proto_get_ascii(&line);
			wantmd5 = proto_get_ascii(&line);
			if (wantmd5 == NULL || line != NULL)
				return (UPDATER_ERR_PROTO);

			sr = &fup->srbuf;
			sr->sr_type = SR_CHECKOUTLIVE;
			sr->sr_file = xstrdup(name);
			sr->sr_date = xstrdup(date);
			sr->sr_tag = xstrdup(tag);
			sr->sr_serverattr = fattr_decode(attr);
			if (sr->sr_serverattr == NULL)
				return (UPDATER_ERR_PROTO);

			fup->expand = keyword_decode_expand(expand);
			if (fup->expand == -1)
				return (UPDATER_ERR_PROTO);
			error = fup_prepare(fup, name, 0);
			if (error)
				return (UPDATER_ERR_PROTO);

			fup->wantmd5 = xstrdup(wantmd5);
			fup->temppath = tempname(fup->destpath);
			error = updater_diff(up, fup);
			if (error)
				return (error);
			break;
		case 'u':
			/* Update dead checked-out file. */
			name = proto_get_ascii(&line);
			tag = proto_get_ascii(&line);
			date = proto_get_ascii(&line);
			attr = proto_get_ascii(&line);
			if (attr == NULL || line != NULL)
				return (UPDATER_ERR_PROTO);

			error = fup_prepare(fup, name, 0);
			if (error)
				return (UPDATER_ERR_PROTO);
			error = updater_delete(up, fup);
			if (error)
				return (error);
			sr = &srbuf;
			sr->sr_type = SR_CHECKOUTDEAD;
			sr->sr_file = name;
			sr->sr_tag = tag;
			sr->sr_date = date;
			sr->sr_serverattr = fattr_decode(attr);
			if (sr->sr_serverattr == NULL)
				return (UPDATER_ERR_PROTO);
			error = status_put(fup->st, sr);
			fattr_free(sr->sr_serverattr);
			if (error) {
				up->errmsg = status_errmsg(fup->st);
				return (UPDATER_ERR_MSG);
			}
			break;
		case 'C':
		case 'Y':
			/* Checkout file. */
			name = proto_get_ascii(&line);
			tag = proto_get_ascii(&line);
			date = proto_get_ascii(&line);
			revnum = proto_get_ascii(&line);
			revdate = proto_get_ascii(&line);
			attr = proto_get_ascii(&line);
			if (attr == NULL || line != NULL)
				return (UPDATER_ERR_PROTO);

			sr = &fup->srbuf;
			sr->sr_type = SR_CHECKOUTLIVE;
			sr->sr_file = xstrdup(name);
			sr->sr_tag = xstrdup(tag);
			sr->sr_date = xstrdup(date);
			sr->sr_revnum = xstrdup(revnum);
			sr->sr_revdate = xstrdup(revdate);
			sr->sr_serverattr = fattr_decode(attr);
			if (sr->sr_serverattr == NULL)
				return (UPDATER_ERR_PROTO);

			t = rcsdatetotime(revdate);
			if (t == -1)
				return (UPDATER_ERR_PROTO);

			sr->sr_clientattr = fattr_new(FT_FILE, t);
			tmp = fattr_forcheckout(sr->sr_serverattr,
			    coll->co_umask);
			fattr_override(sr->sr_clientattr, tmp, FA_MASK);
			fattr_free(tmp);
			fattr_mergedefault(sr->sr_clientattr);
			error = fup_prepare(fup, name, 0);
			if (error)
				return (UPDATER_ERR_PROTO);
			fup->temppath = tempname(fup->destpath);
			if (*cmd == 'Y')
				error = updater_checkout(up, fup, 1);
			else
				error = updater_checkout(up, fup, 0);
			if (error)
				return (error);
			break;
		case 'D':
			/* Delete file. */
			name = proto_get_ascii(&line);
			if (name == NULL || line != NULL)
				return (UPDATER_ERR_PROTO);
			error = fup_prepare(fup, name, 0);
			if (error)
				return (UPDATER_ERR_PROTO);
			error = updater_delete(up, fup);
			if (error)
				return (error);
			error = status_delete(fup->st, name, 0);
			if (error) {
				up->errmsg = status_errmsg(fup->st);
				return (UPDATER_ERR_MSG);
			}
			break;
		case 'A':
		case 'a':
		case 'R':
			name = proto_get_ascii(&line);
			attr = proto_get_ascii(&line);
			if (name == NULL || attr == NULL || line != NULL)
				return (UPDATER_ERR_PROTO);
			attic = (cmd[0] == 'a');
			error = fup_prepare(fup, name, attic);
			if (error)
				return (UPDATER_ERR_PROTO);

			fup->temppath = tempname(fup->destpath);
			sr = &fup->srbuf;
			sr->sr_type = attic ? SR_FILEDEAD : SR_FILELIVE;
			sr->sr_file = xstrdup(name);
			sr->sr_serverattr = fattr_decode(attr);
			if (sr->sr_serverattr == NULL)
				return (UPDATER_ERR_PROTO);
			if (attic)
				lprintf(1, " Create %s -> Attic\n", name);
			else
				lprintf(1, " Create %s\n", name);
			error = updater_addfile(up, fup, attr, 0);
			if (error)
				return (error);
			break;
		case 'r':
			name = proto_get_ascii(&line);
			attr = proto_get_ascii(&line);
			blocksize = proto_get_ascii(&line);
			wantmd5 = proto_get_ascii(&line);
			if (name == NULL || attr == NULL || blocksize == NULL ||
			    wantmd5 == NULL) {
				return (UPDATER_ERR_PROTO);
			}
			error = fup_prepare(fup, name, 0);
			if (error)
				return (UPDATER_ERR_PROTO);
			fup->wantmd5 = xstrdup(wantmd5);
			fup->temppath = tempname(fup->destpath);
			sr = &fup->srbuf;
			sr->sr_file = xstrdup(name);
			sr->sr_serverattr = fattr_decode(attr);
			sr->sr_type = SR_FILELIVE;
			if (sr->sr_serverattr == NULL)
				return (UPDATER_ERR_PROTO);
			error = updater_rsync(up, fup, strtol(blocksize, NULL,
			    10));
			if (error)
				return (error);
			break;
		case 'I':
			/*
			 * Create directory and add DirDown entry in status
			 * file.
			 */
			name = proto_get_ascii(&line);
			if (name == NULL || line != NULL)
				return (UPDATER_ERR_PROTO);
			error = fup_prepare(fup, name, 0);
			if (error)
				return (UPDATER_ERR_PROTO);
			sr = &fup->srbuf;
			sr->sr_type = SR_DIRDOWN;
			sr->sr_file = xstrdup(name);
			sr->sr_serverattr = NULL;
			sr->sr_clientattr = fattr_new(FT_DIRECTORY, -1);
			fattr_mergedefault(sr->sr_clientattr);

			error = mkdirhier(fup->destpath, coll->co_umask);
			if (error)
				return (UPDATER_ERR_PROTO);
			if (access(fup->destpath, F_OK) != 0) {
				lprintf(1, " Mkdir %s\n", name);
				error = fattr_makenode(sr->sr_clientattr,
				    fup->destpath);
				if (error)
					return (UPDATER_ERR_PROTO);
			}
			error = status_put(fup->st, sr);
			if (error) {
				up->errmsg = status_errmsg(fup->st);
				return (UPDATER_ERR_MSG);
			}
			break;
		case 'i':
			/* Remove DirDown entry in status file. */
			name = proto_get_ascii(&line);
			if (name == NULL || line != NULL)
				return (UPDATER_ERR_PROTO);
			error = fup_prepare(fup, name, 0);
			if (error)
				return (UPDATER_ERR_PROTO);
			error = status_delete(fup->st, name, 0);
			if (error) {
				up->errmsg = status_errmsg(fup->st);
				return (UPDATER_ERR_MSG);
			}
			break;
		case 'J':
			/*
			 * Set attributes of directory and update DirUp entry in
			 * status file.
			 */
			name = proto_get_ascii(&line);
			if (name == NULL)
				return (UPDATER_ERR_PROTO);
			attr = proto_get_ascii(&line);
			if (attr == NULL || line != NULL)
				return (UPDATER_ERR_PROTO);
			error = fup_prepare(fup, name, 0);
			if (error)
				return (UPDATER_ERR_PROTO);
			error = updater_setdirattrs(up, coll, fup, name, attr);
			if (error)
				return (error);
			break;
		case 'j':
			/*
			 * Remove directory and delete its DirUp entry in status
			 * file.
			 */
			name = proto_get_ascii(&line);
			if (name == NULL || line != NULL)
				return (UPDATER_ERR_PROTO);
			error = fup_prepare(fup, name, 0);
			if (error)
				return (UPDATER_ERR_PROTO);
			lprintf(1, " Rmdir %s\n", name);
			updater_deletefile(fup->destpath);
			error = status_delete(fup->st, name, 0);
			if (error) {
				up->errmsg = status_errmsg(fup->st);
				return (UPDATER_ERR_MSG);
			}
			break;
		case 'L':
		case 'l':
			name = proto_get_ascii(&line);
			if (name == NULL)
				return (UPDATER_ERR_PROTO);
			attr = proto_get_ascii(&line);
			if (attr == NULL || line != NULL)
				return (UPDATER_ERR_PROTO);
			attic = (cmd[0] == 'l');
			sr = &fup->srbuf;
			sr->sr_type = attic ? SR_FILEDEAD : SR_FILELIVE;
			sr->sr_file = xstrdup(name);
			sr->sr_serverattr = fattr_decode(attr);
			sr->sr_clientattr = fattr_decode(attr);
			if (sr->sr_serverattr == NULL ||
			    sr->sr_clientattr == NULL)
				return (UPDATER_ERR_PROTO);
			
			/* Save space. Described in detail in updatefile. */
			if (!(fattr_getmask(sr->sr_clientattr) & FA_LINKCOUNT)
			    || fattr_getlinkcount(sr->sr_clientattr) <= 1)
				fattr_maskout(sr->sr_clientattr,
				    FA_DEV | FA_INODE);
			fattr_maskout(sr->sr_clientattr, FA_FLAGS);
			error = status_put(fup->st, sr);
			if (error) {
				up->errmsg = status_errmsg(fup->st);
				return (UPDATER_ERR_MSG);
			}
			break;
		case 'N':
		case 'n':
			name = proto_get_ascii(&line);
			attr = proto_get_ascii(&line);
			if (name == NULL || attr == NULL || line != NULL)
				return (UPDATER_ERR_PROTO);
			attic = (cmd[0] == 'n');
			error = fup_prepare(fup, name, attic);
			if (error)
				return (UPDATER_ERR_PROTO);
			sr = &fup->srbuf;
			sr->sr_type = (attic ? SR_FILEDEAD : SR_FILELIVE);
			sr->sr_file = xstrdup(name);
			sr->sr_serverattr = fattr_decode(attr);
			sr->sr_clientattr = fattr_new(FT_SYMLINK, -1);
			fattr_mergedefault(sr->sr_clientattr);
			fattr_maskout(sr->sr_clientattr, FA_FLAGS);
			error = updater_updatenode(up, coll, fup, name, attr);
			if (error)
				return (error);
			break;
		case 'V':
		case 'v':
			name = proto_get_ascii(&line);
			attr = proto_get_ascii(&line);
			optstr = proto_get_ascii(&line);
			wantmd5 = proto_get_ascii(&line);
			rcsopt = NULL; /* XXX: Not supported. */
			if (attr == NULL || line != NULL || wantmd5 == NULL)
				return (UPDATER_ERR_PROTO);
			attic = (cmd[0] == 'v');
			error = fup_prepare(fup, name, attic);
			if (error)
				return (UPDATER_ERR_PROTO);
			fup->temppath = tempname(fup->destpath);
			fup->wantmd5 = xstrdup(wantmd5);
			sr = &fup->srbuf;
			sr->sr_type = attic ? SR_FILEDEAD : SR_FILELIVE;
			sr->sr_file = xstrdup(name);
			sr->sr_serverattr = fattr_decode(attr);
			if (sr->sr_serverattr == NULL)
				return (UPDATER_ERR_PROTO);

			error = updater_rcsedit(up, fup, name, rcsopt);
			if (error)
				return (error);
			break;
		case 'X':
		case 'x':
			name = proto_get_ascii(&line);
			attr = proto_get_ascii(&line);
			if (name == NULL || attr == NULL || line != NULL)
				return (UPDATER_ERR_PROTO);
			attic = (cmd[0] == 'x');
			error = fup_prepare(fup, name, attic);
			if (error)
				return (UPDATER_ERR_PROTO);

			fup->temppath = tempname(fup->destpath);
			sr = &fup->srbuf;
			sr->sr_type = attic ? SR_FILEDEAD : SR_FILELIVE;
			sr->sr_file = xstrdup(name);
			sr->sr_serverattr = fattr_decode(attr);
			if (sr->sr_serverattr == NULL)
				return (UPDATER_ERR_PROTO);
			lprintf(1, " Fixup %s\n", name);
			error = updater_addfile(up, fup, attr, 1);
			if (error)
				return (error);
			break;
		case 'Z':
			name = proto_get_ascii(&line);
			attr = proto_get_ascii(&line);
			pos  = proto_get_ascii(&line);
			if (name == NULL || attr == NULL || pos == NULL ||
			    line != NULL)
				return (UPDATER_ERR_PROTO);
			error = fup_prepare(fup, name, 0);
			fup->temppath = tempname(fup->destpath);
			sr = &fup->srbuf;
			sr->sr_type = SR_FILELIVE;
			sr->sr_file = xstrdup(name);
			sr->sr_serverattr = fattr_decode(attr);
			if (sr->sr_serverattr == NULL)
				return (UPDATER_ERR_PROTO);
			position = strtol(pos, NULL, 10);
			lprintf(1, " Append to %s\n", name);
			error = updater_append_file(up, fup, position);
			if (error)
				return (error);
			break;
		case '!':
			/* Warning from server. */
			msg = proto_get_rest(&line);
			if (msg == NULL)
				return (UPDATER_ERR_PROTO);
			lprintf(-1, "Server warning: %s\n", msg);
			break;
		default:
			return (UPDATER_ERR_PROTO);
		}
		fup_cleanup(fup);
	}
	if (line == NULL)
		return (UPDATER_ERR_READ);
	return (0);
}

/* Delete file. */
static int
updater_delete(struct updater *up, struct file_update *fup)
{
	struct config *config;
	struct coll *coll;

	config = up->config;
	coll = fup->coll;
	if (coll->co_options & CO_DELETE) {
		lprintf(1, " Delete %s\n", fup->coname);
		if (config->deletelim >= 0 &&
		    up->deletecount >= config->deletelim)
			return (UPDATER_ERR_DELETELIM);
		up->deletecount++;
		updater_deletefile(fup->destpath);
		if (coll->co_options & CO_CHECKOUTMODE)
			updater_prunedirs(coll->co_prefix, fup->destpath);
	} else {
		lprintf(1," NoDelete %s\n", fup->coname);
	}
	return (0);
}

static void
updater_deletefile(const char *path)
{
	int error;

	error = fattr_delete(path);
	if (error && errno != ENOENT) {
		lprintf(-1, "Cannot delete \"%s\": %s\n",
		    path, strerror(errno));
	}
}

static int
updater_setattrs(struct updater *up, struct file_update *fup, char *name,
    char *tag, char *date, char *revnum, char *revdate, struct fattr *rcsattr)
{
	struct statusrec sr;
	struct status *st;
	struct coll *coll;
	struct fattr *fileattr, *fa;
	char *path;
	int error, rv;

	coll = fup->coll;
	st = fup->st;
	path = fup->destpath;

	fileattr = fattr_frompath(path, FATTR_NOFOLLOW);
	if (fileattr == NULL) {
		/* The file has vanished. */
		error = status_delete(st, name, 0);
		if (error) {
			up->errmsg = status_errmsg(st);
			return (UPDATER_ERR_MSG);
		}
		return (0);
	}
	fa = fattr_forcheckout(rcsattr, coll->co_umask);
	fattr_override(fileattr, fa, FA_MASK);
	fattr_free(fa);

	rv = fattr_install(fileattr, path, NULL);
	if (rv == -1) {
		lprintf(1, " SetAttrs %s\n", fup->coname);
		fattr_free(fileattr);
		xasprintf(&up->errmsg, "Cannot set attributes for \"%s\": %s",
		    path, strerror(errno));
		return (UPDATER_ERR_MSG);
	}
	if (rv == 1) {
		lprintf(1, " SetAttrs %s\n", fup->coname);
		fattr_free(fileattr);
		fileattr = fattr_frompath(path, FATTR_NOFOLLOW);
		if (fileattr == NULL) {
			/* We're being very unlucky. */
			error = status_delete(st, name, 0);
			if (error) {
				up->errmsg = status_errmsg(st);
				return (UPDATER_ERR_MSG);
			}
			return (0);
		}
	}

	fattr_maskout(fileattr, FA_COIGNORE);

	sr.sr_type = SR_CHECKOUTLIVE;
	sr.sr_file = name;
	sr.sr_tag = tag;
	sr.sr_date = date;
	sr.sr_revnum = revnum;
	sr.sr_revdate = revdate;
	sr.sr_clientattr = fileattr;
	sr.sr_serverattr = rcsattr;

	error = status_put(st, &sr);
	fattr_free(fileattr);
	if (error) {
		up->errmsg = status_errmsg(st);
		return (UPDATER_ERR_MSG);
	}
	return (0);
}

static int
updater_updatefile(struct updater *up, struct file_update *fup,
    const char *md5, int isfixup)
{
	struct coll *coll;
	struct status *st;
	struct statusrec *sr;
	struct fattr *fileattr;
	int error, rv;

	coll = fup->coll;
	sr = &fup->srbuf;
	st = fup->st;

	if (strcmp(fup->wantmd5, md5) != 0) {
		if (isfixup) {
			lprintf(-1, "%s: Checksum mismatch -- "
			    "file not updated\n", fup->destpath);
		} else {
			lprintf(-1, "%s: Checksum mismatch -- "
			    "will transfer entire file\n", fup->destpath);
			fixups_put(up->config->fixups, fup->coll, sr->sr_file);
		}
		if (coll->co_options & CO_KEEPBADFILES)
			lprintf(-1, "Bad version saved in %s\n", fup->temppath);
		else
			updater_deletefile(fup->temppath);
		return (0);
	}

	fattr_umask(sr->sr_clientattr, coll->co_umask);
	rv = fattr_install(sr->sr_clientattr, fup->destpath, fup->temppath);
	if (rv == -1) {
		xasprintf(&up->errmsg, "Cannot install \"%s\" to \"%s\": %s",
		    fup->temppath, fup->destpath, strerror(errno));
		return (UPDATER_ERR_MSG);
	}

	/* XXX Executes */
	/*
	 * We weren't necessarily able to set all the file attributes to the
	 * desired values, and any executes may have altered the attributes.
	 * To make sure we record the actual attribute values, we fetch
	 * them from the file.
	 *
	 * However, we preserve the link count as received from the
	 * server.  This is important for preserving hard links in mirror
	 * mode.
	 */
	fileattr = fattr_frompath(fup->destpath, FATTR_NOFOLLOW);
	if (fileattr == NULL) {
		xasprintf(&up->errmsg, "Cannot stat \"%s\": %s", fup->destpath,
		    strerror(errno));
		return (UPDATER_ERR_MSG);
	}
	fattr_override(fileattr, sr->sr_clientattr, FA_LINKCOUNT);
	fattr_free(sr->sr_clientattr);
	sr->sr_clientattr = fileattr;

	/*
	 * To save space, don't write out the device and inode unless
	 * the link count is greater than 1.  These attributes are used
	 * only for detecting hard links.  If the link count is 1 then we
	 * know there aren't any hard links.
	 */
	if (!(fattr_getmask(sr->sr_clientattr) & FA_LINKCOUNT) ||
	    fattr_getlinkcount(sr->sr_clientattr) <= 1)
		fattr_maskout(sr->sr_clientattr, FA_DEV | FA_INODE);

	if (coll->co_options & CO_CHECKOUTMODE)
		fattr_maskout(sr->sr_clientattr, FA_COIGNORE);

	error = status_put(st, sr);
	if (error) {
		up->errmsg = status_errmsg(st);
		return (UPDATER_ERR_MSG);
	}
	return (0);
}

/*
 * Update attributes of a directory.
 */
static int
updater_setdirattrs(struct updater *up, struct coll *coll,
    struct file_update *fup, char *name, char *attr)
{
	struct statusrec *sr;
	struct fattr *fa;
	int error, rv;

	sr = &fup->srbuf;
	sr->sr_type = SR_DIRUP;
	sr->sr_file = xstrdup(name);
	sr->sr_clientattr = fattr_decode(attr);
	sr->sr_serverattr = fattr_decode(attr);
	if (sr->sr_clientattr == NULL || sr->sr_serverattr == NULL) 
		return (UPDATER_ERR_PROTO);
	fattr_mergedefault(sr->sr_clientattr);
	fattr_umask(sr->sr_clientattr, coll->co_umask);
	rv = fattr_install(sr->sr_clientattr, fup->destpath, NULL);
	lprintf(1, " SetAttrs %s\n", name);
	if (rv == -1) {
		xasprintf(&up->errmsg, "Cannot install \"%s\" to \"%s\": %s",
		    fup->temppath, fup->destpath, strerror(errno));
		return (UPDATER_ERR_MSG);
	}
	/*
	 * Now, make sure they were set and record what was set in the status
	 * file.
	 */
	fa = fattr_frompath(fup->destpath, FATTR_NOFOLLOW);
	if (fa == NULL) {
		xasprintf(&up->errmsg, "Cannot open \%s\": %s", fup->destpath,
		    strerror(errno));
		return (UPDATER_ERR_MSG);
	}
	fattr_free(sr->sr_clientattr);
	fattr_maskout(fa, FA_FLAGS);
	sr->sr_clientattr = fa;
	error = status_put(fup->st, sr);
	if (error) {
		up->errmsg = status_errmsg(fup->st);
		return (UPDATER_ERR_MSG);
	}

	return (0);
}

static int
updater_diff(struct updater *up, struct file_update *fup)
{
	char md5[MD5_DIGEST_SIZE];
	struct coll *coll;
	struct statusrec *sr;
	struct fattr *fa, *tmp;
	char *author, *path, *revnum, *revdate;
	char *line, *cmd;
	int error;

	coll = fup->coll;
	sr = &fup->srbuf;
	path = fup->destpath;

	lprintf(1, " Edit %s\n", fup->coname);
	while ((line = stream_getln(up->rd, NULL)) != NULL) {
		if (strcmp(line, ".") == 0)
			break;
		cmd = proto_get_ascii(&line);
		if (cmd == NULL || strcmp(cmd, "D") != 0)
			return (UPDATER_ERR_PROTO);
		revnum = proto_get_ascii(&line);
		proto_get_ascii(&line); /* XXX - diffbase */
		revdate = proto_get_ascii(&line);
		author = proto_get_ascii(&line);
		if (author == NULL || line != NULL)
			return (UPDATER_ERR_PROTO);
		if (sr->sr_revnum != NULL)
			free(sr->sr_revnum);
		if (sr->sr_revdate != NULL)
			free(sr->sr_revdate);
		if (fup->author != NULL)
			free(fup->author);
		sr->sr_revnum = xstrdup(revnum);
		sr->sr_revdate = xstrdup(revdate);
		fup->author = xstrdup(author);
		if (fup->orig == NULL) {
			/* First patch, the "origin" file is the one we have. */
			fup->orig = stream_open_file(path, O_RDONLY);
			if (fup->orig == NULL) {
				xasprintf(&up->errmsg, "%s: Cannot open: %s",
				    path, strerror(errno));
				return (UPDATER_ERR_MSG);
			}
		} else {
			/* Subsequent patches. */
			stream_close(fup->orig);
			fup->orig = fup->to;
			stream_rewind(fup->orig);
			unlink(fup->temppath);
			free(fup->temppath);
			fup->temppath = tempname(path);
		}
		fup->to = stream_open_file(fup->temppath,
		    O_RDWR | O_CREAT | O_TRUNC, 0600);
		if (fup->to == NULL) {
			xasprintf(&up->errmsg, "%s: Cannot open: %s",
			    fup->temppath, strerror(errno));
			return (UPDATER_ERR_MSG);
		}
		lprintf(2, "  Add delta %s %s %s\n", sr->sr_revnum,
		    sr->sr_revdate, fup->author);
		error = updater_diff_batch(up, fup);
		if (error)
			return (error);
	}
	if (line == NULL)
		return (UPDATER_ERR_READ);

	fa = fattr_frompath(path, FATTR_FOLLOW);
	tmp = fattr_forcheckout(sr->sr_serverattr, coll->co_umask);
	fattr_override(fa, tmp, FA_MASK);
	fattr_free(tmp);
	fattr_maskout(fa, FA_MODTIME);
	sr->sr_clientattr = fa;

	if (MD5_File(fup->temppath, md5) == -1) {
		xasprintf(&up->errmsg,
		    "Cannot calculate checksum for \"%s\": %s",
		    path, strerror(errno));
		return (UPDATER_ERR_MSG);
	}
	error = updater_updatefile(up, fup, md5, 0);
	return (error);
}

/*
 * Edit a file and add delta.
 */
static int
updater_diff_batch(struct updater *up, struct file_update *fup)
{
	struct stream *rd;
	char *cmd, *line, *state, *tok;
	int error;

	state = NULL;
	rd = up->rd;
	while ((line = stream_getln(rd, NULL)) != NULL) {
		if (strcmp(line, ".") == 0)
			break;
		cmd = proto_get_ascii(&line);
		if (cmd == NULL || strlen(cmd) != 1) {
			error = UPDATER_ERR_PROTO;
			goto bad;
		}
		switch (cmd[0]) {
		case 'L':
			line = stream_getln(rd, NULL);
			/* XXX - We're just eating the log for now. */
			while (line != NULL && strcmp(line, ".") != 0 &&
			    strcmp(line, ".+") != 0)
				line = stream_getln(rd, NULL);
			if (line == NULL) {
				error = UPDATER_ERR_READ;
				goto bad;
			}
			break;
		case 'S':
			tok = proto_get_ascii(&line);
			if (tok == NULL || line != NULL) {
				error = UPDATER_ERR_PROTO;
				goto bad;
			}
			if (state != NULL)
				free(state);
			state = xstrdup(tok);
			break;
		case 'T':
			error = updater_diff_apply(up, fup, state);
			if (error)
				goto bad;
			break;
		default:
			error = UPDATER_ERR_PROTO;
			goto bad;
		}
	}
	if (line == NULL) {
		error = UPDATER_ERR_READ;
		goto bad;
	}
	if (state != NULL)
		free(state);
	return (0);
bad:
	if (state != NULL)
		free(state);
	return (error);
}

int
updater_diff_apply(struct updater *up, struct file_update *fup, char *state)
{
	struct diffinfo dibuf, *di;
	struct coll *coll;
	struct statusrec *sr;
	int error;

	coll = fup->coll;
	sr = &fup->srbuf;
	di = &dibuf;

	di->di_rcsfile = sr->sr_file;
	di->di_cvsroot = coll->co_cvsroot;
	di->di_revnum = sr->sr_revnum;
	di->di_revdate = sr->sr_revdate;
	di->di_author = fup->author;
	di->di_tag = sr->sr_tag;
	di->di_state = state;
	di->di_expand = fup->expand;

	error = diff_apply(up->rd, fup->orig, fup->to, coll->co_keyword, di, 1);
	if (error) {
		/* XXX Bad error message */
		xasprintf(&up->errmsg, "Bad diff from server");
		return (UPDATER_ERR_MSG);
	}
	return (0);
}

/* Update or create a node. */
static int
updater_updatenode(struct updater *up, struct coll *coll,
    struct file_update *fup, char *name, char *attr)
{
	struct fattr *fa, *fileattr;
	struct status *st;
	struct statusrec *sr;
	int error, rv;

	sr = &fup->srbuf;
	st = fup->st;
	fa = fattr_decode(attr);

	if (fattr_type(fa) == FT_SYMLINK) {
		lprintf(1, " Symlink %s -> %s\n", name, 
		    fattr_getlinktarget(fa));
	} else {
		lprintf(1, " Mknod %s\n", name);
	}

	/* Create directory. */
	error = mkdirhier(fup->destpath, coll->co_umask);
	if (error)
		return (UPDATER_ERR_PROTO);

	/* If it does not exist, create it. */
	if (access(fup->destpath, F_OK) != 0)
		fattr_makenode(fa, fup->destpath);

	/*
	 * Coming from attic? I don't think this is a problem since we have
	 * determined attic before we call this function (Look at UpdateNode in
	 * cvsup).
	 */
	fattr_umask(fa, coll->co_umask);
	rv = fattr_install(fa, fup->destpath, fup->temppath);
	if (rv == -1) {
		xasprintf(&up->errmsg, "Cannot update attributes on "
	    "\"%s\": %s", fup->destpath, strerror(errno));
		return (UPDATER_ERR_MSG);
	}
	/*
	 * XXX: Executes not implemented. Have not encountered much use for it
	 * yet.
	 */
	/*
	 * We weren't necessarily able to set all the file attributes to the
	 * desired values, and any executes may have altered the attributes.
	 * To make sure we record the actual attribute values, we fetch
	 * them from the file.
	 *
	 * However, we preserve the link count as received from the
	 * server.  This is important for preserving hard links in mirror
	 * mode.
	 */
	fileattr = fattr_frompath(fup->destpath, FATTR_NOFOLLOW);
	if (fileattr == NULL) {
		xasprintf(&up->errmsg, "Cannot stat \"%s\": %s", fup->destpath,
		    strerror(errno));
		return (UPDATER_ERR_MSG);
	}
	fattr_override(fileattr, sr->sr_clientattr, FA_LINKCOUNT);
	fattr_free(sr->sr_clientattr);
	sr->sr_clientattr = fileattr;

	/*
	 * To save space, don't write out the device and inode unless
	 * the link count is greater than 1.  These attributes are used
	 * only for detecting hard links.  If the link count is 1 then we
	 * know there aren't any hard links.
	 */
	if (!(fattr_getmask(sr->sr_clientattr) & FA_LINKCOUNT) ||
	    fattr_getlinkcount(sr->sr_clientattr) <= 1)
		fattr_maskout(sr->sr_clientattr, FA_DEV | FA_INODE);

	/* If it is a symlink, write only out it's path. */
	if (fattr_type(fa) == FT_SYMLINK) {
		fattr_maskout(sr->sr_clientattr, ~(FA_FILETYPE |
		    FA_LINKTARGET));
	}
	fattr_maskout(sr->sr_clientattr, FA_FLAGS);
	error = status_put(st, sr);
	if (error) {
		up->errmsg = status_errmsg(st);
		return (UPDATER_ERR_MSG);
	}
	fattr_free(fa);

	return (0);
}

/*
 * Fetches a new file in CVS mode.
 */
static int
updater_addfile(struct updater *up, struct file_update *fup, char *attr,
    int isfixup)
{
	struct coll *coll;
	struct stream *to;
	struct statusrec *sr;
	struct fattr *fa;
	char buf[BUFSIZE];
	char md5[MD5_DIGEST_SIZE];
	ssize_t nread;
	off_t fsize, remains;
	char *cmd, *line, *path;
	int error; 

	coll = fup->coll;
	path = fup->destpath;
	sr = &fup->srbuf;
	fa = fattr_decode(attr);
	fsize = fattr_filesize(fa);

	error = mkdirhier(path, coll->co_umask);
	if (error)
		return (UPDATER_ERR_PROTO);
	to = stream_open_file(fup->temppath, O_WRONLY | O_CREAT | O_TRUNC, 0755);
	if (to == NULL) {
		xasprintf(&up->errmsg, "%s: Cannot create: %s",
		    fup->temppath, strerror(errno));
		return (UPDATER_ERR_MSG);
	}
	stream_filter_start(to, STREAM_FILTER_MD5, md5);
	remains = fsize;
	do {
		nread = stream_read(up->rd, buf, (BUFSIZE > remains ?
		    remains : BUFSIZE));
		if (nread == -1)
			return (UPDATER_ERR_PROTO);
		remains -= nread;
		if (stream_write(to, buf, nread) == -1)
			goto bad;
	} while (remains > 0);
	stream_close(to);
	line = stream_getln(up->rd, NULL);
	if (line == NULL)
		return (UPDATER_ERR_PROTO);
	/* Check for EOF. */
	if (!(*line == '.' || (strncmp(line, ".<", 2) != 0)))
		return (UPDATER_ERR_PROTO);
	line = stream_getln(up->rd, NULL);
	if (line == NULL)
		return (UPDATER_ERR_PROTO);

	cmd = proto_get_ascii(&line);
	fup->wantmd5 = proto_get_ascii(&line);
	if (fup->wantmd5 == NULL || line != NULL || strcmp(cmd, "5") != 0)
		return (UPDATER_ERR_PROTO);

	sr->sr_clientattr = fattr_frompath(fup->temppath, FATTR_NOFOLLOW);
	if (sr->sr_clientattr == NULL)
		return (UPDATER_ERR_PROTO);
	fattr_override(sr->sr_clientattr, sr->sr_serverattr,
	    FA_MODTIME | FA_MASK);
	error = updater_updatefile(up, fup, md5, isfixup);
	fup->wantmd5 = NULL;	/* So that it doesn't get freed. */
	return (error);
bad:
	xasprintf(&up->errmsg, "%s: Cannot write: %s", fup->temppath,
	    strerror(errno));
	return (UPDATER_ERR_MSG);
}

static int
updater_checkout(struct updater *up, struct file_update *fup, int isfixup)
{
	char md5[MD5_DIGEST_SIZE];
	struct statusrec *sr;
	struct coll *coll;
	struct stream *to;
	ssize_t nbytes;
	size_t size;
	char *cmd, *path, *line;
	int error, first;

	coll = fup->coll;
	sr = &fup->srbuf;
	path = fup->destpath;

	if (isfixup)
		lprintf(1, " Fixup %s\n", fup->coname);
	else
		lprintf(1, " Checkout %s\n", fup->coname);
	error = mkdirhier(path, coll->co_umask);
	if (error) {
		xasprintf(&up->errmsg,
		    "Cannot create directories leading to \"%s\": %s",
		    path, strerror(errno));
		return (UPDATER_ERR_MSG);
	}

	to = stream_open_file(fup->temppath,
	    O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (to == NULL) {
		xasprintf(&up->errmsg, "%s: Cannot create: %s",
		    fup->temppath, strerror(errno));
		return (UPDATER_ERR_MSG);
	}
	stream_filter_start(to, STREAM_FILTER_MD5, md5);
	line = stream_getln(up->rd, &size);
	first = 1;
	while (line != NULL) {
		if (line[size - 1] == '\n')
			size--;
	       	if ((size == 1 && *line == '.') ||
		    (size == 2 && memcmp(line, ".+", 2) == 0))
			break;
		if (size >= 2 && memcmp(line, "..", 2) == 0) {
			size--;
			line++;
		}
		if (!first) {
			nbytes = stream_write(to, "\n", 1);
			if (nbytes == -1)
				goto bad;
		}
		nbytes = stream_write(to, line, size);
		if (nbytes == -1)
			goto bad;
		line = stream_getln(up->rd, &size);
		first = 0;
	}
	if (line == NULL) {
		stream_close(to);
		return (UPDATER_ERR_READ);
	}
	if (size == 1 && *line == '.') {
		nbytes = stream_write(to, "\n", 1);
		if (nbytes == -1)
			goto bad;
	}
	stream_close(to);
	/* Get the checksum line. */
	line = stream_getln(up->rd, NULL);
	if (line == NULL)
		return (UPDATER_ERR_READ);
	cmd = proto_get_ascii(&line);
	fup->wantmd5 = proto_get_ascii(&line);
	if (fup->wantmd5 == NULL || line != NULL || strcmp(cmd, "5") != 0)
		return (UPDATER_ERR_PROTO);
	error = updater_updatefile(up, fup, md5, isfixup);
	fup->wantmd5 = NULL;	/* So that it doesn't get freed. */
	if (error)
		return (error);
	return (0);
bad:
	xasprintf(&up->errmsg, "%s: Cannot write: %s", fup->temppath,
	    strerror(errno));
	return (UPDATER_ERR_MSG);
}

/*
 * Remove all empty directories below file.
 * This function will trash the path passed to it.
 */
static void
updater_prunedirs(char *base, char *file)
{
	char *cp;
	int error;

	while ((cp = strrchr(file, '/')) != NULL) {
		*cp = '\0';
		if (strcmp(base, file) == 0)
			return;
		error = rmdir(file);
		if (error)
			return;
	}
}

/*
 * Edit an RCS file.
 */
static int
updater_rcsedit(struct updater *up, struct file_update *fup, char *name,
    char *rcsopt)
{
	struct coll *coll;
	struct stream *dest;
	struct statusrec *sr;
	struct status *st;
	struct rcsfile *rf;
	struct fattr *oldfattr;
	char md5[MD5_DIGEST_SIZE];
	char *branch, *cmd, *expand, *line, *path, *revnum, *tag, *temppath;
	int error;

	coll = fup->coll;
	sr = &fup->srbuf;
	st = fup->st;
	temppath = fup->temppath;
	path = fup->origpath != NULL ? fup->origpath : fup->destpath;
	error = 0;

	/* If the path is new, we must create the Attic dir if needed. */
	if (fup->origpath != NULL) {
		error = mkdirhier(fup->destpath, coll->co_umask);
		if (error) {
			xasprintf(&up->errmsg, "Unable to create Attic dir for "
			    "%s\n", fup->origpath);
			return (UPDATER_ERR_MSG);
		}
	}
	/*
	 * XXX: we could avoid parsing overhead if we're reading ahead before we
	 * parse the file.
	 */
	oldfattr = fattr_frompath(path, FATTR_NOFOLLOW);
	if (oldfattr == NULL) {
		xasprintf(&up->errmsg, "%s: Cannot get attributes: %s", path,
		    strerror(errno));
		return (UPDATER_ERR_MSG);
	}
	fattr_merge(sr->sr_serverattr, oldfattr);
	rf = NULL;

	/* Macro for making touching an RCS file faster. */
#define UPDATER_OPENRCS(rf, up, path, name, cvsroot, tag) do {		\
	if ((rf) == NULL) {						\
		lprintf(1, " Edit %s", fup->coname);			\
		if (fup->attic)						\
			lprintf(1, " -> Attic");			\
		lprintf(1, "\n");					\
		(rf) = rcsfile_frompath((path), (name), (cvsroot),	\
		    (tag), 0);						\
		if ((rf) == NULL) {					\
			xasprintf(&(up)->errmsg,			\
			    "Error reading rcsfile %s\n", (name));	\
			return (UPDATER_ERR_MSG);			\
		}							\
	}								\
} while (0)

	while ((line = stream_getln(up->rd, NULL)) != NULL) {
		if (strcmp(line, ".") == 0)
			break;
		cmd = proto_get_ascii(&line);
		if (cmd == NULL) {
			lprintf(-1, "Error editing %s\n", name);
			return (UPDATER_ERR_PROTO);
		}
		switch(cmd[0]) {
			case 'B':
				branch = proto_get_ascii(&line);
				if (branch == NULL || line != NULL)
					return (UPDATER_ERR_PROTO);
				UPDATER_OPENRCS(rf, up, path, name,
				    coll->co_cvsroot, coll->co_tag);
				break;
			case 'b':
				UPDATER_OPENRCS(rf, up, path, name,
				    coll->co_cvsroot, coll->co_tag);
				rcsfile_setval(rf, RCSFILE_BRANCH, NULL);
				break;
			case 'D':
				UPDATER_OPENRCS(rf, up, path, name,
				    coll->co_cvsroot, coll->co_tag);
				error = updater_addelta(rf, up->rd, line);
				if (error)
					return (error);
				break;
			case 'd':
				revnum = proto_get_ascii(&line);
				if (revnum == NULL || line != NULL)
					return (UPDATER_ERR_PROTO);
				UPDATER_OPENRCS(rf, up, path, name,
				    coll->co_cvsroot, coll->co_tag);
				rcsfile_deleterev(rf, revnum);
				break;
			case 'E':
				expand = proto_get_ascii(&line);
				if (expand == NULL || line != NULL)
					return (UPDATER_ERR_PROTO);
				UPDATER_OPENRCS(rf, up, path, name,
				    coll->co_cvsroot, coll->co_tag);
				rcsfile_setval(rf, RCSFILE_EXPAND, expand);
				break;
			case 'T':
				tag = proto_get_ascii(&line);
				revnum = proto_get_ascii(&line);
				if (tag == NULL || revnum == NULL ||
				    line != NULL)
					return (UPDATER_ERR_PROTO);
				UPDATER_OPENRCS(rf, up, path, name,
				    coll->co_cvsroot, coll->co_tag);
				rcsfile_addtag(rf, tag, revnum);
				break;
			case 't':
				tag = proto_get_ascii(&line);
				revnum = proto_get_ascii(&line);
				if (tag == NULL || revnum == NULL ||
				    line != NULL)
					return (UPDATER_ERR_PROTO);
				UPDATER_OPENRCS(rf, up, path, name,
				    coll->co_cvsroot, coll->co_tag);
				rcsfile_deletetag(rf, tag, revnum);
				break;
			default:
				return (UPDATER_ERR_PROTO);
		}
	}

	if (rf == NULL) {
		fattr_maskout(oldfattr, ~FA_MODTIME);
		if (fattr_equal(oldfattr, sr->sr_serverattr))
		 	lprintf(1, " SetAttrs %s", fup->coname);
		else
			lprintf(1, " Touch %s", fup->coname);
		/* Install new attributes. */
		fattr_umask(sr->sr_serverattr, coll->co_umask);
		fattr_install(sr->sr_serverattr, fup->destpath, NULL);
		if (fup->attic)
			lprintf(1, " -> Attic");
		lprintf(1, "\n");
		fattr_free(oldfattr);
		goto finish;
	}

	/* Write and rename temp file. */
	dest = stream_open_file(fup->temppath,
	    O_RDWR | O_CREAT | O_TRUNC, 0600);
	if (dest == NULL) {
		xasprintf(&up->errmsg, "Error opening file %s for writing: %s\n", 
		    fup->temppath, strerror(errno));
		return (UPDATER_ERR_MSG);
	}
	stream_filter_start(dest, STREAM_FILTER_MD5RCS, md5);
	error = rcsfile_write(rf, dest);
	stream_close(dest);
	rcsfile_free(rf);
	if (error) {
		xasprintf(&up->errmsg, "%s: Cannot write: %s", fup->temppath,
		    strerror(errno));
		return (UPDATER_ERR_MSG);
	}

finish:
	sr->sr_clientattr = fattr_frompath(path, FATTR_NOFOLLOW);
	if (sr->sr_clientattr == NULL) {
		xasprintf(&up->errmsg, "%s: Cannot get attributes: %s",
		    fup->destpath, strerror(errno));
		return (UPDATER_ERR_MSG);
	}
	fattr_override(sr->sr_clientattr, sr->sr_serverattr,
	    FA_MODTIME | FA_MASK);
	if (rf != NULL) {
		error = updater_updatefile(up, fup, md5, 0);
		fup->wantmd5 = NULL;	/* So that it doesn't get freed. */
		if (error)
			return (error);
	} else {
		/* Record its attributes since we touched it. */
		if (!(fattr_getmask(sr->sr_clientattr) & FA_LINKCOUNT) ||
		    fattr_getlinkcount(sr->sr_clientattr) <= 1)
		fattr_maskout(sr->sr_clientattr, FA_DEV | FA_INODE);
		error = status_put(st, sr);
		if (error) {
			up->errmsg = status_errmsg(st);
			return (UPDATER_ERR_MSG);
		}
	}

	/* In this case, we need to remove the old file afterwards. */
	/* XXX: Can we be sure that a file not edited is moved? I don't think
	 * this is a problem, since if a file is moved, it should be edited to
	 * show if it's dead or not.
	 */
	if (fup->origpath != NULL)
		updater_deletefile(fup->origpath);
	return (0);
}

/*
 * Add a delta to a RCS file.
 */
int
updater_addelta(struct rcsfile *rf, struct stream *rd, char *cmdline)
{
	struct delta *d;
	size_t size;
	char *author, *cmd, *diffbase, *line, *logline;
	char *revdate, *revnum, *state, *textline;

	revnum = proto_get_ascii(&cmdline);
	diffbase = proto_get_ascii(&cmdline);
	revdate = proto_get_ascii(&cmdline);
	author = proto_get_ascii(&cmdline);
	size = 0;

	if (revnum == NULL || revdate == NULL || author == NULL)
		return (UPDATER_ERR_PROTO);

	/* First add the delta so we have it. */
	d = rcsfile_addelta(rf, revnum, revdate, author, diffbase);
	if (d == NULL) {
		lprintf(-1, "Error adding delta %s\n", revnum);
		return (UPDATER_ERR_READ);
	}
	while ((line = stream_getln(rd, NULL)) != NULL) {
		if (strcmp(line, ".") == 0)
			break;
		cmd = proto_get_ascii(&line);
		switch (cmd[0]) {
			case 'L':
				/* Do the same as in 'C' command. */
				logline = stream_getln(rd, &size);
				while (logline != NULL) {
					if (size == 2 && *logline == '.')
						break;
					if (size == 3 && 
					    memcmp(logline, ".+", 2) == 0) {
						rcsdelta_truncatelog(d, -1);
						break;
					}
					if (size >= 3 &&
					    memcmp(logline, "..", 2) == 0) {
						size--;
						logline++;
					}
					if (rcsdelta_appendlog(d, logline, size)
					    < 0)
						return (-1);
					logline = stream_getln(rd, &size);
				}
			break;
			case 'N':
			case 'n':
				/* XXX: Not supported. */
			break;
			case 'S':
				state = proto_get_ascii(&line);
				if (state == NULL)
					return (UPDATER_ERR_PROTO);
				rcsdelta_setstate(d, state);
			break;
			case 'T':
				/* Do the same as in 'C' command. */
				textline = stream_getln(rd, &size);
				while (textline != NULL) {
					if (size == 2 && *textline == '.')
						break;
					if (size == 3 &&
					    memcmp(textline, ".+", 2) == 0) {
						/* Truncate newline. */
						rcsdelta_truncatetext(d, -1);
						break;
					}
					if (size >= 3 &&
					    memcmp(textline, "..", 2) == 0) {
						size--;
						textline++;
					}
					if (rcsdelta_appendtext(d, textline,
					    size) < 0)
						return (-1);
					textline = stream_getln(rd, &size);
				}
			break;
		}
	}

	return (0);
}

int
updater_append_file(struct updater *up, struct file_update *fup, off_t pos)
{
	struct fattr *fa;
	struct stream *to;
	struct statusrec *sr;
	ssize_t nread;
	off_t bytes;
	char buf[BUFSIZE], md5[MD5_DIGEST_SIZE];
	char *line, *cmd;
	int error, fd;

	sr = &fup->srbuf;
	fa = sr->sr_serverattr;
	to = stream_open_file(fup->temppath, O_WRONLY | O_CREAT | O_TRUNC,
	    0755);
	if (to == NULL) {
		xasprintf(&up->errmsg, "%s: Cannot open: %s", fup->temppath,
		    strerror(errno));
		return (UPDATER_ERR_MSG);
	}
	fd = open(fup->destpath, O_RDONLY);
	if (fd < 0) {
		xasprintf(&up->errmsg, "%s: Cannot open: %s", fup->destpath,
		    strerror(errno));
		return (UPDATER_ERR_MSG);
	}

	stream_filter_start(to, STREAM_FILTER_MD5, md5);
	/* First write the existing content. */
	while ((nread = read(fd, buf, BUFSIZE)) > 0) {
		if (stream_write(to, buf, nread) == -1)
			goto bad;
	}
	if (nread == -1) {
		xasprintf(&up->errmsg, "%s: Error reading: %s", fup->destpath,
		    strerror(errno));
		return (UPDATER_ERR_MSG);
	}
	close(fd);

	bytes = fattr_filesize(fa) - pos;
	/* Append the new data. */
	do {
		nread = stream_read(up->rd, buf,
		    (BUFSIZE > bytes) ? bytes : BUFSIZE);
		if (nread == -1)
			return (UPDATER_ERR_PROTO);
		bytes -= nread;
		if (stream_write(to, buf, nread) == -1)
			goto bad;
	} while (bytes > 0);
	stream_close(to);

	line = stream_getln(up->rd, NULL);
	if (line == NULL)
		return (UPDATER_ERR_PROTO);
	/* Check for EOF. */
	if (!(*line == '.' || (strncmp(line, ".<", 2) != 0)))
		return (UPDATER_ERR_PROTO);
	line = stream_getln(up->rd, NULL);
	if (line == NULL)
		return (UPDATER_ERR_PROTO);

	cmd = proto_get_ascii(&line);
	fup->wantmd5 = proto_get_ascii(&line);
	if (fup->wantmd5 == NULL || line != NULL || strcmp(cmd, "5") != 0)
		return (UPDATER_ERR_PROTO);

	sr->sr_clientattr = fattr_frompath(fup->destpath, FATTR_NOFOLLOW);
	if (sr->sr_clientattr == NULL)
		return (UPDATER_ERR_PROTO);
	fattr_override(sr->sr_clientattr, sr->sr_serverattr,
	    FA_MODTIME | FA_MASK);
	error = updater_updatefile(up, fup, md5, 0);
	fup->wantmd5 = NULL;	/* So that it doesn't get freed. */
	return (error);
bad:
	xasprintf(&up->errmsg, "%s: Cannot write: %s", fup->temppath,
	    strerror(errno));
	return (UPDATER_ERR_MSG);
}

/*
 * Read file data from stream of checkout commands, and write it to the
 * destination.
 */
static int
updater_read_checkout(struct stream *src, struct stream *dest)
{
	ssize_t nbytes;
	size_t size;
	char *line;
	int first;

	first = 1;
	line = stream_getln(src, &size);
	while (line != NULL) {
		if (line[size - 1] == '\n')
			size--;
		if ((size == 1 && *line == '.') ||
		    (size == 2 && strncmp(line, ".+", 2) == 0))
			break;
		if (size >= 2 && strncmp(line, "..", 2) == 0) {
			size--;
			line++;
		}
		if (!first) {
			nbytes = stream_write(dest, "\n", 1);
			if (nbytes == -1)
				return (UPDATER_ERR_MSG);
		}
		nbytes = stream_write(dest, line, size);
		if (nbytes == -1)
			return (UPDATER_ERR_MSG);
		line = stream_getln(src, &size);
		first = 0;
	}
	if (line == NULL)
		return (UPDATER_ERR_READ);
	if (size == 1 && *line == '.') {
		nbytes = stream_write(dest, "\n", 1);
		if (nbytes == -1)
			return (UPDATER_ERR_MSG);
	}
	return (0);
}

/* Update file using the rsync protocol. */
static int
updater_rsync(struct updater *up, struct file_update *fup, size_t blocksize)
{
	struct statusrec *sr;
	struct stream *to;
	char md5[MD5_DIGEST_SIZE];
	ssize_t nbytes;
	size_t blocknum, blockstart, blockcount;
	char *buf, *line;
	int error, orig;

	sr = &fup->srbuf;

	lprintf(1, " Rsync %s\n", fup->coname);
	/* First open all files that we are going to work on. */
	to = stream_open_file(fup->temppath, O_WRONLY | O_CREAT | O_TRUNC,
	    0600);
	if (to == NULL) {
		xasprintf(&up->errmsg, "%s: Cannot create: %s",
		    fup->temppath, strerror(errno));
		return (UPDATER_ERR_MSG);
	}
	orig = open(fup->destpath, O_RDONLY);
	if (orig < 0) {
		xasprintf(&up->errmsg, "%s: Cannot open: %s",
		    fup->destpath, strerror(errno));
		return (UPDATER_ERR_MSG);
	}
	stream_filter_start(to, STREAM_FILTER_MD5, md5);

	error = updater_read_checkout(up->rd, to);
	if (error) {
		xasprintf(&up->errmsg, "%s: Cannot write: %s", fup->temppath,
		    strerror(errno));
		return (error);
	}

	/* Buffer must contain blocksize bytes. */
	buf = xmalloc(blocksize);
	/* Done with the initial text, read and write chunks. */
	line = stream_getln(up->rd, NULL);
	while (line != NULL) {
		if (strcmp(line, ".") == 0)
			break;
		error = UPDATER_ERR_PROTO;
		if (proto_get_sizet(&line, &blockstart, 10) != 0)
			goto bad;
		if (proto_get_sizet(&line, &blockcount, 10) != 0)
			goto bad;
		/* Read blocks from original file. */
		lseek(orig, (blocksize * blockstart), SEEK_SET);
		error = UPDATER_ERR_MSG;
		for (blocknum = 0; blocknum < blockcount; blocknum++) {
			nbytes = read(orig, buf, blocksize);
			if (nbytes < 0) {
				xasprintf(&up->errmsg, "%s: Cannot read: %s",
				    fup->destpath, strerror(errno));
				goto bad;
			}
			nbytes = stream_write(to, buf, nbytes);
			if (nbytes == -1) {
				xasprintf(&up->errmsg, "%s: Cannot write: %s",
				    fup->temppath, strerror(errno));
				goto bad;
			}
		}
		/* Get the remaining text from the server. */
		error = updater_read_checkout(up->rd, to);
		if (error) {
			xasprintf(&up->errmsg, "%s: Cannot write: %s",
			    fup->temppath, strerror(errno));
			goto bad;
		}
		line = stream_getln(up->rd, NULL);
	}
	stream_close(to);
	close(orig);

	sr->sr_clientattr = fattr_frompath(fup->destpath, FATTR_NOFOLLOW);
	if (sr->sr_clientattr == NULL)
		return (UPDATER_ERR_PROTO);
	fattr_override(sr->sr_clientattr, sr->sr_serverattr,
	    FA_MODTIME | FA_MASK);

	error = updater_updatefile(up, fup, md5, 0);
	fup->wantmd5 = NULL;	/* So that it doesn't get freed. */
bad:
	free(buf);
	return (error);
}
