/* $Id: cgi.c,v 1.181 2023/04/28 19:11:03 schwarze Exp $ */
/*
 * Copyright (c) 2014-2019, 2021, 2022 Ingo Schwarze <schwarze@usta.de>
 * Copyright (c) 2011, 2012 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2022 Anna Vyalkova <cyber@sysrq.in>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Implementation of the man.cgi(8) program.
 */
#include "config.h"

#include <sys/types.h>
#include <sys/time.h>

#include <ctype.h>
#if HAVE_ERR
#include <err.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mandoc_aux.h"
#include "mandoc.h"
#include "roff.h"
#include "mdoc.h"
#include "man.h"
#include "mandoc_parse.h"
#include "main.h"
#include "manconf.h"
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
	int		  isquery; /* QUERY_STRING used, not PATH_INFO */
};

enum	focus {
	FOCUS_NONE = 0,
	FOCUS_QUERY
};

static	void		 html_print(const char *);
static	void		 html_putchar(char);
static	int		 http_decode(char *);
static	void		 http_encode(const char *);
static	void		 parse_manpath_conf(struct req *);
static	void		 parse_path_info(struct req *, const char *);
static	void		 parse_query_string(struct req *, const char *);
static	void		 pg_error_badrequest(const char *);
static	void		 pg_error_internal(void);
static	void		 pg_index(const struct req *);
static	void		 pg_noresult(const struct req *, int, const char *,
				const char *);
static	void		 pg_redirect(const struct req *, const char *);
static	void		 pg_search(const struct req *);
static	void		 pg_searchres(const struct req *,
				struct manpage *, size_t);
static	void		 pg_show(struct req *, const char *);
static	int		 resp_begin_html(int, const char *, const char *);
static	void		 resp_begin_http(int, const char *);
static	void		 resp_catman(const struct req *, const char *);
static	int		 resp_copy(const char *, const char *);
static	void		 resp_end_html(void);
static	void		 resp_format(const struct req *, const char *);
static	void		 resp_searchform(const struct req *, enum focus);
static	void		 resp_show(const struct req *, const char *);
static	void		 set_query_attr(char **, char **);
static	int		 validate_arch(const char *);
static	int		 validate_filename(const char *);
static	int		 validate_manpath(const struct req *, const char *);
static	int		 validate_urifrag(const char *);

static	const char	 *scriptname = SCRIPT_NAME;

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
    "amd64",       "alpha",       "armv7",       "arm64",
    "hppa",        "i386",        "landisk",     "loongson",
    "luna88k",     "macppc",      "mips64",      "octeon",
    "powerpc64",   "riscv64",     "sparc64",

    "amiga",       "arc",         "armish",      "arm32",
    "atari",       "aviion",      "beagle",      "cats",
    "hppa64",      "hp300",
    "ia64",        "mac68k",      "mvme68k",     "mvme88k",
    "mvmeppc",     "palm",        "pc532",       "pegasos",
    "pmax",        "powerpc",     "sgi",         "socppc",
    "solbourne",   "sparc",
    "sun3",        "vax",         "wgrisc",      "x68k",
    "zaurus"
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
	case '"':
		printf("&quot;");
		break;
	case '&':
		printf("&amp;");
		break;
	case '>':
		printf("&gt;");
		break;
	case '<':
		printf("&lt;");
		break;
	default:
		putchar((unsigned char)c);
		break;
	}
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
parse_query_string(struct req *req, const char *qs)
{
	char		*key, *val;
	size_t		 keysz, valsz;

	req->isquery	= 1;
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
				return 0;
			if ('\0' == (hex[1] = *(p + 2)))
				return 0;
			if (1 != sscanf(hex, "%x", &c))
				return 0;
			if ('\0' == c)
				return 0;

			*q = (char)c;
			p += 2;
		} else
			*q = '+' == *p ? ' ' : *p;
	}

	*q = '\0';
	return 1;
}

static void
http_encode(const char *p)
{
	for (; *p != '\0'; p++) {
		if (isalnum((unsigned char)*p) == 0 &&
		    strchr("-._~", *p) == NULL)
			printf("%%%2.2X", (unsigned char)*p);
		else
			putchar(*p);
	}
}

