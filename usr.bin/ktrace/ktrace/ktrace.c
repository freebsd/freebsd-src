/*-
 * Copyright (c) 1988 The Regents of the University of California.
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
char copyright[] =
"@(#) Copyright (c) 1988 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)ktrace.c	5.2 (Berkeley) 3/5/91";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/ktrace.h>
#include <stdio.h>
#include <signal.h>
#include "ktrace.h"

void noktrace() {
	(void)fprintf(stderr, "ktrace: ktrace not enabled in kernel,to use ktrace\n");
	(void)fprintf(stderr, "you need to add a line \"options KTRACE\" to your kernel\n");
	exit(1);
}

main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	extern char *optarg;
	enum { NOTSET, CLEAR, CLEARALL } clear;
	int append, ch, fd, inherit, ops, pid, pidset, trpoints;
	char *tracefile;

	clear = NOTSET;
	append = ops = pidset = 0;
	trpoints = ALL_POINTS;
	tracefile = DEF_TRACEFILE;
	/* set up a signal handler for SIGSYS, this indicates that ktrace
	is not enabled in the kernel */
	signal(SIGSYS, noktrace);
	while ((ch = getopt(argc,argv,"aCcdf:g:ip:t:")) != EOF)
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
				(void)fprintf(stderr, 
				    "ktrace: unknown facility in %s\n", optarg);
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

	if (clear != NOTSET) {
		if (clear == CLEARALL) {
			ops = KTROP_CLEAR | KTRFLAG_DESCEND;
			pid = 1;
		} else
			ops |= pid ? KTROP_CLEAR : KTROP_CLEARFILE;

		if (ktrace(tracefile, ops, trpoints, pid) < 0)
			error(tracefile);
		exit(0);
	}

	if ((fd = open(tracefile, O_CREAT | O_WRONLY | (append ? 0 : O_TRUNC),
	    DEFFILEMODE)) < 0)
		error(tracefile);
	(void)close(fd);

	if (*argv) { 
		if (ktrace(tracefile, ops, trpoints, getpid()) < 0)
			error();
		execvp(argv[0], &argv[0]);
		error(argv[0]);
		exit(1);
	}
	else if (ktrace(tracefile, ops, trpoints, pid) < 0)
		error(tracefile);
	exit(0);
}

rpid(p)
	char *p;
{
	static int first;

	if (first++) {
		(void)fprintf(stderr,
		    "ktrace: only one -g or -p flag is permitted.\n");
		usage();
	}
	if (!*p) {
		(void)fprintf(stderr, "ktrace: illegal process id.\n");
		usage();
	}
	return(atoi(p));
}

error(name)
	char *name;
{
	(void)fprintf(stderr, "ktrace: %s: %s.\n", name, strerror(errno));
	exit(1);
}

usage()
{
	(void)fprintf(stderr,
"usage:\tktrace [-aCcid] [-f trfile] [-g pgid] [-p pid] [-t [acgn]\n\tktrace [-aCcid] [-f trfile] [-t [acgn] command\n");
	exit(1);
}
