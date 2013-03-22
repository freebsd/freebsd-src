
/*
 * Copyright (c) 1999-2004 Damien Miller <djm@mindrot.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "includes.h"

#include <sys/types.h>
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "xmalloc.h"

#ifndef HAVE___PROGNAME
char *__progname;
#endif

/*
 * NB. duplicate __progname in case it is an alias for argv[0]
 * Otherwise it may get clobbered by setproctitle()
 */
char *ssh_get_progname(char *argv0)
{
#ifdef HAVE___PROGNAME
	extern char *__progname;

	return xstrdup(__progname);
#else
	char *p;

	if (argv0 == NULL)
		return ("unknown");	/* XXX */
	p = strrchr(argv0, '/');
	if (p == NULL)
		p = argv0;
	else
		p++;

	return (xstrdup(p));
#endif
}

#ifndef HAVE_SETLOGIN
int setlogin(const char *name)
{
	return (0);
}
#endif /* !HAVE_SETLOGIN */

#ifndef HAVE_INNETGR
int innetgr(const char *netgroup, const char *host, 
            const char *user, const char *domain)
{
	return (0);
}
#endif /* HAVE_INNETGR */

#if !defined(HAVE_SETEUID) && defined(HAVE_SETREUID)
int seteuid(uid_t euid)
{
	return (setreuid(-1, euid));
}
#endif /* !defined(HAVE_SETEUID) && defined(HAVE_SETREUID) */

#if !defined(HAVE_SETEGID) && defined(HAVE_SETRESGID)
int setegid(uid_t egid)
{
	return(setresgid(-1, egid, -1));
}
#endif /* !defined(HAVE_SETEGID) && defined(HAVE_SETRESGID) */

#if !defined(HAVE_STRERROR) && defined(HAVE_SYS_ERRLIST) && defined(HAVE_SYS_NERR)
const char *strerror(int e)
{
	extern int sys_nerr;
	extern char *sys_errlist[];
	
	if ((e >= 0) && (e < sys_nerr))
		return (sys_errlist[e]);

	return ("unlisted error");
}
#endif

#ifndef HAVE_UTIMES
int utimes(char *filename, struct timeval *tvp)
{
	struct utimbuf ub;

	ub.actime = tvp[0].tv_sec;
	ub.modtime = tvp[1].tv_sec;
	
	return (utime(filename, &ub));
}
#endif 

#ifndef HAVE_TRUNCATE
int truncate(const char *path, off_t length)
{
	int fd, ret, saverrno;

	fd = open(path, O_WRONLY);
	if (fd < 0)
		return (-1);

	ret = ftruncate(fd, length);
	saverrno = errno;
	close(fd);
	if (ret == -1)
		errno = saverrno;

	return(ret);
}
#endif /* HAVE_TRUNCATE */

#if !defined(HAVE_NANOSLEEP) && !defined(HAVE_NSLEEP)
int nanosleep(const struct timespec *req, struct timespec *rem)
{
	int rc, saverrno;
	extern int errno;
	struct timeval tstart, tstop, tremain, time2wait;

	TIMESPEC_TO_TIMEVAL(&time2wait, req)
	(void) gettimeofday(&tstart, NULL);
	rc = select(0, NULL, NULL, NULL, &time2wait);
	if (rc == -1) {
		saverrno = errno;
		(void) gettimeofday (&tstop, NULL);
		errno = saverrno;
		tremain.tv_sec = time2wait.tv_sec - 
			(tstop.tv_sec - tstart.tv_sec);
		tremain.tv_usec = time2wait.tv_usec - 
			(tstop.tv_usec - tstart.tv_usec);
		tremain.tv_sec += tremain.tv_usec / 1000000L;
		tremain.tv_usec %= 1000000L;
	} else {
		tremain.tv_sec = 0;
		tremain.tv_usec = 0;
	}
	if (rem != NULL)
		TIMEVAL_TO_TIMESPEC(&tremain, rem)

	return(rc);
}
#endif

#if !defined(HAVE_USLEEP)
int usleep(unsigned int useconds)
{
	struct timespec ts;

	ts.tv_sec = useconds / 1000000;
	ts.tv_nsec = (useconds % 1000000) * 1000;
	return nanosleep(&ts, NULL);
}
#endif

#ifndef HAVE_TCGETPGRP
pid_t
tcgetpgrp(int fd)
{
	int ctty_pgrp;

	if (ioctl(fd, TIOCGPGRP, &ctty_pgrp) == -1)
		return(-1);
	else
		return(ctty_pgrp);
}
#endif /* HAVE_TCGETPGRP */

#ifndef HAVE_TCSENDBREAK
int
tcsendbreak(int fd, int duration)
{
# if defined(TIOCSBRK) && defined(TIOCCBRK)
	struct timeval sleepytime;

	sleepytime.tv_sec = 0;
	sleepytime.tv_usec = 400000;
	if (ioctl(fd, TIOCSBRK, 0) == -1)
		return (-1);
	(void)select(0, 0, 0, 0, &sleepytime);
	if (ioctl(fd, TIOCCBRK, 0) == -1)
		return (-1);
	return (0);
# else
	return -1;
# endif
}
#endif /* HAVE_TCSENDBREAK */

mysig_t
mysignal(int sig, mysig_t act)
{
#ifdef HAVE_SIGACTION
	struct sigaction sa, osa;

	if (sigaction(sig, NULL, &osa) == -1)
		return (mysig_t) -1;
	if (osa.sa_handler != act) {
		memset(&sa, 0, sizeof(sa));
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
#ifdef SA_INTERRUPT
		if (sig == SIGALRM)
			sa.sa_flags |= SA_INTERRUPT;
#endif
		sa.sa_handler = act;
		if (sigaction(sig, &sa, NULL) == -1)
			return (mysig_t) -1;
	}
	return (osa.sa_handler);
#else
	#undef signal
	return (signal(sig, act));
#endif
}

#ifndef HAVE_STRDUP
char *
strdup(const char *str)
{
	size_t len;
	char *cp;

	len = strlen(str) + 1;
	cp = malloc(len);
	if (cp != NULL)
		return(memcpy(cp, str, len));
	return NULL;
}
#endif

#ifndef HAVE_ISBLANK
int
isblank(int c)
{
	return (c == ' ' || c == '\t');
}
#endif

#ifndef HAVE_GETPGID
pid_t
getpgid(pid_t pid)
{
#if defined(HAVE_GETPGRP) && !defined(GETPGRP_VOID)
	return getpgrp(pid);
#elif defined(HAVE_GETPGRP)
	if (pid == 0)
		return getpgrp();
#endif

	errno = ESRCH;
	return -1;
}
#endif
