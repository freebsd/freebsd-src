/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Target keyword selection: unique-prefix match, the rc pass-through,
 * and the poudriere target (sysrc collection or native make overlays).
 */

#include "sysconf_priv.h"

/*
 * Unified target keywords: libbsdconf formats plus pass-throughs. Exact
 * match wins; otherwise a unique prefix is accepted. Keep alphabetical so
 * ambiguous-match lists read naturally (e.g. `s' -> src, sysctl).
 */
static const char *const target_keywords[] = {
	"generic",
	"loader",
	"make",
	"poudriere",
	"rc",
	"src",
	"sysctl",
	NULL
};

/*
 * Map `token' to a canonical keyword from `keywords'. Exact match wins;
 * otherwise a unique prefix is accepted. Ambiguous prefixes print the
 * matches and call usage(). Unknown tokens return NULL.
 */
const char *
resolve_keyword(const char *token, const char *const *keywords,
    const char *what)
{
	char matches[128];
	const char *found;
	size_t off;
	size_t tlen;
	unsigned int n;
	unsigned int nmatch;

	if (token == NULL || *token == '\0')
		return (NULL);
	tlen = strlen(token);
	for (n = 0; keywords[n] != NULL; n++)
		if (strcmp(keywords[n], token) == 0)
			return (keywords[n]);
	found = NULL;
	nmatch = 0;
	off = 0;
	matches[0] = '\0';
	for (n = 0; keywords[n] != NULL; n++) {
		if (strncmp(keywords[n], token, tlen) != 0)
			continue;
		nmatch++;
		found = keywords[n];
		if (off > 0 && off < sizeof(matches))
			off += (size_t)snprintf(matches + off,
			    sizeof(matches) - off, ", ");
		if (off < sizeof(matches))
			off += (size_t)snprintf(matches + off,
			    sizeof(matches) - off, "%s",
			    keywords[n]);
	}
	if (nmatch == 1)
		return (found);
	if (nmatch > 1) {
		warnx("ambiguous %s `%s'; matches %s", what, token, matches);
		usage();
	}
	return (NULL);
}

static const char *
resolve_target_keyword(const char *token)
{

	return (resolve_keyword(token, target_keywords, "target"));
}

/*
 * Locate an operand in `argv' without consuming or validating anything:
 * the first argument that is neither an option cluster nor the argument
 * of one (per OPTSTRING; an unknown option letter is assumed to take no
 * argument). Used before any getopt(3) processing so that pass-through
 * targets (`rc', `poudriere'; see exec_sysrc() / poudriere_dispatch())
 * can be detected while the command line is still pristine.
 *
 * Attached arguments (`-fPATH', `-R/altroot') are self-contained: the rest
 * of the cluster is the option's argument, so the following argv element is
 * not consumed. Only a trailing option letter that takes an argument and
 * has nothing after it in the cluster (`-f PATH') skips the next element.
 *
 * Poudriere context long options (`--jail'/`--ports'/`--tree'/`--set')
 * take an argument and must not be mistaken for a keyword.
 *
 * `past_dd' is for the top-level target: `--' ends options and the next
 * word is the keyword (`sysconf -- loader'). The poudriere nested keyword
 * passes zero: `--' ends the stem, and the following word is a name
 * (`sysconf p -- src' reads the variable src from poudriere.conf).
 */
int
find_operand(int argc, char *argv[], int from, int past_dd)
{
	int n;
	const char *o;
	const char *p;

	for (n = from; n < argc; n++) {
		if (strcmp(argv[n], "--") == 0) {
			if (!past_dd)
				return (-1);
			return (n + 1 < argc ? n + 1 : -1);
		}
		if (argv[n][0] == '-' && argv[n][1] != '\0') {
			if (argv[n][1] == '-') {
				if (poudriere_longopt_p(argv[n]) &&
				    strchr(argv[n], '=') == NULL &&
				    n + 1 < argc)
					n++;
				continue;
			}
			for (p = argv[n] + 1; *p != '\0'; p++) {
				o = strchr(OPTSTRING, *p);
				if (o != NULL && o[1] == ':') {
					/* Takes an argument */
					if (p[1] != '\0') {
						;
						/* attached: rest of argv[n] */
					} else if (n + 1 < argc) {
						n++;
						/* separate argv */
					}
					break;
				}
			}
			continue;
		}
		return (n);
	}

	return (-1);
}

static int
find_target(int argc, char *argv[])
{

	return (find_operand(argc, argv, 1, 1));
}

/*
 * The `rc' pass-through: rc.conf(5) is sysrc(8)'s domain, and its feature
 * set is a superset of ours, so no option or argument is validated,
 * interpreted, or reordered here. Every argument except the target
 * keyword itself (at argv index `skip') is handed to sysrc(8) verbatim.
 * Never returns.
 */
static void
exec_sysrc(int argc, char *argv[], int skip)
{
	int n;
	int nargc = 0;
	char **nargv;

	if ((nargv = calloc((size_t)argc + 1, sizeof(*nargv))) == NULL)
		err(EXIT_FAILURE, NULL);
	nargv[nargc++] = (char *)(uintptr_t)"sysrc";
	for (n = 1; n < argc; n++) {
		if (n == skip)
			continue;
		nargv[nargc++] = argv[n];
	}
	nargv[nargc] = NULL;

	execvp("sysrc", nargv);
	err(EXIT_FAILURE, "sysrc");
}

/*
 * Resolve the target keyword. The `rc' target execs sysrc(8) and does
 * not return. The `poudriere' target either execs sysrc(8) or compacts
 * argv for a native make/src/src-env overlay (`*argcp' is updated) and
 * returns NULL so main does not treat it as a libbsdconf keyword.
 * Unique-prefix shorthands are expanded (`r', `m', `p'). Returns the
 * canonical keyword, or NULL when the token is unknown, absent, or
 * handled as a poudriere overlay.
 */
const char *
target_resolve(int *argcp, char *argv[])
{
	const char *resolved;
	int n;

	n = find_target(*argcp, argv);
	if (n < 0)
		return (NULL);
	resolved = resolve_target_keyword(argv[n]);
	if (resolved != NULL && strcmp(resolved, "rc") == 0)
		exec_sysrc(*argcp, argv, n); /* never returns */
	if (resolved != NULL && strcmp(resolved, "poudriere") == 0) {
		*argcp = poudriere_dispatch(*argcp, argv, n);
		return (NULL);
	}
	return (resolved);
}
