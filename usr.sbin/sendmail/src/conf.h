/*
 * Copyright (c) 1983, 1995 Eric P. Allman
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
 *	@(#)conf.h	8.220 (Berkeley) 11/29/95
 */

/*
**  CONF.H -- All user-configurable parameters for sendmail
**
**	Send updates to sendmail@Sendmail.ORG so they will be
**	included in the next release.
*/

struct rusage;	/* forward declaration to get gcc to shut up in wait.h */

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
# define MAXMXHOSTS	20		/* max # of MX records */
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
# ifdef __STDC__
extern void	hard_syslog(int, char *, ...);
# endif

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
# define FORK		fork	/* no vfork primitive available */
# define GIDSET_T	gid_t
# define SFS_TYPE	SFS_STATFS	/* use <sys/statfs.h> statfs() impl */
# define SPT_PADCHAR	'\0'	/* pad process title with nulls */
# define LA_TYPE	LA_INT
# define LA_AVENRUN	"avenrun"
#endif


/*
**  Silicon Graphics IRIX
**
**	Compiles on 4.0.1.
**
**	Use IRIX64 instead of IRIX for 64-bit IRIX (6.0).
**	Use IRIX5 instead of IRIX for IRIX 5.x.
**
**	IRIX64 changes from Mark R. Levinson <ml@cvdev.rochester.edu>.
**	IRIX5 changes from Kari E. Hurtta <Kari.Hurtta@fmi.fi>.
*/

#if defined(IRIX64) || defined(IRIX5)
# define IRIX
#endif

#ifdef IRIX
# define SYSTEM5	1	/* this is a System-V derived system */
# define HASSETREUID	1	/* has setreuid(2) call */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASFCHMOD	1	/* has fchmod(2) syscall */
# define HASGETUSERSHELL 0	/* does not have getusershell(3) call */
# define IP_SRCROUTE	1	/* can check IP source routing */
# define FORK		fork	/* no vfork primitive available */
# define setpgid	BSDsetpgrp
# define GIDSET_T	gid_t
# define SFS_TYPE	SFS_4ARGS	/* four argument statfs() call */
# define SFS_BAVAIL	f_bfree		/* alternate field name */
# define LA_TYPE	LA_INT
# ifdef IRIX64
#  define NAMELISTMASK	0x7fffffffffffffff	/* mask for nlist() values */
# else
#  define NAMELISTMASK	0x7fffffff		/* mask for nlist() values */
# endif
# if defined(IRIX64) || defined(IRIX5)
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
# define LA_TYPE	LA_INT

# ifdef SOLARIS_2_3
#  define SOLARIS	203	/* for back compat only -- use -DSOLARIS=203 */
# endif

# ifdef SOLARIS
			/* Solaris 2.x (a.k.a. SunOS 5.x) */
#  ifndef __svr4__
#   define __svr4__		/* use all System V Releae 4 defines below */
#  endif
#  include <sys/time.h>
#  define GIDSET_T	gid_t
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
# define FORK		fork    /* no vfork primitive available */
# define _PATH_VENDOR_CF	"/var/adm/sendmail/sendmail.cf"
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
# define _PATH_VENDOR_CF	"/var/adm/sendmail/sendmail.cf"
#endif


/*
**  OSF/1 (tested on Alpha)
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
# define LA_TYPE	LA_INT
# define SFS_TYPE	SFS_MOUNT	/* use <sys/mount.h> statfs() impl */
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
**  BSD/386 (all versions)
**	From Tony Sanders, BSDI
*/

#ifdef __bsdi__
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
# define HASUNSETENV	1	/* has unsetenv(3) call */
# define HASSETSID	1	/* has the setsid(2) POSIX syscall */
# define USESETEUID	1	/* has useable seteuid(2) call */
# define HASFCHMOD	1	/* has fchmod(2) syscall */
# define HASSNPRINTF	1	/* has snprintf(3) and vsnprintf(3) */
# define HASUNAME	1	/* has uname(2) syscall */
# include <sys/cdefs.h>
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
#  define setreuid	__setreuid
# endif
# if defined(__FreeBSD__)
#  define SPT_TYPE	SPT_PSSTRINGS	/* use PS_STRINGS->... */
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
**	This includes two parts -- the first is for SCO Open Server 3.2v4
**	(contributed by Philippe Brand <phb@colombo.telesys-innov.fr>).
**	The second is, I believe, for an older version.
*/

#ifdef _SCO_unix_4_2
# define _SCO_unix_
# define HASSETREUID	1	/* has setreuid(2) call */
# define _PATH_UNIX		"/unix"
# define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/etc/sendmail.pid"
# endif
#endif

