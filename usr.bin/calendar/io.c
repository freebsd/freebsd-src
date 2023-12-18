/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libutil.h>
#include <locale.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stringlist.h>
#include <time.h>
#include <unistd.h>

#include "pathnames.h"
#include "calendar.h"

enum {
	T_OK = 0,
	T_ERR,
	T_PROCESS,
};

const char *calendarFile = "calendar";	/* default calendar file */
static const char *calendarHomes[] = {".calendar", _PATH_INCLUDE_LOCAL, _PATH_INCLUDE}; /* HOME */
static const char *calendarNoMail = "nomail";/* don't sent mail if file exist */

static char path[MAXPATHLEN];
static const char *cal_home;
static const char *cal_dir;
static const char *cal_file;
static int cal_line;

struct fixs neaster, npaskha, ncny, nfullmoon, nnewmoon;
struct fixs nmarequinox, nsepequinox, njunsolstice, ndecsolstice;

static int cal_parse(FILE *in, FILE *out);

static StringList *definitions = NULL;
static struct event *events[MAXCOUNT];
static char *extradata[MAXCOUNT];

static char *
trimlr(char **buf)
{
	char *walk = *buf;
	char *sep;
	char *last;

	while (isspace(*walk))
		walk++;
	*buf = walk;

	sep = walk;
	while (*sep != '\0' && !isspace(*sep))
		sep++;

	if (*sep != '\0') {
		last = sep + strlen(sep) - 1;
		while (last > walk && isspace(*last))
			last--;
		*(last+1) = 0;
	}

	return (sep);
}

static FILE *
cal_fopen(const char *file)
{
	static int cwdfd = -1;
	FILE *fp;
	char *home = getenv("HOME");
	unsigned int i;
	int fd;
	struct stat sb;
	static bool warned = false;
	static char calendarhome[MAXPATHLEN];

	if (home == NULL || *home == '\0') {
		warnx("Cannot get home directory");
		return (NULL);
	}

	/*
	 * On -a runs, we would have done a chdir() earlier on, but we also
	 * shouldn't have used the initial cwd anyways lest we bring
	 * unpredictable behavior upon us.
	 */
	if (!doall && cwdfd == -1) {
		cwdfd = open(".", O_DIRECTORY | O_PATH);
		if (cwdfd == -1)
			err(1, "open(cwd)");
	}

	/*
	 * Check $PWD first as documented.
	 */
	if (cwdfd != -1) {
		if ((fd = openat(cwdfd, file, O_RDONLY)) != -1) {
			if ((fp = fdopen(fd, "r")) == NULL)
				err(1, "fdopen(%s)", file);

			cal_home = NULL;
			cal_dir = NULL;
			cal_file = file;
			return (fp);
		} else if (errno != ENOENT && errno != ENAMETOOLONG) {
			err(1, "open(%s)", file);
		}
	}

	if (chdir(home) != 0) {
		warnx("Cannot enter home directory \"%s\"", home);
		return (NULL);
	}

	for (i = 0; i < nitems(calendarHomes); i++) {
		if (snprintf(calendarhome, sizeof (calendarhome), calendarHomes[i],
			getlocalbase()) >= (int)sizeof (calendarhome))
			continue;

		if (chdir(calendarhome) != 0)
			continue;

		if ((fp = fopen(file, "r")) != NULL) {
			cal_home = home;
			cal_dir = calendarhome;
			cal_file = file;
			return (fp);
		}
	}

	warnx("can't open calendar file \"%s\"", file);
	if (!warned) {
		snprintf(path, sizeof(path), _PATH_INCLUDE_LOCAL, getlocalbase());
		if (stat(path, &sb) != 0) {
			warnx("calendar data files now provided by calendar-data pkg.");
			warned = true;
		}
	}

	return (NULL);
}

static char*
cal_path(void)
{
	static char buffer[MAXPATHLEN + 10];

	if (cal_dir == NULL)
		snprintf(buffer, sizeof(buffer), "%s", cal_file);
	else if (cal_dir[0] == '/')
		snprintf(buffer, sizeof(buffer), "%s/%s", cal_dir, cal_file);
	else
		snprintf(buffer, sizeof(buffer), "%s/%s/%s", cal_home, cal_dir, cal_file);
	return (buffer);
}

#define	WARN0(format)		   \
	warnx(format " in %s line %d", cal_path(), cal_line)
#define	WARN1(format, arg1)		   \
	warnx(format " in %s line %d", arg1, cal_path(), cal_line)

static char*
cmptoken(char *line, const char* token)
{
	char len = strlen(token);

	if (strncmp(line, token, len) != 0)
		return NULL;
	return (line + len);
}

