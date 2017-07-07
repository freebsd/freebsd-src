/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/asn.1/ktest_equal.c */
/*
 * Copyright (C) 1994 by the Massachusetts Institute of Technology.
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

#include <stdlib.h>
#include <stdio.h>
#include "ktest_equal.h"

#define FALSE 0
#define TRUE 1

#define struct_equal(field,comparator)          \
    comparator(&(ref->field),&(var->field))

#define ptr_equal(field,comparator)             \
    comparator(ref->field,var->field)

#define scalar_equal(field)                     \
    ((ref->field) == (var->field))

#define len_equal(length,field,comparator)              \
    ((ref->length == var->length) &&                    \
     comparator(ref->length,ref->field,var->field))

int
ktest_equal_authenticator(krb5_authenticator *ref, krb5_authenticator *var)
{
    int p = TRUE;

    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && ptr_equal(client,ktest_equal_principal_data);
    p = p && ptr_equal(checksum,ktest_equal_checksum);
    p = p && scalar_equal(cusec);
    p = p && scalar_equal(ctime);
    p = p && ptr_equal(subkey,ktest_equal_keyblock);
    p = p && scalar_equal(seq_number);
    p = p && ptr_equal(authorization_data,ktest_equal_authorization_data);
    return p;
}

int
ktest_equal_principal_data(krb5_principal_data *ref, krb5_principal_data *var)
{
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    return(struct_equal(realm,ktest_equal_data) &&
           len_equal(length,data,ktest_equal_array_of_data) &&
           scalar_equal(type));
}

int
ktest_equal_authdata(krb5_authdata *ref, krb5_authdata *var)
{
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    return(scalar_equal(ad_type) &&
           len_equal(length,contents,ktest_equal_array_of_octet));
}

int
ktest_equal_checksum(krb5_checksum *ref, krb5_checksum *var)
{
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    return(scalar_equal(checksum_type) && len_equal(length,contents,ktest_equal_array_of_octet));
}

int
ktest_equal_keyblock(krb5_keyblock *ref, krb5_keyblock *var)
{
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    return(scalar_equal(enctype) && len_equal(length,contents,ktest_equal_array_of_octet));
}

int
ktest_equal_data(krb5_data *ref, krb5_data *var)
{
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    return(len_equal(length,data,ktest_equal_array_of_char));
}

int
ktest_equal_ticket(krb5_ticket *ref, krb5_ticket *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && ptr_equal(server,ktest_equal_principal_data);
    p = p && struct_equal(enc_part,ktest_equal_enc_data);
    /* enc_part2 is irrelevant, as far as the ASN.1 code is concerned */
    return p;
}

int
ktest_equal_enc_data(krb5_enc_data *ref, krb5_enc_data *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && scalar_equal(enctype);
    p = p && scalar_equal(kvno);
    p = p && struct_equal(ciphertext,ktest_equal_data);
    return p;
}

int
ktest_equal_encryption_key(krb5_keyblock *ref, krb5_keyblock *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && scalar_equal(enctype);
    p = p && len_equal(length,contents,ktest_equal_array_of_octet);
    return p;
}

int
ktest_equal_enc_tkt_part(krb5_enc_tkt_part *ref, krb5_enc_tkt_part *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && scalar_equal(flags);
    p = p && ptr_equal(session,ktest_equal_encryption_key);
    p = p && ptr_equal(client,ktest_equal_principal_data);
    p = p && struct_equal(transited,ktest_equal_transited);
    p = p && struct_equal(times,ktest_equal_ticket_times);
    p = p && ptr_equal(caddrs,ktest_equal_addresses);
    p = p && ptr_equal(authorization_data,ktest_equal_authorization_data);
    return p;
}

int
ktest_equal_transited(krb5_transited *ref, krb5_transited *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && scalar_equal(tr_type);
    p = p && struct_equal(tr_contents,ktest_equal_data);
    return p;
}

int
ktest_equal_ticket_times(krb5_ticket_times *ref, krb5_ticket_times *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && scalar_equal(authtime);
    p = p && scalar_equal(starttime);
    p = p && scalar_equal(endtime);
    p = p && scalar_equal(renew_till);
    return p;
}

