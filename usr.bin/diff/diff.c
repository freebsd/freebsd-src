/*	$OpenBSD: diff.c,v 1.67 2019/06/28 13:35:00 deraadt Exp $	*/

/*
 * Copyright (c) 2003 Todd C. Miller <Todd.Miller@courtesan.com>
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
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#include <sys/cdefs.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "diff.h"
#include "xmalloc.h"

static const char diff_version[] = "FreeBSD diff 20220309";
bool	 lflag, Nflag, Pflag, rflag, sflag, Tflag, cflag;
bool	 ignore_file_case, suppress_common, color, noderef;
static bool help = false;
int	 diff_format, diff_context, status;
int	 tabsize = 8, width = 130;
static int	colorflag = COLORFLAG_NEVER;
char	*start, *ifdefname, *diffargs, *label[2];
char	*ignore_pats, *most_recent_pat;
char	*group_format = NULL;
const char	*add_code, *del_code;
struct stat stb1, stb2;
struct excludes *excludes_list;
regex_t	 ignore_re, most_recent_re;

#define	OPTIONS	"0123456789aBbC:cdD:efF:HhI:iL:lnNPpqrS:sTtU:uwW:X:x:y"
enum {
	OPT_TSIZE = CHAR_MAX + 1,
	OPT_STRIPCR,
	OPT_IGN_FN_CASE,
	OPT_NO_IGN_FN_CASE,
	OPT_NORMAL,
	OPT_HELP,
	OPT_HORIZON_LINES,
	OPT_CHANGED_GROUP_FORMAT,
	OPT_SUPPRESS_COMMON,
	OPT_COLOR,
	OPT_NO_DEREFERENCE,
	OPT_VERSION,
};

static struct option longopts[] = {
	{ "text",			no_argument,		0,	'a' },
	{ "ignore-space-change",	no_argument,		0,	'b' },
	{ "context",			optional_argument,	0,	'C' },
	{ "ifdef",			required_argument,	0,	'D' },
	{ "minimal",			no_argument,		0,	'd' },
	{ "ed",				no_argument,		0,	'e' },
	{ "forward-ed",			no_argument,		0,	'f' },
	{ "show-function-line",		required_argument,	0,	'F' },
	{ "speed-large-files",		no_argument,		NULL,	'H' },
	{ "ignore-blank-lines",		no_argument,		0,	'B' },
	{ "ignore-matching-lines",	required_argument,	0,	'I' },
	{ "ignore-case",		no_argument,		0,	'i' },
	{ "paginate",			no_argument,		NULL,	'l' },
	{ "label",			required_argument,	0,	'L' },
	{ "new-file",			no_argument,		0,	'N' },
	{ "rcs",			no_argument,		0,	'n' },
	{ "unidirectional-new-file",	no_argument,		0,	'P' },
	{ "show-c-function",		no_argument,		0,	'p' },
	{ "brief",			no_argument,		0,	'q' },
	{ "recursive",			no_argument,		0,	'r' },
	{ "report-identical-files",	no_argument,		0,	's' },
	{ "starting-file",		required_argument,	0,	'S' },
	{ "expand-tabs",		no_argument,		0,	't' },
	{ "initial-tab",		no_argument,		0,	'T' },
	{ "unified",			optional_argument,	0,	'U' },
	{ "ignore-all-space",		no_argument,		0,	'w' },
	{ "width",			required_argument,	0,	'W' },
	{ "exclude",			required_argument,	0,	'x' },
	{ "exclude-from",		required_argument,	0,	'X' },
	{ "side-by-side",		no_argument,		NULL,	'y' },
	{ "ignore-file-name-case",	no_argument,		NULL,	OPT_IGN_FN_CASE },
	{ "help",			no_argument,		NULL,	OPT_HELP},
	{ "horizon-lines",		required_argument,	NULL,	OPT_HORIZON_LINES },
	{ "no-dereference",		no_argument,		NULL,	OPT_NO_DEREFERENCE},
	{ "no-ignore-file-name-case",	no_argument,		NULL,	OPT_NO_IGN_FN_CASE },
	{ "normal",			no_argument,		NULL,	OPT_NORMAL },
	{ "strip-trailing-cr",		no_argument,		NULL,	OPT_STRIPCR },
	{ "tabsize",			required_argument,	NULL,	OPT_TSIZE },
	{ "changed-group-format",	required_argument,	NULL,	OPT_CHANGED_GROUP_FORMAT},
	{ "suppress-common-lines",	no_argument,		NULL,	OPT_SUPPRESS_COMMON },
	{ "color",			optional_argument,	NULL,	OPT_COLOR },
	{ "version",			no_argument,		NULL,	OPT_VERSION},
	{ NULL,				0,			0,	'\0'}
};

static void checked_regcomp(char const *, regex_t *);
static void usage(void) __dead2;
static void conflicting_format(void) __dead2;
static void push_excludes(char *);
static void push_ignore_pats(char *);
static void read_excludes_file(char *file);
static void set_argstr(char **, char **);
static char *splice(char *, char *);
static bool do_color(void);

int
main(int argc, char **argv)
{
	const char *errstr = NULL;
	char *ep, **oargv;
	long  l;
	int   ch, dflags, lastch, gotstdin, prevoptind, newarg;

	oargv = argv;
	gotstdin = 0;
	dflags = 0;
	lastch = '\0';
	prevoptind = 1;
	newarg = 1;
	diff_context = 3;
	diff_format = D_UNSET;
#define	FORMAT_MISMATCHED(type)	\
	(diff_format != D_UNSET && diff_format != (type))
	while ((ch = getopt_long(argc, argv, OPTIONS, longopts, NULL)) != -1) {
		switch (ch) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			if (newarg)
				usage();	/* disallow -[0-9]+ */
			else if (lastch == 'c' || lastch == 'u')
				diff_context = 0;
			else if (!isdigit(lastch) || diff_context > INT_MAX / 10)
				usage();
			diff_context = (diff_context * 10) + (ch - '0');
			break;
		case 'a':
			dflags |= D_FORCEASCII;
			break;
		case 'b':
			dflags |= D_FOLDBLANKS;
			break;
		case 'C':
		case 'c':
			if (FORMAT_MISMATCHED(D_CONTEXT))
				conflicting_format();
			cflag = true;
			diff_format = D_CONTEXT;
			if (optarg != NULL) {
				l = strtol(optarg, &ep, 10);
				if (*ep != '\0' || l < 0 || l >= INT_MAX)
					usage();
				diff_context = (int)l;
			}
			break;
		case 'd':
			dflags |= D_MINIMAL;
			break;
		case 'D':
			if (FORMAT_MISMATCHED(D_IFDEF))
				conflicting_format();
			diff_format = D_IFDEF;
			ifdefname = optarg;
			break;
		case 'e':
			if (FORMAT_MISMATCHED(D_EDIT))
				conflicting_format();
			diff_format = D_EDIT;
			break;
		case 'f':
			if (FORMAT_MISMATCHED(D_REVERSE))
				conflicting_format();
			diff_format = D_REVERSE;
			break;
		case 'H':
			/* ignore but needed for compatibility with GNU diff */
			break;
		case 'h':
			/* silently ignore for backwards compatibility */
			break;
		case 'B':
			dflags |= D_SKIPBLANKLINES;
			break;
		case 'F':
			if (dflags & D_PROTOTYPE)
				conflicting_format();
			dflags |= D_MATCHLAST;
			most_recent_pat = xstrdup(optarg);
			break;
		case 'I':
			push_ignore_pats(optarg);
			break;
		case 'i':
			dflags |= D_IGNORECASE;
			break;
		case 'L':
			if (label[0] == NULL)
				label[0] = optarg;
			else if (label[1] == NULL)
				label[1] = optarg;
			else
				usage();
			break;
		case 'l':
			lflag = true;
			break;
		case 'N':
			Nflag = true;
			break;
		case 'n':
			if (FORMAT_MISMATCHED(D_NREVERSE))
				conflicting_format();
			diff_format = D_NREVERSE;
			break;
		case 'p':
			if (dflags & D_MATCHLAST)
				conflicting_format();
			dflags |= D_PROTOTYPE;
			break;
		case 'P':
			Pflag = true;
			break;
		case 'r':
			rflag = true;
			break;
		case 'q':
			if (FORMAT_MISMATCHED(D_BRIEF))
				conflicting_format();
			diff_format = D_BRIEF;
			break;
		case 'S':
			start = optarg;
			break;
		case 's':
			sflag = true;
			break;
		case 'T':
			Tflag = true;
			break;
		case 't':
			dflags |= D_EXPANDTABS;
			break;
		case 'U':
		case 'u':
			if (FORMAT_MISMATCHED(D_UNIFIED))
				conflicting_format();
			diff_format = D_UNIFIED;
			if (optarg != NULL) {
				l = strtol(optarg, &ep, 10);
				if (*ep != '\0' || l < 0 || l >= INT_MAX)
					usage();
				diff_context = (int)l;
			}
			break;
		case 'w':
			dflags |= D_IGNOREBLANKS;
			break;
		case 'W':
			width = (int) strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr) {
				warnx("Invalid argument for width");
				usage();
			}
			break;
		case 'X':
			read_excludes_file(optarg);
			break;
		case 'x':
			push_excludes(optarg);
			break;
		case 'y':
			if (FORMAT_MISMATCHED(D_SIDEBYSIDE))
				conflicting_format();
			diff_format = D_SIDEBYSIDE;
			break;
		case OPT_CHANGED_GROUP_FORMAT:
			if (FORMAT_MISMATCHED(D_GFORMAT))
				conflicting_format();
			diff_format = D_GFORMAT;
			group_format = optarg;
			break;
		case OPT_HELP:
			help = true;
			usage();
			break;
		case OPT_HORIZON_LINES:
			break; /* XXX TODO for compatibility with GNU diff3 */
		case OPT_IGN_FN_CASE:
			ignore_file_case = true;
			break;
		case OPT_NO_IGN_FN_CASE:
			ignore_file_case = false;
			break;
		case OPT_NORMAL:
			if (FORMAT_MISMATCHED(D_NORMAL))
				conflicting_format();
			diff_format = D_NORMAL;
			break;
		case OPT_TSIZE:
			tabsize = (int) strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr) {
				warnx("Invalid argument for tabsize");
				usage();
			}
			break;
		case OPT_STRIPCR:
			dflags |= D_STRIPCR;
			break;
		case OPT_SUPPRESS_COMMON:
			suppress_common = 1;
			break;
		case OPT_COLOR:
			if (optarg == NULL || strncmp(optarg, "auto", 4) == 0)
				colorflag = COLORFLAG_AUTO;
			else if (strncmp(optarg, "always", 6) == 0)
				colorflag = COLORFLAG_ALWAYS;
			else if (strncmp(optarg, "never", 5) == 0)
				colorflag = COLORFLAG_NEVER;
			else
				errx(2, "unsupported --color value '%s' (must be always, auto, or never)",
					optarg);
			break;
		case OPT_NO_DEREFERENCE:
			rflag = true;
			noderef = true;
			break;
		case OPT_VERSION:
			printf("%s\n", diff_version);
			exit(0);
		default:
			usage();
			break;
		}
		lastch = ch;
		newarg = optind != prevoptind;
		prevoptind = optind;
	}
	if (diff_format == D_UNSET && (dflags & D_PROTOTYPE) != 0)
		diff_format = D_CONTEXT;
	if (diff_format == D_UNSET)
		diff_format = D_NORMAL;
	argc -= optind;
	argv += optind;

	if (do_color()) {
		char *p;
		const char *env;

		color = true;
		add_code = "32";
		del_code = "31";
		env = getenv("DIFFCOLORS");
		if (env != NULL && *env != '\0' && (p = strdup(env))) {
			add_code = p;
			strsep(&p, ":");
			if (p != NULL)
				del_code = p;
		}
	}

