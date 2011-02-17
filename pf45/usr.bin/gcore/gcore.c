/*-
 * Copyright (c) 1992, 1993
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
"@(#) Copyright (c) 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#if 0
#ifndef lint
static char sccsid[] = "@(#)gcore.c	8.2 (Berkeley) 9/23/93";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Originally written by Eric Cooper in Fall 1981.
 * Inspired by a version 6 program by Len Levin, 1978.
 * Several pieces of code lifted from Bill Joy's 4BSD ps.
 * Most recently, hacked beyond recognition for 4.4BSD by Steven McCanne,
 * Lawrence Berkeley Laboratory.
 *
 * Portions of this software were developed by the Computer Systems
 * Engineering group at Lawrence Berkeley Laboratory under DARPA
 * contract BG 91-66 and contributed to Berkeley.
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/linker_set.h>
#include <sys/sysctl.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"
int pflags;

static void	killed(int);
static void	usage(void) __dead2;

static pid_t pid;

SET_DECLARE(dumpset, struct dumpers);

int
main(int argc, char *argv[])
{
	int ch, efd, fd, name[4];
	char *binfile, *corefile;
	char passpath[MAXPATHLEN], fname[MAXPATHLEN];
	struct dumpers **d, *dumper;
	size_t len;

	pflags = 0;
	corefile = NULL;
        while ((ch = getopt(argc, argv, "c:fs")) != -1) {
                switch (ch) {
                case 'c':
			corefile = optarg;
                        break;
		case 'f':
			pflags |= PFLAGS_FULL;
			break;
		case 's':
			pflags |= PFLAGS_RESUME;
			break;
		default:
			usage();
			break;
		}
	}
	argv += optind;
	argc -= optind;
	/* XXX we should check that the pid argument is really a number */
	switch (argc) {
	case 1:
		pid = atoi(argv[0]);
		name[0] = CTL_KERN;
		name[1] = KERN_PROC;
		name[2] = KERN_PROC_PATHNAME;
		name[3] = pid;
		len = sizeof(passpath);
		if (sysctl(name, 4, passpath, &len, NULL, 0) == -1)
			errx(1, "kern.proc.pathname failure");
		binfile = passpath;
		break;
	case 2:
		pid = atoi(argv[1]);
		binfile = argv[0];
		break;
	default:
		usage();
	}
	efd = open(binfile, O_RDONLY, 0);
	if (efd < 0)
		err(1, "%s", binfile);
	dumper = NULL;
	SET_FOREACH(d, dumpset) {
		lseek(efd, 0, SEEK_SET);
		if (((*d)->ident)(efd, pid, binfile)) {
			dumper = (*d);
			lseek(efd, 0, SEEK_SET);
			break;
		}
	}
	if (dumper == NULL)
		errx(1, "Invalid executable file");
	if (corefile == NULL) {
		(void)snprintf(fname, sizeof(fname), "core.%d", pid);
		corefile = fname;
	}
	fd = open(corefile, O_RDWR|O_CREAT|O_TRUNC, DEFFILEMODE);
	if (fd < 0)
		err(1, "%s", corefile);
	/*
	 * The semantics of the 's' flag is to stop the target process.
	 * Previous versions of gcore would manage this by trapping SIGHUP,
	 * SIGINT and SIGTERM (to be passed to the target pid), and then
	 * signal the child to stop.
	 *
	 * However, this messes up if the selected dumper uses ptrace calls
	 * that leave the child already stopped. The waitpid call in elfcore
	 * never returns.
	 *
	 * The best thing to do here is to externalize the 's' flag and let
	 * each dumper dispose of what that means, if anything. For the elfcore
	 * dumper, the 's' flag is a no-op since the ptrace attach stops the
	 * process in question already.
	 */

	dumper->dump(efd, fd, pid);
	(void)close(fd);
	(void)close(efd);
	exit(0);
}

void
usage(void)
{

	(void)fprintf(stderr, "usage: gcore [-s] [-c core] [executable] pid\n");
	exit(1);
}
