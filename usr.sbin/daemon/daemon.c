/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1999 Berkeley Software Design, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Berkeley Software Design Inc's name may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN INC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	From BSDI: daemon.c,v 1.2 1996/08/15 01:11:09 jch Exp
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <libutil.h>
#include <login_cap.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#define SYSLOG_NAMES
#include <syslog.h>
#include <time.h>
#include <assert.h>

#define LBUF_SIZE 4096

struct log_params {
	const char *output_filename;
	const char *syslog_tag;
	int syslog_priority;
	int syslog_facility;
	int keep_fds_open;
	int output_fd;
	bool syslog_enabled;
};

static void restrict_process(const char *);
static void handle_term(int);
static void handle_chld(int);
static void handle_hup(int);
static int  open_log(const char *);
static void reopen_log(struct log_params *);
static bool listen_child(int, struct log_params *);
static int  get_log_mapping(const char *, const CODE *);
static void open_pid_files(const char *, const char *, struct pidfh **,
			   struct pidfh **);
static void do_output(const unsigned char *, size_t, struct log_params *);
static void daemon_sleep(time_t, long);

static volatile sig_atomic_t terminate = 0;
static volatile sig_atomic_t child_gone = 0;
static volatile sig_atomic_t pid = 0;
static volatile sig_atomic_t do_log_reopen = 0;

static const char shortopts[] = "+cfHSp:P:ru:o:s:l:t:m:R:T:h";

static const struct option longopts[] = {
        { "change-dir",         no_argument,            NULL,           'c' },
        { "close-fds",          no_argument,            NULL,           'f' },
        { "sighup",             no_argument,            NULL,           'H' },
        { "syslog",             no_argument,            NULL,           'S' },
        { "output-file",        required_argument,      NULL,           'o' },
        { "output-mask",        required_argument,      NULL,           'm' },
        { "child-pidfile",      required_argument,      NULL,           'p' },
        { "supervisor-pidfile", required_argument,      NULL,           'P' },
        { "restart",            no_argument,            NULL,           'r' },
        { "restart-delay",      required_argument,      NULL,           'R' },
        { "title",              required_argument,      NULL,           't' },
        { "user",               required_argument,      NULL,           'u' },
        { "syslog-priority",    required_argument,      NULL,           's' },
        { "syslog-facility",    required_argument,      NULL,           'l' },
        { "syslog-tag",         required_argument,      NULL,           'T' },
        { "help",               no_argument,            NULL,           'h' },
        { NULL,                 0,                      NULL,            0  }
};

static _Noreturn void
usage(int exitcode)
{
	(void)fprintf(stderr,
	    "usage: daemon [-cfHrS] [-p child_pidfile] [-P supervisor_pidfile]\n"
	    "              [-u user] [-o output_file] [-t title]\n"
	    "              [-l syslog_facility] [-s syslog_priority]\n"
	    "              [-T syslog_tag] [-m output_mask] [-R restart_delay_secs]\n"
	    "command arguments ...\n");

	(void)fprintf(stderr,
	    "  --change-dir         -c         Change the current working directory to root\n"
	    "  --close-fds          -f         Set stdin, stdout, stderr to /dev/null\n"
	    "  --sighup             -H         Close and re-open output file on SIGHUP\n"
	    "  --syslog             -S         Send output to syslog\n"
	    "  --output-file        -o <file>  Append output of the child process to file\n"
	    "  --output-mask        -m <mask>  What to send to syslog/file\n"
	    "                                  1=stdout, 2=stderr, 3=both\n"
	    "  --child-pidfile      -p <file>  Write PID of the child process to file\n"
	    "  --supervisor-pidfile -P <file>  Write PID of the supervisor process to file\n"
	    "  --restart            -r         Restart child if it terminates (1 sec delay)\n"
	    "  --restart-delay      -R <N>     Restart child if it terminates after N sec\n"
	    "  --title              -t <title> Set the title of the supervisor process\n"
	    "  --user               -u <user>  Drop privileges, run as given user\n"
	    "  --syslog-priority    -s <prio>  Set syslog priority\n"
	    "  --syslog-facility    -l <flty>  Set syslog facility\n"
	    "  --syslog-tag         -T <tag>   Set syslog tag\n"
	    "  --help               -h         Show this help\n");

	exit(exitcode);
}

