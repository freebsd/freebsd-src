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

/* Main program for all test programs */

#include <sys/wait.h>
#include <sys/stat.h>

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <err.h>
#include <errno.h>

#include "stress.h"

volatile int done_testing;
static int cleanupcalled = 0;
char *home;

static pid_t *r;

static void
handler(int i __unused)
{
	int j;

	done_testing = 1;
	for (j = 0; j < op->incarnations; j++) {
		if (op->verbose > 2)
			printf("handler: kill -HUP %d\n", r[j]);
		if (r[j] != 0 && kill(r[j], SIGHUP) == -1)
			if (errno != ESRCH)
				warn("kill(%d, SIGHUP), %s:%d", r[j], __FILE__, __LINE__);
	}
	if (op->kill == 1) {
		sleep(5);
		/* test programs may have blocked for the SIGHUP, so try harder */
		for (j = 0; j < op->incarnations; j++) {
			if (op->verbose > 2)
				printf("handler: kill -KILL %d\n", r[j]);
			if (r[j] != 0)
				(void) kill(r[j], SIGKILL);
		}
	}
}

static void
run_test_handler(int i __unused)
{

	done_testing = 1;
}

static void
exit_handler(int i __unused)
{

	_exit(1);
}

static void
callcleanup(void)
{
	if (cleanupcalled == 0)
		cleanup();
	cleanupcalled = 1;
}

static void
run_tests(int i)
{
	time_t start;
	int e;

	signal(SIGHUP, run_test_handler);
	signal(SIGINT, exit_handler);
	atexit(callcleanup);
	setup(i);
	if ((strcmp(getprogname(), "run") != 0) && (op->nodelay == 0))
		sleep(random_int(1,10));
	e = 0;
	start = time(NULL);
	while (done_testing == 0 && e == 0 &&
			(time(NULL) - start) < op->run_time) {
		e = test();
	}
	callcleanup();
	exit(e);
}

static void
run_incarnations(void)
{
	int e, i, s;

	e = 0;
	signal(SIGHUP, handler);
	for (i = 0; i < op->incarnations && done_testing == 0; i++) {
		if ((r[i] = fork()) == 0) {
			run_tests(i);
		}
		if (r[i] < 0) {
			warn("fork(), %s:%d", __FILE__, __LINE__);
			r[i] = 0;
			break;
		}
	}
	for (i = 0; i < op->incarnations; i++) {
		if (r[i] != 0 && waitpid(r[i], &s, 0) == -1)
			warn("waitpid(%d), %s:%d", r[i], __FILE__, __LINE__);
		if (s != 0)
			e = 1;
	}

	exit(e);
}

static int
run_test(void)
{
	pid_t p;
	time_t start;
	int status = 0;

	if (random_int(1,100) > op->load)
		return (status);

	show_status();

	start = time(NULL);
	done_testing = 0;
	fflush(stdout);
	rmval();
	p = fork();
	if (p == 0)
		run_incarnations();
	if (p < 0)
		err(1, "fork() in %s:%d", __FILE__, __LINE__);
	while (done_testing != 1 &&
			(time(NULL) - start) < op->run_time) {
		sleep(1);
		if (waitpid(p, &status, WNOHANG) == p)
			return (status != 0);
	}
	if (kill(p, SIGHUP) == -1)
		warn("kill(%d, SIGHUP), %s:%d", p, __FILE__, __LINE__);

	if (waitpid(p, &status, 0) == -1)
		err(1, "waitpid(%d), %s:%d", p, __FILE__, __LINE__);

	return (status != 0);
}

int
main(int argc, char **argv)
{
	struct stat sb;
	int status = 0;

	options(argc, argv);

	umask(0);
	if (stat(op->wd, &sb) == -1) {
		if (mkdir(op->wd, 0770) == -1)
			if (errno != EEXIST)
				err(1, "mkdir(%s) %s:%d", op->wd, __FILE__, __LINE__);
	} else if ((sb.st_mode & S_IRWXU) == 0)
		errx(1, "No RWX access to %s", op->wd);
	if (stat(op->cd, &sb) == -1) {
		if (mkdir(op->cd, 0770) == -1)
			if (errno != EEXIST)
				err(1, "mkdir(%s) %s:%d", op->cd, __FILE__, __LINE__);
	}
	if ((home = getcwd(NULL, 0)) == NULL)
		err(1, "getcwd(), %s:%d",  __FILE__, __LINE__);
	if (chdir(op->wd) == -1)
		err(1, "chdir(%s) %s:%d", op->wd, __FILE__, __LINE__);

	r = (pid_t *)calloc(1, op->incarnations * sizeof(pid_t));

	status = run_test();

	return (status);
}
