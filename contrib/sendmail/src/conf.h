/*
 * Copyright (c) 1998-2001 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 * Copyright (c) 1983, 1995-1997 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *
 *	$Id: conf.h,v 8.496.4.43 2001/05/20 22:29:59 gshapiro Exp $
 */

/* $FreeBSD$ */

/*
**  CONF.H -- All user-configurable parameters for sendmail
**
**	Send updates to sendmail@Sendmail.ORG so they will be
**	included in the next release.
*/

#ifndef CONF_H
#define CONF_H 1

#ifdef __GNUC__
struct rusage;	/* forward declaration to get gcc to shut up in wait.h */
#endif /* __GNUC__ */

# include <sys/param.h>
# include <sys/types.h>
# if SFIO && defined(SF_APPEND)
#  undef SF_APPEND		/* Both sfio/stdio.h and sys/stat.h define it */
# endif /* SFIO && defined(SF_APPEND) */
# include <sys/stat.h>
# ifndef __QNX__
/* in QNX this grabs bogus LOCK_* manifests */
#  include <sys/file.h>
# endif /* ! __QNX__ */
# include <sys/wait.h>
# include <limits.h>
# include <fcntl.h>
# include <signal.h>
# include <netdb.h>
# include <pwd.h>
# include <grp.h>

/* make sure TOBUFSIZ isn't larger than system limit for size of exec() args */
#ifdef ARG_MAX
# if ARG_MAX > 4096
#  define SM_ARG_MAX	4096
# else /* ARG_MAX > 4096 */
#  define SM_ARG_MAX	ARG_MAX
# endif /* ARG_MAX > 4096 */
#else /* ARG_MAX */
# define SM_ARG_MAX	4096
#endif /* ARG_MAX */

/**********************************************************************
**  Table sizes, etc....
**	There shouldn't be much need to change these....
**********************************************************************/

#define MAXLINE		2048		/* max line length */
#define MAXNAME		256		/* max length of a name */
#define MAXPV		256		/* max # of parms to mailers */
#define MAXATOM		1000		/* max atoms per address */
#define MAXRWSETS	200		/* max # of sets of rewriting rules */
#define MAXPRIORITIES	25		/* max values for Precedence: field */
#define MAXMXHOSTS	100		/* max # of MX records for one host */
#define SMTPLINELIM	990		/* maximum SMTP line length */
#define MAXKEY		128		/* maximum size of a database key */
#define MEMCHUNKSIZE	1024		/* chunk size for memory allocation */
#define MAXUSERENVIRON	100		/* max envars saved, must be >= 3 */
#define MAXALIASDB	12		/* max # of alias databases */
#define MAXMAPSTACK	12		/* max # of stacked or sequenced maps */
#if _FFR_MILTER
# define MAXFILTERS	25		/* max # of milter filters */
# define MAXFILTERMACROS	50	/* max # of macros per milter cmd */
#endif /* _FFR_MILTER */
#define MAXSMTPARGS	20		/* max # of ESMTP args for MAIL/RCPT */
#define MAXTOCLASS	8		/* max # of message timeout classes */
#define MAXRESTOTYPES	3		/* max # of resolver timeout types */
#define MAXMIMEARGS	20		/* max args in Content-Type: */
#define MAXMIMENESTING	20		/* max MIME multipart nesting */
#define QUEUESEGSIZE	1000		/* increment for queue size */
#define MAXQFNAME	21		/* max qf file name length */
#define MACBUFSIZE	4096		/* max expanded macro buffer size */
#define TOBUFSIZE	SM_ARG_MAX	/* max buffer to hold address list */
#define MAXSHORTSTR	203		/* max short string length */
#define MAXMACNAMELEN	25		/* max macro name length */
#define MAXMACROID	0377		/* max macro id number */
					/* Must match (BITMAPBITS - 1) */
#ifndef MAXHDRSLEN
# define MAXHDRSLEN	(32 * 1024)	/* max size of message headers */
#endif /* ! MAXHDRSLEN */
#define MAXDAEMONS	10		/* max number of ports to listen to */
#ifndef MAXINTERFACES
# define MAXINTERFACES	512		/* number of interfaces to probe */
#endif /* MAXINTERFACES */
#ifndef MAXSYMLINKS
# define MAXSYMLINKS	32		/* max number of symlinks in a path */
#endif /* ! MAXSYMLINKS */
#define MAXLINKPATHLEN	(MAXPATHLEN * MAXSYMLINKS) /* max link-expanded file */
#define DATA_PROGRESS_TIMEOUT	300	/* how ofter to check DATA progress */
#define ENHSCLEN	10		/* max len of enhanced status code */
#if _FFR_DYNAMIC_TOBUF
# define DEFAULT_MAX_RCPT	100	/* max number of RCPTs per envelope */
#endif /* _FFR_DYNAMIC_TOBUF */

#if SASL
# ifndef AUTH_MECHANISMS
#  if STARTTLS && _FFR_EXT_MECH
#   define AUTH_MECHANISMS	"EXTERNAL GSSAPI KERBEROS_V4 DIGEST-MD5 CRAM-MD5"
#  else /* STARTTLS && _FFR_EXT_MECH */
#   define AUTH_MECHANISMS	"GSSAPI KERBEROS_V4 DIGEST-MD5 CRAM-MD5"
#  endif /* STARTTLS && _FFR_EXT_MECH */
# endif /* ! AUTH_MECHANISMS */
#endif /* SASL */

#ifdef LDAPMAP
# define LDAPMAP_MAX_ATTR	64
# define LDAPMAP_MAX_FILTER	1024
# define LDAPMAP_MAX_PASSWD	256
#endif /* LDAPMAP */

/**********************************************************************
**  Compilation options.
**	#define these to 1 if they are available;
**	#define them to 0 otherwise.
**  All can be overridden from Makefile.
**********************************************************************/

#ifndef NETINET
# define NETINET	1	/* include internet support */
#endif /* ! NETINET */

#ifndef NETINET6
# define NETINET6	0	/* do not include IPv6 support */
#endif /* ! NETINET6 */

#ifndef NETISO
# define NETISO	0		/* do not include ISO socket support */
#endif /* ! NETISO */

#ifndef NAMED_BIND
# define NAMED_BIND	1	/* use Berkeley Internet Domain Server */
#endif /* ! NAMED_BIND */

#ifndef XDEBUG
# define XDEBUG		1	/* enable extended debugging */
#endif /* ! XDEBUG */

#ifndef MATCHGECOS
# define MATCHGECOS	1	/* match user names from gecos field */
#endif /* ! MATCHGECOS */

#ifndef DSN
# define DSN		1	/* include delivery status notification code */
#endif /* ! DSN */

#if !defined(USERDB) && (defined(NEWDB) || defined(HESIOD))
# define USERDB		1	/* look in user database */
#endif /* !defined(USERDB) && (defined(NEWDB) || defined(HESIOD)) */

#ifndef MIME8TO7
# define MIME8TO7	1	/* 8->7 bit MIME conversions */
#endif /* ! MIME8TO7 */

#ifndef MIME7TO8
# define MIME7TO8	1	/* 7->8 bit MIME conversions */
#endif /* ! MIME7TO8 */

/**********************************************************************
**  "Hard" compilation options.
**	#define these if they are available; comment them out otherwise.
**  These cannot be overridden from the Makefile, and should really not
**  be turned off unless absolutely necessary.
**********************************************************************/

#define LOG		1	/* enable logging -- don't turn off */

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
#endif /* __STDC__ */

/*
**  Assume you have standard calls; can be #undefed below if necessary.
*/

#ifndef HASLSTAT
# define HASLSTAT	1	/* has lstat(2) call */
#endif /* ! HASLSTAT */
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
**	11.x support from Richard Allen <ra@hp.is>.
*/

#ifdef __hpux
		/* common definitions for HP-UX 9.x and 10.x */
# undef m_flags		/* conflict between Berkeley DB 1.85 db.h & sys/sysmacros.h on HP 300 */
# define SYSTEM5	1	/* include all the System V defines */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASFCHMOD	1	/* has fchmod(2) syscall */
# define USESETEUID	1	/* has usable seteuid(2) call */
# define BOGUS_O_EXCL	1	/* exclusive open follows symlinks */
# define seteuid(e)	setresuid(-1, e, -1)
# define IP_SRCROUTE	1	/* can check IP source routing */
# define LA_TYPE	LA_HPUX
# define SPT_TYPE	SPT_PSTAT
# define SFS_TYPE	SFS_VFS	/* use <sys/vfs.h> statfs() implementation */
# define GIDSET_T	gid_t
# ifndef HASGETUSERSHELL
#  define HASGETUSERSHELL 0	/* getusershell(3) causes core dumps */
# endif /* ! HASGETUSERSHELL */
# ifdef HPUX11
#  define HASFCHOWN	1	/* has fchown(2) */
#  define HASSNPRINTF	1	/* has snprintf(3) */
#  ifndef BROKEN_RES_SEARCH
#   define BROKEN_RES_SEARCH 1	/* res_search(unknown) returns h_errno=0 */
#  endif /* ! BROKEN_RES_SEARCH */
# else /* HPUX11 */
#  ifndef NOT_SENDMAIL
#   define syslog	hard_syslog
#  endif /* ! NOT_SENDMAIL */
# endif /* HPUX11 */
# define SAFENFSPATHCONF 1	/* pathconf(2) pessimizes on NFS filesystems */

# ifdef V4FS
		/* HP-UX 10.x */
#  define _PATH_UNIX		"/stand/vmunix"
#  ifndef _PATH_VENDOR_CF
#   define _PATH_VENDOR_CF	"/etc/mail/sendmail.cf"
#  endif /* ! _PATH_VENDOR_CF */
#  ifndef _PATH_SENDMAILPID
#   define _PATH_SENDMAILPID	"/etc/mail/sendmail.pid"
#  endif /* ! _PATH_SENDMAILPID */
#  ifndef IDENTPROTO
#   define IDENTPROTO	1	/* TCP/IP implementation fixed in 10.0 */
#  endif /* ! IDENTPROTO */
#  include <sys/mpctl.h>	/* for mpctl() in get_num_procs_online() */
# else /* V4FS */
		/* HP-UX 9.x */
#  define _PATH_UNIX		"/hp-ux"
#  ifndef _PATH_VENDOR_CF
#   define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
#  endif /* ! _PATH_VENDOR_CF */
#  ifndef IDENTPROTO
#   define IDENTPROTO	0	/* TCP/IP implementation is broken */
#  endif /* ! IDENTPROTO */
#  ifdef __STDC__
extern void	hard_syslog(int, char *, ...);
#  else /* __STDC__ */
extern void	hard_syslog();
#  endif /* __STDC__ */
#  define FDSET_CAST	(int *)	/* cast for fd_set parameters to select */
# endif /* V4FS */

#endif /* __hpux */


/*
**  IBM AIX 4.x
*/

#ifdef _AIX4
# define _AIX3		1	/* pull in AIX3 stuff */
# define BSD4_4_SOCKADDR	/* has sa_len */
# define USESETEUID	1	/* seteuid(2) works */
# define TZ_TYPE	TZ_NAME	/* use tzname[] vector */
# define SOCKOPT_LEN_T	size_t	/* arg#5 to getsockopt */
# if _AIX4 >= 40200
#  define HASSETREUID	1	/* setreuid(2) works as of AIX 4.2 */
#  define SOCKADDR_LEN_T	size_t	/* e.g., arg#3 to accept, getsockname */
# endif /* _AIX4 >= 40200 */
# if _AIX4 >= 40300
#  define HASSNPRINTF	1		/* has snprintf starting in 4.3 */
# endif /* _AIX4 >= 40300 */
# if defined(_ILS_MACROS)	/* IBM versions aren't side-effect clean */
#  undef isascii
#  define isascii(c)		!(c & ~0177)
#  undef isdigit
#  define isdigit(__a)		(_IS(__a,_ISDIGIT))
#  undef isspace
#  define isspace(__a)		(_IS(__a,_ISSPACE))
# endif /* defined(_ILS_MACROS) */
#endif /* _AIX4 */


