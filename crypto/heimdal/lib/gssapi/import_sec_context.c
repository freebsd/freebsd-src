/*
 * Copyright (c) 1999 - 2001 Kungliga Tekniska Högskolan
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

#include "gssapi_locl.h"

RCSID("$Id: import_sec_context.c,v 1.5 2001/05/11 09:16:46 assar Exp $");

OM_uint32
gss_import_sec_context (
    OM_uint32 * minor_status,
    const gss_buffer_t interprocess_token,
    gss_ctx_id_t * context_handle
    )
{
    OM_uint32 ret = GSS_S_FAILURE;
    krb5_error_code kret;
    krb5_storage *sp;
    krb5_auth_context ac;
    krb5_address local, remote;
    krb5_address *localp, *remotep;
    krb5_data data;
    gss_buffer_desc buffer;
    krb5_keyblock keyblock;
    int32_t tmp;
    int32_t flags;
    OM_uint32 minor;

    gssapi_krb5_init ();

    sp = krb5_storage_from_mem (interprocess_token->value,
				interprocess_token->length);
    if (sp == NULL) {
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }

    *context_handle = malloc(sizeof(**context_handle));
    if (*context_handle == NULL) {
	*minor_status = ENOMEM;
	krb5_storage_free (sp);
	return GSS_S_FAILURE;
    }
    memset (*context_handle, 0, sizeof(**context_handle));

    kret = krb5_auth_con_init (gssapi_krb5_context,
			       &(*context_handle)->auth_context);
    if (kret) {
	gssapi_krb5_set_error_string ();
	*minor_status = kret;
	ret = GSS_S_FAILURE;
	goto failure;
    }

    /* flags */

    krb5_ret_int32 (sp, &flags);

    /* retrieve the auth context */

    ac = (*context_handle)->auth_context;
    krb5_ret_int32 (sp, &ac->flags);
    if (flags & SC_LOCAL_ADDRESS)
	krb5_ret_address (sp, localp = &local);
    else
	localp = NULL;
    if (flags & SC_REMOTE_ADDRESS)
	krb5_ret_address (sp, remotep  = &remote);
    else
	remotep = NULL;
    krb5_auth_con_setaddrs (gssapi_krb5_context, ac, localp, remotep);
    if (localp)
	krb5_free_address (gssapi_krb5_context, localp);
    if (remotep)
	krb5_free_address (gssapi_krb5_context, remotep);
    krb5_ret_int16 (sp, &ac->local_port);
    krb5_ret_int16 (sp, &ac->remote_port);
    if (flags & SC_KEYBLOCK) {
	krb5_ret_keyblock (sp, &keyblock);
	krb5_auth_con_setkey (gssapi_krb5_context, ac, &keyblock);
	krb5_free_keyblock_contents (gssapi_krb5_context, &keyblock);
    }
    if (flags & SC_LOCAL_SUBKEY) {
	krb5_ret_keyblock (sp, &keyblock);
	krb5_auth_con_setlocalsubkey (gssapi_krb5_context, ac, &keyblock);
	krb5_free_keyblock_contents (gssapi_krb5_context, &keyblock);
    }
    if (flags & SC_REMOTE_SUBKEY) {
	krb5_ret_keyblock (sp, &keyblock);
	krb5_auth_con_setremotesubkey (gssapi_krb5_context, ac, &keyblock);
	krb5_free_keyblock_contents (gssapi_krb5_context, &keyblock);
    }
    krb5_ret_int32 (sp, &ac->local_seqnumber);
    krb5_ret_int32 (sp, &ac->remote_seqnumber);

#if 0
    {
	    size_t sz;

	    krb5_ret_data (sp, &data);
	    ac->authenticator = malloc (sizeof (*ac->authenticator));
	    if (ac->authenticator == NULL) {
		*minor_status = ENOMEM;
		ret = GSS_S_FAILURE;
		goto failure;
	    }

	    kret = decode_Authenticator (data.data, data.length,
					 ac->authenticator, &sz);
	    krb5_data_free (&data);
	    if (kret) {
		*minor_status = kret;
		ret = GSS_S_FAILURE;
		goto failure;
	    }
    }
#endif

    krb5_ret_int32 (sp, &tmp);
    ac->keytype = tmp;
    krb5_ret_int32 (sp, &tmp);
    ac->cksumtype = tmp;

    /* names */

    krb5_ret_data (sp, &data);
    buffer.value  = data.data;
    buffer.length = data.length;

    ret = gss_import_name (minor_status, &buffer, GSS_C_NO_OID,
			   &(*context_handle)->source);
    krb5_data_free (&data);
    if (ret)
	goto failure;

    krb5_ret_data (sp, &data);
    buffer.value  = data.data;
    buffer.length = data.length;

    ret = gss_import_name (minor_status, &buffer, GSS_C_NO_OID,
			   &(*context_handle)->target);
    krb5_data_free (&data);
    if (ret)
	goto failure;

    krb5_ret_int32 (sp, &tmp);
    (*context_handle)->flags = tmp;
    krb5_ret_int32 (sp, &tmp);
    (*context_handle)->more_flags = tmp;

    return GSS_S_COMPLETE;

failure:
    krb5_auth_con_free (gssapi_krb5_context,
			(*context_handle)->auth_context);
    if ((*context_handle)->source != NULL)
	gss_release_name(&minor, &(*context_handle)->source);
    if ((*context_handle)->target != NULL)
	gss_release_name(&minor, &(*context_handle)->target);
    free (*context_handle);
    *context_handle = GSS_C_NO_CONTEXT;
    return ret;
}