int
main(int argc, char *argv[])
{
	bool supervision_enabled = false;
	bool log_reopen = false;
	bool child_eof = false;
	bool restart_enabled = false;
	char *p = NULL;
	const char *child_pidfile = NULL;
	const char *parent_pidfile = NULL;
	const char *title = NULL;
	const char *user = NULL;
	int ch = 0;
	int keep_cur_workdir = 1;
	int pipe_fd[2] = { -1, -1 };
	int restart_delay = 1;
	int stdmask = STDOUT_FILENO | STDERR_FILENO;
	struct log_params logparams = {
		.syslog_enabled = false,
		.syslog_priority = LOG_NOTICE,
		.syslog_tag = "daemon",
		.syslog_facility = LOG_DAEMON,
		.keep_fds_open = 1,
		.output_fd = -1,
		.output_filename = NULL
	};
	struct pidfh *parent_pidfh = NULL;
	struct pidfh *child_pidfh = NULL;
	sigset_t mask_orig;
	sigset_t mask_read;
	sigset_t mask_term;
	sigset_t mask_susp;

	sigemptyset(&mask_susp);
	sigemptyset(&mask_read);
	sigemptyset(&mask_term);
	sigemptyset(&mask_orig);

	/*
	 * Supervision mode is enabled if one of the following options are used:
	 * --child-pidfile -p
	 * --supervisor-pidfile -P
	 * --restart -r / --restart-delay -R
	 * --syslog -S
	 * --syslog-facility -l
	 * --syslog-priority -s
	 * --syslog-tag -T
	 *
	 * In supervision mode daemon executes the command in a forked process
	 * and observes the child by waiting for SIGCHILD. In supervision mode
	 * daemon must never exit before the child, this is necessary  to prevent
	 * orphaning the child and leaving a stale pid file.
	 * To achieve this daemon catches SIGTERM and
	 * forwards it to the child, expecting to get SIGCHLD eventually.
	 */
	while ((ch = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (ch) {
		case 'c':
			keep_cur_workdir = 0;
			break;
		case 'f':
			logparams.keep_fds_open = 0;
			break;
		case 'H':
			log_reopen = true;
			break;
		case 'l':
			logparams.syslog_facility = get_log_mapping(optarg,
			    facilitynames);
			if (logparams.syslog_facility == -1) {
				errx(5, "unrecognized syslog facility");
			}
			logparams.syslog_enabled = true;
			supervision_enabled = true;
			break;
		case 'm':
			stdmask = strtol(optarg, &p, 10);
			if (p == optarg || stdmask < 0 || stdmask > 3) {
				errx(6, "unrecognized listening mask");
			}
			break;
		case 'o':
			logparams.output_filename = optarg;
			/*
			 * TODO: setting output filename doesn't have to turn
			 * the supervision mode on. For non-supervised mode
			 * daemon could open the specified file and set it's
			 * descriptor as both stderr and stout before execve()
			 */
			supervision_enabled = true;
			break;
		case 'p':
			child_pidfile = optarg;
			supervision_enabled = true;
			break;
		case 'P':
			parent_pidfile = optarg;
			supervision_enabled = true;
			break;
		case 'r':
			restart_enabled = true;
			supervision_enabled = true;
			break;
		case 'R':
			restart_enabled = true;
			restart_delay = strtol(optarg, &p, 0);
			if (p == optarg || restart_delay < 1) {
				errx(6, "invalid restart delay");
			}
			break;
		case 's':
			logparams.syslog_priority = get_log_mapping(optarg,
			    prioritynames);
			if (logparams.syslog_priority == -1) {
				errx(4, "unrecognized syslog priority");
			}
			logparams.syslog_enabled = true;
			supervision_enabled = true;
			break;
		case 'S':
			logparams.syslog_enabled = true;
			supervision_enabled = true;
			break;
		case 't':
			title = optarg;
			break;
		case 'T':
			logparams.syslog_tag = optarg;
			logparams.syslog_enabled = true;
			supervision_enabled = true;
			break;
		case 'u':
			user = optarg;
			break;
		case 'h':
			usage(0);
			__builtin_unreachable();
		default:
			usage(1);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		usage(1);
	}

	if (!title) {
		title = argv[0];
	}

	if (logparams.output_filename) {
		logparams.output_fd = open_log(logparams.output_filename);
		if (logparams.output_fd == -1) {
			err(7, "open");
		}
	}

	if (logparams.syslog_enabled) {
		openlog(logparams.syslog_tag, LOG_PID | LOG_NDELAY,
		    logparams.syslog_facility);
	}

	/*
	 * Try to open the pidfile before calling daemon(3),
	 * to be able to report the error intelligently
	 */
	open_pid_files(child_pidfile, parent_pidfile, &child_pidfh, &parent_pidfh);
	if (daemon(keep_cur_workdir, logparams.keep_fds_open) == -1) {
		warn("daemon");
		goto exit;
	}
	/* Write out parent pidfile if needed. */
	pidfile_write(parent_pidfh);

	if (supervision_enabled) {
		struct sigaction act_term = { 0 };
		struct sigaction act_chld = { 0 };
		struct sigaction act_hup = { 0 };

		/* Avoid PID racing with SIGCHLD and SIGTERM. */
		act_term.sa_handler = handle_term;
		sigemptyset(&act_term.sa_mask);
		sigaddset(&act_term.sa_mask, SIGCHLD);

		act_chld.sa_handler = handle_chld;
		sigemptyset(&act_chld.sa_mask);
		sigaddset(&act_chld.sa_mask, SIGTERM);

		act_hup.sa_handler = handle_hup;
		sigemptyset(&act_hup.sa_mask);

		/* Block these when avoiding racing before sigsuspend(). */
		sigaddset(&mask_susp, SIGTERM);
		sigaddset(&mask_susp, SIGCHLD);
		/* Block SIGTERM when we lack a valid child PID. */
		sigaddset(&mask_term, SIGTERM);
		/*
		 * When reading, we wish to avoid SIGCHLD. SIGTERM
		 * has to be caught, otherwise we'll be stuck until
		 * the read() returns - if it returns.
		 */
		sigaddset(&mask_read, SIGCHLD);
		/* Block SIGTERM to avoid racing until we have forked. */
		if (sigprocmask(SIG_BLOCK, &mask_term, &mask_orig)) {
			warn("sigprocmask");
			goto exit;
		}
		if (sigaction(SIGTERM, &act_term, NULL) == -1) {
			warn("sigaction");
			goto exit;
		}
		if (sigaction(SIGCHLD, &act_chld, NULL) == -1) {
			warn("sigaction");
			goto exit;
		}
		/*
		 * Try to protect against pageout kill. Ignore the
		 * error, madvise(2) will fail only if a process does
		 * not have superuser privileges.
		 */
		(void)madvise(NULL, 0, MADV_PROTECT);
		if (log_reopen && logparams.output_fd >= 0 &&
		    sigaction(SIGHUP, &act_hup, NULL) == -1) {
			warn("sigaction");
			goto exit;
		}
restart:
		if (pipe(pipe_fd)) {
			err(1, "pipe");
		}
		/*
		 * Spawn a child to exec the command.
		 */
		child_gone = 0;
		pid = fork();
	}

	/* fork failed, this can only happen when supervision is enabled */
	if (pid == -1) {
		warn("fork");
		goto exit;
	}


	/* fork succeeded, this is child's branch or supervision is disabled */
	if (pid == 0) {
		pidfile_write(child_pidfh);

		if (user != NULL) {
			restrict_process(user);
		}
		/*
		 * In supervision mode, the child gets the original sigmask,
		 * and dup'd pipes.
		 */
		if (supervision_enabled) {
			close(pipe_fd[0]);
			if (sigprocmask(SIG_SETMASK, &mask_orig, NULL)) {
				err(1, "sigprogmask");
			}
			if (stdmask & STDERR_FILENO) {
				if (dup2(pipe_fd[1], STDERR_FILENO) == -1) {
					err(1, "dup2");
				}
			}
			if (stdmask & STDOUT_FILENO) {
				if (dup2(pipe_fd[1], STDOUT_FILENO) == -1) {
					err(1, "dup2");
				}
			}
			if (pipe_fd[1] != STDERR_FILENO &&
			    pipe_fd[1] != STDOUT_FILENO) {
				close(pipe_fd[1]);
			}
		}
		execvp(argv[0], argv);
		/* execvp() failed - report error and exit this process */
		err(1, "%s", argv[0]);
	}

	/*
	 * else: pid > 0
	 * fork succeeded, this is the parent branch, this can only happen when
	 * supervision is enabled
	 *
	 * Unblock SIGTERM after we know we have a valid child PID to signal.
	 */
	if (sigprocmask(SIG_UNBLOCK, &mask_term, NULL)) {
		warn("sigprocmask");
		goto exit;
	}
	close(pipe_fd[1]);
	pipe_fd[1] = -1;

	setproctitle("%s[%d]", title, (int)pid);
	/*
	 * As we have closed the write end of pipe for parent process,
	 * we might detect the child's exit by reading EOF. The child
	 * might have closed its stdout and stderr, so we must wait for
	 * the SIGCHLD to ensure that the process is actually gone.
	 */
	for (;;) {
		/*
		 * We block SIGCHLD when listening, but SIGTERM we accept
		 * so the read() won't block if we wish to depart.
		 *
		 * Upon receiving SIGTERM, we have several options after
		 * sending the SIGTERM to our child:
		 * - read until EOF
		 * - read until EOF but only for a while
		 * - bail immediately
		 *
		 * We go for the third, as otherwise we have no guarantee
		 * that we won't block indefinitely if the child refuses
		 * to depart. To handle the second option, a different
		 * approach would be needed (procctl()?)
		 */
		if (child_gone && child_eof) {
			break;
		}

		if (terminate) {
			goto exit;
		}

		if (child_eof) {
			if (sigprocmask(SIG_BLOCK, &mask_susp, NULL)) {
				warn("sigprocmask");
				goto exit;
			}
			while (!terminate && !child_gone) {
				sigsuspend(&mask_orig);
			}
			if (sigprocmask(SIG_UNBLOCK, &mask_susp, NULL)) {
				warn("sigprocmask");
				goto exit;
			}
			continue;
		}

		if (sigprocmask(SIG_BLOCK, &mask_read, NULL)) {
			warn("sigprocmask");
			goto exit;
		}

		child_eof = !listen_child(pipe_fd[0], &logparams);

		if (sigprocmask(SIG_UNBLOCK, &mask_read, NULL)) {
			warn("sigprocmask");
			goto exit;
		}

	}
	if (restart_enabled && !terminate) {
		daemon_sleep(restart_delay, 0);
	}
	if (sigprocmask(SIG_BLOCK, &mask_term, NULL)) {
		warn("sigprocmask");
		goto exit;
	}
	if (restart_enabled && !terminate) {
		close(pipe_fd[0]);
		pipe_fd[0] = -1;
		goto restart;
	}
exit:
	close(logparams.output_fd);
	close(pipe_fd[0]);
	close(pipe_fd[1]);
	if (logparams.syslog_enabled) {
		closelog();
	}
	pidfile_remove(child_pidfh);
	pidfile_remove(parent_pidfh);
	exit(1); /* If daemon(3) succeeded exit status does not matter. */
}

static void
daemon_sleep(time_t secs, long nsecs)
{
	struct timespec ts = { secs, nsecs };

	while (!terminate && nanosleep(&ts, &ts) == -1) {
		if (errno != EINTR) {
			err(1, "nanosleep");
		}
	}
}

static void
open_pid_files(const char *pidfile, const char *ppidfile,
	       struct pidfh **pfh, struct pidfh **ppfh)
{
	pid_t fpid;
	int serrno;

	if (pidfile) {
		*pfh = pidfile_open(pidfile, 0600, &fpid);
		if (*pfh == NULL) {
			if (errno == EEXIST) {
				errx(3, "process already running, pid: %d",
				    fpid);
			}
			err(2, "pidfile ``%s''", pidfile);
		}
	}
	/* Do the same for the actual daemon process. */
	if (ppidfile) {
		*ppfh = pidfile_open(ppidfile, 0600, &fpid);
		if (*ppfh == NULL) {
			serrno = errno;
			pidfile_remove(*pfh);
			errno = serrno;
			if (errno == EEXIST) {
				errx(3, "process already running, pid: %d",
				     fpid);
			}
			err(2, "ppidfile ``%s''", ppidfile);
		}
	}
}

static int
get_log_mapping(const char *str, const CODE *c)
{
	const CODE *cp;
	for (cp = c; cp->c_name; cp++)
		if (strcmp(cp->c_name, str) == 0) {
			return cp->c_val;
		}
	return -1;
}

static void
restrict_process(const char *user)
{
	struct passwd *pw = NULL;

	pw = getpwnam(user);
	if (pw == NULL) {
		errx(1, "unknown user: %s", user);
	}

	if (setusercontext(NULL, pw, pw->pw_uid, LOGIN_SETALL) != 0) {
		errx(1, "failed to set user environment");
	}

	setenv("USER", pw->pw_name, 1);
	setenv("HOME", pw->pw_dir, 1);
	setenv("SHELL", *pw->pw_shell ? pw->pw_shell : _PATH_BSHELL, 1);
}

/*
 * We try to collect whole lines terminated by '\n'. Otherwise we collect a
 * full buffer, and then output it.
 *
 * Return value of false is assumed to mean EOF or error, and true indicates to
 * continue reading.
 */
static bool
listen_child(int fd, struct log_params *logpar)
{
	static unsigned char buf[LBUF_SIZE];
	static size_t bytes_read = 0;
	int rv;

	assert(logpar);
	assert(bytes_read < LBUF_SIZE - 1);

	if (do_log_reopen) {
		reopen_log(logpar);
	}
	rv = read(fd, buf + bytes_read, LBUF_SIZE - bytes_read - 1);
	if (rv > 0) {
		unsigned char *cp;

		bytes_read += rv;
		assert(bytes_read <= LBUF_SIZE - 1);
		/* Always NUL-terminate just in case. */
		buf[LBUF_SIZE - 1] = '\0';
		/*
		 * Chomp line by line until we run out of buffer.
		 * This does not take NUL characters into account.
		 */
		while ((cp = memchr(buf, '\n', bytes_read)) != NULL) {
			size_t bytes_line = cp - buf + 1;
			assert(bytes_line <= bytes_read);
			do_output(buf, bytes_line, logpar);
			bytes_read -= bytes_line;
			memmove(buf, cp + 1, bytes_read);
		}
		/* Wait until the buffer is full. */
		if (bytes_read < LBUF_SIZE - 1) {
			return true;
		}
		do_output(buf, bytes_read, logpar);
		bytes_read = 0;
		return true;
	} else if (rv == -1) {
		/* EINTR should trigger another read. */
		if (errno == EINTR) {
			return true;
		} else {
			warn("read");
			return false;
		}
	}
	/* Upon EOF, we have to flush what's left of the buffer. */
	if (bytes_read > 0) {
		do_output(buf, bytes_read, logpar);
		bytes_read = 0;
	}
	return false;
}

/*
 * The default behavior is to stay silent if the user wants to redirect
 * output to a file and/or syslog. If neither are provided, then we bounce
 * everything back to parent's stdout.
 */
static void
do_output(const unsigned char *buf, size_t len, struct log_params *logpar)
{
	assert(len <= LBUF_SIZE);
	assert(logpar);

	if (len < 1) {
		return;
	}
	if (logpar->syslog_enabled) {
		syslog(logpar->syslog_priority, "%.*s", (int)len, buf);
	}
	if (logpar->output_fd != -1) {
		if (write(logpar->output_fd, buf, len) == -1)
			warn("write");
	}
	if (logpar->keep_fds_open &&
	    !logpar->syslog_enabled &&
	    logpar->output_fd == -1) {
		printf("%.*s", (int)len, buf);
	}
}

/*
 * We use the global PID acquired directly from fork. If there is no valid
 * child pid, the handler should be blocked and/or child_gone == 1.
 */
static void
handle_term(int signo)
{
	if (pid > 0 && !child_gone) {
		kill(pid, signo);
	}
	terminate = 1;
}

static void
handle_chld(int signo __unused)
{

	for (;;) {
		int rv = waitpid(-1, NULL, WNOHANG);
		if (pid == rv) {
			child_gone = 1;
			break;
		} else if (rv == -1 && errno != EINTR) {
			warn("waitpid");
			return;
		}
	}
}

static void
handle_hup(int signo __unused)
{

	do_log_reopen = 1;
}

static int
open_log(const char *outfn)
{

	return open(outfn, O_CREAT | O_WRONLY | O_APPEND | O_CLOEXEC, 0600);
}

static void
reopen_log(struct log_params *logparams)
{
	int outfd;

	do_log_reopen = 0;
	outfd = open_log(logparams->output_filename);
	if (logparams->output_fd >= 0) {
		close(logparams->output_fd);
	}
	logparams->output_fd = outfd;
}

