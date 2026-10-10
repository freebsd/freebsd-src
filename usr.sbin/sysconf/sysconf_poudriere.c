/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * poudriere(8) target: poudriere.conf via sysrc(8), and the make(1)
 * overlays (make.conf, src.conf, src-env.conf) under poudriere.d.
 */

#include "sysconf_priv.h"

#define POUDRIERE_ETC_DEF	"/usr/local/etc"
#define POUDRIERE_MAXFILES	16

/*
 * Nested poudriere targets (make(1) overlay files under poudriere.d).
 * Exact match wins; otherwise a unique prefix. `s' and `sr' are
 * ambiguous (src vs src-env); `m' uniquely selects make.
 */
static const char *const poudriere_subs[] = {
	"make",
	"src",
	"src-env",
	NULL
};

/* Poudriere context; filled when the poudriere target is selected. */
static const char *poudriere_sub = NULL;	/* make, src, src-env */
static const char *poudriere_jail = NULL;
static const char *poudriere_tree = NULL;	/* --ports or --tree */
static const char *poudriere_set = NULL;

/*
 * True when `arg' is `--name' or `--name=value'.
 */
static int
longopt_eq(const char *arg, const char *name)
{
	size_t nlen;

	if (arg[0] != '-' || arg[1] != '-')
		return (0);
	nlen = strlen(name);
	if (strncmp(arg + 2, name, nlen) != 0)
		return (0);
	return (arg[2 + nlen] == '\0' || arg[2 + nlen] == '=');
}

int
poudriere_longopt_p(const char *arg)
{

	return (longopt_eq(arg, "jail") || longopt_eq(arg, "ports") ||
	    longopt_eq(arg, "tree") || longopt_eq(arg, "set"));
}

static void
poudriere_add_path(char **files, unsigned int *nfiles, unsigned int max,
    const char *path)
{

	if (*nfiles >= max)
		errx(EXIT_FAILURE, "too many poudriere files");
	if ((files[*nfiles] = strdup(path)) == NULL)
		err(EXIT_FAILURE, NULL);
	(*nfiles)++;
}

static void
poudriere_add_if_readable(char **files, unsigned int *nfiles,
    unsigned int max, const char *path)
{

	if (access(path, R_OK) != 0)
		return;
	poudriere_add_path(files, nfiles, max, path);
}

static int
same_realpath(const char *a, const char *b)
{
	char ra[PATH_MAX];
	char rb[PATH_MAX];

	if (realpath(a, ra) == NULL || realpath(b, rb) == NULL)
		return (0);
	return (strcmp(ra, rb) == 0);
}

/*
 * Join `dir'/`name', then prefix `-R dir' when set. `rootdir' is empty
 * until parse_options(), so the sysrc pass-through is unprefixed.
 */
static void
poudriere_join(char *dst, size_t dstsz, const char *dir, const char *name)
{
	char raw[PATH_MAX];
	int n;

	n = snprintf(raw, sizeof(raw), "%s/%s", dir, name);
	if (n < 0 || (size_t)n >= sizeof(raw))
		errx(EXIT_FAILURE, "%s/%s: %s", dir, name,
		    strerror(ENAMETOOLONG));
	n = snprintf(dst, dstsz, "%s%s", rootdir, raw);
	if (n < 0 || (size_t)n >= dstsz)
		errx(EXIT_FAILURE, "%s%s: %s", rootdir, raw,
		    strerror(ENAMETOOLONG));
}

static void
poudriere_etc_d(const char **etc, const char **d, char *dbuf, size_t dbufsz)
{

	*etc = getenv("POUDRIERE_ETC");
	if (*etc == NULL || **etc == '\0')
		*etc = POUDRIERE_ETC_DEF;
	*d = getenv("POUDRIERED");
	if (*d == NULL || **d == '\0') {
		if (snprintf(dbuf, dbufsz, "%s/poudriere.d", *etc) >=
		    (int)dbufsz)
			errx(EXIT_FAILURE, "%s/poudriere.d: %s", *etc,
			    strerror(ENAMETOOLONG));
		*d = dbuf;
	}
}

static void
poudriere_try_named(char **files, unsigned int *nfiles, unsigned int max,
    const char *dir, const char *stem, const char *suffix, int only_readable)
{
	char name[PATH_MAX];
	char path[PATH_MAX];
	int n;

	if (stem == NULL || *stem == '\0')
		return;
	n = snprintf(name, sizeof(name), "%s-%s", stem, suffix);
	if (n < 0 || (size_t)n >= sizeof(name))
		errx(EXIT_FAILURE, "poudriere overlay name: %s",
		    strerror(ENAMETOOLONG));
	poudriere_join(path, sizeof(path), dir, name);
	if (only_readable)
		poudriere_add_if_readable(files, nfiles, max, path);
	else
		poudriere_add_path(files, nfiles, max, path);
}

