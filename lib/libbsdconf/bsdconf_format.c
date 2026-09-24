/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsdconf.h"
#include "bsdconf_formats.h"

/*
 * The format abstraction layer registry.
 *
 * Every configuration file format -- built-in or bolted on at run-time -- is
 * described by the same small descriptor (struct bsdconf_format_def in
 * bsdconf.h): a target keyword, a default write path, an ordered list of
 * configuration sources, and the processing/put bitmasks that drive the
 * shared parsing and writing cores. There is exactly one engine; a format
 * merely parameterizes it.
 *
 * Each built-in format is a self-contained translation unit
 * (bsdconf_format_<keyword>.c) documenting and defining its own descriptor;
 * this file only collects the descriptors into the table indexed by enum
 * bsdconf_format and resolves keywords, basenames, and source lists against
 * it (see bsdconf_formats.h for the recipe to add a format).
 *
 * A format that is "mostly like" an existing one need not be added at all:
 * an application calls bsdconf_format_derive() to inherit the nearest
 * descriptor, adjusts only the members that differ, and registers the
 * result with bsdconf_format_register(). The handle it gets back taps into
 * the same core engine everywhere a built-in format works, including
 * keyword, path, and source-list resolution in sysconf(8).
 */

static const struct bsdconf_format_def *bsdconf_formats[] = {
	[BSDCONF_FORMAT_GENERIC] =	&bsdconf_format_generic_def,
	[BSDCONF_FORMAT_LOADER] =	&bsdconf_format_loader_def,
	[BSDCONF_FORMAT_SYSCTL] =	&bsdconf_format_sysctl_def,
	[BSDCONF_FORMAT_MAKE] =		&bsdconf_format_make_def,
	[BSDCONF_FORMAT_SRC] =		&bsdconf_format_src_def,
};
#define BSDCONF_NFORMATS \
	(sizeof(bsdconf_formats) / sizeof(*bsdconf_formats))

#define BSDCONF_CONF_SUFFIX		".conf"
#define BSDCONF_CONF_SUFFIX_LEN		(sizeof(BSDCONF_CONF_SUFFIX) - 1)

/*
 * Registered (bolt-on) formats. Descriptors are copied by value; the
 * keyword and path strings they reference remain owned by the caller and
 * must stay valid for the life of the registration.
 */
#define BSDCONF_MAXUSERFORMATS	32
static struct bsdconf_format_def bsdconf_user_formats[BSDCONF_MAXUSERFORMATS];
static unsigned int bsdconf_nuser_formats = 0;

/*
 * Return the descriptor for `format' or NULL if the format is neither
 * built-in nor registered.
 */
const struct bsdconf_format_def *
bsdconf_format_lookup(enum bsdconf_format format)
{
	unsigned int n;

	if ((unsigned int)format < BSDCONF_NFORMATS)
		return (bsdconf_formats[format]);

	n = (unsigned int)format - BSDCONF_FORMAT_USER;
	if (format >= BSDCONF_FORMAT_USER && n < bsdconf_nuser_formats)
		return (&bsdconf_user_formats[n]);

	return (NULL);
}

/*
 * Copy the descriptor of `base' into `def' so that a caller can adjust only
 * the members that differ before registering the result as a new format.
 * On success, returns zero; otherwise returns -1 (unknown base format) and
 * errno is set to EINVAL.
 */
int
bsdconf_format_derive(enum bsdconf_format base,
    struct bsdconf_format_def *def)
{
	const struct bsdconf_format_def *found;

	/* Check arguments */
	if (def == NULL || (found = bsdconf_format_lookup(base)) == NULL) {
		errno = EINVAL;
		return (-1);
	}

	*def = *found;
	return (0);
}

/*
 * Register a new configuration file format described by `def', storing its
 * newly assigned handle through `format'. The descriptor is copied; the
 * keyword and path strings it references are not (see above). On success,
 * returns zero; otherwise returns -1 and errno is set (EINVAL for a bad
 * argument, ENOSPC when the registration table is full).
 */
