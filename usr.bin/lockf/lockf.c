/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#define	FDLOCK_PREFIX	"/dev/fd/"

union lock_subject {
	long		 subj_fd;
	const char	*subj_name;
};

static int acquire_lock(union lock_subject *subj, int flags, int silent);
static void cleanup(void);
static void killed(int sig);
static void sigchld(int sig);
static void timeout(int sig);
static void usage(void) __dead2;
static void wait_for_lock(const char *name);

static const char *lockname;
_Static_assert(sizeof(sig_atomic_t) >= sizeof(pid_t),
    "PIDs cannot be managed safely from a signal handler on this platform.");
static sig_atomic_t child = -1;
static int lockfd = -1;
static bool keep;
static bool fdlock;
static int status;
static bool termchild;
static sig_atomic_t timed_out;

/*
 * Check if fdlock is implied by the given `lockname`.  We'll write the fd that
 * is represented by it out to ofd, and the caller is expected to do any
 * necessary validation on it.
 */
static bool
fdlock_implied(const char *name, long *ofd)
{
	char *endp;
	long fd;

	if (strncmp(name, FDLOCK_PREFIX, sizeof(FDLOCK_PREFIX) - 1) != 0)
		return (false);

	/* Skip past the prefix. */
	name += sizeof(FDLOCK_PREFIX) - 1;
	errno = 0;
	fd = strtol(name, &endp, 10);
	if (errno != 0 || *endp != '\0')
		return (false);

	*ofd = fd;
	return (true);
}

/*
 * Execute an arbitrary command while holding a file lock.
 */
int
main(int argc, char **argv)
{
	struct sigaction sa_chld = {
	    .sa_handler = sigchld,
	    .sa_flags = SA_NOCLDSTOP,
	}, sa_prev;
	sigset_t mask, omask;
	long long waitsec;
	const char *errstr;
	union lock_subject subj;
	int ch, flags;
	bool silent, writepid;

	silent = writepid = false;
	flags = O_CREAT | O_RDONLY;
	waitsec = -1;	/* Infinite. */
	while ((ch = getopt(argc, argv, "knpsTt:w")) != -1) {
		switch (ch) {
		case 'k':
			keep = true;
			break;
		case 'n':
			flags &= ~O_CREAT;
			break;
		case 's':
			silent = true;
			break;
		case 'T':
			termchild = true;
			break;
		case 't':
			waitsec = strtonum(optarg, 0, UINT_MAX, &errstr);
			if (errstr != NULL)
				errx(EX_USAGE,
				    "invalid timeout \"%s\"", optarg);
			break;
		case 'p':
			writepid = true;
			flags |= O_TRUNC;
			/* FALLTHROUGH */
		case 'w':
			flags = (flags & ~O_RDONLY) | O_WRONLY;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	lockname = argv[0];

	argc--;
	argv++;

	/*
	 * If there aren't any arguments left, then we must be in fdlock mode.
	 */
	if (argc == 0 && *lockname != '/') {
		fdlock = true;
		subj.subj_fd = -1;
	} else {
		fdlock = fdlock_implied(lockname, &subj.subj_fd);
		if (argc == 0 && !fdlock) {
			fprintf(stderr, "Expected fd, got '%s'\n", lockname);
			usage();
		}
	}

	if (fdlock) {
		if (subj.subj_fd < 0) {
			char *endp;

			errno = 0;
			subj.subj_fd = strtol(lockname, &endp, 10);
			if (errno != 0 || *endp != '\0') {
				fprintf(stderr, "Expected fd, got '%s'\n",
				    lockname);
				usage();
			}
		}

		if (subj.subj_fd < 0 || subj.subj_fd > INT_MAX) {
			fprintf(stderr, "fd '%ld' out of range\n",
			    subj.subj_fd);
			usage();
		}
	} else {
		subj.subj_name = lockname;
	}

	if (waitsec > 0) {		/* Set up a timeout. */
		struct sigaction act;

		act.sa_handler = timeout;
		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;	/* Note that we do not set SA_RESTART. */
		sigaction(SIGALRM, &act, NULL);
		alarm((unsigned int)waitsec);
	}
	/*
	 * If the "-k" option is not given, then we must not block when
	 * acquiring the lock.  If we did, then the lock holder would
	 * unlink the file upon releasing the lock, and we would acquire
	 * a lock on a file with no directory entry.  Then another
	 * process could come along and acquire the same lock.  To avoid
	 * this problem, we separate out the actions of waiting for the
	 * lock to be available and of actually acquiring the lock.
	 *
	 * That approach produces behavior that is technically correct;
	 * however, it causes some performance & ordering problems for
	 * locks that have a lot of contention.  First, it is unfair in
	 * the sense that a released lock isn't necessarily granted to
	 * the process that has been waiting the longest.  A waiter may
	 * be starved out indefinitely.  Second, it creates a thundering
	 * herd situation each time the lock is released.
	 *
	 * When the "-k" option is used, the unlink race no longer
	 * exists.  In that case we can block while acquiring the lock,
	 * avoiding the separate step of waiting for the lock.  This
	 * yields fairness and improved performance.
	 */
	lockfd = acquire_lock(&subj, flags | O_NONBLOCK, silent);
	while (lockfd == -1 && !timed_out && waitsec != 0) {
		if (keep || fdlock) {
			lockfd = acquire_lock(&subj, flags, silent);
		} else {
			wait_for_lock(lockname);
			lockfd = acquire_lock(&subj, flags | O_NONBLOCK,
			    silent);
		}

		/* timed_out */
		atomic_signal_fence(memory_order_acquire);
	}
	if (waitsec > 0)
		alarm(0);
	if (lockfd == -1) {		/* We failed to acquire the lock. */
		if (silent)
			exit(EX_TEMPFAIL);
		errx(EX_TEMPFAIL, "%s: already locked", lockname);
	}

	/* At this point, we own the lock. */

	/* Nothing else to do for FD lock, just exit */
	if (argc == 0) {
		assert(fdlock);
		return 0;
	}

	if (atexit(cleanup) == -1)
		err(EX_OSERR, "atexit failed");

	/*
	 * Block SIGTERM while SIGCHLD is being processed, so that we can safely
	 * waitpid(2) for the child without a concurrent termination observing
	 * an invalid pid (i.e., waited-on).  If our setup between here and the
	 * sigsuspend loop gets any more complicated, we should rewrite it to
	 * just use a pipe to signal the child onto execvp().
	 *
	 * We're blocking SIGCHLD and SIGTERM here so that we don't do any
	 * cleanup before we're ready to (after the pid is written out).
	 */
	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	sigaddset(&mask, SIGTERM);
	(void)sigprocmask(SIG_BLOCK, &mask, &omask);

	memcpy(&sa_chld.sa_mask, &omask, sizeof(omask));
	sigaddset(&sa_chld.sa_mask, SIGTERM);
	(void)sigaction(SIGCHLD, &sa_chld, &sa_prev);

	if ((child = fork()) == -1)
		err(EX_OSERR, "cannot fork");
	if (child == 0) {	/* The child process. */
		(void)sigprocmask(SIG_SETMASK, &omask, NULL);
		close(lockfd);
		execvp(argv[0], argv);
		warn("%s", argv[0]);
		_exit(1);
	}
	/* This is the parent process. */
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTERM, killed);

	fclose(stdin);
	fclose(stdout);
	fclose(stderr);

	/* Write out the pid before we sleep on it. */
	if (writepid)
		(void)dprintf(lockfd, "%d\n", (int)child);

	/* Just in case they were blocked on entry. */
	sigdelset(&omask, SIGCHLD);
	sigdelset(&omask, SIGTERM);
	while (child >= 0) {
		(void)sigsuspend(&omask);
		/* child */
		atomic_signal_fence(memory_order_acquire);
	}

	return (WIFEXITED(status) ? WEXITSTATUS(status) : EX_SOFTWARE);
}

/*
 * Try to acquire a lock on the given file/fd, creating the file if
 * necessary.  The flags argument is O_NONBLOCK or 0, depending on
 * whether we should wait for the lock.  Returns an open file descriptor
 * on success, or -1 on failure.
 */
static int
acquire_lock(union lock_subject *subj, int flags, int silent)
{
	int fd;

	if (fdlock) {
		assert(subj->subj_fd >= 0 && subj->subj_fd <= INT_MAX);
		fd = (int)subj->subj_fd;

		if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
			if (errno == EAGAIN || errno == EINTR)
				return (-1);
			err(EX_CANTCREAT, "cannot lock fd %d", fd);
		}
	} else if ((fd = open(subj->subj_name, O_EXLOCK|flags, 0666)) == -1) {
		if (errno == EAGAIN || errno == EINTR)
			return (-1);
		else if (errno == ENOENT && (flags & O_CREAT) == 0) {
			if (!silent)
				warn("%s", subj->subj_name);
			exit(EX_UNAVAILABLE);
		}
		err(EX_CANTCREAT, "cannot open %s", subj->subj_name);
	}
	return (fd);
}

