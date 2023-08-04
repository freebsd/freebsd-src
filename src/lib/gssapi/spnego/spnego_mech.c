/*
 * Copyright (C) 2006,2008 by the Massachusetts Institute of Technology.
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
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * A module that implements the spnego security mechanism.
 * It is used to negotiate the security mechanism between
 * peers using the GSS-API.  SPNEGO is specified in RFC 4178.
 *
 */
/*
 * Copyright (c) 2006-2008, Novell, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *   * The copyright holder's name is not used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/* #pragma ident	"@(#)spnego_mech.c	1.7	04/09/28 SMI" */

#include	<k5-int.h>
#include	<k5-der.h>
#include	<krb5.h>
#include	<mglueP.h>
#include	"gssapiP_spnego.h"
#include	<gssapi_err_generic.h>


#define HARD_ERROR(v) ((v) != GSS_S_COMPLETE && (v) != GSS_S_CONTINUE_NEEDED)
typedef const gss_OID_desc *gss_OID_const;

/* private routines for spnego_mechanism */
static spnego_token_t make_spnego_token(const char *);
static gss_buffer_desc make_err_msg(const char *);
static int verify_token_header(struct k5input *, gss_OID_const);
static gss_OID get_mech_oid(OM_uint32 *minor_status, struct k5input *);
static gss_buffer_t get_octet_string(struct k5input *);
static gss_OID_set get_mech_set(OM_uint32 *, struct k5input *);
static OM_uint32 get_req_flags(struct k5input *, OM_uint32 *);
static OM_uint32 get_available_mechs(OM_uint32 *, gss_name_t, gss_cred_usage_t,
				     gss_const_key_value_set_t,
				     gss_cred_id_t *, gss_OID_set *,
				     OM_uint32 *);
static OM_uint32 get_negotiable_mechs(OM_uint32 *, spnego_gss_ctx_id_t,
				      spnego_gss_cred_id_t, gss_cred_usage_t);
static void release_spnego_ctx(spnego_gss_ctx_id_t *);
static spnego_gss_ctx_id_t create_spnego_ctx(int);
static int put_mech_set(gss_OID_set mechSet, gss_buffer_t buf);

static OM_uint32
process_mic(OM_uint32 *, gss_buffer_t, spnego_gss_ctx_id_t,
	    gss_buffer_t *, OM_uint32 *, send_token_flag *);
static OM_uint32
handle_mic(OM_uint32 *, gss_buffer_t, int, spnego_gss_ctx_id_t,
	   gss_buffer_t *, OM_uint32 *, send_token_flag *);

static OM_uint32
init_ctx_new(OM_uint32 *, spnego_gss_cred_id_t, send_token_flag *,
	     spnego_gss_ctx_id_t *);
static OM_uint32
init_ctx_nego(OM_uint32 *, spnego_gss_ctx_id_t, OM_uint32, gss_OID,
	      gss_buffer_t *, gss_buffer_t *, send_token_flag *);
static OM_uint32
init_ctx_cont(OM_uint32 *, spnego_gss_ctx_id_t, gss_buffer_t,
	      gss_buffer_t *, gss_buffer_t *,
	      OM_uint32 *, send_token_flag *);
static OM_uint32
init_ctx_reselect(OM_uint32 *, spnego_gss_ctx_id_t, OM_uint32,
		  gss_OID, gss_buffer_t *, gss_buffer_t *, send_token_flag *);
static OM_uint32
init_ctx_call_init(OM_uint32 *, spnego_gss_ctx_id_t, spnego_gss_cred_id_t,
		   OM_uint32, gss_name_t, OM_uint32, OM_uint32, gss_buffer_t,
		   gss_channel_bindings_t,
		   gss_buffer_t, OM_uint32 *, send_token_flag *);

static OM_uint32
acc_ctx_new(OM_uint32 *, gss_buffer_t, spnego_gss_cred_id_t, gss_buffer_t *,
	    gss_buffer_t *, OM_uint32 *, send_token_flag *,
	    spnego_gss_ctx_id_t *);
static OM_uint32
acc_ctx_cont(OM_uint32 *, gss_buffer_t, spnego_gss_ctx_id_t, gss_buffer_t *,
	     gss_buffer_t *, OM_uint32 *, send_token_flag *);
static OM_uint32
acc_ctx_vfy_oid(OM_uint32 *, spnego_gss_ctx_id_t, gss_OID,
		OM_uint32 *, send_token_flag *);
static OM_uint32
acc_ctx_call_acc(OM_uint32 *, spnego_gss_ctx_id_t, spnego_gss_cred_id_t,
		 gss_buffer_t, gss_channel_bindings_t, gss_buffer_t,
		 OM_uint32 *, OM_uint32 *, send_token_flag *);

static gss_OID
negotiate_mech(spnego_gss_ctx_id_t, gss_OID_set, OM_uint32 *);

static int
make_spnego_tokenInit_msg(spnego_gss_ctx_id_t,
			int,
			gss_buffer_t,
			OM_uint32, gss_buffer_t, send_token_flag,
			gss_buffer_t);
static OM_uint32
make_spnego_tokenTarg_msg(uint8_t, gss_OID, gss_buffer_t,
			gss_buffer_t, send_token_flag,
			gss_buffer_t);

static OM_uint32
get_negTokenInit(OM_uint32 *, gss_buffer_t, gss_buffer_t,
		 gss_OID_set *, OM_uint32 *, gss_buffer_t *,
		 gss_buffer_t *);
static OM_uint32
get_negTokenResp(OM_uint32 *, struct k5input *, OM_uint32 *, gss_OID *,
		 gss_buffer_t *, gss_buffer_t *);

static int
is_kerb_mech(gss_OID oid);

/* SPNEGO oid structure */
static const gss_OID_desc spnego_oids[] = {
	{SPNEGO_OID_LENGTH, SPNEGO_OID},
};

const gss_OID_desc * const gss_mech_spnego = spnego_oids+0;
static const gss_OID_set_desc spnego_oidsets[] = {
	{1, (gss_OID) spnego_oids+0},
};
const gss_OID_set_desc * const gss_mech_set_spnego = spnego_oidsets+0;

static gss_OID_desc negoex_mech = { NEGOEX_OID_LENGTH, NEGOEX_OID };

static int make_NegHints(OM_uint32 *, gss_buffer_t *);
static OM_uint32
acc_ctx_hints(OM_uint32 *, spnego_gss_cred_id_t, gss_buffer_t *, OM_uint32 *,
	      send_token_flag *, spnego_gss_ctx_id_t *);

/*
 * The Mech OID for SPNEGO:
 * { iso(1) org(3) dod(6) internet(1) security(5)
 *  mechanism(5) spnego(2) }
 */
static struct gss_config spnego_mechanism =
{
	{SPNEGO_OID_LENGTH, SPNEGO_OID},
	NULL,
	spnego_gss_acquire_cred,
	spnego_gss_release_cred,
	spnego_gss_init_sec_context,
#ifndef LEAN_CLIENT
	spnego_gss_accept_sec_context,
#else
	NULL,
#endif  /* LEAN_CLIENT */
	NULL,				/* gss_process_context_token */
	spnego_gss_delete_sec_context,	/* gss_delete_sec_context */
	spnego_gss_context_time,	/* gss_context_time */
	spnego_gss_get_mic,		/* gss_get_mic */
	spnego_gss_verify_mic,		/* gss_verify_mic */
	spnego_gss_wrap,		/* gss_wrap */
	spnego_gss_unwrap,		/* gss_unwrap */
	spnego_gss_display_status,
	NULL,				/* gss_indicate_mechs */
	spnego_gss_compare_name,
	spnego_gss_display_name,
	spnego_gss_import_name,
	spnego_gss_release_name,
	spnego_gss_inquire_cred,	/* gss_inquire_cred */
	NULL,				/* gss_add_cred */
#ifndef LEAN_CLIENT
	spnego_gss_export_sec_context,		/* gss_export_sec_context */
	spnego_gss_import_sec_context,		/* gss_import_sec_context */
#else
	NULL,				/* gss_export_sec_context */
	NULL,				/* gss_import_sec_context */
#endif /* LEAN_CLIENT */
	NULL, 				/* gss_inquire_cred_by_mech */
	spnego_gss_inquire_names_for_mech,
	spnego_gss_inquire_context,	/* gss_inquire_context */
	NULL,				/* gss_internal_release_oid */
	spnego_gss_wrap_size_limit,	/* gss_wrap_size_limit */
	spnego_gss_localname,
	NULL,				/* gss_userok */
	NULL,				/* gss_export_name */
	spnego_gss_duplicate_name,	/* gss_duplicate_name */
	NULL,				/* gss_store_cred */
 	spnego_gss_inquire_sec_context_by_oid, /* gss_inquire_sec_context_by_oid */
 	spnego_gss_inquire_cred_by_oid,	/* gss_inquire_cred_by_oid */
 	spnego_gss_set_sec_context_option, /* gss_set_sec_context_option */
	spnego_gss_set_cred_option,	/* gssspi_set_cred_option */
 	NULL,				/* gssspi_mech_invoke */
	spnego_gss_wrap_aead,
	spnego_gss_unwrap_aead,
	spnego_gss_wrap_iov,
	spnego_gss_unwrap_iov,
	spnego_gss_wrap_iov_length,
	spnego_gss_complete_auth_token,
	spnego_gss_acquire_cred_impersonate_name,
	NULL,				/* gss_add_cred_impersonate_name */
	spnego_gss_display_name_ext,
	spnego_gss_inquire_name,
	spnego_gss_get_name_attribute,
	spnego_gss_set_name_attribute,
	spnego_gss_delete_name_attribute,
	spnego_gss_export_name_composite,
	spnego_gss_map_name_to_any,
	spnego_gss_release_any_name_mapping,
	spnego_gss_pseudo_random,
	spnego_gss_set_neg_mechs,
	spnego_gss_inquire_saslname_for_mech,
	spnego_gss_inquire_mech_for_saslname,
	spnego_gss_inquire_attrs_for_mech,
	spnego_gss_acquire_cred_from,
	NULL,				/* gss_store_cred_into */
	spnego_gss_acquire_cred_with_password,
	spnego_gss_export_cred,
	spnego_gss_import_cred,
	NULL,				/* gssspi_import_sec_context_by_mech */
	NULL,				/* gssspi_import_name_by_mech */
	NULL,				/* gssspi_import_cred_by_mech */
	spnego_gss_get_mic_iov,
	spnego_gss_verify_mic_iov,
	spnego_gss_get_mic_iov_length
};

#ifdef _GSS_STATIC_LINK
#include "mglueP.h"

static int gss_spnegomechglue_init(void)
{
	struct gss_mech_config mech_spnego;

	memset(&mech_spnego, 0, sizeof(mech_spnego));
	mech_spnego.mech = &spnego_mechanism;
	mech_spnego.mechNameStr = "spnego";
	mech_spnego.mech_type = GSS_C_NO_OID;

	return gssint_register_mechinfo(&mech_spnego);
}
#else
gss_mechanism KRB5_CALLCONV
gss_mech_initialize(void)
{
	return (&spnego_mechanism);
}

MAKE_INIT_FUNCTION(gss_krb5int_lib_init);
MAKE_FINI_FUNCTION(gss_krb5int_lib_fini);
int gss_krb5int_lib_init(void);
#endif /* _GSS_STATIC_LINK */

int gss_spnegoint_lib_init(void)
{
	int err;

	err = k5_key_register(K5_KEY_GSS_SPNEGO_STATUS, NULL);
	if (err)
		return err;

#ifdef _GSS_STATIC_LINK
	return gss_spnegomechglue_init();
#else
	return 0;
#endif
}

void gss_spnegoint_lib_fini(void)
{
	k5_key_delete(K5_KEY_GSS_SPNEGO_STATUS);
}

static OM_uint32
create_spnego_cred(OM_uint32 *minor_status, gss_cred_id_t mcred,
		   spnego_gss_cred_id_t *cred_out)
{
	spnego_gss_cred_id_t spcred;

	*cred_out = NULL;
	spcred = calloc(1, sizeof(*spcred));
	if (spcred == NULL) {
		*minor_status = ENOMEM;
		return GSS_S_FAILURE;
	}
	spcred->mcred = mcred;
	*cred_out = spcred;
	return GSS_S_COMPLETE;
}

/*ARGSUSED*/
OM_uint32 KRB5_CALLCONV
spnego_gss_acquire_cred(OM_uint32 *minor_status,
			gss_name_t desired_name,
			OM_uint32 time_req,
			gss_OID_set desired_mechs,
			gss_cred_usage_t cred_usage,
			gss_cred_id_t *output_cred_handle,
			gss_OID_set *actual_mechs,
			OM_uint32 *time_rec)
{
    return spnego_gss_acquire_cred_from(minor_status, desired_name, time_req,
					desired_mechs, cred_usage, NULL,
					output_cred_handle, actual_mechs,
					time_rec);
}

/*ARGSUSED*/
OM_uint32 KRB5_CALLCONV
spnego_gss_acquire_cred_from(OM_uint32 *minor_status,
			     const gss_name_t desired_name,
			     OM_uint32 time_req,
			     const gss_OID_set desired_mechs,
			     gss_cred_usage_t cred_usage,
			     gss_const_key_value_set_t cred_store,
			     gss_cred_id_t *output_cred_handle,
			     gss_OID_set *actual_mechs,
			     OM_uint32 *time_rec)
{
	OM_uint32 status, tmpmin;
	gss_OID_set amechs;
	gss_cred_id_t mcred = NULL;
	spnego_gss_cred_id_t spcred = NULL;
	dsyslog("Entering spnego_gss_acquire_cred\n");

	if (actual_mechs)
		*actual_mechs = NULL;

	if (time_rec)
		*time_rec = 0;

	/* We will obtain a mechglue credential and wrap it in a
	 * spnego_gss_cred_id_rec structure.  Allocate the wrapper. */
	status = create_spnego_cred(minor_status, mcred, &spcred);
	if (status != GSS_S_COMPLETE)
		return (status);

	/*
	 * Always use get_available_mechs to collect a list of
	 * mechs for which creds are available.
	 */
	status = get_available_mechs(minor_status, desired_name,
				     cred_usage, cred_store, &mcred,
				     &amechs, time_rec);

	if (actual_mechs && amechs != GSS_C_NULL_OID_SET) {
		(void) gssint_copy_oid_set(&tmpmin, amechs, actual_mechs);
	}
	(void) gss_release_oid_set(&tmpmin, &amechs);

	if (status == GSS_S_COMPLETE) {
		spcred->mcred = mcred;
		*output_cred_handle = (gss_cred_id_t)spcred;
	} else {
		free(spcred);
		*output_cred_handle = GSS_C_NO_CREDENTIAL;
	}

	dsyslog("Leaving spnego_gss_acquire_cred\n");
	return (status);
}

/*ARGSUSED*/
OM_uint32 KRB5_CALLCONV
spnego_gss_release_cred(OM_uint32 *minor_status,
			gss_cred_id_t *cred_handle)
{
	spnego_gss_cred_id_t spcred = NULL;

	dsyslog("Entering spnego_gss_release_cred\n");

	if (minor_status == NULL || cred_handle == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);

	*minor_status = 0;

	if (*cred_handle == GSS_C_NO_CREDENTIAL)
		return (GSS_S_COMPLETE);

	spcred = (spnego_gss_cred_id_t)*cred_handle;
	*cred_handle = GSS_C_NO_CREDENTIAL;
	gss_release_oid_set(minor_status, &spcred->neg_mechs);
	gss_release_cred(minor_status, &spcred->mcred);
	free(spcred);

	dsyslog("Leaving spnego_gss_release_cred\n");
	return (GSS_S_COMPLETE);
}

static spnego_gss_ctx_id_t
create_spnego_ctx(int initiate)
{
	spnego_gss_ctx_id_t spnego_ctx = NULL;

	spnego_ctx = malloc(sizeof(*spnego_ctx));
	if (spnego_ctx == NULL) {
		return (NULL);
	}

	spnego_ctx->magic_num = SPNEGO_MAGIC_ID;
	spnego_ctx->ctx_handle = GSS_C_NO_CONTEXT;
	spnego_ctx->mech_set = NULL;
	spnego_ctx->internal_mech = NULL;
	spnego_ctx->DER_mechTypes.length = 0;
	spnego_ctx->DER_mechTypes.value = NULL;
	spnego_ctx->mic_reqd = 0;
	spnego_ctx->mic_sent = 0;
	spnego_ctx->mic_rcvd = 0;
	spnego_ctx->mech_complete = 0;
	spnego_ctx->nego_done = 0;
	spnego_ctx->opened = 0;
	spnego_ctx->initiate = initiate;
	spnego_ctx->internal_name = GSS_C_NO_NAME;
	spnego_ctx->actual_mech = GSS_C_NO_OID;
	spnego_ctx->deleg_cred = GSS_C_NO_CREDENTIAL;
	spnego_ctx->negoex_step = 0;
	memset(&spnego_ctx->negoex_transcript, 0, sizeof(struct k5buf));
	spnego_ctx->negoex_seqnum = 0;
	K5_TAILQ_INIT(&spnego_ctx->negoex_mechs);
	spnego_ctx->kctx = NULL;
	memset(spnego_ctx->negoex_conv_id, 0, GUID_LENGTH);

	return (spnego_ctx);
}

