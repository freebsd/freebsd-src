/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * File text for the ucl target: read, parse, replace, and the flat
 * tree a write is checked against.
 */


#include <sys/stat.h>

#include <ctype.h>
#include <stdckdint.h>

#include <ucl.h>

#include "sysconf_priv.h"
#include "sysconf_ucl.h"

/*
 * Read all of the file open on `fd' (named `path' in errors). Returns
 * malloc'd text. A null character is fatal: UCL is text, and every span
 * below is measured with strlen(3).
 */
char *
slurp_fd(int fd, const char *path, size_t *lenp)
{
	struct stat sb;
	char *text;
	size_t got;
	size_t size;
	ssize_t r;

	if (fstat(fd, &sb) != 0)
		err(EXIT_FAILURE, "%s", path);
	if (sb.st_size < 0 || ckd_add(&size, sb.st_size, 1))
		errx(EXIT_FAILURE, "%s: %s", path, strerror(EFBIG));
	if ((text = malloc(size)) == NULL)
		err(EXIT_FAILURE, NULL);
	for (got = 0; got < size - 1; got += (size_t)r) {
		r = read(fd, text + got, size - 1 - got);
		if (r < 0)
			err(EXIT_FAILURE, "%s", path);
		if (r == 0)
			break;
	}
	if (memchr(text, '\0', got) != NULL)
		errx(EXIT_FAILURE, "%s: contains a null character", path);
	text[got] = '\0';
	*lenp = got;
	return (text);
}

/* Read all of `path' (missing is empty). Returns malloc'd text. */
char *
slurp(const char *path, size_t *lenp)
{
	char *text;
	int fd;

	*lenp = 0;
	if ((fd = open(path, O_RDONLY)) < 0) {
		if (errno != ENOENT)
			err(EXIT_FAILURE, "%s", path);
		if ((text = strdup("")) == NULL)
			err(EXIT_FAILURE, NULL);
		return (text);
	}
	text = slurp_fd(fd, path, lenp);
	close(fd);
	return (text);
}

/* Parse `text'; an empty text is an empty object. Fatal on error. */
ucl_object_t *
ucl_parse_text(const char *path, const char *text, size_t len,
    const char **errp)
{
	struct ucl_parser *parser;
	ucl_object_t *top = NULL;
	const char *perr;
	char *ready;
	size_t rlen;

	if (errp != NULL)
		*errp = NULL;
	ready = ucl_text_ready(text, len, &rlen);
	if ((parser = ucl_parser_new(UCL_PARSE_FLAGS)) == NULL)
		err(EXIT_FAILURE, NULL);
	if (rlen > 0) {
		if (!ucl_parser_add_string(parser, ready, rlen)) {
			perr = ucl_parser_get_error(parser);
			if (perr == NULL)
				perr = "parse error";
			if (errp == NULL)
				errx(EXIT_FAILURE, "%s: %s", path, perr);
			if ((*errp = strdup(perr)) == NULL)
				err(EXIT_FAILURE, NULL);
			ucl_parser_free(parser);
			free(ready);
			return (NULL);
		}
		top = ucl_parser_get_object(parser);
	}
	free(ready);
	ucl_parser_free(parser);
	if (top == NULL && (top = ucl_object_typed_new(UCL_OBJECT)) == NULL)
		err(EXIT_FAILURE, NULL);
	return (top);
}

/*
 * Replace `path' atomically with `text', keeping the mode and (best
 * effort) the ownership of an existing file. A symbolic link is
 * followed: the file it names is replaced, and the link stays. A
 * dangling link is left in place. Only a regular file can be replaced
 * so, as bsdconf_put() does for the other targets, anything else
 * is refused. A path that is not there is created.
 */
void
ucl_replace_file(const char *path, const char *text, size_t len)
{
	struct stat sb;
	mode_t mode;
	mode_t mask;
	size_t off;
	ssize_t w;
	bool exists;
	int fd;
	char rpath[PATH_MAX];
	char tmp[PATH_MAX];

	if (realpath(path, rpath) != NULL)
		exists = (stat(rpath, &sb) == 0);
	else if (errno != ENOENT)
		err(EXIT_FAILURE, "%s", path);
	else if (lstat(path, &sb) == 0)
		errx(EXIT_FAILURE, "%s: %s", path, strerror(ENOENT));
	else if (errno != ENOENT)
		err(EXIT_FAILURE, "%s", path);
	else {
		if (strlcpy(rpath, path, sizeof(rpath)) >= sizeof(rpath))
			errx(EXIT_FAILURE, "%s: %s", path,
			    strerror(ENAMETOOLONG));

		exists = false;
	}
	if (exists && !S_ISREG(sb.st_mode))
		errx(EXIT_FAILURE, "%s: %s", path, strerror(EINVAL));
	if (snprintf(tmp, sizeof(tmp), "%s.XXXXXX", rpath) >=
	    (int)sizeof(tmp))
		errx(EXIT_FAILURE, "%s: %s", rpath, strerror(ENAMETOOLONG));
	if ((fd = mkstemp(tmp)) < 0)
		err(EXIT_FAILURE, "%s", tmp);
	if (exists)
		mode = sb.st_mode & 07777;
	else {
		mask = umask(0);
		umask(mask);
		mode = 0666 & ~mask;
	}
	if (fchmod(fd, mode) != 0) {
		unlink(tmp);
		err(EXIT_FAILURE, "%s", tmp);
	}
	if (exists)
		(void)fchown(fd, sb.st_uid, sb.st_gid); /* best effort */
	for (off = 0; off < len; off += (size_t)w) {
		if ((w = write(fd, text + off, len - off)) < 0) {
			unlink(tmp);
			err(EXIT_FAILURE, "%s", tmp);
		}
	}
	if (fsync(fd) != 0 || close(fd) != 0 || rename(tmp, rpath) != 0) {
		unlink(tmp);
		err(EXIT_FAILURE, "%s", rpath);
	}
}

