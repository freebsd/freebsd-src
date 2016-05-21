/*-
 * Copyright (c) 2016 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <machine/cheri.h>

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <stdio.h>

extern char **environ;

static void
usage(void)
{
	warnx("usage: qtrace (start|stop|exec)");
	exit (1);
}

static inline void
start_trace(void)
{

	CHERI_START_TRACE;
}

static inline void
stop_trace(void)
{

	CHERI_STOP_TRACE;
}

int
main(int argc, char **argv)
{
	int status;
	pid_t pid;

	/* Adjust argc and argv as though we've used getopt. */
	argc--;
	argv++;

	if (argc == 0)
		usage();

	if (strcmp("exec", argv[0]) == 0) {
		pid = fork();
		if (pid < 0)
			err(EX_OSERR, "fork");
		if (pid == 0) {
			start_trace();
			argv++;
			if (execvp(argv[0], argv) == -1)
				err(EX_OSERR, "execvp");
		}

		waitpid(pid, &status, 0);
		stop_trace();
		if (!WIFEXITED(status)) {
			warnx("child exited abnormally");
			exit(-1);
		}
		exit(WEXITSTATUS(status));
	}

	if (argc > 1)
		usage();
	if (strcmp("start", argv[0]) == 0) {
		start_trace();
		exit(0);
	} else if (strcmp("stop", argv[0]) == 0) {
		stop_trace();
		exit(0);
	} else {
		warnx("Unknown command %s\n", argv[0]);
		usage();
	}
}
