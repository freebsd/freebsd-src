/* This is (as usual) a generated file,
   it is also machine dependent */

#ifndef __ROKEN_H__
#define __ROKEN_H__

/* -*- C -*- */
/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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
 * 3. Neither the name of the Institute nor the names of its contributors
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

/* $Id: roken.h,v 1.8 1999/12/02 16:58:36 joda Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <time.h>



#define ROKEN_LIB_FUNCTION

#include <roken-common.h>


int putenv(const char *string);

int setenv(const char *var, const char *val, int rewrite);

void unsetenv(const char *name);

char *getusershell(void);
void endusershell(void);

int snprintf (char *str, size_t sz, const char *format, ...)
     __attribute__ ((format (printf, 3, 4)));

int vsnprintf (char *str, size_t sz, const char *format, va_list ap)
     __attribute__((format (printf, 3, 0)));

int asprintf (char **ret, const char *format, ...)
     __attribute__ ((format (printf, 2, 3)));

int vasprintf (char **ret, const char *format, va_list ap)
     __attribute__((format (printf, 2, 0)));

int asnprintf (char **ret, size_t max_sz, const char *format, ...)
     __attribute__ ((format (printf, 3, 4)));

int vasnprintf (char **ret, size_t max_sz, const char *format, va_list ap)
     __attribute__((format (printf, 3, 0)));

char * strdup(const char *old);

char * strlwr(char *);

int strnlen(char*, int);

char *strsep(char**, const char*);

int strcasecmp(const char *s1, const char *s2);



char * strupr(char *);

size_t strlcpy (char *dst, const char *src, size_t dst_sz);

size_t strlcat (char *dst, const char *src, size_t dst_sz);

int getdtablesize(void);

char *strerror(int eno);

/* This causes a fatal error under Psoriasis */
const char *hstrerror(int herr);

extern int h_errno;

int inet_aton(const char *cp, struct in_addr *adr);

char* getcwd(char *path, size_t size);


int seteuid(uid_t euid);

int setegid(gid_t egid);

int lstat(const char *path, struct stat *buf);

int mkstemp(char *);

int initgroups(const char *name, gid_t basegid);

int fchown(int fd, uid_t owner, gid_t group);

int daemon(int nochdir, int noclose);

int innetgr(const char *netgroup, const char *machine, 
	    const char *user, const char *domain);

int chown(const char *path, uid_t owner, gid_t group);

int rcmd(char **ahost, unsigned short inport, const char *locuser,
	 const char *remuser, const char *cmd, int *fd2p);

int innetgr(const char*, const char*, const char*, const char*);

int iruserok(unsigned raddr, int superuser, const char *ruser,
	     const char *luser);

int gethostname(char *name, int namelen);

ssize_t
writev(int d, const struct iovec *iov, int iovcnt);

ssize_t
readv(int d, const struct iovec *iov, int iovcnt);

int
mkstemp(char *template);

#define LOCK_SH   1		/* Shared lock */
#define LOCK_EX   2		/* Exclusive lock */
#define LOCK_NB   4		/* Don't block when locking */
#define LOCK_UN   8		/* Unlock */

int flock(int fd, int operation);

time_t tm2time (struct tm tm, int local);

int unix_verify_user(char *user, char *password);

void inaddr2str(struct in_addr addr, char *s, size_t len);

void mini_inetd (int port);

int roken_concat (char *s, size_t len, ...);

size_t roken_mconcat (char **s, size_t max_len, ...);

int roken_vconcat (char *s, size_t len, va_list args);

size_t roken_vmconcat (char **s, size_t max_len, va_list args);

ssize_t net_write (int fd, const void *buf, size_t nbytes);

ssize_t net_read (int fd, void *buf, size_t nbytes);

int issuid(void);

struct winsize {
	unsigned short ws_row, ws_col;
	unsigned short ws_xpixel, ws_ypixel;
};

int get_window_size(int fd, struct winsize *);

void vsyslog(int pri, const char *fmt, va_list ap);

extern char *optarg;
extern int optind;
extern int opterr;

extern const char *__progname;

extern char **environ;

/*
 * kludges and such
 */

int roken_gethostby_setup(const char*, const char*);
struct hostent* roken_gethostbyname(const char*);
struct hostent* roken_gethostbyaddr(const void*, size_t, int);

#define roken_getservbyname(x,y) getservbyname((char *)x, (char *)y)

#define roken_openlog(a,b,c) openlog((char *)a,b,c)

void set_progname(char *argv0);

#endif /* __ROKEN_H__ */
