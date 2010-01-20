/*
 * Copyright (c) 1997 - 2000 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "krb5/gsskrb5_locl.h"
#include <gssapi_mech.h>

RCSID("$Id: external.c 22128 2007-12-04 00:56:55Z lha $");

/*
 * The implementation must reserve static storage for a
 * gss_OID_desc object containing the value
 * {10, (void *)"\x2a\x86\x48\x86\xf7\x12"
 *              "\x01\x02\x01\x01"},
 * corresponding to an object-identifier value of
 * {iso(1) member-body(2) United States(840) mit(113554)
 *  infosys(1) gssapi(2) generic(1) user_name(1)}.  The constant
 * GSS_C_NT_USER_NAME should be initialized to point
 * to that gss_OID_desc.
 */

static gss_OID_desc gss_c_nt_user_name_oid_desc =
{10, rk_UNCONST("\x2a\x86\x48\x86\xf7\x12" "\x01\x02\x01\x01")};

gss_OID GSS_C_NT_USER_NAME = &gss_c_nt_user_name_oid_desc;

/*
 * The implementation must reserve static storage for a
 * gss_OID_desc object containing the value
 * {10, (void *)"\x2a\x86\x48\x86\xf7\x12"
 *              "\x01\x02\x01\x02"},
 * corresponding to an object-identifier value of
 * {iso(1) member-body(2) United States(840) mit(113554)
 *  infosys(1) gssapi(2) generic(1) machine_uid_name(2)}.
 * The constant GSS_C_NT_MACHINE_UID_NAME should be
 * initialized to point to that gss_OID_desc.
 */

static gss_OID_desc gss_c_nt_machine_uid_name_oid_desc =
{10, rk_UNCONST("\x2a\x86\x48\x86\xf7\x12" "\x01\x02\x01\x02")};

gss_OID GSS_C_NT_MACHINE_UID_NAME = &gss_c_nt_machine_uid_name_oid_desc;

/*
 * The implementation must reserve static storage for a
 * gss_OID_desc object containing the value
 * {10, (void *)"\x2a\x86\x48\x86\xf7\x12"
 *              "\x01\x02\x01\x03"},
 * corresponding to an object-identifier value of
 * {iso(1) member-body(2) United States(840) mit(113554)
 *  infosys(1) gssapi(2) generic(1) string_uid_name(3)}.
 * The constant GSS_C_NT_STRING_UID_NAME should be
 * initialized to point to that gss_OID_desc.
 */

static gss_OID_desc gss_c_nt_string_uid_name_oid_desc =
{10, rk_UNCONST("\x2a\x86\x48\x86\xf7\x12" "\x01\x02\x01\x03")};

gss_OID GSS_C_NT_STRING_UID_NAME = &gss_c_nt_string_uid_name_oid_desc;

/*
 * The implementation must reserve static storage for a
 * gss_OID_desc object containing the value
 * {6, (void *)"\x2b\x06\x01\x05\x06\x02"},
 * corresponding to an object-identifier value of
 * {iso(1) org(3) dod(6) internet(1) security(5)
 * nametypes(6) gss-host-based-services(2)).  The constant
 * GSS_C_NT_HOSTBASED_SERVICE_X should be initialized to point
 * to that gss_OID_desc.  This is a deprecated OID value, and
 * implementations wishing to support hostbased-service names
 * should instead use the GSS_C_NT_HOSTBASED_SERVICE OID,
 * defined below, to identify such names;
 * GSS_C_NT_HOSTBASED_SERVICE_X should be accepted a synonym
 * for GSS_C_NT_HOSTBASED_SERVICE when presented as an input
 * parameter, but should not be emitted by GSS-API
 * implementations
 */

static gss_OID_desc gss_c_nt_hostbased_service_x_oid_desc =
{6, rk_UNCONST("\x2b\x06\x01\x05\x06\x02")};

gss_OID GSS_C_NT_HOSTBASED_SERVICE_X = &gss_c_nt_hostbased_service_x_oid_desc;

/*
 * The implementation must reserve static storage for a
 * gss_OID_desc object containing the value
 * {10, (void *)"\x2a\x86\x48\x86\xf7\x12"
 *              "\x01\x02\x01\x04"}, corresponding to an
 * object-identifier value of {iso(1) member-body(2)
 * Unites States(840) mit(113554) infosys(1) gssapi(2)
 * generic(1) service_name(4)}.  The constant
 * GSS_C_NT_HOSTBASED_SERVICE should be initialized
 * to point to that gss_OID_desc.
 */
