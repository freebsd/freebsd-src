/*	$Id: mdoc_argv.c,v 1.89 2013/12/25 00:50:05 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2012 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mdoc.h"
#include "mandoc.h"
#include "libmdoc.h"
#include "libmandoc.h"

#define	MULTI_STEP	 5 /* pre-allocate argument values */
#define	DELIMSZ	  	 6 /* max possible size of a delimiter */

enum	argsflag {
	ARGSFL_NONE = 0,
	ARGSFL_DELIM, /* handle delimiters of [[::delim::][ ]+]+ */
	ARGSFL_TABSEP /* handle tab/`Ta' separated phrases */
};

enum	argvflag {
	ARGV_NONE, /* no args to flag (e.g., -split) */
	ARGV_SINGLE, /* one arg to flag (e.g., -file xxx)  */
	ARGV_MULTI /* multiple args (e.g., -column xxx yyy) */
};

struct	mdocarg {
	enum argsflag	 flags;
	const enum mdocargt *argvs;
};

static	void		 argn_free(struct mdoc_arg *, int);
static	enum margserr	 args(struct mdoc *, int, int *, 
				char *, enum argsflag, char **);
static	int		 args_checkpunct(const char *, int);
static	int		 argv_multi(struct mdoc *, int, 
				struct mdoc_argv *, int *, char *);
static	int		 argv_single(struct mdoc *, int, 
				struct mdoc_argv *, int *, char *);

static	const enum argvflag argvflags[MDOC_ARG_MAX] = {
	ARGV_NONE,	/* MDOC_Split */
	ARGV_NONE,	/* MDOC_Nosplit */
	ARGV_NONE,	/* MDOC_Ragged */
	ARGV_NONE,	/* MDOC_Unfilled */
	ARGV_NONE,	/* MDOC_Literal */
	ARGV_SINGLE,	/* MDOC_File */
	ARGV_SINGLE,	/* MDOC_Offset */
	ARGV_NONE,	/* MDOC_Bullet */
	ARGV_NONE,	/* MDOC_Dash */
	ARGV_NONE,	/* MDOC_Hyphen */
	ARGV_NONE,	/* MDOC_Item */
	ARGV_NONE,	/* MDOC_Enum */
	ARGV_NONE,	/* MDOC_Tag */
	ARGV_NONE,	/* MDOC_Diag */
	ARGV_NONE,	/* MDOC_Hang */
	ARGV_NONE,	/* MDOC_Ohang */
	ARGV_NONE,	/* MDOC_Inset */
	ARGV_MULTI,	/* MDOC_Column */
	ARGV_SINGLE,	/* MDOC_Width */
	ARGV_NONE,	/* MDOC_Compact */
	ARGV_NONE,	/* MDOC_Std */
	ARGV_NONE,	/* MDOC_Filled */
	ARGV_NONE,	/* MDOC_Words */
	ARGV_NONE,	/* MDOC_Emphasis */
	ARGV_NONE,	/* MDOC_Symbolic */
	ARGV_NONE	/* MDOC_Symbolic */
};

static	const enum mdocargt args_Ex[] = {
	MDOC_Std,
	MDOC_ARG_MAX
};

static	const enum mdocargt args_An[] = {
	MDOC_Split,
	MDOC_Nosplit,
	MDOC_ARG_MAX
};

static	const enum mdocargt args_Bd[] = {
	MDOC_Ragged,
	MDOC_Unfilled,
	MDOC_Filled,
	MDOC_Literal,
	MDOC_File,
	MDOC_Offset,
	MDOC_Compact,
	MDOC_Centred,
	MDOC_ARG_MAX
};

static	const enum mdocargt args_Bf[] = {
	MDOC_Emphasis,
	MDOC_Literal,
	MDOC_Symbolic,
	MDOC_ARG_MAX
};

static	const enum mdocargt args_Bk[] = {
	MDOC_Words,
	MDOC_ARG_MAX
};

static	const enum mdocargt args_Bl[] = {
	MDOC_Bullet,
	MDOC_Dash,
	MDOC_Hyphen,
	MDOC_Item,
	MDOC_Enum,
	MDOC_Tag,
	MDOC_Diag,
	MDOC_Hang,
	MDOC_Ohang,
	MDOC_Inset,
	MDOC_Column,
	MDOC_Width,
	MDOC_Offset,
	MDOC_Compact,
	MDOC_Nested,
	MDOC_ARG_MAX
};