int
bsdconf_format_register(const struct bsdconf_format_def *def,
    enum bsdconf_format *format)
{

	/* Check arguments */
	if (def == NULL || format == NULL) {
		errno = EINVAL;
		return (-1);
	}
	if (bsdconf_nuser_formats >= BSDCONF_MAXUSERFORMATS) {
		errno = ENOSPC;
		return (-1);
	}

	bsdconf_user_formats[bsdconf_nuser_formats] = *def;
	*format = (enum bsdconf_format)
	    (BSDCONF_FORMAT_USER + bsdconf_nuser_formats);
	bsdconf_nuser_formats++;

	return (0);
}

/*
 * Map a target keyword (e.g., "loader") to its format, storing the result
 * through `format'. Registered formats are searched after the built-ins.
 * On success, returns zero; otherwise returns -1 (unknown keyword; `format'
 * is left untouched).
 */
int
bsdconf_format_find(const char *keyword, enum bsdconf_format *format)
{
	unsigned int n;

	/* Check arguments */
	if (keyword == NULL || format == NULL)
		return (-1);

	for (n = 0; n < BSDCONF_NFORMATS; n++) {
		if (bsdconf_formats[n]->keyword != NULL &&
		    strcmp(bsdconf_formats[n]->keyword, keyword) == 0) {
			*format = (enum bsdconf_format)n;
			return (0);
		}
	}
	for (n = 0; n < bsdconf_nuser_formats; n++) {
		if (bsdconf_user_formats[n].keyword != NULL &&
		    strcmp(bsdconf_user_formats[n].keyword, keyword) == 0) {
			*format = (enum bsdconf_format)
			    (BSDCONF_FORMAT_USER + n);
			return (0);
		}
	}

	return (-1);
}

/*
 * Test whether the basename of `path' matches `<keyword>.conf' with an
 * optional trailing suffix (e.g., "sysctl.conf.local" matches "sysctl").
 */
static int
bsdconf_basename_matches(const char *base, const char *keyword)
{
	size_t klen;

	if (keyword == NULL)
		return (0);
	klen = strlen(keyword);
	if (strncmp(base, keyword, klen) != 0)
		return (0);
	return (strncmp(base + klen, BSDCONF_CONF_SUFFIX,
	    BSDCONF_CONF_SUFFIX_LEN) == 0);
}

/*
 * Guess the format of an arbitrary configuration file from the basename of
 * its `path' (e.g., "/etc/sysctl.conf.local" is BSDCONF_FORMAT_SYSCTL).
 * Registered formats are searched after the built-ins. Returns
 * BSDCONF_FORMAT_GENERIC when nothing matches.
 */
enum bsdconf_format
bsdconf_format_guess(const char *path)
{
	unsigned int n;
	char tmp[PATH_MAX];
	char *base;

	if (path == NULL)
		return (BSDCONF_FORMAT_GENERIC);

	if (strlen(path) >= sizeof(tmp))
		return (BSDCONF_FORMAT_GENERIC);
	memcpy(tmp, path, strlen(path) + 1);
	base = basename(tmp);

	for (n = 0; n < BSDCONF_NFORMATS; n++)
		if (bsdconf_basename_matches(base,
		    bsdconf_formats[n]->keyword))
			return ((enum bsdconf_format)n);
	for (n = 0; n < bsdconf_nuser_formats; n++)
		if (bsdconf_basename_matches(base,
		    bsdconf_user_formats[n].keyword))
			return ((enum bsdconf_format)
			    (BSDCONF_FORMAT_USER + n));

	return (BSDCONF_FORMAT_GENERIC);
}

/*
 * Return the default file path for `format' (e.g., "/boot/loader.conf") or
 * NULL if the format has no fixed path (e.g., BSDCONF_FORMAT_GENERIC).
 */
