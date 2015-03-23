/*	$Id: mansearch.c,v 1.55 2015/03/11 13:11:22 schwarze Exp $ */
/*
 * Copyright (c) 2012 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2013, 2014, 2015 Ingo Schwarze <schwarze@openbsd.org>
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
#include "config.h"

#include <sys/mman.h>
#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <glob.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if HAVE_OHASH
#include <ohash.h>
#else
#include "compat_ohash.h"
#endif
#include <sqlite3.h>
#ifndef SQLITE_DETERMINISTIC
#define SQLITE_DETERMINISTIC 0
#endif

#include "mandoc.h"
#include "mandoc_aux.h"
#include "manpath.h"
#include "mansearch.h"

extern int mansearch_keymax;
extern const char *const mansearch_keynames[];

#define	SQL_BIND_TEXT(_db, _s, _i, _v) \
	do { if (SQLITE_OK != sqlite3_bind_text \
		((_s), (_i)++, (_v), -1, SQLITE_STATIC)) \
		fprintf(stderr, "%s\n", sqlite3_errmsg((_db))); \
	} while (0)
#define	SQL_BIND_INT64(_db, _s, _i, _v) \
	do { if (SQLITE_OK != sqlite3_bind_int64 \
		((_s), (_i)++, (_v))) \
		fprintf(stderr, "%s\n", sqlite3_errmsg((_db))); \
	} while (0)
#define	SQL_BIND_BLOB(_db, _s, _i, _v) \
	do { if (SQLITE_OK != sqlite3_bind_blob \
		((_s), (_i)++, (&_v), sizeof(_v), SQLITE_STATIC)) \
		fprintf(stderr, "%s\n", sqlite3_errmsg((_db))); \
	} while (0)

struct	expr {
	regex_t		 regexp;  /* compiled regexp, if applicable */
	const char	*substr;  /* to search for, if applicable */
	struct expr	*next;    /* next in sequence */
	uint64_t	 bits;    /* type-mask */
	int		 equal;   /* equality, not subsring match */
	int		 open;    /* opening parentheses before */
	int		 and;	  /* logical AND before */
	int		 close;   /* closing parentheses after */
};

struct	match {
	uint64_t	 pageid; /* identifier in database */
	uint64_t	 bits; /* name type mask */
	char		*desc; /* manual page description */
	int		 form; /* bit field: formatted, zipped? */
};

static	void		 buildnames(const struct mansearch *,
				struct manpage *, sqlite3 *,
				sqlite3_stmt *, uint64_t,
				const char *, int form);
static	char		*buildoutput(sqlite3 *, sqlite3_stmt *,
				 uint64_t, uint64_t);
static	void		*hash_alloc(size_t, void *);
static	void		 hash_free(void *, void *);
static	void		*hash_calloc(size_t, size_t, void *);
static	struct expr	*exprcomp(const struct mansearch *,
				int, char *[]);
static	void		 exprfree(struct expr *);
static	struct expr	*exprterm(const struct mansearch *, char *, int);
static	int		 manpage_compare(const void *, const void *);
static	void		 sql_append(char **sql, size_t *sz,
				const char *newstr, int count);
static	void		 sql_match(sqlite3_context *context,
				int argc, sqlite3_value **argv);
static	void		 sql_regexp(sqlite3_context *context,
				int argc, sqlite3_value **argv);
static	char		*sql_statement(const struct expr *);


int
mansearch_setup(int start)
{
	static void	*pagecache;
	int		 c;

#define	PC_PAGESIZE	1280
#define	PC_NUMPAGES	256

	if (start) {
		if (NULL != pagecache) {
			fprintf(stderr, "pagecache already enabled\n");
			return((int)MANDOCLEVEL_BADARG);
		}

		pagecache = mmap(NULL, PC_PAGESIZE * PC_NUMPAGES,
		    PROT_READ | PROT_WRITE,
		    MAP_SHARED | MAP_ANON, -1, 0);

		if (MAP_FAILED == pagecache) {
			perror("mmap");
			pagecache = NULL;
			return((int)MANDOCLEVEL_SYSERR);
		}

		c = sqlite3_config(SQLITE_CONFIG_PAGECACHE,
		    pagecache, PC_PAGESIZE, PC_NUMPAGES);

		if (SQLITE_OK == c)
			return((int)MANDOCLEVEL_OK);

		fprintf(stderr, "pagecache: %s\n", sqlite3_errstr(c));

	} else if (NULL == pagecache) {
		fprintf(stderr, "pagecache missing\n");
		return((int)MANDOCLEVEL_BADARG);
	}

	if (-1 == munmap(pagecache, PC_PAGESIZE * PC_NUMPAGES)) {
		perror("munmap");
		pagecache = NULL;
		return((int)MANDOCLEVEL_SYSERR);
	}

	pagecache = NULL;
	return((int)MANDOCLEVEL_OK);
}

