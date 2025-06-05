/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/preauth_sam2.c - SAM-2 clpreauth module */
/*
 * Copyright 1995, 2003, 2008, 2012 by the Massachusetts Institute of Technology.  All
 * Rights Reserved.
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
 *
 */

#include <k5-int.h>
#include <krb5/clpreauth_plugin.h>
#include "int-proto.h"
#include "os-proto.h"
#include "init_creds_ctx.h"

/* this macro expands to the int,ptr necessary for "%.*s" in an sprintf */

#define SAMDATA(kdata, str, maxsize)                                    \
    (int)((kdata.length)?                                               \
          ((((kdata.length)<=(maxsize))?(kdata.length):strlen(str))):   \
          strlen(str)),                                                 \
        (kdata.length)?                                                 \
        ((((kdata.length)<=(maxsize))?(kdata.data):(str))):(str)
static char *
sam_challenge_banner(krb5_int32 sam_type)
{
    char *label;

    switch (sam_type) {
    case PA_SAM_TYPE_ENIGMA:    /* Enigma Logic */
        label = _("Challenge for Enigma Logic mechanism");
        break;
    case PA_SAM_TYPE_DIGI_PATH: /*  Digital Pathways */
    case PA_SAM_TYPE_DIGI_PATH_HEX: /*  Digital Pathways */
        label = _("Challenge for Digital Pathways mechanism");
        break;
    case PA_SAM_TYPE_ACTIVCARD_DEC: /*  Digital Pathways */
    case PA_SAM_TYPE_ACTIVCARD_HEX: /*  Digital Pathways */
        label = _("Challenge for Activcard mechanism");
        break;
    case PA_SAM_TYPE_SKEY_K0:   /*  S/key where  KDC has key 0 */
        label = _("Challenge for Enhanced S/Key mechanism");
        break;
    case PA_SAM_TYPE_SKEY:      /*  Traditional S/Key */
        label = _("Challenge for Traditional S/Key mechanism");
        break;
    case PA_SAM_TYPE_SECURID:   /*  Security Dynamics */
        label = _("Challenge for Security Dynamics mechanism");
        break;
    case PA_SAM_TYPE_SECURID_PREDICT:   /* predictive Security Dynamics */
        label = _("Challenge for Security Dynamics mechanism");
        break;
    default:
        label = _("Challenge from authentication server");
        break;
    }

    return(label);
}

