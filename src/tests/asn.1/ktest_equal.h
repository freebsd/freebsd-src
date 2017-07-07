/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/asn.1/ktest_equal.h */
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

#ifndef __KTEST_EQUAL_H__
#define __KTEST_EQUAL_H__

#include "k5-int.h"
#include "kdb.h"

/* int ktest_equal_structure(krb5_structure *ref, *var) */
/* effects  Returns true (non-zero) if ref and var are
             semantically equivalent (i.e. have the same values,
             but aren't necessarily the same object).
            Returns false (zero) if ref and var differ. */

#define generic(funcname,type)\
int funcname (type *ref, type *var)

#define len_array(funcname,type)\
int funcname (int length, type *ref, type *var)
#define len_unsigned_array(funcname,type)\
int funcname (unsigned int length, type *ref, type *var)

generic(ktest_equal_authenticator,krb5_authenticator);
generic(ktest_equal_principal_data,krb5_principal_data);
generic(ktest_equal_checksum,krb5_checksum);
generic(ktest_equal_keyblock,krb5_keyblock);
generic(ktest_equal_data,krb5_data);
generic(ktest_equal_authdata,krb5_authdata);
generic(ktest_equal_ticket,krb5_ticket);
generic(ktest_equal_enc_tkt_part,krb5_enc_tkt_part);
generic(ktest_equal_transited,krb5_transited);
generic(ktest_equal_ticket_times,krb5_ticket_times);
generic(ktest_equal_address,krb5_address);
generic(ktest_equal_enc_data,krb5_enc_data);

generic(ktest_equal_enc_kdc_rep_part,krb5_enc_kdc_rep_part);
generic(ktest_equal_priv,krb5_priv);
generic(ktest_equal_cred,krb5_cred);
generic(ktest_equal_error,krb5_error);
generic(ktest_equal_ap_req,krb5_ap_req);
generic(ktest_equal_ap_rep,krb5_ap_rep);
generic(ktest_equal_ap_rep_enc_part,krb5_ap_rep_enc_part);
generic(ktest_equal_safe,krb5_safe);

generic(ktest_equal_last_req_entry,krb5_last_req_entry);
generic(ktest_equal_pa_data,krb5_pa_data);
generic(ktest_equal_cred_info,krb5_cred_info);

generic(ktest_equal_enc_cred_part,krb5_cred_enc_part);
generic(ktest_equal_enc_priv_part,krb5_priv_enc_part);
generic(ktest_equal_as_rep,krb5_kdc_rep);
generic(ktest_equal_tgs_rep,krb5_kdc_rep);
generic(ktest_equal_as_req,krb5_kdc_req);
generic(ktest_equal_tgs_req,krb5_kdc_req);
generic(ktest_equal_kdc_req_body,krb5_kdc_req);
generic(ktest_equal_encryption_key,krb5_keyblock);

generic(ktest_equal_krb5_pa_enc_ts,krb5_pa_enc_ts);

generic(ktest_equal_sam_challenge_2,krb5_sam_challenge_2);
generic(ktest_equal_sam_challenge_2_body,krb5_sam_challenge_2_body);

int ktest_equal_last_req(krb5_last_req_entry **ref, krb5_last_req_entry **var);
int ktest_equal_sequence_of_ticket(krb5_ticket **ref, krb5_ticket **var);
int ktest_equal_sequence_of_pa_data(krb5_pa_data **ref, krb5_pa_data **var);
int ktest_equal_sequence_of_cred_info(krb5_cred_info **ref,
                                      krb5_cred_info **var);
int ktest_equal_sequence_of_principal(krb5_principal *ref,
                                      krb5_principal *var);
int ktest_equal_sequence_of_checksum(krb5_checksum **ref, krb5_checksum **var);
int
ktest_equal_sequence_of_algorithm_identifier(krb5_algorithm_identifier **ref,
                                             krb5_algorithm_identifier **var);
int ktest_equal_sequence_of_otp_tokeninfo(krb5_otp_tokeninfo **ref,
                                          krb5_otp_tokeninfo **var);

len_array(ktest_equal_array_of_enctype,krb5_enctype);
len_array(ktest_equal_array_of_data,krb5_data);
len_unsigned_array(ktest_equal_array_of_octet,krb5_octet);

int ktest_equal_authorization_data(krb5_authdata **ref, krb5_authdata **var);
int ktest_equal_addresses(krb5_address **ref, krb5_address **var);
int ktest_equal_array_of_char(const unsigned int length, char *ref, char *var);

int ktest_equal_etype_info(krb5_etype_info_entry **ref,
                           krb5_etype_info_entry **var);

int ktest_equal_krb5_etype_info_entry(krb5_etype_info_entry *ref,
                                      krb5_etype_info_entry *var);
int ktest_equal_pa_for_user(krb5_pa_for_user *ref, krb5_pa_for_user *var);
int ktest_equal_pa_s4u_x509_user(krb5_pa_s4u_x509_user *ref,
                                 krb5_pa_s4u_x509_user *var);
int ktest_equal_ad_kdcissued(krb5_ad_kdcissued *ref, krb5_ad_kdcissued *var);
int ktest_equal_ad_signedpath_data(krb5_ad_signedpath_data *ref,
                                   krb5_ad_signedpath_data *var);
int ktest_equal_ad_signedpath(krb5_ad_signedpath *ref,
                              krb5_ad_signedpath *var);
int ktest_equal_iakerb_header(krb5_iakerb_header *ref,
                              krb5_iakerb_header *var);
int ktest_equal_iakerb_finished(krb5_iakerb_finished *ref,
                                krb5_iakerb_finished *var);
int ktest_equal_fast_response(krb5_fast_response *ref,
                              krb5_fast_response *var);
int ktest_equal_otp_tokeninfo(krb5_otp_tokeninfo *ref,
                              krb5_otp_tokeninfo *var);
int ktest_equal_pa_otp_challenge(krb5_pa_otp_challenge *ref,
                                 krb5_pa_otp_challenge *var);
int ktest_equal_pa_otp_req(krb5_pa_otp_req *ref, krb5_pa_otp_req *var);

int ktest_equal_ldap_sequence_of_keys(ldap_seqof_key_data *ref,
                                      ldap_seqof_key_data *var);

#ifndef DISABLE_PKINIT
generic(ktest_equal_pa_pk_as_req, krb5_pa_pk_as_req);
generic(ktest_equal_pa_pk_as_req_draft9, krb5_pa_pk_as_req_draft9);
generic(ktest_equal_pa_pk_as_rep, krb5_pa_pk_as_rep);
generic(ktest_equal_auth_pack, krb5_auth_pack);
generic(ktest_equal_auth_pack_draft9, krb5_auth_pack_draft9);
generic(ktest_equal_kdc_dh_key_info, krb5_kdc_dh_key_info);
generic(ktest_equal_reply_key_pack, krb5_reply_key_pack);
generic(ktest_equal_reply_key_pack_draft9, krb5_reply_key_pack_draft9);
#endif /* not DISABLE_PKINIT */

int ktest_equal_kkdcp_message(krb5_kkdcp_message *ref,
                              krb5_kkdcp_message *var);
int ktest_equal_cammac(krb5_cammac *ref, krb5_cammac *var);

int ktest_equal_secure_cookie(krb5_secure_cookie *ref,
                              krb5_secure_cookie *var);

#endif