static int
token(char *line, FILE *out, int *skip, int *unskip)
{
	char *walk, *sep, a, c;
	const char *this_cal_home;
	const char *this_cal_dir;
	const char *this_cal_file;
	int this_cal_line;

	while (isspace(*line))
		line++;

	if (cmptoken(line, "endif")) {
		if (*skip + *unskip == 0) {
			WARN0("#endif without prior #ifdef or #ifndef");
			return (T_ERR);
		}
		if (*skip > 0)
			--*skip;
		else
			--*unskip;

		return (T_OK);
	}

	walk = cmptoken(line, "ifdef");
	if (walk != NULL) {
		sep = trimlr(&walk);

		if (*walk == '\0') {
			WARN0("Expecting arguments after #ifdef");
			return (T_ERR);
		}
		if (*sep != '\0') {
			WARN1("Expecting a single word after #ifdef "
			    "but got \"%s\"", walk);
			return (T_ERR);
		}

		if (*skip != 0 ||
		    definitions == NULL || sl_find(definitions, walk) == NULL)
			++*skip;
		else
			++*unskip;
		
		return (T_OK);
	}

	walk = cmptoken(line, "ifndef");
	if (walk != NULL) {
		sep = trimlr(&walk);

		if (*walk == '\0') {
			WARN0("Expecting arguments after #ifndef");
			return (T_ERR);
		}
		if (*sep != '\0') {
			WARN1("Expecting a single word after #ifndef "
			    "but got \"%s\"", walk);
			return (T_ERR);
		}

		if (*skip != 0 ||
		    (definitions != NULL && sl_find(definitions, walk) != NULL))
			++*skip;
		else
			++*unskip;

		return (T_OK);
	}

	walk = cmptoken(line, "else");
	if (walk != NULL) {
		(void)trimlr(&walk);

		if (*walk != '\0') {
			WARN0("Expecting no arguments after #else");
			return (T_ERR);
		}
		if (*skip + *unskip == 0) {
			WARN0("#else without prior #ifdef or #ifndef");
			return (T_ERR);
		}

		if (*skip == 0) {
			++*skip;
			--*unskip;
		} else if (*skip == 1) {
			--*skip;
			++*unskip;
		}

		return (T_OK);
	}

	if (*skip != 0)
		return (T_OK);

	walk = cmptoken(line, "include");
	if (walk != NULL) {
		(void)trimlr(&walk);

		if (*walk == '\0') {
			WARN0("Expecting arguments after #include");
			return (T_ERR);
		}

		if (*walk != '<' && *walk != '\"') {
			WARN0("Excecting '<' or '\"' after #include");
			return (T_ERR);
		}

		a = *walk == '<' ? '>' : '\"';
		walk++;
		c = walk[strlen(walk) - 1];

		if (a != c) {
			WARN1("Unterminated include expecting '%c'", a);
			return (T_ERR);
		}
		walk[strlen(walk) - 1] = '\0';

		this_cal_home = cal_home;
		this_cal_dir = cal_dir;
		this_cal_file = cal_file;
		this_cal_line = cal_line;
		if (cal_parse(cal_fopen(walk), out))
			return (T_ERR);
		cal_home = this_cal_home;
		cal_dir = this_cal_dir;
		cal_file = this_cal_file;
		cal_line = this_cal_line;

		return (T_OK);
	}

	walk = cmptoken(line, "define");
	if (walk != NULL) {
		if (definitions == NULL)
			definitions = sl_init();
		sep = trimlr(&walk);
		*sep = '\0';

		if (*walk == '\0') {
			WARN0("Expecting arguments after #define");
			return (T_ERR);
		}

		if (sl_find(definitions, walk) == NULL)
			sl_add(definitions, strdup(walk));
		return (T_OK);
	}

	walk = cmptoken(line, "undef");
	if (walk != NULL) {
		if (definitions != NULL) {
			sep = trimlr(&walk);

			if (*walk == '\0') {
				WARN0("Expecting arguments after #undef");
				return (T_ERR);
			}
			if (*sep != '\0') {
				WARN1("Expecting a single word after #undef "
				    "but got \"%s\"", walk);
				return (T_ERR);
			}

			walk = sl_find(definitions, walk);
			if (walk != NULL)
				walk[0] = '\0';
		}
		return (T_OK);
	}

	walk = cmptoken(line, "warning");
	if (walk != NULL) {
		(void)trimlr(&walk);
		WARN1("Warning: %s", walk);
	}

	walk = cmptoken(line, "error");
	if (walk != NULL) {
		(void)trimlr(&walk);
		WARN1("Error: %s", walk);
		return (T_ERR);
	}

	WARN1("Undefined pre-processor command \"#%s\"", line);
	return (T_ERR);
}

