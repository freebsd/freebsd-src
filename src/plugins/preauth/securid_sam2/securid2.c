/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/preauth/securid_sam2/securid2.c */
/*
 * Copyright (C) 2010 by the Massachusetts Institute of Technology.
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
 * Copyright (c) 2002 Naval Research Laboratory (NRL/CCS)
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the software,
 * derivative works or modified versions, and any portions thereof.
 *
 * NRL ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION AND
 * DISCLAIMS ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER
 * RESULTING FROM THE USE OF THIS SOFTWARE.
 */

#ifdef ARL_SECURID_PREAUTH

#include "k5-int.h"
#include <kdb.h>
#include <stdio.h>
#include <adm_proto.h>
#include <syslog.h>
#include <acexport.h>
#include <sdi_defs.h>
#include <sdi_athd.h>
#include "extern.h"

#define KRB5_SAM_SECURID_NEXT_CHALLENGE_MAGIC 0x5ec1d000
struct securid_track_data {
    SDI_HANDLE handle;
    char state;
    char passcode[LENPRNST+1];
    long hostid;
};

#define SECURID_STATE_NEW_PIN           1       /* Ask for a new pin */
#define SECURID_STATE_NEW_PIN_AGAIN     2       /* Ask for new pin again */
#define SECURID_STATE_NEXT_CODE         3       /* Ask for the next pin code */
#define SECURID_STATE_INITIAL           4

static char *PASSCODE_message =         "SecurID Passcode";
static char *NEXT_PASSCODE_message =    "Next Passcode";
static char *NEW_PIN_AGAIN_message =    "New PIN Again";
static char PIN_message[64];            /* Max length should be 50 chars */

/*
 * krb5_error_code get_securid_key():
 *   inputs:  context:  from KDC process
 *            client:   database entry of client executing
 *                      SecurID SAM preauthentication
 *   outputs: client_securid_key: pointer to krb5_keyblock
 *                      which is key for the client's SecurID
 *                      database entry.
 *   returns: 0 on success
 *            KRB5 error codes otherwise
 *
 *   builds pricipal name with final instance of "SECURID" and
 *   finds the database entry, decrypts the key out of the database
 *   and passes the key back to the calling process
 */

static krb5_error_code
get_securid_key(krb5_context context, krb5_db_entry *client,
                krb5_keyblock *client_securid_key)
{
    krb5_db_entry *sam_securid_entry = NULL;
    krb5_key_data *client_securid_key_data = NULL;
    int sam_type = PA_SAM_TYPE_SECURID;
    krb5_error_code retval = 0;

    if (!client_securid_key)
        return KRB5_PREAUTH_NO_KEY;

    retval = sam_get_db_entry(context, client->princ,
                              &sam_type, &sam_securid_entry);
    if (retval)
        return KRB5_PREAUTH_NO_KEY;

    /* Find key with key_type = salt_type = kvno = -1.  This finds the  */
    /* latest kvno in the list.                                         */

    retval = krb5_dbe_find_enctype(context, sam_securid_entry,
                                   -1, -1, -1, &client_securid_key_data);
    if (retval) {
        com_err("krb5kdc", retval,
                "while getting key from client's SAM SecurID entry");
        goto cleanup;
    }
    retval = krb5_dbe_decrypt_key_data(context, NULL, client_securid_key_data,
                                       client_securid_key, NULL);
    if (retval) {
        com_err("krb5kdc", retval,
                "while decrypting key from client's SAM SecurID entry");
        goto cleanup;
    }
cleanup:
    if (sam_securid_entry)
        krb5_db_free_principal(context, sam_securid_entry);
    return retval;
}

static krb5_error_code
securid_decrypt_track_data_2(krb5_context context, krb5_db_entry *client,
                             krb5_data *enc_track_data, krb5_data *output)
{
    krb5_error_code retval;
    krb5_keyblock sam_key;
    krb5_enc_data tmp_enc_data;
    sam_key.contents = NULL;

    retval = get_securid_key(context, client, &sam_key);
    if (retval != 0)
        return retval;

    tmp_enc_data.ciphertext = *enc_track_data;
    tmp_enc_data.enctype = ENCTYPE_UNKNOWN;
    tmp_enc_data.kvno = 0;

    output->length = tmp_enc_data.ciphertext.length;
    free(output->data);
    output->data = k5alloc(output->length, &retval);
    if (output->data == NULL)
        goto cleanup;
    retval = krb5_c_decrypt(context, &sam_key,
                            KRB5_KEYUSAGE_PA_SAM_CHALLENGE_TRACKID, 0,
                            &tmp_enc_data, output);
cleanup:
    krb5_free_keyblock_contents(context, &sam_key);

    if (retval) {
        output->length = 0;
        free(output->data);
        output->data = NULL;
        return retval;
    }

    return 0;
}