#ifdef __OpenBSD__
	if (pledge("stdio rpath tmppath", NULL) == -1)
		err(2, "pledge");
#endif

	/*
	 * Do sanity checks, fill in stb1 and stb2 and call the appropriate
	 * driver routine.  Both drivers use the contents of stb1 and stb2.
	 */
	if (argc != 2)
		usage();
	checked_regcomp(ignore_pats, &ignore_re);
	checked_regcomp(most_recent_pat, &most_recent_re);
	if (strcmp(argv[0], "-") == 0) {
		fstat(STDIN_FILENO, &stb1);
		gotstdin = 1;
	} else if (stat(argv[0], &stb1) != 0) {
		if (!Nflag || errno != ENOENT)
			err(2, "%s", argv[0]);
		dflags |= D_EMPTY1;
		memset(&stb1, 0, sizeof(struct stat));
	}

	if (strcmp(argv[1], "-") == 0) {
		fstat(STDIN_FILENO, &stb2);
		gotstdin = 1;
	} else if (stat(argv[1], &stb2) != 0) {
		if (!Nflag || errno != ENOENT)
			err(2, "%s", argv[1]);
		dflags |= D_EMPTY2;
		memset(&stb2, 0, sizeof(stb2));
		stb2.st_mode = stb1.st_mode;
	}

	if (dflags & D_EMPTY1 && dflags & D_EMPTY2){
		warn("%s", argv[0]);
		warn("%s", argv[1]);
		exit(2);
	}

	if (stb1.st_mode == 0)
		stb1.st_mode = stb2.st_mode;

	if (gotstdin && (S_ISDIR(stb1.st_mode) || S_ISDIR(stb2.st_mode)))
		errx(2, "can't compare - to a directory");
	set_argstr(oargv, argv);
	if (S_ISDIR(stb1.st_mode) && S_ISDIR(stb2.st_mode)) {
		if (diff_format == D_IFDEF)
			errx(2, "-D option not supported with directories");
		diffdir(argv[0], argv[1], dflags);
	} else {
		if (S_ISDIR(stb1.st_mode)) {
			argv[0] = splice(argv[0], argv[1]);
			if (stat(argv[0], &stb1) == -1)
				err(2, "%s", argv[0]);
		}
		if (S_ISDIR(stb2.st_mode)) {
			argv[1] = splice(argv[1], argv[0]);
			if (stat(argv[1], &stb2) == -1)
				err(2, "%s", argv[1]);
		}
		print_status(diffreg(argv[0], argv[1], dflags, 1), argv[0],
		    argv[1], "");
	}
	exit(status);
}

