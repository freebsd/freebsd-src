/*
 * Copyright (c) 1997 - 2000 Kungliga Tekniska Högskolan
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

/* $Id: rsh_locl.h,v 1.24 2000/07/02 15:48:46 assar Exp $ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_IN6_H
#include <netinet/in6.h>
#endif
#ifdef HAVE_NETINET6_IN6_H
#include <netinet6/in6.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifdef HAVE_SHADOW_H
#include <shadow.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#include <errno.h>

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif
#include <err.h>
#include <roken.h>
#include <getarg.h>
#ifdef KRB4
#include <krb.h>
#include <prot.h>
#endif
#include <krb5.h>
#include <kafs.h>

#ifndef _PATH_NOLOGIN
#define _PATH_NOLOGIN   "/etc/nologin"
#endif

#ifndef _PATH_BSHELL
#define _PATH_BSHELL	"/bin/sh"
#endif

#ifndef _PATH_DEFPATH
#define _PATH_DEFPATH	"/usr/bin:/bin"
#endif

#ifndef _PATH_ETC_ENVIRONMENT
#define _PATH_ETC_ENVIRONMENT "/etc/environment"
#endif

/*
 *
 */

enum auth_method { AUTH_KRB4, AUTH_KRB5, AUTH_BROKEN };

extern enum auth_method auth_method;
extern int do_encrypt;
extern krb5_context context;
extern krb5_keyblock *keyblock;
extern krb5_crypto crypto;
#ifdef KRB4
extern des_key_schedule schedule;
extern des_cblock iv;
#endif

#define KCMD_VERSION "KCMDV0.1"

#define USERNAME_SZ 16
#define COMMAND_SZ 1024

#define RSH_BUFSIZ (16 * 1024)

#define PATH_RSH BINDIR "/rsh"

ssize_t do_read (int fd, void *buf, size_t sz);
ssize_t do_write (int fd, void *buf, size_t sz);
