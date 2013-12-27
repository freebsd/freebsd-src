/*-
 * Copyright (c) 2009-2011, Juniper Networks, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY JUNIPER NETWORKS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL JUNIPER NETWORKS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
__FBSDID("$FreeBSD$");

#include <sys/wait.h>
#include <sys/ioctl.h>

#include <dev/filemon/filemon.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <err.h>

/*
 * This simple test of filemon expects a test script called
 * "test_script.sh" in the cwd.
 */

#ifndef BIT
#define BIT ""
#endif

int
main(void) {
	char log_name[] = "filemon_log" BIT ".XXXXXX";
	pid_t child;
	int fm_fd, fm_log;

	if ((fm_fd = open("/dev/filemon", O_RDWR)) == -1)
		err(1, "open(\"/dev/filemon\", O_RDWR)");
	if ((fm_log = mkstemp(log_name)) == -1)
		err(1, "mkstemp(%s)", log_name);

	if (ioctl(fm_fd, FILEMON_SET_FD, &fm_log) == -1)
		err(1, "Cannot set filemon log file descriptor");

	/* Set up these two fd's to close on exec. */
	(void)fcntl(fm_fd, F_SETFD, FD_CLOEXEC);
	(void)fcntl(fm_log, F_SETFD, FD_CLOEXEC);

	switch (child = fork()) {
	case 0:
		child = getpid();
		if (ioctl(fm_fd, FILEMON_SET_PID, &child) == -1)
			err(1, "Cannot set filemon PID to %d", child);
		system("env BIT=" BIT "	./test_script.sh");
		break;
	case -1:
		err(1, "Cannot fork");
	default:
		wait(&child);
		close(fm_fd);
//		printf("Results in %s\n", log_name);
		break;
	}
	return 0;
}
