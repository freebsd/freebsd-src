/*
 * Copyright (c) 2002-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bsdconf.h"
#include "bsdconf_internal.h"

/*
 * Search for a config option (struct bsdconf_option) in the array of config
 * options, returning the struct whose directive matches the given parameter.
 * If no match is found, NULL is returned.
 *
 * This is to eliminate dependency on the index position of an item in the
 * array, since the index position is more apt to be changed as code grows.
 */
struct bsdconf_option *
bsdconf_get_option(struct bsdconf_option options[], const char *directive)
{
	uint32_t n;

	if (options == NULL || directive == NULL)
		return (NULL);

	for (n = 0; options[n].directive != NULL; n++)
		if (strcmp(options[n].directive, directive) == 0)
			return (&options[n]);

	return (NULL);
}

/*
 * Strip one layer of surrounding double-quotes from `value' in place (the
 * parser preserves quotes so that values round-trip losslessly; see
 * bsdconf_fparse() below). Returns `value' for convenience. If the value is
 * not a quoted string, it is returned unmodified.
 */
char *
bsdconf_unquote(char *value)
{
	size_t len;

	if (value == NULL || (len = strlen(value)) < 2)
		return (value);
	if (value[0] != '"' || value[len - 1] != '"')
		return (value);

	memmove(value, value + 1, len - 2);
	value[len - 2] = '\0';

	return (value);
}

/*
 * Copy the remaining contents of the open file descriptor `fd' to an
 * unlinked temporary file and return a seekable descriptor referencing it
 * (which the caller must close(2); the backing storage is reclaimed then).
 * Exported for callers that need a seekable snapshot of a pipe or socket.
 * Returns the new descriptor on success; otherwise returns -1 and errno
 * should be consulted.
 */
int
bsdconf_spool(int fd)
{
	FILE *tmp;
	int error;
	int newfd;
	int tfd;
	ssize_t r;
	char buf[8192];

	if ((tmp = tmpfile()) == NULL)
		return (-1);
	tfd = fileno(tmp);

	for (;;) {
		r = read(fd, buf, sizeof(buf));
		if (r < 0) {
			if (errno == EINTR)
				continue;
			goto fail;
		}
		if (r == 0)
			break;
		if (bsdconf_writeall(tfd, buf, (size_t)r) != 0)
			goto fail;
	}
	if (lseek(tfd, 0, SEEK_SET) == -1)
		goto fail;

	/* Detach the descriptor from the stream before closing it */
	if ((newfd = dup(tfd)) == -1)
		goto fail;
	fclose(tmp);
	return (newfd);

fail:
	error = errno; /* preserve errno across fclose(3) */
	fclose(tmp);
	errno = error;
	return (-1);
}

/*
 * Invoke the unknown-directive call-back with a stack-local option that
 * carries the statement's assignment operator (there is no matched
 * options[] slot to hang it on). Returns the call-back's result.
 */
static int
bsdconf_call_unknown(int (*unknown)(struct bsdconf_option *option,
    uint32_t line, char *directive, char *value), enum bsdconf_op op,
    uint32_t dline, char *directive, char *value)
{
	struct bsdconf_option unk;

	memset(&unk, 0, sizeof(unk));
	unk.op = op;
	return (unknown(&unk, dline, directive, value));
}

/*
 * Parse the configuration data on the open file descriptor `fd' and execute
 * the `parse' call-back functions for any directives defined by the array of
 * config options (first argument).
 *
 * For unknown directives that are encountered, you can optionally pass a
 * call-back function for the third argument to be called for unknowns.
 *
 * The descriptor is read into a bounded in-memory buffer (see
 * bsdconf_slurp()) and then scanned as an array of characters with
 * bsdconf_scan(), the same tokenizer used by bsdconf_put(). The descriptor
 * need not be seekable; a pipe or socket is read to EOF subject to the
 * BSDCONF_MAX_BYTES cap. The descriptor is left positioned at end-of-file
 * (non-seekable input is left drained) and remains open (the caller retains
 * ownership).
 *
 * Returns zero on success; otherwise returns -1 (or the non-zero result of a
 * call-back) and errno should be consulted.
 */
