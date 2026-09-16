/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Ordered conf-file scanning into effective values, trails, and dumps.
 */

#include <ctype.h>

#include "sysconf_priv.h"

/*
 * Apply a make(1)-style assignment operator to the running effective value
 * of a directive. `+=' appends with a single space (as make(1) does);
 * `?=' keeps the existing value when already defined; `=', `:=', and `!='
 * replace. Returns newly allocated storage, or NULL on allocation failure.
 * `old' may be NULL when the directive has not been seen yet.
 */
char *
make_apply(enum bsdconf_op op, const char *old, const char *piece)
{
	size_t len;
	char *out;

	if (piece == NULL)
		piece = "";

	switch (op) {
	case BSDCONF_OP_APPEND:
		if (old == NULL || *old == '\0')
			return (strdup(piece));
		if (*piece == '\0')
			return (strdup(old));
		len = strlen(old) + 1 + strlen(piece) + 1;
		if ((out = malloc(len)) == NULL)
			return (NULL);
		snprintf(out, len, "%s %s", old, piece);
		return (out);
	case BSDCONF_OP_COND:
		if (old != NULL)
			return (strdup(old));
		return (strdup(piece));
	case BSDCONF_OP_EXPAND:
	case BSDCONF_OP_SHELL:
	case BSDCONF_OP_ASSIGN:
	case BSDCONF_OP_DEFAULT:
	default:
		return (strdup(piece));
	}
}

/*
 * Record `directive'/`value' in the dump union (for `-a'). For formats
 * without make(1) operators a later assignment overrides an earlier one
 * while retaining its original position. With BSDCONF_OPERATOR_EQUALS the
 * operator is honored: `+=' accumulates, `?=' keeps a prior definition,
 * and `=' / `:=' / `!=' replace -- matching make(1)'s effective value.
 * Returns zero on success; -1 (errno set) on allocation failure.
 */
static int
dump_record(const char *directive, const char *value, enum bsdconf_op op,
    uint32_t line)
{
	char *merged;
	char *vcopy;
	size_t n;
	struct dumpent *tmp;

	if ((vcopy = strdup(value)) == NULL)
		return (-1);
	bsdconf_unquote(vcopy);

	for (n = 0; n < ndumps; n++) {
		if (strcmp(dumps[n].name, directive) != 0)
			continue;
		if ((bsdconf_format_processing(format) &
		    BSDCONF_OPERATOR_EQUALS) != 0) {
			merged = make_apply(op, dumps[n].value, vcopy);
			if (merged == NULL) {
				free(vcopy);
				return (-1);
			}
			free(dumps[n].value);
			dumps[n].value = merged;
		} else {
			free(dumps[n].value);
			dumps[n].value = vcopy;
			vcopy = NULL; /* owned by dumps[n].value */
		}
		dumps[n].srcidx = (int)conf_scanidx;
		dumps[n].line = line;
		if (trail_push(&dumps[n].trail, &dumps[n].ntrail,
		    conf_scanidx, line, op,
		    vcopy != NULL ? vcopy : dumps[n].value,
		    dumps[n].value) != 0) {
			free(vcopy);
			return (-1);
		}
		free(vcopy);
		return (0);
	}

	if (ndumps >= dumpsize) {
		dumpsize = (dumpsize == 0) ? 64 : dumpsize << 1;
		tmp = realloc(dumps, dumpsize * sizeof(*dumps));
		if (tmp == NULL) {
			free(vcopy);
			return (-1);
		}
		dumps = tmp;
	}
	if ((dumps[ndumps].name = strdup(directive)) == NULL) {
		free(vcopy);
		return (-1);
	}
	dumps[ndumps].value = vcopy;
	dumps[ndumps].srcidx = (int)conf_scanidx;
	dumps[ndumps].line = line;
	dumps[ndumps].trail = NULL;
	dumps[ndumps].ntrail = 0;
	if (trail_push(&dumps[ndumps].trail, &dumps[ndumps].ntrail,
	    conf_scanidx, line, op, vcopy, vcopy) != 0)
		return (-1);
	ndumps++;
	return (0);
}

/*
 * bsdconf_fparse() call-back; record each statement against the requests
 * (and, with `-a', the dump union). For loader/sysctl/generic a later
 * assignment overrides an earlier one -- both within a file and across the
 * ordered file list. For make(1)-syntax targets the statement's operator
 * (passed via option->op) drives accumulation so `foo=1' followed by
 * `foo+=2' yields the effective value `1 2', as make -V reports.
 */
