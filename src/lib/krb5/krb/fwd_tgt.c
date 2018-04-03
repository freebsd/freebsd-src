/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/fwd_tgt.c Definition of krb5_fwd_tgt_creds() routine */
/*
 * Copyright 1995 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include "int-proto.h"
#include "os-proto.h"

/* helper function: convert flags to necessary KDC options */
#define flags2options(flags) (flags & KDC_TKT_COMMON_MASK)

/* Get a TGT for use at the remote host */
krb5_error_code KRB5_CALLCONV
krb5_fwd_tgt_creds(krb5_context context, krb5_auth_context auth_context,
                   const char *rhost, krb5_principal client,
                   krb5_principal server, krb5_ccache cc, int forwardable,
                   krb5_data *outbuf)
/* Should forwarded TGT also be forwardable? */
{
    krb5_replay_data replaydata;
    krb5_data * scratch = 0;
    krb5_address **addrs = NULL;
    krb5_error_code retval;
    krb5_creds creds, tgt;
    krb5_creds *pcreds;
    krb5_flags kdcoptions;
    krb5_ccache defcc = NULL;
    char *def_rhost = NULL;
    krb5_enctype enctype = 0;
    krb5_keyblock *session_key;
    krb5_boolean old_use_conf_ktypes = context->use_conf_ktypes;

    memset(&creds, 0, sizeof(creds));
    memset(&tgt, 0, sizeof(creds));

    if (cc == 0) {
        if ((retval = krb5int_cc_default(context, &defcc)))
            goto errout;
        cc = defcc;
    }
    retval = krb5_auth_con_getkey (context, auth_context, &session_key);
    if (retval)
        goto errout;
    if (session_key) {
        enctype = session_key->enctype;
        krb5_free_keyblock (context, session_key);
        session_key = NULL;
    } else if (server) { /* must server be non-NULL when rhost is given? */
        /* Try getting credentials to see what the remote side supports.
           Not bulletproof, just a heuristic.  */
        krb5_creds in, *out = 0;
        memset (&in, 0, sizeof(in));

        retval = krb5_copy_principal (context, server, &in.server);
        if (retval)
            goto punt;
        retval = krb5_copy_principal (context, client, &in.client);
        if (retval)
            goto punt;
        retval = krb5_get_credentials (context, 0, cc, &in, &out);
        if (retval)
            goto punt;
        /* Got the credentials.  Okay, now record the enctype and
           throw them away.  */
        enctype = out->keyblock.enctype;
        krb5_free_creds (context, out);
    punt:
        krb5_free_cred_contents (context, &in);
    }

    if ((retval = krb5_copy_principal(context, client, &creds.client)))
        goto errout;

    retval = krb5int_tgtname(context, &client->realm, &client->realm,
                             &creds.server);
    if (retval)
        goto errout;

    /* fetch tgt directly from cache */
    context->use_conf_ktypes = 1;
    retval = krb5_cc_retrieve_cred (context, cc, KRB5_TC_SUPPORTED_KTYPES,
                                    &creds, &tgt);
    context->use_conf_ktypes = old_use_conf_ktypes;
    if (retval)
        goto errout;

    /* tgt->client must be equal to creds.client */
    if (!krb5_principal_compare(context, tgt.client, creds.client)) {
        retval = KRB5_PRINC_NOMATCH;
        goto errout;
    }

    if (!tgt.ticket.length) {
        retval = KRB5_NO_TKT_SUPPLIED;
        goto errout;
    }

    if (tgt.addresses && *tgt.addresses) {
        if (rhost == NULL) {
            if (server->type != KRB5_NT_SRV_HST) {
                retval = KRB5_FWD_BAD_PRINCIPAL;
                goto errout;
            }

            if (server->length < 2){
                retval = KRB5_CC_BADNAME;
                goto errout;
            }

            def_rhost = k5memdup0(server->data[1].data, server->data[1].length,
                                  &retval);
            if (def_rhost == NULL)
                goto errout;
            rhost = def_rhost;
        }

        retval = k5_os_hostaddr(context, rhost, &addrs);
        if (retval)
            goto errout;
    }

    creds.keyblock.enctype = enctype;
    creds.times = tgt.times;
    creds.times.starttime = 0;
    kdcoptions = flags2options(tgt.ticket_flags)|KDC_OPT_FORWARDED;

    if (!forwardable) /* Reset KDC_OPT_FORWARDABLE */
        kdcoptions &= ~(KDC_OPT_FORWARDABLE);

    if ((retval = krb5_get_cred_via_tkt(context, &tgt, kdcoptions,
                                        addrs, &creds, &pcreds))) {
        if (enctype) {
            creds.keyblock.enctype = 0;
            if ((retval = krb5_get_cred_via_tkt(context, &tgt, kdcoptions,
                                                addrs, &creds, &pcreds)))
                goto errout;
        }
        else goto errout;
    }
    retval = krb5_mk_1cred(context, auth_context, pcreds,
                           &scratch, &replaydata);
    krb5_free_creds(context, pcreds);

    if (retval) {
        if (scratch)
            krb5_free_data(context, scratch);
    } else {
        *outbuf = *scratch;
        free(scratch);
    }

errout:
    if (addrs)
        krb5_free_addresses(context, addrs);
    if (defcc)
        krb5_cc_close(context, defcc);
    free(def_rhost);
    krb5_free_cred_contents(context, &creds);
    krb5_free_cred_contents(context, &tgt);
    return retval;
}
