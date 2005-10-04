/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Donn Seeley at Berkeley Software Design, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)init.c	8.1 (Berkeley) 7/15/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <db.h>
#include <errno.h>
#include <fcntl.h>
#include <libutil.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <ttyent.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <err.h>

#include <stdarg.h>

#ifdef SECURE
#include <pwd.h>
#endif

#ifdef LOGIN_CAP
#include <login_cap.h>
#endif

#include "pathnames.h"

/*
 * Sleep times; used to prevent thrashing.
 */
#define	GETTY_SPACING		 5	/* N secs minimum getty spacing */
#define	GETTY_SLEEP		30	/* sleep N secs after spacing problem */
#define GETTY_NSPACE             3      /* max. spacing count to bring reaction */
#define	WINDOW_WAIT		 3	/* wait N secs after starting window */
#define	STALL_TIMEOUT		30	/* wait N secs after warning */
#define	DEATH_WATCH		10	/* wait N secs for procs to die */
#define DEATH_SCRIPT		120	/* wait for 2min for /etc/rc.shutdown */
#define RESOURCE_RC		"daemon"
#define RESOURCE_WINDOW 	"default"
#define RESOURCE_GETTY		"default"

void handle(sig_t, ...);
void delset(sigset_t *, ...);

void stall(const char *, ...) __printflike(1, 2);
void warning(const char *, ...) __printflike(1, 2);
void emergency(const char *, ...) __printflike(1, 2);
void disaster(int);
void badsys(int);
int  runshutdown(void);
static char *strk(char *);

/*
 * We really need a recursive typedef...
 * The following at least guarantees that the return type of (*state_t)()
 * is sufficiently wide to hold a function pointer.
 */
typedef long (*state_func_t)(void);
typedef state_func_t (*state_t)(void);

state_func_t single_user(void);
state_func_t runcom(void);
state_func_t read_ttys(void);
state_func_t multi_user(void);
state_func_t clean_ttys(void);
state_func_t catatonia(void);
state_func_t death(void);

enum { AUTOBOOT, FASTBOOT } runcom_mode = AUTOBOOT;
#define FALSE	0
#define TRUE	1

int Reboot = FALSE;
int howto = RB_AUTOBOOT;

int devfs;

void transition(state_t);
state_t requested_transition = runcom;

void setctty(const char *);

typedef struct init_session {
	int	se_index;		/* index of entry in ttys file */
	pid_t	se_process;		/* controlling process */
	time_t	se_started;		/* used to avoid thrashing */
	int	se_flags;		/* status of session */
#define	SE_SHUTDOWN	0x1		/* session won't be restarted */
#define	SE_PRESENT	0x2		/* session is in /etc/ttys */
	int     se_nspace;              /* spacing count */
	char	*se_device;		/* filename of port */
	char	*se_getty;		/* what to run on that port */
	char    *se_getty_argv_space;   /* pre-parsed argument array space */
	char	**se_getty_argv;	/* pre-parsed argument array */
	char	*se_window;		/* window system (started only once) */
	char    *se_window_argv_space;  /* pre-parsed argument array space */
	char	**se_window_argv;	/* pre-parsed argument array */
	char    *se_type;               /* default terminal type */
	struct	init_session *se_prev;
	struct	init_session *se_next;
} session_t;

void free_session(session_t *);
session_t *new_session(session_t *, int, struct ttyent *);
session_t *sessions;

char **construct_argv(char *);
void start_window_system(session_t *);
void collect_child(pid_t);
pid_t start_getty(session_t *);
void transition_handler(int);
void alrm_handler(int);
void setsecuritylevel(int);
int getsecuritylevel(void);
int setupargv(session_t *, struct ttyent *);
#ifdef LOGIN_CAP
void setprocresources(const char *);
#endif
int clang;

void clear_session_logs(session_t *);

int start_session_db(void);
void add_session(session_t *);
void del_session(session_t *);
session_t *find_session(pid_t);
DB *session_db;

/*
 * The mother of all processes.
 */
