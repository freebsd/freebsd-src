/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2015 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "common.h"


/*
 * Test program for inquiring about a security context, intented to be run from
 * a Python test script.  Partially establishes a context to test inquiring
 * about an incomplete context, and then establishes full contexts and inquires
 * them.  Exits with status 0 if all operations are successful, or 1 if not.
 *
 * Usage: ./t_inq_ctx target_name
 */

static void
check_inq_context(gss_ctx_id_t context, int incomplete, gss_OID expected_mech,
                  OM_uint32 expected_flags, int expected_locally_init)
{
    OM_uint32 major, minor;
    gss_name_t out_init_name, out_accept_name;
    OM_uint32 out_lifetime;
    gss_OID out_mech_type;
    OM_uint32 out_flags;
    int out_locally_init;
    int out_open;

    major = gss_inquire_context(&minor, context, &out_init_name,
                                &out_accept_name, &out_lifetime,
                                &out_mech_type, &out_flags, &out_locally_init,
                                &out_open);
    check_gsserr("gss_inquire_context", major, minor);

    assert(gss_oid_equal(out_mech_type, expected_mech));
    assert(out_flags == expected_flags);
    assert(out_locally_init == expected_locally_init);
    if (incomplete) {
        assert(!out_open);
        assert(out_lifetime == 0);
        assert(out_init_name == GSS_C_NO_NAME);
        assert(out_accept_name == GSS_C_NO_NAME);
    } else {
        assert(out_open);
        assert(out_lifetime > 0);
        assert(out_init_name != GSS_C_NO_NAME);
        assert(out_accept_name != GSS_C_NO_NAME);
    }

    (void)gss_release_name(&minor, &out_accept_name);
    (void)gss_release_name(&minor, &out_init_name);
}

/* Call gss_init_sec_context() once to create an initiator context (which will
 * be partial if flags includes GSS_C_MUTUAL_FLAG and the mech is krb5). */
static void
start_init_context(gss_OID mech, gss_cred_id_t cred, gss_name_t tname,
                   OM_uint32 flags, gss_ctx_id_t *ctx)
{
    OM_uint32 major, minor;
    gss_buffer_desc itok = GSS_C_EMPTY_BUFFER;

    *ctx = GSS_C_NO_CONTEXT;
    major = gss_init_sec_context(&minor, cred, ctx, tname, mech, flags,
                                 GSS_C_INDEFINITE, GSS_C_NO_CHANNEL_BINDINGS,
                                 NULL, NULL, &itok, NULL, NULL);
    check_gsserr("gss_init_sec_context", major, minor);
    (void)gss_release_buffer(&minor, &itok);
}

/* Call gss_init_sec_context() and gss_accept_sec_context() once to create an
 * acceptor context. */
static void
start_accept_context(gss_OID mech, gss_cred_id_t icred, gss_cred_id_t acred,
                     gss_name_t tname, OM_uint32 flags, gss_ctx_id_t *ctx)
{
    OM_uint32 major, minor;
    gss_ctx_id_t ictx = GSS_C_NO_CONTEXT;
    gss_buffer_desc itok = GSS_C_EMPTY_BUFFER, atok = GSS_C_EMPTY_BUFFER;

    major = gss_init_sec_context(&minor, icred, &ictx, tname, mech, flags,
                                 GSS_C_INDEFINITE, GSS_C_NO_CHANNEL_BINDINGS,
                                 NULL, NULL, &itok, NULL, NULL);
    check_gsserr("gss_init_sec_context", major, minor);

    *ctx = GSS_C_NO_CONTEXT;
    major = gss_accept_sec_context(&minor, ctx, acred, &itok,
                                   GSS_C_NO_CHANNEL_BINDINGS, NULL, NULL,
                                   &atok, NULL, NULL, NULL);
    check_gsserr("gss_accept_sec_context", major, minor);

    (void)gss_release_buffer(&minor, &itok);
    (void)gss_release_buffer(&minor, &atok);
    (void)gss_delete_sec_context(&minor, &ictx, NULL);
}