/* iso(1) org(3) dod(6) internet(1) private(4) enterprises(1) samba(7165)
 * gssntlmssp(655) controls(1) spnego_req_mechlistMIC(2) */
static const gss_OID_desc spnego_req_mechlistMIC_oid =
	{ 11, "\x2B\x06\x01\x04\x01\xB7\x7D\x85\x0F\x01\x02" };

/*
 * Return nonzero if the mechanism has reason to believe that a mechlistMIC
 * exchange will be required.  Microsoft servers erroneously require SPNEGO
 * mechlistMIC if they see an internal MIC within an NTLMSSP Authenticate
 * message, even if NTLMSSP was the preferred mechanism.
 */
static int
mech_requires_mechlistMIC(spnego_gss_ctx_id_t sc)
{
	OM_uint32 major, minor;
	gss_ctx_id_t ctx = sc->ctx_handle;
	gss_OID oid = (gss_OID)&spnego_req_mechlistMIC_oid;
	gss_buffer_set_t bufs;
	int result;

	major = gss_inquire_sec_context_by_oid(&minor, ctx, oid, &bufs);
	if (major != GSS_S_COMPLETE)
		return 0;

	/* Report true if the mech returns a single buffer containing a single
	 * byte with value 1. */
	result = (bufs != NULL && bufs->count == 1 &&
		  bufs->elements[0].length == 1 &&
		  memcmp(bufs->elements[0].value, "\1", 1) == 0);
	(void) gss_release_buffer_set(&minor, &bufs);
	return result;
}

/* iso(1) org(3) dod(6) internet(1) private(4) enterprises(1) Microsoft(311)
 * security(2) mechanisms(2) NTLM(10) */
static const gss_OID_desc gss_mech_ntlmssp_oid =
	{ 10, "\x2b\x06\x01\x04\x01\x82\x37\x02\x02\x0a" };

/* iso(1) org(3) dod(6) internet(1) private(4) enterprises(1) samba(7165)
 * gssntlmssp(655) controls(1) ntlmssp_reset_crypto(3) */
static const gss_OID_desc ntlmssp_reset_crypto_oid =
	{ 11, "\x2B\x06\x01\x04\x01\xB7\x7D\x85\x0F\x01\x03" };

/*
 * MS-SPNG section 3.3.5.1 warns that the NTLM mechanism requires special
 * handling of the crypto state to interop with Windows.  If the mechanism for
 * sc is SPNEGO, invoke a mechanism-specific operation on the context to reset
 * the RC4 state after producing or verifying a MIC.  Ignore a result of
 * GSS_S_UNAVAILABLE for compatibility with older versions of the mechanism
 * that do not support this functionality.
 */
static OM_uint32
ntlmssp_reset_crypto_state(OM_uint32 *minor_status, spnego_gss_ctx_id_t sc,
			   OM_uint32 verify)
{
	OM_uint32 major, minor;
	gss_buffer_desc value;

	if (!g_OID_equal(sc->internal_mech, &gss_mech_ntlmssp_oid))
		return GSS_S_COMPLETE;

	value.length = sizeof(verify);
	value.value = &verify;
	major = gss_set_sec_context_option(&minor, &sc->ctx_handle,
					   (gss_OID)&ntlmssp_reset_crypto_oid,
					   &value);
	if (major == GSS_S_UNAVAILABLE)
		return GSS_S_COMPLETE;
	*minor_status = minor;
	return major;
}

/*
 * Both initiator and acceptor call here to verify and/or create mechListMIC,
 * and to consistency-check the MIC state.  handle_mic is invoked only if the
 * negotiated mech has completed and supports MICs.
 */
static OM_uint32
handle_mic(OM_uint32 *minor_status, gss_buffer_t mic_in,
	   int send_mechtok, spnego_gss_ctx_id_t sc,
	   gss_buffer_t *mic_out,
	   OM_uint32 *negState, send_token_flag *tokflag)
{
	OM_uint32 ret;

	ret = GSS_S_FAILURE;
	*mic_out = GSS_C_NO_BUFFER;
	if (mic_in != GSS_C_NO_BUFFER) {
		if (sc->mic_rcvd) {
			/* Reject MIC if we've already received a MIC. */
			*negState = REJECT;
			*tokflag = ERROR_TOKEN_SEND;
			return GSS_S_DEFECTIVE_TOKEN;
		}
	} else if (sc->mic_reqd && !send_mechtok) {
		/*
		 * If the peer sends the final mechanism token, it
		 * must send the MIC with that token if the
		 * negotiation requires MICs.
		 */
		*negState = REJECT;
		*tokflag = ERROR_TOKEN_SEND;
		return GSS_S_DEFECTIVE_TOKEN;
	}
	ret = process_mic(minor_status, mic_in, sc, mic_out,
			  negState, tokflag);
	if (ret != GSS_S_COMPLETE) {
		return ret;
	}
	if (sc->mic_reqd) {
		assert(sc->mic_sent || sc->mic_rcvd);
	}
	if (sc->mic_sent && sc->mic_rcvd) {
		ret = GSS_S_COMPLETE;
		*negState = ACCEPT_COMPLETE;
		if (*mic_out == GSS_C_NO_BUFFER) {
			/*
			 * We sent a MIC on the previous pass; we
			 * shouldn't be sending a mechanism token.
			 */
			assert(!send_mechtok);
			*tokflag = NO_TOKEN_SEND;
		} else {
			*tokflag = CONT_TOKEN_SEND;
		}
	} else if (sc->mic_reqd) {
		*negState = ACCEPT_INCOMPLETE;
		ret = GSS_S_CONTINUE_NEEDED;
	} else if (*negState == ACCEPT_COMPLETE) {
		ret = GSS_S_COMPLETE;
	} else {
		ret = GSS_S_CONTINUE_NEEDED;
	}
	return ret;
}

/*
 * Perform the actual verification and/or generation of mechListMIC.
 */
static OM_uint32
process_mic(OM_uint32 *minor_status, gss_buffer_t mic_in,
	    spnego_gss_ctx_id_t sc, gss_buffer_t *mic_out,
	    OM_uint32 *negState, send_token_flag *tokflag)
{
	OM_uint32 ret, tmpmin;
	gss_qop_t qop_state;
	gss_buffer_desc tmpmic = GSS_C_EMPTY_BUFFER;

	ret = GSS_S_FAILURE;
	if (mic_in != GSS_C_NO_BUFFER) {
		ret = gss_verify_mic(minor_status, sc->ctx_handle,
				     &sc->DER_mechTypes,
				     mic_in, &qop_state);
		if (ret == GSS_S_COMPLETE)
			ret = ntlmssp_reset_crypto_state(minor_status, sc, 1);
		if (ret != GSS_S_COMPLETE) {
			*negState = REJECT;
			*tokflag = ERROR_TOKEN_SEND;
			return ret;
		}
		/* If we got a MIC, we must send a MIC. */
		sc->mic_reqd = 1;
		sc->mic_rcvd = 1;
	}
	if (sc->mic_reqd && !sc->mic_sent) {
		ret = gss_get_mic(minor_status, sc->ctx_handle,
				  GSS_C_QOP_DEFAULT,
				  &sc->DER_mechTypes,
				  &tmpmic);
		if (ret == GSS_S_COMPLETE)
			ret = ntlmssp_reset_crypto_state(minor_status, sc, 0);
		if (ret != GSS_S_COMPLETE) {
			gss_release_buffer(&tmpmin, &tmpmic);
			*tokflag = NO_TOKEN_SEND;
			return ret;
		}
		*mic_out = malloc(sizeof(gss_buffer_desc));
		if (*mic_out == GSS_C_NO_BUFFER) {
			gss_release_buffer(&tmpmin, &tmpmic);
			*tokflag = NO_TOKEN_SEND;
			return GSS_S_FAILURE;
		}
		**mic_out = tmpmic;
		sc->mic_sent = 1;
	}
	return GSS_S_COMPLETE;
}

/* Create a new SPNEGO context handle for the initial call to
 * spnego_gss_init_sec_context().  */
static OM_uint32
init_ctx_new(OM_uint32 *minor_status,
	     spnego_gss_cred_id_t spcred,
	     send_token_flag *tokflag,
	     spnego_gss_ctx_id_t *sc_out)
{
	OM_uint32 ret;
	spnego_gss_ctx_id_t sc = NULL;

	*sc_out = NULL;

	sc = create_spnego_ctx(1);
	if (sc == NULL)
		return GSS_S_FAILURE;

	/* determine negotiation mech set */
	ret = get_negotiable_mechs(minor_status, sc, spcred, GSS_C_INITIATE);
	if (ret != GSS_S_COMPLETE)
		goto cleanup;

	/* Set an initial internal mech to make the first context token. */
	sc->internal_mech = &sc->mech_set->elements[0];

	if (put_mech_set(sc->mech_set, &sc->DER_mechTypes) < 0) {
		ret = GSS_S_FAILURE;
		goto cleanup;
	}

	sc->ctx_handle = GSS_C_NO_CONTEXT;
	*sc_out = sc;
	sc = NULL;
	*tokflag = INIT_TOKEN_SEND;
	ret = GSS_S_COMPLETE;

cleanup:
	release_spnego_ctx(&sc);
	return ret;
}

/*
 * Called by second and later calls to spnego_gss_init_sec_context()
 * to decode reply and update state.
 */
static OM_uint32
init_ctx_cont(OM_uint32 *minor_status, spnego_gss_ctx_id_t sc,
	      gss_buffer_t buf, gss_buffer_t *responseToken,
	      gss_buffer_t *mechListMIC, OM_uint32 *acc_negState,
	      send_token_flag *tokflag)
{
	OM_uint32 ret, tmpmin;
	gss_OID supportedMech = GSS_C_NO_OID;
	struct k5input in;

	*acc_negState = UNSPECIFIED;
	*tokflag = ERROR_TOKEN_SEND;

	k5_input_init(&in, buf->value, buf->length);
	ret = get_negTokenResp(minor_status, &in, acc_negState, &supportedMech,
			       responseToken, mechListMIC);
	if (ret != GSS_S_COMPLETE)
		goto cleanup;

	/* Bail out now on a reject with no error token.  If we have an error
	 * token, keep going and get a better error status from the mech. */
	if (*acc_negState == REJECT && *responseToken == GSS_C_NO_BUFFER) {
		if (!sc->nego_done) {
			/* RFC 4178 says to return GSS_S_BAD_MECH on a
			 * mechanism negotiation failure. */
			*minor_status = ERR_SPNEGO_NEGOTIATION_FAILED;
			map_errcode(minor_status);
			ret = GSS_S_BAD_MECH;
		} else {
			ret = GSS_S_FAILURE;
		}
		*tokflag = NO_TOKEN_SEND;
		goto cleanup;
	}
	/*
	 * nego_done is false for the first call to init_ctx_cont()
	 */
	if (!sc->nego_done) {
		ret = init_ctx_nego(minor_status, sc, *acc_negState,
				    supportedMech, responseToken, mechListMIC,
				    tokflag);
	} else if ((!sc->mech_complete && *responseToken == GSS_C_NO_BUFFER) ||
		   (sc->mech_complete && *responseToken != GSS_C_NO_BUFFER)) {
		/* Missing or spurious token from acceptor. */
		ret = GSS_S_DEFECTIVE_TOKEN;
	} else if (!sc->mech_complete ||
		   (sc->mic_reqd &&
		    (sc->ctx_flags & GSS_C_INTEG_FLAG))) {
		/* Not obviously done; we may decide we're done later in
		 * init_ctx_call_init or handle_mic. */
		*tokflag = CONT_TOKEN_SEND;
		ret = GSS_S_COMPLETE;
	} else {
		/* mech finished on last pass and no MIC required, so done. */
		*tokflag = NO_TOKEN_SEND;
		ret = GSS_S_COMPLETE;
	}
cleanup:
	if (supportedMech != GSS_C_NO_OID)
		generic_gss_release_oid(&tmpmin, &supportedMech);
	return ret;
}

/*
 * Consistency checking and mechanism negotiation handling for second
 * call of spnego_gss_init_sec_context().  Call init_ctx_reselect() to
 * update internal state if acceptor has counter-proposed.
 */
static OM_uint32
init_ctx_nego(OM_uint32 *minor_status, spnego_gss_ctx_id_t sc,
	      OM_uint32 acc_negState, gss_OID supportedMech,
	      gss_buffer_t *responseToken, gss_buffer_t *mechListMIC,
	      send_token_flag *tokflag)
{
	OM_uint32 ret;

	*tokflag = ERROR_TOKEN_SEND;
	ret = GSS_S_DEFECTIVE_TOKEN;

	/*
	 * According to RFC 4178, both supportedMech and negState must be
	 * present in the first acceptor token.  However, some Java
	 * implementations include only a responseToken in the first
	 * NegTokenResp.  In this case we can use sc->internal_mech as the
	 * negotiated mechanism.  (We do not currently look at acc_negState
	 * when continuing with the optimistic mechanism.)
	 */
	if (supportedMech == GSS_C_NO_OID)
		supportedMech = sc->internal_mech;

	/*
	 * If the mechanism we sent is not the mechanism returned from
	 * the server, we need to handle the server's counter
	 * proposal.  There is a bug in SAMBA servers that always send
	 * the old Kerberos mech OID, even though we sent the new one.
	 * So we will treat all the Kerberos mech OIDS as the same.
         */
	if (!(is_kerb_mech(supportedMech) &&
	      is_kerb_mech(sc->internal_mech)) &&
	    !g_OID_equal(supportedMech, sc->internal_mech)) {
		ret = init_ctx_reselect(minor_status, sc,
					acc_negState, supportedMech,
					responseToken, mechListMIC, tokflag);

	} else if (*responseToken == GSS_C_NO_BUFFER) {
		if (sc->mech_complete) {
			/*
			 * Mech completed on first call to its
			 * init_sec_context().  Acceptor sends no mech
			 * token.
			 */
			*tokflag = NO_TOKEN_SEND;
			ret = GSS_S_COMPLETE;
		} else {
			/*
			 * Reject missing mech token when optimistic
			 * mech selected.
			 */
			*minor_status = ERR_SPNEGO_NO_TOKEN_FROM_ACCEPTOR;
			map_errcode(minor_status);
			ret = GSS_S_DEFECTIVE_TOKEN;
		}
	} else if ((*responseToken)->length == 0 && sc->mech_complete) {
		/* Handle old IIS servers returning empty token instead of
		 * null tokens in the non-mutual auth case. */
		*tokflag = NO_TOKEN_SEND;
		ret = GSS_S_COMPLETE;
	} else if (sc->mech_complete) {
		/* Reject spurious mech token. */
		ret = GSS_S_DEFECTIVE_TOKEN;
	} else {
		*tokflag = CONT_TOKEN_SEND;
		ret = GSS_S_COMPLETE;
	}
	sc->nego_done = 1;
	return ret;
}

/*
 * Handle acceptor's counter-proposal of an alternative mechanism.
 */
static OM_uint32
init_ctx_reselect(OM_uint32 *minor_status, spnego_gss_ctx_id_t sc,
		  OM_uint32 acc_negState, gss_OID supportedMech,
		  gss_buffer_t *responseToken, gss_buffer_t *mechListMIC,
		  send_token_flag *tokflag)
{
	OM_uint32 tmpmin;
	size_t i;

	gss_delete_sec_context(&tmpmin, &sc->ctx_handle,
			       GSS_C_NO_BUFFER);

	/* Find supportedMech in sc->mech_set. */
	for (i = 0; i < sc->mech_set->count; i++) {
		if (g_OID_equal(supportedMech, &sc->mech_set->elements[i]))
			break;
	}
	if (i == sc->mech_set->count)
		return GSS_S_DEFECTIVE_TOKEN;
	sc->internal_mech = &sc->mech_set->elements[i];

	/*
	 * A server conforming to RFC4178 MUST set REQUEST_MIC here, but
	 * Windows Server 2003 and earlier implement (roughly) RFC2478 instead,
	 * and send ACCEPT_INCOMPLETE.  Tolerate that only if we are falling
	 * back to NTLMSSP.
	 */
	if (acc_negState == ACCEPT_INCOMPLETE) {
		if (!g_OID_equal(supportedMech, &gss_mech_ntlmssp_oid))
			return GSS_S_DEFECTIVE_TOKEN;
	} else if (acc_negState != REQUEST_MIC) {
		return GSS_S_DEFECTIVE_TOKEN;
	}

	sc->mech_complete = 0;
	sc->mic_reqd = (acc_negState == REQUEST_MIC);
	*tokflag = CONT_TOKEN_SEND;
	return GSS_S_COMPLETE;
}

/*
 * Wrap call to mechanism gss_init_sec_context() and update state
 * accordingly.
 */
