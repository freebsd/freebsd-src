/*
 * Copyright (c) 1993 Terrence R. Lambert.
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
 *      This product includes software developed by Terrence R. Lambert.
 * 4. The name Terrence R. Lambert may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TERRENCE R. LAMBERT ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE TERRENCE R. LAMBERT BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
	"$Id: modstat.c,v 1.4.2.3 1997/09/15 09:20:51 jkh Exp $";
#endif /* not lint */

#include <a.out.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/lkm.h>
#include <sys/file.h>
#include "pathnames.h"

static void
usage()
{

	fprintf(stderr, "usage: modstat [-i <module id>] [-n <module name>]\n");
	exit(1);
}

static char *type_names[] = {
	"SYSCALL",
	"VFS",
	"DEV",
	"STRMOD",
	"EXEC",
	"MISC"
};

int
dostat(devfd, modnum, modname)
	int devfd;
	int modnum;
	char *modname;
{
	struct lmc_stat	sbuf;

	sbuf.name[MAXLKMNAME - 1] = '\0'; /* In case strncpy limits the string. */
	if (modname != NULL)
		strncpy(sbuf.name, modname, MAXLKMNAME - 1);

	sbuf.id = modnum;

	if (ioctl(devfd, LMSTAT, &sbuf) == -1) {
		switch (errno) {
		case EINVAL:		/* out of range */
			return 2;
		case ENOENT:		/* no such entry */
			return 1;
		default:		/* other error (EFAULT, etc) */
			warn("LMSTAT");
			return 4;
		}
	}

	/*
	 * Decode this stat buffer...
	 */
	printf("%-7s %3d %3d %08x %04x %8x %3d %s\n",
	    type_names[sbuf.type],
	    sbuf.id,		/* module id */
	    sbuf.offset,	/* offset into modtype struct */
	    sbuf.area,		/* address module loaded at */
	    sbuf.size,		/* size in K */
	    sbuf.private,	/* kernel address of private area */
	    sbuf.ver,		/* Version; always 1 for now */
	    sbuf.name		/* name from private area */
	);

	/*
	 * Done (success).
	 */
	return 0;
}

int devfd;

void
cleanup()
{
	close(devfd);
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int c;
	int modnum = -1;
	char *modname = NULL;

	while ((c = getopt(argc, argv, "i:n:")) != -1) {
		switch (c) {
		case 'i':
			modnum = atoi(optarg);
			break;	/* number */
		case 'n':
			modname = optarg;
			break;	/* name */
		case '?':
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	/*
	 * Open the virtual device device driver for exclusive use (needed
	 * to ioctl() to retrive the loaded module(s) status).
	 */
	if ((devfd = open(_PATH_LKM, O_RDONLY, 0)) == -1)
		err(2, _PATH_LKM);

	atexit(cleanup);

	printf("Type     Id Off Loadaddr Size Info     Rev Module Name\n");

	/*
	 * Oneshot?
	 */
	if (modnum != -1 || modname != NULL) {
		if (dostat(devfd, modnum, modname))
			exit(3);
		exit(0);
	}

	/*
	 * Start at 0 and work up until "EINVAL".
	 */
 	for (modnum = 0; dostat(devfd, modnum, NULL) < 2; modnum++)
 		;

	exit(0);
}
