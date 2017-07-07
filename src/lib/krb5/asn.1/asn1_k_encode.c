/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/asn.1/asn1_k_encode.c */
/*
 * Copyright 1994, 2008 by the Massachusetts Institute of Technology.
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

#include "asn1_encode.h"
#include <assert.h>

DEFINT_IMMEDIATE(krb5_version, KVNO, KRB5KDC_ERR_BAD_PVNO);

static int
int32_not_minus1(const void *p)
{
    return (*(krb5_int32 *)p != -1);
}

static void
init_int32_minus1(void *p)
{
    *(krb5_int32 *)p = -1;
}

DEFBOOLTYPE(boolean, krb5_boolean);
DEFINTTYPE(int32, krb5_int32);
DEFPTRTYPE(int32_ptr, int32);
DEFCOUNTEDSEQOFTYPE(cseqof_int32, krb5_int32, int32_ptr);
DEFOPTIONALZEROTYPE(opt_int32, int32);
DEFOPTIONALTYPE(opt_int32_minus1, int32_not_minus1, init_int32_minus1, int32);

DEFUINTTYPE(uint, unsigned int);
DEFUINTTYPE(octet, krb5_octet);
DEFUINTTYPE(ui_4, krb5_ui_4);
DEFOPTIONALZEROTYPE(opt_uint, uint);

static int
nonempty_data(const void *p)
{
    const krb5_data *val = p;
    return (val->data != NULL && val->length != 0);
}

DEFCOUNTEDDERTYPE(der, char *, unsigned int);
DEFCOUNTEDTYPE(der_data, krb5_data, data, length, der);
DEFOPTIONALTYPE(opt_der_data, nonempty_data, NULL, der_data);

DEFCOUNTEDSTRINGTYPE(octetstring, unsigned char *, unsigned int,
                     k5_asn1_encode_bytestring, k5_asn1_decode_bytestring,
                     ASN1_OCTETSTRING);
DEFCOUNTEDSTRINGTYPE(s_octetstring, char *, unsigned int,
                     k5_asn1_encode_bytestring, k5_asn1_decode_bytestring,
                     ASN1_OCTETSTRING);
DEFCOUNTEDTYPE(ostring_data, krb5_data, data, length, s_octetstring);
DEFPTRTYPE(ostring_data_ptr, ostring_data);
DEFOPTIONALTYPE(opt_ostring_data, nonempty_data, NULL, ostring_data);
DEFOPTIONALZEROTYPE(opt_ostring_data_ptr, ostring_data_ptr);

DEFCOUNTEDSTRINGTYPE(generalstring, char *, unsigned int,
                     k5_asn1_encode_bytestring, k5_asn1_decode_bytestring,
                     ASN1_GENERALSTRING);
DEFCOUNTEDSTRINGTYPE(u_generalstring, unsigned char *, unsigned int,
                     k5_asn1_encode_bytestring, k5_asn1_decode_bytestring,
                     ASN1_GENERALSTRING);
DEFCOUNTEDTYPE(gstring_data, krb5_data, data, length, generalstring);
DEFOPTIONALTYPE(opt_gstring_data, nonempty_data, NULL, gstring_data);
DEFPTRTYPE(gstring_data_ptr, gstring_data);
DEFCOUNTEDSEQOFTYPE(cseqof_gstring_data, krb5_int32, gstring_data_ptr);

DEFCOUNTEDSTRINGTYPE(utf8string, char *, unsigned int,
                     k5_asn1_encode_bytestring, k5_asn1_decode_bytestring,
                     ASN1_UTF8STRING);
DEFCOUNTEDTYPE(utf8_data, krb5_data, data, length, utf8string);
DEFOPTIONALTYPE(opt_utf8_data, nonempty_data, NULL, utf8_data);
DEFPTRTYPE(utf8_data_ptr, utf8_data);
DEFNULLTERMSEQOFTYPE(seqof_utf8_data, utf8_data_ptr);

DEFCOUNTEDSTRINGTYPE(object_identifier, char *, unsigned int,
                     k5_asn1_encode_bytestring, k5_asn1_decode_bytestring,
                     ASN1_OBJECTIDENTIFIER);
DEFCOUNTEDTYPE(oid_data, krb5_data, data, length, object_identifier);
DEFPTRTYPE(oid_data_ptr, oid_data);

DEFOFFSETTYPE(realm_of_principal_data, krb5_principal_data, realm,
              gstring_data);
DEFPTRTYPE(realm_of_principal, realm_of_principal_data);
DEFOPTIONALZEROTYPE(opt_realm_of_principal, realm_of_principal);

DEFFIELD(princname_0, krb5_principal_data, type, 0, int32);
DEFCNFIELD(princname_1, krb5_principal_data, data, length, 1,
           cseqof_gstring_data);
static const struct atype_info *princname_fields[] = {
    &k5_atype_princname_0, &k5_atype_princname_1
};
DEFSEQTYPE(principal_data, krb5_principal_data, princname_fields);
DEFPTRTYPE(principal, principal_data);
DEFOPTIONALZEROTYPE(opt_principal, principal);

/*
 * Define the seqno type, which is an ASN.1 integer represented in a krb5_ui_4.
 * When decoding, negative 32-bit numbers are accepted for interoperability
 * with old implementations.
 */
static asn1_error_code
encode_seqno(asn1buf *buf, const void *p, taginfo *rettag, size_t *len_out)
{
    krb5_ui_4 val = *(krb5_ui_4 *)p;
    rettag->asn1class = UNIVERSAL;
    rettag->construction = PRIMITIVE;
    rettag->tagnum = ASN1_INTEGER;
    return k5_asn1_encode_uint(buf, val, len_out);
}
static asn1_error_code
decode_seqno(const taginfo *t, const unsigned char *asn1, size_t len, void *p)
{
    asn1_error_code ret;
    intmax_t val;
    ret = k5_asn1_decode_int(asn1, len, &val);
    if (ret)
        return ret;
    if (val < KRB5_INT32_MIN || val > 0xFFFFFFFF)
        return ASN1_OVERFLOW;
    /* Negative values will cast correctly to krb5_ui_4. */
    *(krb5_ui_4 *)p = val;
    return 0;
}
static int
check_seqno(const taginfo *t)
{
    return (t->asn1class == UNIVERSAL && t->construction == PRIMITIVE &&
            t->tagnum == ASN1_INTEGER);
}
DEFFNTYPE(seqno, krb5_ui_4, encode_seqno, decode_seqno, check_seqno, NULL);
DEFOPTIONALZEROTYPE(opt_seqno, seqno);

/* Define the kerberos_time type, which is an ASN.1 generaltime represented in
 * a krb5_timestamp. */
static asn1_error_code
encode_kerberos_time(asn1buf *buf, const void *p, taginfo *rettag,
                     size_t *len_out)
{
    /* Range checking for time_t vs krb5_timestamp?  */
    time_t val = *(krb5_timestamp *)p;
    rettag->asn1class = UNIVERSAL;
    rettag->construction = PRIMITIVE;
    rettag->tagnum = ASN1_GENERALTIME;
    return k5_asn1_encode_generaltime(buf, val, len_out);
}
static asn1_error_code
decode_kerberos_time(const taginfo *t, const unsigned char *asn1, size_t len,
                     void *p)
{
    asn1_error_code ret;
    time_t val;
    ret = k5_asn1_decode_generaltime(asn1, len, &val);
    if (ret)
        return ret;
    *(krb5_timestamp *)p = val;
    return 0;
}
static int
check_kerberos_time(const taginfo *t)
{
    return (t->asn1class == UNIVERSAL && t->construction == PRIMITIVE &&
            t->tagnum == ASN1_GENERALTIME);
}
DEFFNTYPE(kerberos_time, krb5_timestamp, encode_kerberos_time,
          decode_kerberos_time, check_kerberos_time, NULL);
DEFOPTIONALZEROTYPE(opt_kerberos_time, kerberos_time);

DEFFIELD(address_0, krb5_address, addrtype, 0, int32);
DEFCNFIELD(address_1, krb5_address, contents, length, 1, octetstring);
const static struct atype_info *address_fields[] = {
    &k5_atype_address_0, &k5_atype_address_1
};
DEFSEQTYPE(address, krb5_address, address_fields);
DEFPTRTYPE(address_ptr, address);
DEFOPTIONALZEROTYPE(opt_address_ptr, address_ptr);

DEFNULLTERMSEQOFTYPE(seqof_host_addresses, address_ptr);
DEFPTRTYPE(ptr_seqof_host_addresses, seqof_host_addresses);
DEFOPTIONALEMPTYTYPE(opt_ptr_seqof_host_addresses, ptr_seqof_host_addresses);

/*
 * krb5_kvno is defined as unsigned int, but historically (MIT krb5 through 1.6
 * in the encoder, and through 1.10 in the decoder) we treat it as signed, in
 * violation of RFC 4120.  kvno values large enough to be problematic are only
 * likely to be seen with Windows read-only domain controllers, which overload
 * the high 16-bits of kvno values for krbtgt principals.  Since Windows
 * encodes kvnos as signed 32-bit values, for interoperability it's best if we
 * do the same.
 */
DEFINTTYPE(kvno, krb5_kvno);
DEFOPTIONALZEROTYPE(opt_kvno, kvno);

DEFFIELD(enc_data_0, krb5_enc_data, enctype, 0, int32);
DEFFIELD(enc_data_1, krb5_enc_data, kvno, 1, opt_kvno);
DEFFIELD(enc_data_2, krb5_enc_data, ciphertext, 2, ostring_data);
static const struct atype_info *encrypted_data_fields[] = {
    &k5_atype_enc_data_0, &k5_atype_enc_data_1, &k5_atype_enc_data_2
};
DEFSEQTYPE(encrypted_data, krb5_enc_data, encrypted_data_fields);
static int
nonempty_enc_data(const void *p)
{
    const krb5_enc_data *val = p;
    return (val->ciphertext.data != NULL);
}
DEFOPTIONALTYPE(opt_encrypted_data, nonempty_enc_data, NULL, encrypted_data);

/* Define the krb5_flags type, which is an ASN.1 bit string represented in a
 * 32-bit integer. */
static asn1_error_code
encode_krb5_flags(asn1buf *buf, const void *p, taginfo *rettag,
                  size_t *len_out)
{
    unsigned char cbuf[4], *cptr = cbuf;
    store_32_be((krb5_ui_4)*(const krb5_flags *)p, cbuf);
    rettag->asn1class = UNIVERSAL;
    rettag->construction = PRIMITIVE;
    rettag->tagnum = ASN1_BITSTRING;
    return k5_asn1_encode_bitstring(buf, &cptr, 4, len_out);
}
static asn1_error_code
decode_krb5_flags(const taginfo *t, const unsigned char *asn1, size_t len,
                  void *val)
{
    asn1_error_code ret;
    size_t i, blen;
    krb5_flags f = 0;
    unsigned char *bits;
    ret = k5_asn1_decode_bitstring(asn1, len, &bits, &blen);
    if (ret)
        return ret;
    /* Copy up to 32 bits into f, starting at the most significant byte. */
    for (i = 0; i < blen && i < 4; i++)
        f |= bits[i] << (8 * (3 - i));
    *(krb5_flags *)val = f;
    free(bits);
    return 0;
}
static int
check_krb5_flags(const taginfo *t)
{
    return (t->asn1class == UNIVERSAL && t->construction == PRIMITIVE &&
            t->tagnum == ASN1_BITSTRING);
}
DEFFNTYPE(krb5_flags, krb5_flags, encode_krb5_flags, decode_krb5_flags,
          check_krb5_flags, NULL);
DEFOPTIONALZEROTYPE(opt_krb5_flags, krb5_flags);

DEFFIELD(authdata_0, krb5_authdata, ad_type, 0, int32);
DEFCNFIELD(authdata_1, krb5_authdata, contents, length, 1, octetstring);
static const struct atype_info *authdata_elt_fields[] = {
    &k5_atype_authdata_0, &k5_atype_authdata_1
};
DEFSEQTYPE(authdata_elt, krb5_authdata, authdata_elt_fields);
DEFPTRTYPE(authdata_elt_ptr, authdata_elt);
DEFNONEMPTYNULLTERMSEQOFTYPE(auth_data, authdata_elt_ptr);
DEFPTRTYPE(auth_data_ptr, auth_data);
DEFOPTIONALEMPTYTYPE(opt_auth_data_ptr, auth_data_ptr);

/* authdata_types retrieves just the types of authdata elements in an array. */
DEFCTAGGEDTYPE(authdata_elt_type_0, 0, int32);
static const struct atype_info *authdata_elt_type_fields[] = {
    &k5_atype_authdata_elt_type_0
};
DEFSEQTYPE(authdata_elt_type, krb5_authdatatype, authdata_elt_type_fields);
DEFPTRTYPE(ptr_authdata_elt_type, authdata_elt_type);
DEFCOUNTEDSEQOFTYPE(cseqof_authdata_elt_type, unsigned int,
                    ptr_authdata_elt_type);
struct authdata_types {
    krb5_authdatatype *types;
    unsigned int ntypes;
};
DEFCOUNTEDTYPE(authdata_types, struct authdata_types, types, ntypes,
               cseqof_authdata_elt_type);

DEFFIELD(keyblock_0, krb5_keyblock, enctype, 0, int32);
DEFCNFIELD(keyblock_1, krb5_keyblock, contents, length, 1, octetstring);
static const struct atype_info *encryption_key_fields[] = {
    &k5_atype_keyblock_0, &k5_atype_keyblock_1
};
DEFSEQTYPE(encryption_key, krb5_keyblock, encryption_key_fields);
DEFPTRTYPE(ptr_encryption_key, encryption_key);
DEFOPTIONALZEROTYPE(opt_ptr_encryption_key, ptr_encryption_key);

