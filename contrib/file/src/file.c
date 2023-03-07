/*
 * Copyright (c) Ian F. Darwin 1986-1995.
 * Software written by Ian F. Darwin and others;
 * maintained 1995-present by Christos Zoulas and others.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * file - find type of a file or files - main program.
 */

#include "file.h"

#ifndef	lint
FILE_RCSID("@(#)$File: file.c,v 1.204 2022/09/13 18:46:07 christos Exp $")
#endif	/* lint */

#include "magic.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#ifdef RESTORE_TIME
# if (__COHERENT__ >= 0x420)
#  include <sys/utime.h>
# else
#  ifdef USE_UTIMES
#   include <sys/time.h>
#  else
#   include <utime.h>
#  endif
# endif
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>	/* for read() */
#endif
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif
#ifdef HAVE_WCTYPE_H
#include <wctype.h>
#endif
#if defined(HAVE_WCHAR_H) && defined(HAVE_MBRTOWC) && defined(HAVE_WCWIDTH) && \
   defined(HAVE_WCTYPE_H)
#define FILE_WIDE_SUPPORT
#else
#include <ctype.h>
#endif

#if defined(HAVE_GETOPT_H) && defined(HAVE_STRUCT_OPTION)
# include <getopt.h>
# ifndef HAVE_GETOPT_LONG
int getopt_long(int, char * const *, const char *,
    const struct option *, int *);
# endif
# else
#  include "mygetopt.h"
#endif

#ifdef S_IFLNK
# define IFLNK_h "h"
# define IFLNK_L "L"
#else
# define IFLNK_h ""
# define IFLNK_L ""
#endif

#define FILE_FLAGS	"bcCdE" IFLNK_h "ik" IFLNK_L "lNnprsSvzZ0"
#define OPTSTRING	"bcCde:Ef:F:hiklLm:nNpP:rsSvzZ0"

# define USAGE  \
    "Usage: %s [-" FILE_FLAGS "] [--apple] [--extension] [--mime-encoding]\n" \
    "            [--mime-type] [-e <testname>] [-F <separator>] " \
    " [-f <namefile>]\n" \
    "            [-m <magicfiles>] [-P <parameter=value>] [--exclude-quiet]\n" \
    "            <file> ...\n" \
    "       %s -C [-m <magicfiles>]\n" \
    "       %s [--help]\n"

private int 		/* Global command-line options 		*/
	bflag = 0,	/* brief output format	 		*/
	nopad = 0,	/* Don't pad output			*/
	nobuffer = 0,   /* Do not buffer stdout 		*/
	nulsep = 0;	/* Append '\0' to the separator		*/

private const char *separator = ":";	/* Default field separator	*/
private const struct option long_options[] = {
#define OPT_HELP		1
#define OPT_APPLE		2
#define OPT_EXTENSIONS		3
#define OPT_MIME_TYPE		4
#define OPT_MIME_ENCODING	5
#define OPT_EXCLUDE_QUIET	6
#define OPT(shortname, longname, opt, def, doc)		\
    {longname, opt, NULL, shortname},
#define OPT_LONGONLY(longname, opt, def, doc, id)	\
    {longname, opt, NULL, id},
#include "file_opts.h"
#undef OPT
#undef OPT_LONGONLY
    {0, 0, NULL, 0}
    };

private const struct {
	const char *name;
	int value;
} nv[] = {
	{ "apptype",	MAGIC_NO_CHECK_APPTYPE },
	{ "ascii",	MAGIC_NO_CHECK_ASCII },
	{ "cdf",	MAGIC_NO_CHECK_CDF },
	{ "compress",	MAGIC_NO_CHECK_COMPRESS },
	{ "csv",	MAGIC_NO_CHECK_CSV },
	{ "elf",	MAGIC_NO_CHECK_ELF },
	{ "encoding",	MAGIC_NO_CHECK_ENCODING },
	{ "soft",	MAGIC_NO_CHECK_SOFT },
	{ "tar",	MAGIC_NO_CHECK_TAR },
	{ "json",	MAGIC_NO_CHECK_JSON },
	{ "text",	MAGIC_NO_CHECK_TEXT },	/* synonym for ascii */
	{ "tokens",	MAGIC_NO_CHECK_TOKENS }, /* OBSOLETE: ignored for backwards compatibility */
};

