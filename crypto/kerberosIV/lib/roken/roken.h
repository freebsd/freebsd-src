/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $Id: roken.h,v 1.63 1997/05/28 05:38:09 assar Exp $ */

#ifndef __ROKEN_H__
#define __ROKEN_H__

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_GRP_H
#include <grp.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#if defined(HAVE_SYS_IOCTL_H) && SunOS != 4
#include <sys/ioctl.h>
#endif

#include "protos.h"

#if !defined(HAVE_SETSID) && defined(HAVE__SETSID)
#define setsid _setsid
#endif

#ifndef HAVE_PUTENV
int putenv(const char *string);
#endif

#ifndef HAVE_SETENV
int setenv(const char *var, const char *val, int rewrite);
#endif

#ifndef HAVE_UNSETENV
void unsetenv(const char *name);
#endif

#ifndef HAVE_GETUSERSHELL
char *getusershell(void);
#endif

#if !defined(__GNUC__) && !defined(__attribute__)
#define __attribute__(x)
#endif

#ifndef HAVE_SNPRINTF
int snprintf (char *str, size_t sz, const char *format, ...)
     __attribute__ ((format (printf, 3, 4)));
#endif

#ifndef HAVE_VSNPRINTF
int vsnprintf (char *str, size_t sz, const char *format, va_list ap)
     __attribute__((format (printf, 3, 0)));
#endif

#ifndef HAVE_ASPRINTF
int asprintf (char **ret, const char *format, ...)
     __attribute__ ((format (printf, 2, 3)));
#endif

#ifndef HAVE_VASPRINTF
int vasprintf (char **ret, const char *format, va_list ap)
     __attribute__((format (printf, 2, 0)));
#endif

#ifndef HAVE_ASNPRINTF
int asnprintf (char **ret, size_t max_sz, const char *format, ...)
     __attribute__ ((format (printf, 3, 4)));
#endif

#ifndef HAVE_VASNPRINTF
int vasnprintf (char **ret, size_t max_sz, const char *format, va_list ap)
     __attribute__((format (printf, 3, 0)));
#endif

#ifndef HAVE_STRDUP
char * strdup(const char *old);
#endif

#ifndef HAVE_STRLWR
char * strlwr(char *);
#endif

#ifndef HAVE_STRNLEN
int strnlen(char*, int);
#endif

#ifndef HAVE_STRTOK_R
char *strtok_r(char *s1, const char *s2, char **lasts);
#endif

#ifndef HAVE_STRUPR
char * strupr(char *);
#endif

#ifndef HAVE_GETDTABLESIZE
int getdtablesize(void);
#endif

#if IRIX != 4 /* fix for compiler bug */
#ifdef RETSIGTYPE
typedef RETSIGTYPE (*SigAction)(/* int??? */);
SigAction signal(int iSig, SigAction pAction); /* BSD compatible */
#endif
#endif

#ifndef SIG_ERR
#define SIG_ERR ((RETSIGTYPE (*)())-1)
#endif

#if !defined(HAVE_STRERROR) && !defined(strerror)
char *strerror(int eno);
#endif

#ifndef HAVE_HSTRERROR
char *hstrerror(int herr);
#endif

#ifndef HAVE_H_ERRNO_DECLARATION
extern int h_errno;
#endif

#ifndef HAVE_INET_ATON
/* Minimal implementation of inet_aton. Doesn't handle hex numbers. */
int inet_aton(const char *cp, struct in_addr *adr);
#endif

#if !defined(HAVE_GETCWD)
char* getcwd(char *path, size_t size);
#endif

#ifndef HAVE_GETENT
int getent(char *cp, char *name);
#endif

#ifdef HAVE_PWD_H
#include <pwd.h>
struct passwd *k_getpwnam (char *user);
struct passwd *k_getpwuid (uid_t uid);
#endif

#ifndef HAVE_SETEUID
int seteuid(int euid);
#endif

#ifndef HAVE_SETEGID
int setegid(int egid);
#endif

#ifndef HAVE_LSTAT
int lstat(const char *path, struct stat *buf);
#endif

#ifndef HAVE_MKSTEMP
int mkstemp(char *);
#endif

#ifndef HAVE_INITGROUPS
int initgroups(const char *name, gid_t basegid);
#endif

#ifndef HAVE_FCHOWN
int fchown(int fd, uid_t owner, gid_t group);
#endif

#ifndef HAVE_CHOWN
int chown(const char *path, uid_t owner, gid_t group);
#endif

#ifndef HAVE_RCMD
int rcmd(char **ahost, unsigned short inport, const char *locuser,
	 const char *remuser, const char *cmd, int *fd2p);
#endif

#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#elif defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#else
#include <time.h>
#endif

time_t tm2time (struct tm tm, int local);

int unix_verify_user(char *user, char *password);

void inaddr2str(struct in_addr addr, char *s, size_t len);

void mini_inetd (int port);

#ifndef HAVE_STRUCT_WINSIZE
struct winsize {
	unsigned short ws_row, ws_col;
	unsigned short ws_xpixel, ws_ypixel;
};
#endif

int get_window_size(int fd, struct winsize *);

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

#ifndef SOMAXCONN
#define SOMAXCONN 5
#endif

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

#ifndef max
#define max(a,b) (((a)>(b))?(a):(b))
#endif

#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
/* Misc definitions for old syslogs */

#ifndef LOG_DAEMON
#define openlog(id,option,facility) openlog((id),(option))
#define	LOG_DAEMON	0
#endif
#ifndef LOG_ODELAY
#define LOG_ODELAY 0
#endif
#ifndef LOG_NDELAY
#define LOG_NDELAY 0x08
#endif
#ifndef LOG_CONS
#define LOG_CONS 0
#endif
#ifndef LOG_AUTH
#define LOG_AUTH 0
#endif
#ifndef LOG_AUTHPRIV
#define LOG_AUTHPRIV LOG_AUTH
#endif
#endif

#ifndef HAVE_OPTARG_DECLARATION
extern char *optarg;
#endif
#ifndef HAVE_OPTIND_DECLARATION
extern int optind;
#endif
#ifndef HAVE_OPTERR_DECLARATION
extern int opterr;
#endif

#ifndef HAVE___PROGNAME_DECLARATION
extern const char *__progname;
#endif

void set_progname(char *argv0);

#ifdef HAVE_PATHS_H
#include <paths.h>
#endif

#ifndef _PATH_DEVNULL
#define _PATH_DEVNULL "/dev/null"
#endif

#endif /*  __ROKEN_H__ */
