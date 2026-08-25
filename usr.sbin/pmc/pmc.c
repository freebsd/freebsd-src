/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018, Matthew Macy
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <stddef.h>
#include <stdlib.h>
#include <err.h>
#include <limits.h>
#include <string.h>
#include <pmc.h>
#include <pmclog.h>
#include <libelf.h>
#include <libpmcstat.h>
#include <sysexits.h>
#include <unistd.h>

#include "cmd_pmc.h"

int	pmc_displayheight = DEFAULT_DISPLAY_HEIGHT;
int	pmc_displaywidth = DEFAULT_DISPLAY_WIDTH;
int	pmc_kq;
struct pmcstat_args pmc_args;

struct pmcstat_pmcs pmcstat_pmcs = LIST_HEAD_INITIALIZER(pmcstat_pmcs);

struct pmcstat_image_hash_list pmcstat_image_hash[PMCSTAT_NHASH];

struct pmcstat_process_hash_list pmcstat_process_hash[PMCSTAT_NHASH];

struct cmd_handler {
	const char *ch_name;
	cmd_disp_t ch_fn;
	const char *ch_desc;
};

static struct cmd_handler disp_table[] = {
	{ "filter", cmd_pmc_filter, NULL },
	{ "frontend", cmd_pmc_frontend, "Analyze front-end stalls" },
	{ "info", cmd_pmc_info, "Display high level information about a log" },
	{ "list-events", cmd_pmc_list_events, "List available PMC events" },
	{ "record", cmd_pmc_record, "Record a performance sample" },
	{ "stat", cmd_pmc_stat, NULL },
	{ "stat-system", cmd_pmc_stat_system, NULL },
	{ "summary", cmd_pmc_summary, NULL },
	{ NULL, NULL, NULL }
};

static void
usage(void)
{
	int i;

	fprintf(stderr, "Usage: pmc <COMMAND> [OPTIONS] ...\n\n");
	fprintf(stderr, "PMC tool\n\n");

	fprintf(stderr, "Commands:\n");
	for (i = 0; disp_table[i].ch_name != NULL; i++) {
		// Hide stuff we plan to deprecate
		if (disp_table[i].ch_desc)
			fprintf(stderr, "    %-12s    %s\n",
			    disp_table[i].ch_name, disp_table[i].ch_desc);
	}

	fprintf(stderr, "\nEnvironment Variables:\n");
	fprintf(stderr, "    CLICOLOR        Enables color graphs\n");
	fprintf(stderr, "    SYSROOT         Path to system root\n");
	fprintf(stderr, "    SYMROOT         Path to debug symbols\n");
	fprintf(stderr, "    TERM            Controls symbols and color palettes\n");

	exit(EX_USAGE);
}

static cmd_disp_t
disp_lookup(char *name)
{
	struct cmd_handler *hnd;

	for (hnd = disp_table; hnd->ch_name != NULL; hnd++)
		if (strcmp(hnd->ch_name, name) == 0)
			return (hnd->ch_fn);
	return (NULL);
}

int
main(int argc, char **argv)
{
	cmd_disp_t disp;

	pmc_args.pa_printfile = stderr;
	STAILQ_INIT(&pmc_args.pa_events);
	SLIST_INIT(&pmc_args.pa_targets);
	if (argc == 1)
		usage();
	if ((disp = disp_lookup(argv[1])) == NULL)
		usage();
	argc--;
	argv++;

	/* Allocate a kqueue */
	if ((pmc_kq = kqueue()) < 0)
		err(EX_OSERR, "ERROR: Cannot allocate kqueue");

	if (pmc_init() < 0)
		err(EX_UNAVAILABLE,
		    "ERROR: Initialization of the pmc(3) library failed"
		    );

	if (elf_version(EV_CURRENT) == EV_NONE)
		err(EX_UNAVAILABLE, "ERROR: ELF library version too old");

	return (disp(argc, argv));
}
