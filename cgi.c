/*	$Id: cgi.c,v 1.46 2013/10/11 00:06:48 schwarze Exp $ */
/*
 * Copyright (c) 2011, 2012 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <sys/wait.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(__sun)
/* for stat() */
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include "apropos_db.h"
#include "mandoc.h"
#include "mdoc.h"
#include "man.h"
#include "main.h"
#include "manpath.h"
#include "mandocdb.h"

#if defined(__linux__) || defined(__sun)
# include <db_185.h>
#else
# include <db.h>
#endif

enum	page {
	PAGE_INDEX,
	PAGE_SEARCH,
	PAGE_SHOW,
	PAGE__MAX
};

struct	paths {
	char		*name;
	char		*path;
};

/*
 * A query as passed to the search function.
 */
struct	query {
	const char	*arch; /* architecture */
	const char	*sec; /* manual section */
	const char	*expr; /* unparsed expression string */
	int		 manroot; /* manroot index (or -1)*/
	int		 legacy; /* whether legacy mode */
};

struct	req {
	struct query	 q;
	struct paths	*p;
	size_t		 psz;
	enum page	 page;
};

static	int		 atou(const char *, unsigned *);
static	void		 catman(const struct req *, const char *);
static	int	 	 cmp(const void *, const void *);
static	void		 format(const struct req *, const char *);
static	void		 html_print(const char *);
static	void		 html_printquery(const struct req *);
static	void		 html_putchar(char);
static	int 		 http_decode(char *);
static	void		 http_parse(struct req *, char *);
static	void		 http_print(const char *);
static	void 		 http_putchar(char);
static	void		 http_printquery(const struct req *);
static	int		 pathstop(DIR *);
static	void		 pathgen(DIR *, char *, struct req *);
static	void		 pg_index(const struct req *, char *);
static	void		 pg_search(const struct req *, char *);
static	void		 pg_show(const struct req *, char *);
static	void		 resp_bad(void);
static	void		 resp_baddb(void);
static	void		 resp_error400(void);
static	void		 resp_error404(const char *);
static	void		 resp_begin_html(int, const char *);
static	void		 resp_begin_http(int, const char *);
static	void		 resp_end_html(void);
static	void		 resp_index(const struct req *);
static	void		 resp_search(struct res *, size_t, void *);
static	void		 resp_searchform(const struct req *);

static	const char	 *progname; /* cgi script name */
static	const char	 *cache; /* cache directory */
static	const char	 *css; /* css directory */
static	const char	 *host; /* hostname */

static	const char * const pages[PAGE__MAX] = {
	"index", /* PAGE_INDEX */ 
	"search", /* PAGE_SEARCH */
	"show", /* PAGE_SHOW */
};

/*
 * This is just OpenBSD's strtol(3) suggestion.
 * I use it instead of strtonum(3) for portability's sake.
 */
static int
atou(const char *buf, unsigned *v)
{
	char		*ep;
	long		 lval;

	errno = 0;
	lval = strtol(buf, &ep, 10);
	if (buf[0] == '\0' || *ep != '\0')
		return(0);
	if ((errno == ERANGE && (lval == LONG_MAX || 
					lval == LONG_MIN)) ||
			(lval > INT_MAX || lval < 0))
		return(0);

	*v = (unsigned int)lval;
	return(1);
}

/*
 * Print a character, escaping HTML along the way.
 * This will pass non-ASCII straight to output: be warned!
 */
static void
html_putchar(char c)
{

	switch (c) {
	case ('"'):
		printf("&quote;");
		break;
	case ('&'):
		printf("&amp;");
		break;
	case ('>'):
		printf("&gt;");
		break;
	case ('<'):
		printf("&lt;");
		break;
	default:
		putchar((unsigned char)c);
		break;
	}
}
static void
http_printquery(const struct req *req)
{

	printf("&expr=");
	http_print(req->q.expr ? req->q.expr : "");
	printf("&sec=");
	http_print(req->q.sec ? req->q.sec : "");
	printf("&arch=");
	http_print(req->q.arch ? req->q.arch : "");
}


