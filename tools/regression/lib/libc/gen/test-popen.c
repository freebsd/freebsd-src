/*-
 * Copyright (c) 2013 Jilles Tjoelker
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
 * Limited test program for popen() as specified by IEEE Std. 1003.1-2008,
 * with BSD extensions.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/wait.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static volatile sig_atomic_t got_sigpipe;

static void
sigpipe_handler(int sig __unused)
{
	got_sigpipe = 1;
}

static void
check_cloexec(FILE *fp, const char *mode)
{
	int flags;

	flags = fcntl(fileno(fp), F_GETFD);
	if (flags == -1)
		fprintf(stderr, "fcntl(F_GETFD) failed\n"), failures++;
	else if ((flags & FD_CLOEXEC) !=
	    (strchr(mode, 'e') != NULL ? FD_CLOEXEC : 0))
		fprintf(stderr, "Bad cloexec flag\n"), failures++;
}

int
main(int argc, char *argv[])
{
	FILE *fp, *fp2;
	int i, j, status;
	const char *mode;
	const char *allmodes[] = { "r", "w", "r+", "re", "we", "r+e", "re+" };
	const char *rmodes[] = { "r", "r+", "re", "r+e", "re+" };
	const char *wmodes[] = { "w", "r+", "we", "r+e", "re+" };
	const char *rwmodes[] = { "r+", "r+e", "re+" };
	char buf[80];
	struct sigaction act, oact;

	for (i = 0; i < sizeof(allmodes) / sizeof(allmodes[0]); i++) {
		mode = allmodes[i];
		fp = popen("exit 7", mode);
		if (fp == NULL) {
			fprintf(stderr, "popen(, \"%s\") failed", mode);
			failures++;
			continue;
		}
		check_cloexec(fp, mode);
		status = pclose(fp);
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 7)
			fprintf(stderr, "Bad exit status (no I/O)\n"), failures++;
	}

	for (i = 0; i < sizeof(rmodes) / sizeof(rmodes[0]); i++) {
		mode = rmodes[i];
		fp = popen("exit 9", mode);
		if (fp == NULL) {
			fprintf(stderr, "popen(, \"%s\") failed", mode);
			failures++;
			continue;
		}
		check_cloexec(fp, mode);
		if (fgetc(fp) != EOF || !feof(fp) || ferror(fp))
			fprintf(stderr, "Input error 1\n"), failures++;
		status = pclose(fp);
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 9)
			fprintf(stderr, "Bad exit status (input)\n"), failures++;
	}

	for (i = 0; i < sizeof(rmodes) / sizeof(rmodes[0]); i++) {
		mode = rmodes[i];
		fp = popen("echo hi there", mode);
		if (fp == NULL) {
			fprintf(stderr, "popen(, \"%s\") failed", mode);
			failures++;
			continue;
		}
		check_cloexec(fp, mode);
		if (fgets(buf, sizeof(buf), fp) == NULL)
			fprintf(stderr, "Input error 2\n"), failures++;
		else if (strcmp(buf, "hi there\n") != 0)
			fprintf(stderr, "Bad input 1\n"), failures++;
		status = pclose(fp);
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
			fprintf(stderr, "Bad exit status (input)\n"), failures++;
	}

	for (i = 0; i < sizeof(wmodes) / sizeof(wmodes[0]); i++) {
		mode = wmodes[i];
		fp = popen("read x && [ \"$x\" = abcd ]", mode);
		if (fp == NULL) {
			fprintf(stderr, "popen(, \"%s\") failed", mode);
			failures++;
			continue;
		}
		check_cloexec(fp, mode);
		if (fputs("abcd\n", fp) == EOF)
			fprintf(stderr, "Output error 1\n"), failures++;
		status = pclose(fp);
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
			fprintf(stderr, "Bad exit status (output)\n"), failures++;
	}

	act.sa_handler = sigpipe_handler;
	act.sa_flags = SA_RESTART;
	sigemptyset(&act.sa_mask);
	if (sigaction(SIGPIPE, &act, &oact) == -1)
		fprintf(stderr, "sigaction() failed\n"), failures++;
	for (i = 0; i < sizeof(wmodes) / sizeof(wmodes[0]); i++) {
		mode = wmodes[i];
		fp = popen("exit 88", mode);
		if (fp == NULL) {
			fprintf(stderr, "popen(, \"%s\") failed", mode);
			failures++;
			continue;
		}
		check_cloexec(fp, mode);
		got_sigpipe = 0;
		while (fputs("abcd\n", fp) != EOF)
			;
		if (!ferror(fp) || errno != EPIPE)
			fprintf(stderr, "Expected EPIPE\n"), failures++;
		if (!got_sigpipe)
			fprintf(stderr, "Expected SIGPIPE\n"), failures++;
		status = pclose(fp);
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 88)
			fprintf(stderr, "Bad exit status (EPIPE)\n"), failures++;
	}
	if (sigaction(SIGPIPE, &oact, NULL) == -1)
		fprintf(stderr, "sigaction() failed\n"), failures++;

	for (i = 0; i < sizeof(rwmodes) / sizeof(rwmodes[0]); i++) {
		mode = rwmodes[i];
		fp = popen("read x && printf '%s\\n' \"Q${x#a}\"", mode);
		if (fp == NULL) {
			fprintf(stderr, "popen(, \"%s\") failed", mode);
			failures++;
			continue;
		}
		check_cloexec(fp, mode);
		if (fputs("abcd\n", fp) == EOF)
			fprintf(stderr, "Output error 2\n"), failures++;
		if (fgets(buf, sizeof(buf), fp) == NULL)
			fprintf(stderr, "Input error 3\n"), failures++;
		else if (strcmp(buf, "Qbcd\n") != 0)
			fprintf(stderr, "Bad input 2\n"), failures++;
		status = pclose(fp);
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
			fprintf(stderr, "Bad exit status (I/O)\n"), failures++;
	}

	for (i = 0; i < sizeof(wmodes) / sizeof(wmodes[0]); i++) {
		for (j = 0; j < sizeof(wmodes) / sizeof(wmodes[0]); j++) {
			mode = wmodes[i];
			fp = popen("read x", mode);
			if (fp == NULL) {
				fprintf(stderr, "popen(, \"%s\") failed", mode);
				failures++;
				continue;
			}
			mode = wmodes[j];
			fp2 = popen("read x", mode);
			if (fp2 == NULL) {
				fprintf(stderr, "popen(, \"%s\") failed", mode);
				failures++;
				pclose(fp);
				continue;
			}
			/* If fp2 inherits fp's pipe, we will deadlock here. */
			status = pclose(fp);
			if (!WIFEXITED(status) || WEXITSTATUS(status) != 1) {
				fprintf(stderr, "Bad exit status (2 pipes)\n");
				failures++;
			}
			status = pclose(fp2);
			if (!WIFEXITED(status) || WEXITSTATUS(status) != 1) {
				fprintf(stderr, "Bad exit status (2 pipes)\n");
				failures++;
			}
		}
	}

	if (failures == 0)
		printf("PASS popen()\n");

	return (failures != 0);
}