static void
checked_regcomp(char const *pattern, regex_t *comp)
{
	char buf[BUFSIZ];
	int error;

	if (pattern == NULL)
		return;

	error = regcomp(comp, pattern, REG_NEWLINE | REG_EXTENDED);
	if (error != 0) {
		regerror(error, comp, buf, sizeof(buf));
		if (*pattern != '\0')
			errx(2, "%s: %s", pattern, buf);
		else
			errx(2, "%s", buf);
	}
}

static void
set_argstr(char **av, char **ave)
{
	size_t argsize;
	char **ap;

	argsize = 4 + *ave - *av + 1;
	diffargs = xmalloc(argsize);
	strlcpy(diffargs, "diff", argsize);
	for (ap = av + 1; ap < ave; ap++) {
		if (strcmp(*ap, "--") != 0) {
			strlcat(diffargs, " ", argsize);
			strlcat(diffargs, *ap, argsize);
		}
	}
}

/*
 * Read in an excludes file and push each line.
 */
static void
read_excludes_file(char *file)
{
	FILE *fp;
	char *buf, *pattern;
	size_t len;

	if (strcmp(file, "-") == 0)
		fp = stdin;
	else if ((fp = fopen(file, "r")) == NULL)
		err(2, "%s", file);
	while ((buf = fgetln(fp, &len)) != NULL) {
		if (buf[len - 1] == '\n')
			len--;
		if ((pattern = strndup(buf, len)) == NULL)
			err(2, "xstrndup");
		push_excludes(pattern);
	}
	if (strcmp(file, "-") != 0)
		fclose(fp);
}

