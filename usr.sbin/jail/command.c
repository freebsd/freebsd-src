/*-
 * Copyright (c) 2010 James Gritton
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/event.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <kvm.h>
#include <login_cap.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "jailp.h"

#define COMSTRING_DUMMY		((struct cfstring *)1)
#define DEFAULT_STOP_TIMEOUT	10
#define PHASH_SIZE		256

LIST_HEAD(phhead, phash);

struct phash {
	LIST_ENTRY(phash)	le;
	struct cfjail		*j;
	pid_t			pid;
};

int paralimit = -1;

extern char **environ;

static int get_user_info(struct cfjail *j, const char *username,
    const struct passwd **pwdp, login_cap_t **lcapp);
static void add_proc(struct cfjail *j, pid_t pid);
static void clear_procs(struct cfjail *j);
static struct cfjail *find_proc(pid_t pid);
static int term_procs(struct cfjail *j);
static int check_path(struct cfjail *j, const char *pname, const char *path,
    int isfile, const char *umount_type);

static struct cfjails sleeping = TAILQ_HEAD_INITIALIZER(sleeping);
static struct cfjails runnable = TAILQ_HEAD_INITIALIZER(runnable);
static struct phhead phash[PHASH_SIZE];
static int kq;

/*
 * Run a command associated with a jail, possibly inside the jail.
 */
