/* $Id: acconfig.h,v 1.183 2005/07/07 10:33:36 dtucker Exp $ */

/*
 * Copyright (c) 1999-2003 Damien Miller.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _CONFIG_H
#define _CONFIG_H

/* Generated automatically from acconfig.h by autoheader. */
/* Please make your changes there */

@TOP@

/* Define if your platform breaks doing a seteuid before a setuid */
#undef SETEUID_BREAKS_SETUID

/* Define if your setreuid() is broken */
#undef BROKEN_SETREUID

/* Define if your setregid() is broken */
#undef BROKEN_SETREGID

/* Define if your setresuid() is broken */
#undef BROKEN_SETRESUID

/* Define if your setresgid() is broken */
#undef BROKEN_SETRESGID

/* Define to a Set Process Title type if your system is */
/* supported by bsd-setproctitle.c */
#undef SPT_TYPE
#undef SPT_PADCHAR

/* SCO workaround */
#undef BROKEN_SYS_TERMIO_H

/* Define if you have SecureWare-based protected password database */
#undef HAVE_SECUREWARE

/* If your header files don't define LOGIN_PROGRAM, then use this (detected) */
/* from environment and PATH */
#undef LOGIN_PROGRAM_FALLBACK

/* Full path of your "passwd" program */
#undef _PATH_PASSWD_PROG

/* Define if your password has a pw_class field */
#undef HAVE_PW_CLASS_IN_PASSWD

/* Define if your password has a pw_expire field */
#undef HAVE_PW_EXPIRE_IN_PASSWD

/* Define if your password has a pw_change field */
#undef HAVE_PW_CHANGE_IN_PASSWD

/* Define if your system uses access rights style file descriptor passing */
#undef HAVE_ACCRIGHTS_IN_MSGHDR

/* Define if your system uses ancillary data style file descriptor passing */
#undef HAVE_CONTROL_IN_MSGHDR

/* Define if you system's inet_ntoa is busted (e.g. Irix gcc issue) */
#undef BROKEN_INET_NTOA

/* Define if your system defines sys_errlist[] */
#undef HAVE_SYS_ERRLIST

/* Define if your system defines sys_nerr */
#undef HAVE_SYS_NERR

/* Define if your system choked on IP TOS setting */
#undef IP_TOS_IS_BROKEN

/* Define if you have the getuserattr function.  */
#undef HAVE_GETUSERATTR

/* Define if you have the basename function. */
#undef HAVE_BASENAME

/* Work around problematic Linux PAM modules handling of PAM_TTY */
#undef PAM_TTY_KLUDGE

/* Define if pam_chauthtok wants real uid set to the unpriv'ed user */
#undef SSHPAM_CHAUTHTOK_NEEDS_RUID

/* Use PIPES instead of a socketpair() */
#undef USE_PIPES

/* Define if your snprintf is busted */
#undef BROKEN_SNPRINTF

/* Define if you are on Cygwin */
#undef HAVE_CYGWIN

/* Define if you have a broken realpath. */
#undef BROKEN_REALPATH

/* Define if you are on NeXT */
#undef HAVE_NEXT

/* Define if you want to enable PAM support */
#undef USE_PAM

/* Define if you want to enable AIX4's authenticate function */
#undef WITH_AIXAUTHENTICATE

/* Define if your AIX loginfailed() function takes 4 arguments (AIX >= 5.2) */
#undef AIX_LOGINFAILED_4ARG

/* Define if your skeychallenge() function takes 4 arguments (eg NetBSD) */
#undef SKEYCHALLENGE_4ARG

/* Define if you have/want arrays (cluster-wide session managment, not C arrays) */
#undef WITH_IRIX_ARRAY

/* Define if you want IRIX project management */
#undef WITH_IRIX_PROJECT

/* Define if you want IRIX audit trails */
#undef WITH_IRIX_AUDIT

/* Define if you want IRIX kernel jobs */
#undef WITH_IRIX_JOBS

/* Location of PRNGD/EGD random number socket */
#undef PRNGD_SOCKET

/* Port number of PRNGD/EGD random number socket */
#undef PRNGD_PORT

/* Builtin PRNG command timeout */
#undef ENTROPY_TIMEOUT_MSEC

/* non-privileged user for privilege separation */
#undef SSH_PRIVSEP_USER

/* Define if you want to install preformatted manpages.*/
#undef MANTYPE

/* Define if your ssl headers are included with #include <openssl/header.h>  */
#undef HAVE_OPENSSL

/* Define if you are linking against RSAref.  Used only to print the right
 * message at run-time. */
#undef RSAREF

/* struct timeval */
#undef HAVE_STRUCT_TIMEVAL