int
mansearch(const struct mansearch *search,
		const struct manpaths *paths,
		int argc, char *argv[],
		struct manpage **res, size_t *sz)
{
	int		 fd, rc, c, indexbit;
	int64_t		 pageid;
	uint64_t	 outbit, iterbit;
	char		 buf[PATH_MAX];
	char		*sql;
	struct manpage	*mpage;
	struct expr	*e, *ep;
	sqlite3		*db;
	sqlite3_stmt	*s, *s2;
	struct match	*mp;
	struct ohash_info info;
	struct ohash	 htab;
	unsigned int	 idx;
	size_t		 i, j, cur, maxres;

	info.calloc = hash_calloc;
	info.alloc = hash_alloc;
	info.free = hash_free;
	info.key_offset = offsetof(struct match, pageid);

	*sz = cur = maxres = 0;
	sql = NULL;
	*res = NULL;
	fd = -1;
	e = NULL;
	rc = 0;

	if (0 == argc)
		goto out;
	if (NULL == (e = exprcomp(search, argc, argv)))
		goto out;

	if (NULL != search->outkey) {
		outbit = TYPE_Nd;
		for (indexbit = 0, iterbit = 1;
		     indexbit < mansearch_keymax;
		     indexbit++, iterbit <<= 1) {
			if (0 == strcasecmp(search->outkey,
			    mansearch_keynames[indexbit])) {
				outbit = iterbit;
				break;
			}
		}
	} else
		outbit = 0;

	/*
	 * Save a descriptor to the current working directory.
	 * Since pathnames in the "paths" variable might be relative,
	 * and we'll be chdir()ing into them, we need to keep a handle
	 * on our current directory from which to start the chdir().
	 */

	if (NULL == getcwd(buf, PATH_MAX)) {
		perror("getcwd");
		goto out;
	} else if (-1 == (fd = open(buf, O_RDONLY, 0))) {
		perror(buf);
		goto out;
	}

	sql = sql_statement(e);

	/*
	 * Loop over the directories (containing databases) for us to
	 * search.
	 * Don't let missing/bad databases/directories phase us.
	 * In each, try to open the resident database and, if it opens,
	 * scan it for our match expression.
	 */

