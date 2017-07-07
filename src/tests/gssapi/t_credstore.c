/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2011 Red Hat, Inc.
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

#include "common.h"

static void
usage(void)
{
    fprintf(stderr,
            "Usage: t_credstore [-sabi] principal [{key value} ...]\n");
    exit(1);
}

int
main(int argc, char *argv[])
{
    OM_uint32 minor, major;
    gss_key_value_set_desc store;
    gss_name_t name;
    gss_cred_usage_t cred_usage = GSS_C_BOTH;
    gss_OID_set mechs = GSS_C_NO_OID_SET;
    gss_cred_id_t cred = GSS_C_NO_CREDENTIAL;
    gss_ctx_id_t ictx = GSS_C_NO_CONTEXT, actx = GSS_C_NO_CONTEXT;
    gss_buffer_desc itok, atok;
    krb5_boolean store_creds = FALSE, replay = FALSE;
    char opt;

    /* Parse options. */
    for (argv++; *argv != NULL && **argv == '-'; argv++) {
        opt = (*argv)[1];
        if (opt == 's')
            store_creds = TRUE;
        else if (opt == 'r')
            replay = TRUE;
        else if (opt == 'a')
            cred_usage = GSS_C_ACCEPT;
        else if (opt == 'b')
            cred_usage = GSS_C_BOTH;
        else if (opt == 'i')
            cred_usage = GSS_C_INITIATE;
        else
            usage();
    }

    /* Get the principal name. */
    if (*argv == NULL)
        usage();
    name = import_name(*argv++);

    /* Put any remaining arguments into the store. */
    store.elements = calloc(argc, sizeof(struct gss_key_value_element_struct));
    if (!store.elements)
        errout("OOM");
    store.count = 0;
    while (*argv != NULL) {
        if (*(argv + 1) == NULL)
            usage();
        store.elements[store.count].key = *argv;
        store.elements[store.count].value = *(argv + 1);
        store.count++;
        argv += 2;
    }

    if (store_creds) {
        /* Acquire default creds and try to store them in the cred store. */
        major = gss_acquire_cred(&minor, GSS_C_NO_NAME, 0, GSS_C_NO_OID_SET,
                                 GSS_C_INITIATE, &cred, NULL, NULL);
        check_gsserr("gss_acquire_cred", major, minor);

        major = gss_store_cred_into(&minor, cred, GSS_C_INITIATE,
                                    GSS_C_NO_OID, 1, 0, &store, NULL, NULL);
        check_gsserr("gss_store_cred_into", major, minor);

        gss_release_cred(&minor, &cred);
    }

    /* Try to acquire creds from store. */
    major = gss_acquire_cred_from(&minor, name, 0, mechs, cred_usage,
                                  &store, &cred, NULL, NULL);
    check_gsserr("gss_acquire_cred_from", major, minor);

    if (replay) {
        /* Induce a replay using cred as the acceptor cred, to test the replay
         * cache indicated by the store. */
        major = gss_init_sec_context(&minor, GSS_C_NO_CREDENTIAL, &ictx, name,
                                     &mech_krb5, 0, GSS_C_INDEFINITE,
                                     GSS_C_NO_CHANNEL_BINDINGS,
                                     GSS_C_NO_BUFFER, NULL, &itok, NULL, NULL);
        check_gsserr("gss_init_sec_context", major, minor);
        (void)gss_delete_sec_context(&minor, &ictx, NULL);

        major = gss_accept_sec_context(&minor, &actx, cred, &itok,
                                       GSS_C_NO_CHANNEL_BINDINGS, NULL, NULL,
                                       &atok, NULL, NULL, NULL);
        check_gsserr("gss_accept_sec_context(1)", major, minor);
        (void)gss_release_buffer(&minor, &atok);
        (void)gss_delete_sec_context(&minor, &actx, NULL);

        major = gss_accept_sec_context(&minor, &actx, cred, &itok,
                                       GSS_C_NO_CHANNEL_BINDINGS, NULL, NULL,
                                       &atok, NULL, NULL, NULL);
        check_gsserr("gss_accept_sec_context(2)", major, minor);
        (void)gss_release_buffer(&minor, &itok);
        (void)gss_release_buffer(&minor, &atok);
        (void)gss_delete_sec_context(&minor, &actx, NULL);
    }

    gss_release_name(&minor, &name);
    gss_release_cred(&minor, &cred);
    free(store.elements);
    return 0;
}
