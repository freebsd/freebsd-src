/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/krb/aead.c */
/*
 * Copyright 2008 by the Massachusetts Institute of Technology.
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

#include "crypto_int.h"

krb5_crypto_iov *
krb5int_c_locate_iov(krb5_crypto_iov *data, size_t num_data,
                     krb5_cryptotype type)
{
    size_t i;
    krb5_crypto_iov *iov = NULL;

    if (data == NULL)
        return NULL;

    for (i = 0; i < num_data; i++) {
        if (data[i].flags == type) {
            if (iov == NULL)
                iov = &data[i];
            else
                return NULL; /* can't appear twice */
        }
    }

    return iov;
}

krb5_error_code
krb5int_c_iov_decrypt_stream(const struct krb5_keytypes *ktp, krb5_key key,
                             krb5_keyusage keyusage, const krb5_data *ivec,
                             krb5_crypto_iov *data, size_t num_data)
{
    krb5_error_code ret;
    unsigned int header_len, trailer_len;
    krb5_crypto_iov *iov;
    krb5_crypto_iov *stream;
    size_t i, j;
    int got_data = 0;

    stream = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_STREAM);
    assert(stream != NULL);

    header_len = ktp->crypto_length(ktp, KRB5_CRYPTO_TYPE_HEADER);
    trailer_len = ktp->crypto_length(ktp, KRB5_CRYPTO_TYPE_TRAILER);

    if (stream->data.length < header_len + trailer_len)
        return KRB5_BAD_MSIZE;

    iov = calloc(num_data + 2, sizeof(krb5_crypto_iov));
    if (iov == NULL)
        return ENOMEM;

    i = 0;

    iov[i].flags = KRB5_CRYPTO_TYPE_HEADER; /* takes place of STREAM */
    iov[i].data = make_data(stream->data.data, header_len);
    i++;

    for (j = 0; j < num_data; j++) {
        if (data[j].flags == KRB5_CRYPTO_TYPE_DATA) {
            if (got_data) {
                free(iov);
                return KRB5_BAD_MSIZE;
            }

            got_data++;

            data[j].data.data = stream->data.data + header_len;
            data[j].data.length = stream->data.length - header_len
                - trailer_len;
        }
        if (data[j].flags == KRB5_CRYPTO_TYPE_SIGN_ONLY ||
            data[j].flags == KRB5_CRYPTO_TYPE_DATA)
            iov[i++] = data[j];
    }

    /* Use empty padding since tokens don't indicate the padding length. */
    iov[i].flags = KRB5_CRYPTO_TYPE_PADDING;
    iov[i].data = empty_data();
    i++;

    iov[i].flags = KRB5_CRYPTO_TYPE_TRAILER;
    iov[i].data = make_data(stream->data.data + stream->data.length -
                            trailer_len, trailer_len);
    i++;

    assert(i <= num_data + 2);

    ret = ktp->decrypt(ktp, key, keyusage, ivec, iov, i);
    free(iov);
    return ret;
}

unsigned int
krb5int_c_padding_length(const struct krb5_keytypes *ktp, size_t data_length)
{
    unsigned int header, padding;

    /*
     * Add in the header length since the header is encrypted along with the
     * data.  (arcfour violates this assumption since not all of the header is
     * encrypted, but that's okay since it has no padding.  If there is ever an
     * enctype using a similar token format and a block cipher, we will have to
     * move this logic into an enctype-dependent function.)
     */
    header = ktp->crypto_length(ktp, KRB5_CRYPTO_TYPE_HEADER);
    data_length += header;

    padding = ktp->crypto_length(ktp, KRB5_CRYPTO_TYPE_PADDING);
    if (padding == 0 || (data_length % padding) == 0)
        return 0;
    else
        return padding - (data_length % padding);
}

/* Return the next iov (starting from ind) which cursor should process, or
 * cursor->iov_count if there are none remaining. */
static size_t
next_iov_to_process(struct iov_cursor *cursor, size_t ind)
{
    const krb5_crypto_iov *iov;

    for (; ind < cursor->iov_count; ind++) {
        iov = &cursor->iov[ind];
        if (cursor->signing ? SIGN_IOV(iov) : ENCRYPT_IOV(iov))
            break;
    }
    return ind;
}

void
k5_iov_cursor_init(struct iov_cursor *cursor, const krb5_crypto_iov *iov,
                   size_t count, size_t block_size, krb5_boolean signing)
{
    cursor->iov = iov;
    cursor->iov_count = count;
    cursor->block_size = block_size;
    cursor->signing = signing;
    cursor->in_iov = next_iov_to_process(cursor, 0);
    cursor->out_iov = cursor->in_iov;
    cursor->in_pos = cursor->out_pos = 0;
}

/* Fetch one block from cursor's input position. */
krb5_boolean
k5_iov_cursor_get(struct iov_cursor *cursor, unsigned char *block)
{
    size_t nbytes, bsz = cursor->block_size, remain = cursor->block_size;
    const krb5_crypto_iov *iov;

    remain = cursor->block_size;
    while (remain > 0 && cursor->in_iov < cursor->iov_count) {
        iov = &cursor->iov[cursor->in_iov];

        nbytes = iov->data.length - cursor->in_pos;
        if (nbytes > remain)
            nbytes = remain;

        memcpy(block + bsz - remain, iov->data.data + cursor->in_pos, nbytes);
        cursor->in_pos += nbytes;
        remain -= nbytes;

        if (cursor->in_pos == iov->data.length) {
            cursor->in_iov = next_iov_to_process(cursor, cursor->in_iov + 1);
            cursor->in_pos = 0;
        }
    }

    if (remain == bsz)
        return FALSE;
    if (remain > 0)
        memset(block + bsz - remain, 0, remain);
    return TRUE;
}

/* Write a block to a cursor's output position. */
void
k5_iov_cursor_put(struct iov_cursor *cursor, unsigned char *block)
{
    size_t nbytes, bsz = cursor->block_size, remain = cursor->block_size;
    const krb5_crypto_iov *iov;

    remain = cursor->block_size;
    while (remain > 0 && cursor->out_iov < cursor->iov_count) {
        iov = &cursor->iov[cursor->out_iov];

        nbytes = iov->data.length - cursor->out_pos;
        if (nbytes > remain)
            nbytes = remain;

        memcpy(iov->data.data + cursor->out_pos, block + bsz - remain, nbytes);
        cursor->out_pos += nbytes;
        remain -= nbytes;

        if (cursor->out_pos == iov->data.length) {
            cursor->out_iov = next_iov_to_process(cursor, cursor->out_iov + 1);
            cursor->out_pos = 0;
        }
    }
}
