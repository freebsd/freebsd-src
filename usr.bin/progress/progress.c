/*	$NetBSD: progress.c,v 1.25 2021/08/17 07:18:43 gson Exp $ */

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by John Hawkinson.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: progress.c,v 1.25 2021/08/17 07:18:43 gson Exp $");
#endif				/* not lint */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define GLOBAL			/* force GLOBAL decls in progressbar.h to be
				 * declared */
#include "progressbar.h"

static void broken_pipe(int unused);
__dead static void usage(void);

static void
broken_pipe(int unused)
{
	signal(SIGPIPE, SIG_DFL);
	progressmeter(1);
	kill(getpid(), SIGPIPE);
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-ez] [-b buffersize] [-f file] [-l length]\n"
	    "       %*.s [-p prefix] cmd [args...]\n",
	    getprogname(), (int) strlen(getprogname()), "");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	char *fb_buf;
	char *infile = NULL;
	pid_t pid = 0, gzippid = 0, deadpid;
	int ch, fd, outpipe[2];
	int ws, gzipstat, cmdstat;
	int eflag = 0, lflag = 0, zflag = 0;
	ssize_t nr, nw, off;
	size_t buffersize;
	struct stat statb;
	struct ttysize ts;

	setprogname(argv[0]);

	/* defaults: Read from stdin, 0 filesize (no completion estimate) */
	fd = STDIN_FILENO;
	filesize = 0;
	buffersize = 64 * 1024;
	prefix = NULL;

	while ((ch = getopt(argc, argv, "b:ef:l:p:z")) != -1)
		switch (ch) {
		case 'b':
			buffersize = (size_t) strsuftoll("buffer size", optarg,
			    0, SSIZE_MAX);
			break;
		case 'e':
			eflag++;
			break;
		case 'f':
			infile = optarg;
			break;
		case 'l':
			lflag++;
			filesize = strsuftoll("input size", optarg, 0,
			    LLONG_MAX);
			break;
		case 'p':
			prefix = optarg;
			break;
		case 'z':
			zflag++;
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	if (infile && (fd = open(infile, O_RDONLY, 0)) < 0)
		err(1, "%s", infile);

	/* stat() to get the filesize unless overridden, or -z */
	if (!zflag && !lflag && (fstat(fd, &statb) == 0)) {
		if (S_ISFIFO(statb.st_mode)) {
			/* stat(2) on pipe may return only the
			 * first few bytes with more coming.
			 * Don't trust!
			 */
		} else {
			filesize = statb.st_size;
		}
	}

	/* gzip -l the file if we have the name and -z is given */
	if (zflag && !lflag && infile != NULL) {
		FILE *gzipsizepipe;
		char buf[256], *cp, *cmd;

		/*
		 * Read second word of last line of gzip -l output. Looks like:
		 * % gzip -l ../etc.tgz 
		 *   compressed uncompressed  ratio uncompressed_name
		 * 	 119737       696320  82.8% ../etc.tar
		 */

		asprintf(&cmd, "gzip -l %s", infile);
		if ((gzipsizepipe = popen(cmd, "r")) == NULL)
			err(1, "reading compressed file length");
		for (; fgets(buf, 256, gzipsizepipe) != NULL;)
		    continue;
		strtoimax(buf, &cp, 10);
		filesize = strtoimax(cp, NULL, 10);
		if (pclose(gzipsizepipe) < 0)
			err(1, "closing compressed file length pipe");
		free(cmd);
	}
	/* Pipe input through gzip -dc if -z is given */
	if (zflag) {
		int gzippipe[2];

		if (pipe(gzippipe) < 0)
			err(1, "gzip pipe");
		gzippid = fork();
		if (gzippid < 0)
			err(1, "fork for gzip");

		if (gzippid) {
			/* parent */
			dup2(gzippipe[0], fd);
			close(gzippipe[0]);
			close(gzippipe[1]);
		} else {
			dup2(gzippipe[1], STDOUT_FILENO);
			dup2(fd, STDIN_FILENO);
			close(gzippipe[0]);
			close(gzippipe[1]);
			if (execlp("gzip", "gzip", "-dc", NULL))
				err(1, "exec()ing gzip");
		}
	}

	/* Initialize progressbar.c's global state */
	bytes = 0;
	progress = 1;
	ttyout = eflag ? stderr : stdout;

	if (ioctl(fileno(ttyout), TIOCGSIZE, &ts) == -1)
		ttywidth = 80;
	else
		ttywidth = ts.ts_cols;

	fb_buf = malloc(buffersize);
	if (fb_buf == NULL)
		err(1, "malloc for buffersize");

	if (pipe(outpipe) < 0)
		err(1, "output pipe");
	pid = fork();
	if (pid < 0)
		err(1, "fork for output pipe");

	if (pid == 0) {
		/* child */
		dup2(outpipe[0], STDIN_FILENO);
		close(outpipe[0]);
		close(outpipe[1]);
		execvp(argv[0], argv);
		err(1, "could not exec %s", argv[0]);
	}
	close(outpipe[0]);

	signal(SIGPIPE, broken_pipe);
	progressmeter(-1);

	while (1) {
		do {
			nr = read(fd, fb_buf, buffersize);
		} while (nr < 0 && errno == EINTR);
		if (nr < 0) {
			progressmeter(1);
			err(1, "reading input");
		}
		if (nr == 0)
			break;
		for (off = 0; nr; nr -= nw, off += nw, bytes += nw)
			if ((nw = write(outpipe[1], fb_buf + off,
			    (size_t) nr)) < 0) {
				if (errno == EINTR) {
					nw = 0;
					continue;
				}
				progressmeter(1);
				err(1, "writing %u bytes to output pipe",
							(unsigned) nr);
			}
	}
	close(outpipe[1]);

	gzipstat = 0;
	cmdstat = 0;
	while (pid || gzippid) {
		deadpid = wait(&ws);
		/*
		 * We need to exit with an error if the command (or gzip)
		 * exited abnormally.
		 * Unfortunately we can't generate a true 'exited by signal'
		 * error without sending the signal to ourselves :-(
		 */
		ws = WIFSIGNALED(ws) ? WTERMSIG(ws) : WEXITSTATUS(ws);

		if (deadpid == -1 && errno == EINTR)
			continue;
		if (deadpid == pid) {
			pid = 0;
			cmdstat = ws;
			continue;
		}
		if (deadpid == gzippid) {
			gzippid = 0;
			gzipstat = ws;
			continue;
		}
		break;
	}

	progressmeter(1);
	signal(SIGPIPE, SIG_DFL);

	free(fb_buf);

	exit(cmdstat ? cmdstat : gzipstat);
}