static void
html_printquery(const struct req *req)
{

	printf("&amp;expr=");
	html_print(req->q.expr ? req->q.expr : "");
	printf("&amp;sec=");
	html_print(req->q.sec ? req->q.sec : "");
	printf("&amp;arch=");
	html_print(req->q.arch ? req->q.arch : "");
}

static void
http_print(const char *p)
{

	if (NULL == p)
		return;
	while ('\0' != *p)
		http_putchar(*p++);
}

/*
 * Call through to html_putchar().
 * Accepts NULL strings.
 */
static void
html_print(const char *p)
{
	
	if (NULL == p)
		return;
	while ('\0' != *p)
		html_putchar(*p++);
}

/*
 * Parse out key-value pairs from an HTTP request variable.
 * This can be either a cookie or a POST/GET string, although man.cgi
 * uses only GET for simplicity.
 */
static void
http_parse(struct req *req, char *p)
{
	char            *key, *val, *manroot;
	int		 i, legacy;

	memset(&req->q, 0, sizeof(struct query));

	legacy = -1;
	manroot = NULL;

	while ('\0' != *p) {
		key = p;
		val = NULL;

		p += (int)strcspn(p, ";&");
		if ('\0' != *p)
			*p++ = '\0';
		if (NULL != (val = strchr(key, '=')))
			*val++ = '\0';

		if ('\0' == *key || NULL == val || '\0' == *val)
			continue;

		/* Just abort handling. */

		if ( ! http_decode(key))
			break;
		if (NULL != val && ! http_decode(val))
			break;

		if (0 == strcmp(key, "expr"))
			req->q.expr = val;
		else if (0 == strcmp(key, "query"))
			req->q.expr = val;
		else if (0 == strcmp(key, "sec"))
			req->q.sec = val;
		else if (0 == strcmp(key, "sektion"))
			req->q.sec = val;
		else if (0 == strcmp(key, "arch"))
			req->q.arch = val;
		else if (0 == strcmp(key, "manpath"))
			manroot = val;
		else if (0 == strcmp(key, "apropos"))
			legacy = 0 == strcmp(val, "0");
	}

	/* Test for old man.cgi compatibility mode. */

	req->q.legacy = legacy > 0;

	/* 
	 * Section "0" means no section when in legacy mode.
	 * For some man.cgi scripts, "default" arch is none.
	 */

	if (req->q.legacy && NULL != req->q.sec)
		if (0 == strcmp(req->q.sec, "0"))
			req->q.sec = NULL;
	if (req->q.legacy && NULL != req->q.arch)
		if (0 == strcmp(req->q.arch, "default"))
			req->q.arch = NULL;

	/* Default to first manroot. */

	if (NULL != manroot) {
		for (i = 0; i < (int)req->psz; i++)
			if (0 == strcmp(req->p[i].name, manroot))
				break;
		req->q.manroot = i < (int)req->psz ? i : -1;
	}
}

static void
http_putchar(char c)
{

	if (isalnum((unsigned char)c)) {
		putchar((unsigned char)c);
		return;
	} else if (' ' == c) {
		putchar('+');
		return;
	}
	printf("%%%.2x", c);
}

/*
 * HTTP-decode a string.  The standard explanation is that this turns
 * "%4e+foo" into "n foo" in the regular way.  This is done in-place
 * over the allocated string.
 */
static int
http_decode(char *p)
{
	char             hex[3];
	int              c;

	hex[2] = '\0';

	for ( ; '\0' != *p; p++) {
		if ('%' == *p) {
			if ('\0' == (hex[0] = *(p + 1)))
				return(0);
			if ('\0' == (hex[1] = *(p + 2)))
				return(0);
			if (1 != sscanf(hex, "%x", &c))
				return(0);
			if ('\0' == c)
				return(0);

			*p = (char)c;
			memmove(p + 1, p + 3, strlen(p + 3) + 1);
		} else
			*p = '+' == *p ? ' ' : *p;
	}

	*p = '\0';
	return(1);
}

