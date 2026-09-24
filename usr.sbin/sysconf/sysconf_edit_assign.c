/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * CLI assignment parsing and sysrc(8)-style list word edits.
 */

#include <ctype.h>

#include "sysconf_priv.h"

/*
 * Split a `name[=value]' argument into a request. With `remove' set, the
 * argument is a bare name to delete. For make(1)-style targets, a trailing
 * operator character on the name (`+', `?', `:', `!') selects the
 * corresponding assignment modifier: `+=' appends a new `name+=value' line
 * (make accumulates; we never rewrite a prior assignment), while `=', `?=',
 * `:=', and `!=' rewrite or add a single assignment as today. `name-=word'
 * is a list strike: the last assignment whose value contains each word is
 * rewritten (or removed if emptied); see do_make_strikes(). For every other
 * target, `name+=word' and `name-=word' are sysrc(8)-style list edits
 * against the effective value (see merge_list_requests() below).
 */
void
split_request(char *arg, struct request *req, int remove)
{
	char *eq;
	size_t n;

	req->name = arg;
	req->newvalue = NULL;
	req->op = BSDCONF_OP_DEFAULT;
	req->listop = '\0';
	req->value = NULL;
	req->found = 0;
	req->append_present = 0;
	req->remove = remove;
	req->srcidx = -1;
	req->line = 0;
	req->trail = NULL;
	req->ntrail = 0;
	req->assigns = NULL;
	req->nassigns = 0;
	if ((req->infile = calloc(nconf_files, 1)) == NULL)
		err(EXIT_FAILURE, NULL);

	if (remove) {
		if (strchr(arg, '=') != NULL)
			errx(EXIT_FAILURE,
			    "%s: -x does not take a value", arg);
		return;
	}

	if ((eq = strchr(arg, '=')) == NULL)
		return;

	*eq = '\0';

	/*
	 * Strip one balanced layer of double-quotes from the value as a
	 * courtesy (e.g., a literal zfs_load="YES" from a quoted shell
	 * word); the library re-applies the target format's quoting rules
	 * on output.
	 */
	req->newvalue = bsdconf_unquote(eq + 1);

	/* Split an operator character off the tail of the name */
	n = strlen(arg);
	if ((bsdconf_format_processing(format) &
	    BSDCONF_OPERATOR_EQUALS) != 0 && n > 1) {
		switch (arg[n - 1]) {
		case '+': req->op = BSDCONF_OP_APPEND; break;
		case '?': req->op = BSDCONF_OP_COND; break;
		case ':': req->op = BSDCONF_OP_EXPAND; break;
		case '!': req->op = BSDCONF_OP_SHELL; break;
		case '-':
			/* make(1) has no `-='; treat as a list strike */
			req->listop = '-';
			arg[n - 1] = '\0';
			break;
		}
		if (req->op != BSDCONF_OP_DEFAULT)
			arg[n - 1] = '\0';
	} else if (n > 1 && (arg[n - 1] == '+' || arg[n - 1] == '-')) {
		req->listop = arg[n - 1];
		arg[n - 1] = '\0';
	}

	if (*arg == '\0')
		errx(EXIT_FAILURE, "empty directive name");
}

/* True when `req' is a make/src `name-=word' strike (not a sysrc list edit). */
/*
 * Test whether the whitespace-separated `list' contains `word' (of length
 * `wlen') as a whole word.
 */
int
list_has(const char *list, const char *word, size_t wlen)
{
	size_t len;

	if (list == NULL)
		return (0);
	while (*list != '\0') {
		list += strspn(list, " \t");
		if ((len = strcspn(list, " \t")) == 0)
			break;
		if (len == wlen && strncmp(list, word, wlen) == 0)
			return (1);
		list += len;
	}

	return (0);
}

/*
 * Merge the whitespace-separated `words' into (op `+') or out of (op `-')
 * the whitespace-separated list `current' (NULL for an unset directive),
 * appending only words not already present and joining the survivors with
 * single spaces. Returns newly allocated storage.
 */
char *
list_merge(const char *current, const char *words, int op)
{
	size_t len;
	size_t rlen = 0;
	size_t size;
	char *result;
	const char *token;

	if (current == NULL)
		current = "";
	size = strlen(current) + strlen(words) + 2;
	if ((result = malloc(size)) == NULL)
		err(EXIT_FAILURE, NULL);
	result[0] = '\0';

	/* Copy the current words, striking matches under `-' */
	token = current;
	while (*token != '\0') {
		token += strspn(token, " \t");
		if ((len = strcspn(token, " \t")) == 0)
			break;
		if (op == '-' && list_has(words, token, len)) {
			token += len;
			continue;
		}
		if (rlen > 0)
			result[rlen++] = ' ';
		memcpy(result + rlen, token, len);
		rlen += len;
		result[rlen] = '\0';
		token += len;
	}

	/* Append the new words not already present under `+' */
	if (op == '+') {
		token = words;
		while (*token != '\0') {
			token += strspn(token, " \t");
			if ((len = strcspn(token, " \t")) == 0)
				break;
			if (!list_has(result, token, len)) {
				if (rlen > 0)
					result[rlen++] = ' ';
				memcpy(result + rlen, token, len);
				rlen += len;
				result[rlen] = '\0';
			}
			token += len;
		}
	}

	return (result);
}

/*
 * Materialize sysrc(8)-style list-edit requests (`name+=word ...' and
 * `name-=word ...') against the effective values recorded by scan_pass(),
 * rewriting each request's pending value in place so the ordinary write,
 * check, and echo machinery sees a plain assignment. Make/src `name-=word'
 * strikes are handled separately by do_make_strikes(). Appending to an
 * unset directive creates it; striking words from one is reported like any
 * other unknown directive (there is nothing to remove) and the request is
 * neutralized. Returns EXIT_SUCCESS or EXIT_FAILURE.
 */
int
merge_list_requests(void)
{
	int rv = EXIT_SUCCESS;
	unsigned int n;

	for (n = 0; n < nreqs; n++) {
		if (reqs[n].listop == '\0' || reqs[n].newvalue == NULL)
			continue;
		/* make/src `-=` is a per-statement strike, not a list merge */
		if (make_strike_p(&reqs[n]))
			continue;
		if (reqs[n].listop == '-' && !reqs[n].found) {
			if (!ignore_unknown) {
				if (!quiet)
					warnx("unknown directive '%s'",
					    reqs[n].name);
				rv = EXIT_FAILURE;
			}
			/* Convert to a no-op */
			reqs[n].newvalue = NULL;
			reqs[n].name = "";
			continue;
		}
		reqs[n].newvalue = list_merge(reqs[n].value,
		    reqs[n].newvalue, reqs[n].listop);
	}

	return (rv);
}
