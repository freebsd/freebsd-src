/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/ser_actx.c - Serialize krb5_auth_context structure */
/*
 * Copyright 1995, 2008 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"
#include "int-proto.h"
#include "auth_con.h"

#define TOKEN_RADDR     950916
#define TOKEN_RPORT     950917
#define TOKEN_LADDR     950918
#define TOKEN_LPORT     950919
#define TOKEN_KEYBLOCK  950920
#define TOKEN_LSKBLOCK  950921
#define TOKEN_RSKBLOCK  950922

krb5_error_code
k5_size_auth_context(krb5_auth_context auth_context, size_t *sizep)
{
    krb5_error_code     kret;
    size_t              required;

    /*
     * krb5_auth_context requires at minimum:
     *  krb5_int32              for KV5M_AUTH_CONTEXT
     *  krb5_int32              for auth_context_flags
     *  krb5_int32              for remote_seq_number
     *  krb5_int32              for local_seq_number
     *  krb5_int32              for req_cksumtype
     *  krb5_int32              for safe_cksumtype
     *  krb5_int32              for size of i_vector
     *  krb5_int32              for KV5M_AUTH_CONTEXT
     */
    kret = EINVAL;
    if (auth_context != NULL) {
        kret = 0;

        required = auth_context->cstate.length;
        required += sizeof(krb5_int32)*8;

        /* Calculate size required by remote_addr, if appropriate */
        if (!kret && auth_context->remote_addr) {
            kret = k5_size_address(auth_context->remote_addr, &required);
            if (!kret)
                required += sizeof(krb5_int32);
        }

        /* Calculate size required by remote_port, if appropriate */
        if (!kret && auth_context->remote_port) {
            kret = k5_size_address(auth_context->remote_port, &required);
            if (!kret)
                required += sizeof(krb5_int32);
        }

        /* Calculate size required by local_addr, if appropriate */
        if (!kret && auth_context->local_addr) {
            kret = k5_size_address(auth_context->local_addr, &required);
            if (!kret)
                required += sizeof(krb5_int32);
        }

        /* Calculate size required by local_port, if appropriate */
        if (!kret && auth_context->local_port) {
            kret = k5_size_address(auth_context->local_port, &required);
            if (!kret)
                required += sizeof(krb5_int32);
        }

        /* Calculate size required by key, if appropriate */
        if (!kret && auth_context->key) {
            kret = k5_size_keyblock(&auth_context->key->keyblock, &required);
            if (!kret)
                required += sizeof(krb5_int32);
        }

        /* Calculate size required by send_subkey, if appropriate */
        if (!kret && auth_context->send_subkey) {
            kret = k5_size_keyblock(&auth_context->send_subkey->keyblock,
                                    &required);
            if (!kret)
                required += sizeof(krb5_int32);
        }

        /* Calculate size required by recv_subkey, if appropriate */
        if (!kret && auth_context->recv_subkey) {
            kret = k5_size_keyblock(&auth_context->recv_subkey->keyblock,
                                    &required);
            if (!kret)
                required += sizeof(krb5_int32);
        }

        /* Calculate size required by authentp, if appropriate */
        if (!kret && auth_context->authentp)
            kret = k5_size_authenticator(auth_context->authentp, &required);

    }
    if (!kret)
        *sizep += required;
    return(kret);
}

