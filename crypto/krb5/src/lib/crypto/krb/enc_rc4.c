/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*

  ARCFOUR cipher (based on a cipher posted on the Usenet in Spring-95).
  This cipher is widely believed and has been tested to be equivalent
  with the RC4 cipher from RSA Data Security, Inc.  (RC4 is a trademark
  of RSA Data Security)

*/
#include "crypto_int.h"

#define CONFOUNDERLENGTH 8

const char l40[] = "fortybits";

krb5_keyusage
krb5int_arcfour_translate_usage(krb5_keyusage usage)
{
    switch (usage) {
    case 1:  return 1;   /* AS-REQ PA-ENC-TIMESTAMP padata timestamp,  */
    case 2:  return 2;   /* ticket from kdc */
    case 3:  return 8;   /* as-rep encrypted part */
    case 4:  return 4;   /* tgs-req authz data */
    case 5:  return 5;   /* tgs-req authz data in subkey */
    case 6:  return 6;   /* tgs-req authenticator cksum */
    case 7:  return 7;   /* tgs-req authenticator */
    case 8:  return 8;
    case 9:  return 9;   /* tgs-rep encrypted with subkey */
    case 10: return 10;  /* ap-rep authentication cksum (never used by MS) */
    case 11: return 11;  /* app-req authenticator */
    case 12: return 12;  /* app-rep encrypted part */
    case 23: return 13;  /* sign wrap token*/
    default: return usage;
    }
}

/* Derive a usage key from a session key and krb5 usage constant. */
static krb5_error_code
usage_key(const struct krb5_enc_provider *enc,
          const struct krb5_hash_provider *hash,
          const krb5_keyblock *session_keyblock, krb5_keyusage usage,
          krb5_keyblock *out)
{
    char salt_buf[14];
    unsigned int salt_len;
    krb5_data out_data = make_data(out->contents, out->length);
    krb5_crypto_iov iov;
    krb5_keyusage ms_usage;

    /* Generate the salt. */
    ms_usage = krb5int_arcfour_translate_usage(usage);
    if (session_keyblock->enctype == ENCTYPE_ARCFOUR_HMAC_EXP) {
        memcpy(salt_buf, l40, 10);
        store_32_le(ms_usage, salt_buf + 10);
        salt_len = 14;
    } else {
        store_32_le(ms_usage, salt_buf);
        salt_len = 4;
    }

    /* Compute HMAC(key, salt) to produce the usage key. */
    iov.flags = KRB5_CRYPTO_TYPE_DATA;
    iov.data = make_data(salt_buf, salt_len);
    return krb5int_hmac_keyblock(hash, session_keyblock, &iov, 1, &out_data);
}

/* Derive an encryption key from a usage key and (typically) checksum. */
static krb5_error_code
enc_key(const struct krb5_enc_provider *enc,
        const struct krb5_hash_provider *hash,
        const krb5_keyblock *usage_keyblock, const krb5_data *checksum,
        krb5_keyblock *out)
{
    krb5_keyblock *trunc_keyblock = NULL;
    krb5_data out_data = make_data(out->contents, out->length);
    krb5_crypto_iov iov;
    krb5_error_code ret;

    /* Copy usage_keyblock to trunc_keyblock and truncate if exportable. */
    ret = krb5int_c_copy_keyblock(NULL, usage_keyblock, &trunc_keyblock);
    if (ret != 0)
        return ret;
    if (trunc_keyblock->enctype == ENCTYPE_ARCFOUR_HMAC_EXP)
        memset(trunc_keyblock->contents + 7, 0xab, 9);

    /* Compute HMAC(trunc_key, checksum) to produce the encryption key. */
    iov.flags = KRB5_CRYPTO_TYPE_DATA;
    iov.data = *checksum;
    ret = krb5int_hmac_keyblock(hash, trunc_keyblock, &iov, 1, &out_data);
    krb5int_c_free_keyblock(NULL, trunc_keyblock);
    return ret;
}

unsigned int
krb5int_arcfour_crypto_length(const struct krb5_keytypes *ktp,
                              krb5_cryptotype type)
{
    switch (type) {
    case KRB5_CRYPTO_TYPE_HEADER:
        return ktp->hash->hashsize + CONFOUNDERLENGTH;
    case KRB5_CRYPTO_TYPE_PADDING:
    case KRB5_CRYPTO_TYPE_TRAILER:
        return 0;
    case KRB5_CRYPTO_TYPE_CHECKSUM:
        return ktp->hash->hashsize;
    default:
        assert(0 &&
               "invalid cryptotype passed to krb5int_arcfour_crypto_length");
        return 0;
    }
}

