/*
 * Copyright (c) 1997-2006 Kungliga Tekniska Högskolan
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

/* $Id: krb5_locl.h 22226 2007-12-08 21:31:53Z lha $ */

#ifndef __KRB5_LOCL_H__
#define __KRB5_LOCL_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#if defined(HAVE_SYS_IOCTL_H) && SunOS != 40
#include <sys/ioctl.h>
#endif
#ifdef HAVE_PWD_H
#undef _POSIX_PTHREAD_SEMANTICS
/* This gets us the 5-arg getpwnam_r on Solaris 9.  */
#define _POSIX_PTHREAD_SEMANTICS
#include <pwd.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <time.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
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
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef _AIX
struct ether_addr;
struct mbuf;
struct sockaddr_dl;
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif
#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#ifdef HAVE_CRYPT_H
#undef des_encrypt
#define des_encrypt wingless_pigs_mostly_fail_to_fly
#include <crypt.h>
#undef des_encrypt
#endif

#ifdef HAVE_DOOR_CREATE
#include <door.h>
#endif

#include <roken.h>
#include <parse_time.h>
#include <base64.h>

#include "crypto-headers.h"


#include <krb5_asn1.h>

struct send_to_kdc;

/* XXX glue for pkinit */
struct krb5_pk_identity;
struct krb5_pk_cert;
struct ContentInfo;
typedef struct krb5_pk_init_ctx_data *krb5_pk_init_ctx;
struct krb5_dh_moduli;

/* v4 glue */
struct _krb5_krb_auth_data;

#include <der.h>

#include <krb5.h>
#include <krb5_err.h>
#include <asn1_err.h>
#ifdef PKINIT
#include <hx509_err.h>
#endif
#include <krb5-private.h>

#include "heim_threads.h"

#define ALLOC(X, N) (X) = calloc((N), sizeof(*(X)))
#define ALLOC_SEQ(X, N) do { (X)->len = (N); ALLOC((X)->val, (N)); } while(0)

/* should this be public? */
#define KEYTAB_DEFAULT "ANY:FILE:" SYSCONFDIR "/krb5.keytab,krb4:" SYSCONFDIR "/srvtab"
#define KEYTAB_DEFAULT_MODIFY "FILE:" SYSCONFDIR "/krb5.keytab"

#define MODULI_FILE SYSCONFDIR "/krb5.moduli"

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define KRB5_BUFSIZ 1024

typedef enum {
    KRB5_INIT_CREDS_TRISTATE_UNSET = 0,
    KRB5_INIT_CREDS_TRISTATE_TRUE,
    KRB5_INIT_CREDS_TRISTATE_FALSE
} krb5_get_init_creds_tristate;

struct _krb5_get_init_creds_opt_private {
    int refcount;
    /* ENC_TIMESTAMP */
    const char *password;
    krb5_s2k_proc key_proc;
    /* PA_PAC_REQUEST */
    krb5_get_init_creds_tristate req_pac;
    /* PKINIT */
    krb5_pk_init_ctx pk_init_ctx;
    KRB_ERROR *error;
    krb5_get_init_creds_tristate addressless;
    int flags;
#define KRB5_INIT_CREDS_CANONICALIZE		1
#define KRB5_INIT_CREDS_NO_C_CANON_CHECK	2
};

typedef struct krb5_context_data {
    krb5_enctype *etypes;
    krb5_enctype *etypes_des;
    char **default_realms;
    time_t max_skew;
    time_t kdc_timeout;
    unsigned max_retries;
    int32_t kdc_sec_offset;
    int32_t kdc_usec_offset;
    krb5_config_section *cf;
    struct et_list *et_list;
    struct krb5_log_facility *warn_dest;
    krb5_cc_ops *cc_ops;
    int num_cc_ops;
    const char *http_proxy;
    const char *time_fmt;
    krb5_boolean log_utc;
    const char *default_keytab;
    const char *default_keytab_modify;
    krb5_boolean use_admin_kdc;
    krb5_addresses *extra_addresses;
    krb5_boolean scan_interfaces;	/* `ifconfig -a' */
    krb5_boolean srv_lookup;		/* do SRV lookups */
    krb5_boolean srv_try_txt;		/* try TXT records also */
    int32_t fcache_vno;			/* create cache files w/ this
                                           version */
    int num_kt_types;			/* # of registered keytab types */
    struct krb5_keytab_data *kt_types;  /* registered keytab types */
    const char *date_fmt;
    char *error_string;
    char error_buf[256];
    krb5_addresses *ignore_addresses;
    char *default_cc_name;
    char *default_cc_name_env;
    int default_cc_name_set;
    void *mutex;			/* protects error_string/error_buf */
    int large_msg_size;
    int flags;
#define KRB5_CTX_F_DNS_CANONICALIZE_HOSTNAME	1
#define KRB5_CTX_F_CHECK_PAC			2
    struct send_to_kdc *send_to_kdc;
} krb5_context_data;

#define KRB5_DEFAULT_CCNAME_FILE "FILE:/tmp/krb5cc_%{uid}"
#define KRB5_DEFAULT_CCNAME_API "API:"
#define KRB5_DEFAULT_CCNAME_KCM "KCM:%{uid}"

#define EXTRACT_TICKET_ALLOW_CNAME_MISMATCH		1
#define EXTRACT_TICKET_ALLOW_SERVER_MISMATCH		2
#define EXTRACT_TICKET_MATCH_REALM			4

/*
 * Configurable options
 */

#ifndef KRB5_DEFAULT_CCTYPE
#ifdef __APPLE__
#define KRB5_DEFAULT_CCTYPE (&krb5_acc_ops)
#else
#define KRB5_DEFAULT_CCTYPE (&krb5_fcc_ops)
#endif
#endif

#ifndef KRB5_ADDRESSLESS_DEFAULT
#define KRB5_ADDRESSLESS_DEFAULT TRUE
#endif

#endif /* __KRB5_LOCL_H__ */
