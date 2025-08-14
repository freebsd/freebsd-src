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
#define MSGAUTH_SIZE (2 + MD5_DIGEST_SIZE)
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
packet_new(void)
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

/* Determine if a packet requires a Message-Authenticator attribute. */
static inline krb5_boolean
requires_msgauth(const char *secret, krad_code code)
{
    /* If no secret is provided, assume that the transport is a UNIX socket.
     * Message-Authenticator is required only on UDP and TCP connections. */
    if (*secret == '\0')
        return FALSE;

    /*
     * Per draft-ietf-radext-deprecating-radius-03 sections 5.2.1 and 5.2.4,
     * Message-Authenticator is required in Access-Request packets and all
     * potential responses when UDP or TCP transport is used.
     */
    return code == KRAD_CODE_ACCESS_REQUEST ||
        code == KRAD_CODE_ACCESS_ACCEPT || code == KRAD_CODE_ACCESS_REJECT ||
        code == KRAD_CODE_ACCESS_CHALLENGE;
}

/* Check if the packet has a Message-Authenticator attribute. */
static inline krb5_boolean
has_pkt_msgauth(const krad_packet *pkt)
{
    return krad_attrset_get(pkt->attrset, KRAD_ATTR_MESSAGE_AUTHENTICATOR,
                            0) != NULL;
}

/* Return the beginning of the Message-Authenticator attribute in pkt, or NULL
 * if no such attribute is present. */
static const uint8_t *
lookup_msgauth_addr(const krad_packet *pkt)
{
    size_t i;
    uint8_t *p;

    i = OFFSET_ATTR;
    while (i + 2 < pkt->pkt.length) {
        p = (uint8_t *)offset(&pkt->pkt, i);
        if (*p == KRAD_ATTR_MESSAGE_AUTHENTICATOR)
            return p;
        i += p[1];
    }

    return NULL;
}

/*
 * Calculate the message authenticator MAC for pkt as specified in RFC 2869
 * section 5.14, placing the result in mac_out.  Use the provided authenticator
 * auth, which may be from pkt or from a corresponding request.
 */