const char *
bsdconf_format_path(enum bsdconf_format format)
{
	const struct bsdconf_format_def *def;

	if ((def = bsdconf_format_lookup(format)) == NULL)
		return (NULL);
	return (def->path);
}

/*
 * Return the processing_options bitmask suited to `format', for passing to
 * bsdconf_parse(), bsdconf_fparse(), and bsdconf_put(). Unknown formats
 * fall back to the generic descriptor.
 */
uint16_t
bsdconf_format_processing(enum bsdconf_format format)
{
	const struct bsdconf_format_def *def;

	if ((def = bsdconf_format_lookup(format)) == NULL)
		def = bsdconf_formats[BSDCONF_FORMAT_GENERIC];
	return (def->processing);
}

/*
 * Return the put_options bitmask suited to `format', for passing to
 * bsdconf_put(). Unknown formats fall back to the generic descriptor.
 */
uint16_t
bsdconf_format_put(enum bsdconf_format format)
{
	const struct bsdconf_format_def *def;

	if ((def = bsdconf_format_lookup(format)) == NULL)
		def = bsdconf_formats[BSDCONF_FORMAT_GENERIC];
	return (def->put);
}

/*
 * Append `path' (already allocated; ownership is taken) to the growing
 * `files' array. On success, returns zero; otherwise returns -1 (errno set
 * by realloc(3)) and `path' is freed.
 */
static int
bsdconf_files_add(char ***files, size_t *nfiles, size_t *size, char *path)
{
	char **tmp;

	if (path == NULL)
		return (-1);
	if (*nfiles >= *size) {
		*size = (*size == 0) ? 8 : *size << 1;
		tmp = realloc(*files, *size * sizeof(**files));
		if (tmp == NULL) {
			free(path);
			return (-1);
		}
		*files = tmp;
	}
	(*files)[(*nfiles)++] = path;
	return (0);
}

/*
 * scandir(3) filter accepting non-hidden `*.conf' entries.
 */
static int
bsdconf_files_filter(const struct dirent *entry)
{
	size_t len;

	if (entry->d_name[0] == '.')
		return (0);
	len = strlen(entry->d_name);
	return (len > BSDCONF_CONF_SUFFIX_LEN &&
	    strcmp(entry->d_name + len - BSDCONF_CONF_SUFFIX_LEN,
	    BSDCONF_CONF_SUFFIX) == 0);
}

/*
 * Compose the full path `prefix' + `path' [+ `/' + `name' [+ `.conf']] into
 * newly allocated storage. Returns NULL on allocation failure (errno set).
 *
 * asprintf(3) is not in POSIX.1-2008; size with snprintf(NULL, 0) then fill
 * so the library still compiles under _POSIX_C_SOURCE=200809L.
 */
static char *
bsdconf_files_path(const char *prefix, const char *path, const char *name,
    const char *suffix)
{
	char *full;
	int len;

	len = snprintf(NULL, 0, "%s%s%s%s%s", prefix, path,
	    name != NULL ? "/" : "", name != NULL ? name : "",
	    suffix != NULL ? suffix : "");
	if (len < 0 || (full = malloc((size_t)len + 1)) == NULL)
		return (NULL);
	snprintf(full, (size_t)len + 1, "%s%s%s%s%s", prefix, path,
	    name != NULL ? "/" : "", name != NULL ? name : "",
	    suffix != NULL ? suffix : "");
	return (full);
}

/*
 * Append each existing `*.conf' entry of directory `path' (prefixed with
 * `prefix'), sorted, to the growing `files' array. A missing or unreadable
 * directory is not an error (a drop-in directory is optional by nature).
 * On success, returns zero; otherwise returns -1 (errno set).
 */
