/* defs.h - include or define things that aren't present on all systems
 * vixie@decwrl 26dec92 [new]
 *
 * $Id: portability.h,v 1.1 1994/09/22 20:45:08 pst Exp $
 */

/*
 * ++Copyright++
 * -
 * Copyright (c)
 *    The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
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
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */

/* XXX:	this file has become a hopeless morass, and will be redone someday. */

#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#ifndef TIME_H_INCLUDED
# include <sys/time.h>
# define TIME_H_INCLUDED
#endif

#if defined(__convex__)
# if !defined(_POSIX_SOURCE)
#  define _POSIX_SOURCE
# endif
# define USE_UTIME
# define NEED_PUTENV
#endif

#if defined(_CRAY)
# if !defined(_POSIX_SOURCE)
#  define _POSIX_SOURCE
# endif
# define writev(a,b,c) __writev(a,b,c)
# define setitimer(a,b,c) __setitimer(a,b,c)
#endif

/* SCO UNIX defines only this unique symbol, apparently. */
#if defined(M_UNIX)
/* XXX - why is this POSIX_SOURCE instead of _POSIX_SOURCE? */
# undef POSIX_SOURCE
# define POSIX_SIGNALS
# define HAVE_FCHMOD 0
# define writev(a,b,c) __writev(a,b,c)
# define ftruncate(a,b) __ftruncate(a,b)
#endif

#ifdef NeXT
# define NEED_PUTENV
# define NEED_SETENV
#endif

#if defined(__sgi)
# define BSD 43
# define vfork fork
#endif

#if defined(SUNOS4)
# define BSD 43
#endif

#if defined(_POSIX_SOURCE) || defined(__sgi) || defined(ultrix) || \
	defined(__hpux) || (defined(BSD) && (BSD >= 199103)) || \
	(defined(sun) && defined(SYSV))
# define USE_POSIX
#endif

#if defined(ultrix) && !defined(BSD)
# define BSD 42
#endif

#if defined(host_mips) && defined(SYSTYPE_BSD43)
# define RISCOS_BSD
#endif

#if defined(SVR4) && !defined(SYSV)
# define SYSV
#endif

#if defined(SYSV) || defined(ultrix) || (defined(BSD) && BSD >= 199306)
# define USE_UTIME
#endif

#if defined(BSD) && BSD >= 199006 && !defined(i386) && !defined(RISCOS_BSD)
# define HAVE_DAEMON
#endif

#if defined(__hpux)
# if defined(__STDC__)
#  define select(a,b,c,d,e) select(a, (int *)b, (int *)c, (int *)d, e)
#  define ctime(x) ctime((const time_t *)x)
# endif /*__STDC__*/
# ifndef SYSV
# define USE_UTIME
# define setlinebuf(x) setvbuf(x, NULL, _IOLBF, BUFSIZ)
# define SIGWINCH SIGWINDOW
# endif /*SYSV*/
#endif /*__hpux*/

#if defined(_SEQUENT_)
# include <netinet/in_systm.h>
# define USE_UTIME
# define USE_POSIX
# define NEED_GETTIMEOFDAY
# define _TIMEZONE timezoneBSD
struct timezoneBSD {
	int tz_minuteswest;
	int tz_dsttime;
};
#endif

#ifndef __P
# if defined(__STDC__) || defined(__GNUC__)
#  define __P(x) x
# else
#  define __P(x) ()
# endif
#endif

#ifndef _TIMEZONE
# define _TIMEZONE timezone
#endif

#if defined(USE_POSIX)
# include <stdlib.h>
# include <unistd.h>
# include <limits.h>

#else

# define NEED_STRTOUL

# define STDIN_FILENO	0
# define STDOUT_FILENO	1
# define STDERR_FILENO	2
# ifndef NeXT
extern char *getenv __P((char *));
# else
extern char *getenv __P((const char *));
# endif
extern int errno;

# if !defined(DMALLOC) && !defined(NeXT)
extern char *malloc(), *realloc(), *calloc();
#  if defined(sun)
extern int free();
#  else
extern void free();
#  endif
# endif