int
ktest_equal_address(krb5_address *ref, krb5_address *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && scalar_equal(addrtype);
    p = p && len_equal(length,contents,ktest_equal_array_of_octet);
    return p;
}

int
ktest_equal_enc_kdc_rep_part(krb5_enc_kdc_rep_part *ref,
                             krb5_enc_kdc_rep_part *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && ptr_equal(session,ktest_equal_keyblock);
    p = p && ptr_equal(last_req,ktest_equal_last_req);
    p = p && scalar_equal(nonce);
    p = p && scalar_equal(key_exp);
    p = p && scalar_equal(flags);
    p = p && struct_equal(times,ktest_equal_ticket_times);
    p = p && ptr_equal(server,ktest_equal_principal_data);
    p = p && ptr_equal(caddrs,ktest_equal_addresses);
    return p;
}

int
ktest_equal_priv(krb5_priv *ref, krb5_priv *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && struct_equal(enc_part,ktest_equal_enc_data);
    return p;
}

int
ktest_equal_cred(krb5_cred *ref, krb5_cred *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && ptr_equal(tickets,ktest_equal_sequence_of_ticket);
    p = p && struct_equal(enc_part,ktest_equal_enc_data);
    return p;
}

int
ktest_equal_error(krb5_error *ref, krb5_error *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && scalar_equal(ctime);
    p = p && scalar_equal(cusec);
    p = p && scalar_equal(susec);
    p = p && scalar_equal(stime);
    p = p && scalar_equal(error);
    p = p && ptr_equal(client,ktest_equal_principal_data);
    p = p && ptr_equal(server,ktest_equal_principal_data);
    p = p && struct_equal(text,ktest_equal_data);
    p = p && struct_equal(e_data,ktest_equal_data);
    return p;
}

int
ktest_equal_ap_req(krb5_ap_req *ref, krb5_ap_req *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && scalar_equal(ap_options);
    p = p && ptr_equal(ticket,ktest_equal_ticket);
    p = p && struct_equal(authenticator,ktest_equal_enc_data);
    return p;
}

int
ktest_equal_ap_rep(krb5_ap_rep *ref, krb5_ap_rep *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && struct_equal(enc_part,ktest_equal_enc_data);
    return p;
}

int
ktest_equal_ap_rep_enc_part(krb5_ap_rep_enc_part *ref,
                            krb5_ap_rep_enc_part *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && scalar_equal(ctime);
    p = p && scalar_equal(cusec);
    p = p && ptr_equal(subkey,ktest_equal_encryption_key);
    p = p && scalar_equal(seq_number);
    return p;
}

int
ktest_equal_safe(krb5_safe *ref, krb5_safe *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && struct_equal(user_data,ktest_equal_data);
    p = p && scalar_equal(timestamp);
    p = p && scalar_equal(usec);
    p = p && scalar_equal(seq_number);
    p = p && ptr_equal(s_address,ktest_equal_address);
    p = p && ptr_equal(r_address,ktest_equal_address);
    p = p && ptr_equal(checksum,ktest_equal_checksum);
    return p;
}


int
ktest_equal_enc_cred_part(krb5_cred_enc_part *ref, krb5_cred_enc_part *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && scalar_equal(nonce);
    p = p && scalar_equal(timestamp);
    p = p && scalar_equal(usec);
    p = p && ptr_equal(s_address,ktest_equal_address);
    p = p && ptr_equal(r_address,ktest_equal_address);
    p = p && ptr_equal(ticket_info,ktest_equal_sequence_of_cred_info);
    return p;
}

int
ktest_equal_enc_priv_part(krb5_priv_enc_part *ref, krb5_priv_enc_part *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && struct_equal(user_data,ktest_equal_data);
    p = p && scalar_equal(timestamp);
    p = p && scalar_equal(usec);
    p = p && scalar_equal(seq_number);
    p = p && ptr_equal(s_address,ktest_equal_address);
    p = p && ptr_equal(r_address,ktest_equal_address);
    return p;
}

int
ktest_equal_as_rep(krb5_kdc_rep *ref, krb5_kdc_rep *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && scalar_equal(msg_type);
    p = p && ptr_equal(padata,ktest_equal_sequence_of_pa_data);
    p = p && ptr_equal(client,ktest_equal_principal_data);
    p = p && ptr_equal(ticket,ktest_equal_ticket);
    p = p && struct_equal(enc_part,ktest_equal_enc_data);
    p = p && ptr_equal(enc_part2,ktest_equal_enc_kdc_rep_part);
    return p;
}

