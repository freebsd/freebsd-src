/*	$Id: apropos_db.c,v 1.32.2.3 2013/10/10 23:43:04 schwarze Exp $ */
/*
 * Copyright (c) 2011, 2012 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011 Ingo Schwarze <schwarze@openbsd.org>
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

#include <sys/param.h>

#include <assert.h>
#include <fcntl.h>
#include <regex.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(__APPLE__)
# include <libkern/OSByteOrder.h>
#elif defined(__linux__)
# include <endian.h>
#elif defined(__sun)
# include <sys/byteorder.h>
#else
# include <sys/endian.h>
#endif

#if defined(__linux__) || defined(__sun)
# include <db_185.h>
#else
# include <db.h>
#endif

#include "mandocdb.h"
#include "apropos_db.h"
#include "mandoc.h"

#define	RESFREE(_x) \
	do { \
		free((_x)->file); \
		free((_x)->cat); \
		free((_x)->title); \
		free((_x)->arch); \
		free((_x)->desc); \
		free((_x)->matches); \
	} while (/*CONSTCOND*/0)

struct	expr {
	int		 regex; /* is regex? */
	int		 index; /* index in match array */
	uint64_t 	 mask; /* type-mask */
	int		 and; /* is rhs of logical AND? */
	char		*v; /* search value */
	regex_t	 	 re; /* compiled re, if regex */
	struct expr	*next; /* next in sequence */
	struct expr	*subexpr;
};

struct	type {
	uint64_t	 mask;
	const char	*name;
};

struct	rectree {
	struct res	*node; /* record array for dir tree */
	int		 len; /* length of record array */
};

static	const struct type types[] = {
	{ TYPE_An, "An" },
	{ TYPE_Ar, "Ar" },
	{ TYPE_At, "At" },
	{ TYPE_Bsx, "Bsx" },
	{ TYPE_Bx, "Bx" },
	{ TYPE_Cd, "Cd" },
	{ TYPE_Cm, "Cm" },
	{ TYPE_Dv, "Dv" },
	{ TYPE_Dx, "Dx" },
	{ TYPE_Em, "Em" },
	{ TYPE_Er, "Er" },
	{ TYPE_Ev, "Ev" },
	{ TYPE_Fa, "Fa" },
	{ TYPE_Fl, "Fl" },
	{ TYPE_Fn, "Fn" },
	{ TYPE_Fn, "Fo" },
	{ TYPE_Ft, "Ft" },
	{ TYPE_Fx, "Fx" },
	{ TYPE_Ic, "Ic" },
	{ TYPE_In, "In" },
	{ TYPE_Lb, "Lb" },
	{ TYPE_Li, "Li" },
	{ TYPE_Lk, "Lk" },
	{ TYPE_Ms, "Ms" },
	{ TYPE_Mt, "Mt" },
	{ TYPE_Nd, "Nd" },
	{ TYPE_Nm, "Nm" },
	{ TYPE_Nx, "Nx" },
	{ TYPE_Ox, "Ox" },
	{ TYPE_Pa, "Pa" },
	{ TYPE_Rs, "Rs" },
	{ TYPE_Sh, "Sh" },
	{ TYPE_Ss, "Ss" },
	{ TYPE_St, "St" },
	{ TYPE_Sy, "Sy" },
	{ TYPE_Tn, "Tn" },
	{ TYPE_Va, "Va" },
	{ TYPE_Va, "Vt" },
	{ TYPE_Xr, "Xr" },
	{ UINT64_MAX, "any" },
	{ 0, NULL }
};

static	DB	*btree_open(void);
static	int	 btree_read(const DBT *, const DBT *,
			const struct mchars *,
			uint64_t *, recno_t *, char **);
static	int	 expreval(const struct expr *, int *);
static	void	 exprexec(const struct expr *,
			const char *, uint64_t, struct res *);
static	int	 exprmark(const struct expr *,
			const char *, uint64_t, int *);
