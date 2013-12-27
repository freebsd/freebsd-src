/*-
 * Copyright (c) 2006, Maxime Henrion <mux@FreeBSD.org>
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
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "fattr.h"
#include "misc.h"
#include "pathcomp.h"
#include "proto.h"
#include "queue.h"
#include "status.h"
#include "stream.h"

#define	STATUS_VERSION	5

/* Internal error codes. */
#define	STATUS_ERR_READ		(-1)
#define	STATUS_ERR_WRITE	(-2)
#define	STATUS_ERR_PARSE	(-3)
#define	STATUS_ERR_UNSORTED	(-4)
#define	STATUS_ERR_TRUNC	(-5)
#define	STATUS_ERR_BOGUS_DIRUP	(-6)
#define	STATUS_ERR_BAD_TYPE	(-7)
#define	STATUS_ERR_RENAME	(-8)

static struct status	*status_new(char *, time_t, struct stream *);
static struct statusrec	*status_rd(struct status *);
static struct statusrec	*status_rdraw(struct status *, char **);
static int		 status_wr(struct status *, struct statusrec *);
static int		 status_wrraw(struct status *, struct statusrec *,
			     char *);
static struct status	*status_fromrd(char *, struct stream *);
static struct status	*status_fromnull(char *);
static void		 status_free(struct status *);

static void		 statusrec_init(struct statusrec *);
static void		 statusrec_fini(struct statusrec *);
static int		 statusrec_cook(struct statusrec *, char *);
static int		 statusrec_cmp(struct statusrec *, struct statusrec *);

struct status {
	char *path;
	char *tempfile;
	int error;
	int suberror;
	struct pathcomp *pc;
	struct statusrec buf;
	struct statusrec *previous;
	struct statusrec *current;
	struct stream *rd;
	struct stream *wr;
	time_t scantime;
	int eof;
	int linenum;
	int depth;
	int dirty;
};

static void
statusrec_init(struct statusrec *sr)
{

	memset(sr, 0, sizeof(*sr));
}

static int
statusrec_cook(struct statusrec *sr, char *line)
{
	char *clientattr, *serverattr;

	switch (sr->sr_type) {
	case SR_FILEDEAD:
	case SR_FILELIVE:
		clientattr = proto_get_ascii(&line);
		if (clientattr == NULL || line != NULL)
			return (-1);
		sr->sr_clientattr = fattr_decode(clientattr);
		if (sr->sr_clientattr == NULL)
			return (-1);
		break;
	case SR_DIRDOWN:
		/* Nothing to do. */
		if (line != NULL)
			return (-1);
		break;
	case SR_CHECKOUTLIVE:
		sr->sr_tag = proto_get_ascii(&line);
		sr->sr_date = proto_get_ascii(&line);
		serverattr = proto_get_ascii(&line);
		sr->sr_revnum = proto_get_ascii(&line);
		sr->sr_revdate = proto_get_ascii(&line);
		clientattr = proto_get_ascii(&line);
		if (clientattr == NULL || line != NULL)
			return (-1);
		sr->sr_serverattr = fattr_decode(serverattr);
		if (sr->sr_serverattr == NULL)
			return (-1);
		sr->sr_clientattr = fattr_decode(clientattr);
		if (sr->sr_clientattr == NULL) {
			fattr_free(sr->sr_serverattr);
			return (-1);
		}
		break;
	case SR_CHECKOUTDEAD:
		sr->sr_tag = proto_get_ascii(&line);
		sr->sr_date = proto_get_ascii(&line);
		serverattr = proto_get_ascii(&line);
		if (serverattr == NULL || line != NULL)
			return (-1);
		sr->sr_serverattr = fattr_decode(serverattr);
		if (sr->sr_serverattr == NULL)
			return (-1);
		break;
	case SR_DIRUP:
		clientattr = proto_get_ascii(&line);
		if (clientattr == NULL || line != NULL)
			return (-1);
		sr->sr_clientattr = fattr_decode(clientattr);
		if (sr->sr_clientattr == NULL)
			return (-1);
		break;
	default:
		return (-1);
	}
	return (0);
}