static gss_OID_desc gss_c_nt_hostbased_service_oid_desc =
{10, rk_UNCONST("\x2a\x86\x48\x86\xf7\x12" "\x01\x02\x01\x04")};

gss_OID GSS_C_NT_HOSTBASED_SERVICE = &gss_c_nt_hostbased_service_oid_desc;

/*
 * The implementation must reserve static storage for a
 * gss_OID_desc object containing the value
 * {6, (void *)"\x2b\x06\01\x05\x06\x03"},
 * corresponding to an object identifier value of
 * {1(iso), 3(org), 6(dod), 1(internet), 5(security),
 * 6(nametypes), 3(gss-anonymous-name)}.  The constant
 * and GSS_C_NT_ANONYMOUS should be initialized to point
 * to that gss_OID_desc.
 */

static gss_OID_desc gss_c_nt_anonymous_oid_desc =
{6, rk_UNCONST("\x2b\x06\01\x05\x06\x03")};

gss_OID GSS_C_NT_ANONYMOUS = &gss_c_nt_anonymous_oid_desc;

/*
 * The implementation must reserve static storage for a
 * gss_OID_desc object containing the value
 * {6, (void *)"\x2b\x06\x01\x05\x06\x04"},
 * corresponding to an object-identifier value of
 * {1(iso), 3(org), 6(dod), 1(internet), 5(security),
 * 6(nametypes), 4(gss-api-exported-name)}.  The constant
 * GSS_C_NT_EXPORT_NAME should be initialized to point
 * to that gss_OID_desc.
 */

static gss_OID_desc gss_c_nt_export_name_oid_desc =
{6, rk_UNCONST("\x2b\x06\x01\x05\x06\x04") };

gss_OID GSS_C_NT_EXPORT_NAME = &gss_c_nt_export_name_oid_desc;

/*
 *   This name form shall be represented by the Object Identifier {iso(1)
 *   member-body(2) United States(840) mit(113554) infosys(1) gssapi(2)
 *   krb5(2) krb5_name(1)}.  The recommended symbolic name for this type
 *   is "GSS_KRB5_NT_PRINCIPAL_NAME".
 */

static gss_OID_desc gss_krb5_nt_principal_name_oid_desc =
{10, rk_UNCONST("\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x01") };

gss_OID GSS_KRB5_NT_PRINCIPAL_NAME = &gss_krb5_nt_principal_name_oid_desc;

/*
 *   This name form shall be represented by the Object Identifier {iso(1)
 *   member-body(2) United States(840) mit(113554) infosys(1) gssapi(2)
 *   generic(1) user_name(1)}.  The recommended symbolic name for this
 *   type is "GSS_KRB5_NT_USER_NAME".
 */

gss_OID GSS_KRB5_NT_USER_NAME = &gss_c_nt_user_name_oid_desc;

/*
 *   This name form shall be represented by the Object Identifier {iso(1)
 *   member-body(2) United States(840) mit(113554) infosys(1) gssapi(2)
 *   generic(1) machine_uid_name(2)}.  The recommended symbolic name for
 *   this type is "GSS_KRB5_NT_MACHINE_UID_NAME".
 */

gss_OID GSS_KRB5_NT_MACHINE_UID_NAME = &gss_c_nt_machine_uid_name_oid_desc;

/*
 *   This name form shall be represented by the Object Identifier {iso(1)
 *   member-body(2) United States(840) mit(113554) infosys(1) gssapi(2)
 *   generic(1) string_uid_name(3)}.  The recommended symbolic name for
 *   this type is "GSS_KRB5_NT_STRING_UID_NAME".
 */

gss_OID GSS_KRB5_NT_STRING_UID_NAME = &gss_c_nt_string_uid_name_oid_desc;

/*
 *   To support ongoing experimentation, testing, and evolution of the
 *   specification, the Kerberos V5 GSS-API mechanism as defined in this
 *   and any successor memos will be identified with the following Object
 *   Identifier, as defined in RFC-1510, until the specification is
 *   advanced to the level of Proposed Standard RFC:
 *
 *   {iso(1), org(3), dod(5), internet(1), security(5), kerberosv5(2)}
 *
 *   Upon advancement to the level of Proposed Standard RFC, the Kerberos
 *   V5 GSS-API mechanism will be identified by an Object Identifier
 *   having the value:
 *
 *   {iso(1) member-body(2) United States(840) mit(113554) infosys(1)
 *   gssapi(2) krb5(2)}
 */

#if 0 /* This is the old OID */

static gss_OID_desc gss_krb5_mechanism_oid_desc =
{5, rk_UNCONST("\x2b\x05\x01\x05\x02")};

#endif