static	struct expr *exprexpr(int, char *[], int *, int *, size_t *);
static	struct expr *exprterm(char *, int);
static	DB	*index_open(void);
static	int	 index_read(const DBT *, const DBT *, int,
			const struct mchars *, struct res *);
static	void	 norm_string(const char *,
			const struct mchars *, char **);
static	size_t	 norm_utf8(unsigned int, char[7]);
static	int	 single_search(struct rectree *, const struct opts *,
			const struct expr *, size_t terms,
			struct mchars *, int);

/*
 * Open the keyword mandoc-db database.
 */
static DB *
btree_open(void)
{
	BTREEINFO	 info;
	DB		*db;

	memset(&info, 0, sizeof(BTREEINFO));
	info.lorder = 4321;
	info.flags = R_DUP;

	db = dbopen(MANDOC_DB, O_RDONLY, 0, DB_BTREE, &info);
	if (NULL != db)
		return(db);

	return(NULL);
}

/*
 * Read a keyword from the database and normalise it.
 * Return 0 if the database is insane, else 1.
 */
static int
btree_read(const DBT *k, const DBT *v, const struct mchars *mc,
		uint64_t *mask, recno_t *rec, char **buf)
{
	uint64_t	 vbuf[2];

	/* Are our sizes sane? */
	if (k->size < 2 || sizeof(vbuf) != v->size)
		return(0);

	/* Is our string nil-terminated? */
	if ('\0' != ((const char *)k->data)[(int)k->size - 1])
		return(0);

	norm_string((const char *)k->data, mc, buf);
	memcpy(vbuf, v->data, v->size);
	*mask = betoh64(vbuf[0]);
	*rec  = betoh64(vbuf[1]);
	return(1);
}

/*
 * Take a Unicode codepoint and produce its UTF-8 encoding.
 * This isn't the best way to do this, but it works.
 * The magic numbers are from the UTF-8 packaging.
 * They're not as scary as they seem: read the UTF-8 spec for details.
 */
static size_t
norm_utf8(unsigned int cp, char out[7])
{
	int		 rc;

	rc = 0;

	if (cp <= 0x0000007F) {
		rc = 1;
		out[0] = (char)cp;
	} else if (cp <= 0x000007FF) {
		rc = 2;
		out[0] = (cp >> 6  & 31) | 192;
		out[1] = (cp       & 63) | 128;
	} else if (cp <= 0x0000FFFF) {
		rc = 3;
		out[0] = (cp >> 12 & 15) | 224;
		out[1] = (cp >> 6  & 63) | 128;
		out[2] = (cp       & 63) | 128;
	} else if (cp <= 0x001FFFFF) {
		rc = 4;
		out[0] = (cp >> 18 & 7) | 240;
		out[1] = (cp >> 12 & 63) | 128;
		out[2] = (cp >> 6  & 63) | 128;
		out[3] = (cp       & 63) | 128;
	} else if (cp <= 0x03FFFFFF) {
		rc = 5;
		out[0] = (cp >> 24 & 3) | 248;
		out[1] = (cp >> 18 & 63) | 128;
		out[2] = (cp >> 12 & 63) | 128;
		out[3] = (cp >> 6  & 63) | 128;
		out[4] = (cp       & 63) | 128;
	} else if (cp <= 0x7FFFFFFF) {
		rc = 6;
		out[0] = (cp >> 30 & 1) | 252;
		out[1] = (cp >> 24 & 63) | 128;
		out[2] = (cp >> 18 & 63) | 128;
		out[3] = (cp >> 12 & 63) | 128;
		out[4] = (cp >> 6  & 63) | 128;
		out[5] = (cp       & 63) | 128;
	} else
		return(0);

	out[rc] = '\0';
	return((size_t)rc);
}

/*
 * Normalise strings from the index and database.
 * These strings are escaped as defined by mandoc_char(7) along with
 * other goop in mandoc.h (e.g., soft hyphens).
 * This function normalises these into a nice UTF-8 string.
 * Returns 0 if the database is fucked.
 */