int
run_command(struct cfjail *j, enum intparam comparam)
{
	const struct passwd *pwd;
	struct cfstring *comstring, *s;
	login_cap_t *lcap;
	char **argv;
	char *cs, *addr, *comcs, *devpath;
	const char *jidstr, *conslog, *path, *ruleset, *term, *username;
	size_t comlen;
	pid_t pid;
	int argc, bg, clean, consfd, down, fib, i, injail, sjuser, timeout;

	static char *cleanenv;

	if (comparam) {
		switch (comparam) {
		case IP_MOUNT_DEVFS:
			if (!bool_param(j->intparams[IP_MOUNT_DEVFS]))
				return 0;
			/* FALLTHROUGH */
		case IP_STOP_TIMEOUT:
			j->comstring = COMSTRING_DUMMY;
			break;
		default:
			if (j->intparams[comparam] == NULL)
				return 0;
			j->comstring =
			    STAILQ_FIRST(&j->intparams[comparam]->val);
		}
		j->comparam = comparam;
	} else
		comparam = j->comparam;
 next_comstring:
	comstring = j->comstring;
	if (comstring == NULL)
		return 0;
	if (paralimit == 0) {
		requeue(j, &runnable);
		return 1;
	}
	j->comstring =
	    comstring == COMSTRING_DUMMY ? NULL : STAILQ_NEXT(comstring, tq);
	if (comstring != COMSTRING_DUMMY && comstring->len == 0)
		goto next_comstring;
	/*
	 * Collect exec arguments.  Internal commands for network and
	 * mounting build their own argument lists.
	 */
	bg = j->flags & JF_FAILED;
	down = j->flags & (JF_STOP | JF_FAILED);
	switch (comparam) {
	case IP_STOP_TIMEOUT:
		/* This isn't really a command */
		return term_procs(j);

	case IP__IP4_IFADDR:
		argv = alloca(8 * sizeof(char *));
		*(const char **)&argv[0] = _PATH_IFCONFIG;
		if ((cs = strchr(comstring->s, '|'))) {
			argv[1] = alloca(cs - comstring->s + 1);
			strlcpy(argv[1], comstring->s, cs - comstring->s + 1);
			addr = cs + 1;
		} else {
			*(const char **)&argv[1] =
			    string_param(j->intparams[IP_INTERFACE]);
			addr = comstring->s;
		}
		*(const char **)&argv[2] = "inet";
		if (!(cs = strchr(addr, '/'))) {
			argv[3] = addr;
			*(const char **)&argv[4] = "netmask";
			*(const char **)&argv[5] = "255.255.255.255";
			argc = 6;
		} else if (strchr(cs + 1, '.')) {
			argv[3] = alloca(cs - addr + 1);
			strlcpy(argv[3], addr, cs - addr + 1);
			*(const char **)&argv[4] = "netmask";
			*(const char **)&argv[5] = cs + 1;
			argc = 6;
		} else {
			argv[3] = addr;
			argc = 4;
		}
		*(const char **)&argv[argc] = down ? "-alias" : "alias";
		argv[argc + 1] = NULL;
		j->flags |= JF_IFUP;
		break;

#ifdef INET6
	case IP__IP6_IFADDR:
		argv = alloca(8 * sizeof(char *));
		*(const char **)&argv[0] = _PATH_IFCONFIG;
		if ((cs = strchr(comstring->s, '|'))) {
			argv[1] = alloca(cs - comstring->s + 1);
			strlcpy(argv[1], comstring->s, cs - comstring->s + 1);
			addr = cs + 1;
		} else {
			*(const char **)&argv[1] =
			    string_param(j->intparams[IP_INTERFACE]);
			addr = comstring->s;
		}
		*(const char **)&argv[2] = "inet6";
		argv[3] = addr;
		if (!(cs = strchr(addr, '/'))) {
			*(const char **)&argv[4] = "prefixlen";
			*(const char **)&argv[5] = "128";
			argc = 6;
		} else
			argc = 4;
		*(const char **)&argv[argc] = down ? "-alias" : "alias";
		argv[argc + 1] = NULL;
		j->flags |= JF_IFUP;
		break;	
#endif

	case IP_VNET_INTERFACE:
		argv = alloca(5 * sizeof(char *));
		*(const char **)&argv[0] = _PATH_IFCONFIG;
		argv[1] = comstring->s;
		*(const char **)&argv[2] = down ? "-vnet" : "vnet";
		jidstr = string_param(j->intparams[KP_JID]);
		*(const char **)&argv[3] =
			jidstr ? jidstr : string_param(j->intparams[KP_NAME]);
		argv[4] = NULL;
		j->flags |= JF_IFUP;
		break;

	case IP_MOUNT:
	case IP__MOUNT_FROM_FSTAB:
		argv = alloca(8 * sizeof(char *));
		comcs = alloca(comstring->len + 1);
		strcpy(comcs, comstring->s);
		argc = 0;
		for (cs = strtok(comcs, " \t\f\v\r\n"); cs && argc < 4;
		     cs = strtok(NULL, " \t\f\v\r\n"))
			argv[argc++] = cs;
		if (argc == 0)
			goto next_comstring;
		if (argc < 3) {
			jail_warnx(j, "%s: %s: missing information",
			    j->intparams[comparam]->name, comstring->s);
			failed(j);
			return -1;
		}
		if (check_path(j, j->intparams[comparam]->name, argv[1], 0,
		    down ? argv[2] : NULL) < 0) {
			failed(j);
			return -1;
		}
		if (down) {
			argv[4] = NULL;
			argv[3] = argv[1];
			*(const char **)&argv[0] = "/sbin/umount";
		} else {
			if (argc == 4) {
				argv[7] = NULL;
				argv[6] = argv[1];
				argv[5] = argv[0];
				argv[4] = argv[3];
				*(const char **)&argv[3] = "-o";
			} else {
				argv[5] = NULL;
				argv[4] = argv[1];
				argv[3] = argv[0];
			}
			*(const char **)&argv[0] = _PATH_MOUNT;
		}
		*(const char **)&argv[1] = "-t";
		j->flags |= JF_MOUNTED;
		break;

	case IP_MOUNT_DEVFS:
		path = string_param(j->intparams[KP_PATH]);
		if (path == NULL) {
			jail_warnx(j, "mount.devfs: no path");
			failed(j);
			return -1;
		}
		devpath = alloca(strlen(path) + 5);
		sprintf(devpath, "%s/dev", path);
		if (check_path(j, "mount.devfs", devpath, 0,
		    down ? "devfs" : NULL) < 0) {
			failed(j);
			return -1;
		}
		if (down) {
			argv = alloca(3 * sizeof(char *));
			*(const char **)&argv[0] = "/sbin/umount";
			argv[1] = devpath;
			argv[2] = NULL;
		} else {
			argv = alloca(4 * sizeof(char *));
			*(const char **)&argv[0] = _PATH_BSHELL;
			*(const char **)&argv[1] = "-c";
			ruleset = string_param(j->intparams
			    [IP_MOUNT_DEVFS_RULESET]);
			argv[2] = alloca(strlen(path) +
			    (ruleset ? strlen(ruleset) + 1 : 0) + 56);
			sprintf(argv[2], ". /etc/rc.subr; load_rc_config .; "
			    "devfs_mount_jail %s/dev%s%s", path,
			    ruleset ? " " : "", ruleset ? ruleset : "");
			argv[3] = NULL;
		}
		j->flags |= JF_MOUNTED;
		break;

	case IP_COMMAND:
		if (j->name != NULL)
			goto default_command;
		argc = 0;
		STAILQ_FOREACH(s, &j->intparams[IP_COMMAND]->val, tq)
			argc++;
		argv = alloca((argc + 1) * sizeof(char *));
		argc = 0;
		STAILQ_FOREACH(s, &j->intparams[IP_COMMAND]->val, tq)
			argv[argc++] = s->s;
		argv[argc] = NULL;
		j->comstring = NULL;
		break;

	default:
	default_command:
		if ((cs = strpbrk(comstring->s, "!\"$&'()*;<>?[\\]`{|}~")) &&
		    !(cs[0] == '&' && cs[1] == '\0')) {
			argv = alloca(4 * sizeof(char *));
			*(const char **)&argv[0] = _PATH_BSHELL;
			*(const char **)&argv[1] = "-c";
			argv[2] = comstring->s;
			argv[3] = NULL;
		} else {
			if (cs) {
				*cs = 0;
				bg = 1;
			}
			comcs = alloca(comstring->len + 1);
			strcpy(comcs, comstring->s);	
			argc = 0;
			for (cs = strtok(comcs, " \t\f\v\r\n"); cs;
			     cs = strtok(NULL, " \t\f\v\r\n"))
				argc++;
			argv = alloca((argc + 1) * sizeof(char *));
			strcpy(comcs, comstring->s);	
			argc = 0;
			for (cs = strtok(comcs, " \t\f\v\r\n"); cs;
			     cs = strtok(NULL, " \t\f\v\r\n"))
				argv[argc++] = cs;
			argv[argc] = NULL;
		}
	}

	if (argv[0] == NULL)
		goto next_comstring;
	if (int_param(j->intparams[IP_EXEC_TIMEOUT], &timeout) &&
	    timeout != 0) {
		clock_gettime(CLOCK_REALTIME, &j->timeout);
		j->timeout.tv_sec += timeout;
	} else
		j->timeout.tv_sec = 0;

	injail = comparam == IP_EXEC_START || comparam == IP_COMMAND ||
	    comparam == IP_EXEC_STOP;
	clean = bool_param(j->intparams[IP_EXEC_CLEAN]);
	username = string_param(j->intparams[injail
	    ? IP_EXEC_JAIL_USER : IP_EXEC_SYSTEM_USER]);
	sjuser = bool_param(j->intparams[IP_EXEC_SYSTEM_JAIL_USER]);

	consfd = 0;
	if (injail &&
	    (conslog = string_param(j->intparams[IP_EXEC_CONSOLELOG]))) {
		if (check_path(j, "exec.consolelog", conslog, 1, NULL) < 0) {
			failed(j);
			return -1;
		}
		consfd =
		    open(conslog, O_WRONLY | O_CREAT | O_APPEND, DEFFILEMODE);
		if (consfd < 0) {
			jail_warnx(j, "open %s: %s", conslog, strerror(errno));
			failed(j);
			return -1;
		}
	}

	comlen = 0;
	for (i = 0; argv[i]; i++)
		comlen += strlen(argv[i]) + 1;
	j->comline = cs = emalloc(comlen);
	for (i = 0; argv[i]; i++) {
		strcpy(cs, argv[i]);
		if (argv[i + 1]) {
			cs += strlen(argv[i]) + 1;
			cs[-1] = ' ';
		}
	}
	if (verbose > 0)
		jail_note(j, "run command%s%s%s: %s\n",
		    injail ? " in jail" : "", username ? " as " : "",
		    username ? username : "", j->comline);

	pid = fork();
	if (pid < 0)
		err(1, "fork");
	if (pid > 0) {
		if (bg) {
			free(j->comline);
			j->comline = NULL;
			requeue(j, &ready);
		} else {
			paralimit--;
			add_proc(j, pid);
		}
		return 1;
	}
	if (bg)
		setsid();

	pwd = NULL;
	lcap = NULL;
	if ((clean || username) && injail && sjuser &&
	    get_user_info(j, username, &pwd, &lcap) < 0)
		exit(1);
	if (injail) {
		/* jail_attach won't chdir along with its chroot. */
		path = string_param(j->intparams[KP_PATH]);
		if (path && chdir(path) < 0) {
			jail_warnx(j, "chdir %s: %s", path, strerror(errno));
			exit(1);
		}
		if (int_param(j->intparams[IP_EXEC_FIB], &fib) &&
		    setfib(fib) < 0) {
			jail_warnx(j, "setfib: %s", strerror(errno));
			exit(1);
		}
		if (jail_attach(j->jid) < 0) {
			jail_warnx(j, "jail_attach: %s", strerror(errno));
			exit(1);
		}
	}
	if (clean || username) {
		if (!(injail && sjuser) &&
		    get_user_info(j, username, &pwd, &lcap) < 0)
			exit(1);
		if (clean) {
			term = getenv("TERM");
			environ = &cleanenv;
			setenv("PATH", "/bin:/usr/bin", 0);
			setenv("TERM", term, 1);
		}
		if (setusercontext(lcap, pwd, pwd->pw_uid, username
		    ? LOGIN_SETALL & ~LOGIN_SETGROUP & ~LOGIN_SETLOGIN
		    : LOGIN_SETPATH | LOGIN_SETENV) < 0) {
			jail_warnx(j, "setusercontext %s: %s", pwd->pw_name,
			    strerror(errno));
			exit(1);
		}
		login_close(lcap);
		setenv("USER", pwd->pw_name, 1);
		setenv("HOME", pwd->pw_dir, 1);
		setenv("SHELL",
		    *pwd->pw_shell ? pwd->pw_shell : _PATH_BSHELL, 1);
		if (clean && chdir(pwd->pw_dir) < 0) {
			jail_warnx(j, "chdir %s: %s",
			    pwd->pw_dir, strerror(errno));
			exit(1);
		}
		endpwent();
	}

	if (consfd != 0 && (dup2(consfd, 1) < 0 || dup2(consfd, 2) < 0)) {
		jail_warnx(j, "exec.consolelog: %s", strerror(errno));
		exit(1);
	}
	closefrom(3);
	execvp(argv[0], argv);
	jail_warnx(j, "exec %s: %s", argv[0], strerror(errno));
	exit(1);
}

