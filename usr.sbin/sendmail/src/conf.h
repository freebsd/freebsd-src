/*
 * Copyright (c) 1983, 1995, 1996 Eric P. Allman
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *
 *	@(#)conf.h	8.279 (Berkeley) 12/1/96
 */

/*
**  CONF.H -- All user-configurable parameters for sendmail
**
**	Send updates to sendmail@Sendmail.ORG so they will be
**	included in the next release.
*/

#ifdef __GNUC__
struct rusage;	/* forward declaration to get gcc to shut up in wait.h */
#endif

# include <sys/param.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <sys/file.h>
# include <sys/wait.h>
# include <limits.h>
# include <fcntl.h>
# include <signal.h>
# include <netdb.h>
# include <pwd.h>

/**********************************************************************
**  Table sizes, etc....
**	There shouldn't be much need to change these....
**********************************************************************/

# define MAXLINE	2048		/* max line length */
# define MAXNAME	256		/* max length of a name */
# define MAXPV		40		/* max # of parms to mailers */
# define MAXATOM	200		/* max atoms per address */
# define MAXMAILERS	25		/* maximum mailers known to system */
# define MAXRWSETS	200		/* max # of sets of rewriting rules */
# define MAXPRIORITIES	25		/* max values for Precedence: field */
# define MAXMXHOSTS	100		/* max # of MX records for one host */
# define SMTPLINELIM	990		/* maximum SMTP line length */
# define MAXKEY		128		/* maximum size of a database key */
# define MEMCHUNKSIZE	1024		/* chunk size for memory allocation */
# define MAXUSERENVIRON	100		/* max envars saved, must be >= 3 */
# define MAXALIASDB	12		/* max # of alias databases */
# define MAXMAPSTACK	12		/* max # of stacked or sequenced maps */
# define MAXTOCLASS	8		/* max # of message timeout classes */
# define MAXMIMEARGS	20		/* max args in Content-Type: */
# define MAXMIMENESTING	20		/* max MIME multipart nesting */
# define QUEUESEGSIZE	1000		/* increment for queue size */

/**********************************************************************
**  Compilation options.
**	#define these to 1 if they are available;
**	#define them to 0 otherwise.
**  All can be overridden from Makefile.
**********************************************************************/

# ifndef NETINET
#  define NETINET	1	/* include internet support */
# endif

# ifndef NETISO
#  define NETISO	0	/* do not include ISO socket support */
# endif

# ifndef NAMED_BIND
#  define NAMED_BIND	1	/* use Berkeley Internet Domain Server */
# endif

# ifndef XDEBUG
#  define XDEBUG	1	/* enable extended debugging */
# endif

# ifndef MATCHGECOS
#  define MATCHGECOS	1	/* match user names from gecos field */
# endif

# ifndef DSN
#  define DSN		1	/* include delivery status notification code */
# endif

# if !defined(USERDB) && (defined(NEWDB) || defined(HESIOD))
#  define USERDB	1	/* look in user database */
# endif

# ifndef MIME8TO7
#  define MIME8TO7	1	/* 8->7 bit MIME conversions */
# endif

# ifndef MIME7TO8
#  define MIME7TO8	1	/* 7->8 bit MIME conversions */
# endif

/**********************************************************************
**  "Hard" compilation options.
**	#define these if they are available; comment them out otherwise.
**  These cannot be overridden from the Makefile, and should really not
**  be turned off unless absolutely necessary.
**********************************************************************/

# define LOG			/* enable logging -- don't turn off */

/**********************************************************************
**  End of site-specific configuration.
**********************************************************************/
/*
**  General "standard C" defines.
**
**	These may be undone later, to cope with systems that claim to
**	be Standard C but aren't.  Gcc is the biggest offender -- it
**	doesn't realize that the library is part of the language.
**
**	Life would be much easier if we could get rid of this sort
**	of bozo problems.
*/

#ifdef __STDC__
# define HASSETVBUF	1	/* we have setvbuf(3) in libc */
#endif

/*
**  Assume you have standard calls; can be #undefed below if necessary.
*/

# define HASLSTAT	1	/* has lstat(2) call */
/**********************************************************************
**  Operating system configuration.
**
**	Unless you are porting to a new OS, you shouldn't have to
**	change these.
**********************************************************************/

/*
**  HP-UX -- tested for 8.07, 9.00, and 9.01.
**
**	If V4FS is defined, compile for HP-UX 10.0.
*/

#ifdef __hpux
		/* common definitions for HP-UX 9.x and 10.x */
# undef m_flags		/* conflict between db.h & sys/sysmacros.h on HP 300 */
# define SYSTEM5	1	/* include all the System V defines */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASFCHMOD	1	/* has fchmod(2) syscall */
# define USESETEUID	1	/* has useable seteuid(2) call */
# define seteuid(e)	setresuid(-1, e, -1)
# define IP_SRCROUTE	1	/* can check IP source routing */
# define LA_TYPE	LA_HPUX
# define SPT_TYPE	SPT_PSTAT
# define SFS_TYPE	SFS_VFS	/* use <sys/vfs.h> statfs() implementation */
# define GIDSET_T	gid_t
# ifndef HASGETUSERSHELL
#  define HASGETUSERSHELL 0	/* getusershell(3) causes core dumps */
# endif
# define syslog		hard_syslog

# ifdef V4FS
		/* HP-UX 10.x */
#  define _PATH_UNIX		"/stand/vmunix"
#  define _PATH_VENDOR_CF	"/etc/mail/sendmail.cf"
#  ifndef _PATH_SENDMAILPID
#   define _PATH_SENDMAILPID	"/etc/mail/sendmail.pid"
#  endif
#  ifndef IDENTPROTO
#   define IDENTPROTO	1	/* TCP/IP implementation fixed in 10.0 */
#  endif

# else
		/* HP-UX 9.x */
#  define _PATH_UNIX		"/hp-ux"
#  define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
#  ifndef IDENTPROTO
#   define IDENTPROTO	0	/* TCP/IP implementation is broken */
#  endif
#  ifdef __STDC__
extern void	hard_syslog(int, char *, ...);
#  endif
# endif

#endif


/*
**  IBM AIX 4.x
*/

#ifdef _AIX4
# define _AIX3		1	/* pull in AIX3 stuff */
# define USESETEUID	1	/* seteuid(2) works */
# define TZ_TYPE	TZ_NAME	/* use tzname[] vector */
# if _AIX4 >= 40200
#  define HASSETREUID	1	/* setreuid(2) works as of AIX 4.2 */
# endif
#endif


/*
**  IBM AIX 3.x -- actually tested for 3.2.3
*/

#ifdef _AIX3
# include <paths.h>
# include <sys/machine.h>	/* to get byte order */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASUNAME	1	/* use System V uname(2) system call */
# define HASGETUSERSHELL 0	/* does not have getusershell(3) call */
# define HASFCHMOD	1	/* has fchmod(2) syscall */
# define IP_SRCROUTE	0	/* Something is broken with getsockopt() */
# define GIDSET_T	gid_t
# define SFS_TYPE	SFS_STATFS	/* use <sys/statfs.h> statfs() impl */
# define SPT_PADCHAR	'\0'	/* pad process title with nulls */
# define LA_TYPE	LA_INT
# define FSHIFT		16
# define LA_AVENRUN	"avenrun"
#endif


/*
**  IBM AIX 2.2.1 -- actually tested for osupdate level 2706+1773
**
**	From Mark Whetzel <markw@wg.waii.com>.
*/

#ifdef AIX			/* AIX/RT compiler pre-defines this */
# include <paths.h>
# include <sys/time.h>		/* AIX/RT resource.h does NOT include this */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASUNAME	1	/* use System V uname(2) system call */
# define HASGETUSERSHELL 0	/* does not have getusershell(3) call */
# define HASFCHMOD	0	/* does not have fchmod(2) syscall */
# define HASSETREUID	1	/* use setreuid(2) -lbsd system call */
# define HASSETVBUF	1	/* use setvbuf(2) system call */
# define HASSETRLIMIT	0	/* does not have setrlimit call */
# define HASFLOCK	0	/* does not have flock call - use fcntl */
# define HASULIMIT	1	/* use ulimit instead of setrlimit call */
# define NEEDGETOPT	1	/* Do we need theirs or ours */
# define SYS5SETPGRP	1	/* don't have setpgid on AIX/RT */
# define IP_SRCROUTE	0	/* Something is broken with getsockopt() */
# define BSD4_3		1	/* NOT bsd 4.4 or posix signals */
# define GIDSET_T	int
# define SFS_TYPE	SFS_STATFS	/* use <sys/statfs.h> statfs() impl */
# define SPT_PADCHAR	'\0'		/* pad process title with nulls */
# define LA_TYPE	LA_SUBR		/* use our ported loadavgd daemon */
# define TZ_TYPE	TZ_TZNAME	/* use tzname[] vector */
# define ARBPTR_T	int *
# define void		int
typedef int		pid_t;
/* RTisms for BSD compatibility, specified in the Makefile
  define BSD		1
  define BSD_INCLUDES		1
  define BSD_REMAP_SIGNAL_TO_SIGVEC
    RTisms needed above */