int
main(int argc, char *argv[])
{
	int c;
	struct sigaction sa;
	sigset_t mask;


	/* Dispose of random users. */
	if (getuid() != 0)
		errx(1, "%s", strerror(EPERM));

	/* System V users like to reexec init. */
	if (getpid() != 1) {
#ifdef COMPAT_SYSV_INIT
		/* So give them what they want */
		if (argc > 1) {
			if (strlen(argv[1]) == 1) {
				char runlevel = *argv[1];
				int sig;

				switch (runlevel) {
					case '0': /* halt + poweroff */
						sig = SIGUSR2;
						break;
					case '1': /* single-user */
						sig = SIGTERM;
						break;
					case '6': /* reboot */
						sig = SIGINT;
						break;
					case 'c': /* block further logins */
						sig = SIGTSTP;
						break;
					case 'q': /* rescan /etc/ttys */
						sig = SIGHUP;
						break;
					default:
						goto invalid;
				}
				kill(1, sig);
				_exit(0);
			} else
invalid:
				errx(1, "invalid run-level ``%s''", argv[1]);
		} else
#endif
			errx(1, "already running");
	}
	/*
	 * Note that this does NOT open a file...
	 * Does 'init' deserve its own facility number?
	 */
	openlog("init", LOG_CONS|LOG_ODELAY, LOG_AUTH);

	/*
	 * Create an initial session.
	 */
	if (setsid() < 0)
		warning("initial setsid() failed: %m");

	/*
	 * Establish an initial user so that programs running
	 * single user do not freak out and die (like passwd).
	 */
	if (setlogin("root") < 0)
		warning("setlogin() failed: %m");

	/*
	 * This code assumes that we always get arguments through flags,
	 * never through bits set in some random machine register.
	 */
	while ((c = getopt(argc, argv, "dsf")) != -1)
		switch (c) {
		case 'd':
			devfs = 1;
			break;
		case 's':
			requested_transition = single_user;
			break;
		case 'f':
			runcom_mode = FASTBOOT;
			break;
		default:
			warning("unrecognized flag '-%c'", c);
			break;
		}

	if (optind != argc)
		warning("ignoring excess arguments");

	if (devfs) {
		struct iovec iov[4];
		char *s;
		int i;

		char _fstype[]	= "fstype";
		char _devfs[]	= "devfs";
		char _fspath[]	= "fspath";
		char _path_dev[]= _PATH_DEV;

		iov[0].iov_base = _fstype;
		iov[0].iov_len = sizeof(_fstype);
		iov[1].iov_base = _devfs;
		iov[1].iov_len = sizeof(_devfs);
		iov[2].iov_base = _fspath;
		iov[2].iov_len = sizeof(_fspath);
		/* 
		 * Try to avoid the trailing slash in _PATH_DEV.
		 * Be *very* defensive.
		 */
		s = strdup(_PATH_DEV);
		if (s != NULL) {
			i = strlen(s);
			if (i > 0 && s[i - 1] == '/')
				s[i - 1] = '\0';
			iov[3].iov_base = s;
			iov[3].iov_len = strlen(s) + 1;
		} else {
			iov[3].iov_base = _path_dev;
			iov[3].iov_len = sizeof(_path_dev);
		}
		nmount(iov, 4, 0);
		if (s != NULL)
			free(s);
	}

	/*
	 * We catch or block signals rather than ignore them,
	 * so that they get reset on exec.
	 */
	handle(badsys, SIGSYS, 0);
	handle(disaster, SIGABRT, SIGFPE, SIGILL, SIGSEGV,
	       SIGBUS, SIGXCPU, SIGXFSZ, 0);
	handle(transition_handler, SIGHUP, SIGINT, SIGTERM, SIGTSTP,
		SIGUSR1, SIGUSR2, 0);
	handle(alrm_handler, SIGALRM, 0);
	sigfillset(&mask);
	delset(&mask, SIGABRT, SIGFPE, SIGILL, SIGSEGV, SIGBUS, SIGSYS,
		SIGXCPU, SIGXFSZ, SIGHUP, SIGINT, SIGTERM, SIGTSTP, SIGALRM, 
		SIGUSR1, SIGUSR2, 0);
	sigprocmask(SIG_SETMASK, &mask, (sigset_t *) 0);
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = SIG_IGN;
	(void) sigaction(SIGTTIN, &sa, (struct sigaction *)0);
	(void) sigaction(SIGTTOU, &sa, (struct sigaction *)0);

	/*
	 * Paranoia.
	 */
	close(0);
	close(1);
	close(2);

	/*
	 * Start the state machine.
	 */
	transition(requested_transition);

	/*
	 * Should never reach here.
	 */
	return 1;
}

/*
 * Associate a function with a signal handler.
 */
void
handle(sig_t handler, ...)
{
	int sig;
	struct sigaction sa;
	sigset_t mask_everything;
	va_list ap;
	va_start(ap, handler);

	sa.sa_handler = handler;
	sigfillset(&mask_everything);

	while ((sig = va_arg(ap, int)) != 0) {
		sa.sa_mask = mask_everything;
		/* XXX SA_RESTART? */
		sa.sa_flags = sig == SIGCHLD ? SA_NOCLDSTOP : 0;
		sigaction(sig, &sa, (struct sigaction *) 0);
	}
	va_end(ap);
}

/*
 * Delete a set of signals from a mask.
 */
void
delset(sigset_t *maskp, ...)
{
	int sig;
	va_list ap;
	va_start(ap, maskp);

	while ((sig = va_arg(ap, int)) != 0)
		sigdelset(maskp, sig);
	va_end(ap);
}

/*
 * Log a message and sleep for a while (to give someone an opportunity
 * to read it and to save log or hardcopy output if the problem is chronic).
 * NB: should send a message to the session logger to avoid blocking.
 */
void
stall(const char *message, ...)
{
	va_list ap;
	va_start(ap, message);

	vsyslog(LOG_ALERT, message, ap);
	va_end(ap);
	sleep(STALL_TIMEOUT);
}

/*
 * Like stall(), but doesn't sleep.
 * If cpp had variadic macros, the two functions could be #defines for another.
 * NB: should send a message to the session logger to avoid blocking.
 */
void
warning(const char *message, ...)
{
	va_list ap;
	va_start(ap, message);

	vsyslog(LOG_ALERT, message, ap);
	va_end(ap);
}

/*
 * Log an emergency message.
 * NB: should send a message to the session logger to avoid blocking.
 */
void
emergency(const char *message, ...)
{
	va_list ap;
	va_start(ap, message);

	vsyslog(LOG_EMERG, message, ap);
	va_end(ap);
}

/*
 * Catch a SIGSYS signal.
 *
 * These may arise if a system does not support sysctl.
 * We tolerate up to 25 of these, then throw in the towel.
 */
