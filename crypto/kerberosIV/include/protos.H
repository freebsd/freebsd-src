/* -*- C -*-
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

/*
 * Add here functions that don't have a prototype on your system.
 *
 * $Id: protos.H,v 1.46 1999/12/02 16:58:36 joda Exp $
 */

#ifdef NEED_CRYPT_PROTO
char *crypt(const char*, const char*);
#endif

#ifdef NEED_STRTOK_R_PROTO
char *strtok_r (char *s1, const char *s2, char **lasts);
#endif

#ifndef HAVE_OPTARG_DECLARATION
extern char *optarg;
#endif
#ifndef HAVE_OPTERR_DECLARATION
extern int opterr;
#endif
#ifndef HAVE_OPTIND_DECLARATION
extern int optind;
#endif
#ifndef HAVE_OPTOPT_DECLARATION
extern int optopt;
#endif

#if defined(__GNUC__) && SunOS == 4

/* To get type fd_set */
#include <sys/types.h>
#include <sys/time.h>

/* To get struct sockaddr, struct in_addr and struct hostent */
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

/* To get struct stat */
#include <sys/stat.h>

/* To get struct utimbuf */
#include <utime.h>

#if !defined(HAVE_ATEXIT) && defined(HAVE_ON_EXIT)
#define atexit(X) on_exit(X, NULL)
#define HAVE_ATEXIT 1
#endif
#ifdef NEED_UTIME_PROTO
int utime(const char *, const struct utimbuf *);
#endif
int syscall(int, ...);
pid_t getpid(void);
int ftruncate(int, off_t);
int fchmod(int, mode_t);
int fchown(int fd, int owner, int group);
int fsync(int);
int seteuid(uid_t);
int setreuid(int, int);
int flock(int, int);
int gettimeofday(struct timeval *tp, struct timezone *tzp);
int lstat(const char *, struct stat *);
int ioctl(int, int, void *); 
int getpriority(int which, int who);
int setpriority(int which, int who, int priority);
int getdtablesize(void);
int initgroups(const char *name, int basegid);
long ulimit(int cmd, long newlimit);
int vhangup(void);

int sigblock(int);
int sigsetmask(int);
int setitimer(int which, struct itimerval *value, struct itimerval *ovalue);

int munmap(caddr_t addr, int len);

int socket(int, int, int);
int setsockopt(int, int, int, void *, int);
int bind(int, void *, int);
int getsockname(int, struct sockaddr *, int *);
int accept(int, struct sockaddr *, int *);
int connect(int, struct sockaddr *, int);
int listen(int, int);
int recv(int s, void *buf, int len, int flags);
int recvfrom(int, char *, int, int, void *, int *);
int sendto(int, const char *, int, int, void *, int);
int select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
int shutdown(int, int);
int getpeername(int, struct sockaddr *, int *);
int getsockopt(int, int, int, void *, int *);
int send(int s, const void *msg, int len, int flags);
struct strbuf;
int getmsg(int fd, struct strbuf *ctlptr, struct strbuf *dataptr, int *flags);

char *inet_ntoa(struct in_addr in);
unsigned long inet_addr(const char *cp);
int gethostname(char *, int);
struct hostent *gethostbyname(const char *);
int dn_expand(const u_char *msg,
	      const u_char *eomorig,
	      const u_char *comp_dn,
	      char *exp_dn,
	      int length);
int res_search(const char *dname,
	       int class,
	       int type,
	       u_char *answer,
	       int anslen);

int yp_get_default_domain (char **outdomain);
int innetgr(const char *netgroup, const char *machine,
	    const char *user, const char *domain);

char *getwd(char *pathname);

void bzero(char *b, int length);
int strcasecmp(const char *, const char *);
void swab(const char *, char *, int);
int atoi(const char *str);
char *mktemp(char *);
void srandom(int seed);
int random(void);

int rcmd(char **, unsigned short, char *, char *, char *, int *);
int rresvport(int *);
int openlog(const char *ident, int logopt, int facility);
int syslog(int priority, const char *message, ...);
int ttyslot(void);

char *getpass(const char *);

char *getusershell(void);
void setpwent();
void endpwent();

#include <stdio.h>
int fclose(FILE *);

#endif /* SunOS4 */

#if SunOS == 5

#include <sys/types.h>
#include <sys/resource.h>

char *getusershell(void);
char *strtok_r(char *, const char *, char **);
int getpriority (int which, id_t who);
int setpriority (int which, id_t who, int prio);
int getdtablesize (void);
char *getusershell(void);
void setusershell(void);
void endusershell(void);

#if defined(__GNUC__)

int syscall(int, ...);
int gethostname(char *, int);

struct timeval;
int gettimeofday(struct timeval *tp, void *);

#endif
#endif

#if defined(__osf__) /* OSF/1 */

#if 0
/* To get type fd_set */
#include <sys/types.h>
#include <sys/time.h>

int select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
int fsync(int fildes);
int gethostname(char *address, int address_len);
int setreuid(int ruid, int euid);
int ioctl(int d, unsigned long request, void * arg);
#endif
int flock(int fildes, int operation);
int syscall(int, ...);

unsigned short htons(unsigned short hostshort);
unsigned int   htonl(unsigned int hostint);
unsigned short ntohs(unsigned short netshort);
unsigned int   ntohl(unsigned int netint);

char *mktemp(char *template);
char *getusershell(void);

int rcmd(char **, unsigned short, char *, char *, char *, int *);
int rresvport (int *port);

#endif /* OSF/1 */

#if defined(__sgi)
#include <sys/types.h>

char *ptsname(int fd);
struct spwd *getspuid(uid_t);
#endif /* IRIX */

#if defined(__GNUC__) && defined(_AIX) /* AIX */

struct timeval;
struct timezone;
int gettimeofday (struct timeval *Tp, void *Tzp);

#endif /* AIX */

#if defined(__GNUC__) && defined(__hpux) /* HP-UX */

int syscall(int, ...);

int vhangup(void);

char *ptsname(int fildes);

void utmpname(const char *file);

int innetgr(const char *netgroup, const char *machine,
	    const char *user, const char *domain);

int dn_comp(char *exp_dn, char *comp_dn, int length,
	    char **dnptrs, char **lastdnptr);

int res_query(char *dname, int class, int type,
	      unsigned char *answer, int anslen);

int dn_expand(char *msg, char *eomorig, char *comp_dn,
	      char *exp_dn, int length);

int res_search(char *dname, int class, int type,
	       unsigned char *answer, int anslen);

#endif /* HP-UX */

#if defined(WIN32)	/* Visual C++ 4.0 (Windows95/NT) */

int open(const char *, int, ...);
int close(int);
int	read(int, void *, unsigned int);
int write(int, const void *, unsigned int);

#endif /* WIN32 */