/* make this sendmail in a completely different place */
# define _PATH_VENDORCF		"/usr/local/newmail/sendmail.cf"
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/usr/local/newmail/sendmail.pid"
# endif
#endif


/*
**  Silicon Graphics IRIX
**
**	Compiles on 4.0.1.
**
**	Use IRIX64 instead of IRIX for 64-bit IRIX (6.0).
**	Use IRIX5 instead of IRIX for IRIX 5.x.
** 
**	This version tries to be adaptive using _MIPS_SIM:
**		_MIPS_SIM == _ABIO32 (= 1)    Abi: -32  on IRIX 6.2
**		_MIPS_SIM == _ABIN32 (= 2)    Abi: -n32 on IRIX 6.2
**		_MIPS_SIM == _ABI64  (= 3)    Abi: -64 on IRIX 6.2
**
**		_MIPS_SIM is 1 also on IRIX 5.3
**
**	IRIX64 changes from Mark R. Levinson <ml@cvdev.rochester.edu>.
**	IRIX5 changes from Kari E. Hurtta <Kari.Hurtta@fmi.fi>.
**	Adaptive changes from Kari E. Hurtta <Kari.Hurtta@fmi.fi>.
*/

#if defined(__sgi)
# ifndef IRIX
#  define IRIX
# endif
# if _MIPS_SIM > 0 && !defined(IRIX5)
#  define IRIX5			/* IRIX5 or IRIX6 */
# endif
# if _MIPS_SIM > 1 && !defined(IRIX6) && !defined(IRIX64)
#  define IRIX6			/* IRIX6 */
# endif

#endif

#ifdef IRIX
# define SYSTEM5	1	/* this is a System-V derived system */
# define HASSETREUID	1	/* has setreuid(2) call */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASFCHMOD	1	/* has fchmod(2) syscall */
# define HASGETUSERSHELL 0	/* does not have getusershell(3) call */
# define IP_SRCROUTE	1	/* can check IP source routing */
# define setpgid	BSDsetpgrp
# define GIDSET_T	gid_t
# define SFS_TYPE	SFS_4ARGS	/* four argument statfs() call */
# define SFS_BAVAIL	f_bfree		/* alternate field name */
# ifdef IRIX6
#  define LA_TYPE	LA_IRIX6	/* figure out at run time */
# else
#  define LA_TYPE	LA_INT

#  ifdef IRIX64
#   define NAMELISTMASK	0x7fffffffffffffff	/* mask for nlist() values */
#  else
#   define NAMELISTMASK	0x7fffffff		/* mask for nlist() values */
#  endif
# endif
# if defined(IRIX64) || defined(IRIX5) 
#  include <sys/cdefs.h>
#  include <paths.h>
#  define ARGV_T	char *const *
#  define HASSETRLIMIT	1	/* has setrlimit(2) syscall */
#  define HASGETDTABLESIZE 1    /* has getdtablesize(2) syscall */
# else
#  define ARGV_T	const char **
#  define WAITUNION	1	/* use "union wait" as wait argument type */
# endif
#endif


/*
**  SunOS and Solaris
**
**	Tested on SunOS 4.1.x (a.k.a. Solaris 1.1.x) and
**	Solaris 2.4 (a.k.a. SunOS 5.4).
*/

#if defined(sun) && !defined(BSD)

# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASUNAME	1	/* use System V uname(2) system call */
# define HASGETUSERSHELL 1	/* DOES have getusershell(3) call in libc */
# define HASFCHMOD	1	/* has fchmod(2) syscall */
# define IP_SRCROUTE	1	/* can check IP source routing */
# ifndef LA_TYPE
#  define LA_TYPE	LA_INT
# endif

# ifdef SOLARIS_2_3
#  define SOLARIS	20300	/* for back compat only -- use -DSOLARIS=20300 */
# endif

# if defined(NOT_SENDMAIL) && !defined(SOLARIS) && defined(sun) && (defined(__svr4__) || defined(__SVR4))
#  define SOLARIS	1	/* unknown Solaris version */
# endif

# ifdef SOLARIS
			/* Solaris 2.x (a.k.a. SunOS 5.x) */
#  ifndef __svr4__
#   define __svr4__		/* use all System V Releae 4 defines below */
#  endif
#  include <sys/time.h>
#  define GIDSET_T	gid_t
#  define USE_SA_SIGACTION	1	/* use sa_sigaction field */
#  ifndef _PATH_UNIX
#   define _PATH_UNIX		"/dev/ksyms"
#  endif
#  define _PATH_VENDOR_CF	"/etc/mail/sendmail.cf"
#  ifndef _PATH_SENDMAILPID
#   define _PATH_SENDMAILPID	"/etc/mail/sendmail.pid"
#  endif
#  ifndef _PATH_HOSTS
#   define _PATH_HOSTS		"/etc/inet/hosts"
#  endif
#  ifndef SYSLOG_BUFSIZE
#   define SYSLOG_BUFSIZE	1024	/* allow full size syslog buffer */
#  endif
#  if SOLARIS >= 20300 || (SOLARIS < 10000 && SOLARIS >= 203)
#   define USESETEUID	1		/* seteuid works as of 2.3 */
#  endif
#  if SOLARIS >= 20500 || (SOLARIS < 10000 && SOLARIS >= 205)
#   define HASSNPRINTF	1		/* has snprintf starting in 2.5 */
#   define HASSETREUID	1		/* setreuid works as of 2.5 */
#   if SOLARIS == 20500 || SOLARIS == 205
#    define snprintf	__snprintf	/* but names it oddly in 2.5 */
#    define vsnprintf	__vsnprintf
#   endif
#   ifndef LA_TYPE
#    define LA_TYPE	LA_KSTAT	/* use kstat(3k) -- may work in < 2.5 */
#   endif
#  endif
#  ifndef HASGETUSERSHELL
#   define HASGETUSERSHELL 0	/* getusershell(3) causes core dumps */
#  endif

# else
			/* SunOS 4.0.3 or 4.1.x */
#  define HASSETREUID	1	/* has setreuid(2) call */
#  ifndef HASFLOCK
#   define HASFLOCK	1	/* has flock(2) call */
#  endif
#  define SFS_TYPE	SFS_VFS	/* use <sys/vfs.h> statfs() implementation */
#  define TZ_TYPE	TZ_TM_ZONE	/* use tm->tm_zone */
#  include <vfork.h>

#  ifdef SUNOS403
			/* special tweaking for SunOS 4.0.3 */
#   include <malloc.h>
#   define BSD4_3	1	/* 4.3 BSD-based */
#   define NEEDSTRSTR	1	/* need emulation of strstr(3) routine */
#   define WAITUNION	1	/* use "union wait" as wait argument type */
#   undef WIFEXITED
#   undef WEXITSTATUS
#   undef HASUNAME
#   define setpgid	setpgrp
#   define MODE_T	int
typedef int		pid_t;
extern char		*getenv();

#  else
			/* 4.1.x specifics */
#   define HASSETSID	1	/* has Posix setsid(2) call */
#   define HASSETVBUF	1	/* we have setvbuf(3) in libc */

#  endif
# endif
#endif

/*
**  DG/UX
**
**	Tested on 5.4.2 and 5.4.3.  Use DGUX_5_4_2 to get the
**	older support.
**	5.4.3 changes from Mark T. Robinson <mtr@ornl.gov>.
*/

#ifdef DGUX_5_4_2
# define DGUX		1
#endif

#ifdef	DGUX
# define SYSTEM5	1
# define LA_TYPE	LA_DGUX
# define HASSETREUID	1	/* has setreuid(2) call */
# define HASUNAME	1	/* use System V uname(2) system call */
# define HASSETSID	1	/* has Posix setsid(2) call */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define IP_SRCROUTE	0	/* does not have <netinet/ip_var.h> */
# define HASGETUSERSHELL 0	/* does not have getusershell(3) */
# define HASSNPRINTF	1	/* has snprintf(3) */
# ifndef IDENTPROTO
#  define IDENTPROTO	0	/* TCP/IP implementation is broken */
# endif
# define SPT_TYPE	SPT_NONE	/* don't use setproctitle */
# define SFS_TYPE	SFS_4ARGS	/* four argument statfs() call */

/* these include files must be included early on DG/UX */
# include <netinet/in.h>
# include <arpa/inet.h>

/* compiler doesn't understand const? */
# define const

# ifdef DGUX_5_4_2
#  define inet_addr	dgux_inet_addr
extern long	dgux_inet_addr();
# endif
#endif


/*
**  Digital Ultrix 4.2A or 4.3
**
**	Apparently, fcntl locking is broken on 4.2A, in that locks are
**	not dropped when the process exits.  This causes major problems,
**	so flock is the only alternative.
*/

#ifdef ultrix
# define HASSETREUID	1	/* has setreuid(2) call */
# define HASUNSETENV	1	/* has unsetenv(3) call */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASUNAME	1	/* use System V uname(2) system call */
# define HASFCHMOD	1	/* has fchmod(2) syscall */
# ifndef HASFLOCK
#  define HASFLOCK	1	/* has flock(2) call */
# endif
# define HASGETUSERSHELL 0	/* does not have getusershell(3) call */
# ifndef BROKEN_RES_SEARCH
#  define BROKEN_RES_SEARCH 1	/* res_search(unknown) returns h_errno=0 */
# endif
# ifdef vax
#  define LA_TYPE	LA_FLOAT
# else
#  define LA_TYPE	LA_INT
#  define LA_AVENRUN	"avenrun"
# endif
# define SFS_TYPE	SFS_MOUNT	/* use <sys/mount.h> statfs() impl */
# ifndef IDENTPROTO
#  define IDENTPROTO	0	/* pre-4.4 TCP/IP implementation is broken */
# endif
#endif