void
badsys(int sig)
{
	static int badcount = 0;

	if (badcount++ < 25)
		return;
	disaster(sig);
}

/*
 * Catch an unexpected signal.
 */
void
disaster(int sig)
{
	emergency("fatal signal: %s",
		(unsigned)sig < NSIG ? sys_siglist[sig] : "unknown signal");

	sleep(STALL_TIMEOUT);
	_exit(sig);		/* reboot */
}

/*
 * Get the security level of the kernel.
 */
int
getsecuritylevel(void)
{
#ifdef KERN_SECURELVL
	int name[2], curlevel;
	size_t len;

	name[0] = CTL_KERN;
	name[1] = KERN_SECURELVL;
	len = sizeof curlevel;
	if (sysctl(name, 2, &curlevel, &len, NULL, 0) == -1) {
		emergency("cannot get kernel security level: %s",
		    strerror(errno));
		return (-1);
	}
	return (curlevel);
#else
	return (-1);
#endif
}

/*
 * Set the security level of the kernel.
 */
void
setsecuritylevel(int newlevel)
{
#ifdef KERN_SECURELVL
	int name[2], curlevel;

	curlevel = getsecuritylevel();
	if (newlevel == curlevel)
		return;
	name[0] = CTL_KERN;
	name[1] = KERN_SECURELVL;
	if (sysctl(name, 2, NULL, NULL, &newlevel, sizeof newlevel) == -1) {
		emergency(
		    "cannot change kernel security level from %d to %d: %s",
		    curlevel, newlevel, strerror(errno));
		return;
	}
#ifdef SECURE
	warning("kernel security level changed from %d to %d",
	    curlevel, newlevel);
#endif
#endif
}

/*
 * Change states in the finite state machine.
 * The initial state is passed as an argument.
 */
void
transition(state_t s)
{
	for (;;)
		s = (state_t) (*s)();
}

/*
 * Close out the accounting files for a login session.
 * NB: should send a message to the session logger to avoid blocking.
 */
void
clear_session_logs(session_t *sp)
{
	char *line = sp->se_device + sizeof(_PATH_DEV) - 1;

	if (logout(line))
		logwtmp(line, "", "");
}

/*
 * Start a session and allocate a controlling terminal.
 * Only called by children of init after forking.
 */
void
setctty(const char *name)
{
	int fd;

	(void) revoke(name);
	if ((fd = open(name, O_RDWR)) == -1) {
		stall("can't open %s: %m", name);
		_exit(1);
	}
	if (login_tty(fd) == -1) {
		stall("can't get %s for controlling terminal: %m", name);
		_exit(1);
	}
}

/*
 * Bring the system up single user.
 */
state_func_t
single_user(void)
{
	pid_t pid, wpid;
	int status;
	sigset_t mask;
	const char *shell = _PATH_BSHELL;
	char *argv[2];
#ifdef SECURE
	struct ttyent *typ;
	struct passwd *pp;
	static const char banner[] =
		"Enter root password, or ^D to go multi-user\n";
	char *clear, *password;
#endif
#ifdef DEBUGSHELL
	char altshell[128];
#endif

	if (Reboot) {
		/* Instead of going single user, let's reboot the machine */
		sync();
		alarm(2);
		pause();
		reboot(howto);
		_exit(0);
	}

	if ((pid = fork()) == 0) {
		/*
		 * Start the single user session.
		 */
		setctty(_PATH_CONSOLE);

#ifdef SECURE
		/*
		 * Check the root password.
		 * We don't care if the console is 'on' by default;
		 * it's the only tty that can be 'off' and 'secure'.
		 */
		typ = getttynam("console");
		pp = getpwnam("root");
		if (typ && (typ->ty_status & TTY_SECURE) == 0 &&
		    pp && *pp->pw_passwd) {
			write(STDERR_FILENO, banner, sizeof banner - 1);
			for (;;) {
				clear = getpass("Password:");
				if (clear == 0 || *clear == '\0')
					_exit(0);
				password = crypt(clear, pp->pw_passwd);
				bzero(clear, _PASSWORD_LEN);
				if (strcmp(password, pp->pw_passwd) == 0)
					break;
				warning("single-user login failed\n");
			}
		}
		endttyent();
		endpwent();
#endif /* SECURE */

#ifdef DEBUGSHELL
		{
			char *cp = altshell;
			int num;

#define	SHREQUEST \
	"Enter full pathname of shell or RETURN for " _PATH_BSHELL ": "
			(void)write(STDERR_FILENO,
			    SHREQUEST, sizeof(SHREQUEST) - 1);
			while ((num = read(STDIN_FILENO, cp, 1)) != -1 &&
			    num != 0 && *cp != '\n' && cp < &altshell[127])
					cp++;
			*cp = '\0';
			if (altshell[0] != '\0')
				shell = altshell;
		}
#endif /* DEBUGSHELL */

		/*
		 * Unblock signals.
		 * We catch all the interesting ones,
		 * and those are reset to SIG_DFL on exec.
		 */
		sigemptyset(&mask);
		sigprocmask(SIG_SETMASK, &mask, (sigset_t *) 0);

		/*
		 * Fire off a shell.
		 * If the default one doesn't work, try the Bourne shell.
		 */

		char name[] = "-sh";

		argv[0] = name;
		argv[1] = 0;
		execv(shell, argv);
		emergency("can't exec %s for single user: %m", shell);
		execv(_PATH_BSHELL, argv);
		emergency("can't exec %s for single user: %m", _PATH_BSHELL);
		sleep(STALL_TIMEOUT);
		_exit(1);
	}

	if (pid == -1) {
		/*
		 * We are seriously hosed.  Do our best.
		 */
		emergency("can't fork single-user shell, trying again");
		while (waitpid(-1, (int *) 0, WNOHANG) > 0)
			continue;
		return (state_func_t) single_user;
	}

	requested_transition = 0;
	do {
		if ((wpid = waitpid(-1, &status, WUNTRACED)) != -1)
			collect_child(wpid);
		if (wpid == -1) {
			if (errno == EINTR)
				continue;
			warning("wait for single-user shell failed: %m; restarting");
			return (state_func_t) single_user;
		}
		if (wpid == pid && WIFSTOPPED(status)) {
			warning("init: shell stopped, restarting\n");
			kill(pid, SIGCONT);
			wpid = -1;
		}
	} while (wpid != pid && !requested_transition);

	if (requested_transition)
		return (state_func_t) requested_transition;

	if (!WIFEXITED(status)) {
		if (WTERMSIG(status) == SIGKILL) {
			/*
			 *  reboot(8) killed shell?
			 */
			warning("single user shell terminated.");
			sleep(STALL_TIMEOUT);
			_exit(0);
		} else {
			warning("single user shell terminated, restarting");
			return (state_func_t) single_user;
		}
	}

	runcom_mode = FASTBOOT;
	return (state_func_t) runcom;
}

