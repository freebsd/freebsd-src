/*
 * Copyright (C) 2004-2009  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2002  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: os.c,v 1.89.12.5 2009-03-02 03:03:54 marka Exp $ */

/*! \file */

#include <config.h>
#include <stdarg.h>

#include <sys/types.h>	/* dev_t FreeBSD 2.1 */
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>		/* Required for initgroups() on IRIX. */
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <syslog.h>
#ifdef HAVE_TZSET
#include <time.h>
#endif
#include <unistd.h>

#include <isc/buffer.h>
#include <isc/file.h>
#include <isc/print.h>
#include <isc/resource.h>
#include <isc/result.h>
#include <isc/strerror.h>
#include <isc/string.h>

#include <named/main.h>
#include <named/os.h>
#ifdef HAVE_LIBSCF
#include <named/ns_smf_globals.h>
#endif

static char *pidfile = NULL;
static int devnullfd = -1;

#ifndef ISC_FACILITY
#define ISC_FACILITY LOG_DAEMON
#endif

/*
 * If there's no <linux/capability.h>, we don't care about <sys/prctl.h>
 */
#ifndef HAVE_LINUX_CAPABILITY_H
#undef HAVE_SYS_PRCTL_H
#endif

/*
 * Linux defines:
 *	(T) HAVE_LINUXTHREADS
 *	(C) HAVE_SYS_CAPABILITY_H (or HAVE_LINUX_CAPABILITY_H)
 *	(P) HAVE_SYS_PRCTL_H
 * The possible cases are:
 *	none:	setuid() normally
 *	T:	no setuid()
 *	C:	setuid() normally, drop caps (keep CAP_SETUID)
 *	T+C:	no setuid(), drop caps (don't keep CAP_SETUID)
 *	T+C+P:	setuid() early, drop caps (keep CAP_SETUID)
 *	C+P:	setuid() normally, drop caps (keep CAP_SETUID)
 *	P:	not possible
 *	T+P:	not possible
 *
 * if (C)
 *	caps = BIND_SERVICE + CHROOT + SETGID
 *	if ((T && C && P) || !T)
 *		caps += SETUID
 *	endif
 *	capset(caps)
 * endif
 * if (T && C && P && -u)
 *	setuid()
 * else if (T && -u)
 *	fail
 * --> start threads
 * if (!T && -u)
 *	setuid()
 * if (C && (P || !-u))
 *	caps = BIND_SERVICE
 *	capset(caps)
 * endif
 *
 * It will be nice when Linux threads work properly with setuid().
 */

#ifdef HAVE_LINUXTHREADS
static pid_t mainpid = 0;
#endif

static struct passwd *runas_pw = NULL;
static isc_boolean_t done_setuid = ISC_FALSE;
static int dfd[2] = { -1, -1 };

#ifdef HAVE_LINUX_CAPABILITY_H

static isc_boolean_t non_root = ISC_FALSE;
static isc_boolean_t non_root_caps = ISC_FALSE;

#ifdef HAVE_SYS_CAPABILITY_H
#include <sys/capability.h>
#else
/*%
 * We define _LINUX_FS_H to prevent it from being included.  We don't need
 * anything from it, and the files it includes cause warnings with 2.2
 * kernels, and compilation failures (due to conflicts between <linux/string.h>
 * and <string.h>) on 2.3 kernels.
 */
#define _LINUX_FS_H
#include <linux/capability.h>
#include <syscall.h>
#ifndef SYS_capset
#ifndef __NR_capset
#include <asm/unistd.h> /* Slackware 4.0 needs this. */
#endif /* __NR_capset */
#define SYS_capset __NR_capset
#endif /* SYS_capset */
#endif /* HAVE_SYS_CAPABILITY_H */

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>		/* Required for prctl(). */

/*
 * If the value of PR_SET_KEEPCAPS is not in <sys/prctl.h>, define it
 * here.  This allows setuid() to work on systems running a new enough
 * kernel but with /usr/include/linux pointing to "standard" kernel
 * headers.
 */
#ifndef PR_SET_KEEPCAPS
#define PR_SET_KEEPCAPS 8
#endif

#endif /* HAVE_SYS_PRCTL_H */