/*
**  OSF/1 for KSR.
**
**      Contributed by Todd C. Miller <Todd.Miller@cs.colorado.edu>
*/

#ifdef __ksr__
# define __osf__	1       /* get OSF/1 defines below */
# ifndef TZ_TYPE
#  define TZ_TYPE	TZ_TZNAME	/* use tzname[] vector */
# endif
#endif


/*
**  OSF/1 for Intel Paragon.
**
**	Contributed by Jeff A. Earickson <jeff@ssd.intel.com>
**	of Intel Scalable Systems Divison.
*/

#ifdef __PARAGON__
# define __osf__	1	/* get OSF/1 defines below */
# ifndef TZ_TYPE
#  define TZ_TYPE	TZ_TZNAME	/* use tzname[] vector */
# endif
#endif


/*
**  OSF/1 (tested on Alpha) -- now known as Digital UNIX.
**
**	Tested for 3.2 and 4.0.
*/

#ifdef __osf__
# define HASUNSETENV	1	/* has unsetenv(3) call */
# define USESETEUID	1	/* has useable seteuid(2) call */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASFCHMOD	1	/* has fchmod(2) syscall */
# define IP_SRCROUTE	1	/* can check IP source routing */
# ifndef HASFLOCK
#  define HASFLOCK	1	/* has flock(2) call */
# endif
# define LA_TYPE	LA_ALPHAOSF
# define SFS_TYPE	SFS_MOUNT	/* use <sys/mount.h> statfs() impl */
# define _PATH_VENDOR_CF	"/var/adm/sendmail/sendmail.cf"
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/var/run/sendmail.pid"
# endif
#endif


/*
**  NeXTstep
*/

#ifdef NeXT
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define NEEDPUTENV	2	/* need putenv(3) call; no setenv(3) call */
# ifndef HASFLOCK
#  define HASFLOCK	1	/* has flock(2) call */
# endif
# define NEEDGETOPT	1	/* need a replacement for getopt(3) */
# define WAITUNION	1	/* use "union wait" as wait argument type */
# define UID_T		int	/* compiler gripes on uid_t */
# define GID_T		int	/* ditto for gid_t */
# define MODE_T		int	/* and mode_t */
# define sleep		sleepX
# define setpgid	setpgrp
# ifndef LA_TYPE
#  define LA_TYPE	LA_MACH
# endif
# define SFS_TYPE	SFS_VFS	/* use <sys/vfs.h> statfs() implementation */
# ifndef _POSIX_SOURCE
typedef int		pid_t;
#  undef WEXITSTATUS
#  undef WIFEXITED
# endif
# define _PATH_VENDOR_CF	"/etc/sendmail/sendmail.cf"
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/etc/sendmail/sendmail.pid"
# endif
#endif


/*
**  4.4 BSD
**
**	See also BSD defines.
*/

#if defined(BSD4_4) && !defined(__bsdi__)
# include <paths.h>
# define HASUNSETENV	1	/* has unsetenv(3) call */
# define USESETEUID	1	/* has useable seteuid(2) call */
# define HASFCHMOD	1	/* has fchmod(2) syscall */
# define HASSNPRINTF	1	/* has snprintf(3) and vsnprintf(3) */
# include <sys/cdefs.h>
# define ERRLIST_PREDEFINED	/* don't declare sys_errlist */
# define BSD4_4_SOCKADDR	/* has sa_len */
# define NETLINK	1	/* supports AF_LINK */
# ifndef LA_TYPE
#  define LA_TYPE	LA_SUBR
# endif
# define SFS_TYPE	SFS_MOUNT	/* use <sys/mount.h> statfs() impl */
# define SPT_TYPE	SPT_PSSTRINGS	/* use PS_STRINGS pointer */
#endif


/*
**  BSD/OS (was BSD/386) (all versions)
**	From Tony Sanders, BSDI
*/

#ifdef __bsdi__
# include <paths.h>
# define HASUNSETENV	1	/* has the unsetenv(3) call */
# define HASSETSID	1	/* has the setsid(2) POSIX syscall */
# define USESETEUID	1	/* has useable seteuid(2) call */
# define HASFCHMOD	1	/* has fchmod(2) syscall */
# define HASSNPRINTF	1	/* has snprintf(3) and vsnprintf(3) */
# define HASUNAME	1	/* has uname(2) syscall */
# include <sys/cdefs.h>
# define ERRLIST_PREDEFINED	/* don't declare sys_errlist */
# define BSD4_4_SOCKADDR	/* has sa_len */
# define NETLINK	1	/* supports AF_LINK */
# define SFS_TYPE	SFS_MOUNT	/* use <sys/mount.h> statfs() impl */
# ifndef LA_TYPE
#  define LA_TYPE	LA_SUBR
# endif
# define GIDSET_T	gid_t
# if defined(_BSDI_VERSION) && _BSDI_VERSION >= 199312
			/* version 1.1 or later */
#  undef SPT_TYPE
#  define SPT_TYPE	SPT_BUILTIN	/* setproctitle is in libc */
# else
			/* version 1.0 or earlier */
#  ifndef OLD_NEWDB
#   define OLD_NEWDB	1	/* old version of newdb library */
#  endif
#  define SPT_PADCHAR	'\0'	/* pad process title with nulls */
# endif
#endif



/*
**  FreeBSD / NetBSD (all architectures, all versions)
**
**  4.3BSD clone, closer to 4.4BSD	for FreeBSD 1.x and NetBSD 0.9x
**  4.4BSD-Lite based			for FreeBSD 2.x and NetBSD 1.x
**
**	See also BSD defines.
*/

#if defined(__FreeBSD__) || defined(__NetBSD__)
# include <paths.h>
# define HASUNSETENV	1	/* has unsetenv(3) call */
# define HASSETSID	1	/* has the setsid(2) POSIX syscall */
# define USESETEUID	1	/* has useable seteuid(2) call */
# define HASFCHMOD	1	/* has fchmod(2) syscall */
# define HASSNPRINTF	1	/* has snprintf(3) and vsnprintf(3) */
# define HASUNAME	1	/* has uname(2) syscall */
# define ERRLIST_PREDEFINED	/* don't declare sys_errlist */
# define BSD4_4_SOCKADDR	/* has sa_len */
# define NETLINK	1	/* supports AF_LINK */
# define GIDSET_T	gid_t
# ifndef LA_TYPE
#  define LA_TYPE	LA_SUBR
# endif
# define SFS_TYPE	SFS_MOUNT	/* use <sys/mount.h> statfs() impl */
# if defined(__NetBSD__) && (NetBSD > 199307 || NetBSD0_9 > 1)
#  undef SPT_TYPE
#  define SPT_TYPE	SPT_BUILTIN	/* setproctitle is in libc */
# endif
# if defined(__FreeBSD__)
#  undef SPT_TYPE
#  if __FreeBSD__ == 2
#   include <osreldate.h>		/* and this works */
#   if __FreeBSD_version >= 199512	/* 2.2-current right now */
#    include <libutil.h>
#    define SPT_TYPE	SPT_BUILTIN
#   endif
#  endif
#  ifndef SPT_TYPE
#   define SPT_TYPE	SPT_REUSEARGV
#   define SPT_PADCHAR	'\0'		/* pad process title with nulls */
#  endif
# endif
#endif



/*
**  Mach386
**
**	For mt Xinu's Mach386 system.
*/

#if defined(MACH) && defined(i386)
# define MACH386	1
# define HASUNSETENV	1	/* has unsetenv(3) call */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# ifndef HASFLOCK
#  define HASFLOCK	1	/* has flock(2) call */
# endif
# define NEEDGETOPT	1	/* need a replacement for getopt(3) */
# define NEEDSTRTOL	1	/* need the strtol() function */
# define setpgid	setpgrp
# ifndef LA_TYPE
#  define LA_TYPE	LA_FLOAT
# endif
# define SFS_TYPE	SFS_VFS	/* use <sys/vfs.h> statfs() implementation */
# undef HASSETVBUF		/* don't actually have setvbuf(3) */
# undef WEXITSTATUS
# undef WIFEXITED
# define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/etc/sendmail.pid"
# endif
#endif


/*
**  4.3 BSD -- this is for very old systems
**
**	Should work for mt Xinu MORE/BSD and Mips UMIPS-BSD 2.1.
**
**	You'll also have to install a new resolver library.
**	I don't guarantee that support for this environment is complete.
*/

#if defined(oldBSD43) || defined(MORE_BSD) || defined(umipsbsd)
# define NEEDVPRINTF	1	/* need a replacement for vprintf(3) */
# define NEEDGETOPT	1	/* need a replacement for getopt(3) */
# define ARBPTR_T	char *
# define setpgid	setpgrp
# ifndef LA_TYPE
#  define LA_TYPE	LA_FLOAT
# endif
# define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
# ifndef IDENTPROTO
#  define IDENTPROTO	0	/* TCP/IP implementation is broken */
# endif
# undef WEXITSTATUS
# undef WIFEXITED
typedef short		pid_t;
extern int		errno;
#endif