static struct statusrec *
status_rd(struct status *st)
{
	struct statusrec *sr;
	char *line;
	int error;

	sr = status_rdraw(st, &line);
	if (sr == NULL)
		return (NULL);
	error = statusrec_cook(sr, line);
	if (error) {
		st->error = STATUS_ERR_PARSE;
		return (NULL);
	}
	return (sr);
}

static struct statusrec *
status_rdraw(struct status *st, char **linep)
{
	struct statusrec sr;
	char *cmd, *line, *file;

	if (st->rd == NULL || st->eof)
		return (NULL);
	line = stream_getln(st->rd, NULL);
	if (line == NULL) {
		if (stream_eof(st->rd)) {
			if (st->depth != 0) {
				st->error = STATUS_ERR_TRUNC;
				return (NULL);
			}
			st->eof = 1;
			return (NULL);
		}
		st->error = STATUS_ERR_READ;
		st->suberror = errno;
		return (NULL);
	}
	st->linenum++;
	cmd = proto_get_ascii(&line);
	file = proto_get_ascii(&line);
	if (file == NULL || strlen(cmd) != 1) {
		st->error = STATUS_ERR_PARSE;
		return (NULL);
	}

	switch (cmd[0]) {
	case 'A':
		sr.sr_type = SR_FILELIVE;
		break;
	case 'D':
		sr.sr_type = SR_DIRDOWN;
		st->depth++;
		break;
	case 'C':
		sr.sr_type = SR_CHECKOUTLIVE;
		break;
	case 'c':
		sr.sr_type = SR_CHECKOUTDEAD;
		break;
	case 'U':
		sr.sr_type = SR_DIRUP;
		if (st->depth <= 0) {
			st->error = STATUS_ERR_BOGUS_DIRUP;
			return (NULL);
		}
		st->depth--;
		break;
	case 'V':
		sr.sr_type = SR_FILELIVE;
		break;
	case 'v':
		sr.sr_type = SR_FILEDEAD;
		break;
	default:
		st->error = STATUS_ERR_BAD_TYPE;
		st->suberror = cmd[0];
		return (NULL);
	}

	sr.sr_file = xstrdup(file);
	if (st->previous != NULL &&
	    statusrec_cmp(st->previous, &sr) >= 0) {
		st->error = STATUS_ERR_UNSORTED;
		free(sr.sr_file);
		return (NULL);
	}

	if (st->previous == NULL) {
		st->previous = &st->buf;
	} else {
		statusrec_fini(st->previous);
		statusrec_init(st->previous);
	}
	st->previous->sr_type = sr.sr_type;
	st->previous->sr_file = sr.sr_file;
	*linep = line;
	return (st->previous);
}

static int
status_wr(struct status *st, struct statusrec *sr)
{
	struct pathcomp *pc;
	const struct fattr *fa;
	char *name;
	int error, type, usedirupattr;

	pc = st->pc;
	error = 0;
	usedirupattr = 0;
	if (sr->sr_type == SR_DIRDOWN) {
		pathcomp_put(pc, PC_DIRDOWN, sr->sr_file);
	} else if (sr->sr_type == SR_DIRUP) {
		pathcomp_put(pc, PC_DIRUP, sr->sr_file);
		usedirupattr = 1;
	} else {
		pathcomp_put(pc, PC_FILE, sr->sr_file);
	}

	while (pathcomp_get(pc, &type, &name)) {
		if (type == PC_DIRDOWN) {
			error = proto_printf(st->wr, "D %s\n", name);
		} else if (type == PC_DIRUP) {
			if (usedirupattr)
				fa = sr->sr_clientattr;
			else
				fa = fattr_bogus;
			usedirupattr = 0;
			error = proto_printf(st->wr, "U %s %f\n", name, fa);
		}
		if (error)
			goto bad;
	}

	switch (sr->sr_type) {
	case SR_DIRDOWN:
	case SR_DIRUP:
		/* Already emitted above. */
		break;
	case SR_CHECKOUTLIVE:
		error = proto_printf(st->wr, "C %s %s %s %f %s %s %f\n",
		    sr->sr_file, sr->sr_tag, sr->sr_date, sr->sr_serverattr,
		    sr->sr_revnum, sr->sr_revdate, sr->sr_clientattr);
		break;
	case SR_CHECKOUTDEAD:
		error = proto_printf(st->wr, "c %s %s %s %f\n", sr->sr_file,
		    sr->sr_tag, sr->sr_date, sr->sr_serverattr);
		break;
	case SR_FILELIVE:
		error = proto_printf(st->wr, "V %s %f\n", sr->sr_file,
		    sr->sr_clientattr);
		break;
	case SR_FILEDEAD:
		error = proto_printf(st->wr, "v %s %f\n", sr->sr_file,
		    sr->sr_clientattr);
		break;
	}
	if (error)
		goto bad;
	return (0);
bad:
	st->error = STATUS_ERR_WRITE;
	st->suberror = errno;
	return (-1);
}

