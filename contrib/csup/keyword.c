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
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "diff.h"
#include "keyword.h"
#include "misc.h"
#include "queue.h"
#include "stream.h"

/*
 * The keyword API is used to expand the CVS/RCS keywords in files,
 * such as $Id$, $Revision$, etc.  The server does it for us when it
 * sends us entire files, but we need to handle the expansion when
 * applying a diff update.
 */

enum rcskey {
	RCSKEY_AUTHOR,
	RCSKEY_CVSHEADER,
	RCSKEY_DATE,
	RCSKEY_HEADER,
	RCSKEY_ID,
	RCSKEY_LOCKER,
	RCSKEY_LOG,
	RCSKEY_NAME,
	RCSKEY_RCSFILE,
	RCSKEY_REVISION,
	RCSKEY_SOURCE,
	RCSKEY_STATE
};

typedef enum rcskey rcskey_t;

struct tag {
	char *ident;
	rcskey_t key;
	int enabled;
	STAILQ_ENTRY(tag) next;
};

static struct tag	*tag_new(const char *, rcskey_t);
static char		*tag_expand(struct tag *, struct diffinfo *);
static void		 tag_free(struct tag *);

struct keyword {
	STAILQ_HEAD(, tag) keywords;		/* Enabled keywords. */
	size_t minkeylen;
	size_t maxkeylen;
};

/* Default CVS keywords. */
static struct {
	const char *ident;
	rcskey_t key;
} tag_defaults[] = {
	{ "Author",	RCSKEY_AUTHOR },
	{ "CVSHeader",	RCSKEY_CVSHEADER },
	{ "Date",	RCSKEY_DATE },
	{ "Header",	RCSKEY_HEADER },
	{ "Id",		RCSKEY_ID },
	{ "Locker",	RCSKEY_LOCKER },
	{ "Log",	RCSKEY_LOG },
	{ "Name",	RCSKEY_NAME },
	{ "RCSfile",	RCSKEY_RCSFILE },
	{ "Revision",	RCSKEY_REVISION },
	{ "Source",	RCSKEY_SOURCE },
	{ "State",	RCSKEY_STATE },
	{ NULL,		0, }
};

struct keyword *
keyword_new(void)
{
	struct keyword *new;
	struct tag *tag;
	size_t len;
	int i;

	new = xmalloc(sizeof(struct keyword));
	STAILQ_INIT(&new->keywords);
	new->minkeylen = ~0;
	new->maxkeylen = 0;
	for (i = 0; tag_defaults[i].ident != NULL; i++) {
		tag = tag_new(tag_defaults[i].ident, tag_defaults[i].key);
		STAILQ_INSERT_TAIL(&new->keywords, tag, next);
		len = strlen(tag->ident);
		/*
		 * These values are only computed here and not updated when
		 * adding an alias.  This is a bug, but CVSup has it and we
		 * need to be bug-to-bug compatible since the server will
		 * expect us to do the same, and we will fail with an MD5
		 * checksum mismatch if we don't.
		 */
		new->minkeylen = min(new->minkeylen, len);
		new->maxkeylen = max(new->maxkeylen, len);
	}
	return (new);
}

int
keyword_decode_expand(const char *expand)
{

	if (strcmp(expand, ".") == 0)
		return (EXPAND_DEFAULT);
	else if (strcmp(expand, "kv") == 0)
		return (EXPAND_KEYVALUE);
	else if (strcmp(expand, "kvl") == 0)
		return (EXPAND_KEYVALUELOCKER);
	else if (strcmp(expand, "k") == 0)
		return (EXPAND_KEY);
	else if (strcmp(expand, "o") == 0)
		return (EXPAND_OLD);
	else if (strcmp(expand, "b") == 0)
		return (EXPAND_BINARY);
	else if (strcmp(expand, "v") == 0)
		return (EXPAND_VALUE);
	else
		return (-1);
}

void
keyword_free(struct keyword *keyword)
{
	struct tag *tag;

	if (keyword == NULL)
		return;
	while (!STAILQ_EMPTY(&keyword->keywords)) {
		tag = STAILQ_FIRST(&keyword->keywords);
		STAILQ_REMOVE_HEAD(&keyword->keywords, next);
		tag_free(tag);
	}
	free(keyword);
}

int
keyword_alias(struct keyword *keyword, const char *ident, const char *rcskey)
{
	struct tag *new, *tag;

	STAILQ_FOREACH(tag, &keyword->keywords, next) {
		if (strcmp(tag->ident, rcskey) == 0) {
			new = tag_new(ident, tag->key);
			STAILQ_INSERT_HEAD(&keyword->keywords, new, next);
			return (0);
		}
	}
	errno = ENOENT;
	return (-1);
}

