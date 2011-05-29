/*-
 * Copyright (c) 2008, 2009 Yahoo!, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 * $FreeBSD$
 */

#include <sys/errno.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mfiutil.h"

SET_DECLARE(MFI_DATASET(top), struct mfiutil_command);

MFI_TABLE(top, start);
MFI_TABLE(top, stop);
MFI_TABLE(top, abort);

int mfi_unit;

static void
usage(void)
{

	fprintf(stderr, "usage: mfiutil [-u unit] <command> ...\n\n");
	fprintf(stderr, "Commands include:\n");
	fprintf(stderr, "    version\n");
	fprintf(stderr, "    show adapter              - display controller information\n");
	fprintf(stderr, "    show battery              - display battery information\n");
	fprintf(stderr, "    show config               - display RAID configuration\n");
	fprintf(stderr, "    show drives               - list physical drives\n");
	fprintf(stderr, "    show events               - display event log\n");
	fprintf(stderr, "    show firmware             - list firmware images\n");
	fprintf(stderr, "    show volumes              - list logical volumes\n");
	fprintf(stderr, "    show patrol               - display patrol read status\n");
	fprintf(stderr, "    show progress             - display status of active operations\n");
	fprintf(stderr, "    fail <drive>              - fail a physical drive\n");
	fprintf(stderr, "    good <drive>              - mark a bad physical drive as good\n");
	fprintf(stderr, "    rebuild <drive>           - mark failed drive ready for rebuild\n");
	fprintf(stderr, "    drive progress <drive>    - display status of active operations\n");
	fprintf(stderr, "    drive clear <drive> <start|stop> - clear a drive with all 0x00\n");
	fprintf(stderr, "    start rebuild <drive>\n");
	fprintf(stderr, "    abort rebuild <drive>\n");
	fprintf(stderr, "    locate <drive> <on|off>   - toggle drive LED\n");
	fprintf(stderr, "    cache <volume> [command [setting]]\n");
	fprintf(stderr, "    name <volume> <name>\n");
	fprintf(stderr, "    volume progress <volume>  - display status of active operations\n");
	fprintf(stderr, "    clear                     - clear volume configuration\n");
	fprintf(stderr, "    create <type> [-v] <drive>[,<drive>[,...]] [<drive>[,<drive>[,...]]\n");
	fprintf(stderr, "    delete <volume>\n");
	fprintf(stderr, "    add <drive> [volume]      - add a hot spare\n");
	fprintf(stderr, "    remove <drive>            - remove a hot spare\n");
	fprintf(stderr, "    patrol <disable|auto|manual> [interval [start]]\n");
	fprintf(stderr, "    start patrol              - start a patrol read\n");
	fprintf(stderr, "    stop patrol               - stop a patrol read\n");
	fprintf(stderr, "    flash <firmware>\n");
#ifdef DEBUG
	fprintf(stderr, "    debug                     - debug 'show config'\n");
	fprintf(stderr, "    dump                      - display 'saved' config\n");
#endif
	exit(1);
}

static int
version(int ac, char **av)
{

	printf("mfiutil version 1.0.13");
#ifdef DEBUG
	printf(" (DEBUG)");
#endif
	printf("\n");
	return (0);
}
MFI_COMMAND(top, version, version);

int
main(int ac, char **av)
{
	struct mfiutil_command **cmd;
	int ch;

	while ((ch = getopt(ac, av, "u:")) != -1) {
		switch (ch) {
		case 'u':
			mfi_unit = atoi(optarg);
			break;
		case '?':
			usage();
		}
	}

	av += optind;
	ac -= optind;

	/* getopt() eats av[0], so we can't use mfi_table_handler() directly. */
	if (ac == 0)
		usage();

	SET_FOREACH(cmd, MFI_DATASET(top)) {
		if (strcmp((*cmd)->name, av[0]) == 0) {
			if ((*cmd)->handler(ac, av))
				return (1);
			else
				return (0);
		}
	}
	warnx("Unknown command %s.", av[0]);
	return (1);
}
