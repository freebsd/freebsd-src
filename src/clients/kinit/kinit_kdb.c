/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* clients/kinit/kinit_kdb.c */
/*
 * Copyright (C) 2010 by the Massachusetts Institute of Technology.
 * All rights reserved.
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

/**
 *    @file kinit_kdb.c
 *    Operations to open the KDB and make the KDB key table available
 *    for kinit.
 */


#include <k5-int.h>
#include <kadm5/admin.h>
#include <kdb.h>
#include "extern.h"

/** Server handle */
static void *server_handle;

/**
 * @internal  Initialize KDB for given realm
 * @param context pointer to context that will be re-initialized
 * @@param realm name of realm to initialize
 */
krb5_error_code
kinit_kdb_init(krb5_context *pcontext, char *realm)
{
    kadm5_config_params config;
    krb5_error_code retval = 0;

    if (*pcontext) {
        krb5_free_context(*pcontext);
        *pcontext = NULL;
    }
    memset(&config, 0, sizeof config);
    retval = kadm5_init_krb5_context(pcontext);
    if (retval)
        return retval;
    config.mask = KADM5_CONFIG_REALM;
    config.realm = realm;
    retval = kadm5_init(*pcontext, "kinit", NULL /*pass*/,
                        "kinit", &config,
                        KADM5_STRUCT_VERSION, KADM5_API_VERSION_4, NULL,
                        &server_handle);
    if (retval)
        return retval;
    retval = krb5_db_register_keytab(*pcontext);
    return retval;
}

void
kinit_kdb_fini()
{
    kadm5_destroy(server_handle);
}