DEFFIELD(checksum_0, krb5_checksum, checksum_type, 0, int32);
DEFCNFIELD(checksum_1, krb5_checksum, contents, length, 1, octetstring);
static const struct atype_info *checksum_fields[] = {
    &k5_atype_checksum_0, &k5_atype_checksum_1
};
DEFSEQTYPE(checksum, krb5_checksum, checksum_fields);
DEFPTRTYPE(checksum_ptr, checksum);
DEFNULLTERMSEQOFTYPE(seqof_checksum, checksum_ptr);
DEFPTRTYPE(ptr_seqof_checksum, seqof_checksum);
DEFOPTIONALZEROTYPE(opt_checksum_ptr, checksum_ptr);

/* Define the last_req_type type, which is a krb5_int32 with some massaging
 * on decode for backward compatibility. */
static asn1_error_code
encode_lr_type(asn1buf *buf, const void *p, taginfo *rettag, size_t *len_out)
{
    krb5_int32 val = *(krb5_int32 *)p;
    rettag->asn1class = UNIVERSAL;
    rettag->construction = PRIMITIVE;
    rettag->tagnum = ASN1_INTEGER;
    return k5_asn1_encode_int(buf, val, len_out);
}
static asn1_error_code
decode_lr_type(const taginfo *t, const unsigned char *asn1, size_t len,
               void *p)
{
    asn1_error_code ret;
    intmax_t val;
    ret = k5_asn1_decode_int(asn1, len, &val);
    if (ret)
        return ret;
    if (val > KRB5_INT32_MAX || val < KRB5_INT32_MIN)
        return ASN1_OVERFLOW;
#ifdef KRB5_GENEROUS_LR_TYPE
    /* If type is in the 128-255 range, treat it as a negative 8-bit value. */
    if (val >= 128 && val <= 255)
        val -= 256;
#endif
    *(krb5_int32 *)p = val;
    return 0;
}
static int
check_lr_type(const taginfo *t)
{
    return (t->asn1class == UNIVERSAL && t->construction == PRIMITIVE &&
            t->tagnum == ASN1_INTEGER);
}
DEFFNTYPE(last_req_type, krb5_int32, encode_lr_type, decode_lr_type,
          check_lr_type, NULL);

DEFFIELD(last_req_0, krb5_last_req_entry, lr_type, 0, last_req_type);
DEFFIELD(last_req_1, krb5_last_req_entry, value, 1, kerberos_time);
static const struct atype_info *lr_fields[] = {
    &k5_atype_last_req_0, &k5_atype_last_req_1
};
DEFSEQTYPE(last_req_ent, krb5_last_req_entry, lr_fields);

DEFPTRTYPE(last_req_ent_ptr, last_req_ent);
DEFNONEMPTYNULLTERMSEQOFTYPE(last_req, last_req_ent_ptr);
DEFPTRTYPE(last_req_ptr, last_req);

DEFCTAGGEDTYPE(ticket_0, 0, krb5_version);
DEFFIELD(ticket_1, krb5_ticket, server, 1, realm_of_principal);
DEFFIELD(ticket_2, krb5_ticket, server, 2, principal);
DEFFIELD(ticket_3, krb5_ticket, enc_part, 3, encrypted_data);
static const struct atype_info *ticket_fields[] = {
    &k5_atype_ticket_0, &k5_atype_ticket_1, &k5_atype_ticket_2,
    &k5_atype_ticket_3
};
DEFSEQTYPE(untagged_ticket, krb5_ticket, ticket_fields);
DEFAPPTAGGEDTYPE(ticket, 1, untagged_ticket);

/* First context tag is 1, not 0. */
DEFFIELD(pa_data_1, krb5_pa_data, pa_type, 1, int32);
DEFCNFIELD(pa_data_2, krb5_pa_data, contents, length, 2, octetstring);
static const struct atype_info *pa_data_fields[] = {
    &k5_atype_pa_data_1, &k5_atype_pa_data_2
};
DEFSEQTYPE(pa_data, krb5_pa_data, pa_data_fields);
DEFPTRTYPE(pa_data_ptr, pa_data);

DEFNULLTERMSEQOFTYPE(seqof_pa_data, pa_data_ptr);
DEFPTRTYPE(ptr_seqof_pa_data, seqof_pa_data);
DEFOPTIONALEMPTYTYPE(opt_ptr_seqof_pa_data, ptr_seqof_pa_data);

DEFPTRTYPE(ticket_ptr, ticket);
DEFNONEMPTYNULLTERMSEQOFTYPE(seqof_ticket,ticket_ptr);
DEFPTRTYPE(ptr_seqof_ticket, seqof_ticket);
DEFOPTIONALEMPTYTYPE(opt_ptr_seqof_ticket, ptr_seqof_ticket);

static int
is_enc_kdc_rep_start_set(const void *p)
{
    const krb5_enc_kdc_rep_part *val = p;
    return (val->times.starttime != 0);
}
static void
init_enc_kdc_rep_start(void *p)
{
    krb5_enc_kdc_rep_part *val = p;
    val->times.starttime = val->times.authtime;
}
static int
is_renewable_flag_set(const void *p)
{
    const krb5_enc_kdc_rep_part *val = p;
    return (val->flags & TKT_FLG_RENEWABLE);
}
DEFFIELD(enc_kdc_rep_0, krb5_enc_kdc_rep_part, session, 0, ptr_encryption_key);
DEFFIELD(enc_kdc_rep_1, krb5_enc_kdc_rep_part, last_req, 1, last_req_ptr);
DEFFIELD(enc_kdc_rep_2, krb5_enc_kdc_rep_part, nonce, 2, int32);
DEFFIELD(enc_kdc_rep_3, krb5_enc_kdc_rep_part, key_exp, 3, opt_kerberos_time);
DEFFIELD(enc_kdc_rep_4, krb5_enc_kdc_rep_part, flags, 4, krb5_flags);
DEFFIELD(enc_kdc_rep_5, krb5_enc_kdc_rep_part, times.authtime, 5,
         kerberos_time);
DEFFIELD(enc_kdc_rep_6_def, krb5_enc_kdc_rep_part, times.starttime, 6,
         kerberos_time);
DEFOPTIONALTYPE(enc_kdc_rep_6, is_enc_kdc_rep_start_set,
                init_enc_kdc_rep_start, enc_kdc_rep_6_def);
DEFFIELD(enc_kdc_rep_7, krb5_enc_kdc_rep_part, times.endtime, 7,
         kerberos_time);
DEFFIELD(enc_kdc_rep_8_def, krb5_enc_kdc_rep_part, times.renew_till, 8,
         kerberos_time);
DEFOPTIONALTYPE(enc_kdc_rep_8, is_renewable_flag_set, NULL, enc_kdc_rep_8_def);
DEFFIELD(enc_kdc_rep_9, krb5_enc_kdc_rep_part, server, 9, realm_of_principal);
DEFFIELD(enc_kdc_rep_10, krb5_enc_kdc_rep_part, server, 10, principal);
DEFFIELD(enc_kdc_rep_11, krb5_enc_kdc_rep_part, caddrs, 11,
         opt_ptr_seqof_host_addresses);
DEFFIELD(enc_kdc_rep_12, krb5_enc_kdc_rep_part, enc_padata, 12,
         opt_ptr_seqof_pa_data);
static const struct atype_info *enc_kdc_rep_part_fields[] = {
    &k5_atype_enc_kdc_rep_0, &k5_atype_enc_kdc_rep_1, &k5_atype_enc_kdc_rep_2,
    &k5_atype_enc_kdc_rep_3, &k5_atype_enc_kdc_rep_4, &k5_atype_enc_kdc_rep_5,
    &k5_atype_enc_kdc_rep_6, &k5_atype_enc_kdc_rep_7, &k5_atype_enc_kdc_rep_8,
    &k5_atype_enc_kdc_rep_9, &k5_atype_enc_kdc_rep_10,
    &k5_atype_enc_kdc_rep_11, &k5_atype_enc_kdc_rep_12
};
DEFSEQTYPE(enc_kdc_rep_part, krb5_enc_kdc_rep_part, enc_kdc_rep_part_fields);

/*
 * Yuck!  Eventually push this *up* above the encoder API and make the
 * rest of the library put the realm name in one consistent place.  At
 * the same time, might as well add the msg-type field and encode both
 * AS-REQ and TGS-REQ through the same descriptor.
 */
typedef struct kdc_req_hack {
    krb5_kdc_req v;
    krb5_data server_realm;
} kdc_req_hack;
DEFFIELD(req_body_0, kdc_req_hack, v.kdc_options, 0, krb5_flags);
DEFFIELD(req_body_1, kdc_req_hack, v.client, 1, opt_principal);
DEFFIELD(req_body_2, kdc_req_hack, server_realm, 2, gstring_data);
DEFFIELD(req_body_3, kdc_req_hack, v.server, 3, opt_principal);
DEFFIELD(req_body_4, kdc_req_hack, v.from, 4, opt_kerberos_time);
DEFFIELD(req_body_5, kdc_req_hack, v.till, 5, kerberos_time);
DEFFIELD(req_body_6, kdc_req_hack, v.rtime, 6, opt_kerberos_time);
DEFFIELD(req_body_7, kdc_req_hack, v.nonce, 7, int32);
DEFCNFIELD(req_body_8, kdc_req_hack, v.ktype, v.nktypes, 8, cseqof_int32);
DEFFIELD(req_body_9, kdc_req_hack, v.addresses, 9,
         opt_ptr_seqof_host_addresses);
DEFFIELD(req_body_10, kdc_req_hack, v.authorization_data, 10,
         opt_encrypted_data);
DEFFIELD(req_body_11, kdc_req_hack, v.second_ticket, 11, opt_ptr_seqof_ticket);
static const struct atype_info *kdc_req_hack_fields[] = {
    &k5_atype_req_body_0, &k5_atype_req_body_1, &k5_atype_req_body_2,
    &k5_atype_req_body_3, &k5_atype_req_body_4, &k5_atype_req_body_5,
    &k5_atype_req_body_6, &k5_atype_req_body_7, &k5_atype_req_body_8,
    &k5_atype_req_body_9, &k5_atype_req_body_10, &k5_atype_req_body_11
};
DEFSEQTYPE(kdc_req_body_hack, kdc_req_hack, kdc_req_hack_fields);
static asn1_error_code
encode_kdc_req_body(asn1buf *buf, const void *p, taginfo *tag_out,
                    size_t *len_out)
{
    const krb5_kdc_req *val = p;
    kdc_req_hack h;
    h.v = *val;
    if (val->kdc_options & KDC_OPT_ENC_TKT_IN_SKEY) {
        if (val->second_ticket != NULL && val->second_ticket[0] != NULL)
            h.server_realm = val->second_ticket[0]->server->realm;
        else
            return ASN1_MISSING_FIELD;
    } else if (val->server != NULL)
        h.server_realm = val->server->realm;
    else
        return ASN1_MISSING_FIELD;
    return k5_asn1_encode_atype(buf, &h, &k5_atype_kdc_req_body_hack, tag_out,
                                len_out);
}
static void
free_kdc_req_body(void *val)
{
    krb5_kdc_req *req = val;
    krb5_free_principal(NULL, req->client);
    krb5_free_principal(NULL, req->server);
    free(req->ktype);
    krb5_free_addresses(NULL, req->addresses);
    free(req->authorization_data.ciphertext.data);
    krb5_free_tickets(NULL, req->second_ticket);
}
static asn1_error_code
decode_kdc_req_body(const taginfo *t, const unsigned char *asn1, size_t len,
                    void *val)
{
    asn1_error_code ret;
    kdc_req_hack h;
    krb5_kdc_req *b = val;
    memset(&h, 0, sizeof(h));
    ret = k5_asn1_decode_atype(t, asn1, len, &k5_atype_kdc_req_body_hack, &h);
    if (ret)
        return ret;
    b->kdc_options = h.v.kdc_options;
    b->client = h.v.client;
    b->server = h.v.server;
    b->from = h.v.from;
    b->till = h.v.till;
    b->rtime = h.v.rtime;
    b->nonce = h.v.nonce;
    b->ktype = h.v.ktype;
    b->nktypes = h.v.nktypes;
    b->addresses = h.v.addresses;
    b->authorization_data = h.v.authorization_data;
    b->second_ticket = h.v.second_ticket;
    if (b->client != NULL && b->server != NULL) {
        ret = krb5int_copy_data_contents(NULL, &h.server_realm,
                                         &b->client->realm);
        if (ret) {
            free_kdc_req_body(b);
            free(h.server_realm.data);
            memset(&h, 0, sizeof(h));
            return ret;
        }
        b->server->realm = h.server_realm;
    } else if (b->client != NULL)
        b->client->realm = h.server_realm;
    else if (b->server != NULL)
        b->server->realm = h.server_realm;
    else
        free(h.server_realm.data);
    return 0;
}
static int
check_kdc_req_body(const taginfo *t)
{
    return (t->asn1class == UNIVERSAL && t->construction == CONSTRUCTED &&
            t->tagnum == ASN1_SEQUENCE);
}
DEFFNTYPE(kdc_req_body, krb5_kdc_req, encode_kdc_req_body, decode_kdc_req_body,
          check_kdc_req_body, free_kdc_req_body);
/* end ugly hack */

DEFFIELD(transited_0, krb5_transited, tr_type, 0, octet);
DEFFIELD(transited_1, krb5_transited, tr_contents, 1, ostring_data);
static const struct atype_info *transited_fields[] = {
    &k5_atype_transited_0, &k5_atype_transited_1
};
DEFSEQTYPE(transited, krb5_transited, transited_fields);

