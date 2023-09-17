/*-
 * Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sysexits.h>
#include <string.h>
#include <time.h>
#include <err.h>

#include "stress.h"

static opt_t opt;
opt_t *op;

static char path[64];

static void
usage(const char *where)
{
	const char *help;

	if (where != NULL)
		printf("Error in \"%s\"\n", where);
	fprintf(stderr, "Usage: %s [-t | -l | -i | -d | -h | -k | -v]\n", getprogname());
	help =  " t <number><s|m|h|d> : time to run test\n"
		" l <pct>             : load factor 0 - 100%\n"
		" i <number>          : max # of parallel incarnations\n"
		" d <path>            : working directory\n"
		" h                   : hog resources\n"
		" k                   : terminate with SIGHUP + SIGKILL\n"
		" n                   : no startup delay\n"
		" v                   : verbose\n";
	printf("%s", help);
	exit(EX_USAGE);
}

static int
time2sec(const char *string)
{
	int r, s = 0;
	char modifier;
	r = sscanf(string, "%d%c", &s, &modifier);
	if (r == 2)
		switch(modifier) {
		case 's': break;
		case 'm': s = s * 60; break;
		case 'h': s = s * 60 * 60; break;
		case 'd': s = s * 60 * 60 * 24; break;
		default:
			usage("-t");
		}
	else
		usage("-t");
	return (s);
}

static char *gete(const char *name)
{
	char *cp;
	char help[128];

	snprintf(help, sizeof(help), "%s%s", getprogname(), name);
	cp = getenv(help);
	if (cp == NULL)
		cp = getenv(name);
	return (cp);
}

static void
environment(void)
{
	char *cp;

	if ((cp = gete("INCARNATIONS")) != NULL) {
		if (sscanf(cp, "%d", &op->incarnations) != 1)
			usage("INCARNATIONS");
	}
	if ((cp = gete("LOAD")) != NULL) {
		if (sscanf(cp, "%d", &op->load) != 1)
			usage("LOAD");
	}
	if ((cp = gete("RUNTIME")) != NULL) {
		op->run_time = time2sec(cp);
	}
	if ((cp = gete("RUNDIR")) != NULL) {
		op->wd = cp;
	}
	if ((cp = gete("CTRLDIR")) != NULL) {
		op->cd = cp;
	}
	if ((cp = gete("HOG")) != NULL) {
		op->hog = 1;
	}
	if ((cp = gete("KILL")) != NULL) {
		op->kill = 1;
	}
	if ((cp = gete("NODELAY")) != NULL) {
		op->nodelay = 1;
	}
	if ((cp = gete("VERBOSE")) != NULL) {
		if (sscanf(cp, "%d", &op->verbose) != 1)
			usage("VERBOSE");
	}
	if ((cp = gete("KBLOCKS")) != NULL) {
		if (sscanf(cp, "%jd", &op->kblocks) != 1)
			usage("KBLOCKS");
	}
	if ((cp = gete("INODES")) != NULL) {
		if (sscanf(cp, "%jd", &op->inodes) != 1)
			usage("INODES");
	}
}

void
options(int argc, char **argv)
{
	int ch;

	op = &opt;

	op->run_time	= 60;
	op->load	= 100;
	op->wd		= strdup("/tmp/stressX");
	op->cd		= strdup("/tmp/stressX.control");
	op->incarnations	= 1;
	op->hog		= 0;
	op->kill	= 0;
	op->nodelay	= 0;
	op->verbose	= 0;
	op->kblocks	= 0;
	op->inodes	= 0;

	environment();

	while ((ch = getopt(argc, argv, "t:l:i:d:hknv")) != -1)
		switch(ch) {
		case 't':	/* run time */
			op->run_time = time2sec(optarg);
			break;
		case 'l':	/* load factor in pct */
			if (sscanf(optarg, "%d", &op->load) != 1)
				usage("-l");
			break;
		case 'i':	/* max incarnations */
			if (sscanf(optarg, "%d", &op->incarnations) != 1)
				usage("-i");
			break;
		case 'd':	/* working directory */
			op->wd = strdup(optarg);
			break;
		case 'h':	/* hog flag */
			op->hog += 1;
			break;
		case 'k':	/* kill flag */
			op->kill = 1;
			break;
		case 'n':	/* no delay flag */
			op->nodelay = 1;
			break;
		case 'v':	/* verbose flag */
			op->verbose += 1;
			break;
		default:
			usage(NULL);
		}
	op->argc = argc -= optind;
	op->argv = argv += optind;

	if (op->incarnations < 1)
		op->incarnations = 1;
	if (op->hog == 0)
		op->incarnations = random_int(1, op->incarnations);
	if (op->run_time < 15)
		op->run_time = 15;
	if (op->load < 0 || op->load > 100)
		op->load = 100;
}

void
show_status(void)
{
	char buf[80], pgname[9];
	int days;
	time_t t;

	if (op->verbose > 0) {
		strncpy(pgname, getprogname(), sizeof(pgname));
		pgname[8] = 0;
		t = op->run_time;
		days = t / (60 * 60 * 24);
		t = t % (60 * 60 * 24);
		strftime(buf, sizeof(buf), "%T", gmtime(&t));
		printf("%8s: run time %2d+%s, incarnations %3d, load %3d, "
			"verbose %d\n",
			pgname, days, buf, op->incarnations, op->load,
			op->verbose);
		fflush(stdout);
	}
}

void
rmval(void)
{
	if (snprintf(path, sizeof(path), "%s/%s.conf", op->cd,
	    getprogname()) < 0)
		err(1, "snprintf path");
	(void) unlink(path);
}

void
putval(unsigned long v)
{
	char buf[64];

	rmval();
	snprintf(buf, sizeof(buf), "%lu", v);
	if (symlink(buf, path) < 0)
		err(1, "symlink(%s, %s)", path, buf);
}

unsigned long
getval(void)
{
	int i, n;
	unsigned long val;
	char buf[64];

	if ((n = readlink(path, buf, sizeof(buf) -1)) < 0) {
		for (i = 0; i < 60; i++) {
			sleep(1);
			if ((n = readlink(path, buf, sizeof(buf) -1)) > 0)
				break;
		}
		if (n < 0)
			err(1, "readlink(%s). %s:%d", path, __FILE__,
			    __LINE__);
	}
	buf[n] = '\0';
	if (sscanf(buf, "%ld", &val) != 1)
		err(1, "sscanf(%s)", buf);
	return val;
}
