/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)swapon.c	8.1 (Berkeley) 6/5/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <err.h>
#include <errno.h>
#include <fstab.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(const char *);
static int is_swapoff(const char *);
int	swap_on_off(char *name, int ignoreebusy, int do_swapoff);

int
main(int argc, char **argv)
{
	struct fstab *fsp;
	int stat;
	int ch, doall;
	int do_swapoff;
	char *pname = argv[0];

	do_swapoff = is_swapoff(pname);

	doall = 0;
	while ((ch = getopt(argc, argv, "a")) != -1)
		switch((char)ch) {
		case 'a':
			doall = 1;
			break;
		case '?':
		default:
			usage(pname);
		}
	argv += optind;

	stat = 0;
	if (doall)
		while ((fsp = getfsent()) != NULL) {
			if (strcmp(fsp->fs_type, FSTAB_SW))
				continue;
			if (strstr(fsp->fs_mntops, "noauto"))
				continue;
			if (swap_on_off(fsp->fs_spec, 1, do_swapoff))
				stat = 1;
			else
				printf("%s: %sing %s as swap device\n",
				    pname, do_swapoff ? "remov" : "add",
				    fsp->fs_spec);
		}
	else if (!*argv)
		usage(pname);
	for (; *argv; ++argv)
		stat |= swap_on_off(*argv, 0, do_swapoff);
	exit(stat);
}

int
swap_on_off(char *name, int ignoreebusy, int do_swapoff)
{
	if ((do_swapoff ? swapoff(name) : swapon(name)) == -1) {
		switch (errno) {
		case EBUSY:
			if (!ignoreebusy)
				warnx("%s: device already in use", name);
			break;
		default:
			warn("%s", name);
			break;
		}
		return(1);
	}
	return(0);
}

static void
usage(const char *pname)
{
	fprintf(stderr, "usage: %s [-a] [special_file ...]\n", pname);
	exit(1);
}

static int
is_swapoff(const char *s)
{
	const char *u;

	if ((u = strrchr(s, '/')) != NULL)
		++u;
	else
		u = s;
	if (strcmp(u, "swapoff") == 0)
		return 1;
	else
		return 0;
}
