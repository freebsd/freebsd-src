/*
 * Copyright (c) 1997 - 2003 Kungliga Tekniska Högskolan
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

RCSID("$Id: delete_sec_context.c,v 1.11 2003/03/16 17:46:40 lha Exp $");

OM_uint32 gss_delete_sec_context
           (OM_uint32 * minor_status,
            gss_ctx_id_t * context_handle,
            gss_buffer_t output_token
           )
{
    GSSAPI_KRB5_INIT ();

    if (output_token) {
	output_token->length = 0;
	output_token->value  = NULL;
    }

    krb5_auth_con_free (gssapi_krb5_context,
			(*context_handle)->auth_context);
    if((*context_handle)->source)
	krb5_free_principal (gssapi_krb5_context,
			     (*context_handle)->source);
    if((*context_handle)->target)
	krb5_free_principal (gssapi_krb5_context,
			     (*context_handle)->target);
    if ((*context_handle)->ticket) {
	krb5_free_ticket (gssapi_krb5_context,
			  (*context_handle)->ticket);
	free((*context_handle)->ticket);
    }

    free (*context_handle);
    *context_handle = GSS_C_NO_CONTEXT;
    *minor_status = 0;
    return GSS_S_COMPLETE;
}