static void
norm_string(const char *val, const struct mchars *mc, char **buf)
{
	size_t		  sz, bsz;
	char		  utfbuf[7];
	const char	 *seq, *cpp;
	int		  len, u, pos;
	enum mandoc_esc	  esc;
	static const char res[] = { '\\', '\t',
				ASCII_NBRSP, ASCII_HYPH, '\0' };

	/* Pre-allocate by the length of the input */

	bsz = strlen(val) + 1;
	*buf = mandoc_realloc(*buf, bsz);
	pos = 0;

	while ('\0' != *val) {
		/*
		 * Halt on the first escape sequence.
		 * This also halts on the end of string, in which case
		 * we just copy, fallthrough, and exit the loop.
		 */
		if ((sz = strcspn(val, res)) > 0) {
			memcpy(&(*buf)[pos], val, sz);
			pos += (int)sz;
			val += (int)sz;
		}

		if (ASCII_HYPH == *val) {
			(*buf)[pos++] = '-';
			val++;
			continue;
		} else if ('\t' == *val || ASCII_NBRSP == *val) {
			(*buf)[pos++] = ' ';
			val++;
			continue;
		} else if ('\\' != *val)
			break;

		/* Read past the slash. */

		val++;
		u = 0;

		/*
		 * Parse the escape sequence and see if it's a
		 * predefined character or special character.
		 */

		esc = mandoc_escape(&val, &seq, &len);
		if (ESCAPE_ERROR == esc)
			break;

		/*
		 * XXX - this just does UTF-8, but we need to know
		 * beforehand whether we should do text substitution.
		 */

		switch (esc) {
		case (ESCAPE_SPECIAL):
			if (0 != (u = mchars_spec2cp(mc, seq, len)))
				break;
			/* FALLTHROUGH */
		default:
			continue;
		}

		/*
		 * If we have a Unicode codepoint, try to convert that
		 * to a UTF-8 byte string.
		 */

		cpp = utfbuf;
		if (0 == (sz = norm_utf8(u, utfbuf)))
			continue;

		/* Copy the rendered glyph into the stream. */

		sz = strlen(cpp);
		bsz += sz;

		*buf = mandoc_realloc(*buf, bsz);

		memcpy(&(*buf)[pos], cpp, sz);
		pos += (int)sz;
	}

	(*buf)[pos] = '\0';
}

/*
 * Open the filename-index mandoc-db database.
 * Returns NULL if opening failed.
 */
static DB *
index_open(void)
{
	DB		*db;

	db = dbopen(MANDOC_IDX, O_RDONLY, 0, DB_RECNO, NULL);
	if (NULL != db)
		return(db);

	return(NULL);
}

/*
 * Safely unpack from an index file record into the structure.
 * Returns 1 if an entry was unpacked, 0 if the database is insane.
 */
static int
index_read(const DBT *key, const DBT *val, int index,
		const struct mchars *mc, struct res *rec)
{
	size_t		 left;
	char		*np, *cp;
	char		 type;

#define	INDEX_BREAD(_dst) \
	do { \
		if (NULL == (np = memchr(cp, '\0', left))) \
			return(0); \
		norm_string(cp, mc, &(_dst)); \
		left -= (np - cp) + 1; \
		cp = np + 1; \
	} while (/* CONSTCOND */ 0)

	if (0 == (left = val->size))
		return(0);

	cp = val->data;
	assert(sizeof(recno_t) == key->size);
	memcpy(&rec->rec, key->data, key->size);
	rec->volume = index;

	if ('d' == (type = *cp++))
		rec->type = RESTYPE_MDOC;
	else if ('a' == type)
		rec->type = RESTYPE_MAN;
	else if ('c' == type)
		rec->type = RESTYPE_CAT;
	else
		return(0);

	left--;
	INDEX_BREAD(rec->file);
	INDEX_BREAD(rec->cat);
	INDEX_BREAD(rec->title);
	INDEX_BREAD(rec->arch);
	INDEX_BREAD(rec->desc);
	return(1);
}

/*
 * Search mandocdb databases in paths for expression "expr".
 * Filter out by "opts".
 * Call "res" with the results, which may be zero.
 * Return 0 if there was a database error, else return 1.
 */
