/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/gssapi/t_store_cred.c - gss_store_cred() test harness */
/*
 * Copyright (C) 2021 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Usage: t_store_cred [-d] [-i] [-o] src_ccname [dest_ccname]
 *
 * Acquires creds from src_ccname using gss_acquire_cred_from() and then stores
 * them, using gss_store_cred_into() if -i is specified or gss_store_cred()
 * otherwise.  If dest_ccname is specified with -i, it is included in the cred
 * store for the store operation; if it is specified without -i, it is set with
 * gss_krb5_ccache_name() before the store operation.  If -d and/or -o are
 * specified they set the default_cred and overwrite_cred flags to true
 * respectively.
 */

#include "k5-platform.h"
#include <gssapi/gssapi_ext.h>
#include "common.h"

int
main(int argc, char *argv[])
{
    OM_uint32 major, minor;
    gss_key_value_set_desc store;
    gss_key_value_element_desc elem;
    gss_cred_id_t cred;
    krb5_boolean def = FALSE, into = FALSE, overwrite = FALSE;
    const char *src_ccname, *dest_ccname;
    int c;

    /* Parse arguments. */
    while ((c = getopt(argc, argv, "dio")) != -1) {
        switch (c) {
        case 'd':
            def = TRUE;
            break;
        case 'i':
            into = TRUE;
            break;
        case 'o':
            overwrite = TRUE;
            break;
        default:
            abort();
        }
    }
    argc -= optind;
    argv += optind;
    assert(argc == 1 || argc == 2);
    src_ccname = argv[0];
    dest_ccname = argv[1];

    elem.key = "ccache";
    elem.value = src_ccname;
    store.count = 1;
    store.elements = &elem;
    major = gss_acquire_cred_from(&minor, GSS_C_NO_NAME, GSS_C_INDEFINITE,
                                  &mechset_krb5, GSS_C_INITIATE, &store, &cred,
                                  NULL, NULL);
    check_gsserr("acquire_cred", major, minor);

    if (into) {
        if (dest_ccname != NULL) {
            elem.key = "ccache";
            elem.value = dest_ccname;
            store.count = 1;
        } else {
            store.count = 0;
        }
        major = gss_store_cred_into(&minor, cred, GSS_C_INITIATE, &mech_krb5,
                                    overwrite, def, &store, NULL, NULL);
        check_gsserr("store_cred_into", major, minor);
    } else {
        if (dest_ccname != NULL) {
            major = gss_krb5_ccache_name(&minor, dest_ccname, NULL);
            check_gsserr("ccache_name", major, minor);
        }
        major = gss_store_cred(&minor, cred, GSS_C_INITIATE, &mech_krb5,
                               overwrite, def, NULL, NULL);
        check_gsserr("store_cred", major, minor);
    }

    gss_release_cred(&minor, &cred);
    return 0;
}
