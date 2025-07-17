/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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
#include <sys/ptrace.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/linker_set.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"
int pflags;

static void	killed(int);
static void	usage(void) __dead2;

static pid_t pid;
static bool kflag = false;

SET_DECLARE(dumpset, struct dumpers);

static int
open_corefile(char *corefile)
{
	char fname[MAXPATHLEN];
	int fd;

	if (corefile == NULL) {
		(void)snprintf(fname, sizeof(fname), "core.%d", pid);
		corefile = fname;
	}
	fd = open(corefile, O_RDWR | O_CREAT | O_TRUNC, DEFFILEMODE);
	if (fd < 0)
		err(1, "%s", corefile);
	return (fd);
}

static void
kcoredump(int fd, pid_t pid)
{
	struct ptrace_coredump pc;
	int error, res, ret, waited;

	error = ptrace(PT_ATTACH, pid, NULL, 0);
	if (error != 0)
		err(1, "attach");

	waited = waitpid(pid, &res, 0);
	if (waited == -1)
		err(1, "wait for STOP");

	ret = 0;
	memset(&pc, 0, sizeof(pc));
	pc.pc_fd = fd;
	pc.pc_flags = (pflags & PFLAGS_FULL) != 0 ? PC_ALL : 0;
	error = ptrace(PT_COREDUMP, pid, (void *)&pc, sizeof(pc));
	if (error == -1) {
		warn("coredump");
		ret = 1;
	}

	waited = waitpid(pid, &res, WNOHANG);
	if (waited == -1) {
		warn("wait after coredump");
		ret = 1;
	}

	error = ptrace(PT_DETACH, pid, NULL, 0);
	if (error == -1) {
		warn("detach failed, check process status");
		ret = 1;
	}

	exit(ret);
}

int
main(int argc, char *argv[])
{
	int ch, efd, fd, name[4];
	char *binfile, *corefile;
	char passpath[MAXPATHLEN];
	struct dumpers **d, *dumper;
	size_t len;

	pflags = 0;
	corefile = NULL;
        while ((ch = getopt(argc, argv, "c:fk")) != -1) {
                switch (ch) {
                case 'c':
			corefile = optarg;
                        break;
		case 'f':
			pflags |= PFLAGS_FULL;
			break;
		case 'k':
			kflag = true;
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
		break;
	case 2:
		binfile = argv[0];
		pid = atoi(argv[1]);
		break;
	default:
		usage();
	}

	if (kflag) {
		fd = open_corefile(corefile);
		kcoredump(fd, pid);
	}

	if (argc == 1) {
		name[0] = CTL_KERN;
		name[1] = KERN_PROC;
		name[2] = KERN_PROC_PATHNAME;
		name[3] = pid;
		len = sizeof(passpath);
		if (sysctl(name, 4, passpath, &len, NULL, 0) == -1)
			errx(1, "kern.proc.pathname failure");
		binfile = passpath;
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
	fd = open_corefile(corefile);

	dumper->dump(efd, fd, pid);
	(void)close(fd);
	(void)close(efd);
	exit(0);
}

void
usage(void)
{

	(void)fprintf(stderr,
	    "usage: gcore [-kf] [-c core] [executable] pid\n");
	exit(1);
}
