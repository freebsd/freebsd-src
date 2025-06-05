/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/ccache/ccselect_hostname.c - hostname ccselect module */
/*
 * Copyright (C) 2017 by Red Hat, Inc.
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

#include "k5-int.h"
#include "cc-int.h"
#include <ctype.h>
#include <krb5/ccselect_plugin.h>

/* Swap a and b, using tmp as an intermediate. */
#define SWAP(a, b, tmp)                         \
    tmp = a;                                    \
    a = b;                                      \
    b = tmp;

static krb5_error_code
hostname_init(krb5_context context, krb5_ccselect_moddata *data_out,
              int *priority_out)
{
    *data_out = NULL;
    *priority_out = KRB5_CCSELECT_PRIORITY_HEURISTIC;
    return 0;
}

static krb5_error_code
hostname_choose(krb5_context context, krb5_ccselect_moddata data,
                krb5_principal server, krb5_ccache *ccache_out,
                krb5_principal *princ_out)
{
    krb5_error_code ret;
    char *p, *host = NULL;
    size_t hostlen;
    krb5_cccol_cursor col_cursor;
    krb5_ccache ccache, tmp_ccache, best_ccache = NULL;
    krb5_principal princ, tmp_princ, best_princ = NULL;
    krb5_data domain;

    *ccache_out = NULL;
    *princ_out = NULL;

    if (server->type != KRB5_NT_SRV_HST || server->length < 2)
        return KRB5_PLUGIN_NO_HANDLE;

    /* Compute upper-case hostname. */
    hostlen = server->data[1].length;
    host = k5memdup0(server->data[1].data, hostlen, &ret);
    if (host == NULL)
        return ret;
    for (p = host; *p != '\0'; p++) {
        if (islower(*p))
            *p = toupper(*p);
    }

    /* Scan the collection for a cache with a client principal whose realm is
     * the longest tail of the server hostname. */
    ret = krb5_cccol_cursor_new(context, &col_cursor);
    if (ret)
        goto done;

    for (ret = krb5_cccol_cursor_next(context, col_cursor, &ccache);
         ret == 0 && ccache != NULL;
         ret = krb5_cccol_cursor_next(context, col_cursor, &ccache)) {
        ret = krb5_cc_get_principal(context, ccache, &princ);
        if (ret) {
            krb5_cc_close(context, ccache);
            break;
        }

        /* Check for a longer match than we have. */
        domain = make_data(host, hostlen);
        while (best_princ == NULL ||
               best_princ->realm.length < domain.length) {
            if (data_eq(princ->realm, domain)) {
                SWAP(best_ccache, ccache, tmp_ccache);
                SWAP(best_princ, princ, tmp_princ);
                break;
            }

            /* Try the next parent domain. */
            p = memchr(domain.data, '.', domain.length);
            if (p == NULL)
                break;
            domain = make_data(p + 1, hostlen - (p + 1 - host));
        }

        if (ccache != NULL)
            krb5_cc_close(context, ccache);
        krb5_free_principal(context, princ);
    }

    krb5_cccol_cursor_free(context, &col_cursor);

    if (best_ccache != NULL) {
        *ccache_out = best_ccache;
        *princ_out = best_princ;
    } else {
        ret = KRB5_PLUGIN_NO_HANDLE;
    }

done:
    free(host);
    return ret;
}

krb5_error_code
ccselect_hostname_initvt(krb5_context context, int maj_ver, int min_ver,
                         krb5_plugin_vtable vtable)
{
    krb5_ccselect_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (krb5_ccselect_vtable)vtable;
    vt->name = "hostname";
    vt->init = hostname_init;
    vt->choose = hostname_choose;
    return 0;
}