static krb5_error_code
securid_encrypt_track_data_2(krb5_context context, krb5_db_entry *client,
                             krb5_data *track_data, krb5_data *output)
{
    krb5_error_code retval;
    size_t olen;
    krb5_keyblock sam_key;
    krb5_enc_data tmp_enc_data;

    output->data = NULL;

    retval = get_securid_key(context,client, &sam_key);
    if (retval != 0)
        return retval;

    retval = krb5_c_encrypt_length(context, sam_key.enctype,
                                   track_data->length, &olen);
    if (retval  != 0)
        goto cleanup;
    assert(olen <= 65536);
    output->length = olen;
    output->data = k5alloc(output->length, &retval);
    if (retval)
        goto cleanup;
    tmp_enc_data.ciphertext = *output;
    tmp_enc_data.enctype = sam_key.enctype;
    tmp_enc_data.kvno = 0;

    retval = krb5_c_encrypt(context, &sam_key,
                            KRB5_KEYUSAGE_PA_SAM_CHALLENGE_TRACKID, 0,
                            track_data, &tmp_enc_data);
cleanup:
    krb5_free_keyblock_contents(context, &sam_key);

    if (retval) {
        output->length = 0;
        free(output->data);
        output->data = NULL;
        return retval;
    }
    return 0;
}


krb5_error_code
get_securid_edata_2(krb5_context context, krb5_db_entry *client,
                    krb5_keyblock *client_key, krb5_sam_challenge_2 *sc2)
{
    krb5_error_code retval;
    krb5_data scratch, track_id = empty_data();
    char *user = NULL;
    char *def_user = "<unknown user>";
    struct securid_track_data sid_track_data;
    krb5_data tmp_data;
    krb5_sam_challenge_2_body sc2b;

    scratch.data = NULL;

    retval = krb5_unparse_name(context, client->princ, &user);
    if (retval)
        goto cleanup;

    memset(&sc2b, 0, sizeof(sc2b));
    sc2b.magic = KV5M_SAM_CHALLENGE_2;
    sc2b.sam_flags = KRB5_SAM_SEND_ENCRYPTED_SAD;
    sc2b.sam_type_name.length = 0;
    sc2b.sam_challenge_label.length = 0;
    sc2b.sam_challenge.length = 0;
    sc2b.sam_response_prompt.data = PASSCODE_message;
    sc2b.sam_response_prompt.length = strlen(sc2b.sam_response_prompt.data);
    sc2b.sam_pk_for_sad.length = 0;
    sc2b.sam_type = PA_SAM_TYPE_SECURID;

    sid_track_data.state = SECURID_STATE_INITIAL;
    sid_track_data.hostid = gethostid();
    tmp_data.data = (char *)&sid_track_data;
    tmp_data.length = sizeof(sid_track_data);
    retval = securid_encrypt_track_data_2(context, client, &tmp_data,
                                          &track_id);
    if (retval != 0) {
        com_err("krb5kdc", retval, "while encrypting nonce track data");
        goto cleanup;
    }
    sc2b.sam_track_id = track_id;

    scratch.data = (char *)&sc2b.sam_nonce;
    scratch.length = sizeof(sc2b.sam_nonce);
    retval = krb5_c_random_make_octets(context, &scratch);
    if (retval) {
        com_err("krb5kdc", retval,
                "while generating nonce data in get_securid_edata_2 (%s)",
                user ? user : def_user);
        goto cleanup;
    }

    /* Get the client's key */
    sc2b.sam_etype = client_key->enctype;

    retval = sam_make_challenge(context, &sc2b, client_key, sc2);
    if (retval) {
        com_err("krb5kdc", retval,
                "while making SAM_CHALLENGE_2 checksum (%s)",
                user ? user : def_user);
    }

cleanup:
    free(user);
    krb5_free_data_contents(context, &track_id);
    return retval;
}