static int
is_safe_timestamp_set(const void *p)
{
    const krb5_safe *val = p;
    return (val->timestamp != 0);
}
DEFFIELD(safe_body_0, krb5_safe, user_data, 0, ostring_data);
DEFFIELD(safe_body_1, krb5_safe, timestamp, 1, opt_kerberos_time);
DEFFIELD(safe_body_2_def, krb5_safe, usec, 2, int32);
DEFOPTIONALTYPE(safe_body_2, is_safe_timestamp_set, NULL, safe_body_2_def);
DEFFIELD(safe_body_3, krb5_safe, seq_number, 3, opt_seqno);
DEFFIELD(safe_body_4, krb5_safe, s_address, 4, address_ptr);
DEFFIELD(safe_body_5, krb5_safe, r_address, 5, opt_address_ptr);
static const struct atype_info *safe_body_fields[] = {
    &k5_atype_safe_body_0, &k5_atype_safe_body_1, &k5_atype_safe_body_2,
    &k5_atype_safe_body_3, &k5_atype_safe_body_4, &k5_atype_safe_body_5
};
DEFSEQTYPE(safe_body, krb5_safe, safe_body_fields);

DEFFIELD(cred_info_0, krb5_cred_info, session, 0, ptr_encryption_key);
DEFFIELD(cred_info_1, krb5_cred_info, client, 1, opt_realm_of_principal);
DEFFIELD(cred_info_2, krb5_cred_info, client, 2, opt_principal);
DEFFIELD(cred_info_3, krb5_cred_info, flags, 3, opt_krb5_flags);
DEFFIELD(cred_info_4, krb5_cred_info, times.authtime, 4, opt_kerberos_time);
DEFFIELD(cred_info_5, krb5_cred_info, times.starttime, 5, opt_kerberos_time);
DEFFIELD(cred_info_6, krb5_cred_info, times.endtime, 6, opt_kerberos_time);
DEFFIELD(cred_info_7, krb5_cred_info, times.renew_till, 7, opt_kerberos_time);
DEFFIELD(cred_info_8, krb5_cred_info, server, 8, opt_realm_of_principal);
DEFFIELD(cred_info_9, krb5_cred_info, server, 9, opt_principal);
DEFFIELD(cred_info_10, krb5_cred_info, caddrs, 10,
         opt_ptr_seqof_host_addresses);
static const struct atype_info *krb_cred_info_fields[] = {
    &k5_atype_cred_info_0, &k5_atype_cred_info_1, &k5_atype_cred_info_2,
    &k5_atype_cred_info_3, &k5_atype_cred_info_4, &k5_atype_cred_info_5,
    &k5_atype_cred_info_6, &k5_atype_cred_info_7, &k5_atype_cred_info_8,
    &k5_atype_cred_info_9, &k5_atype_cred_info_10
};
DEFSEQTYPE(cred_info, krb5_cred_info, krb_cred_info_fields);
DEFPTRTYPE(cred_info_ptr, cred_info);
DEFNULLTERMSEQOFTYPE(seqof_cred_info, cred_info_ptr);

DEFPTRTYPE(ptrseqof_cred_info, seqof_cred_info);

static int
is_salt_present(const void *p)
{
    const krb5_etype_info_entry *val = p;
    return (val->length != KRB5_ETYPE_NO_SALT);
}
static void
init_no_salt(void *p)
{
    krb5_etype_info_entry *val = p;
    val->length = KRB5_ETYPE_NO_SALT;
}
DEFFIELD(etype_info_0, krb5_etype_info_entry, etype, 0, int32);
DEFCNFIELD(etype_info_1_def, krb5_etype_info_entry, salt, length, 1,
           octetstring);
DEFOPTIONALTYPE(etype_info_1, is_salt_present, init_no_salt, etype_info_1_def);
static const struct atype_info *etype_info_entry_fields[] = {
    &k5_atype_etype_info_0, &k5_atype_etype_info_1
};
DEFSEQTYPE(etype_info_entry, krb5_etype_info_entry, etype_info_entry_fields);

/* First field is the same as etype-info. */
DEFCNFIELD(etype_info2_1_def, krb5_etype_info_entry, salt, length, 1,
           u_generalstring);
DEFOPTIONALTYPE(etype_info2_1, is_salt_present, init_no_salt,
                etype_info2_1_def);
DEFFIELD(etype_info2_2, krb5_etype_info_entry, s2kparams, 2, opt_ostring_data);
static const struct atype_info *etype_info2_entry_fields[] = {
    &k5_atype_etype_info_0, &k5_atype_etype_info2_1, &k5_atype_etype_info2_2
};
DEFSEQTYPE(etype_info2_entry, krb5_etype_info_entry, etype_info2_entry_fields);

DEFPTRTYPE(etype_info_entry_ptr, etype_info_entry);
DEFNULLTERMSEQOFTYPE(etype_info, etype_info_entry_ptr);

DEFPTRTYPE(etype_info2_entry_ptr, etype_info2_entry);
DEFNULLTERMSEQOFTYPE(etype_info2, etype_info2_entry_ptr);

DEFFIELD(sch_0, krb5_sam_challenge_2, sam_challenge_2_body, 0, der_data);
DEFFIELD(sch_1, krb5_sam_challenge_2, sam_cksum, 1, ptr_seqof_checksum);
static const struct atype_info *sam_challenge_2_fields[] = {
    &k5_atype_sch_0, &k5_atype_sch_1
};
DEFSEQTYPE(sam_challenge_2, krb5_sam_challenge_2, sam_challenge_2_fields);

DEFFIELD(schb_0, krb5_sam_challenge_2_body, sam_type, 0, int32);
DEFFIELD(schb_1, krb5_sam_challenge_2_body, sam_flags, 1, krb5_flags);
DEFFIELD(schb_2, krb5_sam_challenge_2_body, sam_type_name, 2,
         opt_ostring_data);
DEFFIELD(schb_3, krb5_sam_challenge_2_body, sam_track_id, 3, opt_ostring_data);
DEFFIELD(schb_4, krb5_sam_challenge_2_body, sam_challenge_label, 4,
         opt_ostring_data);
DEFFIELD(schb_5, krb5_sam_challenge_2_body, sam_challenge, 5,
         opt_ostring_data);
DEFFIELD(schb_6, krb5_sam_challenge_2_body, sam_response_prompt, 6,
         opt_ostring_data);
DEFFIELD(schb_7, krb5_sam_challenge_2_body, sam_pk_for_sad, 7,
         opt_ostring_data);
DEFFIELD(schb_8, krb5_sam_challenge_2_body, sam_nonce, 8, int32);
DEFFIELD(schb_9, krb5_sam_challenge_2_body, sam_etype, 9, int32);
static const struct atype_info *sam_challenge_2_body_fields[] = {
    &k5_atype_schb_0, &k5_atype_schb_1, &k5_atype_schb_2, &k5_atype_schb_3,
    &k5_atype_schb_4, &k5_atype_schb_5, &k5_atype_schb_6, &k5_atype_schb_7,
    &k5_atype_schb_8, &k5_atype_schb_9
};
DEFSEQTYPE(sam_challenge_2_body,krb5_sam_challenge_2_body,
           sam_challenge_2_body_fields);

DEFFIELD(esre_0, krb5_enc_sam_response_enc_2, sam_nonce, 0, int32);
DEFFIELD(esre_1, krb5_enc_sam_response_enc_2, sam_sad, 1, opt_ostring_data);
static const struct atype_info *enc_sam_response_enc_2_fields[] = {
    &k5_atype_esre_0, &k5_atype_esre_1
};
DEFSEQTYPE(enc_sam_response_enc_2, krb5_enc_sam_response_enc_2,
           enc_sam_response_enc_2_fields);

DEFFIELD(sam_resp_0, krb5_sam_response_2, sam_type, 0, int32);
DEFFIELD(sam_resp_1, krb5_sam_response_2, sam_flags, 1, krb5_flags);
DEFFIELD(sam_resp_2, krb5_sam_response_2, sam_track_id, 2, opt_ostring_data);
DEFFIELD(sam_resp_3, krb5_sam_response_2, sam_enc_nonce_or_sad, 3,
         encrypted_data);
DEFFIELD(sam_resp_4, krb5_sam_response_2, sam_nonce, 4, int32);
static const struct atype_info *sam_response_2_fields[] = {
    &k5_atype_sam_resp_0, &k5_atype_sam_resp_1, &k5_atype_sam_resp_2,
    &k5_atype_sam_resp_3, &k5_atype_sam_resp_4
};
DEFSEQTYPE(sam_response_2, krb5_sam_response_2, sam_response_2_fields);

DEFCTAGGEDTYPE(authenticator_0, 0, krb5_version);
DEFFIELD(authenticator_1, krb5_authenticator, client, 1, realm_of_principal);
DEFFIELD(authenticator_2, krb5_authenticator, client, 2, principal);
DEFFIELD(authenticator_3, krb5_authenticator, checksum, 3, opt_checksum_ptr);
DEFFIELD(authenticator_4, krb5_authenticator, cusec, 4, int32);
DEFFIELD(authenticator_5, krb5_authenticator, ctime, 5, kerberos_time);
DEFFIELD(authenticator_6, krb5_authenticator, subkey, 6,
         opt_ptr_encryption_key);
DEFFIELD(authenticator_7, krb5_authenticator, seq_number, 7, opt_seqno);
DEFFIELD(authenticator_8, krb5_authenticator, authorization_data, 8,
         opt_auth_data_ptr);
static const struct atype_info *authenticator_fields[] = {
    &k5_atype_authenticator_0, &k5_atype_authenticator_1,
    &k5_atype_authenticator_2, &k5_atype_authenticator_3,
    &k5_atype_authenticator_4, &k5_atype_authenticator_5,
    &k5_atype_authenticator_6, &k5_atype_authenticator_7,
    &k5_atype_authenticator_8
};
DEFSEQTYPE(untagged_authenticator, krb5_authenticator, authenticator_fields);
DEFAPPTAGGEDTYPE(authenticator, 2, untagged_authenticator);

DEFFIELD(enc_tkt_0, krb5_enc_tkt_part, flags, 0, krb5_flags);
DEFFIELD(enc_tkt_1, krb5_enc_tkt_part, session, 1, ptr_encryption_key);
DEFFIELD(enc_tkt_2, krb5_enc_tkt_part, client, 2, realm_of_principal);
DEFFIELD(enc_tkt_3, krb5_enc_tkt_part, client, 3, principal);
DEFFIELD(enc_tkt_4, krb5_enc_tkt_part, transited, 4, transited);
DEFFIELD(enc_tkt_5, krb5_enc_tkt_part, times.authtime, 5, kerberos_time);
DEFFIELD(enc_tkt_6, krb5_enc_tkt_part, times.starttime, 6, opt_kerberos_time);
DEFFIELD(enc_tkt_7, krb5_enc_tkt_part, times.endtime, 7, kerberos_time);
DEFFIELD(enc_tkt_8, krb5_enc_tkt_part, times.renew_till, 8, opt_kerberos_time);
DEFFIELD(enc_tkt_9, krb5_enc_tkt_part, caddrs, 9,
         opt_ptr_seqof_host_addresses);
DEFFIELD(enc_tkt_10, krb5_enc_tkt_part, authorization_data, 10,
         opt_auth_data_ptr);
static const struct atype_info *enc_tkt_part_fields[] = {
    &k5_atype_enc_tkt_0, &k5_atype_enc_tkt_1, &k5_atype_enc_tkt_2,
    &k5_atype_enc_tkt_3, &k5_atype_enc_tkt_4, &k5_atype_enc_tkt_5,
    &k5_atype_enc_tkt_6, &k5_atype_enc_tkt_7, &k5_atype_enc_tkt_8,
    &k5_atype_enc_tkt_9, &k5_atype_enc_tkt_10
};
DEFSEQTYPE(untagged_enc_tkt_part, krb5_enc_tkt_part, enc_tkt_part_fields);
DEFAPPTAGGEDTYPE(enc_tkt_part, 3, untagged_enc_tkt_part);

DEFAPPTAGGEDTYPE(enc_as_rep_part, 25, enc_kdc_rep_part);
DEFAPPTAGGEDTYPE(enc_tgs_rep_part, 26, enc_kdc_rep_part);

DEFCTAGGEDTYPE(kdc_rep_0, 0, krb5_version);
DEFFIELD(kdc_rep_1, krb5_kdc_rep, msg_type, 1, uint);
DEFFIELD(kdc_rep_2, krb5_kdc_rep, padata, 2, opt_ptr_seqof_pa_data);
DEFFIELD(kdc_rep_3, krb5_kdc_rep, client, 3, realm_of_principal);
DEFFIELD(kdc_rep_4, krb5_kdc_rep, client, 4, principal);
DEFFIELD(kdc_rep_5, krb5_kdc_rep, ticket, 5, ticket_ptr);
DEFFIELD(kdc_rep_6, krb5_kdc_rep, enc_part, 6, encrypted_data);
static const struct atype_info *kdc_rep_fields[] = {
    &k5_atype_kdc_rep_0, &k5_atype_kdc_rep_1, &k5_atype_kdc_rep_2,
    &k5_atype_kdc_rep_3, &k5_atype_kdc_rep_4, &k5_atype_kdc_rep_5,
    &k5_atype_kdc_rep_6
};
DEFSEQTYPE(kdc_rep, krb5_kdc_rep, kdc_rep_fields);
DEFAPPTAGGEDTYPE(as_rep, 11, kdc_rep);
DEFAPPTAGGEDTYPE(tgs_rep, 13, kdc_rep);

