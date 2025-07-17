/*
 * Copyright (c) 1994 University of Maryland
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of U.M. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  U.M. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * U.M. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL U.M.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: James da Silva, Systems Design and Analysis Group
 *			   Computer Science Department
 *			   University of Maryland at College Park
 */
/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2020 Alex Richardson <arichardson@FreeBSD.org>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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
/*
 * crunched_main.c - main program for crunched binaries, it branches to a
 * 	particular subprogram based on the value of argv[0].  Also included
 *	is a little program invoked when the crunched binary is called via
 *	its EXECNAME.  This one prints out the list of compiled-in binaries,
 *	or calls one of them based on argv[1].   This allows the testing of
 *	the crunched binary without creating all the links.
 */

#include <sys/param.h>
#include <sys/auxv.h>
#include <sys/sysctl.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int crunched_stub_t(int, char **, char **);

struct stub {
	const char *name;
	crunched_stub_t *f;
};

extern const char *__progname;
extern struct stub entry_points[];
extern int num_entry_points;

static void crunched_usage(int);

crunched_stub_t crunched_main;
crunched_stub_t crunched_list;

static struct stub *
find_entry_point(const char *basename)
{
	for (int i = 0; i < num_entry_points; i++) {
		struct stub *ep = &entry_points[i];
		if (!strcmp(basename, ep->name))
			return (ep);
	}
	return (NULL);
}

static const char *
get_basename(const char *exe_path)
{
	const char *slash = strrchr(exe_path, '/');
	return (slash ? slash + 1 : exe_path);
}

int
main(int argc, char **argv, char **envp)
{
	struct stub *ep = NULL;
	const char *basename = NULL;
	char buf[MAXPATHLEN];

	/*
	 * Look at __progname first (this will be set if the crunched binary is
	 * invoked directly).
	 */
	if (__progname) {
		basename = get_basename(__progname);
		ep = find_entry_point(basename);
	}

	/*
	 * Otherwise try to find entry point based on argv[0] (this works for
	 * both symlinks as well as hardlinks). However, it does not work when
	 * su invokes a crunched shell because it sets argv[0] to _su when
	 * invoking the shell. In that case we look at AT_EXECPATH as a
	 * fallback.
	 */
	if (ep == NULL) {
		basename = get_basename(argv[0]);
		ep = find_entry_point(basename);
	}

	/*
	 * If we didn't find the entry point based on __progname or argv[0],
	 * try AT_EXECPATH to get the actual binary that was executed.
	 */
	if (ep == NULL) {
		int error = elf_aux_info(AT_EXECPATH, &buf, sizeof(buf));

		if (error == 0) {
			const char *exe_name = get_basename(buf);
			/*
			 * Keep using argv[0] if AT_EXECPATH is the crunched
			 * binary so that symlinks to the crunched binary report
			 * "not compiled in" instead of invoking
			 * crunched_main().
			 */
			if (strcmp(exe_name, EXECNAME) != 0) {
				basename = exe_name;
				ep = find_entry_point(basename);
			}
		} else {
			warnc(error, "elf_aux_info(AT_EXECPATH) failed");
		}
	}

	if (basename == NULL || *basename == '\0')
		crunched_usage(1);

	if (ep != NULL) {
		return ep->f(argc, argv, envp);
	} else {
		fprintf(stderr, "%s: %s not compiled in\n\n",
		    EXECNAME, basename);
		crunched_usage(1);
	}
}

int
crunched_main(int argc, char **argv, char **envp)
{
	if (argc <= 1)
		crunched_usage(0);

	__progname = get_basename(argv[1]);
	return main(--argc, ++argv, envp);
}

int
crunched_list(int argc __unused, char **argv __unused, char **envp __unused)
{
	for (int i = 0; i < num_entry_points - 2; i++)
		printf("%s\n", entry_points[i].name);
	return (0);
}

static void
crunched_usage(int code)
{
	int columns, len;
	FILE *out = stdout;
	if (code > 0)
		out = stderr;

	fprintf(out,
	    "usage: %s program [args ...]\n"
	    "       %s --list\n"
	    "       program [args ...]\n"
	    "\n"
	    "%s combines several programs in one executable. Create a link to this\n"
	    "executable with the program name to run that program, or give the program\n"
	    "name as the first argument.\n"
	    "\n"
	    "Currently defined programs:\n",
	    EXECNAME, EXECNAME, EXECNAME);
	columns = 0;
	for (int i = 0; i < num_entry_points - 2; i++) {
		struct stub *ep = &entry_points[i];
		len = strlen(ep->name) + 1;
		if (columns + len < 80)
			columns += len;
		else {
			fprintf(out, "\n");
			columns = len;
		}
		fprintf(out, " %s", ep->name);
	}
	fprintf(out, "\n");
	exit(code);
}

/* end of crunched_main.c */
