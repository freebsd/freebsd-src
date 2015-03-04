/*	$Id: cgi.c,v 1.104 2015/02/10 08:05:30 schwarze Exp $ */
/*
 * Copyright (c) 2011, 2012 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2014 Ingo Schwarze <schwarze@usta.de>
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

#include <sys/types.h>
#include <sys/time.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mandoc.h"
#include "mandoc_aux.h"
#include "main.h"
#include "manpath.h"
#include "mansearch.h"
#include "cgi.h"

/*
 * A query as passed to the search function.
 */
struct	query {
	char		*manpath; /* desired manual directory */
	char		*arch; /* architecture */
	char		*sec; /* manual section */
	char		*query; /* unparsed query expression */
	int		 equal; /* match whole names, not substrings */
};

struct	req {
	struct query	  q;
	char		**p; /* array of available manpaths */
	size_t		  psz; /* number of available manpaths */
};

static	void		 catman(const struct req *, const char *);
static	void		 format(const struct req *, const char *);
static	void		 html_print(const char *);
static	void		 html_putchar(char);
static	int		 http_decode(char *);
static	void		 http_parse(struct req *, const char *);
static	void		 http_print(const char *);
static	void		 http_putchar(char);
static	void		 http_printquery(const struct req *, const char *);
static	void		 pathgen(struct req *);
static	void		 pg_error_badrequest(const char *);
static	void		 pg_error_internal(void);
static	void		 pg_index(const struct req *);
static	void		 pg_noresult(const struct req *, const char *);
static	void		 pg_search(const struct req *);
static	void		 pg_searchres(const struct req *,
				struct manpage *, size_t);
static	void		 pg_show(struct req *, const char *);
static	void		 resp_begin_html(int, const char *);
static	void		 resp_begin_http(int, const char *);
static	void		 resp_end_html(void);
static	void		 resp_searchform(const struct req *);
static	void		 resp_show(const struct req *, const char *);
static	void		 set_query_attr(char **, char **);
static	int		 validate_filename(const char *);
static	int		 validate_manpath(const struct req *, const char *);
static	int		 validate_urifrag(const char *);

static	const char	 *scriptname; /* CGI script name */

static	const int sec_prios[] = {1, 4, 5, 8, 6, 3, 7, 2, 9};
static	const char *const sec_numbers[] = {
    "0", "1", "2", "3", "3p", "4", "5", "6", "7", "8", "9"
};
static	const char *const sec_names[] = {
    "All Sections",
    "1 - General Commands",
    "2 - System Calls",
    "3 - Library Functions",
    "3p - Perl Library",
    "4 - Device Drivers",
    "5 - File Formats",
    "6 - Games",
    "7 - Miscellaneous Information",
    "8 - System Manager\'s Manual",
    "9 - Kernel Developer\'s Manual"
};
static	const int sec_MAX = sizeof(sec_names) / sizeof(char *);

