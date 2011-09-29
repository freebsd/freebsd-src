/*
 * Copyright (c) 1995 - 2005 Kungliga Tekniska Högskolan
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

/* $Id: roken-common.h 20867 2007-06-03 21:00:45Z lha $ */

#ifndef __ROKEN_COMMON_H__
#define __ROKEN_COMMON_H__

#ifndef ROKEN_LIB_FUNCTION
#ifdef _WIN32
#define ROKEN_LIB_FUNCTION _stdcall
#else
#define ROKEN_LIB_FUNCTION
#endif
#endif

#ifdef __cplusplus
#define ROKEN_CPP_START	extern "C" {
#define ROKEN_CPP_END	}
#else
#define ROKEN_CPP_START
#define ROKEN_CPP_END
#endif

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK 0x7f000001
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

#ifndef F_OK
#define F_OK 0
#endif

#ifndef O_ACCMODE
#define O_ACCMODE	003
#endif

#ifndef _PATH_DEV
#define _PATH_DEV "/dev/"
#endif

#ifndef _PATH_DEVNULL
#define _PATH_DEVNULL "/dev/null"
#endif

#ifndef _PATH_HEQUIV
#define _PATH_HEQUIV "/etc/hosts.equiv"
#endif

#ifndef _PATH_VARRUN
#define _PATH_VARRUN "/var/run/"
#endif

#ifndef _PATH_BSHELL
#define _PATH_BSHELL "/bin/sh"
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN (1024+4)
#endif

#ifndef SIG_ERR
#define SIG_ERR ((RETSIGTYPE (*)(int))-1)
#endif

/*
 * error code for getipnodeby{name,addr}
 */

#ifndef HOST_NOT_FOUND
#define HOST_NOT_FOUND 1
#endif

#ifndef TRY_AGAIN
#define TRY_AGAIN 2
#endif

#ifndef NO_RECOVERY
#define NO_RECOVERY 3
#endif

#ifndef NO_DATA
#define NO_DATA 4
#endif

#ifndef NO_ADDRESS
#define NO_ADDRESS NO_DATA
#endif

/*
 * error code for getaddrinfo
 */

#ifndef EAI_NOERROR
#define EAI_NOERROR	0	/* no error */
#endif

#ifndef EAI_NONAME

#define EAI_ADDRFAMILY	1	/* address family for nodename not supported */
#define EAI_AGAIN	2	/* temporary failure in name resolution */
#define EAI_BADFLAGS	3	/* invalid value for ai_flags */
#define EAI_FAIL	4	/* non-recoverable failure in name resolution */
#define EAI_FAMILY	5	/* ai_family not supported */
#define EAI_MEMORY	6	/* memory allocation failure */
#define EAI_NODATA	7	/* no address associated with nodename */
#define EAI_NONAME	8	/* nodename nor servname provided, or not known */
#define EAI_SERVICE	9	/* servname not supported for ai_socktype */
#define EAI_SOCKTYPE   10	/* ai_socktype not supported */
#define EAI_SYSTEM     11	/* system error returned in errno */

#endif /* EAI_NONAME */

/* flags for getaddrinfo() */

#ifndef AI_PASSIVE
#define AI_PASSIVE	0x01
#define AI_CANONNAME	0x02
#endif /* AI_PASSIVE */

#ifndef AI_NUMERICHOST
#define AI_NUMERICHOST	0x04
#endif

/* flags for getnameinfo() */

#ifndef NI_DGRAM
#define NI_DGRAM	0x01
#define NI_NAMEREQD	0x02
#define NI_NOFQDN	0x04
#define NI_NUMERICHOST	0x08
#define NI_NUMERICSERV	0x10
#endif

/*
 * constants for getnameinfo
 */

#ifndef NI_MAXHOST
#define NI_MAXHOST  1025
#define NI_MAXSERV    32
#endif

/*
 * constants for inet_ntop
 */

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN    16
#endif

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN   46
#endif

/*
 * for shutdown(2)
 */

#ifndef SHUT_RD
#define SHUT_RD 0
#endif

#ifndef SHUT_WR
#define SHUT_WR 1
#endif

#ifndef SHUT_RDWR
#define SHUT_RDWR 2
#endif

#ifndef HAVE___ATTRIBUTE__
#define __attribute__(x)
#endif

ROKEN_CPP_START

#ifndef IRIX4 /* fix for compiler bug */
#ifdef RETSIGTYPE
typedef RETSIGTYPE (*SigAction)(int);
SigAction signal(int iSig, SigAction pAction); /* BSD compatible */
#endif
#endif