static OM_uint32
init_ctx_call_init(OM_uint32 *minor_status,
		   spnego_gss_ctx_id_t sc,
		   spnego_gss_cred_id_t spcred,
		   OM_uint32 acc_negState,
		   gss_name_t target_name,
		   OM_uint32 req_flags,
		   OM_uint32 time_req,
		   gss_buffer_t mechtok_in,
		   gss_channel_bindings_t bindings,
		   gss_buffer_t mechtok_out,
		   OM_uint32 *time_rec,
		   send_token_flag *send_token)
{
	OM_uint32 ret, tmpret, tmpmin, mech_req_flags;
	gss_cred_id_t mcred;

	mcred = (spcred == NULL) ? GSS_C_NO_CREDENTIAL : spcred->mcred;

	mech_req_flags = req_flags;
	if (spcred == NULL || !spcred->no_ask_integ)
		mech_req_flags |= GSS_C_INTEG_FLAG;

	if (gss_oid_equal(sc->internal_mech, &negoex_mech)) {
		ret = negoex_init(minor_status, sc, mcred, target_name,
				  mech_req_flags, time_req, mechtok_in,
				  bindings, mechtok_out, time_rec);
	} else {
		ret = gss_init_sec_context(minor_status, mcred,
					   &sc->ctx_handle, target_name,
					   sc->internal_mech, mech_req_flags,
					   time_req, bindings, mechtok_in,
					   &sc->actual_mech, mechtok_out,
					   &sc->ctx_flags, time_rec);
	}

	/* Bail out if the acceptor gave us an error token but the mech didn't
	 * see it as an error. */
	if (acc_negState == REJECT && !GSS_ERROR(ret)) {
		ret = GSS_S_DEFECTIVE_TOKEN;
		goto fail;
	}

	if (ret == GSS_S_COMPLETE) {
		sc->mech_complete = 1;
		/*
		 * Microsoft SPNEGO implementations expect an even number of
		 * token exchanges.  So if we're sending a final token, ask for
		 * a zero-length token back from the server.  Also ask for a
		 * token back if this is the first token or if a MIC exchange
		 * is required.
		 */
		if (*send_token == CONT_TOKEN_SEND &&
		    mechtok_out->length == 0 &&
		    (!sc->mic_reqd || !(sc->ctx_flags & GSS_C_INTEG_FLAG)))
			*send_token = NO_TOKEN_SEND;

		return GSS_S_COMPLETE;
	}

	if (ret == GSS_S_CONTINUE_NEEDED)
		return GSS_S_COMPLETE;

	if (*send_token != INIT_TOKEN_SEND) {
		*send_token = ERROR_TOKEN_SEND;
		return ret;
	}

	/*
	 * Since this is the first token, we can fall back to later mechanisms
	 * in the list.  Since the mechanism list is expected to be short, we
	 * can do this with recursion.  If all mechanisms produce errors, the
	 * caller should get the error from the first mech in the list.
	 */
	gssalloc_free(sc->mech_set->elements->elements);
	memmove(sc->mech_set->elements, sc->mech_set->elements + 1,
		--sc->mech_set->count * sizeof(*sc->mech_set->elements));
	if (sc->mech_set->count == 0)
		goto fail;
	gss_release_buffer(&tmpmin, &sc->DER_mechTypes);
	if (put_mech_set(sc->mech_set, &sc->DER_mechTypes) < 0)
		goto fail;
	gss_delete_sec_context(&tmpmin, &sc->ctx_handle, GSS_C_NO_BUFFER);
	tmpret = init_ctx_call_init(&tmpmin, sc, spcred, acc_negState,
				    target_name, req_flags, time_req,
				    mechtok_in, bindings, mechtok_out,
				    time_rec, send_token);
	if (HARD_ERROR(tmpret))
		goto fail;
	*minor_status = tmpmin;
	return tmpret;

fail:
	/* Don't output token on error from first call. */
	*send_token = NO_TOKEN_SEND;
	return ret;
}

/*ARGSUSED*/
OM_uint32 KRB5_CALLCONV
spnego_gss_init_sec_context(
			OM_uint32 *minor_status,
			gss_cred_id_t claimant_cred_handle,
			gss_ctx_id_t *context_handle,
			gss_name_t target_name,
			gss_OID mech_type,
			OM_uint32 req_flags,
			OM_uint32 time_req,
			gss_channel_bindings_t bindings,
			gss_buffer_t input_token,
			gss_OID *actual_mech,
			gss_buffer_t output_token,
			OM_uint32 *ret_flags,
			OM_uint32 *time_rec)
{
	send_token_flag send_token = NO_TOKEN_SEND;
	OM_uint32 tmpmin, ret, negState = UNSPECIFIED, acc_negState;
	gss_buffer_t mechtok_in, mechListMIC_in, mechListMIC_out;
	gss_buffer_desc mechtok_out = GSS_C_EMPTY_BUFFER;
	spnego_gss_cred_id_t spcred = NULL;
	spnego_gss_ctx_id_t spnego_ctx = NULL;

	dsyslog("Entering init_sec_context\n");

	mechtok_in = mechListMIC_out = mechListMIC_in = GSS_C_NO_BUFFER;

	/*
	 * This function works in three steps:
	 *
	 *   1. Perform mechanism negotiation.
	 *   2. Invoke the negotiated or optimistic mech's gss_init_sec_context
	 *      function and examine the results.
	 *   3. Process or generate MICs if necessary.
	 *
	 * The three steps share responsibility for determining when the
	 * exchange is complete.  If the selected mech completed in a previous
	 * call and no MIC exchange is expected, then step 1 will decide.  If
	 * the selected mech completes in this call and no MIC exchange is
	 * expected, then step 2 will decide.  If a MIC exchange is expected,
	 * then step 3 will decide.  If an error occurs in any step, the
	 * exchange will be aborted, possibly with an error token.
	 *
	 * negState determines the state of the negotiation, and is
	 * communicated to the acceptor if a continuing token is sent.
	 * send_token is used to indicate what type of token, if any, should be
	 * generated.
	 */

	/* Validate arguments. */
	if (minor_status != NULL)
		*minor_status = 0;
	if (output_token != GSS_C_NO_BUFFER) {
		output_token->length = 0;
		output_token->value = NULL;
	}
	if (minor_status == NULL ||
	    output_token == GSS_C_NO_BUFFER ||
	    context_handle == NULL)
		return GSS_S_CALL_INACCESSIBLE_WRITE;

	if (actual_mech != NULL)
		*actual_mech = GSS_C_NO_OID;
	if (time_rec != NULL)
		*time_rec = 0;

	/* Step 1: perform mechanism negotiation. */
	spcred = (spnego_gss_cred_id_t)claimant_cred_handle;
	spnego_ctx = (spnego_gss_ctx_id_t)*context_handle;
	if (spnego_ctx == NULL) {
		ret = init_ctx_new(minor_status, spcred, &send_token,
				   &spnego_ctx);
		if (ret != GSS_S_COMPLETE)
			goto cleanup;
		*context_handle = (gss_ctx_id_t)spnego_ctx;
		acc_negState = UNSPECIFIED;
	} else {
		ret = init_ctx_cont(minor_status, spnego_ctx, input_token,
				    &mechtok_in, &mechListMIC_in,
				    &acc_negState, &send_token);
		if (ret != GSS_S_COMPLETE)
			goto cleanup;
	}

	/* Step 2: invoke the selected or optimistic mechanism's
	 * gss_init_sec_context function, if it didn't complete previously. */
	if (!spnego_ctx->mech_complete) {
		ret = init_ctx_call_init(minor_status, spnego_ctx, spcred,
					 acc_negState, target_name, req_flags,
					 time_req, mechtok_in, bindings,
					 &mechtok_out, time_rec, &send_token);
		if (ret != GSS_S_COMPLETE)
			goto cleanup;

		/* Give the mechanism a chance to force a mechlistMIC. */
		if (mech_requires_mechlistMIC(spnego_ctx))
			spnego_ctx->mic_reqd = 1;
	}

	/* Step 3: process or generate the MIC, if the negotiated mech is
	 * complete and supports MICs.  Also decide the outgoing negState. */
	negState = ACCEPT_INCOMPLETE;
	if (spnego_ctx->mech_complete &&
	    (spnego_ctx->ctx_flags & GSS_C_INTEG_FLAG)) {

		ret = handle_mic(minor_status,
				 mechListMIC_in,
				 (mechtok_out.length != 0),
				 spnego_ctx, &mechListMIC_out,
				 &negState, &send_token);
		if (HARD_ERROR(ret))
			goto cleanup;
	}

	if (ret_flags != NULL)
		*ret_flags = spnego_ctx->ctx_flags & ~GSS_C_PROT_READY_FLAG;

	ret = (send_token == NO_TOKEN_SEND || negState == ACCEPT_COMPLETE) ?
		GSS_S_COMPLETE : GSS_S_CONTINUE_NEEDED;

cleanup:
	if (send_token == INIT_TOKEN_SEND) {
		if (make_spnego_tokenInit_msg(spnego_ctx,
					      0,
					      mechListMIC_out,
					      req_flags,
					      &mechtok_out, send_token,
					      output_token) < 0) {
			ret = GSS_S_FAILURE;
		}
	} else if (send_token != NO_TOKEN_SEND) {
		if (send_token == ERROR_TOKEN_SEND)
			negState = REJECT;
		if (make_spnego_tokenTarg_msg(negState, GSS_C_NO_OID,
					      &mechtok_out, mechListMIC_out,
					      send_token,
					      output_token) < 0) {
			ret = GSS_S_FAILURE;
		}
	}
	gss_release_buffer(&tmpmin, &mechtok_out);
	if (ret == GSS_S_COMPLETE) {
		spnego_ctx->opened = 1;
		if (actual_mech != NULL)
			*actual_mech = spnego_ctx->actual_mech;
		/* Get an updated lifetime if we didn't call into the mech. */
		if (time_rec != NULL && *time_rec == 0) {
			(void) gss_context_time(&tmpmin,
						spnego_ctx->ctx_handle,
						time_rec);
		}
	} else if (ret != GSS_S_CONTINUE_NEEDED) {
		if (spnego_ctx != NULL) {
			gss_delete_sec_context(&tmpmin,
					       &spnego_ctx->ctx_handle,
					       GSS_C_NO_BUFFER);
			release_spnego_ctx(&spnego_ctx);
		}
		*context_handle = GSS_C_NO_CONTEXT;
	}
	if (mechtok_in != GSS_C_NO_BUFFER) {
		gss_release_buffer(&tmpmin, mechtok_in);
		free(mechtok_in);
	}
	if (mechListMIC_in != GSS_C_NO_BUFFER) {
		gss_release_buffer(&tmpmin, mechListMIC_in);
		free(mechListMIC_in);
	}
	if (mechListMIC_out != GSS_C_NO_BUFFER) {
		gss_release_buffer(&tmpmin, mechListMIC_out);
		free(mechListMIC_out);
	}
	return ret;
} /* init_sec_context */

/* We don't want to import KRB5 headers here */
static const gss_OID_desc gss_mech_krb5_oid =
	{ 9, "\052\206\110\206\367\022\001\002\002" };
static const gss_OID_desc gss_mech_krb5_wrong_oid =
	{ 9, "\052\206\110\202\367\022\001\002\002" };

/*
 * NegHints ::= SEQUENCE {
 *    hintName       [0]  GeneralString      OPTIONAL,
 *    hintAddress    [1]  OCTET STRING       OPTIONAL
 * }
 */

#define HOST_PREFIX	"host@"
#define HOST_PREFIX_LEN	(sizeof(HOST_PREFIX) - 1)

/* Encode the dummy hintname string (as specified in [MS-SPNG]) into a
 * DER-encoded [0] tagged GeneralString, and place the result in *outbuf. */
static int
make_NegHints(OM_uint32 *minor_status, gss_buffer_t *outbuf)
{
	OM_uint32 major_status;
	size_t hint_len, tlen;
	uint8_t *t;
	const char *hintname = "not_defined_in_RFC4178@please_ignore";
	const size_t hintname_len = strlen(hintname);
	struct k5buf buf;

	*outbuf = GSS_C_NO_BUFFER;
	major_status = GSS_S_FAILURE;

	hint_len = k5_der_value_len(hintname_len);
	tlen = k5_der_value_len(hint_len);

	t = gssalloc_malloc(tlen);
	if (t == NULL) {
		*minor_status = ENOMEM;
		goto errout;
	}
	k5_buf_init_fixed(&buf, t, tlen);

	k5_der_add_taglen(&buf, CONTEXT | 0x00, hint_len);
	k5_der_add_value(&buf, GENERAL_STRING, hintname, hintname_len);
	assert(buf.len == tlen);

	*outbuf = (gss_buffer_t)malloc(sizeof(gss_buffer_desc));
	if (*outbuf == NULL) {
		*minor_status = ENOMEM;
		goto errout;
	}
	(*outbuf)->value = (void *)t;
	(*outbuf)->length = tlen;

	t = NULL; /* don't free */

	*minor_status = 0;
	major_status = GSS_S_COMPLETE;

errout:
	if (t != NULL) {
		free(t);
	}

	return (major_status);
}

/*
 * Create a new SPNEGO context handle for the initial call to
 * spnego_gss_accept_sec_context() when the request is empty.  For empty
 * requests, we implement the Microsoft NegHints extension to SPNEGO for
 * compatibility with some versions of Samba.  See:
 * https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-spng/8e71cf53-e867-4b79-b5b5-38c92be3d472
 */
static OM_uint32
acc_ctx_hints(OM_uint32 *minor_status,
	      spnego_gss_cred_id_t spcred,
	      gss_buffer_t *mechListMIC,
	      OM_uint32 *negState,
	      send_token_flag *return_token,
	      spnego_gss_ctx_id_t *sc_out)
{
	OM_uint32 ret;
	spnego_gss_ctx_id_t sc = NULL;

	*mechListMIC = GSS_C_NO_BUFFER;
	*return_token = NO_TOKEN_SEND;
	*negState = REJECT;
	*minor_status = 0;
	*sc_out = NULL;

	ret = make_NegHints(minor_status, mechListMIC);
	if (ret != GSS_S_COMPLETE)
		goto cleanup;

	sc = create_spnego_ctx(0);
	if (sc == NULL) {
		ret = GSS_S_FAILURE;
		goto cleanup;
	}

	ret = get_negotiable_mechs(minor_status, sc, spcred, GSS_C_ACCEPT);
	if (ret != GSS_S_COMPLETE)
		goto cleanup;

	if (put_mech_set(sc->mech_set, &sc->DER_mechTypes) < 0) {
		ret = GSS_S_FAILURE;
		goto cleanup;
	}
	sc->internal_mech = GSS_C_NO_OID;

	*negState = ACCEPT_INCOMPLETE;
	*return_token = INIT_TOKEN_SEND;
	sc->firstpass = 1;
	*sc_out = sc;
	sc = NULL;
	ret = GSS_S_COMPLETE;

cleanup:
	release_spnego_ctx(&sc);

	return ret;
}

/*
 * Create a new SPNEGO context handle for the initial call to
 * spnego_gss_accept_sec_context().  Set negState to REJECT if the token is
 * defective, else ACCEPT_INCOMPLETE or REQUEST_MIC, depending on whether
 * the initiator's preferred mechanism is supported.
 */
static OM_uint32
acc_ctx_new(OM_uint32 *minor_status,
	    gss_buffer_t buf,
	    spnego_gss_cred_id_t spcred,
	    gss_buffer_t *mechToken,
	    gss_buffer_t *mechListMIC,
	    OM_uint32 *negState,
	    send_token_flag *return_token,
	    spnego_gss_ctx_id_t *sc_out)
{
	OM_uint32 tmpmin, ret, req_flags;
	gss_OID_set mechTypes;
	gss_buffer_desc der_mechTypes;
	gss_OID mech_wanted;
	spnego_gss_ctx_id_t sc = NULL;

	ret = GSS_S_DEFECTIVE_TOKEN;
	der_mechTypes.length = 0;
	der_mechTypes.value = NULL;
	*mechToken = *mechListMIC = GSS_C_NO_BUFFER;
	mechTypes = GSS_C_NO_OID_SET;
	*return_token = ERROR_TOKEN_SEND;
	*negState = REJECT;
	*minor_status = 0;

	ret = get_negTokenInit(minor_status, buf, &der_mechTypes,
			       &mechTypes, &req_flags,
			       mechToken, mechListMIC);
	if (ret != GSS_S_COMPLETE) {
		goto cleanup;
	}

	sc = create_spnego_ctx(0);
	if (sc == NULL) {
		ret = GSS_S_FAILURE;
		*return_token = NO_TOKEN_SEND;
		goto cleanup;
	}

	ret = get_negotiable_mechs(minor_status, sc, spcred, GSS_C_ACCEPT);
	if (ret != GSS_S_COMPLETE) {
		*return_token = NO_TOKEN_SEND;
		goto cleanup;
	}
	/*
	 * Select the best match between the list of mechs
	 * that the initiator requested and the list that
	 * the acceptor will support.
	 */
	mech_wanted = negotiate_mech(sc, mechTypes, negState);
	if (*negState == REJECT) {
		ret = GSS_S_BAD_MECH;
		goto cleanup;
	}

	sc->internal_mech = mech_wanted;
	sc->DER_mechTypes = der_mechTypes;
	der_mechTypes.length = 0;
	der_mechTypes.value = NULL;

	if (*negState == REQUEST_MIC)
		sc->mic_reqd = 1;

	*return_token = INIT_TOKEN_SEND;
	sc->firstpass = 1;
	*sc_out = sc;
	sc = NULL;
	ret = GSS_S_COMPLETE;

cleanup:
	release_spnego_ctx(&sc);
	gss_release_oid_set(&tmpmin, &mechTypes);
	if (der_mechTypes.length != 0)
		gss_release_buffer(&tmpmin, &der_mechTypes);

	return ret;
}