/* Encrypt or decrypt using a keyblock. */
static krb5_error_code
keyblock_crypt(const struct krb5_enc_provider *enc, krb5_keyblock *keyblock,
               const krb5_data *ivec, krb5_crypto_iov *data, size_t num_data)
{
    krb5_error_code ret;
    krb5_key key;

    ret = krb5_k_create_key(NULL, keyblock, &key);
    if (ret != 0)
        return ret;
    /* Works for encryption or decryption since arcfour is a stream cipher. */
    ret = enc->encrypt(key, ivec, data, num_data);
    krb5_k_free_key(NULL, key);
    return ret;
}

krb5_error_code
krb5int_arcfour_encrypt(const struct krb5_keytypes *ktp, krb5_key key,
                        krb5_keyusage usage, const krb5_data *ivec,
                        krb5_crypto_iov *data, size_t num_data)
{
    const struct krb5_enc_provider *enc = ktp->enc;
    const struct krb5_hash_provider *hash = ktp->hash;
    krb5_error_code ret;
    krb5_crypto_iov *header, *trailer;
    krb5_keyblock *usage_keyblock = NULL, *enc_keyblock = NULL;
    krb5_data checksum, confounder, header_data;
    size_t i;

    /*
     * Caller must have provided space for the header, padding
     * and trailer; per RFC 4757 we will arrange it as:
     *
     *      Checksum | E(Confounder | Plaintext)
     */

    header = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_HEADER);
    if (header == NULL ||
        header->data.length < hash->hashsize + CONFOUNDERLENGTH)
        return KRB5_BAD_MSIZE;

    header_data = header->data;

    /* Trailer may be absent. */
    trailer = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_TRAILER);
    if (trailer != NULL)
        trailer->data.length = 0;

    /* Ensure that there is no padding. */
    for (i = 0; i < num_data; i++) {
        if (data[i].flags == KRB5_CRYPTO_TYPE_PADDING)
            data[i].data.length = 0;
    }

    ret = krb5int_c_init_keyblock(NULL, key->keyblock.enctype, enc->keybytes,
                                  &usage_keyblock);
    if (ret != 0)
        goto cleanup;
    ret = krb5int_c_init_keyblock(NULL, key->keyblock.enctype, enc->keybytes,
                                  &enc_keyblock);
    if (ret != 0)
        goto cleanup;

    /* Derive a usage key from the session key and usage. */
    ret = usage_key(enc, hash, &key->keyblock, usage, usage_keyblock);
    if (ret != 0)
        goto cleanup;

    /* Generate a confounder in the header block, after the checksum. */
    header->data.length = hash->hashsize + CONFOUNDERLENGTH;
    confounder = make_data(header->data.data + hash->hashsize,
                           CONFOUNDERLENGTH);
    ret = krb5_c_random_make_octets(0, &confounder);
    if (ret != 0)
        goto cleanup;
    checksum = make_data(header->data.data, hash->hashsize);

    /* Adjust pointers so confounder is at start of header. */
    header->data.length -= hash->hashsize;
    header->data.data += hash->hashsize;

    /* Compute the checksum using the usage key. */
    ret = krb5int_hmac_keyblock(hash, usage_keyblock, data, num_data,
                                &checksum);
    if (ret != 0)
        goto cleanup;

    /* Derive the encryption key from the usage key and checksum. */
    ret = enc_key(enc, hash, usage_keyblock, &checksum, enc_keyblock);
    if (ret)
        goto cleanup;

    ret = keyblock_crypt(enc, enc_keyblock, ivec, data, num_data);

cleanup:
    header->data = header_data; /* Restore header pointers. */
    krb5int_c_free_keyblock(NULL, usage_keyblock);
    krb5int_c_free_keyblock(NULL, enc_keyblock);
    return ret;
}