	for (i = 0; i < paths->sz; i++) {
		if (-1 == fchdir(fd)) {
			perror(buf);
			free(*res);
			break;
		} else if (-1 == chdir(paths->paths[i])) {
			perror(paths->paths[i]);
			continue;
		}

		c = sqlite3_open_v2(MANDOC_DB, &db,
		    SQLITE_OPEN_READONLY, NULL);

		if (SQLITE_OK != c) {
			fprintf(stderr, "%s/%s: %s\n",
			    paths->paths[i], MANDOC_DB, strerror(errno));
			sqlite3_close(db);
			continue;
		}

		/*
		 * Define the SQL functions for substring
		 * and regular expression matching.
		 */

		c = sqlite3_create_function(db, "match", 2,
		    SQLITE_UTF8 | SQLITE_DETERMINISTIC,
		    NULL, sql_match, NULL, NULL);
		assert(SQLITE_OK == c);
		c = sqlite3_create_function(db, "regexp", 2,
		    SQLITE_UTF8 | SQLITE_DETERMINISTIC,
		    NULL, sql_regexp, NULL, NULL);
		assert(SQLITE_OK == c);

		j = 1;
		c = sqlite3_prepare_v2(db, sql, -1, &s, NULL);
		if (SQLITE_OK != c)
			fprintf(stderr, "%s\n", sqlite3_errmsg(db));

		for (ep = e; NULL != ep; ep = ep->next) {
			if (NULL == ep->substr) {
				SQL_BIND_BLOB(db, s, j, ep->regexp);
			} else
				SQL_BIND_TEXT(db, s, j, ep->substr);
			if (0 == ((TYPE_Nd | TYPE_Nm) & ep->bits))
				SQL_BIND_INT64(db, s, j, ep->bits);
		}

		memset(&htab, 0, sizeof(struct ohash));
		ohash_init(&htab, 4, &info);

		/*
		 * Hash each entry on its [unique] document identifier.
		 * This is a uint64_t.
		 * Instead of using a hash function, simply convert the
		 * uint64_t to a uint32_t, the hash value's type.
		 * This gives good performance and preserves the
		 * distribution of buckets in the table.
		 */
		while (SQLITE_ROW == (c = sqlite3_step(s))) {
			pageid = sqlite3_column_int64(s, 2);
			idx = ohash_lookup_memory(&htab,
			    (char *)&pageid, sizeof(uint64_t),
			    (uint32_t)pageid);

			if (NULL != ohash_find(&htab, idx))
				continue;

			mp = mandoc_calloc(1, sizeof(struct match));
			mp->pageid = pageid;
			mp->form = sqlite3_column_int(s, 1);
			mp->bits = sqlite3_column_int64(s, 3);
			if (TYPE_Nd == outbit)
				mp->desc = mandoc_strdup((const char *)
				    sqlite3_column_text(s, 0));
			ohash_insert(&htab, idx, mp);
		}

		if (SQLITE_DONE != c)
			fprintf(stderr, "%s\n", sqlite3_errmsg(db));

		sqlite3_finalize(s);

		c = sqlite3_prepare_v2(db,
		    "SELECT sec, arch, name, pageid FROM mlinks "
		    "WHERE pageid=? ORDER BY sec, arch, name",
		    -1, &s, NULL);
		if (SQLITE_OK != c)
			fprintf(stderr, "%s\n", sqlite3_errmsg(db));

		c = sqlite3_prepare_v2(db,
		    "SELECT bits, key, pageid FROM keys "
		    "WHERE pageid=? AND bits & ?",
		    -1, &s2, NULL);
		if (SQLITE_OK != c)
			fprintf(stderr, "%s\n", sqlite3_errmsg(db));

		for (mp = ohash_first(&htab, &idx);
				NULL != mp;
				mp = ohash_next(&htab, &idx)) {
			if (cur + 1 > maxres) {
				maxres += 1024;
				*res = mandoc_reallocarray(*res,
				    maxres, sizeof(struct manpage));
			}
			mpage = *res + cur;
			mpage->ipath = i;
			mpage->bits = mp->bits;
			mpage->sec = 10;
			mpage->form = mp->form;
			buildnames(search, mpage, db, s, mp->pageid,
			    paths->paths[i], mp->form);
			if (mpage->names != NULL) {
				mpage->output = TYPE_Nd & outbit ?
				    mp->desc : outbit ?
				    buildoutput(db, s2, mp->pageid, outbit) :
				    NULL;
				cur++;
			}
			free(mp);
		}

		sqlite3_finalize(s);
		sqlite3_finalize(s2);
		sqlite3_close(db);
		ohash_delete(&htab);

		/*
		 * In man(1) mode, prefer matches in earlier trees
		 * over matches in later trees.
		 */

		if (cur && search->firstmatch)
			break;
	}
	qsort(*res, cur, sizeof(struct manpage), manpage_compare);
	rc = 1;
out:
	if (-1 != fd) {
		if (-1 == fchdir(fd))
			perror(buf);
		close(fd);
	}
	exprfree(e);
	free(sql);
	*sz = cur;
	return(rc);
}

void
mansearch_free(struct manpage *res, size_t sz)
{
	size_t	 i;

	for (i = 0; i < sz; i++) {
		free(res[i].file);
		free(res[i].names);
		free(res[i].output);
	}
	free(res);
}

static int
manpage_compare(const void *vp1, const void *vp2)
{
	const struct manpage	*mp1, *mp2;
	int			 diff;

	mp1 = vp1;
	mp2 = vp2;
	return(	(diff = mp2->bits - mp1->bits) ? diff :
		(diff = mp1->sec - mp2->sec) ? diff :
		strcasecmp(mp1->names, mp2->names));
}