static void
resp_begin_http(int code, const char *msg)
{

	if (200 != code)
		printf("Status: %d %s\n", code, msg);

	puts("Content-Type: text/html; charset=utf-8\n"
	     "Cache-Control: no-cache\n"
	     "Pragma: no-cache\n"
	     "");

	fflush(stdout);
}

static void
resp_begin_html(int code, const char *msg)
{

	resp_begin_http(code, msg);

	printf("<!DOCTYPE HTML PUBLIC "
	       " \"-//W3C//DTD HTML 4.01//EN\""
	       " \"http://www.w3.org/TR/html4/strict.dtd\">\n"
	       "<HTML>\n"
	       "<HEAD>\n"
	       "<META HTTP-EQUIV=\"Content-Type\""
	       " CONTENT=\"text/html; charset=utf-8\">\n"
	       "<LINK REL=\"stylesheet\" HREF=\"%s/man-cgi.css\""
	       " TYPE=\"text/css\" media=\"all\">\n"
	       "<LINK REL=\"stylesheet\" HREF=\"%s/man.css\""
	       " TYPE=\"text/css\" media=\"all\">\n"
	       "<TITLE>System Manpage Reference</TITLE>\n"
	       "</HEAD>\n"
	       "<BODY>\n"
	       "<!-- Begin page content. //-->\n", css, css);
}

static void
resp_end_html(void)
{

	puts("</BODY>\n"
	     "</HTML>");
}

static void
resp_searchform(const struct req *req)
{
	int		 i;

	puts("<!-- Begin search form. //-->");
	printf("<DIV ID=\"mancgi\">\n"
	       "<FORM ACTION=\"%s/search.html\" METHOD=\"get\">\n"
	       "<FIELDSET>\n"
	       "<LEGEND>Search Parameters</LEGEND>\n"
	       "<INPUT TYPE=\"submit\" "
	       " VALUE=\"Search\"> for manuals satisfying \n"
	       "<INPUT TYPE=\"text\" NAME=\"expr\" VALUE=\"",
	       progname);
	html_print(req->q.expr ? req->q.expr : "");
	printf("\">, section "
	       "<INPUT TYPE=\"text\""
	       " SIZE=\"4\" NAME=\"sec\" VALUE=\"");
	html_print(req->q.sec ? req->q.sec : "");
	printf("\">, arch "
	       "<INPUT TYPE=\"text\""
	       " SIZE=\"8\" NAME=\"arch\" VALUE=\"");
	html_print(req->q.arch ? req->q.arch : "");
	printf("\">");
	if (req->psz > 1) {
		puts(", <SELECT NAME=\"manpath\">");
		for (i = 0; i < (int)req->psz; i++) {
			printf("<OPTION %s VALUE=\"",
				(i == req->q.manroot) ||
				(0 == i && -1 == req->q.manroot) ?
				"SELECTED=\"selected\"" : "");
			html_print(req->p[i].name);
			printf("\">");
			html_print(req->p[i].name);
			puts("</OPTION>");
		}
		puts("</SELECT>");
	}
	puts(".\n"
	     "<INPUT TYPE=\"reset\" VALUE=\"Reset\">\n"
	     "</FIELDSET>\n"
	     "</FORM>\n"
	     "</DIV>");
	puts("<!-- End search form. //-->");
}

static void
resp_index(const struct req *req)
{

	resp_begin_html(200, NULL);
	resp_searchform(req);
	resp_end_html();
}

static void
resp_error400(void)
{

	resp_begin_html(400, "Query Malformed");
	printf("<H1>Malformed Query</H1>\n"
	       "<P>\n"
	       "The query your entered was malformed.\n"
	       "Try again from the\n"
	       "<A HREF=\"%s/index.html\">main page</A>.\n"
	       "</P>", progname);
	resp_end_html();
}

