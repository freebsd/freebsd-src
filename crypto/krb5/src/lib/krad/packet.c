/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krad/packet.c - Packet functions for libkrad */
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

#include "internal.h"

#include <string.h>

#include <arpa/inet.h>

typedef unsigned char uchar;

/* RFC 2865 */
#define OFFSET_CODE 0
#define OFFSET_ID 1
#define OFFSET_LENGTH 2
#define OFFSET_AUTH 4
#define OFFSET_ATTR 20
#define AUTH_FIELD_SIZE (OFFSET_ATTR - OFFSET_AUTH)

#define offset(d, o) (&(d)->data[o])
#define pkt_code_get(p) (*(krad_code *)offset(&(p)->pkt, OFFSET_CODE))
#define pkt_code_set(p, v) (*(krad_code *)offset(&(p)->pkt, OFFSET_CODE)) = v
#define pkt_id_get(p) (*(uchar *)offset(&(p)->pkt, OFFSET_ID))
#define pkt_id_set(p, v) (*(uchar *)offset(&(p)->pkt, OFFSET_ID)) = v
#define pkt_len_get(p)  load_16_be(offset(&(p)->pkt, OFFSET_LENGTH))
#define pkt_len_set(p, v)  store_16_be(v, offset(&(p)->pkt, OFFSET_LENGTH))
#define pkt_auth(p) ((uchar *)offset(&(p)->pkt, OFFSET_AUTH))
#define pkt_attr(p) ((unsigned char *)offset(&(p)->pkt, OFFSET_ATTR))

struct krad_packet_st {
    char buffer[KRAD_PACKET_SIZE_MAX];
    krad_attrset *attrset;
    krb5_data pkt;
};

typedef struct {
    uchar x[(UCHAR_MAX + 1) / 8];
} idmap;

/* Ensure the map is empty. */
static inline void
idmap_init(idmap *map)
{
    memset(map, 0, sizeof(*map));
}

/* Set an id as already allocated. */
static inline void
idmap_set(idmap *map, uchar id)
{
    map->x[id / 8] |= 1 << (id % 8);
}

/* Determine whether or not an id is used. */
static inline krb5_boolean
idmap_isset(const idmap *map, uchar id)
{
    return (map->x[id / 8] & (1 << (id % 8))) != 0;
}

/* Find an unused id starting the search at the value specified in id.
 * NOTE: For optimal security, the initial value of id should be random. */
static inline krb5_error_code
idmap_find(const idmap *map, uchar *id)
{
    krb5_int16 i;

    for (i = *id; i >= 0 && i <= UCHAR_MAX; (*id % 2 == 0) ? i++ : i--) {
        if (!idmap_isset(map, i))
            goto success;
    }

    for (i = *id; i >= 0 && i <= UCHAR_MAX; (*id % 2 == 1) ? i++ : i--) {
        if (!idmap_isset(map, i))
            goto success;
    }

    return ERANGE;

success:
    *id = i;
    return 0;
}

/* Generate size bytes of random data into the buffer. */
static inline krb5_error_code
randomize(krb5_context ctx, void *buffer, unsigned int size)
{
    krb5_data rdata = make_data(buffer, size);
    return krb5_c_random_make_octets(ctx, &rdata);
}

/* Generate a radius packet id. */
static krb5_error_code
id_generate(krb5_context ctx, krad_packet_iter_cb cb, void *data, uchar *id)
{
    krb5_error_code retval;
    const krad_packet *tmp;
    idmap used;
    uchar i;

    retval = randomize(ctx, &i, sizeof(i));
    if (retval != 0) {
        if (cb != NULL)
            (*cb)(data, TRUE);
        return retval;
    }

    if (cb != NULL) {
        idmap_init(&used);
        for (tmp = (*cb)(data, FALSE); tmp != NULL; tmp = (*cb)(data, FALSE))
            idmap_set(&used, tmp->pkt.data[1]);

        retval = idmap_find(&used, &i);
        if (retval != 0)
            return retval;
    }

    *id = i;
    return 0;
}

/* Generate a random authenticator field. */
static krb5_error_code
auth_generate_random(krb5_context ctx, uchar *rauth)
{
    krb5_ui_4 trunctime;
    time_t currtime;

    /* Get the least-significant four bytes of the current time. */
    currtime = time(NULL);
    if (currtime == (time_t)-1)
        return errno;
    trunctime = (krb5_ui_4)currtime;
    memcpy(rauth, &trunctime, sizeof(trunctime));

    /* Randomize the rest of the buffer. */
    return randomize(ctx, rauth + sizeof(trunctime),
                     AUTH_FIELD_SIZE - sizeof(trunctime));
}

