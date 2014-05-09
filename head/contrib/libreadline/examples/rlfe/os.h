/* Copyright (c) 1993-2002
 *      Juergen Weigert (jnweiger@immd4.informatik.uni-erlangen.de)
 *      Michael Schroeder (mlschroe@immd4.informatik.uni-erlangen.de)
 * Copyright (c) 1987 Oliver Laumann
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 ****************************************************************
 * $Id: os.h,v 1.10 1994/05/31 12:32:22 mlschroe Exp $ FAU
 */

#include <stdio.h>
#include <errno.h>

#include <sys/param.h>

/* In strict ANSI mode, HP-UX machines define __hpux but not hpux */
#if defined(__hpux) && !defined(hpux)
# define hpux
#endif

#if defined(__bsdi__) || defined(__386BSD__) || defined(_CX_UX) || defined(hpux) || defined(_IBMR2) || defined(linux)
# include <signal.h>
#endif /* __bsdi__ || __386BSD__ || _CX_UX || hpux || _IBMR2 || linux */

#ifdef ISC
# ifdef ENAMETOOLONG
#  undef ENAMETOOLONG
# endif
# ifdef ENOTEMPTY
#  undef ENOTEMPTY
# endif
# include <sys/bsdtypes.h>
# include <net/errno.h>
#endif

#ifdef sun
# define getpgrp __getpgrp
# define exit __exit
#endif
#ifdef POSIX
# include <unistd.h>
# if defined(__STDC__)
#  include <stdlib.h>
# endif /* __STDC__ */
#endif /* POSIX */
#ifdef sun
# undef getpgrp
# undef exit
#endif /* sun */

#ifndef linux /* all done in <errno.h> */
extern int errno;
#endif /* linux */
#ifndef HAVE_STRERROR
/* No macros, please */
#undef strerror
#endif

#if !defined(SYSV) && !defined(linux)
# ifdef NEWSOS
#  define strlen ___strlen___
#  include <strings.h>
#  undef strlen
# else /* NEWSOS */
#  include <strings.h>
# endif /* NEWSOS */
#else /* SYSV */
# if defined(SVR4) || defined(NEWSOS)
#  define strlen ___strlen___
#  include <string.h>
#  undef strlen
#  if !defined(NEWSOS) && !defined(__hpux)
    extern size_t strlen(const char *);
#  endif
# else /* SVR4 */
#  include <string.h>
# endif /* SVR4 */
#endif /* SYSV */

#ifdef USEVARARGS
# if defined(__STDC__)
#  include <stdarg.h>
#  define VA_LIST(var) va_list var;
#  define VA_DOTS ...
#  define VA_DECL
#  define VA_START(ap, fmt) va_start(ap, fmt)
#  define VA_ARGS(ap) ap
#  define VA_END(ap) va_end(ap)
# else
#  include <varargs.h>
#  define VA_LIST(var) va_list var;
#  define VA_DOTS va_alist
#  define VA_DECL va_dcl
#  define VA_START(ap, fmt) va_start(ap)
#  define VA_ARGS(ap) ap
#  define VA_END(ap) va_end(ap)
# endif
#else
# define VA_LIST(var)
# define VA_DOTS p1, p2, p3, p4, p5, p6
# define VA_DECL unsigned long VA_DOTS;
# define VA_START(ap, fmt)
# define VA_ARGS(ap) VA_DOTS
# define VA_END(ap)
# undef vsnprintf
# define vsnprintf xsnprintf
#endif

#if !defined(sun) && !defined(B43) && !defined(ISC) && !defined(pyr) && !defined(_CX_UX)
# include <time.h>
#endif
#include <sys/time.h>

#ifdef M_UNIX   /* SCO */
# include <sys/stream.h>
# include <sys/ptem.h>
# define ftruncate(fd, s) chsize(fd, s)
#endif

#ifdef SYSV
# define index strchr
# define rindex strrchr
# define bzero(poi,len) memset(poi,0,len)
# define bcmp memcmp
# define killpg(pgrp,sig) kill( -(pgrp), sig)
#endif

