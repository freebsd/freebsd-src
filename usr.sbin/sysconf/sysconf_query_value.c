/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Report effective directive values (named reads and -a).
 */

#include "sysconf_priv.h"

/*
 * Report read requests (and `-a') from the results of a completed
 * scan_pass(). Returns EXIT_SUCCESS or EXIT_FAILURE.
 */
int
do_reads(void)
{
	int rv = EXIT_SUCCESS;
	size_t d;
	unsigned int n;

	if (dump_all) {
		for (d = 0; d < ndumps; d++) {
			/*
			 * Without -A, a plain -a dumps only what the conf
			 * files themselves configure; a directive whose
			 * value still comes from the (always-scanned)
			 * defaults file is out of scope.
			 */
			if (!with_defaults && defaults_idx >= 0 &&
			    dumps[d].srcidx == defaults_idx)
				continue;
			if (!quiet)
				warn_with_equals_no(dumps[d].name,
				    dumps[d].value,
				    conf_files[dumps[d].srcidx],
				    dumps[d].line);
			if (show_desc)
				print_desc(dumps[d].name);
			else if (verbose_trail)
				print_trail(dumps[d].name, dumps[d].trail,
				    dumps[d].ntrail);
			else
				print_pair(conf_files[dumps[d].srcidx],
				    dumps[d].name, dumps[d].value);
		}
		return (EXIT_SUCCESS);
	}

	/* Report the results in the order requested */
	for (n = 0; n < nreqs; n++) {
		if (reqs[n].newvalue != NULL || reqs[n].remove ||
		    reqs[n].name[0] == '\0') /* neutralized by validation */
			continue;
		if (!reqs[n].found) {
			if (ignore_unknown)
				continue;
			if (!quiet)
				warnx("unknown directive '%s'",
				    reqs[n].name);
			rv = EXIT_FAILURE;
			continue;
		}
		if (!quiet)
			warn_with_equals_no(reqs[n].name, reqs[n].value,
			    conf_files[reqs[n].srcidx], reqs[n].line);
		if (verbose_trail)
			print_trail(reqs[n].name, reqs[n].trail,
			    reqs[n].ntrail);
		else
			print_pair(conf_files[reqs[n].srcidx], reqs[n].name,
			    reqs[n].value);
	}

	return (rv);
}