static void
buildnames(const struct mansearch *search, struct manpage *mpage,
		sqlite3 *db, sqlite3_stmt *s,
		uint64_t pageid, const char *path, int form)
{
	glob_t		 globinfo;
	char		*firstname, *newnames, *prevsec, *prevarch;
	const char	*oldnames, *sep1, *name, *sec, *sep2, *arch, *fsec;
	size_t		 i;
	int		 c, globres;

	mpage->file = NULL;
	mpage->names = NULL;
	firstname = prevsec = prevarch = NULL;
	i = 1;
	SQL_BIND_INT64(db, s, i, pageid);
	while (SQLITE_ROW == (c = sqlite3_step(s))) {

		/* Decide whether we already have some names. */

		if (NULL == mpage->names) {
			oldnames = "";
			sep1 = "";
		} else {
			oldnames = mpage->names;
			sep1 = ", ";
		}

		/* Fetch the next name, rejecting sec/arch mismatches. */

		sec = (const char *)sqlite3_column_text(s, 0);
		if (search->sec != NULL && strcasecmp(sec, search->sec))
			continue;
		arch = (const char *)sqlite3_column_text(s, 1);
		if (search->arch != NULL && *arch != '\0' &&
		    strcasecmp(arch, search->arch))
			continue;
		name = (const char *)sqlite3_column_text(s, 2);

		/* Remember the first section found. */

		if (9 < mpage->sec && '1' <= *sec && '9' >= *sec)
			mpage->sec = (*sec - '1') + 1;

		/* If the section changed, append the old one. */

		if (NULL != prevsec &&
		    (strcmp(sec, prevsec) ||
		     strcmp(arch, prevarch))) {
			sep2 = '\0' == *prevarch ? "" : "/";
			mandoc_asprintf(&newnames, "%s(%s%s%s)",
			    oldnames, prevsec, sep2, prevarch);
			free(mpage->names);
			oldnames = mpage->names = newnames;
			free(prevsec);
			free(prevarch);
			prevsec = prevarch = NULL;
		}

		/* Save the new section, to append it later. */

		if (NULL == prevsec) {
			prevsec = mandoc_strdup(sec);
			prevarch = mandoc_strdup(arch);
		}

		/* Append the new name. */

		mandoc_asprintf(&newnames, "%s%s%s",
		    oldnames, sep1, name);
		free(mpage->names);
		mpage->names = newnames;

		/* Also save the first file name encountered. */

		if (mpage->file != NULL)
			continue;

		if (form & FORM_SRC) {
			sep1 = "man";
			fsec = sec;
		} else {
			sep1 = "cat";
			fsec = "0";
		}
		sep2 = *arch == '\0' ? "" : "/";
		mandoc_asprintf(&mpage->file, "%s/%s%s%s%s/%s.%s",
		    path, sep1, sec, sep2, arch, name, fsec);
		if (access(mpage->file, R_OK) != -1)
			continue;

		/* Handle unusual file name extensions. */

		if (firstname == NULL)
			firstname = mpage->file;
		else
			free(mpage->file);
		mandoc_asprintf(&mpage->file, "%s/%s%s%s%s/%s.*",
		    path, sep1, sec, sep2, arch, name);
		globres = glob(mpage->file, 0, NULL, &globinfo);
		free(mpage->file);
		mpage->file = globres ? NULL :
		    mandoc_strdup(*globinfo.gl_pathv);
		globfree(&globinfo);
	}
	if (c != SQLITE_DONE)
		fprintf(stderr, "%s\n", sqlite3_errmsg(db));
	sqlite3_reset(s);

	/* If none of the files is usable, use the first name. */

	if (mpage->file == NULL)
		mpage->file = firstname;
	else if (mpage->file != firstname)
		free(firstname);

	/* Append one final section to the names. */

	if (prevsec != NULL) {
		sep2 = *prevarch == '\0' ? "" : "/";
		mandoc_asprintf(&newnames, "%s(%s%s%s)",
		    mpage->names, prevsec, sep2, prevarch);
		free(mpage->names);
		mpage->names = newnames;
		free(prevsec);
		free(prevarch);
	}
}

static char *
buildoutput(sqlite3 *db, sqlite3_stmt *s, uint64_t pageid, uint64_t outbit)
{
	char		*output, *newoutput;
	const char	*oldoutput, *sep1, *data;
	size_t		 i;
	int		 c;

	output = NULL;
	i = 1;
	SQL_BIND_INT64(db, s, i, pageid);
	SQL_BIND_INT64(db, s, i, outbit);
	while (SQLITE_ROW == (c = sqlite3_step(s))) {
		if (NULL == output) {
			oldoutput = "";
			sep1 = "";
		} else {
			oldoutput = output;
			sep1 = " # ";
		}
		data = (const char *)sqlite3_column_text(s, 1);
		mandoc_asprintf(&newoutput, "%s%s%s",
		    oldoutput, sep1, data);
		free(output);
		output = newoutput;
	}
	if (SQLITE_DONE != c)
		fprintf(stderr, "%s\n", sqlite3_errmsg(db));
	sqlite3_reset(s);
	return(output);
}

