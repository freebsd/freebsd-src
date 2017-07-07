/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * NAME
 *    cred.c
 *
 * DESCRIPTION
 *    Provide an interface to assemble and disassemble krb5_cred
 *    structures.
 *
 */
#include "k5-int.h"
#include "int-proto.h"
#include "cleanup.h"
#include "auth_con.h"

#include <stddef.h>           /* NULL */
#include <stdlib.h>           /* malloc */
#include <errno.h>            /* ENOMEM */

/*-------------------- encrypt_credencpart --------------------*/

/*
 * encrypt the enc_part of krb5_cred
 */
static krb5_error_code
encrypt_credencpart(krb5_context context, krb5_cred_enc_part *pcredpart,
                    krb5_key pkey, krb5_enc_data *pencdata)
{
    krb5_error_code       retval;
    krb5_data           * scratch;

    /* start by encoding to-be-encrypted part of the message */
    if ((retval = encode_krb5_enc_cred_part(pcredpart, &scratch)))
        return retval;

    /*
     * If the keyblock is NULL, just copy the data from the encoded
     * data to the ciphertext area.
     */
    if (pkey == NULL) {
        pencdata->ciphertext.data = scratch->data;
        pencdata->ciphertext.length = scratch->length;
        free(scratch);
        return 0;
    }

    /* call the encryption routine */
    retval = k5_encrypt_keyhelper(context, pkey,
                                  KRB5_KEYUSAGE_KRB_CRED_ENCPART, scratch,
                                  pencdata);

    memset(scratch->data, 0, scratch->length);
    krb5_free_data(context, scratch);

    return retval;
}

/*----------------------- krb5_mk_ncred_basic -----------------------*/

static krb5_error_code
krb5_mk_ncred_basic(krb5_context context,
                    krb5_creds **ppcreds, krb5_int32 nppcreds,
                    krb5_key key, krb5_replay_data *replaydata,
                    krb5_address *local_addr, krb5_address *remote_addr,
                    krb5_cred *pcred)
{
    krb5_cred_enc_part    credenc;
    krb5_error_code       retval;
    size_t                size;
    int                   i;

    credenc.magic = KV5M_CRED_ENC_PART;

    credenc.s_address = 0;
    credenc.r_address = 0;
    if (local_addr) krb5_copy_addr(context, local_addr, &credenc.s_address);
    if (remote_addr) krb5_copy_addr(context, remote_addr, &credenc.r_address);

    credenc.nonce = replaydata->seq;
    credenc.usec = replaydata->usec;
    credenc.timestamp = replaydata->timestamp;

    /* Get memory for creds and initialize it */
    size = sizeof(krb5_cred_info *) * (nppcreds + 1);
    credenc.ticket_info = (krb5_cred_info **) calloc(1, size);
    if (credenc.ticket_info == NULL)
        return ENOMEM;

    /*
     * For each credential in the list, initialize a cred info
     * structure and copy the ticket into the ticket list.
     */
    for (i = 0; i < nppcreds; i++) {
        credenc.ticket_info[i] = calloc(1, sizeof(krb5_cred_info));
        if (credenc.ticket_info[i] == NULL) {
            retval = ENOMEM;
            goto cleanup;
        }
        credenc.ticket_info[i+1] = NULL;

        credenc.ticket_info[i]->magic = KV5M_CRED_INFO;
        credenc.ticket_info[i]->times = ppcreds[i]->times;
        credenc.ticket_info[i]->flags = ppcreds[i]->ticket_flags;

        if ((retval = decode_krb5_ticket(&ppcreds[i]->ticket,
                                         &pcred->tickets[i])))
            goto cleanup;

        if ((retval = krb5_copy_keyblock(context, &ppcreds[i]->keyblock,
                                         &credenc.ticket_info[i]->session)))
            goto cleanup;

        if ((retval = krb5_copy_principal(context, ppcreds[i]->client,
                                          &credenc.ticket_info[i]->client)))
            goto cleanup;

        if ((retval = krb5_copy_principal(context, ppcreds[i]->server,
                                          &credenc.ticket_info[i]->server)))
            goto cleanup;

        if ((retval = krb5_copy_addresses(context, ppcreds[i]->addresses,
                                          &credenc.ticket_info[i]->caddrs)))
            goto cleanup;
    }

    /*
     * NULL terminate the lists.
     */
    pcred->tickets[i] = NULL;

    /* encrypt the credential encrypted part */
    retval = encrypt_credencpart(context, &credenc, key, &pcred->enc_part);

cleanup:
    krb5_free_cred_enc_part(context, &credenc);
    return retval;
}

/*----------------------- krb5_mk_ncred -----------------------*/

/*
 * This functions takes as input an array of krb5_credentials, and
 * outputs an encoded KRB_CRED message suitable for krb5_rd_cred
 */
