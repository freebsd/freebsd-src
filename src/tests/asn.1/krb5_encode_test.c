/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/asn.1/krb5_encode_test.c */
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

#include "k5-int.h"
#include "com_err.h"
#include "utility.h"

#include "ktest.h"
#include "debug.h"

extern int current_appl_type;

krb5_context test_context;
int error_count = 0;
int do_trval = 0;
int first_trval = 1;
int trval2();

static void
encoder_print_results(krb5_data *code, char *typestring, char *description)
{
    char        *code_string = NULL;
    int r, rlen;

    if (do_trval) {
        if (first_trval)
            first_trval = 0;
        else
            printf("\n");
        printf("encode_krb5_%s%s:\n", typestring, description);
        r = trval2(stdout, code->data, code->length, 0, &rlen);
        printf("\n");
        if (rlen < 0 || (unsigned int) rlen != code->length) {
            printf("Error: length mismatch: was %d, parsed %d\n",
                   code->length, rlen);
            exit(1);
        }
        if (r != 0) {
            printf("Error: Return from trval2 is %d.\n", r);
            exit(1);
        }
        current_appl_type = -1; /* Reset type */
    } else {
        asn1_krb5_data_unparse(code,&(code_string));
        printf("encode_krb5_%s%s: %s\n", typestring, description,
               code_string);
        free(code_string);
    }
    ktest_destroy_data(&code);
}

static void PRS(argc, argv)
    int argc;
    char        **argv;
{
    extern char *optarg;
    int optchar;
    extern int print_types, print_krb5_types, print_id_and_len,
        print_constructed_length, print_skip_context,
        print_skip_tagnum, print_context_shortcut;

    while ((optchar = getopt(argc, argv, "tp:")) != -1) {
        switch(optchar) {
        case 't':
            do_trval = 1;
            break;
        case 'p':
            sample_principal_name = optarg;
            break;
        case '?':
        default:
            fprintf(stderr, "Usage: %s [-t] [-p principal]\n",
                    argv[0]);
            exit(1);
        }
    }
    print_types = 1;
    print_krb5_types = 1;
    print_id_and_len = 0;
    print_constructed_length = 0;
    print_skip_context = 1;
    print_skip_tagnum = 1;
    print_context_shortcut = 1;
}

