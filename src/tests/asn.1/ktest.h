/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/asn.1/ktest.h */
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

#ifndef __KTEST_H__
#define __KTEST_H__

#include "k5-int.h"
#include "kdb.h"

#define SAMPLE_USEC 123456
#define SAMPLE_TIME 771228197  /* Fri Jun 10  6:03:17 GMT 1994 */
#define SAMPLE_SEQ_NUMBER 17
#define SAMPLE_NONCE 42
#define SAMPLE_FLAGS 0xFEDCBA98
#define SAMPLE_ERROR 0x3C;

void ktest_make_sample_data(krb5_data *d);
void ktest_make_sample_authenticator(krb5_authenticator *a);
void ktest_make_sample_principal(krb5_principal *p);
void ktest_make_sample_checksum(krb5_checksum *cs);
void ktest_make_sample_keyblock(krb5_keyblock *kb);
void ktest_make_sample_ticket(krb5_ticket *tkt);
void ktest_make_sample_enc_data(krb5_enc_data *ed);
void ktest_make_sample_enc_tkt_part(krb5_enc_tkt_part *etp);
void ktest_make_sample_transited(krb5_transited *t);
void ktest_make_sample_ticket_times(krb5_ticket_times *tt);
void ktest_make_sample_addresses(krb5_address ***caddrs);
void ktest_make_sample_address(krb5_address *a);
void ktest_make_sample_authorization_data(krb5_authdata ***ad);
void ktest_make_sample_authdata(krb5_authdata *ad);
void ktest_make_sample_enc_kdc_rep_part(krb5_enc_kdc_rep_part *ekr);
void ktest_make_sample_kdc_req(krb5_kdc_req *kr);

void ktest_make_sample_last_req(krb5_last_req_entry ***lr);
void ktest_make_sample_last_req_entry(krb5_last_req_entry **lre);
void ktest_make_sample_kdc_rep(krb5_kdc_rep *kdcr);
void ktest_make_sample_pa_data_array(krb5_pa_data ***pad);
void ktest_make_sample_empty_pa_data_array(krb5_pa_data ***pad);
void ktest_make_sample_pa_data(krb5_pa_data *pad);
void ktest_make_sample_ap_req(krb5_ap_req *ar);
void ktest_make_sample_ap_rep(krb5_ap_rep *ar);
void ktest_make_sample_ap_rep_enc_part(krb5_ap_rep_enc_part *arep);
void ktest_make_sample_kdc_req_body(krb5_kdc_req *krb);
void ktest_make_sample_safe(krb5_safe *s);
void ktest_make_sample_priv(krb5_priv *p);
void ktest_make_sample_priv_enc_part(krb5_priv_enc_part *pep);
void ktest_make_sample_cred(krb5_cred *c);
void ktest_make_sample_cred_enc_part(krb5_cred_enc_part *cep);
void ktest_make_sample_sequence_of_ticket(krb5_ticket ***sot);
void ktest_make_sample_error(krb5_error *kerr);
void ktest_make_sequence_of_cred_info(krb5_cred_info ***soci);
void ktest_make_sample_cred_info(krb5_cred_info *ci);

void ktest_make_sample_etype_info(krb5_etype_info_entry ***p);
void ktest_make_sample_etype_info2(krb5_etype_info_entry ***p);
void ktest_make_sample_pa_enc_ts(krb5_pa_enc_ts *am);
void ktest_make_sample_sam_challenge_2(krb5_sam_challenge_2 *p);
void ktest_make_sample_sam_challenge_2_body(krb5_sam_challenge_2_body *p);
void ktest_make_sample_sam_response_2(krb5_sam_response_2 *p);
void ktest_make_sample_enc_sam_response_enc_2(krb5_enc_sam_response_enc_2 *p);
void ktest_make_sample_pa_for_user(krb5_pa_for_user *p);
void ktest_make_sample_pa_s4u_x509_user(krb5_pa_s4u_x509_user *p);
void ktest_make_sample_ad_kdcissued(krb5_ad_kdcissued *p);
void ktest_make_sample_ad_signedpath_data(krb5_ad_signedpath_data *p);
void ktest_make_sample_ad_signedpath(krb5_ad_signedpath *p);
void ktest_make_sample_iakerb_header(krb5_iakerb_header *p);
void ktest_make_sample_iakerb_finished(krb5_iakerb_finished *p);
void ktest_make_sample_fast_response(krb5_fast_response *p);
void ktest_make_sha256_alg(krb5_algorithm_identifier *p);
void ktest_make_sha1_alg(krb5_algorithm_identifier *p);
void ktest_make_minimal_otp_tokeninfo(krb5_otp_tokeninfo *p);
void ktest_make_maximal_otp_tokeninfo(krb5_otp_tokeninfo *p);
void ktest_make_minimal_pa_otp_challenge(krb5_pa_otp_challenge *p);
void ktest_make_maximal_pa_otp_challenge(krb5_pa_otp_challenge *p);
void ktest_make_minimal_pa_otp_req(krb5_pa_otp_req *p);
void ktest_make_maximal_pa_otp_req(krb5_pa_otp_req *p);