/*
**  IBM AIX 3.x -- actually tested for 3.2.3
*/

#ifdef _AIX3
# include <paths.h>
# include <sys/machine.h>	/* to get byte order */
# include <sys/select.h>
# define HASFCHOWN	1	/* has fchown(2) */
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
#endif /* _AIX3 */


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
# ifndef _PATH_VENDOR_CF
#  define _PATH_VENDOR_CF	"/usr/local/newmail/sendmail.cf"
# endif /* ! _PATH_VENDOR_CF */
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/usr/local/newmail/sendmail.pid"
# endif /* ! _PATH_SENDMAILPID */
#endif /* AIX */


/*
**  Silicon Graphics IRIX
**
**	Compiles on 4.0.1.
**
**	Use IRIX64 instead of IRIX for 64-bit IRIX (6.0).
**	Use IRIX5 instead of IRIX for IRIX 5.x.
**
**	This version tries to be adaptive using _MIPS_SIM:
**		_MIPS_SIM == _ABIO32 (= 1)    Abi: -32 on IRIX 6.2
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
# endif /* ! IRIX */
# if _MIPS_SIM > 0 && !defined(IRIX5)
#  define IRIX5			/* IRIX5 or IRIX6 */
# endif /* _MIPS_SIM > 0 && !defined(IRIX5) */
# if _MIPS_SIM > 1 && !defined(IRIX6) && !defined(IRIX64)
#  define IRIX6			/* IRIX6 */
# endif /* _MIPS_SIM > 1 && !defined(IRIX6) && !defined(IRIX64) */
#endif /* defined(__sgi) */

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
# define SYSLOG_BUFSIZE 512
# ifdef IRIX6
#  define STAT64	1
#  define QUAD_T	unsigned long long
#  define LA_TYPE	LA_IRIX6	/* figure out at run time */
#  define SAFENFSPATHCONF 0	/* pathconf(2) lies on NFS filesystems */
# else /* IRIX6 */
#  define LA_TYPE	LA_INT

#  ifdef IRIX64
#   define STAT64	1
#   define QUAD_T	unsigned long long
#   define NAMELISTMASK	0x7fffffffffffffff	/* mask for nlist() values */
#  else /* IRIX64 */
#   define STAT64	0
#   define NAMELISTMASK	0x7fffffff		/* mask for nlist() values */
#  endif /* IRIX64 */
# endif /* IRIX6 */
# if defined(IRIX64) || defined(IRIX5) || defined(IRIX6)
#  include <sys/cdefs.h>
#  include <paths.h>
#  define ARGV_T	char *const *
#  define HASFCHOWN	1	/* has fchown(2) */
#  define HASSETRLIMIT	1	/* has setrlimit(2) syscall */
#  define HASGETDTABLESIZE 1	/* has getdtablesize(2) syscall */
#  define HASSTRERROR	1	/* has strerror(3) */
# else /* defined(IRIX64) || defined(IRIX5) || defined(IRIX6) */
#  define ARGV_T	const char **
#  define WAITUNION	1	/* use "union wait" as wait argument type */
# endif /* defined(IRIX64) || defined(IRIX5) || defined(IRIX6) */
#endif /* IRIX */


/*
**  SunOS and Solaris
**
**	Tested on SunOS 4.1.x (a.k.a. Solaris 1.1.x) and
**	Solaris 2.4 (a.k.a. SunOS 5.4).
*/

#if defined(sun) && !defined(BSD)

# include <sys/time.h>
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASUNAME	1	/* use System V uname(2) system call */
# define HASFCHMOD	1	/* has fchmod(2) syscall */
# define IP_SRCROUTE	1	/* can check IP source routing */
# define SAFENFSPATHCONF 1	/* pathconf(2) pessimizes on NFS filesystems */
# ifndef HASFCHOWN
#  define HASFCHOWN	1	/* fchown(2) */
# endif /* ! HASFCHOWN */

# ifdef SOLARIS_2_3
#  define SOLARIS	20300	/* for back compat only -- use -DSOLARIS=20300 */
# endif /* SOLARIS_2_3 */

# if defined(NOT_SENDMAIL) && !defined(SOLARIS) && defined(sun) && (defined(__svr4__) || defined(__SVR4))
#  define SOLARIS	1	/* unknown Solaris version */
# endif /* defined(NOT_SENDMAIL) && !defined(SOLARIS) && defined(sun) && (defined(__svr4__) || defined(__SVR4)) */

# ifdef SOLARIS
			/* Solaris 2.x (a.k.a. SunOS 5.x) */
#  ifndef __svr4__
#   define __svr4__		/* use all System V Release 4 defines below */
#  endif /* ! __svr4__ */
#  define GIDSET_T	gid_t
#  define USE_SA_SIGACTION	1	/* use sa_sigaction field */
#  if _FFR_MILTER
#   define BROKEN_PTHREAD_SLEEP	1	/* sleep after pthread_create() fails */
#  endif /* _FFR_MILTER */
#  ifndef _PATH_UNIX
#   define _PATH_UNIX		"/dev/ksyms"
#  endif /* ! _PATH_UNIX */
#  ifndef _PATH_VENDOR_CF
#   define _PATH_VENDOR_CF	"/etc/mail/sendmail.cf"
#  endif /* ! _PATH_VENDOR_CF */
#  ifndef _PATH_SENDMAILPID
#   define _PATH_SENDMAILPID	"/etc/mail/sendmail.pid"
#  endif /* ! _PATH_SENDMAILPID */
#  ifndef _PATH_HOSTS
#   define _PATH_HOSTS		"/etc/inet/hosts"
#  endif /* ! _PATH_HOSTS */
#  ifndef SYSLOG_BUFSIZE
#   define SYSLOG_BUFSIZE	1024	/* allow full size syslog buffer */
#  endif /* ! SYSLOG_BUFSIZE */
#  ifndef TZ_TYPE
#   define TZ_TYPE	TZ_TZNAME
#  endif /* ! TZ_TYPE */
#  if SOLARIS >= 20300 || (SOLARIS < 10000 && SOLARIS >= 203)
#   define USESETEUID	1		/* seteuid works as of 2.3 */
#  endif /* SOLARIS >= 20300 || (SOLARIS < 10000 && SOLARIS >= 203) */
#  if SOLARIS >= 20500 || (SOLARIS < 10000 && SOLARIS >= 205)
#   define HASSETREUID	1		/* setreuid works as of 2.5 */
#   if SOLARIS < 207 || (SOLARIS > 10000 && SOLARIS < 20700)
#    ifndef LA_TYPE
#     define LA_TYPE	LA_KSTAT	/* use kstat(3k) -- may work in < 2.5 */
#    endif /* ! LA_TYPE */
#    ifndef RANDOMSHIFT		/* random() doesn't work well (sometimes) */
#     define RANDOMSHIFT	8
#    endif /* RANDOMSHIFT */
#   endif /* SOLARIS < 207 || (SOLARIS > 10000 && SOLARIS < 20700) */
#  else /* SOLARIS >= 20500 || (SOLARIS < 10000 && SOLARIS >= 205) */
#   ifndef HASRANDOM
#    define HASRANDOM	0		/* doesn't have random(3) */
#   endif /* ! HASRANDOM */
#  endif /* SOLARIS >= 20500 || (SOLARIS < 10000 && SOLARIS >= 205) */
#  if SOLARIS >= 20600 || (SOLARIS < 10000 && SOLARIS >= 206)
#   define HASSNPRINTF	1		/* has snprintf starting in 2.6 */
#  else /* SOLARIS >= 20600 || (SOLARIS < 10000 && SOLARIS >= 206) */
#   if _FFR_MILTER
#    define SM_INT32	int	/* 32bit integer */
#   endif /* _FFR_MILTER */
#  endif /* SOLARIS >= 20600 || (SOLARIS < 10000 && SOLARIS >= 206) */
#  if SOLARIS >= 20700 || (SOLARIS < 10000 && SOLARIS >= 207)
#   ifndef LA_TYPE
#    include <sys/loadavg.h>
#    if SOLARIS >= 20900 || (SOLARIS < 10000 && SOLARIS >= 209)
#     include <sys/pset.h>
#     define LA_TYPE	LA_PSET	/* pset_getloadavg(3c) appears in 2.9 */
#    else
#     define LA_TYPE	LA_SUBR	/* getloadavg(3c) appears in 2.7 */
#    endif /* SOLARIS >= 20900 || (SOLARIS < 10000 && SOLARIS >= 209) */
#   endif /* ! LA_TYPE */
#   define HASGETUSERSHELL 1	/* getusershell(3c) bug fixed in 2.7 */
#  endif /* SOLARIS >= 20700 || (SOLARIS < 10000 && SOLARIS >= 207) */
#  if SOLARIS >= 20800 || (SOLARIS < 10000 && SOLARIS >= 208)
#   define HASSTRL	1	/* str*(3) added in 2.8 */
#   undef _PATH_SENDMAILPID	/* tmpfs /var/run added in 2.8 */
#   define _PATH_SENDMAILPID	"/var/run/sendmail.pid"
#  endif /* SOLARIS >= 20800 || (SOLARIS < 10000 && SOLARIS >= 208) */
#  if SOLARIS >= 20900 || (SOLARIS < 10000 && SOLARIS >= 209)
#   define HASURANDOMDEV	1	/* /dev/[u]random added in S9 */
#  endif /* SOLARIS >= 20900 || (SOLARIS < 10000 && SOLARIS >= 209) */
#  ifndef HASGETUSERSHELL
#   define HASGETUSERSHELL 0	/* getusershell(3) causes core dumps pre-2.7 */
#  endif /* ! HASGETUSERSHELL */

# else /* SOLARIS */
			/* SunOS 4.0.3 or 4.1.x */
#  define HASGETUSERSHELL 1	/* DOES have getusershell(3) call in libc */
#  define HASSETREUID	1	/* has setreuid(2) call */
#  ifndef HASFLOCK
#   define HASFLOCK	1	/* has flock(2) call */
#  endif /* ! HASFLOCK */
#  define SFS_TYPE	SFS_VFS	/* use <sys/vfs.h> statfs() implementation */
#  define TZ_TYPE	TZ_TM_ZONE	/* use tm->tm_zone */
#  include <memory.h>
#  include <vfork.h>
#  ifdef __GNUC__
#   define strtoul	strtol	/* gcc library bogosity */
#  endif /* __GNUC__ */
#  define memmove(d, s, l)	(bcopy((s), (d), (l)))

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

#  else /* SUNOS403 */
			/* 4.1.x specifics */
#   define HASSETSID	1	/* has Posix setsid(2) call */
#   define HASSETVBUF	1	/* we have setvbuf(3) in libc */

#  endif /* SUNOS403 */
# endif /* SOLARIS */

# ifndef LA_TYPE
#  define LA_TYPE	LA_INT
# endif /* ! LA_TYPE */

#endif /* defined(sun) && !defined(BSD) */

/*
**  DG/UX
**
**	Tested on 5.4.2 and 5.4.3.  Use DGUX_5_4_2 to get the
**	older support.
**	5.4.3 changes from Mark T. Robinson <mtr@ornl.gov>.
*/

#ifdef DGUX_5_4_2
# define DGUX		1
#endif /* DGUX_5_4_2 */

#ifdef DGUX
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
# endif /* ! IDENTPROTO */
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
# endif /* DGUX_5_4_2 */
#endif /* DGUX */


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
# define HASFCHOWN	1	/* has fchown(2) syscall */
# ifndef HASFLOCK
#  define HASFLOCK	1	/* has flock(2) call */
# endif /* ! HASFLOCK */
# define HASGETUSERSHELL 0	/* does not have getusershell(3) call */
# ifndef BROKEN_RES_SEARCH
#  define BROKEN_RES_SEARCH 1	/* res_search(unknown) returns h_errno=0 */
# endif /* ! BROKEN_RES_SEARCH */
# if !defined(NEEDLOCAL_HOSTNAME_LENGTH) && NAMED_BIND && __RES >= 19931104 && __RES < 19950621
#  define NEEDLOCAL_HOSTNAME_LENGTH	1	/* see sendmail/README */
# endif /* !defined(NEEDLOCAL_HOSTNAME_LENGTH) && NAMED_BIND && __RES >= 19931104 && __RES < 19950621 */
# ifdef vax
#  define LA_TYPE	LA_FLOAT
# else /* vax */
#  define LA_TYPE	LA_INT
#  define LA_AVENRUN	"avenrun"
# endif /* vax */
# define SFS_TYPE	SFS_MOUNT	/* use <sys/mount.h> statfs() impl */
# ifndef IDENTPROTO
#  define IDENTPROTO	0	/* pre-4.4 TCP/IP implementation is broken */
# endif /* ! IDENTPROTO */
# define SYSLOG_BUFSIZE	256
#endif /* ultrix */