/* struct utmp and struct utmpx fields */
#undef HAVE_HOST_IN_UTMP
#undef HAVE_HOST_IN_UTMPX
#undef HAVE_ADDR_IN_UTMP
#undef HAVE_ADDR_IN_UTMPX
#undef HAVE_ADDR_V6_IN_UTMP
#undef HAVE_ADDR_V6_IN_UTMPX
#undef HAVE_SYSLEN_IN_UTMPX
#undef HAVE_PID_IN_UTMP
#undef HAVE_TYPE_IN_UTMP
#undef HAVE_TYPE_IN_UTMPX
#undef HAVE_TV_IN_UTMP
#undef HAVE_TV_IN_UTMPX
#undef HAVE_ID_IN_UTMP
#undef HAVE_ID_IN_UTMPX
#undef HAVE_EXIT_IN_UTMP
#undef HAVE_TIME_IN_UTMP
#undef HAVE_TIME_IN_UTMPX

/* Define if you don't want to use your system's login() call */
#undef DISABLE_LOGIN

/* Define if you don't want to use pututline() etc. to write [uw]tmp */
#undef DISABLE_PUTUTLINE

/* Define if you don't want to use pututxline() etc. to write [uw]tmpx */
#undef DISABLE_PUTUTXLINE

/* Define if you don't want to use lastlog */
#undef DISABLE_LASTLOG

/* Define if you don't want to use lastlog in session.c */
#undef NO_SSH_LASTLOG

/* Define if you don't want to use utmp */
#undef DISABLE_UTMP

/* Define if you don't want to use utmpx */
#undef DISABLE_UTMPX

/* Define if you don't want to use wtmp */
#undef DISABLE_WTMP

/* Define if you don't want to use wtmpx */
#undef DISABLE_WTMPX

/* Some systems need a utmpx entry for /bin/login to work */
#undef LOGIN_NEEDS_UTMPX

/* Some versions of /bin/login need the TERM supplied on the commandline */
#undef LOGIN_NEEDS_TERM

/* Define if your login program cannot handle end of options ("--") */
#undef LOGIN_NO_ENDOPT

/* Define if you want to specify the path to your lastlog file */
#undef CONF_LASTLOG_FILE

/* Define if you want to specify the path to your utmp file */
#undef CONF_UTMP_FILE

/* Define if you want to specify the path to your wtmp file */
#undef CONF_WTMP_FILE

/* Define if you want to specify the path to your utmpx file */
#undef CONF_UTMPX_FILE

/* Define if you want to specify the path to your wtmpx file */
#undef CONF_WTMPX_FILE

/* Define if you want external askpass support */
#undef USE_EXTERNAL_ASKPASS

/* Define if libc defines __progname */
#undef HAVE___PROGNAME

/* Define if compiler implements __FUNCTION__ */
#undef HAVE___FUNCTION__

/* Define if compiler implements __func__ */
#undef HAVE___func__

/* Define this is you want GSSAPI support in the version 2 protocol */
#undef GSSAPI

/* Define if you want Kerberos 5 support */
#undef KRB5

/* Define this if you are using the Heimdal version of Kerberos V5 */
#undef HEIMDAL

/* Define this if you want to use libkafs' AFS support */
#undef USE_AFS

/* Define if you want S/Key support */
#undef SKEY

/* Define if you want TCP Wrappers support */
#undef LIBWRAP

/* Define if your libraries define login() */
#undef HAVE_LOGIN

/* Define if your libraries define daemon() */
#undef HAVE_DAEMON

/* Define if your libraries define getpagesize() */
#undef HAVE_GETPAGESIZE

/* Define if xauth is found in your path */
#undef XAUTH_PATH

/* Define if you want to allow MD5 passwords */
#undef HAVE_MD5_PASSWORDS

/* Define if you want to disable shadow passwords */
#undef DISABLE_SHADOW

/* Define if you want to use shadow password expire field */
#undef HAS_SHADOW_EXPIRE

/* Define if you have Digital Unix Security Integration Architecture */
#undef HAVE_OSF_SIA

/* Define if you have getpwanam(3) [SunOS 4.x] */
#undef HAVE_GETPWANAM

/* Define if you have an old version of PAM which takes only one argument */
/* to pam_strerror */
#undef HAVE_OLD_PAM

/* Define if you are using Solaris-derived PAM which passes pam_messages  */
/* to the conversation function with an extra level of indirection */
#undef PAM_SUN_CODEBASE

/* Set this to your mail directory if you don't have maillock.h */
#undef MAIL_DIRECTORY

