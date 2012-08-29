/* $Id: bsd-misc.h,v 1.20 2012/02/14 18:03:31 tim Exp $ */

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

#ifndef _BSD_MISC_H
#define _BSD_MISC_H

#include "includes.h"

char *ssh_get_progname(char *);

#ifndef HAVE_SETSID
#define setsid() setpgrp(0, getpid())
#endif /* !HAVE_SETSID */

#ifndef HAVE_SETENV
int setenv(const char *, const char *, int);
#endif /* !HAVE_SETENV */

#ifndef HAVE_SETLOGIN
int setlogin(const char *);
#endif /* !HAVE_SETLOGIN */

#ifndef HAVE_INNETGR
int innetgr(const char *, const char *, const char *, const char *);
#endif /* HAVE_INNETGR */

#if !defined(HAVE_SETEUID) && defined(HAVE_SETREUID)
int seteuid(uid_t);
#endif /* !defined(HAVE_SETEUID) && defined(HAVE_SETREUID) */

#if !defined(HAVE_SETEGID) && defined(HAVE_SETRESGID)
int setegid(uid_t);
#endif /* !defined(HAVE_SETEGID) && defined(HAVE_SETRESGID) */

#if !defined(HAVE_STRERROR) && defined(HAVE_SYS_ERRLIST) && defined(HAVE_SYS_NERR)
const char *strerror(int);
#endif 


#ifndef HAVE_UTIMES
#ifndef HAVE_STRUCT_TIMEVAL
struct timeval {
	long tv_sec;
	long tv_usec;
}
#endif /* HAVE_STRUCT_TIMEVAL */

int utimes(char *, struct timeval *);
#endif /* HAVE_UTIMES */

#ifndef HAVE_TRUNCATE
int truncate (const char *, off_t);
#endif /* HAVE_TRUNCATE */

#if !defined(HAVE_NANOSLEEP) && !defined(HAVE_NSLEEP)
#ifndef HAVE_STRUCT_TIMESPEC
struct timespec {
	time_t	tv_sec;
	long	tv_nsec;
};
#endif
int nanosleep(const struct timespec *, struct timespec *);
#endif

#ifndef HAVE_TCGETPGRP
pid_t tcgetpgrp(int);
#endif

#ifndef HAVE_TCSENDBREAK
int tcsendbreak(int, int);
#endif

#ifndef HAVE_UNSETENV
int unsetenv(const char *);
#endif

/* wrapper for signal interface */
typedef void (*mysig_t)(int);
mysig_t mysignal(int sig, mysig_t act);

#define signal(a,b) mysignal(a,b)

#ifndef HAVE_ISBLANK
int	isblank(int);
#endif

#endif /* _BSD_MISC_H */