DEFINT_IMMEDIATE(ap_req_msg_type, ASN1_KRB_AP_REQ, 0);
DEFCTAGGEDTYPE(ap_req_0, 0, krb5_version);
DEFCTAGGEDTYPE(ap_req_1, 1, ap_req_msg_type);
DEFFIELD(ap_req_2, krb5_ap_req, ap_options, 2, krb5_flags);
DEFFIELD(ap_req_3, krb5_ap_req, ticket, 3, ticket_ptr);
DEFFIELD(ap_req_4, krb5_ap_req, authenticator, 4, encrypted_data);
static const struct atype_info *ap_req_fields[] = {
    &k5_atype_ap_req_0, &k5_atype_ap_req_1, &k5_atype_ap_req_2,
    &k5_atype_ap_req_3, &k5_atype_ap_req_4
};
DEFSEQTYPE(untagged_ap_req, krb5_ap_req, ap_req_fields);
DEFAPPTAGGEDTYPE(ap_req, 14, untagged_ap_req);

DEFINT_IMMEDIATE(ap_rep_msg_type, ASN1_KRB_AP_REP, 0);
DEFCTAGGEDTYPE(ap_rep_0, 0, krb5_version);
DEFCTAGGEDTYPE(ap_rep_1, 1, ap_rep_msg_type);
DEFFIELD(ap_rep_2, krb5_ap_rep, enc_part, 2, encrypted_data);
static const struct atype_info *ap_rep_fields[] = {
    &k5_atype_ap_rep_0, &k5_atype_ap_rep_1, &k5_atype_ap_rep_2
};
DEFSEQTYPE(untagged_ap_rep, krb5_ap_rep, ap_rep_fields);
DEFAPPTAGGEDTYPE(ap_rep, 15, untagged_ap_rep);

DEFFIELD(ap_rep_enc_part_0, krb5_ap_rep_enc_part, ctime, 0, kerberos_time);
DEFFIELD(ap_rep_enc_part_1, krb5_ap_rep_enc_part, cusec, 1, int32);
DEFFIELD(ap_rep_enc_part_2, krb5_ap_rep_enc_part, subkey, 2,
         opt_ptr_encryption_key);
DEFFIELD(ap_rep_enc_part_3, krb5_ap_rep_enc_part, seq_number, 3, opt_seqno);
static const struct atype_info *ap_rep_enc_part_fields[] = {
    &k5_atype_ap_rep_enc_part_0, &k5_atype_ap_rep_enc_part_1,
    &k5_atype_ap_rep_enc_part_2, &k5_atype_ap_rep_enc_part_3
};
DEFSEQTYPE(untagged_ap_rep_enc_part, krb5_ap_rep_enc_part,
           ap_rep_enc_part_fields);
DEFAPPTAGGEDTYPE(ap_rep_enc_part, 27, untagged_ap_rep_enc_part);

/* First context tag is 1.  Fourth field is the encoding of the krb5_kdc_req
 * structure as a KDC-REQ-BODY. */
DEFCTAGGEDTYPE(kdc_req_1, 1, krb5_version);
DEFFIELD(kdc_req_2, krb5_kdc_req, msg_type, 2, uint);
DEFFIELD(kdc_req_3, krb5_kdc_req, padata, 3, opt_ptr_seqof_pa_data);
DEFCTAGGEDTYPE(kdc_req_4, 4, kdc_req_body);
static const struct atype_info *kdc_req_fields[] = {
    &k5_atype_kdc_req_1, &k5_atype_kdc_req_2, &k5_atype_kdc_req_3,
    &k5_atype_kdc_req_4
};
DEFSEQTYPE(kdc_req, krb5_kdc_req, kdc_req_fields);
DEFAPPTAGGEDTYPE(as_req, 10, kdc_req);
DEFAPPTAGGEDTYPE(tgs_req, 12, kdc_req);

/* This is only needed because libkrb5 doesn't set msg_type when encoding
 * krb5_kdc_reqs.  If we fix that, we can use the above types for encoding. */
DEFINT_IMMEDIATE(as_req_msg_type, KRB5_AS_REQ, 0);
DEFCTAGGEDTYPE(as_req_2, 2, as_req_msg_type);
DEFINT_IMMEDIATE(tgs_req_msg_type, KRB5_TGS_REQ, 0);
DEFCTAGGEDTYPE(tgs_req_2, 2, tgs_req_msg_type);
static const struct atype_info *as_req_fields[] = {
    &k5_atype_kdc_req_1, &k5_atype_as_req_2, &k5_atype_kdc_req_3,
    &k5_atype_kdc_req_4
};
static const struct atype_info *tgs_req_fields[] = {
    &k5_atype_kdc_req_1, &k5_atype_tgs_req_2, &k5_atype_kdc_req_3,
    &k5_atype_kdc_req_4
};
DEFSEQTYPE(untagged_as_req, krb5_kdc_req, as_req_fields);
DEFAPPTAGGEDTYPE(as_req_encode, 10, untagged_as_req);
DEFSEQTYPE(untagged_tgs_req, krb5_kdc_req, tgs_req_fields);
DEFAPPTAGGEDTYPE(tgs_req_encode, 12, untagged_tgs_req);

DEFINT_IMMEDIATE(safe_msg_type, ASN1_KRB_SAFE, 0);
DEFCTAGGEDTYPE(safe_0, 0, krb5_version);
DEFCTAGGEDTYPE(safe_1, 1, safe_msg_type);
DEFCTAGGEDTYPE(safe_2, 2, safe_body);
DEFFIELD(safe_3, krb5_safe, checksum, 3, checksum_ptr);
static const struct atype_info *safe_fields[] = {
    &k5_atype_safe_0, &k5_atype_safe_1, &k5_atype_safe_2, &k5_atype_safe_3
};
DEFSEQTYPE(untagged_safe, krb5_safe, safe_fields);
DEFAPPTAGGEDTYPE(safe, 20, untagged_safe);

/* Hack to encode a KRB-SAFE with a pre-specified body encoding.  The integer-
 * immediate fields are borrowed from krb5_safe_fields above. */
DEFPTRTYPE(saved_safe_body_ptr, der_data);
DEFOFFSETTYPE(safe_checksum_only, krb5_safe, checksum, checksum_ptr);
DEFPTRTYPE(safe_checksum_only_ptr, safe_checksum_only);
DEFFIELD(safe_with_body_2, struct krb5_safe_with_body, body, 2,
         saved_safe_body_ptr);
DEFFIELD(safe_with_body_3, struct krb5_safe_with_body, safe, 3,
         safe_checksum_only_ptr);
static const struct atype_info *safe_with_body_fields[] = {
    &k5_atype_safe_0, &k5_atype_safe_1, &k5_atype_safe_with_body_2,
    &k5_atype_safe_with_body_3
};
DEFSEQTYPE(untagged_safe_with_body, struct krb5_safe_with_body,
           safe_with_body_fields);
DEFAPPTAGGEDTYPE(safe_with_body, 20, untagged_safe_with_body);

/* Third tag is [3] instead of [2]. */
DEFINT_IMMEDIATE(priv_msg_type, ASN1_KRB_PRIV, 0);
DEFCTAGGEDTYPE(priv_0, 0, krb5_version);
DEFCTAGGEDTYPE(priv_1, 1, priv_msg_type);
DEFFIELD(priv_3, krb5_priv, enc_part, 3, encrypted_data);
static const struct atype_info *priv_fields[] = {
    &k5_atype_priv_0, &k5_atype_priv_1, &k5_atype_priv_3
};
DEFSEQTYPE(untagged_priv, krb5_priv, priv_fields);
DEFAPPTAGGEDTYPE(priv, 21, untagged_priv);

static int
is_priv_timestamp_set(const void *p)
{
    const krb5_priv_enc_part *val = p;
    return (val->timestamp != 0);
}
DEFFIELD(priv_enc_part_0, krb5_priv_enc_part, user_data, 0, ostring_data);
DEFFIELD(priv_enc_part_1, krb5_priv_enc_part, timestamp, 1, opt_kerberos_time);
DEFFIELD(priv_enc_part_2_def, krb5_priv_enc_part, usec, 2, int32);
DEFOPTIONALTYPE(priv_enc_part_2, is_priv_timestamp_set, NULL,
                priv_enc_part_2_def);
DEFFIELD(priv_enc_part_3, krb5_priv_enc_part, seq_number, 3, opt_seqno);
DEFFIELD(priv_enc_part_4, krb5_priv_enc_part, s_address, 4, address_ptr);
DEFFIELD(priv_enc_part_5, krb5_priv_enc_part, r_address, 5, opt_address_ptr);
static const struct atype_info *priv_enc_part_fields[] = {
    &k5_atype_priv_enc_part_0, &k5_atype_priv_enc_part_1,
    &k5_atype_priv_enc_part_2, &k5_atype_priv_enc_part_3,
    &k5_atype_priv_enc_part_4, &k5_atype_priv_enc_part_5
};
DEFSEQTYPE(untagged_priv_enc_part, krb5_priv_enc_part, priv_enc_part_fields);
DEFAPPTAGGEDTYPE(priv_enc_part, 28, untagged_priv_enc_part);

DEFINT_IMMEDIATE(cred_msg_type, ASN1_KRB_CRED, 0);
DEFCTAGGEDTYPE(cred_0, 0, krb5_version);
DEFCTAGGEDTYPE(cred_1, 1, cred_msg_type);
DEFFIELD(cred_2, krb5_cred, tickets, 2, ptr_seqof_ticket);
DEFFIELD(cred_3, krb5_cred, enc_part, 3, encrypted_data);
static const struct atype_info *cred_fields[] = {
    &k5_atype_cred_0, &k5_atype_cred_1, &k5_atype_cred_2, &k5_atype_cred_3
};
DEFSEQTYPE(untagged_cred, krb5_cred, cred_fields);
DEFAPPTAGGEDTYPE(krb5_cred, 22, untagged_cred);

static int
is_cred_timestamp_set(const void *p)
{
    const krb5_cred_enc_part *val = p;
    return (val->timestamp != 0);
}
DEFFIELD(enc_cred_part_0, krb5_cred_enc_part, ticket_info, 0,
         ptrseqof_cred_info);
DEFFIELD(enc_cred_part_1, krb5_cred_enc_part, nonce, 1, opt_int32);
DEFFIELD(enc_cred_part_2, krb5_cred_enc_part, timestamp, 2, opt_kerberos_time);
DEFFIELD(enc_cred_part_3_def, krb5_cred_enc_part, usec, 3, int32);
DEFOPTIONALTYPE(enc_cred_part_3, is_cred_timestamp_set, NULL,
                enc_cred_part_3_def);
DEFFIELD(enc_cred_part_4, krb5_cred_enc_part, s_address, 4, opt_address_ptr);
DEFFIELD(enc_cred_part_5, krb5_cred_enc_part, r_address, 5, opt_address_ptr);
static const struct atype_info *enc_cred_part_fields[] = {
    &k5_atype_enc_cred_part_0, &k5_atype_enc_cred_part_1,
    &k5_atype_enc_cred_part_2, &k5_atype_enc_cred_part_3,
    &k5_atype_enc_cred_part_4, &k5_atype_enc_cred_part_5
};
DEFSEQTYPE(untagged_enc_cred_part, krb5_cred_enc_part, enc_cred_part_fields);
DEFAPPTAGGEDTYPE(enc_cred_part, 29, untagged_enc_cred_part);

DEFINT_IMMEDIATE(error_msg_type, ASN1_KRB_ERROR, 0);
DEFCTAGGEDTYPE(error_0, 0, krb5_version);
DEFCTAGGEDTYPE(error_1, 1, error_msg_type);
DEFFIELD(error_2, krb5_error, ctime, 2, opt_kerberos_time);
DEFFIELD(error_3, krb5_error, cusec, 3, opt_int32);
DEFFIELD(error_4, krb5_error, stime, 4, kerberos_time);
DEFFIELD(error_5, krb5_error, susec, 5, int32);
DEFFIELD(error_6, krb5_error, error, 6, ui_4);
DEFFIELD(error_7, krb5_error, client, 7, opt_realm_of_principal);
DEFFIELD(error_8, krb5_error, client, 8, opt_principal);
DEFFIELD(error_9, krb5_error, server, 9, realm_of_principal);
DEFFIELD(error_10, krb5_error, server, 10, principal);
DEFFIELD(error_11, krb5_error, text, 11, opt_gstring_data);
DEFFIELD(error_12, krb5_error, e_data, 12, opt_ostring_data);
static const struct atype_info *error_fields[] = {
    &k5_atype_error_0, &k5_atype_error_1, &k5_atype_error_2, &k5_atype_error_3,
    &k5_atype_error_4, &k5_atype_error_5, &k5_atype_error_6, &k5_atype_error_7,
    &k5_atype_error_8, &k5_atype_error_9, &k5_atype_error_10,
    &k5_atype_error_11, &k5_atype_error_12
};
DEFSEQTYPE(untagged_krb5_error, krb5_error, error_fields);
DEFAPPTAGGEDTYPE(krb5_error, 30, untagged_krb5_error);

DEFFIELD(pa_enc_ts_0, krb5_pa_enc_ts, patimestamp, 0, kerberos_time);
DEFFIELD(pa_enc_ts_1, krb5_pa_enc_ts, pausec, 1, opt_int32);
static const struct atype_info *pa_enc_ts_fields[] = {
    &k5_atype_pa_enc_ts_0, &k5_atype_pa_enc_ts_1
};
DEFSEQTYPE(pa_enc_ts, krb5_pa_enc_ts, pa_enc_ts_fields);