extern int getdtablesize __P((void));
# ifdef SHORT_FNAMES
extern long pathconf __P((const char *path, int name));
# endif

#endif /*USE_POSIX*/

#ifndef UINT_MAX
# ifdef __STDC__
#  define UINT_MAX	4294967295u             /* max value of an "u_int" */
# else
#  define UINT_MAX	((unsigned)4294967295)  /* max value of an "u_int" */
# endif
#  define ULONG_MAX	UINT_MAX        /* max decimal value of a "u_long" */
#endif

#ifndef INT_MAX
# define INT_MAX	2147483647	/* max decimal value of an "int" */
#endif

#ifndef	IN_LOOPBACKNET
# define IN_LOOPBACKNET	127
#endif

#ifndef	INADDR_NONE
# define INADDR_NONE	0xffffffff
#endif

#if defined(apollo)
		/* Defined in /usr/include/netinet/in.h but doesn't work */
#undef IP_OPTIONS
#endif

#if !defined(__STDC__) && !defined(const)
# define const /*constant*/
#endif

#if !defined(__convex__) && (!defined(BSD) || (BSD < 199103))
int      strcasecmp __P((const char *, const char *));
#endif

/* is USE_POSIX the right thing to use here? */
#if (!defined(BSD) || (BSD <= 43)) && \
	!defined(NeXT) && \
	!defined(__convex__) && \
	!defined(USE_POSIX)
extern void syslog();
extern char *ctime __P((const time_t *clock));
extern int	close(), setitimer(), recv(), sendto(), sigsetmask(),
		atoi(), getpid(), fork(), read(), ioctl(),
		setsockopt(), socket(), bind();
#endif

#if !defined(bcopy)	/* some machines have their own macros for this */
# if defined(USE_POSIX) || \
	 (defined(__STDC__) && !defined(sun) && !defined(sequent))
/* use ANSI C3.159-1989 (``ANSI C'') functions if possible;
 * ideally we would change the code to use them and then
 * define them in terms of bcopy et al if !defined(__STDC__)
 * but that's more work.
 */
#  define bcopy(a,b,c) memmove(b,a,c)
#  define bzero(a,b) memset(a,0,b)
#  define bcmp(a,b,c) memcmp(a,b,c)
# else
extern void bcopy();
extern void bzero();
extern int bcmp();
# endif /* BSD */
#endif /* bcopy */

#if (!defined(BSD) || (BSD < 43) || defined(RISCOS_BSD)) \
	&& !defined(USE_POSIX) && !defined(apollo) && !defined(sequent) \
	&& !defined(M_UNIX)
# define NEED_STRERROR
# define NEED_PUTENV
#endif

#if defined(SUNOS4)
# define NEED_STRERROR
#endif

#if (!defined(BSD) || (BSD < 43))
# define NEED_MKSTEMP
# if !defined(ultrix) && !defined(apollo)
#  define NEED_STRCASECMP
#  define NEED_MKTEMP
#  define NEED_STRPBRK
# endif
#endif

#if defined(USE_POSIX)
# define POSIX_SIGNALS
#endif

/*
 * Attempt to configure for type of function returned by signal-catching
 * functions (which signal and sigvec.sv_handler take a pointer to).
 * This can guess for BSD; otherwise, define SIG_FN externally.
 */
#ifndef	SIG_FN
# ifdef BSD
#  if (BSD >= 199006) || defined(NeXT) || defined(__osf__) || defined(sun) \
	|| defined(ultrix) || defined(apollo) || defined(POSIX_SIGNALS)
#   define SIG_FN void		/* signal-catching functions return void */
#  else
#   define SIG_FN int		/* signal-catching functions return int */
#  endif
# else /*BSD*/
#  define SIG_FN void		/* signal-catching functions return void */
# endif /*BSD*/
#endif

#if !defined(ntohl) && !defined(htonl) && defined(BSD) && (BSD <= 43)
/* if these aren't null macros in netinet/in.h, extern them here. */
extern u_short htons(), ntohs();
extern u_long htonl(), ntohl();
#endif