static krb5_error_code
calculate_mac(const char *secret, const krad_packet *pkt,
              const uint8_t auth[AUTH_FIELD_SIZE],
              uint8_t mac_out[MD5_DIGEST_SIZE])
{
    const uint8_t *msgauth_attr, *msgauth_end, *pkt_end;
    krb5_crypto_iov input[5];
    krb5_data ksecr, mac;
    static const uint8_t zeroed_msgauth[MSGAUTH_SIZE] = {
        KRAD_ATTR_MESSAGE_AUTHENTICATOR, MSGAUTH_SIZE
    };

    msgauth_attr = lookup_msgauth_addr(pkt);
    if (msgauth_attr == NULL)
        return EINVAL;
    msgauth_end = msgauth_attr + MSGAUTH_SIZE;
    pkt_end = (const uint8_t *)pkt->pkt.data + pkt->pkt.length;

    /* Read code, id, and length from the packet. */
    input[0].flags = KRB5_CRYPTO_TYPE_DATA;
    input[0].data = make_data(pkt->pkt.data, OFFSET_AUTH);

    /* Read the provided authenticator. */
    input[1].flags = KRB5_CRYPTO_TYPE_DATA;
    input[1].data = make_data((uint8_t *)auth, AUTH_FIELD_SIZE);

    /* Read any attributes before Message-Authenticator. */
    input[2].flags = KRB5_CRYPTO_TYPE_DATA;
    input[2].data = make_data(pkt_attr(pkt), msgauth_attr - pkt_attr(pkt));

    /* Read Message-Authenticator with the data bytes all set to zero, per RFC
     * 2869 section 5.14. */
    input[3].flags = KRB5_CRYPTO_TYPE_DATA;
    input[3].data = make_data((uint8_t *)zeroed_msgauth, MSGAUTH_SIZE);

    /* Read any attributes after Message-Authenticator. */
    input[4].flags = KRB5_CRYPTO_TYPE_DATA;
    input[4].data = make_data((uint8_t *)msgauth_end, pkt_end - msgauth_end);

    mac = make_data(mac_out, MD5_DIGEST_SIZE);
    ksecr = string2data((char *)secret);
    return k5_hmac_md5(&ksecr, input, 5, &mac);
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
    krb5_boolean msgauth_required;

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

    /* Determine if Message-Authenticator is required. */
    msgauth_required = (*secret != '\0' && code == KRAD_CODE_ACCESS_REQUEST);

    /* Encode the attributes. */
    retval = kr_attrset_encode(set, secret, pkt_auth(pkt), msgauth_required,
                               pkt_attr(pkt), &attrset_len);
    if (retval != 0)
        goto error;

    /* Set the code, ID and length. */
    pkt->pkt.length = attrset_len + OFFSET_ATTR;
    pkt_code_set(pkt, code);
    pkt_len_set(pkt, pkt->pkt.length);

    if (msgauth_required) {
        /* Calculate and set the Message-Authenticator MAC. */
        retval = calculate_mac(secret, pkt, pkt_auth(pkt), pkt_attr(pkt) + 2);
        if (retval != 0)
            goto error;
    }

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
    krb5_boolean msgauth_required;

    pkt = packet_new();
    if (pkt == NULL)
        return ENOMEM;

    /* Determine if Message-Authenticator is required. */
    msgauth_required = requires_msgauth(secret, code);

    /* Encode the attributes. */
    retval = kr_attrset_encode(set, secret, pkt_auth(request),
                               msgauth_required, pkt_attr(pkt), &attrset_len);
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

    if (msgauth_required) {
        /*
         * Calculate and replace the Message-Authenticator MAC.  Per RFC 2869
         * section 5.14, use the authenticator from the request, not from the
         * response.
         */
        retval = calculate_mac(secret, pkt, pkt_auth(request),
                               pkt_attr(pkt) + 2);
        if (retval != 0)
            goto error;
    }

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

/* Verify the Message-Authenticator value in pkt, using the provided
 * authenticator (which may be from pkt or from a corresponding request). */
static krb5_error_code
verify_msgauth(const char *secret, const krad_packet *pkt,
               const uint8_t auth[AUTH_FIELD_SIZE])
{
    uint8_t mac[MD5_DIGEST_SIZE];
    const krb5_data *msgauth;
    krb5_error_code retval;

    msgauth = krad_packet_get_attr(pkt, KRAD_ATTR_MESSAGE_AUTHENTICATOR, 0);
/* XXX ENODATA does not exist in FreeBSD. The closest thing we have to */
/* XXX ENODATA is ENOATTR. We use that instead. */
#define ENODATA ENOATTR
    if (msgauth == NULL)
        return ENODATA;

    retval = calculate_mac(secret, pkt, auth, mac);
    if (retval)
        return retval;

    if (msgauth->length != MD5_DIGEST_SIZE)
        return EMSGSIZE;

    if (k5_bcmp(mac, msgauth->data, MD5_DIGEST_SIZE) != 0)
        return EBADMSG;

    return 0;
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
    krad_packet *req;
    krb5_error_code retval;

    retval = decode_packet(ctx, secret, buffer, &req);
    if (retval)
        return retval;

    /* Verify Message-Authenticator if present. */
    if (has_pkt_msgauth(req)) {
        retval = verify_msgauth(secret, req, pkt_auth(req));
        if (retval) {
            krad_packet_free(req);
            return retval;
        }
    }

    if (cb != NULL) {
        for (tmp = (*cb)(data, FALSE); tmp != NULL; tmp = (*cb)(data, FALSE)) {
            if (pkt_id_get(*reqpkt) == pkt_id_get(tmp))
                break;
        }

        if (tmp != NULL)
            (*cb)(data, TRUE);
    }

    *reqpkt = req;
    *duppkt = tmp;
    return 0;
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

            /* Verify the response authenticator. */
            if (k5_bcmp(pkt_auth(*rsppkt), auth, sizeof(auth)) != 0)
                continue;

            /* Verify Message-Authenticator if present. */
            if (has_pkt_msgauth(*rsppkt)) {
                if (verify_msgauth(secret, *rsppkt, pkt_auth(tmp)) != 0)
                    continue;
            }

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