static	const struct mdocarg mdocargs[MDOC_MAX] = {
	{ ARGSFL_DELIM, NULL }, /* Ap */
	{ ARGSFL_NONE, NULL }, /* Dd */
	{ ARGSFL_NONE, NULL }, /* Dt */
	{ ARGSFL_NONE, NULL }, /* Os */
	{ ARGSFL_NONE, NULL }, /* Sh */
	{ ARGSFL_NONE, NULL }, /* Ss */ 
	{ ARGSFL_NONE, NULL }, /* Pp */ 
	{ ARGSFL_DELIM, NULL }, /* D1 */
	{ ARGSFL_DELIM, NULL }, /* Dl */
	{ ARGSFL_NONE, args_Bd }, /* Bd */
	{ ARGSFL_NONE, NULL }, /* Ed */
	{ ARGSFL_NONE, args_Bl }, /* Bl */
	{ ARGSFL_NONE, NULL }, /* El */
	{ ARGSFL_NONE, NULL }, /* It */
	{ ARGSFL_DELIM, NULL }, /* Ad */ 
	{ ARGSFL_DELIM, args_An }, /* An */
	{ ARGSFL_DELIM, NULL }, /* Ar */
	{ ARGSFL_DELIM, NULL }, /* Cd */
	{ ARGSFL_DELIM, NULL }, /* Cm */
	{ ARGSFL_DELIM, NULL }, /* Dv */ 
	{ ARGSFL_DELIM, NULL }, /* Er */ 
	{ ARGSFL_DELIM, NULL }, /* Ev */ 
	{ ARGSFL_NONE, args_Ex }, /* Ex */
	{ ARGSFL_DELIM, NULL }, /* Fa */ 
	{ ARGSFL_NONE, NULL }, /* Fd */ 
	{ ARGSFL_DELIM, NULL }, /* Fl */
	{ ARGSFL_DELIM, NULL }, /* Fn */ 
	{ ARGSFL_DELIM, NULL }, /* Ft */ 
	{ ARGSFL_DELIM, NULL }, /* Ic */ 
	{ ARGSFL_DELIM, NULL }, /* In */ 
	{ ARGSFL_DELIM, NULL }, /* Li */
	{ ARGSFL_NONE, NULL }, /* Nd */ 
	{ ARGSFL_DELIM, NULL }, /* Nm */ 
	{ ARGSFL_DELIM, NULL }, /* Op */
	{ ARGSFL_NONE, NULL }, /* Ot */
	{ ARGSFL_DELIM, NULL }, /* Pa */
	{ ARGSFL_NONE, args_Ex }, /* Rv */
	{ ARGSFL_DELIM, NULL }, /* St */ 
	{ ARGSFL_DELIM, NULL }, /* Va */
	{ ARGSFL_DELIM, NULL }, /* Vt */ 
	{ ARGSFL_DELIM, NULL }, /* Xr */
	{ ARGSFL_NONE, NULL }, /* %A */
	{ ARGSFL_NONE, NULL }, /* %B */
	{ ARGSFL_NONE, NULL }, /* %D */
	{ ARGSFL_NONE, NULL }, /* %I */
	{ ARGSFL_NONE, NULL }, /* %J */
	{ ARGSFL_NONE, NULL }, /* %N */
	{ ARGSFL_NONE, NULL }, /* %O */
	{ ARGSFL_NONE, NULL }, /* %P */
	{ ARGSFL_NONE, NULL }, /* %R */
	{ ARGSFL_NONE, NULL }, /* %T */
	{ ARGSFL_NONE, NULL }, /* %V */
	{ ARGSFL_DELIM, NULL }, /* Ac */
	{ ARGSFL_NONE, NULL }, /* Ao */
	{ ARGSFL_DELIM, NULL }, /* Aq */
	{ ARGSFL_DELIM, NULL }, /* At */
	{ ARGSFL_DELIM, NULL }, /* Bc */
	{ ARGSFL_NONE, args_Bf }, /* Bf */ 
	{ ARGSFL_NONE, NULL }, /* Bo */
	{ ARGSFL_DELIM, NULL }, /* Bq */
	{ ARGSFL_DELIM, NULL }, /* Bsx */
	{ ARGSFL_DELIM, NULL }, /* Bx */
	{ ARGSFL_NONE, NULL }, /* Db */
	{ ARGSFL_DELIM, NULL }, /* Dc */
	{ ARGSFL_NONE, NULL }, /* Do */
	{ ARGSFL_DELIM, NULL }, /* Dq */
	{ ARGSFL_DELIM, NULL }, /* Ec */
	{ ARGSFL_NONE, NULL }, /* Ef */
	{ ARGSFL_DELIM, NULL }, /* Em */ 
	{ ARGSFL_NONE, NULL }, /* Eo */
	{ ARGSFL_DELIM, NULL }, /* Fx */
	{ ARGSFL_DELIM, NULL }, /* Ms */
	{ ARGSFL_DELIM, NULL }, /* No */
	{ ARGSFL_DELIM, NULL }, /* Ns */
	{ ARGSFL_DELIM, NULL }, /* Nx */
	{ ARGSFL_DELIM, NULL }, /* Ox */
	{ ARGSFL_DELIM, NULL }, /* Pc */
	{ ARGSFL_DELIM, NULL }, /* Pf */
	{ ARGSFL_NONE, NULL }, /* Po */
	{ ARGSFL_DELIM, NULL }, /* Pq */
	{ ARGSFL_DELIM, NULL }, /* Qc */
	{ ARGSFL_DELIM, NULL }, /* Ql */
	{ ARGSFL_NONE, NULL }, /* Qo */
	{ ARGSFL_DELIM, NULL }, /* Qq */
	{ ARGSFL_NONE, NULL }, /* Re */
	{ ARGSFL_NONE, NULL }, /* Rs */
	{ ARGSFL_DELIM, NULL }, /* Sc */
	{ ARGSFL_NONE, NULL }, /* So */
	{ ARGSFL_DELIM, NULL }, /* Sq */
	{ ARGSFL_NONE, NULL }, /* Sm */
	{ ARGSFL_DELIM, NULL }, /* Sx */
	{ ARGSFL_DELIM, NULL }, /* Sy */
	{ ARGSFL_DELIM, NULL }, /* Tn */
	{ ARGSFL_DELIM, NULL }, /* Ux */
	{ ARGSFL_DELIM, NULL }, /* Xc */
	{ ARGSFL_NONE, NULL }, /* Xo */
	{ ARGSFL_NONE, NULL }, /* Fo */ 
	{ ARGSFL_DELIM, NULL }, /* Fc */ 
	{ ARGSFL_NONE, NULL }, /* Oo */
	{ ARGSFL_DELIM, NULL }, /* Oc */
	{ ARGSFL_NONE, args_Bk }, /* Bk */
	{ ARGSFL_NONE, NULL }, /* Ek */
	{ ARGSFL_NONE, NULL }, /* Bt */
	{ ARGSFL_NONE, NULL }, /* Hf */
	{ ARGSFL_NONE, NULL }, /* Fr */
	{ ARGSFL_NONE, NULL }, /* Ud */
	{ ARGSFL_DELIM, NULL }, /* Lb */
	{ ARGSFL_NONE, NULL }, /* Lp */
	{ ARGSFL_DELIM, NULL }, /* Lk */
	{ ARGSFL_DELIM, NULL }, /* Mt */
	{ ARGSFL_DELIM, NULL }, /* Brq */
	{ ARGSFL_NONE, NULL }, /* Bro */
	{ ARGSFL_DELIM, NULL }, /* Brc */
	{ ARGSFL_NONE, NULL }, /* %C */
	{ ARGSFL_NONE, NULL }, /* Es */
	{ ARGSFL_NONE, NULL }, /* En */
	{ ARGSFL_DELIM, NULL }, /* Dx */
	{ ARGSFL_NONE, NULL }, /* %Q */
	{ ARGSFL_NONE, NULL }, /* br */
	{ ARGSFL_NONE, NULL }, /* sp */
	{ ARGSFL_NONE, NULL }, /* %U */
	{ ARGSFL_NONE, NULL }, /* Ta */
};