int
keyword_enable(struct keyword *keyword, const char *ident)
{
	struct tag *tag;
	int all;

	all = 0;
	if (strcmp(ident, ".") == 0)
		all = 1;

	STAILQ_FOREACH(tag, &keyword->keywords, next) {
		if (!all && strcmp(tag->ident, ident) != 0)
			continue;
		tag->enabled = 1;
		if (!all)
			return (0);
	}
	if (!all) {
		errno = ENOENT;
		return (-1);
	}
	return (0);
}

int
keyword_disable(struct keyword *keyword, const char *ident)
{
	struct tag *tag;
	int all;

	all = 0;
	if (strcmp(ident, ".") == 0)
		all = 1;

	STAILQ_FOREACH(tag, &keyword->keywords, next) {
		if (!all && strcmp(tag->ident, ident) != 0)
			continue;
		tag->enabled = 0;
		if (!all)
			return (0);
	}

	if (!all) {
		errno = ENOENT;
		return (-1);
	}
	return (0);
}

void
keyword_prepare(struct keyword *keyword)
{
	struct tag *tag, *temp;

	STAILQ_FOREACH_SAFE(tag, &keyword->keywords, next, temp) {
		if (!tag->enabled) {
			STAILQ_REMOVE(&keyword->keywords, tag, tag, next);
			tag_free(tag);
			continue;
		}
	}
}

/*
 * Expand appropriate RCS keywords.  If there's no tag to expand,
 * keyword_expand() returns 0, otherwise it returns 1 and writes a
 * pointer to the new line in *buf and the new len in *len.  The
 * new line is allocated with malloc() and needs to be freed by the
 * caller after use.
 */
int
keyword_expand(struct keyword *keyword, struct diffinfo *di, char *line,
    size_t size, char **buf, size_t *len)
{
	struct tag *tag;
	char *dollar, *keystart, *valstart, *vallim, *next;
	char *linestart, *newline, *newval, *cp, *tmp;
	size_t left, newsize, vallen;

	if (di->di_expand == EXPAND_OLD || di->di_expand == EXPAND_BINARY)
		return (0);
	newline = NULL;
	newsize = 0;
	left = size;
	linestart = cp = line;
again:
	dollar = memchr(cp, '$', left);
	if (dollar == NULL) {
		if (newline != NULL) {
			*buf = newline;
			*len = newsize;
			return (1);
		}
		return (0);
	}
	keystart = dollar + 1;
	left -= keystart - cp;
	vallim = memchr(keystart, '$', left);
	if (vallim == NULL) {
		if (newline != NULL) {
			*buf = newline;
			*len = newsize;
			return (1);
		}
		return (0);
	}
	if (vallim == keystart) {
		cp = keystart;
		goto again;
	}
	valstart = memchr(keystart, ':', left);
	if (valstart == keystart) {
		cp = vallim;
		left -= vallim - keystart;
		goto again;
	}
	if (valstart == NULL || valstart > vallim)
		valstart = vallim;

	if (valstart < keystart + keyword->minkeylen ||
	    valstart > keystart + keyword->maxkeylen) {
		cp = vallim;
		left -= vallim -keystart;
		goto again;
	}
	STAILQ_FOREACH(tag, &keyword->keywords, next) {
		if (strncmp(tag->ident, keystart, valstart - keystart) == 0 &&
		    tag->ident[valstart - keystart] == '\0') {
			if (newline != NULL)
				tmp = newline;
			else
				tmp = NULL;
			newval = NULL;
			if (di->di_expand == EXPAND_KEY) {
				newsize = dollar - linestart + 1 +
				    valstart - keystart + 1 +
				    size - (vallim + 1 - linestart);
				newline = xmalloc(newsize);
				cp = newline;
				memcpy(cp, linestart, dollar - linestart);
				cp += dollar - linestart;
				*cp++ = '$';
				memcpy(cp, keystart, valstart - keystart);
				cp += valstart - keystart;
				*cp++ = '$';
				next = cp;
				memcpy(cp, vallim + 1,
				    size - (vallim + 1 - linestart));
			} else if (di->di_expand == EXPAND_VALUE) {
				newval = tag_expand(tag, di);
				if (newval == NULL)
					vallen = 0;
				else
					vallen = strlen(newval);
				newsize = dollar - linestart +
				    vallen +
				    size - (vallim + 1 - linestart);
				newline = xmalloc(newsize);
				cp = newline;
				memcpy(cp, linestart, dollar - linestart);
				cp += dollar - linestart;
				if (newval != NULL) {
					memcpy(cp, newval, vallen);
					cp += vallen;
				}
				next = cp;
				memcpy(cp, vallim + 1,
				    size - (vallim + 1 - linestart));
			} else {
				assert(di->di_expand == EXPAND_DEFAULT ||
				    di->di_expand == EXPAND_KEYVALUE ||
				    di->di_expand == EXPAND_KEYVALUELOCKER);
				newval = tag_expand(tag, di);
				if (newval == NULL)
					vallen = 0;
				else
					vallen = strlen(newval);
				newsize = dollar - linestart + 1 +
				    valstart - keystart + 2 +
				    vallen + 2 +
				    size - (vallim + 1 - linestart);
				newline = xmalloc(newsize);
				cp = newline;
				memcpy(cp, linestart, dollar - linestart);
				cp += dollar - linestart;
				*cp++ = '$';
				memcpy(cp, keystart, valstart - keystart);
				cp += valstart - keystart;
				*cp++ = ':';
				*cp++ = ' ';
				if (newval != NULL) {
					memcpy(cp, newval, vallen);
					cp += vallen;
				}
				*cp++ = ' ';
				*cp++ = '$';
				next = cp;
				memcpy(cp, vallim + 1,
				    size - (vallim + 1 - linestart));
			}
			if (newval != NULL)
				free(newval);
			if (tmp != NULL)
				free(tmp);
			/*
			 * Continue looking for tags in the rest of the line.
			 */
			cp = next;
			size = newsize;
			left = size - (cp - newline);
			linestart = newline;
			goto again;
		}
	}
	cp = vallim;
	left = size - (cp - linestart);
	goto again;
}

