/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * The pkg target: the collection of files that influence pkg(8), as a
 * derivative of the ucl target. The syntax is libucl's (see
 * sysconf_ucl.c); this file is policy only: the paths, the sourcing
 * order, the read-only base repository files, and where a write lands.
 *
 * Sourcing order matches pkg's defaults (REPOS_DIR is not re-read):
 *   each .conf file in /etc/pkg
 *   /usr/local/etc/pkg.conf
 *   each .conf file in /usr/local/etc/pkg/repos
 * Files under /etc/pkg are the base repository definitions. They are
 * consulted, and -F names them, but a write is never applied there: a
 * repository key (a dotted path, the repository tag plus the inner key)
 * is overridden in /usr/local/etc/pkg/repos/FreeBSD.conf, and a global
 * key is written to /usr/local/etc/pkg.conf. That is the split the
 * comment in /etc/pkg/FreeBSD.conf asks for.
 *
 * pkg(8) itself offers no setter for these files: `pkg config' reads one
 * merged value, and `pkg repositories enable' records a side override
 * under the package database rather than editing a repository file.
 * That switch stays pkg's own; this target never writes repos_state.
 */

#include "sysconf_priv.h"

static const struct bsdconf_source pkg_sources[] = {
	{ BSDCONF_SOURCE_DIR,	"/etc/pkg" },
	{ BSDCONF_SOURCE_FILE,	"/usr/local/etc/pkg.conf" },
	{ BSDCONF_SOURCE_DIR,	"/usr/local/etc/pkg/repos" },
	{ BSDCONF_SOURCE_FILE,	NULL },
};

#define PKG_CONF_REL	"/usr/local/etc/pkg.conf"
#define PKG_REPOS_REL	"/usr/local/etc/pkg/repos/FreeBSD.conf"
#define PKG_REPOS_DIR	"/usr/local/etc/pkg/repos/"
#define PKG_BASE_DIR	"/etc/pkg/"

enum bsdconf_format	pkg_format;

/*
 * Register pkg as a derivative of the (already registered) ucl target.
 * Called from ucl_register(); idempotent.
 */
int
pkg_register(void)
{
	static bool done;
	struct bsdconf_format_def def;

	if (done)
		return (0);
	if (bsdconf_format_derive(ucl_format, &def) != 0)
		return (-1);
	def.keyword = "pkg";
	def.path = PKG_REPOS_REL;
	def.sources = pkg_sources;
	if (bsdconf_format_register(&def, &pkg_format) != 0)
		return (-1);
	done = true;
	return (0);
}

static const char *
rel_of(const char *path)
{
	size_t n;

	n = strlen(rootdir);
	if (n > 0 && strncmp(path, rootdir, n) == 0)
		return (path + n);
	return (path);
}

static char *
join_root(const char *rel)
{
	char *path;
	char buf[PATH_MAX];

	if (snprintf(buf, sizeof(buf), "%s%s", rootdir, rel) >=
	    (int)sizeof(buf))
		errx(EXIT_FAILURE, "%s%s: %s", rootdir, rel,
		    strerror(ENAMETOOLONG));
	if ((path = strdup(buf)) == NULL)
		err(EXIT_FAILURE, NULL);
	return (path);
}

static int
index_of(const char *path)
{
	size_t n;

	for (n = 0; n < nconf_files; n++) {
		if (strcmp(conf_files[n], path) == 0)
			return ((int)n);
	}
	return (-1);
}

static void
insert_at(size_t at, char *path)
{
	char **tmp;
	size_t n;

	tmp = reallocarray(conf_files, nconf_files + 1, sizeof(*tmp));
	if (tmp == NULL)
		err(EXIT_FAILURE, NULL);
	conf_files = tmp;
	for (n = nconf_files; n > at; n--)
		conf_files[n] = conf_files[n - 1];
	conf_files[at] = path;
	nconf_files++;
}

void
pkg_prepare(void)
{
	size_t at;
	size_t n;
	int idx;
	char *conf;
	char *repos;
	const char *rel;

	if (format != pkg_format)
		return;

	conf = join_root(PKG_CONF_REL);
	idx = index_of(conf);
	free(conf);
	if (idx < 0)
		errx(EXIT_FAILURE, "pkg.conf missing from file list");
	pkg_conf_idx = (size_t)idx;
	write_idx = pkg_conf_idx;

	repos = join_root(PKG_REPOS_REL);
	idx = index_of(repos);
	if (idx < 0) {
		at = nconf_files;
		for (n = 0; n < nconf_files; n++) {
			rel = rel_of(conf_files[n]);
			if (strncmp(rel, PKG_REPOS_DIR,
			    sizeof(PKG_REPOS_DIR) - 1) != 0)
				continue;
			if (strcmp(repos, conf_files[n]) < 0) {
				at = n;
				break;
			}
			at = n + 1;
		}
		insert_at(at, repos);
		pkg_repos_idx = at;
		if (pkg_conf_idx >= at)
			pkg_conf_idx++;
		write_idx = pkg_conf_idx;
	} else {
		free(repos);
		pkg_repos_idx = (size_t)idx;
	}

	if ((conf_ro = calloc(nconf_files, sizeof(*conf_ro))) == NULL)
		err(EXIT_FAILURE, NULL);
	for (n = 0; n < nconf_files; n++) {
		rel = rel_of(conf_files[n]);
		if (strncmp(rel, PKG_BASE_DIR, sizeof(PKG_BASE_DIR) - 1) == 0)
			conf_ro[n] = 1;
	}
	pkg_target = true;
}