/*
 * Run the system startup script.
 */
state_func_t
runcom(void)
{
	pid_t pid, wpid;
	int status;
	char *argv[4];
	struct sigaction sa;

	if ((pid = fork()) == 0) {
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sa.sa_handler = SIG_IGN;
		(void) sigaction(SIGTSTP, &sa, (struct sigaction *)0);
		(void) sigaction(SIGHUP, &sa, (struct sigaction *)0);

		setctty(_PATH_CONSOLE);

		char _sh[]	 	= "sh";
		char _path_runcom[]	= _PATH_RUNCOM;
		char _autoboot[]	= "autoboot";

		argv[0] = _sh;
		argv[1] = _path_runcom;
		argv[2] = runcom_mode == AUTOBOOT ? _autoboot : 0;
		argv[3] = 0;

		sigprocmask(SIG_SETMASK, &sa.sa_mask, (sigset_t *) 0);

#ifdef LOGIN_CAP
		setprocresources(RESOURCE_RC);
#endif
		execv(_PATH_BSHELL, argv);
		stall("can't exec %s for %s: %m", _PATH_BSHELL, _PATH_RUNCOM);
		_exit(1);	/* force single user mode */
	}

	if (pid == -1) {
		emergency("can't fork for %s on %s: %m",
			_PATH_BSHELL, _PATH_RUNCOM);
		while (waitpid(-1, (int *) 0, WNOHANG) > 0)
			continue;
		sleep(STALL_TIMEOUT);
		return (state_func_t) single_user;
	}

	/*
	 * Copied from single_user().  This is a bit paranoid.
	 */
	requested_transition = 0;
	do {
		if ((wpid = waitpid(-1, &status, WUNTRACED)) != -1)
			collect_child(wpid);
		if (wpid == -1) {
			if (requested_transition == death)
				return (state_func_t) death;
			if (errno == EINTR)
				continue;
			warning("wait for %s on %s failed: %m; going to single user mode",
				_PATH_BSHELL, _PATH_RUNCOM);
			return (state_func_t) single_user;
		}
		if (wpid == pid && WIFSTOPPED(status)) {
			warning("init: %s on %s stopped, restarting\n",
				_PATH_BSHELL, _PATH_RUNCOM);
			kill(pid, SIGCONT);
			wpid = -1;
		}
	} while (wpid != pid);

	if (WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM &&
	    requested_transition == catatonia) {
		/* /etc/rc executed /sbin/reboot; wait for the end quietly */
		sigset_t s;

		sigfillset(&s);
		for (;;)
			sigsuspend(&s);
	}

	if (!WIFEXITED(status)) {
		warning("%s on %s terminated abnormally, going to single user mode",
			_PATH_BSHELL, _PATH_RUNCOM);
		return (state_func_t) single_user;
	}

	if (WEXITSTATUS(status))
		return (state_func_t) single_user;

	runcom_mode = AUTOBOOT;		/* the default */
	/* NB: should send a message to the session logger to avoid blocking. */
	logwtmp("~", "reboot", "");
	return (state_func_t) read_ttys;
}

/*
 * Open the session database.
 *
 * NB: We could pass in the size here; is it necessary?
 */
int
start_session_db(void)
{
	if (session_db && (*session_db->close)(session_db))
		emergency("session database close: %s", strerror(errno));
	if ((session_db = dbopen(NULL, O_RDWR, 0, DB_HASH, NULL)) == 0) {
		emergency("session database open: %s", strerror(errno));
		return (1);
	}
	return (0);

}

/*
 * Add a new login session.
 */
void
add_session(session_t *sp)
{
	DBT key;
	DBT data;

	key.data = &sp->se_process;
	key.size = sizeof sp->se_process;
	data.data = &sp;
	data.size = sizeof sp;

	if ((*session_db->put)(session_db, &key, &data, 0))
		emergency("insert %d: %s", sp->se_process, strerror(errno));
}

