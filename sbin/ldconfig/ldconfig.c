/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (c) 1993,1995 Paul Kranenburg
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <a.out.h>
#include <ctype.h>
#include <dirent.h>
#include <elf-hints.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ldconfig.h"
#include "rtld_paths.h"

static void usage(void) __dead2;

int
main(int argc, char **argv)
{
	const char *hints_file;
	int c;
	bool is_32, justread, merge, rescan, force_be;

	force_be = is_32 = justread = merge = rescan = false;

	while (argc > 1) {
		if (strcmp(argv[1], "-aout") == 0) {
			errx(1, "aout is not supported");
		} else if (strcmp(argv[1], "-elf") == 0) {
			argc--;
			argv++;
		} else if (strcmp(argv[1], "-32") == 0) {
			is_32 = true;
			argc--;
			argv++;
		} else {
			break;
		}
	}

	if (is_32)
		hints_file = __PATH_ELF_HINTS("32");
	else
		hints_file = _PATH_ELF_HINTS;
	while((c = getopt(argc, argv, "BRf:imrsv")) != -1) {
		switch (c) {
		case 'B':
			force_be = true;
			break;
		case 'R':
			rescan = true;
			break;
		case 'f':
			hints_file = optarg;
			break;
		case 'i':
			insecure = true;
			break;
		case 'm':
			merge = true;
			break;
		case 'r':
			justread = true;
			break;
		case 's':
			/* was nostd */
			break;
		case 'v':
			/* was verbose */
			break;
		default:
			usage();
			break;
		}
	}

	if (justread) {
		list_elf_hints(hints_file);
	} else {
		if (argc == optind)
			rescan = true;
		update_elf_hints(hints_file, argc - optind,
		    argv + optind, merge || rescan, force_be);
	}
	exit(0);
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: ldconfig [-32] [-BRimr] [-f hints_file]"
	    "[directory | file ...]\n");
	exit(1);
}