/*
 * A flattened tree: dotted path -> rendering, last definition wins.
 * Used to prove an edit did exactly what was asked.
 */
void
flat_set(struct flat *f, const char *name, const char *value)
{
	char **tmp;
	size_t i;

	for (i = 0; i < f->n; i++) {
		if (strcmp(f->names[i], name) == 0) {
			free(f->values[i]);
			if ((f->values[i] = strdup(value)) == NULL)
				err(EXIT_FAILURE, NULL);
			return;
		}
	}
	if ((tmp = reallocarray(f->names, f->n + 1, sizeof(*tmp))) == NULL)
		err(EXIT_FAILURE, NULL);
	f->names = tmp;
	if ((tmp = reallocarray(f->values, f->n + 1, sizeof(*tmp))) == NULL)
		err(EXIT_FAILURE, NULL);
	f->values = tmp;
	if ((f->names[f->n] = strdup(name)) == NULL ||
	    (f->values[f->n] = strdup(value)) == NULL)
		err(EXIT_FAILURE, NULL);
	f->n++;
}

/* Drop `name' and everything beneath `name.' */
void
flat_remove(struct flat *f, const char *name)
{
	size_t i;
	size_t j;
	size_t len = strlen(name);

	for (i = j = 0; i < f->n; i++) {
		if (strcmp(f->names[i], name) == 0 ||
		    (strncmp(f->names[i], name, len) == 0 &&
		    f->names[i][len] == '.')) {
			free(f->names[i]);
			free(f->values[i]);
			continue;
		}
		f->names[j] = f->names[i];
		f->values[j] = f->values[i];
		j++;
	}
	f->n = j;
}

int
flat_cb(const char *name, const char *value, ucl_type_t type __unused,
    void *arg)
{

	flat_set(arg, name, value);
	return (0);
}

void
flat_free(struct flat *f)
{
	size_t i;

	for (i = 0; i < f->n; i++) {
		free(f->names[i]);
		free(f->values[i]);
	}
	free(f->names);
	free(f->values);
}

/* True when the two flattenings agree exactly */
bool
flat_equal(const struct flat *a, const struct flat *b)
{
	size_t i;
	size_t j;

	if (a->n != b->n)
		return (false);
	for (i = 0; i < a->n; i++) {
		for (j = 0; j < b->n; j++)
			if (strcmp(a->names[i], b->names[j]) == 0)
				break;
		if (j == b->n || strcmp(a->values[i], b->values[j]) != 0)
			return (false);
	}
	return (true);
}

/*
 * The file that receives a write request: the authoritative file when it
 * is writable, otherwise the target's default (pkg sends a repository key
 * to the local repos file and a global key to pkg.conf).
 */
size_t
ucl_write_target(const struct request *req)
{

	if (req->srcidx < 0 || req->srcidx == defaults_idx ||
	    (conf_ro != NULL && conf_ro[req->srcidx])) {
		if (pkg_target && strchr(req->ucl_base != NULL ?
		    req->ucl_base : req->name, '.') != NULL)
			return (pkg_repos_idx);
		return (write_idx);
	}
	return ((size_t)req->srcidx);
}

/*
 * Replace the parse `top' of a file being edited with that of its
 * current text, so later requests in the same run see earlier edits.
 * Text libucl rejects is fatal here, before anything is written.
 */
ucl_object_t *
ucl_reparse(ucl_object_t *top, const char *path, const char *text,
    size_t len)
{
	ucl_object_t *fresh;
	const char *perr = NULL;

	ucl_object_unref(top);
	if ((fresh = ucl_parse_text(path, text, len, &perr)) == NULL)
		errx(EXIT_FAILURE,
		    "%s: edit would not parse (%s); file left unchanged",
		    path, perr != NULL ? perr : "unknown error");
	return (fresh);
}

