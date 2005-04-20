/*
 * Copyright (c) 1999 - 2003 Kungliga Tekniska Högskolan
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

RCSID("$Id: export_sec_context.c,v 1.6 2003/03/16 18:02:52 lha Exp $");

OM_uint32
gss_export_sec_context (
    OM_uint32 * minor_status,
    gss_ctx_id_t * context_handle,
    gss_buffer_t interprocess_token
    )
{
    krb5_storage *sp;
    krb5_auth_context ac;
    OM_uint32 ret = GSS_S_COMPLETE;
    krb5_data data;
    gss_buffer_desc buffer;
    int flags;
    OM_uint32 minor;
    krb5_error_code kret;

    GSSAPI_KRB5_INIT ();
    if (!((*context_handle)->flags & GSS_C_TRANS_FLAG)) {
	*minor_status = 0;
	return GSS_S_UNAVAILABLE;
    }

    sp = krb5_storage_emem ();
    if (sp == NULL) {
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }
    ac = (*context_handle)->auth_context;

    /* flagging included fields */

    flags = 0;
    if (ac->local_address)
	flags |= SC_LOCAL_ADDRESS;
    if (ac->remote_address)
	flags |= SC_REMOTE_ADDRESS;
    if (ac->keyblock)
	flags |= SC_KEYBLOCK;
    if (ac->local_subkey)
	flags |= SC_LOCAL_SUBKEY;
    if (ac->remote_subkey)
	flags |= SC_REMOTE_SUBKEY;

    kret = krb5_store_int32 (sp, flags);
    if (kret) {
	*minor_status = kret;
	goto failure;
    }

    /* marshall auth context */

    kret = krb5_store_int32 (sp, ac->flags);
    if (kret) {
	*minor_status = kret;
	goto failure;
    }
    if (ac->local_address) {
	kret = krb5_store_address (sp, *ac->local_address);
	if (kret) {
	    *minor_status = kret;
	    goto failure;
	}
    }
    if (ac->remote_address) {
	kret = krb5_store_address (sp, *ac->remote_address);
	if (kret) {
	    *minor_status = kret;
	    goto failure;
	}
    }
    kret = krb5_store_int16 (sp, ac->local_port);
    if (kret) {
	*minor_status = kret;
	goto failure;
    }
    kret = krb5_store_int16 (sp, ac->remote_port);
    if (kret) {
	*minor_status = kret;
	goto failure;
    }
    if (ac->keyblock) {
	kret = krb5_store_keyblock (sp, *ac->keyblock);
	if (kret) {
	    *minor_status = kret;
	    goto failure;
	}
    }
    if (ac->local_subkey) {
	kret = krb5_store_keyblock (sp, *ac->local_subkey);
	if (kret) {
	    *minor_status = kret;
	    goto failure;
	}
    }
    if (ac->remote_subkey) {
	kret = krb5_store_keyblock (sp, *ac->remote_subkey);
	if (kret) {
	    *minor_status = kret;
	    goto failure;
	}
    }
    kret = krb5_store_int32 (sp, ac->local_seqnumber);
	if (kret) {
	    *minor_status = kret;
	    goto failure;
	}
    kret = krb5_store_int32 (sp, ac->remote_seqnumber);
	if (kret) {
	    *minor_status = kret;
	    goto failure;
	}

    kret = krb5_store_int32 (sp, ac->keytype);
    if (kret) {
	*minor_status = kret;
	goto failure;
    }
    kret = krb5_store_int32 (sp, ac->cksumtype);
    if (kret) {
	*minor_status = kret;
	goto failure;
    }

    /* names */

    ret = gss_export_name (minor_status, (*context_handle)->source, &buffer);
    if (ret)
	goto failure;
    data.data   = buffer.value;
    data.length = buffer.length;
    kret = krb5_store_data (sp, data);
    gss_release_buffer (&minor, &buffer);
    if (kret) {
	*minor_status = kret;
	goto failure;
    }

    ret = gss_export_name (minor_status, (*context_handle)->target, &buffer);
    if (ret)
	goto failure;
    data.data   = buffer.value;
    data.length = buffer.length;

    ret = GSS_S_FAILURE;

    kret = krb5_store_data (sp, data);
    gss_release_buffer (&minor, &buffer);
    if (kret) {
	*minor_status = kret;
	goto failure;
    }

    kret = krb5_store_int32 (sp, (*context_handle)->flags);
    if (kret) {
	*minor_status = kret;
	goto failure;
    }
    kret = krb5_store_int32 (sp, (*context_handle)->more_flags);
    if (kret) {
	*minor_status = kret;
	goto failure;
    }
    kret = krb5_store_int32 (sp, (*context_handle)->lifetime);
    if (kret) {
	*minor_status = kret;
	goto failure;
    }

    kret = krb5_storage_to_data (sp, &data);
    krb5_storage_free (sp);
    if (kret) {
	*minor_status = kret;
	return GSS_S_FAILURE;
    }
    interprocess_token->length = data.length;
    interprocess_token->value  = data.data;
    ret = gss_delete_sec_context (minor_status, context_handle,
				  GSS_C_NO_BUFFER);
    if (ret != GSS_S_COMPLETE)
	gss_release_buffer (NULL, interprocess_token);
    *minor_status = 0;
    return ret;
 failure:
    krb5_storage_free (sp);
    return ret;
}
