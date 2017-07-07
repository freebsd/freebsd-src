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

#ifndef _PKINIT_ACCESSOR_H
#define _PKINIT_ACCESSOR_H

/*
 * Function prototypes
 */
krb5_error_code pkinit_accessor_init(void);

#define DEF_EXT_FUNC_PTRS(type) \
extern krb5_error_code (*k5int_encode_##type)(const type *, krb5_data **); \
extern krb5_error_code (*k5int_decode_##type)(const krb5_data *, type **)

#define DEF_EXT_FUNC_PTRS_ARRAY(type) \
extern krb5_error_code (*k5int_encode_##type)(const type **, krb5_data **); \
extern krb5_error_code (*k5int_decode_##type)(const krb5_data *, type ***)

DEF_EXT_FUNC_PTRS(krb5_auth_pack);
DEF_EXT_FUNC_PTRS(krb5_auth_pack_draft9);
DEF_EXT_FUNC_PTRS(krb5_kdc_dh_key_info);
DEF_EXT_FUNC_PTRS(krb5_pa_pk_as_rep);
DEF_EXT_FUNC_PTRS(krb5_pa_pk_as_req);
DEF_EXT_FUNC_PTRS(krb5_pa_pk_as_req_draft9);
DEF_EXT_FUNC_PTRS(krb5_reply_key_pack);
DEF_EXT_FUNC_PTRS(krb5_reply_key_pack_draft9);

/* special cases... */
extern krb5_error_code (*k5int_decode_krb5_principal_name)
	(const krb5_data *, krb5_principal_data **);

extern krb5_error_code (*k5int_encode_krb5_pa_pk_as_rep_draft9)
	(const krb5_pa_pk_as_rep_draft9 *, krb5_data **code);

extern krb5_error_code (*k5int_encode_krb5_td_dh_parameters)
	(krb5_algorithm_identifier *const *, krb5_data **code);
extern krb5_error_code (*k5int_decode_krb5_td_dh_parameters)
	(const krb5_data *, krb5_algorithm_identifier ***);

extern krb5_error_code (*k5int_encode_krb5_td_trusted_certifiers)
	(krb5_external_principal_identifier *const *, krb5_data **code);
extern krb5_error_code (*k5int_decode_krb5_td_trusted_certifiers)
	(const krb5_data *, krb5_external_principal_identifier ***);

extern krb5_error_code (*k5int_encode_krb5_kdc_req_body)
	(const krb5_kdc_req *rep, krb5_data **code);
extern void KRB5_CALLCONV (*k5int_krb5_free_kdc_req)
	(krb5_context, krb5_kdc_req * );
extern void (*k5int_set_prompt_types)
	(krb5_context, krb5_prompt_type *);

#endif /* _PKINIT_ACCESSOR_H */
