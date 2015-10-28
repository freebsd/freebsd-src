/*-
 * Copyright (c) 2007, 2011 Robert N. M. Watson
 * Copyright (c) 2015 Allan Jude <allanjude@freebsd.org>
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

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <err.h>
#include <libprocstat.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include "procstat.h"

static int aflag, bflag, cflag, eflag, fflag, iflag, jflag, kflag, lflag, rflag;
static int sflag, tflag, vflag, xflag, Sflag;
int	hflag, nflag, Cflag, Hflag;

static void
usage(void)
{

	xo_error("usage: procstat [-CHhn] [-M core] [-N system] "
	    "[-w interval]\n"
	    "                [-b | -c | -e | -f | -i | -j | -k | "
	    "-l | -r | -s | -S | -t | -v | -x]\n"
	    "                [-a | pid | core ...]\n");
	xo_finish();
	exit(EX_USAGE);
}

static void
procstat(struct procstat *prstat, struct kinfo_proc *kipp)
{
	char *pidstr = NULL;

	asprintf(&pidstr, "%d", kipp->ki_pid);
	if (pidstr == NULL)
		xo_errc(1, ENOMEM, "Failed to allocate memory in procstat()");
	xo_open_container(pidstr);

	if (bflag)
		procstat_bin(prstat, kipp);
	else if (cflag)
		procstat_args(prstat, kipp);
	else if (eflag)
		procstat_env(prstat, kipp);
	else if (fflag)
		procstat_files(prstat, kipp);
	else if (iflag)
		procstat_sigs(prstat, kipp);
	else if (jflag)
		procstat_threads_sigs(prstat, kipp);
	else if (kflag)
		procstat_kstack(prstat, kipp, kflag);
	else if (lflag)
		procstat_rlimit(prstat, kipp);
	else if (rflag)
		procstat_rusage(prstat, kipp);
	else if (sflag)
		procstat_cred(prstat, kipp);
	else if (tflag)
		procstat_threads(prstat, kipp);
	else if (vflag)
		procstat_vm(prstat, kipp);
	else if (xflag)
		procstat_auxv(prstat, kipp);
	else if (Sflag)
		procstat_cs(prstat, kipp);
	else
		procstat_basic(kipp);

	xo_close_container(pidstr);
	free(pidstr);
}

/*
 * Sort processes first by pid and then tid.
 */
static int
kinfo_proc_compare(const void *a, const void *b)
{
	int i;

	i = ((const struct kinfo_proc *)a)->ki_pid -
	    ((const struct kinfo_proc *)b)->ki_pid;
	if (i != 0)
		return (i);
	i = ((const struct kinfo_proc *)a)->ki_tid -
	    ((const struct kinfo_proc *)b)->ki_tid;
	return (i);
}

void
kinfo_proc_sort(struct kinfo_proc *kipp, int count)
{

	qsort(kipp, count, sizeof(*kipp), kinfo_proc_compare);
}

