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

struct daemon_state {
	sigset_t mask_orig;
	sigset_t mask_read;
	sigset_t mask_term;
	sigset_t mask_susp;
	int pipe_fd[2];
	char **argv;
	const char *child_pidfile;
	const char *parent_pidfile;
	const char *output_filename;
	const char *syslog_tag;
	const char *title;
	const char *user;
	struct pidfh *parent_pidfh;
	struct pidfh *child_pidfh;
	int keep_cur_workdir;
	int restart_delay;
	int stdmask;
	int syslog_priority;
	int syslog_facility;
	int keep_fds_open;
	int output_fd;
	bool supervision_enabled;
	bool child_eof;
	bool restart_enabled;
	bool syslog_enabled;
	bool log_reopen;
};

static void setup_signals(struct daemon_state *);
static void restrict_process(const char *);
static void handle_term(int);
static void handle_chld(int);
static void handle_hup(int);
static int  open_log(const char *);
static void reopen_log(struct daemon_state *);
static bool listen_child(int, struct daemon_state *);
static int  get_log_mapping(const char *, const CODE *);
static void open_pid_files(struct daemon_state *);
static void do_output(const unsigned char *, size_t, struct daemon_state *);
static void daemon_sleep(time_t, long);
static void daemon_state_init(struct daemon_state *);
static void daemon_eventloop(struct daemon_state *);
static void daemon_terminate(struct daemon_state *);

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
	char *p = NULL;
	int ch = 0;
	struct daemon_state state;

	daemon_state_init(&state);

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
			state.keep_cur_workdir = 0;
			break;
		case 'f':
			state.keep_fds_open = 0;
			break;
		case 'H':
			state.log_reopen = true;
			break;
		case 'l':
			state.syslog_facility = get_log_mapping(optarg,
			    facilitynames);
			if (state.syslog_facility == -1) {
				errx(5, "unrecognized syslog facility");
			}
			state.syslog_enabled = true;
			state.supervision_enabled = true;
			break;
		case 'm':
			state.stdmask = strtol(optarg, &p, 10);
			if (p == optarg || state.stdmask < 0 || state.stdmask > 3) {
				errx(6, "unrecognized listening mask");
			}
			break;
		case 'o':
			state.output_filename = optarg;
			/*
			 * TODO: setting output filename doesn't have to turn
			 * the supervision mode on. For non-supervised mode
			 * daemon could open the specified file and set it's
			 * descriptor as both stderr and stout before execve()
			 */
			state.supervision_enabled = true;
			break;
		case 'p':
			state.child_pidfile = optarg;
			state.supervision_enabled = true;
			break;
		case 'P':
			state.parent_pidfile = optarg;
			state.supervision_enabled = true;
			break;
		case 'r':
			state.restart_enabled = true;
			state.supervision_enabled = true;
			break;
		case 'R':
			state.restart_enabled = true;
			state.restart_delay = strtol(optarg, &p, 0);
			if (p == optarg || state.restart_delay < 1) {
				errx(6, "invalid restart delay");
			}
			break;
		case 's':
			state.syslog_priority = get_log_mapping(optarg,
			    prioritynames);
			if (state.syslog_priority == -1) {
				errx(4, "unrecognized syslog priority");
			}
			state.syslog_enabled = true;
			state.supervision_enabled = true;
			break;
		case 'S':
			state.syslog_enabled = true;
			state.supervision_enabled = true;
			break;
		case 't':
			state.title = optarg;
			break;
		case 'T':
			state.syslog_tag = optarg;
			state.syslog_enabled = true;
			state.supervision_enabled = true;
			break;
		case 'u':
			state.user = optarg;
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
	state.argv = argv;

	if (argc == 0) {
		usage(1);
	}

	if (!state.title) {
		state.title = argv[0];
	}

	if (state.output_filename) {
		state.output_fd = open_log(state.output_filename);
		if (state.output_fd == -1) {
			err(7, "open");
		}
	}

	if (state.syslog_enabled) {
		openlog(state.syslog_tag, LOG_PID | LOG_NDELAY,
		    state.syslog_facility);
	}

	/*
	 * Try to open the pidfile before calling daemon(3),
	 * to be able to report the error intelligently
	 */
	open_pid_files(&state);
	if (daemon(state.keep_cur_workdir, state.keep_fds_open) == -1) {
		warn("daemon");
		daemon_terminate(&state);
	}
	/* Write out parent pidfile if needed. */
	pidfile_write(state.parent_pidfh);

	if (state.supervision_enabled) {
		/* Block SIGTERM to avoid racing until the child is spawned. */
		if (sigprocmask(SIG_BLOCK, &state.mask_term, &state.mask_orig)) {
			warn("sigprocmask");
			daemon_terminate(&state);
		}

		setup_signals(&state);

		/*
		 * Try to protect against pageout kill. Ignore the
		 * error, madvise(2) will fail only if a process does
		 * not have superuser privileges.
		 */
		(void)madvise(NULL, 0, MADV_PROTECT);
	}
	do {
		daemon_eventloop(&state);
		close(state.pipe_fd[0]);
		state.pipe_fd[0] = -1;
	} while (state.restart_enabled && !terminate);

	daemon_terminate(&state);
}


