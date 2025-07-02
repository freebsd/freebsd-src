/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef _KRB5_KDB5_H_
#define _KRB5_KDB5_H_

#include <k5-int.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <errno.h>
#include <utime.h>
#include "kdb.h"

#define KRB5_DB_GET_DB_CONTEXT(kcontext) (((kdb5_dal_handle*) (kcontext)->dal_handle)->db_context)
#define KRB5_DB_GET_PROFILE(kcontext)  ((kcontext)->profile)
#define KRB5_DB_GET_REALM(kcontext)    ((kcontext)->default_realm)

typedef struct _db_library {
    char name[KDB_MAX_DB_NAME];
    int reference_cnt;
    struct plugin_dir_handle dl_dir_handle;
    kdb_vftabl vftabl;
    struct _db_library *next, *prev;
} *db_library;

struct _kdb5_dal_handle
{
    /* Helps us to change db_library without affecting modules to some
       extent.  */
    void *db_context;
    db_library lib_handle;
    krb5_keylist_node *master_keylist;
    krb5_principal master_princ;
};
/* typedef kdb5_dal_handle is in k5-int.h now */

#endif  /* end of _KRB5_KDB5_H_ */
