/*-
 * Copyright (C) 1005 Diomidis Spinellis. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/syslog.h>

#include "portald.h"

/* Usage conventions for the pipe's endpoints. */
#define READ_END	0
#define WRITE_END	1

static int  errlog(void);
static int  parse_argv(char *args, char **argv);

int portal_pipe(pcr, key, v, so, fdp)
struct portal_cred *pcr;
char *key;
char **v;
int so;
int *fdp;
{
	int fd[2];		/* Pipe endpoints. */
	int caller_end;		/* The pipe end we will use. */
	int process_end;	/* The pipe end the spawned process will use. */
	int redirect_fd;	/* The fd to redirect on the spawned process. */
	char pbuf[MAXPATHLEN];
	int error = 0;
	int i;
	char **argv;
	int argc;
	/* Variables used to save the the caller's credentials. */
	uid_t old_uid;
	int ngroups;
	gid_t old_groups[NGROUPS_MAX];

	/* Validate open mode, and assign roles. */
	if ((pcr->pcr_flag & FWRITE) && (pcr->pcr_flag & FREAD))
		/* Don't allow both on a single fd. */
		return (EINVAL);
	else if (pcr->pcr_flag & FREAD) {
		/*
		 * The caller reads from the pipe,
		 * the spawned process writes to it.
		 */
		caller_end = READ_END;
		process_end = WRITE_END;
		redirect_fd = STDOUT_FILENO;
	} else if (pcr->pcr_flag & FWRITE) {
		/*
		 * The caller writes to the pipe,
		 * the spawned process reads from it.
		 */
		caller_end = WRITE_END;
		process_end = READ_END;
		redirect_fd = STDIN_FILENO;
	} else
		return (EINVAL);

	/* Get and check command line. */
	pbuf[0] = '/';
	strcpy(pbuf+1, key + (v[1] ? strlen(v[1]) : 0));
	argc = parse_argv(pbuf, NULL);
	if (argc == 0)
		return (ENOENT);

	/* Swap priviledges. */
	old_uid = geteuid();
	if ((ngroups = getgroups(NGROUPS_MAX, old_groups)) < 0)
		return (errno);
	if (setgroups(pcr->pcr_ngroups, pcr->pcr_groups) < 0)
		return (errno);
	if (seteuid(pcr->pcr_uid) < 0)
		return (errno);

	/* Redirect and spawn the specified process. */
	fd[READ_END] = fd[WRITE_END] = -1;
	if (pipe(fd) < 0) {
		error = errno;
		goto done;
	}
	switch (fork()) {
	case -1: /* Error */
		error = errno;
		break;
	default: /* Parent */
		(void)close(fd[process_end]);
		break;
	case 0: /* Child */
		argv = (char **)malloc((argc + 1) * sizeof(char *));
		if (argv == 0) {
			syslog(LOG_ALERT,
			    "malloc: failed to get space for %d pointers",
			    argc + 1);
			exit(EXIT_FAILURE);
		}
		parse_argv(pbuf, argv);

		if (dup2(fd[process_end], redirect_fd) < 0) {
			syslog(LOG_ERR, "dup2: %m");
			exit(EXIT_FAILURE);
		}
		(void)close(fd[caller_end]);
		(void)close(fd[process_end]);
		if (errlog() < 0) {
			syslog(LOG_ERR, "errlog: %m");
			exit(EXIT_FAILURE);
		}
		if (execv(argv[0], argv) < 0) {
			syslog(LOG_ERR, "execv(%s): %m", argv[0]);
			exit(EXIT_FAILURE);
		}
		/* NOTREACHED */
	}

done:
	/* Re-establish our priviledges. */
	if (seteuid(old_uid) < 0) {
		error = errno;
		syslog(LOG_ERR, "seteuid: %m");
	}
	if (setgroups(ngroups, old_groups) < 0) {
		error = errno;
		syslog(LOG_ERR, "setgroups: %m");
	}

	/* Set return fd value. */
	if (error == 0)
		*fdp = fd[caller_end];
	else {
		for (i = 0; i < 2; i++)
			if (fd[i] >= 0)
				(void)close(fd[i]);
		*fdp = -1;
	}

	return (error);
}

/*
 * Redirect stderr to the system log.
 * Return 0 if ok.
 * Return -1 with errno set on error.
 */
static int
errlog(void)
{
	int fd[2];
	char buff[1024];
	FILE *f;
	int ret = 0;

	if (pipe(fd) < 0)
		return (-1);
	switch (fork()) {
	case -1: /* Error */
		return (-1);
	case 0: /* Child */
		if ((f = fdopen(fd[READ_END], "r")) == NULL) {
			syslog(LOG_ERR, "fdopen: %m");
			exit(EXIT_FAILURE);
		}
		(void)close(fd[WRITE_END]);
		while (fgets(buff, sizeof(buff), f) != NULL)
			syslog(LOG_ERR, "exec: %s", buff);
		exit(EXIT_SUCCESS);
		/* NOTREACHED */
	default: /* Parent */
		if (dup2(fd[WRITE_END], STDERR_FILENO) < 0)
			ret = -1;
		(void)close(fd[READ_END]);
		(void)close(fd[WRITE_END]);
		break;
	}
	return (ret);
}

/*
 * Parse the args string as a space-separated argument vector.
 * If argv is not NULL, split the string into its constituent
 * components, and set argv to point to the beginning  of each
 * string component; NULL-terminating argv.
 * Return the number of string components.
 */
static int
parse_argv(char *args, char **argv)
{
	int count = 0;
	char *p;
	enum {WORD, SPACE} state = SPACE;

	for (p = args; *p; p++)
		switch (state) {
		case WORD:
			if (isspace(*p)) {
				if (argv)
					*p = '\0';
				state = SPACE;
			}
			break;
		case SPACE:
			if (!isspace(*p)) {
				if (argv)
					argv[count] = p;
				count++;
				state = WORD;
			}
		}
	if (argv)
		argv[count] = NULL;
	return (count);
}
