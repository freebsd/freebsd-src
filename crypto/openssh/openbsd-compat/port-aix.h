/* $Id: port-aix.h,v 1.24 2005/02/16 11:49:31 dtucker Exp $ */

/*
 *
 * Copyright (c) 2001 Gert Doering.  All rights reserved.
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

#ifdef _AIX

#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>	/* for seteuid() */
#endif

#ifdef WITH_AIXAUTHENTICATE
# include <login.h>
# include <userpw.h>
# if defined(HAVE_SYS_AUDIT_H) && defined(AIX_LOGINFAILED_4ARG)
#  include <sys/audit.h>
# endif
# include <usersec.h>
#endif

#include "buffer.h"

/* These should be in the system headers but are not. */
int usrinfo(int, char *, int);
int setauthdb(const char *, char *);
/* these may or may not be in the headers depending on the version */
#if (HAVE_DECL_AUTHENTICATE == 0)
int authenticate(char *, char *, int *, char **);
#endif
#if (HAVE_DECL_LOGINFAILED == 0)
int loginfailed(char *, char *, char *);
#endif
#if (HAVE_DECL_LOGINRESTRICTIONS == 0)
int loginrestrictions(char *, int, char *, char **);
#endif
#if (HAVE_DECL_LOGINSUCCESS == 0)
int loginsuccess(char *, char *, char *, char **);
#endif
#if (HAVE_DECL_PASSWDEXPIRED == 0)
int passwdexpired(char *, char **);
#endif

/* Some versions define r_type in the above headers, which causes a conflict */
#ifdef r_type
# undef r_type
#endif

/* AIX 4.2.x doesn't have nanosleep but does have nsleep which is equivalent */
#if !defined(HAVE_NANOSLEEP) && defined(HAVE_NSLEEP)
# define nanosleep(a,b) nsleep(a,b)
#endif

/* For struct timespec on AIX 4.2.x */
#ifdef HAVE_SYS_TIMERS_H
# include <sys/timers.h>
#endif

/*
 * According to the setauthdb man page, AIX password registries must be 15
 * chars or less plus terminating NUL.
 */
#ifdef HAVE_SETAUTHDB
# define REGISTRY_SIZE	16
#endif

void aix_usrinfo(struct passwd *);

#ifdef WITH_AIXAUTHENTICATE
# define CUSTOM_SYS_AUTH_PASSWD 1
# define CUSTOM_SYS_AUTH_ALLOWED_USER 1
int sys_auth_allowed_user(struct passwd *, Buffer *);
# define CUSTOM_SYS_AUTH_RECORD_LOGIN 1
int sys_auth_record_login(const char *, const char *, const char *, Buffer *);
# define CUSTOM_FAILED_LOGIN 1
#endif

void aix_setauthdb(const char *);
void aix_restoreauthdb(void);
void aix_remove_embedded_newlines(char *);

#if defined(AIX_GETNAMEINFO_HACK) && !defined(BROKEN_GETADDRINFO)
# ifdef getnameinfo
#  undef getnameinfo
# endif
int sshaix_getnameinfo(const struct sockaddr *, size_t, char *, size_t,
    char *, size_t, int);
# define getnameinfo(a,b,c,d,e,f,g) (sshaix_getnameinfo(a,b,c,d,e,f,g))
#endif

#endif /* _AIX */