/*
**  OSF/1 for KSR.
**
**	Contributed by Todd C. Miller <Todd.Miller@cs.colorado.edu>
*/

#ifdef __ksr__
# define __osf__	1	/* get OSF/1 defines below */
# ifndef TZ_TYPE
#  define TZ_TYPE	TZ_TZNAME	/* use tzname[] vector */
# endif /* ! TZ_TYPE */
#endif /* __ksr__ */


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
# endif /* ! TZ_TYPE */
# define GIDSET_T	gid_t
# define MAXNAMLEN	NAME_MAX
#endif /* __PARAGON__ */


/*
**  Tru64 UNIX, formerly known as Digital UNIX, formerly known as DEC OSF/1
**
**	Tested for 3.2 and 4.0.
*/

#ifdef __osf__
# define HASUNAME	1	/* has uname(2) call */
# define HASUNSETENV	1	/* has unsetenv(3) call */
# define USESETEUID	1	/* has usable seteuid(2) call */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASFCHMOD	1	/* has fchmod(2) syscall */
# define HASFCHOWN	1	/* has fchown(2) syscall */
# define HASSETLOGIN	1	/* has setlogin(2) */
# define IP_SRCROUTE	1	/* can check IP source routing */
# define HAS_ST_GEN	1	/* has st_gen field in stat struct */
# define GIDSET_T	gid_t
# if _FFR_MILTER
#  define SM_INT32	int	/* 32bit integer */
# endif /* _FFR_MILTER */
# ifndef HASFLOCK
#  define HASFLOCK	1	/* has flock(2) call */
# endif /* ! HASFLOCK */
# define LA_TYPE	LA_ALPHAOSF
# define SFS_TYPE	SFS_STATVFS	/* use <sys/statvfs.h> statfs() impl */
# ifndef _PATH_VENDOR_CF
#  define _PATH_VENDOR_CF	"/var/adm/sendmail/sendmail.cf"
# endif /* ! _PATH_VENDOR_CF */
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/var/run/sendmail.pid"
# endif /* ! _PATH_SENDMAILPID */
#endif /* __osf__ */


/*
**  NeXTstep
*/

#ifdef NeXT
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define NEEDPUTENV	2	/* need putenv(3) call; no setenv(3) call */
# ifndef HASFLOCK
#  define HASFLOCK	1	/* has flock(2) call */
# endif /* ! HASFLOCK */
# define NEEDGETOPT	1	/* need a replacement for getopt(3) */
# define WAITUNION	1	/* use "union wait" as wait argument type */
# define UID_T		int	/* compiler gripes on uid_t */
# define GID_T		int	/* ditto for gid_t */
# define MODE_T		int	/* and mode_t */
# define setpgid	setpgrp
# ifndef NOT_SENDMAIL
#  define sleep		sleepX
# endif /* ! NOT_SENDMAIL */
# ifndef LA_TYPE
#  define LA_TYPE	LA_MACH
# endif /* ! LA_TYPE */
# define SFS_TYPE	SFS_VFS	/* use <sys/vfs.h> statfs() implementation */
# ifndef _POSIX_SOURCE
typedef int		pid_t;
#  undef WEXITSTATUS
#  undef WIFEXITED
#  undef WIFSTOPPED
#  undef WTERMSIG
# endif /* ! _POSIX_SOURCE */
# ifndef _PATH_VENDOR_CF
#  define _PATH_VENDOR_CF	"/etc/sendmail/sendmail.cf"
# endif /* ! _PATH_VENDOR_CF */
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/etc/sendmail/sendmail.pid"
# endif /* ! _PATH_SENDMAILPID */

# ifdef TCPWRAPPERS
#  ifndef HASUNSETENV
#   define HASUNSETENV	1
#  endif /* ! HASUNSETENV */
#  undef NEEDPUTENV
# endif /* TCPWRAPPERS */

#endif /* NeXT */

/*
**  Apple Rhapsody
**	Contributed by Wilfredo Sanchez <wsanchez@apple.com>
**
**	Also used for Apple Darwin support.
*/

#if defined(DARWIN)
# define HASFCHMOD	1	/* has fchmod(2) syscall */
# define HASFLOCK	1	/* has flock(2) syscall */
# define HASUNAME	1	/* has uname(2) syscall */
# define HASUNSETENV	1
# define HASSETSID	1	/* has the setsid(2) POSIX syscall */
# define HASINITGROUPS	1
# define HASSETVBUF	1
# define HASSETREUID	1
# define USESETEUID	1	/* has usable seteuid(2) call */
# define HASLSTAT	1
# define HASSETRLIMIT	1
# define HASWAITPID	1
# define HASSTRERROR	1	/* has strerror(3) */
# define HASSNPRINTF	1	/* has snprintf(3) and vsnprintf(3) */
# define HASSTRERROR	1	/* has strerror(3) */
# define HASGETDTABLESIZE	1
# define HASGETUSERSHELL	1
# define NEEDGETOPT	1	/* need a replacement for getopt(3) */
# define BSD4_4_SOCKADDR	/* has sa_len */
# define NETLINK	1	/* supports AF_LINK */
# define HAS_ST_GEN	1	/* has st_gen field in stat struct */
# define GIDSET_T	gid_t
# define LA_TYPE	LA_SUBR		/* use getloadavg(3) */
# define SFS_TYPE	SFS_MOUNT	/* use <sys/mount.h> statfs() impl */
# define SPT_TYPE	SPT_PSSTRINGS
# define SPT_PADCHAR	'\0'	/* pad process title with nulls */
# define ERRLIST_PREDEFINED	/* don't declare sys_errlist */
#endif /* DARWIN */


/*
**  4.4 BSD
**
**	See also BSD defines.
*/

#if defined(BSD4_4) && !defined(__bsdi__) && !defined(__GNU__)
# include <paths.h>
# define HASUNSETENV	1	/* has unsetenv(3) call */
# define USESETEUID	1	/* has usable seteuid(2) call */
# define HASFCHMOD	1	/* has fchmod(2) syscall */
# define HASSNPRINTF	1	/* has snprintf(3) and vsnprintf(3) */
# define HASSTRERROR	1	/* has strerror(3) */
# define HAS_ST_GEN	1	/* has st_gen field in stat struct */
# include <sys/cdefs.h>
# define ERRLIST_PREDEFINED	/* don't declare sys_errlist */
# define BSD4_4_SOCKADDR	/* has sa_len */
# define NEED_PRINTF_PERCENTQ	1	/* doesn't have %lld */
# define NETLINK	1	/* supports AF_LINK */
# ifndef LA_TYPE
#  define LA_TYPE	LA_SUBR
# endif /* ! LA_TYPE */
# define SFS_TYPE	SFS_MOUNT	/* use <sys/mount.h> statfs() impl */
# define SPT_TYPE	SPT_PSSTRINGS	/* use PS_STRINGS pointer */
#endif /* defined(BSD4_4) && !defined(__bsdi__) && !defined(__GNU__) */


/*
**  BSD/OS (was BSD/386) (all versions)
**	From Tony Sanders, BSDI
*/

#ifdef __bsdi__
# include <paths.h>
# define HASUNSETENV	1	/* has the unsetenv(3) call */
# define HASSETSID	1	/* has the setsid(2) POSIX syscall */
# define USESETEUID	1	/* has usable seteuid(2) call */
# define HASFCHMOD	1	/* has fchmod(2) syscall */
# define HASSETLOGIN	1	/* has setlogin(2) */
# define HASSNPRINTF	1	/* has snprintf(3) and vsnprintf(3) */
# define HASUNAME	1	/* has uname(2) syscall */
# define HASSTRERROR	1	/* has strerror(3) */
# define HAS_ST_GEN	1	/* has st_gen field in stat struct */
# include <sys/cdefs.h>
# define ERRLIST_PREDEFINED	/* don't declare sys_errlist */
# define BSD4_4_SOCKADDR	/* has sa_len */
# define NETLINK	1	/* supports AF_LINK */
# define SFS_TYPE	SFS_MOUNT	/* use <sys/mount.h> statfs() impl */
# ifndef LA_TYPE
#  define LA_TYPE	LA_SUBR
# endif /* ! LA_TYPE */
# define GIDSET_T	gid_t
# define QUAD_T		quad_t
# if defined(_BSDI_VERSION) && _BSDI_VERSION >= 199312
			/* version 1.1 or later */
#  undef SPT_TYPE
#  define SPT_TYPE	SPT_BUILTIN	/* setproctitle is in libc */
# else /* defined(_BSDI_VERSION) && _BSDI_VERSION >= 199312 */
			/* version 1.0 or earlier */
#  define SPT_PADCHAR	'\0'	/* pad process title with nulls */
# endif /* defined(_BSDI_VERSION) && _BSDI_VERSION >= 199312 */
# if defined(_BSDI_VERSION) && _BSDI_VERSION >= 199701	/* on 3.x */
#  define HASSETUSERCONTEXT 1	/* has setusercontext */
# endif /* defined(_BSDI_VERSION) && _BSDI_VERSION >= 199701 */
#endif /* __bsdi__ */


/*
**  QNX 4.2x
**	Contributed by Glen McCready <glen@qnx.com>.
**
**	Should work with all versions of QNX.
*/

#if defined(__QNX__)
# include <unix.h>
# include <sys/select.h>
# undef NGROUPS_MAX
# define HASSETSID	1	/* has the setsid(2) POSIX syscall */
# define USESETEUID	1	/* has usable seteuid(2) call */
# define HASFCHMOD	1	/* has fchmod(2) syscall */
# define HASGETDTABLESIZE 1	/* has getdtablesize(2) call */
# define HASSETREUID	1	/* has setreuid(2) call */
# define HASSTRERROR	1	/* has strerror(3) */
# define HASFLOCK	0
# undef HASINITGROUPS		/* has initgroups(3) call */
# define NEEDGETOPT	1	/* use sendmail's getopt */
# define IP_SRCROUTE	1	/* can check IP source routing */
# define TZ_TYPE	TZ_TMNAME	/* use tmname variable */
# define GIDSET_T	gid_t
# define LA_TYPE	LA_ZERO
# define SFS_TYPE	SFS_NONE
# define SPT_TYPE	SPT_REUSEARGV
# define SPT_PADCHAR	'\0'	/* pad process title with nulls */
# define HASGETUSERSHELL 0
# define E_PSEUDOBASE	512
# define _FILE_H_INCLUDED
#endif /* defined(__QNX__) */


