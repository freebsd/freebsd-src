/*-
 * Copyright (c) 2011 Jilles Tjoelker
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
 */

/*
 * Test program for posix_spawn() and posix_spawnp() as specified by
 * IEEE Std. 1003.1-2008.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/wait.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <spawn.h>

int
main(int argc, char *argv[])
{
	int error, status;
	pid_t pid, waitres;
	char *myargs[4];
	char *myenv[2] = { "answer=42", NULL };

	/* Make sure we have no child processes. */
	while (waitpid(-1, NULL, 0) != -1)
		;
	assert(errno == ECHILD);

	/* Simple test. */
	myargs[0] = "sh";
	myargs[1] = "-c";
	myargs[2] = "exit $answer";
	myargs[3] = NULL;
	error = posix_spawnp(&pid, myargs[0], NULL, NULL, myargs, myenv);
	assert(error == 0);
	waitres = waitpid(pid, &status, 0);
	assert(waitres == pid);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 42);

	/*
	 * If the executable does not exist, the function shall either fail
	 * and not create a child process or succeed and create a child
	 * process that exits with status 127.
	 */
	myargs[0] = "/var/empty/nonexistent";
	myargs[1] = NULL;
	error = posix_spawn(&pid, myargs[0], NULL, NULL, myargs, myenv);
	if (error == 0) {
		waitres = waitpid(pid, &status, 0);
		assert(waitres == pid);
		assert(WIFEXITED(status) && WEXITSTATUS(status) == 127);
	} else {
		assert(error == ENOENT);
		waitres = waitpid(-1, NULL, 0);
		assert(waitres == -1 && errno == ECHILD);
	}

	printf("PASS posix_spawn()\n");
	printf("PASS posix_spawnp()\n");

	return (0);
}