/*
**  SCO Unix
**
**	This includes three parts:
**
**	The first is for SCO OpenServer 5.
**	(Contributed by Keith Reynolds <keithr@sco.COM>).
**
**		SCO OpenServer 5 has a compiler version number macro,
**		which we can use to figure out what version we're on.
**		This may have to change in future releases.
**
**	The second is for SCO UNIX 3.2v4.2/Open Desktop 3.0.
**	(Contributed by Philippe Brand <phb@colombo.telesys-innov.fr>).
**
**	The third is for SCO UNIX 3.2v4.0/Open Desktop 2.0 and earlier.
*/

/* SCO OpenServer 5 */
#if _SCO_DS >= 1
# include <paths.h>
# define _SCO_unix_4_2
# define HASSNPRINTF	1	/* has snprintf(3) call */
# define HASFCHMOD	1	/* has fchmod(2) call */
# define HASSETRLIMIT	1	/* has setrlimit(2) call */
# define USESETEUID	1	/* has seteuid(2) call */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASGETDTABLESIZE 1	/* has getdtablesize(2) call */
# define RLIMIT_NEEDS_SYS_TIME_H	1
# ifndef LA_TYPE
#  define LA_TYPE	LA_DEVSHORT
# endif
# define _PATH_AVENRUN	"/dev/table/avenrun"
#endif

/* SCO UNIX 3.2v4.2/Open Desktop 3.0 */
#ifdef _SCO_unix_4_2
# define _SCO_unix_
# define HASSETREUID	1	/* has setreuid(2) call */
#endif

/* SCO UNIX 3.2v4.0 Open Desktop 2.0 and earlier */
#ifdef _SCO_unix_
# include <sys/stream.h>	/* needed for IP_SRCROUTE */
# define SYSTEM5	1	/* include all the System V defines */
# define HASGETUSERSHELL 0	/* does not have getusershell(3) call */
# define NOFTRUNCATE	0	/* has (simulated) ftruncate call */
# define MAXPATHLEN	PATHSIZE
# define SFS_TYPE	SFS_4ARGS	/* use <sys/statfs.h> 4-arg impl */
# define SFS_BAVAIL	f_bfree		/* alternate field name */
# define SPT_TYPE	SPT_SCO		/* write kernel u. area */
# define TZ_TYPE	TZ_TM_NAME	/* use tm->tm_name */
# define UID_T		uid_t
# define GID_T		gid_t
# define GIDSET_T	gid_t
# define _PATH_UNIX		"/unix"
# define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/etc/sendmail.pid"
# endif

/* stuff fixed in later releases */
# ifndef _SCO_unix_4_2
#  define SYS5SIGNALS	1	/* SysV signal semantics -- reset on each sig */
# endif

# ifndef _SCO_DS
#  define ftruncate	chsize	/* use chsize(2) to emulate ftruncate */
#  define NEEDFSYNC	1	/* needs the fsync(2) call stub */
#  define NETUNIX	0	/* no unix domain socket support */
#  define LA_TYPE	LA_SHORT
# endif

#endif


/*
**  ISC (SunSoft) Unix.
**
**	Contributed by J.J. Bailey <jjb@jagware.bcc.com>
*/

#ifdef ISC_UNIX
# include <net/errno.h>
# include <sys/stream.h>	/* needed for IP_SRCROUTE */
# include <sys/bsdtypes.h>
# define SYSTEM5	1	/* include all the System V defines */
# define SYS5SIGNALS	1	/* SysV signal semantics -- reset on each sig */
# define HASGETUSERSHELL 0	/* does not have getusershell(3) call */
# define HASSETREUID	1	/* has setreuid(2) call */
# define NEEDFSYNC	1	/* needs the fsync(2) call stub */
# define NETUNIX	0	/* no unix domain socket support */
# define MAXPATHLEN	1024
# define LA_TYPE	LA_SHORT
# define SFS_TYPE	SFS_STATFS	/* use <sys/statfs.h> statfs() impl */
# define SFS_BAVAIL	f_bfree		/* alternate field name */
# define _PATH_UNIX		"/unix"
# define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/etc/sendmail.pid"
# endif

#endif


/*
**  Altos System V (5.3.1)
**	Contributed by Tim Rice <tim@trr.metro.net>.
*/

#ifdef ALTOS_SYSTEM_V
# include <sys/stream.h>
# include <limits.h>
# define SYSTEM5	1	/* include all the System V defines */
# define SYS5SIGNALS	1	/* SysV signal semantics -- reset on each sig */
# define HASGETUSERSHELL 0	/* does not have getusershell(3) call */
# define WAITUNION	1	/* use "union wait" as wait argument type */
# define NEEDFSYNC	1	/* no fsync(2) in system library */
# define NEEDSTRSTR	1	/* need emulation of the strstr(3) call */
# define MAXPATHLEN	PATH_MAX
# define LA_TYPE	LA_SHORT
# define SFS_TYPE	SFS_STATFS	/* use <sys/statfs.h> statfs() impl */
# define SFS_BAVAIL	f_bfree		/* alternate field name */
# define TZ_TYPE	TZ_TZNAME	/* use tzname[] vector */
# define NETUNIX	0	/* no unix domain socket support */
# undef WIFEXITED
# undef WEXITSTATUS
# define strtoul	strtol	/* gcc library bogosity */

typedef unsigned short	uid_t;
typedef unsigned short	gid_t;
typedef short		pid_t;

/* some stuff that should have been in the include files */
# include <grp.h>
extern char		*malloc();
extern struct passwd	*getpwent();
extern struct passwd	*getpwnam();
extern struct passwd	*getpwuid();
extern char		*getenv();
extern struct group	*getgrgid();
extern struct group	*getgrnam();

#endif


/*
**  ConvexOS 11.0 and later
**
**	"Todd C. Miller" <millert@mroe.cs.colorado.edu> claims this
**	works on 9.1 as well.
**
**  ConvexOS 11.5 and later, should work on 11.0 as defined.
**  For pre-ConvexOOS 11.0, define NEEDGETOPT, undef IDENTPROTO
**
**	Eric Schnoebelen (eric@cirr.com) For CONVEX Computer Corp.
**		(now the CONVEX Technologies Center of Hewlett Packard)
*/

#ifdef _CONVEX_SOURCE
# define HASGETDTABLESIZE	1	/* has getdtablesize(2) */
# define HASINITGROUPS	1	/* has initgroups(3) */
# define HASUNAME	1	/* use System V uname(2) system call */
# define HASSETSID	1	/* has POSIX setsid(2) call */
# define HASUNSETENV	1	/* has unsetenv(3) */
# define HASFLOCK	1	/* has flock(2) */
# define HASSETRLIMIT	1	/* has setrlimit(2) */
# define HASSETREUID	1	/* has setreuid(2) */
# define BROKEN_RES_SEARCH	1	/* res_search(unknown) returns h_error=0 */
# define NEEDPUTENV	1	/* needs putenv (written in terms of setenv) */
# define NEEDGETOPT	0	/* need replacement for getopt(3) */
# define IP_SRCROUTE	0	/* Something is broken with getsockopt() */
# define LA_TYPE	LA_FLOAT
# define SFS_TYPE	SFS_VFS	/* use <sys/vfs.h> statfs() implementation */
# define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
# ifndef S_IREAD
#  define S_IREAD	_S_IREAD
#  define S_IWRITE	_S_IWRITE
#  define S_IEXEC	_S_IEXEC
#  define S_IFMT	_S_IFMT
#  define S_IFCHR	_S_IFCHR
#  define S_IFBLK	_S_IFBLK
# endif
# ifndef TZ_TYPE
#  define TZ_TYPE	TZ_TIMEZONE
# endif
# ifndef IDENTPROTO
#  define IDENTPROTO	1
# endif
# ifndef SHARE_V1
#  define SHARE_V1	1	/* version 1 of the fair share scheduler */
# endif
# if !defined(__GNUC__ )
#  define UID_T	int		/* GNUC gets it right, ConvexC botches */
#  define GID_T	int		/* GNUC gets it right, ConvexC botches */
# endif
# if SECUREWARE
#  define FORK	fork		/* SecureWare wants the real fork! */
# else
#  define FORK	vfork		/* the rest of the OS versions don't care */
# endif
#endif


/*
**  RISC/os 4.52
**
**	Gives a ton of warning messages, but otherwise compiles.
*/

#ifdef RISCOS

# define HASUNSETENV	1	/* has unsetenv(3) call */
# ifndef HASFLOCK
#  define HASFLOCK	1	/* has flock(2) call */
# endif
# define WAITUNION	1	/* use "union wait" as wait argument type */
# define NEEDGETOPT	1	/* need a replacement for getopt(3) */
# define NEEDPUTENV	1	/* need putenv(3) call */
# define NEEDSTRSTR	1	/* need emulation of the strstr(3) call */
# define SFS_TYPE	SFS_VFS	/* use <sys/vfs.h> statfs() implementation */
# define LA_TYPE	LA_INT
# define LA_AVENRUN	"avenrun"
# define _PATH_UNIX	"/unix"
# undef WIFEXITED

