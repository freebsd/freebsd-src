/* This is an OS dependent, generated file */


#ifndef __ROKEN_H__
#define __ROKEN_H__

/* -*- C -*- */
/*
 * Copyright (c) 1995 - 2002 Kungliga Tekniska Högskolan
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

/* $Id: roken.h.in,v 1.169 2002/08/26 21:43:38 assar Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>

#include <sys/param.h>
#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <grp.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <syslog.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>

#include <paths.h>


#define ROKEN_LIB_FUNCTION


#include <roken-common.h>

ROKEN_CPP_START










int asnprintf (char **ret, size_t max_sz, const char *format, ...)
     __attribute__ ((format (printf, 3, 4)));

int vasnprintf (char **ret, size_t max_sz, const char *format, va_list ap)
     __attribute__((format (printf, 3, 0)));


char * strndup(const char *old, size_t sz);

char * strlwr(char *);

size_t strnlen(const char*, size_t);


ssize_t strsep_copy(const char**, const char*, char*, size_t);




char * strupr(char *);











#include <pwd.h>
struct passwd *k_getpwnam (const char *user);
struct passwd *k_getpwuid (uid_t uid);

const char *get_default_username (void);


















void pidfile (const char*);

unsigned int bswap32(unsigned int);

unsigned short bswap16(unsigned short);


time_t tm2time (struct tm tm, int local);

int unix_verify_user(char *user, char *password);

int roken_concat (char *s, size_t len, ...);

size_t roken_mconcat (char **s, size_t max_len, ...);

int roken_vconcat (char *s, size_t len, va_list args);

size_t roken_vmconcat (char **s, size_t max_len, va_list args);

ssize_t net_write (int fd, const void *buf, size_t nbytes);

ssize_t net_read (int fd, void *buf, size_t nbytes);

int issuid(void);


int get_window_size(int fd, struct winsize *);



extern const char *__progname;

extern char **environ;




struct hostent *
copyhostent (const struct hostent *h);








int
getnameinfo_verified(const struct sockaddr *sa, socklen_t salen,
		     char *host, size_t hostlen,
		     char *serv, size_t servlen,
		     int flags);

int roken_getaddrinfo_hostspec(const char *, int, struct addrinfo **); 
int roken_getaddrinfo_hostspec2(const char *, int, int, struct addrinfo **);



void *emalloc (size_t);
void *ecalloc(size_t num, size_t sz);
void *erealloc (void *, size_t);
char *estrdup (const char *);

/*
 * kludges and such
 */

int roken_gethostby_setup(const char*, const char*);
struct hostent* roken_gethostbyname(const char*);
struct hostent* roken_gethostbyaddr(const void*, size_t, int);

#define roken_getservbyname(x,y) getservbyname(x,y)

#define roken_openlog(a,b,c) openlog(a,b,c)

#define roken_getsockname(a,b,c) getsockname(a,b,c)



void mini_inetd_addrinfo (struct addrinfo*);
void mini_inetd (int port);

void set_progname(char *argv0);
const char *get_progname(void);


int
strsvis(char *dst, const char *src, int flag, const char *extra);




char *
svis(char *dst, int c, int flag, int nextc, const char *extra);



ROKEN_CPP_END
#define ROKEN_VERSION 0.4f

#endif /* __ROKEN_H__ */
