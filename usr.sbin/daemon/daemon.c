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

#include <sys/event.h>
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
#define SYSLOG_NAMES
#include <syslog.h>
#include <time.h>
#include <assert.h>

/* 1 year in seconds */
#define MAX_RESTART_DELAY 60*60*24*365

/* Maximum number of restarts */
#define MAX_RESTART_COUNT 128

#define LBUF_SIZE 4096

enum daemon_mode {
	MODE_DAEMON = 0,   /* simply daemonize, no supervision */
	MODE_SUPERVISE,    /* initial supervision state */
	MODE_TERMINATING,  /* user requested termination */
	MODE_NOCHILD,      /* child is terminated, final state of the event loop */
};


struct daemon_state {
	unsigned char buf[LBUF_SIZE];
	size_t pos;
	char **argv;
	const char *child_pidfile;
	const char *parent_pidfile;
	const char *output_filename;
	const char *syslog_tag;
	const char *title;
	const char *user;
	struct pidfh *parent_pidfh;
	struct pidfh *child_pidfh;
	enum daemon_mode mode;
	int pid;
	int pipe_rd;
	int pipe_wr;
	int keep_cur_workdir;
	int kqueue_fd;
	int restart_delay;
	int stdmask;
	int syslog_priority;
	int syslog_facility;
	int keep_fds_open;
	int output_fd;
	bool restart_enabled;
	bool syslog_enabled;
	bool log_reopen;
	int restart_count;
	int restarted_count;
};

static void restrict_process(const char *);
static int  open_log(const char *);
static void reopen_log(struct daemon_state *);
static bool listen_child(struct daemon_state *);
static int  get_log_mapping(const char *, const CODE *);
static void open_pid_files(struct daemon_state *);
static void do_output(const unsigned char *, size_t, struct daemon_state *);
static void daemon_sleep(struct daemon_state *);
static void daemon_state_init(struct daemon_state *);
static void daemon_eventloop(struct daemon_state *);
static void daemon_terminate(struct daemon_state *);
static void daemon_exec(struct daemon_state *);
static bool daemon_is_child_dead(struct daemon_state *);
static void daemon_set_child_pipe(struct daemon_state *);
static int daemon_setup_kqueue(void);

static int pidfile_truncate(struct pidfh *);

static const char shortopts[] = "+cfHSp:P:ru:o:s:l:t:m:R:T:C:h";

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
	{ "restart-count",      required_argument,      NULL,           'C' },
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
	    "              [-C restart_count]\n"
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
	    "  --restart-count      -C <N>     Restart child at most N times, then exit\n"
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
	const char *e = NULL;
	int ch = 0;
	struct daemon_state state;

	daemon_state_init(&state);

	/* Signals are processed via kqueue */
	signal(SIGHUP, SIG_IGN);
	signal(SIGTERM, SIG_IGN);

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
		case 'C':
			state.restart_count = (int)strtonum(optarg, 0,
			    MAX_RESTART_COUNT, &e);
			if (e != NULL) {
				errx(6, "invalid restart count: %s", e);
			}
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
			state.mode = MODE_SUPERVISE;
			break;
		case 'm':
			state.stdmask = (int)strtonum(optarg, 0, 3, &e);
			if (e != NULL) {
				errx(6, "unrecognized listening mask: %s", e);
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
			state.mode = MODE_SUPERVISE;
			break;
		case 'p':
			state.child_pidfile = optarg;
			state.mode = MODE_SUPERVISE;
			break;
		case 'P':
			state.parent_pidfile = optarg;
			state.mode = MODE_SUPERVISE;
			break;
		case 'r':
			state.restart_enabled = true;
			state.mode = MODE_SUPERVISE;
			break;
		case 'R':
			state.restart_enabled = true;
			state.restart_delay = (int)strtonum(optarg, 1,
			    MAX_RESTART_DELAY, &e);
			if (e != NULL) {
				errx(6, "invalid restart delay: %s", e);
			}
			state.mode = MODE_SUPERVISE;
			break;
		case 's':
			state.syslog_priority = get_log_mapping(optarg,
			    prioritynames);
			if (state.syslog_priority == -1) {
				errx(4, "unrecognized syslog priority");
			}
			state.syslog_enabled = true;
			state.mode = MODE_SUPERVISE;
			break;
		case 'S':
			state.syslog_enabled = true;
			state.mode = MODE_SUPERVISE;
			break;
		case 't':
			state.title = optarg;
			break;
		case 'T':
			state.syslog_tag = optarg;
			state.syslog_enabled = true;
			state.mode = MODE_SUPERVISE;
			break;
		case 'u':
			state.user = optarg;
			break;
		case 'h':
			usage(0);
			__unreachable();
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

	/*
	 * TODO: add feature to avoid backgrounding
	 * i.e. --foreground, -f
	 */
	if (daemon(state.keep_cur_workdir, state.keep_fds_open) == -1) {
		warn("daemon");
		daemon_terminate(&state);
	}

	if (state.mode == MODE_DAEMON) {
		daemon_exec(&state);
	}

	/* Write out parent pidfile if needed. */
	pidfile_write(state.parent_pidfh);

	state.kqueue_fd = daemon_setup_kqueue();

	do {
		state.mode = MODE_SUPERVISE;
		daemon_eventloop(&state);
		daemon_sleep(&state);
		if (state.restart_enabled && state.restart_count > -1) {
			if (state.restarted_count >= state.restart_count) {
				state.restart_enabled = false;
			}
			state.restarted_count++;
		}
	} while (state.restart_enabled);

	daemon_terminate(&state);
}

