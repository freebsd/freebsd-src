#include <stdio.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

int main()
{
puts("/* This is an OS dependent, generated file */");
puts("\n");
puts("#ifndef __ROKEN_H__");
puts("#define __ROKEN_H__");
puts("");
puts("/* -*- C -*- */");
puts("/*");
puts(" * Copyright (c) 1995 - 2002 Kungliga Tekniska Högskolan");
puts(" * (Royal Institute of Technology, Stockholm, Sweden).");
puts(" * All rights reserved.");
puts(" * ");
puts(" * Redistribution and use in source and binary forms, with or without");
puts(" * modification, are permitted provided that the following conditions");
puts(" * are met:");
puts(" * ");
puts(" * 1. Redistributions of source code must retain the above copyright");
puts(" *    notice, this list of conditions and the following disclaimer.");
puts(" * ");
puts(" * 2. Redistributions in binary form must reproduce the above copyright");
puts(" *    notice, this list of conditions and the following disclaimer in the");
puts(" *    documentation and/or other materials provided with the distribution.");
puts(" * ");
puts(" * 3. Neither the name of the Institute nor the names of its contributors");
puts(" *    may be used to endorse or promote products derived from this software");
puts(" *    without specific prior written permission.");
puts(" * ");
puts(" * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND");
puts(" * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE");
puts(" * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE");
puts(" * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE");
puts(" * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL");
puts(" * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS");
puts(" * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)");
puts(" * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT");
puts(" * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY");
puts(" * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF");
puts(" * SUCH DAMAGE.");
puts(" */");
puts("");
puts("/* $Id: roken.h.in,v 1.169 2002/08/26 21:43:38 assar Exp $ */");
puts("");
puts("#include <stdio.h>");
puts("#include <stdlib.h>");
puts("#include <stdarg.h>");
puts("#include <string.h>");
puts("#include <signal.h>");
puts("");
#ifdef _AIX
puts("struct ether_addr;");
puts("struct sockaddr_dl;");
#endif
#ifdef HAVE_SYS_PARAM_H
puts("#include <sys/param.h>");
#endif
#ifdef HAVE_INTTYPES_H
puts("#include <inttypes.h>");
#endif
#ifdef HAVE_SYS_TYPES_H
puts("#include <sys/types.h>");
#endif
#ifdef HAVE_SYS_BITYPES_H
puts("#include <sys/bitypes.h>");
#endif
#ifdef HAVE_BIND_BITYPES_H
puts("#include <bind/bitypes.h>");
#endif
#ifdef HAVE_NETINET_IN6_MACHTYPES_H
puts("#include <netinet/in6_machtypes.h>");
#endif
#ifdef HAVE_UNISTD_H
puts("#include <unistd.h>");
#endif
#ifdef HAVE_SYS_SOCKET_H
puts("#include <sys/socket.h>");
#endif
#ifdef HAVE_SYS_UIO_H
puts("#include <sys/uio.h>");
#endif
#ifdef HAVE_GRP_H
puts("#include <grp.h>");
#endif
#ifdef HAVE_SYS_STAT_H
puts("#include <sys/stat.h>");
#endif
#ifdef HAVE_NETINET_IN_H
puts("#include <netinet/in.h>");
#endif
#ifdef HAVE_NETINET_IN6_H
puts("#include <netinet/in6.h>");
#endif
#ifdef HAVE_NETINET6_IN6_H
puts("#include <netinet6/in6.h>");
#endif
#ifdef HAVE_ARPA_INET_H
puts("#include <arpa/inet.h>");
#endif
#ifdef HAVE_NETDB_H
puts("#include <netdb.h>");
#endif
#ifdef HAVE_ARPA_NAMESER_H
puts("#include <arpa/nameser.h>");
#endif
#ifdef HAVE_RESOLV_H
puts("#include <resolv.h>");
#endif
#ifdef HAVE_SYSLOG_H
puts("#include <syslog.h>");
#endif
#ifdef HAVE_FCNTL_H
puts("#include <fcntl.h>");
#endif
#ifdef HAVE_ERRNO_H
puts("#include <errno.h>");
#endif
#ifdef HAVE_ERR_H
puts("#include <err.h>");
#endif
#ifdef HAVE_TERMIOS_H
puts("#include <termios.h>");
#endif
#if defined(HAVE_SYS_IOCTL_H) && SunOS != 40
puts("#include <sys/ioctl.h>");
#endif
#ifdef TIME_WITH_SYS_TIME
puts("#include <sys/time.h>");
puts("#include <time.h>");
#elif defined(HAVE_SYS_TIME_H)
puts("#include <sys/time.h>");
#else
puts("#include <time.h>");
#endif
puts("");
#ifdef HAVE_PATHS_H
puts("#include <paths.h>");
#endif
puts("");
puts("");
#ifndef ROKEN_LIB_FUNCTION
#if defined(__BORLANDC__)
puts("#define ROKEN_LIB_FUNCTION /* not-ready-definition-yet */");
#elif defined(_MSC_VER)
puts("#define ROKEN_LIB_FUNCTION /* not-ready-definition-yet2 */");
#else
puts("#define ROKEN_LIB_FUNCTION");
#endif
#endif
puts("");
#ifndef HAVE_SSIZE_T
puts("typedef int ssize_t;");
#endif
puts("");
puts("#include <roken-common.h>");
puts("");
puts("ROKEN_CPP_START");
puts("");
#if !defined(HAVE_SETSID) && defined(HAVE__SETSID)
puts("#define setsid _setsid");
#endif
puts("");
#ifndef HAVE_PUTENV
puts("int putenv(const char *string);");
#endif
puts("");
#if !defined(HAVE_SETENV) || defined(NEED_SETENV_PROTO)
puts("int setenv(const char *var, const char *val, int rewrite);");
#endif
puts("");
#if !defined(HAVE_UNSETENV) || defined(NEED_UNSETENV_PROTO)
puts("void unsetenv(const char *name);");
#endif
puts("");
#if !defined(HAVE_GETUSERSHELL) || defined(NEED_GETUSERSHELL_PROTO)
puts("char *getusershell(void);");
puts("void endusershell(void);");
#endif
puts("");
#if !defined(HAVE_SNPRINTF) || defined(NEED_SNPRINTF_PROTO)
puts("int snprintf (char *str, size_t sz, const char *format, ...)");
puts("     __attribute__ ((format (printf, 3, 4)));");
#endif
puts("");
#if !defined(HAVE_VSNPRINTF) || defined(NEED_VSNPRINTF_PROTO)
puts("int vsnprintf (char *str, size_t sz, const char *format, va_list ap)");
puts("     __attribute__((format (printf, 3, 0)));");
#endif
puts("");
#if !defined(HAVE_ASPRINTF) || defined(NEED_ASPRINTF_PROTO)
puts("int asprintf (char **ret, const char *format, ...)");
puts("     __attribute__ ((format (printf, 2, 3)));");
#endif
puts("");
#if !defined(HAVE_VASPRINTF) || defined(NEED_VASPRINTF_PROTO)
puts("int vasprintf (char **ret, const char *format, va_list ap)");
puts("     __attribute__((format (printf, 2, 0)));");
#endif
puts("");
#if !defined(HAVE_ASNPRINTF) || defined(NEED_ASNPRINTF_PROTO)
puts("int asnprintf (char **ret, size_t max_sz, const char *format, ...)");
puts("     __attribute__ ((format (printf, 3, 4)));");
#endif
puts("");
#if !defined(HAVE_VASNPRINTF) || defined(NEED_VASNPRINTF_PROTO)
puts("int vasnprintf (char **ret, size_t max_sz, const char *format, va_list ap)");
puts("     __attribute__((format (printf, 3, 0)));");
#endif
puts("");
#ifndef HAVE_STRDUP
puts("char * strdup(const char *old);");
#endif
puts("");
#if !defined(HAVE_STRNDUP) || defined(NEED_STRNDUP_PROTO)
puts("char * strndup(const char *old, size_t sz);");
#endif
puts("");
#ifndef HAVE_STRLWR
puts("char * strlwr(char *);");
#endif
puts("");
#ifndef HAVE_STRNLEN
puts("size_t strnlen(const char*, size_t);");
#endif
puts("");
#if !defined(HAVE_STRSEP) || defined(NEED_STRSEP_PROTO)
puts("char *strsep(char**, const char*);");
#endif
puts("");
#if !defined(HAVE_STRSEP_COPY) || defined(NEED_STRSEP_COPY_PROTO)
puts("ssize_t strsep_copy(const char**, const char*, char*, size_t);");
#endif
puts("");
#ifndef HAVE_STRCASECMP
puts("int strcasecmp(const char *s1, const char *s2);");
#endif
puts("");
#ifdef NEED_FCLOSE_PROTO
puts("int fclose(FILE *);");
#endif
puts("");
#ifdef NEED_STRTOK_R_PROTO
puts("char *strtok_r(char *s1, const char *s2, char **lasts);");
#endif
puts("");
#ifndef HAVE_STRUPR
puts("char * strupr(char *);");
#endif
puts("");
#ifndef HAVE_STRLCPY
puts("size_t strlcpy (char *dst, const char *src, size_t dst_sz);");
#endif
puts("");
#ifndef HAVE_STRLCAT
puts("size_t strlcat (char *dst, const char *src, size_t dst_sz);");
#endif
puts("");
#ifndef HAVE_GETDTABLESIZE
puts("int getdtablesize(void);");
#endif
puts("");
#if !defined(HAVE_STRERROR) && !defined(strerror)
puts("char *strerror(int eno);");
#endif
puts("");
#if !defined(HAVE_HSTRERROR) || defined(NEED_HSTRERROR_PROTO)
puts("/* This causes a fatal error under Psoriasis */");
#if !(defined(SunOS) && (SunOS >= 50))
puts("const char *hstrerror(int herr);");
#endif
#endif
puts("");
#ifndef HAVE_H_ERRNO_DECLARATION
puts("extern int h_errno;");
#endif
puts("");
#if !defined(HAVE_INET_ATON) || defined(NEED_INET_ATON_PROTO)
puts("int inet_aton(const char *cp, struct in_addr *adr);");
#endif
puts("");
#ifndef HAVE_INET_NTOP
puts("const char *");
puts("inet_ntop(int af, const void *src, char *dst, size_t size);");
#endif
puts("");
#ifndef HAVE_INET_PTON
puts("int");
puts("inet_pton(int af, const char *src, void *dst);");
#endif
puts("");
#if !defined(HAVE_GETCWD)
puts("char* getcwd(char *path, size_t size);");
#endif
puts("");
#ifdef HAVE_PWD_H
puts("#include <pwd.h>");
puts("struct passwd *k_getpwnam (const char *user);");
puts("struct passwd *k_getpwuid (uid_t uid);");
#endif
puts("");
puts("const char *get_default_username (void);");
puts("");
#ifndef HAVE_SETEUID
puts("int seteuid(uid_t euid);");
#endif
puts("");
#ifndef HAVE_SETEGID
puts("int setegid(gid_t egid);");
#endif
puts("");
#ifndef HAVE_LSTAT
puts("int lstat(const char *path, struct stat *buf);");
#endif
puts("");
#if !defined(HAVE_MKSTEMP) || defined(NEED_MKSTEMP_PROTO)
puts("int mkstemp(char *);");
#endif
puts("");
#ifndef HAVE_CGETENT
puts("int cgetent(char **buf, char **db_array, const char *name);");
puts("int cgetstr(char *buf, const char *cap, char **str);");
#endif
puts("");
#ifndef HAVE_INITGROUPS
puts("int initgroups(const char *name, gid_t basegid);");
#endif
puts("");
#ifndef HAVE_FCHOWN
puts("int fchown(int fd, uid_t owner, gid_t group);");
#endif
puts("");
#ifndef HAVE_DAEMON
puts("int daemon(int nochdir, int noclose);");
#endif
puts("");
#ifndef HAVE_INNETGR
puts("int innetgr(const char *netgroup, const char *machine, ");
puts("	    const char *user, const char *domain);");
#endif
puts("");
#ifndef HAVE_CHOWN
puts("int chown(const char *path, uid_t owner, gid_t group);");
#endif
puts("");
#ifndef HAVE_RCMD
puts("int rcmd(char **ahost, unsigned short inport, const char *locuser,");
puts("	 const char *remuser, const char *cmd, int *fd2p);");
#endif
puts("");
#if !defined(HAVE_INNETGR) || defined(NEED_INNETGR_PROTO)
puts("int innetgr(const char*, const char*, const char*, const char*);");
#endif
puts("");
#ifndef HAVE_IRUSEROK
puts("int iruserok(unsigned raddr, int superuser, const char *ruser,");
puts("	     const char *luser);");
#endif
puts("");
#if !defined(HAVE_GETHOSTNAME) || defined(NEED_GETHOSTNAME_PROTO)
puts("int gethostname(char *name, int namelen);");
#endif
puts("");
#ifndef HAVE_WRITEV
puts("ssize_t");
puts("writev(int d, const struct iovec *iov, int iovcnt);");
#endif
puts("");
#ifndef HAVE_READV
puts("ssize_t");
puts("readv(int d, const struct iovec *iov, int iovcnt);");
#endif
puts("");
#ifndef HAVE_MKSTEMP
puts("int");
puts("mkstemp(char *template);");
#endif
puts("");
#ifndef HAVE_PIDFILE
puts("void pidfile (const char*);");
#endif
puts("");
#ifndef HAVE_BSWAP32
puts("unsigned int bswap32(unsigned int);");
#endif
puts("");
#ifndef HAVE_BSWAP16
puts("unsigned short bswap16(unsigned short);");
#endif
puts("");
#ifndef HAVE_FLOCK
#ifndef LOCK_SH
puts("#define LOCK_SH   1		/* Shared lock */");
#endif
#ifndef	LOCK_EX
puts("#define LOCK_EX   2		/* Exclusive lock */");
#endif
#ifndef LOCK_NB
puts("#define LOCK_NB   4		/* Don't block when locking */");
#endif
#ifndef LOCK_UN
puts("#define LOCK_UN   8		/* Unlock */");
#endif
puts("");
puts("int flock(int fd, int operation);");
#endif /* HAVE_FLOCK */
puts("");
puts("time_t tm2time (struct tm tm, int local);");
puts("");
puts("int unix_verify_user(char *user, char *password);");
puts("");
puts("int roken_concat (char *s, size_t len, ...);");
puts("");
puts("size_t roken_mconcat (char **s, size_t max_len, ...);");
puts("");
puts("int roken_vconcat (char *s, size_t len, va_list args);");
puts("");
puts("size_t roken_vmconcat (char **s, size_t max_len, va_list args);");
puts("");
puts("ssize_t net_write (int fd, const void *buf, size_t nbytes);");
puts("");
puts("ssize_t net_read (int fd, void *buf, size_t nbytes);");
puts("");
puts("int issuid(void);");
puts("");
#ifndef HAVE_STRUCT_WINSIZE
puts("struct winsize {");
puts("	unsigned short ws_row, ws_col;");
puts("	unsigned short ws_xpixel, ws_ypixel;");
puts("};");
#endif
puts("");
puts("int get_window_size(int fd, struct winsize *);");
puts("");
#ifndef HAVE_VSYSLOG
puts("void vsyslog(int pri, const char *fmt, va_list ap);");
#endif
puts("");
#ifndef HAVE_OPTARG_DECLARATION
puts("extern char *optarg;");
#endif
#ifndef HAVE_OPTIND_DECLARATION
puts("extern int optind;");
#endif
#ifndef HAVE_OPTERR_DECLARATION
puts("extern int opterr;");
#endif
puts("");
#ifndef HAVE___PROGNAME_DECLARATION
puts("extern const char *__progname;");
#endif
puts("");
#ifndef HAVE_ENVIRON_DECLARATION
puts("extern char **environ;");
#endif
puts("");
#ifndef HAVE_GETIPNODEBYNAME
puts("struct hostent *");
puts("getipnodebyname (const char *name, int af, int flags, int *error_num);");
#endif
puts("");
#ifndef HAVE_GETIPNODEBYADDR
puts("struct hostent *");
puts("getipnodebyaddr (const void *src, size_t len, int af, int *error_num);");
#endif
puts("");
#ifndef HAVE_FREEHOSTENT
puts("void");
puts("freehostent (struct hostent *h);");
#endif
puts("");
#ifndef HAVE_COPYHOSTENT
puts("struct hostent *");
puts("copyhostent (const struct hostent *h);");
#endif
puts("");
#ifndef HAVE_SOCKLEN_T
puts("typedef int socklen_t;");
#endif
puts("");
#ifndef HAVE_STRUCT_SOCKADDR_STORAGE
puts("");
#ifndef HAVE_SA_FAMILY_T
puts("typedef unsigned short sa_family_t;");
#endif
puts("");
#ifdef HAVE_IPV6
puts("#define _SS_MAXSIZE sizeof(struct sockaddr_in6)");
#else
puts("#define _SS_MAXSIZE sizeof(struct sockaddr_in)");
#endif
puts("");
puts("#define _SS_ALIGNSIZE	sizeof(unsigned long)");
puts("");
#if HAVE_STRUCT_SOCKADDR_SA_LEN
puts("");
puts("typedef unsigned char roken_sa_family_t;");
puts("");
puts("#define _SS_PAD1SIZE   ((2 * _SS_ALIGNSIZE - sizeof (roken_sa_family_t) - sizeof(unsigned char)) % _SS_ALIGNSIZE)");
puts("#define _SS_PAD2SIZE   (_SS_MAXSIZE - (sizeof (roken_sa_family_t) + sizeof(unsigned char) + _SS_PAD1SIZE + _SS_ALIGNSIZE))");
puts("");
puts("struct sockaddr_storage {");
puts("    unsigned char	ss_len;");
puts("    roken_sa_family_t	ss_family;");
puts("    char		__ss_pad1[_SS_PAD1SIZE];");
puts("    unsigned long	__ss_align[_SS_PAD2SIZE / sizeof(unsigned long) + 1];");
puts("};");
puts("");
#else /* !HAVE_STRUCT_SOCKADDR_SA_LEN */
puts("");
puts("typedef unsigned short roken_sa_family_t;");
puts("");
puts("#define _SS_PAD1SIZE   ((2 * _SS_ALIGNSIZE - sizeof (roken_sa_family_t)) % _SS_ALIGNSIZE)");
puts("#define _SS_PAD2SIZE   (_SS_MAXSIZE - (sizeof (roken_sa_family_t) + _SS_PAD1SIZE + _SS_ALIGNSIZE))");
puts("");
puts("struct sockaddr_storage {");
puts("    roken_sa_family_t	ss_family;");
puts("    char		__ss_pad1[_SS_PAD1SIZE];");
puts("    unsigned long	__ss_align[_SS_PAD2SIZE / sizeof(unsigned long) + 1];");
puts("};");
puts("");
#endif /* HAVE_STRUCT_SOCKADDR_SA_LEN */
puts("");
#endif /* HAVE_STRUCT_SOCKADDR_STORAGE */
puts("");
#ifndef HAVE_STRUCT_ADDRINFO
puts("struct addrinfo {");
puts("    int    ai_flags;");
puts("    int    ai_family;");
puts("    int    ai_socktype;");
puts("    int    ai_protocol;");
puts("    size_t ai_addrlen;");
puts("    char  *ai_canonname;");
puts("    struct sockaddr *ai_addr;");
puts("    struct addrinfo *ai_next;");
puts("};");
#endif
puts("");
#ifndef HAVE_GETADDRINFO
puts("int");
puts("getaddrinfo(const char *nodename,");
puts("	    const char *servname,");
puts("	    const struct addrinfo *hints,");
puts("	    struct addrinfo **res);");
#endif
puts("");
#ifndef HAVE_GETNAMEINFO
puts("int getnameinfo(const struct sockaddr *sa, socklen_t salen,");
puts("		char *host, size_t hostlen,");
puts("		char *serv, size_t servlen,");
puts("		int flags);");
#endif
puts("");
#ifndef HAVE_FREEADDRINFO
puts("void");
puts("freeaddrinfo(struct addrinfo *ai);");
#endif
puts("");
#ifndef HAVE_GAI_STRERROR
puts("char *");
puts("gai_strerror(int ecode);");
#endif
puts("");
puts("int");
puts("getnameinfo_verified(const struct sockaddr *sa, socklen_t salen,");
puts("		     char *host, size_t hostlen,");
puts("		     char *serv, size_t servlen,");
puts("		     int flags);");
puts("");
puts("int roken_getaddrinfo_hostspec(const char *, int, struct addrinfo **); ");
puts("int roken_getaddrinfo_hostspec2(const char *, int, int, struct addrinfo **);");
puts("");
#ifndef HAVE_STRFTIME
puts("size_t");
puts("strftime (char *buf, size_t maxsize, const char *format,");
puts("	  const struct tm *tm);");
#endif
puts("");
#ifndef HAVE_STRPTIME
puts("char *");
puts("strptime (const char *buf, const char *format, struct tm *timeptr);");
#endif
puts("");
#ifndef HAVE_EMALLOC
puts("void *emalloc (size_t);");
#endif
#ifndef HAVE_ECALLOC
puts("void *ecalloc(size_t num, size_t sz);");
#endif
#ifndef HAVE_EREALLOC
puts("void *erealloc (void *, size_t);");
#endif
#ifndef HAVE_ESTRDUP
puts("char *estrdup (const char *);");
#endif
puts("");
puts("/*");
puts(" * kludges and such");
puts(" */");
puts("");
#if 1
puts("int roken_gethostby_setup(const char*, const char*);");
puts("struct hostent* roken_gethostbyname(const char*);");
puts("struct hostent* roken_gethostbyaddr(const void*, size_t, int);");
#else
#ifdef GETHOSTBYNAME_PROTO_COMPATIBLE
puts("#define roken_gethostbyname(x) gethostbyname(x)");
#else
puts("#define roken_gethostbyname(x) gethostbyname((char *)x)");
#endif
puts("");
#ifdef GETHOSTBYADDR_PROTO_COMPATIBLE
puts("#define roken_gethostbyaddr(a, l, t) gethostbyaddr(a, l, t)");
#else
puts("#define roken_gethostbyaddr(a, l, t) gethostbyaddr((char *)a, l, t)");
#endif
#endif
puts("");
#ifdef GETSERVBYNAME_PROTO_COMPATIBLE
puts("#define roken_getservbyname(x,y) getservbyname(x,y)");
#else
puts("#define roken_getservbyname(x,y) getservbyname((char *)x, (char *)y)");
#endif
puts("");
#ifdef OPENLOG_PROTO_COMPATIBLE
puts("#define roken_openlog(a,b,c) openlog(a,b,c)");
#else
puts("#define roken_openlog(a,b,c) openlog((char *)a,b,c)");
#endif
puts("");
#ifdef GETSOCKNAME_PROTO_COMPATIBLE
puts("#define roken_getsockname(a,b,c) getsockname(a,b,c)");
#else
puts("#define roken_getsockname(a,b,c) getsockname(a, b, (void*)c)");
#endif
puts("");
#ifndef HAVE_SETPROGNAME
puts("void setprogname(const char *argv0);");
#endif
puts("");
#ifndef HAVE_GETPROGNAME
puts("const char *getprogname(void);");
#endif
puts("");
puts("void mini_inetd_addrinfo (struct addrinfo*);");
puts("void mini_inetd (int port);");
puts("");
puts("void set_progname(char *argv0);");
puts("const char *get_progname(void);");
puts("");
#ifndef HAVE_LOCALTIME_R
puts("struct tm *");
puts("localtime_r(const time_t *timer, struct tm *result);");
#endif
puts("");
#if !defined(HAVE_STRSVIS) || defined(NEED_STRSVIS_PROTO)
puts("int");
puts("strsvis(char *dst, const char *src, int flag, const char *extra);");
#endif
puts("");
#if !defined(HAVE_STRUNVIS) || defined(NEED_STRUNVIS_PROTO)
puts("int");
puts("strunvis(char *dst, const char *src);");
#endif
puts("");
#if !defined(HAVE_STRVIS) || defined(NEED_STRVIS_PROTO)
puts("int");
puts("strvis(char *dst, const char *src, int flag);");
#endif
puts("");
#if !defined(HAVE_STRVISX) || defined(NEED_STRVISX_PROTO)
puts("int");
puts("strvisx(char *dst, const char *src, size_t len, int flag);");
#endif
puts("");
#if !defined(HAVE_SVIS) || defined(NEED_SVIS_PROTO)
puts("char *");
puts("svis(char *dst, int c, int flag, int nextc, const char *extra);");
#endif
puts("");
#if !defined(HAVE_UNVIS) || defined(NEED_UNVIS_PROTO)
puts("int");
puts("unvis(char *cp, int c, int *astate, int flag);");
#endif
puts("");
#if !defined(HAVE_VIS) || defined(NEED_VIS_PROTO)
puts("char *");
puts("vis(char *dst, int c, int flag, int nextc);");
#endif
puts("");
puts("ROKEN_CPP_END");
puts("#define ROKEN_VERSION " VERSION );
puts("");
puts("#endif /* __ROKEN_H__ */");
return 0;
}