static void
resp_error404(const char *page)
{

	resp_begin_html(404, "Not Found");
	puts("<H1>Page Not Found</H1>\n"
	     "<P>\n"
	     "The page you're looking for, ");
	printf("<B>");
	html_print(page);
	printf("</B>,\n"
	       "could not be found.\n"
	       "Try searching from the\n"
	       "<A HREF=\"%s/index.html\">main page</A>.\n"
	       "</P>", progname);
	resp_end_html();
}

static void
resp_bad(void)
{
	resp_begin_html(500, "Internal Server Error");
	puts("<P>Generic badness happened.</P>");
	resp_end_html();
}

static void
resp_baddb(void)
{

	resp_begin_html(500, "Internal Server Error");
	puts("<P>Your database is broken.</P>");
	resp_end_html();
}

static void
resp_search(struct res *r, size_t sz, void *arg)
{
	size_t		 i, matched;
	const struct req *req;

	req = (const struct req *)arg;

	if (sz > 0)
		assert(req->q.manroot >= 0);

	for (matched = i = 0; i < sz; i++)
		if (r[i].matched)
			matched++;
	
	if (1 == matched) {
		for (i = 0; i < sz; i++)
			if (r[i].matched)
				break;
		/*
		 * If we have just one result, then jump there now
		 * without any delay.
		 */
		puts("Status: 303 See Other");
		printf("Location: http://%s%s/show/%d/%u/%u.html?",
				host, progname, req->q.manroot,
				r[i].volume, r[i].rec);
		http_printquery(req);
		puts("\n"
		     "Content-Type: text/html; charset=utf-8\n");
		return;
	}

	resp_begin_html(200, NULL);
	resp_searchform(req);

	puts("<DIV CLASS=\"results\">");

	if (0 == matched) {
		puts("<P>\n"
		     "No results found.\n"
		     "</P>\n"
		     "</DIV>");
		resp_end_html();
		return;
	}

	qsort(r, sz, sizeof(struct res), cmp);

	puts("<TABLE>");

	for (i = 0; i < sz; i++) {
		if ( ! r[i].matched)
			continue;
		printf("<TR>\n"
		       "<TD CLASS=\"title\">\n"
		       "<A HREF=\"%s/show/%d/%u/%u.html?", 
				progname, req->q.manroot,
				r[i].volume, r[i].rec);
		html_printquery(req);
		printf("\">");
		html_print(r[i].title);
		putchar('(');
		html_print(r[i].cat);
		if (r[i].arch && '\0' != *r[i].arch) {
			putchar('/');
			html_print(r[i].arch);
		}
		printf(")</A>\n"
		       "</TD>\n"
		       "<TD CLASS=\"desc\">");
		html_print(r[i].desc);
		puts("</TD>\n"
		     "</TR>");
	}

	puts("</TABLE>\n"
	     "</DIV>");
	resp_end_html();
}

/* ARGSUSED */
static void
pg_index(const struct req *req, char *path)
{

	resp_index(req);
}