/*
 * Delete an old login session.
 */
void
del_session(session_t *sp)
{
	DBT key;

	key.data = &sp->se_process;
	key.size = sizeof sp->se_process;

	if ((*session_db->del)(session_db, &key, 0))
		emergency("delete %d: %s", sp->se_process, strerror(errno));
}

/*
 * Look up a login session by pid.
 */
session_t *
find_session(pid_t pid)
{
	DBT key;
	DBT data;
	session_t *ret;

	key.data = &pid;
	key.size = sizeof pid;
	if ((*session_db->get)(session_db, &key, &data, 0) != 0)
		return 0;
	bcopy(data.data, (char *)&ret, sizeof(ret));
	return ret;
}

/*
 * Construct an argument vector from a command line.
 */
char **
construct_argv(char *command)
{
	int argc = 0;
	char **argv = (char **) malloc(((strlen(command) + 1) / 2 + 1)
						* sizeof (char *));

	if ((argv[argc++] = strk(command)) == 0) {
		free(argv);
		return (NULL);
	}
	while ((argv[argc++] = strk((char *) 0)) != NULL)
		continue;
	return argv;
}

/*
 * Deallocate a session descriptor.
 */
void
free_session(session_t *sp)
{
	free(sp->se_device);
	if (sp->se_getty) {
		free(sp->se_getty);
		free(sp->se_getty_argv_space);
		free(sp->se_getty_argv);
	}
	if (sp->se_window) {
		free(sp->se_window);
		free(sp->se_window_argv_space);
		free(sp->se_window_argv);
	}
	if (sp->se_type)
		free(sp->se_type);
	free(sp);
}

/*
 * Allocate a new session descriptor.
 * Mark it SE_PRESENT.
 */
session_t *
new_session(session_t *sprev, int session_index, struct ttyent *typ)
{
	session_t *sp;
	int fd;

	if ((typ->ty_status & TTY_ON) == 0 ||
	    typ->ty_name == 0 ||
	    typ->ty_getty == 0)
		return 0;

	sp = (session_t *) calloc(1, sizeof (session_t));

	sp->se_index = session_index;
	sp->se_flags |= SE_PRESENT;

	sp->se_device = malloc(sizeof(_PATH_DEV) + strlen(typ->ty_name));
	(void) sprintf(sp->se_device, "%s%s", _PATH_DEV, typ->ty_name);

	/*
	 * Attempt to open the device, if we get "device not configured"
	 * then don't add the device to the session list.
	 */
	if ((fd = open(sp->se_device, O_RDONLY | O_NONBLOCK, 0)) < 0) {
		if (errno == ENXIO) {
			free_session(sp);
			return (0);
		}
	} else
		close(fd);

	if (setupargv(sp, typ) == 0) {
		free_session(sp);
		return (0);
	}

	sp->se_next = 0;
	if (sprev == 0) {
		sessions = sp;
		sp->se_prev = 0;
	} else {
		sprev->se_next = sp;
		sp->se_prev = sprev;
	}

	return sp;
}

/*
 * Calculate getty and if useful window argv vectors.
 */
int
setupargv(session_t *sp, struct ttyent *typ)
{

	if (sp->se_getty) {
		free(sp->se_getty);
		free(sp->se_getty_argv_space);
		free(sp->se_getty_argv);
	}
	sp->se_getty = malloc(strlen(typ->ty_getty) + strlen(typ->ty_name) + 2);
	(void) sprintf(sp->se_getty, "%s %s", typ->ty_getty, typ->ty_name);
	sp->se_getty_argv_space = strdup(sp->se_getty);
	sp->se_getty_argv = construct_argv(sp->se_getty_argv_space);
	if (sp->se_getty_argv == 0) {
		warning("can't parse getty for port %s", sp->se_device);
		free(sp->se_getty);
		free(sp->se_getty_argv_space);
		sp->se_getty = sp->se_getty_argv_space = 0;
		return (0);
	}
	if (sp->se_window) {
		free(sp->se_window);
		free(sp->se_window_argv_space);
		free(sp->se_window_argv);
	}
	sp->se_window = sp->se_window_argv_space = 0;
	sp->se_window_argv = 0;
	if (typ->ty_window) {
		sp->se_window = strdup(typ->ty_window);
		sp->se_window_argv_space = strdup(sp->se_window);
		sp->se_window_argv = construct_argv(sp->se_window_argv_space);
		if (sp->se_window_argv == 0) {
			warning("can't parse window for port %s",
				sp->se_device);
			free(sp->se_window_argv_space);
			free(sp->se_window);
			sp->se_window = sp->se_window_argv_space = 0;
			return (0);
		}
	}
	if (sp->se_type)
		free(sp->se_type);
	sp->se_type = typ->ty_type ? strdup(typ->ty_type) : 0;
	return (1);
}

/*
 * Walk the list of ttys and create sessions for each active line.
 */
