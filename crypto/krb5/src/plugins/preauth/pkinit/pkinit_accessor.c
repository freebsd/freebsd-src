/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * COPYRIGHT (C) 2006,2007
 * THE REGENTS OF THE UNIVERSITY OF MICHIGAN
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of The University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use of distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION
 * FROM THE UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY
 * PURPOSE, AND WITHOUT WARRANTY BY THE UNIVERSITY OF
 * MICHIGAN OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING
 * WITHOUT LIMITATION THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE
 * REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE LIABLE
 * FOR ANY DAMAGES, INCLUDING SPECIAL, INDIRECT, INCIDENTAL, OR
 * CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 */

#include <k5-int.h>
#include "pkinit_accessor.h"

#define DEF_FUNC_PTRS(type)                                             \
    krb5_error_code (*k5int_encode_##type)(const type *, krb5_data **); \
    krb5_error_code (*k5int_decode_##type)(const krb5_data *, type **)

#define DEF_FUNC_PTRS_ARRAY(type)                                       \
    krb5_error_code (*k5int_encode_##type)(const type **, krb5_data **); \
    krb5_error_code (*k5int_decode_##type)(const krb5_data *, type ***)

DEF_FUNC_PTRS(krb5_auth_pack);
DEF_FUNC_PTRS(krb5_kdc_dh_key_info);
DEF_FUNC_PTRS(krb5_pa_pk_as_rep);
DEF_FUNC_PTRS(krb5_pa_pk_as_req);
DEF_FUNC_PTRS(krb5_reply_key_pack);

/* special cases... */
krb5_error_code
(*k5int_decode_krb5_principal_name)(const krb5_data *, krb5_principal_data **);

krb5_error_code
(*k5int_encode_krb5_td_dh_parameters)(krb5_algorithm_identifier *const *,
                                      krb5_data **code);
krb5_error_code
(*k5int_decode_krb5_td_dh_parameters)(const krb5_data *,
                                      krb5_algorithm_identifier ***);

krb5_error_code
(*k5int_encode_krb5_td_trusted_certifiers)
(krb5_external_principal_identifier *const *, krb5_data **code);

krb5_error_code
(*k5int_decode_krb5_td_trusted_certifiers)
(const krb5_data *,
 krb5_external_principal_identifier ***);

krb5_error_code
(*k5int_encode_krb5_kdc_req_body)(const krb5_kdc_req *rep, krb5_data **code);

void KRB5_CALLCONV
(*k5int_krb5_free_kdc_req)(krb5_context, krb5_kdc_req * );

void
(*k5int_set_prompt_types)(krb5_context, krb5_prompt_type *);


/*
 * Grab internal function pointers from the krb5int_accessor
 * structure and make them available
 */
krb5_error_code
pkinit_accessor_init(void)
{
    krb5_error_code retval;
    krb5int_access k5int;

    retval = krb5int_accessor(&k5int, KRB5INT_ACCESS_VERSION);
    if (retval)
        return retval;
#define SET_PTRS(type)                          \
    k5int_encode_##type = k5int.encode_##type;  \
    k5int_decode_##type = k5int.decode_##type;

    SET_PTRS(krb5_auth_pack);
    SET_PTRS(krb5_kdc_dh_key_info);
    SET_PTRS(krb5_pa_pk_as_rep);
    SET_PTRS(krb5_pa_pk_as_req);
    SET_PTRS(krb5_reply_key_pack);
    SET_PTRS(krb5_td_dh_parameters);
    SET_PTRS(krb5_td_trusted_certifiers);

    /* special cases... */
    k5int_decode_krb5_principal_name = k5int.decode_krb5_principal_name;
    k5int_encode_krb5_kdc_req_body = k5int.encode_krb5_kdc_req_body;
    k5int_krb5_free_kdc_req = k5int.free_kdc_req;
    k5int_set_prompt_types = k5int.set_prompt_types;
    return 0;
}