int
ktest_equal_tgs_rep(krb5_kdc_rep *ref, krb5_kdc_rep *var)
{
    return ktest_equal_as_rep(ref,var);
}

int
ktest_equal_as_req(krb5_kdc_req *ref, krb5_kdc_req *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && scalar_equal(msg_type);
    p = p && ptr_equal(padata,ktest_equal_sequence_of_pa_data);
    p = p && scalar_equal(kdc_options);
    p = p && ptr_equal(client,ktest_equal_principal_data);
    p = p && ptr_equal(server,ktest_equal_principal_data);
    p = p && scalar_equal(from);
    p = p && scalar_equal(till);
    p = p && scalar_equal(rtime);
    p = p && scalar_equal(nonce);
    p = p && len_equal(nktypes,ktype,ktest_equal_array_of_enctype);
    p = p && ptr_equal(addresses,ktest_equal_addresses);
    p = p && struct_equal(authorization_data,ktest_equal_enc_data);
/* This field isn't actually in the ASN.1 encoding. */
/* p = p && ptr_equal(unenc_authdata,ktest_equal_authorization_data); */
    return p;
}

int
ktest_equal_tgs_req(krb5_kdc_req *ref, krb5_kdc_req *var)
{
    return ktest_equal_as_req(ref,var);
}

int
ktest_equal_kdc_req_body(krb5_kdc_req *ref, krb5_kdc_req *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && scalar_equal(kdc_options);
    p = p && ptr_equal(client,ktest_equal_principal_data);
    p = p && ptr_equal(server,ktest_equal_principal_data);
    p = p && scalar_equal(from);
    p = p && scalar_equal(till);
    p = p && scalar_equal(rtime);
    p = p && scalar_equal(nonce);
    p = p && len_equal(nktypes,ktype,ktest_equal_array_of_enctype);
    p = p && ptr_equal(addresses,ktest_equal_addresses);
    p = p && struct_equal(authorization_data,ktest_equal_enc_data);
    /* This isn't part of the ASN.1 encoding. */
    /* p = p && ptr_equal(unenc_authdata,ktest_equal_authorization_data); */
    return p;
}

int
ktest_equal_last_req_entry(krb5_last_req_entry *ref, krb5_last_req_entry *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && scalar_equal(lr_type);
    p = p && scalar_equal(value);
    return p;
}

int
ktest_equal_pa_data(krb5_pa_data *ref, krb5_pa_data *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && scalar_equal(pa_type);
    p = p && len_equal(length,contents,ktest_equal_array_of_octet);
    return p;
}

int
ktest_equal_cred_info(krb5_cred_info *ref, krb5_cred_info *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && ptr_equal(session,ktest_equal_keyblock);
    p = p && ptr_equal(client,ktest_equal_principal_data);
    p = p && ptr_equal(server,ktest_equal_principal_data);
    p = p && scalar_equal(flags);
    p = p && struct_equal(times,ktest_equal_ticket_times);
    p = p && ptr_equal(caddrs,ktest_equal_addresses);

    return p;
}

int
ktest_equal_krb5_etype_info_entry(krb5_etype_info_entry *ref,
                                  krb5_etype_info_entry *var)
{
    if (ref->etype != var->etype)
        return FALSE;
    if (ref->length != var->length)
        return FALSE;
    if (ref->length > 0 && ref->length != KRB5_ETYPE_NO_SALT)
        if (memcmp(ref->salt, var->salt, ref->length) != 0)
            return FALSE;
    return TRUE;
}

int
ktest_equal_krb5_pa_enc_ts(krb5_pa_enc_ts *ref, krb5_pa_enc_ts *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && scalar_equal(patimestamp);
    p = p && scalar_equal(pausec);
    return p;
}

#define equal_str(f) struct_equal(f,ktest_equal_data)

