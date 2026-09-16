/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Apply or check assignments against conf files (bsdconf_put path).
 */

#include <ctype.h>

#include "sysconf_priv.h"

/*
 * Process `-c' from the results of a completed scan_pass() without touching
 * any file: a write request differs when the authoritative value does not
 * match, a make(1) `+=value' request differs when no identical `+=value'
 * line is already present, and a removal request differs when the
 * directive is still present. Returns EXIT_SUCCESS when no changes would
 * be required.
 */
int
do_checks(void)
{
	int differs;
	int rv = EXIT_SUCCESS;
	unsigned int n;

	for (n = 0; n < nreqs; n++) {
		if (reqs[n].newvalue == NULL && !reqs[n].remove)
			continue;
		if (reqs[n].remove)
			differs = reqs[n].found;
		else if (reqs[n].op == BSDCONF_OP_APPEND)
			differs = !reqs[n].append_present;
		else
			differs = !reqs[n].found ||
			    strcmp(reqs[n].value, reqs[n].newvalue) != 0;
		if (differs) {
			if (!quiet)
				warnx("%s: value differs", reqs[n].name);
			rv = EXIT_FAILURE;
		}
	}

	return (rv);
}

/*
 * Process write and remove requests using the srcidx/infile maps recorded
 * by scan_pass(). A directive is rewritten in the file that authoritatively
 * defines it (the last file in sourcing order that lists it) so the change
 * survives a reboot; a directive defined nowhere is appended to the
 * format's default file. Removals are applied to every file listing the
 * directive, lest an earlier definition be unmasked. Each modified file is
 * replaced in one atomic bsdconf_put() transaction. Returns EXIT_SUCCESS
 * or EXIT_FAILURE.
 */
