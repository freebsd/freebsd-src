/*-
 * Copyright (c) 2008, Ulf Lilleengen <lulf@FreeBSD.org>
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
 * 
 */

#include <stdlib.h>
#include <assert.h>
#include "rcstokenizer.h"
#include "rcsparse.h"
#include "rcsfile.h"
#include "misc.h"
#include "queue.h"

/*
 * This is an RCS-parser using lex for tokenizing and makes sure the RCS syntax
 * is correct as it constructs an RCS file that is used by csup.
 */

static void	asserttoken(yyscan_t *, int);
static int	parse_admin(struct rcsfile *, yyscan_t *);
static int	parse_deltas(struct rcsfile *, yyscan_t *, int);
static int	parse_deltatexts(struct rcsfile *, yyscan_t *, int);
static char	*duptext(yyscan_t *);

struct string {
	char *str;
	STAILQ_ENTRY(string) next;
};

static void
asserttoken(yyscan_t *sp, int token)
{
	int t;

	t = token;
	t = rcslex(*sp);
	assert(t == token);
}

static char *
duptext(yyscan_t *sp)
{
	char *tmp, *val;
	int len;

	tmp = rcsget_text(*sp);
	len = rcsget_leng(*sp);
	val = xmalloc(len + 2);
	strlcpy(val, tmp, len + 1);
	return (val);
}

/*
 * Start up parser, and use the rcsfile hook to add objects.
 */
int
rcsparse_run(struct rcsfile *rf, FILE *infp)
{
	yyscan_t scanner;
	char *desc;
	int error, tok;
	
	error = 0;
	rcslex_init(&scanner);
	rcsset_in(infp, scanner);
	tok = parse_admin(rf, &scanner);
	tok = parse_deltas(rf, &scanner, tok);
	assert(tok == KEYWORD);
	asserttoken(&scanner, STRING);
	desc = duptext(&scanner);
	rcsfile_setval(rf, RCSFILE_DESC, desc);
	free(desc);
	tok = rcslex(scanner);
	error = parse_deltatexts(rf, &scanner, tok);
	if (error)
		return (error);
	rcslex_destroy(scanner);
	return (0);
}

/*
 * Parse the admin part of a RCS file.
 */
static int
parse_admin(struct rcsfile *rf, yyscan_t *sp)
{
	char *head;
	char *branch;
	char *comment;
	char *id;
	char *expand;
	char *tag, *revnum;
	char *tmp;
	int strict, token;

	strict = 0;
	branch = NULL;

	/* head {num}; */
	asserttoken(sp, KEYWORD);
	asserttoken(sp, NUM);
	head = duptext(sp);
	rcsfile_setval(rf, RCSFILE_HEAD, head);
	free(head);
	asserttoken(sp, SEMIC);

	/* { branch {num}; } */
	token = rcslex(*sp);
	if (token == KEYWORD_TWO) {
		asserttoken(sp, NUM);
		branch = duptext(sp);
		rcsfile_setval(rf, RCSFILE_BRANCH, branch);
		free(branch);
		asserttoken(sp, SEMIC);
		token = rcslex(*sp);
	}

	/* access {id]*; */
	assert(token == KEYWORD);
	token = rcslex(*sp);
	while (token == ID) {
		id = duptext(sp);
		rcsfile_addaccess(rf, id);
		free(id);
		token = rcslex(*sp);
	}
	assert(token == SEMIC);

	/* symbols {sym : num}*; */
	asserttoken(sp, KEYWORD);
	token = rcslex(*sp);
	while (token == ID) {
		tag = duptext(sp);
		asserttoken(sp, COLON);
		asserttoken(sp, NUM);
		revnum = duptext(sp);
		rcsfile_importtag(rf, tag, revnum);
		free(tag);
		free(revnum);
		token = rcslex(*sp);
	}
	assert(token == SEMIC);

	/* locks {id : num}*; */
	asserttoken(sp, KEYWORD);
	token = rcslex(*sp);
	while (token == ID) {
		/* XXX: skip locks */
		asserttoken(sp, COLON);
		asserttoken(sp, NUM);
		token = rcslex(*sp);
	}
	assert(token == SEMIC);
	token = rcslex(*sp);
	while (token == KEYWORD) {
		tmp = rcsget_text(*sp);

		/* {strict  ;} */
		if (!strcmp(tmp, "strict")) {
			rcsfile_setval(rf, RCSFILE_STRICT, tmp);
			asserttoken(sp, SEMIC);
		/* { comment {string}; } */
		} else if (!strcmp(tmp, "comment")) {
			token = rcslex(*sp);
			if (token == STRING) {
				comment = duptext(sp);
				rcsfile_setval(rf, RCSFILE_COMMENT, comment);
				free(comment);
			}
			asserttoken(sp, SEMIC);
		/* { expand {string}; } */
		} else if (!strcmp(tmp, "expand")) {
			token = rcslex(*sp);
			if (token == STRING) {
				expand = duptext(sp);
				rcsfile_setval(rf, RCSFILE_EXPAND, expand);
				free(expand);
			}
			asserttoken(sp, SEMIC);
		} 
		/* {newphrase }* */
		token = rcslex(*sp);
		while (token == ID) {
			token = rcslex(*sp);
			/* XXX: ignore for now. */
			while (token == ID || token == NUM || token == STRING ||
			    token == COLON) {
				token = rcslex(*sp);
			}
			asserttoken(sp, SEMIC);
			token = rcslex(*sp);
		}
	}
	return (token);
}

