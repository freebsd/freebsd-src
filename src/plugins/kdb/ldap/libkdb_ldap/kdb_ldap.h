/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kdb/ldap/libkdb_ldap/kdb_ldap.h */
/*
 * Copyright (c) 2004-2005, Novell, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *   * The copyright holder's name is not used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* */
#ifndef _KDB_LDAP_H
#define _KDB_LDAP_H 1

#include "k5-int.h"
#include <k5-thread.h>
#include <kdb5.h>
#include "ldap_krbcontainer.h"
#include "ldap_realm.h"

/* We want the interfaces marked "deprecated" in OpenLDAP.  */
#define LDAP_DEPRECATED 1
#include <ldap.h>

/* Check for acceptable versions.

   OpenLDAP version 2.2.6 is known to have some kind of problem that
   is tickled by the use of multiple handles in this code.  Version
   2.2.19 in Mac OS 10.4.7 seems to be buggy as well.  Version 2.2.24
   doesn't have this problem.  Other in-between versions have not been
   tested.  */
#ifndef BUILD_WITH_BROKEN_LDAP
# if defined(LDAP_API_FEATURE_X_OPENLDAP)
#  if LDAP_VENDOR_VERSION < 20224
#   error This code triggers bugs in old OpenLDAP implementations.  Please update to 2.2.24 or later.
#  endif
# endif
#endif /* BUILD_WITH_BROKEN_LDAP */

extern struct timeval timelimit;

#define  DEFAULT_CONNS_PER_SERVER    5
#define  REALM_READ_REFRESH_INTERVAL (5 * 60)

#if !defined(LDAP_OPT_RESULT_CODE) && defined(LDAP_OPT_ERROR_NUMBER)
#define LDAP_OPT_RESULT_CODE LDAP_OPT_ERROR_NUMBER
#endif

#define MAXINTLEN  10

#define IGNORE_STATUS              0
#define CHECK_STATUS               1

#define SETUP_CONTEXT() if (context == NULL || context->dal_handle == NULL \
                            || context->dal_handle->db_context == NULL) { \
        return EINVAL;                                                  \
    }                                                                   \
    dal_handle = context->dal_handle;                                   \
    ldap_context = (krb5_ldap_context *) dal_handle->db_context;        \
    if (ldap_context == NULL || ldap_context->server_info_list == NULL) \
        return KRB5_KDB_DBNOTINITED;

#define GET_HANDLE()  ld = NULL;                                        \
    st = krb5_ldap_request_handle_from_pool(ldap_context, &ldap_server_handle); \
    if (st != 0) {                                                      \
        k5_wrapmsg(context, st, KRB5_KDB_ACCESS_ERROR,                  \
                   "LDAP handle unavailable");                          \
        st = KRB5_KDB_ACCESS_ERROR;                                     \
        goto cleanup;                                                   \
    }                                                                   \
    ld = ldap_server_handle->ldap_handle;

extern int set_ldap_error (krb5_context ctx, int st, int op);

#define LDAP_SEARCH(base, scope, filter, attrs)   LDAP_SEARCH_1(base, scope, filter, attrs, CHECK_STATUS)

#define LDAP_SEARCH_1(base, scope, filter, attrs, status_check)         \
    tempst = 0;                                                         \
    st = ldap_search_ext_s(ld, base, scope, filter, attrs, 0, NULL,     \
                           NULL, &timelimit, LDAP_NO_LIMIT, &result);   \
    if (translate_ldap_error(st, OP_SEARCH) == KRB5_KDB_ACCESS_ERROR) { \
        tempst = krb5_ldap_rebind(ldap_context, &ldap_server_handle);   \
        if (ldap_server_handle)                                         \
            ld = ldap_server_handle->ldap_handle;                       \
        if (tempst == 0)                                                \
            st = ldap_search_ext_s(ld, base, scope, filter, attrs, 0,   \
                                   NULL, NULL, &timelimit,              \
                                   LDAP_NO_LIMIT, &result);             \
    }                                                                   \
                                                                        \
    if (status_check != IGNORE_STATUS) {                                \
        if (tempst != 0) {                                              \
            k5_wrapmsg(context, st, KRB5_KDB_ACCESS_ERROR,              \
                       "LDAP handle unavailable");                      \
            st = KRB5_KDB_ACCESS_ERROR;                                 \
            goto cleanup;                                               \
        }                                                               \
        if (st != LDAP_SUCCESS) {                                       \
            st = set_ldap_error(context, st, OP_SEARCH);                \
            goto cleanup;                                               \
        }                                                               \
    }


#define CHECK_CLASS_VALIDITY(st, mask, str)                             \
    if (st != 0 || mask == 0) {                                         \
        if (st == 0 && mask == 0) {                                     \
            st = set_ldap_error(context, LDAP_OBJECT_CLASS_VIOLATION, OP_SEARCH); \
        }                                                               \
        k5_prependmsg(context, st, str);                                \
        goto cleanup;                                                   \
    }

#define CHECK_NULL(ptr) if (ptr == NULL) {      \
        st = ENOMEM;                            \
        goto cleanup;                           \
    }

#define  STORE16_INT(ptr, val)  store_16_be(val, ptr)
#define  STORE32_INT(ptr, val)  store_32_be(val, ptr)
#define UNSTORE16_INT(ptr, val) (val = load_16_be(ptr))
#define UNSTORE32_INT(ptr, val) (val = load_32_be(ptr))

#define  KDB_TL_USER_INFO      0x7ffe

#define KDB_TL_PRINCTYPE          0x01
#define KDB_TL_PRINCCOUNT         0x02
#define KDB_TL_USERDN             0x03
#define KDB_TL_KEYINFO            0x04
#define KDB_TL_MASK               0x05
/* 0x06 was KDB_TL_CONTAINERDN but is no longer used */
#define KDB_TL_LINKDN             0x07


