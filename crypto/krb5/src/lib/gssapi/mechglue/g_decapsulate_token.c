/*
 * Copyright (c) 2011, PADL Software Pty Ltd.
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
 * 3. Neither the name of PADL Software nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PADL SOFTWARE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PADL SOFTWARE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "mglueP.h"

OM_uint32 KRB5_CALLCONV
gss_decapsulate_token(gss_const_buffer_t input_token,
                      gss_const_OID token_oid,
                      gss_buffer_t output_token)
{
    struct k5input in;

    if (input_token == GSS_C_NO_BUFFER || token_oid == GSS_C_NO_OID)
        return GSS_S_CALL_INACCESSIBLE_READ;

    if (output_token == GSS_C_NO_BUFFER)
        return GSS_S_CALL_INACCESSIBLE_WRITE;

    k5_input_init(&in, input_token->value, input_token->length);
    if (!g_verify_token_header(&in, token_oid))
        return GSS_S_DEFECTIVE_TOKEN;

    output_token->value = gssalloc_malloc(in.len);
    if (output_token->value == NULL)
        return GSS_S_FAILURE;

    memcpy(output_token->value, in.ptr, in.len);
    output_token->length = in.len;

    return GSS_S_COMPLETE;
}