private struct {
	const char *name;
	size_t value;
	size_t def;
	const char *desc;
	int tag;
	int set;
} pm[] = {
	{ "bytes", 0, FILE_BYTES_MAX, "max bytes to look inside file",
	    MAGIC_PARAM_BYTES_MAX, 0 },
	{ "elf_notes", 0, FILE_ELF_NOTES_MAX, "max ELF notes processed",
	    MAGIC_PARAM_ELF_NOTES_MAX, 0 },
	{ "elf_phnum", 0, FILE_ELF_PHNUM_MAX, "max ELF prog sections processed",
	    MAGIC_PARAM_ELF_PHNUM_MAX, 0 },
	{ "elf_shnum", 0, FILE_ELF_SHNUM_MAX, "max ELF sections processed",
	    MAGIC_PARAM_ELF_SHNUM_MAX, 0 },
	{ "encoding", 0, FILE_ENCODING_MAX, "max bytes to scan for encoding",
	    MAGIC_PARAM_ENCODING_MAX, 0 },
	{ "indir", 0, FILE_INDIR_MAX, "recursion limit for indirection",
	    MAGIC_PARAM_INDIR_MAX, 0 },
	{ "name", 0, FILE_NAME_MAX, "use limit for name/use magic",
	    MAGIC_PARAM_NAME_MAX, 0 },
	{ "regex", 0, FILE_REGEX_MAX, "length limit for REGEX searches",
	    MAGIC_PARAM_REGEX_MAX, 0 },
};

private int posixly;

#ifdef __dead
__dead
#endif
private void usage(void);
private void docprint(const char *, int);
#ifdef __dead
__dead
#endif
private void help(void);

private int unwrap(struct magic_set *, const char *);
private int process(struct magic_set *ms, const char *, int);
private struct magic_set *load(const char *, int);
private void setparam(const char *);
private void applyparam(magic_t);


/*
 * main - parse arguments and handle options
 */