/*
**  FreeBSD / NetBSD / OpenBSD (all architectures, all versions)
**
**  4.3BSD clone, closer to 4.4BSD	for FreeBSD 1.x and NetBSD 0.9x
**  4.4BSD-Lite based			for FreeBSD 2.x and NetBSD 1.x
**
**	See also BSD defines.
*/

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
# include <paths.h>
# define HASUNSETENV	1	/* has unsetenv(3) call */
# define HASSETSID	1	/* has the setsid(2) POSIX syscall */
# define USESETEUID	1	/* has usable seteuid(2) call */
# define HASFCHMOD	1	/* has fchmod(2) syscall */
# define HASFCHOWN	1	/* fchown(2) */
# define HASSNPRINTF	1	/* has snprintf(3) and vsnprintf(3) */
# define HASUNAME	1	/* has uname(2) syscall */
# define HASSTRERROR	1	/* has strerror(3) */
# define HAS_ST_GEN	1	/* has st_gen field in stat struct */
# define NEED_PRINTF_PERCENTQ	1	/* doesn't have %lld */
# include <sys/cdefs.h>
# define ERRLIST_PREDEFINED	/* don't declare sys_errlist */
# define BSD4_4_SOCKADDR	/* has sa_len */
# define NETLINK	1	/* supports AF_LINK */
# define SAFENFSPATHCONF 1	/* pathconf(2) pessimizes on NFS filesystems */
# define GIDSET_T	gid_t
# define QUAD_T		unsigned long long
# define SFIO_STDIO_COMPAT	1	/* can use RES_DEBUG */
# ifndef LA_TYPE
#  define LA_TYPE	LA_SUBR
# endif /* ! LA_TYPE */
# define SFS_TYPE	SFS_MOUNT	/* use <sys/mount.h> statfs() impl */
# if defined(__NetBSD__) && (NetBSD > 199307 || NetBSD0_9 > 1)
#  undef SPT_TYPE
#  define SPT_TYPE	SPT_BUILTIN	/* setproctitle is in libc */
# endif /* defined(__NetBSD__) && (NetBSD > 199307 || NetBSD0_9 > 1) */
# if defined(__NetBSD__) && ((__NetBSD_Version__ > 102070000) || (NetBSD1_2 > 8) || defined(NetBSD1_4) || defined(NetBSD1_3))
#   define HASURANDOMDEV	1	/* has /dev/urandom(4) */
# endif /* defined(__NetBSD__) && ((__NetBSD_Version__ > 102070000) || (NetBSD1_2 > 8) || defined(NetBSD1_4) || defined(NetBSD1_3)) */
# if defined(__FreeBSD__)
#  define HASSETLOGIN	1	/* has setlogin(2) */
#  if __FreeBSD_version >= 227001
#   define HASSRANDOMDEV	1	/* has srandomdev(3) */
#   define HASURANDOMDEV	1	/* has /dev/urandom(4) */
#  endif /* __FreeBSD_version >= 227001 */
#  undef SPT_TYPE
#  if __FreeBSD__ >= 2
#   include <osreldate.h>
#   if __FreeBSD_version >= 199512	/* 2.2-current when it appeared */
#    include <libutil.h>
#    define SPT_TYPE	SPT_BUILTIN
#   endif /* __FreeBSD_version >= 199512 */
#   if __FreeBSD_version >= 222000	/* 2.2.2-release and later */
#    define HASSETUSERCONTEXT	1	/* BSDI-style login classes */
#   endif /* __FreeBSD_version >= 222000 */
#   if __FreeBSD_version >= 330000	/* 3.3.0-release and later */
#    ifndef HASSTRL
#     define HASSTRL		1	/* has strlc{py,at}(3) functions */
#    endif /* HASSTRL */
#   endif /* __FreeBSD_version >= 330000 */
#   define USESYSCTL		1	/* use sysctl(3) for getting ncpus */
#   include <sys/sysctl.h>
#  endif /* __FreeBSD__ >= 2 */
#  ifndef SPT_TYPE
#   define SPT_TYPE	SPT_REUSEARGV
#   define SPT_PADCHAR	'\0'		/* pad process title with nulls */
#  endif /* ! SPT_TYPE */
# endif /* defined(__FreeBSD__) */
# if defined(__OpenBSD__)
#  undef SPT_TYPE
#  define SPT_TYPE	SPT_BUILTIN	/* setproctitle is in libc */
#  define HASSETLOGIN	1	/* has setlogin(2) */
#  define HASSETREUID	0	/* OpenBSD has broken setreuid(2) emulation */
#  define HASURANDOMDEV	1	/* has /dev/urandom(4) */
#  if OpenBSD < 199912
#   define HASSTRL	0	/* strlcat(3) is broken in 2.5 and earlier */
#  else /* OpenBSD < 199912 */
#   define HASSTRL	1	/* has strlc{py,at}(3) functions */
#   if OpenBSD >= 200006
#    define HASSRANDOMDEV	1	/* has srandomdev(3) */
#   endif
#   if OpenBSD >= 200012
#    define HASSETUSERCONTEXT	1	/* BSDI-style login classes */
#   endif
#  endif /* OpenBSD < 199912 */
# endif /* defined(__OpenBSD__) */
#endif /* defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) */


/*
**  Mach386
**
**	For mt Xinu's Mach386 system.
*/

#if defined(MACH) && defined(i386) && !defined(__GNU__)
# define MACH386	1
# define HASUNSETENV	1	/* has unsetenv(3) call */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# ifndef HASFLOCK
#  define HASFLOCK	1	/* has flock(2) call */
# endif /* ! HASFLOCK */
# define NEEDGETOPT	1	/* need a replacement for getopt(3) */
# define NEEDSTRTOL	1	/* need the strtol() function */
# define setpgid	setpgrp
# ifndef LA_TYPE
#  define LA_TYPE	LA_FLOAT
# endif /* ! LA_TYPE */
# define SFS_TYPE	SFS_VFS	/* use <sys/vfs.h> statfs() implementation */
# undef HASSETVBUF		/* don't actually have setvbuf(3) */
# undef WEXITSTATUS
# undef WIFEXITED
# ifndef _PATH_VENDOR_CF
#  define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
# endif /* ! _PATH_VENDOR_CF */
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/etc/sendmail.pid"
# endif /* ! _PATH_SENDMAILPID */
#endif /* defined(MACH) && defined(i386) && !defined(__GNU__) */



/*
**  GNU OS (hurd)
**	Largely BSD & posix compatible.
**	Port contributed by Miles Bader <miles@gnu.ai.mit.edu>.
**	Updated by Mark Kettenis <kettenis@wins.uva.nl>.
*/

#if defined(__GNU__) && !defined(NeXT)
# include <paths.h>
# define HASFCHMOD	1	/* has fchmod(2) call */
# define HASFCHOWN	1	/* has fchown(2) call */
# define HASUNAME	1	/* has uname(2) call */
# define HASUNSETENV	1	/* has unsetenv(3) call */
# define HAS_ST_GEN	1	/* has st_gen field in stat struct */
# define HASSNPRINTF	1	/* has snprintf(3) and vsnprintf(3) */
# define HASSTRERROR	1	/* has strerror(3) */
# define GIDSET_T	gid_t
# define SOCKADDR_LEN_T	socklen_t
# define SOCKOPT_LEN_T	socklen_t
# if (__GLIBC__ == 2 && __GLIBC_MINOR__ > 1) || __GLIBC__ > 2
#  define LA_TYPE	LA_SUBR
# else
#  define LA_TYPE	LA_MACH
   /* GNU uses mach[34], which renames some rpcs from mach2.x. */
#  define host_self	mach_host_self
# endif
# define SFS_TYPE	SFS_STATFS
# define SPT_TYPE	SPT_CHANGEARGV
# define ERRLIST_PREDEFINED	1	/* don't declare sys_errlist */
# define BSD4_4_SOCKADDR	1	/* has sa_len */
# define SIOCGIFCONF_IS_BROKEN  1	/* SIOCGFCONF doesn't work */
# define HAS_IN_H	1	/* GNU has netinet/in.h. */
/* GNU has no MAXPATHLEN; ideally the code should be changed to not use it. */
# define MAXPATHLEN	2048
#endif /* defined(__GNU__) && !defined(NeXT) */

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
# endif /* ! LA_TYPE */
# ifndef _PATH_VENDOR_CF
#  define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
# endif /* ! _PATH_VENDOR_CF */
# ifndef IDENTPROTO
#  define IDENTPROTO	0	/* TCP/IP implementation is broken */
# endif /* ! IDENTPROTO */
# undef WEXITSTATUS
# undef WIFEXITED
typedef short		pid_t;
#endif /* defined(oldBSD43) || defined(MORE_BSD) || defined(umipsbsd) */


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
# define SIOCGIFNUM_IS_BROKEN 1	/* SIOCGIFNUM returns bogus value */
# define HASSNPRINTF	1	/* has snprintf(3) call */
# define HASFCHMOD	1	/* has fchmod(2) call */
# define HASFCHOWN	1	/* has fchown(2) call */
# define HASSETRLIMIT	1	/* has setrlimit(2) call */
# define USESETEUID	1	/* has seteuid(2) call */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASGETDTABLESIZE 1	/* has getdtablesize(2) call */
# define RLIMIT_NEEDS_SYS_TIME_H	1
# ifndef LA_TYPE
#  define LA_TYPE	LA_DEVSHORT
# endif /* ! LA_TYPE */
# define _PATH_AVENRUN	"/dev/table/avenrun"
# ifndef _SCO_unix_4_2
#  define _SCO_unix_4_2
# else /* ! _SCO_unix_4_2 */
#  define SOCKADDR_LEN_T	size_t	/* e.g., arg#3 to accept, getsockname */
#  define SOCKOPT_LEN_T		size_t	/* arg#5 to getsockopt */
# endif /* ! _SCO_unix_4_2 */
#endif /* _SCO_DS >= 1 */

/* SCO UNIX 3.2v4.2/Open Desktop 3.0 */
#ifdef _SCO_unix_4_2
# define _SCO_unix_
# define HASSETREUID	1	/* has setreuid(2) call */
#endif /* _SCO_unix_4_2 */

/* SCO UNIX 3.2v4.0 Open Desktop 2.0 and earlier */
#ifdef _SCO_unix_
# include <sys/stream.h>	/* needed for IP_SRCROUTE */
# define SYSTEM5	1	/* include all the System V defines */
# define HASGETUSERSHELL 0	/* does not have getusershell(3) call */
# define NOFTRUNCATE	0	/* has (simulated) ftruncate call */
# define USE_SIGLONGJMP	1	/* sigsetjmp needed for signal handling */
# define MAXPATHLEN	PATHSIZE
# define SFS_TYPE	SFS_4ARGS	/* use <sys/statfs.h> 4-arg impl */
# define SFS_BAVAIL	f_bfree		/* alternate field name */
# define SPT_TYPE	SPT_SCO		/* write kernel u. area */
# define TZ_TYPE	TZ_TM_NAME	/* use tm->tm_name */
# define UID_T		uid_t
# define GID_T		gid_t
# define GIDSET_T	gid_t
# define _PATH_UNIX		"/unix"
# ifndef _PATH_VENDOR_CF
#  define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
# endif /* ! _PATH_VENDOR_CF */
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/etc/sendmail.pid"
# endif /* ! _PATH_SENDMAILPID */

/* stuff fixed in later releases */
# ifndef _SCO_unix_4_2
#  define SYS5SIGNALS	1	/* SysV signal semantics -- reset on each sig */
# endif /* ! _SCO_unix_4_2 */

# ifndef _SCO_DS
#  define ftruncate	chsize	/* use chsize(2) to emulate ftruncate */
#  define NEEDFSYNC	1	/* needs the fsync(2) call stub */
#  define NETUNIX	0	/* no unix domain socket support */
#  define LA_TYPE	LA_SHORT
# endif /* ! _SCO_DS */

#endif /* _SCO_unix_ */


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
# ifndef _PATH_VENDOR_CF
#  define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
# endif /* ! _PATH_VENDOR_CF */
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/etc/sendmail.pid"
# endif /* ! _PATH_SENDMAILPID */
#endif /* ISC_UNIX */


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
# define NOFTRUNCATE	1	/* do not have ftruncate(2) */
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
typedef unsigned long	mode_t;

/* some stuff that should have been in the include files */
extern char		*malloc();
extern struct passwd	*getpwent();
extern struct passwd	*getpwnam();
extern struct passwd	*getpwuid();
extern char		*getenv();
extern struct group	*getgrgid();
extern struct group	*getgrnam();