int
do_writes(void)
{
	int fd;
	int rv = EXIT_SUCCESS;
	size_t f;
	unsigned int n;
	unsigned int nopts;
	struct bsdconf_option *opt;
	struct bsdconf_option *options;
	struct request **owner;

	if ((options = calloc(nreqs + 1, sizeof(*options))) == NULL ||
	    (owner = calloc(nreqs, sizeof(*owner))) == NULL)
		err(EXIT_FAILURE, NULL);

	for (f = 0; f < nconf_files; f++) {
		/* Collect the requests this file is responsible for */
		nopts = 0;
		for (n = 0; n < nreqs; n++) {
			size_t target;

			if (reqs[n].remove) {
				if (!reqs[n].infile[f])
					continue;
			} else if (reqs[n].newvalue != NULL) {
				/*
				 * A directive whose only definition is
				 * the read-only defaults file is written
				 * like one defined nowhere: appended to
				 * the format's default write target.
				 */
				if (reqs[n].srcidx < 0 ||
				    reqs[n].srcidx == defaults_idx)
					target = write_idx;
				else
					target = (size_t)reqs[n].srcidx;
				if (target != f)
					continue;
				/*
				 * An identical make(1) `+=value' line is
				 * already present: nothing to write, but
				 * still report the effective value unless
				 * -q (see report loop for the SET case).
				 */
				if (reqs[n].op == BSDCONF_OP_APPEND &&
				    reqs[n].append_present) {
					if (!quiet)
						print_write_echo(conf_files[f],
						    0, reqs[n].name,
						    BSDCONF_OP_APPEND,
						    reqs[n].value,
						    reqs[n].value, 0, 0, 0);
					continue;
				}
				/*
				 * -s only ensures presence: if the
				 * WITH_/WITHOUT_ knob is already defined
				 * (any value), leave the file alone.
				 * An explicit name= (including empty)
				 * always writes the requested value.
				 */
				if (set_knobs && make_knob_p(reqs[n].name) &&
				    reqs[n].found) {
					if (!quiet)
						print_write_echo(conf_files[f],
						    0, reqs[n].name,
						    reqs[n].op,
						    reqs[n].value,
						    reqs[n].value, 0, 0,
						    0);
					continue;
				}
			} else
				continue;
			opt = &options[nopts];
			memset(opt, 0, sizeof(*opt));
			opt->type = BSDCONF_TYPE_STR;
			opt->directive = reqs[n].name;
			opt->op = reqs[n].op;
			if (reqs[n].remove) {
				opt->action = BSDCONF_ACTION_REMOVE;
				opt->value.str = NULL;
			} else {
				opt->action = BSDCONF_ACTION_SET_VALUE;
				opt->value.str =
				    (char *)(uintptr_t)reqs[n].newvalue;
			}
			owner[nopts++] = &reqs[n];
		}
		if (nopts == 0)
			continue;
		memset(&options[nopts], 0, sizeof(options[nopts]));

		/*
		 * Create the target if it does not yet exist. O_EXCL makes
		 * the test-and-create atomic: an existing file (or one that
		 * appears concurrently) is left untouched.
		 */
		if ((fd = open(conf_files[f], O_WRONLY | O_CREAT | O_EXCL,
		    0666)) >= 0)
			close(fd);
		else if (errno != EEXIST)
			err(EXIT_FAILURE, "%s", conf_files[f]);

		if (bsdconf_put(options, conf_files[f],
		    bsdconf_format_processing(format),
		    bsdconf_format_put(format)) != 0)
			err(EXIT_FAILURE, "%s", conf_files[f]);

		/* Report the results */
		for (n = 0; n < nopts; n++) {
			opt = &options[n];
			if (opt->action == BSDCONF_ACTION_REMOVE) {
				if ((opt->result &
				    BSDCONF_DIRECTIVE_REMOVED) != 0) {
					owner[n]->found = 1;
					/*
					 * Removals are quiet unless -v/-V
					 * (or -q which is already silent).
					 */
					if (!quiet &&
					    (verbose || verbose_trail))
						print_write_echo(
						    conf_files[f],
						    opt->line,
						    opt->directive,
						    opt->op, "", "", 0, 0,
						    1);
				}
				continue;
			}
			if (!quiet) {
				const char *old = owner[n]->value;
				char *neweff = NULL;
				const char *shown;

				/*
				 * Gate the echo on whether bsdconf_put()
				 * actually edited the file
				 * (BSDCONF_VALUE_CHANGED). With -V, shape the
				 * echo like a read trail
				 * (`file:line: name=old -> name=new'). For
				 * make(1) `+=', show the effective value
				 * before and after the append rather than
				 * the fragment alone.
				 */
				if (opt->op == BSDCONF_OP_APPEND) {
					neweff = make_apply(BSDCONF_OP_APPEND,
					    old, opt->value.str);
					if (neweff == NULL)
						err(EXIT_FAILURE, NULL);
					shown = neweff;
				} else
					shown = opt->value.str;
				/*
				 * -V shows statement shape at file:line; a
				 * newly added statement is `(added)' rather
				 * than `op -> op+value'. -v keeps effective
				 * old -> new for `+='.
				 */
				if (verbose_trail &&
				    (opt->result &
				    BSDCONF_DIRECTIVE_ADDED) != 0)
					print_write_echo(conf_files[f],
					    opt->line, opt->directive, opt->op,
					    "", opt->value.str,
					    (opt->result &
					    BSDCONF_VALUE_CHANGED) != 0, 1, 0);
				else if (verbose_trail)
					print_write_echo(conf_files[f],
					    opt->line, opt->directive, opt->op,
					    old == NULL ? "" : old, shown,
					    (opt->result &
					    BSDCONF_VALUE_CHANGED) != 0, 0, 0);
				else
					print_write_echo(conf_files[f],
					    opt->line, opt->directive, opt->op,
					    old, shown,
					    (opt->result &
					    BSDCONF_VALUE_CHANGED) != 0, 0, 0);
				if (neweff != NULL) {
					free(owner[n]->value);
					owner[n]->value = neweff;
				}
			}
		}
	}

	/* A removal that matched no file at all is an unknown directive */
	for (n = 0; n < nreqs; n++) {
		if (!reqs[n].remove || reqs[n].found)
			continue;
		if (ignore_unknown)
			continue;
		if (!quiet)
			warnx("unknown directive '%s'", reqs[n].name);
		rv = EXIT_FAILURE;
	}

	free(options);
	free(owner);
	return (rv);
}
