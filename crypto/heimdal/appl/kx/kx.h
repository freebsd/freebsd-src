/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska Högskolan
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

/* $Id: kx.h,v 1.39 2001/09/17 01:59:41 assar Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifdef HAVE_GRP_H
#include <grp.h>
#endif
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#elif defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#else
#include <time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xauth.h>

#ifdef HAVE_SYS_STREAM_H
#include <sys/stream.h>
#endif
#ifdef HAVE_SYS_STROPTS_H
#include <sys/stropts.h>
#endif

/* defined by aix's sys/stream.h and again by arpa/nameser.h */

#undef NOERROR

/* as far as we know, this is only used with later versions of Slowlaris */
#if SunOS >= 50 && defined(HAVE_SYS_STROPTS_H) && defined(HAVE_FATTACH) && defined(I_PUSH)
#define MAY_HAVE_X11_PIPES
#endif

#ifdef SOCKS
#include <socks.h>
/* This doesn't belong here. */
struct tm *localtime(const time_t *);
struct hostent  *gethostbyname(const char *);
#endif

#ifdef KRB4
#include <krb.h>
#include <prot.h>
#endif
#ifdef KRB5
#include <krb5.h>
#endif

#include <err.h>
#include <getarg.h>
#include <roken.h>

struct x_socket {
    char *pathname;
    int fd;
    enum {
	LISTENP     = 0x80,
	TCP         = LISTENP | 1,
	UNIX_SOCKET = LISTENP | 2,
	STREAM_PIPE = 3
    } flags;
};

extern char x_socket[];
extern u_int32_t display_num;
extern char display[];
extern int display_size;
extern char xauthfile[];
extern int xauthfile_size;
extern u_char cookie[];
extern size_t cookie_len;

int get_xsockets (int *number, struct x_socket **sockets, int tcpp);
int chown_xsockets (int n, struct x_socket *sockets, uid_t uid, gid_t gid);

int connect_local_xsocket (unsigned dnr);
int create_and_write_cookie (char *xauthfile,
			     size_t size,
			     u_char *cookie,
			     size_t sz);
int verify_and_remove_cookies (int fd, int sock, int cookiesp);
int replace_cookie(int xserver, int fd, char *filename, int cookiesp);

int suspicious_address (int sock, struct sockaddr_in addr);

#define KX_PORT 2111

#define KX_OLD_VERSION "KXSERV.1"
#define KX_VERSION "KXSERV.2"

#define COOKIE_TYPE "MIT-MAGIC-COOKIE-1"

enum { INIT = 0, ACK = 1, NEW_CONN = 2, ERROR = 3 };

enum kx_flags { PASSIVE = 1, KEEP_ALIVE = 2 };

typedef enum kx_flags kx_flags;

struct kx_context {
    int (*authenticate)(struct kx_context *kc, int s);
    int (*userok)(struct kx_context *kc, char *user);
    ssize_t (*read)(struct kx_context *kc,
		    int fd, void *buf, size_t len);
    ssize_t (*write)(struct kx_context *kc,
		     int fd, const void *buf, size_t len);
    int (*copy_encrypted)(struct kx_context *kc,
			  int fd1, int fd2);
    void (*destroy)(struct kx_context *kc);
    const char *host;
    const char *user;
    int port;
    int debug_flag;
    int keepalive_flag;
    int tcp_flag;
    struct sockaddr_in thisaddr, thataddr;
    void *data;
};

typedef struct kx_context kx_context;

void
context_set (kx_context *kc, const char *host, const char *user, int port,
	     int debug_flag, int keepalive_flag, int tcp_flag);

void
context_destroy (kx_context *kc);

int
context_authenticate (kx_context *kc, int s);

int
context_userok (kx_context *kc, char *user);

ssize_t
kx_read (kx_context *kc, int fd, void *buf, size_t len);

ssize_t
kx_write (kx_context *kc, int fd, const void *buf, size_t len);

int
copy_encrypted (kx_context *kc, int fd1, int fd2);

#ifdef KRB4

void
krb4_make_context (kx_context *c);

int
recv_v4_auth (kx_context *kc, int sock, u_char *buf);

#endif

#ifdef KRB5

void
krb5_make_context (kx_context *c);

int
recv_v5_auth (kx_context *kc, int sock, u_char *buf);

#endif

void
fatal (kx_context *kc, int fd, char *format, ...)
#ifdef __GNUC__
__attribute__ ((format (printf, 3, 4)))
#endif
;

#ifndef KRB4

int
krb_get_int(void *f, u_int32_t *to, int size, int lsb);

int
krb_put_int(u_int32_t from, void *to, size_t rem, int size);

#endif
