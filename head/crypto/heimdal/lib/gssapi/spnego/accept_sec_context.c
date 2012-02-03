/*
 * Copyright (c) 1997 - 2006 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * Portions Copyright (c) 2004 PADL Software Pty Ltd.
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

#include "spnego/spnego_locl.h"

RCSID("$Id: accept_sec_context.c 21461 2007-07-10 14:01:13Z lha $");
/* $FreeBSD$ */

static OM_uint32
send_reject (OM_uint32 *minor_status,
	     gss_buffer_t output_token)
{
    NegotiationToken nt;
    size_t size;

    nt.element = choice_NegotiationToken_negTokenResp;

    ALLOC(nt.u.negTokenResp.negResult, 1);
    if (nt.u.negTokenResp.negResult == NULL) {
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }
    *(nt.u.negTokenResp.negResult)  = reject;
    nt.u.negTokenResp.supportedMech = NULL;
    nt.u.negTokenResp.responseToken = NULL;
    nt.u.negTokenResp.mechListMIC   = NULL;
    
    ASN1_MALLOC_ENCODE(NegotiationToken,
		       output_token->value, output_token->length, &nt,
		       &size, *minor_status);
    free_NegotiationToken(&nt);
    if (*minor_status != 0)
	return GSS_S_FAILURE;

    return GSS_S_BAD_MECH;
}

static OM_uint32
acceptor_approved(gss_name_t target_name, gss_OID mech)
{
    gss_cred_id_t cred = GSS_C_NO_CREDENTIAL;
    gss_OID_set oidset;
    OM_uint32 junk, ret;

    if (target_name == GSS_C_NO_NAME)
	return GSS_S_COMPLETE;

    gss_create_empty_oid_set(&junk, &oidset);
    gss_add_oid_set_member(&junk, mech, &oidset);
    
    ret = gss_acquire_cred(&junk, target_name, GSS_C_INDEFINITE, oidset,
			   GSS_C_ACCEPT, &cred, NULL, NULL);
    gss_release_oid_set(&junk, &oidset);
    if (ret != GSS_S_COMPLETE)
	return ret;
    gss_release_cred(&junk, &cred);
    
    return GSS_S_COMPLETE;
}