int
main(int argc, char *argv[])
{
	int c;
	size_t i, j, wid, nw;
	int action = 0, didsomefiles = 0, errflg = 0;
	int flags = 0, e = 0;
#ifdef HAVE_LIBSECCOMP
	int sandbox = 1;
#endif
	struct magic_set *magic = NULL;
	int longindex;
	const char *magicfile = NULL;		/* where the magic is	*/
	char *progname;

	/* makes islower etc work for other langs */
	(void)setlocale(LC_CTYPE, "");

#ifdef __EMX__
	/* sh-like wildcard expansion! Shouldn't hurt at least ... */
	_wildcard(&argc, &argv);
#endif

	if ((progname = strrchr(argv[0], '/')) != NULL)
		progname++;
	else
		progname = argv[0];

	file_setprogname(progname);


#ifdef S_IFLNK
	posixly = getenv("POSIXLY_CORRECT") != NULL;
	flags |=  posixly ? MAGIC_SYMLINK : 0;
#endif
	while ((c = getopt_long(argc, argv, OPTSTRING, long_options,
	    &longindex)) != -1)
		switch (c) {
		case OPT_HELP:
			help();
			break;
		case OPT_APPLE:
			flags |= MAGIC_APPLE;
			break;
		case OPT_EXTENSIONS:
			flags |= MAGIC_EXTENSION;
			break;
		case OPT_MIME_TYPE:
			flags |= MAGIC_MIME_TYPE;
			break;
		case OPT_MIME_ENCODING:
			flags |= MAGIC_MIME_ENCODING;
			break;
		case '0':
			nulsep++;
			break;
		case 'b':
			bflag++;
			break;
		case 'c':
			action = FILE_CHECK;
			break;
		case 'C':
			action = FILE_COMPILE;
			break;
		case 'd':
			flags |= MAGIC_DEBUG|MAGIC_CHECK;
			break;
		case 'E':
			flags |= MAGIC_ERROR;
			break;
		case 'e':
		case OPT_EXCLUDE_QUIET:
			for (i = 0; i < __arraycount(nv); i++)
				if (strcmp(nv[i].name, optarg) == 0)
					break;

			if (i == __arraycount(nv)) {
				if (c != OPT_EXCLUDE_QUIET)
					errflg++;
			} else
				flags |= nv[i].value;
			break;

		case 'f':
			if(action)
				usage();
			if (magic == NULL)
				if ((magic = load(magicfile, flags)) == NULL)
					return 1;
			applyparam(magic);
			e |= unwrap(magic, optarg);
			++didsomefiles;
			break;
		case 'F':
			separator = optarg;
			break;
		case 'i':
			flags |= MAGIC_MIME;
			break;
		case 'k':
			flags |= MAGIC_CONTINUE;
			break;
		case 'l':
			action = FILE_LIST;
			break;
		case 'm':
			magicfile = optarg;
			break;
		case 'n':
			++nobuffer;
			break;
		case 'N':
			++nopad;
			break;
#if defined(HAVE_UTIME) || defined(HAVE_UTIMES)
		case 'p':
			flags |= MAGIC_PRESERVE_ATIME;
			break;
#endif
		case 'P':
			setparam(optarg);
			break;
		case 'r':
			flags |= MAGIC_RAW;
			break;
		case 's':
			flags |= MAGIC_DEVICES;
			break;
		case 'S':
#ifdef HAVE_LIBSECCOMP
			sandbox = 0;
#endif
			break;
		case 'v':
			if (magicfile == NULL)
				magicfile = magic_getpath(magicfile, action);
			(void)fprintf(stdout, "%s-%s\n", file_getprogname(),
			    VERSION);
			(void)fprintf(stdout, "magic file from %s\n",
			    magicfile);
#ifdef HAVE_LIBSECCOMP
			(void)fprintf(stdout, "seccomp support included\n");
#endif
			return 0;
		case 'z':
			flags |= MAGIC_COMPRESS;
			break;

		case 'Z':
			flags |= MAGIC_COMPRESS|MAGIC_COMPRESS_TRANSP;
			break;
#ifdef S_IFLNK
		case 'L':
			flags |= MAGIC_SYMLINK;
			break;
		case 'h':
			flags &= ~MAGIC_SYMLINK;
			break;
#endif
		case '?':
		default:
			errflg++;
			break;
		}

	if (errflg) {
		usage();
	}
	if (e)
		return e;

#ifdef HAVE_LIBSECCOMP
#if 0
	if (sandbox && enable_sandbox_basic() == -1)
#else
	if (sandbox && enable_sandbox_full() == -1)
#endif
		file_err(EXIT_FAILURE, "SECCOMP initialisation failed");
#endif /* HAVE_LIBSECCOMP */

	if (MAGIC_VERSION != magic_version())
		file_warnx("Compiled magic version [%d] "
		    "does not match with shared library magic version [%d]\n",
		    MAGIC_VERSION, magic_version());

	switch(action) {
	case FILE_CHECK:
	case FILE_COMPILE:
	case FILE_LIST:
		/*
		 * Don't try to check/compile ~/.magic unless we explicitly
		 * ask for it.
		 */
		magic = magic_open(flags|MAGIC_CHECK);
		if (magic == NULL) {
			file_warn("Can't create magic");
			return 1;
		}


		switch(action) {
		case FILE_CHECK:
			c = magic_check(magic, magicfile);
			break;
		case FILE_COMPILE:
			c = magic_compile(magic, magicfile);
			break;
		case FILE_LIST:
			c = magic_list(magic, magicfile);
			break;
		default:
			abort();
		}
		if (c == -1) {
			file_warnx("%s", magic_error(magic));
			e = 1;
			goto out;
		}
		goto out;
	default:
		if (magic == NULL)
			if ((magic = load(magicfile, flags)) == NULL)
				return 1;
		applyparam(magic);
	}

	if (optind == argc) {
		if (!didsomefiles)
			usage();
		goto out;
	}

	for (wid = 0, j = CAST(size_t, optind); j < CAST(size_t, argc);
	    j++) {
		nw = file_mbswidth(magic, argv[j]);
		if (nw > wid)
			wid = nw;
	}

	/*
	 * If bflag is only set twice, set it depending on
	 * number of files [this is undocumented, and subject to change]
	 */
	if (bflag == 2) {
		bflag = optind >= argc - 1;
	}
	for (; optind < argc; optind++)
		e |= process(magic, argv[optind], wid);

out:
	if (!nobuffer)
		e |= fflush(stdout) != 0;

	if (magic)
		magic_close(magic);
	return e;
}