# define setpgid	setpgrp

extern int		errno;
typedef int		pid_t;
#define			SIGFUNC_DEFINED
typedef int		(*sigfunc_t)();
extern char		*getenv();
extern void		*malloc();

# include <sys/time.h>

#endif


/*
**  Linux 0.99pl10 and above...
**
**  Thanks to, in reverse order of contact:
**
**	John Kennedy <warlock@csuchico.edu>
**	Andrew Pam <avatar@aus.xanadu.com>
**	Florian La Roche <rzsfl@rz.uni-sb.de>
**	Karl London <karl@borg.demon.co.uk>
**
**  Last compiled against:	[06/10/96 @ 09:21:40 PM (Monday)]
**	sendmail 8.8-a4		named bind-4.9.4-T4B	db-1.85
**	gcc 2.7.2		libc-5.3.12		linux 2.0.0
**
**  NOTE: Override HASFLOCK as you will but, as of 1.99.6, mixed-style
** 	file locking is no longer allowed.  In particular, make sure
**	your DBM library and sendmail are both using either flock(2)
**	*or* fcntl(2) file locking, but not both.
*/

#ifdef __linux__
# define BSD		1	/* include BSD defines */
# define NEEDGETOPT	1	/* need a replacement for getopt(3) */
# define HASUNAME	1	/* use System V uname(2) system call */
# define HASUNSETENV	1	/* has unsetenv(3) call */
# ifndef HASSNPRINTF
#  define HASSNPRINTF	1	/* has snprintf(3) and vsnprintf(3) */
# endif
# define ERRLIST_PREDEFINED	/* don't declare sys_errlist */
# define GIDSET_T	gid_t	/* from <linux/types.h> */
# define HASGETUSERSHELL 0	/* getusershell(3) broken in Slackware 2.0 */
# define IP_SRCROUTE	0	/* linux <= 1.2.8 doesn't support IP_OPTIONS */
# ifndef HASFLOCK
#  include <linux/version.h>
#  if LINUX_VERSION_CODE < 66399
#   define HASFLOCK	0	/* flock(2) is broken after 0.99.13 */
#  else
#   define HASFLOCK	1	/* flock(2) fixed after 1.3.95 */
#  endif
# endif
# ifndef LA_TYPE
#  define LA_TYPE	LA_PROCSTR
# endif
# define SFS_TYPE	SFS_VFS		/* use <sys/vfs.h> statfs() impl */
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/var/run/sendmail.pid"
# endif
# define TZ_TYPE	TZ_TNAME
# include <sys/sysmacros.h>
# undef atol			/* wounded in <stdlib.h> */
#endif


/*
**  DELL SVR4 Issue 2.2, and others
**	From Kimmo Suominen <kim@grendel.lut.fi>
**
**	It's on #ifdef DELL_SVR4 because Solaris also gets __svr4__
**	defined, and the definitions conflict.
**
**	Peter Wemm <peter@perth.DIALix.oz.au> claims that the setreuid
**	trick works on DELL 2.2 (SVR4.0/386 version 4.0) and ESIX 4.0.3A
**	(SVR4.0/386 version 3.0).
*/

#ifdef DELL_SVR4
				/* no changes necessary */
				/* see general __svr4__ defines below */
#endif


/*
**  Apple A/UX 3.0
*/

#ifdef _AUX_SOURCE
# include <sys/sysmacros.h>
# define BSD			/* has BSD routines */
# define HASSETRLIMIT	0	/* ... but not setrlimit(2) */
# define BROKEN_RES_SEARCH 1	/* res_search(unknown) returns h_errno=0 */
# define HASUNAME	1	/* use System V uname(2) system call */
# define HASFCHMOD	1	/* has fchmod(2) syscall */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASSETVBUF	1	/* we have setvbuf(3) in libc */
# define SIGFUNC_DEFINED	/* sigfunc_t already defined */
# ifndef IDENTPROTO
#  define IDENTPROTO	0	/* TCP/IP implementation is broken */
# endif
# ifndef LA_TYPE
#  define LA_TYPE	LA_INT
#  define FSHIFT	16
# endif
# define LA_AVENRUN	"avenrun"
# define SFS_TYPE	SFS_VFS	/* use <sys/vfs.h> statfs() implementation */
# define TZ_TYPE	TZ_TZNAME
# ifndef _PATH_UNIX
#  define _PATH_UNIX		"/unix"		/* should be in <paths.h> */
# endif
# define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
# undef WIFEXITED
# undef WEXITSTATUS
#endif


/*
**  Encore UMAX V
**
**	Not extensively tested.
*/

#ifdef UMAXV
# define HASUNAME	1	/* use System V uname(2) system call */
# define HASSETVBUF	1	/* we have setvbuf(3) in libc */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASGETUSERSHELL 0	/* does not have getusershell(3) call */
# define SYS5SIGNALS	1	/* SysV signal semantics -- reset on each sig */
# define SYS5SETPGRP	1	/* use System V setpgrp(2) syscall */
# define SFS_TYPE	SFS_4ARGS	/* four argument statfs() call */
# define MAXPATHLEN	PATH_MAX
extern struct passwd	*getpwent(), *getpwnam(), *getpwuid();
extern struct group	*getgrent(), *getgrnam(), *getgrgid();
# undef WIFEXITED
# undef WEXITSTATUS
#endif


/*
**  Stardent Titan 3000 running TitanOS 4.2.
**
**	Must be compiled in "cc -43" mode.
**
**	From Kate Hedstrom <kate@ahab.rutgers.edu>.
**
**	Note the tweaking below after the BSD defines are set.
*/

#ifdef titan
# define setpgid	setpgrp
typedef int		pid_t;
# undef WIFEXITED
# undef WEXITSTATUS
#endif


/*
**  Sequent DYNIX 3.2.0
**
**	From Jim Davis <jdavis@cs.arizona.edu>.
*/

#ifdef sequent

# define BSD		1
# define HASUNSETENV	1
# define BSD4_3		1	/* to get signal() in conf.c */
# define WAITUNION	1
# define LA_TYPE	LA_FLOAT
# ifdef	_POSIX_VERSION
#  undef _POSIX_VERSION		/* set in <unistd.h> */
# endif
# undef HASSETVBUF		/* don't actually have setvbuf(3) */
# define setpgid	setpgrp

/* Have to redefine WIFEXITED to take an int, to work with waitfor() */
# undef	WIFEXITED
# define WIFEXITED(s)	(((union wait*)&(s))->w_stopval != WSTOPPED && \
			 ((union wait*)&(s))->w_termsig == 0)
# define WEXITSTATUS(s)	(((union wait*)&(s))->w_retcode)
typedef int		pid_t;
# define isgraph(c)	(isprint(c) && (c != ' '))

# ifndef IDENTPROTO
#  define IDENTPROTO	0	/* TCP/IP implementation is broken */
# endif

# ifndef _PATH_UNIX
#  define _PATH_UNIX		"/dynix"
# endif
# define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"

#endif


/*
**  Sequent DYNIX/ptx v2.0 (and higher)
**
**	For DYNIX/ptx v1.x, undefine HASSETREUID.
**
**	From Tim Wright <timw@sequent.com>.
**	Update from Jack Woolley <jwoolley@sctcorp.com>, 26 Dec 1995,
**		for DYNIX/ptx 4.0.2.
*/

#ifdef _SEQUENT_
# include <sys/stream.h>
# define SYSTEM5	1	/* include all the System V defines */
# define HASSETSID	1	/* has POSIX setsid(2) call */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASSETREUID	1	/* has setreuid(2) call */
# define HASGETUSERSHELL 0	/* does not have getusershell(3) call */
# define GIDSET_T	gid_t
# define LA_TYPE	LA_INT
# define SFS_TYPE	SFS_STATFS	/* use <sys/statfs.h> statfs() impl */
# define SPT_TYPE	SPT_NONE	/* don't use setproctitle */
# ifndef IDENTPROTO
#  define IDENTPROTO	0	/* TCP/IP implementation is broken */
# endif
# define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/etc/sendmail.pid"
# endif
#endif


/*
**  Cray Unicos
**
**	Ported by David L. Kensiski, Sterling Sofware <kensiski@nas.nasa.gov>
*/

#ifdef UNICOS
# define SYSTEM5	1	/* include all the System V defines */
# define SYS5SIGNALS	1	/* SysV signal semantics -- reset on each sig */
# define MAXPATHLEN	PATHSIZE
# define LA_TYPE	LA_ZERO
# define SFS_TYPE	SFS_4ARGS	/* four argument statfs() call */
# define SFS_BAVAIL	f_bfree		/* alternate field name */
#endif


/*
**  Apollo DomainOS
**
**  From Todd Martin <tmartint@tus.ssi1.com> & Don Lewis <gdonl@gv.ssi1.com>
**
**  15 Jan 1994; updated 2 Aug 1995
**
*/

