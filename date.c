/* Display or set the current time and date.  */

/* Copyright 1985, 1987, 1988 The Regents of the University of California.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of the University nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
   SUCH DAMAGE.  */

#include "private.h"
#include <locale.h>
#include <stdio.h>

#if !HAVE_POSIX_DECLS
extern char *		optarg;
extern int		optind;
#endif

static int		retval = EXIT_SUCCESS;

static void		display(const char *, time_t);
static void		dogmt(void);
static void		errensure(void);
static void		timeout(FILE *, const char *, const struct tm *);
static ATTRIBUTE_NORETURN void usage(void);

int
main(const int argc, char *argv[])
{
	register const char *	format = "+%+";
	register int		ch;
	register bool		rflag = false;
	time_t			t;
	intmax_t		secs;
	char *			endarg;

#ifdef LC_ALL
	setlocale(LC_ALL, "");
#endif /* defined(LC_ALL) */
#if HAVE_GETTEXT
# ifdef TZ_DOMAINDIR
	bindtextdomain(TZ_DOMAIN, TZ_DOMAINDIR);
# endif /* defined(TEXTDOMAINDIR) */
	textdomain(TZ_DOMAIN);
#endif /* HAVE_GETTEXT */
	t = time(NULL);
	while ((ch = getopt(argc, argv, "ucr:")) != EOF && ch != -1) {
		switch (ch) {
		default:
			usage();
		case 'u':		/* do it in UT */
		case 'c':
			dogmt();
			break;
		case 'r':		/* seconds since 1970 */
			if (rflag) {
				fprintf(stderr,
					_("date: error: multiple -r's used"));
				usage();
			}
			rflag = true;
			errno = 0;
			secs = strtoimax(optarg, &endarg, 0);
			if (*endarg || optarg == endarg)
				errno = EINVAL;
			else if (! (TIME_T_MIN <= secs && secs <= TIME_T_MAX))
				errno = ERANGE;
			if (errno) {
				perror(optarg);
				errensure();
				exit(retval);
			}
			t = secs;
			break;
		}
	}
	if (optind < argc) {
	  if (argc - optind != 1) {
	    fprintf(stderr,
		    _("date: error: multiple operands in command line\n"));
	    usage();
	  }
	  format = argv[optind];
	  if (*format != '+') {
	    fprintf(stderr, _("date: unknown operand: %s\n"), format);
	    usage();
	  }
	}

	display(format, t);
	return retval;
}

static void
dogmt(void)
{
	static char **	fakeenv;

	if (fakeenv == NULL) {
		static char	tzeutc0[] = "TZ=UTC0";
		ptrdiff_t from, to, n;

		for (n = 0;  environ[n] != NULL;  ++n)
			continue;
#if defined ckd_add && defined ckd_mul
		if (!ckd_add(&n, n, 2) && !ckd_mul(&n, n, sizeof *fakeenv)
		    && n <= SIZE_MAX)
		  fakeenv = malloc(n);
#else
		if (n <= min(PTRDIFF_MAX, SIZE_MAX) / sizeof *fakeenv - 2)
		  fakeenv = malloc((n + 2) * sizeof *fakeenv);
#endif
		if (fakeenv == NULL) {
			fprintf(stderr, _("date: Memory exhausted\n"));
			errensure();
			exit(retval);
		}
		to = 0;
		fakeenv[to++] = tzeutc0;
		for (from = 1; environ[from] != NULL; ++from)
			if (strncmp(environ[from], "TZ=", 3) != 0)
				fakeenv[to++] = environ[from];
		fakeenv[to] = NULL;
		environ = fakeenv;
	}
}

static void
errensure(void)
{
	if (retval == EXIT_SUCCESS)
		retval = EXIT_FAILURE;
}

static void
usage(void)
{
	fprintf(stderr,
		       _("date: usage: date [-u] [-c] [-r seconds]"
			 " [+format]\n"));
	errensure();
	exit(retval);
}

static void
display(char const *format, time_t now)
{
	struct tm *tmp;

	tmp = localtime(&now);
	if (!tmp) {
		fprintf(stderr,
			_("date: error: time out of range\n"));
		errensure();
		return;
	}
	timeout(stdout, format, tmp);
	putchar('\n');
	fflush(stdout);
	fflush(stderr);
	if (ferror(stdout) || ferror(stderr)) {
		fprintf(stderr,
			_("date: error: couldn't write results\n"));
		errensure();
	}
}

static void
timeout(FILE *fp, char const *format, struct tm const *tmp)
{
	char *cp = NULL;
	ptrdiff_t result;
	ptrdiff_t size = 1024 / 2;

	for ( ; ; ) {
#ifdef ckd_mul
		bool bigger = !ckd_mul(&size, size, 2) && size <= SIZE_MAX;
#else
		bool bigger = (size <= min(PTRDIFF_MAX, SIZE_MAX) / 2
			       && (size *= 2, true));
#endif
		char *newcp = bigger ? realloc(cp, size) : NULL;
		if (!newcp) {
			fprintf(stderr,
				_("date: error: can't get memory\n"));
			errensure();
			exit(retval);
		}
		cp = newcp;
		result = strftime(cp, size, format, tmp);
		if (result != 0)
			break;
	}
	fwrite(cp + 1, 1, result - 1, fp);
	free(cp);
}