#ifndef DISABLE_PKINIT
void ktest_make_sample_pa_pk_as_req(krb5_pa_pk_as_req *p);
void ktest_make_sample_pa_pk_as_req_draft9(krb5_pa_pk_as_req_draft9 *p);
void ktest_make_sample_pa_pk_as_rep_dhInfo(krb5_pa_pk_as_rep *p);
void ktest_make_sample_pa_pk_as_rep_encKeyPack(krb5_pa_pk_as_rep *p);
void ktest_make_sample_pa_pk_as_rep_draft9_dhSignedData(
    krb5_pa_pk_as_rep_draft9 *p);
void ktest_make_sample_pa_pk_as_rep_draft9_encKeyPack(
    krb5_pa_pk_as_rep_draft9 *p);
void ktest_make_sample_auth_pack(krb5_auth_pack *p);
void ktest_make_sample_auth_pack_draft9(krb5_auth_pack_draft9 *p);
void ktest_make_sample_kdc_dh_key_info(krb5_kdc_dh_key_info *p);
void ktest_make_sample_reply_key_pack(krb5_reply_key_pack *p);
void ktest_make_sample_reply_key_pack_draft9(krb5_reply_key_pack_draft9 *p);
void ktest_make_sample_sp80056a_other_info(krb5_sp80056a_other_info *p);
void ktest_make_sample_pkinit_supp_pub_info(krb5_pkinit_supp_pub_info *p);
#endif

#ifdef ENABLE_LDAP
void ktest_make_sample_ldap_seqof_key_data(ldap_seqof_key_data *p);
#endif

void ktest_make_sample_kkdcp_message(krb5_kkdcp_message *p);
void ktest_make_minimal_cammac(krb5_cammac *p);
void ktest_make_maximal_cammac(krb5_cammac *p);
void ktest_make_sample_secure_cookie(krb5_secure_cookie *p);

/*----------------------------------------------------------------------*/

void ktest_empty_authorization_data(krb5_authdata **ad);
void ktest_destroy_authorization_data(krb5_authdata ***ad);
void ktest_destroy_authorization_data(krb5_authdata ***ad);
void ktest_empty_addresses(krb5_address **a);
void ktest_destroy_addresses(krb5_address ***a);
void ktest_destroy_address(krb5_address **a);
void ktest_empty_pa_data_array(krb5_pa_data **pad);
void ktest_destroy_pa_data_array(krb5_pa_data ***pad);
void ktest_destroy_pa_data(krb5_pa_data **pad);

void ktest_destroy_data(krb5_data **d);
void ktest_empty_data(krb5_data *d);
void ktest_destroy_principal(krb5_principal *p);
void ktest_destroy_checksum(krb5_checksum **cs);
void ktest_empty_keyblock(krb5_keyblock *kb);
void ktest_destroy_keyblock(krb5_keyblock **kb);
void ktest_destroy_authdata(krb5_authdata **ad);
void ktest_destroy_sequence_of_integer(long **soi);
void ktest_destroy_sequence_of_ticket(krb5_ticket ***sot);
void ktest_destroy_ticket(krb5_ticket **tkt);
void ktest_empty_ticket(krb5_ticket *tkt);
void ktest_destroy_enc_data(krb5_enc_data *ed);
void ktest_empty_error(krb5_error *kerr);
void ktest_destroy_etype_info_entry(krb5_etype_info_entry *i);
void ktest_destroy_etype_info(krb5_etype_info_entry **info);