int
apropos_search(int pathsz, char **paths, const struct opts *opts,
		const struct expr *expr, size_t terms, void *arg,
		size_t *sz, struct res **resp,
		void (*res)(struct res *, size_t, void *))
{
	struct rectree	 tree;
	struct mchars	*mc;
	int		 i;

	memset(&tree, 0, sizeof(struct rectree));

	mc = mchars_alloc();
	*sz = 0;
	*resp = NULL;

	/*
	 * Main loop.  Change into the directory containing manpage
	 * databases.  Run our expession over each database in the set.
	 */

	for (i = 0; i < pathsz; i++) {
		assert('/' == paths[i][0]);
		if (chdir(paths[i]))
			continue;
		if (single_search(&tree, opts, expr, terms, mc, i))
			continue;

		resfree(tree.node, tree.len);
		mchars_free(mc);
		return(0);
	}

	(*res)(tree.node, tree.len, arg);
	*sz = tree.len;
	*resp = tree.node;
	mchars_free(mc);
	return(1);
}

static int
single_search(struct rectree *tree, const struct opts *opts,
		const struct expr *expr, size_t terms,
		struct mchars *mc, int vol)
{
	int		 root, leaf, ch;
	DBT		 key, val;
	DB		*btree, *idx;
	char		*buf;
	struct res	*rs;
	struct res	 r;
	uint64_t	 mask;
	recno_t		 rec;

	root	= -1;
	leaf	= -1;
	btree	= NULL;
	idx	= NULL;
	buf	= NULL;
	rs	= tree->node;

	memset(&r, 0, sizeof(struct res));

	if (NULL == (btree = btree_open()))
		return(1);

	if (NULL == (idx = index_open())) {
		(*btree->close)(btree);
		return(1);
	}

	while (0 == (ch = (*btree->seq)(btree, &key, &val, R_NEXT))) {
		if ( ! btree_read(&key, &val, mc, &mask, &rec, &buf))
			break;

		/*
		 * See if this keyword record matches any of the
		 * expressions we have stored.
		 */
		if ( ! exprmark(expr, buf, mask, NULL))
			continue;

		/*
		 * O(log n) scan for prior records.  Since a record
		 * number is unbounded, this has decent performance over
		 * a complex hash function.
		 */

		for (leaf = root; leaf >= 0; )
			if (rec > rs[leaf].rec &&
					rs[leaf].rhs >= 0)
				leaf = rs[leaf].rhs;
			else if (rec < rs[leaf].rec &&
					rs[leaf].lhs >= 0)
				leaf = rs[leaf].lhs;
			else
				break;

		/*
		 * If we find a record, see if it has already evaluated
		 * to true.  If it has, great, just keep going.  If not,
		 * try to evaluate it now and continue anyway.
		 */

		if (leaf >= 0 && rs[leaf].rec == rec) {
			if (0 == rs[leaf].matched)
				exprexec(expr, buf, mask, &rs[leaf]);
			continue;
		}

		/*
		 * We have a new file to examine.
		 * Extract the manpage's metadata from the index
		 * database, then begin partial evaluation.
		 */

		key.data = &rec;
		key.size = sizeof(recno_t);

		if (0 != (*idx->get)(idx, &key, &val, 0))
			break;

		r.lhs = r.rhs = -1;
		if ( ! index_read(&key, &val, vol, mc, &r))
			break;

		/* XXX: this should be elsewhere, I guess? */

		if (opts->cat && strcasecmp(opts->cat, r.cat))
			continue;

		if (opts->arch && *r.arch)
			if (strcasecmp(opts->arch, r.arch))
				continue;

		tree->node = rs = mandoc_realloc
			(rs, (tree->len + 1) * sizeof(struct res));

		memcpy(&rs[tree->len], &r, sizeof(struct res));
		memset(&r, 0, sizeof(struct res));
		rs[tree->len].matches =
			mandoc_calloc(terms, sizeof(int));

		exprexec(expr, buf, mask, &rs[tree->len]);

		/* Append to our tree. */

		if (leaf >= 0) {
			if (rec > rs[leaf].rec)
				rs[leaf].rhs = tree->len;
			else
				rs[leaf].lhs = tree->len;
		} else
			root = tree->len;

		tree->len++;
	}