#ifdef HAVE_LIBCAP
#define SETCAPS_FUNC "cap_set_proc "
#else
typedef unsigned int cap_t;
#define SETCAPS_FUNC "syscall(capset) "
#endif /* HAVE_LIBCAP */

static void
linux_setcaps(cap_t caps) {
#ifndef HAVE_LIBCAP
	struct __user_cap_header_struct caphead;
	struct __user_cap_data_struct cap;
#endif
	char strbuf[ISC_STRERRORSIZE];

	if ((getuid() != 0 && !non_root_caps) || non_root)
		return;
#ifndef HAVE_LIBCAP
	memset(&caphead, 0, sizeof(caphead));
	caphead.version = _LINUX_CAPABILITY_VERSION;
	caphead.pid = 0;
	memset(&cap, 0, sizeof(cap));
	cap.effective = caps;
	cap.permitted = caps;
	cap.inheritable = 0;
#endif
#ifdef HAVE_LIBCAP
	if (cap_set_proc(caps) < 0) {
#else
	if (syscall(SYS_capset, &caphead, &cap) < 0) {
#endif
		isc__strerror(errno, strbuf, sizeof(strbuf));
		ns_main_earlyfatal(SETCAPS_FUNC "failed: %s:"
				   " please ensure that the capset kernel"
				   " module is loaded.  see insmod(8)",
				   strbuf);
	}
}

#ifdef HAVE_LIBCAP
#define SET_CAP(flag) \
	do { \
		capval = (flag); \
		cap_flag_value_t curval; \
		err = cap_get_flag(curcaps, capval, CAP_PERMITTED, &curval); \
		if (err != -1 && curval) { \
			err = cap_set_flag(caps, CAP_EFFECTIVE, 1, &capval, CAP_SET); \
			if (err == -1) { \
				isc__strerror(errno, strbuf, sizeof(strbuf)); \
				ns_main_earlyfatal("cap_set_proc failed: %s", strbuf); \
			} \
			\
			err = cap_set_flag(caps, CAP_PERMITTED, 1, &capval, CAP_SET); \
			if (err == -1) { \
				isc__strerror(errno, strbuf, sizeof(strbuf)); \
				ns_main_earlyfatal("cap_set_proc failed: %s", strbuf); \
			} \
		} \
	} while (0)
#define INIT_CAP \
	do { \
		caps = cap_init(); \
		if (caps == NULL) { \
			isc__strerror(errno, strbuf, sizeof(strbuf)); \
			ns_main_earlyfatal("cap_init failed: %s", strbuf); \
		} \
		curcaps = cap_get_proc(); \
		if (curcaps == NULL) { \
			isc__strerror(errno, strbuf, sizeof(strbuf)); \
			ns_main_earlyfatal("cap_get_proc failed: %s", strbuf); \
		} \
	} while (0)
#define FREE_CAP \
	{ \
		cap_free(caps); \
		cap_free(curcaps); \
	} while (0)
#else
#define SET_CAP(flag) do { caps |= (1 << (flag)); } while (0)
#define INIT_CAP do { caps = 0; } while (0)
#endif /* HAVE_LIBCAP */

static void
linux_initialprivs(void) {
	cap_t caps;
#ifdef HAVE_LIBCAP
	cap_t curcaps;
	cap_value_t capval;
	char strbuf[ISC_STRERRORSIZE];
	int err;
#endif

	/*%
	 * We don't need most privileges, so we drop them right away.
	 * Later on linux_minprivs() will be called, which will drop our
	 * capabilities to the minimum needed to run the server.
	 */
	INIT_CAP;

	/*
	 * We need to be able to bind() to privileged ports, notably port 53!
	 */
	SET_CAP(CAP_NET_BIND_SERVICE);

	/*
	 * We need chroot() initially too.
	 */
	SET_CAP(CAP_SYS_CHROOT);

#if defined(HAVE_SYS_PRCTL_H) || !defined(HAVE_LINUXTHREADS)
	/*
	 * We can setuid() only if either the kernel supports keeping
	 * capabilities after setuid() (which we don't know until we've
	 * tried) or we're not using threads.  If either of these is
	 * true, we want the setuid capability.
	 */
	SET_CAP(CAP_SETUID);
#endif

	/*
	 * Since we call initgroups, we need this.
	 */
	SET_CAP(CAP_SETGID);

	/*
	 * Without this, we run into problems reading a configuration file
	 * owned by a non-root user and non-world-readable on startup.
	 */
	SET_CAP(CAP_DAC_READ_SEARCH);

	/*
	 * XXX  We might want to add CAP_SYS_RESOURCE, though it's not
	 *      clear it would work right given the way linuxthreads work.
	 * XXXDCL But since we need to be able to set the maximum number
	 * of files, the stack size, data size, and core dump size to
	 * support named.conf options, this is now being added to test.
	 */
	SET_CAP(CAP_SYS_RESOURCE);

	linux_setcaps(caps);

#ifdef HAVE_LIBCAP
	FREE_CAP;
#endif
}

static void
linux_minprivs(void) {
	cap_t caps;
#ifdef HAVE_LIBCAP
	cap_t curcaps;
	cap_value_t capval;
	char strbuf[ISC_STRERRORSIZE];
	int err;
#endif

	INIT_CAP;
	/*%
	 * Drop all privileges except the ability to bind() to privileged
	 * ports.
	 *
	 * It's important that we drop CAP_SYS_CHROOT.  If we didn't, it
	 * chroot() could be used to escape from the chrooted area.
	 */

	SET_CAP(CAP_NET_BIND_SERVICE);

	/*
	 * XXX  We might want to add CAP_SYS_RESOURCE, though it's not
	 *      clear it would work right given the way linuxthreads work.
	 * XXXDCL But since we need to be able to set the maximum number
	 * of files, the stack size, data size, and core dump size to
	 * support named.conf options, this is now being added to test.
	 */
	SET_CAP(CAP_SYS_RESOURCE);

	linux_setcaps(caps);

#ifdef HAVE_LIBCAP
	FREE_CAP;
#endif
}

#ifdef HAVE_SYS_PRCTL_H
static void
linux_keepcaps(void) {
	char strbuf[ISC_STRERRORSIZE];
	/*%
	 * Ask the kernel to allow us to keep our capabilities after we
	 * setuid().
	 */

	if (prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0) < 0) {
		if (errno != EINVAL) {
			isc__strerror(errno, strbuf, sizeof(strbuf));
			ns_main_earlyfatal("prctl() failed: %s", strbuf);
		}
	} else {
		non_root_caps = ISC_TRUE;
		if (getuid() != 0)
			non_root = ISC_TRUE;
	}
}
#endif