static void
partial_iakerb_acceptor(const char *username, const char *password,
                        gss_name_t tname, OM_uint32 flags, gss_ctx_id_t *ctx)
{
    OM_uint32 major, minor;
    gss_name_t name;
    gss_buffer_desc ubuf, pwbuf;
    gss_OID_set_desc mechlist;
    gss_cred_id_t icred, acred;

    mechlist.count = 1;
    mechlist.elements = &mech_iakerb;

    /* Import the username. */
    ubuf.value = (void *)username;
    ubuf.length = strlen(username);
    major = gss_import_name(&minor, &ubuf, GSS_C_NT_USER_NAME, &name);
    check_gsserr("gss_import_name", major, minor);

    /* Create an IAKERB initiator cred with the username and password. */
    pwbuf.value = (void *)password;
    pwbuf.length = strlen(password);
    major = gss_acquire_cred_with_password(&minor, name, &pwbuf, 0,
                                           &mechlist, GSS_C_INITIATE, &icred,
                                           NULL, NULL);
    check_gsserr("gss_acquire_cred_with_password", major, minor);

    /* Create an acceptor cred with support for IAKERB. */
    major = gss_acquire_cred(&minor, GSS_C_NO_NAME, GSS_C_INDEFINITE,
                             &mechlist, GSS_C_ACCEPT, &acred, NULL, NULL);
    check_gsserr("gss_acquire_cred", major, minor);

    /* Begin context establishment to get a partial acceptor context. */
    start_accept_context(&mech_iakerb, icred, acred, tname, flags, ctx);

    (void)gss_release_name(&minor, &name);
    (void)gss_release_cred(&minor, &icred);
    (void)gss_release_cred(&minor, &acred);
}

/* Create a partially established SPNEGO acceptor. */
static void
partial_spnego_acceptor(gss_name_t tname, gss_ctx_id_t *ctx)
{
    OM_uint32 major, minor;
    gss_buffer_desc itok = GSS_C_EMPTY_BUFFER, atok;

    /*
     * We could construct a fixed SPNEGO initiator token which forces a
     * renegotiation, but a simpler approach is to pass an empty token to
     * gss_accept_sec_context(), taking advantage of our compatibility support
     * for SPNEGO NegHints.
     */
    *ctx = GSS_C_NO_CONTEXT;
    major = gss_accept_sec_context(&minor, ctx, GSS_C_NO_CREDENTIAL, &itok,
                                   GSS_C_NO_CHANNEL_BINDINGS, NULL, NULL,
                                   &atok, NULL, NULL, NULL);
    check_gsserr("gss_accept_sec_context(neghints)", major, minor);

    (void)gss_release_buffer(&minor, &atok);
}

int
main(int argc, char *argv[])
{
    OM_uint32 minor, flags, dce_flags;
    gss_name_t tname;
    gss_ctx_id_t ictx, actx;
    const char *username, *password;

    if (argc != 4) {
        fprintf(stderr, "Usage: %s username password targetname\n", argv[0]);
        return 1;
    }
    username = argv[1];
    password = argv[2];
    tname = import_name(argv[3]);

    flags = GSS_C_SEQUENCE_FLAG | GSS_C_MUTUAL_FLAG | GSS_C_CONF_FLAG |
        GSS_C_INTEG_FLAG;
    start_init_context(&mech_krb5, GSS_C_NO_CREDENTIAL, tname, flags, &ictx);
    check_inq_context(ictx, 1, &mech_krb5, flags | GSS_C_TRANS_FLAG, 1);
    (void)gss_delete_sec_context(&minor, &ictx, NULL);

    start_init_context(&mech_iakerb, GSS_C_NO_CREDENTIAL, tname, flags, &ictx);
    check_inq_context(ictx, 1, &mech_iakerb, flags, 1);
    (void)gss_delete_sec_context(&minor, &ictx, NULL);

    start_init_context(&mech_spnego, GSS_C_NO_CREDENTIAL, tname, flags, &ictx);
    check_inq_context(ictx, 1, &mech_spnego, flags, 1);
    (void)gss_delete_sec_context(&minor, &ictx, NULL);

    dce_flags = flags | GSS_C_DCE_STYLE;
    start_accept_context(&mech_krb5, GSS_C_NO_CREDENTIAL, GSS_C_NO_CREDENTIAL,
                         tname, dce_flags, &actx);
    check_inq_context(actx, 1, &mech_krb5, dce_flags | GSS_C_TRANS_FLAG, 0);
    (void)gss_delete_sec_context(&minor, &actx, NULL);

    partial_iakerb_acceptor(username, password, tname, flags, &actx);
    check_inq_context(actx, 1, &mech_iakerb, 0, 0);
    (void)gss_delete_sec_context(&minor, &actx, NULL);

    partial_spnego_acceptor(tname, &actx);
    check_inq_context(actx, 1, &mech_spnego, 0, 0);
    (void)gss_delete_sec_context(&minor, &actx, NULL);

    establish_contexts(&mech_krb5, GSS_C_NO_CREDENTIAL, GSS_C_NO_CREDENTIAL,
                       tname, flags, &ictx, &actx, NULL, NULL, NULL);

    check_inq_context(ictx, 0, &mech_krb5, flags | GSS_C_TRANS_FLAG, 1);
    check_inq_context(actx, 0, &mech_krb5,
                      flags | GSS_C_TRANS_FLAG | GSS_C_PROT_READY_FLAG, 0);

    (void)gss_delete_sec_context(&minor, &ictx, NULL);
    (void)gss_delete_sec_context(&minor, &actx, NULL);

    (void)gss_release_name(&minor, &tname);
    return 0;
}
