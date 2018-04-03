/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/builtin/enc_provider/rc4.c */
/*
 * Copyright (c) 2000 by Computer Science Laboratory,
 *                       Rensselaer Polytechnic Institute
 *
 * #include STD_DISCLAIMER
 */

#include "crypto_int.h"

typedef struct
{
    unsigned int x;
    unsigned int y;
    unsigned char state[256];
} ArcfourContext;

typedef struct {
    int initialized;
    ArcfourContext ctx;
} ArcFourCipherState;

/* gets the next byte from the PRNG */
#if ((__GNUC__ >= 2) )
static __inline__ unsigned int k5_arcfour_byte(ArcfourContext *);
#else
static unsigned int k5_arcfour_byte(ArcfourContext *);
#endif /* gcc inlines*/

/* Initializes the context and sets the key. */
static krb5_error_code k5_arcfour_init(ArcfourContext *ctx, const unsigned char *key,
                                       unsigned int keylen);

/* Encrypts/decrypts data. */
static void k5_arcfour_crypt(ArcfourContext *ctx, unsigned char *dest,
                             const unsigned char *src, unsigned int len);

static inline unsigned int k5_arcfour_byte(ArcfourContext * ctx)
{
    unsigned int x;
    unsigned int y;
    unsigned int sx, sy;
    unsigned char *state;

    state = ctx->state;
    x = (ctx->x + 1) & 0xff;
    sx = state[x];
    y = (sx + ctx->y) & 0xff;
    sy = state[y];
    ctx->x = x;
    ctx->y = y;
    state[y] = sx;
    state[x] = sy;
    return state[(sx + sy) & 0xff];
}

static void k5_arcfour_crypt(ArcfourContext *ctx, unsigned char *dest,
                             const unsigned char *src, unsigned int len)
{
    unsigned int i;
    for (i = 0; i < len; i++)
        dest[i] = src[i] ^ k5_arcfour_byte(ctx);
}


static krb5_error_code
k5_arcfour_init(ArcfourContext *ctx, const unsigned char *key,
                unsigned int key_len)
{
    unsigned int t, u;
    unsigned int keyindex;
    unsigned int stateindex;
    unsigned char* state;
    unsigned int counter;

    if (key_len != 16)
        return KRB5_BAD_MSIZE;     /*this is probably not the correct error code
                                     to return */
    state = &ctx->state[0];
    ctx->x = 0;
    ctx->y = 0;
    for (counter = 0; counter < 256; counter++)
        state[counter] = counter;
    keyindex = 0;
    stateindex = 0;
    for (counter = 0; counter < 256; counter++)
    {
        t = state[counter];
        stateindex = (stateindex + key[keyindex] + t) & 0xff;
        u = state[stateindex];
        state[stateindex] = t;
        state[counter] = u;
        if (++keyindex >= key_len)
            keyindex = 0;
    }
    return 0;
}


static krb5_error_code
k5_arcfour_docrypt(krb5_key key, const krb5_data *state, krb5_crypto_iov *data,
                   size_t num_data)
{
    ArcfourContext *arcfour_ctx = NULL;
    ArcFourCipherState *cipher_state = NULL;
    krb5_error_code ret;
    size_t i;

    if (key->keyblock.length != 16)
        return KRB5_BAD_KEYSIZE;
    if (state != NULL && (state->length != sizeof(ArcFourCipherState)))
        return KRB5_BAD_MSIZE;

    if (state != NULL) {
        cipher_state = (ArcFourCipherState *)(void *)state->data;
        arcfour_ctx = &cipher_state->ctx;
        if (cipher_state->initialized == 0) {
            ret = k5_arcfour_init(arcfour_ctx, key->keyblock.contents,
                                  key->keyblock.length);
            if (ret != 0)
                return ret;

            cipher_state->initialized = 1;
        }
    } else {
        arcfour_ctx = (ArcfourContext *)malloc(sizeof(ArcfourContext));
        if (arcfour_ctx == NULL)
            return ENOMEM;

        ret = k5_arcfour_init(arcfour_ctx, key->keyblock.contents,
                              key->keyblock.length);
        if (ret != 0) {
            free(arcfour_ctx);
            return ret;
        }
    }

    for (i = 0; i < num_data; i++) {
        krb5_crypto_iov *iov = &data[i];

        if (ENCRYPT_IOV(iov))
            k5_arcfour_crypt(arcfour_ctx, (unsigned char *)iov->data.data,
                             (const unsigned char *)iov->data.data, iov->data.length);
    }

    if (state == NULL)
        zapfree(arcfour_ctx, sizeof(ArcfourContext));

    return 0;
}

static krb5_error_code
k5_arcfour_init_state (const krb5_keyblock *key,
                       krb5_keyusage keyusage, krb5_data *new_state)
{
    /* Note that we can't actually set up the state here  because the key
     * will change  between now and when encrypt is called
     * because  it is data dependent.  Yeah, this has strange
     * properties. --SDH
     */
    new_state->length = sizeof (ArcFourCipherState);
    new_state->data = malloc (new_state->length);
    if (new_state->data) {
        memset (new_state->data, 0 , new_state->length);
        /* That will set initialized to zero*/
    }else {
        return (ENOMEM);
    }
    return 0;
}

/* Since the arcfour cipher is identical going forwards and backwards,
   we just call "docrypt" directly
*/
const struct krb5_enc_provider krb5int_enc_arcfour = {
    /* This seems to work... although I am not sure what the
       implications are in other places in the kerberos library */
    1,
    /* Keysize is arbitrary in arcfour, but the constraints of the
       system, and to attempt to work with the MSFT system forces us
       to 16byte/128bit.  Since there is no parity in the key, the
       byte and length are the same.  */
    16, 16,
    k5_arcfour_docrypt,
    k5_arcfour_docrypt,
    NULL,
    k5_arcfour_init_state, /*xxx not implemented yet*/
    krb5int_default_free_state
};