#endif	/* HAVE_LINUX_CAPABILITY_H */


static void
setup_syslog(const char *progname) {
	int options;

	options = LOG_PID;
#ifdef LOG_NDELAY
	options |= LOG_NDELAY;
#endif
	openlog(isc_file_basename(progname), options, ISC_FACILITY);
}

void
ns_os_init(const char *progname) {
	setup_syslog(progname);
#ifdef HAVE_LINUX_CAPABILITY_H
	linux_initialprivs();
#endif
#ifdef HAVE_LINUXTHREADS
	mainpid = getpid();
#endif
#ifdef SIGXFSZ
	signal(SIGXFSZ, SIG_IGN);
#endif
}

void
ns_os_daemonize(void) {
	pid_t pid;
	char strbuf[ISC_STRERRORSIZE];

	if (pipe(dfd) == -1) {
		isc__strerror(errno, strbuf, sizeof(strbuf));
		ns_main_earlyfatal("pipe(): %s", strbuf);
	}

	pid = fork();
	if (pid == -1) {
		isc__strerror(errno, strbuf, sizeof(strbuf));
		ns_main_earlyfatal("fork(): %s", strbuf);
	}
	if (pid != 0) {
		int n;
		/*
		 * Wait for the child to finish loading for the first time.
		 * This would be so much simpler if fork() worked once we
		 * were multi-threaded.
		 */
		(void)close(dfd[1]);
		do {
			char buf;
			n = read(dfd[0], &buf, 1);
			if (n == 1)
				_exit(0);
		} while (n == -1 && errno == EINTR);
		_exit(1);
	}
	(void)close(dfd[0]);

	/*
	 * We're the child.
	 */

#ifdef HAVE_LINUXTHREADS
	mainpid = getpid();
#endif

	if (setsid() == -1) {
		isc__strerror(errno, strbuf, sizeof(strbuf));
		ns_main_earlyfatal("setsid(): %s", strbuf);
	}

	/*
	 * Try to set stdin, stdout, and stderr to /dev/null, but press
	 * on even if it fails.
	 *
	 * XXXMLG The close() calls here are unneeded on all but NetBSD, but
	 * are harmless to include everywhere.  dup2() is supposed to close
	 * the FD if it is in use, but unproven-pthreads-0.16 is broken
	 * and will end up closing the wrong FD.  This will be fixed eventually,
	 * and these calls will be removed.
	 */
	if (devnullfd != -1) {
		if (devnullfd != STDIN_FILENO) {
			(void)close(STDIN_FILENO);
			(void)dup2(devnullfd, STDIN_FILENO);
		}
		if (devnullfd != STDOUT_FILENO) {
			(void)close(STDOUT_FILENO);
			(void)dup2(devnullfd, STDOUT_FILENO);
		}
		if (devnullfd != STDERR_FILENO) {
			(void)close(STDERR_FILENO);
			(void)dup2(devnullfd, STDERR_FILENO);
		}
	}
}