int
bsdconf_fparse(struct bsdconf_option options[], int fd,
    int (*unknown)(struct bsdconf_option *option, uint32_t line,
    char *directive, char *value), uint16_t processing_options)
{
	bool bequals;
	bool bsemicolon;
	bool case_sensitive;
	bool found;
	bool operator_equals;
	bool require_equals;
	bool strict_equals;
	char *buf = NULL;
	char *directive = NULL;
	char *t;
	char *value = NULL;
	enum bsdconf_op op;
	int error;
	int rv = 0;
	int sverrno;
	size_t buflen = 0;
	size_t dsize = 0;
	size_t i = 0;
	size_t n;
	size_t vsize = 0;
	struct bsdconf_stmt st;
	uint32_t line = 1;
	unsigned int x;

	/* Sanity check: if no options and no unknown function, return */
	if (options == NULL && unknown == NULL) {
		errno = EINVAL;
		return (-1);
	}

	if ((buf = bsdconf_slurp(fd, &buflen)) == NULL)
		return (-1);

	/* Processing options */
	bequals = processing_options & BSDCONF_BREAK_ON_EQUALS;
	bsemicolon = processing_options & BSDCONF_BREAK_ON_SEMICOLON;
	case_sensitive = processing_options & BSDCONF_CASE_SENSITIVE;
	operator_equals = processing_options & BSDCONF_OPERATOR_EQUALS;
	require_equals = processing_options & BSDCONF_REQUIRE_EQUALS;
	strict_equals = processing_options & BSDCONF_STRICT_EQUALS;

	while (bsdconf_scan(buf, buflen, &i, &line, bequals, bsemicolon,
	    strict_equals, operator_equals, &st)) {
		n = st.dir_end - st.dir_start;
		if (directive == NULL || n > dsize) {
			if ((t = realloc(directive, n + 1)) == NULL)
				goto fail;
			directive = t;
			dsize = n;
		}
		memcpy(directive, buf + st.dir_start, n);
		directive[n] = '\0';

		op = st.op;
		if (!case_sensitive)
			bsdconf_strtolower(directive);

		if (!st.have_value) {
			if (value == NULL && (value = malloc(1)) == NULL)
				goto fail;
			value[0] = '\0';
			goto call_function;
		}

		n = st.val_end - st.val_start;
		if (value == NULL || n > vsize) {
			if ((t = realloc(value, n + 1)) == NULL)
				goto fail;
			value = t;
			vsize = n;
		}
		memcpy(value, buf + st.val_start, n);
		value[n] = '\0';

		/* Escape the escaped quotes (see bsdconf_string.c) */
		x = bsdconf_strcount(value, "\\\"");
		if (x != 0 && (n + x) > vsize) {
			if ((t = realloc(value, n + x + 1)) == NULL)
				goto fail;
			value = t;
			vsize = n + x;
		}
		if (bsdconf_replaceall(value, vsize + 1, "\\\"", "\\\\\"") < 0)
			goto fail;

		/* Remove all escaped newline characters */
		if (bsdconf_replaceall(value, vsize + 1, "\\\n", "") < 0)
			goto fail;

		/* Resolve escape sequences */
		bsdconf_strunexpand(value, value);

call_function:
		/* Abort if we're seeking only assignments */
		if (require_equals && !st.have_equals) {
			errno = EINVAL;
			goto fail;
		}

		found = 0;

		/*
		 * Report the statement's assignment operator through a
		 * stack-local option when invoking the unknown call-back
		 * (there is no matched options[] slot to hang it on).
		 */
		if (options == NULL && unknown != NULL) {
			error = bsdconf_call_unknown(unknown, op, st.line,
			    directive, value);
			if (error != 0) {
				rv = error;
				goto cleanup;
			}
			continue;
		}

		/* Loop through the array looking for a match */
		for (n = 0; options[n].directive != NULL; n++) {
			error = fnmatch(options[n].directive, directive,
			    FNM_NOESCAPE);
			if (error == 0) {
				found = 1;
				/* Call function for array index item */
				options[n].op = op;
				if (options[n].parse != NULL) {
					error = options[n].parse(&options[n],
					    st.line, directive, value);
					if (error != 0) {
						rv = error;
						goto cleanup;
					}
				}
			} else if (error != FNM_NOMATCH) {
				/* An error has occurred */
				errno = EINVAL;
				goto fail;
			}
		}
		if (!found && unknown != NULL) {
			/*
			 * No match was found for the value we read from the
			 * file; call function designated for unknown values.
			 */
			error = bsdconf_call_unknown(unknown, op, st.line,
			    directive, value);
			if (error != 0) {
				rv = error;
				goto cleanup;
			}
		}
	}

	goto cleanup;

fail:
	rv = -1;

cleanup:
	sverrno = errno; /* preserve errno across free(3) */
	free(buf);
	free(directive);
	free(value);
	errno = sverrno;

	return (rv);
}

/*
 * Parse the configuration file at `path' and execute the `parse' call-back
 * functions for any directives defined by the array of config options (first
 * argument). This is a convenience wrapper around bsdconf_fparse() above.
 *
 * Returns zero on success; otherwise returns -1 (or the non-zero result of a
 * call-back) and errno should be consulted.
 */
int
bsdconf_parse(struct bsdconf_option options[], const char *path,
    int (*unknown)(struct bsdconf_option *option, uint32_t line,
    char *directive, char *value), uint16_t processing_options)
{
	int error;
	int fd;

	/* Sanity check: if no options and no unknown function, return */
	if (path == NULL || (options == NULL && unknown == NULL)) {
		errno = EINVAL;
		return (-1);
	}

	/* Open the file */
	if ((fd = open(path, O_RDONLY)) < 0)
		return (-1);

	error = bsdconf_fparse(options, fd, unknown, processing_options);

	close(fd);
	return (error);
}
