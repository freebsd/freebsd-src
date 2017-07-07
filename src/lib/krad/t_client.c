/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krad/t_client.c - Client request test program */
/*
 * Copyright 2013 Red Hat, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "t_daemon.h"

#define EVENT_COUNT 4

static struct
{
    int count;
    struct event events[EVENT_COUNT];
} record;

static verto_ctx *vctx;

static void
callback(krb5_error_code retval, const krad_packet *request,
         const krad_packet *response, void *data)
{
    struct event *evt;

    evt = &record.events[record.count++];
    evt->error = retval != 0;
    if (evt->error)
        evt->result.retval = retval;
    else
        evt->result.code = krad_packet_get_code(response);
    verto_break(vctx);
}

int
main(int argc, const char **argv)
{
    krad_attrset *attrs;
    krad_client *rc;
    krb5_context kctx;
    krb5_data tmp;

    if (!daemon_start(argc, argv)) {
        fprintf(stderr, "Unable to start pyrad daemon, skipping test...\n");
        return 0;
    }

    noerror(krb5_init_context(&kctx));
    vctx = verto_new(NULL, VERTO_EV_TYPE_IO | VERTO_EV_TYPE_TIMEOUT);
    insist(vctx != NULL);
    noerror(krad_client_new(kctx, vctx, &rc));

    tmp = string2data("testUser");
    noerror(krad_attrset_new(kctx, &attrs));
    noerror(krad_attrset_add(attrs, krad_attr_name2num("User-Name"), &tmp));

    /* Test accept. */
    tmp = string2data("accept");
    noerror(krad_attrset_add(attrs, krad_attr_name2num("User-Password"),
                             &tmp));
    noerror(krad_client_send(rc, krad_code_name2num("Access-Request"), attrs,
                             "localhost", "foo", 1000, 3, callback, NULL));
    verto_run(vctx);

    /* Test reject. */
    tmp = string2data("reject");
    krad_attrset_del(attrs, krad_attr_name2num("User-Password"), 0);
    noerror(krad_attrset_add(attrs, krad_attr_name2num("User-Password"),
                             &tmp));
    noerror(krad_client_send(rc, krad_code_name2num("Access-Request"), attrs,
                             "localhost", "foo", 1000, 3, callback, NULL));
    verto_run(vctx);

    /* Test timeout. */
    daemon_stop();
    noerror(krad_client_send(rc, krad_code_name2num("Access-Request"), attrs,
                             "localhost", "foo", 1000, 3, callback, NULL));
    verto_run(vctx);

    /* Test outstanding packet freeing. */
    noerror(krad_client_send(rc, krad_code_name2num("Access-Request"), attrs,
                             "localhost", "foo", 1000, 3, callback, NULL));
    krad_client_free(rc);
    rc = NULL;

    /* Verify the results. */
    insist(record.count == EVENT_COUNT);
    insist(record.events[0].error == FALSE);
    insist(record.events[0].result.code ==
           krad_code_name2num("Access-Accept"));
    insist(record.events[1].error == FALSE);
    insist(record.events[1].result.code ==
           krad_code_name2num("Access-Reject"));
    insist(record.events[2].error == TRUE);
    insist(record.events[2].result.retval == ETIMEDOUT);
    insist(record.events[3].error == TRUE);
    insist(record.events[3].result.retval == ECANCELED);

    krad_attrset_free(attrs);
    krad_client_free(rc);
    verto_free(vctx);
    krb5_free_context(kctx);
    return 0;
}