static	const char *const arch_names[] = {
    "amd64",       "alpha",       "armish",      "armv7",
    "aviion",      "hppa",        "hppa64",      "i386",
    "ia64",        "landisk",     "loongson",    "luna88k",
    "macppc",      "mips64",      "octeon",      "sgi",
    "socppc",      "solbourne",   "sparc",       "sparc64",
    "vax",         "zaurus",
    "amiga",       "arc",         "arm32",       "atari",
    "beagle",      "cats",        "hp300",       "mac68k",
    "mvme68k",     "mvme88k",     "mvmeppc",     "palm",
    "pc532",       "pegasos",     "pmax",        "powerpc",
    "sun3",        "wgrisc",      "x68k"
};
static	const int arch_MAX = sizeof(arch_names) / sizeof(char *);

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
http_printquery(const struct req *req, const char *sep)
{

	if (NULL != req->q.query) {
		printf("query=");
		http_print(req->q.query);
	}
	if (0 == req->q.equal)
		printf("%sapropos=1", sep);
	if (NULL != req->q.sec) {
		printf("%ssec=", sep);
		http_print(req->q.sec);
	}
	if (NULL != req->q.arch) {
		printf("%sarch=", sep);
		http_print(req->q.arch);
	}
	if (strcmp(req->q.manpath, req->p[0])) {
		printf("%smanpath=", sep);
		http_print(req->q.manpath);
	}
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
 * Transfer the responsibility for the allocated string *val
 * to the query structure.
 */
static void
set_query_attr(char **attr, char **val)
{

	free(*attr);
	if (**val == '\0') {
		*attr = NULL;
		free(*val);
	} else
		*attr = *val;
	*val = NULL;
}

/*
 * Parse the QUERY_STRING for key-value pairs
 * and store the values into the query structure.
 */
static void
http_parse(struct req *req, const char *qs)
{
	char		*key, *val;
	size_t		 keysz, valsz;

	req->q.manpath	= NULL;
	req->q.arch	= NULL;
	req->q.sec	= NULL;
	req->q.query	= NULL;
	req->q.equal	= 1;

	key = val = NULL;
	while (*qs != '\0') {

		/* Parse one key. */

		keysz = strcspn(qs, "=;&");
		key = mandoc_strndup(qs, keysz);
		qs += keysz;
		if (*qs != '=')
			goto next;

		/* Parse one value. */

		valsz = strcspn(++qs, ";&");
		val = mandoc_strndup(qs, valsz);
		qs += valsz;

		/* Decode and catch encoding errors. */

		if ( ! (http_decode(key) && http_decode(val)))
			goto next;

		/* Handle key-value pairs. */

		if ( ! strcmp(key, "query"))
			set_query_attr(&req->q.query, &val);

		else if ( ! strcmp(key, "apropos"))
			req->q.equal = !strcmp(val, "0");

		else if ( ! strcmp(key, "manpath")) {
#ifdef COMPAT_OLDURI
			if ( ! strncmp(val, "OpenBSD ", 8)) {
				val[7] = '-';
				if ('C' == val[8])
					val[8] = 'c';
			}
#endif
			set_query_attr(&req->q.manpath, &val);
		}

		else if ( ! (strcmp(key, "sec")
#ifdef COMPAT_OLDURI
		    && strcmp(key, "sektion")
#endif
		    )) {
			if ( ! strcmp(val, "0"))
				*val = '\0';
			set_query_attr(&req->q.sec, &val);
		}

		else if ( ! strcmp(key, "arch")) {
			if ( ! strcmp(val, "default"))
				*val = '\0';
			set_query_attr(&req->q.arch, &val);
		}

		/*
		 * The key must be freed in any case.
		 * The val may have been handed over to the query
		 * structure, in which case it is now NULL.
		 */
next:
		free(key);
		key = NULL;
		free(val);
		val = NULL;

		if (*qs != '\0')
			qs++;
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
	char		*q;
	int              c;

	hex[2] = '\0';

	q = p;
	for ( ; '\0' != *p; p++, q++) {
		if ('%' == *p) {
			if ('\0' == (hex[0] = *(p + 1)))
				return(0);
			if ('\0' == (hex[1] = *(p + 2)))
				return(0);
			if (1 != sscanf(hex, "%x", &c))
				return(0);
			if ('\0' == c)
				return(0);

			*q = (char)c;
			p += 2;
		} else
			*q = '+' == *p ? ' ' : *p;
	}

	*q = '\0';
	return(1);
}

static void
resp_begin_http(int code, const char *msg)
{

	if (200 != code)
		printf("Status: %d %s\r\n", code, msg);

	printf("Content-Type: text/html; charset=utf-8\r\n"
	     "Cache-Control: no-cache\r\n"
	     "Pragma: no-cache\r\n"
	     "\r\n");

	fflush(stdout);
}

static void
resp_begin_html(int code, const char *msg)
{

	resp_begin_http(code, msg);

	printf("<!DOCTYPE html>\n"
	       "<HTML>\n"
	       "<HEAD>\n"
	       "<META CHARSET=\"UTF-8\" />\n"
	       "<LINK REL=\"stylesheet\" HREF=\"%s/man-cgi.css\""
	       " TYPE=\"text/css\" media=\"all\">\n"
	       "<LINK REL=\"stylesheet\" HREF=\"%s/man.css\""
	       " TYPE=\"text/css\" media=\"all\">\n"
	       "<TITLE>%s</TITLE>\n"
	       "</HEAD>\n"
	       "<BODY>\n"
	       "<!-- Begin page content. //-->\n",
	       CSS_DIR, CSS_DIR, CUSTOMIZE_TITLE);
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

	puts(CUSTOMIZE_BEGIN);
	puts("<!-- Begin search form. //-->");
	printf("<DIV ID=\"mancgi\">\n"
	       "<FORM ACTION=\"%s\" METHOD=\"get\">\n"
	       "<FIELDSET>\n"
	       "<LEGEND>Manual Page Search Parameters</LEGEND>\n",
	       scriptname);

	/* Write query input box. */

	printf(	"<TABLE><TR><TD>\n"
		"<INPUT TYPE=\"text\" NAME=\"query\" VALUE=\"");
	if (NULL != req->q.query)
		html_print(req->q.query);
	puts("\" SIZE=\"40\">");

	/* Write submission and reset buttons. */

	printf(	"<INPUT TYPE=\"submit\" VALUE=\"Submit\">\n"
		"<INPUT TYPE=\"reset\" VALUE=\"Reset\">\n");

	/* Write show radio button */

	printf(	"</TD><TD>\n"
		"<INPUT TYPE=\"radio\" ");
	if (req->q.equal)
		printf("CHECKED=\"checked\" ");
	printf(	"NAME=\"apropos\" ID=\"show\" VALUE=\"0\">\n"
		"<LABEL FOR=\"show\">Show named manual page</LABEL>\n");

	/* Write section selector. */

	puts(	"</TD></TR><TR><TD>\n"
		"<SELECT NAME=\"sec\">");
	for (i = 0; i < sec_MAX; i++) {
		printf("<OPTION VALUE=\"%s\"", sec_numbers[i]);
		if (NULL != req->q.sec &&
		    0 == strcmp(sec_numbers[i], req->q.sec))
			printf(" SELECTED=\"selected\"");
		printf(">%s</OPTION>\n", sec_names[i]);
	}
	puts("</SELECT>");

	/* Write architecture selector. */

	printf(	"<SELECT NAME=\"arch\">\n"
		"<OPTION VALUE=\"default\"");
	if (NULL == req->q.arch)
		printf(" SELECTED=\"selected\"");
	puts(">All Architectures</OPTION>");
	for (i = 0; i < arch_MAX; i++) {
		printf("<OPTION VALUE=\"%s\"", arch_names[i]);
		if (NULL != req->q.arch &&
		    0 == strcmp(arch_names[i], req->q.arch))
			printf(" SELECTED=\"selected\"");
		printf(">%s</OPTION>\n", arch_names[i]);
	}
	puts("</SELECT>");

	/* Write manpath selector. */

	if (req->psz > 1) {
		puts("<SELECT NAME=\"manpath\">");
		for (i = 0; i < (int)req->psz; i++) {
			printf("<OPTION ");
			if (strcmp(req->q.manpath, req->p[i]) == 0)
				printf("SELECTED=\"selected\" ");
			printf("VALUE=\"");
			html_print(req->p[i]);
			printf("\">");
			html_print(req->p[i]);
			puts("</OPTION>");
		}
		puts("</SELECT>");
	}

	/* Write search radio button */

	printf(	"</TD><TD>\n"
		"<INPUT TYPE=\"radio\" ");
	if (0 == req->q.equal)
		printf("CHECKED=\"checked\" ");
	printf(	"NAME=\"apropos\" ID=\"search\" VALUE=\"1\">\n"
		"<LABEL FOR=\"search\">Search with apropos query</LABEL>\n");

	puts("</TD></TR></TABLE>\n"
	     "</FIELDSET>\n"
	     "</FORM>\n"
	     "</DIV>");
	puts("<!-- End search form. //-->");
}

static int
validate_urifrag(const char *frag)
{

	while ('\0' != *frag) {
		if ( ! (isalnum((unsigned char)*frag) ||
		    '-' == *frag || '.' == *frag ||
		    '/' == *frag || '_' == *frag))
			return(0);
		frag++;
	}
	return(1);
}

static int
validate_manpath(const struct req *req, const char* manpath)
{
	size_t	 i;

	if ( ! strcmp(manpath, "mandoc"))
		return(1);

	for (i = 0; i < req->psz; i++)
		if ( ! strcmp(manpath, req->p[i]))
			return(1);

	return(0);
}

static int
validate_filename(const char *file)
{

	if ('.' == file[0] && '/' == file[1])
		file += 2;

	return ( ! (strstr(file, "../") || strstr(file, "/..") ||
	    (strncmp(file, "man", 3) && strncmp(file, "cat", 3))));
}

static void
pg_index(const struct req *req)
{

	resp_begin_html(200, NULL);
	resp_searchform(req);
	printf("<P>\n"
	       "This web interface is documented in the\n"
	       "<A HREF=\"%s/mandoc/man8/man.cgi.8\">man.cgi</A>\n"
	       "manual, and the\n"
	       "<A HREF=\"%s/mandoc/man1/apropos.1\">apropos</A>\n"
	       "manual explains the query syntax.\n"
	       "</P>\n",
	       scriptname, scriptname);
	resp_end_html();
}

static void
pg_noresult(const struct req *req, const char *msg)
{
	resp_begin_html(200, NULL);
	resp_searchform(req);
	puts("<P>");
	puts(msg);
	puts("</P>");
	resp_end_html();
}

static void
pg_error_badrequest(const char *msg)
{

	resp_begin_html(400, "Bad Request");
	puts("<H1>Bad Request</H1>\n"
	     "<P>\n");
	puts(msg);
	printf("Try again from the\n"
	       "<A HREF=\"%s\">main page</A>.\n"
	       "</P>", scriptname);
	resp_end_html();
}

static void
pg_error_internal(void)
{
	resp_begin_html(500, "Internal Server Error");
	puts("<P>Internal Server Error</P>");
	resp_end_html();
}

static void
pg_searchres(const struct req *req, struct manpage *r, size_t sz)
{
	char		*arch, *archend;
	size_t		 i, iuse, isec;
	int		 archprio, archpriouse;
	int		 prio, priouse;
	char		 sec;

	for (i = 0; i < sz; i++) {
		if (validate_filename(r[i].file))
			continue;
		fprintf(stderr, "invalid filename %s in %s database\n",
		    r[i].file, req->q.manpath);
		pg_error_internal();
		return;
	}

	if (1 == sz) {
		/*
		 * If we have just one result, then jump there now
		 * without any delay.
		 */
		printf("Status: 303 See Other\r\n");
		printf("Location: http://%s%s/%s/%s?",
		    HTTP_HOST, scriptname, req->q.manpath, r[0].file);
		http_printquery(req, "&");
		printf("\r\n"
		     "Content-Type: text/html; charset=utf-8\r\n"
		     "\r\n");
		return;
	}

	resp_begin_html(200, NULL);
	resp_searchform(req);
	puts("<DIV CLASS=\"results\">");
	puts("<TABLE>");

	for (i = 0; i < sz; i++) {
		printf("<TR>\n"
		       "<TD CLASS=\"title\">\n"
		       "<A HREF=\"%s/%s/%s?",
		    scriptname, req->q.manpath, r[i].file);
		http_printquery(req, "&amp;");
		printf("\">");
		html_print(r[i].names);
		printf("</A>\n"
		       "</TD>\n"
		       "<TD CLASS=\"desc\">");
		html_print(r[i].output);
		puts("</TD>\n"
		     "</TR>");
	}

	puts("</TABLE>\n"
	     "</DIV>");

	/*
	 * In man(1) mode, show one of the pages
	 * even if more than one is found.
	 */

	if (req->q.equal) {
		puts("<HR>");
		iuse = 0;
		priouse = 10;
		archpriouse = 3;
		for (i = 0; i < sz; i++) {
			isec = strcspn(r[i].file, "123456789");
			sec = r[i].file[isec];
			if ('\0' == sec)
				continue;
			prio = sec_prios[sec - '1'];
			if (NULL == req->q.arch) {
				archprio =
				    (NULL == (arch = strchr(
					r[i].file + isec, '/'))) ? 3 :
				    (NULL == (archend = strchr(
					arch + 1, '/'))) ? 0 :
				    strncmp(arch, "amd64/",
					archend - arch) ? 2 : 1;
				if (archprio < archpriouse) {
					archpriouse = archprio;
					priouse = prio;
					iuse = i;
					continue;
				}
				if (archprio > archpriouse)
					continue;
			}
			if (prio >= priouse)
				continue;
			priouse = prio;
			iuse = i;
		}
		resp_show(req, r[iuse].file);
	}

	resp_end_html();
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
		puts("<P>You specified an invalid manual file.</P>");
		return;
	}

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
	     "</DIV>");

	fclose(f);
}

