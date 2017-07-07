/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/gssapi/t_enctypes.c - gss_krb5_set_allowable_enctypes test */
/*
 * Copyright (C) 2013 by the Massachusetts Institute of Technology.
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
#include "common.h"

/*
 * This test program establishes contexts with the krb5 mech, the default
 * initiator name, a specified target name, and the default acceptor name.
 * Before the exchange, gss_set_allowable_enctypes is called for the initiator
 * and the acceptor cred if requested.  If the exchange is successful, the
 * resulting contexts are exported with gss_krb5_export_lucid_sec_context,
 * checked for mismatches, and the GSS protocol and keys are displayed.  Exits
 * with status 0 if all operations are successful, or 1 if not.
 *
 * Usage: ./t_enctypes [-i initenctypes] [-a accenctypes] targetname
 */

static void
usage()
{
    errout("Usage: t_enctypes [-i initenctypes] [-a accenctypes] "
           "targetname");
}

/* Error out if ikey is not the same as akey. */
static void
check_key_match(gss_krb5_lucid_key_t *ikey, gss_krb5_lucid_key_t *akey)
{
    if (ikey->type != akey->type || ikey->length != akey->length ||
        memcmp(ikey->data, akey->data, ikey->length) != 0)
        errout("Initiator and acceptor keys do not match");
}

/* Display the name of enctype. */
static void
display_enctype(krb5_enctype enctype)
{
    char ename[128];

    if (krb5_enctype_to_name(enctype, FALSE, ename, sizeof(ename)) == 0)
        fputs(ename, stdout);
    else
        fputs("(unknown)", stdout);
}

int
main(int argc, char *argv[])
{
    krb5_error_code ret;
    krb5_context kctx = NULL;
    krb5_enctype *ienc = NULL, *aenc = NULL, zero = 0;
    OM_uint32 minor, major, flags;
    gss_name_t tname;
    gss_cred_id_t icred = GSS_C_NO_CREDENTIAL, acred = GSS_C_NO_CREDENTIAL;
    gss_ctx_id_t ictx, actx;
    gss_krb5_lucid_context_v1_t *ilucid, *alucid;
    gss_krb5_rfc1964_keydata_t *i1964, *a1964;
    gss_krb5_cfx_keydata_t *icfx, *acfx;
    size_t count;
    void *lptr;
    int c;

    ret = krb5_init_context(&kctx);
    check_k5err(kctx, "krb5_init_context", ret);

    /* Parse arguments. */
    while ((c = getopt(argc, argv, "i:a:")) != -1) {
        switch (c) {
        case 'i':
            ret = krb5int_parse_enctype_list(kctx, "", optarg, &zero, &ienc);
            check_k5err(kctx, "krb5_parse_enctype_list(initiator)", ret);
            break;
        case 'a':
            ret = krb5int_parse_enctype_list(kctx, "", optarg, &zero, &aenc);
            check_k5err(kctx, "krb5_parse_enctype_list(acceptor)", ret);
            break;
        default:
            usage();
        }
    }
    argc -= optind;
    argv += optind;
    if (argc != 1)
        usage();
    tname = import_name(*argv);

    if (ienc != NULL) {
        major = gss_acquire_cred(&minor, GSS_C_NO_NAME, GSS_C_INDEFINITE,
                                 &mechset_krb5, GSS_C_INITIATE, &icred, NULL,
                                 NULL);
        check_gsserr("gss_acquire_cred(initiator)", major, minor);

        for (count = 0; ienc[count]; count++);
        major = gss_krb5_set_allowable_enctypes(&minor, icred, count, ienc);
        check_gsserr("gss_krb5_set_allowable_enctypes(init)", major, minor);
    }
    if (aenc != NULL) {
        major = gss_acquire_cred(&minor, GSS_C_NO_NAME, GSS_C_INDEFINITE,
                                 &mechset_krb5, GSS_C_ACCEPT, &acred, NULL,
                                 NULL);
        check_gsserr("gss_acquire_cred(acceptor)", major, minor);

        for (count = 0; aenc[count]; count++);
        major = gss_krb5_set_allowable_enctypes(&minor, acred, count, aenc);
        check_gsserr("gss_krb5_set_allowable_enctypes(acc)", major, minor);
    }

    flags = GSS_C_REPLAY_FLAG | GSS_C_SEQUENCE_FLAG | GSS_C_MUTUAL_FLAG;
    establish_contexts(&mech_krb5, icred, acred, tname, flags, &ictx, &actx,
                       NULL, NULL, NULL);

    /* Export to lucid contexts. */
    major = gss_krb5_export_lucid_sec_context(&minor, &ictx, 1, &lptr);
    check_gsserr("gss_export_lucid_sec_context(initiator)", major, minor);
    ilucid = lptr;
    major = gss_krb5_export_lucid_sec_context(&minor, &actx, 1, &lptr);
    check_gsserr("gss_export_lucid_sec_context(acceptor)", major, minor);
    alucid = lptr;

    /* Grab the session keys and make sure they match. */
    if (ilucid->protocol != alucid->protocol)
        errout("Initiator/acceptor protocol mismatch");
    if (ilucid->protocol) {
        icfx = &ilucid->cfx_kd;
        acfx = &alucid->cfx_kd;
        if (icfx->have_acceptor_subkey != acfx->have_acceptor_subkey)
            errout("Initiator/acceptor have_acceptor_subkey mismatch");
        check_key_match(&icfx->ctx_key, &acfx->ctx_key);
        if (icfx->have_acceptor_subkey)
            check_key_match(&icfx->acceptor_subkey, &acfx->acceptor_subkey);
        fputs("cfx ", stdout);
        display_enctype(icfx->ctx_key.type);
        if (icfx->have_acceptor_subkey) {
            fputs(" ", stdout);
            display_enctype(icfx->acceptor_subkey.type);
        }
        fputs("\n", stdout);
    } else {
        i1964 = &ilucid->rfc1964_kd;
        a1964 = &alucid->rfc1964_kd;
        if (i1964->sign_alg != a1964->sign_alg ||
            i1964->seal_alg != a1964->seal_alg)
            errout("Initiator/acceptor sign or seal alg mismatch");
        check_key_match(&i1964->ctx_key, &a1964->ctx_key);
        fputs("rfc1964 ", stdout);
        display_enctype(i1964->ctx_key.type);
        fputs("\n", stdout);
    }

    krb5_free_context(kctx);
    free(ienc);
    free(aenc);
    (void)gss_release_name(&minor, &tname);
    (void)gss_release_cred(&minor, &icred);
    (void)gss_release_cred(&minor, &acred);
    (void)gss_delete_sec_context(&minor, &ictx, NULL);
    (void)gss_delete_sec_context(&minor, &actx, NULL);
    (void)gss_krb5_free_lucid_sec_context(&minor, ilucid);
    (void)gss_krb5_free_lucid_sec_context(&minor, alucid);
    return 0;
}