static OM_uint32
send_supported_mechs (OM_uint32 *minor_status,
		      gss_buffer_t output_token)
{
    NegotiationTokenWin nt;
    char hostname[MAXHOSTNAMELEN + 1], *p;
    gss_buffer_desc name_buf;
    gss_OID name_type;
    gss_name_t target_princ;
    gss_name_t canon_princ;
    OM_uint32 minor;
    size_t buf_len;
    gss_buffer_desc data;
    OM_uint32 ret;

    memset(&nt, 0, sizeof(nt));

    nt.element = choice_NegotiationTokenWin_negTokenInit;
    nt.u.negTokenInit.reqFlags = NULL;
    nt.u.negTokenInit.mechToken = NULL;
    nt.u.negTokenInit.negHints = NULL;

    ret = _gss_spnego_indicate_mechtypelist(minor_status, GSS_C_NO_NAME,
					    acceptor_approved, 1, NULL,
					    &nt.u.negTokenInit.mechTypes, NULL);
    if (ret != GSS_S_COMPLETE) {
	return ret;
    }

    memset(&target_princ, 0, sizeof(target_princ));
    if (gethostname(hostname, sizeof(hostname) - 2) != 0) {
	*minor_status = errno;
	free_NegotiationTokenWin(&nt);
	return GSS_S_FAILURE;
    }
    hostname[sizeof(hostname) - 1] = '\0';

    /* Send the constructed SAM name for this host */
    for (p = hostname; *p != '\0' && *p != '.'; p++) {
	*p = toupper((unsigned char)*p);
    }
    *p++ = '$';
    *p = '\0';

    name_buf.length = strlen(hostname);
    name_buf.value = hostname;

    ret = gss_import_name(minor_status, &name_buf,
			  GSS_C_NO_OID,
			  &target_princ);
    if (ret != GSS_S_COMPLETE) {
	free_NegotiationTokenWin(&nt);
	return ret;
    }

    name_buf.length = 0;
    name_buf.value = NULL;

    /* Canonicalize the name using the preferred mechanism */
    ret = gss_canonicalize_name(minor_status,
				target_princ,
				GSS_C_NO_OID,
				&canon_princ);
    if (ret != GSS_S_COMPLETE) {
	free_NegotiationTokenWin(&nt);
	gss_release_name(&minor, &target_princ);
	return ret;
    }

    ret = gss_display_name(minor_status, canon_princ,
			   &name_buf, &name_type);
    if (ret != GSS_S_COMPLETE) {
	free_NegotiationTokenWin(&nt);
	gss_release_name(&minor, &canon_princ);
	gss_release_name(&minor, &target_princ);
	return ret;
    }

    gss_release_name(&minor, &canon_princ);
    gss_release_name(&minor, &target_princ);

    ALLOC(nt.u.negTokenInit.negHints, 1);
    if (nt.u.negTokenInit.negHints == NULL) {
	*minor_status = ENOMEM;
	gss_release_buffer(&minor, &name_buf);
	free_NegotiationTokenWin(&nt);
	return GSS_S_FAILURE;
    }

    ALLOC(nt.u.negTokenInit.negHints->hintName, 1);
    if (nt.u.negTokenInit.negHints->hintName == NULL) {
	*minor_status = ENOMEM;
	gss_release_buffer(&minor, &name_buf);
	free_NegotiationTokenWin(&nt);
	return GSS_S_FAILURE;
    }

    *(nt.u.negTokenInit.negHints->hintName) = name_buf.value;
    name_buf.value = NULL;
    nt.u.negTokenInit.negHints->hintAddress = NULL;

    ASN1_MALLOC_ENCODE(NegotiationTokenWin, 
		       data.value, data.length, &nt, &buf_len, ret);
    free_NegotiationTokenWin(&nt);
    if (ret) {
	return ret;
    }
    if (data.length != buf_len)
	abort();

    ret = gss_encapsulate_token(&data, GSS_SPNEGO_MECHANISM, output_token);

    free (data.value);

    if (ret != GSS_S_COMPLETE)
	return ret;

    *minor_status = 0;

    return GSS_S_CONTINUE_NEEDED;
}