/*
 * Main event loop: fork the child and watch for events.
 * In legacy mode simply execve into the target process.
 *
 * Signal handling logic:
 *
 * - SIGTERM is masked while there is no child.
 *
 * - SIGCHLD is masked while reading from the pipe. SIGTERM has to be
 *   caught, to avoid indefinite blocking on read().
 *
 * - Both SIGCHLD and SIGTERM are masked before calling sigsuspend()
 *   to avoid racing.
 *
 * - After SIGTERM is recieved and propagated to the child there are
 *   several options on what to do next:
 *   - read until EOF
 *   - read until EOF but only for a while
 *   - bail immediately
 *   Currently the third option is used, because otherwise there is no
 *   guarantee that read() won't block indefinitely if the child refuses
 *   to depart. To handle the second option, a different approach
 *   would be needed (procctl()?).
 *
 * - Child's exit might be detected by receiveing EOF from the pipe.
 *   But the child might have closed its stdout and stderr, so deamon
 *   must wait for the SIGCHLD to ensure that the child is actually gone.
 */
static void
daemon_eventloop(struct daemon_state *state)
{
	if (state->supervision_enabled) {
		if (pipe(state->pipe_fd)) {
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
		daemon_terminate(state);
	}

	/* fork succeeded, this is child's branch or supervision is disabled */
	if (pid == 0) {
		pidfile_write(state->child_pidfh);

		if (state->user != NULL) {
			restrict_process(state->user);
		}
		/*
		 * In supervision mode, the child gets the original sigmask,
		 * and dup'd pipes.
		 */
		if (state->supervision_enabled) {
			close(state->pipe_fd[0]);
			if (sigprocmask(SIG_SETMASK, &state->mask_orig, NULL)) {
				err(1, "sigprogmask");
			}
			if (state->stdmask & STDERR_FILENO) {
				if (dup2(state->pipe_fd[1], STDERR_FILENO) == -1) {
					err(1, "dup2");
				}
			}
			if (state->stdmask & STDOUT_FILENO) {
				if (dup2(state->pipe_fd[1], STDOUT_FILENO) == -1) {
					err(1, "dup2");
				}
			}
			if (state->pipe_fd[1] != STDERR_FILENO &&
			    state->pipe_fd[1] != STDOUT_FILENO) {
				close(state->pipe_fd[1]);
			}
		}
		execvp(state->argv[0], state->argv);
		/* execvp() failed - report error and exit this process */
		err(1, "%s", state->argv[0]);
	}

	/*
	 * else: pid > 0
	 * fork succeeded, this is the parent branch, this can only happen when
	 * supervision is enabled.
	 *
	 * Unblock SIGTERM - now there is a valid child PID to signal to.
	 */
	if (sigprocmask(SIG_UNBLOCK, &state->mask_term, NULL)) {
		warn("sigprocmask");
		daemon_terminate(state);
	}
	close(state->pipe_fd[1]);
	state->pipe_fd[1] = -1;

	setproctitle("%s[%d]", state->title, (int)pid);
	for (;;) {
		if (child_gone && state->child_eof) {
			break;
		}

		if (terminate) {
			daemon_terminate(state);
		}

		if (state->child_eof) {
			if (sigprocmask(SIG_BLOCK, &state->mask_susp, NULL)) {
				warn("sigprocmask");
				daemon_terminate(state);
			}
			while (!terminate && !child_gone) {
				sigsuspend(&state->mask_orig);
			}
			if (sigprocmask(SIG_UNBLOCK, &state->mask_susp, NULL)) {
				warn("sigprocmask");
				daemon_terminate(state);
			}
			continue;
		}

		if (sigprocmask(SIG_BLOCK, &state->mask_read, NULL)) {
			warn("sigprocmask");
			daemon_terminate(state);
		}

		state->child_eof = !listen_child(state->pipe_fd[0], state);

		if (sigprocmask(SIG_UNBLOCK, &state->mask_read, NULL)) {
			warn("sigprocmask");
			daemon_terminate(state);
		}

	}

	/*
	 * At the end of the loop the the child is already gone.
	 * Block SIGTERM to avoid racing until the child is spawned.
	 */
	if (sigprocmask(SIG_BLOCK, &state->mask_term, NULL)) {
		warn("sigprocmask");
		daemon_terminate(state);
	}

	/* sleep before exiting mainloop if restart is enabled */
	if (state->restart_enabled && !terminate) {
		daemon_sleep(state->restart_delay, 0);
	}
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

/*
 * Setup SIGTERM, SIGCHLD and SIGHUP handlers.
 * To avoid racing SIGCHLD with SIGTERM corresponding
 * signal handlers mask the other signal.
 */
static void
setup_signals(struct daemon_state *state)
{
	struct sigaction act_term = { 0 };
	struct sigaction act_chld = { 0 };
	struct sigaction act_hup = { 0 };

	/* Setup SIGTERM */
	act_term.sa_handler = handle_term;
	sigemptyset(&act_term.sa_mask);
	sigaddset(&act_term.sa_mask, SIGCHLD);
	if (sigaction(SIGTERM, &act_term, NULL) == -1) {
		warn("sigaction");
		daemon_terminate(state);
	}

	/* Setup SIGCHLD */
	act_chld.sa_handler = handle_chld;
	sigemptyset(&act_chld.sa_mask);
	sigaddset(&act_chld.sa_mask, SIGTERM);
	if (sigaction(SIGCHLD, &act_chld, NULL) == -1) {
		warn("sigaction");
		daemon_terminate(state);
	}

	/* Setup SIGHUP if configured */
	if (!state->log_reopen || state->output_fd < 0) {
		return;
	}

	act_hup.sa_handler = handle_hup;
	sigemptyset(&act_hup.sa_mask);
	if (sigaction(SIGHUP, &act_hup, NULL) == -1) {
		warn("sigaction");
		daemon_terminate(state);
	}
}

static void
open_pid_files(struct daemon_state *state)
{
	pid_t fpid;
	int serrno;

	if (state->child_pidfile) {
		state->child_pidfh = pidfile_open(state->child_pidfile, 0600, &fpid);
		if (state->child_pidfh == NULL) {
			if (errno == EEXIST) {
				errx(3, "process already running, pid: %d",
				    fpid);
			}
			err(2, "pidfile ``%s''", state->child_pidfile);
		}
	}
	/* Do the same for the actual daemon process. */
	if (state->parent_pidfile) {
		state->parent_pidfh= pidfile_open(state->parent_pidfile, 0600, &fpid);
		if (state->parent_pidfh == NULL) {
			serrno = errno;
			pidfile_remove(state->child_pidfh);
			errno = serrno;
			if (errno == EEXIST) {
				errx(3, "process already running, pid: %d",
				     fpid);
			}
			err(2, "ppidfile ``%s''", state->parent_pidfile);
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
listen_child(int fd, struct daemon_state *state)
{
	static unsigned char buf[LBUF_SIZE];
	static size_t bytes_read = 0;
	int rv;

	assert(state != NULL);
	assert(bytes_read < LBUF_SIZE - 1);

	if (do_log_reopen) {
		reopen_log(state);
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
			do_output(buf, bytes_line, state);
			bytes_read -= bytes_line;
			memmove(buf, cp + 1, bytes_read);
		}
		/* Wait until the buffer is full. */
		if (bytes_read < LBUF_SIZE - 1) {
			return true;
		}
		do_output(buf, bytes_read, state);
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
		do_output(buf, bytes_read, state);
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
do_output(const unsigned char *buf, size_t len, struct daemon_state *state)
{
	assert(len <= LBUF_SIZE);
	assert(state != NULL);

	if (len < 1) {
		return;
	}
	if (state->syslog_enabled) {
		syslog(state->syslog_priority, "%.*s", (int)len, buf);
	}
	if (state->output_fd != -1) {
		if (write(state->output_fd, buf, len) == -1)
			warn("write");
	}
	if (state->keep_fds_open &&
	    !state->syslog_enabled &&
	    state->output_fd == -1) {
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
reopen_log(struct daemon_state *state)
{
	int outfd;

	do_log_reopen = 0;
	outfd = open_log(state->output_filename);
	if (state->output_fd >= 0) {
		close(state->output_fd);
	}
	state->output_fd = outfd;
}

static void
daemon_state_init(struct daemon_state *state)
{
	*state = (struct daemon_state) {
		.pipe_fd = { -1, -1 },
		.argv = NULL,
		.parent_pidfh = NULL,
		.child_pidfh = NULL,
		.child_pidfile = NULL,
		.parent_pidfile = NULL,
		.title = NULL,
		.user = NULL,
		.supervision_enabled = false,
		.child_eof = false,
		.restart_enabled = false,
		.keep_cur_workdir = 1,
		.restart_delay = 1,
		.stdmask = STDOUT_FILENO | STDERR_FILENO,
		.syslog_enabled = false,
		.log_reopen = false,
		.syslog_priority = LOG_NOTICE,
		.syslog_tag = "daemon",
		.syslog_facility = LOG_DAEMON,
		.keep_fds_open = 1,
		.output_fd = -1,
		.output_filename = NULL,
	};

	sigemptyset(&state->mask_susp);
	sigemptyset(&state->mask_read);
	sigemptyset(&state->mask_term);
	sigemptyset(&state->mask_orig);
	sigaddset(&state->mask_susp, SIGTERM);
	sigaddset(&state->mask_susp, SIGCHLD);
	sigaddset(&state->mask_term, SIGTERM);
	sigaddset(&state->mask_read, SIGCHLD);

}

static _Noreturn void
daemon_terminate(struct daemon_state *state)
{
	assert(state != NULL);
	close(state->output_fd);
	close(state->pipe_fd[0]);
	close(state->pipe_fd[1]);
	if (state->syslog_enabled) {
		closelog();
	}
	pidfile_remove(state->child_pidfh);
	pidfile_remove(state->parent_pidfh);

	/*
	 * Note that the exit value here doesn't matter in the case of a clean
	 * exit; daemon(3) already detached us from the caller, nothing is left
	 * to care about this one.
	 */
	exit(1);
}