int
ktest_equal_sam_challenge_2_body(krb5_sam_challenge_2_body *ref,
                                 krb5_sam_challenge_2_body *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && scalar_equal(sam_type);
    p = p && scalar_equal(sam_flags);
    p = p && equal_str(sam_type_name);
    p = p && equal_str(sam_track_id);
    p = p && equal_str(sam_challenge_label);
    p = p && equal_str(sam_challenge);
    p = p && equal_str(sam_response_prompt);
    p = p && equal_str(sam_pk_for_sad);
    p = p && scalar_equal(sam_nonce);
    p = p && scalar_equal(sam_etype);
    return p;
}

int
ktest_equal_sam_challenge_2(krb5_sam_challenge_2 *ref,
                            krb5_sam_challenge_2 *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && equal_str(sam_challenge_2_body);
    p = p && ptr_equal(sam_cksum,ktest_equal_sequence_of_checksum);
    return p;
}

int
ktest_equal_pa_for_user(krb5_pa_for_user *ref, krb5_pa_for_user *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && ptr_equal(user, ktest_equal_principal_data);
    p = p && struct_equal(cksum, ktest_equal_checksum);
    p = p && equal_str(auth_package);
    return p;
}

int
ktest_equal_pa_s4u_x509_user(krb5_pa_s4u_x509_user *ref,
                             krb5_pa_s4u_x509_user *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && scalar_equal(user_id.nonce);
    p = p && ptr_equal(user_id.user,ktest_equal_principal_data);
    p = p && struct_equal(user_id.subject_cert,ktest_equal_data);
    p = p && scalar_equal(user_id.options);
    p = p && struct_equal(cksum,ktest_equal_checksum);
    return p;
}

int
ktest_equal_ad_kdcissued(krb5_ad_kdcissued *ref, krb5_ad_kdcissued *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && struct_equal(ad_checksum,ktest_equal_checksum);
    p = p && ptr_equal(i_principal,ktest_equal_principal_data);
    p = p && ptr_equal(elements,ktest_equal_authorization_data);
    return p;
}

int
ktest_equal_ad_signedpath_data(krb5_ad_signedpath_data *ref,
                               krb5_ad_signedpath_data *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && ptr_equal(client,ktest_equal_principal_data);
    p = p && scalar_equal(authtime);
    p = p && ptr_equal(delegated,ktest_equal_sequence_of_principal);
    p = p && ptr_equal(method_data,ktest_equal_sequence_of_pa_data);
    p = p && ptr_equal(authorization_data,ktest_equal_authorization_data);
    return p;
}

int
ktest_equal_ad_signedpath(krb5_ad_signedpath *ref, krb5_ad_signedpath *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && scalar_equal(enctype);
    p = p && struct_equal(checksum,ktest_equal_checksum);
    p = p && ptr_equal(delegated,ktest_equal_sequence_of_principal);
    p = p && ptr_equal(method_data,ktest_equal_sequence_of_pa_data);
    return p;
}

int
ktest_equal_iakerb_header(krb5_iakerb_header *ref, krb5_iakerb_header *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && struct_equal(target_realm,ktest_equal_data);
    p = p && ptr_equal(cookie,ktest_equal_data);
    return p;
}

int
ktest_equal_iakerb_finished(krb5_iakerb_finished *ref,
                            krb5_iakerb_finished *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && struct_equal(checksum,ktest_equal_checksum);
    return p;
}

static int
ktest_equal_fast_finished(krb5_fast_finished *ref, krb5_fast_finished *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && scalar_equal(timestamp);
    p = p && scalar_equal(usec);
    p = p && ptr_equal(client, ktest_equal_principal_data);
    p = p && struct_equal(ticket_checksum, ktest_equal_checksum);
    return p;
}

int
ktest_equal_fast_response(krb5_fast_response *ref, krb5_fast_response *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && ptr_equal(padata, ktest_equal_sequence_of_pa_data);
    p = p && ptr_equal(strengthen_key, ktest_equal_keyblock);
    p = p && ptr_equal(finished, ktest_equal_fast_finished);
    p = p && scalar_equal(nonce);
    return p;
}

static int
ktest_equal_algorithm_identifier(krb5_algorithm_identifier *ref,
                                 krb5_algorithm_identifier *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && equal_str(algorithm);
    p = p && equal_str(parameters);
    return p;
}