/*
 * Parse an argument from line text.  This comes in the form of -key
 * [value0...], which may either have a single mandatory value, at least
 * one mandatory value, an optional single value, or no value.
 */
enum margverr
mdoc_argv(struct mdoc *mdoc, int line, enum mdoct tok,
		struct mdoc_arg **v, int *pos, char *buf)
{
	char		 *p, sv;
	struct mdoc_argv tmp;
	struct mdoc_arg	 *arg;
	const enum mdocargt *ap;

	if ('\0' == buf[*pos])
		return(ARGV_EOLN);
	else if (NULL == (ap = mdocargs[tok].argvs))
		return(ARGV_WORD);
	else if ('-' != buf[*pos])
		return(ARGV_WORD);

	/* Seek to the first unescaped space. */

	p = &buf[++(*pos)];

	assert(*pos > 0);

	for ( ; buf[*pos] ; (*pos)++)
		if (' ' == buf[*pos] && '\\' != buf[*pos - 1])
			break;

	/* 
	 * We want to nil-terminate the word to look it up (it's easier
	 * that way).  But we may not have a flag, in which case we need
	 * to restore the line as-is.  So keep around the stray byte,
	 * which we'll reset upon exiting (if necessary).
	 */

	if ('\0' != (sv = buf[*pos])) 
		buf[(*pos)++] = '\0';

