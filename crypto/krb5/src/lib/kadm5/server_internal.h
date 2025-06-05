/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved
 *
 * $Header$
 */

/*
 * This header file is used internally by the Admin API server
 * libraries and Admin server.  IF YOU THINK YOU NEED TO USE THIS FILE
 * FOR ANYTHING, YOU'RE ALMOST CERTAINLY WRONG.
 */

#ifndef __KADM5_SERVER_INTERNAL_H__
#define __KADM5_SERVER_INTERNAL_H__

#include    "autoconf.h"
#ifdef HAVE_MEMORY_H
#include    <memory.h>
#endif
#include    <stdlib.h>
#include    <errno.h>
#include    <kdb.h>
#include    <kadm5/admin.h>
#include    <krb5/plugin.h>
#include    "admin_internal.h"

/*
 * This is the history key version for a newly created DB.  We use this value
 * for principals which have no password history yet to avoid having to look up
 * the history key.  Values other than 2 will cause compatibility issues with
 * pre-1.8 libkadm5 code; the older code will reject key changes when it sees
 * an unexpected value of admin_history_kvno.
 */
#define INITIAL_HIST_KVNO 2

/* A pwqual_handle represents a password quality plugin module. */
typedef struct pwqual_handle_st *pwqual_handle;

typedef struct kadm5_hook_handle_st *kadm5_hook_handle;

typedef struct _kadm5_server_handle_t {
    krb5_ui_4       magic_number;
    krb5_ui_4       struct_version;
    krb5_ui_4       api_version;
    krb5_context    context;
    krb5_principal  current_caller;
    kadm5_config_params  params;
    struct _kadm5_server_handle_t *lhandle;
    char **db_args;
    pwqual_handle   *qual_handles;
    kadm5_hook_handle *hook_handles;
} kadm5_server_handle_rec, *kadm5_server_handle_t;

#define OSA_ADB_PRINC_VERSION_1  0x12345C01

typedef struct _osa_pw_hist_t {
    int n_key_data;
    krb5_key_data *key_data;
} osa_pw_hist_ent, *osa_pw_hist_t;

typedef struct _osa_princ_ent_t {
    int                         version;
    char                        *policy;
    long                        aux_attributes;
    unsigned int                old_key_len;
    unsigned int                old_key_next;
    krb5_kvno                   admin_history_kvno;
    osa_pw_hist_ent             *old_keys;
} osa_princ_ent_rec, *osa_princ_ent_t;


kadm5_ret_t    passwd_check(kadm5_server_handle_t handle,
                            const char *pass, kadm5_policy_ent_t policy,
                            krb5_principal principal);
kadm5_ret_t    principal_exists(krb5_principal principal);
krb5_error_code     kdb_init_master(kadm5_server_handle_t handle,
                                    char *r, int from_keyboard);
krb5_error_code     kdb_get_active_mkey(kadm5_server_handle_t handle,
                                        krb5_kvno *act_kvno_out,
                                        krb5_keyblock **act_mkey_out);
krb5_error_code     kdb_init_hist(kadm5_server_handle_t handle,
                                  char *r);
krb5_error_code     kdb_get_hist_key(kadm5_server_handle_t handle,
                                     krb5_keyblock **keyblocks_out,
                                     krb5_kvno *kvno_out);
void                kdb_free_keyblocks(kadm5_server_handle_t handle,
                                       krb5_keyblock *keyblocks);
krb5_error_code     kdb_get_entry(kadm5_server_handle_t handle,
                                  krb5_principal principal,
                                  krb5_db_entry **kdb, osa_princ_ent_rec *adb);
krb5_error_code     kdb_free_entry(kadm5_server_handle_t handle,
                                   krb5_db_entry *kdb, osa_princ_ent_rec *adb);
krb5_error_code     kdb_put_entry(kadm5_server_handle_t handle,
                                  krb5_db_entry *kdb, osa_princ_ent_rec *adb);
krb5_error_code     kdb_delete_entry(kadm5_server_handle_t handle,
                                     krb5_principal name);
krb5_error_code     kdb_iter_entry(kadm5_server_handle_t handle,
                                   char *match_entry,
                                   void (*iter_fct)(void *, krb5_principal),
                                   void *data);

kadm5_ret_t         init_pwqual(kadm5_server_handle_t handle);
void                destroy_pwqual(kadm5_server_handle_t handle);

/* XXX this ought to be in libkrb5.a, but isn't */
kadm5_ret_t krb5_copy_key_data_contents(krb5_context context,
                                        krb5_key_data *from,
                                        krb5_key_data *to);
kadm5_ret_t krb5_free_key_data_contents(krb5_context context,
                                        krb5_key_data *key);

/*
 * *Warning*
 * *Warning*        This is going to break if we
 * *Warning*        ever go multi-threaded
 * *Warning*
 */
extern  krb5_principal  current_caller;

/*
 * Why is this (or something similar) not defined *anywhere* in krb5?
 */
#define KSUCCESS        0
#define WORD_NOT_FOUND  1

/*
 * all the various mask bits or'd together
 */

#define ALL_PRINC_MASK                                                  \
    (KADM5_PRINCIPAL | KADM5_PRINC_EXPIRE_TIME | KADM5_PW_EXPIRATION |  \
     KADM5_LAST_PWD_CHANGE | KADM5_ATTRIBUTES | KADM5_MAX_LIFE |        \
     KADM5_MOD_TIME | KADM5_MOD_NAME | KADM5_KVNO | KADM5_MKVNO |       \
     KADM5_AUX_ATTRIBUTES | KADM5_POLICY_CLR | KADM5_POLICY |           \
     KADM5_MAX_RLIFE | KADM5_TL_DATA | KADM5_KEY_DATA | KADM5_FAIL_AUTH_COUNT )