/*
 * Push a pattern onto the excludes list.
 */
static void
push_excludes(char *pattern)
{
	struct excludes *entry;

	entry = xmalloc(sizeof(*entry));
	entry->pattern = pattern;
	entry->next = excludes_list;
	excludes_list = entry;
}

static void
push_ignore_pats(char *pattern)
{
	size_t len;

	if (ignore_pats == NULL)
		ignore_pats = xstrdup(pattern);
	else {
		/* old + "|" + new + NUL */
		len = strlen(ignore_pats) + strlen(pattern) + 2;
		ignore_pats = xreallocarray(ignore_pats, 1, len);
		strlcat(ignore_pats, "|", len);
		strlcat(ignore_pats, pattern, len);
	}
}

void
print_status(int val, char *path1, char *path2, const char *entry)
{
	if (label[0] != NULL)
		path1 = label[0];
	if (label[1] != NULL)
		path2 = label[1];

	switch (val) {
	case D_BINARY:
		printf("Binary files %s%s and %s%s differ\n",
		    path1, entry, path2, entry);
		break;
	case D_DIFFER:
		if (diff_format == D_BRIEF)
			printf("Files %s%s and %s%s differ\n",
			    path1, entry, path2, entry);
		break;
	case D_SAME:
		if (sflag)
			printf("Files %s%s and %s%s are identical\n",
			    path1, entry, path2, entry);
		break;
	case D_MISMATCH1:
		printf("File %s%s is a directory while file %s%s is a regular file\n",
		    path1, entry, path2, entry);
		break;
	case D_MISMATCH2:
		printf("File %s%s is a regular file while file %s%s is a directory\n",
		    path1, entry, path2, entry);
		break;
	case D_SKIPPED1:
		printf("File %s%s is not a regular file or directory and was skipped\n",
		    path1, entry);
		break;
	case D_SKIPPED2:
		printf("File %s%s is not a regular file or directory and was skipped\n",
		    path2, entry);
		break;
	case D_ERROR:
		break;
	}
}