/*
 * Overlay stems in poudriere(8) construction order. `full' is the
 * make.conf / poudriere.conf set (set, tree, jail, and the four combos).
 * src.conf and src-env.conf are global plus set and jail only.
 */
static void
poudriere_add_overlays(char **files, unsigned int *nfiles, unsigned int max,
    const char *dir, const char *suffix, int full, int only_readable)
{
	char stem[PATH_MAX];
	int n;

	poudriere_try_named(files, nfiles, max, dir, poudriere_set, suffix,
	    only_readable);
	if (full)
		poudriere_try_named(files, nfiles, max, dir, poudriere_tree,
		    suffix, only_readable);
	poudriere_try_named(files, nfiles, max, dir, poudriere_jail, suffix,
	    only_readable);
	if (!full)
		return;
	if (poudriere_tree != NULL && *poudriere_tree != '\0' &&
	    poudriere_set != NULL && *poudriere_set != '\0') {
		n = snprintf(stem, sizeof(stem), "%s-%s", poudriere_tree,
		    poudriere_set);
		if (n < 0 || (size_t)n >= sizeof(stem))
			errx(EXIT_FAILURE, "poudriere overlay name: %s",
			    strerror(ENAMETOOLONG));
		poudriere_try_named(files, nfiles, max, dir, stem, suffix,
		    only_readable);
	}
	if (poudriere_jail != NULL && *poudriere_jail != '\0' &&
	    poudriere_tree != NULL && *poudriere_tree != '\0') {
		n = snprintf(stem, sizeof(stem), "%s-%s", poudriere_jail,
		    poudriere_tree);
		if (n < 0 || (size_t)n >= sizeof(stem))
			errx(EXIT_FAILURE, "poudriere overlay name: %s",
			    strerror(ENAMETOOLONG));
		poudriere_try_named(files, nfiles, max, dir, stem, suffix,
		    only_readable);
	}
	if (poudriere_jail != NULL && *poudriere_jail != '\0' &&
	    poudriere_set != NULL && *poudriere_set != '\0') {
		n = snprintf(stem, sizeof(stem), "%s-%s", poudriere_jail,
		    poudriere_set);
		if (n < 0 || (size_t)n >= sizeof(stem))
			errx(EXIT_FAILURE, "poudriere overlay name: %s",
			    strerror(ENAMETOOLONG));
		poudriere_try_named(files, nfiles, max, dir, stem, suffix,
		    only_readable);
	}
	if (poudriere_jail != NULL && *poudriere_jail != '\0' &&
	    poudriere_tree != NULL && *poudriere_tree != '\0' &&
	    poudriere_set != NULL && *poudriere_set != '\0') {
		n = snprintf(stem, sizeof(stem), "%s-%s-%s", poudriere_jail,
		    poudriere_tree, poudriere_set);
		if (n < 0 || (size_t)n >= sizeof(stem))
			errx(EXIT_FAILURE, "poudriere overlay name: %s",
			    strerror(ENAMETOOLONG));
		poudriere_try_named(files, nfiles, max, dir, stem, suffix,
		    only_readable);
	}
}

/*
 * Build the poudriere.conf collection in include_poudriere_confs() order:
 * POUDRIERE_ETC/poudriere.conf, then POUDRIERED/poudriere.conf if distinct,
 * then optional set/tree/jail overlays. At least one global file must exist.
 */
static void
poudriere_collect_files(char **files, unsigned int *nfiles, unsigned int max,
    int only_readable)
{
	char dbuf[PATH_MAX];
	char path[PATH_MAX];
	const char *d;
	const char *etc;

	poudriere_etc_d(&etc, &d, dbuf, sizeof(dbuf));

	poudriere_join(path, sizeof(path), etc, "poudriere.conf");
	if (only_readable)
		poudriere_add_if_readable(files, nfiles, max, path);
	else
		poudriere_add_path(files, nfiles, max, path);

	poudriere_join(path, sizeof(path), d, "poudriere.conf");
	if (*nfiles == 0 || !same_realpath(files[0], path)) {
		if (only_readable)
			poudriere_add_if_readable(files, nfiles, max, path);
		else
			poudriere_add_path(files, nfiles, max, path);
	}

	if (only_readable && *nfiles == 0)
		errx(EXIT_FAILURE,
		    "Unable to find a readable poudriere.conf in %s or %s",
		    etc, d);

	poudriere_add_overlays(files, nfiles, max, d, "poudriere.conf", 1,
	    only_readable);
}