/* Generate a response authenticator field. */
static krb5_error_code
auth_generate_response(krb5_context ctx, const char *secret,
                       const krad_packet *response, const uchar *auth,
                       uchar *rauth)
{
    krb5_error_code retval;
    krb5_checksum hash;
    krb5_data data;

    /* Allocate the temporary buffer. */
    retval = alloc_data(&data, response->pkt.length + strlen(secret));
    if (retval != 0)
        return retval;

    /* Encoded RADIUS packet with the request's
     * authenticator and the secret at the end. */
    memcpy(data.data, response->pkt.data, response->pkt.length);
    memcpy(data.data + OFFSET_AUTH, auth, AUTH_FIELD_SIZE);
    memcpy(data.data + response->pkt.length, secret, strlen(secret));

    /* Hash it. */
    retval = krb5_c_make_checksum(ctx, CKSUMTYPE_RSA_MD5, NULL, 0, &data,
                                  &hash);
    free(data.data);
    if (retval != 0)
        return retval;

    memcpy(rauth, hash.contents, AUTH_FIELD_SIZE);
    krb5_free_checksum_contents(ctx, &hash);
    return 0;
}

/* Create a new packet. */
static krad_packet *
packet_new()
{
    krad_packet *pkt;

    pkt = calloc(1, sizeof(krad_packet));
    if (pkt == NULL)
        return NULL;
    pkt->pkt = make_data(pkt->buffer, sizeof(pkt->buffer));

    return pkt;
}

/* Set the attrset object by decoding the packet. */
static krb5_error_code
packet_set_attrset(krb5_context ctx, const char *secret, krad_packet *pkt)
{
    krb5_data tmp;

    tmp = make_data(pkt_attr(pkt), pkt->pkt.length - OFFSET_ATTR);
    return kr_attrset_decode(ctx, &tmp, secret, pkt_auth(pkt), &pkt->attrset);
}

ssize_t
krad_packet_bytes_needed(const krb5_data *buffer)
{
    size_t len;

    if (buffer->length < OFFSET_AUTH)
        return OFFSET_AUTH - buffer->length;

    len = load_16_be(offset(buffer, OFFSET_LENGTH));
    if (len > KRAD_PACKET_SIZE_MAX)
        return -1;

    return (buffer->length > len) ? 0 : len - buffer->length;
}

void
krad_packet_free(krad_packet *pkt)
{
    if (pkt)
        krad_attrset_free(pkt->attrset);
    free(pkt);
}

/* Create a new request packet. */
krb5_error_code
krad_packet_new_request(krb5_context ctx, const char *secret, krad_code code,
                        const krad_attrset *set, krad_packet_iter_cb cb,
                        void *data, krad_packet **request)
{
    krb5_error_code retval;
    krad_packet *pkt;
    uchar id;
    size_t attrset_len;

    pkt = packet_new();
    if (pkt == NULL) {
        if (cb != NULL)
            (*cb)(data, TRUE);
        return ENOMEM;
    }

    /* Generate the ID. */
    retval = id_generate(ctx, cb, data, &id);
    if (retval != 0)
        goto error;
    pkt_id_set(pkt, id);

    /* Generate the authenticator. */
    retval = auth_generate_random(ctx, pkt_auth(pkt));
    if (retval != 0)
        goto error;

    /* Encode the attributes. */
    retval = kr_attrset_encode(set, secret, pkt_auth(pkt), pkt_attr(pkt),
                               &attrset_len);
    if (retval != 0)
        goto error;

    /* Set the code, ID and length. */
    pkt->pkt.length = attrset_len + OFFSET_ATTR;
    pkt_code_set(pkt, code);
    pkt_len_set(pkt, pkt->pkt.length);

    /* Copy the attrset for future use. */
    retval = packet_set_attrset(ctx, secret, pkt);
    if (retval != 0)
        goto error;

    *request = pkt;
    return 0;

error:
    free(pkt);
    return retval;
}