static void
setup_locale(const char *locale)
{
	(void)setlocale(LC_ALL, locale);
#ifdef WITH_ICONV
	if (!doall)
		set_new_encoding();
#endif
	setnnames();
}

#define	REPLACE(string, slen, struct_) \
		if (strncasecmp(buf, (string), (slen)) == 0 && buf[(slen)]) { \
			if (struct_.name != NULL)			      \
				free(struct_.name);			      \
			if ((struct_.name = strdup(buf + (slen))) == NULL)    \
				errx(1, "cannot allocate memory");	      \
			struct_.len = strlen(buf + (slen));		      \
			continue;					      \
		}
static int
cal_parse(FILE *in, FILE *out)
{
	char *mylocale = NULL;
	char *line = NULL;
	char *buf, *bufp;
	size_t linecap = 0;
	ssize_t linelen;
	ssize_t l;
	static int count = 0;
	int i;
	int month[MAXCOUNT];
	int day[MAXCOUNT];
	int year[MAXCOUNT];
	int skip = 0;
	int unskip = 0;
	char *pp, p;
	int flags;
	char *c, *cc;
	bool incomment = false;

	if (in == NULL)
		return (1);

	cal_line = 0;
	while ((linelen = getline(&line, &linecap, in)) > 0) {
		cal_line++;
		buf = line;
		if (buf[linelen - 1] == '\n')
			buf[--linelen] = '\0';

		if (incomment) {
			c = strstr(buf, "*/");
			if (c) {
				c += 2;
				linelen -= c - buf;
				buf = c;
				incomment = false;
			} else {
				continue;
			}
		}
		if (!incomment) {
			bufp = buf;
			do {
				c = strstr(bufp, "//");
				cc = strstr(bufp, "/*");
				if (c != NULL && (cc == NULL || c - cc < 0)) {
					bufp = c + 2;
					/* ignore "//" within string to allow it in an URL */
					if (c == buf || isspace(c[-1])) {
						/* single line comment */
						*c = '\0';
						linelen = c - buf;
						break;
					}
				} else if (cc != NULL) {
					c = strstr(cc + 2, "*/");
					if (c != NULL) { // 'a /* b */ c' -- cc=2, c=7+2
						/* multi-line comment ending on same line */
						c += 2;
						memmove(cc, c, buf + linelen + 1 - c);
						linelen -= c - cc;
						bufp = cc;
					} else {
						/* multi-line comment */
						*cc = '\0';
						linelen = cc - buf;
						incomment = true;
						break;
					}
				}
			} while (c != NULL || cc != NULL);
		}

		for (l = linelen;
		     l > 0 && isspace((unsigned char)buf[l - 1]);
		     l--)
			;
		buf[l] = '\0';
		if (buf[0] == '\0')
			continue;

		if (buf == line && *buf == '#') {
			switch (token(buf+1, out, &skip, &unskip)) {
			case T_ERR:
				free(line);
				return (1);
			case T_OK:
				continue;
			case T_PROCESS:
				break;
			default:
				break;
			}
		}

		if (skip != 0)
			continue;

		/*
		 * Setting LANG in user's calendar was an old workaround
		 * for 'calendar -a' being run with C locale to properly
		 * print user's calendars in their native languages.
		 * Now that 'calendar -a' does fork with setusercontext(),
		 * and does not run iconv(), this variable has little use.
		 */
		if (strncmp(buf, "LANG=", 5) == 0) {
			if (mylocale == NULL)
				mylocale = strdup(setlocale(LC_ALL, NULL));
			setup_locale(buf + 5);
			continue;
		}
		/* Parse special definitions: Easter, Paskha etc */
		REPLACE("Easter=", 7, neaster);
		REPLACE("Paskha=", 7, npaskha);
		REPLACE("ChineseNewYear=", 15, ncny);
		REPLACE("NewMoon=", 8, nnewmoon);
		REPLACE("FullMoon=", 9, nfullmoon);
		REPLACE("MarEquinox=", 11, nmarequinox);
		REPLACE("SepEquinox=", 11, nsepequinox);
		REPLACE("JunSolstice=", 12, njunsolstice);
		REPLACE("DecSolstice=", 12, ndecsolstice);
		if (strncmp(buf, "SEQUENCE=", 9) == 0) {
			setnsequences(buf + 9);
			continue;
		}

		/*
		 * If the line starts with a tab, the data has to be
		 * added to the previous line
		 */
		if (buf[0] == '\t') {
			for (i = 0; i < count; i++)
				event_continue(events[i], buf);
			continue;
		}

		/* Get rid of leading spaces (non-standard) */
		while (isspace((unsigned char)buf[0]))
			memcpy(buf, buf + 1, strlen(buf));

		/* No tab in the line, then not a valid line */
		if ((pp = strchr(buf, '\t')) == NULL)
			continue;

		/* Trim spaces in front of the tab */
		while (isspace((unsigned char)pp[-1]))
			pp--;

		p = *pp;
		*pp = '\0';
		if ((count = parsedaymonth(buf, year, month, day, &flags,
		    extradata)) == 0)
			continue;
		*pp = p;
		if (count < 0) {
			/* Show error status based on return value */
			if (debug)
				WARN1("Ignored: \"%s\"", buf);
			if (count == -1)
				continue;
			count = -count + 1;
		}

		/* Find the last tab */
		while (pp[1] == '\t')
			pp++;

		for (i = 0; i < count; i++) {
			if (debug)
				WARN1("got \"%s\"", pp);
			events[i] = event_add(year[i], month[i], day[i],
			    ((flags &= F_VARIABLE) != 0) ? 1 : 0, pp,
			    extradata[i]);
		}
	}
	while (skip-- > 0 || unskip-- > 0) {
		cal_line++;
		WARN0("Missing #endif assumed");
	}

	free(line);
	fclose(in);
	if (mylocale != NULL) {
		setup_locale(mylocale);
		free(mylocale);
	}

	return (0);
}