static void
catman(const struct req *req, const char *file)
{
	FILE		*f;
	size_t		 len;
	int		 i;
	char		*p;
	int		 italic, bold;

	if (NULL == (f = fopen(file, "r"))) {
		resp_baddb();
		return;
	}

	resp_begin_html(200, NULL);
	resp_searchform(req);
	puts("<DIV CLASS=\"catman\">\n"
	     "<PRE>");

	while (NULL != (p = fgetln(f, &len))) {
		bold = italic = 0;
		for (i = 0; i < (int)len - 1; i++) {
			/* 
			 * This means that the catpage is out of state.
			 * Ignore it and keep going (although the
			 * catpage is bogus).
			 */

			if ('\b' == p[i] || '\n' == p[i])
				continue;

			/*
			 * Print a regular character.
			 * Close out any bold/italic scopes.
			 * If we're in back-space mode, make sure we'll
			 * have something to enter when we backspace.
			 */

			if ('\b' != p[i + 1]) {
				if (italic)
					printf("</I>");
				if (bold)
					printf("</B>");
				italic = bold = 0;
				html_putchar(p[i]);
				continue;
			} else if (i + 2 >= (int)len)
				continue;

			/* Italic mode. */

			if ('_' == p[i]) {
				if (bold)
					printf("</B>");
				if ( ! italic)
					printf("<I>");
				bold = 0;
				italic = 1;
				i += 2;
				html_putchar(p[i]);
				continue;
			}

			/* 
			 * Handle funny behaviour troff-isms.
			 * These grok'd from the original man2html.c.
			 */

			if (('+' == p[i] && 'o' == p[i + 2]) ||
					('o' == p[i] && '+' == p[i + 2]) ||
					('|' == p[i] && '=' == p[i + 2]) ||
					('=' == p[i] && '|' == p[i + 2]) ||
					('*' == p[i] && '=' == p[i + 2]) ||
					('=' == p[i] && '*' == p[i + 2]) ||
					('*' == p[i] && '|' == p[i + 2]) ||
					('|' == p[i] && '*' == p[i + 2]))  {
				if (italic)
					printf("</I>");
				if (bold)
					printf("</B>");
				italic = bold = 0;
				putchar('*');
				i += 2;
				continue;
			} else if (('|' == p[i] && '-' == p[i + 2]) ||
					('-' == p[i] && '|' == p[i + 1]) ||
					('+' == p[i] && '-' == p[i + 1]) ||
					('-' == p[i] && '+' == p[i + 1]) ||
					('+' == p[i] && '|' == p[i + 1]) ||
					('|' == p[i] && '+' == p[i + 1]))  {
				if (italic)
					printf("</I>");
				if (bold)
					printf("</B>");
				italic = bold = 0;
				putchar('+');
				i += 2;
				continue;
			}

			/* Bold mode. */
			
			if (italic)
				printf("</I>");
			if ( ! bold)
				printf("<B>");
			bold = 1;
			italic = 0;
			i += 2;
			html_putchar(p[i]);
		}

		/* 
		 * Clean up the last character.
		 * We can get to a newline; don't print that. 
		 */

		if (italic)
			printf("</I>");
		if (bold)
			printf("</B>");

		if (i == (int)len - 1 && '\n' != p[i])
			html_putchar(p[i]);

		putchar('\n');
	}

	puts("</PRE>\n"
	     "</DIV>\n"
	     "</BODY>\n"
	     "</HTML>");

	fclose(f);
}

static void
format(const struct req *req, const char *file)
{
	struct mparse	*mp;
	int		 fd;
	struct mdoc	*mdoc;
	struct man	*man;
	void		*vp;
	enum mandoclevel rc;
	char		 opts[PATH_MAX + 128];

	if (-1 == (fd = open(file, O_RDONLY, 0))) {
		resp_baddb();
		return;
	}

	mp = mparse_alloc(MPARSE_AUTO, MANDOCLEVEL_FATAL, NULL, NULL, NULL);
	rc = mparse_readfd(mp, fd, file);
	close(fd);

	if (rc >= MANDOCLEVEL_FATAL) {
		resp_baddb();
		return;
	}

	snprintf(opts, sizeof(opts), "fragment,"
			"man=%s/search.html?sec=%%S&expr=Nm~^%%N$,"
			/*"includes=/cgi-bin/man.cgi/usr/include/%%I"*/,
			progname);

	mparse_result(mp, &mdoc, &man);
	if (NULL == man && NULL == mdoc) {
		resp_baddb();
		mparse_free(mp);
		return;
	}

	resp_begin_html(200, NULL);
	resp_searchform(req);

	vp = html_alloc(opts);

	if (NULL != mdoc)
		html_mdoc(vp, mdoc);
	else
		html_man(vp, man);

	puts("</BODY>\n"
	     "</HTML>");

	html_free(vp);
	mparse_free(mp);
}