static void
format(const struct req *req, const char *file)
{
	struct mparse	*mp;
	struct mchars	*mchars;
	struct mdoc	*mdoc;
	struct man	*man;
	void		*vp;
	char		*opts;
	int		 fd;
	int		 usepath;

	if (-1 == (fd = open(file, O_RDONLY, 0))) {
		puts("<P>You specified an invalid manual file.</P>");
		return;
	}

	mchars = mchars_alloc();
	mp = mparse_alloc(MPARSE_SO, MANDOCLEVEL_BADARG, NULL,
	    mchars, req->q.manpath);
	mparse_readfd(mp, fd, file);
	close(fd);

	usepath = strcmp(req->q.manpath, req->p[0]);
	mandoc_asprintf(&opts,
	    "fragment,man=%s?query=%%N&sec=%%S%s%s%s%s",
	    scriptname,
	    req->q.arch	? "&arch="       : "",
	    req->q.arch	? req->q.arch    : "",
	    usepath	? "&manpath="    : "",
	    usepath	? req->q.manpath : "");

	mparse_result(mp, &mdoc, &man, NULL);
	if (NULL == man && NULL == mdoc) {
		fprintf(stderr, "fatal mandoc error: %s/%s\n",
		    req->q.manpath, file);
		pg_error_internal();
		mparse_free(mp);
		mchars_free(mchars);
		return;
	}

	vp = html_alloc(mchars, opts);

	if (NULL != mdoc)
		html_mdoc(vp, mdoc);
	else
		html_man(vp, man);

	html_free(vp);
	mparse_free(mp);
	mchars_free(mchars);
	free(opts);
}

