/*
 * Copyright (c) 1987, 1988, 1993
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
"@(#) Copyright (c) 1987, 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)time.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/sysctl.h>
#include <stdlib.h>
#include <sys/wait.h>

#include <err.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static int getstathz __P((void));
static void humantime __P((FILE *, long, long));
static void usage __P((void));

int
main(argc, argv)
	int argc;
	char **argv;
{
	register int pid;
	int aflag, ch, hflag, lflag, status, pflag;
	struct timeval before, after;
	struct rusage ru;
	FILE *out = stderr;
	char *ofn = NULL;
	int exitonsig = 0; /* Die with same signal as child */

	aflag = hflag = lflag = pflag = 0;
	while ((ch = getopt(argc, argv, "ahlo:p")) != -1)
		switch((char)ch) {
		case 'a':
			aflag = 1;
			break;
		case 'h':
			hflag = 1;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'o':
			ofn = optarg;
			break;
		case 'p':
			pflag = 1;
			break;
		case '?':
		default:
			usage();
		}

	if (!(argc -= optind))
		exit(0);
	argv += optind;

	if (ofn) {
	        if ((out = fopen(ofn, aflag ? "a" : "w")) == NULL)
		        err(1, "%s", ofn);
		setvbuf(out, (char *)NULL, _IONBF, (size_t)0);
	}

	gettimeofday(&before, (struct timezone *)NULL);
	switch(pid = fork()) {
	case -1:			/* error */
		err(1, "time");
		/* NOTREACHED */
	case 0:				/* child */
		errno = 0;
		execvp(*argv, argv);
		warn("%s", *argv);
		if (errno == ENOENT)
			_exit(127); /* POSIX: utility could not be found */
		else
			_exit(126); /* POSIX: utility could not be invoked */
		/* NOTREACHED */
	}
	/* parent */
	(void)signal(SIGINT, SIG_IGN);
	(void)signal(SIGQUIT, SIG_IGN);
	while (wait3(&status, 0, &ru) != pid);		/* XXX use waitpid */
	gettimeofday(&after, (struct timezone *)NULL);
	if ( ! WIFEXITED(status))
		warnx("command terminated abnormally");
	if (WIFSIGNALED(status))
		exitonsig = WTERMSIG(status);
	after.tv_sec -= before.tv_sec;
	after.tv_usec -= before.tv_usec;
	if (after.tv_usec < 0)
		after.tv_sec--, after.tv_usec += 1000000;
	if (pflag) {
		/* POSIX wants output that must look like
		"real %f\nuser %f\nsys %f\n" and requires
		at least two digits after the radix. */
		fprintf(out, "real %ld.%02ld\n", 
			after.tv_sec, after.tv_usec/10000);
		fprintf(out, "user %ld.%02ld\n",
			ru.ru_utime.tv_sec, ru.ru_utime.tv_usec/10000);
		fprintf(out, "sys %ld.%02ld\n",
			ru.ru_stime.tv_sec, ru.ru_stime.tv_usec/10000);
	} else if (hflag) {
		humantime(out, after.tv_sec, after.tv_usec/10000);
		fprintf(out, " real\t");
		humantime(out, ru.ru_utime.tv_sec, ru.ru_utime.tv_usec/10000);
		fprintf(out, " user\t");
		humantime(out, ru.ru_stime.tv_sec, ru.ru_stime.tv_usec/10000);
		fprintf(out, " sys\n");
	} else {
		fprintf(out, "%9ld.%02ld real ",
			after.tv_sec, after.tv_usec/10000);
		fprintf(out, "%9ld.%02ld user ",
			ru.ru_utime.tv_sec, ru.ru_utime.tv_usec/10000);
		fprintf(out, "%9ld.%02ld sys\n",
			ru.ru_stime.tv_sec, ru.ru_stime.tv_usec/10000);
	}
	if (lflag) {
		int hz = getstathz();
		u_long ticks;

		ticks = hz * (ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) +
		     hz * (ru.ru_utime.tv_usec + ru.ru_stime.tv_usec) / 1000000;

		/*
		 * If our round-off on the tick calculation still puts us at 0,
		 * then always assume at least one tick.
		 */
		if (ticks == 0)
			ticks = 1;

		fprintf(out, "%10ld  %s\n",
			ru.ru_maxrss, "maximum resident set size");
		fprintf(out, "%10ld  %s\n",
			ru.ru_ixrss / ticks, "average shared memory size");
		fprintf(out, "%10ld  %s\n",
			ru.ru_idrss / ticks, "average unshared data size");
		fprintf(out, "%10ld  %s\n",
			ru.ru_isrss / ticks, "average unshared stack size");
		fprintf(out, "%10ld  %s\n",
			ru.ru_minflt, "page reclaims");
		fprintf(out, "%10ld  %s\n",
			ru.ru_majflt, "page faults");
		fprintf(out, "%10ld  %s\n",
			ru.ru_nswap, "swaps");
		fprintf(out, "%10ld  %s\n",
			ru.ru_inblock, "block input operations");
		fprintf(out, "%10ld  %s\n",
			ru.ru_oublock, "block output operations");
		fprintf(out, "%10ld  %s\n",
			ru.ru_msgsnd, "messages sent");
		fprintf(out, "%10ld  %s\n",
			ru.ru_msgrcv, "messages received");
		fprintf(out, "%10ld  %s\n",
			ru.ru_nsignals, "signals received");
		fprintf(out, "%10ld  %s\n",
			ru.ru_nvcsw, "voluntary context switches");
		fprintf(out, "%10ld  %s\n",
			ru.ru_nivcsw, "involuntary context switches");
	}
	if (exitonsig) {
		if (signal(exitonsig, SIG_DFL) < 0)
			perror("signal");
		else
			kill(getpid(), exitonsig);
	}
	exit (WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE);
}

static void
usage()
{
	fprintf(stderr, "usage: time [-al] [-h|-p] [-o file] command\n");
	exit(1);
}

/*
 * Return the frequency of the kernel's statistics clock.
 */
static int
getstathz()
{
	struct clockinfo clockrate;
	int mib[2];
	size_t size;

	mib[0] = CTL_KERN;
	mib[1] = KERN_CLOCKRATE;
	size = sizeof clockrate;
	if (sysctl(mib, 2, &clockrate, &size, NULL, 0) == -1)
		err(1, "sysctl kern.clockrate");
	return clockrate.stathz;
}

static void
humantime(out, sec, usec)
	FILE *out;
	long sec;
	long usec;
{
	long days, hrs, mins;

	days = sec / (60L * 60 * 24);
	sec %= (60L * 60 * 24);
	hrs = sec / (60L * 60);
	sec %= (60L * 60);
	mins = sec / 60;
	sec %= 60;

	fprintf(out, "\t");
	if (days)
		fprintf(out, "%ldd", days);
	if (hrs)
		fprintf(out, "%ldh", hrs);
	if (mins)
		fprintf(out, "%ldm", mins);
	fprintf(out, "%ld.%02lds", sec, usec);
}