static void
pg_show(const struct req *req, char *path)
{
	struct manpaths	 ps;
	size_t		 sz;
	char		*sub;
	char		 file[PATH_MAX];
	const char	*cp;
	int		 rc, catm;
	unsigned int	 vol, rec, mr;
	DB		*idx;
	DBT		 key, val;

	idx = NULL;

	/* Parse out mroot, volume, and record from the path. */

	if (NULL == path || NULL == (sub = strchr(path, '/'))) {
		resp_error400();
		return;
	} 
	*sub++ = '\0';
	if ( ! atou(path, &mr)) {
		resp_error400();
		return;
	}
	path = sub;
	if (NULL == (sub = strchr(path, '/'))) {
		resp_error400();
		return;
	}
	*sub++ = '\0';
	if ( ! atou(path, &vol) || ! atou(sub, &rec)) {
		resp_error400();
		return;
	} else if (mr >= (unsigned int)req->psz) {
		resp_error400();
		return;
	}

	/*
	 * Begin by chdir()ing into the manroot.
	 * This way we can pick up the database files, which are
	 * relative to the manpath root.
	 */

	if (-1 == chdir(req->p[(int)mr].path)) {
		perror(req->p[(int)mr].path);
		resp_baddb();
		return;
	}

	memset(&ps, 0, sizeof(struct manpaths));
	manpath_manconf(&ps, "etc/catman.conf");

	if (vol >= (unsigned int)ps.sz) {
		resp_error400();
		goto out;
	}

	sz = strlcpy(file, ps.paths[vol], PATH_MAX);
	assert(sz < PATH_MAX);
	strlcat(file, "/", PATH_MAX);
	strlcat(file, MANDOC_IDX, PATH_MAX);

	/* Open the index recno(3) database. */

	idx = dbopen(file, O_RDONLY, 0, DB_RECNO, NULL);
	if (NULL == idx) {
		perror(file);
		resp_baddb();
		goto out;
	}

	key.data = &rec;
	key.size = 4;

	if (0 != (rc = (*idx->get)(idx, &key, &val, 0))) {
		rc < 0 ? resp_baddb() : resp_error400();
		goto out;
	} else if (0 == val.size) {
		resp_baddb();
		goto out;
	}

	cp = (char *)val.data;
	catm = 'c' == *cp++;

	if (NULL == memchr(cp, '\0', val.size - 1)) 
		resp_baddb();
	else {
 		file[(int)sz] = '\0';
 		strlcat(file, "/", PATH_MAX);
 		strlcat(file, cp, PATH_MAX);
		if (catm) 
			catman(req, file);
		else
			format(req, file);
	}
out:
	if (idx)
		(*idx->close)(idx);
	manpath_free(&ps);
}

static void
pg_search(const struct req *req, char *path)
{
	size_t		  tt, ressz;
	struct manpaths	  ps;
	int		  i, sz, rc;
	const char	 *ep, *start;
	struct res	*res;
	char		**cp;
	struct opts	  opt;
	struct expr	 *expr;

	if (req->q.manroot < 0 || 0 == req->psz) {
		resp_search(NULL, 0, (void *)req);
		return;
	}

	memset(&opt, 0, sizeof(struct opts));

	ep 	 = req->q.expr;
	opt.arch = req->q.arch;
	opt.cat  = req->q.sec;
	rc 	 = -1;
	sz 	 = 0;
	cp	 = NULL;
	ressz	 = 0;
	res	 = NULL;

	/*
	 * Begin by chdir()ing into the root of the manpath.
	 * This way we can pick up the database files, which are
	 * relative to the manpath root.
	 */

	assert(req->q.manroot < (int)req->psz);
	if (-1 == (chdir(req->p[req->q.manroot].path))) {
		perror(req->p[req->q.manroot].path);
		resp_search(NULL, 0, (void *)req);
		return;
	}

	memset(&ps, 0, sizeof(struct manpaths));
	manpath_manconf(&ps, "etc/catman.conf");

	/*
	 * Poor man's tokenisation: just break apart by spaces.
	 * Yes, this is half-ass.  But it works for now.
	 */

	while (ep && isspace((unsigned char)*ep))
		ep++;

	while (ep && '\0' != *ep) {
		cp = mandoc_realloc(cp, (sz + 1) * sizeof(char *));
		start = ep;
		while ('\0' != *ep && ! isspace((unsigned char)*ep))
			ep++;
		cp[sz] = mandoc_malloc((ep - start) + 1);
		memcpy(cp[sz], start, ep - start);
		cp[sz++][ep - start] = '\0';
		while (isspace((unsigned char)*ep))
			ep++;
	}

	/*
	 * Pump down into apropos backend.
	 * The resp_search() function is called with the results.
	 */

	expr = req->q.legacy ? 
		termcomp(sz, cp, &tt) : exprcomp(sz, cp, &tt);

	if (NULL != expr)
		rc = apropos_search
			(ps.sz, ps.paths, &opt, expr, tt, 
			 (void *)req, &ressz, &res, resp_search);

	/* ...unless errors occured. */

	if (0 == rc)
		resp_baddb();
	else if (-1 == rc)
		resp_search(NULL, 0, NULL);

	for (i = 0; i < sz; i++)
		free(cp[i]);

	free(cp);
	resfree(res, ressz);
	exprfree(expr);
	manpath_free(&ps);
}