private void
applyparam(magic_t magic)
{
	size_t i;

	for (i = 0; i < __arraycount(pm); i++) {
		if (!pm[i].set)
			continue;
		if (magic_setparam(magic, pm[i].tag, &pm[i].value) == -1)
			file_err(EXIT_FAILURE, "Can't set %s", pm[i].name);
	}
}

private void
setparam(const char *p)
{
	size_t i;
	char *s;

	if ((s = CCAST(char *, strchr(p, '='))) == NULL)
		goto badparm;

	for (i = 0; i < __arraycount(pm); i++) {
		if (strncmp(p, pm[i].name, s - p) != 0)
			continue;
		pm[i].value = atoi(s + 1);
		pm[i].set = 1;
		return;
	}
badparm:
	file_errx(EXIT_FAILURE, "Unknown param %s", p);
}

private struct magic_set *
/*ARGSUSED*/
load(const char *magicfile, int flags)
{
	struct magic_set *magic = magic_open(flags);
	const char *e;

	if (magic == NULL) {
		file_warn("Can't create magic");
		return NULL;
	}
	if (magic_load(magic, magicfile) == -1) {
		file_warn("%s", magic_error(magic));
		magic_close(magic);
		return NULL;
	}
	if ((e = magic_error(magic)) != NULL)
		file_warn("%s", e);
	return magic;
}

/*
 * unwrap -- read a file of filenames, do each one.
 */
private int
unwrap(struct magic_set *ms, const char *fn)
{
	FILE *f;
	ssize_t len;
	char *line = NULL;
	size_t llen = 0;
	int wid = 0, cwid;
	int e = 0;
	size_t fi = 0, fimax = 0;
	char **flist = NULL;

	if (strcmp("-", fn) == 0)
		f = stdin;
	else {
		if ((f = fopen(fn, "r")) == NULL) {
			file_warn("Cannot open `%s'", fn);
			return 1;
		}
	}

	while ((len = getline(&line, &llen, f)) > 0) {
		if (line[len - 1] == '\n')
			line[len - 1] = '\0';
		cwid = file_mbswidth(ms, line);
		if (nobuffer) {
			e |= process(ms, line, cwid);
			free(line);
			line = NULL;
			llen = 0;
			continue;
		}
		if (cwid > wid)
			wid = cwid;
		if (fi >= fimax) {
			fimax += 100;
			char **nf = CAST(char **,
			    realloc(flist, fimax * sizeof(*flist)));
			if (nf == NULL) {
				file_err(EXIT_FAILURE,
				    "Cannot allocate memory for file list");
			}
			flist = nf;
		}
		flist[fi++] = line;
		line = NULL;
		llen = 0;
	}

	if (!nobuffer) {
		fimax = fi;
		for (fi = 0; fi < fimax; fi++) {
			e |= process(ms, flist[fi], wid);
			free(flist[fi]);
		}
	}
	free(flist);

	if (f != stdin)
		(void)fclose(f);
	return e;
}