#ifndef HAVE_GETCWD
# define getcwd(b,l) getwd(b)
#endif

#ifndef USEBCOPY
# ifdef USEMEMMOVE
#  define bcopy(s,d,len) memmove(d,s,len)
# else
#  ifdef USEMEMCPY
#   define bcopy(s,d,len) memcpy(d,s,len)
#  else
#   define NEED_OWN_BCOPY
#   define bcopy xbcopy
#  endif
# endif
#endif

#ifdef hpux
# define setreuid(ruid, euid) setresuid(ruid, euid, -1)
# define setregid(rgid, egid) setresgid(rgid, egid, -1)
#endif

#if defined(HAVE_SETEUID) || defined(HAVE_SETREUID)
# define USE_SETEUID
#endif

#if !defined(HAVE__EXIT) && !defined(_exit)
#define _exit(x) exit(x)
#endif

#ifndef HAVE_UTIMES
# define utimes utime
#endif

#ifdef BUILTIN_TELNET
# include <netinet/in.h>
# include <arpa/inet.h>
#endif

#if defined(USE_LOCALE) && (!defined(HAVE_SETLOCALE) || !defined(HAVE_STRFTIME))
# undef USE_LOCALE
#endif

/*****************************************************************
 *    terminal handling
 */

#ifdef POSIX
# include <termios.h>
# ifdef hpux
#  include <bsdtty.h>
# endif /* hpux */
# ifdef NCCS
#  define MAXCC NCCS
# else
#  define MAXCC 256
# endif
#else /* POSIX */
# ifdef TERMIO
#  include <termio.h>
#  ifdef NCC
#   define MAXCC NCC
#  else
#   define MAXCC 256
#  endif
#  ifdef CYTERMIO
#   include <cytermio.h>
#  endif
# else /* TERMIO */
#  include <sgtty.h>
# endif /* TERMIO */
#endif /* POSIX */

#ifndef VDISABLE
# ifdef _POSIX_VDISABLE
#  define VDISABLE _POSIX_VDISABLE
# else
#  define VDISABLE 0377
# endif /* _POSIX_VDISABLE */
#endif /* !VDISABLE */


/* on sgi, regardless of the stream head's read mode (RNORM/RMSGN/RMSGD)
 * TIOCPKT mode causes data loss if our buffer is too small (IOSIZE)
 * to hold the whole packet at first read().
 * (Marc Boucher)
 *
 * matthew green:
 * TIOCPKT is broken on dgux 5.4.1 generic AViiON mc88100
 *
 * Joe Traister: On AIX4, programs like irc won't work if screen
 * uses TIOCPKT (select fails to return on pty read).
 */
#if defined(sgi) || defined(DGUX) || defined(_IBMR2)
# undef TIOCPKT
#endif

/* linux ncurses is broken, we have to use our own tputs */
#if defined(linux) && defined(TERMINFO)
# define tputs xtputs
#endif

/* Alexandre Oliva: SVR4 style ptys don't work with osf */
#ifdef __osf__
# undef HAVE_SVR4_PTYS
#endif

/*****************************************************************
 *   utmp handling
 */

#ifdef GETUTENT
  typedef char *slot_t;
#else
  typedef int slot_t;
#endif

#if defined(UTMPOK) || defined(BUGGYGETLOGIN)
# if defined(SVR4) && !defined(DGUX) && !defined(__hpux) && !defined(linux)
#  include <utmpx.h>
#  define UTMPFILE	UTMPX_FILE
#  define utmp		utmpx
#  define getutent	getutxent
#  define getutid	getutxid
#  define getutline	getutxline
#  define pututline	pututxline
#  define setutent	setutxent
#  define endutent	endutxent
#  define ut_time	ut_xtime
# else /* SVR4 */
#  include <utmp.h>
# endif /* SVR4 */
# ifdef apollo
   /* 
    * We don't have GETUTENT, so we dig into utmp ourselves.
    * But we save the permanent filedescriptor and
    * open utmp just when we need to. 
    * This code supports an unsorted utmp. jw.
    */