#ifdef apollo
# define HASSETREUID	1	/* has setreuid(2) call */
# define HASINITGROUPS	1	/* has initgroups(2) call */
# define IP_SRCROUTE	0	/* does not have <netinet/ip_var.h> */
# define SPT_TYPE	SPT_NONE	/* don't use setproctitle */
# define LA_TYPE	LA_SUBR		/* use getloadavg.c */
# define SFS_TYPE	SFS_4ARGS	/* four argument statfs() call */
# define SFS_BAVAIL	f_bfree		/* alternate field name */
# define TZ_TYPE	TZ_TZNAME
# define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/etc/sendmail.pid"
# endif
# undef  S_IFSOCK		/* S_IFSOCK and S_IFIFO are the same */
# undef  S_IFIFO
# define S_IFIFO	0010000
# ifndef IDENTPROTO
#  define IDENTPROTO	0	/* TCP/IP implementation is broken */
# endif
# define RLIMIT_NEEDS_SYS_TIME_H	1
#endif


/*
**  UnixWare 2.x
*/

#ifdef UNIXWARE2
# define UNIXWARE	1
# define HASSNPRINTF	1	/* has snprintf(3) and vsnprintf(3) */
#endif


/*
**  UnixWare 1.1.2.
**
**	Updated by Petr Lampa <lampa@fee.vutbr.cz>.
**	From Evan Champion <evanc@spatial.synapse.org>.
*/

#ifdef UNIXWARE
# include <sys/mkdev.h>
# define SYSTEM5		1
# define HASGETUSERSHELL	0	/* does not have getusershell(3) call */
# define HASSETREUID		1
# define HASSETSID		1
# define HASINITGROUPS		1
# define GIDSET_T		gid_t
# define SLEEP_T		unsigned
# define SFS_TYPE		SFS_STATVFS
# define LA_TYPE		LA_ZERO
# undef WIFEXITED
# undef WEXITSTATUS
# define _PATH_UNIX		"/unix"
# define _PATH_VENDOR_CF	"/usr/ucblib/sendmail.cf"
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/usr/ucblib/sendmail.pid"
# endif
# define SYSLOG_BUFSIZE	128
#endif


/*
**  Intergraph CLIX 3.1
**
**	From Paul Southworth <pauls@locust.cic.net>
*/

#ifdef CLIX
# define SYSTEM5	1	/* looks like System V */
# ifndef HASGETUSERSHELL
#  define HASGETUSERSHELL 0	/* does not have getusershell(3) call */
# endif
# define DEV_BSIZE	512	/* device block size not defined */
# define GIDSET_T	gid_t
# undef LOG			/* syslog not available */
# define NEEDFSYNC	1	/* no fsync in system library */
# define GETSHORT	_getshort
#endif


/*
**  NCR MP-RAS 2.x (SysVr4) with Wollongong TCP/IP
**
**	From Kevin Darcy <kevin@tech.mis.cfc.com>.
*/

#ifdef NCR_MP_RAS2
# include <sys/sockio.h>
# define __svr4__
# define IP_SRCROUTE	0	/* Something is broken with getsockopt() */
# define SYSLOG_BUFSIZE	1024
# define SPT_TYPE  SPT_NONE
#endif


/*
**  NCR MP-RAS 3.x (SysVr4) with STREAMware TCP/IP
**
**	From Tom Moore <Tom.Moore@DaytonOH.NCR.COM>
*/

#ifdef NCR_MP_RAS3
# define __svr4__
# define SYSLOG_BUFSIZE	1024
# define SPT_TYPE  SPT_NONE
#endif


/*
**  Tandem NonStop-UX SVR4
**
**	From Rick McCarty <mccarty@mpd.tandem.com>.
*/

#ifdef NonStop_UX_BXX
# define __svr4__
#endif


/*
**  Hitachi 3050R & 3050RX Workstations running HI-UX/WE2.
**
**	Tested for 1.04 and 1.03
**	From Akihiro Hashimoto ("Hash") <hash@dominic.ipc.chiba-u.ac.jp>.
*/

#ifdef __H3050R
# define SYSTEM5	1	/* include all the System V defines */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define setreuid(r, e)	setresuid(r, e, -1)
# define LA_TYPE	LA_FLOAT
# define SFS_TYPE	SFS_VFS	/* use <sys/vfs.h> statfs() implementation */
# define HASSETVBUF	/* HI-UX has no setlinebuf */
# ifndef GIDSET_T
#  define GIDSET_T	gid_t
# endif
# ifndef _PATH_UNIX
#  define _PATH_UNIX		"/HI-UX"
# endif
# define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
# ifndef IDENTPROTO
#  define IDENTPROTO	0	/* TCP/IP implementation is broken */
# endif
# ifndef HASGETUSERSHELL
#  define HASGETUSERSHELL 0	/* getusershell(3) causes core dumps */
# endif

/* avoid m_flags conflict between db.h & sys/sysmacros.h on HIUX 3050 */
# undef m_flags

# ifdef __STDC__
extern int	syslog(int, char *, ...);
# endif

#endif


/*
**  Amdahl UTS System V 2.1.5 (SVr3-based)
**
**    From: Janet Jackson <janet@dialix.oz.au>.
*/

#ifdef _UTS
# include <sys/sysmacros.h>
# undef HASLSTAT		/* has symlinks, but they cause problems */
# define NEEDFSYNC	1	/* system fsync(2) fails on non-EFS filesys */
# define SYS5SIGNALS	1	/* System V signal semantics */
# define SYS5SETPGRP	1	/* use System V setpgrp(2) syscall */
# define HASUNAME	1	/* use System V uname(2) system call */
# define HASINITGROUPS	1	/* has initgroups(3) function */
# define HASSETVBUF	1	/* has setvbuf(3) function */
# ifndef HASGETUSERSHELL
#  define HASGETUSERSHELL 0	/* does not have getusershell(3) function */
# endif
# define GIDSET_T	gid_t	/* type of 2nd arg to getgroups(2) isn't int */
# define LA_TYPE	LA_ZERO		/* doesn't have load average */
# define SFS_TYPE	SFS_4ARGS	/* use 4-arg statfs() */
# define SFS_BAVAIL	f_bfree		/* alternate field name */
# define _PATH_UNIX		"/unix"
# define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
#endif

/*
**  Cray Computer Corporation's CSOS
**
**	From Scott Bolte <scott@craycos.com>.
*/

#ifdef _CRAYCOM
# define SYSTEM5	1	/* include all the System V defines */
# define SYS5SIGNALS	1	/* SysV signal semantics -- reset on each sig */
# define NEEDFSYNC	1	/* no fsync in system library */
# define MAXPATHLEN	PATHSIZE
# define LA_TYPE	LA_ZERO
# define SFS_TYPE	SFS_4ARGS	/* four argument statfs() call */
# define SFS_BAVAIL	f_bfree		/* alternate field name */
# define _POSIX_CHOWN_RESTRICTED	-1
extern struct group	*getgrent(), *getgrnam(), *getgrgid();
#endif


/*
**  Sony NEWS-OS 4.2.1R and 6.0.3
**
**	From Motonori NAKAMURA <motonori@cs.ritsumei.ac.jp>.
*/

#ifdef sony_news
# ifndef __svr4
			/* NEWS-OS 4.2.1R */
#  ifndef BSD
#   define BSD			/* has BSD routines */
#  endif
#  define HASUNSETENV	1	/* has unsetenv(2) call */
#  undef HASSETVBUF		/* don't actually have setvbuf(3) */
#  define WAITUNION	1	/* use "union wait" as wait argument type */
#  define LA_TYPE	LA_INT
#  define SFS_TYPE	SFS_VFS /* use <sys/vfs.h> statfs() implementation */
#  ifndef HASFLOCK
#   define HASFLOCK	1	/* has flock(2) call */
#  endif
#  define setpgid	setpgrp
#  undef WIFEXITED
#  undef WEXITSTATUS
#  define MODE_T	int	/* system include files have no mode_t */
typedef int		pid_t;
typedef int		(*sigfunc_t)();
#  define SIGFUNC_DEFINED

# else
			/* NEWS-OS 6.0.3 with /bin/cc */
#  ifndef __svr4__
#   define __svr4__		/* use all System V Releae 4 defines below */
#  endif
#  define HASSETSID	1	/* has Posix setsid(2) call */
#  define HASGETUSERSHELL 1	/* DOES have getusershell(3) call in libc */
#  define LA_TYPE	LA_READKSYM	/* use MIOC_READKSYM ioctl */
#  ifndef SPT_TYPE
#   define SPT_TYPE	SPT_SYSMIPS	/* use sysmips() (OS 6.0.2 or later) */
#  endif
#  define GIDSET_T	gid_t
#  undef WIFEXITED
#  undef WEXITSTATUS
#  ifndef SYSLOG_BUFSIZE
#   define SYSLOG_BUFSIZE	1024
#  endif
#  define _PATH_UNIX		"/stand/unix"
#  define _PATH_VENDOR_CF	"/etc/mail/sendmail.cf"
#  ifndef _PATH_SENDMAILPID
#   define _PATH_SENDMAILPID	"/etc/mail/sendmail.pid"
#  endif

# endif
#endif


/*
**  Omron LUNA/UNIOS-B 3.0, LUNA2/Mach and LUNA88K Mach
**
**	From Motonori NAKAMURA <motonori@cs.ritsumei.ac.jp>.
*/