static krb5_error_code
sam2_process(krb5_context context, krb5_clpreauth_moddata moddata,
             krb5_clpreauth_modreq modreq, krb5_get_init_creds_opt *opt,
             krb5_clpreauth_callbacks cb, krb5_clpreauth_rock rock,
             krb5_kdc_req *request, krb5_data *encoded_request_body,
             krb5_data *encoded_previous_request, krb5_pa_data *padata,
             krb5_prompter_fct prompter, void *prompter_data,
             krb5_pa_data ***out_padata)
{
    krb5_init_creds_context ctx = (krb5_init_creds_context)rock;
    krb5_error_code retval;
    krb5_sam_challenge_2 *sc2 = NULL;
    krb5_sam_challenge_2_body *sc2b = NULL;
    krb5_data tmp_data;
    krb5_data response_data;
    char name[100], banner[100], prompt[100], response[100];
    krb5_prompt kprompt;
    krb5_prompt_type prompt_type;
    krb5_data defsalt, *salt;
    krb5_checksum **cksum;
    krb5_data *scratch = NULL;
    krb5_boolean valid_cksum = 0;
    krb5_enc_sam_response_enc_2 enc_sam_response_enc_2;
    krb5_sam_response_2 sr2;
    size_t ciph_len;
    krb5_pa_data **sam_padata;

    if (prompter == NULL)
        return KRB5_LIBOS_CANTREADPWD;

    tmp_data.length = padata->length;
    tmp_data.data = (char *)padata->contents;

    if ((retval = decode_krb5_sam_challenge_2(&tmp_data, &sc2)))
        return(retval);

    retval = decode_krb5_sam_challenge_2_body(&sc2->sam_challenge_2_body, &sc2b);

    if (retval) {
        krb5_free_sam_challenge_2(context, sc2);
        return(retval);
    }

    if (!sc2->sam_cksum || ! *sc2->sam_cksum) {
        krb5_free_sam_challenge_2(context, sc2);
        krb5_free_sam_challenge_2_body(context, sc2b);
        return(KRB5_SAM_NO_CHECKSUM);
    }

    if (sc2b->sam_flags & KRB5_SAM_MUST_PK_ENCRYPT_SAD) {
        krb5_free_sam_challenge_2(context, sc2);
        krb5_free_sam_challenge_2_body(context, sc2b);
        return(KRB5_SAM_UNSUPPORTED);
    }

    if (!krb5_c_valid_enctype(sc2b->sam_etype)) {
        krb5_free_sam_challenge_2(context, sc2);
        krb5_free_sam_challenge_2_body(context, sc2b);
        return(KRB5_SAM_INVALID_ETYPE);
    }

    /* All of the above error checks are KDC-specific, that is, they     */
    /* assume a failure in the KDC reply.  By returning anything other   */
    /* than KRB5_KDC_UNREACH, KRB5_PREAUTH_FAILED,               */
    /* KRB5_LIBOS_PWDINTR, or KRB5_REALM_CANT_RESOLVE, the client will   */
    /* most likely go on to try the AS_REQ against master KDC            */

    if (!(sc2b->sam_flags & KRB5_SAM_USE_SAD_AS_KEY)) {
        /* We will need the password to obtain the key used for */
        /* the checksum, and encryption of the sam_response.    */
        /* Go ahead and get it now, preserving the ordering of  */
        /* prompts for the user.                                */

        salt = ctx->default_salt ? NULL : &ctx->salt;
        retval = ctx->gak_fct(context, request->client, sc2b->sam_etype,
                              prompter, prompter_data, salt, &ctx->s2kparams,
                              &ctx->as_key, ctx->gak_data, ctx->rctx.items);
        if (retval) {
            krb5_free_sam_challenge_2(context, sc2);
            krb5_free_sam_challenge_2_body(context, sc2b);
            return(retval);
        }
    }

    snprintf(name, sizeof(name), "%.*s",
             SAMDATA(sc2b->sam_type_name, _("SAM Authentication"),
                     sizeof(name) - 1));

    snprintf(banner, sizeof(banner), "%.*s",
             SAMDATA(sc2b->sam_challenge_label,
                     sam_challenge_banner(sc2b->sam_type),
                     sizeof(banner)-1));

    snprintf(prompt, sizeof(prompt), "%s%.*s%s%.*s",
             sc2b->sam_challenge.length?"Challenge is [":"",
             SAMDATA(sc2b->sam_challenge, "", 20),
             sc2b->sam_challenge.length?"], ":"",
             SAMDATA(sc2b->sam_response_prompt, "passcode", 55));

    response_data.data = response;
    response_data.length = sizeof(response);
    kprompt.prompt = prompt;
    kprompt.hidden = 1;
    kprompt.reply = &response_data;

    prompt_type = KRB5_PROMPT_TYPE_PREAUTH;
    k5_set_prompt_types(context, &prompt_type);

    if ((retval = ((*prompter)(context, prompter_data, name,
                               banner, 1, &kprompt)))) {
        krb5_free_sam_challenge_2(context, sc2);
        krb5_free_sam_challenge_2_body(context, sc2b);
        k5_set_prompt_types(context, NULL);
        return(retval);
    }

    k5_set_prompt_types(context, NULL);

    /* Generate salt used by string_to_key() */
    if (ctx->default_salt) {
        if ((retval =
             krb5_principal2salt(context, request->client, &defsalt))) {
            krb5_free_sam_challenge_2(context, sc2);
            krb5_free_sam_challenge_2_body(context, sc2b);
            return(retval);
        }
        salt = &defsalt;
    } else {
        salt = &ctx->salt;
        defsalt.length = 0;
    }

    /* Get encryption key to be used for checksum and sam_response */
    if (!(sc2b->sam_flags & KRB5_SAM_USE_SAD_AS_KEY)) {
        /* Retain as_key from above gak_fct call. */
        if (defsalt.length)
            free(defsalt.data);

        if (!(sc2b->sam_flags & KRB5_SAM_SEND_ENCRYPTED_SAD)) {
            /*
             * If no flags are set, the protocol calls for us to combine the
             * initial reply key with the SAD, using a method which is only
             * specified for DES and 3DES enctypes.  We no longer support this
             * case.
             */
            krb5_free_sam_challenge_2(context, sc2);
            krb5_free_sam_challenge_2_body(context, sc2b);
            return(KRB5_SAM_UNSUPPORTED);
        }
    } else {
        /* as_key = string_to_key(SAD) */

        if (ctx->as_key.length) {
            krb5_free_keyblock_contents(context, &ctx->as_key);
            ctx->as_key.length = 0;
        }

        /* generate a key using the supplied password */
        retval = krb5_c_string_to_key(context, sc2b->sam_etype,
                                      &response_data, salt, &ctx->as_key);

        if (defsalt.length)
            free(defsalt.data);

        if (retval) {
            krb5_free_sam_challenge_2(context, sc2);
            krb5_free_sam_challenge_2_body(context, sc2b);
            return(retval);
        }
    }

    /* Now we have a key, verify the checksum on the sam_challenge */

    cksum = sc2->sam_cksum;

    for (; *cksum; cksum++) {
        if (!krb5_c_is_keyed_cksum((*cksum)->checksum_type))
            continue;
        /* Check this cksum */
        retval = krb5_c_verify_checksum(context, &ctx->as_key,
                                        KRB5_KEYUSAGE_PA_SAM_CHALLENGE_CKSUM,
                                        &sc2->sam_challenge_2_body,
                                        *cksum, &valid_cksum);
        if (retval) {
            krb5_free_data(context, scratch);
            krb5_free_sam_challenge_2(context, sc2);
            krb5_free_sam_challenge_2_body(context, sc2b);
            return(retval);
        }
        if (valid_cksum)
            break;
    }

    if (!valid_cksum) {
        krb5_free_sam_challenge_2(context, sc2);
        krb5_free_sam_challenge_2_body(context, sc2b);
        /*
         * Note: We return AP_ERR_BAD_INTEGRITY so upper-level applications
         * can interpret that as "password incorrect", which is probably
         * the best error we can return in this situation.
         */
        return(KRB5KRB_AP_ERR_BAD_INTEGRITY);
    }

    /* fill in enc_sam_response_enc_2 */
    enc_sam_response_enc_2.magic = KV5M_ENC_SAM_RESPONSE_ENC_2;
    enc_sam_response_enc_2.sam_nonce = sc2b->sam_nonce;
    if (sc2b->sam_flags & KRB5_SAM_SEND_ENCRYPTED_SAD) {
        enc_sam_response_enc_2.sam_sad = response_data;
    } else {
        enc_sam_response_enc_2.sam_sad.data = NULL;
        enc_sam_response_enc_2.sam_sad.length = 0;
    }

    /* encode and encrypt enc_sam_response_enc_2 with as_key */
    retval = encode_krb5_enc_sam_response_enc_2(&enc_sam_response_enc_2,
                                                &scratch);
    if (retval) {
        krb5_free_sam_challenge_2(context, sc2);
        krb5_free_sam_challenge_2_body(context, sc2b);
        return(retval);
    }

    /* Fill in sam_response_2 */
    memset(&sr2, 0, sizeof(sr2));
    sr2.sam_type = sc2b->sam_type;
    sr2.sam_flags = sc2b->sam_flags;
    sr2.sam_track_id = sc2b->sam_track_id;
    sr2.sam_nonce = sc2b->sam_nonce;

    /* Now take care of sr2.sam_enc_nonce_or_sad by encrypting encoded   */
    /* enc_sam_response_enc_2 from above */

    retval = krb5_c_encrypt_length(context, ctx->as_key.enctype,
                                   scratch->length, &ciph_len);
    if (retval) {
        krb5_free_sam_challenge_2(context, sc2);
        krb5_free_sam_challenge_2_body(context, sc2b);
        krb5_free_data(context, scratch);
        return(retval);
    }
    sr2.sam_enc_nonce_or_sad.ciphertext.length = ciph_len;

    sr2.sam_enc_nonce_or_sad.ciphertext.data =
        (char *)malloc(sr2.sam_enc_nonce_or_sad.ciphertext.length);

    if (!sr2.sam_enc_nonce_or_sad.ciphertext.data) {
        krb5_free_sam_challenge_2(context, sc2);
        krb5_free_sam_challenge_2_body(context, sc2b);
        krb5_free_data(context, scratch);
        return(ENOMEM);
    }

    retval = krb5_c_encrypt(context, &ctx->as_key,
                            KRB5_KEYUSAGE_PA_SAM_RESPONSE, NULL, scratch,
                            &sr2.sam_enc_nonce_or_sad);
    if (retval) {
        krb5_free_sam_challenge_2(context, sc2);
        krb5_free_sam_challenge_2_body(context, sc2b);
        krb5_free_data(context, scratch);
        krb5_free_data_contents(context, &sr2.sam_enc_nonce_or_sad.ciphertext);
        return(retval);
    }
    krb5_free_data(context, scratch);
    scratch = NULL;

    /* Encode the sam_response_2 */
    retval = encode_krb5_sam_response_2(&sr2, &scratch);
    krb5_free_sam_challenge_2(context, sc2);
    krb5_free_sam_challenge_2_body(context, sc2b);
    krb5_free_data_contents(context, &sr2.sam_enc_nonce_or_sad.ciphertext);

    if (retval) {
        return (retval);
    }

    /* Almost there, just need to make padata !  */
    sam_padata = malloc(2 * sizeof(*sam_padata));
    if (sam_padata == NULL) {
        krb5_free_data(context, scratch);
        return(ENOMEM);
    }
    sam_padata[0] = malloc(sizeof(krb5_pa_data));
    if (sam_padata[0] == NULL) {
        krb5_free_data(context, scratch);
        free(sam_padata);
        return(ENOMEM);
    }

    sam_padata[0]->magic = KV5M_PA_DATA;
    sam_padata[0]->pa_type = KRB5_PADATA_SAM_RESPONSE_2;
    sam_padata[0]->length = scratch->length;
    sam_padata[0]->contents = (krb5_octet *) scratch->data;
    free(scratch);
    sam_padata[1] = NULL;

    *out_padata = sam_padata;
    cb->disable_fallback(context, rock);

    return(0);
}

static krb5_preauthtype sam2_pa_types[] = {
    KRB5_PADATA_SAM_CHALLENGE_2, 0};

krb5_error_code
clpreauth_sam2_initvt(krb5_context context, int maj_ver, int min_ver,
                      krb5_plugin_vtable vtable)
{
    krb5_clpreauth_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (krb5_clpreauth_vtable)vtable;
    vt->name = "sam2";
    vt->pa_type_list = sam2_pa_types;
    vt->process = sam2_process;
    return 0;
}