/*
 * Implement substring match as an application-defined SQL function.
 * Using the SQL LIKE or GLOB operators instead would be a bad idea
 * because that would require escaping metacharacters in the string
 * being searched for.
 */
static void
sql_match(sqlite3_context *context, int argc, sqlite3_value **argv)
{

	assert(2 == argc);
	sqlite3_result_int(context, NULL != strcasestr(
	    (const char *)sqlite3_value_text(argv[1]),
	    (const char *)sqlite3_value_text(argv[0])));
}

/*
 * Implement regular expression match
 * as an application-defined SQL function.
 */
static void
sql_regexp(sqlite3_context *context, int argc, sqlite3_value **argv)
{

	assert(2 == argc);
	sqlite3_result_int(context, !regexec(
	    (regex_t *)sqlite3_value_blob(argv[0]),
	    (const char *)sqlite3_value_text(argv[1]),
	    0, NULL, 0));
}

static void
sql_append(char **sql, size_t *sz, const char *newstr, int count)
{
	size_t		 newsz;

	newsz = 1 < count ? (size_t)count : strlen(newstr);
	*sql = mandoc_realloc(*sql, *sz + newsz + 1);
	if (1 < count)
		memset(*sql + *sz, *newstr, (size_t)count);
	else
		memcpy(*sql + *sz, newstr, newsz);
	*sz += newsz;
	(*sql)[*sz] = '\0';
}

/*
 * Prepare the search SQL statement.
 */
static char *
sql_statement(const struct expr *e)
{
	char		*sql;
	size_t		 sz;
	int		 needop;

	sql = mandoc_strdup(e->equal ?
	    "SELECT desc, form, pageid, bits "
		"FROM mpages NATURAL JOIN names WHERE " :
	    "SELECT desc, form, pageid, 0 FROM mpages WHERE ");
	sz = strlen(sql);

	for (needop = 0; NULL != e; e = e->next) {
		if (e->and)
			sql_append(&sql, &sz, " AND ", 1);
		else if (needop)
			sql_append(&sql, &sz, " OR ", 1);
		if (e->open)
			sql_append(&sql, &sz, "(", e->open);
		sql_append(&sql, &sz,
		    TYPE_Nd & e->bits
		    ? (NULL == e->substr
			? "desc REGEXP ?"
			: "desc MATCH ?")
		    : TYPE_Nm == e->bits
		    ? (NULL == e->substr
			? "pageid IN (SELECT pageid FROM names "
			  "WHERE name REGEXP ?)"
			: e->equal
			? "name = ? "
			: "pageid IN (SELECT pageid FROM names "
			  "WHERE name MATCH ?)")
		    : (NULL == e->substr
			? "pageid IN (SELECT pageid FROM keys "
			  "WHERE key REGEXP ? AND bits & ?)"
			: "pageid IN (SELECT pageid FROM keys "
			  "WHERE key MATCH ? AND bits & ?)"), 1);
		if (e->close)
			sql_append(&sql, &sz, ")", e->close);
		needop = 1;
	}

	return(sql);
}

/*
 * Compile a set of string tokens into an expression.
 * Tokens in "argv" are assumed to be individual expression atoms (e.g.,
 * "(", "foo=bar", etc.).
 */