static gss_OID_desc gss_krb5_mechanism_oid_desc =
{9, rk_UNCONST("\x2a\x86\x48\x86\xf7\x12\x01\x02\x02") };

gss_OID GSS_KRB5_MECHANISM = &gss_krb5_mechanism_oid_desc;

/*
 * draft-ietf-cat-iakerb-09, IAKERB:
 *   The mechanism ID for IAKERB proxy GSS-API Kerberos, in accordance
 *   with the mechanism proposed by SPNEGO [7] for negotiating protocol
 *   variations, is:  {iso(1) org(3) dod(6) internet(1) security(5)
 *   mechanisms(5) iakerb(10) iakerbProxyProtocol(1)}.  The proposed
 *   mechanism ID for IAKERB minimum messages GSS-API Kerberos, in
 *   accordance with the mechanism proposed by SPNEGO for negotiating
 *   protocol variations, is: {iso(1) org(3) dod(6) internet(1)
 *   security(5) mechanisms(5) iakerb(10)
 *   iakerbMinimumMessagesProtocol(2)}.
 */

static gss_OID_desc gss_iakerb_proxy_mechanism_oid_desc =
{7, rk_UNCONST("\x2b\x06\x01\x05\x05\x0a\x01")};

gss_OID GSS_IAKERB_PROXY_MECHANISM = &gss_iakerb_proxy_mechanism_oid_desc;

static gss_OID_desc gss_iakerb_min_msg_mechanism_oid_desc =
{7, rk_UNCONST("\x2b\x06\x01\x05\x05\x0a\x02") };

gss_OID GSS_IAKERB_MIN_MSG_MECHANISM = &gss_iakerb_min_msg_mechanism_oid_desc;

/*
 *
 */

static gss_OID_desc gss_c_peer_has_updated_spnego_oid_desc =
{9, (void *)"\x2b\x06\x01\x04\x01\xa9\x4a\x13\x05"};

gss_OID GSS_C_PEER_HAS_UPDATED_SPNEGO = &gss_c_peer_has_updated_spnego_oid_desc;

/*
 * 1.2.752.43.13 Heimdal GSS-API Extentions
 */