state_func_t
read_ttys(void)
{
	int session_index = 0;
	session_t *sp, *snext;
	struct ttyent *typ;

	/*
	 * Destroy any previous session state.
	 * There shouldn't be any, but just in case...
	 */
	for (sp = sessions; sp; sp = snext) {
		if (sp->se_process)
			clear_session_logs(sp);
		snext = sp->se_next;
		free_session(sp);
	}
	sessions = 0;
	if (start_session_db())
		return (state_func_t) single_user;

	/*
	 * Allocate a session entry for each active port.
	 * Note that sp starts at 0.
	 */
	while ((typ = getttyent()) != NULL)
		if ((snext = new_session(sp, ++session_index, typ)) != NULL)
			sp = snext;

	endttyent();

	return (state_func_t) multi_user;
}

/*
 * Start a window system running.
 */
void
start_window_system(session_t *sp)
{
	pid_t pid;
	sigset_t mask;
	char term[64], *env[2];

	if ((pid = fork()) == -1) {
		emergency("can't fork for window system on port %s: %m",
			sp->se_device);
		/* hope that getty fails and we can try again */
		return;
	}

	if (pid)
		return;

	sigemptyset(&mask);
	sigprocmask(SIG_SETMASK, &mask, (sigset_t *) 0);

	if (setsid() < 0)
		emergency("setsid failed (window) %m");

#ifdef LOGIN_CAP
	setprocresources(RESOURCE_WINDOW);
#endif
	if (sp->se_type) {
		/* Don't use malloc after fork */
		strcpy(term, "TERM=");
		strncat(term, sp->se_type, sizeof(term) - 6);
		env[0] = term;
		env[1] = 0;
	}
	else
		env[0] = 0;
	execve(sp->se_window_argv[0], sp->se_window_argv, env);
	stall("can't exec window system '%s' for port %s: %m",
		sp->se_window_argv[0], sp->se_device);
	_exit(1);
}

/*
 * Start a login session running.
 */
pid_t
start_getty(session_t *sp)
{
	pid_t pid;
	sigset_t mask;
	time_t current_time = time((time_t *) 0);
	int too_quick = 0;
	char term[64], *env[2];

	if (current_time >= sp->se_started &&
	    current_time - sp->se_started < GETTY_SPACING) {
		if (++sp->se_nspace > GETTY_NSPACE) {
			sp->se_nspace = 0;
			too_quick = 1;
		}
	} else
		sp->se_nspace = 0;

	/*
	 * fork(), not vfork() -- we can't afford to block.
	 */
	if ((pid = fork()) == -1) {
		emergency("can't fork for getty on port %s: %m", sp->se_device);
		return -1;
	}

	if (pid)
		return pid;

	if (too_quick) {
		warning("getty repeating too quickly on port %s, sleeping %d secs",
			sp->se_device, GETTY_SLEEP);
		sleep((unsigned) GETTY_SLEEP);
	}

	if (sp->se_window) {
		start_window_system(sp);
		sleep(WINDOW_WAIT);
	}

	sigemptyset(&mask);
	sigprocmask(SIG_SETMASK, &mask, (sigset_t *) 0);

#ifdef LOGIN_CAP
	setprocresources(RESOURCE_GETTY);
#endif
	if (sp->se_type) {
		/* Don't use malloc after fork */
		strcpy(term, "TERM=");
		strncat(term, sp->se_type, sizeof(term) - 6);
		env[0] = term;
		env[1] = 0;
	}
	else
		env[0] = 0;
	execve(sp->se_getty_argv[0], sp->se_getty_argv, env);
	stall("can't exec getty '%s' for port %s: %m",
		sp->se_getty_argv[0], sp->se_device);
	_exit(1);
}

/*
 * Collect exit status for a child.
 * If an exiting login, start a new login running.
 */
void
collect_child(pid_t pid)
{
	session_t *sp, *sprev, *snext;

	if (! sessions)
		return;

	if (! (sp = find_session(pid)))
		return;

	clear_session_logs(sp);
	del_session(sp);
	sp->se_process = 0;

	if (sp->se_flags & SE_SHUTDOWN) {
		if ((sprev = sp->se_prev) != NULL)
			sprev->se_next = sp->se_next;
		else
			sessions = sp->se_next;
		if ((snext = sp->se_next) != NULL)
			snext->se_prev = sp->se_prev;
		free_session(sp);
		return;
	}

	if ((pid = start_getty(sp)) == -1) {
		/* serious trouble */
		requested_transition = clean_ttys;
		return;
	}

	sp->se_process = pid;
	sp->se_started = time((time_t *) 0);
	add_session(sp);
}

/*
 * Catch a signal and request a state transition.
 */
void
transition_handler(int sig)
{

	switch (sig) {
	case SIGHUP:
		requested_transition = clean_ttys;
		break;
	case SIGUSR2:
		howto = RB_POWEROFF;
	case SIGUSR1:
		howto |= RB_HALT;
	case SIGINT:
		Reboot = TRUE;
	case SIGTERM:
		requested_transition = death;
		break;
	case SIGTSTP:
		requested_transition = catatonia;
		break;
	default:
		requested_transition = 0;
		break;
	}
}

/*
 * Take the system multiuser.
 */
state_func_t
multi_user(void)
{
	pid_t pid;
	session_t *sp;

	requested_transition = 0;

	/*
	 * If the administrator has not set the security level to -1
	 * to indicate that the kernel should not run multiuser in secure
	 * mode, and the run script has not set a higher level of security
	 * than level 1, then put the kernel into secure mode.
	 */
	if (getsecuritylevel() == 0)
		setsecuritylevel(1);

	for (sp = sessions; sp; sp = sp->se_next) {
		if (sp->se_process)
			continue;
		if ((pid = start_getty(sp)) == -1) {
			/* serious trouble */
			requested_transition = clean_ttys;
			break;
		}
		sp->se_process = pid;
		sp->se_started = time((time_t *) 0);
		add_session(sp);
	}

	while (!requested_transition)
		if ((pid = waitpid(-1, (int *) 0, 0)) != -1)
			collect_child(pid);

	return (state_func_t) requested_transition;
}

