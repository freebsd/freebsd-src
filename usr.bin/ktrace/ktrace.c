/*-
 * Copyright (c) 1988, 1993
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
static char copyright[] =
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)ktrace.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/ktrace.h>

#include <err.h>
#include <stdio.h>
#include <unistd.h>

#include "ktrace.h"

void no_ktrace __P((int));
void usage __P((void));

main(argc, argv)
	int argc;
	char **argv;
{
	enum { NOTSET, CLEAR, CLEARALL } clear;
	int append, ch, fd, inherit, ops, pid, pidset, trpoints;
	char *tracefile;
	mode_t omask;
	struct stat sb;

	clear = NOTSET;
	append = ops = pidset = inherit = 0;
	trpoints = DEF_POINTS;
	tracefile = DEF_TRACEFILE;
	while ((ch = getopt(argc,argv,"aCcdf:g:ip:t:")) != -1)
		switch((char)ch) {
		case 'a':
			append = 1;
			break;
		case 'C':
			clear = CLEARALL;
			pidset = 1;
			break;
		case 'c':
			clear = CLEAR;
			break;
		case 'd':
			ops |= KTRFLAG_DESCEND;
			break;
		case 'f':
			tracefile = optarg;
			break;
		case 'g':
			pid = -rpid(optarg);
			pidset = 1;
			break;
		case 'i':
			inherit = 1;
			break;
		case 'p':
			pid = rpid(optarg);
			pidset = 1;
			break;
		case 't':
			trpoints = getpoints(optarg);
			if (trpoints < 0) {
				warnx("unknown facility in %s", optarg);
				usage();
			}
			break;
		default:
			usage();
		}
	argv += optind;
	argc -= optind;
	
	if (pidset && *argv || !pidset && !*argv)
		usage();
			
	if (inherit)
		trpoints |= KTRFAC_INHERIT;

	(void)signal(SIGSYS, no_ktrace);
	if (clear != NOTSET) {
		if (clear == CLEARALL) {
			ops = KTROP_CLEAR | KTRFLAG_DESCEND;
			trpoints = ALL_POINTS;
			pid = 1;
		} else
			ops |= pid ? KTROP_CLEAR : KTROP_CLEARFILE;

		if (ktrace(tracefile, ops, trpoints, pid) < 0)
			err(1, "%s", tracefile);
		exit(0);
	}

	omask = umask(S_IRWXG|S_IRWXO);
	if (append) {
		if ((fd = open(tracefile, O_CREAT | O_WRONLY, DEFFILEMODE)) < 0)
			err(1, "%s", tracefile);
		if (fstat(fd, &sb) != 0 || sb.st_uid != getuid())
			errx(1, "Refuse to append to %s not owned by you.",
			    tracefile);
	} else {
		if (unlink(tracefile) == -1 && errno != ENOENT)
			err(1, "unlink %s", tracefile);
		if ((fd = open(tracefile, O_CREAT | O_EXCL | O_WRONLY,
		    DEFFILEMODE)) < 0)
			err(1, "%s", tracefile);
	}
	(void)umask(omask);
	(void)close(fd);

	if (*argv) { 
		if (ktrace(tracefile, ops, trpoints, getpid()) < 0)
			err(1, "%s", tracefile);
		execvp(argv[0], &argv[0]);
		err(1, "exec of '%s' failed", argv[0]);
	}
	else if (ktrace(tracefile, ops, trpoints, pid) < 0)
		err(1, "%s", tracefile);
	exit(0);
}

rpid(p)
	char *p;
{
	static int first;

	if (first++) {
		warnx("only one -g or -p flag is permitted.");
		usage();
	}
	if (!*p) {
		warnx("illegal process id.");
		usage();
	}
	return(atoi(p));
}

void
usage()
{
	(void)fprintf(stderr, "%s\n%s\n",
"usage: ktrace [-aCcid] [-f trfile] [-g pgid] [-p pid] [-t [cnisuv]",
"       ktrace [-aCcid] [-f trfile] [-t [cnisuw] command");
	exit(1);
}

void
no_ktrace(sig)
        int sig;
{
        (void)fprintf(stderr,
"error:\tktrace() system call not supported in the running kernel\n\tre-compile kernel with 'options KTRACE'\n");
        exit(1);
}