/* Data types */
#undef HAVE_U_INT
#undef HAVE_INTXX_T
#undef HAVE_U_INTXX_T
#undef HAVE_UINTXX_T
#undef HAVE_INT64_T
#undef HAVE_U_INT64_T
#undef HAVE_U_CHAR
#undef HAVE_SIZE_T
#undef HAVE_SSIZE_T
#undef HAVE_CLOCK_T
#undef HAVE_MODE_T
#undef HAVE_PID_T
#undef HAVE_SA_FAMILY_T
#undef HAVE_STRUCT_SOCKADDR_STORAGE
#undef HAVE_STRUCT_ADDRINFO
#undef HAVE_STRUCT_IN6_ADDR
#undef HAVE_STRUCT_SOCKADDR_IN6

/* Fields in struct sockaddr_storage */
#undef HAVE_SS_FAMILY_IN_SS
#undef HAVE___SS_FAMILY_IN_SS

/* Define if you have /dev/ptmx */
#undef HAVE_DEV_PTMX

/* Define if you have /dev/ptc */
#undef HAVE_DEV_PTS_AND_PTC

/* Define if you need to use IP address instead of hostname in $DISPLAY */
#undef IPADDR_IN_DISPLAY

/* Specify default $PATH */
#undef USER_PATH

/* Specify location of ssh.pid */
#undef _PATH_SSH_PIDDIR

/* getaddrinfo is broken (if present) */
#undef BROKEN_GETADDRINFO

/* updwtmpx is broken (if present) */
#undef BROKEN_UPDWTMPX

/* Workaround more Linux IPv6 quirks */
#undef DONT_TRY_OTHER_AF

/* Detect IPv4 in IPv6 mapped addresses and treat as IPv4 */
#undef IPV4_IN_IPV6

/* Define if you have BSD auth support */
#undef BSD_AUTH

/* Define if X11 doesn't support AF_UNIX sockets on that system */
#undef NO_X11_UNIX_SOCKETS

/* Define if the concept of ports only accessible to superusers isn't known */
#undef NO_IPPORT_RESERVED_CONCEPT

/* Needed for SCO and NeXT */
#undef BROKEN_SAVED_UIDS

/* Define if your system glob() function has the GLOB_ALTDIRFUNC extension */
#undef GLOB_HAS_ALTDIRFUNC

/* Define if your system glob() function has gl_matchc options in glob_t */
#undef GLOB_HAS_GL_MATCHC

/* Define in your struct dirent expects you to allocate extra space for d_name */
#undef BROKEN_ONE_BYTE_DIRENT_D_NAME

/* Define if your system has /etc/default/login */
#undef HAVE_ETC_DEFAULT_LOGIN

/* Define if your getopt(3) defines and uses optreset */
#undef HAVE_GETOPT_OPTRESET

/* Define on *nto-qnx systems */
#undef MISSING_NFDBITS

/* Define on *nto-qnx systems */
#undef MISSING_HOWMANY

/* Define on *nto-qnx systems */
#undef MISSING_FD_MASK

/* Define if you want smartcard support */
#undef SMARTCARD

/* Define if you want smartcard support using sectok */
#undef USE_SECTOK

/* Define if you want smartcard support using OpenSC */
#undef USE_OPENSC

/* Define if you want to use OpenSSL's internally seeded PRNG only */
#undef OPENSSL_PRNG_ONLY

/* Define if you shouldn't strip 'tty' from your ttyname in [uw]tmp */
#undef WITH_ABBREV_NO_TTY

/* Define if you want a different $PATH for the superuser */
#undef SUPERUSER_PATH

/* Path that unprivileged child will chroot() to in privep mode */
#undef PRIVSEP_PATH

/* Define if your platform needs to skip post auth file descriptor passing */
#undef DISABLE_FD_PASSING

/* Silly mkstemp() */
#undef HAVE_STRICT_MKSTEMP

/* Some systems put this outside of libc */
#undef HAVE_NANOSLEEP

/* Define if sshd somehow reacquires a controlling TTY after setsid() */
#undef SSHD_ACQUIRES_CTTY

/* Define if cmsg_type is not passed correctly */
#undef BROKEN_CMSG_TYPE

/*
 * Define to whatever link() returns for "not supported" if it doesn't
 * return EOPNOTSUPP.
 */
#undef LINK_OPNOTSUPP_ERRNO

/* Strings used in /etc/passwd to denote locked account */
#undef LOCKED_PASSWD_STRING
#undef LOCKED_PASSWD_PREFIX
#undef LOCKED_PASSWD_SUBSTR

/* Define if getrrsetbyname() exists */
#undef HAVE_GETRRSETBYNAME

/* Define if HEADER.ad exists in arpa/nameser.h */
#undef HAVE_HEADER_AD

/* Define if your resolver libs need this for getrrsetbyname */
#undef BIND_8_COMPAT

/* Define if you have /proc/$pid/fd */
#undef HAVE_PROC_PID

@BOTTOM@

/* ******************* Shouldn't need to edit below this line ************** */

#endif /* _CONFIG_H */