krb5_error_code
krb5int_arcfour_decrypt(const struct krb5_keytypes *ktp, krb5_key key,
                        krb5_keyusage usage, const krb5_data *ivec,
                        krb5_crypto_iov *data, size_t num_data)
{
    const struct krb5_enc_provider *enc = ktp->enc;
    const struct krb5_hash_provider *hash = ktp->hash;
    krb5_error_code ret;
    krb5_crypto_iov *header, *trailer;
    krb5_keyblock *usage_keyblock = NULL, *enc_keyblock = NULL;
    krb5_data checksum, header_data, comp_checksum = empty_data();

    header = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_HEADER);
    if (header == NULL ||
        header->data.length != hash->hashsize + CONFOUNDERLENGTH)
        return KRB5_BAD_MSIZE;

    header_data = header->data;

    trailer = krb5int_c_locate_iov(data, num_data, KRB5_CRYPTO_TYPE_TRAILER);
    if (trailer != NULL && trailer->data.length != 0)
        return KRB5_BAD_MSIZE;

    /* Allocate buffers. */
    ret = alloc_data(&comp_checksum, hash->hashsize);
    if (ret != 0)
        goto cleanup;
    ret = krb5int_c_init_keyblock(NULL, key->keyblock.enctype, enc->keybytes,
                                  &usage_keyblock);
    if (ret != 0)
        goto cleanup;
    ret = krb5int_c_init_keyblock(NULL, key->keyblock.enctype, enc->keybytes,
                                  &enc_keyblock);
    if (ret != 0)
        goto cleanup;

    checksum = make_data(header->data.data, hash->hashsize);

    /* Adjust pointers so confounder is at start of header. */
    header->data.length -= hash->hashsize;
    header->data.data += hash->hashsize;

    /* We may have to try two usage values; see below. */
    do {
        /* Derive a usage key from the session key and usage. */
        ret = usage_key(enc, hash, &key->keyblock, usage, usage_keyblock);
        if (ret != 0)
            goto cleanup;

        /* Derive the encryption key from the usage key and checksum. */
        ret = enc_key(enc, hash, usage_keyblock, &checksum, enc_keyblock);
        if (ret)
            goto cleanup;

        /* Decrypt the ciphertext. */
        ret = keyblock_crypt(enc, enc_keyblock, ivec, data, num_data);
        if (ret != 0)
            goto cleanup;

        /* Compute HMAC(usage key, plaintext) to get the checksum. */
        ret = krb5int_hmac_keyblock(hash, usage_keyblock, data, num_data,
                                    &comp_checksum);
        if (ret != 0)
            goto cleanup;

        if (k5_bcmp(checksum.data, comp_checksum.data, hash->hashsize) != 0) {
            if (usage == 9) {
                /*
                 * RFC 4757 specifies usage 8 for TGS-REP encrypted parts
                 * encrypted in a subkey, but the value used by MS is actually
                 * 9.  We now use 9 to start with, but fall back to 8 on
                 * failure in case we are communicating with a KDC using the
                 * value from the RFC.  ivec is always NULL in this case.
                 * We need to re-encrypt the data in the wrong key first.
                 */
                ret = keyblock_crypt(enc, enc_keyblock, NULL, data, num_data);
                if (ret != 0)
                    goto cleanup;
                usage = 8;
                continue;
            }
            ret = KRB5KRB_AP_ERR_BAD_INTEGRITY;
            goto cleanup;
        }

        break;
    } while (1);

cleanup:
    header->data = header_data; /* Restore header pointers. */
    krb5int_c_free_keyblock(NULL, usage_keyblock);
    krb5int_c_free_keyblock(NULL, enc_keyblock);
    zapfree(comp_checksum.data, comp_checksum.length);
    return ret;
}

krb5_error_code
krb5int_arcfour_gsscrypt(const krb5_keyblock *keyblock, krb5_keyusage usage,
                         const krb5_data *kd_data, krb5_crypto_iov *data,
                         size_t num_data)
{
    const struct krb5_enc_provider *enc = &krb5int_enc_arcfour;
    const struct krb5_hash_provider *hash = &krb5int_hash_md5;
    krb5_keyblock *usage_keyblock = NULL, *enc_keyblock = NULL;
    krb5_error_code ret;

    ret = krb5int_c_init_keyblock(NULL, keyblock->enctype, enc->keybytes,
                                  &usage_keyblock);
    if (ret != 0)
        goto cleanup;
    ret = krb5int_c_init_keyblock(NULL, keyblock->enctype, enc->keybytes,
                                  &enc_keyblock);
    if (ret != 0)
        goto cleanup;

    /* Derive a usage key from the session key and usage. */
    ret = usage_key(enc, hash, keyblock, usage, usage_keyblock);
    if (ret != 0)
        goto cleanup;

    /* Derive the encryption key from the usage key and kd_data. */
    ret = enc_key(enc, hash, usage_keyblock, kd_data, enc_keyblock);
    if (ret != 0)
        goto cleanup;

    /* Encrypt or decrypt (encrypt_iov works for both) the input. */
    ret = keyblock_crypt(enc, enc_keyblock, 0, data, num_data);

cleanup:
    krb5int_c_free_keyblock(NULL, usage_keyblock);
    krb5int_c_free_keyblock(NULL, enc_keyblock);
    return ret;
}