int
ktest_equal_otp_tokeninfo(krb5_otp_tokeninfo *ref, krb5_otp_tokeninfo *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && scalar_equal(flags);
    p = p && equal_str(vendor);
    p = p && equal_str(challenge);
    p = p && scalar_equal(length);
    p = p && scalar_equal(format);
    p = p && equal_str(token_id);
    p = p && equal_str(alg_id);
    p = p && ptr_equal(supported_hash_alg,
                       ktest_equal_sequence_of_algorithm_identifier);
    p = p && scalar_equal(iteration_count);
    return p;
}

int
ktest_equal_pa_otp_challenge(krb5_pa_otp_challenge *ref,
                             krb5_pa_otp_challenge *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && equal_str(nonce);
    p = p && equal_str(service);
    p = p && ptr_equal(tokeninfo, ktest_equal_sequence_of_otp_tokeninfo);
    p = p && equal_str(salt);
    p = p && equal_str(s2kparams);
    return p;
}

int
ktest_equal_pa_otp_req(krb5_pa_otp_req *ref, krb5_pa_otp_req *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && scalar_equal(flags);
    p = p && equal_str(nonce);
    p = p && struct_equal(enc_data, ktest_equal_enc_data);
    p = p && ptr_equal(hash_alg, ktest_equal_algorithm_identifier);
    p = p && scalar_equal(iteration_count);
    p = p && equal_str(otp_value);
    p = p && equal_str(pin);
    p = p && equal_str(challenge);
    p = p && scalar_equal(time);
    p = p && equal_str(counter);
    p = p && scalar_equal(format);
    p = p && equal_str(token_id);
    p = p && equal_str(alg_id);
    p = p && equal_str(vendor);
    return p;
}

#ifdef ENABLE_LDAP
static int
equal_key_data(krb5_key_data *ref, krb5_key_data *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && scalar_equal(key_data_type[0]);
    p = p && scalar_equal(key_data_type[1]);
    p = p && len_equal(key_data_length[0],key_data_contents[0],
                   ktest_equal_array_of_octet);
    p = p && len_equal(key_data_length[1],key_data_contents[1],
                   ktest_equal_array_of_octet);
    return p;
}

static int
equal_key_data_array(int n, krb5_key_data *ref, krb5_key_data *val)
{
    int i, p = TRUE;
    for (i = 0; i < n; i++) {
        p = p && equal_key_data(ref+i, val+i);
    }
    return p;
}

int
ktest_equal_ldap_sequence_of_keys(ldap_seqof_key_data *ref,
                                  ldap_seqof_key_data *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && scalar_equal(mkvno);
    p = p && scalar_equal(kvno);
    p = p && len_equal(n_key_data,key_data,equal_key_data_array);
    return p;
}
#endif

/**** arrays ****************************************************************/