static void
resp_begin_http(int code, const char *msg)
{

	if (200 != code)
		printf("Status: %d %s\r\n", code, msg);

	printf("Content-Type: text/html; charset=utf-8\r\n"
	     "Cache-Control: no-cache\r\n"
	     "Content-Security-Policy: default-src 'none'; "
	     "style-src 'self' 'unsafe-inline'\r\n"
	     "Pragma: no-cache\r\n"
	     "\r\n");

	fflush(stdout);
}

static int
resp_copy(const char *element, const char *filename)
{
	char	 buf[4096];
	ssize_t	 sz;
	int	 fd;

	if ((fd = open(filename, O_RDONLY)) == -1)
		return 0;

	if (element != NULL)
		printf("<%s>\n", element);
	fflush(stdout);
	while ((sz = read(fd, buf, sizeof(buf))) > 0)
		write(STDOUT_FILENO, buf, sz);
	close(fd);
	return 1;
}

static int
resp_begin_html(int code, const char *msg, const char *file)
{
	const char	*name, *sec, *cp;
	int		 namesz, secsz;

	resp_begin_http(code, msg);

	printf("<!DOCTYPE html>\n"
	       "<html>\n"
	       "<head>\n"
	       "  <meta charset=\"UTF-8\"/>\n"
	       "  <meta name=\"viewport\""
		      " content=\"width=device-width, initial-scale=1.0\">\n"
	       "  <link rel=\"stylesheet\" href=\"%s/mandoc.css\""
	       " type=\"text/css\" media=\"all\">\n"
	       "  <title>",
	       CSS_DIR);
	if (file != NULL) {
		cp = strrchr(file, '/');
		name = cp == NULL ? file : cp + 1;
		cp = strrchr(name, '.');
		namesz = cp == NULL ? strlen(name) : cp - name;
		sec = NULL;
		if (cp != NULL && cp[1] != '0') {
			sec = cp + 1;
			secsz = strlen(sec);
		} else if (name - file > 1) {
			for (cp = name - 2; cp >= file; cp--) {
				if (*cp < '1' || *cp > '9')
					continue;
				sec = cp;
				secsz = name - cp - 1;
				break;
			}
		}
		printf("%.*s", namesz, name);
		if (sec != NULL)
			printf("(%.*s)", secsz, sec);
		fputs(" - ", stdout);
	}
	printf("%s</title>\n"
	       "</head>\n"
	       "<body>\n",
	       CUSTOMIZE_TITLE);

	return resp_copy("header", MAN_DIR "/header.html");
}

static void
resp_end_html(void)
{
	if (resp_copy("footer", MAN_DIR "/footer.html"))
		puts("</footer>");

	puts("</body>\n"
	     "</html>");
}

static void
resp_searchform(const struct req *req, enum focus focus)
{
	int		 i;

	printf("<form role=\"search\" action=\"/%s\" method=\"get\" "
	       "autocomplete=\"off\" autocapitalize=\"none\">\n"
	       "  <fieldset>\n"
	       "    <legend>Manual Page Search Parameters</legend>\n",
	       scriptname);

	/* Write query input box. */

	printf("    <label>Search query:\n"
	       "      <input type=\"search\" name=\"query\" value=\"");
	if (req->q.query != NULL)
		html_print(req->q.query);
	printf("\" size=\"40\"");
	if (focus == FOCUS_QUERY)
		printf(" autofocus");
	puts(">\n    </label>");

	/* Write submission buttons. */

	printf(	"    <button type=\"submit\" name=\"apropos\" value=\"0\">"
		"man</button>\n"
		"    <button type=\"submit\" name=\"apropos\" value=\"1\">"
		"apropos</button>\n"
		"    <br/>\n");

	/* Write section selector. */

	puts("    <select name=\"sec\" aria-label=\"Manual section\">");
	for (i = 0; i < sec_MAX; i++) {
		printf("      <option value=\"%s\"", sec_numbers[i]);
		if (NULL != req->q.sec &&
		    0 == strcmp(sec_numbers[i], req->q.sec))
			printf(" selected=\"selected\"");
		printf(">%s</option>\n", sec_names[i]);
	}
	puts("    </select>");

	/* Write architecture selector. */

	printf(	"    <select name=\"arch\" aria-label=\"CPU architecture\">\n"
		"      <option value=\"default\"");
	if (NULL == req->q.arch)
		printf(" selected=\"selected\"");
	puts(">All Architectures</option>");
	for (i = 0; i < arch_MAX; i++) {
		printf("      <option");
		if (NULL != req->q.arch &&
		    0 == strcmp(arch_names[i], req->q.arch))
			printf(" selected=\"selected\"");
		printf(">%s</option>\n", arch_names[i]);
	}
	puts("    </select>");

	/* Write manpath selector. */

	if (req->psz > 1) {
		puts("    <select name=\"manpath\""
		     " aria-label=\"Manual path\">");
		for (i = 0; i < (int)req->psz; i++) {
			printf("      <option");
			if (strcmp(req->q.manpath, req->p[i]) == 0)
				printf(" selected=\"selected\"");
			printf(">");
			html_print(req->p[i]);
			puts("</option>");
		}
		puts("    </select>");
	}

	puts("  </fieldset>\n"
	     "</form>");
}