#  define UTNOKEEP
# endif /* apollo */

# ifndef UTMPFILE
#  ifdef UTMP_FILE
#   define UTMPFILE	UTMP_FILE
#  else
#   ifdef _PATH_UTMP
#    define UTMPFILE	_PATH_UTMP
#   else
#    define UTMPFILE	"/etc/utmp"
#   endif /* _PATH_UTMP */
#  endif
# endif

#endif /* UTMPOK || BUGGYGETLOGIN */

#if !defined(UTMPOK) && defined(USRLIMIT)
# undef USRLIMIT
#endif

#ifdef LOGOUTOK
# ifndef LOGINDEFAULT
#  define LOGINDEFAULT 0
# endif
#else
# ifdef LOGINDEFAULT
#  undef LOGINDEFAULT
# endif
# define LOGINDEFAULT 1
#endif


/*****************************************************************
 *    file stuff
 */

#ifndef F_OK
#define F_OK 0
#endif
#ifndef X_OK
#define X_OK 1
#endif
#ifndef W_OK
#define W_OK 2
#endif
#ifndef R_OK
#define R_OK 4
#endif

#ifndef S_IFIFO
#define S_IFIFO  0010000
#endif
#ifndef S_IREAD
#define S_IREAD  0000400
#endif
#ifndef S_IWRITE
#define S_IWRITE 0000200
#endif
#ifndef S_IEXEC
#define S_IEXEC  0000100
#endif

#if defined(S_IFIFO) && defined(S_IFMT) && !defined(S_ISFIFO)
#define S_ISFIFO(mode) (((mode) & S_IFMT) == S_IFIFO)
#endif
#if defined(S_IFSOCK) && defined(S_IFMT) && !defined(S_ISSOCK)
#define S_ISSOCK(mode) (((mode) & S_IFMT) == S_IFSOCK)
#endif
#if defined(S_IFCHR) && defined(S_IFMT) && !defined(S_ISCHR)
#define S_ISCHR(mode) (((mode) & S_IFMT) == S_IFCHR)
#endif
#if defined(S_IFDIR) && defined(S_IFMT) && !defined(S_ISDIR)
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif
#if defined(S_IFLNK) && defined(S_IFMT) && !defined(S_ISLNK)
#define S_ISLNK(mode) (((mode) & S_IFMT) == S_IFLNK)
#endif

/*
 * SunOS 4.1.3: `man 2V open' has only one line that mentions O_NOBLOCK:
 *
 *     O_NONBLOCK     Same as O_NDELAY above.
 *
 * on the very same SunOS 4.1.3, I traced the open system call and found
 * that an open("/dev/ttyy08", O_RDWR|O_NONBLOCK|O_NOCTTY) was blocked,
 * whereas open("/dev/ttyy08", O_RDWR|O_NDELAY  |O_NOCTTY) went through.
 *
 * For this simple reason I now favour O_NDELAY. jw. 4.5.95
 */
#if defined(sun) && !defined(SVR4)
# undef O_NONBLOCK
#endif

#if !defined(O_NONBLOCK) && defined(O_NDELAY)
# define O_NONBLOCK O_NDELAY
#endif

#if !defined(FNBLOCK) && defined(FNONBLOCK)
# define FNBLOCK FNONBLOCK
#endif
#if !defined(FNBLOCK) && defined(FNDELAY)
# define FNBLOCK FNDELAY
#endif
#if !defined(FNBLOCK) && defined(O_NONBLOCK)
# define FNBLOCK O_NONBLOCK
#endif

#ifndef POSIX
#undef mkfifo
#define mkfifo(n,m) mknod(n,S_IFIFO|(m),0)
#endif

#if !defined(HAVE_LSTAT) && !defined(lstat)
# define lstat stat
#endif