/* Create a new request packet. */
krb5_error_code
krad_packet_new_response(krb5_context ctx, const char *secret, krad_code code,
                         const krad_attrset *set, const krad_packet *request,
                         krad_packet **response)
{
    krb5_error_code retval;
    krad_packet *pkt;
    size_t attrset_len;

    pkt = packet_new();
    if (pkt == NULL)
        return ENOMEM;

    /* Encode the attributes. */
    retval = kr_attrset_encode(set, secret, pkt_auth(request), pkt_attr(pkt),
                               &attrset_len);
    if (retval != 0)
        goto error;

    /* Set the code, ID and length. */
    pkt->pkt.length = attrset_len + OFFSET_ATTR;
    pkt_code_set(pkt, code);
    pkt_id_set(pkt, pkt_id_get(request));
    pkt_len_set(pkt, pkt->pkt.length);

    /* Generate the authenticator. */
    retval = auth_generate_response(ctx, secret, pkt, pkt_auth(request),
                                    pkt_auth(pkt));
    if (retval != 0)
        goto error;

    /* Copy the attrset for future use. */
    retval = packet_set_attrset(ctx, secret, pkt);
    if (retval != 0)
        goto error;

    *response = pkt;
    return 0;

error:
    free(pkt);
    return retval;
}

/* Decode a packet. */
static krb5_error_code
decode_packet(krb5_context ctx, const char *secret, const krb5_data *buffer,
              krad_packet **pkt)
{
    krb5_error_code retval;
    krad_packet *tmp;
    krb5_ui_2 len;

    tmp = packet_new();
    if (tmp == NULL) {
        retval = ENOMEM;
        goto error;
    }

    /* Ensure a proper message length. */
    retval = (buffer->length < OFFSET_ATTR) ? EMSGSIZE : 0;
    if (retval != 0)
        goto error;
    len = load_16_be(offset(buffer, OFFSET_LENGTH));
    retval = (len < OFFSET_ATTR) ? EBADMSG : 0;
    if (retval != 0)
        goto error;
    retval = (len > buffer->length || len > tmp->pkt.length) ? EBADMSG : 0;
    if (retval != 0)
        goto error;

    /* Copy over the buffer. */
    tmp->pkt.length = len;
    memcpy(tmp->pkt.data, buffer->data, len);

    /* Parse the packet to ensure it is well-formed. */
    retval = packet_set_attrset(ctx, secret, tmp);
    if (retval != 0)
        goto error;

    *pkt = tmp;
    return 0;

error:
    krad_packet_free(tmp);
    return retval;
}

krb5_error_code
krad_packet_decode_request(krb5_context ctx, const char *secret,
                           const krb5_data *buffer, krad_packet_iter_cb cb,
                           void *data, const krad_packet **duppkt,
                           krad_packet **reqpkt)
{
    const krad_packet *tmp = NULL;
    krb5_error_code retval;

    retval = decode_packet(ctx, secret, buffer, reqpkt);
    if (cb != NULL && retval == 0) {
        for (tmp = (*cb)(data, FALSE); tmp != NULL; tmp = (*cb)(data, FALSE)) {
            if (pkt_id_get(*reqpkt) == pkt_id_get(tmp))
                break;
        }
    }

    if (cb != NULL && (retval != 0 || tmp != NULL))
        (*cb)(data, TRUE);

    *duppkt = tmp;
    return retval;
}

krb5_error_code
krad_packet_decode_response(krb5_context ctx, const char *secret,
                            const krb5_data *buffer, krad_packet_iter_cb cb,
                            void *data, const krad_packet **reqpkt,
                            krad_packet **rsppkt)
{
    uchar auth[AUTH_FIELD_SIZE];
    const krad_packet *tmp = NULL;
    krb5_error_code retval;

    retval = decode_packet(ctx, secret, buffer, rsppkt);
    if (cb != NULL && retval == 0) {
        for (tmp = (*cb)(data, FALSE); tmp != NULL; tmp = (*cb)(data, FALSE)) {
            if (pkt_id_get(*rsppkt) != pkt_id_get(tmp))
                continue;

            /* Response */
            retval = auth_generate_response(ctx, secret, *rsppkt,
                                            pkt_auth(tmp), auth);
            if (retval != 0) {
                krad_packet_free(*rsppkt);
                break;
            }

            /* If the authenticator matches, then the response is valid. */
            if (memcmp(pkt_auth(*rsppkt), auth, sizeof(auth)) == 0)
                break;
        }
    }

    if (cb != NULL && (retval != 0 || tmp != NULL))
        (*cb)(data, TRUE);

    *reqpkt = tmp;
    return retval;
}

const krb5_data *
krad_packet_encode(const krad_packet *pkt)
{
    return &pkt->pkt;
}

krad_code
krad_packet_get_code(const krad_packet *pkt)
{
    if (pkt == NULL)
        return 0;

    return pkt_code_get(pkt);
}

const krb5_data *
krad_packet_get_attr(const krad_packet *pkt, krad_attr type, size_t indx)
{
    return krad_attrset_get(pkt->attrset, type, indx);
}
