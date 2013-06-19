/*-
 * Copyright (c) 2012 David O'Brien
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
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <err.h>
#include <assert.h>

#ifndef SLEEP
#define	SLEEP	20	/* seconds */
#endif

static int verbose;

static void
usage(void)
{
	fprintf(stderr, "usage: %s\n", getprogname());
	fprintf(stderr, "\t\t-n : length of fork(2) chain\n");
	fprintf(stderr, "\t\t-t : limit run-time seconds\n");
	exit(1);
	/* NOTREACHED */
}

void term(int);
void
term(int signum)
{

	if (getpid() == getpgrp() || verbose) {
		fprintf(stderr,
	    "pid %d pgroup %d (ppid %d): Received SIGTERM(%d), exiting...\n",
		    getpid(), getpgrp(), getppid(), signum);
	 }
	exit(1);
}

void angel_of_mercy(int);
void
angel_of_mercy(int sig __unused)
{

	signal(SIGALRM, SIG_IGN);	/* ignore this signal */
	printf("Master process: alarmed waking up\n");
	killpg(0, SIGTERM);
	return;
}

int bombing_run(unsigned, int);
int
bombing_run(unsigned chainlen, int stime)
{
	struct rusage ru;
	pid_t pid, cpid;
	int status;

	if (chainlen) {
		switch (pid = fork()) {
		case -1:
			errx(1, "%s: can't fork", __func__);

		case 0:
			/* This is the code the child runs. */
			bombing_run(--chainlen, stime);
			break;

		default:
			/* This is the code the parent runs. */
			if (getpid() == getpgrp()) {
				signal(SIGALRM, angel_of_mercy);
				alarm(stime);	// time for bombing run...
				cpid = wait4(pid, &status, 0, &ru);
				alarm(0);
				printf(
		"Cleanly shutting down - pid %d pgroup %d (ppid %d)\n",
				    getpid(), getpgrp(), getppid());
			} else {
				cpid = wait4(pid, &status, 0, &ru);
			}
		}
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	time_t start /*,tvec*/;
	char *endptr, *ctm;
	size_t len;
	int nflag, tflag;
	int ch, k, maxprocperuid;

	(void)signal(SIGTERM, term);

	nflag = 0;
	tflag = SLEEP;

	start = time(NULL);
	ctm = ctime(&start);
	ctm[24] = '\0';		// see: man 3 ctime
	fprintf(stderr, "*** fork() generation started on \"%s\" ***\n", ctm);

	while ((ch = getopt(argc, argv, "n:t:v")) != -1)
		switch (ch) {
		case 'n':
			nflag = strtol(optarg, &endptr, 10);
			if (nflag <= 0 || *endptr != '\0')
				errx(1, "illegal number, -n argument -- %s",
				    optarg);
			break;
		case 't':
			tflag = strtol(optarg, &endptr, 10);
			if (tflag <= 0 || *endptr != '\0')
				errx(1, "illegal number, -t argument -- %s",
				    optarg);
			break;
		case 'v':
			++verbose;
			break;
		default:
			usage();
		}
	argv += optind;

	if (!nflag) {
		len = sizeof(maxprocperuid);
		k = sysctlbyname("kern.maxprocperuid", &maxprocperuid, &len,
		    NULL, 0);
		assert(k != ENOMEM);
		/* Try to allow a shell to still be started. */
		nflag = maxprocperuid - 10;
	}

	// Ensure a unique process group to make killing all children easier.
	setpgrp(0,0);
	printf("    pid %d pgroup %d (ppid %d), %d fork chain over %d sec\n",
	    getpid(), getpgrp(), getppid(), nflag - 1, tflag);

	return bombing_run(nflag, tflag);
}
