/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Backing-file discovery and file-list reporting (-l/-L).
 */

#include "sysconf_priv.h"

static void	apply_src_env_overrides(void);

/*
 * Resolve the pathname of the target format's defaults file: the format's
 * environment override (e.g., LOADER_DEFAULTS) verbatim when set and
 * non-empty, otherwise the descriptor's defaults file prefixed with the
 * `-R dir' root. Returns a newly allocated path, or NULL when the format
 * has no defaults file.
 */
char *
defaults_path(void)
{
	char *env;
	char *path;
	const struct bsdconf_format_def *def;
	char buf[PATH_MAX];

	if ((def = bsdconf_format_lookup(format)) == NULL ||
	    def->defaults == NULL)
		return (NULL);
	if (def->defaults_env != NULL &&
	    (env = getenv(def->defaults_env)) != NULL && *env != '\0') {
		if ((path = strdup(env)) == NULL)
			err(EXIT_FAILURE, NULL);
		return (path);
	}
	if (snprintf(buf, sizeof(buf), "%s%s", rootdir, def->defaults) >=
	    (int)sizeof(buf))
		errx(EXIT_FAILURE, "%s%s: %s", rootdir, def->defaults,
		    strerror(ENAMETOOLONG));
	if ((path = strdup(buf)) == NULL)
		err(EXIT_FAILURE, NULL);
	return (path);
}

/*
 * Resolve the target format and its ordered list of backing files from the
 * command line: either the `-f file' override (a single file; the format is
 * guessed from its name) or the leading `target' keyword (`loader',
 * `sysctl', `make'), whose multi-file list -- optionally including the
 * `-k module' drop-in -- comes from the format abstraction layer (which,
 * for targets carrying a defaults file, performs the same discovery the
 * boot-time consumer does; see bsdconf_format_files(3)). All paths honor
 * the `-R dir' (or `-j jail') root. A target with a defaults file always
 * has it prepended to the scan order (defaults first, mirroring the boot
 * loader, so reads report the effective value even for directives no conf
 * file mentions), but the slot is read-only bookkeeping: see defaults_idx.
 * With `-D', the defaults file replaces the list outright; `-A' widens
 * dump scope to include directives still at their default. Combined
 * (`-AD'), the scoping of `-D' wins and `-A' retains only its
 * dump-implying role, so `-ADd' describes all defaults and only defaults,
 * as in sysrc(8).
 */
