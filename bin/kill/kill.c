/*
 * Copyright (c) 1988 Regents of the University of California.
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
"@(#) Copyright (c) 1988 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)kill.c	5.3 (Berkeley) 7/1/91";
#endif /* not lint */

#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *signals[] = {
	"HUP", "INT", "QUIT", "ILL", "TRAP", "ABRT",		/*  1 - 6  */
	"EMT", "FPE", "KILL", "BUS", "SEGV", "SYS",		/*  7 - 12 */
	"PIPE", "ALRM", "TERM", "URG", "STOP", "TSTP",		/* 13 - 18 */
	"CONT", "CHLD", "TTIN", "TTOU", "IO", "XCPU",		/* 19 - 24 */
	"XFSZ", "VTALRM", "PROF", "WINCH", "INFO", "USR1",	/* 25 - 30 */
	"USR2", NULL,						/* 31 - 32 */
};

main(argc, argv)
	int argc;
	char **argv;
{
	register int errors, numsig, pid;
	register char **p;
	char *ep;

	if (argc < 2)
		usage();

	numsig = SIGTERM;
	argc--, argv++;
	if (strcmp(*argv, "-l") == 0) {
		if (argc > 2) {
			usage ();
			/* NOTREACHED */
		}
		if (argc == 2) {
			argv++;
			if (isdigit(**argv)) {
				numsig = strtol(*argv, &ep, 10);
				if (*argv && !*ep) {
					if (numsig > 0 && numsig < NSIG) {
						printsig (numsig);
						exit (0);
					}

					numsig -= 128;
					if (numsig > 0 && numsig < NSIG) {
						printsig (numsig);
						exit (0);
					}
				}
				(void)fprintf(stderr,
				    "kill: illegal signal number %s\n", *argv);
				exit(1);
			}
			usage ();
			/* NOTREACHED */
		}
		printsignals(stdout);
		exit(0);
	} else if (strcmp(*argv, "-s") == 0) {
		if (argc < 2) {
			(void)fprintf(stderr,
				"kill: option requires an argument -- s\n");
			usage();
		}
		argc--,argv++;
		if (strcmp (*argv, "0") == 0) {
			numsig = 0;
		} else {
			if ((numsig = signame_to_signum (*argv)) < 0) {
				nosig(*argv);
				/* NOTREACHED */
			}
		}
		argc--,argv++;
	} else if (**argv == '-') {
		++*argv;
		if (isalpha(**argv)) {
			if ((numsig = signame_to_signum (*argv)) < 0) {
				nosig(*argv);
				/* NOTREACHED */
			}
		} else if (isdigit(**argv)) {
			numsig = strtol(*argv, &ep, 10);
			if (!*argv || *ep) {
				(void)fprintf(stderr,
				    "kill: illegal signal number %s\n", *argv);
				exit(1);
			}
			if (numsig <= 0 || numsig >= NSIG) {
				nosig(*argv);
				/* NOTREACHED */
			}
		} else
			nosig(*argv);
		argc--,argv++;
	}

	if (!*argv)
		usage();

	for (errors = 0; *argv; ++argv) {
		pid = strtol(*argv, &ep, 10);
		if (!*argv || *ep) {
			(void)fprintf(stderr,
			    "kill: illegal process id %s\n", *argv);
			continue;
		}
		if (kill(pid, numsig) == -1) {
			(void)fprintf(stderr,
			    "kill: %s: %s\n", *argv, strerror(errno));
			errors = 1;
		}
	}
	exit(errors);
}

int
signame_to_signum (sig)
	char *sig;
{
	char **p;

	if (!strncasecmp(sig, "sig", 3))
		sig += 3;
	for (p = signals; *p; ++p) {
		if (!strcasecmp(*p, sig)) {
			return p - signals + 1;
		}
	}
	return -1;
}

nosig(name)
	char *name;
{
	(void)fprintf(stderr,
	    "kill: unknown signal %s; valid signals:\n", name);
	printsignals(stderr);
	exit(1);
}

printsig(sig)
	int sig;
{
	printf ("%s\n", signals[sig - 1]);
}

printsignals(fp)
	FILE *fp;
{
	register char **p = signals;;

	/* From POSIX 1003.2, Draft 11.2: 
		When the -l option is specified, the symbolic name of each
	   signal shall be written in the following format:
		"%s%c", <signal_name>, <separator>
           where the <signal_name> is in uppercase, without the SIG prefix,
	   and the <separator> shall either be a <newline> or a <space>.
	   For the last signal written, <separator> shall be a <newline> */

	/* This looses if the signals array is empty; But, since it
	   will "never happen", there is no need to add wrap this 
	   in a conditional that will always succeed. */
	(void)fprintf(fp, "%s", *p);
	
	for (++p ; *p; ++p) {
		(void)fprintf(fp, " %s", *p);
	}
	(void)fprintf(fp, "\n");
}

usage()
{
	(void)fprintf(stderr, "usage: kill [-s signal_name] pid ...\n");
	(void)fprintf(stderr, "       kill -l [exit_status]\n");
	(void)fprintf(stderr, "obsolete usage:\n");
	(void)fprintf(stderr, "       kill -signal_name pid ...\n");
	(void)fprintf(stderr, "       kill -signal_number pid ...\n");
	exit(1);
}