DEFFIELD(setpw_0, struct krb5_setpw_req, password, 0, ostring_data);
DEFFIELD(setpw_1, struct krb5_setpw_req, target, 1, principal);
DEFFIELD(setpw_2, struct krb5_setpw_req, target, 2, realm_of_principal);
static const struct atype_info *setpw_req_fields[] = {
    &k5_atype_setpw_0, &k5_atype_setpw_1, &k5_atype_setpw_2
};
DEFSEQTYPE(setpw_req, struct krb5_setpw_req, setpw_req_fields);

/* [MS-SFU] Section 2.2.1. */
DEFFIELD(pa_for_user_0, krb5_pa_for_user, user, 0, principal);
DEFFIELD(pa_for_user_1, krb5_pa_for_user, user, 1, realm_of_principal);
DEFFIELD(pa_for_user_2, krb5_pa_for_user, cksum, 2, checksum);
DEFFIELD(pa_for_user_3, krb5_pa_for_user, auth_package, 3, gstring_data);
static const struct atype_info *pa_for_user_fields[] = {
    &k5_atype_pa_for_user_0, &k5_atype_pa_for_user_1, &k5_atype_pa_for_user_2,
    &k5_atype_pa_for_user_3,
};
DEFSEQTYPE(pa_for_user, krb5_pa_for_user, pa_for_user_fields);

/* [MS-SFU] Section 2.2.2. */
/* The user principal name may be absent, but the realm is required. */
static int
is_s4u_principal_present(const void *p)
{
    krb5_const_principal val = *(krb5_const_principal *)p;
    return (val->length != 0);
}
DEFOPTIONALTYPE(opt_s4u_principal, is_s4u_principal_present, NULL, principal);
DEFFIELD(s4u_userid_0, krb5_s4u_userid, nonce, 0, int32);
DEFFIELD(s4u_userid_1, krb5_s4u_userid, user, 1, opt_s4u_principal);
DEFFIELD(s4u_userid_2, krb5_s4u_userid, user, 2, realm_of_principal);
DEFFIELD(s4u_userid_3, krb5_s4u_userid, subject_cert, 3, opt_ostring_data);
DEFFIELD(s4u_userid_4, krb5_s4u_userid, options, 4, opt_krb5_flags);
static const struct atype_info *s4u_userid_fields[] = {
    &k5_atype_s4u_userid_0, &k5_atype_s4u_userid_1, &k5_atype_s4u_userid_2,
    &k5_atype_s4u_userid_3, &k5_atype_s4u_userid_4
};
DEFSEQTYPE(s4u_userid, krb5_s4u_userid, s4u_userid_fields);

DEFFIELD(pa_s4u_x509_user_0, krb5_pa_s4u_x509_user, user_id, 0, s4u_userid);
DEFFIELD(pa_s4u_x509_user_1, krb5_pa_s4u_x509_user, cksum, 1, checksum);
static const struct atype_info *pa_s4u_x509_user_fields[] = {
    &k5_atype_pa_s4u_x509_user_0, &k5_atype_pa_s4u_x509_user_1
};
DEFSEQTYPE(pa_s4u_x509_user, krb5_pa_s4u_x509_user, pa_s4u_x509_user_fields);

DEFFIELD(pa_pac_req_0, krb5_pa_pac_req, include_pac, 0, boolean);
static const struct atype_info *pa_pac_req_fields[] = {
    &k5_atype_pa_pac_req_0
};
DEFSEQTYPE(pa_pac_req, krb5_pa_pac_req, pa_pac_req_fields);

/* RFC 4537 */
DEFCOUNTEDTYPE(etype_list, krb5_etype_list, etypes, length, cseqof_int32);

/* draft-ietf-krb-wg-preauth-framework-09 */
DEFFIELD(fast_armor_0, krb5_fast_armor, armor_type, 0, int32);
DEFFIELD(fast_armor_1, krb5_fast_armor, armor_value, 1, ostring_data);
static const struct atype_info *fast_armor_fields[] = {
    &k5_atype_fast_armor_0, &k5_atype_fast_armor_1
};
DEFSEQTYPE(fast_armor, krb5_fast_armor, fast_armor_fields);
DEFPTRTYPE(ptr_fast_armor, fast_armor);
DEFOPTIONALZEROTYPE(opt_ptr_fast_armor, ptr_fast_armor);

DEFFIELD(fast_armored_req_0, krb5_fast_armored_req, armor, 0,
         opt_ptr_fast_armor);
DEFFIELD(fast_armored_req_1, krb5_fast_armored_req, req_checksum, 1, checksum);
DEFFIELD(fast_armored_req_2, krb5_fast_armored_req, enc_part, 2,
         encrypted_data);
static const struct atype_info *fast_armored_req_fields[] = {
    &k5_atype_fast_armored_req_0, &k5_atype_fast_armored_req_1,
    &k5_atype_fast_armored_req_2
};
DEFSEQTYPE(fast_armored_req, krb5_fast_armored_req, fast_armored_req_fields);

/* This is a CHOICE type with only one choice (so far) and we're not using a
 * distinguisher/union for it. */
DEFTAGGEDTYPE(pa_fx_fast_request, CONTEXT_SPECIFIC, CONSTRUCTED, 0, 0,
              fast_armored_req);

DEFOFFSETTYPE(fast_req_padata, krb5_kdc_req, padata, ptr_seqof_pa_data);
DEFPTRTYPE(ptr_fast_req_padata, fast_req_padata);
DEFPTRTYPE(ptr_kdc_req_body, kdc_req_body);
DEFFIELD(fast_req_0, krb5_fast_req, fast_options, 0, krb5_flags);
DEFFIELD(fast_req_1, krb5_fast_req, req_body, 1, ptr_fast_req_padata);
DEFFIELD(fast_req_2, krb5_fast_req, req_body, 2, ptr_kdc_req_body);
static const struct atype_info *fast_req_fields[] = {
    &k5_atype_fast_req_0, &k5_atype_fast_req_1, &k5_atype_fast_req_2
};
DEFSEQTYPE(fast_req, krb5_fast_req, fast_req_fields);

DEFFIELD(fast_finished_0, krb5_fast_finished, timestamp, 0, kerberos_time);
DEFFIELD(fast_finished_1, krb5_fast_finished, usec, 1, int32);
DEFFIELD(fast_finished_2, krb5_fast_finished, client, 2, realm_of_principal);
DEFFIELD(fast_finished_3, krb5_fast_finished, client, 3, principal);
DEFFIELD(fast_finished_4, krb5_fast_finished, ticket_checksum, 4, checksum);
static const struct atype_info *fast_finished_fields[] = {
    &k5_atype_fast_finished_0, &k5_atype_fast_finished_1,
    &k5_atype_fast_finished_2, &k5_atype_fast_finished_3,
    &k5_atype_fast_finished_4
};
DEFSEQTYPE(fast_finished, krb5_fast_finished, fast_finished_fields);
DEFPTRTYPE(ptr_fast_finished, fast_finished);
DEFOPTIONALZEROTYPE(opt_ptr_fast_finished, ptr_fast_finished);

DEFFIELD(fast_response_0, krb5_fast_response, padata, 0, ptr_seqof_pa_data);
DEFFIELD(fast_response_1, krb5_fast_response, strengthen_key, 1,
         opt_ptr_encryption_key);
DEFFIELD(fast_response_2, krb5_fast_response, finished, 2,
         opt_ptr_fast_finished);
DEFFIELD(fast_response_3, krb5_fast_response, nonce, 3, int32);
static const struct atype_info *fast_response_fields[] = {
    &k5_atype_fast_response_0, &k5_atype_fast_response_1,
    &k5_atype_fast_response_2, &k5_atype_fast_response_3
};
DEFSEQTYPE(fast_response, krb5_fast_response, fast_response_fields);

DEFCTAGGEDTYPE(fast_rep_0, 0, encrypted_data);
static const struct atype_info *fast_rep_fields[] = {
    &k5_atype_fast_rep_0
};
DEFSEQTYPE(fast_rep, krb5_enc_data, fast_rep_fields);

/* This is a CHOICE type with only one choice (so far) and we're not using a
 * distinguisher/union for it. */
DEFTAGGEDTYPE(pa_fx_fast_reply, CONTEXT_SPECIFIC, CONSTRUCTED, 0, 0,
              fast_rep);

DEFFIELD(ad_kdcissued_0, krb5_ad_kdcissued, ad_checksum, 0, checksum);
DEFFIELD(ad_kdcissued_1, krb5_ad_kdcissued, i_principal, 1,
         opt_realm_of_principal);
DEFFIELD(ad_kdcissued_2, krb5_ad_kdcissued, i_principal, 2, opt_principal);
DEFFIELD(ad_kdcissued_3, krb5_ad_kdcissued, elements, 3, auth_data_ptr);
static const struct atype_info *ad_kdcissued_fields[] = {
    &k5_atype_ad_kdcissued_0, &k5_atype_ad_kdcissued_1,
    &k5_atype_ad_kdcissued_2, &k5_atype_ad_kdcissued_3
};
DEFSEQTYPE(ad_kdc_issued, krb5_ad_kdcissued, ad_kdcissued_fields);

DEFCTAGGEDTYPE(princ_plus_realm_0, 0, principal_data);
DEFCTAGGEDTYPE(princ_plus_realm_1, 1, realm_of_principal_data);
static const struct atype_info *princ_plus_realm_fields[] = {
    &k5_atype_princ_plus_realm_0, &k5_atype_princ_plus_realm_1
};
DEFSEQTYPE(princ_plus_realm_data, krb5_principal_data,
           princ_plus_realm_fields);
DEFPTRTYPE(princ_plus_realm, princ_plus_realm_data);
DEFNULLTERMSEQOFTYPE(seqof_princ_plus_realm, princ_plus_realm);
DEFPTRTYPE(ptr_seqof_princ_plus_realm, seqof_princ_plus_realm);
DEFOPTIONALEMPTYTYPE(opt_ptr_seqof_princ_plus_realm,
                     ptr_seqof_princ_plus_realm);

DEFFIELD(spdata_0, krb5_ad_signedpath_data, client, 0, princ_plus_realm);
DEFFIELD(spdata_1, krb5_ad_signedpath_data, authtime, 1, kerberos_time);
DEFFIELD(spdata_2, krb5_ad_signedpath_data, delegated, 2,
         opt_ptr_seqof_princ_plus_realm);
DEFFIELD(spdata_3, krb5_ad_signedpath_data, method_data, 3,
         opt_ptr_seqof_pa_data);
DEFFIELD(spdata_4, krb5_ad_signedpath_data, authorization_data, 4,
         opt_auth_data_ptr);
static const struct atype_info *ad_signedpath_data_fields[] = {
    &k5_atype_spdata_0, &k5_atype_spdata_1, &k5_atype_spdata_2,
    &k5_atype_spdata_3, &k5_atype_spdata_4
};
DEFSEQTYPE(ad_signedpath_data, krb5_ad_signedpath_data,
           ad_signedpath_data_fields);

DEFFIELD(signedpath_0, krb5_ad_signedpath, enctype, 0, int32);
DEFFIELD(signedpath_1, krb5_ad_signedpath, checksum, 1, checksum);
DEFFIELD(signedpath_2, krb5_ad_signedpath, delegated, 2,
         opt_ptr_seqof_princ_plus_realm);
DEFFIELD(signedpath_3, krb5_ad_signedpath, method_data, 3,
         opt_ptr_seqof_pa_data);
static const struct atype_info *ad_signedpath_fields[] = {
    &k5_atype_signedpath_0, &k5_atype_signedpath_1, &k5_atype_signedpath_2,
    &k5_atype_signedpath_3
};
DEFSEQTYPE(ad_signedpath, krb5_ad_signedpath, ad_signedpath_fields);

/* First context tag is 1, not 0. */
DEFFIELD(iakerb_header_1, krb5_iakerb_header, target_realm, 1, ostring_data);
DEFFIELD(iakerb_header_2, krb5_iakerb_header, cookie, 2, opt_ostring_data_ptr);
static const struct atype_info *iakerb_header_fields[] = {
    &k5_atype_iakerb_header_1, &k5_atype_iakerb_header_2
};
DEFSEQTYPE(iakerb_header, krb5_iakerb_header, iakerb_header_fields);

/* First context tag is 1, not 0. */
DEFFIELD(iakerb_finished_0, krb5_iakerb_finished, checksum, 1, checksum);
static const struct atype_info *iakerb_finished_fields[] = {
    &k5_atype_iakerb_finished_0
};
DEFSEQTYPE(iakerb_finished, krb5_iakerb_finished, iakerb_finished_fields);

/* Exported complete encoders -- these produce a krb5_data with
   the encoding in the correct byte order.  */

MAKE_ENCODER(encode_krb5_authenticator, authenticator);
MAKE_DECODER(decode_krb5_authenticator, authenticator);
MAKE_ENCODER(encode_krb5_ticket, ticket);
MAKE_DECODER(decode_krb5_ticket, ticket);
MAKE_ENCODER(encode_krb5_encryption_key, encryption_key);
MAKE_DECODER(decode_krb5_encryption_key, encryption_key);
MAKE_ENCODER(encode_krb5_enc_tkt_part, enc_tkt_part);
MAKE_DECODER(decode_krb5_enc_tkt_part, enc_tkt_part);

krb5_error_code KRB5_CALLCONV
krb5_decode_ticket(const krb5_data *code, krb5_ticket **repptr)
{
    return decode_krb5_ticket(code, repptr);
}

/*
 * For backwards compatibility, we encode both EncASRepPart and EncTGSRepPart
 * with application tag 26.  On decode, we accept either app tag and set the
 * msg_type field of the resulting structure.  This could be simplified and
 * pushed up into libkrb5.
 */
