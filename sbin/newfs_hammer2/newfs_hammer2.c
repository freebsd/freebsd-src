/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2022 Tomohiro Kusumi <tkusumi@netbsd.org>
 * Copyright (c) 2011-2015 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@dragonflybsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <err.h>

#include "mkfs_hammer2.h"

static void usage(void);

int
main(int ac, char **av)
{
	hammer2_mkfs_options_t opt;
	int ch;
	int label_specified = 0;

	/*
	 * Initialize option structure.
	 */
	hammer2_mkfs_init(&opt);

	/*
	 * Parse arguments.
	 */
	while ((ch = getopt(ac, av, "L:b:r:V:d")) != -1) {
		switch(ch) {
		case 'b':
			opt.BootAreaSize = getsize(optarg,
					 HAMMER2_NEWFS_ALIGN,
					 HAMMER2_BOOT_MAX_BYTES, 2);
			break;
		case 'r':
			opt.AuxAreaSize = getsize(optarg,
					 HAMMER2_NEWFS_ALIGN,
					 HAMMER2_AUX_MAX_BYTES, 2);
			break;
		case 'V':
			opt.Hammer2Version = strtol(optarg, NULL, 0);
			if (opt.Hammer2Version < HAMMER2_VOL_VERSION_MIN ||
			    opt.Hammer2Version >= HAMMER2_VOL_VERSION_WIP) {
				errx(1, "I don't understand how to format "
				     "HAMMER2 version %d",
				     opt.Hammer2Version);
			}
			break;
		case 'L':
			label_specified = 1;
			if (strcasecmp(optarg, "none") == 0) {
				break;
			}
			if (opt.NLabels >= MAXLABELS) {
				errx(1, "Limit of %d local labels",
				     MAXLABELS - 1);
			}
			if (strlen(optarg) == 0) {
				errx(1, "Volume label '%s' cannot be 0-length",
					optarg);
			}
			if (strlen(optarg) >= HAMMER2_INODE_MAXNAME) {
				errx(1, "Volume label '%s' is too long "
					"(%d chars max)",
					optarg,
					HAMMER2_INODE_MAXNAME - 1);
			}
			opt.Label[opt.NLabels++] = strdup(optarg);
			break;
		case 'd':
			opt.DebugOpt = 1;
			break;
		default:
			usage();
			break;
		}
	}

	ac -= optind;
	av += optind;

	if (ac == 0)
		errx(1, "You must specify at least one disk device");
	if (ac > HAMMER2_MAX_VOLUMES)
		errx(1, "The maximum number of volumes is %d",
		     HAMMER2_MAX_VOLUMES);

	/*
	 * Check default label type.
	 */
	if (!label_specified)
		opt.DefaultLabelType = HAMMER2_LABEL_DATA;

	/*
	 * Create Hammer2 filesystem.
	 */
	hammer2_mkfs(ac, av, &opt);

	/*
	 * Cleanup option structure.
	 */
	hammer2_mkfs_cleanup(&opt);

	return(0);
}

static
void
usage(void)
{
	fprintf(stderr,
		"usage: newfs_hammer2 [-b bootsize] [-r auxsize] "
		"[-V version] [-L label ...] special ...\n"
	);
	exit(1);
}