/*
 * Remove the lock file.
 */
static void
cleanup(void)
{

	if (keep || fdlock)
		flock(lockfd, LOCK_UN);
	else
		unlink(lockname);
}

/*
 * Signal handler for SIGTERM.  Cleans up the lock file, then re-raises
 * the signal.
 */
static void
killed(int sig)
{

	if (termchild && child >= 0)
		kill(child, sig);
	cleanup();
	signal(sig, SIG_DFL);
	if (raise(sig) == -1)
		_Exit(EX_OSERR);
}

/*
 * Signal handler for SIGCHLD.  Simply waits for the child and ensures that we
 * don't end up in a sticky situation if we receive a SIGTERM around the same
 * time.
 */
static void
sigchld(int sig __unused)
{
	int ostatus;

	while (waitpid(child, &ostatus, 0) != child) {
		if (errno != EINTR)
			_exit(EX_OSERR);
	}

	status = ostatus;
	child = -1;
	atomic_signal_fence(memory_order_release);
}

/*
 * Signal handler for SIGALRM.
 */
static void
timeout(int sig __unused)
{

	timed_out = 1;
	atomic_signal_fence(memory_order_release);
}

static void
usage(void)
{

	fprintf(stderr,
	    "usage: lockf [-knsw] [-t seconds] file command [arguments]\n"
	    "       lockf [-s] [-t seconds] fd\n");
	exit(EX_USAGE);
}

/*
 * Wait until it might be possible to acquire a lock on the given file.
 * If the file does not exist, return immediately without creating it.
 */
static void
wait_for_lock(const char *name)
{
	int fd;

	if ((fd = open(name, O_RDONLY|O_EXLOCK, 0666)) == -1) {
		if (errno == ENOENT || errno == EINTR)
			return;
		err(EX_CANTCREAT, "cannot open %s", name);
	}
	close(fd);
}