MAKE_ENCODER(encode_krb5_enc_kdc_rep_part, enc_tgs_rep_part);
krb5_error_code
decode_krb5_enc_kdc_rep_part(const krb5_data *code,
                             krb5_enc_kdc_rep_part **rep_out)
{
    asn1_error_code ret;
    krb5_enc_kdc_rep_part *rep;
    void *rep_ptr;
    krb5_msgtype msg_type = KRB5_TGS_REP;

    *rep_out = NULL;
    ret = k5_asn1_full_decode(code, &k5_atype_enc_tgs_rep_part, &rep_ptr);
    if (ret == ASN1_BAD_ID) {
        msg_type = KRB5_AS_REP;
        ret = k5_asn1_full_decode(code, &k5_atype_enc_as_rep_part, &rep_ptr);
    }
    if (ret)
        return ret;
    rep = rep_ptr;
    rep->msg_type = msg_type;
    *rep_out = rep;
    return 0;
}

MAKE_ENCODER(encode_krb5_as_rep, as_rep);
MAKE_DECODER(decode_krb5_as_rep, as_rep);
MAKE_ENCODER(encode_krb5_tgs_rep, tgs_rep);
MAKE_DECODER(decode_krb5_tgs_rep, tgs_rep);
MAKE_ENCODER(encode_krb5_ap_req, ap_req);
MAKE_DECODER(decode_krb5_ap_req, ap_req);
MAKE_ENCODER(encode_krb5_ap_rep, ap_rep);
MAKE_DECODER(decode_krb5_ap_rep, ap_rep);
MAKE_ENCODER(encode_krb5_ap_rep_enc_part, ap_rep_enc_part);
MAKE_DECODER(decode_krb5_ap_rep_enc_part, ap_rep_enc_part);
MAKE_ENCODER(encode_krb5_as_req, as_req_encode);
MAKE_DECODER(decode_krb5_as_req, as_req);
MAKE_ENCODER(encode_krb5_tgs_req, tgs_req_encode);
MAKE_DECODER(decode_krb5_tgs_req, tgs_req);
MAKE_ENCODER(encode_krb5_kdc_req_body, kdc_req_body);
MAKE_DECODER(decode_krb5_kdc_req_body, kdc_req_body);
MAKE_ENCODER(encode_krb5_safe, safe);
MAKE_DECODER(decode_krb5_safe, safe);

/* encode_krb5_safe_with_body takes a saved KRB-SAFE-BODY encoding to avoid
 * mismatches from re-encoding if the sender isn't quite DER-compliant. */
MAKE_ENCODER(encode_krb5_safe_with_body, safe_with_body);

/*
 * decode_krb5_safe_with_body fully decodes a KRB-SAFE, but also returns
 * the KRB-SAFE-BODY encoding.  This interface was designed for an earlier
 * generation of decoder and should probably be re-thought.
 */
krb5_error_code
decode_krb5_safe_with_body(const krb5_data *code, krb5_safe **rep_out,
                           krb5_data **body_out)
{
    asn1_error_code ret;
    void *swb_ptr, *safe_ptr;
    struct krb5_safe_with_body *swb;
    krb5_safe *safe;

    ret = k5_asn1_full_decode(code, &k5_atype_safe_with_body, &swb_ptr);
    if (ret)
        return ret;
    swb = swb_ptr;
    ret = k5_asn1_full_decode(swb->body, &k5_atype_safe_body, &safe_ptr);
    if (ret) {
        krb5_free_safe(NULL, swb->safe);
        krb5_free_data(NULL, swb->body);
        free(swb);
        return ret;
    }
    safe = safe_ptr;
    safe->checksum = swb->safe->checksum;
    free(swb->safe);
    *rep_out = safe;
    *body_out = swb->body;
    free(swb);
    return 0;
}

MAKE_ENCODER(encode_krb5_priv, priv);
MAKE_DECODER(decode_krb5_priv, priv);
MAKE_ENCODER(encode_krb5_enc_priv_part, priv_enc_part);
MAKE_DECODER(decode_krb5_enc_priv_part, priv_enc_part);
MAKE_ENCODER(encode_krb5_checksum, checksum);
MAKE_DECODER(decode_krb5_checksum, checksum);

MAKE_ENCODER(encode_krb5_cred, krb5_cred);
MAKE_DECODER(decode_krb5_cred, krb5_cred);
MAKE_ENCODER(encode_krb5_enc_cred_part, enc_cred_part);
MAKE_DECODER(decode_krb5_enc_cred_part, enc_cred_part);
MAKE_ENCODER(encode_krb5_error, krb5_error);
MAKE_DECODER(decode_krb5_error, krb5_error);
MAKE_ENCODER(encode_krb5_authdata, auth_data);
MAKE_DECODER(decode_krb5_authdata, auth_data);
MAKE_ENCODER(encode_krb5_etype_info, etype_info);
MAKE_DECODER(decode_krb5_etype_info, etype_info);
MAKE_ENCODER(encode_krb5_etype_info2, etype_info2);
MAKE_DECODER(decode_krb5_etype_info2, etype_info2);
MAKE_ENCODER(encode_krb5_enc_data, encrypted_data);
MAKE_DECODER(decode_krb5_enc_data, encrypted_data);
MAKE_ENCODER(encode_krb5_pa_enc_ts, pa_enc_ts);
MAKE_DECODER(decode_krb5_pa_enc_ts, pa_enc_ts);
MAKE_ENCODER(encode_krb5_padata_sequence, seqof_pa_data);
MAKE_DECODER(decode_krb5_padata_sequence, seqof_pa_data);
/* sam preauth additions */
MAKE_ENCODER(encode_krb5_sam_challenge_2, sam_challenge_2);
MAKE_DECODER(decode_krb5_sam_challenge_2, sam_challenge_2);
MAKE_ENCODER(encode_krb5_sam_challenge_2_body, sam_challenge_2_body);
MAKE_DECODER(decode_krb5_sam_challenge_2_body, sam_challenge_2_body);
MAKE_ENCODER(encode_krb5_enc_sam_response_enc_2, enc_sam_response_enc_2);
MAKE_DECODER(decode_krb5_enc_sam_response_enc_2, enc_sam_response_enc_2);
MAKE_ENCODER(encode_krb5_sam_response_2, sam_response_2);
MAKE_DECODER(decode_krb5_sam_response_2, sam_response_2);

/* setpw_req has an odd decoder interface which should probably be
 * normalized. */
MAKE_ENCODER(encode_krb5_setpw_req, setpw_req);
krb5_error_code
decode_krb5_setpw_req(const krb5_data *code, krb5_data **password_out,
                      krb5_principal *target_out)
{
    asn1_error_code ret;
    void *req_ptr;
    struct krb5_setpw_req *req;
    krb5_data *data;

    *password_out = NULL;
    *target_out = NULL;
    data = malloc(sizeof(*data));
    if (data == NULL)
        return ENOMEM;
    ret = k5_asn1_full_decode(code, &k5_atype_setpw_req, &req_ptr);
    if (ret) {
        free(data);
        return ret;
    }
    req = req_ptr;
    *data = req->password;
    *password_out = data;
    *target_out = req->target;
    return 0;
}

MAKE_ENCODER(encode_krb5_pa_for_user, pa_for_user);
MAKE_DECODER(decode_krb5_pa_for_user, pa_for_user);
MAKE_ENCODER(encode_krb5_s4u_userid, s4u_userid);
MAKE_ENCODER(encode_krb5_pa_s4u_x509_user, pa_s4u_x509_user);
MAKE_DECODER(decode_krb5_pa_s4u_x509_user, pa_s4u_x509_user);
MAKE_ENCODER(encode_krb5_pa_pac_req, pa_pac_req);
MAKE_DECODER(decode_krb5_pa_pac_req, pa_pac_req);
MAKE_ENCODER(encode_krb5_etype_list, etype_list);
MAKE_DECODER(decode_krb5_etype_list, etype_list);

MAKE_ENCODER(encode_krb5_pa_fx_fast_request, pa_fx_fast_request);
MAKE_DECODER(decode_krb5_pa_fx_fast_request, pa_fx_fast_request);
MAKE_ENCODER(encode_krb5_fast_req, fast_req);
MAKE_DECODER(decode_krb5_fast_req, fast_req);
MAKE_ENCODER(encode_krb5_pa_fx_fast_reply, pa_fx_fast_reply);
MAKE_DECODER(decode_krb5_pa_fx_fast_reply, pa_fx_fast_reply);
MAKE_ENCODER(encode_krb5_fast_response, fast_response);
MAKE_DECODER(decode_krb5_fast_response, fast_response);

MAKE_ENCODER(encode_krb5_ad_kdcissued, ad_kdc_issued);
MAKE_DECODER(decode_krb5_ad_kdcissued, ad_kdc_issued);
MAKE_ENCODER(encode_krb5_ad_signedpath_data, ad_signedpath_data);
MAKE_ENCODER(encode_krb5_ad_signedpath, ad_signedpath);
MAKE_DECODER(decode_krb5_ad_signedpath, ad_signedpath);
MAKE_ENCODER(encode_krb5_iakerb_header, iakerb_header);
MAKE_DECODER(decode_krb5_iakerb_header, iakerb_header);
MAKE_ENCODER(encode_krb5_iakerb_finished, iakerb_finished);
MAKE_DECODER(decode_krb5_iakerb_finished, iakerb_finished);

krb5_error_code KRB5_CALLCONV
krb5int_get_authdata_containee_types(krb5_context context,
                                     const krb5_authdata *authdata,
                                     unsigned int *num_out,
                                     krb5_authdatatype **types_out)
{
    asn1_error_code ret;
    struct authdata_types *atypes;
    void *atypes_ptr;
    krb5_data d = make_data(authdata->contents, authdata->length);

    ret = k5_asn1_full_decode(&d, &k5_atype_authdata_types, &atypes_ptr);
    if (ret)
        return ret;
    atypes = atypes_ptr;
    *num_out = atypes->ntypes;
    *types_out = atypes->types;
    free(atypes);
    return 0;
}

/* RFC 3280.  No context tags. */
DEFOFFSETTYPE(algid_0, krb5_algorithm_identifier, algorithm, oid_data);
DEFOFFSETTYPE(algid_1, krb5_algorithm_identifier, parameters, opt_der_data);
static const struct atype_info *algorithm_identifier_fields[] = {
    &k5_atype_algid_0, &k5_atype_algid_1
};
DEFSEQTYPE(algorithm_identifier, krb5_algorithm_identifier,
           algorithm_identifier_fields);
DEFPTRTYPE(ptr_algorithm_identifier, algorithm_identifier);
DEFOPTIONALZEROTYPE(opt_ptr_algorithm_identifier, ptr_algorithm_identifier);
DEFNULLTERMSEQOFTYPE(seqof_algorithm_identifier, ptr_algorithm_identifier);
DEFPTRTYPE(ptr_seqof_algorithm_identifier, seqof_algorithm_identifier);
DEFOPTIONALEMPTYTYPE(opt_ptr_seqof_algorithm_identifier,
                     ptr_seqof_algorithm_identifier);

/*
 * PKINIT
 */

#ifndef DISABLE_PKINIT

DEFCTAGGEDTYPE(kdf_alg_id_0, 0, oid_data);
static const struct atype_info *kdf_alg_id_fields[] = {
    &k5_atype_kdf_alg_id_0
};
DEFSEQTYPE(kdf_alg_id, krb5_data, kdf_alg_id_fields);
DEFPTRTYPE(ptr_kdf_alg_id, kdf_alg_id);
DEFNONEMPTYNULLTERMSEQOFTYPE(supported_kdfs, ptr_kdf_alg_id);
DEFPTRTYPE(ptr_supported_kdfs, supported_kdfs);
DEFOPTIONALZEROTYPE(opt_ptr_kdf_alg_id, ptr_kdf_alg_id);
DEFOPTIONALZEROTYPE(opt_ptr_supported_kdfs, ptr_supported_kdfs);

/* KRB5PrincipalName from RFC 4556 (*not* PrincipalName from RFC 4120) */
DEFCTAGGEDTYPE(pkinit_princ_0, 0, realm_of_principal_data);
DEFCTAGGEDTYPE(pkinit_princ_1, 1, principal_data);
static const struct atype_info *pkinit_krb5_principal_name_fields[] = {
    &k5_atype_pkinit_princ_0, &k5_atype_pkinit_princ_1
};
DEFSEQTYPE(pkinit_krb5_principal_name_data, krb5_principal_data,
           pkinit_krb5_principal_name_fields);
DEFPTRTYPE(pkinit_krb5_principal_name, pkinit_krb5_principal_name_data);

/* SP80056A OtherInfo, for pkinit agility.  No context tag on first field. */
DEFTAGGEDTYPE(pkinit_krb5_principal_name_wrapped, UNIVERSAL, PRIMITIVE,
              ASN1_OCTETSTRING, 0, pkinit_krb5_principal_name);
DEFOFFSETTYPE(oinfo_notag, krb5_sp80056a_other_info, algorithm_identifier,
              algorithm_identifier);
DEFFIELD(oinfo_0, krb5_sp80056a_other_info, party_u_info, 0,
         pkinit_krb5_principal_name_wrapped);
DEFFIELD(oinfo_1, krb5_sp80056a_other_info, party_v_info, 1,
         pkinit_krb5_principal_name_wrapped);
DEFFIELD(oinfo_2, krb5_sp80056a_other_info, supp_pub_info, 2, ostring_data);
static const struct atype_info *sp80056a_other_info_fields[] = {
    &k5_atype_oinfo_notag, &k5_atype_oinfo_0, &k5_atype_oinfo_1,
    &k5_atype_oinfo_2
};
DEFSEQTYPE(sp80056a_other_info, krb5_sp80056a_other_info,
           sp80056a_other_info_fields);