	(*btree->close)(btree);
	(*idx->close)(idx);

	free(buf);
	RESFREE(&r);
	return(1 == ch);
}

void
resfree(struct res *rec, size_t sz)
{
	size_t		 i;

	for (i = 0; i < sz; i++)
		RESFREE(&rec[i]);
	free(rec);
}

/*
 * Compile a list of straight-up terms.
 * The arguments are re-written into ~[[:<:]]term[[:>:]], or "term"
 * surrounded by word boundaries, then pumped through exprterm().
 * Terms are case-insensitive.
 * This emulates whatis(1) behaviour.
 */
struct expr *
termcomp(int argc, char *argv[], size_t *tt)
{
	char		*buf;
	int		 pos;
	struct expr	*e, *next;
	size_t		 sz;

	buf = NULL;
	e = NULL;
	*tt = 0;

	for (pos = argc - 1; pos >= 0; pos--) {
		sz = strlen(argv[pos]) + 18;
		buf = mandoc_realloc(buf, sz);
		strlcpy(buf, "Nm~[[:<:]]", sz);
		strlcat(buf, argv[pos], sz);
		strlcat(buf, "[[:>:]]", sz);
		if (NULL == (next = exprterm(buf, 0))) {
			free(buf);
			exprfree(e);
			return(NULL);
		}
		next->next = e;
		e = next;
		(*tt)++;
	}

	free(buf);
	return(e);
}

/*
 * Compile a sequence of logical expressions.
 * See apropos.1 for a grammar of this sequence.
 */
struct expr *
exprcomp(int argc, char *argv[], size_t *tt)
{
	int		 pos, lvl;
	struct expr	*e;

	pos = lvl = 0;
	*tt = 0;

	e = exprexpr(argc, argv, &pos, &lvl, tt);

	if (0 == lvl && pos >= argc)
		return(e);

	exprfree(e);
	return(NULL);
}

/*
 * Compile an array of tokens into an expression.
 * An informal expression grammar is defined in apropos(1).
 * Return NULL if we fail doing so.  All memory will be cleaned up.
 * Return the root of the expression sequence if alright.
 */
static struct expr *
exprexpr(int argc, char *argv[], int *pos, int *lvl, size_t *tt)
{
	struct expr	*e, *first, *next;
	int		 log;

	first = next = NULL;

	for ( ; *pos < argc; (*pos)++) {
		e = next;

		/*
		 * Close out a subexpression.
		 */

		if (NULL != e && 0 == strcmp(")", argv[*pos])) {
			if (--(*lvl) < 0)
				goto err;
			break;
		}

		/*
		 * Small note: if we're just starting, don't let "-a"
		 * and "-o" be considered logical operators: they're
		 * just tokens unless pairwise joining, in which case we
		 * record their existence (or assume "OR").
		 */
		log = 0;

		if (NULL != e && 0 == strcmp("-a", argv[*pos]))
			log = 1;
		else if (NULL != e && 0 == strcmp("-o", argv[*pos]))
			log = 2;

		if (log > 0 && ++(*pos) >= argc)
			goto err;

		/*
		 * Now we parse the term part.  This can begin with
		 * "-i", in which case the expression is case
		 * insensitive.
		 */

		if (0 == strcmp("(", argv[*pos])) {
			++(*pos);
			++(*lvl);
			next = mandoc_calloc(1, sizeof(struct expr));
			next->subexpr = exprexpr(argc, argv, pos, lvl, tt);
			if (NULL == next->subexpr) {
				free(next);
				next = NULL;
			}
		} else if (0 == strcmp("-i", argv[*pos])) {
			if (++(*pos) >= argc)
				goto err;
			next = exprterm(argv[*pos], 0);
		} else
			next = exprterm(argv[*pos], 1);

		if (NULL == next)
			goto err;

		next->and = log == 1;
		next->index = (int)(*tt)++;

		/* Append to our chain of expressions. */

		if (NULL == first) {
			assert(NULL == e);
			first = next;
		} else {
			assert(NULL != e);
			e->next = next;
		}
	}

	return(first);
err:
	exprfree(first);
	return(NULL);
}