static OM_uint32
acc_ctx_cont(OM_uint32 *minstat,
	     gss_buffer_t buf,
	     spnego_gss_ctx_id_t sc,
	     gss_buffer_t *responseToken,
	     gss_buffer_t *mechListMIC,
	     OM_uint32 *negState,
	     send_token_flag *return_token)
{
	OM_uint32 ret, tmpmin;
	gss_OID supportedMech;
	struct k5input in;

	ret = GSS_S_DEFECTIVE_TOKEN;
	*negState = REJECT;
	*minstat = 0;
	supportedMech = GSS_C_NO_OID;
	*return_token = ERROR_TOKEN_SEND;
	*responseToken = *mechListMIC = GSS_C_NO_BUFFER;

	k5_input_init(&in, buf->value, buf->length);

	/* Attempt to work with old Sun SPNEGO. */
	if (in.len > 0 && *in.ptr == HEADER_ID) {
		ret = verify_token_header(&in, gss_mech_spnego);
		if (ret) {
			*minstat = ret;
			return GSS_S_DEFECTIVE_TOKEN;
		}
	}

	ret = get_negTokenResp(minstat, &in, negState, &supportedMech,
			       responseToken, mechListMIC);
	if (ret != GSS_S_COMPLETE)
		goto cleanup;

	if (*responseToken == GSS_C_NO_BUFFER &&
	    *mechListMIC == GSS_C_NO_BUFFER) {

		ret = GSS_S_DEFECTIVE_TOKEN;
		goto cleanup;
	}
	if (supportedMech != GSS_C_NO_OID) {
		ret = GSS_S_DEFECTIVE_TOKEN;
		goto cleanup;
	}
	sc->firstpass = 0;
	*negState = ACCEPT_INCOMPLETE;
	*return_token = CONT_TOKEN_SEND;
cleanup:
	if (supportedMech != GSS_C_NO_OID) {
		generic_gss_release_oid(&tmpmin, &supportedMech);
	}
	return ret;
}

/*
 * Verify that mech OID is either exactly the same as the negotiated
 * mech OID, or is a mech OID supported by the negotiated mech.  MS
 * implementations can list a most preferred mech using an incorrect
 * krb5 OID while emitting a krb5 initiator mech token having the
 * correct krb5 mech OID.
 */
static OM_uint32
acc_ctx_vfy_oid(OM_uint32 *minor_status,
		spnego_gss_ctx_id_t sc, gss_OID mechoid,
		OM_uint32 *negState, send_token_flag *tokflag)
{
	OM_uint32 ret, tmpmin;
	gss_mechanism mech = NULL;
	gss_OID_set mech_set = GSS_C_NO_OID_SET;
	int present = 0;

	if (g_OID_equal(sc->internal_mech, mechoid))
		return GSS_S_COMPLETE;

	mech = gssint_get_mechanism(sc->internal_mech);
	if (mech == NULL || mech->gss_indicate_mechs == NULL) {
		*minor_status = ERR_SPNEGO_NEGOTIATION_FAILED;
		map_errcode(minor_status);
		*negState = REJECT;
		*tokflag = ERROR_TOKEN_SEND;
		return GSS_S_BAD_MECH;
	}
	ret = mech->gss_indicate_mechs(minor_status, &mech_set);
	if (ret != GSS_S_COMPLETE) {
		*tokflag = NO_TOKEN_SEND;
		map_error(minor_status, mech);
		goto cleanup;
	}
	ret = gss_test_oid_set_member(minor_status, mechoid,
				      mech_set, &present);
	if (ret != GSS_S_COMPLETE)
		goto cleanup;
	if (!present) {
		*minor_status = ERR_SPNEGO_NEGOTIATION_FAILED;
		map_errcode(minor_status);
		*negState = REJECT;
		*tokflag = ERROR_TOKEN_SEND;
		ret = GSS_S_BAD_MECH;
	}
cleanup:
	gss_release_oid_set(&tmpmin, &mech_set);
	return ret;
}
#ifndef LEAN_CLIENT
/*
 * Wrap call to gss_accept_sec_context() and update state
 * accordingly.
 */
static OM_uint32
acc_ctx_call_acc(OM_uint32 *minor_status, spnego_gss_ctx_id_t sc,
		 spnego_gss_cred_id_t spcred, gss_buffer_t mechtok_in,
		 gss_channel_bindings_t bindings, gss_buffer_t mechtok_out,
		 OM_uint32 *time_rec, OM_uint32 *negState,
		 send_token_flag *tokflag)
{
	OM_uint32 ret, tmpmin;
	gss_OID_desc mechoid;
	gss_cred_id_t mcred;
	int negoex = gss_oid_equal(sc->internal_mech, &negoex_mech);

	if (sc->ctx_handle == GSS_C_NO_CONTEXT && !negoex) {
		/*
		 * mechoid is an alias; don't free it.
		 */
		ret = gssint_get_mech_type(&mechoid, mechtok_in);
		if (ret != GSS_S_COMPLETE) {
			*tokflag = NO_TOKEN_SEND;
			return ret;
		}
		ret = acc_ctx_vfy_oid(minor_status, sc, &mechoid,
				      negState, tokflag);
		if (ret != GSS_S_COMPLETE)
			return ret;
	}

	mcred = (spcred == NULL) ? GSS_C_NO_CREDENTIAL : spcred->mcred;
	if (negoex) {
		ret = negoex_accept(minor_status, sc, mcred, mechtok_in,
				    bindings, mechtok_out, time_rec);
	} else {
		(void) gss_release_name(&tmpmin, &sc->internal_name);
		(void) gss_release_cred(&tmpmin, &sc->deleg_cred);
		ret = gss_accept_sec_context(minor_status, &sc->ctx_handle,
					     mcred, mechtok_in, bindings,
					     &sc->internal_name,
					     &sc->actual_mech, mechtok_out,
					     &sc->ctx_flags, time_rec,
					     &sc->deleg_cred);
	}
	if (ret == GSS_S_COMPLETE) {
#ifdef MS_BUG_TEST
		/*
		 * Force MIC to be not required even if we previously
		 * requested a MIC.
		 */
		char *envstr = getenv("MS_FORCE_NO_MIC");

		if (envstr != NULL && strcmp(envstr, "1") == 0 &&
		    !(sc->ctx_flags & GSS_C_MUTUAL_FLAG) &&
		    sc->mic_reqd) {

			sc->mic_reqd = 0;
		}
#endif
		sc->mech_complete = 1;

		if (!sc->mic_reqd ||
		    !(sc->ctx_flags & GSS_C_INTEG_FLAG)) {
			/* No MIC exchange required, so we're done. */
			*negState = ACCEPT_COMPLETE;
			ret = GSS_S_COMPLETE;
		} else {
			/* handle_mic will decide if we're done. */
			ret = GSS_S_CONTINUE_NEEDED;
		}
	} else if (ret != GSS_S_CONTINUE_NEEDED) {
		*negState = REJECT;
		*tokflag = ERROR_TOKEN_SEND;
	}
	return ret;
}

/*ARGSUSED*/
OM_uint32 KRB5_CALLCONV
spnego_gss_accept_sec_context(
			    OM_uint32 *minor_status,
			    gss_ctx_id_t *context_handle,
			    gss_cred_id_t verifier_cred_handle,
			    gss_buffer_t input_token,
			    gss_channel_bindings_t bindings,
			    gss_name_t *src_name,
			    gss_OID *mech_type,
			    gss_buffer_t output_token,
			    OM_uint32 *ret_flags,
			    OM_uint32 *time_rec,
			    gss_cred_id_t *delegated_cred_handle)
{
	OM_uint32 ret, tmpmin, negState;
	send_token_flag return_token;
	gss_buffer_t mechtok_in, mic_in, mic_out;
	gss_buffer_desc mechtok_out = GSS_C_EMPTY_BUFFER;
	spnego_gss_ctx_id_t sc = NULL;
	spnego_gss_cred_id_t spcred = NULL;
	int sendTokenInit = 0, tmpret;

	mechtok_in = mic_in = mic_out = GSS_C_NO_BUFFER;

	/*
	 * This function works in three steps:
	 *
	 *   1. Perform mechanism negotiation.
	 *   2. Invoke the negotiated mech's gss_accept_sec_context function
	 *      and examine the results.
	 *   3. Process or generate MICs if necessary.
	 *
	 * Step one determines whether the negotiation requires a MIC exchange,
	 * while steps two and three share responsibility for determining when
	 * the exchange is complete.  If the selected mech completes in this
	 * call and no MIC exchange is expected, then step 2 will decide.  If a
	 * MIC exchange is expected, then step 3 will decide.  If an error
	 * occurs in any step, the exchange will be aborted, possibly with an
	 * error token.
	 *
	 * negState determines the state of the negotiation, and is
	 * communicated to the acceptor if a continuing token is sent.
	 * return_token is used to indicate what type of token, if any, should
	 * be generated.
	 */

	/* Validate arguments. */
	if (minor_status != NULL)
		*minor_status = 0;
	if (output_token != GSS_C_NO_BUFFER) {
		output_token->length = 0;
		output_token->value = NULL;
	}
	if (src_name != NULL)
		*src_name = GSS_C_NO_NAME;
	if (mech_type != NULL)
		*mech_type = GSS_C_NO_OID;
	if (time_rec != NULL)
		*time_rec = 0;
	if (ret_flags != NULL)
		*ret_flags = 0;
	if (delegated_cred_handle != NULL)
		*delegated_cred_handle = GSS_C_NO_CREDENTIAL;

	if (minor_status == NULL ||
	    output_token == GSS_C_NO_BUFFER ||
	    context_handle == NULL)
		return GSS_S_CALL_INACCESSIBLE_WRITE;

	if (input_token == GSS_C_NO_BUFFER)
		return GSS_S_CALL_INACCESSIBLE_READ;

	/* Step 1: Perform mechanism negotiation. */
	sc = (spnego_gss_ctx_id_t)*context_handle;
	spcred = (spnego_gss_cred_id_t)verifier_cred_handle;
	if (sc == NULL && input_token->length == 0) {
		/* Process a request for NegHints. */
		ret = acc_ctx_hints(minor_status, spcred, &mic_out, &negState,
				    &return_token, &sc);
		if (ret != GSS_S_COMPLETE)
			goto cleanup;
		*context_handle = (gss_ctx_id_t)sc;
		sendTokenInit = 1;
		ret = GSS_S_CONTINUE_NEEDED;
	} else if (sc == NULL || sc->internal_mech == GSS_C_NO_OID) {
		if (sc != NULL) {
			/* Discard the context from the NegHints request. */
			release_spnego_ctx(&sc);
			*context_handle = GSS_C_NO_CONTEXT;
		}
		/* Process an initial token; can set negState to
		 * REQUEST_MIC. */
		ret = acc_ctx_new(minor_status, input_token, spcred,
				  &mechtok_in, &mic_in, &negState,
				  &return_token, &sc);
		if (ret != GSS_S_COMPLETE)
			goto cleanup;
		*context_handle = (gss_ctx_id_t)sc;
		ret = GSS_S_CONTINUE_NEEDED;
	} else {
		/* Process a response token.  Can set negState to
		 * ACCEPT_INCOMPLETE. */
		ret = acc_ctx_cont(minor_status, input_token, sc, &mechtok_in,
				   &mic_in, &negState, &return_token);
		if (ret != GSS_S_COMPLETE)
			goto cleanup;
		ret = GSS_S_CONTINUE_NEEDED;
	}

	/* Step 2: invoke the negotiated mechanism's gss_accept_sec_context
	 * function. */
	/*
	 * Handle mechtok_in and mic_in only if they are
	 * present in input_token.  If neither is present, whether
	 * this is an error depends on whether this is the first
	 * round-trip.  RET is set to a default value according to
	 * whether it is the first round-trip.
	 */
	if (negState != REQUEST_MIC && mechtok_in != GSS_C_NO_BUFFER) {
		ret = acc_ctx_call_acc(minor_status, sc, spcred, mechtok_in,
				       bindings, &mechtok_out, time_rec,
				       &negState, &return_token);
	}

	/* Step 3: process or generate the MIC, if the negotiated mech is
	 * complete and supports MICs. */
	if (!HARD_ERROR(ret) && sc->mech_complete &&
	    (sc->ctx_flags & GSS_C_INTEG_FLAG)) {

		ret = handle_mic(minor_status, mic_in,
				 (mechtok_out.length != 0),
				 sc, &mic_out,
				 &negState, &return_token);
	}

	if (!HARD_ERROR(ret) && ret_flags != NULL)
		*ret_flags = sc->ctx_flags & ~GSS_C_PROT_READY_FLAG;

cleanup:
	if (return_token == INIT_TOKEN_SEND && sendTokenInit) {
		assert(sc != NULL);
		tmpret = make_spnego_tokenInit_msg(sc, 1, mic_out, 0,
						   GSS_C_NO_BUFFER,
						   return_token, output_token);
		if (tmpret < 0)
			ret = GSS_S_FAILURE;
	} else if (return_token != NO_TOKEN_SEND &&
		   return_token != CHECK_MIC) {
		tmpret = make_spnego_tokenTarg_msg(negState,
						   sc ? sc->internal_mech :
						   GSS_C_NO_OID,
						   &mechtok_out, mic_out,
						   return_token,
						   output_token);
		if (tmpret < 0)
			ret = GSS_S_FAILURE;
	}
	if (ret == GSS_S_COMPLETE) {
		sc->opened = 1;
		if (sc->internal_name != GSS_C_NO_NAME &&
		    src_name != NULL) {
			*src_name = sc->internal_name;
			sc->internal_name = GSS_C_NO_NAME;
		}
		if (mech_type != NULL)
			*mech_type = sc->actual_mech;
		/* Get an updated lifetime if we didn't call into the mech. */
		if (time_rec != NULL && *time_rec == 0) {
			(void) gss_context_time(&tmpmin, sc->ctx_handle,
						time_rec);
		}
		if (delegated_cred_handle != NULL) {
			*delegated_cred_handle = sc->deleg_cred;
			sc->deleg_cred = GSS_C_NO_CREDENTIAL;
		}
	} else if (ret != GSS_S_CONTINUE_NEEDED) {
		if (sc != NULL) {
			gss_delete_sec_context(&tmpmin, &sc->ctx_handle,
					       GSS_C_NO_BUFFER);
			release_spnego_ctx(&sc);
		}
		*context_handle = GSS_C_NO_CONTEXT;
	}
	gss_release_buffer(&tmpmin, &mechtok_out);
	if (mechtok_in != GSS_C_NO_BUFFER) {
		gss_release_buffer(&tmpmin, mechtok_in);
		free(mechtok_in);
	}
	if (mic_in != GSS_C_NO_BUFFER) {
		gss_release_buffer(&tmpmin, mic_in);
		free(mic_in);
	}
	if (mic_out != GSS_C_NO_BUFFER) {
		gss_release_buffer(&tmpmin, mic_out);
		free(mic_out);
	}
	return ret;
}
#endif /*  LEAN_CLIENT */

static struct {
	OM_uint32 status;
	const char *msg;
} msg_table[] = {
	{ ERR_SPNEGO_NO_MECHS_AVAILABLE,
	  N_("SPNEGO cannot find mechanisms to negotiate") },
	{ ERR_SPNEGO_NO_CREDS_ACQUIRED,
	  N_("SPNEGO failed to acquire creds") },
	{ ERR_SPNEGO_NO_MECH_FROM_ACCEPTOR,
	  N_("SPNEGO acceptor did not select a mechanism") },
	{ ERR_SPNEGO_NEGOTIATION_FAILED,
	  N_("SPNEGO failed to negotiate a mechanism") },
	{ ERR_SPNEGO_NO_TOKEN_FROM_ACCEPTOR,
	  N_("SPNEGO acceptor did not return a valid token") },
	{ ERR_NEGOEX_INVALID_MESSAGE_SIGNATURE,
	  N_("Invalid NegoEx signature") },
	{ ERR_NEGOEX_INVALID_MESSAGE_TYPE,
	  N_("Invalid NegoEx message type") },
	{ ERR_NEGOEX_INVALID_MESSAGE_SIZE,
	  N_("Invalid NegoEx message size") },
	{ ERR_NEGOEX_INVALID_CONVERSATION_ID,
	  N_("Invalid NegoEx conversation ID") },
	{ ERR_NEGOEX_AUTH_SCHEME_NOT_FOUND,
	  N_("NegoEx authentication scheme not found") },
	{ ERR_NEGOEX_MISSING_NEGO_MESSAGE,
	  N_("Missing NegoEx negotiate message") },
	{ ERR_NEGOEX_MISSING_AP_REQUEST_MESSAGE,
	  N_("Missing NegoEx authentication protocol request message") },
	{ ERR_NEGOEX_NO_AVAILABLE_MECHS,
	  N_("No mutually supported NegoEx authentication schemes") },
	{ ERR_NEGOEX_NO_VERIFY_KEY,
	  N_("No NegoEx verify key") },
	{ ERR_NEGOEX_UNKNOWN_CHECKSUM_SCHEME,
	  N_("Unknown NegoEx checksum scheme") },
	{ ERR_NEGOEX_INVALID_CHECKSUM,
	  N_("Invalid NegoEx checksum") },
	{ ERR_NEGOEX_UNSUPPORTED_CRITICAL_EXTENSION,
	  N_("Unsupported critical NegoEx extension") },
	{ ERR_NEGOEX_UNSUPPORTED_VERSION,
	  N_("Unsupported NegoEx version") },
	{ ERR_NEGOEX_MESSAGE_OUT_OF_SEQUENCE,
	  N_("NegoEx message out of sequence") },
};

