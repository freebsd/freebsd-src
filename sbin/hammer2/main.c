/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2022 Tomohiro Kusumi <tkusumi@netbsd.org>
 * Copyright (c) 2011-2019 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@dragonflybsd.org>
 * by Venkatesh Srinivas <vsrinivas@dragonflybsd.org>
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

/*
 * XXX HAMMER2 userspace consists of sbin/{hammer2,newfs_hammer2,
 * mount_hammer2,fsck_hammer2}.  These are basically same as DragonFly
 * except that write related are currently removed.  Avoid non functional
 * changes in the name of cleanup which makes it less easy to sync with
 * DragonFly.
 */

#include "hammer2.h"

int VerboseOpt;
int QuietOpt;

static void usage(int code);

int
main(int ac, char **av)
{
	char *sel_path = NULL;
	int ecode = 0;
	int ch;

	/*
	 * Core options
	 */
	while ((ch = getopt(ac, av, "s:vq")) != -1) {
		switch(ch) {
		case 's':
			sel_path = strdup(optarg);
			break;
		case 'v':
			if (QuietOpt)
				--QuietOpt;
			else
				++VerboseOpt;
			break;
		case 'q':
			if (VerboseOpt)
				--VerboseOpt;
			else
				++QuietOpt;
			break;
		default:
			fprintf(stderr, "Unknown option: %c\n", ch);
			usage(1);
			/* not reached */
			break;
		}
	}

	/*
	 * Adjust, then process the command
	 */
	ac -= optind;
	av += optind;
	if (ac < 1) {
		fprintf(stderr, "Missing command\n");
		usage(1);
		/* not reached */
	}

	if (strcmp(av[0], "dumpchain") == 0) {
		if (ac < 2)
			ecode = cmd_dumpchain(".", (u_int)-1);
		else if (ac < 3)
			ecode = cmd_dumpchain(av[1], (u_int)-1);
		else
			ecode = cmd_dumpchain(av[1],
					      (u_int)strtoul(av[2], NULL, 0));
	} else if (strcmp(av[0], "pfs-clid") == 0) {
		/*
		 * Print cluster id (uuid) for specific PFS
		 */
		if (ac < 2) {
			fprintf(stderr, "pfs-clid: requires name\n");
			usage(1);
		}
		ecode = cmd_pfs_getid(sel_path, av[1], 0);
	} else if (strcmp(av[0], "pfs-fsid") == 0) {
		/*
		 * Print private id (uuid) for specific PFS
		 */
		if (ac < 2) {
			fprintf(stderr, "pfs-fsid: requires name\n");
			usage(1);
		}
		ecode = cmd_pfs_getid(sel_path, av[1], 1);
	} else if (strcmp(av[0], "pfs-list") == 0) {
		/*
		 * List all PFSs
		 */
		if (ac >= 2) {
			ecode = cmd_pfs_list(ac - 1,
					     (char **)(void *)&av[1]);
		} else {
			ecode = cmd_pfs_list(1, &sel_path);
		}
	} else if (strcmp(av[0], "stat") == 0) {
		ecode = cmd_stat(ac - 1, (const char **)(void *)&av[1]);
	} else if (strcmp(av[0], "show") == 0) {
		/*
		 * Raw dump of filesystem.  Use -v to check all crc's, and
		 * -vv to dump bulk file data.
		 */
		if (ac != 2) {
			fprintf(stderr, "show: requires device path\n");
			usage(1);
		} else {
			cmd_show(av[1], 0);
		}
	} else if (strcmp(av[0], "freemap") == 0) {
		/*
		 * Raw dump of freemap.  Use -v to check all crc's, and
		 * -vv to dump bulk file data.
		 */
		if (ac != 2) {
			fprintf(stderr, "freemap: requires device path\n");
			usage(1);
		} else {
			cmd_show(av[1], 1);
		}
	} else if (strcmp(av[0], "volhdr") == 0) {
		/*
		 * Dump the volume header.
		 */
		if (ac != 2) {
			fprintf(stderr, "volhdr: requires device path\n");
			usage(1);
		} else {
			cmd_show(av[1], 2);
		}
	} else if (strcmp(av[0], "volume-list") == 0) {
		/*
		 * List all volumes
		 */
		if (ac >= 2) {
			ecode = cmd_volume_list(ac - 1,
					     (char **)(void *)&av[1]);
		} else {
			ecode = cmd_volume_list(1, &sel_path);
		}
	} else if (strcmp(av[0], "printinode") == 0) {
		if (ac != 2) {
			fprintf(stderr,
				"printinode: requires directory/file path\n");
			usage(1);
		} else {
			print_inode(av[1]);
		}
	} else {
		fprintf(stderr, "Unrecognized command: %s\n", av[0]);
		usage(1);
	}

	return (ecode);
}

static
void
usage(int code)
{
	fprintf(stderr,
		"hammer2 [options] command [argument ...]\n"
		"    -s path            Select filesystem\n"
		"\n"
		"    pfs-list [<path>...]              "
			"List PFSs\n"
		"    pfs-clid <label>                  "
			"Print cluster id for specific PFS\n"
		"    pfs-fsid <label>                  "
			"Print private id for specific PFS\n"
		"    stat [<path>...]                  "
			"Return inode quota & config\n"
		"    show <devpath>                    "
			"Raw hammer2 media dump for topology\n"
		"    freemap <devpath>                 "
			"Raw hammer2 media dump for freemap\n"
		"    volhdr <devpath>                  "
			"Raw hammer2 media dump for the volume header(s)\n"
		"    volume-list [<path>...]           "
			"List volumes\n"
		"    printinode <path>                 "
			"Dump inode\n"
	);
	exit(code);
}
