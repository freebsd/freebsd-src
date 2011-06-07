/*
 * Copyright (c) 1997 - 2006 Kungliga Tekniska Högskolan
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

/* $Id: gssapi_krb5.h 20385 2007-04-18 08:51:32Z lha $ */

#ifndef GSSAPI_KRB5_H_
#define GSSAPI_KRB5_H_

#include <gssapi/gssapi.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This is for kerberos5 names.
 */

extern gss_OID GSS_KRB5_NT_PRINCIPAL_NAME;
extern gss_OID GSS_KRB5_NT_USER_NAME;
extern gss_OID GSS_KRB5_NT_MACHINE_UID_NAME;
extern gss_OID GSS_KRB5_NT_STRING_UID_NAME;

extern gss_OID GSS_KRB5_MECHANISM;

/* for compatibility with MIT api */

#define gss_mech_krb5 GSS_KRB5_MECHANISM
#define gss_krb5_nt_general_name GSS_KRB5_NT_PRINCIPAL_NAME

/* Extensions set contexts options */
extern gss_OID GSS_KRB5_COPY_CCACHE_X;
extern gss_OID GSS_KRB5_COMPAT_DES3_MIC_X;
extern gss_OID GSS_KRB5_REGISTER_ACCEPTOR_IDENTITY_X;
extern gss_OID GSS_KRB5_SET_DNS_CANONICALIZE_X;
extern gss_OID GSS_KRB5_SEND_TO_KDC_X;
extern gss_OID GSS_KRB5_SET_DEFAULT_REALM_X;
extern gss_OID GSS_KRB5_CCACHE_NAME_X;
/* Extensions inquire context */
extern gss_OID GSS_KRB5_GET_TKT_FLAGS_X;
extern gss_OID GSS_KRB5_EXTRACT_AUTHZ_DATA_FROM_SEC_CONTEXT_X;
extern gss_OID GSS_C_PEER_HAS_UPDATED_SPNEGO;
extern gss_OID GSS_KRB5_EXPORT_LUCID_CONTEXT_X;
extern gss_OID GSS_KRB5_EXPORT_LUCID_CONTEXT_V1_X;
extern gss_OID GSS_KRB5_GET_SUBKEY_X;
extern gss_OID GSS_KRB5_GET_INITIATOR_SUBKEY_X;
extern gss_OID GSS_KRB5_GET_ACCEPTOR_SUBKEY_X;
extern gss_OID GSS_KRB5_GET_AUTHTIME_X;
extern gss_OID GSS_KRB5_GET_SERVICE_KEYBLOCK_X;
/* Extensions creds */
extern gss_OID GSS_KRB5_IMPORT_CRED_X;
extern gss_OID GSS_KRB5_SET_ALLOWABLE_ENCTYPES_X;

/*
 * kerberos mechanism specific functions
 */

struct krb5_keytab_data;
struct krb5_ccache_data;
struct Principal;

OM_uint32
gss_krb5_ccache_name(OM_uint32 * /*minor_status*/, 
		     const char * /*name */,
		     const char ** /*out_name */);

OM_uint32 gsskrb5_register_acceptor_identity
        (const char */*identity*/);

OM_uint32 gss_krb5_copy_ccache
	(OM_uint32 */*minor*/,
	 gss_cred_id_t /*cred*/,
	 struct krb5_ccache_data */*out*/);

OM_uint32
gss_krb5_import_cred(OM_uint32 */*minor*/,
		     struct krb5_ccache_data * /*in*/,
		     struct Principal * /*keytab_principal*/,
		     struct krb5_keytab_data * /*keytab*/,
		     gss_cred_id_t */*out*/);

OM_uint32 gss_krb5_get_tkt_flags
	(OM_uint32 */*minor*/,
	 gss_ctx_id_t /*context_handle*/,
	 OM_uint32 */*tkt_flags*/);

OM_uint32
gsskrb5_extract_authz_data_from_sec_context
	(OM_uint32 * /*minor_status*/,
	 gss_ctx_id_t /*context_handle*/,
	 int /*ad_type*/,
	 gss_buffer_t /*ad_data*/);

OM_uint32
gsskrb5_set_dns_canonicalize(int);

struct gsskrb5_send_to_kdc {
    void *func;
    void *ptr;
};

OM_uint32
gsskrb5_set_send_to_kdc(struct gsskrb5_send_to_kdc *);

OM_uint32
gsskrb5_set_default_realm(const char *);

OM_uint32
gsskrb5_extract_authtime_from_sec_context(OM_uint32 *, gss_ctx_id_t, time_t *);

struct EncryptionKey;

OM_uint32 
gsskrb5_extract_service_keyblock(OM_uint32 *minor_status,
				 gss_ctx_id_t context_handle,
				 struct EncryptionKey **out);
OM_uint32 
gsskrb5_get_initiator_subkey(OM_uint32 *minor_status,
				 gss_ctx_id_t context_handle,
				 struct EncryptionKey **out);
OM_uint32 
gsskrb5_get_subkey(OM_uint32 *minor_status,
		   gss_ctx_id_t context_handle,
		   struct EncryptionKey **out);

/*
 * Lucid - NFSv4 interface to GSS-API KRB5 to expose key material to
 * do GSS content token handling in-kernel.
 */

typedef struct gss_krb5_lucid_key {
	OM_uint32	type;
	OM_uint32	length;
	void *		data;
} gss_krb5_lucid_key_t;

typedef struct gss_krb5_rfc1964_keydata {
	OM_uint32		sign_alg;
	OM_uint32		seal_alg;
	gss_krb5_lucid_key_t	ctx_key;
} gss_krb5_rfc1964_keydata_t;

typedef struct gss_krb5_cfx_keydata {
	OM_uint32		have_acceptor_subkey;
	gss_krb5_lucid_key_t	ctx_key;
	gss_krb5_lucid_key_t	acceptor_subkey;
} gss_krb5_cfx_keydata_t;

typedef struct gss_krb5_lucid_context_v1 {
	OM_uint32	version;
	OM_uint32	initiate;
	OM_uint32	endtime;
	OM_uint64	send_seq;
	OM_uint64	recv_seq;
	OM_uint32	protocol;
	gss_krb5_rfc1964_keydata_t rfc1964_kd;
	gss_krb5_cfx_keydata_t	   cfx_kd;
} gss_krb5_lucid_context_v1_t;

typedef struct gss_krb5_lucid_context_version {
	OM_uint32	version;	/* Structure version number */
} gss_krb5_lucid_context_version_t;

/*
 * Function declarations
 */

OM_uint32
gss_krb5_export_lucid_sec_context(OM_uint32 *minor_status,
				  gss_ctx_id_t *context_handle,
				  OM_uint32 version,
				  void **kctx);


OM_uint32
gss_krb5_free_lucid_sec_context(OM_uint32 *minor_status,
				void *kctx);


OM_uint32
gss_krb5_set_allowable_enctypes(OM_uint32 *minor_status, 
				gss_cred_id_t cred,
				OM_uint32 num_enctypes,
				int32_t *enctypes);

#ifdef __cplusplus
}
#endif

#endif /* GSSAPI_SPNEGO_H_ */