static int
bsdconf_files_add_dir(char ***files, size_t *nfiles, size_t *size,
    const char *prefix, const char *path)
{
	int n, nentries;
	char *dir;
	char *full;
	struct dirent **entries;

	if ((dir = bsdconf_files_path(prefix, path, NULL, NULL)) == NULL)
		return (-1);
	nentries = scandir(dir, &entries, bsdconf_files_filter, alphasort);
	free(dir);
	if (nentries < 0)
		return (0);
	for (n = 0; n < nentries; n++) {
		full = bsdconf_files_path(prefix, path, entries[n]->d_name,
		    NULL);
		if (bsdconf_files_add(files, nfiles, size, full) != 0) {
			while (n < nentries)
				free(entries[n++]);
			free(entries);
			return (-1);
		}
		free(entries[n]);
	}
	free(entries);
	return (0);
}

/*
 * Parse call-back for the list directives chased by
 * bsdconf_files_discover() below. The option's `value.data' member points
 * at the (char *) slot holding the current value of the directive; each
 * assignment encountered replaces the slot (last assignment wins, exactly
 * as the consumer of the file behaves).
 */
static int
bsdconf_files_list_cb(struct bsdconf_option *option, uint32_t line,
    char *directive, char *value)
{
	char *copy;
	char **slot = (char **)option->value.data;

	(void)line;
	(void)directive;

	if ((copy = strdup(bsdconf_unquote(value))) == NULL)
		return (-1);
	free(*slot);
	*slot = copy;
	return (0);
}

/*
 * Advance to the next whitespace-separated token of `list' at or beyond
 * `cursor', storing its length through `lenp'. Returns the token or NULL
 * when the list is exhausted.
 */
static const char *
bsdconf_files_token(const char *cursor, size_t *lenp)
{

	if (cursor == NULL)
		return (NULL);
	cursor += strspn(cursor, " \t");
	if ((*lenp = strcspn(cursor, " \t")) == 0)
		return (NULL);
	return (cursor);
}

/*
 * Ceiling on the number of configuration files discovery will chase,
 * bounding run-away (or maliciously self-referential) file lists.
 */
#define BSDCONF_DISCOVER_MAX	64

/*
 * Resolve the backing files of a format whose descriptor carries a
 * `defaults' file by performing the same discovery its consumer does: read
 * the defaults file, then chase the file-list directive (e.g.,
 * loader_conf_files) -- re-reading it after every file, as the loader does
 * when a file queues additional names -- and finally append the drop-in
 * directory entries and local files named by the final values of the other
 * two list directives (likewise applied only after the file-list walk).
 * Every file named by the file-list directive is included in
 * the result whether or not it exists (a listed file is a legitimate
 * write target before its first directive lands); the defaults file
 * itself is deliberately excluded. The write candidate stored through
 * `write_idx' is the last file-list entry, the final word among the
 * regular configuration files.
 *
 * The defaults file is `defaults' verbatim when non-NULL (a caller-level
 * override; see sysconf(8)'s LOADER_DEFAULTS); otherwise the descriptor
 * default prefixed with `rootdir'.
 *
 * On success, returns zero. Returns 1 when the defaults file is missing
 * or unreadable, directing the caller to fall back to the static source
 * list. Otherwise returns -1 (errno set).
 */