static int
status_wrraw(struct status *st, struct statusrec *sr, char *line)
{
	char *name;
	char cmd;
	int error, ret, type;

	if (st->wr == NULL)
		return (0);

	/*
	 * Keep the compressor in sync.  At this point, the necessary
	 * DirDowns and DirUps should have already been emitted, so the
	 * compressor should return exactly one value in the PC_DIRDOWN
	 * and PC_DIRUP case and none in the PC_FILE case.
	 */
	if (sr->sr_type == SR_DIRDOWN)
		pathcomp_put(st->pc, PC_DIRDOWN, sr->sr_file);
	else if (sr->sr_type == SR_DIRUP)
		pathcomp_put(st->pc, PC_DIRUP, sr->sr_file);
	else
		pathcomp_put(st->pc, PC_FILE, sr->sr_file);
	if (sr->sr_type == SR_DIRDOWN || sr->sr_type == SR_DIRUP) {
		ret = pathcomp_get(st->pc, &type, &name);
		assert(ret);
		if (sr->sr_type == SR_DIRDOWN)
			assert(type == PC_DIRDOWN);
		else
			assert(type == PC_DIRUP);
	}
	ret = pathcomp_get(st->pc, &type, &name);
	assert(!ret);

	switch (sr->sr_type) {
	case SR_DIRDOWN:
		cmd = 'D';
		break;
	case SR_DIRUP:
		cmd = 'U';
		break;
	case SR_CHECKOUTLIVE:
		cmd = 'C';
		break;
	case SR_CHECKOUTDEAD:
		cmd = 'c';
		break;
	case SR_FILELIVE:
		cmd = 'V';
		break;
	case SR_FILEDEAD:
		cmd = 'v';
		break;
	default:
		assert(0);
		return (-1);
	}
	if (sr->sr_type == SR_DIRDOWN)
		error = proto_printf(st->wr, "%c %S\n", cmd, sr->sr_file);
	else
		error = proto_printf(st->wr, "%c %s %S\n", cmd, sr->sr_file,
		    line);
	if (error) {
		st->error = STATUS_ERR_WRITE;
		st->suberror = errno;
		return (-1);
	}
	return (0);
}

static void
statusrec_fini(struct statusrec *sr)
{

	fattr_free(sr->sr_serverattr);
	fattr_free(sr->sr_clientattr);
	free(sr->sr_file);
}

static int
statusrec_cmp(struct statusrec *a, struct statusrec *b)
{
	size_t lena, lenb;

	if (a->sr_type == SR_DIRUP || b->sr_type == SR_DIRUP) {
		lena = strlen(a->sr_file);
		lenb = strlen(b->sr_file);
		if (a->sr_type == SR_DIRUP &&
		    ((lena < lenb && b->sr_file[lena] == '/') || lena == lenb)
		    && strncmp(a->sr_file, b->sr_file, lena) == 0)
			return (1);
		if (b->sr_type == SR_DIRUP &&
		    ((lenb < lena && a->sr_file[lenb] == '/') || lenb == lena)
		    && strncmp(a->sr_file, b->sr_file, lenb) == 0)
			return (-1);
	}
	return (pathcmp(a->sr_file, b->sr_file));
}

