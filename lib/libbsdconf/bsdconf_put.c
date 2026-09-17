/*
 * Copyright (c) 2015-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bsdconf.h"
#include "bsdconf_internal.h"

/*
 * Rewrite the configuration file at `path', applying the per-directive
 * actions described by the array of config options (first argument). Each
 * option's action is one of:
 *
 * BSDCONF_ACTION_SET_VALUE	set the directive to option->value.str,
 *				editing it in place if present or appending
 *				it to the file if absent;
 * BSDCONF_ACTION_REMOVE	delete the directive from the file;
 * BSDCONF_ACTION_CHECK		report (via option->result) whether the
 *				current value differs from option->value,
 *				without modifying the file.
 *
 * Comments, blank lines, statement ordering, and the formatting of untouched
 * statements are preserved. With BSDCONF_OPERATOR_EQUALS, a non-default
 * option->op both matches and rewrites the statement's assignment operator
 * (make.conf(5) `+=' et al.). For each processed option, option->result is
 * set to a bitmask of BSDCONF_DIRECTIVE_FOUND, BSDCONF_VALUE_CHANGED,
 * BSDCONF_DIRECTIVE_ADDED, and BSDCONF_DIRECTIVE_REMOVED, and option->line
 * is set to the line of the first match (if found).
 *
 * When every option is BSDCONF_ACTION_CHECK, or every SET_VALUE/REMOVE would
 * leave the file unchanged, the original is left untouched: no temporary is
 * created, mtime is not bumped, and hard links are not severed. Otherwise
 * the file is replaced atomically: output is written to a temporary file in
 * the same directory, flushed to disk with fsync(2), given the original's
 * mode masked to 0666 (and, if permitted, its ownership), and then renamed
 * over it. A crash mid-transaction leaves the original untouched.
 *
 * Returns zero on success; otherwise returns -1 and errno should be
 * consulted.
 */