static void
usage(void)
{
	(void)fprintf(help ? stdout : stderr,
	    "usage: diff [-aBbdilpTtw] [-c | -e | -f | -n | -q | -u] [--ignore-case]\n"
	    "            [--no-ignore-case] [--normal] [--strip-trailing-cr] [--tabsize]\n"
	    "            [-I pattern] [-F pattern] [-L label] file1 file2\n"
	    "       diff [-aBbdilpTtw] [-I pattern] [-L label] [--ignore-case]\n"
	    "            [--no-ignore-case] [--normal] [--strip-trailing-cr] [--tabsize]\n"
	    "            [-F pattern] -C number file1 file2\n"
	    "       diff [-aBbdiltw] [-I pattern] [--ignore-case] [--no-ignore-case]\n"
	    "            [--normal] [--strip-trailing-cr] [--tabsize] -D string file1 file2\n"
	    "       diff [-aBbdilpTtw] [-I pattern] [-L label] [--ignore-case]\n"
	    "            [--no-ignore-case] [--normal] [--tabsize] [--strip-trailing-cr]\n"
	    "            [-F pattern] -U number file1 file2\n"
	    "       diff [-aBbdilNPprsTtw] [-c | -e | -f | -n | -q | -u] [--ignore-case]\n"
	    "            [--no-ignore-case] [--normal] [--tabsize] [-I pattern] [-L label]\n"
	    "            [-F pattern] [-S name] [-X file] [-x pattern] dir1 dir2\n"
	    "       diff [-aBbditwW] [--expand-tabs] [--ignore-all-blanks]\n"
	    "            [--ignore-blank-lines] [--ignore-case] [--minimal]\n"
	    "            [--no-ignore-file-name-case] [--strip-trailing-cr]\n"
	    "            [--suppress-common-lines] [--tabsize] [--text] [--width]\n"
	    "            -y | --side-by-side file1 file2\n"
	    "       diff [--help] [--version]\n");

	if (help)
		exit(0);
	else
		exit(2);
}

static void
conflicting_format(void)
{

	fprintf(stderr, "error: conflicting output format options.\n");
	usage();
}

static bool
do_color(void)
{
	const char *p, *p2;

	switch (colorflag) {
	case COLORFLAG_AUTO:
		p = getenv("CLICOLOR");
		p2 = getenv("COLORTERM");
		if ((p != NULL && *p != '\0') || (p2 != NULL && *p2 != '\0'))
			return isatty(STDOUT_FILENO);
		break;
	case COLORFLAG_ALWAYS:
		return (true);
	case COLORFLAG_NEVER:
		return (false);
	}

	return (false);
}

static char *
splice(char *dir, char *path)
{
	char *tail, *buf;
	size_t dirlen;

	dirlen = strlen(dir);
	while (dirlen != 0 && dir[dirlen - 1] == '/')
	    dirlen--;
	if ((tail = strrchr(path, '/')) == NULL)
		tail = path;
	else
		tail++;
	xasprintf(&buf, "%.*s/%s", (int)dirlen, dir, tail);
	return (buf);
}