/*
 * Check command exit status
 */
int
finish_command(struct cfjail *j)
{
	int error;

	if (!(j->flags & JF_SLEEPQ))
		return 0;
	j->flags &= ~JF_SLEEPQ;
	if (j->comparam != IP_STOP_TIMEOUT) {
		paralimit++;
		if (!TAILQ_EMPTY(&runnable))
			requeue(TAILQ_FIRST(&runnable), &ready);
	}
	error = 0;
	if (j->flags & JF_TIMEOUT) {
		j->flags &= ~JF_TIMEOUT;
		if (j->comparam != IP_STOP_TIMEOUT) {
			jail_warnx(j, "%s: timed out", j->comline);
			failed(j);
			error = -1;
		} else if (verbose > 0)
			jail_note(j, "timed out\n");
	} else if (j->pstatus != 0) {
		if (WIFSIGNALED(j->pstatus))
			jail_warnx(j, "%s: exited on signal %d",
			    j->comline, WTERMSIG(j->pstatus));
		else
			jail_warnx(j, "%s: failed", j->comline);
		j->pstatus = 0;
		failed(j);
		error = -1;
	}
	free(j->comline);
	j->comline = NULL;
	return error;
}

/*
 * Check for finished processed or timeouts.
 */
struct cfjail *
next_proc(int nonblock)
{
	struct kevent ke;
	struct timespec ts;
	struct timespec *tsp;
	struct cfjail *j;

	if (!TAILQ_EMPTY(&sleeping)) {
	again:
		tsp = NULL;
		if ((j = TAILQ_FIRST(&sleeping)) && j->timeout.tv_sec) {
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_sec = j->timeout.tv_sec - ts.tv_sec;
			ts.tv_nsec = j->timeout.tv_nsec - ts.tv_nsec;
			if (ts.tv_nsec < 0) {
				ts.tv_sec--;
				ts.tv_nsec += 1000000000;
			}
			if (ts.tv_sec < 0 ||
			    (ts.tv_sec == 0 && ts.tv_nsec == 0)) {
				j->flags |= JF_TIMEOUT;
				clear_procs(j);
				return j;
			}
			tsp = &ts;
		}
		if (nonblock) {
			ts.tv_sec = 0;
			ts.tv_nsec = 0;
			tsp = &ts;
		}
		switch (kevent(kq, NULL, 0, &ke, 1, tsp)) {
		case -1:
			if (errno != EINTR)
				err(1, "kevent");
			goto again;
		case 0:
			if (!nonblock) {
				j = TAILQ_FIRST(&sleeping);
				j->flags |= JF_TIMEOUT;
				clear_procs(j);
				return j;
			}
			break;
		case 1:
			(void)waitpid(ke.ident, NULL, WNOHANG);
			if ((j = find_proc(ke.ident))) {
				j->pstatus = ke.data;
				return j;
			}
			goto again;
		}
	}
	return NULL;
}

