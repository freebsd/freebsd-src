/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2006 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/**********************************************************************
 *
 *       C %name:                db2_exp.c %
 *       Instance:               idc_sec_2
 *       Description:
 *       %created_by:    spradeep %
 *       %date_created:  Tue Apr  5 11:44:00 2005 %
 *
 **********************************************************************/
#include "k5-int.h"

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <db.h>
#include <stdio.h>
#include <errno.h>
#include <utime.h>
#include "kdb5.h"
#include "kdb_db2.h"
#include "kdb_xdr.h"
#include "policy_db.h"

/* Quick and dirty wrapper functions to provide for thread safety
   within the plugin, instead of making the kdb5 library do it.  Eventually
   these should be integrated into the real functions.

   Some of the functions wrapped here are also called directly from
   within this library (e.g., create calls open), so simply dropping
   locking code into the top and bottom of each referenced function
   won't do.  (We aren't doing recursive locks, currently.)  */

k5_mutex_t *krb5_db2_mutex;

#define WRAP(NAME,TYPE,ARGLIST,ARGNAMES)                \
    static TYPE wrap_##NAME ARGLIST                     \
    {                                                   \
        TYPE result;                                    \
        k5_mutex_lock (krb5_db2_mutex);                 \
        result = NAME ARGNAMES;                         \
        k5_mutex_unlock (krb5_db2_mutex);               \
        return result;                                  \
    }                                                   \
    /* hack: decl to allow a following ";" */           \
    static TYPE wrap_##NAME ()

/* Two special cases: void (can't assign result), and krb5_error_code
   (return error from locking code).  */

#define WRAP_VOID(NAME,ARGLIST,ARGNAMES)                \
    static void wrap_##NAME ARGLIST                     \
    {                                                   \
        k5_mutex_lock (krb5_db2_mutex);                 \
        NAME ARGNAMES;                                  \
        k5_mutex_unlock (krb5_db2_mutex);               \
    }                                                   \
    /* hack: decl to allow a following ";" */           \
    static void wrap_##NAME ()

#define WRAP_K(NAME,ARGLIST,ARGNAMES)                   \
    WRAP(NAME,krb5_error_code,ARGLIST,ARGNAMES)

WRAP_K (krb5_db2_open,
        ( krb5_context kcontext,
          char *conf_section,
          char **db_args,
          int mode ),
        (kcontext, conf_section, db_args, mode));
WRAP_K (krb5_db2_fini, (krb5_context ctx), (ctx));
WRAP_K (krb5_db2_create,
        ( krb5_context kcontext, char *conf_section, char **db_args ),
        (kcontext, conf_section, db_args));
WRAP_K (krb5_db2_destroy,
        ( krb5_context kcontext, char *conf_section, char **db_args ),
        (kcontext, conf_section, db_args));
WRAP_K (krb5_db2_get_age,
        (krb5_context ctx,
         char *s,
         time_t *t),
        (ctx, s, t));

WRAP_K (krb5_db2_lock,
        ( krb5_context    context,
          int             in_mode),
        (context, in_mode));
WRAP_K (krb5_db2_unlock, (krb5_context ctx), (ctx));

WRAP_K (krb5_db2_get_principal,
        (krb5_context ctx,
         krb5_const_principal p,
         unsigned int f,
         krb5_db_entry **d),
        (ctx, p, f, d));
WRAP_K (krb5_db2_put_principal,
        (krb5_context ctx,
         krb5_db_entry *d,
         char **db_args),
        (ctx, d, db_args));
WRAP_K (krb5_db2_delete_principal,
        (krb5_context context,
         krb5_const_principal searchfor),
        (context, searchfor));

WRAP_K (krb5_db2_iterate,
        (krb5_context ctx, char *s,
         krb5_error_code (*f) (krb5_pointer,
                               krb5_db_entry *),
         krb5_pointer p, krb5_flags flags),
        (ctx, s, f, p, flags));

WRAP_K (krb5_db2_create_policy,
        (krb5_context context, osa_policy_ent_t entry),
        (context, entry));
WRAP_K (krb5_db2_get_policy,
        ( krb5_context kcontext,
          char *name,
          osa_policy_ent_t *policy),
        (kcontext, name, policy));
WRAP_K (krb5_db2_put_policy,
        ( krb5_context kcontext, osa_policy_ent_t policy ),
        (kcontext, policy));
WRAP_K (krb5_db2_iter_policy,
        ( krb5_context kcontext,
          char *match_entry,
          osa_adb_iter_policy_func func,
          void *data ),
        (kcontext, match_entry, func, data));
WRAP_K (krb5_db2_delete_policy,
        ( krb5_context kcontext, char *policy ),
        (kcontext, policy));

WRAP_K (krb5_db2_promote_db,
        ( krb5_context kcontext, char *conf_section, char **db_args ),
        (kcontext, conf_section, db_args));

WRAP_K (krb5_db2_check_policy_as,
        (krb5_context kcontext, krb5_kdc_req *request, krb5_db_entry *client,
         krb5_db_entry *server, krb5_timestamp kdc_time, const char **status,
         krb5_pa_data ***e_data),
        (kcontext, request, client, server, kdc_time, status, e_data));

WRAP_VOID (krb5_db2_audit_as_req,
           (krb5_context kcontext, krb5_kdc_req *request,
            krb5_db_entry *client, krb5_db_entry *server,
            krb5_timestamp authtime, krb5_error_code error_code),
           (kcontext, request, client, server, authtime, error_code));

static krb5_error_code
hack_init (void)
{
    krb5_error_code c;

    c = krb5int_mutex_alloc (&krb5_db2_mutex);
    if (c)
        return c;
    return krb5_db2_lib_init ();
}

static krb5_error_code
hack_cleanup (void)
{
    krb5int_mutex_free (krb5_db2_mutex);
    krb5_db2_mutex = NULL;
    return krb5_db2_lib_cleanup();
}


/*
 *      Exposed API
 */

kdb_vftabl PLUGIN_SYMBOL_NAME(krb5_db2, kdb_function_table) = {
    KRB5_KDB_DAL_MAJOR_VERSION,             /* major version number */
    0,                                      /* minor version number 0 */
    /* init_library */                  hack_init,
    /* fini_library */                  hack_cleanup,
    /* init_module */                   wrap_krb5_db2_open,
    /* fini_module */                   wrap_krb5_db2_fini,
    /* create */                        wrap_krb5_db2_create,
    /* destroy */                       wrap_krb5_db2_destroy,
    /* get_age */                       wrap_krb5_db2_get_age,
    /* lock */                          wrap_krb5_db2_lock,
    /* unlock */                        wrap_krb5_db2_unlock,
    /* get_principal */                 wrap_krb5_db2_get_principal,
    /* put_principal */                 wrap_krb5_db2_put_principal,
    /* delete_principal */              wrap_krb5_db2_delete_principal,
    /* rename_principal */              NULL,
    /* iterate */                       wrap_krb5_db2_iterate,
    /* create_policy */                 wrap_krb5_db2_create_policy,
    /* get_policy */                    wrap_krb5_db2_get_policy,
    /* put_policy */                    wrap_krb5_db2_put_policy,
    /* iter_policy */                   wrap_krb5_db2_iter_policy,
    /* delete_policy */                 wrap_krb5_db2_delete_policy,
    /* blah blah blah */ 0,0,0,0,0,
    /* promote_db */                    wrap_krb5_db2_promote_db,
    0, 0, 0, 0,
    /* check_policy_as */               wrap_krb5_db2_check_policy_as,
    0,
    /* audit_as_req */                  wrap_krb5_db2_audit_as_req,
    0, 0
};
