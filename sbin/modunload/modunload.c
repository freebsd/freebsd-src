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
 *
 *	$Id: modunload.c,v 1.1.8.1 1996/12/14 13:57:48 joerg Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <string.h>
#include <a.out.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/lkm.h>
#include <sys/file.h>
#include <sys/errno.h>
#include "pathnames.h"

void
usage()
{

	fprintf(stderr, "usage: modunload [-i <module id>] [-n <module name>]\n");
	exit(1);
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
	struct lmc_unload ulbuf;

	while ((c = getopt(argc, argv, "i:n:")) != -1) {
		switch (c) {
		case 'i':
			modnum = atoi(optarg);
			break;	/* number */
		case 'n':
			modname = optarg;
			break;	/* name */
		case '?':
			usage();
		default:
			printf("default!\n");
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0 || (modnum == -1 && modname == NULL))
		usage();


	/*
	 * Open the virtual device device driver for exclusive use (needed
	 * to ioctl() to retrive the loaded module(s) status).
	 */
	if ((devfd = open(_PATH_LKM, O_RDWR, 0)) == -1)
		err(2, _PATH_LKM);

	atexit(cleanup);

	/*
	 * Unload the requested module.
	 */
	ulbuf.name = modname;
	ulbuf.id = modnum;

	if (ioctl(devfd, LMUNLOAD, &ulbuf) == -1) {
		switch (errno) {
		case EINVAL:		/* out of range */
			errx(3, "id out of range");
		case ENOENT:		/* no such entry */
			errx(3, "no such module");
		default:		/* other error (EFAULT, etc) */
			err(5, "LMUNLOAD");
		}
	}

	return 0;
}