#endif /* ALTOS_SYSTEM_V */


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
# ifndef _PATH_VENDOR_CF
#  define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
# endif /* ! _PATH_VENDOR_CF */
# ifndef S_IREAD
#  define S_IREAD	_S_IREAD
#  define S_IWRITE	_S_IWRITE
#  define S_IEXEC	_S_IEXEC
#  define S_IFMT	_S_IFMT
#  define S_IFCHR	_S_IFCHR
#  define S_IFBLK	_S_IFBLK
# endif /* ! S_IREAD */
# ifndef TZ_TYPE
#  define TZ_TYPE	TZ_TIMEZONE
# endif /* ! TZ_TYPE */
# ifndef IDENTPROTO
#  define IDENTPROTO	1
# endif /* ! IDENTPROTO */
# ifndef SHARE_V1
#  define SHARE_V1	1	/* version 1 of the fair share scheduler */
# endif /* ! SHARE_V1 */
# if !defined(__GNUC__ )
#  define UID_T	int		/* GNUC gets it right, ConvexC botches */
#  define GID_T	int		/* GNUC gets it right, ConvexC botches */
# endif /* !defined(__GNUC__ ) */
# if SECUREWARE
#  define FORK	fork		/* SecureWare wants the real fork! */
# else /* SECUREWARE */
#  define FORK	vfork		/* the rest of the OS versions don't care */
# endif /* SECUREWARE */
#endif /* _CONVEX_SOURCE */


/*
**  RISC/os 4.52
**
**	Gives a ton of warning messages, but otherwise compiles.
*/

#ifdef RISCOS

# define HASUNSETENV	1	/* has unsetenv(3) call */
# ifndef HASFLOCK
#  define HASFLOCK	1	/* has flock(2) call */
# endif /* ! HASFLOCK */
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

typedef int		pid_t;
# define SIGFUNC_DEFINED
# define SIGFUNC_RETURN	(0)
# define SIGFUNC_DECL	int
typedef int		(*sigfunc_t)();
extern char		*getenv();
extern void		*malloc();

/* added for RISC/os 4.01...which is dumber than 4.50 */
# ifdef RISCOS_4_0
#  ifndef ARBPTR_T
#   define ARBPTR_T	char *
#  endif /* ! ARBPTR_T */
#  undef HASFLOCK
#  define HASFLOCK	0
# endif /* RISCOS_4_0 */

# include <sys/time.h>

#endif /* RISCOS */


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
**  Last compiled against:	[07/21/98 @ 11:47:34 AM (Tuesday)]
**	sendmail 8.9.1		bind-8.1.2		db-2.4.14
**	gcc-2.8.1		glibc-2.0.94		linux-2.1.109
**
**  NOTE: Override HASFLOCK as you will but, as of 1.99.6, mixed-style
**	file locking is no longer allowed.  In particular, make sure
**	your DBM library and sendmail are both using either flock(2)
**	*or* fcntl(2) file locking, but not both.
*/

#ifdef __linux__
# include <linux/version.h>
# if !defined(KERNEL_VERSION)	/* not defined in 2.0.x kernel series */
#  define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
# endif /* KERNEL_VERSION */
# define BSD		1	/* include BSD defines */
# define USESETEUID	0	/* Have it due to POSIX, but doesn't work */
# define NEEDGETOPT	1	/* need a replacement for getopt(3) */
# define HASUNAME	1	/* use System V uname(2) system call */
# define HASUNSETENV	1	/* has unsetenv(3) call */
# ifndef HASSNPRINTF
#  define HASSNPRINTF	1	/* has snprintf(3) and vsnprintf(3) */
# endif /* ! HASSNPRINTF */
# define ERRLIST_PREDEFINED	/* don't declare sys_errlist */
# define GIDSET_T	gid_t	/* from <linux/types.h> */
# define HASGETUSERSHELL 0	/* getusershell(3) broken in Slackware 2.0 */
# ifndef IP_SRCROUTE
#  define IP_SRCROUTE	0	/* linux <= 1.2.8 doesn't support IP_OPTIONS */
# endif /* ! IP_SRCROUTE */
# ifndef HAS_IN_H
#  define HAS_IN_H	1	/* use netinet/in.h */
# endif /* ! HAS_IN_H */
# define USE_SIGLONGJMP	1	/* sigsetjmp needed for signal handling */
# ifndef HASFLOCK
#  if LINUX_VERSION_CODE < 66399
#   define HASFLOCK	0	/* flock(2) is broken after 0.99.13 */
#  else /* LINUX_VERSION_CODE < 66399 */
#   define HASFLOCK	1	/* flock(2) fixed after 1.3.95 */
#  endif /* LINUX_VERSION_CODE < 66399 */
# endif /* ! HASFLOCK */
# ifndef LA_TYPE
#  define LA_TYPE	LA_PROCSTR
# endif /* ! LA_TYPE */
# define SFS_TYPE	SFS_VFS		/* use <sys/vfs.h> statfs() impl */
# define SPT_PADCHAR	'\0'		/* pad process title with nulls */
# if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,0,0))
#  ifndef HASURANDOMDEV
#   define HASURANDOMDEV 1	/* 2.0 (at least) has linux/drivers/char/random.c */
#  endif /* ! HASURANDOMDEV */
# endif /* LINUX_VERSION_CODE */
# ifndef TZ_TYPE
#  define TZ_TYPE	TZ_NONE		/* no standard for Linux */
# endif /* ! TZ_TYPE */
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/var/run/sendmail.pid"
# endif /* ! _PATH_SENDMAILPID */
# include <sys/sysmacros.h>
# undef atol			/* wounded in <stdlib.h> */
# if NETINET6
   /*
   **  Linux doesn't have a good way to tell userland what interfaces are
   **  IPv6-capable.  Therefore, the BIND resolver can not determine if there
   **  are IPv6 interfaces to honor AI_ADDRCONFIG.  Unfortunately, it assumes
   **  that none are present.  (Excuse the macro name ADDRCONFIG_IS_BROKEN.)
   */
#  define ADDRCONFIG_IS_BROKEN	1

   /*
   **  Indirectly included from glibc's <feature.h>.  IPv6 support is native
   **  in 2.1 and later, but the APIs appear before the functions.
   */
#  if defined(__GLIBC__) && defined(__GLIBC_MINOR__)
#   define GLIBC_VERSION ((__GLIBC__ << 8) + __GLIBC_MINOR__)
#   if (GLIBC_VERSION >= 0x201)
#    undef IPPROTO_ICMPV6	/* linux #defines, glibc enums */
#   else /* (GLIBC_VERSION >= 0x201) */
#    include <linux/in6.h>	/* IPv6 support */
#   endif /* (GLIBC_VERSION >= 0x201) */
#   if (GLIBC_VERSION >= 0x201 && !defined(NEEDSGETIPNODE))
     /* Have APIs in <netdb.h>, but no support in glibc */
#    define NEEDSGETIPNODE	1
#   endif /* (GLIBC_VERSION >= 0x201 && !defined(NEEDSGETIPNODE)) */
#   undef GLIBC_VERSION
#  endif /* defined(__GLIBC__) && defined(__GLIBC_MINOR__) */
# endif /* NETINET6 */
# ifndef HASFCHOWN
#  define HASFCHOWN	1	/* fchown(2) */
# endif /* ! HASFCHOWN */
#endif /* __linux__ */


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
#endif /* DELL_SVR4 */


/*
**  Apple A/UX 3.0
*/

#ifdef _AUX_SOURCE
# include <sys/sysmacros.h>
# define BSD			/* has BSD routines */
# define HASSETRLIMIT	0	/* ... but not setrlimit(2) */
# define BROKEN_RES_SEARCH 1	/* res_search(unknown) returns h_errno=0 */
# define BOGUS_O_EXCL	1	/* exclusive open follows symlinks */
# define HASUNAME	1	/* use System V uname(2) system call */
# define HASFCHMOD	1	/* has fchmod(2) syscall */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASSETVBUF	1	/* has setvbuf(3) in libc */
# define HASSTRERROR	1	/* has strerror(3) */
# define SIGFUNC_DEFINED	/* sigfunc_t already defined */
# define SIGFUNC_RETURN		/* POSIX-mode */
# define SIGFUNC_DECL	void	/* POSIX-mode */
# define ERRLIST_PREDEFINED	1
# ifndef IDENTPROTO
#  define IDENTPROTO	0	/* TCP/IP implementation is broken */
# endif /* ! IDENTPROTO */
# ifndef LA_TYPE
#  define LA_TYPE	LA_INT
#  define FSHIFT	16
# endif /* ! LA_TYPE */
# define LA_AVENRUN	"avenrun"
# define SFS_TYPE	SFS_VFS	/* use <sys/vfs.h> statfs() implementation */
# define TZ_TYPE	TZ_TZNAME
# ifndef _PATH_UNIX
#  define _PATH_UNIX		"/unix"		/* should be in <paths.h> */
# endif /* ! _PATH_UNIX */
# ifndef _PATH_VENDOR_CF
#  define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
# endif /* ! _PATH_VENDOR_CF */
# undef WIFEXITED
# undef WEXITSTATUS
#endif /* _AUX_SOURCE */


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
#endif /* UMAXV */


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
#endif /* titan */


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
# ifdef _POSIX_VERSION
#  undef _POSIX_VERSION		/* set in <unistd.h> */
# endif /* _POSIX_VERSION */
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
# endif /* ! IDENTPROTO */

# ifndef _PATH_UNIX
#  define _PATH_UNIX		"/dynix"
# endif /* ! _PATH_UNIX */
# ifndef _PATH_VENDOR_CF
#  define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
# endif /* ! _PATH_VENDOR_CF */
#endif /* sequent */


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
# endif /* ! IDENTPROTO */
# ifndef _PATH_VENDOR_CF
#  define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
# endif /* ! _PATH_VENDOR_CF */
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/etc/sendmail.pid"
# endif /* ! _PATH_SENDMAILPID */
#endif /* _SEQUENT_ */


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
#endif /* UNICOS */


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
# ifndef _PATH_VENDOR_CF
#  define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
# endif /* ! _PATH_VENDOR_CF */
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/etc/sendmail.pid"
# endif /* ! _PATH_SENDMAILPID */
# undef	 S_IFSOCK		/* S_IFSOCK and S_IFIFO are the same */
# undef	 S_IFIFO
# define S_IFIFO	0010000
# ifndef IDENTPROTO
#  define IDENTPROTO	0	/* TCP/IP implementation is broken */
# endif /* ! IDENTPROTO */
# define RLIMIT_NEEDS_SYS_TIME_H	1
# if defined(NGROUPS_MAX) && !NGROUPS_MAX
#  undef NGROUPS_MAX
# endif /* defined(NGROUPS_MAX) && !NGROUPS_MAX */
#endif /* apollo */

/*
**  System V Rel 5.x (a.k.a Unixware7 w/o BSD-Compatibility Libs ie. native)
**
**	Contributed by Paul Gampe <paulg@apnic.net>
*/

#ifdef __svr5__
# include <sys/mkdev.h>
# define __svr4__
# define SYS5SIGNALS		1
# define HASSETSID		1
# define HASSNPRINTF		1
# define HASSETREUID		1
# define HASWAITPID		1
# define HASGETDTABLESIZE	1
# define GIDSET_T		gid_t
# define SOCKADDR_LEN_T		size_t
# define SOCKOPT_LEN_T		size_t
# ifndef _PATH_UNIX
#  define _PATH_UNIX		"/stand/unix"
# endif /* ! _PATH_UNIX */
# define SPT_PADCHAR		'\0'	/* pad process title with nulls */
# ifndef SYSLOG_BUFSIZE
#  define SYSLOG_BUFSIZE	1024	/* unsure */
# endif /* SYSLOG_BUFSIZE */
# ifndef _PATH_VENDOR_CF
#  define _PATH_VENDOR_CF	"/etc/sendmail.cf"
# endif /* ! _PATH_VENDOR_CF */
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/etc/sendmail.pid"
# endif /* ! _PATH_SENDMAILPID */
# undef offsetof		/* avoid stddefs.h, sys/sysmacros.h conflict */
#if !defined(SM_SET_H_ERRNO) && defined(_REENTRANT)
# define SM_SET_H_ERRNO(err)	set_h_errno((err))
#endif /* ! SM_SET_H_ERRNO && _REENTRANT */
#endif /* __svr5__ */

/* ###################################################################### */

/*
**  UnixWare 2.x
*/

