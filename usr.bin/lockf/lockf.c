/*
 * Copyright (C) 1997 John D. Polstra.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY JOHN D. POLSTRA AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JOHN D. POLSTRA OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: lockf.c,v 1.1.1.1.2.1 1997/01/09 20:01:47 jdp Exp $
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

static int acquire_lock(const char *name);
static void cleanup(void);
static void killed(int sig);
static void timeout(int sig);
static void usage(void);
static void wait_for_lock(const char *name);

static const char *lockname;
static volatile sig_atomic_t timed_out;

/*
 * Execute an arbitrary command while holding a file lock.
 */
int
main(int argc, char **argv)
{
    int ch;
    int lockfd;
    int silent;
    int status;
    int waitsec;
    pid_t child;

    silent = 0;
    waitsec = -1;	/* Infinite. */
    while ((ch = getopt(argc, argv, "st:")) != EOF) {
	switch (ch) {

	case 's':
	    silent = 1;
	    break;

	case 't':
	    {
		char *endptr;
		waitsec = strtol(optarg, &endptr, 0);
		if (*optarg == '\0' || *endptr != '\0' || waitsec < 0)
		    errx(EX_USAGE, "invalid timeout \"%s\"", optarg);
	    }
	    break;

	default:
	    usage();
	}
    }
    if (argc - optind < 2)
	usage();
    lockname = argv[optind++];
    argc -= optind;
    argv += optind;

    if (waitsec > 0) {		/* Set up a timeout. */
	struct sigaction act;

	act.sa_handler = timeout;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;	/* Note that we do not set SA_RESTART. */

	sigaction(SIGALRM, &act, NULL);
	alarm(waitsec);
    }

    lockfd = acquire_lock(lockname);
    while (lockfd == -1 && !timed_out && waitsec != 0) {
	wait_for_lock(lockname);
	lockfd = acquire_lock(lockname);
    }

    if (waitsec > 0)
	alarm(0);

    if (lockfd == -1) {		/* We failed to acquire the lock. */
	if (silent)
	    exit(EX_TEMPFAIL);
	errx(EX_TEMPFAIL, "%s: already locked", lockname);
    }

    /* At this point, we own the lock. */

    if (atexit(cleanup) == -1)
	err(EX_OSERR, "atexit failed");

    if ((child = fork()) == -1)
	err(EX_OSERR, "cannot fork");

    if (child == 0) {	/* The child process. */
	close(lockfd);
	execvp(argv[0], argv);
	perror(argv[0]);
	_exit(1);
    }

    /* This is the parent process. */

    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTERM, killed);

    if (waitpid(child, &status, 0) == -1)
	err(EX_OSERR, "waitpid failed");

    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

/*
 * Try to acquire a lock on the given file, but don't wait for it.  Returns
 * an open file descriptor on success, or -1 on failure.
 */
static int
acquire_lock(const char *name)
{
    int fd;

    if ((fd = open(name, O_RDONLY|O_CREAT|O_EXLOCK|O_NONBLOCK, 0666)) == -1) {
	if (errno == EAGAIN || errno == EINTR)
	    return -1;
	err(EX_CANTCREAT, "cannot open %s", name);
    }
    return fd;
}

/*
 * Remove the lock file.
 */
static void
cleanup(void)
{
    unlink(lockname);
}

/*
 * Signal handler for SIGTERM.  Cleans up the lock file, then re-raises
 * the signal.
 */
static void
killed(int sig)
{
    cleanup();
    signal(sig, SIG_DFL);
    if (kill(getpid(), sig) == -1)
	err(EX_OSERR, "kill failed");
}

/*
 * Signal handler for SIGALRM.
 */
static void
timeout(int sig)
{
    timed_out = 1;
}

static void
usage(void)
{
	fprintf(stderr,
		"usage: lockf [-s] [-t seconds] file command [arguments]\n");
	exit(EX_USAGE);
}

/*
 * Wait until it might be possible to acquire a lock on the given file.
 */
static void
wait_for_lock(const char *name)
{
    int fd;

    if ((fd = open(name, O_RDONLY|O_EXLOCK)) == -1) {
	if (errno == ENOENT || errno == EINTR)
	    return;
	err(EX_CANTCREAT, "cannot open %s", name);
    }
    close(fd);
    return;
}