static void
resp_show(const struct req *req, const char *file)
{

	if ('.' == file[0] && '/' == file[1])
		file += 2;

	if ('c' == *file)
		catman(req, file);
	else
		format(req, file);
}

static void
pg_show(struct req *req, const char *fullpath)
{
	char		*manpath;
	const char	*file;

	if ((file = strchr(fullpath, '/')) == NULL) {
		pg_error_badrequest(
		    "You did not specify a page to show.");
		return;
	}
	manpath = mandoc_strndup(fullpath, file - fullpath);
	file++;

	if ( ! validate_manpath(req, manpath)) {
		pg_error_badrequest(
		    "You specified an invalid manpath.");
		free(manpath);
		return;
	}

	/*
	 * Begin by chdir()ing into the manpath.
	 * This way we can pick up the database files, which are
	 * relative to the manpath root.
	 */

	if (chdir(manpath) == -1) {
		fprintf(stderr, "chdir %s: %s\n",
		    manpath, strerror(errno));
		pg_error_internal();
		free(manpath);
		return;
	}

	if (strcmp(manpath, "mandoc")) {
		free(req->q.manpath);
		req->q.manpath = manpath;
	} else
		free(manpath);

	if ( ! validate_filename(file)) {
		pg_error_badrequest(
		    "You specified an invalid manual file.");
		return;
	}

	resp_begin_html(200, NULL);
	resp_searchform(req);
	resp_show(req, file);
	resp_end_html();
}