/*
 * Parse RCS deltas.
 */
static int
parse_deltas(struct rcsfile *rf, yyscan_t *sp, int token)
{
	STAILQ_HEAD(, string) branchlist;
	char *revnum, *revdate, *author, *state, *next;

	/* In case we don't have deltas. */
	if (token != NUM)
		return (token);
	do {
		next = NULL;
		state = NULL;

		/* num */
		assert(token == NUM);
		revnum = duptext(sp);
		/* date num; */
		asserttoken(sp, KEYWORD);
		asserttoken(sp, NUM);
		revdate = duptext(sp);
		asserttoken(sp, SEMIC);
		/* author id; */
		asserttoken(sp, KEYWORD);
		asserttoken(sp, ID);
		author = duptext(sp);
		asserttoken(sp, SEMIC);
		/* state {id}; */
		asserttoken(sp, KEYWORD);
		token = rcslex(*sp);
		if (token == ID) {
			state = duptext(sp);
			token = rcslex(*sp);
		}
		assert(token == SEMIC);
		/* branches {num}*; */
		asserttoken(sp, KEYWORD);
		token = rcslex(*sp);
		STAILQ_INIT(&branchlist);
		while (token == NUM)
			token = rcslex(*sp);
		assert(token == SEMIC);
		/* next {num}; */
		asserttoken(sp, KEYWORD);
		token = rcslex(*sp);
		if (token == NUM) {
			next = duptext(sp);
			token = rcslex(*sp);
		}
		assert(token == SEMIC);
		/* {newphrase }* */
		token = rcslex(*sp);
		while (token == ID) {
			token = rcslex(*sp);
			/* XXX: ignore for now. */
			while (token == ID || token == NUM || token == STRING ||
			    token == COLON) {
				token = rcslex(*sp);
			}
			asserttoken(sp, SEMIC);
			token = rcslex(*sp);
		}
		rcsfile_importdelta(rf, revnum, revdate, author, state, next);
		free(revnum);
		free(revdate);
		free(author);
		if (state != NULL)
			free(state);
		if (next != NULL)
			free(next);
	} while (token == NUM);

	return (token);
}

/*
 * Parse RCS deltatexts.
 */
static int
parse_deltatexts(struct rcsfile *rf, yyscan_t *sp, int token)
{
	struct delta *d;
	char *revnum, *log, *text;
	int error;

	error = 0;
	/* In case we don't have deltatexts. */
	if (token != NUM) {
		fprintf(stderr, "Tokens Was %d\n", token);
		return (token);
	}
	do {
		/* num */
		assert(token == NUM);
		revnum = duptext(sp);
		/* Get delta we're adding text to. */
		d = rcsfile_getdelta(rf, revnum);
		free(revnum);

		/* log string */
		asserttoken(sp, KEYWORD);
		asserttoken(sp, STRING);
		log = duptext(sp);
		error = rcsdelta_addlog(d, log);
		free(log);
		if (error)
			return (-1);
		/* { newphrase }* */
		token = rcslex(*sp);
		while (token == ID) {
			token = rcslex(*sp);
			/* XXX: ignore for now. */
			while (token == ID || token == NUM || token == STRING ||
			    token == COLON) {
				token = rcslex(*sp);
			}
			asserttoken(sp, SEMIC);
			token = rcslex(*sp);
		}
		/* text string */
		assert(token == KEYWORD);
		asserttoken(sp, STRING);
		text = duptext(sp);
		error = rcsdelta_addtext(d, text);
		/* 
		 * If this happens, something is wrong with the RCS file, and it
		 * should be resent.
		 */
		free(text);
		if (error)
			return (-1);
		token = rcslex(*sp);
	} while (token == NUM);

	return (0);
}