	/*
	 * Now look up the word as a flag.  Use temporary storage that
	 * we'll copy into the node's flags, if necessary.
	 */

	memset(&tmp, 0, sizeof(struct mdoc_argv));

	tmp.line = line;
	tmp.pos = *pos;
	tmp.arg = MDOC_ARG_MAX;

	while (MDOC_ARG_MAX != (tmp.arg = *ap++))
		if (0 == strcmp(p, mdoc_argnames[tmp.arg]))
			break;

	if (MDOC_ARG_MAX == tmp.arg) {
		/* 
		 * The flag was not found.
		 * Restore saved zeroed byte and return as a word.
		 */
		if (sv)
			buf[*pos - 1] = sv;
		return(ARGV_WORD);
	}

	/* Read to the next word (the argument). */

	while (buf[*pos] && ' ' == buf[*pos])
		(*pos)++;

	switch (argvflags[tmp.arg]) {
	case (ARGV_SINGLE):
		if ( ! argv_single(mdoc, line, &tmp, pos, buf))
			return(ARGV_ERROR);
		break;
	case (ARGV_MULTI):
		if ( ! argv_multi(mdoc, line, &tmp, pos, buf))
			return(ARGV_ERROR);
		break;
	case (ARGV_NONE):
		break;
	}

	if (NULL == (arg = *v))
		arg = *v = mandoc_calloc(1, sizeof(struct mdoc_arg));

	arg->argc++;
	arg->argv = mandoc_realloc
		(arg->argv, arg->argc * sizeof(struct mdoc_argv));

	memcpy(&arg->argv[(int)arg->argc - 1], 
			&tmp, sizeof(struct mdoc_argv));

	return(ARGV_ARG);
}

void
mdoc_argv_free(struct mdoc_arg *p)
{
	int		 i;

	if (NULL == p)
		return;

	if (p->refcnt) {
		--(p->refcnt);
		if (p->refcnt)
			return;
	}
	assert(p->argc);

	for (i = (int)p->argc - 1; i >= 0; i--)
		argn_free(p, i);

	free(p->argv);
	free(p);
}

static void
argn_free(struct mdoc_arg *p, int iarg)
{
	struct mdoc_argv *arg;
	int		  j;

	arg = &p->argv[iarg];

	if (arg->sz && arg->value) {
		for (j = (int)arg->sz - 1; j >= 0; j--) 
			free(arg->value[j]);
		free(arg->value);
	}

	for (--p->argc; iarg < (int)p->argc; iarg++)
		p->argv[iarg] = p->argv[iarg+1];
}

enum margserr
mdoc_zargs(struct mdoc *mdoc, int line, int *pos, char *buf, char **v)
{

	return(args(mdoc, line, pos, buf, ARGSFL_NONE, v));
}