static int
bsdconf_files_discover(const struct bsdconf_format_def *def,
    const char *rootdir, const char *defaults, char ***files,
    size_t *nfiles, size_t *size, size_t *write_idx)
{
	int error, rv = -1;
	uint16_t processing;
	size_t len, n, nvisited = 0;
	char *dpath = NULL;
	char *full;
	char *name;
	char *conf_dirs = NULL;
	char *conf_files = NULL;
	char *local_files = NULL;
	const char *token;
	char *visited[BSDCONF_DISCOVER_MAX];
	struct bsdconf_option options[4];

	/* Wire each list directive to the slot holding its value */
	memset(options, 0, sizeof(options));
	n = 0;
	if (def->files_directive != NULL) {
		options[n].directive = def->files_directive;
		options[n].value.data = &conf_files;
		options[n].parse = bsdconf_files_list_cb;
		n++;
	}
	if (def->dirs_directive != NULL) {
		options[n].directive = def->dirs_directive;
		options[n].value.data = &conf_dirs;
		options[n].parse = bsdconf_files_list_cb;
		n++;
	}
	if (def->local_directive != NULL) {
		options[n].directive = def->local_directive;
		options[n].value.data = &local_files;
		options[n].parse = bsdconf_files_list_cb;
		n++;
	}

	/* Resolve and read the defaults file (missing means fall back) */
	if (defaults != NULL)
		dpath = strdup(defaults);
	else
		dpath = bsdconf_files_path(rootdir, def->defaults, NULL,
		    NULL);
	if (dpath == NULL)
		goto cleanup;
	processing = def->processing & ~BSDCONF_REQUIRE_EQUALS;
	if (bsdconf_parse(options, dpath, NULL, processing) != 0) {
		rv = 1;
		goto cleanup;
	}

	/*
	 * Chase the file-list directive. Each pass rescans the current
	 * value of the list from the start for the first file not yet
	 * visited (any file read may have revised the list), reads it,
	 * and repeats until every listed file has been visited.
	 */
	for (;;) {
		token = conf_files;
		while ((token = bsdconf_files_token(token, &len)) != NULL) {
			for (n = 0; n < nvisited; n++)
				if (strncmp(visited[n], token, len) == 0 &&
				    visited[n][len] == '\0')
					break;
			if (n == nvisited)
				break;
			token += len;
		}
		if (token == NULL || nvisited >= BSDCONF_DISCOVER_MAX)
			break;
		if ((visited[nvisited] = strndup(token, len)) == NULL)
			goto cleanup;
		full = bsdconf_files_path(rootdir, visited[nvisited], NULL,
		    NULL);
		nvisited++;
		if (bsdconf_files_add(files, nfiles, size, full) != 0)
			goto cleanup;
		/* A listed file that does not (yet) exist is normal */
		(void)bsdconf_parse(options, full, NULL, processing);
	}
	*write_idx = (*nfiles > 0) ? *nfiles - 1 : 0;

	/* Drop-in directories, then local files, from the final lists */
	token = conf_dirs;
	while ((token = bsdconf_files_token(token, &len)) != NULL) {
		if ((name = strndup(token, len)) == NULL)
			goto cleanup;
		token += len;
		error = bsdconf_files_add_dir(files, nfiles, size, rootdir,
		    name);
		free(name);
		if (error != 0)
			goto cleanup;
	}
	token = local_files;
	while ((token = bsdconf_files_token(token, &len)) != NULL) {
		if ((name = strndup(token, len)) == NULL)
			goto cleanup;
		token += len;
		full = bsdconf_files_path(rootdir, name, NULL, NULL);
		free(name);
		if (bsdconf_files_add(files, nfiles, size, full) != 0)
			goto cleanup;
	}

	rv = 0;

cleanup:
	n = errno; /* preserve errno across free(3) */
	while (nvisited > 0)
		free(visited[--nvisited]);
	free(conf_files);
	free(conf_dirs);
	free(local_files);
	free(dpath);
	errno = n;
	return (rv);
}