static int
scan_cb(struct bsdconf_option *option, uint32_t line,
    char *directive, char *value)
{
	char *copy;
	char *merged;
	enum bsdconf_op file_op;
	unsigned int n;

	file_op = (option != NULL) ? option->op : BSDCONF_OP_ASSIGN;
	if (file_op == BSDCONF_OP_DEFAULT)
		file_op = BSDCONF_OP_ASSIGN;

	for (n = 0; n < nreqs; n++) {
		if (strcmp(reqs[n].name, directive) != 0)
			continue;
		/*
		 * The defaults file is read-only: it informs the effective
		 * value seen by reads, checks, and list edits, but can
		 * never satisfy a removal (nothing there may be removed).
		 */
		if (reqs[n].remove && (int)conf_scanidx == defaults_idx)
			continue;
		if ((copy = strdup(value)) == NULL)
			return (-1);
		bsdconf_unquote(copy);

		/*
		 * An identical make(1) `+=value' line already in the file
		 * satisfies a pending `name+=value' write or check.
		 */
		if (reqs[n].op == BSDCONF_OP_APPEND &&
		    reqs[n].newvalue != NULL &&
		    file_op == BSDCONF_OP_APPEND &&
		    strcmp(copy, reqs[n].newvalue) == 0)
			reqs[n].append_present = 1;

		if ((bsdconf_format_processing(format) &
		    BSDCONF_OPERATOR_EQUALS) != 0) {
			if (make_strike_p(&reqs[n]) &&
			    make_assign_push(&reqs[n], file_op, copy,
			    line) != 0) {
				free(copy);
				return (-1);
			}
			merged = make_apply(file_op, reqs[n].value, copy);
			if (merged == NULL) {
				free(copy);
				return (-1);
			}
			free(reqs[n].value);
			reqs[n].value = merged;
			if (trail_push(&reqs[n].trail, &reqs[n].ntrail,
			    conf_scanidx, line, file_op, copy,
			    reqs[n].value) != 0) {
				free(copy);
				return (-1);
			}
			free(copy);
		} else {
			free(reqs[n].value);
			reqs[n].value = copy;
			if (trail_push(&reqs[n].trail, &reqs[n].ntrail,
			    conf_scanidx, line, file_op, copy, copy) != 0)
				return (-1);
		}
		reqs[n].found = 1;
		reqs[n].srcidx = (int)conf_scanidx;
		reqs[n].line = line;
		reqs[n].infile[conf_scanidx] = 1;
	}

	if (dump_all && dump_record(directive, value, file_op, line) != 0)
		return (-1);

	return (0);
}

/*
 * Parse every existing backing file in deterministic order, feeding each
 * statement through scan_cb(). With `sandbox' set (pure read operations),
 * all descriptors are opened up front and the remainder of the pass runs
 * inside a Capsicum sandbox on FreeBSD. Returns EXIT_SUCCESS or exits on
 * hard errors.
 */
int
scan_pass(int sandbox)
{
	int *fds;
	size_t n;
	uint16_t processing;

	if ((fds = calloc(nconf_files, sizeof(*fds))) == NULL)
		err(EXIT_FAILURE, NULL);

	for (n = 0; n < nconf_files; n++) {
		fds[n] = open(conf_files[n], O_RDONLY);
		if (fds[n] < 0 && errno != ENOENT)
			err(EXIT_FAILURE, "%s", conf_files[n]);

		/*
		 * Spool input that cannot seek (a fifo or /dev/stdin named
		 * by `-f') before the sandbox slams shut: the library would
		 * otherwise spool lazily inside bsdconf_fparse(), where
		 * creating the temporary is no longer permitted.
		 */
		if (fds[n] >= 0 && lseek(fds[n], 0, SEEK_CUR) == -1) {
			int sfd;

			if (errno != ESPIPE ||
			    (sfd = bsdconf_spool(fds[n])) == -1)
				err(EXIT_FAILURE, "%s", conf_files[n]);
			close(fds[n]);
			fds[n] = sfd;
		}
	}

#ifdef __FreeBSD__
	if (sandbox) {
		cap_rights_t rights;

		if (caph_limit_stdio() < 0)
			err(EXIT_FAILURE, "capsicum");
		cap_rights_init(&rights, CAP_READ, CAP_FSTAT, CAP_SEEK);
		for (n = 0; n < nconf_files; n++) {
			if (fds[n] >= 0 &&
			    caph_rights_limit(fds[n], &rights) < 0)
				err(EXIT_FAILURE, "capsicum");
		}
		if (caph_enter() < 0)
			err(EXIT_FAILURE, "capsicum");
	}
#else
	(void)sandbox;
#endif

	processing = bsdconf_format_processing(format);
	for (n = 0; n < nconf_files; n++) {
		if (fds[n] < 0)
			continue;
		conf_scanidx = n;
		if (bsdconf_fparse(NULL, fds[n], scan_cb, processing) != 0)
			err(EXIT_FAILURE, "%s", conf_files[n]);
		close(fds[n]);
	}

	free(fds);
	return (EXIT_SUCCESS);
}