#ifdef _SCO_unix_
# include <sys/stream.h>	/* needed for IP_SRCROUTE */
# define SYSTEM5	1	/* include all the System V defines */
# define SYS5SIGNALS	1	/* SysV signal semantics -- reset on each sig */
# define HASGETUSERSHELL 0	/* does not have getusershell(3) call */
# define NOFTRUNCATE	0	/* does not have ftruncate(3) call */
# define NEEDFSYNC	1	/* needs the fsync(2) call stub */
# define FORK		fork
# define MAXPATHLEN	PATHSIZE
# define LA_TYPE	LA_SHORT
# define SFS_TYPE	SFS_4ARGS	/* use <sys/statfs.h> 4-arg impl */
# define SFS_BAVAIL	f_bfree		/* alternate field name */
# define SPT_TYPE	SPT_SCO		/* write kernel u. area */
# define TZ_TYPE	TZ_TM_NAME	/* use tm->tm_name */
# define NETUNIX	0	/* no unix domain socket support */
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
# define FORK		fork
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
**  Altos System V.
**	Contributed by Tim Rice <timr@crl.com>.
*/

#ifdef ALTOS_SYS_V
# define SYSTEM5	1	/* include all the System V defines */
# define SYS5SIGNALS	1	/* SysV signal semantics -- reset on each sig */
# define HASGETUSERSHELL 0	/* does not have getusershell(3) call */
# define WAITUNION	1	/* use "union wait" as wait argument type */
# define NEEDFSYNC	1	/* no fsync(2) in system library */
# define FORK		fork
# define MAXPATHLEN	PATHSIZE
# define LA_TYPE	LA_SHORT
# define SFS_TYPE	SFS_STATFS	/* use <sys/statfs.h> statfs() impl */
# define SFS_BAVAIL	f_bfree		/* alternate field name */
# define TZ_TYPE	TZ_TM_NAME	/* use tm->tm_name */
# define NETUNIX	0	/* no unix domain socket support */
# undef WIFEXITED
# undef WEXITSTATUS
# define strtoul	strtol	/* gcc library bogosity */

typedef unsigned short	uid_t;
typedef unsigned short	gid_t;
typedef short		pid_t;
#endif


/*
**  ConvexOS 11.0 and later
**
**	"Todd C. Miller" <millert@mroe.cs.colorado.edu> claims this
**	works on 9.1 as well.
*/

#ifdef _CONVEX_SOURCE
# define BSD		1	/* include all the BSD defines */
# define HASUNAME	1	/* use System V uname(2) system call */
# define HASSETSID	1	/* has POSIX setsid(2) call */
# define NEEDGETOPT	1	/* need replacement for getopt(3) */
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
# ifndef IDENTPROTO
#  define IDENTPROTO	0	/* TCP/IP implementation is broken */
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
**  Last compiled against:	[09/06/95 @ 10:20:58 AM (Wednesday)]
**	sendmail 8.7-b14	named 4.9.3-beta17	db-1.85
**	gcc 2.7.0		libc-5.2.7		linux 1.2.13
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
#  define HASFLOCK	0	/* flock(2) is broken after 0.99.13 */
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
# define FORK		fork
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
# define FORK		fork	/* no vfork(2) primitive available */
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
*/

#ifdef _SEQUENT_
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
**  NCR 3000 Series (SysVr4)
**
**	From Kevin Darcy <kevin@tech.mis.cfc.com>.
*/

#ifdef NCR3000
# include <sys/sockio.h>
# define __svr4__
# define IP_SRCROUTE	0	/* Something is broken with getsockopt() */
# undef BSD
# define LA_AVENRUN	"avenrun"
# define SYSLOG_BUFSIZE	1024
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
# define HASSIGSETMASK	0	/* does not have sigsetmask(2) function */
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
*/

#ifdef UXPDS
# define __svr4__
# define HASGETUSERSHELL	1
# define HASFLOCK		0
# define _PATH_UNIX		"/stand/unix"
# define _PATH_VENDOR_CF	"/etc/mail/sendmail.cf"
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/etc/mail/sendmail.pid"
# endif
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
#  define BSD_COMP	1	/* get BSD ioctl calls */
# ifndef HASSETRLIMIT
#  define HASSETRLIMIT	1	/* has setrlimit(2) call */
# endif
# ifndef HASGETUSERSHELL
#  define HASGETUSERSHELL 0	/* does not have getusershell(3) call */
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

#ifdef ALTOS_SYS_V
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

/* heuristic setting of HASSETSIGMASK; can override above */
#ifndef HASSIGSETMASK
# ifdef SIGVTALRM
#  define HASSETSIGMASK	1
# else
#  define HASSETSIGMASK	0
# endif
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

#ifndef SIZE_T
# define SIZE_T		size_t
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
# define SMTP		1	/* enable user and server SMTP */
# define QUEUE		1	/* enable queueing */
# define DAEMON		1	/* include the daemon (requires IPC & SMTP) */
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

#if !defined(MAXHOSTNAMELEN) && !defined(_SCO_unix_) && !defined(NonStop_UX_BXX) && !defined(ALTOS_SYS_V)
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
# define FORK		vfork		/* function to call to fork mailer */
# endif

/*
**  Default to using scanf in readcf.
*/

#ifndef SCANF
# define SCANF		1
#endif