#define CHECK_LDAP_HANDLE(lcontext)     if (!(ldap_context              \
                                              && ldap_context->server_info_list)) { \
        return KRB5_KDB_DBNOTINITED;                                    \
    }

#define HNDL_LOCK(lcontext) k5_mutex_lock(&lcontext->hndl_lock)
#define HNDL_UNLOCK(lcontext) k5_mutex_unlock(&lcontext->hndl_lock)

/* ldap server info structure */

typedef enum _server_type {PRIMARY, SECONDARY} krb5_ldap_server_type;

typedef enum _server_status {OFF, ON, NOTSET} krb5_ldap_server_status;

typedef struct _krb5_ldap_server_info krb5_ldap_server_info;

typedef struct  _krb5_ldap_server_handle {
    int                              msgid;
    LDAP                             *ldap_handle;
    krb5_boolean                     server_info_update_pending;
    krb5_ldap_server_info            *server_info;
    struct _krb5_ldap_server_handle  *next;
} krb5_ldap_server_handle;

struct _krb5_ldap_server_info {
    krb5_ldap_server_type        server_type;
    krb5_ldap_server_status      server_status;
    krb5_ui_4                    num_conns;
    krb5_ldap_server_handle      *ldap_server_handles;
    time_t                       downtime;
    char                        *server_name;
    int                          modify_increment;
    struct _krb5_ldap_server_info *next;
};


/* ldap server structure */

typedef enum {SERVICE_DN_TYPE_SERVER, SERVICE_DN_TYPE_CLIENT} krb5_ldap_servicetype;

typedef struct _krb5_ldap_context {
    krb5_ldap_servicetype         service_type;
    krb5_ldap_server_info         **server_info_list;
    krb5_ui_4                     max_server_conns;
    char                          *conf_section;
    char                          *bind_dn;
    char                          *bind_pwd;
    char                          *service_password_file;
    char                          *sasl_mech;
    char                          *sasl_authcid;
    char                          *sasl_authzid;
    char                          *sasl_realm;
    char                          *root_certificate_file;
    krb5_ui_4                     cert_count; /* certificate count */
    k5_mutex_t                    hndl_lock;
    char                          *container_dn;
    krb5_ldap_realm_params        *lrparams;
    krb5_boolean                  disable_last_success;
    krb5_boolean                  disable_lockout;
    int                           ldap_debug;
    krb5_context                  kcontext;   /* to set the error code and message */
} krb5_ldap_context;


typedef struct {
    int           nkey;
    struct berval **keys;
}KEY;

#define k5ldap_inited(c) (c && c->db_context                            \
                          && ((kdb5_dal_handle*)c->db_context)->db_context \
                          && ((krb5_ldap_context *) ((kdb5_dal_handle*)c->db_context)->db_context))


/* misc functions */

krb5_error_code
krb5_ldap_db_init(krb5_context, krb5_ldap_context *);

krb5_error_code
krb5_ldap_db_single_init(krb5_ldap_context *);

krb5_error_code
krb5_ldap_rebind(krb5_ldap_context *, krb5_ldap_server_handle **);

krb5_error_code
krb5_ldap_get_age(krb5_context, char *, time_t *);

krb5_error_code
krb5_ldap_lib_init(void);

krb5_error_code
krb5_ldap_lib_cleanup(void);

void *
krb5_ldap_alloc( krb5_context kcontext,  void *ptr, size_t size );

void
krb5_ldap_free( krb5_context kcontext, void *ptr );

krb5_error_code
krb5_ldap_create(krb5_context , char *, char **);

krb5_error_code
krb5_ldap_open( krb5_context , char *,
                char **db_args,
                int mode );
krb5_error_code
krb5_ldap_close( krb5_context );

krb5_error_code
krb5_ldap_free_ldap_context(krb5_ldap_context *);

krb5_error_code
krb5_ldap_read_startup_information(krb5_context );

krb5_boolean
has_modify_increment(krb5_context, const char *);

void
krb5_ldap_free_server_context_params(krb5_ldap_context *ldap_context);

krb5_error_code
krb5_ldap_check_policy_as(krb5_context kcontext, krb5_kdc_req *request,
                          krb5_db_entry *client, krb5_db_entry *server,
                          krb5_timestamp kdc_time, const char **status,
                          krb5_pa_data ***e_data);

void
krb5_ldap_audit_as_req(krb5_context kcontext, krb5_kdc_req *request,
                       krb5_db_entry *client, krb5_db_entry *server,
                       krb5_timestamp authtime, krb5_error_code error_code);

krb5_error_code
krb5_ldap_check_allowed_to_delegate(krb5_context context,
                                    krb5_const_principal client,
                                    const krb5_db_entry *server,
                                    krb5_const_principal proxy);

/* DAL functions */


krb5_error_code
krb5_ldap_lock( krb5_context, int );

krb5_error_code
krb5_ldap_unlock( krb5_context );

#ifndef HAVE_LDAP_INITIALIZE
int
ldap_initialize(LDAP **, char *);
#endif
#ifndef HAVE_LDAP_UNBIND_EXT_S
int
ldap_unbind_ext_s(LDAP *, LDAPControl **, LDAPControl **);
#endif

/* lockout.c */
krb5_error_code
krb5_ldap_lockout_check_policy(krb5_context context,
                               krb5_db_entry *entry,
                               krb5_timestamp stamp);

krb5_error_code
krb5_ldap_lockout_audit(krb5_context context,
                        krb5_db_entry *entry,
                        krb5_timestamp stamp,
                        krb5_error_code status);

#endif