/*
 * This is an (n*2)+(n^2) algorithm.  We hope it isn't run often...
 */
state_func_t
clean_ttys(void)
{
	session_t *sp, *sprev;
	struct ttyent *typ;
	int session_index = 0;
	int devlen;
	char *old_getty, *old_window, *old_type;

	/* 
	 * mark all sessions for death, (!SE_PRESENT) 
	 * as we find or create new ones they'll be marked as keepers,
	 * we'll later nuke all the ones not found in /etc/ttys
	 */
	for (sp = sessions; sp != NULL; sp = sp->se_next)
		sp->se_flags &= ~SE_PRESENT;

	devlen = sizeof(_PATH_DEV) - 1;
	while ((typ = getttyent()) != NULL) {
		++session_index;

		for (sprev = 0, sp = sessions; sp; sprev = sp, sp = sp->se_next)
			if (strcmp(typ->ty_name, sp->se_device + devlen) == 0)
				break;

		if (sp) {
			/* we want this one to live */
			sp->se_flags |= SE_PRESENT;
			if (sp->se_index != session_index) {
				warning("port %s changed utmp index from %d to %d",
				       sp->se_device, sp->se_index,
				       session_index);
				sp->se_index = session_index;
			}
			if ((typ->ty_status & TTY_ON) == 0 ||
			    typ->ty_getty == 0) {
				sp->se_flags |= SE_SHUTDOWN;
				kill(sp->se_process, SIGHUP);
				continue;
			}
			sp->se_flags &= ~SE_SHUTDOWN;
			old_getty = sp->se_getty ? strdup(sp->se_getty) : 0;
			old_window = sp->se_window ? strdup(sp->se_window) : 0;
			old_type = sp->se_type ? strdup(sp->se_type) : 0;
			if (setupargv(sp, typ) == 0) {
				warning("can't parse getty for port %s",
					sp->se_device);
				sp->se_flags |= SE_SHUTDOWN;
				kill(sp->se_process, SIGHUP);
			}
			else if (   !old_getty
				 || (!old_type && sp->se_type)
				 || (old_type && !sp->se_type)
				 || (!old_window && sp->se_window)
				 || (old_window && !sp->se_window)
				 || (strcmp(old_getty, sp->se_getty) != 0)
				 || (old_window && strcmp(old_window, sp->se_window) != 0)
				 || (old_type && strcmp(old_type, sp->se_type) != 0)
				) {
				/* Don't set SE_SHUTDOWN here */
				sp->se_nspace = 0;
				sp->se_started = 0;
				kill(sp->se_process, SIGHUP);
			}
			if (old_getty)
				free(old_getty);
			if (old_window)
				free(old_window);
			if (old_type)
				free(old_type);
			continue;
		}

		new_session(sprev, session_index, typ);
	}

	endttyent();

	/*
	 * sweep through and kill all deleted sessions
	 * ones who's /etc/ttys line was deleted (SE_PRESENT unset)
	 */
	for (sp = sessions; sp != NULL; sp = sp->se_next) {
		if ((sp->se_flags & SE_PRESENT) == 0) {
			sp->se_flags |= SE_SHUTDOWN;
			kill(sp->se_process, SIGHUP);
		}
	}

	return (state_func_t) multi_user;
}

/*
 * Block further logins.
 */
state_func_t
catatonia(void)
{
	session_t *sp;

	for (sp = sessions; sp; sp = sp->se_next)
		sp->se_flags |= SE_SHUTDOWN;

	return (state_func_t) multi_user;
}

/*
 * Note SIGALRM.
 */
void
alrm_handler(int sig)
{
	(void)sig;
	clang = 1;
}

/*
 * Bring the system down to single user.
 */
state_func_t
death(void)
{
	session_t *sp;
	int i;
	pid_t pid;
	static const int death_sigs[2] = { SIGTERM, SIGKILL };

	/* NB: should send a message to the session logger to avoid blocking. */
	logwtmp("~", "shutdown", "");

	for (sp = sessions; sp; sp = sp->se_next) {
		sp->se_flags |= SE_SHUTDOWN;
		kill(sp->se_process, SIGHUP);
	}

	/* Try to run the rc.shutdown script within a period of time */
	(void) runshutdown();
    
	for (i = 0; i < 2; ++i) {
		if (kill(-1, death_sigs[i]) == -1 && errno == ESRCH)
			return (state_func_t) single_user;

		clang = 0;
		alarm(DEATH_WATCH);
		do
			if ((pid = waitpid(-1, (int *)0, 0)) != -1)
				collect_child(pid);
		while (clang == 0 && errno != ECHILD);

		if (errno == ECHILD)
			return (state_func_t) single_user;
	}

	warning("some processes would not die; ps axl advised");

	return (state_func_t) single_user;
}

/*
 * Run the system shutdown script.
 *
 * Exit codes:      XXX I should document more
 * -2       shutdown script terminated abnormally
 * -1       fatal error - can't run script
 * 0        good.
 * >0       some error (exit code)
 */
