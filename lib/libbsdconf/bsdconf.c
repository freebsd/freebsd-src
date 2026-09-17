/*
 * Copyright (c) 2002-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <ctype.h>
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
 * This adapts input that cannot seek -- a pipe or socket, standard input
 * included -- for the scanner in bsdconf_fparse() below, which seeks
 * liberally. Returns the new descriptor on success; otherwise returns -1
 * and errno should be consulted.
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
 * Read one byte into `*p', restarting on EINTR. Returns 1 on success, 0 on
 * EOF, or -1 on error (with errno set). Callers must treat a negative return
 * as failure: a loop conditioned only on `r != 0' spins forever on error
 * because read(2) returns -1, and a length counter in such a loop can grow
 * without bound (see the directive scan in bsdconf_fparse() below).
 */
static ssize_t
bsdconf_read1(int fd, char *p)
{
	ssize_t r;

	do {
		r = read(fd, p, 1);
	} while (r < 0 && errno == EINTR);
	return (r);
}

/*
 * Read exactly `n' bytes into `buf', restarting on EINTR. Returns 0 on
 * success, or -1 on error / premature EOF (with errno set; EIO for a short
 * read after the caller measured a length on a seekable descriptor).
 */
static int
bsdconf_readn(int fd, void *buf, size_t n)
{
	char *p = buf;
	size_t off = 0;
	ssize_t r;

	while (off < n) {
		r = read(fd, p + off, n - off);
		if (r < 0) {
			if (errno == EINTR)
				continue;
			return (-1);
		}
		if (r == 0) {
			errno = EIO;
			return (-1);
		}
		off += (size_t)r;
	}
	return (0);
}

/*
 * Advance past horizontal whitespace (spaces and tabs, not newline).
 * Updates `*r' and the byte in `*p'. Returns 0 on success, or -1 on
 * read error (errno set).
 */
static int
bsdconf_skip_hspace(int fd, char *p, ssize_t *r)
{

	while (*r > 0 && isspace((unsigned char)*p) && *p != '\n') {
		*r = bsdconf_read1(fd, p);
		if (*r < 0)
			return (-1);
	}
	return (0);
}

/*
 * Truncate trailing whitespace from a NUL-terminated string whose end
 * (the NUL) is at `end'. Returns a pointer to the last remaining
 * character, or to `value' when the string is empty.
 */
static char *
bsdconf_rtrim_ws(char *value, char *end)
{
	char *t = end;

	while (t > value && isspace((unsigned char)*--t))
		*t = '\0';
	return (t);
}

/*
 * Drop a trailing inline `#' or unescaped `;' that rode along with the
 * value (historic figpar behavior), then trim again. `ecomment' is set
 * when the end-key scan stopped on an unquoted `#'.
 */
