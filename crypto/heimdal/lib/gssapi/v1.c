/*
 * Copyright (c) 1997 Kungliga Tekniska Högskolan
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

RCSID("$Id: v1.c,v 1.2 1999/12/02 17:05:04 joda Exp $");

/* These functions are for V1 compatibility */

OM_uint32 gss_sign
           (OM_uint32 * minor_status,
            gss_ctx_id_t context_handle,
            int qop_req,
            gss_buffer_t message_buffer,
            gss_buffer_t message_token
           )
{
  return gss_get_mic(minor_status,
	context_handle,
	(gss_qop_t)qop_req,
	message_buffer,
	message_token);
}

OM_uint32 gss_verify
           (OM_uint32 * minor_status,
            gss_ctx_id_t context_handle,
            gss_buffer_t message_buffer,
            gss_buffer_t token_buffer,
            int * qop_state
           )
{
  return gss_verify_mic(minor_status,
	context_handle,
	message_buffer,
	token_buffer,
	(gss_qop_t *)qop_state);
}

OM_uint32 gss_seal
           (OM_uint32 * minor_status,
            gss_ctx_id_t context_handle,
            int conf_req_flag,
            int qop_req,
            gss_buffer_t input_message_buffer,
            int * conf_state,
            gss_buffer_t output_message_buffer
           )
{
  return gss_wrap(minor_status,
	context_handle,
	conf_req_flag,
	(gss_qop_t)qop_req,
	input_message_buffer,
	conf_state,
	output_message_buffer);
}

OM_uint32 gss_unseal
           (OM_uint32 * minor_status,
            gss_ctx_id_t context_handle,
            gss_buffer_t input_message_buffer,
            gss_buffer_t output_message_buffer,
            int * conf_state,
            int * qop_state
           )
{
  return gss_unwrap(minor_status,
	context_handle,
	input_message_buffer,
	output_message_buffer,
	conf_state,
	(gss_qop_t *)qop_state);
}