int ROKEN_LIB_FUNCTION
simple_execve(const char*, char*const[], char*const[]);

int ROKEN_LIB_FUNCTION
simple_execve_timed(const char *, char *const[], 
		    char *const [], time_t (*)(void *), 
		    void *, time_t);
int ROKEN_LIB_FUNCTION
simple_execvp(const char*, char *const[]);

int ROKEN_LIB_FUNCTION
simple_execvp_timed(const char *, char *const[], 
		    time_t (*)(void *), void *, time_t);
int ROKEN_LIB_FUNCTION
simple_execlp(const char*, ...);

int ROKEN_LIB_FUNCTION
simple_execle(const char*, ...);

int ROKEN_LIB_FUNCTION
simple_execl(const char *file, ...);

int ROKEN_LIB_FUNCTION
wait_for_process(pid_t);

int ROKEN_LIB_FUNCTION
wait_for_process_timed(pid_t, time_t (*)(void *), 
					      void *, time_t);
int ROKEN_LIB_FUNCTION
pipe_execv(FILE**, FILE**, FILE**, const char*, ...);

void ROKEN_LIB_FUNCTION
print_version(const char *);

ssize_t ROKEN_LIB_FUNCTION
eread (int fd, void *buf, size_t nbytes);

ssize_t ROKEN_LIB_FUNCTION
ewrite (int fd, const void *buf, size_t nbytes);

struct hostent;

const char * ROKEN_LIB_FUNCTION
hostent_find_fqdn (const struct hostent *);

void ROKEN_LIB_FUNCTION
esetenv(const char *, const char *, int);

void ROKEN_LIB_FUNCTION
socket_set_address_and_port (struct sockaddr *, const void *, int);

size_t ROKEN_LIB_FUNCTION
socket_addr_size (const struct sockaddr *);

void ROKEN_LIB_FUNCTION
socket_set_any (struct sockaddr *, int);

size_t ROKEN_LIB_FUNCTION
socket_sockaddr_size (const struct sockaddr *);

void * ROKEN_LIB_FUNCTION
socket_get_address (struct sockaddr *);

int ROKEN_LIB_FUNCTION
socket_get_port (const struct sockaddr *);

void ROKEN_LIB_FUNCTION
socket_set_port (struct sockaddr *, int);

void ROKEN_LIB_FUNCTION
socket_set_portrange (int, int, int);

void ROKEN_LIB_FUNCTION
socket_set_debug (int);

void ROKEN_LIB_FUNCTION
socket_set_tos (int, int);

void ROKEN_LIB_FUNCTION
socket_set_reuseaddr (int, int);

void ROKEN_LIB_FUNCTION
socket_set_ipv6only (int, int);

char ** ROKEN_LIB_FUNCTION
vstrcollect(va_list *ap);

char ** ROKEN_LIB_FUNCTION
strcollect(char *first, ...);

void ROKEN_LIB_FUNCTION
timevalfix(struct timeval *t1);

void ROKEN_LIB_FUNCTION
timevaladd(struct timeval *t1, const struct timeval *t2);

void ROKEN_LIB_FUNCTION
timevalsub(struct timeval *t1, const struct timeval *t2);

char *ROKEN_LIB_FUNCTION
pid_file_write (const char *progname);

void ROKEN_LIB_FUNCTION
pid_file_delete (char **);

int ROKEN_LIB_FUNCTION
read_environment(const char *file, char ***env);

void ROKEN_LIB_FUNCTION
free_environment(char **);

void ROKEN_LIB_FUNCTION
warnerr(int doerrno, const char *fmt, va_list ap)
    __attribute__ ((format (printf, 2, 0)));

void * ROKEN_LIB_FUNCTION
rk_realloc(void *, size_t);

struct rk_strpool;

char * ROKEN_LIB_FUNCTION
rk_strpoolcollect(struct rk_strpool *);

struct rk_strpool * ROKEN_LIB_FUNCTION
rk_strpoolprintf(struct rk_strpool *, const char *, ...)
    __attribute__ ((format (printf, 2, 3)));

void ROKEN_LIB_FUNCTION
rk_strpoolfree(struct rk_strpool *);

void ROKEN_LIB_FUNCTION
rk_dumpdata (const char *, const void *, size_t);

ROKEN_CPP_END

#endif /* __ROKEN_COMMON_H__ */