static void
pg_search(const struct req *req)
{
	struct mansearch	  search;
	struct manpaths		  paths;
	struct manpage		 *res;
	char			**argv;
	char			 *query, *rp, *wp;
	size_t			  ressz;
	int			  argc;

	/*
	 * Begin by chdir()ing into the root of the manpath.
	 * This way we can pick up the database files, which are
	 * relative to the manpath root.
	 */

	if (-1 == (chdir(req->q.manpath))) {
		fprintf(stderr, "chdir %s: %s\n",
		    req->q.manpath, strerror(errno));
		pg_error_internal();
		return;
	}

	search.arch = req->q.arch;
	search.sec = req->q.sec;
	search.outkey = "Nd";
	search.argmode = req->q.equal ? ARG_NAME : ARG_EXPR;
	search.firstmatch = 1;

	paths.sz = 1;
	paths.paths = mandoc_malloc(sizeof(char *));
	paths.paths[0] = mandoc_strdup(".");

	/*
	 * Break apart at spaces with backslash-escaping.
	 */

	argc = 0;
	argv = NULL;
	rp = query = mandoc_strdup(req->q.query);
	for (;;) {
		while (isspace((unsigned char)*rp))
			rp++;
		if (*rp == '\0')
			break;
		argv = mandoc_reallocarray(argv, argc + 1, sizeof(char *));
		argv[argc++] = wp = rp;
		for (;;) {
			if (isspace((unsigned char)*rp)) {
				*wp = '\0';
				rp++;
				break;
			}
			if (rp[0] == '\\' && rp[1] != '\0')
				rp++;
			if (wp != rp)
				*wp = *rp;
			if (*rp == '\0')
				break;
			wp++;
			rp++;
		}
	}

	if (0 == mansearch(&search, &paths, argc, argv, &res, &ressz))
		pg_noresult(req, "You entered an invalid query.");
	else if (0 == ressz)
		pg_noresult(req, "No results found.");
	else
		pg_searchres(req, res, ressz);

	free(query);
	mansearch_free(res, ressz);
	free(paths.paths[0]);
	free(paths.paths);
}