#if defined(USE_POSIX) && !defined(sun) && !defined(__sgi) \
	&& !defined(__convex__) && !defined(ultrix)
# define PORT_NONBLOCK	O_NONBLOCK
# define PORT_WOULDBLK	EAGAIN
#else
# define PORT_NONBLOCK	O_NDELAY
# define PORT_WOULDBLK	EWOULDBLOCK
#endif

#if defined(USE_POSIX)
# define USE_SETSID
#endif

#if defined(USE_POSIX) || !defined(SYSV)
#define USE_WAITPID
#endif

#if !defined(USE_POSIX)
#define waitpid(x,y,z) (wait3(y,z,(struct rusage *)NULL))
#endif

#if defined(NeXT) || defined(_AIX)
# undef WIFEXITED
# undef WEXITSTATUS
# undef WIFSIGNALED
# undef WTERMSIG
#endif /* NeXT */

#if defined(sequent)
#define WEXITSTATUS(x)	((x).w_retcode)
#define WTERMSIG(x)	((x).w_termsig)
#endif /* sequent */

#if !defined(WIFEXITED)
# define WIFEXITED(x) (!(x & 0200))
#endif
#if !defined(WEXITSTATUS)
# define WEXITSTATUS(x) (x >> 8)
#endif
#if !defined(WIFSIGNALED)
# define WIFSIGNALED(x) ((x & 0200) && ((x & 0200) != 0177))
#endif
#if !defined(WTERMSIG)
# define WTERMSIG(x) (x & 0177)
#endif

#ifndef S_ISDIR
# ifndef S_IFMT
#  define S_IFMT 0170000
# endif
# ifndef S_IFDIR
#  define S_IFDIR 0040000
# endif
# define S_ISDIR(m)	((m & S_IFMT) == S_IFDIR)
#endif

#ifndef S_ISREG
# ifndef S_IFMT
#  define S_IFMT 0170000
# endif
# ifndef S_IFREG
#  define S_IFREG 0100000
# endif
# define S_ISREG(m)	((m & S_IFMT) == S_IFREG)
#endif

#ifndef S_ISFIFO
# ifndef S_IFMT
#  define S_IFMT 0170000
# endif
# ifndef S_IFIFO
#  define S_IFIFO 0010000
# endif
# define S_ISFIFO(m)	((m & S_IFMT) == S_IFIFO)
#endif

#if (defined(ultrix) || defined(__osf__)) && defined(NEED_STRTOUL)
# undef NEED_STRTOUL
#endif

#if defined(ultrix) || defined(__osf__)
# define MAYBE_HESIOD
#endif

#ifndef FD_SET
#define	NFDBITS		32
#define	FD_SETSIZE	32
#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)	bzero((char *)(p), sizeof(*(p)))
#endif

#ifndef MIN
# define MIN(x, y)	((x > y) ?y :x)
#endif
#ifndef MAX
# define MAX(x, y)	((x > y) ?x :y)
#endif

#if !defined(PATH_MAX)
# if defined(_POSIX_PATH_MAX)
#  define PATH_MAX _POSIX_PATH_MAX
# else
#  if defined(MAXPATHLEN)
#   define PATH_MAX MAXPATHLEN
#  endif
# endif
#endif

/*
 *  Because Convex has true library function feof() which is
 *  patently wrong (it test bit _IOREAD) we need feof() as
 *  a macro.
 */
#if defined(__convex__) && !defined(feof)
#  define   feof(p)	((p)->_flag&_IOEOF)
#endif

/*
 * Assume that a system has fchmod() unless something above says otherwise.
 */
#if !defined(HAVE_FCHMOD)
# define HAVE_FCHMOD 1
#endif

/*
 * Prototype the functions we'll be supplying.
 */
#ifdef NEED_PUTENV
extern int putenv __P((char *));
#endif

#ifdef NEED_GETTIMEOFDAY
extern int gettimeofday __P((struct timeval *, struct _TIMEZONE *));
#endif