static struct expr *
exprcomp(const struct mansearch *search, int argc, char *argv[])
{
	uint64_t	 mask;
	int		 i, toopen, logic, igncase, toclose;
	struct expr	*first, *prev, *cur, *next;

	first = cur = NULL;
	logic = igncase = toopen = toclose = 0;

	for (i = 0; i < argc; i++) {
		if (0 == strcmp("(", argv[i])) {
			if (igncase)
				goto fail;
			toopen++;
			toclose++;
			continue;
		} else if (0 == strcmp(")", argv[i])) {
			if (toopen || logic || igncase || NULL == cur)
				goto fail;
			cur->close++;
			if (0 > --toclose)
				goto fail;
			continue;
		} else if (0 == strcmp("-a", argv[i])) {
			if (toopen || logic || igncase || NULL == cur)
				goto fail;
			logic = 1;
			continue;
		} else if (0 == strcmp("-o", argv[i])) {
			if (toopen || logic || igncase || NULL == cur)
				goto fail;
			logic = 2;
			continue;
		} else if (0 == strcmp("-i", argv[i])) {
			if (igncase)
				goto fail;
			igncase = 1;
			continue;
		}
		next = exprterm(search, argv[i], !igncase);
		if (NULL == next)
			goto fail;
		if (NULL == first)
			first = next;
		else
			cur->next = next;
		prev = cur = next;

		/*
		 * Searching for descriptions must be split out
		 * because they are stored in the mpages table,
		 * not in the keys table.
		 */

		for (mask = TYPE_Nm; mask <= TYPE_Nd; mask <<= 1) {
			if (mask & cur->bits && ~mask & cur->bits) {
				next = mandoc_calloc(1,
				    sizeof(struct expr));
				memcpy(next, cur, sizeof(struct expr));
				prev->open = 1;
				cur->bits = mask;
				cur->next = next;
				cur = next;
				cur->bits &= ~mask;
			}
		}
		prev->and = (1 == logic);
		prev->open += toopen;
		if (cur != prev)
			cur->close = 1;

		toopen = logic = igncase = 0;
	}
	if ( ! (toopen || logic || igncase || toclose))
		return(first);

fail:
	if (NULL != first)
		exprfree(first);
	return(NULL);
}

static struct expr *
exprterm(const struct mansearch *search, char *buf, int cs)
{
	char		 errbuf[BUFSIZ];
	struct expr	*e;
	char		*key, *val;
	uint64_t	 iterbit;
	int		 i, irc;

	if ('\0' == *buf)
		return(NULL);

	e = mandoc_calloc(1, sizeof(struct expr));

	if (search->argmode == ARG_NAME) {
		e->bits = TYPE_Nm;
		e->substr = buf;
		e->equal = 1;
		return(e);
	}

	/*
	 * Separate macro keys from search string.
	 * If needed, request regular expression handling
	 * by setting e->substr to NULL.
	 */

	if (search->argmode == ARG_WORD) {
		e->bits = TYPE_Nm;
		e->substr = NULL;
		mandoc_asprintf(&val, "[[:<:]]%s[[:>:]]", buf);
		cs = 0;
	} else if ((val = strpbrk(buf, "=~")) == NULL) {
		e->bits = TYPE_Nm | TYPE_Nd;
		e->substr = buf;
	} else {
		if (val == buf)
			e->bits = TYPE_Nm | TYPE_Nd;
		if ('=' == *val)
			e->substr = val + 1;
		*val++ = '\0';
		if (NULL != strstr(buf, "arch"))
			cs = 0;
	}

	/* Compile regular expressions. */

	if (NULL == e->substr) {
		irc = regcomp(&e->regexp, val,
		    REG_EXTENDED | REG_NOSUB | (cs ? 0 : REG_ICASE));
		if (search->argmode == ARG_WORD)
			free(val);
		if (irc) {
			regerror(irc, &e->regexp, errbuf, sizeof(errbuf));
			fprintf(stderr, "regcomp: %s\n", errbuf);
			free(e);
			return(NULL);
		}
	}

	if (e->bits)
		return(e);

	/*
	 * Parse out all possible fields.
	 * If the field doesn't resolve, bail.
	 */

	while (NULL != (key = strsep(&buf, ","))) {
		if ('\0' == *key)
			continue;
		for (i = 0, iterbit = 1;
		     i < mansearch_keymax;
		     i++, iterbit <<= 1) {
			if (0 == strcasecmp(key,
			    mansearch_keynames[i])) {
				e->bits |= iterbit;
				break;
			}
		}
		if (i == mansearch_keymax) {
			if (strcasecmp(key, "any")) {
				free(e);
				return(NULL);
			}
			e->bits |= ~0ULL;
		}
	}

	return(e);
}

static void
exprfree(struct expr *p)
{
	struct expr	*pp;

	while (NULL != p) {
		pp = p->next;
		free(p);
		p = pp;
	}
}

static void *
hash_calloc(size_t nmemb, size_t sz, void *arg)
{

	return(mandoc_calloc(nmemb, sz));
}

static void *
hash_alloc(size_t sz, void *arg)
{

	return(mandoc_malloc(sz));
}

static void
hash_free(void *p, void *arg)
{

	free(p);
}