/*
 * make.conf / src.conf / src-env.conf under POUDRIERED. The global file
 * is always listed (ENOENT is tolerated on read; a write creates it).
 * Overlays are appended only when readable, unless `only_readable' is 0
 * (`-L' lists every stem, including files that do not exist yet).
 * Plugin hook make.conf is not collected.
 */
static void
poudriere_collect_make_files(char **files, unsigned int *nfiles,
    unsigned int max, const char *suffix, int full, int only_readable)
{
	char dbuf[PATH_MAX];
	char path[PATH_MAX];
	const char *d;
	const char *etc;

	poudriere_etc_d(&etc, &d, dbuf, sizeof(dbuf));
	poudriere_join(path, sizeof(path), d, suffix);
	poudriere_add_path(files, nfiles, max, path);
	poudriere_add_overlays(files, nfiles, max, d, suffix, full,
	    only_readable);
}

/*
 * True when remaining argv (except the target at `skip') contains sysrc(8)
 * `-f'. Clusters are scanned like getopt: `f' takes an argument, as do
 * `j', `R', and `s' (so `-qf FILE' counts and `-F' does not).
 */
static int
argv_has_sysrc_f(int argc, char *argv[], int skip)
{
	const char *p;
	int n;

	for (n = 1; n < argc; n++) {
		if (n == skip)
			continue;
		if (strcmp(argv[n], "--") == 0)
			return (0);
		if (argv[n][0] != '-' || argv[n][1] == '\0' ||
		    argv[n][1] == '-')
			continue;
		for (p = argv[n] + 1; *p != '\0'; p++) {
			if (*p == 'f')
				return (1);
			if (*p == 'j' || *p == 'R' || *p == 's') {
				if (p[1] != '\0')
					break;
				if (n + 1 < argc)
					n++;
				break;
			}
		}
	}
	return (0);
}

/*
 * If `arg' is `--name' or `--name=value', return the value. Separate-argv
 * form consumes `next' (*consumed = 1). Missing or empty values are errors.
 */
static const char *
longopt_arg(const char *arg, const char *next, int *consumed,
    const char *name)
{
	size_t nlen;

	*consumed = 0;
	if (!longopt_eq(arg, name))
		return (NULL);
	nlen = strlen(name);
	if (arg[2 + nlen] == '=') {
		if (arg[3 + nlen] == '\0')
			errx(EXIT_FAILURE,
			    "option `--%s' requires an argument", name);
		return (arg + 3 + nlen);
	}
	if (next == NULL || *next == '\0')
		errx(EXIT_FAILURE, "option `--%s' requires an argument",
		    name);
	*consumed = 1;
	return (next);
}

static void
poudriere_parse_longopts(int argc, char *argv[])
{
	const char *next;
	const char *val;
	int consumed;
	int n;

	for (n = 1; n < argc; n++) {
		if (strcmp(argv[n], "--") == 0)
			break;
		consumed = 0;
		next = (n + 1 < argc) ? argv[n + 1] : NULL;
		if ((val = longopt_arg(argv[n], next, &consumed,
		    "jail")) != NULL)
			poudriere_jail = val;
		else if ((val = longopt_arg(argv[n], next, &consumed,
		    "ports")) != NULL)
			poudriere_tree = val;
		else if ((val = longopt_arg(argv[n], next, &consumed,
		    "tree")) != NULL)
			poudriere_tree = val;
		else if ((val = longopt_arg(argv[n], next, &consumed,
		    "set")) != NULL)
			poudriere_set = val;
		if (consumed)
			n++;
	}
}

/*
 * Drop the poudriere keyword, optional nested target, and context long
 * options so the remainder can be parsed as a native sysconf command
 * line (or, for the sysrc path, rebuilt without those tokens).
 */
static int
poudriere_compact(int argc, char *argv[], int tgt, int sub)
{
	int dst;
	int endopts;
	int n;

	dst = 1;
	endopts = 0;
	for (n = 1; n < argc; n++) {
		if (n == tgt || n == sub)
			continue;
		if (!endopts && strcmp(argv[n], "--") == 0)
			endopts = 1;
		if (!endopts && poudriere_longopt_p(argv[n])) {
			if (strchr(argv[n], '=') == NULL && n + 1 < argc)
				n++;
			continue;
		}
		argv[dst++] = argv[n];
	}
	argv[dst] = NULL;
	return (dst);
}

/*
 * `-l' / `-L' on this target. Returns 1 when either is present.
 * `*all' is `-L' (every location for the named jail, tree, and set,
 * including files that do not exist). `*exist' is `-E'. `*filep' is an
 * explicit `-f'. `*names' is a non-option operand (listing takes none).
 * `-R dir' is applied as the sysconf root for the paths printed.
 */