int
main(void)
{
	struct req	 req;
	struct itimerval itimer;
	const char	*path;
	const char	*querystring;
	int		 i;

	/* Poor man's ReDoS mitigation. */

	itimer.it_value.tv_sec = 2;
	itimer.it_value.tv_usec = 0;
	itimer.it_interval.tv_sec = 2;
	itimer.it_interval.tv_usec = 0;
	if (setitimer(ITIMER_VIRTUAL, &itimer, NULL) == -1) {
		fprintf(stderr, "setitimer: %s\n", strerror(errno));
		pg_error_internal();
		return(EXIT_FAILURE);
	}

	/* Scan our run-time environment. */

	if (NULL == (scriptname = getenv("SCRIPT_NAME")))
		scriptname = "";

	if ( ! validate_urifrag(scriptname)) {
		fprintf(stderr, "unsafe SCRIPT_NAME \"%s\"\n",
		    scriptname);
		pg_error_internal();
		return(EXIT_FAILURE);
	}

	/*
	 * First we change directory into the MAN_DIR so that
	 * subsequent scanning for manpath directories is rooted
	 * relative to the same position.
	 */

	if (-1 == chdir(MAN_DIR)) {
		fprintf(stderr, "MAN_DIR: %s: %s\n",
		    MAN_DIR, strerror(errno));
		pg_error_internal();
		return(EXIT_FAILURE);
	}

	memset(&req, 0, sizeof(struct req));
	pathgen(&req);

	/* Next parse out the query string. */

	if (NULL != (querystring = getenv("QUERY_STRING")))
		http_parse(&req, querystring);

	if (req.q.manpath == NULL)
		req.q.manpath = mandoc_strdup(req.p[0]);
	else if ( ! validate_manpath(&req, req.q.manpath)) {
		pg_error_badrequest(
		    "You specified an invalid manpath.");
		return(EXIT_FAILURE);
	}

	if ( ! (NULL == req.q.arch || validate_urifrag(req.q.arch))) {
		pg_error_badrequest(
		    "You specified an invalid architecture.");
		return(EXIT_FAILURE);
	}

	/* Dispatch to the three different pages. */

	path = getenv("PATH_INFO");
	if (NULL == path)
		path = "";
	else if ('/' == *path)
		path++;

	if ('\0' != *path)
		pg_show(&req, path);
	else if (NULL != req.q.query)
		pg_search(&req);
	else
		pg_index(&req);

	free(req.q.manpath);
	free(req.q.arch);
	free(req.q.sec);
	free(req.q.query);
	for (i = 0; i < (int)req.psz; i++)
		free(req.p[i]);
	free(req.p);
	return(EXIT_SUCCESS);
}

/*
 * Scan for indexable paths.
 */
static void
pathgen(struct req *req)
{
	FILE	*fp;
	char	*dp;
	size_t	 dpsz;

	if (NULL == (fp = fopen("manpath.conf", "r"))) {
		fprintf(stderr, "%s/manpath.conf: %s\n",
			MAN_DIR, strerror(errno));
		pg_error_internal();
		exit(EXIT_FAILURE);
	}

	while (NULL != (dp = fgetln(fp, &dpsz))) {
		if ('\n' == dp[dpsz - 1])
			dpsz--;
		req->p = mandoc_realloc(req->p,
		    (req->psz + 1) * sizeof(char *));
		dp = mandoc_strndup(dp, dpsz);
		if ( ! validate_urifrag(dp)) {
			fprintf(stderr, "%s/manpath.conf contains "
			    "unsafe path \"%s\"\n", MAN_DIR, dp);
			pg_error_internal();
			exit(EXIT_FAILURE);
		}
		if (NULL != strchr(dp, '/')) {
			fprintf(stderr, "%s/manpath.conf contains "
			    "path with slash \"%s\"\n", MAN_DIR, dp);
			pg_error_internal();
			exit(EXIT_FAILURE);
		}
		req->p[req->psz++] = dp;
	}

	if ( req->p == NULL ) {
		fprintf(stderr, "%s/manpath.conf is empty\n", MAN_DIR);
		pg_error_internal();
		exit(EXIT_FAILURE);
	}
}