private void
file_octal(unsigned char c)
{
	putc('\\', stdout);
	putc(((c >> 6) & 7) + '0', stdout);
	putc(((c >> 3) & 7) + '0', stdout);
	putc(((c >> 0) & 7) + '0', stdout);
}

private void
fname_print(const char *inname)
{
	size_t n = strlen(inname);
#ifdef FILE_WIDE_SUPPORT
	mbstate_t state;
	wchar_t nextchar;
	size_t bytesconsumed;


	(void)memset(&state, 0, sizeof(state));
	while (n > 0) {
		bytesconsumed = mbrtowc(&nextchar, inname, n, &state);
		if (bytesconsumed == CAST(size_t, -1) ||
		    bytesconsumed == CAST(size_t, -2))  {
			nextchar = *inname++;
			n--;
			(void)memset(&state, 0, sizeof(state));
			file_octal(CAST(unsigned char, nextchar));
			continue;
		}
		inname += bytesconsumed;
		n -= bytesconsumed;
		if (iswprint(nextchar)) {
			printf("%lc", (wint_t)nextchar);
			continue;
		}
		/* XXX: What if it is > 255? */
		file_octal(CAST(unsigned char, nextchar));
	}
#else
	size_t i;
	for (i = 0; i < n; i++) {
		unsigned char c = CAST(unsigned char, inname[i]);
		if (isprint(c)) {
			putc(c);
			continue;
		}
		file_octal(c);
	}
#endif
}

/*
 * Called for each input file on the command line (or in a list of files)
 */
private int
process(struct magic_set *ms, const char *inname, int wid)
{
	const char *type, c = nulsep > 1 ? '\0' : '\n';
	int std_in = strcmp(inname, "-") == 0;
	int haderror = 0;

	if (wid > 0 && !bflag) {
		const char *pname = std_in ? "/dev/stdin" : inname;
		if ((ms->flags & MAGIC_RAW) == 0)
			fname_print(pname);
		else
			(void)printf("%s", pname);
		if (nulsep)
			(void)putc('\0', stdout);
		if (nulsep < 2) {
			(void)printf("%s", separator);
			(void)printf("%*s ", CAST(int, nopad ? 0
			    : (wid - file_mbswidth(ms, inname))), "");
		}
	}

	type = magic_file(ms, std_in ? NULL : inname);

	if (type == NULL) {
		haderror |= printf("ERROR: %s%c", magic_error(ms), c);
	} else {
		haderror |= printf("%s%c", type, c) < 0;
	}
	if (nobuffer)
		haderror |= fflush(stdout) != 0;
	return haderror || type == NULL;
}

protected size_t
file_mbswidth(struct magic_set *ms, const char *s)
{
	size_t width = 0;
#ifdef FILE_WIDE_SUPPORT
	size_t bytesconsumed, n;
	mbstate_t state;
	wchar_t nextchar;

	(void)memset(&state, 0, sizeof(state));
	n = strlen(s);

	while (n > 0) {
		bytesconsumed = mbrtowc(&nextchar, s, n, &state);
		if (bytesconsumed == CAST(size_t, -1) ||
		    bytesconsumed == CAST(size_t, -2)) {
			nextchar = *s;
			bytesconsumed = 1;
			(void)memset(&state, 0, sizeof(state));
			width += 4;
		} else {
			int w = wcwidth(nextchar);
			width += ((ms->flags & MAGIC_RAW) != 0
			    || iswprint(nextchar)) ? (w > 0 ? w : 1) : 4;
		}

		s += bytesconsumed, n -= bytesconsumed;
	}
#else
	for (; *s; s++) {
		width += (ms->flags & MAGIC_RAW) != 0
		    || isprint(CAST(unsigned char, *s)) ? 1 : 4;
	}
#endif
	return width;
}