static OM_uint32
send_accept (OM_uint32 *minor_status,
	     gssspnego_ctx context_handle,
	     gss_buffer_t mech_token,
	     int initial_response,
	     gss_buffer_t mech_buf,
	     gss_buffer_t output_token)
{
    NegotiationToken nt;
    OM_uint32 ret;
    gss_buffer_desc mech_mic_buf;
    size_t size;

    memset(&nt, 0, sizeof(nt));

    nt.element = choice_NegotiationToken_negTokenResp;

    ALLOC(nt.u.negTokenResp.negResult, 1);
    if (nt.u.negTokenResp.negResult == NULL) {
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }

    if (context_handle->open) {
	if (mech_token != GSS_C_NO_BUFFER
	    && mech_token->length != 0
	    && mech_buf != GSS_C_NO_BUFFER)
	    *(nt.u.negTokenResp.negResult)  = accept_incomplete;
	else
	    *(nt.u.negTokenResp.negResult)  = accept_completed;
    } else {
	if (initial_response && context_handle->require_mic)
	    *(nt.u.negTokenResp.negResult)  = request_mic;
	else
	    *(nt.u.negTokenResp.negResult)  = accept_incomplete;
    }

    if (initial_response) {
	ALLOC(nt.u.negTokenResp.supportedMech, 1);
	if (nt.u.negTokenResp.supportedMech == NULL) {
	    free_NegotiationToken(&nt);
	    *minor_status = ENOMEM;
	    return GSS_S_FAILURE;
	}

	ret = der_get_oid(context_handle->preferred_mech_type->elements,
			  context_handle->preferred_mech_type->length,
			  nt.u.negTokenResp.supportedMech,
			  NULL);
	if (ret) {
	    free_NegotiationToken(&nt);
	    *minor_status = ENOMEM;
	    return GSS_S_FAILURE;
	}
    } else {
	nt.u.negTokenResp.supportedMech = NULL;
    }

    if (mech_token != GSS_C_NO_BUFFER && mech_token->length != 0) {
	ALLOC(nt.u.negTokenResp.responseToken, 1);
	if (nt.u.negTokenResp.responseToken == NULL) {
	    free_NegotiationToken(&nt);
	    *minor_status = ENOMEM;
	    return GSS_S_FAILURE;
	}
	nt.u.negTokenResp.responseToken->length = mech_token->length;
	nt.u.negTokenResp.responseToken->data   = mech_token->value;
	mech_token->length = 0;
	mech_token->value  = NULL;
    } else {
	nt.u.negTokenResp.responseToken = NULL;
    }

    if (mech_buf != GSS_C_NO_BUFFER) {
	ret = gss_get_mic(minor_status,
			  context_handle->negotiated_ctx_id,
			  0,
			  mech_buf,
			  &mech_mic_buf);
	if (ret == GSS_S_COMPLETE) {
	    ALLOC(nt.u.negTokenResp.mechListMIC, 1);
	    if (nt.u.negTokenResp.mechListMIC == NULL) {
		gss_release_buffer(minor_status, &mech_mic_buf);
		free_NegotiationToken(&nt);
		*minor_status = ENOMEM;
		return GSS_S_FAILURE;
	    }
	    nt.u.negTokenResp.mechListMIC->length = mech_mic_buf.length;
	    nt.u.negTokenResp.mechListMIC->data   = mech_mic_buf.value;
	} else if (ret == GSS_S_UNAVAILABLE) {
	    nt.u.negTokenResp.mechListMIC = NULL;
	} else {
	    free_NegotiationToken(&nt);
	    return ret;
	}

    } else
	nt.u.negTokenResp.mechListMIC = NULL;
 
    ASN1_MALLOC_ENCODE(NegotiationToken,
		       output_token->value, output_token->length,
		       &nt, &size, ret);
    if (ret) {
	free_NegotiationToken(&nt);
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    /*
     * The response should not be encapsulated, because
     * it is a SubsequentContextToken (note though RFC 1964
     * specifies encapsulation for all _Kerberos_ tokens).
     */

    if (*(nt.u.negTokenResp.negResult) == accept_completed)
	ret = GSS_S_COMPLETE;
    else
	ret = GSS_S_CONTINUE_NEEDED;
    free_NegotiationToken(&nt);
    return ret;
}


static OM_uint32
verify_mechlist_mic
	   (OM_uint32 *minor_status,
	    gssspnego_ctx context_handle,
	    gss_buffer_t mech_buf,
	    heim_octet_string *mechListMIC
	   )
{
    OM_uint32 ret;
    gss_buffer_desc mic_buf;

    if (context_handle->verified_mic) {
	/* This doesn't make sense, we've already verified it? */
	*minor_status = 0;
	return GSS_S_DUPLICATE_TOKEN;
    }

    if (mechListMIC == NULL) {
	*minor_status = 0;
	return GSS_S_DEFECTIVE_TOKEN;
    }

    mic_buf.length = mechListMIC->length;
    mic_buf.value  = mechListMIC->data;

    ret = gss_verify_mic(minor_status,
			 context_handle->negotiated_ctx_id,
			 mech_buf,
			 &mic_buf,
			 NULL);

    if (ret != GSS_S_COMPLETE)
	ret = GSS_S_DEFECTIVE_TOKEN;

    return ret;
}

static OM_uint32
select_mech(OM_uint32 *minor_status, MechType *mechType, int verify_p,
	    gss_OID *mech_p)
{
    char mechbuf[64];
    size_t mech_len;
    gss_OID_desc oid;
    gss_OID oidp;
    gss_OID_set mechs;
    int i;
    OM_uint32 ret, junk;

    ret = der_put_oid ((unsigned char *)mechbuf + sizeof(mechbuf) - 1,
		       sizeof(mechbuf),
		       mechType,
		       &mech_len);
    if (ret) {
	return GSS_S_DEFECTIVE_TOKEN;
    }

    oid.length   = mech_len;
    oid.elements = mechbuf + sizeof(mechbuf) - mech_len;

    if (gss_oid_equal(&oid, GSS_SPNEGO_MECHANISM)) {
	return GSS_S_BAD_MECH;
    }

    *minor_status = 0;

    /* Translate broken MS Kebreros OID */
    if (gss_oid_equal(&oid, &_gss_spnego_mskrb_mechanism_oid_desc))
	    oidp = &_gss_spnego_krb5_mechanism_oid_desc;
    else
	    oidp = &oid;


    ret = gss_indicate_mechs(&junk, &mechs);
    if (ret)
	    return (ret);

    for (i = 0; i < mechs->count; i++)
	    if (gss_oid_equal(&mechs->elements[i], oidp))
		    break;

    if (i == mechs->count) {
	    gss_release_oid_set(&junk, &mechs);
	    return GSS_S_BAD_MECH;
    }
    gss_release_oid_set(&junk, &mechs);

    ret = gss_duplicate_oid(minor_status,
			    &oid, /* possibly this should be oidp */
			    mech_p);

    if (verify_p) {
	gss_name_t name = GSS_C_NO_NAME;
	gss_buffer_desc namebuf;
	char *str = NULL, *host, hostname[MAXHOSTNAMELEN];

	host = getenv("GSSAPI_SPNEGO_NAME");
	if (host == NULL || issuid()) {
	    if (gethostname(hostname, sizeof(hostname)) != 0) {
		*minor_status = errno;
		return GSS_S_FAILURE;
	    }
	    asprintf(&str, "host@%s", hostname);
	    host = str;
	}

	namebuf.length = strlen(host);
	namebuf.value = host;

	ret = gss_import_name(minor_status, &namebuf,
			      GSS_C_NT_HOSTBASED_SERVICE, &name);
	if (str)
	    free(str);
	if (ret != GSS_S_COMPLETE)
	    return ret;

	ret = acceptor_approved(name, *mech_p);
	gss_release_name(&junk, &name);
    }

    return ret;
}


static OM_uint32
acceptor_complete(OM_uint32 * minor_status,
		  gssspnego_ctx ctx,
		  int *get_mic,
		  gss_buffer_t mech_buf,
		  gss_buffer_t mech_input_token,
		  gss_buffer_t mech_output_token,
		  heim_octet_string *mic,
		  gss_buffer_t output_token)
{
    OM_uint32 ret;
    int require_mic, verify_mic;
    gss_buffer_desc buf;

    buf.length = 0;
    buf.value = NULL;

    ret = _gss_spnego_require_mechlist_mic(minor_status, ctx, &require_mic);
    if (ret)
	return ret;
    
    ctx->require_mic = require_mic;

    if (mic != NULL)
	require_mic = 1;
    
    if (ctx->open && require_mic) {
	if (mech_input_token == GSS_C_NO_BUFFER) { /* Even/One */
	    verify_mic = 1;
	    *get_mic = 0;
	} else if (mech_output_token != GSS_C_NO_BUFFER &&
		   mech_output_token->length == 0) { /* Odd */
	    *get_mic = verify_mic = 1;
	} else { /* Even/One */
	    verify_mic = 0;
	    *get_mic = 1;
	}
	
	if (verify_mic || get_mic) {
	    int eret;
	    size_t buf_len;
	    
	    ASN1_MALLOC_ENCODE(MechTypeList, 
			       mech_buf->value, mech_buf->length,
			       &ctx->initiator_mech_types, &buf_len, eret);
	    if (eret) {
		*minor_status = eret;
		return GSS_S_FAILURE;
	    }
	    if (buf.length != buf_len)
		abort();
	}
	
	if (verify_mic) {
	    ret = verify_mechlist_mic(minor_status, ctx, mech_buf, mic);
	    if (ret) {
		if (get_mic)
		    send_reject (minor_status, output_token);
		if (buf.value)
		    free(buf.value);
		return ret;
	    }
	    ctx->verified_mic = 1;
	}
	if (buf.value)
	    free(buf.value);

    } else
	*get_mic = verify_mic = 0;
    
    return GSS_S_COMPLETE;
}


static OM_uint32
acceptor_start
	   (OM_uint32 * minor_status,
	    gss_ctx_id_t * context_handle,
	    const gss_cred_id_t acceptor_cred_handle,
	    const gss_buffer_t input_token_buffer,
	    const gss_channel_bindings_t input_chan_bindings,
	    gss_name_t * src_name,
	    gss_OID * mech_type,
	    gss_buffer_t output_token,
	    OM_uint32 * ret_flags,
	    OM_uint32 * time_rec,
	    gss_cred_id_t *delegated_cred_handle
	   )
{
    OM_uint32 ret, junk, minor;
    NegotiationToken nt;
    size_t nt_len;
    NegTokenInit *ni;
    int i;
    gss_buffer_desc data;
    gss_buffer_t mech_input_token = GSS_C_NO_BUFFER;
    gss_buffer_desc mech_output_token;
    gss_buffer_desc mech_buf;
    gss_OID preferred_mech_type = GSS_C_NO_OID;
    gssspnego_ctx ctx;
    gssspnego_cred acceptor_cred = (gssspnego_cred)acceptor_cred_handle;
    int get_mic = 0;
    int first_ok = 0;

    mech_output_token.value = NULL;
    mech_output_token.length = 0;
    mech_buf.value = NULL;

    if (input_token_buffer->length == 0)
	return send_supported_mechs (minor_status, output_token);
	
    ret = _gss_spnego_alloc_sec_context(minor_status, context_handle);
    if (ret != GSS_S_COMPLETE)
	return ret;

    ctx = (gssspnego_ctx)*context_handle;

    /*
     * The GSS-API encapsulation is only present on the initial
     * context token (negTokenInit).
     */
    ret = gss_decapsulate_token (input_token_buffer,
				 GSS_SPNEGO_MECHANISM,
				 &data);
    if (ret)
	return ret;

    ret = decode_NegotiationToken(data.value, data.length, &nt, &nt_len);
    gss_release_buffer(minor_status, &data);
    if (ret) {
	*minor_status = ret;
	return GSS_S_DEFECTIVE_TOKEN;
    }
    if (nt.element != choice_NegotiationToken_negTokenInit) {
	*minor_status = 0;
	return GSS_S_DEFECTIVE_TOKEN;
    }
    ni = &nt.u.negTokenInit;

    if (ni->mechTypes.len < 1) {
	free_NegotiationToken(&nt);
	*minor_status = 0;
	return GSS_S_DEFECTIVE_TOKEN;
    }

    HEIMDAL_MUTEX_lock(&ctx->ctx_id_mutex);

    ret = copy_MechTypeList(&ni->mechTypes, &ctx->initiator_mech_types);
    if (ret) {
	HEIMDAL_MUTEX_unlock(&ctx->ctx_id_mutex);
	free_NegotiationToken(&nt);
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    /*
     * First we try the opportunistic token if we have support for it,
     * don't try to verify we have credential for the token,
     * gss_accept_sec_context will (hopefully) tell us that.
     * If that failes, 
     */

    ret = select_mech(minor_status,
		      &ni->mechTypes.val[0], 
		      0,
		      &preferred_mech_type);

    if (ret == 0 && ni->mechToken != NULL) {
	gss_cred_id_t mech_delegated_cred = GSS_C_NO_CREDENTIAL;
	gss_cred_id_t mech_cred;
	gss_buffer_desc ibuf;

	ibuf.length = ni->mechToken->length;
	ibuf.value = ni->mechToken->data;
	mech_input_token = &ibuf;

	if (acceptor_cred != NULL)
	    mech_cred = acceptor_cred->negotiated_cred_id;
	else
	    mech_cred = GSS_C_NO_CREDENTIAL;
	
	if (ctx->mech_src_name != GSS_C_NO_NAME)
	    gss_release_name(&minor, &ctx->mech_src_name);
	
	if (ctx->delegated_cred_id != GSS_C_NO_CREDENTIAL)
	    _gss_spnego_release_cred(&minor, &ctx->delegated_cred_id);
	
	ret = gss_accept_sec_context(&minor,
				     &ctx->negotiated_ctx_id,
				     mech_cred,
				     mech_input_token,
				     input_chan_bindings,
				     &ctx->mech_src_name,
				     &ctx->negotiated_mech_type,
				     &mech_output_token,
				     &ctx->mech_flags,
				     &ctx->mech_time_rec,
				     &mech_delegated_cred);
	if (ret == GSS_S_COMPLETE || ret == GSS_S_CONTINUE_NEEDED) {
	    ctx->preferred_mech_type = preferred_mech_type;
	    ctx->negotiated_mech_type = preferred_mech_type;
	    if (ret == GSS_S_COMPLETE)
		ctx->open = 1;

	    if (mech_delegated_cred && delegated_cred_handle)
		ret = _gss_spnego_alloc_cred(minor_status,
					     mech_delegated_cred,
					     delegated_cred_handle);
	    else
		gss_release_cred(&junk, &mech_delegated_cred);

	    ret = acceptor_complete(minor_status,
				    ctx,
				    &get_mic,
				    &mech_buf,
				    mech_input_token,
				    &mech_output_token,
				    ni->mechListMIC,
				    output_token);
	    if (ret != GSS_S_COMPLETE)
		goto out;

	    first_ok = 1;
	}
    }

    /*
     * If opportunistic token failed, lets try the other mechs.
     */

    if (!first_ok) {

	/* Call glue layer to find first mech we support */
	for (i = 1; i < ni->mechTypes.len; ++i) {
	    ret = select_mech(minor_status,
			      &ni->mechTypes.val[i],
			      1,
			      &preferred_mech_type);
	    if (ret == 0)
		break;
	}
	if (preferred_mech_type == GSS_C_NO_OID) {
	    HEIMDAL_MUTEX_unlock(&ctx->ctx_id_mutex);
	    free_NegotiationToken(&nt);
	    return GSS_S_BAD_MECH;
	}

	ctx->preferred_mech_type = preferred_mech_type;
	ctx->negotiated_mech_type = preferred_mech_type;
    }

    /*
     * The initial token always have a response
     */

    ret = send_accept (minor_status,
		       ctx,
		       &mech_output_token,
		       1,
		       get_mic ? &mech_buf : NULL,
		       output_token);
    if (ret)
	goto out;
    
out:
    if (mech_output_token.value != NULL)
	gss_release_buffer(&minor, &mech_output_token);
    if (mech_buf.value != NULL) {
	free(mech_buf.value);
	mech_buf.value = NULL;
    }
    free_NegotiationToken(&nt);


    if (ret == GSS_S_COMPLETE) {
	if (src_name != NULL && ctx->mech_src_name != NULL) {
	    spnego_name name;

	    name = calloc(1, sizeof(*name));
	    if (name) {
		name->mech = ctx->mech_src_name;
		ctx->mech_src_name = NULL;
		*src_name = (gss_name_t)name;
	    }
	}
        if (delegated_cred_handle != NULL) {
	    *delegated_cred_handle = ctx->delegated_cred_id;
	    ctx->delegated_cred_id = GSS_C_NO_CREDENTIAL;
	}
    }
    
    if (mech_type != NULL)
	*mech_type = ctx->negotiated_mech_type;
    if (ret_flags != NULL)
	*ret_flags = ctx->mech_flags;
    if (time_rec != NULL)
	*time_rec = ctx->mech_time_rec;

    if (ret == GSS_S_COMPLETE || ret == GSS_S_CONTINUE_NEEDED) {
	HEIMDAL_MUTEX_unlock(&ctx->ctx_id_mutex);
 	return ret;
    }

    _gss_spnego_internal_delete_sec_context(&minor, context_handle,
					    GSS_C_NO_BUFFER);
    
    return ret;
}


static OM_uint32
acceptor_continue
	   (OM_uint32 * minor_status,
	    gss_ctx_id_t * context_handle,
	    const gss_cred_id_t acceptor_cred_handle,
	    const gss_buffer_t input_token_buffer,
	    const gss_channel_bindings_t input_chan_bindings,
	    gss_name_t * src_name,
	    gss_OID * mech_type,
	    gss_buffer_t output_token,
	    OM_uint32 * ret_flags,
	    OM_uint32 * time_rec,
	    gss_cred_id_t *delegated_cred_handle
	   )
{
    OM_uint32 ret, ret2, minor;
    NegotiationToken nt;
    size_t nt_len;
    NegTokenResp *na;
    unsigned int negResult = accept_incomplete;
    gss_buffer_t mech_input_token = GSS_C_NO_BUFFER;
    gss_buffer_t mech_output_token = GSS_C_NO_BUFFER;
    gss_buffer_desc mech_buf;
    gssspnego_ctx ctx;
    gssspnego_cred acceptor_cred = (gssspnego_cred)acceptor_cred_handle;

    mech_buf.value = NULL;

    ctx = (gssspnego_ctx)*context_handle;

    /*
     * The GSS-API encapsulation is only present on the initial
     * context token (negTokenInit).
     */

    ret = decode_NegotiationToken(input_token_buffer->value, 
				  input_token_buffer->length,
				  &nt, &nt_len);
    if (ret) {
	*minor_status = ret;
	return GSS_S_DEFECTIVE_TOKEN;
    }
    if (nt.element != choice_NegotiationToken_negTokenResp) {
	*minor_status = 0;
	return GSS_S_DEFECTIVE_TOKEN;
    }
    na = &nt.u.negTokenResp;

    if (na->negResult != NULL) {
	negResult = *(na->negResult);
    }

    HEIMDAL_MUTEX_lock(&ctx->ctx_id_mutex);

    {
	gss_buffer_desc ibuf, obuf;
	int require_mic, get_mic = 0;
	int require_response;
	heim_octet_string *mic;

	if (na->responseToken != NULL) {
	    ibuf.length = na->responseToken->length;
	    ibuf.value = na->responseToken->data;
	    mech_input_token = &ibuf;
	} else {
	    ibuf.value = NULL;
	    ibuf.length = 0;
	}

	if (mech_input_token != GSS_C_NO_BUFFER) {
	    gss_cred_id_t mech_cred;
	    gss_cred_id_t mech_delegated_cred;
	    gss_cred_id_t *mech_delegated_cred_p;

	    if (acceptor_cred != NULL)
		mech_cred = acceptor_cred->negotiated_cred_id;
	    else
		mech_cred = GSS_C_NO_CREDENTIAL;

	    if (delegated_cred_handle != NULL) {
		mech_delegated_cred = GSS_C_NO_CREDENTIAL;
		mech_delegated_cred_p = &mech_delegated_cred;
	    } else {
		mech_delegated_cred_p = NULL;
	    }

	    if (ctx->mech_src_name != GSS_C_NO_NAME)
		gss_release_name(&minor, &ctx->mech_src_name);

	    if (ctx->delegated_cred_id != GSS_C_NO_CREDENTIAL)
		_gss_spnego_release_cred(&minor, &ctx->delegated_cred_id);

	    ret = gss_accept_sec_context(&minor,
					 &ctx->negotiated_ctx_id,
					 mech_cred,
					 mech_input_token,
					 input_chan_bindings,
					 &ctx->mech_src_name,
					 &ctx->negotiated_mech_type,
					 &obuf,
					 &ctx->mech_flags,
					 &ctx->mech_time_rec,
					 mech_delegated_cred_p);
	    if (ret == GSS_S_COMPLETE || ret == GSS_S_CONTINUE_NEEDED) {
		if (mech_delegated_cred_p != NULL &&
		    mech_delegated_cred != GSS_C_NO_CREDENTIAL) {
		    ret2 = _gss_spnego_alloc_cred(minor_status,
						  mech_delegated_cred,
						  &ctx->delegated_cred_id);
		    if (ret2 != GSS_S_COMPLETE)
			ret = ret2;
		}
		mech_output_token = &obuf;
	    }
	    if (ret != GSS_S_COMPLETE && ret != GSS_S_CONTINUE_NEEDED) {
		free_NegotiationToken(&nt);
		send_reject (minor_status, output_token);
		HEIMDAL_MUTEX_unlock(&ctx->ctx_id_mutex);
		return ret;
	    }
	    if (ret == GSS_S_COMPLETE)
		ctx->open = 1;
	} else
	    ret = GSS_S_COMPLETE;

	ret2 = _gss_spnego_require_mechlist_mic(minor_status, 
						ctx,
						&require_mic);
	if (ret2)
	    goto out;

	ctx->require_mic = require_mic;

	mic = na->mechListMIC;
	if (mic != NULL)
	    require_mic = 1;

	if (ret == GSS_S_COMPLETE)
	    ret = acceptor_complete(minor_status,
				    ctx,
				    &get_mic,
				    &mech_buf,
				    mech_input_token,
				    mech_output_token,
				    na->mechListMIC,
				    output_token);

	if (ctx->mech_flags & GSS_C_DCE_STYLE)
	    require_response = (negResult != accept_completed);
	else
	    require_response = 0;

	/*
	 * Check whether we need to send a result: there should be only
	 * one accept_completed response sent in the entire negotiation
	 */
	if ((mech_output_token != GSS_C_NO_BUFFER &&
	     mech_output_token->length != 0)
	    || (ctx->open && negResult == accept_incomplete)
	    || require_response
	    || get_mic) {
	    ret2 = send_accept (minor_status,
				ctx,
				mech_output_token,
				0,
				get_mic ? &mech_buf : NULL,
				output_token);
	    if (ret2)
		goto out;
	}

     out:
	if (ret2 != GSS_S_COMPLETE)
	    ret = ret2;
	if (mech_output_token != NULL)
	    gss_release_buffer(&minor, mech_output_token);
	if (mech_buf.value != NULL)
	    free(mech_buf.value);
	free_NegotiationToken(&nt);
    }

    if (ret == GSS_S_COMPLETE) {
	if (src_name != NULL && ctx->mech_src_name != NULL) {
	    spnego_name name;

	    name = calloc(1, sizeof(*name));
	    if (name) {
		name->mech = ctx->mech_src_name;
		ctx->mech_src_name = NULL;
		*src_name = (gss_name_t)name;
	    }
	}
        if (delegated_cred_handle != NULL) {
	    *delegated_cred_handle = ctx->delegated_cred_id;
	    ctx->delegated_cred_id = GSS_C_NO_CREDENTIAL;
	}
    }

    if (mech_type != NULL)
	*mech_type = ctx->negotiated_mech_type;
    if (ret_flags != NULL)
	*ret_flags = ctx->mech_flags;
    if (time_rec != NULL)
	*time_rec = ctx->mech_time_rec;

    if (ret == GSS_S_COMPLETE || ret == GSS_S_CONTINUE_NEEDED) {
	HEIMDAL_MUTEX_unlock(&ctx->ctx_id_mutex);
 	return ret;
    }

    _gss_spnego_internal_delete_sec_context(&minor, context_handle,
				   GSS_C_NO_BUFFER);

    return ret;
}

OM_uint32
_gss_spnego_accept_sec_context
	   (OM_uint32 * minor_status,
	    gss_ctx_id_t * context_handle,
	    const gss_cred_id_t acceptor_cred_handle,
	    const gss_buffer_t input_token_buffer,
	    const gss_channel_bindings_t input_chan_bindings,
	    gss_name_t * src_name,
	    gss_OID * mech_type,
	    gss_buffer_t output_token,
	    OM_uint32 * ret_flags,
	    OM_uint32 * time_rec,
	    gss_cred_id_t *delegated_cred_handle
	   )
{
    _gss_accept_sec_context_t *func;

    *minor_status = 0;

    output_token->length = 0;
    output_token->value  = NULL;

    if (src_name != NULL)
	*src_name = GSS_C_NO_NAME;
    if (mech_type != NULL)
	*mech_type = GSS_C_NO_OID;
    if (ret_flags != NULL)
	*ret_flags = 0;
    if (time_rec != NULL)
	*time_rec = 0;
    if (delegated_cred_handle != NULL)
	*delegated_cred_handle = GSS_C_NO_CREDENTIAL;


    if (*context_handle == GSS_C_NO_CONTEXT) 
	func = acceptor_start;
    else
	func = acceptor_continue;
    

    return (*func)(minor_status, context_handle, acceptor_cred_handle,
		   input_token_buffer, input_chan_bindings,
		   src_name, mech_type, output_token, ret_flags,
		   time_rec, delegated_cred_handle);
}