/* For PkinitSuppPubInfo, for pkinit agility */
DEFFIELD(supp_pub_0, krb5_pkinit_supp_pub_info, enctype, 0, int32);
DEFFIELD(supp_pub_1, krb5_pkinit_supp_pub_info, as_req, 1, ostring_data);
DEFFIELD(supp_pub_2, krb5_pkinit_supp_pub_info, pk_as_rep, 2, ostring_data);
static const struct atype_info *pkinit_supp_pub_info_fields[] = {
    &k5_atype_supp_pub_0, &k5_atype_supp_pub_1, &k5_atype_supp_pub_2
};
DEFSEQTYPE(pkinit_supp_pub_info, krb5_pkinit_supp_pub_info,
           pkinit_supp_pub_info_fields);

MAKE_ENCODER(encode_krb5_pkinit_supp_pub_info, pkinit_supp_pub_info);
MAKE_ENCODER(encode_krb5_sp80056a_other_info, sp80056a_other_info);

/* A krb5_checksum encoded as an OCTET STRING, for PKAuthenticator. */
DEFCOUNTEDTYPE(ostring_checksum, krb5_checksum, contents, length, octetstring);

DEFFIELD(pk_authenticator_0, krb5_pk_authenticator, cusec, 0, int32);
DEFFIELD(pk_authenticator_1, krb5_pk_authenticator, ctime, 1, kerberos_time);
DEFFIELD(pk_authenticator_2, krb5_pk_authenticator, nonce, 2, int32);
DEFFIELD(pk_authenticator_3, krb5_pk_authenticator, paChecksum, 3,
         ostring_checksum);
static const struct atype_info *pk_authenticator_fields[] = {
    &k5_atype_pk_authenticator_0, &k5_atype_pk_authenticator_1,
    &k5_atype_pk_authenticator_2, &k5_atype_pk_authenticator_3
};
DEFSEQTYPE(pk_authenticator, krb5_pk_authenticator, pk_authenticator_fields);

DEFFIELD(pkauth9_0, krb5_pk_authenticator_draft9, kdcName, 0, principal);
DEFFIELD(pkauth9_1, krb5_pk_authenticator_draft9, kdcName, 1,
         realm_of_principal);
DEFFIELD(pkauth9_2, krb5_pk_authenticator_draft9, cusec, 2, int32);
DEFFIELD(pkauth9_3, krb5_pk_authenticator_draft9, ctime, 3, kerberos_time);
DEFFIELD(pkauth9_4, krb5_pk_authenticator_draft9, nonce, 4, int32);
static const struct atype_info *pk_authenticator_draft9_fields[] = {
    &k5_atype_pkauth9_0, &k5_atype_pkauth9_1, &k5_atype_pkauth9_2,
    &k5_atype_pkauth9_3, &k5_atype_pkauth9_4
};
DEFSEQTYPE(pk_authenticator_draft9, krb5_pk_authenticator_draft9,
           pk_authenticator_draft9_fields);

DEFCOUNTEDSTRINGTYPE(s_bitstring, char *, unsigned int,
                     k5_asn1_encode_bitstring, k5_asn1_decode_bitstring,
                     ASN1_BITSTRING);
DEFCOUNTEDTYPE(bitstring_data, krb5_data, data, length, s_bitstring);

/* RFC 3280.  No context tags. */
DEFOFFSETTYPE(spki_0, krb5_subject_pk_info, algorithm, algorithm_identifier);
DEFOFFSETTYPE(spki_1, krb5_subject_pk_info, subjectPublicKey, bitstring_data);
static const struct atype_info *subject_pk_info_fields[] = {
    &k5_atype_spki_0, &k5_atype_spki_1
};
DEFSEQTYPE(subject_pk_info, krb5_subject_pk_info, subject_pk_info_fields);
DEFPTRTYPE(subject_pk_info_ptr, subject_pk_info);
DEFOPTIONALZEROTYPE(opt_subject_pk_info_ptr, subject_pk_info_ptr);

DEFFIELD(auth_pack_0, krb5_auth_pack, pkAuthenticator, 0, pk_authenticator);
DEFFIELD(auth_pack_1, krb5_auth_pack, clientPublicValue, 1,
         opt_subject_pk_info_ptr);
DEFFIELD(auth_pack_2, krb5_auth_pack, supportedCMSTypes, 2,
         opt_ptr_seqof_algorithm_identifier);
DEFFIELD(auth_pack_3, krb5_auth_pack, clientDHNonce, 3, opt_ostring_data);
DEFFIELD(auth_pack_4, krb5_auth_pack, supportedKDFs, 4,
         opt_ptr_supported_kdfs);
static const struct atype_info *auth_pack_fields[] = {
    &k5_atype_auth_pack_0, &k5_atype_auth_pack_1, &k5_atype_auth_pack_2,
    &k5_atype_auth_pack_3, &k5_atype_auth_pack_4
};
DEFSEQTYPE(auth_pack, krb5_auth_pack, auth_pack_fields);

DEFFIELD(auth_pack9_0, krb5_auth_pack_draft9, pkAuthenticator, 0,
         pk_authenticator_draft9);
DEFFIELD(auth_pack9_1, krb5_auth_pack_draft9, clientPublicValue, 1,
         opt_subject_pk_info_ptr);
static const struct atype_info *auth_pack_draft9_fields[] = {
    &k5_atype_auth_pack9_0, &k5_atype_auth_pack9_1
};
DEFSEQTYPE(auth_pack_draft9, krb5_auth_pack_draft9, auth_pack_draft9_fields);

DEFFIELD_IMPLICIT(extprinc_0, krb5_external_principal_identifier,
                  subjectName, 0, opt_ostring_data);
DEFFIELD_IMPLICIT(extprinc_1, krb5_external_principal_identifier,
                  issuerAndSerialNumber, 1, opt_ostring_data);
DEFFIELD_IMPLICIT(extprinc_2, krb5_external_principal_identifier,
                  subjectKeyIdentifier, 2, opt_ostring_data);
static const struct atype_info *external_principal_identifier_fields[] = {
    &k5_atype_extprinc_0, &k5_atype_extprinc_1, &k5_atype_extprinc_2
};
DEFSEQTYPE(external_principal_identifier, krb5_external_principal_identifier,
           external_principal_identifier_fields);
DEFPTRTYPE(external_principal_identifier_ptr, external_principal_identifier);

DEFNULLTERMSEQOFTYPE(seqof_external_principal_identifier,
                     external_principal_identifier_ptr);
DEFPTRTYPE(ptr_seqof_external_principal_identifier,
           seqof_external_principal_identifier);
DEFOPTIONALZEROTYPE(opt_ptr_seqof_external_principal_identifier,
                    ptr_seqof_external_principal_identifier);

DEFFIELD_IMPLICIT(pa_pk_as_req_0, krb5_pa_pk_as_req, signedAuthPack, 0,
                  ostring_data);
DEFFIELD(pa_pk_as_req_1, krb5_pa_pk_as_req, trustedCertifiers, 1,
         opt_ptr_seqof_external_principal_identifier);
DEFFIELD_IMPLICIT(pa_pk_as_req_2, krb5_pa_pk_as_req, kdcPkId, 2,
                  opt_ostring_data);
static const struct atype_info *pa_pk_as_req_fields[] = {
    &k5_atype_pa_pk_as_req_0, &k5_atype_pa_pk_as_req_1,
    &k5_atype_pa_pk_as_req_2
};
DEFSEQTYPE(pa_pk_as_req, krb5_pa_pk_as_req, pa_pk_as_req_fields);

/*
 * In draft-ietf-cat-kerberos-pk-init-09, this sequence has four fields, but we
 * only ever use the first and third.  The fields are specified as explicitly
 * tagged, but our historical behavior is to pretend that they are wrapped in
 * IMPLICIT OCTET STRING (i.e., generate primitive context tags), and we don't
 * want to change that without interop testing.
 */
DEFFIELD_IMPLICIT(pa_pk_as_req9_0, krb5_pa_pk_as_req_draft9, signedAuthPack, 0,
                  ostring_data);
DEFFIELD_IMPLICIT(pa_pk_as_req9_2, krb5_pa_pk_as_req_draft9, kdcCert, 2,
                  opt_ostring_data);
static const struct atype_info *pa_pk_as_req_draft9_fields[] = {
    &k5_atype_pa_pk_as_req9_0, &k5_atype_pa_pk_as_req9_2
};
DEFSEQTYPE(pa_pk_as_req_draft9, krb5_pa_pk_as_req_draft9,
           pa_pk_as_req_draft9_fields);
/* For decoding, we only care about the first field; we can ignore the rest. */
static const struct atype_info *pa_pk_as_req_draft9_decode_fields[] = {
    &k5_atype_pa_pk_as_req9_0
};
DEFSEQTYPE(pa_pk_as_req_draft9_decode, krb5_pa_pk_as_req_draft9,
           pa_pk_as_req_draft9_decode_fields);

DEFFIELD_IMPLICIT(dh_rep_info_0, krb5_dh_rep_info, dhSignedData, 0,
                  ostring_data);
DEFFIELD(dh_rep_info_1, krb5_dh_rep_info, serverDHNonce, 1, opt_ostring_data);
DEFFIELD(dh_rep_info_2, krb5_dh_rep_info, kdfID, 2, opt_ptr_kdf_alg_id);
static const struct atype_info *dh_rep_info_fields[] = {
    &k5_atype_dh_rep_info_0, &k5_atype_dh_rep_info_1, &k5_atype_dh_rep_info_2
};
DEFSEQTYPE(dh_rep_info, krb5_dh_rep_info, dh_rep_info_fields);

DEFFIELD(dh_key_0, krb5_kdc_dh_key_info, subjectPublicKey, 0, bitstring_data);
DEFFIELD(dh_key_1, krb5_kdc_dh_key_info, nonce, 1, int32);
DEFFIELD(dh_key_2, krb5_kdc_dh_key_info, dhKeyExpiration, 2,
         opt_kerberos_time);
static const struct atype_info *kdc_dh_key_info_fields[] = {
    &k5_atype_dh_key_0, &k5_atype_dh_key_1, &k5_atype_dh_key_2
};
DEFSEQTYPE(kdc_dh_key_info, krb5_kdc_dh_key_info, kdc_dh_key_info_fields);

DEFFIELD(reply_key_pack_0, krb5_reply_key_pack, replyKey, 0, encryption_key);
DEFFIELD(reply_key_pack_1, krb5_reply_key_pack, asChecksum, 1, checksum);
static const struct atype_info *reply_key_pack_fields[] = {
    &k5_atype_reply_key_pack_0, &k5_atype_reply_key_pack_1
};
DEFSEQTYPE(reply_key_pack, krb5_reply_key_pack, reply_key_pack_fields);

DEFFIELD(key_pack9_0, krb5_reply_key_pack_draft9, replyKey, 0, encryption_key);
DEFFIELD(key_pack9_1, krb5_reply_key_pack_draft9, nonce, 1, int32);
static const struct atype_info *reply_key_pack_draft9_fields[] = {
    &k5_atype_key_pack9_0, &k5_atype_key_pack9_1
};
DEFSEQTYPE(reply_key_pack_draft9, krb5_reply_key_pack_draft9,
           reply_key_pack_draft9_fields);

DEFCTAGGEDTYPE(pa_pk_as_rep_0, 0, dh_rep_info);
DEFCTAGGEDTYPE_IMPLICIT(pa_pk_as_rep_1, 1, ostring_data);
static const struct atype_info *pa_pk_as_rep_alternatives[] = {
    &k5_atype_pa_pk_as_rep_0, &k5_atype_pa_pk_as_rep_1
};
DEFCHOICETYPE(pa_pk_as_rep_choice, union krb5_pa_pk_as_rep_choices,
              enum krb5_pa_pk_as_rep_selection, pa_pk_as_rep_alternatives);
DEFCOUNTEDTYPE_SIGNED(pa_pk_as_rep, krb5_pa_pk_as_rep, u, choice,
                      pa_pk_as_rep_choice);

/*
 * draft-ietf-cat-kerberos-pk-init-09 specifies these alternatives as
 * explicitly tagged SignedData and EnvelopedData respectively, which means
 * they should have constructed context tags.  However, our historical behavior
 * is to use primitive context tags, and we don't want to change that behavior
 * without interop testing.  We have the encodings for each alternative in a
 * krb5_data object; pretend that they are wrapped in IMPLICIT OCTET STRING in
 * order to wrap them in primitive [0] and [1] tags.
 */
DEFCTAGGEDTYPE_IMPLICIT(pa_pk_as_rep9_0, 0, ostring_data);
DEFCTAGGEDTYPE_IMPLICIT(pa_pk_as_rep9_1, 1, ostring_data);
static const struct atype_info *pa_pk_as_rep_draft9_alternatives[] = {
    &k5_atype_pa_pk_as_rep9_0, &k5_atype_pa_pk_as_rep9_1
};
DEFCHOICETYPE(pa_pk_as_rep_draft9_choice,
              union krb5_pa_pk_as_rep_draft9_choices,
              enum krb5_pa_pk_as_rep_draft9_selection,
              pa_pk_as_rep_draft9_alternatives);
DEFCOUNTEDTYPE_SIGNED(pa_pk_as_rep_draft9, krb5_pa_pk_as_rep_draft9, u, choice,
                      pa_pk_as_rep_draft9_choice);