static int
validate_urifrag(const char *frag)
{

	while ('\0' != *frag) {
		if ( ! (isalnum((unsigned char)*frag) ||
		    '-' == *frag || '.' == *frag ||
		    '/' == *frag || '_' == *frag))
			return 0;
		frag++;
	}
	return 1;
}

static int
validate_manpath(const struct req *req, const char* manpath)
{
	size_t	 i;

	for (i = 0; i < req->psz; i++)
		if ( ! strcmp(manpath, req->p[i]))
			return 1;

	return 0;
}

static int
validate_arch(const char *arch)
{
	int	 i;

	for (i = 0; i < arch_MAX; i++)
		if (strcmp(arch, arch_names[i]) == 0)
			return 1;

	return 0;
}

static int
validate_filename(const char *file)
{

	if ('.' == file[0] && '/' == file[1])
		file += 2;

	return ! (strstr(file, "../") || strstr(file, "/..") ||
	    (strncmp(file, "man", 3) && strncmp(file, "cat", 3)));
}

static void
pg_index(const struct req *req)
{
	if (resp_begin_html(200, NULL, NULL) == 0)
		puts("<header>");
	resp_searchform(req, FOCUS_QUERY);
	printf("</header>\n"
	       "<main>\n"
	       "<p role=\"doc-notice\" aria-label=\"Usage\">\n"
	       "This web interface is documented in the\n"
	       "<a class=\"Xr\" href=\"/%s%sman.cgi.8\""
	       " aria-label=\"man dot CGI, section 8\">man.cgi(8)</a>\n"
	       "manual, and the\n"
	       "<a class=\"Xr\" href=\"/%s%sapropos.1\""
	       " aria-label=\"apropos, section 1\">apropos(1)</a>\n"
	       "manual explains the query syntax.\n"
	       "</p>\n"
	       "</main>\n",
	       scriptname, *scriptname == '\0' ? "" : "/",
	       scriptname, *scriptname == '\0' ? "" : "/");
	resp_end_html();
}

static void
pg_noresult(const struct req *req, int code, const char *http_msg,
    const char *user_msg)
{
	if (resp_begin_html(code, http_msg, NULL) == 0)
		puts("<header>");
	resp_searchform(req, FOCUS_QUERY);
	puts("</header>");
	puts("<main>");
	puts("<p role=\"doc-notice\" aria-label=\"No result\">");
	puts(user_msg);
	puts("</p>");
	puts("</main>");
	resp_end_html();
}

static void
pg_error_badrequest(const char *msg)
{
	if (resp_begin_html(400, "Bad Request", NULL))
		puts("</header>");
	puts("<main>\n"
	     "<h1>Bad Request</h1>\n"
	     "<p role=\"doc-notice\" aria-label=\"Bad Request\">");
	puts(msg);
	printf("Try again from the\n"
	       "<a href=\"/%s\">main page</a>.\n"
	       "</p>\n"
	       "</main>\n", scriptname);
	resp_end_html();
}

static void
pg_error_internal(void)
{
	if (resp_begin_html(500, "Internal Server Error", NULL))
		puts("</header>");
	puts("<main><p role=\"doc-notice\">Internal Server Error</p></main>");
	resp_end_html();
}

static void
pg_redirect(const struct req *req, const char *name)
{
	printf("Status: 303 See Other\r\n"
	    "Location: /");
	if (*scriptname != '\0')
		printf("%s/", scriptname);
	if (strcmp(req->q.manpath, req->p[0]))
		printf("%s/", req->q.manpath);
	if (req->q.arch != NULL)
		printf("%s/", req->q.arch);
	http_encode(name);
	if (req->q.sec != NULL) {
		putchar('.');
		http_encode(req->q.sec);
	}
	printf("\r\nContent-Type: text/html; charset=utf-8\r\n\r\n");
}