static int
argv_wants_list(int argc, char *argv[], int skip, int *all, int *exist,
    const char **filep, int *names)
{
	const char *arg;
	const char *p;
	int list;
	int n;

	*all = 0;
	*exist = 0;
	*filep = NULL;
	*names = 0;
	list = 0;
	for (n = 1; n < argc; n++) {
		if (n == skip)
			continue;
		if (strcmp(argv[n], "--") == 0) {
			if (n + 1 < argc)
				*names = 1;
			break;
		}
		if (argv[n][0] == '-' && argv[n][1] == '-') {
			if (poudriere_longopt_p(argv[n]) &&
			    strchr(argv[n], '=') == NULL && n + 1 < argc)
				n++;
			continue;
		}
		if (argv[n][0] != '-' || argv[n][1] == '\0') {
			*names = 1;
			continue;
		}
		for (p = argv[n] + 1; *p != '\0'; p++) {
			if (*p == 'l')
				list = 1;
			else if (*p == 'L') {
				list = 1;
				*all = 1;
			} else if (*p == 'E')
				*exist = 1;
			else if (*p == 'f' || *p == 'j' || *p == 'R' ||
			    *p == 's') {
				if (p[1] != '\0')
					arg = p + 1;
				else if (n + 1 < argc)
					arg = argv[++n];
				else
					arg = NULL;
				if (*p == 'f')
					*filep = arg;
				else if (*p == 'R' && arg != NULL)
					rootdir = arg;
				break;
			}
		}
	}
	return (list);
}

static void
poudriere_puts_files(char **files, unsigned int nfiles, int exist_only)
{
	unsigned int n;

	for (n = 0; n < nfiles; n++) {
		if (exist_only && access(files[n], F_OK) != 0)
			continue;
		puts(files[n]);
	}
}

/*
 * The `poudriere' target without a nested make/src/src-env keyword:
 * resolve the shell-sourced poudriere.conf collection (same order as
 * poudriere's include_poudriere_confs()) and hand it to sysrc(8) as
 * `-f' files. `--jail'/`--ports'/`--tree'/`--set' select overlays;
 * they are stripped so sysrc never sees them. An explicit sysrc `-f'
 * replaces discovery. `-l' and `-L' are printed here, one path per
 * line. Never returns.
 */
static void
exec_poudriere_sysrc(int argc, char *argv[], int skip)
{
	char *files[POUDRIERE_MAXFILES];
	char **nargv;
	char path[PATH_MAX];
	const char *list_file;
	unsigned int nfiles;
	int all;
	int endopts;
	int exist;
	int n;
	int names;
	int nargc;
	int user_f;

	nfiles = 0;
	nargc = 0;
	if (argv_wants_list(argc, argv, skip, &all, &exist, &list_file,
	    &names)) {
		if (names)
			errx(EXIT_FAILURE, "-l/-L take no names");
		if (list_file != NULL) {
			if (snprintf(path, sizeof(path), "%s%s", rootdir,
			    list_file) >= (int)sizeof(path))
				errx(EXIT_FAILURE, "%s%s: %s", rootdir,
				    list_file, strerror(ENAMETOOLONG));
			if (!exist || access(path, F_OK) == 0)
				puts(path);
			exit(EXIT_SUCCESS);
		}
		poudriere_collect_files(files, &nfiles, POUDRIERE_MAXFILES,
		    all ? 0 : 1);
		poudriere_puts_files(files, nfiles, exist);
		exit(EXIT_SUCCESS);
	}
	user_f = argv_has_sysrc_f(argc, argv, skip);
	if (!user_f)
		poudriere_collect_files(files, &nfiles, POUDRIERE_MAXFILES,
		    1);

	if ((nargv = calloc((size_t)argc + (size_t)nfiles * 2 + 2,
	    sizeof(*nargv))) == NULL)
		err(EXIT_FAILURE, NULL);
	nargv[nargc++] = (char *)(uintptr_t)"sysrc";
	for (n = 0; n < (int)nfiles; n++) {
		nargv[nargc++] = (char *)(uintptr_t)"-f";
		nargv[nargc++] = files[n];
	}
	endopts = 0;
	for (n = 1; n < argc; n++) {
		if (n == skip)
			continue;
		if (!endopts && strcmp(argv[n], "--") == 0) {
			endopts = 1;
			nargv[nargc++] = argv[n];
			continue;
		}
		if (!endopts && poudriere_longopt_p(argv[n])) {
			if (strchr(argv[n], '=') == NULL && n + 1 < argc)
				n++;
			continue;
		}
		nargv[nargc++] = argv[n];
	}
	nargv[nargc] = NULL;

	execvp("sysrc", nargv);
	err(EXIT_FAILURE, "sysrc");
}