krb5_error_code
verify_securid_data_2(krb5_context context, krb5_db_entry *client,
                      krb5_sam_response_2 *sr2,
                      krb5_enc_tkt_part *enc_tkt_reply, krb5_pa_data *pa,
                      krb5_sam_challenge_2 **sc2_out)
{
    krb5_error_code retval;
    int new_pin = 0;
    krb5_key_data *client_key_data = NULL;
    krb5_keyblock client_key;
    krb5_data scratch;
    krb5_enc_sam_response_enc_2 *esre2 = NULL;
    struct securid_track_data sid_track_data, *trackp = NULL;
    krb5_data tmp_data;
    SDI_HANDLE sd_handle = SDI_HANDLE_NONE;
    krb5_sam_challenge_2 *sc2p = NULL;
    char *cp, *user = NULL;
    char *securid_user = NULL;
    char passcode[LENPRNST+1];
    char max_pin_len, min_pin_len, alpha_pin;

    memset(&client_key, 0, sizeof(client_key));
    memset(&scratch, 0, sizeof(scratch));
    *sc2_out = NULL;

    retval = krb5_unparse_name(context, client->princ, &user);
    if (retval != 0) {
        com_err("krb5kdc", retval,
                "while unparsing client name in verify_securid_data_2");
        return retval;
    }

    if ((sr2->sam_enc_nonce_or_sad.ciphertext.data == NULL) ||
        (sr2->sam_enc_nonce_or_sad.ciphertext.length <= 0)) {
        retval = KRB5KDC_ERR_PREAUTH_FAILED;
        k5_setmsg(context, retval,
                  "No preauth data supplied in verify_securid_data_2 (%s)",
                  user);
        goto cleanup;
    }

    retval = krb5_dbe_find_enctype(context, client,
                                   sr2->sam_enc_nonce_or_sad.enctype,
                                   KRB5_KDB_SALTTYPE_NORMAL,
                                   sr2->sam_enc_nonce_or_sad.kvno,
                                   &client_key_data);
    if (retval) {
        com_err("krb5kdc", retval,
                "while getting client key in verify_securid_data_2 (%s)",
                user);
        goto cleanup;
    }

    retval = krb5_dbe_decrypt_key_data(context, NULL, client_key_data,
                                       &client_key, NULL);
    if (retval != 0) {
        com_err("krb5kdc", retval,
                "while decrypting client key in verify_securid_data_2 (%s)",
                user);
        goto cleanup;
    }

    scratch.length = sr2->sam_enc_nonce_or_sad.ciphertext.length;
    scratch.data = k5alloc(scratch.length, &retval);
    if (retval)
        goto cleanup;
    retval = krb5_c_decrypt(context, &client_key,
                            KRB5_KEYUSAGE_PA_SAM_RESPONSE, 0,
                            &sr2->sam_enc_nonce_or_sad, &scratch);
    if (retval) {
        com_err("krb5kdc", retval,
                "while decrypting SAD in verify_securid_data_2 (%s)", user);
        goto cleanup;
    }

    retval = decode_krb5_enc_sam_response_enc_2(&scratch, &esre2);
    if (retval) {
        com_err("krb5kdc", retval,
                "while decoding SAD in verify_securid_data_2 (%s)", user);
        esre2 = NULL;
        goto cleanup;
    }

    if (sr2->sam_nonce != esre2->sam_nonce) {
        com_err("krb5kdc", KRB5KDC_ERR_PREAUTH_FAILED,
                "while checking nonce in verify_securid_data_2 (%s)", user);
        retval = KRB5KDC_ERR_PREAUTH_FAILED;
        goto cleanup;
    }

    if (esre2->sam_sad.length == 0 || esre2->sam_sad.data == NULL) {
        com_err("krb5kdc", KRB5KDC_ERR_PREAUTH_FAILED,
                "No SecurID passcode in verify_securid_data_2 (%s)", user);
        retval = KRB5KDC_ERR_PREAUTH_FAILED;
        goto cleanup;
    }

    /* Copy out SAD to null-terminated buffer */
    memset(passcode, 0, sizeof(passcode));
    if (esre2->sam_sad.length > (sizeof(passcode) - 1)) {
        retval = KRB5KDC_ERR_PREAUTH_FAILED;
        com_err("krb5kdc", retval,
                "SecurID passcode/PIN too long (%d bytes) in "
                "verify_securid_data_2 (%s)",
                esre2->sam_sad.length, user);
        goto cleanup;
    }
    if (esre2->sam_sad.length > 0)
        memcpy(passcode, esre2->sam_sad.data, esre2->sam_sad.length);

    securid_user = strdup(user);
    if (!securid_user) {
        retval = ENOMEM;
        com_err("krb5kdc", ENOMEM,
                "while copying user name in verify_securid_data_2 (%s)", user);
        goto cleanup;
    }
    cp = strchr(securid_user, '@');
    if (cp != NULL)
        *cp = '\0';

    /* Check for any track_id data that may have state from a previous attempt
     * at SecurID authentication. */

    if (sr2->sam_track_id.data && (sr2->sam_track_id.length > 0)) {
        krb5_data track_id_data;

        memset(&track_id_data, 0, sizeof(track_id_data));
        retval = securid_decrypt_track_data_2(context, client,
                                              &sr2->sam_track_id,
                                              &track_id_data);
        if (retval) {
            com_err("krb5kdc", retval,
                    "while decrypting SecurID trackID in "
                    "verify_securid_data_2 (%s)", user);
            goto cleanup;
        }
        if (track_id_data.length < sizeof (struct securid_track_data)) {
            retval = KRB5KDC_ERR_PREAUTH_FAILED;
            com_err("krb5kdc", retval, "Length of track data incorrect");
            goto cleanup;
        }
        trackp = (struct securid_track_data *)track_id_data.data;

        if(trackp->hostid != gethostid()) {
            krb5_klog_syslog(LOG_INFO, "Unexpected challenge response");
            retval = KRB5KDC_ERR_DISCARD;
            goto cleanup;
        }

        switch(trackp->state) {
        case SECURID_STATE_INITIAL:
            goto initial;
            break;
        case SECURID_STATE_NEW_PIN_AGAIN:
        {
            int pin1_len, pin2_len;

            trackp->handle = ntohl(trackp->handle);
            pin2_len = strlen(passcode);
            pin1_len = strlen(trackp->passcode);

            if ((pin1_len != pin2_len) ||
                (memcmp(passcode, trackp->passcode, pin1_len) != 0)) {
                retval = KRB5KDC_ERR_PREAUTH_FAILED;
                krb5_klog_syslog(LOG_INFO, "New SecurID PIN Failed for user "
                                 "%s: PIN mis-match", user);
                break;
            }
            retval = SD_Pin(trackp->handle, passcode);
            SD_Close(trackp->handle);
            if (retval == ACM_NEW_PIN_ACCEPTED) {
                enc_tkt_reply->flags|=  TKT_FLG_HW_AUTH;
                enc_tkt_reply->flags|=  TKT_FLG_PRE_AUTH;
                krb5_klog_syslog(LOG_INFO, "SecurID PIN Accepted for %s in "
                                 "verify_securid_data_2",
                                 securid_user);
                retval = 0;
            } else {
                retval = KRB5KDC_ERR_PREAUTH_FAILED;
                krb5_klog_syslog(LOG_INFO,
                                 "SecurID PIN Failed for user %s (AceServer "
                                 "returns %d) in verify_securid_data_2",
                                 user, retval);
            }
            break;
        }
        case SECURID_STATE_NEW_PIN: {
            krb5_sam_challenge_2_body sc2b;
            sc2p = k5alloc(sizeof *sc2p, &retval);
            if (retval)
                goto cleanup;
            memset(sc2p, 0, sizeof(*sc2p));
            memset(&sc2b, 0, sizeof(sc2b));
            sc2b.sam_type = PA_SAM_TYPE_SECURID;
            sc2b.sam_response_prompt.data = NEW_PIN_AGAIN_message;
            sc2b.sam_response_prompt.length =
                strlen(sc2b.sam_response_prompt.data);
            sc2b.sam_flags = KRB5_SAM_SEND_ENCRYPTED_SAD;
            sc2b.sam_etype = client_key.enctype;

            tmp_data.data = (char *)&sc2b.sam_nonce;
            tmp_data.length = sizeof(sc2b.sam_nonce);
            if ((retval = krb5_c_random_make_octets(context, &tmp_data))) {
                com_err("krb5kdc", retval,
                        "while making nonce for SecurID new "
                        "PIN2 SAM_CHALLENGE_2 (%s)", user);
                goto cleanup;
            }
            sid_track_data.state = SECURID_STATE_NEW_PIN_AGAIN;
            sid_track_data.handle = trackp->handle;
            sid_track_data.hostid = gethostid();
            /* Should we complain if sizes don't work ??  */
            memcpy(sid_track_data.passcode, passcode,
                   sizeof(sid_track_data.passcode));
            tmp_data.data = (char *)&sid_track_data;
            tmp_data.length = sizeof(sid_track_data);
            if ((retval = securid_encrypt_track_data_2(context, client,
                                                       &tmp_data,
                                                       &sc2b.sam_track_id))) {
                com_err("krb5kdc", retval,
                        "while encrypting NEW PIN2 SecurID "
                        "track data for SAM_CHALLENGE_2 (%s)",
                        securid_user);
                goto cleanup;
            }
            retval = sam_make_challenge(context, &sc2b, &client_key, sc2p);
            if (retval) {
                com_err("krb5kdc", retval,
                        "while making cksum for "
                        "SAM_CHALLENGE_2 (new PIN2) (%s)", securid_user);
                goto cleanup;
            }
            krb5_klog_syslog(LOG_INFO,
                             "Requesting verification of new PIN for user %s",
                             securid_user);
            *sc2_out = sc2p;
            sc2p = NULL;
            /*sc2_out may be set even on error path*/
            retval = KRB5KDC_ERR_PREAUTH_REQUIRED;
            goto cleanup;
        }
        case SECURID_STATE_NEXT_CODE:
            trackp->handle = ntohl(trackp->handle);
            retval = SD_Next(trackp->handle, passcode);
            SD_Close(trackp->handle);
            if (retval == ACM_OK) {
                enc_tkt_reply->flags |=  TKT_FLG_HW_AUTH | TKT_FLG_PRE_AUTH;

                krb5_klog_syslog(LOG_INFO, "Next SecurID Code Accepted for "
                                 "user %s", securid_user);
                retval = 0;
            } else {
                krb5_klog_syslog(LOG_INFO, "Next SecurID Code Failed for user "
                                 "%s (AceServer returns %d) in "
                                 "verify_securid_data_2", user, retval);
                retval = KRB5KDC_ERR_PREAUTH_FAILED;
            }
            break;
        }
    } else {            /* No track data, this is first of N attempts */
    initial:
        retval = SD_Init(&sd_handle);
        if (retval) {
            com_err("krb5kdc", KRB5KDC_ERR_PREAUTH_FAILED,
                    "SD_Init() returns error %d in verify_securid_data_2 (%s)",
                    retval, securid_user);
            retval = KRB5KDC_ERR_PREAUTH_FAILED;
            goto cleanup;
        }

        retval = SD_Lock(sd_handle, securid_user);
        if (retval != ACM_OK) {
            SD_Close(sd_handle);
            retval = KRB5KDC_ERR_PREAUTH_FAILED;
            krb5_klog_syslog(LOG_INFO,
                             "SD_Lock() failed (AceServer returns %d) for %s",
                             retval, securid_user);
            goto cleanup;
        }

        retval = SD_Check(sd_handle, passcode, securid_user);
        switch (retval) {
        case ACM_OK:
            SD_Close(sd_handle);
            enc_tkt_reply->flags|=  TKT_FLG_HW_AUTH;
            enc_tkt_reply->flags|=  TKT_FLG_PRE_AUTH;
            krb5_klog_syslog(LOG_INFO, "SecurID passcode accepted for user %s",
                             user);
            retval = 0;
            break;
        case ACM_ACCESS_DENIED:
            SD_Close(sd_handle);
            retval = KRB5KDC_ERR_PREAUTH_FAILED;
            krb5_klog_syslog(LOG_INFO, "AceServer returns Access Denied for "
                             "user %s (SAM2)", user);
            goto cleanup;
        case ACM_NEW_PIN_REQUIRED:
            new_pin = 1;
            /*fall through*/
        case ACM_NEXT_CODE_REQUIRED: {
            krb5_sam_challenge_2_body sc2b;
            sc2p = k5alloc(sizeof *sc2p, &retval);
            if (retval)
                goto cleanup;

            memset(sc2p, 0, sizeof(*sc2p));
            memset(&sc2b, 0, sizeof(sc2b));

            sc2b.sam_type = PA_SAM_TYPE_SECURID;
            sc2b.sam_response_prompt.data = NEXT_PASSCODE_message;
            sc2b.sam_response_prompt.length =
                strlen(sc2b.sam_response_prompt.data);
            sc2b.sam_flags = KRB5_SAM_SEND_ENCRYPTED_SAD;
            sc2b.sam_etype = client_key.enctype;
            if (new_pin) {
                if ((AceGetMaxPinLen(sd_handle, &max_pin_len) == ACE_SUCCESS)
                    && (AceGetMinPinLen(sd_handle,
                                        &min_pin_len) == ACE_SUCCESS)
                    && (AceGetAlphanumeric(sd_handle,
                                           &alpha_pin) == ACE_SUCCESS)) {
                    sprintf(PIN_message,
                            "New PIN must contain %d to %d %sdigits",
                            min_pin_len, max_pin_len,
                            (alpha_pin == 0) ? "" : "alphanumeric ");
                    sc2b.sam_challenge_label.data = PIN_message;
                    sc2b.sam_challenge_label.length =
                        strlen(sc2b.sam_challenge_label.data);
                } else {
                    sc2b.sam_challenge_label.length = 0;
                }
            }

            tmp_data.data = (char *)&sc2b.sam_nonce;
            tmp_data.length = sizeof(sc2b.sam_nonce);
            if ((retval = krb5_c_random_make_octets(context, &tmp_data))) {
                com_err("krb5kdc", retval,
                        "while making nonce for SecurID SAM_CHALLENGE_2 (%s)",
                        user);
                goto cleanup;
            }
            if (new_pin)
                sid_track_data.state = SECURID_STATE_NEW_PIN;
            else
                sid_track_data.state = SECURID_STATE_NEXT_CODE;
            sid_track_data.handle = htonl(sd_handle);
            sid_track_data.hostid = gethostid();
            tmp_data.data = (char *)&sid_track_data;
            tmp_data.length = sizeof(sid_track_data);
            retval = securid_encrypt_track_data_2(context, client, &tmp_data,
                                                  &sc2b.sam_track_id);
            if (retval) {
                com_err("krb5kdc", retval,
                        "while encrypting SecurID track "
                        "data for SAM_CHALLENGE_2 (%s)",
                        securid_user);
                goto cleanup;
            }
            retval = sam_make_challenge(context, &sc2b, &client_key, sc2p);
            if (retval) {
                com_err("krb5kdc", retval,
                        "while making cksum for SAM_CHALLENGE_2 (%s)",
                        securid_user);
            }
            if (new_pin)
                krb5_klog_syslog(LOG_INFO, "New SecurID PIN required for "
                                 "user %s", securid_user);
            else
                krb5_klog_syslog(LOG_INFO, "Next SecurID passcode required "
                                 "for user %s", securid_user);
            *sc2_out = sc2p;
            sc2p = NULL;
            retval = KRB5KDC_ERR_PREAUTH_REQUIRED;
            /*sc2_out is permitted as an output on error path*/
            goto cleanup;
        }
        default:
            com_err("krb5kdc", KRB5KDC_ERR_PREAUTH_FAILED,
                    "AceServer returns unknown error code %d "
                    "in verify_securid_data_2\n", retval);
            retval = KRB5KDC_ERR_PREAUTH_FAILED;
            goto cleanup;
        }
    }   /* no track_id data */

cleanup:
    krb5_free_keyblock_contents(context, &client_key);
    free(scratch.data);
    krb5_free_enc_sam_response_enc_2(context, esre2);
    free(user);
    free(securid_user);
    free(trackp);
    krb5_free_sam_challenge_2(context, sc2p);
    return retval;
}

#endif /* ARL_SECURID_PREAUTH */
