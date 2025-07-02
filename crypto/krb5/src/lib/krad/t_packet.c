/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krad/t_packet.c - RADIUS packet test program */
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

#define ACCEPT_PACKET 0
#define REJECT_PACKET 1

static krad_packet *packets[3];

static const krad_packet *
iterator(void *data, krb5_boolean cancel)
{
    krad_packet *tmp;
    int *i = data;

    if (cancel || packets[*i] == NULL)
        return NULL;

    tmp = packets[*i];
    *i += 1;
    return tmp;
}

static krb5_error_code
make_packet(krb5_context ctx, const krb5_data *username,
            const krb5_data *password, krad_packet **pkt)
{
    krad_attrset *set = NULL;
    krad_packet *tmp = NULL;
    krb5_error_code retval;
    const krb5_data *data;
    int i = 0;
    krb5_data nas_id;

    nas_id = string2data("12345678901234567890123456789012345678901234567890"
                         "12345678901234567890123456789012345678901234567890"
                         "12345678901234567890123456789012345678901234567890"
                         "12345678901234567890123456789012345678901234567890"
                         "12345678901234567890123456789012345678901234567890"
                         "123");

    retval = krad_attrset_new(ctx, &set);
    if (retval != 0)
        goto out;

    retval = krad_attrset_add(set, krad_attr_name2num("User-Name"), username);
    if (retval != 0)
        goto out;

    retval = krad_attrset_add(set, krad_attr_name2num("User-Password"),
                              password);
    if (retval != 0)
        goto out;

    retval = krad_attrset_add(set, krad_attr_name2num("NAS-Identifier"),
                              &nas_id);
    if (retval != 0)
        goto out;

    retval = krad_packet_new_request(ctx, "foo",
                                     krad_code_name2num("Access-Request"),
                                     set, iterator, &i, &tmp);
    if (retval != 0)
        goto out;

    data = krad_packet_get_attr(tmp, krad_attr_name2num("User-Name"), 0);
    if (data == NULL) {
        retval = ENOENT;
        goto out;
    }

    if (data->length != username->length ||
        memcmp(data->data, username->data, data->length) != 0) {
        retval = EINVAL;
        goto out;
    }

    *pkt = tmp;
    tmp = NULL;

out:
    krad_attrset_free(set);
    krad_packet_free(tmp);
    return retval;
}

static krb5_error_code
do_auth(krb5_context ctx, struct addrinfo *ai, const char *secret,
        const krad_packet *rqst, krb5_boolean *auth)
{
    const krad_packet *req = NULL;
    char tmp[KRAD_PACKET_SIZE_MAX];
    const krb5_data *request;
    krad_packet *rsp = NULL;
    krb5_error_code retval;
    krb5_data response;
    int sock = -1, i;

    response = make_data(tmp, sizeof(tmp));

    sock = socket(ai->ai_family, ai->ai_socktype, 0);
    if (sock < 0) {
        retval = errno;
        goto out;
    }

    request = krad_packet_encode(rqst);
    if (sendto(sock, request->data, request->length, 0, ai->ai_addr,
               ai->ai_addrlen) < 0) {
        retval = errno;
        goto out;
    }

    i = recv(sock, response.data, sizeof(tmp), 0);
    if (i < 0) {
        retval = errno;
        goto out;
    }
    response.length = i;

    i = 0;
    retval = krad_packet_decode_response(ctx, secret, &response, iterator, &i,
                                         &req, &rsp);
    if (retval != 0)
        goto out;

    if (req != rqst) {
        retval = EBADMSG;
        goto out;
    }

    *auth = krad_packet_get_code(rsp) == krad_code_name2num("Access-Accept");

out:
    krad_packet_free(rsp);
    if (sock >= 0)
        close(sock);
    return retval;
}

int
main(int argc, const char **argv)
{
    struct addrinfo *ai = NULL, hints;
    krb5_data username, password;
    krb5_boolean auth = FALSE;
    krb5_context ctx;

    username = string2data("testUser");

    if (!daemon_start(argc, argv)) {
        fprintf(stderr, "Unable to start pyrad daemon, skipping test...\n");
        return 0;
    }

    noerror(krb5_init_context(&ctx));

    password = string2data("accept");
    noerror(make_packet(ctx, &username, &password, &packets[ACCEPT_PACKET]));

    password = string2data("reject");
    noerror(make_packet(ctx, &username, &password, &packets[REJECT_PACKET]));

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    noerror(gai_error_code(getaddrinfo("127.0.0.1", "radius", &hints, &ai)));

    noerror(do_auth(ctx, ai, "foo", packets[ACCEPT_PACKET], &auth));
    insist(auth == TRUE);

    noerror(do_auth(ctx, ai, "foo", packets[REJECT_PACKET], &auth));
    insist(auth == FALSE);

    krad_packet_free(packets[ACCEPT_PACKET]);
    krad_packet_free(packets[REJECT_PACKET]);
    krb5_free_context(ctx);
    freeaddrinfo(ai);
    return 0;
}