static struct tag *
tag_new(const char *ident, rcskey_t key)
{
	struct tag *new;

	new = xmalloc(sizeof(struct tag));
	new->ident = xstrdup(ident);
	new->key = key;
	new->enabled = 1;
	return (new);
}

static void
tag_free(struct tag *tag)
{

	free(tag->ident);
	free(tag);
}

/*
 * Expand a specific tag and return the new value.  If NULL
 * is returned, the tag is empty.
 */
static char *
tag_expand(struct tag *tag, struct diffinfo *di)
{
	/*
	 * CVS formats dates as "XXXX/XX/XX XX:XX:XX".  32 bytes
	 * is big enough until year 10,000,000,000,000,000 :-).
	 */
	char cvsdate[32];
	struct tm tm;
	char *filename, *val;
	int error;

	error = rcsdatetotm(di->di_revdate, &tm);
	if (error)
		err(1, "strptime");
	if (strftime(cvsdate, sizeof(cvsdate), "%Y/%m/%d %H:%M:%S", &tm) == 0)
		err(1, "strftime");
	filename = strrchr(di->di_rcsfile, '/');
	if (filename == NULL)
		filename = di->di_rcsfile;
	else
		filename++;

	switch (tag->key) {
	case RCSKEY_AUTHOR:
		xasprintf(&val, "%s", di->di_author);
		break;
	case RCSKEY_CVSHEADER:
		xasprintf(&val, "%s %s %s %s %s", di->di_rcsfile,
		    di->di_revnum, cvsdate, di->di_author, di->di_state);
		break;
	case RCSKEY_DATE:
		xasprintf(&val, "%s", cvsdate);
		break;
	case RCSKEY_HEADER:
		xasprintf(&val, "%s/%s %s %s %s %s", di->di_cvsroot,
		    di->di_rcsfile, di->di_revnum, cvsdate, di->di_author,
		    di->di_state);
		break;
	case RCSKEY_ID:
		xasprintf(&val, "%s %s %s %s %s", filename, di->di_revnum,
		    cvsdate, di->di_author, di->di_state);
		break;
	case RCSKEY_LOCKER:
		/*
		 * Unimplemented even in CVSup sources.  It seems we don't
		 * even have this information sent by the server.
		 */
		return (NULL);
	case RCSKEY_LOG:
		/* XXX */
		printf("%s: Implement Log keyword expansion\n", __func__);
		return (NULL);
	case RCSKEY_NAME:
		if (di->di_tag != NULL)
			xasprintf(&val, "%s", di->di_tag);
		else
			return (NULL);
		break;
	case RCSKEY_RCSFILE:
		xasprintf(&val, "%s", filename);
		break;
	case RCSKEY_REVISION:
		xasprintf(&val, "%s", di->di_revnum);
		break;
	case RCSKEY_SOURCE:
		xasprintf(&val, "%s/%s", di->di_cvsroot, di->di_rcsfile);
		break;
	case RCSKEY_STATE:
		xasprintf(&val, "%s", di->di_state);
		break;
	}
	return (val);
}