void
resolve_target(const char *target)
{
	char *defaults;
	const char *env = NULL;
	const struct bsdconf_format_def *def;

	if (target == NULL) {
		warnx("no target provided");
		usage();
	}
	if (bsdconf_format_find(target, &format) != 0) {
		if (strcmp(target, "src-env") == 0)
			warnx("unknown target `src-env'; use `src' "
			    "(includes src-env.conf in the build triad) "
			    "or `-f' for a specific file");
		else
			warnx("unknown target `%s'", target);
		usage();
	}

	if (file != NULL) {
		char path[PATH_MAX];
		const char *prefix;

		/*
		 * `-f file' overrides which file(s) are consulted and
		 * modified, but never the format; that is always stated
		 * explicitly by the target keyword. Standard input
		 * (`-f -') is the caller's own and is not subject to -R.
		 */
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

	def = bsdconf_format_lookup(format);
	if (def == NULL || (def->path == NULL && def->sources == NULL))
		errx(EXIT_FAILURE,
		    "target `%s' has no default files; specify one with -f",
		    target);

	/* `-D' consults the defaults file alone */
	if (defaults_only) {
		if ((conf_files = calloc(1, sizeof(*conf_files))) == NULL)
			err(EXIT_FAILURE, NULL);
		conf_files[0] = defaults_path();
		nconf_files = 1;
		write_idx = 0;
		return;
	}

	if (def->defaults_env != NULL &&
	    (env = getenv(def->defaults_env)) != NULL && *env == '\0')
		env = NULL;
	if (bsdconf_format_files(format, rootdir, module, env, &conf_files,
	    &nconf_files, &write_idx) != 0 || nconf_files == 0)
		err(EXIT_FAILURE, "%s", target);

	/*
	 * The src triad honors the same environment overrides make(1)
	 * does when building /usr/src (verbatim, not subject to -R).
	 */
	if (format == BSDCONF_FORMAT_SRC)
		apply_src_env_overrides();

	/*
	 * A target with a defaults file sources it first at boot, so the
	 * effective value of a directive may come from the defaults alone
	 * even when no conf file mentions it. Prepend the defaults file
	 * to the scan order (defaults first, so every override lands
	 * later) and remember its index: the defaults are read-only, so
	 * that slot is never chosen as a write target, never satisfies a
	 * removal, and is never shown by -l/-L (see defaults_idx users).
	 */
	if ((defaults = defaults_path()) != NULL) {
		char **tmp;
		size_t n;

		tmp = calloc(nconf_files + 1, sizeof(*tmp));
		if (tmp == NULL)
			err(EXIT_FAILURE, NULL);
		tmp[0] = defaults;
		for (n = 0; n < nconf_files; n++)
			tmp[n + 1] = conf_files[n];
		free(conf_files);
		conf_files = tmp;
		nconf_files++;
		defaults_idx = 0;
		write_idx++;
	}
}

/*
 * Replace src triad paths with SRC_ENV_CONF / __MAKE_CONF / SRCCONF when
 * set and non-empty (same variables share/mk consults). Values are used
 * verbatim, matching LOADER_DEFAULTS.
 */
static void
apply_src_env_overrides(void)
{
	static const char *const envs[] = {
		"SRC_ENV_CONF",
		"__MAKE_CONF",
		"SRCCONF",
	};
	const char *env;
	char *path;
	size_t i;

	if (nconf_files != sizeof(envs) / sizeof(envs[0]))
		return;
	for (i = 0; i < nconf_files; i++) {
		if ((env = getenv(envs[i])) == NULL || *env == '\0')
			continue;
		if ((path = strdup(env)) == NULL)
			err(EXIT_FAILURE, NULL);
		free(conf_files[i]);
		conf_files[i] = path;
	}
}

/*
 * Print a candidate path unless `-E' excludes it for not existing or it
 * was already printed (`seen' is a NULL-terminated list of prior paths).
 */
static void
list_candidate(const char *path, char *seen[], size_t nseen)
{
	size_t n;

	for (n = 0; n < nseen; n++)
		if (seen[n] != NULL && strcmp(seen[n], path) == 0)
			return;
	if (existing_only && access(path, F_OK) != 0)
		return;
	puts(path);
}

/*
 * Record `path' in the `seen' list (for suppressing duplicates) after
 * printing it as a candidate.
 */
static void
list_candidate_seen(const char *path, char **seen, size_t *nseen,
    size_t maxseen)
{

	list_candidate(path, seen, *nseen);
	if (*nseen < maxseen)
		seen[(*nseen)++] = strdup(path);
}

/*
 * Process `-l' (list the files backing the target) and `-L' (additionally
 * list every drop-in candidate; for targets with a module drop-in
 * directory, one candidate per loaded kernel module plus any `*.conf'
 * already on disk). With `-E', only files that exist are shown. Returns
 * EXIT_SUCCESS.
 */
int
do_list(void)
{
	size_t n;
	size_t nseen = 0;
	char **seen;
	const char *moddir = NULL;
	const struct bsdconf_format_def *def;
	const struct bsdconf_source *source;
#define LIST_MAXSEEN	512
	char dpath[PATH_MAX];
	char path[PATH_MAX];

	if ((seen = calloc(LIST_MAXSEEN, sizeof(*seen))) == NULL)
		err(EXIT_FAILURE, NULL);

	for (n = 0; n < nconf_files; n++) {
		/* The read-only defaults file is never a write candidate */
		if ((int)n == defaults_idx)
			continue;
		list_candidate_seen(conf_files[n], seen, &nseen,
		    LIST_MAXSEEN);
	}

	if (!list_all)
		goto done;

	/* Locate the module drop-in directory (if the format has one) */
	def = bsdconf_format_lookup(format);
	if (def != NULL && def->sources != NULL)
		for (source = def->sources; source->path != NULL; source++)
			if (source->type == BSDCONF_SOURCE_MODDIR)
				moddir = source->path;
	if (moddir == NULL)
		goto done;

#ifdef __FreeBSD__
	/*
	 * One candidate per loaded kernel module, whether or not the file
	 * exists yet (the module being loaded is what makes the file
	 * eligible for sourcing by rc.subr(8)).
	 */
	{
		char name[MAXPATHLEN];
		char *dot;
		int fileid;
		struct kld_file_stat stat;

		for (fileid = kldnext(0); fileid > 0;
		    fileid = kldnext(fileid)) {
			stat.version = sizeof(stat);
			if (kldstat(fileid, &stat) != 0)
				continue;
			if (strcmp(stat.name, "kernel") == 0)
				continue;
			if (strlcpy(name, stat.name, sizeof(name)) >=
			    sizeof(name))
				continue;
			if ((dot = strrchr(name, '.')) != NULL &&
			    strcmp(dot, ".ko") == 0)
				*dot = '\0';
			if (snprintf(path, sizeof(path), "%s%s/%s.conf",
			    rootdir, moddir, name) >= (int)sizeof(path))
				continue;
			list_candidate_seen(path, seen, &nseen,
			    LIST_MAXSEEN);
		}
	}
#endif

	/* Plus any drop-in already on disk (module not currently loaded) */
	{
		int n2;
		int nentries;
		struct dirent **entries;

		if (snprintf(dpath, sizeof(dpath), "%s%s", rootdir,
		    moddir) >= (int)sizeof(dpath))
			goto done;
		nentries = scandir(dpath, &entries, NULL, alphasort);
		for (n2 = 0; n2 < nentries; n2++) {
			size_t len = strlen(entries[n2]->d_name);

			if (entries[n2]->d_name[0] != '.' && len > 5 &&
			    strcmp(entries[n2]->d_name + len - 5,
			    ".conf") == 0 &&
			    snprintf(path, sizeof(path), "%s/%s", dpath,
			    entries[n2]->d_name) < (int)sizeof(path))
				list_candidate_seen(path, seen, &nseen,
				    LIST_MAXSEEN);
			free(entries[n2]);
		}
		if (nentries >= 0)
			free(entries);
	}

done:
	for (n = 0; n < nseen; n++)
		free(seen[n]);
	free(seen);
	return (EXIT_SUCCESS);
}