private void
usage(void)
{
	const char *pn = file_getprogname();
	(void)fprintf(stderr, USAGE, pn, pn, pn);
	exit(EXIT_FAILURE);
}

private void
defprint(int def)
{
	if (!def)
		return;
	if (((def & 1) && posixly) || ((def & 2) && !posixly))
		fprintf(stdout, " (default)");
	fputc('\n', stdout);
}

private void
docprint(const char *opts, int def)
{
	size_t i;
	int comma, pad;
	char *sp, *p;

	p = CCAST(char *, strchr(opts, '%'));
	if (p == NULL) {
		fprintf(stdout, "%s", opts);
		defprint(def);
		return;
	}

	for (sp = p - 1; sp > opts && *sp == ' '; sp--)
		continue;

	fprintf(stdout, "%.*s", CAST(int, p - opts), opts);
	pad = (int)CAST(int, p - sp - 1);

	switch (*++p) {
	case 'e':
		comma = 0;
		for (i = 0; i < __arraycount(nv); i++) {
			fprintf(stdout, "%s%s", comma++ ? ", " : "", nv[i].name);
			if (i && i % 5 == 0 && i != __arraycount(nv) - 1) {
				fprintf(stdout, ",\n%*s", pad, "");
				comma = 0;
			}
		}
		break;
	case 'P':
		for (i = 0; i < __arraycount(pm); i++) {
			fprintf(stdout, "%9s %7zu %s", pm[i].name, pm[i].def,
			    pm[i].desc);
			if (i != __arraycount(pm) - 1)
				fprintf(stdout, "\n%*s", pad, "");
		}
		break;
	default:
		file_errx(EXIT_FAILURE, "Unknown escape `%c' in long options",
		   *p);
		break;
	}
	fprintf(stdout, "%s", opts + (p - opts) + 1);

}

private void
help(void)
{
	(void)fputs(
"Usage: file [OPTION...] [FILE...]\n"
"Determine type of FILEs.\n"
"\n", stdout);
#define OPT(shortname, longname, opt, def, doc)      \
	fprintf(stdout, "  -%c, --" longname, shortname), \
	docprint(doc, def);
#define OPT_LONGONLY(longname, opt, def, doc, id)    	\
	fprintf(stdout, "      --" longname),	\
	docprint(doc, def);
#include "file_opts.h"
#undef OPT
#undef OPT_LONGONLY
	fprintf(stdout, "\nReport bugs to https://bugs.astron.com/\n");
	exit(EXIT_SUCCESS);
}

private const char *file_progname;

protected void
file_setprogname(const char *progname)
{
	file_progname = progname;
}

protected const char *
file_getprogname(void)
{
	return file_progname;
}

protected void
file_err(int e, const char *fmt, ...)
{
	va_list ap;
	int se = errno;

	va_start(ap, fmt);
	fprintf(stderr, "%s: ", file_progname);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (se)
		fprintf(stderr, " (%s)\n", strerror(se));
	else
		fputc('\n', stderr);
	exit(e);
}

protected void
file_errx(int e, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "%s: ", file_progname);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	exit(e);
}

protected void
file_warn(const char *fmt, ...)
{
	va_list ap;
	int se = errno;

	va_start(ap, fmt);
	fprintf(stderr, "%s: ", file_progname);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (se)
		fprintf(stderr, " (%s)\n", strerror(se));
	else
		fputc('\n', stderr);
	errno = se;
}

protected void
file_warnx(const char *fmt, ...)
{
	va_list ap;
	int se = errno;

	va_start(ap, fmt);
	fprintf(stderr, "%s: ", file_progname);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	errno = se;
}