int
main(argc, argv)
    int argc;
    char        **argv;
{
    krb5_data *code;
    krb5_error_code retval;

    PRS(argc, argv);

    retval = krb5_init_context(&test_context);
    if (retval) {
        com_err(argv[0], retval, "while initializing krb5");
        exit(1);
    }
    init_access(argv[0]);

#define encode_run(value,typestring,description,encoder)                \
    retval = encoder(&(value),&(code));                                 \
    if (retval) {                                                       \
        com_err("krb5_encode_test", retval,"while encoding %s", typestring); \
        exit(1);                                                        \
    }                                                                   \
    encoder_print_results(code, typestring, description);

    /****************************************************************/
    /* encode_krb5_authenticator */
    {
        krb5_authenticator authent;
        ktest_make_sample_authenticator(&authent);

        encode_run(authent, "authenticator", "", encode_krb5_authenticator);

        ktest_destroy_checksum(&(authent.checksum));
        ktest_destroy_keyblock(&(authent.subkey));
        authent.seq_number = 0;
        ktest_empty_authorization_data(authent.authorization_data);
        encode_run(authent, "authenticator", "(optionals empty)",
                   encode_krb5_authenticator);

        ktest_destroy_authorization_data(&(authent.authorization_data));
        encode_run(authent, "authenticator", "(optionals NULL)",
                   encode_krb5_authenticator);
        ktest_empty_authenticator(&authent);
    }

    /****************************************************************/
    /* encode_krb5_ticket */
    {
        krb5_ticket tkt;
        ktest_make_sample_ticket(&tkt);
        encode_run(tkt, "ticket", "", encode_krb5_ticket);
        ktest_empty_ticket(&tkt);
    }

    /****************************************************************/
    /* encode_krb5_encryption_key */
    {
        krb5_keyblock keyblk;
        ktest_make_sample_keyblock(&keyblk);
        current_appl_type = 1005;
        encode_run(keyblk, "keyblock", "", encode_krb5_encryption_key);
        ktest_empty_keyblock(&keyblk);
    }

    /****************************************************************/
    /* encode_krb5_enc_tkt_part */
    {
        krb5_ticket tkt;
        memset(&tkt, 0, sizeof(krb5_ticket));
        tkt.enc_part2 = ealloc(sizeof(krb5_enc_tkt_part));
        ktest_make_sample_enc_tkt_part(tkt.enc_part2);

        encode_run(*tkt.enc_part2, "enc_tkt_part", "",
                   encode_krb5_enc_tkt_part);

        tkt.enc_part2->times.starttime = 0;
        tkt.enc_part2->times.renew_till = 0;
        ktest_destroy_address(&(tkt.enc_part2->caddrs[1]));
        ktest_destroy_address(&(tkt.enc_part2->caddrs[0]));
        ktest_destroy_authdata(&(tkt.enc_part2->authorization_data[1]));
        ktest_destroy_authdata(&(tkt.enc_part2->authorization_data[0]));

        /* ISODE version fails on the empty caddrs field */
        ktest_destroy_addresses(&(tkt.enc_part2->caddrs));
        ktest_destroy_authorization_data(&(tkt.enc_part2->authorization_data));

        encode_run(*tkt.enc_part2, "enc_tkt_part", "(optionals NULL)",
                   encode_krb5_enc_tkt_part);
        ktest_empty_ticket(&tkt);
    }

    /****************************************************************/
    /* encode_krb5_enc_kdc_rep_part */
    {
        krb5_kdc_rep kdcr;

        memset(&kdcr, 0, sizeof(kdcr));

        kdcr.enc_part2 = ealloc(sizeof(krb5_enc_kdc_rep_part));
        ktest_make_sample_enc_kdc_rep_part(kdcr.enc_part2);

        encode_run(*kdcr.enc_part2, "enc_kdc_rep_part", "",
                   encode_krb5_enc_kdc_rep_part);

        kdcr.enc_part2->key_exp = 0;
        kdcr.enc_part2->times.starttime = 0;
        kdcr.enc_part2->flags &= ~TKT_FLG_RENEWABLE;
        ktest_destroy_addresses(&(kdcr.enc_part2->caddrs));

        encode_run(*kdcr.enc_part2, "enc_kdc_rep_part", "(optionals NULL)",
                   encode_krb5_enc_kdc_rep_part);

        ktest_empty_kdc_rep(&kdcr);
    }

    /****************************************************************/
    /* encode_krb5_as_rep */
    {
        krb5_kdc_rep kdcr;
        ktest_make_sample_kdc_rep(&kdcr);

/*    kdcr.msg_type = KRB5_TGS_REP;
      test(encode_krb5_as_rep(&kdcr,&code) == KRB5_BADMSGTYPE,
      "encode_krb5_as_rep type check\n");
      ktest_destroy_data(&code);*/

        kdcr.msg_type = KRB5_AS_REP;
        encode_run(kdcr, "as_rep", "", encode_krb5_as_rep);

        ktest_destroy_pa_data_array(&(kdcr.padata));
        encode_run(kdcr, "as_rep", "(optionals NULL)", encode_krb5_as_rep);

        ktest_empty_kdc_rep(&kdcr);

    }

    /****************************************************************/
    /* encode_krb5_tgs_rep */
    {
        krb5_kdc_rep kdcr;
        ktest_make_sample_kdc_rep(&kdcr);

/*    kdcr.msg_type = KRB5_AS_REP;
      test(encode_krb5_tgs_rep(&kdcr,&code) == KRB5_BADMSGTYPE,
      "encode_krb5_tgs_rep type check\n");*/

        kdcr.msg_type = KRB5_TGS_REP;
        encode_run(kdcr, "tgs_rep", "", encode_krb5_tgs_rep);

        ktest_destroy_pa_data_array(&(kdcr.padata));
        encode_run(kdcr, "tgs_rep", "(optionals NULL)", encode_krb5_tgs_rep);

        ktest_empty_kdc_rep(&kdcr);

    }

    /****************************************************************/
    /* encode_krb5_ap_req */
    {
        krb5_ap_req apreq;
        ktest_make_sample_ap_req(&apreq);
        encode_run(apreq, "ap_req", "", encode_krb5_ap_req);
        ktest_empty_ap_req(&apreq);
    }

    /****************************************************************/
    /* encode_krb5_ap_rep */
    {
        krb5_ap_rep aprep;
        ktest_make_sample_ap_rep(&aprep);
        encode_run(aprep, "ap_rep", "", encode_krb5_ap_rep);
        ktest_empty_ap_rep(&aprep);
    }

    /****************************************************************/
    /* encode_krb5_ap_rep_enc_part */
    {
        krb5_ap_rep_enc_part apenc;
        ktest_make_sample_ap_rep_enc_part(&apenc);
        encode_run(apenc, "ap_rep_enc_part", "", encode_krb5_ap_rep_enc_part);

        ktest_destroy_keyblock(&(apenc.subkey));
        apenc.seq_number = 0;
        encode_run(apenc, "ap_rep_enc_part", "(optionals NULL)",
                   encode_krb5_ap_rep_enc_part);
        ktest_empty_ap_rep_enc_part(&apenc);
    }

    /****************************************************************/
    /* encode_krb5_as_req */
    {
        krb5_kdc_req asreq;
        ktest_make_sample_kdc_req(&asreq);
        asreq.msg_type = KRB5_AS_REQ;
        asreq.kdc_options &= ~KDC_OPT_ENC_TKT_IN_SKEY;
        encode_run(asreq, "as_req", "", encode_krb5_as_req);

        ktest_destroy_pa_data_array(&(asreq.padata));
        ktest_destroy_principal(&(asreq.client));
#ifndef ISODE_SUCKS
        ktest_destroy_principal(&(asreq.server));
#endif
        asreq.kdc_options |= KDC_OPT_ENC_TKT_IN_SKEY;
        asreq.from = 0;
        asreq.rtime = 0;
        ktest_destroy_addresses(&(asreq.addresses));
        ktest_destroy_enc_data(&(asreq.authorization_data));
        encode_run(asreq, "as_req", "(optionals NULL except second_ticket)",
                   encode_krb5_as_req);
        ktest_destroy_sequence_of_ticket(&(asreq.second_ticket));
#ifndef ISODE_SUCKS
        ktest_make_sample_principal(&(asreq.server));
#endif
        asreq.kdc_options &= ~KDC_OPT_ENC_TKT_IN_SKEY;
        encode_run(asreq, "as_req", "(optionals NULL except server)",
                   encode_krb5_as_req);
        ktest_empty_kdc_req(&asreq);
    }

    /****************************************************************/
    /* encode_krb5_tgs_req */
    {
        krb5_kdc_req tgsreq;
        ktest_make_sample_kdc_req(&tgsreq);
        tgsreq.msg_type = KRB5_TGS_REQ;
        tgsreq.kdc_options &= ~KDC_OPT_ENC_TKT_IN_SKEY;
        encode_run(tgsreq, "tgs_req", "", encode_krb5_tgs_req);

        ktest_destroy_pa_data_array(&(tgsreq.padata));
        ktest_destroy_principal(&(tgsreq.client));
#ifndef ISODE_SUCKS
        ktest_destroy_principal(&(tgsreq.server));
#endif
        tgsreq.kdc_options |= KDC_OPT_ENC_TKT_IN_SKEY;
        tgsreq.from = 0;
        tgsreq.rtime = 0;
        ktest_destroy_addresses(&(tgsreq.addresses));
        ktest_destroy_enc_data(&(tgsreq.authorization_data));
        encode_run(tgsreq, "tgs_req", "(optionals NULL except second_ticket)",
                   encode_krb5_tgs_req);

        ktest_destroy_sequence_of_ticket(&(tgsreq.second_ticket));
#ifndef ISODE_SUCKS
        ktest_make_sample_principal(&(tgsreq.server));
#endif
        tgsreq.kdc_options &= ~KDC_OPT_ENC_TKT_IN_SKEY;
        encode_run(tgsreq, "tgs_req", "(optionals NULL except server)",
                   encode_krb5_tgs_req);

        ktest_empty_kdc_req(&tgsreq);
    }

    /****************************************************************/
    /* encode_krb5_kdc_req_body */
    {
        krb5_kdc_req kdcrb;
        memset(&kdcrb, 0, sizeof(kdcrb));
        ktest_make_sample_kdc_req_body(&kdcrb);
        kdcrb.kdc_options &= ~KDC_OPT_ENC_TKT_IN_SKEY;
        current_appl_type = 1007;       /* Force interpretation as kdc-req-body */
        encode_run(kdcrb, "kdc_req_body", "", encode_krb5_kdc_req_body);

        ktest_destroy_principal(&(kdcrb.client));
#ifndef ISODE_SUCKS
        ktest_destroy_principal(&(kdcrb.server));
#endif
        kdcrb.kdc_options |= KDC_OPT_ENC_TKT_IN_SKEY;
        kdcrb.from = 0;
        kdcrb.rtime = 0;
        ktest_destroy_addresses(&(kdcrb.addresses));
        ktest_destroy_enc_data(&(kdcrb.authorization_data));
        current_appl_type = 1007;       /* Force interpretation as kdc-req-body */
        encode_run(kdcrb, "kdc_req_body",
                   "(optionals NULL except second_ticket)",
                   encode_krb5_kdc_req_body);

        ktest_destroy_sequence_of_ticket(&(kdcrb.second_ticket));
#ifndef ISODE_SUCKS
        ktest_make_sample_principal(&(kdcrb.server));
#endif
        kdcrb.kdc_options &= ~KDC_OPT_ENC_TKT_IN_SKEY;
        current_appl_type = 1007;       /* Force interpretation as kdc-req-body */
        encode_run(kdcrb, "kdc_req_body", "(optionals NULL except server)",
                   encode_krb5_kdc_req_body);

        ktest_empty_kdc_req(&kdcrb);
    }

    /****************************************************************/
    /* encode_krb5_safe */
    {
        krb5_safe s;
        ktest_make_sample_safe(&s);
        encode_run(s, "safe", "", encode_krb5_safe);

        s.timestamp = 0;
        /* s.usec should be opted out by the timestamp */
        s.seq_number = 0;
        ktest_destroy_address(&(s.r_address));
        encode_run(s, "safe", "(optionals NULL)", encode_krb5_safe);

        ktest_empty_safe(&s);
    }

    /****************************************************************/
    /* encode_krb5_priv */
    {
        krb5_priv p;
        ktest_make_sample_priv(&p);
        encode_run(p, "priv", "", encode_krb5_priv);
        ktest_empty_priv(&p);
    }

    /****************************************************************/
    /* encode_krb5_enc_priv_part */
    {
        krb5_priv_enc_part ep;
        ktest_make_sample_priv_enc_part(&ep);
        encode_run(ep, "enc_priv_part", "", encode_krb5_enc_priv_part);

        ep.timestamp = 0;
        /* ep.usec should be opted out along with timestamp */
        ep.seq_number = 0;
        ktest_destroy_address(&(ep.r_address));
        encode_run(ep, "enc_priv_part", "(optionals NULL)",
                   encode_krb5_enc_priv_part);

        ktest_empty_priv_enc_part(&ep);
    }

    /****************************************************************/
    /* encode_krb5_cred */
    {
        krb5_cred c;
        ktest_make_sample_cred(&c);
        encode_run(c, "cred", "", encode_krb5_cred);
        ktest_empty_cred(&c);
    }

    /****************************************************************/
    /* encode_krb5_enc_cred_part */
    {
        krb5_cred_enc_part cep;
        ktest_make_sample_cred_enc_part(&cep);
        encode_run(cep, "enc_cred_part", "", encode_krb5_enc_cred_part);

        ktest_destroy_principal(&(cep.ticket_info[0]->client));
        ktest_destroy_principal(&(cep.ticket_info[0]->server));
        cep.ticket_info[0]->flags = 0;
        cep.ticket_info[0]->times.authtime = 0;
        cep.ticket_info[0]->times.starttime = 0;
        cep.ticket_info[0]->times.endtime = 0;
        cep.ticket_info[0]->times.renew_till = 0;
        ktest_destroy_addresses(&(cep.ticket_info[0]->caddrs));
        cep.nonce = 0;
        cep.timestamp = 0;
        ktest_destroy_address(&(cep.s_address));
        ktest_destroy_address(&(cep.r_address));
        encode_run(cep, "enc_cred_part", "(optionals NULL)",
                   encode_krb5_enc_cred_part);

        ktest_empty_cred_enc_part(&cep);
    }

    /****************************************************************/
    /* encode_krb5_error */
    {
        krb5_error kerr;
        ktest_make_sample_error(&kerr);
        encode_run(kerr, "error", "", encode_krb5_error);

        kerr.ctime = 0;
        ktest_destroy_principal(&(kerr.client));
        ktest_empty_data(&(kerr.text));
        ktest_empty_data(&(kerr.e_data));
        encode_run(kerr, "error", "(optionals NULL)", encode_krb5_error);

        ktest_empty_error(&kerr);
    }

    /****************************************************************/
    /* encode_krb5_authdata */
    {
        krb5_authdata **ad;
        ktest_make_sample_authorization_data(&ad);

        retval = encode_krb5_authdata(ad,&(code));
        if (retval) {
            com_err("encoding authorization_data",retval,"");
            exit(1);
        }
        current_appl_type = 1004;       /* Force type to be authdata */
        encoder_print_results(code, "authorization_data", "");

        ktest_destroy_authorization_data(&ad);
    }

    /****************************************************************/
    /* encode_padata_sequence and encode_krb5_typed_data */
    {
        krb5_pa_data **pa;

        ktest_make_sample_pa_data_array(&pa);
        encode_run(*pa, "padata_sequence", "", encode_krb5_padata_sequence);
        encode_run(*pa, "typed_data", "", encode_krb5_typed_data);
        ktest_destroy_pa_data_array(&pa);

        ktest_make_sample_empty_pa_data_array(&pa);
        encode_run(*pa, "padata_sequence", "(empty)",
                   encode_krb5_padata_sequence);
        ktest_destroy_pa_data_array(&pa);
    }

    /****************************************************************/
    /* encode_etype_info */
    {
        krb5_etype_info_entry **info;

        ktest_make_sample_etype_info(&info);
        encode_run(*info, "etype_info", "", encode_krb5_etype_info);

        ktest_destroy_etype_info_entry(info[2]);      info[2] = 0;
        ktest_destroy_etype_info_entry(info[1]);      info[1] = 0;
        encode_run(*info, "etype_info", "(only 1)", encode_krb5_etype_info);

        ktest_destroy_etype_info_entry(info[0]);      info[0] = 0;
        encode_run(*info, "etype_info", "(no info)", encode_krb5_etype_info);

        ktest_destroy_etype_info(info);
    }

    /* encode_etype_info2 */
    {
        krb5_etype_info_entry **info;

        ktest_make_sample_etype_info2(&info);
        encode_run(*info, "etype_info2", "", encode_krb5_etype_info2);

        ktest_destroy_etype_info_entry(info[2]);      info[2] = 0;
        ktest_destroy_etype_info_entry(info[1]);      info[1] = 0;
        encode_run(*info, "etype_info2", "(only 1)", encode_krb5_etype_info2);

        /* etype_info2 sequences aren't allowed to be empty. */

        ktest_destroy_etype_info(info);
    }

    /****************************************************************/
    /* encode_pa_enc_ts */
    {
        krb5_pa_enc_ts pa_enc;
        ktest_make_sample_pa_enc_ts(&pa_enc);
        encode_run(pa_enc, "pa_enc_ts", "", encode_krb5_pa_enc_ts);
        pa_enc.pausec = 0;
        encode_run(pa_enc, "pa_enc_ts (no usec)", "", encode_krb5_pa_enc_ts);
    }

    /****************************************************************/
    /* encode_enc_data */
    {
        krb5_enc_data enc_data;
        ktest_make_sample_enc_data(&enc_data);
        current_appl_type = 1001;
        encode_run(enc_data, "enc_data", "", encode_krb5_enc_data);
        enc_data.kvno = 0xFF000000;
        current_appl_type = 1001;
        encode_run(enc_data, "enc_data", "(MSB-set kvno)",
                   encode_krb5_enc_data);
        enc_data.kvno = 0xFFFFFFFF;
        current_appl_type = 1001;
        encode_run(enc_data, "enc_data", "(kvno=-1)", encode_krb5_enc_data);
        ktest_destroy_enc_data(&enc_data);
    }
    /****************************************************************/
    /* encode_krb5_sam_challenge_2 */
    {
        krb5_sam_challenge_2 sam_ch2;
        ktest_make_sample_sam_challenge_2(&sam_ch2);
        encode_run(sam_ch2, "sam_challenge_2", "",
                   encode_krb5_sam_challenge_2);
        ktest_empty_sam_challenge_2(&sam_ch2);
    }
    /****************************************************************/
    /* encode_krb5_sam_challenge_2_body */
    {
        krb5_sam_challenge_2_body body;
        ktest_make_sample_sam_challenge_2_body(&body);
        encode_run(body, "sam_challenge_2_body", "",
                   encode_krb5_sam_challenge_2_body);
        ktest_empty_sam_challenge_2_body(&body);
    }
    /****************************************************************/
    /* encode_krb5_sam_response_2 */
    {
        krb5_sam_response_2 sam_ch2;
        ktest_make_sample_sam_response_2(&sam_ch2);
        encode_run(sam_ch2, "sam_response_2", "", encode_krb5_sam_response_2);
        ktest_empty_sam_response_2(&sam_ch2);
    }
    /****************************************************************/
    /* encode_krb5_sam_response_enc_2 */
    {
        krb5_enc_sam_response_enc_2 sam_ch2;
        ktest_make_sample_enc_sam_response_enc_2(&sam_ch2);
        encode_run(sam_ch2, "enc_sam_response_enc_2", "",
                   encode_krb5_enc_sam_response_enc_2);
        ktest_empty_enc_sam_response_enc_2(&sam_ch2);
    }
    /****************************************************************/
    /* encode_krb5_pa_for_user */
    {
        krb5_pa_for_user s4u;
        ktest_make_sample_pa_for_user(&s4u);
        encode_run(s4u, "pa_for_user", "", encode_krb5_pa_for_user);
        ktest_empty_pa_for_user(&s4u);
    }
    /****************************************************************/
    /* encode_krb5_pa_s4u_x509_user */
    {
        krb5_pa_s4u_x509_user s4u;
        ktest_make_sample_pa_s4u_x509_user(&s4u);
        encode_run(s4u, "pa_s4u_x509_user", "", encode_krb5_pa_s4u_x509_user);
        ktest_empty_pa_s4u_x509_user(&s4u);
    }
    /****************************************************************/
    /* encode_krb5_ad_kdcissued */
    {
        krb5_ad_kdcissued kdci;
        ktest_make_sample_ad_kdcissued(&kdci);
        encode_run(kdci, "ad_kdcissued", "", encode_krb5_ad_kdcissued);
        ktest_empty_ad_kdcissued(&kdci);
    }
    /****************************************************************/
    /* encode_krb5_ad_signedpath_data */
    {
        krb5_ad_signedpath_data spd;
        ktest_make_sample_ad_signedpath_data(&spd);
        encode_run(spd, "ad_signedpath_data", "",
                   encode_krb5_ad_signedpath_data);
        ktest_empty_ad_signedpath_data(&spd);
    }
    /****************************************************************/
    /* encode_krb5_ad_signedpath */
    {
        krb5_ad_signedpath sp;
        ktest_make_sample_ad_signedpath(&sp);
        encode_run(sp, "ad_signedpath", "", encode_krb5_ad_signedpath);
        ktest_empty_ad_signedpath(&sp);
    }
    /****************************************************************/
    /* encode_krb5_iakerb_header */
    {
        krb5_iakerb_header ih;
        ktest_make_sample_iakerb_header(&ih);
        encode_run(ih, "iakerb_header", "", encode_krb5_iakerb_header);
        ktest_empty_iakerb_header(&ih);
    }
    /****************************************************************/
    /* encode_krb5_iakerb_finished */
    {
        krb5_iakerb_finished ih;
        ktest_make_sample_iakerb_finished(&ih);
        encode_run(ih, "iakerb_finished", "", encode_krb5_iakerb_finished);
        ktest_empty_iakerb_finished(&ih);
    }
    /****************************************************************/
    /* encode_krb5_fast_response */
    {
        krb5_fast_response fr;
        ktest_make_sample_fast_response(&fr);
        encode_run(fr, "fast_response", "", encode_krb5_fast_response);
        ktest_empty_fast_response(&fr);
    }
    /****************************************************************/
    /* encode_krb5_pa_fx_fast_reply */
    {
        krb5_enc_data enc_data;
        ktest_make_sample_enc_data(&enc_data);
        encode_run(enc_data, "pa_fx_fast_reply", "",
                   encode_krb5_pa_fx_fast_reply);
        ktest_destroy_enc_data(&enc_data);
    }
    /****************************************************************/
    /* encode_krb5_otp_tokeninfo */
    {
        krb5_otp_tokeninfo ti;
        ktest_make_minimal_otp_tokeninfo(&ti);
        encode_run(ti, "otp_tokeninfo", "(optionals NULL)",
                   encode_krb5_otp_tokeninfo);
        ktest_empty_otp_tokeninfo(&ti);
        ktest_make_maximal_otp_tokeninfo(&ti);
        encode_run(ti, "otp_tokeninfo", "", encode_krb5_otp_tokeninfo);
        ktest_empty_otp_tokeninfo(&ti);
    }
    /****************************************************************/
    /* encode_krb5_pa_otp_challenge */
    {
        krb5_pa_otp_challenge ch;
        ktest_make_minimal_pa_otp_challenge(&ch);
        encode_run(ch, "pa_otp_challenge", "(optionals NULL)",
                   encode_krb5_pa_otp_challenge);
        ktest_empty_pa_otp_challenge(&ch);
        ktest_make_maximal_pa_otp_challenge(&ch);
        encode_run(ch, "pa_otp_challenge", "", encode_krb5_pa_otp_challenge);
        ktest_empty_pa_otp_challenge(&ch);
    }
    /****************************************************************/
    /* encode_krb5_pa_otp_req */
    {
        krb5_pa_otp_req req;
        ktest_make_minimal_pa_otp_req(&req);
        encode_run(req, "pa_otp_req", "(optionals NULL)",
                   encode_krb5_pa_otp_req);
        ktest_empty_pa_otp_req(&req);
        ktest_make_maximal_pa_otp_req(&req);
        encode_run(req, "pa_otp_req", "", encode_krb5_pa_otp_req);
        ktest_empty_pa_otp_req(&req);
    }
    /****************************************************************/
    /* encode_krb5_pa_otp_enc_request */
    {
        krb5_data d;
        ktest_make_sample_data(&d);
        encode_run(d, "pa_otp_enc_req", "", encode_krb5_pa_otp_enc_req);
        ktest_empty_data(&d);
    }
    /****************************************************************/
    /* encode_krb5_kkdcp_message */
    {
        krb5_kkdcp_message info;
        ktest_make_sample_kkdcp_message(&info);
        encode_run(info, "kkdcp_message", "", encode_krb5_kkdcp_message);
        ktest_empty_kkdcp_message(&info);
    }
    /* encode_krb5_cammac */
    {
        krb5_cammac req;
        ktest_make_minimal_cammac(&req);
        encode_run(req, "cammac", "(optionals NULL)", encode_krb5_cammac);
        ktest_empty_cammac(&req);
        ktest_make_maximal_cammac(&req);
        encode_run(req, "cammac", "", encode_krb5_cammac);
        ktest_empty_cammac(&req);
    }
    /****************************************************************/
    /* encode_krb5_secure_cookie */
    {
        krb5_secure_cookie cookie;
        ktest_make_sample_secure_cookie(&cookie);
        encode_run(cookie, "secure_cookie", "", encode_krb5_secure_cookie);
        ktest_empty_secure_cookie(&cookie);
    }
#ifndef DISABLE_PKINIT
    /****************************************************************/
    /* encode_krb5_pa_pk_as_req */
    {
        krb5_pa_pk_as_req req;
        ktest_make_sample_pa_pk_as_req(&req);
        encode_run(req, "pa_pk_as_req", "", acc.encode_krb5_pa_pk_as_req);
        ktest_empty_pa_pk_as_req(&req);
    }
    /****************************************************************/
    /* encode_krb5_pa_pk_as_req_draft9 */
    {
        krb5_pa_pk_as_req_draft9 req;
        ktest_make_sample_pa_pk_as_req_draft9(&req);
        encode_run(req, "pa_pk_as_req_draft9", "",
                   acc.encode_krb5_pa_pk_as_req_draft9);
        ktest_empty_pa_pk_as_req_draft9(&req);
    }
    /****************************************************************/
    /* encode_krb5_pa_pk_as_rep */
    {
        krb5_pa_pk_as_rep rep;
        ktest_make_sample_pa_pk_as_rep_dhInfo(&rep);
        encode_run(rep, "pa_pk_as_rep", "(dhInfo)",
                   acc.encode_krb5_pa_pk_as_rep);
        ktest_empty_pa_pk_as_rep(&rep);
        ktest_make_sample_pa_pk_as_rep_encKeyPack(&rep);
        encode_run(rep, "pa_pk_as_rep", "(encKeyPack)",
                   acc.encode_krb5_pa_pk_as_rep);
        ktest_empty_pa_pk_as_rep(&rep);
    }
    /****************************************************************/
    /* encode_krb5_pa_pk_as_rep_draft9 */
    {
        krb5_pa_pk_as_rep_draft9 rep;
        ktest_make_sample_pa_pk_as_rep_draft9_dhSignedData(&rep);
        encode_run(rep, "pa_pk_as_rep_draft9", "(dhSignedData)",
                   acc.encode_krb5_pa_pk_as_rep_draft9);
        ktest_empty_pa_pk_as_rep_draft9(&rep);
        ktest_make_sample_pa_pk_as_rep_draft9_encKeyPack(&rep);
        encode_run(rep, "pa_pk_as_rep_draft9", "(encKeyPack)",
                   acc.encode_krb5_pa_pk_as_rep_draft9);
        ktest_empty_pa_pk_as_rep_draft9(&rep);
    }
    /****************************************************************/
    /* encode_krb5_auth_pack */
    {
        krb5_auth_pack pack;
        ktest_make_sample_auth_pack(&pack);
        encode_run(pack, "auth_pack", "", acc.encode_krb5_auth_pack);
        ktest_empty_auth_pack(&pack);
    }
    /****************************************************************/
    /* encode_krb5_auth_pack_draft9_draft9 */
    {
        krb5_auth_pack_draft9 pack;
        ktest_make_sample_auth_pack_draft9(&pack);
        encode_run(pack, "auth_pack_draft9", "",
                   acc.encode_krb5_auth_pack_draft9);
        ktest_empty_auth_pack_draft9(&pack);
    }
    /****************************************************************/
    /* encode_krb5_kdc_dh_key_info */
    {
        krb5_kdc_dh_key_info ki;
        ktest_make_sample_kdc_dh_key_info(&ki);
        encode_run(ki, "kdc_dh_key_info", "", acc.encode_krb5_kdc_dh_key_info);
        ktest_empty_kdc_dh_key_info(&ki);
    }
    /****************************************************************/
    /* encode_krb5_reply_key_pack */
    {
        krb5_reply_key_pack pack;
        ktest_make_sample_reply_key_pack(&pack);
        encode_run(pack, "reply_key_pack", "", acc.encode_krb5_reply_key_pack);
        ktest_empty_reply_key_pack(&pack);
    }
    /****************************************************************/
    /* encode_krb5_reply_key_pack_draft9 */
    {
        krb5_reply_key_pack_draft9 pack;
        ktest_make_sample_reply_key_pack_draft9(&pack);
        encode_run(pack, "reply_key_pack_draft9", "",
                   acc.encode_krb5_reply_key_pack_draft9);
        ktest_empty_reply_key_pack_draft9(&pack);
    }
    /****************************************************************/
    /* encode_krb5_sp80056a_other_info */
    {
        krb5_sp80056a_other_info info;
        ktest_make_sample_sp80056a_other_info(&info);
        encode_run(info, "sp80056a_other_info", "",
                   encode_krb5_sp80056a_other_info);
        ktest_empty_sp80056a_other_info(&info);
    }
    /****************************************************************/
    /* encode_krb5_pkinit_supp_pub_info */
    {
        krb5_pkinit_supp_pub_info info;
        ktest_make_sample_pkinit_supp_pub_info(&info);
        encode_run(info, "pkinit_supp_pub_info", "",
                   encode_krb5_pkinit_supp_pub_info);
        ktest_empty_pkinit_supp_pub_info(&info);
    }
#endif /* not DISABLE_PKINIT */
#ifdef ENABLE_LDAP
    {
        ldap_seqof_key_data skd;

        ktest_make_sample_ldap_seqof_key_data(&skd);
        encode_run(skd, "ldap_seqof_key_data", "",
                   acc.asn1_ldap_encode_sequence_of_keys);
        ktest_empty_ldap_seqof_key_data(test_context, &skd);
    }
#endif

    krb5_free_context(test_context);
    exit(error_count);
    return(error_count);
}