void
cal(void)
{
	FILE *fpin;
	FILE *fpout;
	int i;

	for (i = 0; i < MAXCOUNT; i++)
		extradata[i] = (char *)calloc(1, 20);


	if ((fpin = opencalin()) == NULL)
		return;

	if ((fpout = opencalout()) == NULL) {
		fclose(fpin);
		return;
	}

	if (cal_parse(fpin, fpout))
		return;

	event_print_all(fpout);
	closecal(fpout);
}

FILE *
opencalin(void)
{
	struct stat sbuf;
	FILE *fpin;

	/* open up calendar file */
	cal_file = calendarFile;
	if ((fpin = fopen(calendarFile, "r")) == NULL) {
		if (doall) {
			if (chdir(calendarHomes[0]) != 0)
				return (NULL);
			if (stat(calendarNoMail, &sbuf) == 0)
				return (NULL);
			if ((fpin = fopen(calendarFile, "r")) == NULL)
				return (NULL);
		} else {
			fpin = cal_fopen(calendarFile);
		}
	}
	return (fpin);
}

FILE *
opencalout(void)
{
	int fd;

	/* not reading all calendar files, just set output to stdout */
	if (!doall)
		return (stdout);

	/* set output to a temporary file, so if no output don't send mail */
	snprintf(path, sizeof(path), "%s/_calXXXXXX", _PATH_TMP);
	if ((fd = mkstemp(path)) < 0)
		return (NULL);
	return (fdopen(fd, "w+"));
}

void
closecal(FILE *fp)
{
	struct stat sbuf;
	int nread, pdes[2], status;
	char buf[1024];

	if (!doall)
		return;

	rewind(fp);
	if (fstat(fileno(fp), &sbuf) || !sbuf.st_size)
		goto done;
	if (pipe(pdes) < 0)
		goto done;
	switch (fork()) {
	case -1:			/* error */
		(void)close(pdes[0]);
		(void)close(pdes[1]);
		goto done;
	case 0:
		/* child -- set stdin to pipe output */
		if (pdes[0] != STDIN_FILENO) {
			(void)dup2(pdes[0], STDIN_FILENO);
			(void)close(pdes[0]);
		}
		(void)close(pdes[1]);
		execl(_PATH_SENDMAIL, "sendmail", "-i", "-t", "-F",
		    "\"Reminder Service\"", (char *)NULL);
		warn(_PATH_SENDMAIL);
		_exit(1);
	}
	/* parent -- write to pipe input */
	(void)close(pdes[0]);

	write(pdes[1], "From: \"Reminder Service\" <", 26);
	write(pdes[1], pw->pw_name, strlen(pw->pw_name));
	write(pdes[1], ">\nTo: <", 7);
	write(pdes[1], pw->pw_name, strlen(pw->pw_name));
	write(pdes[1], ">\nSubject: ", 11);
	write(pdes[1], dayname, strlen(dayname));
	write(pdes[1], "'s Calendar\nPrecedence: bulk\n\n", 30);

	while ((nread = read(fileno(fp), buf, sizeof(buf))) > 0)
		(void)write(pdes[1], buf, nread);
	(void)close(pdes[1]);
done:	(void)fclose(fp);
	(void)unlink(path);
	while (wait(&status) >= 0);
}