int
ktest_equal_array_of_data(int length, krb5_data *ref, krb5_data *var)
{
    int i,p = TRUE;

    if (length == 0 || ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    for (i=0; i<(length); i++) {
        p = p && ktest_equal_data(&(ref[i]),&(var[i]));
    }
    return p;
}

int
ktest_equal_array_of_octet(unsigned int length, krb5_octet *ref,
                           krb5_octet *var)
{
    unsigned int i, p = TRUE;

    if (length == 0 || ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    for (i=0; i<length; i++)
        p = p && (ref[i] == var[i]);
    return p;
}

int
ktest_equal_array_of_char(unsigned int length, char *ref, char *var)
{
    unsigned int i, p = TRUE;

    if (length == 0 || ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    for (i=0; i<length; i++)
        p = p && (ref[i] == var[i]);
    return p;
}

int
ktest_equal_array_of_enctype(int length, krb5_enctype *ref, krb5_enctype *var)
{
    int i, p = TRUE;

    if (length == 0 || ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    for (i=0; i<length; i++)
        p = p && (ref[i] == var[i]);
    return p;
}

#define array_compare(comparator)                       \
    int i,p = TRUE;                                       \
    if (ref == var) return TRUE;                          \
    if (!ref || !ref[0])                                \
        return (!var || !var[0]);                       \
    if (!var || !var[0]) return FALSE;                  \
    for (i=0; ref[i] != NULL && var[i] != NULL; i++)    \
        p = p && comparator(ref[i],var[i]);             \
    if (ref[i] == NULL && var[i] == NULL) return p;     \
    else return FALSE

int
ktest_equal_authorization_data(krb5_authdata **ref, krb5_authdata **var)
{
    array_compare(ktest_equal_authdata);
}

int
ktest_equal_addresses(krb5_address **ref, krb5_address **var)
{
    array_compare(ktest_equal_address);
}

int
ktest_equal_last_req(krb5_last_req_entry **ref, krb5_last_req_entry **var)
{
    array_compare(ktest_equal_last_req_entry);
}

int
ktest_equal_sequence_of_ticket(krb5_ticket **ref, krb5_ticket **var)
{
    array_compare(ktest_equal_ticket);
}

int
ktest_equal_sequence_of_pa_data(krb5_pa_data **ref, krb5_pa_data **var)
{
    array_compare(ktest_equal_pa_data);
}

int
ktest_equal_sequence_of_cred_info(krb5_cred_info **ref, krb5_cred_info **var)
{
    array_compare(ktest_equal_cred_info);
}

int
ktest_equal_sequence_of_principal(krb5_principal *ref, krb5_principal *var)
{
    array_compare(ktest_equal_principal_data);
}

int
ktest_equal_etype_info(krb5_etype_info_entry **ref, krb5_etype_info_entry **var)
{
    array_compare(ktest_equal_krb5_etype_info_entry);
}

int
ktest_equal_sequence_of_checksum(krb5_checksum **ref, krb5_checksum **var)
{
    array_compare(ktest_equal_checksum);
}

int
ktest_equal_sequence_of_algorithm_identifier(krb5_algorithm_identifier **ref,
                                             krb5_algorithm_identifier **var)
{
    array_compare(ktest_equal_algorithm_identifier);
}

int
ktest_equal_sequence_of_otp_tokeninfo(krb5_otp_tokeninfo **ref,
                                      krb5_otp_tokeninfo **var)
{
    array_compare(ktest_equal_otp_tokeninfo);
}

#ifndef DISABLE_PKINIT

static int
ktest_equal_pk_authenticator(krb5_pk_authenticator *ref,
                             krb5_pk_authenticator *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && scalar_equal(cusec);
    p = p && scalar_equal(ctime);
    p = p && scalar_equal(nonce);
    p = p && struct_equal(paChecksum, ktest_equal_checksum);
    return p;
}

static int
ktest_equal_pk_authenticator_draft9(krb5_pk_authenticator_draft9 *ref,
                                    krb5_pk_authenticator_draft9 *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && ptr_equal(kdcName, ktest_equal_principal_data);
    p = p && scalar_equal(cusec);
    p = p && scalar_equal(ctime);
    p = p && scalar_equal(nonce);
    return p;
}

static int
ktest_equal_subject_pk_info(krb5_subject_pk_info *ref,
                            krb5_subject_pk_info *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && struct_equal(algorithm, ktest_equal_algorithm_identifier);
    p = p && equal_str(subjectPublicKey);
    return p;
}

static int
ktest_equal_external_principal_identifier(
    krb5_external_principal_identifier *ref,
    krb5_external_principal_identifier *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && equal_str(subjectName);
    p = p && equal_str(issuerAndSerialNumber);
    p = p && equal_str(subjectKeyIdentifier);
    return p;
}

static int
ktest_equal_sequence_of_external_principal_identifier(
    krb5_external_principal_identifier **ref,
    krb5_external_principal_identifier **var)
{
    array_compare(ktest_equal_external_principal_identifier);
}

int
ktest_equal_pa_pk_as_req(krb5_pa_pk_as_req *ref, krb5_pa_pk_as_req *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && equal_str(signedAuthPack);
    p = p && ptr_equal(trustedCertifiers,
                       ktest_equal_sequence_of_external_principal_identifier);
    p = p && equal_str(kdcPkId);
    return p;
}

int
ktest_equal_pa_pk_as_req_draft9(krb5_pa_pk_as_req_draft9 *ref,
                                krb5_pa_pk_as_req_draft9 *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && equal_str(signedAuthPack);
    p = p && equal_str(kdcCert);
    return p;
}

static int
ktest_equal_dh_rep_info(krb5_dh_rep_info *ref, krb5_dh_rep_info *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && equal_str(dhSignedData);
    p = p && equal_str(serverDHNonce);
    p = p && ptr_equal(kdfID, ktest_equal_data);
    return p;
}

int
ktest_equal_pa_pk_as_rep(krb5_pa_pk_as_rep *ref, krb5_pa_pk_as_rep *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    if (ref->choice != var->choice) return FALSE;
    if (ref->choice == choice_pa_pk_as_rep_dhInfo)
        p = p && struct_equal(u.dh_Info, ktest_equal_dh_rep_info);
    else if (ref->choice == choice_pa_pk_as_rep_encKeyPack)
        p = p && equal_str(u.encKeyPack);
    return p;
}

static int
ktest_equal_sequence_of_data(krb5_data **ref, krb5_data **var)
{
    array_compare(ktest_equal_data);
}

int
ktest_equal_auth_pack(krb5_auth_pack *ref, krb5_auth_pack *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && struct_equal(pkAuthenticator, ktest_equal_pk_authenticator);
    p = p && ptr_equal(clientPublicValue, ktest_equal_subject_pk_info);
    p = p && ptr_equal(supportedCMSTypes,
                       ktest_equal_sequence_of_algorithm_identifier);
    p = p && equal_str(clientDHNonce);
    p = p && ptr_equal(supportedKDFs, ktest_equal_sequence_of_data);
    return p;
}

int
ktest_equal_auth_pack_draft9(krb5_auth_pack_draft9 *ref,
                             krb5_auth_pack_draft9 *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && struct_equal(pkAuthenticator,
                          ktest_equal_pk_authenticator_draft9);
    p = p && ptr_equal(clientPublicValue, ktest_equal_subject_pk_info);
    return p;
}

int
ktest_equal_kdc_dh_key_info(krb5_kdc_dh_key_info *ref,
                            krb5_kdc_dh_key_info *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && equal_str(subjectPublicKey);
    p = p && scalar_equal(nonce);
    p = p && scalar_equal(dhKeyExpiration);
    return p;
}

int
ktest_equal_reply_key_pack(krb5_reply_key_pack *ref, krb5_reply_key_pack *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && struct_equal(replyKey, ktest_equal_keyblock);
    p = p && struct_equal(asChecksum, ktest_equal_checksum);
    return p;
}

int
ktest_equal_reply_key_pack_draft9(krb5_reply_key_pack_draft9 *ref,
                                  krb5_reply_key_pack_draft9 *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && struct_equal(replyKey, ktest_equal_keyblock);
    p = p && scalar_equal(nonce);
    return p;
}

#endif /* not DISABLE_PKINIT */

int
ktest_equal_kkdcp_message(krb5_kkdcp_message *ref, krb5_kkdcp_message *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && data_eq(ref->kerb_message, var->kerb_message);
    p = p && data_eq(ref->target_domain, var->target_domain);
    p = p && (ref->dclocator_hint == var->dclocator_hint);
    return p;
}

static int
vmac_eq(krb5_verifier_mac *ref, krb5_verifier_mac *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && ptr_equal(princ, ktest_equal_principal_data);
    p = p && scalar_equal(kvno);
    p = p && scalar_equal(enctype);
    p = p && struct_equal(checksum, ktest_equal_checksum);
    return p;
}

static int
vmac_list_eq(krb5_verifier_mac **ref, krb5_verifier_mac **var)
{
    array_compare(vmac_eq);
}

int
ktest_equal_cammac(krb5_cammac *ref, krb5_cammac *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && ptr_equal(elements, ktest_equal_authorization_data);
    p = p && ptr_equal(kdc_verifier, vmac_eq);
    p = p && ptr_equal(svc_verifier, vmac_eq);
    p = p && ptr_equal(other_verifiers, vmac_list_eq);
    return p;
}

int
ktest_equal_secure_cookie(krb5_secure_cookie *ref, krb5_secure_cookie *var)
{
    int p = TRUE;
    if (ref == var) return TRUE;
    else if (ref == NULL || var == NULL) return FALSE;
    p = p && ktest_equal_sequence_of_pa_data(ref->data, var->data);
    p = p && ref->time == ref->time;
    return p;
}