int
bsdconf_put(struct bsdconf_option options[], const char *path,
    uint16_t processing_options, uint16_t put_options)
{
	bool backup;
	bool bequals;
	bool bsemicolon;
	bool case_sensitive;
	bool emptyok;
	bool nodup;
	bool operator_equals;
	bool quote_always;
	bool require_equals;
	bool strict_equals;
	bool unquoted;
	int dirfd = -1;
	int fd = -1;
	int last_ch = -1; /* last byte emitted, or -1 if none */
	int opchange;
	int rv = -1;
	int saved_errno;
	int tmpfd = -1;
	char *buf = NULL;
	char *slash;
	char *val = NULL;
	const char *sep;
	size_t buflen = 0;
	size_t i;
	size_t n;
	size_t vlen;
	size_t wc; /* write cursor: next unemitted byte of buf */
	uint32_t line = 1;
	struct bsdconf_option *option;
	struct bsdconf_stmt st;
	struct stat sb;
	char rpath[PATH_MAX];
	char tpath[PATH_MAX];

	/* Sanity check the arguments */
	if (options == NULL || path == NULL) {
		errno = EINVAL;
		return (-1);
	}

	/* Nothing to unlink until mkstemp(3) succeeds (see cleanup) */
	tpath[0] = '\0';

	/* Decode processing options */
	bequals = processing_options & BSDCONF_BREAK_ON_EQUALS;
	bsemicolon = processing_options & BSDCONF_BREAK_ON_SEMICOLON;
	case_sensitive = processing_options & BSDCONF_CASE_SENSITIVE;
	operator_equals = processing_options & BSDCONF_OPERATOR_EQUALS;
	require_equals = processing_options & BSDCONF_REQUIRE_EQUALS;
	strict_equals = processing_options & BSDCONF_STRICT_EQUALS;

	/* Decode put options */
	backup = put_options & BSDCONF_PUT_BACKUP;
	emptyok = put_options & BSDCONF_PUT_ALLOW_EMPTY;
	nodup = put_options & BSDCONF_PUT_NO_DUPLICATES;
	quote_always = put_options & BSDCONF_PUT_QUOTE_ALWAYS;
	unquoted = put_options & BSDCONF_PUT_UNQUOTED;

	/* Quoting directives are mutually exclusive */
	if (unquoted && quote_always) {
		errno = EINVAL;
		return (-1);
	}

	/* Reset per-option results and reject empty values up front */
	for (n = 0; options[n].directive != NULL; n++) {
		options[n].result = 0;
		options[n].line = 0;
		if (!emptyok && options[n].action == BSDCONF_ACTION_SET_VALUE &&
		    bsdconf_value_empty(&options[n])) {
			errno = EINVAL;
			return (-1);
		}
	}

	/* Resolve the file path */
	if (realpath(path, rpath) == NULL)
		return (-1);

	/*
	 * Open the original for reading. Only a regular file can be
	 * atomically replaced, so anything else is rejected below;
	 * O_NONBLOCK makes that check reachable (it is a no-op for regular
	 * files, but without it opening a fifo blocks awaiting a writer).
	 */
	if ((fd = open(rpath, O_RDONLY | O_NONBLOCK)) < 0)
		return (-1);
	if (fstat(fd, &sb) != 0)
		goto cleanup;
	if (!S_ISREG(sb.st_mode)) {
		errno = EINVAL;
		goto cleanup;
	}

	/* Slurp the original into memory */
	if ((buf = bsdconf_readfile(fd, (size_t)sb.st_size, &buflen)) == NULL)
		goto cleanup;

	/*
	 * Walk the original statement by statement. The replacement temporary
	 * is created lazily on the first real edit (see bsdconf_ensure_tmp());
	 * CHECK and equal-value paths never open it. Bytes are emitted in
	 * order via the write cursor `wc'; a matched statement diverts
	 * around the region it edits or removes.
	 */
	wc = 0;
	i = 0;
	while (bsdconf_scan(buf, buflen, &i, &line, bequals, bsemicolon,
	    strict_equals, operator_equals, &st)) {
		/* Statements without an `=' are unwritable targets */
		if (require_equals && !st.have_equals)
			continue;

		/* Locate the option (if any) matching this directive */
		option = NULL;
		for (n = 0; options[n].directive != NULL; n++) {
			if (!bsdconf_dir_matches(buf, st.dir_start, st.dir_end,
			    options[n].directive, case_sensitive))
				continue;
			/*
			 * A non-zero match_line selects one physical
			 * statement (e.g. make(1) `+=' strike/edit of the
			 * last assignment containing a word).
			 */
			if (options[n].match_line != 0 &&
			    options[n].match_line != st.line)
				continue;
			option = &options[n];
			break;
		}
		if (option == NULL)
			continue; /* untouched; bytes flushed later */

		/*
		 * make(1)-style `+=': when match_line is unset, never rewrite
		 * an existing statement and do not mark the directive FOUND,
		 * so a fresh `name+=value' line is appended below. Existing
		 * assignments for the same name are left intact so make's
		 * cumulative semantics are preserved. With match_line set,
		 * rewrite (or remove) that specific statement instead. The
		 * caller (sysconf(8)) skips the put entirely when an
		 * identical `+=value' line is already present.
		 */
		if (operator_equals &&
		    option->action == BSDCONF_ACTION_SET_VALUE &&
		    option->op == BSDCONF_OP_APPEND &&
		    option->match_line == 0)
			continue;

		/* Enforce the no-duplicates policy */
		if (nodup && (option->result & BSDCONF_DIRECTIVE_FOUND) != 0) {
			errno = EEXIST;
			goto cleanup;
		}
		if ((option->result & BSDCONF_DIRECTIVE_FOUND) == 0)
			option->line = st.line;
		option->result |= BSDCONF_DIRECTIVE_FOUND;

		if (option->action == BSDCONF_ACTION_REMOVE) {
			size_t rm_start;
			size_t rm_end;
			size_t k;
			int done = 0;
			int first_on_line = 1;

			/*
			 * Is this the first statement on its physical line?
			 * With BSDCONF_BREAK_ON_SEMICOLON an earlier
			 * statement may precede it on the same line.
			 */
			for (k = st.line_start; k < st.dir_start; k++)
				if (!isspace((unsigned char)buf[k])) {
					first_on_line = 0;
					break;
				}

			/*
			 * If a further statement follows on the same line
			 * (terminator is a semicolon with a directive after
			 * it), drop this statement and the trailing
			 * separator, leaving the neighbour intact.
			 */
			if (bsemicolon && st.term < buflen &&
			    buf[st.term] == ';') {
				size_t f = st.term + 1;

				while (f < buflen &&
				    isspace((unsigned char)buf[f]) &&
				    buf[f] != '\n')
					f++;
				if (f < buflen && buf[f] != '\n' &&
				    buf[f] != '#') {
					rm_start = st.dir_start;
					rm_end = f;
					done = 1;
				}
			}

			if (!done && first_on_line) {
				/* Alone on the line: drop the whole line */
				rm_start = st.line_start;
				rm_end = st.line_end;
			} else if (!done) {
				/*
				 * Last on a shared line: drop the preceding
				 * separator (whitespace, `;', whitespace)
				 * along with this statement, keeping the
				 * terminator.
				 */
				size_t s = st.dir_start;

				while (s > st.line_start &&
				    isspace((unsigned char)buf[s - 1]))
					s--;
				if (s > st.line_start && buf[s - 1] == ';')
					s--;
				while (s > st.line_start &&
				    isspace((unsigned char)buf[s - 1]))
					s--;
				rm_start = s;
				rm_end = st.term;
			}

			/* Never rewind before already-emitted bytes */
			if (rm_start < wc)
				rm_start = wc;
			if (bsdconf_ensure_tmp(&tmpfd, tpath, sizeof(tpath),
			    rpath, &sb) != 0)
				goto cleanup;
			if (bsdconf_emit(tmpfd, buf + wc, rm_start - wc,
			    &last_ch) != 0)
				goto cleanup;
			if (rm_end > wc)
				wc = rm_end;
			option->result |= BSDCONF_DIRECTIVE_REMOVED;
			continue;
		}

		/* SET_VALUE and CHECK both need the formatted value */
		free(val);
		if ((val = bsdconf_format_value(option, unquoted,
		    quote_always, bsemicolon)) == NULL)
			goto cleanup;
		vlen = strlen(val);

		/* Does the assignment operator need to be rewritten? */
		opchange = operator_equals &&
		    option->op != BSDCONF_OP_DEFAULT && st.have_equals &&
		    st.op != option->op;

		/* Compare against the value currently in the file */
		n = st.val_end - st.val_start;
		if (!opchange && vlen == n && (n == 0 ||
		    memcmp(val, buf + st.val_start, n) == 0)) {
			/* Unchanged; nothing to do for either action */
			continue;
		}

		if (option->action == BSDCONF_ACTION_CHECK) {
			option->result |= BSDCONF_VALUE_CHANGED;
			continue;
		}

		/* BSDCONF_ACTION_SET_VALUE and an edit is required */
		if (bsdconf_ensure_tmp(&tmpfd, tpath, sizeof(tpath),
		    rpath, &sb) != 0)
			goto cleanup;
		if (opchange) {
			/* Rewrite the operator token in place */
			if (bsdconf_emit(tmpfd, buf + wc, st.op_start - wc,
			    &last_ch) != 0)
				goto cleanup;
			if (bsdconf_emit(tmpfd, bsdconf_op_token(option->op),
			    strlen(bsdconf_op_token(option->op)),
			    &last_ch) != 0)
				goto cleanup;
			wc = st.eq + 1;
		}

		if (st.have_value) {
			/* Replace the existing value in place */
			if (bsdconf_emit(tmpfd, buf + wc, st.val_start - wc,
			    &last_ch) != 0)
				goto cleanup;
			if (bsdconf_emit(tmpfd, val, vlen, &last_ch) != 0)
				goto cleanup;
			wc = st.val_end;
		} else if (option->type != BSDCONF_TYPE_NONE) {
			/* Insert a value onto a value-less directive */
			if (bsdconf_emit(tmpfd, buf + wc, st.val_start - wc,
			    &last_ch) != 0)
				goto cleanup;
			/* Supply a separator unless one already precedes */
			if (st.val_start == wc ||
			    (buf[st.val_start - 1] != '=' &&
			    !isspace((unsigned char)buf[st.val_start - 1]))) {
				sep = st.have_equals || require_equals ||
				    bequals ? "=" : " ";
				if (bsdconf_emit(tmpfd, sep, 1,
				    &last_ch) != 0)
					goto cleanup;
			}
			if (bsdconf_emit(tmpfd, val, vlen, &last_ch) != 0)
				goto cleanup;
			wc = st.val_start;
		}
		option->result |= BSDCONF_VALUE_CHANGED;
	}

	/*
	 * Append any SET_VALUE directives that were not found, and finalize
	 * the result flags for CHECK directives that were absent. The tail of
	 * the original is flushed only when a temporary already exists or an
	 * append forces one into being.
	 */
	for (n = 0; options[n].directive != NULL; n++) {
		option = &options[n];
		if ((option->result & BSDCONF_DIRECTIVE_FOUND) != 0)
			continue;
		if (option->action == BSDCONF_ACTION_CHECK) {
			/* Absent means it differs from the desired value */
			option->result |= BSDCONF_VALUE_CHANGED;
			continue;
		}
		if (option->action != BSDCONF_ACTION_SET_VALUE)
			continue; /* REMOVE of an absent directive: no-op */

		if (bsdconf_ensure_tmp(&tmpfd, tpath, sizeof(tpath),
		    rpath, &sb) != 0)
			goto cleanup;
		/* Flush any unread prefix once before the first append */
		if (wc < buflen) {
			if (bsdconf_emit(tmpfd, buf + wc, buflen - wc,
			    &last_ch) != 0)
				goto cleanup;
			wc = buflen;
		}

		/* Ensure the emitted output ends with a newline first */
		if (last_ch != -1 && last_ch != '\n') {
			if (bsdconf_emit(tmpfd, "\n", 1, &last_ch) != 0)
				goto cleanup;
		}

		if (bsdconf_emit(tmpfd, option->directive,
		    strlen(option->directive), &last_ch) != 0)
			goto cleanup;
		if (option->type != BSDCONF_TYPE_NONE) {
			free(val);
			if ((val = bsdconf_format_value(option, unquoted,
			    quote_always, bsemicolon)) == NULL)
				goto cleanup;
			if (operator_equals &&
			    option->op != BSDCONF_OP_DEFAULT)
				sep = bsdconf_op_token(option->op);
			else
				sep = (bequals || require_equals) ? "=" : " ";
			if (bsdconf_emit(tmpfd, sep, strlen(sep),
			    &last_ch) != 0)
				goto cleanup;
			if (bsdconf_emit(tmpfd, val, strlen(val),
			    &last_ch) != 0)
				goto cleanup;
		}
		if (bsdconf_emit(tmpfd, "\n", 1, &last_ch) != 0)
			goto cleanup;
		/*
		 * Line number of the statement appended at EOF: one past
		 * the number of newlines in the original, or the next line
		 * if we have to emit a separating newline first.
		 */
		{
			uint32_t al = 1;
			size_t k;

			for (k = 0; k < buflen; k++)
				if (buf[k] == '\n')
					al++;
			if (buflen > 0 && buf[buflen - 1] != '\n')
				al++;
			option->line = al;
		}
		option->result |=
		    BSDCONF_DIRECTIVE_ADDED | BSDCONF_VALUE_CHANGED;
	}

	/*
	 * Nothing changed: every option was CHECK or an equal-value
	 * SET_VALUE/REMOVE-miss. Leave the original file alone.
	 */
	if (tmpfd < 0) {
		rv = 0;
		goto cleanup;
	}

	/* Flush any remaining unread tail of the original */
	if (bsdconf_emit(tmpfd, buf + wc, buflen - wc, &last_ch) != 0)
		goto cleanup;

	/* Optionally back up the original before replacing it */
	if (backup) {
		char bpath[PATH_MAX];
		int bfd;

		if (snprintf(bpath, sizeof(bpath), "%s.bak", rpath) >=
		    (int)sizeof(bpath)) {
			errno = ENAMETOOLONG;
			goto cleanup;
		}
		/*
		 * O_NOFOLLOW: refuse to follow a planted symbolic link
		 * lest the backup clobber whatever it points at.
		 */
		if ((bfd = open(bpath, O_WRONLY | O_CREAT | O_TRUNC |
		    O_NOFOLLOW, 0600)) < 0)
			goto cleanup;
		if (fchmod(bfd, sb.st_mode & 0777) != 0) {
			saved_errno = errno;
			close(bfd);
			errno = saved_errno;
			goto cleanup;
		}
		if (bsdconf_writeall(bfd, buf, buflen) != 0) {
			saved_errno = errno;
			close(bfd);
			errno = saved_errno;
			goto cleanup;
		}
		if (close(bfd) != 0)
			goto cleanup;
	}

	/* Commit: flush to disk, then atomically replace the original */
	if (fsync(tmpfd) != 0)
		goto cleanup;
	if (close(tmpfd) != 0) {
		tmpfd = -1;
		goto cleanup;
	}
	tmpfd = -1;
	if (rename(tpath, rpath) != 0)
		goto cleanup;
	tpath[0] = '\0'; /* renamed away; nothing to unlink */

	/* Best-effort: persist the directory entry change */
	if ((slash = strrchr(rpath, '/')) != NULL) {
		char dpath[PATH_MAX];
		size_t dlen = (size_t)(slash - rpath);

		if (dlen == 0)
			dlen = 1; /* the root directory */
		if (dlen < sizeof(dpath)) {
			memcpy(dpath, rpath, dlen);
			dpath[dlen] = '\0';
			if ((dirfd = open(dpath, O_RDONLY)) >= 0) {
				(void)fsync(dirfd);
				close(dirfd);
				dirfd = -1;
			}
		}
	}

	rv = 0;

cleanup:
	saved_errno = errno;
	if (fd >= 0)
		close(fd);
	if (tmpfd >= 0)
		close(tmpfd);
	if (dirfd >= 0)
		close(dirfd);
	if (rv != 0 && tpath[0] != '\0')
		(void)unlink(tpath); /* discard the incomplete temporary */
	free(buf);
	free(val);
	errno = saved_errno;
	return (rv);
}