enum margserr
mdoc_args(struct mdoc *mdoc, int line, int *pos, 
		char *buf, enum mdoct tok, char **v)
{
	enum argsflag	  fl;
	struct mdoc_node *n;

	fl = mdocargs[tok].flags;

	if (MDOC_It != tok)
		return(args(mdoc, line, pos, buf, fl, v));

	/*
	 * We know that we're in an `It', so it's reasonable to expect
	 * us to be sitting in a `Bl'.  Someday this may not be the case
	 * (if we allow random `It's sitting out there), so provide a
	 * safe fall-back into the default behaviour.
	 */

	for (n = mdoc->last; n; n = n->parent)
		if (MDOC_Bl == n->tok)
			if (LIST_column == n->norm->Bl.type) {
				fl = ARGSFL_TABSEP;
				break;
			}

	return(args(mdoc, line, pos, buf, fl, v));
}

static enum margserr
args(struct mdoc *mdoc, int line, int *pos, 
		char *buf, enum argsflag fl, char **v)
{
	char		*p, *pp;
	int		 pairs;
	enum margserr	 rc;

	if ('\0' == buf[*pos]) {
		if (MDOC_PPHRASE & mdoc->flags)
			return(ARGS_EOLN);
		/*
		 * If we're not in a partial phrase and the flag for
		 * being a phrase literal is still set, the punctuation
		 * is unterminated.
		 */
		if (MDOC_PHRASELIT & mdoc->flags)
			mdoc_pmsg(mdoc, line, *pos, MANDOCERR_BADQUOTE);

		mdoc->flags &= ~MDOC_PHRASELIT;
		return(ARGS_EOLN);
	}

	*v = &buf[*pos];

	if (ARGSFL_DELIM == fl)
		if (args_checkpunct(buf, *pos))
			return(ARGS_PUNCT);

	/*
	 * First handle TABSEP items, restricted to `Bl -column'.  This
	 * ignores conventional token parsing and instead uses tabs or
	 * `Ta' macros to separate phrases.  Phrases are parsed again
	 * for arguments at a later phase.
	 */

	if (ARGSFL_TABSEP == fl) {
		/* Scan ahead to tab (can't be escaped). */
		p = strchr(*v, '\t');
		pp = NULL;

		/* Scan ahead to unescaped `Ta'. */
		if ( ! (MDOC_PHRASELIT & mdoc->flags)) 
			for (pp = *v; ; pp++) {
				if (NULL == (pp = strstr(pp, "Ta")))
					break;
				if (pp > *v && ' ' != *(pp - 1))
					continue;
				if (' ' == *(pp + 2) || '\0' == *(pp + 2))
					break;
			}

		/* By default, assume a phrase. */
		rc = ARGS_PHRASE;

		/* 
		 * Adjust new-buffer position to be beyond delimiter
		 * mark (e.g., Ta -> end + 2).
		 */
		if (p && pp) {
			*pos += pp < p ? 2 : 1;
			rc = pp < p ? ARGS_PHRASE : ARGS_PPHRASE;
			p = pp < p ? pp : p;
		} else if (p && ! pp) {
			rc = ARGS_PPHRASE;
			*pos += 1;
		} else if (pp && ! p) {
			p = pp;
			*pos += 2;
		} else {
			rc = ARGS_PEND;
			p = strchr(*v, 0);
		}

		/* Whitespace check for eoln case... */
		if ('\0' == *p && ' ' == *(p - 1))
			mdoc_pmsg(mdoc, line, *pos, MANDOCERR_EOLNSPACE);

		*pos += (int)(p - *v);

		/* Strip delimiter's preceding whitespace. */
		pp = p - 1;
		while (pp > *v && ' ' == *pp) {
			if (pp > *v && '\\' == *(pp - 1))
				break;
			pp--;
		}
		*(pp + 1) = 0;

		/* Strip delimiter's proceeding whitespace. */
		for (pp = &buf[*pos]; ' ' == *pp; pp++, (*pos)++)
			/* Skip ahead. */ ;

		return(rc);
	}

	/*
	 * Process a quoted literal.  A quote begins with a double-quote
	 * and ends with a double-quote NOT preceded by a double-quote.
	 * NUL-terminate the literal in place.
	 * Collapse pairs of quotes inside quoted literals.
	 * Whitespace is NOT involved in literal termination.
	 */

	if (MDOC_PHRASELIT & mdoc->flags || '\"' == buf[*pos]) {
		if ( ! (MDOC_PHRASELIT & mdoc->flags))
			*v = &buf[++(*pos)];

		if (MDOC_PPHRASE & mdoc->flags)
			mdoc->flags |= MDOC_PHRASELIT;

		pairs = 0;
		for ( ; buf[*pos]; (*pos)++) {
			/* Move following text left after quoted quotes. */
			if (pairs)
				buf[*pos - pairs] = buf[*pos];
			if ('\"' != buf[*pos])
				continue;
			/* Unquoted quotes end quoted args. */
			if ('\"' != buf[*pos + 1])
				break;
			/* Quoted quotes collapse. */
			pairs++;
			(*pos)++;
		}
		if (pairs)
			buf[*pos - pairs] = '\0';

		if ('\0' == buf[*pos]) {
			if (MDOC_PPHRASE & mdoc->flags)
				return(ARGS_QWORD);
			mdoc_pmsg(mdoc, line, *pos, MANDOCERR_BADQUOTE);
			return(ARGS_QWORD);
		}

		mdoc->flags &= ~MDOC_PHRASELIT;
		buf[(*pos)++] = '\0';

		if ('\0' == buf[*pos])
			return(ARGS_QWORD);

		while (' ' == buf[*pos])
			(*pos)++;

		if ('\0' == buf[*pos])
			mdoc_pmsg(mdoc, line, *pos, MANDOCERR_EOLNSPACE);

		return(ARGS_QWORD);
	}

	p = &buf[*pos];
	*v = mandoc_getarg(mdoc->parse, &p, line, pos);

	return(ARGS_WORD);
}