static void
daemon_exec(struct daemon_state *state)
{
	pidfile_write(state->child_pidfh);

	if (state->user != NULL) {
		restrict_process(state->user);
	}

	/* Ignored signals remain ignored after execve, unignore them */
	signal(SIGHUP, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	execvp(state->argv[0], state->argv);
	/* execvp() failed - report error and exit this process */
	err(1, "%s", state->argv[0]);
}

/* Main event loop: fork the child and watch for events.
 * After SIGTERM is received and propagated to the child there are
 * several options on what to do next:
 * - read until EOF
 * - read until EOF but only for a while
 * - bail immediately
 * Currently the third option is used, because otherwise there is no
 * guarantee that read() won't block indefinitely if the child refuses
 * to depart. To handle the second option, a different approach
 * would be needed (procctl()?).
 */
static void
daemon_eventloop(struct daemon_state *state)
{
	struct kevent event;
	int kq;
	int ret;
	int pipe_fd[2];

	/*
	 * Try to protect against pageout kill. Ignore the
	 * error, madvise(2) will fail only if a process does
	 * not have superuser privileges.
	 */
	(void)madvise(NULL, 0, MADV_PROTECT);

	if (pipe(pipe_fd)) {
		err(1, "pipe");
	}
	state->pipe_rd = pipe_fd[0];
	state->pipe_wr = pipe_fd[1];

	kq = state->kqueue_fd;
	EV_SET(&event, state->pipe_rd, EVFILT_READ, EV_ADD|EV_CLEAR, 0, 0,
	    NULL);
	if (kevent(kq, &event, 1, NULL, 0, NULL) == -1) {
		err(EXIT_FAILURE, "failed to register kevent");
	}

	memset(&event, 0, sizeof(struct kevent));

	/* Spawn a child to exec the command. */
	state->pid = fork();

	/* fork failed, this can only happen when supervision is enabled */
	switch (state->pid) {
	case -1:
		warn("fork");
		state->mode = MODE_NOCHILD;
		return;
	/* fork succeeded, this is child's branch */
	case 0:
		close(kq);
		daemon_set_child_pipe(state);
		daemon_exec(state);
		break;
	}

	/* case: pid > 0; fork succeeded */
	close(state->pipe_wr);
	state->pipe_wr = -1;
	setproctitle("%s[%d]", state->title, (int)state->pid);
	setbuf(stdout, NULL);

	while (state->mode != MODE_NOCHILD) {
		ret = kevent(kq, NULL, 0, &event, 1, NULL);
		switch (ret) {
		case -1:
			if (errno == EINTR)
				continue;
			err(EXIT_FAILURE, "kevent wait");
		case 0:
			continue;
		}

		if (event.flags & EV_ERROR) {
			errx(EXIT_FAILURE, "Event error: %s",
			    strerror((int)event.data));
		}

		switch (event.filter) {
		case EVFILT_SIGNAL:

			switch (event.ident) {
			case SIGCHLD:
				if (daemon_is_child_dead(state)) {
					/* child is dead, read all until EOF */
					state->pid = -1;
					state->mode = MODE_NOCHILD;
					while (listen_child(state)) {
						continue;
					}
				}
				continue;
			case SIGTERM:
				if (state->mode != MODE_SUPERVISE) {
					/* user is impatient */
					/* TODO: warn about repeated SIGTERM? */
					continue;
				}

				state->mode = MODE_TERMINATING;
				state->restart_enabled = false;
				if (state->pid > 0) {
					kill(state->pid, SIGTERM);
				}
				/*
				 * TODO set kevent timer to exit
				 * unconditionally after some time
				 */
				continue;
			case SIGHUP:
				if (state->log_reopen && state->output_fd >= 0) {
					reopen_log(state);
				}
				continue;
			}
			break;

		case EVFILT_READ:
			/*
			 * detecting EOF is no longer necessary
			 * if child closes the pipe daemon will stop getting
			 * EVFILT_READ events
			 */

			if (event.data > 0) {
				(void)listen_child(state);
			}
			continue;
		default:
			assert(0 && "Unexpected kevent filter type");
			continue;
		}
	}

	/* EVFILT_READ kqueue filter goes away here. */
	close(state->pipe_rd);
	state->pipe_rd = -1;

	/*
	 * We don't have to truncate the pidfile, but it's easier to test
	 * daemon(8) behavior in some respects if we do.  We won't bother if
	 * the child won't be restarted.
	 */
	if (state->child_pidfh != NULL && state->restart_enabled) {
		pidfile_truncate(state->child_pidfh);
	}
}

/*
 * Note that daemon_sleep() should not be called with anything but the signal
 * events in the kqueue without further consideration.
 */
static void
daemon_sleep(struct daemon_state *state)
{
	struct kevent event = { 0 };
	int ret;

	assert(state->pipe_rd == -1);
	assert(state->pipe_wr == -1);

	if (!state->restart_enabled) {
		return;
	}

	EV_SET(&event, 0, EVFILT_TIMER, EV_ADD|EV_ONESHOT, NOTE_SECONDS,
	    state->restart_delay, NULL);
	if (kevent(state->kqueue_fd, &event, 1, NULL, 0, NULL) == -1) {
		err(1, "failed to register timer");
	}

	for (;;) {
		ret = kevent(state->kqueue_fd, NULL, 0, &event, 1, NULL);
		if (ret == -1) {
			if (errno != EINTR) {
				err(1, "kevent");
			}

			continue;
		}

		/*
		 * Any other events being raised are indicative of a problem
		 * that we need to investigate.  Most likely being that
		 * something was not cleaned up from the eventloop.
		 */
		assert(event.filter == EVFILT_TIMER ||
		    event.filter == EVFILT_SIGNAL);

		if (event.filter == EVFILT_TIMER) {
			/* Break's over, back to work. */
			break;
		}

		/* Process any pending signals. */
		switch (event.ident) {
		case SIGTERM:
			/*
			 * We could disarm the timer, but we'll be terminating
			 * promptly anyways.
			 */
			state->restart_enabled = false;
			return;
		case SIGHUP:
			if (state->log_reopen && state->output_fd >= 0) {
				reopen_log(state);
			}

			break;
		case SIGCHLD:
		default:
			/* Discard */
			break;
		}
	}

	/* SIGTERM should've returned immediately. */
	assert(state->restart_enabled);
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
listen_child(struct daemon_state *state)
{
	ssize_t rv;
	unsigned char *cp;

	assert(state != NULL);
	assert(state->pos < LBUF_SIZE - 1);

	rv = read(state->pipe_rd, state->buf + state->pos,
	    LBUF_SIZE - state->pos - 1);
	if (rv > 0) {
		state->pos += rv;
		assert(state->pos <= LBUF_SIZE - 1);
		/* Always NUL-terminate just in case. */
		state->buf[LBUF_SIZE - 1] = '\0';

		/*
		 * Find position of the last newline in the buffer.
		 * The buffer is guaranteed to have one or more complete lines
		 * if at least one newline was found when searching in reverse.
		 * All complete lines are flushed.
		 * This does not take NUL characters into account.
		 */
		cp = memrchr(state->buf, '\n', state->pos);
		if (cp != NULL) {
			size_t bytes_line = cp - state->buf + 1;
			assert(bytes_line <= state->pos);
			do_output(state->buf, bytes_line, state);
			state->pos -= bytes_line;
			memmove(state->buf, cp + 1, state->pos);
		}
		/* Wait until the buffer is full. */
		if (state->pos < LBUF_SIZE - 1) {
			return true;
		}
		do_output(state->buf, state->pos, state);
		state->pos = 0;
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
	if (state->pos > 0) {
		do_output(state->buf, state->pos, state);
		state->pos = 0;
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

static int
open_log(const char *outfn)
{

	return open(outfn, O_CREAT | O_WRONLY | O_APPEND | O_CLOEXEC, 0600);
}

static void
reopen_log(struct daemon_state *state)
{
	int outfd;

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
		.buf = {0},
		.pos = 0,
		.argv = NULL,
		.parent_pidfh = NULL,
		.child_pidfh = NULL,
		.child_pidfile = NULL,
		.parent_pidfile = NULL,
		.title = NULL,
		.user = NULL,
		.mode = MODE_DAEMON,
		.restart_enabled = false,
		.pid = 0,
		.pipe_rd = -1,
		.pipe_wr = -1,
		.keep_cur_workdir = 1,
		.kqueue_fd = -1,
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
		.restart_count = -1,
		.restarted_count = 0
	};
}

static _Noreturn void
daemon_terminate(struct daemon_state *state)
{
	assert(state != NULL);

	if (state->kqueue_fd >= 0) {
		close(state->kqueue_fd);
	}
	if (state->output_fd >= 0) {
		close(state->output_fd);
	}
	if (state->pipe_rd >= 0) {
		close(state->pipe_rd);
	}

	if (state->pipe_wr >= 0) {
		close(state->pipe_wr);
	}
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

/*
 * Returns true if SIGCHILD came from state->pid due to its exit.
 */
static bool
daemon_is_child_dead(struct daemon_state *state)
{
	int status;

	for (;;) {
		int who = waitpid(-1, &status, WNOHANG);
		if (state->pid == who && (WIFEXITED(status) ||
		    WIFSIGNALED(status))) {
			return true;
		}
		if (who == 0) {
			return false;
		}
		if (who == -1 && errno != EINTR) {
			warn("waitpid");
			return false;
		}
	}
}

static void
daemon_set_child_pipe(struct daemon_state *state)
{
	if (state->stdmask & STDERR_FILENO) {
		if (dup2(state->pipe_wr, STDERR_FILENO) == -1) {
			err(1, "dup2");
		}
	}
	if (state->stdmask & STDOUT_FILENO) {
		if (dup2(state->pipe_wr, STDOUT_FILENO) == -1) {
			err(1, "dup2");
		}
	}
	if (state->pipe_wr != STDERR_FILENO &&
	    state->pipe_wr != STDOUT_FILENO) {
		close(state->pipe_wr);
	}

	/* The child gets dup'd pipes. */
	close(state->pipe_rd);
}

static int
daemon_setup_kqueue(void)
{
	int kq;
	struct kevent event = { 0 };

	kq = kqueuex(KQUEUE_CLOEXEC);
	if (kq == -1) {
		err(EXIT_FAILURE, "kqueue");
	}

	EV_SET(&event, SIGHUP,  EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
	if (kevent(kq, &event, 1, NULL, 0, NULL) == -1) {
		err(EXIT_FAILURE, "failed to register kevent");
	}

	EV_SET(&event, SIGTERM, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
	if (kevent(kq, &event, 1, NULL, 0, NULL) == -1) {
		err(EXIT_FAILURE, "failed to register kevent");
	}

	EV_SET(&event, SIGCHLD, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
	if (kevent(kq, &event, 1, NULL, 0, NULL) == -1) {
		err(EXIT_FAILURE, "failed to register kevent");
	}

	return (kq);
}

static int
pidfile_truncate(struct pidfh *pfh)
{
	int pfd = pidfile_fileno(pfh);

	assert(pfd >= 0);

	if (ftruncate(pfd, 0) == -1)
		return (-1);

	/*
	 * pidfile_write(3) will always pwrite(..., 0) today, but let's assume
	 * it may not always and do a best-effort reset of the position just to
	 * set a good example.
	 */
	(void)lseek(pfd, 0, SEEK_SET);
	return (0);
}