/*
 * Add a process to the hash, tied to a jail.
 */
static void
add_proc(struct cfjail *j, pid_t pid)
{
	struct kevent ke;
	struct cfjail *tj;
	struct phash *ph;

	if (!kq && (kq = kqueue()) < 0)
		err(1, "kqueue");
	EV_SET(&ke, pid, EVFILT_PROC, EV_ADD, NOTE_EXIT, 0, NULL);
	if (kevent(kq, &ke, 1, NULL, 0, NULL) < 0)
		err(1, "kevent");
	ph = emalloc(sizeof(struct phash));
	ph->j = j;
	ph->pid = pid;
	LIST_INSERT_HEAD(&phash[pid % PHASH_SIZE], ph, le);
	j->nprocs++;
	j->flags |= JF_SLEEPQ;
	if (j->timeout.tv_sec == 0)
		requeue(j, &sleeping);
	else {
		/* File the jail in the sleep queue acording to its timeout. */
		TAILQ_REMOVE(j->queue, j, tq);
		TAILQ_FOREACH(tj, &sleeping, tq) {
			if (!tj->timeout.tv_sec ||
			    j->timeout.tv_sec < tj->timeout.tv_sec ||
			    (j->timeout.tv_sec == tj->timeout.tv_sec &&
			    j->timeout.tv_nsec <= tj->timeout.tv_nsec)) {
				TAILQ_INSERT_BEFORE(tj, j, tq);
				break;
			}
		}
		if (tj == NULL)
			TAILQ_INSERT_TAIL(&sleeping, j, tq);
		j->queue = &sleeping;
	}
}