static struct status *
status_new(char *path, time_t scantime, struct stream *file)
{
	struct status *st;

	st = xmalloc(sizeof(struct status));
	st->path = path;
	st->error = 0;
	st->suberror = 0;
	st->tempfile = NULL;
	st->scantime = scantime;
	st->rd = file;
	st->wr = NULL;
	st->previous = NULL;
	st->current = NULL;
	st->dirty = 0;
	st->eof = 0;
	st->linenum = 0;
	st->depth = 0;
	st->pc = pathcomp_new();
	statusrec_init(&st->buf);
	return (st);
}

static void
status_free(struct status *st)
{

	if (st->previous != NULL)
		statusrec_fini(st->previous);
	if (st->rd != NULL)
		stream_close(st->rd);
	if (st->wr != NULL)
		stream_close(st->wr);
	if (st->tempfile != NULL)
		free(st->tempfile);
	free(st->path);
	pathcomp_free(st->pc);
	free(st);
}

static struct status *
status_fromrd(char *path, struct stream *file)
{
	struct status *st;
	char *id, *line;
	time_t scantime;
	int error, ver;

	/* Get the first line of the file and validate it. */
	line = stream_getln(file, NULL);
	if (line == NULL) {
		stream_close(file);
		return (NULL);
	}
	id = proto_get_ascii(&line);
	error = proto_get_int(&line, &ver, 10);
	if (error) {
		stream_close(file);
		return (NULL);
	}
	error = proto_get_time(&line, &scantime);
	if (error || line != NULL) {
		stream_close(file);
		return (NULL);
	}

	if (strcmp(id, "F") != 0 || ver != STATUS_VERSION) {
		stream_close(file);
		return (NULL);
	}

	st = status_new(path, scantime, file);
	st->linenum = 1;
	return (st);
}

static struct status *
status_fromnull(char *path)
{
	struct status *st;

	st = status_new(path, -1, NULL);
	st->eof = 1;
	return (st);
}

/*
 * Open the status file.  If scantime is not -1, the file is opened
 * for updating, otherwise, it is opened read-only. If the status file
 * couldn't be opened, NULL is returned and errmsg is set to the error
 * message.
 */
struct status *
status_open(struct coll *coll, time_t scantime, char **errmsg)
{
	struct status *st;
	struct stream *file;
	struct fattr *fa;
	char *destpath, *path;
	int error, rv;

	path = coll_statuspath(coll);
	file = stream_open_file(path, O_RDONLY);
	if (file == NULL) {
		if (errno != ENOENT) {
			xasprintf(errmsg, "Could not open \"%s\": %s\n",
			    path, strerror(errno));
			free(path);
			return (NULL);
		}
		st = status_fromnull(path);
	} else {
		st = status_fromrd(path, file);
		if (st == NULL) {
			xasprintf(errmsg, "Error in \"%s\": Bad header line",
			    path);
			free(path);
			return (NULL);
		}
	}

	if (scantime != -1) {
		/* Open for writing too. */
		xasprintf(&destpath, "%s/%s/%s/", coll->co_base,
		    coll->co_colldir, coll->co_name);
		st->tempfile = tempname(destpath);
		if (mkdirhier(destpath, coll->co_umask) != 0) {
			xasprintf(errmsg, "Cannot create directories leading "
			    "to \"%s\": %s", destpath, strerror(errno));
			free(destpath);
			status_free(st);
			return (NULL);
		}
		free(destpath);
		st->wr = stream_open_file(st->tempfile,
		    O_CREAT | O_TRUNC | O_WRONLY, 0644);
		if (st->wr == NULL) {
			xasprintf(errmsg, "Cannot create \"%s\": %s",
			    st->tempfile, strerror(errno));
			status_free(st);
			return (NULL);
		}
		fa = fattr_new(FT_FILE, -1);
		fattr_mergedefault(fa);
		fattr_umask(fa, coll->co_umask);
		rv = fattr_install(fa, st->tempfile, NULL);
		fattr_free(fa);
		if (rv == -1) {
			xasprintf(errmsg,
			    "Cannot set attributes for \"%s\": %s",
			    st->tempfile, strerror(errno));
			status_free(st);
			return (NULL);
		}
		if (scantime != st->scantime)
			st->dirty = 1;
		error = proto_printf(st->wr, "F %d %t\n", STATUS_VERSION,
		    scantime);
		if (error) {
			st->error = STATUS_ERR_WRITE;
			st->suberror = errno;
			*errmsg = status_errmsg(st);
			status_free(st);
			return (NULL);
		}
	}
	return (st);
}