MAKE_ENCODER(encode_krb5_pa_pk_as_req, pa_pk_as_req);
MAKE_DECODER(decode_krb5_pa_pk_as_req, pa_pk_as_req);
MAKE_ENCODER(encode_krb5_pa_pk_as_req_draft9, pa_pk_as_req_draft9);
MAKE_DECODER(decode_krb5_pa_pk_as_req_draft9, pa_pk_as_req_draft9_decode);
MAKE_ENCODER(encode_krb5_pa_pk_as_rep, pa_pk_as_rep);
MAKE_DECODER(decode_krb5_pa_pk_as_rep, pa_pk_as_rep);
MAKE_ENCODER(encode_krb5_pa_pk_as_rep_draft9, pa_pk_as_rep_draft9);
MAKE_ENCODER(encode_krb5_auth_pack, auth_pack);
MAKE_DECODER(decode_krb5_auth_pack, auth_pack);
MAKE_ENCODER(encode_krb5_auth_pack_draft9, auth_pack_draft9);
MAKE_DECODER(decode_krb5_auth_pack_draft9, auth_pack_draft9);
MAKE_ENCODER(encode_krb5_kdc_dh_key_info, kdc_dh_key_info);
MAKE_DECODER(decode_krb5_kdc_dh_key_info, kdc_dh_key_info);
MAKE_ENCODER(encode_krb5_reply_key_pack, reply_key_pack);
MAKE_DECODER(decode_krb5_reply_key_pack, reply_key_pack);
MAKE_ENCODER(encode_krb5_reply_key_pack_draft9, reply_key_pack_draft9);
MAKE_DECODER(decode_krb5_reply_key_pack_draft9, reply_key_pack_draft9);
MAKE_ENCODER(encode_krb5_td_trusted_certifiers,
             seqof_external_principal_identifier);
MAKE_DECODER(decode_krb5_td_trusted_certifiers,
             seqof_external_principal_identifier);
MAKE_ENCODER(encode_krb5_td_dh_parameters, seqof_algorithm_identifier);
MAKE_DECODER(decode_krb5_td_dh_parameters, seqof_algorithm_identifier);
MAKE_DECODER(decode_krb5_principal_name, pkinit_krb5_principal_name_data);

#else /* DISABLE_PKINIT */

/* Stubs for exported pkinit encoder functions. */

krb5_error_code
encode_krb5_sp80056a_other_info(const krb5_sp80056a_other_info *rep,
                                krb5_data **code)
{
    return EINVAL;
}

krb5_error_code
encode_krb5_pkinit_supp_pub_info(const krb5_pkinit_supp_pub_info *rep,
                                 krb5_data **code)
{
    return EINVAL;
}

#endif /* not DISABLE_PKINIT */

DEFFIELD(typed_data_0, krb5_pa_data, pa_type, 0, int32);
DEFCNFIELD(typed_data_1, krb5_pa_data, contents, length, 1, octetstring);
static const struct atype_info *typed_data_fields[] = {
    &k5_atype_typed_data_0, &k5_atype_typed_data_1
};
DEFSEQTYPE(typed_data, krb5_pa_data, typed_data_fields);
DEFPTRTYPE(typed_data_ptr, typed_data);

DEFNULLTERMSEQOFTYPE(seqof_typed_data, typed_data_ptr);
MAKE_ENCODER(encode_krb5_typed_data, seqof_typed_data);
MAKE_DECODER(decode_krb5_typed_data, seqof_typed_data);

/* Definitions for OTP preauth (RFC 6560) */

DEFFIELD_IMPLICIT(tokinfo_0, krb5_otp_tokeninfo, flags, 0, krb5_flags);
DEFFIELD_IMPLICIT(tokinfo_1, krb5_otp_tokeninfo, vendor, 1, opt_utf8_data);
DEFFIELD_IMPLICIT(tokinfo_2, krb5_otp_tokeninfo, challenge, 2,
                  opt_ostring_data);
DEFFIELD_IMPLICIT(tokinfo_3, krb5_otp_tokeninfo, length, 3, opt_int32_minus1);
DEFFIELD_IMPLICIT(tokinfo_4, krb5_otp_tokeninfo, format, 4, opt_int32_minus1);
DEFFIELD_IMPLICIT(tokinfo_5, krb5_otp_tokeninfo, token_id, 5,
                  opt_ostring_data);
DEFFIELD_IMPLICIT(tokinfo_6, krb5_otp_tokeninfo, alg_id, 6, opt_utf8_data);
DEFFIELD_IMPLICIT(tokinfo_7, krb5_otp_tokeninfo, supported_hash_alg, 7,
                  opt_ptr_seqof_algorithm_identifier);
DEFFIELD_IMPLICIT(tokinfo_8, krb5_otp_tokeninfo, iteration_count, 8,
                  opt_int32_minus1);
static const struct atype_info *otp_tokeninfo_fields[] = {
    &k5_atype_tokinfo_0, &k5_atype_tokinfo_1, &k5_atype_tokinfo_2,
    &k5_atype_tokinfo_3, &k5_atype_tokinfo_4, &k5_atype_tokinfo_5,
    &k5_atype_tokinfo_6, &k5_atype_tokinfo_7, &k5_atype_tokinfo_8
};
DEFSEQTYPE(otp_tokeninfo, krb5_otp_tokeninfo, otp_tokeninfo_fields);
MAKE_ENCODER(encode_krb5_otp_tokeninfo, otp_tokeninfo);
MAKE_DECODER(decode_krb5_otp_tokeninfo, otp_tokeninfo);

DEFPTRTYPE(otp_tokeninfo_ptr, otp_tokeninfo);
DEFNONEMPTYNULLTERMSEQOFTYPE(seqof_otp_tokeninfo, otp_tokeninfo_ptr);
DEFPTRTYPE(ptr_seqof_otp_tokeninfo, seqof_otp_tokeninfo);

DEFFIELD_IMPLICIT(otp_ch_0, krb5_pa_otp_challenge, nonce, 0, ostring_data);
DEFFIELD_IMPLICIT(otp_ch_1, krb5_pa_otp_challenge, service, 1, opt_utf8_data);
DEFFIELD_IMPLICIT(otp_ch_2, krb5_pa_otp_challenge, tokeninfo, 2,
                  ptr_seqof_otp_tokeninfo);
DEFFIELD_IMPLICIT(otp_ch_3, krb5_pa_otp_challenge, salt, 3, opt_gstring_data);
DEFFIELD_IMPLICIT(otp_ch_4, krb5_pa_otp_challenge, s2kparams, 4,
                  opt_ostring_data);
static const struct atype_info *pa_otp_challenge_fields[] = {
    &k5_atype_otp_ch_0, &k5_atype_otp_ch_1, &k5_atype_otp_ch_2,
    &k5_atype_otp_ch_3, &k5_atype_otp_ch_4
};
DEFSEQTYPE(pa_otp_challenge, krb5_pa_otp_challenge, pa_otp_challenge_fields);
MAKE_ENCODER(encode_krb5_pa_otp_challenge, pa_otp_challenge);
MAKE_DECODER(decode_krb5_pa_otp_challenge, pa_otp_challenge);

DEFFIELD_IMPLICIT(otp_req_0, krb5_pa_otp_req, flags, 0, krb5_flags);
DEFFIELD_IMPLICIT(otp_req_1, krb5_pa_otp_req, nonce, 1, opt_ostring_data);
DEFFIELD_IMPLICIT(otp_req_2, krb5_pa_otp_req, enc_data, 2, encrypted_data);
DEFFIELD_IMPLICIT(otp_req_3, krb5_pa_otp_req, hash_alg, 3,
                  opt_ptr_algorithm_identifier);
DEFFIELD_IMPLICIT(otp_req_4, krb5_pa_otp_req, iteration_count, 4,
                  opt_int32_minus1);
DEFFIELD_IMPLICIT(otp_req_5, krb5_pa_otp_req, otp_value, 5, opt_ostring_data);
DEFFIELD_IMPLICIT(otp_req_6, krb5_pa_otp_req, pin, 6, opt_utf8_data);
DEFFIELD_IMPLICIT(otp_req_7, krb5_pa_otp_req, challenge, 7, opt_ostring_data);
DEFFIELD_IMPLICIT(otp_req_8, krb5_pa_otp_req, time, 8, opt_kerberos_time);
DEFFIELD_IMPLICIT(otp_req_9, krb5_pa_otp_req, counter, 9, opt_ostring_data);
DEFFIELD_IMPLICIT(otp_req_10, krb5_pa_otp_req, format, 10, opt_int32_minus1);
DEFFIELD_IMPLICIT(otp_req_11, krb5_pa_otp_req, token_id, 11, opt_ostring_data);
DEFFIELD_IMPLICIT(otp_req_12, krb5_pa_otp_req, alg_id, 12, opt_utf8_data);
DEFFIELD_IMPLICIT(otp_req_13, krb5_pa_otp_req, vendor, 13, opt_utf8_data);
static const struct atype_info *pa_otp_req_fields[] = {
    &k5_atype_otp_req_0, &k5_atype_otp_req_1, &k5_atype_otp_req_2,
    &k5_atype_otp_req_3, &k5_atype_otp_req_4, &k5_atype_otp_req_5,
    &k5_atype_otp_req_6, &k5_atype_otp_req_7, &k5_atype_otp_req_8,
    &k5_atype_otp_req_9, &k5_atype_otp_req_10, &k5_atype_otp_req_11,
    &k5_atype_otp_req_12, &k5_atype_otp_req_13
};
DEFSEQTYPE(pa_otp_req, krb5_pa_otp_req, pa_otp_req_fields);
MAKE_ENCODER(encode_krb5_pa_otp_req, pa_otp_req);
MAKE_DECODER(decode_krb5_pa_otp_req, pa_otp_req);

DEFCTAGGEDTYPE_IMPLICIT(pa_otp_enc_req_0, 0, ostring_data);
static const struct atype_info *pa_otp_enc_req_fields[] = {
    &k5_atype_pa_otp_enc_req_0
};
DEFSEQTYPE(pa_otp_enc_req, krb5_data, pa_otp_enc_req_fields);
MAKE_ENCODER(encode_krb5_pa_otp_enc_req, pa_otp_enc_req);
MAKE_DECODER(decode_krb5_pa_otp_enc_req, pa_otp_enc_req);

DEFFIELD(kkdcp_message_0, krb5_kkdcp_message,
         kerb_message, 0, ostring_data);
DEFFIELD(kkdcp_message_1, krb5_kkdcp_message,
         target_domain, 1, opt_gstring_data);
DEFFIELD(kkdcp_message_2, krb5_kkdcp_message,
         dclocator_hint, 2, opt_int32);
static const struct atype_info *kkdcp_message_fields[] = {
    &k5_atype_kkdcp_message_0, &k5_atype_kkdcp_message_1,
    &k5_atype_kkdcp_message_2
};
DEFSEQTYPE(kkdcp_message, krb5_kkdcp_message,
           kkdcp_message_fields);
MAKE_ENCODER(encode_krb5_kkdcp_message, kkdcp_message);
MAKE_DECODER(decode_krb5_kkdcp_message, kkdcp_message);

DEFFIELD(vmac_0, krb5_verifier_mac, princ, 0, opt_principal);
DEFFIELD(vmac_1, krb5_verifier_mac, kvno, 1, opt_kvno);
DEFFIELD(vmac_2, krb5_verifier_mac, enctype, 2, opt_int32);
DEFFIELD(vmac_3, krb5_verifier_mac, checksum, 3, checksum);
static const struct atype_info *vmac_fields[] = {
    &k5_atype_vmac_0, &k5_atype_vmac_1, &k5_atype_vmac_2, &k5_atype_vmac_3
};
DEFSEQTYPE(vmac, krb5_verifier_mac, vmac_fields);
DEFPTRTYPE(vmac_ptr, vmac);
DEFOPTIONALZEROTYPE(opt_vmac_ptr, vmac_ptr);
DEFNONEMPTYNULLTERMSEQOFTYPE(vmacs, vmac_ptr);
DEFPTRTYPE(vmacs_ptr, vmacs);
DEFOPTIONALEMPTYTYPE(opt_vmacs_ptr, vmacs_ptr);

DEFFIELD(cammac_0, krb5_cammac, elements, 0, auth_data_ptr);
DEFFIELD(cammac_1, krb5_cammac, kdc_verifier, 1, opt_vmac_ptr);
DEFFIELD(cammac_2, krb5_cammac, svc_verifier, 2, opt_vmac_ptr);
DEFFIELD(cammac_3, krb5_cammac, other_verifiers, 3, opt_vmacs_ptr);
static const struct atype_info *cammac_fields[] = {
    &k5_atype_cammac_0, &k5_atype_cammac_1, &k5_atype_cammac_2,
    &k5_atype_cammac_3
};
DEFSEQTYPE(cammac, krb5_cammac, cammac_fields);

MAKE_ENCODER(encode_krb5_cammac, cammac);
MAKE_DECODER(decode_krb5_cammac, cammac);

MAKE_ENCODER(encode_utf8_strings, seqof_utf8_data);
MAKE_DECODER(decode_utf8_strings, seqof_utf8_data);

/*
 * SecureCookie ::= SEQUENCE {
 *     time     INTEGER,
 *     data     SEQUENCE OF PA-DATA,
 *     ...
 * }
 */
DEFINTTYPE(inttime, time_t);
DEFOFFSETTYPE(secure_cookie_0, krb5_secure_cookie, time, inttime);
DEFOFFSETTYPE(secure_cookie_1, krb5_secure_cookie, data, ptr_seqof_pa_data);
static const struct atype_info *secure_cookie_fields[] = {
    &k5_atype_secure_cookie_0, &k5_atype_secure_cookie_1
};
DEFSEQTYPE(secure_cookie, krb5_secure_cookie, secure_cookie_fields);
MAKE_ENCODER(encode_krb5_secure_cookie, secure_cookie);
MAKE_DECODER(decode_krb5_secure_cookie, secure_cookie);