/* 
 * Check if the string consists only of space-separated closing
 * delimiters.  This is a bit of a dance: the first must be a close
 * delimiter, but it may be followed by middle delimiters.  Arbitrary
 * whitespace may separate these tokens.
 */
static int
args_checkpunct(const char *buf, int i)
{
	int		 j;
	char		 dbuf[DELIMSZ];
	enum mdelim	 d;

	/* First token must be a close-delimiter. */

	for (j = 0; buf[i] && ' ' != buf[i] && j < DELIMSZ; j++, i++)
		dbuf[j] = buf[i];

	if (DELIMSZ == j)
		return(0);

	dbuf[j] = '\0';
	if (DELIM_CLOSE != mdoc_isdelim(dbuf))
		return(0);

	while (' ' == buf[i])
		i++;

	/* Remaining must NOT be open/none. */
	
	while (buf[i]) {
		j = 0;
		while (buf[i] && ' ' != buf[i] && j < DELIMSZ)
			dbuf[j++] = buf[i++];

		if (DELIMSZ == j)
			return(0);

		dbuf[j] = '\0';
		d = mdoc_isdelim(dbuf);
		if (DELIM_NONE == d || DELIM_OPEN == d)
			return(0);

		while (' ' == buf[i])
			i++;
	}

	return('\0' == buf[i]);
}

static int
argv_multi(struct mdoc *mdoc, int line, 
		struct mdoc_argv *v, int *pos, char *buf)
{
	enum margserr	 ac;
	char		*p;

	for (v->sz = 0; ; v->sz++) {
		if ('-' == buf[*pos])
			break;
		ac = args(mdoc, line, pos, buf, ARGSFL_NONE, &p);
		if (ARGS_ERROR == ac)
			return(0);
		else if (ARGS_EOLN == ac)
			break;

		if (0 == v->sz % MULTI_STEP)
			v->value = mandoc_realloc(v->value, 
				(v->sz + MULTI_STEP) * sizeof(char *));

		v->value[(int)v->sz] = mandoc_strdup(p);
	}

	return(1);
}

static int
argv_single(struct mdoc *mdoc, int line, 
		struct mdoc_argv *v, int *pos, char *buf)
{
	enum margserr	 ac;
	char		*p;

	ac = args(mdoc, line, pos, buf, ARGSFL_NONE, &p);
	if (ARGS_ERROR == ac)
		return(0);
	if (ARGS_EOLN == ac)
		return(1);

	v->sz = 1;
	v->value = mandoc_malloc(sizeof(char *));
	v->value[0] = mandoc_strdup(p);

	return(1);
}