int
main(void)
{
	int		 i;
	char		 buf[PATH_MAX];
	DIR		*cwd;
	struct req	 req;
	char		*p, *path, *subpath;

	/* Scan our run-time environment. */

	if (NULL == (cache = getenv("CACHE_DIR")))
		cache = "/cache/man.cgi";

	if (NULL == (progname = getenv("SCRIPT_NAME")))
		progname = "";

	if (NULL == (css = getenv("CSS_DIR")))
		css = "";

	if (NULL == (host = getenv("HTTP_HOST")))
		host = "localhost";

	/*
	 * First we change directory into the cache directory so that
	 * subsequent scanning for manpath directories is rooted
	 * relative to the same position.
	 */

	if (-1 == chdir(cache)) {
		perror(cache);
		resp_bad();
		return(EXIT_FAILURE);
	} else if (NULL == (cwd = opendir(cache))) {
		perror(cache);
		resp_bad();
		return(EXIT_FAILURE);
	} 

	memset(&req, 0, sizeof(struct req));

	strlcpy(buf, ".", PATH_MAX);
	pathgen(cwd, buf, &req);
	closedir(cwd);

	/* Next parse out the query string. */

	if (NULL != (p = getenv("QUERY_STRING")))
		http_parse(&req, p);

	/*
	 * Now juggle paths to extract information.
	 * We want to extract our filetype (the file suffix), the
	 * initial path component, then the trailing component(s).
	 * Start with leading subpath component. 
	 */

	subpath = path = NULL;
	req.page = PAGE__MAX;

	if (NULL == (path = getenv("PATH_INFO")) || '\0' == *path)
		req.page = PAGE_INDEX;

	if (NULL != path && '/' == *path && '\0' == *++path)
		req.page = PAGE_INDEX;

	/* Strip file suffix. */

	if (NULL != path && NULL != (p = strrchr(path, '.')))
		if (NULL != p && NULL == strchr(p, '/'))
			*p++ = '\0';

	/* Resolve subpath component. */

	if (NULL != path && NULL != (subpath = strchr(path, '/')))
		*subpath++ = '\0';

	/* Map path into one we recognise. */

	if (NULL != path && '\0' != *path)
		for (i = 0; i < (int)PAGE__MAX; i++) 
			if (0 == strcmp(pages[i], path)) {
				req.page = (enum page)i;
				break;
			}

	/* Route pages. */

	switch (req.page) {
	case (PAGE_INDEX):
		pg_index(&req, subpath);
		break;
	case (PAGE_SEARCH):
		pg_search(&req, subpath);
		break;
	case (PAGE_SHOW):
		pg_show(&req, subpath);
		break;
	default:
		resp_error404(path);
		break;
	}

	for (i = 0; i < (int)req.psz; i++) {
		free(req.p[i].path);
		free(req.p[i].name);
	}

	free(req.p);
	return(EXIT_SUCCESS);
}

