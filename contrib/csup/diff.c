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

#include <sys/limits.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diff.h"
#include "keyword.h"
#include "misc.h"
#include "stream.h"
#include "queue.h"

typedef long lineno_t;

#define	EC_ADD	0
#define	EC_DEL	1
#define	MAXKEY	LONG_MAX

/* Editing command and state. */
struct editcmd {
	int cmd;
	long key;
	int havetext;
	int offset;
	lineno_t where;
	lineno_t count;
	lineno_t lasta;
	lineno_t lastd;
	lineno_t editline;
	/* For convenience. */
	struct keyword *keyword;
	struct diffinfo *di;
	struct stream *orig;
	struct stream *dest;
	LIST_ENTRY(editcmd) next;
};

struct diffstart {
	LIST_HEAD(, editcmd) dhead;
};

static int	diff_geteditcmd(struct editcmd *, char *);
static int	diff_copyln(struct editcmd *, lineno_t);
static int	diff_ignoreln(struct editcmd *, lineno_t);
static void	diff_write(struct editcmd *, void *, size_t);
static int	diff_insert_edit(struct diffstart *, struct editcmd *);
static void	diff_free(struct diffstart *);

int
diff_apply(struct stream *rd, struct stream *orig, struct stream *dest,
    struct keyword *keyword, struct diffinfo *di, int comode)
{
	struct editcmd ec;
	lineno_t i;
	size_t size;
	char *line;
	int empty, error, noeol;

	memset(&ec, 0, sizeof(ec));
	empty = 0;
	noeol = 0;
	ec.di = di;
	ec.keyword = keyword;
	ec.orig = orig;
	ec.dest = dest;
	line = stream_getln(rd, NULL);
	while (line != NULL && strcmp(line, ".") != 0 &&
	    strcmp(line, ".+") != 0) {
		/*
		 * The server sends an empty line and then terminates
		 * with .+ for forced (and thus empty) commits.
		 */
		if (*line == '\0') {
			if (empty)
				return (-1);
			empty = 1;
			line = stream_getln(rd, NULL);
			continue;
		}
		error = diff_geteditcmd(&ec, line);
		if (error)
			return (-1);

		if (ec.cmd == EC_ADD) {
			error = diff_copyln(&ec, ec.where);
			if (error)
				return (-1);
			for (i = 0; i < ec.count; i++) {
				line = stream_getln(rd, &size);
				if (line == NULL)
					return (-1);
				if (comode && line[0] == '.') {
					line++;
					size--;
				}
				diff_write(&ec, line, size);
			}
		} else {
			assert(ec.cmd == EC_DEL);
			error = diff_copyln(&ec, ec.where - 1);
			if (error)
				return (-1);
			for (i = 0; i < ec.count; i++) {
				line = stream_getln(orig, NULL);
				if (line == NULL)
					return (-1);
				ec.editline++;
			}
		}
		line = stream_getln(rd, NULL);
	}
	if (comode && line == NULL)
		return (-1);
	/* If we got ".+", there's no ending newline. */
	if (comode && strcmp(line, ".+") == 0 && !empty)
		noeol = 1;
	ec.where = 0;
	while ((line = stream_getln(orig, &size)) != NULL)
		diff_write(&ec, line, size);
	stream_flush(dest);
	if (noeol) {
		error = stream_truncate_rel(dest, -1);
		if (error) {
			warn("stream_truncate_rel");
			return (-1);
		}
	}
	return (0);
}

/*
 * Reverse a diff using the same algorithm as in cvsup.
 */
static int
diff_write_reverse(struct stream *dest, struct diffstart *ds)
{
	struct editcmd *ec, *nextec;
	long editline, endline, firstoutputlinedeleted;
	long num_added, num_deleted, startline;
	int num;

	nextec = LIST_FIRST(&ds->dhead);
	editline = 0;
	num = 0;
	while (nextec != NULL) {
		ec = nextec;
		nextec = LIST_NEXT(nextec, next);
		if (nextec == NULL)
			break;
		num++;
		num_deleted = 0;
		if (ec->havetext)
			num_deleted = ec->count;
		num_added = num_deleted + nextec->offset - ec->offset;
		if (num_deleted > 0) {
			firstoutputlinedeleted = ec->key - num_deleted + 1;
			stream_printf(dest, "d%ld %ld\n", firstoutputlinedeleted,
			    num_deleted);
			if (num_added <= 0)
				continue;
		}
		if (num_added > 0) {
			stream_printf(dest, "a%ld %ld\n", ec->key, num_added);
			startline = ec->key - num_deleted + 1 + ec->offset;
			endline = startline + num_added - 1;

			/* Copy lines from original file. First ignore some. */
			ec->editline = editline;
			diff_ignoreln(ec,  startline - 1);
			diff_copyln(ec, endline);
			editline = ec->editline;
		}
	}
	return (0);
}

/* 
 * Insert a diff into the list sorted on key. Should perhaps use quicker
 * algorithms than insertion sort, but do this for now.
 */
static int
diff_insert_edit(struct diffstart *ds, struct editcmd *ec)
{
	struct editcmd *curec;

	if (ec == NULL)
		return (0);

	if (LIST_EMPTY(&ds->dhead)) {
		LIST_INSERT_HEAD(&ds->dhead, ec, next);
		return (0);
	}

	/* Insertion sort based on key. */
	LIST_FOREACH(curec, &ds->dhead, next) {
		if (ec->key < curec->key) {
			LIST_INSERT_BEFORE(curec, ec, next);
			return (0);
		}
		if (LIST_NEXT(curec, next) == NULL)
			break;
	}
	/* Just insert it after. */
	LIST_INSERT_AFTER(curec, ec, next);
	return (0);
}

