/*
 * Copyright (c) 2007, 2008 	Jeffrey Roberson <jeff@freebsd.org>
 * All rights reserved.
 *
 * Copyright (c) 2008 Nokia Corporation
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/cpuset.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

int cflag;
int gflag;
int iflag;
int jflag;
int lflag;
int pflag;
int rflag;
int sflag;
int tflag;
int xflag;
id_t id;
cpulevel_t level;
cpuwhich_t which;

void usage(void);

static void printset(cpuset_t *mask);

static void
parselist(char *list, cpuset_t *mask)
{
	enum { NONE, NUM, DASH } state;
	int lastnum;
	int curnum;
	char *l;

	state = NONE;
	curnum = lastnum = 0;
	for (l = list; *l != '\0';) {
		if (isdigit(*l)) {
			curnum = atoi(l);
			if (curnum > CPU_SETSIZE)
				errx(EXIT_FAILURE,
				    "Only %d cpus supported", CPU_SETSIZE);
			while (isdigit(*l))
				l++;
			switch (state) {
			case NONE:
				lastnum = curnum;
				state = NUM;
				break;
			case DASH:
				for (; lastnum <= curnum; lastnum++)
					CPU_SET(lastnum, mask);
				state = NONE;
				break;
			case NUM:
			default:
				goto parserr;
			}
			continue;
		}
		switch (*l) {
		case ',':
			switch (state) {
			case NONE:
				break;
			case NUM:
				CPU_SET(curnum, mask);
				state = NONE;
				break;
			case DASH:
				goto parserr;
				break;
			}
			break;
		case '-':
			if (state != NUM)
				goto parserr;
			state = DASH;
			break;
		default:
			goto parserr;
		}
		l++;
	}
	switch (state) {
		case NONE:
			break;
		case NUM:
			CPU_SET(curnum, mask);
			break;
		case DASH:
			goto parserr;
	}
	return;
parserr:
	errx(EXIT_FAILURE, "Malformed cpu-list %s", list);
}

static void
printset(cpuset_t *mask)
{
	int once;
	int cpu;

	for (once = 0, cpu = 0; cpu < CPU_SETSIZE; cpu++) {
		if (CPU_ISSET(cpu, mask)) {
			if (once == 0) {
				printf("%d", cpu);
				once = 1;
			} else
				printf(", %d", cpu);
		}
	}
	printf("\n");
}

const char *whichnames[] = { NULL, "tid", "pid", "cpuset", "irq", "jail" };
const char *levelnames[] = { NULL, " root", " cpuset", "" };

static void
printaffinity(void)
{
	cpuset_t mask;

	if (cpuset_getaffinity(level, which, id, sizeof(mask), &mask) != 0)
		err(EXIT_FAILURE, "getaffinity");
	printf("%s %jd%s mask: ", whichnames[which], (intmax_t)id,
	    levelnames[level]);
	printset(&mask);
	exit(EXIT_SUCCESS);
}

static void
printsetid(void)
{
	cpusetid_t setid;

	/*
	 * Only LEVEL_WHICH && WHICH_CPUSET has a numbered id.
	 */
	if (level == CPU_LEVEL_WHICH && !sflag)
		level = CPU_LEVEL_CPUSET;
	if (cpuset_getid(level, which, id, &setid))
		err(errno, "getid");
	printf("%s %jd%s id: %d\n", whichnames[which], (intmax_t)id,
	    levelnames[level], setid);
}

int
main(int argc, char *argv[])
{
	cpusetid_t setid;
	cpuset_t mask;
	lwpid_t tid;
	pid_t pid;
	int ch;

	CPU_ZERO(&mask);
	level = CPU_LEVEL_WHICH;
	which = CPU_WHICH_PID;
	id = pid = tid = setid = -1;
	while ((ch = getopt(argc, argv, "cgij:l:p:rs:t:x:")) != -1) {
		switch (ch) {
		case 'c':
			if (rflag)
				usage();
			cflag = 1;
			level = CPU_LEVEL_CPUSET;
			break;
		case 'g':
			gflag = 1;
			break;
		case 'i':
			iflag = 1;
			break;
		case 'j':
			jflag = 1;
			which = CPU_WHICH_JAIL;
			id = atoi(optarg);
			break;
		case 'l':
			lflag = 1;
			parselist(optarg, &mask);
			break;
		case 'p':
			pflag = 1;
			which = CPU_WHICH_PID;
			id = pid = atoi(optarg);
			break;
		case 'r':
			if (cflag)
				usage();
			level = CPU_LEVEL_ROOT;
			rflag = 1;
			break;
		case 's':
			sflag = 1;
			which = CPU_WHICH_CPUSET;
			id = setid = atoi(optarg);
			break;
		case 't':
			tflag = 1;
			which = CPU_WHICH_TID;
			id = tid = atoi(optarg);
			break;
		case 'x':
			xflag = 1;
			which = CPU_WHICH_IRQ;
			id = atoi(optarg);
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (gflag) {
		if (argc || lflag)
			usage();
		/* Only one identity specifier. */
		if (jflag + xflag + sflag + pflag + tflag > 1)
			usage();
		if (iflag)
			printsetid();
		else
			printaffinity();
		exit(EXIT_SUCCESS);
	}
	if (iflag)
		usage();
	/*
	 * The user wants to run a command with a set and possibly cpumask.
	 */
	if (argc) {
		if (pflag | rflag | tflag | xflag | jflag)
			usage();
		if (sflag) {
			if (cpuset_setid(CPU_WHICH_PID, -1, setid))
				err(argc, "setid");
		} else {
			if (cpuset(&setid))
				err(argc, "newid");
		}
		if (lflag) {
			if (cpuset_setaffinity(level, CPU_WHICH_PID,
			    -1, sizeof(mask), &mask) != 0)
				err(EXIT_FAILURE, "setaffinity");
		}
		errno = 0;
		execvp(*argv, argv);
		err(errno == ENOENT ? 127 : 126, "%s", *argv);
	}
	/*
	 * We're modifying something that presently exists.
	 */
	if (!lflag && (cflag || rflag))
		usage();
	if (!lflag && !sflag)
		usage();
	/* You can only set a mask on a thread. */
	if (tflag && (sflag | pflag | xflag | jflag))
		usage();
	/* You can only set a mask on an irq. */
	if (xflag && (jflag | pflag | sflag | tflag))
		usage();
	if (pflag && sflag) {
		if (cpuset_setid(CPU_WHICH_PID, pid, setid))
			err(EXIT_FAILURE, "setid");
		/*
		 * If the user specifies a set and a list we want the mask
		 * to effect the pid and not the set.
		 */
		which = CPU_WHICH_PID;
		id = pid;
	}
	if (lflag) {
		if (cpuset_setaffinity(level, which, id, sizeof(mask),
		    &mask) != 0)
			err(EXIT_FAILURE, "setaffinity");
	}

	exit(EXIT_SUCCESS);
}

void
usage(void)
{

	fprintf(stderr,
	    "usage: cpuset [-l cpu-list] [-s setid] cmd ...\n");
	fprintf(stderr,
	    "       cpuset [-l cpu-list] [-s setid] -p pid\n");
	fprintf(stderr,
	    "       cpuset [-cr] [-l cpu-list] [-j jailid | -p pid | -t tid | -s setid | -x irq]\n");
	fprintf(stderr,
	    "       cpuset [-cgir] [-j jailid | -p pid | -t tid | -s setid | -x irq]\n");
	exit(1);
}