#ifdef luna
# ifndef IDENTPROTO
#  define IDENTPROTO	0	/* TCP/IP implementation is broken */
# endif
# define HASUNSETENV	1	/* has unsetenv(2) call */
# define NEEDPUTENV	1	/* need putenv(3) call */
# define NEEDGETOPT	1	/* need a replacement for getopt(3) */
# define NEEDSTRSTR	1	/* need emulation of the strstr(3) call */
# define WAITUNION	1	/* use "union wait" as wait argument type */
# ifdef uniosb
#  include <sys/time.h>
#  define NEEDVPRINTF	1	/* need a replacement for vprintf(3) */
#  define LA_TYPE	LA_INT
#  define TZ_TYPE	TZ_TM_ZONE	/* use tm->tm_zone */
# endif
# ifdef luna2
#  define LA_TYPE	LA_SUBR
#  define TZ_TYPE	TZ_TM_ZONE	/* use tm->tm_zone */
# endif
# ifdef luna88k
#  define HASSNPRINTF	1	/* has snprintf(3) and vsnprintf(3) */
#  define LA_TYPE	LA_INT
# endif
# define SFS_TYPE	SFS_VFS /* use <sys/vfs.h> statfs() implementation */
# define setpgid	setpgrp
# undef WIFEXITED
# undef WEXITSTATUS
typedef int		pid_t;
typedef int		(*sigfunc_t)();
# define SIGFUNC_DEFINED
extern char	*getenv();
extern int	errno;
# define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
#endif

  
/*
**  NEC EWS-UX/V 4.2 (with /usr/ucb/cc)
**
**	From Motonori NAKAMURA <motonori@cs.ritsumei.ac.jp>.
*/

#if defined(nec_ews_svr4) || defined(_nec_ews_svr4)
# ifndef __svr4__
#  define __svr4__		/* use all System V Releae 4 defines below */
# endif
# define SYS5SIGNALS	1	/* SysV signal semantics -- reset on each sig */
# define HASSETSID	1	/* has Posix setsid(2) call */
# define LA_TYPE	LA_READKSYM	/* use MIOC_READSYM ioctl */
# define SFS_TYPE	SFS_USTAT	/* use System V ustat(2) syscall */
# define GIDSET_T	gid_t
# undef WIFEXITED
# undef WEXITSTATUS
# define NAMELISTMASK	0x7fffffff	/* mask for nlist() values */
# ifndef SYSLOG_BUFSIZE
#  define SYSLOG_BUFSIZE	1024	/* allow full size syslog buffer */
# endif
#endif


/*
**  Fujitsu/ICL UXP/DS (For the DS/90 Series)
**
**	From Diego R. Lopez <drlopez@cica.es>.
**	Additional changes from Fumio Moriya and Toshiaki Nomura of the
**		Fujitsu Fresoftware gruop <dsfrsoft@oai6.yk.fujitsu.co.jp>.
*/

#ifdef __uxp__
# include <arpa/nameser.h>
# include <sys/sysmacros.h>
# include <sys/mkdev.h>
# define __svr4__
# define HASGETUSERSHELL	0
# define HASFLOCK		0
# if UXPDS == 10
#  define HASSNPRINTF		0	/* no snprintf(3) or vsnprintf(3) */
# else
#  define HASSNPRINTF		1	/* has snprintf(3) and vsnprintf(3) */
# endif
# define _PATH_UNIX		"/stand/unix"
# define _PATH_VENDOR_CF	"/usr/ucblib/sendmail.cf"
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/usr/ucblib/sendmail.pid"
# endif
#endif

/*
**  Pyramid DC/OSx
**
**	From Earle Ake <akee@wpdis01.wpafb.af.mil>.
*/

#ifdef DCOSx
# define GIDSET_T	gid_t
# ifndef IDENTPROTO
#  define IDENTPROTO	0	/* TCP/IP implementation is broken */
# endif
#endif

/*
**  Concurrent Computer Corporation Maxion
**
**	From Donald R. Laster Jr. <laster@access.digex.net>.
*/

#ifdef __MAXION__

# include <sys/stream.h>
# define __svr4__		1	/* SVR4.2MP */
# define HASSETREUID		1	/* have setreuid(2) */
# define HASLSTAT		1	/* have lstat(2) */
# define HASSETRLIMIT		1	/* have setrlimit(2) */
# define HASGETDTABLESIZE	1	/* have getdtablesize(2) */
# define HASSNPRINTF		1	/* have snprintf(3) */
# define HASGETUSERSHELL	1	/* have getusershell(3) */
# define NOFTRUNCATE		1	/* do not have ftruncate(2) */
# define SLEEP_T		unsigned
# define SFS_TYPE		SFS_STATVFS
# define SFS_BAVAIL		f_bavail
# ifndef SYSLOG_BUFSIZE
#  define SYSLOG_BUFSIZE	256	/* Use 256 bytes */
# endif

# undef WUNTRACED
# undef WIFEXITED
# undef WIFSIGNALED
# undef WIFSTOPPED
# undef WEXITSTATUS
# undef WTERMSIG
# undef WSTOPSIG

#endif

/**********************************************************************
**  End of Per-Operating System defines
**********************************************************************/
/**********************************************************************
**  More general defines
**********************************************************************/

/* general BSD defines */
#ifdef BSD
# define HASGETDTABLESIZE 1	/* has getdtablesize(2) call */
# define HASSETREUID	1	/* has setreuid(2) call */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# ifndef IP_SRCROUTE
#  define IP_SRCROUTE	1	/* can check IP source routing */
# endif
# ifndef HASSETRLIMIT
#  define HASSETRLIMIT	1	/* has setrlimit(2) call */
# endif
# ifndef HASFLOCK
#  define HASFLOCK	1	/* has flock(2) call */
# endif
# ifndef TZ_TYPE
#  define TZ_TYPE	TZ_TM_ZONE	/* use tm->tm_zone variable */
# endif
#endif

/* general System V Release 4 defines */
#ifdef __svr4__
# define SYSTEM5	1
# define USESETEUID	1	/* has useable seteuid(2) call */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define BSD_COMP	1	/* get BSD ioctl calls */
# ifndef HASSETRLIMIT
#  define HASSETRLIMIT	1	/* has setrlimit(2) call */
# endif
# ifndef HASGETUSERSHELL
#  define HASGETUSERSHELL 0	/* does not have getusershell(3) call */
# endif
# ifndef HASFCHMOD
#  define HASFCHMOD	1	/* most (all?) SVr4s seem to have fchmod(2) */
# endif

# ifndef _PATH_UNIX
#  define _PATH_UNIX		"/unix"
# endif
# ifndef _PATH_VENDOR_CF
#  define _PATH_VENDOR_CF	"/usr/ucblib/sendmail.cf"
# endif
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/usr/ucblib/sendmail.pid"
# endif
# ifndef SYSLOG_BUFSIZE
#  define SYSLOG_BUFSIZE	128
# endif
# ifndef SFS_TYPE
#  define SFS_TYPE		SFS_STATVFS
# endif

/* SVr4 uses different routines for setjmp/longjmp with signal support */
# define jmp_buf		sigjmp_buf
# define setjmp(env)		sigsetjmp(env, 1)
# define longjmp(env, val)	siglongjmp(env, val)
#endif

/* general System V defines */
#ifdef SYSTEM5
# include <sys/sysmacros.h>
# define HASUNAME	1	/* use System V uname(2) system call */
# define SYS5SETPGRP	1	/* use System V setpgrp(2) syscall */
# define HASSETVBUF	1	/* we have setvbuf(3) in libc */
# ifndef HASULIMIT
#  define HASULIMIT	1	/* has the ulimit(2) syscall */
# endif
# ifndef LA_TYPE
#  ifdef MIOC_READKSYM
#   define LA_TYPE	LA_READKSYM	/* use MIOC_READKSYM ioctl */
#  else
#   define LA_TYPE	LA_INT		/* assume integer load average */
#  endif
# endif
# ifndef SFS_TYPE
#  define SFS_TYPE	SFS_USTAT	/* use System V ustat(2) syscall */
# endif
# ifndef TZ_TYPE
#  define TZ_TYPE	TZ_TZNAME	/* use tzname[] vector */
# endif
# define bcopy(s, d, l)		(memmove((d), (s), (l)))
# define bzero(d, l)		(memset((d), '\0', (l)))
# define bcmp(s, d, l)		(memcmp((s), (d), (l)))
#endif

/* general POSIX defines */
#ifdef _POSIX_VERSION
# define HASSETSID	1	/* has Posix setsid(2) call */
# define HASWAITPID	1	/* has Posix waitpid(2) call */
# if _POSIX_VERSION >= 199500 && !defined(USESETEUID)
#  define USESETEUID	1	/* has useable seteuid(2) call */
# endif
#endif
/*
**  Tweaking for systems that (for example) claim to be BSD or POSIX
**  but don't have all the standard BSD or POSIX routines (boo hiss).
*/

#ifdef titan
# undef HASINITGROUPS		/* doesn't have initgroups(3) call */
#endif

#ifdef _CRAYCOM
# undef HASSETSID		/* despite POSIX claim, doesn't have setsid */
#endif

#ifdef ISC_UNIX
# undef bcopy			/* despite SystemV claim, uses BSD bcopy */
#endif