#ifdef UNIXWARE2
# define UNIXWARE	1
# define HASSNPRINTF	1	/* has snprintf(3) and vsnprintf(3) */
# undef offsetof		/* avoid stddefs.h, sys/sysmacros.h conflict */
#endif /* UNIXWARE2 */


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
# ifndef _PATH_UNIX
#  define _PATH_UNIX		"/unix"
# endif /* ! _PATH_UNIX */
# ifndef _PATH_VENDOR_CF
#  define _PATH_VENDOR_CF	"/usr/ucblib/sendmail.cf"
# endif /* ! _PATH_VENDOR_CF */
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/usr/ucblib/sendmail.pid"
# endif /* ! _PATH_SENDMAILPID */
# define SYSLOG_BUFSIZE	128
#endif /* UNIXWARE */


/*
**  Intergraph CLIX 3.1
**
**	From Paul Southworth <pauls@locust.cic.net>
*/

#ifdef CLIX
# define SYSTEM5	1	/* looks like System V */
# ifndef HASGETUSERSHELL
#  define HASGETUSERSHELL 0	/* does not have getusershell(3) call */
# endif /* ! HASGETUSERSHELL */
# define DEV_BSIZE	512	/* device block size not defined */
# define GIDSET_T	gid_t
# undef LOG			/* syslog not available */
# define NEEDFSYNC	1	/* no fsync in system library */
# define GETSHORT	_getshort
#endif /* CLIX */


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
#endif /* NCR_MP_RAS2 */


/*
**  NCR MP-RAS 3.x (SysVr4) with STREAMware TCP/IP
**
**	From Tom Moore <Tom.Moore@DaytonOH.NCR.COM>
*/

#ifdef NCR_MP_RAS3
# define __svr4__
# define HASFCHOWN	1	/* has fchown(2) call */
# define SIOCGIFNUM_IS_BROKEN	1	/* SIOCGIFNUM has non-std interface */
# define SO_REUSEADDR_IS_BROKEN	1	/* doesn't work if accept() fails */
# define SYSLOG_BUFSIZE	1024
# define SPT_TYPE	SPT_NONE
# ifndef _XOPEN_SOURCE
#  define _XOPEN_SOURCE
#  define _XOPEN_SOURCE_EXTENDED 1
#  include <sys/resource.h>
#  undef _XOPEN_SOURCE
#  undef _XOPEN_SOURCE_EXTENDED
# endif /* ! _XOPEN_SOURCE */
#endif /* NCR_MP_RAS3 */


/*
**  Tandem NonStop-UX SVR4
**
**	From Rick McCarty <mccarty@mpd.tandem.com>.
*/

#ifdef NonStop_UX_BXX
# define __svr4__
#endif /* NonStop_UX_BXX */


/*
**  Hitachi 3050R/3050RX and 3500 Workstations running HI-UX/WE2.
**
**	Tested for 1.04, 1.03
**	From Akihiro Hashimoto ("Hash") <hash@dominic.ipc.chiba-u.ac.jp>.
**
**	Tested for 4.02, 6.10 and 7.10
**	From Motonori NAKAMURA <motonori@media.kyoto-u.ac.jp>.
*/

#if !defined(__hpux) && (defined(_H3050R) || defined(_HIUX_SOURCE))
# define SYSTEM5	1	/* include all the System V defines */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASFCHMOD	1	/* has fchmod(2) syscall */
# define setreuid(r, e)	setresuid(r, e, -1)
# define LA_TYPE	LA_FLOAT
# define SPT_TYPE	SPT_PSTAT
# define SFS_TYPE	SFS_VFS	/* use <sys/vfs.h> statfs() implementation */
# ifndef HASSETVBUF
#  define HASSETVBUF	/* HI-UX has no setlinebuf */
# endif /* ! HASSETVBUF */
# ifndef GIDSET_T
#  define GIDSET_T	gid_t
# endif /* ! GIDSET_T */
# ifndef _PATH_UNIX
#  define _PATH_UNIX		"/HI-UX"
# endif /* ! _PATH_UNIX */
# ifndef _PATH_VENDOR_CF
#  define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
# endif /* ! _PATH_VENDOR_CF */
# ifndef IDENTPROTO
#  define IDENTPROTO	0	/* TCP/IP implementation is broken */
# endif /* ! IDENTPROTO */
# ifndef HASGETUSERSHELL
#  define HASGETUSERSHELL 0	/* getusershell(3) causes core dumps */
# endif /* ! HASGETUSERSHELL */
# define FDSET_CAST	(int *)	/* cast for fd_set parameters to select */

/*
**  avoid m_flags conflict between Berkeley DB 1.85 db.h & sys/sysmacros.h
**  on HIUX 3050
*/
# undef m_flags

# ifdef __STDC__
extern int	syslog(int, char *, ...);
# else /* __STDC__ */
extern int	syslog();
# endif /* __STDC__ */

#endif /* !defined(__hpux) && (defined(_H3050R) || defined(_HIUX_SOURCE)) */


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
# endif /* ! HASGETUSERSHELL */
# define GIDSET_T	gid_t	/* type of 2nd arg to getgroups(2) isn't int */
# define LA_TYPE	LA_ZERO		/* doesn't have load average */
# define SFS_TYPE	SFS_4ARGS	/* use 4-arg statfs() */
# define SFS_BAVAIL	f_bfree		/* alternate field name */
# define _PATH_UNIX		"/unix"
# ifndef _PATH_VENDOR_CF
#  define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
# endif /* ! _PATH_VENDOR_CF */
#endif /* _UTS */

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
#endif /* _CRAYCOM */


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
#  endif /* ! BSD */
#  define HASUNSETENV	1	/* has unsetenv(2) call */
#  undef HASSETVBUF		/* don't actually have setvbuf(3) */
#  define WAITUNION	1	/* use "union wait" as wait argument type */
#  define LA_TYPE	LA_INT
#  define SFS_TYPE	SFS_VFS /* use <sys/vfs.h> statfs() implementation */
#  ifndef HASFLOCK
#   define HASFLOCK	1	/* has flock(2) call */
#  endif /* ! HASFLOCK */
#  define setpgid	setpgrp
#  undef WIFEXITED
#  undef WEXITSTATUS
#  define MODE_T	int	/* system include files have no mode_t */
typedef int		pid_t;
typedef int		(*sigfunc_t)();
#  define SIGFUNC_DEFINED
#  define SIGFUNC_RETURN	(0)
#  define SIGFUNC_DECL		int

# else /* ! __svr4 */
			/* NEWS-OS 6.0.3 with /bin/cc */
#  ifndef __svr4__
#   define __svr4__		/* use all System V Release 4 defines below */
#  endif /* ! __svr4__ */
#  define HASSETSID	1	/* has Posix setsid(2) call */
#  define HASGETUSERSHELL 1	/* DOES have getusershell(3) call in libc */
#  define LA_TYPE	LA_READKSYM	/* use MIOC_READKSYM ioctl */
#  ifndef SPT_TYPE
#   define SPT_TYPE	SPT_SYSMIPS	/* use sysmips() (OS 6.0.2 or later) */
#  endif /* ! SPT_TYPE */
#  define GIDSET_T	gid_t
#  undef WIFEXITED
#  undef WEXITSTATUS
#  ifndef SYSLOG_BUFSIZE
#   define SYSLOG_BUFSIZE	256
#  endif /* ! SYSLOG_BUFSIZE */
#  define _PATH_UNIX		"/stand/unix"
#  ifndef _PATH_VENDOR_CF
#   define _PATH_VENDOR_CF	"/etc/mail/sendmail.cf"
#  endif /* ! _PATH_VENDOR_CF */
#  ifndef _PATH_SENDMAILPID
#   define _PATH_SENDMAILPID	"/etc/mail/sendmail.pid"
#  endif /* ! _PATH_SENDMAILPID */

# endif /* ! __svr4 */
#endif /* sony_news */


/*
**  Omron LUNA/UNIOS-B 3.0, LUNA2/Mach and LUNA88K Mach
**
**	From Motonori NAKAMURA <motonori@cs.ritsumei.ac.jp>.
*/

#ifdef luna
# ifndef IDENTPROTO
#  define IDENTPROTO	0	/* TCP/IP implementation is broken */
# endif /* ! IDENTPROTO */
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
# endif /* uniosb */
# ifdef luna2
#  define LA_TYPE	LA_SUBR
#  define TZ_TYPE	TZ_TM_ZONE	/* use tm->tm_zone */
# endif /* luna2 */
# ifdef luna88k
#  define HASSNPRINTF	1	/* has snprintf(3) and vsnprintf(3) */
#  define LA_TYPE	LA_INT
# endif /* luna88k */
# define SFS_TYPE	SFS_VFS /* use <sys/vfs.h> statfs() implementation */
# define setpgid	setpgrp
# undef WIFEXITED
# undef WEXITSTATUS
typedef int		pid_t;
typedef int		(*sigfunc_t)();
# define SIGFUNC_DEFINED
# define SIGFUNC_RETURN	(0)
# define SIGFUNC_DECL	int
extern char	*getenv();
# ifndef _PATH_VENDOR_CF
#  define _PATH_VENDOR_CF	"/usr/lib/sendmail.cf"
# endif /* ! _PATH_VENDOR_CF */
#endif /* luna */


/*
**  NEC EWS-UX/V 4.2 (with /usr/ucb/cc)
**
**	From Motonori NAKAMURA <motonori@cs.ritsumei.ac.jp>.
*/

#if defined(nec_ews_svr4) || defined(_nec_ews_svr4)
# ifndef __svr4__
#  define __svr4__		/* use all System V Release 4 defines below */
# endif /* ! __svr4__ */
# define SYS5SIGNALS	1	/* SysV signal semantics -- reset on each sig */
# define HASSETSID	1	/* has Posix setsid(2) call */
# define LA_TYPE	LA_READKSYM	/* use MIOC_READSYM ioctl */
# define SFS_TYPE	SFS_USTAT	/* use System V ustat(2) syscall */
# define GIDSET_T	gid_t
# undef WIFEXITED
# undef WEXITSTATUS
# define NAMELISTMASK	0x7fffffff	/* mask for nlist() values */
# ifndef _PATH_VENDOR_CF
#  define _PATH_VENDOR_CF	"/usr/ucblib/sendmail.cf"
# endif /* ! _PATH_VENDOR_CF */
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/usr/ucblib/sendmail.pid"
# endif /* ! _PATH_SENDMAILPID */
# ifndef SYSLOG_BUFSIZE
#  define SYSLOG_BUFSIZE	1024	/* allow full size syslog buffer */
# endif /* ! SYSLOG_BUFSIZE */
#endif /* defined(nec_ews_svr4) || defined(_nec_ews_svr4) */


/*
**  Fujitsu/ICL UXP/DS (For the DS/90 Series)
**
**	From Diego R. Lopez <drlopez@cica.es>.
**	Additional changes from Fumio Moriya and Toshiaki Nomura of the
**		Fujitsu Fresoftware group <dsfrsoft@oai6.yk.fujitsu.co.jp>.
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
# else /* UXPDS == 10 */
#  define HASSNPRINTF		1	/* has snprintf(3) and vsnprintf(3) */
# endif /* UXPDS == 10 */
# define _PATH_UNIX		"/stand/unix"
# ifndef _PATH_VENDOR_CF
#  define _PATH_VENDOR_CF	"/usr/ucblib/sendmail.cf"
# endif /* ! _PATH_VENDOR_CF */
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/usr/ucblib/sendmail.pid"
# endif /* ! _PATH_SENDMAILPID */
#endif /* __uxp__ */

/*
**  Pyramid DC/OSx
**
**	From Earle Ake <akee@wpdiss1.wpafb.af.mil>.
*/

#ifdef DCOSx
# define GIDSET_T	gid_t
# ifndef IDENTPROTO
#  define IDENTPROTO	0	/* TCP/IP implementation is broken */
# endif /* ! IDENTPROTO */
#endif /* DCOSx */

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
# endif /* ! SYSLOG_BUFSIZE */

# undef WUNTRACED
# undef WIFEXITED
# undef WIFSIGNALED
# undef WIFSTOPPED
# undef WEXITSTATUS
# undef WTERMSIG
# undef WSTOPSIG