krb5_error_code
k5_externalize_auth_context(krb5_auth_context auth_context,
                            krb5_octet **buffer, size_t *lenremain)
{
    krb5_error_code     kret;
    size_t              required;
    krb5_octet          *bp;
    size_t              remain;

    required = 0;
    bp = *buffer;
    remain = *lenremain;
    kret = EINVAL;
    if (auth_context != NULL) {
        kret = ENOMEM;
        if (!k5_size_auth_context(auth_context, &required) &&
            required <= remain) {

            /* Write fixed portion */
            (void) krb5_ser_pack_int32(KV5M_AUTH_CONTEXT, &bp, &remain);
            (void) krb5_ser_pack_int32(auth_context->auth_context_flags,
                                       &bp, &remain);
            (void) krb5_ser_pack_int32(auth_context->remote_seq_number,
                                       &bp, &remain);
            (void) krb5_ser_pack_int32(auth_context->local_seq_number,
                                       &bp, &remain);
            (void) krb5_ser_pack_int32((krb5_int32) auth_context->req_cksumtype,
                                       &bp, &remain);
            (void) krb5_ser_pack_int32((krb5_int32) auth_context->safe_cksumtype,
                                       &bp, &remain);

            /* Write the cipher state */
            (void) krb5_ser_pack_int32(auth_context->cstate.length, &bp,
                                       &remain);
            (void) krb5_ser_pack_bytes((krb5_octet *)auth_context->cstate.data,
                                       auth_context->cstate.length,
                                       &bp, &remain);

            kret = 0;

            /* Now handle remote_addr, if appropriate */
            if (!kret && auth_context->remote_addr) {
                (void) krb5_ser_pack_int32(TOKEN_RADDR, &bp, &remain);
                kret = k5_externalize_address(auth_context->remote_addr,
                                              &bp, &remain);
            }

            /* Now handle remote_port, if appropriate */
            if (!kret && auth_context->remote_port) {
                (void) krb5_ser_pack_int32(TOKEN_RPORT, &bp, &remain);
                kret = k5_externalize_address(auth_context->remote_addr,
                                              &bp, &remain);
            }

            /* Now handle local_addr, if appropriate */
            if (!kret && auth_context->local_addr) {
                (void) krb5_ser_pack_int32(TOKEN_LADDR, &bp, &remain);
                kret = k5_externalize_address(auth_context->local_addr,
                                              &bp, &remain);
            }

            /* Now handle local_port, if appropriate */
            if (!kret && auth_context->local_port) {
                (void) krb5_ser_pack_int32(TOKEN_LPORT, &bp, &remain);
                kret = k5_externalize_address(auth_context->local_addr,
                                              &bp, &remain);
            }

            /* Now handle keyblock, if appropriate */
            if (!kret && auth_context->key) {
                (void) krb5_ser_pack_int32(TOKEN_KEYBLOCK, &bp, &remain);
                kret = k5_externalize_keyblock(&auth_context->key->keyblock,
                                               &bp, &remain);
            }

            /* Now handle subkey, if appropriate */
            if (!kret && auth_context->send_subkey) {
                (void) krb5_ser_pack_int32(TOKEN_LSKBLOCK, &bp, &remain);
                kret = k5_externalize_keyblock(&auth_context->
                                               send_subkey->keyblock,
                                               &bp, &remain);
            }

            /* Now handle subkey, if appropriate */
            if (!kret && auth_context->recv_subkey) {
                (void) krb5_ser_pack_int32(TOKEN_RSKBLOCK, &bp, &remain);
                kret = k5_externalize_keyblock(&auth_context->
                                               recv_subkey->keyblock,
                                               &bp, &remain);
            }

            /* Now handle authentp, if appropriate */
            if (!kret && auth_context->authentp)
                kret = k5_externalize_authenticator(auth_context->authentp,
                                                    &bp, &remain);

            /*
             * If we were successful, write trailer then update the pointer and
             * remaining length;
             */
            if (!kret) {
                /* Write our trailer */
                (void) krb5_ser_pack_int32(KV5M_AUTH_CONTEXT, &bp, &remain);
                *buffer = bp;
                *lenremain = remain;
            }
        }
    }
    return(kret);
}

/* Internalize a keyblock and convert it to a key. */
static krb5_error_code
intern_key(krb5_key *key, krb5_octet **bp, size_t *sp)
{
    krb5_keyblock *keyblock;
    krb5_error_code ret;

    ret = k5_internalize_keyblock(&keyblock, bp, sp);
    if (ret != 0)
        return ret;
    ret = krb5_k_create_key(NULL, keyblock, key);
    krb5_free_keyblock(NULL, keyblock);
    return ret;
}