krb5_error_code KRB5_CALLCONV
krb5_mk_ncred(krb5_context context, krb5_auth_context auth_context,
              krb5_creds **ppcreds, krb5_data **ppdata,
              krb5_replay_data *outdata)
{
    krb5_address * premote_fulladdr = NULL;
    krb5_address * plocal_fulladdr = NULL;
    krb5_address remote_fulladdr;
    krb5_address local_fulladdr;
    krb5_error_code     retval;
    krb5_key            key;
    krb5_replay_data    replaydata;
    krb5_cred            * pcred;
    krb5_int32          ncred;
    krb5_boolean increased_sequence = FALSE;

    local_fulladdr.contents = 0;
    remote_fulladdr.contents = 0;
    memset(&replaydata, 0, sizeof(krb5_replay_data));

    if (ppcreds == NULL)
        return KRB5KRB_AP_ERR_BADADDR;

    /*
     * Allocate memory for a NULL terminated list of tickets.
     */
    for (ncred = 0; ppcreds[ncred]; ncred++)
        ;

    if ((pcred = (krb5_cred *)calloc(1, sizeof(krb5_cred))) == NULL)
        return ENOMEM;

    if ((pcred->tickets
         = (krb5_ticket **)calloc((size_t)ncred+1,
                                  sizeof(krb5_ticket *))) == NULL) {
        retval = ENOMEM;
        goto error;
    }

    /* Get keyblock */
    if ((key = auth_context->send_subkey) == NULL)
        key = auth_context->key;

    /* Get replay info */
    if ((auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_DO_TIME) &&
        (auth_context->rcache == NULL)) {
        retval = KRB5_RC_REQUIRED;
        goto error;
    }

    if (((auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_RET_TIME) ||
         (auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_RET_SEQUENCE))
        && (outdata == NULL)) {
        /* Need a better error */
        retval = KRB5_RC_REQUIRED;
        goto error;
    }

    if ((retval = krb5_us_timeofday(context, &replaydata.timestamp,
                                    &replaydata.usec)))
        goto error;
    if (auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_RET_TIME) {
        outdata->timestamp = replaydata.timestamp;
        outdata->usec = replaydata.usec;
    }
    if ((auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_DO_SEQUENCE) ||
        (auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_RET_SEQUENCE)) {
        replaydata.seq = auth_context->local_seq_number++;
        increased_sequence = TRUE;
        if (auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_RET_SEQUENCE)
            outdata->seq = replaydata.seq;
    }

    if (auth_context->local_addr) {
        if (auth_context->local_port) {
            if ((retval = krb5_make_fulladdr(context, auth_context->local_addr,
                                             auth_context->local_port,
                                             &local_fulladdr)))
                goto error;
            plocal_fulladdr = &local_fulladdr;
        } else {
            plocal_fulladdr = auth_context->local_addr;
        }
    }

    if (auth_context->remote_addr) {
        if (auth_context->remote_port) {
            if ((retval = krb5_make_fulladdr(context,auth_context->remote_addr,
                                             auth_context->remote_port,
                                             &remote_fulladdr)))
                goto error;
            premote_fulladdr = &remote_fulladdr;
        } else {
            premote_fulladdr = auth_context->remote_addr;
        }
    }

    /* Setup creds structure */
    if ((retval = krb5_mk_ncred_basic(context, ppcreds, ncred, key,
                                      &replaydata, plocal_fulladdr,
                                      premote_fulladdr, pcred))) {
        goto error;
    }

    if (auth_context->auth_context_flags & KRB5_AUTH_CONTEXT_DO_TIME) {
        krb5_donot_replay replay;

        if ((retval = krb5_gen_replay_name(context, auth_context->local_addr,
                                           "_forw", &replay.client)))
            goto error;

        replay.server = "";             /* XXX */
        replay.msghash = NULL;
        replay.cusec = replaydata.usec;
        replay.ctime = replaydata.timestamp;
        if ((retval = krb5_rc_store(context, auth_context->rcache, &replay))) {
            /* should we really error out here? XXX */
            free(replay.client);
            goto error;
        }
        free(replay.client);
    }

    /* Encode creds structure */
    retval = encode_krb5_cred(pcred, ppdata);

error:
    free(local_fulladdr.contents);
    free(remote_fulladdr.contents);
    krb5_free_cred(context, pcred);

    if (retval) {
        if (increased_sequence)
            auth_context->local_seq_number--;
    }
    return retval;
}

/*----------------------- krb5_mk_1cred -----------------------*/

/*
 * A convenience function that calls krb5_mk_ncred.
 */
krb5_error_code KRB5_CALLCONV
krb5_mk_1cred(krb5_context context, krb5_auth_context auth_context,
              krb5_creds *pcreds, krb5_data **ppdata,
              krb5_replay_data *outdata)
{
    krb5_error_code retval;
    krb5_creds **ppcreds;

    if ((ppcreds = (krb5_creds **)malloc(sizeof(*ppcreds) * 2)) == NULL) {
        return ENOMEM;
    }

    ppcreds[0] = pcreds;
    ppcreds[1] = NULL;

    retval = krb5_mk_ncred(context, auth_context, ppcreds,
                           ppdata, outdata);

    free(ppcreds);
    return retval;
}