#endif /* __MAXION__ */

/*
**  Harris Nighthawk PowerUX (nh6000 box)
**
**  Contributed by Bob Miorelli, Pratt & Whitney <miorelli@pweh.com>
*/

#ifdef _PowerUX
# ifndef __svr4__
#  define __svr4__
# endif /* ! __svr4__ */
# ifndef _PATH_VENDOR_CF
#  define _PATH_VENDOR_CF	"/etc/mail/sendmail.cf"
# endif /* ! _PATH_VENDOR_CF */
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/etc/mail/sendmail.pid"
# endif /* ! _PATH_SENDMAILPID */
# define SYSLOG_BUFSIZE		1024
# define HASSNPRINTF		1	/* has snprintf(3) and vsnprintf(3) */
# define LA_TYPE		LA_ZERO
typedef struct msgb		mblk_t;
# undef offsetof	/* avoid stddefs.h and sys/sysmacros.h conflict */
#endif /* _PowerUX */

/*
**  Siemens Nixdorf Informationssysteme AG SINIX
**
**	Contributed by Gerald Rinske <Gerald.Rinske@mch.sni.de>
**	of Siemens Business Services VAS.
*/
#ifdef sinix
# define HASRANDOM		0	/* has random(3) */
# define SYSLOG_BUFSIZE		1024
#endif /* sinix */

/*
**  CRAY T3E
**
**	Contributed by Manu Mahonen <mailadm@csc.fi>
**	of Center for Scientific Computing.
*/
#ifdef _CRAY
# define GET_IPOPT_DST(dst)	*(struct in_addr *)&(dst)
#endif /* _CRAY */

/*
**  Motorola 922, MC88110, UNIX SYSTEM V/88 Release 4.0 Version 4.3
**
**	Contributed by Sergey Rusanov <rsm@utfoms.udmnet.ru>
*/

#ifdef MOTO
# define HASFCHMOD		1
# define HASSETRLIMIT		0
# define HASSETSID		1
# define HASSETREUID		1
# define HASULIMIT		1
# define HASWAITPID		1
# define HASGETDTABLESIZE	1
# define HASGETUSERSHELL	1
# define IP_SRCROUTE		0
# define IDENTPROTO		0
# define RES_DNSRCH_VARIABLE	_res_dnsrch
# define _PATH_UNIX		"/unix"
# define _PATH_VENDOR_CF	"/etc/sendmail.cf"
# define _PATH_SENDMAILPID	"/var/run/sendmail.pid"
#endif /* MOTO */


/**********************************************************************
**  End of Per-Operating System defines
**********************************************************************/
/**********************************************************************
**  More general defines
**********************************************************************/

/* general BSD defines */
#ifdef BSD
# define HASGETDTABLESIZE 1	/* has getdtablesize(2) call */
# ifndef HASSETREUID
#  define HASSETREUID	1	/* has setreuid(2) call */
# endif /* ! HASSETREUID */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# ifndef IP_SRCROUTE
#  define IP_SRCROUTE	1	/* can check IP source routing */
# endif /* ! IP_SRCROUTE */
# ifndef HASSETRLIMIT
#  define HASSETRLIMIT	1	/* has setrlimit(2) call */
# endif /* ! HASSETRLIMIT */
# ifndef HASFLOCK
#  define HASFLOCK	1	/* has flock(2) call */
# endif /* ! HASFLOCK */
# ifndef TZ_TYPE
#  define TZ_TYPE	TZ_TM_ZONE	/* use tm->tm_zone variable */
# endif /* ! TZ_TYPE */
#endif /* BSD */

/* general System V Release 4 defines */
#ifdef __svr4__
# define SYSTEM5	1
# define USESETEUID	1	/* has usable seteuid(2) call */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define BSD_COMP	1	/* get BSD ioctl calls */
# ifndef HASSETRLIMIT
#  define HASSETRLIMIT	1	/* has setrlimit(2) call */
# endif /* ! HASSETRLIMIT */
# ifndef HASGETUSERSHELL
#  define HASGETUSERSHELL 0	/* does not have getusershell(3) call */
# endif /* ! HASGETUSERSHELL */
# ifndef HASFCHMOD
#  define HASFCHMOD	1	/* most (all?) SVr4s seem to have fchmod(2) */
# endif /* ! HASFCHMOD */

# ifndef _PATH_UNIX
#  define _PATH_UNIX		"/unix"
# endif /* ! _PATH_UNIX */
# ifndef _PATH_VENDOR_CF
#  define _PATH_VENDOR_CF	"/usr/ucblib/sendmail.cf"
# endif /* ! _PATH_VENDOR_CF */
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/usr/ucblib/sendmail.pid"
# endif /* ! _PATH_SENDMAILPID */
# ifndef SYSLOG_BUFSIZE
#  define SYSLOG_BUFSIZE	128
# endif /* ! SYSLOG_BUFSIZE */
# ifndef SFS_TYPE
#  define SFS_TYPE		SFS_STATVFS
# endif /* ! SFS_TYPE */

# define USE_SIGLONGJMP	1	/* sigsetjmp needed for signal handling */
#endif /* __svr4__ */

/* general System V defines */
#ifdef SYSTEM5
# include <sys/sysmacros.h>
# define HASUNAME	1	/* use System V uname(2) system call */
# define SYS5SETPGRP	1	/* use System V setpgrp(2) syscall */
# define HASSETVBUF	1	/* we have setvbuf(3) in libc */
# ifndef HASULIMIT
#  define HASULIMIT	1	/* has the ulimit(2) syscall */
# endif /* ! HASULIMIT */
# ifndef LA_TYPE
#  ifdef MIOC_READKSYM
#   define LA_TYPE	LA_READKSYM	/* use MIOC_READKSYM ioctl */
#  else /* MIOC_READKSYM */
#   define LA_TYPE	LA_INT		/* assume integer load average */
#  endif /* MIOC_READKSYM */
# endif /* ! LA_TYPE */
# ifndef SFS_TYPE
#  define SFS_TYPE	SFS_USTAT	/* use System V ustat(2) syscall */
# endif /* ! SFS_TYPE */
# ifndef TZ_TYPE
#  define TZ_TYPE	TZ_TZNAME	/* use tzname[] vector */
# endif /* ! TZ_TYPE */
#endif /* SYSTEM5 */

/* general POSIX defines */
#ifdef _POSIX_VERSION
# define HASSETSID	1	/* has Posix setsid(2) call */
# define HASWAITPID	1	/* has Posix waitpid(2) call */
# if _POSIX_VERSION >= 199500 && !defined(USESETEUID)
#  define USESETEUID	1	/* has usable seteuid(2) call */
# endif /* _POSIX_VERSION >= 199500 && !defined(USESETEUID) */
#endif /* _POSIX_VERSION */
/*
**  Tweaking for systems that (for example) claim to be BSD or POSIX
**  but don't have all the standard BSD or POSIX routines (boo hiss).
*/

#ifdef titan
# undef HASINITGROUPS		/* doesn't have initgroups(3) call */
#endif /* titan */

#ifdef _CRAYCOM
# undef HASSETSID		/* despite POSIX claim, doesn't have setsid */
#endif /* _CRAYCOM */

#ifdef MOTO
# undef USESETEUID
#endif /* MOTO */

/*
**  Due to a "feature" in some operating systems such as Ultrix 4.3 and
**  HPUX 8.0, if you receive a "No route to host" message (ICMP message
**  ICMP_UNREACH_HOST) on _any_ connection, all connections to that host
**  are closed.  Some firewalls return this error if you try to connect
**  to the IDENT port (113), so you can't receive email from these hosts
**  on these systems.  The firewall really should use a more specific
**  message such as ICMP_UNREACH_PROTOCOL or _PORT or _FILTER_PROHIB.  If
**  not explicitly set to zero above, default it on.
*/

#ifndef IDENTPROTO
# define IDENTPROTO	1	/* use IDENT proto (RFC 1413) */
#endif /* ! IDENTPROTO */

#ifndef IP_SRCROUTE
# define IP_SRCROUTE	1	/* Detect IP source routing */
#endif /* ! IP_SRCROUTE */

#ifndef HASGETUSERSHELL
# define HASGETUSERSHELL 1	/* libc has getusershell(3) call */
#endif /* ! HASGETUSERSHELL */

#ifndef NETUNIX
# define NETUNIX	1	/* include unix domain support */
#endif /* ! NETUNIX */

#ifndef HASRANDOM
# define HASRANDOM	1	/* has random(3) support */
#endif /* ! HASRANDOM */

#ifndef HASFLOCK
# define HASFLOCK	0	/* assume no flock(2) support */
#endif /* ! HASFLOCK */

#ifndef HASSETREUID
# define HASSETREUID	0	/* assume no setreuid(2) call */
#endif /* ! HASSETREUID */

#ifndef HASFCHMOD
# define HASFCHMOD	0	/* assume no fchmod(2) syscall */
#endif /* ! HASFCHMOD */

#ifndef USESETEUID
# define USESETEUID	0	/* assume no seteuid(2) call or no saved ids */
#endif /* ! USESETEUID */

#ifndef HASSETRLIMIT
# define HASSETRLIMIT	0	/* assume no setrlimit(2) support */
#endif /* ! HASSETRLIMIT */

#ifndef HASULIMIT
# define HASULIMIT	0	/* assume no ulimit(2) support */
#endif /* ! HASULIMIT */

#ifndef SECUREWARE
# define SECUREWARE	0	/* assume no SecureWare C2 auditing hooks */
#endif /* ! SECUREWARE */

#ifndef USE_SIGLONGJMP
# define USE_SIGLONGJMP	0	/* assume setjmp handles signals properly */
#endif /* ! USE_SIGLONGJMP */

#ifndef FDSET_CAST
# define FDSET_CAST		/* (empty) cast for fd_set arg to select */
#endif /* ! FDSET_CAST */

/*
**  Pick a mailer setuid method for changing the current uid
*/

#define USE_SETEUID	0
#define USE_SETREUID	1
#define USE_SETUID	2

#if USESETEUID
# define MAILER_SETUID_METHOD	USE_SETEUID
#else /* USESETEUID */
# if HASSETREUID
#  define MAILER_SETUID_METHOD	USE_SETREUID
# else /* HASSETREUID */
#  define MAILER_SETUID_METHOD	USE_SETUID
# endif /* HASSETREUID */
#endif /* USESETEUID */

/*
**  If no type for argument two of getgroups call is defined, assume
**  it's an integer -- unfortunately, there seem to be several choices
**  here.
*/

#ifndef GIDSET_T
# define GIDSET_T	int
#endif /* ! GIDSET_T */

#ifndef UID_T
# define UID_T		uid_t
#endif /* ! UID_T */

#ifndef GID_T
# define GID_T		gid_t
#endif /* ! GID_T */

#ifndef SIZE_T
# define SIZE_T		size_t
#endif /* ! SIZE_T */

#ifndef MODE_T
# define MODE_T		mode_t
#endif /* ! MODE_T */

#ifndef ARGV_T
# define ARGV_T		char **
#endif /* ! ARGV_T */

#ifndef SOCKADDR_LEN_T
# define SOCKADDR_LEN_T	int
#endif /* ! SOCKADDR_LEN_T */

#ifndef SOCKOPT_LEN_T
# define SOCKOPT_LEN_T	int
#endif /* ! SOCKOPT_LEN_T */

#ifndef QUAD_T
# define QUAD_T	unsigned long
#endif /* ! QUAD_T */
/**********************************************************************
**  Remaining definitions should never have to be changed.  They are
**  primarily to provide back compatibility for older systems -- for
**  example, it includes some POSIX compatibility definitions
**********************************************************************/

