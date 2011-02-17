/*-
 * Copyright (c) 2007 Robert N. M. Watson
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
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include "procstat.h"

static int aflag, bflag, cflag, fflag, iflag, jflag, kflag, sflag, tflag, vflag;
int	hflag, nflag;

static void
usage(void)
{

	fprintf(stderr, "usage: procstat [-h] [-n] [-w interval] [-b | -c | -f | "
	    "-i | -j | -k | -s | -t | -v]\n");
	fprintf(stderr, "                [-a | pid ...]\n");
	exit(EX_USAGE);
}

static void
procstat(pid_t pid, struct kinfo_proc *kipp)
{

	if (bflag)
		procstat_bin(pid, kipp);
	else if (cflag)
		procstat_args(pid, kipp);
	else if (fflag)
		procstat_files(pid, kipp);
	else if (iflag)
		procstat_sigs(pid, kipp);
	else if (jflag)
		procstat_threads_sigs(pid, kipp);
	else if (kflag)
		procstat_kstack(pid, kipp, kflag);
	else if (sflag)
		procstat_cred(pid, kipp);
	else if (tflag)
		procstat_threads(pid, kipp);
	else if (vflag)
		procstat_vm(pid, kipp);
	else
		procstat_basic(pid, kipp);
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
	int ch, interval, name[4], tmp;
	unsigned int i;
	struct kinfo_proc *kipp;
	size_t len;
	long l;
	pid_t pid;
	char *dummy;

	interval = 0;
	while ((ch = getopt(argc, argv, "abcfijknhstvw:")) != -1) {
		switch (ch) {
		case 'a':
			aflag++;
			break;

		case 'b':
			bflag++;
			break;

		case 'c':
			cflag++;
			break;

		case 'f':
			fflag++;
			break;

		case 'i':
			iflag++;
			break;

		case 'j':
			jflag++;
			break;

		case 'k':
			kflag++;
			break;

		case 'n':
			nflag++;
			break;

		case 'h':
			hflag++;
			break;

		case 's':
			sflag++;
			break;

		case 't':
			tflag++;
			break;

		case 'v':
			vflag++;
			break;

		case 'w':
			l = strtol(optarg, &dummy, 10);
			if (*dummy != '\0')
				usage();
			if (l < 1 || l > INT_MAX)
				usage();
			interval = l;
			break;

		case '?':
		default:
			usage();
		}

	}
	argc -= optind;
	argv += optind;

	/* We require that either 0 or 1 mode flags be set. */
	tmp = bflag + cflag + fflag + (kflag ? 1 : 0) + sflag + tflag + vflag;
	if (!(tmp == 0 || tmp == 1))
		usage();

	/* We allow -k to be specified up to twice, but not more. */
	if (kflag > 2)
		usage();

	/* Must specify either the -a flag or a list of pids. */
	if (!(aflag == 1 && argc == 0) && !(aflag == 0 && argc > 0))
		usage();

	do {
		if (aflag) {
			name[0] = CTL_KERN;
			name[1] = KERN_PROC;
			name[2] = KERN_PROC_PROC;

			len = 0;
			if (sysctl(name, 3, NULL, &len, NULL, 0) < 0)
				err(-1, "sysctl: kern.proc.all");

			kipp = malloc(len);
			if (kipp == NULL)
				err(-1, "malloc");

			if (sysctl(name, 3, kipp, &len, NULL, 0) < 0) {
				free(kipp);
				err(-1, "sysctl: kern.proc.all");
			}
			if (len % sizeof(*kipp) != 0)
				err(-1, "kinfo_proc mismatch");
			if (kipp->ki_structsize != sizeof(*kipp))
				err(-1, "kinfo_proc structure mismatch");
			kinfo_proc_sort(kipp, len / sizeof(*kipp));
			for (i = 0; i < len / sizeof(*kipp); i++) {
				procstat(kipp[i].ki_pid, &kipp[i]);

				/* Suppress header after first process. */
				hflag = 1;
			}
			free(kipp);
		}
		for (i = 0; i < (unsigned int)argc; i++) {
			l = strtol(argv[i], &dummy, 10);
			if (*dummy != '\0')
				usage();
			if (l < 0)
				usage();
			pid = l;

			name[0] = CTL_KERN;
			name[1] = KERN_PROC;
			name[2] = KERN_PROC_PID;
			name[3] = pid;

			len = 0;
			if (sysctl(name, 4, NULL, &len, NULL, 0) < 0)
				err(-1, "sysctl: kern.proc.pid: %d", pid);

			kipp = malloc(len);
			if (kipp == NULL)
				err(-1, "malloc");

			if (sysctl(name, 4, kipp, &len, NULL, 0) < 0) {
				free(kipp);
				err(-1, "sysctl: kern.proc.pid: %d", pid);
			}
			if (len != sizeof(*kipp))
				err(-1, "kinfo_proc mismatch");
			if (kipp->ki_structsize != sizeof(*kipp))
				errx(-1, "kinfo_proc structure mismatch");
			if (kipp->ki_pid != pid)
				errx(-1, "kinfo_proc pid mismatch");
			procstat(pid, kipp);
			free(kipp);

			/* Suppress header after first process. */
			hflag = 1;
		}
		if (interval)
			sleep(interval);
	} while (interval);
	exit(0);
}