static int
cmp(const void *p1, const void *p2)
{

	return(strcasecmp(((const struct res *)p1)->title,
				((const struct res *)p2)->title));
}

/*
 * Check to see if an "etc" path consists of a catman.conf file.  If it
 * does, that means that the path contains a tree created by catman(8)
 * and should be used for indexing.
 */
static int
pathstop(DIR *dir)
{
	struct dirent	*d;
#if defined(__sun)
	struct stat	 sb;
#endif

	while (NULL != (d = readdir(dir))) {
#if defined(__sun)
		stat(d->d_name, &sb);
		if (S_IFREG & sb.st_mode)
#else
		if (DT_REG == d->d_type)
#endif
			if (0 == strcmp(d->d_name, "catman.conf"))
				return(1);
  }

	return(0);
}

/*
 * Scan for indexable paths.
 * This adds all paths with "etc/catman.conf" to the buffer.
 */
static void
pathgen(DIR *dir, char *path, struct req *req)
{
	struct dirent	*d;
	char		*cp;
	DIR		*cd;
	int		 rc;
	size_t		 sz, ssz;
#if defined(__sun)
	struct stat	 sb;
#endif

	sz = strlcat(path, "/", PATH_MAX);
	if (sz >= PATH_MAX) {
		fprintf(stderr, "%s: Path too long", path);
		return;
	} 

	/* 
	 * First, scan for the "etc" directory.
	 * If it's found, then see if it should cause us to stop.  This
	 * happens when a catman.conf is found in the directory.
	 */

	rc = 0;
	while (0 == rc && NULL != (d = readdir(dir))) {
#if defined(__sun)
		stat(d->d_name, &sb);
		if (!(S_IFDIR & sb.st_mode)
#else
		if (DT_DIR != d->d_type
#endif
        || strcmp(d->d_name, "etc"))
			continue;

		path[(int)sz] = '\0';
		ssz = strlcat(path, d->d_name, PATH_MAX);

		if (ssz >= PATH_MAX) {
			fprintf(stderr, "%s: Path too long", path);
			return;
		} else if (NULL == (cd = opendir(path))) {
			perror(path);
			return;
		} 
		
		rc = pathstop(cd);
		closedir(cd);
	}

	if (rc > 0) {
		/* This also strips the trailing slash. */
		path[(int)--sz] = '\0';
		req->p = mandoc_realloc
			(req->p, 
			 (req->psz + 1) * sizeof(struct paths));
		/*
		 * Strip out the leading "./" unless we're just a ".",
		 * in which case use an empty string as our name.
		 */
		req->p[(int)req->psz].path = mandoc_strdup(path);
		req->p[(int)req->psz].name = 
			cp = mandoc_strdup(path + (1 == sz ? 1 : 2));
		req->psz++;
		/* 
		 * The name is just the path with all the slashes taken
		 * out of it.  Simple but effective. 
		 */
		for ( ; '\0' != *cp; cp++) 
			if ('/' == *cp)
				*cp = ' ';
		return;
	} 

	/*
	 * If no etc/catman.conf was found, recursively enter child
	 * directory and continue scanning.
	 */

	rewinddir(dir);
	while (NULL != (d = readdir(dir))) {
#if defined(__sun)
		stat(d->d_name, &sb);
		if (!(S_IFDIR & sb.st_mode)
#else
		if (DT_DIR != d->d_type
#endif
        || '.' == d->d_name[0])
			continue;

		path[(int)sz] = '\0';
		ssz = strlcat(path, d->d_name, PATH_MAX);

		if (ssz >= PATH_MAX) {
			fprintf(stderr, "%s: Path too long", path);
			return;
		} else if (NULL == (cd = opendir(path))) {
			perror(path);
			return;
		}

		pathgen(cd, path, req);
		closedir(cd);
	}
}