int
main(int argc, char *argv[])
{
	int ch, interval, tmp;
	int i;
	struct kinfo_proc *p;
	struct procstat *prstat, *cprstat;
	long l;
	pid_t pid;
	char *dummy;
	char *nlistf, *memf;
	const char *xocontainer;
	int cnt;

	interval = 0;
	memf = nlistf = NULL;
	argc = xo_parse_args(argc, argv);
	xocontainer = "basic";

	while ((ch = getopt(argc, argv, "CHN:M:abcefijklhrsStvw:x")) != -1) {
		switch (ch) {
		case 'C':
			Cflag++;
			break;

		case 'H':
			Hflag++;
			break;

		case 'M':
			memf = optarg;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 'S':
			Sflag++;
			xocontainer = "cs";
			break;
		case 'a':
			aflag++;
			break;

		case 'b':
			bflag++;
			xocontainer = "binary";
			break;

		case 'c':
			cflag++;
			xocontainer = "arguments";
			break;

		case 'e':
			eflag++;
			xocontainer = "environment";
			break;

		case 'f':
			fflag++;
			xocontainer = "files";
			break;

		case 'i':
			iflag++;
			xocontainer = "signals";
			break;

		case 'j':
			jflag++;
			xocontainer = "thread_signals";
			break;

		case 'k':
			kflag++;
			xocontainer = "kstack";
			break;

		case 'l':
			lflag++;
			xocontainer = "rlimit";
			break;

		case 'n':
			nflag++;
			break;

		case 'h':
			hflag++;
			break;

		case 'r':
			rflag++;
			xocontainer = "rusage";
			break;

		case 's':
			sflag++;
			xocontainer = "credentials";
			break;

		case 't':
			tflag++;
			xocontainer = "threads";
			break;

		case 'v':
			vflag++;
			xocontainer = "vm";
			break;

		case 'w':
			l = strtol(optarg, &dummy, 10);
			if (*dummy != '\0')
				usage();
			if (l < 1 || l > INT_MAX)
				usage();
			interval = l;
			break;

		case 'x':
			xflag++;
			xocontainer = "auxv";
			break;

		case '?':
		default:
			usage();
		}

	}
	argc -= optind;
	argv += optind;

	/* We require that either 0 or 1 mode flags be set. */
	tmp = bflag + cflag + eflag + fflag + iflag + jflag + (kflag ? 1 : 0) +
	    lflag + rflag + sflag + tflag + vflag + xflag + Sflag;
	if (!(tmp == 0 || tmp == 1))
		usage();

	/* We allow -k to be specified up to twice, but not more. */
	if (kflag > 2)
		usage();

	/* Must specify either the -a flag or a list of pids. */
	if (!(aflag == 1 && argc == 0) && !(aflag == 0 && argc > 0))
		usage();

	/* Only allow -C with -f. */
	if (Cflag && !fflag)
		usage();

	if (memf != NULL)
		prstat = procstat_open_kvm(nlistf, memf);
	else
		prstat = procstat_open_sysctl();
	if (prstat == NULL)
		xo_errx(1, "procstat_open()");
	do {
		xo_set_version(PROCSTAT_XO_VERSION);
		xo_open_container("procstat");
		xo_open_container(xocontainer);

		if (aflag) {
			p = procstat_getprocs(prstat, KERN_PROC_PROC, 0, &cnt);
			if (p == NULL)
				xo_errx(1, "procstat_getprocs()");
			kinfo_proc_sort(p, cnt);
			for (i = 0; i < cnt; i++) {
				procstat(prstat, &p[i]);

				/* Suppress header after first process. */
				hflag = 1;
				xo_flush();
			}
			procstat_freeprocs(prstat, p);
		}
		for (i = 0; i < argc; i++) {
			l = strtol(argv[i], &dummy, 10);
			if (*dummy == '\0') {
				if (l < 0)
					usage();
				pid = l;

				p = procstat_getprocs(prstat, KERN_PROC_PID,
				    pid, &cnt);
				if (p == NULL)
					xo_errx(1, "procstat_getprocs()");
				if (cnt != 0)
					procstat(prstat, p);
				procstat_freeprocs(prstat, p);
			} else {
				cprstat = procstat_open_core(argv[i]);
				if (cprstat == NULL) {
					warnx("procstat_open()");
					continue;
				}
				p = procstat_getprocs(cprstat, KERN_PROC_PID,
				    -1, &cnt);
				if (p == NULL)
					xo_errx(1, "procstat_getprocs()");
				if (cnt != 0)
					procstat(cprstat, p);
				procstat_freeprocs(cprstat, p);
				procstat_close(cprstat);
			}
			/* Suppress header after first process. */
			hflag = 1;
		}

		xo_close_container(xocontainer);
		xo_close_container("procstat");
		xo_finish();
		if (interval)
			sleep(interval);
	} while (interval);

	procstat_close(prstat);

	exit(0);
}