/* System 5 compatibility */
#ifndef S_ISREG
# define S_ISREG(foo)	((foo & S_IFMT) == S_IFREG)
#endif /* ! S_ISREG */
#ifndef S_ISDIR
# define S_ISDIR(foo)	((foo & S_IFMT) == S_IFDIR)
#endif /* ! S_ISDIR */
#if !defined(S_ISLNK) && defined(S_IFLNK)
# define S_ISLNK(foo)	((foo & S_IFMT) == S_IFLNK)
#endif /* !defined(S_ISLNK) && defined(S_IFLNK) */
#if !defined(S_ISFIFO)
# if defined(S_IFIFO)
#  define S_ISFIFO(foo)	((foo & S_IFMT) == S_IFIFO)
# else /* defined(S_IFIFO) */
#  define S_ISFIFO(foo)	FALSE
# endif /* defined(S_IFIFO) */
#endif /* !defined(S_ISFIFO) */
#ifndef S_IRUSR
# define S_IRUSR		0400
#endif /* ! S_IRUSR */
#ifndef S_IWUSR
# define S_IWUSR		0200
#endif /* ! S_IWUSR */
#ifndef S_IRGRP
# define S_IRGRP		0040
#endif /* ! S_IRGRP */
#ifndef S_IWGRP
# define S_IWGRP		0020
#endif /* ! S_IWGRP */
#ifndef S_IROTH
# define S_IROTH		0004
#endif /* ! S_IROTH */
#ifndef S_IWOTH
# define S_IWOTH		0002
#endif /* ! S_IWOTH */

/* close-on-exec flag */
#ifndef FD_CLOEXEC
# define FD_CLOEXEC	1
#endif /* ! FD_CLOEXEC */

/*
**  Older systems don't have this error code -- it should be in
**  /usr/include/sysexits.h.
*/

#ifndef EX_CONFIG
# define EX_CONFIG	78	/* configuration error */
#endif /* ! EX_CONFIG */

/* pseudo-codes */
#define EX_QUIT		22	/* drop out of server immediately */
#define EX_RESTART	23	/* restart sendmail daemon */
#define EX_SHUTDOWN	24	/* shutdown sendmail daemon */

/* pseudo-code used for mci_setstat */
#define EX_NOTSTICKY	-5	/* don't save persistent status */


/*
**  An "impossible" file mode to indicate that the file does not exist.
*/

#define ST_MODE_NOFILE	0171147		/* unlikely to occur */


/* type of arbitrary pointer */
#ifndef ARBPTR_T
# define ARBPTR_T	void *
#endif /* ! ARBPTR_T */

#ifndef __P
# include "sendmail/cdefs.h"
#endif /* ! __P */

#if HESIOD && !defined(NAMED_BIND)
# define NAMED_BIND	1	/* not one without the other */
#endif /* HESIOD && !defined(NAMED_BIND) */

# if NAMED_BIND && !defined( __ksr__ ) && !defined( h_errno )
extern int	h_errno;
# endif /* NAMED_BIND && !defined( __ksr__ ) && !defined( h_errno ) */

#ifdef LDAPMAP
# include <sys/time.h>
# include <lber.h>
# include <ldap.h>

/* Some LDAP constants */
# define LDAPMAP_FALSE		0
# define LDAPMAP_TRUE		1

/*
**  ldap_init(3) is broken in Umich 3.x and OpenLDAP 1.0/1.1.
**  Use the lack of LDAP_OPT_SIZELIMIT to detect old API implementations
**  and assume (falsely) that all old API implementations are broken.
**  (OpenLDAP 1.2 and later have a working ldap_init(), add -DUSE_LDAP_INIT)
*/

# if defined(LDAP_OPT_SIZELIMIT) && !defined(USE_LDAP_INIT)
#  define USE_LDAP_INIT	1
# endif /* defined(LDAP_OPT_SIZELIMIT) && !defined(USE_LDAP_INIT) */

/*
**  LDAP_OPT_SIZELIMIT is not defined under Umich 3.x nor OpenLDAP 1.x,
**  hence ldap_set_option() must not exist.
*/

# if defined(LDAP_OPT_SIZELIMIT) && !defined(USE_LDAP_SET_OPTION)
#  define USE_LDAP_SET_OPTION	1
# endif /* defined(LDAP_OPT_SIZELIMIT) && !defined(USE_LDAP_SET_OPTION) */

#endif /* LDAPMAP */

/*
**  Do some required dependencies
*/

#if NETINET || NETINET6 || NETISO
# ifndef SMTP
#  define SMTP		1	/* enable user and server SMTP */
# endif /* ! SMTP */
# ifndef QUEUE
#  define QUEUE		1	/* enable queueing */
# endif /* ! QUEUE */
# ifndef DAEMON
#  define DAEMON	1	/* include the daemon (requires IPC & SMTP) */
# endif /* ! DAEMON */
#endif /* NETINET || NETINET6 || NETISO */


/*
**  Arrange to use either varargs or stdargs
*/

#ifdef __STDC__

# include <stdarg.h>

# define VA_LOCAL_DECL	va_list ap;
# define VA_START(f)	va_start(ap, f)
# define VA_END		va_end(ap)

#else /* __STDC__ */

# include <varargs.h>

# define VA_LOCAL_DECL	va_list ap;
# define VA_START(f)	va_start(ap)
# define VA_END		va_end(ap)

#endif /* __STDC__ */

#if HASUNAME
# include <sys/utsname.h>
# ifdef newstr
#  undef newstr
# endif /* newstr */
#else /* HASUNAME */
# define NODE_LENGTH 32
struct utsname
{
	char nodename[NODE_LENGTH + 1];
};
#endif /* HASUNAME */

#if !defined(MAXHOSTNAMELEN) && !defined(_SCO_unix_) && !defined(NonStop_UX_BXX) && !defined(ALTOS_SYSTEM_V)
# define MAXHOSTNAMELEN	256
#endif /* !defined(MAXHOSTNAMELEN) && !defined(_SCO_unix_) && !defined(NonStop_UX_BXX) && !defined(ALTOS_SYSTEM_V) */

#if !defined(SIGCHLD) && defined(SIGCLD)
# define SIGCHLD	SIGCLD
#endif /* !defined(SIGCHLD) && defined(SIGCLD) */

#ifndef STDIN_FILENO
# define STDIN_FILENO	0
#endif /* ! STDIN_FILENO */

#ifndef STDOUT_FILENO
# define STDOUT_FILENO	1
#endif /* ! STDOUT_FILENO */

#ifndef STDERR_FILENO
# define STDERR_FILENO	2
#endif /* ! STDERR_FILENO */

#ifndef LOCK_SH
# define LOCK_SH	0x01	/* shared lock */
# define LOCK_EX	0x02	/* exclusive lock */
# define LOCK_NB	0x04	/* non-blocking lock */
# define LOCK_UN	0x08	/* unlock */
#endif /* ! LOCK_SH */

#ifndef S_IXOTH
# define S_IXOTH	(S_IEXEC >> 6)
#endif /* ! S_IXOTH */

#ifndef S_IXGRP
# define S_IXGRP	(S_IEXEC >> 3)
#endif /* ! S_IXGRP */

#ifndef S_IXUSR
# define S_IXUSR	(S_IEXEC)
#endif /* ! S_IXUSR */

#ifndef SEEK_SET
# define SEEK_SET	0
# define SEEK_CUR	1
# define SEEK_END	2
#endif /* ! SEEK_SET */

#ifndef SIG_ERR
# define SIG_ERR	((void (*)()) -1)
#endif /* ! SIG_ERR */

#ifndef WEXITSTATUS
# define WEXITSTATUS(st)	(((st) >> 8) & 0377)
#endif /* ! WEXITSTATUS */
#ifndef WIFEXITED
# define WIFEXITED(st)		(((st) & 0377) == 0)
#endif /* ! WIFEXITED */
#ifndef WIFSTOPPED
# define WIFSTOPPED(st)		(((st) & 0100) == 0)
#endif /* ! WIFSTOPPED */
#ifndef WCOREDUMP
# define WCOREDUMP(st)		(((st) & 0200) != 0)
#endif /* ! WCOREDUMP */
#ifndef WTERMSIG
# define WTERMSIG(st)		(((st) & 0177))
#endif /* ! WTERMSIG */

#ifndef SIGFUNC_DEFINED
typedef void		(*sigfunc_t) __P((int));
#endif /* ! SIGFUNC_DEFINED */
#ifndef SIGFUNC_RETURN
# define SIGFUNC_RETURN
#endif /* ! SIGFUNC_RETURN */
#ifndef SIGFUNC_DECL
# define SIGFUNC_DECL	void
#endif /* ! SIGFUNC_DECL */

/* size of syslog buffer */
#ifndef SYSLOG_BUFSIZE
# define SYSLOG_BUFSIZE	1024
#endif /* ! SYSLOG_BUFSIZE */

/*
**  Size of prescan buffer.
**	Despite comments in the _sendmail_ book, this probably should
**	not be changed; there are some hard-to-define dependencies.
*/

#define PSBUFSIZE	(MAXNAME + MAXATOM)	/* size of prescan buffer */

/* fork routine -- set above using #ifdef _osname_ or in Makefile */
#ifndef FORK
# define FORK		fork		/* function to call to fork mailer */
#endif /* ! FORK */

/* setting h_errno */
#ifndef SM_SET_H_ERRNO
# define SM_SET_H_ERRNO(err)	h_errno = (err)
#endif /* SM_SET_H_ERRNO */

/* random routine -- set above using #ifdef _osname_ or in Makefile */
#if HASRANDOM
# define get_random()	random()
#else /* HASRANDOM */
# define get_random()	((long) rand())
# ifndef RANDOMSHIFT
#  define RANDOMSHIFT	8
# endif /* RANDOMSHIFT */
#endif /* HASRANDOM */

/*
**  Default to using scanf in readcf.
*/

#ifndef SCANF
# define SCANF		1
#endif /* ! SCANF */

#if _FFR_MILTER
/* 32 bit type */
# ifndef SM_INT32
#  define SM_INT32	int32_t
# endif /* SM_INT32 */
#endif /* _FFR_MILTER */

/*
**  SVr4 and similar systems use different routines for setjmp/longjmp
**  with signal support
*/

#if USE_SIGLONGJMP
# ifdef jmp_buf
#  undef jmp_buf
# endif /* jmp_buf */
# define jmp_buf		sigjmp_buf
# ifdef setjmp
#  undef setjmp
# endif /* setjmp */
# define setjmp(env)		sigsetjmp(env, 1)
# ifdef longjmp
#  undef longjmp
# endif /* longjmp */
# define longjmp(env, val)	siglongjmp(env, val)
#endif /* USE_SIGLONGJMP */

#if !defined(NGROUPS_MAX) && defined(NGROUPS)
# define NGROUPS_MAX	NGROUPS		/* POSIX naming convention */
#endif /* !defined(NGROUPS_MAX) && defined(NGROUPS) */

/*
**  Some snprintf() implementations are rumored not to NUL terminate.
*/
#if SNPRINTF_IS_BROKEN
# ifdef snprintf
#  undef snprintf
# endif /* snprintf */
# define snprintf	sm_snprintf
# ifdef vsnprintf
#  undef vsnprintf
# endif /* vsnprintf */
# define vsnprintf	sm_vsnprintf
#endif /* SNPRINTF_IS_BROKEN */

/*
**  If we don't have a system syslog, simulate it.
*/

#if !LOG
# define LOG_EMERG	0	/* system is unusable */
# define LOG_ALERT	1	/* action must be taken immediately */
# define LOG_CRIT	2	/* critical conditions */
# define LOG_ERR	3	/* error conditions */
# define LOG_WARNING	4	/* warning conditions */
# define LOG_NOTICE	5	/* normal but significant condition */
# define LOG_INFO	6	/* informational */
# define LOG_DEBUG	7	/* debug-level messages */
#endif /* !LOG */

#if SFIO
# ifdef ERRLIST_PREDEFINED
#  undef ERRLIST_PREDEFINED
# endif /* ERRLIST_PREDEFINED */
# if !HASSNPRINTF
#  define HASSNPRINTF	1	/* sfio includes snprintf() */
# endif /* !HASSNPRINTF */
#endif /* SFIO */

#ifndef SFIO_STDIO_COMPAT
# define SFIO_STDIO_COMPAT	0
#endif /* ! SFIO_STDIO_COMPAT */

#endif /* CONF_H */
