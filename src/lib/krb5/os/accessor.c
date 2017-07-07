/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/accessor.c */
/*
 * Copyright 1990, 2008 by the Massachusetts Institute of Technology.
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
#include "os-proto.h"
#include "../krb/int-proto.h"

/* If this trick gets used elsewhere, move it to k5-platform.h.  */
#ifndef DESIGNATED_INITIALIZERS
#define DESIGNATED_INITIALIZERS                         \
    /* ANSI/ISO C 1999 supports this...  */             \
    (__STDC_VERSION__ >= 199901L                        \
     /* ...as does GCC, since version 2.something.  */  \
     || (!defined __cplusplus && __GNUC__ >= 3))
#endif

krb5_error_code KRB5_CALLCONV
krb5int_accessor(krb5int_access *internals, krb5_int32 version)
{
    if (version == KRB5INT_ACCESS_VERSION) {
#if DESIGNATED_INITIALIZERS
#define S(FIELD, VAL)   .FIELD = VAL
#if defined __GNUC__ && __STDC_VERSION__ < 199901L
        __extension__
#endif
            static const krb5int_access internals_temp = {
#else
#define S(FIELD, VAL)   internals_temp.FIELD = VAL
            krb5int_access internals_temp;
#endif
            S (auth_con_get_subkey_enctype, krb5_auth_con_get_subkey_enctype),

            S (clean_hostname, k5_clean_hostname),

#ifndef LEAN_CLIENT
#define SC(FIELD, VAL)  S(FIELD, VAL)
#else /* disable */
#define SC(FIELD, VAL)  S(FIELD, 0)
#endif
            SC (ser_pack_int64, krb5_ser_pack_int64),
            SC (ser_unpack_int64, krb5_ser_unpack_int64),
#undef SC

#ifdef ENABLE_LDAP
#define SC(FIELD, VAL)  S(FIELD, VAL)
#else
#define SC(FIELD, VAL)  S(FIELD, 0)
#endif
            SC (asn1_ldap_encode_sequence_of_keys, krb5int_ldap_encode_sequence_of_keys),
            SC (asn1_ldap_decode_sequence_of_keys, krb5int_ldap_decode_sequence_of_keys),
#undef SC

#ifndef DISABLE_PKINIT
#define SC(FIELD, VAL)  S(FIELD, VAL)
#else /* disable */
#define SC(FIELD, VAL)  S(FIELD, 0)
#endif
            SC (encode_krb5_pa_pk_as_req, encode_krb5_pa_pk_as_req),
            SC (encode_krb5_pa_pk_as_req_draft9, encode_krb5_pa_pk_as_req_draft9),
            SC (encode_krb5_pa_pk_as_rep, encode_krb5_pa_pk_as_rep),
            SC (encode_krb5_pa_pk_as_rep_draft9, encode_krb5_pa_pk_as_rep_draft9),
            SC (encode_krb5_auth_pack, encode_krb5_auth_pack),
            SC (encode_krb5_auth_pack_draft9, encode_krb5_auth_pack_draft9),
            SC (encode_krb5_kdc_dh_key_info, encode_krb5_kdc_dh_key_info),
            SC (encode_krb5_reply_key_pack, encode_krb5_reply_key_pack),
            SC (encode_krb5_reply_key_pack_draft9, encode_krb5_reply_key_pack_draft9),
            SC (encode_krb5_td_trusted_certifiers, encode_krb5_td_trusted_certifiers),
            SC (encode_krb5_td_dh_parameters, encode_krb5_td_dh_parameters),
            SC (decode_krb5_pa_pk_as_req, decode_krb5_pa_pk_as_req),
            SC (decode_krb5_pa_pk_as_req_draft9, decode_krb5_pa_pk_as_req_draft9),
            SC (decode_krb5_pa_pk_as_rep, decode_krb5_pa_pk_as_rep),
            SC (decode_krb5_auth_pack, decode_krb5_auth_pack),
            SC (decode_krb5_auth_pack_draft9, decode_krb5_auth_pack_draft9),
            SC (decode_krb5_kdc_dh_key_info, decode_krb5_kdc_dh_key_info),
            SC (decode_krb5_principal_name, decode_krb5_principal_name),
            SC (decode_krb5_reply_key_pack, decode_krb5_reply_key_pack),
            SC (decode_krb5_reply_key_pack_draft9, decode_krb5_reply_key_pack_draft9),
            SC (decode_krb5_td_trusted_certifiers, decode_krb5_td_trusted_certifiers),
            SC (decode_krb5_td_dh_parameters, decode_krb5_td_dh_parameters),
            SC (encode_krb5_kdc_req_body, encode_krb5_kdc_req_body),
            SC (free_kdc_req, krb5_free_kdc_req),
            SC (set_prompt_types, k5_set_prompt_types),
#undef SC

#if DESIGNATED_INITIALIZERS
        };
#else
        0;
#endif
        *internals = internals_temp;
        return 0;
    }
    return KRB5_OBSOLETE_FN;
}
