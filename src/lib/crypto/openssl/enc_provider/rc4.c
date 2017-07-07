/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/openssl/enc_provider/rc4.c */
/*
 * Copyright (C) 2009 by the Massachusetts Institute of Technology.
 * All rights reserved.
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

/*
 * Copyright (c) 2000 by Computer Science Laboratory,
 *                       Rensselaer Polytechnic Institute
 *
 * #include STD_DISCLAIMER
 */


#include "crypto_int.h"
#include <openssl/evp.h>

/*
 * The loopback field is a pointer to the structure.  If the application copies
 * the state (not a valid operation, but one which happens to works with some
 * other enc providers), we can detect it via the loopback field and return a
 * sane error code.
 */
struct arcfour_state {
    struct arcfour_state *loopback;
    EVP_CIPHER_CTX *ctx;
};

#define RC4_KEY_SIZE 16
#define RC4_BLOCK_SIZE 1

/* Interface layer to krb5 crypto layer */

/* The workhorse of the arcfour system,
 * this impliments the cipher
 */

/* In-place IOV crypto */
static krb5_error_code
k5_arcfour_docrypt(krb5_key key,const krb5_data *state, krb5_crypto_iov *data,
                   size_t num_data)
{
    size_t i;
    int ret = 1, tmp_len = 0;
    krb5_crypto_iov *iov     = NULL;
    EVP_CIPHER_CTX *ctx = NULL;
    struct arcfour_state *arcstate;

    arcstate = (state != NULL) ? (struct arcfour_state *) state->data : NULL;
    if (arcstate != NULL) {
        ctx = arcstate->ctx;
        if (arcstate->loopback != arcstate)
            return KRB5_CRYPTO_INTERNAL;
    }

    if (ctx == NULL) {
        ctx = EVP_CIPHER_CTX_new();
        if (ctx == NULL)
            return ENOMEM;

        ret = EVP_EncryptInit_ex(ctx, EVP_rc4(), NULL, key->keyblock.contents,
                                 NULL);
        if (!ret) {
            EVP_CIPHER_CTX_free(ctx);
            return KRB5_CRYPTO_INTERNAL;
        }

        if (arcstate != NULL)
            arcstate->ctx = ctx;
    }

    for (i = 0; i < num_data; i++) {
        iov = &data[i];
        if (ENCRYPT_IOV(iov)) {
            ret = EVP_EncryptUpdate(ctx,
                                    (unsigned char *) iov->data.data, &tmp_len,
                                    (unsigned char *) iov->data.data,
                                    iov->data.length);
            if (!ret)
                break;
        }
    }

    if (arcstate == NULL)
        EVP_CIPHER_CTX_free(ctx);

    if (!ret)
        return KRB5_CRYPTO_INTERNAL;

    return 0;
}

static void
k5_arcfour_free_state(krb5_data *state)
{
    struct arcfour_state *arcstate = (struct arcfour_state *) state->data;

    EVP_CIPHER_CTX_free(arcstate->ctx);
    free(arcstate);
}

static krb5_error_code
k5_arcfour_init_state(const krb5_keyblock *key,
                      krb5_keyusage keyusage, krb5_data *new_state)
{
    struct arcfour_state *arcstate;

    /* Create a state structure with an uninitialized context. */
    arcstate = calloc(1, sizeof(*arcstate));
    if (arcstate == NULL)
        return ENOMEM;
    arcstate->loopback = arcstate;
    arcstate->ctx = NULL;
    new_state->data = (char *) arcstate;
    new_state->length = sizeof(*arcstate);
    return 0;
}

/* Since the arcfour cipher is identical going forwards and backwards,
   we just call "docrypt" directly
*/
const struct krb5_enc_provider krb5int_enc_arcfour = {
    /* This seems to work... although I am not sure what the
       implications are in other places in the kerberos library */
    RC4_BLOCK_SIZE,
    /* Keysize is arbitrary in arcfour, but the constraints of the
       system, and to attempt to work with the MSFT system forces us
       to 16byte/128bit.  Since there is no parity in the key, the
       byte and length are the same.  */
    RC4_KEY_SIZE, RC4_KEY_SIZE,
    k5_arcfour_docrypt,
    k5_arcfour_docrypt,
    NULL,
    k5_arcfour_init_state,
    k5_arcfour_free_state
};