/*ARGSUSED*/
OM_uint32 KRB5_CALLCONV
spnego_gss_display_status(
		OM_uint32 *minor_status,
		OM_uint32 status_value,
		int status_type,
		gss_OID mech_type,
		OM_uint32 *message_context,
		gss_buffer_t status_string)
{
	OM_uint32 maj = GSS_S_COMPLETE;
	const char *msg;
	size_t i;
	int ret;

	*message_context = 0;
	for (i = 0; i < sizeof(msg_table) / sizeof(*msg_table); i++) {
		if (status_value == msg_table[i].status) {
			msg = dgettext(KRB5_TEXTDOMAIN, msg_table[i].msg);
			*status_string = make_err_msg(msg);
			return GSS_S_COMPLETE;
		}
	}

	/* Not one of our minor codes; might be from a mech.  Call back
	 * to gss_display_status, but first check for recursion. */
	if (k5_getspecific(K5_KEY_GSS_SPNEGO_STATUS) != NULL) {
		/* Perhaps we returned a com_err code like ENOMEM. */
		const char *err = error_message(status_value);
		*status_string = make_err_msg(err);
		return GSS_S_COMPLETE;
	}
	/* Set a non-null pointer value; doesn't matter which one. */
	ret = k5_setspecific(K5_KEY_GSS_SPNEGO_STATUS, &ret);
	if (ret != 0) {
		*minor_status = ret;
		return GSS_S_FAILURE;
	}

	maj = gss_display_status(minor_status, status_value,
				 status_type, mech_type,
				 message_context, status_string);
	/* This is unlikely to fail; not much we can do if it does. */
	(void)k5_setspecific(K5_KEY_GSS_SPNEGO_STATUS, NULL);

	return maj;
}


/*ARGSUSED*/
OM_uint32 KRB5_CALLCONV
spnego_gss_import_name(
		    OM_uint32 *minor_status,
		    gss_buffer_t input_name_buffer,
		    gss_OID input_name_type,
		    gss_name_t *output_name)
{
	OM_uint32 status;

	dsyslog("Entering import_name\n");

	status = gss_import_name(minor_status, input_name_buffer,
			input_name_type, output_name);

	dsyslog("Leaving import_name\n");
	return (status);
}

/*ARGSUSED*/
OM_uint32 KRB5_CALLCONV
spnego_gss_release_name(
			OM_uint32 *minor_status,
			gss_name_t *input_name)
{
	OM_uint32 status;

	dsyslog("Entering release_name\n");

	status = gss_release_name(minor_status, input_name);

	dsyslog("Leaving release_name\n");
	return (status);
}