#ifdef ALTOS_SYSTEM_V
# undef bcopy			/* despite SystemV claim, uses BSD bcopy */
# undef bzero			/* despite SystemV claim, uses BSD bzero */
# undef bcmp			/* despite SystemV claim, uses BSD bcmp */
#endif


/*
**  Due to a "feature" in some operating systems such as Ultrix 4.3 and
**  HPUX 8.0, if you receive a "No route to host" message (ICMP message
**  ICMP_UNREACH_HOST) on _any_ connection, all connections to that host
**  are closed.  Some firewalls return this error if you try to connect
**  to the IDENT port (113), so you can't receive email from these hosts
**  on these systems.  The firewall really should use a more specific
**  message such as ICMP_UNREACH_PROTOCOL or _PORT or _NET_PROHIB.  If
**  not explicitly set to zero above, default it on.
*/

#ifndef IDENTPROTO
# define IDENTPROTO	1	/* use IDENT proto (RFC 1413) */
#endif

#ifndef IP_SRCROUTE
# define IP_SRCROUTE	1	/* Detect IP source routing */
#endif

#ifndef HASGETUSERSHELL
# define HASGETUSERSHELL 1	/* libc has getusershell(3) call */
#endif

#ifndef NETUNIX
# define NETUNIX	1	/* include unix domain support */
#endif

#ifndef HASFLOCK
# define HASFLOCK	0	/* assume no flock(2) support */
#endif

#ifndef HASSETREUID
# define HASSETREUID	0	/* assume no setreuid(2) call */
#endif

#ifndef HASFCHMOD
# define HASFCHMOD	0	/* assume no fchmod(2) syscall */
#endif

#ifndef USESETEUID
# define USESETEUID	0	/* assume no seteuid(2) call or no saved ids */
#endif

#ifndef HASSETRLIMIT
# define HASSETRLIMIT	0	/* assume no setrlimit(2) support */
#endif

#ifndef HASULIMIT
# define HASULIMIT	0	/* assume no ulimit(2) support */
#endif

#ifndef OLD_NEWDB
# define OLD_NEWDB	0	/* assume newer version of newdb */
#endif

#ifndef SECUREWARE
# define SECUREWARE	0	/* assume no SecureWare C2 auditing hooks */
#endif

/*
**  If no type for argument two of getgroups call is defined, assume
**  it's an integer -- unfortunately, there seem to be several choices
**  here.
*/

#ifndef GIDSET_T
# define GIDSET_T	int
#endif

#ifndef UID_T
# define UID_T		uid_t
#endif

#ifndef GID_T
# define GID_T		gid_t
#endif

#ifndef SIZE_T
# define SIZE_T		size_t
#endif

#ifndef MODE_T
# define MODE_T		mode_t
#endif

#ifndef ARGV_T
# define ARGV_T		char **
#endif
/**********************************************************************
**  Remaining definitions should never have to be changed.  They are
**  primarily to provide back compatibility for older systems -- for
**  example, it includes some POSIX compatibility definitions
**********************************************************************/

/* System 5 compatibility */
#ifndef S_ISREG
# define S_ISREG(foo)	((foo & S_IFMT) == S_IFREG)
#endif
#ifndef S_ISDIR
# define S_ISDIR(foo)	((foo & S_IFMT) == S_IFDIR)
#endif
#if !defined(S_ISLNK) && defined(S_IFLNK)
# define S_ISLNK(foo)	((foo & S_IFMT) == S_IFLNK)
#endif
#ifndef S_IWUSR
# define S_IWUSR		0200
#endif
#ifndef S_IWGRP
# define S_IWGRP		0020
#endif
#ifndef S_IWOTH
# define S_IWOTH		0002
#endif

/*
**  Older systems don't have this error code -- it should be in
**  /usr/include/sysexits.h.
*/

# ifndef EX_CONFIG
# define EX_CONFIG	78	/* configuration error */
# endif

/* pseudo-code used in server SMTP */
# define EX_QUIT	22	/* drop out of server immediately */

/* pseudo-code used for mci_setstat */
# define EX_NOTSTICKY	-5	/* don't save persistent status */


/*
**  These are used in a few cases where we need some special
**  error codes, but where the system doesn't provide something
**  reasonable.  They are printed in errstring.
*/

#ifndef E_PSEUDOBASE
# define E_PSEUDOBASE	256
#endif

#define EOPENTIMEOUT	(E_PSEUDOBASE + 0)	/* timeout on open */
#define E_DNSBASE	(E_PSEUDOBASE + 20)	/* base for DNS h_errno */

/* type of arbitrary pointer */
#ifndef ARBPTR_T
# define ARBPTR_T	void *
#endif

#ifndef __P
# include "cdefs.h"
#endif

#if NAMED_BIND
# include <arpa/nameser.h>
# ifdef __svr4__
#  ifdef NOERROR
#   undef NOERROR		/* avoid compiler conflict with stream.h */
#  endif
# endif
# ifndef __ksr__
extern int h_errno;
# endif
#endif

/*
**  The size of an IP address -- can't use sizeof because of problems
**  on Crays, where everything is 64 bits.  This will break if/when
**  IP addresses are expanded to eight bytes.
*/

#ifndef INADDRSZ
# define INADDRSZ	4
#endif

/*
**  The size of various known types -- for reading network protocols.
**  Again, we can't use sizeof because of compiler randomness.
*/

#ifndef INT16SZ
# define INT16SZ	2
#endif
#ifndef INT32SZ
# define INT32SZ	4
#endif

/*
**  Do some required dependencies
*/

#if NETINET || NETISO
# ifndef SMTP
#  define SMTP		1	/* enable user and server SMTP */
# endif
# ifndef QUEUE
#  define QUEUE		1	/* enable queueing */
# endif
# ifndef DAEMON
#  define DAEMON	1	/* include the daemon (requires IPC & SMTP) */
# endif
#endif


/*
**  Arrange to use either varargs or stdargs
*/

# ifdef __STDC__

# include <stdarg.h>

# define VA_LOCAL_DECL	va_list ap;
# define VA_START(f)	va_start(ap, f)
# define VA_END		va_end(ap)

# else

# include <varargs.h>

# define VA_LOCAL_DECL	va_list ap;
# define VA_START(f)	va_start(ap)
# define VA_END		va_end(ap)

# endif

#ifdef HASUNAME
# include <sys/utsname.h>
# ifdef newstr
#  undef newstr
# endif
#else /* ! HASUNAME */
# define NODE_LENGTH 32
struct utsname
{
	char nodename[NODE_LENGTH+1];
};
#endif /* HASUNAME */

#if !defined(MAXHOSTNAMELEN) && !defined(_SCO_unix_) && !defined(NonStop_UX_BXX) && !defined(ALTOS_SYSTEM_V)
# define MAXHOSTNAMELEN	256
#endif

#if !defined(SIGCHLD) && defined(SIGCLD)
# define SIGCHLD	SIGCLD
#endif

#ifndef STDIN_FILENO
# define STDIN_FILENO	0
#endif

#ifndef STDOUT_FILENO
# define STDOUT_FILENO	1
#endif

#ifndef STDERR_FILENO
# define STDERR_FILENO	2
#endif

#ifndef LOCK_SH
# define LOCK_SH	0x01	/* shared lock */
# define LOCK_EX	0x02	/* exclusive lock */
# define LOCK_NB	0x04	/* non-blocking lock */
# define LOCK_UN	0x08	/* unlock */
#endif

#ifndef SEEK_SET
# define SEEK_SET	0
# define SEEK_CUR	1
# define SEEK_END	2
#endif

#ifndef SIG_ERR
# define SIG_ERR	((void (*)()) -1)
#endif

#ifndef WEXITSTATUS
# define WEXITSTATUS(st)	(((st) >> 8) & 0377)
#endif
#ifndef WIFEXITED
# define WIFEXITED(st)		(((st) & 0377) == 0)
#endif

#ifndef SIGFUNC_DEFINED
typedef void		(*sigfunc_t) __P((int));
#endif

/* size of syslog buffer */
#ifndef SYSLOG_BUFSIZE
# define SYSLOG_BUFSIZE	1024
#endif

/*
**  Size of tobuf (deliver.c)
**	Tweak this to match your syslog implementation.  It will have to
**	allow for the extra information printed.
*/

#ifndef TOBUFSIZE
# if (SYSLOG_BUFSIZE) > 768
#  define TOBUFSIZE	(SYSLOG_BUFSIZE - 512)
# else
#  define TOBUFSIZE	256
# endif
#endif

/* TOBUFSIZE must never be permitted to exceed MAXLINE - 128 */
#if TOBUFSIZE > (MAXLINE - 128)
# undef TOBUFSIZE
# define TOBUFSIZE	(MAXLINE - 128)
#endif

/*
**  Size of prescan buffer.
**	Despite comments in the _sendmail_ book, this probably should
**	not be changed; there are some hard-to-define dependencies.
*/

# define PSBUFSIZE	(MAXNAME + MAXATOM)	/* size of prescan buffer */

/* fork routine -- set above using #ifdef _osname_ or in Makefile */
# ifndef FORK
# define FORK		fork		/* function to call to fork mailer */
# endif

/*
**  Default to using scanf in readcf.
*/

#ifndef SCANF
# define SCANF		1
#endif
