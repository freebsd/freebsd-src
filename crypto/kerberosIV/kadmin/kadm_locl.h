/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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

/* $Id: kadm_locl.h,v 1.31 1999/12/02 16:58:36 joda Exp $ */
/* $FreeBSD$ */

#include "config.h"
#include "protos.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

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

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <errno.h>
#include <signal.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif /* HAVE_SYS_RESOURCE_H */
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#include <err.h>

#ifdef SOCKS
#include <socks.h>
/* This doesn't belong here. */
struct tm *localtime(const time_t *);
struct hostent  *gethostbyname(const char *);
#endif

#include <roken.h>

#include <com_err.h>
#include <sl.h>

#define OPENSSL_DES_LIBDES_COMPATIBILITY
#include <openssl/des.h>
#include <krb.h>
#include <krb_err.h>
#include <krb_db.h>
#include <kadm.h>
#include <kadm_err.h>
#include <acl.h>

#include <krb_log.h>

#include "kadm_server.h"
#include "pw_check.h"

/* from libacl */
/* int acl_check(char *acl, char *principal); */

/* GLOBALS */
extern char *acldir;
extern Kadm_Server server_parm;

/* Utils */
int kadm_change (char *, char *, char *, des_cblock);
int kadm_add_entry (char *, char *, char *, Kadm_vals *, Kadm_vals *);
int kadm_mod_entry (char *, char *, char *, Kadm_vals *, Kadm_vals *, Kadm_vals *);
int kadm_get_entry (char *, char *, char *, Kadm_vals *, u_char *, Kadm_vals *);
int kadm_delete_entry (char *, char *, char *, Kadm_vals *);
int kadm_ser_cpw (u_char *, int, AUTH_DAT *, u_char **, int *);
int kadm_ser_add (u_char *, int, AUTH_DAT *, u_char **, int *);
int kadm_ser_mod (u_char *, int, AUTH_DAT *, u_char **, int *);
int kadm_ser_get (u_char *, int, AUTH_DAT *, u_char **, int *);
int kadm_ser_delete (u_char *, int, AUTH_DAT *, u_char **, int *);
int kadm_ser_init (int inter, char realm[], struct in_addr);
int kadm_ser_in (u_char **, int *, u_char *);

int get_pw_new_pwd  (char *pword, int pwlen, krb_principal *pr, int print_realm);

/* cracklib */
char *FascistCheck (char *password, char *path, char **strings);

void
random_password(char *pw, size_t len, u_int32_t *low, u_int32_t *high);