/*ARGSUSED*/
OM_uint32 KRB5_CALLCONV
spnego_gss_duplicate_name(
			OM_uint32 *minor_status,
			const gss_name_t input_name,
			gss_name_t *output_name)
{
	OM_uint32 status;

	dsyslog("Entering duplicate_name\n");

	status = gss_duplicate_name(minor_status, input_name, output_name);

	dsyslog("Leaving duplicate_name\n");
	return (status);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_inquire_cred(
			OM_uint32 *minor_status,
			gss_cred_id_t cred_handle,
			gss_name_t *name,
			OM_uint32 *lifetime,
			int *cred_usage,
			gss_OID_set *mechanisms)
{
	OM_uint32 status;
	spnego_gss_cred_id_t spcred = NULL;
	gss_cred_id_t creds = GSS_C_NO_CREDENTIAL;
	OM_uint32 tmp_minor_status;
	OM_uint32 initiator_lifetime, acceptor_lifetime;

	dsyslog("Entering inquire_cred\n");

	/*
	 * To avoid infinite recursion, if GSS_C_NO_CREDENTIAL is
	 * supplied we call gss_inquire_cred_by_mech() on the
	 * first non-SPNEGO mechanism.
	 */
	spcred = (spnego_gss_cred_id_t)cred_handle;
	if (spcred == NULL) {
		status = get_available_mechs(minor_status,
			GSS_C_NO_NAME,
			GSS_C_BOTH,
			GSS_C_NO_CRED_STORE,
			&creds,
			mechanisms, NULL);
		if (status != GSS_S_COMPLETE) {
			dsyslog("Leaving inquire_cred\n");
			return (status);
		}

		if ((*mechanisms)->count == 0) {
			gss_release_cred(&tmp_minor_status, &creds);
			gss_release_oid_set(&tmp_minor_status, mechanisms);
			dsyslog("Leaving inquire_cred\n");
			return (GSS_S_DEFECTIVE_CREDENTIAL);
		}

		assert((*mechanisms)->elements != NULL);

		status = gss_inquire_cred_by_mech(minor_status,
			creds,
			&(*mechanisms)->elements[0],
			name,
			&initiator_lifetime,
			&acceptor_lifetime,
			cred_usage);
		if (status != GSS_S_COMPLETE) {
			gss_release_cred(&tmp_minor_status, &creds);
			dsyslog("Leaving inquire_cred\n");
			return (status);
		}

		if (lifetime != NULL)
			*lifetime = (*cred_usage == GSS_C_ACCEPT) ?
				acceptor_lifetime : initiator_lifetime;

		gss_release_cred(&tmp_minor_status, &creds);
	} else {
		status = gss_inquire_cred(minor_status, spcred->mcred,
					  name, lifetime,
					  cred_usage, mechanisms);
	}

	dsyslog("Leaving inquire_cred\n");

	return (status);
}

/*ARGSUSED*/
OM_uint32 KRB5_CALLCONV
spnego_gss_compare_name(
			OM_uint32 *minor_status,
			const gss_name_t name1,
			const gss_name_t name2,
			int *name_equal)
{
	OM_uint32 status = GSS_S_COMPLETE;
	dsyslog("Entering compare_name\n");

	status = gss_compare_name(minor_status, name1, name2, name_equal);

	dsyslog("Leaving compare_name\n");
	return (status);
}

/*ARGSUSED*/
/*ARGSUSED*/
OM_uint32 KRB5_CALLCONV
spnego_gss_display_name(
			OM_uint32 *minor_status,
			gss_name_t input_name,
			gss_buffer_t output_name_buffer,
			gss_OID *output_name_type)
{
	OM_uint32 status = GSS_S_COMPLETE;
	dsyslog("Entering display_name\n");

	status = gss_display_name(minor_status, input_name,
			output_name_buffer, output_name_type);

	dsyslog("Leaving display_name\n");
	return (status);
}


/*ARGSUSED*/
OM_uint32 KRB5_CALLCONV
spnego_gss_inquire_names_for_mech(
				OM_uint32	*minor_status,
				gss_OID		mechanism,
				gss_OID_set	*name_types)
{
	OM_uint32   major, minor;

	dsyslog("Entering inquire_names_for_mech\n");
	/*
	 * We only know how to handle our own mechanism.
	 */
	if ((mechanism != GSS_C_NULL_OID) &&
	    !g_OID_equal(gss_mech_spnego, mechanism)) {
		*minor_status = 0;
		return (GSS_S_FAILURE);
	}

	major = gss_create_empty_oid_set(minor_status, name_types);
	if (major == GSS_S_COMPLETE) {
		/* Now add our members. */
		if (((major = gss_add_oid_set_member(minor_status,
				(gss_OID) GSS_C_NT_USER_NAME,
				name_types)) == GSS_S_COMPLETE) &&
		    ((major = gss_add_oid_set_member(minor_status,
				(gss_OID) GSS_C_NT_MACHINE_UID_NAME,
				name_types)) == GSS_S_COMPLETE) &&
		    ((major = gss_add_oid_set_member(minor_status,
				(gss_OID) GSS_C_NT_STRING_UID_NAME,
				name_types)) == GSS_S_COMPLETE)) {
			major = gss_add_oid_set_member(minor_status,
				(gss_OID) GSS_C_NT_HOSTBASED_SERVICE,
				name_types);
		}

		if (major != GSS_S_COMPLETE)
			(void) gss_release_oid_set(&minor, name_types);
	}

	dsyslog("Leaving inquire_names_for_mech\n");
	return (major);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_unwrap(
		OM_uint32 *minor_status,
		gss_ctx_id_t context_handle,
		gss_buffer_t input_message_buffer,
		gss_buffer_t output_message_buffer,
		int *conf_state,
		gss_qop_t *qop_state)
{
	OM_uint32 ret;
	spnego_gss_ctx_id_t sc = (spnego_gss_ctx_id_t)context_handle;

	if (sc->ctx_handle == GSS_C_NO_CONTEXT)
		return (GSS_S_NO_CONTEXT);

	ret = gss_unwrap(minor_status,
			sc->ctx_handle,
			input_message_buffer,
			output_message_buffer,
			conf_state,
			qop_state);

	return (ret);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_wrap(
		OM_uint32 *minor_status,
		gss_ctx_id_t context_handle,
		int conf_req_flag,
		gss_qop_t qop_req,
		gss_buffer_t input_message_buffer,
		int *conf_state,
		gss_buffer_t output_message_buffer)
{
	OM_uint32 ret;
	spnego_gss_ctx_id_t sc = (spnego_gss_ctx_id_t)context_handle;

	if (sc->ctx_handle == GSS_C_NO_CONTEXT)
		return (GSS_S_NO_CONTEXT);

	ret = gss_wrap(minor_status,
		    sc->ctx_handle,
		    conf_req_flag,
		    qop_req,
		    input_message_buffer,
		    conf_state,
		    output_message_buffer);

	return (ret);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_process_context_token(
				OM_uint32	*minor_status,
				const gss_ctx_id_t context_handle,
				const gss_buffer_t token_buffer)
{
	OM_uint32 ret;
	spnego_gss_ctx_id_t sc = (spnego_gss_ctx_id_t)context_handle;

	/* SPNEGO doesn't have its own context tokens. */
	if (!sc->opened)
		return (GSS_S_DEFECTIVE_TOKEN);

	ret = gss_process_context_token(minor_status,
					sc->ctx_handle,
					token_buffer);

	return (ret);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_delete_sec_context(
			    OM_uint32 *minor_status,
			    gss_ctx_id_t *context_handle,
			    gss_buffer_t output_token)
{
	OM_uint32 ret = GSS_S_COMPLETE;
	spnego_gss_ctx_id_t *ctx =
		    (spnego_gss_ctx_id_t *)context_handle;

	*minor_status = 0;

	if (context_handle == NULL)
		return (GSS_S_FAILURE);

	if (*ctx == NULL)
		return (GSS_S_COMPLETE);

	(void) gss_delete_sec_context(minor_status, &(*ctx)->ctx_handle,
				      output_token);
	(void) release_spnego_ctx(ctx);

	return (ret);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_context_time(
			OM_uint32	*minor_status,
			const gss_ctx_id_t context_handle,
			OM_uint32	*time_rec)
{
	OM_uint32 ret;
	spnego_gss_ctx_id_t sc = (spnego_gss_ctx_id_t)context_handle;

	if (sc->ctx_handle == GSS_C_NO_CONTEXT)
		return (GSS_S_NO_CONTEXT);

	ret = gss_context_time(minor_status,
			    sc->ctx_handle,
			    time_rec);
	return (ret);
}
#ifndef LEAN_CLIENT
OM_uint32 KRB5_CALLCONV
spnego_gss_export_sec_context(
			    OM_uint32	  *minor_status,
			    gss_ctx_id_t *context_handle,
			    gss_buffer_t interprocess_token)
{
	OM_uint32 ret;
	spnego_gss_ctx_id_t sc = *(spnego_gss_ctx_id_t *)context_handle;

	/* We don't currently support exporting partially established
	 * contexts. */
	if (!sc->opened)
		return GSS_S_UNAVAILABLE;

	ret = gss_export_sec_context(minor_status,
				    &sc->ctx_handle,
				    interprocess_token);
	if (sc->ctx_handle == GSS_C_NO_CONTEXT) {
		release_spnego_ctx(&sc);
		*context_handle = GSS_C_NO_CONTEXT;
	}
	return (ret);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_import_sec_context(
	OM_uint32		*minor_status,
	const gss_buffer_t	interprocess_token,
	gss_ctx_id_t		*context_handle)
{
	OM_uint32 ret, tmpmin;
	gss_ctx_id_t mctx;
	spnego_gss_ctx_id_t sc;
	int initiate, opened;

	ret = gss_import_sec_context(minor_status, interprocess_token, &mctx);
	if (ret != GSS_S_COMPLETE)
		return ret;

	ret = gss_inquire_context(&tmpmin, mctx, NULL, NULL, NULL, NULL, NULL,
				  &initiate, &opened);
	if (ret != GSS_S_COMPLETE || !opened) {
		/* We don't currently support importing partially established
		 * contexts. */
		(void) gss_delete_sec_context(&tmpmin, &mctx, GSS_C_NO_BUFFER);
		return GSS_S_FAILURE;
	}

	sc = create_spnego_ctx(initiate);
	if (sc == NULL) {
		(void) gss_delete_sec_context(&tmpmin, &mctx, GSS_C_NO_BUFFER);
		return GSS_S_FAILURE;
	}
	sc->ctx_handle = mctx;
	sc->opened = 1;
	*context_handle = (gss_ctx_id_t)sc;
	return GSS_S_COMPLETE;
}
#endif /* LEAN_CLIENT */

OM_uint32 KRB5_CALLCONV
spnego_gss_inquire_context(
			OM_uint32	*minor_status,
			const gss_ctx_id_t context_handle,
			gss_name_t	*src_name,
			gss_name_t	*targ_name,
			OM_uint32	*lifetime_rec,
			gss_OID		*mech_type,
			OM_uint32	*ctx_flags,
			int		*locally_initiated,
			int		*opened)
{
	OM_uint32 ret = GSS_S_COMPLETE;
	spnego_gss_ctx_id_t sc = (spnego_gss_ctx_id_t)context_handle;

	if (src_name != NULL)
		*src_name = GSS_C_NO_NAME;
	if (targ_name != NULL)
		*targ_name = GSS_C_NO_NAME;
	if (lifetime_rec != NULL)
		*lifetime_rec = 0;
	if (mech_type != NULL)
		*mech_type = (gss_OID)gss_mech_spnego;
	if (ctx_flags != NULL)
		*ctx_flags = 0;
	if (locally_initiated != NULL)
		*locally_initiated = sc->initiate;
	if (opened != NULL)
		*opened = sc->opened;

	if (sc->ctx_handle != GSS_C_NO_CONTEXT) {
		ret = gss_inquire_context(minor_status, sc->ctx_handle,
					  src_name, targ_name, lifetime_rec,
					  mech_type, ctx_flags, NULL, NULL);
	}

	if (!sc->opened) {
		/*
		 * We are still doing SPNEGO negotiation, so report SPNEGO as
		 * the OID.  After negotiation is complete we will report the
		 * underlying mechanism OID.
		 */
		if (mech_type != NULL)
			*mech_type = (gss_OID)gss_mech_spnego;

		/*
		 * Remove flags we don't support with partially-established
		 * contexts.  (Change this to keep GSS_C_TRANS_FLAG if we add
		 * support for exporting partial SPNEGO contexts.)
		 */
		if (ctx_flags != NULL) {
			*ctx_flags &= ~GSS_C_PROT_READY_FLAG;
			*ctx_flags &= ~GSS_C_TRANS_FLAG;
		}
	}

	return (ret);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_wrap_size_limit(
	OM_uint32	*minor_status,
	const gss_ctx_id_t context_handle,
	int		conf_req_flag,
	gss_qop_t	qop_req,
	OM_uint32	req_output_size,
	OM_uint32	*max_input_size)
{
	OM_uint32 ret;
	spnego_gss_ctx_id_t sc = (spnego_gss_ctx_id_t)context_handle;

	if (sc->ctx_handle == GSS_C_NO_CONTEXT)
		return (GSS_S_NO_CONTEXT);

	ret = gss_wrap_size_limit(minor_status,
				sc->ctx_handle,
				conf_req_flag,
				qop_req,
				req_output_size,
				max_input_size);
	return (ret);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_localname(OM_uint32 *minor_status, const gss_name_t pname,
		     const gss_const_OID mech_type, gss_buffer_t localname)
{
	return gss_localname(minor_status, pname, GSS_C_NO_OID, localname);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_get_mic(
		OM_uint32 *minor_status,
		const gss_ctx_id_t context_handle,
		gss_qop_t  qop_req,
		const gss_buffer_t message_buffer,
		gss_buffer_t message_token)
{
	OM_uint32 ret;
	spnego_gss_ctx_id_t sc = (spnego_gss_ctx_id_t)context_handle;

	if (sc->ctx_handle == GSS_C_NO_CONTEXT)
		return (GSS_S_NO_CONTEXT);

	ret = gss_get_mic(minor_status,
		    sc->ctx_handle,
		    qop_req,
		    message_buffer,
		    message_token);
	return (ret);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_verify_mic(
		OM_uint32 *minor_status,
		const gss_ctx_id_t context_handle,
		const gss_buffer_t msg_buffer,
		const gss_buffer_t token_buffer,
		gss_qop_t *qop_state)
{
	OM_uint32 ret;
	spnego_gss_ctx_id_t sc = (spnego_gss_ctx_id_t)context_handle;

	if (sc->ctx_handle == GSS_C_NO_CONTEXT)
		return (GSS_S_NO_CONTEXT);

	ret = gss_verify_mic(minor_status,
			    sc->ctx_handle,
			    msg_buffer,
			    token_buffer,
			    qop_state);
	return (ret);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_inquire_sec_context_by_oid(
		OM_uint32 *minor_status,
		const gss_ctx_id_t context_handle,
		const gss_OID desired_object,
		gss_buffer_set_t *data_set)
{
	OM_uint32 ret;
	spnego_gss_ctx_id_t sc = (spnego_gss_ctx_id_t)context_handle;

	/* There are no SPNEGO-specific OIDs for this function. */
	if (sc->ctx_handle == GSS_C_NO_CONTEXT)
		return (GSS_S_UNAVAILABLE);

	ret = gss_inquire_sec_context_by_oid(minor_status,
			    sc->ctx_handle,
			    desired_object,
			    data_set);
	return (ret);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_inquire_cred_by_oid(
		OM_uint32 *minor_status,
		const gss_cred_id_t cred_handle,
		const gss_OID desired_object,
		gss_buffer_set_t *data_set)
{
	OM_uint32 ret;
	spnego_gss_cred_id_t spcred = (spnego_gss_cred_id_t)cred_handle;
	gss_cred_id_t mcred;
	mcred = (spcred == NULL) ? GSS_C_NO_CREDENTIAL : spcred->mcred;
	ret = gss_inquire_cred_by_oid(minor_status,
				mcred,
				desired_object,
				data_set);
	return (ret);
}

/* This is the same OID as KRB5_NO_CI_FLAGS_X_OID. */
#define NO_CI_FLAGS_X_OID_LENGTH 6
#define NO_CI_FLAGS_X_OID "\x2a\x85\x70\x2b\x0d\x1d"
static const gss_OID_desc no_ci_flags_oid[] = {
	{NO_CI_FLAGS_X_OID_LENGTH, NO_CI_FLAGS_X_OID},
};

OM_uint32 KRB5_CALLCONV
spnego_gss_set_cred_option(
		OM_uint32 *minor_status,
		gss_cred_id_t *cred_handle,
		const gss_OID desired_object,
		const gss_buffer_t value)
{
	OM_uint32 ret;
	OM_uint32 tmp_minor_status;
	spnego_gss_cred_id_t spcred = (spnego_gss_cred_id_t)*cred_handle;
	gss_cred_id_t mcred;

	mcred = (spcred == NULL) ? GSS_C_NO_CREDENTIAL : spcred->mcred;
	ret = gss_set_cred_option(minor_status,
				  &mcred,
				  desired_object,
				  value);
	if (ret == GSS_S_COMPLETE && spcred == NULL) {
		/*
		 * If the mechanism allocated a new credential handle, then
		 * we need to wrap it up in an SPNEGO credential handle.
		 */

		ret = create_spnego_cred(minor_status, mcred, &spcred);
		if (ret != GSS_S_COMPLETE) {
			gss_release_cred(&tmp_minor_status, &mcred);
			return (ret);
		}
		*cred_handle = (gss_cred_id_t)spcred;
	}

	if (ret != GSS_S_COMPLETE)
		return (ret);

	/* Recognize KRB5_NO_CI_FLAGS_X_OID and avoid asking for integrity. */
	if (g_OID_equal(desired_object, no_ci_flags_oid))
		spcred->no_ask_integ = 1;

	return (GSS_S_COMPLETE);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_set_sec_context_option(
		OM_uint32 *minor_status,
		gss_ctx_id_t *context_handle,
		const gss_OID desired_object,
		const gss_buffer_t value)
{
	OM_uint32 ret;
	spnego_gss_ctx_id_t sc = (spnego_gss_ctx_id_t)*context_handle;

	/* There are no SPNEGO-specific OIDs for this function, and we cannot
	 * construct an empty SPNEGO context with it. */
	if (sc == NULL || sc->ctx_handle == GSS_C_NO_CONTEXT)
		return (GSS_S_UNAVAILABLE);

	ret = gss_set_sec_context_option(minor_status,
			    &sc->ctx_handle,
			    desired_object,
			    value);
	return (ret);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_wrap_aead(OM_uint32 *minor_status,
		     gss_ctx_id_t context_handle,
		     int conf_req_flag,
		     gss_qop_t qop_req,
		     gss_buffer_t input_assoc_buffer,
		     gss_buffer_t input_payload_buffer,
		     int *conf_state,
		     gss_buffer_t output_message_buffer)
{
	OM_uint32 ret;
	spnego_gss_ctx_id_t sc = (spnego_gss_ctx_id_t)context_handle;

	if (sc->ctx_handle == GSS_C_NO_CONTEXT)
		return (GSS_S_NO_CONTEXT);

	ret = gss_wrap_aead(minor_status,
			    sc->ctx_handle,
			    conf_req_flag,
			    qop_req,
			    input_assoc_buffer,
			    input_payload_buffer,
			    conf_state,
			    output_message_buffer);

	return (ret);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_unwrap_aead(OM_uint32 *minor_status,
		       gss_ctx_id_t context_handle,
		       gss_buffer_t input_message_buffer,
		       gss_buffer_t input_assoc_buffer,
		       gss_buffer_t output_payload_buffer,
		       int *conf_state,
		       gss_qop_t *qop_state)
{
	OM_uint32 ret;
	spnego_gss_ctx_id_t sc = (spnego_gss_ctx_id_t)context_handle;

	if (sc->ctx_handle == GSS_C_NO_CONTEXT)
		return (GSS_S_NO_CONTEXT);

	ret = gss_unwrap_aead(minor_status,
			      sc->ctx_handle,
			      input_message_buffer,
			      input_assoc_buffer,
			      output_payload_buffer,
			      conf_state,
			      qop_state);
	return (ret);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_wrap_iov(OM_uint32 *minor_status,
		    gss_ctx_id_t context_handle,
		    int conf_req_flag,
		    gss_qop_t qop_req,
		    int *conf_state,
		    gss_iov_buffer_desc *iov,
		    int iov_count)
{
	OM_uint32 ret;
	spnego_gss_ctx_id_t sc = (spnego_gss_ctx_id_t)context_handle;

	if (sc->ctx_handle == GSS_C_NO_CONTEXT)
		return (GSS_S_NO_CONTEXT);

	ret = gss_wrap_iov(minor_status,
			   sc->ctx_handle,
			   conf_req_flag,
			   qop_req,
			   conf_state,
			   iov,
			   iov_count);
	return (ret);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_unwrap_iov(OM_uint32 *minor_status,
		      gss_ctx_id_t context_handle,
		      int *conf_state,
		      gss_qop_t *qop_state,
		      gss_iov_buffer_desc *iov,
		      int iov_count)
{
	OM_uint32 ret;
	spnego_gss_ctx_id_t sc = (spnego_gss_ctx_id_t)context_handle;

	if (sc->ctx_handle == GSS_C_NO_CONTEXT)
		return (GSS_S_NO_CONTEXT);

	ret = gss_unwrap_iov(minor_status,
			     sc->ctx_handle,
			     conf_state,
			     qop_state,
			     iov,
			     iov_count);
	return (ret);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_wrap_iov_length(OM_uint32 *minor_status,
			   gss_ctx_id_t context_handle,
			   int conf_req_flag,
			   gss_qop_t qop_req,
			   int *conf_state,
			   gss_iov_buffer_desc *iov,
			   int iov_count)
{
	OM_uint32 ret;
	spnego_gss_ctx_id_t sc = (spnego_gss_ctx_id_t)context_handle;

	if (sc->ctx_handle == GSS_C_NO_CONTEXT)
		return (GSS_S_NO_CONTEXT);

	ret = gss_wrap_iov_length(minor_status,
				  sc->ctx_handle,
				  conf_req_flag,
				  qop_req,
				  conf_state,
				  iov,
				  iov_count);
	return (ret);
}


OM_uint32 KRB5_CALLCONV
spnego_gss_complete_auth_token(
		OM_uint32 *minor_status,
		const gss_ctx_id_t context_handle,
		gss_buffer_t input_message_buffer)
{
	OM_uint32 ret;
	spnego_gss_ctx_id_t sc = (spnego_gss_ctx_id_t)context_handle;

	if (sc->ctx_handle == GSS_C_NO_CONTEXT)
		return (GSS_S_UNAVAILABLE);

	ret = gss_complete_auth_token(minor_status,
				      sc->ctx_handle,
				      input_message_buffer);
	return (ret);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_acquire_cred_impersonate_name(OM_uint32 *minor_status,
					 const gss_cred_id_t impersonator_cred_handle,
					 const gss_name_t desired_name,
					 OM_uint32 time_req,
					 gss_OID_set desired_mechs,
					 gss_cred_usage_t cred_usage,
					 gss_cred_id_t *output_cred_handle,
					 gss_OID_set *actual_mechs,
					 OM_uint32 *time_rec)
{
	OM_uint32 status, tmpmin;
	gss_OID_set amechs = GSS_C_NULL_OID_SET;
	spnego_gss_cred_id_t imp_spcred = NULL, out_spcred = NULL;
	gss_cred_id_t imp_mcred, out_mcred = GSS_C_NO_CREDENTIAL;

	dsyslog("Entering spnego_gss_acquire_cred_impersonate_name\n");

	if (actual_mechs)
		*actual_mechs = NULL;

	if (time_rec)
		*time_rec = 0;

	imp_spcred = (spnego_gss_cred_id_t)impersonator_cred_handle;
	imp_mcred = imp_spcred ? imp_spcred->mcred : GSS_C_NO_CREDENTIAL;
	status = gss_inquire_cred(minor_status, imp_mcred, NULL, NULL,
				  NULL, &amechs);
	if (status != GSS_S_COMPLETE)
		return status;

	status = gss_acquire_cred_impersonate_name(minor_status, imp_mcred,
						   desired_name, time_req,
						   amechs, cred_usage,
						   &out_mcred, actual_mechs,
						   time_rec);
	if (status != GSS_S_COMPLETE)
		goto cleanup;

	status = create_spnego_cred(minor_status, out_mcred, &out_spcred);
	if (status != GSS_S_COMPLETE)
		goto cleanup;

	out_mcred = GSS_C_NO_CREDENTIAL;
	*output_cred_handle = (gss_cred_id_t)out_spcred;

cleanup:
	(void) gss_release_oid_set(&tmpmin, &amechs);
	(void) gss_release_cred(&tmpmin, &out_mcred);

	dsyslog("Leaving spnego_gss_acquire_cred_impersonate_name\n");
	return (status);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_acquire_cred_with_password(OM_uint32 *minor_status,
				      const gss_name_t desired_name,
				      const gss_buffer_t password,
				      OM_uint32 time_req,
				      const gss_OID_set desired_mechs,
				      gss_cred_usage_t cred_usage,
				      gss_cred_id_t *output_cred_handle,
				      gss_OID_set *actual_mechs,
				      OM_uint32 *time_rec)
{
	OM_uint32 status, tmpmin;
	gss_OID_set amechs = GSS_C_NULL_OID_SET;
	gss_cred_id_t mcred = NULL;
	spnego_gss_cred_id_t spcred = NULL;

	dsyslog("Entering spnego_gss_acquire_cred_with_password\n");

	if (actual_mechs)
		*actual_mechs = NULL;

	if (time_rec)
		*time_rec = 0;

	status = get_available_mechs(minor_status, desired_name,
				     cred_usage, GSS_C_NO_CRED_STORE,
				     NULL, &amechs, NULL);
	if (status != GSS_S_COMPLETE)
	    goto cleanup;

	status = gss_acquire_cred_with_password(minor_status, desired_name,
						password, time_req, amechs,
						cred_usage, &mcred,
						actual_mechs, time_rec);
	if (status != GSS_S_COMPLETE)
	    goto cleanup;

	status = create_spnego_cred(minor_status, mcred, &spcred);
	if (status != GSS_S_COMPLETE)
		goto cleanup;

	mcred = GSS_C_NO_CREDENTIAL;
	*output_cred_handle = (gss_cred_id_t)spcred;

cleanup:

	(void) gss_release_oid_set(&tmpmin, &amechs);
	(void) gss_release_cred(&tmpmin, &mcred);

	dsyslog("Leaving spnego_gss_acquire_cred_with_password\n");
	return (status);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_display_name_ext(OM_uint32 *minor_status,
			    gss_name_t name,
			    gss_OID display_as_name_type,
			    gss_buffer_t display_name)
{
	OM_uint32 ret;
	ret = gss_display_name_ext(minor_status,
				   name,
				   display_as_name_type,
				   display_name);
	return (ret);
}


OM_uint32 KRB5_CALLCONV
spnego_gss_inquire_name(OM_uint32 *minor_status,
			gss_name_t name,
			int *name_is_MN,
			gss_OID *MN_mech,
			gss_buffer_set_t *attrs)
{
	OM_uint32 ret;
	ret = gss_inquire_name(minor_status,
			       name,
			       name_is_MN,
			       MN_mech,
			       attrs);
	return (ret);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_get_name_attribute(OM_uint32 *minor_status,
			      gss_name_t name,
			      gss_buffer_t attr,
			      int *authenticated,
			      int *complete,
			      gss_buffer_t value,
			      gss_buffer_t display_value,
			      int *more)
{
	OM_uint32 ret;
	ret = gss_get_name_attribute(minor_status,
				     name,
				     attr,
				     authenticated,
				     complete,
				     value,
				     display_value,
				     more);
	return (ret);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_set_name_attribute(OM_uint32 *minor_status,
			      gss_name_t name,
			      int complete,
			      gss_buffer_t attr,
			      gss_buffer_t value)
{
	OM_uint32 ret;
	ret = gss_set_name_attribute(minor_status,
				     name,
				     complete,
				     attr,
				     value);
	return (ret);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_delete_name_attribute(OM_uint32 *minor_status,
				 gss_name_t name,
				 gss_buffer_t attr)
{
	OM_uint32 ret;
	ret = gss_delete_name_attribute(minor_status,
					name,
					attr);
	return (ret);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_export_name_composite(OM_uint32 *minor_status,
				 gss_name_t name,
				 gss_buffer_t exp_composite_name)
{
	OM_uint32 ret;
	ret = gss_export_name_composite(minor_status,
					name,
					exp_composite_name);
	return (ret);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_map_name_to_any(OM_uint32 *minor_status,
			   gss_name_t name,
			   int authenticated,
			   gss_buffer_t type_id,
			   gss_any_t *output)
{
	OM_uint32 ret;
	ret = gss_map_name_to_any(minor_status,
				  name,
				  authenticated,
				  type_id,
				  output);
	return (ret);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_release_any_name_mapping(OM_uint32 *minor_status,
				    gss_name_t name,
				    gss_buffer_t type_id,
				    gss_any_t *input)
{
	OM_uint32 ret;
	ret = gss_release_any_name_mapping(minor_status,
					   name,
					   type_id,
					   input);
	return (ret);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_pseudo_random(OM_uint32 *minor_status,
			 gss_ctx_id_t context,
			 int prf_key,
			 const gss_buffer_t prf_in,
			 ssize_t desired_output_len,
			 gss_buffer_t prf_out)
{
	OM_uint32 ret;
	spnego_gss_ctx_id_t sc = (spnego_gss_ctx_id_t)context;

	if (sc->ctx_handle == GSS_C_NO_CONTEXT)
		return (GSS_S_NO_CONTEXT);

	ret = gss_pseudo_random(minor_status,
				sc->ctx_handle,
				prf_key,
				prf_in,
				desired_output_len,
				prf_out);
        return (ret);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_set_neg_mechs(OM_uint32 *minor_status,
			 gss_cred_id_t cred_handle,
			 const gss_OID_set mech_list)
{
	OM_uint32 ret;
	spnego_gss_cred_id_t spcred = (spnego_gss_cred_id_t)cred_handle;

	/* Store mech_list in spcred for use in negotiation logic. */
	gss_release_oid_set(minor_status, &spcred->neg_mechs);
	ret = generic_gss_copy_oid_set(minor_status, mech_list,
				       &spcred->neg_mechs);
	if (ret == GSS_S_COMPLETE) {
		(void) gss_set_neg_mechs(minor_status,
					 spcred->mcred,
					 spcred->neg_mechs);
	}

	return (ret);
}

#define SPNEGO_SASL_NAME	"SPNEGO"
#define SPNEGO_SASL_NAME_LEN	(sizeof(SPNEGO_SASL_NAME) - 1)

OM_uint32 KRB5_CALLCONV
spnego_gss_inquire_mech_for_saslname(OM_uint32 *minor_status,
                                     const gss_buffer_t sasl_mech_name,
                                     gss_OID *mech_type)
{
	if (sasl_mech_name->length == SPNEGO_SASL_NAME_LEN &&
	    memcmp(sasl_mech_name->value, SPNEGO_SASL_NAME,
		   SPNEGO_SASL_NAME_LEN) == 0) {
		if (mech_type != NULL)
			*mech_type = (gss_OID)gss_mech_spnego;
		return (GSS_S_COMPLETE);
	}

	return (GSS_S_BAD_MECH);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_inquire_saslname_for_mech(OM_uint32 *minor_status,
                                     const gss_OID desired_mech,
                                     gss_buffer_t sasl_mech_name,
                                     gss_buffer_t mech_name,
                                     gss_buffer_t mech_description)
{
	*minor_status = 0;

	if (!g_OID_equal(desired_mech, gss_mech_spnego))
		return (GSS_S_BAD_MECH);

	if (!g_make_string_buffer(SPNEGO_SASL_NAME, sasl_mech_name) ||
	    !g_make_string_buffer("spnego", mech_name) ||
	    !g_make_string_buffer("Simple and Protected GSS-API "
				  "Negotiation Mechanism", mech_description))
		goto fail;

	return (GSS_S_COMPLETE);

fail:
	*minor_status = ENOMEM;
	return (GSS_S_FAILURE);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_inquire_attrs_for_mech(OM_uint32 *minor_status,
				  gss_const_OID mech,
				  gss_OID_set *mech_attrs,
				  gss_OID_set *known_mech_attrs)
{
	OM_uint32 major, tmpMinor;

	/* known_mech_attrs is handled by mechglue */
	*minor_status = 0;

	if (mech_attrs == NULL)
	    return (GSS_S_COMPLETE);

	major = gss_create_empty_oid_set(minor_status, mech_attrs);
	if (GSS_ERROR(major))
		goto cleanup;

#define MA_SUPPORTED(ma)    do {					\
		major = gss_add_oid_set_member(minor_status,		\
					       (gss_OID)ma, mech_attrs); \
		if (GSS_ERROR(major))					\
			goto cleanup;					\
	} while (0)

	MA_SUPPORTED(GSS_C_MA_MECH_NEGO);
	MA_SUPPORTED(GSS_C_MA_ITOK_FRAMED);

cleanup:
	if (GSS_ERROR(major))
		gss_release_oid_set(&tmpMinor, mech_attrs);

	return (major);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_export_cred(OM_uint32 *minor_status,
		       gss_cred_id_t cred_handle,
		       gss_buffer_t token)
{
	spnego_gss_cred_id_t spcred = (spnego_gss_cred_id_t)cred_handle;

	return (gss_export_cred(minor_status, spcred->mcred, token));
}

OM_uint32 KRB5_CALLCONV
spnego_gss_import_cred(OM_uint32 *minor_status,
		       gss_buffer_t token,
		       gss_cred_id_t *cred_handle)
{
	OM_uint32 ret;
	spnego_gss_cred_id_t spcred;
	gss_cred_id_t mcred;

	ret = gss_import_cred(minor_status, token, &mcred);
	if (GSS_ERROR(ret))
		return (ret);

	ret = create_spnego_cred(minor_status, mcred, &spcred);
	if (GSS_ERROR(ret))
	    return (ret);

	*cred_handle = (gss_cred_id_t)spcred;
	return (ret);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_get_mic_iov(OM_uint32 *minor_status, gss_ctx_id_t context_handle,
		       gss_qop_t qop_req, gss_iov_buffer_desc *iov,
		       int iov_count)
{
    spnego_gss_ctx_id_t sc = (spnego_gss_ctx_id_t)context_handle;

    if (sc->ctx_handle == GSS_C_NO_CONTEXT)
	    return (GSS_S_NO_CONTEXT);

    return gss_get_mic_iov(minor_status, sc->ctx_handle, qop_req, iov,
			   iov_count);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_verify_mic_iov(OM_uint32 *minor_status, gss_ctx_id_t context_handle,
			  gss_qop_t *qop_state, gss_iov_buffer_desc *iov,
			  int iov_count)
{
    spnego_gss_ctx_id_t sc = (spnego_gss_ctx_id_t)context_handle;

    if (sc->ctx_handle == GSS_C_NO_CONTEXT)
	    return (GSS_S_NO_CONTEXT);

    return gss_verify_mic_iov(minor_status, sc->ctx_handle, qop_state, iov,
			      iov_count);
}

OM_uint32 KRB5_CALLCONV
spnego_gss_get_mic_iov_length(OM_uint32 *minor_status,
			      gss_ctx_id_t context_handle, gss_qop_t qop_req,
			      gss_iov_buffer_desc *iov, int iov_count)
{
    spnego_gss_ctx_id_t sc = (spnego_gss_ctx_id_t)context_handle;

    if (sc->ctx_handle == GSS_C_NO_CONTEXT)
	    return (GSS_S_NO_CONTEXT);

    return gss_get_mic_iov_length(minor_status, sc->ctx_handle, qop_req, iov,
				  iov_count);
}

/*
 * We will release everything but the ctx_handle so that it
 * can be passed back to init/accept context. This routine should
 * not be called until after the ctx_handle memory is assigned to
 * the supplied context handle from init/accept context.
 */
static void
release_spnego_ctx(spnego_gss_ctx_id_t *ctx)
{
	spnego_gss_ctx_id_t context;
	OM_uint32 minor_stat;
	context = *ctx;

	if (context != NULL) {
		(void) gss_release_buffer(&minor_stat,
					&context->DER_mechTypes);

		(void) gss_release_oid_set(&minor_stat, &context->mech_set);

		(void) gss_release_name(&minor_stat, &context->internal_name);
		(void) gss_release_cred(&minor_stat, &context->deleg_cred);

		negoex_release_context(context);

		free(context);
		*ctx = NULL;
	}
}

/*
 * Can't use gss_indicate_mechs by itself to get available mechs for
 * SPNEGO because it will also return the SPNEGO mech and we do not
 * want to consider SPNEGO as an available security mech for
 * negotiation. For this reason, get_available_mechs will return
 * all available, non-deprecated mechs except SPNEGO and NegoEx-
 * only mechanisms.
 *
 * Note that gss_acquire_cred_from(GSS_C_NO_OID_SET) will filter
 * out hidden (GSS_C_MA_NOT_INDICATED) mechanisms such as NegoEx, so
 * calling gss_indicate_mechs_by_attrs() also works around that.
 *
 * If a ptr to a creds list is given, this function will attempt
 * to acquire creds for the creds given and trim the list of
 * returned mechanisms to only those for which creds are valid.
 *
 */
static OM_uint32
get_available_mechs(OM_uint32 *minor_status,
	gss_name_t name, gss_cred_usage_t usage,
	gss_const_key_value_set_t cred_store,
	gss_cred_id_t *creds, gss_OID_set *rmechs, OM_uint32 *time_rec)
{
	OM_uint32 major_status = GSS_S_COMPLETE, tmpmin;
	gss_OID_set mechs, goodmechs;
	gss_OID_set_desc except_attrs;
	gss_OID_desc attr_oids[3];

	*rmechs = GSS_C_NO_OID_SET;

	attr_oids[0] = *GSS_C_MA_DEPRECATED;
	attr_oids[1] = *GSS_C_MA_NOT_DFLT_MECH;
	attr_oids[2] = *GSS_C_MA_MECH_NEGO;     /* Exclude ourselves */
	except_attrs.count = sizeof(attr_oids) / sizeof(attr_oids[0]);
	except_attrs.elements = attr_oids;
	major_status = gss_indicate_mechs_by_attrs(minor_status,
						   GSS_C_NO_OID_SET,
						   &except_attrs,
						   GSS_C_NO_OID_SET, &mechs);

	/*
	 * If the caller wanted a list of creds returned,
	 * trim the list of mechanisms down to only those
	 * for which the creds are valid.
	 */
	if (mechs->count > 0 && major_status == GSS_S_COMPLETE &&
	    creds != NULL) {
		major_status = gss_acquire_cred_from(minor_status, name,
						     GSS_C_INDEFINITE,
						     mechs, usage,
						     cred_store, creds,
						     &goodmechs, time_rec);

		/*
		 * Drop the old list in favor of the new
		 * "trimmed" list.
		 */
		if (major_status == GSS_S_COMPLETE) {
			(void) gss_release_oid_set(&tmpmin, &mechs);
			mechs = goodmechs;
		}
	}

	if (mechs->count > 0 && major_status == GSS_S_COMPLETE) {
		*rmechs = mechs;
	} else {
		(void) gss_release_oid_set(&tmpmin, &mechs);
		*minor_status = ERR_SPNEGO_NO_MECHS_AVAILABLE;
		map_errcode(minor_status);
		if (major_status == GSS_S_COMPLETE)
			major_status = GSS_S_FAILURE;
	}

	return (major_status);
}

/* Return true if mech asserts the GSS_C_MA_NEGOEX_AND_SPNEGO attribute. */
static int
negoex_and_spnego(gss_OID mech)
{
	OM_uint32 ret, minor;
	gss_OID_set attrs;
	int present;

	ret = gss_inquire_attrs_for_mech(&minor, mech, &attrs, NULL);
	if (ret != GSS_S_COMPLETE || attrs == GSS_C_NO_OID_SET)
		return 0;

	(void) generic_gss_test_oid_set_member(&minor,
					       GSS_C_MA_NEGOEX_AND_SPNEGO,
					       attrs, &present);
	(void) gss_release_oid_set(&minor, &attrs);
	return present;
}

/*
 * Fill sc->mech_set with the SPNEGO-negotiable mechanism OIDs, and
 * sc->negoex_mechs with an entry for each NegoEx-negotiable mechanism.  Take
 * into account the mech set provided with gss_set_neg_mechs() if it exists.
 */
static OM_uint32
get_negotiable_mechs(OM_uint32 *minor_status, spnego_gss_ctx_id_t sc,
		     spnego_gss_cred_id_t spcred, gss_cred_usage_t usage)
{
	OM_uint32 ret, tmpmin;
	gss_cred_id_t creds = GSS_C_NO_CREDENTIAL;
	gss_OID_set cred_mechs = GSS_C_NULL_OID_SET, mechs;
	unsigned int i;
	int present, added_negoex = 0;
	auth_scheme scheme;

	if (spcred != NULL) {
		/* Get the list of mechs in the mechglue cred. */
		ret = gss_inquire_cred(minor_status, spcred->mcred, NULL,
				       NULL, NULL, &cred_mechs);
		if (ret != GSS_S_COMPLETE)
			return (ret);
	} else {
		/* Start with the list of available mechs. */
		ret = get_available_mechs(minor_status, GSS_C_NO_NAME, usage,
					  GSS_C_NO_CRED_STORE, &creds,
					  &cred_mechs, NULL);
		if (ret != GSS_S_COMPLETE)
			return (ret);
		gss_release_cred(&tmpmin, &creds);
	}

	/* If gss_set_neg_mechs() was called, use that to determine the
	 * iteration order.  Otherwise iterate over the credential mechs. */
	mechs = (spcred != NULL && spcred->neg_mechs != GSS_C_NULL_OID_SET) ?
	    spcred->neg_mechs : cred_mechs;

	ret = gss_create_empty_oid_set(minor_status, &sc->mech_set);
	if (ret != GSS_S_COMPLETE)
		goto cleanup;

	for (i = 0; i < mechs->count; i++) {
		if (mechs != cred_mechs) {
			/* Intersect neg_mechs with cred_mechs. */
			gss_test_oid_set_member(&tmpmin, &mechs->elements[i],
						cred_mechs, &present);
			if (!present)
				continue;
		}

		/* Query the auth scheme to see if this is a NegoEx mech. */
		ret = gssspi_query_mechanism_info(&tmpmin, &mechs->elements[i],
						  scheme);
		if (ret == GSS_S_COMPLETE) {
			/* Add an entry for this mech to the NegoEx list. */
			ret = negoex_add_auth_mech(minor_status, sc,
						   &mechs->elements[i],
						   scheme);
			if (ret != GSS_S_COMPLETE)
				goto cleanup;

			/* Add the NegoEx OID to the SPNEGO list at the
			 * position of the first NegoEx mechanism. */
			if (!added_negoex) {
				ret = gss_add_oid_set_member(minor_status,
							     &negoex_mech,
							     &sc->mech_set);
				if (ret != GSS_S_COMPLETE)
					goto cleanup;
				added_negoex = 1;
			}

			/* Skip this mech in the SPNEGO list unless it asks for
			 * direct SPNEGO negotiation. */
			if (!negoex_and_spnego(&mechs->elements[i]))
				continue;
		}

		/* Add this mech to the SPNEGO list. */
		ret = gss_add_oid_set_member(minor_status, &mechs->elements[i],
					     &sc->mech_set);
		if (ret != GSS_S_COMPLETE)
			goto cleanup;
	}

	*minor_status = 0;

cleanup:
	if (ret != GSS_S_COMPLETE || sc->mech_set->count == 0) {
		*minor_status = ERR_SPNEGO_NO_MECHS_AVAILABLE;
		map_errcode(minor_status);
		ret = GSS_S_FAILURE;
	}

	gss_release_oid_set(&tmpmin, &cred_mechs);
	return (ret);
}

/* following are token creation and reading routines */

/*
 * If in contains a tagged OID encoding, return a copy of the contents as a
 * gss_OID and advance in past the encoding.  Otherwise return NULL and do not
 * advance in.
 */
static gss_OID
get_mech_oid(OM_uint32 *minor_status, struct k5input *in)
{
	struct k5input oidrep;
	OM_uint32 status;
	gss_OID_desc oid;
	gss_OID mech_out = NULL;

	if (!k5_der_get_value(in, MECH_OID, &oidrep))
		return (NULL);

	oid.length = oidrep.len;
	oid.elements = (uint8_t *)oidrep.ptr;
	status = generic_gss_copy_oid(minor_status, &oid, &mech_out);
	if (status != GSS_S_COMPLETE) {
		map_errcode(minor_status);
		mech_out = NULL;
	}

	return (mech_out);
}

/*
 * If in contains a tagged octet string encoding, return a copy of the contents
 * as a gss_buffer_t and advance in past the encoding.  Otherwise return NULL
 * and do not advance in.
 */
static gss_buffer_t
get_octet_string(struct k5input *in)
{
	gss_buffer_t input_token;
	struct k5input ostr;

	if (!k5_der_get_value(in, OCTET_STRING, &ostr))
		return (NULL);

	input_token = (gss_buffer_t)malloc(sizeof (gss_buffer_desc));
	if (input_token == NULL)
		return (NULL);

	input_token->length = ostr.len;
	if (input_token->length > 0) {
		input_token->value = gssalloc_malloc(input_token->length);
		if (input_token->value == NULL) {
			free(input_token);
			return (NULL);
		}

		memcpy(input_token->value, ostr.ptr, input_token->length);
	} else {
		input_token->value = NULL;
	}
	return (input_token);
}

/*
 * verify that buff_in points to a sequence of der encoding. The mech
 * set is the only sequence of encoded object in the token, so if it is
 * a sequence of encoding, decode the mechset into a gss_OID_set and
 * return it, advancing the buffer pointer.
 */
static gss_OID_set
get_mech_set(OM_uint32 *minor_status, struct k5input *in)
{
	gss_OID_set returned_mechSet;
	OM_uint32 major_status, tmpmin;
	struct k5input seq;

	if (!k5_der_get_value(in, SEQUENCE_OF, &seq))
		return (NULL);

	major_status = gss_create_empty_oid_set(minor_status,
						&returned_mechSet);
	if (major_status != GSS_S_COMPLETE)
		return (NULL);

	while (!seq.status && seq.len > 0) {
		gss_OID_desc *oid = get_mech_oid(minor_status, &seq);

		if (oid == NULL) {
			gss_release_oid_set(&tmpmin, &returned_mechSet);
			return (NULL);
		}

		major_status = gss_add_oid_set_member(minor_status,
						      oid, &returned_mechSet);
		generic_gss_release_oid(minor_status, &oid);
		if (major_status != GSS_S_COMPLETE) {
			gss_release_oid_set(&tmpmin, &returned_mechSet);
			return (NULL);
		}
	}

	return (returned_mechSet);
}

/*
 * Encode mechSet into buf.
 */
static int
put_mech_set(gss_OID_set mechSet, gss_buffer_t buffer_out)
{
	uint8_t *ptr;
	size_t ilen, tlen, i;
	struct k5buf buf;

	ilen = 0;
	for (i = 0; i < mechSet->count; i++)
	    ilen += k5_der_value_len(mechSet->elements[i].length);
	tlen = k5_der_value_len(ilen);

	ptr = gssalloc_malloc(tlen);
	if (ptr == NULL)
		return -1;
	k5_buf_init_fixed(&buf, ptr, tlen);

	k5_der_add_taglen(&buf, SEQUENCE_OF, ilen);
	for (i = 0; i < mechSet->count; i++) {
		k5_der_add_value(&buf, MECH_OID,
				 mechSet->elements[i].elements,
				 mechSet->elements[i].length);
	}
	assert(buf.len == tlen);

	buffer_out->value = ptr;
	buffer_out->length = tlen;
	return 0;
}

/* Decode SPNEGO request flags from the DER encoding of a bit string and set
 * them in *ret_flags. */
static OM_uint32
get_req_flags(struct k5input *in, OM_uint32 *req_flags)
{
	if (in->status || in->len != 4 ||
	    k5_input_get_byte(in) != BIT_STRING ||
	    k5_input_get_byte(in) != BIT_STRING_LENGTH ||
	    k5_input_get_byte(in) != BIT_STRING_PADDING)
		return GSS_S_DEFECTIVE_TOKEN;

	*req_flags = k5_input_get_byte(in) >> 1;
	return GSS_S_COMPLETE;
}

static OM_uint32
get_negTokenInit(OM_uint32 *minor_status,
		 gss_buffer_t buf,
		 gss_buffer_t der_mechSet,
		 gss_OID_set *mechSet,
		 OM_uint32 *req_flags,
		 gss_buffer_t *mechtok,
		 gss_buffer_t *mechListMIC)
{
	OM_uint32 err;
	struct k5input in, seq, field;

	*minor_status = 0;
	der_mechSet->length = 0;
	der_mechSet->value = NULL;
	*mechSet = GSS_C_NO_OID_SET;
	*req_flags = 0;
	*mechtok = *mechListMIC = GSS_C_NO_BUFFER;

	k5_input_init(&in, buf->value, buf->length);

	/* Advance past the framing header. */
	err = verify_token_header(&in, gss_mech_spnego);
	if (err)
		return GSS_S_DEFECTIVE_TOKEN;

	/* Advance past the [0] tag for the NegotiationToken choice. */
	if (!k5_der_get_value(&in, CONTEXT, &seq))
		return GSS_S_DEFECTIVE_TOKEN;

	/* Advance past the SEQUENCE tag. */
	if (!k5_der_get_value(&seq, SEQUENCE, &seq))
		return GSS_S_DEFECTIVE_TOKEN;

	/* Get the contents of the mechTypes field.  Reject an empty field here
	 * since we musn't allocate a zero-length buffer in the next step. */
	if (!k5_der_get_value(&seq, CONTEXT, &field) || field.len == 0)
		return GSS_S_DEFECTIVE_TOKEN;

	/* Store a copy of the contents for MIC computation. */
	der_mechSet->value = gssalloc_malloc(field.len);
	if (der_mechSet->value == NULL)
		return GSS_S_FAILURE;
	memcpy(der_mechSet->value, field.ptr, field.len);
	der_mechSet->length = field.len;

	/* Decode the contents into an OID set. */
	*mechSet = get_mech_set(minor_status, &field);
	if (*mechSet == NULL)
		return GSS_S_FAILURE;

	if (k5_der_get_value(&seq, CONTEXT | 0x01, &field)) {
		err = get_req_flags(&field, req_flags);
		if (err != GSS_S_COMPLETE)
			return err;
	}

	if (k5_der_get_value(&seq, CONTEXT | 0x02, &field)) {
		*mechtok = get_octet_string(&field);
		if (*mechtok == GSS_C_NO_BUFFER)
			return GSS_S_FAILURE;
	}

	if (k5_der_get_value(&seq, CONTEXT | 0x03, &field)) {
		*mechListMIC = get_octet_string(&field);
		if (*mechListMIC == GSS_C_NO_BUFFER)
			return GSS_S_FAILURE;
	}

	return seq.status ? GSS_S_DEFECTIVE_TOKEN : GSS_S_COMPLETE;
}

/* Decode a NegotiationToken of type negTokenResp. */
static OM_uint32
get_negTokenResp(OM_uint32 *minor_status, struct k5input *in,
		 OM_uint32 *negState, gss_OID *supportedMech,
		 gss_buffer_t *responseToken, gss_buffer_t *mechListMIC)
{
	struct k5input seq, field, en;

	*negState = UNSPECIFIED;
	*supportedMech = GSS_C_NO_OID;
	*responseToken = *mechListMIC = GSS_C_NO_BUFFER;

	/* Advance past the [1] tag for the NegotiationToken choice. */
	if (!k5_der_get_value(in, CONTEXT | 0x01, &seq))
		return GSS_S_DEFECTIVE_TOKEN;

	/* Advance seq past the SEQUENCE tag (historically this code allows the
	 * tag to be missing). */
	(void)k5_der_get_value(&seq, SEQUENCE, &seq);

	if (k5_der_get_value(&seq, CONTEXT, &field)) {
		if (!k5_der_get_value(&field, ENUMERATED, &en))
			return GSS_S_DEFECTIVE_TOKEN;
		if (en.len != ENUMERATION_LENGTH)
			return GSS_S_DEFECTIVE_TOKEN;
		*negState = *en.ptr;
	}

	if (k5_der_get_value(&seq, CONTEXT | 0x01, &field)) {
		*supportedMech = get_mech_oid(minor_status, &field);
		if (*supportedMech == GSS_C_NO_OID)
			return GSS_S_DEFECTIVE_TOKEN;
	}

	if (k5_der_get_value(&seq, CONTEXT | 0x02, &field)) {
		*responseToken = get_octet_string(&field);
		if (*responseToken == GSS_C_NO_BUFFER)
			return GSS_S_DEFECTIVE_TOKEN;
	}

	if (k5_der_get_value(&seq, CONTEXT | 0x04, &field)) {
		*mechListMIC = get_octet_string(&field);

                /* Handle Windows 2000 duplicate response token */
                if (*responseToken &&
                    ((*responseToken)->length == (*mechListMIC)->length) &&
                    !memcmp((*responseToken)->value, (*mechListMIC)->value,
                            (*responseToken)->length)) {
			OM_uint32 tmpmin;

			gss_release_buffer(&tmpmin, *mechListMIC);
			free(*mechListMIC);
			*mechListMIC = NULL;
		}
	}

	return seq.status ? GSS_S_DEFECTIVE_TOKEN : GSS_S_COMPLETE;
}

/*
 * This routine compares the received mechset to the mechset that
 * this server can support. It looks sequentially through the mechset
 * and the first one that matches what the server can support is
 * chosen as the negotiated mechanism. If one is found, negResult
 * is set to ACCEPT_INCOMPLETE if it's the first mech, REQUEST_MIC if
 * it's not the first mech, otherwise we return NULL and negResult
 * is set to REJECT. The returned pointer is an alias into
 * received->elements and should not be freed.
 *
 * NOTE: There is currently no way to specify a preference order of
 * mechanisms supported by the acceptor.
 */
static gss_OID
negotiate_mech(spnego_gss_ctx_id_t ctx, gss_OID_set received,
	       OM_uint32 *negResult)
{
	size_t i, j;
	int wrong_krb5_oid;

	for (i = 0; i < received->count; i++) {
		gss_OID mech_oid = &received->elements[i];

		/* Accept wrong mechanism OID from MS clients */
		wrong_krb5_oid = 0;
		if (g_OID_equal(mech_oid, &gss_mech_krb5_wrong_oid)) {
			mech_oid = (gss_OID)&gss_mech_krb5_oid;
			wrong_krb5_oid = 1;
		}

		for (j = 0; j < ctx->mech_set->count; j++) {
			if (g_OID_equal(mech_oid,
					&ctx->mech_set->elements[j])) {
				*negResult = (i == 0) ? ACCEPT_INCOMPLETE :
					REQUEST_MIC;
				return wrong_krb5_oid ?
					(gss_OID)&gss_mech_krb5_wrong_oid :
					&ctx->mech_set->elements[j];
			}
		}
	}
	*negResult = REJECT;
	return (NULL);
}

/*
 * the next two routines make a token buffer suitable for
 * spnego_gss_display_status. These currently take the string
 * in name and place it in the token. Eventually, if
 * spnego_gss_display_status returns valid error messages,
 * these routines will be changes to return the error string.
 */
static spnego_token_t
make_spnego_token(const char *name)
{
	return (spnego_token_t)gssalloc_strdup(name);
}

static gss_buffer_desc
make_err_msg(const char *name)
{
	gss_buffer_desc buffer;

	if (name == NULL) {
		buffer.length = 0;
		buffer.value = NULL;
	} else {
		buffer.length = strlen(name)+1;
		buffer.value = make_spnego_token(name);
	}

	return (buffer);
}

/*
 * Create the client side spnego token passed back to gss_init_sec_context
 * and eventually up to the application program and over to the server.
 *
 * Use DER rules, definite length method per RFC 2478
 */
static int
make_spnego_tokenInit_msg(spnego_gss_ctx_id_t spnego_ctx, int negHintsCompat,
			  gss_buffer_t mic, OM_uint32 req_flags,
			  gss_buffer_t token, send_token_flag sendtoken,
			  gss_buffer_t outbuf)
{
	size_t f0len, f2len, f3len, fields_len, seq_len, choice_len;
	size_t mech_len, framed_len;
	uint8_t *t;
	struct k5buf buf;

	if (outbuf == GSS_C_NO_BUFFER)
		return (-1);

	outbuf->length = 0;
	outbuf->value = NULL;

	/* Calculate the length of each field and the total fields length. */
	fields_len = 0;
	/* mechTypes [0] MechTypeList, previously assembled in spnego_ctx */
	f0len = spnego_ctx->DER_mechTypes.length;
	fields_len += k5_der_value_len(f0len);
	if (token != NULL) {
		/* mechToken [2] OCTET STRING OPTIONAL */
		f2len = k5_der_value_len(token->length);
		fields_len += k5_der_value_len(f2len);
	}
	if (mic != GSS_C_NO_BUFFER) {
		/* mechListMIC [3] OCTET STRING OPTIONAL */
		f3len = k5_der_value_len(mic->length);
		fields_len += k5_der_value_len(f3len);
	}

	/* Calculate the length of the sequence and choice. */
	seq_len = k5_der_value_len(fields_len);
	choice_len = k5_der_value_len(seq_len);

	/* Calculate the framed token length. */
	mech_len = k5_der_value_len(gss_mech_spnego->length);
	framed_len = k5_der_value_len(mech_len + choice_len);

	/* Allocate space and prepare a buffer. */
	t = gssalloc_malloc(framed_len);
	if (t == NULL)
		return (-1);
	k5_buf_init_fixed(&buf, t, framed_len);

	/* Add generic token framing. */
	k5_der_add_taglen(&buf, HEADER_ID, mech_len + choice_len);
	k5_der_add_value(&buf, MECH_OID, gss_mech_spnego->elements,
			 gss_mech_spnego->length);

	/* Add NegotiationToken choice tag and NegTokenInit sequence tag. */
	k5_der_add_taglen(&buf, CONTEXT | 0x00, seq_len);
	k5_der_add_taglen(&buf, SEQUENCE, fields_len);

	/* Add the already-encoded mechanism list as mechTypes. */
	k5_der_add_value(&buf, CONTEXT | 0x00, spnego_ctx->DER_mechTypes.value,
			 spnego_ctx->DER_mechTypes.length);

	if (token != NULL) {
		k5_der_add_taglen(&buf, CONTEXT | 0x02, f2len);
		k5_der_add_value(&buf, OCTET_STRING, token->value,
				 token->length);
	}

	if (mic != GSS_C_NO_BUFFER) {
		uint8_t id = negHintsCompat ? SEQUENCE : OCTET_STRING;
		k5_der_add_taglen(&buf, CONTEXT | 0x03, f3len);
		k5_der_add_value(&buf, id, mic->value, mic->length);
	}

	assert(buf.len == framed_len);
	outbuf->length = framed_len;
	outbuf->value = t;

	return (0);
}

/*
 * create the server side spnego token passed back to
 * gss_accept_sec_context and eventually up to the application program
 * and over to the client.
 */
static OM_uint32
make_spnego_tokenTarg_msg(uint8_t status, gss_OID mech_wanted,
			  gss_buffer_t token, gss_buffer_t mic,
			  send_token_flag sendtoken,
			  gss_buffer_t outbuf)
{
	size_t f0len, f1len, f2len, f3len, fields_len, seq_len, choice_len;
	uint8_t *t;
	struct k5buf buf;

	if (outbuf == GSS_C_NO_BUFFER)
		return (GSS_S_DEFECTIVE_TOKEN);
	if (sendtoken == INIT_TOKEN_SEND && mech_wanted == GSS_C_NO_OID)
		return (GSS_S_DEFECTIVE_TOKEN);

	outbuf->length = 0;
	outbuf->value = NULL;

	/* Calculate the length of each field and the total fields length. */
	fields_len = 0;
	/* negState [0] ENUMERATED { ... } OPTIONAL */
	f0len = k5_der_value_len(1);
	fields_len += k5_der_value_len(f0len);
	if (sendtoken == INIT_TOKEN_SEND) {
		/* supportedMech [1] MechType OPTIONAL */
		f1len = k5_der_value_len(mech_wanted->length);
		fields_len += k5_der_value_len(f1len);
	}
	if (token != NULL && token->length > 0) {
		/* mechToken [2] OCTET STRING OPTIONAL */
		f2len = k5_der_value_len(token->length);
		fields_len += k5_der_value_len(f2len);
	}
	if (mic != NULL) {
		/* mechListMIC [3] OCTET STRING OPTIONAL */
		f3len = k5_der_value_len(mic->length);
		fields_len += k5_der_value_len(f3len);
	}

	/* Calculate the length of the sequence and choice. */
	seq_len = k5_der_value_len(fields_len);
	choice_len = k5_der_value_len(seq_len);

	/* Allocate space and prepare a buffer. */
	t = gssalloc_malloc(choice_len);
	if (t == NULL)
		return (GSS_S_DEFECTIVE_TOKEN);
	k5_buf_init_fixed(&buf, t, choice_len);

	/* Add the choice tag and begin the sequence. */
	k5_der_add_taglen(&buf, CONTEXT | 0x01, seq_len);
	k5_der_add_taglen(&buf, SEQUENCE, fields_len);

	/* Add the negState field. */
	k5_der_add_taglen(&buf, CONTEXT | 0x00, f0len);
	k5_der_add_value(&buf, ENUMERATED, &status, 1);

	if (sendtoken == INIT_TOKEN_SEND) {
		/* Add the supportedMech field. */
		k5_der_add_taglen(&buf, CONTEXT | 0x01, f1len);
		k5_der_add_value(&buf, MECH_OID, mech_wanted->elements,
				 mech_wanted->length);
	}

	if (token != NULL && token->length > 0) {
		/* Add the mechToken field. */
		k5_der_add_taglen(&buf, CONTEXT | 0x02, f2len);
		k5_der_add_value(&buf, OCTET_STRING, token->value,
				 token->length);
	}

	if (mic != NULL) {
		/* Add the mechListMIC field. */
		k5_der_add_taglen(&buf, CONTEXT | 0x03, f3len);
		k5_der_add_value(&buf, OCTET_STRING, mic->value, mic->length);
	}

	assert(buf.len == choice_len);
	outbuf->length = choice_len;
	outbuf->value = t;

	return (0);
}

/* Advance in past the [APPLICATION 0] tag and thisMech field of an
 * InitialContextToken encoding, checking that thisMech matches mech. */
static int
verify_token_header(struct k5input *in, gss_OID_const mech)
{
	gss_OID_desc oid;
	struct k5input field;

	if (!k5_der_get_value(in, HEADER_ID, in))
		return (G_BAD_TOK_HEADER);
	if (!k5_der_get_value(in, MECH_OID, &field))
		return (G_BAD_TOK_HEADER);

	oid.length = field.len;
	oid.elements = (uint8_t *)field.ptr;
	return g_OID_equal(&oid, mech) ? 0 : G_WRONG_MECH;
}

/*
 * Return non-zero if the oid is one of the kerberos mech oids,
 * otherwise return zero.
 *
 * N.B. There are 3 oids that represent the kerberos mech:
 * RFC-specified GSS_MECH_KRB5_OID,
 * Old pre-RFC   GSS_MECH_KRB5_OLD_OID,
 * Incorrect MS  GSS_MECH_KRB5_WRONG_OID
 */

static int
is_kerb_mech(gss_OID oid)
{
	int answer = 0;
	OM_uint32 minor;
	extern const gss_OID_set_desc * const gss_mech_set_krb5_both;

	(void) gss_test_oid_set_member(&minor,
		oid, (gss_OID_set)gss_mech_set_krb5_both, &answer);

	return (answer);
}