void
ns_os_started(void) {
	char buf = 0;

	/*
	 * Signal to the parent that we started successfully.
	 */
	if (dfd[0] != -1 && dfd[1] != -1) {
		if (write(dfd[1], &buf, 1) != 1)
			ns_main_earlyfatal("unable to signal parent that we "
					   "otherwise started successfully.");
		close(dfd[1]);
		dfd[0] = dfd[1] = -1;
	}
}

void
ns_os_opendevnull(void) {
	devnullfd = open("/dev/null", O_RDWR, 0);
}

void
ns_os_closedevnull(void) {
	if (devnullfd != STDIN_FILENO &&
	    devnullfd != STDOUT_FILENO &&
	    devnullfd != STDERR_FILENO) {
		close(devnullfd);
		devnullfd = -1;
	}
}

static isc_boolean_t
all_digits(const char *s) {
	if (*s == '\0')
		return (ISC_FALSE);
	while (*s != '\0') {
		if (!isdigit((*s)&0xff))
			return (ISC_FALSE);
		s++;
	}
	return (ISC_TRUE);
}

void
ns_os_chroot(const char *root) {
	char strbuf[ISC_STRERRORSIZE];
#ifdef HAVE_LIBSCF
	ns_smf_chroot = 0;
#endif
	if (root != NULL) {
#ifdef HAVE_CHROOT
		if (chroot(root) < 0) {
			isc__strerror(errno, strbuf, sizeof(strbuf));
			ns_main_earlyfatal("chroot(): %s", strbuf);
		}
#else
		ns_main_earlyfatal("chroot(): disabled");
#endif
		if (chdir("/") < 0) {
			isc__strerror(errno, strbuf, sizeof(strbuf));
			ns_main_earlyfatal("chdir(/): %s", strbuf);
		}
#ifdef HAVE_LIBSCF
		/* Set ns_smf_chroot flag on successful chroot. */
		ns_smf_chroot = 1;
#endif
	}
}

void
ns_os_inituserinfo(const char *username) {
	char strbuf[ISC_STRERRORSIZE];
	if (username == NULL)
		return;

	if (all_digits(username))
		runas_pw = getpwuid((uid_t)atoi(username));
	else
		runas_pw = getpwnam(username);
	endpwent();

	if (runas_pw == NULL)
		ns_main_earlyfatal("user '%s' unknown", username);

	if (getuid() == 0) {
		if (initgroups(runas_pw->pw_name, runas_pw->pw_gid) < 0) {
			isc__strerror(errno, strbuf, sizeof(strbuf));
			ns_main_earlyfatal("initgroups(): %s", strbuf);
		}
	}

}