static char *
bsdconf_trim_value_key(char *value, char *t, bool ecomment, bool bsemicolon)
{
	uint32_t x;

	if (ecomment && t > value && *t == '#') {
		*t = '\0';
		return (bsdconf_rtrim_ws(value, t));
	}
	if (bsemicolon && t > value && *t == ';') {
		for (x = 0; t - x > value && *(t - x - 1) == '\\'; x++)
			;
		if ((x & 1) == 0) {
			*t = '\0';
			return (bsdconf_rtrim_ws(value, t));
		}
	}
	return (t);
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
 * Scan from the current byte in `*p' to the end of the value. Handles
 * quotes, escaped end-keys, inline comments, and semicolon terminators.
 * On return, `*p' holds the terminating key (or is at EOF), and `*r',
 * `*line', `*comment', and `*ecomment' are updated. Returns 0 on
 * success, or -1 on seek/read error (errno set).
 */
static int
bsdconf_scan_value_end(int fd, char *p, ssize_t *r, uint32_t *line,
    uint8_t *comment, uint8_t *ecomment, bool bsemicolon)
{
	uint8_t end = 0;
	uint8_t quote = 0;
	uint32_t n;
	off_t charpos;

	*ecomment = 0;
	while (*r > 0 && end == 0) {
		/* Advance to the next character if we know we can */
		if (*p != '\"' && *p != '#' && *p != '\n' &&
		    (!bsemicolon || *p != ';')) {
			*r = bsdconf_read1(fd, p);
			if (*r < 0)
				return (-1);
			continue;
		}

		/*
		 * If we get this far, we've hit an end-key
		 */

		/* Get the current offset */
		if ((charpos = lseek(fd, 0, SEEK_CUR)) == -1)
			return (-1);
		charpos--;

		/*
		 * Go back so we can read the character before the key to
		 * check if the character is escaped (which means we should
		 * continue).
		 */
		if (lseek(fd, -2, SEEK_CUR) == -1)
			return (-1);
		*r = bsdconf_read1(fd, p);
		if (*r < 0)
			return (-1);

		/*
		 * Count how many backslashes there are (an odd number means
		 * the key is escaped, even means otherwise).
		 */
		for (n = 1; *r > 0 && *p == '\\'; n++) {
			/* Move back another offset to read */
			if (lseek(fd, -2, SEEK_CUR) == -1)
				return (-1);
			*r = bsdconf_read1(fd, p);
			if (*r < 0)
				return (-1);
		}

		/* Move offset back to the key and read it */
		if (lseek(fd, charpos, SEEK_SET) == -1)
			return (-1);
		*r = bsdconf_read1(fd, p);
		if (*r < 0)
			return (-1);

		/*
		 * If an even number of backslashes was counted meaning key
		 * is not escaped, we should evaluate what to do.
		 */
		if ((n & 1) == 1) {
			switch (*p) {
			case '\"':
				/*
				 * Flag current sequence of characters to
				 * follow as being quoted (hashes are not
				 * considered comments).
				 */
				quote = !quote;
				break;
			case '#':
				/*
				 * If we aren't in a quoted series, we just
				 * hit an inline comment and have found the
				 * end of the value. Flag the remainder of
				 * the line as a comment so it is not
				 * mistaken for a new directive.
				 */
				if (!quote) {
					*ecomment = *comment = 1;
					end = 1;
				}
				break;
			case '\n':
				/*
				 * Newline characters must always be escaped,
				 * whether inside a quoted series or not,
				 * otherwise they terminate the value.
				 */
				(*line)++;
				end = 1;
				/* FALLTHROUGH */
			case ';':
				if (!quote && bsemicolon)
					end = 1;
				break;
			}
		} else if (*p == '\n')
			/* Escaped newline character. increment */
			(*line)++;

		/* Advance to the next character */
		*r = bsdconf_read1(fd, p);
		if (*r < 0)
			return (-1);
	}
	return (0);
}

/*
 * Parse the configuration data on the open file descriptor `fd' and execute
 * the `parse' call-back functions for any directives defined by the array of
 * config options (first argument).
 *
 * For unknown directives that are encountered, you can optionally pass a
 * call-back function for the third argument to be called for unknowns.
 *
 * The scanner requires a seekable descriptor; input that cannot seek (a
 * pipe or socket, standard input included) is detected up front and spooled
 * through bsdconf_spool() above, parsed from the temporary, and costs one
 * transient copy of the data. The descriptor is left positioned at
 * end-of-file (non-seekable input is left drained) and remains open (the
 * caller retains ownership).
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
	bool operator_equals;
	bool require_equals;
	bool strict_equals;
	uint8_t comment = 0;
	uint8_t ecomment;
	uint8_t found;
	uint8_t have_equals = 0;
	char p[2];
	char *directive = NULL;
	char *t;
	char *value = NULL;
	enum bsdconf_op op;
	int error;
	int rv = 0;
	int spoolfd = -1;
	ssize_t r = 1;
	uint32_t dline;
	uint32_t dsize = 0;
	uint32_t line = 1;
	uint32_t n;
	uint32_t vsize = 0;
	uint32_t x;
	off_t charpos;
	off_t curpos;

	/* Sanity check: if no options and no unknown function, return */
	if (options == NULL && unknown == NULL) {
		errno = EINVAL;
		return (-1);
	}

	/* Spool input that cannot seek (see bsdconf_spool() above) */
	if (lseek(fd, 0, SEEK_CUR) == -1) {
		if (errno != ESPIPE)
			return (-1);
		if ((spoolfd = bsdconf_spool(fd)) == -1)
			return (-1);
		fd = spoolfd;
	}

	/* Processing options */
	bequals = processing_options & BSDCONF_BREAK_ON_EQUALS;
	bsemicolon = processing_options & BSDCONF_BREAK_ON_SEMICOLON;
	case_sensitive = processing_options & BSDCONF_CASE_SENSITIVE;
	operator_equals = processing_options & BSDCONF_OPERATOR_EQUALS;
	require_equals = processing_options & BSDCONF_REQUIRE_EQUALS;
	strict_equals = processing_options & BSDCONF_STRICT_EQUALS;

	/* Read the file until EOF */
	while (r > 0) {
		r = bsdconf_read1(fd, p);
		if (r < 0)
			goto fail;

		/* Skip to the beginning of a directive */
		while (r > 0 && (isspace((unsigned char)*p) || *p == '#' ||
		    comment || (bsemicolon && *p == ';'))) {
			if (*p == '#')
				comment = 1;
			else if (*p == '\n') {
				comment = 0;
				line++;
			}
			r = bsdconf_read1(fd, p);
			if (r < 0)
				goto fail;
		}
		/* Test for EOF; if EOF then no directive was found */
		if (r == 0)
			goto cleanup;

		/* Record the line number the directive appears on */
		dline = line;

		/* Get the current offset */
		if ((curpos = lseek(fd, 0, SEEK_CUR)) == -1)
			goto fail;
		curpos--;

		/* Find the length of the directive */
		for (n = 0; r > 0; n++) {
			if (isspace((unsigned char)*p))
				break;
			if (bequals && *p == '=') {
				have_equals = 1;
				break;
			}
			if (bsemicolon && *p == ';')
				break;
			r = bsdconf_read1(fd, p);
			if (r < 0)
				goto fail;
		}

		/* Test for EOF, if EOF then no directive was found */
		if (n == 0 && r == 0)
			goto cleanup;

		/* Go back to the beginning of the directive */
		if (lseek(fd, curpos, SEEK_SET) == -1)
			goto fail;

		/*
		 * Allocate and read the directive into memory. The buffer
		 * must be grown on the first pass (directive == NULL) even
		 * when the name is empty (a line beginning with `='), lest
		 * the string terminator below store through a NULL pointer.
		 */
		if (directive == NULL || n > dsize) {
			if ((t = realloc(directive, n + 1)) == NULL)
				goto fail;
			directive = t;
			dsize = n;
		}
		if (bsdconf_readn(fd, directive, n) != 0)
			goto fail;

		/* Advance beyond the equals sign if appropriate/desired */
		if (bequals && *p == '=') {
			if (lseek(fd, 1, SEEK_CUR) != -1) {
				r = bsdconf_read1(fd, p);
				if (r < 0)
					goto fail;
			}
			if (strict_equals && isspace((unsigned char)*p))
				*p = '\n';
		}

		/* Terminate the string */
		directive[n] = '\0';

		/*
		 * Split a make(1)-style operator (`+=' `?=' `:=' `!=') off
		 * the tail of the directive if requested. The operator
		 * character rode along with the directive because only the
		 * `=' terminates the directive scan (above).
		 */
		op = have_equals ? BSDCONF_OP_ASSIGN : BSDCONF_OP_DEFAULT;
		if (operator_equals && have_equals && n > 1) {
			switch (directive[n - 1]) {
			case '+': op = BSDCONF_OP_APPEND; break;
			case '?': op = BSDCONF_OP_COND; break;
			case ':': op = BSDCONF_OP_EXPAND; break;
			case '!': op = BSDCONF_OP_SHELL; break;
			}
			if (op != BSDCONF_OP_ASSIGN)
				directive[--n] = '\0';
		}

		/* Convert directive to lower case before comparison */
		if (!case_sensitive)
			bsdconf_strtolower(directive);

		/* Move to what may be the start of the value */
		if (!(bsemicolon && *p == ';') &&
		    !(strict_equals && *p == '=')) {
			if (bsdconf_skip_hspace(fd, p, &r) != 0)
				goto fail;
		}

		/* An equals sign may have stopped us, should we eat it? */
		if (r > 0 && bequals && *p == '=' && !strict_equals) {
			have_equals = 1;
			r = bsdconf_read1(fd, p);
			if (r < 0)
				goto fail;
			if (bsdconf_skip_hspace(fd, p, &r) != 0)
				goto fail;
		}

		/* If no value, allocate a dummy value and jump to action */
		if (r == 0 || *p == '\n' || *p == '#' ||
		    (bsemicolon && *p == ';')) {
			/* Count the consumed terminator if a newline */
			if (r > 0 && *p == '\n')
				line++;
			/* Flag a trailing comment so it is skipped */
			if (r > 0 && *p == '#')
				comment = 1;
			/* Initialize the value if not already done */
			if (value == NULL && (value = malloc(1)) == NULL)
				goto fail;
			value[0] = '\0';
			goto call_function;
		}

		/* Get the current offset */
		if ((curpos = lseek(fd, 0, SEEK_CUR)) == -1)
			goto fail;
		curpos--;

		/* Find the end of the value */
		if (bsdconf_scan_value_end(fd, p, &r, &line, &comment,
		    &ecomment, bsemicolon) != 0)
			goto fail;

		/* Get the current offset */
		if ((charpos = lseek(fd, 0, SEEK_CUR)) == -1)
			goto fail;

		/* Get the length of the value */
		n = (uint32_t)(charpos - curpos);
		if (r > 0) /* more to read, but don't read ending key */
			n--;

		/* Move offset back to the beginning of the value */
		if (lseek(fd, curpos, SEEK_SET) == -1)
			goto fail;

		/* Allocate and read the value into memory */
		if (n > vsize) {
			if ((t = realloc(value, n + 1)) == NULL)
				goto fail;
			value = t;
			vsize = n;
		}
		if (bsdconf_readn(fd, value, n) != 0)
			goto fail;

		/* Terminate the string */
		value[n] = '\0';

		/* Cut trailing whitespace and a trailing `#' / `;' key */
		t = bsdconf_rtrim_ws(value, value + n);
		t = bsdconf_trim_value_key(value, t, ecomment != 0,
		    bsemicolon);

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
		if (require_equals && !have_equals) {
			errno = EINVAL;
			goto fail;
		}

		found = have_equals = 0; /* reset */

		/*
		 * Report the statement's assignment operator through a
		 * stack-local option when invoking the unknown call-back
		 * (there is no matched options[] slot to hang it on).
		 */
		if (options == NULL && unknown != NULL) {
			error = bsdconf_call_unknown(unknown, op, dline,
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
					    dline, directive, value);
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
			error = bsdconf_call_unknown(unknown, op, dline,
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
	x = errno; /* preserve errno across free(3) and close(2) */
	if (spoolfd != -1)
		close(spoolfd);
	free(directive);
	free(value);
	errno = x;

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