/*
 * Get an entry from the status file.  If name is NULL, the next entry
 * is returned.  If name is not NULL, the entry matching this name is
 * returned, or NULL if it couldn't be found.  If deleteto is set to 1,
 * all the entries read from the status file while looking for the
 * given name are deleted.
 */
int
status_get(struct status *st, char *name, int isdirup, int deleteto,
    struct statusrec **psr)
{
	struct statusrec key;
	struct statusrec *sr;
	char *line;
	int c, error;

	if (st->eof)
		return (0);

	if (st->error)
		return (-1);

	if (name == NULL) {
		sr = status_rd(st);
		if (sr == NULL) {
			if (st->error)
				return (-1);
			return (0);
		}
		*psr = sr;
		return (1);
	}

	if (st->current != NULL) {
		sr = st->current;
		st->current = NULL;
	} else {
		sr = status_rd(st);
		if (sr == NULL) {
			if (st->error)
				return (-1);
			return (0);
		}
	}

	key.sr_file = name;
	if (isdirup)
		key.sr_type = SR_DIRUP;
	else
		key.sr_type = SR_CHECKOUTLIVE;

	c = statusrec_cmp(sr, &key);
	if (c < 0) {
		if (st->wr != NULL && !deleteto) {
			error = status_wr(st, sr);
			if (error)
				return (-1);
		}
		/* Loop until we find the good entry. */
		for (;;) {
			sr = status_rdraw(st, &line);
			if (sr == NULL) {
				if (st->error)
					return (-1);
				return (0);
			}
			c = statusrec_cmp(sr, &key);
			if (c >= 0)
				break;
			if (st->wr != NULL && !deleteto) {
				error = status_wrraw(st, sr, line);
				if (error)
					return (-1);
			}
		}
		error = statusrec_cook(sr, line);
		if (error) {
			st->error = STATUS_ERR_PARSE;
			return (-1);
		}
	}
	st->current = sr;
	if (c != 0)
		return (0);
	*psr = sr;
	return (1);
}

/*
 * Put this entry into the status file.  If an entry with the same name
 * existed in the status file, it is replaced by this one, otherwise,
 * the entry is added to the status file.
 */
int
status_put(struct status *st, struct statusrec *sr)
{
	struct statusrec *old;
	int error, ret;

	ret = status_get(st, sr->sr_file, sr->sr_type == SR_DIRUP, 0, &old);
	if (ret == -1)
		return (-1);
	if (ret) {
		if (old->sr_type == SR_DIRDOWN) {
			/* DirUp should never match DirDown */
			assert(old->sr_type != SR_DIRUP);
			if (sr->sr_type == SR_CHECKOUTLIVE ||
			    sr->sr_type == SR_CHECKOUTDEAD) {
				/* We are replacing a directory with a file.
				   Delete all entries inside the directory we
				   are replacing. */
				ret = status_get(st, sr->sr_file, 1, 1, &old);
				if (ret == -1)
					return (-1);
				assert(ret);
			}
		} else
			st->current = NULL;
	}
	st->dirty = 1;
	error = status_wr(st, sr);
	if (error)
		return (-1);
	return (0);
}

/*
 * Delete the specified entry from the status file.
 */
int
status_delete(struct status *st, char *name, int isdirup)
{
	struct statusrec *sr;
	int ret;

	ret = status_get(st, name, isdirup, 0, &sr);
	if (ret == -1)
		return (-1);
	if (ret) {
		st->current = NULL;
		st->dirty = 1;
	}
	return (0);
}