/*
 * Remove any processes from the hash that correspond to a jail.
 */
static void
clear_procs(struct cfjail *j)
{
	struct kevent ke;
	struct phash *ph, *tph;
	int i;

	j->nprocs = 0;
	for (i = 0; i < PHASH_SIZE; i++)
		LIST_FOREACH_SAFE(ph, &phash[i], le, tph)
			if (ph->j == j) {
				EV_SET(&ke, ph->pid, EVFILT_PROC, EV_DELETE,
				    NOTE_EXIT, 0, NULL);
				(void)kevent(kq, &ke, 1, NULL, 0, NULL);
				LIST_REMOVE(ph, le);
				free(ph);
			}
}

/*
 * Find the jail that corresponds to an exited process.
 */
static struct cfjail *
find_proc(pid_t pid)
{
	struct cfjail *j;
	struct phash *ph;

	LIST_FOREACH(ph, &phash[pid % PHASH_SIZE], le)
		if (ph->pid == pid) {
			j = ph->j;
			LIST_REMOVE(ph, le);
			free(ph);
			return --j->nprocs ? NULL : j;
		}
	return NULL;
}

/*
 * Send SIGTERM to all processes in a jail and wait for them to die.
 */
static int
term_procs(struct cfjail *j)
{
	struct kinfo_proc *ki;
	int i, noted, pcnt, timeout;

	static kvm_t *kd;

	if (!int_param(j->intparams[IP_STOP_TIMEOUT], &timeout))
		timeout = DEFAULT_STOP_TIMEOUT;
	else if (timeout == 0)
		return 0;

	if (kd == NULL) {
		kd = kvm_open(NULL, NULL, NULL, O_RDONLY, "jail");
		if (kd == NULL)
			exit(1);
	}

	ki = kvm_getprocs(kd, KERN_PROC_PROC, 0, &pcnt);
	if (ki == NULL)
		exit(1);
	noted = 0;
	for (i = 0; i < pcnt; i++)
		if (ki[i].ki_jid == j->jid &&
		    kill(ki[i].ki_pid, SIGTERM) == 0) {
			add_proc(j, ki[i].ki_pid);
			if (verbose > 0) {
				if (!noted) {
					noted = 1;
					jail_note(j, "sent SIGTERM to:");
				}
				printf(" %d", ki[i].ki_pid);
			}
		}
	if (noted)
		printf("\n");
	if (j->nprocs > 0) {
		clock_gettime(CLOCK_REALTIME, &j->timeout);
		j->timeout.tv_sec += timeout;
		return 1;
	}
	return 0;
}