/*
 * Resolve the ordered list of configuration files backing `format' into a
 * newly allocated array of newly allocated paths, stored through `filesp'
 * with the count stored through `nfilesp'. Files appear in the order the
 * system sources them at boot; a directive in a later file overrides the
 * same directive in an earlier one, making the last file listing a
 * directive the authoritative source of its value. When `write_idxp' is
 * non-NULL, the index of the file recommended for directives found in no
 * file at all is stored through it (the format's default write path, or
 * the last regular configuration file when discovery is in play).
 *
 * A format whose descriptor names a defaults file resolves its list by
 * discovery (see bsdconf_files_discover() above); `defaults' overrides
 * the descriptor's defaults file when non-NULL and is used verbatim (it
 * is not prefixed with `rootdir'). For all other formats `defaults' is
 * ignored and the static source list governs, as it also does when the
 * defaults file is missing.
 *
 * Each static path is prefixed with `rootdir' unless NULL or empty.
 * Sources of type BSDCONF_SOURCE_FILE are always listed, whether or not
 * the file exists; sources of type BSDCONF_SOURCE_DIR contribute each
 * existing `*.conf' entry (sorted); sources of type BSDCONF_SOURCE_MODDIR
 * contribute `<module>.conf' only when `module' is non-NULL. A format
 * with no source list resolves to its default path alone.
 *
 * On success, returns zero and the caller frees the result with
 * bsdconf_format_files_free(). Otherwise returns -1 and errno is set
 * (EINVAL for an unknown format or a format with no backing files).
 */
int
bsdconf_format_files(enum bsdconf_format format, const char *rootdir,
    const char *module, const char *defaults, char ***filesp,
    size_t *nfilesp, size_t *write_idxp)
{
	int error;
	size_t nfiles = 0;
	size_t size = 0;
	size_t write_idx = 0;
	char **files = NULL;
	char *path;
	const struct bsdconf_format_def *def;
	const struct bsdconf_source *source;

	/* Check arguments */
	if (filesp == NULL || nfilesp == NULL ||
	    (def = bsdconf_format_lookup(format)) == NULL) {
		errno = EINVAL;
		return (-1);
	}
	if (rootdir == NULL)
		rootdir = "";

	/* Discover the list when the descriptor names a defaults file */
	if (def->defaults != NULL) {
		error = bsdconf_files_discover(def, rootdir, defaults,
		    &files, &nfiles, &size, &write_idx);
		if (error < 0)
			goto cleanup;
		if (error == 0 && nfiles > 0)
			goto done;
		/* Missing defaults file; fall back to static sources */
		bsdconf_format_files_free(files, nfiles);
		files = NULL;
		nfiles = size = write_idx = 0;
	}

	/* A format with no source list is backed by its path alone */
	if (def->sources == NULL) {
		if (def->path == NULL) {
			errno = EINVAL;
			return (-1);
		}
		path = bsdconf_files_path(rootdir, def->path, NULL, NULL);
		if (bsdconf_files_add(&files, &nfiles, &size, path) != 0)
			goto cleanup;
		goto done;
	}

	for (source = def->sources; source->path != NULL; source++) {
		switch (source->type) {
		case BSDCONF_SOURCE_FILE:
			path = bsdconf_files_path(rootdir, source->path,
			    NULL, NULL);
			if (def->path != NULL &&
			    strcmp(source->path, def->path) == 0)
				write_idx = nfiles;
			if (bsdconf_files_add(&files, &nfiles, &size,
			    path) != 0)
				goto cleanup;
			break;
		case BSDCONF_SOURCE_DIR:
			if (bsdconf_files_add_dir(&files, &nfiles, &size,
			    rootdir, source->path) != 0)
				goto cleanup;
			break;
		case BSDCONF_SOURCE_MODDIR:
			if (module == NULL)
				break;
			path = bsdconf_files_path(rootdir, source->path,
			    module, BSDCONF_CONF_SUFFIX);
			if (bsdconf_files_add(&files, &nfiles, &size,
			    path) != 0)
				goto cleanup;
			break;
		}
	}

done:
	*filesp = files;
	*nfilesp = nfiles;
	if (write_idxp != NULL)
		*write_idxp = write_idx;
	return (0);

cleanup:
	bsdconf_format_files_free(files, nfiles);
	return (-1);
}

/*
 * Release an array of paths obtained from bsdconf_format_files().
 */
void
bsdconf_format_files_free(char **files, size_t nfiles)
{
	size_t n;

	if (files == NULL)
		return;
	for (n = 0; n < nfiles; n++)
		free(files[n]);
	free(files);
}