int
poudriere_native(void)
{

	return (poudriere_sub != NULL);
}

/*
 * Nested `poudriere make|src|src-env': native make(1) format over the
 * matching poudriere.d overlay collection. `-f' replaces discovery.
 */
void
poudriere_setup(void)
{
	char *files[POUDRIERE_MAXFILES];
	char path[PATH_MAX];
	const char *prefix;
	const char *suffix;
	int full;
	unsigned int i;
	unsigned int nfiles;

	format = BSDCONF_FORMAT_MAKE;
	if (file != NULL) {
		prefix = file_stdin ? "" : rootdir;
		if ((conf_files = calloc(1, sizeof(*conf_files))) == NULL)
			err(EXIT_FAILURE, NULL);
		if (snprintf(path, sizeof(path), "%s%s", prefix, file) >=
		    (int)sizeof(path))
			errx(EXIT_FAILURE, "%s%s: %s", prefix, file,
			    strerror(ENAMETOOLONG));
		if ((conf_files[0] = strdup(path)) == NULL)
			err(EXIT_FAILURE, NULL);
		nconf_files = 1;
		write_idx = 0;
		return;
	}

	if (strcmp(poudriere_sub, "make") == 0) {
		suffix = "make.conf";
		full = 1;
	} else if (strcmp(poudriere_sub, "src") == 0) {
		suffix = "src.conf";
		full = 0;
	} else {
		suffix = "src-env.conf";
		full = 0;
	}

	nfiles = 0;
	/* `-L' lists missing overlays; reads and `-l' do not. */
	poudriere_collect_make_files(files, &nfiles, POUDRIERE_MAXFILES,
	    suffix, full, list_all ? 0 : 1);
	if ((conf_files = calloc(nfiles, sizeof(*conf_files))) == NULL)
		err(EXIT_FAILURE, NULL);
	for (i = 0; i < nfiles; i++)
		conf_files[i] = files[i];
	nconf_files = nfiles;
	write_idx = 0;
}

/*
 * On the sysrc path, exec and do not return. On the native
 * make/src/src-env path, rewrite argv and return the new argc.
 * A bare `--' before the next word ends the nested keyword, so that
 * word is a poudriere.conf variable (`sysconf p -- src').
 */
int
poudriere_dispatch(int argc, char *argv[], int tgt)
{
	int subn;

	poudriere_parse_longopts(argc, argv);
	subn = find_operand(argc, argv, tgt + 1, 0);
	if (subn > 0 && strchr(argv[subn], '=') == NULL)
		poudriere_sub = resolve_keyword(argv[subn], poudriere_subs,
		    "poudriere target");
	if (poudriere_sub == NULL)
		exec_poudriere_sysrc(argc, argv, tgt); /* never returns */
	return (poudriere_compact(argc, argv, tgt, subn));
}

void
poudriere_usage(void)
{

	fprintf(stderr,
	    "       %s poudriere [--jail name] [--ports name] [--tree name]\n",
	    pgm);
	fprintf(stderr,
	    "               [--set name] [sysrc(8) argument ...]\n");
	fprintf(stderr,
	    "       %s poudriere {make|src|src-env} [-ceFinNqsvVx]\n", pgm);
	fprintf(stderr,
	    "               [--jail name] [--ports name] [--tree name]\n");
	fprintf(stderr,
	    "               [--set name] [-j jail | -R dir] [-f file]\n");
	fprintf(stderr,
	    "               name[[+|-]=value] ...\n");
	fprintf(stderr,
	    "       %s poudriere {make|src|src-env} [-eFnNqvV]\n", pgm);
	fprintf(stderr,
	    "               [--jail name] [--ports name] [--tree name]\n");
	fprintf(stderr,
	    "               [--set name] [-f file] -a\n");
	fprintf(stderr,
	    "       %s poudriere {make|src|src-env} [-E]\n", pgm);
	fprintf(stderr,
	    "               [--jail name] [--ports name] [--tree name]\n");
	fprintf(stderr,
	    "               [--set name] [-f file] -l | -L\n");
}

void
poudriere_help(void)
{

	fprintf(stderr, "\t%-9s %s\n", "poudriere",
	    "poudriere.conf via sysrc(8); make/src/src-env overlays");
	fprintf(stderr, "\t%-9s %s\n", "",
	    "(--jail/--ports/--tree/--set select poudriere.d files)");
}