#define ALL_POLICY_MASK                                                 \
    (KADM5_POLICY | KADM5_PW_MAX_LIFE | KADM5_PW_MIN_LIFE |             \
     KADM5_PW_MIN_LENGTH | KADM5_PW_MIN_CLASSES | KADM5_PW_HISTORY_NUM | \
     KADM5_REF_COUNT | KADM5_PW_MAX_FAILURE | KADM5_PW_FAILURE_COUNT_INTERVAL | \
     KADM5_PW_LOCKOUT_DURATION | KADM5_POLICY_ATTRIBUTES |              \
     KADM5_POLICY_MAX_LIFE | KADM5_POLICY_MAX_RLIFE |                   \
     KADM5_POLICY_ALLOWED_KEYSALTS | KADM5_POLICY_TL_DATA)

#define SERVER_CHECK_HANDLE(handle)             \
    {                                           \
        kadm5_server_handle_t srvr =            \
            (kadm5_server_handle_t) handle;     \
                                                \
        if (! srvr->current_caller)             \
            return KADM5_BAD_SERVER_HANDLE;     \
        if (! srvr->lhandle)                    \
            return KADM5_BAD_SERVER_HANDLE;     \
    }

#define CHECK_HANDLE(handle)                                    \
    GENERIC_CHECK_HANDLE(handle, KADM5_OLD_SERVER_API_VERSION,  \
                         KADM5_NEW_SERVER_API_VERSION)          \
    SERVER_CHECK_HANDLE(handle)

bool_t          xdr_osa_princ_ent_rec(XDR *xdrs, osa_princ_ent_t objp);

void
osa_free_princ_ent(osa_princ_ent_t val);

/*** Password quality plugin consumer interface ***/

/* Load all available password quality plugin modules, bind each module to the
 * realm's dictionary file, and store the result into *handles_out.  Free the
 * result with k5_pwqual_free_handles. */
krb5_error_code
k5_pwqual_load(krb5_context context, const char *dict_file,
               pwqual_handle **handles_out);

/* Release a handle list allocated by k5_pwqual_load. */
void
k5_pwqual_free_handles(krb5_context context, pwqual_handle *handles);

/* Return the name of a password quality plugin module. */
const char *
k5_pwqual_name(krb5_context context, pwqual_handle handle);

/* Check a password using a password quality plugin module. */
krb5_error_code
k5_pwqual_check(krb5_context context, pwqual_handle handle,
                const char *password, const char *policy_name,
                krb5_principal princ);

/*** initvt functions for built-in password quality modules ***/

/* The dict module checks passwords against the realm's dictionary. */
krb5_error_code
pwqual_dict_initvt(krb5_context context, int maj_ver, int min_ver,
                   krb5_plugin_vtable vtable);

/* The empty module rejects empty passwords (even with no password policy). */
krb5_error_code
pwqual_empty_initvt(krb5_context context, int maj_ver, int min_ver,
                    krb5_plugin_vtable vtable);

/* The hesiod module checks passwords against GECOS fields from Hesiod passwd
 * information (only if the tree was built with Hesiod support). */
krb5_error_code
pwqual_hesiod_initvt(krb5_context context, int maj_ver, int min_ver,
                     krb5_plugin_vtable vtable);

/* The princ module checks passwords against principal components. */
krb5_error_code
pwqual_princ_initvt(krb5_context context, int maj_ver, int min_ver,
                    krb5_plugin_vtable vtable);

/** @{
 * @name kadm5_hook plugin support
 */

/** Load all kadm5_hook plugins. */
krb5_error_code
k5_kadm5_hook_load(krb5_context context,
                   kadm5_hook_handle **handles_out);

/** Free handles allocated by k5_kadm5_hook_load(). */
void
k5_kadm5_hook_free_handles(krb5_context context, kadm5_hook_handle *handles);

/** Call the chpass entry point on every kadm5_hook in @a handles. */
kadm5_ret_t
k5_kadm5_hook_chpass (krb5_context context,
                      kadm5_hook_handle *handles,
                      int stage, krb5_principal princ,
                      krb5_boolean keepold,
                      int n_ks_tuple,
                      krb5_key_salt_tuple *ks_tuple,
                      const char *newpass);

/** Call the create entry point for kadm5_hook_plugins. */
kadm5_ret_t
k5_kadm5_hook_create (krb5_context context,
                      kadm5_hook_handle *handles,
                      int stage,
                      kadm5_principal_ent_t princ, long mask,
                      int n_ks_tuple,
                      krb5_key_salt_tuple *ks_tuple,
                      const char *newpass);

/** Call modify kadm5_hook entry point. */
kadm5_ret_t
k5_kadm5_hook_modify (krb5_context context,
                      kadm5_hook_handle *handles,
                      int stage,
                      kadm5_principal_ent_t princ, long mask);

/** Call remove kadm5_hook entry point. */
kadm5_ret_t
k5_kadm5_hook_remove (krb5_context context,
                      kadm5_hook_handle *handles,
                      int stage,
                      krb5_principal princ);

/** Call rename kadm5_hook entry point. */
kadm5_ret_t
k5_kadm5_hook_rename (krb5_context context,
                      kadm5_hook_handle *handles,
                      int stage,
                      krb5_principal oprinc, krb5_principal nprinc);

/** @}*/

#endif /* __KADM5_SERVER_INTERNAL_H__ */