/*
 * Check whether we hit the end of file.
 */
int
status_eof(struct status *st)
{

	return (st->eof);
}

/*
 * Returns the error message if there was an error, otherwise returns
 * NULL.  The error message is allocated dynamically and needs to be
 * freed by the caller after use.
 */
char *
status_errmsg(struct status *st)
{
	char *errmsg;

	if (!st->error)
		return (NULL);
	switch (st->error) {
	case STATUS_ERR_READ:
		xasprintf(&errmsg, "Read failure on \"%s\": %s",
		    st->path, strerror(st->suberror));
		break;
	case STATUS_ERR_WRITE:
		xasprintf(&errmsg, "Write failure on \"%s\": %s",
		    st->tempfile, strerror(st->suberror));
		break;
	case STATUS_ERR_PARSE:
		xasprintf(&errmsg, "Error in \"%s\": %d: "
		    "Could not parse status record", st->path, st->linenum);
		break;
	case STATUS_ERR_UNSORTED:
		xasprintf(&errmsg, "Error in \"%s\": %d: "
		    "File is not sorted properly", st->path, st->linenum);
		break;
	case STATUS_ERR_TRUNC:
		xasprintf(&errmsg, "Error in \"%s\": "
		    "File is truncated", st->path);
		break;
	case STATUS_ERR_BOGUS_DIRUP:
		xasprintf(&errmsg, "Error in \"%s\": %d: "
		    "\"U\" entry has no matching \"D\"", st->path, st->linenum);
		break;
	case STATUS_ERR_BAD_TYPE:
		xasprintf(&errmsg, "Error in \"%s\": %d: "
		    "Invalid file type \"%c\"", st->path, st->linenum,
		    st->suberror);
		break;
	case STATUS_ERR_RENAME:
		xasprintf(&errmsg, "Cannot rename \"%s\" to \"%s\": %s",
		    st->tempfile, st->path, strerror(st->suberror));
		break;
	default:
		assert(0);
		return (NULL);
	}
	return (errmsg);
}

/*
 * Close the status file and free any resource associated with it.
 * It is OK to pass NULL for errmsg only if the status file was
 * opened read-only.  If it wasn't opened read-only, status_close()
 * can produce an error and errmsg is not allowed to be NULL.  If
 * there was no errors, errmsg is set to NULL.
 */
void
status_close(struct status *st, char **errmsg)
{
	struct statusrec *sr;
	char *line, *name;
	int error, type;

	if (st->wr != NULL) {
		if (st->dirty) {
			if (st->current != NULL) {
				error = status_wr(st, st->current);
				if (error) {
					*errmsg = status_errmsg(st);
					goto bad;
				}
				st->current = NULL;
			}
			/* Copy the rest of the file. */
			while ((sr = status_rdraw(st, &line)) != NULL) {
				error = status_wrraw(st, sr, line);
				if (error) {
					*errmsg = status_errmsg(st);
					goto bad;
				}
			}
			if (st->error) {
				*errmsg = status_errmsg(st);
				goto bad;
			}

			/* Close off all the open directories. */
			pathcomp_finish(st->pc);
			while (pathcomp_get(st->pc, &type, &name)) {
				assert(type == PC_DIRUP);
				error = proto_printf(st->wr, "U %s %f\n",
				    name, fattr_bogus);
				if (error) {
					st->error = STATUS_ERR_WRITE;
					st->suberror = errno;
					*errmsg = status_errmsg(st);
					goto bad;
				}
			}

			/* Rename tempfile. */
			error = rename(st->tempfile, st->path);
			if (error) {
				st->error = STATUS_ERR_RENAME;
				st->suberror = errno;
				*errmsg = status_errmsg(st);
				goto bad;
			}
		} else {
			/* Just discard the tempfile. */
			unlink(st->tempfile);
		}
		*errmsg = NULL;
	}
	status_free(st);
	return;
bad:
	status_free(st);
}