void
ns_os_changeuser(void) {
	char strbuf[ISC_STRERRORSIZE];
	if (runas_pw == NULL || done_setuid)
		return;

	done_setuid = ISC_TRUE;

#ifdef HAVE_LINUXTHREADS
#ifdef HAVE_LINUX_CAPABILITY_H
	if (!non_root_caps)
		ns_main_earlyfatal("-u with Linux threads not supported: "
				   "requires kernel support for "
				   "prctl(PR_SET_KEEPCAPS)");
#else
	ns_main_earlyfatal("-u with Linux threads not supported: "
			   "no capabilities support or capabilities "
			   "disabled at build time");
#endif
#endif

	if (setgid(runas_pw->pw_gid) < 0) {
		isc__strerror(errno, strbuf, sizeof(strbuf));
		ns_main_earlyfatal("setgid(): %s", strbuf);
	}

	if (setuid(runas_pw->pw_uid) < 0) {
		isc__strerror(errno, strbuf, sizeof(strbuf));
		ns_main_earlyfatal("setuid(): %s", strbuf);
	}

#if defined(HAVE_SYS_PRCTL_H) && defined(PR_SET_DUMPABLE)
	/*
	 * Restore the ability of named to drop core after the setuid()
	 * call has disabled it.
	 */
	if (prctl(PR_SET_DUMPABLE,1,0,0,0) < 0) {
		isc__strerror(errno, strbuf, sizeof(strbuf));
		ns_main_earlywarning("prctl(PR_SET_DUMPABLE) failed: %s",
				     strbuf);
	}
#endif
#if defined(HAVE_LINUX_CAPABILITY_H) && !defined(HAVE_LINUXTHREADS)
	linux_minprivs();
#endif
}

void
ns_os_adjustnofile() {
#ifdef HAVE_LINUXTHREADS
	isc_result_t result;
	isc_resourcevalue_t newvalue;

	/*
	 * Linux: max number of open files specified by one thread doesn't seem
	 * to apply to other threads on Linux.
	 */
	newvalue = ISC_RESOURCE_UNLIMITED;

	result = isc_resource_setlimit(isc_resource_openfiles, newvalue);
	if (result != ISC_R_SUCCESS)
		ns_main_earlywarning("couldn't adjust limit on open files");
#endif
}

void
ns_os_minprivs(void) {
#ifdef HAVE_SYS_PRCTL_H
	linux_keepcaps();
#endif

#ifdef HAVE_LINUXTHREADS
	ns_os_changeuser(); /* Call setuid() before threads are started */
#endif

#if defined(HAVE_LINUX_CAPABILITY_H) && defined(HAVE_LINUXTHREADS)
	linux_minprivs();
#endif
}