/*****************************************************************
 *    signal handling
 */

#ifdef SIGVOID
# define SIGRETURN
# define sigret_t void
#else
# define SIGRETURN return 0;
# define sigret_t int
#endif

/* Geeeee, reverse it? */
#if defined(SVR4) || (defined(SYSV) && defined(ISC)) || defined(_AIX) || defined(linux) || defined(ultrix) || defined(__386BSD__) || defined(__bsdi__) || defined(POSIX) || defined(NeXT)
# define SIGHASARG
#endif

#ifdef SIGHASARG
# define SIGPROTOARG   (int)
# define SIGDEFARG     (sigsig) int sigsig;
# define SIGARG        0
#else
# define SIGPROTOARG   (void)
# define SIGDEFARG     ()
# define SIGARG
#endif

#ifndef SIGCHLD
#define SIGCHLD SIGCLD
#endif

#if defined(POSIX) || defined(hpux)
# define signal xsignal
#else
# ifdef USESIGSET
#  define signal sigset
# endif /* USESIGSET */
#endif

/* used in screen.c and attacher.c */
#ifndef NSIG		/* kbeal needs these w/o SYSV */
# define NSIG 32
#endif /* !NSIG */


/*****************************************************************
 *    Wait stuff
 */

#if (!defined(sysV68) && !defined(M_XENIX)) || defined(NeXT) || defined(M_UNIX)
# include <sys/wait.h>
#endif

#ifndef WTERMSIG
# ifndef BSDWAIT /* if wait is NOT a union: */
#  define WTERMSIG(status) (status & 0177)
# else
#  define WTERMSIG(status) status.w_T.w_Termsig 
# endif
#endif

#ifndef WSTOPSIG
# ifndef BSDWAIT /* if wait is NOT a union: */
#  define WSTOPSIG(status) ((status >> 8) & 0377)
# else
#  define WSTOPSIG(status) status.w_S.w_Stopsig 
# endif
#endif

/* NET-2 uses WCOREDUMP */
#if defined(WCOREDUMP) && !defined(WIFCORESIG)
# define WIFCORESIG(status) WCOREDUMP(status)
#endif

#ifndef WIFCORESIG
# ifndef BSDWAIT /* if wait is NOT a union: */
#  define WIFCORESIG(status) (status & 0200)
# else
#  define WIFCORESIG(status) status.w_T.w_Coredump
# endif
#endif

#ifndef WEXITSTATUS
# ifndef BSDWAIT /* if wait is NOT a union: */
#  define WEXITSTATUS(status) ((status >> 8) & 0377)
# else
#  define WEXITSTATUS(status) status.w_T.w_Retcode
# endif
#endif


/*****************************************************************
 *    select stuff
 */

#if defined(M_XENIX) || defined(M_UNIX) || defined(_SEQUENT_)
#include <sys/select.h>		/* for timeval + FD... */
#endif

/*
 * SunOS 3.5 - Tom Schmidt - Micron Semiconductor, Inc - 27-Jul-93
 * tschmidt@vax.micron.com
 */
#ifndef FD_SET
# ifndef SUNOS3
typedef struct fd_set { int fds_bits[1]; } fd_set;
# endif
# define FD_ZERO(fd) ((fd)->fds_bits[0] = 0)
# define FD_SET(b, fd) ((fd)->fds_bits[0] |= 1 << (b))
# define FD_ISSET(b, fd) ((fd)->fds_bits[0] & 1 << (b))
# define FD_SETSIZE 32
#endif


/*****************************************************************
 *    user defineable stuff
 */

#ifndef TERMCAP_BUFSIZE
# define TERMCAP_BUFSIZE 2048
#endif

#ifndef MAXPATHLEN
# define MAXPATHLEN 1024
#endif

/* 
 * you may try to vary this value. Use low values if your (VMS) system
 * tends to choke when pasting. Use high values if you want to test
 * how many characters your pty's can buffer.
 */
#define IOSIZE		4096