static void 
diff_free(struct diffstart *ds)
{
	struct editcmd *ec;

	while(!LIST_EMPTY(&ds->dhead)) {
		ec = LIST_FIRST(&ds->dhead);
		LIST_REMOVE(ec, next);
		free(ec);
	}
}

/*
 * Write the reverse diff from the diff in rd, and original file into
 * destination. This algorithm is the same as used in cvsup.
 */
int
diff_reverse(struct stream *rd, struct stream *orig, struct stream *dest,
    struct keyword *keyword, struct diffinfo *di)
{
	struct diffstart ds;
	struct editcmd ec, *addec, *delec;
	lineno_t i;
	char *line;
	int error, offset;

	memset(&ec, 0, sizeof(ec));
	ec.orig = orig;
	ec.dest = dest;
	ec.keyword = keyword;
	ec.di = di;
	addec = NULL;
	delec = NULL;
	ec.havetext = 0;
	offset = 0;
	LIST_INIT(&ds.dhead);

	/* Start with next since we need it. */
	line = stream_getln(rd, NULL);
	/* First we build up the list of diffs from input. */
	while (line != NULL) {
		error = diff_geteditcmd(&ec, line);
		if (error)
			break;
		if (ec.cmd == EC_ADD) {
			addec = xmalloc(sizeof(struct editcmd));
			*addec = ec;
			addec->havetext = 1;
			/* Ignore the lines we was supposed to add. */
			for (i = 0; i < ec.count; i++) {
				line = stream_getln(rd, NULL);
				if (line == NULL)
					return (-1);
			}

			/* Get the next diff command if we have one. */
			addec->key = addec->where + addec->count - offset;
			if (delec != NULL &&
			    delec->key == addec->key - addec->count) {
				delec->key = addec->key;
				delec->havetext = addec->havetext;
				delec->count = addec->count;
				diff_insert_edit(&ds, delec);
				free(addec);
				delec = NULL;
				addec = NULL;
			} else {
				if (delec != NULL) {
					diff_insert_edit(&ds, delec);
				}
				delec = NULL;
				addec->offset = offset;
				diff_insert_edit(&ds, addec);
				addec = NULL;
			}
			offset -= ec.count;
		} else if (ec.cmd == EC_DEL) {
			if (delec != NULL) {
				/* Update offset to our next. */
				diff_insert_edit(&ds, delec);
				delec = NULL;
			}
			delec = xmalloc(sizeof(struct editcmd));
			*delec = ec;
			delec->key = delec->where - 1 - offset;
			delec->offset = offset;
			delec->count = 0;
			delec->havetext = 0;
			/* Important to use the count we had before reset.*/
			offset += ec.count;
		}
		line = stream_getln(rd, NULL);
	}

	while (line != NULL)
		line = stream_getln(rd, NULL);
	if (delec != NULL) {
		diff_insert_edit(&ds, delec);
		delec = NULL;
	}

	addec = xmalloc(sizeof(struct editcmd));
	/* Should be filesize, but we set it to max value. */
	addec->key = MAXKEY;
	addec->offset = offset;
	addec->havetext = 0;
	addec->count = 0;
	diff_insert_edit(&ds, addec);
	addec = NULL;
	diff_write_reverse(dest, &ds);
	diff_free(&ds);
	stream_flush(dest);
	return (0);
}

/* Get an editing command from the diff. */
static int
diff_geteditcmd(struct editcmd *ec, char *line)
{
	char *end;

	if (line[0] == 'a')
		ec->cmd = EC_ADD;
	else if (line[0] == 'd')
		ec->cmd = EC_DEL;
	else
		return (-1);
	errno = 0;
	ec->where = strtol(line + 1, &end, 10);
	if (errno || ec->where < 0 || *end != ' ')
		return (-1);
	line = end + 1;
	errno = 0;
	ec->count = strtol(line, &end, 10);
	if (errno || ec->count <= 0 || *end != '\0')
		return (-1);
	if (ec->cmd == EC_ADD) {
		if (ec->where < ec->lasta)
			return (-1);
		ec->lasta = ec->where + 1;
	} else {
		if (ec->where < ec->lasta || ec->where < ec->lastd)
			return (-1);
		ec->lasta = ec->where;
		ec->lastd = ec->where + ec->count;
	}
	return (0);
}

/* Copy lines from the original version of the file up to line "to". */
static int
diff_copyln(struct editcmd *ec, lineno_t to)
{
	size_t size;
	char *line;

	while (ec->editline < to) {
		line = stream_getln(ec->orig, &size);
		if (line == NULL)
			return (-1);
		ec->editline++;
		diff_write(ec, line, size);
	}
	return (0);
}

/* Ignore lines from the original version of the file up to line "to". */
static int
diff_ignoreln(struct editcmd *ec, lineno_t to)
{
	size_t size;
	char *line;

	while (ec->editline < to) {
		line = stream_getln(ec->orig, &size);
		if (line == NULL)
			return (-1);
		ec->editline++;
	}
	return (0);
}

/* Write a new line to the file, expanding RCS keywords appropriately. */
static void
diff_write(struct editcmd *ec, void *buf, size_t size)
{
	size_t newsize;
	char *line, *newline;
	int ret;

	line = buf;
	ret = keyword_expand(ec->keyword, ec->di, line, size,
	    &newline, &newsize);
	if (ret) {
		stream_write(ec->dest, newline, newsize);
		free(newline);
	} else {
		stream_write(ec->dest, buf, size);
	}
}