krb5_error_code
k5_internalize_auth_context(krb5_auth_context *argp,
                            krb5_octet **buffer, size_t *lenremain)
{
    krb5_error_code     kret;
    krb5_auth_context   auth_context;
    krb5_int32          ibuf;
    krb5_octet          *bp;
    size_t              remain;
    krb5_int32          cstate_len;
    krb5_int32          tag;

    bp = *buffer;
    remain = *lenremain;
    kret = EINVAL;
    /* Read our magic number */
    if (krb5_ser_unpack_int32(&ibuf, &bp, &remain))
        ibuf = 0;
    if (ibuf == KV5M_AUTH_CONTEXT) {
        kret = ENOMEM;

        /* Get memory for the auth_context */
        if ((remain >= (5*sizeof(krb5_int32))) &&
            (auth_context = (krb5_auth_context)
             calloc(1, sizeof(struct _krb5_auth_context)))) {

            /* Get auth_context_flags */
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            auth_context->auth_context_flags = ibuf;

            /* Get remote_seq_number */
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            auth_context->remote_seq_number = ibuf;

            /* Get local_seq_number */
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            auth_context->local_seq_number = ibuf;

            /* Get req_cksumtype */
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            auth_context->req_cksumtype = (krb5_cksumtype) ibuf;

            /* Get safe_cksumtype */
            (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
            auth_context->safe_cksumtype = (krb5_cksumtype) ibuf;

            /* Get length of cstate */
            (void) krb5_ser_unpack_int32(&cstate_len, &bp, &remain);

            if (cstate_len) {
                kret = alloc_data(&auth_context->cstate, cstate_len);
                if (!kret) {
                    kret = krb5_ser_unpack_bytes((krb5_octet *)
                                                 auth_context->cstate.data,
                                                 cstate_len, &bp, &remain);
                }
            }
            else
                kret = 0;

            /* Peek at next token */
            tag = 0;
            if (!kret)
                kret = krb5_ser_unpack_int32(&tag, &bp, &remain);

            /* This is the remote_addr */
            if (!kret && (tag == TOKEN_RADDR)) {
                if (!(kret = k5_internalize_address(&auth_context->remote_addr,
                                                    &bp, &remain)))
                    kret = krb5_ser_unpack_int32(&tag, &bp, &remain);
            }

            /* This is the remote_port */
            if (!kret && (tag == TOKEN_RPORT)) {
                if (!(kret = k5_internalize_address(&auth_context->remote_port,
                                                    &bp, &remain)))
                    kret = krb5_ser_unpack_int32(&tag, &bp, &remain);
            }

            /* This is the local_addr */
            if (!kret && (tag == TOKEN_LADDR)) {
                if (!(kret = k5_internalize_address(&auth_context->local_addr,
                                                    &bp, &remain)))
                    kret = krb5_ser_unpack_int32(&tag, &bp, &remain);
            }

            /* This is the local_port */
            if (!kret && (tag == TOKEN_LPORT)) {
                if (!(kret = k5_internalize_address(&auth_context->local_port,
                                                    &bp, &remain)))
                    kret = krb5_ser_unpack_int32(&tag, &bp, &remain);
            }

            /* This is the keyblock */
            if (!kret && (tag == TOKEN_KEYBLOCK)) {
                if (!(kret = intern_key(&auth_context->key, &bp, &remain)))
                    kret = krb5_ser_unpack_int32(&tag, &bp, &remain);
            }

            /* This is the send_subkey */
            if (!kret && (tag == TOKEN_LSKBLOCK)) {
                if (!(kret = intern_key(&auth_context->send_subkey,
                                        &bp, &remain)))
                    kret = krb5_ser_unpack_int32(&tag, &bp, &remain);
            }

            /* This is the recv_subkey */
            if (!kret) {
                if (tag == TOKEN_RSKBLOCK) {
                    kret = intern_key(&auth_context->recv_subkey,
                                      &bp, &remain);
                }
                else {
                    /*
                     * We read the next tag, but it's not of any use here, so
                     * we effectively 'unget' it here.
                     */
                    bp -= sizeof(krb5_int32);
                    remain += sizeof(krb5_int32);
                }
            }

            /* Now find the authentp */
            if (!kret) {
                kret = k5_internalize_authenticator(&auth_context->authentp,
                                                    &bp, &remain);
                if (kret == EINVAL)
                    kret = 0;
            }

            /* Finally, find the trailer */
            if (!kret) {
                kret = krb5_ser_unpack_int32(&ibuf, &bp, &remain);
                if (!kret && (ibuf != KV5M_AUTH_CONTEXT))
                    kret = EINVAL;
            }
            if (!kret) {
                *buffer = bp;
                *lenremain = remain;
                auth_context->magic = KV5M_AUTH_CONTEXT;
                *argp = auth_context;
            }
            else
                krb5_auth_con_free(NULL, auth_context);
        }
    }
    return(kret);
}