int
runshutdown(void)
{
	pid_t pid, wpid;
	int status;
	int shutdowntimeout;
	size_t len;
	char *argv[4];
	struct sigaction sa;
	struct stat sb;

	/*
	 * rc.shutdown is optional, so to prevent any unnecessary
	 * complaints from the shell we simply don't run it if the
	 * file does not exist. If the stat() here fails for other
	 * reasons, we'll let the shell complain.
	 */
	if (stat(_PATH_RUNDOWN, &sb) == -1 && errno == ENOENT)
		return 0;

	if ((pid = fork()) == 0) {
		int	fd;

		/* Assume that init already grab console as ctty before */

		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sa.sa_handler = SIG_IGN;
		(void) sigaction(SIGTSTP, &sa, (struct sigaction *)0);
		(void) sigaction(SIGHUP, &sa, (struct sigaction *)0);

		if ((fd = open(_PATH_CONSOLE, O_RDWR)) == -1)
		    warning("can't open %s: %m", _PATH_CONSOLE);
		else {
		    (void) dup2(fd, 0);
		    (void) dup2(fd, 1);
		    (void) dup2(fd, 2);
		    if (fd > 2)
			close(fd);
		}

		/*
		 * Run the shutdown script.
		 */

		char _sh[]	= "sh";
		char _reboot[]	= "reboot";
		char _single[]	= "single";
		char _path_rundown[] = _PATH_RUNDOWN;

		argv[0] = _sh;
		argv[1] = _path_rundown;
		argv[2] = Reboot ? _reboot : _single;
		argv[3] = 0;

		sigprocmask(SIG_SETMASK, &sa.sa_mask, (sigset_t *) 0);

#ifdef LOGIN_CAP
		setprocresources(RESOURCE_RC);
#endif
		execv(_PATH_BSHELL, argv);
		warning("can't exec %s for %s: %m", _PATH_BSHELL, _PATH_RUNDOWN);
		_exit(1);	/* force single user mode */
	}

	if (pid == -1) {
		emergency("can't fork for %s on %s: %m",
			_PATH_BSHELL, _PATH_RUNDOWN);
		while (waitpid(-1, (int *) 0, WNOHANG) > 0)
			continue;
		sleep(STALL_TIMEOUT);
		return -1;
	}

	len = sizeof(shutdowntimeout);
	if (sysctlbyname("kern.init_shutdown_timeout",
			 &shutdowntimeout,
			 &len, NULL, 0) == -1 || shutdowntimeout < 2)
	    shutdowntimeout = DEATH_SCRIPT;
	alarm(shutdowntimeout);
	clang = 0;
	/*
	 * Copied from single_user().  This is a bit paranoid.
	 * Use the same ALRM handler.
	 */
	do {
		if ((wpid = waitpid(-1, &status, WUNTRACED)) != -1)
			collect_child(wpid);
		if (clang == 1) {
			/* we were waiting for the sub-shell */
			kill(wpid, SIGTERM);
			warning("timeout expired for %s on %s: %m; going to single user mode",
				_PATH_BSHELL, _PATH_RUNDOWN);
			return -1;
		}
		if (wpid == -1) {
			if (errno == EINTR)
				continue;
			warning("wait for %s on %s failed: %m; going to single user mode",
				_PATH_BSHELL, _PATH_RUNDOWN);
			return -1;
		}
		if (wpid == pid && WIFSTOPPED(status)) {
			warning("init: %s on %s stopped, restarting\n",
				_PATH_BSHELL, _PATH_RUNDOWN);
			kill(pid, SIGCONT);
			wpid = -1;
		}
	} while (wpid != pid && !clang);

	/* Turn off the alarm */
	alarm(0);

	if (WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM &&
	    requested_transition == catatonia) {
		/*
		 * /etc/rc.shutdown executed /sbin/reboot;
		 * wait for the end quietly
		 */
		sigset_t s;

		sigfillset(&s);
		for (;;)
			sigsuspend(&s);
	}

	if (!WIFEXITED(status)) {
		warning("%s on %s terminated abnormally, going to single user mode",
			_PATH_BSHELL, _PATH_RUNDOWN);
		return -2;
	}

	if ((status = WEXITSTATUS(status)) != 0)
		warning("%s returned status %d", _PATH_RUNDOWN, status);

	return status;
}

static char *
strk(char *p)
{
    static char *t;
    char *q;
    int c;

    if (p)
	t = p;
    if (!t)
	return 0;

    c = *t;
    while (c == ' ' || c == '\t' )
	c = *++t;
    if (!c) {
	t = 0;
	return 0;
    }
    q = t;
    if (c == '\'') {
	c = *++t;
	q = t;
	while (c && c != '\'')
	    c = *++t;
	if (!c)  /* unterminated string */
	    q = t = 0;
	else
	    *t++ = 0;
    } else {
	while (c && c != ' ' && c != '\t' )
	    c = *++t;
	*t++ = 0;
	if (!c)
	    t = 0;
    }
    return q;
}

#ifdef LOGIN_CAP
void
setprocresources(const char *cname)
{
	login_cap_t *lc;
	if ((lc = login_getclassbyname(cname, NULL)) != NULL) {
		setusercontext(lc, (struct passwd*)NULL, 0, LOGIN_SETPRIORITY|LOGIN_SETRESOURCES);
		login_close(lc);
	}
}
#endif