/*
 * Parse a terminal expression with the grammar as defined in
 * apropos(1).
 * Return NULL if we fail the parse.
 */
static struct expr *
exprterm(char *buf, int cs)
{
	struct expr	 e;
	struct expr	*p;
	char		*key;
	int		 i;

	memset(&e, 0, sizeof(struct expr));

	/* Choose regex or substring match. */

	if (NULL == (e.v = strpbrk(buf, "=~"))) {
		e.regex = 0;
		e.v = buf;
	} else {
		e.regex = '~' == *e.v;
		*e.v++ = '\0';
	}

	/* Determine the record types to search for. */

	e.mask = 0;
	if (buf < e.v) {
		while (NULL != (key = strsep(&buf, ","))) {
			i = 0;
			while (types[i].mask &&
					strcmp(types[i].name, key))
				i++;
			e.mask |= types[i].mask;
		}
	}
	if (0 == e.mask)
		e.mask = TYPE_Nm | TYPE_Nd;

	if (e.regex) {
		i = REG_EXTENDED | REG_NOSUB | (cs ? 0 : REG_ICASE);
		if (regcomp(&e.re, e.v, i))
			return(NULL);
	}

	e.v = mandoc_strdup(e.v);

	p = mandoc_calloc(1, sizeof(struct expr));
	memcpy(p, &e, sizeof(struct expr));
	return(p);
}

void
exprfree(struct expr *p)
{
	struct expr	*pp;

	while (NULL != p) {
		if (p->subexpr)
			exprfree(p->subexpr);
		if (p->regex)
			regfree(&p->re);
		free(p->v);
		pp = p->next;
		free(p);
		p = pp;
	}
}

static int
exprmark(const struct expr *p, const char *cp,
		uint64_t mask, int *ms)
{

	for ( ; p; p = p->next) {
		if (p->subexpr) {
			if (exprmark(p->subexpr, cp, mask, ms))
				return(1);
			continue;
		} else if ( ! (mask & p->mask))
			continue;

		if (p->regex) {
			if (regexec(&p->re, cp, 0, NULL, 0))
				continue;
		} else if (NULL == strcasestr(cp, p->v))
			continue;

		if (NULL == ms)
			return(1);
		else
			ms[p->index] = 1;
	}

	return(0);
}

static int
expreval(const struct expr *p, int *ms)
{
	int		 match;

	/*
	 * AND has precedence over OR.  Analysis is left-right, though
	 * it doesn't matter because there are no side-effects.
	 * Thus, step through pairwise ANDs and accumulate their Boolean
	 * evaluation.  If we encounter a single true AND collection or
	 * standalone term, the whole expression is true (by definition
	 * of OR).
	 */

	for (match = 0; p && ! match; p = p->next) {
		/* Evaluate a subexpression, if applicable. */
		if (p->subexpr && ! ms[p->index])
			ms[p->index] = expreval(p->subexpr, ms);

		match = ms[p->index];
		for ( ; p->next && p->next->and; p = p->next) {
			/* Evaluate a subexpression, if applicable. */
			if (p->next->subexpr && ! ms[p->next->index])
				ms[p->next->index] =
					expreval(p->next->subexpr, ms);
			match = match && ms[p->next->index];
		}
	}

	return(match);
}

/*
 * First, update the array of terms for which this expression evaluates
 * to true.
 * Second, logically evaluate all terms over the updated array of truth
 * values.
 * If this evaluates to true, mark the expression as satisfied.
 */
static void
exprexec(const struct expr *e, const char *cp,
		uint64_t mask, struct res *r)
{

	assert(0 == r->matched);
	exprmark(e, cp, mask, r->matches);
	r->matched = expreval(e, r->matches);
}
