/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krad/t_remote.c - Protocol test program */
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

#define EVENT_COUNT 6

static struct
{
    int count;
    struct event events[EVENT_COUNT];
} record;

static krad_attrset *set;
static krad_remote *rr;
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

static void
remote_new(krb5_context kctx, krad_remote **remote)
{
    struct addrinfo *ai = NULL, hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    noerror(gai_error_code(getaddrinfo("127.0.0.1", "radius", &hints, &ai)));

    noerror(kr_remote_new(kctx, vctx, ai, "foo", remote));
    insist(kr_remote_equals(*remote, ai, "foo"));
    freeaddrinfo(ai);
}

static krb5_error_code
do_auth(const char *password, const krad_packet **pkt)
{
    const krad_packet *tmppkt;
    krb5_error_code retval;
    krb5_data tmp = string2data((char *)password);

    retval = krad_attrset_add(set, krad_attr_name2num("User-Password"), &tmp);
    if (retval != 0)
        return retval;

    retval = kr_remote_send(rr, krad_code_name2num("Access-Request"), set,
                            callback, NULL, 1000, 3, &tmppkt);
    krad_attrset_del(set, krad_attr_name2num("User-Password"), 0);
    if (retval != 0)
        return retval;

    if (pkt != NULL)
        *pkt = tmppkt;
    return 0;
}

static void
test_timeout(verto_ctx *ctx, verto_ev *ev)
{
    static const krad_packet *pkt;

    noerror(do_auth("accept", &pkt));
    kr_remote_cancel(rr, pkt);
}

int
main(int argc, const char **argv)
{
    krb5_context kctx = NULL;
    krb5_data tmp;

    if (!daemon_start(argc, argv)) {
        fprintf(stderr, "Unable to start pyrad daemon, skipping test...\n");
        return 0;
    }

    /* Initialize. */
    noerror(krb5_init_context(&kctx));
    vctx = verto_new(NULL, VERTO_EV_TYPE_IO | VERTO_EV_TYPE_TIMEOUT);
    insist(vctx != NULL);
    remote_new(kctx, &rr);

    /* Create attribute set. */
    noerror(krad_attrset_new(kctx, &set));
    tmp = string2data("testUser");
    noerror(krad_attrset_add(set, krad_attr_name2num("User-Name"), &tmp));

    /* Send accept packet. */
    noerror(do_auth("accept", NULL));
    verto_run(vctx);

    /* Send reject packet. */
    noerror(do_auth("reject", NULL));
    verto_run(vctx);

    /* Send canceled packet. */
    insist(verto_add_timeout(vctx, VERTO_EV_FLAG_NONE, test_timeout, 0) !=
           NULL);
    verto_run(vctx);

    /* Test timeout. */
    daemon_stop();
    noerror(do_auth("accept", NULL));
    verto_run(vctx);

    /* Test outstanding packet freeing. */
    noerror(do_auth("accept", NULL));
    kr_remote_free(rr);
    krad_attrset_free(set);

    /* Verify the results. */
    insist(record.count == EVENT_COUNT);
    insist(record.events[0].error == FALSE);
    insist(record.events[0].result.code ==
           krad_code_name2num("Access-Accept"));
    insist(record.events[1].error == FALSE);
    insist(record.events[1].result.code ==
           krad_code_name2num("Access-Reject"));
    insist(record.events[2].error == TRUE);
    insist(record.events[2].result.retval == ECANCELED);
    insist(record.events[3].error == TRUE);
    insist(record.events[3].result.retval == ETIMEDOUT);
    insist(record.events[4].error == TRUE);
    insist(record.events[4].result.retval == ECANCELED);
    insist(record.events[5].error == TRUE);
    insist(record.events[5].result.retval == ECANCELED);

    verto_free(vctx);
    krb5_free_context(kctx);
    return 0;
}