static void
pg_searchres(const struct req *req, struct manpage *r, size_t sz)
{
	char		*arch, *archend;
	const char	*sec;
	size_t		 i, iuse;
	int		 archprio, archpriouse;
	int		 prio, priouse;
	int		 have_header;

	for (i = 0; i < sz; i++) {
		if (validate_filename(r[i].file))
			continue;
		warnx("invalid filename %s in %s database",
		    r[i].file, req->q.manpath);
		pg_error_internal();
		return;
	}

	if (req->isquery && sz == 1) {
		/*
		 * If we have just one result, then jump there now
		 * without any delay.
		 */
		printf("Status: 303 See Other\r\n"
		    "Location: /");
		if (*scriptname != '\0')
			printf("%s/", scriptname);
		if (strcmp(req->q.manpath, req->p[0]))
			printf("%s/", req->q.manpath);
		printf("%s\r\n"
		    "Content-Type: text/html; charset=utf-8\r\n\r\n",
		    r[0].file);
		return;
	}

	/*
	 * In man(1) mode, show one of the pages
	 * even if more than one is found.
	 */

	iuse = 0;
	if (req->q.equal || sz == 1) {
		priouse = 20;
		archpriouse = 3;
		for (i = 0; i < sz; i++) {
			sec = r[i].file;
			sec += strcspn(sec, "123456789");
			if (sec[0] == '\0')
				continue;
			prio = sec_prios[sec[0] - '1'];
			if (sec[1] != '/')
				prio += 10;
			if (req->q.arch == NULL) {
				archprio =
				    ((arch = strchr(sec + 1, '/'))
					== NULL) ? 3 :
				    ((archend = strchr(arch + 1, '/'))
					== NULL) ? 0 :
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
		have_header = resp_begin_html(200, NULL, r[iuse].file);
	} else
		have_header = resp_begin_html(200, NULL, NULL);

	if (have_header == 0)
		puts("<header>");
	resp_searchform(req,
	    req->q.equal || sz == 1 ? FOCUS_NONE : FOCUS_QUERY);
	puts("</header>");

	if (sz > 1) {
		puts("<nav>");
		puts("<table class=\"results\">");
		for (i = 0; i < sz; i++) {
			printf("  <tr>\n"
			       "    <td>"
			       "<a class=\"Xr\" href=\"/");
			if (*scriptname != '\0')
				printf("%s/", scriptname);
			if (strcmp(req->q.manpath, req->p[0]))
				printf("%s/", req->q.manpath);
			printf("%s\">", r[i].file);
			html_print(r[i].names);
			printf("</a></td>\n"
			       "    <td><span class=\"Nd\">");
			html_print(r[i].output);
			puts("</span></td>\n"
			     "  </tr>");
		}
		puts("</table>");
		puts("</nav>");
	}

	if (req->q.equal || sz == 1) {
		puts("<hr>");
		resp_show(req, r[iuse].file);
	}

	resp_end_html();
}

static void
resp_catman(const struct req *req, const char *file)
{
	FILE		*f;
	char		*p;
	size_t		 sz;
	ssize_t		 len;
	int		 i;
	int		 italic, bold;

	if ((f = fopen(file, "r")) == NULL) {
		puts("<p role=\"doc-notice\">\n"
		     "  You specified an invalid manual file.\n"
		     "</p>");
		return;
	}

	puts("<div class=\"catman\">\n"
	     "<pre>");

	p = NULL;
	sz = 0;

	while ((len = getline(&p, &sz, f)) != -1) {
		bold = italic = 0;
		for (i = 0; i < len - 1; i++) {
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
					printf("</i>");
				if (bold)
					printf("</b>");
				italic = bold = 0;
				html_putchar(p[i]);
				continue;
			} else if (i + 2 >= len)
				continue;

			/* Italic mode. */

			if ('_' == p[i]) {
				if (bold)
					printf("</b>");
				if ( ! italic)
					printf("<i>");
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
					printf("</i>");
				if (bold)
					printf("</b>");
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
					printf("</i>");
				if (bold)
					printf("</b>");
				italic = bold = 0;
				putchar('+');
				i += 2;
				continue;
			}

			/* Bold mode. */

			if (italic)
				printf("</i>");
			if ( ! bold)
				printf("<b>");
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
			printf("</i>");
		if (bold)
			printf("</b>");

		if (i == len - 1 && p[i] != '\n')
			html_putchar(p[i]);

		putchar('\n');
	}
	free(p);

	puts("</pre>\n"
	     "</div>");

	fclose(f);
}

static void
resp_format(const struct req *req, const char *file)
{
	struct manoutput conf;
	struct mparse	*mp;
	struct roff_meta *meta;
	void		*vp;
	int		 fd;
	int		 usepath;

	if (-1 == (fd = open(file, O_RDONLY))) {
		puts("<p role=\"doc-notice\">\n"
		     "  You specified an invalid manual file.\n"
		     "</p>");
		return;
	}

	mchars_alloc();
	mp = mparse_alloc(MPARSE_SO | MPARSE_UTF8 | MPARSE_LATIN1 |
	    MPARSE_VALIDATE, MANDOC_OS_OTHER, req->q.manpath);
	mparse_readfd(mp, fd, file);
	close(fd);
	meta = mparse_result(mp);

	memset(&conf, 0, sizeof(conf));
	conf.fragment = 1;
	conf.style = mandoc_strdup(CSS_DIR "/mandoc.css");
	usepath = strcmp(req->q.manpath, req->p[0]);
	mandoc_asprintf(&conf.man, "/%s%s%s%s%%N.%%S",
	    scriptname, *scriptname == '\0' ? "" : "/",
	    usepath ? req->q.manpath : "", usepath ? "/" : "");

	vp = html_alloc(&conf);
	if (meta->macroset == MACROSET_MDOC)
		html_mdoc(vp, meta);
	else
		html_man(vp, meta);

	html_free(vp);
	mparse_free(mp);
	mchars_free();
	free(conf.man);
	free(conf.style);
}

static void
resp_show(const struct req *req, const char *file)
{

	if ('.' == file[0] && '/' == file[1])
		file += 2;

	if ('c' == *file)
		resp_catman(req, file);
	else
		resp_format(req, file);
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
		warn("chdir %s", manpath);
		pg_error_internal();
		free(manpath);
		return;
	}
	free(manpath);

	if ( ! validate_filename(file)) {
		pg_error_badrequest(
		    "You specified an invalid manual file.");
		return;
	}

	if (resp_begin_html(200, NULL, file) == 0)
		puts("<header>");
	resp_searchform(req, FOCUS_NONE);
	puts("</header>");
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

	if (chdir(req->q.manpath) == -1) {
		warn("chdir %s", req->q.manpath);
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

	res = NULL;
	ressz = 0;
	if (req->isquery && req->q.equal && argc == 1)
		pg_redirect(req, argv[0]);
	else if (mansearch(&search, &paths, argc, argv, &res, &ressz) == 0)
		pg_noresult(req, 400, "Bad Request",
		    "You entered an invalid query.");
	else if (ressz == 0)
		pg_noresult(req, 404, "Not Found", "No results found.");
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

#if HAVE_PLEDGE
	/*
	 * The "rpath" pledge could be revoked after mparse_readfd()
	 * if the file descriptor to "/footer.html" would be opened
	 * up front, but it's probably not worth the complication
	 * of the code it would cause: it would require scattering
	 * pledge() calls in multiple low-level resp_*() functions.
	 */

	if (pledge("stdio rpath", NULL) == -1) {
		warn("pledge");
		pg_error_internal();
		return EXIT_FAILURE;
	}
#endif

	/* Poor man's ReDoS mitigation. */

	itimer.it_value.tv_sec = 2;
	itimer.it_value.tv_usec = 0;
	itimer.it_interval.tv_sec = 2;
	itimer.it_interval.tv_usec = 0;
	if (setitimer(ITIMER_VIRTUAL, &itimer, NULL) == -1) {
		warn("setitimer");
		pg_error_internal();
		return EXIT_FAILURE;
	}

	/*
	 * First we change directory into the MAN_DIR so that
	 * subsequent scanning for manpath directories is rooted
	 * relative to the same position.
	 */

	if (chdir(MAN_DIR) == -1) {
		warn("MAN_DIR: %s", MAN_DIR);
		pg_error_internal();
		return EXIT_FAILURE;
	}

	memset(&req, 0, sizeof(struct req));
	req.q.equal = 1;
	parse_manpath_conf(&req);

	/* Parse the path info and the query string. */

	if ((path = getenv("PATH_INFO")) == NULL)
		path = "";
	else if (*path == '/')
		path++;

	if (*path != '\0') {
		parse_path_info(&req, path);
		if (req.q.manpath == NULL || req.q.sec == NULL ||
		    *req.q.query == '\0' || access(path, F_OK) == -1)
			path = "";
	} else if ((querystring = getenv("QUERY_STRING")) != NULL)
		parse_query_string(&req, querystring);

	/* Validate parsed data and add defaults. */

	if (req.q.manpath == NULL)
		req.q.manpath = mandoc_strdup(req.p[0]);
	else if ( ! validate_manpath(&req, req.q.manpath)) {
		pg_error_badrequest(
		    "You specified an invalid manpath.");
		return EXIT_FAILURE;
	}

	if (req.q.arch != NULL && validate_arch(req.q.arch) == 0) {
		pg_error_badrequest(
		    "You specified an invalid architecture.");
		return EXIT_FAILURE;
	}

	/* Dispatch to the three different pages. */

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
	return EXIT_SUCCESS;
}

/*
 * Translate PATH_INFO to a query.
 */
static void
parse_path_info(struct req *req, const char *path)
{
	const char	*name, *sec, *end;

	req->isquery = 0;
	req->q.equal = 1;
	req->q.manpath = NULL;
	req->q.arch = NULL;

	/* Mandatory manual page name. */
	if ((name = strrchr(path, '/')) == NULL)
		name = path;
	else
		name++;

	/* Optional trailing section. */
	sec = strrchr(name, '.');
	if (sec != NULL && isdigit((unsigned char)*++sec)) {
		req->q.query = mandoc_strndup(name, sec - name - 1);
		req->q.sec = mandoc_strdup(sec);
	} else {
		req->q.query = mandoc_strdup(name);
		req->q.sec = NULL;
	}

	/* Handle the case of name[.section] only. */
	if (name == path)
		return;

	/* Optional manpath. */
	end = strchr(path, '/');
	req->q.manpath = mandoc_strndup(path, end - path);
	if (validate_manpath(req, req->q.manpath)) {
		path = end + 1;
		if (name == path)
			return;
	} else {
		free(req->q.manpath);
		req->q.manpath = NULL;
	}

	/* Optional section. */
	if (strncmp(path, "man", 3) == 0 || strncmp(path, "cat", 3) == 0) {
		path += 3;
		end = strchr(path, '/');
		free(req->q.sec);
		req->q.sec = mandoc_strndup(path, end - path);
		path = end + 1;
		if (name == path)
			return;
	}

	/* Optional architecture. */
	end = strchr(path, '/');
	if (end + 1 != name) {
		pg_error_badrequest(
		    "You specified too many directory components.");
		exit(EXIT_FAILURE);
	}
	req->q.arch = mandoc_strndup(path, end - path);
	if (validate_arch(req->q.arch) == 0) {
		pg_error_badrequest(
		    "You specified an invalid directory component.");
		exit(EXIT_FAILURE);
	}
}

/*
 * Scan for indexable paths.
 */
static void
parse_manpath_conf(struct req *req)
{
	FILE	*fp;
	char	*dp;
	size_t	 dpsz;
	ssize_t	 len;

	if ((fp = fopen("manpath.conf", "r")) == NULL) {
		warn("%s/manpath.conf", MAN_DIR);
		pg_error_internal();
		exit(EXIT_FAILURE);
	}

	dp = NULL;
	dpsz = 0;

	while ((len = getline(&dp, &dpsz, fp)) != -1) {
		if (dp[len - 1] == '\n')
			dp[--len] = '\0';
		req->p = mandoc_realloc(req->p,
		    (req->psz + 1) * sizeof(char *));
		if ( ! validate_urifrag(dp)) {
			warnx("%s/manpath.conf contains "
			    "unsafe path \"%s\"", MAN_DIR, dp);
			pg_error_internal();
			exit(EXIT_FAILURE);
		}
		if (strchr(dp, '/') != NULL) {
			warnx("%s/manpath.conf contains "
			    "path with slash \"%s\"", MAN_DIR, dp);
			pg_error_internal();
			exit(EXIT_FAILURE);
		}
		req->p[req->psz++] = dp;
		dp = NULL;
		dpsz = 0;
	}
	free(dp);

	if (req->p == NULL) {
		warnx("%s/manpath.conf is empty", MAN_DIR);
		pg_error_internal();
		exit(EXIT_FAILURE);
	}
}
