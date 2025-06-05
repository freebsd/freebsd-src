/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ... copyright ... */

/*
 * Novell key-format scheme:
 *
 * KrbKeySet ::= SEQUENCE {
 * attribute-major-vno       [0] UInt16,
 * attribute-minor-vno       [1] UInt16,
 * kvno                      [2] UInt32,
 * mkvno                     [3] UInt32 OPTIONAL,
 * keys                      [4] SEQUENCE OF KrbKey,
 * ...
 * }
 *
 * KrbKey ::= SEQUENCE {
 * salt      [0] KrbSalt OPTIONAL,
 * key       [1] EncryptionKey,
 * s2kparams [2] OCTET STRING OPTIONAL,
 *  ...
 * }
 *
 * KrbSalt ::= SEQUENCE {
 * type      [0] Int32,
 * salt      [1] OCTET STRING OPTIONAL
 * }
 *
 * EncryptionKey ::= SEQUENCE {
 * keytype   [0] Int32,
 * keyvalue  [1] OCTET STRING
 * }
 *
 */

#include <k5-int.h>
#include <kdb.h>

#include "krbasn1.h"
#include "asn1_encode.h"

#ifdef ENABLE_LDAP

/************************************************************************/
/* Encode the Principal's keys                                          */
/************************************************************************/

/*
 * Imports from asn1_k_encode.c.
 * XXX Must be manually synchronized for now.
 */
IMPORT_TYPE(int32, int32_t);

DEFINTTYPE(int16, int16_t);
DEFINTTYPE(uint16, uint16_t);

DEFCOUNTEDSTRINGTYPE(ui2_octetstring, uint8_t *, uint16_t,
                     k5_asn1_encode_bytestring, k5_asn1_decode_bytestring,
                     ASN1_OCTETSTRING);

static int
is_value_present(const void *p)
{
    const krb5_key_data *val = p;
    return (val->key_data_length[1] != 0);
}
DEFCOUNTEDTYPE(krbsalt_salt, krb5_key_data, key_data_contents[1],
               key_data_length[1], ui2_octetstring);
DEFOPTIONALTYPE(krbsalt_salt_if_present, is_value_present, NULL, krbsalt_salt);
DEFFIELD(krbsalt_0, krb5_key_data, key_data_type[1], 0, int16);
DEFCTAGGEDTYPE(krbsalt_1, 1, krbsalt_salt_if_present);
static const struct atype_info *krbsalt_fields[] = {
    &k5_atype_krbsalt_0, &k5_atype_krbsalt_1
};
DEFSEQTYPE(krbsalt, krb5_key_data, krbsalt_fields);

DEFFIELD(encryptionkey_0, krb5_key_data, key_data_type[0], 0, int16);
DEFCNFIELD(encryptionkey_1, krb5_key_data, key_data_contents[0],
           key_data_length[0], 1, ui2_octetstring);
static const struct atype_info *encryptionkey_fields[] = {
    &k5_atype_encryptionkey_0, &k5_atype_encryptionkey_1
};
DEFSEQTYPE(encryptionkey, krb5_key_data, encryptionkey_fields);

static int
is_salt_present(const void *p)
{
    const krb5_key_data *val = p;
    return val->key_data_ver > 1;
}
static void
no_salt(void *p)
{
    krb5_key_data *val = p;
    val->key_data_ver = 1;
}
DEFOPTIONALTYPE(key_data_salt_if_present, is_salt_present, no_salt, krbsalt);
DEFCTAGGEDTYPE(key_data_0, 0, key_data_salt_if_present);
DEFCTAGGEDTYPE(key_data_1, 1, encryptionkey);
static const struct atype_info *key_data_fields[] = {
    &k5_atype_key_data_0, &k5_atype_key_data_1
};
DEFSEQTYPE(key_data, krb5_key_data, key_data_fields);
DEFPTRTYPE(ptr_key_data, key_data);
DEFCOUNTEDSEQOFTYPE(cseqof_key_data, int16_t, ptr_key_data);

DEFINT_IMMEDIATE(one, 1, ASN1_BAD_FORMAT);
DEFCTAGGEDTYPE(ldap_key_seq_0, 0, one);
DEFCTAGGEDTYPE(ldap_key_seq_1, 1, one);
DEFFIELD(ldap_key_seq_2, ldap_seqof_key_data, kvno, 2, uint16);
DEFFIELD(ldap_key_seq_3, ldap_seqof_key_data, mkvno, 3, int32);
DEFCNFIELD(ldap_key_seq_4, ldap_seqof_key_data, key_data, n_key_data, 4,
           cseqof_key_data);
static const struct atype_info *ldap_key_seq_fields[] = {
    &k5_atype_ldap_key_seq_0, &k5_atype_ldap_key_seq_1,
    &k5_atype_ldap_key_seq_2, &k5_atype_ldap_key_seq_3,
    &k5_atype_ldap_key_seq_4
};
DEFSEQTYPE(ldap_key_seq, ldap_seqof_key_data, ldap_key_seq_fields);

/* Export a function to do the whole encoding.  */
MAKE_ENCODER(krb5int_ldap_encode_sequence_of_keys, ldap_key_seq);
MAKE_DECODER(krb5int_ldap_decode_sequence_of_keys, ldap_key_seq);

#endif