static int
safe_open(const char *filename, isc_boolean_t append) {
	int fd;
	struct stat sb;

	if (stat(filename, &sb) == -1) {
		if (errno != ENOENT)
			return (-1);
	} else if ((sb.st_mode & S_IFREG) == 0) {
		errno = EOPNOTSUPP;
		return (-1);
	}

	if (append)
		fd = open(filename, O_WRONLY|O_CREAT|O_APPEND,
			  S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	else {
		if (unlink(filename) < 0 && errno != ENOENT)
			return (-1);
		fd = open(filename, O_WRONLY|O_CREAT|O_EXCL,
			  S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	}
	return (fd);
}

static void
cleanup_pidfile(void) {
	int n;
	if (pidfile != NULL) {
		n = unlink(pidfile);
		if (n == -1 && errno != ENOENT)
			ns_main_earlywarning("unlink '%s': failed", pidfile);
		free(pidfile);
	}
	pidfile = NULL;
}

static int
mkdirpath(char *filename, void (*report)(const char *, ...)) {
	char *slash = strrchr(filename, '/');
	char strbuf[ISC_STRERRORSIZE];
	unsigned int mode;

	if (slash != NULL && slash != filename) {
		struct stat sb;
		*slash = '\0';

		if (stat(filename, &sb) == -1) {
			if (errno != ENOENT) {
				isc__strerror(errno, strbuf, sizeof(strbuf));
				(*report)("couldn't stat '%s': %s", filename,
					  strbuf);
				goto error;
			}
			if (mkdirpath(filename, report) == -1)
				goto error;
			mode = S_IRUSR | S_IWUSR | S_IXUSR;	/* u=rwx */
			mode |= S_IRGRP | S_IXGRP;		/* g=rx */
			mode |= S_IROTH | S_IXOTH;		/* o=rx */
			if (mkdir(filename, mode) == -1) {
				isc__strerror(errno, strbuf, sizeof(strbuf));
				(*report)("couldn't mkdir '%s': %s", filename,
					  strbuf);
				goto error;
			}
		}
		*slash = '/';
	}
	return (0);

 error:
	*slash = '/';
	return (-1);
}

void
ns_os_writepidfile(const char *filename, isc_boolean_t first_time) {
	int fd;
	FILE *lockfile;
	size_t len;
	pid_t pid;
	char strbuf[ISC_STRERRORSIZE];
	void (*report)(const char *, ...);

	/*
	 * The caller must ensure any required synchronization.
	 */

	report = first_time ? ns_main_earlyfatal : ns_main_earlywarning;

	cleanup_pidfile();

	if (filename == NULL)
		return;

	len = strlen(filename);
	pidfile = malloc(len + 1);
	if (pidfile == NULL) {
		isc__strerror(errno, strbuf, sizeof(strbuf));
		(*report)("couldn't malloc '%s': %s", filename, strbuf);
		return;
	}

	/* This is safe. */
	strcpy(pidfile, filename);

	/*
	 * Make the containing directory if it doesn't exist.
	 */
	if (mkdirpath(pidfile, report) == -1) {
		free(pidfile);
		pidfile = NULL;
		return;
	}

	fd = safe_open(filename, ISC_FALSE);
	if (fd < 0) {
		isc__strerror(errno, strbuf, sizeof(strbuf));
		(*report)("couldn't open pid file '%s': %s", filename, strbuf);
		free(pidfile);
		pidfile = NULL;
		return;
	}
	lockfile = fdopen(fd, "w");
	if (lockfile == NULL) {
		isc__strerror(errno, strbuf, sizeof(strbuf));
		(*report)("could not fdopen() pid file '%s': %s",
			  filename, strbuf);
		(void)close(fd);
		cleanup_pidfile();
		return;
	}
#ifdef HAVE_LINUXTHREADS
	pid = mainpid;
#else
	pid = getpid();
#endif
	if (fprintf(lockfile, "%ld\n", (long)pid) < 0) {
		(*report)("fprintf() to pid file '%s' failed", filename);
		(void)fclose(lockfile);
		cleanup_pidfile();
		return;
	}
	if (fflush(lockfile) == EOF) {
		(*report)("fflush() to pid file '%s' failed", filename);
		(void)fclose(lockfile);
		cleanup_pidfile();
		return;
	}
	(void)fclose(lockfile);
}

void
ns_os_shutdown(void) {
	closelog();
	cleanup_pidfile();
}

isc_result_t
ns_os_gethostname(char *buf, size_t len) {
	int n;

	n = gethostname(buf, len);
	return ((n == 0) ? ISC_R_SUCCESS : ISC_R_FAILURE);
}

static char *
next_token(char **stringp, const char *delim) {
	char *res;

	do {
		res = strsep(stringp, delim);
		if (res == NULL)
			break;
	} while (*res == '\0');
	return (res);
}

void
ns_os_shutdownmsg(char *command, isc_buffer_t *text) {
	char *input, *ptr;
	unsigned int n;
	pid_t pid;

	input = command;

	/* Skip the command name. */
	ptr = next_token(&input, " \t");
	if (ptr == NULL)
		return;

	ptr = next_token(&input, " \t");
	if (ptr == NULL)
		return;

	if (strcmp(ptr, "-p") != 0)
		return;

#ifdef HAVE_LINUXTHREADS
	pid = mainpid;
#else
	pid = getpid();
#endif

	n = snprintf((char *)isc_buffer_used(text),
		     isc_buffer_availablelength(text),
		     "pid: %ld", (long)pid);
	/* Only send a message if it is complete. */
	if (n < isc_buffer_availablelength(text))
		isc_buffer_add(text, n);
}

void
ns_os_tzset(void) {
#ifdef HAVE_TZSET
	tzset();
#endif
}