/*
 * Look up a user in the passwd and login.conf files.
 */
static int
get_user_info(struct cfjail *j, const char *username,
    const struct passwd **pwdp, login_cap_t **lcapp)
{
	const struct passwd *pwd;

	*pwdp = pwd = username ? getpwnam(username) : getpwuid(getuid());
	if (pwd == NULL) {
		if (errno)
			jail_warnx(j, "getpwnam%s%s: %s", username ? " " : "",
			    username ? username : "", strerror(errno));
		else if (username)
			jail_warnx(j, "%s: no such user", username);
		else
			jail_warnx(j, "unknown uid %d", getuid());
		return -1;
	}
	*lcapp = login_getpwclass(pwd);
	if (*lcapp == NULL) {
		jail_warnx(j, "getpwclass %s: %s", pwd->pw_name,
		    strerror(errno));
		return -1;
	}
	/* Set the groups while the group file is still available */
	if (initgroups(pwd->pw_name, pwd->pw_gid) < 0) {
		jail_warnx(j, "initgroups %s: %s", pwd->pw_name,
		    strerror(errno));
		return -1;
	}
	return 0;
}

/*
 * Make sure a mount or consolelog path is a valid absolute pathname
 * with no symlinks.
 */
static int
check_path(struct cfjail *j, const char *pname, const char *path, int isfile,
    const char *umount_type)
{
	struct stat st, mpst;
	struct statfs stfs;
	char *tpath, *p;
	const char *jailpath;
	size_t jplen;

	if (path[0] != '/') {
		jail_warnx(j, "%s: %s: not an absolute pathname",
		    pname, path);
		return -1;
	}
	/*
	 * Only check for symlinks in components below the jail's path,
	 * since that's where the security risk lies.
	 */
	jailpath = string_param(j->intparams[KP_PATH]);
	if (jailpath == NULL)
		jailpath = "";
	jplen = strlen(jailpath);
	if (!strncmp(path, jailpath, jplen) && path[jplen] == '/') {
		tpath = alloca(strlen(path) + 1);
		strcpy(tpath, path);
		for (p = tpath + jplen; p != NULL; ) {
			p = strchr(p + 1, '/');
			if (p)
				*p = '\0';
			if (lstat(tpath, &st) < 0) {
				if (errno == ENOENT && isfile && !p)
					break;
				jail_warnx(j, "%s: %s: %s", pname, tpath,
				    strerror(errno));
				return -1;
			}
			if (S_ISLNK(st.st_mode)) {
				jail_warnx(j, "%s: %s is a symbolic link",
				    pname, tpath);
				return -1;
			}
			if (p)
				*p = '/';
		}
	}
	if (umount_type != NULL) {
		if (stat(path, &st) < 0 || statfs(path, &stfs) < 0) {
			jail_warnx(j, "%s: %s: %s", pname, path,
			    strerror(errno));
			return -1;
		}
		if (stat(stfs.f_mntonname, &mpst) < 0) {
			jail_warnx(j, "%s: %s: %s", pname, stfs.f_mntonname,
			    strerror(errno));
			return -1;
		}
		if (st.st_ino != mpst.st_ino) {
			jail_warnx(j, "%s: %s: not a mount point",
			    pname, path);
			return -1;
		}
		if (strcmp(stfs.f_fstypename, umount_type)) {
			jail_warnx(j, "%s: %s: not a %s mount",
			    pname, path, umount_type);
			return -1;
		}
	}
	return 0;
}