/* 1.2.752.43.13.1 */
static gss_OID_desc gss_krb5_copy_ccache_x_oid_desc =
{6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x01")};

gss_OID GSS_KRB5_COPY_CCACHE_X = &gss_krb5_copy_ccache_x_oid_desc;

/* 1.2.752.43.13.2 */
static gss_OID_desc gss_krb5_get_tkt_flags_x_oid_desc =
{6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x02")};

gss_OID GSS_KRB5_GET_TKT_FLAGS_X = &gss_krb5_get_tkt_flags_x_oid_desc;

/* 1.2.752.43.13.3 */
static gss_OID_desc gss_krb5_extract_authz_data_from_sec_context_x_oid_desc =
{6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x03")};

gss_OID GSS_KRB5_EXTRACT_AUTHZ_DATA_FROM_SEC_CONTEXT_X = &gss_krb5_extract_authz_data_from_sec_context_x_oid_desc;

/* 1.2.752.43.13.4 */
static gss_OID_desc gss_krb5_compat_des3_mic_x_oid_desc =
{6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x04")};

gss_OID GSS_KRB5_COMPAT_DES3_MIC_X = &gss_krb5_compat_des3_mic_x_oid_desc;

/* 1.2.752.43.13.5 */
static gss_OID_desc gss_krb5_register_acceptor_identity_x_desc =
{6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x05")};

gss_OID GSS_KRB5_REGISTER_ACCEPTOR_IDENTITY_X = &gss_krb5_register_acceptor_identity_x_desc;

/* 1.2.752.43.13.6 */
static gss_OID_desc gss_krb5_export_lucid_context_x_desc =
{6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x06")};

gss_OID GSS_KRB5_EXPORT_LUCID_CONTEXT_X = &gss_krb5_export_lucid_context_x_desc;

/* 1.2.752.43.13.6.1 */
static gss_OID_desc gss_krb5_export_lucid_context_v1_x_desc =
{7, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x06\x01")};

gss_OID GSS_KRB5_EXPORT_LUCID_CONTEXT_V1_X = &gss_krb5_export_lucid_context_v1_x_desc;

/* 1.2.752.43.13.7 */
static gss_OID_desc gss_krb5_set_dns_canonicalize_x_desc =
{6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x07")};

gss_OID GSS_KRB5_SET_DNS_CANONICALIZE_X = &gss_krb5_set_dns_canonicalize_x_desc;

/* 1.2.752.43.13.8 */
static gss_OID_desc gss_krb5_get_subkey_x_desc =
{6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x08")};

gss_OID GSS_KRB5_GET_SUBKEY_X = &gss_krb5_get_subkey_x_desc;

/* 1.2.752.43.13.9 */
static gss_OID_desc gss_krb5_get_initiator_subkey_x_desc =
{6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x09")};

gss_OID GSS_KRB5_GET_INITIATOR_SUBKEY_X = &gss_krb5_get_initiator_subkey_x_desc;

/* 1.2.752.43.13.10 */
static gss_OID_desc gss_krb5_get_acceptor_subkey_x_desc =
{6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x0a")};

gss_OID GSS_KRB5_GET_ACCEPTOR_SUBKEY_X = &gss_krb5_get_acceptor_subkey_x_desc;

/* 1.2.752.43.13.11 */
static gss_OID_desc gss_krb5_send_to_kdc_x_desc =
{6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x0b")};

gss_OID GSS_KRB5_SEND_TO_KDC_X = &gss_krb5_send_to_kdc_x_desc;

/* 1.2.752.43.13.12 */
static gss_OID_desc gss_krb5_get_authtime_x_desc =
{6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x0c")};

gss_OID GSS_KRB5_GET_AUTHTIME_X = &gss_krb5_get_authtime_x_desc;

/* 1.2.752.43.13.13 */
static gss_OID_desc gss_krb5_get_service_keyblock_x_desc =
{6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x0d")};

gss_OID GSS_KRB5_GET_SERVICE_KEYBLOCK_X = &gss_krb5_get_service_keyblock_x_desc;

/* 1.2.752.43.13.14 */
static gss_OID_desc gss_krb5_set_allowable_enctypes_x_desc =
{6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x0e")};

gss_OID GSS_KRB5_SET_ALLOWABLE_ENCTYPES_X = &gss_krb5_set_allowable_enctypes_x_desc;

/* 1.2.752.43.13.15 */
static gss_OID_desc gss_krb5_set_default_realm_x_desc =
{6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x0f")};

gss_OID GSS_KRB5_SET_DEFAULT_REALM_X = &gss_krb5_set_default_realm_x_desc;

/* 1.2.752.43.13.16 */
static gss_OID_desc gss_krb5_ccache_name_x_desc =
{6, rk_UNCONST("\x2a\x85\x70\x2b\x0d\x10")};

gss_OID GSS_KRB5_CCACHE_NAME_X = &gss_krb5_ccache_name_x_desc;

/* 1.2.752.43.14.1 */
static gss_OID_desc gss_sasl_digest_md5_mechanism_desc =
{6, rk_UNCONST("\x2a\x85\x70\x2b\x0e\x01") };

gss_OID GSS_SASL_DIGEST_MD5_MECHANISM = &gss_sasl_digest_md5_mechanism_desc;

/*
 * Context for krb5 calls.
 */

/*
 *
 */

static gssapi_mech_interface_desc krb5_mech = {
    GMI_VERSION,
    "kerberos 5",
    {9, "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02" },
    _gsskrb5_acquire_cred,
    _gsskrb5_release_cred,
    _gsskrb5_init_sec_context,
    _gsskrb5_accept_sec_context,
    _gsskrb5_process_context_token,
    _gsskrb5_delete_sec_context,
    _gsskrb5_context_time,
    _gsskrb5_get_mic,
    _gsskrb5_verify_mic,
    _gsskrb5_wrap,
    _gsskrb5_unwrap,
    _gsskrb5_display_status,
    _gsskrb5_indicate_mechs,
    _gsskrb5_compare_name,
    _gsskrb5_display_name,
    _gsskrb5_import_name,
    _gsskrb5_export_name,
    _gsskrb5_release_name,
    _gsskrb5_inquire_cred,
    _gsskrb5_inquire_context,
    _gsskrb5_wrap_size_limit,
    _gsskrb5_add_cred,
    _gsskrb5_inquire_cred_by_mech,
    _gsskrb5_export_sec_context,
    _gsskrb5_import_sec_context,
    _gsskrb5_inquire_names_for_mech,
    _gsskrb5_inquire_mechs_for_name,
    _gsskrb5_canonicalize_name,
    _gsskrb5_duplicate_name,
    _gsskrb5_inquire_sec_context_by_oid,
    _gsskrb5_inquire_cred_by_oid,
    _gsskrb5_set_sec_context_option,
    _gsskrb5_set_cred_option,
    _gsskrb5_pseudo_random
};

gssapi_mech_interface
__gss_krb5_initialize(void)
{
    return &krb5_mech;
}
