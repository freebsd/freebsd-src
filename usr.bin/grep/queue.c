/*	$NetBSD: queue.c,v 1.5 2011/08/31 16:24:57 plunky Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1999 James Howard and Dag-Erling Smørgrav
 * All rights reserved.
 * Copyright (c) 2020 Kyle Evans <kevans@FreeBSD.org>
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
 */

/*
 * A really poor man's queue.  It does only what it has to and gets out of
 * Dodge.  It is used in place of <sys/queue.h> to get a better performance.
 */

#include <sys/param.h>
#include <sys/queue.h>

#include <stdlib.h>
#include <string.h>

#include "grep.h"

typedef struct str		qentry_t;

static long long		filled;
static qentry_t			*qend, *qpool;

/*
 * qnext is the next entry to populate.  qlist is where the list actually
 * starts, for the purposes of printing.
 */
static qentry_t		*qlist, *qnext;

void
initqueue(void)
{

	qlist = qnext = qpool = grep_calloc(Bflag, sizeof(*qpool));
	qend = qpool + (Bflag - 1);
}

static qentry_t *
advqueue(qentry_t *itemp)
{

	if (itemp == qend)
		return (qpool);
	return (itemp + 1);
}

/*
 * Enqueue another line; return true if we've dequeued a line as a result
 */
bool
enqueue(struct str *x)
{
	qentry_t *item;
	bool rotated;

	item = qnext;
	qnext = advqueue(qnext);
	rotated = false;

	if (filled < Bflag) {
		filled++;
	} else if (filled == Bflag) {
		/* We had already filled up coming in; just rotate. */
		qlist = advqueue(qlist);
		rotated = true;
		free(item->dat);
	}
	/* len + 1 for NUL-terminator */
	item->dat = grep_malloc(sizeof(char) * x->len + 1);
	item->len = x->len;
	item->line_no = x->line_no;
	item->boff = x->boff;
	item->off = x->off;
	memcpy(item->dat, x->dat, x->len);
	item->dat[x->len] = '\0';
	item->file = x->file;

	return (rotated);
}

void
printqueue(void)
{
	qentry_t *item;

	item = qlist;
	do {
		/* Buffer must have ended early. */
		if (item->dat == NULL)
			break;

		grep_printline(item, '-');
		free(item->dat);
		item->dat = NULL;
		item = advqueue(item);
	} while (item != qlist);

	qlist = qnext = qpool;
	filled = 0;
}

void
clearqueue(void)
{
	qentry_t *item;

	item = qlist;
	do {
		free(item->dat);
		item->dat = NULL;
		item = advqueue(item);
	} while (item != qlist);

	qlist = qnext = qpool;
	filled = 0;
}