void ktest_empty_kdc_req(krb5_kdc_req *kr);
void ktest_empty_kdc_rep(krb5_kdc_rep *kr);

void ktest_empty_authenticator(krb5_authenticator *a);
void ktest_empty_enc_tkt_part(krb5_enc_tkt_part *etp);
void ktest_destroy_enc_tkt_part(krb5_enc_tkt_part **etp);
void ktest_empty_enc_kdc_rep_part(krb5_enc_kdc_rep_part *ekr);
void ktest_destroy_transited(krb5_transited *t);
void ktest_empty_ap_rep(krb5_ap_rep *ar);
void ktest_empty_ap_req(krb5_ap_req *ar);
void ktest_empty_cred_enc_part(krb5_cred_enc_part *cep);
void ktest_destroy_cred_info(krb5_cred_info **ci);
void ktest_destroy_sequence_of_cred_info(krb5_cred_info ***soci);
void ktest_empty_safe(krb5_safe *s);
void ktest_empty_priv(krb5_priv *p);
void ktest_empty_priv_enc_part(krb5_priv_enc_part *pep);
void ktest_empty_cred(krb5_cred *c);
void ktest_destroy_last_req(krb5_last_req_entry ***lr);
void ktest_empty_ap_rep_enc_part(krb5_ap_rep_enc_part *arep);
void ktest_empty_sam_challenge_2(krb5_sam_challenge_2 *p);
void ktest_empty_sam_challenge_2_body(krb5_sam_challenge_2_body *p);
void ktest_empty_sam_response_2(krb5_sam_response_2 *p);
void ktest_empty_enc_sam_response_enc_2(krb5_enc_sam_response_enc_2 *p);
void ktest_empty_pa_for_user(krb5_pa_for_user *p);
void ktest_empty_pa_s4u_x509_user(krb5_pa_s4u_x509_user *p);
void ktest_empty_ad_kdcissued(krb5_ad_kdcissued *p);
void ktest_empty_ad_signedpath_data(krb5_ad_signedpath_data *p);
void ktest_empty_ad_signedpath(krb5_ad_signedpath *p);
void ktest_empty_iakerb_header(krb5_iakerb_header *p);
void ktest_empty_iakerb_finished(krb5_iakerb_finished *p);
void ktest_empty_fast_response(krb5_fast_response *p);
void ktest_empty_otp_tokeninfo(krb5_otp_tokeninfo *p);
void ktest_empty_pa_otp_challenge(krb5_pa_otp_challenge *p);
void ktest_empty_pa_otp_req(krb5_pa_otp_req *p);

#ifndef DISABLE_PKINIT
void ktest_empty_pa_pk_as_req(krb5_pa_pk_as_req *p);
void ktest_empty_pa_pk_as_req_draft9(krb5_pa_pk_as_req_draft9 *p);
void ktest_empty_pa_pk_as_rep(krb5_pa_pk_as_rep *p);
void ktest_empty_pa_pk_as_rep_draft9(krb5_pa_pk_as_rep_draft9 *p);
void ktest_empty_auth_pack(krb5_auth_pack *p);
void ktest_empty_auth_pack_draft9(krb5_auth_pack_draft9 *p);
void ktest_empty_kdc_dh_key_info(krb5_kdc_dh_key_info *p);
void ktest_empty_reply_key_pack(krb5_reply_key_pack *p);
void ktest_empty_reply_key_pack_draft9(krb5_reply_key_pack_draft9 *p);
void ktest_empty_sp80056a_other_info(krb5_sp80056a_other_info *p);
void ktest_empty_pkinit_supp_pub_info(krb5_pkinit_supp_pub_info *p);
#endif

#ifdef ENABLE_LDAP
void ktest_empty_ldap_seqof_key_data(krb5_context, ldap_seqof_key_data *p);
#endif

void ktest_empty_kkdcp_message(krb5_kkdcp_message *p);
void ktest_empty_cammac(krb5_cammac *p);
void ktest_empty_secure_cookie(krb5_secure_cookie *p);

extern krb5_context test_context;
extern char *sample_principal_name;

#endif
